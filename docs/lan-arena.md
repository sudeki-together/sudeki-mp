# LAN arena development guide

The LAN arena is a closed, host-authoritative two-process experiment. The host
controls Tal and owns simulation, AI, damage, resources, the training dummy,
and all native action execution. The client controls Ailish and renders
authenticated host snapshots through its own full-screen Sudeki process.

This profile never reads, writes, copies, or transfers campaign saves. It
always launches Sudeki's cleanroom `testroom`, and it does not share hooks or
state with the local split-screen, fixed-three, roster, or custom QuickMenu
experiments.

## Current playable slice

- Protocol/build: `LAN4`, exact GOG executable hash only.
- Roles: Tal host, Ailish client.
- Transport: direct IPv4 UDP, default port `26770`.
- Client actions: camera-relative movement and weak attack.
- Host snapshots: Tal, Ailish, the fixed training dummy, HP/SP, transforms,
  facing, presentation/combat state, and match state at 20 Hz.
- Presentation: each process uses a separate full-screen native camera and HUD.
- Replica locomotion: continuous movement retains a 25 ms interpolation
  buffer. A moving-to-idle edge consumes that final buffered distance in its
  run pose instead of snapping or sliding. Tal's two verified native idle
  variants and Ailish's verified selector-4/selector-5 idle variants cross the
  wire as actor-neutral semantic states. Each client maps those semantics back
  to its actor-local native clips; native selector numbers never cross the
  network. A generic Ailish idle also suppresses client-only fidgets, while her
  renderer still owns animation-clock and blend progression inside the
  authenticated semantic clip.
- Client native QuickMenu, skills, items, Spirit, save/load, transitions,
  dialogue, shops, and loot are blocked in this slice.

Packets are versioned and sequenced. The handshake validates the exact game
hash, mod build, cleanroom map, fixed Tal/Ailish role tuple, and a fresh session
token. Stale, duplicate, malformed, mismatched, or wrong-authority packets are
rejected. A timeout ends the session; it never silently reconnects into the old
session. Cleanup stops inputs, discards client replicas, and restores Ailish's
native AI lease on the host.

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

Press `Esc` to expose Sudeki's unchanged native pause layer with the mod-owned
`MULTIPLAYER` status panel over it. The host uses `F9` to host and `F10` to end
the session. The client may type an `IP[:port]`, use `F9` to join, and `F10` to
leave. The panel reports protocol, hash, build, map, role, token, sequence,
malformed-packet, authority, busy, and timeout failures.

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
