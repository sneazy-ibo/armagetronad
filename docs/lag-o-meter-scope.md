# Lag-o-meter #6 / #7 — scope

The lag-o-meter is a client-side overlay drawn around your cycle showing the
network-lag uncertainty region (where you *might* actually be vs where you're
drawn). Cosmetic, `#ifndef DEDICATED`, **no network/wire/determinism impact** —
a low-risk area to work in.

## Where it lives

All in `src/tron/gCycle.cpp`:
- `namespace gLaggometer` (3886–4023): `Colour`, `DirectionTransformer`,
  **`LagOMeterRenderer`** (the live one), `AxesIndicator`.
- Call site 4666–4693 (the `else` of `ENABLE_OLD_LAG_O_METER`): sets up the local
  frame — `glTranslatef(0,0,h)`, `glScalef(f,f,f)` where `f = 2*speed*scale`,
  `glTranslatef(-l,0,0)` — then `LagOMeterRenderer(this).render(l)` if
  `f*l > LAG_O_METER_THRESHOLD`.
- There is also an **old** renderer (`sg_laggometerUseOld`, line 4642, a
  `BeginLineLoop`). It's opt-in legacy — **target the new one**, ignore the old.

### What it draws today

`LagOMeterRenderer::render` draws **two opaque `BeginLineStrip` outlines** (one per
winding direction) via the recursive `drawTriangle`. Inputs: `lag` (`Lag()`,
distance units), `delay` (`GetTurnDelay()`), and grid axis directions
(`DirectionTransformer` → `grid->GetDirection(i)`). Colour is player colour blended
toward white, **3-component, no alpha, no fill, wall-unaware**. Geometry is purely
analytic from lag+delay+axes.

Note: `Colour::toGl()` is raw `glColor3f` and `drawTriangle` uses raw `glVertex2f` —
so this unit is also part of R-1's geometry leak. If you touch it, convert these to
`Color()`/`Vertex()` in passing (advances R-1a for free in one small unit).

## #6 — SUPERSEDED — was a mis-scope of the trail-end fade

> **This section is obsolete.** #6 ("drive-through zone at the trail end") was *not*
> about the lag-o-meter spiral below — the user meant the cycle **wall/trail's
> disappearing tail end**. It SHIPPED 2026-06-29 as `TRAIL_END_FADE` in `gWall.cpp`
> (a flat 0.5-alpha band of length `speed*lag` at the trail tail). See dev-log
> 2026-06-29, TASKS #6, and the "Cycle walls / trails" notes in `sharp-edges.md`.
> The lag-o-meter-fill design below was never built; kept only for history.

**Idea (NOT BUILT):** fill the lag-uncertainty region at the trail end as a translucent
(alpha 0.5) polygon instead of just an outline, marking the area lag still lets you
drive through.

**It's small because the geometry already exists.** `drawTriangle` already computes
the triangle's vertices in the right local frame. The work:
1. Add a **fill pass**: `BeginTriangleFan()` (not `BeginLineStrip`) over the same
   vertices, then keep the outline pass on top if wanted.
2. **Alpha**: give `Colour` an `toGl(REAL a)` / use the `Color(r,g,b,a)` free fn;
   the current `toGl()` is 3-component only.
3. **Blend on**: the meter currently only `glDisable(GL_TEXTURE_2D)`. Add
   `glEnable(GL_BLEND)` + `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` around
   the fill (or `renderer->SetFlag(ALPHA_BLEND,true)` once state goes through the
   abstraction). Restore after.
4. A `tSettingItem<bool>` / blend config to toggle it (mirror
   `LAG_O_METER_BLEND/THRESHOLD`).

**Effort:** low — one fill pass + alpha + a toggle, contained to `LagOMeterRenderer`.

**Design decided (user, 2026-06-28):** the drive-through zone is the **sub-region at
the trail end** — *not* the whole lag triangle. So #6 fills only the rear portion of
the shape (the area right behind the cycle where lag still lets you pass), at alpha
0.5. Concretely: fill the trail-end vertices of `drawTriangle`'s output, not the full
fan. Exact rear extent is a tuning knob (`tSettingItem<REAL>`), start with the
turn-delay segment near `loc=(0,0)`.

## #7 — reachability-based shape (MEDIUM; reframed by user 2026-06-28)

**Reframed idea (cleaner than "clip to walls"):** the meter edge should be the
boundary of *everything the player could actually have reached* within the lag
uncertainty time — i.e. run the look-ahead and draw the line at the reachable edge.
This automatically handles the awkward case the user named: another player drove
into a wall and left a **hole**, and you can drive through it. No special-casing —
because a hole is simply absence of an `eHalfEdge`, so a look-ahead/sensor cast
reports straight through it.

**This maps onto existing machinery — that's the point of "just run the look-ahead":**
- `drawTriangle`'s recursion **already enumerates the turn-branching reachable set**
  (it branches every `turnDelay` along the grid axes). It just expands each branch by
  the raw `lag` distance, ignoring walls.
- Replace each branch's extent with a **wall-limited** one: `MaxSpaceAhead(cycle, ts,
  lookAhead, maxReport)` (gCycleMovement.cpp:2031) and/or `gSensor::detect` already
  answer "how far can I drive in this direction before a wall, on the live grid."
  Holes included for free.
- So #7 ≈ thread a per-branch sensor cast into the existing recursion and stop the
  branch at the reported distance.

**Hazards / why still medium:**
- **Read-only probing only.** Use `MaxSpaceAhead`/`gSensor::detect` (non-mutating).
  Do **NOT** drive the real `Timestep`/`DoTurn`/`TimestepCore` to "simulate" — those
  mutate cycle state and would corrupt physics / break record-playback determinism.
  (If a throwaway sim is ever wanted, `gCycleExtrapolator` is the existing
  prediction-cycle pattern — but sensors are far cheaper and enough here.)
- **Frame mapping:** sensors cast in world coords; the meter draws in the
  `DirectionTransformer`-conjugated, `f`-scaled, `-l`-translated local frame. Hit
  *distances* are scalars along a direction, so it's a 1:1 divide-by-`f` — simpler
  than a full transform, but get the origin/direction right.
- **Cost:** one sensor cast per branch per **render** frame (a handful to ~a dozen).
  Cheap individually, but new per-frame render cost — throttle (recompute every N
  frames / cache the shape between turns) if it shows up.

**Effort:** medium, but well-grounded — the recursion and the look-ahead primitive
both already exist; the work is wiring them together + the scalar frame mapping.

## Recommendation

- **#6: worth doing** — cheap, self-contained, visible, design now settled (trail-end
  sub-region, alpha 0.5, rear-extent knob). It's a fill pass on existing geometry.
  Convert the unit's raw `glColor3f`/`glVertex2f` to `Color`/`Vertex` while there.
- **#7: do after #6** — now that it's reframed as "run the look-ahead and draw the
  reachable edge," it reuses `MaxSpaceAhead`/`gSensor` + the existing recursion, and
  holes work for free. Still medium effort; the throttle + frame-mapping are the real
  work. Build it on top of #6's geometry.

Both are render-only and safe to iterate on live (no wire/determinism risk).
