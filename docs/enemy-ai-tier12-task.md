# Task brief: enemy targeting tiers 1, 1.5 and 2 (threat memory, group alert, idle pressure), saved-story co-op #54 / #97

**Where to work.** The tier 0 candidate (enemy cap parity, Lead-PC rule with
living fallback, companion Defend anchoring) and its review corrections exist
only in the existing `shared-simulation` worktree of branch
`codex/shared-simulation`, dirty on `fa50351`. Work there. Do not check the
branch out elsewhere, stash, reset or reformat. Other dirty files belong to
the owner. Do not commit; the owner commits.

Read `AGENTS.md` first and follow it: exact-image guards, one adapter per
native seam, workers never touch native objects, the lifetime and teardown
rules in `runtime-lifecycle.md`, the evidence vocabulary in
`evidence-index.md`, and never write "works" or "fixed" for something that
only compiled or passed a pure test. Line numbers drift; function names are
authoritative.

This brief builds on [enemy-ai-task.md](enemy-ai-task.md) (the design: tier
definitions, feeds, native seams, tests, live gates; owner decisions 1–9 are
all settled as of 2026-10-19) and on
[enemy-ai-review-tier0.md](enemy-ai-review-tier0.md) (applied). Read the tier
definitions, the Feeds and Native seams sections, and the "Modes" paragraph
of the design thesis before starting. Where this brief is more specific than
the design, this brief wins; where it resolves a design ambiguity it says so,
and Part D records the resolution in the design.

**Ordering with the live gate.** The owner is running the tier 0 live gate
(castle-road ambush, watch then tier 0) in parallel. Do Part 0 first (it is
small and the owner can take it live immediately), then the tiers. Nothing
above tier 0 is switched on live until gate 1 passes (owner decision 7). If
the gate changes a tier 0 rule, this brief is amended before the affected
part is built; ask rather than guess.

## Owner decisions that bound this task (all 2026-10-19)

- Decision 4: tier 1.5 responders are up to **2 ranged-capable + 1 melee**
  enemies per alert, ranged first. All thresholds, hold/fade/cooldown and
  distance defaults in the design are accepted as *starting values* to be
  tuned live through the ini; none is live-validated. No audible bark (issue
  #98, later).
- Decision 5: tier 2 keeps `idle_seconds=5`, `push_hold=8`,
  `push_cooldown=15`.
- Decision 6: **no tier 3**. Do not add scaling, a scaling switch, or a
  placeholder for one.
- Decisions 1–3, 8, 9 (tier 0 and Defend) stay exactly as implemented; this
  task must not change their behaviour. The companion Defend branch is not
  touched.
- Decision 7: levels are cumulative and each tier is gated live in order.

**Formerly open, now answered by the owner (record both in
`enemy-ai-task.md` as decisions 10 and 11):**

- O1 — **ANSWERED 2026-10-19: no.** Tier 1 (threat) never picks an AWAY body
  and an AWAY body's hits never raise an alert; decision 3 covers attacker
  caps, not aggro. Tiers 1–2 pick only HUMAN_HOST / HUMAN_REMOTE; AI and
  AWAY bodies stay reachable only through tier 0b's decision-8 fallback.
- O2 — **ANSWERED 2026-10-19: cap it.** At most **one active alert per
  shooter seat** per set: while an alert on a shooter is active (alerting
  record or any orphan responder copy with that shooter), no other enemy
  raises one on that shooter, and the refused raise does not touch the
  refusing enemy's cooldown. So at most 2 ranged + 1 melee come for a shooter
  at any time; new recruits only after that alert ends and its cooldown
  passes on the enemy that raises the next one. A unit still belongs to at
  most one alert or push at a time.

## Part 0 — tier 0 live-gate diagnostic (do first, small, separate from the tiers)

Live on 2026-10-19 with Tal the only client and away (lease drained,
`host_control_blocked reason=drained`), the host's `status` record flapped
`active=yes/no` every 2–6 s with `known=1`. `remote` is FALSE for an away
peer without a native key, so `enemy_coop` depended on
`SudekiMpLanStoryRuntimeAwaySeatExact`, and some clause of it toggled;
`transport_confirmed` is sticky and no `host_front_use` transition was
logged, so the clause is UNKNOWN. The candidate's claim "targeting stays
active when the last connected client is away" is CONFIRMED_TEST only.

Add a sibling `SudekiMpLanStoryRuntimeAwaySeatRefusal(unsigned c, const
SudekiMpLanPartyLease *connection)` returning an enum naming the first
failing clause, listed in the order `AwaySeatExact` evaluates them:
invalid_argument (c ≥ 4, NULL connection, token 0), not_initialized
(!session, !saved_profile, !control_attempted, !story_policy_initialized),
client_seat (local_seat), thread, stopping, binding_not_ready,
menu_captures, not_owner, swap_phase, not_connected, not_away,
control_not_ai, native_key_retained, acquired, draining,
character_mismatch, peer_status (PeerStatusGet failed), peer_phase,
transport, connection_mismatch, none — with `AwaySeatExact` implemented as
`Refusal(...) == none` (one body, no duplicated logic). In
`enemy_targeting_seat`, when `seat->connected && seat->away` and the proof
fails, log `story_enemy_targeting event=away_refused seat=%u reason=%s`
once per reason change per seat, bounded 64 per namespace. Test: in the
split-control test, every existing `AWAY_REFUSES` mutation and the NULL-key,
c=4 and c=3 asserts check their named enum value, plus a zero-token case,
and `AwaySeatExact` agrees with `none`.

Owner confirmation (2026-10-19): the host was opening the SudekiMP Esc menu
while Tal was away, which matches the `menu_captures` clause
(`SudekiMpLanStoryMenuCapturesInput()`) of `AwaySeatExact`. Enemy targeting
admission must not depend on the host's menu — an away body is under normal
player pressure (decision 3) whether or not the host is reading a menu.
Remove that clause from `AwaySeatExact` only; `CompanionSeatExact` keeps its
own menu clause (the accepted Defend-anchor reset after a modal). Flip the
split-control `menu_captures` mutation: `AwaySeatExact` accepts,
`CompanionSeatExact` still refuses. Keep the diagnostic so any other clause
(for example `binding_not_ready` through the menu's effect on
`SudekiMpLanStoryHostControlReady`) shows up live. Report nothing as fixed;
the owner reruns the away case with the menu open and closed.

## What already exists (do not re-implement; extend)

In `src/hooks/story_enemy_targeting.c` and `src/network/story_threat.c`:

- The S1 selector detour (`select_target`) with the enemy branch
  (`lead_pick`, mode 0xB only, which computes per-seat list distances and
  `live_kinds` from this tick's list), the companion branch, the per-second
  `counters[]`, and the rule that the global list 0x7D5540/0x7C30AC is read
  only for modes 0 and 0xB after the native call (never for 0xE).
- S2 cap parity (`cap_decision`, the two stubs) — unchanged by this task.
- The hit journal (`hit_observe`): victim identity by lease serial, attacker
  seat from packet+0x30, applied amount from the stats delta, victim max HP,
  XZ distance, `is_far` against `melee_reach`, ranged classification
  (missile match or arbiter ranged mode), the Spirit-Strike hit-all tag with
  its `spirit_known` bit, the per-seat firing signal.
- The per-frame manager walk (`walk`): per-seat engaged / melee / ranged /
  attacking / in-melee counts, `nearest[c]` and `melee_distance[c]`, the
  `identities[]` table keyed by lease serial + epoch with positive
  retirement, and the per-second `seat` records with measured run speed.
- Seat classification per frame (`kinds[]`, `seat_inputs[]`,
  `firing_signal[]`, which is 0 for AI and AWAY seats) from the runtime's
  `enemy_targeting_seat`.
- The full threshold table, the level enum (`STORY_TARGET_TIER1`,
  `STORY_TARGET_TIER15`, `STORY_TARGET_TIER2`) and
  `SudekiMpStoryThreatEffectiveLevel` (today used only for the
  `level_clamped` log; Part B.6 fixes that).
- Bounded logging with namespace resets (`reset_selection_logs`, the
  `lead_substitution` change/throttle bookkeeping, the `type` seen-set).

Missing: the memory, the three rules, their arbitration at the seam, their
feeds from the walk, the shadow (watch) evaluation, their logs and tests.

## Conventions for every new rule

- **Time.** `now` and every stored timestamp are `GetTickCount` milliseconds
  (uint32); the adapter passes the same `now` it already samples per frame
  (`Service`) or per call (`GetTickCount()` at the seam and in
  `hit_observe`). Every seconds-valued key is multiplied by 1000 inside the
  policy. Elapsed time is `int32_t e = (int32_t)(now - t); if (e < 0) e = 0`.
  Deadlines compare wrap-safely: `(int32_t)(now - t) >= 0`. Every "active"
  state and every tracker carries an explicit flag; a zero timestamp is never
  a sentinel.
- **Sequence.** The adapter keeps `list_seq`, incremented on every mode-0/0xB
  list observation; records and alerts store sequence numbers so "post-raise"
  and "post-start" comparisons never depend on clock granularity.
- **Level.** Every gating call (`Pick`, `AlertRaise`, `AlertFade`,
  `ResponderFade`, `PushEvaluate`, `PushEnd`, `Decide`) receives `level`
  from the adapter: the stored effective level
  (`SudekiMpStoryThreatEffectiveLevel(&config)`) on the live path, the
  hypothetical level on the shadow path. The plain writers (`AlertCommit`,
  `AlertClear`, `PushStart`, `PushTrack`, `Touch`, `Decay`, `AddHit`) take
  no level: a gate always precedes them. No policy function derives a level
  from `config`. Each rule gates on its own level inside the policy (tier 1
  ≥ `TIER1`, alerts ≥ `TIER15`, pushes ≥ `TIER2`).
- **Admission.** Every entry point that can write memory or name a seat
  (`AddHit`, `Pick`, `AlertRaise`, `PushEvaluate`, `Decide`) takes
  `(cooperative, self_type, excluded)` like `SudekiMpStoryThreatLeadPick`
  (for `PushEvaluate`, which is per seat, only `cooperative`) and returns "no
  opinion" / writes nothing when co-op is inactive, the self type is outside
  1–3, or the unit is excluded by `EnemyTargetingExcludeTypes` (entity name
  or lease resource, computed as `lead_pick` does, for mode 0 as well as
  0xB). Every seat-naming branch returns "no opinion" for a request mode
  other than 0 or 0xB. Excluded units never accrue memory, never raise an
  alert, are never responders or pushed enemies.
- **Seats.** Tiers 1–2 name only seats whose live kind for this call is
  HUMAN_HOST or HUMAN_REMOTE (O1). DOWN and ABSENT seats are never named by
  any branch even when the native list contains them. The kinds used at the
  seam are `live_kinds` as `lead_pick` computes them per list member, not the
  per-frame `kinds[]`. Threat accrues for every attributed seat (for the
  logs); sniper only for HOST/REMOTE shooters.
- **Scores.** `threat`/`sniper` are HP-fraction units. `decayed(x) = x *
  2^(-elapsed / (half_life*1000))` computed without writing; `Decay` and
  `AddHit` are the only writers. `score(seat) = decayed_threat[seat] / (1 +
  d/d0)` is used only by the tier 1 pick; responder and push comparisons use
  raw decayed threat.
- **No pointers.** Records, alerts, pushes and the per-frame unit table hold
  serials, seats, floats, flags and timestamps only. A CAiUnit pointer is
  resolved from the current frame's walk or the lease observation on each
  use.
- **Native writes.** Only `*out` inside the detour, after the direct
  0x5B7120 recheck under the same selector scope (the `lead_pick` pattern).
  No reads of the global list outside modes 0/0xB; no new hooks; no change
  to S2, the companion branch, the runtime seat mapping or the dead-lead
  hold. Everything runs under the bound thread (`fresh()` / `Service`).
- **Sets.** Alert and push state exist twice, indexed `STORY_SET_LIVE=0` and
  `STORY_SET_SHADOW=1`. Live calls read and write only the live set; the
  shadow path (Part B.7) only the shadow set. Threat memory is shared.

## Part A — pure policy (`story_threat.c/.h`, plain data, natively testable)

### A.1 Data

```
STORY_ALERT_RESPONDERS_MAX = 3   /* decision 4: 2 ranged + 1 melee */

Alerting   { active, shooter, raised_at, raise_seq, until, last_fire,
             responders[MAX], count, cooldown_active, cooldown_until }
Responding { active, shooter, alerting_serial, raised_at, raise_seq, until,
             last_fire, recheck_failed }
Record     { serial, mode_seen, last_mode, list_seen, last_list_mask,
             last_list_at, last_list_seq, threat[4], sniper[4], updated_at,
             alerting[2], responding[2], lock_blocked }
PushState  { active, serial, started_at, start_seq, until, cooldown_active,
             cooldown_until, idle_active, idle_since, recheck_failed }   /* per seat, [2] sets */
Candidate  { serial, distance, ranged_capable, same_group, type, excluded,
             substitutable, shooter_listed (TRUE/FALSE/UNKNOWN), designated,
             current_target_seat, current_target_threat }
SeatFrame  { kind, firing, engaged_known, engaged_by_any, engaged_by_other }
```

`AlertResponders` never writes past `capacity` and clamps
`alert_ranged + alert_melee` to `STORY_ALERT_RESPONDERS_MAX` (document the
clamp; `Parse` keeps accepting up to 32 per key). A zeroed record is
"never observed" (`mode_seen=0`, `list_seen=0`).

Derived predicates (pure helpers, tested):

- `substitutable = !mode_seen || last_mode ∈ {0, 0xB}`.
- `shooter_listed`: UNKNOWN when `!list_seen` or the observation is older
  than `fire_recent`; TRUE when a fresh mask contains the shooter; FALSE when
  a fresh mask lacks it. UNKNOWN is admitted and reported, never treated as
  FALSE.
- `SudekiMpStoryThreatResponderListed(record, set)` → UNKNOWN unless
  `list_seen && last_list_seq > responding[set].raise_seq`; otherwise TRUE or
  FALSE by whether `last_list_mask` contains `responding[set].shooter`.
  `SudekiMpStoryThreatPushListed(record, push_state, seat)` does the same
  against `start_seq` and `seat`. The adapter builds every `listed` /
  `pushed_listed` input only through these helpers (UNKNOWN when the table
  is incomplete), so a pre-raise or pre-start observation never counts.
- `designated(candidate, set)` = the candidate's `responding[set].active`,
  or some `push_states[*][set].active` whose `serial` is the candidate's.
- `same_group` = both group ids non-zero and equal.
- A cooldown whose deadline has passed is cleared (`cooldown_active = 0`) by
  the next `AlertRaise` / `PushEvaluate` that inspects it, so no deadline is
  ever compared more than ~24 days after it passed.

### A.2 Memory

- `SudekiMpStoryThreatDecay(config, record, now)` writes the decay into both
  arrays with `config->value[STORY_THREAT_HALF_LIFE]` (seconds × 1000;
  `Parse` keeps it above 0) and stamps `updated_at`.
- `SudekiMpStoryThreatAddHit(config, cooperative, self_type, excluded,
  record, seat, kind, amount, max_hp, ranged, hit_all_known, hit_all, firing,
  unengaged, far, now)` → `{added_threat, added_sniper}` flags. Refuses
  (writes nothing) on admission failure, `max_hp <= 0`, non-finite or
  negative amount, `seat >= 4`. Decays, adds `amount / max_hp` to
  `threat[seat]` for every attributed hit (hit-all included), and to
  `sniper[seat]` only when `kind ∈ {HOST, REMOTE} && ranged && hit_all_known
  && !hit_all && firing && unengaged && far`. An unknown hit-all state never
  counts as sniper.
- `SudekiMpStoryThreatTouch(record, set, shooter, now)` refreshes `last_fire`
  on the record's alerting and responding state of that set when its shooter
  is `shooter`. The adapter calls it over all identities on every attributed
  hit by `shooter` (not only the victim) and on every frame while
  `firing_signal[shooter]` is set.

### A.3 Pick (tier 1)

`SudekiMpStoryThreatPick(config, level, cooperative, self_type, excluded,
mode, record, now, current, eligible, kinds, distances)` → seat or 4.
Requires `level >= TIER1`, admission, mode 0 or 0xB. Considers only
`eligible` seats of kind HOST/REMOTE (O1). Computes `score` on decayed
threat. The candidate set is the seats whose score exceeds `threat_floor`;
if empty → 4. Within that set, keep `current` when it is in the set and no
other member's score exceeds the current's by more than `threat_margin` (a
fraction of the current's score); otherwise the best. Header note: the floor
is compared with the distance-weighted score, so at `d = d0` it means twice
the design's "5 % of HP".

### A.4 Alert (tier 1.5)

- `SudekiMpStoryThreatAlertRaise(config, level, cooperative, self_type,
  excluded, set, record, seat, shooter_kind, sniper_added,
  shooter_alert_active, now)` → BOOL. TRUE when `level >= TIER15`,
  admission, `sniper_added` (this hit added sniper, which implies a
  HOST/REMOTE shooter), decayed `sniper[seat] > alert_threshold`, no alert
  active on `record.alerting[set]`, `!shooter_alert_active` (O2: the adapter
  passes TRUE when any identity has `alerting[set].active` or
  `responding[set].active` with `shooter == seat`), and no active cooldown.
  Writes nothing; a refusal for `shooter_alert_active` leaves this record's
  cooldown untouched.
- `SudekiMpStoryThreatAlertResponders(config, reference, candidates, n, out,
  capacity)` → count. `reference` is the alerting enemy's raw decayed
  `threat[shooter]`. Admit only candidates with type 1–3, not excluded, not
  designated, substitutable, finite `distance <= alert_radius`,
  `shooter_listed != FALSE`, and `current_target_threat` not strictly
  greater than `reference` (ties qualify, including 0 vs 0: the alert exists
  to turn enemies the shooter never hit; a candidate with no target or whose
  target is the shooter passes 0). The alerting enemy is an ordinary
  candidate at distance 0 (with the defaults its own weighted score can be
  below `threat_floor`, so tier 1 does not reliably turn it). Order: same
  group first, nearest first within a group; fill up to `alert_ranged`
  ranged-capable, then `alert_melee` melee, never more than `capacity`. Pure
  selection; writes nothing.
- `SudekiMpStoryThreatAlertCommit(config, set, alerting_record, shooter,
  responder_serials, count, responder_records[], now, seq)` → FALSE and
  writes nothing when `count == 0`. Otherwise sets the alerting state
  (`active, shooter, raised_at = last_fire = now, raise_seq = seq, until =
  now + alert_hold*1000, responders[], count`) and writes each responder's
  `responding[set]` copy (`active, shooter, alerting_serial, raised_at,
  raise_seq, until, last_fire = now, recheck_failed = 0`). The only function
  that *activates* alert state; the other writers are `Touch` (`last_fire`
  only), the adapter's `recheck_failed` (B.4), `AlertFade`, `ResponderFade`,
  `AlertClear`, the level-change clear (B.6), the namespace clear and walk
  retirement (B.1).
- `SudekiMpStoryThreatAlertClear(set, responder_record, alerting_serial,
  raise_seq)` clears `responding[set]` only when both values match; a copy
  that does not match survives.
- `SudekiMpStoryThreatAlertFade(config, level, cooperative, set,
  alerting_record, responders[] {record, close, listed}, count,
  shooter_kind, shooter_firing, now, released[], released_count)` → reason
  or none. The adapter passes one entry per serial in
  `alerting[set].responders[]` whose identity still resolves (none for a
  serial without a record); `responders[]`/`count` on the alerting record
  are never compacted. An entry **belongs** to the alert only when its
  `record.responding[set]` is active with `alerting_serial ==
  alerting_record.serial` and `raise_seq == alerting[set].raise_seq`; any
  other entry is ignored — never released, never counted, never cleared
  (a released responder later recruited by another alert is that alert's).
  Among belonging entries it releases individually (clears the copy, writes
  the serial to `released[]`) every one whose `listed` is FALSE or whose
  `recheck_failed` is set. Then it ends the alert with the first applicable
  reason: `level` (`level < TIER15` or `!cooperative`), `down` (shooter kind
  DOWN/ABSENT), `stop` (`now - last_fire > fade_seconds*1000` and
  `!shooter_firing`), `close` (any still-belonging responder's `close`),
  `left` (no belonging responder remains), `expired` (`until` passed). On an
  end it starts the cooldown (`cooldown_active, cooldown_until = now +
  alert_cooldown*1000`) and the adapter calls `AlertClear` for every
  belonging copy. An alert whose shooter is temporarily AI/AWAY survives
  (Decide (a) falls through meanwhile) and ends by `stop` or `expired`.
- `SudekiMpStoryThreatResponderFade(config, level, cooperative, set, record,
  close, listed, shooter_kind, shooter_firing, now)` → reason or none, for
  **orphan** copies only (the alerting record retired): same reasons from the
  copy's own fields (`left` = own `listed` FALSE or `recheck_failed`). On any
  reason the adapter clears the copy with `AlertClear`; when the last orphan
  copy of an alert clears, it logs `alert_end reason=retired`.

### A.5 Idle push (tier 2)

- `SudekiMpStoryThreatPushTrack(set, push_state, engaged_known,
  engaged_by_any, now)` runs first on every `Service` call (B.5), before any
  gate: with
  `engaged_known && engaged_by_any == 0 && !idle_active` → `idle_active = 1,
  idle_since = now`; with `engaged_by_any > 0` or `!engaged_known` → clear
  `idle_active` (unknown is not idle). Engagement during a push or a
  cooldown therefore restarts the idle clock.
- `SudekiMpStoryThreatPushEvaluate(config, level, cooperative, set,
  push_state, seat_kind, firing, now)` → TRUE when `level >= TIER2`,
  `cooperative`, kind HOST/REMOTE, `firing`, `idle_active && now -
  idle_since >= idle_seconds*1000`, no active push, no active cooldown.
  Writes nothing.
- `SudekiMpStoryThreatPushStart(config, set, push_state, serial, now, seq)`
  sets `active, serial, started_at = now, start_seq = seq, until = now +
  push_hold*1000, cooldown_active, cooldown_until = now + push_cooldown*1000,
  recheck_failed = 0`. The adapter calls it only when its Aware search
  returned a serial; with none, nothing is recorded, no cooldown starts, and
  the tracker is left as is.
- `SudekiMpStoryThreatPushEnd(config, level, cooperative, set, push_state,
  seat_kind, engaged_known, engaged_by_other, pushed_exists, pushed_listed,
  now)` → reason or none: `level`, `down`, `invalid` (`!pushed_exists`, or
  `recheck_failed`, or post-start `pushed_listed == FALSE` — the design's
  "leaves the list"; UNKNOWN stays valid), `engaged` (`engaged_known &&
  engaged_by_other > 0`; the pushed enemy's own engagement never ends its
  push), `expired`. Clears `active` on an end; the cooldown stays.

### A.6 Arbitration

`SudekiMpStoryThreatDecide(config, level, cooperative, self_type, excluded,
mode, set, record_or_NULL, push_states, table_complete, now, current,
eligible, kinds, distances, lead_result)` → `{seat, rule ∈ alert | push |
threat | lead | none}`. Admission and the mode gate first; with
`record == NULL` (self has no identity record: unleased, lease observation
failed, no free slot) only (d) runs, so tier 0b is unchanged for those
units. Order: (a) `record.responding[set]` active, `level >= TIER15`,
`table_complete`, shooter `eligible` with kind HOST/REMOTE → shooter;
(b) some `push_states[seat]` active with `serial == record.serial`, `level >=
TIER2`, `table_complete`, seat `eligible` with kind HOST/REMOTE → seat;
(c) `Pick`; (d) `lead_result` when mode is 0xB; (e) none. A seat the native
list does not contain is never named.

### A.7

Keep every function total and allocation-free; invalid inputs mean "no
opinion". Record the formulas and the resolutions of O1/O2 in the header.

## Part B — adapter (`story_enemy_targeting.c`, host game thread only)

1. **Storage.** Extend the `identities[]` entry (serial + epoch, 128 slots)
   with the Part A record (`serial` set when the slot is created) so memory,
   alerting and responding state retire exactly when the identity does.
   Only the walk creates slots, and only for monster/boss units (self type
   1–3, entity vtable 0x2D5FA0 or 0x2D69EC); a leased ally (vtable
   0x2D55D4) never gets one. A hit whose victim is a leased monster/boss
   without a slot is counted (`hit_unslotted`; party and ally victims are
   not counted) and dropped from memory. Per-seat `PushState[4][2]` lives
   beside `scale[]` and resets with it (namespace change, Uninstall). In
   `walk`'s retirement path, before the memset: an alerting record with
   live copies logs `alert_retired serial=%u` (copies become orphans); one
   without logs `alert_end reason=retired`; a retiring responder clears
   its copy; a retiring pushed enemy is seen by `PushEnd` as
   `!pushed_exists` next frame. The namespace clear in `Service` (epoch or
   revision change) and the effective-level clear (B.6) instead log one
   `alert_end reason=namespace|level` per active alert and one `push_end
   reason=namespace|level` per active push before clearing everything;
   nothing becomes an orphan there.
2. **Per-frame unit table.** `walk()` fills, in the same pass that counts
   per-seat engagement, a plain table (≤128 entries, stamped with
   `walk_sequence`, no pointers kept past the frame) of `{serial, x, z,
   type, excluded, ranged_capable (ai+0x46 bit 0 only; the design's
   state-vtable alternative is not used — say so in the header),
   target_seat (+0xC0; 4 = none), aware (+0xC0 empty and +0xF4 non-empty,
   read as the walk reads +0xC0), group_id (lease observation; 0 = none)}`.
   Units enter after the existing liveness filter (`+0x3B`, `+0x44` flags),
   including aware units with no seat target that the walk currently skips.
   Per-seat `{x, z, kind, engaged (every live targeter, leased or not),
   melee, nearest (any partition), position_unknown, firing}` count every
   live unit as today. `position_unknown[c]` is TRUE when seat c's own
   `position()` fails or any live unit targeting c has an unreadable
   position (that unit still counts as engaged). Unleased units are counted
   (`unleased=%u` in the per-second `seat` record) and never candidates; the
   design's zone-cluster fallback is dropped (a unit without a serial cannot
   be designated without keeping a pointer). The table is **incomplete** for
   the frame when `manager_units` fails, the lease observation fails, or
   more than 128 units qualify: then no alert is raised, no push starts,
   `alert_empty` does not increment, table-dependent fade/end reasons are
   skipped, and `Decide` gets `table_complete = 0`.
3. **Hit feed.** In `hit_observe(after)`, when the victim has a slot and the
   source is a seat, call `AddHit` with the journal's amount, max HP,
   `kinds[seat]`, ranged (`!spirit && (missile || ranged_mode)`),
   `spirit_known`/`spirit`, the hit's `firing`, `unengaged = melee[seat]==0 &&
   !position_unknown[seat] && nearest[seat] > melee_reach` from the
   current-frame table, and `far = distance > far_distance` (not the
   journal's `is_far`). Then `Touch` every identity for this shooter. Then,
   if `added_sniper`, `AlertRaise` with `shooter_alert_active` computed over
   all identities in the same set (O2); on TRUE, and only when the table is
   complete and contains the alerting enemy (its position and group are the
   reference), build the candidate array from the table and the candidates'
   own records (`designated` as defined in A.1, within the same set),
   call `AlertResponders`, then `AlertCommit` (no responders → nothing,
   `alert_empty++`), and log `event=alert serial=%u shooter=%u responders=%u
   serials=… unlisted=%u` once. A lethal hit may raise an alert.
4. **Seam.** Restructure the enemy branch so one helper computes this tick's
   per-seat list distances, `live_kinds` and `eligible` for modes 0 and 0xB
   (today `lead_pick` returns on its first line for other modes; keep its
   exclusion and fail-closed returns), records on this unit's record
   `mode_seen, last_mode` (request+0x24, every observed call in every mode,
   0xE included, without reading the global list) and, for modes 0/0xB,
   `list_seen, last_list_mask, last_list_at, last_list_seq = ++list_seq`,
   then calls `Decide` with `lead_result` from the existing lead rule and
   self's record resolved by serial through the lease observation (NULL
   when unresolved). When `Decide` names a seat different from the native
   result, revalidate through the direct 0x5B7120 call under the same scope
   and replace `*out`; a failed recheck sets `recheck_failed` on the
   responding copy (rule `alert`) or the push state (rule `push`), and then
   applies the lead result once (mode 0xB) before keeping the native result,
   so tier 0b behaviour is unchanged. Count and log by rule:
   `lead_substitution` as today; `threat_substitution
   rule=threat|alert|push` change-counted and throttled exactly like it.
   Mode 0xE is **not** substituted; a 0xE call by a unit designated in the
   live set whose native result is not already the designated seat
   increments that record's `lock_blocked` (for the `designated` record)
   and a cumulative adapter counter reported in `status` (a lock already on
   the designated seat is success, not blocking).
5. **Maintenance** in `Service`. On every call: a `push_tracked` flag and
   `push_tracked_at` stamp record the last maintenance pass; a call that is
   not admitted runs `PushTrack` with `engaged_known = 0` for every seat and
   set (unknown is not idle), and an admitted call more than 250 ms after
   the previous pass does the same before its normal pass, so idle never
   accrues across unobserved stretches. On every admitted call, outside the
   walk guard: `PushTrack` for every seat and set (`engaged_known` = table
   complete this frame), then `AlertFade` / `ResponderFade` / `PushEnd` for
   both sets with `level`/`cooperative` from the live or shadow level (co-op
   inactive ends live alerts and pushes with `level` even though the walk
   did not run), then `Touch` for seats with `firing_signal`, then
   `PushEvaluate` per seat and set. Inputs: `listed` / `pushed_listed`
   through the A.1 helpers (UNKNOWN when the table is incomplete);
   `engaged_known` = table complete; `engaged_by_other = engaged[seat] - 1`
   when the pushed serial's current-frame entry targets that seat, else
   `engaged[seat]` (an unleased engager ends the push); `close` = table
   complete `&& !position_unknown[shooter] &&` the responder has a
   current-frame entry `&&` XZ distance(shooter seat, responder entry) `<=
   close_distance` (`STORY_CLOSE_DISTANCE`, default 6), otherwise FALSE
   (`close` has no UNKNOWN state; FALSE skips the reason). The Aware enemy
   for a push is the nearest table entry within `idle_radius` of the seat
   that is `aware`, type 1–3, not excluded, not designated in that set, and
   substitutable.
6. **Config.** Store the effective level once per `read_config`
   (`config.level` keeps the raw value for the log) and pass it everywhere.
   Remove the `owner_gate … higher_tiers=unimplemented` record and replace
   its image-test assertions; keep `level_clamped`; log `event=level
   effective=%s` whenever the effective level differs from the last value
   logged (a `level_logged` flag, cleared at Install/Uninstall; make the
   `level_clamped` latch a file-static cleared there too — today it is a
   function-local static that is never cleared). On an effective level
   change log `alert_end`/`push_end reason=level` for every active alert and
   push in both sets, then clear both sets' alert/push state.
7. **Shadow evaluation.** On every live call and frame, the adapter also
   runs the shadow set with a config copy whose `level` is
   `STORY_TARGET_TIER2` passed through `SudekiMpStoryThreatEffectiveLevel`
   (cap parity off still clamps to tier 0), over the real threat memory:
   the same list distances and kinds, `SudekiMpStoryThreatLeadPick` on that
   copy for `lead_result` (the live `LeadPick` call is unchanged), responder
   and Aware searches in full with `designated` computed within the shadow
   set only, `AlertRaise`/`Commit`/`Fade`, `PushTrack`/`Evaluate`/`Start`/
   `End` on shadow state. The shadow path never calls 0x5B7120 and never
   writes `*out`. Per (resource, mode) `selection` entries gain
   `would_lead=%lu would_threat=%lu would_alert=%lu would_push=%lu`,
   counting shadow decisions whose seat differs from the final `*out`; shadow
   `alert`/`push` records are emitted with `watch=1` under the same bounds.
   testing.md must say watch-level counters are computed against the
   native-cap list (cap parity is off in watch), so the owner compares them
   at `tier0`.
8. **Logs.** `alert` / `alert_end reason=stop|close|left|expired|down|level|
   retired|namespace` / `alert_retired` / `responder_released serial=%u` /
   `push seat=%u serial=%u` / `push_end
   reason=engaged|down|invalid|expired|level|namespace`
   are per-event records bounded per namespace (64 each; design: one line
   per alert), not rate-throttled. `status` gains `threat_substitutions=%lu
   alerts=%lu alert_empty=%lu pushes=%lu lock_blocked=%lu hit_unslotted=%lu`.
   All bounds reset in `reset_selection_logs`.

## Part C — tests (one mutation per rule or guard)

1. **Pure (`tests/story_threat_test.c`)**, `now` in ms, defaults unless
   stated; the fixtures must fix distance and mode where the result depends
   on them:
   - decay halves at `updated_at + 4000` with the default and at
     `updated_at + 2000` with `threat_half_life=2` (a hard-coded 4 s must
     fail); elapsed is clamped at 0 (an `updated_at` a few ms ahead of `now`
     neither wipes nor amplifies); a record with one 0.15 hit at `d = 0`,
     mode 0, decided at last hit + 2 × half_life with no further hit,
     returns 4 (undecayed scoring must fail);
   - `AddHit`: admission (`cooperative=0`, type 0/4/6, `excluded=1`) writes
     nothing; `max_hp <= 0`, a non-finite or negative amount and `seat >= 4`
     write nothing; threat accrues for every seat kind; a hit-all hit adds
     threat and no sniper; sniper needs HOST/REMOTE, ranged, known
     not-hit-all, firing, unengaged and far (flip each); `added_sniper`
     reported correctly;
   - `Touch` with a different shooter leaves `last_fire` unchanged on both
     the alerting and the responding state, so the alert still ends by
     `stop`;
   - the predicates: `substitutable` FALSE for a last observed mode 1 or
     0xE, TRUE for 0/0xB, TRUE when never observed; `ResponderListed` /
     `PushListed` UNKNOWN when never observed or when the observation is not
     after the raise/start sequence, TRUE/FALSE from a post-raise/post-start
     mask; `designated` from an active copy and from an active push with the
     candidate's serial; a passed cooldown is cleared by the next
     `AlertRaise`/`PushEvaluate`;
   - pick: below the floor 4; a current target below the floor is replaced
     by the best above it; sticky within `threat_margin`; an AI and an AWAY
     seat with the top score are not chosen (O1); DOWN/ABSENT never; modes
     other than 0/0xB never; each admission input; `tier0`/`watch`/`off`;
   - alert raise: threshold, `sniper_added` (an AI/AWAY shooter with residual
     sniper above threshold raises nothing), no active alert on this record,
     `!shooter_alert_active` (O2: a second enemy's raise on the same shooter
     is refused while the first alert or an orphan copy of it is active, and
     the refusing enemy's cooldown stays untouched; a raise on a different
     shooter succeeds), no cooldown, `TIER15`, each admission input (flip
     each);
   - responders: an unhit candidate on a zero-threat seat is chosen; a
     candidate with no target is chosen; a target's raw threat above the
     reference is not; at equality it is; the alerting enemy at distance 0
     first in its group even with weighted score below the floor; ranged
     first then melee with exact serial sets and more candidates than slots;
     same group first (both ids non-zero), nearest within a group;
     `alert_radius` boundary and non-finite distance; excluded, type-4,
     non-substitutable, designated and `shooter_listed == FALSE` skipped;
     UNKNOWN admitted; `alert_ranged=32/alert_melee=32` fill at most
     `STORY_ALERT_RESPONDERS_MAX` with nothing written past the array;
   - commit: `count == 0` writes nothing and starts no cooldown; copies carry
     `alerting_serial`, `raise_seq`, `last_fire`;
   - fade: each reason in order; `stop` refreshed by `Touch` from a hit on an
     unrelated enemy; `left` only when every belonging responder's `listed`
     is FALSE, with one lacking the shooter released individually (reported
     in `released[]`) and an UNKNOWN one not counting; an entry whose copy
     belongs to another alert (different `alerting_serial`/`raise_seq`) is
     neither counted, released nor cleared — a responder released from alert
     A and recruited by alert B stays B's, and A ends with `left`;
     `AlertClear` leaves a non-matching copy intact; `recheck_failed`
     releases; `down`; `level` with `cooperative=0` and with a lower level;
     the cooldown afterwards; `ResponderFade` on an orphan copy by each
     reason, after which the copy is inactive; a second alert does not
     recruit an active responder (O2); the alerting enemy as its own
     responder keeps both states consistent;
   - push: `PushTrack` first (engagement during a push or cooldown, or an
     incomplete frame, clears idle so the next push needs a fresh
     `idle_seconds`; a non-firing gap does not); `PushEvaluate` needs
     `TIER2`, `cooperative`, HOST/REMOTE, firing, idle, no push, no cooldown
     (flip each); `PushStart` fields; `PushEnd`: the pushed enemy's own
     engagement keeps the push until `until`, a second engager (including an
     unleased one via the count) ends it with `engaged`, `invalid` from each
     of `!pushed_exists` / `recheck_failed` / post-start `pushed_listed ==
     FALSE` (a pre-start observation does not end it), `down`, `level`,
     `expired`; once per cooldown;
   - arbitration: alert over push over threat over lead; `record == NULL`
     gives only the lead result; `table_complete = 0` skips (a) and (b); a
     push for a different serial is not named; each branch inert for an
     eligible but DOWN or ABSENT seat; an active alert or push decided at a
     lower level falls through; a seat outside `eligible` never named;
     admission and mode: from an active responding copy, an active push on
     this serial, above-floor threat and an eligible HOST seat in mode 0
     (and a non-4 `lead_result` in mode 0xB), each of `cooperative=0`,
     `self_type` 0/4/6 and `excluded=1` returns `{4, none}`, and the same
     state in mode 1 and in mode 0xE returns `{4, none}` (the adapter masks
     these inputs, so these pure cases are their only mutation tests);
   - wrap: raise, cooldown and expiry behave the same with `now` near
     0x7FFFFFF0 and across the 2^32 wrap;
   - config: the clamp when cap parity is off; the responder clamp.
2. **Exact-image (`tests/story_enemy_targeting_image_test.c`)**, with the
   synthetic list builder and the real S2 filter (these substitution cases are
   CONFIRMED_TEST, not CONFIRMED_EXACT_IMAGE; say so in testing.md). Extend
   the `SudekiMpLanStorySpawnLeasesObserve` stub to return several leases
   (the current one returns at most one) and the exact-roster `Service`
   fixture to several enemies, before writing these:
   - a threat pick, an alert responder substitution and a push substitution
     each reach `*out` only after the direct 0x5B7120 recheck; the native
     result passes through on no opinion; a failed recheck of a threat seat
     still applies the lead result in mode 0xB and a failed push recheck sets
     `recheck_failed`; request and unit bytes unchanged (`memcmp`);
   - the selector called repeatedly through an active alert and an active
     push keeps substituting until the fade/end; a unit whose latest call
     was mode 1 or 0xE is neither recruited nor pushed; a designated unit
     calling in 0xE with a native lock on another seat increments
     `lock_blocked`, with the lock already on the designated seat it does not;
   - an excluded mode-0 unit with threat above the floor gets no
     substitution and no memory; a unit without a record (lease observation
     off) keeps the existing lead behaviour (the current unleased-self lead
     tests must pass unchanged);
   - `EnemyTargeting=tier2` with `EnemyTargetingCapParity=off`: tier 0b lead
     substitution still occurs, zero threat/alert/push substitutions, zero
     `alert`/`push` records, exactly one `level effective=tier0` record;
   - `Service` with the exact-roster fixture: a ranged-partition targeter
     (+0x46 bit 0, no +0xF2 flag) 2 units from the shooter makes the hit
     non-sniper and at `melee_reach + 1` restores it; a targeter with a
     broken position back-pointer keeps the hit non-sniper; the hit-journal →
     `AddHit` mapping (passing `is_far` must fail); a freshly-down shooter in
     the list is not substituted; an unleased targeter on the pushed seat
     ends the push with `engaged`; a failed lease observation marks the table
     incomplete and raises nothing; memory, alerting, responding and push
     state retire with identities: manager absence logs
     `alert_retired`/`alert_end reason=retired` through the walk path, a
     namespace change logs `reason=namespace` ends through the `Service`
     path; a `Service` call with `coop=FALSE` during an active alert and
     push logs both `reason=level` ends and nothing resumes when co-op
     returns; a responder at `close_distance` ends the alert with `close`,
     one at `close_distance + 0.01` does not, and an unknown shooter position
     does not; the pushed enemy's pre-start list lacking the seat keeps the
     push until a post-start observation; idle accrued before a non-admitted
     `Service` call or a >250 ms gap does not count, so the next push needs a
     fresh `idle_seconds`; with `firing_signal` clear, the shooter's
     non-sniper hit on an enemy that is neither the alerting enemy nor a
     responder keeps the alert from ending by `stop` until `fade_seconds`
     after that hit (a victim-only `Touch` must fail); the Aware-enemy choice
     for a push (nearest, within `idle_radius`, aware, not excluded, not
     designated, substitutable) with a mutation per filter;
   - watch: threat, alert and push conditions produce non-zero `would_*`
     counters and `watch=1` records while `*out`, request and unit bytes are
     unchanged and no 0x5B7120 call happens; a live designation does not
     block a shadow recruit and vice versa; shadow state does not carry over
     when the ini level changes from `watch` to `tier15`;
   - log bounds asserted by counting records; the `owner_gate` assertions
     replaced by the `level` record.
3. **Part 0** tests per clause (split-control test), see above.
4. **Leftovers from the tier 0 delta review** (low, do them here):
   - `docs/enemy-ai-task.md` seat-kind paragraph: scope "host-side input
     holds do not demote a human seat" to native busy-body/steering holds;
     the observer's background-load hold (and its 1500 ms grace) stops
     packet intake, so a seat reads stale there (pre-existing, unchanged);
   - the >250 ms gap: `companions[].sampled_at` is cleared and then re-stamped
     by `companion_sample` in the same `Service`, so assert the effect —
     after the gap `companion_sample` re-samples immediately (a new
     `companion_state` record) instead of waiting out the 1 s throttle;
   - the worker-thread `CompanionSeatExact` check is masked by
     `AwaySeatExact`'s guard: make a character available and unclaimed in
     the fixture (as the existing `available|=1` case does) and call
     `CompanionSeatExact` for it from the worker.

## Part D — documentation

A new testing.md section (date, dirty revision, DLL SHA256, supported EXE
hash) stating per test what is CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE and the
limits (synthetic list builder, synthetic world filter, no live run, watch
counters against the native-cap list). The earlier 2026-10-19 sections
describe hash-pinned artifacts that truly lacked the tiers: leave their
sentences as they are and add one "superseded by" pointer to the new
section at the top of each. In
`enemy-ai-task.md`: the tier 1 / 1.5 / 2 paragraphs with the resolutions
(no-stealing by raw threat with ties qualifying, the alerting enemy as a
candidate, human-only picks, one designation per unit, ms time base, idle
tracking, `engaged_by_other`, unengaged via nearest, 0xE not substituted and
the lock-blocked count, no zone clustering, the ranged-capability narrowing)
and the O1/O2 answers as owner decisions 10 and 11 (no AWAY aggro; one
active alert per shooter); the Feeds "Engagement" and "Groups" bullets; the
"Switches and table" paragraph (tiers implemented, `owner_gate` retired, new
log vocabulary); the Modes paragraph (0xE-empty substitution not
implemented); the Tier 3 bullet ("decided against, decision 6"); the live
gates wording: tier 1 — after the decay the enemy follows the tier 0b
distance rule again (which may keep it on Tal when he is nearest); tier 1.5
— *up to* two Shooters and one melee turn on her within `alert_hold`, the
switch noticeable on her screen; tier 2 — the push comes *after*
`idle_seconds`. The adapter's header comment. The status-matrix rows for #97
(status stays RESEARCH-ONLY / UNVERIFIED_DIRTY_WORK; update state,
limitations and Next columns; no live upgrade). The plan sentences saying
higher tiers are unimplemented. A §9 entry in `coop-story-plan-54.md`. Keep
#98 (bark) out.

## Out of scope

Tier 3 (decided against); mode-0xE substitution; an audible bark (#98);
host kick; host step-away; companion Attack/Retreat/formation; any client
change; any native write beyond `*out`; AwareForceIn weights, the evict
routine, the lock nibble; the state-vtable ranged scan; unleased-unit
clustering; the LAN arena.

## Acceptance

1. Build with the documented Flatpak toolchain; `git diff --check` clean.
2. Native C11 `story_threat_test.c` with `-Wall -Wextra -Werror`; Wine
   `SudekiMP.StoryThreatTest.exe`,
   `SudekiMP.StoryEnemyTargetingImageTest.exe <supported SUDEKI.exe>`,
   `SudekiMP.StorySplitControlTest.exe`, `SudekiMP.LanArenaHitFeedbackTest.exe`.
3. The full suite with the known non-pass set unchanged.
4. Part D documentation; report file by file; do not commit; do not claim
   live behaviour; O1/O2 are answered (see above) and recorded as
   decisions 10 and 11; report Part 0 separately so the owner can take it
   live first.
5. Owner live gates afterwards (not Codex), cumulative on a passed tier 0
   gate, castle-road ambush, Ailish host / Tal client, each level by ini,
   reading the `would_*` counters at `tier0` first: `tier1` — Tal hits a
   Trooper that is on Ailish and it turns on Tal; after the decay it follows
   the distance rule again; `tier15` — Ailish snipes from range while Tal
   fights and within a few seconds up to two Shooters and one melee turn on
   her, noticeably on her screen, returning when she closes in or stops or
   after `alert_hold`; `tier2` — a free, firing player is engaged by one
   Aware enemy after `idle_seconds` and it stays for the hold; sparring
   partner and absent seats untouched; host-alone and arena inert;
   disconnect / title / rejoin / teardown clean.
