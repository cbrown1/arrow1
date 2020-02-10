#include "log.hpp"

#include <cstdarg>
#include <cstdio>

namespace olo {

namespace {
LogLevel log_level = INFO;
}

void set_loglevel(LogLevel l) {
    log_level = l;
}

void log(LogLevel level, const char* format, ...) {
    using namespace std;
    if (level < log_level) {
        return;
    }
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}
}
