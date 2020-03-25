[![Build Status](https://travis-ci.org/cbrown1/arrow1.svg?branch=master)](https://travis-ci.org/cbrown1/arrow1)

# arrow1

Play and record multi-channel audio using Jack.

## Prerequisites

- [jack](http://jackaudio.org/)

- [libsndfile](http://www.mega-nerd.com/libsndfile/)

- [Boost](https://www.boost.org/) - Apart from header libraries libboost-program-options binary is required.

## Installing

### Download:

```bash
git clone https://github.com/cbrown1/arrow1.git
```

### Compile and install:

```bash
cd arrow1
make && sudo make install
```

Or with [CMake](https://cmake.org/), which gives more control over the process:

```bash
cd arrow1
mkdir build && cd build && cmake .. && make
```

## Usage

Set port aliases in jack for convenience. Afterwards, ports can be called via out1, out2, in1, in2, etc. Aliases do not persist after restarts. (Note: From jack's perspective, capture or record ports are 'output' ports, and vice-versa)

```bash
ins=$(jack_lsp -p | awk '/output/{print previous_line}{previous_line=$0}')
i=1
for j in ${ins}; do jack_alias ${j} in${i}; ((i++)); done
outs=$(jack_lsp -p | awk '/input/{print previous_line}{previous_line=$0}')
i=1
for j in ${outs}; do jack_alias ${j} out${i}; ((i++)); done
```

If you don't want to use aliases, go the hard way. List jack port names (your system will have different names):

```bash
$ arrow1 -c
2 Output (playback) channels:
   1: system:playback_1
   2: system:playback_2
2 Input (record) channels:
   1: system:capture_1
   2: system:capture_2
$
```

Play a two-channel soundfile and record for 5.2s from channel 2 and write to a soundfile:

```bash
$ arrow1 -r test/2_channels.wav -o system:playback_1,system:playback_2 -w test.wav -i system:capture_2 -D 5.2
frames read: 132300 (3.004s)
frames written: 229320 (5.204s)
$
```

The same thing, using aliases created above:

```bash
$ arrow1 -r test/2_channels.wav -o in1,in2 -w test.wav -i out2 -D 5.2
frames read: 132300 (3.004s)
frames written: 229320 (5.204s)
$
```

Play channels 2 and 6 of a 6-channel file:

```bash
$ arrow1 -r test/6_channels.wav -o null,system:playback_1,null,null,null,system:playback_2
frames read: 132300 (3.004s)
```

Record from all available Jack inputs until explicitly stopped with ^C:

```bash
$ arrow1 --duration=0 -w foo.wav
^CReactor::signal_handler_(): stopping on signal 2
frames written: 368896 (7.690s)
```


## Notes

- This project was developed for recording room and head-related impulse responses (RIRs and HRIRs)

- To be clear, this program is meant for play-and-record, not record-and-play

- It was originally a fork of [recapture](https://gist.github.com/jedahu/5028736#file-multichannel-play-record-jack-md), but has since been completely re-written

- The name is an old Twilight Zone reference


## Related Projects

- [ecasound](http://www.eca.cx/ecasound/): multitrack audio processing software package


## TODO

- Proper Python wrapper (no temp files, etc)


## Authors

- [**Christopher Brown**](https://github.com/cbrown1)

- [**Andrzej Ciarkowski**](https://github.com/andrzejc)

- **Jeremy Hughes** - [recapture](https://gist.github.com/jedahu/5028736#file-multichannel-play-record-jack-md)

## License

This project is licensed under the GPLv3 - see the [LICENSE.md](LICENSE.md) file for details.
