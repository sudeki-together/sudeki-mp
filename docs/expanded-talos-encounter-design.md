# Expanded Talos encounter design

## Status

The current default-off research profile is a live-proven two-human/four-hero
Talos prototype. It deliberately leaves the retail final transition alone:
Sudeki removes Buki, Ailish, and Elco, runs the temporary Kazel lifecycle, and
finishes the FMA07/TSA handoff. Only after an exact same-process Kazel-delete
and post-movie settle ticket does the mod respawn Ailish, Buki, and Elco.
Kazel is gone before restoration, so the native four-slot party never contains
a fifth actor.

This is the architectural pivot from the earlier carry-through investigation.
After roughly six hours of pursuing pre-cutscene companion preservation, the
evidence showed that the extra ownership, capacity, and SOL-continuation risk
was unnecessary for the playable goal. The earlier attempt that spawned on an
early Kazel signal was unsafe because its trigger preceded the exact delete and
TSA settle; post-movie spawning itself was not the defect. The replacement uses
a one-shot lifecycle-owned ticket at the proven quiet boundary.

The live checkpoint restores the exact group and formation to four heroes,
keeps Tal as Player 1, claims Ailish as Player 2, and leaves Buki and Elco under
native AI. A strict runtime gate then enables a left/right compositor only
after the restore is exact: Tal is on the left and Ailish is on the right.
Ailish movement is transformed through the orientation basis of her presented
view. The camera contract is seat-local so later human seats can receive their
own view/input bases, but the production runtime is still only P1/P2; P3/P4
cameras and controls are not implemented.

Use the two closed launch profiles:

```bash
./tools/continue-research.sh --talos-post-movie-party-test
./tools/continue-research.sh --talos-post-movie-dual-camera-test
```

The second profile adds only split rendering, the render-only P2 camera, the
alternating two-frame cache, and camera-relative P2 navigation. Independent
Ailish right-stick camera rotation is the next milestone and remains disabled
in this checkpoint.

## Long-term product contract

- The encounter roster is always Tal, Ailish, Buki, and Elco.
- Tal remains Player 1 and native host/leader.
- Connected controllers may own Ailish, Buki, and Elco as Players 2 through 4.
  Every unassigned hero remains native AI.
- Only human seats receive viewports: full screen for one, left/right for two,
  top-wide plus two bottom halves for three, and a 2x2 grid for four.
- Talos maximum HP is `45,000 x 4 = 180,000`, applied and verified once after
  the native boss combat object and boss-bar binding are both valid.
- The host confirms the immutable assignment before the Void transition.
  Cancel returns to the current world without starting the load.
- Native cinematics, Talos teleports, and uncertain global-camera ownership use
  one full-width native presentation. Split rendering resumes only after fresh
  per-seat frames exist.

## Companion combat policy

AI companions are not commanded to attack one hard-coded boss pointer. The
integration layer may only prove that their native targeting system is alive
and that a non-null selected target is a verified member of the Talos encounter
(the real boss or one of its authored clones). A null target is allowed while
the native AI is idle or reacquiring.

Target identity is deliberately excluded from the immutable admission ticket.
That lets Sudeki retarget a companion between the real boss and clones using
its own authored state. Previous live research observed that switching
behavior. Static selectors confirm that proximity materially participates:
candidate queries radius-filter by squared distance, then ordinary and combat
selection choose the smallest distance inside a winning priority/forward-near
bucket. Authored request modes can choose a different bucket, so this is not a
guarantee that every decision selects the globally closest Talos. We also have
not proved that taking a hit itself forces a retarget. Neither stronger rule is
imposed or advertised.

If the real type-3 Talos remains excluded by the authored candidate-filter bit,
the eventual adapter may broaden eligibility using the previously observed
temporary-request-copy seam. It must not write a target pointer or persistently
change faction/AI state.

The exact authored threat set is real `BOSS_TALOS` (resource ID 447, AI type
3) plus `BOSS_TALOS_FAKE` and `BOSS_TALOS_FAKE2` (IDs 317/318, AI type 1).
The clones are naturally eligible. For an exact retained companion evaluating
only the exact real-Talos candidate, the existing narrow adapter may copy the
0x28-byte request, clear candidate-filter bit `0x04` at copy offset `0x25`,
invoke the native validator, and discard the copy. It may not alter the live
request, factions, or selected-target state.

## Implemented inert components

- `TalosEncounterSession`: immutable world/source/host lease, all four existing
  hero actor identities and lifecycle generations, encounter serial,
  hero-to-seat/AI assignment, controller/input identity and generation, one-shot
  confirmation and transition claim, stale/replay rejection, and quarantine.
- `TalosEncounterAdmission`: four distinct native heroes, exact control/AI
  ownership, native targeter readiness, boss/combat/boss-bar identity, bounded
  pre-commit abandonment, and one verified HP ticket.
- `TalosCompanionCarry`: an inert transaction model binding the immutable
  encounter to the exact Buki/Ailish/Elco `DeletePC` sequence, the one allowed
  `SetZone("Void")`, one verified native formation-pop effect, and verified
  empty final teardown. Before the first omitted delete it can abort to the
  untouched vanilla path; after the first omission, any uncertainty is
  quarantined and the zone load is blocked rather than guessed through.
- `LocalViewportLayout`: exact one-to-four-seat pixel layouts with stable seat
  ordering and no overlap or uncovered pixels.
- `LocalInputHub`: opt-in stable controller-slot reservations with reconnect
  generations and neutral input fences. It has no production start caller.
- A pure adaptive-render activation gate requiring exact actor, camera, render,
  HUD, input, and cache coverage for every active human seat.
- A passive transition-lineage model. Production continuation is hard-disabled
  until its exact live SOL task ancestry is observed.
- A separate pointer-free, one-attempt ordinary-world staging coordinator. It
  emits only symbolic public-remove/public-add tickets, grants no production,
  carry, or actor-lifetime authority, and has strict replay, drift, quarantine,
  and terminal reload tests.
- A pure exact-build membership-ABI v3 seam validator, a service-only native
  controller-update observer path, and an immutable two-view native capture
  and sampler bridge. They can publish one pointer-free ordinary-world
  observation, but none calls a membership seam or enables staging mutation.

## Historical observation-only live profile

The sections below preserve the evidence path that led to the post-movie
restore. They are not the current playable launch instructions and do not
override the status above.

The only live entry point for the next evidence pass is:

```bash
./tools/continue-research.sh --talos-lifecycle-observation
```

The Linux beta launcher exposes the same mode as **Talos lifecycle
observation**. This is a research trace, not the expanded encounter. Before it
builds or launches, the command requires both supported GOG images:

| Image | Required SHA256 |
|---|---|
| `SUDEKI.exe` | `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94` |
| `Data/SOLWORLDM.gex` | `e36a5974f9aedea5b5b428fe2445cf496c52911ff01d4934ea8ab8124abf1ff9` |

The generated configuration is closed rather than additive. It sets every
`Enable*` option to `false`, then enables only
`EnableExpandedTalosLifecycleTrace=true`; it also sets
`SkipStartupMovies=false` and enables the existing passive C-level zone trace.
This guarantees that the retired Talos restoration, expanded encounter,
co-op roster, controller sources, party-atomic transitions, votes, split
renderer, skills, merchant/blacksmith experiments, traversal, and test boosts
cannot join the run. The launcher restores the ordinary generated INI after
Sudeki exits.

The lifecycle adapter is observation-only. It may defensively copy values into
its own log records, but every SOL/native call must execute once with its
original arguments, ordering, result, and return edge. It may not show a setup
prompt, suppress or replay `DeletePC`, defer or replay `SetZone`, preserve or
spawn a companion, change Talos HP, claim an actor/input/camera, or alter final
cleanup. If the loader does not report the lifecycle trace as installed, the
run is not evidence and must not be used to relax any gate.

The current observer covers exact opcode-29/opcode-27 entry and return edges,
the `LoadTheVoid` task-constructor return with opaque task/thread generations,
nested `SetZoneNOW` before/after edges, native `DeletePC` before/after edges
with copied `ResourceName` kind/identifier/bounded backing-name evidence, and
native `RemoveAllPlayers` before/after samples of
the group and AI formation. It also records native
`AiPCFormationPopMembers` before/after samples, `TSAIsPlaying` at each exact
opcode edge, and the exact nested `TSASetPlaying(false)` native falling edge
that follows authored control/gamepad restoration. Each roster sample includes
per-run opaque member tokens and an identity-set equality result. The corrected
observer also classifies Tal, Ailish, Buki, and Elco without an engine call by
comparing each actor's three exact relocated vtables, then corroborates that
classification when the named `ResourceName` and the same token disappear
from both native rosters across the authored `4 -> 3 -> 2 -> 1` sequence.
These facts correlate a run without exposing raw addresses, but the resulting
roster lease generations are not authoritative allocation generations.
`remove_all_verified_empty=true` means both roster samples were readable and
both copied count/mask pairs were empty; it is not proof of actor identity
continuity by itself. After the exact Void `SetZoneNOW` return, the existing
`SetRenderCamera` wrapper also supplies borrowed, read-only evidence to the
lifecycle observer. It accepts only a committed global `default` camera whose
selected render state matches the scene renderer, invalidates that evidence
on a later successful unproven camera change, and revalidates it at the exact
TSA falling edge. That same edge requires the original surviving Tal to be the
native controller target in gameplay modes with native AI and override control
inactive. The observer still does not claim stable actor-allocation generation
equivalence or frame-cache freshness. Those remain blockers regardless of how
clean this trace looks.

The observer pins its DLL before the first patch. Installation and rollback are
supported only during the launcher's suspended-startup transaction; arbitrary
runtime enable/disable or dynamic unload is outside the contract.

Before treating a log as evidence, require both exact records:

```text
talos_lifecycle_trace_install=success
expanded_talos_lifecycle_trace_applied=true
```

Use this exact first-run sequence:

1. Close every existing `SUDEKI.exe` process. Use one human player with
   keyboard and mouse; do not start a co-op roster or controller bridge.
2. Start the profile and load a known save immediately before the final Talos
   handoff. Before activating it, visually confirm that Tal leads the native
   four-hero party.
3. Move Tal once and wait for an ordinary stable gameplay frame. If Tal is not
   lead, all four heroes are not present, loading is active, or a cinematic is
   already running, exit normally without triggering the handoff.
4. Activate the authored final interaction as Tal. Do not switch characters,
   open a menu, pause, fast-forward, press a controller button, or skip FMA07.
5. Let retail Sudeki remove Buki, Ailish, and Elco and enter the Void normally.
   Companion preservation/restoration, a mod prompt, split presentation, or an
   HP change is a failed observation.
6. Once Tal reaches a stable playable Void frame, move once. For at least one
   run, complete or exit through the authored ending so the exact count-generic
   `RemoveAllPlayers` boundary can be verified. Preserve `SudekiMP.log`; do not
   overwrite it before review.
7. Complete two independent unskipped runs with the corrected observer. Require
   the same semantic task/delete/resource/hero/TSA sequence rather than equal
   process-specific tokens. The log intentionally exposes no raw addresses and
   does not claim allocator-lifetime authority.

Any exact-image/signature/install failure, missing log return edge, reordered or
extra companion delete, wrong resource, stale/reused task generation, R6025, or
unreadable/incomplete `RemoveAllPlayers` samples fails the run. A visible camera
or actor-lifecycle anomaly also fails the run, but a visually clean result does
not prove either unobserved contract. Before the handoff, stop by exiting
normally. After native transition work has started, the observer must never
interject: allow retail Sudeki to finish, mark the trace failed, and begin any
retry in a new process.

## Historical transition evidence and carry-through blockers

The supported GOG script data identifies the authored chain as
`CC_NPC_Caprine_TalkingT3|PP -> LoadTheVoid -> SetZone|S -> SetZoneNOW|S`.
`LoadTheVoid` contains the FMA07/Void sequence and three exact companion
`DeletePC|R` calls: Buki at raw/logical `0x497B9/0x2194D`, Ailish at
`0x497C4/0x21958`, and Elco at `0x497CF/0x21963`. The following `SetZone|S`
opcode begins at raw/logical `0x497DA/0x2196E`.

The candidate carry-through seam is to omit only those three calls while the
exact task lineage, SOL resource literals `4/3/5`, native PC identifier/name
evidence, four-actor group, and AI formation all still match the immutable
prompt. It must never intercept `DeletePC` broadly:
the same transition invokes `TalKazelMerge`, whose separate PC_KAZEL lifecycle
spawns and later deletes resource 15. After the first omitted delete, a clean
vanilla fallback is no longer guaranteed, so every prerequisite must pass
before any omission.

The first passive run also exposed a harder capacity constraint. After the
retail companion removals and Void entry, TalKazelMerge temporarily placed Tal
and `PC_KAZEL` in both the four-slot active group and four-slot AI formation,
then its out-of-scope `DeletePC` returned both structures to Tal-only. Keeping
all four heroes resident in either structure would therefore leave no slot for
that authored Kazel step. Static exact-build analysis shows that this is not a
safe capacity probe: the raw group-add core has no `count == 4` rejection and
would write through the fifth slot at `group+0xC0` before increasing the count
to five, while the paired formation-add core does reject a fifth member. A
live full-roster probe is forbidden.

The passive observer therefore adds no SpawnPC or listener mutation. It binds
the exact ordered `TalKazelMerge -> SpawnPC -> InternalSpawnPC` SOL opcode
before/after edges, classifies the transient `DarkTalEntity` by its complete
relocated vtable/RTTI
triplet, and wraps only the exact relative call at RVA `0x000B15DB` into the
raw group-add core. The ABI bridge calls the original once and records the
group and formation before/after sets; the existing exact `DeletePC(PC_KAZEL)`
edge must remove that same opaque token. The observer accepts only one such
final-battle lifecycle per process; later LoadVoid sessions are quarantined to
prevent stale asynchronous completion attribution. A corrected passive run
proved the retail `1 -> 2 -> 1` sequence and same-token insertion/removal.
Omitting the three companion deletes remains blocked until a native
ownership-safe staging/reattachment lifecycle is separately proven.

The selected capacity design does not suppress Kazel's raw add. Immediately
before that authored add, it will temporarily remove the exact group-last
nonlead hero through `CGroupPlayers::RemovePlayer` (RVA `0x00023390`). The
native group listener must synchronously reduce both group and formation from
four to three. Kazel then keeps every retail listener and completion callback
through `3 -> 4 -> 3`. Only after the exact Kazel delete may the same live hero
be restored through `CGroupPlayers::AddPlayer` (RVA `0x00023230`). Group and
formation orders are captured independently: the proven baseline is
`[Tal,Ailish,Buki,Elco]` versus `[Tal,Elco,Ailish,Buki]`, so Elco is group-last
but not formation-last. Acceptance requires native canonicalization to restore
both original ordered arrays exactly. A set-only match is insufficient.

This path is still mutation-blocked. Public remove/add calls are void, their
formation-listener result is not propagated, and the roster's intrusive
`TPtr` records are liveness observers rather than lifetime owners. Before any
disposable ordinary-world remove/re-add can even be considered, the default-off
observation profile below must first capture the exact four-hero, one-human
side-effect closure. A valid observation still would not authorize either
membership call or prove that Elco survives one. Any later mutation proof would
require a separate decision and must not be combined with Void,
companion-delete suppression, combat, a door, or a save.

### Ordinary-world staging groundwork

The pointer-free research coordinator for that proof is implemented and
covered by strict host tests. It permits one process-lifetime attempt and
requires the exact independent baseline orders
`[Tal,Ailish,Buki,Elco]` and `[Tal,Elco,Ailish,Buki]`. Its preflight,
detached, restored, and immediate-stability snapshots bind the process,
native thread, source, world, group, formation owner, formation, controller,
Tal front actor, camera, selected render camera, render state, scene manager,
and scene renderer. Any post-remove identity, order, quiescence, or authority
drift quarantines the attempt and requires a reload. Camera and render tokens
are continuity evidence only; they do not prove that a camera targets Elco,
Tal, or any other actor.

The separate membership-ABI v3 validator purely checks the supported PE32
seam bytes, relocated operands, relative-call targets, calling conventions,
public membership wrappers, formation and listener paths, the intrusive
`TPtr`/wrapper registry and destructor machinery, HUD and stat-display update
paths, Elco's arbiter path, and the stat-display camera-sync path. A successful
`seams_valid` result is not a whole-image identity check. The closed loader
profile separately authenticates the complete files against these SHA256
values:

| Image | Required SHA256 |
|---|---|
| `SUDEKI.exe` | `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94` |
| `Data/SOLWORLDM.gex` | `e36a5974f9aedea5b5b428fe2445cf496c52911ff01d4934ea8ab8124abf1ff9` |

Neither the validator nor either authenticated-file fact grants native-call,
mutation, or actor-lifetime authority.

Static exact-build evidence identifies `GetPC`'s returned GELPointer/PtrObj as
a `0x18`-byte wrapper with a `0x0C`-byte embedded weak `TPtr` at `+0x0C`.
The planned synchronous transaction keeps that same wrapper across the one
public `CGroupPlayers::RemovePlayer` and one public
`CGroupPlayers::AddPlayer` call, then invokes its scalar deleting destructor
exactly once and clears the local wrapper reference. The weak `TPtr` does not
keep Elco alive, so unchanged wrapper and actor observations remain required
after each public call. Direct calls to the raw add/remove cores are forbidden;
their validated control flow is ABI context for the public listener routes,
not an alternate mutation entry point.

The control-separation hook now has a service-only profile that calls the
original native controller update exactly once and then invokes the one
registered capture observer with the actual borrowed `controller` and
`update_data`. Its pointer-free dispatch witness must prove the exact
service-post-original source, native game thread, outermost non-overlapping
dispatch, one original call, owned hook and slot, sole observer, and stable
observer-registry generation. The capture bridge revalidates that same witness
after its final copies. Co-op and all gameplay services remain off in this
profile.

The native bridge first uses discardable immutable A/B captures to plan the
mapped-image and dynamically discovered object spans. Each view is bounded to
128 ranges and 5 MiB. It then revalidates the complete range layout and native
read/write permissions before crossing the final boundary, takes independent
final A and B copies with barriers in the same callback and without yielding,
performs no further memory query after that boundary, and revalidates the
dispatch witness. Planning output, permission drift, capture drift, witness
drift, overlap, foreground loss, an overflow, or any incomplete span fails
closed and cannot reach the adapter sink.

The pure sampler parses only those two immutable views; it makes no OS or
engine call and retains no native address. In addition to exact group order
`[Tal,Ailish,Buki,Elco]` and formation order `[Tal,Elco,Ailish,Buki]`, it
requires the finite same-frame membership side-effect closure: formation
mutation fields and control backpointers, complete intrusive `TPtr` heads,
the sole listener and its dispatch slots, all four control/HUD gizmo/label and
no-allocation resource paths, stat displays and health-bar caches, stat-camera
sync nodes, Elco's arbiter coherence, and active camera/render/scene identity.
The resulting observation is wrapperless and pointer-free; Elco's wrapper
token is zero because no `GetPC` path exists.

The normal DLL adapter accepts at most the first valid, witness-bound result
and publishes it as observation only. Its production backend contains no
`GetPC`, wrapper resolver, `RemovePlayer`, `AddPlayer`, or wrapper-destructor
callback, performs no coordinator transition, and grants no actor-lifetime or
membership-mutation authority. The complete synthetic `4 -> 3 -> 4`
choreography remains confined to an injected test backend.

Exact-build follow-up confirmed that broad modal and transition predicates
cannot be comprehensive authority. The CUIScene input receiver can remain the
native CharacterController while TSA, Quick Menu, shop, blacksmith, quit,
dialog/conversation, or pause state is active, and `IsUIActive()` omits several
of those owners. Transition state is fragmented across world, async, HD-cache,
object, texture, door, script, and PVS domains. Those values remain
non-authoritative diagnostics. The finite listener/HUD/stat/arbiter/camera
closure plus the exact game-thread, post-original, no-yield witness is enough
to accept one observation of that frame; it is explicitly not enough to
authorize a membership call or establish actor lifetime.

`EnableTalosCompanionStagingObservation` is a default-off, closed profile. If
enabled, the loader rejects every other optional gameplay/trace feature, the
zone-trace environment hook, and `SkipStartupMovies=true`; it installs the
capture observer last and tears it down through disable, unregister, drain,
capture reset, adapter uninstall, and controller-hook uninstall. The checked-in
configuration remains `false`. No Sudeki process has been launched for this
observation pipeline, and no ordinary-world remove/add mutation has been run
live.

`tools/continue-research.sh --talos-staging-observation` builds and verifies
the exact images, generates that closed configuration, clears the zone-trace
environment hook, and prints the ordinary-world acceptance steps. It does not
enter the final transition: the intended live frame comes from a normal
four-hero save, so the long FMA07 Tal/Kazel movie is not part of this run.

The captured actor generation is an allocation/lifecycle identity, not a zone
counter. It must remain stable when the same actor crosses a world boundary. If
live observation instead shows reconstruction or generation churn, admission
fails; the equality rule is not weakened to make the experiment pass.

Once native Tal placement and the Void zone settle, the strongest placement
candidate is one native `AiPCFormationPopMembers()` call against the preserved
group. That route is already proven for ordinary temporary-zone formation but
not yet in the Void. It avoids `SpawnPC`, `AddPlayer`, and raw transforms.
Retail final cleanup is favorable: `GameFinish` calls the count-generic native
`RemoveAllPlayers`, which removes and schedules deletion of every remaining
group member. The first passive run observed its original call returning with
both native party owners changing from Tal-only to verified empty. A later
preserved four-person run must still repeat that acceptance; placement remains
entirely unproven in the Void.

The retired pre-Void carry-through and full four-human product remain blocked
by all of the following. These blockers do not apply to the closed two-human
post-movie respawn profile described at the top of this document:

1. A live-proven, target-specific pre-Void continuation that can be deferred
   and replayed exactly once without corrupting SOL task state.
2. A live-proven all-or-nothing omission of the three exact companion deletes,
   followed by native formation placement and count-generic final cleanup.
3. Per-seat Player 3/4 actor leases, camera/render states, HUD passes, frame
   caches, and control consumers. The current runtime is still two-seat.
4. Live traces of companion priority-bucket decisions, hit-response behavior,
   and cleanup across Talos teleports, clones, cinematics, death/revive, and
   exit.
5. Ordered acceptance runs: one human plus three AI, then two humans plus two
   AI, then three and four controllers.

No supported launcher should expose that carry-through/four-seat encounter
until those gates pass.
