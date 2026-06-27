# Tasks / backlog

Durable backlog so a fresh session can pick up without prior context. Keep this in
sync at the end of a work session: tick off what shipped, add new items, note who/why.
Status: `[ ]` open · `[~]` in progress · `[x]` done · `[-]` dropped/moot (keep with reason).

## Active projects

### Server-browser responsiveness ("Option B")
Plan + findings: `docs/server-browser-cooperative-fetch.md`. Paused 2026-06-27.
- [x] Skip post-login syncs on master connect (`waitSync`) — connect 2.0s → 0.3s.
- [x] B-1: split `GetFromMaster` into Begin/Step/End.
- [x] B-4: open browser on cached list, stream master fetch in menu pump, prefetch
      first page (`BROWSER_PREFETCH_SECONDS`).
- [-] B-2 query-during-fetch — DROPPED: queries and master fetch share `peers[0]`.
- [-] B-3 `StartQueryAll` no-reset — MOOT: `Polling()` keys off `advancedInfoSetEver`,
      so re-query of answered servers doesn't flicker.
- [ ] B-5: eager master prefetch when the "Internet" menu item is *hovered* (not
      entered). Needs the pump to run on hover. Connect is cheap now (0.3s), so no
      cooperative-connect refactor required.
- [ ] B-6: thread master-timeout retry through the step machine (today
      `GetFromMasterBegin` recurses into the blocking `GetFromMaster`).

### std-library migration (scoped 2026-06-28 — see docs/std-library-migration-scope.md)
Goal: replace custom `tString` → `std::string` and `tJUST_CONTROLLED_PTR`/`tSafePTR`
(src/tools/tSafePTR.h) → `std::shared_ptr`/`std::unique_ptr`, so we can turn on
stricter safeguards (compiler warnings-as-errors, sanitizers, clang-tidy) that
previously fired false positives on the custom types.
- [x] Scope it → `docs/std-library-migration-scope.md`. Two independent halves;
      `tString` is the tractable one, smart-ptrs probably skippable. Headlines below.
- [ ] **Step 0 (do first):** turn safeguards on, record which diagnostic fires on
      which type. The "false positives" haven't been pinned to a type — measure
      before refactoring. Likely it's `tString`'s `tArray<char>` storage, not the
      pointers.
- [ ] **Part A:** reparent `tString`/`tColoredString` onto `std::string` (one class,
      not 99 files) — ~25 methods become wrappers, 1765 callsites unchanged. Trap:
      `Len()` is `strlen+1` (NUL stored in array); preserve it, don't map to `size()`.
- [ ] **Part B (probably skip):** intrusive refcount (`tReferencable`, 13 classes
      incl. `nMessage` on the frozen-wire net path) → `std::shared_ptr` is NOT a
      drop-in (intrusive vs control block, `tStackObject`, 89 AddRef/Release sites).
      Default: keep intrusive, confirm it's sanitizer-clean. Migrate only leaf
      classes if Step 0 proves they trip a check we want.
- [ ] Turn on the extra safeguards once a unit is migrated; fix what they surface.
- Note: during this project, modernising IS the task — but still one area at a time,
  building between steps. The CLAUDE.md "don't modernise in passing" rule is about
  *incidental* changes, not this sanctioned effort.

### Renderer abstraction / Metal (reviewed RCL's port — see docs/metal-port-review.md)
Lesson from the friend's Metal branch: a backend swap that doesn't first route
geometry through `rRenderer` produces a scaffold that can't draw the game (theirs
renders ~5%: clear+fonts+floor+logo, because ~170 raw `glBegin/glVertex` sites
bypass the abstraction). So the ordering below is a hard dependency chain.
- [x] R-0: Salvage `rMatrixState` (software matrix stack) + self-check. Lifted to
      `src/render/rMatrixState.{h,cpp}` + `rMatrixState_test.cpp` (passes). NOT in
      the build target yet — parked salvage for R-2/#1; no consumer.
- [ ] R-1 (**keystone, unblocks the rest**): route raw immediate-mode GL through
      `rRenderer`. SCOPED 2026-06-28 → `docs/r1-geometry-routing-scope.md`.
      Correction to the count below: `Begin*/End` + matrix-mode are **already**
      routed (rRender.h `#error`s `glBegin`); what leaks is the per-vertex calls
      *inside* those blocks. Real target (game code): **137** raw
      `glVertex/glTexCoord/glColor` → `Vertex/TexCoord/Color` (R-1a) + 3 stray debug
      `glBegin` blocks; plus ~91 raw matrix calls → `PushMatrix/…` (R-1b, separable).
      Mechanical 1:1; byte-identical on the current GL backend (verify per file, zero
      visual diff). Heaviest: `gCycle` 49, `eDisplay` 39, `gWall` 26.
      **Ponytail:** zero user-visible value alone — start only when R-2 or #1 is
      committed; don't do it speculatively.
- [ ] R-2 (**blocked by R-1**): Metal (or GL core profile) backend. Only worth it
      once R-1 lands. If we do it, take the lessons not their code: implement
      texture wrap, polygon offset, blendFunc/alphaFunc, a buffer ring; avoid the
      `#define gl* sr_metal_gl*` macro shim (it caused a recursion crash on theirs
      and only exists because geometry bypasses `rRenderer` — R-1 removes the need).
- [ ] R-3 (**blocked by R-2**): in-game menu item to select the rendering backend.
      FEASIBLE & low-effort: a `uMenuItemSelection<int>` bound to the
      `ARMAGETRON_GRAPHICS_BACKEND`-style config, applied via the existing
      `sr_ReinitDisplay` path (the same `sr_ExitDisplay`+`sr_InitDisplay` the
      resolution menu uses, `gMenus.cpp:283`) — window flags + context get rebuilt,
      textures/lists already regenerate on reinit. Pointless until R-2 makes the
      alternate backend actually render. Hide the item on non-Metal platforms.

## Open items (not started)
- [ ] #1 Decide: rip out OpenGL display lists for VBOs? (legacy frozen-geometry path)
      Related modern-GL debt: `gluBuild2DMipmaps` (deprecated on modern macOS) in
      `rTexture.cpp` → `glTexImage2D` + `glGenerateMipmap`. See docs/sharp-edges.md.
      NOTE: shares the R-1 prerequisite (route geometry through `rRenderer`) and can
      reuse the salvaged `rMatrixState` for a core-profile path. See metal-port-review.md.
- [x] #2 Fix sound after the SDL2→SDL3 move. Ported the device glue in `eSound.cpp`
      to SDL3 (`SDL_OpenAudioDeviceStream` + get-callback, lock/pause/destroy);
      restored real audio locking; dropped the dead "Buffer Length" menu knob and
      replaced it with a master **Volume** control (native `SDL_SetAudioStreamGain`).
      All Sound-menu items apply live. User confirmed audio + volume work.
      Music (fire.xm/SDL_mixer) is WIN32-only — out of scope. See dev-log + sharp-edges.
- [ ] #6 Lag-o-meter: 0.5-opacity "drive-through" zone at the trail end. SCOPED
      2026-06-28 → `docs/lag-o-meter-scope.md`. SMALL/recommended: `gLaggometer::
      LagOMeterRenderer` in gCycle.cpp already computes the triangle vertices — add a
      `BeginTriangleFan` fill pass + alpha + blend + toggle. Design DECIDED: fill only
      the **trail-end sub-region** (rear of the shape), not the whole triangle; rear
      extent is a tuning knob. Render-only, no wire risk.
- [ ] #7 Lag-o-meter: reachability-based shape. SCOPED + REFRAMED 2026-06-28 (user) →
      same doc. Not "clip to walls" but "run the look-ahead, draw the reachable edge"
      — handles holes from others' crashes for free (a hole = no eHalfEdge → sensor
      reports through it). `drawTriangle`'s recursion already enumerates the
      turn-branching reachable set; swap each branch's raw-`lag` extent for a
      `MaxSpaceAhead`/`gSensor` wall-limited one. MEDIUM. HAZARD: read-only sensor
      probes only — never the mutating Timestep/DoTurn (determinism). Do after #6.
- [ ] #9 "Big slide" bug: new wall begin stamped ahead of the turn point. Experiments
      parked on branch `teleport-fix-attempt`.
- [x] #10 Zone center marker: a vertical line at each zone's center, sticking up out of
      the grid, same colour as the zone, toggled by a console command. SHIPPED
      2026-06-28 (`gWinZone.cpp`): `ZONE_CENTER_LINE` (bool, default off) +
      `ZONE_CENTER_LINE_HEIGHT` (world units, default 10). Drawn outside the cached
      cylinder list via routed `BeginLines`/`Color`/`Vertex`. Builds clean. User
      confirmed working in-game 2026-06-28. Original scope below.
      SMALL, render-only (zero wire impact — `color_`/`pos` already synced).
      Where: `gZone::Render` (`tron/gWinZone.cpp:445`, `#ifndef DEDICATED`, base class →
      covers win/death/base/all zone types). Facts:
      - The `glMultMatrixf(m)` frame maps local (0,0,z) → world (pos.x, pos.y,
        `sg_zoneBottom` + z·`sg_zoneHeight`); local z=1 is the zone top
        (`ZONE_HEIGHT` default 5). So a center line is local (0,0,0)→(0,0,k).
      - Colour is already set by `glColor4f(color_.r,g,b,alpha)` at :474 — the line
        inherits the zone colour for free (give it its own alpha if you don't want it
        faded by the zone's `alpha`).
      - **Draw it OUTSIDE the cached cylinder.** The cylinder is a `static rDisplayList
        zoneList` keyed on `useAlpha`; emit the line as immediate geometry after that
        block (after :530, before `glPopMatrix()` :532) so the toggle works instantly
        with no list-invalidation. ~2 verts/zone/frame — trivial.
      - Toggle: `static tSettingItem<bool>("ZONE_CENTER_LINE", sg_zoneCenterLine)`,
        mirror `sg_laggometerUseOld`. Optional `ZONE_CENTER_LINE_HEIGHT` (world units →
        local z = H/`sg_zoneHeight`) so "sticks up" isn't tied to `ZONE_HEIGHT`.
      - Use `BeginLines()`/`Color`/`Vertex`/`RenderEnd` (routed) — also keeps it off
        R-1's raw-GL list.

## Done (recent, for context)
- [x] title.jpg wrong colours at launch — SDL3_image BGR/RGB swap (`rTexture.cpp`).
- [x] Corner-gap trail bug — invalidate display list instead of snapping (`gWall.cpp`).
- [x] Pause-creep — `nAverager` float-denormal underflow; weight floor.
- [x] DEBUG console overlay for wall reference geometry.
- [x] Multi-line console paste runs each line as its own command.
