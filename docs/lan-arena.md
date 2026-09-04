# LAN arena development guide

The LAN arena is a closed, authoritative shared-simulation two-process
experiment. The host process currently runs that shared simulation: it
controls Tal and resolves AI, damage, resources, the training dummy, and
native action execution. The client controls Ailish and renders authenticated
snapshots through its own full-screen Sudeki process. Combat mode itself is
not invented by the host networking layer; Sudeki's native dungeon/world
trigger changes that state and the shared simulation replicates the result.

The cross-module input-to-authority-to-wire-to-replay contract and its next
research targets are maintained in the
[LAN arena combat synchronization graph](lan-arena-combat-graph.md).

The first shared-simulation extraction is now explicit in
`lan_arena_shared_simulation`. Player identity and simulation authority are
separate concepts. A canonical native-world node commits validated frames;
replica nodes may only accept authenticated, monotonically newer frames for
presentation. Both are pinned to a fresh session token. A canonical commit
requires an explicit native combat observation, which replaces the candidate
frame's combat bit before validation. Consequently, neither Tal input,
Ailish input, nor a client presentation adapter can author combat mode. The
listen server currently places the canonical node in Tal's process, but the
contract has no Tal/host dependency and can move to a dedicated process.

The native-world observation now owns the complete shared consequence domain:
match/combat state, Tal and Ailish HP/SP, and the bounded enemy set. A candidate
snapshot no longer exists at the canonical boundary. Independent Tal and
Ailish actor observations supply only presentation and transforms; the reducer
combines them with the world observation into a fresh frame. This keeps
future actor-owned input or movement contributions from quietly becoming
damage, resource, enemy-spawn, or match authority.

This profile never reads, writes, copies, or transfers campaign saves. It
always launches Sudeki's cleanroom `testroom`, and it does not share hooks or
state with the local split-screen, fixed-three, roster, or custom QuickMenu
experiments.

## Current playable slice

- Protocol/build: `LA22`, exact GOG executable hash only. Actor snapshots
  include a bounded four-edge action journal so rapid Tal combo stages are
  presented once instead of being collapsed by the 20 Hz snapshot cadence.
  The currently active semantic action also carries a 1/256-unit host phase;
  clients interpolate that phase instead of independently running a terminal
  attack clock. Post-action snapshots retain the host-observed terminal action
  phase and first idle phase until the next action, so a lost/coalesced packet
  cannot omit the authored tail or restart idle at a client-invented zero
  timestamp.
- Roles: Tal host, Ailish client.
- Transport: direct IPv4 UDP, default port `26770`.
- Client actions: camera-relative movement and host-validated weak attack.
- Host tools: `F8` opens the cleanroom tools overlay in the host process. The
  LAN runtime retains actor/dummy lifecycle ownership, while Combat Mode,
  Infinite SP, Infinite Spirit, Infinite Jetpack, and All Party Skills use the
  existing native cleanroom adapters. Resource and skill cheats cover every
  present retail party actor; the client may browse the mirrored training
  catalog, but only the host executes an activation.
- Host snapshots: Tal, Ailish, the fixed training dummy, HP/SP, transforms,
  facing, presentation/combat state, and match state at 20 Hz.
- Presentation: each process uses a separate full-screen native camera and HUD.
- Remote Tal skills on the Ailish client run a presentation-only native
  `CSkill::Use` for the authenticated host-approved slot. Client damage and
  resource authority remain blocked, the Ailish-owned camera is retained, and
  native time-stop requests are held at realtime. The matching host snapshot
  retires the task and restores the exact owner-view lease.
- Remote Ailish skills still execute their native task on the authoritative
  host for gameplay effects. Their native game-speed mode lifecycle remains
  intact. After mode 2 applies its native Player 1 disable transition, the
  host invokes Sudeki's exact inverse seat-0 transition once and presents mode
  0 only while Tal's controller update runs. The task-owned mode is restored
  immediately afterward. This keeps Tal independently mobile without
  preventing the remote native skill task from reaching its cleanup. Because
  Sudeki omits the ordinary Player-1 movement call while mode 2 owns the task,
  the controller boundary reads Tal's live axes and uses the verified native
  camera transform plus collision-aware absolute-delta movement path.
- While Tal owns a native skill task, authenticated Ailish input remains live
  through the same native remote actor lease. If Sudeki omits or rejects its
  usual non-caster movement path, the host uses the same bounded
  absolute-delta fallback. In both directions only the caster is skill-locked;
  the other player remains independently mobile.
- Tal's retail-global Spirit transaction is a distinct wire presentation,
  never a fabricated `CSkill` slot. The host sends the verified Tal renderer
  channels under one Spirit sequence, the client presents those channels
  without starting another gameplay task, and Ailish remains independently
  mobile. During either actor's skill, a host-side non-caster compositor owns
  only that other actor's base RUN/IDLE channels so Sudeki's global skill mode
  cannot visually freeze a player who is still moving in real time.
- Spirit audio is presentation-only and deliberately narrower than the native
  transaction. Fresh traces of both variants showed `spiritstrike_start`
  followed by six `stop_*` cues. LA22 journals only the semantic START edge,
  bound to Tal's exact active Spirit sequence; the client maps that enum to a
  compiled-in cue name and calls only its local `CSound::PlayCue`. Stop cues,
  raw strings, Spirit-manager entry, effects, camera, time, and damage never
  cross this adapter. The client preflight verifies the relocated `GetSound`
  singleton operand against its live module base and the complete `PlayCue`
  body before enabling replay. Eight bounded events make the theoretical
  maximum snapshot 746 bytes, below the fixed 768-byte datagram limit.
- The host-only `--host-spirit 1|2` diagnostic enters that same retail
  transaction through a callback-free two-phase rail. The exact game-thread
  observer requires an authenticated canonical session, stable initialized Tal
  and Ailish identities, Tal's exact Player-1 controller target, Ailish's exact
  Player-2 lease, native combat, no menus/skills/Spirit transaction, and an
  available Tal option. A teardown barrier spans native option validation and
  the existing 75 ms UI prime. After positive retirement, it re-proves the
  session token, roles, both actor identities/control leases, and native state
  before calling the retail validator/activation under the same barrier. The
  operator request never allocates a wire sequence; only the later observed
  native Spirit-manager edge does so.
- Replica locomotion: continuous movement retains a 100 ms interpolation
  buffer (two 50 ms host snapshots). A moving-to-idle edge consumes that final
  buffered distance in its run pose instead of snapping or sliding. Tal's two
  verified native idle
  variants and Ailish's verified selector-4/selector-5 idle variants cross the
  wire as actor-neutral semantic states. Each client maps those semantics back
  to its actor-local native clips; native selector numbers never cross the
  network. A generic Ailish idle also suppresses client-only fidgets, while her
  renderer still owns animation-clock and blend progression inside the
  authenticated semantic clip.
- Combat is native world state. In campaign play, Sudeki's authored
  dungeon/world trigger changes the group combat flag; the shared simulation
  observes that result and serializes it in every snapshot. Each client mirrors
  the same verified native transition for camera, HUD, weapons, and actor
  presentation. Out-of-combat attack edges are consumed without native
  execution or a replicated attack pose. Because the cleanroom has no authored
  dungeon trigger, the Multiplayer page and `F8` expose an explicitly
  test-only way to invoke the same native transition. They are not the
  production combat-state source. Damage, AI, targeting, and action execution
  remain canonical shared-simulation outcomes.
- A bounded semantic action variant crosses the wire; each process keeps its
  native selector numbers local. The host captures and executes those actions.
  After `F8`, Sudeki's native Tal/Ailish weapon-draw transition first attaches
  the weapons; replica animation resumes only after that transition reaches
  its verified idle. On the Ailish client, readiness also requires Sudeki's
  actor-scoped ranged model refresh, a successful native `WeaponFollow`
  reattachment, and a visible weapon render object; summon particles alone do
  not satisfy the gate. Ailish's first-person and world renderers are treated
  as distinct tables, and every world selector is resolved through the active
  actor-local animation bank before it may be written. Leaving combat likewise
  waits for the native sheath transition before world presentation resumes.
- Host approval of an Ailish skill does not override an invalid client actor
  state. Native validator results `2` and authenticated combat result `3`
  trigger Sudeki's ranged combat/UI priming and a delayed retry; both the outer
  validator and the validator nested inside `CSkill::Use` must then return
  zero. This keeps sound, camera ownership, task completion, and movement
  release on one valid native skill lifetime instead of admitting a partial
  presentation task.
- Tal's melee presentation now follows the native combo transition graph, not
  inferred mouse edges. `W` means native Weak/Mouse1 and `S` means native
  Strong/Mouse2. The first two stages and all eight observed three-input
  branches are represented independently:

  | Native input history | Host selector | LAN semantic |
  | --- | ---: | --- |
  | `W` | 50 | Weak 1 |
  | `S` | 52 | Strong 1 |
  | `WW` | 51 | Weak 2 |
  | `WS` | 53 | Strong 2 |
  | `WWW` | 62 | Weak 3 |
  | `WWS` | 54 | WWS finisher |
  | `SWW` | 60 | SWW finisher |
  | `SSS` | 61 | SSS finisher |
  | `SWS` | 63 | SWS finisher |
  | `SSW` | 65 | SSW finisher |
  | `WSW` | 68 | WSW finisher |
  | `WSS` | 69 | WSS finisher |
  | `WSS` (native alternate) | 70 | WSS alternate finisher |

  Sweep and block retain separate semantic variants. Inputs rejected by
  Sudeki's timing/state gates do not create a LAN action: the host transmits
  only an observed native selector transition. This prevents an ignored late
  click from fabricating another attack on the client. Selector `70` is kept
  distinct because the same acknowledged `WSS` history selected both `69`
  and `70` under different native timing/target/direction conditions; the
  exact transition lookup confirms those conditions remain host-owned.
- Action retirement is also part of the shared timeline. While a native action
  is active, the canonical simulation quantizes its actor-local animation
  clock into the LA18 semantic snapshot. The first host IDLE snapshot remains
  a latched retirement witness, so traces and bounded fallback presentation do
  not lose the authored tail when snapshots coalesce.
- Tal's normal client presentation no longer forces those renderer clocks.
  Each new authenticated semantic action is replayed as the corresponding
  weak/strong/sweep/block input on the client Tal arbiter. Sudeki's own combat
  graph chooses the expected host-observed selector and performs its native
  transition back to combat idle. The client installs an authenticated
  `ApplyDamage` guard around this presentation-only replay; HP, reactions, and
  world consequences continue to come only from host snapshots. If native
  admission never reaches the expected selector within the bounded lease, the
  existing low-level snapshot presenter resumes as a fail-safe.
- Input acknowledgement is also simulation-owned in LA18. The socket layer
  may receive and coalesce packets, but it cannot acknowledge one merely for
  reaching the host process. The canonical reducer validates and admits the
  actor-scoped contribution first. Its wire actor must match the authenticated
  player role at both the transport and reducer boundaries; only that admitted
  sequence may appear in a later snapshot. Tal and Ailish input histories are
  independent, while neither can contribute match, enemy, resource, or native
  combat state.
- Match lifecycle is one-way within a session token: waiting may become
  active or ended, active may become ended, and ended cannot reactivate.
  Waiting/ended frames must carry combat disabled. Starting again requires a
  fresh handshake and token rather than reviving stale shared-world state.
- The client may open and browse Ailish's native QuickMenu. Native confirm/use
  commands are consumed locally until category-specific requests can be
  validated and executed by the host. Skills, items, weapons, client-originated
  Spirit,
  save/load, transitions, dialogue, shops, and loot remain non-authoritative
  or blocked in this slice.

Packets are versioned and sequenced. The `LA22` handshake validates the exact
game hash, mod build, cleanroom map, fixed Tal/Ailish player-role tuple,
independent canonical/replica simulation-node tuple, and a fresh session
token. Connected packet direction is authorized by simulation node rather
than character identity. The current playable topology still admits only
Tal/canonical and Ailish/replica; the separation is a safe migration seam, not
a claim that arbitrary role assignment is playable. Stale, duplicate,
malformed, mismatched, or wrong-authority packets are rejected. A timeout ends
the session; it never silently reconnects into the old session. Cleanup stops
inputs, discards client replicas, and restores Ailish's native AI lease on the
host.

## Two machines

On the host machine:

```sh
SUDEKIMP_LAN_ARENA_PORT=26770 \
  tools/continue-research.sh --lan-arena-host
```

On the client machine, replace the address with the host's LAN/VPN IPv4:

```sh
SUDEKIMP_LAN_ARENA_HOST=192.168.1.50 \
SUDEKIMP_LAN_ARENA_PORT=26770 \
  tools/continue-research.sh --lan-arena-client
```

Allow inbound UDP `26770` on the host firewall. Network discovery and
matchmaking are intentionally outside this milestone.

Press `Esc` to open Sudeki's native Quit menu. `MULTIPLAYER` is appended as a
new row after the shipped entries. Its sibling page releases the native pause
transaction so both processes continue rendering and consuming authoritative
snapshots while either player reads it; gameplay input is neutralized only in
the process that owns the page. The host page can start/end the session and
enter/leave native arena combat. The client page can join/leave and reports
combat as native world state synchronized by the shared simulation. It also
reports protocol, hash, build, map, role, token, sequence, malformed-packet,
authority, busy, and timeout failures.

## Same-machine validation

The harness creates isolated 32-bit Wine prefixes and keeps their registry,
user data, runtime logs, and save trees separate. Live host and client runs use
two ordinary windows; the harness never alternates physical X11 focus, so it
does not produce a rapid desktop/window flicker:

```sh
tools/lan-arena-loopback.sh --check
tools/lan-arena-loopback.sh --network-test
tools/lan-arena-loopback.sh --start
tools/lan-arena-loopback.sh --stop
```

For bounded local diagnostics, `tools/lan-arena-live-control.sh` resolves only
the exact named host/client windows. It can report state, request host combat,
exercise Tal's weak/strong/sweep/block inputs, hold client movement briefly,
move the client mouse, or capture both windows while recording each action in
`build/mingw32/lan-loopback/live-control.log`. It is an opt-in test tool and is
never started by either LAN profile. The host-combat commands use
`SudekiMP.LanArenaOperator.exe` and an auto-reset event in the host's isolated
Wine/NT namespace. Prefer the explicit, idempotent `--host-combat-on` and
`--host-combat-off` commands; the legacy `--host-combat` command remains a
manual toggle. The client never emits a combat toggle; it only mirrors the
authenticated host snapshot.

The same local operator API can exercise the current combat slice without
moving desktop focus or synthesizing cross-prefix mouse state:

```sh
tools/lan-arena-live-control.sh --status
tools/lan-arena-live-control.sh --host-combat-on
tools/lan-arena-live-control.sh --host-spirit 1
tools/lan-arena-live-control.sh --host-spirit 2
tools/lan-arena-live-control.sh --client-turn-right
tools/lan-arena-live-control.sh --client-fire-hold 3000
tools/lan-arena-live-control.sh --host-combo 450
tools/lan-arena-live-control.sh --host-sequence WWS 450
tools/lan-arena-live-control.sh --host-sequence WSW 450
tools/lan-arena-live-control.sh --host-strong
tools/lan-arena-live-control.sh --host-sweep
tools/lan-arena-live-control.sh --host-block 500
tools/lan-arena-live-control.sh --capture
```

The operator executable opens named, prefix-local Win32 events; it is not a
network service and cannot be reached by the LAN peer. The shell wrapper also
requires exactly one window with each role-specific title before it performs
any focus-sensitive action. `--host-combo` contains no intermediate image
capture, because a Wine screenshot can take long enough to miss Sudeki's
native second/third-hit timing window. Use `--host-combo-capture` only for
presentation snapshots, not timing acceptance. The current supported image
accepts all three native Tal stages across the verified 350–550 ms interval;
the command defaults to 450 ms when no interval is supplied.

The LAN socket pump runs on a synchronized transport-only worker. It never
reads or writes Sudeki game objects, and it keeps the handshake/keepalive alive
when either Wine window is unfocused. The harness does not contain a focus
pump. Sudeki's native `WM_KILLFOCUS` path normally calls
`ShowWindow(SW_MINIMIZE)`, and its `WM_ACTIVATE` path clears the application
active byte through both `WM_ACTIVATE` and `WM_ACTIVATEAPP`; the main loop then
skips updates/rendering and sleeps for 500 ms.
Focus loss also passes `false` through Sudeki's graphics-device registry,
which stops new frames from being presented even if simulation is allowed to
continue. The exact LAN-only window policy changes the show command to
`SW_SHOWNA`, keeps the application-active byte set, and preserves the canonical
`true` graphics-device activation state. Both processes therefore continue
simulation and presentation while only the foreground process receives
physical keyboard/mouse and DirectInput focus. The harness then places the two
ordinary windows on separate connected monitors, or tiles them on one monitor.
The harness titles them `Sudeki LAN Host - Tal` and
`Sudeki LAN Client - Ailish` so every live report identifies the exact
role/process. Wine may recreate a top-level window while D3D initializes, so
the harness resolves and titles both current HWNDs again after the client
warmup. Placement remains best-effort: `wmctrl` failures produce a warning and
never terminate an otherwise healthy LAN session.

All other launch profiles retain the original focus-loss behavior. The
same-machine harness has an explicit graphics backend selector:

- `wined3d` is the default and restores the known WineD3D runtime during every
  seed. Every seeded runtime DLL is copied to a same-directory temporary,
  checksum-verified, and atomically renamed over its target, so interruption
  cannot truncate the active baseline DLL. On this NVIDIA development host it
  currently advances both native simulations at about `0.2x` because two
  WineD3D/OpenGL contexts reach only about five frames per second.
- `software` injects Mesa software GL into the two game processes. It is the
  proven real-time diagnostic fallback, not an implicit global environment
  change.
- `dxvk` is an experimental acceptance path. It is pinned as one unit to the
  exact GE-Proton11-3 `wine`, `wineserver`, Wine Vulkan bridge, and verified
  i386 DXVK D3D9 DLL. Its preflight parses the NVIDIA i686 ICD's
  `library_path` and requires both that library and the Vulkan loader to be
  ELF32. The D3D9 replacement is staged and checksum-verified before an atomic
  install in each prefix. Every normal exit, failure, or interrupt restores
  WineD3D from a retained verified backup; DXVK never persists after the
  harness exits.

Select a non-default backend per invocation:

```sh
SUDEKIMP_LAN_GRAPHICS_BACKEND=software \
  tools/lan-arena-loopback.sh --start

SUDEKIMP_LAN_GRAPHICS_BACKEND=dxvk \
  tools/lan-arena-loopback.sh --start
```

Before a hardware-backed `--start`, the harness checks the current
systemd-logind session. If that active X11/Wayland session reports itself
locked, it warns that WineD3D/DXVK performance measurements may be
compositor-throttled while locked and continues launching. An unavailable
lock hint never blocks launch, and the Mesa software diagnostic does not emit
the hardware-performance warning.

Plain `tools/lan-arena-loopback.sh --stop` does not require the backend
variable used at startup. It tries a bounded, deduplicated list containing the
pinned GE-Proton11-3 wineserver, the ambient wineserver, and installed GE
siblings against both exact isolated prefixes.

Host and client DXVK logs and shader caches are isolated under
`build/mingw32/lan-loopback/dxvk/{host,client}`. Before a new live start can
truncate the console/runtime logs or replace a DXVK log, the harness copies
the previous evidence into a timestamped
`build/mingw32/lan-loopback/evidence/` directory. The first DXVK attempt is
therefore retained: its host log selected `NVIDIA GeForce RTX 3050
(610.43.3)` successfully, while the client failed later during LAN runtime
initialization. That attempt proves host Vulkan device selection, not yet a
two-process DXVK acceptance.

`--network-test` is headless and deterministic. It proves handshake, movement,
weak attack, authoritative snapshots, timeout/disconnect handling, and no save
mutation. `--start` launches both actual Sudeki processes for visual acceptance
and requires working host graphics for two concurrent Wine windows.
The LAN exact profile blocks native Previous/Next character rotation (including
F1-driven rotation) because v1 authority is fixed to Tal on the host and Ailish
on the client. Accepting a native switch would assign two authorities to one
actor.

## Scaling presentation to NPCs

Snapshots deliberately carry semantic presentation (`idle`, `idle variant 1`,
`idle variant 2`, `moving`, `action`, `incapacitated`) rather than renderer
selectors. NPC expansion therefore does not require a different packet format
for every model. Actors that share a skeleton/animation family can share one
verified presentation adapter. A genuinely different rig, locomotion graph, or
action layer needs its own exact selector/state/rate mapping and rollback test.
The network and interpolation work grows mostly with replicated actor count;
the reverse-engineering cost grows with the number of distinct animation
families, not simply the number of NPC instances.

## Acceptance boundary

The foundation is not complete online co-op. The next native authority slices
are damage/enemy-state convergence under a full live graphics run, followed by
host-routed skill, item, weapon, and Spirit actions. Client-side native actions
remain blocked until their host adapters and transaction arbitration are
verified; no client prediction is used in this version.
