#include <CGAL/Gmpq.h>
#include <CGAL/Cartesian.h>
#include <CGAL/Arr_segment_traits_2.h>
#include <CGAL/Arr_extended_dcel.h>
#include <CGAL/Arrangement_2.h>

typedef CGAL::Gmpq RT;
typedef CGAL::Cartesian<RT> Extended_kernel;
typedef CGAL::Arr_segment_traits_2<Extended_kernel> ArrTraits;
typedef CGAL::Arr_extended_dcel<ArrTraits, bool, short, bool> DCEL;
typedef CGAL::Arrangement_2<ArrTraits, DCEL> Arrangement;

int main() {
  Arrangement arr;
  return (arr.number_of_vertices() == 0) ? 0 : 1;
}
