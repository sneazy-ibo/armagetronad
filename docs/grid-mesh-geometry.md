# eGrid mesh geometry & the largest-arena bug — debug notes

*Written 2026-07-07 from a full read of `eGrid.cpp` (+ `eCoord.h`, `gArena.cpp`,
`gParser.cpp`). Context: a bug seen in local games at the largest arena size, possibly
`TASKS.md` #9 ("big slide": new wall begin stamped ahead of the turn point). This doc
holds the exact geometry facts + precision analysis + how to see the mesh, so a debug
session can start immediately.*

## The world triangle (the whole grid lives inside one triangle)

Construction (`eGrid::Create()`, eGrid.cpp:2075):
```
base = (20, 100)                     |base| = √10400 ≈ 101.98
A = base·(1, 0)        = (20, 100)           (Turn() = complex multiplication)
B = base·(−.5, .87)    ≈ 120° rotated
C = base·(−.5, −.87)   ≈ −120° rotated
```
- **Shape: near-equilateral, NOT right-angled.** All corners ≈60°. But note `.87` vs the
  exact `sin 120° = 0.8660254`: |(−.5,.87)| = 1.00343, angle = 119.89°. So **B and C sit
  0.34% farther out than A and the angles are off by ~0.11°** — the world triangle is
  deliberately(?) slightly irregular.
- **Direction: not axis-aligned.** Vertex A points toward atan2(100,20) ≈ **78.7° CCW
  from +x** ("north-north-east"). Probably intentional: grid edges start out
  non-parallel to the axis-aligned gameplay walls, dodging degenerate parallel cases.
- The rim = 3 **unsplittable eWalls**; everything (map rim included) is triangulated
  inside it. Interior faces are whatever the incremental constrained triangulation
  produces — sliver triangles are routine, and **edges fan from gameplay points out to
  the world corners** (they can be ~100k units long on a big arena).

## Growth (`Grow()`, eGrid.cpp:2147)

`base = base.Turn(−4, 0)` → multiply by (−4,0) = **scale ×4 AND rotate 180°** (the
triangle flips direction every growth). Old rim walls are cleared; six new faces stitch
old triangle to new.

| growths | circumradius | note |
|---|---|---|
| 0 | ~102 | initial |
| 1 | ~408 | |
| 2 | ~1 632 | |
| 3 | ~6 527 | |
| 4 | ~26 107 | |
| 5 | ~104 429 | ← reached at menu-max arena |
| 9 | ~26.7M | last with r² < 1e15 |
| 10 | ~106.9M | **final; then Grow() refuses** (`se_maxGridSize = 1E+15` on r²) |

`Range(n²)` grows while `n²·4 > maxNorm²`, i.e. guarantees inserted points satisfy
|p| ≤ r/2 — the **incircle** of an equilateral triangle (inradius = circumradius/2), so
containment holds regardless of the flip. Consequence: points beyond ~53M can never be
guaranteed inside → `DrawLine` "Point does not have an eFace" / give-up paths. **At
menu-max arena the cap is nowhere near reached — the bug is precision, not the cap.**

## Menu-max arena numbers (SIZE_FACTOR 10)

- `sizeMultiplier = 2^(sizeFactor/2)` (gGame.cpp `exponent()`), menu range −10…+10
  step .5 → max mult = 2^5 = **32**.
- Default map square is (0,0)–(500,500) → scaled: **16 000 × 16 000**, far corner
  |(16000,16000)| ≈ 22 627. → 5 growths, world radius ~104k.

## Float precision at this scale (REAL = 32-bit float, 24-bit mantissa)

| quantity | magnitude | ulp (spacing) |
|---|---|---|
| gameplay coords | ~16k–23k | **~2 mm** |
| world-corner coords / void-edge endpoints | ~104k | **~8–16 mm** |
| accumulated `distance` after a long round | ~100k+ | **~8 mm** |
| products in intersection math (`a·b`) | ~1e9–1e10 | relative 1.2e-7 |

Notes:
- **`eCoord::operator==` is fuzzy with magnitude-scaled tolerance**:
  `dist² ≤ EPS²·(|a·b|+|a×b|)` with `EPS = 1e-7` (defs.h:61). At gameplay scale on the
  max arena two points ~2 mm apart compare equal. Point *merging* on insert uses this.
- **Wall segments live in distance space** (`dbegin`, `gPlayerWallCoord::Pos` are
  *cumulative driven distance*). Distance only grows during a round; its ulp grows with
  it. A turn's wall-begin computed as `distance − F(dir, pos − lastTurnPos)` mixes a
  large accumulated distance with a small local offset → **the wall's start position
  quantizes to ~cm as the round progresses on a big arena**. This is the cleanest
  mechanism matching "#9 new wall begin stamped ahead of the turn point" — and it
  needs no network, matching "local games". (Unverified hypothesis — instrument it.)
- Intersections against 100k-long void edges (`IntersectWithCareless`,
  ratio-clamped to [1e-5, 1−1e-5]) carry ~1–2 cm error; near-parallel pairs fall back
  to an "inverse center of gravity" average (eGrid.cpp:2276) — pure fabrication of a
  point, by design, for degenerate input.

## Self-healing & give-up inventory (what can silently distort geometry)

| mechanism | where | effect |
|---|---|---|
| `eFace::CorrectArea()` | eGrid.cpp:1755 | flipped (negative-area) triangle healed **by moving a grid point** (only wall-free points — `Movable()` guard) |
| `DrawLine` timeout (10 000 steps) | eGrid.cpp:721 | wall insertion **silently stops partway**, `requestCleanup` set |
| `FindSurroundingFace` timeout | eGrid.cpp:2547 | falls back to O(n) scan over all faces (DEBUG prints a warning) |
| parallel-intersection fallback | eGrid.cpp:2276 | invented intersection point |
| ratio clamp | DrawLine | intersection forced into [1e-5, 1−1e-5] of edge |
| fuzzy `==` point merge | Insert/DrawLine | nearby points collapse (tolerance grows with coords) |
| `SimplifyAll` after `requestCleanup` | eGrid.cpp:2045 | mass CorrectArea pass — many points may move at once |

On a max-size arena all tolerances are ~10× the default arena's, and CorrectArea
passes get more frequent as slivers flip. Any of these moving a face that a wall's
danger-interval data was computed against ⇒ collision tests (`IsDangerous(alpha,
time)`) can disagree with drawn geometry.

## Seeing the mesh (yes, we can draw it on the floor)

There's no existing triangulation renderer, but all the pieces exist:
- Every edge is enumerable: `grid->edges` (`tList`-style, `edges(i)`), each `eHalfEdge`
  has `Point()` / `Other()->Point()`.
- `eDebugLine` (engine/eDebugLine.h) already renders world-space lines every frame —
  `eDebugLine::Render()` is called from eDisplay.cpp:563, `Update()` from eCamera.
- Plan (~40 lines, render-only, zero sim impact): a `tSettingItem<bool>`
  `GRID_DEBUG_DRAW`; in `eGrid::Render` (eDisplay.cpp:617), when set, loop edges and
  draw each at z ≈ 0.05 over the floor. Color code: **red** = edge carries a wall,
  **dim grey** = plain triangulation edge, **green** = the 3 world-rim edges,
  **yellow** = faces whose `CorrectArea()` fired this round (needs a 1-bit flag on
  eFace, set in CorrectArea). Camera far above at max arena → also worth a console
  dump variant (`GRID_DUMP` → write all edges to a file for offline gnuplot) for
  when the overlay is too dense.

## When the bug next reproduces — capture list

1. **DEBUG build** (`Check()` asserts + the FindSurroundingFace warning print).
2. **tRecorder recording running** — grid ops are self-checked in recordings
   (`_GRID_ADD_EDGE` etc.), and a recording makes the failure replayable forever.
3. `GRID_DEBUG_DRAW` on (once implemented) — watch for yellow (healed) faces near the
   glitch, walls whose red edge disagrees with the drawn trail.
4. Note: SIZE_FACTOR, map, WALLS_LENGTH (finite vs infinite changes mesh churn),
   round duration at failure (distance-precision hypothesis predicts **late-round**
   failures), single vs multiple cycles.
5. If the symptom is the cycle sliding/teleporting: in local games `correctPosSmooth`
   should be inert (no syncs) — if it's nonzero anyway, that's a finding by itself.
   Check `MoveSafely` and wall-drop paths instead.

## Observed symptoms (user, 2026-07-07 — confirmed same bug as TASKS #9)

- Sometimes **tiny slips**, sometimes **massive leaps** (the "big slide").
- **Dominant symptom: the misplaced wall begin leaves a GAP that can be driven
  through.** Trail joining up/wrapping is the rare case (<5%); the gap is the norm.
  So the usual failure is the new wall starting *ahead* of the turn point (gap
  behind it = passable), occasionally *behind* a prior segment (overlap = "join").
  Both directions of the same misplacement — consistent with quantization error
  that can land either side of the true value.
- Occurs with **WALLS_LENGTH 400** (finite trail). Two implications:
  1. **Mesh churn is constantly active**: finite trails mean the tail expires
     continuously → walls leave edges → background `Simplify`/point-merge/
     `CorrectArea` runs all round. The "discrete event" mechanisms (big leaps)
     are live in exactly this mode; with infinite walls the mesh only grows and
     those paths mostly idle.
  2. **The trail tail is computed by big-number subtraction**: tail position ≈
     values derived from `distance − wallsLength` — two large, nearly-equal
     floats late in a round (both ~10⁵ at ulp ~8mm). The visible tail (and its
     danger interval) inherits ~cm error that *marches* with the cycle. A tail
     endpoint quantizing past a later segment's `Pos` inverts segment order in
     distance space → exactly a "trail joins up/wraps" render.

Reading the symptoms against the hypotheses:
- Tiny-slip vs massive-leap bimodality fits *two* mechanisms stacking: continuous
  ulp-level distance quantization (slips) + a discrete event — a CorrectArea point
  move, a fuzzy point-merge, or a wall-split misplacement (leaps).
- **"Trail joins up over time" is the strongest clue**: trail segments are intervals
  in cumulative-distance space (`dbegin`/`Pos` along the wall). If accumulated
  `distance` loses precision, *distinct* turn points can quantize to the same or
  out-of-order distance values → segments visually merge/bridge. "Over time" =
  precision decays as distance grows through the round — direct support for the
  distance-space hypothesis.

## Open questions (answers would sharpen the hypothesis)

- Does it appear only late in a round / after much driving? (Distance hypothesis
  predicts: never right after spawn.)
- Menu-max (SIZE_FACTOR 10) or console-set larger? (Beyond ~1e7 coordinates the
  Grow() cap becomes real and adds a third failure mode.)
- WALLS_LENGTH finite (mesh constantly simplifying) vs infinite (mesh only grows)?
