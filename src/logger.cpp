#include "logger.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::set_level(LogLevel level) {
    min_level_ = level;
}

void Logger::log(LogLevel level, const std::string& msg) {
    if (level < min_level_) return;
    std::lock_guard<std::mutex> lock(mutex_);

    std::string line = "[" + timestamp() + "] [" + level_str(level) + "] " + msg;

    if (level >= LogLevel::ERROR) {
        std::cerr << line << "\n";
    } else {
        std::cout << line << "\n";
    }
}

std::string Logger::level_str(LogLevel l) {
    switch (l) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return " INFO";
        case LogLevel::WARN:  return " WARN";
        case LogLevel::ERROR: return "ERROR";
    }
    return "?????";
}

std::string Logger::timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    localtime_r(&t, &tm_buf);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}
