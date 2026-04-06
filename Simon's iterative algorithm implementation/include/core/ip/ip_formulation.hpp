#pragma once

#include "../geometry/arrangement.hpp"
#include "../geometry/visibility.hpp"
#include <boost/optional.hpp>
#include <map>
#include <set>
#include <vector>
#include <string>

namespace agp {
namespace ip {

/**
 * @brief IP Type enumeration
 */
enum class IPType {
    ONE_SHOT,
    NORMAL,
    BIG,
    SIMPLE
};

/**
 * @brief IP Variable representation
 */
struct IPVariable {
    std::string name;
    int index;
    bool is_vertex_candidate;
    bool is_face_candidate;
    bool is_face_witness;
    size_t original_id;  // ID in arrangement

    bool is_splittable;
};

/**
 * @brief IP Constraint representation
 */
struct IPConstraint {
    std::vector<int> variable_indices;
    std::vector<double> coefficients;
    double lower_bound;
    double upper_bound;
    std::string name;
};

/**
 * @brief IP Solution representation
 */
struct IPSolution {
    std::vector<size_t> vertex_guards;
    std::vector<size_t> face_guards;
    std::vector<size_t> unseen_witnesses;
    double objective_value;
    bool is_optimal;
    bool success;  // For backward compatibility

    std::vector<double> variable_values;
};

/**
 * @brief IP Formulation for Art Gallery Problem
 *
 * @tparam Kernel CGAL kernel type
 */
template <typename Kernel = CGAL::Exact_predicates_exact_constructions_kernel>
class IPFormulation {
public:
    using Arrangement = geometry::Arrangement<Kernel>;
    using Point = geometry::Point<Kernel>;
    using Polygon = geometry::Polygon<Kernel>;
    using FT = typename Kernel::FT;
    using Face_handle = typename Arrangement::Face_handle;
    using Vertex_handle = typename Arrangement::Vertex_handle;

    // Constructor
    IPFormulation(Arrangement& arrangement, IPType type = IPType::NORMAL)
        : arrangement_(arrangement), type_(type) {}

    // Build the IP formulation
    void build(
        const typename Arrangement::CandidateSet& candidates,
        const typename Arrangement::WitnessSet& witnesses,
        const std::set<typename Arrangement::Face_handle>& critical_witnesses = {}
    ) {
        // Calculate epsilon per paper formula
        epsilon_ = calculate_epsilon();

        // Build variables and constraints
        build_common();
    }

    // Variable access
    const std::vector<IPVariable>& variables() const { return variables_; }
    const std::vector<IPConstraint>& constraints() const { return constraints_; }

    // Objective function
    const std::vector<double>& objective_coefficients() const { return objective_; }
    bool is_minimization() const { return is_minimization_; }

    // Build specific IP types
    void build_one_shot(double epsilon = 0.0) {
        // Calculate epsilon per paper formula if not provided
        if (epsilon <= 0.0) {
            epsilon_ = calculate_epsilon();
        } else {
            epsilon_ = epsilon;
        }
        build_common();
    }

    void build_normal(double epsilon = 0.0) {
        if (epsilon <= 0.0) {
            epsilon_ = calculate_epsilon();
        } else {
            epsilon_ = epsilon;
        }
        build_common();
    }

    /**
     * @brief Build normal IP restricted to a critical witness set W*.
     *
     * Per paper Section 5: to avoid solving the full IP in every cycle,
     * we only add face-witness constraints for witnesses in W*.
     * Vertex-witness constraints are never filtered (they are cheap).
     *
     * @param epsilon           Epsilon for objective
     * @param critical_witnesses  Set of face-witness handles that must be covered
     */
    void build_normal_with_critical(
            double epsilon,
            const std::set<Face_handle>& critical_witnesses) {
        epsilon_ = (epsilon <= 0.0) ? calculate_epsilon() : epsilon;
        build_common_with_critical(critical_witnesses);
    }

    void build_big(int guard_count, double epsilon = 0.0) {
        // Calculate epsilon per paper formula if not provided
        if (epsilon <= 0.0) {
            epsilon_ = calculate_epsilon();
        } else {
            epsilon_ = epsilon;
        }
        build_common();
        guard_count_ = guard_count;

        // Add constraint for fixed number of guards
        IPConstraint guard_constraint;
        guard_constraint.name = "guard_count";
        guard_constraint.lower_bound = guard_count;
        guard_constraint.upper_bound = guard_count;

        for (const auto& var : variables_) {
            if (var.is_vertex_candidate || var.is_face_candidate) {
                guard_constraint.variable_indices.push_back(var.index);
                guard_constraint.coefficients.push_back(1.0);
            }
        }
        constraints_.push_back(guard_constraint);

        // Add splittable face-witness constraints per paper Section 4.4:
        //   1 - ε·Σ_{c ∈ VIS(w)} [c] ≥ [w]   ∀w ∈ splittable(face(W))
        // Rearranged: [w] + ε·Σ_{c ∈ VIS(w)} [c] ≤ 1
        // This ensures: if any candidate sees w, then [w]=0 is forced.
        auto all_verts_local = arrangement_.all_vertices();
        auto all_faces_local = arrangement_.all_faces();
        for (const auto& var : variables_) {
            if (var.is_face_witness && var.is_splittable) {
                auto fit = all_faces_local[var.original_id];
                if (fit->is_unbounded() || fit->is_fictitious()) continue;

                Polygon witness_face = arrangement_.face_to_polygon(fit);

                IPConstraint splittable_con;
                splittable_con.name = "splittable_" + var.name;
                splittable_con.lower_bound = 0.0;
                splittable_con.upper_bound = 1.0;  // [w] + ε·VIS ≤ 1

                // [w] coefficient (the witness variable)
                splittable_con.variable_indices.push_back(var.index);
                splittable_con.coefficients.push_back(1.0);

                // Only add candidates c ∈ VIS(w) (those that can see the witness face)
                for (const auto& cand_var : variables_) {
                    if (!cand_var.is_vertex_candidate && !cand_var.is_face_candidate) continue;

                    Point cand_pt;
                    if (cand_var.is_vertex_candidate && cand_var.original_id < all_verts_local.size()) {
                        cand_pt = Point(all_verts_local[cand_var.original_id]->point());
                    } else if (cand_var.is_face_candidate && cand_var.original_id < all_faces_local.size()) {
                        auto cf = all_faces_local[cand_var.original_id];
                        if (cf->is_unbounded() || cf->is_fictitious()) continue;
                        cand_pt = arrangement_.compute_face_centroid(cf);
                    } else {
                        continue;
                    }

                    // Only include if candidate actually sees the witness face
                    if (geometry::Visibility<Kernel>::sees_completely(
                            arrangement_.get_polygon(), cand_pt, witness_face)) {
                        splittable_con.variable_indices.push_back(cand_var.index);
                        splittable_con.coefficients.push_back(epsilon_);
                    }
                }
                constraints_.push_back(splittable_con);
            }
        }

        // Override objective for BIG IP: MAXIMIZE splittable face witnesses AND candidates
        // Per paper Section 4.4: f = Σ_{x ∈ splittable(W ∪ C)} [x]
        // This ensures the solution involves at least one splittable face.
        objective_.assign(variables_.size(), 0.0);
        for (const auto& var : variables_) {
            if (var.is_splittable &&
                (var.is_face_witness || var.is_face_candidate)) {
                objective_[var.index] = 1.0;
            }
        }
        is_minimization_ = false;  // BIG IP is maximization
    }

    void build_simple(double epsilon = 0.0) {
        // Calculate epsilon per paper formula if not provided
        if (epsilon <= 0.0) {
            epsilon_ = calculate_epsilon();
        } else {
            epsilon_ = epsilon;
        }
        build_common();
    }

    /**
     * @brief Build IP from pre-constructed raw variables, constraints, and objective.
     *
     * Allows callers (e.g. vision-stability check) to inject a fully custom
     * formulation without going through build_common().  After this call the
     * formulation is ready to be passed to IPSolver::solve().
     *
     * @param variables      The IP variables
     * @param constraints    The IP constraints
     * @param objective      Objective coefficient vector (same length as variables)
     * @param is_minimization true = minimize, false = maximize
     */
    void build_from_raw(std::vector<IPVariable> variables,
                        std::vector<IPConstraint> constraints,
                        std::vector<double> objective,
                        bool is_minimization = true) {
        variables_       = std::move(variables);
        constraints_     = std::move(constraints);
        objective_       = std::move(objective);
        is_minimization_ = is_minimization;
    }

    /**
     * @brief Build Stage 2 of the Two-Stage IP (paper Section 4.2).
     *
     * Stage 2: Fix the guard count s from Stage 1, then minimize the number
     * of face-guards and unseen face-witnesses:
     *   min  Σ_{c ∈ face(C)} [c]  +  Σ_{w ∈ face(W)} [w]
     *   s.t. Σ_{c ∈ C} [c] = s
     *        same vertex/face-witness visibility constraints
     *
     * @param guard_count  The optimal guard count s from Stage 1
     * @param epsilon      Epsilon parameter for the objective
     */
    void build_stage2(int guard_count, double epsilon = 0.0) {
        epsilon_ = (epsilon <= 0.0) ? calculate_epsilon() : epsilon;
        build_common();
        guard_count_ = guard_count;

        // Fix guard count constraint: Σ_{c ∈ C} [c] = s
        IPConstraint guard_con;
        guard_con.name = "guard_count_fixed";
        guard_con.lower_bound = guard_count;
        guard_con.upper_bound = guard_count;
        for (const auto& var : variables_) {
            if (var.is_vertex_candidate || var.is_face_candidate) {
                guard_con.variable_indices.push_back(var.index);
                guard_con.coefficients.push_back(1.0);
            }
        }
        constraints_.push_back(guard_con);

        // Override objective: minimize face-guards + unseen witnesses
        // min Σ_{c ∈ face(C)} [c] + Σ_{w ∈ face(W)} [w]
        objective_.assign(variables_.size(), 0.0);
        for (const auto& var : variables_) {
            if (var.is_face_candidate) {
                objective_[var.index] = 1.0;
            } else if (var.is_face_witness) {
                objective_[var.index] = 1.0;
            }
        }
        is_minimization_ = true;   // Stage 2 minimizes
    }

    // Solution interpretation
    IPSolution interpret_solution(const std::vector<double>& values) const {
        IPSolution solution;
        solution.variable_values = values;
        solution.is_optimal = true;
        solution.success = true;

        // Parse variables
        for (size_t i = 0; i < variables_.size() && i < values.size(); ++i) {
            const auto& var = variables_[i];

            if (values[i] > 0.5) {  // Variable selected (=1)
                if (var.is_vertex_candidate) {
                    solution.vertex_guards.push_back(var.original_id);
                } else if (var.is_face_candidate) {
                    solution.face_guards.push_back(var.original_id);
                } else if (var.is_face_witness) {
                    // [w]=1 means the face is UNSEEN (no guard covers it)
                    solution.unseen_witnesses.push_back(var.original_id);
                }
            }
            // [w]=0 (value <= 0.5) for face_witness means the face IS seen — do nothing
        }

        // Compute objective value
        solution.objective_value = 0;
        for (size_t i = 0; i < values.size() && i < objective_.size(); ++i) {
            solution.objective_value += objective_[i] * values[i];
        }

        return solution;
    }

    /**
     * @brief Calculate epsilon per paper formula
     *
     * ε = 1 / (|C| + |W| + 1)
     * where |C| is number of candidates and |W| is number of witnesses
     *
     * Can be called before or after build_common()
     */
    double calculate_epsilon() const {
        size_t num_candidates = 0;
        size_t num_witnesses = 0;

        // If variables already populated, use them
        if (!variables_.empty()) {
            for (const auto& var : variables_) {
                if (var.is_vertex_candidate || var.is_face_candidate) {
                    num_candidates++;
                }
                if (var.is_face_witness) {
                    num_witnesses++;
                }
            }
        } else {
            // Otherwise estimate from arrangement
            auto all_verts = arrangement_.all_vertices();
            auto all_faces = arrangement_.all_faces();

            // Vertex candidates
            num_candidates += all_verts.size();
            // Face candidates
            for (auto fit = all_faces.begin(); fit != all_faces.end(); ++fit) {
                if (!(*fit)->is_unbounded() && !(*fit)->is_fictitious()) {
                    num_candidates++;
                }
            }
            // Face witnesses
            for (auto fit = all_faces.begin(); fit != all_faces.end(); ++fit) {
                if (!(*fit)->is_unbounded() && !(*fit)->is_fictitious()) {
                    num_witnesses++;
                }
            }
        }

        return 1.0 / (num_candidates + num_witnesses + 1);
    }

    /**
     * @brief Get number of candidate variables
     */
    size_t num_candidates() const {
        size_t count = 0;
        for (const auto& var : variables_) {
            if (var.is_vertex_candidate || var.is_face_candidate) {
                count++;
            }
        }
        return count;
    }

    /**
     * @brief Get number of witness variables
     */
    size_t num_witnesses() const {
        size_t count = 0;
        for (const auto& var : variables_) {
            if (var.is_face_witness) {
                count++;
            }
        }
        return count;
    }

private:
    Arrangement& arrangement_;  // Non-const reference needed for querying
    IPType type_;
    std::vector<IPVariable> variables_;
    std::vector<IPConstraint> constraints_;
    std::vector<double> objective_;
    double epsilon_ = 0.0;
    int guard_count_ = 0;  // For BIG IP type
    bool is_minimization_ = true;  // Default is minimization

    // Common build steps
    void build_common() {
        // Add candidate variables
        int idx = 0;
        auto all_verts = arrangement_.all_vertices();
        for (size_t i = 0; i < all_verts.size(); ++i) {
            IPVariable var;
            var.name = "v_" + std::to_string(idx);
            var.index = idx;
            var.is_vertex_candidate = true;
            var.is_face_candidate = false;
            var.is_face_witness = false;
            var.original_id = i;
            variables_.push_back(var);
            idx++;
        }

        // Add face candidate variables
        auto all_faces = arrangement_.all_faces();
        for (size_t i = 0; i < all_faces.size(); ++i) {
            IPVariable var;
            var.name = "f_" + std::to_string(idx);
            var.index = idx;
            var.is_vertex_candidate = false;
            var.is_face_candidate = true;
            var.is_face_witness = false;
            var.original_id = i;
            var.is_splittable = arrangement_.is_splittable(all_faces[i], arrangement_.current_granularity());
            variables_.push_back(var);
            idx++;
        }

        // Add face witness variables
        for (size_t i = 0; i < all_faces.size(); ++i) {
            IPVariable var;
            var.name = "w_" + std::to_string(idx);
            var.index = idx;
            var.is_vertex_candidate = false;
            var.is_face_candidate = false;
            var.is_face_witness = true;
            var.original_id = i;
            var.is_splittable = arrangement_.is_splittable(all_faces[i], arrangement_.current_granularity());
            variables_.push_back(var);
            idx++;
        }

        // Build objective function
        objective_.resize(variables_.size(), 0.0);

        for (auto& var : variables_) {
            if (var.is_vertex_candidate) {
                objective_[var.index] = 1.0;
            } else if (var.is_face_candidate) {
                objective_[var.index] = 1.0 + epsilon_;
            } else if (var.is_face_witness) {
                objective_[var.index] = epsilon_;
            }
        }

        // Build vertex witness constraints
        // For each vertex witness w: [w] + sum_{c sees w} [c] >= 1
        // Only include witnesses that are inside the polygon
        for (size_t i = 0; i < all_verts.size(); ++i) {
            // Witness point
            Point witness_point(all_verts[i]->point());

            // Skip witnesses outside the polygon
            if (!arrangement_.get_polygon().contains(witness_point)) {
                continue;
            }

            IPConstraint constraint;
            constraint.name = "vertex_witness_" + std::to_string(i);

            // Find candidates that see this vertex
            for (const auto& var : variables_) {
                if (var.is_vertex_candidate) {
                    // Vertex candidate
                    Point candidate_point(all_verts[var.original_id]->point());
                    if (geometry::Visibility<Kernel>::is_visible(arrangement_.get_polygon(),
                                                       candidate_point, witness_point)) {
                        constraint.variable_indices.push_back(var.index);
                        constraint.coefficients.push_back(1.0);
                    }
                } else if (var.is_face_candidate) {
                    // Face candidate - use face centroid
                    auto fit = all_faces[var.original_id];
                    if (!fit->is_unbounded() && !fit->is_fictitious()) {
                        Point candidate_point = arrangement_.compute_face_centroid(fit);
                        if (geometry::Visibility<Kernel>::is_visible(arrangement_.get_polygon(),
                                                           candidate_point, witness_point)) {
                            constraint.variable_indices.push_back(var.index);
                            constraint.coefficients.push_back(1.0);
                        }
                    }
                }
            }

            constraint.lower_bound = 1.0;
            constraint.upper_bound = std::numeric_limits<double>::infinity();
            constraints_.push_back(constraint);
        }

        // Build face witness constraints
        // For each face witness w: [w] + sum_{c sees w} [c] >= 1
        for (size_t i = 0; i < all_faces.size(); ++i) {
            auto fit = all_faces[i];
            if (fit->is_unbounded() || fit->is_fictitious()) continue;

            IPConstraint constraint;
            constraint.name = "face_witness_" + std::to_string(i);

            // Add the witness variable itself with coefficient 1
            auto witness_var = std::find_if(variables_.begin(), variables_.end(),
                [i](const IPVariable& v) {
                    return v.is_face_witness && v.original_id == i;
                });
            if (witness_var != variables_.end()) {
                constraint.variable_indices.push_back(witness_var->index);
                constraint.coefficients.push_back(1.0);
            }

            // Get the face as a polygon for complete-sees checking
            Polygon witness_face = arrangement_.face_to_polygon(fit);
            Point witness_point = arrangement_.compute_face_centroid(fit);

            // Find candidates that see this face witness COMPLETELY
            for (const auto& var : variables_) {
                if (var.is_vertex_candidate) {
                    // Vertex candidate - check if it sees entire face
                    Point candidate_point(all_verts[var.original_id]->point());
                    if (geometry::Visibility<Kernel>::sees_completely(
                            arrangement_.get_polygon(), candidate_point, witness_face)) {
                        constraint.variable_indices.push_back(var.index);
                        constraint.coefficients.push_back(1.0);
                    }
                } else if (var.is_face_candidate && var.original_id != i) {
                    // Face candidate (different from this witness face)
                    auto cand_fit = all_faces[var.original_id];
                    if (!cand_fit->is_unbounded() && !cand_fit->is_fictitious()) {
                        // For face candidates, check if centroid sees the entire witness face
                        Point candidate_point = arrangement_.compute_face_centroid(cand_fit);
                        if (geometry::Visibility<Kernel>::sees_completely(
                                arrangement_.get_polygon(), candidate_point, witness_face)) {
                            constraint.variable_indices.push_back(var.index);
                            constraint.coefficients.push_back(1.0);
                        }
                    }
                }
            }

            constraint.lower_bound = 1.0;
            constraint.upper_bound = std::numeric_limits<double>::infinity();
            constraints_.push_back(constraint);
        }
    }

    /**
     * @brief Build IP with face-witness constraints restricted to critical_witnesses.
     *
     * Variables and the objective are built for ALL candidates and witnesses
     * (so the solution space is complete), but face-witness constraints
     * are only emitted for the faces in critical_witnesses.  Vertex-witness
     * constraints are always emitted.
     */
    void build_common_with_critical(const std::set<Face_handle>& critical_witnesses) {
        // --- Variables (same as build_common) ---
        variables_.clear();
        constraints_.clear();
        objective_.clear();

        int idx = 0;
        auto all_verts = arrangement_.all_vertices();
        auto all_faces = arrangement_.all_faces();

        for (size_t i = 0; i < all_verts.size(); ++i) {
            IPVariable var;
            var.name = "v_" + std::to_string(idx);
            var.index = idx;
            var.is_vertex_candidate = true;
            var.is_face_candidate   = false;
            var.is_face_witness     = false;
            var.original_id         = i;
            var.is_splittable       = false;
            variables_.push_back(var);
            idx++;
        }
        for (size_t i = 0; i < all_faces.size(); ++i) {
            IPVariable var;
            var.name = "f_" + std::to_string(idx);
            var.index = idx;
            var.is_vertex_candidate = false;
            var.is_face_candidate   = true;
            var.is_face_witness     = false;
            var.original_id         = i;
            var.is_splittable = arrangement_.is_splittable(all_faces[i], arrangement_.current_granularity());
            variables_.push_back(var);
            idx++;
        }
        for (size_t i = 0; i < all_faces.size(); ++i) {
            IPVariable var;
            var.name = "w_" + std::to_string(idx);
            var.index = idx;
            var.is_vertex_candidate = false;
            var.is_face_candidate   = false;
            var.is_face_witness     = true;
            var.original_id         = i;
            var.is_splittable = arrangement_.is_splittable(all_faces[i], arrangement_.current_granularity());
            variables_.push_back(var);
            idx++;
        }

        // --- Objective ---
        objective_.resize(variables_.size(), 0.0);
        for (auto& var : variables_) {
            if (var.is_vertex_candidate)        objective_[var.index] = 1.0;
            else if (var.is_face_candidate)     objective_[var.index] = 1.0 + epsilon_;
            else if (var.is_face_witness)       objective_[var.index] = epsilon_;
        }

        // --- Vertex-witness constraints (always all vertices inside polygon) ---
        for (size_t i = 0; i < all_verts.size(); ++i) {
            Point wp(all_verts[i]->point());

            // Skip witnesses outside the polygon
            if (!arrangement_.get_polygon().contains(wp)) {
                continue;
            }

            IPConstraint con;
            con.name = "vertex_witness_" + std::to_string(i);
            for (const auto& var : variables_) {
                if (var.is_vertex_candidate) {
                    Point cp(all_verts[var.original_id]->point());
                    if (geometry::Visibility<Kernel>::is_visible(arrangement_.get_polygon(), cp, wp)) {
                        con.variable_indices.push_back(var.index);
                        con.coefficients.push_back(1.0);
                    }
                } else if (var.is_face_candidate) {
                    auto fit = all_faces[var.original_id];
                    if (!fit->is_unbounded() && !fit->is_fictitious()) {
                        Point cp = arrangement_.compute_face_centroid(fit);
                        if (geometry::Visibility<Kernel>::is_visible(arrangement_.get_polygon(), cp, wp)) {
                            con.variable_indices.push_back(var.index);
                            con.coefficients.push_back(1.0);
                        }
                    }
                }
            }
            con.lower_bound = 1.0;
            con.upper_bound = std::numeric_limits<double>::infinity();
            constraints_.push_back(con);
        }

        // --- Face-witness constraints (only for critical witnesses W*) ---
        for (size_t i = 0; i < all_faces.size(); ++i) {
            auto fit = all_faces[i];
            if (fit->is_unbounded() || fit->is_fictitious()) continue;

            // Skip if not in critical witness set
            if (!critical_witnesses.empty() && critical_witnesses.count(fit) == 0) continue;

            IPConstraint con;
            con.name = "face_witness_" + std::to_string(i);

            // Add [w] variable
            auto wvar = std::find_if(variables_.begin(), variables_.end(),
                [i](const IPVariable& v) { return v.is_face_witness && v.original_id == i; });
            if (wvar != variables_.end()) {
                con.variable_indices.push_back(wvar->index);
                con.coefficients.push_back(1.0);
            }

            Polygon wface = arrangement_.face_to_polygon(fit);
            for (const auto& var : variables_) {
                if (var.is_vertex_candidate) {
                    Point cp(all_verts[var.original_id]->point());
                    if (geometry::Visibility<Kernel>::sees_completely(
                            arrangement_.get_polygon(), cp, wface)) {
                        con.variable_indices.push_back(var.index);
                        con.coefficients.push_back(1.0);
                    }
                } else if (var.is_face_candidate && var.original_id != i) {
                    auto cf = all_faces[var.original_id];
                    if (!cf->is_unbounded() && !cf->is_fictitious()) {
                        Point cp = arrangement_.compute_face_centroid(cf);
                        if (geometry::Visibility<Kernel>::sees_completely(
                                arrangement_.get_polygon(), cp, wface)) {
                            con.variable_indices.push_back(var.index);
                            con.coefficients.push_back(1.0);
                        }
                    }
                }
            }
            con.lower_bound = 1.0;
            con.upper_bound = std::numeric_limits<double>::infinity();
            constraints_.push_back(con);
        }
    }
};

} // namespace ip
} // namespace agp
