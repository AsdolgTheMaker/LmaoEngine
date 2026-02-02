#pragma once
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace lmao {

enum class LogLevel : uint8_t { Trace, Debug, Info, Warn, Error, Fatal };

inline LogLevel g_minLogLevel = LogLevel::Debug;

namespace detail {
inline const char* levelStr(LogLevel l) {
    constexpr const char* names[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    return names[static_cast<int>(l)];
}
} // namespace detail

template <typename... Args>
void log(LogLevel level, std::source_location loc, const char* fmt, Args&&... args) {
    if (level < g_minLogLevel) return;
    std::fprintf(stderr, "[%s] %s:%d: ", detail::levelStr(level), loc.file_name(), loc.line());
    std::fprintf(stderr, fmt, args...);
    std::fprintf(stderr, "\n");
    if (level == LogLevel::Fatal) std::abort();
}

#define LMAO_LOG(level, fmt, ...) \
    ::lmao::log(level, std::source_location::current(), fmt __VA_OPT__(,) __VA_ARGS__)

#define LMAO_TRACE(fmt, ...) LMAO_LOG(::lmao::LogLevel::Trace, fmt __VA_OPT__(,) __VA_ARGS__)
#define LMAO_DEBUG(fmt, ...) LMAO_LOG(::lmao::LogLevel::Debug, fmt __VA_OPT__(,) __VA_ARGS__)
#define LMAO_INFO(fmt, ...)  LMAO_LOG(::lmao::LogLevel::Info,  fmt __VA_OPT__(,) __VA_ARGS__)
#define LMAO_WARN(fmt, ...)  LMAO_LOG(::lmao::LogLevel::Warn,  fmt __VA_OPT__(,) __VA_ARGS__)
#define LMAO_ERROR(fmt, ...) LMAO_LOG(::lmao::LogLevel::Error, fmt __VA_OPT__(,) __VA_ARGS__)
#define LMAO_FATAL(fmt, ...) LMAO_LOG(::lmao::LogLevel::Fatal, fmt __VA_OPT__(,) __VA_ARGS__)

} // namespace lmao
