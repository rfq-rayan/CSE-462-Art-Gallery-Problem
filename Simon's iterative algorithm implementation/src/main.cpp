/**
 * @file main.cpp
 * @brief Main entry point for the Art Gallery Problem solver
 */

#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <memory>

#include "core/geometry/polygon.hpp"
#include "core/geometry/point.hpp"
#include "core/geometry/arrangement.hpp"
#include "core/geometry/visibility.hpp"
#include "core/geometry/wvpt.hpp"
#include "core/algorithm/iterative.hpp"
#include "core/algorithm/splitter.hpp"
#include "core/algorithm/verifier.hpp"
#include "core/algorithm/one_shot.hpp"
#include "core/ip/ip_formulation.hpp"
#include "core/ip/ip_solver.hpp"
#include "core/utils/config.hpp"

#include <nlohmann/json.hpp>

using namespace agp;
using namespace agp::geometry;

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " [options] <polygon_file>\n\n"
              << "Options:\n"
              << "  --help                  Display this help message and exit\n"
              << "  --input <file>          Input polygon JSON file\n"
              << "  --output <file>         Output solution JSON file\n"
              << "  --granularity <double>  Granularity parameter (default: 1/16)\n"
              << "  --use-cplex             Use CPLEX solver (default: OFF)\n"
              << "  --use-glpk              Use GLPK solver (default: ON)\n"
              << "  --max-iterations <n>    Max iterations (0 = unlimited, default: 10000)\n"
              << "  --time-limit <sec>      Time limit in seconds (default: 3600)\n"
              << "  --verbosity <level>     Verbosity level (0=quiet, 1=info, 2=debug)\n"
              << "  --version               Show version information\n"
              << "\nVersion: 1.0.0\n";
}

int main(int argc, char* argv[]) {
    // Default parameters
    std::string input_file;
    std::string output_file;
    std::string mode = "iterative"; // or "oneshot"
    double granularity = config::INITIAL_GRANULARITY;
    bool use_cplex = false;
    bool use_glpk = true;
    int max_iterations = config::MAX_ITERATIONS;
    double time_limit = config::TIME_LIMIT_SECONDS;
    int verbosity = 1;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--version") {
            std::cout << "Art Gallery Problem Solver v1.0.0\n";
            return 0;
        } else if (arg == "--mode" && i + 1 < argc) {
            mode = argv[++i];
        } else if (arg == "--input" && i + 1 < argc) {
            input_file = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg == "--granularity" && i + 1 < argc) {
            granularity = std::stod(argv[++i]);
        } else if (arg == "--use-cplex") {
            use_cplex = true;
            use_glpk = false;
        } else if (arg == "--use-glpk") {
            use_glpk = true;
            use_cplex = false;
        } else if (arg == "--max-iterations" && i + 1 < argc) {
            max_iterations = std::stoi(argv[++i]);
        } else if (arg == "--time-limit" && i + 1 < argc) {
            time_limit = std::stod(argv[++i]);
        } else if (arg == "--verbosity" && i + 1 < argc) {
            verbosity = std::stoi(argv[++i]);
        } else if (arg[0] != '-') {
            input_file = arg;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (input_file.empty()) {
        std::cerr << "Error: No input file specified\n";
        print_usage(argv[0]);
        return 1;
    }

    try {
        // Load polygon from JSON file
        std::ifstream file(input_file);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file: " << input_file << "\n";
            return 1;
        }

        nlohmann::json j;
        file >> j;

        // Parse vertices
        std::vector<std::pair<double, double>> vertex_coords;
        if (j.is_array()) {
            // Check if first element has "vertices" key (array of polygon objects)
            if (!j.empty() && j[0].contains("vertices")) {
                // Array of polygon objects - take first one
                std::cerr << "Warning: Multiple polygons in file, using first one\n";
                for (const auto& v : j[0]["vertices"]) {
                    if (v.contains("x") && v.contains("y")) {
                        vertex_coords.emplace_back(v["x"], v["y"]);
                    } else if (v.is_array() && v.size() >= 2) {
                        vertex_coords.emplace_back(v[0], v[1]);
                    }
                }
            } else {
                // Direct array of vertex coordinates
                for (const auto& v : j) {
                    if (v.contains("x") && v.contains("y")) {
                        vertex_coords.emplace_back(v["x"], v["y"]);
                    } else if (v.is_array() && v.size() >= 2) {
                        vertex_coords.emplace_back(v[0], v[1]);
                    }
                }
            }
        } else if (j.contains("vertices")) {
            for (const auto& v : j["vertices"]) {
                if (v.contains("x") && v.contains("y")) {
                    vertex_coords.emplace_back(v["x"], v["y"]);
                } else if (v.is_array() && v.size() >= 2) {
                    vertex_coords.emplace_back(v[0], v[1]);
                }
            }
        }

        if (vertex_coords.size() < 3) {
            std::cerr << "Error: Polygon must have at least 3 vertices\n";
            return 1;
        }

        // Create polygon
        std::vector<PointE> vertices;
        for (const auto& [x, y] : vertex_coords) {
            vertices.push_back(PointE(x, y));
        }
        PolygonE polygon(vertices);

        if (verbosity >= 1) {
            std::cout << "Loaded polygon with " << polygon.num_vertices() << " vertices\n";
        }

        // Run selected algorithm mode
        std::vector<PointE> guards;
        bool optimal_found = false;
        std::string status_message;
        size_t final_candidates = 0, final_witnesses = 0, num_iterations = 0;
        double ip_time = 0, visibility_time = 0, split_time = 0;
        int final_granularity_k = 4;
        int num_granularity_updates = 0;

        auto start_time = std::chrono::high_resolution_clock::now();

        if (mode == "oneshot") {
            if (verbosity >= 1)
                std::cout << "Mode: one-shot (single IP)\n";

            algorithm::OneShotAlgorithmE one_shot;
            auto result = one_shot.solve(polygon);

            guards = result.guards;
            optimal_found = result.optimal_found;
            status_message = result.status_message;
            final_candidates = result.num_candidates;
            final_witnesses = result.num_witnesses;
            ip_time = result.ip_time;
        } else {
            if (verbosity >= 1)
                std::cout << "Mode: iterative (paper's main algorithm)\n";

            // Configure algorithm
            algorithm::IterativeAlgorithmConfig config;
            config.initial_granularity = granularity;
            config.max_iterations = max_iterations;
            config.time_limit_seconds = time_limit;
            config.use_critical_witnesses = config::USE_CRITICAL_WITNESSES;

            algorithm::IterativeAlgorithm<CGAL::Exact_predicates_exact_constructions_kernel> alg(config);
            auto result = alg.solve(polygon);

            guards = result.guards;
            optimal_found = result.optimal_found;
            status_message = result.status_message;
            final_candidates = result.final_num_candidates;
            final_witnesses = result.final_num_witnesses;
            num_iterations = result.num_iterations;
            final_granularity_k = result.final_granularity_k;
            num_granularity_updates = result.num_granularity_updates;
            ip_time = result.ip_time;
            visibility_time = result.visibility_time;
            split_time = result.split_time;
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Output results
        if (verbosity >= 1) {
            std::cout << "\n=== Solution ===\n";
            std::cout << "Status: " << (optimal_found ? "Optimal" : "Suboptimal") << "\n";
            std::cout << "Number of guards: " << guards.size() << "\n";
            std::cout << "Iterations: " << num_iterations << "\n";
            std::cout << "Total time: " << (duration.count() / 1000.0) << " seconds\n";

            if (verbosity >= 2) {
                std::cout << "\nGuards:\n";
                for (size_t i = 0; i < guards.size(); ++i) {
                    const auto& g = guards[i];
                    std::cout << "  Guard " << (i + 1) << ": ("
                              << g.x_double() << ", " << g.y_double() << ")\n";
                }
            }

            std::cout << "\nArrangement statistics:\n";
            std::cout << "  Final candidates: " << final_candidates << "\n";
            std::cout << "  Final witnesses: "  << final_witnesses  << "\n";
            std::cout << "  Granularity: 2^-"   << final_granularity_k << "\n";
            std::cout << "  Granularity updates: " << num_granularity_updates << "\n";
        }

        // Write output to JSON if specified
        if (!output_file.empty()) {
            nlohmann::json output;
            output["status"]      = optimal_found ? "optimal" : "suboptimal";
            output["mode"]        = mode;
            output["num_guards"]  = guards.size();
            output["iterations"]  = num_iterations;
            output["solve_time_seconds"] = duration.count() / 1000.0;

            output["guards"] = nlohmann::json::array();
            for (const auto& g : guards) {
                output["guards"].push_back({{
                    "x", g.x_double()}, {"y", g.y_double()}
                });
            }

            output["statistics"] = {
                {"final_candidates",     final_candidates},
                {"final_witnesses",      final_witnesses},
                {"final_granularity_k",  final_granularity_k},
                {"granularity_updates",  num_granularity_updates},
                {"ip_time",              ip_time},
                {"visibility_time",      visibility_time},
                {"split_time",           split_time}
            };

            std::ofstream out_file(output_file);
            out_file << output.dump(2);

            if (verbosity >= 1)
                std::cout << "\nSolution written to: " << output_file << "\n";
        }

        return optimal_found ? 0 : 2;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
