#!/usr/bin/env bash
# Example: run solver with geometry trace, then render PNGs.
# Usage: ./run_trace_and_render.sh instances/final/random-80-23.pol [CHWA_POINTS] [2]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
POL="${1:?path to .pol}"
WITNESS="${2:-CHWA_POINTS}"
MODE="${3:-2}"
BASE="$(basename "$POL" .pol)"
TRACE_DIR="$ROOT/runs/${BASE}_trace"
VIZ_DIR="$ROOT/runs/${BASE}_viz"
mkdir -p "$TRACE_DIR" "$VIZ_DIR"
export AGSOL_TRACE_DIR="$TRACE_DIR"
LOG_OUT="$ROOT/runs/${BASE}.stdout.txt"
SUMMARY_LOG="$ROOT/runs/${BASE}.summary.log"
(
  cd "$ROOT/build"
  LD_LIBRARY_PATH=. ./artGallerySolver "$ROOT/$POL" "$SUMMARY_LOG" "$WITNESS" "$MODE"
) >"$LOG_OUT" 2>&1
python3 "$ROOT/visualization/render_trace_coverage.py" \
  --trace-dir "$TRACE_DIR" \
  --pol "$ROOT/$POL" \
  --out-dir "$VIZ_DIR"
echo "stdout: $LOG_OUT"
echo "trace:  $TRACE_DIR/step_*.txt"
echo "png:    $VIZ_DIR/ + index.html"
