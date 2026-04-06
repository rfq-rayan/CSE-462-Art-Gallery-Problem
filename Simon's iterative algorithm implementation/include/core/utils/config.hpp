#pragma once

#include <chrono>
#include <string>

namespace agp {
namespace config {

//=============================================================================
// Granularity Parameters
//=============================================================================

// Initial granularity λ = 1/16 (k = 4)
constexpr double INITIAL_GRANULARITY = 1.0 / 16.0;

// Minimum granularity λ = 1/4096 (k = 12)
constexpr double MIN_GRANULARITY = 1.0 / 4096.0;

// Granularity divisor for updates
constexpr int GRANULARITY_DIVISOR = 2;

//=============================================================================
// IP Solver Configuration
//=============================================================================

enum class SolverType {
    CPLEX,
    GLPK
};

// Default solver
#ifdef USE_CPLEX
constexpr SolverType DEFAULT_SOLVER = SolverType::CPLEX;
#else
constexpr SolverType DEFAULT_SOLVER = SolverType::GLPK;
#endif

//=============================================================================
// Critical Witnesses Configuration
//=============================================================================

// Use critical witnesses optimization
constexpr bool USE_CRITICAL_WITNESSES = true;

// Initial fraction of witnesses to mark as critical (10%)
constexpr double INITIAL_CRITICAL_FRACTION = 0.1;

// Use delayed critical witness protocol
constexpr bool DELAYED_CRITICAL_WITNESS_PROTOCOL = false;

//=============================================================================
// Split Protocol Probabilities
//=============================================================================

// Probability of angular split
constexpr double ANGULAR_SPLIT_PROB = 0.6;

// Probability of visibility line split
constexpr double VISIBILITY_LINE_SPLIT_PROB = 0.2;

// Probability of reflex chord split
constexpr double REFLEX_CHORD_SPLIT_PROB = 0.1;

// Probability of extension split
constexpr double EXTENSION_SPLIT_PROB = 0.1;

// Always try square split when face is approximately square
constexpr double SQUARE_SPLIT_THRESHOLD = 0.1; // Aspect ratio threshold

//=============================================================================
// Algorithm Limits
//=============================================================================

// Maximum number of iterations
constexpr int MAX_ITERATIONS = 10000;

// Time limit in seconds
constexpr double TIME_LIMIT_SECONDS = 3600.0;

// Granularity update threshold (milliseconds)
constexpr int GRANULARITY_UPDATE_THRESHOLD_MS = 10;

//=============================================================================
// Verification Parameters
//=============================================================================

// Grid spacing for solution verification
constexpr double VERIFICATION_GRID_SPACING = 0.01;
constexpr double VERIFICATION_grid_spacing = 0.01; // Alias

// Use safe guards (verify each guard placement)
constexpr bool USE_SAFE_GUARDS = true;

//=============================================================================
// Numerical Tolerance
//=============================================================================

// Epsilon for floating point comparisons
constexpr double EPSILON = 1e-9;

// Tolerance for point containment checks
constexpr double CONTAINMENT_TOLERANCE = 1e-10;

//=============================================================================
// Logging Configuration
//=============================================================================

// Enable debug logging
constexpr bool ENABLE_DEBUG_LOGGING = true;

// Log visibility computations
constexpr bool LOG_VISIBILITY = true;

// Log IP formulation details
constexpr bool LOG_IP_DETAILS = true;

// Log split operations
constexpr bool LOG_SPLIT_OPERATIONS = true;

//=============================================================================
// WVPT Configuration
//=============================================================================

// Maximum WVPT tree depth
constexpr int MAX_WVPT_DEPTH = 100;

// Minimum polygon vertices for WVPT construction
constexpr int MIN_VERTICES_FOR_WVPT = 4;

//=============================================================================
// Arrangement Configuration
//=============================================================================

// Maximum number of faces in arrangement
constexpr size_t MAX_ARRANGEMENT_FACES = 1000000;

// Maximum number of edges in arrangement
constexpr size_t MAX_ARRANGEMENT_EDGES = 2000000;

//=============================================================================
// Visibility Configuration
//=============================================================================

// Use WVPT for visibility optimization
constexpr bool USE_WVPT_VISIBILITY = true;

// Cache visibility results
constexpr bool CACHE_VISIBILITY = true;

// Maximum visibility cache size
constexpr size_t MAX_VISIBILITY_CACHE_SIZE = 100000;

//=============================================================================
// Output Configuration
//=============================================================================

// Output directory for visualizations
const std::string OUTPUT_DIRECTORY = "output";

// Visualization file format
const std::string VISUALIZATION_FORMAT = "svg";

// Export solution to JSON
constexpr bool EXPORT_SOLUTION_JSON = true;

// Export arrangement to file
constexpr bool EXPORT_ARRANGEMENT = false;

} // namespace config
} // namespace agp
