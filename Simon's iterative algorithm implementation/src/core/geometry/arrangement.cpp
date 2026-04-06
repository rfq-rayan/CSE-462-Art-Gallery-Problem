/**
 * @file arrangement.cpp
 * @brief Arrangement implementation (explicit template instantiations)
 *
 * Most of the Arrangement class implementation is in the header file as inline
 * methods. This file contains only the explicit template instantiations.
 */

#include "core/geometry/arrangement.hpp"

namespace agp {
namespace geometry {

// Explicit template instantiation
template class Arrangement<CGAL::Exact_predicates_exact_constructions_kernel>;
template class Arrangement<CGAL::Simple_cartesian<double>>;

} // namespace geometry
} // namespace agp
