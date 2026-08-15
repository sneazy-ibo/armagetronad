# R-1 — route raw immediate-mode GL through `rRenderer` (scope)

Keystone refactor that unblocks Metal (R-2) **and** VBOs / display-list removal
(#1). This scope corrects the count in `metal-port-review.md` after reading the
current branch.

## What's actually true (measured, not the doc's framing)

The review doc says "~170 raw `glBegin/glVertex` sites bypass `rRenderer`." Two
corrections after measuring `src/`:

1. **The abstraction already exists and is broad.** `rRender.h` defines
   `class rRenderer` (Vertex/TexCoord/Color/Begin*/End, matrix + flag ops) *and*
   free-function wrappers (`Vertex()`, `Color()`, `BeginQuads()`, `RenderEnd()`, …)
   that forward to `renderer->`. It even `#define`s `glBegin`/`glEnd`/`glMatrixMode`
   to `#error` — so **`Begin*/End` and matrix-mode are already routed**; raw
   `glBegin` survives only in 3 stray debug blocks + the render backend itself.

2. **What leaks is the *inside* of the blocks.** The live pattern is already:
   ```
   BeginTriangleFan();              // routed → renderer->BeginTriangleFan()
       glTexCoord2f(x, y);          // RAW — hits GL directly, Metal-blind
       glVertex3f  (x, y, h);       // RAW
   RenderEnd();                     // routed → renderer->End()
   ```
   So R-1 is **converting the per-vertex/color/texcoord calls inside
   already-abstracted Begin/End blocks** — `glVertex*→Vertex`, `glTexCoord*→TexCoord`,
   `glColor*→Color`. Mechanical 1:1. Not 170 `glBegin` rewrites.

## The real target (game code only: `tron/ engine/ ui/`)

| Bucket | Count | Action |
|--------|-------|--------|
| Raw geometry calls (`glVertex2f/3f`, `glTexCoord2f`, `glColor3f/4f`, `glNormal3f`) | **137** | → `Vertex` / `TexCoord` / `Color` free fns |
| Stray `glBegin/glEnd` blocks (gCycle:4278, eCamera:1454, eDisplay:436 — all debug overlays) | 3 | → `BeginLines()/BeginTriangles()` + `RenderEnd()` |
| Raw matrix/state (`glPushMatrix`/`glPopMatrix`/`glTranslatef`/`glScalef`/`glMultMatrixf`/`glLoadIdentity`) | ~91 | → `PushMatrix`/`PopMatrix`/`TranslateMatrix`/`ScaleMatrix`/`MultMatrix`/`IdentityMatrix` (R-1b, separable) |

Per-file geometry leak (heaviest first): `gCycle` 49, `eDisplay` 39, `gWall` 26,
`rViewport` 7, `eCamera` 5, `gFloor`/`gExplosion`/`rModel` 4 each, plus a few in
`ePath`/`gMenus`/`gSparks`/`gWinZone`/`uMenu`.

**Out of scope (legit raw GL — the backend implementation):** `render/rGLRender.cpp`,
`render/rModel.cpp` internals, `render/rFont.cpp`, `render/rDisplayList.cpp`,
`render/rConsoleGraph.cpp`, `render/testgl.cpp`, `thirdparty/particles`. These
*are* what `Vertex()` ultimately calls; they stay raw.

## Split into two independent passes

- **R-1a — geometry** (137 calls + 3 stray `glBegin`). The keystone. Pure
  mechanical conversion inside existing blocks.
- **R-1b — matrix/state** (~91 calls). Also mechanical, but verify the abstraction's
  matrix semantics match (this is where the salvaged `rMatrixState` becomes the
  core-profile backing). Can land after R-1a; not needed to prove the geometry path.

Keep `TexCoord();Vertex();` as separate 1:1 conversions (lowest-risk). The
`TexVertex(x,y,z,u,v)` one-call helper is an *optional* later consolidation, not
part of the mechanical pass.

## Why this is safe to do incrementally

Under the **current GL backend**, `Vertex()`/`Color()` just call `glVertex`/`glColor` —
so every conversion renders **byte-identical**. Expected visual diff per file: zero.
That's the checkpoint: convert one file → build → run → confirm no change. Each file
is independent; no big-bang.

## Risks / things to verify (small)

- **`Color()` outside a Begin block** sets current colour as state (e.g.
  `eDisplay:295` before a block). Confirm `rGLRender::Color` issues `glColor`
  immediately (it should) so state-setting conversions are equivalent.
- **Display-list recording is unaffected on GL.** `gWall` records geometry into
  `rDisplayList`; `Vertex()`→`glVertex` is captured by `glNewList` exactly as the
  raw call was. (Display lists vs Metal/VBO is task #1, *not* R-1.)
- Re-enable the guard at the end: once a file is clean, the existing
  `#define glBegin #error` pattern can be extended to `glVertex*`/`glColor*` to
  prevent regressions — but only after *all* game-code files convert, since the
  `#define` is global to any TU including `rRender.h`.

## Ponytail check — should we do this *now*?

R-1 delivers **zero user-visible change** on its own; its entire value is unblocking
R-2 (Metal) or #1 (VBOs/display-list removal). So **don't start R-1 speculatively.**
Do it when one of those is actually committed — then it's the necessary first step,
and a clean one. Until then it's enabling work for a backend we haven't decided to
build (R-2 is explicitly "only worth it once R-1 lands" — circular only if nobody
commits to the backend). Recommendation: greenlight R-2 or #1 first; R-1a is then
1–2 sessions of mechanical, individually-verifiable file conversions.

## Suggested order (when greenlit)

Warm-ups (small, prove the pattern): `eCamera`, `gExplosion`, `gFloor`, `rViewport`.
Then the big three: `gWall` → `eDisplay` → `gCycle`. Build + run after each;
expect no visual change. R-1b (matrix) after R-1a is green.
