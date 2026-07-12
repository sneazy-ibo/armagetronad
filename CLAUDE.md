# Armagetron Advanced — working notes for Claude

Old, long-lived C++ game (Tron lightcycles). Currently on a macOS branch doing an
SDL2 → SDL3 migration and responsiveness work. Working branch: `macos0.2.9.3.0`;
PR target: `trunk`.

## Build & lint
`./build.sh` — macOS Xcode build, terse output (errors + `** BUILD SUCCEEDED **`
only). `./build.sh -v` for the full log. **Don't** use root `make` for the mac app:
that's the autotools/Linux build. Build once after a logical change, not per edit.

`./tidy.sh src/path/file.cpp` runs clang-tidy (`brew` llvm). After build changes:
`./tidy.sh --refresh` rebuilds the compile DB from the Xcode log. Details +
gotchas in `docs/sharp-edges.md`. The editor's clangd is configured via `.clangd`.

## Prime directive: write code that reads like its neighbours
This codebase has its own idioms. In existing files, match them — don't modernise
in passing:
- Types: `REAL` (float), `tString`, `tColoredString`, `tOutput` (localised text),
  `tArray`/`tList`. Network messages are `nMessage`.
- Memory: `tNEW(T)(...)` and `tJUST_CONTROLLED_PTR<T>` (ref-counted), not bare
  `new`/`unique_ptr`. Many objects self-register in global intrusive lists.
- Settings: `tSettingItem<T>( "CONFIG_NAME", var )` exposes a console/config var.
- File-scoped statics with `sg_`/`sn_` prefixes are the norm for module state.

## Modern-C++ values (Jason-Turner-flavoured), applied with restraint
When writing *new* code or a genuinely fresh unit, prefer: `const`/`constexpr`,
RAII, narrow scopes, stdlib algorithms over hand loops, no raw owning pointers.
But: a clean diff that matches the file beats a "better" diff that doesn't. Don't
refactor surrounding code for style while doing an unrelated change. **No new
dependencies** for what a few lines do.

Exception — there is a *sanctioned* migration project to replace the custom
`tString` → `std::string` and `tSafePTR`/`tJUST_CONTROLLED_PTR` → `std::shared_ptr`,
so we can turn on stricter safeguards (warnings-as-errors, sanitizers, clang-tidy)
that gave false positives on the custom types. *During that project*, modernising IS
the task — but still one area at a time, building between steps. See `docs/TASKS.md`.

## Common tasks — where things are
- Net core: `src/network/nNetwork.cpp/.h` — `sn_Connect`, `sn_Sync`, net state,
  `peers[]`, `sn_Bend`. NetObjects + sync: `src/network/nNetObject.cpp`.
- Server list / master / queries: `src/network/nServerInfo.cpp/.h`.
- Server-browser UI + menu pump: `src/tron/gServerBrowser.cpp`.
- Game connect to a server: `src/tron/gGame.cpp` (`server->Connect()`).
- Walls / trail / display lists: `src/tron/gWall.cpp`.
- Textures / SDL3 image: `src/render/rTexture.cpp`. GL swap/clear: `rSysDep`.
- Timing: `src/tools/tSysTime.h` (`tSysTimeFloat` vs `tRealSysTimeFloat`),
  `src/engine/eTimer.h`. Console/idle pump: `src/tools/tConsole.h`.
- Menus: `src/ui/uMenu.h`. Strings: `src/tools/tString.h`. Smart ptrs:
  `src/tools/tSafePTR.h`. Settings macro `tSettingItem`: `src/tools/`.

## Hard constraints (learned the hard way)
- **Network is single-threaded by design.** Global singletons (`sn_GetNetState()`,
  `sn_Connections`, `peers[0]`) and `tRecorder` record/playback determinism depend
  on it. Do **not** add threads to the net path. Make slow things *cooperative*
  (step machines driven by the menu/frame pump) instead.
- **`peers[0]` is shared.** Both game-server queries (`QueryServer`→`sn_Bend`) and
  the master connection use connection 0. They cannot run concurrently — sequence
  them as phases. (This is why server-browser prefetch and master-fetch are
  separate phases.) See `docs/server-browser-cooperative-fetch.md`.
- **Wire format is frozen** for network compatibility. Change *scheduling*, never
  message contents/layout, unless explicitly asked.
- SDL3 migration is live: watch for image byte-order (BGR/RGB), audio, and timing
  API differences vs SDL2. Legacy OpenGL **display lists** are still in use
  (frozen geometry snapshots); VBO replacement is an open, not-yet-decided item.

## How this developer likes to work
- **Verify before you assert — and recursively.** Never state a fact, mechanism, or
  *severity* as established before checking it (grep / read / `./build.sh` / `curl` /
  WebFetch — you **do** have working network and both build schemes; use them). If a
  check proves you wrong, don't stop at that one point: **surface what else you
  assumed, state each assumption, and re-check until your assertions line up with
  reality.** Anything you couldn't verify, label "unverified." The user should never
  have to say "check your assumptions" — this was the #1 friction source across
  sessions. Lead security findings with the threat model + preconditions, *then* the
  severity label, not the other way round.
- **Measure before refactoring.** Add temporary instrumentation, get real numbers,
  then cut the minimum. (E.g. phase-timing showed the connect cost was the post-
  login syncs, not the login — so we skipped syncs instead of a big refactor.)
- **Incremental, test-each-step.** Ship the smallest verifiable slice, build it, let
  them run it, then continue. Commit WIP frequently with clear messages.
- **Push back with data, not vibes.** If a plan is over-built or an assumption is
  wrong, say so and show why; they prefer the lazy-but-correct path (ponytail mode).
- Prefer a recommendation over an option-menu; only ask when the answer changes the
  work and you can't settle it from the code.
- **Fix blockers; don't route around them.** This is the default, and it matters to
  this user. A missing tool → *install it* (`brew install …`), don't fall back to a
  lesser workaround and move on. A diagnostic fires → *fix the code*. A check can't
  run → *make it runnable*. "I noted that X is missing/broken and did Y instead" is
  the anti-pattern to avoid. Noting-and-deferring is a genuine **last resort**, only
  when fixing is out of scope or unsafe, and then it goes in `docs/sharp-edges.md` or
  a `// ponytail: WIP` note *with the reason* — never a silent skip.
- **Don't ignore LSP/clangd diagnostics — they are blocking.** Any diagnostic on code
  you touched MUST be resolved in the **same turn it appears**, before the next edit or
  before finishing. **NEVER dismiss one as "stale" or "false positive" by assumption** —
  re-run the build or `./tidy.sh` to confirm before dismissing. A passing build is NOT a
  substitute for clearing a flagged diagnostic (clangd catches things the build tolerates
  and vice versa). The ONLY pre-approved false positive is clangd "unused include" on
  shared `.cpp`s (the include may be needed by Win32/Linux targets), already suppressed
  in `.clangd` so it shouldn't appear; don't blind-remove includes.

## Debugging meta-lessons (learned the hard way on #9 — the expensive ones)
- **Suspect the PRODUCER, not just the consumer.** When code that cleans/reduces/processes
  data keeps choking on bad inputs, question whether the thing *producing* the data is
  broken — don't keep hardening the consumer. (#9: ~5 rounds fixing the mesh reducer before
  realising the mesh *construction* was emitting non-manifold garbage. That question was the
  whole game.)
- **Verify a debug channel actually EMITS in this build before trusting its silence.**
  `eDebugLine` markers are `#ifdef DEBUG` no-ops in the default Release build; "no marker on
  screen" got read as "event didn't happen" twice. The *file* logs were ground truth.
- **Front-load the safety net for work you can't test yourself.** Before writing risky code
  you can't exercise (DCEL surgery; you can't run the game), stand up the checker FIRST —
  the `DEBUG=1` Xcode build (`sharp-edges.md`) makes a bad edit trip a precise assert instead
  of silently corrupting. Don't write blind then hope.
- **Whack-a-mole tripwire + fresh-eyes checkpoint.** If ~3 iterations of fixes produce no
  *net* progress (e.g. MERGED stayed 0), STOP and challenge the approach/assumption instead
  of patching again. This is the antidote to anchoring — see the note below on using a
  cold-context reviewer (subagent or fresh session) to de-bias.
- **Calibrate confidence.** Say "deep/uncertain/fragile" when it is; don't call something a
  "one-line fix" or "almost there" when it's unverified surgery.

### Using a cold-context reviewer to break anchoring (opt-in)
The anchoring miss above is the one thing extra *agents* genuinely help — because a fresh
subagent (or fresh session) starts **cold, with no bias toward the path we're already on**.
Use it as a deliberate **skeptic at a tripwire**, not as continuous observers (more watchers
add cost/noise, not insight). Mechanism: at a stuck point, hand a fresh agent a tight problem
statement + current hypothesis + the stuck symptom and ask *"what are we assuming that's
wrong? producer or consumer? should we step back?"* Caveats: it can't run the game (no
surgery verification), and it starts cold (costs context to brief) — so propose it at the
tripwire and let the user decide, don't spawn reflexively. The other meta-lessons above are
discipline, not agent problems.

## Fresh-session continuity
The user prefers starting **new sessions** over compacting context. So everything a
fresh session needs lives in files — keep them current as you work, and when you learn
something add it:
- `docs/TASKS.md` — backlog + project status (tick off / add items each session).
- `docs/dev-log.md` — newest-first session log (add an entry when you finish work).
- `docs/sharp-edges.md` — gotchas + lessons (add when something surprises you).
- Deep-dives (e.g. `docs/server-browser-cooperative-fetch.md`,
  `docs/metal-port-review.md`) — one per area, as the work warrants.
**At session start, read through the docs before touching code** — at minimum
`TASKS.md` and the top `dev-log.md` entry, but skim `sharp-edges.md` and the
deep-dives too. The notes are the source of truth for where things stand; don't
re-derive from the code what a doc already records.

## Session workflow (how these sessions run — 6 stages)
Most sessions follow this arc. Name the stage when it helps; watch for stage-6 material
throughout.
1. **Load context.** Read `docs/` (at least `TASKS.md` + top `dev-log.md`). Current
   method, works fine.
2. **Agree the objective + scope BEFORE working.** If you're missing information,
   **stop and ask — do not guess.** Explicitly flag the scope — **explore / decide /
   implement** — and calibrate depth. The user is a software engineer: go technical,
   but always go at least deep enough to get your assumptions right. They'll say if
   they want terser or deeper.
3. **Code loop:** small change → play-test / check the numbers look right → validate
   against real-world data → iterate. Two mechanics that matter here:
   - **Don't rely on the in-game console for debug output — it's hard to copy from.**
     If you need text out, write it to a **file**.
   - **Visual in-game indicators are gold** — a line/marker/colour the user can see
     beats a number they have to read. Prefer them for anything spatial.
4. **Clean up.** Remove test cruft, temporary instrumentation, and debug prints. Leave
   the tree with *less* tech debt than you found — not optional.
5. **Commit.** Good, finished work → `macos0.2.9.3.0` (the user's fork). A side-piece or
   WIP → a **separate branch** to resume later (e.g. the `auth-https-migration` split).
6. **Debrief.** Two-way — give the user honest feedback too; they're learning as well.
   If a pattern keeps recurring *despite* earlier fixes, add a `TASKS.md` item to work
   it out deliberately (a fix that didn't stick needs its own session, not another
   promise). Leave notes as you notice things so stage 6 has material, not memory.

## Git
- Don't commit or push unless asked. Branch off `trunk` for PRs.
- **A `git-clang-format` pre-commit hook reformats changed lines** and *aborts* the
  commit when it does, leaving the reflow unstaged — so a naive commit needs two
  tries. Avoid that: stage, then run the formatter yourself before committing:
  `git add <files> && pre-commit run --files <files> ; git add <files>` then commit.
  (`pre-commit run` exits non-zero if it changed anything — that's fine; just re-add.)
  One clean pass, no failed first commit.
- End commit messages with: `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`
- Commit signing is configured; if it ever fails with a key/agent error, ask the
  user rather than disabling signing.

Claude's own memory index (`MEMORY.md` in the project memory dir) also holds
pointers to project status and confirmed approaches.
