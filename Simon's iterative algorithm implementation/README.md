# Art Gallery Problem Solver

A complete, faithful implementation of the algorithm from the paper "The Art Gallery Problem is in P" (SoCG 2021).

## Overview

This project implements an iterative algorithm for solving the Art Gallery Problem (AGP). The algorithm finds a minimum set of guards that together see the entire polygon.

### Key Features

- **Exact arithmetic** using CGAL's exact predicates exact constructions kernel
- **Integer Programming** formulation with GLPK (fallback) or CPLEX (primary) solvers
- **Multiple split protocols**: Square, Angular, Reflex chord, Extension, Visibility line
- **Weak Visibility Polygon Tree (WVPT)** for efficient visibility queries
- **Critical witnesses optimization** for faster convergence
- **Vision-stability** detection for termination

## Building

### Prerequisites

- CMake 3.14+
- C++17 compiler
- CGAL 5.0+
- GMP and MPFR
- Boost
- GLPK (default) or CPLEX (optional)

### Build Steps

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### CMake Options

- `-DUSE_CPLEX=ON` - Use CPLEX solver (requires license)
- `-DUSE_GLPK=ON` - Use GLPK solver (default)
- `-DBUILD_TESTS=ON` - Build test suite (default: ON)

## Usage

### Command Line

```bash
./agp_solver input.json -o solution.json -v

Options:
  --help                  Show help message
  --input <file>          Input polygon JSON file
  --output <file>         Output solution JSON file
  --granularity <double>  Initial granularity (default: 1/16)
  --use-cplex             Use CPLEX solver
  --use-glpk              Use GLPK solver (default)
  --max-iterations <n>    Maximum iterations (default: 10000)
  --time-limit <sec>      Time limit in seconds (default: 3600)
  --verbosity <level>     Verbosity level (0=quiet, 1=info, 2=debug)
```

### Input Format

```json
{
  "vertices": [
    {"x": 0, "y": 0},
    {"x": 10, "y": 0},
    {"x": 10, "y": 10},
    {"x": 0, "y": 10}
  ]
}
```

Or as a simple array:

```json
[
  {"x": 0, "y": 0},
  {"x": 10, "y": 0},
  {"x": 10, "y": 10},
  {"x": 0, "y": 10}
]
```

### Output Format

```json
{
  "status": "optimal",
  "num_guards": 1,
  "iterations": 5,
  "solve_time_seconds": 0.123,
  "guards": [
    {"x": 5.0, "y": 5.0}
  ],
  "statistics": {
    "final_candidates": 100,
    "final_witnesses": 100,
    "final_granularity_k": 4,
    "granularity_updates": 0
  }
}
```

## Web Frontend

A Flask-based web frontend is provided for visualization.

### Setup

```bash
cd python
pip install -r requirements.txt
python app.py
```

Then open http://localhost:5000 in your browser.

### Features

- Upload polygon JSON files
- Draw polygons interactively
- Solve and visualize guard placements
- Export solutions

## Algorithm Details

### Iterative Algorithm

1. **Initialize**: Build arrangement A with rays from reflex vertices
2. **Build WVPT**: Construct Weak Visibility Polygon Tree for optimization
3. **Main Loop**:
   - Build IP formulation
   - Solve for minimum guards
   - If optimal (all vertex guards), terminate
   - Otherwise, split faces and repeat
4. **Termination**: When polygon is vision-stable

### IP Formulations

- **Normal IP**: Minimize guards, prefer vertex over face guards
- **Big IP**: Fix guard count, maximize face coverage
- **One-shot IP**: Single iteration with enhanced visibility
- **Simple IP**: Normal IP only, simplified approach

### Split Protocols

1. **Square split**: Divide face with horizontal and vertical lines
2. **Angular split**: Shoot rays from reflex vertex at angular intervals
3. **Reflex chord split**: Connect pairs of reflex vertices
4. **Extension split**: Extend edges incident to reflex vertices
5. **Visibility line split**: Split along visibility boundaries

## Project Structure

```
Alg3/
├── include/core/
│   ├── geometry/
│   │   ├── point.hpp
│   │   ├── segment.hpp
│   │   ├── polygon.hpp
│   │   ├── visibility.hpp
│   │   ├── arrangement.hpp
│   │   └── wvpt.hpp
│   ├── ip/
│   │   ├── ip_formulation.hpp
│   │   └── ip_solver.hpp
│   ├── algorithm/
│   │   ├── iterative.hpp
│   │   ├── splitter.hpp
│   │   └── verifier.hpp
│   └── utils/
│       ├── config.hpp
│       └── logger.hpp
├── src/
│   ├── main.cpp
│   ├── core/geometry/
│   ├── core/ip/
│   ├── core/algorithm/
│   └── core/utils/
├── python/
│   ├── app.py
│   ├── requirements.txt
│   └── templates/
│       └── index.html
├── examples/
│   └── polygons/
├── tests/
├── CMakeLists.txt
└── README.md
```

## References

- "The Art Gallery Problem is in P" - SoCG 2021
- CGAL Documentation: https://doc.cgal.org/
- GLPK Documentation: https://www.gnu.org/software/glpk/

## License

MIT License
