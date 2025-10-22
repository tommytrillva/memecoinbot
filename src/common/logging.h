#pragma once

#include <chrono>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>

namespace common {

enum class LogLevel {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
};

class Logger {
public:
    static Logger& instance();

    void setMinimumLevel(LogLevel level);
    LogLevel minimumLevel() const;

    void log(LogLevel level, const std::string& message);

private:
    Logger() = default;

    static const char* levelTag(LogLevel level);
    static std::string timestamp();

    mutable std::mutex mutex_;
    LogLevel minimumLevel_{LogLevel::Info};
};

}  // namespace common

#define LOG_TRACE(message) ::common::Logger::instance().log(::common::LogLevel::Trace, (message))
#define LOG_DEBUG(message) ::common::Logger::instance().log(::common::LogLevel::Debug, (message))
#define LOG_INFO(message) ::common::Logger::instance().log(::common::LogLevel::Info, (message))
#define LOG_WARN(message) ::common::Logger::instance().log(::common::LogLevel::Warn, (message))
#define LOG_ERROR(message) ::common::Logger::instance().log(::common::LogLevel::Error, (message))
