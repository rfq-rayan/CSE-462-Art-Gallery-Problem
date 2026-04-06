/**
 * @file splitter.cpp
 * @brief Face splitter explicit template instantiation
 */

#include "core/algorithm/splitter.hpp"

namespace agp {
namespace algorithm {

// Explicit template instantiation
template class Splitter<CGAL::Exact_predicates_exact_constructions_kernel>;
template class Splitter<CGAL::Simple_cartesian<double>>;

} // namespace algorithm
} // namespace agp
