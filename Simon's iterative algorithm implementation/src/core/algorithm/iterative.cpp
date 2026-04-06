/**
 * @file iterative.cpp
 * @brief Iterative algorithm implementation
 */

#include "core/algorithm/iterative.hpp"
#include "core/algorithm/splitter.hpp"
#include "core/algorithm/verifier.hpp"
#include "core/ip/ip_solver.hpp"
#include "core/utils/config.hpp"
#include <algorithm>
#include <chrono>
#include <random>

namespace agp {
namespace algorithm {

// Note: The main implementation is in the header file as a template class.
// This file contains explicit template instantiations and any non-template
// helper functions.

// Helper function to create default configuration
IterativeAlgorithmConfig create_default_config() {
    return IterativeAlgorithmConfig::default_config();
}

// Helper function to create fast configuration
IterativeAlgorithmConfig create_fast_config() {
    return IterativeAlgorithmConfig::fast();
}

// Helper function to create thorough configuration
IterativeAlgorithmConfig create_thorough_config() {
    return IterativeAlgorithmConfig::thorough();
}

// Explicit template instantiation
template class IterativeAlgorithm<CGAL::Exact_predicates_exact_constructions_kernel>;
template class IterativeAlgorithm<CGAL::Simple_cartesian<double>>;

} // namespace algorithm
} // namespace agp
