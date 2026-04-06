#!/usr/bin/env python3
"""
Parse AGSOL_TRACE_V1 step_*.txt files (from artGallerySolver with AGSOL_TRACE_DIR set)
and render PNG figures: polygon + shaded visibility regions + missed area + guards.

Also writes overview_area_miss.png across all steps.
"""

from __future__ import annotations

import argparse
import glob
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
import numpy as np
from matplotlib.patches import Polygon as MplPolygon
from matplotlib.collections import PatchCollection
from shapely.geometry import Polygon as ShyPolygon
from pol_sol_io import parse_pol_file


def _read_ring(lines: List[str], idx: int) -> Tuple[List[Tuple[float, float]], int]:
    n = int(lines[idx].strip())
    idx += 1
    pts = []
    for _ in range(n):
        parts = lines[idx].split()
        idx += 1
        pts.append((float(parts[0]), float(parts[1])))
    return pts, idx


@dataclass
class TraceStep:
    path: str
    outer_iteration: int
    inner_iteration: int
    area_miss: float
    guards: List[Tuple[float, float]] = field(default_factory=list)
    visibility_rings: List[List[Tuple[float, float]]] = field(default_factory=list)
    uncovered: List[Tuple[List[Tuple[float, float]], List[List[Tuple[float, float]]]]] = field(
        default_factory=list
    )
    """Each uncovered component: (outer_ring, [hole_rings...])"""


def parse_trace_file(path: str) -> TraceStep:
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        lines = [ln.rstrip() for ln in f.readlines()]
    if not lines or lines[0].strip() != "AGSOL_TRACE_V1":
        raise ValueError(f"Not AGSOL_TRACE_V1: {path}")

    ts = TraceStep(path=path, outer_iteration=0, inner_iteration=0, area_miss=0.0)
    i = 1
    n = len(lines)
    while i < n:
        line = lines[i].strip()
        if not line:
            i += 1
            continue
        if line.startswith("outer_iteration "):
            ts.outer_iteration = int(line.split()[1])
        elif line.startswith("inner_iteration "):
            ts.inner_iteration = int(line.split()[1])
        elif line.startswith("area_miss "):
            ts.area_miss = float(line.split()[1])
        elif line.startswith("num_guards "):
            ng = int(line.split()[1])
            i += 1
            for _ in range(ng):
                if i >= n:
                    break
                parts = lines[i].split()
                i += 1
                if len(parts) >= 3 and parts[0] == "guard":
                    ts.guards.append((float(parts[1]), float(parts[2])))
            continue
        elif line.startswith("num_visibility "):
            nv = int(line.split()[1])
            i += 1
            for _ in range(nv):
                if i >= n or not lines[i].startswith("visibility_polygon "):
                    break
                i += 1
                ring, i = _read_ring(lines, i)
                ts.visibility_rings.append(ring)
            continue
        elif line.startswith("num_uncovered "):
            nu = int(line.split()[1])
            i += 1
            for _ in range(nu):
                if i >= n or not lines[i].startswith("uncovered "):
                    break
                i += 1
                outer, i = _read_ring(lines, i)
                holes: List[List[Tuple[float, float]]] = []
                if i < n and lines[i].startswith("num_holes "):
                    nh = int(lines[i].split()[1])
                    i += 1
                    for _ in range(nh):
                        hr, i = _read_ring(lines, i)
                        holes.append(hr)
                ts.uncovered.append((outer, holes))
            continue
        i += 1

    return ts


def ring_to_mpl(ring: List[Tuple[float, float]]) -> MplPolygon:
    if len(ring) >= 2 and ring[0] == ring[-1]:
        ring = ring[:-1]
    return MplPolygon(np.array(ring), closed=True)


def render_step(
    inst_pol: ShyPolygon,
    step: TraceStep,
    out_png: str,
) -> None:
    fig, ax = plt.subplots(1, 1, figsize=(11, 10))

    # Instance outline
    x, y = inst_pol.exterior.xy
    ax.fill(x, y, facecolor="white", edgecolor="navy", linewidth=2, zorder=1)
    for inter in inst_pol.interiors:
        xi, yi = inter.xy
        ax.fill(xi, yi, facecolor="white", edgecolor="navy", linewidth=2, zorder=2)

    # Visibility polygons (semi-transparent green)
    vis_patches = []
    for ring in step.visibility_rings:
        if len(ring) < 3:
            continue
        try:
            shy = ShyPolygon(ring)
            if not shy.is_valid:
                shy = shy.buffer(0)
            if shy.is_empty:
                continue
            if shy.geom_type == "MultiPolygon":
                for g in shy.geoms:
                    vis_patches.append(MplPolygon(np.array(list(g.exterior.coords)), closed=True))
            else:
                vis_patches.append(MplPolygon(np.array(list(shy.exterior.coords)), closed=True))
        except Exception:
            vis_patches.append(ring_to_mpl(ring))

    if vis_patches:
        pc = PatchCollection(vis_patches, facecolor="limegreen", edgecolor="darkgreen", linewidth=0.4, alpha=0.28, zorder=3)
        ax.add_collection(pc)

    # Uncovered (missed area) — red
    miss_patches = []
    for outer, holes in step.uncovered:
        if len(outer) < 3:
            continue
        try:
            shy = ShyPolygon(outer, holes if holes else None)
            if not shy.is_valid:
                shy = shy.buffer(0)
            if shy.is_empty:
                continue
            geoms = shy.geoms if shy.geom_type == "MultiPolygon" else [shy]
            for g in geoms:
                miss_patches.append(MplPolygon(np.array(list(g.exterior.coords)), closed=True))
                for h in getattr(g, "interiors", []):
                    miss_patches.append(MplPolygon(np.array(h.coords), closed=True))
        except Exception:
            miss_patches.append(ring_to_mpl(outer))

    if miss_patches:
        pc2 = PatchCollection(miss_patches, facecolor="red", edgecolor="darkred", linewidth=0.8, alpha=0.42, zorder=4)
        ax.add_collection(pc2)

    # Guards
    if step.guards:
        gx, gy = zip(*step.guards)
        ax.scatter(gx, gy, c="gold", s=220, zorder=6, marker="*", edgecolors="black", linewidths=0.8, label=f"Guards ({len(step.guards)})")

    ax.set_aspect("equal")
    ax.grid(True, alpha=0.22)
    ax.set_title(
        f"outer_iter={step.outer_iteration}  inner_iter={step.inner_iteration}\n"
        f"area_miss = {step.area_miss:.6g}"
    )
    ax.legend(loc="upper right")
    fig.savefig(out_png, dpi=150, bbox_inches="tight")
    plt.close(fig)


def inst_to_shy(inst) -> ShyPolygon:
    from agp_viz_app import to_shapely_poly

    return to_shapely_poly(inst)


def main() -> int:
    ap = argparse.ArgumentParser(description="Render AGSOL trace step PNGs")
    ap.add_argument("--trace-dir", required=True, help="Directory with step_*.txt from AGSOL_TRACE_DIR")
    ap.add_argument("--pol", required=True, help="Same .pol file used for the solver run")
    ap.add_argument("--out-dir", required=True, help="Output directory for PNGs")
    args = ap.parse_args()

    trace_dir = os.path.abspath(args.trace_dir)
    out_dir = os.path.abspath(args.out_dir)
    os.makedirs(out_dir, exist_ok=True)

    steps = sorted(glob.glob(os.path.join(trace_dir, "step_*.txt")))
    if not steps:
        print(f"No step_*.txt in {trace_dir}", file=sys.stderr)
        return 1

    inst = parse_pol_file(os.path.abspath(args.pol))
    shy_inst = inst_to_shy(inst)
    if not shy_inst.is_valid:
        shy_inst = shy_inst.buffer(0)

    am_vals: List[float] = []
    outer_ids: List[int] = []
    for path in steps:
        st = parse_trace_file(path)
        am_vals.append(st.area_miss)
        outer_ids.append(st.outer_iteration)
        base = os.path.basename(path).replace(".txt", ".png")
        out_png = os.path.join(out_dir, base)
        render_step(shy_inst, st, out_png)
        print(f"Wrote {out_png}")

    # Overview plot (area miss can go up again when a new outer iteration refines witnesses)
    fig, ax = plt.subplots(figsize=(10, 5.5))
    xs = list(range(1, len(am_vals) + 1))
    ax.plot(xs, am_vals, "o-", color="darkred", linewidth=2, markersize=9)
    if len(set(outer_ids)) > 1:
        ax.scatter(xs, am_vals, c=outer_ids, cmap="tab10", s=55, zorder=5, alpha=0.85)
        sm = plt.cm.ScalarMappable(cmap="tab10", norm=plt.Normalize(min(outer_ids), max(outer_ids)))
        sm.set_array([])
        cbar = fig.colorbar(sm, ax=ax)
        cbar.set_label("outer_iteration (from trace)")
    ax.set_xlabel("Trace step index (one file per solveIteration)")
    ax.set_ylabel("Area miss (continuous uncovered area)")
    ax.set_title(
        "Area miss vs inner step — can be non-monotonic when outer iteration advances"
    )
    ax.grid(True, alpha=0.3)
    for i, v in enumerate(am_vals):
        ax.annotate(f"{v:.4g}", (i + 1, v), textcoords="offset points", xytext=(0, 10), ha="center", fontsize=9)
    overview = os.path.join(out_dir, "overview_area_miss.png")
    fig.savefig(overview, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"Wrote {overview}")

    idx_html = os.path.join(out_dir, "index.html")
    with open(idx_html, "w", encoding="utf-8") as f:
        f.write("<!DOCTYPE html><html><head><meta charset='utf-8'><title>Trace steps</title></head><body>\n")
        f.write("<h1>AGSOL trace coverage</h1>\n")
        f.write(f"<p><img src='overview_area_miss.png' style='max-width:900px'/></p>\n")
        for path in sorted(glob.glob(os.path.join(out_dir, "step_*.png"))):
            bn = os.path.basename(path)
            f.write(f"<h2>{bn}</h2><img src='{bn}' style='max-width:900px'/>\n")
        f.write("</body></html>\n")
    print(f"Wrote {idx_html}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
