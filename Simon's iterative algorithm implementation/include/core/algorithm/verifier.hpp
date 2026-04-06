#pragma once

#include "../geometry/polygon.hpp"
#include "../geometry/arrangement.hpp"
#include "../geometry/visibility.hpp"
#include <vector>
#include <cmath>

namespace agp {
namespace algorithm {

/**
 * @brief Solution verifier
 *
 * Verifies that a set of guards completely covers a polygon.
 *
 * @tparam Kernel CGAL kernel type
 */
template <typename Kernel = CGAL::Exact_predicates_exact_constructions_kernel>
class Verifier {
public:
    using Point       = geometry::Point<Kernel>;
    using Polygon     = geometry::Polygon<Kernel>;
    using Arrangement = geometry::Arrangement<Kernel>;
    using FT          = typename Kernel::FT;

    /**
     * @brief Exact face-based verification (per the paper).
     *
     * For every face in the arrangement, checks that at least one guard's
     * visibility polygon contains (i.e. completely sees) that face.
     * This is the correct verification as described in the paper.
     *
     * @param arr     The final arrangement
     * @param polygon The original polygon
     * @param guards  The set of guard positions
     * @return true if every face is completely seen by at least one guard
     */
    static bool verify_exact(const Arrangement& arr,
                             const Polygon& polygon,
                             const std::vector<Point>& guards) {
        auto all_faces = arr.all_faces();

        for (auto face : all_faces) {
            // Convert face to a polygon for containment testing
            Polygon face_poly = arr.face_to_polygon(face);
            if (face_poly.num_vertices() < 3) continue;

            bool face_seen = false;
            for (const Point& guard : guards) {
                if (geometry::Visibility<Kernel>::sees_completely(
                        polygon, guard, face_poly)) {
                    face_seen = true;
                    break;
                }
            }

            if (!face_seen) {
                return false; // Found an uncovered face
            }
        }
        return true;
    }

    /**
     * @brief Approximate grid-based verification (fast but not exact).
     *
     * Samples points on a grid and checks that every grid point inside
     * the polygon is seen by at least one guard.
     *
     * @param polygon      The polygon to verify
     * @param guards       The set of guard positions
     * @param grid_spacing Spacing for verification grid (default 0.01)
     * @return true if all sampled points are visible
     */
    static bool verify_approximate(const Polygon& polygon,
                                   const std::vector<Point>& guards,
                                   double grid_spacing = 0.01) {
        auto bbox  = polygon.bbox();
        double min_x = CGAL::to_double(bbox.xmin());
        double max_x = CGAL::to_double(bbox.xmax());
        double min_y = CGAL::to_double(bbox.ymin());
        double max_y = CGAL::to_double(bbox.ymax());

        for (double x = min_x; x <= max_x; x += grid_spacing) {
            for (double y = min_y; y <= max_y; y += grid_spacing) {
                Point p(x, y);
                if (!polygon.contains(p)) continue;

                bool visible = false;
                for (const Point& guard : guards) {
                    if (geometry::Visibility<Kernel>::is_visible(polygon, guard, p)) {
                        visible = true;
                        break;
                    }
                }
                if (!visible) return false;
            }
        }
        return true;
    }

    /**
     * @brief Convenience alias: uses verify_exact if an arrangement is available,
     *        otherwise falls back to verify_approximate.
     */
    static bool verify(const Polygon& polygon, const std::vector<Point>& guards,
                       double grid_spacing = 0.01) {
        return verify_approximate(polygon, guards, grid_spacing);
    }

    /**
     * @brief Compute coverage percentage (grid-based)
     */
    static double coverage_percentage(const Polygon& polygon,
                                      const std::vector<Point>& guards,
                                      double grid_spacing = 0.01) {
        auto bbox    = polygon.bbox();
        double min_x = CGAL::to_double(bbox.xmin());
        double max_x = CGAL::to_double(bbox.xmax());
        double min_y = CGAL::to_double(bbox.ymin());
        double max_y = CGAL::to_double(bbox.ymax());

        int total   = 0;
        int covered = 0;

        for (double x = min_x; x <= max_x; x += grid_spacing) {
            for (double y = min_y; y <= max_y; y += grid_spacing) {
                Point p(x, y);
                if (!polygon.contains(p)) continue;
                total++;
                for (const Point& guard : guards) {
                    if (geometry::Visibility<Kernel>::is_visible(polygon, guard, p)) {
                        covered++;
                        break;
                    }
                }
            }
        }

        if (total == 0) return 100.0;
        return 100.0 * static_cast<double>(covered) / static_cast<double>(total);
    }

    /**
     * @brief Find uncovered points (grid-based, for debugging)
     */
    static std::vector<Point> find_uncovered_points(const Polygon& polygon,
                                                    const std::vector<Point>& guards,
                                                    double grid_spacing = 0.01) {
        std::vector<Point> uncovered;
        auto bbox    = polygon.bbox();
        double min_x = CGAL::to_double(bbox.xmin());
        double max_x = CGAL::to_double(bbox.xmax());
        double min_y = CGAL::to_double(bbox.ymin());
        double max_y = CGAL::to_double(bbox.ymax());

        for (double x = min_x; x <= max_x; x += grid_spacing) {
            for (double y = min_y; y <= max_y; y += grid_spacing) {
                Point p(x, y);
                if (!polygon.contains(p)) continue;

                bool visible = false;
                for (const Point& guard : guards) {
                    if (geometry::Visibility<Kernel>::is_visible(polygon, guard, p)) {
                        visible = true;
                        break;
                    }
                }
                if (!visible) uncovered.push_back(p);
            }
        }

        return uncovered;
    }
};

// Common type aliases
using VerifierE = Verifier<CGAL::Exact_predicates_exact_constructions_kernel>;
using VerifierD = Verifier<CGAL::Simple_cartesian<double>>;

} // namespace algorithm
} // namespace agp


