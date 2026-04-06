/**
 * @file logger.hpp
 * @brief Logger utility for the Art Gallery Problem solver
 */

#pragma once

#include <string>
#include <fstream>
#include <mutex>

namespace agp {

/**
 * @brief Log level enumeration
 */
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

/**
 * @brief Simple logger class
 *
 * Thread-safe logging to console and optional file.
 */
class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level);
    void set_log_file(const std::string& filename);

    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

private:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(LogLevel level, const std::string& message);

    LogLevel level_;
    std::ofstream log_file_;
    bool output_to_file_;
    std::mutex mutex_;
};

// Convenience macros
#define LOG_DEBUG(msg) agp::Logger::instance().debug(msg)
#define LOG_INFO(msg) agp::Logger::instance().info(msg)
#define LOG_WARNING(msg) agp::Logger::instance().warning(msg)
#define LOG_ERROR(msg) agp::Logger::instance().error(msg)

} // namespace agp
