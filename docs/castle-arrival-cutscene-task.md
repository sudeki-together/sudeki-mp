# Task brief: castle-arrival cutscene freeze and the switch to Tal (#54) — research, then fix

**Where to work.** The `shared-simulation` worktree of `codex/shared-simulation`,
dirty on `27f7296`, current DLL `67c8271d…` (the Q2 pass, preserved in
`build/artifacts/castle-q2-67c8271d/`). Build on it; do not commit, stash,
reset or reformat; other dirty files belong to the owner. Keep every earlier
artifact separate.

**Review budget.** The reviewer (Claude) is at its weekly limit, so this task
has no outside review pass. Do the research, the fix and an adversarial
self-review (part S) yourself, and be strict about evidence labels: anything
not shown by a log line, a source read, an exact-image check or a test is
UNKNOWN. Read `AGENTS.md` and follow it. Line numbers drift; function names
are authoritative.

## What the owner saw (2026-10-20, DLL `20547266…`, live-mp70 pair, Ailish host / Tal client, castle-road start)

1. The party made it through the tunnel and back to the castle. This is the
   first live crossing on the capacity build.
2. At the quest completion for returning to the castle, a cutscene starts.
   Tal's client window freezes while Ailish's host plays the cutscene.
3. When the cutscene ends, the story has sent Tal to his room. The host ends
   up controlling Tal, and the label above Tal's head reads "Guest AI".

A few facts the reviewer read cheaply from that run's two runtime logs
(counts over the whole run, not timed to the cutscene):

- Host `lan_story event=host_frame` lines: 292 with `world_captured=0`, 222
  with `world_captured=1`, and no `event=refused operation=capture` line.
- Client `spectator` lines: 289 `state=5 reason=native_presentation_changed`,
  190 `state=0 reason=not_ready`, 243 `state=4 reason=presented`, 4
  `state=7 reason=waiting_for_world_resources`, 2 `state=6`.
- Client: 4 `event=refused operation=apply stage=clip_resource_prepared`,
  3 `present_refused step=prepare`.
- No literal "Guest AI" string exists in `src/`; the label is assembled
  somewhere (find it).

The handoff message names where the private logs are. Read them in place.
Never copy raw log lines, machine paths or `.agent-local/` content into
tracked docs; publish sanitized conclusions with evidence labels.

## Owner decision (2026-10-20)

**When the story sends Tal to his room, Tal's player controls Tal and the host
spectates Tal.** The host's local input must not drive Tal, and the host's
camera follows Tal. The label over Tal must reflect who controls him. The
client must not freeze during the host's cutscene.

Anything beyond this that changes the network authority model, the accepted
gameplay semantics or the save format: stop and ask (AGENTS.md "Stop for a
human decision"). Write the question into the research doc and implement
nothing for it.

## Owner correction after the first candidate run (2026-10-20)

This supersedes the follow-camera wording above and in F2: **both windows use
Tal's native authored room camera while he is in his room; only his connected
reserved client controls Tal.** The temporary-host fallback remains for a
real disconnect, after the existing lease drains. A connected away owner is
returned to active control at the scripted singleton transition.

The owner also proposed later host-only kick/body-release controls: a released
body may be claimed by a connected spectator; the host can move a player back
to spectator temporarily or permanently. Record this direction separately.
Reservation transfer, rejoin restrictions, permanence and UI remain a later
work item; do not silently implement them as part of the arrival correction.

## Approved movie extension (2026-10-20)

The owner approved R7: **synchronized playback of the installed movie on each
client, with the host alone controlling skip/end.** Implement the closed
FMA04A channel using the existing MoviePlay adapter, verified game-thread
playback and native stop/cleanup. Keep transport plain-data-only; fence by
connection and scene, reject replay/late starts, handle host loss and retain
native dependencies through teardown. This approval settles the previously
open movie-owner extension. Other authority or gameplay decisions still stop.
The existing handoff/room-camera-only `4c7ed0f6` artifact stays separate.

## Part R — research (write `docs/castle-arrival-cutscene-research.md`)

Sanitized, with `docs/evidence-index.md` labels on every claim.

- **R0. Part A live record.** From the run's logs, record what the tunnel
  crossing shows:
  - whether `world_captured=1` held through the crossing and inside the
    castle;
  - whether `world_omitted` appeared;
  - the castle numbers for `apply_mean_us`, `world_mean_us` and
    `capture_mean_us`, against the 16 ms / 96-actor fallback in testing.md;
  - which control mode Tal was in.

  Explain the 292 `world_captured=0` host frames: capture not attempted, a
  scene not READY, or refusals hidden by `finish()`'s 64-line lifetime cap.
  Write it as an owner-observation plus log record. Do not upgrade it beyond
  what the logs show.
- **R1. Timeline**, with millisecond stamps on both seats, of the castle
  arrival: the quest-completion trigger, cutscene start and end, area and zone
  changes, scene epoch and revision, `available_mask` and `leader_character`
  changes, and where and how Tal is placed in his room (zone, temporary area
  or interior).
- **R2. The client freeze.** Find which gate held the client during the
  cutscene (`native_presentation_changed`, `not_ready`, or something else) and
  whether the host kept sending party and world frames. Show what the existing
  cinematic and cast-view replication does: `cast_view`, `host_cinematic_view`,
  `SudekiMpLanStoryReplicaRestoreView` and `lan_story_cast.c`. Explain why it
  did not keep the client presenting.
- **R3. Control at the end.**
  - Statically, on the supported image only, find how the native story
    switches the playable character to Tal: the controlled-character or leader
    global and the script or call that sets it. Reuse
    `docs/engine-functions.md` and `docs/structures.md`.
  - Find the mod's control state for Tal before and after the switch: the
    client's control lease, away state, `host_control`, the companion seat,
    and the AI override at `*(actor+0x94)+0x16A`.
  - Explain why the host's input ended up driving Tal, and what the "Guest AI"
    label reads to decide its text.
- **R4. What already exists** for "the native lead is a client's character":
  - the plan's G row (lead vs ownership);
  - the swap phases (`WAIT_ACK`, `RESERVED`/`HANDOFF`);
  - the tier 0 lead rule;
  - step-away;
  - the F2/#96 spectator notes.

  Say what is built and what is missing for a host with no controlled
  character who must spectate.
- **R5. Design.** Give the smallest design that meets the owner decision,
  the alternatives, and their risks. Cover the reverse transition: when the
  story returns Ailish as lead, control must go back to Ailish on the host and
  Tal on the client. If the current save cannot reach that transition, record
  it as UNKNOWN and keep the code symmetric.

## Part F — fix (after R; implement R5's design)

1. **No client freeze through the host cutscene.** The client keeps
   presenting the host's party and world frames. If the existing cast-view
   path can replicate the cinematic camera, use it. Otherwise the client
   keeps its own camera on the replicated world. At the latest, presentation
   resumes when the cutscene ends.
2. **Tal-only lead.** When the native switch to Tal is observed on the host:
   - Tal's client seat holds his control through the existing remote-control
     lease path.
   - The host's local input is kept off Tal through the existing
     input-separation seam, not a new hook unless exact-image verified.
   - The host camera follows Tal.
   - The overhead label shows Tal as player-controlled.
3. **Reverse transition** back to Ailish host / Tal client, symmetric with 2.
4. **Default off.** Gate parts 2 and 3 behind `[StoryAreas] ClientLeadSpectate`,
   read like the other `[StoryAreas]` booleans. The owner enables it on the
   stage. Part 1, the freeze fix, is not gated if R2 shows it only corrects a
   gate.

Constraints:

- One adapter per native seam.
- Verify bytes, targets and ABI before any install. Install transactionally.
- Transport workers never touch native objects.
- No native write outside an existing documented seam unless R3 proves the
  seam on the exact image.
- Teardown and rejoin must leave no stale lease, input block or camera
  override.

## Tests

Use fixtures, in the style of the existing runtime and split-control tests.
Each change names the mutation that must fail it:

- leader switch to the client's character: the lease goes to the client
  seat, host input is blocked, and the spectate target is Tal;
- the reverse switch;
- the cutscene presentation gate keeping the client presenting;
- the label state;
- teardown and disconnect mid-switch.

Run each mutant in isolation; it must fail at an assertion.

## Part S — self-review (in place of an outside review)

When the fix passes, re-read this brief and
`docs/castle-q2-pose-omit-task.md`. Write a table in testing.md with one row
per requirement of both briefs. Each row gives the code location, the test
and mutant that prove it, and the evidence label. Every requirement without a
discriminating test is marked as such. List the residual risks and the
UNKNOWNs. For the Q2 pass, check at least that its 12 test groups catch the
mutants that brief names. A group that does not must be fixed or relabelled.

## Docs and acceptance

- **Docs.** Add the research doc, a testing.md section pinned to the new DLL
  hash, a §9 entry recording the owner decision and the R0 live record, and a
  status-matrix row for the castle-arrival transition (RESEARCH-ONLY, no live
  upgrade).
- **Build and tests.** Build with `tools/build-linux.sh`; `git diff --check`
  must be clean. The focused set from the Q2 pass passes, plus the new
  fixtures, the native C11 tests and the full suite with the known non-pass
  set unchanged. Run `graphify update .`.
- **Artifact and report.** Preserve the new DLL under `build/artifacts/` with
  its `.sha256` and diff. Report file by file, ending with the owner's live
  gate:
  1. Tunnel and castle with Tal away, then in control.
  2. Quest completion: the client keeps presenting through the cutscene.
  3. Tal in his room: Tal's player controls him and the host camera follows
     him.
  4. Return to the Ailish lead, if reachable.
  5. Leave and rejoin.

  No commit and no live claim.
