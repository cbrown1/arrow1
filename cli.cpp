#include "cli.hpp"

#include <boost/program_options.hpp>
#include <boost/tokenizer.hpp>

#include <iostream>
#include <cstdlib>

namespace olo {
namespace po = boost::program_options;

namespace {
auto split_ports(const vector<string>& ports) {
    vector<string> res;
    for (auto& port: ports) {
        boost::tokenizer<boost::char_separator<char>> tok(port, 
            boost::char_separator<char>(","));
        std::copy(tok.begin(), tok.end(), std::back_inserter(res));
    }
    return res;
}

bool validate(const po::variables_map& vm, Args& args) {
    if (args.show_ports || args.show_version) {
        // These args override any others and disable their validation
        return true;
    }
    if (args.output_file.empty() && args.input_file.empty()) {
        std::cerr << "at least one file name for recording or playback must be present\n";
        return false;
    }
    if (!args.output_file.empty() && args.input_file.empty() && args.duration_secs == 0) {
        std::cerr << "recording requires either playback file name or duration to be set\n";
        return false;
    }
    if (vm.count("duration") && args.duration_secs == 0) {
        std::cerr << "record duration can't be 0\n";
        return false;
    }
    // For compatibility with comma-separated input
    args.input_ports = split_ports(args.input_ports);
    args.output_ports = split_ports(args.output_ports);
    return true;
}

string dump_version() {
    return JACK_CLIENT_NAME + " v" +VERSION +": Play and record multi-channel audio using jack.\n"
        "Based on recapture:https://gist.github.com/jedahu/5028736\n"
        "Bugs to <cbrown1@pitt.edu>\n";
}
}

Args handle_cli(int argc, char** argv) {
    Args args;
    po::options_description opts("Options");
    opts.add_options()
        ("help,h", 
            "Produce help message")
        ("version,v", po::bool_switch(&args.show_version), 
            "Print version info & exit")
        ("ports,p", po::bool_switch(&args.show_ports), 
            "Print available ports & exit")
        ("debug,d", po::bool_switch(&args.debug), 
            "Allow debugging output")
        ("buffer,b", po::value(&args.buffer_size), 
            "Jack buffer size")
        ("in,i", po::value(&args.input_ports), 
            "Jack input ports")
        ("out,o", po::value(&args.output_ports), 
            "Jack output ports")
        ("duration,D", po::value(&args.duration_secs), 
            "Duration of the playback and recording; if not set, will be equal to the length of playback file")
        ("start,s", po::value(&args.start_offset_secs), 
            "Offset the playback should start at")
        ("input-file,r", po::value(&args.input_file), "Input audio file")
        ("output-file,w", po::value(&args.output_file), "Output audio file")
    ;
    po::positional_options_description pos;
    pos.add("input-file", 1).add("output-file", 1);

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv)
                .options(opts)
                .positional(pos)
                .run(), 
            vm);
        po::notify(vm);
    } catch (po::error& e) {
        std::cerr << e.what() << "\n";
        std::cout << "\n" << opts << "\n";
        std::exit(EXIT_FAILURE);
    }

    if (vm.count("help")) {
        std::cout << dump_version() << "\n" << opts << "\n";
        std::exit(EXIT_SUCCESS);
    }
    if (args.show_version) {
        std::cout << dump_version();
        std::exit(EXIT_SUCCESS);
    }
    if (!validate(vm, args)) {
        std::cout << "\n" << opts << "\n";
        std::exit(EXIT_FAILURE);
    }
    return args;
}
}
