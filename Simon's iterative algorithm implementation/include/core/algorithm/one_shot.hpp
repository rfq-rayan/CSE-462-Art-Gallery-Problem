#pragma once

/**
 * @file one_shot.hpp
 * @brief One-Shot Algorithm for the Art Gallery Problem
 *
 * Implements Algorithm 1 from "A Practical Algorithm with Performance Guarantees
 * for the Art Gallery Problem" (LIPIcs.SoCG.2021.44).
 *
 * The one-shot algorithm:
 * 1. Computes ALL candidate lines (vertex-vertex + vertex-reflex tangent lines)
 * 2. Builds a single full arrangement from all these lines
 * 3. Solves a single (potentially large) IP
 * 4. Returns guards from the solution
 */

#include "../geometry/polygon.hpp"
#include "../geometry/arrangement.hpp"
#include "../geometry/visibility.hpp"
#include "../geometry/segment.hpp"
#include "../ip/ip_formulation.hpp"
#include "../ip/ip_solver.hpp"
#include <chrono>
#include <vector>
#include <set>

namespace agp {
namespace algorithm {

/**
 * @brief One-Shot Algorithm configuration
 */
struct OneShotConfig {
    ip::SolverType solver_type = ip::SolverType::GLPK;
    double time_limit_seconds  = 3600.0;
    int    verbosity           = 1;

    static OneShotConfig default_config() { return OneShotConfig{}; }
};

/**
 * @brief One-Shot Algorithm for Art Gallery Problem
 *
 * Computes all candidate lines up-front, builds a single arrangement,
 * and solves one IP — trading initialization cost for a smaller number
 * of solver calls.
 *
 * @tparam Kernel CGAL kernel type
 */
template <typename Kernel = CGAL::Exact_predicates_exact_constructions_kernel>
class OneShotAlgorithm {
public:
    using Polygon     = geometry::Polygon<Kernel>;
    using Point       = geometry::Point<Kernel>;
    using Segment     = geometry::Segment<Kernel>;
    using Arrangement = geometry::Arrangement<Kernel>;
    using FT          = typename Kernel::FT;

    struct Result {
        std::vector<Point> guards;
        double total_time;
        double ip_time;
        bool   optimal_found;
        size_t num_candidates;
        size_t num_witnesses;
        std::string status_message;
    };

    explicit OneShotAlgorithm(const OneShotConfig& config = OneShotConfig::default_config())
        : config_(config) {}

    /**
     * @brief Solve the Art Gallery Problem with the one-shot approach.
     */
    Result solve(const Polygon& polygon) {
        Result result;
        result.optimal_found = false;

        auto t_start = std::chrono::high_resolution_clock::now();

        // ----- Step 1: Compute all candidate lines -----
        auto candidate_lines = compute_candidate_lines(polygon);

        // ----- Step 2: Build arrangement -----
        Arrangement arr(polygon);
        for (const auto& seg : candidate_lines) {
            arr.insert_segment(seg);
        }

        result.num_candidates = arr.get_candidates().size();
        result.num_witnesses  = arr.get_witnesses().size();

        // ----- Step 3: Build and solve IP -----
        ip::IPFormulation<Kernel> formulation(arr, ip::IPType::NORMAL);
        double epsilon = formulation.calculate_epsilon();
        formulation.build_normal(epsilon);

        ip::IPSolver solver(config_.solver_type);
        solver.set_time_limit(config_.time_limit_seconds);

        auto t_ip_start  = std::chrono::high_resolution_clock::now();
        auto solver_result = solver.solve(formulation);
        auto t_ip_end    = std::chrono::high_resolution_clock::now();

        result.ip_time = std::chrono::duration<double>(t_ip_end - t_ip_start).count();

        // ----- Step 4: Extract guards -----
        if (solver_result.success) {
            auto sol = formulation.interpret_solution(solver_result.solution);
            result.optimal_found = sol.face_guards.empty() && sol.unseen_witnesses.empty();
            result.status_message = result.optimal_found ? "Optimal" : "Feasible";

            // Convert vertex guard indices to points
            auto all_verts = arr.all_vertices();
            for (size_t idx : sol.vertex_guards) {
                if (idx < all_verts.size()) {
                    result.guards.push_back(Point(all_verts[idx]->point()));
                }
            }
            // Convert face guard indices to centroids
            auto all_faces = arr.all_faces();
            for (size_t idx : sol.face_guards) {
                if (idx < all_faces.size()) {
                    result.guards.push_back(arr.compute_face_centroid(all_faces[idx]));
                }
            }
        } else {
            result.status_message = "IP solver failed: " + solver_result.status_message;
        }

        auto t_end = std::chrono::high_resolution_clock::now();
        result.total_time = std::chrono::duration<double>(t_end - t_start).count();

        return result;
    }

private:
    OneShotConfig config_;

    /**
     * @brief Compute all candidate lines for the one-shot arrangement.
     *
     * Per the paper, candidate lines are:
     *   (a) Vertex-vertex lines: line through every pair of polygon vertices,
     *       clipped to the polygon interior.
     *   (b) Vertex-reflex tangent lines: for each reflex vertex r and each
     *       visible vertex v, the line through r and v extended to the boundary.
     *
     * We return these as segments clipped inside the polygon.
     */
    std::vector<Segment> compute_candidate_lines(const Polygon& polygon) {
        std::vector<Segment> lines;
        size_t n = polygon.num_vertices();
        if (n < 2) return lines;

        auto reflex_indices = polygon.reflex_indices();

        // (a) All vertex-vertex lines
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                Point p1 = polygon.vertex(i);
                Point p2 = polygon.vertex(j);

                // Check if the line segment is inside the polygon
                // (midpoint must be inside)
                Segment seg(p1, p2);
                if (polygon.contains(seg.midpoint())) {
                    lines.push_back(seg);
                }
            }
        }

        // (b) Reflex-vertex tangent lines: for each reflex vertex, extend
        //     lines through all other visible polygon vertices
        for (size_t ri : reflex_indices) {
            Point r = polygon.vertex(ri);
            for (size_t i = 0; i < n; ++i) {
                if (i == ri) continue;
                Point v = polygon.vertex(i);

                // Only if r can see v
                if (!geometry::Visibility<Kernel>::is_visible(polygon, r, v)) continue;

                // Extend the line r→v to the polygon boundary in both directions
                FT dx = v.x() - r.x();
                FT dy = v.y() - r.y();

                Point far_end = geometry::Visibility<Kernel>::find_ray_polygon_intersection(
                    polygon, r, dx, dy);
                Point far_start = geometry::Visibility<Kernel>::find_ray_polygon_intersection(
                    polygon, r, -dx, -dy);

                Segment extended(far_start, far_end);
                if (polygon.contains(extended.midpoint())) {
                    lines.push_back(extended);
                }
            }
        }

        return lines;
    }
};

// Common type aliases
using OneShotAlgorithmE = OneShotAlgorithm<CGAL::Exact_predicates_exact_constructions_kernel>;

} // namespace algorithm
} // namespace agp
