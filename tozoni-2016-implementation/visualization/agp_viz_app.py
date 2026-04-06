#!/usr/bin/env python3
"""
AGSol (Tozoni) — Art Gallery visualization frontend

Flask web UI + CLI to visualize .pol instances and optional .sol guard placements,
and optionally run tozoni-2016-implementation/build/artGallerySolver.
"""

from __future__ import annotations

import argparse
import base64
import io
import json
import os
import subprocess
import sys
import tempfile
from typing import Any, Dict, List, Optional, Tuple

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from flask import Flask, jsonify, render_template, request
from shapely.geometry import Polygon

from pol_sol_io import ParsedInstance, expected_sol_path, parse_pol_file, parse_sol_file

app = Flask(__name__)

# Default solver relative to this package
_VIZ_DIR = os.path.dirname(os.path.abspath(__file__))
_REPO_TOZONI = os.path.normpath(os.path.join(_VIZ_DIR, ".."))
_DEFAULT_SOLVER = os.path.join(_REPO_TOZONI, "build", "artGallerySolver")


def _solver_binary() -> str:
    return os.environ.get("AGSOLVER", _DEFAULT_SOLVER)


def to_shapely_poly(inst: ParsedInstance) -> Polygon:
    """Build a Shapely polygon with holes from parsed data."""
    shell = list(inst.outer)
    if len(shell) >= 2 and shell[0] == shell[-1]:
        shell = shell[:-1]
    holes = []
    for h in inst.holes:
        ring = list(h)
        if len(ring) >= 2 and ring[0] == ring[-1]:
            ring = ring[:-1]
        holes.append(ring)
    return Polygon(shell, holes)


def visualize_instance(
    inst: ParsedInstance,
    guards: Optional[List[Tuple[float, float]]] = None,
    title: str = "AGSol instance",
    output_path: Optional[str] = None,
) -> str:
    """
    Render polygon (with holes) and optional guard markers.
    Returns base64 PNG if output_path is None, else writes file and returns path.
    """
    poly = to_shapely_poly(inst)
    if not poly.is_valid:
        poly = poly.buffer(0)

    fig, ax = plt.subplots(1, 1, figsize=(10, 10))

    # Exterior
    x, y = poly.exterior.xy
    ax.fill(x, y, color="lightblue", alpha=0.45, zorder=1)
    ax.plot(x, y, "b-", linewidth=2, zorder=2)

    # Holes (unfilled interior)
    for inter in poly.interiors:
        xi, yi = inter.xy
        ax.fill(xi, yi, color="white", zorder=3)
        ax.plot(xi, yi, "b-", linewidth=2, zorder=4)

    vx, vy = zip(*list(poly.exterior.coords)[:-1])
    ax.scatter(vx, vy, c="navy", s=28, zorder=5, label="Vertices")

    if guards:
        gx, gy = zip(*guards)
        ax.scatter(gx, gy, c="red", s=160, zorder=6, marker="*", label=f"Guards ({len(guards)})")
        ax.legend(loc="upper right")

    ax.set_xlabel("x")
    ax.set_ylabel("y")
    ax.set_title(title)
    ax.grid(True, alpha=0.25)
    ax.set_aspect("equal")

    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches="tight")
        plt.close(fig)
        return output_path

    buf = io.BytesIO()
    plt.savefig(buf, format="png", dpi=150, bbox_inches="tight")
    plt.close(fig)
    buf.seek(0)
    return base64.b64encode(buf.read()).decode("utf-8")


def run_solver(
    pol_path: str,
    witness: str = "CHWA_POINTS",
    mode: str = "2",
    timeout_s: int = 600,
) -> Dict[str, Any]:
    """
    Run artGallerySolver; writes .sol next to .pol (same rule as C++ getFileName + cwd).
    """
    solver = _solver_binary()
    if not os.path.isfile(solver) or not os.access(solver, os.X_OK):
        return {"ok": False, "error": f"Solver not found or not executable: {solver}"}

    pol_abs = os.path.abspath(pol_path)
    workdir = os.path.dirname(pol_abs) or "."
    sol_path = expected_sol_path(pol_abs)
    log_fd, log_path = tempfile.mkstemp(suffix="_agp_viz.log", text=True)
    os.close(log_fd)

    env = os.environ.copy()
    build_dir = os.path.dirname(os.path.abspath(solver))
    ld = build_dir + os.pathsep + env.get("LD_LIBRARY_PATH", "")
    env["LD_LIBRARY_PATH"] = ld.rstrip(os.pathsep)

    cmd = [solver, pol_abs, log_path, witness, mode]
    try:
        proc = subprocess.run(
            cmd,
            cwd=workdir,
            env=env,
            capture_output=True,
            text=True,
            timeout=timeout_s,
        )
    except subprocess.TimeoutExpired:
        return {"ok": False, "error": f"Solver exceeded {timeout_s}s"}
    finally:
        if os.path.exists(log_path):
            try:
                os.unlink(log_path)
            except OSError:
                pass

    if proc.returncode != 0:
        return {
            "ok": False,
            "error": "solver failed",
            "returncode": proc.returncode,
            "stderr": proc.stderr[-4000:],
            "stdout": proc.stdout[-4000:],
        }

    if not os.path.isfile(sol_path):
        return {
            "ok": False,
            "error": f"Expected solution file not found: {sol_path}",
            "stdout": proc.stdout[-2000:],
        }

    guards, _ref = parse_sol_file(sol_path)
    return {
        "ok": True,
        "guards": [{"x": g[0], "y": g[1]} for g in guards],
        "count": len(guards),
        "sol_path": sol_path,
    }


# --- Flask ---


@app.route("/")
def index():
    return render_template("index.html", solver_default=_solver_binary())


@app.route("/api/visualize", methods=["POST"])
def api_visualize():
    data = request.get_json(force=True, silent=True) or {}
    pol_text = data.get("pol_text")
    sol_text = data.get("sol_text")
    pol_path = data.get("pol_path")

    try:
        if pol_text:
            tokens = pol_text.split()
            # minimal parse inline
            import tempfile

            fd, tmp = tempfile.mkstemp(suffix=".pol", text=True)
            with os.fdopen(fd, "w") as f:
                f.write(pol_text)
            inst = parse_pol_file(tmp)
            os.unlink(tmp)
        elif pol_path and os.path.isfile(pol_path):
            inst = parse_pol_file(pol_path)
        else:
            return jsonify({"error": "Provide pol_text or valid pol_path"}), 400

        guards = None
        if sol_text:
            fd, tmp = tempfile.mkstemp(suffix=".sol", text=True)
            with os.fdopen(fd, "w") as f:
                f.write(sol_text.strip() + "\n")
            guards, _ = parse_sol_file(tmp)
            os.unlink(tmp)
        elif data.get("sol_path") and os.path.isfile(data["sol_path"]):
            guards, _ = parse_sol_file(data["sol_path"])

        name = os.path.basename(getattr(inst, "source_path", "") or "polygon")
        img = visualize_instance(inst, guards, title=name)
        return jsonify(
            {
                "image": img,
                "guard_count": len(guards) if guards is not None else 0,
            }
        )
    except Exception as e:
        return jsonify({"error": str(e)}), 400


@app.route("/api/solve", methods=["POST"])
def api_solve():
    data = request.get_json(force=True, silent=True) or {}
    pol_path = data.get("pol_path")
    if not pol_path or not os.path.isfile(pol_path):
        return jsonify({"error": "pol_path required"}), 400
    witness = data.get("witness", "CHWA_POINTS")
    mode = str(data.get("solver_mode", "2"))
    res = run_solver(pol_path, witness=witness, mode=mode)
    if not res.get("ok"):
        return jsonify(res), 500
    return jsonify(res)


def run_cli():
    p = argparse.ArgumentParser(description="AGSol polygon / solution visualizer")
    p.add_argument("pol", help="Path to .pol file")
    p.add_argument("--sol", "-s", help="Path to .sol file (optional)")
    p.add_argument("--output", "-o", help="Output PNG path")
    p.add_argument(
        "--solve",
        action="store_true",
        help="Run artGallerySolver first (writes .sol beside .pol)",
    )
    p.add_argument("--witness", default="CHWA_POINTS", help="Witness mode for --solve")
    p.add_argument("--solver-mode", default="2", help="Solver mode for --solve")
    p.add_argument("--port", type=int, default=5000, help="Port for default web server (no pol arg)")
    args = p.parse_args()

    inst = parse_pol_file(args.pol)
    guards = None
    if args.solve:
        res = run_solver(args.pol, witness=args.witness, mode=args.solver_mode)
        if not res.get("ok"):
            print(res.get("error", res), file=sys.stderr)
            sys.exit(1)
        guards = [(g["x"], g["y"]) for g in res["guards"]]
        print(f"Solved: {res['count']} guards -> {res.get('sol_path')}")
    elif args.sol:
        guards, _ = parse_sol_file(args.sol)
    else:
        sol_auto = expected_sol_path(os.path.abspath(args.pol))
        if os.path.isfile(sol_auto):
            guards, _ = parse_sol_file(sol_auto)
            print(f"Using existing solution: {sol_auto}")

    out = args.output
    if not out:
        base = os.path.splitext(os.path.basename(args.pol))[0]
        out = f"{base}_viz.png"

    visualize_instance(
        inst,
        guards,
        title=os.path.basename(args.pol),
        output_path=out,
    )
    print(f"Wrote {out}")


if __name__ == "__main__":
    if len(sys.argv) == 1:
        port = int(os.environ.get("PORT", "5000"))
        print(f"AGSol visualizer — http://127.0.0.1:{port}/")
        print(f"Solver (for /api/solve): {_solver_binary()}")
        app.run(debug=True, host="0.0.0.0", port=port)
    else:
        run_cli()
