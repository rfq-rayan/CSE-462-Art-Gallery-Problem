/**
 * @file visibility.cpp
 * @brief Visibility computation implementation (explicit template instantiations)
 *
 * Most of the Visibility class implementation is in the header file as inline
 * methods. This file contains only the explicit template instantiations.
 */

#include "core/geometry/visibility.hpp"

namespace agp {
namespace geometry {

// Explicit template instantiation
template class Visibility<CGAL::Exact_predicates_exact_constructions_kernel>;
template class Visibility<CGAL::Simple_cartesian<double>>;

} // namespace geometry
} // namespace agp
