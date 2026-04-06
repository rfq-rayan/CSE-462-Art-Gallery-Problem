#pragma once

#include "../geometry/polygon.hpp"
#include "../geometry/arrangement.hpp"
#include "../geometry/wvpt.hpp"
#include "../geometry/visibility.hpp"
#include "../ip/ip_formulation.hpp"
#include "../ip/ip_solver.hpp"
#include "../utils/step_recorder.hpp"
#include "splitter.hpp"
#include "verifier.hpp"
#include <functional>
#include <memory>
#include <chrono>
#include <vector>

namespace agp {
namespace algorithm {

/**
 * @brief IP Protocol enumeration
 */
enum class IPProtocol {
    NORMAL,      // Normal IP + Big IP
    SIMPLE       // Only Normal IP
};

/**
 * @brief Iterative algorithm configuration
 */
struct IterativeAlgorithmConfig {
    // Split protocol
    SplitProtocol split_protocol;

    // IP protocol
    IPProtocol ip_protocol = IPProtocol::NORMAL;

    // Critical witnesses
    bool use_critical_witnesses = true;
    double initial_critical_fraction = 0.1;

    // Granularity
    double initial_granularity = 1.0 / 16.0;

    // Termination
    int max_iterations = 10000;
    double time_limit_seconds = 3600.0;

    // Solver
    ip::SolverType solver_type = ip::SolverType::GLPK;

    // Verbosity
    int verbosity = 1;

    // Step recording output file (empty = disabled)
    std::string step_output_file;

    // Default configuration
    static IterativeAlgorithmConfig default_config() {
        return IterativeAlgorithmConfig{};
    }

    // Fast configuration (simpler IP, no critical witnesses)
    static IterativeAlgorithmConfig fast() {
        IterativeAlgorithmConfig config;
        config.ip_protocol = IPProtocol::SIMPLE;
        config.use_critical_witnesses = false;
        return config;
    }

    // Thorough configuration (uses all features)
    static IterativeAlgorithmConfig thorough() {
        IterativeAlgorithmConfig config;
        config.ip_protocol = IPProtocol::NORMAL;
        config.use_critical_witnesses = true;
        config.max_iterations = 50000;
        return config;
    }
};

/**
 * @brief Iterative Algorithm for Art Gallery Problem
 *
 * Main implementation of the iterative algorithm from the paper.
 *
 * @tparam Kernel CGAL kernel type
 */
template <typename Kernel = CGAL::Exact_predicates_exact_constructions_kernel>
class IterativeAlgorithm {
public:
    using Polygon = geometry::Polygon<Kernel>;
    using Point = geometry::Point<Kernel>;
    using Arrangement = geometry::Arrangement<Kernel>;
    using WVPT = geometry::WVPT<Kernel>;
    using FT = typename Kernel::FT;

    /**
     * @brief Iterative algorithm result
     */
    struct Result {
        std::vector<Point> guards;
        int num_iterations;
        double total_time;
        double ip_time;
        double visibility_time;
        double split_time;
        bool optimal_found;
        std::string status_message;

        // Statistics
        int final_granularity_k;
        size_t final_num_candidates;
        size_t final_num_witnesses;
        size_t num_splits;
        size_t num_granularity_updates;

        // Verification
        double coverage_percentage;
        bool verified;
    };

    // Legacy typedef for backward compatibility
    using IterativeAlgorithmResult = Result;

    explicit IterativeAlgorithm(const IterativeAlgorithmConfig& config = IterativeAlgorithmConfig::default_config())
        : config_(config) {}

    /**
     * @brief Main solve function
     */
    Result solve(const Polygon& polygon) {
        Result result;
        result.optimal_found = false;
        result.num_iterations = 0;
        result.num_splits = 0;
        result.num_granularity_updates = 0;
        result.ip_time = 0;
        result.visibility_time = 0;
        result.split_time = 0;

        auto start_time = std::chrono::high_resolution_clock::now();

        try {
            // Initialize
            initialize(polygon);

            // Main loop
            bool converged = false;
            while (!converged && result.num_iterations < config_.max_iterations) {
                // Check time limit
                auto current_time = std::chrono::high_resolution_clock::now();
                double elapsed = std::chrono::duration<double>(current_time - start_time).count();
                if (elapsed > config_.time_limit_seconds) {
                    result.status_message = "Time limit exceeded";
                    break;
                }

                // Run iteration
                auto iter_result = run_iteration(result);

                result.num_iterations++;

                if (iter_result == IterationResult::OPTIMAL_FOUND) {
                    result.optimal_found = true;
                    converged = true;
                    result.status_message = "Optimal solution found";
                } else if (iter_result == IterationResult::NO_PROGRESS) {
                    result.status_message = "No progress - polygon may not be vision-stable";
                    break;
                }

                // Progress callback
                if (progress_callback_) {
                    progress_callback_(result.num_iterations, result);
                }
            }

            // Extract final guards
            result.guards = extract_guards();

            // Final statistics
            result.final_granularity_k = splitter_->current_k();
            result.final_num_candidates = arrangement_->get_candidates().size();
            result.final_num_witnesses = arrangement_->get_witnesses().size();

            // Verify solution
            if (!result.guards.empty()) {
                result.coverage_percentage = Verifier<Kernel>::coverage_percentage(polygon, result.guards);
                result.verified = result.coverage_percentage > 99.9;
            }

        } catch (const std::exception& e) {
            result.status_message = std::string("Error: ") + e.what();
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        result.total_time = std::chrono::duration<double>(end_time - start_time).count();

        return result;
    }

    /**
     * @brief Set progress callback
     */
    using ProgressCallback = std::function<void(int iteration, const IterativeAlgorithmResult& current)>;
    void set_progress_callback(ProgressCallback callback) {
        progress_callback_ = callback;
    }

private:
    IterativeAlgorithmConfig config_;
    ProgressCallback progress_callback_;

    // Algorithm state
    std::unique_ptr<WVPT> wvpt_;
    std::unique_ptr<Arrangement> arrangement_;
    std::unique_ptr<Splitter<Kernel>> splitter_;
    std::unique_ptr<ip::IPSolver> solver_;

    // Critical witnesses
    std::set<typename Arrangement::Face_handle> critical_witnesses_;

    // Current solution
    ip::IPSolution current_solution_;

    // Step recorder for visualization
    std::unique_ptr<utils::StepRecorder> step_recorder_;

    /**
     * @brief Record initialization step
     */
    void recordInitialization();

    /**
     * @brief Record IP solve step
     */
    void recordIPSolve(int stage, const ip::IPSolution& solution);

    /**
     * @brief Record split operation
     */
    void recordSplit(int face_id, const std::string& split_type);

    /**
     * @brief Record granularity update
     */
    void recordGranularityUpdate(int new_k);

    /**
     * @brief Record termination step
     */
    void recordTermination(const std::vector<Point>& final_guards);

    /**
     * @brief Iteration result enumeration
     */
    enum class IterationResult {
        OPTIMAL_FOUND,
        SPLIT_PERFORMED,
        GRANULARITY_UPDATED,
        CRITICAL_WITNESSES_ADDED,
        NO_PROGRESS
    };

    /**
     * @brief Initialize algorithm state
     */
    void initialize(const Polygon& polygon) {
        // Build WVPT (needed for chord initialization and critical witnesses)
        wvpt_ = std::make_unique<WVPT>(polygon);

        // Build initial arrangement from polygon edges
        arrangement_ = std::make_unique<Arrangement>(polygon);

        // Initialize arrangement with WVPT defining chords per paper Section 3:
        // "We start by adding all chords of the WVPT to the arrangement"
        if (wvpt_->root()) {
            auto chords = wvpt_->get_defining_chords();
            arrangement_->initialize_from_wvpt(chords);
        }

        // Initialize splitter
        splitter_ = std::make_unique<Splitter<Kernel>>(*arrangement_, config_.split_protocol);
        splitter_->set_granularity(FT(config_.initial_granularity));

        // Initialize solver
        solver_ = std::make_unique<ip::IPSolver>(config_.solver_type);
        solver_->set_time_limit(config_.time_limit_seconds / 10);

        // Initialize critical witnesses
        if (config_.use_critical_witnesses) {
            initialize_critical_witnesses();
        }
    }

    /**
     * @brief Initialize critical witnesses by sampling 10% per WVPT node.
     *
     * Per paper Section 3: critical witnesses are sampled per WVPT node,
     * not globally, to ensure even coverage of all polygon regions.
     */
    void initialize_critical_witnesses() {
        auto all_faces = arrangement_->all_faces();
        std::mt19937 rng{std::random_device{}()};

        if (!wvpt_->root()) {
            // Fallback: global sample if no WVPT
            size_t sample_size = static_cast<size_t>(
                all_faces.size() * config_.initial_critical_fraction);
            std::sample(all_faces.begin(), all_faces.end(),
                       std::inserter(critical_witnesses_, critical_witnesses_.end()),
                       sample_size, rng);
            return;
        }

        // Sample 10% from each WVPT node's corresponding arrangement faces
        std::function<void(typename WVPT::Node*)> sample_node;
        sample_node = [&](typename WVPT::Node* node) {
            if (!node) return;

            // Find arrangement faces that lie within this node's WVP
            std::vector<typename Arrangement::Face_handle> node_faces;
            for (auto f : all_faces) {
                Point c = arrangement_->compute_face_centroid(f);
                if (node->weak_visibility_polygon.contains(c)) {
                    node_faces.push_back(f);
                }
            }

            // Sample at least 1, at most all faces in this node
            size_t sample_size = std::max(size_t(1),
                static_cast<size_t>(node_faces.size() * config_.initial_critical_fraction));
            sample_size = std::min(sample_size, node_faces.size());

            std::sample(node_faces.begin(), node_faces.end(),
                       std::inserter(critical_witnesses_, critical_witnesses_.end()),
                       sample_size, rng);

            for (auto& child : node->children) {
                sample_node(child.get());
            }
        };

        sample_node(wvpt_->root());
    }

    /**
     * @brief Run single iteration
     */
    IterationResult run_iteration(IterativeAlgorithmResult& result) {
        // Run critical witness cycle if enabled
        if (config_.use_critical_witnesses) {
            auto cw_result = run_critical_witness_cycle(result);
            if (cw_result == IterationResult::OPTIMAL_FOUND) {
                return IterationResult::OPTIMAL_FOUND;
            }
        }

        // Run IP protocol
        return run_ip_protocol(result);
    }

    /**
     * @brief Run critical witness cycle.
     *
     * Per paper Section 5: run IPs restricted to W* until either optimal or
     * all unseen witnesses are in W*.  Each cycle that finds unseen witnesses
     * outside W* adds a subset to W* and re-solves with the enlarged W*.
     */
    IterationResult run_critical_witness_cycle(IterativeAlgorithmResult& result) {
        int max_cycles = 100;
        int cycle = 0;

        while (cycle < max_cycles) {
            // Build and solve normal IP restricted to current critical witnesses (W*)
            auto ip_result = run_normal_ip(critical_witnesses_);

            if (!ip_result.success) {
                break;
            }

            // Check if solution is optimal w.r.t. critical witnesses
            if (check_optimal_solution(ip_result)) {
                current_solution_ = ip_result;
                return IterationResult::OPTIMAL_FOUND;
            }

            // Find unseen witnesses from the FULL witness set (not just W*).
            // We need to know whether the current guards miss non-critical witnesses.
            auto unseen = find_unseen_witnesses_outside_critical(ip_result);

            if (unseen.empty()) {
                // No unseen witnesses outside W* — the solution is valid for the full set.
                current_solution_ = ip_result;
                return IterationResult::OPTIMAL_FOUND;
            }

            // Add a subset of unseen non-critical witnesses to W*
            // Paper says: add a small constant-size subset
            const size_t max_to_add = std::max(size_t(1),
                std::min(unseen.size(), size_t(10)));
            size_t added = 0;
            for (auto w : unseen) {
                if (added >= max_to_add) break;
                critical_witnesses_.insert(w);
                ++added;
            }

            cycle++;
        }

        return IterationResult::CRITICAL_WITNESSES_ADDED;
    }

    /**
     * @brief Run IP protocol
     *
     * Per paper: uses the two-stage IP (Stage 1: minimize guards with ε=0;
     * Stage 2: fix guard count, maximize use of splittable faces).
     */
    IterationResult run_ip_protocol(IterativeAlgorithmResult& result) {
        // Run two-stage IP as per paper
        auto normal_result = run_two_stage_ip();

        if (!normal_result.success) {
            return IterationResult::NO_PROGRESS;
        }

        // Store solution
        current_solution_ = normal_result;

        // Check for optimal solution (all guards are vertex guards, no unseen witnesses)
        if (check_optimal_solution(normal_result)) {
            return IterationResult::OPTIMAL_FOUND;
        }

        // Check vision-stability termination
        if (check_termination(result)) {
            return IterationResult::OPTIMAL_FOUND;
        }

        // Step 3 (paper Section 7.3): Check for splittable faces from normal IP
        auto splittable = get_splittable_faces(normal_result);
        if (!splittable.empty()) {
            for (auto face : splittable) {
                splitter_->split(face);
                result.num_splits++;
            }
            return IterationResult::SPLIT_PERFORMED;
        }

        // Step 4 (paper Section 7.3): Run BIG IP to find a solution with splittable faces
        // "Run Big IP" — only when normal IP found no splittable faces
        if (config_.ip_protocol == IPProtocol::NORMAL) {
            int guard_count = static_cast<int>(normal_result.vertex_guards.size() +
                                               normal_result.face_guards.size());
            auto big_result = run_big_ip(guard_count);
            if (big_result.success) {
                auto big_splittable = get_splittable_faces(big_result);
                if (!big_splittable.empty()) {
                    for (auto face : big_splittable) {
                        splitter_->split(face);
                        result.num_splits++;
                    }
                    return IterationResult::SPLIT_PERFORMED;
                }
            }
        }

        // Step 5 (paper Section 7.3): Update granularity if all faces unsplittable
        if (has_unsplittable_face()) {
            splitter_->update_granularity();
            result.num_granularity_updates++;
            return IterationResult::GRANULARITY_UPDATED;
        }

        return IterationResult::NO_PROGRESS;
    }

    /**
     * @brief Run normal IP, optionally restricted to a set of critical witnesses.
     *
     * When critical_witnesses is non-empty, face-witness constraints are
     * only built for the witnesses in that set.  This is the exact
     * critical-witness optimisation from paper Section 5.
     *
     * @param critical_witnesses  If non-empty, restrict face-witness constraints
     *                            to this set.  Pass empty set to use all witnesses.
     */
    ip::IPSolution run_normal_ip(
            const std::set<typename Arrangement::Face_handle>& critical_witnesses = {}) {
        ip::IPFormulation<Kernel> formulation(*arrangement_, ip::IPType::NORMAL);

        // ε = 1/(|C|+|W|+1) per paper formula
        double epsilon = formulation.calculate_epsilon();

        if (critical_witnesses.empty()) {
            // No critical witness filtering — use all witnesses
            formulation.build_normal(epsilon);
        } else {
            // Critical witness mode: only build face-witness constraints for W*
            formulation.build_normal_with_critical(epsilon, critical_witnesses);
        }

        auto solver_result = solver_->solve(formulation);

        if (solver_result.success) {
            return formulation.interpret_solution(solver_result.solution);
        }

        return ip::IPSolution{};
    }

    /**
     * @brief Run big IP
     */
    ip::IPSolution run_big_ip(int guard_count) {
        ip::IPFormulation<Kernel> formulation(*arrangement_, ip::IPType::BIG);

        // Calculate epsilon per paper formula: ε = 1/(|C| + |W| + 1)
        // This can be calculated from the arrangement before building
        double epsilon = formulation.calculate_epsilon();

        // Build BIG IP with fixed guard count (single build, not double)
        formulation.build_big(guard_count, epsilon);

        auto solver_result = solver_->solve(formulation);

        if (solver_result.success) {
            return formulation.interpret_solution(solver_result.solution);
        }

        return ip::IPSolution{};
    }

    /**
     * @brief Run two-stage IP as per paper Section 4.2.
     *
     * Stage 1: minimize total guard count (ε=0)
     *          f = Σ_{c ∈ C} [c]
     * Stage 2: with guard count fixed to s, minimize face-guards + unseen
     *          witnesses to prefer vertex guards and observed faces:
     *          f = Σ_{c ∈ face(C)} [c] + Σ_{w ∈ face(W)} [w]
     *          s.t. Σ_{c ∈ C} [c] = s
     */
    ip::IPSolution run_two_stage_ip() {
        // Stage 1: minimize guard count with ε=0 (pure minimization)
        ip::IPFormulation<Kernel> stage1(*arrangement_, ip::IPType::NORMAL);
        stage1.build_normal(0.0);  // ε=0 for pure guard-count minimization

        auto result1 = solver_->solve(stage1);
        if (!result1.success) {
            return ip::IPSolution{};
        }

        // s = optimal guard count from Stage 1
        int guard_count = static_cast<int>(std::round(result1.objective_value));

        if (guard_count == 0) {
            return stage1.interpret_solution(result1.solution);
        }

        // Stage 2: fix guard count = s, minimize face-guards + unseen witnesses
        // This is NOT the Big IP — it uses build_stage2() per paper Section 4.2.
        ip::IPFormulation<Kernel> stage2(*arrangement_, ip::IPType::NORMAL);
        stage2.build_stage2(guard_count);   // <— correct Stage-2 formulation

        auto result2 = solver_->solve(stage2);
        if (result2.success) {
            return stage2.interpret_solution(result2.solution);
        }

        // Fallback to Stage 1 result if Stage 2 fails
        return stage1.interpret_solution(result1.solution);
    }

    /**
     * @brief Check if solution is optimal
     */
    bool check_optimal_solution(const ip::IPSolution& solution) {
        // Optimal if:
        // 1. All guards are vertex-guards
        // 2. All face-witnesses are seen
        return solution.face_guards.empty() && solution.unseen_witnesses.empty();
    }

    /**
     * @brief Find unseen witnesses
     */
    std::vector<typename Arrangement::Face_handle> find_unseen_witnesses(const ip::IPSolution& solution) {
        std::vector<typename Arrangement::Face_handle> unseen;
        for (size_t idx : solution.unseen_witnesses) {
            auto faces = arrangement_->all_faces();
            if (idx < faces.size()) {
                unseen.push_back(faces[idx]);
            }
        }
        return unseen;
    }

    /**
     * @brief Add to critical witnesses
     */
    void add_to_critical_witnesses(const std::vector<typename Arrangement::Face_handle>& witnesses) {
        for (auto w : witnesses) {
            critical_witnesses_.insert(w);
        }
    }

    /**
     * @brief Find faces that are NOT seen by the current solution's guards
     *        AND are NOT already in the critical witness set W*.
     *
     * Per paper Section 5.3: after solving with W*, we check the full
     * arrangement to see if any non-critical witness is still uncovered.
     * These need to be added to W* before the next IP solve.
     */
    std::vector<typename Arrangement::Face_handle>
    find_unseen_witnesses_outside_critical(const ip::IPSolution& solution) {
        std::vector<typename Arrangement::Face_handle> result;
        auto all_faces = arrangement_->all_faces();

        // Build the set of guard positions from the current solution
        std::vector<Point> guard_points;
        auto all_verts = arrangement_->all_vertices();
        for (size_t idx : solution.vertex_guards) {
            if (idx < all_verts.size()) {
                guard_points.push_back(Point(all_verts[idx]->point()));
            }
        }
        for (size_t idx : solution.face_guards) {
            if (idx < all_faces.size()) {
                guard_points.push_back(arrangement_->compute_face_centroid(all_faces[idx]));
            }
        }

        const Polygon& P = arrangement_->get_polygon();
        for (size_t i = 0; i < all_faces.size(); ++i) {
            auto face = all_faces[i];
            if (face->is_unbounded() || face->is_fictitious()) continue;

            // Skip if already in W*
            if (critical_witnesses_.count(face) > 0) continue;

            // Check if any guard sees this face completely
            Polygon face_poly = arrangement_->face_to_polygon(face);
            bool seen = false;
            for (const auto& g : guard_points) {
                if (geometry::Visibility<Kernel>::sees_completely(P, g, face_poly)) {
                    seen = true;
                    break;
                }
            }

            if (!seen) {
                result.push_back(face);
            }
        }

        return result;
    }

    /**
     * @brief Get splittable faces from solution
     */
    std::vector<typename Arrangement::Face_handle> get_splittable_faces(const ip::IPSolution& solution) {
        std::vector<typename Arrangement::Face_handle> splittable;
        auto all_faces = arrangement_->all_faces();

        // Check face-guards
        for (size_t idx : solution.face_guards) {
            if (idx < all_faces.size()) {
                if (arrangement_->is_splittable(all_faces[idx], splitter_->current_granularity())) {
                    splittable.push_back(all_faces[idx]);
                }
            }
        }

        // Check unseen witnesses
        for (size_t idx : solution.unseen_witnesses) {
            if (idx < all_faces.size()) {
                if (arrangement_->is_splittable(all_faces[idx], splitter_->current_granularity())) {
                    splittable.push_back(all_faces[idx]);
                }
            }
        }

        return splittable;
    }

    /**
     * @brief Check if there's an unsplittable face
     */
    bool has_unsplittable_face() {
        auto all_faces = arrangement_->all_faces();
        for (auto face : all_faces) {
            if (arrangement_->is_unsplittable(face, splitter_->current_granularity())) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Check termination using the paper's vision-stability criterion.
     *
     * Per paper Section 3.2, Corollary 9: when all faces are unsplittable at
     * granularity λ, the polygon is vision-stable for δ = 8πλ.
     *
     * We verify this by solving two IPs:
     *   - Enhanced IP:   uses vis_{+δ}(c) for each candidate c
     *   - Diminished IP: uses vis_{-δ}(c) for each candidate c
     *
     * If opt(P, +δ) == opt(P, -δ) the polygon is vision-stable ⇒ terminate.
     */
    bool check_termination(IterativeAlgorithmResult& result) {
        // δ = 8πλ  (paper Corollary 9)
        FT lambda = splitter_->current_granularity();
        const double pi = M_PI;
        FT delta = FT(8.0 * pi) * lambda;

        const Polygon& P = arrangement_->get_polygon();

        // Solve enhanced IP: visibility enlarged by δ at each reflex vertex
        int opt_enhanced  = solve_ip_with_modified_visibility(P, delta, true);
        // Solve diminished IP: visibility shrunk by δ at each reflex vertex
        int opt_diminished = solve_ip_with_modified_visibility(P, delta, false);

        // Vision-stability condition: opt(P, +δ) == opt(P, -δ)
        if (opt_enhanced >= 0 && opt_diminished >= 0 &&
            opt_enhanced == opt_diminished && opt_enhanced > 0) {
            result.optimal_found = true;
            result.status_message =
                "Polygon is vision-stable (opt(P,+δ)=opt(P,-δ)=" +
                std::to_string(opt_enhanced) + ")"; 
            return true;
        }

        // Fallback: check if all faces are unsplittable → update granularity
        auto all_faces = arrangement_->all_faces();
        bool all_unsplittable = std::all_of(all_faces.begin(), all_faces.end(),
            [&](auto face) {
                return arrangement_->is_unsplittable(face, splitter_->current_granularity());
            });

        if (all_unsplittable) {
            splitter_->update_granularity();
            result.num_granularity_updates++;
            if (splitter_->current_granularity() < FT(1.0 / 1048576.0)) {
                result.status_message = "Minimum granularity reached without vision-stability";
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Solve a minimization IP using enhanced or diminished visibility.
     *
     * Builds an IPFormulation from the current arrangement then overrides the
     * face-witness visibility check to use vis_{+δ} (enhanced=true) or
     * vis_{-δ} (enhanced=false).  Returns the optimal integer guard count,
     * or -1 on solver failure.
     *
     * Implementation detail: we build the standard Normal IP (which uses
     * sees_completely(P, c, face)) and then patch the constraint coefficients
     * for each face-witness row using the enhanced/diminished predicate.
     */
    int solve_ip_with_modified_visibility(const Polygon& P, FT delta, bool use_enhanced) {
        auto all_verts = arrangement_->all_vertices();
        auto all_faces = arrangement_->all_faces();
        double epsilon = 1.0 / (all_verts.size() + 2 * all_faces.size() + 1);

        // --- Build variables ---
        std::vector<ip::IPVariable> vars;
        int idx = 0;

        // Vertex candidates
        for (size_t i = 0; i < all_verts.size(); ++i) {
            ip::IPVariable v;
            v.name  = "vc_" + std::to_string(idx);
            v.index = idx++;
            v.is_vertex_candidate = true;
            v.is_face_candidate   = false;
            v.is_face_witness     = false;
            v.original_id         = i;
            v.is_splittable       = false;
            vars.push_back(v);
        }
        // Face candidates
        for (size_t i = 0; i < all_faces.size(); ++i) {
            ip::IPVariable v;
            v.name  = "fc_" + std::to_string(idx);
            v.index = idx++;
            v.is_vertex_candidate = false;
            v.is_face_candidate   = true;
            v.is_face_witness     = false;
            v.original_id         = i;
            v.is_splittable = arrangement_->is_splittable(all_faces[i],
                                    splitter_->current_granularity());
            vars.push_back(v);
        }
        int witness_start_idx = idx;
        // Face witnesses
        for (size_t i = 0; i < all_faces.size(); ++i) {
            ip::IPVariable v;
            v.name  = "wf_" + std::to_string(idx);
            v.index = idx++;
            v.is_vertex_candidate = false;
            v.is_face_candidate   = false;
            v.is_face_witness     = true;
            v.original_id         = i;
            v.is_splittable = arrangement_->is_splittable(all_faces[i],
                                    splitter_->current_granularity());
            vars.push_back(v);
        }

        // --- Objective ---
        std::vector<double> obj(vars.size(), 0.0);
        for (const auto& v : vars) {
            if (v.is_vertex_candidate)   obj[v.index] = 1.0;
            else if (v.is_face_candidate) obj[v.index] = 1.0 + epsilon;
            else if (v.is_face_witness)   obj[v.index] = epsilon;
        }

        // --- Visibility predicate using enhanced or diminished vis ---
        auto cand_sees_face = [&](const Point& cand, const Polygon& face_poly) -> bool {
            Polygon vis_mod = use_enhanced
                ? geometry::Visibility<Kernel>::compute_enhanced_visibility(P, cand, delta)
                : geometry::Visibility<Kernel>::compute_diminished_visibility(P, cand, delta);
            if (vis_mod.num_vertices() < 3) return false;
            // Check that every vertex of face_poly lies inside vis_mod
            auto vis_cgal = vis_mod.cgal_polygon();
            if (vis_cgal.orientation() == CGAL::CLOCKWISE)
                vis_cgal.reverse_orientation();
            for (const auto& vtx : face_poly.vertices()) {
                if (vis_cgal.bounded_side(vtx.cgal_point()) == CGAL::ON_UNBOUNDED_SIDE)
                    return false;
            }
            return true;
        };

        // --- Constraints ---
        std::vector<ip::IPConstraint> constraints;
        for (size_t i = 0; i < all_faces.size(); ++i) {
            auto face = all_faces[i];
            if (face->is_unbounded() || face->is_fictitious()) continue;

            Polygon face_poly = arrangement_->face_to_polygon(face);

            ip::IPConstraint con;
            con.name = "vis_fw_" + std::to_string(i);
            // Add witness variable for this face
            con.variable_indices.push_back(witness_start_idx + static_cast<int>(i));
            con.coefficients.push_back(1.0);

            // Vertex candidates
            for (size_t vi = 0; vi < all_verts.size(); ++vi) {
                Point cp(all_verts[vi]->point());
                if (cand_sees_face(cp, face_poly)) {
                    con.variable_indices.push_back(static_cast<int>(vi));
                    con.coefficients.push_back(1.0);
                }
            }
            // Face candidates
            for (size_t fi = 0; fi < all_faces.size(); ++fi) {
                if (fi == i) continue;
                auto cf = all_faces[fi];
                if (cf->is_unbounded() || cf->is_fictitious()) continue;
                Point cp = arrangement_->compute_face_centroid(cf);
                if (cand_sees_face(cp, face_poly)) {
                    con.variable_indices.push_back(static_cast<int>(all_verts.size() + fi));
                    con.coefficients.push_back(1.0);
                }
            }

            con.lower_bound = 1.0;
            con.upper_bound = std::numeric_limits<double>::infinity();
            constraints.push_back(con);
        }

        // Also add vertex-witness constraints (plain visibility — no change needed)
        for (size_t vi = 0; vi < all_verts.size(); ++vi) {
            Point wp(all_verts[vi]->point());
            ip::IPConstraint con;
            con.name = "vis_vw_" + std::to_string(vi);

            for (size_t ci = 0; ci < all_verts.size(); ++ci) {
                Point cp(all_verts[ci]->point());
                if (geometry::Visibility<Kernel>::is_visible(P, cp, wp)) {
                    con.variable_indices.push_back(static_cast<int>(ci));
                    con.coefficients.push_back(1.0);
                }
            }
            for (size_t fi = 0; fi < all_faces.size(); ++fi) {
                auto cf = all_faces[fi];
                if (cf->is_unbounded() || cf->is_fictitious()) continue;
                Point cp = arrangement_->compute_face_centroid(cf);
                if (geometry::Visibility<Kernel>::is_visible(P, cp, wp)) {
                    con.variable_indices.push_back(static_cast<int>(all_verts.size() + fi));
                    con.coefficients.push_back(1.0);
                }
            }
            con.lower_bound = 1.0;
            con.upper_bound = std::numeric_limits<double>::infinity();
            constraints.push_back(con);
        }

        // --- Solve using the custom formulation via build_from_raw() ---
        // Now that IPFormulation supports build_from_raw(), we inject the
        // custom variables, constraints, and objective directly.
        ip::IPFormulation<Kernel> custom_formulation(*arrangement_, ip::IPType::NORMAL);
        custom_formulation.build_from_raw(
            std::move(vars),
            std::move(constraints),
            std::move(obj),
            /*is_minimization=*/true
        );

        auto solver_result = solver_->solve(custom_formulation);
        if (!solver_result.success) return -1;
        return static_cast<int>(std::round(solver_result.objective_value));
    }

    /**
     * @brief Extract final guard positions
     */
    std::vector<Point> extract_guards() {
        std::vector<Point> guards;

        // Extract vertex guards
        auto all_vertices = arrangement_->all_vertices();
        for (size_t idx : current_solution_.vertex_guards) {
            if (idx < all_vertices.size()) {
                guards.push_back(Point(all_vertices[idx]->point()));
            }
        }

        // Convert face guards to representative points
        auto all_faces = arrangement_->all_faces();
        for (size_t idx : current_solution_.face_guards) {
            if (idx < all_faces.size()) {
                // Use centroid as representative
                guards.push_back(arrangement_->compute_face_centroid(all_faces[idx]));
            }
        }

        return guards;
    }
};

// Common type aliases
using IterativeAlgorithmE = IterativeAlgorithm<CGAL::Exact_predicates_exact_constructions_kernel>;
using IterativeAlgorithmD = IterativeAlgorithm<CGAL::Simple_cartesian<double>>;

} // namespace algorithm
} // namespace agp
