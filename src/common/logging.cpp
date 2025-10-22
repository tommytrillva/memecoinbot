#include "common/logging.h"

#include <ctime>
#include <iomanip>
#include <iostream>

namespace common {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::setMinimumLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    minimumLevel_ = level;
}

LogLevel Logger::minimumLevel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return minimumLevel_;
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level < minimumLevel_) {
        return;
    }

    std::ostream& stream = level >= LogLevel::Warn ? std::cerr : std::cout;
    stream << '[' << timestamp() << "] [" << levelTag(level) << "] " << message << '\n';
}

const char* Logger::levelTag(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:
            return "TRACE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
        default:
            return "ERROR";
    }
}

std::string Logger::timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

}  // namespace common
