#include "cli.h"

#include <stdlib.h>
#include <argp.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <float.h>
#include <pthread.h>
#include <sndfile.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>

// recapture code paste begin:

#if 1
#define DEBUG(...) (fprintf(stderr, "olinout: "), fprintf(stderr, __VA_ARGS__))
#else
#define DEBUG
#endif
#define MSG(...) (fprintf(stderr, "olinout: "), fprintf(stderr, __VA_ARGS__))
#define ERR(...) (fprintf(stderr, "olinout: error: "), fprintf(stderr, __VA_ARGS__))

// Usage notice and some useful macros.

#define MAX_PORTS 32

// ### Structs and typedefs

// Note: This is in fact float.
typedef jack_default_audio_sample_t recap_sample_t;

// Note: original value of -1 used as "error flag" is on the edge of valid [-1, 1]
// sample value range and can appear in Jack stream naturally (esp. if driven > 100%);
// changing this to some highly improbable value should improve robustness.
#define RECAP_SAMPLE_ERR FLT_MAX

// `jack_default_audio_sample_t` is a little long.

typedef enum _recap_status {
 IDLE, RUNNING, DONE
} recap_status_t;

// This program starts two extra threads. The main thread sets up a number of callbacks which the jack process runs in realtime. The two extra threads are a read thread and a write thread. This split is necessary because reading and writing cannot occur within a realtime thread without wrecking its realtime guarantees. The read and write threads are connected to the jack thread by a ringbuffer each.

// Before initial synchronization the read thread is `IDLE`, once reading has commenced it is `RUNNING`, and when reading is finished it is `DONE`. The jack processing thread also uses `IDLE` and `DONE`.

typedef struct _recap_state {
 volatile int can_play;
 volatile int can_capture;
 volatile int can_read;
 volatile recap_status_t reading;
 volatile recap_status_t playing;
} recap_state_t;

// A single instance of this struct is shared among all three threads. Play and record start when `can_play`, `can_capture`, and `can_read` (which correspond to the three threads) are all true; they finish when `reading` and `playing` are both `DONE`.

typedef struct _recap_io_info {
 pthread_t thread_id;
 char* path;
 SNDFILE* file;
 jack_ringbuffer_t* ring;
 long underruns;
 recap_state_t* state;
} recap_io_info_t;

// Both read and write threads receive an instance of this struct as their only argument.

typedef struct _recap_process_info {
 long overruns;
 long underruns;
 recap_io_info_t* writer_info;
 recap_io_info_t* reader_info;
 recap_state_t* state;
} recap_process_info_t;

// An instance of this struct is passed to the callback responsible for processing the signals.

// ### Global values

const size_t sample_size = sizeof(recap_sample_t);

// This works out to be the size of a 32 bit float. Not bad given the sample size of CD audio is 16 bits.

pthread_mutex_t read_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ready_to_read = PTHREAD_COND_INITIALIZER;
pthread_mutex_t write_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ready_to_write = PTHREAD_COND_INITIALIZER;

// Locks and condition variables for each IO thread.

recap_process_info_t* proc_info;

// Needs to be global for signal handling.

/* read only after initialization in main() */
int channel_count_r = 0;
int frame_size_r = 0;
int channel_count_w = 0;
int frame_size_w = 0;
int frames_need = 0;
int frames_got = 0;
int jack_fs = 0;
int debug = 0;
jack_nframes_t ring_size = 65536; /* pow(4, 8) */
jack_port_t* recap_in_ports[MAX_PORTS];
jack_port_t* recap_out_ports[MAX_PORTS];
jack_client_t* client;

// Variables that are not changed subsequent to initialization and therefore do not need to be in thread specific structs.

// `channel_count_r` and `channel_count_w` hold the number of read and write signals. The `frame_size` of each is calculated by multiplying the channel count by the sample size. The default `ring_size` can be overridden by command line argument. `recap_in_ports` and `recap_out_ports` will hold the jack ports this client connects to.

// ### Helper functions

static size_t array_length(char** array) {
 int i = -1;
 while (array[++i] != NULL);
 return i;
}

// There is probably a more standard if not more general way to do this, but a) I couldn't find it, and b) this function does the job.

typedef recap_sample_t (*next_value_fn) (void*);

// TODO: This function is always called with `next_value()` function pointer, consider inlining it as function 
// pointers make this highly ineffective.
int uninterleave(recap_sample_t** buffers, size_t length, size_t count,
                next_value_fn next_value, void* arg) {
 int i;
 int k;
 for (i = 0; i < count; i++) {
   for (k = 0; k < length; k++) {
     recap_sample_t sample = next_value(arg);
     if (sample == RECAP_SAMPLE_ERR) {
       return -1;
     } else {
       buffers[k][i] = sample;
     }
   }
 }
 return 0;
}

// Pull next sample from jack_ringbuffer passed as `arg`, return `RECAP_SAMPLE_ERR` on underrun, 
// proper sample value otherwise.
recap_sample_t next_value(void* arg) {
 recap_sample_t sample;
 jack_ringbuffer_t* rb = (jack_ringbuffer_t*) arg;
 size_t count = jack_ringbuffer_read(rb, (char*) &sample, sample_size);
 if (count < sample_size) 
  return RECAP_SAMPLE_ERR;
 return sample;
}

// libsndfile expects multichannel data to be interleaved. `uninterleave()` uses a supplied function to read interleaved data and writes it to an array of output buffers (one per channel).

// `next_value()` simply reads the next `sample_size` bytes from the supplied ringbuffer.

typedef int (*write_value_fn) (recap_sample_t, void*);

// TODO: This function is always called with `write_value()` function pointer, consider inlining it as function 
// pointers make this highly ineffective.
int interleave(recap_sample_t** buffers, size_t length, size_t count,
              write_value_fn write_value, void* arg) {
 int i;
 int k;
 for (i = 0; i < count; i++) {
   for (k = 0; k < length; k++) {
     int status = write_value(buffers[k][i], arg);
     if (status) 
      return status;
   }
 }
 return 0;
}

// Push next sample to jack_ringbuffer passed as `arg`, return -1 status on overrun, 0 otherwise.
int write_value(recap_sample_t sample, void* arg) {
 jack_ringbuffer_t* rb = (jack_ringbuffer_t*) arg;
 size_t count = jack_ringbuffer_write(rb, (char*) &sample, sample_size);
 if (count < sample_size) 
  return -1;
 return 0;
}

// `interleave()` reads data from an array of input buffers and uses the supplied function to write it interleaved.

// `write_value()` simple writes `sample_size` bytes to the supplied ringbuffer.

static void recap_mute(recap_sample_t** buffers, int count, jack_nframes_t nframes) {
 int i;
 size_t bufsize = nframes * sample_size;
 for (i = 0; i < count; i++)
   memset(buffers[i], 0, bufsize);
}

// After playing has finished, jack output must be muted (zeroed) otherwise jack will continue playing whatever is left in the output buffers, looping through the last cycle of whatever signal was being played. Definitely not what you want.

// ### Signal handling

static void cancel_process(recap_process_info_t* info) {
 pthread_cancel(info->reader_info->thread_id);
 pthread_cancel(info->writer_info->thread_id);
}

static void signal_handler(int sig) {
 MSG("signal received, exiting ...\n");
 cancel_process(proc_info);
}

static void jack_shutdown(void* arg) {
 recap_process_info_t* info = (recap_process_info_t*) arg;
 MSG("JACK shutdown\n");
 cancel_process(info);
}

// Signal handing. `cancel_process()` ensures things are cleaned up nicely. `jack_shutdown()` is a callback that the jack process calls on exit.

// ### Thread abstraction

typedef int (*io_thread_fn) (recap_io_info_t*);
typedef void (*cleanup_fn) (void*);

#define FINISHED -1

static void* common_thread(pthread_mutex_t* lock, pthread_cond_t* cond, io_thread_fn fn, cleanup_fn cu, void* arg) {
 int* exit = (int*) malloc(sizeof(int));
 *exit = 0;
 int status = 0;
 recap_io_info_t* info = (recap_io_info_t*) arg;
 pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
 pthread_cleanup_push(cu, arg);
 pthread_mutex_lock(lock);
 while (1) {
   if ((status = fn(info)) != 0) break;
   pthread_cond_wait(cond, lock);
 }
 pthread_mutex_unlock(lock);
 *exit = status;
 if (status == FINISHED) *exit = 0;
 pthread_exit(exit);
 pthread_cleanup_pop(1);
}

// This abstracts out the common parts of setting up a thread and its loop. The supplied `io_thread_fn` function is executed every iteration until it returns non zero. The supplied `cleanup_fn` is called whenever the thread is exited. After each iteration the thread waits until it is signalled to continue.

typedef int (*io_test_fn) (recap_io_info_t*);
typedef size_t (*io_size_fn) (recap_io_info_t*);
typedef int (*io_body_fn) (void*, size_t, recap_io_info_t*);

static int io_thread(io_test_fn can_run, io_test_fn is_done, io_size_fn ring_space, io_body_fn body, recap_io_info_t* info) {
 int status = 0;
 if (can_run(info)) {
   size_t space = ring_space(info);
   if (is_done(info)) {
     status = FINISHED;
   } else if (space > 0) {
     void* buf = malloc(space); /* TODO malloc and memset once only */
     status = body(buf, space, info);
     free(buf);
   }
 }
 return status;
}

// Further abstracted out is the code common to the read and write threads. `io_test_fn` checks when the thread is finished and should exit; `io_size_fn` returns how much read or write space is available; `io_body_fn` contains code specific to reading or writing.

// It would be good to malloc `void* buf` only once at the beginning of the thread to ensure no pagefaults. However, since the allocation does not occur in a realtime thread and no overruns or underruns (dropouts) were observed in testing, it was not a high priority to fix.

// ### Thread implementation

static int reader_can_run(recap_io_info_t* info) {
 return info->state->can_read;
}

static int writer_can_run(recap_io_info_t* info) {
 return info->state->can_capture;
}

static int reader_is_done(recap_io_info_t* info) {
 return 0;
}

static int writer_is_done(recap_io_info_t* info) {
 return jack_ringbuffer_read_space(info->ring) == 0 && info->state->playing == DONE;
}

static size_t reader_space(recap_io_info_t* info) {
 return jack_ringbuffer_write_space(info->ring);
}

static size_t writer_space(recap_io_info_t* info) {
 return jack_ringbuffer_read_space(info->ring);
}

// Read and write implementations of the above typedefs.

static int reader_body(void* buf, size_t space, recap_io_info_t* info) {
 static int underfill = 0;
 int status = 0;
 sf_count_t nframes = space / frame_size_r;
 sf_count_t frame_count = sf_readf_float(info->file, buf, nframes);
 if (frame_count == 0) {
   if (debug == 1) { DEBUG("reached end of read file: %s\n", info->path); }
   info->state->reading = DONE;
   status = FINISHED;
 } else if (underfill > 0) {
   if (frames_got == frames_need) {
    if (debug == 1) { DEBUG("reached end of read file: %s\n", info->path); }
    info->state->reading = DONE;
    status = FINISHED;
   } else {
    ERR("cannot read from file: %s\n", info->path);
    status = EIO;
   }
 } else {
   sf_count_t size = frame_count * frame_size_r;
   if (jack_ringbuffer_write(info->ring, buf, size) < size) {
     ++info->underruns;
     ERR("reader thread: buffer underrun\n");
   }
   frames_got = frames_got + frame_count;
   if (frames_got < frames_need) { 
    if (debug == 1) { DEBUG("read %i of %i frames\r", frames_got, frames_need); }
   } else { 
    if (debug == 1) { DEBUG("read %i of %i frames\n", frames_got, frames_need); }
   }
   info->state->reading = RUNNING;
   if (frame_count < nframes && underfill == 0) {
     if (debug == 1) { DEBUG("expected %ld frames but only read %ld,\n", nframes, frame_count); }
     if (debug == 1) { DEBUG("wait for one cycle to make sure.\n"); }
     ++underfill;
   }
 }
 return status;
}

// Reader implementation of `io_body_fn`. It is impossible to tell if the first underfill is caused by IO problems or by reaching the end of the sound file. If the frame count for the following cycle is zero, it is assumed that the end of the file has been reached; otherwise if `underfill` is greater than zero it must be an IO issue so the thread exits.

// The use of `static int underfill` means no more than one reader thread can be active during the lifetime of the program.

static int writer_body(void* buf, size_t space, recap_io_info_t* info) {
 int status = 0;
 sf_count_t nframes = space / frame_size_w;
 jack_ringbuffer_read(info->ring, buf, space);
 if (sf_writef_float(info->file, buf, nframes) < nframes) {
   ERR("cannot write to file (%s)\n", sf_strerror(info->file));
   status = EIO;
 }
 //DEBUG("wrote %5ld frames\n", nframes);
 return status;
}

// Writer implementatino of `io_body_fn`.

static int reader_thread_fn(recap_io_info_t* info) {
 return io_thread(&reader_can_run, &reader_is_done,
                  &reader_space, &reader_body, info);
}

static int writer_thread_fn(recap_io_info_t* info) {
 return io_thread(&writer_can_run, &writer_is_done,
                  &writer_space, &writer_body, info);
}

// Read and write implementations of `io_thread_fn`. Due to the earlier abstraction these definitions are simple.

static void io_cleanup(void* arg) {
 recap_io_info_t* info = (recap_io_info_t*) arg;
 sf_close(info->file);
}

static void io_free(recap_io_info_t* info) {
 jack_ringbuffer_free(info->ring);
}

// `io_cleanup()` is passed to `common_thread` as the thread cleanup callback. `io_free()` is used at the end of `main()` to free resources which the jack thread may still be using after the IO threads exit.

static void* writer_thread(void* arg) {
 return common_thread(&write_lock, &ready_to_write,
                      &writer_thread_fn, &io_cleanup, arg);
}

static void* reader_thread(void* arg) {
 return common_thread(&read_lock, &ready_to_read,
                      &reader_thread_fn, &io_cleanup, arg);
}

// Functions to start writer and reader threads.

// ### Main jack callback

static int process(jack_nframes_t nframes, void* arg) {
 recap_process_info_t* info = (recap_process_info_t*) arg;
 recap_state_t* state = info->state;

// The main jack callback. This function must read `nframes` of signal from the connected input ports and write `nframes` of signal to the connected output ports. The size of `nframes` is determined by the caller (jack).

 if (!state->can_play || !state->can_capture || !state->can_read)
   return 0;

// No point reading or writing anything to jack’s buffers if everything isn’t ready to go.

 recap_sample_t* in[MAX_PORTS];
 recap_sample_t* out[MAX_PORTS];
 int i = 0;
 jack_port_t* prt;
 while ((prt = recap_in_ports[i]) != NULL) {
   in[i++] = (recap_sample_t*) jack_port_get_buffer(prt, nframes);
 }
 in[i] = NULL;

 i = 0;
 while ((prt = recap_out_ports[i]) != NULL) {
   out[i++] = (recap_sample_t*) jack_port_get_buffer(prt, nframes);
 }
 out[i] = NULL;

// Get the signal buffers of each input and output port. It is recommended in the jack documentation that these are not cached.

 if (state->playing != DONE && state->reading != IDLE) {

// If `state->reading` is `IDLE` there is nothing to play yet, and if `state->playing` is `DONE` it is time to exit the program.

   jack_ringbuffer_t* rring = info->reader_info->ring;
   size_t space = jack_ringbuffer_read_space(rring);
   if ((space == 0 && state->reading == DONE) || (frames_got == frames_need)) {
     state->playing = DONE;
     recap_mute(out, channel_count_r, nframes);
   } else {
     if (frames_need - frames_got < nframes) {
      int err = uninterleave(out, channel_count_r, frames_need-frames_got, &next_value, rring);
      if (err && frames_got < frames_need) {
        ++info->underruns;
        ERR("control thread: buffer underrun\n");
      }
     } else {
      int err = uninterleave(out, channel_count_r, nframes, &next_value, rring);
      if (err && frames_got < frames_need) {
        ++info->underruns;
        ERR("control thread: buffer underrun\n");
      }
     }
   }

// This, the guts of the processing is simply uninterleaving the file data and writing it to the buffers of the appropriate output ports. Jack handles the rest.

   jack_ringbuffer_t* wring = info->writer_info->ring;
   int err = interleave(in, channel_count_w, nframes, &write_value, wring);
   if (err) {
     ++info->overruns;
     ERR("control thread: buffer overrun\n");
   }
 }

// Similarly simple. Interleaving the input port data and writing to the writer thread’s ringbuffer.

 if (pthread_mutex_trylock(&read_lock) == 0) {
   pthread_cond_signal(&ready_to_read);
   pthread_mutex_unlock(&read_lock);
 }

 if (pthread_mutex_trylock(&write_lock) == 0) {
   pthread_cond_signal(&ready_to_write);
   pthread_mutex_unlock(&write_lock);
 }
 return 0;
}

// Data has been written to the writer thread’s ringbuffer and removed from the reader thread’s ringbuffer, so signal each thread to begin another iteration.

// ### Thread setup and running

static int setup_writer_thread(recap_io_info_t* info) {
 int status = 0;
 SF_INFO sf_info;
 sf_info.samplerate = jack_get_sample_rate(client);
 sf_info.channels = channel_count_w;
 sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_32;
 if ((info->file = sf_open(info->path, SFM_WRITE, &sf_info)) == NULL) {
   ERR("cannot open file to write \"%s\" (%s)\n",
       info->path, sf_strerror(info->file));
   jack_client_close(client);
   status = EIO;
 }
 if (debug == 1) { DEBUG("file to write: %s\n", info->path); }
 if (debug == 1) { DEBUG("writing %i channels\n", channel_count_w); }
 info->ring = jack_ringbuffer_create(sample_size * ring_size);
 memset(info->ring->buf, 0, info->ring->size);
 info->state->can_capture = 0;
 pthread_create(&info->thread_id, NULL, writer_thread, info);
 return status;
}

// Set up resources for the writer thread then create the thread. This means opening a (multichannel) WAV file to write to, creating a ringbuffer, and touching all its allocated memory to prevent pagefaults later on.

static int setup_reader_thread(recap_io_info_t* info) {
 int status = 0;
 SF_INFO sf_info;
 sf_info.format = 0;
 if (debug == 1) { DEBUG("file to read: %s\n", info->path); }
 if ((info->file = sf_open(info->path, SFM_READ, &sf_info)) == NULL) {
   ERR("cannot read file: %s\n", info->path);
   status = EIO;
 }
 channel_count_r = sf_info.channels;
 frame_size_r = channel_count_r * sample_size;
 if (debug == 1) { DEBUG("reading %i channels\n", channel_count_r); }
 frames_need = sf_info.frames;
 if (sf_info.samplerate != jack_get_sample_rate(client)) {
   ERR("output sound file sample rate: %i; jack sample rate: %i)\n", sf_info.samplerate, jack_get_sample_rate(client));
   cancel_process(proc_info);
 }
 info->ring = jack_ringbuffer_create(sample_size * ring_size);
 memset(info->ring->buf, 0, info->ring->size);
 info->state->can_read = 0;
 pthread_create(&info->thread_id, NULL, reader_thread, info);
 return status;
}

// Same purpose as the previous function but for the reader thread. Opens a (multichannel) WAV file to read from, creates a ringbuffer, and touches all its memory.

// Similarites between these two functions could probably be extracted into a general `setup_io_thread()` function.

static int run_io_thread(recap_io_info_t* info) {
 int* status;
 int ret;
 pthread_join(info->thread_id, (void**) &status);
 if (status == PTHREAD_CANCELED) {
   ret = EPIPE;
 } else {
   ret = *status;
   free(status);
 }
 return ret;
}

// Joins to a thread and returns its exit status.

// ### Port registration and connection

static jack_port_t* register_port(char* name, int flags) {
  jack_port_t* port;
  if ((port = jack_port_register(client, name, JACK_DEFAULT_AUDIO_TYPE, flags, 0)) == 0) {
    if (debug == 1) { DEBUG("cannot register port \"%s\"\n", name); }
    jack_client_close(client);
    exit(1);
  }
  return port;
}

// Register the named port with the jack process.

static void connect(const char* out, const char* in) {
 if (jack_connect(client, out, in)) {
   if (debug == 1) { DEBUG("cannot connect port \"%s\" to \"%s\"\n", out, in); }
   jack_client_close(client);
   exit(1);
 }
}

static void connect_ports(char** in_names, char** out_names) {
 int i;
 char* s;
 for (i = 0; i < channel_count_w; i++) {
   char prt[32];
   sprintf(prt, "olinout:input_%i", i);
   char* shrt = prt + strlen("olinout:");
   register_port(shrt, JackPortIsInput);
   recap_in_ports[i] = jack_port_by_name(client, prt);
   if ((s = in_names[i]) != NULL) connect(s, prt);
 }
 recap_in_ports[i] = NULL;
 for (i = 0; i < channel_count_r; i++) {
   char prt[32];
   sprintf(prt, "olinout:output_%i", i);
   char* shrt = prt + strlen("olinout:");
   register_port(shrt, JackPortIsOutput);
   recap_out_ports[i] = jack_port_by_name(client, prt);
   if ((s = out_names[i]) != NULL) connect(prt, s);
 }
 recap_out_ports[i] = NULL;
}

// Initialize `recap_in_ports` and `recap_out_ports` with this client’s in and out ports, and connect them to the supplied jack ports.

// ### Set callbacks and handlers

static void set_handlers(jack_client_t* client, recap_process_info_t* info) {
 jack_set_process_callback(client, process, info);
 jack_on_shutdown(client, jack_shutdown, info);
 signal(SIGQUIT, signal_handler);
 signal(SIGTERM, signal_handler);
 signal(SIGHUP, signal_handler);
 signal(SIGINT, signal_handler);
}

// Set jack callbacks and signal handlers.

// ### Run jack client

static int run_client(jack_client_t* client, recap_process_info_t* info) {
 recap_state_t* state = info->state;

 state->can_play    = 1;
 state->can_capture = 1;
 state->can_read    = 1;

 int reader_status = run_io_thread(info->reader_info);
 int writer_status = run_io_thread(info->writer_info);
 int other_status = 0;

 if (info->overruns > 0) {
   ERR("olinout failed with %ld overruns.\n", info->overruns);
   ERR("try a bigger buffer than -b %" PRIu32 ".\n", ring_size);
   other_status = EPIPE;
 }
 long underruns = info->underruns + info->reader_info->underruns;
 if (underruns > 0) {
   ERR("olinout failed with %ld underruns.\n", underruns);
   ERR("try a bigger buffer than -b %" PRIu32 ".\n", ring_size);
   other_status = EPIPE;
 }
 return reader_status || writer_status || other_status;
}

// Run reader and writer threads, and return their status.

// Looks like `can_play`, `can_capture`, and `can_read` could be collapsed in to one field.

// recapture code paste end

static void split_names(char* str, char** list) {
 int i = 0;
 char* s = strtok(str, ",");
 while (s != NULL) {
   list[i++] = s;
   s = strtok(NULL, ",");
 }
 list[i] = NULL;
}

int main (int argc, char **argv) {

 recap_process_info_t info;
 recap_io_info_t reader_info;
 recap_io_info_t writer_info;
 recap_state_t state;
 memset(&info, 0, sizeof(info));
 memset(&reader_info, 0, sizeof(reader_info));
 memset(&writer_info, 0, sizeof(writer_info));
 memset(&state, 0, sizeof(state));
 info.reader_info = &reader_info;
 info.writer_info = &writer_info;
 info.state = &state;
 reader_info.state = &state;
 writer_info.state = &state;
 state.can_play = 0;
 state.reading = IDLE;
 state.playing = IDLE;
 proc_info = &info;

 struct arguments arguments = handle_cli(argc, argv);
 debug = arguments.debug;

 if ((client = jack_client_open("olinout", JackNullOption, NULL)) == 0) {
   ERR("jack server not running?\n");
   exit(1);
 }

 // output = capture
 if (arguments.show_ports == 1) {
  int size;
  const char **ports_in;
  ports_in = jack_get_ports (client, NULL, NULL,
  JackPortIsPhysical|JackPortIsInput);
  for (size = 0; ports_in[size] != NULL; size++);
 
  int i = 0;
  printf("%i Output ports:\n", size);
  while (ports_in[i] != '\0'){
      printf("  %2i: %s\n", i+1, ports_in[i]);
      i++;
  }
  free(ports_in);
  const char **ports_out;
  ports_out = jack_get_ports (client, NULL, NULL,
  JackPortIsPhysical|JackPortIsOutput);
  for (size = 0; ports_out[size] != NULL; size++);
 
  i = 0;
  printf("%i Input ports:\n", size);
  while (ports_out[i] != '\0'){
      printf("  %2i: %s\n", i+1, ports_out[i]);
      i++;
  }
  free(ports_out);

  exit(0);
 }

 jack_fs = jack_get_sample_rate (client);
 if (debug == 1) { DEBUG ("jack sample rate: %i\n", jack_fs); }

 set_handlers(client, proc_info);

 char* in_port_names[MAX_PORTS];
 char* out_port_names[MAX_PORTS];

 split_names(arguments.ports_in, in_port_names);
 split_names(arguments.ports_out, out_port_names);

//  int count = 0;
//  char** p_in  = in_port_names; /* assign a pointer temp that we will use for the iteration */
//  while(*p_in != NULL)     /* while the value contained in the first level of temp is not NULL */
//  {
//      printf("INPORT: %s\n", *p_in++); /* print the value and increment the pointer to the next cell */
//      count++;
//  }
//  printf("Count is %d\n", count);

// Port names and file paths.

 proc_info->reader_info->path = arguments.file_read;
 proc_info->writer_info->path = arguments.file_write;

 channel_count_w = array_length(in_port_names);
 frame_size_w = channel_count_w * sample_size;
// Writer thread channel count and frame size. Those for the reader thread are taken from the input file in `setup_reader_thread()`.

// Connect to jack and set up jack and signal callbacks.

 int status = 0;
 if (!(status = setup_writer_thread(proc_info->writer_info)) &&
     !(status = setup_reader_thread(proc_info->reader_info))) {
   if (jack_activate(client)) {
     ERR("cannot activate client\n");
     status = 1;
   } else {
     connect_ports(in_port_names, out_port_names);
     if (debug == 1) { DEBUG("connected ports\n"); }
     status = run_client(client, proc_info);
     jack_client_close(client);
   }
 }

// Provided the IO threads execute ok, run this client and then close it once `run_client()` returns.

 io_free(proc_info->reader_info);
 io_free(proc_info->writer_info);
 return status;
}
