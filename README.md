[![Build Status](https://travis-ci.org/cbrown1/olinout.svg?branch=master)](https://travis-ci.org/cbrown1/olinout)

# olinout

Play and record multi-channel audio using jack.

This is the same code as [recapture](https://gist.github.com/jedahu/5028736#file-multichannel-play-record-jack-md) with a few minor bugfixes, and improved logging and option handling. But the essential code is identical.

## Prerequisites

- [jack](http://jackaudio.org/)

- [libsndfile](http://www.mega-nerd.com/libsndfile/)

- [Boost](https://www.boost.org/) - Apart from header libraries libboost-program-options binary is required.

## Installing

### Download:

```bash
git clone https://github.com/cbrown1/olinout.git
```

### Compile and install:

```bash
make && sudo make install
```

Or with CMake, which gives more control over the process:

```bash
mkdir build && cd build && cmake ... && make
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
$ olinout --ports
8 Output ports:
   1: firewire_pcm:000a35007ca897e1_pbk_analog-1_out
   2: firewire_pcm:000a35007ca897e1_pbk_analog-2_out
   3: firewire_pcm:000a35007ca897e1_pbk_analog-3_out
   4: firewire_pcm:000a35007ca897e1_pbk_analog-4_out
   5: firewire_pcm:000a35007ca897e1_pbk_analog-5_out
   6: firewire_pcm:000a35007ca897e1_pbk_analog-6_out
   7: firewire_pcm:000a35007ca897e1_pbk_analog-7_out
   8: firewire_pcm:000a35007ca897e1_pbk_analog-8_out
8 Input ports:
   1: firewire_pcm:000a35007ca897e1_cap_analog-1_in
   2: firewire_pcm:000a35007ca897e1_cap_analog-2_in
   3: firewire_pcm:000a35007ca897e1_cap_analog-3_in
   4: firewire_pcm:000a35007ca897e1_cap_analog-4_in
   5: firewire_pcm:000a35007ca897e1_cap_analog-5_in
   6: firewire_pcm:000a35007ca897e1_cap_analog-6_in
   7: firewire_pcm:000a35007ca897e1_cap_analog-7_in
   8: firewire_pcm:000a35007ca897e1_cap_analog-8_in
$ 
```
Play a two-channel soundfile, record from channel 2 to a soundfile (no aliases):

```bash
$ olinout -o firewire_pcm:000a35007ca897e1_pbk_analog-1_out,firewire_pcm:000a35007ca897e1_pbk_analog-1_out -i firewire_pcm:000a35007ca897e1_cap_analog-2_in --output-file in_1_channel.wav test/2_channels.wav
olinout: jack sample rate: 44100
olinout: file to write: in_1_channel.wav
olinout: writing 1 channels
olinout: file to read: test/2_channels.wav
olinout: reading 2 channels
olinout: connected ports
olinout: read 122880 of 122880 frames
olinout: reached end of read file: test/2_channels.wav
$ 
```
The same thing, using aliases created above:

```bash
$ olinout --out=out1,out2 --in=in2 --output-file=in_1_channel.wav test/2_channels.wav
olinout: jack sample rate: 44100
olinout: file to write: in_1_channel.wav
olinout: writing 1 channels
olinout: file to read: test/2_channels.wav
olinout: reading 2 channels
olinout: connected ports
olinout: read 122880 of 122880 frames
olinout: reached end of read file: test/2_channels.wav
$ 
```

## Authors

- **Jeremy Hughes** - [recapture](https://gist.github.com/jedahu/5028736#file-multichannel-play-record-jack-md)

- **Christopher Brown**

- [**Andrzej Ciarkowski**](https://github.com/andrzejc)

## License

This project is licensed under the GPLv3 - see the [LICENSE.md](LICENSE.md) file for details.
