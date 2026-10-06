# Independent story-area ownership

## Owner-selected behavior

For story issues #24/#25, only a player using an authored door travels. Other
players can stay outside with control, collision, interactions and world time
continuing, or enter later themselves. Forced party travel, replacing their
view with a spectator, or freezing them until the host returns is not success.
The owner approved pursuing host-managed occupied areas. The first integration
boundary is New Brightwater plus one authored temporary interior. This is a
research boundary, not a permanent product limit. Dungeon catch-up travel (#37)
and independent NPC conversation work remain separate.

## Current implementation boundary

`src/engine/story_area.*` is a pure host-side lifetime/membership policy. It
does not call native functions, change the saved-story wire protocol, acquire
an actor, or make either area simulate. The runtime is not connected to it yet.
`SudekiMP.StoryAreaTest` tests this policy only; a passing result does not prove
independent doorway travel, loader progress, collision, rendering or teardown.

`lan_story_area_membership` adds read-only native observations of each sparse
party member's own packed region. It checks descriptor-table membership,
resource identity and reciprocal collision ownership, then re-observes before
publishing copied fields. The story host samples at most once a second and
logs at most 64 changed snapshots; no gameplay decision consumes these logs.
The area's name/descriptor comes from the native AI-tracking cache. A separate
movement-sector observation reports the collision-produced packed number, its
range validity, region agreement and opaque native flags. Missing movement or an
invalid native sector is explicitly unknown, not replaced by the tracking cache.
Placement must establish both before treating travel as settled; matching caches
alone are still not active simulation or collision-query proof.
Present resources and collision registration/enabled fields are diagnostics,
not proof of active simulation or safe collision queries. The adapter currently
requires the existing READY roster witness and does not observe through loading.
Its supported-image/synthetic topology test does not execute native gameplay.

The same adapter can resolve an explicit area's authored arrival marker from
its resident native catalog, checking both the native identifier and exact
authored text. It does not change the global current room, construct resource
names, request loads or create missing markers. Duplicate IDs in that area,
wrong-area or missing/duplicate navigation sectors, incomplete resources and
changing catalogs fail without publishing a destination. The copied result is not a native lifetime lease or
travel authorization; movement and activation still require separate integration.
Navigation-catalog membership alone does not prove terrain collision, active
simulation or that a character's native movement/AI sectors have been updated.

`lan_story_collision_owner` prepares collision-source identities without running
a native query. It requires unique enrollment in both the collision system and
spatial grid, then resolves reciprocal terrain ownership or a bounded chain of
native position parents to a registered entity. Auxiliary trigger data is not
treated as a terrain descriptor. Unknown, duplicate or changing ownership fails
without publishing an identity. Entity ownership is not an area assignment: the
future coordinator must map it through its retained area lease. The returned
native addresses are synchronous comparison identities, not lifetime leases.
This resolver is not called by runtime yet and installs no collision filter.
`SudekiMP.StoryCollisionOwnerImageTest` covers supported-image layout gates and
synthetic ownership only, not native collision, travel or concurrent-area play.

Its separate authored-origin lookup now joins those entity identities to unique
resident spawn/resource bindings for supported scenery, NPC and breakable
classes. It scans noncurrent resident areas too, checks exact resource identity
and authored names, and refuses duplicate bindings or incomplete catalogs. Two
captures detect a position child changing parent or a spawn changing its entity
binding without an allocation change. Party actors and projectiles are excluded.
Authored origin is deliberately not current membership: the coordinator must
still retain native ownership and track transfers, explicit player assignments
and projectile birth areas. The lookup is synchronous preparation, not a cached
lease or a hot collision callback. Expanded owner image tests cover these
bindings, ambiguity, reassignment, read-only behavior and bounded catalog work;
they do not execute native spawn, travel or independent simulation.

`lan_story_collision_query` adds an isolated low-level movement-query adapter.
It is compiled but **not installed or called by story runtime**. Its explicit
query requires a complete, sorted source-to-area enrollment supplied under
native lifetime ownership. It checks the grid's full enrollment and policy
area lifetimes, pins those policy records through the call, and copies the
source map into private synchronous storage. These policy pins do not themselves
retain any native object; the native coordinator is still required.

The early admission bridge excludes foreign-area sources before the native
500-source broad-phase limit. It preserves x87/SSE, registers and flags, checks
the movement caller, and does not unlink grid nodes or change collision flags.
Unscoped searches keep native behavior. Unknown candidates remain present but
invalidate the query. A second seam witnesses normal geometry completion; the
native scratch-capacity bailout paths do not count as success. An incomplete
query must never be consumed as valid hits or interpreted as an absent floor.
The adapter retains dependencies after failed restoration or pin release.
`SudekiMP.StoryCollisionQueryImageTest` covers exact-image seam installation,
synthetic early admission, ABI preservation, policy pins and teardown/retry.
It does not execute retail collision or prove native object leases, gameplay,
area activation, loading safety or performance. Whole-grid preflight remains
costly and requires measurement/cached enrollment before live integration.

`SudekiMP.StoryCollisionTerrainImageTest` extends that evidence by executing the
supported native candidate collector, triangle distance tests and hit storage
with invented overlapping floors. The real scoped-query adapter admits only
the requesting area's floor in either grid-list order. A disabled own floor
and an actual triangle-distance miss produce complete empty results, without
using the overlapping foreign floor as a fallback. Five hundred foreign
candidates starve the local floor in the unscoped query; early scoped admission
avoids that starvation. Saturation within the requested area remains explicitly
incomplete and does not publish an empty result as absent terrain.

The same fixture executes native terrain suspension/resumption, collision-system
vector removal/insertion and pending spatial-grid placement. Across repeated
alternating cycles in both initial list orders, removing one floor leaves the
other queryable. The descriptor and resource bytes remain intact despite loss
of the removed floor: readable/resident records are not active collision.
The stale enrollment snapshot is rejected before touching output. Recapturing
the actual enrollment permits a complete empty result; native re-enrollment
and pending-grid processing restore only that area's floor. The test does not
repair grid membership itself between these operations.

This is bounded `CONFIRMED_EXACT_IMAGE` evidence, not gameplay proof. The fixture
uses synthetic owners, geometry and lifetime notifications, two matrix-import
substitutes, and an explicit masked 53-bit x87 environment. Other floating-point
modes and mapped CRT error handling are not covered. Only the actual production
query/lifetime hooks patch native text; the image is hash-gated and full image
bytes, page permissions and caller math state are restored. No real terrain
loading, movement solver, native resource keepalive or independent travel is
established. Source: `tests/story_collision_terrain_image_test.c`, specifically
`native_floors`, `native_capacity` and `native_terrain_enrollment`. The target
and the Query, Owner, Lifetime and Scheduler image-test regressions passed on
2026-10-05 against the supported image documented in `executable.md`, at revision
`3238d5b3d65ea2b56d251c0f597c012bec299f2e` plus uncommitted checkpoint-36 work.
Run each target with a legitimately supplied supported executable as its sole
argument. No production runtime integration or live acceptance is implied.

`lan_story_collision_lifetime` now supplies passive incarnation history to that
query adapter. It observes the persistent heap-source factory's return and
invalidates a source at deletion entry; temporary stack query objects are not
enrolled. A reused address gets a new identity. The query checks every source
incarnation and a common journal revision before native work, then rechecks the
revision on return. Replacement during collection produces an incomplete result.
This detects stale identities but does not retain native objects or decide their
area. Authored origin, player assignment and projectile birth-area enrollment
still need the native coordinator.

The observer requires suspended startup before native world/collision creation;
it cannot bootstrap continuous history by scanning an already-running game.
Native events bind its owner thread independently of the installation thread.
Missing, duplicate, foreign-thread or exhausted events latch unknown; no runtime
reset API manufactures fresh history. The query remains attached through failed
teardown, and failed removal closes new admission. Exact-image/synthetic tests
cover the journal, actual assembly bridges, same-address reuse, query/journal
integration and startup-to-native-thread handoff. Neither adapter is installed
by the story runtime yet; live native coverage and performance remain unproven.


`lan_story_area_task` supplies a separate experimental loading-job observer,
compiled but **not installed or called by story runtime**. A game-thread caller
arms an exact unloaded descriptor before submission. The observer records the
request before it reaches the worker queue and retains its policy pin until
both the enqueue call and the request's destructor have returned. Worker-side
callbacks compare opaque identities; they do not dereference a destroyed
request or mutate the game-thread area policy. The caller must still retain
the native world/descriptor separately: the policy pin is not a native lease.

The observer does not load, cancel, unpause or move anything. A terminal receipt
proves only that particular request's lifetime, not successful loading, child
resource tasks, visibility readiness, collision or playability. Unknown events
retain dependencies. Removal closes admission and requires a quiescent worker
boundary; failed restoration retains originals and supports retry. The focused
`StoryAreaTaskImageTest` covers exact-image patch/lock signatures and synthetic
threaded job lifecycles, ABI preservation, freed requests, reused addresses,
invalid observations and teardown failures.

The same owner now observes the request's native resource-publication stores.
Its receipt copies the descriptor, returned interface and typed zone-resource
identity after those stores, within the matching request's worker execution.
It does not read any of those objects in the observer callback. An early return
has no publication; a published non-zone interface has no typed zone resource.
Neither is silently upgraded to successful loading. Duplicate or inconsistent
publication closes observation and retains the existing policy dependency.
The typed result now copies the resource journal's LIVE generation at that
publication, under the resource owner's lock. New watches require an attached,
healthy resource journal; missing or unhealthy history at publication invalidates
the request observation without publishing partial fields or releasing its
policy pin. A non-zone result retains generation zero. A copied generation is
immutable even after that address is reused or the coordinator acknowledges
consumed root history. Lock order is request observer then resource observer;
the resource owner never calls back into the request owner.

This is identity bookkeeping, not resource keepalive or a journal pin. The
coordinator must consume root history before acknowledging it, and retain
native ownership across lookup, publication and later use. This capture does
not establish the lifetime of the resource during earlier native lookup or
prevent native destruction between calls. An address match to an arbitrary
later finaliser remains insufficient. No runtime integration is enabled.

The expanded request test exercises all four installation/restoration stages
and the publication bridge's registers, flags, floating-point state and last
error. It also runs the supported native request body on a real fixture worker,
with resource/name lookup replaced by synthetic dependencies. The native type
check, publication and early-return paths execute, using a real fixture resource
manager lock. There is no real resource loading, gameplay or readiness proof.
The fixture links the real resource journal and executes its construction/body
bridges with synthetic native bodies. It checks same-address replacement,
acknowledged old history, new-request generations, absent/dead resources,
missing consumers and observer ownership loss after request admission.

`lan_story_area_finalise` adds a separate experimental finaliser journal,
compiled but **not installed or called by story runtime**. It observes the
class-specific enqueue, run and destructor plus the enclosing resource update.
It copies opaque register identities into bounded records; worker events never
dereference native objects or change game-thread policy. Reused task addresses
receive new tickets, and cancellation without a run remains distinguishable
from a terminal setup return. Neither result is successful loading or readiness.

A future coordinator can consume snapshots only at a verified game-thread
boundary outside observed callbacks. A record can be acknowledged only after
both submission and destruction have returned, outside the enclosing update.
The journal now also associates the initial finaliser's visibility submission
with its exact native queue node. Acknowledgement waits for that node's free
call to return after dispatch, even if the finaliser has already been destroyed.
Unrelated reloads using the same descriptor and callback cannot discharge it.
Freed-node address reuse receives the new finaliser's distinct ticket; missing,
duplicate or foreign-thread ownership fails closed. This releases no native
object or policy pin. Callback retirement still does not prove initialization
succeeded: cancellation can skip it, and initialization can enqueue further
work. Scripts, other resource work and outer pumps remain separate obligations. Overflow or
inconsistent events retain dependencies and close observation; there is no
runtime reset that manufactures fresh history. Startup rollback is retryable;
live removal intentionally refuses until native producer admission and owner
drain have been integrated. Worker exit alone is insufficient. The adapter must
not be enabled before that ownership work. The focused test covers thirteen exact
seams, synthetic threaded lifetimes, ABI state, address reuse, cancellation,
overflow, and startup restoration/retry. Additional native-connected modes run
the real finaliser stage, queue and descriptor callback with synthetic resource
dependencies, checking delayed retirement and same-descriptor unrelated nodes.
They do not execute real resource loading, establish native owner retention or
prove gameplay; even the cancelled callback can produce a retirement receipt.

The same adapter now records the separate deferred area-cleanup lifecycle. It
associates the native child-list copy with its source resource and cleanup job,
then observes enqueue and the cleanup job's deleting destructor. These copied
addresses are not native leases; the resource generation is tracked separately
as described below. A receipt cannot be
acknowledged until copy and enqueue have returned, the destructor has returned
through the exact resource-manager deletion path, and that enclosing update has
returned after native list erasure. Snapshot/acknowledgement never calls a native
readiness predicate or advances cleanup. Unobserved deletion paths, foreign
threads, duplicate/inconsistent events and overflow quarantine the journal.
Same-address cleanup-job reuse gets a new ticket without consuming the older
receipt. Pending retirement records also prevent detaching the consumer.

`StoryAreaRetirementImageTest` executes supported native copy, enqueue,
resource-update, graphics cleanup, child deletion dispatch and list-erasure
control flow with this observer installed in an isolated mapped image. Allocation,
vector resizing, names and child/graphics objects are synthetic. It checks two
concurrent cleanup jobs and graphics counts 0, 1, 16, 17 and 33: one job can make
progress while another remains pending, and a fixture scheduled task runs on
every update. Receipt access is refused inside copy/cleanup callbacks, including
after fixture memory is freed. This is not advancing outside-player simulation
or live game-time evidence. The finalisation test separately covers retirement
ABI preservation, address reuse, nested updates, bad ownership/phase/thread,
overflow, insufficient snapshot capacity and all thirteen install/restore stages.

These receipts are one cleanup obligation, not proof that an entire area or
world is gone. Outer root deletion, resource keepalive, other descendant jobs and
producer admission remain separate work. No runtime installation is enabled,
and empty journals still do not authorize live removal of these hooks.

The owner also observes resource construction and the entry/return of its
destructor body. Each construction gets a unique resource generation; native
finaliser enqueue and deferred cleanup copy bind that generation synchronously.
These associations no longer rely on later address matching. A finaliser whose
resource has been destroyed or replaced cannot publish a valid run observation.
Constructor entry/return must agree on the allocation identity and thread;
the later destruction may run on another thread but its own entry/return must
match. No observer callback dereferences the resource. Missing, duplicate or
inconsistent lifetimes quarantine the journal while originals still execute.

Resource history remains until the destructor body has returned and all
associated finaliser/cleanup receipts have been consumed. This is NOT native
keepalive: destructor entry already ends the live identity, and body return
precedes the enclosing deleting wrapper's possible allocation free. Retiring a
history record does not authorize world teardown or say all other descendants
have finished. The requesting worker now captures its generation synchronously
from this journal; it does not join a later snapshot by address. This capture
keeps an immutable copy, not an additional root-history pin. World/resource
ownership, producer admission and the playable-area coordinator remain
unimplemented.

Expanded finalisation tests cover same-address resource replacement while old
job receipts remain, history consumption ordering, stale jobs, missing/duplicate
construction, bad constructor return, wrong return thread, capacity exhaustion,
and construction/destruction bridge ABI. The native-connected cleanup fixture
now passes through these bridges and checks the copied generations, but uses
synthetic constructor/full-destructor bodies. It does not execute native full
resource construction/destruction or establish occupied-area retention.

`lan_story_area_eviction` adds an experimental descriptor-eviction reservation,
compiled but **not installed or called by story runtime**. It filters the two
native automatic-swap unload calls, exported named and last-active unloads, and
per-descriptor admission in non-active area cleanup: reserved descriptors are skipped, unrelated candidates keep
native behavior, and destination load calls are unchanged.
The caller must separately retain the native world and descriptor table. Its
policy pins and identity checks do not establish a native lifetime lease.
Other explicit teardown, remove-all/Nexus cleanup, world replacement, temporary-area suspension
and other resource teardown are not covered. Non-active cleanup still clears
the native world's pending slots and toggles its cleanup mode; the coordinator
must separately own those contexts. This alone cannot keep an occupied area
playable. Unknown observations retain reservations and stop these descriptor
evictions; they do not claim that loading succeeded. The focused image test
covers selective filtering, assembly ABI preservation, native-thread handoff,
admission failures and retryable restoration for all six seams. A separate
bounded case runs the native non-active cleanup loop with synthetic resources
and stubbed cleanup operations, asserting both descriptor protection and the
remaining global pending-slot writes. It does not execute real resource
destruction, loading, travel or gameplay. Another case runs the native named-
unload wrapper with synthetic lookup and resource state changes, checking
reserved/unreserved/missing destinations and native behavior after release.
Skipped named unloads are not queued or replayed when a reservation is released.
The native last-active cleanup loop is also exercised with synthetic state
changes. Its two callsites share the same reservation owner and existing ABI
bridge. Reserved neighbors and the last-area root remain unchanged, while
unreserved candidates keep native loop behavior. The native current-area
exception applies to neighbors only; the root still needs protection even
when it equals the current area. The fixture covers 240 current/last/state/
reservation combinations, repeated requests, empty neighbors, a missing last
area and native behavior after reservation release. It checks the full fixture
world remains unchanged by the wrapper. This does not execute native resource
destruction or establish a general native world/resource lease.
The same test now executes the native named-load wrapper with synthetic lookup
and state-change dependencies. It verifies that repeated calls request loading
state and clear the descriptor's load context even for an already loading or
resident descriptor. Eviction reservations leave this separate operation alone.
The coordinator must reuse an existing area's load/lifetime for later entrants;
a repeated native load call is not an idempotent residency check. The test does
not execute the loader or prove what downstream resource work such a call starts.

`lan_story_area_close` now owns an experimental early window-close admission
gate, compiled but **not installed by runtime**. A future area coordinator can
hold one exact game window, observe a close request, and post it back only after
separately proving its dependencies drained. It intercepts before the native
stop-running write; it does not destroy the world from a controller callback.
A private window property binds a never-repeated ticket to the window incarnation,
so a reused handle alone cannot receive an old deferred close. Foreign markers,
wrong threads, lost hook ownership and unknown observations retain the hold.
Posting failure is retryable. Once admitted, nested/repeated delivery cannot
start native cleanup again. Dispatch is not cleanup-return or task-drain proof.

This covers WM_CLOSE only, not save loading, frontend quit, script cleanup,
ordinary area retirement or an explicit DLL-unload policy. It must not be enabled
without a coordinator able to service pending requests and own those other
boundaries. Startup rollback and unheld restoration are retryable; pending,
posted or unknown obligations retain the callback dependencies.

`StoryAreaCloseImageTest` retains its test-only native control-flow
experiment and now exercises this adapter. It runs the supported window-message procedure and nested message
pump with synthetic imports and cleanup. Its fixture demonstrates that close
deferral must precede the native stop-running write: skipping only cleanup
still stops the loop and reaches window destruction. Early deferral preserves
the running flag and unrelated message handling; a later explicit fixture
request can take the original close path. All patched bytes/imports and page
protections are restored for the unretained fixture. Adapter modes cover its
actual assembly bridge, register/FP preservation, install/restore failure and
retry, startup/native-thread handoff, repeated/nested closes, post failure,
window-marker replacement/removal failure and handle reuse. Retained-mode
fixtures keep the adapter and image allocated until test-process exit; this is
not successful live uninstall. One separate mode checks real window properties
and asynchronous posting on its own invisible message-only test window, with
synthetic procedure identity and no retail procedure attached. No game window
or game resource is destroyed. These tests do not prove native task drain,
world/resource lifetime retention or safe independent-area gameplay shutdown.

`StoryAreaIntentImageTest` retains its baseline experiment for a different
destruction boundary: native pause-menu reload/quit and exported load/quit/remove wrappers. It
executes the menu input caller and action body with synthetic UI/catalog owners,
recording substitutes for script dispatch, loading, audio, fade and world reset.
A fixture-only early gate defers the reload/quit action before its script event
and UI changes. Repeated late refusal at the cleanup callee still reaches those
effects repeatedly; it is not whole-request deferral. Other menu actions pass
through the selective gate. The earlier input helper still runs, so this is not
a claim that all input-side effects have been intercepted.

The fixture also exercises both reload counter branches, successful/failed
record-reader results and an absent selected save. The outer menu caller drops
the reader's success/failure while its UI changes remain. A simulated script
event changes the selected catalog record before reload resolves it: storing an
earlier index is not proof of the record ultimately read. Exported quit/load and
the title index wrapper bypass the fixture's menu gate, and an unready exported
load can mutate catalog-request flags without calling the record reader.

Those are bounded native caller/control-flow results, not execution of actual
story scripts, loading or destruction. The baseline mode restores all image
bytes and executable page permissions. Integration must share the existing
story-load and story-exit owners and account for additional exported/script
routes; gating only an inner reset or only the two title/save-page callsites
cannot establish whole-world ownership.

`lan_story_area_intent` now supplies a separately compiled early menu-action
admission guard, **not installed or called by runtime**. One adapter owns the
quit-menu action entry before its event, UI and reload/quit effects. With an
attached consumer, only reload/quit from the verified native input caller are
admitted or deferred; unrelated selectors preserve native behavior. The same
adapter also guards the public quit-to-front-end export before its first scene
dereference or cleanup effect. Its initial global-pointer load and tail remain
intact for the existing lobby exit owner's identity check. These requests share
one journal: deferred receipts copy a monotonic ticket, request kind, phase,
source and optional signed save index, never a menu/catalog
pointer. Acknowledgment only removes that journal record after the coordinator
adopts the whole request; it does not replay native code or certify cleanup.
Outstanding receipts prevent detach and a newer RUN overtaking deferred work.
Unknown decisions, capacity loss, callback reentry, foreign threads or hook
ownership loss retain the guard and close destructive-action admission.
The copied source distinguishes menu, quit-export, load-export and remove-all requests; it is not
caller authentication or permission to run story consequences.

The load-export path reuses the existing `save_book_intercept` patch owner in
an exclusive, opt-in story mode. It installs no save-menu hook or local vote and
does not initialize native catalogs. Its paired begin/end callbacks span the
admitted original call or deferred return. The cdecl stack argument is copied
for the inner original call; neither the export index nor a receipt is a file
lease. The parent intent adapter owns the four-stage installation/rollback
transaction; the shared owner retains a failed stage until that rollback.
Legacy vote teardown cannot detach the story registration, and incompatible
profile installation refuses the already-owned export.

The fourth stage guards public `RemoveAllZones` before reading the world pointer
or entering native cleanup. Its separate REMOVE_AREAS kind shares the same
bounded admission journal, so it cannot overtake a deferred menu, load or quit
request. Deferring it publishes no native pointer and makes no change to native
world state. RUN executes this original wrapper now; acknowledgment never
replays it. The caller of this void export still continues after deferral.
Direct internal cleanup/reset calls, descriptor and world destruction, and
other explicit unload paths bypass this wrapper and remain uncovered. This
guard is not a native resource lease or a substitute for whole-caller admission.

The focused image test executes the real wrapper and its native empty-world
cleanup branch with a synthetic world. Deferred repeated requests preserve the
entire fixture world, including current/pending context; a missing global world
can be deferred before dereference. Admitted or unattached requests perform
the expected native context clearing. No descriptors or child resources are
present, so these checks do not exercise resource destruction. Additional modes
cover all cross-route ordering pairs involving remove-all, callback reentry,
foreign threads, hook ownership loss, ABI/FP preservation and a still-live
remove-all trampoline after failed startup rollback. All four installation and
restoration stages, including ten partial-rollback positions, are covered.

The consumer must synchronously own each deferred plan, including any exact
reload-file reservation, and later use a separately verified exit/load route.
This module neither pins a save nor keeps a menu alive for delayed replay.
The original script event may change the selected save before an admitted
native reload; the guard does not promise that an earlier index stays selected.
Deferring the void quit export still returns to its caller; it does not suspend
a script or undo effects the caller already performed. The coordinator must
authorize its own final native-exit call after drain, not interpret a deferred
export return as completed cleanup. The load export is likewise void: deferring
it does not suspend its calling script or undo that caller's earlier effects.
Earlier input cues, direct reader/index loads,
outer caller effects, direct inner-quit paths, save pages, WM_CLOSE, other world
cleanup and doors remain separate boundaries. Do not enable this guard without
a coordinator and coverage of those routes. After any native action, uninstall
retains its hook/dependencies for process lifetime; that is not a policy for
explicit DLL unloading.

Additional `StoryAreaIntentImageTest` modes link the actual guard and execute
the native input/action with synthetic consequences. They check deferred state
preservation, admitted native behavior, menu/catalog storage reuse without
retained pointers, acknowledgment without replay, no overtaking, and the
explicitly unguarded direct index route. Export modes cover deferral with a
missing scene, admitted native no-title behavior, unchanged lobby entry checks,
and requests from either path attempting to overtake the other. Separate modes cover unknown decisions,
overflow, callback reentry, wrong callers, foreign threads and lost ownership.
The ABI modes use native entry/callsite code with synthetic tails and check
registers, flags, stack, x87/SSE/MXCSR and LastError preservation. Injected
startup failures before/after all four patches, rollback restoration failures,
ordinary restoration failure and foreign patch ownership all exercise retry.
Every independent restoration is attempted in reverse order. Attachment during
partial restoration refuses before probing missing hooks, preserving cleanup
retry. Separate retained-startup modes execute any remaining hook after
failed installation and failed rollback, verifying that its original trampoline
remains usable while new consumer attachment and reinstall are refused.
Retained-mode fixtures leave the guard and mapped image alive until isolated
process exit. These checks do not execute real scripts, save loading or native
destruction, and do not establish native world ownership or playable areas.

Load-specific modes also defer signed invalid indexes before catalog access,
check ordinary admitted/native-unready behavior, cross-route ordering, reentry,
thread and ownership loss, and verify the copied stack argument in the ABI
fixture. The focused target compiles the same shared owner with unrelated
legacy UI code removed by the compiler; `SkillTraceImageTest` separately links
the full ordinary owner and tests existing hooks plus profile exclusion and
legacy teardown during story ownership. Neither fixture opens user saves.

The existing `lan_story_load` owner now has an optional reload-file reservation
API, **not invoked by runtime**. It acquires the reviewed local file pair and
checks the current native selected record/root before accepting a request. It
stores copied metadata and an opaque file lease, not a delayed native menu or
catalog pointer. The active load attempt/fingerprint remains available while
the old runtime drains. The reader's existing owner also services reads of the
reserved files; no second native file-open hook is installed.

Ordinary lobby cancellation can retire the old load proof only after positive
native exit **and** completed runtime cleanup. It preserves the reserved pair.
Once that lobby is inactive, the coordinator may promote the same file handles
to a new prepared load; the next ordinary preparation must verify the matching
local fingerprint before adopting it. Wrong selection, duplicate native record,
unknown exit, stale ticket or wrong consumer cannot promote a different save.
Explicit reservation cancellation releases only the pending pair. No entry
point in this API initiates native quit, loading or a network session.

`StoryLoadIdentityTest` covers those transitions with synthetic leases/catalogs,
including an unreadable former catalog during promotion. `StoryReloadFilesTest`
uses real Win32 handles and hashes over test-created save-shaped files under a
unique temporary root. It checks that writes/deletes remain denied through old
load cancellation, promotion and adoption, that the same handles transfer, and
that cancellation restores file access. Only its application-data path and
native/lobby evidence are substituted; it does not open user saves or execute
native loading. These tests do not prove whole-request menu admission, automatic
reload orchestration, native cleanup, reconnect or multi-area gameplay. The
coordinator must still connect those boundaries; do not enable this API alone.

The existing `lan_story_task_trace` owner now offers optional pre-fetch script
admission alongside its shared cast routing. It is compiled but **no runtime
consumer is registered**. A coordinator may let an instruction run or return
native yield before the VM fetches it; it does not skip a binding after consuming
its arguments or rewind an executed instruction. Constructor-recorded identity
distinguishes pending tasks, and a cast router's later yield keeps a wait pending.
Detach, world-journal clearing and uninstall refuse outstanding or uncertain
admission. Native retirement notifies the consumer using copied identity only,
after the retirement returns; failed acknowledgment or observer uncertainty
before/during that notification retains the pending record and dependencies.

`StoryTaskHostImageTest` now executes the native scheduler/interpreter with two
synthetic tasks and tiny fixture bytecode. While one waits, the other advances;
resuming the first preserves its instruction/value-stack position. Real fixture
critical sections are used, but no real script binding, game task construction,
retirement, resource load or world consequence executes. Separate synthetic
retirement cases make the former task memory inaccessible before notification.
Unknown identity, callback reentry and unacknowledged retirement retain the
registration. The native immediate-submission wrapper is also exercised with
synthetic construction through the existing constructor bridge. Yield returns
an outstanding handle, leaves the result output untouched, and clears the
manager's executing-task field; the later scheduler can resume that task.
Crucially, its C caller returns while the script remains pending. Holding an
event script therefore does not suspend a caller's subsequent reload or quit;
whole-action admission still has to precede those caller-side effects.
An optional instruction-inspection API now runs only inside that consumer's
pre-fetch admission callback. It copies the next opcode and bounded call
metadata without fetching, popping arguments or invoking native code. For global
calls it respects compiled-function precedence, validates lookup-pool membership,
bounds collision probes, and reports the current native target when supported.
Method/child calls are explicitly separate, not treated as harmless. Output is
unchanged on invalid context or unsupported/malformed lookup. The copied result
is not a VM, resource or area lease, and a native target does not classify its
transitive effects. The coordinator must recheck on resume and must never replay
a cached target address. No runtime consumer uses this API yet.

The same exact-image target's `inspect` mode executes the native scheduler, VM,
global-binding lookup/dispatcher and public cleanup wrapper against a synthetic
empty world. Its test consumer holds the resolved cleanup target before fetch,
while another task advances; release executes the native binding and expected
empty-world field changes. It checks compiled precedence, namespace collisions,
a changed target during a wait, bounded full-table probes, malformed pools,
indices/ABI/argument count, truncated operands, callback-only access and unchanged
output on refusal. Native construction, populated-area cleanup, real script
catalogs, program lifetime, whole-action admission and live performance remain
unproven. This does not yet join script waits to the area coordinator or make
independent-area gameplay safe to enable.

`StoryAreaWorkerImageTest` separately executes the supported loader creator,
both worker loops, native queue submission/flush and the I/O wait pump. It uses
real test threads, events and critical sections, with synthetic tasks and I/O
completion. Its isolated stop requests do not authorize live worker shutdown.
It checks idle wakeup, active work after dequeue, destructor return after real
fixture storage is freed, queued cancellation versus active work, and positive
thread exit using retained duplicate handles. The fixture also checks that
native submission can strand new work after both workers have exited.

These workers are shared infrastructure, not per-area lifetime owners. Their
exit is a possible shutdown witness only after new submissions and descendant
work are separately contained; it is not an ordinary area-retirement strategy.
Queue emptiness, stop-request state and queue-lock availability cannot replace
that proof. The test restores code/imports and page protections after joining
its threads, but does not implement the production ownership/admission boundary,
execute actual resource loading or establish that vanilla shutdown is unsafe.

`StoryAreaPvsImageTest` is another test-only control-flow experiment. It runs
the supported visibility-queue submission/drain with synthetic dependencies
and real allocation/free of fixture nodes. It tests global pending-work gates,
nested callbacks, node retirement and new work submitted during an existing
drain. An empty list is not proof that its detached callback has returned;
a callback is not a per-area readiness receipt.

The same fixture runs native finalisation through empty synthetic stages and
then the native area visibility callback, with initialization and notification
replaced by recording stubs. It distinguishes a finaliser's terminal return
from a still-queued callback, checks direct-versus-queued initialization order,
and exercises the callback's foreground-selection flag. The fixture has no
real terrain, actors, renderer or scripts. Its byte/protection restoration and
bounded native control-flow results do not establish native lifetime ownership,
a production observer, initialized gameplay or safe independent-area travel.

The policy separates:

- Session identity and stable area lifetime, independent of a player's travel.
- Player assignment, character identity and actor/connection generations.
- Per-player requested, native-active and settled transitions. Source and
  destination remain retained until native work and old presentation release
  have positively completed. Uninvolved players do not enter a travel barrier.
- Exact asynchronous dependency pins and retirement. A disconnect revokes
  controls without pretending to cancel native work or remove the actor.

Repeated requests reuse one pending ticket and an already loaded interior.
A retired slot receives a new lifetime; delayed callbacks cannot retire its
replacement. An interior also retains its exterior parent while its native
return context may depend on it. Empty occupancy alone is not an unload signal.
No policy operation resets loot, saves, inventory, the session clock or story
progress. Area identifiers are not native addresses. Structs are not wire or
persistence formats.

## Native integration still required

`StoryAreaSchedulerImageTest` now executes the supported shared game-update
scheduler, both intrusive update lists, node-time dispatch and area entity
suspend/resume routines. Two synthetic descriptor/spawn catalogs contain four
generic entities split across both scheduler lists. Their virtual update and
pause/resume callbacks are recording substitutes; actual game entities, terrain,
AI, scripts, loading, graphics and audio are not exercised.

The fixture checks staggered enrollment in either order, equal native timestamps
for matching update rates, and unchanged fixture world-selection fields during
each update. Balanced nested suspension of one catalog stops only its entities;
the other continues. Resumption and repeated independent suspension do not
duplicate enrollment or produce a catch-up burst in these cases. Suspension
from inside each entity's native scheduler dispatch also preserves list integrity
and unrelated progress. Unscheduled entities remain unscheduled. A negative
case demonstrates that resuming without a corresponding suspension underflows
both the entity's disable byte and its component's signed disable count.

No native text is patched in this experiment. The executable is hash-gated,
the two fixture global pointers are restored, the full mapped image is compared
against its initial bytes, and page permissions are restored. This supports
using the existing scheduler with correctly owned enrollment, not adding a
second per-area tick or blindly undoing TEMP pause counts. It is still not a
native lifetime lease or proof of live outdoor simulation: a production
coordinator must preserve each area's resource, collision and entity ownership,
and prevent the incompatible whole-area TEMP deactivation/resumption path.

`lan_story_temp_exterior` is the first adapter that changes TEMP behavior
rather than observing it. It is compiled into the DLL but installed only by an
exclusive, default-off single-player research profile
(`[StoryAreas] TempExteriorProbe=1`); story runtime does not install it.
Static analysis of the supported image shows `EnterTemporaryZone` suspends the
current exterior and each listed neighbor through one helper (entity pause and
hide, terrain collision removal) and stops their localized audio;
`ExitTemporaryZone` performs the inverse through a second helper plus an
inline neighbor loop. Those helpers have no other callers. The adapter owns
exactly the two relative calls that begin each unit. When its consumer keeps a
state-3 exterior, entry skips the whole deactivation unit and resumes at the
native TEMP selection; exit skips the whole inverse unit for that exact
descriptor and resumes at native lead return. Nothing is resumed that was not
suspended. A mismatched exit still skips resume but latches unknown, keeping
later entries native and the hooks retained. Both function bodies are verified
by a relocation-normalized hash; any other patch inside them, including the
legacy party-transition lead-mover hook, refuses installation.

This does not load a background area, make two areas foreground, filter
collision, place a second player, or change the network. TEMP foreground
selection still runs natively and releases the exterior's presentation
resource; the main exterior's graphics release before the first seam also
remains native. Whether exterior entities, AI and collision behave correctly
while the TEMP is current is UNKNOWN until a live probe.

`StoryTempExteriorImageTest` installs the real hooks on the hash-checked
mapped image, then redirects only leaf callees (lookup, graphics release,
resource-name refresh, camera, state change, collision enrollment, resume and
the neighbor suspension call) to recording stubs, and executes the actual
native enter/exit bodies and reference-list helpers with a synthetic world.
It covers four kept round trips and native round trips with and without a
consumer, callee-saved register/stack preservation across both skip jumps,
non-state-3 refusal, detach/uninstall refusal while outstanding, install
failure rollback, foreign-byte and active-TEMP install refusal, and a separate
retained mismatch mode. The adapter's originals are replaced with substitutes,
so no native suspension, collision, AI, audio, loading or gameplay runs.
`CONFIRMED_EXACT_IMAGE` for the bounded control flow only.

### First live split-area milestone (host lead enters, remote player stays out)

A saved-story host whose lead uses an authored TEMP door can now leave other
remote-controlled characters playable in the exterior. Owner-driven two-window
runs (host Ailish, client Tal, New Brightwater church) completed several
balanced entry/exit round trips with no host fault or control drain; Tal kept
input, the NPC cluster around Tal stayed leased and advancing, and Ailish stayed
controllable after exit. This is `CONFIRMED_LIVE` for that shape only.

Pieces (all host-authoritative):

- Scene v2 (`lan_party_story`, wire 144, SMP4 version 14): `world` is the
  exterior, `temporary` the occupied interior, `inside_mask` the characters in
  it. An in-epoch temporary change is valid only while the exterior stays
  occupied on both sides; whole-party travel still takes a new epoch.
- Observer split callbacks keep the epoch, mark only the lead inside, hold the
  last READY scene during the native transition and treat the alternating
  current descriptor as the same party.
- `lan_story_split` joins the exterior keep-live adapter and wraps
  `SetModeLeadOnly`/`SetModeFullParty` so outside characters are exempted
  from the script's follower suspension with balanced native node resume/pause.
  Lead-only adds one reference per follower in the party-slot loop and one
  more in the global character-list loop; the wrapper samples each outside
  actor's counter before the native call, removes exactly the added count and
  restores it before full-party releases (`CONFIRMED_LIVE`: baseline 1 on a
  remote-controlled follower, added 1, released back to baseline).
- `lan_story_split` also owns the main world scene animation walk call
  (`CALL 0x5D4820` at RVA `0xA662`). After the native walk it runs the native
  per-object update (`0x5D64D0`, thiscall, `ret 4`) with the same dt for the
  host capture catalog and the outside party characters. Lead-only hides the
  follower's model object (object `+0x34` bit `0x04`) and the foreground switch
  re-homes party objects into the interior's cells, while the exterior's PVS
  cells are not active for the interior camera, so no native walker visits
  outside actors: their clip clock stops and with it the walk-cycle root
  motion that moves them. The native frame stamp (object `+0x32` against
  `0x7C3150`) prevents double updates; bit `0x20` (update while hidden) is
  raised only for the duration of the call. `CONFIRMED_LIVE` (live-mp24): the
  outside player walks normally and exterior NPCs resume. The animation
  renderer itself was shown not to be camera-gated: during a split the host
  renderer update receives real dt for every visited model.
- Host world capture freezes the exterior catalog at split start; clients
  compare their own character's area and leave other-area characters' poses.
- Control fences stay valid across same-epoch revisions; the session keeps
  control/acknowledgement on same-epoch scene changes; host binding, native
  control scope, observer native identity and the cast-light/spirit owners
  compare the epoch rather than revision/descriptor.

Presentation cadence and client cost (`CONFIRMED_LIVE`, two-window loopback):
the host captures party/world snapshots on every controller tick (~21 ms;
previously a 33 ms gate on that tick captured every second tick, 42-63 ms
steps at the edge of the client's 50 ms interpolation window) and clients send
movement on every frame. The host's input freshness uses a signed age: the
transport receipt stamp may be newer than the tick stamp, and the earlier
unsigned comparison zeroed movement for one tick, which the native animation
controller showed as an idle pop between run cycles. Client presentation cost
per applied world frame fell from about 29 ms to 11 ms through a per-traversal
memory-region cache in the world and client validators (every range is still
checked against a committed region; repeated queries within one traversal are
not repeated) and a pointer-chain recheck between the admitted setters of one
target, with the full roster/scheduler/registry proof kept at each target's
start and in the post-write readback. A world snapshot is about 6.7 KB for 27
actors, so every-tick capture is roughly 300 KB/s per client; acceptable on a
LAN, unmeasured over VPN.

Research diagnostics (bounded, log-only: clip/pose/input transition logs, the
client phase profile, the split animate summary and the hitch detector) are on
by default and switched off with `[StoryAreas] ResearchDiagnostics=false`;
the animation renderer trace stays opt-in through `[StoryAreas] AnimTrace`.

Client ambient animation (CONFIRMED_LIVE, 2026-10-06, Ailish host / Tal
client, New Brightwater): the contained client holds the native full pause, so
the frame dispatcher (RVA 0xA5B0) hands every scene update a zero delta.
Zone-placed props (river, waterfall, fountain, chimes, leaves) are scene
objects without a registry entity, so no replicated pose reaches them and the
animation renderer update (RVA 0x222B50) never advances them; the harbour sea
is the zone renderer's one sub-object (class vtable RVA 0x2DF7A4) whose update
(RVA 0x2171B0) only accumulates two phase values from the delta. The
client-only adapter `src/hooks/lan_story_ambient.c` owns both entries and,
while the runtime renews its lease after each successful presentation,
replaces a zero incoming delta with a bounded local frame delta (cap 50 ms,
zero after a 250 ms gap). Renderers owned by registry entities (the entity
CPosition render wrapper and the party-character model banks, enumerated by
`SudekiMpLanStoryWorldOwnedRenderers`) keep the native zero delta, so
presentation stays authoritative; the host never substitutes. Owner confirmed
harbour, river and fountain motion on the client with NPCs unchanged. Disabled
with `[StoryAreas] AmbientAnimation=false`; mutually exclusive with the
AnimTrace probe, which uses the same entry bytes. Other zones' water classes
are unverified, and the UI scene also receives the cosmetic delta.

Reload/recharge/weapon-swap freeze (CONFIRMED_LIVE, 2026-10-06, Ailish host /
Tal client): Ailish's first-person channels carry semantics the first-person
to world bridge does not map (193–195) during those actions, and one refused
actor used to fail the whole world capture, so no world frame left the host
for the duration and the client's entire world stopped. Capture now keeps a
ranged-attached party character's last published pose, refreshed with the
native position and heading, while its projection is refused
(`ranged_pose_fallback`), and sends the frame; the client no longer freezes.
Ailish's own reload/recharge/swap animation is not yet translated for the
client, so she holds her last pose for the action.

Known limits: only the host lead can use a door; remote players cannot enter.
The client still tolerates empty channels per channel for actors the host did
not advance. Other-area characters remain visible at their last pose. The host
frame time rises while both areas simulate. The exterior collision terrain
stays enabled while the interior is current, so overlapping interior/exterior
coordinates are a known risk until collision queries are scoped per area.
Later entrants, client doors, disconnect during split and multiple interiors
are untested.

The maintained evidence in `research-log.md`, "2026-08-26 — Roster
participation and party-atomic TEMP transitions", establishes that vanilla
deactivates the exterior and suspends followers. Its historical whole-party
formation solution is not the behavior selected here.

Before enabling this policy in story runtime:

1. Establish exact ownership of both area descriptors, scene/collision
   registration, update scheduling, resource tasks and authored initialization.
   Identify every global-current-area dependency on these paths. Repeating a
   native activate call or clearing pause counters is not a valid substitute.
2. Use the existing story observer's hook ownership, not a competing transition
   detour. Separate native area observations from the present single global
   scene/roster epoch. Keep actor-specific placement and exterior return context.
3. Connect authenticated player-specific doorway requests and area-scoped
   state. Preserve host consequence authority and unchanged Test Room behavior.
   A client camera or received destination name never authorizes story scripts.
4. Complete client resource enrollment, allowed presentation initialization,
   camera ownership and positive loading/retirement witnesses. Do not blindly
   recapture its registry or globally unpause client native gameplay.
5. Preserve unrelated area updates while a player loads. Measure loader stalls
   and CPU/memory costs; merely presenting stale snapshots is not a live area.
6. Exercise failure, duplicate/reordered messages, disconnect, view cleanup,
   area reuse and repeated entry/exit without erasing retained dependencies.

The existing source epoch guard remains intact until this ownership path can
replace it safely. No native completion is inferred from elapsed time.

## Acceptance gate

On the supported executable and matching saved-story build, verify Ailish
entering while Tal stays outside, then reverse which player enters first.
The outside player must keep moving against valid terrain/collision and receive
advancing world activity. The second player can enter the same occupied room;
either can exit independently. Repeat both directions and join/leave ordering.
Verify correct authored placement/camera, no cross-area actors/effects/audio,
no shared pause or catch-up speed burst, and no reset/duplication of story
effects or rewards. Also exercise loading failure and disconnect during each
native phase. Record actual native and visual results separately from policy,
protocol and exact-image tests. Until these gates pass, #24/#25 remain open.
