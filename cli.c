#include "cli.h"

#include <argp.h>
#include <jack/jack.h>
#include <stdlib.h>
#include <stdbool.h>

const char *olinout_program_version = "olinout v 1.0";

/* Program documentation. */
static char doc[] =
    "olinout: Play and record multi-channel audio using jack.\n"
    "Based on recapture: https://gist.github.com/jedahu/5028736\n"
    "Bugs to <cbrown1@pitt.edu>";

/* A description of the arguments we accept. */
static char args_doc[] = "FILE_READ";

/* The options we understand. */
static struct argp_option options[] = {
    {"version", 'v', 0,      0,   "Print version info & exit" },
    {"buffer",  'b', "BUFFER_SIZE",     0,   "Jack buffer size" },
    {"debug",   'd', "1",    0,   "1=Print debugging messages; 0=Don't" },
    {"ports",   'p', 0,      0,   "Print available ports & exit" },
    {"in",      'i', "",     0,   "Input ports from jack, comma-separated" },
    {"out",     'o', "",     0,   "Output ports from jack, comma-separated" },
    {"write",   'w', "FILE", 0,   "Path to file to write to" },
    {"duration", 'D', "TIME_SECS_DBL", 0,
        "Duration of the playback and recording; if not set, will be equal to the length of playback file."},
    {"start",   's', "TIME_SECS_DBL", 0, "Offset the playback should start at." },
    { 0 } /*   An options vector should be terminated by an option with all fields zero. */
};

/* Parse a single option. */
static error_t parse_opt (int key, char *arg, struct argp_state *state) {

    /* Get the input argument from argp_parse, which we
       know is a pointer to our arguments structure. */
    struct arguments *arguments = state->input;
    switch (key) {
    case 'b':
        if (!(arguments->buffer = atoi(arg))) {
            argp_usage(state);
        }
      break;
    case 'p':
        arguments->show_ports = 1;
        break;
    case 'd':
        arguments->debug = atoi(arg);
        break;
    case 'v':
        arguments->version = 1;
        break;
    case 'i':
        arguments->ports_in = arg;
        break;
    case 'o':
        arguments->ports_out = arg;
        break;
    case 'w':
        arguments->file_write = arg;
        break;
    case 'D':
        if (!(arguments->duration = atof(arg))) {
            argp_usage (state);
        }
      break;
    case ARGP_KEY_ARG:
        if (arguments->file_read != NULL) {
            // Multiple positional arguments not allowed
            argp_usage (state);
        }
        arguments->file_read = arg;
        break;
    case ARGP_KEY_NO_ARGS:
        if (!arguments->show_ports && 
            !arguments->version && 
            !arguments->file_write) 
        {
            // At least one of the following params is mandatory:
            // -p, -v, -w or positional arg. 
            argp_usage (state);
        }
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static bool validate_arguments(const struct arguments* args) {
    if (args->show_ports || args->version) {
        // These args override any others and disable their validation
        return true;
    }
    if (args->file_write == NULL && args->file_read == NULL) {
        fprintf(stderr, "olinout: at least one file name for recording or playback must be present.\n");
        return false;
    }
    if (args->file_write != NULL && (args->file_read == NULL || args->duration == 0)) {
        fprintf(stderr, "olinout: recording requires either playback file name or duration to be set.\n");
        return false;
    }

    return true;
}

// Reasonable defaults for stereo working out-of-the-box on many Jack systems
char ports_in_def[] = "system:capture_1,system:capture_2";
char ports_out_def[] = "system:playback_1,system:playback_2";

struct arguments handle_cli(int argc, char** argv) {
    struct argp argp = { options, parse_opt, args_doc, doc };
    struct arguments arguments = {0};

    /* Default values. */
    arguments.buffer = 65536;
    arguments.debug = 1;
    arguments.file_write = NULL;
    // Default to stereo for one-click operation
    arguments.ports_in = ports_in_def;
    arguments.ports_out = ports_out_def;

    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    if (arguments.version) {
        printf ("%s, jack %s\n", olinout_program_version, jack_get_version_string());
        exit(0);
    }

    if (!validate_arguments(&arguments)) {
        exit(1);
    }
    return arguments;
}
