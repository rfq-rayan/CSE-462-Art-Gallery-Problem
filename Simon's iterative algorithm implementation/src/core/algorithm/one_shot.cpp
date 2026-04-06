/**
 * @file one_shot.cpp
 * @brief Template instantiation for OneShotAlgorithm
 */

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include "core/algorithm/one_shot.hpp"

// Explicit instantiation for Exact kernel
namespace agp {
namespace algorithm {

template class OneShotAlgorithm<CGAL::Exact_predicates_exact_constructions_kernel>;

} // namespace algorithm
} // namespace agp
