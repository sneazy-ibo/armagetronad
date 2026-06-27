# Dev log

Newest first. One entry per work session: what changed, why, what to do next. Keep
entries short — detail lives in commits, `TASKS.md`, and `sharp-edges.md`. The point
is that a fresh session can read the top entry and know exactly where things stand.

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
