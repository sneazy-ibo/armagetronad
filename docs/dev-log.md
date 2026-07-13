# Dev log

Newest first. One entry per work session: what changed, why, what to do next. Keep
entries short — detail lives in commits, `TASKS.md`, and `sharp-edges.md`. The point
is that a fresh session can read the top entry and know exactly where things stand.

---

## 2026-07-13 — full-release (Dedicated target) build fix + warning cleanup

**Fixed the release build.** "Build for Any Mac" compiles the **Dedicated** target too
(the "My Mac" client build never touches it). It failed: `eSound.cpp` `fill_audio`'s
`SDL_AudioStream*` signature sat *outside* the `#ifndef DEDICATED` guard, and the
dedicated SDL shim (`rSDL.h`) has no `SDL_AudioStream` typedef. Fix: guard the whole
function (dead in a headless server anyway).

**Warning cleanup (292 → 234, −58), no bulk risk taken:**
- Real SDL3 bug surfaced by `-Wtautological-constant-compare`: SDL3 `SDL_Init` returns
  `bool`, so `SDL_Init(...) < 0` (gArmagetron) and `>= 0` (eSound) were dead checks —
  init failure never detected / always "ok". Fixed to `!SDL_Init(...)` / bare. See
  sharp-edges "Audio (SDL3)".
- 48 GL `-Wdeprecated-declarations` → 2 (leftover are AppKit `sizeToFit`, not GL):
  moved `GL_SILENCE_DEPRECATION` from `rGL.h` to a **global** build define (covered
  `opengl.cpp` too; removed the 104 `-Wmacro-redefined` the header-local define caused).
- `SDLMain`: added `<NSApplicationDelegate>` conformance (kills incompatible-pointer),
  fixed the non-existent `stringByReplacingRange:with:` selector →
  `stringByReplacingCharactersInRange:withString:` (menu-rename was silently no-op).
- Removed unused `uses_sdl_mixer`.

**Deferred (not touched, on purpose):** 210 `-Wshorten-64-to-32` (size_t→int truncation
sweep — its own task, wire/behavior risk), 8 unused static functions in `tDirectories`/
`rScreen` (need per-platform check before deleting), 8 `-Wunreachable-code` in gameplay
files, `testgl.cpp` SDL_Init `<0` (not in app build).

**Also:** opted out of macOS window restoration explicitly in `SDLMain` (kills the
`restoreWindowWithIdentifier … className=(null)` launch log). The `linkd.autoShortcut` /
App-Intents / Process-Instance-Registry console spam is OS daemon noise from a dev-signed
unsandboxed build — not our code, not fixable, ignored.

**Rewired `armagetronad://host:port` direct-connect (launch-time).** Registered the
`armagetronad` scheme in `src/macosx/Info.plist` (`CFBundleURLTypes`) and a GetURL Apple
Event handler in `SDLMain` (`applicationWillFinishLaunching`, so the launch URL is captured
*before* `didFinishLaunching` hands off to the blocking `SDL_main`). Handler → C shim
`st_QueueDirectConnectC` → `st_QueueDirectConnect` (gGame.cpp) stashes host/port; consumed
by `st_ConsumeDirectConnect()` right before `MainMenu()` in gArmagetron.cpp, building an
`nServerInfoRedirect` + `ConnectToServer` (same path as favorites). Both targets build,
no new warnings. **Runtime-verified 2026-07-13** (user): clicking an `armagetronad://`
link launches the game and joins the server. **Launch-time only** — clicking a link when
the app is *already running* queues the
target but the in-menu loop doesn't poll it yet (needs a menu-pump hook → TASKS). Test:
build the .app, `lsregister -f "…/Armagetron Advanced.app"`, then `open
armagetronad://157.245.224.31:4536`.

---

## 2026-07-10 — #9 teleport bug: diagnosed end-to-end (debug branch parked)

**Diagnosed** the grid-teleport/"big slide" bug via an instrumented session. Root:
`eGameObject::Move`'s face-walk can't converge across the **thin wall-bounded triangles**
that accumulate in dense-turn regions (times out even at `GAMEOBJECT_MOVE_TIMEOUT 3000`),
so it strands the cycle on a wandered edge — or `FindCurrentFace` relocates it — → teleport
(measured up to 166u for a 32u move), backward → death. Thin triangles come from the
single-bounding-triangle fan + `DrawLine` cutting un-flippable crossings + `Simplify`
refusing wall-adjacent points. Full writeup + evidence at the **top of
`docs/grid-mesh-geometry.md`**.

**Corrected several earlier hypotheses (all with evidence):** NOT a leak (mesh is
Euler-minimal `faces=2·points−5` every sample; bursts to 12k faces then reclaims), NOT
precision, NOT `CorrectArea` (marker never fired), NOT `DrawLine` give-up (log empty), NOT
the distance-space wall-stamping (that's the remote `SyncFromExtrapolator` path).
**Delaunay legalization rejected** with the new context (can't flip constrained edges;
removes no points). Verified the grid is **local/not-on-the-wire** and deaths are
**server-authoritative** → every candidate fix is wire-safe.

**Parked (branch `grid-teleport-debug`):** the `GRID_DEBUG_*` mesh overlay, teleport /
DrawLine / grid-stats file probes, `GAMEOBJECT_MOVE_KEEP_DEST` + `GRID_SIMPLIFY_RATE`
configs, and a real mitigation — **adaptive reclamation** (`SimplifyAll += edges/32`, stops
the monotonic accumulation). **Kept on `macos0.2.9.3.0`:** this doc set + the `tidy.sh` DB
fix (index-store / `-gmodules` / `@response` stripping so brew clang-tidy runs again).

**Next (own focused session):** #1 collinear-wall-point merge in `Simplify` (collapse
straight walls / corridors — the user's Q1/Q2 — preserving the wall danger interval),
and/or #3 harden the walk to end benignly at the destination.

**Process note:** file logs repeatedly caught wrong conclusions that on-screen markers had
led me to (I twice read "no marker seen" as "event didn't happen"). Write debug to files.
Also over-anchored on the doc's headline hypothesis instead of its symptom→area pointer.

---

## 2026-07-07 — Rust-rewrite exploration + full engine read-through (docs only)

**Shipped:** `docs/rust-rewrite-notes.md` — complete architecture notes for a
hypothetical Rust rewrite, based on an actual read of `src/` (~117k lines). Contains
the agreed crate/thread/determinism architecture, the full subsystem inventory, the
corrected ECS sketch, and 11 "architecture traps". No code changed.

**Headline discoveries (details in the doc):** explosions blow attributed holes in
walls (sim, not VFX); the wire protocol is destination-based (already quasi-
event-sourced); walls are piecewise time-stamped growing/shrinking objects; 127
physics settings in the cycle alone (per-wall-type accel = core gameplay);
`CYCLE_PING_RUBBER` makes physics read network state; chatbot autopilot + uncanny-
timing cheat detection exist in the sim; eTimer is a real clock-sync subsystem.

**Also this session (memory files, not repo):** fork build states (RCL = live SDL3+
Metal Xcode; others = autotools; wrapper-repo idea dropped), camera-feel investigation
(0.2.9↔0.4 camera pipeline byte-identical), netcode model corrections (ping charity
is symmetric-by-design; turns are frame-quantized). See memory index.

**Next (if rewrite exploration continues):** milestone 1 = `tron-sim` + terminal
client over fake-latency localhost; the notes doc has the scope list.

---

## 2026-06-29 — Trail-end lag-uncertainty fade (#6)

**Shipped (`gWall.cpp`, `gCycle.cpp`, `language/english_base.txt`):** a flat
0.5-alpha band of length `speed*lag` at the **disappearing tail end** of cycle
trails, marking the part whose drawn length is uncertain under network lag. Configs
`TRAIL_END_FADE` (bool, off) + `TRAIL_END_FADE_SCALE` (REAL, 1). Reverts to solid on
death (`Alive()` gate). Uses `cycle_->Lag()` (= `laggometerSmooth`, same metric as the
lag-o-meter — user-confirmed correct).

**The task wording lied:** "#6 Lag-o-meter drive-through zone" was actually about the
**wall/trail tail**, not the lag-o-meter spiral. First mis-scoped in
`lag-o-meter-scope.md` (now superseded). Lesson: confirm the subsystem before scoping
a vaguely-worded task.

**Hard-won implementation notes (all now in sharp-edges):**
- The fade lives in `gNetPlayerWall::RenderList`, applied **before the trail-style
  dispatch** (`if(sg_simpleTrail) … else …`). First attempt put it in one branch and
  it silently did nothing for anyone with `SIMPLE_TRAIL` on. Wall rendering has 3
  paths (normal / simple / growing-tip); a per-segment effect must precede the split.
- The band is the oldest `speed*lag` of the drawn trail; segment **split at the band
  edge** (`continue` after) gives an exact length (no segment-snapping) and a hard
  0.5, not a gradient (user explicitly did not want a gradient).
- The wall **quad body was hardcoded `glColor4f(r,g,b,1)`** — alpha only reached the
  upper line. Threaded `a` into the quad (kept death-fade on a separate `lineAlpha` so
  dying walls render as before).
- **Display-list cache footgun:** cached walls (`wallsWithDisplayList_`) replay frozen
  geometry via `displayList_.Call()` and their `RenderList` never runs, so a per-frame
  effect freezes solid. Added a per-frame `displayList_.Clear` in
  `RenderAllWithDisplayList` while the fade is active. Moot for the user (display lists
  default **off**, `sr_useDisplayLists=rDisplayList_Off`) but correct when on.

**Debugging that found it:** temporary yellow/green vertical markers at the band edges
+ a whole-wall red `fade` tint isolated the bug to the `SIMPLE_TRAIL` path. All debug
scaffolding (and a `g_trailDrawnTailPos` helper from a since-reverted anchor idea)
removed before finishing.

**Next:** unchanged backlog. #7 (reachability lag-o-meter shape) still open; R-1 etc.

---

## 2026-06-28 — Scoped backlog tasks; shipped zone center marker (#10)

**Scoped (new deep-dive docs):** `std-library-migration-scope.md` (two independent
halves; `tString` reparent onto `std::string` is the tractable win, intrusive
smart-ptrs probably skippable — measure which type trips the safeguards first),
`r1-geometry-routing-scope.md` (corrected the review's count: Begin/End already
routed, only ~137 per-vertex calls leak; zero user-visible value alone, do it only
when a backend/VBO goal is committed), `lag-o-meter-scope.md` (#6 fill the trail-end
sub-region — small; #7 reframed by user as "run the look-ahead, draw the reachable
edge" — reuses `MaxSpaceAhead`/`gSensor`, holes fall out for free, read-only probes
only).

**Shipped (#10, `gWinZone.cpp`):** zone center marker — a vertical line at each
zone's centre, same colour as the zone, sticking up out of the grid. New configs
`ZONE_CENTER_LINE` (bool, default off) + `ZONE_CENTER_LINE_HEIGHT` (world units,
default 10). Drawn **outside** the cached cylinder display list (after the fill
block, before `glPopMatrix`) so the toggle applies instantly with no list
invalidation; routed `BeginLines`/`Color`/`Vertex`. The active `glMultMatrixf` frame
maps local (0,0,z)→world height `sg_zoneBottom + z*sg_zoneHeight`, so the line at
x=y=0 rides the zone axis; top local z = `height / sg_zoneHeight`. Render-only, base
`gZone::Render` so it covers all zone types, zero wire impact. **Builds clean; user
confirmed working in-game** (set `ZONE_CENTER_LINE 1`).

**Next:** confirm #10 visually; then #6 (lag-o-meter fill) or scope B-5/B-6.

---

## 2026-06-27 — Reviewed RCL's Metal port; salvaged rMatrixState

Reviewed a friend's AI-assisted Metal rendering port (`retrocyclesleague/
armagetronad-rcl`, commit `2a9b7d43`). Full writeup: `docs/metal-port-review.md`.

**Headline finding:** the port is a well-structured *scaffold* that renders ~5%
of the game, not a working backend. Its GL→Metal shim only intercepts matrix/state
calls; the ~170 raw `glBegin/glVertex` immediate-mode sites (gCycle, eDisplay,
gWall, gZone, gFloor, rModel…) bypass the `rRenderer` abstraction entirely, so
Metal (which has no GL context) draws only clear+fonts+floor+logo. The fragile
`#define gl* sr_metal_gl*` macro hijack (which crashed them with infinite
recursion) exists *only* because geometry bypasses `rRenderer`.

**Lesson, now in TASKS as a dependency chain (R-0…R-3):** the real prerequisite
for Metal *or* the VBO/core-profile task (#1) is **R-1 — route raw GL through
`rRenderer` first.** That's a GL-only refactor worth doing regardless of backend.

**Salvaged:** `rMatrixState` (their software matrix stack) → `src/render/
rMatrixState.{h,cpp}` + a framework-free self-check `rMatrixState_test.cpp`
(passes: `clang++ -std=c++17 src/render/rMatrixState_test.cpp -o /tmp/mtest`).
Pure column-major matrix math, no GL dependency, reusable for any core-profile
path. **Not wired into the Xcode target** — no consumer yet, parked salvage.

**Also noted (didn't take):** their SDL3 audio port is the *same* approach we
already shipped (#2) — independent confirmation ours is right. Menu/RCL-theme
work is fork-specific. Two SDL3/macOS footguns from their devlog logged in
metal-port-review.md (DATA_DIR startup crash; server-browser destructor segfault).

**Next:** unchanged backlog — or start R-1 (the keystone refactor), which also
unblocks #1.

---

## 2026-06-27 — SDL3 sound port (#2)

**Changed (`src/engine/eSound.cpp`, `language/english_base.txt`):** the software
mixer was fully intact — only the SDL device glue was stubbed. Ported the five
stubs to SDL3:
- `se_SoundInit`: `SDL_OpenAudioDeviceStream(DEFAULT_PLAYBACK, &spec, fill_audio, NULL)`
  + `SDL_ResumeAudioStreamDevice`. SDL3 streams auto-convert to device format, so the
  old 16-bit-stereo emulation fallback isn't needed (never existed in the stub).
- `fill_audio`: new SDL3 callback sig `(udata, SDL_AudioStream*, additional, total)`.
  **Mixes into a zeroed static buffer then `SDL_PutAudioStreamData`** — the mix is
  additive (reads dest first) and SDL3 doesn't hand you silence, so the memset is
  load-bearing (SDL2 device buffer used to arrive silent).
- pause→`SDL_Pause/ResumeAudioStreamDevice`; close→`SDL_DestroyAudioStream`;
  lock→`SDL_Lock/UnlockAudioStream` (restored real locking — the stub `locks++` was a
  latent race on the global player list).

**Also:** dropped the "Buffer Length" menu knob (SDL3 `SDL_AudioSpec` has no `samples`
field; the knob did nothing) and replaced it with a master **Volume** slider (0–100,
step 10, `SOUND_VOLUME` config) applied via native `SDL_SetAudioStreamGain`.

**All Sound-menu items now apply live** (no longer only on menu exit): tiny
`uMenuItem` subclasses override `LeftRight` — `eSoundQualityMenuItem` re-inits the
device on quality change, `eSoundVolumeMenuItem` sets the gain per step. Sources was
already live (the mix callback reads `sound_sources` each call). `se_SoundMenu` is now
just `Sound_menu.Enter()`.

**Builds clean.** clang-tidy on the file is all pre-existing noise except one int→float
narrowing in my gain calc, now an explicit `static_cast`. **User confirmed** audio,
volume, and live quality switching all work. Shipped as `4367655d`; #2 ticked off.

**Next:** sound is closed. Pick from the backlog — server-browser B-5 (hover prefetch)
or B-6 (master retry through the step machine), or scope the std-library migration.
The one deferred sound item if it ever bites: a WAV in an unsupported format still
throws (as before) — wire `SDL_ConvertAudioSamples` then.

---

## 2026-06-27 — server browser load time (Option B / B-4)

**Shipped (committed on `macos0.2.9.3.0`):**
- `3c2d1341` Skip post-login `sn_Sync` on the master connect (`waitSync=false`).
  Phase timing proved the ~2s connect was the two syncs, not the login → connect
  2.05s → 0.34s. Game joins unchanged.
- `eb8984ab` B-4 core: open the browser on the cached list; stream the master fetch
  through the menu pump (`gBrowserMenuItem::RenderBackground`) instead of blocking;
  `GetFromMasterStep` made non-blocking from the pump; join-mid-stream ends the fetch
  first; `GetFromMasterEnd(pruneStale=false)` on the live path.
- `a385177e` B-4 prefetch: phase machine PREFETCH → MASTERFETCH → QUERY. Ping the
  cached first page (score order) for `BROWSER_PREFETCH_SECONDS` (1.5) before the
  master refresh, so visible servers show real pings within ~1 RTT.
- `dc7189f3` doc update.

**Result:** master browse went from a ~6-8s blank wait to an instant, navigable list
with real first-page pings in ~1s. User confirmed "works perfect". Notably we
*avoided* the cooperative-`sn_Connect` refactor the doc first scoped — the syncs-skip
made the connect cheap enough.

**Also this session:** added `CLAUDE.md`, `build.sh`, and this docs knowledge base
(`TASKS.md`, `sharp-edges.md`, `dev-log.md`). Started planning the std-library
migration (tString/shared-ptr → stdlib) to unlock stricter safeguards.

**Tooling:** `.clangd` suppresses include-cleaner false positives on shared `.cpp`s
(keeps real checks). Installed clang-tidy (`brew llvm`) and added `./tidy.sh` +
`./build.sh`. Getting clang-tidy working took real effort (xcode-build-server packs
two clang jobs per command, multiple universal/SDK entries, Apple-only `-ivfsstatcache`)
— `tidy.sh --refresh` normalises the DB; it lives in gitignored `.tidy/` so it doesn't
hijack the editor's clangd. Fixed the one real diagnostic found (`gServerBrowser.cpp`
tab/space `else if`). Principle reinforced in CLAUDE.md: **fix blockers, don't route
around them** (install the missing tool, fix the diagnostic).

**Next:** B-5 (hover prefetch) or B-6 (master retry in the step machine); or scope the
std-library migration. Network thread is at a clean, committed stop.

---

<!-- Template for new entries:
## YYYY-MM-DD — <short title>
**Shipped:** <commits / what landed>
**Why / findings:** <the non-obvious bits>
**Next:** <where to resume>
-->
