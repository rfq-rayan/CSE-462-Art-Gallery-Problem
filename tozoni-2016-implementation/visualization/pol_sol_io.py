"""
Parse AGSol / CGAL .pol instance files and .sol solution files.

See README.txt sections 4.a and 4.c.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import List, Tuple


@dataclass
class ParsedInstance:
    """Outer boundary (CCW) and holes (CW per CGAL convention in solver)."""

    outer: List[Tuple[float, float]]
    holes: List[List[Tuple[float, float]]]
    source_path: str = ""


def parse_rational(token: str) -> float:
    token = token.strip()
    if not token:
        raise ValueError("empty rational token")
    if "/" in token:
        num, den = token.split("/", 1)
        return float(num) / float(den)
    return float(token)


def parse_pol_file(path: str) -> ParsedInstance:
    """
    One line (possibly wrapped): n, then n vertices as x/y rationals,
    then hole count, then each hole as m + m vertices.
    """
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()
    tokens = text.split()
    if not tokens:
        raise ValueError(f"empty file: {path}")

    i = 0
    n = int(tokens[i])
    i += 1
    outer: List[Tuple[float, float]] = []
    for _ in range(n):
        if i + 1 >= len(tokens):
            raise ValueError("unexpected end while reading outer boundary")
        x = parse_rational(tokens[i])
        y = parse_rational(tokens[i + 1])
        i += 2
        outer.append((x, y))

    if i >= len(tokens):
        return ParsedInstance(outer=outer, holes=[], source_path=path)

    n_holes = int(tokens[i])
    i += 1
    holes: List[List[Tuple[float, float]]] = []
    for _ in range(n_holes):
        if i >= len(tokens):
            raise ValueError("unexpected end while reading hole count")
        m = int(tokens[i])
        i += 1
        hole: List[Tuple[float, float]] = []
        for _ in range(m):
            if i + 1 >= len(tokens):
                raise ValueError("unexpected end while reading hole")
            x = parse_rational(tokens[i])
            y = parse_rational(tokens[i + 1])
            i += 2
            hole.append((x, y))
        holes.append(hole)

    return ParsedInstance(outer=outer, holes=holes, source_path=path)


def expected_sol_path(pol_path: str) -> str:
    """Matches artGallerySolver: basename without extension + .sol in same directory as input path handling."""
    import os

    # C++ getFileName: strip from start to last '.' — for a/b/foo.pol -> a/b/foo.sol
    s = pol_path.replace("\\", "/")
    dot = s.rfind(".")
    base = s[:dot] if dot != -1 else s
    return base + ".sol"


def parse_sol_file(path: str) -> Tuple[List[Tuple[float, float]], str]:
    """
    Line 1: instance path string (informational).
    Line 2: k, then 2k rationals x1 y1 ...
    """
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        lines = [ln.strip() for ln in f.readlines() if ln.strip()]
    if len(lines) < 2:
        raise ValueError(f"solution file needs 2 lines, got {len(lines)}: {path}")

    ref = lines[0]
    parts = lines[1].split()
    k = int(parts[0])
    rest = parts[1:]
    if len(rest) != 2 * k:
        raise ValueError(f"expected {2 * k} coordinates for {k} guards, got {len(rest)}")

    guards: List[Tuple[float, float]] = []
    for j in range(k):
        x = parse_rational(rest[2 * j])
        y = parse_rational(rest[2 * j + 1])
        guards.append((x, y))

    return guards, ref
