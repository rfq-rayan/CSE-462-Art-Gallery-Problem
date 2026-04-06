#pragma once

#include "point.hpp"
#include "segment.hpp"
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/bounding_box.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <set>
#include <algorithm>
#include <cmath>

namespace agp {
namespace geometry {

/**
 * @brief Simple Polygon class with reflex vertex detection
 *
 * Represents a simple polygon (no self-intersections) with support
 * for detecting reflex vertices and basic geometric queries.
 *
 * @tparam Kernel CGAL kernel type
 */
template <typename Kernel = CGAL::Exact_predicates_exact_constructions_kernel>
class Polygon {
public:
    using Point = Point<Kernel>;
    using Segment = Segment<Kernel>;
    using FT = typename Kernel::FT;
    using Polygon_2 = CGAL::Polygon_2<Kernel>;
    using Polygon_with_holes_2 = CGAL::Polygon_with_holes_2<Kernel>;
    using Iso_rectangle_2 = typename Kernel::Iso_rectangle_2;

    // Constructors
    Polygon() = default;

    explicit Polygon(const std::vector<Point>& vertices)
        : vertices_(vertices) {
        compute_reflex_vertices();
    }

    explicit Polygon(const Polygon_2& poly)
        : vertices_(poly.vertices_begin(), poly.vertices_end()) {
        compute_reflex_vertices();
    }

    // From iterator range
    template<typename InputIterator>
    Polygon(InputIterator first, InputIterator last)
        : vertices_(first, last) {
        compute_reflex_vertices();
    }

    // Accessors
    size_t num_vertices() const { return vertices_.size(); }
    size_t num_reflex_vertices() const { return reflex_indices_.size(); }
    const std::vector<Point>& vertices() const { return vertices_; }
    const std::vector<size_t>& reflex_indices() const { return reflex_indices_; }

    // Get reflex vertex positions
    std::vector<Point> reflex_vertex_positions() const {
        std::vector<Point> positions;
        positions.reserve(reflex_indices_.size());
        for (size_t idx : reflex_indices_) {
            positions.push_back(vertices_[idx]);
        }
        return positions;
    }

    // Individual vertex access
    const Point& vertex(size_t i) const {
        return vertices_.at(i);
    }

    const Point& operator[](size_t i) const {
        return vertices_[i];
    }

    // Edges
    Segment edge(size_t i) const {
        return Segment(vertices_[i], vertices_[(i + 1) % num_vertices()]);
    }

    std::vector<Segment> edges() const {
        std::vector<Segment> result;
        result.reserve(num_vertices());
        for (size_t i = 0; i < num_vertices(); ++i) {
            result.push_back(edge(i));
        }
        return result;
    }

    // Check if vertex is reflex
    bool is_reflex(size_t vertex_index) const {
        return std::find(reflex_indices_.begin(), reflex_indices_.end(),
                        vertex_index) != reflex_indices_.end();
    }

    // Check if polygon is simple (no self-intersections)
    bool is_simple() const {
        return cgal_polygon().is_simple();
    }

    // Check if polygon is convex
    bool is_convex() const {
        return cgal_polygon().is_convex();
    }

    // Orientation (COUNTERCLOCKWISE or CLOCKWISE)
    CGAL::Orientation orientation() const {
        return cgal_polygon().orientation();
    }

    // Area (positive if counterclockwise, negative if clockwise)
    FT area() const {
        return cgal_polygon().area();
    }

    // Bounding box
    Iso_rectangle_2 bbox() const {
        return CGAL::bounding_box(
            reinterpret_cast<const std::vector<typename Kernel::Point_2>&>(vertices_).begin(),
            reinterpret_cast<const std::vector<typename Kernel::Point_2>&>(vertices_).end()
        );
    }

    // Point containment tests
    bool contains(const Point& p) const {
        auto bounded = cgal_polygon().bounded_side(p.cgal_point());
        return bounded == CGAL::ON_BOUNDED_SIDE ||
               bounded == CGAL::ON_BOUNDARY;
    }

    bool contains_strictly(const Point& p) const {
        return cgal_polygon().bounded_side(p.cgal_point()) == CGAL::ON_BOUNDED_SIDE;
    }

    bool is_on_boundary(const Point& p) const {
        return cgal_polygon().bounded_side(p.cgal_point()) == CGAL::ON_BOUNDARY;
    }

    // CGAL conversion
    Polygon_2 cgal_polygon() const {
        Polygon_2 poly;
        for (const auto& v : vertices_) {
            poly.push_back(v.cgal_point());
        }
        return poly;
    }

    // Reorient to counterclockwise if needed
    void reorient_ccw() {
        if (orientation() == CGAL::CLOCKWISE) {
            std::reverse(vertices_.begin(), vertices_.end());
            compute_reflex_vertices();
        }
    }

    // I/O operations
    static Polygon from_json_file(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + filename);
        }

        nlohmann::json j;
        file >> j;
        return from_json(j);
    }

    static Polygon from_json(const nlohmann::json& j) {
        std::vector<Point> vertices;

        if (j.is_array()) {
            for (const auto& item : j) {
                vertices.push_back(Point::from_json(item));
            }
        } else if (j.contains("vertices")) {
            for (const auto& item : j["vertices"]) {
                vertices.push_back(Point::from_json(item));
            }
        } else if (j.contains("coordinates")) {
            // GeoJSON format
            for (const auto& coord : j["coordinates"]) {
                if (coord.is_array() && coord.size() >= 2) {
                    vertices.push_back(Point(coord[0].get<double>(),
                                            coord[1].get<double>()));
                }
            }
        }

        return Polygon(vertices);
    }

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["vertices"] = nlohmann::json::array();
        for (const auto& v : vertices_) {
            j["vertices"].push_back(v.to_json());
        }
        j["num_reflex"] = reflex_indices_.size();
        return j;
    }

    void to_json_file(const std::string& filename) const {
        std::ofstream file(filename);
        file << to_json().dump(2);
    }

    // Output
    friend std::ostream& operator<<(std::ostream& os, const Polygon& poly) {
        os << "Polygon[" << poly.num_vertices() << " vertices, "
           << poly.num_reflex_vertices() << " reflex]";
        return os;
    }

private:
    std::vector<Point> vertices_;
    std::vector<size_t> reflex_indices_;

    // Compute reflex vertices using cross product test
    void compute_reflex_vertices() {
        reflex_indices_.clear();
        if (vertices_.size() < 3) return;

        // Ensure counterclockwise orientation for reflex detection
        bool reversed = false;
        if (orientation() == CGAL::CLOCKWISE) {
            std::reverse(vertices_.begin(), vertices_.end());
            reversed = true;
        }

        size_t n = vertices_.size();
        for (size_t i = 0; i < n; ++i) {
            size_t prev = (i + n - 1) % n;
            size_t next = (i + 1) % n;

            // Compute cross product of edges (prev->i) x (i->next)
            // If positive, vertex i is reflex in a CCW polygon
            FT dx1 = vertices_[i].x() - vertices_[prev].x();
            FT dy1 = vertices_[i].y() - vertices_[prev].y();
            FT dx2 = vertices_[next].x() - vertices_[i].x();
            FT dy2 = vertices_[next].y() - vertices_[i].y();

            FT cross = dx1 * dy2 - dy1 * dx2;

            if (cross > FT(0)) {
                reflex_indices_.push_back(i);
            }
        }

        // Restore original orientation if we reversed
        if (reversed) {
            std::reverse(vertices_.begin(), vertices_.end());
            // Update reflex indices for the new vertex positions
            for (auto& idx : reflex_indices_) {
                idx = n - 1 - idx;
            }
            std::sort(reflex_indices_.begin(), reflex_indices_.end());
        }
    }
};

// Common type aliases
using PolygonE = Polygon<CGAL::Exact_predicates_exact_constructions_kernel>;
using PolygonD = Polygon<CGAL::Simple_cartesian<double>>;

} // namespace geometry
} // namespace agp
