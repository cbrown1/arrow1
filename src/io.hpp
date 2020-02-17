#pragma once
#include "types.hpp"

#include <sndfile.h>
#include <jack/ringbuffer.h>

#include <memory>
#include <thread>
#include <condition_variable>

namespace olo {

// Shared properties and bits of implementation of Reader & Writer.
class IoWorker {
protected:
    size_t sample_rate_;
    size_t channel_count_;
    size_t frame_size_;
    // Ringbuffer size in frames.
    size_t buffer_size_;
    std::unique_ptr<jack_ringbuffer_t, typeof(&jack_ringbuffer_free)> ring_;
    std::unique_ptr<Sample[]> buff_;
    std::unique_ptr<std::thread> thread_;
    std::mutex mx_;
    std::condition_variable cv_;
    std::unique_ptr<SNDFILE, typeof(&sf_close)> sf_;
    // Read/write at most needed_ frames.
    size_t needed_ = 0;
    // Stores number of frames read/written so far.
    size_t done_ = 0;
    volatile bool break_ = false;
    // Stores exception thrown in worker thread for rethrow in join()
    std::exception_ptr ex_;

    explicit IoWorker(size_t sample_rate, size_t channel_count, size_t buffer_size);
    virtual void work_cycle() = 0;
    void pump();

public:
    // We're joining thread in the destructor, which may throw
    virtual ~IoWorker() noexcept(false);

    jack_ringbuffer_t* buffer() const { return ring_.get(); }
    size_t frame_size() const { return frame_size_; }
    size_t channel_count() const { return channel_count_; }
    size_t buffer_size() const { return buffer_size_; }
    size_t sample_rate() const { return sample_rate_; }
    size_t frames_needed() const { return needed_; }
    size_t frames_done() const { return done_; }

    void wake();
    void stop();
    void join();
    bool finished() const { return break_; }
};

class Reader: public IoWorker {
    void work_cycle() override;

public:
    explicit Reader(
        const string& path,
        size_t sample_rate,
        size_t channel_count,
        size_t buffer_size,
        double duration_secs = 0.,
        double start_offset_secs = 0.
    );
};

class Writer: public IoWorker {
    void work_cycle() override;
    bool done() const { return needed_ != 0 && done_ == needed_; }

public:
    explicit Writer(
        const string& path,
        size_t sample_rate,
        size_t channel_count,
        size_t buffer_size,
        double duration_secs = 0.
    );
};

size_t query_audio_file_channels(const string& path);
}
