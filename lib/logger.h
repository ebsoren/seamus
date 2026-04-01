#pragma once

#include <cstdarg>
#include <cstdio>
#include <mutex>

#include "consts.h"

enum class LogLevel : uint8_t {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    NONE = 4,
};

namespace logger {

inline std::mutex log_mtx;

inline bool enabled(LogLevel level) {
    return static_cast<uint8_t>(level) >= LOG_LEVEL;
}

inline void log(LogLevel level, const char *fmt, ...) {
    if (!enabled(level)) return;

    const char *prefix;
    switch (level) {
    case LogLevel::DEBUG:
        prefix = "[DEBUG] ";
        break;
    case LogLevel::INFO:
        prefix = "[INFO]  ";
        break;
    case LogLevel::WARN:
        prefix = "[WARN]  ";
        break;
    case LogLevel::ERROR:
        prefix = "[ERROR] ";
        break;
    default:
        return;
    }

    std::lock_guard<std::mutex> lock(log_mtx);
    fputs(prefix, stderr);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

inline void debug(const char *fmt, ...) {
    if (!enabled(LogLevel::DEBUG)) return;
    std::lock_guard<std::mutex> lock(log_mtx);
    fputs("[DEBUG] ", stderr);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

inline void info(const char *fmt, ...) {
    if (!enabled(LogLevel::INFO)) return;
    std::lock_guard<std::mutex> lock(log_mtx);
    fputs("[INFO]  ", stderr);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

inline void warn(const char *fmt, ...) {
    if (!enabled(LogLevel::WARN)) return;
    std::lock_guard<std::mutex> lock(log_mtx);
    fputs("[WARN]  ", stderr);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

inline void error(const char *fmt, ...) {
    if (!enabled(LogLevel::ERROR)) return;
    std::lock_guard<std::mutex> lock(log_mtx);
    fputs("[ERROR] ", stderr);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

}   // namespace logger
