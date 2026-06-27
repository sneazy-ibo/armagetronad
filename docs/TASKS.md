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

### std-library migration (planned, not started)
Goal: replace custom `tString` → `std::string` and `tJUST_CONTROLLED_PTR`/`tSafePTR`
(src/tools/tSafePTR.h) → `std::shared_ptr`/`std::unique_ptr`, so we can turn on
stricter safeguards (compiler warnings-as-errors, sanitizers, clang-tidy) that
previously fired false positives on the custom types.
- [ ] Scope it: inventory uses, decide migration order (leaf modules first), pick a
      compatibility shim strategy so it can land incrementally and keep building.
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
      `rRenderer`. ~170 sites in `gCycle`(35), `eDisplay`(34), `gWall`(16),
      `gFloor`(8), `gZone`(7), `rModel`(5), `gExplosion`, `gHudMap`, `rViewport`,
      `eCamera`. Template: `glVertex2f→Vertex`, `glTexCoord2f;glVertex2f→TexVertex`,
      `glColor*→Color`, `glBegin/glEnd→Begin*/End`. Valuable even staying on GL.
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
- [ ] #6 Lag-o-meter: 0.5-opacity "drive-through" zone at the trail end.
- [ ] #7 Lag-o-meter: dynamic shape based on surrounding walls.
- [ ] #9 "Big slide" bug: new wall begin stamped ahead of the turn point. Experiments
      parked on branch `teleport-fix-attempt`.

## Done (recent, for context)
- [x] title.jpg wrong colours at launch — SDL3_image BGR/RGB swap (`rTexture.cpp`).
- [x] Corner-gap trail bug — invalidate display list instead of snapping (`gWall.cpp`).
- [x] Pause-creep — `nAverager` float-denormal underflow; weight floor.
- [x] DEBUG console overlay for wall reference geometry.
- [x] Multi-line console paste runs each line as its own command.
