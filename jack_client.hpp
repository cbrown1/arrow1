#pragma once
#include "types.hpp"

#include <jack/jack.h>

#include <memory>

namespace olo {

class JackClient {
    std::unique_ptr<jack_client_t, typeof(&jack_client_close)> client_;
    const char* name_;
    size_t sample_rate_;

public:
    explicit JackClient(const string& name);

    const char* name() const { return name_; }
    jack_client_t* handle() const { return client_.get(); }
    size_t sample_rate() const { return sample_rate_; }

    void dump_ports() const;
    vector<string> enumerate_ports(int type) const;
    vector<string> capture_ports() const { return enumerate_ports(JackPortIsPhysical | JackPortIsOutput); }
    vector<string> playback_ports() const { return enumerate_ports(JackPortIsPhysical | JackPortIsInput); }
};

}