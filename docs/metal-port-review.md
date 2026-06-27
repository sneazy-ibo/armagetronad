# Metal port review — lessons from the RCL branch

Review of a friend's AI-assisted Metal rendering port so we don't repeat its
mistakes when/if we add Metal (or any modern-GL backend) on our branch. Source:
`retrocyclesleague/armagetronad-rcl`, branch `macos-sdl3`, commit `2a9b7d43`
("Add Metal rendering backend and modernize macOS UI/audio", Jamie Legg + Cursor).

**Verdict: good bones, honest internal docs, but the commit oversells a scaffold
that renders ~5% of the game. Do NOT cherry-pick the Metal files.** We salvaged
exactly one piece (`rMatrixState`) and the lessons below.

## What it is

A backend-abstraction scaffold gated behind `ARMAGETRON_GRAPHICS_BACKEND 1`,
Xcode-only, OpenGL still default. Six new files (~1050 lines):

| File | Role |
|------|------|
| `rGraphicsBackend.{h,cpp}` | pick GL vs Metal; context create / present / destroy |
| `rMatrixState.{h,cpp}` | software fixed-function matrix stack (mv/proj/tex) |
| `rMetalGLCompat.{h,cpp}` | intercept ~18 `gl*` state/matrix calls |
| `rMetalBackend.mm` | MTLDevice/queue/pipelines/shaders, per-frame encoder, texture upload |
| `rMetalRender.mm` | `metalRenderer : rRenderer`, batched primitives, 2 shaders |
| `rMetalStub.cpp` | autotools/Linux no-op so the tree still links |

Plus a working font reroute (`rFont.cpp`) and Metal texture upload in `rTexture.cpp`.

## The central compromise (the one that matters)

**Metal never sees the game's geometry.** The compat shim macro-hijacks only
*matrix and state* GL calls (via `#define glMatrixMode sr_metal_glMatrixMode` etc.
in `rGL.h`). It does **not** touch `glBegin / glVertex / glTexCoord / glColor` —
the immediate-mode geometry. Those only reach Metal if the code goes through the
`rRenderer` interface (`metalRenderer`), and almost nothing does:

| Path | Where |
|------|-------|
| Through `rRenderer` → Metal works | `gFloor.cpp`, `gLogo.cpp`, fonts. That's it. |
| Raw `glBegin/glVertex` → Metal blind | ~170 sites: `gCycle`(35), `eDisplay`(34), `gWall`(16), `gFloor`(8), `gZone`(7), `rModel`(5), `gExplosion`, `gHudMap`, `rViewport`, `eCamera`, … |

Under the Metal backend there is **no GL context** (it skips
`SDL_GL_CreateContext`), so those 170 raw calls hit real GL entry points with no
current context → dropped or UB. So Metal currently draws: clear color, fonts,
floor, logo. Not cycles, trails, walls, zones, HUD, or models. The branch's own
`docs/MACOS-MODERN.md` lists this honestly as Phase C/D "next" — but the commit
title does not. **It is not a flip-on-and-play renderer.**

This is also the root cause of the ugliest code: the `#define gl* sr_metal_gl*`
textual hijack across every translation unit, which already produced an
infinite-recursion stack overflow (`sr_metal_glDisable` → `glDisable` →
`sr_metal_glDisable`), now patched with `#undef` guards + `real_gl*` wrappers +
keeping `rGL.h` out of the backend `.mm`. **That whole fragile macro layer exists
only because geometry bypasses the renderer.** Fix the routing and it disappears.

## The lesson for us

The real prerequisite for *any* renderer swap (Metal, or OpenGL core
profile / VBOs for task #1) is **routing all raw immediate-mode GL through the
`rRenderer` abstraction first.** That is a GL-only refactor with value even if we
never adopt Metal — it's the same chokepoint VBOs need, and it kills display
lists' last excuse. Sequence is: migrate geometry → then a backend is a small,
honest change. Do it the other way (backend first) and you get this: a scaffold
that can't draw the game and a macro shim papering over the gap.

**The migration template already exists** in their `rFont.cpp` diff — it's a
clean 1:1 pattern, and it's the same edit repeated ~170 times:

```
glVertex2f(x, y)            -> Vertex(x, y)              // or renderer->Vertex(...)
glTexCoord2f(u, v); glVertex2f(x, y)
                           -> TexVertex(x, y, 0.f, u, v)
glColor4f(r, g, b, a)      -> Color(r, g, b, a)
glBegin(GL_QUADS) … glEnd  -> BeginQuads() … End()
```

## What we took

- **`rMatrixState`** → lifted to `src/render/rMatrixState.{h,cpp}` + a
  framework-free self-check `rMatrixState_test.cpp`. It is pure column-major
  matrix math with no GL/Metal dependency, and we'll need exactly it for any
  core-profile / VBO path (task #1) because GL 3+ deletes the fixed-function
  matrix stack too. **Not wired into the build target yet** — no consumer; it's
  parked salvage. The self-check is the proof it's correct:
  `clang++ -std=c++17 src/render/rMatrixState_test.cpp -o /tmp/mtest && /tmp/mtest`
- **The migration template** above.
- **The lesson**, written into TASKS as a blocked-task chain.

## What we did NOT take, and why

- **The Metal files** (`rMetal*`, `rGraphicsBackend`, the `rGL.h` macro shim) —
  experimental scaffolding for a port that doesn't render the game; the macro
  hijack is a footgun we don't want in our tree.
- **Audio** (`eSound.cpp` SDL3 via `SDL_OpenAudioDeviceStream`) — they arrived at
  the **same approach we already shipped and confirmed** (`4367655d`).
  Independent convergence validates ours; ours additionally restored real audio
  locking and added the Volume control. Nothing to take.
- **Menu mouse + [1]–[9] shortcuts, RCL theme** — product decisions for their
  fork, orthogonal to our SDL3/responsiveness goals.

## Other footguns spotted (if anyone continues a Metal path)

- **Texture wrap** (`GL_REPEAT`) only wired for the font atlas → tiled
  floors/walls would clamp, not tile.
- **No `glPolygonOffset` / backface cull / smooth shade** in the pipeline —
  walls and trails rely on polygon offset to avoid z-fighting; they'd shimmer.
- **`glBlendFunc` / `glAlphaFunc` tracked but ignored** — blend is hardcoded
  src-alpha in the pipeline; alpha-test isn't in the shader.
- **Fresh `MTLBuffer` per batch** (`newBufferWithBytes` every draw) — no ring
  buffer / reuse.
- **No display-list story.** Our branch knows display lists are load-bearing
  frozen geometry (`gWall` corner-gap fix). The Metal path can't record them;
  this couples a backend swap to task #1 (rip out display lists).
- **Runtime backend switch is feasible but pointless until the above.** See the
  TASKS entry: it can reuse `sr_ReinitDisplay` (the same teardown/recreate the
  resolution menu uses), but switching to Metal today just shows a near-empty
  scene.

## Footguns that bit *their* branch (worth pre-empting on ours)

These are from their `docs/DEVLOG.md` and are SDL3/macOS issues we may hit too:

- macOS startup crash from a wrong `DATA_DIR` (DerivedData build couldn't find
  `language/languages.txt`) → they added a startup check + null-`english` locale
  guard. Worth a similar guard.
- Server-browser segfault: `gServerMenuItem` destructor removed/re-added the
  filter menu item while destroying a row → dangling/null slots; fixed by letting
  `~uMenuItem` handle removal + a null guard in `Update()`. (Our browser work is
  in this area — keep it in mind.)
