#include "types.hpp"
#include "cli.hpp"
#include "jack_client.hpp"
#include "io.hpp"
#include "reactor.hpp"
#include "log.hpp"

#include <jack/jack.h>

#include <memory>
#include <exception>
#include <iostream>

namespace olo {
using std::unique_ptr;

void main(int argc, char** argv) {
    auto args = handle_cli(argc, argv);
    if (args.debug) {
        set_loglevel(DEBUG);
    }
    JackClient client(JACK_CLIENT_NAME);
    if (args.show_ports) {
        client.dump_ports();
        return;
    }

    unique_ptr<Reader> reader;
    if (!args.input_file.empty()) {
        reader.reset(new Reader {
            args.input_file,
            client.sample_rate(),
            args.output_ports.size(),
            args.buffer_size,
            args.duration_secs,
            args.start_offset_secs
        });
    }

    unique_ptr<Writer> writer;
    if (!args.output_file.empty()) {
        writer.reset(new Writer {
            args.output_file,
            client.sample_rate(),
            args.input_ports.size(),
            args.buffer_size,
            args.duration_secs
        });
    }

    Reactor reactor {
        client, 
        args.input_ports, 
        args.output_ports,
        reader.get(),
        writer.get(),
    };

    reactor.wait_finished();

    if (reader) {
        reader->stop();
        std::cout << "frames read: " << reader->frames_done() << "\n";
    }
    if (writer) {
        writer->stop();
        std::cout << "frames written: " << writer->frames_done() << "\n";
    }
}
}

int main(int argc, char** argv) {
    try {
        olo::main(argc, argv);
        return EXIT_SUCCESS;
    } catch (std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
