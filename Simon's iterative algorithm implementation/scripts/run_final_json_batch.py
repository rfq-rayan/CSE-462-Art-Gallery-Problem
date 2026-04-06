#!/usr/bin/env python3
"""
Batch-run agp_solver on every JSON in our_dataset/final_json.
Writes our_dataset/final_output/<stem>_output.json and <stem>_runtime.txt per instance.
Appends a full run log to our_dataset/final_output/batch_run.log (see --log-file).

Resumability (default on): skips instances that already finished successfully (valid
*_output.json and *_runtime.txt with returncode 0 or 2). Failed or incomplete runs
are re-executed. Use --force to re-run everything.

Usage (from repo root):
  python3 scripts/run_final_json_batch.py

Or with custom dirs:
  python3 scripts/run_final_json_batch.py --solver build/agp_solver \\
      --input-dir our_dataset/final_json --output-dir our_dataset/final_output
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import subprocess
import sys
import time
from pathlib import Path


def setup_logging(log_path: Path, console: bool) -> logging.Logger:
    """File log always on; optional brief lines to stdout."""
    log_path.parent.mkdir(parents=True, exist_ok=True)
    logger = logging.getLogger("run_final_json_batch")
    logger.handlers.clear()
    logger.setLevel(logging.DEBUG)
    logger.propagate = False

    fh = logging.FileHandler(log_path, mode="a", encoding="utf-8")
    fh.setLevel(logging.DEBUG)
    fh.setFormatter(
        logging.Formatter("%(asctime)s [%(levelname)s] %(message)s")
    )
    logger.addHandler(fh)

    if console:
        sh = logging.StreamHandler(sys.stdout)
        sh.setLevel(logging.INFO)
        sh.setFormatter(logging.Formatter("%(message)s"))
        logger.addHandler(sh)

    return logger


def parse_runtime_returncode(runtime_txt: Path) -> int | None:
    """Read `returncode: N` from *_runtime.txt; None if missing or unreadable."""
    if not runtime_txt.is_file():
        return None
    try:
        text = runtime_txt.read_text(encoding="utf-8")
    except OSError:
        return None
    for line in text.splitlines():
        line = line.strip()
        if line.lower().startswith("returncode:"):
            try:
                return int(line.split(":", 1)[1].strip())
            except ValueError:
                return None
    return None


def output_looks_complete(out_json: Path) -> bool:
    """True if solver output JSON exists and looks like a finished solution."""
    if not out_json.is_file():
        return False
    try:
        with open(out_json, encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError):
        return False
    if not isinstance(data, dict):
        return False
    return "guards" in data or "num_guards" in data or "status" in data


def already_successful(
    out_json: Path,
    runtime_txt: Path,
) -> bool:
    """
    True if this instance need not be run again: good output and success returncode.
    Matches Flask app: returncode 0 (optimal) and 2 (suboptimal) are OK.
    If runtime file is missing but output is complete, treat as success (resume).
    """
    if not output_looks_complete(out_json):
        return False
    rc = parse_runtime_returncode(runtime_txt)
    if rc is None:
        return True
    return rc in (0, 2)


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    p = argparse.ArgumentParser(description="Run agp_solver on all final_json instances.")
    p.add_argument(
        "--solver",
        type=Path,
        default=repo / "build" / "agp_solver",
        help="Path to agp_solver executable",
    )
    p.add_argument(
        "--input-dir",
        type=Path,
        default=repo / "our_dataset" / "final_json",
        help="Directory containing input polygon JSON files",
    )
    p.add_argument(
        "--output-dir",
        type=Path,
        default=repo / "our_dataset" / "final_output",
        help="Directory for *_output.json and *_runtime.txt",
    )
    p.add_argument(
        "--verbosity",
        type=int,
        default=1,
        help="Passed to agp_solver (0=quiet, 1=info, 2=debug; default: 1 for logs)",
    )
    p.add_argument(
        "--log-file",
        type=Path,
        default=None,
        help="Append log path (default: <output-dir>/batch_run.log)",
    )
    p.add_argument(
        "--no-console-log",
        action="store_true",
        help="Do not mirror progress lines to stdout (file log still written)",
    )
    p.add_argument(
        "--resume",
        action="store_true",
        default=True,
        help="Skip instances already completed successfully (default: on)",
    )
    p.add_argument(
        "--no-resume",
        action="store_false",
        dest="resume",
        help="Run every instance even if a previous success exists",
    )
    p.add_argument(
        "--force",
        action="store_true",
        help="Same as --no-resume: re-run all instances",
    )
    p.add_argument(
        "--skip-existing",
        action="store_true",
        help="Legacy: skip if <stem>_output.json exists (ignores success/failure)",
    )
    args = p.parse_args()
    if args.force:
        args.resume = False

    solver = args.solver.resolve()
    if not solver.is_file() or not os.access(solver, os.X_OK):
        print(f"ERROR: solver not found or not executable: {solver}", file=sys.stderr)
        print("Build first: mkdir -p build && cd build && cmake .. && make -j", file=sys.stderr)
        return 1

    indir = args.input_dir.resolve()
    outdir = args.output_dir.resolve()
    outdir.mkdir(parents=True, exist_ok=True)

    log_path = (args.log_file if args.log_file is not None else outdir / "batch_run.log").resolve()
    log = setup_logging(log_path, console=not args.no_console_log)
    log.info(
        "Batch start: solver=%s input_dir=%s output_dir=%s resume=%s",
        solver,
        indir,
        outdir,
        args.resume and not args.force,
    )

    inputs = sorted(indir.glob("*.json"))
    if not inputs:
        log.error("No *.json in %s", indir)
        return 1

    for path in inputs:
        stem = path.stem
        out_json = outdir / f"{stem}_output.json"
        runtime_txt = outdir / f"{stem}_runtime.txt"

        if args.skip_existing and out_json.is_file():
            log.info("skip (exists, --skip-existing): %s", stem)
            continue

        if args.resume and already_successful(out_json, runtime_txt):
            log.info("resume skip (already complete): %s", stem)
            continue

        cmd = [
            str(solver),
            str(path),
            "--output",
            str(out_json),
            "--verbosity",
            str(args.verbosity),
        ]

        log.info("--- instance %s ---", stem)
        log.info("command: %s", " ".join(cmd))

        t0 = time.perf_counter()
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
        )
        wall = time.perf_counter() - t0

        if proc.stdout:
            log.info("solver stdout:\n%s", proc.stdout.rstrip())
        else:
            log.debug("solver stdout: (empty)")
        if proc.stderr:
            log.info("solver stderr:\n%s", proc.stderr.rstrip())
        else:
            log.debug("solver stderr: (empty)")

        log.info(
            "instance %s finished: wall_clock_s=%.6f returncode=%s",
            stem,
            wall,
            proc.returncode,
        )

        solve_time = None
        status = None
        if out_json.is_file():
            try:
                with open(out_json, encoding="utf-8") as f:
                    data = json.load(f)
                solve_time = data.get("solve_time_seconds")
                status = data.get("status")
            except (OSError, json.JSONDecodeError):
                pass

        lines = [
            f"instance: {stem}",
            f"wall_clock_seconds: {wall:.6f}",
        ]
        if solve_time is not None:
            lines.append(f"solve_time_seconds: {solve_time}")
        else:
            lines.append("solve_time_seconds: (n/a)")
        if status is not None:
            lines.append(f"status: {status}")
        else:
            lines.append("status: (n/a)")
        lines.append(f"returncode: {proc.returncode}")

        runtime_txt.write_text("\n".join(lines) + "\n", encoding="utf-8")
        log.info("%s: wall=%.3fs return=%s", stem, wall, proc.returncode)

    log.info("Batch finished (%d input file(s)).", len(inputs))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
