#pragma once

/* Used by main to communicate with parse_opt. */
// TODO: who owns strings? Minor leak, ignored.
struct arguments {
  int show_ports;
  int buffer;
  int debug; 
  int version;
  char *ports_in;
  char *ports_out;
  char *file_write;
  char *file_read;
  double duration;
  double start_offset;
};

struct arguments handle_cli(int argc, char** argv);
