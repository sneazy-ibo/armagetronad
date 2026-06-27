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
- **Don't ignore LSP/clangd diagnostics.** Treat them as errors: if clangd flags code
  you touched, fix it. The one genuinely-unsafe category — clangd "unused include" on
  shared `.cpp`s (the include may be needed by Win32/Linux targets) — is already
  suppressed in `.clangd`, so it shouldn't appear; don't blind-remove includes.

## Fresh-session continuity
The user prefers starting **new sessions** over compacting context. So everything a
fresh session needs lives in files — keep them current as you work, and when you learn
something add it:
- `docs/TASKS.md` — backlog + project status (tick off / add items each session).
- `docs/dev-log.md` — newest-first session log (add an entry when you finish work).
- `docs/sharp-edges.md` — gotchas + lessons (add when something surprises you).
- `docs/server-browser-cooperative-fetch.md` — the network/browser deep-dive.
Read TASKS + the top dev-log entry first to know where things stand.

## Git
- Don't commit or push unless asked. Branch off `trunk` for PRs.
- End commit messages with: `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`
- Commit signing is configured; if it ever fails with a key/agent error, ask the
  user rather than disabling signing.

Claude's own memory index (`MEMORY.md` in the project memory dir) also holds
pointers to project status and confirmed approaches.
