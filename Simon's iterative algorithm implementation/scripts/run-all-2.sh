#!/usr/bin/env bash
# Full Simon's algorithm pipeline: batch solve → visualize → compare with Tozoni.
# Run from anywhere; working directory is set to the Simon project root.
# Any arguments are forwarded only to run_final_json_batch.py (e.g. --force).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${REPO_ROOT}"

echo "==> [1/3] run_final_json_batch.py"
python3 "${SCRIPT_DIR}/run_final_json_batch.py" "$@"

echo "==> [2/3] visualize_agp_simulation.py"
python3 "${SCRIPT_DIR}/visualize_agp_simulation.py"

echo "==> [3/3] compare_results.py"
python3 "${SCRIPT_DIR}/compare_results.py"

echo "Pipeline finished."
