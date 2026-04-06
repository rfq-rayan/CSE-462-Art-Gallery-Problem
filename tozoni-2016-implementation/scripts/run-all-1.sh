#!/usr/bin/env bash
# Reproduce the full project pipeline end-to-end.
#
# Steps:
#   1. make-sample.sh           — pick one .pol per category into instances/sample/
#   2. run_final_batch.sh       — solve every instance/final/*.pol (all witness & solver modes)
#   3. run_trace_and_render.sh  — run one instance with geometry trace + render PNGs
#   4. render_final_viz.sh      — render solution visualisations from final_output/
#
# Usage:
#   ./scripts/run-all-1.sh                            (uses default trace case)
#   ./scripts/run-all-1.sh instances/agp2009a-orthorand/random-60-2.pol   (custom trace case)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SCRIPTS="$ROOT/scripts"

TRACE_POL="${1:-instances/agp2009a-orthorand/random-40-8.pol}"

echo "========================================"
echo " Step 1/4 — Sampling instances"
echo "========================================"
bash "$SCRIPTS/make-sample.sh"

echo ""
echo "========================================"
echo " Step 2/4 — Running final batch solver"
echo "========================================"
bash "$SCRIPTS/run_final_batch.sh"

echo ""
echo "========================================"
echo " Step 3/4 — Trace & render: $TRACE_POL"
echo "========================================"
bash "$SCRIPTS/run_trace_and_render.sh" "$TRACE_POL"

echo ""
echo "========================================"
echo " Step 4/4 — Rendering final visualisations"
echo "========================================"
bash "$SCRIPTS/render_final_viz.sh"

echo ""
echo "========================================"
echo " All done."
echo "========================================"
