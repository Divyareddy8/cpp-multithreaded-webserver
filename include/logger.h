#pragma once
#include <string>
#include <mutex>
#include <fstream>

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Logger {
public:
    static Logger& instance();
    void log(LogLevel level, const std::string& msg);
    void set_level(LogLevel level);

    void info(const std::string& msg)  { log(LogLevel::INFO,  msg); }
    void warn(const std::string& msg)  { log(LogLevel::WARN,  msg); }
    void error(const std::string& msg) { log(LogLevel::ERROR, msg); }
    void debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }

private:
    Logger() = default;
    std::mutex mutex_;
    LogLevel min_level_ = LogLevel::INFO;
    std::string level_str(LogLevel l);
    std::string timestamp();
};

#define LOG_INFO(msg)  Logger::instance().info(msg)
#define LOG_WARN(msg)  Logger::instance().warn(msg)
#define LOG_ERROR(msg) Logger::instance().error(msg)
#define LOG_DEBUG(msg) Logger::instance().debug(msg)
