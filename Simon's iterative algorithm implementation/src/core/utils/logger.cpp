/**
 * @file logger.cpp
 * @brief Logger implementation
 */

#include "core/utils/logger.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace agp {

Logger::Logger() : level_(LogLevel::INFO), output_to_file_(false) {
}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::set_level(LogLevel level) {
    level_ = level;
}

void Logger::set_log_file(const std::string& filename) {
    if (log_file_.is_open()) {
        log_file_.close();
    }

    log_file_.open(filename, std::ios::out | std::ios::app);
    output_to_file_ = log_file_.is_open();
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level > level_) {
        return;
    }

    std::string level_str;
    switch (level) {
        case LogLevel::DEBUG:
            level_str = "DEBUG";
            break;
        case LogLevel::INFO:
            level_str = "INFO";
            break;
        case LogLevel::WARNING:
            level_str = "WARNING";
            break;
        case LogLevel::ERROR:
            level_str = "ERROR";
            break;
    }

    // Get timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");

    std::string log_line = "[" + ss.str() + "] [" + level_str + "] " + message;

    // Output to console
    if (level >= LogLevel::WARNING) {
        std::cerr << log_line << std::endl;
    } else {
        std::cout << log_line << std::endl;
    }

    // Output to file
    if (output_to_file_) {
        log_file_ << log_line << std::endl;
    }
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

} // namespace agp
