# Modification: Bounding-Box Pre-Filter (Solver Mode 3)

## Overview

A new solver mode (**mode 3**) has been added that accelerates the visibility
matrix construction phase (`initSolverTime`) using an axis-aligned bounding-box
(AABB) pre-filter. Modes 1, 2, and 4 are completely unchanged.

## Motivation

Profiling shows that `initSolverTime` — building the witness × candidate
visibility matrix — is the dominant bottleneck, consuming 60–86 % of total
runtime on larger instances. Inside that phase, the hot operation is
`bounded_side()`, an O(E) point-in-polygon test called for every
(witness, candidate) pair. Many of these calls are wasted: the candidate is
nowhere near the witness's visible region.

## What Changed

### 1. `art-gallery-pg/artGallerySolver.C` — `initSolver()`

**Original (modes 1, 2, 4):**
The outer loop iterates over guard candidates (j), the inner loop over
witnesses (i). For each (i, j) pair, the witness's visibility polygon is
looked up from `_visCache` and **deep-copied** (`PolygonExt visPolCache =
itVis->second`), then `bounded_side()` is called unconditionally.

**Mode 3 (new):**
- **Loop order swapped** — witnesses are in the outer loop, candidates in the
  inner loop. Each witness's visibility polygon is looked up **once** and held
  by `const` reference (zero copies).
- **AABB pre-filter** — before the expensive `bounded_side()` call, a cheap
  `CGAL::do_overlap(candBB[j], visBB)` test checks whether the candidate's
  bounding box overlaps the witness's visibility-polygon bounding box. If they
  don't overlap, the candidate is definitely not visible and the O(E)
  edge-walk is skipped entirely.
- Bounding boxes for all candidates and each witness's visibility polygon are
  pre-computed outside the inner loop.

Both the iteration-1 path (fresh matrix) and the iteration->1 path (cached
`_visibilityTest` lookups) are optimized.

### 2. `art-gallery-pg/artGallerySolver.C` — `addIterationGridPoints()`

Same AABB pre-filter treatment for the inner-loop matrix extension that runs
during the AGPFC horizontal iterations (`stepArt`). Gated behind
`_solverMode == 3`; original code runs for all other modes.

### 3. `pre-solver/PreSolver.C` — `PreSolver::solve()`

Mode 3 previously required the commercial Xpress solver and was a dead code
path when only GLPK is available. It now falls back to GLPK + Lagrangian
heuristic (same SCP strategy as mode 1) when Xpress is not compiled in.

### 4. `INSTALL.txt`

Updated solver-mode documentation to reflect mode 3.

## Correctness

The AABB test is a **conservative over-approximation**: if a point is truly
inside a polygon, it is guaranteed to be inside the polygon's bounding box.
The filter can produce false positives (box overlap but point outside polygon)
but **never** false negatives. Therefore the visibility matrix is identical to
the original — the algorithm produces the same optimal solutions.

## Usage

```bash
# Original algorithm (unchanged)
./artGallerySolver input.pol logfile CHWA_POINTS 1

# Optimized with bbox pre-filter
./artGallerySolver input.pol logfile CHWA_POINTS 3
```

## Measured Improvement (`initSolverTime`)

| Instance             | Candidates | Mode 1    | Mode 3      | Speedup |
|----------------------|------------|-----------|-------------|---------|
| randvon-100-24       | 450        | 0.097 s   | **0.058 s** | 1.7×    |
| randsimple-100-13    | 416        | 0.161 s   | **0.109 s** | 1.5×    |
| randsimple-80-23     | 212        | 0.077 s   | **0.060 s** | 1.3×    |

Guard counts and iteration counts are identical across modes.
The improvement scales with the number of candidates and the geometric
complexity of the polygon (more reflex vertices → smaller, more localized
visibility polygons → higher bbox rejection rate).
