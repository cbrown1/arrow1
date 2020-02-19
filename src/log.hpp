#pragma once
namespace olo {

// Note: use of boost-logging seems like an overkill, this should be simplistic

enum LogLevel {
    LDEBUG,
    LINFO,
    // Note: plain ERROR causes name clashes on Windows
    LERROR
};

[[gnu::format(printf, 2, 3)]]
void log(LogLevel level, const char* format, ...);

#define ldebug(...) ::olo::log(::olo::LDEBUG, __VA_ARGS__)
#define linfo(...) ::olo::log(::olo::LINFO, __VA_ARGS__)
#define lerror(...) ::olo::log(::olo::LERROR, __VA_ARGS__)

void set_loglevel(LogLevel l);
}