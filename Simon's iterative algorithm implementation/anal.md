 
# Art Gallery Problem — Vision-Stable Algorithm: Agent Implementation Guide

> Based on: **"A Practical Algorithm with Performance Guarantees for the Art Gallery Problem"**
> Hengeveld & Miltzow, DMTCS vol. 25:2 #18 (2023)  
> Reference implementation: https://github.com/simonheng/AGPIterative

---

## 0. Context & Problem Statement

**Goal:** Given a closed simple polygon P, find a minimum-size set G ⊂ P of *point guards* such that every point in P is *seen* by at least one guard (i.e., the line segment from the guard to that point lies entirely within P).

**Why this is hard:**
- The problem is ∃ℝ-complete — algebraic methods are unavoidable for any exact algorithm without assumptions.
- Optimal guards may require irrational coordinates, so naive discretization schemes can loop forever.
- Previous practical algorithms had no termination guarantee for all inputs.

**Key innovation:** The paper introduces *vision-stability* as an assumption on the input polygon, under which a polynomial-size candidate set provably contains an optimal solution. The resulting algorithm either returns the optimum or certifies that vision-stability was violated.

---

## 1. Core Definitions

### 1.1 Visibility

```
vis(q) = { x ∈ P : seg(q, x) ⊆ P }
```

Two points p, q *see each other* iff `seg(p,q) ⊆ P`.

### 1.2 Reflex Vertex

A vertex r of P is *reflex* if its interior angle > π. Reflex vertices are the only locations where visibility regions change discontinuously. Denote the set of reflex vertices by R.

### 1.3 Enhanced / Diminished Visibility

For a reflex vertex r visible from q, define the *visibility-enhancing region* A(r, δ):
- Rotate a ray with apex r by angle δ clockwise and counter-clockwise.
- Each swept position of the ray yields a maximal segment inside P with endpoint r.
- A(r, δ) = union of all such segments.

```
vis_δ(q)  = vis(q) ∪ { A(r, δ) : r ∈ R, r visible from q }   [ENHANCED, δ > 0]
vis_{-δ}(q) = vis(q) \ { A(r, δ) : r ∈ R, r visible from q }  [DIMINISHED, δ > 0]
```

Both are defined as closed sets.

### 1.4 δ-Guarding

A set G is *δ-guarding* P iff:
```
∪_{g ∈ G} vis_δ(g) = P
```

`opt(P, δ)` = size of the minimum δ-guarding set.  
`opt(P, 0)` = `opt(P)` = the standard art gallery optimum.

### 1.5 Vision-Stability

Polygon P has *vision-stability δ > 0* iff:
```
opt(P, -δ) = opt(P, δ)
```

Intuition: small perturbation of guards (enhanced or diminished by δ) does not change the optimal count. `opt(P, x)` is a monotone decreasing step function with finitely many breakpoints; P is vision-stable iff no breakpoint lies at 0.

**Lemma (probabilistic justification):** For any simple polygon P on n vertices, if x is chosen uniformly from [-1/2, 1/2]:
```
Pr[opt(P, x-δ) = opt(P, x+δ)] ≥ 1 - 2δn
```
So most polygons are vision-stable for δ = Ω(1/n).

### 1.6 Angular Capacity

For a convex face f and reflex vertex r visible from f:
```
α(r, f) = angle of minimum cone with apex r that fully contains f
capacity(f) = max_{r ∈ vis(f) ∩ R} α(r, f)
```

### 1.7 Chord-Visibility Width

A *chord* of P is a line segment inside P connecting two non-adjacent vertices.  
For chord c: `n(c)` = number of P-vertices visible from c.

```
cw(P) = max_{c is a chord of P} n(c)
```

This captures local complexity. Practical polygons often have small cw(P) even when n is large.

### 1.8 Representative of a Face

Given a convex face f in the arrangement A:
```
representative(f) = r           if f contains a reflex vertex r
                  = lex-smallest non-convex vertex of f   otherwise
```

---

## 2. Algorithm Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    ITERATIVE ALGORITHM                       │
│                                                             │
│  INIT: Build arrangement A (weak visibility tree + rays)    │
│                           │                                 │
│  ┌────────────────────────▼──────────────────────────────┐  │
│  │                      LOOP                             │  │
│  │  1. Extract relevant faces                            │  │
│  │  2. Solve Normal IP (+ optionally Big IP)             │  │
│  │  3. Check result:                                     │  │
│  │     a) All point-guards, all witnesses seen → DONE    │  │
│  │     b) Splittable face found → split, update A        │  │
│  │     c) No splittable face, all unsplittable → lower λ │  │
│  │  4. Update: critical witnesses, visibility pairs       │  │
│  └───────────────────────────────────────────────────────┘  │
│                           │                                 │
│                    REPORT SOLUTION                          │
└─────────────────────────────────────────────────────────────┘
```

Two algorithm variants:
- **One-Shot (theoretical):** Builds full arrangement upfront, solves one IP. O(r⁸/δ⁴ · log n) preprocessing.
- **Iterative (practical):** Starts coarse, refines adaptively. Same worst-case bound, much faster in practice.

---

## 3. Data Structures

### 3.1 Polygon Representation

```python
class Polygon:
    vertices: List[Point]          # ordered CCW
    edges: List[Segment]
    reflex_vertices: List[Point]   # vertices with interior angle > π
    
class Point:
    x: Fraction                    # use exact arithmetic (e.g., Python Fraction or CGAL Exact)
    y: Fraction

class Segment:
    p: Point
    q: Point
```

**Critical:** Use exact arithmetic (rational numbers or CGAL Exact_predicates_exact_constructions_kernel) throughout. Floating point will cause incorrect visibility queries.

### 3.2 Arrangement A

```python
class Arrangement:
    vertices: List[Point]          # intersection points of segments inside P
    edges: List[Segment]           # arrangement edges
    faces: List[ConvexFace]        # convex regions

class ConvexFace:
    vertices: List[Point]          # vertices of this convex polygon
    interior_point: Point          # a point strictly inside
    reflex_vertices_on_boundary: List[Point]
    angular_capacity: float        # capacity(f)
    is_splittable: bool
    representative: Point          # representative(f)
```

### 3.3 Candidate and Witness Sets

```python
class CandidateSet:
    vertex_candidates: List[Point]   # arrangement vertices (non-convex P-vertices)
    face_candidates: List[ConvexFace]

class WitnessSet:
    vertex_witnesses: List[Point]    # one interior point per face of A
    face_witnesses: List[ConvexFace] # all faces of A
    
    # For iterative algorithm:
    critical_witnesses: Set[Union[Point, ConvexFace]]  # W* ⊆ W
```

### 3.4 Visibility Matrix

```python
# VIS[c][w] = True iff candidate c completely sees witness w
VIS: Dict[Candidate, Set[Witness]]
```

### 3.5 Weak Visibility Polygon Tree

```python
class WVPNode:
    polygon: Polygon               # a weak visibility polygon
    defining_chord: Segment        # the chord that defines this node
    children: List[WVPNode]
    parent: Optional[WVPNode]
    vertices_of_P: List[Point]     # P-vertices inside this node
    
class WVPTree:
    root: WVPNode
    nodes: List[WVPNode]
```

---

## 4. One-Shot Algorithm (Correctness Reference)

### 4.1 Build Arrangement A

```
Input: polygon P, vision-stability estimate δ, reflex vertices R
Output: arrangement A

1. For each reflex vertex r ∈ R:
   a. Shoot rays from r with angular spacing ≤ δ/2
      (use 2^k directions to avoid trig: k=4 initially → 16 directions,
       spanning [0, 2π). Use the approximation πλ ≤ α ≤ 4πλ where λ = 2^{-k})
   b. Each ray: extend to boundary of P or next obstruction inside P

2. For each pair (r1, r2) ∈ R×R such that r1 sees r2:
   a. Add the chord chord(r1, r2) to A

3. For any face incident to more than one reflex vertex:
   a. Subdivide it (e.g., add the reflex chord through it)

4. All resulting segments form arrangement A
   Complexity: O(r²/δ) segments → O(r⁴/δ²) vertices/edges/faces
```

### 4.2 Define Candidates and Witnesses

```
vertex(C) = all vertices of A that are NOT convex vertices of P
face(C)   = all faces of A
vertex(W) = one interior point per face of A
face(W)   = all faces of A

Note: every face of A has angular capacity ≤ δ/2 by construction
```

### 4.3 Compute Visibility Pairs

For each (c, w) ∈ C × W:
- Determine if c completely sees w
- Use O(log n) shortest-path map queries (Guibas et al. 1987 or Hengeveld et al. follow-up)
- Total pairs: O(r⁸/δ⁴)

### 4.4 Integer Program (One-Shot IP)

Variables:
- `x[c]` ∈ {0,1} for each candidate c ∈ C (1 = used as guard)
- `y[w]` ∈ {0,1} for each face-witness w ∈ face(W) (1 = unseen)

Choose ε = 1 / (|C| + |W| + 1)

**Objective (minimize):**
```
min  Σ_{c ∈ vertex(C)} x[c]
   + (1+ε) · Σ_{c ∈ face(C)} x[c]
   + ε · Σ_{w ∈ face(W)} y[w]
```

The (1+ε) factor makes the IP prefer vertex-candidates over face-candidates.
The ε·y term penalizes unseen face-witnesses.

**Hard constraints (all vertex-witnesses must be seen):**
```
For each w ∈ vertex(W):
    Σ_{c ∈ VIS(w)} x[c] ≥ 1
```

**Soft constraints (face-witnesses should be seen, or pay ε):**
```
For each w ∈ face(W):
    y[w] + Σ_{c ∈ VIS(w)} x[c] ≥ 1
```

**Bounds:**
```
x[c] ∈ {0, 1}  ∀c ∈ C
y[w] ∈ {0, 1}  ∀w ∈ face(W)
```

**Two-stage trick (numerical stability):**
- Stage 1: Solve with ε=0. Get optimal size s.
- Stage 2: Fix Σ x[c] = s. Minimize Σ face-candidates used + Σ y[w].

### 4.5 Interpret Result

```
IF all guards are vertex-candidates AND all y[w] = 0:
    → Return the guards. This IS the optimal solution. (Reliability guarantee)
ELSE:
    → Report "vision-stability δ was too large; polygon has lower vision-stability"
    → Halve δ and repeat
```

---

## 5. Iterative Algorithm (Practical Version)

### 5.1 Initialization

```python
def initialize(P: Polygon) -> (Arrangement, WVPTree):
    T = build_weak_visibility_polygon_tree(P)
    A = Arrangement()
    
    # Add defining chords of T
    for node in T.nodes:
        A.add_segment(node.defining_chord)
    
    # Shoot horizontal and vertical rays from each reflex vertex r
    for r in P.reflex_vertices:
        for direction in [RIGHT, LEFT, UP, DOWN]:
            ray = shoot_ray(r, direction, P, A)
            A.add_segment(ray)
    
    # Result: O(r) complexity arrangement, all faces convex
    return A, T
```

### 5.2 Main Loop

```python
def iterative_agp(P: Polygon, delta_hint: float = 1.0) -> List[Point]:
    A, T = initialize(P)
    lambda_ = 1/16   # granularity, λ = 2^{-k}, k=4
    W_critical = init_critical_witnesses(A, T)  # 10% random sample per WVP node
    
    while True:
        C = build_candidates(A)       # vertex(C) + face(C)
        W = build_witnesses(A)        # vertex(W) + face(W)
        
        # Compute visibilities between W_critical and C
        VIS = compute_visibilities(C, W_critical, T)
        
        # Solve Normal IP
        guards, unseen_faces = solve_normal_ip(C, W_critical, VIS, epsilon)
        
        # Update critical witnesses
        W_critical, added = update_critical_witnesses(guards, W, W_critical, A, T)
        if added:
            continue  # re-solve IP with expanded W_critical
        
        # Check termination
        if all_point_guards(guards) and all_witnesses_seen(guards, W, T):
            return [g for g in guards]  # OPTIMAL SOLUTION
        
        # Find faces to split
        to_split = get_splittable_faces(guards, unseen_faces, A)
        
        if to_split:
            for face in to_split:
                split_face(face, A, lambda_)
        else:
            # Try Big IP to find any splittable face
            splittable = solve_big_ip(C, W, VIS)
            if splittable:
                for face in splittable:
                    split_face(face, A, lambda_)
            else:
                # No splittable face → lower granularity
                lambda_ /= 2
                if lambda_ < EPSILON_MIN:
                    raise VisionStabilityViolation("Polygon likely not vision-stable at this δ")
```

### 5.3 Critical Witnesses

```python
def init_critical_witnesses(A, T):
    W_critical = set()
    for node in T.nodes:
        faces_in_node = [f for f in A.faces if f.interior_point in node.polygon]
        sample_size = max(1, len(faces_in_node) // 10)
        W_critical.update(random.sample(faces_in_node, sample_size))
        # Also sample vertex-witnesses
        ...
    return W_critical

def update_critical_witnesses(guards, W_all, W_critical, A, T):
    # Find what guards can and cannot see
    unseen = compute_unseen(guards, W_all, T)
    newly_unseen = unseen - W_critical
    
    if not newly_unseen:
        return W_critical, False
    
    # Add a small constant-size random subset of newly unseen witnesses
    num_to_add = NUM_CORES  # scale with parallelism
    to_add = random.sample(list(newly_unseen), min(num_to_add, len(newly_unseen)))
    W_critical.update(to_add)
    return W_critical, True
```

### 5.4 Split Types

Five types, chosen randomly per the normal protocol:

```python
SPLIT_PROBS = {
    'angular': 0.60,
    'visibility_line': 0.20,
    'chord': 0.10,
    'extension': 0.10,
    'square': 0.00  # only when face has >1 reflex vertex
}

def split_face(face: ConvexFace, A: Arrangement, lambda_: float):
    if len(face.reflex_vertices_on_boundary) > 1:
        square_split(face, A)
        return
    
    split_type = choose_split_type(face, SPLIT_PROBS)
    
    if split_type == 'square':
        square_split(face, A)
    elif split_type == 'angular':
        angular_split(face, A, lambda_)
    elif split_type == 'chord':
        reflex_chord_split(face, A)
    elif split_type == 'extension':
        extension_split(face, A)
    elif split_type == 'visibility_line':
        visibility_line_split(face, A)
```

#### 5.4.1 Square Split

```python
def square_split(face: ConvexFace, A: Arrangement):
    """
    Divide face using a horizontal and a vertical segment through its centroid.
    Each new face has half the width and height.
    Advantage: easy, ensures ≤1 reflex vertex per child face.
    Disadvantage: does NOT reduce angular capacity when face is adjacent to a reflex vertex.
    """
    cx, cy = centroid(face)
    h_seg = Segment(Point(face.xmin, cy), Point(face.xmax, cy))
    v_seg = Segment(Point(cx, face.ymin), Point(cx, face.ymax))
    A.add_segment(h_seg.clip_to_face(face))
    A.add_segment(v_seg.clip_to_face(face))
```

#### 5.4.2 Angular Split

```python
def angular_split(face: ConvexFace, A: Arrangement, lambda_: float):
    """
    Shoot a ray from a visible reflex vertex to bisect the face's angular capacity.
    Use 2^k direction quantization (λ = 2^{-k}) to avoid trigonometric functions.
    Angular directions: { (cos(π·j·λ), sin(π·j·λ)) : j = 0,...,1/λ-1 }
    Effect: reduces angular capacity to at most 4πλ.
    """
    r = worst_reflex_vertex(face)  # the one maximizing α(r, face)
    direction = bisecting_quantized_ray(r, face, lambda_)
    ray = shoot_ray_from(r, direction, P, A)
    A.add_segment(ray.clip_to_face(face))
```

#### 5.4.3 Reflex Chord Split

```python
def reflex_chord_split(face: ConvexFace, A: Arrangement):
    """
    If a reflex chord chord(r1, r2) properly intersects face, use it to split.
    Critical for correctness of Lemma 8 (Face-Point-Replacement).
    """
    for r1 in P.reflex_vertices:
        for r2 in P.reflex_vertices:
            if r1 != r2 and sees(r1, r2):
                chord = chord_segment(r1, r2)
                if chord.properly_intersects(face):
                    A.add_segment(chord.clip_to_face(face))
                    return
    # No reflex chord found → try other split type
```

#### 5.4.4 Extension Split

```python
def extension_split(face: ConvexFace, A: Arrangement):
    """
    For each reflex vertex r, the two 'extension rays' are parallel to r's two incident edges.
    If such an extension properly intersects face, use it.
    Useful when optimal guard lies exactly on an extension.
    """
    for r in P.reflex_vertices:
        for ext in extensions_of(r, P):
            if ext.properly_intersects(face):
                A.add_segment(ext.clip_to_face(face))
                return
```

#### 5.4.5 Visibility Line Split

```python
def visibility_line_split(face: ConvexFace, A: Arrangement, guards, unseen):
    """
    Two cases:
    (a) face is a face-guard: find witness w seen by face but not by other guards,
        and where face only partially sees w. Split face using ∂vis(w) ∩ face.
    (b) face is unseen face-witness: find guard g that only partially sees face.
        Split face using ∂vis(g) ∩ face.
    Particularly effective for low-vision-stability polygons.
    """
    if face in guards:
        w = find_partial_witness(face, guards)
        if w:
            split_line = vis_boundary_intersection(w, face)
            A.add_segment(split_line)
    elif face in unseen:
        g = find_partial_guard(face, guards)
        if g:
            split_line = vis_boundary_intersection(g, face)
            A.add_segment(split_line)
```

### 5.5 Unsplittable Face Criteria

A face f is declared *unsplittable* iff ALL of:
1. f is incident to at most one reflex vertex
2. No reflex chord properly intersects f
3. No extension split is possible on f
4. f is not splittable by angular splits at current granularity λ
   (equivalently: `capacity(f) ≤ 4πλ`)

When f is unsplittable with `δ ≥ 8πλ`, all conditions of Lemma 8 are satisfied.

---

## 6. Integer Program Formulations

### 6.1 Normal IP

```
Variables:
  x[c] ∈ {0,1}   for c ∈ C          (1 = guard placed here)
  y[w] ∈ {0,1}   for w ∈ face(W*)   (1 = this face-witness is unseen)

Minimize:
  Σ_{c∈vertex(C)} x[c]  +  (1+ε)·Σ_{c∈face(C)} x[c]  +  ε·Σ_{w∈face(W*)} y[w]

Subject to:
  Σ_{c∈VIS(w)} x[c] ≥ 1           ∀ w ∈ vertex(W*)    [hard: all vertex-witnesses seen]
  y[w] + Σ_{c∈VIS(w)} x[c] ≥ 1   ∀ w ∈ face(W*)      [soft: face-witnesses or pay ε]
  x[c] ∈ {0,1},  y[w] ∈ {0,1}
```

### 6.2 Big IP

Used to find any solution that forces a splittable face (either as guard or unseen witness).

```
Let s = value of Normal IP (# guards)

Variables: x[c], y[w] as above

Maximize:
  Σ_{x ∈ splittable(W∪C)} x_var

Subject to:
  Σ_{c∈C} x[c] = s                               [same number of guards]
  Σ_{c∈VIS(w)} x[c] ≥ 1        ∀ w ∈ vertex(W)  [all vertex-witnesses seen]
  
  For splittable w ∈ splittable(face(W)):
    (1 - ε·Σ_{c∈VIS(w)} x[c]) ≥ y[w]            [y[w]=1 only if w not fully seen]
  
  x[c], y[w] ∈ {0,1}
```

### 6.3 Practical IP Notes

- Use two-stage approach for numerical stability (avoid small ε).
- Recommended solvers: IBM ILOG CPLEX, Gurobi, or (free) GLPK / CBC / HiGHS.
- IP solving dominates CPU in practice (see Figure 6 of paper: ~70% of runtime).
- Critical witness trick reduces IP size by ~8-10× in practice.

---

## 7. Weak Visibility Polygon Tree

### 7.1 Construction

```python
def build_weak_visibility_polygon_tree(P: Polygon) -> WVPTree:
    """
    Build the WVP tree via BFS/DFS.
    Each node = weak visibility polygon of some chord of P.
    Reference algorithm: Hengeveld, Miltzow, Staals (follow-up work).
    Alternative: Abrahamsen 2013 master's thesis (slower in practice).
    """
    e0 = random_boundary_edge(P)
    root_wvp = weak_visibility_polygon(P, e0)
    root = WVPNode(polygon=root_wvp, defining_chord=e0, children=[])
    
    queue = [root]
    while queue:
        node = queue.pop()
        for e in node.polygon.interior_edges():  # edges not on ∂P
            child_wvp = weak_visibility_polygon(P_minus_already_covered, e)
            child = WVPNode(polygon=child_wvp, defining_chord=e, parent=node)
            node.children.append(child)
            queue.append(child)
    
    return WVPTree(root=root)
```

### 7.2 Visibility Pruning (Key Lemma)

**Lemma:** If p and q are in the interiors of two WVP-tree nodes that are neither siblings nor in a parent-child relationship, then p and q **cannot see each other**.

```python
def can_possibly_see(node_p: WVPNode, node_q: WVPNode) -> bool:
    return (node_q in node_p.children or
            node_p in node_q.children or
            node_p.parent == node_q.parent)  # siblings
```

This prunes visibility queries dramatically:
- 60-vertex polygons: saves 16.7% of queries
- 500-vertex polygons: saves 87.3% of queries

### 7.3 Properties

- Tree size ≈ n / (avg WVP size)
- Largest WVP node: ~20-28 vertices regardless of n (see Table 3)
- Largest reflex count per node: ~6-9 regardless of n
- This justifies chord-visibility width as a practical parameter

---

## 8. Correctness — Reliability Guarantee

The algorithm is *reliable*: if it reports an optimal solution, it IS optimal — regardless of whether P is vision-stable.

**Proof sketch:**
1. Let G be the returned point-guards with all face-witnesses seen.
2. G guards all of P → |G| ≥ opt.
3. Let F be an optimal point-guard set. `face(F)` sees all vertex-witnesses. The IP could use `face(F)` as face-guards (cost: opt + ε·|some unseen faces|). Since IP returned point-guards of size |G|, we get |G| < opt+1.
4. With ε chosen small enough: |G| = opt. ∎

**When P is vision-stable with stability δ:**  
Additionally, `s ≤ opt` and `s ≥ opt` both hold (see Section 3 of paper), so the IP value equals opt.

---

## 9. FPT Algorithms

### 9.1 Reflex-FPT (Corollary 3)

For vision-stable P with fixed δ, the one-shot algorithm is FPT in r (number of reflex vertices):
- IP size depends only on r and δ, independent of n.
- Preprocessing: O(r⁸/δ⁴ · log n + n log n)

### 9.2 Chord-Width-FPT (Theorem 4)

FPT algorithm parameterized by chord-visibility width k = cw(P):

```
Running time: 2^{O(k^7/δ²)} · n^{O(1)}
```

Uses dynamic programming on the WVP tree:
- Each node has ≤ k vertices (Lemma 12) and ≤ k windows/children (Lemma 13).
- For vertex-guarding: table per node over subsets of k vertices → 2^{k³} · n^{O(1)}.
- For point-guarding with vision-stability: candidate set per node has O(k⁶/δ²) vertices → 2^{O(k^7/δ²)} · n^{O(1)}.

Both algorithms are **theoretical contributions only** — prohibitively slow in practice.

---

## 10. Implementation Roadmap

### Phase 1: Core Geometry Engine

```
[ ] Exact arithmetic (Python: fractions.Fraction or mpq; C++: CGAL EPEC kernel)
[ ] Polygon input/output (AGPLIB format or custom)
[ ] Reflex vertex detection
[ ] Visibility query: does segment seg(p,q) lie inside P?
[ ] Visibility polygon computation: vis(q)
[ ] Shortest-path map inside polygon
[ ] Chord computation: chord(a, b)
```

### Phase 2: Arrangement Construction

```
[ ] Segment intersection (exact)
[ ] Arrangement data structure (CGAL Arrangement_2 or custom DCEL)
[ ] Face extraction (all convex faces of arrangement clipped to P)
[ ] Angular capacity computation per face
[ ] Representative selection per face
[ ] Reflex chord enumeration
[ ] Extension ray shooting
```

### Phase 3: Weak Visibility Polygon Tree

```
[ ] Weak visibility polygon computation (vis(edge e) w.r.t. P)
[ ] Tree construction (BFS from root edge)
[ ] node(p) lookup for a point p
[ ] Sibling/parent-child relationship query
[ ] Shortest path map per WVP node (for visibility queries)
```

### Phase 4: Integer Programming Interface

```
[ ] IP solver integration (CPLEX / Gurobi / HiGHS / CBC)
[ ] Normal IP builder
[ ] Big IP builder
[ ] Two-stage solve procedure
[ ] Solution extraction: vertex-guards vs. face-guards, unseen witnesses
```

### Phase 5: Iterative Algorithm

```
[ ] Initialization: arrangement + WVP tree
[ ] Critical witness management
[ ] Visibility computation loop (WVP-pruned)
[ ] Split dispatch (5 types, random selection)
[ ] Granularity tracking and update
[ ] Termination detection (reliability check)
[ ] Parallelism for visibility queries (embarrassingly parallel)
```

### Phase 6: Testing & Validation

```
[ ] Test on AGPLIB library instances (www.ic.unicamp.br/~cid/Problem-instances/Art-Gallery)
[ ] Cross-validate against Tozoni et al. implementation
[ ] Test on irrational-guard polygon (Abrahamsen et al. 2017)
[ ] Measure Hausdorff distance convergence for non-vision-stable inputs
[ ] Benchmark against Table 1 targets: 60-vertex ≈ 0.39s, 500-vertex ≈ 18.2s
```

---

## 11. Key Algorithmic Subtleties

### 11.1 Avoiding Trigonometric Functions

The real RAM model (and practical exact arithmetic) cannot handle sin/cos. For angular splits, use:
```
Quantized directions at angles { π·j·λ : j = 0, 1, ..., 1/λ - 1 }
where λ = 2^{-k}

These can be represented as rational (cos, sin) approximations, or
use coordinate-based directions: rotate by 90° successively and bisect.

Key bound: for α ∈ [0, π/4]: α/2 ≤ sin(α) ≤ α
So angle between consecutive rays: πλ ≤ α ≤ 4πλ
```

### 11.2 Face Guards vs. Point Guards

The algorithm uses **both** faces and points as candidate guards during iteration:
- Face-guards are "coarser" approximations — they cover regions, not points.
- A solution is final only when all guards are point-guards.
- Face-guards serve as splittable placeholders that drive refinement.

### 11.3 The ε Weight

Choose ε = 1 / (|C| + |W| + 1).

This ensures that the fractional difference between a face-guard solution and a vertex-guard solution is < 1, so the IP integer rounding correctly prefers vertex-guards.

In two-stage variant: multiply objective by 1/ε to get large integers (IP solvers are numerically more stable with large integers than small rationals).

### 11.4 Why Splitting Works

When the Normal IP uses a face-guard or leaves a face unseen:
- If that face is splittable: split it → new smaller faces → new vertex candidates that the IP can use next iteration.
- If all faces are unsplittable: every face has capacity ≤ 4πλ, and by Lemma 8 (Face-Point-Replacement), `vis_{-δ}(face) ⊆ vis(representative)` for δ ≥ 8πλ. So if there exists an optimal solution, the IP will find it among vertex-candidates.

### 11.5 Face-Point-Replacement Lemma (Lemma 8)

This is the core technical lemma enabling the algorithm. Sufficient conditions for `f' ⊆ vis_{γ+δ}(p)` where p = representative(f):

1. `capacity(f), capacity(f') ≤ δ/2`
2. Neither f nor f' is properly intersected by a reflex chord
3. f has at most one reflex vertex on its boundary
4. `vis_γ(f) ∩ int(f') ≠ ∅` for some `γ ∈ [-δ, 0]`

Intuition: a small convex region f that "almost sees" f' can be replaced by its representative point p with slightly enhanced vision, and p will still see all of f'.

### 11.6 Granularity vs Vision-Stability

- `λ` (granularity) is the algorithm's internal estimate of vision-stability.
- After final convergence: `δ ≤ 8πλ*` where `λ*` is the final granularity.
- Correlation between granularity and running time: 0.3–0.6 across polygon sizes (Table 2).
- The algorithm cannot compute vision-stability directly — granularity is a proxy.

---

## 12. Performance Targets

| Polygon size | Tozoni et al. (their HW) | Our algorithm |
|---|---|---|
| 60 vertices | 0.26 s | 0.39 s |
| 100 vertices | 0.94 s | 0.52 s |
| 200 vertices | 3.77 s | 2.02 s |
| 500 vertices | 35.04 s | 18.2 s |

Notes:
- CGAL runs 2-3× faster on Linux vs Windows — these times are Windows.
- IP solver matters: CPLEX/Gurobi >> GLPK/CBC. Use best available.
- Visibility queries: parallelizable — use all CPU cores.
- IP solving dominates runtime (~70%) after other optimizations are applied.
- Critical witnesses give 2.2–2.9× speedup vs no critical witnesses.
- WVP tree saves 16–87% of visibility queries (grows with polygon size).

---

## 13. Known Failure Modes

1. **Polygon #25 (100-vertex AGPLIB):** Extremely low vision-stability. Times out at 30 min with default parameters. Fix: increase probability of visibility-line splits.

2. **Square split infinite loop:** A polygon with two guards at exact positions can never have the "wrong" face fully visible after any number of square-splits (see Figure 16a). Mitigation: always allow angular and other split types; use normal protocol not square-only.

3. **Non-vision-stable polygons:** The irrational-guard polygon (Abrahamsen et al. 2017) requires irrational coordinates. The algorithm never terminates in exact sense but converges exponentially fast (Hausdorff distance ≈ 2^{-50} after 300 iterations; Figure 7). This is an open question for general convergence proof.

4. **Collinear configurations (P1):** Polygon P1 in Figure 9 (left) is not vision-stable due to collinearities. These are measure-zero but arise in synthetic polygons. Algorithm handles them in practice via extension/chord splits.

---

## 14. Dependencies

### Recommended Stack (C++)

```
CGAL 4.13+ (Exact_predicates_exact_constructions_kernel)
  - Arrangement_2
  - Polygon_2 
  - Shortest_path
  - Visibility_2
IBM ILOG CPLEX 12.10+ or Gurobi 9+   [IP solver]
Boost 1.65+                           [general utilities]
OpenMP or TBB                         [parallelism for visibility]
```

### Recommended Stack (Python, slower but simpler)

```
shapely          [polygon geometry]
sympy / fractions [exact arithmetic for critical operations]
python-igraph    [arrangement structure]
scipy.optimize   [for LP relaxations]
PuLP or OR-Tools [IP modeling]
CBC / HiGHS      [free IP solvers via PuLP]
networkx         [WVP tree]
```

### Data

```
AGPLIB: www.ic.unicamp.br/~cid/Problem-instances/Art-Gallery
  - Formats: 60, 100, 200, 500 vertex random simple polygons
  - 30 instances per size class
Irrational-guard polygon: see Abrahamsen et al. 2017 (arXiv:1701.05475)
```

---

## 15. Pseudocode Summary

### One-Shot Algorithm

```
OneShot(P, δ):
  A ← BuildArrangement(P, δ)
  C ← vertex(A) ∪ face(A)
  W ← interior_points(A) ∪ face(A)
  VIS ← ComputeAllVisibilities(C, W)
  (G, U) ← SolveIP(C, W, VIS)
  if G ⊆ vertex(C) and U = ∅:
    return G
  else:
    return "vision-stability < δ; retry with δ/2"
```

### Iterative Algorithm

```
Iterative(P):
  (A, T) ← Init(P)
  λ ← 1/16
  W* ← RandomSample(face(A), 10%)
  
  loop:
    C ← Candidates(A)
    VIS* ← Visibilities(C, W*, T)
    (G, U) ← SolveNormalIP(C, W*, VIS*)
    
    (W*, changed) ← UpdateCriticalWitnesses(G, W_all, W*, T)
    if changed: continue  // re-solve
    
    if AllPointGuards(G) and SeesAll(G, W_all, T):
      return ExtractPoints(G)
    
    splittable ← SplittableFaces(G ∪ U)
    if splittable:
      Split(splittable, A, λ)
    else:
      (G2, U2) ← SolveBigIP(C, W_all, VIS_all)
      splittable2 ← SplittableFaces(G2 ∪ U2)
      if splittable2:
        Split(splittable2, A, λ)
      else:
        λ ← λ/2   // lower granularity estimate
```

---

## 16. Open Questions (from paper)

1. Does the iterative algorithm always converge to the optimal solution, even for non-vision-stable polygons? (Empirically yes, theoretically open.)
2. Can the algorithm be extended to polygons with holes? (Main bottleneck: visibility in polygons with holes.)
3. What is the smallest IP that guarantees optimality for vision-stable polygons? (Current bound has room for improvement.)
4. Can the algorithm handle orthogonal polygons, von Koch polygons, spike polygons?
5. Is NP = ∃ℝ? (This would imply polynomial-size witnesses for the art gallery problem exist, contradicting the paper's motivation.)