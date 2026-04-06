#pragma once

#include "ip_formulation.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace agp {
namespace ip {

/**
 * @brief Solver type enumeration
 */
enum class SolverType {
    CPLEX,
    GLPK
};

/**
 * @brief Solver result structure
 */
struct SolverResult {
    bool success;
    std::vector<double> solution;
    double objective_value;
    double solve_time;
    std::string status_message;
    int status_code;
};

/**
 * @brief IP Solver interface
 *
 * Abstract interface for integer programming solvers.
 * Supports CPLEX (primary) and GLPK (fallback).
 */
class IPSolver {
public:
    explicit IPSolver(SolverType type = SolverType::GLPK);
    ~IPSolver();

    // Configuration
    void set_time_limit(double seconds);
    void set_memory_limit(double megabytes);
    void set_output_verbosity(int level);
    void set_mip_gap(double gap);
    void set_num_threads(int threads);

    // Solve the IP
    template<typename Kernel>
    SolverResult solve(const IPFormulation<Kernel>& formulation, bool is_binary = true);

    // Callback for progress reporting
    using ProgressCallback = std::function<void(int iteration, double objective, double gap)>;
    void set_progress_callback(ProgressCallback callback);

private:
    SolverType type_;
    double time_limit_ = 3600.0;
    double memory_limit_ = 4096.0;
    int verbosity_ = 0;
    double mip_gap_ = 0.0;
    int num_threads_ = 1;
    ProgressCallback progress_callback_;

    // GLPK implementation
    SolverResult solve_glpk(const std::vector<IPVariable>& variables,
                           const std::vector<IPConstraint>& constraints,
                           const std::vector<double>& objective,
                           bool is_minimization,
                           bool is_binary);

    // CPLEX implementation (stub - requires CPLEX license)
    SolverResult solve_cplex(const std::vector<IPVariable>& variables,
                            const std::vector<IPConstraint>& constraints,
                            const std::vector<double>& objective,
                            bool is_minimization,
                            bool is_binary);
};

// Template implementation
template<typename Kernel>
SolverResult IPSolver::solve(const IPFormulation<Kernel>& formulation, bool is_binary) {
    const auto& variables = formulation.variables();
    const auto& constraints = formulation.constraints();
    const auto& objective = formulation.objective_coefficients();
    bool is_minimization = formulation.is_minimization();

    switch (type_) {
        case SolverType::GLPK:
            return solve_glpk(variables, constraints, objective, is_minimization, is_binary);
        case SolverType::CPLEX:
            return solve_cplex(variables, constraints, objective, is_minimization, is_binary);
        default:
            return {false, {}, 0.0, 0.0, "Unknown solver type", -1};
    }
}

} // namespace ip
} // namespace agp
