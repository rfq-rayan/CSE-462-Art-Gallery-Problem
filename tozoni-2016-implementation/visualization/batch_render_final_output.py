#!/usr/bin/env python3
"""
Render PNG visualizations for every final_output/*.sol that matches the batch naming
  <polygon-basename>__<WITNESS>__<MODE>.sol
with instances/final/<polygon-basename>.pol

Usage (from repository root):
  python3 visualization/batch_render_final_output.py
  python3 visualization/batch_render_final_output.py --output-dir final_output/viz
"""

from __future__ import annotations

import argparse
import html
import os
import re
import sys
from typing import List

_VIZ_DIR = os.path.dirname(os.path.abspath(__file__))
_REPO_ROOT = os.path.dirname(_VIZ_DIR)
if _VIZ_DIR not in sys.path:
    sys.path.insert(0, _VIZ_DIR)

from agp_viz_app import visualize_instance  # noqa: E402
from pol_sol_io import parse_pol_file, parse_sol_file  # noqa: E402

_SOL_NAME = re.compile(
    r"^(.+)__(ALL_VERTICES|CONVEX_VERTICES|CHWA_POINTS|CHWA_POINTS_EXTENDED)__([123])\.sol$"
)


def find_repo_root() -> str:
    return _REPO_ROOT


def main() -> int:
    root = find_repo_root()
    p = argparse.ArgumentParser(description="Batch-render PNGs from final_output .sol files")
    p.add_argument(
        "--final-dir",
        default=os.path.join(root, "final_output"),
        help="Directory containing *.sol from run_final_batch.sh",
    )
    p.add_argument(
        "--pol-dir",
        default=os.path.join(root, "instances", "final"),
        help="Directory containing matching .pol files",
    )
    p.add_argument(
        "--output-dir",
        default=None,
        help="Where to write PNGs (default: <final-dir>/viz)",
    )
    p.add_argument(
        "--no-index",
        action="store_true",
        help="Do not write index.html in output directory",
    )
    args = p.parse_args()

    final_dir = os.path.abspath(args.final_dir)
    pol_dir = os.path.abspath(args.pol_dir)
    out_dir = os.path.abspath(args.output_dir or os.path.join(final_dir, "viz"))

    if not os.path.isdir(final_dir):
        print(f"Not a directory: {final_dir}", file=sys.stderr)
        return 1
    os.makedirs(out_dir, exist_ok=True)

    sol_files: List[str] = []
    for name in sorted(os.listdir(final_dir)):
        if name.endswith(".sol") and _SOL_NAME.match(name):
            sol_files.append(os.path.join(final_dir, name))

    if not sol_files:
        print(f"No matching *.sol files in {final_dir}", file=sys.stderr)
        return 1

    ok = 0
    skipped = 0
    errors: List[str] = []

    for sol_path in sol_files:
        base_name = os.path.basename(sol_path)
        m = _SOL_NAME.match(base_name)
        assert m
        poly_base = m.group(1)
        witness = m.group(2)
        mode = m.group(3)
        pol_path = os.path.join(pol_dir, poly_base + ".pol")
        png_name = os.path.splitext(base_name)[0] + ".png"
        png_path = os.path.join(out_dir, png_name)

        if os.path.isfile(png_path):
            skipped += 1
            continue

        if not os.path.isfile(pol_path):
            errors.append(f"missing .pol for {base_name}: {pol_path}")
            continue

        try:
            inst = parse_pol_file(pol_path)
            guards, _ = parse_sol_file(sol_path)
            title = f"{poly_base}  |  {witness}  |  mode {mode}"
            visualize_instance(
                inst,
                guards,
                title=title,
                output_path=png_path,
            )
            ok += 1
            print(f"Wrote {png_path}")
        except Exception as e:
            errors.append(f"{base_name}: {e}")

    print(f"Rendered: {ok}, skipped (png exists): {skipped}, errors: {len(errors)}")
    for line in errors[:50]:
        print(line, file=sys.stderr)
    if len(errors) > 50:
        print(f"... and {len(errors) - 50} more errors", file=sys.stderr)

    if not args.no_index:
        _write_index(out_dir)

    return 1 if errors else 0


def _write_index(out_dir: str) -> None:
    pngs = sorted(f for f in os.listdir(out_dir) if f.endswith(".png"))
    rows = []
    for f in pngs:
        esc = html.escape(f, quote=True)
        rows.append(
            f'<section class="case"><h2>{html.escape(f)}</h2>'
            f'<img src="{esc}" alt="{esc}" loading="lazy" /></section>'
        )
    body = "\n".join(rows)
    html_doc = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <title>AGSol — final_output visualizations</title>
  <style>
    body {{ font-family: system-ui, sans-serif; margin: 1rem 2rem; background: #fafafa; }}
    h1 {{ font-size: 1.25rem; }}
    section.case {{ margin-bottom: 2.5rem; break-inside: avoid; }}
    section.case h2 {{ font-size: 0.85rem; font-weight: 600; color: #333; margin-bottom: 0.5rem; }}
    img {{ max-width: min(900px, 100%); height: auto; border: 1px solid #ccc; background: #fff; }}
  </style>
</head>
<body>
  <h1>final_output — {len(pngs)} figure(s)</h1>
{body}
</body>
</html>
"""
    index_path = os.path.join(out_dir, "index.html")
    with open(index_path, "w", encoding="utf-8") as fp:
        fp.write(html_doc)
    print(f"Wrote {index_path}")


if __name__ == "__main__":
    raise SystemExit(main())
