#pragma once

#include "polygon.hpp"
#include "visibility.hpp"
#include <CGAL/Boolean_set_operations_2.h>
#include <memory>
#include <vector>
#include <optional>
#include <list>
#include <random>

namespace agp {
namespace geometry {

/**
 * @brief Weak Visibility Polygon Tree (WVPT)
 *
 * A tree structure that decomposes the polygon hierarchically using
 * weak visibility polygons. Each node stores the WVP from its defining edge/chord,
 * and children are built recursively in the complement region (Section 2.2 of the paper).
 *
 * @tparam Kernel CGAL kernel type
 */
template <typename Kernel = CGAL::Exact_predicates_exact_constructions_kernel>
class WVPT {
public:
    using Point = Point<Kernel>;
    using Polygon = Polygon<Kernel>;
    using Segment = Segment<Kernel>;
    using FT = typename Kernel::FT;
    using Polygon_2 = CGAL::Polygon_2<Kernel>;
    using PWH = CGAL::Polygon_with_holes_2<Kernel>;

    /**
     * @brief Node in the WVPT
     *
     * Each node represents a "pocket" — a region of P visible from its defining chord.
     * The defining_chord is either a polygon edge (root) or an interior chord (children).
     */
    struct Node {
        Polygon weak_visibility_polygon; ///< WVP from the defining chord
        Segment defining_chord;          ///< The chord (edge or diagonal) that defines this node
        Node* parent;
        std::vector<std::unique_ptr<Node>> children;

        // Statistics
        size_t num_non_reflex_vertices = 0;
        size_t num_reflex_vertices = 0;

        Node() : parent(nullptr) {}
    };

    // Constructor
    explicit WVPT(const Polygon& P) : polygon_(P) {
        if (P.num_vertices() >= 3) {
            build_tree();
        }
    }

    // Accessors
    Node* root() { return root_.get(); }
    const Node* root() const { return root_.get(); }

    size_t size() const {
        return count_nodes_recursive(root_.get());
    }

    /**
     * @brief Collect all defining chords from all nodes in the tree.
     *
     * These chords are used to initialize the arrangement per the paper's
     * iterative algorithm initialization (Section 3 of the paper).
     */
    std::vector<Segment> get_defining_chords() const {
        std::vector<Segment> chords;
        collect_chords_recursive(root_.get(), chords);
        return chords;
    }

    // Node queries
    const Node* find_node(const Point& p) const {
        if (!root_) return nullptr;
        return find_node_recursive(p, root_.get());
    }

    Node* find_node(const Point& p) {
        if (!root_) return nullptr;
        return find_node_recursive(p, root_.get());
    }

    std::vector<Node*> siblings(Node* n) const {
        std::vector<Node*> result;
        if (n && n->parent) {
            for (auto& child : n->parent->children) {
                if (child.get() != n) {
                    result.push_back(child.get());
                }
            }
        }
        return result;
    }

    std::vector<Node*> children_of(Node* n) const {
        if (!n) return {};
        std::vector<Node*> result;
        for (auto& child : n->children) {
            result.push_back(child.get());
        }
        return result;
    }

    Node* parent_of(Node* n) const {
        return n ? n->parent : nullptr;
    }

    /**
     * @brief Get nodes relevant for visibility queries from point p.
     *
     * Per Lemma 10: visibility from p only depends on cells in the same subtree.
     */
    std::vector<Node*> relevant_nodes_for_visibility(const Point& p) {
        std::vector<Node*> relevant;
        Node* node = find_node(p);
        if (node) {
            if (node->parent) {
                relevant.push_back(node->parent);
            }
            for (auto& child : node->children) {
                relevant.push_back(child.get());
            }
        }
        return relevant;
    }

    // Statistics
    size_t max_weak_visibility_polygon_size() const {
        size_t max_size = 0;
        compute_max_size_recursive(root_.get(), max_size);
        return max_size;
    }

    size_t max_reflex_vertices_in_node() const {
        size_t max_reflex = 0;
        compute_max_reflex_recursive(root_.get(), max_reflex);
        return max_reflex;
    }

    size_t total_nodes() const {
        return count_nodes_recursive(root_.get());
    }

private:
    std::unique_ptr<Node> root_;
    Polygon polygon_;

    void build_tree() {
        // Use the first polygon edge as the starting chord (root)
        if (polygon_.num_vertices() < 2) return;

        Segment start_edge = polygon_.edge(0);

        // Compute the weak visibility polygon from the first edge
        Polygon wvp = Visibility<Kernel>::compute_weak_visibility_polygon(
            polygon_, start_edge.source(), start_edge.target());

        root_ = std::make_unique<Node>();
        root_->weak_visibility_polygon = wvp;
        root_->defining_chord = start_edge;
        root_->parent = nullptr;
        root_->num_reflex_vertices = wvp.num_reflex_vertices();
        root_->num_non_reflex_vertices = wvp.num_vertices() - wvp.num_reflex_vertices();

        // Build children by decomposing the complement of this WVP
        build_children_recursive(root_.get(), polygon_, 0);
    }

    /**
     * @brief Recursively build children of a node.
     *
     * Per the paper: for each chord (non-boundary edge of the WVP), compute
     * the WVP in the complement region P \ WVP(parent) on the other side of that chord.
     *
     * @param parent      Current node
     * @param remaining   The polygon region not yet decomposed (initially = P)
     * @param depth       Current recursion depth (for cycle prevention)
     */
    void build_children_recursive(Node* parent, const Polygon& remaining, int depth) {
        if (depth > 50) return; // Safety guard against infinite recursion

        const Polygon& wvp = parent->weak_visibility_polygon;
        if (wvp.num_vertices() < 3) return;

        // Validate polygons before CGAL boolean operations
        if (remaining.num_vertices() < 3) return;

        // Compute the complement: remaining \ WVP(parent)
        // Use CGAL difference to get the remaining undecomposed region
        Polygon_2 remaining_cgal = remaining.cgal_polygon();
        Polygon_2 wvp_cgal = wvp.cgal_polygon();

        // Validate both polygons are simple
        if (!remaining_cgal.is_simple() || !wvp_cgal.is_simple()) {
            return;  // Skip invalid geometry
        }

        if (remaining_cgal.orientation() == CGAL::CLOCKWISE)
            remaining_cgal.reverse_orientation();
        if (wvp_cgal.orientation() == CGAL::CLOCKWISE)
            wvp_cgal.reverse_orientation();

        std::list<PWH> complement_list;
        try {
            CGAL::difference(remaining_cgal, wvp_cgal, std::back_inserter(complement_list));
        } catch (const std::exception& e) {
            // Silently skip this branch on CGAL errors - don't crash
            return;
        }

        // Each connected component of the complement is a pocket
        // Check which edges of the WVP are actual chords (not boundary of remaining polygon)
        auto wvp_edges = wvp.edges();
        for (const auto& edge : wvp_edges) {
            // A chord is an edge of the WVP whose interior is not on the boundary of 'remaining'
            Point mid = edge.midpoint();
            bool on_boundary = remaining.is_on_boundary(mid);
            if (on_boundary) continue; // Boundary edge, skip

            // Determine which complement piece this chord borders
            for (const auto& complement_pwh : complement_list) {
                Polygon complement_poly(complement_pwh.outer_boundary());
                if (complement_poly.num_vertices() < 3) continue;

                // Check if edge midpoint is on the boundary of this complement piece
                // or if edge belongs to this complement region
                if (complement_poly.is_on_boundary(mid) ||
                    complement_poly.contains(mid)) {

                    // Compute WVP from this chord within the complement region
                    Polygon child_wvp = Visibility<Kernel>::compute_weak_visibility_polygon(
                        complement_poly, edge.source(), edge.target());

                    if (child_wvp.num_vertices() < 3) continue;

                    auto child = std::make_unique<Node>();
                    child->weak_visibility_polygon = child_wvp;
                    child->defining_chord = edge;
                    child->parent = parent;
                    child->num_reflex_vertices = child_wvp.num_reflex_vertices();
                    child->num_non_reflex_vertices =
                        child_wvp.num_vertices() - child_wvp.num_reflex_vertices();

                    // Recurse into this complement piece
                    build_children_recursive(child.get(), complement_poly, depth + 1);

                    parent->children.push_back(std::move(child));
                    break; // Each chord borders at most one complement piece
                }
            }
        }
    }

    void collect_chords_recursive(const Node* node, std::vector<Segment>& chords) const {
        if (!node) return;
        // Add this node's defining chord (skip root if it's a boundary edge)
        chords.push_back(node->defining_chord);
        for (const auto& child : node->children) {
            collect_chords_recursive(child.get(), chords);
        }
    }

    void compute_max_size_recursive(const Node* node, size_t& max_size) const {
        if (!node) return;
        max_size = std::max(max_size, node->weak_visibility_polygon.num_vertices());
        for (const auto& child : node->children) {
            compute_max_size_recursive(child.get(), max_size);
        }
    }

    void compute_max_reflex_recursive(const Node* node, size_t& max_reflex) const {
        if (!node) return;
        max_reflex = std::max(max_reflex, node->num_reflex_vertices);
        for (const auto& child : node->children) {
            compute_max_reflex_recursive(child.get(), max_reflex);
        }
    }

    size_t count_nodes_recursive(const Node* node) const {
        if (!node) return 0;
        size_t count = 1;
        for (const auto& child : node->children) {
            count += count_nodes_recursive(child.get());
        }
        return count;
    }

    // Find node containing point (non-const)
    Node* find_node_recursive(const Point& p, Node* current) {
        if (!current) return nullptr;
        if (current->weak_visibility_polygon.contains(p)) {
            return current;
        }
        for (auto& child : current->children) {
            Node* found = find_node_recursive(p, child.get());
            if (found) return found;
        }
        return nullptr;
    }

    // Find node containing point (const)
    const Node* find_node_recursive(const Point& p, const Node* current) const {
        if (!current) return nullptr;
        if (current->weak_visibility_polygon.contains(p)) {
            return current;
        }
        for (const auto& child : current->children) {
            const Node* found = find_node_recursive(p, child.get());
            if (found) return found;
        }
        return nullptr;
    }
};

// Common type aliases
using WVPTE = WVPT<CGAL::Exact_predicates_exact_constructions_kernel>;
using WVPTD = WVPT<CGAL::Simple_cartesian<double>>;

} // namespace geometry
} // namespace agp
