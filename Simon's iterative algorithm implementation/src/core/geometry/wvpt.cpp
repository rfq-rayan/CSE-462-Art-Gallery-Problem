/**
 * @file wvpt.cpp
 * @brief Weak Visibility Polygon Tree implementation (explicit template instantiations)
 *
 * Most of the WVPT class implementation is in the header file as inline
 * methods. This file contains only the explicit template instantiations.
 */

#include "core/geometry/wvpt.hpp"

namespace agp {
namespace geometry {

// Explicit template instantiation
template class WVPT<CGAL::Exact_predicates_exact_constructions_kernel>;
template class WVPT<CGAL::Simple_cartesian<double>>;

} // namespace geometry
} // namespace agp
