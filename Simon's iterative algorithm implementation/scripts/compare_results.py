#!/usr/bin/env python3
"""
Compare our solver results with Tozoni's results for matching instances.
Reads from our_dataset/final_output/ and final_output_tozoni/.
"""

import json
import os
import glob

OUR_DIR = "our_dataset/final_output"
TOZONI_DIR = "final_output_tozoni"

STRATEGIES = ["ALL_VERTICES", "CHWA_POINTS", "CHWA_POINTS_EXTENDED", "CONVEX_VERTICES"]
RUNS = [1, 2, 3]


def parse_our_output(instance):
    output_path = os.path.join(OUR_DIR, f"{instance}_output.json")
    runtime_path = os.path.join(OUR_DIR, f"{instance}_runtime.txt")

    if not os.path.exists(output_path):
        return None

    with open(output_path) as f:
        data = json.load(f)

    wall_time = None
    if os.path.exists(runtime_path):
        with open(runtime_path) as f:
            for line in f:
                if line.startswith("wall_clock_seconds:"):
                    wall_time = float(line.split(":")[1].strip())

    return {
        "guards": data.get("num_guards", len(data.get("guards", []))),
        "time": data.get("solve_time_seconds", wall_time),
        "status": data.get("status", "unknown"),
        "iterations": data.get("iterations", "?"),
    }


def parse_tozoni_log(instance):
    """Parse all Tozoni runs for an instance, return best result and all results."""
    results = []

    for strategy in STRATEGIES:
        for run in RUNS:
            log_path = os.path.join(TOZONI_DIR, f"{instance}__{strategy}__{run}.log")
            if not os.path.exists(log_path):
                continue

            with open(log_path) as f:
                lines = f.readlines()

            if len(lines) < 2:
                continue

            header = lines[0].strip().split()
            values = lines[1].strip().split()

            try:
                guards_idx = header.index("guards")
                time_idx = header.index("totalTime")
                iter_idx = header.index("iterations")

                guards = int(values[guards_idx])
                total_time = float(values[time_idx])
                iterations = int(values[iter_idx])

                results.append({
                    "strategy": strategy,
                    "run": run,
                    "guards": guards,
                    "time": total_time,
                    "iterations": iterations,
                })
            except (ValueError, IndexError):
                continue

    if not results:
        return None, []

    best = min(results, key=lambda r: (r["guards"], r["time"]))
    return best, results


def get_our_instances():
    instances = []
    for f in sorted(glob.glob(os.path.join(OUR_DIR, "*_output.json"))):
        basename = os.path.basename(f)
        instance = basename.replace("_output.json", "")
        instances.append(instance)
    return instances


def format_time(seconds):
    if seconds is None:
        return "N/A"
    if seconds < 1:
        return f"{seconds*1000:.1f}ms"
    if seconds < 60:
        return f"{seconds:.2f}s"
    return f"{seconds/60:.1f}min"


def main():
    instances = get_our_instances()
    if not instances:
        print("No output files found in", OUR_DIR)
        return

    print("=" * 120)
    print(f"{'INSTANCE':<40} {'VERTS':>5} | {'OURS':>6} {'TIME':>10} {'STATUS':>10} | {'TOZONI':>6} {'TIME':>10} {'STRATEGY':<25} | {'DELTA':>6}")
    print("=" * 120)

    total_ours = 0
    total_tozoni = 0
    total_match = 0

    for instance in instances:
        ours = parse_our_output(instance)
        tozoni_best, tozoni_all = parse_tozoni_log(instance)

        if not ours:
            continue

        # Extract vertex count from instance name (e.g., "random-80-23" -> 80)
        parts = instance.split("-")
        verts = "?"
        for p in parts:
            if p.isdigit() and int(p) >= 10:
                verts = p
                break

        our_guards = ours["guards"]
        our_time = format_time(ours["time"])
        our_status = ours["status"]

        if tozoni_best:
            tz_guards = tozoni_best["guards"]
            tz_time = format_time(tozoni_best["time"])
            tz_strategy = tozoni_best["strategy"]
            delta = our_guards - tz_guards
            delta_str = f"{delta:+d}" if delta != 0 else "="

            total_ours += our_guards
            total_tozoni += tz_guards
            if delta == 0:
                total_match += 1
        else:
            tz_guards = "N/A"
            tz_time = "N/A"
            tz_strategy = ""
            delta_str = "?"

        print(f"{instance:<40} {verts:>5} | {our_guards:>6} {our_time:>10} {our_status:>10} | {tz_guards:>6} {tz_time:>10} {tz_strategy:<25} | {delta_str:>6}")

    print("=" * 120)
    print()

    n = len(instances)
    print(f"SUMMARY ({n} instances)")
    print(f"  Guard count matches:  {total_match}/{n}")
    print(f"  Our total guards:     {total_ours}")
    print(f"  Tozoni total guards:  {total_tozoni}")
    print(f"  Our excess guards:    {total_ours - total_tozoni:+d}")
    print()

    # Detailed Tozoni breakdown per instance
    print("=" * 120)
    print("TOZONI DETAILED BREAKDOWN (all strategies, best run per strategy)")
    print("=" * 120)

    for instance in instances:
        _, tozoni_all = parse_tozoni_log(instance)
        if not tozoni_all:
            continue

        print(f"\n  {instance}:")
        by_strategy = {}
        for r in tozoni_all:
            key = r["strategy"]
            if key not in by_strategy or r["guards"] < by_strategy[key]["guards"] or \
               (r["guards"] == by_strategy[key]["guards"] and r["time"] < by_strategy[key]["time"]):
                by_strategy[key] = r

        for s in STRATEGIES:
            if s in by_strategy:
                r = by_strategy[s]
                print(f"    {s:<25}  guards={r['guards']}  time={format_time(r['time'])}  iters={r['iterations']}")


if __name__ == "__main__":
    main()
