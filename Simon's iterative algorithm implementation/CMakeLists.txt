cmake_minimum_required(VERSION 3.14)
project(ArtGalleryProblem VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find required packages
find_package(CGAL REQUIRED)
find_package(GMP REQUIRED)
find_package(MPFR REQUIRED)
find_package(Boost REQUIRED)

# Optional: CPLEX solver
option(USE_CPLEX "Use CPLEX IP solver" OFF)
if(USE_CPLEX)
    find_package(CPLEX)
    if(CPLEX_FOUND)
        message(STATUS "CPLEX found: using CPLEX IP solver")
        add_definitions(-DUSE_CPLEX)
    else()
        message(WARNING "CPLEX not found, falling back to GLPK")
        set(USE_CPLEX OFF)
    endif()
endif()

# Optional: GLPK solver (fallback)
option(USE_GLPK "Use GLPK IP solver" ON)
if(USE_GLPK AND NOT USE_CPLEX)
    find_package(GLPK)
    if(GLPK_FOUND)
        message(STATUS "GLPK found: using GLPK IP solver")
        add_definitions(-DUSE_GLPK)
    else()
        message(WARNING "GLPK not found, IP solving will be limited")
    endif()
endif()

# nlohmann_json via FetchContent
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CGAL_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

# Source files
set(GEOMETRY_SOURCES
    src/core/geometry/polygon.cpp
    src/core/geometry/visibility.cpp
    src/core/geometry/arrangement.cpp
    src/core/geometry/wvpt.cpp
)

set(IP_SOURCES
    src/core/ip/ip_formulation.cpp
    src/core/ip/ip_solver.cpp
)

set(ALGORITHM_SOURCES
    src/core/algorithm/iterative.cpp
    src/core/algorithm/splitter.cpp
    src/core/algorithm/verifier.cpp
    src/core/algorithm/one_shot.cpp
)

set(UTIL_SOURCES
    src/core/utils/logger.cpp
    src/core/utils/step_recorder.cpp
)

set(ALL_SOURCES
    ${GEOMETRY_SOURCES}
    ${IP_SOURCES}
    ${ALGORITHM_SOURCES}
    ${UTIL_SOURCES}
)

# Main library
add_library(agp_lib STATIC ${ALL_SOURCES})

target_link_libraries(agp_lib
    PUBLIC
        CGAL::CGAL
        nlohmann_json::nlohmann_json
    PRIVATE
        ${Boost_LIBRARIES}
)

# Link CPLEX if available
if(USE_CPLEX AND CPLEX_FOUND)
    target_link_libraries(agp_lib PRIVATE ${CPLEX_LIBRARIES})
    target_include_directories(agp_lib PRIVATE ${CPLEX_INCLUDE_DIRS})
endif()

# Link GLPK if available
if(USE_GLPK AND GLPK_FOUND AND NOT USE_CPLEX)
    target_link_libraries(agp_lib PRIVATE ${GLPK_LIBRARIES})
    target_include_directories(agp_lib PRIVATE ${GLPK_INCLUDE_DIRS})
endif()

# Main executable
add_executable(agp_solver src/main.cpp)
target_link_libraries(agp_solver PRIVATE agp_lib)

# Test executable (optional)
# Test executable (optional)
option(BUILD_TESTS "Build test suite" ON)
if(BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG        release-1.12.1
    )
    FetchContent_MakeAvailable(googletest)

    add_executable(agp_tests
        tests/test_main.cpp
        tests/test_geometry.cpp
        tests/test_visibility.cpp
        tests/test_arrangement.cpp
    )
    target_link_libraries(agp_tests
        PRIVATE
            agp_lib
            gtest
            gtest_main
    )

    # Add tests
    include(GoogleTest)
    gtest_discover_tests(agp_tests)
endif()

# Install
install(TARGETS agp_solver DESTINATION bin)
install(TARGETS agp_lib DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

# Add benchmark_driver
add_executable(benchmark_driver examples/benchmark_driver.cpp)
target_link_libraries(benchmark_driver PRIVATE agp_lib)

# Print configuration summary
message(STATUS "")
message(STATUS "=== Art Gallery Problem Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "CGAL version: ${CGAL_VERSION}")
message(STATUS "Boost version: ${Boost_VERSION}")
message(STATUS "Use CPLEX: ${USE_CPLEX}")
message(STATUS "Use GLPK: ${USE_GLPK}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "")
