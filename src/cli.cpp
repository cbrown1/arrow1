#include "cli.hpp"

#include <boost/program_options.hpp>
#include <boost/tokenizer.hpp>

#include <iostream>
#include <cstdlib>

namespace olo {
const vector<string> Args::PORTS_DEFAULT = {"__default"};

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
        std::cerr << ABOUT +
        "\nNo playback or record file specified. Nothing to do!\n";
        return false;
    }
    if (!args.output_file.empty() && args.input_file.empty() && args.duration_secs == 0) {
        std::cerr << "Recording requires a playback file name and/or a duration to be specified\n";
        return false;
    }
    if (vm.count("duration") && args.duration_secs == 0) {
        std::cerr << "Record duration can't be 0\n";
        return false;
    }
    // For compatibility with comma-separated input
    args.input_ports = split_ports(args.input_ports);
    args.output_ports = split_ports(args.output_ports);
    return true;
}

string dump_version() {
    return ABOUT;
}
}

Args handle_cli(int argc, char** argv) {
    Args args;
    po::options_description opts("Options");
    opts.add_options()
        ("help,h",
            "Print this help message & exit")
        ("version,v", po::bool_switch(&args.show_version),
            "Print version info & exit")
        ("channels,c", po::bool_switch(&args.show_ports),
            "Print available Jack channels & exit")
        ("debug,d", po::bool_switch(&args.debug),
            "Allow debugging output")
        ("buffer,b", po::value(&args.buffer_size),
            "Jack buffer size in samples")
        ("in,i", po::value(&args.input_ports),
            "Jack input (record) channels, specified using a comma-separated list; first item specifies soundfile ch 1, etc")
        ("out,o", po::value(&args.output_ports),
            "Jack output (playback) channels, specified using a comma-separated list; first item specifies soundfile ch 1, etc")
        ("duration,D", po::value(&args.duration_secs),
            "Duration of playback and recording in s; if not set, will be equal to the length of playback file")
        ("start,s", po::value(&args.start_offset_secs),
            "Offset to start at when reading audio file, in s")
        ("play-file,p", po::value(&args.input_file), "File path to read playback audio from, in any format supported by libsndfile")
        ("record-file,r", po::value(&args.output_file), "File path to write recorded audio to, in wav format")
    ;
    po::positional_options_description pos;
    pos.add("play-file", 1).add("record-file", 1);

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
