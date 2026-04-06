#!/usr/bin/env bash
# Run artGallerySolver on every .pol in instances/final/ for all witness modes and
# solver modes 1 and 2. Logs (and copied .sol) go under final_output/.
#
# Usage: ./run_final_batch.sh
# Requires: build/artGallerySolver (run ./build.sh first)
#
# Resume: skips a run if the corresponding final_output/*.log already exists.
# Delete that .log (and optional matching .sol) to force a re-run.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
FINAL="$ROOT/instances/final"
OUT="$ROOT/final_output"
SOLVER="$BUILD/artGallerySolver"

WITNESSES=(ALL_VERTICES CONVEX_VERTICES CHWA_POINTS CHWA_POINTS_EXTENDED)
MODES=(1 2 3)

if [[ ! -x "$SOLVER" ]]; then
  echo "Missing executable: $SOLVER (build the project first: ./build.sh)" >&2
  exit 1
fi

mkdir -p "$OUT"
export LD_LIBRARY_PATH="$BUILD${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

shopt -s nullglob
pols=("$FINAL"/*.pol)
if [[ ${#pols[@]} -eq 0 ]]; then
  echo "No .pol files in $FINAL" >&2
  exit 1
fi

count=0
skipped=0
ok=0
fail=0
total=$((${#pols[@]} * ${#WITNESSES[@]} * ${#MODES[@]}))
echo "Instances: ${#pols[@]}, runs per instance: $((${#WITNESSES[@]} * ${#MODES[@]})), total slots: $total (existing logs are skipped)"

for pol in "${pols[@]}"; do
  base=$(basename "$pol" .pol)
  for w in "${WITNESSES[@]}"; do
    for m in "${MODES[@]}"; do
      ((++count)) || true
      log="$OUT/${base}__${w}__${m}.log"
      if [[ -f "$log" ]]; then
        ((++skipped)) || true
        echo "[$count/$total] SKIP (exists): $log"
        continue
      fi
      echo "[$count/$total] RUN $pol  witness=$w  mode=$m"
      if "$SOLVER" "$pol" "$log" "$w" "$m"; then
        ((++ok)) || true
        sol_side="${pol%.pol}.sol"
        if [[ -f "$sol_side" ]]; then
          cp -f "$sol_side" "$OUT/${base}__${w}__${m}.sol"
        fi
      else
        ((++fail)) || true
        echo "FAILED: $pol $w $m" >&2
      fi
    done
  done
done

echo "Done. Ran: $ok ok, $fail failed, $skipped skipped. Output: $OUT"
