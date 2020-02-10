#pragma once
#include "types.hpp"

namespace olo {

struct Args {
    bool show_ports = false;
    bool debug
#ifdef NDEBUG
        = false;
#else
        = true;
#endif
    bool show_version = false;
    size_t buffer_size = BUFFER_SIZE_DEFAULT;
    vector<string> input_ports = {"system:capture_1", "system:capture_2"}; 
    vector<string> output_ports = {"system:playback_1", "system:playback_2"};
    string input_file;
    string output_file;
    double duration_secs = 0.;
    double start_offset_secs = 0.;
};

Args handle_cli(int argc, char** argv);

}
