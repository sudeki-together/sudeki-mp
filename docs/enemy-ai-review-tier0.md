# Task brief: tier 0 enemy targeting review fixes, saved-story co-op #54

**Where to work.** The uncommitted tier 0 enemy-targeting / companion Defend
candidate (from [enemy-ai-task.md](enemy-ai-task.md), its "Updated
implementation and validation plan", and the four 2026-10-19 sections in
[testing.md](testing.md)) exists only in the existing `shared-simulation`
worktree of branch `codex/shared-simulation`, dirty on `fa50351`. Work there.
Do not check the branch out elsewhere, stash, reset or reformat. Other dirty
files in that tree belong to the owner. Do not commit; the owner commits.

Read `AGENTS.md` first and follow it: exact-image guards, one adapter per
native seam, workers never touch native objects, the lifetime and teardown
rules in `runtime-lifecycle.md`, the evidence vocabulary in
`evidence-index.md`, and never write "works" or "fixed" for something that
only compiled or passed a pure test. Line numbers below are from the current
dirty tree and will drift; function names are authoritative.

## What the review found (2026-10-19, static and test reading only)

Six area reviewers read the candidate, every finding was then given to two
independent skeptics, and the surviving items are below. No native-write,
crash, teardown or threading defect survived. The REMOTE/AWAY admission chain,
the cap stubs, the five-stage install rollback, failed-restore retention, the
hit-observer re-entrancy, the companion request matching (checked against the
image RTTI and the 0x5ADF60 / 0x5AE000 builders) and the config/log surface
were verified clean (CONFIRMED_STATIC / CONFIRMED_TEST). One correctness bug
remains, and it sits in the one path that no test exercises. Nothing in this
brief is live proof; the live gate at the end is still the owner's.

## Part A — the bug: a busy client body is reclassified as AI (fix first)

**Mechanism (CONFIRMED_STATIC).** In `service_story_controls`
(`lan_story_runtime.c`, the `if(enemy_targeting_attempted)` block) the adapter
inputs are built as

```c
seats[c].held=host_control[p].input_held || !lease;
seats[c].receipt=host_control[p].last_input_at;
```

Earlier in the same function, the host-control input loop sets
`host_control[p].input_held=TRUE` and zeroes `last_input_at` whenever
`SudekiMpLanStoryControlMove` reports `temporarily_held`
(`lan_story_control.c`: TEMP pause at actor+0x2B, native item use with
arbiter+0x50 bit 0x20, a non-ordinary world, any `body_idle_common` hold —
`weapon`, `skill`, `skill_task`, `cast_body`, `interaction`, `ranged_prime` —
and the `ERROR_BUSY` draw/attack case when movement+0xBE bit 8 is clear).
`SudekiMpStoryThreatSeatKind` (`story_threat.c`) then returns `STORY_SEAT_AI`
for a connected, still-sending client exactly while it uses a Skill Strike,
casts, uses an item, swaps weapons or interacts. Ordinary melee combo frames
may be unaffected (attack bit 0x1000 is deliberately excluded from the
movement-hold mask in `lan_story_control.c`); how often movement+0xBE bit 8
clears during swings is UNKNOWN.

**Consequences.** For those frames `kinds[c]==STORY_SEAT_AI`, so
`remote_cap_lease` refuses, the cap stub replays the native comparison with
`+0x16A==1` (forced 1/1 cap), and extra attackers already on the client are
refused on re-validation and drop `+0xC0`; `SudekiMpStoryThreatLeadPick`
removes the client from the humans pool, so the 20 %/2-unit margin cannot
retain it and Lead-PC enemies swap to the host for the length of the action
and back afterwards. That is the per-action target flapping the live gate
forbids and it defeats the intent of owner decisions 1 and 2. The code
faithfully implements the brief's own seat-kind paragraph
([enemy-ai-task.md](enemy-ai-task.md), "Seat kinds": "or
`host_control[p].input_held`"), so the brief carries the same defect.

**Rule to implement.** The runtime already states the correct rule in the
interior-hold service (`lan_story_runtime.c`, `host_holds` comment): *while
the host itself holds this player's input (control not ready, menus, shop
UI, a busy body) it takes no packets: that is not client silence.* A host-side
hold must never demote a HUMAN_REMOTE seat. Concretely:

1. `seats[c].receipt` comes from `host_control[p].heard_at` (last packet
   receipt; it stays current across native holds because packets are still
   taken while `host_control[p].ready`), not from `last_input_at`, which the
   hold logic zeroes.
2. `seats[c].held` means only "no proven native control lease"
   (`!lease`); it must not include `host_control[p].input_held`.
3. `ready`, `draining`, `away`, `connected` and `owned_ai` keep their
   current derivation. A seat whose control is not `ready` (mid-handoff) still
   classifies AI through `ready`; a drained lease still classifies AI through
   `draining`/`!lease`. The cap-site re-proofs (`SudekiMpLanPartyControlStoryTurnOwner`,
   `SudekiMpLanStoryRuntimeTurnLeaseExact`) do not depend on `input_held`,
   so nothing else changes.
4. The `heard_at` sentinel `1u` (receipt 0) must classify as stale, as today.
5. Edit the brief's seat-kind paragraph to match: AI for a connected seat
   whose last packet receipt is older than `SUDEKIMP_LAN_PARTY_INPUT_MAX_AGE_MS`
   or that is draining or without a proven lease; HUMAN_REMOTE requires
   `ready && !draining`, a proven lease, control HUMAN, away bit clear and a
   fresh receipt. Remove `input_held` from both clauses. Note in the brief
   that the client movement heartbeat is sent every client frame, so
   freshness only ever distinguishes a stalled or disconnected client.

Do not change `SudekiMpStoryThreatSeatKind` itself; the fix is in what the
runtime feeds it. Do not touch the dead-lead hold's own `remote`/`input_fresh`
computation.

## Part B — tests that must exist before Part A counts

The runtime mapping and `SudekiMpStoryEnemyTargetingService` currently have
no test at all: the only `Service` call in any test passes a NULL roster
(`tests/story_enemy_targeting_image_test.c`, companion_tests tail), and the
seats[] block is dead-stripped in both runtime harnesses because nothing sets
`enemy_targeting_attempted`. Part A is exactly the regression this would have
caught.

1. **Runtime seat mapping.** Factor the per-seat derivation in
   `service_story_controls` into a static function in `lan_story_runtime.c`
   (for example `enemy_targeting_seat(unsigned c, SudekiMpStoryThreatSeat *seat,
   unsigned *firing, SudekiMpLanPartyLease *away_key, BOOL *coop, uint32_t now)`)
   with no semantic change to its callers, and add a case to
   `tests/story_split_control_test.c` (next to `away_targeting_admission`)
   that drives it with the existing fixture and asserts, through
   `SudekiMpStoryThreatSeatKind`:
   - connected, ready, lease acquired, `heard_at` fresh, `input_held=TRUE`
     → `STORY_SEAT_REMOTE` (the Part A case; must fail before the fix);
   - `heard_at` older than the max age → `STORY_SEAT_AI`;
   - `draining` → AI; no lease → AI; control not HUMAN → AI;
   - away bit set with the drained lease → `STORY_SEAT_AWAY`, `away_key`
     filled, `coop` TRUE; away bit set with a retained native key → AWAY
     with an empty `away_key`;
   - unclaimed character → `owned_ai` set, not connected;
   - the host's character → neither connected nor away, kind `STORY_SEAT_HOST`
     once `seat.host` is applied as `Service` does.
2. **`Service` with an exact roster.** In the image test, build a synthetic
   roster and scene (the fixture already builds `roster` by hand and the
   `SudekiMpLanStoryObserverNativeRosterExact` stub returns `proof`), call
   `SudekiMpStoryEnemyTargetingService(&roster,&scene,seats,firing,away_keys,TRUE,now)`
   and assert `kinds[]` and `seat_inputs[]` for: host, remote, away,
   unclaimed AI (`+0x16A==0`, `mode[0xB]==1`), down via arbiter+0x50 bit
   0x10000, down via ai+0x44 & 0x58, absent via `scene->inside_mask`, absent
   via the `SudekiMpLanStorySceneSameArea` stub returning 0; an unreadable
   mode pointer → `admitted==FALSE` and no companion/walk call; an epoch or
   revision change → `companions[]` and `identities[]` cleared; a >250 ms
   gap → `companions[]` cleared; `firing_signal[]` from the host arbiter bit
   0x400000 and from a `SudekiMpLanStoryShotsLatest` stub returning a recent
   tick, and never for AI/AWAY seats.

Each new guard gets one mutation check (flip the input, assert the opposite
kind), as in the existing `AWAY_REFUSES` pattern.

## Part C — logging and testing.md corrections (low)

1. `lead_substitution` (`lead_pick`): the 128-per-install cap counts selector
   calls whose pick differs from the native result; native mode B always
   returns slot 0, so the cap is spent seconds into the first Lead-PC fight
   and later host-KO / all-away / return events leave no record. Count target
   *changes* (chosen seat differs from the seat derived from self+0xC0)
   separately from calls, rate-limit the per-event record per second, and
   reset the bound on roster namespace change rather than per install. The
   `status` record reports both counts. No enemy pointer may be cached for
   this; a seat index keyed by lease serial is acceptable.
2. `event=type` (`select_target`): it is emitted when `entry->count==1`, but
   `report()` zeroes `counters[]` every second, so each active (resource,
   mode 0/11) pair logs a new type line every second. Emit once per
   (resource, mode) per roster namespace with a small seen-set that the
   namespace clear resets; leave `counters[]` as is.
3. `companion_selection` bound: the >250 ms gap `memset(companions,…)` also
   zeroes `selection_logs`/`selection_at`, so the documented "128 per
   character per roster namespace" restarts after every stall. Clear only the
   anchor and `sampled_at` on the gap; keep the log counters until the
   namespace clear. Then testing.md's wording is true.
4. `cap_refusals` never counts AWAY seats: `cap_decision` computes `cap_only`
   only for `kinds[c]==STORY_SEAT_REMOTE`. Extend it to AWAY (it is
   statistics only; the `SudekiMpStoryThreatCapOnly` math is unchanged), or
   the live away/return reading shows `cap_refusals=0` for the away body even
   when native caps refused it on every request — the decision-3 symptom the
   gate is meant to measure.
5. testing.md overclaims to correct:
   - the `LanArenaHitFeedbackTest` row ("busy/wrong-thread consumer
     removal"): no foreign-thread `SudekiMpLanHitObserverUnsubscribe` call
     exists, and the foreign-thread Subscribe/Uninstall checks are masked by
     the duplicate-consumer and consumer-present refusals. Add the
     assertions (Unsubscribe from a worker thread refuses; Uninstall from a
     worker thread refuses with no consumer subscribed) or drop the claim;
   - the `StorySplitControlTest` bullet ("modal/unbound/wrong-thread"):
     `away_targeting_admission` only sets `runtime_thread=0`, which
     short-circuits before the thread comparison. Add a worker-thread call of
     `SudekiMpLanStoryRuntimeAwaySeatExact`/`CompanionSeatExact` or write
     "unbound";
   - the companion Defend section ("at most four per second and 128 per
     character per roster namespace", "bounded known/unknown observation"):
     the image test never counts records or touches `selection_logs`, and its
     two `companion_sample` calls are exactly 1000 ms apart. Assert the 250 ms
     gate, the 128 cap and the 1 s `companion_state` throttle, or scope the
     claim to "implemented, not asserted".
6. Also untested, add assertions where cheap: foreign-thread and foreign-id
   refusal of `SudekiMpStoryEnemyTargetingBindThread` and
   `SudekiMpLanHitObserverBindThread`; after the forced popup-restore
   failure assert the retained quarantine state (`seams[0..2]` restored,
   `installed` TRUE, `subscribed` FALSE, `admitted` FALSE, adapter BindThread
   refuses); `SudekiMpLanHitHostSnapshot` binds the thread on first call and
   is inert from a second thread; the hit journal's `seat_kind=` and
   `firing=` fields for REMOTE/AWAY/AI sources; `walk()`'s `stale_count`
   and `report()`'s `selection` picks/empty/native_absent counters;
   `read_config` through a temporary ini file with one valid and one invalid
   key (`invalid_config`, `level_clamped`, `owner_gate` records).

## Part D — contract alignment (no gameplay change)

1. **S1 recheck.** `lead_pick` calls 0x5B7120 directly on the chosen roster
   body before replacing `*out`; the brief's S1 rule says list membership is
   the proof and "do not call 0x5B7120 again". The review-pass disassembly of
   the supported image found no memory stores in 0x5B7120 or 0x5B6EC0 (only
   compares and the same per-candidate helper calls the native loop makes), so
   the recheck is harmless and it is the stronger guard (it applies cap
   parity to the chosen body under the same scope). Keep the code; amend the
   S1 sentence in the brief to "membership plus a direct 0x5B7120 recheck
   under the same selector scope" and record the no-store observation as
   CONFIRMED_STATIC next to the 0x5B7120 entry in `engine-functions.md` (cite
   the address range, not bytes).
2. **Anchor after a modal.** `SudekiMpLanStoryRuntimeCompanionSeatExact`
   refuses while the SudekiMP Esc menu / tools capture input or a swap is in
   progress, and both `companion_sample` and the `companion_pick` else-branch
   then clear the Defend anchor: one non-sticky re-pick after the modal ends.
   Accepted as is; add one sentence to the brief's plan item 5 so it is not
   reported again. The vanilla Q menu does not trigger it.
3. **`cap_decision` ABI pin.** The naked stubs call `_cap_decision` with
   stack arguments; the function is `static` and its address is never taken
   from C, so GCC may legally choose a register convention for it. The current
   object reads its arguments from the stack. Add `regparm(0)` to its
   attribute list so a toolchain or flag change cannot silently break the
   stub, and say so in the comment above `CAP_STUB`.

## Reviewed and refuted — do not act on these

- *Host-away body gets no parity/Defend anchoring.* The host cannot step away
  in story mode (`SudekiMpLanStoryRuntimeRequestAway` requires `local_seat`;
  `SudekiMpStoryAwayAdmit` refuses player 0), so the p==0 exclusions in the
  seat mapping, `AwaySeatExact` and `CompanionSeatExact` are latent until
  host step-away exists. Leave a one-line note where host step-away is
  planned; no code change.
- *`Service` leaves `seat_inputs[]` stale on the ABSENT path.* Unreachable
  while `SudekiMpLanStoryObserverNativeRosterExact` holds; optional tidy-up
  only.
- *Mode-B list may be stale like mode E.* Disassembly of 0x5B76C0 shows every
  mode except 0xE zeroes 0x7C30AC and rebuilds 0x7D5540 before dispatch, and
  0x5B6B10 returns slot 0 only when it is in this call's list
  (CONFIRMED_STATIC, review pass). `lead_pick` reads this invocation's list.
- *Unbound-thread Uninstall race; arena off-thread uninstall regression.*
  Only reachable through an explicit `FreeLibrary` unload, which is the open
  DLL-unload owner policy, not this task.
- *Away × type-3 boss not in the image test.* `actor_unit` checks the boss
  vtable identically for every seat kind and the kind × type policy is
  exhaustively pure-tested.
- *Human-first pool switch is not margin-gated.* Dropping an AI/AWAY fallback
  target or anchor the moment a standing human is eligible is what decisions
  8 and 9 say; the within-pool margin is what was accepted. The only genuine
  oscillation source was Part A.

## Out of scope

Tiers 1.5/2/3 and their open decisions 4–6; host kick; host step-away;
companion Attack/Retreat/formation changes; any change to native cap values,
stance, request, formation, target-field or counter writes; the castle crash
audit and the stage 1.1 step-away work (separate briefs).

## Acceptance

1. Build with the documented flatpak toolchain; `git diff --check` clean.
2. Focused: `SudekiMP.StoryThreatTest.exe`,
   `SudekiMP.StoryEnemyTargetingImageTest.exe <supported SUDEKI.exe>`,
   `SudekiMP.StorySplitControlTest.exe`, `SudekiMP.LanArenaHitFeedbackTest.exe`
   under Wine, plus the native C11 `story_threat_test.c` build. Show the
   Part B seat-mapping case failing before the Part A fix and passing after.
3. The full suite with the known non-pass set unchanged.
4. A new testing.md section (date, dirty revision, DLL SHA256) stating what
   each test proves and the evidence limits; the corrections in Part C.5
   applied; a §9 entry in `coop-story-plan-54.md`. Report file-by-file what
   changed. Do not claim live behaviour.
5. Owner live gate afterwards (not Codex): castle-road ambush, Ailish host /
   Tal client, `EnemyTargeting=tier0`, `EnemyTargetingCapParity=on`,
   `EnemyTargetingLeadRule=always`. Watch for enemies staying on Tal through a
   Skill Strike, a cast and an item use; Lead-PC enemies not swapping to
   Ailish during Tal's actions; readable `lead_substitution` change counts;
   `cap_refusals` for an away Tal; then host KO, all-away fallback, return to
   the same body, shop separation, disconnect/title/rejoin and teardown.
   Host-alone and arena profiles must stay inert.
