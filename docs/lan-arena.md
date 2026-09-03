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

- Protocol/build: `LA16`, exact GOG executable hash only. Actor snapshots
  include a bounded four-edge action journal so rapid Tal combo stages are
  presented once instead of being collapsed by the 20 Hz snapshot cadence.
  The currently active semantic action also carries a 1/256-unit host phase;
  clients interpolate that phase instead of independently running a terminal
  attack clock.
- Roles: Tal host, Ailish client.
- Transport: direct IPv4 UDP, default port `26770`.
- Client actions: camera-relative movement and host-validated weak attack.
- Host snapshots: Tal, Ailish, the fixed training dummy, HP/SP, transforms,
  facing, presentation/combat state, and match state at 20 Hz.
- Presentation: each process uses a separate full-screen native camera and HUD.
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
  its verified idle. Ailish's first-person and world renderers are treated as
  distinct tables, and every world selector is resolved through the active
  actor-local animation bank before it may be written. Leaving combat likewise
  waits for the native sheath transition before world presentation resumes.
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
  clock into the LA16
  semantic snapshot. Clients interpolate and verify that phase on their own
  actor-local selector. The first host IDLE snapshot is the retirement edge;
  there is no client-only timeout or fixed crossfade deciding when an attack
  ends.
- Input acknowledgement is also simulation-owned in LA16. The socket layer
  may receive and coalesce packets, but it cannot acknowledge one merely for
  reaching the host process. The canonical reducer validates and admits the
  actor-scoped contribution first; only that admitted sequence may appear in
  a later snapshot. Tal and Ailish input histories are independent, while
  neither can contribute match, enemy, resource, or native combat state.
- Match lifecycle is one-way within a session token: waiting may become
  active or ended, active may become ended, and ended cannot reactivate.
  Waiting/ended frames must carry combat disabled. Starting again requires a
  fresh handshake and token rather than reviving stale shared-world state.
- The client may open and browse Ailish's native QuickMenu. Native confirm/use
  commands are consumed locally until category-specific requests can be
  validated and executed by the host. Skills, items, weapons, Spirit,
  save/load, transitions, dialogue, shops, and loot remain non-authoritative
  or blocked in this slice.

Packets are versioned and sequenced. The `LA16` handshake validates the exact
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
the exact named host/client windows. It can report state, request client combat,
exercise Tal's weak/strong/sweep/block inputs, hold client movement briefly,
move the client mouse, or capture both windows while recording each action in
`build/mingw32/lan-loopback/live-control.log`. It is an opt-in test tool and is
never started by either LAN profile. The client-combat command uses
`SudekiMP.LanArenaOperator.exe` and an auto-reset event in that client's
isolated Wine/NT namespace. It therefore cannot leak an F8 edge into the host
or depend on desktop-global Wine key state.

The same local operator API can exercise the current combat slice without
moving desktop focus or synthesizing cross-prefix mouse state:

```sh
tools/lan-arena-live-control.sh --status
tools/lan-arena-live-control.sh --client-combat
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
role/process.
All other launch profiles retain the original focus-loss behavior. On this NVIDIA
development host, two concurrent WineD3D contexts require Mesa software GL:

```sh
__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 \
  tools/lan-arena-loopback.sh --start
```

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
