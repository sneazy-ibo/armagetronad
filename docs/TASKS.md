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

## Open items (not started)
- [ ] #1 Decide: rip out OpenGL display lists for VBOs? (legacy frozen-geometry path)
- [ ] #2 Fix sound after the SDL2→SDL3 move.
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
