# Option B — Cooperative server-browser fetch

Replace the blocking master-list fetch with a per-frame pump so the UI stays
live, the list fills incrementally, per-server queries overlap the fetch, and it
is cancellable. This one refactor unlocks three optimisations:

- **#1 cache** — open the menu on the cached list instantly, refresh in the pump.
- **#2 query-as-it-arrives** — start ping/player queries while the list is still
  loading from the master.
- **#4 hover prefetch** — kick the pump off when the menu item is hovered.

Wire format is unchanged throughout (same messages, different scheduling).

## Current structure

- `gServerBrowser::BrowseMaster` (`gServerBrowser.cpp:230`): `GetFromMaster()`
  **[blocks]** → `BrowseServers()` → `StartQueryAll(sg_queryType)` + the
  `gServerMenu` render loop.
- `nServerInfo::GetFromMaster`: connect (`sn_Connect`, already responsive) → send
  `RequestSmallServerInfo` → **receive loop** (60 s; now `select()` + idle-pump) →
  cleanup (prune stale, `Save`, `nSTANDALONE`).
- `gServerMenu` pump (`gServerBrowser.cpp:868-877`): per-frame `sn_Receive` /
  `sn_SendPlanned` / `DoQueryAll` / render — **already cooperative**. This is the
  host loop we move the fetch into.

## What the #2 spike taught us (constraints)

1. Query config is browser-owned: type `sg_queryType` (`QUERY_OPTOUT`),
   `sg_simultaneous` (20). The network-layer fetch doesn't know them.
2. `DoQueryAll` is the pacing: caps in-flight at `simultaneous`, spaces by 25 ms,
   walks `sn_Requesting`, re-scans for stragglers. Firing `QueryServer` per
   arrival bypasses the cap. (In practice the master delivers ~11 servers per
   master-RTT, so in-flight self-paces to ~11/RTT anyway — the cap is a safety net.)
3. Per-server query init is bulk: `StartQueryAll` sets `queryType_` / `queried` /
   `advancedInfoSet` over a *complete* list; incremental arrivals miss it.
4. **`StartQueryAll` resets `advancedInfoSet = 0` on every server**
   (`nServerInfo.cpp:2056`) and clears `sn_Polling` — so querying during the fetch
   and then calling `StartQueryAll` (as today) wipes the head-start and
   re-queries everything. Must be addressed for #2 to stick.

## Target: one pump, small phase machine

```
CONNECT → FETCH ⟂ QUERY → DONE        (⟂ = overlap in the same per-frame pump)
```

- **FETCH**: each frame `sn_Receive` drains arriving servers; `GetSmallServerInfo`
  inits each new server for query.
- **QUERY**: `DoQueryAll(sg_simultaneous)` each frame, concurrent with FETCH once
  any server exists.
- Render every frame → live list, no beach ball, ESC-cancellable.

## Concrete changes

1. **Split `GetFromMaster` into Begin/Step/End:**
   - `GetFromMasterBegin(master, suffix)` — connect, send request, set fetch state.
   - `GetFromMasterStep()` — one old loop-body iteration; returns *still fetching?*
   - `GetFromMasterEnd(suffix)` — prune stale, `Save`, `nSTANDALONE`.
   Turns the 60 s `while` into resumable steps. State (timeout, suffix) → statics.

2. **`GetSmallServerInfo`** — on a new server during a browse, init it for
   querying (`SetQueryType(activeType)`, `queried = 0`) from an `activeType`
   static the browser sets, so `DoQueryAll` adopts it without a bulk pass.

3. **`StartQueryAll` no-reset tweak** — don't reset `advancedInfoSet` for servers
   already answered this run (or bypass `StartQueryAll` and continue the running
   pipeline).

4. **`gServerBrowser::BrowseMaster`** — create `gServerMenu` on the cached
   (`frommaster.srv`) list **first**, then run `GetFromMasterBegin` + a unified
   per-frame loop (`GetFromMasterStep` + `DoQueryAll` + render +
   `tConsole::Idle(true)` for cancel) until fetch *and* queries drain, then
   `GetFromMasterEnd`. Gives #1 at the same time.

## Risks / tricky bits

- **Master retry** (hardest): today timeout → recurse with the next master. In the
  step machine, `Step`/`Begin` reports timeout → the loop calls
  `Begin(GetBestMaster())` for the next. Thread the `multiMaster` retry through
  state instead of recursion.
- **Cancel**: `tConsole::Idle(true)` returns true on ESC → end gracefully, keep
  what arrived.
- **Recording/playback determinism**: `Select`/`Idle` already record; preserve the
  `tAdvanceFrame` cadence and message order across the per-frame split.
- **Overall 60 s timeout** → a wall-clock check inside `Step`.

## Increment plan (each builds + is verifiable)

1. Split into Begin/Step/End, still called in a tight local loop — **zero
   behaviour change** (safety checkpoint). ← keystone
2. Move the Step loop into the browser pump + interleave `DoQueryAll` → **#2**.
3. `StartQueryAll` no-reset → makes #2 stick (no double-query).
4. Open menu on cached list first → **#1**.
5. Hover prefetch trigger → **#4**.
6. Master retry through the step machine.
