#include "jack_client.hpp"

#include <stdexcept>
#include <cstdio>

namespace olo {

namespace {
// Length of NULL-terminated array of pointers
int array_length_(const void** array) {
    int i = -1;
    while (array[++i] != NULL);
    return i;
}
// Addresses the casting necessary to silence pointer type mismatch warnings with array_length_()
#define array_length(arr) array_length_((const void**)arr)
}

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
}

void JackClient::dump_ports() const {
    using std::printf;
    const char **ports_in = jack_get_ports(handle(), NULL, NULL, JackPortIsPhysical | JackPortIsInput);
    int size = array_length(ports_in);
    int i = 0;
    printf("%i Output ports:\n", size);
    while (ports_in[i] != nullptr) {
        printf("  %2i: %s\n", i + 1, ports_in[i]);
        i++;
    }
    jack_free(ports_in);
    const char **ports_out;
    ports_out = jack_get_ports(handle(), NULL, NULL, JackPortIsPhysical | JackPortIsOutput);
    size = array_length(ports_out);
    i = 0;
    printf("%i Input ports:\n", size);
    while (ports_out[i] != NULL) {
        printf("  %2i: %s\n", i + 1, ports_out[i]);
        i++;
    }
    jack_free(ports_out);
}

}
