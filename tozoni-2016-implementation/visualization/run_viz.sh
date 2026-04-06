#!/usr/bin/env bash
# Run the AGSol visualizer using the repo-root venv (tozoni_venv).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VENV="$ROOT/tozoni_venv"
PY="$VENV/bin/python"
if [[ ! -x "$PY" ]]; then
  echo "Missing $PY — from repo root run: python3 -m venv tozoni_venv && tozoni_venv/bin/pip install -r tozoni-2016-implementation/visualization/requirements.txt" >&2
  exit 1
fi
exec "$PY" "$ROOT/tozoni-2016-implementation/visualization/agp_viz_app.py" "$@"
