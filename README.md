[![Build Status](https://travis-ci.org/cbrown1/arrow1.svg?branch=master)](https://travis-ci.org/cbrown1/arrow1)

# arrow1

Play and record multi-channel audio using jack.

This is the same code as [recapture](https://gist.github.com/jedahu/5028736#file-multichannel-play-record-jack-md) with a few minor bugfixes, and improved logging and option handling. But the essential code is identical.

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

Or with CMake, which gives more control over the process:

```bash
cd arrow1
mkdir build && cd build && cmake ../src && make
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
$ arrow1 -p test/2_channels.wav -o system:playback_1,system:playback_2 -r test.wav -i system:capture_2 -D 5.2
frames read: 132300 (3.004s)
frames written: 229320 (5.204s)
$
```
The same thing, using aliases created above:

```bash
$ arrow1 -p test/2_channels.wav -o in1,in2 -r test.wav -i out2 -D 5.2
frames read: 132300 (3.004s)
frames written: 229320 (5.204s)
$ 
```

Play channels 2 & 4 of a 6-channel file:

```bash
$ arrow1 -p test/6_channels.wav -o null,system:playback_1,null,system:playback_2,null,null
frames read: 132300 (3.004s)
double free or corruption (out)
Aborted
```


## TODO

- Default playback channel map; soundfile_ch1 -> jack_output_ch1, etc.

- Indefinite record; user specifies --duration 0, which means hit a key to stop

- Proper Python wrapper (no temp files, etc)


## Authors

- [**Christopher Brown**](https://github.com/cbrown1)

- [**Andrzej Ciarkowski**](https://github.com/andrzejc)

- **Jeremy Hughes** - [recapture](https://gist.github.com/jedahu/5028736#file-multichannel-play-record-jack-md)

## License

This project is licensed under the GPLv3 - see the [LICENSE.md](LICENSE.md) file for details.
