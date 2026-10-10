# Saved-story client combat presentation — issue #55

Research recorded 2026-10-10 for [issue #55](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/55).
The owner accepted A5's repeated-hit, lethal-hit and next-wave regression check,
while reporting absent damage numbers, absent death chunks and possibly lingering
blood-like trails. The owner accepted the three fixes on 2026-10-10; the
remaining combat/control observations are tracked separately in the plan.

## Owner acceptance — issue #55 resolved (2026-10-10)

**CONFIRMED_LIVE (owner visual acceptance):** on the paired saved-story
Ailish-host/Tal-client candidate, the owner confirmed that client damage numbers
appear, lingering hit effects finish and disappear, and death chunks appear.
The owner explicitly reported that everything in #55 appears resolved.
Both staged DLLs still match the candidate hash below. This closes the three
reported #55 symptoms for the tested encounter; it is not general campaign,
new-game, full teardown, or A7 ring/sigil acceptance.

The same play-test reported four separate follow-ups: the client shows the enemy
blocking pose but lacks the accompanying effect; Ailish's sweep lacks its blue
wave (expected visibility outside first person remains unknown); Tal cannot
activate skills/strikes; and Tal becomes very slow after the last enemy dies,
with an item and pickup flare observed. The owner corrected the initial apparent
freeze to slowdown. Causes, exact input/resource gates, and whether the slowdown
is local or shared remain **UNKNOWN**. These observations do not reopen the
accepted #55 symptoms; see the plan's acceptance/follow-up entry.

The acceptance commit saves the independent guards, native helper headers,
particle-capacity change and its focused test, plus the client residency adapter
hunks and this evidence. The spawn/world/protocol integration and expanded image
fixtures still depend on the owner's existing uncommitted #54 prototype and
remain in that same worktree under the no-baseline-commit instruction. The
committed subset alone is not the complete tested multiplayer build.
The exact scoped commit was also built separately from the dirty prototype;
its DLL build, `StoryEffectClockImageTest`, `StoryMonsterDamageGuardImageTest`
and `StoryResidencyCallTest` pass. This checks the commit subset independently,
without broadening the owner-accepted live scope.

## Implementation and pre-acceptance smoke test

The owner subsequently requested implementation. Candidate: `12e5767` plus the
preserved dirty #54 prototype and the #55 changes, saved-story host Ailish/client
Tal, supported executable below. Candidate DLL SHA-256:
`9d05e92778091aa4fb4e194a4c0d9f27516075726d5cc936df4c36cc89a98b26`.
This section records the pre-acceptance implementation and smoke test. The
owner acceptance below supersedes its pending visual checks.

- **Numbers:** host display eligibility is carried on monster snapshots. Before
  replay, the exact owned CStatDisplay is armed only when the host is eligible;
  local display settings still apply natively. The displayed value uses the
  host HP delta, while replay damage reserves positive client HP even after
  binary16 storage. **CONFIRMED_LIVE:** the host timer was positive while the
  client's was negative before this change; the candidate visibly shows damage
  numbers, including repeated `-19` values, on the client.
- **Hit lifetime:** particle topology now accepts up to the verified 4,096-entry
  capacity, retaining resource/node/owner checks. Ordinary SpecialEffects are
  discovered from the current registry each tick and receive native animation
  and completion updates without the old 24-slot, 15-second or active-group
  cutoff. Native completion owns retirement. **CONFIRMED_LIVE:** all six sampled
  `b51df833` hit-effect identities were later removed from the registry; the
  sampled run reached 4,200 admitted cosmetic updates with zero refusals.
  This does not identify every blood-like resource or prove visual parity.
- **Death presentation:** the host retains its selected death style until
  despawn. The client resolves the corresponding action's authored independent
  SFX from its matched resident monster and calls visual creation `0x417DA0`.
  It does not call gameplay death `0x4D31C0`, award loot or XP, or let replayed
  damage kill the copy. **CONFIRMED_EXACT_IMAGE:** AL=1, CL=0, six stack words,
  RET 24 and copied ResourceName lease consumption. Attached/weapon resources
  remain refused. **UNKNOWN:** chunk-finisher visual parity. The smoke test's
  four deaths selected style 3, whose resident action has no independent effect;
  it did not exercise the new chunk creation branch.

The paired candidate uses session protocol 18 and world frame version 8. Six
remaining area-message bytes pack sixteen bounded death styles; the area body
is exactly 1,440 bytes and the datagram 1,468 bytes. Old protocol peers are
rejected. This temporary encoding does not complete the planned B1 split.

### A6 integration corrections discovered by the live test

The stricter pause proof exposed two actual asynchronous native continuations.
The native group-start replay also now acknowledges the observed group so the
same request does not restart it every frame.

`SoloSpawnSetup` jobs constructed at `0x4762EA` during our native group update
are journaled through their real destructor return. Their exact callback at
`0x472D50` registers the actor and later enables scheduling/initialization.
Each callback gets its own synchronous witness and immediate accounting; only
its proved actor may make the native schedule transition. Pending jobs retain
the hooks. There is no combat-time permission window or fabricated cancellation.

After host-driven despawn, the animation owner's deferred destructor stops its
sigil through the call at `0x4E1870` to `0x531DF0`. The adapter brackets that exact
call and allows only that owned effect to leave the registry, with no task
creation, additions, other removals or changed surviving pause identities.
Its post-witness does not read released storage. Native work runs exactly once
on refusal or nested entry. `0x531DF0` stays byte-for-byte unchanged for
`SudekiMpLanArenaSpiritVfxVisualImageMatches`, `lifetime_image_exact` and the
jetpack validator; symbolic names and literal addresses were checked.

**CONFIRMED_LIVE:** the final candidate replayed 43 hits and followed four
host despawns, including four deferred sigil releases, without an unknown
containment state. Later-wave monsters spawned, but later-wave combat and
complete end-to-end teardown were not accepted. This supersedes any inference
that A6's earlier pure tests alone covered asynchronous native residency.

### Validation and remaining gate

DLL build and focused tests passed: `StorySpawnTimingImageTest`,
`StoryEffectClockImageTest`, `StoryMonsterDamageGuardImageTest`,
`StoryMonsterDamageFlushImageTest`, `StoryAreaStateTest`, `LanPartySessionTest`
and `StoryResidencyCallTest`. Tests cover native effect completion, the visual
creation ABI, 48-effect bursts, 318 particle instances, repeated nonlethal HP
packing, display ownership/gates, death-style wire bounds, journaled constructor
arguments and retirement, pending-job teardown refusal, and unchanged retirement
function bytes. Native-image fixtures use inert renderer/allocation endpoints;
they do not prove chunk rendering or an actual game DLL unload.

Owner procedure: compare both peers during repeated hits, a chunk-producing
finisher, ordinary kills and at least two later waves. Numbers should appear
when enabled; chunk-producing host deaths should produce the corresponding
client effect; hit/blood effects should finish without accumulating at feet.
Check that movement, repeated hits and subsequent spawns continue after kills.
This was the requested procedure; the owner subsequently accepted the three
#55 symptoms as recorded above. Do not infer unreported route or wave coverage.

A7 remains separate: its provisional six-second sigil hide and host-driven
termination dependency are not completed by these fixes. The initial research
below is retained as historical evidence; its unresolved mechanisms are updated
by the candidate findings above.

## Evidence boundary

The observed candidate was `04a31d5` plus the preserved dirty #54 prototype,
saved-story host Ailish/client Tal, castle-road ambush and subsequent combat.
Acceptance was recorded by `69383ec`; that initial investigation changed no gameplay.
Native research used the supported executable SHA-256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Addresses below are virtual addresses at preferred base `0x400000`.
Raw logs and native research output remain private. Conclusions below are
sanitized from the owner-authorized investigation.

## Particle advancement stops at the instance guard

**CONFIRMED_LIVE:** the accepted client's log first rejects the particle manager
at 129 instances, then records counts rising to 318. All 36 sampled manager
rejections report `instance_limit`; the final sampled refusal counter is 5,689.
This is sampled logging, not a count of 36 failed frames.

**CONFIRMED_STATIC:** `lan_story_effects.c` limits `MAX_INSTANCES` to 128.
`topology()` refuses the entire cosmetic update above that bound. In the paused
client path the fallback native update receives the original zero delta, so
that fallback does not provide the refused cosmetic time. This establishes a
real route to stopped particle aging during the observed combat run.

**UNKNOWN:** which blood-like resource the owner saw, its expected native
lifetime, and why instances accumulated. The owner's trail-expiry observation
is tentative. The count alone does not prove that every instance leaked or
that all visible trails share this cause. Increasing the bound alone is not a
lifecycle repair and must not bypass topology/ownership validation.

Additional **CONFIRMED_STATIC** constraints in `lan_story_spawn_trace.c` deserve
measurement: manual SpecialEffect tracking has 24 slots and stops tracking an
entry after 15 seconds; it runs only while a replayed group is tracked. Pointer
caches and slot exhaustion are candidate omissions, not proved leak causes.
On the supported image, SpecialEffect update `0x5319B0` advances its animation
and retires eligible effects through `0x531DF0` only after completion flags are
observed. Particle advancement alone does not prove SpecialEffect retirement.

Next check: identify each hit resource on both peers, record creation, native
completion, registry removal and particle-instance removal across repeated hits,
kills and two later waves. Distinguish intentional looping assets from lost
updates/cleanup. Keep ordinary hit effects out of the A4 sigil-hide predicate.
A7's clock continuity work must account for this observed guard rejection;
the broader impact-effect lifecycle remains tracked in #55.

## Damage numbers need more than replayed HP

**CONFIRMED_STATIC**, exact-image disassembly/decompilation: native hit handling
`0x4D21D0` calls `0x529500`, which stores a damage value at CStatDisplay `+0x74`
and calls `0x529800`. That routine requires byte `+0x24` and only arms the number
state at `+0x48` when the float at `+0x18` is positive. Constructor `0x527CE0`
initializes that float to zero. Update `0x528FF0` additionally requires byte
`+0x30`; its settings-manager `+0x28C` branch can refresh `+0x18`.
Number state machine `0x5298A0` operates on separate UI/renderer objects.
The later combat-visibility branch in `0x528FF0` must not be mistaken for the
earlier number-state update.

The current feedback lease manually calls `0x528FF0` with the real delta at
timing-block `+0x0C`. Its named `stat_entry` validator agrees with the supported
entry; the native return pops eight bytes. The separate `flash_entry` validation
and `0x4DFAB0` path were also located by name and address. No new native patch or
ABI change was made during research.

**CONFIRMED_LIVE:** 233 accepted-run replayed hits have `identity_proof=1` and
`restore_pending=0`; the owner reports visible damage but no numbers.
**UNKNOWN:** which display gate or UI lifetime fails in that run. Existing logs
do not capture the required fields. There was no usable live combat registry
left for a read-only follow-up probe, so no field values are invented.

Phase D5 should capture `+0x18`, `+0x24`, `+0x30`, `+0x48`, `+0x74`, the current
settings-manager gate and owned UI/renderer state immediately before/after a hit
and feedback tick on both peers. Prove the full component span and current
owners before observation. Test repeated and lethal hits; host-like numbers,
bar and flash still require the owner's visual acceptance.

## Host despawn is not a complete death presentation

**CONFIRMED_STATIC:** client damage replay constructs a fresh generic
DamageStructure (`0x585530`, amount setter `0x585960`) from an HP delta.
It does not reproduce host fatal-hit metadata. Native death selection
`0x4D31C0` branches on DamageStructure bytes `+0x60`, `+0x62` and `+0x63`, and
chooses an action in the `0x17`–`0x1A` range. Which action produces the observed
chunks is **UNKNOWN**; do not assign that meaning from these numbers alone.
The same routine includes gameplay death/loot work, so calling it merely to
obtain visuals would violate Phase D1's host-owned death rule.

`SudekiMpLanStorySpawnDespawnFollow` directly removes the matching client entity
when the host reports despawn. There is no separate host death-presentation
selector or visual-completion contract in that message.
**CONFIRMED_LIVE:** the acceptance snapshots contain 22 host despawn records
and 22 corresponding client follow records with a found entity; the owner
reports enemies disappearing where host enemies should break into chunks.
**INFERENCE:** the missing fatal-hit data and presentation-completion path
explain why HP/despawn replication alone cannot guarantee native death visuals.
The exact chunk emitter/action consumer still requires native tracing.

Next check: correlate host fatal-hit type, selected death action, chunk/effect
creation and despawn order with the client. Prove a visual-only path that does
not execute client death, loot or XP. Phase D1 must keep client HP above zero
and follow host despawns; Phase D5's hit-feedback work does not itself settle
chunks. Keep #55 open until the owner verifies the relevant deaths.
