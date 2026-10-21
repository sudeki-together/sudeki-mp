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
AwaySeatExact while the same away-reserved companion still refuses
CompanionSeatExact. The unclaimed-companion menu case was not asserted in
this artifact; it is covered by the review addendum below.
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
| Native GCC C11 `story_threat_test.c` with `-Wall -Wextra -Werror`, and Wine `SudekiMP.StoryThreatTest.exe` | **CONFIRMED_TEST:** admission and human-kind mutations, default/custom decay, distance-weighted floor/stickiness, hit-all and each sniper guard, list/sequence predicates, raw-threat recruitment with ties and capacity clamp, ownership-matched copies and orphan fades, per-shooter alert refusal, idle tracking, every push end reason, arbitration and deadlines queried after the signed/unsigned clock boundaries. Straddling queries and pairwise end-reason precedence were not asserted in this artifact; see the review addendum. These are plain-data tests, not native hook or gameplay proof. |
| Wine `SudekiMP.StorySplitControlTest.exe` | **CONFIRMED_TEST:** Part 0 named clauses/menu behavior/log bounds above, existing heard_at/lease-only-held regression and otherwise eligible worker-thread Away/Companion refusal. Session/native services remain synthetic. |
| Wine `SudekiMP.StoryEnemyTargetingImageTest.exe <supported SUDEKI.exe>` — new tier scenarios | **CONFIRMED_TEST:** a multi-lease, exact synthetic roster drives Service and actual hit-observer/selector adapters. Threat, alert and push replace output only through the real S2 filter, whose non-cap world predicate and list builder are synthetic. Recheck failure/fallback, repeated same-tick alert selections and push hold/expiry through Service, fresh DOWN, excluded/unleased units, mode-1/0xE refusal and lock counting, nearest-any-partition sniper gating, bad positions, overflow/failed lease observation, hit distance mapping, unslotted/ally victims, unleased second engagement, close boundary, post-start membership, >250 ms/non-admitted idle reset and a future-stamp non-gap, unrelated-hit Touch, alerter/responder retirement, namespace/co-op loss, Aware filters tested by derived-table mutations, independent sets, watch read-only counters and ini transition/clamp all pass. Request/unit byte comparisons show no writes in those fixtures; native state commits and visible pursuit remain unproved. |
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

## Enemy tiers 1/1.5/2 review addendum — 2026-10-19

Scope: all 16 items in [the tier review](enemy-ai-tier12-review.md), saved-story
host/co-op profile and the same cumulative opt-in levels. `watch` remains the
default. Candidate base: **dirty, uncommitted `27f7296`** on
`codex/shared-simulation`. HEAD advanced externally during this pass; the eight
review files' starting contents exactly match that commit. This pass made no
commit, deployment or live game run. Candidate DLL SHA256:
`009d285e7bce91d3bed2b26f1cf78fd221e851159bb8a6793a517d137c4e4d82`.
Supported EXE SHA256:
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

**Item 1 changes behavior conditionally.** A leased, live pushed enemy that is
targeting its own seat now remains in the frame table when its position read
fails, with `position_unknown` and NaN coordinates. Maintenance can subtract
its own engagement instead of ending the push as `engaged`. Distance-based
recruitment, close checks and Aware searches skip that unknown position. The
synthetic test keeps the push until expiry; another applicable end condition
can still end it earlier. This changes a live tier-2 push if that position-read
failure occurs (and shadow bookkeeping at lower/watch levels). Whether the
owner's live run is exercising that condition is **UNKNOWN**. The original
owner live-gate DLL, SHA256
`bbf23b04749a5ac77b11c29e52a4d0393525914b36db3cabaf2bf2f756dc41db`,
is preserved separately in `build/artifacts/tier12-owner-bbf23b04/SudekiMP.dll`.
The candidate is separately preserved in
`build/artifacts/tier12-review-009d285e/SudekiMP.dll`; no installed artifact was
replaced by this pass.

**Discriminating evidence: CONFIRMED_TEST.** The following assertions pass in
the unmodified candidate. Isolated source copies outside the worktree were
compiled with each listed mutation and executed: **31 native policy mutants
and 22 image/split-control mutants all failed assertions**, with no compile
failure counted as a test rejection. Native, image and split-control mutation
controls all passed. The mutation run never installed a mutant DLL.

| Review item | Assertion/fixture and rejected mutation |
| --- | --- |
| 1 | `tier_unknown_push_position_test`: broken position back-pointer with own engagement retains the push through `until - 1`, expires at `until`, and never logs `engaged`. Restoring `lease && positioned` fails the active-push assertion. Four additional mutants removing each unknown-position distance guard fail; finite-coordinate probes isolate the explicit guards from NaN arithmetic. |
| 2 | `deadline_straddle_tests`: push/alert expiry and both cooldown gates straddle 2^32 and 2^31 in each direction, with not-due/due assertions. Both plain unsigned `now >= until` and signed absolute comparisons fail. |
| 3 | `predicates_and_pick_tests`: current .045 or .05 versus best .055 selects best despite the sticky margin. Removing the current-above-floor clause fails. |
| 4 | `tier_seam_tests`: ten successful alert substitutions interleaved with 100 ms Service passes remain recruited; the existing forced failed recheck then releases the responder. Setting `recheck_failed` on success fails after maintenance. |
| 5 | `tier_push_retirement_tests`: manager removal and departed lease independently retire the pushed record and log `push_end reason=invalid`. Replacing the adapter's `r != NULL` with 1 fails each case; the second mutant run puts lease departure first to isolate it. |
| 6 | `tier_walk_filter_tests`: native +0xF4 empty and +0xC0 occupied independently prevent Aware pushes; name-only and resource-only exclusions prevent both push and recruitment at tier 2. Dropping either awareness input or either exclusion source from walk derivation fails (four mutants). Awareness is a push requirement, not an alert-recruitment requirement; the accepted alert policy was not changed. |
| 7 | `tier_hit_mapping_tests`: ordinary far ranged hit is the positive control; non-ranged, Spirit, unknown Spirit classification and non-firing hits each add .3 threat and zero sniper. A feed spy calls the real pure helper and asserts ranged/known/hit-all/firing arguments as well as memory results. Each argument forced separately to 0 or 1 fails (eight mutants). The spy makes hit-all=0 distinguishable despite Spirit also disabling the ranged flag. |
| 8 | `review_commit_and_group_tests` checks 0/0, 0/7, 7/7, 7/8 through `SudekiMpStoryThreatSameGroup`; `tier_zero_group_test` asserts responder order 3,4,1 for mixed zero/nonzero groups. Removing the nonzero rule fails both pure and image fixtures. |
| 9 | `reason_precedence_tests`: all 15 pairs for orphan/shared alert reasons, 14 realizable pairs for AlertFade, and all 10 push pairs assert the first reason. All 15 alert-order swaps and all 10 push-order swaps fail. AlertFade derives close only from remaining responders, so close/all-left cannot coexist there; the orphan/shared-rule assertion covers that pair. |
| 10 | `review_commit_and_group_tests`: an idle valid alerter, valid non-NULL arrays, and count zero refuse without mutation; count one succeeds. Removing only `!count` fails. |
| 11 | `push_and_decide_tests`: an otherwise eligible active push falls through to threat at TIER15 and wins again at TIER2. Weakening the Decide push gate to TIER15 fails. |
| 12 | `away_targeting_admission`: available unclaimed character 0 accepts before/after menu capture and refuses while capture is active. Moving the companion menu guard below the unclaimed early return fails. |
| 13–16 | The brief now labels unchanged mode-0xE output **CONFIRMED_TEST** and the no-list-read/no-direct-filter rule **CONFIRMED_STATIC**. Decision 11 names the next alerter's cooldown. The public policy note preserves AI/AWAY access through Decide (d)'s tier-0b lead branch. The preceding hash-pinned record's menu, clock, hold and walk-filter claims were narrowed to their actual assertions; this addendum records the stronger tests. These wording corrections add no runtime behavior. |

**Build and focused checks.** `tools/build-linux.sh` passed with the documented
Flatpak/MinGW toolchain. Native GCC C11 policy tests passed with
`-std=c11 -Wall -Wextra -Werror`. Wine `StoryThreatTest`,
`StoryEnemyTargetingImageTest <supported SUDEKI.exe>`, `StorySplitControlTest`
and `LanArenaHitFeedbackTest` all passed against the final sources.
`git diff --check` passed. The required AST-only `graphify update .` completed;
its existing partial-extraction warnings remain navigation limitations, not
code evidence.

**Full suite:** the same 222 invocations across 220 built `Test.exe` /
`Probe.exe` executables completed with **218 passes and the same four known
non-passes**, identical names and exit codes to the baseline:

| Non-pass | Exit | Existing limitation |
| --- | --- | --- |
| `DirectInputProbe` | 1 | No connected controller. |
| `XInputProbe` | 1 | No connected controller. |
| `LanArenaLoopbackPeerTest` | 2 | Standalone invocation lacks the paired role/port harness. |
| `LauncherModsUiTest` | 3 | Standalone invocation lacks the external fixture-directory argument. |

LauncherTabs used a fresh Windows temporary fixture directory and the existing
external harness declined only that test process's exact save-on-close prompt.
Its exit 0 is a **UI-assisted fixture result**, not an unattended pass. No test
source, owner launcher or existing fixture was changed for that assistance.
No new suite failure or retry was needed. This manifest is not every optional
command-line variant, and the four non-passes remain unsuccessful checks.

The supported-image fixture also retained its byte/ABI/native-cap contracts,
transactional installation, wrong-thread/busy teardown, restoration-failure
retention, retry and reinstall assertions (**CONFIRMED_EXACT_IMAGE** for those
contracts only). Synthetic roster/lease/world providers and assertions about
higher-tier choices remain **CONFIRMED_TEST**. Visible pursuit, native state
commits, balance, performance and live teardown remain **UNKNOWN**. The feature
remains research-only; the owner's cumulative live gates are unchanged. No
tier 3, mode-0xE substitution or bark was added.

## Step-away stage 1.1 — 2026-10-20

Profile: saved-story co-op, stage-1 step-away enabled. Code: dirty `27f7296`
on `codex/shared-simulation`, retaining the preceding enemy-AI review work.
Candidate DLL SHA256: `dd917ee5601fc2a479c2eb22329355e009247933ceb4d381f4ce1500f0e5b80d`. Supported executable SHA256:
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
No commit, deployment, game launch, or new live behavior claim.

The documented `tools/build-linux.sh` build completed all targets. The final
focused run passed 16 tests. The final full Wine suite passed
221/225 invocations (223 executables). The four known
non-pass cases are unchanged: DirectInputProbe and XInputProbe (no controller),
LanArenaLoopbackPeerTest (paired arguments required), and LauncherModsUiTest
(external fixture required). LauncherTabs used its isolated temporary fixture
and only its own save-on-close prompt handling. Earlier failures during this
pass exposed fixture setup/dependency issues and the too-narrow pending-swap
retention guard; the final run supersedes them.

76/76 isolated mutations were rejected by assertions
or explicit fixture checks; compiler/link/launch failures were not counted.
The matrix covers the listed helpers and fixtures, not all runtime wiring (see the subsequent castle/stage-1.1 review). It covers away hold, PREPARE ACK/input, holding refusal, authoritative
away state, menu/fence routing, Esc close, no-op request ID, released-key epoch
receipt, retention exclusions, parking identity/counters/depth/thread/teardown,
return fade/state/snap/unpark/exit/H1b, TEMP bounds and membership, cast rebase,
retire and host fallback, presentation reseed, sky family/role/replay/bounds,
and TPtr stack/link/cycle/bound checks. Actor/group parking guards have
redundant witnesses: removing only one witness survived, so the discriminating
mutants remove the complete logical identity guard and use a structurally valid
foreign group. No production mutant was installed or deployed.

The isolated mutation names below identify the exercised regressions. Each
row uses the same unmodified fixture that passed in the focused run.

| Fixture | Rejected mutations |
| --- | --- |
| `story_step_away_host_test.c` | `A1-away-disarms`, `A4-no-hold`, `A5-host-stale` |
| `story_step_away_runtime_test.c` | `A2-no-prepare-ack`, `A2-ready-acks`, `A2-ignore-away`, `A2-drive-ready`, `A6-ignore-fence`, `B5-reseed-ready`, `B5-reseed-area`, `B5-reseed-mask`, `B5-reseed-generation`, `B5-region-exact`, `B5-region-epoch`, `B5-region-revision`, `B5-region-mask`, `B5-region-region`, `B5-region-unknown-lead`, `B5-region-unknown-candidate` |
| `story_step_away_menu_test.c` | `A7-menu-open` |
| `party_ownership_test.c` | `A8-noop-id` |
| `story_split_control_test.c` | `B1-lead`, `B1-unavailable`, `B1-swap`, `B1-assignment`, `B1-peer-phase`, `B1-transport`, `B1-held-overlap` |
| `story_split_preflight_test.c` | `B2-away-retain`, `B2-depth`, `B2-actor`, `B2-group`, `B2-away-resumes`, `B2-unsettled`, `B2-thread`, `B2-underflow`, `B2-uninstall-record`, `B2-fullparty-foreign`, `B2-no-refusal-bound` |
| `story_away_interior_test.c` | `B3-split`, `B3-drain`, `B3-house`, `B3-downed`, `B3-fade`, `B3-state`, `B3-unpark`, `B3-snap`, `B3-inside-exit`, `B3-h1b-unknown` |
| `story_temporary_context_test.c` | `B5-lead-only`, `B5-ready-clears-transition`, `B5-no-unknown-temp`, `B5-no-unknown-bound`, `B5-membership-epoch`, `B5-membership-revision` |
| `story_cast_admission_test.c` | `B5-no-retire`, `B5-no-rebase`, `B5-rebase-identity`, `B5-midstrike-retire` |
| `spirit_instance_abi_test.c` | `B5-no-host-camera`, `B5-owned-camera`, `B5-swapped-globals`, `B5-no-host-filter`, `B5-no-entry-recovery` |
| `story_sky_light_image_test.c` | `D-reset`, `D-family`, `D-client`, `D-replay`, `D-dome`, `D-budget` |
| `tptr_audit_test.c` | `E-stack`, `E-own`, `E-links`, `E-cycle`, `E-bound` |
| `skill_trace_image_test.c` | `A3-zero-released` |

The following files changed for this pass (earlier dirty enemy-targeting files
were preserved):

| File(s) | Change and verification scope |
| --- | --- |
| `CMakeLists.txt` | Registers the new death-hold, menu and TPtr walker fixtures. |
| `src/hooks/lan_story_runtime.c`, `.h` | Authoritative tri-state away query; PREPARE ACK/input hold; bounded request logs; dead-lead retention; existing READY/menu M gates; held/away split masks; return transaction present in source (the runtime service was not exercised in this pass); banner; cast retirement/witness; membership and presentation reseed. Synthetic runtime/host/split tests. |
| `src/hooks/control_separation.c` | Keeps released keys through epoch rebind; real SkillTraceImageTest fixture and zeroing mutation. |
| `src/hooks/lan_story_menu.c` | Closes successful Esc Away/Return; actual menu command fixture. |
| `src/hooks/lan_story_split.c`, `.h` | Away retention, recorded native pause increments, guarded park/unpark, balanced FullParty and refusal to uninstall retained records. Actual owner with native doubles. |
| `src/hooks/lan_story_remote_interior.c`, `.h` | Parked shop Return, outside story-house Return, already-inside away AI flag, return cancellation and closed unknown entry gate. Actual coordinator with inert snap/placement functions. |
| `src/hooks/lan_story_cast.c`, `.h` | Fresh superseded identity proof, rebase, positive-drain retirement/rebind. Synthetic namespace lifecycle. |
| `src/hooks/lan_story_observer.c`, `.h` | Bounded vanilla transition witness, lead-only first READY membership and exact revision setter. Synthetic helper and existing exact-image TEMP regression. |
| `src/engine/spirit_instance_abi.c` | Narrow host camera/filter fallback under neutral globals; synthetic ABI fixture exercises owned/swapped/operation refusal and entry proofs. |
| `src/hooks/lan_story_sky.c`, `.h` | Explicit client role, reset on reinstall, local override re-arm, bounded client logs, no shadowing of `applied`. Existing exact-image hook install/restore plus ledger fixtures. |
| `src/ui/story_away_banner.c`, `.h` | Optional inside-place message through the existing banner renderer. Existing layout tests; pixels remain an owner gate. |
| `src/cleanroom/engine.c`, `.h` | Read-only current-stack reference audit before cleanup/destruction; one suspect report. No cleanup or name-lookup change. |
| `src/engine/tptr_audit.h` | Pure bounded intrusive-list walker; synthetic linked nodes. |
| `src/loader/dllmain.c` | Independently opt-in, default-false `[StoryAreas] TPtrAudit` configuration. Static wiring only. |
| `tests/story_step_away_host_test.c` | Remote actor recorder; away hold/drain, authoritative state and host admission cases. |
| `tests/story_step_away_runtime_test.c` | Actual ACK/intent/M coordinator and presentation reseed conditions. |
| `tests/story_step_away_death_hold_test.c` (new) | Actual death and pending-rotation predicates. |
| `tests/story_step_away_menu_test.c` (new) | Actual successful/refused Esc commands and menu state. |
| `tests/party_ownership_test.c` | Asserts unchanged intent consumes the request ID without revision churn. |
| `tests/lan_party_control_image_fixture.c` | Prior released key still drains after epoch-11 rebind. |
| `tests/story_split_control_test.c` | Runtime `(ready, held, away)` recorder and exclusion cases; preceding enemy-seat tests retained. |
| `tests/story_split_preflight_test.c` | Actual split entry/parking, native pause doubles, identity/depth/thread/counter guards and lifecycle. |
| `tests/story_away_interior_test.c` | Actual return wait/placement/failure/cancel/inside-away/eviction and H1b gate. |
| `tests/story_temporary_context_test.c` | Controlled-clock TEMP/EXIT bounds, first READY membership and widening fences. |
| `tests/story_cast_admission_test.c` | Rebase without fault, retained effects prevent retirement, later retirement and rebind. |
| `tests/spirit_instance_abi_test.c`, `tests/cast_light_abi_test.c` | Transient witness branches; host camera/filter continuation versus owned/swapped refusal. |
| `tests/story_sky_light_image_test.c` | Reinstall/role/family/replay/dome/log bounds and original native hashes for all five TPtr audit sites. |
| `tests/tptr_audit_test.c` (new) | Consistent list, own-stack exemption, foreign stack, object/back-link errors, cycle, bound and unreadable node. |
| `docs/coop-story-plan-54.md`, `docs/coop-story-research-54.md` | Retires the farmhouse take-along claim and dependent proposals; new §9 evidence and A8 correction. |
| `docs/step-away-stage1.1-audit.md` (new) | Release/load/reoffer, intrusive reference and thread audit, exact-image scope and unresolved castle writer. |
| `docs/testing.md` | This hash-pinned verification record. |

Evidence is **CONFIRMED_TEST**, **CONFIRMED_STATIC**, and the specifically
identified **CONFIRMED_EXACT_IMAGE** contracts only. See the new §9 entry and
[ordered owner acceptance gates](step-away-stage1.1-task.md#acceptance) for
remaining live checks. In particular, the castle crash is not claimed resolved,
parked-body vulnerability remains UNKNOWN, and no stage-2 behavior was added.

`graphify update .` completed after the code changes (AST-only). It reported
34 files with partial syntax extraction and no nodes for `hooks.json`; the
graph remains a navigation aid, not validation evidence. No semantic/API
refresh was run.

## Castle capacity / stage 1.1 review — Part A, 2026-10-20

2026-10-20: Superseded for DLL `67c8271d6ef3a69f7b1883fdceb195e9a8b0338af24c0bc321d556235e2241ca` by [Castle Q2 pose omission and bounded hold](#castle-q2-pose-omission-and-bounded-hold--2026-10-20); earlier DLLs keep this gate.

Saved-story co-op, dirty `27f7296` on `codex/shared-simulation`. Previous
stage-1.1 DLL: `dd917ee5601fc2a479c2eb22329355e009247933ceb4d381f4ce1500f0e5b80d`.
Separate Part A candidate SHA256: `264d42a82a27552a4307108fa74f0769c984ac97f2b325705dc25894a4e199c9`,
retained at `build/artifacts/castle-part-a-264d42a8/SudekiMP.dll` with
`part-a.sha256`, `a-diff.txt`, `a-final.diff` and `part-a-source/` beside it.
`build/mingw32/bin/SudekiMP.dll` is the final A+B+C build (the latest review
candidate below); the preceding final candidate is preserved at
`build/artifacts/castle-final-66e851b6/SudekiMP.dll`.
Supported executable SHA256 remains
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
This record is **CONFIRMED_TEST / CONFIRMED_STATIC**, with no deployment,
gameplay run or claim that the tunnel freeze or castle crash is resolved.

The documented Flatpak build passed. Final focused LanPartySessionTest,
StoryWorldCapacityTest and StoryRangedProjectionTest passed. The full Wine
suite passed 222/226 invocations with the same four environment-dependent
non-pass cases listed above. The final focused rerun explicitly enables
assertions in the new session helper; the earlier release build had disabled
them and does not establish its reassembly assertions.

The actual world adapter runs against 150 synthetic registry entries, far
low-priority entries first and party last: 128 sorted actors, two ambiguous
copies omitted across the boundary, wrapperless scenery excluded
(**CONFIRMED_STATIC** for this historical hash; discriminating test in the
review follow-up below), 5 m stable replacement, real codec, client apply with synthetic setters and a different
lead plus extra local entries. Actual Begin/FinishRecruitment retain 127
survivors. The session helper drives the actual authenticated receive path
through all 16 chunks; catch-up round-trips 128 actors. Previous and future
wire versions refuse; profile 27 refuses and 28 admits.

| Fixture | Rejected isolated runtime mutations |
| --- | --- |
| World capacity | Capacity refusal; registry-order continue; registry-order candidate cap; pure per-frame rank; independent client rank; omitted-NPC release removed; client-seen guard removed; NPC target failure refuses; door target failure refuses; recruitment bound 64; uint64 survivor mask; ambient exclusion bound 128; diagnostic index lost; omission namespace reset removed |
| Session | Eight-bit assembly mask; old gameplay profile; future world version accepted |

17/17 mutations were rejected by fixture assertions/checks, not compile or
launch failures. Separately, reverting MAX_CHUNKS to 8, catch-up MAX_SIZE to
11000, or MAX_FRAGMENTS to 11 fails the intended compile-time capacity assert.
No mutant enters the shared build. The ambient fixture uses the real ownership
list and refresh/lookup with a late posed renderer after 128 wrappers.

Measured i686 `-O2 -g -DNDEBUG -fstack-usage` totals (bytes):

| Function | 64-actor baseline | Part A 128-actor candidate |
| --- | ---: | ---: |
| WorldCapture | 68,368 | 103,440 |
| WorldFinishRecruitment | 57,984 | 83,600 |
| WorldPrepare | 41,152 | 49,152 |
| WorldBeginRecruitment | 39,520 | 46,192 |
| runtime present | 24,992 | 44,448 |
| runtime service | 13,888 | 23,616 |
| CatchupDecode | 12,512 | 22,240 |

The measured heap session grows from 494,848 to 688,416 bytes; each in-memory
actor is 152 bytes. The new 8192-entry candidate scratch is single-owner
static storage. The service→Capture frames total 127,056 bytes before deeper
callees, against the owner's observed 1 MB game-thread stack. CatchupDecode
stays on the transport worker: it handles plain data only. Catch-up's 20,480
byte buffer / 20 fragments carries a once-per-join snapshot; this accepted
cost does not add that snapshot every host tick. World v13 / gameplay profile
28 is an intentional owner-approved protocol break; peers must match.

Owner-approved visible costs: overflow keeps party, group monsters/allies,
doors/chests, then stable nearby NPC/scenery. A changed set costs one refused
interpolation pair. Omitted NPCs idle locally only after client ownership is
known and may snap to host pose on rebinding (Q1); the shop rule is unchanged.
Q2 covers `target()` failures inside `catalog()`: the non-party target is
omitted and its slot refilled; an omitted door's open state does not mirror
until it binds again. Party identity failures still refuse. This does not
cover non-party pose reads after catalog: host `read_pose_detail` failures
(transform / channel_pose / submodel_pose, with `ranged_pose_fallback` FALSE
for `character>=4`) and `wire_pose_bounds`, or client matched-target
`read_pose`, still refuse the whole frame outside the existing combatant
omission branch. Generic 0x2d5b00 identity/position failures in the catalog
loop and unreadable registry entries also refuse. Entries past castle index
290 have never been evaluated live: reaching these failures is **UNKNOWN**.
Extending Q2 to pose reads is an outstanding owner decision; this review makes
no such code change. Omission summaries and per-entity target lines have
bounded namespace logging.

2026-10-20: Superseded for DLL `67c8271d6ef3a69f7b1883fdceb195e9a8b0338af24c0bc321d556235e2241ca` by [Castle Q2 pose omission and bounded hold](#castle-q2-pose-omission-and-bounded-hold--2026-10-20); earlier DLLs keep this gate.

Owner live gate: Tal away first, then controlled, while Ailish goes through
the tunnel and back. The client keeps presenting with `world_captured=1`
throughout; `world_omitted` appears only when eligible count exceeds 128 or an
entity fails `target()`. Far omitted NPCs idle on the client instead of freezing
(Q1); no `target()`-stage refusal for a non-party entity (Q2). A Q2 omission is
`stage=target_omitted`, not a frame refusal; a real refusal is `event=refused`
with `world_captured=0`. The post-catalog pose, generic pre-pass and unreadable
entry failures above remain outside Q2; their castle reachability is **UNKNOWN**.
Record `apply_mean_us` (`event=spectator`), `world_mean_us`
(`event=presentation_cost`) and `capture_mean_us` (`event=host_frame`) against
the 16 ms / 96 fallback, plus NPC rebind snap and omitted door state. Keep the
crash investigation separate. The previous ~9 ms at
56 actors gives only an **INFERENCE** of ~20 ms at 128, not a measurement.
If the owner's live apply mean exceeds 16 ms, the approved fallback is 96
actors / 12 chunks / catch-up size at least 15,640 / 16 fragments; if 128 has
gone live, also world v14 / profile 29. Restate the tests as 96 kept / 12
chunks / `count=97` refused and this gate as 96; widen the session assembly
mask either way. This candidate remains 128; no silent
fallback was made. The castle-crash writer and live cost remain **UNKNOWN**.

## Castle capacity / stage 1.1 review — final Parts B/C, 2026-10-20

Profile: saved-story co-op, dirty `27f7296` on `codex/shared-simulation`.
Final DLL SHA256: `66e851b67bd69188a666b844732033ce780cf28dc3b60b8f15b59a7ab3d91514`,
retained at `build/artifacts/castle-final-66e851b6/SudekiMP.dll` for the owner live gate.
The separately preserved Part A DLL remains `264d42a82a27552a4307108fa74f0769c984ac97f2b325705dc25894a4e199c9` at
`build/artifacts/castle-part-a-264d42a8/SudekiMP.dll` (hash, diffs and source snapshot alongside it);
its capacity/memory record above is unchanged. The supported executable hash
is unchanged. No commit, stash, reset, reformat, deployment or game run.

The documented Flatpak build completed all targets and `git diff --check`
passed. Across the scoped focused runs, all 20 distinct requested/relevant
fixtures pass: LanPartySession, StoryWorldCapacity, StoryAwayWiring,
StoryTransitionArming, StorySplitControl, StoryStepAwayHost,
StorySplitPreflight, StoryAwayInterior, StoryStepAwayRuntime,
StoryTemporaryContext, SpiritInstanceAbi, TPtrAudit, TPtrCapture,
StoryRangedProjection, StoryCastAdmission, StoryEnemyTargetingImage,
StoryThreat, StoryStepAway, StoryStepAwayTransport and StorySkyLightImage
(all Test targets). The original image-test arguments use the supported EXE;
no executable bytes or game data are published here.

Final full Wine suite: **225/229 invocations pass (227 executables)**.
The same four known non-pass cases remain: DirectInputProbe and XInputProbe
exit 1 without a controller; LanArenaLoopbackPeerTest exits 2 without paired
arguments; LauncherModsUiTest exits 3 without its external fixture. LauncherTabs
uses an isolated temporary fixture and handles only its own save-on-close
prompt. Native C11 `-O2 -Wall -Wextra -Werror` runs also pass for TPtrAuditTest,
StoryStepAwayTest and StoryThreatTest. Graphify AST update completed; its
existing unsupported/partial parser warnings mean the graph remains navigation,
not evidence.

42/42 isolated Part B mutations were rejected by assertions or explicit
checks. This includes two exact pre-fix reproductions: the Part A version of
AwayDrained fails `!parked_body && parks==0` after member_enter with a stale
scene snapshot; the Part A runtime fails the consumed-M / waiting-notice
assertion with client_input_ready=FALSE. No compile/link/launch failure counts
as a rejected mutation. Together with Part A, this pass has 59 rejected
runtime mutations and three separately reported compile-time capacity guards.
Early fixture setup/compile issues are superseded by the passing build/runs.

| Review item | Fixture and rejected mutation / actual proof |
| --- | --- |
| B1 | StoryAwayInterior: `B1-before-stale-frame`, `B1-no-phase-guard`, `B1-skip-inside-unpark`. Uses a distinct frame snapshot; observer stub still updates test_scene. Asserts no repark, failed unpark remains BUSY, successful return shows and clears away_inside. |
| B2 | StoryAwayInterior: `B2-no-ordinary-return`, `B2-hold-unavailable`, `B2-unconditional-early-out`, `B2-scene-before-hold`, `B2-roster-refusal`. Scene-not-READY remains unavailable even while holding; an inexact roster refuses. Ordinary IDLE/holding return has no move/unpark; parked hold is BUSY; already-inside return still shows/clears state. |
| B3 offer and return | StoryAwayWiring calls actual service_story_ownership with TakeCommand and recording seams. `B3-offer-parked`, `B3-ignore-return-result`, `B3-wrong-result-mapping`, `B3-commit-on-refused`, `B3-no-reverse-cancel`. Disconnect/reconnect keeps the park record while away clears; refused/BUSY candidates do not commit. |
| B3 bounds and drains | StoryAwayWiring: `B3-request-unbounded`, `B3-no-drained-call`, `B3-no-tokenless-call`, `B3-skip-activity-drain`, `B3-skip-activity-service`. Request lines stop at 64; both runtime AwayDrained sites and retained activity refusal are exercised. |
| B3 post-service publication | StoryAwayWiring: `B3-post-mask-overlap`, `B3-post-away-zero`. Explicit initialized/known roster, activity/split attempted, ObserverHolding, away reservation, acquired non-draining exact lease. Records the outer SetEligibility arguments, so the real split owner's redundant mask cannot hide a regression. |
| B4 | StoryTransitionArming calls real begin_zone TEMP and EXIT, no split owner or AreaFollow; `B4-no-arming`, `B4-temp-only`. EXIT WorldLoading stays TRUE at +20000 while IdentityUnknown becomes FALSE. StoryCastAdmission: `B4-cast-transient`. StoryAwayWiring: `B4-runtime-no-unknown`, `B4-runtime-no-superseded`. |
| B5 | SpiritInstanceAbi: `B5-foreign-filter`. Foreign controller under update_fault neither passes native filter/edge cleanup nor changes +0x84. |
| B6 | StoryStepAwayHost: `B6-host-swap-ignored`. Real valid RESERVED/HANDOFF transactions with READY/HUMAN control; zeroing swap_involves fails. WAIT_ACK alone no longer stands in for this claim. |
| B7 / original B2 | StorySplitPreflight: `B7-ended-keeps-park`. Parks a member in the actual split owner, calls ended with its record held, then checks cleared actor and !parking_held. |
| B8 | TPtrAudit: `B8-head-only`, `B8-stop-own`. Own-node exemption asserts count; own head → heap → foreign stack chain asserts STACK and count 3. |
| B9 | StoryStepAwayRuntime: `B9-before-input-gate`, `B9-readiness-readded`, `B9-no-esc`. Invalid presentation, input readiness FALSE, selected client and raw M: Esc closed consumes with waiting notice, no open/intent; Esc open consumes without notice. |
| B10 | **CONFIRMED_STATIC** reference-user audit with lifetime verdicts; MOVE/SNAP copy/end ordering retains the reviewer's **CONFIRMED_EXACT_IMAGE** evidence. Documentation-only, no runtime mutation claimed. |
| B11 capture | TPtrCapture includes the actual cleanroom audit and replaces logging with a source-overwriting recorder: `B11-copy-after-log` fails, proving all nodes were copied before the first header. Pure TPtr capture also survives simulated source overwrite. CAS-owned static storage is a source property. |
| B11 telemetry | StoryAwayWiring: `B11-no-default-off`; StoryTransitionArming: `B11-window-unbounded`, `B11-member-owner`; TPtrAudit: `B11-trace-window`, `B11-trace-cadence`, `B11-trace-state-change`, `B11-trace-budget`, `B11-trace-state-four`. Host field reader checks position/pause/override/mode; client cadence/state/budget use the same production plain-data helper. Last-presented position capture and game-service call-site wiring are **CONFIRMED_STATIC**, not client pixels. |
| B12 / C | Historical evidence narrowed in the original §9 entry; research citations annotated after B6 retirement; refined crash evidence and H1/H2 explicitly distinguished. Documentation-only, no crash fix or mutation claimed. |

The current synthetic evidence establishes the runtime return transaction's
result mapping, conditional policy commit, cancellation call and drain wiring;
actual native movement, camera continuity and control timing remain **UNKNOWN**.
On the Kamo Return owner gate, record **press_to_control_ms**: the host's 1.6 s
door fade plus a fresh offer competes with the client's 5 s RETURNING fallback.
Whether live returns ever exceed it is **UNKNOWN**. Keep the brief's remaining
owner gates, including parked vulnerability and controlled tunnel crossings.

All Research54 event kinds share **one 40,000-line LINE_BUDGET per process**;
it is not a separate budget for the party trace. The owner's long runs exhausted
it. TPtrAudit's default-off post-switch rows use their own 20 s window and
64-sample bound (at most 256 member rows per switch), on the game-thread service
seam. Host: once per second; client: spectator !=4 on state change or at most
once per second otherwise, using last presented positions. No per-frame log.
Node capture still cannot distinguish a live caller from dead stack; the audit
frame and VirtualQuery may overwrite near-SP nodes before capture. The owner's
research rule is persistence longer than about 5 ms across lookups, not a single
STACK flag. See [the audit](step-away-stage1.1-audit.md).

File-by-file inventory for this task (earlier unrelated dirty work preserved;
rows marked reviewed include the unfinished Part A already present on resume):

| File | Result in this task |
| --- | --- |
| `CMakeLists.txt` | Four new executable fixtures; actual-session receive helper replaces the session TU for its existing test, explicitly enabling assertions. |
| `src/network/lan_story_world_frame.h` | Reviewed 128 actors, 16 chunks, world version 13. |
| `src/network/lan_story_world_frame.c` | Reviewed coverage static assert; codec regression tests retained. |
| `src/network/lan_story_catchup.h` | Reviewed 20480-byte / 20-fragment capacity. |
| `src/network/lan_party_session.c` | Reviewed 16-bit world assembly mask and gameplay profile 28. |
| `src/hooks/lan_story_world.c` | Reviewed candidate/identity/priority/stability/host-set selection, refill, diagnostic context, Q1 and array-based recruitment; completed per-side/epoch omission budgets and semantic side labels. |
| `src/hooks/lan_story_ambient.c` | Reviewed 8200-entry exclusion list and sorted binary lookup; real adapter exercised above the old bound. |
| `src/hooks/lan_story_remote_interior.c` | Coordinator phase prevents stale-snapshot repark; already-inside return unparks before showing; ordinary return precedes interior gates; holding is BUSY. |
| `src/hooks/lan_story_remote_interior.h` | Return-value contract distinguishes unavailable, busy and admitted. |
| `src/hooks/lan_story_runtime.c` | Esc-only invalid-presentation M gate; default-off bounded switch telemetry and last presented position snapshot on the existing service seam. Existing ownership wiring now exercised. |
| `src/hooks/lan_story_observer.c` | Native-thread background-switch window getter and guarded diagnostic member field reads. Existing vanilla arming now exercised. |
| `src/hooks/lan_story_observer.h` | Diagnostic window/member-read API and plain-data member record. |
| `src/cleanroom/engine.c` | CAS-winner-owned static capture before any audit log; read-only setting getter. Native cleanup unchanged. |
| `src/cleanroom/engine.h` | TPtrAudit setting getter declaration. |
| `src/engine/tptr_audit.h` | Plain-data all-node byte capture helper alongside the existing walker. |
| `src/engine/story_switch_trace.h` (new) | Pure 20 s / 1 s / state-change / 64-sample gate shared by runtime and tests. |
| `tests/lan_party_session_test.c` | Reviewed capacity/catch-up/profile/version cases; invokes the new actual receive helper. |
| `tests/lan_party_story_capacity_fixture.c` (new) | Actual 16-packet authenticated session receive/reassembly and size output. |
| `tests/story_world_capacity_test.c` (new) | Real capture/prepare/apply/recruitment with synthetic registry and native setter doubles, identity/priority/stability/omission/diagnostic cases. |
| `tests/story_ambient_capacity_fixture.c` (new) | Actual ambient refresh, sorted exclusion and late renderer over 128 entries. |
| `tests/story_ranged_projection_test.c` | Reviewed updated Q1 posed-versus-omitted NPC expectation. |
| `tests/story_away_interior_test.c` | Distinct stale frame, ordinary hold return, inside unpark retry and state-clear assertions. |
| `tests/story_step_away_host_test.c` | Shared recording seams and valid RESERVED/HANDOFF READY/HUMAN admission cases. |
| `tests/story_away_wiring_test.c` (new) | Actual ownership service commands, result/commit/cancel/budget, parked reconnect, both drain sites, activity, outer mask and cast witnesses. |
| `tests/story_transition_arming_test.c` (new) | Actual vanilla TEMP/EXIT arming and deadline; diagnostic window and native field reader. |
| `tests/story_step_away_runtime_test.c` | Input-not-ready invalid presentation M and Esc refusal. |
| `tests/story_cast_admission_test.c` | IdentityUnknown transient service branch without fault/rebase. |
| `tests/spirit_instance_abi_test.c` | Faulted foreign filter refuses without changing input filter or invoking native cleanup. |
| `tests/story_split_preflight_test.c` | Actual ended clears a held park record. |
| `tests/tptr_audit_test.c` | Own/heap/foreign stack count, all-node capture and telemetry cadence/bounds, Wine and native C11. |
| `tests/tptr_capture_test.c` (new) | Actual audit capture-before-log ordering, default-off and one-report behavior. |
| `docs/testing.md` | Separate A/final hashes, memory, tests, mutations, costs, limits and this inventory. |
| `docs/coop-story-plan-54.md` | Original evidence narrowed; new A and B/C §9 entries, approved decisions and remaining owner gates. |
| `docs/coop-story-research-54.md` | Historical line references to the retired farmhouse row annotated; no restored take-along premise. |
| `docs/status-matrix.md` | Castle capacity candidate remains RESEARCH-ONLY pending owner observation. |
| `docs/step-away-stage1.1-audit.md` | Missing reference-user lifetime rows, capture caveats, switch telemetry and sanitized crash record; H1 and H2 remain hypotheses. |

`architecture-current.md`, LAN-arena documentation, the task brief, tier 3,
mode-0xE substitution, host step-away and the crash mechanism were not changed.

## Castle/stage-1.1 review follow-ups — 2026-10-20

2026-10-20: Superseded for DLL `67c8271d6ef3a69f7b1883fdceb195e9a8b0338af24c0bc321d556235e2241ca` by [Castle Q2 pose omission and bounded hold](#castle-q2-pose-omission-and-bounded-hold--2026-10-20); earlier DLLs keep this gate.

Applied items 1–19 of `castle-and-stage11-fixes-review.md` on dirty `27f7296`,
`codex/shared-simulation`, saved-story co-op. New DLL SHA256:
`d523d43c7df3fb1c1a4a906f6d5cdde98b2c9fcfddae189a8d5c4014383522db`.
Candidate: `build/mingw32/bin/SudekiMP.dll`, also retained in
`build/artifacts/castle-review-d523d43c/SudekiMP.dll` with `review.sha256`,
`review.diff` and the changed-source snapshot. The owner's preceding live-gate
artifact remains `build/artifacts/castle-final-66e851b6/SudekiMP.dll`, SHA256
`66e851b67bd69188a666b844732033ce780cf28dc3b60b8f15b59a7ab3d91514`.
The reviewer had already preserved Part A; this pass verified both historical
hashes and corrected their documentation, without recopying them.
Supported executable SHA256:
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

**CONFIRMED_TEST / CONFIRMED_STATIC:** documented `tools/build-linux.sh`
Flatpak build and `git diff --check` pass. All 21 focused Wine executables pass:
LanPartySession, StoryWorldCapacity (including StoryAmbientCapacity helper),
StoryAwayWiring, StoryTransitionArming, StorySplitControl, StoryStepAwayHost,
StorySplitPreflight, StoryAwayInterior, StoryStepAwayRuntime,
StoryTemporaryContext, SpiritInstanceAbi, TPtrAudit, TPtrCapture,
StoryRangedProjection, StoryCastAdmission, StoryEnemyTargetingImage,
StoryThreat, StoryStepAway, StoryStepAwayTransport, StorySkyLightImage and
StoryTrailClockImage (all Test targets). Image fixtures use the supported
executable above. Native C11 `-O2 -Wall -Wextra -Werror` TPtrAuditTest,
StoryStepAwayTest and StoryThreatTest pass.

Full Wine suite: **225/229 invocations pass (227 executables)**, with the same
four known non-pass cases: DirectInputProbe and XInputProbe exit 1 without a
controller; LanArenaLoopbackPeerTest exits 2 without paired arguments;
LauncherModsUiTest exits 3 without its external fixture. LauncherTabs uses an
isolated temporary fixture and handles only its own save-on-close prompt.
Graphify AST update completed; existing partial/unsupported parser warnings
remain a navigation limitation. No commit, stash, reset, reformat, deployment
or live game run; no castle freeze/crash or gameplay acceptance claim.

**Behavior change in the owner's current scenario (item 8):** the ownership
switch becomes immediate on the next ambient observation, even within the same
native frame and without clock advancement, instead of up to 250 ms late.
The world adapter increments its ownership serial on initial/rebind publication,
Begin/FinishRecruitment membership changes, a changed local-zone pointer and
successful uninstall. Ambient refresh compares it before the same-frame early
return. A set change still costs **one refused interpolation pair**; NPC rebind
snap and omitted door state remain accepted costs. More than 64 bound
combatants being reachable live is **UNKNOWN** and is not part of the castle
gate. Q2 remains restricted to `target()` omissions: the pose-read and other
whole-frame refusals listed in Part A remain unchanged pending the owner decision.

20/20 isolated runtime mutation cases compile/link and fail at the intended
assertions or explicit CHECKs (18 assertion exits, two explicit-check exits).
No launch/build failure counts as runtime discrimination. Item 17's two
compile failures are separate **CONFIRMED_STATIC** hardening evidence.

| Review item | Actual fixture and rejected mutation |
| --- | --- |
| 1–7 | Documentation/source audit only: durable artifact locations, dirty status and A5/fallback gate, bounded Q2 scope, dated owner H2 citation, once-per-process audit latch and conditional target-refresh traffic. No runtime mutation claimed. |
| 8 | StoryWorldCapacity / StoryAmbientCapacity: bind NPC A, omit B, then apply a real rebind with B bound and A omitted at the same fixed clock/frame. Time-only refresh fails `slot_a==frame_dt_bits && slot_b==0`. Begin/Finish serial increments and unchanged/changed zone-pointer serials are also asserted. |
| 9 | StoryAmbientCapacity: below, between and above sorted owned entries all reach actual observe and receive delta with substituted+1 / excluded unchanged. Dropping lookup equality fails `!owns_renderer`. |
| 10 | StoryRangedProjection directly calls WorldLocalIdleNpc for unseen, same-zone/spawn unbound, bound, foreign spawn, broken vtable and NULL zone. FALSE, unbound-only, no spawn proof and no bound-loop mutants each fail. OwnedRenderers still releases the unbound NPC with NULL zone. |
| 11 | StoryWorldCapacity diagnostics: raw NPC 0x0f9a / wire 0x0f9b omission keeps a 128-actor frame, with raw identifier, vtable and registry index; party raw 0x1f81 refuses. Restoring normalized stage_kind fails the NPC assertion and, in the separate `party-identity` invocation, the party assertion. |
| 12 | StoryWorldCapacity recruitment moves the client lead to x=10000 and adds client-only NPCs at 151–156 before both Begin and Finish, restoring the host view for capture. Separate CATALOG_HOST mutants fail Begin and Finish; baseline retains 127 then 128 with recruitment cleared. |
| 13 | StoryWorldCapacity: wrapperless entity 2 is inside the kept range; unconditional generic inclusion fails `target_log==0 && find(1002)<0`. Counting ambiguous candidates fails `side=host total=146 kept=128`. Retention across an epoch change fails the 1020-present / 1021-absent check. Wrapperless exclusion now has discriminating CONFIRMED_TEST evidence for this candidate. |
| 14 | Actual post-selection ambiguity scan over targets[0..n), rather than whole-registry pre-pass: fails `ambiguous_log==1 && find(1001)<0`; the original `priority-boundary` invocation fails the loop at i=1 on retained 1001, so it does not independently prove the displaced slot (corrected by second-pass item 22 below). The older mutation is correctly named registry-order candidate cap, not this rule. |
| 15 | LanPartySession large catch-up: distinct non-PC position/time/sequence/fingerprint/channels, nontrivial transaction; every world actor field, world header, party actor/view/header, scenes and recruitment fields compared without struct memcmp. Decode writing only out->world fails transaction and non-world field CHECKs. |
| 16 | LanPartySession: valid sorted 129th actor in adjacent enclosing storage, compile-asserted at actors[128]; removing count bound now accepts the frame and fails the explicit refusal CHECK. ChunkCount(129)==0 retained. |
| 17 | Session TU compilation: eight-bit mask fails the mask-bit assertion; eight chunks fails the ceiling(actor capacity / chunk actors) assertion. CONFIRMED_STATIC, not runtime tests. |
| 18 | StoryWorldCapacity client apply: 72 combatants each acquire a slot and HP drops produce pending>0. Restoring 64 fails slot coverage. Saturated stale slots log once per epoch; removing that bound fails hp_full_log==1. feedback[] follows the shared capacity. |
| 19 | StoryAwayInterior snapshots inside_shows before the already-inside return. Moving ShowMember inside the parked block fails shows==inside_shows+1, despite the earlier lead_here show. |

File-by-file changes in this review (all other dirty work preserved):

| File | Change |
| --- | --- |
| `src/hooks/lan_story_world.c` | Ownership serial publication; raw candidate diagnostic fields and wire labels; HP/feedback capacity and once-per-epoch saturation log. |
| `src/hooks/lan_story_world.h` | Read-only game-thread ownership serial API. |
| `src/hooks/lan_story_ambient.c` | Serial invalidation before same-frame return, retaining periodic refresh and one delta per frame. |
| `src/network/lan_party_session.c` | Assembly mask and chunk-count compile assertions. |
| `tests/story_ambient_capacity_fixture.c` | Fixed-clock actual observe negative cases and ownership swap. |
| `tests/story_world_capacity_test.c` | Raw diagnostics, divergent recruitment, priority/epoch/ambiguity, real rebind and 72-combatant apply cases. |
| `tests/story_ranged_projection_test.c` | Direct shop rule and zone serial assertions; Q1 ownership expectations retained. |
| `tests/story_trail_clock_image_test.c` | Serial getter stub for the existing exact-image ambient fixture. |
| `tests/lan_party_session_test.c` | Field-by-field large catch-up equality and deterministic 129th-actor storage. |
| `tests/story_away_interior_test.c` | Requires a fresh ShowMember call on inside return. |
| `docs/testing.md` | Historical provenance/gate/scope corrections and this hash-pinned review record. |
| `docs/status-matrix.md` | Dirty-work label and complete castle owner gate. |
| `docs/step-away-stage1.1-audit.md` | Dated H2 evidence citation, process latch, switch-telemetry owner gate and target-refresh admission limits. |
| `docs/coop-story-plan-54.md` | §9 artifact/scope/capacity corrections and review candidate entry. |

## Castle/stage-1.1 review second pass — 2026-10-20

2026-10-20: Superseded for DLL `67c8271d6ef3a69f7b1883fdceb195e9a8b0338af24c0bc321d556235e2241ca` by [Castle Q2 pose omission and bounded hold](#castle-q2-pose-omission-and-bounded-hold--2026-10-20); earlier DLLs keep this gate.

Applied items 20–22 of `castle-and-stage11-fixes-review.md` on dirty
`27f7296`, `codex/shared-simulation`, saved-story co-op. Item 21's header-only
change caused a rebuild; new DLL SHA256:
`2054726676010da07bf9db3aec66c09c9bfa9a7b96ca56abf3db013b08527920`.
Candidate: `build/mingw32/bin/SudekiMP.dll`, preserved at
`build/artifacts/castle-review2-20547266/SudekiMP.dll` with `review.sha256`,
`review.diff`, changed-source snapshot and validation results. The previous
`d523d43c…`, `66e851b6…` and Part A artifacts remain unchanged. The DLL's
`.text` section is byte-for-byte identical to `d523d43c…`; executable logic
was not changed by this pass. This does not establish live behavior.
Supported executable SHA256 remains
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

| Item / file | Result and evidence |
| --- | --- |
| 20 — `docs/step-away-stage1.1-audit.md` | **CONFIRMED_STATIC:** H1 now separates target-refresh acquire/commit admission from native-control acquire/release, movement/zero-speed blend-stop and dodge calls. Whether the latter link/unlink party TPtrs and whether the refresh gates open in the castle switch window remain **UNKNOWN**. MOVE/SNAP and H1's hypothesis status are retained. |
| 21 — `src/hooks/lan_story_world.h` | The H1b comment again directly precedes SetLocalZone. The read-only serial declaration follows it and names only its actual bump sites; registry/vtable/wrapper observations still rely on the 250 ms refresh. Comment/declaration ordering only, no API or runtime logic change. |
| 22 — `tests/story_world_capacity_test.c` | **CONFIRMED_TEST:** explicit `find(&first,1021)>=0 && find(&first,1020)<0` before the loop. Baseline normal and `priority-boundary` invocations pass. The same select-before-ambiguity mutant still fails the split ambiguous-1001 assertion normally; in `priority-boundary` it now fails the explicit 1021/1020 boundary assertion before reaching i=1. Both isolated mutations compile/link and exit 3 on assertions. |
| 22 / record — `docs/testing.md` | Historical row 14 now says the original loop failed at i=1 on retained 1001; this addendum records the new independent boundary check without upgrading the old hash's evidence. |
| Record — `docs/coop-story-plan-54.md` | §9 second-pass candidate and validation entry. |

The documented `tools/build-linux.sh` build and `git diff --check` pass.
All 21 focused Wine fixtures listed in the preceding review pass on this
candidate, including StoryWorldCapacity / StoryAmbientCapacity, plus the
explicit `StoryWorldCapacityTest.exe priority-boundary` baseline run.
Native C11 `-O2 -Wall -Wextra -Werror` TPtrAuditTest, StoryStepAwayTest and
StoryThreatTest pass. Full Wine suite: **225/229 invocations (227 executables)**,
with the unchanged four known non-pass cases: DirectInputProbe and XInputProbe
exit 1 without a controller; LanArenaLoopbackPeerTest exits 2 without paired
arguments; LauncherModsUiTest exits 3 without its external fixture. The
existing isolated LauncherTabs fixture handles only its own save prompt.
Both item-22 mutation runs fail at the specified assertions, not compile or
launch failures. Graphify AST update completed with its existing partial-parser
limitations. Evidence is **CONFIRMED_TEST / CONFIRMED_STATIC**, not live acceptance.

No commit, stash, reset, reformat, deployment or game run. Q2's scope and the
owner's live gates are unchanged; no freeze/crash resolution or live acceptance
is claimed. Items 20 and 21 are source/documentation corrections, so no runtime
mutation is attributed to them.

## Castle Q2 pose omission and bounded hold — 2026-10-20

Dirty `27f7296` on `codex/shared-simulation`; implements
[the Q2 brief](castle-q2-pose-omit-task.md). DLL SHA-256:
`67c8271d6ef3a69f7b1883fdceb195e9a8b0338af24c0bc321d556235e2241ca`.
Preserved at `build/artifacts/castle-q2-67c8271d/SudekiMP.dll`, with
`q2.sha256`, `q2.diff`, `changed-source/` and `validation-results.json`.
The preceding `2054726676010da07bf9db3aec66c09c9bfa9a7b96ca56abf3db013b08527920`
DLL remains separate in `build/artifacts/castle-review2-20547266/`; its old
`target()`-stage-only gate and the earlier artifacts remain intact.
Evidence here is **CONFIRMED_TEST / CONFIRMED_STATIC**, with no live claim.

**Extended scope and hold.** A non-party catalog registry-entry, generic
identity or position failure now omits that entry; an available party pointer
still refuses, and the generic resource-vtable image check still refuses the
frame. Capture probes every non-party pose, removes late pose/death-state/wire
bounds failures, and removes changed targets during the final recheck.
Parallel target, HP, bound-record and frame arrays stay aligned. Party failures,
ranged projection and native write-transaction checks retain their refusals.

The brief authorizes the extension and the reviewer's H choice: a bound
non-party entity with the same full target identity may publish its entire
previous valid record for **2000 ms inclusive**, using unsigned subtraction
from its last fresh committed `read_tick`. Held captures update the ordinary
capture tick but never extend this window. HP, flags, position, generation and
animation sequence all remain the previous values. A fresh read within the
window keeps the generation and publishes the new HP; expiry removes the entry
and a later return gets a new generation. Identity/position/recheck failures
never hold. Generation gaps after wire validation are harmless: consumers use
equality, not adjacency (**CONFIRMED_STATIC**).

Client Prepare now probes every matched non-party pose and caches selectors,
empty-channel bits and the empty-clip observation. A failed support probe still
requests its resource immediately. Any attempted native residency acquisition
triggers a new read/support pass over every retained non-party entry before
membership/rebind decisions; that pass never loads another resource. The
main loop consumes its cached results. A client omission and return rebind the
intersection; interpolation runs on host frames before Prepare, so this client
filtering does not refuse an interpolation pair.

**Static call/cost audit.** B adds one `read_pose_detail` → `read_native_pose`
pre-pass read per non-party target per capture (a held entry is read twice).
It reads position/forward, channel rows and clip-occurrence headers;
`bank_header_readable` may call VirtualQuery outside its last proven span.
Between host probe and main read, only reads and module-local state execute.
No native game call occurs on a continuing client path between the final
non-party probe and its main-loop read. A party support failure can acquire a
resource, but immediately aborts Prepare. The material backing readiness
checks are observations, not leases: caching slightly widens their existing
gap, which already spans ReplicaApply between Prepare and ApplyPrepared.
Another thread changing these observations, or the visible effect, is
**UNKNOWN**. No synthetic timing is offered as a live cost measurement; read
`capture_mean_us` and `capture_max_us` on `lan_story event=host_frame` live.

Q1 is unchanged. Only omitted NPCs return to local idle. Client-omitted doors,
chests, scenery, monsters and allies remain owned with zero ambient delta and
retain their last applied pose (**CONFIRMED_STATIC**, visible effect UNKNOWN).

**Diagnostics.** All per-entity records use `event=entity_omitted side=…`
with literal `stage=target_omitted`; `detail` is always the registry index,
while `pose_stage`/`pose_detail` preserve the failing read (or the hold reason).
Deduplication is per side and epoch on `(entity, wire identifier, held/removed)`:
one held line and one removal line are possible; the first removal cause wins.
There are 64 slots per side, followed by exactly one `entity_omitted_budget`
line for a new key at capacity. `world_omitted total/kept` still counts only the
catalog selection pass, excluding pre-catalog skips, later removals and holds.
`target_still` fills raw entity/vtable/kind context; the runtime records
`present_refused step=world_apply` with the saved error and still restores the
cinematic view. ApplyPrepared's refusal/write semantics are unchanged.

**Validation and discriminating mutations.** All-target Flatpak build and
`git diff --check` pass. The 21 executables from the review follow-ups plus
StoryMaterialOutputImageTest, StoryMonsterDamageGuardImageTest and
StoryTrailOwnerImageTest pass (**24 focused executables**). Capacity also
passes explicit `priority-boundary` and `party-identity` invocations and the
12 Q2 case groups. Native C11 `-O2 -Wall -Wextra -Werror` TPtrAudit,
StoryStepAway and StoryThreat pass. All **35 isolated mutants** below compile
and launch, then fail at assertions (exit 3), not compile/launch errors.

| Brief test / fixture mode | Mutations rejected at assertions |
| --- | --- |
| 1 / `q2-unbound` | Pre-pass narrowed to combatants; three non-party failures still produce valid, sorted, decodable frames with `cause=pose`, never late/held. |
| 2 / `q2-late` | Late pose restored to whole-frame refusal; a kept ally's one-shot callback invalidates a later unbound NPC after pre-pass, while survivor generations stay stable. |
| 3 / `q2-wire` | ActorValid failure restored to whole-frame refusal; state 2, negative rate and blend 2 are omitted, with valid distinct generations and stable survivors. |
| 4 / `q2-death` | Death-state failure restored to refusal; C's HP-array move removed. Adjacent 1145 retains HP 70 when 1144 with HP 40 leaves. |
| 5 / `q2-hold` | Hold removed; window removed; held read_tick refreshed; window uses Bound.tick; strict `<`; pose-read success refreshes read_tick; held HP uses fresh HP; death hold falls through; same_target removed; dedup class removed (**10 mutants**). Schedules T+1000/1990/2000/2010, whole-record equality, interpolation, fresh recovery, hidden-bit preservation and identity replacement discriminate them. |
| 6 / `q2-catalog` | Each of three A refusals restored; available-party comparison removed; available_mask dropped; global image check folded into an entity skip (**6 mutants**). Converted unreadable/generic assertions retain raw pointers, kind, identifiers and exact registry-index fields. |
| 7 / `q2-recheck` | Recheck restored to whole-frame refusal; a one-shot door resource-handle change removes it, and return has a new generation. Party variant refuses. |
| 8 / `q2-client` | Client pose probe narrowed to combatants; Prepare/Apply omit an NPC with non-finite x, then rebind it on recovery, without scenery_unsupported. |
| 9 / `q2-load` | Re-probe removed; acquisition flag suppressed; resource request left to the later main-loop path. NPC B sorts before A; A's acquisition corrupts B, both leave the intersection, exactly one acquisition occurs, and apply succeeds. |
| 10 / `q2-party` | B's non-party guard removed; C's late-pose guard removed; E's probe moved outside its guard. Bound/unbound Tal still refuse at transform with no omission record. |
| 11 / unbound, catalog, budget | Old world_refused event restored; omission logged before party refusal; budget line removed. The extra party-log mutant initially survived a missing assertion; the fixture now checks one refusal and zero omission records there and rejects it. 65 distinct failures give 64 records and one budget line. |
| 12 / `q2-write` | Raw write-owner context removed; runtime diagnostic removed; early return introduced after diagnostic. Actual ApplyPrepared identity failure produces raw context and one runtime world_apply refusal; cinematic restore still executes (**3 mutants**). |

Two equivalent mutations are deliberately not claimed as discriminated: B's
HP move cannot be observed because the next slot's HP is recomputed; H's
non-party guard is redundant with its guarded B/C callers. Both guards remain,
and bound-party refusal is tested. The late-read callback simulates a change;
it is not evidence that such a change occurs in the game.

Full suite: **225/229 invocations, 227 executables**. The known non-pass set
is unchanged: DirectInputProbe and XInputProbe exit 1 without a controller;
LanArenaLoopbackPeerTest exits 2 without paired arguments; LauncherModsUiTest
exits 3 without its external fixture. An initial shared-desktop LauncherTabs
run timed out; the final full run uses an isolated Windows desktop and TMP/TEMP
and declines only that test process's exact save-on-close prompt. Its assertions
pass there. Graphify AST update completed with the existing partial-parser
limitations. No game was launched.

**Remaining refusals and costs (retained deliberately):**

1. ApplyPrepared pre-write identity, in-write and readback checks; castle
   reachability UNKNOWN.
2. A failed native ResidencyAcquire latches `resource_fault`: subsequent
   Prepare refuses at `clip_resource_unknown` / ERROR_INVALID_STATE until
   uninstall. Recruitment and TrailOwner return bare FALSE at their
   precondition; AimDirection returns FALSE with reason `precondition`. A
   failed native call cannot authorize an assumed-safe rollback.
3. Duplicate registry pointers (`duplicate_entity`) and the generic resource
   image check (`generic_resource_vtable`) still refuse the frame.
4. Recruitment retains exact equality. E widens the existing filtered-bind /
   post-recruitment-frame mismatch: FinishRecruitment can retry every frame
   while retained. The specified castle saves already recruited Ailish; using
   an intersection for recruitment is a later owner decision, not this pass.
5. Per-entity animation-sequence and global generation overflow still refuse.
   next_generation commits only on successful capture and resets only at
   uninstall. Its exhaustion is unreachable in practice (about two years at
   one new target per capture, or six days at all 128 new on every capture).
6. Client main-loop defensive refusals remain; after the final probe they are
   reachable only through another thread (reachability UNKNOWN).
7. Host-set removal costs one refused interpolation pair, and a return with
   a new generation costs another. A first failure of an unbound entity
   changes no prior set. H does not cover identity/position/recheck failures;
   alternating failures of a bound identity can refuse every pair and stop
   client presentation, worse than the preceding behavior. Castle
   reachability UNKNOWN. H bounds only the specified pose-related failures.

**Owner live gate for `67c8271d…`:** Tal away first, then controlled, while
Ailish traverses the castle tunnel in both directions. The client keeps
presenting with `world_captured=1` throughout; no frame is refused for a
non-party entity's identity, position, pose, death-state, wire-bounds or
recheck failure, subject to remaining refusals 1–7 above and the unchanged
party/registry/roster/frame-consistency proofs. Read `event=entity_omitted`
separately from frame `event=refused`; `world_omitted` keeps its catalog-only
accounting. Watch repeated presentation-invalid / interpolation refusals
coinciding with entity_omitted lines, and note any pose_held entity that stands
still. Observe Q1 NPC idle/rebind snap, omitted door state, and the last pose of
client-omitted non-NPC kinds. Measure apply_mean_us (spectator), world_mean_us
(presentation_cost), capture_mean_us and capture_max_us (host_frame). Keep
the prior 16 ms mean-apply threshold and owner-approved 96-actor fallback:
12 chunks, catch-up at least 15,640 bytes / 16 fragments, plus v14/profile29
if 128 has gone live. This candidate remains 128 / v13 / profile28. Keep the
castle crash investigation separate. No live behavior or crash resolution is
claimed by these synthetic checks.

| File | This pass |
| --- | --- |
| `src/hooks/lan_story_world.c` | A–E/G/H omission, complete-record hold, cached client preflight/re-probe and bounded logs; F raw write-owner context only. |
| `src/hooks/lan_story_runtime.c` | F preserves ApplyPrepared's error and logs its refusal without skipping cinematic restoration. |
| `tests/story_world_capacity_test.c` | Reset/log/callback/residency plumbing and converted catalog assertions; runs the Q2 groups. |
| `tests/story_world_pose_omit_fixture.h` | Twelve isolated case groups against the actual adapter, codecs and interpolation. |
| `tests/story_world_apply_runtime_fixture.c` | Actual runtime failure route invoking the real adapter through a fixture callback; cinematic-restore assertion. |
| `CMakeLists.txt` | Adds the runtime fixture to the capacity executable with its existing whole-program/assertions-enabled test options. |
| `docs/testing.md` | This hash-pinned evidence, mutations, retained refusals and new owner gate; dated supersession lines only in old records. |
| `docs/coop-story-plan-54.md` | §9 owner extension/reviewer-H decision and evidence; prior Part A gate retained by hash. |
| `docs/status-matrix.md` | New verification link and separate old/new DLL gates, retaining its evidence/maturity vocabulary. |

## Castle arrival cutscene and client lead — 2026-10-20

Dirty `27f7296` on `codex/shared-simulation`; implements
[the arrival brief](castle-arrival-cutscene-task.md) on the preserved Q2 build.
DLL SHA-256: `aa5deba44f442537153197bc32ff51225f62daec478ec42289eae53c22950f5f`. Artifact:
`build/artifacts/castle-arrival-aa5deba4/SudekiMP.dll`, with `arrival.sha256`,
`arrival.diff`, `changed-source/`, mutation definitions and sanitized
`validation-results.json`. Earlier `20547266…` and `67c8271d…` artifacts and
their hash-pinned gates remain unchanged. No commit, deployment or game run.
Evidence for this candidate is **CONFIRMED_TEST / CONFIRMED_STATIC**, plus
specified **CONFIRMED_EXACT_IMAGE** checks; no live upgrade.

[Research R0–R5](castle-arrival-cutscene-research.md) separates the observed
20547266 run from this candidate. The owner reported the first tunnel/castle
crossing while Tal was away. Logged host samples kept world capture through
that crossing; no `world_omitted` appeared. Castle client apply averaged
**17368 us** (nine window means 16577–17971), world 7568 us, host capture
3833 us. The **16 ms cost gate failed**; the earlier 96-actor fallback remains
applicable, but no separate capacity/protocol migration is made here. Zero
world captures coincided with failed/not-attempted party capture, predominantly
non-READY scenes. Raw logs, machine paths and tokens remain private.

**Behavior and scope.** An exact same-world party replacement retains a proved
TEMP destination while abandoning the old split. Intermediate scene/area/mask
changes wait instead of permanently latching presentation off. The native
movie blocks the host controller producer: this does not transmit the movie
or manufacture fresh frames. The implementation targets resumption after it.
Client presentation keeps the native two-character party contained and projects
only Tal to Replica/World during the closed room scene. Adjacent scene epochs,
actor generations, full native identity and world catalog are still proved.
The reverse uses the full native roster; rejoin takes a current Tal-only seed.

`[StoryAreas] ClientLeadSpectate=false` is the default. When enabled, connected
Tal owners return from away and receive the normal PREPARE/ACK/READY lease.
The host input adapter fences Tal before the native consumer. Self-review
found that native FilterNone is asynchronous: the guard now commits only its
own pending None using the verified controller-tail write and native camera
input notification, rather than assuming the request already disabled input.
A proved native Aimed camera targeting Tal may request Exploration through the
existing camera-state entry. On disconnect the host takes temporary control
only after the retained remote lease drains. Rejoin starts a fresh lease and
blocks host input again. Returning Ailish restores ordinary host/client roles.
Reservations and wire/save formats are unchanged; no assignment UI is added.

**Validation.** All-target `tools/build-linux.sh` build and `git diff --check`
pass. The Q2 **24 focused executables**, both new arrival executables, the
host-filter and control-image fixtures, WorldCapacity `arrival`,
`priority-boundary` and `party-identity` invocations pass. Native C11
`-O2 -Wall -Wextra -Werror` TPtrAudit, StoryStepAway and StoryThreat pass
(StoryThreat links libm). Full suite: **228/232 invocations, 229 executables**;
the same known non-pass set remains: DirectInputProbe and XInputProbe exit 1
without controllers, LanArenaLoopbackPeerTest exits 2 without paired arguments,
and LauncherModsUiTest exits 3 without its external fixture. LauncherTabs uses
the isolated desktop and private TMP/TEMP harness, affecting only its own
save-on-close prompt. Graphify AST update completed with its existing parser
limitations. No test executes a live game session.

**Mutation method.** Each mutant has its own source tree, object and executable;
production files are not changed to run it. All **35 Q2** and **37 arrival**
mutants compile, launch and fail at assertions. Q2 and ordinary arrival fixtures
exit 3; the nine native control-image mutants use the fixture's CHECK assertions
and exit 1 with explicit failure lines. Compile failures and crashes do not
count. Initial mutation-only syntax and warning failures were corrected before accepting results. A stale-seed mutant
initially survived because the fixture changed the area as well as the seed;
independent same-area epoch and revision assertions now reject it. The
asynchronous None check also exposed a production gap; the corrected code has
its own regression and mutations A35–A37.

| Arrival mutation | Discriminating fixture / assertion |
| --- | --- |
| A01 drop retained TEMP; A02 allow foreign TEMP | ArrivalObserver: real ObserverSample reaches stable Tal-only READY; descriptor/world/group/controller/name/table/pending variants refuse. |
| A03 restore permanent scene latch | ArrivalPresentation: transient mismatch waits, clears stale history, later matching scene proceeds; hard prior latch remains. |
| A04 remove closed generation reseed; A05 ignore generation guard | ArrivalPresentation: forward/reverse increment, absent Ailish generation memory, every actor/epoch mismatch. |
| A06 skip full native proof; A07 retain Ailish in projection | ArrivalPresentation: exact reconstructed Tal projection, unchanged full roster, pointer/native-proof drift refusal. |
| A08 ignore connection; A09 ignore frame/scene proof | ArrivalPresentation ArrivalApply: no writer reached on stale transport proof; projected equipment/prepare/apply ordering, cancel and error routing. |
| A10 leave away set; A11 ignore default-off | ArrivalPresentation: accepted forced return without fabricated ACK/HUMAN; default off unchanged. |
| A12 leave client away intent set | ArrivalPresentation: exact PREPARE/READY offer changes RETURNING intent; wrong player/character/epoch/phase refuses. |
| A13 refuse Solo catchup; A14 ignore seed epoch/revision | ArrivalPresentation: current Solo available, stale same-area seed and stale receipt refused. |
| A15 disallow intermediate pair lead edge | ArrivalPresentation: SetLead-before-RemovePC and reverse pair edge. |
| A16 disable world epoch grant; A17 ignore world owner | WorldCapacity `arrival`: real Prepare/Apply both directions; no grant, skipped epoch and foreign world refuse; grant consumed. |
| A18 runtime rejects native lead; A19 handoff rejects native lead | StepAwayHost ArrivalRuntimeHandshake: actual runtime service and handoff matcher, PREPARE then ACK/READY, reverse, disconnect busy/drained, rejoin new connection. |
| A20 omit temporary host label | StepAwayHost: active Guest name/no away suffix; after disconnect/drain name resolves to Host. |
| A21 omit None call | HostFilter ArrivalHostInput: existing native filter seam called and exact input owner retained. |
| A22 omit Exploration request; A23 accept foreign camera target | HostFilter ArrivalCamera: request once with Tal target; foreign target/render/state/bytes/native proof refuse. |
| A24 keep remote policy after disconnect | HostFilter: native lease retained before fallback, then temporary host input with asynchronous All completion. |
| A25 refuse reverse lead in guard | HostFilter: native Ailish target passes without being rotated back to Tal. |
| A26 skip guard; A27 ignore its refusal | SkillTraceImage ArrivalInputSeam: guard before real wrapper's original spy; false issues neither original nor post-original witness. |
| A28 unregister in callback; A29 uninstall while registered | SkillTraceImage: in-flight callback/lifecycle assertions, dependency retention, successful unregister and final teardown. |
| A30 reject native lead; A31 require companion default mode | SkillTraceImage ArrivalLeadLease: exact lead acquire/release with mode 0; ordinary companion still mode 1. |
| A32 forbid host-identity change while draining; A33 forbid intermediate pair drain | SkillTraceImage: retained Tal/AI release crosses native role change, not new acquisition. |
| A34 require companion mode on load drain | SkillTraceImage: busy load drain retains, released native lead verifies mode 0. |
| A35 omit early current-mode commit; A36 omit camera-input notification | HostFilter: asynchronous None becomes owned 0/0 before consumer, manager input disabled via native-ABI spy. |
| A37 bypass settlement image proof | HostFilter: altered tail/call/entry/return or foreign camera slot refuses before the current-mode write. Separately all 36 signature-byte mutations refuse on the supported image. |

**Part S — arrival requirements.** A test with spies proves routing or admission,
not native simulation or visuals. Every row without a discriminating executable
mutation is explicitly identified. Names below refer to the table above.

| Brief requirement | Code/evidence location | Discriminating test / mutant; evidence and limit |
| --- | --- | --- |
| R0 capture through tunnel/castle | Research R0; runtime capture order | Owner/log record CONFIRMED_LIVE for 20547266 only; no test/mutant, candidate live UNKNOWN. |
| R0 omissions | Research R0 prefixes | No world_omitted in inspected prefixes; no test/mutant, unlogged ticks UNKNOWN. |
| R0 cost / 16 ms / 96 fallback | Research R0 weighted counts; prior cost gate | Arithmetic/log read, no timing fixture; CONFIRMED_LIVE samples, failed gate INFERENCE. |
| R0 Tal control mode / zero captures | Research R0/R1; runtime service | Away transaction and capture ordering; no discriminating test, CONFIRMED_STATIC/log record. |
| R1 two-seat millisecond timeline, quest/movie/areas/epochs/masks/lead | Research R1 | Log brackets + authored GEX order; no test/mutant; exact quest/movie instants UNKNOWN. |
| R1 Tal room location | Research R1; ObserverSample | TEMP-table fixture A01/A02; CONFIRMED_TEST/STATIC, live placement only observed old build. |
| R2 freeze gate / frame production | Research R2; presentation_scene_gate | A03 and A01; CONFIRMED_TEST/STATIC. No fresh stream during movie; live recovery UNKNOWN. |
| R2 cast_view, cinematic view, RestoreView | Research R2; apply_presented_frame | Q2 test 12 restore routing; CONFIRMED_TEST/STATIC. Cinematic visual playback UNKNOWN. |
| R3 native lead global/script/setter | Research R1/R3 | Supported-image decompilation, no native setter execution/mutant; CONFIRMED_EXACT_IMAGE/STATIC. |
| R3 client lease/away/host/companion/AI byte | Research R3; service_story_controls; native control | A10/A18/A19/A30–A34; CONFIRMED_TEST. Run's settled AI ref count UNKNOWN. |
| R3 why host drove Tal / Guest AI text | Research R3; RuntimePlayerName/CharacterAway; name-tags renderer | A10/A20 tests state/name; string assembly STATIC. Rendered tag not tested. |
| R4 plan G and swap phases | Research R4; plan G; ownership service | Source/document read, no new general-G test/mutant; CONFIRMED_STATIC. |
| R4 tier 0 lead rule / step-away | Research R4; away_host_admit/arrival_resume_owner | A10–A12; ordinary enemy suites unchanged. General lead-policy adaptation not added. |
| R4 F2/#96 / missing host spectator owner | Research R4 | Source/evidence read, no test/mutant; broader spectator system UNKNOWN. |
| R5 smallest design / alternatives / risks | Research R5 | Static design record; no executable proof for a design choice; CONFIRMED_STATIC/INFERENCE. |
| R5 symmetric reverse / unreachable save | story_arrival.h; generation journal; native drain | A04/A15/A25/A30–A33; CONFIRMED_TEST. Actual save reverse route UNKNOWN. |
| F1 resume party/world after movie | ObserverSample; present; apply_arrival_party_world; WorldPrepare | A01–A09/A15–A17 and Q2 restore test; CONFIRMED_TEST at component boundaries. Full runtime/native sequence and live resumption UNKNOWN. |
| F1 use existing camera or keep local view | Existing cinematic path; research R2 | Q2 restore test; no discriminating moving-camera/movie test, UNKNOWN visually. |
| F2 client lead lease | HostControlLeadPolicy; service_story_controls; handoff matcher | A18/A19/A30/A31; CONFIRMED_TEST, no authority transfer to client. |
| F2 host input off Tal | lead_input_guard/lead_settle_none; shared pre-original seam | A21/A26/A27/A35–A37; CONFIRMED_TEST/EXACT_IMAGE. Complete native input event execution UNKNOWN. |
| F2 host camera follows Tal | lead_camera | A22/A23 native-call spies; CONFIRMED_TEST/STATIC admission only. Actual follow/room-script reassertion UNKNOWN. |
| F2 overhead controller label | RuntimePlayerName/CharacterAway; name tags | A10/A20; CONFIRMED_TEST metadata, rendered label UNKNOWN. |
| F3 reverse roles/camera | native role drain; host input release; native camera binding | A04/A15/A25/A30–A33; CONFIRMED_TEST control. Native reverse camera and actual re-add UNKNOWN. |
| F4 default-off INI; ungated gate correction | dllmain profile branch; runtime setter | RETIRED: the saved-story wiring claim was wrong; aa5deba4 read the switch only in story-observe. A11 tested a direct setter, not launch routing. Corrected by the follow-up below. A01/A03 remain ungated. |
| Constraint one adapter per seam / transactional install | shared StoryInputGuard; HostControlInstall/Uninstall | A28/A29; existing rollback paths STATIC. New registration install rollback not independently fault-injected, UNKNOWN. |
| Constraint bytes/targets/ABI | filter ownership; camera entry guards; settlement tail/notification and regparm(0) thunk | A23/A37 plus 36 exact-image byte changes; ABI spy stack succeeds. Full native camera function UNKNOWN. |
| Constraint transport does not touch native objects | setters queue plain scene data; game-thread service and pre-original wrapper | Native witness tests A26–A34; source call-path audit CONFIRMED_STATIC, no race stress test. |
| Constraint native writes through proved seam | research R3; lead_settle_none; existing AI/filter/CameraState entries | Native tail/ABI exact-image read and A35–A37; no new hook, no persistence/party writes. |
| Constraint teardown, disconnect, rejoin | runtime drain/policy, native control, callback unregister, current Solo catchup | A13/A14/A24/A28/A29/A34, actual handshake test; CONFIRMED_TEST. Whole-game rejoin/teardown UNKNOWN. |
| Tests five named scenarios, isolated assertion mutants | Arrival tests + existing image/runtime fixtures | A01–A37; CONFIRMED_TEST; detailed limits above. |
| S both briefs / all Q2 groups | This table and Q2 tables below | 35 Q2 + 37 arrival assertion failures; no independent reviewer, no complete live proof. |
| Docs/research/hash/§9/status | Research doc; this section; plan §9; status matrix | Document/diff/hash read; no executable mutant, CONFIRMED_STATIC. |
| Build/focused/native/full/diff/graph | Validation paragraph and artifact manifest | Commands actually run; CONFIRMED_TEST, no gameplay upgrade. |
| Preserve artifacts/no destructive Git/report/live gate | Separate arrival artifact and prior hashes | File/hash/status inspection; no executable mutant, CONFIRMED_STATIC. |

**Part S — Q2 implementation requirements re-audit.** `W` means
`src/hooks/lan_story_world.c`; `R` means `src/hooks/lan_story_runtime.c`.
`Q1`–`Q12` refer to the independently run test groups in the following table.
Rows marked static have no dedicated discriminating mutant and are not promoted
to native/live proof.

| Q2 requirement | Location | Test / mutant and evidence limit |
| --- | --- | --- |
| A image clause before generic identity; keep other callers | W catalog/entity_identity | Q6 image-check-skips; CONFIRMED_TEST. Other callers retained by source read, STATIC. |
| A unreadable/identity/position skip; available party pointer still refuses | W available_party_entity/catalog | Q6 three restored-refusal, no-roster-compare, no-available-mask; CONFIRMED_TEST. |
| A stack diagnostic candidate/raw kind/index/side; registry reproved | W catalog/registry_still | Q6 raw diagnostic assertions; registry and recruitment-side path STATIC, no distinct mutation. |
| A recruitment exact counts and per-index equality unchanged | W BeginRecruitment/FinishRecruitment | Source read only; no new discriminating recruitment mutation, CONFIRMED_STATIC, affected recruitment live UNKNOWN. |
| B probe every non-party, preserve held entry until main loop | W Capture pre-pass/pose_hold | Q1 narrowed probe; Q5 no-hold; CONFIRMED_TEST. |
| B remove aligned targets/HP, pose detail and monster log | W Capture/log_pose_omitted | Q1/Q4 assertions. Pre-pass HP memmove mutant equivalent (later recomputation); not claimed discriminated. |
| B extra-read cost and pure interval | W Capture/read_pose_detail | Source call audit only, CONFIRMED_STATIC; no timing assertion, live Q2 cost UNKNOWN. |
| C late pose/death/wire holds or removes; counts/parallel arrays/retry index | W Capture pose_failed/held path | Q2–Q5 late/death/HP/wire and hold mutations; CONFIRMED_TEST. |
| C generation gaps safe by equality | World frame codec/Prepare/interpolator/damage checks | Source audit + Q3/Q7 valid frame and return generation assertions; equality design STATIC, overflow not mutation-tested. |
| C party/ranged/native-pose/generation/animation overflow refusals retained | W Capture | Q10 party mutants; ranged suite baseline. Overflow/native-pose retained STATIC, no new discriminating mutants. |
| D recheck removes synchronized target/bound/frame; never holds | W Capture final recheck | Q7 restored-refusal, changed resource and return generation; CONFIRMED_TEST. |
| D party/global registry/roster/frame mismatches still refuse | W Capture final checks | Q7 party assertion; global retained checks STATIC without dedicated mutations. |
| E filter every non-party and record client omission | W Prepare filter | Q8 narrowed probe, omitted/rebound NPC; CONFIRMED_TEST. |
| E cache selectors/empty-state and reuse only non-party | W Prepare matched_selectors/empty caches | Q9 native-load re-probe assertions; cache reuse STATIC, no dedicated reuse-removed mutation. |
| E immediate resource request retains failure context | W prepare_missing_resource/filter | Q9 load-deferred; CONFIRMED_TEST. |
| E flag immediately before any acquisition, reset per Prepare | W prepare_native_load | Q9 no-load-flag; CONFIRMED_TEST for successful load. Failed-acquire branch STATIC, no separate new mutation. |
| E re-probe before membership/rebind; refresh outputs; remove paired arrays, never reload | W Prepare re-probe | Q9 no-reprobe / no-load-flag / load-deferred; acquisition count and applied set assertions, CONFIRMED_TEST. |
| E party probes after loading; defensive non-party main refusals retained | W Prepare main loop | Q10 client party; remaining branches STATIC, worker timing UNKNOWN. |
| E no native call on continuing path after last probe | W Prepare main loop | Source call audit CONFIRMED_STATIC; no race/native-timing test. |
| E client intersection rebind; only NPC idle release | W Prepare/client_unbound_npc/OwnedRenderers | Q8 rebind; capacity/Q1 idle assertions. Omitted non-NPC held pose STATIC, visible result UNKNOWN. |
| F ApplyPrepared write/refusal semantics not relaxed | W ApplyPrepared | Q12 forced identity failure, existing image tests. Arrival adds only authorized epoch-grant consumption at binding; setters/refusals unchanged. |
| F save error before timing, diagnostic, no early return before view restore | R apply_arrival_party_world/apply_presented_frame | Q12 diagnostic removed / early return; error survives helper timing and cinematic restore, CONFIRMED_TEST. |
| F raw write_owner entity/vtable/native kind before checks | W target_still | Q12 context removed; CONFIRMED_TEST. |
| G literal entity_omitted format/causes/registry detail/pose reason | W log_target_omitted/log_pose_omitted | Q1–Q11 formatted assertions + old-event; CONFIRMED_TEST. |
| G one helper/dedup class per side and epoch; first removal wins | W target_omitted_seen | Q5 no-dedup-class; CONFIRMED_TEST for fixture schedule, helper/namespace reset STATIC. |
| G 64 slots/side and one exhaustion line, no attempt counter | W target_omitted_count/budget | Q11 no-budget and 65 failures; CONFIRMED_TEST. |
| G world_omitted only catalog selection; historical records retained | W catalog and earlier hash-pinned docs | Source/diff read CONFIRMED_STATIC; no dedicated mutation. |
| H read_tick only after fresh death/ActorValid success; inclusive 2000 ms unsigned age | W Bound/pose_hold/Capture | Q5 refresh-held, tick-window, strict-window, pose-success-refresh, no-window; CONFIRMED_TEST. |
| H matching full identity, bound only, no party | W pose_hold/Capture | Q5 no-identity; Q10 bound/unbound party. H guard redundant with guarded callers, equivalent mutant not claimed. |
| H one hold point, whole previous record, skip current HP/flags/validity work | W Capture pose_failed/held commit | Q5 held-fresh-hp/death-fallthrough/no-hold, whole-record equality; CONFIRMED_TEST. |
| H interpolate held pair; fresh HP drop continuity, expiry new generation | World interpolator/monster_damage_bound | Q5 equality/interpolation/fresh/expiry assertions; actual native damage replay STATIC/UNKNOWN live. |
| Remaining 1 native write checks | W ApplyPrepared | Q12 forced write-owner refusal; other readbacks existing image suites, castle reachability UNKNOWN. |
| Remaining 2 failed native resource call latches retained world | W resource_fault | Source retained, no new failed-acquire mutant; CONFIRMED_STATIC, uncertain native effects remain UNKNOWN. |
| Remaining 3 duplicate entity / image guard | W catalog | Q6 image mutant; capacity duplicate checks baseline; CONFIRMED_TEST. |
| Remaining 4 recruitment equality hole | W recruitment | Source read only, no discriminating new test; UNKNOWN live. |
| Remaining 5 animation/global generation overflow | W Capture | Source read only; years/days estimate not runtime proof. Retained, no discriminating new test. |
| Remaining 6 concurrent client main-loop failure | W Prepare | Read path retained; no race reproducer, UNKNOWN. |
| Remaining 7 host removal/return refuses interpolation pair | WorldFrameInterpolate/hold | Q5 interpolation and generation tests; alternating-identity live frequency UNKNOWN. |
| Docs/hash-pinned supersession/status/§9 gate | Existing Q2 section and plan §9; arrival record | Diff/source inspection only; CONFIRMED_STATIC. Earlier records not rewritten. |
| Acceptance build/focused/native/full/mutants/graph/artifact/report | Validation and tables | Executed checks CONFIRMED_TEST. Separate artifacts and no-commit verified statically. Live gate UNKNOWN. |

**Part S — all twelve Q2 test groups.** These repeat the named Q2 mutants
against the arrival candidate, not merely a reading of the prior report.

| Brief group / invocation | Isolated mutations rejected (35 total) | Evidence |
| --- | --- | --- |
| Q1 `q2-unbound` | Narrow pre-pass to combatants | CONFIRMED_TEST |
| Q2 `q2-late` | Late pose refuses whole frame | CONFIRMED_TEST |
| Q3 `q2-wire` | Wire invalidity refuses whole frame | CONFIRMED_TEST |
| Q4 `q2-death` | Death failure refuses; omit C HP-array move | CONFIRMED_TEST |
| Q5 `q2-hold` | No hold; no window; refresh held read_tick; use Bound.tick; strict boundary; refresh on pose success; fresh HP in held record; death fallthrough; omit identity; omit dedup class (10) | CONFIRMED_TEST |
| Q6 `q2-catalog` | Restore each of three refusals; remove party comparison; remove available mask; turn image mismatch into skip (6) | CONFIRMED_TEST |
| Q7 `q2-recheck` | Recheck restores whole-frame refusal | CONFIRMED_TEST |
| Q8 `q2-client` | Narrow client probe to combatants | CONFIRMED_TEST |
| Q9 `q2-load` | No re-probe; no acquisition flag; defer resource request | CONFIRMED_TEST |
| Q10 `q2-party` | Remove B guard; remove C guard; remove E guard | CONFIRMED_TEST |
| Q11 unbound/catalog/budget | Restore world_refused text; log omission before party refusal; remove budget line | CONFIRMED_TEST |
| Q12 `q2-write` | Omit raw owner context; omit runtime diagnostic; return before cinematic restore | CONFIRMED_TEST |

**Static cost and unresolved risks.** The Q2 cost statement still applies:
one extra pose pre-pass read per non-party target per capture, two reads for
a held entry, including channel/occurrence/header probes. No native game call
occurs on a continuing Prepare path between the final non-party probe and its
main-loop read; a party resource acquisition immediately aborts Prepare.
Backing readiness is an observation, not a lease, and another thread's change
remains UNKNOWN. Per side/epoch, an entity logs at most one held and one removed
record; the first removal cause wins, with 64 total slots and one budget line.
Omitted client non-NPCs retain their last applied pose under zero ambient delta.

The arrival candidate has no full native cutscene/rejoin integration run. The
movie supplies no fresh frames; repeated scripts may reassert the static
camera. Packet loss skipping an adjacent membership epoch, replaced native
actors on reverse, foreign modal ownership, remote drain that cannot prove
completion, and concurrent native resource changes may retain/refuse rather
than recover. Skill/VFX behavior in the projected singleton and the runtime's
short revision-gap path are not newly proven. Callback and component tests do
not prove these cases away. The general plan-G ownership model, AI-controlled
native lead, another executable, assignment UI, save migration and 96-capacity
fallback are outside this bounded implementation.

**Owner live gate — no candidate live claim.** Enable
`[StoryAreas] ClientLeadSpectate=true` only for the intended test stage. Keep
the existing Q2 extended omission/hold gate and seven remaining refusals above;
watch interpolation refusals, held poses, NPC rebind snaps and capture/apply
cost separately from the crash investigation.

1. Tunnel and castle with Tal away, then in control.
2. Quest completion: observe the client through the cutscene; it must resume
   presenting after the blocking native movie at the latest.
3. Tal in his room: his connected owner returns from away, controls him, and
   the host camera follows without host input driving Tal; verify the label.
4. Return to Ailish lead, if reachable: host Ailish / client Tal and camera.
5. Leave and rejoin, including a busy drain: temporary host control only after
   release, then a fresh acknowledged client lease, with no stale input block.

## Castle arrival first live replay — loader and room-camera correction, 2026-10-20

Dirty `27f7296`; new DLL SHA-256: `4c7ed0f61da827b7c9476a179f28679bbec6bb0915cdd8b20a5d94a22560ba56`.
Separate artifact: `build/artifacts/castle-arrival-followup-4c7ed0f6/`.
The earlier aa5deba4 artifact and its original validation record remain intact.
No commit or deployment of this correction; no claim that its live gate passes.

**Owner/log record.** On aa5deba4, presentation resumed after host movie skip
and reached Tal's room, but Tal stayed away, the host drove him, both return
paths refused, and the client camera was wrong. Both INIs had the switch on;
the saved-story loader never applied it. That defeats the earlier F4 static
wiring claim. [Research R6](castle-arrival-cutscene-research.md#r6--first-arrival-candidate-owner-run-and-correction-2026-10-20)
records the qualified timeline and the misleading error message. During the
native movie there were no new world frames; that remaining freeze is not
claimed fixed by this candidate.

**Owner decision.** Both windows use the game's authored Tal-room camera,
with exclusive control for his connected reserved client. The previous host
Exploration override is removed. Tests A22/A23 are historical evidence for a
superseded camera policy, not acceptance requirements for this build. Temporary
host control remains restricted to disconnect and completed native drain.
Later host-only kick/body-release and spectator-claim controls are recorded,
not implemented.

**Validation.** All-target `tools/build-linux.sh` passes. The 27 focused invocations pass;
the full executable suite passes **229/233 invocations, 230 executables**.
The four known non-pass cases are unchanged: DirectInputProbe 1 and XInputProbe
1 (no controllers), LanArenaLoopbackPeerTest 2 (no paired arguments), and
LauncherModsUiTest 3 (external fixture absent). The full suite includes the
host-filter/control exact-image cases and WorldCapacity arrival case. Separate
WorldCapacity priority-boundary and party-identity invocations also pass. Thirteen isolated mutants compile and all fail at assertions
(exit 3), not linker errors or crashes. `git diff --check` passes; Graphify
AST update completed with the existing parser limitations. No game was
launched by this validation.

| Regression / isolated mutant | Evidence and practical limit |
| --- | --- |
| StoryLoaderProfile: F01 omit saved-story setter, F02 force false, F03 force true | CONFIRMED_TEST: compiled actual title-profile branch/boolean readers, native install spies, real INI IO; missing/false/true/on/invalid, entry/load-only and call order. The unfixed saved-story source fails an assertion. Earlier executable/build-identity gates and native installs are not executed. |
| StepAwayRuntime: F04 restore wrong message | CONFIRMED_TEST: menu and raw-M return through the real service/mailbox; refused return stays away, no retry loop, distinct away rejection. Unfixed source fails an assertion. |
| HostFilter: F05 reinstate Exploration override | CONFIRMED_TEST: actual host service preserves Aimed camera state across repeated calls while input stays blocked; inert native camera spies. Host-filter exact-image byte checks still pass. Actual authored camera pixels UNKNOWN. |
| ArrivalPresentation: F06 disable room view; F07 foreign connection; F08 unmatched frame; F09 foreign area | CONFIRMED_TEST: room-view selection requires enabled profile, selected client, valid view, READY native scene, same connection/area and exact Solo scene/frame. Explicit invalid variants refuse; no native rendering. |
| ArrivalPresentation: F10 forget failed restore, F11 skip cast restore, F12 skip room restore, F13 treat room view as cinematic input hold | CONFIRMED_TEST: shared-view begin/end restores cast and replica leases, retains failed release for retry, handles movie-to-room and exit without false gameplay hold. Camera helper selection and lease calls are tested; complete apply-frame/native camera/input ordering remains CONFIRMED_STATIC / UNKNOWN live. |

The remaining acceptance checks are the forced return's real PREPARE/ACK/READY
handshake, exclusive Tal client input, matching authored room framing, reverse
Ailish transition and disconnect/rejoin. No live process was restarted or
modified during this correction. The native movie freeze needs the separately
scoped [R7 design](castle-arrival-cutscene-research.md#r7--remaining-movie-presentation-boundary-owner-approved).

## Castle arrival synchronized movie — approved extension, 2026-10-20

Dirty `27f7296`; DLL SHA-256:
`498b9b003ce379fe5280d30b8094d307f0cdc4aebebd69ba614fa7f0b104afe6`.
Separate artifact: `build/artifacts/castle-arrival-movie-498b9b00/`.
The handoff/room-camera-only `4c7ed0f6` artifact, `aa5deba4`, `67c8271d`
and `20547266` remain separate and unchanged. No commit, deployment, stage
reload or live acceptance of this candidate.

**Accepted owner decision.** Synchronized installed movie on each client;
the host controls skip/end. Both windows use the authored Tal-room camera
with exclusive control for Tal's connected reserved owner afterward. The
existing `ClientLeadSpectate` switch still defaults off and gates that handoff.
The movie channel runs in saved-story independently of that switch and of
`SkipStartupMovies`. Other movies, body assignment/kicking and general native
lead AI remain outside the implementation. [R8](castle-arrival-cutscene-research.md#r8--synchronized-fma04a-implementation-and-native-contracts)
records native addresses, lifecycle, transport policy and limitations.

**Validation — CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE.** All-target
`tools/build-linux.sh` passes. All **34 focused invocations** pass: the prior
27 plus MovieTransport, MovieImage (synthetic and supported-image preflight),
MovieRuntime, StartupMovieSkip with the supported image, and WorldCapacity
priority-boundary/party-identity. The full suite passes **233/237 invocations,
233 unique executables**. The unchanged non-pass set is DirectInputProbe 1,
XInputProbe 1, LanArenaLoopbackPeerTest 2 and LauncherModsUiTest 3 (the same
hardware/paired-argument/external-fixture limitations as the prior record).
Native C11 TPtrAudit, StoryStepAway and StoryThreat checks pass with warnings
as errors. `git diff --check` passes. Graphify AST update completed; its existing
partial-parser/zero-node warnings remain navigation limitations.

All **26 isolated mutants** compile and fail at assertions (exit 3); no crash,
link failure or timeout counts as rejection. The final candidate is the rebuilt
hash above, including game-thread shutdown retry and the optional-channel
heartbeat correction. Earlier validation of intermediate `a3850520` is not
substituted for this final run.

| Requirement / production location | Discriminating evidence and mutation | Limit |
| --- | --- | --- |
| Shared MoviePlay owner, independent of startup skipping (`lan_arena_startup_movie_skip.c`) | MovieImage executes the real entry detour/trampoline with inert native bodies; M22 implicit logo skip fails; existing StartupMovieSkip checks pass | Decoder/render/audio are not executed |
| Installed FMA04A only, local client skip disabled (`lan_story_movie.c`, shared trampoline) | Exact basename positives/negatives, actual cdecl stack arguments; M21 enable client skip fails | Other movies remain passthrough; installed asset availability live UNKNOWN |
| Exact native loop/close seams (`MovieImageMatches`) | Hash-checked supported PE mapping with relocation; nine signature/target/operand mutations each refuse; synthetic install, second-stage rollback, active-call refusal, foreign patch restore failure and retry | Native stop contract additionally supported by exact-image disassembly; native device recovery UNKNOWN |
| Game-thread ownership and safe teardown (`MovieService`, `MovieUninstall`) | M18 permit self-suspend and M25 wrong-thread uninstall fail; active callback retains owner; main-thread retry succeeds | Runtime's stopping-branch retry routing is CONFIRMED_STATIC; whole-game shutdown/unload not established |
| Host-only events, real wire admission (`receive_message`) | MovieTransport loopback and injected envelopes; M01 old envelope limit, M03 omit capability confirmation, M04 accept client authority, M05 omit scene fence, M06 wrong token, M07 wrong generation all fail | Tests use local UDP, not a remote-machine run |
| Plain worker delivery while host world publication is blocked (`service_movie_transport`) | M02 no worker events and M13 require fresh world scene fail; start succeeds after scene freshness expires and continues without scene publication | Host native callback timing in an actual quest is UNKNOWN |
| Replay, receipt and clock fences (`story_movie.h`, movie receiver) | End-before-start terminal record, duplicate/delayed start rejection, codec invalid fields; M08 allow terminal replay, M11 allow stale event, M12 elapsed underflow fail | Not an exhaustive packet-fuzzing or clock-wrap proof |
| Recipient snapshot / additive compatibility | Capability absent or arriving late never enrolls a connection; dropped first event is recovered; M09 ignore recipient generation, M10 ignore capability and M26 starve ordinary heartbeat fail | Old-host behavior is simulated by ignoring only the new capability; no public mixed-build compatibility promise |
| Native client end and recovery (`movie_tick`) | M14 omit host stop, M19 close a foreign handle, M20 omit shutdown stop fail; host loss closes the owned fake native handle | No native handle/task field is written by the mod; real codec cleanup and restored video/audio UNKNOWN |
| At most one play per connection/serial and bounded late start | M15 replay late start and M16 duplicate replay fail; missing/refused native play does not retry | Starts may differ by network/scheduling delay; no frame-number synchronization or seek |
| Restore borrowed views and discard stale world state (`prepare_story_movie`) | M17 skip preparation, M23 forget failed view restore, M24 keep stale presentation fail; input stays closed until fresh ordinary presentation | Full render/input dispatch and actual authored room pixels remain UNKNOWN |
| Prior Q2 and handoff requirements | Existing Part S tables remain applicable; focused/full suites re-run on this hash | Earlier owner observations belong to their original DLLs, not this one |

**Self-review boundaries.** The full native movie loop is not executed in the
image fixture: start/update/close are inert spies. Supported-image checks prove
identity/signatures/targets, not playback. The runtime fixture executes the
actual view/input preparation helper against owner spies, not every native
controller/render callback. No claim of exact simultaneous frames, audible
playback, device-loss recovery, post-movie exclusive control, camera framing,
reverse story transition or whole-game disconnect/rejoin is made. The previous
16 ms castle apply-cost failure and 96-actor fallback decision remain separate.

**Owner live gate.** Use matching candidate DLLs on the intended stage and keep
`ClientLeadSpectate=true` for the handoff check. Repeat the castle arrival with
Tal away: both windows should play FMA04A; client input should not skip it; host
skip or natural end should retire client playback. Then verify the authored
room camera on both windows and exclusive Tal client control, both return-from-
away UI paths, reverse Ailish transition if reachable, and disconnect/rejoin.
Check bounded `story_movie` start/end/return/refusal records; correlate them
with fresh scene/frame recovery. Missing assets or starts older than 1500 ms
may retain the last world image until end/expiry. The 1000 ms channel timeout
bounds the polling decision, not native device-reset waits. No live windows
were changed during implementation or test validation.

**File-by-file report.** The movie extension adds to the separately preserved
handoff/room-camera correction; rows marked “carried forward” were validated
and packaged as `4c7ed0f6`, then included unchanged in the current DLL.

| File | Change |
| --- | --- |
| `CMakeLists.txt` | Movie adapter in DLL; MovieTransport, MovieImage and MovieRuntime targets; existing loader fixture retained |
| `src/hooks/lan_story_movie.c` | New exact-image movie-loop adapter, host observer, client playback, stop/handle/thread checks, bounded diagnostics and retained teardown |
| `src/hooks/lan_story_movie.h` | Adapter lifecycle, service and image-preflight contracts |
| `src/hooks/lan_arena_startup_movie_skip.c` | Shared observer/trampoline API, active-call retention, explicit cdecl register convention and independent logo policy |
| `src/hooks/lan_arena_startup_movie_skip.h` | Observer/playback API shared with the single MoviePlay owner |
| `src/network/story_movie.h` | Pointer-free whitelist event codec and monotonic/terminal policy |
| `src/network/lan_party_session.c` | Optional capability, scene/connection-fenced events, worker repetition, ordinary heartbeat preservation, end/rejoin handling |
| `src/network/lan_party_session.h` | Movie begin/end/read transport API |
| `src/hooks/lan_story_runtime.c` | Movie install/service, safe view/input preparation and game-thread retirement retry; prior independent room-view correction retained |
| `src/hooks/lan_story_host_control.c` | Carried forward: preserve authored room camera while blocking host gameplay input |
| `src/loader/dllmain.c` | Carried forward: read ClientLeadSpectate in the actual saved-story title profile |
| `tests/story_movie_transport_test.c` | Real loopback, adversarial envelopes, stalled publication, loss/replay/scene/connection/capability/heartbeat checks |
| `tests/story_movie_image_test.c` | Supported-image preflight, actual hook/trampoline with native spies, input ABI, native-owner and teardown/retry checks |
| `tests/story_movie_runtime_test.c` | Production preparation helper with failed view-restore retention and input/history invalidation |
| `tests/story_loader_profile_test.c` | Carried forward: actual loader branch and INI parsing regression |
| `tests/story_step_away_runtime_test.c` | Carried forward: return refusal wording through M and menu routing |
| `tests/story_arrival_host_fixture.h` | Carried forward: host-service fixture preserves room camera |
| `tests/story_arrival_presentation_test.c` | Carried forward: room-view selection, restoration and independent gameplay admission |
| `docs/castle-arrival-cutscene-task.md` | Records approved movie extension and superseding room-camera decision |
| `docs/castle-arrival-cutscene-research.md` | R7 approval and R8 native/transport evidence with explicit unknowns |
| `docs/testing.md` | This hash-pinned validation/mutation/self-review report; clarifies separate earlier WorldCapacity runs |
| `docs/coop-story-plan-54.md` | §9 owner decision and candidate evidence entry |
| `docs/status-matrix.md` | Research-only movie/arrival candidate; no live upgrade |


## Castle movie preparation scope correction — 2026-10-20

Dirty `27f7296`; DLL SHA-256:
`7d3596822dcf355694ef60a66660e6231ed46262b7a00732904183c24b9e7ae8`.
Separate artifact: `build/artifacts/castle-arrival-movie-scope-7d359682/`.
The earlier `498b9b00`, `4c7ed0f6`, `aa5deba4`, `67c8271d` and `20547266`
artifacts remain separate. No commit, deployment or reload of this correction.

**Earlier candidate replay.** On `498b9b00`, the owner reports no apparent
client movie, host skip followed by Tal's room, and exclusive Tal control on
the client. Logs and a read-only terminal-state inspection confirm event
receipt without a native playback attempt, followed by the room/control
transition. This is not live playback acceptance. [R9](castle-arrival-cutscene-research.md#r9--first-synchronized-movie-replay-preparation-used-the-wrong-pause-proof)
records the evidence limits and the deterministic controller/menu pause-proof
mismatch. Raw evidence remains private.

**Change.** Prepare retained views under the existing fresh controller
containment bracket, then leave that scope before MoviePlay. No native menu
pause guard is weakened; no movie is entered inside a graphics frame. Keep
host authority and all existing event/deadline/teardown rules. Add bounded,
named refusal diagnostics with serial, scene and timing fields.

**Validation.** All-target build passes. Final focused results are **35/35**
(the prior 34 plus ClientRegistry). The synthetic pause fixture initially
copied 28 of 29 signature bytes; that setup failure is retained separately,
corrected, and the affected Runtime test rerun successfully. The other 34
focused passes are unchanged. Full-suite result: **233/237 invocations (233 unique executables)**, with the same four known non-pass cases: DirectInputProbe 1 and XInputProbe 1 (hardware), LanArenaLoopbackPeerTest 2 (paired arguments), LauncherModsUiTest 3 (external fixture).
Native C11 TPtrAudit, StoryStepAway and StoryThreat checks also pass with
warnings treated as errors. Eight isolated mutants compile and fail at assertions, exit 3; no timeout,
crash or compile failure counts as rejection. Final diff and graph checks:
**PASS** (Graphify AST update; existing partial-parser warnings remain).

| Requirement / production location | Discriminating evidence | Boundary |
| --- | --- | --- |
| Controller preparation can prove owned pause outside MenuNative frame (`service_story_movie`, `prepare_story_movie`) | Runtime uses real MenuNativePauseExact/ObserveOwnedPause on validated synthetic owners; S01 restoring the original menu-only proof fails | Actual game containment and view owners are spies; first packet latency UNKNOWN |
| Fresh scoped dispatch, no permission retained into later calls (`service_story_movie`, ClientRestoreViewDispatch) | Runtime checks missing/invalid witness, missing pause, nested entry, scope ending before playback and failed postcondition; S04 retain witness, S05 omit scope, S06 ignore failure all fail | New client entry shares the existing production bracket unchanged; whole native bracket is CONFIRMED_STATIC, not executed by this fixture |
| Restore exact views; keep state on refusal; close input and invalidate old presentation | Runtime exercises cast and replica refusal/retry with exact callback; S02 ignore failed restore and S03 retain presentation fail | Native renderer and borrowed-camera objects are represented by spies |
| Refusal diagnostics bounded by reason/serial and install | Image test repeats a failed prepare 100 times, then 80 new serials; S07 repeat refusal and S08 remove cap fail; late refusal is checked | Does not prove live playback or codec recovery |
| Earlier Q2, handoff, host-only movie authority and lifecycle requirements | Existing Part S and movie tables remain applicable; focused/full suites run on this candidate | Earlier 26-mutation results belong to `498b9b00`; only the eight scope/diagnostic mutants above were run in this correction |

**File-by-file report.**

| File | Change |
| --- | --- |
| `src/hooks/lan_story_client.c` | Share the existing controller containment bracket; expose a bounded retained-view restoration entry |
| `src/hooks/lan_story_client.h` | Define restoration scope and forbid movie/simulation/pause work inside it |
| `src/hooks/lan_story_runtime.c` | Bind the current controller witness, restore with ClientRosterExact, clear the witness on return, retain input/presentation fencing |
| `src/hooks/lan_story_movie.c` | Named preparation/admission/late refusals, timing and per-reason deduplication under the existing 64-record cap |
| `src/hooks/lan_story_movie.h` | Clarify preparation must finish its scoped proof before playback |
| `tests/story_movie_runtime_test.c` | Controller routing and preparation regression with real native pause checks and explicit containment/view spies |
| `tests/story_movie_menu_fixture.c` | Synthetic native pause owners and signatures; execute production native pause checks without injecting into a game |
| `tests/story_movie_image_test.c` | Late/refusal diagnostic and log-bound regressions; existing hook/codec-spy tests retained |
| `CMakeLists.txt` | Link the real pause fixture into Runtime test, exporting only its required test seams |
| `docs/castle-arrival-cutscene-research.md` | R9 replay evidence, cause, native render boundary and remaining unknowns |
| `docs/testing.md` | This hash-pinned test/mutation report and explicit fixture limits |
| `docs/coop-story-plan-54.md` | §9 replay and correction entry |
| `docs/status-matrix.md` | Replace unplayed-old-candidate wording with partial owner observation; keep RESEARCH-ONLY |

**Owner live gate.** Reload matching candidate DLLs when requested, keep
ClientLeadSpectate enabled, repeat arrival and check actual movie pixels/audio
on the client, host-only skip and fresh room recovery. The owner's prior
exclusive-control observation remains tied to `498b9b00`; this correction's
live playback, authored camera framing, reverse and reconnect remain UNKNOWN.


## Castle–Brightwater loop accepted checkpoint — 2026-10-20

**CONFIRMED_LIVE — owner report, bounded route acceptance.** After the requested
reload of both seats, the owner reports that the correction works and that the
castle ↔ Brightwater loop was completed. Evidence ID:
`owner-report-castle-brightwater-loop-20261020`. This is the owner's gameplay
observation, not an automated visual inspection. The run used dirty `27f7296`,
the saved-story title-lobby LAN profile, host Ailish / reserved-client Tal,
`ClientLeadSpectate=true` on both seats, and two Linux/Wine windows on one
machine with the supported GOG executable. The King's Road test start and
party-invulnerability settings were retained. Both staged DLLs and the build
artifact were verified as SHA-256:
`7d3596822dcf355694ef60a66660e6231ed46262b7a00732904183c24b9e7ae8`.

Expected: the corrected arrival path permits continued co-op play and a return
through the castle/Brightwater route. Observed: the owner accepts the result
and reports completion of that loop. This supersedes the unplayed gate for
this candidate's route outcome in the preceding section. It does not supply
separate measurements of movie audio/video skew, natural EOF, camera framing,
frame time or the 16 ms apply budget. Disconnect/rejoin, hook teardown/retry,
other routes, two machines and native Windows remain unproven by this report.

**Checkpoint validation.** `bash tools/build-linux.sh` passes again with no
C/CMake changes since the tested candidate; Ninja reports no work and the DLL
hash is unchanged. The recorded 35/35 focused passes, 233/237 full-suite
invocations, three native C11 passes and eight assertion-rejected mutants
above remain the validation for these sources; they were not rerun for this
documentation-only checkpoint. The four full-suite non-pass cases remain
DirectInputProbe and XInputProbe (hardware), LanArenaLoopbackPeerTest (paired
arguments), and LauncherModsUiTest (external fixture). All 13 files in the
candidate source snapshot matched before this acceptance documentation edit.

The owner requests a commit, push and build at this point, with bug fixes and
slowdown investigation before release. This records an experimental gameplay
checkpoint, not release readiness. Raw logs, configs and prior DLL artifacts
remain private and separate; no live process was reloaded for the checkpoint.


## Launcher 0.7.0 Windows build preparation — 2026-10-20

The owner requests a successful branch build, merge to main, a fresh main
Windows build, then a 0.7.0 release. Windows workflow run
[211](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/actions/runs/211)
on `d433688` reached compilation on the online runner, then failed under
`-Werror=maybe-uninitialized` in `StoryPlayerMovementImageTest`:
`controller_parity` left `local`, `heading` and `speed` uninitialized before
asserted helper calls. The helper-success assertions already existed; this was
not an offline-runner failure. No package was uploaded by that failed run.

The fixture now initializes those outputs to NaN, preserving both success
assertions and invalid-output detection. No movement/runtime behavior changes.
Launcher version strings, manifest, package filenames and the existing surface
check advance to 0.7.0. Packaged notes and README reflect per-game inventory,
join-time save import, opt-in castle arrival and the bounded owner acceptance;
[release notes](release-notes-0.7.0.md) remain explicit about untested Windows
play and continuing bug/performance work.

All-target cross-build passes. StoryPlayerMovementImageTest on the supported
image, LauncherReleaseCheckTest, isolated LauncherTabsTest, launcher surface
and shell syntax checks pass. The first direct LauncherTabs run encountered
an existing temporary-directory fixture and exited 3; it was rerun with a
fresh temp directory and the existing isolated desktop harness. Both results
are retained privately. Graphify AST update and diff checks pass. The runtime
DLL remains `7d3596822dcf355694ef60a66660e6231ed46262b7a00732904183c24b9e7ae8`;
the existing gameplay validation is unchanged. Native Windows retry and final
main-build artifact validation are subsequent gates, recorded on the release.
