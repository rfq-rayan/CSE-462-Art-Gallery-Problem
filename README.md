# Art Gallery Solution implementations

This repository bundles two independent **Art Gallery Problem (AGP)** codebases used for coursework and experiments: a modern iterative solver based on Simon’s SoCG 2021 line of work, and a reference implementation aligned with **Tozoni’s 2016** solver and evaluation pipeline. They live in separate directories, build with different toolchains, and are orchestrated only by the optional top-level `run-all.sh` script.

---

## Top-level runner

[`run-all.sh`](run-all.sh) runs one or both project pipelines from the repo root:

| Command | Meaning |
|--------|---------|
| `./run-all.sh` | Run **both** pipelines (default) |
| `./run-all.sh --1` or `--tozoni` | **Tozoni** pipeline only (`tozoni-2016-implementation/scripts/run-all-1.sh`) |
| `./run-all.sh --2` or `--simon` | **Simon** pipeline only (`Simon's iterative algorithm implementation/scripts/run-all-2.sh`) |
| `./run-all.sh --help` | Print usage |

If you select a **single** pipeline, extra arguments are passed through to that script (e.g. a custom `.pol` path for Tozoni, or `--force` for the Simon batch step). See `./run-all.sh --help` for details.

---

## `Simon's iterative algorithm implementation/`

**What it is:** A C++ implementation of the iterative AGP approach from the paper *“The Art Gallery Problem is in P”* (SoCG 2021). It uses CGAL (exact predicates / constructions), integer programming (GLPK by default, optional CPLEX), weak-visibility structures, multiple split protocols, and terminates using vision-stability style criteria.

**What you can do there:**

- **Build** the `agp_solver` binary with CMake (CGAL, GMP, MPFR, Boost, GLPK; see the project [`README.md`](Simon's%20iterative%20algorithm%20implementation/README.md)).
- **Run** the solver on polygon JSON from the command line, or use the **Flask** app under `python/` for upload, drawing, and visualization.
- **Batch evaluation:** `scripts/run_final_json_batch.py` runs the solver on every file in `our_dataset/final_json/` and writes results under `our_dataset/final_output/`.
- **Visualization:** `scripts/visualize_agp_simulation.py` builds a step-by-step PNG series for a default (or configured) instance.
- **Comparison:** `scripts/compare_results.py` compares this solver’s outputs to precomputed **Tozoni** logs in `final_output_tozoni/` (guards, time, iterations), when those files are present.

**Bundled pipeline:** `scripts/run-all-2.sh` runs, in order: batch JSON solve → AGP simulation visualization → `compare_results.py`. Any CLI flags apply only to the batch step (e.g. `--force` to re-run all instances).

For build flags, JSON formats, and algorithm notes, see [`Simon's iterative algorithm implementation/README.md`](Simon's%20iterative%20algorithm%20implementation/README.md).

---

## `tozoni-2016-implementation/`

**What it is:** A self-contained project centered on **Tozoni’s** art-gallery solver and the surrounding **benchmark / visualization** workflow: `.pol` instances, batch runs over witness strategies and solver modes, optional geometry **trace** runs with rendered steps, and final solution **renders**.

**What you can do there:**

- **Build** the `artGallerySolver` executable (see `build.sh` and CMake setup in that tree).
- **Sample instances:** `scripts/make-sample.sh` picks representative `.pol` files into `instances/sample/`.
- **Batch solving:** `scripts/run_final_batch.sh` runs the solver on every `instances/final/*.pol` for multiple witness sets (`ALL_VERTICES`, `CONVEX_VERTICES`, `CHWA_POINTS`, `CHWA_POINTS_EXTENDED`) and modes, writing logs (and related artifacts) under `final_output/`.
- **Trace + render:** `scripts/run_trace_and_render.sh` runs one chosen instance with a detailed trace and produces PNGs for the run.
- **Final visuals:** `scripts/render_final_viz.sh` renders solution visualizations from `final_output/`.

**Bundled pipeline:** `scripts/run-all-1.sh` runs all four stages above in order (sampling → final batch → trace/render for one instance → final viz). An optional first argument overrides the default trace polygon (`.pol` path).

---

## Relationship between the two

- **Different inputs and binaries:** Simon’s tree primarily uses **JSON** polygons and `agp_solver`; Tozoni’s uses **`.pol`** files and `artGallerySolver`.
- **Comparison** of Simon’s runs against Tozoni-style results is done inside the Simon project via `compare_results.py`, which expects Tozoni log layout under `final_output_tozoni/` inside that project—not by running the top-level `run-all.sh` alone.

---

## License

Refer to license files or headers inside each subdirectory if present (the Simon project README cites MIT).
