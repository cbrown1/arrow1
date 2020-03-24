#include "io.hpp"
#include "log.hpp"

#include <sndfile.h>
#include <boost/format.hpp>

#include <stdexcept>
#include <cstring>
#include <cassert>

namespace olo {
using std::runtime_error;
using boost::format;

namespace {
auto open_sndfile(const string& path, int mode, SF_INFO& si) {
    std::unique_ptr<SNDFILE, decltype(&sf_close)> sf {
        sf_open(path.c_str(), mode, &si),
        sf_close
    };
    if (!sf) {
        if (mode == SFM_READ) {
            throw runtime_error{str(format("can't open playback file: %1%") % path)};
        }
        if (mode == SFM_WRITE) {
            throw runtime_error{str(format("can't open recording file: %1%") % path)};
        }
    }
    return sf;
}
}

IoWorker::IoWorker(size_t sample_rate, size_t channel_count, size_t buffer_size):
    sample_rate_{sample_rate},
    channel_count_{channel_count},
    frame_size_{channel_count * sizeof(Sample)},
    buffer_size_{buffer_size},
    ring_ {
        jack_ringbuffer_create(buffer_size_ * frame_size_),
        &jack_ringbuffer_free
    },
    buff_{new Sample[buffer_size_ * channel_count_]},
    sf_ {nullptr, sf_close}
{
    if (!ring_) {
        throw runtime_error{str(format("reader unable to allocate ring buffer of %1% bytes")
            % (buffer_size_ * frame_size_))};
    }
}

void IoWorker::join() {
    if (thread_) {
        ldebug("IoWorker::join(): waiting for worker thread\n");
        thread_->join();
        thread_.reset();
    }
    if (ex_) {
        ldebug("IoWorker::join(): rethrowing exception from worker thread\n");
        std::exception_ptr ex;
        // Clear exception so that the join in destructor won't throw
        std::swap(ex_, ex);
        std::rethrow_exception(ex);
    }
}

void IoWorker::wake() {
    cv_.notify_one();
}

void IoWorker::stop() {
    if (!break_) {
        ldebug("IoWorker::stop(): requesting worker stop\n");
        break_ = true;
        wake();
    }
    join();
}

void IoWorker::pump() {
    try {
        std::unique_lock<std::mutex> lock{mx_};
        while (!break_) {
            cv_.wait(lock);
            if (break_) {
                break;
            }
            // No need to hold mutex - jack_ringbuffer is lock-free
            lock.unlock();

            work_cycle();

            lock.lock();
        }
    } catch (...) {
        lerror("IoWorker::pump(): exception in worker thread, will be rethrown on join()\n");
        ex_ = std::current_exception();
    }
}

IoWorker::~IoWorker() noexcept(false) {
    stop();
}

Reader::Reader(
    const string& path,
    size_t sample_rate,
    size_t channel_count,
    size_t buffer_size,
    double duration_secs,
    double start_offset_secs
):
    IoWorker{sample_rate, channel_count, buffer_size}
{
    SF_INFO si = {0};
    sf_ = open_sndfile(path, SFM_READ, si);
    if (si.samplerate != sample_rate_) {
        throw runtime_error{str(format("playback file sample rate: %1%; engine sample rate: %2%")
            % si.samplerate % sample_rate_)};
    }
    if (si.channels != channel_count_) {
        throw runtime_error{str(format("playback file channels: %1%; engine channels: %2%")
            % si.channels % channel_count_)};
    }
    ldebug("Reader: reading from %s with %zd sample rate and %zd channels\n",
        path.c_str(), sample_rate_, channel_count_);
    sf_count_t frames_avail = si.frames;
    sf_count_t start_frame = start_offset_secs * sample_rate_ + .5;
    start_frame = std::min(frames_avail, start_frame);
    if (sf_seek(sf_.get(), start_frame, SEEK_SET) < 0) {
        throw runtime_error{str(format("failed seeking input file to frame %1%")
            % start_frame)};
    }
    frames_avail -= start_frame;
    if (duration_secs != 0) {
        sf_count_t duration_frames = duration_secs * sample_rate_ + .5;
        frames_avail = std::min(frames_avail, duration_frames);
        ldebug("Reader::Reader(): limiting duration to %zd frames\n", frames_avail);
    }
    needed_ = frames_avail;

    // Prefill ringbuffer with as much input file data as possible to minimize underrun probability.
    work_cycle();

    if (!break_) {
        thread_.reset(new std::thread(&Reader::pump, this));
    } else {
        ldebug("Reader::Reader(): not starting worker, whole file in ringbuffer\n");
    }
}

void Reader::work_cycle() {
    size_t writable = jack_ringbuffer_write_space(buffer()) / frame_size_;
    // Don't read past `needed_` frames
    assert(done_ <= needed_);
    // Limit the size because jack_rigbuffer_create may allocate buffer larger
    // than buffer_size_ (rounding upwards to powers of 2) and reports the real
    // allocated space here, leading to buffer overflow of buff_
    writable = std::min(writable, buffer_size_);
    writable = std::min(needed_ - done_, writable);
    auto read = sf_readf_float(sf_.get(), buff_.get(), writable);
    if (read != writable) {
        throw runtime_error{str(format("unexpected read of %1% frames when requested %2%, premature EOF?")
            % read % writable)};
    }
    size_t written = jack_ringbuffer_write(buffer(), reinterpret_cast<const char*>(buff_.get()), read * frame_size_);
    assert(written == read * frame_size_);  // As we are the only producer
    done_ += read;
    if (done_ == needed_) {
        ldebug("Reader::refill(): requesting worker stop, we're done after %zd frames\n", done_);
        break_ = true;
    }
}

Writer::Writer(
    const string& path,
    size_t sample_rate,
    size_t channel_count,
    size_t buffer_size,
    double duration_secs
):
    IoWorker{sample_rate, channel_count, buffer_size}
{
    SF_INFO si = {0};
    si.channels = channel_count_;
    si.samplerate = sample_rate_;
    si.format = SF_FORMAT_WAV | SF_FORMAT_PCM_32;
    sf_ = open_sndfile(path, SFM_WRITE, si);
    ldebug("Writer: writing to %s with %zd sample rate and %zd channels\n",
        path.c_str(), sample_rate_, channel_count_);
    needed_ = duration_secs * sample_rate_ + .5;
    thread_.reset(new std::thread(&Writer::pump, this));
}

void Writer::work_cycle() {
    size_t readable = jack_ringbuffer_read_space(buffer()) / frame_size_;
    // Limit the size because jack_rigbuffer_create may allocate buffer larger
    // than buffer_size_ (rounding upwards to powers of 2) and reports the real
    // allocated space here, leading to buffer overflow of buff_
    readable = std::min(readable, buffer_size_);
    if (0 != needed_) {
        assert(done_ <= needed_);
        readable = std::min(readable, needed_ - done_);
    }
    size_t read = jack_ringbuffer_read(buffer(), reinterpret_cast<char*>(buff_.get()), readable * frame_size_);
    assert(read == readable * frame_size_);  // As we are the only consumer
    auto written = sf_writef_float(sf_.get(), buff_.get(), readable);
    if (written != readable) {
        throw runtime_error{str(format("unexpected write of %1% frames when requested %2%, no more space?")
            % written % readable)};
    }
    done_ += written;
    if (0 != needed_ && done_ == needed_) {
        ldebug("Writer::drain(): requesting worker stop, we're done after %zd frames\n", done_);
        break_ = true;
    }
}

size_t query_audio_file_channels(const string& path) {
    SF_INFO si = {0};
    auto sf = open_sndfile(path, SFM_READ, si);
    return si.channels;
}

}
