#!/usr/bin/env bash
# Render PNGs + index.html from final_output/*.sol (see visualization/batch_render_final_output.py).
cd "$(dirname "$0")/.."
exec python3 visualization/batch_render_final_output.py "$@"
