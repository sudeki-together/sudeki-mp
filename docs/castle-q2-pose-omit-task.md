# Task brief: extend Q2 — a non-party entity never refuses the world frame (#54)

**Where to work.** The `shared-simulation` worktree of `codex/shared-simulation`,
dirty on `27f7296`, current DLL `20547266…` (preserved in
`build/artifacts/castle-review2-20547266/`). Work there; do not commit, stash,
reset or reformat; other dirty files belong to the owner. Keep `20547266…` as
a separate artifact: the owner may take it live before this lands, and its
hash-pinned records must keep describing what it does.

Read `AGENTS.md` and follow it. Line numbers drift; function names are
authoritative. Everything is in `src/hooks/lan_story_world.c` and
`tests/story_world_capacity_test.c` unless named otherwise. This brief was
checked against the source twice by independent reviews (three readers, two
skeptics per finding, 2026-10-20); CONFIRMED_STATIC below means the
reviewers read the code.

## Owner decision (2026-10-20)

Q2 of [castle-and-stage11-fixes-task.md](castle-and-stage11-fixes-task.md)
left an entity out of the frame when its `target()` check failed. The owner
extended it: **a per-entity failure of a non-party entity (NPC, door, chest,
scenery, group-spawned monster or ally) on the read-only capture and prepare
paths must not refuse the whole frame.** A party character's failure still
refuses: that is the pairing contract. The castle freeze was the client
waiting on refused frames; one bad entity that fails on every capture can
reproduce it.

**Reviewer's design choice, flagged to the owner: hold before omit (H).**
Omitting has its own cost. `SudekiMpLanStoryWorldFrameInterpolate` refuses a
pair whose count or per-index kind, identifier, generation, bank fingerprint
or submodels differ; a refused pair ends the client present
(`presentation_sample.valid=FALSE`, input cleared with Tal in control); a
returning entity gets a new generation. So an entity whose read fails on
alternate captures would make *every* pair refuse — worse than today, where
only alternate captures are refused and the client interpolates across the
gaps (CONFIRMED_STATIC). H holds a *bound* entity's last published record for
a bounded window when its pose (not its identity) cannot be read. The owner
may overrule H; if so, drop it and keep everything else. H cannot cover
identity failures (A, D, `target()`), so those keep the cost (Remaining
refusals 7).

## What refuses today (CONFIRMED_STATIC)

`catalog()` registry pass (shared by host capture, client prepare and both
recruitment paths):

1. `!readable(entity,0x3c)` → `return FALSE` (stage `registry_entry`).
2. A generic `0x2d5b00` entity whose `entity_identity()` fails → `return FALSE`.
   One clause of that branch, `*(void **)(base+0x2d5b44u+0xcu)!=base+0x3ae30u`,
   reads a slot of the image's resource vtable, not the entity: it is an
   exact-image guard, the same for every entity, and the only image-level read
   in that branch and in the position check. Nothing else in the world
   adapter checks it (`SudekiMpInitializeLanStoryWorld` checks only
   `0x2d65e4+0xc`; `methods_exact()` does not cover it; `origin_image_exact()`
   in `lan_story_collision_owner.c` checks the same slot, as `0x2d5b50`, but
   only for its own seam, so capture and prepare must not rely on it).
3. The same entity failing the CPosition check (`position`) → `return FALSE`.

`SudekiMpLanStoryWorldCapture` (host):

4. The pre-pass probes `read_pose_detail` only for `combatant()` targets and
   removes them on failure (`monster_omitted`); any other non-party target
   reaches the main loop, where `!pose_read` → `ranged_pose_fallback` (FALSE
   for `character>=4u`) → `goto fail`.
5. `SudekiMpLanStorySpawnDeathState` failure (combatants; it runs inside
   `if(pose_read && combatant(…))`) → `goto fail`.
6. `!SudekiMpLanStoryWorldActorValid` (`wire_pose_bounds`, after the
   generation/animation block) → `goto fail`. Such inputs exist:
   `read_native_pose` checks only `selector<animations`, `state<=255` and
   finite rate/time/phase and copies blends unchecked, while `ActorValid`
   requires state in {0,1,64,65,128,192}, rate in [0,256], time in [0,4096],
   blend in [0,1].
7. The post-loop recheck (`target(…)` / `same_target`) → `goto fail`.

`SudekiMpLanStoryWorldPrepare` (client):

8. The filter loop probes `read_pose` only for `combatant()` targets. A
   non-party non-combatant whose `read_pose` fails reaches the main loop's
   `if(!read_pose(&targets[i],&current)) goto fail`. The main loop's
   `pose_supported` is also reachable for non-party targets that passed the
   filter: `pose_supported` → `material_children_closed` →
   `SudekiMpLanStoryMaterialOwnerExact` → `backings_ready`
   (`lan_story_material.c`) uses `TryEnterCriticalSection` on the native
   resource-job lock and worker-owned backing flags, so a second call can fail
   (`material_resource_waiting`, ERROR_IO_PENDING) after the filter's passed;
   and the filter's `(void)prepare_missing_resource` for a later unsupported
   target can run `SudekiMpLanStoryResidencyAcquire` (the native clip load at
   `image+0x223b70`) between an earlier target's filter probe and its
   main-loop reads.

## Changes

**A. Catalog pass (1–3).**
- Item 2's image clause: in `catalog()`'s `0x2d5b00` branch, test
  `*(void **)(base+0x2d5b44u+0xcu)==base+0x3ae30u` *before* calling
  `entity_identity()`; on mismatch keep refusing the frame with its own stage
  (`observe_stage("generic_resource_vtable",identifier,i); return FALSE;`).
  Leave the clause in `entity_identity()` for its other callers. Only the
  per-entity sub-checks of item 2 become skips (`readable(e,0x254)`,
  `readable(e,0x314)`, the `+8` and `+0x2c` vtables, the kind,
  `+0x44==e+0x210`, the lens name; the `!roster` clause cannot fail inside
  `catalog()`).
- Items 1, 2 (per-entity part) and 3: if the entity is a party actor — some
  `c<4u` with `(roster->available_mask&(1u<<c)) && roster->actors[c]==entity`
  (a pointer compare, no read) — keep refusing; otherwise `continue` and
  record it once (G) with cause `registry_entry`, `entity_identity` or
  `position`. The mask test is required: unavailable roster slots are NULL
  and `registry_capture` does not reject NULL entries. Pass `side!=0u` as the
  record's side flag, as `append_candidate` does, never `catalog()`'s
  `write` (both recruitment calls pass `write=FALSE` but are client-side).
  These skips happen before a `candidates[]` slot exists, so record from a
  stack `Candidate`: for the unreadable entry, zero it except `entity` and
  `registry_index=i`; for 2 and 3, set `entity`, `registry_index=i`,
  `vtable_rva=stage_vtable_rva`, `native_kind=kind=stage_kind`,
  `native_identifier=identifier=native_identifier`. The skip does not trust
  the unreadable entry: `registry_still()` still re-proves the registry
  snapshot at the end of the capture or prepare.
- The recruitment paths keep their exact-equality contracts.
  `SudekiMpLanStoryWorldBeginRecruitment` (`CATALOG_BOUND`) still requires
  `count==client_count` and `same_target` per index;
  `SudekiMpLanStoryWorldFinishRecruitment` (`CATALOG_FRAME`) still requires
  `count==client_count+1u && count==frame->count` and per-index matches. A
  skipped bound or frame entity makes them retry, exactly as today's refusal
  and today's combatant and `scenery_unsupported` filters already do. Do not
  relax them here (Remaining refusals 4).

**B. Capture pre-pass (4).** Extend the existing probe from
`combatant(&targets[i])` to every `character>=4u` target. On
`read_pose_detail` failure, look up `old` the way the main loop does
(`previous.kind`+`identifier` in `host_bound` under
`host_seen && host_epoch==party->epoch`, then
`same_target(&old->target,&targets[i])`) and apply H's window test. If H
applies, leave the entry in `targets`/`hp_of`: no memmove, no record, no
`monster_omitted` line, no separate mark array; the main loop decides. If H
does not apply, `memmove` the entry out of `targets` and `hp_of`, `--count`,
and record it with `cause=pose` (`pose_stage`/`pose_detail` from the probe's
`stage`/`stage_detail`); a removed combatant keeps its `monster_omitted` line
as well. Between the probe and the main-loop read only pure reads and
module-local state run (`read_pose_detail` of earlier targets,
`SudekiMpLanStoryRemoteInteriorHeldPose`, `SudekiMpLanStorySpawnDeathState`,
`SudekiMpLanStoryResearch54Monster`/`Name`, `SudekiMpLanStoryAnimTraceWatch`,
the ranged projection helpers, logs) — CONFIRMED_STATIC; a change by another
thread is UNKNOWN. Cost, stated statically in the report: one extra
`read_pose_detail` → `read_native_pose` per non-party target per capture
(two for a held entry), covering the position/forward checks, per-channel
rows and the occurrence loop with its `bank_header_readable` VirtualQuery
when a header falls outside the last proven span. No fixture timing
assertion; the live number is the existing `capture_mean_us`/`capture_max_us`
on `lan_story event=host_frame`.

**C. Capture main loop (4 defensive, 5, 6).** For a `character>=4u` target:
- `!pose_read` (the defensive path; only another thread can reach it after
  B, except for an entry B left for H), a `SpawnDeathState` failure, or a
  failed `SudekiMpLanStoryWorldActorValid` holds the entry when H applies.
- Otherwise remove it in place instead of `goto fail`: record it first (G,
  while `targets[i]` is still the removed entity) with `cause=pose_late`,
  `death_state` or `wire_pose_bounds`, then `memmove` `targets`, `hp_of` and
  `next` down from `i+1`, `--count`, and re-run index `i`. Set `frame.count`
  from `count` after the loop (it is set before the loop today). Do not call
  `ranged_pose_fallback` for a non-party target.
- Generation gaps are safe (CONFIRMED_STATIC): an ActorValid removal happens
  after `++generation`. Every consumer compares generations for equality only
  (`bound_same` and `retained_owner_changed` in `Prepare`,
  `SudekiMpLanStoryWorldFrameInterpolate`, `monster_damage_bound`, the damage
  identity in `ApplyPrepared`); `FrameMatches` compares only party
  generations, which come from the party frame; refused captures already
  discard consumed generations.
- Party targets, the ranged-base projection, `!actor->native_pose`, the global
  `generation==UINT32_MAX` check and the per-entity
  `animation_sequence==UINT32_MAX` check keep refusing (Remaining refusals 5).

**D. Post-loop recheck (7).** A non-party target whose recheck fails is
removed from `targets`, `next` and `frame.actors` together (order preserved,
`count` and `frame.count` updated), recorded with `cause=recheck`, never held
(its identity is no longer proven). A party target still refuses.
`registry_still`, `SudekiMpLanStoryObserverRosterStillExact` and
`SudekiMpLanStoryWorldFrameMatches` stay refusals.

**E. Client prepare (8).**
- Extend the filter-loop probe from `combatant(&targets[i])` to every
  `character>=4u` target: on `read_pose` failure, `++j; continue` (not
  matched, not posed) and record it client-side with `cause=pose`;
  combatants keep `monster_omitted`.
- For each matched `character>=4u` target, keep the filter's `pose_supported`
  output (`selectors[5]`, `empty_channels`, `empty_clip_hit`) with the matched
  entry; the main loop reuses it instead of calling `pose_supported` again.
- Keep the filter's `(void)prepare_missing_resource` exactly where it is,
  right after the failing `pose_supported`. Do not defer it: it decides from
  the globals `stage`/`stage_identifier`/`stage_detail` (and `GetLastError()`)
  left by that failure, so a later call would return `ERROR_NOT_SUPPORTED`
  without loading, and the unsupported `Target` is overwritten by
  `memcpy(targets,matched,…)`.
- Add a per-Prepare flag, cleared at the start of
  `SudekiMpLanStoryWorldPrepare` and set inside `prepare_missing_resource`
  immediately before it calls `SudekiMpLanStoryResidencyAcquire`, whatever
  the acquire returns. After the filter loop, before the party
  `character_unmatched` check (so before the `identity_subset` log, the
  `memcpy` into `targets` and the rebind/`catalog_identity` block): if the
  flag is set, re-probe every matched `character>=4u` entry in order —
  `read_pose` first, then `pose_supported` (reset `empty_clip_hit`/
  `empty_channels` first) against its `filtered_frame` actor — and replace its
  cached outputs. Remove any entry that now fails from `matched[]` and
  `filtered_frame.actors` together (order preserved, `m` updated): a
  `read_pose` failure records `cause=pose`, a `pose_supported` failure keeps
  the `scenery_unsupported … policy=local_state` line. Never call
  `prepare_missing_resource` in this pass; that target retries on the next
  `Prepare`. Why (CONFIRMED_STATIC): the acquire raises a bank reference
  count that renderers sharing the bank can see; its LOADED branch states
  `policy=fresh_preflight_required` ("no pose is applied under the native
  observations made before the file load"); and `ApplyPrepared` does not
  repeat the bank preflight — it re-checks identity and the pointer chain
  (`target_still`, `target_chain_still`), and for changed setters
  `selector_cleanup_closed` (old selector, `starting=FALSE`) and
  `rate_change_closed`, but never `material_children_closed`,
  `selector_events_closed(…,TRUE)`, `resolve_clip` or `clip_time_bounds`.
  Party targets keep calling `pose_supported` in the main loop, which runs
  after every load, and their `!supported → prepare_missing_resource;
  goto fail` path is unchanged.
- After these, no native call runs between a non-party target's last probe
  (filter or re-probe pass) and its main-loop `read_pose` (CONFIRMED_STATIC,
  to be confirmed in the report). The main-loop non-party refusals stay as
  defensive paths, reachable only through another thread (UNKNOWN). Reuse
  slightly widens the gap between the `backings_ready` observation of the
  worker-owned flags and the write; `backings_ready` is "an observation, not a
  lease", and the gap already exists today (`apply_presented_frame` runs
  `SudekiMpLanStoryReplicaApply` between `Prepare` and `ApplyPrepared`).
  Effect UNKNOWN; say so in the report.
- A client-side omission changes the prepared set, so the next apply rebinds
  (`client_rebind`): the filter builds `matched` and `filtered_frame` element
  by element, so `rebind` is TRUE whenever `bound_same` fails in the same
  epoch, and interpolation runs on host frames before `Prepare`, so a client
  omission never refuses a pair. An omitted NPC (vtable `0x2d65a0`) is
  released to the local idle clock by `client_unbound_npc` (Q1). Every other
  omitted kind (door, chest, scenery, monster, ally) stays in
  `SudekiMpLanStoryWorldOwnedRenderers` with a zero ambient delta and holds its
  last applied pose, as `scenery_unsupported` entities already do
  (CONFIRMED_STATIC; visible effect UNKNOWN). Do not widen the Q1 release in
  this change; state the held pose in the report and testing.md.

**F. `SudekiMpLanStoryWorldApplyPrepared` stays out of this change.** Its
pre-write `target_still` identity recheck (`stage=write_owner`) and its
in-write and readback checks (`channel_readback`, `visible_matrix`,
`clip_identity_readback`) verify the native write transaction; omitting an
entity there changes write semantics after `client_bound` is published and
needs its own design. Its failure is poorly named today, so add two
diagnostics-only changes with no refusal change:
- In `apply_presented_frame`, save `DWORD world_error=GetLastError();` right
  after the `SudekiMpLanStoryWorldApplyPrepared` call (before `timing_end`, as
  `prepare_error` is saved) and add `if(!world_applied)
  present_refusal("world_apply",world_error);` with **no early return**: the
  function still runs the cinematic-view restore and returns `world_applied`
  at its end.
- In `target_still`, right after `observe_stage("write_owner",…)`, set
  `stage_entity=t->entity; stage_vtable_rva=0; stage_kind=0;` (a pointer copy,
  no read); then, only if `readable(t->entity,0x3cu)`, set `stage_vtable_rva`
  to the RVA of `*(void **)t->entity` when it lies in `[base,base+0x400000)`
  and `stage_kind=(uint16_t)(*(uint32_t *)(t->entity+0x30u)&0x1fffu)` — the raw
  native kind, never `t->kind` (the wire kind; for a PC 0x0f81). These feed
  only `finish()`'s refusal line; do not move or skip the
  `exact()`/`entity_identity`/`target_chain_still` checks.

**G. Record format.** Rename the event and use one literal format for every
omission cause on both sides (do not print the stage through `%s`: the
fixture stub matches the format string):
`lan_story_world event=entity_omitted side=%s stage=target_omitted cause=%s pose_stage=%s pose_detail=%lu entity=%p vtable_rva=%08lx kind=%04x identifier=%08lx wire_kind=%04x wire_identifier=%08lx detail=%lu\r\n`
- `detail` is always the registry index (the A3 rule). `pose_stage`/
  `pose_detail` are `-`/0 except for `pose` and `pose_late` (the failed read's
  `stage`/`stage_detail`) and `pose_held` (the hold reason saved at the failure
  point, H).
- Causes: the existing `target()`-stage names, `registry_entry`,
  `entity_identity`, `position`, `pose`, `pose_late`, `pose_held`,
  `death_state`, `wire_pose_bounds`, `recheck`.
- Post-catalog callers look up their `Candidate` by entity pointer in the
  static `candidates[]` (valid within the same capture or prepare call); A's
  skips use the stack `Candidate` described in A. One `Target`-based helper
  shares `target_omitted_seen` and `target_omitted_count`. Widen the dedup key
  to `(entity, wire identifier, class)`, where class is `held` for
  `cause=pose_held` and `removed` for every other cause: today's key is
  cleared only when the namespace epoch changes in `catalog()`, so the first
  `pose_held` line would suppress the later removal line. Rule: per side and
  per epoch, each entity logs at most one `held` and one `removed` line (the
  first removal cause wins); say so in the report.
- Budget: 64 slots per side, and an entity can use two. Add a per-side flag
  reset with `target_omitted_count` in `catalog()` and log exactly one
  `lan_story_world event=entity_omitted_budget side=%s recorded=64\r\n` line
  the first time a new key finds the table full. No per-attempt counter.
- `world_omitted` (`total`/`kept`) still counts only catalog-pass omissions;
  A-skips and B–E removals and holds are not in it. Say so.
- No tool, doc or fixture parses `event=world_refused`; the only reader of
  `stage=target_omitted` is the `SudekiMpLogFormat` stub in
  `tests/story_world_capacity_test.c`. Do not edit historical briefs,
  hash-pinned testing.md sections or `research-log.md` — apart from the single
  dated "Superseded for DLL …" line that Docs requires — since they record
  what earlier builds logged. A real refusal is unchanged: host
  `event=refused operation=capture` with `host_frame … world_captured=0`;
  client `event=refused operation=apply` with `present_refused step=…`.

**H. Hold before omit (host, bound entities only).**
- Add `uint32_t read_tick` to `Bound`: the host tick at which the entry's
  last *freshly read* record was committed. Set
  `next[i].read_tick=party->host_tick` only beside
  `next[i].previous=frame.actors[i]` on a non-held entry, after
  `SpawnDeathState` and `ActorValid` have both passed. Never set it because
  `pose_read` succeeded (a death-state or `ActorValid` failure always follows
  a successful pose read), and never measure the window from `Bound.tick`
  (which is refreshed on every committed capture; that is why
  `ranged_pose_fallback`'s 2000 ms check limits only the gap between
  captures, not how long its fallback lasts).
- A `character>=4u` target is held when it fails its pose read (B's probe or
  C's `!pose_read`), its death-state read or `ActorValid`, **and** there is an
  `old` record with `same_target(&old->target,&targets[i])` (identity proven
  this capture by `target()`) **and** `party->host_tick - old->read_tick <=
  HOLD_MS` (a named constant, 2000; unsigned subtraction handles wrap). Past
  the window, or with no matching `old`, remove as in B/C with that cause.
- Apply the hold in exactly one place. At each failure point set a per-entry
  hold flag and save the reason (`pose_stage`/`pose_detail`: the read's
  `stage`/`stage_detail` for a pose failure; `death_state`/0 for a death-state
  failure, since `SpawnDeathState` never calls `observe_stage`;
  `wire_pose_bounds`/0 for `ActorValid`), then skip the rest of that entry's
  per-entity work — the dead/death-count/HIDDEN ORs, `ranged_pose_fallback`
  and the `ActorValid` check on the current read (its failure branch would
  overwrite `stage`) — and go to the hold point. Immediately before
  `next[i].previous=frame.actors[i]`, assign
  `frame.actors[i]=old->previous; next[i].read_tick=old->read_tick;`. The held
  record already passed `ActorValid` when it was published; position, clips,
  generation, `animation_sequence`, HP and every `visual_flags` bit come from
  it. Record the hold once (G dedup) with `cause=pose_held`.
- A held record is identical to the previous one, so the pair interpolates and
  the entity stands still on the client. For a combatant, a drop read during
  a hold is published, and replayed as `pending` on the client, when the
  first fresh record arrives with the same generation; a drop read during a
  hold that expires into removal is not replayed (the return has a new
  generation and `monster_damage_bound` fails) — the same as today's
  `monster_omitted` path (CONFIRMED_STATIC).
- Never hold after an identity failure (A, D or `target()`), and never for a
  party target.

## Remaining refusals and costs a non-party entity can still cause (record these, do not relax them)

1. `ApplyPrepared`'s pre-write identity recheck and in-write/readback
   verification (F). Castle reachability UNKNOWN.
2. The client `resource_fault` latch: a failed `SudekiMpLanStoryResidencyAcquire`
   in `prepare_missing_resource` logs `clip_resource_refused …
   policy=retain_world_until_native_exit`. Every later `Prepare` then refuses
   with stage `clip_resource_unknown` / ERROR_INVALID_STATE (an
   `event=refused operation=apply` line) until uninstall; both recruitment
   calls and `TrailOwner` return a bare FALSE at their precondition (no stage,
   no line), and `AimDirection` returns FALSE with reason `precondition`.
   Deliberate: a failed native call is not proof that nothing changed
   (AGENTS.md).
3. `catalog()` `duplicate_entity` (the same pointer at two registry indices)
   and the A image check (`generic_resource_vtable`).
4. Recruitment equality while recruiting (A). E widens an existing hole: an
   entity filtered at the last bind and still in the post-recruitment frame
   makes `FinishRecruitment` retry every frame while `recruitment.retained`
   holds. The live saves already have Ailish recruited, so the castle gate does
   not reach it; whether recruitment should use the host-set intersection is
   an owner question for later.
5. The per-entity `animation_sequence` overflow and the global
   `generation==UINT32_MAX` overflow (C). `next_generation` is committed only
   by a successful capture and reset only at uninstall; one is consumed per
   new or returning target per committed capture. Unreachable in practice
   (about two years at one per capture; about six days if all 128 targets were
   new on every capture). Say so.
6. The client main-loop defensive refusals (E), reachable only through
   another thread.
7. Interpolation-pair refusals when the host set changes (CONFIRMED_STATIC):
   any entity that was in the last committed frame and is now removed — an A
   skip, a D recheck removal, a `target()` omission, a B/C removal with no
   `old`, or an H hold past its window — costs one refused pair, and its
   return (always a new generation) costs another. A first failure of an
   entity that was not in the last frame changes nothing. H never holds A, D
   or `target()` failures, so a bound entity whose identity, position or
   recheck fails on alternate captures makes every pair refuse and the client
   presentation stops — worse than today. Castle reachability UNKNOWN.

## Tests (each names the mutant that must fail at an assertion, run in isolation)

Fixture plumbing: in `setup()` reset `diagnostic_count=0`, `last_stage=NULL`,
`last_identifier=last_detail=0`, `last_error=0` (only uninstall resets them
today). Widen the `SudekiMpLogFormat` stub to keep the rendered text and three
separate counters: lines containing `event=refused operation=`; records keyed
on `event=entity_omitted side=` (or keep `target_log` on
`stage=target_omitted`); lines containing `event=entity_omitted_budget` — never
key the record counter on bare `event=entity_omitted`, which the budget line
also contains. When matching record fields, use a leading space (` kind=`,
` identifier=`) so `wire_kind=`/`wire_identifier=` cannot match, and match
` detail=N\r` so `pose_detail=` cannot. `capture()` adds 20 ms to
`party.host_tick`; set it explicitly where a test needs a schedule.

1. **Host removal of an unbound NPC.** In its own `setup()`, assert a baseline
   capture keeps the chosen NPC; then call `setup()` again (the only reset of
   `host_bound`, `host_seen`, `host_count`, `host_epoch`) so the corrupted
   capture has nothing bound — otherwise H holds it. Corrupt only a field
   `read_native_pose` checks and `catalog()` selection ignores: CPosition
   `+0x1c` non-finite or ≥1e6, a zero/non-finite forward at `+0x50..+0x58`, or
   a selector beyond the bank (`word(rows[0],0,(192u<<16)|1u)` with
   `animations=1`, stage `channel_pose`); never x or z (selection distance
   becomes INFINITY and the NPC drops out by selection). Assert it is absent,
   the frame produced, sorted and decodes, one record `cause=pose` with the
   exact `pose_stage`, no `pose_late` or `pose_held`. Mutant: B narrowed back
   to `combatant()` (C's defensive branch then removes it as `pose_late`).
2. **Defensive late removal.** A one-shot fixture flag makes the
   `SpawnDeathState` stub, when called for a kept ally (0x0f82 sorts before
   doors, scenery, NPCs and chests), corrupt a later *unbound* NPC's channel-0
   selector after the pre-pass passed. Assert the NPC is absent, the frame
   produced and sorted, survivors keep their generations, one `cause=pose_late`
   record. Mutant: `goto fail` restored on that branch. This simulates memory
   changing between probe and read; it is not evidence it happens in game.
3. **`wire_pose_bounds`.** A new (unbound) NPC with row state 2
   (`word(rows[0],0,2u<<16)`), rate −1.0f at `rows[0]+4`, or blend
   `blends+0xc` = 2.0f passes `read_pose_detail` and fails `ActorValid`.
   Assert absent, frame valid/sorted/decodes, kept generations nonzero and
   distinct, a second capture keeps survivors' generations. Mutant: the
   ActorValid `goto fail` restored. Do not patch the validator.
4. **Death state and `hp_of`.** Give the stub a per-entity failure switch
   (cleared in `setup()`) and adjacent monsters distinct HP (1144 → 40,
   1145 → 70). Fail 1144 on an unbound capture: absent, frame produced, 1145
   published with 70. Mutants: `goto fail` restored; the `hp_of` memmove
   dropped from C (1145 would carry 40). B's `hp_of` memmove is an equivalent
   mutant (no fixture can observe it); keep it and say so.
5. **Hold (H).** Bind an NPC, a monster (CStats `+0x2c` = 100.0f) and a
   second framed NPC (odd index ≥21, e.g. entity 31) with a normal capture at
   host tick T. Then make the first NPC's and the monster's pose reads fail
   and set the monster's CStats `+0x2c` to 40.0f. Captures at T+1000, T+1990
   and T+2000 publish them unchanged (same generation, `animation_sequence`,
   position; monster HP 100, not 40); `SudekiMpLanStoryWorldFrameInterpolate`
   succeeds on a consecutive pair; exactly one `cause=pose_held` record each
   across the held captures. A failing capture at T+2010 removes them, each
   with exactly one `cause=pose` record after its `pose_held` (same epoch).
   Restore the reads: they return with a new generation. Separately, restore
   the reads inside the window and check the first fresh capture publishes HP
   40 with the same generation.
   *Death-state hold:* on a bound monster use test 4's switch while its pose
   reads keep succeeding (and, if the fixture can, set its object's `+0x34`
   bit 4 after binding): held inside the window with the previous
   `visual_flags` (no HIDDEN added) and `pose_stage=death_state pose_detail=0`,
   removed with `cause=death_state` after HOLD_MS.
   *Identity guard:* before the next capture change only entity 31's resource
   handle (`word(entities[31].resource,0,124)`: `bank_fingerprint` changes,
   `target()` still succeeds, `old` is still found) and break its pose read:
   it is absent with one `cause=pose` and no `pose_held` record.
   Mutants: hold removed (Interpolate refuses the pair); window check removed
   (still held at T+2010); `read_tick` refreshed while held, or the window
   measured from `Bound.tick` (both still held at T+2010); `<` instead of `<=`
   (removed at T+2000); `read_tick` set on `pose_read` success (the
   death-state hold never expires); held combatant published with the fresh
   `hp_of[i]`; the hold applied at the death-state point with fall-through
   (HIDDEN appears or `pose_stage` reads `channel_pose`); `same_target`
   dropped from the H condition; the dedup key without the class (the
   removal line is suppressed). Do not jump time in one step from the first
   held capture: under the refresh mutant that gap alone exceeds 2000 ms.
6. **Catalog.** Convert, do not delete, the two existing refusal assertions
   in `diagnostics_and_failures()` (the A3 contract):
   `registry_entries[7]=(void *)1` now expects a produced frame and one record
   with that pointer, ` detail=7\r`, `vtable_rva=00000000`, ` kind=0000`;
   `pointer(entities[0].actor,8,NULL)` expects one record with
   `cause=entity_identity`, `vtable_rva=002d5b00`, ` kind=0f8f`,
   ` identifier=000003e8`, ` detail=0\r`. Add `registry_entries[k]=NULL`
   (k ≥21, only Tal available): produced, one `cause=registry_entry` record,
   pointer NULL, ` detail=k\r`. Add `cause=position` by breaking one generic
   entity's embedded CPosition vtable or `+0x10` back-pointer at
   `entity+0x210`, on an entity the frame would otherwise keep (index ≥21).
   Clearing the global `0x2d5b44+0xc` slot instead must still refuse with
   `stage=generic_resource_vtable`; write
   `pointer(base,0x2d5b44+0xc,base+0x3ae30)` back right after (or in
   `setup()`), or every later test refuses at entity 0. An unreadable roster
   actor (the same pointer in an *available* slot, `roster.actors[2]`, and in
   the registry) refuses with `stage=registry_entry`, its index and pointer,
   and no record. Mutants: each `return FALSE` restored; the roster compare
   removed (a bare `!capture` would not catch it, because `party_membership`
   refuses later); the `available_mask` test dropped (the NULL entry then
   refuses); the image check folded into the per-entity skip.
7. **Recheck.** A one-shot hook in the first
   `SudekiMpLanStoryObserverRosterStillExact` stub call (made just before the
   recheck loop) changes only a door's or chest's `resource` handle — never
   the registry or roster. Assert the entity is absent, one `cause=recheck`
   record, the frame produced and valid, `host_count==frame.count`, and the
   next capture contains it again with a new generation. Mutant: `goto fail`
   restored. A party variant still refuses.
8. **Client omission.** Capture normally first (host and client share fixture
   entities, so the break comes after `capture()`), then set a non-finite
   `position+0x18` on a framed NPC: `read_native_pose` rejects it at
   `transform`; `target()`, `catalog()` and `pose_supported` do not read it.
   Do not use an input that also fails `pose_supported` (the
   `scenery_unsupported` branch already omits those). Use a dedicated helper,
   not `apply()` (which asserts `prepared.count==frame->count`): `Prepare`
   succeeds with `prepared.count==frame->count-1`, `ApplyPrepared` with
   `placements==count`, one client `cause=pose pose_stage=transform` record,
   no `scenery_unsupported` line. Restore it: the next frame logs
   `client_rebind` and poses all. Mutant: E's probe narrowed back to
   `combatant()`.
9. **Client clip load (re-probe pass).** The residency stubs are `assert(0)`
   today; give them a per-test mode cleared in `setup()` (default stays
   `assert(0)`). Choose two framed NPCs, B sorting before A. Before `capture()`
   (`bank_fingerprint` hashes `resource+0x1c`, so a later change fails at
   `bank_compatibility`), give A a non-resident selector: set
   `entities[A].resource+0x1c` to `0x80000000u` and point `entities[A].bank+0x10`
   at a static package whose `+4` header has `row_count` 1 at `+4` and whose
   `+0x14` row 0 has bit 0 of byte `+4` clear, so `selector_available` fails at
   `clip_residency`, detail 0. Inspect records its arguments and returns
   `SUDEKIMP_STORY_RESIDENCY_NEEDS_LOAD`; Acquire records its call, calls
   `check(context)`, gives B a non-finite `position+0x18`, and returns
   `SUDEKIMP_STORY_RESIDENCY_LOADED` (leave A's row bit clear). Assert:
   Inspect and Acquire each called once with A's renderer, bank and selector 0;
   one `clip_resource_prepared` line; `Prepare` succeeds with A and B absent;
   one client `cause=pose pose_stage=transform` record for B; `ApplyPrepared`
   succeeds. Mutants: the re-probe pass removed, or not triggered by the flag
   (`Prepare` refuses at B's main-loop `read_pose`); `prepare_missing_resource`
   moved after the main loop (Acquire never called).
10. **Party unchanged, on its own stage.** Host (a) unbound: Tal (entity 149)
    with a non-finite `+0x1c` on the first capture refuses with
    `ERROR_NOT_SUPPORTED`, `stage=transform` and Tal's identifier — not a later
    `FrameMatches` refusal (`ERROR_RETRY`) — and no record names Tal. Host (b)
    bound: one normal capture binds Tal; break his pose read the same way
    inside the window; the same refusal, no `pose_held` or `entity_omitted`
    record (a hold would publish his previous record, which matches
    `party.actors[2]`, so `FrameMatches` would pass). Client: `Prepare` refuses
    at the main-loop `read_pose` stage with Tal's identifier, not
    `character_unmatched`, and no record. Mutants: the `character>=4u` guard
    dropped in B or in C's `!pose_read` branch (fail a and b), in H (fails b
    only; if H is reachable only from inside B's and C's guarded branches it
    is equivalent — keep (b) and say so), and E's probe moved outside the
    guard.
11. **Log and budget.** Every omission line from tests 1–9 contains
    `event=entity_omitted side=` and no "refused"; a party refusal (test 10)
    gives exactly one `event=refused` line and zero records. 65 distinct
    failing non-party entities give exactly 64 records and one
    `entity_omitted_budget` line. Mutants: `event=world_refused` restored; an
    omission logged before the party refusal; the budget line removed.
12. **F diagnostics.** A forced `ApplyPrepared` identity-recheck failure logs
    `present_refused step=world_apply` once and a refusal line whose entity,
    vtable RVA and raw kind are filled; the cinematic-view restore still runs.

## Docs

Do not rewrite the hash-pinned records (`docs/evidence-index.md`, "Resolving
conflicting evidence", rule 1): the testing.md Part A section and its owner
gate (which applies to `66e851b6…`, `d523d43c…` and `20547266…`), the
follow-ups and second-pass sections, and the §9 Part A entry. Add one dated
line to each: "Superseded for DLL `<new>` by <new section>; earlier DLLs keep
this gate", following the existing "Superseded by …" pattern in the
enemy-targeting sections.

Add a new testing.md section pinned to the new hash and artifact path, with
the mutation rows, the extended Q2 scope, H and the Remaining refusals list.
Its owner gate is the earlier gate with: "no frame refused for a non-party
entity's identity, position, pose, death-state, wire-bounds or recheck
failure", followed by the enumerated remaining refusals; `event=entity_omitted`
in place of the old event text; "watch for repeated presentation-invalid /
interpolation refusals coinciding with `entity_omitted` lines (Remaining 7),
and note any `pose_held` entity that stands still"; and the held last pose of
client-omitted non-NPC kinds (E). Add a §9 entry recording the owner decision
and the reviewer's H choice. In the status-matrix castle row keep that file's
own vocabulary (Evidence `CODE_EXISTS, PROTOCOL_TESTED, UNVERIFIED_DIRTY_WORK`,
maturity RESEARCH-ONLY), point "Last known verification" at the new section,
and write "Next proof required" as two gates keyed by DLL (`20547266…`:
`target()`-stage only; `<new>`: extended). Evidence in testing.md and §9 stays
CONFIRMED_TEST / CONFIRMED_STATIC; no live claim.

## Acceptance

Build with `tools/build-linux.sh` (the documented Flatpak toolchain, all
targets); `git diff --check` clean. Focused Wine tests, all passing: the 21
executables listed in testing.md "Castle/stage-1.1 review follow-ups",
including `SudekiMP.StoryWorldCapacityTest` with the new cases plus its
explicit `priority-boundary` and `party-identity` invocations; and the three
other targets that include `lan_story_world.c`:
`SudekiMP.StoryMaterialOutputImageTest`,
`SudekiMP.StoryMonsterDamageGuardImageTest`,
`SudekiMP.StoryTrailOwnerImageTest`. Native C11 tests (TPtrAudit,
StoryStepAway, StoryThreat). Each named mutant built and run in isolation,
failing at an assertion, not at compile or launch. Full suite with the known
non-pass set unchanged. `graphify update .` after the code change. New DLL
preserved under `build/artifacts/` with its `.sha256` and diff. Report file by
file, including the static cost statement (B), the E no-native-call
confirmation and the dedup rule (G); no commit; no live claim.
