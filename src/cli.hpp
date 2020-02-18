#pragma once
#include "types.hpp"

namespace olo {

struct Args {
    static const vector<string> PORTS_DEFAULT;

    bool show_ports = false;
    bool debug = false;
    bool show_version = false;
    size_t buffer_size = BUFFER_SIZE_DEFAULT;
    optional<size_t> input_channel_count;
    vector<string> input_ports = PORTS_DEFAULT;
    vector<string> output_ports = PORTS_DEFAULT;
    string input_file;
    string output_file;
    double duration_secs = 0.;
    double start_offset_secs = 0.;
};

Args handle_cli(int argc, char** argv);

}
