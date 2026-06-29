# Sharp edges & lessons learned

Non-obvious traps in this codebase, with the lesson attached. Add to this whenever
something surprises you — it's cheaper than re-discovering it in a fresh session.

## Networking

- **Single-threaded by design.** Global singletons (`sn_GetNetState()`,
  `sn_Connections`, `peers[0]`) and `tRecorder` record/playback determinism rely on
  it. Don't add threads to the net path. Make slow things *cooperative* (step
  machines driven by the frame/menu pump) instead. → this is why the master fetch is
  a `Begin/Step/End` pump, not a worker thread.

- **`peers[0]` is shared between all of connection 0's users.** Both game-server
  queries (`nServerInfo::QueryServer` → `sn_Bend(GetAddress())`) and the master
  connection point `peers[0]` at their target. They **cannot run concurrently** —
  the master's acks would go to a game server and its send window stalls (~9-11
  servers then hang). Lesson: sequence them as phases (prefetch → master fetch →
  query), never overlap. (Killed the original "query-during-fetch" idea.)

- **The master has no game netObjects**, so the two post-login `sn_Sync(40)` calls
  in `sn_Connect` are dead round-trips (~1.8s) on the master path. We skip them with
  `waitSync=false`. Lesson worth generalising: *measure each sub-phase before
  refactoring* — the fix was 3 lines, not a cooperative-connect rewrite.

- **Wire format is frozen** for compatibility. Change *scheduling*, never message
  contents/layout, unless explicitly asked.

- **The `.srv` server cache stores no ping and no player counts** (`nServerInfo::Save`
  /`Load`), and `Load` resets `advancedInfoSetEver=false`. So cached servers render
  as "polling" until queried fresh. That's why "instant pings" was solved by
  *prefetching* the first page, not by displaying cached pings.

- **`nServerInfo::Polling()` keys off `advancedInfoSetEver`**, which survives a
  re-`StartQueryAll`. So re-querying an already-answered server keeps showing its
  ping (no flicker back to "polling"). Handy property; relied on by B-4.

## Rendering / SDL3

- SDL2 → SDL3 migration is live. Byte-order bit you already: SDL3_image hands back
  BGR; fixed by surface conversion in `rSurface::Create` (`rTexture.cpp`).
- Legacy **OpenGL display lists** are still used — frozen geometry snapshots. A trail
  that won't update visually is often a stale display list; invalidate it rather than
  mutating geometry to compensate (the corner-gap fix in `gWall.cpp`).
- **`gluBuild2DMipmaps` is deprecated/removed on modern macOS.** Texture upload in
  `rTexture.cpp` (`rISurfaceTexture::Upload`) still calls it. The modern replacement is
  `glTexImage2D` + `glGenerateMipmap` (load `glGenerateMipmap` via
  `SDL_GL_GetProcAddress` since it's not in the legacy GL header). Relevant to the
  VBO/modern-GL task (#1). When you do migrate it, **keep the existing format
  selection** (`sr_texturesTruecolor` → `GL_RGBA8`/`GL_RGB8`/`GL_RGBA4`/`GL_RGB5`) —
  an early attempt flattened everything to `GL_RGBA` and lost that distinction.
- **SDL3_image needs no `IMG_Init`.** `IMG_Load` works directly; `IMG_Init`/`IMG_Quit`
  are no-ops/deprecated in SDL3_image. Don't reintroduce them.
- `src/macosx/Info.plist`: bundle id is better as `$(PRODUCT_BUNDLE_IDENTIFIER)` (build
  setting) than a hardcoded string; `NSHighResolutionCapable` should be set. Minor.
- Background on the above: salvaged from an old SDL3 debug stash, archived at
  `docs/archive/2026-06-16-sdl3-wip-stash.patch` (the implementation there was
  throwaway cerr-laden scaffolding; only these facts are worth keeping).

## Audio (SDL3)

- **The mixer is the asset, not the SDL glue.** `eSound.cpp`'s software mixer
  (`fill_audio` → camera/global-player `Mix`) survived the SDL2→SDL3 move untouched;
  only the device open/close/lock/pause was stubbed. Port the glue, leave the mixer.
- **SDL3 get-callback hands you a stream, not a silenced buffer.** The mix is
  *additive* (reads `dest` before writing), so the callback **must memset its working
  buffer to silence** before mixing, then `SDL_PutAudioStreamData`. SDL2's device
  buffer used to arrive silent, hiding this. Skipping the memset = garbage/noise.
- **`SDL_AudioSpec` lost `samples` in SDL3** — that's why the old "Buffer Length" menu
  knob became dead and was replaced by a Volume slider. Master volume = native
  `SDL_SetAudioStreamGain(stream, 0..1)`; don't hand-scale the buffer.
- **Audio locking is real again.** `eSoundLocker`/`se_SoundLock` wrap
  `SDL_Lock/UnlockAudioStream` (only at depth 0). It guards the global player list
  from the SDL audio thread; the interim no-op stub was a latent data race.
- **Music (`fire.xm` via SDL_mixer) is WIN32-only** (`HAVE_LIBSDL_MIXER`). The mac
  build never had it; don't reintroduce SDL3_mixer for the mac path.

## Cycle walls / trails (learned shipping the trail-end fade, #6)

- **Wall rendering has THREE paths; a per-segment effect must go before the split.**
  `gNetPlayerWall::RenderList` dispatches each segment to `if(sg_simpleTrail){…}` /
  `else if(te+gBEG_LEN<=time){…}` (normal steady) / `else{…}` (growing tip). Putting
  an effect in just the steady branch silently does nothing for anyone with
  `SIMPLE_TRAIL` on. Apply per-segment effects **after the "cut the end of the wall"
  block but before the `if(sg_simpleTrail)` dispatch**, then `continue`.
- **Wall display-list cache freezes per-frame effects.** Walls split into two lists:
  `wallList_` (fresh, re-rendered every frame) and `wallsWithDisplayList_` (cached).
  `displayList_.Call()` replays the cached GL geometry and **returns early — the
  cached walls' `RenderList` never runs**, so a per-frame effect baked into them
  freezes. Cache only rebuilds when walls expire at the tail or new walls pile up. To
  animate cached walls, force `displayList_.Clear` each frame
  (`RenderAllWithDisplayList`). NB: display lists default **off**
  (`sr_useDisplayLists = rDisplayList_Off`, rScreen.cpp), so this only bites with them on.
- **The wall quad body ignored vertex alpha** — `RenderNormal` hardcoded
  `glColor4f(r,g,b,1)` on the 4 quad verts; only the upper-edge **line**
  (`upperlinecolor`) used the passed `a`. To fade the wall *body* you must thread `a`
  into the quad. (Death-fade keeps a separate `lineAlpha` so dying walls are unchanged.)
- **The tail "cut" is `GetDistance() - ThisWallsLength()`** (line in `RenderList`);
  older geometry is dropped. With `PREDICT_OBJECTS` **off** (the default) the drawn
  tail still sits at that cut — predicted and drawn coincide; a debug marker confirmed
  it. The lag-uncertain band is the oldest `speed*lag` of the drawn trail, from the cut
  inward.

## Lag metrics — two different things, don't conflate

- **`eNetGameObject::Lag()` returns `laggometerSmooth`** = an EWMA of raw ping
  (`se_GetPing`) / sync-delay (`se_GameTime()-lastSyncMessage_.time`). This is what the
  **lag-o-meter AND the trail-end fade** both use — the honest "where might this be,
  visually" metric.
- **`LagThreshold()` → `eLag::Threshold()`** is the *separate* lag-compensation /
  "ping charity" credit budget. It is **server-side and returns 0 on clients**
  (`sn_GetNetState()!=nSERVER → 0`). It is NOT the laggometer and is NOT used by the
  visual lag indicators. So `Lag()` does **not** subtract lag credit; a charity-aware
  visual would need the server to send the credit down (wire change). User confirmed
  raw-lag is the correct metric for these indicators (and keeps fade ⟂ lag-o-meter
  consistent). Own cycle's `Lag()` is ~0 on a client — the laggometer is only
  populated for non-owned cycles (`gCycle.cpp` `Owner()!=sn_myNetID` block), so these
  effects show on **enemy** trails, not your own.

## Renderer abstraction (learned from RCL's Metal port — docs/metal-port-review.md)

- **Most gameplay rendering bypasses the `rRenderer` abstraction.** Only
  `gFloor.cpp` and `gLogo.cpp` go through `renderer->`; ~170 sites across
  `gCycle`, `eDisplay`, `gWall`, `gZone`, `gFloor`, `rModel`, `gExplosion`,
  `gHudMap`, `rViewport`, `eCamera` call raw immediate-mode `glBegin/glVertex/
  glTexCoord/glColor`. **Any** renderer swap (Metal, GL core profile, VBOs for
  task #1) is blocked on routing those through `rRenderer` first (TASKS R-1).
  A backend added before that refactor can only draw clear+fonts+floor+logo.
- **Don't intercept GL with `#define gl* sr_metal_gl*` macros** (RCL's approach).
  It's a textual hijack across every TU, it caused an infinite-recursion stack
  overflow there, and it only exists to paper over the bypass above. Route
  through the `rRenderer` vtable instead — it already exists.
- **A backend switch can reuse `sr_ReinitDisplay`** (`sr_ExitDisplay` +
  `sr_InitDisplay`, the resolution-menu path, `gMenus.cpp:283`): it rebuilds the
  window + context and regenerates textures/display lists. So an in-game
  "Renderer" menu item is low-effort — but only useful once an alternate backend
  actually renders the game.
- **`rMatrixState` is salvaged** (`src/render/rMatrixState.{h,cpp}`, self-check in
  `rMatrixState_test.cpp`) for when we need a software matrix stack (core profile
  drops the fixed-function one). Not in the build target yet — no consumer.

## Timing

- `tSysTimeFloat()` is frame-stamped (same value within a frame); `tRealSysTimeFloat()`
  re-reads the clock per call. Use the latter for measuring sub-frame durations.
- `nAverager` (EWMA) can hit float-denormal underflow with zero `Add()`s → runaway
  values (showed up as pause-creep). Floor the weight.

## Build / tooling

- Root `make` is the **autotools/Linux** build; macOS builds via **Xcode**. Use
  `./build.sh` (terse) or `./build.sh -v`. Don't expect `make` to build the mac app.
- **clangd "unused include" warnings on shared `.cpp` files are not safe to auto-fix.**
  Includes may be needed by the Win32/Linux build targets even if the macOS TU
  compiles without them. These false positives are now **suppressed via `.clangd`**
  (`Diagnostics.UnusedIncludes/MissingIncludes: None`) so they don't create noise;
  real clang-tidy bug checks stay on. If you ever do want an include audit, flip that
  off temporarily and verify across all targets. Don't blind-remove.
- A clangd "misleading indentation" warning often points at pre-existing tab/space
  mixing in old code — fix it if you're editing that block, otherwise record it.

## clang-tidy

- Installed via `brew install llvm` → `/opt/homebrew/opt/llvm/bin/clang-tidy`.
  Run it with **`./tidy.sh src/path/file.cpp`** (add `--checks=...` as needed).
- It needs a compile DB. `./tidy.sh --refresh` regenerates `.tidy/compile_commands.json`
  from the latest Xcode build log via `xcode-build-server`. **So build first**, then
  refresh, then tidy. `.tidy/` is gitignored (machine-specific paths + the DerivedData
  hash).
- The DB lives in `.tidy/`, **not** the repo root, on purpose: a root
  `compile_commands.json` makes the editor's clangd switch off the build-server flags
  and then mis-resolve generated headers (false "nTrueVersion.h not found", spurious
  `-Wshorten-64-to-32`). Keep it out of clangd's discovery path.
- `tidy.sh --refresh` post-processes the DB because `xcode-build-server` output isn't
  directly usable by brew clang-tidy: it emits several entries per file (universal
  build, multiple SDKs), packs **two** clang invocations into one `command` (an
  SDK-stat-cache build `;` the real compile → "expected exactly one compiler job"),
  and carries Apple-clang-only flags like `-ivfsstatcache` that brew clang rejects.
  The script keeps one arm64 real-compile entry per source file with those flags stripped.
- No `.clang-tidy` check-set is committed yet — pick the check policy deliberately
  (good first step of the std-library migration, when the custom-type false positives
  go away). Until then pass `--checks=` explicitly.
