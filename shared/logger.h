#pragma once

#include <syslog.h>
#include <string>
#include <iostream>
#include <format>
#include <source_location>
#include <cstdlib>

namespace fbx::shared {

enum class LogLevel {
    DEBUG = LOG_DEBUG,
    INFO = LOG_INFO,
    WARN = LOG_WARNING,
    ERROR = LOG_ERR
};

enum class LogMode {
    CONSOLE,
    SYSLOG,
    BOTH
};

class Logger {
public:
    static void init(const std::string& ident) noexcept {
        ident_ = ident;
        
        // Read configuration from environment
        const char* mode_env = std::getenv("FREEBSDX_LOG_MODE");
        if (mode_env) {
            std::string mode(mode_env);
            if (mode == "syslog") mode_ = LogMode::SYSLOG;
            else if (mode == "both") mode_ = LogMode::BOTH;
            else mode_ = LogMode::CONSOLE;
        } else {
            mode_ = LogMode::CONSOLE; // Default to console for iterative development
        }

        applyMode();
        initialized_ = true;
    }

    static void setMode(LogMode mode) noexcept {
        mode_ = mode;
        applyMode();
    }

private:
    static void applyMode() noexcept {
        if ((mode_ == LogMode::SYSLOG || mode_ == LogMode::BOTH) && !syslog_initialized_) {
            ::openlog(ident_.c_str(), LOG_PID | LOG_NDELAY, LOG_DAEMON);
            syslog_initialized_ = true;
        } else if (mode_ == LogMode::CONSOLE && syslog_initialized_) {
            ::closelog();
            syslog_initialized_ = false;
        }
    }

public:
    static void cleanup() noexcept {
        if (syslog_initialized_) {
            ::closelog();
            syslog_initialized_ = false;
        }
        initialized_ = false;
    }

    template<typename... Args>
    static void log(LogLevel level, std::format_string<Args...> fmt, Args&&... args) noexcept {
        if (!initialized_) return;

        try {
            std::string message = std::format(fmt, std::forward<Args>(args)...);
            
            if (mode_ == LogMode::SYSLOG || mode_ == LogMode::BOTH) {
                ::syslog(static_cast<int>(level), "%s", message.c_str());
            }

            if (mode_ == LogMode::CONSOLE || mode_ == LogMode::BOTH) {
                const char* color = "\033[0m"; // Reset
                const char* level_str = "INFO";
                
                if (level == LogLevel::DEBUG) { color = "\033[36m"; level_str = "DEBUG"; } // Cyan
                else if (level == LogLevel::WARN) { color = "\033[33m"; level_str = "WARN"; } // Yellow
                else if (level == LogLevel::ERROR) { color = "\033[31m"; level_str = "ERROR"; } // Red
                else { color = "\033[32m"; } // Green for INFO

                auto& stream = (level == LogLevel::ERROR || level == LogLevel::WARN) ? std::cerr : std::cout;
                stream << color << "[" << ident_ << "] [" << level_str << "] " << message << "\033[0m" << std::endl;
            }
        } catch (...) {
            // Never throw from the logger
        }
    }

private:
    static inline bool initialized_ = false;
    static inline bool syslog_initialized_ = false;
    static inline std::string ident_ = "freebsd-x";
    static inline LogMode mode_ = LogMode::CONSOLE;
};

// Convenience Macros
#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARN
#undef LOG_ERROR

#define LOG_DEBUG(...) fbx::shared::Logger::log(fbx::shared::LogLevel::DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  fbx::shared::Logger::log(fbx::shared::LogLevel::INFO,  __VA_ARGS__)
#define LOG_WARN(...)  fbx::shared::Logger::log(fbx::shared::LogLevel::WARN,  __VA_ARGS__)
#define LOG_ERROR(...) fbx::shared::Logger::log(fbx::shared::LogLevel::ERROR, __VA_ARGS__)

// XASSERT: compositor invariant check for programmer errors (not recoverable system failures).
// Logs the failure at ERROR level then aborts. Use only for conditions that indicate a code bug,
// not for expected runtime failures like Vulkan OOM (those should graceful-degrade instead).
#define XASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            LOG_ERROR("XASSERT [" __FILE__ ":{}]: " #cond " — " msg, __LINE__); \
            std::abort(); \
        } \
    } while (0)

} // namespace fbx::shared
