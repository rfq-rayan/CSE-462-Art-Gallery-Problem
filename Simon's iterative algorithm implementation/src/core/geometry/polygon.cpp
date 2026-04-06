/**
 * @file polygon.cpp
 * @brief Polygon implementation (explicit template instantiations)
 *
 * Most of the Polygon class implementation is in the header file as inline
 * methods. This file contains only the explicit template instantiations.
 */

#include "core/geometry/polygon.hpp"

namespace agp {
namespace geometry {

// Explicit template instantiation
template class Polygon<CGAL::Exact_predicates_exact_constructions_kernel>;
template class Polygon<CGAL::Simple_cartesian<double>>;

} // namespace geometry
} // namespace agp
