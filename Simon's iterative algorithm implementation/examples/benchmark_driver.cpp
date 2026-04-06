#include "core/geometry/polygon.hpp"
#include "core/geometry/point.hpp"
#include "core/algorithm/iterative.hpp"
#include "core/algorithm/one_shot.hpp"
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <iostream>
#include <vector>
#include <string>

using KernelE = CGAL::Exact_predicates_exact_constructions_kernel;
using PointE = agp::geometry::Point<KernelE>;
using PolygonE = agp::geometry::Polygon<KernelE>;
using IterativeAlgorithmE = agp::algorithm::IterativeAlgorithm<KernelE>;
using OneShotAlgorithmE = agp::algorithm::OneShotAlgorithm<KernelE>;

using namespace agp::geometry;
using namespace agp::algorithm;

void run_benchmark(const PolygonE& poly, const std::string& name) {
    std::cout << "========================================\n";
    std::cout << "--- Benchmark: " << name << " ---\n";
    std::cout << "Vertices: " << poly.num_vertices() << " | Reflex: " << poly.num_reflex_vertices() << "\n\n";

    // One-Shot
    std::cout << "Running One-Shot Algorithm...\n";
    OneShotAlgorithmE os_alg;
    auto os_res = os_alg.solve(poly);
    std::cout << "[One-Shot] Guards: " << os_res.guards.size() 
              << " | Total Time: " << os_res.total_time << "s"
              << " | IP Time: " << os_res.ip_time << "s"
              << " | Status: " << os_res.status_message << "\n\n";

    // Iterative
    std::cout << "Running Iterative Algorithm...\n";
    IterativeAlgorithmE iter_alg;
    auto iter_res = iter_alg.solve(poly);
    std::cout << "[Iterative] Guards: " << iter_res.guards.size() 
              << " | Total Time: " << iter_res.total_time << "s"
              << " | Iters: " << iter_res.num_iterations 
              << " | Status: " << iter_res.status_message << "\n";
    std::cout << "========================================\n\n";
}

int main() {
    // 1. Valid L-Shaped polygon
    std::vector<PointE> l_vs = {
        PointE(0,0), PointE(10,0), PointE(10,5), 
        PointE(5,5), PointE(5,10), PointE(0,10)
    };
    PolygonE l_shape(l_vs);
    run_benchmark(l_shape, "L-Shape");

    // 2. Simple Square
    std::vector<PointE> sq_vs = {
        PointE(0,0), PointE(10,0), PointE(10,10), PointE(0,10)
    };
    PolygonE square(sq_vs);
    run_benchmark(square, "Square");

    return 0;
}
