AGSol visualization (Python)
============================

Virtual environment (from repository root 966/):

  python3 -m venv tozoni_venv
  tozoni_venv/bin/pip install -r tozoni-2016-implementation/visualization/requirements.txt

Use this interpreter for all commands below:

  tozoni_venv/bin/python tozoni-2016-implementation/visualization/agp_viz_app.py ...

Or from this directory:

  ./run_viz.sh /path/to/instance.pol -o out.png

Command line:
  tozoni_venv/bin/python agp_viz_app.py /path/to/instance.pol -o out.png
  tozoni_venv/bin/python agp_viz_app.py /path/to/instance.pol --sol /path/to/instance.sol -o out.png
  tozoni_venv/bin/python agp_viz_app.py /path/to/instance.pol --solve -o out.png

If a .sol file exists next to the .pol (same basename), it is loaded automatically.

Web UI:
  tozoni_venv/bin/python agp_viz_app.py
  Open http://127.0.0.1:5000/ — paste .pol text (and optional .sol), click Render.

Solver for /api/solve:
  Build tozoni-2016-implementation/build/artGallerySolver first.
  Set AGSOLVER=/path/to/artGallerySolver if needed.
  LD_LIBRARY_PATH is set automatically when invoking the binary from the API.
