#pragma once
namespace olo {

// Note: use of boost-logging seems like an overkill, this should be simplistic

enum LogLevel {
    DEBUG,
    INFO,
    ERROR
};

[[gnu::format(printf, 2, 3)]]
void log(LogLevel level, const char* format, ...);

#define ldebug(...) ::olo::log(::olo::DEBUG, __VA_ARGS__)
#define linfo(...) ::olo::log(::olo::INFO, __VA_ARGS__)
#define lerror(...) ::olo::log(::olo::ERROR, __VA_ARGS__)

void set_loglevel(LogLevel l);
}