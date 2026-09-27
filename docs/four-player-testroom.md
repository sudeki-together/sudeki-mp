# Fixed four-player testroom

Owner-selected milestone: one Buki listen host, with Elco, Tal and Ailish
connecting as three independently owned clients. No dedicated server,
character switching, campaign support or server browser in this milestone.
Acceptance requires four game windows and verified gameplay, not connection
logs alone.

## Current evidence and boundary

`CONFIRMED_TEST`, dirty working tree based on `2e10c9c`: a fresh 32-bit MinGW
build completed for the DLL and focused test executables. The exact-image
fixture passed against the supported `SUDEKI.exe` identity
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`; the
three-client UDP/session test passed with independent rejoin. These checks do
not launch Sudeki. The transport, roster, native control and four-actor
movement presentation now link into an explicitly opt-in private runtime.
Four-window gameplay and cleanup acceptance remain pending.

## Current source delta (2026-09-27; live gameplay unverified)

The private source path now extends the host coordinator and four-actor
replica to combat mode. On the host, remote weak-attack edges enter the exact
leased actor's native combat arbiter; ranged held fire is paced there, and
weapon changes use that actor's native inventory activation. Host snapshots
capture actor-specific combat motion/action state, Elco reload observation,
aim vectors/targets, and the host training dummy's health/hit journal. SMP4
now connects the existing Elco aim hooks: Buki's host validates fresh input,
combat/weapon readiness, and an actor-local retained lease before applying
projectile direction; clients use a fresh host frame for Elco's authored pose
and cannot create projectile authority. Ailish's existing v3 3D camera vector
reaches the host's actor-local facing path and the host now uses a validated
host-derived ray point to correct her native projectile direction. Her
first-person renderer now consumes host-confirmed fire/idle and weapon-swap
snapshots only on local seat 3. Its cosmetic lease is keyed to the exact actor,
component, wrapper, renderer, token and generation; it submits no native
client combat action and returns the channel to idle on combat exit, drained
disconnect/rejoin and teardown. Ailish ammo/reload timing remains absent from
the Elco-only v3 weapon state. These routes are build-verified only;
four-window camera and animation convergence remain unverified. Elco's own
client window reconciles its charge/reload display through the existing
local-controller-checked adapter. Clients sync the host's combat-mode bit and
apply actor-local combat presentation plus host-confirmed dummy feedback. The
shared native drain gate now also waits for an actor's pending weapon swap
and action terminal before releasing its borrowed-control lease. Client action
retirement checks the exact actor's own applied renderer and accepts only a
known terminal selector/state while the lease is retained in `DRAINING` after
quiesce. Elco/Ailish fire snapshots now read their fifth native action channel
through an exact-entity reader; the common four-channel locomotion observation
does not contain that event. The host held-fire pacing guard now uses the same
exact-actor channel reader, so an active native shot cannot be treated as idle
because the four-channel record has no fifth value.
On a validated host combat-to-movement frame, replica playback retires the
exact Elco/Ailish fifth firing channel and clears its action lease; melee's
primary channel is replaced by the validated noncombat locomotion frame.
This exit path is compiled but has not been exercised against a live renderer.
Combat playback also preserves the prior action sequence before updating its
presentation lease, so a repeated same-selector shot or attack restarts from
the beginning. If a selector write fails partway through, the next valid frame
retains that restart request. This correction is source- and build-verified
only.
Only the Buki-host runtime installs the shared damage/popup observers, gated by
an active peer and a retained native lease, to populate that hit journal.

The later continuation pass added an Ailish-specific held-fire readiness gate:
the Buki host resolves Ailish's exact leased actor `CMissileManager` and uses
the supported image's read-only `CanFire` and `IsFiring` predicates alongside
the existing pacing guard. This is source/build verified only; the cached
manager is lease-bound and its native owner pointer is checked at use.

The dirty source built with the locally installed MinGW SDK in a separate
temporary build directory; the resulting DLL SHA256 is
`ca467f2ae0777940cdd0f27baf9599fbeaaeadcaae5a3d38500601bc3cb05cdf`.
`SudekiMP.CleanroomEngineTest` passed;
`SudekiMP.SkillTraceImageTest` passed, including
`lan_party_control_image_fixture: PASS (inert native calls)`; and
`SudekiMP.LanPartySessionTest` passed for three simultaneous UDP clients,
four-actor frame routing and independent rejoin. These tests cover bounded
engine storage, supported-image install/restore, routing, and policy fixtures;
they do not exercise the new drain against a live actor or any of the four game
windows. No gameplay inputs or process restarts have been issued. Strong
attacks, block and dodge have no request fields in the current SMP4 input
payload; adding them requires an explicitly versioned protocol change. Skills
and Spirit stay outside this basic slice.
After those passing offline suites, the Ailish native cadence gate was added
and the DLL was rebuilt successfully. The focused suites were not rerun against
that latest DLL. Detailed build identity, Graphify counts, and bounded local
checks are recorded in `.agent-local/active-session.md`. The latest read-only
window/process query found no Sudeki windows or game process; no restart or
scripted input was issued.
The subsequent Ailish host projectile-direction witness also builds against
the unchanged v3 wire contract; its geometry and live trajectory have not been
visually verified.
The latest compile-only DLL also includes local Ailish first-person fire/idle
and weapon-swap playback with generation-bound retirement. Its reload timing,
live visuals, and interaction with host-confirmed dummy damage have not been
verified in the four windows. DLL SHA256:
`de287be9a562d6977f616023f216a872911edd995ed745ddb526fc7085bc55a4`.
The movement-only statements below describe the previous source boundary and
are superseded only for this unverified subset; they are not gameplay evidence.

The 2026-09-27 continuation connects optional basic-combat input and Ailish
reload presentation end to end. SMP4 v3 negotiates additive message kinds
while leaving the existing envelope payloads and nested LA42 packets intact;
older v3 peers keep the existing weak/ranged subset. New clients send Tal
strong/sweep edges and held block state. Buki's host submits them through the
exact leased actor arbiter and releases block on input expiry or lease
retirement. Client native combat transitions remain suppressed locally. The
host sends Ailish's observed selected item and reload timer in a separate
per-frame sidecar; replica sampling retains it, and the seat-3 renderer lease
uses her first-person C2/C3 reload selectors. The loader scope is now
`basic-combat`. This continuation is compile-only: no game was restarted and
no controls were sent. Tal's native dodge trigger is still untraced; Buki's
local dodge remains on her existing native host path. Four-window visual
acceptance is open.

The existing LA42 Buki/Elco profile remains the regression baseline. Its byte
format is unchanged. Explicit-context codec functions replace dependence on
the process-global pair only for new callers; old callers keep their wrappers.

## Transport contract

- Fixed player seats: 0 Buki/canonical world, 1 Elco, 2 Tal, 3 Ailish.
- Separate `SMP4` envelope, draft version 3, retaining the bounded LA42 actor
  serialization. The build and embedded codec version are checked. A compile
  assertion requires revisiting the envelope if that codec changes.
- Each client requests a fixed seat. The host validates build, game hash and
  map, issues a random token and increasing seat generation, then waits for
  the client to prove receipt. Duplicate handshakes are idempotent; another
  claimant cannot replace an occupied or draining seat.
- Successful network negotiation leaves a seat PENDING. Only the game-thread
  coordinator may approve it after acquiring an exact native actor lease.
- Inputs are source-, token-, generation-, sequence- and actor-checked. Edges
  coalesce independently per peer. Receipt is not execution: acknowledgement
  advances only after the coordinator admits a taken, still-fresh input.
  Input expiry is 250 ms, independently of transport heartbeat health.
- Four actors form one canonical frame split into two bounded UDP chunks:
  Buki/Elco and Tal/Ailish. Chunk-local presentation indices do not assign
  network authority. World/dummy feedback is carried once, in chunk zero.
  Complete VFX rosters belong to their respective chunks.
- Each chunk preserves existing animation histories, aim, shot/resource state,
  and cast presentation fields. Four bounded assembly slots tolerate reordered
  chunks. Only a complete, consistent, newer canonical tick enters the
  eight-frame receive queue. Loss never fabricates an empty actor/effect roster.
- END or timeout closes only the affected peer's admission and enters DRAINING.
  The transport cannot declare native cleanup complete. The game-thread owner
  must positively drain tasks and release control before releasing the slot.
  Rejoin obtains a new token/generation; old input cannot control the actor.

## Reproducible tests

Build with `tools/build-linux.sh` (or the existing Windows CMake workflow), then
run `build/mingw32/bin/SudekiMP.LanPartySessionTest.exe` on Windows or Wine.
Tests create ephemeral loopback sockets; no live game instance is required.

Coverage includes three simultaneous peers; pending-vs-approved admission;
independent input latching and acknowledgements; all-four-actor fanout;
duplicate character requests; mismatched hash/version; wrong-actor and
wrong-source datagrams; stale generations/tokens; input expiry; duplicate and
reordered snapshots; incomplete/mixed-tick frames; bounded queue overflow;
isolated timeout/disconnect; delayed native-drain release; fresh reconnect;
and host END. Tests bypass sender-side helpers for adversarial receive checks.

The existing protocol, session, shared-simulation, authority, replica, multicast,
aim, runtime-hook, hit-feedback, damage-guard, skill/Spirit ABI, combat-input,
cleanroom, owner-view, window-policy, profile, combo, Buki-native and exact-image
regression suites are separate evidence. Their passing does not establish
four-player gameplay acceptance.

## Native control integration boundary

`CONFIRMED_TEST` / `CONFIRMED_EXACT_IMAGE`, dirty work based on `2e10c9c`:
`lan_party_control.h` and its implementation in the existing control adapter
provide three independent generation-bound AI leases. They require the borrowed
service-post-original controller witness, a Buki-controlled four-member group,
fresh actor/component ownership, and exact native acquire/default/movement
entry bytes. They do not borrow the legacy companion slot or a split-screen
camera. Existing local-seat dependencies and legacy two-player routes remain
unchanged.

`src/hooks/lan_party_host_control.c` connects that adapter to the three-client
transport queue on the game thread. Pending peers are approved only after
native acquisition. Held movement is serviced independently, expired input
stops that actor, and disconnect closes that peer's admission before waiting
for positive native drain. Rejoining obtains a fresh generation. Another peer
need not release its actor. Uninstall refuses to discard retained native
leases; a consumed native release reference is not decremented twice.

At the preceding movement-only checkpoint, this route handled **ordinary
locomotion only**. The current unverified source delta above adds weak attack,
ranged held fire, host-validated weapon changes, combat snapshots and
combat-mode synchronization. Strong attack, block, dodge, skills and Spirit
remain unsupported. The private runtime supplies a
positive native drain probe: exact actor, inactive CSkill, no skill task, no
ranged prime, and positively idle global Spirit state. It creates no casts or
camera/effect tasks. This conservative proof is not the future concurrent-cast
cleanup policy, and there is no timeout substitute for native completion.

The added `tests/lan_party_control_image_fixture.c`, run by
`SudekiMP.SkillTraceImageTest`, checks the actual mapped executable's entries
and existing controller wrapper. Three real UDP client endpoints pass movement
through that wrapper to three distinct **inert actor fixtures**. AI acquisition,
release and movement are recording stubs; native `SetSpeedImmediate` is exercised
in mapped memory. Tests cover ownership mismatches, copied/escaped witnesses,
independent acquisition order, duplicate acquisition, stale generations,
input expiry/clock races, unsupported-action rejection, disconnect/drain/rejoin,
fresh host-session generation reset, failed and partially verified release,
and retained-lease uninstall rejection.
They are not evidence that three native companions move or animate in a game.

Full build and 21 regression suites, including the updated exact-image test,
pass at this boundary. Existing regression assertions were retained.

## Host roster startup integration

`CONFIRMED_TEST`, dirty work based on `2e10c9c`:
`lan_party_roster.c` now runs from `SudekiMpLanPartyHostControlService`, before
peer approval. It binds Buki's original group/controller and room anchor,
observes all present retail members, and requests missing Elco/Tal/Ailish
through the existing asynchronous cleanroom spawn API. Only one spawn is
outstanding at a time. A return from `InternalSpawnPC` means submitted, not
completed; repeated callbacks never retry a submitted spawn on a timer.

Actor initialization reuses the accepted actor-family weapon helpers and
cleanroom initializer, including Buki's/Elco's testroom weapon grants. Existing
equipped weapons and valid resources are not deliberately reset. This setup
runs once per observed actor in this host session, not on every peer reconnect.
The production boundary requires testroom/Buki launch options, the supported
initialized engine/skill/weapon adapters, a borrowed service-post-original
witness, current group/controller/actor identity, no startup combat, and
positively inactive observed native skill/Spirit work before mutations.

All three peers remain PENDING until all four actors are present and initialized
and their separate control-acquire probes pass. Unknown roster observations
suspend admission and movement acknowledgement. A positively changed group,
controller or previously observed actor closes this host session's admission;
it does not silently respawn or retarget an owned character. Existing control
leases remain retained until their original native identities can be safely
released. Spawned characters remain engine-owned party members; shutdown does
not invent native spawn cancellation or despawn them.

The exact-image fixture exercises this integrated path with three real UDP
clients and **inert spawn/equipment callbacks**: delayed completion over 100
controller callbacks, pending joins, duplicate group members, failed initializer
retry, wrong controller, escaped dispatch witness, combat startup deferral,
fixed initial spawn anchor, all-four readiness, unchanged setup on reconnect,
unknown-world input suspension, and roster replacement with retained cleanup.
Build and all 21 regression suites pass. This verifies coordinator ordering and
identity checks, **not live native spawning, equipment or four-window gameplay**.

## Private movement runtime

The loader accepts `[FourPlayerTest] Enabled=true` only with the explicit
`Scope=basic-combat`, a fixed seat and validated endpoint settings, after the
unchanged supported-executable checks. This path does not install the legacy
two-player coordinator. Native startup requires the matching testroom/local
hero command line. The socket worker reads plain transport data only; the
existing service-only controller observer owns native roster, input and
presentation work.

Each client prepares the four-character roster with its own actor in the
native controller/front slot. It acquires presentation-only AI leases on the
other three actors. Only the host calls their movement/authority route. Native
input capture resolves the immutable local actor rather than changing the old
global pair. Unknown/replaced actors or incomplete frames close admission.

`lan_party_replica.c` uses one interpolation clock for both chunks, atomically
admits complete four-actor frames, and resets both histories on a discontinuity.
`lan_party_motion.c` defines character-keyed noncombat banks. SMP4 v2 carries
host phases, state and blends through the bounded locomotion payload with a
separate closed noncombat clip namespace. The explicit codec context and
sampler preserve these records for all four actors; legacy LA42 rejects them
outside combat as before. Native playback verifies loaded clips on transition,
revalidates owners around mutation, and reuses the bounded phase/rate correction
instead of restarting a clip on every snapshot. Unsupported native selectors
reject the frame, never fall back to fabricated idle.

Tests additionally cover all three native client-owner layouts, retained leases
through disconnect, complete frame admission, common-clock interpolation,
phase epochs and loop fences, invalid/missing motion, matching codec round trips,
legacy context rejection, and exact-image input/replica install/restoration.
These checks do not establish complete visual gameplay acceptance.

## Remaining integration

1. Finish and accept the private four-window movement path, including native
   animation quality, focus behavior and independent disconnect/rejoin.
2. Verify the current source in four windows, including each actor's
   melee/ranged behavior, combat entry/exit, weapon selection/reload, host-only
   dummy damage, positive action drain, and independent disconnect/rejoin.
   The additive basic input extension covers Tal strong/sweep and block;
   trace Tal's native dodge trigger before adding it. Skills and Spirit follow
   this basic slice. Reuse canonical lifecycle validation before applying
   frames.
3. Promote only after matching-build four-window gameplay checks: movement,
   combat/idle transitions, attacks, weapons, skills/strikes, effects, dummy
   feedback and disconnect/rejoin. Preserve all accepted character corrections.

Do not enable this profile merely because the socket tests pass. Do not swap
process-global actor identities between peers to bypass native ownership.
