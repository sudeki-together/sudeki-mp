# Saved-story client combat presentation — issue #55

Research recorded 2026-10-10 for [issue #55](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/55).
The owner accepted A5's repeated-hit, lethal-hit and next-wave regression check,
while reporting absent damage numbers, absent death chunks and possibly lingering
blood-like trails. These presentation issues remain open.

## Evidence boundary

The observed candidate was `04a31d5` plus the preserved dirty #54 prototype,
saved-story host Ailish/client Tal, castle-road ambush and subsequent combat.
Acceptance was recorded by `69383ec`; this investigation changes no gameplay.
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
