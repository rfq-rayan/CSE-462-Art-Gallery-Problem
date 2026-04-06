#pragma once

#include "polygon.hpp"
#include "point.hpp"
#include "visibility.hpp"
#include <CGAL/Arr_segment_traits_2.h>
#include <CGAL/Arrangement_2.h>
#include <boost/optional.hpp>
#include <set>
#include <map>
#include <random>

namespace agp {
namespace geometry {

/**
 * @brief Arrangement data structure for candidate/witness management
 *
 * Partitions the polygon into faces using visibility regions and provides
 * splitting operations for the iterative algorithm.
 *
 * @tparam Kernel CGAL kernel type
 */
template <typename Kernel = CGAL::Exact_predicates_exact_constructions_kernel>
class Arrangement {
public:
    using Point = Point<Kernel>;
    using Polygon = Polygon<Kernel>;
    using Segment = Segment<Kernel>;
    using FT = typename Kernel::FT;

    using Traits_2 = CGAL::Arr_segment_traits_2<Kernel>;
    using CGAL_Arrangement_2 = CGAL::Arrangement_2<Traits_2>;
    using Face_handle = typename CGAL_Arrangement_2::Face_handle;
    using Vertex_handle = typename CGAL_Arrangement_2::Vertex_handle;
    using Halfedge_handle = typename CGAL_Arrangement_2::Halfedge_handle;
    using Face_const_handle = typename CGAL_Arrangement_2::Face_const_handle;
    using Vertex_const_handle = typename CGAL_Arrangement_2::Vertex_const_handle;
    using Halfedge_const_handle = typename CGAL_Arrangement_2::Halfedge_const_handle;

    /**
     * @brief Split type enumeration
     */
    enum class SplitType {
        SQUARE,
        ANGULAR,
        REFLEX_CHORD,
        EXTENSION,
        VISIBILITY_LINE
    };

    /**
     * @brief Result of a split operation
     */
    struct SplitResult {
        bool success;
        std::vector<Face_handle> new_faces;
        SplitType type;
        std::string message;
    };

    /**
     * @brief Candidate set (vertices and faces)
     */
    struct CandidateSet {
        std::set<Vertex_handle> vertex_candidates;
        std::set<Face_handle> face_candidates;

        size_t size() const {
            return vertex_candidates.size() + face_candidates.size();
        }

        bool empty() const {
            return vertex_candidates.empty() && face_candidates.empty();
        }

        void clear() {
            vertex_candidates.clear();
            face_candidates.clear();
        }
    };

    /**
     * @brief Witness set (vertices and faces)
     */
    struct WitnessSet {
        std::set<Vertex_handle> vertex_witnesses;
        std::set<Face_handle> face_witnesses;

        size_t size() const {
            return vertex_witnesses.size() + face_witnesses.size();
        }

        bool empty() const {
            return vertex_witnesses.empty() && face_witnesses.empty();
        }

        void clear() {
            vertex_witnesses.clear();
            face_witnesses.clear();
        }
    };

    // Constructors
    Arrangement() = default;

    explicit Arrangement(const Polygon& polygon)
        : polygon_(polygon), current_granularity_(FT(1) / FT(16)) {
        build_initial_arrangement();
    }

    // Accessors
    CGAL_Arrangement_2& cgal_arrangement() { return arrangement_; }
    const CGAL_Arrangement_2& cgal_arrangement() const { return arrangement_; }

    const Polygon& get_polygon() const { return polygon_; }
    FT current_granularity() const { return current_granularity_; }

    // Face queries (non-const version)
    Face_handle find_face(const Point& p) {
        // Walk through faces to find containing face
        for (auto fit = arrangement_.faces_begin(); fit != arrangement_.faces_end(); ++fit) {
            if (!fit->is_unbounded() && !fit->is_fictitious()) {
                if (point_in_face(p, fit)) {
                    return fit;
                }
            }
        }
        return arrangement_.faces_end(); // Not found
    }

    // Const version
    Face_const_handle find_face(const Point& p) const {
        for (auto fit = arrangement_.faces_begin(); fit != arrangement_.faces_end(); ++fit) {
            if (!fit->is_unbounded() && !fit->is_fictitious()) {
                if (point_in_face(p, fit)) {
                    return fit;
                }
            }
        }
        return arrangement_.faces_end();
    }

    std::vector<Face_handle> all_faces() {
        std::vector<Face_handle> faces;
        for (auto fit = arrangement_.faces_begin(); fit != arrangement_.faces_end(); ++fit) {
            if (!fit->is_unbounded() && !fit->is_fictitious()) {
                faces.push_back(fit);
            }
        }
        return faces;
    }

    std::vector<Vertex_handle> all_vertices() {
        std::vector<Vertex_handle> verts;
        for (auto vit = arrangement_.vertices_begin(); vit != arrangement_.vertices_end(); ++vit) {
            verts.push_back(vit);
        }
        return verts;
    }

    // Const versions returning size_t counts instead of handles
    size_t num_faces() const {
        size_t count = 0;
        for (auto fit = arrangement_.faces_begin(); fit != arrangement_.faces_end(); ++fit) {
            if (!fit->is_unbounded() && !fit->is_fictitious()) {
                count++;
            }
        }
        return count;
    }

    size_t num_vertices() const {
        size_t count = 0;
        for (auto vit = arrangement_.vertices_begin(); vit != arrangement_.vertices_end(); ++vit) {
            count++;
        }
        return count;
    }

    // Const versions returning const handles
    std::vector<Face_const_handle> all_faces() const {
        std::vector<Face_const_handle> faces;
        for (auto fit = arrangement_.faces_begin(); fit != arrangement_.faces_end(); ++fit) {
            if (!fit->is_unbounded() && !fit->is_fictitious()) {
                faces.push_back(fit);
            }
        }
        return faces;
    }

    std::vector<Vertex_const_handle> all_vertices() const {
        std::vector<Vertex_const_handle> verts;
        for (auto vit = arrangement_.vertices_begin(); vit != arrangement_.vertices_end(); ++vit) {
            verts.push_back(vit);
        }
        return verts;
    }

    // Initialization from WVPT
    void initialize_from_wvpt(const std::vector<Segment>& defining_chords) {
        // Add defining chords to arrangement
        for (const auto& chord : defining_chords) {
            insert_segment(chord);
        }

        // Add horizontal and vertical rays from reflex vertices
        add_horizontal_vertical_rays();
    }

    void add_horizontal_vertical_rays() {
        for (size_t reflex_idx : polygon_.reflex_indices()) {
            Point reflex = polygon_.vertex(reflex_idx);

            // Shoot rays in all 4 directions (per paper specification)
            std::vector<std::pair<FT, FT>> directions = {
                {FT(1), FT(0)},   // +X
                {FT(-1), FT(0)},  // -X
                {FT(0), FT(1)},   // +Y
                {FT(0), FT(-1)}   // -Y
            };

            for (const auto& [dx, dy] : directions) {
                Point target = find_ray_intersection(reflex, dx, dy);
                if (target != reflex) {
                    insert_segment(Segment(reflex, target));
                }
            }
        }
    }

    // Segment insertion
    void insert_segment(const Segment& seg) {
        CGAL::insert(arrangement_, seg.cgal_segment());
    }

    // Splitting operations
    SplitResult square_split(Face_handle face) {
        SplitResult result;
        result.type = SplitType::SQUARE;
        result.success = false;

        if (face->is_unbounded() || face->is_fictitious()) {
            result.message = "Invalid face";
            return result;
        }

        // Get face bounding box
        auto bbox = compute_face_bbox(face);
        FT mid_x = (bbox.first.x() + bbox.second.x()) / FT(2);
        FT mid_y = (bbox.first.y() + bbox.second.y()) / FT(2);

        // Create horizontal and vertical split segments
        Point left(bbox.first.x(), mid_y);
        Point right(bbox.second.x(), mid_y);
        Point bottom(mid_x, bbox.first.y());
        Point top(mid_x, bbox.second.y());

        // Insert split segments
        insert_segment(Segment(left, right));
        insert_segment(Segment(bottom, top));

        result.success = true;
        result.message = "Square split applied";
        return result;
    }

    SplitResult angular_split(Face_handle face, FT granularity) {
        SplitResult result;
        result.type = SplitType::ANGULAR;
        result.success = false;

        // Find reflex vertex adjacent to this face
        auto reflex = get_adjacent_reflex_vertex(face);
        if (!reflex) {
            result.message = "No adjacent reflex vertex";
            return result;
        }

        Point r = *reflex;

        // Compute face centroid
        Point centroid = compute_face_centroid(face);

        // Shoot rays at angles i * granularity
        // Using discrete angles based on granularity
        int num_rays = static_cast<int>(CGAL::to_double(FT(1) / granularity));
        for (int i = -num_rays; i <= num_rays; ++i) {
            FT angle = FT(i) * granularity * FT(CGAL_PI);

            // Compute ray direction
            double dx = std::cos(CGAL::to_double(angle));
            double dy = std::sin(CGAL::to_double(angle));

            Point target = find_ray_intersection(r, FT(dx), FT(dy));
            if (target != r) {
                insert_segment(Segment(r, target));
            }
        }

        result.success = true;
        result.message = "Angular split applied";
        return result;
    }

    SplitResult reflex_chord_split(Face_handle face) {
        SplitResult result;
        result.type = SplitType::REFLEX_CHORD;
        result.success = false;

        // Check for reflex chords intersecting the face
        for (size_t i = 0; i < polygon_.num_reflex_vertices(); ++i) {
            for (size_t j = i + 1; j < polygon_.num_reflex_vertices(); ++j) {
                size_t idx1 = polygon_.reflex_indices()[i];
                size_t idx2 = polygon_.reflex_indices()[j];

                Segment chord(polygon_.vertex(idx1), polygon_.vertex(idx2));

                // Check if chord is valid (inside polygon) and intersects face
                if (polygon_.contains_strictly(chord.midpoint())) {
                    // Check intersection with face
                    if (face_intersects_segment(face, chord)) {
                        insert_segment(chord);
                        result.success = true;
                        result.message = "Reflex chord split applied";
                        return result;
                    }
                }
            }
        }

        result.message = "No valid reflex chord found";
        return result;
    }

    SplitResult extension_split(Face_handle face) {
        SplitResult result;
        result.type = SplitType::EXTENSION;
        result.success = false;

        auto reflex = get_adjacent_reflex_vertex(face);
        if (!reflex) {
            result.message = "No adjacent reflex vertex";
            return result;
        }

        Point r = *reflex;
        size_t reflex_idx = *get_reflex_index(r);
        size_t n = polygon_.num_vertices();

        // Get edges incident to reflex vertex
        Segment edge1 = polygon_.edge(reflex_idx);
        Segment edge2 = polygon_.edge((reflex_idx + n - 1) % n);

        // Shoot extensions AWAY from reflex vertex (opposite direction of incident edge)
        // The extension goes through the polygon interior from the reflex vertex
        for (const auto& edge : {edge1, edge2}) {
            // Get the other endpoint of the edge (not the reflex vertex)
            Point other = (edge.source() == r) ? edge.target() : edge.source();

            // Direction is AWAY from the reflex vertex (opposite of edge direction toward neighbor)
            FT dx = r.x() - other.x();
            FT dy = r.y() - other.y();

            // Normalize
            double len = std::sqrt(CGAL::to_double(dx * dx + dy * dy));
            if (len < 1e-10) continue;
            dx = FT(CGAL::to_double(dx) / len);
            dy = FT(CGAL::to_double(dy) / len);

            Point target = find_ray_intersection(r, dx, dy);
            if (target != r) {
                insert_segment(Segment(r, target));
            }
        }

        result.success = true;
        result.message = "Extension split applied";
        return result;
    }

    SplitResult visibility_line_split(Face_handle face, const std::vector<Point>& witnesses, const std::vector<Point>& guards) {
        SplitResult result;
        result.type = SplitType::VISIBILITY_LINE;
        result.success = false;

        // Per paper: split the face by inserting edges of vis(w) that cross the face boundary.
        // This separates the face into parts that are and aren't visible from w.
        for (const auto& w : witnesses) {
            // Compute visibility polygon from witness w
            Polygon vis_w = Visibility<Kernel>::compute_visibility_polygon(polygon_, w);
            if (vis_w.num_vertices() < 3) continue;

            // Insert any edge of vis(w) that intersects the face boundary
            bool inserted = false;
            for (size_t i = 0; i < vis_w.num_vertices(); ++i) {
                Segment vis_edge = vis_w.edge(i);
                // Only insert if the edge's midpoint is inside the polygon
                // and it crosses the face (splits it)
                if (polygon_.contains(vis_edge.midpoint()) &&
                    face_intersects_segment(face, vis_edge)) {
                    insert_segment(vis_edge);
                    inserted = true;
                }
            }

            if (inserted) {
                result.success = true;
                result.message = "Visibility line split applied";
                return result;
            }
        }

        // Fallback for empty witnesses: try any guard's visibility boundary
        for (const auto& g : guards) {
            Polygon vis_g = Visibility<Kernel>::compute_visibility_polygon(polygon_, g);
            for (size_t i = 0; i < vis_g.num_vertices(); ++i) {
                Segment vis_edge = vis_g.edge(i);
                if (polygon_.contains(vis_edge.midpoint()) &&
                    face_intersects_segment(face, vis_edge)) {
                    insert_segment(vis_edge);
                    result.success = true;
                    result.message = "Visibility line split applied (guard fallback)";
                    return result;
                }
            }
        }

        result.message = "No valid visibility line found";
        return result;
    }

    // Splittability checks
    bool is_splittable(Face_handle face, FT granularity) const {
        // Check unsplittable conditions
        if (is_unsplittable(face, granularity)) {
            return false;
        }
        return true;
    }

    // Const handle version - simplified check for IP formulation
    bool is_splittable(Face_const_handle face, FT granularity) const {
        // Unbounded or fictitious faces are unsplittable
        if (face->is_unbounded() || face->is_fictitious()) {
            return false;
        }
        // For the IP formulation, we conservatively assume splittable
        // The actual split check happens during the algorithm
        return true;
    }

    bool is_unsplittable(Face_handle face, FT granularity) const {
        // Unbounded or fictitious faces are unsplittable
        if (face->is_unbounded() || face->is_fictitious()) {
            return true;
        }

        // Condition 1: Face incident to more than one reflex vertex -> square split works
        int reflex_count = count_adjacent_reflex_vertices(face);
        if (reflex_count > 1) {
            return false; // Splittable by square split
        }

        // Condition 2: Angular capacity > 4πλ -> angular split works
        FT angular_cap = angular_capacity(face);
        FT threshold = FT(4) * granularity * FT(3.14159265359);
        if (angular_cap > threshold) {
            return false; // Splittable by angular split
        }

        // Condition 3: Check if reflex chord split is possible
        if (can_reflex_chord_split(face)) {
            return false;
        }

        // Condition 4: Check if extension split is possible
        if (can_extension_split(face)) {
            return false;
        }

        return true; // All conditions met = unsplittable
    }

    /**
     * @brief Check if reflex chord split is possible for this face
     */
    bool can_reflex_chord_split(Face_handle face) const {
        auto reflex_indices = polygon_.reflex_indices();
        if (reflex_indices.size() < 2) return false;

        for (size_t i = 0; i < reflex_indices.size(); ++i) {
            for (size_t j = i + 1; j < reflex_indices.size(); ++j) {
                Point r1 = polygon_.vertex(reflex_indices[i]);
                Point r2 = polygon_.vertex(reflex_indices[j]);
                Segment chord(r1, r2);

                // Check if chord midpoint is inside polygon (chord is valid)
                if (polygon_.contains(chord.midpoint())) {
                    // Check if chord intersects this face
                    if (face_intersects_segment(face, chord)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    /**
     * @brief Check if extension split is possible for this face
     */
    bool can_extension_split(Face_handle face) const {
        auto reflex = get_adjacent_reflex_vertex(face);
        if (!reflex) return false;

        // Get the reflex vertex index
        auto reflex_idx_opt = get_reflex_index(*reflex);
        if (!reflex_idx_opt) return false;

        size_t reflex_idx = *reflex_idx_opt;
        size_t n = polygon_.num_vertices();

        // Check extensions of incident edges
        Segment prev_edge = polygon_.edge((reflex_idx + n - 1) % n);
        Segment next_edge = polygon_.edge(reflex_idx);

        for (const auto& edge : {prev_edge, next_edge}) {
            Point dir = edge.direction();
            double len_sq = CGAL::to_double(dir.x() * dir.x() + dir.y() * dir.y());
            if (len_sq == 0) continue;
            double len = std::sqrt(len_sq);

            FT dx = dir.x() / len;
            FT dy = dir.y() / len;

            // Check if extension in this direction would intersect the face
            Point target = find_ray_intersection(*reflex, dx, dy);
            if (target != *reflex) {
                Segment extension(*reflex, target);
                if (face_intersects_segment(face, extension)) {
                    return true;
                }
            }
        }

        return false;
    }

    FT angular_capacity(Face_handle face) const {
        // Compute angular capacity of face
        // This is the angular range from which a reflex vertex can see the face
        auto reflex = get_adjacent_reflex_vertex(face);
        if (!reflex) {
            return FT(0); // No reflex vertex = no angular capacity
        }

        Point r = *reflex;

        // Get actual face vertices (not bbox corners)
        std::vector<Point> face_vertices;
        auto he_circ = face->outer_ccb();
        auto he_curr = he_circ;
        do {
            face_vertices.push_back(Point(he_curr->source()->point()));
            ++he_curr;
        } while (he_curr != he_circ);

        if (face_vertices.empty()) {
            return FT(0);
        }

        // Compute angular range from reflex vertex to all face vertices
        FT min_angle = FT(1e18), max_angle = FT(-1e18);
        for (const auto& v : face_vertices) {
            FT angle = compute_angle(r, v);
            min_angle = std::min(min_angle, angle);
            max_angle = std::max(max_angle, angle);
        }

        return max_angle - min_angle;
    }

    // Candidate/Witness sets (non-const to return non-const handles)
    CandidateSet get_candidates() {
        CandidateSet candidates;
        for (auto vit : all_vertices()) {
            candidates.vertex_candidates.insert(vit);
        }
        for (auto fit : all_faces()) {
            candidates.face_candidates.insert(fit);
        }
        return candidates;
    }

    WitnessSet get_witnesses() {
        WitnessSet witnesses;
        for (auto vit : all_vertices()) {
            witnesses.vertex_witnesses.insert(vit);
        }
        for (auto fit : all_faces()) {
            witnesses.face_witnesses.insert(fit);
        }
        return witnesses;
    }

    // Granularity management
    void set_granularity(FT lambda) {
        current_granularity_ = lambda;
    }

    void update_granularity() {
        current_granularity_ /= FT(2);
    }

    // Public face centroid computation using actual polygon centroid formula
    Point compute_face_centroid(Face_handle face) const {
        if (face->is_unbounded() || face->is_fictitious()) {
            return Point();  // Invalid face
        }

        // Collect all face vertices from outer CCB
        std::vector<Point> vertices;
        auto he_circ = face->outer_ccb();
        auto he_curr = he_circ;
        do {
            vertices.push_back(Point(he_curr->source()->point()));
            ++he_curr;
        } while (he_curr != he_circ);

        if (vertices.empty()) {
            return Point();
        }

        if (vertices.size() == 1) {
            return vertices[0];
        }

        if (vertices.size() == 2) {
            // Edge - return midpoint
            return Point((vertices[0].x() + vertices[1].x()) / FT(2),
                         (vertices[0].y() + vertices[1].y()) / FT(2));
        }

        // Compute actual centroid using shoelace formula
        FT area = FT(0);
        FT cx = FT(0), cy = FT(0);
        size_t n = vertices.size();

        for (size_t i = 0; i < n; ++i) {
            size_t j = (i + 1) % n;
            FT cross = vertices[i].x() * vertices[j].y() - vertices[j].x() * vertices[i].y();
            area += cross;
            cx += (vertices[i].x() + vertices[j].x()) * cross;
            cy += (vertices[i].y() + vertices[j].y()) * cross;
        }

        area /= FT(2);

        if (area == FT(0)) {
            // Degenerate face (collinear points), return vertex average
            FT avg_x = FT(0), avg_y = FT(0);
            for (const auto& v : vertices) {
                avg_x += v.x();
                avg_y += v.y();
            }
            return Point(avg_x / FT(n), avg_y / FT(n));
        }

        cx /= (FT(6) * area);
        cy /= (FT(6) * area);

        Point centroid(cx, cy);

        // Verify centroid is inside the face using point-in-polygon test
        if (point_in_face(centroid, face)) {
            return centroid;
        }

        // Fallback: use vertex average (may be outside for non-convex, but better than bbox)
        FT avg_x = FT(0), avg_y = FT(0);
        for (const auto& v : vertices) {
            avg_x += v.x();
            avg_y += v.y();
        }
        return Point(avg_x / FT(n), avg_y / FT(n));
    }

    Point compute_face_centroid(Face_const_handle face) const {
        if (face->is_unbounded() || face->is_fictitious()) {
            return Point();  // Invalid face
        }

        // Collect all face vertices from outer CCB
        std::vector<Point> vertices;
        auto he_circ = face->outer_ccb();
        auto he_curr = he_circ;
        do {
            vertices.push_back(Point(he_curr->source()->point()));
            ++he_curr;
        } while (he_curr != he_circ);

        if (vertices.empty()) {
            return Point();
        }

        if (vertices.size() == 1) {
            return vertices[0];
        }

        if (vertices.size() == 2) {
            return Point((vertices[0].x() + vertices[1].x()) / FT(2),
                         (vertices[0].y() + vertices[1].y()) / FT(2));
        }

        // Compute actual centroid using shoelace formula
        FT area = FT(0);
        FT cx = FT(0), cy = FT(0);
        size_t n = vertices.size();

        for (size_t i = 0; i < n; ++i) {
            size_t j = (i + 1) % n;
            FT cross = vertices[i].x() * vertices[j].y() - vertices[j].x() * vertices[i].y();
            area += cross;
            cx += (vertices[i].x() + vertices[j].x()) * cross;
            cy += (vertices[i].y() + vertices[j].y()) * cross;
        }

        area /= FT(2);

        if (area == FT(0)) {
            FT avg_x = FT(0), avg_y = FT(0);
            for (const auto& v : vertices) {
                avg_x += v.x();
                avg_y += v.y();
            }
            return Point(avg_x / FT(n), avg_y / FT(n));
        }

        cx /= (FT(6) * area);
        cy /= (FT(6) * area);

        Point centroid(cx, cy);

        if (point_in_face(centroid, face)) {
            return centroid;
        }

        FT avg_x = FT(0), avg_y = FT(0);
        for (const auto& v : vertices) {
            avg_x += v.x();
            avg_y += v.y();
        }
        return Point(avg_x / FT(n), avg_y / FT(n));
    }

    /**
     * @brief Convert a face to a polygon
     * @param face The face handle
     * @return Polygon representing the face boundary
     */
    Polygon face_to_polygon(Face_handle face) const {
        if (face->is_unbounded() || face->is_fictitious()) {
            return Polygon();  // Invalid face
        }

        std::vector<Point> vertices;
        auto he_circ = face->outer_ccb();
        auto he_curr = he_circ;
        do {
            vertices.push_back(Point(he_curr->source()->point()));
            ++he_curr;
        } while (he_curr != he_circ);

        return Polygon(vertices);
    }

    Polygon face_to_polygon(Face_const_handle face) const {
        if (face->is_unbounded() || face->is_fictitious()) {
            return Polygon();
        }

        std::vector<Point> vertices;
        auto he_circ = face->outer_ccb();
        auto he_curr = he_circ;
        do {
            vertices.push_back(Point(he_curr->source()->point()));
            ++he_curr;
        } while (he_curr != he_circ);

        return Polygon(vertices);
    }

    /**
     * @brief Count reflex vertices adjacent to a face
     * @param face The face handle
     * @return Number of adjacent reflex vertices
     */
    int count_adjacent_reflex_vertices(Face_handle face) const {
        int count = 0;
        auto first = face->outer_ccb();
        auto curr = first;
        do {
            auto he = curr;
            Point p(he->source()->point());

            for (size_t idx : polygon_.reflex_indices()) {
                if (polygon_.vertex(idx) == p) {
                    count++;
                    break;
                }
            }
            ++curr;
        } while (curr != first);

        return count;
    }

    int count_adjacent_reflex_vertices(Face_const_handle face) const {
        int count = 0;
        auto first = face->outer_ccb();
        auto curr = first;
        do {
            auto he = curr;
            Point p(he->source()->point());

            for (size_t idx : polygon_.reflex_indices()) {
                if (polygon_.vertex(idx) == p) {
                    count++;
                    break;
                }
            }
            ++curr;
        } while (curr != first);

        return count;
    }

private:
    CGAL_Arrangement_2 arrangement_;
    Polygon polygon_;
    FT current_granularity_;
    std::mt19937 rng_;

    void build_initial_arrangement() {
        // Insert polygon edges
        for (size_t i = 0; i < polygon_.num_vertices(); ++i) {
            insert_segment(polygon_.edge(i));
        }
    }

    bool point_in_face(const Point& p, Face_const_handle face) const {
        if (face->is_unbounded() || face->is_fictitious()) {
            return false;
        }

        // Check if point is inside the face
        // Use winding number or ray casting
        int winding = 0;
        auto first = face->outer_ccb();
        auto curr = first;
        do {
            auto he = curr;
            Point source(he->source()->point());
            Point target(he->target()->point());

            // Ray crossing test
            if ((source.y() <= p.y()) != (target.y() <= p.y())) {
                FT x_intersect = (target.x() - source.x()) * (p.y() - source.y()) /
                                 (target.y() - source.y()) + source.x();
                if (p.x() < x_intersect) {
                    winding++;
                }
            }
            ++curr;
        } while (curr != first);

        return winding % 2 == 1;
    }

    // Non-const version for backward compatibility
    bool point_in_face(const Point& p, Face_handle face) const {
        return point_in_face(p, Face_const_handle(face));
    }

    Point find_ray_intersection(const Point& source, FT dx, FT dy) const {
        // Check for zero direction vector
        if (dx == FT(0) && dy == FT(0)) {
            return source;  // Invalid ray direction
        }

        // Find intersection of ray from source in direction (dx, dy) with polygon boundary
        // Use a large finite value instead of infinity to avoid division by zero
        FT best_t = FT(1e18);
        bool found = false;

        for (size_t i = 0; i < polygon_.num_vertices(); ++i) {
            Segment edge = polygon_.edge(i);
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
                    found = true;
                }
            }
        }

        if (!found) {
            return source;  // No intersection found
        }

        return Point(source.x() + dx * best_t, source.y() + dy * best_t);
    }

    std::pair<Point, Point> compute_face_bbox(Face_handle face) const {
        // Initialize with first vertex
        auto first = face->outer_ccb();
        auto curr = first;
        auto he = curr;
        Point p(he->source()->point());
        FT xmin = p.x(), xmax = p.x();
        FT ymin = p.y(), ymax = p.y();
        ++curr;

        // Process remaining vertices
        while (curr != first) {
            auto he = curr;
            Point p(he->source()->point());
            xmin = std::min(xmin, p.x());
            xmax = std::max(xmax, p.x());
            ymin = std::min(ymin, p.y());
            ymax = std::max(ymax, p.y());
            ++curr;
        }

        return {Point(xmin, ymin), Point(xmax, ymax)};
    }

    // Const handle version of compute_face_bbox
    std::pair<Point, Point> compute_face_bbox(Face_const_handle face) const {
        FT xmin = FT(1e18), xmax = FT(-1e18);
        FT ymin = FT(1e18), ymax = FT(-1e18);

        auto hec = face->outer_ccb();
        auto curr = hec;
        do {
            auto vertex = curr->source();
            xmin = std::min(xmin, vertex->point().x());
            xmax = std::max(xmax, vertex->point().x());
            ymin = std::min(ymin, vertex->point().y());
            ymax = std::max(ymax, vertex->point().y());
            ++curr;
        } while (curr != hec);

        return {Point(xmin, ymin), Point(xmax, ymax)};
    }

    boost::optional<Point> get_adjacent_reflex_vertex(Face_handle face) const {
        // Check if any reflex vertex is adjacent to this face
        auto first = face->outer_ccb();
        auto curr = first;
        do {
            auto he = curr;
            Point p(he->source()->point());

            // Check if this is a reflex vertex
            for (size_t idx : polygon_.reflex_indices()) {
                if (polygon_.vertex(idx) == p) {
                    return p;
                }
            }
            ++curr;
        } while (curr != first);

        return boost::none;
    }

    boost::optional<size_t> get_reflex_index(const Point& p) const {
        for (size_t idx : polygon_.reflex_indices()) {
            if (polygon_.vertex(idx) == p) {
                return idx;
            }
        }
        return boost::none;
    }

    bool face_intersects_segment(Face_handle face, const Segment& seg) const {
        auto first = face->outer_ccb();
        auto curr = first;
        do {
            auto he = curr;
            Segment edge(Point(he->source()->point()), Point(he->target()->point()));

            if (seg.intersects(edge)) {
                return true;
            }
            ++curr;
        } while (curr != first);

        return false;
    }

    FT compute_angle(const Point& origin, const Point& target) const {
        FT dx = target.x() - origin.x();
        FT dy = target.y() - origin.y();
        // Return approximate angle (simplified)
        return FT(std::atan2(CGAL::to_double(dy), CGAL::to_double(dx)));
    }
};

// Common type aliases
using ArrangementE = Arrangement<CGAL::Exact_predicates_exact_constructions_kernel>;
using ArrangementD = Arrangement<CGAL::Simple_cartesian<double>>;

} // namespace geometry
} // namespace agp
