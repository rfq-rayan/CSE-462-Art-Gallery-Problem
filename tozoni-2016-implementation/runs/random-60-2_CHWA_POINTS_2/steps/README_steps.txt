How to analyze random-60-2_CHWA_POINTS_2.stdout.txt
================================================

1) HEADER (lines 1–8)
   Instance path, witness mode (CHWA_POINTS), solver mode (2).

2) ONE OUTER ITERATION: "Initiating iteration 1"
   The main loop builds witnesses, candidates, then calls runArt().

3) BLOCK "Running solver..." / runArt INNER LOOP
   Each cycle is:
     A) SCP (GLPK): set cover on discrete witnesses → binary solution.
        [PreSolver] shrinks candidates/constraints; GLPK obj / mip value is the
        number of selected guards (e.g. 8), NOT polygon area.
     B) [SolverPLIGlpk] Result: k   ← SCP cardinality for this pass.
     C) [ArtGallerySolver] Area miss: α   ← CONTINUOUS check: area of P \ (∪ visibility(guards)).
        If α > 0, discrete witnesses were insufficient to certify full coverage.
     D) # of uncovered regions, then "N new witnesses were chosen" (refinement).
     E) If area miss > 0, the loop repeats with more witnesses/constraints.

   In this log: two inner steps — first Area miss ≈ 1.63 (3 regions, 3 new witnesses),
   second Area miss = 0. Both SCP results show 8 guards; the second pass closes the gap.

4) AFTER runArt: "Comparing upper bound with lower bound"
   Lower Bound / Upper Bound refer to proof on guard COUNT (here both 8) → optimal.

5) WHAT THE LOG DOES NOT CONTAIN
   Coordinates of witness points or candidate guards (only counts and GLPK traces).

Figures:
  01 — Input polygon.
  02 — Boundary vertices only (not CHWA witnesses).
  03 — Area miss vs runArt inner step; annotations show |guards| from [SolverPLIGlpk] Result.
  04 — LB/UB plot (single point if one outer iteration).
  05 — Parsed inner-step table.
  06 — Final guard positions from .sol (written beside .pol by the binary).
