#pragma once

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Simple_cartesian.h>
#include <boost/optional.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <cmath>
#include <type_traits>

namespace agp {
namespace geometry {

/**
 * @brief 2D Point class with exact arithmetic using CGAL
 *
 * This class wraps CGAL's exact computation kernel to provide robust
 * geometric operations without floating-point errors.
 *
 * @tparam Kernel CGAL kernel type (default: Exact_predicates_exact_constructions_kernel)
 */
template <typename Kernel = CGAL::Exact_predicates_exact_constructions_kernel>
class Point {
public:
    using FT = typename Kernel::FT;
    using Point_2 = typename Kernel::Point_2;
    using Vector_2 = typename Kernel::Vector_2;

    // Constructors
    Point() : point_(FT(0), FT(0)) {}

    Point(FT x, FT y) : point_(x, y) {}

    Point(const Point_2& p) : point_(p) {}

    // Constructor from doubles (only when FT is not double)
    template<typename T = FT, typename = std::enable_if_t<!std::is_same_v<T, double>>>
    Point(double x, double y) : point_(FT(x), FT(y)) {}

    // Copy and move
    Point(const Point& other) = default;
    Point(Point&& other) = default;
    Point& operator=(const Point& other) = default;
    Point& operator=(Point&& other) = default;

    // Accessors
    FT x() const { return point_.x(); }
    FT y() const { return point_.y(); }
    const Point_2& cgal_point() const { return point_; }

    // Arithmetic operations
    Point operator+(const Point& other) const {
        return Point(point_.x() + other.point_.x(), point_.y() + other.point_.y());
    }

    Point operator-(const Point& other) const {
        return Point(point_.x() - other.point_.x(), point_.y() - other.point_.y());
    }

    Point operator*(const FT& scalar) const {
        return Point(point_.x() * scalar, point_.y() * scalar);
    }

    // Comparison operators
    bool operator==(const Point& other) const {
        return point_ == other.point_;
    }

    bool operator!=(const Point& other) const {
        return !(*this == other);
    }

    bool operator<(const Point& other) const {
        return point_ < other.point_;
    }

    bool operator<=(const Point& other) const {
        return point_ <= other.point_;
    }

    bool operator>(const Point& other) const {
        return point_ > other.point_;
    }

    bool operator>=(const Point& other) const {
        return point_ >= other.point_;
    }

    // Distance and geometric queries
    FT squared_distance(const Point& other) const {
        return CGAL::squared_distance(point_, other.point_);
    }

    double distance(const Point& other) const {
        return std::sqrt(CGAL::to_double(squared_distance(other)));
    }

    // Conversion to double (for output/visualization)
    double x_double() const {
        return CGAL::to_double(point_.x());
    }

    double y_double() const {
        return CGAL::to_double(point_.y());
    }

    // I/O operations
    std::string to_string() const {
        std::ostringstream oss;
        oss << "(" << CGAL::to_double(point_.x()) << ", "
            << CGAL::to_double(point_.y()) << ")";
        return oss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const Point& p) {
        os << p.to_string();
        return os;
    }

    // JSON serialization
    nlohmann::json to_json() const {
        return {
            {"x", CGAL::to_double(point_.x())},
            {"y", CGAL::to_double(point_.y())}
        };
    }

    static Point from_json(const nlohmann::json& j) {
        return Point(j["x"].get<double>(), j["y"].get<double>());
    }

    // Batch read from JSON file
    template<typename OutputIterator>
    static OutputIterator read_json_file(const std::string& filename, OutputIterator out) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + filename);
        }

        nlohmann::json j;
        file >> j;

        if (j.is_array()) {
            for (const auto& item : j) {
                *out++ = from_json(item);
            }
        } else if (j.contains("vertices")) {
            for (const auto& item : j["vertices"]) {
                *out++ = from_json(item);
            }
        }

        return out;
    }

    // Hash function for use in containers
    struct Hash {
        size_t operator()(const Point& p) const {
            // Use approximate double values for hashing
            size_t h1 = std::hash<double>{}(p.x_double());
            size_t h2 = std::hash<double>{}(p.y_double());
            return h1 ^ (h2 << 1);
        }
    };

private:
    Point_2 point_;
};

// Common type aliases
using PointE = Point<CGAL::Exact_predicates_exact_constructions_kernel>;
using PointD = Point<CGAL::Simple_cartesian<double>>;

} // namespace geometry
} // namespace agp
