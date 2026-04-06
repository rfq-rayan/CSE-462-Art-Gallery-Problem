cmake_minimum_required(VERSION 3.16)
project(tozoni_agsol LANGUAGES CXX)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(CGAL REQUIRED)

option(ENABLE_GLPK "Build with GLPK ILP solver" ON)
option(ENABLE_XPRESS "Build with FICO Xpress ILP solver" OFF)

set(GLPK_ROOT "" CACHE PATH "Prefix containing include/glpk.h and lib/libglpk (optional)")
set(XPRESS_ROOT "" CACHE PATH "Prefix for FICO Xpress SDK (optional)")

if(ENABLE_GLPK)
  find_path(GLPK_INCLUDE_DIR NAMES glpk.h
    HINTS ${GLPK_ROOT} ENV GLPK_PATH
    PATHS /usr /usr/local
    PATH_SUFFIXES include)
  find_library(GLPK_LIBRARY NAMES glpk
    HINTS ${GLPK_ROOT} ENV GLPK_PATH
    PATHS /usr /usr/local
    PATH_SUFFIXES lib lib64)
  if(NOT GLPK_INCLUDE_DIR OR NOT GLPK_LIBRARY)
    message(FATAL_ERROR "GLPK not found. Install libglpk-devel (or set GLPK_ROOT / GLPK_PATH), or set -DENABLE_GLPK=OFF.")
  endif()
endif()

if(NOT ENABLE_GLPK AND NOT ENABLE_XPRESS)
  message(FATAL_ERROR "Enable at least one of ENABLE_GLPK or ENABLE_XPRESS.")
endif()

if(ENABLE_XPRESS)
  find_path(XPRESS_INCLUDE_DIR NAMES xprs.h
    HINTS ${XPRESS_ROOT} ENV XPRESS_PATH
    PATH_SUFFIXES include)
  find_library(XPRESS_LIBRARY NAMES xprs
    HINTS ${XPRESS_ROOT} ENV XPRESS_PATH
    PATH_SUFFIXES lib lib64)
  if(NOT XPRESS_INCLUDE_DIR OR NOT XPRESS_LIBRARY)
    message(FATAL_ERROR "Xpress not found. Set XPRESS_ROOT or disable ENABLE_XPRESS.")
  endif()
endif()

function(agsol_cgal_props target)
  target_link_libraries(${target} PUBLIC CGAL::CGAL)
  target_compile_options(${target} PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-frounding-math>)
endfunction()

set(POLY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/polygon)
set(GRID_DIR ${CMAKE_CURRENT_SOURCE_DIR}/grid)
set(SCP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/scp-solver)
set(PRE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/pre-solver)
set(AG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/art-gallery-pg)

# --- polygon ---
add_library(PolygonExt SHARED polygon/PolygonExt.C)
target_include_directories(PolygonExt PUBLIC ${POLY_DIR})
agsol_cgal_props(PolygonExt)

add_library(PolygonWithHolesExt SHARED polygon/PolygonWithHolesExt.C)
target_include_directories(PolygonWithHolesExt PUBLIC ${POLY_DIR})
target_link_libraries(PolygonWithHolesExt PUBLIC PolygonExt)
agsol_cgal_props(PolygonWithHolesExt)

add_library(polAlgorithms SHARED polygon/polAlgorithms.C)
target_include_directories(polAlgorithms PUBLIC ${POLY_DIR})
target_link_libraries(polAlgorithms PUBLIC PolygonExt)
agsol_cgal_props(polAlgorithms)

# --- grid ---
add_library(IGrid SHARED grid/IGrid.C)
target_include_directories(IGrid PUBLIC ${GRID_DIR} ${POLY_DIR})
target_link_libraries(IGrid PUBLIC PolygonWithHolesExt PolygonExt)
agsol_cgal_props(IGrid)

add_library(AVPLightGrid SHARED grid/AVPLightGrid.C)
target_include_directories(AVPLightGrid PUBLIC ${GRID_DIR} ${POLY_DIR})
target_link_libraries(AVPLightGrid PUBLIC IGrid PolygonWithHolesExt PolygonExt)
agsol_cgal_props(AVPLightGrid)

# --- scp-solver ---
add_library(Lagrangian SHARED scp-solver/Lagrangian.C)
target_include_directories(Lagrangian PUBLIC ${SCP_DIR})

if(ENABLE_GLPK)
  add_library(SolverPLIGlpk SHARED scp-solver/SolverPLIGlpk.C)
  target_include_directories(SolverPLIGlpk PRIVATE ${GLPK_INCLUDE_DIR})
  target_link_libraries(SolverPLIGlpk PRIVATE ${GLPK_LIBRARY})
endif()

if(ENABLE_XPRESS)
  add_library(SolverPLIXpress SHARED scp-solver/SolverPLIXpress.C)
  target_include_directories(SolverPLIXpress PRIVATE ${XPRESS_INCLUDE_DIR})
  target_link_libraries(SolverPLIXpress PRIVATE ${XPRESS_LIBRARY})
  target_compile_definitions(SolverPLIXpress PRIVATE XPRESS)
endif()

# --- pre-solver ---
add_library(PreSolver SHARED pre-solver/PreSolver.C)
target_include_directories(PreSolver PUBLIC ${PRE_DIR} ${SCP_DIR})
target_link_libraries(PreSolver PUBLIC Lagrangian m)
if(ENABLE_GLPK)
  target_compile_definitions(PreSolver PRIVATE GLPK)
  target_link_libraries(PreSolver PRIVATE SolverPLIGlpk)
endif()
if(ENABLE_XPRESS)
  target_compile_definitions(PreSolver PRIVATE XPRESS)
  target_link_libraries(PreSolver PRIVATE SolverPLIXpress)
endif()

# --- main executable ---
add_executable(artGallerySolver
  art-gallery-pg/artGallerySolver.C
  art-gallery-pg/AuxGallery.C)
target_include_directories(artGallerySolver PRIVATE
  ${AG_DIR} ${POLY_DIR} ${GRID_DIR} ${SCP_DIR} ${PRE_DIR})
target_link_libraries(artGallerySolver PRIVATE
  m
  PolygonWithHolesExt
  PolygonExt
  polAlgorithms
  IGrid
  AVPLightGrid
  PreSolver
  Lagrangian
  CGAL::CGAL)

install(TARGETS artGallerySolver RUNTIME DESTINATION bin)
install(TARGETS
  PolygonExt PolygonWithHolesExt polAlgorithms IGrid AVPLightGrid
  Lagrangian PreSolver
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
if(ENABLE_GLPK)
  install(TARGETS SolverPLIGlpk LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
endif()
if(ENABLE_XPRESS)
  install(TARGETS SolverPLIXpress LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
endif()

# Smoke test: arrangement + extended DCEL compile against CGAL
enable_testing()
add_executable(test_arrangement_smoke tests/test_arrangement_smoke.cpp)
target_link_libraries(test_arrangement_smoke PRIVATE CGAL::CGAL)
target_compile_options(test_arrangement_smoke PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-frounding-math>)
add_test(NAME arrangement_smoke COMMAND test_arrangement_smoke)
