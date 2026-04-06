#!/usr/bin/env python3
"""
Build step-by-step figures from a captured artGallerySolver stdout log.

The log records outer iterations, inner SCP steps (Area miss), uncovered region
counts, new witness counts, and final LB/UB. Geometry for intermediate witness
sets is not printed; we visualize polygon + parsed metrics + final .sol.

Usage:
  python3 visualization/step_viz_from_runlog.py \\
    --stdout runs/random-60-2_CHWA_POINTS_2.stdout.txt \\
    --out-dir runs/random-60-2_CHWA_POINTS_2/steps
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

_VIZ_DIR = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.dirname(_VIZ_DIR)
if _VIZ_DIR not in sys.path:
    sys.path.insert(0, _VIZ_DIR)

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

from agp_viz_app import visualize_instance, to_shapely_poly
from pol_sol_io import parse_pol_file, parse_sol_file, expected_sol_path


RE_INSTANCE = re.compile(r"\[ArtGallerySolver\] Instance:\s*(.+)")
RE_OUTER = re.compile(r"\[ArtGallerySolver\] Initiating iteration\s+(\d+)")
RE_AREA = re.compile(r"\[ArtGallerySolver\] Area miss:\s*([0-9.eE+-]+)")
RE_UNC = re.compile(r"\[ArtGallerySolver\] # of uncovered regions:\s*(\d+)")
RE_NEW_W = re.compile(r"\[ArtGallerySolver\]\s+(\d+)\s+new witnesses were chosen")
RE_LB = re.compile(r"\[ArtGallerySolver\] Lower Bound:\s*(\d+)")
RE_UB = re.compile(r"\[ArtGallerySolver\] Upper Bound:\s*(\d+)")
RE_WITNESS = re.compile(r"== Witness Set Option:\s*(\S+)")
RE_MODE = re.compile(r"== Solver Mode Option:\s*(\d+)")
RE_SCP_RESULT = re.compile(r"\[SolverPLIGlpk\] Result:\s*(\d+)")


@dataclass
class InnerStep:
    """One pass through solveIteration() inside runArt(): SCP then geometric area check."""

    outer_iter: int
    scp_cardinality: Optional[int]  # from [SolverPLIGlpk] Result, if seen before Area miss
    area_miss: float
    n_uncovered_regions: Optional[int] = None
    n_new_witnesses: Optional[int] = None


@dataclass
class ParsedRun:
    instance_path: str = ""
    witness: str = ""
    solver_mode: str = ""
    outer_iterations: List[int] = field(default_factory=list)
    area_miss_steps: List[Tuple[int, float]] = field(default_factory=list)
    uncovered_after_area: List[Tuple[int, int]] = field(default_factory=list)
    new_witnesses: List[Tuple[int, int]] = field(default_factory=list)
    bound_snapshots: List[Tuple[int, int, int]] = field(default_factory=list)
    inner_steps: List[InnerStep] = field(default_factory=list)
    _current_outer: int = 0


def parse_stdout(text: str) -> ParsedRun:
    pr = ParsedRun()
    lines = text.splitlines()
    pending_lb: Optional[int] = None
    pending_scp: Optional[int] = None
    for i, line in enumerate(lines):
        if not pr.instance_path:
            m = RE_INSTANCE.search(line)
            if m:
                pr.instance_path = m.group(1).strip()
        m = RE_WITNESS.search(line)
        if m:
            pr.witness = m.group(1).strip()
        m = RE_MODE.search(line)
        if m:
            pr.solver_mode = m.group(1).strip()

        m = RE_OUTER.search(line)
        if m:
            pr._current_outer = int(m.group(1))
            pr.outer_iterations.append(pr._current_outer)

        m = RE_SCP_RESULT.search(line)
        if m:
            pending_scp = int(m.group(1))

        m = RE_AREA.search(line)
        if m:
            am = float(m.group(1))
            pr.area_miss_steps.append((pr._current_outer, am))
            pr.inner_steps.append(
                InnerStep(
                    outer_iter=pr._current_outer,
                    scp_cardinality=pending_scp,
                    area_miss=am,
                )
            )
            pending_scp = None

        m = RE_UNC.search(line)
        if m:
            pr.uncovered_after_area.append((pr._current_outer, int(m.group(1))))
            if pr.inner_steps and pr.inner_steps[-1].n_uncovered_regions is None:
                pr.inner_steps[-1].n_uncovered_regions = int(m.group(1))

        m = RE_NEW_W.search(line)
        if m:
            pr.new_witnesses.append((pr._current_outer, int(m.group(1))))
            if pr.inner_steps and pr.inner_steps[-1].n_new_witnesses is None:
                pr.inner_steps[-1].n_new_witnesses = int(m.group(1))

        m = RE_LB.search(line)
        if m:
            pending_lb = int(m.group(1))
        m = RE_UB.search(line)
        if m and pending_lb is not None:
            pr.bound_snapshots.append((pr._current_outer, pending_lb, int(m.group(1))))
            pending_lb = None

    return pr


def _resolve_pol_path(raw: str) -> str:
    """Log paths are often relative to build/ (e.g. ../instances/final/foo.pol)."""
    p = raw.strip().replace("\\", "/")
    if os.path.isabs(p) and os.path.isfile(p):
        return p
    cand = os.path.normpath(os.path.join(_REPO, p.lstrip("./")))
    if os.path.isfile(cand):
        return cand
    if p.startswith("../"):
        cand2 = os.path.normpath(os.path.join(_REPO, p[3:]))
        if os.path.isfile(cand2):
            return cand2
    base = os.path.basename(p)
    cand3 = os.path.join(_REPO, "instances", "final", base)
    if os.path.isfile(cand3):
        return cand3
    return cand


def main() -> int:
    ap = argparse.ArgumentParser(description="Step figures from solver stdout log")
    ap.add_argument("--stdout", required=True, help="Captured stdout+stderr file")
    ap.add_argument("--out-dir", required=True, help="Directory for PNG outputs")
    ap.add_argument("--pol", default=None, help="Override .pol path")
    ap.add_argument("--sol", default=None, help="Override .sol path")
    args = ap.parse_args()

    with open(args.stdout, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()

    pr = parse_stdout(text)
    pol_path = args.pol or (pr.instance_path and _resolve_pol_path(pr.instance_path))
    if not pol_path or not os.path.isfile(pol_path):
        print("Could not resolve .pol path from log; use --pol", file=sys.stderr)
        return 1

    sol_path = args.sol
    if not sol_path:
        sol_path = expected_sol_path(os.path.abspath(pol_path))
    if not os.path.isfile(sol_path):
        print(f"Warning: no .sol at {sol_path} — final guard figure skipped", file=sys.stderr)

    os.makedirs(args.out_dir, exist_ok=True)

    inst = parse_pol_file(pol_path)
    base_title = os.path.basename(pol_path)

    # 1) Instance only (no guards)
    p01 = os.path.join(args.out_dir, "01_instance_polygon.png")
    visualize_instance(inst, None, title=f"1. Instance\n{base_title}", output_path=p01)
    print(f"Wrote {p01}")

    # 2) Vertices only (geometric anchor; not full CHWA witness set)
    poly = to_shapely_poly(inst)
    if not poly.is_valid:
        poly = poly.buffer(0)
    fig, ax = plt.subplots(figsize=(8, 8))
    x, y = poly.exterior.xy
    ax.fill(x, y, color="lightblue", alpha=0.45)
    ax.plot(x, y, "b-", linewidth=2)
    for inter in poly.interiors:
        xi, yi = inter.xy
        ax.fill(xi, yi, color="white")
        ax.plot(xi, yi, "b-", linewidth=2)
    vx, vy = zip(*list(poly.exterior.coords)[:-1])
    ax.scatter(vx, vy, c="navy", s=35, zorder=5, label="Vertices")
    ax.set_title(
        "2. Boundary vertices only (reference)\n"
        "Not the CHWA witness set — witness points are not printed in this log."
    )
    ax.set_aspect("equal")
    ax.grid(True, alpha=0.25)
    ax.legend(loc="upper right")
    p02 = os.path.join(args.out_dir, "02_polygon_vertices.png")
    fig.savefig(p02, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"Wrote {p02}")

    # 3) Area miss sequence — inner runArt steps (each follows one SCP solve)
    if pr.area_miss_steps:
        xs = list(range(1, len(pr.area_miss_steps) + 1))
        ys = [a[1] for a in pr.area_miss_steps]
        fig, ax = plt.subplots(figsize=(9, 6))
        ax.plot(xs, ys, "o-", color="darkred", linewidth=2, markersize=8)
        ax.set_xlabel("runArt inner step index (each: SCP solve → geometric coverage check)")
        ax.set_ylabel("Area miss (continuous uncovered area)")
        ax.set_title(
            "3. After each SCP: union of guard visibilities vs polygon\n"
            "(Discrete set-cover can be tight while area miss > 0 until witnesses refine.)"
        )
        ax.grid(True, alpha=0.3)
        for i, (x, y) in enumerate(zip(xs, ys)):
            ax.annotate(f"{y:.4g}", (x, y), textcoords="offset points", xytext=(0, 8), ha="center", fontsize=8)
        # annotate SCP cardinality if parsed
        for i, step in enumerate(pr.inner_steps, start=1):
            if step.scp_cardinality is not None:
                ax.annotate(
                    f"|guards|={step.scp_cardinality}",
                    (i, ys[i - 1]),
                    textcoords="offset points",
                    xytext=(12, -14),
                    ha="left",
                    fontsize=8,
                    color="darkgreen",
                )
        p03 = os.path.join(args.out_dir, "03_area_miss_sequence.png")
        fig.savefig(p03, dpi=150, bbox_inches="tight")
        plt.close(fig)
        print(f"Wrote {p03}")

    # 4) Bounds when present
    if pr.bound_snapshots:
        fig, ax = plt.subplots(figsize=(8, 5))
        outers = [b[0] for b in pr.bound_snapshots]
        lbs = [b[1] for b in pr.bound_snapshots]
        ubs = [b[2] for b in pr.bound_snapshots]
        ax.plot(outers, lbs, "s-", label="Lower bound", color="green")
        ax.plot(outers, ubs, "^-", label="Upper bound", color="orange")
        ax.set_xlabel("Outer iteration (from log)")
        ax.set_ylabel("Guard count")
        ax.set_title("4. Bounds when LB/UB were printed")
        ax.legend()
        ax.grid(True, alpha=0.3)
        p04 = os.path.join(args.out_dir, "04_bounds_outer_iterations.png")
        fig.savefig(p04, dpi=150, bbox_inches="tight")
        plt.close(fig)
        print(f"Wrote {p04}")

    # 5) Text summary card (parsed fields + inner-step table)
    fig, ax = plt.subplots(figsize=(11, 7))
    ax.axis("off")
    lines = [
        f"Instance: {pol_path}",
        f"Witness mode: {pr.witness}",
        f"Solver mode: {pr.solver_mode}",
        "",
        "Inner runArt steps (SCP → Area miss → [uncovered regions] → [new witnesses]):",
    ]
    for j, st in enumerate(pr.inner_steps, start=1):
        scp = st.scp_cardinality if st.scp_cardinality is not None else "?"
        ur = st.n_uncovered_regions if st.n_uncovered_regions is not None else "—"
        nw = st.n_new_witnesses if st.n_new_witnesses is not None else "—"
        lines.append(
            f"  step {j}:  SCP |guards|={scp}  →  area_miss={st.area_miss:g}  "
            f" uncovered_regions={ur}  new_witnesses={nw}"
        )
    lines.extend(
        [
            "",
            f"Outer LB/UB (after runArt, when printed): {pr.bound_snapshots}",
        ]
    )
    ax.text(0.02, 0.98, "\n".join(lines), transform=ax.transAxes, va="top", ha="left", family="monospace", fontsize=9)
    ax.set_title("5. Parsed inner steps + bounds (from stdout)")
    p05 = os.path.join(args.out_dir, "05_parsed_log_summary.png")
    fig.savefig(p05, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"Wrote {p05}")

    # 6) Final solution
    if sol_path and os.path.isfile(sol_path):
        guards, _ = parse_sol_file(sol_path)
        p06 = os.path.join(args.out_dir, "06_final_guard_placement.png")
        visualize_instance(
            inst,
            guards,
            title=f"6. Final solution ({len(guards)} guards)\n{base_title}",
            output_path=p06,
        )
        print(f"Wrote {p06}")

    # Companion: how to read stdout (this run)
    summary_path = os.path.join(args.out_dir, "README_steps.txt")
    with open(summary_path, "w", encoding="utf-8") as fp:
        fp.write(
            """How to analyze random-60-2_CHWA_POINTS_2.stdout.txt
================================================

1) HEADER (lines 1–8)
   Instance path, witness mode (CHWA_POINTS), solver mode (2).

2) ONE OUTER ITERATION: "Initiating iteration 1"
   The main loop builds witnesses, candidates, then calls runArt().

3) BLOCK "Running solver..." / runArt INNER LOOP
   Each cycle is:
     A) SCP (GLPK): set cover on discrete witnesses → binary solution.
        [PreSolver] shrinks candidates/constraints; GLPK obj / mip value is the
        number of selected guards (e.g. 8), NOT polygon area.
     B) [SolverPLIGlpk] Result: k   ← SCP cardinality for this pass.
     C) [ArtGallerySolver] Area miss: α   ← CONTINUOUS check: area of P \\ (∪ visibility(guards)).
        If α > 0, discrete witnesses were insufficient to certify full coverage.
     D) # of uncovered regions, then "N new witnesses were chosen" (refinement).
     E) If area miss > 0, the loop repeats with more witnesses/constraints.

   In this log: two inner steps — first Area miss ≈ 1.63 (3 regions, 3 new witnesses),
   second Area miss = 0. Both SCP results show 8 guards; the second pass closes the gap.

4) AFTER runArt: "Comparing upper bound with lower bound"
   Lower Bound / Upper Bound refer to proof on guard COUNT (here both 8) → optimal.

5) WHAT THE LOG DOES NOT CONTAIN
   Coordinates of witness points or candidate guards (only counts and GLPK traces).

Figures:
  01 — Input polygon.
  02 — Boundary vertices only (not CHWA witnesses).
  03 — Area miss vs runArt inner step; annotations show |guards| from [SolverPLIGlpk] Result.
  04 — LB/UB plot (single point if one outer iteration).
  05 — Parsed inner-step table.
  06 — Final guard positions from .sol (written beside .pol by the binary).
"""
        )
    print(f"Wrote {summary_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
