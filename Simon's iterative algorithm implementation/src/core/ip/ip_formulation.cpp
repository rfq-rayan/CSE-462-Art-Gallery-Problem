/**
 * @file ip_formulation.cpp
 * @brief IP Formulation implementation (explicit template instantiations)
 */

#include "core/ip/ip_formulation.hpp"

namespace agp {
namespace ip {

// Note: Most of the implementation is in the header file as a template class.
// This file contains explicit template instantiations.

// Explicit template instantiation
template class IPFormulation<CGAL::Exact_predicates_exact_constructions_kernel>;
template class IPFormulation<CGAL::Simple_cartesian<double>>;

} // namespace ip
} // namespace agp
