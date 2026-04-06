#pragma once

#include "../geometry/arrangement.hpp"
#include "../geometry/visibility.hpp"
#include <random>
#include <vector>

namespace agp {
namespace algorithm {

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
 * @brief Split protocol configuration
 */
struct SplitProtocol {
    double angular_probability = 0.6;
    double visibility_line_probability = 0.2;
    double chord_probability = 0.1;
    double extension_probability = 0.1;
    bool use_square_only = false;
    bool use_square_for_multiple_reflex = true;

    // Default normal protocol
    static SplitProtocol normal() {
        return SplitProtocol{};
    }

    // Square-only protocol
    static SplitProtocol square_only() {
        SplitProtocol sp;
        sp.use_square_only = true;
        return sp;
    }
};

/**
 * @brief Face splitter class
 *
 * Handles splitting of faces in the arrangement using various protocols.
 *
 * @tparam Kernel CGAL kernel type
 */
template <typename Kernel = CGAL::Exact_predicates_exact_constructions_kernel>
class Splitter {
public:
    using Arrangement = geometry::Arrangement<Kernel>;
    using Point = geometry::Point<Kernel>;
    using Polygon = geometry::Polygon<Kernel>;
    using FT = typename Kernel::FT;
    using Face_handle = typename Arrangement::Face_handle;

    /**
     * @brief Constructor
     */
    Splitter(Arrangement& arrangement, const SplitProtocol& protocol = SplitProtocol::normal())
        : arrangement_(arrangement), protocol_(protocol), granularity_(FT(1) / FT(16)), k_(4) {
        rng_.seed(std::random_device{}());
    }

    /**
     * @brief Choose split type based on protocol
     */
    SplitType choose_split_type(Face_handle face) const {
        // Check if face has multiple reflex vertices
        int reflex_count = count_adjacent_reflex_vertices(face);

        if (reflex_count > 1 && protocol_.use_square_for_multiple_reflex) {
            return SplitType::SQUARE;
        }

        if (protocol_.use_square_only) {
            return SplitType::SQUARE;
        }

        // Random selection based on probabilities
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double r = dist(const_cast<std::mt19937&>(rng_));

        if (r < protocol_.angular_probability) {
            return SplitType::ANGULAR;
        } else if (r < protocol_.angular_probability + protocol_.visibility_line_probability) {
            return SplitType::VISIBILITY_LINE;
        } else if (r < protocol_.angular_probability + protocol_.visibility_line_probability + protocol_.chord_probability) {
            return SplitType::REFLEX_CHORD;
        } else {
            return SplitType::EXTENSION;
        }
    }

    /**
     * @brief Perform split on face
     */
    bool split(Face_handle face) {
        SplitType type = choose_split_type(face);
        return split_with_type(face, type);
    }

    /**
     * @brief Split with specific type
     */
    bool split_with_type(Face_handle face, SplitType type) {
        // Try the requested split type first
        switch (type) {
            case SplitType::SQUARE:
                if (try_square_split(face)) return true;
                break;
            case SplitType::ANGULAR:
                if (try_angular_split(face)) return true;
                break;
            case SplitType::REFLEX_CHORD:
                if (try_reflex_chord_split(face)) return true;
                break;
            case SplitType::EXTENSION:
                if (try_extension_split(face)) return true;
                break;
            case SplitType::VISIBILITY_LINE:
                if (try_visibility_line_split(face)) return true;
                break;
        }

        // Try other split types if primary fails
        if (type != SplitType::SQUARE && try_square_split(face)) return true;
        if (type != SplitType::ANGULAR && try_angular_split(face)) return true;
        if (type != SplitType::REFLEX_CHORD && try_reflex_chord_split(face)) return true;
        if (type != SplitType::EXTENSION && try_extension_split(face)) return true;
        if (type != SplitType::VISIBILITY_LINE && try_visibility_line_split(face)) return true;

        return false;
    }

    // Granularity management
    FT current_granularity() const { return granularity_; }
    int current_k() const { return k_; }

    void update_granularity() {
        granularity_ /= FT(2);
        k_++;
    }

    void set_granularity(FT lambda) {
        granularity_ = lambda;
        k_ = static_cast<int>(std::round(-std::log2(CGAL::to_double(lambda))));
    }

    /**
     * @brief Check if there are splittable faces
     */
    bool has_splittable_face(const std::vector<Face_handle>& faces) const {
        for (Face_handle f : faces) {
            if (arrangement_.is_splittable(f, granularity_)) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Get all splittable faces
     */
    std::vector<Face_handle> get_splittable_faces(const std::vector<Face_handle>& faces) const {
        std::vector<Face_handle> result;
        for (Face_handle f : faces) {
            if (arrangement_.is_splittable(f, granularity_)) {
                result.push_back(f);
            }
        }
        return result;
    }

private:
    Arrangement& arrangement_;
    SplitProtocol protocol_;
    FT granularity_;
    int k_;
    mutable std::mt19937 rng_;

    // Individual split implementations
    bool try_square_split(Face_handle face) {
        auto result = arrangement_.square_split(face);
        return result.success;
    }

    bool try_angular_split(Face_handle face) {
        auto result = arrangement_.angular_split(face, granularity_);
        return result.success;
    }

    bool try_reflex_chord_split(Face_handle face) {
        auto result = arrangement_.reflex_chord_split(face);
        return result.success;
    }

    bool try_extension_split(Face_handle face) {
        auto result = arrangement_.extension_split(face);
        return result.success;
    }

    bool try_visibility_line_split(Face_handle face) {
        auto result = arrangement_.visibility_line_split(face, {}, {});
        return result.success;
    }

    int count_adjacent_reflex_vertices(Face_handle face) const {
        return arrangement_.count_adjacent_reflex_vertices(face);
    }
};

// Common type aliases
using SplitterE = Splitter<CGAL::Exact_predicates_exact_constructions_kernel>;
using SplitterD = Splitter<CGAL::Simple_cartesian<double>>;

} // namespace algorithm
} // namespace agp
