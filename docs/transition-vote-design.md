# Co-op room-transition vote boundary

**Status: retired from authored campaign co-op.** Normal temporary rooms are
host-led: P1 starts the native door/interior action, Sudeki moves the active
party through its own formation path, and SudekiMP reacquires P2 only after
the destination has settled. The pure vote state machine, overlay, and late
native adapter remain compiled as isolated research for future divergent or
custom content; the live lifecycle profile keeps
`EnableTransitionVotePrototype=false`.

This is grounded in the Kamo's Shop trace: P1 initiated `LNBr_Kamo_shop`, the
party-atomic path staged Ailish, accepted the native formation transition,
then reclaimed the locked P2 roster after placement. No prompt, delay, replay,
or synthetic interaction was required.

Any future vote must be a consent gate in front of one native Sudeki action.
It is not a replacement loading system and it must never begin the native
approach or invoke `EnterTemporaryZone` until consent has reached `READY`.
The checked-in INI and `tools/continue-research.sh --party-lifecycle-trace`
keep that experiment off.

A live Player 2 veto rejected the existing integration seam. Sudeki had already
changed the controller to scripted state, approached the door, and hidden the
party before the hook deferred `EnterTemporaryZone`. Cancelling only cleared
SudekiMP's saved resource, vote state, and input suppression; it could not
cancel or complete the opaque native script task. The process remained alive
with `paused=1` and controller modes `0/0`, but no load or recovery followed.

## Research-only state flow

1. A live human requests a door/interior transition.
2. Build an active-human mask from players who currently own input. Do not
   include AI companions, selected characters that have not joined the story,
   or players who dropped out.
3. With one active human, the pure vote becomes `READY` immediately and the
   native single-player path continues without a prompt.
4. With the two supported active humans, suppress that native call and show
   the destination, requester, responses, and a five-second countdown. The
   requester is implicitly accepted. The pure policy supports four bits, but
   additional runtime input slots are not wired yet.
5. Any participating human may cancel. If every participant explicitly
   accepts, commit early. Silence auto-accepts when five seconds expires.
6. A dropout is removed from the in-flight participant snapshot. A new drop-in
   does not join an already-visible vote. If the requester drops out, cancel.
7. Move `READY` to `COMMITTING` exactly once, quiesce multiplayer input/cameras
   with `SudekiMpSplitScreenBeginPartyTransition`, then invoke the one saved
   native transition. The existing party-atomic settle path performs native
   placement, restores presentation, and reacquires locked roster ownership.

## Rejected late integration seam

`trace_enter_temporary_zone` at RVA `0x000064b0` is the confirmed native load
boundary and is safe for observing or wrapping a transition that will proceed.
It is too late to implement a cancellable vote. Sudeki has already begun the
native interaction task and called `CGroupPlayers::HidePartyMembers` on the
observed door path before this function begins. The experimental adapter used a
reversible presentation lease:

- consume the exact snapshot captured by the existing
  `trace_hide_party_members` hook;
- call the verified native `ShowPartyMembers` once to return the exterior to
  its pre-request visible state while the vote is waiting;
- on cancel, leave it visible and clear only the owned lease (this restores
  presentation, not the native interaction task);
- on commit, call the verified native `HidePartyMembers` once before the saved
  transition so the native hide-depth accounting is identical to an ordinary
  door entry.

Do not integrate the vote by either shallow-copying the `resource_name`
argument or reconstructing it from the destination text. The third argument
may identify an authored start marker distinct from the room name, and its
12-byte value contains a reference-backed pointer. The adapter uses
`SudekiMpCleanroomEngineRetainResourceNameExact` to copy the exact value and
increment its validated backing reference once, then retains it through the
corresponding exit following the existing `active_temporary_resource`
lifecycle. It also validates that the saved world pointer, source-descriptor
identity, and source generation are unchanged before commit. A different
transition, cutscene, unload, or destination request cancels the pending vote
rather than replaying a stale native call.

The late adapter can block the native load, but that is not equivalent to
cancelling the action that called it. Every veto, missing-overlay failure,
changed-source rejection, or dropout can strand the already-running native
script. Consequently this adapter must not be enabled in an integrated or
user-facing profile.

The release gate is a verified target-specific hook before native OnAction
changes controller state or begins the approach. It must retain the exact door
target and original host/target arguments without starting the task. Accepting
may invoke that exact callback once under a one-shot target/world-generation
token; veto and all presentation failures must simply discard the retained
request while native controller, pause, and script state remain untouched.
Replaying global `ac_GUI_Select`, restoring controller fields manually, or
calling `EnterTemporaryZone` after a veto are not acceptable substitutes.

## Input and presentation

The pure helper does not read devices. For the current two-player runtime,
Player 1 is implicitly accepted and may veto with `Esc`; Player 2 accepts with
the input-bridge `A` edge or vetoes with `B`. Responses carry the prompt serial
so a button edge from an old prompt cannot affect a newer one. Future
controller slots use the same four-bit participant mask.

During `WAITING`, the controller hook skips Player 1's native controller update
and neutralizes Player 1 movement plus Player 2's movement, attack, skill, and
camera state without dropping roster participation or collapsing split-screen.
The update observer continues running so raw consent edges, the timer, and the
overlay remain live. The vote adapter reads raw bridge A/B edges through a
separate consent-only seam. Releasing the gate requires one neutral bridge
packet so a held vote button cannot leak into Player 2 gameplay. A separate
release latch keeps Player 1's native and cleanroom-menu input frozen until
`Esc` is physically up, preventing the veto press from opening a native menu on
the next frame. At `COMMITTING`, the existing party-transition quiesce runs
before the native transition; this releases Player 2 control/camera and
prevents another action from entering between consent and the native load. The
native Player 1 door transition owns Player 1 control from that point.

The receiver rejects duplicate, out-of-order, and ambiguous half-range
sequence numbers with wrap-aware ordering, and resets its sender epoch only
after a real timeout. A vote additionally refuses Player 2 consent until it
has observed a strictly newer, fully neutral packet after opening; only later
newer A/B edges may answer that serial.

The vote panel is implemented inside `SudekiMpCleanroomMenuRender`, preserving
the existing split-screen overlay renderer owner. It renders the destination,
Player 1/Player 2 status, controls, and a `5.0` to `0.0` countdown derived from
`SudekiMpTransitionVoteRemainingMs`; no external assets are required. The
first successful draw acknowledges the exact prompt serial and restarts the
full five-second visible window. At the future pre-OnAction seam, a failed draw
or unreported prompt must safely discard a request that has not started native
interaction. The current late adapter cannot provide that guarantee and
therefore remains disabled.
