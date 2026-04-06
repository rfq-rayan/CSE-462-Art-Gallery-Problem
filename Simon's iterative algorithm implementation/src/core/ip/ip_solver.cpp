/**
 * @file ip_solver.cpp
 * @brief IP Solver implementation using GLPK
 */

#include "core/ip/ip_solver.hpp"
#include "core/utils/config.hpp"

#ifdef USE_GLPK
#include <glpk.h>
#endif

#ifdef USE_CPLEX
#include <ilcplex/ilocplex.h>
#endif

#include <cmath>
#include <algorithm>
#include <chrono>

namespace agp {
namespace ip {

// Constructor
IPSolver::IPSolver(SolverType type)
    : type_(type)
    , time_limit_(config::TIME_LIMIT_SECONDS)
    , memory_limit_(4096.0)
    , verbosity_(0)
    , mip_gap_(0.0)
    , num_threads_(1)
    , progress_callback_(nullptr) {
}

// Destructor
IPSolver::~IPSolver() = default;

// Configuration methods
void IPSolver::set_time_limit(double seconds) {
    time_limit_ = seconds;
}

void IPSolver::set_memory_limit(double megabytes) {
    memory_limit_ = megabytes;
}

void IPSolver::set_output_verbosity(int level) {
    verbosity_ = level;
}

void IPSolver::set_mip_gap(double gap) {
    mip_gap_ = gap;
}

void IPSolver::set_num_threads(int threads) {
    num_threads_ = threads;
}

void IPSolver::set_progress_callback(ProgressCallback callback) {
    progress_callback_ = std::move(callback);
}

SolverResult IPSolver::solve_glpk(
    const std::vector<IPVariable>& variables,
    const std::vector<IPConstraint>& constraints,
    const std::vector<double>& objective,
    bool is_minimization,
    bool is_binary
) {
    SolverResult result;
    result.success = false;

#ifdef USE_GLPK
    // Create GLPK problem
    glp_prob* prob = glp_create_prob();
    if (!prob) {
        result.status_message = "Failed to create GLPK problem";
        return result;
    }

    // Set problem name
    glp_set_prob_name(prob, "ArtGalleryProblem");

    // Set objective direction
    glp_set_obj_dir(prob, is_minimization ? GLP_MIN : GLP_MAX);

    // Add columns (variables)
    int num_vars = static_cast<int>(variables.size());
    glp_add_cols(prob, num_vars);

    for (int i = 0; i < num_vars; ++i) {
        const auto& var = variables[i];
        glp_set_col_name(prob, i + 1, var.name.c_str());

        if (is_binary) {
            glp_set_col_kind(prob, i + 1, GLP_BV);
        } else {
            glp_set_col_kind(prob, i + 1, GLP_CV);
            glp_set_col_bnds(prob, i + 1, GLP_DB, 0.0, 1.0);
        }

        // Set objective coefficient
        if (static_cast<int>(objective.size()) > i) {
            glp_set_obj_coef(prob, i + 1, objective[i]);
        }
    }

    // Add rows (constraints)
    int num_constraints = static_cast<int>(constraints.size());
    glp_add_rows(prob, num_constraints);

    // Build constraint matrix
    std::vector<int> ia(1);  // 1-indexed row indices
    std::vector<int> ja(1);  // 1-indexed column indices
    std::vector<double> ar(1);  // coefficient values

    int nz = 0;  // number of non-zero elements

    for (int i = 0; i < num_constraints; ++i) {
        const auto& constraint = constraints[i];
        glp_set_row_name(prob, i + 1, constraint.name.c_str());

        // Set constraint bounds
        double lb = constraint.lower_bound;
        double ub = constraint.upper_bound;

        if (lb == ub) {
            glp_set_row_bnds(prob, i + 1, GLP_FX, lb, ub);
        } else if (std::isinf(ub)) {
            glp_set_row_bnds(prob, i + 1, GLP_LO, lb, 0.0);
        } else if (std::isinf(lb) && lb < 0) {
            glp_set_row_bnds(prob, i + 1, GLP_UP, 0.0, ub);
        } else {
            glp_set_row_bnds(prob, i + 1, GLP_DB, lb, ub);
        }

        // Add coefficients
        for (size_t j = 0; j < constraint.variable_indices.size(); ++j) {
            nz++;
            ia.push_back(i + 1);  // row index (1-based)
            ja.push_back(constraint.variable_indices[j] + 1);  // column index (1-based)
            ar.push_back(constraint.coefficients[j]);
        }
    }

    // Load constraint matrix
    glp_load_matrix(prob, nz, ia.data(), ja.data(), ar.data());

    // Set solver parameters
    glp_iocp parm;
    glp_init_iocp(&parm);
    parm.presolve = GLP_ON;
    parm.tm_lim = static_cast<int>(config::TIME_LIMIT_SECONDS * 1000);  // milliseconds
    parm.msg_lev = config::ENABLE_DEBUG_LOGGING ? GLP_MSG_ALL : GLP_MSG_ERR;

    // Solve MIP
    auto start_time = std::chrono::high_resolution_clock::now();
    int ret = glp_intopt(prob, &parm);
    auto end_time = std::chrono::high_resolution_clock::now();

    result.solve_time = std::chrono::duration<double>(end_time - start_time).count();

    // Debug output
    if (config::ENABLE_DEBUG_LOGGING) {
        std::cerr << "[GLPK] Return code: " << ret << std::endl;
        std::cerr << "[GLPK] Variables: " << num_vars << std::endl;
        std::cerr << "[GLPK] Constraints: " << num_constraints << std::endl;
        for (int i = 0; i < num_constraints && i < 5; ++i) {
            std::cerr << "[GLPK] Constraint " << i << " has "
                      << constraints[i].variable_indices.size() << " variables, lb="
                      << constraints[i].lower_bound << std::endl;
        }
    }

    // Check solution status
    int status = glp_mip_status(prob);

    if (config::ENABLE_DEBUG_LOGGING) {
        std::cerr << "[GLPK] MIP status: " << status << std::endl;
    }

    if (status == GLP_OPT || status == GLP_FEAS) {
        result.success = (status == GLP_OPT);
        result.objective_value = glp_mip_obj_val(prob);
        result.status_code = status;
        result.status_message = (status == GLP_OPT) ? "Optimal" : "Feasible";

        // Extract variable values
        result.solution.resize(num_vars);
        for (int i = 0; i < num_vars; ++i) {
            result.solution[i] = glp_mip_col_val(prob, i + 1);
        }
    } else {
        result.status_code = status;
        switch (status) {
            case GLP_NOFEAS:
                result.status_message = "No feasible solution";
                break;
            case GLP_UNDEF:
                result.status_message = "Solution undefined";
                break;
            default:
                result.status_message = "Unknown status: " + std::to_string(status);
        }
    }

    // Clean up
    glp_delete_prob(prob);

#else
    result.status_message = "GLPK not available";
#endif

    return result;
}

SolverResult IPSolver::solve_cplex(
    const std::vector<IPVariable>& variables,
    const std::vector<IPConstraint>& constraints,
    const std::vector<double>& objective,
    bool is_minimization,
    bool is_binary
) {
    SolverResult result;
    result.success = false;

#ifdef USE_CPLEX
    try {
        IloEnv env;
        IloModel model(env);

        // Create variables
        IloNumVarArray vars(env, variables.size());
        for (size_t i = 0; i < variables.size(); ++i) {
            if (is_binary) {
                vars[i] = IloNumVar(env, 0, 1, ILOBOOL, variables[i].name.c_str());
            } else {
                vars[i] = IloNumVar(env, 0, 1, ILOFLOAT, variables[i].name.c_str());
            }
        }

        // Add objective
        IloExpr obj_expr(env);
        for (size_t i = 0; i < objective.size(); ++i) {
            obj_expr += objective[i] * vars[i];
        }
        model.add(is_minimization ? IloMinimize(env, obj_expr) : IloMaximize(env, obj_expr));

        // Add constraints
        for (const auto& constraint : constraints) {
            IloExpr expr(env);
            for (size_t i = 0; i < constraint.variable_indices.size(); ++i) {
                expr += constraint.coefficients[i] * vars[constraint.variable_indices[i]];
            }

            if (constraint.lower_bound == constraint.upper_bound) {
                model.add(expr == constraint.lower_bound);
            } else {
                if (!std::isinf(constraint.lower_bound)) {
                    model.add(expr >= constraint.lower_bound);
                }
                if (!std::isinf(constraint.upper_bound)) {
                    model.add(expr <= constraint.upper_bound);
                }
            }
        }

        // Solve
        IloCplex cplex(model);
        cplex.setOut(env.getNullStream());

        if (!config::ENABLE_DEBUG_LOGGING) {
            cplex.setWarning(env.getNullStream());
        }

        // Set time limit
        cplex.setParam(IloCplex::Param::TimeLimit, config::TIME_LIMIT_SECONDS);

        auto start_time = std::chrono::high_resolution_clock::now();
        bool solved = cplex.solve();
        auto end_time = std::chrono::high_resolution_clock::now();

        result.solve_time = std::chrono::duration<double>(end_time - start_time).count();

        if (solved) {
            result.success = (cplex.getStatus() == IloAlgorithm::Optimal);
            result.objective_value = cplex.getObjValue();
            result.status_code = static_cast<int>(cplex.getStatus());
            result.status_message = (cplex.getStatus() == IloAlgorithm::Optimal) ? "Optimal" : "Feasible";

            // Extract solution
            result.solution.resize(variables.size());
            for (size_t i = 0; i < variables.size(); ++i) {
                result.solution[i] = cplex.getValue(vars[i]);
            }
        } else {
            result.status_message = "No solution found";
        }

        env.end();

    } catch (const IloException& e) {
        result.status_message = std::string("CPLEX error: ") + e.getMessage();
    }
#else
    result.status_message = "CPLEX not available";
#endif

    return result;
}

} // namespace ip
} // namespace agp
