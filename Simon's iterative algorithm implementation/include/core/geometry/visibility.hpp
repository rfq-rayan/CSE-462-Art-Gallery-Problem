#pragma once

#include "polygon.hpp"
#include "point.hpp"
#include <CGAL/Rotational_sweep_visibility_2.h>
#include <CGAL/Triangular_expansion_visibility_2.h>
#include <CGAL/Arr_segment_traits_2.h>
#include <CGAL/Arrangement_2.h>
#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <boost/optional.hpp>
#include <list>
#include <vector>
#include <algorithm>

namespace agp {
namespace geometry {

/**
 * @brief Visibility computation class
 *
 * Provides methods for computing visibility polygons and checking
 * visibility between points and faces using CGAL's visibility algorithms.
 *
 * @tparam Kernel CGAL kernel type
 */
template <typename Kernel = CGAL::Exact_predicates_exact_constructions_kernel>
class Visibility {
public:
    using Point = Point<Kernel>;
    using Polygon = Polygon<Kernel>;
    using Segment = Segment<Kernel>;
    using FT = typename Kernel::FT;

    using Traits_2 = CGAL::Arr_segment_traits_2<Kernel>;
    using Arrangement_2 = CGAL::Arrangement_2<Traits_2>;
    using Face_handle = typename Arrangement_2::Face_handle;
    using Halfedge_handle = typename Arrangement_2::Halfedge_handle;
    using Vertex_handle = typename Arrangement_2::Vertex_handle;

    using VP_type_1 = CGAL::Rotational_sweep_visibility_2<Arrangement_2>;
    using VP_type_2 = CGAL::Triangular_expansion_visibility_2<Arrangement_2>;

    /**
     * @brief Compute visibility polygon from a point
     *
     * @param P The containing polygon
     * @param guard The guard position
     * @return Polygon The visibility polygon
     */
    static Polygon compute_visibility_polygon(const Polygon& P, const Point& guard) {
        // Build arrangement from polygon edges
        Arrangement_2 arr;
        build_arrangement_from_polygon(P, arr);

        VP_type_1 vp(arr);
        Arrangement_2 vp_arr;

        // 1. Is guard a vertex?
        Vertex_handle v_match;
        bool found_v = false;
        for (auto vit = arr.vertices_begin(); vit != arr.vertices_end(); ++vit) {
            if (vit->point() == guard.cgal_point()) {
                v_match = vit;
                found_v = true;
                break;
            }
        }

        if (found_v) {
            auto circ = v_match->incident_halfedges();
            auto start = circ;
            Halfedge_handle best_e;
            bool found_best_e = false;
            do {
                if (!circ->face()->is_unbounded()) {
                    best_e = circ;
                    found_best_e = true;
                    break;
                }
            } while (++circ != start);
            if (!found_best_e) return Polygon();
            vp.compute_visibility(guard.cgal_point(), best_e, vp_arr);
            return arrangement_to_polygon(vp_arr);
        }

        // 2. Is guard on an edge?
        Halfedge_handle e_match;
        bool found_e = false;
        for (auto eit = arr.halfedges_begin(); eit != arr.halfedges_end(); ++eit) {
            if (!eit->is_fictitious()) {
                Segment seg(Point(eit->source()->point()), Point(eit->target()->point()));
                if (seg.has_on(guard)) {
                    e_match = eit;
                    found_e = true;
                    break;
                }
            }
        }

        if (found_e) {
            if (e_match->face()->is_unbounded()) {
                e_match = e_match->twin();
            }
            if (e_match->face()->is_unbounded()) return Polygon();
            vp.compute_visibility(guard.cgal_point(), e_match, vp_arr);
            return arrangement_to_polygon(vp_arr);
        }

        // 3. Guard is strictly inside
        auto face = find_containing_face(arr, guard);
        if (face == arr.faces_end() || face->is_unbounded()) {
            return Polygon(); // Guard strictly outside
        }

        vp.compute_visibility(guard.cgal_point(), face, vp_arr);
        return arrangement_to_polygon(vp_arr);
    }

    /**
     * @brief Check if two points can see each other
     *
     * @param P The containing polygon
     * @param observer The observer point
     * @param target The target point
     * @return true if line of sight exists
     */
    static bool is_visible(const Polygon& P, const Point& observer, const Point& target) {
        // Quick check: both points must be in polygon
        if (!P.contains(observer) || !P.contains(target)) {
            return false;
        }

        // Same point
        if (observer == target) {
            return true;
        }

        // Check line segment for intersections with polygon edges
        Segment sight_line(observer, target);

        // Check intersection with each polygon edge
        for (size_t i = 0; i < P.num_vertices(); ++i) {
            Segment edge = P.edge(i);

            // Skip edges adjacent to observer or target
            if (edge.source() == observer || edge.target() == observer ||
                edge.source() == target || edge.target() == target) {
                continue;
            }

            // Check for proper intersection
            if (sight_line.intersects(edge)) {
                auto intersection = sight_line.intersection(edge);
                if (intersection) {
                    Point ip = *intersection;
                    // Check if intersection is interior to both segments
                    if (sight_line.has_on_interior(ip) && edge.has_on_interior(ip)) {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    /**
     * @brief Check if a point sees a face completely
     *
     * Per paper: candidate c completely sees face f iff vis(c) ⊇ f.
     * We verify this exactly: first check all face vertices lie inside vis(c),
     * then confirm via CGAL boolean difference that face \ vis(c) is empty,
     * catching any interior slivers that lie outside vis(c) between vertices.
     *
     * @param P The containing polygon
     * @param observer The observer point
     * @param face The face to check (as polygon)
     * @return true if observer sees entire face
     */
    static bool sees_completely(const Polygon& P, const Point& observer,
                               const Polygon& face) {
        if (face.num_vertices() == 0) return false;

        // Compute actual visibility polygon from observer
        Polygon vis = compute_visibility_polygon(P, observer);
        if (vis.num_vertices() < 3) return false;

        using Polygon_2 = CGAL::Polygon_2<Kernel>;
        using PWH = CGAL::Polygon_with_holes_2<Kernel>;

        Polygon_2 vis_cgal = vis.cgal_polygon();
        if (vis_cgal.orientation() == CGAL::CLOCKWISE)
            vis_cgal.reverse_orientation();

        // Quick vertex check: all face vertices must be inside vis(observer)
        for (const auto& vertex : face.vertices()) {
            auto side = vis_cgal.bounded_side(vertex.cgal_point());
            if (side == CGAL::ON_UNBOUNDED_SIDE) {
                return false;
            }
        }

        // Exact containment check: face \ vis must be empty.
        // This catches interior slivers outside vis(observer) between vertices.
        if (face.num_vertices() < 3) return true; // degenerate face, vertex check sufficient

        Polygon_2 face_cgal = face.cgal_polygon();
        if (face_cgal.orientation() == CGAL::CLOCKWISE)
            face_cgal.reverse_orientation();

        // Validate polygons before CGAL boolean operations
        if (!vis_cgal.is_simple() || !face_cgal.is_simple()) {
            // Fall back to vertex-only check if polygons are not simple
            for (const auto& vertex : face.vertices()) {
                if (vis_cgal.bounded_side(vertex.cgal_point()) == CGAL::ON_UNBOUNDED_SIDE) {
                    return false;
                }
            }
            return true;
        }

        // Wrap difference in try-catch to handle CGAL edge cases
        try {
            std::list<PWH> diff;
            CGAL::difference(face_cgal, vis_cgal, std::back_inserter(diff));
            return diff.empty();
        } catch (const std::exception& e) {
            // Fall back to vertex check on CGAL errors
            for (const auto& vertex : face.vertices()) {
                if (vis_cgal.bounded_side(vertex.cgal_point()) == CGAL::ON_UNBOUNDED_SIDE) {
                    return false;
                }
            }
            return true;
        }
    }

    /**
     * @brief Check if a face sees another face completely
     *
     * @param P The containing polygon
     * @param observer_face The observer face
     * @param target_face The target face
     * @return true if any point in observer_face sees all of target_face
     */
    static bool sees_completely(const Polygon& P,
                               const Polygon& observer_face,
                               const Polygon& target_face) {
        // Use representative points from observer face
        std::vector<Point> representatives = get_representative_points(observer_face);

        for (const Point& rep : representatives) {
            if (sees_completely(P, rep, target_face)) {
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Compute weak visibility polygon from an edge
     *
     * A weak visibility polygon from edge e is the set of all points
     * visible from at least one point on e.
     *
     * @param P The containing polygon
     * @param edge_start Start point of edge
     * @param edge_end End point of edge
     * @return Polygon The weak visibility polygon
     */
    static Polygon compute_weak_visibility_polygon(const Polygon& P,
                                                   const Point& edge_start,
                                                   const Point& edge_end) {
        // Build arrangement from polygon
        Arrangement_2 arr;
        build_arrangement_from_polygon(P, arr);

        // Find the edge in the arrangement
        Segment query_edge(edge_start, edge_end);
        Halfedge_handle e = find_edge(arr, query_edge);

        if (e == arr.halfedges_end()) {
            return Polygon();
        }

        // Use triangular expansion for weak visibility
        // Need a query point on the edge - use midpoint
        Point midpoint = query_edge.midpoint();

        VP_type_2 vp(arr);
        Arrangement_2 vp_arr;

        vp.compute_visibility(midpoint.cgal_point(), e, vp_arr);

        return arrangement_to_polygon(vp_arr);
    }

    /**
     * @brief Compute enhanced visibility region vis_δ(q)
     *
     * vis_δ(q) = vis(q) ∪ (∪_{r ∈ reflex(P)} A(r, δ))
     * Per Definition 4 in the paper.
     *
     * @param P The containing polygon
     * @param guard The guard position
     * @param delta The vision-stability parameter
     * @return Polygon The enhanced visibility region
     */
    static Polygon compute_enhanced_visibility(const Polygon& P,
                                               const Point& guard,
                                               FT delta) {
        Polygon vis = compute_visibility_polygon(P, guard);
        auto reflex_vertices = P.reflex_vertex_positions();

        if (reflex_vertices.empty()) {
            return vis;
        }

        // Use CGAL boolean union to add wedge regions at each reflex vertex.
        // We work with Polygon_with_holes_2 throughout.
        using Polygon_2 = CGAL::Polygon_2<Kernel>;
        using PWH = CGAL::Polygon_with_holes_2<Kernel>;

        // Start with the visibility polygon as a simple polygon (no holes)
        Polygon_2 current = vis.cgal_polygon();
        // Ensure CCW orientation for boolean ops
        if (current.orientation() == CGAL::CLOCKWISE) current.reverse_orientation();

        // Validate base visibility polygon
        if (!current.is_simple() || current.size() < 3) {
            return vis;  // Return original visibility on invalid geometry
        }

        for (const auto& r : reflex_vertices) {
            Polygon wedge = compute_vision_enhancing_region(P, r, delta);
            if (wedge.num_vertices() < 3) continue;

            Polygon_2 wedge_cgal = wedge.cgal_polygon();
            if (wedge_cgal.orientation() == CGAL::CLOCKWISE)
                wedge_cgal.reverse_orientation();

            // Skip invalid wedge polygons
            if (!wedge_cgal.is_simple()) continue;

            try {
                // CGAL::join returns true if union is a single polygon
                PWH union_result;
                bool joined = CGAL::join(current, wedge_cgal, union_result);
                if (joined) {
                    current = union_result.outer_boundary();
                    if (current.orientation() == CGAL::CLOCKWISE)
                        current.reverse_orientation();
                }
                // If join returns false, polygons are disjoint — keep current
            } catch (const std::exception& e) {
                // Skip this wedge on CGAL errors - keep current polygon
                continue;
            }
        }

        return Polygon(current);
    }

    /**
     * @brief Compute diminished visibility region vis_{-δ}(q)
     *
     * vis_{-δ}(q) = vis(q) \ (∪_{r ∈ reflex(P)} A(r, δ))
     * Per Definition 4 in the paper.
     *
     * @param P The containing polygon
     * @param guard The guard position
     * @param delta The vision-stability parameter
     * @return Polygon The diminished visibility region
     */
    static Polygon compute_diminished_visibility(const Polygon& P,
                                                 const Point& guard,
                                                 FT delta) {
        Polygon vis = compute_visibility_polygon(P, guard);
        auto reflex_vertices = P.reflex_vertex_positions();

        if (reflex_vertices.empty()) {
            return vis;
        }

        using Polygon_2 = CGAL::Polygon_2<Kernel>;
        using PWH = CGAL::Polygon_with_holes_2<Kernel>;

        Polygon_2 current = vis.cgal_polygon();
        if (current.orientation() == CGAL::CLOCKWISE) current.reverse_orientation();

        // Validate base visibility polygon
        if (!current.is_simple() || current.size() < 3) {
            return vis;  // Return original visibility on invalid geometry
        }

        for (const auto& r : reflex_vertices) {
            Polygon wedge = compute_vision_enhancing_region(P, r, delta);
            if (wedge.num_vertices() < 3) continue;

            Polygon_2 wedge_cgal = wedge.cgal_polygon();
            if (wedge_cgal.orientation() == CGAL::CLOCKWISE)
                wedge_cgal.reverse_orientation();

            // Skip invalid wedge polygons
            if (!wedge_cgal.is_simple()) continue;

            try {
                std::list<PWH> diff_result;
                CGAL::difference(current, wedge_cgal, std::back_inserter(diff_result));

                if (!diff_result.empty()) {
                    // Take the largest remaining piece (the main visibility region)
                    auto best = diff_result.begin();
                    for (auto it = diff_result.begin(); it != diff_result.end(); ++it) {
                        if (CGAL::to_double(it->outer_boundary().area()) >
                            CGAL::to_double(best->outer_boundary().area())) {
                            best = it;
                        }
                    }
                    current = best->outer_boundary();
                    if (current.orientation() == CGAL::CLOCKWISE)
                        current.reverse_orientation();
                }
            } catch (const std::exception& e) {
                // Skip this wedge on CGAL errors - keep current polygon
                continue;
            }
        }

        return Polygon(current);
    }

    /**
     * @brief Compute vision-enhancing region A(r, δ) at reflex vertex
     *
     * Per paper: A(r, δ) is computed by rotating a ray around r by angle δ
     * from the internal angle bisector direction.
     *
     * @param P The containing polygon
     * @param reflex_vertex The reflex vertex position
     * @param delta The angle parameter
     * @return Polygon The vision-enhancing region (triangular wedge)
     */
    static Polygon compute_vision_enhancing_region(const Polygon& P,
                                                   const Point& reflex_vertex,
                                                   FT delta) {
        // The vision-enhancing region is a wedge at the reflex vertex
        // with angle δ on each side of the internal angle bisector

        // Find the edges incident to the reflex vertex
        std::vector<Point> adjacent_vertices;
        size_t reflex_idx = 0;
        for (size_t i = 0; i < P.num_vertices(); ++i) {
            if (P.vertex(i) == reflex_vertex) {
                size_t prev = (i + P.num_vertices() - 1) % P.num_vertices();
                size_t next = (i + 1) % P.num_vertices();
                adjacent_vertices.push_back(P.vertex(prev));
                adjacent_vertices.push_back(P.vertex(next));
                reflex_idx = i;
                break;
            }
        }

        if (adjacent_vertices.size() != 2) {
            return Polygon();
        }

        // Get direction vectors from reflex vertex to adjacent vertices
        Point v1 = adjacent_vertices[0];
        Point v2 = adjacent_vertices[1];

        FT dx1 = v1.x() - reflex_vertex.x();
        FT dy1 = v1.y() - reflex_vertex.y();
        FT dx2 = v2.x() - reflex_vertex.x();
        FT dy2 = v2.y() - reflex_vertex.y();

        // Normalize directions
        double len1 = std::sqrt(CGAL::to_double(dx1 * dx1 + dy1 * dy1));
        double len2 = std::sqrt(CGAL::to_double(dx2 * dx2 + dy2 * dy2));

        if (len1 < 1e-10 || len2 < 1e-10) {
            return Polygon();
        }

        double nx1 = CGAL::to_double(dx1) / len1;
        double ny1 = CGAL::to_double(dy1) / len1;
        double nx2 = CGAL::to_double(dx2) / len2;
        double ny2 = CGAL::to_double(dy2) / len2;

        // Compute internal angle bisector direction
        // For a reflex vertex, the internal angle is > 180 degrees
        // The bisector points into the interior of the polygon
        double bisector_x = nx1 + nx2;
        double bisector_y = ny1 + ny2;
        double bisector_len = std::sqrt(bisector_x * bisector_x + bisector_y * bisector_y);

        if (bisector_len < 1e-10) {
            // Vectors point in opposite directions, use perpendicular
            bisector_x = -ny1;
            bisector_y = nx1;
            bisector_len = std::sqrt(bisector_x * bisector_x + bisector_y * bisector_y);
        }

        bisector_x /= bisector_len;
        bisector_y /= bisector_len;

        // Check if bisector points into polygon interior
        Point bisector_test(reflex_vertex.x() + FT(bisector_x * 0.001),
                            reflex_vertex.y() + FT(bisector_y * 0.001));
        if (!P.contains(bisector_test)) {
            // Flip direction
            bisector_x = -bisector_x;
            bisector_y = -bisector_y;
        }

        // Compute angle of bisector
        double bisector_angle = std::atan2(bisector_y, bisector_x);

        // Convert delta to radians (delta is a fraction of pi)
        double delta_rad = CGAL::to_double(delta) * CGAL_PI;

        // Create wedge vertices: reflex vertex + two rays at ±δ from bisector
        double angle1 = bisector_angle - delta_rad;
        double angle2 = bisector_angle + delta_rad;

        // Direction vectors for the two rays
        double dir1_x = std::cos(angle1);
        double dir1_y = std::sin(angle1);
        double dir2_x = std::cos(angle2);
        double dir2_y = std::sin(angle2);

        // Find where rays intersect polygon boundary
        Point ray1_end = find_ray_polygon_intersection(P, reflex_vertex,
                                                        FT(dir1_x), FT(dir1_y));
        Point ray2_end = find_ray_polygon_intersection(P, reflex_vertex,
                                                        FT(dir2_x), FT(dir2_y));

        // Create the wedge polygon (triangle)
        std::vector<Point> wedge_vertices;
        wedge_vertices.push_back(reflex_vertex);
        wedge_vertices.push_back(ray1_end);
        wedge_vertices.push_back(ray2_end);

        return Polygon(wedge_vertices);
    }

    /**
     * @brief Get all vertices visible from a point
     *
     * @param P The containing polygon
     * @param guard The guard position
     * @return std::vector<Point> Visible vertices
     */
    static std::vector<Point> get_visible_vertices(const Polygon& P,
                                                   const Point& guard) {
        std::vector<Point> visible;
        visible.reserve(P.num_vertices());

        for (const auto& vertex : P.vertices()) {
            if (is_visible(P, guard, vertex)) {
                visible.push_back(vertex);
            }
        }

        return visible;
    }

    /**
     * @brief Compute visible portion of an edge
     *
     * @param P The containing polygon
     * @param guard The guard position
     * @param edge The edge to check
     * @return std::vector<Point> Visible points on the edge
     */
    static std::vector<Point> compute_visible_portion(const Polygon& P,
                                                       const Point& guard,
                                                       const Segment& edge) {
        std::vector<Point> visible;
        if (is_visible(P, guard, edge.source())) {
            visible.push_back(edge.source());
        }
        if (is_visible(P, guard, edge.target())) {
            visible.push_back(edge.target());
        }
        return visible;
    }

    /**
     * @brief Get indices of all visible vertices
     *
     * @param P The containing polygon
     * @param guard The guard position
     * @return std::vector<size_t> Indices of visible vertices
     */
    static std::vector<size_t> visible_vertices(const Polygon& P, const Point& guard) {
        std::vector<size_t> result;
        for (size_t i = 0; i < P.num_vertices(); ++i) {
            if (is_visible(P, guard, P.vertex(i))) {
                result.push_back(i);
            }
        }
        return result;
    }

    /**
     * @brief Check if guards cover entire polygon
     *
     * @param P The polygon
     * @param guards The guard positions
     * @return true if guards see entire polygon
     */
    static bool covers_polygon(const Polygon& P, const std::vector<Point>& guards) {
        for (size_t i = 0; i < P.num_vertices(); ++i) {
            Point vertex = P.vertex(i);
            bool seen = false;
            for (const auto& guard : guards) {
                if (is_visible(P, guard, vertex)) {
                    seen = true;
                    break;
                }
            }
            if (!seen) return false;
        }
        return true;
    }

    /**
     * @brief Check if a witness point is covered by a set of guards using enhanced visibility
     *
     * Helper used by the iterative algorithm's vision-stability check.
     * A witness is "enhanced-covered" if it lies in vis_δ(g) for at least one guard g.
     */
    static bool is_covered_enhanced(const Polygon& P, FT delta,
                                    const Point& witness,
                                    const std::vector<Point>& guards) {
        for (const Point& guard : guards) {
            Polygon vis_enh = compute_enhanced_visibility(P, guard, delta);
            if (vis_enh.contains(witness) || vis_enh.is_on_boundary(witness)) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Check if a witness point is covered using diminished visibility
     */
    static bool is_covered_diminished(const Polygon& P, FT delta,
                                      const Point& witness,
                                      const std::vector<Point>& guards) {
        for (const Point& guard : guards) {
            Polygon vis_dim = compute_diminished_visibility(P, guard, delta);
            if (vis_dim.contains(witness) || vis_dim.is_on_boundary(witness)) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Find ray intersection with polygon boundary
     *
     * @param P The polygon
     * @param source The ray source point
     * @param dx X component of ray direction
     * @param dy Y component of ray direction
     * @return Point The intersection point with the polygon boundary
     */
    static Point find_ray_polygon_intersection(const Polygon& P,
                                               const Point& source,
                                               FT dx, FT dy) {
        auto bbox = P.bbox();
        FT best_t = FT(bbox.xmax() - bbox.xmin()) + FT(bbox.ymax() - bbox.ymin());

        for (size_t i = 0; i < P.num_vertices(); ++i) {
            Segment edge = P.edge(i);
            Point e1 = edge.source();
            Point e2 = edge.target();

            // Ray-segment intersection
            FT denom = dx * (e2.y() - e1.y()) - dy * (e2.x() - e1.x());
            if (denom == FT(0)) continue; // Parallel

            FT t = ((e1.x() - source.x()) * (e2.y() - e1.y()) -
                   (e1.y() - source.y()) * (e2.x() - e1.x())) / denom;
            FT u = ((e1.x() - source.x()) * dy - (e1.y() - source.y()) * dx) / denom;

            if (t > FT(0) && u >= FT(0) && u <= FT(1)) {
                if (t < best_t) {
                    best_t = t;
                }
            }
        }

        return Point(source.x() + dx * best_t, source.y() + dy * best_t);
    }

private:

    /**
     * @brief Build CGAL arrangement from polygon edges
     */
    static void build_arrangement_from_polygon(const Polygon& P, Arrangement_2& arr) {
        auto edges = P.edges();
        std::vector<typename Traits_2::X_monotone_curve_2> curves;
        curves.reserve(edges.size());

        for (const auto& edge : edges) {
            curves.push_back(typename Traits_2::X_monotone_curve_2(
                edge.source().cgal_point(),
                edge.target().cgal_point()
            ));
        }

        CGAL::insert(arr, curves.begin(), curves.end());
    }

    /**
     * @brief Find the face containing a point using ray-crossing (winding number) test
     */
    static Face_handle find_containing_face(Arrangement_2& arr, const Point& p) {
        for (auto fit = arr.faces_begin(); fit != arr.faces_end(); ++fit) {
            if (!fit->is_unbounded() && fit->has_outer_ccb()) {
                // Ray-crossing point-in-polygon test
                int crossings = 0;
                auto he_circ = fit->outer_ccb();
                auto he_curr = he_circ;
                do {
                    auto src = he_curr->source()->point();
                    auto tgt = he_curr->target()->point();
                    // Check if the edge crosses the horizontal ray from p to +infinity
                    bool src_above = (src.y() > p.y());
                    bool tgt_above = (tgt.y() > p.y());
                    if (src_above != tgt_above) {
                        // Compute x-intercept of edge at y = p.y()
                        FT x_int = src.x() + (p.y() - src.y()) *
                                   (tgt.x() - src.x()) / (tgt.y() - src.y());
                        if (p.x() < x_int) {
                            crossings++;
                        }
                    }
                    ++he_curr;
                } while (he_curr != he_circ);

                if (crossings % 2 == 1) {
                    return fit;
                }
            }
        }
        return arr.faces_end();
    }

    /**
     * @brief Find edge in arrangement
     */
    static Halfedge_handle find_edge(Arrangement_2& arr, const Segment& s) {
        for (auto eit = arr.halfedges_begin(); eit != arr.halfedges_end(); ++eit) {
            if (!eit->is_fictitious()) {
                auto source = eit->source()->point();
                auto target = eit->target()->point();

                if ((source == s.source().cgal_point() &&
                     target == s.target().cgal_point()) ||
                    (source == s.target().cgal_point() &&
                     target == s.source().cgal_point())) {
                    return eit;
                }
            }
        }
        return arr.halfedges_end();
    }

    /**
     * @brief Convert arrangement to polygon
     */
    static Polygon arrangement_to_polygon(const Arrangement_2& arr) {
        std::vector<Point> vertices;

        // Find the bounded face (visibility polygon)
        for (auto fit = arr.faces_begin(); fit != arr.faces_end(); ++fit) {
            if (!fit->is_unbounded() && fit->has_outer_ccb()) {
                auto he_circ = fit->outer_ccb();
                auto he_curr = he_circ;
                do {
                    if (!he_curr->is_fictitious()) {
                        vertices.push_back(Point(he_curr->source()->point()));
                    }
                    ++he_curr;
                } while (he_curr != he_circ);
                break;
            }
        }

        return Polygon(vertices);
    }



    /**
     * @brief Compute centroid of a polygon
     */
    static Point compute_centroid(const Polygon& poly) {
        FT cx = FT(0), cy = FT(0);
        for (const auto& v : poly.vertices()) {
            cx += v.x();
            cy += v.y();
        }
        cx /= FT(poly.num_vertices());
        cy /= FT(poly.num_vertices());
        return Point(cx, cy);
    }

    /**
     * @brief Get representative points from a face
     */
    static std::vector<Point> get_representative_points(const Polygon& face) {
        std::vector<Point> reps;

        // Add vertices
        for (const auto& v : face.vertices()) {
            reps.push_back(v);
        }

        // Add centroid
        reps.push_back(compute_centroid(face));

        return reps;
    }
};

// Common type aliases
using VisibilityE = Visibility<CGAL::Exact_predicates_exact_constructions_kernel>;
using VisibilityD = Visibility<CGAL::Simple_cartesian<double>>;

} // namespace geometry
} // namespace agp
