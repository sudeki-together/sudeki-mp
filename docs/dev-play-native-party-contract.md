# Dev Play native party replacement

This describes the initial all-Talos/None exterior route. Native party
replacement has live evidence; full gameplay acceptance remains separate.
Mixed selected heroes, duplicate hero construction,
temporary areas, and story-script recruitment after replacement are outside
this adapter's admission contract.

## Why the saved heroes appear

`CONFIRMED_STATIC`: native `SpawnLoadedPlayers` (RVA `101D60`) reconstructs the
saved player records before Dev Play's independent avatar jobs run. The native
group tracks pending player construction using its `+58/+5C` counts and `+60`
handles. Choosing Talos in the separate picker does not alter those records.
Changing only a record or pending count would break that load protocol.

The implementation lets that native load finish, then removes the unselected
native party actors through the engine. It does not edit or serialize the save,
hide a hero, change a hero's model, or reuse a hero's identity for Talos.

## Native operations and ownership

`CONFIRMED_STATIC` / `CONFIRMED_EXACT_IMAGE`: the public group functions accept
a native `GELPointer` resolving an actual entity. `AddPlayer` (`23230`) has no
hero-only predicate. `SetAsLeadPlayer` (`24E40`) checks membership and arbiter
eligibility, then queues the entity at group `+C0`; the original group update
(`23080`) and rotation (`24060`) perform the switch. `RemovePlayerAndDelete`
(`235E0`) removes the actual member and invokes native resource deletion.
The name-based `RemovePC` path also serializes player state and is deliberately
not used.

Native pointer factory `1C20` is stdcall with one entity argument. Its result
has the `GELGroupPtr` class at `2C0098`; deleting destructor `1B30` is thiscall
with flag 1. The adapter creates and destroys each wrapper synchronously around
one group call. Deletion succeeds only when the wrapper's native weak entity
handle clears and the exact catalogue no longer contains that actor. An
unavailable or unexpected result quarantines the operation; it is not retried.

The initial sequence preserves a nonempty native group:

1. Delete unselected nonleader heroes, leaving the original leader.
2. Add the independently completed local `ALLY_TALOS` entity.
3. Queue native leadership and wait for the original engine switch and pending
   handle retirement. The group has exactly two members during this step.
4. Delete the former hero leader, now a nonleader.
5. Admit gameplay only with one native member: the actual local Talos.

Other players' Talos instances retain their own spawn identities outside this
process's native group. Native leadership compares resource identifiers and
does not distinguish two same-resource Talos actors; this implementation never
asks it to rotate between them.

The leader closure (`237B0`, `EF700`, `EC2D0`) assigns the native controller's
weak target, camera target and player AI mode. It uses Talos's actual generic
components. Native leader AI mode is 0 with override reference count 0. The
local control adapter must preserve that state; outside-group remote avatars
continue to use their separate companion override leases.

## Admission and lifecycle

`lan_story_avatar_party` checks the supported image, native dispatch, original
roster, spawn generation, world, catalogue, component classes and backlinks,
AI state/buffers, native formation owner, neutral input caches and input hook.
It acquires the verified native input filter None before changing membership.
The request (`8AC0`) changes controller pending filter `+84`; it leaves current
filter `+80` unchanged. Native controller update `27CF0` commits the pending
value at `285D7`. The adapter retains an explicit `FILTER_PENDING` phase for
the expected `1/0` current/pending state, then requires `0/0` on a newer native
dispatch before any membership operation. It never repeats the request or
writes the current filter itself. Other filter states quarantine the lease.
The native HUD controller, initialized four-widget group, animation/model owner
chains and membership listeners must already exist. Default native Ally HUD
handling is bounded, but initially shows the default portrait; the replacement
HUD binds only after party readiness.

The public AddPlayer entry contains the task tracer's existing call hook at
`23260`. Before hashing that entry, the party verifier requires the tracer's
exact hook record, live observer target and retained native target, then
normalizes only the four displacement bytes. Pristine code is accepted only
while the tracer is fully uninstalled. This code-ownership check is available
before load/thread enrollment and grants no native object or task lease.

The spawn observer exposes explicit add/remove/rotation receipts. Observations
are suspended while a native operation owns a receipt. Only the exact expected
membership change can rebase all completed spawn identities. Unannounced group
changes still invalidate them. Party observations publish current members and
heroes only; deleted hero entries and unused member entries are NULL. Expected
pending transitions preserve the original scene epoch and remain LOADING.

Group `+D0` is a native switching prohibition. It gates unfinished membership
work but does not by itself replace the identity of a completed party.

The spawn observer remains the sole shared EntitySetup consumer. Shutdown order
after positively verified native Quit is:

1. `TaskTraceForgetExitedWorld`, which emits the shared positive world-exit event.
2. `AvatarPartyUninstall`, which consumes the spawn owner's exact
   epoch/load-generation/world exit receipt without touching retired objects.
3. `AvatarSpawnShutdown`, then shared task observer teardown.

Neither NULL globals nor elapsed time establish retirement. A live or uncertain
party prevents unload. Native pending rotation receipts retire through the same
positive world-exit event; no cancellation is fabricated.

## Reproduction and limits

`tools/ghidra/DevPlayHeroExclusionReport.java` is a read-only, SHA-gated report for
the supported executable. The adapter's normalized signatures contain hashes
and relocation offsets, not proprietary executable contents.

`CONFIRMED_TEST`: `StoryAvatarPartyImageTest` checks exact-image signatures and
fixture native operations through the complete transition, AI/input/HUD
refusals, uncertain deletion, unchanged pending selection, post-readiness native
switch locks, signature drift, pending/ready/unknown exit and reinstall. The
test runs the relocated native filter request against fixture memory and
checks delayed commit, no repeated request, no early membership operations,
foreign filter transitions and pending-filter exit. Its native group calls
and later controller commit are fixtures; it does not delete live game actors.
`StoryAvatarSpawnImageTest` executes the real relocated constructor and shared
completion observer with fixture actors. It checks membership receipts and
rejects premature, wrong-world, wrong-epoch, wrong-generation and stale exit
receipts. It also installs the real task-tracer hooks before load/thread
enrollment and verifies party compatibility, foreign-call/body/metadata
tamper rejection, and restored pristine code after teardown.

`CONFIRMED_LIVE` (2026-10-07, candidate `834a3e9f`): both processes logged the
pending filter, later committed filter and party readiness with zero heroes,
one native member and membership revision 5. A read-only inspection found two
independent Talos actors and no actor of any of the four canonical hero
classes in either catalogue. Each native group's sole member and controller
target was that process's selected local Talos; filters were `0/0`. The host
local leader retained AI mode 0 with override count 0, while its separately
controlled remote Talos retained override count 1. Both processes reported
native and replica loading complete. These observations establish the actor
replacement.

`CONFIRMED_LIVE` (2026-10-07, candidate `85a275e`): both processes again reached
party readiness with zero heroes, one native member and membership revision 5.
Screenshots confirmed Talos names and portraits in the native HUD. After the
host selected End Session, both processes logged a verified native Quit
return, task-journal retirement and completed runtime cleanup, and both
visibly returned to the title screen. Starting another session in those same
processes remains pending verification.

Normal shutdown restores camera targets and drains gameplay actor leases
before native Quit. The party's None filter and character-input hook remain
owned through Quit; the verified world-exit receipt then retires the party
without restoring deleted heroes or writing a filter into a retired world.
These checks do not establish camera quality, damage behavior, mixed
selections or general campaign compatibility. A save can contain scripts
that expect canonical heroes; this adapter
does not claim to repair those scripts or make arbitrary campaign areas safe.
