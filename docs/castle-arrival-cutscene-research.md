# Castle arrival: evidence and bounded design, 2026-10-20

**INFERENCE — scope.** This is the research required by
[the arrival brief](castle-arrival-cutscene-task.md), against dirty `27f7296`
with Q2 artifact `67c8271d…`. The observed run used **20547266…**, not Q2.
No implementation or test below is a new live acceptance.

## R0 — owner crossing and the cost gate

**CONFIRMED_LIVE — owner observation, limited scope.** The owner reports an
Ailish-host / Tal-client crossing through the tunnel back into the castle,
then a frozen client during arrival and host control of Tal afterwards. The
private paired runtime logs were read in place before any reload. Sanitized
figures below refer to the first-read prefixes (host 30,052 lines; client
26,563 lines). They continued receiving idle samples while being inspected.
Thus the brief's earlier whole-run counts and these counts are different
snapshots of the same run, not contradictory captures.

**CONFIRMED_LIVE — logged samples, not every native tick.** The tunnel window
(approximately 280912000–280960000 ms) has 48 host `host_frame` samples,
all `captured=1, world_captured=1`. The castle pre-movie window
(approximately 280960000–280968200 ms) has eight, all captured, with 100
world entities. Neither prefix contains `world_omitted` or a capture-refusal
record. Tal requested away at approximately 280885294–280885306 ms and did
not request return before arrival. This is an **away** crossing, not a
client-in-control crossing.

**CONFIRMED_LIVE — cost samples.** Weighted means use each logged timing
window's sample count, not the unweighted mean of window means. Values are
microseconds; millisecond boundaries come from adjacent diagnostic anchors.

| Window | Client apply mean (range of window means) | Client world mean | Host capture mean |
| --- | --- | --- | --- |
| Tunnel | 14505 (8402–17633), 1228 samples | 5920 (2571–7806), 1228 | 3484 (2626–4005), 2444 |
| Castle before movie | 17368 (16577–17971), 194 samples / nine windows | 7568 (7274–7938), 194 | 3833 (3671–3964), 347 samples / eight windows |

**INFERENCE — acceptance consequence.** All nine castle apply window means
exceed 16 ms. The prior testing.md cost gate therefore did **not** pass;
its already approved 96-actor fallback is applicable. This arrival work does
not silently ship that separate protocol/profile capacity migration. This
record is not a performance measurement of the 67c8271d Q2 code or of the
arrival candidate. Client-in-control crossing, omitted doors, rebind snaps,
and the Q2 hold's visible behavior remain **UNKNOWN**.

**CONFIRMED_LIVE / CONFIRMED_STATIC — zero captures explained.** First-read
host totals are 222 world captures and 452 zero captures (the brief had
222 and 292 before more idle samples). Every zero also has `captured=0`:
two with phase UNKNOWN, 449 with phase LOADING and no available members,
one during the retained split transition with stale available mask 12.
`lan_story_runtime.c:service` invokes WorldCapture only after successful
CaptureMovement/shot capture. Therefore world capture was **not attempted**
in those logged zero samples. There is no logged case of successful party
capture followed by failed world capture here. `lan_story_world.c:finish`'s
64-line refusal cap cannot establish the absence of all unlogged failures,
but does not explain these logged zero samples.

## R1 — arrival timeline

**CONFIRMED_LIVE — clock qualification.** The ordinary story lines have no
millisecond timestamp. The ranges below are enclosing `ms`/`tick` anchors
on the same machine's GetTickCount clock. They are not invented exact event
timestamps. In particular the movie entry and the subsequent TEMP request
share a long bracket because the host controller diagnostic is silent during
the native movie. Their ordering, not their exact time within that bracket,
is observed. The exact quest-completion timestamp is **UNKNOWN**.

| Seat | Clock / enclosing bracket (ms) | Sanitized observation |
| --- | --- | --- |
| Host | ~280743487; ~280745000 | Initial ready scene epoch 5/revision 4, then epoch 6/revision 5; mask 12, Ailish lead 3. World identity remains `newbrightwater` while streaming zones change. |
| Client | ~280754585 | Local selection and control handshake reached Tal, local native lead 2. |
| Both | ~280758351–280758406 | Tal away request 1; his control lease drains. |
| Both | ~280818116–280818321 | Tal returns (request 2), remote control transaction 24 becomes ready. |
| Both | ~280885294–280885306 | Tal away request 3; transaction 24 drains, host controlling mask 3 becomes 1. |
| Host / client | ~280912094 / ~280912133 | Current streaming zone enters `ws_country_nw_to_castle_s`; no world replacement. |
| Host / client | ~280923729 / ~280923897 | Castle background load begins. |
| Host / client | ~280960055 / ~280960080 | Current zone becomes `illuminacastle`; captured world continues. |
| Host | 280968133–281127841 | FMA04A passthrough; after it, TEMP entry requests `lica_tals_room` with marker `SP_TALTOWER`. Split begins using stale Ailish lead/inside bit 8 and available mask 12. |
| Client | 280968134–280968409 | Host cinematic view activates; last presented stream reports 9267 received frames. |
| Client | 280968409–280968682 | State 6: no matching fresh party/world pair. |
| Client | 280968939 onward | State 0: not ready / stale scene during movie gap. Received count remains 9267. |
| Host | 281127841–281128088 | Native party now has one member; room transition first has unavailable descriptor, then exterior kind 3, while the split expects TEMP kind 4. |
| Host | 281128122–281128163 | Exact Tal-only roster triggers split abandonment: epoch 7/revision 7, READY, mask 4, lead 2, but **temporary name cleared**. Next sample sees kind 4 and immediately becomes epoch 8/revision 8, LOADING, mask 0/lead 4; it stays there. |
| Client | 281127848–281128098 | Brief state 6 after movie returns. |
| Client | 281128098–281128356 onward | State 5, native_presentation_changed, permanently latched; received count remains 9267. |

**CONFIRMED_STATIC — authored placement.** The installed SOLWORLDM GEX
function `ICExt_TalAilishHome__OnEntry|PP` (hash `0x0cc60d5c`) tests story
block 2000, disables gamepad, advances to 2010, completes the escort quest,
waits 1.5 seconds, starts TSA, requests PC 2 as lead, plays FMA04A, removes
PC 3 if present, calls TSAEnterInterior for `LICa_Tals_Room` with authored
resource `0x430`, sets exit resource `0x431`, waypoint `0x432`, and static
camera resource `0x2b0`, then ends TSA and fades in. The log independently
identifies the entry marker as `SP_TALTOWER`. This is a **temporary interior
in the existing world**, not an independent world load. Its return coordinates
are native context, not a camera-follow target. Script control flow is static
evidence; its exact instruction execution times in this run are **UNKNOWN**.

## R2 — why the client stopped

**CONFIRMED_EXACT_IMAGE / CONFIRMED_LIVE.** Supported EXE SHA-256 is
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Read-only Ghidra decompilation of `Dont_Use_PlayMovie` (VA `0x504d90`)
shows a synchronous movie/input/render loop. The paired logs show the long
controller-service gap and no fresh accepted party/world stream on the
client. This is a native movie, not a camera track encoded in party frames.
Replicating its video is not implemented by the cast-view path.

**CONFIRMED_STATIC / INFERENCE.** In pre-arrival ObserverSample, `replaced` is true when
the exact actor set changes. The split abandonment branch unconditionally
clears `observed.temporary`. In this case the native current descriptor is
still TEMP kind 4. The following exactness check expects kind 3 when the
name is empty, so the observer remains non-READY. The logged mask/epoch/
kind sequence matches this source path. It is the host's persistent gate
failure after the movie; no capacity or pose omission caused it.

**CONFIRMED_STATIC — pre-arrival baseline.** Runtime `present` separately turns a new-epoch scene
that is temporarily not READY, in another area, or has another party mask
into the permanent `presentation_changed` latch. It does so before waiting
for local area follow. That turns an ordinary intermediate observation into
an irreversible refusal. Connection identity and actual actor-generation
changes have separate hard refusals and must keep them.

**CONFIRMED_STATIC.** `apply_presented_frame` borrows the host frame view
when letterbox is present, a local character is selected, the view is valid,
and the local shop is closed. It restores the local cast view before that
borrow and calls ReplicaRestoreView when leaving it. `lan_story_cast.c`
and per-character `cast_view` records cover admitted skill/camera scopes,
with control-fence/epoch/revision/actor-generation ownership. None creates
world frames during a native movie or reconciles a removed party member.
Clearing these leases or ignoring frame freshness would not prove recovery.

**CONFIRMED_STATIC — baseline gap; end-to-end recovery UNKNOWN.** Correcting the observer and
transient presentation gate alone does not reconcile the client's paused
mask 12 with host mask 4, retire/rebind the removed actor, or grant a lease
on the native lead. The existing closed Lighthouse recruitment journal is
addition-only (Tal to Tal+Ailish); it is not a general removal/re-add journal.
An ungated recovery must preserve the client pause, registry and generation
proofs rather than accepting unrelated native replacements.

## R3 — native lead and the label

**CONFIRMED_EXACT_IMAGE.** Read-only decompilation on the hash above confirms
`CGroupPlayers::SetAsLeadPlayer` VA `0x424e40`, pending lead at group+0xc0,
and group-update retry at `0x4231e4` through Next `0x424060`. Shared
reassignment `0x4237b0` updates the global controller target at
`*(image+0x408da4)+0x248`, component modes and camera binding through
`0x4ef700` / `0x42a370`. Group slot 0 is group+0x90 at
`*(image+0x408d94)`. RemovePC export `0x4b23a0` invokes RemovePlayer
`0x423390`, whose helper `0x4233e0` rotates a removed lead through Previous
at **0x423433**. The older G note's `0x424433` is not that callsite.
RemovePC also touches the persistence/script machinery: it is not safe to
replay it as a presentation-only membership edit without that contract.

**CONFIRMED_STATIC / INFERENCE.** The existing death-hold adapter covers
pending-lead Next and down/death Previous, but not removal Previous. It arms
for connected, reserved remote characters, including away seats. Thus the
script's pending Tal request can be held while Ailish remains present, but
Ailish's removal can still switch the native controller to Tal. The exact
call that switched it in this run is inferred from these paths and the
observed final roster; there is no live call trace of 0x423433.

**CONFIRMED_LIVE.** Tal's last ready remote transaction was 24, drained when
he went away. At the exact Tal-only sample, AI override is 1, mode byte 0,
and the controller target is Tal. That single sample is during script/TSA
settling; it is not proof of a surviving client lease or of the settled AI
reference count. Later settled override count is **UNKNOWN**.

**CONFIRMED_STATIC — pre-arrival baseline.** Runtime does not offer a lease to an away seat or to
`roster.leader_character`. HostControlBound requires the locked host
character to be native lead. CompanionSeatExact/AwaySeatExact also exclude
the native lead. The old remote-control path cannot acquire Tal as lead.
The native target instead drives host input once native script filters
release. The owner's observation agrees, but exact release timing is
**UNKNOWN**.

**CONFIRMED_STATIC.** `lan_story_name_tags.c:SudekiMpLanStoryNameTagsRender`
formats the lobby player name plus `" (AI)"` when
RuntimeCharacterAway(character) is true. RuntimePlayerName supplies the
name. It does **not** inspect the native controller target, AI byte or a
ready control lease. Therefore the observed Guest/AI text is consistent
with Tal's still-away session flag even though native host input drives him.
An active client should lose that suffix by the actual away/control state,
not by hard-coding a special Tal label.

**CONFIRMED_EXACT_IMAGE / CONFIRMED_STATIC — input ordering found during
self-review.** FilterNone VA 0x408ac0 writes pending controller+0x84 and calls
0x4290d0 to clear edges; it does not change current controller+0x80.
Controller Update 0x427cf0 dispatches input before the tail at 0x4285c9
publishes current mode. Input callback 0x4277b0 gates actions on the current
mode. Consequently a pre-original request alone is insufficient. The arrival
guard now settles only its own ordinary-to-None request ahead of the consumer,
using the exact tail's current-mode write and camera-input notification at
0x438c50 (AL=false, manager on stack, callee pops four bytes). The thunk pins
`regparm(0)` and preserves the native stack contract. It proves the same
controller owner, pending None, tail/call bytes, notification entry/return,
manager and camera slots before writing. Unknown state refuses the original
call and retains the owner. Native All still settles through the original
tail on release; foreign filters are never adopted or committed.

**CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST — input proof limits.** HostFilterTest
checks all 36 signature bytes against the supported image and rejects each
single-byte mutation. Its asynchronous fixture checks current/pending mode,
camera notification, identity refusal and retries. The input consumer fixture
checks callback ordering and suppression. These are separate native spies;
no full native controller body was executed and no live input claim follows.

## R4 — existing contracts

**CONFIRMED_STATIC.** Plan G remains a broader, unimplemented separation of
native lead and player ownership. The built swap phases reserve characters
and drain/acknowledge a transfer; WAIT_ACK or RESERVED/HANDOFF never proves
ready input. Tier 0 enemy targeting still treats native lead specially;
step-away returns ordinary non-lead characters to native companion AI.
Neither establishes a remotely controlled native lead or an AI-controlled
native lead. Existing seat parity decisions remain accepted.

**CONFIRMED_STATIC / UNKNOWN.** F2/#96 describe spectator camera ownership;
existing client spectators without an available character consume the host
view. There is no corresponding host-without-a-character camera/input
owner. Native `0x42a370` follows a newly selected lead, but this arrival
script subsequently installs an authored static camera. That later binding's
lifetime and a safe follow-camera override/restoration need their own exact
proof; assuming the earlier lead switch is sufficient is unproven.

## R5 — design and owner boundary

**INFERENCE — independent corrections.** First preserve the already observed
TEMP name when an exact same-world/group/controller replacement abandons a
split, but only after proving the native descriptor belongs to the world
zone table, has state 4, has no pending transition and names that same TEMP.
Drop exterior split ownership; do not preserve the old party mask or actors.
Second treat new-epoch non-READY/area/mask mismatch as a wait, retaining all
connection and actor-generation hard refusals. Tests must call the actual
observer and runtime gate, including a later valid sample and foreign cases.
These fixes are ungated; neither writes native state.

**CONFIRMED_STATIC / CONFIRMED_TEST — implemented control design.** Keep the
host as simulation/story authority and preserve native scripted lead changes.
`[StoryAreas] ClientLeadSpectate` defaults off. For the closed castle pair /
Tal-room singleton, HostControlLeadPolicy grants a bounded native-lead scope.
Tal receives the existing PREPARE/ACK/READY transaction. Connected away owners
are returned to active intent on both seats; no fabricated ACK or HUMAN state
is used. The shared control-separation adapter calls the host input guard
immediately before its original controller consumer. It retains the same
controller/world/group/Tal/AI identities, borrows ordinary input through the
existing FilterNone entry, and refuses unknown owned identity. Its callback
cannot be replaced in flight; separation teardown refuses while registered.

**CONFIRMED_STATIC / CONFIRMED_TEST — native leases.** Existing AIOverride /
AIDefault remain the only acquisition/release calls. Exact native lead mode 0
is distinguished from companion mode 1. Old leases may cross the proved role
change only while draining; new leases still require fresh epoch, capture,
connection and transaction fences. SetLead-before-RemovePC and
AddPC-before-reverse-SetLead intermediate pairs are covered. Disconnection
holds the host block until retained native work drains, then releases the
owned filter for temporary host play. Rejoin starts a new transaction and
revokes temporary local input. Reservations remain intact.

**CONFIRMED_STATIC / CONFIRMED_TEST — presentation design.** The client keeps
its paused native Tal+Ailish party. Only the Replica/World presentation calls
receive a stack projection containing Tal; its callback reconstructs the
expected projection and re-proves the full native roster every time. Ailish
is hidden by the existing foreign-character AreaFade path. No RemovePC,
persistence change, native party edit or containment release occurs. A closed
scene-edge journal expects adjacent epochs and one generation increment per
available actor, retaining absent Ailish's last generation for the reverse.
WorldPrepare receives a one-edge metadata grant, still requires the fresh
matching catalog and consumes the grant at bound-set publication. Reverse
presentation uses the full roster. Rejoin uses a current Tal-only seed and
waits for the existing local area follower; it does not reuse a stale pair
snapshot. This narrows control-fence admission without changing wire layout,
packet version or save format.

**CONFIRMED_EXACT_IMAGE / CONFIRMED_STATIC — host camera.** The authored
SetCameraStatic path (VA 0x438500 / 0x4e7d80) chooses the native Aimed state.
The adapter reuses the already documented CanSetState (0x437c30) /
SetCameraState (0x437530) entry/ABI contract. It proves the selected default
camera, renderer, Aimed singleton with no state data, and a live
GameObjectTarget whose actor is retained Tal, then requests native
Exploration. It never replaces the camera target, borrows a reference,
patches a second seam or owns a render override. Native lead reassignment
remains responsible for the reverse camera binding. HostFilterTest proves
request arguments, target refusal and idempotence with inert native spies;
it is **CONFIRMED_TEST**, not proof of an animated follow camera. Later
script reassertion, actual camera movement and the save's reverse route are
**UNKNOWN** until the owner gate.

**INFERENCE — alternatives and risk.** Replaying RemovePC on the client could
mutate persistence and launch native work under a retained pause. General
mask relaxation would discard membership proof. An AI-controlled native lead
would need a larger ownership design. The selected projection avoids those
native operations but depends on exact unchanged client actors, matching area
follow and the closed adjacent scene sequence. An unobserved intermediate
epoch or a replaced/re-added native identity safely refuses; recovery in such
a run is **UNKNOWN**, not guaranteed by helper tests.

**Owner decisions, 2026-10-20 (accepted).** At the scripted Tal-only
transition, a connected player who reserved Tal returns from away to player
control. There is no AI-controlled native-lead extension in this pass. If
that player disconnects, the host may control Tal temporarily; the owner also
permits assignment to another connected player. The implementation uses the
temporary-host option, preserves reservations, and yields to the freshly
acknowledged remote lease on rejoin. A new assignment UI is outside this
bounded transition. These decisions resolve the former R5 boundary; they do
not authorize taking Tal from an active connected client.

## R6 — first arrival-candidate owner run and correction (2026-10-20)

**CONFIRMED_LIVE — reported behavior on aa5deba4.** Both staged DLLs matched
`aa5deba44f442537153197bc32ff51225f62daec478ec42289eae53c22950f5f` and
both INIs contained `ClientLeadSpectate=true`. The owner reports a frozen
client at quest completion/movie entry, recovery after the host skips, Tal's
room appearing with the wrong camera, host control of Tal, and an error when
returning from away using either M or the menu. The error says “Could not step
away - try again” although the requested action is return. No crash was
reported. This is a failed ownership/camera gate, not acceptance of F2.

**CONFIRMED_LIVE — bounded log prefixes.** The first preserved host/client
prefixes contain 4,672,790 and 4,541,501 bytes. The last pre-movie clock anchor
is approximately 284585101 ms; the next host controller anchor is
284593697 ms. Client receipt count stays at 6460 through that gap. Afterward
the host reaches epoch 7/revision 7, READY, mask 4, lead 2 and the retained
Tal-room TEMP; it resumes party/world capture. The client adopts that room
and presents again. There is no permanent native_presentation_changed latch
in these prefixes. Five return requests are logged as result 5 (UNAVAILABLE),
with ownership still at revision 9, available mask 12 and only the host
controlling. Exact movie entry/skip timestamps remain UNKNOWN.

**CONFIRMED_STATIC / CONFIRMED_TEST — loader fault.** The new switch was read
only in the FourPlayerTest story-observe branch; the TitleMenu saved-story
branch never called its setter. Its default false value therefore survived
both INIs. This explains the absent forced return, stale ownership availability
and ordinary rejection of return to the native lead. The former Part S F4
claim of saved-story wiring was incorrect. The new StoryLoaderProfile fixture
compiles that complete title-profile branch plus the production boolean
readers against native-install spies, uses real temporary INIs, and fails on
the old source. It covers missing/false/true/on/invalid values, entry/load-only
exclusion and setter ordering before lobby installation. It does not execute
native hooks or all of SudekiMP_Initialize.

**CONFIRMED_STATIC / CONFIRMED_TEST — message fault.** service_step_away used
step-away wording for every refused ACK. It now preserves whether the intent
was RETURNING before resetting it to authoritative away state. Both menu and
raw-M return refusals are tested through the real mailbox/service; ordinary
away rejection retains its existing wording. The old code fails this test.

**Accepted owner camera correction.** Both windows use the authored room
camera; the connected reserved Tal client alone controls him. This supersedes
R5's Exploration request. The host adapter now leaves the native camera alone.
The client borrows the fresh matching host view in the exact closed Tal-room
scene, through the existing Replica view lease. It yields local camera updates
without setting the cinematic gameplay-input hold. Leaving the room restores
that lease; a failed restore retains ownership for retry. No camera pointer,
party membership or save data is rewritten by this correction. Native room
framing and actual exclusive control on the corrected DLL remain UNKNOWN
until replayed. Selection, restore/retry and the host service are exercised by
fixtures; the complete native client camera/input frame is not.

## R7 — remaining movie presentation boundary (owner approved)

**CONFIRMED_EXACT_IMAGE / CONFIRMED_STATIC.** Re-reading the supported-image
Dont_Use_PlayMovie decompilation at 0x504d90 confirms a synchronous movie,
input and rendering loop. Its host controller publication seam is not serviced
during playback. The `4c7ed0f6` startup-movie adapter at the same entry calls
the original for FMA04A; that artifact has no movie-start/end transport or client
movie player. The observed stall is therefore not repaired by the loader or room
camera correction. Reusing stale world frames would not display the movie.

**Accepted owner decision (2026-10-20): synchronized movie, host controls skip.**
The owner explicitly approved the following extension.

**INFERENCE — approved design.** Add host-authored
presentation events for the whitelisted FMA04A asset: start, sequence and end /
skip, fenced by the existing connection and scene identity. Queue only plain
data through transport; send start before the host enters the native movie
and end after it returns. The client plays its own installed copy on a proved
game/render seam while preserving containment, never replaying quest scripts.
The host alone decides skip/end. A client loop must observe the queued end
without transferring native work to the socket thread. Rejoin, host loss,
resource failure and late start need bounded recovery to the ordinary fresh
frame path.

The native-owner extension is approved; it does not change campaign simulation,
character reservations or save authority. R8 records the researched implementation
and its remaining unknowns. The earlier `4c7ed0f6` artifact contains the handoff /
room-camera correction only and remains separate.

**Accepted direction, later work.** Host-only kicking/body release and spectator
claims were proposed by the owner. The current disconnect-only temporary-host
fallback remains. Permanent versus temporary removal, reservation transfer and
rejoin restrictions need their own design; this pass does not add an assignment
UI or take a body from an active connected owner.


## R8 — synchronized FMA04A implementation and native contracts

**CONFIRMED_EXACT_IMAGE / CONFIRMED_STATIC.** Supported image only (identity
in executable.md). Native MoviePlay at VA `0x504d90` is cdecl, two stack
arguments (name and bool), bool result in AL. It calls `0x504c10` before opening
the installed basename via `0x5bf2d0`. The begin routine suspends the two
background worker handles at `0x724004/8`; `0x5bb2c0` creates those workers.
It suspends native sound activity and drains pending sound work. MoviePlay
pumps messages, updates the movie at callsite `0x504e1a` → `0x5bf1d0`, then
updates input. Its normal skip branch is separate from that update call.

**CONFIRMED_EXACT_IMAGE.** `0x5bf450` synchronously closes the Bink handle,
clears the native movie slot `0x7c30ec`, releases its surface/buffer resources
and restores render state. Native EOF, device-loss handling, the exported
StopMovie and final MoviePlay cleanup use that same routine. `0x504cf0` calls
it, drains until the slot is null, resumes the two workers and restores sound
pause depth. This is a verified native close contract; the mod never writes
the Bink handle or task-private completion fields. The existing window policy
keeps the native activity flag at owner+0x11 active while unfocused; that is the
flag MoviePlay's final message-pump/recovery loop reads. Device-loss recovery
still belongs to native code and can wait for a usable device.

**CONFIRMED_STATIC.** `lan_arena_startup_movie_skip` remains the sole MoviePlay
entry owner. A saved-story observer shares its trampoline; attaching without
the startup-skip option does not enable logo skipping. The movie adapter owns
only the inner relative callsite, with supported-image identity, byte, operand
and target checks. Native cdecl bridges explicitly pin `regparm(0)`. Calls on
the native stack prevent teardown; the runtime stopping branch retries native
restoration on its bound game thread before dependent observers retire; failed restoration retains dependencies
and permits retry. The runtime binds the game thread at its verified controller
post-update seam. Before client playback it proves the paused roster, restores
borrowed cast/replica views, closes input and invalidates stale presentation.
It refuses to enter from either suspended worker or while another native movie
owns the slot. Native worker/render/audio integration is not exercised by spies.

**CONFIRMED_STATIC.** The optional SMP4 movie channel leaves existing envelope
and saved-story profile versions unchanged. A client advertises capability on
its authenticated connection; that echo also confirms the issued token. Only
capable, confirmed connections present at host start are recipients. No late
join or replacement generation inherits an in-progress movie. The host queues
start before calling the native trampoline and end after it returns. A worker
repeats the latest plain event every 100 ms, including end for 2000 ms;
capability advertisement is limited to once per second so it cannot starve
ordinary keepalives if an older host ignores it; it never
reads native objects. Events contain only version, whitelist ID, active/end,
serial, scene epoch/revision and elapsed milliseconds. No asset bytes, paths,
quest calls or actor ownership cross this channel.

The scene anchor is the latest valid host-published scene: a native script may
already have stopped controller publication before entering MoviePlay, so
normal frame freshness is deliberately not required. The client requires that
anchor when admitting a new start. End for its exact active movie survives a
subsequent scene change. Envelope sequence, immutable movie identity, elapsed
monotonicity and terminal state prevent old starts from undoing end. Receipt
freshness is independent of world-frame freshness. Sampling before the queue
lock cannot underflow a new movie's elapsed time.

**CONFIRMED_STATIC.** The client invokes only installed `FMA04A`, unskippable
locally, on the bound game thread. Its scoped movie-loop callback polls the
plain host event. Host end, connection loss, shutdown or the ten-minute safety
limit invokes native close only for that playback's observed handle. Normal
native cleanup remains in charge. A start older than 1500 ms is not replayed
from the beginning; one serial plays at most once per connection. Missing
assets / immediate native return are logged and not retried. The regular
runtime remains input-closed while a fresh active movie event exists; a lost
movie channel expires after 1000 ms and ordinary scene/frame freshness still
governs recovery. The timeout bounds polling policy, not native device recovery.

**UNKNOWN — live gate.** This synchronizes the movie identity and host end,
not exact decoded frame numbers. Start latency/skew, native asset audio/video,
EOF/device recovery, post-movie presentation and real lease handoff remain
unplayed on this candidate. A late or missing-asset client can retain its last
world presentation until host end/expiry. Other movies are native passthrough;
this is not a general cutscene replication system. Tests and isolated mutation
results are recorded in the new testing.md addendum, separately from live proof.


## R9 — first synchronized-movie replay: preparation used the wrong pause proof

**Owner observation / CONFIRMED_LIVE, limited to the replay on `498b9b00…`.**
The owner reports that the client movie did not appear to load. Host skip
returned both seats to Tal's room, where the client alone could move Tal.
This supports that run's exclusive-control outcome; it does not establish
movie playback, authored camera pixels, natural EOF, reverse transition or
whole-game reconnect. The profile was the dirty saved-story candidate with
`ClientLeadSpectate=true` on both seats.

**CONFIRMED_LIVE — bounded diagnostics and read-only state.** The preserved
host log records FMA04A start and end on epoch 6/revision 5; the client has no
movie playback-attempt record. A read-only inspection of the matching loaded
DLL's journal finds the same terminal movie identity on both seats, elapsed
4480 ms. The client's consumed serial is 1, while its native-playback identity
and movie-log count remain zero. Host return-from-away and client room-view
selection are logged after the movie. First movie packet receipt and the exact
first preparation refusal were not logged and remain UNKNOWN. Private logs,
process mappings, snapshots and identities stay outside tracked documentation.

**CONFIRMED_STATIC / CONFIRMED_TEST — deterministic blocker.** Runtime
`service` called `prepare_story_movie` from the controller post-update seam.
That helper called ClientPausedRoster → ClientService →
MenuNativePauseExact, whose `frame_active` guard requires the native menu
callback. The controller seam has no such menu-frame permission. Repeated
preparation failure can therefore consume the movie as late after 1500 ms
without ever calling MoviePlay. Restoring this old path in isolated mutant
S01 fails the corrected regression at an assertion with the real native pause
checks and a fully validated synthetic pause owner. The run's absent attempt
and consumed serial are consistent with this blocker; packet timing itself
is not reconstructed from the terminal journal.

**CONFIRMED_STATIC — bounded correction.** Playback stays on the controller
seam. `ClientRestoreViewDispatch` shares the existing fresh-dispatch/full-
containment bracket with ClientObserveDispatch. It grants only restoration of
retained views, uses ClientRosterExact inside the synchronous presentation
scope, and rechecks containment before returning. MoviePlay begins after that
scope closes. The shutdown-only cleanup proof and the native menu's
`frame_active` requirement are unchanged. Runtime keeps the dispatch witness
only for the synchronous call, clears it on return and refuses nested entry.
No worker performs native work and no render callback starts MoviePlay.

**CONFIRMED_EXACT_IMAGE / CONFIRMED_STATIC — render boundary.** Existing
supported-image evidence puts the menu callback at VA `0x68d572`, before
frame end at `0x68d58c` → `0x5dd540` / D3D EndScene. Consequently the menu
callback is not used as a movie-entry fallback. See the frame-end contracts in
engine-functions.md. No extra native patch seam is added.

The movie adapter now reports not-ready, preparation, worker, occupied-movie,
changed-hook, active-call and late-start refusals. Deferrals are once per
reason/serial/connection; all movie records retain the 64-record install cap.
The existing start deadline, scene/connection fences, host-only skip and
native stop policy remain unchanged.

**UNKNOWN — correction live gate.** The new candidate has not been deployed
or replayed. The runtime regression uses real MenuNative pause checks and
production routing/preparation, with containment/roster/view-owner spies.
The shared production client bracket is reviewed source; the test does not
execute its entire native containment graph. Native codec, renderer/audio,
room-camera appearance, EOF/device recovery and end-to-end teardown still
require the owner replay. Hash, tests and mutations are recorded in testing.md.


## R10 — owner accepts the corrected arrival and castle–Brightwater loop

**CONFIRMED_LIVE — owner report, 2026-10-20.** Both seats were reloaded onto
`7d3596822dcf355694ef60a66660e6231ed46262b7a00732904183c24b9e7ae8`, dirty
`27f7296`, with the saved-story title-lobby LAN profile and ClientLeadSpectate
enabled. The owner reports that it works and that the castle ↔ Brightwater
loop was completed. This is the first owner acceptance of the corrected
candidate after R9. Evidence ID: `owner-report-castle-brightwater-loop-20261020`.
The [testing record](testing.md#castlebrightwater-loop-accepted-checkpoint--2026-10-20)
records the exact profile and validation.

The earlier unplayed statements describe the state when those candidates were
prepared; this report supersedes them for this route on this DLL. It is not a
per-check movie/camera checklist or a timing capture. Exact start skew,
audio/video synchronization, natural EOF/device recovery, disconnect/rejoin,
teardown/retry and wider campaign behavior remain UNKNOWN. The owner requests
a commit/push/build checkpoint and bug/performance work before release.
