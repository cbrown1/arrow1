#pragma once
#include <jack/jack.h>
#include <cstddef>
#include <string>
#include <vector>

namespace olo {

using std::string;
using std::size_t;
using std::vector;
using Sample = jack_default_audio_sample_t;

const size_t BUFFER_SIZE_DEFAULT = 65536 / 8;
const string JACK_CLIENT_NAME = "olinout";
const string VERSION = "2.0";

class Reader;
class Writer;
class JackClient;

}
