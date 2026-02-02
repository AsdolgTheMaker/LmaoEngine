#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <source_location>

namespace lmao {

enum class LogLevel : uint8_t { Trace, Debug, Info, Warn, Error, Fatal, COUNT };

enum class LogCategory : uint8_t {
    Core,       // Engine lifecycle, main loop
    Vulkan,     // Vulkan context, device, validation
    Swapchain,  // Swapchain creation, recreation, present
    Memory,     // VMA, buffer/image allocation
    Pipeline,   // Pipeline, shader, descriptor creation
    Render,     // Render passes, draw commands, frame orchestration
    Scene,      // Scene graph, camera, entities
    Assets,     // Model/texture loading, asset management
    Input,      // Keyboard, mouse, gamepad
    Gui,        // ImGui, debug UI
    COUNT
};

namespace detail {

inline const char* levelStr(LogLevel l) {
    constexpr const char* names[] = {"TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL"};
    return names[static_cast<int>(l)];
}

inline const char* categoryStr(LogCategory c) {
    constexpr const char* names[] = {
        "Core", "Vulkan", "Swap", "Mem", "Pipe", "Render", "Scene", "Assets", "Input", "Gui"
    };
    return names[static_cast<int>(c)];
}

// Per-category minimum log level. Indexed by LogCategory.
inline LogLevel g_categoryLevels[static_cast<int>(LogCategory::COUNT)] = {
    LogLevel::Debug, // Core
    LogLevel::Debug, // Vulkan
    LogLevel::Debug, // Swapchain
    LogLevel::Info,  // Memory (quiet by default)
    LogLevel::Debug, // Pipeline
    LogLevel::Debug, // Render
    LogLevel::Debug, // Scene
    LogLevel::Debug, // Assets
    LogLevel::Info,  // Input (quiet by default)
    LogLevel::Debug, // Gui
};

// Global minimum level (overrides per-category if higher)
inline LogLevel g_globalMinLevel = LogLevel::Debug;

// Category enable mask (bit per category, all enabled by default)
inline uint32_t g_categoryMask = 0xFFFFFFFF;

} // namespace detail

// Configure the log system
inline void logSetGlobalLevel(LogLevel level) { detail::g_globalMinLevel = level; }
inline void logSetCategoryLevel(LogCategory cat, LogLevel level) {
    detail::g_categoryLevels[static_cast<int>(cat)] = level;
}
inline void logEnableCategory(LogCategory cat, bool enable) {
    uint32_t bit = 1u << static_cast<int>(cat);
    if (enable) detail::g_categoryMask |= bit;
    else detail::g_categoryMask &= ~bit;
}

// Core logging function
template <typename... Args>
void log(LogLevel level, LogCategory cat, std::source_location loc, const char* fmt, Args&&... args) {
    // Check global minimum
    if (level < detail::g_globalMinLevel) return;
    // Check category mask
    if (!(detail::g_categoryMask & (1u << static_cast<int>(cat)))) return;
    // Check per-category minimum
    if (level < detail::g_categoryLevels[static_cast<int>(cat)]) return;

    // Format: [LEVEL] [Category] filename:line: message
    // Extract just the filename from the full path
    const char* file = loc.file_name();
    const char* lastSep = file;
    for (const char* p = file; *p; ++p) {
        if (*p == '/' || *p == '\\') lastSep = p + 1;
    }

    std::fprintf(stderr, "[%s] [%-6s] %s:%d: ",
        detail::levelStr(level), detail::categoryStr(cat),
        lastSep, loc.line());
    if constexpr (sizeof...(args) > 0) {
        std::fprintf(stderr, fmt, args...);
    } else {
        std::fputs(fmt, stderr);
    }
    std::fprintf(stderr, "\n");

    if (level == LogLevel::Fatal) std::abort();
}

// Primary logging macro: LOG(Category, Level, fmt, ...)
// Usage: LOG(Vulkan, Info, "Device: %s", name);
//        LOG(Core, Error, "Init failed: %d", err);
#define LOG(cat, level, fmt, ...) \
    ::lmao::log(::lmao::LogLevel::level, ::lmao::LogCategory::cat, \
                std::source_location::current(), fmt __VA_OPT__(,) __VA_ARGS__)

} // namespace lmao
