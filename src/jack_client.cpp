#include "jack_client.hpp"
#include "log.hpp"

#include <stdexcept>
#include <cstdio>

namespace olo {

JackClient::JackClient(const string& name):
    client_ {
        jack_client_open(name.c_str(), JackNullOption, NULL),
        jack_client_close
    }
{
    if (!client_) {
        throw std::runtime_error("unable to create Jack client, is server running?");
    }
    // Server is free to change client name to make it unique
    name_ = jack_get_client_name(handle());
    sample_rate_ = jack_get_sample_rate(handle());
    ldebug("JackClient: engine is using sample rate %zd\n", sample_rate_);
}

vector<string> JackClient::enumerate_ports(int type) const {
    const char **ports = jack_get_ports(handle(), NULL, JACK_DEFAULT_AUDIO_TYPE, type);
    if (ports == nullptr) {
        throw std::runtime_error("enumerating Jack channels failed");
    }
    vector<string> res;
    for (auto p = ports; *p != nullptr; ++p) {
        res.push_back(*p);
    }
    jack_free(ports);
    return res;
}

void JackClient::dump_ports() const {
    using std::printf;
    auto playback = playback_ports();
    printf("%zd Output (playback) channels:\n", playback.size());
    for (size_t i = 0; i != playback.size(); ++i) {
        printf("  %2zd: %s\n", i + 1, playback[i].c_str());
    }
    auto capture = capture_ports();
    printf("%zd Input (record) channels:\n", capture.size());
    for (size_t i = 0; i != capture.size(); ++i) {
        printf("  %2zd: %s\n", i + 1, capture[i].c_str());
    }
}

}
