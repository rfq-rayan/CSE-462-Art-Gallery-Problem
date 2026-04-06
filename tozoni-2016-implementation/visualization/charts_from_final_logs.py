#!/usr/bin/env python3
"""
Build charts from final_output/*.log (writeLog format). Skips *.log.par.

Outputs (default out-dir = <final-dir>/charts):
  01_runtime_by_witness_mode.png   (box plots)
  02_guards_by_witness_mode.png
  04_polsize_vs_totaltime.png        (scatter)
  05_witness_cand_vs_time.png
  06_time_breakdown_stacked.png
  07_summary_table.png
  08_solver_mode_timing.png
  solver_mode_timing_comparison.csv
  summary_stats.csv
"""

from __future__ import annotations

import argparse
import csv
import glob
import os
import re
import statistics
import sys
from collections import defaultdict
from dataclasses import dataclass
from itertools import combinations
from typing import Dict, List, Optional, Tuple

_VIZ_DIR = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.dirname(_VIZ_DIR)
if _VIZ_DIR not in sys.path:
    sys.path.insert(0, _VIZ_DIR)

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

LOG_NAME = re.compile(
    r"^(.+)__(ALL_VERTICES|CONVEX_VERTICES|CHWA_POINTS|CHWA_POINTS_EXTENDED)__([123])\.log$"
)

WITNESSES = ["ALL_VERTICES", "CONVEX_VERTICES", "CHWA_POINTS", "CHWA_POINTS_EXTENDED"]

MODE_LABELS = {
    1: "Mode 1\nGLPK+heuristic",
    2: "Mode 2\nGLPK only",
    3: "Mode 3\nModified"
}

MODE_LABELS_SHORT = {
    1: "GLPK+heur.",
    2: "GLPK",
    3: "Modified"
}


@dataclass
class LogRow:
    id_file: str
    pol_size: int
    guards: int
    iterations: int
    init_witn_size: int
    final_witn_size: int
    init_cand_size: int
    final_cand_size: int
    max_horizontal: int
    ip_solved: int
    init_dis_time: float
    insert_dis_time: float
    select_cand_time: float
    init_solver_time: float
    solver_time: float
    scp_resol_time: float
    new_dis_time: float
    total_time: float
    basename: str
    witness: str
    solver_mode: int


def parse_log_filename(name: str) -> Optional[Tuple[str, str, int]]:
    m = LOG_NAME.match(name)
    if not m:
        return None
    return m.group(1), m.group(2), int(m.group(3))


def parse_data_line(line: str) -> Optional[Tuple[List[str], List[str]]]:
    parts = line.split()
    if len(parts) < 18:
        return None
    tail = parts[-17:]
    id_parts = parts[:-17]
    return id_parts, tail


def row_from_tail(
    id_parts: List[str], tail: List[str], base: str, witness: str, mode: int
) -> LogRow:
    return LogRow(
        id_file=" ".join(id_parts) if id_parts else "",
        pol_size=int(tail[0]),
        guards=int(tail[1]),
        iterations=int(tail[2]),
        init_witn_size=int(tail[3]),
        final_witn_size=int(tail[4]),
        init_cand_size=int(tail[5]),
        final_cand_size=int(tail[6]),
        max_horizontal=int(tail[7]),
        ip_solved=int(tail[8]),
        init_dis_time=float(tail[9]),
        insert_dis_time=float(tail[10]),
        select_cand_time=float(tail[11]),
        init_solver_time=float(tail[12]),
        solver_time=float(tail[13]),
        scp_resol_time=float(tail[14]),
        new_dis_time=float(tail[15]),
        total_time=float(tail[16]),
        basename=base,
        witness=witness,
        solver_mode=mode,
    )


def load_rows(final_dir: str) -> List[LogRow]:
    rows: List[LogRow] = []
    pattern = os.path.join(final_dir, "*.log")
    for path in sorted(glob.glob(pattern)):
        if path.endswith(".log.par"):
            continue
        bn = os.path.basename(path)
        parsed = parse_log_filename(bn)
        if not parsed:
            continue
        base, witness, mode = parsed
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            lines = [ln.strip() for ln in f.readlines() if ln.strip()]
        if len(lines) < 2:
            continue
        pl = parse_data_line(lines[1])
        if not pl:
            continue
        id_parts, tail = pl
        rows.append(row_from_tail(id_parts, tail, base, witness, mode))
    return rows


def _get_modes(rows: List[LogRow]) -> List[int]:
    return sorted(set(r.solver_mode for r in rows))


def chart_01_02_box(
    rows: List[LogRow],
    out_path: str,
    *,
    field: str,
    title: str,
    ylabel: str,
) -> None:
    modes = _get_modes(rows)
    ncols = len(modes)
    fig, axes = plt.subplots(1, ncols, figsize=(6 * ncols, 5), sharey=True, squeeze=False)
    for ax, mode in zip(axes[0], modes):
        data = []
        labels = []
        for w in WITNESSES:
            vals = [getattr(r, field) for r in rows if r.witness == w and r.solver_mode == mode]
            if vals:
                data.append(vals)
                labels.append(w.replace("_", "\n"))
        if not data:
            ax.set_visible(False)
            continue
        ax.boxplot(data, tick_labels=labels, showmeans=True)
        ax.set_title(MODE_LABELS.get(mode, f"Mode {mode}"))
        ax.set_ylabel(ylabel)
        ax.tick_params(axis="x", labelsize=7)
        ax.grid(True, axis="y", alpha=0.3)
    fig.suptitle(title)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def chart_04_scatter(rows: List[LogRow], out_path: str) -> None:
    modes = _get_modes(rows)
    colors = plt.cm.tab10(np.linspace(0, 0.9, len(WITNESSES)))
    w_to_c = {w: colors[i] for i, w in enumerate(WITNESSES)}
    ncols = len(modes)
    fig, axes = plt.subplots(1, ncols, figsize=(6 * ncols, 5), sharey=True, squeeze=False)
    for ax, mode in zip(axes[0], modes):
        for w in WITNESSES:
            pts = [(r.pol_size, r.total_time) for r in rows if r.witness == w and r.solver_mode == mode]
            if not pts:
                continue
            xs, ys = zip(*pts)
            ax.scatter(xs, ys, c=[w_to_c[w]], label=w, alpha=0.75, s=36)
        ax.set_xlabel("polSize")
        ax.set_ylabel("totalTime")
        ax.set_title(MODE_LABELS.get(mode, f"Mode {mode}"))
        ax.grid(True, alpha=0.25)
        ax.legend(fontsize=7, loc="upper left")
    fig.suptitle("4. polSize vs totalTime (color = witness mode)")
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def chart_05_scatter(rows: List[LogRow], out_path: str) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    for ax, (xfield, xlabel) in zip(
        axes,
        [("final_witn_size", "finalWitnSize"), ("final_cand_size", "finalCandSize")],
    ):
        for w in WITNESSES:
            pts = [
                (getattr(r, xfield), r.total_time)
                for r in rows
                if r.witness == w
            ]
            if not pts:
                continue
            xs, ys = zip(*pts)
            ax.scatter(xs, ys, label=w, alpha=0.65, s=30)
        ax.set_xlabel(xlabel)
        ax.set_ylabel("totalTime")
        ax.legend(fontsize=7)
        ax.grid(True, alpha=0.25)
    fig.suptitle("5. Witness/candidate set size vs totalTime (all modes)")
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


TIME_KEYS = [
    ("init_dis_time", "initDis"),
    ("insert_dis_time", "insertDis"),
    ("select_cand_time", "selectCand"),
    ("init_solver_time", "initSolver"),
    ("solver_time", "solver"),
    ("scp_resol_time", "scpResol"),
    ("new_dis_time", "newDis"),
]


def chart_06_stacked(rows: List[LogRow], out_path: str) -> None:
    modes = _get_modes(rows)
    groups: List[Tuple[str, int]] = []
    for w in WITNESSES:
        for m in modes:
            groups.append((w, m))

    n_groups = len(groups)
    bottoms = np.zeros(n_groups)
    fig, ax = plt.subplots(figsize=(max(14, 4 * len(modes)), 6))
    x = np.arange(n_groups)
    labels = [f"{w[:4]}…\nm{m}" for w, m in groups]

    for attr, short in TIME_KEYS:
        heights = []
        for w, m in groups:
            subset = [getattr(r, attr) for r in rows if r.witness == w and r.solver_mode == m]
            heights.append(statistics.mean(subset) if subset else 0.0)
        heights = np.array(heights)
        ax.bar(x, heights, bottom=bottoms, label=short, width=0.85)
        bottoms = bottoms + heights

    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=7, rotation=0)
    ax.set_ylabel("Mean time (s), stacked")
    ax.set_title("6. Mean time breakdown by witness × solver mode (stacked)")
    ax.legend(loc="upper left", ncol=4, fontsize=8)
    ax.grid(True, axis="y", alpha=0.25)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def write_summary_csv(rows: List[LogRow], out_path: str) -> None:
    by_wm: Dict[Tuple[str, int], List[LogRow]] = defaultdict(list)
    for r in rows:
        by_wm[(r.witness, r.solver_mode)].append(r)

    with open(out_path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(
            [
                "witness",
                "solver_mode",
                "n",
                "mean_totalTime",
                "median_totalTime",
                "mean_guards",
                "median_guards",
                "mean_iterations",
            ]
        )
        modes = _get_modes(rows)
        for witness in WITNESSES:
            for mode in modes:
                key = (witness, mode)
                rs = by_wm.get(key, [])
                if not rs:
                    continue
                tt = [r.total_time for r in rs]
                gg = [r.guards for r in rs]
                it = [r.iterations for r in rs]
                w.writerow(
                    [
                        witness,
                        mode,
                        len(rs),
                        f"{statistics.mean(tt):.6g}",
                        f"{statistics.median(tt):.6g}",
                        f"{statistics.mean(gg):.6g}",
                        f"{statistics.median(gg):.6g}",
                        f"{statistics.mean(it):.6g}",
                    ]
                )


def chart_07_table(rows: List[LogRow], out_path: str) -> None:
    by_wm: Dict[Tuple[str, int], List[LogRow]] = defaultdict(list)
    for r in rows:
        by_wm[(r.witness, r.solver_mode)].append(r)

    modes = _get_modes(rows)
    fig, ax = plt.subplots(figsize=(14, max(4, 1 + len(WITNESSES) * len(modes) * 0.5)))
    ax.axis("off")
    cells = []
    header = [
        "witness",
        "mode",
        "n",
        "mean totalTime",
        "median totalTime",
        "mean guards",
        "median guards",
        "mean iter",
    ]
    cells.append(header)
    for witness in WITNESSES:
        for mode in modes:
            rs = by_wm.get((witness, mode), [])
            if not rs:
                continue
            tt = [r.total_time for r in rs]
            gg = [r.guards for r in rs]
            it = [r.iterations for r in rs]
            cells.append(
                [
                    witness,
                    MODE_LABELS_SHORT.get(mode, str(mode)),
                    str(len(rs)),
                    f"{statistics.mean(tt):.4g}",
                    f"{statistics.median(tt):.4g}",
                    f"{statistics.mean(gg):.4g}",
                    f"{statistics.median(gg):.4g}",
                    f"{statistics.mean(it):.4g}",
                ]
            )

    table = ax.table(
        cellText=cells[1:],
        colLabels=cells[0],
        loc="center",
        cellLoc="center",
    )
    table.scale(1.0, 1.8)
    ax.set_title("7. Summary: mean/median by witness × solver mode", pad=20)
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def chart_08_solver_mode_timing(rows: List[LogRow], out_png: str, out_csv: str) -> None:
    """All-modes solver timing comparison (generalised for any set of modes)."""
    modes = _get_modes(rows)
    if len(modes) < 2:
        return

    times_by_mode: Dict[int, List[float]] = {m: [r.total_time for r in rows if r.solver_mode == m] for m in modes}
    mode_colors = dict(zip(modes, ["steelblue", "darkorange", "seagreen", "orchid", "brown"]))

    # Pairwise diffs for all mode pairs
    paired: Dict[Tuple[str, str], Dict[int, float]] = defaultdict(dict)
    for r in rows:
        paired[(r.basename, r.witness)][r.solver_mode] = r.total_time

    mode_pairs = list(combinations(modes, 2))

    n_panels = 2 + len(mode_pairs)
    nrows_fig = (n_panels + 1) // 2
    fig, axes = plt.subplots(nrows_fig, 2, figsize=(14, 4.5 * nrows_fig))
    axes = axes.flatten()

    # Panel 0: boxplot of all modes
    ax = axes[0]
    box_data = [times_by_mode[m] for m in modes]
    box_labels = [MODE_LABELS.get(m, f"Mode {m}") for m in modes]
    ax.boxplot(box_data, tick_labels=box_labels, showmeans=True)
    ax.set_ylabel("totalTime (s)")
    ax.set_title("All runs: totalTime distribution")
    ax.grid(True, axis="y", alpha=0.3)

    # Panel 1: grouped bar chart — mean totalTime per witness, one bar per mode
    ax = axes[1]
    x = np.arange(len(WITNESSES))
    bar_w = 0.8 / len(modes)
    for idx, m in enumerate(modes):
        means = []
        for wit in WITNESSES:
            vals = [r.total_time for r in rows if r.witness == wit and r.solver_mode == m]
            means.append(statistics.mean(vals) if vals else 0.0)
        offset = (idx - (len(modes) - 1) / 2) * bar_w
        ax.bar(x + offset, means, width=bar_w,
               label=MODE_LABELS_SHORT.get(m, f"Mode {m}"),
               color=mode_colors.get(m, f"C{idx}"))
    ax.set_xticks(x)
    ax.set_xticklabels([w.replace("_", "\n") for w in WITNESSES], fontsize=7)
    ax.set_ylabel("Mean totalTime (s)")
    ax.set_title("Mean totalTime by witness mode")
    ax.legend(fontsize=7)
    ax.grid(True, axis="y", alpha=0.3)

    # Panels 2+: paired-difference histograms for each mode pair
    for pi, (ma, mb) in enumerate(mode_pairs):
        ax = axes[2 + pi]
        diffs = []
        for _k, d in paired.items():
            if ma in d and mb in d:
                diffs.append(d[ma] - d[mb])
        if diffs:
            ax.hist(diffs, bins=min(20, max(5, len(diffs) // 3)),
                    color=mode_colors.get(ma, "gray"), edgecolor="black", alpha=0.75)
            ax.axvline(0, color="red", linestyle="--", linewidth=1)
        la = MODE_LABELS_SHORT.get(ma, str(ma))
        lb = MODE_LABELS_SHORT.get(mb, str(mb))
        ax.set_xlabel(f"totalTime({la}) − totalTime({lb}) [s]")
        ax.set_ylabel("count")
        ax.set_title(f"Paired diff: +ve ⇒ {la} slower")
        ax.grid(True, alpha=0.25)

    # Hide unused axes
    for i in range(2 + len(mode_pairs), len(axes)):
        axes[i].axis("off")

    # Text summary in last visible unused panel (or create one)
    summary_ax = axes[2 + len(mode_pairs)] if (2 + len(mode_pairs)) < len(axes) else axes[-1]
    summary_ax.axis("off")
    lines = ["Summary (totalTime)", ""]
    for m in modes:
        t = times_by_mode[m]
        label = MODE_LABELS_SHORT.get(m, f"Mode {m}")
        lines.append(f"{label:20s}: n={len(t):3d}  mean={statistics.mean(t):.6g}s  med={statistics.median(t):.6g}s")
    lines.append("")
    for ma, mb in mode_pairs:
        ta, tb = times_by_mode[ma], times_by_mode[mb]
        la = MODE_LABELS_SHORT.get(ma, str(ma))
        lb = MODE_LABELS_SHORT.get(mb, str(mb))
        ratio = statistics.mean(tb) / statistics.mean(ta) if statistics.mean(ta) else 0
        lines.append(f"mean({lb})/mean({la}) = {ratio:.4f}")
    summary_ax.text(0.05, 0.95, "\n".join(lines), transform=summary_ax.transAxes,
                    va="top", family="monospace", fontsize=8)

    fig.suptitle("Solver mode timing comparison (all modes)", fontsize=12)
    fig.tight_layout()
    fig.savefig(out_png, dpi=150, bbox_inches="tight")
    plt.close(fig)

    # CSV with per-mode columns
    with open(out_csv, "w", newline="", encoding="utf-8") as f:
        cw = csv.writer(f)
        header = ["witness"]
        for m in modes:
            ml = MODE_LABELS_SHORT.get(m, str(m)).replace("\n", " ")
            header += [f"n_mode{m}", f"mean_totalTime_m{m}", f"median_totalTime_m{m}",
                       f"mean_solverTime_m{m}"]
        cw.writerow(header)
        for wit in WITNESSES:
            row: list = [wit]
            for m in modes:
                rs = [r for r in rows if r.witness == wit and r.solver_mode == m]
                row.append(str(len(rs)))
                row.append(f"{statistics.mean([r.total_time for r in rs]):.8g}" if rs else "")
                row.append(f"{statistics.median([r.total_time for r in rs]):.8g}" if rs else "")
                row.append(f"{statistics.mean([r.solver_time for r in rs]):.8g}" if rs else "")
            cw.writerow(row)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--final-dir",
        default=os.path.join(_REPO, "final_output"),
        help="Directory with *.log files",
    )
    ap.add_argument(
        "--out-dir",
        default=None,
        help="Output directory (default: <final-dir>/charts)",
    )
    args = ap.parse_args()

    final_dir = os.path.abspath(args.final_dir)
    out_dir = os.path.abspath(args.out_dir or os.path.join(final_dir, "charts"))
    os.makedirs(out_dir, exist_ok=True)

    rows = load_rows(final_dir)
    if not rows:
        print(f"No parseable logs in {final_dir}", file=sys.stderr)
        return 1

    chart_01_02_box(
        rows,
        os.path.join(out_dir, "01_runtime_by_witness_mode.png"),
        field="total_time",
        title="1. totalTime by witness mode (box plots)",
        ylabel="totalTime (s)",
    )
    chart_01_02_box(
        rows,
        os.path.join(out_dir, "02_guards_by_witness_mode.png"),
        field="guards",
        title="2. guards by witness mode (box plots)",
        ylabel="guards",
    )
    chart_04_scatter(rows, os.path.join(out_dir, "04_polsize_vs_totaltime.png"))
    chart_05_scatter(rows, os.path.join(out_dir, "05_witness_cand_vs_time.png"))
    chart_06_stacked(rows, os.path.join(out_dir, "06_time_breakdown_stacked.png"))
    write_summary_csv(rows, os.path.join(out_dir, "summary_stats.csv"))
    chart_07_table(rows, os.path.join(out_dir, "07_summary_table.png"))
    chart_08_solver_mode_timing(
        rows,
        os.path.join(out_dir, "08_solver_mode_timing.png"),
        os.path.join(out_dir, "solver_mode_timing_comparison.csv"),
    )

    print(f"Wrote charts to {out_dir} ({len(rows)} log rows)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
