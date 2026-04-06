#!/usr/bin/env python3
"""
Generate a step-by-step PNG series for the AGP iterative algorithm (conceptual + data-driven).

Default case: randsimple-40-7 (40 vertices, instance id 7 in the randsimple family).

The C++ solver does not export arrangement geometry to JSON; this script reconstructs:
  - polygon + reflex/convex classification
  - axis-aligned rays from reflex vertices (matching the solver's add_horizontal_vertical_rays)
  - final guards and approximate visibility (segment-in-polygon checks to vertices)

Usage:
  python3 scripts/visualize_agp_simulation.py
  python3 scripts/visualize_agp_simulation.py --out-dir /tmp/agp_sim
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import matplotlib.pyplot as plt
from shapely.geometry import LineString, Point, Polygon as ShpPolygon


def load_polygon(path: Path) -> list[tuple[float, float]]:
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    if isinstance(data, list):
        data = data[0]
    verts = data["vertices"]
    return [(float(v["x"]), float(v["y"])) for v in verts]


def load_solution(path: Path | None) -> dict | None:
    if path is None or not path.exists():
        return None
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def signed_area(verts: list[tuple[float, float]]) -> float:
    a = 0.0
    n = len(verts)
    for i in range(n):
        x1, y1 = verts[i]
        x2, y2 = verts[(i + 1) % n]
        a += (x2 - x1) * (y2 + y1)
    return a


def classify_vertices(verts: list[tuple[float, float]]) -> tuple[list[int], list[int]]:
    n = len(verts)
    ccw = signed_area(verts) < 0
    reflex: list[int] = []
    convex: list[int] = []
    for i in range(n):
        p = verts[(i - 1) % n]
        q = verts[i]
        r = verts[(i + 1) % n]
        cross = (q[0] - p[0]) * (r[1] - q[1]) - (q[1] - p[1]) * (r[0] - q[0])
        is_reflex = cross > 0 if ccw else cross < 0
        (reflex if is_reflex else convex).append(i)
    return reflex, convex


def first_ray_hit(
    poly: ShpPolygon, ox: float, oy: float, ux: float, uy: float
) -> tuple[float, float] | None:
    """First intersection of open ray from (ox,oy) in direction (ux,uy) with polygon boundary."""
    L = 1e7
    line = LineString([(ox, oy), (ox + ux * L, oy + uy * L)])
    b = poly.boundary
    inter = line.intersection(b)
    if inter.is_empty:
        return None

    def collect_points(geom) -> list[tuple[float, float]]:
        out: list[tuple[float, float]] = []
        if geom.geom_type == "Point":
            out.append((geom.x, geom.y))
        elif geom.geom_type == "MultiPoint":
            for g in geom.geoms:
                out.append((g.x, g.y))
        elif geom.geom_type == "LineString":
            for c in geom.coords:
                out.append((float(c[0]), float(c[1])))
        elif geom.geom_type == "MultiLineString":
            for ls in geom.geoms:
                for c in ls.coords:
                    out.append((float(c[0]), float(c[1])))
        elif geom.geom_type == "GeometryCollection":
            for g in geom.geoms:
                out.extend(collect_points(g))
        return out

    pts = collect_points(inter)
    best = None
    best_t = None
    for x, y in pts:
        t = (x - ox) * ux + (y - oy) * uy
        if t > 1e-7:
            if best_t is None or t < best_t:
                best_t = t
                best = (x, y)
    return best


def setup_axes(ax, verts: list[tuple[float, float]], title: str) -> None:
    xs = [v[0] for v in verts]
    ys = [v[1] for v in verts]
    pad_x = (max(xs) - min(xs)) * 0.08 + 0.5
    pad_y = (max(ys) - min(ys)) * 0.08 + 0.5
    ax.set_xlim(min(xs) - pad_x, max(xs) + pad_x)
    ax.set_ylim(min(ys) - pad_y, max(ys) + pad_y)
    ax.set_aspect("equal")
    ax.set_title(title, fontsize=12)
    ax.grid(True, alpha=0.25)


def draw_polygon(ax, verts: list[tuple[float, float]], **kwargs) -> None:
    xs = [v[0] for v in verts] + [verts[0][0]]
    ys = [v[1] for v in verts] + [verts[0][1]]
    ax.fill(xs, ys, **{"alpha": 0.35, "color": "steelblue", **kwargs})
    ax.plot(xs, ys, color="navy", linewidth=1.8)


def save_fig(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    plt.tight_layout()
    plt.savefig(path, dpi=160, bbox_inches="tight", facecolor="white")
    plt.close()


def frame_01_polygon(out: Path, verts: list[tuple[float, float]]) -> None:
    fig, ax = plt.subplots(figsize=(10, 9))
    draw_polygon(ax, verts)
    setup_axes(ax, verts, "Step 1 — Input polygon")
    ax.text(
        0.02,
        0.98,
        "Boundary walk (CCW)\nSimple polygon",
        transform=ax.transAxes,
        fontsize=11,
        verticalalignment="top",
        bbox=dict(boxstyle="round", facecolor="wheat", alpha=0.9),
    )
    save_fig(out)


def frame_02_reflex(out: Path, verts: list[tuple[float, float]], reflex: list[int]) -> None:
    fig, ax = plt.subplots(figsize=(10, 9))
    draw_polygon(ax, verts)
    conv = [i for i in range(len(verts)) if i not in set(reflex)]
    ax.scatter(
        [verts[i][0] for i in conv],
        [verts[i][1] for i in conv],
        c="tab:blue",
        s=38,
        zorder=5,
        label=f"Convex ({len(conv)})",
    )
    ax.scatter(
        [verts[i][0] for i in reflex],
        [verts[i][1] for i in reflex],
        c="tab:orange",
        s=55,
        zorder=6,
        label=f"Reflex ({len(reflex)})",
    )
    ax.legend(loc="lower right")
    setup_axes(ax, verts, "Step 2 — Vertex classification (interior angle)")
    save_fig(out)


def frame_03_rays(out: Path, verts: list[tuple[float, float]], reflex: list[int]) -> None:
    poly = ShpPolygon(verts)
    fig, ax = plt.subplots(figsize=(10, 9))
    draw_polygon(ax, verts)
    dirs = [(1.0, 0.0), (-1.0, 0.0), (0.0, 1.0), (0.0, -1.0)]
    for ri in reflex:
        ox, oy = verts[ri]
        for ux, uy in dirs:
            hit = first_ray_hit(poly, ox, oy, ux, uy)
            if hit:
                hx, hy = hit
                ax.plot([ox, hx], [oy, hy], color="darkgreen", linewidth=0.7, alpha=0.65)
        ax.scatter([ox], [oy], c="tab:orange", s=40, zorder=5)
    setup_axes(
        ax,
        verts,
        "Step 3 — Axis-aligned rays from reflex vertices (4 per reflex, as in solver)",
    )
    ax.text(
        0.02,
        0.98,
        "Rays stop at first boundary hit.\nRefines the arrangement.",
        transform=ax.transAxes,
        fontsize=10,
        verticalalignment="top",
        bbox=dict(boxstyle="round", facecolor="honeydew", alpha=0.92),
    )
    save_fig(out)


def frame_04_wvpt_note(out: Path, verts: list[tuple[float, float]]) -> None:
    fig, ax = plt.subplots(figsize=(10, 9))
    draw_polygon(ax, verts)
    setup_axes(ax, verts, "Step 4 — WVPT defining chords (conceptual)")
    ax.text(
        0.5,
        0.55,
        "Weak Visibility Polygon Tree (WVPT)\n"
        "• Adds defining chords into the arrangement\n"
        "• Groups regions for critical-witness sampling\n\n"
        "(Exact chord geometry is computed in C++; not in JSON export)",
        transform=ax.transAxes,
        ha="center",
        va="center",
        fontsize=11,
        bbox=dict(boxstyle="round", facecolor="lightyellow", alpha=0.95),
    )
    save_fig(out)


def frame_05_stats(out: Path, verts: list[tuple[float, float]], sol: dict | None) -> None:
    fig, ax = plt.subplots(figsize=(10, 9))
    draw_polygon(ax, verts)
    setup_axes(ax, verts, "Step 5 — Integer program (after initial arrangement)")
    st = (sol or {}).get("statistics") or {}
    lines = [
        "IP formulation:",
        "  • Binary variables: vertex candidates + face (centroid) candidates",
        "  • Witnesses: one per arrangement face (+ vertex witnesses)",
        "",
    ]
    if sol:
        lines.extend(
            [
                f"From solver output for this instance:",
                f"  Candidates (final): {st.get('final_candidates', '?')}",
                f"  Witnesses (final):  {st.get('final_witnesses', '?')}",
                f"  Granularity k:      {st.get('final_granularity_k', '?')}   (λ = 2⁻ᵏ)",
                f"  Iterations:         {sol.get('iterations', '?')}",
                f"  Status:             {sol.get('status', '?')}",
            ]
        )
    else:
        lines.append("No solution JSON — run the C++ solver with --output to fill stats.")
    ax.text(
        0.5,
        0.5,
        "\n".join(lines),
        transform=ax.transAxes,
        ha="center",
        va="center",
        fontsize=10,
        family="monospace",
        bbox=dict(boxstyle="round", facecolor="aliceblue", alpha=0.96),
    )
    save_fig(out)


def segment_in_polygon(poly: ShpPolygon, p: tuple[float, float], q: tuple[float, float]) -> bool:
    seg = LineString([p, q])
    if seg.length < 1e-12:
        return poly.covers(Point(p))
    return poly.covers(seg)


def frame_06_guards(out: Path, verts: list[tuple[float, float]], sol: dict | None) -> None:
    fig, ax = plt.subplots(figsize=(10, 9))
    draw_polygon(ax, verts)
    setup_axes(ax, verts, "Step 6 — Optimal guard placement (vertex guards)")
    if not sol or not sol.get("guards"):
        ax.text(
            0.5,
            0.5,
            "No guards in solution JSON.",
            transform=ax.transAxes,
            ha="center",
            fontsize=12,
        )
        save_fig(out)
        return
    guards = sol["guards"]
    gx = [g["x"] for g in guards]
    gy = [g["y"] for g in guards]
    ax.scatter(gx, gy, c="crimson", s=220, marker="*", zorder=8, edgecolors="darkred", linewidths=0.8)
    for k, g in enumerate(guards):
        ax.annotate(
            f"G{k+1}",
            (g["x"], g["y"]),
            textcoords="offset points",
            xytext=(6, 6),
            fontsize=10,
            fontweight="bold",
            color="darkred",
        )
    save_fig(out)


def frame_07_visibility(out: Path, verts: list[tuple[float, float]], sol: dict | None) -> None:
    poly = ShpPolygon(verts)
    fig, ax = plt.subplots(figsize=(10, 9))
    draw_polygon(ax, verts)
    setup_axes(ax, verts, "Step 7 — Coverage check (segment inside polygon → visible)")
    if not sol or not sol.get("guards"):
        save_fig(out)
        return
    for g in sol["guards"]:
        gp = (g["x"], g["y"])
        for v in verts:
            if segment_in_polygon(poly, gp, v):
                ax.plot(
                    [gp[0], v[0]],
                    [gp[1], v[1]],
                    color="crimson",
                    alpha=0.12,
                    linewidth=0.6,
                )
    for g in sol["guards"]:
        ax.scatter(
            [g["x"]],
            [g["y"]],
            c="crimson",
            s=200,
            marker="*",
            zorder=8,
            edgecolors="darkred",
        )
    ax.text(
        0.02,
        0.98,
        "Dashed-style lines: guard → vertex when the segment lies in P.\n"
        "Full face coverage uses exact 'sees_completely' in the solver.",
        transform=ax.transAxes,
        fontsize=9,
        verticalalignment="top",
        bbox=dict(boxstyle="round", facecolor="mistyrose", alpha=0.9),
    )
    save_fig(out)


def frame_08_summary(out: Path, verts: list[tuple[float, float]], sol: dict | None, reflex: list[int]) -> None:
    fig, ax = plt.subplots(figsize=(10, 9))
    draw_polygon(ax, verts)
    if sol and sol.get("guards"):
        for g in sol["guards"]:
            ax.scatter(
                [g["x"]],
                [g["y"]],
                c="crimson",
                s=200,
                marker="*",
                zorder=8,
            )
    setup_axes(ax, verts, "Summary — randsimple-40-7")
    st = (sol or {}).get("statistics") or {}
    msg = [
        f"Vertices: {len(verts)}  |  Reflex: {len(reflex)}",
        f"Guards: {sol.get('num_guards', '?') if sol else '?'}  |  Iterations: {sol.get('iterations', '?') if sol else '?'}",
        f"Solve time: {sol.get('solve_time_seconds', '?') if sol else '?'} s",
        f"Candidates / witnesses: {st.get('final_candidates', '?')} / {st.get('final_witnesses', '?')}",
    ]
    ax.text(
        0.5,
        0.08,
        "\n".join(msg),
        transform=ax.transAxes,
        ha="center",
        va="bottom",
        fontsize=10,
        bbox=dict(boxstyle="round", facecolor="white", alpha=0.92),
    )
    save_fig(out)


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description="AGP step-by-step simulation PNG series")
    parser.add_argument(
        "--polygon",
        type=Path,
        default=root / "our_dataset" / "final_json" / "randsimple-40-7.json",
        help="Polygon JSON (vertices)",
    )
    parser.add_argument(
        "--solution",
        type=Path,
        default=root / "our_dataset" / "final_output" / "randsimple-40-7_output.json",
        help="Solver output JSON (optional)",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=root / "our_dataset" / "final_output" / "simulation_randsimple_40_7",
        help="Directory for PNG sequence",
    )
    args = parser.parse_args()

    if not args.polygon.exists():
        print(f"Missing polygon file: {args.polygon}", file=sys.stderr)
        return 1

    verts = load_polygon(args.polygon)
    sol = load_solution(args.solution)
    reflex, _ = classify_vertices(verts)

    out_dir = args.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    sequence = [
        ("01_polygon.png", lambda: frame_01_polygon(out_dir / "01_polygon.png", verts)),
        ("02_reflex_convex.png", lambda: frame_02_reflex(out_dir / "02_reflex_convex.png", verts, reflex)),
        ("03_reflex_rays.png", lambda: frame_03_rays(out_dir / "03_reflex_rays.png", verts, reflex)),
        ("04_wvpt_note.png", lambda: frame_04_wvpt_note(out_dir / "04_wvpt_note.png", verts)),
        ("05_ip_stats.png", lambda: frame_05_stats(out_dir / "05_ip_stats.png", verts, sol)),
        ("06_guards.png", lambda: frame_06_guards(out_dir / "06_guards.png", verts, sol)),
        ("07_visibility.png", lambda: frame_07_visibility(out_dir / "07_visibility.png", verts, sol)),
        ("08_summary.png", lambda: frame_08_summary(out_dir / "08_summary.png", verts, sol, reflex)),
    ]

    for name, fn in sequence:
        fn()
        print(f"Wrote {out_dir / name}")

    print(f"\nDone. Open images in: {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
