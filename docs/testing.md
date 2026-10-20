# Testing and evidence hierarchy

This document owns the test/evidence boundary and result-reporting contract. It
does not duplicate individual test cases, build instructions, or live research
logs. SudekiMP tests undocumented native behavior at several distinct levels; a
claim must state which level was actually reached.

## 1. Build/compile checks

Examples include compiling the DLL, launchers, helpers, and test executables.

They prove:

- source and declared dependencies compile for the selected toolchain;
- link-time symbols and calling-convention declarations are internally usable;
- selected static warnings are absent.

They do **not** prove:

- that a hook matches the retail executable;
- that the native ABI interpretation is correct at runtime;
- that the game launches;
- that animation, camera, input, damage, or teardown looks/behaves correctly.

## 2. Pure and policy tests

These exercise modules without a live game image: state machines, authority
rules, layouts, roster policy, handoff decisions, interpolation math, and other
bounded logic.

They prove deterministic behavior for the tested inputs and invariants.

They do **not** prove that native pointers, callsites, objects, timing, or thread
boundaries match the game.

## 3. Protocol tests

These cover packet validation, encode/decode, versioning, sequencing, malformed
input, authority fields, action journals, snapshot rules, and interpolation.

They prove the tested wire contract between project modules.

They do **not** prove:

- socket behavior under a real network;
- native host admission or client presentation;
- cryptographic authenticity or Internet safety;
- that a structurally valid native selector/state is safe to apply.

## 4. Synthetic hook tests

These construct controlled mapped memory/callsites and exercise hook install,
redirect, ordering, rollback, restoration, and retry behavior.

They prove the generic or feature-specific hook policy under the synthetic
fixture.

They do **not** prove that the expected bytes or calling context exist in the
supported retail executable.

## 5. Exact supported-image tests

These map or inspect the user-supplied supported `SUDEKI.exe` and verify exact
bytes, targets, exports, vtables, signatures, install scope, and restoration.

They prove that the tested hook/native seam matches that exact image and that
the tested install/restore procedure works without executing normal gameplay.

They do **not** prove:

- that the hook is reached in the expected gameplay state;
- native object lifetime across a real frame or transition;
- correct visible animation, camera, audio, UI, collision, or combat;
- safe disconnect or process teardown during an asynchronous task.

Exact-image hook installation is not live gameplay proof.

## 6. Loopback/integration tests

LAN and controller harnesses may start helpers or two isolated processes and
exercise transport/session integration.

They can prove bounded end-to-end message flow, role/session setup, cleanup,
and selected state convergence in the tested environment.

They do **not** automatically prove two-machine behavior, adverse-network
behavior, presentation quality, or campaign safety.

## 7. Live in-game acceptance

A live proof records an exact revision/profile/environment and a human-observed
or instrumented gameplay outcome.

A useful live record includes:

- clean or dirty revision identity;
- executable identity and protocol/profile;
- participants/actors/map;
- exact action sequence;
- expected and observed result;
- relevant logs/screenshots/video identifiers;
- timeout, disconnect, teardown, or retry behavior;
- limitations and negative observations.

Live proof is bounded. “Ailish moved once in testroom” does not prove campaign
movement, another actor bank, reconnect, or teardown.

## Actual execution mechanisms

The repository uses CMake to define many standalone test executables. No CTest
`add_test` registration was observed at the audit baseline, so `ctest` is not
the canonical runner.

Tests are invoked through combinations of:

- Linux cross-build scripts;
- individual Wine commands;
- shell loopback/launcher harnesses;
- native Windows build scripts;
- a selective, manually triggered Windows forge workflow;
- exact-image tests requiring a legitimate user-supplied executable;
- explicit live game sessions.

Do not say “the test suite passed” unless the exact command list is recorded.

## Evidence result format

For every claimed verification, record:

```text
Revision: <commit and dirty-tree identifier>
Profile: <closed launch/config profile>
Environment class: <Windows/Wine; no private machine identity>
Command/test: <exact reproducible command or target>
Expected: <bounded expected result>
Observed: <bounded observed result>
Evidence level: <category from evidence-index.md>
Artifacts: <sanitized repository evidence or private local evidence ID>
Limitations: <untested cases and known failures>
```

## Merge and release gates

> **HUMAN DECISION REQUIRED:** The repository does not currently establish one
> mandatory, comprehensive merge gate.

The owner should define separate minimum gates for:

- pure/policy-only changes;
- protocol changes;
- generic hook changes;
- exact-image adapter changes;
- local co-op runtime changes;
- LAN runtime/lifecycle changes;
- launcher/package changes;
- public release candidates.

Until that decision exists, agents must report exactly what they ran and must
not convert a selected green subset into a global “all tests pass” claim.

## Recommended status update rule

Update `status-matrix.md` only after the evidence record exists. Preserve the
lowest status when:

- the code is dirty/uncommitted;
- exact-image coverage is missing;
- a live behavior is only inferred from logs;
- rollback/disconnect has not been exercised;
- evidence from two documents conflicts.


## Enemy targeting watch build — 2026-10-19

Superseded by the [away-menu diagnostic and tiers 1/1.5/2 candidate](#enemy-targeting-away-menu-diagnostic-and-tiers-1152--2026-10-19); this section retains its original hash-pinned evidence.

Historical watch-only artifact. The subsequent [cap parity build](#enemy-targeting-cap-parity--2026-10-19)
implements owner decision 1. The current [tier 0 candidate](#enemy-targeting-away-parity-and-lead-pc-fallback--2026-10-19)
also implements away parity and Lead-PC fallback, retaining the watch default.

Profile: saved-story host (`story_observation=2`), `[StoryAreas]
EnemyTargeting=watch`. Revision: dirty `fa50351`, watch DLL SHA256
`514b2216b6778966170794003e6c26bbec749a37af86a5b4d7c9f2515eea3dbc`.
Supported executable: the hash-pinned GOG image in [executable.md](executable.md).
Environment class: Linux cross-build and Wine, synthetic fixtures using the
supported image; no live game or owner acceptance in this record.

The host installs a read-only selector observer, the two cap-compare observers,
and a consumer of the existing shared HP receiver. The arena keeps its own
feedback consumer path. Story clients do not install the adapter. The runtime
passes its existing dead-lead `remote` computation and binds the HP observer
from its verified native callback before registering the story consumer.

`watch` is the default. `off` disables observation; the installed cap thunks
still replay the real comparison. The full threshold table is parsed at
installation and once per second by `story_threat.c`. Invalid values retain
safe defaults. Gameplay levels `tier0`, `tier1`, `tier15`, and `tier2` are
**not implemented in this watch release**: selecting one logs `owner_gate`
and keeps watch behavior. The prerequisite clamp to tier 0 when cap parity is
off is logged separately. No config setting in this release enables cap parity
or a target substitution. The brief's owner decisions remain open, except for
the requested watch-first ordering (decision 7).

### Native contract correction

CONFIRMED_STATIC and CONFIRMED_EXACT_IMAGE: selector VA `0x5B76C0` is
**stdcall**, correcting the brief's original cdecl annotation (now corrected there too). All three exits at VAs
`0x5B7726`, `0x5B773E`, and `0x5B79EC` use `RET 0x10`; EAX is the out-slot
address and `*out` is a raw CAiUnit pointer. The adapter checks the complete
selector body with relocation normalization, the cap-filter body (including
ECX compares and the following JG/mode checks), and all 24 rel32 callers.
The image fixture repeatedly executes the native mode-E path, checking stack
balance and both return values. It also verifies the existing-target discount
in both native cap branches. Watch never reads the stale global candidate
list for mode E. No target, aware target, attacker nibble, or request-mode
field is written by the adapter.

### Executed checks

`tools/build-linux.sh` completed successfully. The following named executables
were run under Wine and passed (these are selected checks, not the whole suite):

| Executable in `build/mingw32/bin` | Arguments | Evidence and coverage |
| --- | --- | --- |
| `SudekiMP.StoryThreatTest.exe` | none | CONFIRMED_TEST: seat precedence, stale/held/draining input, named defaults, invalid config, prerequisites, cap math and target discount |
| `SudekiMP.StoryEnemyTargetingImageTest.exe` | supported `SUDEKI.exe` path | CONFIRMED_EXACT_IMAGE: every-byte selector/filter refusal, 24 caller checks, native stdcall/result contract, real cap branches, all five install failure stages, duplicate install, thread/busy refusal, retained restoration failures and retry/reinstall. CONFIRMED_TEST: unchanged native answers for every mode, inactive/unknown/ally guards, HP delta and hit-all journal fields, source==victim, spawn replacement, and unknown-versus-positive retirement |
| `SudekiMP.LanArenaHitFeedbackTest.exe` | none | CONFIRMED_TEST: existing popup/reaction journal and replay checks, plus explicit thread binding, duplicate consumer rejection, nested independent captures, exactly one damage call, and busy consumer-removal refusal during callbacks. Isolated foreign-thread removal/uninstall assertions were added in the [review corrections](#enemy-targeting-tier-0-review-corrections--2026-10-19) |
| `SudekiMP.LanPartyProjectileLifetimeTest.exe` | supported `SUDEKI.exe` path | CONFIRMED_EXACT_IMAGE: existing native weak-pointer/destructor/signature checks. CONFIRMED_TEST: both MissileData packet forms, match/no-match, callback-time read-only attribution, bad ownership refusal and existing lifetime/retirement cases |
| `SudekiMP.StoryShotsTest.exe` | none | CONFIRMED_TEST: existing shot journal/codec and combo UI regression checks |

`tests/story_threat_test.c` also passed as a native C11 build with
`-Wall -Wextra -Werror`. `git diff --check` passed. No evidence here proves
live callback reachability, playability, observer overhead, or disconnect
behavior in a running game.

### Owner-run watch gate

Use the existing saved-story launch profile and its host `SudekiMP.ini`:

```ini
[StoryAreas]
EnemyTargeting=watch
EnemyTargetingCapParity=off
EnemyTargetingLeadRule=host_down
EnemyTargetingExcludeTypes=MON_GuardSparPartner_Melee
```

The next gate is the castle-road ambush, Ailish host / Tal client. Observe
ordinary melee and ranged hits, a Spirit Strike, each player being nearer,
the host down, Tal running and entering melee range, a shop/house separation,
step-away/return, and disconnect/title/rejoin. Expected: unchanged gameplay,
zero substitutions, and the following `story_enemy_targeting` records:

- `selection`: per-type/mode requests, empty native answers, per-character
  picks and cap-only refusals. The latter is counted inside S2 after the
  non-cap native filter succeeds, including an empty final candidate list;
  both authored caps must otherwise admit the candidate.
- `hit`: spawn identity, attributed source, applied HP delta/max HP, lethal,
  distance, missile and arbiter classifications, explicit observation-known
  bits, Spirit tag, packet result/class/flags, and firing. Self-source packets
  are unattributable. Unreadable or unleased victims increment `hit_unknown`.
- `seat`: seat kind, firing, targeters/attackers, shot sequence/tick, measured
  speed, nearest targeter distance and native melee-reach distance. These
  measurements test the proposed world scale; the default distances are not
  live-validated tuning values.
- `enemy` / `retired`: epoch, spawn serial/resource and spawn group, then
  positive retirement. An unknown registry observation never retires a record.
- `stale_count`: native nibbles compared with manager targeters, partitioned
  by the native +0x46 ranged bit. These are observations, never counter repairs.
- `installed`: AI damage multiplier, then periodic `status` records. Host
  alone must report `active=no`; the LAN arena must have no adapter records.

The watch build does not choose alert groups, accumulate threat, push targets,
or scale stats. Distance tuning, fallback semantics, away treatment and all
other gameplay decisions in [the brief](enemy-ai-task.md#owner-decisions)
remain for the owner before the corresponding tiers are implemented.

## Enemy targeting cap parity — 2026-10-19

Superseded by the [away-menu diagnostic and tiers 1/1.5/2 candidate](#enemy-targeting-away-menu-diagnostic-and-tiers-1152--2026-10-19); this section retains its original hash-pinned evidence.

Historical active-client-only artifact. Its implementation limits and owner
gates below describe that artifact; the current candidate is recorded in the
[next section](#enemy-targeting-away-parity-and-lead-pc-fallback--2026-10-19).

Owner decision 1 is accepted in [the task brief](enemy-ai-task.md#owner-decisions):
active human clients should participate in the same fight as the host and may
face several enemies at once. This accepts the gameplay tradeoff, not a live
validation result. Decision 2 was subsequently accepted as always-nearest;
its implementation remains pending the complete fallback choice (decision 8).
Decision 3 was subsequently accepted as normal player caps during away AI
control; that extension is not in the artifact below. Decisions 4–6 and 8
remain open.

Profile: saved-story host, runtime co-op active, `EnemyTargeting=tier0` and
`EnemyTargetingCapParity=on`. Revision: dirty `fa50351`, DLL SHA256
`a886dec0be5cc20c82657841f1bc059909d4a0c8754115c5652446e86dbe112a`.
Supported image: the GOG executable pinned in [executable.md](executable.md).
Environment: Linux cross-build and Wine fixtures; no live gameplay, deployment,
or disconnect/title/rejoin run was performed for this change.

Tier 0a changes only the result of the two S2 override comparisons for a proven
HUMAN_REMOTE candidate. Native code keeps its authored melee/ranged limits,
existing-target discount, unlimited-cap sentinel, non-cap filtering, and target
commit. No fixed higher cap is introduced. The adapter does not write the
control-override field, attacker counters, target fields, or request mode.
The native selector continues to choose its result in every mode; cap parity
changes candidate admission without replacing that result. Lead targeting
(tier 0b), threat memory, alerts, idle pressure and scaling remain unimplemented.

Admission rechecks the retained actor/control lease and current session using
`SudekiMpLanPartyControlStoryTurnOwner` and
`SudekiMpLanStoryRuntimeTurnLeaseExact`. It also requires the exact roster,
fresh per-seat input, a live remote seat, the single owned override, and native
player mode. Watch/off, host-alone, held/stale/draining input, lost ownership,
down/absent/away/AI seats, and unproven identities retain native comparisons.
Enemy types 1–3 alone can receive parity; excluded names or resource hashes,
including the sparring partner, retain native treatment. Resource hash matching
uses the checksum documented in [resource-swap.md](resource-swap.md).
The owner subsequently accepted normal player caps for away characters
(decision 3); this earlier implementation grants no parity to away seats.
That accepted extension still needs implementation and validation.

### Validation

`tools/build-linux.sh` passed. These focused executables in `build/mingw32/bin`
ran under Wine and passed:

- `SudekiMP.StoryThreatTest.exe`: CONFIRMED_TEST for every seat/type/level
  combination, inactive/excluded/disabled parity, accepted defaults, config
  parsing, resource exclusions, and existing cap math/seat precedence.
- `SudekiMP.StoryEnemyTargetingImageTest.exe <supported SUDEKI.exe>`:
  CONFIRMED_EXACT_IMAGE for execution of both native S2 branches through the
  hooked stdcall mode-E selector, additional attacker admission, refusal at
  both authored limits, both self-discounts, unlimited and zero limits, and
  preserved native AI/request bytes. The non-cap predicate and external lease
  providers are synthetic; their live reachability is not proved here.
  CONFIRMED_TEST for the lease/session/thread/identity/input/seat/type/exclusion
  refusals, unchanged watch/off behavior, and unchanged direct filter calls
  outside an enemy selector scope. The full existing every-byte, install
  rollback, retained failed restore/retry, hit-journal, and retirement fixtures
  also passed.

The pure policy test also passed with native GCC C11 and
`-Wall -Wextra -Werror`; `git diff --check` passed. These checks establish bounded
native execution and synthetic admission behavior, not visible encounter
balance or live safety through transitions.

### Switches and next live gate

The master default remains `EnemyTargeting=watch`; watch never enables parity,
even though the separate `EnemyTargetingCapParity` default is now `on` following
owner acceptance. After the watch observations, the cap-only experiment uses:

```ini
[StoryAreas]
EnemyTargeting=tier0
EnemyTargetingCapParity=on
EnemyTargetingExcludeTypes=MON_GuardSparPartner_Melee
```

`EnemyTargetingLeadRule` currently has no gameplay effect. Choosing tier 0 or a
higher level logs `owner_gate effective=tier0a`, identifying the unimplemented
lead rule and higher tiers. Selecting a higher tier cannot enable them.
Turning parity off restores native comparisons while keeping observations.
The existing prerequisite clamp for higher levels with parity off is retained.
Configuration reloads on the existing one-second timer.

The `status` record reports whether parity is enabled. Per-seat
`cap_parity_requests` counts selector requests where an override comparison was
successfully bypassed; it does not claim that the final caps admitted the seat
or that the enemy picked it. `cap_refusals` continues counting actual native
cap-only refusals on unchanged comparisons. Substitutions remain zero.

Owner live gate: castle-road ambush, Ailish host / Tal client, compare watch and
cap parity with each player nearer. Check additional melee and ranged attackers
on Tal within authored limits, excluded sparring behavior, stale/held input,
step-away/return, and disconnect/title/rejoin/teardown. Host-alone and arena
must remain inert. Lead-PC enemies can still favor Ailish or idle while she is
down: tier 0b is not implemented yet. The owner subsequently accepted
always-nearest (decision 2) and full attacker caps while away (decision 3).
The complete fallback when no standing player is available (decision 8) remains
open. This is a
documentation-only acceptance update; the tested DLL above is unchanged.

## Enemy targeting away parity and Lead-PC fallback — 2026-10-19

Superseded by the [away-menu diagnostic and tiers 1/1.5/2 candidate](#enemy-targeting-away-menu-diagnostic-and-tiers-1152--2026-10-19); this section retains its original hash-pinned evidence.

This artifact precedes the [companion Defend addition](#companion-defend-with-tier-0--2026-10-19)
below. Its enemy behavior and evidence limits still apply to that candidate.

Profile: saved-story host, co-op active, `EnemyTargeting=tier0`,
`EnemyTargetingCapParity=on`, `EnemyTargetingLeadRule=always`. Revision:
dirty `fa50351`; DLL SHA256
`f6cd92111b150b60679f7e932e8cbee657eef4597627a2589e067a283f6c520d`.
Supported image: the GOG executable pinned in [executable.md](executable.md).
Environment: Linux cross-build, native policy tests and Wine fixtures. No
deployment, running-game test or owner live acceptance is claimed here.

Owner decisions 1–3 and 8 are implemented in this candidate. A proven active
client or connected away player's AI body takes both native authored-cap
branches. Away admission requires an exact current connection/reservation,
native AI ownership, no retained input key and no acquisition/drain in progress.
The runtime keeps enemy targeting active when its last connected client is
away and that client's native input lease has drained. The existing dead-lead
hold predicate is unchanged. No cap value, ownership, stance, AI mode or native
attacker counter is written.

For Lead-PC requests only (mode B), the adapter selects the nearest eligible
standing human from this invocation's native list, with the accepted switching
margin. If no human is eligible, it chooses a living AI/away party member from
that list. A candidate is checked again through the native filter before
replacing `*out`; the state caller retains responsibility for committing it.
No candidate or unknown identity means no substitution. Native Nearest mode
and nonempty mode-E locks retain their selection rules; the existing cap
adapter can still affect their candidate eligibility. No mode-E substitution,
threat memory, alerts, idle pressure or companion Defend substitution is present.
Boss attacks that directly address formation slot 0 remain outside this seam.

### Executed checks and evidence limits

`tools/build-linux.sh` passed. The following focused checks passed:

- `tests/story_threat_test.c`, native GCC C11 with
  `-Wall -Wextra -Werror`: CONFIRMED_TEST for human priority, AI/away fallback,
  absent/down rejection, nearest choice, fractional/absolute switching margins,
  equal-distance stability, returning-human priority, invalid distances,
  inactive/excluded/type/mode gates and both lead-rule variants. Existing
  classification, config and cap-policy checks also pass.
- `SudekiMP.StoryEnemyTargetingImageTest.exe <supported SUDEKI.exe>` under
  Wine: CONFIRMED_EXACT_IMAGE for both native authored-cap continuations with
  AI mode 1 and override count 0, both authored limits, existing-target
  discounts, zero/unlimited limits, stdcall/out-slot contract and retained
  dependencies after failed restoration. CONFIRMED_TEST for away identity,
  session/thread/ownership refusals, watch/off/inactive paths and the same
  body's eligibility after returning to human control. External lease
  providers and the non-cap predicate are synthetic.
- The same image executable also exercises Lead-PC substitutions through S1
  with a **synthetic candidate-list/native-result builder**, followed by the
  actual native S2 filter/cap code. It checks nearest/sticky selection,
  AI/away fallback, returning humans, invalid component ownership, live down
  flags, caps, exclusions, unknown roster and unchanged unrelated modes.
  This is bounded adapter evidence, not proof of native mode-B list construction
  or the calling state's subsequent target commit. Existing signature refusal,
  transactional install/rollback, failed restore/retry, hit and retirement
  checks also pass.
- `SudekiMP.StorySplitControlTest.exe` under Wine: CONFIRMED_TEST of the actual
  runtime away-admission function with synthetic session/transport inputs and
  the real ownership policy. It refuses retained leases, acquisition/drain,
  pending swaps, stale connection identities, missing transport, changed
  ownership, modal/unbound and inactive profile states. This artifact tested
  an unbound runtime, not a worker calling the away accessor; actual worker-thread
  coverage was added in the [review corrections](#enemy-targeting-tier-0-review-corrections--2026-10-19).
  Existing split-control/camera cases pass.
- `tests/party_ownership_test.c`, native GCC C11 with
  `-Wall -Wextra -Werror`: CONFIRMED_TEST of the existing reserved-away-body and
  occupied-target swap rules. No ownership-policy change was needed.

The away/return fixture does not prove uninterrupted target retention through
the live control handshake. The design grants no immunity, and the same body
can remain or become a target subject to native eligibility and attacker limits.
It does not force every enemy to switch to the returning player.

### Current switches and next live gate

The master default remains `watch`; no target or cap behavior changes in watch.
For the tier 0 experiment after observation:

```ini
[StoryAreas]
EnemyTargeting=tier0
EnemyTargetingCapParity=on
EnemyTargetingLeadRule=always
EnemyTargetingExcludeTypes=MON_GuardSparPartner_Melee
```

`EnemyTargetingLeadRule=host_down` remains available for comparison. Turning
cap parity off restores native cap branches; it does not disable the tier 0
Lead-PC rule. Use `watch` or `off` to disable all substitutions. Higher tiers
log `owner_gate effective=tier0 higher_tiers=unimplemented`; their policy is not
enabled. Configuration still reloads once per second. `lead_substitution`
records are bounded and identify the chosen seat and whether it is a fallback;
periodic status includes the total substitutions. Mode-E substitutions remain
zero.

Owner live gate: castle-road ambush, Ailish host / Tal client. Compare watch
and tier 0 with each player nearer, then host KO, no eligible standing human,
the last client away after lease release, and return to that same body. Check
native melee/ranged limits, no unexpected target oscillation, stale/held input,
shop/house separation, sparring exclusion, disconnect/title/rejoin and teardown.
Host-alone and arena profiles must remain inert. Companion stance preservation,
formation behavior and Defend adaptation require their separate observation
and validation plan; enemy-tier tests do not prove them.

## Companion Defend with tier 0 — 2026-10-19

Superseded by the [away-menu diagnostic and tiers 1/1.5/2 candidate](#enemy-targeting-away-menu-diagnostic-and-tiers-1152--2026-10-19); this section retains its original hash-pinned evidence.

Owner decision 9 is fully accepted in [the brief](enemy-ai-task.md#owner-decisions):
protect the nearest eligible standing human; without one, protect the nearest
other living AI/away companion; use self only when alone. Keep an eligible
anchor unless another is closer by more than `max(20% of current distance,
2 units)`. Enable Defend adaptation with enemy tier 0. Attack, Retreat,
ranged self-defense priority, formation following and stance ownership remain
native.

Profile: saved-story host, co-op active; master default remains `watch`.
Revision: dirty `fa50351`; DLL SHA256
`58ddcf13a01881d3a81717b39cab595a830a4f9f4667650d289ddc945c9ca248`.
Supported image: the GOG executable pinned in [executable.md](executable.md).
Linux cross-build and native/Wine fixtures only; no deployment or live game run.

The existing S1 adapter now has a separate party-self branch. It requires an
exact roster AI, owned stats/state-array/formation layout, selected Defend row,
the corresponding evaluated state's own request, native AI control and a
current unclaimed or fully released connected-away reservation. A state may
be evaluated while Formation or no state is active. S2's admission remains
enemy-only. The native list and a fresh native filter check govern eligible
enemies; the adapter replaces only `*out`. No stance, formation, native target
field, counter or ownership write is added.

CONFIRMED_STATIC correction during implementation: the melee and missile
constructors at `0x5AE8C0` / `0x5AE1F0` initialize request mode flags to `0xC0`;
the builders preserve bit 7, yielding `0xF9` for Defend, and preserve the high
nibble of request+25. The policy checks only the consumed mode/constraint/side
and low filter bits, preserving those unrelated bits. Fixtures include them.

This candidate implemented a one-second `companion_state` throttle and a
250 ms `companion_selection` throttle with a 128-record limit per character.
Those bounds were not asserted by this artifact's tests; its >250 ms admission
gap also reset the selection budget. The [review corrections](#enemy-targeting-tier-0-review-corrections--2026-10-19)
preserve that budget until a roster namespace change and assert both throttles
and the cap. Records include stance row, AI mode,
active/evaluated states, actual formation leader identity, native target
resource and distances to self/formation leader. Observation failure remains
unknown. Target decisions are never cached; anchor memory stores only a
character index, survives normal per-frame admission suspension, and is cleared
by invalid/expired observations, roster namespace changes or watch/off.

`tools/build-linux.sh` passed, followed by:

- Native `story_threat_test.c` with GCC C11 `-Wall -Wextra -Werror`:
  CONFIRMED_TEST for human-first anchoring, other-companion/away/self fallback,
  invalid/down/absent anchors, returning humans, both switching margins,
  watch/off/inactive guards and configurable defaults. Existing enemy-policy
  tests also pass.
- Wine `SudekiMP.StoryEnemyTargetingImageTest.exe <supported SUDEKI.exe>`:
  CONFIRMED_EXACT_IMAGE for native melee and missile request builders, their
  constructor-initialized flag values, and a full native selector candidate-list
  and formation-relative mode-9 metric execution: watch chooses the enemy near
  formation slot 0; tier 0 chooses the enemy near the selected standing human.
  The non-cap world predicate, ownership provider, actor components and manager
  are synthetic. Native movement, attack execution and the state's subsequent
  commit are not exercised.
- The same executable: CONFIRMED_TEST for all four Defend state classes,
  evaluated-before-active and no-active-state cases, exact state-request ownership,
  invalid stats/formation/position ownership, override/control rejection, all
  unrelated modes passing through, down/absent fallback, anchor switching,
  unchanged ranged self-defense and preserved native fields/LastError. It checks
  known/unknown observation and anchor suspension/invalid-admission behavior;
  log timing and count bounds were not asserted until the review corrections. Every-byte refusal now also covers the request builders, stance
  readers/weights and formation layout helpers; vtable request slots are checked.
  Existing native cap, signature, install/rollback, failed restoration/retry,
  hit and retirement tests pass.
- Wine `SudekiMP.StorySplitControlTest.exe`: CONFIRMED_TEST of the actual
  runtime companion-admission accessor for available unclaimed characters and
  fully drained away reservations, including host, unavailable, draining,
  not-away, modal and invalid-character refusals. Existing runtime cases pass.

`[StoryAreas] EnemyTargeting=tier0` enables both accepted enemy and Defend
changes. `CompanionDefendMarginFraction=0.20` and
`CompanionDefendMarginUnits=2` tune anchor switching on the existing one-second
configuration reload. No separate companion enable switch was added. `watch`
and `off` disable substitutions; watch retains observations. Higher enemy
levels still add no threat, alerts, idle pressure or scaling.

Next owner live gate: watch first, then tier 0 with two separated standing
players, each of the four characters in Defend, downed/absent humans, only AI
companions available, and a returning player. Verify three away/return cycles,
KO/revive, a door and save/load, actual formation identity and stance-row
preservation. Check ranged self-defense, host-alone/arena inertness, native
following/warps, observer overhead and teardown. Those outcomes remain UNKNOWN;
this record is not a claim of complete removal of single-player lead dependence.


## Enemy targeting tier 0 review corrections — 2026-10-19

Superseded by the [away-menu diagnostic and tiers 1/1.5/2 candidate](#enemy-targeting-away-menu-diagnostic-and-tiers-1152--2026-10-19); this section retains its original hash-pinned evidence.

Profile: saved-story host, co-op active, opt-in `[StoryAreas]`
`EnemyTargeting=tier0`, `EnemyTargetingCapParity=on`,
`EnemyTargetingLeadRule=always`; master default remains `watch`.
Revision: dirty `fa50351` on `codex/shared-simulation`. Built DLL SHA256:
`c685b39ea9825d69eef55c08f06ddd49bacf448aefda74f264746cb235693bae`.
Supported executable SHA256:
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
This applies [Parts A–D of the tier 0 review](enemy-ai-review-tier0.md);
none of its refuted findings was implemented. No commit, deployment or live
session was performed.

The runtime now derives enemy-targeting seats through `enemy_targeting_seat`,
using `heard_at` packet receipt for freshness and `held = !lease`. The receipt-0
sentinel remains stale. Host-side native input holds no longer feed the AI
classification. Readiness, drain, away, connectivity and ownership derivations
remain; the classifier itself and the dead-lead hold's input logic are unchanged.

**Fail-before/pass-after evidence (CONFIRMED_TEST).** After extracting the
mapping but before changing its two bad inputs, the new
`StorySplitControlTest` case printed:

```text
REGRESSION input_held: fresh heard_at=5000 last_input_at=0 mapped=ai expected=human_remote
```

It then failed `kind==STORY_SEAT_REMOTE` (exit 3). With packet freshness and
lease-only `held`, the same case passes (exit 0), together with stale receipt,
sentinel 1 even inside the startup freshness horizon, draining, no lease,
not-ready and non-HUMAN mutation cases. Retained/drained away keys, unclaimed
AI, host mapping and real worker-thread AwaySeatExact/CompanionSeatExact
refusals also pass. Session/roster providers are synthetic; this exercises the
actual runtime mapping and pure seat classifier, not live packet scheduling.

Logging now distinguishes `substitutions` (selector output replacements) from
`changes` (those replacements choosing a different seat from native self+0xC0).
The latter is a proposed target change, not proof that the subsequent native
state committed it; repeated evaluations before a commit can each increment it.
`lead_substitution` records only these changes, at most once per second across
the adapter and 128 records per roster epoch/revision namespace. Stable
substitutions do not spend that budget. Both counts appear in `status` and reset
with the namespace. No enemy pointer is retained for this bookkeeping.

`type` uses a 128-pair `(resource, mode)` seen-set per namespace, independent of
the per-second `selection` counter flush; saturation suppresses additional type
records rather than evicting entries. A >250 ms service gap clears only Defend
anchors and observation timestamps, preserving selection-log counts/timestamps.
AWAY and REMOTE cap refusals now contribute to the same statistics. The cap
helper has an explicit `regparm(0)` pin; the S1 direct filter recheck is retained.
The [engine contract](engine-functions.md#saved-story-enemy-candidate-filter-supported-image)
records the supported-image address ranges and the precise no-native-state-store
observation. The brief also records the accepted anchor reset after a mod modal
or swap, excluding the vanilla Q menu.

`tools/build-linux.sh` passed with the documented Flatpak/MinGW toolchain.
`git diff --check` and a whitespace check of the changed untracked sources passed.
The focused checks below all passed:

| Check | Evidence and limits |
| --- | --- |
| Wine `SudekiMP.StorySplitControlTest.exe` | CONFIRMED_TEST: regression and runtime mapping mutations above; real worker-thread refusal with otherwise eligible away/companion fixtures. |
| Wine `SudekiMP.StoryEnemyTargetingImageTest.exe <supported SUDEKI.exe>` | CONFIRMED_TEST: Service receives an exact synthetic roster/scene and classifies host, remote, away and native AI; tests both native down sources, inside/same-area absence, native-AI flags, unreadable-mode refusal before sampling/walking, epoch/revision retirement, gap anchor reset with log-budget preservation, host firing and recent-shot expiry with AI/AWAY exclusion. |
| Same image executable | CONFIRMED_TEST: 500 stable substitutions leave the change budget unused; 999/1000 ms lead throttle and 128 limit, once-per-pair type records across report flushes and bounded saturation, 249/250 ms companion throttle and 128 limit, 999/1000 ms known/unknown state observations; selection picks/empty/native_absent fields, stale_count mutation, AWAY cap_refusals, REMOTE/AWAY/AI hit seat_kind/firing, valid/invalid temporary INI reload and invalid_config/level_clamped/owner_gate records. |
| Same image executable | CONFIRMED_EXACT_IMAGE: existing native cap branches and companion builders/selectors, exact-byte/caller refusal and five-stage rollback still pass. After forced popup-restore failure all three adapter seams are restored while installed/dependencies remain retained, subscribed/admitted are false and rebinding refuses; restoration retry/reinstall passes. Foreign-thread/foreign-id adapter and shared-observer binding refuse. Non-cap world and ownership providers remain synthetic. |
| Wine `SudekiMP.LanArenaHitFeedbackTest.exe` | CONFIRMED_TEST: worker Subscribe uses a vacant slot, worker Unsubscribe targets a subscribed consumer, and worker Uninstall runs with no consumers, isolating thread refusal. Foreign-id/thread BindThread refusal, HostSnapshot first-call binding and second-thread inertness, nested capture/busy removal and retry tests pass. |
| Wine `SudekiMP.StoryThreatTest.exe` and native GCC C11 `story_threat_test.c` | CONFIRMED_TEST: existing seat, cap, Lead-PC and Defend policy checks; native build uses `-Wall -Wextra -Werror`, `-Isrc`, `src/network/story_threat.c` and `-lm`. |

**Full-suite comparison.** The same pre-change and post-change Wine sweep ran
222 invocations across all 220 built executables ending in `Test.exe` or
`Probe.exe`: baseline 218 passed; the initial post-change sweep had 217 passes.
Its additional `LauncherTabsTest` failure was `CreateDirectoryW(settings, NULL)`:
the existing test's PID-named temporary directory collided with prior test
output. The unchanged executable passed on a targeted retry with a fresh,
isolated `TMP`/`TEMP` directory. After that retry, 218 of 222 cases pass and the
same four non-pass results/exit codes remain. No launcher source was changed
and no prior fixture directory was deleted. Supported-image tests received the
supported EXE; SOL shop/interior
fixtures received the supported GEX where required. AreaFinalise used `normal`;
AreaWorker ran `idle`, `active` and `flush`. This is the full built-binary sweep,
not every optional command-line scenario or a live integration session.

| Unchanged non-pass | Exit | Observed limitation |
| --- | --- | --- |
| `SudekiMP.DirectInputProbe.exe` | 1 | No connected controller. |
| `SudekiMP.XInputProbe.exe` | 1 | No connected controller. |
| `SudekiMP.LanArenaLoopbackPeerTest.exe` | 2 | Standalone sweep lacks the paired role/port harness; prints usage. |
| `SudekiMP.LauncherModsUiTest.exe` | 3 | Standalone sweep lacks its external fixture-directory argument. |

After the documented fixture-directory retry, the comparison leaves no new
non-pass executable. These are
build, policy and supported-image fixture results only. Native movement,
combat-state commits, input timing, visible target retention, performance and
live teardown remain UNKNOWN. The owner live gate remains the castle-road
Ailish-host/Tal-client ambush: Skill Strike, cast and item holds; host KO;
all-away fallback/return; AWAY refusal logs; shop separation;
disconnect/title/rejoin and teardown; host-alone and arena inertness. No live
behaviour claim is made by this record.


## Enemy targeting away-menu diagnostic and tiers 1/1.5/2 — 2026-10-19

Profile: saved-story host, co-op active; `[StoryAreas] EnemyTargeting` defaults
to `watch`, with opt-in cumulative `tier0`, `tier1`, `tier15`, `tier2` and
`EnemyTargetingCapParity=on`. Revision: **dirty, uncommitted `fa50351`** on
`codex/shared-simulation`. Final DLL SHA256:
`bbf23b04749a5ac77b11c29e52a4d0393525914b36db3cabaf2bf2f756dc41db`.
Supported EXE SHA256:
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
This implements [Parts 0–D of the tier brief](enemy-ai-tier12-task.md).
No commit, deployment or live game run was performed.

**Part 0, delivered separately before the tiers.** The owner identified host
Esc-menu opening during the last client's away state as the trigger matching
AwaySeatExact's menu clause. The candidate removes only that clause;
CompanionSeatExact retains its own menu guard. AwaySeatExact delegates to the
single named-refusal body and accepts exactly `STORY_AWAY_REFUSAL_NONE`.
Connected-away refusals log on reason change per seat, bounded to 64 records
per epoch/revision namespace. The unused `menu_captures` enum value remains
available by name but is never returned by the away proof.

The Part-0-only DLL was built and preserved before tier changes, SHA256
`e23fc2071c742882a63027cf6f5078f7ed9e3bcb3b93d090146024c350691320`.
Its build and Wine `StorySplitControlTest` passed. **CONFIRMED_TEST:** every
split-control away mutation checks the named refusal and agreement with
Exact; null key, out-of-range/host/unclaimed characters, zero token, session
absence and real worker-thread refusal are included. The menu mutation accepts
AwaySeatExact while an otherwise available, unclaimed companion still refuses.
The runtime mapping keeps an accepted away key/co-op admission with the menu
open. Repeated same-reason samples emit one diagnostic, alternating refusals
stop at 64, and epoch/revision changes renew the budget. This establishes
synthetic admission, not the live `status active` outcome. The owner must
rerun the last-away case with Esc open and closed and inspect any remaining
`away_refused` reason, including `binding_not_ready`.

**Tier implementation.** Shared decaying damage/sniper memory and separate live
and shadow alert/push sets use serial identities and wrap-safe millisecond
clocks. Higher-tier picks name only humans (already accepted decision 10).
One active alert per shooter per set includes orphan copies (already accepted
decision 11); refused second raises preserve that enemy's cooldown. The walk
counts leased/unleased targeters from both partitions and treats incomplete
observations as unknown. Only modes 0/0xB can replace `*out`, after the direct
native filter recheck. Mode 0xE only reports a blocked live designation.
Companion Defend and S2 cap behavior retain their accepted semantics.

`owner_gate` is retired in this candidate; `level effective=...` records the
stored effective level and state clears on a change. Cap-parity off clamps
higher tiers to tier 0. Selection records expose `would_lead`, `would_threat`,
`would_alert`, `would_push`. **Watch counters use the native-cap candidate
list**, since parity is off in watch; compare them at tier 0 before evaluating
higher tiers live. Shadow evaluation makes no direct filter calls and no
native writes. Each alert/push event family has a 64-record namespace budget,
shared across live/shadow; `threat_substitution` has the 1 s / 128-change limit.

`tools/build-linux.sh` passed using the documented Flatpak/MinGW toolchain.
The four focused Wine tests and native C11 policy test passed. A new exact
float-equality assertion failed in the 32-bit policy build while passing
natively; using a tolerance for the HP ratio passed both builds. Final
`git diff --check` and the changed-untracked-file whitespace check passed.

| Check | Evidence reached and limits |
| --- | --- |
| Native GCC C11 `story_threat_test.c` with `-Wall -Wextra -Werror`, and Wine `SudekiMP.StoryThreatTest.exe` | **CONFIRMED_TEST:** admission and human-kind mutations, default/custom decay, distance-weighted floor/stickiness, hit-all and each sniper guard, list/sequence predicates, raw-threat recruitment with ties and capacity clamp, ownership-matched copies and orphan fades, per-shooter alert refusal, idle tracking, every push end reason, arbitration and signed/unsigned clock wrap. These are plain-data tests, not native hook or gameplay proof. |
| Wine `SudekiMP.StorySplitControlTest.exe` | **CONFIRMED_TEST:** Part 0 named clauses/menu behavior/log bounds above, existing heard_at/lease-only-held regression and otherwise eligible worker-thread Away/Companion refusal. Session/native services remain synthetic. |
| Wine `SudekiMP.StoryEnemyTargetingImageTest.exe <supported SUDEKI.exe>` — new tier scenarios | **CONFIRMED_TEST:** a multi-lease, exact synthetic roster drives Service and actual hit-observer/selector adapters. Threat, alert and push replace output only through the real S2 filter, whose non-cap world predicate and list builder are synthetic. Recheck failure/fallback, repeated holds/expiry, fresh DOWN, excluded/unleased units, mode-1/0xE refusal and lock counting, nearest-any-partition sniper gating, bad positions, overflow/failed lease observation, hit distance mapping, unslotted/ally victims, unleased second engagement, close boundary, post-start membership, >250 ms/non-admitted idle reset and a future-stamp non-gap, unrelated-hit Touch, retirement/namespace/co-op loss, each Aware filter, independent sets, watch read-only counters and ini transition/clamp all pass. Request/unit byte comparisons show no writes in those fixtures; native state commits and visible pursuit remain unproved. |
| Same image executable — event bounds | **CONFIRMED_TEST:** repeated actual hit/Service/manager-retirement paths count exactly 64 each of alert, alert_end, alert_retired, responder_released, push and push_end, then reset on namespace change. Stable higher substitutions do not spend change budget; 999/1000 ms and 128-record boundaries pass. The >250 ms companion test observes an immediate new companion_state sample, preserving its log budget. |
| Same image executable — retained native contracts | **CONFIRMED_EXACT_IMAGE:** supported-byte/caller validation, selector ABI and native cap/companion routines, changed-byte refusals, transactional rollback, busy/wrong-thread teardown, retained dependencies after failed restoration, retry and reinstall still pass. World/ownership providers remain synthetic. This evidence applies to those native contracts, not the synthetic higher-tier target choices. |
| Wine `SudekiMP.LanArenaHitFeedbackTest.exe` | **CONFIRMED_TEST:** shared-observer registration/thread, nested/busy removal and retry regressions pass. No LAN arena targeting changes or live combat run. |

**Full suite.** The same 222-invocation manifest across all 220 built
`Test.exe` / `Probe.exe` targets completed, using supported EXE/GEX arguments,
AreaFinalise `normal` and AreaWorker `idle`, `active`, `flush`. The sweep
reported 217 passes and a LauncherTabs timeout in addition to the four known
non-passes. Unix TMP/TEMP overrides were ignored by Wine. A retry set those
variables inside Windows to a fresh fixture directory; the unchanged UI test
then reached its own "Save your changes before quitting?" dialog and waited.
A temporary external harness identified that test's process/window and declined
only that exact save prompt. The test finished successfully (exit 0). No
launcher source or pre-existing fixture was changed. Thus the reconciled result
is **218/222**, with the same four non-pass names and exit codes as the prior
review baseline; the launcher pass is explicitly a UI-assisted fixture result,
not an unattended-suite pass. After final guard isolation and the signed service-gap correction, the final
DLL above received another full sweep with that same test-only prompt handling.
It reported 217 passes: `LanArenaProtocolTest` encountered Wine's
`recvmsg: Connection reset by peer` before its test ran; the unchanged binary
passed immediately on retry (exit 0). Final reconciled result: **218/222**,
with the baseline's same four names and exit codes. LauncherTabs passed in this
final sweep using its fresh Windows temporary directory without invoking the
optional dialog handler. All four focused tests and native C11 policy tests
also passed against the final sources.

| Unchanged non-pass | Exit | Limitation |
| --- | --- | --- |
| `SudekiMP.DirectInputProbe.exe` | 1 | No connected controller. |
| `SudekiMP.XInputProbe.exe` | 1 | No connected controller. |
| `SudekiMP.LanArenaLoopbackPeerTest.exe` | 2 | Standalone invocation lacks its paired role/port harness. |
| `SudekiMP.LauncherModsUiTest.exe` | 3 | Standalone invocation lacks its external fixture-directory argument. |

This is the full built-binary manifest, not every optional command-line variant
or a live multiplayer session. The four non-passes are not successful tests.

Evidence stays **CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE** within the scopes
above; the dirty feature remains **RESEARCH-ONLY / UNVERIFIED_DIRTY_WORK**.
Native movement, state commits, responsiveness, balance, performance and live
teardown remain UNKNOWN. No tier 3, mode-0xE substitution or bark is included.
Owner gates remain cumulative after a passed tier 0 gate: menu-open/closed
last-away rerun; tier-1 hit/decay back to the distance rule (possibly still Tal);
tier-1.5 up to two ranged and one melee within alert_hold, noticeable on the
shooter's screen; tier-2 push after idle_seconds. Include host-alone/arena,
excluded/absent seats, disconnect/title/rejoin and teardown checks.
