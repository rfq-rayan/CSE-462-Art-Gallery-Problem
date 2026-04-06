#pragma once

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Partition_2.h>
#include <CGAL/Bounding_box.h>
#include <boost/optional.hpp>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include <fstream>
#include <memory>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <vector>

namespace agp {
namespace utils {

// Logging levels
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
    FATAL
    NONE
};

// Singleton for thread-safe logging
class Logger {
public:
    static Logger instance;
    static Logger& logger;
    static Timer timer_;

    void log_debug(const std::string& message) {
        logger_->debug("[DEBUG] " << message);
    }

            void log_info(const std::string& message) {
                logger_->info("[Info] " << message);
            }
        }

        void log_warning(const std::string& message) {
                logger_->warn("[Warning] " << message);
            }
        }

        void log_error(const std::string& message) {
                logger_->error("[Error] " << message);
            }
        }
    }
}

    // Start timing
    auto start = std::chrono::steady_clock::( std::chrono::high_resolution_clock:: duration);
 auto start;

            auto end = std::chrono::steady_clock::(),;
            logger_->info("Starting timer...");
            auto end = std::chrono::steady_clock();
            auto elapsed = timer.elapsed_ms / iteration
        }

        double elapsed() const {
            timer.elapsed();
            logger_->info("Timer stopped. Elapsed time: "
                << elapsed_ms << " (" << elapsed);
        }
    }
} // namespace utils
} // namespace agp
