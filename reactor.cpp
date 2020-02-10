#include "reactor.hpp"
#include "jack_client.hpp"
#include "io.hpp"
#include "log.hpp"

#include <jack/jack.h>

#include <boost/format.hpp>

#include <memory>
#include <stdexcept>
#include <chrono>
#include <cstring>

namespace olo {
using std::unique_ptr;
using std::runtime_error;
using boost::format;

namespace {
struct PortDeleter {
    JackClient& client;
    void operator()(jack_port_t* port) const {
        jack_port_unregister(client.handle(), port);
    }
};

auto create_port(JackClient& client, const string& name, int flags) {
    unique_ptr<jack_port_t, PortDeleter> port {
        jack_port_register(client.handle(), name.c_str(), JACK_DEFAULT_AUDIO_TYPE, flags, 0),
        PortDeleter{client}
    };
    if (!port) {
        throw runtime_error{str(format("failed creating port %1%") % name)};
    }
    return port;
}

Reactor* instance = nullptr;
}

void Reactor::register_ports(const vector<string>& input_ports, const vector<string>& output_ports) {
    if (writer_ != nullptr) {
        inputs_.reserve(input_ports.size());
        input_names_.reserve(input_ports.size());
        for (size_t i = 0; i != input_ports.size(); ++i) {
            auto short_name = str(format("input_%1%") % i);
            auto port = create_port(client_, short_name, JackPortIsInput);
            input_names_.push_back(string{client_.name()} + ":" + short_name);
            inputs_.push_back(port.release());
        }
        input_buffers_.resize(input_ports.size());
    }
    if (reader_ != nullptr) {
        outputs_.reserve(output_ports.size());
        output_names_.reserve(output_ports.size());
        for (size_t i = 0; i != output_ports.size(); ++i) {
            auto short_name = str(format("output_%1%") % i);
            auto port = create_port(client_, short_name, JackPortIsOutput);
            output_names_.push_back(string{client_.name()} + ":" + short_name);
            outputs_.push_back(port.release());
        }
        output_buffers_.resize(output_ports.size());
    }
}

void Reactor::connect_ports(const vector<string>& input_ports, const vector<string>& output_ports) {
    if (writer_ != nullptr) {
        for (size_t i = 0; i != input_ports.size(); ++i) {
            int err = jack_connect(client_.handle(), input_ports[i].c_str(), input_names_[i].c_str());
            if (0 != err) {
                throw runtime_error{str(format("failed connecting port %1% to %2% with Jack error %3%")
                    % input_ports[i] % input_names_[i] % err)};
            } 
        }
    }
    if (reader_ != nullptr) {
        for (size_t i = 0; i != output_ports.size(); ++i) {
            int err = jack_connect(client_.handle(), output_names_[i].c_str(), output_ports[i].c_str());
            if (0 != err) {
                throw runtime_error{str(format("failed connecting port %1% to %2% with Jack error %3%")
                    % output_names_[i] % output_ports[i] % err)};
            } 
        }
    }
}

void Reactor::activate() {
    assert(!activated_);
    int err;
    if (0 != (err = jack_activate(client_.handle()))) {
        throw runtime_error{str(format("failed activating Jack client with error %1%") % err)};
    } else {
        ldebug("Reactor::activate(): Jack client activated\n");
        activated_ = true;
    }
}

void Reactor::deactivate() {
    if (activated_) {
        jack_deactivate(client_.handle());
        ldebug("Reactor::deactivat(): Jack client deactivated\n");
        activated_ = false;        
    }
}

Reactor::Reactor(
    JackClient& client,
    const vector<string>& input_ports, 
    const vector<string>& output_ports,
    Reader* reader,
    Writer* writer
):
    client_{client},
    reader_{reader},
    writer_{writer},
    needed_{
        std::max(
            reader ? reader->frames_needed() : 0, 
            writer ? writer->frames_needed() : 0
        )}
{
    if (instance != nullptr) {
        throw runtime_error{"reactor instance is already present"};
    } else {
        instance = this;
    }
    register_ports(input_ports, output_ports);
    int err;
    if (0 != (err = jack_set_process_callback(client_.handle(), process_, this)))  {
        throw runtime_error{str(format("failed setting Jack process callback with error %1%") % err)};
    }
    // TODO setup signal and shutdown handlers

    activate();
    try {
        connect_ports(input_ports, output_ports);
    } catch (...) {
        lerror("Reactor::Reactor(): exception while connecting ports, rethrowing after deactivate\n");
        deactivate();
        throw;
    }
}

Reactor::~Reactor() {
    deactivate();
    for (auto& port: inputs_) {
        jack_port_disconnect(client_.handle(), port);
        jack_port_unregister(client_.handle(), port);
    }
    for (auto& port: outputs_) {
        jack_port_disconnect(client_.handle(), port);
        jack_port_unregister(client_.handle(), port);
    }
    if (instance == this) {
        instance = nullptr;
    }
}

void Reactor::signal_finished() {
    if (!finished_fired_) {
        finished_fired_ = true;
        finished_.set_value();
    }
}

int Reactor::process_(jack_nframes_t frame_count, void* arg) {
    Reactor* reactor = static_cast<Reactor*>(arg);
    assert(reactor != nullptr);
    try {
        reactor->process(frame_count);
    } catch (...) {
        if (!reactor->finished_fired_) {
            reactor->finished_.set_exception(std::current_exception());
        } else {
            // Just let the world burn, we are already done here
            throw;
        }
    }
    return 0;
}

void Reactor::wait_finished() {
    finished_.get_future().wait();
    deactivate();
    ldebug("Reactor::wait_finished(): done processing %ld frames\n    overruns: %ld\n    underruns: %ld\n", done_, overruns_, underruns_);
}

void Reactor::playback(size_t frame_count) {
    assert(reader_ != nullptr);
    const auto channels = reader_->channel_count();
    size_t n, c;
    // Update buffer pointers
    for (size_t c = 0; c != channels; ++c) {
        output_buffers_[c] = static_cast<Sample*>(jack_port_get_buffer(outputs_[c], frame_count));
        if (output_buffers_[c] == nullptr) {
            throw runtime_error{str(format("unable to obtain playback buffer for port %1%") 
                % output_names_[c])};
        }
    }
    // Demultiplex samples into port buffers
    for (n = 0; n != frame_count; ++n) {
        bool break_outer = false;
        for (c = 0; c != channels; ++c) {
            size_t read = jack_ringbuffer_read(
                reader_->buffer(), 
                reinterpret_cast<char*>(&output_buffers_[c][n]),
                sizeof(Sample)
            );
            if (read != sizeof(Sample)) {
                if (!reader_->finished()) {
                    lerror("Reactor::playback(): ringbuffer read failed, UNDERRUN\n");
                    ++underruns_;
                }
                break_outer = true;
                break;
            }
        }
        if (break_outer) {
            break;
        }
    }
    // Signal reader we're done
    if (!reader_->finished()) {
        reader_->wake();
    }
    // Mute the remaining samples in case of underrun or stream end
    if (n != frame_count) {
        for (c = 0; c != channels; ++c) {
            std::memset(&output_buffers_[c][n], 0, sizeof(Sample) * (frame_count - n));
        }
    }
}

void Reactor::capture(size_t frame_count) {
    assert(writer_ != nullptr);
    if (writer_->finished()) {
        // Don't even bother, drop samples into vacuum
        return;
    }
    const auto channels = writer_->channel_count();
    size_t n, c;
    // Update buffer pointers
    for (size_t c = 0; c != channels; ++c) {
        input_buffers_[c] = static_cast<const Sample*>(jack_port_get_buffer(inputs_[c], frame_count));
        if (input_buffers_[c] == nullptr) {
            throw runtime_error{str(format("unable to obtain capture buffer for port %1%") 
                % input_buffers_[c])};
        }
    }
    // Multiplex samples into writer's ringbuffer
    for (n = 0; n != frame_count; ++n) {
        bool break_outer = false;
        for (c = 0; c != channels; ++c) {
            size_t written = jack_ringbuffer_write(
                writer_->buffer(), 
                reinterpret_cast<const char*>(&input_buffers_[c][n]),
                sizeof(Sample)
            );
            if (written != sizeof(Sample)) {
                if (!writer_->finished()) {
                    lerror("Reactor::capture(): ringbuffer write failed, OVERRUN\n");
                    ++overruns_;
                }
                break_outer = true;
                break;
            }
        }
        if (break_outer) {
            break;
        }
    }
    // Signal writer we're done
    if (!writer_->finished()) {
        writer_->wake();
    }
}

void Reactor::process(size_t frame_count) {
    if (reader_) {
        playback(frame_count);
    }

    if (writer_) {
        capture(frame_count);
    }

    done_ += frame_count;
    if (done_ >= needed_) {
        ldebug("Reactor::process(): signalling done to control thread after %ld frames\n", done_);
        signal_finished();
    }
}

}
