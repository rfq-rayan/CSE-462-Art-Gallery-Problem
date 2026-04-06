#!/usr/bin/env bash
# Top-level runner for lab pipelines (Tozoni vs Simon). See usage() or run with --help.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOZONI_RUN="${ROOT}/tozoni-2016-implementation/scripts/run-all-1.sh"
SIMON_RUN="${ROOT}/Simon's iterative algorithm implementation/scripts/run-all-2.sh"

usage() {
  cat <<'EOF'
Top-level runner for lab pipelines (Tozoni vs Simon).

Usage:
  ./run-all.sh                    Run both pipelines (default)
  ./run-all.sh --1                Tozoni only (tozoni-2016-implementation/scripts/run-all-1.sh)
  ./run-all.sh --2                Simon only (Simon's .../scripts/run-all-2.sh)
  ./run-all.sh --both             Same as no arguments
  ./run-all.sh --1 --2            Both pipelines, explicit

Aliases: --1 / --tozoni,  --2 / --simon,  --both / --all

With a single pipeline, extra arguments are forwarded to that script:
  ./run-all.sh --1 [trace.pol]
  ./run-all.sh --2 -- --force
EOF
}

RUN_1=false
RUN_2=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --1|--tozoni)
      RUN_1=true
      shift
      ;;
    --2|--simon)
      RUN_2=true
      shift
      ;;
    --both|--all)
      RUN_1=true
      RUN_2=true
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      break
      ;;
  esac
done

if ! "$RUN_1" && ! "$RUN_2"; then
  RUN_1=true
  RUN_2=true
fi

if "$RUN_1" && "$RUN_2"; then
  bash "$TOZONI_RUN"
  bash "$SIMON_RUN"
elif "$RUN_1"; then
  bash "$TOZONI_RUN" "$@"
elif "$RUN_2"; then
  bash "$SIMON_RUN" "$@"
fi
