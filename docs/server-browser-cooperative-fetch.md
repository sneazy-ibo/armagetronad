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

## FINDING: #2 (query-during-fetch) is infeasible on the shared connection

Tried in B-2 and reverted. Game-server queries (`QueryServer`) call
`sn_Bend(GetAddress())`, which sets `peers[0] = <game server>` — but `peers[0]`
**is the master connection** during the fetch. So the next `sn_SendPlanned()`
sends the master's acks to a game server instead of the master; the master's
send window (~10-11) fills with no acks and it stops. Symptom: the list hangs
after ~one window (~9-11 servers). Confirmed in `nNetwork.cpp:3127`
(`sn_Bend` → `peers[0] = address`).

Queries and the master fetch cannot run concurrently on `peers[0]`. A true
overlap would need a *separate* socket for queries (large change), and the
payoff is small: the master fetch is only ~4 s, while the per-server query phase
(140 servers, paced, with timeouts) dominates. So **#2 is dropped**; the fetch
must complete before querying.

Revised priorities: **#1 cache (instant cached list)** is the big perceived win
and still wants the B-1 `Step` split to refresh cooperatively. #4 hover prefetch
also stands. B-3 (no-reset) is moot without B-2.

## FINDING: the connect is a stacked blocking chain (gates B-4 and B-5)

Reading the menu flow for B-4 surfaced two crash/UX risks and one shared blocker.

**Risk A — the connect freezes the (now-visible) menu.** `GetFromMasterBegin` →
`masterInfo->Connect` → `sn_Connect` (`nNetwork.cpp:3146`) is **blocking, ~3 s**,
and it is not a single loop but a chain of blocking sub-phases:

1. send login message;
2. wait loop for `login_succeeded`/`login_failed` (5 s timeout; pumps
   `tConsole::Idle(true)` for the beach ball + ESC abort) — `:3238`;
3. on `Login_All` timeout it **recurses** with `Login_Pre0252` (`:3273`);
4. on success, `sn_Sync(40)` then `sn_Sync(40,true)` — each another blocking
   wait — plus relabeling (`:3295`+).

Run from the menu pump, step 2 alone is one frame that takes seconds → the
cached list appears instantly, then **freezes during the connect**.

**This is a shared blocker.** The same freeze:
- hitches B-4's background refresh (menu static for ~3 s), and
- makes **B-5 (hover prefetch) unusable** — hovering the menu item can't freeze
  the menu for 3 s.

So a cooperative (non-freezing) connect is a **prerequisite** for smooth B-4 and
for B-5 at all.

**Risk B — pruning servers while the menu is live.** `GetFromMasterEnd` `delete`s
servers no longer on the master. In the old flow that runs before the menu
exists; in B-4 the menu already holds `gServerMenuItem`s pointing at those
`gServerInfo`s → dangling-pointer crash risk. Mitigation: **skip the stale-prune
on the cooperative path** (the next browse's `DeleteAll` clears them anyway).

## CLEANUP JOB (prerequisite): cooperative connect

Goal: make `sn_Connect` drivable a step at a time so the caller's frame loop
keeps rendering, instead of blocking for seconds. Same shape as B-1.

- **Split** `sn_Connect` into:
  - `sn_ConnectBegin(server, loginType, socket)` — the setup + send-login
    (`:3146`-`:3231`); stores phase state.
  - `sn_ConnectStep()` — one iteration; returns CONNECTING / SUCCEEDED / FAILED /
    TIMEOUT. Internally walks the sub-phases: login-wait → (retry Pre0252) →
    sync1 → sync2 → relabel.
- **Keep a blocking wrapper** `sn_Connect(...)` = `Begin` + `while(Step==CONNECTING)`
  so game-server connects and every other caller are byte-for-byte unchanged.
- **Sub-phase wrinkles**:
  - `sn_Sync(40)` is itself a blocking wait — either split it too, or treat the
    post-login syncs as short enough to run in one step (measure first).
  - The `Login_All → Login_Pre0252` retry is recursion today → thread through the
    step state (a `loginType` field that downgrades once on timeout).
- **Payoff**: the browser drives `sn_ConnectStep` from the menu pump → no freeze;
  the menu stays live during connect; B-5 hover-prefetch becomes feasible.
- **Risk**: `sn_Connect` is shared with game-server joins; the blocking wrapper
  must preserve exact behaviour (timeouts, retry, deny/abort returns). Verify a
  normal game join still connects after the split.

Sequencing: do the cooperative connect (this cleanup) **before** B-4's
menu-first refresh, otherwise B-4 ships with the freeze and B-5 is impossible.

## SHIPPED (what actually happened)

The cooperative-connect refactor was **avoided**. Phase timing showed the ~2s
connect was almost all the two post-login `sn_Sync(40)` calls, which exist to
confirm game netObjects — of which the **master has none**. So instead of making
`sn_Connect`/`sn_Sync` cooperative, we just **skip the syncs on the master path**
(`waitSync=false`, threaded through `sn_Connect`/`nServerInfoBase::Connect`).
Connect dropped 2.05s → 0.34s; game joins keep `waitSync=true`, unchanged.
(commit "Skip post-login object syncs on the master fetch")

B-4 then shipped as a phase machine in the menu pump
(`gBrowserMenuItem::RenderBackground`), no cooperative connect needed:

- `PREFETCH` — `LoadCache` + `StartQueryAll`, ping the cached servers in score
  order for `BROWSER_PREFETCH_SECONDS` (default 1.5) so the visible first page
  shows real pings within ~1 RTT.
- `MASTERFETCH` — `GetFromMasterStart(reload=false)` keeps the prefetched list,
  `GetFromMasterStep(0)` streams the rest at frame rate, `Update()` each frame.
- `QUERY` — after the fetch, `StartQueryAll` the whole list; already-answered
  servers keep their ping because `Polling()` keys off `advancedInfoSetEver`
  (so B-3's no-reset turned out unnecessary).

Risk B handled: `GetFromMasterEnd(pruneStale=false)` on the live path. Joining
mid-fetch ends the fetch first for a clean `peers[0]`.

Remaining: B-5 (hover prefetch), B-6 (master retry through the step machine).
B-5 no longer needs cooperative connect either — the connect is 0.3s now — but
it does need the pump to run while the *menu item is merely hovered* (not entered).

## Increment plan (each builds + is verifiable)

1. Split into Begin/Step/End, still called in a tight local loop — **zero
   behaviour change** (safety checkpoint). ← keystone
2. Move the Step loop into the browser pump + interleave `DoQueryAll` → **#2**.
3. `StartQueryAll` no-reset → makes #2 stick (no double-query).
4. Open menu on cached list first → **#1**.
5. Hover prefetch trigger → **#4**.
6. Master retry through the step machine.
