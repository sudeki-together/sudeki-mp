# Task brief: castle world-frame capacity, stage 1.1 review fixes, castle-crash research record (#54)

**Where to work.** The `shared-simulation` worktree of `codex/shared-simulation`,
dirty on `27f7296`: it holds the verified tiers 1–2 follow-ups (DLL
`009d285e…`) and the stage 1.1 implementation (DLL `dd917ee5…`, deployed to the
owner's stage with `[StoryAreas] TPtrAudit=true`). Work there. Do not check the
branch out elsewhere, stash, reset or reformat. Do not commit; the owner
commits. Report Part A separately as soon as it builds and passes: the owner
wants to take it live first.

Read `AGENTS.md` and follow it: exact-image guards, one adapter per native
seam, workers never touch native objects, the lifetime and teardown rules in
`runtime-lifecycle.md`, the evidence vocabulary in `evidence-index.md`, never
"works"/"fixed" for something that only compiled or passed a pure test. Line
numbers drift; function names are authoritative. This brief was checked
against the source by an independent review (four readers, two skeptics per
finding, 2026-10-20); where it says CONFIRMED_STATIC the reviewers read the
code, where it says UNKNOWN the owner has not decided.

All owner decisions for this task are recorded below (the capacity approach,
Q1 and Q2 in A2). Record each in the §9 entry; the owner checks their visible
effects live.

## Part A — the castle freeze: the world frame's 64-actor table (owner decision: cap + omit)

**What happens (CONFIRMED_LIVE 2026-10-20, three crossings on two builds;
CONFIRMED_STATIC in source).** When the lead crosses the tunnel midpoint
(`WS_Country_NW_to_Castle_S`, x≈21–26, z≈−205) the host background-loads
Illumina Castle (`zone_switch_requested target=illuminacastle
policy=background_load`; registry 220 → 371). The castle registry holds about
106 capture-included entities (66 NPCs `0x2d65a0`, 19 scenery `0x2d5b00`, 13
chests `0x2d5200`, 8 doors `0x2d6430`), the world frame's actor table is
`SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS` = 64 (`src/network/lan_story_world_frame.h`),
and `catalog()` in `src/hooks/lan_story_world.c` does
`if(n==SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS || !target(...)) return FALSE;` on the
65th included entity (live: the chest `ICa_Chest_General1h`, registry index
290). The whole frame is refused every frame (`lan_story_world event=refused
operation=capture stage=registry_entry identifier=00000000 detail=290
win32_error=1237`, then `host_frame world_captured=0 entities=0`), the client
waits for a matching world (`spectator state=6
reason=waiting_for_matching_party_world`, `replica_applied=0`) and its picture
freezes; the client's own apply overflows the same table (`present_refused
step=prepare error=1237`, index 289). With Tal away his host body keeps
following natively (seen to x≈50); with Tal in control the client also clears
its movement input while its presentation is invalid (the
`pending_presentation_invalid` gate in `lan_story_runtime.c` calls
`SudekiMpLanStoryInputClear()`), so the body stops on the host too
(CONFIRMED_STATIC, not yet observed). Only client seats are affected. The
countryside peaks around 56 included entities.

**Owner decision (2026-10-20): both.** Raise the table and never refuse a frame
for capacity.

### A1. Capacity: the constants and everything that follows them

`SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS` 64 → 128, `SUDEKIMP_LAN_STORY_WORLD_MAX_CHUNKS`
8 → 16, `SUDEKIMP_LAN_STORY_WORLD_VERSION` 12 → 13, and with it
`STORY_GAMEPLAY_PROFILE` 27 → 28 in `src/network/lan_party_session.c`. The
profile bump is required, not optional: `SudekiMpLanStoryWorldChunkDecode`
refuses a foreign version byte, but `accept_story_world` and
`accept_story_catchup` drop such chunks silently (the session has no logger)
and `hello_received` checks only the build id, the arena protocol, the game
hash and the profile, so a v12/v13 pair would connect and sit at
`spectator state=6 reason=waiting_for_matching_party_world` — the very
symptom this task removes (CONFIRMED_STATIC). The profile bump makes
`hello_received` refuse the old peer with `SUDEKIMP_LAN_ARENA_REJECT_MAP`,
logged by `lan_party event=peer_phase … reason=`, as profile 25 / world 9 did
(plan §9). Chunks stay 8 actors of 144 wire bytes under the 1468-byte datagram
assert; a 128-actor frame is 16 chunks per host tick.

Everything below follows from those constants. Grep finds the first group;
the second group hard-codes the old limit and grep does not find it
(CONFIRMED_STATIC, reviewers read each site):

- `src/network/lan_party_session.c` `StoryWorldAssembly.mask` is a `uint8_t`
  and the reassembly does `a->mask|=(uint8_t)(1u<<c.index)` then compares
  with `(1u<<a->chunks)-1u`: chunks 8–15 would be dropped and a 16-chunk frame
  would never complete, silently. Widen it (`uint16_t`/`uint32_t`).
- `src/network/lan_story_world_frame.c`: `SudekiMpLanStoryWorldChunkCount`,
  the codec and the reassembly all derive the chunk count from `MAX_ACTORS`;
  `MAX_CHUNKS` is consumed only by the catch-up assert, so leaving it at 8
  compiles and passes every other test while `SudekiMpLanPartyPublishStoryCatchup`
  fails to encode any scene above ~64 actors and that join never completes.
  Add `_Static_assert(SUDEKIMP_LAN_STORY_WORLD_MAX_CHUNKS*SUDEKIMP_LAN_STORY_WORLD_CHUNK_ACTORS>=SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS, …)`
  in `lan_story_world_frame.c`.
- `src/network/lan_story_catchup.c` asserts that `SUDEKIMP_STORY_CATCHUP_MAX_SIZE`
  holds `8 + 2×144 + 32 + 4 + SUDEKIMP_LAN_STORY_FRAME_MAX_SIZE (1100) +
  MAX_CHUNKS × (4 + 1180)`: 20,376 bytes at 16 chunks. Set
  `SUDEKIMP_STORY_CATCHUP_MAX_SIZE` to 20480 and
  `SUDEKIMP_STORY_CATCHUP_MAX_FRAGMENTS` 11 → 20 (the second assert needs
  `MAX_FRAGMENTS × 1024 ≥ MAX_SIZE`; the slot mask is already `uint32_t`).
  The catch-up snapshot is sent once per join, so the extra bytes are
  acceptable; say so in the report.
- `src/hooks/lan_story_world.c` `SudekiMpLanStoryWorldFinishRecruitment`
  tracks bound actors in a `uint64_t survivors` mask (`UINT64_C(1)<<previous`,
  `(UINT64_C(1)<<client_count)-1u`) and refuses with `client_count>=64u` →
  `ERROR_RETRY`. At 128 actors `previous>=64` is undefined behaviour and a
  recruitment with 64 or more bound actors is refused every frame; while
  `SudekiMpLanStoryWorldRecruiting()` holds, `SudekiMpLanStoryWorldPrepare`
  answers `ERROR_IO_PENDING` and the client freezes. Replace the mask with a
  `BOOL`/bitset array of `SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS` entries and the
  bound with the constant. Whether any live recruitment zone reaches 64
  included entities is UNKNOWN.
- `src/hooks/lan_story_ambient.c` `MAX_OWNED` = 128 is the client's ambient
  exclusion list. `SudekiMpLanStoryWorldOwnedRenderers` fills it in registry
  order with every registry entity's position-wrapper renderer (posed or not;
  only effects and `local_idle_npc` NPCs are skipped) plus two model-wrapper
  renderers per party character, and stops at the limit. With the castle
  loaded (registry 371) the list fills before the castle entries (index 220
  and above), so castle actors the host poses would get the local ambient
  delta on top of the host pose, against the exclusion rule in
  `docs/story-effect-clock.md`. A 128-actor frame plus the party wrappers
  cannot fit in 128 in any case. Size the list for every registry entity with
  a wrapper plus the party wrappers (the registry bound `MAX_REGISTRY` is one
  option), keep the `observe()` lookup cheap (it runs per renderer update),
  and log truncation once if any bound remains. Test: more than 128 wrapper
  entities with a late posed entity still owned; the old bound must fail it.
- `tests/lan_party_session_test.c` `test_monster_display_wire` hard-codes the
  version: `CHECK(size==172u && bytes[3]==12u)` and two
  `bytes[3]=11u; … bytes[3]=12u;` patch-and-restore lines. Change them to
  `SUDEKIMP_LAN_STORY_WORLD_VERSION`, `VERSION-1`, `VERSION+1`, always restoring
  the constant; otherwise the size check fails after the bump and the
  restore leaves a foreign byte that fails every later decode.
- Fixed arrays: `lan_story_world.c` `skip`, `targets`, `selectors[][5]`,
  `host_bound`, `client_bound`, `ambiguous_kind`/`ambiguous_identifier`
  (see A2); `lan_story_split.c` ~350 animate-target buffer.
- Memory: the session's `story_world_assembly[4]`, `story_world_frames[8]`
  and the four `catchup[].snapshot.world` frames grow by 64 actors ×
  sizeof(`SudekiMpLanStoryWorldActor`) (152 bytes in memory on i686 mingw,
  not the 144-byte wire size) ≈ 156 KB, plus four `catchup[].bytes` buffers.
  A reviewer measured `sizeof(SudekiMpLanPartySession)` 494,848 → 688,416
  bytes at 128 actors with MAX_SIZE 20480. The session is `calloc`ed (heap).
  Report the old and new size.
- Stack: reviewers compiled with `-fstack-usage` at 128/16:
  `SudekiMpLanStoryWorldCapture` 68,368 → 103,440 B (its `Registry registry`
  of 8192 pointers is 32.8 KB on its own, `Bound next[]` 36.9 KB, `frame`
  19.5 KB), `FinishRecruitment` 57,984 → 83,072, `Prepare` 41,152 → 49,152,
  `BeginRecruitment` 39,520 → 46,176, runtime `present()` 24,992 → 44,448,
  `service()` 13,888 → 23,616; the host chain service → Capture is about
  127 KB on the game thread, whose stack was 1 MB in the 2026-10-20 run
  (CONFIRMED_LIVE, gdb). Acceptable, but put the new candidate buffer (A2)
  in single-owner static storage as `prepared`/`filtered_frame` already are,
  not on the stack, and record the per-function totals in testing.md.
- One world-frame local does run on the client's transport worker:
  `SudekiMpLanStoryCatchupDecode` keeps a `SudekiMpLanStoryCatchup` on its
  stack (≈12.5 KB today, ≈22 KB at 128), reached through `poll_network` →
  `SudekiMpLanPartyPoll` → `receive_message` → `accept_story_catchup` under the
  session lock. That is permitted (plain data, no native object; AGENTS.md
  threading rules). Leave it there; do not move it to the game thread.

### A2. Overflow policy

`catalog()` today fills the caller's `targets[MAX_ACTORS]` in one
registry-order pass through `target()`, its capacity check and the monster
`n+4u` party reserve depend on registry position, and its
`ambiguous_kind`/`ambiguous_identifier` lists are also `MAX_ACTORS` long and
fall through to `duplicate_entity` → `return FALSE` when full
(CONFIRMED_STATIC). The easy wrong fix — `continue` instead of `return FALSE`,
keeping the first 128 in registry order — must be made to fail (A4). Do it in
passes:

1. **Candidates.** Walk the registry once and resolve every included
   entity's wire identity (kind plus `+0x34`, or the gate/spawn identity
   `catalog()` substitutes) into a candidate record `{entity, class, kind,
   identifier, xz distance to the lead}` in a static single-owner candidate
   buffer sized by the registry bound, not by `MAX_ACTORS`. Readability and
   identity failures are recorded as in A3.
2. **Identity over all candidates.** The same-entity refusal (a repeated
   registry pointer or a duplicated party character → `duplicate_entity`,
   which still refuses the frame) and the ambiguous-duplicate rule (two
   distinct non-party entities sharing one (kind, identifier) → both left out
   on both sides) run over the whole candidate set, with the ambiguous lists
   sized like the candidate buffer. Capping before this pass would publish
   the nearer copy of a pair split by the boundary, and the client could
   match it to the other copy.
3. **Selection (host).** Keep, in this order: party actors; group-spawned
   monsters and allies; doors and chests (their state matters to play); then
   NPCs and scenery by XZ distance to the host's lead, nearest first. The
   party is selected first, so the `n+4u` reserve becomes redundant — remove
   it or keep it as an assert. The NPC/scenery part must be **stable**:
   `SudekiMpLanStoryWorldFrameInterpolate` refuses a pair whose count or
   per-index kind/identifier/generation differ, a refused pair ends the
   present with `presentation_sample.valid=FALSE` (and clears input with Tal
   in control), and an entity that re-enters gets a new generation because
   `host_bound` holds only the last kept set (CONFIRMED_STATIC). So within an
   epoch keep the NPCs and scenery from the previous capture's kept set while
   they are still candidates, fill free slots nearest first, and replace the
   farthest kept one only when an omitted candidate is nearer to the lead by
   more than a fixed margin (5 m; a named constant). A set change still costs
   one refused pair; that is the accepted cost of the cap.
4. **Selection (client).** The client must not rank on its own: while it is
   in control its `roster->leader_character` is its own character
   (`client_revision_gap_matches`), its registry streams independently (#42),
   and `SudekiMpLanStoryWorldPrepare` poses only the intersection of the
   client catalog and the frame, so an independent client-side distance cut
   would silently drop entities the host sent (CONFIRMED_STATIC). On the
   client `catalog()` keeps the party, then only registry entities whose
   (kind, identifier) is in the host's set: the received frame in
   `SudekiMpLanStoryWorldPrepare` and `FinishRecruitment`; the bound
   `client_bound[].previous` set in `BeginRecruitment`, which has no frame.
   Client-only entities take no slot; they keep local state.
5. **Targets.** Run `target()` for the kept entries in priority order, then
   the existing `qsort` by (kind, identifier) — `FrameValid`, the chunk
   decoder and the client merge all require that order. Log one bounded line
   per namespace when anything was left out: `lan_story_world
   event=world_omitted side=%s total=%u kept=%u first_omitted_kind=%04x
   identifier=%08lx`.

**Decision Q1 (owner, 2026-10-20): omitted NPCs get the client's local idle
clock.** Today they do *not* idle: `local_idle_npc` releases only NPCs
spawned by the client's own entered door-table shop
(`SudekiMpLanStoryWorldSetLocalZone(SudekiMpLanStoryRemoteInteriorClientShopData())`,
NULL everywhere else), every other registry entity with a wrapper stays in
`OwnedRenderers`, and `lan_story_ambient.c` `observe()` leaves its renderer a
zero delta, so an omitted castle NPC would hold its last applied or locally
loaded pose until it is kept again (CONFIRMED_STATIC;
`tests/story_ranged_projection_test.c` asserts the "no local shop → owned"
case). Change: `SudekiMpLanStoryWorldOwnedRenderers` releases a registry NPC
that is not in the current `client_bound` set while `client_seen`, the same
`client_bound` check `local_idle_npc` already uses, so the ambient clock
animates it locally; an NPC that is kept again returns to host ownership
and host pose on its next bind. Omitted scenery keeps the existing
`scenery_unsupported policy=local_state` behaviour. Keep the shop rule
unchanged. Test: an omitted NPC is not owned and a posed NPC is owned in the
same frame; reverting the release (or the `client_seen` condition) must
fail. The visible cost, a far NPC snapping from local idle to the host pose
when it is kept again, goes in the report and testing.md; the owner will
check it live.

**Decision Q2 (owner, 2026-10-20): a non-party `target()` failure leaves that
entity out, never the frame.** Today only monsters have that rule ("never at
the cost of the frame"), a `target()` failure (identity, position, bank) for
an NPC, door, chest or scenery entity still refuses the whole frame, and the
castle entries past index 290 have never been evaluated live. Change: in the
A2 targets pass a non-party `target()` refusal frees the slot for the next
candidate and is counted in `world_omitted`, with the A3 record of the
failing entity logged once per namespace (`stage=target_omitted`, pointer,
vtable RVA, kind, identifier, registry index) so the owner's walk names it.
Party actors keep refusing the frame: a party identity failure is the
pairing contract, not a capacity case. Test: a fixture NPC whose
`target()` fails (unreadable bank pointer) is omitted, the next candidate
takes its slot, the frame is produced; restoring `return FALSE` for that
case must fail. A door whose `target()` fails is omitted the same way and
logged; its open state then does not mirror the host until it binds again,
which the report states.

The `exclude_zone_spawns` scope option (leave a background-loaded zone's
spawns out until the party's area is that zone) was considered and not
chosen: the castle itself exceeds 64, so it only moves the freeze to the gate.

### A3. Diagnostic

The refusal record must name what it refused and keep the registry index.
`observe_stage`/`finish()` carry only `identifier` and `detail` today. Extend
them with the entity pointer plus its vtable RVA and kind (`+0x30 & 0x1fff`)
when readable, and keep `detail` as the registry index for every refusal
raised inside `catalog()`'s loop:

- overflow: a distinct stage `actor_table_full` (now unreachable; keep the
  log for the assert);
- an unreadable entity (`!readable(entity,0x3c)`) has no readable vtable:
  pointer and index only;
- a failed generic `entity_identity`/CPosition check: pointer, vtable RVA,
  kind, `+0x34` identifier;
- `target()` sets `observe_stage("entity_identity",0,0)` before its identity
  check, so an NPC, door, chest or PC failing there is recorded today as
  `stage=entity_identity identifier=00000000 detail=0` with the index lost.
  Give that stage the same record and carry the registry index on the later
  `target()` stages when reached from `catalog()`;
- the post-loop refusals (`matches!=1u`, `registry_still`) must not inherit
  the last iteration's `registry_entry` record.

### A4. Tests

- **Capture/apply priority** (a fixture that includes `lan_story_world.c`
  the way `tests/story_ranged_projection_test.c` does): a synthetic
  150-entity registry whose order is the *reverse* of the priority order (far
  NPCs and scenery first; doors, chests, group-spawned monsters and the party
  last), with one ambiguous pair (two distinct non-party, non-group-spawned
  entities sharing one (kind, identifier), the second placed after the 128th
  eligible candidate), two far NPCs and two generic scenery without wrappers.
  Expected: both members of the ambiguous pair omitted on both sides, logged
  once and not counted; the wrapperless scenery not included; exactly the
  priority-selected 128 kept; the rest omitted with one `world_omitted`; the
  frame produced, sorted by (kind, identifier), encoded and decoded. Do not
  repeat a registry pointer or a party character here (that is the
  `duplicate_entity` refusal; test it in its own case that still expects a
  refused frame). Mutations that must each fail: `return FALSE` restored;
  `continue` (registry-order truncation); capping before the ambiguity pass.
- **Stability:** two consecutive host captures in which a boundary NPC moves
  slightly keep the same NPC set and the pair interpolates; a pure per-frame
  rank must fail. A move beyond the margin swaps exactly one entry.
- **Client membership:** a client with a different lead position and extra
  client-only entities poses every host-kept entity it holds; a client-side
  distance rank must fail. Cover `Prepare`, `BeginRecruitment` and
  `FinishRecruitment`.
- **Q1 and Q2:** the omitted-NPC ownership case and the non-party
  `target()`-failure case described in A2, each with its named mutation.
- **Recruitment at scale:** drive `BeginRecruitment`/`FinishRecruitment`
  with more than 64 bound actors; the `uint64_t` mask or the `64u` bound
  restored must fail.
- **Codec and session** (`tests/lan_party_session_test.c`, built as
  `LanPartySessionTest`; today it has one single-chunk world case and no
  reassembly case): 128 actors round-trip in 16 chunks through encode,
  decode and the session receive path chunk by chunk (keeping the `uint8_t`
  mask must fail); `count=129` refused; `VERSION-1` and `VERSION+1` bytes
  both refused by the decoder (the `+1` case stands in for "a 12 receiver
  refuses 13", which one build cannot run; weakening `!=` to `<` must fail).
  In the profile-refusal sequence of `test_old_story_movement_profile`, add
  `hello[44]=27` → REJECT_MAP and change the final accepted case from 27 to
  28 → ACK; leaving the profile at 27 must fail. A 128-actor catch-up
  round-trip (encode fits MAX_SIZE and 20 fragments, decode equal) that
  fails with `MAX_CHUNKS=8` — no test calls `SudekiMpLanStoryCatchupEncode`
  or `Decode` today.
- **Cost.** Pure and fixture tests do not prove native timing (testing.md
  §2), so they cannot decide whether 128 fits a 16 ms frame; a fixture time
  goes in testing.md only as a regression number. Ship 128 (the owner's
  decision). The decision comes from the owner's live castle log
  (CONFIRMED_LIVE): `lan_story event=spectator … apply_mean_us` (which
  includes the per-target world preflight in `Prepare`), `presentation_cost
  … world_mean_us`, and the host `host_frame … capture_mean_us`. Today's
  countryside numbers are about 9 ms client apply at ~56 entities and 14 µs
  per entity host capture; linear scaling gives about 20 ms at 128
  (INFERENCE), so write the fallback down now. The 96 variant, if the live
  number exceeds 16 ms: `MAX_ACTORS` 96, `MAX_CHUNKS` 12, catch-up
  `MAX_SIZE` ≥ 15,640 with 16 fragments, `WORLD_VERSION` 14 and profile 29 if
  a 128-actor build has already gone live, tests restated as 96 kept / 12
  chunks / `count=97` refused, the live gate reading 96; the session mask is
  widened either way.

### A5. Docs and the live gate

testing.md section with both DLL hashes, the stack-usage totals and the
session size. Record the protocol change where the world-frame version
history lives: the `docs/coop-story-plan-54.md` §9 entry ("World version 12
→ 13", 64 → 128 actors, 8 → 16 chunks, profile 27 → 28, the catch-up
constants, and the three owner decisions of 2026-10-20: cap + omit, Q1
omitted NPCs idle locally, Q2 non-party `target()` failures omit the
entity); `docs/lan-arena.md` and `architecture-current.md` do not describe
the story world frame, so add nothing there. Status-matrix row for the castle
route (RESEARCH-ONLY, no live upgrade). The owner's live gate: Tal away, then
in control, Ailish through the tunnel into the castle and back; the client
keeps presenting, `world_captured=1` throughout, `world_omitted` only if the
count exceeds 128 or an entity fails `target()`, far omitted NPCs idle on the
client instead of freezing (Q1), no frame refused for a non-party entity
(Q2), and the apply/capture numbers above recorded.

## Part B — stage 1.1 review follow-ups (one pass; every item names the mutation its test must fail)

The review of the stage 1.1 implementation (five area readers, two skeptics
per finding) found one high defect and a set of untested runtime guards and
overclaims. The remote-interior functions themselves are well tested; the
runtime wiring around them is not.

1. **High (CONFIRMED_STATIC) — `SudekiMpLanStoryRemoteInteriorAwayDrained`
   re-parks a returning player inside the shop in the same frame.** It parks
   any away member whose bit is missing from the *passed* scene's
   `inside_mask` without checking `members[c].phase`. The frame dispatch
   calls `HostService(&start_roster,&native,…)` and then
   `service_story_ownership`/`service_story_controls(&native)` with the same
   `native` copy and no re-sample; `SetRemoteInterior` writes only the
   observer's `observed`/`remote_mask`. So in the frame where `member_enter`
   unparks and places the player inside, `AwayDrained` (the else-branch call
   needs only connected && known && away && SplitActive) calls
   `SplitParkMember` again, which finds the record on the exempt list and
   re-pauses the body (INFERENCE for the `away_body_standing` step right
   after `place()`; no live observation). The next return hits the `INSIDE &&
   away_inside` branch of `AwayReturn`, which never unparks, and the offer
   gate logs `offer_parked` until the lead leaves; `parked[]` is cleared only
   by `full_party`/`ended`/`Uninstall`. The re-park is skipped only when the
   client's 250 ms BUSY retry is consumed by `service_story_ownership` in
   that same frame. Fix: `AwayDrained` decides from the member record (phase
   INSIDE/LOADING or `away_return` in progress → never park), not from the
   scene bit; `AwayReturn`'s `INSIDE && away_inside` path answers 0 (BUSY)
   while `SplitMemberParked(c)` and unparks through the path `member_enter`
   uses, so a parked body can never be handed control. Test, in
   `tests/story_away_interior_test.c`: keep its stateful
   `SplitParkMember`/`SplitMemberParked`/`SplitUnparkMember` stubs
   (`parks`/`parked_body` already model the re-park; the real
   `lan_story_split.c` cannot join that translation unit — its statics
   `base`/`readable` collide and its records need the preflight fixture —
   and removing the stubs would drop the Unpark stub's `snap_changed &&
   !base[RVA_SNAP_FLAG]` check). Model the frame's `native` copy separately
   from `observed`: after `AwayReturn` has made member 2 LOADING, take
   `SudekiMpLanStoryScene frame=test_scene;`, run the
   `member_service(2,&test_roster,&frame,…)` step that calls `member_enter`
   (the `SetRemoteInterior` stub keeps OR-ing into `test_scene` as the
   stand-in for `observed`; do not make it non-mutating globally or the
   existing INSIDE step at now=50000 ends with `scene_cleared`), then call
   `SudekiMpLanStoryRemoteInteriorAwayDrained(&test_roster,&frame,2)` and
   assert `!parked_body && parks==0`; then that the next `AwayReturn` returns
   1 with `shows` incremented and `!members[2].away_inside`. Removing the
   phase guard must fail it.
2. **`AwayReturn` refuses ordinary returns.** Its generic gates (`ready`,
   scene READY, `!ObserverHolding`, exact roster) run before the
   `!SplitMemberParked → return 1` early-out, so a plain return during a
   background-load hold's 1.5 s grace is answered UNAVAILABLE ("Could not
   step away - try again", no retry) instead of being admitted as in
   stage 1. Reorder as: (1) the argument checks (`!ready || !r || !s || c>=4u
   || c==leader` → −1); (2) the not-parked early-out restricted to members
   with no interior state — `!SplitMemberParked(c) && !(phase==INSIDE &&
   away_inside) && !(away_return && phase==LOADING)` → 1 — never an
   unconditional early-out ahead of the `INSIDE && away_inside` branch: that
   member is normally *not* parked, and answering 1 there would skip
   `SplitShowMember` and leave `away_inside` set so `member_service` keeps
   skipping the exit trigger and stale checks; (3) the scene-READY /
   `ObserverHolding()` / roster gates, where holding now answers 0 (BUSY, the
   client retries) as `away_host_admit` already does for AWAY, and
   scene-not-READY / roster-not-exact still answer −1; (4) the `INSIDE &&
   away_inside` branch (with item 1's parked → unpark/BUSY) and the rest in
   their current order, so `SplitShowMember` still runs only on an exact
   roster. Update the return-value comment in `lan_story_remote_interior.h`.
   Tests: a non-parked IDLE return with `holding=TRUE` answers 1 with no move
   and no unpark (assert `unparks==0`; leaving the early-out after the gates
   or deleting it must fail); guard 5 (holding) in
   `tests/story_away_interior_test.c` changes from `<0` to `==0` with the body
   still parked; the existing `AwayReturn(...)==1 && !members[2].away_inside`
   assertion stays green.
3. **Runtime wiring has no fixture.** Both runtime fixtures
   (`story_step_away_host_test.c`, `story_split_control_test.c`) stub
   `SplitMemberParked` → FALSE, `AwayDrained` → no-op, `AwayReturn` → 1, and
   no fixture calls `service_story_ownership`. Add a host fixture (own CMake
   target, the `-fwhole-program -ffunction-sections -fdata-sections` /
   `-Wl,--gc-sections` pattern of `StoryStepAwayHostTest`) with a `TakeCommand`
   stub and recording seams, and one mutation each for: the `parked ||` offer
   gate (`offer_parked`); the `AwayReturn` result mapping (`entry<=0` →
   BUSY/UNAVAILABLE, candidate committed only on OK); `CancelAwayReturn` on a
   reversed AWAY; the 64-line `away_request_traces` bound; the `AwayDrained`
   calls on drain success and in the else-branch; the `activity_attempted`
   branch; and the post-service `SetEligibility` publication with `&~away`
   (the second B1 site). For that last one the state must be explicit,
   because `split_mask` can contain an away character only when the
   per-player loop skips the stepped-away drain: `activity_attempted`,
   `split_attempted` and `story_policy_initialized` TRUE; a known exact
   roster; `SudekiMpLanStoryObserverHolding()` TRUE (the 1.5 s post-hold
   grace); an away player with an acquired, non-draining lease whose
   `SudekiMpLanStoryControlExact` stub returns TRUE (the existing host
   fixture's stub returns FALSE). Assert the `(mask, away)` arguments seen by
   a recording `SudekiMpLanStorySplitSetEligibility` stub; dropping `&~away`
   or passing `0u` must fail. Do not use the real `lan_story_split.c` owner
   for this mutation (it applies `mask&~away` itself, so the mutant is
   equivalent there); optionally mutate that inner `&~away` in
   `StorySplitPreflightTest`. The disconnect-then-reconnect case (away bit
   cleared, park record kept) must show `offer_parked` refusing the lease.
4. **B5 arming untested.** Add a fixture that drives the real vanilla path
   of `begin_zone` once for TEMP and once for EXIT: no `split_owner`,
   `SudekiMpLanStoryAreaFollowCalling` stubbed FALSE, `background_switch_tick=0`,
   `ObserverHolding()` FALSE, `GetTickCount` on the fixture clock. TEMP:
   `IdentityUnknown` TRUE right after the call. EXIT, with `observed.world`
   non-empty so `SudekiMpLanStoryObserverWorldLoading()` stays TRUE: TRUE right
   away and FALSE at +20000 ms while `WorldLoading()` is still TRUE — the
   EXIT arming's only visible effect is that deadline, so a TRUE-only EXIT
   assertion passes without it. Deleting the arming (`lan_story_observer.c`
   ~392-393) must fail (TEMP half) and narrowing it to `kind==TEMP` must fail
   (EXIT half). The existing `away_vanilla_context` case sets
   `vanilla_transition_pending` by hand and does not count. Also cover the
   cast transient branch (`lan_story_cast.c` ~593, `IdentityUnknown → return
   TRUE`) and the runtime witness `IdentityUnknown() || CastSuperseded()`
   (one mutation each).
5. **Decision-4 filter clause.** `route_skill_filter`
   (`spirit_instance_abi.c` ~1433): add a negative test — a foreign
   controller with `update_fault` set is *not* passed through and its +0x84
   is unchanged; deleting the `address(image+0x408da4, controller)` clause
   must fail.
6. **Swap constraint on the host path.** The "swap WAIT_ACK stays BUSY" host
   assertion sets only `control[1]`; add RESERVED/HANDOFF for a READY HUMAN
   seat so `.swap_involves=0` in `away_host_admit` fails.
7. **`ended()` clears parked records.** In `tests/story_split_preflight_test.c`
   (the only fixture that compiles `lan_story_split.c`, owns `parked[]` and
   already has the native pause doubles of `parking_cases()`): park a member
   (`parked[c].actor` set), run `ended()` while the record is held, assert
   `parked[c].actor==NULL` and `!parking_held()`. Deleting the
   `memset(parked,0,sizeof(parked))` in `ended()` must fail. Record it next to
   the B2 rows in testing.md.
8. **TPtr walker test:** own node at the head, then a heap node, then a
   foreign stack node → `SUDEKIMP_TPTR_STACK` with the count asserted (a
   head-only or stop-at-own walker must fail); assert `count` in the
   own-exemption case.
9. **A6 stale-presentation gate.** The brief asked for an Esc-only gate; the
   implementation also gates on `client_input_ready`, which `away_hold_offer`
   clears at the top of every `service_client_control` pass and sets TRUE
   only after every pending check passes, and `service_client_control` runs
   before `service_step_away` in the same frame — so the real gate is FALSE
   for invalid and stale presentations, M is silently ignored, and the
   owner's "Waiting for the host…" notice can no longer appear there
   (CONFIRMED_STATIC). Restore the Esc-only gate: pass `connected &&
   !SudekiMpLanStoryMenuCapturesInput()` as `admitted` and drop
   `client_input_ready &&`. Test in `tests/story_step_away_runtime_test.c`:
   after `reset()` set `client_input_ready=FALSE` (what the same frame's
   `service_client_control` leaves; `reset()` sets it TRUE, which would let
   the test pass on the unfixed code), `presentation_sample.valid=FALSE` (an
   invalid presentation, not merely an old receipt — with `reset()`'s fresh
   combat sample a valid-but-stale one decides STEP_AWAY),
   `client_local_selected=TRUE`, Esc closed, `raw_m=TRUE`; call
   `service_step_away()`; assert the edge is consumed, nothing opens, no
   away intent is set and `last_notice` is "Waiting for the host…". Mutation:
   adding `client_input_ready &&` back must fail. Keep the Esc-open case (same
   setup, `esc=TRUE` → no notice); dropping
   `!SudekiMpLanStoryMenuCapturesInput()` must fail it. If you believe the
   stage-1 admission order requires the extra term, keep the brief's
   behaviour and raise it as a question in the report; do not change the
   answer silently.
10. **Castle audit omissions** (`docs/step-away-stage1.1-audit.md`), each
    with a lifetime verdict:
    - `lan_story_remote_use.c`: 16 native `CALL 0x4015B0` front-handle copies
      redirected to the opener's party-slot handle inside the remote-use
      scope. An open can start only through `SudekiMpLanStoryRemoteUseTry`,
      called under the runtime's in-control `admitted` gate; once started the
      scope is reopened from the `Open` record alone by `update_wrapper` (the
      native treasure-node update and `SudekiMpLanStoryRemoteUseService`, once
      per host frame) and by `remote_use_finish_enter`, nothing re-checks
      control, and no drain or step-away path cancels the open (the runtime
      uses `RemoteUseActive` only to zero buttons). The redirect therefore
      continues until state 0 or 8, abort in state 1 after `ABORT_MS` (5 s),
      or release after `OPEN_LIMIT_MS` (20 s), whatever the opener's control
      state by then (CONFIRMED_STATIC). The row gives the verdict for both
      windows: the in-control start and the post-drain continuation.
    - `armour_menu.c` `refresh_preview`: a stack TPtr built through 0x4015B0
      passed by value to 0x560F90.
    - `story_ai_movement.c` (the MOVE/SNAP gate) and the per-tick
      `SudekiMpStoryTargetRefresh` call in `control_separation.c` (commit
      through the 0xB9D20 intrusive helper): Part C names them, so they need
      rows. Note for the move gate: a reviewer's objdump of the supported
      image shows `AI_MOVE_FRAME` (0x4F4BB0) copies controller+0x248 into a
      stack local with `CALL 0x4015B0` and ends it with `CALL 0x4015E0`
      *before* the patched compare at 0x4F4CB3, on every AI tick, whatever the
      gate decides (CONFIRMED_EXACT_IMAGE) — so that pair is not
      in-control-only traffic.
11. **TPtrAudit.** Copy every node's bytes before the first log call into
    static storage owned by the `tptr_audit_logged` CAS winner (not a stack
    array: a local copy deepens `tptr_audit`'s own frame and overwrites more
    dead stack than the logger's 1 KB buffer does); document that
    `tptr_audit`'s frame and the `VirtualQuery` calls in `readable_memory`
    can still overwrite nodes near SP before they are read, that the walk
    cannot tell a live caller's node from a dead one, and that the live-run
    rule is "a stack node that persists longer than ~5 ms across lookups is
    dangling". Add the post-switch telemetry the research asked for,
    default-off behind the same `[StoryAreas] TPtrAudit` key as stage 1.1
    decision 6, outside r54's shared `LINE_BUDGET`, bounded to 20 s after each
    background zone switch (the observer's `background_switch_tick` window,
    through a getter), sampled on the game-thread story service seam where
    `SudekiMpLanStoryResearch54Sample` is already called, never from a worker.
    Host: once per second, each member's native position (position+0x18),
    pause refs (`actor[0x2B]`), AI override (`*(int16_t*)(*(actor+0x94)+0x16A)`)
    and AI mode (`*(uint8_t*)(*(*(actor+0x94)+0x3C)+0x0B)`), readability
    checked as `roster_identity_locked` does. Client: while the spectator
    state is other than 4, the last presented party positions, on every
    state change and otherwise once per second — the same gate `present()`
    already uses for the `spectator` line; `present()` runs every render
    frame, never log per frame. Note in testing.md that *all* r54 event kinds
    share one 40,000-line `LINE_BUDGET` per process (not the party trace
    alone) and that today's long runs exhausted it.
12. **Docs.** In the §9 2026-10-20 table narrow these clauses to
    CONFIRMED_STATIC until the named fixtures exist, leaving the rest of each
    row CONFIRMED_TEST (A2's PREPARE ACK, input and READY claims and A4's
    holding refusal have rejected mutations): A2/A4 "Request logs are bounded"
    (item 3) and the host-path "involved swaps … remain BUSY" (item 6; the
    pure `SudekiMpStoryAwayAdmit` rule stays CONFIRMED_TEST); B1 "Outside drain
    parks; an INSIDE visit is marked away" (call sites stubbed, item 3);
    B3/B4 "places before enabling return", "Failed unpark leaves it
    away/unmoved", "Reversed return intent cancels only the synchronous
    placement wait" (item 3); B5 "starts immediately" (item 4). Narrow
    testing.md's "validated return transaction"; annotate the research
    passages that still cite the lines deleted by the B6 retirement; record
    for the Kamo-join gate that `press_to_control_ms` must be read (the host
    door fade of 1.6 s plus a fresh offer competes with the client's 5 s
    RETURNING fallback — UNKNOWN whether it ever exceeds it).

## Part C — castle crash: research record only (no fix in this task)

Record in `docs/step-away-stage1.1-audit.md` (sanitized) the outcome of the
2026-10-20 research on the 2026-10-19 crash (DLL cfa8f432), with these
labels: the fault is the second `ActorEntity(TAL)` lookup in
`roster_identity_locked`, which returned the correct entity with ESI=0
(CONFIRMED_LIVE crash registers + CONFIRMED_STATIC); ESI is restored from the
saved-ESI slot in `entity_from_gel_pointer`, so that slot was overwritten
with 0 while the frame was live (CONFIRMED_STATIC). The only main-thread
store in that window into memory the lookup does not own is the GEL
destructor's unlink at 0x401B9E (`[next+4] = node.prev`, 0 for the head node;
CONFIRMED_STATIC/CONFIRMED_EXACT_IMAGE; 0x401B50 is the destructor body,
0x401B30 its deleting wrapper). If the main thread made the bad write, Tal's
list head at that call was a dead stack node lying on that frame's
saved-register slots (INFERENCE: Wine's stack dump starts at ESP, so the
node's address is not observed). The first TAL lookup at the same ESP in the
same call did not clobber (CONFIRMED_STATIC), so that node was not Tal's list
head then and became the head between the two lookups (INFERENCE; whether it
was newly created or already deeper in the list is UNKNOWN).

Leading explanation H1 — **HYPOTHESIS, not INFERENCE**: no source or live
record yet shows any non-main thread linking or unlinking a party actor's
TPtr, so the audit's "UNKNOWN: whether another thread was involved" stands
until the probe below. Another thread, for example the two background loader
workers 0x5BB100/0x5BB1E0 that run during the castle load and execute queued
job objects through virtual calls, mutates the unlocked intrusive list: the
link-at-head copy 0x4015B0, the assignment 0x401750, the unlink 0x4015E0 and
the destructor 0x401B50 all take no lock (CONFIRMED_STATIC). Only the
in-control case adds per-frame main-thread list traffic that would make such
a race reachable: the native-control lease and the per-tick target refresh in
`control_separation.c` (the move-gate copy/end pair runs every AI tick
regardless of the gate, see B10). Alternative H2 (HYPOTHESIS): a leaked
main-thread node. Live facts from 2026-10-20 that belong in the record: the
mod's own lookups are balanced (25 GEL creations / 25 destructions in a
window, the same object recycled); Tal's and Ailish's reference lists each
hold ~47,000 nodes and grow by ~43 per second while the party stands still,
from a deep native call chain (origin UNKNOWN — not the mod's lookup path);
the `TPtrAudit` diagnostic fires on the first lookup of every run because
live caller frames legitimately hold linked nodes. The decisive probe
(read-only `/proc/<pid>/mem` walk of Tal's list at 1 ms during an in-control
crossing, then a hardware watchpoint on Tal+4 conditioned on a non-main-thread
stack) is the owner's next live step once Part A is in: a write from a
non-main-thread stack supports H1, only main-thread writes support H2.

## Out of scope

Any change to the party/world pairing contract on the client ("one complete
party/NPC observation"); mode-0xE substitution; the crash fix itself; host
step-away; the LAN arena; the shop rule in `local_idle_npc` (Q1 changes only
the omitted-NPC case).

## Acceptance

1. Build with the documented Flatpak toolchain; `git diff --check` clean.
2. Focused Wine tests: `LanPartySessionTest`, the new capture/apply priority
   fixture, the new runtime host-wiring fixture (item 3) and the B5-arming
   fixture (item 4), each registered in `CMakeLists.txt`; plus
   `StorySplitControlTest`, `StoryStepAwayHostTest`, `StorySplitPreflightTest`,
   `StoryAwayInteriorTest`, `StoryStepAwayRuntimeTest`,
   `StoryTemporaryContextTest`, `SpiritInstanceAbiTest`, `TPtrAuditTest`,
   `StoryRangedProjectionTest`, and the enemy-targeting set unchanged; native
   C11 tests where they exist.
3. The full suite with the known non-pass set unchanged; one mutation per
   item above recorded in testing.md.
4. Part A reported separately with its DLL hash as soon as it passes; the
   full report file by file; no commit; no live claim.
