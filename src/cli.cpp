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
        std::cerr << ABOUT <<
        "\nNo playback or record files specified. Nothing to do!\n";
        return false;
    }
    if (!args.output_file.empty() && args.input_file.empty() && !args.duration_secs) {
        std::cerr << "Recording requires a playback file name and/or a duration to be specified\n";
        return false;
    }
    if (args.input_channel_count && vm.count("in") != 0) {
        std::cerr << "Options --input-channel-count and --in cannot be set at the same time\n";
        return false;
    }
    if (args.duration_secs && *args.duration_secs < 0) {
        std::cerr << "Duration must not be negative\n";
        return false;
    }
    if (args.start_offset_secs < 0) {
        std::cerr << "Start offset must not be negative\n";
        return false;
    }
    // For compatibility with comma-separated input
    args.input_ports = split_ports(args.input_ports);
    args.output_ports = split_ports(args.output_ports);
    return true;
}
}

Args handle_cli(int argc, char** argv) {
    Args args;
    po::options_description opts("Options");
    opts.add_options()
        ("help,h",
            "Print this help message & exit")
        ("version,v", po::bool_switch(&args.show_version),
            "Print version and copyright info & exit")
        ("channels,c", po::bool_switch(&args.show_ports),
            "Print available Jack channels & exit")
        ("debug,d", po::bool_switch(&args.debug),
            "Allow debugging output")
        ("buffer,b", po::value(&args.buffer_size),
            "Jack buffer size in samples")
        ("in,i", po::value(&args.input_ports),
            "Jack input (record) channels, specified using a comma-separated list ; first item specifies which Jack channel to route to soundfile ch 1, etc")
        ("input-channel-count,I", po::value(&args.input_channel_count),
            "Number of input (record) channels to use (use alternatively with --in) ; the first I input channels will be used")
        ("out,o", po::value(&args.output_ports),
            "Jack output (playback) channels, specified using a comma-separated list ; first item specifies which Jack channel to route soundfile ch 1 to, etc ; use null to not play a particular soundfile channel")
        ("duration,D", po::value(&args.duration_secs),
            "Duration of playback and recording in s ; if not set, the duration of playback file will be used ; required for recording without playback ; use 0 to record until terminated with ^C")
        ("start,s", po::value(&args.start_offset_secs),
            "Offset to start at when reading playback file, in s")
        ("read-file,r", po::value(&args.input_file), "File path to read playback audio data from, in any format supported by libsndfile")
        ("write-file,w", po::value(&args.output_file), "File path to write recorded audio data to, in wav format ; warning, existing files will be overwritten")
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
        std::cout << ABOUT << "\n" << opts << "\n";
        std::exit(EXIT_SUCCESS);
    }
    if (args.show_version) {
        std::cout << ABOUT;
        std::exit(EXIT_SUCCESS);
    }
    if (!validate(vm, args)) {
        std::cout << "\n" << opts << "\n";
        std::exit(EXIT_FAILURE);
    }
    return args;
}
}
