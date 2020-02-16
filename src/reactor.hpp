#pragma once
#include "types.hpp"

#include <jack/jack.h>

#include <exception>
#include <future>

namespace olo {

class Reactor {
    JackClient& client_;
    // Names of client-side Jack ports used for connecting
    vector<string> input_names_;
    vector<string> output_names_;
    // Client-side Jack ports
    vector<jack_port_t*> inputs_;
    vector<jack_port_t*> outputs_;
    // Pre-allocated arrays for storing port buffers in RT thread
    vector<Sample*> output_buffers_;
    vector<const Sample*> input_buffers_;
    Reader* reader_ = nullptr;
    Writer* writer_ = nullptr;
    size_t underruns_ = 0;
    size_t overruns_ = 0;
    // Total number of frames needed to process to consider RT thread work as finished
    size_t needed_ = 0;
    // Number of frames processed so far
    size_t done_ = 0;
    // Protects `finished_` from being signalled multiple times which has catastrophical results.
    bool finished_fired_ = false;
    // Delivers signal that RT thread is finished to the control thread
    std::promise<void> finished_;
    // True if jack_activate() succeded and needs to be paired with jack_deactivate()
    bool activated_ = false;

    void register_ports(const vector<string>& input_ports, const vector<string>& output_ports);
    void connect_ports(const vector<string>& input_ports, const vector<string>& output_ports);

    static int process_(jack_nframes_t frame_count, void* arg);
    static void shutdown_(void* arg);
    static void signal_handler_(int sig);

    void process(size_t frame_count);
    void deactivate();
    void activate();
    void signal_finished();
    void playback(size_t frame_count);
    void capture(size_t frame_count);

public:
    explicit Reactor(
        JackClient& client,
        const vector<string>& input_ports, 
        const vector<string>& output_ports,
        Reader* reader = nullptr,
        Writer* writer = nullptr
    );

    ~Reactor();

    void wait_finished();
};

}