# Review follow-ups: castle capacity, stage 1.1 fixes, crash record (2026-10-20)

**Where to work.** The implementation of
[castle-and-stage11-fixes-task.md](castle-and-stage11-fixes-task.md) sits
uncommitted in the `shared-simulation` worktree of `codex/shared-simulation`
(dirty on `27f7296`, final DLL `66e851b6…`). Work there; do not commit, stash,
reset or reformat; other dirty files belong to the owner.

**Verdict of the review** (five area readers, two skeptics per finding, read-only;
the reviewer re-ran the 17 focused Wine fixtures on `66e851b6…` and all pass):
no high finding and no runtime-safety defect. The capacity change, the
selection passes, the client membership rule, the AwayDrained/AwayReturn
corrections, the runtime wiring fixtures and the Part C record match the brief.
What survived is one artifact-provenance problem (already fixed by the
reviewer, see item 1), one ownership-timing gap worth closing before the owner
judges Q1 visually (item 2), two tests that do not discriminate the rule they
name (items 3 and 4), and a set of small test, assert and doc corrections.
None of it blocks the owner's live gate, which runs on `66e851b6…`. Fix all of
it in one pass; every item names the mutation the new or changed test must
fail. Line numbers drift; function names are authoritative.

## Artifacts and records

1. **Part A DLL provenance (medium, artifact part done).** The "separately
   preserved" Part A candidate `264d42a8…` existed only on tmpfs, with no
   recorded location; the worktree (A+B+C) cannot rebuild it. The reviewer has
   copied it, with `part-a.sha256`, `a-diff.txt`, `a-final.diff` and the
   `part-a-source/` snapshot, to `build/artifacts/castle-part-a-264d42a8/`
   (git-ignored, the tier12 convention) and the final DLL to
   `build/artifacts/castle-final-66e851b6/SudekiMP.dll`; both hashes verified.
   Your part: name those repo-relative paths next to the hashes in the
   testing.md Part A section ("Separate Part A candidate SHA256 …"), in the
   final-section sentence "The separately preserved Part A DLL remains …",
   and in the §9 Part A entry, in the wording of the tier12 records; state
   that `build/mingw32/bin/SudekiMP.dll` is the final A+B+C build. No
   machine path in tracked docs.
2. **Status-matrix row:** add `UNVERIFIED_DIRTY_WORK` to the "Castle
   world-frame capacity / omission" row (every other row citing a dirty
   revision carries it), and rewrite "Next proof required" to the brief's A5
   gate: Tal away then in control, Ailish through the tunnel and back; the
   client keeps presenting with `world_captured=1` throughout;
   `world_omitted` only when the eligible count exceeds 128 or an entity fails
   `target()`; far omitted NPCs idle on the client instead of freezing (Q1);
   no `target()`-stage refusal for a non-party entity (Q2); `apply_mean_us`
   (`event=spectator`), `world_mean_us` (`event=presentation_cost`) and
   `capture_mean_us` (`event=host_frame`) recorded against the 16 ms / 96
   fallback; NPC rebind snap and omitted door state; the crash stays separate.
3. **testing.md Part A gate and fallback:** the "Owner live gate" paragraph
   must carry the same criteria as item 2, say that a Q2 omission is logged
   as `stage=target_omitted` and is not a refusal (a real refusal is
   `event=refused` with `world_captured=0`), and extend the 96 fallback with
   the brief's clause (tests restated as 96 kept / 12 chunks / `count=97`
   refused, the gate reading 96, the session mask widened either way). Add
   "64 → 128 actors, 8 → 16 chunks" to the §9 Part A entry beside
   "World v12→13 / profile 27→28".
4. **Q2 scope statement (docs only; owner question).** Q2 as implemented
   covers `target()` failures inside `catalog()`. Non-party pose-read
   failures after `catalog()` still refuse the whole frame on both sides
   (host `SudekiMpLanStoryWorldCapture`: `read_pose_detail` → transform /
   channel_pose / submodel_pose → `ranged_pose_fallback` FALSE for
   `character>=4` → `goto fail`; `wire_pose_bounds`; client
   `SudekiMpLanStoryWorldPrepare` `read_pose` for a matched target), and so
   do a generic 0x2d5b00 entity failing identity/position in the loop and an
   unreadable registry entry. Castle entries past index 290 have never been
   evaluated live, so whether the walk hits one is UNKNOWN. State this scope
   in the Part A visible-costs paragraph, the gate and the §9 entry, and
   reword "no frame refused for a non-party entity" to "no `target()`-stage
   refusal for a non-party entity". Make no code change unless the owner
   extends Q2 to pose-read failures (the combatant branch's memmove-out is
   the model); the reviewer is asking the owner.
5. **Part C citation.** The H2 bullet's CONFIRMED_LIVE facts (25/25 GEL
   creations and destructions, ~47,000 nodes per list growing ~43/s while
   stationary, TPtrAudit firing on the first lookup) need the evidence record
   `evidence-index.md` requires: measured on 2026-10-20 on the deployed
   stage-1.1 candidate `dd917ee5…` (dirty `27f7296`, `[StoryAreas]
   TPtrAudit=true`), live-mp70 pair Ailish host / Tal client, countryside
   castle-road start, by a read-only `/proc/<pid>/mem` list walk and gdb
   hardware breakpoints on the host (the node count is not a TPtrAudit
   output; the walker is capped at 64). Private artifact id: the owner's
   session log of that date; do not invent one or copy from `.agent-local/`.
6. **Audit owner gate (TPtrAudit latch).** `tptr_audit` reports once per
   process: the `tptr_audit_logged` CAS is reset only by
   `SudekiMpCleanroomEngineSetTPtrAudit` at startup, and the record itself
   says the first lookup of every run consumes it. Rewrite the audit's
   "Owner gate" paragraph: drop "and any suspect lines"; the tunnel outputs
   are no crash, control on each arrival, and the default-off `story_tptr
   event=switch_host` / `switch_client` rows in the 20 s post-switch window;
   the decisive data remain the owner's 1 ms list walk and conditioned
   watchpoint. Change the stage-1.1 §9 gate list's "with the diagnostic" to
   "with the TPtrAudit switch telemetry". Do not re-arm the latch per switch
   without a new brief.
7. **Target-refresh audit row.** "committing through RVA 0xB9D20 each control
   tick" overstates: `SudekiMpLanStoryControlMove` reaches
   `SudekiMpLanPartyControlStoryRefreshTarget` only on an exact-lease move
   tick not held by pause refs (actor+0x2B) or native item use (arbiter+0x50
   bit 0x20) and while `ordinary_world(TRUE)` holds, and
   `SudekiMpStoryTargetRefresh` reaches the 0xBA1C0 acquire only with native
   auto-target enabled (targeter+0x84 bit 2), not ranged/first-person
   (arbiter+0x58), the arbiter+0x50 & 0x0041c410 flags clear and arbiter+0x60
   bit 4 clear. Write it that way (CONFIRMED_STATIC), qualify the H1 sentence
   the same way, and add that whether those gates are open during the castle
   switch window is UNKNOWN.

## Code (small, with tests)

8. **Ambient ownership lags the bind by up to 250 ms (medium).** Q1 decides
   ownership from `client_bound`, but `refresh_frame()` in
   `lan_story_ambient.c` rebuilds `owned[]` only every `REFRESH_MS` and
   nothing invalidates it when the bind changes. After a set change the
   swapped-out NPC stays owned (frozen) and the swapped-in NPC keeps
   receiving the local delta on top of the host pose until the next refresh
   — the exclusion rule the MAX_OWNED fix exists to keep. Add a static
   `ownership_serial` in `lan_story_world.c`, incremented at every ownership
   input change (the `!client_seen || prepared.rebind` bind block in
   `ApplyPrepared`, the `client_bound` removal in `BeginRecruitment`, the copy
   in `FinishRecruitment`, `SetLocalZone` when the pointer changes,
   `SudekiMpUninstallLanStoryWorld`), exposed read-only as
   `SudekiMpLanStoryWorldOwnershipSerial()`; `refresh_frame()` also rebuilds
   when the serial differs from the one it last used. Fixture (ambient
   capacity fixture or ranged projection test): NPC A bound and B omitted,
   rebind so B is bound and A omitted without advancing the clock; the next
   `observe()` leaves B's slot at zero and writes the delta into A's.
   Reverting to time-only refresh must fail it. Record the remaining cost
   (one refused pair per set change) in testing.md.
9. **`owns_renderer` negative cases.** The new binary search is tested only
   for an owned renderer. In `test_ambient_capacity`, after the owned case,
   call `observe()` with an address strictly between two owned entries, one
   below the minimum and one above the maximum; assert `!owns_renderer`,
   `slot==frame_dt_bits`, `substituted` up by one, `excluded` unchanged.
   `return low<owned_count;` (equality check dropped) must fail.
10. **Shop rule lost its coverage.** Rewriting the two
    `effect_renderer_ownership` assertions from `==2` to `==1` for Q1 made
    `local_idle_npc` invisible: `client_unbound_npc` is a strict superset
    inside `OwnedRenderers`, and no other test reaches the real rule, which
    gates the native NPC gesture and turn writes. Keep the Q1 assertions and
    add direct `SudekiMpLanStoryWorldLocalIdleNpc(npc)` assertions: FALSE with
    `client_seen` FALSE; TRUE with this zone and this spawn, `client_seen`
    TRUE, unbound; FALSE when bound; FALSE for a foreign spawn; FALSE with a
    broken zone vtable; FALSE after `SetLocalZone(NULL)` while
    `OwnedRenderers` still releases the NPC. Mutants that must fail:
    `local_idle_npc → FALSE`, `→ client_unbound_npc(e)`, `zone_spawned`
    replaced by TRUE, the `client_bound` loop removed (dropping `!local_zone`
    alone is equivalent; do not require it). Fix the "no local shop" comment:
    that case passes because `client_seen` is still FALSE there.
11. **A3 record: raw kind and +0x34.** `candidate_stage()` reports
    `c->kind`/`c->identifier`, which the pre-pass has already normalised to
    the wire kind and the spawn/gate identity, so a `target()`
    `entity_identity` refusal prints the expected kind, not the bad one. Add
    `native_kind` (`+0x30 & 0x1fff` before the vtable override) and
    `native_identifier` (raw `+0x34` before the spawn/gate substitution) to
    `Candidate`; `candidate_stage()` sets `stage_kind` from `native_kind`;
    `log_target_omitted()` prints `kind=`/`identifier=` from the native
    fields and adds `wire_kind=`/`wire_identifier=`. Selection, ambiguity,
    membership and `identity_changed` keep using the wire fields. Tests in
    `diagnostics_and_failures()`: an NPC (vtable 0x2d65a0) with +0x30 =
    0x0f9a is omitted, the frame produced, the line carries
    `cause=entity_identity kind=0f9a wire_kind=0f9b vtable_rva=002d65a0
    detail=<index>`; a party PC with +0x30 = 0x1f81 refuses with
    `stage_kind==0x1f81` and the index. `stage_kind=c->kind` restored must
    fail both.
12. **Client membership on the recruitment paths.** Only `Prepare` is tested
    with a different client lead and client-only entities;
    `recruitment_at_scale()` calls `setup()` first, so `BeginRecruitment` and
    `FinishRecruitment` run with the host's registry and lead and a
    recruitment-only `CATALOG_HOST` rank would pass. Before
    `BeginRecruitment`: move the client lead (`entities[149]` x=10000) and
    add client-only NPCs at 151..156 (x=10000, registry count raised; keep
    150 as Ailish and out of the memmove range); assert Begin succeeds with
    `client_count==127`. Restore the host view before `capture(&after)`, then
    apply the same divergence before `FinishRecruitment`; assert
    `client_count==128` and `!recruitment.retained`. Mutants: Begin's
    `CATALOG_BOUND → CATALOG_HOST` and Finish's `CATALOG_FRAME → CATALOG_HOST`,
    each rejected by an assertion.
13. **Priority fixture expectations.** Three brief expectations are not
    discriminated: (a) wrapperless scenery — `entities[2]` and `[4]` sit
    beyond the distance boundary, so `include=TRUE` in the generic-scenery
    branch changes nothing; move `entities[2]` inside the kept range (wrapper
    still NULL) and assert `target_log==0 && find(&first,1002)<0` after the
    first capture; (b) the ambiguous pair not counted — capture the
    `world_omitted` line into its own buffer in the log stub and assert
    `side=host total=146 kept=128` (`++eligible` moved ahead of the
    `ambiguous` skip gives 148); (c) retention resets at an epoch change —
    restore the start set, move entity 21 to x=180.2 (inside the margin),
    bump `scene.epoch`/`party.epoch`/`roster.epoch`, capture, assert 1020
    kept and 1021 gone (`host_epoch==epoch` removed from the retained check
    must fail). Until (a) is done, testing.md's "wrapperless scenery
    excluded" is CONFIRMED_STATIC, not CONFIRMED_TEST.
14. **Select-before-ambiguity mutant.** The recorded "cap before ambiguity"
    row is a registry-order candidate truncation, which the priority asserts
    reject anyway. Add the brief's actual mutant — ambiguity marked only
    among `targets[0..n)` after selection — run it, and record that it fails
    on `ambiguous_log==1 && find(&first,1001)<0` and the `i>=21` loop; split
    that assert so the attribution is visible; relabel the old row
    "registry-order candidate cap".
15. **Catch-up decode equality.** `test_large_story_catchup` spot-checks
    `count` and `actors[127].identifier`. Give the non-PC actors distinct
    values (position[1], time[0], animation_sequence, bank_fingerprint, …),
    a non-trivial `transaction`, and compare the decoded snapshot field by
    field (a helper for the actor — no `memcmp`, the struct has padding —
    plus the world header, party frame, target and seed_scene). Mutant:
    `CatchupDecode` writing only `out->world` instead of `*out=s` must fail.
16. **`count=129` case is not deterministic.** With the
    `f->count>MAX_ACTORS` clause removed, `FrameValid` reads
    `actors[128]` past the stack object and fails by accident. Put the frame
    in a struct followed by a valid, sorted 129th actor (static assert that
    `extra` occupies the `actors[128]` slot), set `count=129`, and assert
    refusal; removing the clause must now return TRUE and fail. Keep the
    `ChunkCount(129)==0` check.
17. **Assembly mask bound.** Beside `StoryWorldAssembly`, add
    `_Static_assert(SUDEKIMP_LAN_STORY_WORLD_MAX_CHUNKS <= sizeof(mask)*CHAR_BIT, …)`
    and `_Static_assert((MAX_ACTORS+CHUNK_ACTORS-1)/CHUNK_ACTORS <= MAX_CHUNKS, …)`,
    so a future chunk raise cannot silently reintroduce the dropped-chunk
    freeze. Compile-time only; record as CONFIRMED_STATIC hardening.
18. **Combatant HP table.** Without the `n+4u` reserve a frame can carry far
    more than 60 combatants, but `MAX_MONSTER_HP` is a literal 64 and
    `ApplyPrepared` silently `continue`s when the table is full. Size it from
    `SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS` (feedback[] follows), keep the
    full-table branch with one bounded log per epoch, and add a client apply
    case with 70+ bound combatants asserting every one has a slot and an HP
    drop lands as `pending>0`; `64` restored must fail. Live reachability of
    more than 64 bound combatants is UNKNOWN; not needed for the castle gate.
19. **B1 `shows` increment.** The item-1 interior test asserts `shows`
    non-zero after the next `AwayReturn`, but `member_service`'s `lead_here`
    path already made it 1. Capture `unsigned inside_shows=shows;` before
    that call (a new name; `before_shows` is declared later) and assert
    `shows==inside_shows+1`. A mutant that calls `SplitShowMember` only
    inside the `if(SplitMemberParked(c))` block must fail.

## Acceptance

Build; `git diff --check`; the 17 focused Wine fixtures of the brief plus
`StoryAmbientCapacity`/`StoryWorldCapacityTest` with the new cases; native
C11 tests; the full suite with the known non-pass set unchanged; one
mutation per item recorded in testing.md with the new DLL hash; report file
by file; no commit; no live claim. Keep `66e851b6…` as the owner's live-gate
artifact in `build/artifacts/castle-final-66e851b6/`; say explicitly if
item 8 changes behaviour the owner is exercising (it does: the ownership
switch becomes immediate instead of ≤250 ms late).

## Second pass (2026-10-20, after DLL `d523d43c…`)

Verification of the 19 items (three Opus verifiers, two skeptics per
problem): 18 DONE, item 7 PARTIAL, plus one comment defect and one evidence
wording. All three are documentation; none changes runtime behaviour, and
the reviewer's 17 focused Wine fixtures pass on `d523d43c…`.

20. **Item 7, H1 sentence overreaches** (`docs/step-away-stage1.1-audit.md`,
    H1 bullet). "Native control and target refresh add main-thread traffic
    only on the admitted exact-lease move/ordinary-world path, with … native
    auto-target/ranged/arbiter gates clear" applies the target-refresh gates
    to native control too. Those gates (targeter+0x84 & 2, arbiter+0x58,
    arbiter+0x50 & 0x0041c410, arbiter+0x60 & 4) live only inside
    `SudekiMpStoryTargetRefresh`; `SudekiMpLanStoryControlMove` still calls
    native code when `ordinary_world(TRUE)` is FALSE (dodge when
    `block_world_ready`, then `SudekiMpLanPartyControlStoryMove` with x=z=0,
    which reaches the blend-stop 0xDB070 through `SudekiMpStoryMovementSubmit`),
    and the lease acquire/release through `party_call_ai` is not on the move
    tick at all. Split into two labelled sentences: (a) CONFIRMED_STATIC —
    target refresh adds list traffic (acquire 0xBA1C0, commit 0xB9D20) only
    on the audit-row path; whether those gates are open in the castle switch
    window is UNKNOWN; (b) the native-control lease's own native calls
    (acquire/release via `party_call_ai`; per-tick native Move 0xDAE80 /
    blend-stop 0xDB070 and dodge, which also run with zero speed outside
    `ordinary_world`) are not bounded by those gates, and whether they link
    or unlink party TPtrs is UNKNOWN. Keep the MOVE/SNAP clause and "H1 is
    not upgraded to INFERENCE".
21. **Item 8, header comment** (`src/hooks/lan_story_world.h`). The serial
    declaration was inserted between the H1b `SetLocalZone` doc comment and
    its declaration, and its own comment claims the serial changes "whenever
    the renderer ownership inputs change", which is more than the code does
    (registry membership, vtables and render-wrapper pointers still reach the
    ambient adapter only through the 250 ms refresh). Move the declaration
    below `SetLocalZone` and say exactly what bumps it: the client bound set
    or `client_seen` (ApplyPrepared bind/rebind, Begin/FinishRecruitment),
    the local shop zone pointer, and uninstall. Comment only; the rebuild
    changes the hash, not behaviour.
22. **Item 14, mutant attribution** (`tests/story_world_capacity_test.c`,
    testing.md row 14). Under the select-before-ambiguity mutant the
    `priority-boundary` invocation's `(find>=0)==(i>=21)` loop aborts at i=1
    on the same kept-1001 fact the skipped assert covers, so "also fails the
    i>=21 loop" does not show the displaced slot on its own. Either skip i==1
    in that mode (1001 is the split assert's fact) or add
    `assert(find(&first,1021)>=0 && find(&first,1020)<0)` before the loop,
    re-run the mutant, and record that it trips at 1021; otherwise reword the
    row to say the loop fails at i=1 on the retained 1001.

Acceptance as above; a new hash only if item 21 is applied.
