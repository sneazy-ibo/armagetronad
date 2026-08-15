# Rust rewrite — architecture notes & full engine inventory

*Status: exploration (2026-07). Nothing committed to. This doc exists so a rewrite can
start without architectural mistakes, based on an actual read-through of `src/`
(~117k lines) rather than surface impressions.*

## Agreed architecture (from design discussions)

- **Crate split:** `tron-sim` (deterministic core, engine-agnostic, no rendering) +
  `tron-client` (render/input/audio shell) + `tron-server` (headless, async net).
  The sim crate is the crown jewel; the shell choice (hand-rolled winit+wgpu vs an
  engine) stays swappable behind it.
- **ECS as pattern, lightweight (`hecs` or plain SoA) — NOT Bevy's World** for the sim:
  snapshot/rollback needs full control of storage. Client visuals can use anything.
- **Determinism:** f64 sim, same-machine determinism only (Rust default float semantics,
  no fast-math). Server-authoritative; clients predict + reconcile. NO fixed-point, NO
  cross-machine bit-matching, NO attempt to bit-match the C++ sim (libm differences make
  that infeasible). Interop with C++ peers = wire-protocol level only.
- **Event-sourced sim:** `step(state, events) -> state`; snapshot ring + event log =
  rollback fuel, replay format, and net wire content in one design.
- **Netcode goals:** symmetric ping charity (see `reference_netcode_and_movement_model`
  memory + https://bagnalla.github.io/posts/ping-charity.html), sub-tick turn timestamps
  (kills the current fps→turn-precision coupling), adaptive jitter buffer for peers,
  local prediction of own cycle.
- **Threads (client):** input (raw/evdev, kernel timestamps) / sim (fixed tick) /
  render (free-running, VRR, 1-frame-in-flight) / net (decode+stamp) — lock-free handoff.

---

## What the first sketch got WRONG (found by reading)

1. **Explosions are SIM, not VFX.** `gExplosion` is a networked game object that
   **blows holes in walls** (`S_BlowHoles`, `gPlayerWall::BlowHole`). Holes carry
   *attribution* (which explosion made them → kill credit for anyone dying in the gap).
   Rollback must therefore roll back holes too. Only the particle visuals are client-side.
2. **The wire model is destination-based, not raw-command.** `gDestination` = a
   timestamped intent (position, direction, distance, turn count, braking state,
   messageID). Clients broadcast destinations; every machine re-derives movement by
   steering the cycle through its destination list (`pendingTurns` deque,
   `GetDestinationBefore` on sync). This is *already* quasi-event-sourced — good news
   for the event-log design, and mandatory knowledge for any wire-compat bridge.
3. **Walls are piecewise, time-stamped, growing objects — not static segments.**
   `gPlayerWallCoord[]`: per-point `alpha`, `IsDangerous`, holer pointer. A wall has
   `BegPos/EndPos/Time(alpha)` — the segment behind the cycle *grows over time*
   (`CYCLE_WALL_TIME`-style build-up), trails have finite length that *shrinks from the
   tail* (`CYCLE_DIST_WALL_SHRINK`), and danger checks are time-dependent. Render wall
   (`gPlayerWall`) and net object (`gNetPlayerWall`) are separate classes.
4. **Physics ≠ "speed + rubber".** See the settings inventory below — 127 knobs in the
   cycle alone, including per-wall-type acceleration, which is *core competitive
   gameplay* (wall-riding speed).
5. **Scoring needs kill attribution.** `gEnemyInfluence` tracks the last enemy whose
   wall/sensor influenced this cycle (with time windows + penalties) so deaths credit
   the right player. A death event must carry this.

---

## Subsystem inventory (what actually exists, by C++ module)

### The sim proper

**`gCycleMovement`** (4.5k lines — read this before writing the Rust cycle):
- **Verlet integration**: `verletSpeed_` is speed of half a frame ago;
  `AccelerationDiscontinuity()` handles sharp accel changes; timesteps are *split at
  events* (turns, wall hits) — `TimestepCore` recurses at sub-events.
- **Acceleration sources** (per nearby wall, distance-weighted): own wall / team wall /
  enemy wall / rim wall each have separate `CYCLE_ACCEL_*` factors; plus `SLINGSHOT`
  (between two walls) and `TUNNEL` variants. Wall-hugging speed is THE core skill.
- **Boost**: turning off a wall grants `CYCLE_BOOST_*`/`CYCLE_BOOSTFACTOR_*` by wall type.
- **Rubber** (the death-forgiveness reservoir): `rubber`, `rubberMalus` (usage
  multiplier), `rubberSpeedFactor` (approach-speed shaping), depletion time tracking,
  `CYCLE_PING_RUBBER` (**extra rubber budget scaled by ping** — physics depends on a
  per-player network stat!), `CYCLE_RUBBER_LEGACY`, minadjust/mindistance family with
  **gap logic**: `gap_[2]`, `keepLookingForGap_`, `gapIsBackdoor_` — whether the cycle
  could squeeze through a gap changes rubber near wall ends
  (`CYCLE_RUBBER_MINDISTANCE_GAP*`).
- **Turn rules**: `CYCLE_DELAY` between turns, `CYCLE_DELAY_DOUBLEBIND_BONUS` (different
  delay for same-direction double turns), `CYCLE_DELAY_TIMEBASED`, turn *queueing*
  (`pendingTurns`), `CanMakeTurn(time, dir)`, `GetNextTurn`.
- **Braking**: a depleting/refilling reservoir (`CYCLE_BRAKE_DEPLETE/REFILL`, toggle mode).
- **Misc sim-relevant**: spawn protection/invulnerability (`CYCLE_FIRST_SPAWN_PROTECTION`,
  `CYCLE_INVULNERABLE_TIME`), `MoveSafely`, `maxSpaceAhead` raycast cache,
  packetloss/packetmiss tolerance (sync leniency), `CYCLE_AVOID_OLDCLIENT_BAD_SYNC`.
- **Cheat heuristic built in**: `uncannyTimingToReport_` — flags superhuman
  double-turn timing server-side (gCycleMovement.cpp ~3100, ~3400). Keep in rewrite.
- **Chatbot**: when a player is chatting, an autopilot (`gCycleChatBot`, `CHATBOT_*`
  settings) steers their cycle — sim-relevant, feeds the same command stream.

**`eGrid`/`eTess2`** (the collision world): a **half-edge planar subdivision** (points ↔
faces duality, `eHalfEdge` carries walls). Every `eGameObject` holds `currentFace`;
movement is face-to-face **edge-crossing walks**; collision = crossing an edge whose wall
is dangerous *at that time*; `eSensor` raycasts by walking faces. Walls are inserted
incrementally (`DrawLine` with splitting/intersection/simplification). ⚠ Rewrite
decision: replace the structure with a spatial hash + segment tests, but reproduce the
*behavior* (exact crossing order, time-dependent danger, hole segments). The mesh also
serves AI pathfinding and floor rendering — those need substitutes (navgrid / flat quad).

**`eTimer`** — game clock is a *networked object* with real sync machinery: averagers for
start-time offset, drift, and sync quality; game time = smoothed system time − smoothed
start; `IsSynced()` gates rendering after join. The rewrite needs an equivalent
clock-sync module (tick-stamped server time + client drift smoothing) — do not hand-wave.

**`eLagCompensation`** — server-side lag credit: `eLag::TakeCredit(client, lag)` /
`Credit(client)` + a base tolerance. This is the ping-charity budget accounting.

**`gGame`** — the round/match state machine **is itself a net object** (`state`,
`stateNext` synced; `StateUpdate()`). Match flow: warmup? → round init (arena build,
spawn) → play → round end (scores, winner announce) → next round / match end.
`gGameSettings`: game type (freestyle/duel/HvA), finish modes
(express/immediately/speedup/normal), win-zone spawn when rounds drag.

**`gWinZone`** — zones are `eNetGameObject`s with `tFunction` (offset+slope linear
time-functions) driving radius/rotation; kinds: win, death, ball/fortress-ish variants.

**`gArena`/`gSpawn`** — spawn point sets with orderly assignment; arena sizing via
`SIZE_FACTOR` (exponential scale).

**`gParser` + map XML** — maps define: **axes count** (not always 4! `ARENA_AXES`/map
axes — the winding system is per-map), spawn points, rim wall polygons (lines/rects),
zones, and per-element version alternatives. Maps are **resources** fetched by
`tResourceManager` from HTTP repositories with local cache; the **server dictates** the
map resource; auto-download on join.

**`gAIBase`/`gAICharacter`/`ePath`** — real AI subsystem: state machine
(survive / **AI_TRACE** wall-trace / **AI_PATH** pathfind-to-target / close combat),
per-bot ability profiles from `aiplayers.cfg` (`gAICharacter`), pathfinding **on the grid
mesh** (`ePath`, A*-ish with `ePATH_ORIGIN` back-links), sensor-based steering, team
balancing via AIs. Rewrite: bots emit commands into the event log (replay-stable).

**`eTeam`** — team formation/balancing: `maxPlayers`, `maxImbalance`, `balanceWithAIs`,
`enforceRulesOnQuit`, locked teams + invitations, color IDs, name-after-player,
score handling. Player↔team wishes live on `ePlayerNetID`.

### Network stack (all of this exists and matters)

- **`nNetObject`** — object sync model: per-client `knowsAbout[]` bitfields (creation
  acked / sync acked / sync requested), `ClearToTransmit` **dependency ordering** (a wall
  won't transmit before its cycle exists remotely), `SyncIsNew` ordering, ownership +
  `TakeOwnership` (server adopting client objects), client-owned-object rules
  (`AcceptClientSync`), global id registry with owner table, id recycling.
- **`nNetwork`** — connection state machine, login handshake, **`nVersion` +
  `nVersionFeature`**: per-connection negotiated protocol range; features turn on/off by
  version — this is the wire-compat mechanism. Reliable + unreliable message classes,
  acks, resends.
- **`nPriorizing`** — full **bandwidth QoS**: `nBandwidthScheduler`/`Arbitrator`/`Task`
  priority system feeding per-connection `nBandwidthControl` budgets. Sync messages are
  *prioritized within a byte budget*, not blasted. The rewrite needs at least a simple
  priority + budget scheme per client.
- **`nConfig`** — settings replicated server→client (`nSettingItem` vs local-only
  `tSettingItem`), with watchers and access-level requirements. Map/rules arrive this way.
- **`nAuthentication`/`nKrawall`** — md5 salt/scramble password handshake +
  **Armathentication** (remote authority auth, `user@forums`), feeding `tAccessLevel`.
- **`nSpamProtection`** + **`eChat`** — scored spam levels with time decay; prefix-spam
  detection (`eChatLastSaid::Prefix` lists).
- **`nServerInfo`** (3.3k lines) — master-server protocol, server browser polling, ping
  measurement; `gServerBrowser`, `gServerFavorites`, `gFriends` on top.
  `src/network/master.cpp` is the **standalone master-server binary**.
- **`nObserver`** — weak refs to net objects (used by kill attribution).
- **`nSimulatePing`**, `nettest`, `l2_demo`/`l3_demo` — test scaffolding.

### Player & identity model (two-class split — keep it)

- **`ePlayer`** = LOCAL profile (per local player, up to 4): name, camera prefs,
  **instant-chat macro strings**, per-player key bindings, spectate wish, team prefs
  (`favoriteNumberOfPlayersPerTeam`, `nameTeamAfterMe`).
- **`ePlayerNetID`** = networked identity (9.5k-line file!): access level
  (`eAccessLevelHolder`), silenced/suspended flags, team membership + wishes, scores,
  chat handling, /commands (msg, team, admin, vote…), name coloring & sanitization,
  spectator mode. ⚠ Names contain **inline color codes** (`0xRRGGBB` in strings —
  `tColoredString`); parsing/sanitizing them is everywhere (chat, HUD, scoreboard, logs).
- **`eVoter`** — vote items (kick/suspend/silence/include-config), voters identified
  **by IP** (`nMachine`), vote menus on clients.
- **Access levels** (`tAccessLevel`): Owner(0) → Admin → Moderator → … → TeamLeader(7)
  → TeamMember(8) → Local(12) → …; gates config items, chat commands, votes.

### Client shell

- **Input**: `uAction` hierarchy — `uActionPlayer` (bound per local player),
  `uActionCamera`, global actions; keyboard/mouse/joystick binds; action tooltips are
  config items; instant chat = N bindable macros per player.
- **Splitscreen**: `rViewportConfiguration` — nested proportional viewports, up to 4
  local players, each with own camera + HUD; subviewport composition math in rViewport.
- **Camera**: modes smart/custom/server-custom/follow/free/incam + **glancing**;
  smart-cam wall avoidance via sensors (`se_visibility*` settings). Port the *measured
  0.2.9 behavior* (see `reference_camera_feel_029_vs_04` memory: 0.4 camera code is
  byte-identical; feel differences come from elsewhere).
- **Cycle visuals**: model rendering (`rModel`, ASE format + moviepack support), wheel
  rotation + suspension, **skew (lean into turns)**, tail **blinking**
  (`CYCLE_BLINK_FREQUENCY`), and — critical — **`correctPosSmooth`**: displayed pos =
  sim pos + exponentially decaying correction offset. This is how server corrections are
  *hidden* today; the rewrite's reconciliation should do the same (never teleport
  visually).
- **VFX**: `gExplosion` visuals (expanding shockwave ring), `gParticles` (debris),
  `gSparks` (grind sparks with wall-color), `gLogo`; trail glow; floor grid texture
  (`gFloor`/`eFloor`, render-only now that grid-mesh floor is gone).
- **HUD** (`gHud`): speed/rubber/brake gauges, ping, framerate, clock, message zones;
  scoreboard; center countdown messages; console overlay (`rConsole` + `rConsoleCout`
  stdout mirror); chat input line with history.
- **Audio** (`eSound`): custom **software mixer inside the SDL audio callback** —
  `eWavData`/`eSoundPlayer`, per-source stereo volume + playback speed (pitch = engine
  hum by cycle speed) + seamless looping. Rewrite: `rodio`/`kira` gives this for free,
  but keep pitch-by-speed + positional volume.
- **Menus** (`uMenu`): settings menus are *auto-generated from config items*
  (`uConfigItem`); language menu; server browser UI. Console + menus should NOT be
  ECS entities (plain structs + immediate-mode draw).
- **`rScreen`/`rSysdep`**: resolution/fullscreen management, swap control, GL init.
  `rDisplayList`, `rTexture`, `rFont` (textured bitmap font) — all replaced by wgpu-era
  equivalents.

### Tools layer → Rust equivalents

| C++ | What it is | Rust answer |
|---|---|---|
| `tConfiguration` | config items w/ access levels, doc strings, CASACL, layered .cfg files (default/autoexec/everytime/user) | settings registry struct + serde; keep access levels + file layering; console `SET`/`SET x help` |
| `tRecorder` | **section-based record/playback of every nondeterministic input** (net packets, sys time, random, input) — full-process replay debugging | sim event log covers the sim; consider recording net+time at the shell boundary too for full-session replays |
| `tLocale`/`tOutput` | **deferred translation**: messages cross the wire as template-ID + args; client renders in own language | keep this exact design — server sends message IDs + params, never rendered strings |
| `tString`/`tColoredString` | strings with inline `0xRRGGBB` color codes | a small ColoredString type with parse/strip; used by chat/names/console everywhere |
| `tRandomizer` | seeded RNG singleton | seeded RNG in sim state (snapshotted) |
| `tDirectories` | per-platform data/config/resource path resolution + search order | `directories` crate + resource cache dir |
| `tResourceManager` | HTTP fetch + cache of maps/resources, server-dictated repo | `reqwest` on net/IO thread + cache; same trust model (server names resource, client fetches by URI) |
| `tHeap`, `tList`, `tArray`, `tMemManager`, `tSafePTR` | custom containers/allocator/smart ptrs | std collections; delete the lot |
| `tConsole`/`tCommandLine` | console with filters, idle pump; CLI parsing | plain; `clap` for CLI |
| `tError`/`tBackTrace`/`tCrypt`/`md5` | crash handler, hashing | `std::panic` hook + `backtrace`; `md5` crate only for legacy auth compat |

---

## Corrected ECS inventory (delta over the chat sketch)

**Sim entities/components — additions:**
- `Explosion { center, time, radius_fn, owner }` — SIM entity; holes walls in radius.
- Wall/trail storage: per-cycle ring of `(pos, time_built, danger_intervals)` where
  danger intervals subtract **holes** `{begin_d, end_d, holer: PlayerId}` — holes need
  attribution and are rollback-affected.
- `Destination`/command queue per cycle: `pendingTurns`, turn-delay state
  (`lastTurnTimeLeft/Right`), braking flag.
- `EnemyInfluence { last_enemy, time }` per cycle — kill attribution.
- `RubberState { used, malus, speed_factor, deplete_time }` + gap-analysis scratch.
- `BrakeReservoir(f64)`.
- `Invulnerability { until, spawn_protection }`.
- `ChatState { chatting, chatbot_active }` — sim-relevant (autopilot).
- `AiState { mode: Survive|Trace|Path|CloseCombat, character, path, timers }`.
- Resources: `SpawnLayout`, `AxesConfig` (per-map winding count!), `WinZoneRule`,
  `LagCreditLedger (per client)`, `MatchFlow { state, next, timers }` (synced),
  `SimRng (seeded, snapshotted)`.

**Sim systems — additions:** `chatbot_autopilot`, `ai_think` (emits commands),
`explosion_holing`, `wall_growth_and_shrink` (time-dependent danger), `gap_analysis`
(feeds rubber), `wall_accel` (per-type acceleration + boost), `turn_queue_drain`
(delay rules), `kill_attribution`, `win_zone_spawn` (round-drag rule),
`team_balance` (round boundaries), `uncanny_timing_check`.

**Server — additions:** auth handshake (legacy md5 + remote authority), access-level
enforcement on config/commands, vote engine, spam scoring, bandwidth budget scheduler,
master-server announce/heartbeat, resource-repo pointer config, ladder/highscore files,
per-client protocol-version feature gates.

**Client — additions:** splitscreen viewport tree + per-viewport cameras/HUDs (design in
from day one — it warps camera/HUD/input assumptions), instant-chat macros, friends +
favorites + server browser (master protocol client), auto-download of map resources,
correctPosSmooth-style visual error decay, colored-string rendering, auto-generated
settings menus from the settings registry, language files.

---

## Deep-read findings (second pass: eGrid.cpp, gCycle.cpp, gGame.cpp, ePlayer.cpp bodies)

### eGrid.cpp — the mesh is stranger than its header suggests
- **The world is one growing triangle.** `Create()` makes a triangle (radius ~100);
  `Range()`/`Grow()` scales it ×4 on demand up to 1e15. The outer rim = 3 unsplittable
  `eWall`s. Map rim walls are ordinary walls inside it.
- **Geometry self-mutates to fix float errors.** `eFace::CorrectArea()` heals
  negative-area (flipped) triangles **by physically moving grid points**
  (`ClampMovement`-limited). The collision world warps itself slightly whenever
  numerics degrade. Don't port; do note that exact replay comparisons vs C++ are
  hopeless partly because of this.
- **Silent give-up paths**: `DrawLine` has a 10k-step timeout → wall placement can
  *silently stop partway* (`requestCleanup`); `FindSurroundingFace` timeout → O(n)
  linear scan fallback. The game shrugs and keeps going.
- **Fuzzy float equality is load-bearing.** `eCoord::operator==` is EPS-based; point
  merging on insert and a dozen degenerate-case branches (`*A == *C` restart, endpoint
  snapping, ratio clamped to [1e-5, 1-1e-5]) depend on it.
- **`BUG_RIP`** — a *network-synced setting* that deliberately re-enables an old
  wall-rip bug for old-style play. Bug-for-bug compat as a feature.
- **Tie-breaking is directional**: `eFace::Insideness()` biases its epsilon by the
  object's movement direction — deterministic resolution of "exactly on the edge".
- The growing trail tip lives **outside** the mesh (`wallsNotYetInserted` list) and has
  its own broad-phase path in `ProcessWallsInRange`. Three trail representations:
  current (ungridded) wall / gridded walls / render walls.
- Background **incremental simplification** with budgets (`se_simplifyEdges`); a face
  with refcount > 4 = "an object stands here", exempt from collapse. Zombie faces +
  replacement graph migrate stale `currentFace` pointers.
- `tRecorderSync` archives grid edge IDs (`_GRID_ADD_EDGE`…) — desync self-checks are
  baked into recordings.

### gCycle.cpp — the client is already a rollback machine
- **The extrapolator IS client-side rollback-resim.** On each server sync, the client
  spins up a *complete second simulation instance* (`gCycleExtrapolator`), seeds it
  from the sync state, re-simulates it forward **through the buffered destinations** at
  `SYNC_FF`=10× speed (spread over frames!), and when it catches up, swaps its state in
  (`SyncFromExtrapolator`). The visual difference goes into `correctPosSmooth` and
  decays smoothly (`DecaySmooth`, min-speed + out-of-bounds acceleration). The crude
  backward-forward method (`old_ReadSync`/`SyncEnemy`) survives as the legacy/enemy
  path. So the rewrite's predict+reconcile is *less* novel than assumed — port the
  policies, not the mechanism.
- **Future walls aren't dangerous — "I was here first."** `EdgeIsDangerous` ignores
  walls whose per-point build time is later than *my* time at the crossing point; the
  other cycle gets pushed back (or killed) when *it* passes. Exception: teammates that
  can't be pushed back are protected. A `gJustChecking` RAII flag makes this rule apply
  only to speculative checks, not actual passage. This is the engine's simultaneity
  resolution — port it deliberately or grinding races change.
- **Death mechanics**: preliminary walls get gridded; the dead cycle's *not-yet-drawn*
  wall beyond death distance is blown into one giant hole (`sg_KillFutureWalls`);
  position is forced to death pos so the explosion sits right.
- **Sacrifice scoring**: driving through a hole a *teammate* blew in an *enemy* wall
  fires the SACRIFICE ladder event with three-way attribution (driver, holer, wall
  owner) and one-shot accounting (`AccountForHole`). Default scores: kill +3, die −2,
  suicide −4 (`sg_suicideTimeout` bounds influence age), hole/survive 0.
- **Wire format details**: sync fields were appended over protocol history — readers
  are chains of `if (!m.End())` guards. Rubber/brake cross the wire as 16-bit
  fixed-point 0..1 (`gFloatCompressor`), and rubber is sent *skewed*
  (`pre = rubber/(RUBBER+.1)`, `malus = 1/(1+malus)`). `SyncIsNew` orders by
  (time, turn count).
- **Corrections are physically sanified**: `se_SanifyDisplacement` raycasts the
  correction path, projects onto any hit wall, backs off ×.99, ≤5 iterations;
  `ClampForward` forbids corrections behind the last turn.
- Server does **emergency syncs the moment rubber engages** (`RequestSyncOwner`), and
  walls checkpoint at rubberSpeedFactor thresholds (.99/.1/.01).
- **Server watchdog**: a cycle lagging > `Lag()+1` behind sim time is force-killed
  (guards a known invulnerability bug).
- Chatbot victims get a 30s attribution penalty (`sg_enemyChatbotTimePenalty`) — dying
  to an autopilot counts differently.
- `Timestep` tolerates `currentTime < lastTime` ("nothing special if simulating
  backwards") — backward simulation is a supported code path.

### gGame.cpp — round flow is an ecosystem API
- State machine: `GS_DELETE_GRID → TRANSFER_SETTINGS → CREATE_GRID → CREATE_OBJECTS →
  TRANSFER_OBJECTS → CAMERA → SYNCING → PLAY`, with **server-side blocking loops**:
  wait-until-every-netobject-transmitted-per-client (with timeout + per-object blame
  console output), and **`WaitToLeaveChat()` — the round literally won't start while
  players are chatting**. Map parse failure → auto-revert to previous map.
- **External scripts are first-class citizens**: `WAIT_FOR_EXTERNAL_SCRIPT` blocks the
  round loop (reading stdin) until a script acks; `DECLARE_ROUND_WINNER` lets a script
  dictate the outcome. This + ladderlog is the contract the sty/ct server ecosystem is
  built on — the rewrite's server must preserve an equivalent hook surface.
- Built-in **highscores + ladder with betting math** (min bet, 10% percentage bet, tax,
  decay-on-load) writing flat files. `MAP_FILE` is a *version-watched, protocol-breaking*
  setting (`Group_Breaking`, v8).
- Win detection (`Analysis`) counts alive teams but special-cases
  "alive but disconnected" humans; win zone spawns with randomness after round drags.
- Client join: player info is withheld until the game timer reports `IsSynced()`
  (`synced_` gate) — joining is a staged handshake, not an event.
- Dedicated server self-regulates at `DEDICATED_FPS` 40 (idle factor 2).

### ePlayer.cpp — a small operating system (9.5k lines, 160 settings, 28 /commands)
- **Full IRC-style moderation** via chat: `/login` (admin login *through the chat
  pipeline* — see `se_AdminLogin_ReallyOnlyCallFromChatKTHNXBYE`), `/op` `/deop`
  `/invite` `/lock` `/shout` `/team` `/msg` `/me` `/vote` `/players` `/rtfm` `/teach` …
  with per-command access levels and a spam-tester threaded through every path.
- **`eUncannyTimingDetector` is a real anti-timebot system**: three timescale
  analyzers (fast/medium/slow — slow disabled) using a bucket-ratio test (fraction of
  turns landing in the "too accurate" half over a 40-event window vs a max-human
  ratio), sensitivity knob, and a **4-level danger escalation with configurable actions**
  (`se_timebotAction{Medium,High,Max}`). Directly relevant to tron-integrity; port the
  statistical core.
- **Play-time gating**: the client accumulates PLAY_TIME_{TOTAL,ONLINE,TEAM} and
  servers can demand minimums (`MIN_PLAY_TIME_*`) before you may play. Client-reported
  → trivially forgeable → known-weak trust model; rewrite should server-track.
- **Name machinery is deep**: escape/unescape for auth names (`@`, `%XX` hex, legacy
  log format), `ALLOW_IMPOSTORS`, hidden-player prefix for the player list, forced
  RENAME (moderator), color codes inside names everywhere.
- Shout vs team-chat defaults per spectator/player, `ACCESS_LEVEL_SHOUT`/`_PLAY`
  (including a *sliding* play access level), team-chat routing incl. spectators,
  suspension decrement + shuffle-spam testers at round boundaries.

---

## Architecture traps (each of these bit someone once)

1. **Anything that can affect a death must live in the sim event/rollback domain** —
   including explosion holes, chatbot steering, AI commands, mid-round setting changes
   (`SettingChange` is an event), and invulnerability windows.
2. **Physics reads network state**: `CYCLE_PING_RUBBER` scales rubber by ping. In the
   rewrite, per-player rubber budget becomes explicit per-player *state* set by the
   server (an event), never a live network read from inside `step()`.
3. **Walls are time-dependent**: a segment is dangerous only after its build time;
   trails grow at the tip and shrink at the tail; holes open (and close? no — holes are
   permanent once blown, but *rollback can re-decide them*). Collision tests take a time
   argument today (`IsDangerous(alpha, time)`). Keep that signature.
4. **Turn semantics are subtle**: delay differs for same-direction vs opposite double
   turns; turns queue rather than drop; sub-tick timestamps must respect delay rules or
   grinding changes feel. Port `CanMakeTurn`/`GetNextTurn` semantics exactly.
5. **The current protocol is destination-based** — any bridge/compat layer or behavior
   comparison must speak destinations, and destination application on sync
   (`GetDestinationBefore`) is fiddly. Don't plan C++-server interop casually.
6. **Clock sync is a real subsystem** (offset+drift averagers, quality gating). Budget
   for it; "client uses server tick numbers" is not enough for smooth rendering.
7. **Splitscreen (≤4 local players) and per-map axes counts (≠4)** are cheap if designed
   in, expensive to retrofit. Winding/axes must be data, not a hardcoded 4.
8. **Messages cross the wire language-neutral** (template + args). Design the
   chat/console message enum that way from the start.
9. **Scoring/ladder needs attribution chains** (enemy influence, hole ownership) — a
   plain "cycle X hit wall" event loses information the current game keeps.
10. **The 127+ physics settings are the game.** Competitive servers differ wildly
    (sty/ct/rubber configs). The sim config must be a complete, versioned, serializable
    struct synced to clients — it IS part of protocol state and part of replay files.
11. **Recorder-grade debuggability** is a superpower of the current engine (deterministic
    full-process replay drove decades of bug fixes). The event log gives the sim part;
    keep shell-boundary recording in mind for net/time.
12. **The future-wall simultaneity rule** (walls not yet built at your crossing time are
    passable; the later cycle gets pushed back) is invisible in normal play but decides
    grinding races and head-to-head near-misses. Port it explicitly; naive "wall exists
    or not" collision changes outcomes.
13. **The external-script/ladderlog contract is an API.** Server operators' whole
    ecosystem (sty/ct tooling, tournament scripts) drives rounds via ladderlog output +
    stdin commands (`DECLARE_ROUND_WINNER`, `WAIT_FOR_EXTERNAL_SCRIPT`). A rewrite
    server without an equivalent surface is dead on arrival for hosting.
14. **Client reconciliation is already resim-based** (the extrapolator), with smooth
    visual error decay + raycast-sanified corrections that never tunnel through walls
    and never move behind the last turn. These *policies* (sanify, clamp-forward,
    decay-with-min-speed) are the accumulated fix for years of visual glitches — reuse
    them verbatim.
15. **Anti-cheat exists and is statistical**: the timing-bot detector (bucket-ratio over
    40-turn windows, multi-timescale, escalating actions) — don't lose it in the
    rewrite; it's also prior art for tron-integrity.
16. **Protocol grew by appending optional fields** (`if (!m.End())` chains) — any
    wire-compat bridge must replicate exact field order *and* absence semantics, plus
    quirks like skewed rubber encoding and 16-bit fixed-point compression.

## Suggested first milestone (unchanged by the read-through, but now honest)

`tron-sim` + terminal client over localhost with fake latency/jitter:
cycles, growing/shrinking walls, rubber (basic + malus), turn delay + queue, wall accel,
explosion holing, kill attribution, event log + snapshot rollback, symmetric charity,
prediction + reconciliation. That set exercises every trap above except splitscreen,
auth, and resources — and it's still only a few thousand lines of Rust.
