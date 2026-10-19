# Task brief: step-away stage 1.1 (fixes from live play and review), saved-story co-op #54

**Where to work.** The uncommitted stage 1 step-away work (from
[step-away-task.md](step-away-task.md), §9 entry "2026-10-18 — #54 client
step-away stage 1" in `coop-story-plan-54.md`) exists only in the existing
`shared-simulation` worktree of branch `codex/shared-simulation`. Work there.
Do not check the branch out elsewhere, stash, reset or reformat. Other dirty
files in that tree (the #87 weapon/menu/audio work) belong to the owner.

Read `AGENTS.md` first and follow it: exact-image guards, one adapter per
native seam, socket/transport workers never touch native objects, lifetime and
teardown rules in `runtime-lifecycle.md`, the evidence vocabulary in
`evidence-index.md`, and never write "works" or "fixed" for something that
only compiled or passed a pure test. Line numbers below are from the current
dirty tree and will drift; function names are authoritative.

## Live results of stage 1 (owner, 2026-10-19, Ailish host / Tal client)

CONFIRMED_LIVE: step-away in a fight (`M decision=STEP_AWAY`, drain reason
`stepped_away`, one `offer_stepped_away`); the AI fights; banner and AI label
shown; own-body orbit; when combat ends the AI sheathes and follows Ailish;
Q and F1 do nothing while away; M returns control in and out of combat; M out
of combat opens the Main Menu and closing it never steps away; Esc Away and
Return to Play in and out of combat; host M in combat shows the notice;
leaving the game while away (or not) hands Tal to AI.

Failed: **Tal away, Ailish enters Kamo's shop → both screens show only the
skybox** (part B). Also seen: a **host crash during the whole-world switch to
Illumina Castle** (part E, separate) and **a rejoining client missing the dark
storm sky** of the castle-road ambush (part D).

## Owner decisions for this task (2026-10-19)

1. **Away body at doors.** When the lead goes into any house (shop or story
   house), the away character's body waits hidden at the door, the vanilla
   way. In vanilla every temporary-zone door, shops included, places only the
   lead: followers wait paused and hidden and are placed on the lead at exit.
   No native take-along exists; members enter a shop only through D11/H1b. No
   AI walks inside. While the lead is inside, the away player keeps the street
   view with the banner `<Lead> is inside <place> - press M to join`.
   (Watching the lead inside through the house camera is stage 2.)
2. **Returning (M or Esc Return to Play) while the lead is inside:**
   - in a **door-table shop** (D11 lets members in): loading screen, then the
     player appears just inside the door, a step aside from the exit trigger,
     with the camera on them, through the existing H1b member entry and the
     lead's-shop placement (the path used when Tal walks into Kamo's himself);
   - in a **story house** (lead-only under D11): for now the player comes back
     outside at the door, like a member who stayed out. The owner's target is
     inside with her (#93, #96); that needs story-house support on clients and
     is stage 2.

   If the lead leaves while the player is still away, the native exit places
   the body on her outside and the AI follows.
3. **Keep the dead-lead hold armed while any player is away** (the
   pre-step-away behaviour), so the hold's rotations never hand the host's
   controller to an away body.
4. **After a Spirit-camera (story cast) fault, the host's own cameras and
   control filters keep working**; only owned Spirit/instance cameras stop.
5. **Retire the wrong 2026-10-14 claim** that a native take-along put Tal
   inside the farmhouse (its own host log shows Tal outside, paused, and placed
   at the door only after the exit).
6. **The castle crash gets an audit and a diagnostic only**; no weakening of
   identity proofs in this task.

**Cannot arise (owner, 2026-10-19):** a downed away body parked at a shop door.
The lead cannot use a door during combat, and outside combat the party is
always together (in vanilla the others either fight beside the lead or wait
as placeholders until the map loads); a whole-party death is game over and a
save reload. Keep only a defensive guard: if it is ever reached, refuse the
return entry and leave the body parked (log it).

**Not decided (UNKNOWN; do not rely on them, ask the owner):**
(b) whether a parked body in the live exterior can be hit by enemies or NPCs
(vanilla would suspend the exterior): observe it during the story-house gate;


**Decided later the same day:** the story cast also retires when no remote
player is READY or retained, e.g. after a disconnect (owner: yes; see B5).

## Part A — review fixes (each confirmed by a second reviewer)

**A1 (high) Dead-lead hold disarmed for away peers.** `lan_story_runtime.c`
~1945–1960: an away peer counts only while `host_control[p].native_key.token`
is set, so after the stepped_away drain `SetActive(FALSE)` disarms the hold and
the away actor leaves `remote_actors` (live: `story_death_hold event=disarmed`
right after the drain). Then the host lead going down (Previous rotation
0x423F60 from 0x4241C9 / 0x42428C, skipped only while the hold is active) or a
script naming the away character as lead (pending rotation at 0x4231E4,
skipped only for actors in `remote_actors`; the castle-road ambush script
names Tal) moves the host's controller and camera onto the away body, and that
player's Return loops. Fix per decision 3: an OBSERVING peer that is away
still arms the hold and its actor stays in `remote_actors`. Scope: this covers
the hold's rotations (0x4241C9, 0x42428C, 0x4231E4); the lead-removal rotation
0x423433 stays uncovered as before (research G). Known effect, accepted by
decision 3: with the hold armed and the host lead down, enemies idle
(CONFIRMED_LIVE, §10 G row); that is the pre-step-away behaviour, not a
regression. Update the code comment at ~1945.

**A2 (medium) Away while the host's offer is in PREPARE freezes the body.**
Reached by Esc Away (or M in combat) while the host's control offer is still in
PREPARE (for example right after a door, house or world load), and by the 5 s
return fallback. Host admission treats PREPARE as BUSY
(`.prepare = token && !ready && !draining`, ~1739; `SudekiMpStoryAwayAdmit`),
and the client stops acknowledging PREPARE once `want_away` is set
(`service_client_control` returns at `if(!offered || away_client.want_away)`
before the PREPARE ACK). Required fix: while `want_away` is set and the host has
not yet set `away`, the client still sends the PREPARE ACK
(`SudekiMpLanPartyAcknowledgeStoryControl`), keeps input cleared, and never
drives a READY lease; the host then reaches READY and admits. Optional: admit
away on the host during a plain PREPARE only in ownership phase IDLE, stating
that this revises the stage-1 admission rule. Never drain or admit away during
a party swap (`swap_involves`): that stays BUSY and resolves through the ACK.
Give the host's `story_step_away event=request` line a bounded counter.

**A3 (low) Epoch rebind zeroes `released[]`** (`control_separation.c`
~8089–8101) while another seat's key, released before a world load, still needs
it to finish its drain (3+ players). Do not zero those entries (generations are
monotonic, so stale entries cannot match a new key).

**A4 (low) Away accepted in the 1.5 s grace after a background-load hold**
(`SudekiMpLanStoryObserverHolding()` TRUE but the roster exact,
`lan_story_observer.c` ~1144): `background_load_hold` (~1983) skips the drain;
a Return then leaves ownership DRAINING on a live READY lease. Preferred fix:
answer BUSY to an AWAY while `SudekiMpLanStoryObserverHolding()` and the seat
holds a non-draining token (bounded by the hold; the client retries every
250 ms). If a DRAINING→HUMAN transition is chosen instead, it is a new
`party_ownership.c` transition; add `PartyOwnershipTest` cases and keep the
arena `lan_party_presence.c` behaviour unchanged.

**A5 (low) H1b away guard fails open on a stale presence.**
`SudekiMpLanStoryRuntimeCharacterAway` (runtime ~335–340) is shared: on clients
it drives the `(AI)` name tag (`lan_story_name_tags.c` ~209) and the Esc menu
Away/Return rows (`lan_story_menu.c` ~393). On the host (`!local_seat`,
`story_policy_initialized`) read `story_presence.ownership.away` directly (no
staleness window). Keep the session-presence read on clients. Fail closed only
in the H1b admission: `entry_member_admitted` treats an unknown answer as away
(tri-state result or a host-only query). Display callers keep treating unknown
as not away.

**A6 (low) M routing lost the stage-1 gates.** `away_m_decision(mine,live,now)`
replaced the stage-1 `owned && admitted` gate, so M can open the Main Menu under
the Esc party menu, during PREPARE, an offer gap or a swap (~2593). In phase
NONE pass the `owned`/`admitted` already computed at the top of
`service_quick_menu` (they include the Esc-menu gate and the READY fence),
restoring "not owned or not admitted → nothing". Drain the raw edge when the
press is not acted on. Apply the same Esc-menu gate in the stale-presentation
branch of `service_step_away` (~2507).

**A7 (low) Esc Away / Return to Play leaves `menu_open` TRUE**
(`lan_story_menu.c` ~263): F8 stops working and the Players page shows "Menu
open". On a successful AWAY/RETURN command set `menu_open=FALSE` and log
`lan_story_menu event=closed` as CLOSE does; leave it unchanged when the
command is refused (`party_menu.c` `activate()` closes the panel only on
success).

**A8 (low) Untested claim.** The §9 entry says `PartyOwnershipTest` covers
no-op requests "consuming their ID"; no test asserts it (deleting
`s->last_request[p]=request;` in the unchanged-intent branch, `party_ownership.c`
~211, survives). Add the assertion. In the new §9 entry, mark the stage-1 "1.3
Death hold" row superseded by decision 3 and A1 (its rationale was wrong), and
state that the "consuming their ID" claim had no assertion and its mutation
survived the 64-of-64 count until now.

## Part B — doors while away (addresses the Kamo void)

**Root cause.** The step-away drain zeroes Tal's lease; split eligibility counts
only acquired, non-draining leases, so `outside_mask` became 0; `begin_temp`
declined silently (`lan_story_split.c` ~602–605); `keep_exterior` gave
`keep_live=0`; the door ran the vanilla temporary-zone entry, which replaced the
observer epoch (6→7→8) (CONFIRMED_STATIC + log). The bound story cast's witness
(`retained_actor` → `exact()` → `SudekiMpLanStoryObserverNativeRosterExact(&owners)`)
can never succeed after that; `SudekiMpLanStoryObserverIdentityUnknown` is FALSE
during a TEMP load, so the instance light latched `light_fault` (line 221, site
296) and the next spirit-instance Enter latched `update_fault` (site 2226,
failure 505) (CONFIRMED_STATIC + log). The named-camera pointer hook on the
shared CCamera slot 0x2CCE70 (native 0xE7660) then returns before the native
update for every camera, including the host's "default" (CONFIRMED_STATIC; the
camera-freeze class is CONFIRMED_LIVE in commit 0c8a9a7). So the house loaded
but Ailish's view stayed at the suspended exterior pose (INFERENCE: the sky-only
picture has not been read live). `update_fault` also makes `route_skill_filter`
drop the arrival script's SetControlFilterAll (host control-locked), and the
host binding stays at `filter_none`. The client latched `presentation_changed`
on the epoch change and followed a mislabelled `inside_mask` into its own Kamo
shop.

**B1 Away members keep the split.** In `lan_story_runtime.c` add
`away_retain_mask(roster)`: bit c for p=1..3 when `story_presence.ownership.away`
has p, the peer is OBSERVING with transport confirmed, no swap blocks p, c is
the peer's character, c != leader, c is available and has an actor. Compute it
from presence regardless of drain state, so the window between the away request
and drain completion is covered. **Away wins:** at both publication points an
away character is removed from the held mask, so it is never treated as an
in-control outside member. Pass it as a third argument to
`SudekiMpLanStorySplitSetEligibility` in `refresh_split_eligibility` and in the
post-service publication. Update every test site of that signature together:
the recorder stub in `tests/story_split_control_test.c` (~106), the call in
`tests/story_split_preflight_test.c` (~207) and the stub in
`tests/story_step_away_host_test.c` (~78); add stubs for the new Park/Unpark
exports in tests that include `lan_story_runtime.c` or
`lan_story_remote_interior.c`. In the drain-success branch of
`service_story_controls` (~2019–2024), when `SudekiMpLanStorySplitActive()`:
if the drained character is standing outside, call
`SudekiMpLanStorySplitParkMember(c)`; if he is INSIDE an H1b visit, see B3.

**B2 Split accepts away members** (`lan_story_split.c`): store `away_mask`;
`begin_temp` begins on `(outside_mask|away_mask) & ~lead`, and its
`event=begin` line adds `away=%u`; SetEligibility's `event=eligibility` trace
adds `away=%u` too. When `begin_temp` declines while the host is ready, log once
per class `lan_story_split event=begin_refused reason=no_members`. `lead_only`:
eligible on `outside_mask|away_mask`; sample `refs_before` for away members too;
after the trampoline exempt outside members as today and record away members as
parked (actor, refs_before, added) **without** `node_resume`, so they keep the
native pause and door hide. Add `SudekiMpLanStorySplitUnparkMember` (native
thread; split active and settled inside; exact actor and group,
`group+0xD0==1`, pause count ≥ added; resumes exactly the added refs and moves
the record to the exempt list so `full_party` re-pauses before the native
FullParty) and the inverse `SudekiMpLanStorySplitParkMember` for an outside
member who steps away mid-split. `full_party`, `ended` and Uninstall clear the
records; Uninstall refuses while any record remains.

**B3 Return into the lead's shop** (`lan_story_remote_interior.c`
`SudekiMpLanStoryRemoteInteriorHostService`, and the return path in
`lan_story_runtime.c`). When a parked away player asks to return while the lead
is inside a door-table shop, the split is active, the body is not downed (defensive guard; see "cannot arise"), no house is draining and no other house is in use: run the
existing H1b member entry for that character as if he had used the door
(LOADING with the door fade, ZoneState 3, exact roster, then the lead's-shop
placement a step aside from the exit trigger and `SplitShowMember`). Log (new)
`story_remote_interior event=away_return_entry zone=%s character=%u`. In
`member_enter`, after `snap_off()`, unpark (B2); on refusal finish without
moving and stay away. Only after the placement completes does the ordinary
offer path hand control back (never a lease on a paused or hidden body). The
client loads its own copy of the shop as for a walked-in visit; its camera is on
the player's body. From then on it is an ordinary H1b visit. Door-touch entries
stay refused for away members (an AI body never enters on its own). Show
`<Lead> is inside <place> - press M to join` on the away client while the lead
is inside. **A member who steps away while already INSIDE the lead's shop**
(H1b visit) is not parked: his body stays inside, visible, under the native AI;
flag that visit so it ignores the exit trigger (otherwise the AI can walk into
it and be placed outside unparked and unleased) and leaves only with the lead
(evict); Return there is immediate and clears the flag.

**B4 Story houses, and shops when the player stays away.** The away member
stays parked at the door (vanilla). On Return while the lead is in a story house
(decision 2) the offer path re-acquires him **outside** at the door: unpark
first (or refuse the offer with `offer_parked` until unparked), so a lease is
never acquired on a paused, hidden body. When the lead comes out with the player
still away, the native FullParty/Show bring him back and the AI follows.

**B5 Defense in depth (the vanilla path can still run** — disconnect, swap
pending, no retained member):
- `IdentityUnknown` returns TRUE while the scene is not READY after a vanilla
  TEMP/EXIT, bounded (about 20 s, like `local_transition`).
- Add `SudekiMpLanStoryCastSuperseded()`: bound, the observer published a READY
  epoch different from `owners.epoch`, and a **fresh**
  `SudekiMpLanStoryObserverNativeRosterExact` proof of the current READY scene
  (with its name lookups) yields the same world/group/controller/actors/AI as
  `owners`. Evaluate it at most once per service or transition and share the
  result (prove-once, like `enter_task`/`leave_task`). Wire the cast-light and
  spirit-instance unknown witnesses (runtime ~3880–3881) to
  `IdentityUnknown() || CastSuperseded()`.
- `SudekiMpLanStoryCastService` must not set `fault` (`lan_story_cast.c` ~569)
  when `!exact()` means only that the epoch was superseded: rebase `owners` to
  the runtime-supplied roster (already proved exact at the new READY epoch) when
  world, group, controller, actors and AI are equal, or treat the state as
  transient until the retire. `drain_story_casts` and `TryBind` accept that
  state; the cast rebinds through the existing runtime path (~2185–2193).
- Retire when superseded and fully drained. The plain
  `SudekiMpLanStoryCastUninstall` path cannot do this as written:
  `SudekiMpDestroyCastLight` → `lm_owner` calls the retained witness, which
  calls the stale `exact()`. Give destruction a superseded-aware owner proof.
- Also retire the cast binding when no remote player is READY or retained (for
  example after a disconnect), so a later house entry cannot trip it (owner,
  2026-10-19).
- Decision 4: after `update_fault`, `named_camera_update` still runs the native
  update for a **non-owned** camera node when no instance scope or operation is
  active and the primary manager/camera globals are exact; owned instance
  cameras stay blocked. `route_skill_filter` must not swallow the script's
  SetControlFilterNone/All for the host's own controller, and the host-binding
  filter-entry proofs (`SudekiMpSpiritInstanceFilterNoneEntryExact` /
  `AllEntryExact`, which require `!update_fault`) accept the hook in that host
  pass-through state, so `host_binding_ready` can recover. Known residuals
  (record as UNKNOWN in §9; changing them needs the owner): after a Leave
  failure the globals stay swapped and this fallback stays blocked; `light_fault`
  still freezes world-light updates; epoch-keeping faults (for example battle
  start) with the cast bound while a member is away are not covered by
  `CastSuperseded`.
- `inside_mask` for a non-split TEMP: at the first READY publish only the lead
  (`1u<<lead`); the runtime then widens it through a new observer setter once
  `SudekiMpLanStoryAreaMembershipObserve` region membership (region =
  sector>>8) for that epoch and revision is exact. Unknown membership never adds
  members.
- Client: when the host epoch changes, re-seed presentation instead of latching
  `presentation_changed` forever, when the new scene is READY, the area
  matches, the available masks match and frame generations are unchanged;
  otherwise latch as today.

**B6 Docs (decision 5).** Mark RETIRED the 2026-10-14 farmhouse "native
take-along put Tal inside" claim in `coop-story-plan-54.md` (~4898–4911 and the
§10 chart) and in `coop-story-research-54.md` (H1rest), citing its own log, and
annotate the passages that rest on it (plan ~6529; research ~636, ~643, ~656,
~1364) with: vanilla temporary-zone entry is lead-only.

## Part C — tests (one mutation per new guard)

- **A1:** in `tests/story_step_away_host_test.c` turn the
  `SudekiMpLanStoryDeathHoldSetRemoteActors` stub into a recorder; flip the two
  existing `!dead_hold` assertions to `dead_hold`; assert the hold stays armed
  and the away actor is in the recorded `remote_actors` before and after the
  stepped_away drain. Add a new test that includes `lan_story_death_hold.c` and
  asserts `death_hold_decide` / `pending_hold_decide` skip both rotations for
  that actor.
- **A2/A4/A6:** `tests/story_step_away_runtime_test.c` and host fixtures: ACK
  continues while `want_away` is pending; swap WAIT_ACK stays BUSY; AWAY answered
  BUSY while holding; M ignored under the Esc menu and without a READY fence.
- **A3:** in `story_epoch_fixture` (`tests/lan_party_control_image_fixture.c`),
  after the epoch-11 rebind, `SudekiMpLanPartyControlStoryDrain`/`LoadDrain` of
  the key released before the rebind still returns TRUE. Mutation: restore the
  `ZeroMemory(&story_native_control.released,...)`.
- **A5:** in a fixture that compiles `lan_story_runtime.c`
  (`tests/story_step_away_host_test.c`): with `ownership.away` set and no fresh
  session presence, the host query reports away; the display path still reports
  not away when unknown. `tests/story_away_interior_test.c` checks only the
  gate's use of the query.
- **A7:** a new fixture that includes `lan_story_menu.c` (or a small extracted
  helper): `menu_open` FALSE after a successful AWAY/RETURN, unchanged after a
  refused one. **A8:** the `last_request` assertion.
- **B1:** recorder of `(ready, held, away)`: away bit set from the request on
  (drain window included) and removed from held; excluded: lead, swap-blocked,
  disconnected, unavailable; on return the bit moves to held; ParkMember called
  for an outside member and the inside flag set for an INSIDE one.
- **B2:** `tests/story_split_preflight_test.c`: with `outside_mask=0` and the
  member's bit in `away_mask`, `begin_temp` succeeds and `keep_exterior`
  returns TRUE; exactly one `begin_refused` line when nobody qualifies.
  `tests/story_split_control_test.c`: `lead_only` records an away member parked
  with refs unchanged; Unpark/Park balance; Unpark refused when not settled
  inside, wrong actor, changed group, depth≠1, wrong thread; Uninstall refuses
  with records.
- **B3:** `tests/story_away_interior_test.c`: return-entry armed only for a
  parked away player with the lead inside a door-table shop and the split
  active; not in a story house (return goes outside), while draining, with
  another house in use, or for a downed body (defensive guard); `snap_off` before unpark; unpark
  refusal keeps the player away and unmoved; control offered only after
  placement; an INSIDE member who steps away ignores the exit trigger and leaves
  with the lead; door-touch still refused.
- **B5:** `tests/story_temporary_context_test.c`: `IdentityUnknown` TRUE after a
  vanilla TEMP/EXIT until READY and FALSE past the bound; a non-split TEMP's
  `inside_mask` is lead-only until membership widens it.
  `tests/story_cast_admission_test.c`: a superseded epoch sets no fault;
  retire only when idle (refused mid-strike); the cast rebinds afterwards.
  Spirit-instance/cast-light tests: superseded witness takes the transient
  branch; after `update_fault` a non-owned node with neutral globals still runs
  the original while an owned node stays blocked; the Leave-failure case stays
  blocked; filter-entry proofs recover. Client presentation re-seed only under
  the stated conditions.

## Part D — rejoining client misses the host's storm sky

Root cause (CONFIRMED_STATIC + log): `lan_story_sky.c` keeps its client follow
ledger (`applied[]`, `dome_applied`, `light_applied`, `blend`) across
uninstall/install, so a rejoin skips host sky serials it thinks it already
applied; the client's own zone scripts then write the default sky over the
host's. The host republishes its full retained state (throttled to about
100 ms); no wire change.

- At Install, reset the follow ledger to process-start values (move `blend`
  above Install).
- Re-arm on local native writes, client only: in `record()` after the
  `replaying || thread` guard, clear the applied serials for the written kind.
  Sky colour, blend and brightness (kinds 0–2,
  `SUDEKIMP_STORY_SKY_COLOUR/BLEND/BRIGHTNESS`, `lan_story_effect_state.h`) are
  one family, so they replay in serial order; fog colour, fog range and vertical
  fog (3–5) each clear only themselves. In `dome_hook_fn` clear `dome_applied`
  inside the existing `self==scene() && !replaying && native thread` guard (so
  replays never re-arm themselves). Do not re-arm the cumulative light ring.
- New logs (`story_sky event=follow_reset reason=install`,
  `story_sky event=local_override kind=%u`) are client-only and get their own
  bounded counter (or reset the 5000-line budget at install). Do not reuse the
  name `applied` for locals (one already shadows it in
  `SudekiMpLanStorySkyReleaseHeld`).
- State in §9: during a rejoin's zone-entry loop the dome may swap Storm↔Light
  a few times before settling; the host's retained value now overrides
  client-local sky writes in the host's area. Record as UNKNOWN: the light
  ring's rejoin behaviour; host-side `latest[]`/`light_ring`/`dome_name` not
  cleared on re-host; spawn-group `replayed[]` keyed on host epoch, which a
  rejoin does not change.
- Tests: extend `tests/story_sky_light_image_test.c` (reinstall resets the
  ledger; local writes re-arm the right kinds per family; replays do not
  re-arm; logs client-only).
- The rejoin was not slower in itself (6–8 s vs 15.5 s for the first join after
  the fade); the extra time was leaving through the title and lobby.

## Part E — castle crash: audit and diagnostic only (decision 6)

Host crash (build cfa8f432) during `zone_switch_requested target=illuminacastle`
from New Brightwater: page fault reading 0x3C in `roster_identity_locked`
(`lan_story_observer.c` ~840), reached from `named_camera_update` →
`SudekiMpLeaveSpiritInstance` → `caster_exact` → `retained_actor` →
`SudekiMpLanStoryObserverNativeRosterExact`. ESI (holding `&r`) came back 0 from
`SudekiMpCleanroomEngineActorEntity(TAL)`. Every function on that path preserves
ESI (CONFIRMED_STATIC); ESI is restored from a stack slot in
`entity_from_gel_pointer`, so that slot was overwritten: memory corruption,
most likely the GELPointer destructor's intrusive TPtr unlink (0x401b9e,
next->prev = 0) acting on a dangling node in Tal's reference list that points
into a dead stack frame (INFERENCE). The node appeared between the two TAL
lookups of one roster check (INFERENCE from CONFIRMED_STATIC: the first lookup
at the same ESP did not clobber). The one state difference from the two earlier
castle switches that did not crash: during the crashing switch Tal was back
under SudekiMP native control (`ai_override=1`, `actor_owned`); in the earlier
ones he was stepped away.

- **Goal (owner, 2026-10-19):** Tal must be able to walk with Ailish, in
  control, through the tunnel into Illumina Castle and back. Until the cause is
  found, the workaround is to step Tal away before the tunnel (both
  non-crashing switches had him away; a pattern from two runs, not a
  guarantee). This is required for the Illumina Castle ↔ New Brightwater
  milestone.
- **Audit focus first:** the whole-world switch with an in-control client:
  `before_world_change` / `ReleaseBeforeWorldChange`, `LoadDrain`, the
  post-load re-offer and Acquire (including the stage-1 epoch rebind), the
  native-control release (`AiDefaultControl`, lease and AI override paths) and
  anything they call that takes a reference to the client's actor across the
  `SwitchZoneNOW` background load.
- **Audit, with evidence:** every SudekiMP path that links a TPtr/GEL reference
  to a party actor (direct TPtr link 0x401750 users, natives called with a
  stack-held TPtr, the dirty step-away and native-control code in
  `control_separation.c`, `lan_story_control.c`, `lan_story_input.c`,
  `party_ownership.c`), plus writers on other threads (SudekiMP workers; retail
  loader workers 0x5bb100 / 0x5bb1e0, which retail suspends around 0x43e840 via
  0x5bd140 / 0x5bd190 from 0x556160) and any SudekiMP hook reached inside GetPC
  (0x504025 vtable[3], 0x401e35 GetName). Record how the native-control state
  difference could fit that window.
- **Diagnostic, off by default:** a new opt-in key read with
  `read_config_boolean`, e.g. `[StoryAreas] TPtrAudit=true` (default false;
  `[StoryAreas] ResearchDiagnostics` alone is not enough, it defaults on). In
  `entity_from_gel_pointer`, before cleanup and destroy, walk the entity's
  reference list (bounded, back-links checked) and log once if any node other
  than this frame's own `tracked_entity` lies in the current thread's stack
  (NT_TIB StackLimit..StackBase), or if the list is inconsistent: node
  addresses, 32 bytes around each, thread id. Log only; a clean walk proves
  nothing; do not claim coverage of other threads' stacks.
- Tests: a pure walker test with synthetic {obj, prev, next} nodes (consistent
  list, own `tracked_entity` ignored, foreign stack node flagged, broken
  back-link, cycle, walk bound) and an exact-image hash check of 0x401b30,
  0x401b50, 0x4015e0, 0x401750 and 0x5bf4e0.
- Do not add a register-saving bridge (it would only move where the stray store
  lands) and do not replace the name lookup in identity proofs.

## Where this is heading (stage 2, issue #96; do not build here)

Owner design, so stage 1.1 stays compatible with it: away means spectating
(own AI by default, F1/F2 cycles any party member); when the watched person
goes into a shop or house the away player's view follows them in with the
same loading screen, through that place's own camera, while the body waits
hidden at the door; pressing M puts the player **next to whoever they are
watching** (inside a shop: just inside the door; inside a story house: inside
with the lead; outside: beside them; own AI: where it stands). Stage 2 needs a
client to present the watched person's area even when its own body is
elsewhere, a return that moves the body to the watched person (the shop entry
inside; the #73 placement warp outside), and story-house support on clients.
Keep 1.1's parking, return entry and banner easy to reuse for that.

Also for the stage 2 pass (owner, 2026-10-19): consider binding each player's
story-cast Spirit kit (private instance, lights, named camera) only while that
player is actually casting, instead of for the whole session, so the always-on
identity checks and the shared camera hook cannot interfere between strikes.
Only worth doing if cast faults still occur after 1.1's door-load fix and the
decision-4 narrowing.

## Out of scope

Stage 2 spectating (issue #96: F1/F2 cycle; watching the lead inside a house
through that house's camera; returning next to the watched person), host step-away
(G), Pause Party, F1 client cluster following, AI skill/Spirit use by the away
body, the personal-quests question (#95) and #93's story-house rule beyond
decision 2.

## Acceptance

1. Build all targets; the Wine suite passes except the known non-pass set; one
   mutation per new guard is caught.
2. Live gates (owner), in this order:
   - **Kamo, stay away:** Tal away, Ailish enters the shop. Host log:
     `lan_story_split event=begin ... away=4`, `exterior_entry keep_live=1`,
     `split_begin policy=exterior_epoch_retained`, no `zone_begin method=5`
     epoch bump, no `story_fault`. Ailish sees and moves inside the shop; Tal's
     screen keeps the street with the "press M to join" banner and his body
     hidden at the door. Ailish leaves: Tal's AI is placed on her outside and
     follows.
   - **Kamo, join:** same, then Tal presses M: `story_remote_interior
     event=away_return_entry`, loading screen, he appears just inside the door
     with the camera on him and can play.
   - **Mid-split step-away:** Tal steps away while outside during an H1b split
     (parked), and while inside Kamo's with Ailish (stays inside under AI,
     ignores the exit trigger, leaves with her).
   - **Vanilla path with the cast bound:** with Tal disconnected or a swap
     pending and Ailish entering a house: no `story_fault`, the camera follows
     her, and the host controller filters (+0x80/+0x84) return to 1,1.
   - **Story house** (e.g. the farmhouse): Tal away waits at the door and comes
     out with Ailish; Return while she is inside puts Tal outside at the door;
     observe whether the parked body can be hit (not decided (b)).
   - **Ailish goes down while Tal is away:** her controls and camera never move
     to Tal; the fight behaves as before step-away (lead held in slot 0; record
     whether enemies idle). With every other player away, record whether the
     AI teammates finish the fight on their own so Ailish rises afterwards (if
     the fight stalls, note it for the enemy-targeting work).
   - **Away while the host's offer is in PREPARE** right after a door or load:
     no freeze.
   - **Rejoin during the castle-road storm:** client log `replayed kind=4` and
     `kind=3` with the host serials, `sky_dome_replayed
     name=SFXSD001_SKY_DOME_STORM.HOM ok=1`, fog_state ff070511/42700000;
     the dark sky and fog show; a rejoin after the post-battle restore shows the
     default sky with no stale storm; replays stop once the client is in
     gameplay.
   - **Castle tunnel in control, with the diagnostic** (after the audit): Tal,
     in control, walks with Ailish through the tunnel into Illumina Castle and
     back, twice; no crash; Tal has control on arrival each time; the
     diagnostic logs nothing (or its lines identify the culprit).
3. A §9 entry with the evidence level reached per item (including the A8
   corrections); `graphify update .` after code changes; commit only when the
   owner says, as `heywander` with the noreply address.
