#pragma once

#include "point.hpp"
#include <CGAL/Segment_2.h>
#include <CGAL/intersections.h>
#include <boost/optional.hpp>

namespace agp {
namespace geometry {

/**
 * @brief 2D Line Segment class
 *
 * Represents a line segment between two points with exact arithmetic.
 *
 * @tparam Kernel CGAL kernel type
 */
template <typename Kernel = CGAL::Exact_predicates_exact_constructions_kernel>
class Segment {
public:
    using Point = Point<Kernel>;
    using FT = typename Kernel::FT;
    using Segment_2 = typename Kernel::Segment_2;
    using Line_2 = typename Kernel::Line_2;

    // Constructors
    Segment() = default;

    Segment(const Point& source, const Point& target)
        : segment_(source.cgal_point(), target.cgal_point()),
          source_(source), target_(target) {}

    Segment(const Segment_2& seg)
        : segment_(seg),
          source_(seg.source()),
          target_(seg.target()) {}

    // Accessors
    const Point& source() const { return source_; }
    const Point& target() const { return target_; }
    const Segment_2& cgal_segment() const { return segment_; }

    FT squared_length() const {
        return segment_.squared_length();
    }

    double length() const {
        return std::sqrt(CGAL::to_double(squared_length()));
    }

    // Midpoint
    Point midpoint() const {
        return Point(CGAL::midpoint(segment_.source(), segment_.target()));
    }

    // Check if point is on segment
    bool has_on(const Point& p) const {
        return segment_.has_on(p.cgal_point());
    }

    bool has_on_boundary(const Point& p) const {
        return segment_.has_on(p.cgal_point());
    }

    // Check if point is strictly inside (not endpoints)
    bool has_on_interior(const Point& p) const {
        return segment_.has_on(p.cgal_point()) &&
               p != source_ && p != target_;
    }

    // Direction vector
    Point direction() const {
        return target_ - source_;
    }

    // Opposite segment
    Segment opposite() const {
        return Segment(target_, source_);
    }

    // Bounding box
    FT xmin() const { return std::min(source_.x(), target_.x()); }
    FT xmax() const { return std::max(source_.x(), target_.x()); }
    FT ymin() const { return std::min(source_.y(), target_.y()); }
    FT ymax() const { return std::max(source_.y(), target_.y()); }

    // Intersection tests
    bool intersects(const Segment& other) const {
        return CGAL::do_intersect(segment_, other.segment_);
    }

    boost::optional<Point> intersection(const Segment& other) const {
        auto result = CGAL::intersection(segment_, other.segment_);
        if (result) {
            if (const typename Kernel::Point_2* p = boost::get<typename Kernel::Point_2>(&*result)) {
                return Point(*p);
            }
            // If intersection is a segment, return one endpoint
            if (const typename Kernel::Segment_2* s = boost::get<typename Kernel::Segment_2>(&*result)) {
                return Point(s->source());
            }
        }
        return boost::none;
    }

    // Line containing this segment
    Line_2 supporting_line() const {
        return segment_.supporting_line();
    }

    // Comparison operators
    bool operator==(const Segment& other) const {
        return segment_ == other.segment_;
    }

    bool operator!=(const Segment& other) const {
        return !(*this == other);
    }

    // I/O
    std::string to_string() const {
        return source_.to_string() + " -> " + target_.to_string();
    }

    friend std::ostream& operator<<(std::ostream& os, const Segment& s) {
        os << s.to_string();
        return os;
    }

    // JSON serialization
    nlohmann::json to_json() const {
        return {
            {"source", source_.to_json()},
            {"target", target_.to_json()}
        };
    }

    static Segment from_json(const nlohmann::json& j) {
        return Segment(Point::from_json(j["source"]),
                       Point::from_json(j["target"]));
    }

private:
    Segment_2 segment_;
    Point source_;
    Point target_;
};

// Common type aliases
using SegmentE = Segment<CGAL::Exact_predicates_exact_constructions_kernel>;
using SegmentD = Segment<CGAL::Simple_cartesian<double>>;

} // namespace geometry
} // namespace agp
