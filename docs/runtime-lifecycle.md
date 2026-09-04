# Runtime lifecycle and native safety

This document owns the cross-module native-lifetime safety contract: hook
ownership, object leases, task drain, and teardown. It points to implementations
and tests for detail and does not replace `docs/mod-loader.md`, native subsystem
research, or the chronological research log. It is not a claim that every
existing module already satisfies every rule.

## Status vocabulary

- **PROVEN RULE:** repeated in code and/or maintained project documentation.
- **ACTIVE RESEARCH:** current implementation is being hardened or the native
  contract is incomplete.
- **UNKNOWN:** a human decision or additional native evidence is required.

## 1. Patch and hook ownership

**PROVEN RULE:** A native callsite, inline sequence, vtable slot, export slot, or
global pointer must have one owning adapter. Other systems should consume an
observer/callback interface exposed by that owner rather than patching the same
seam independently.

Before installation, verify as applicable:

- exact supported executable identity;
- expected opcode bytes and relative-call target;
- pointer-slot or vtable ownership;
- calling convention and parameter ABI;
- module/image bounds;
- feature-profile compatibility;
- original callback storage published before the patched site can execute.

Unknown or conflicting ownership fails closed.

## 2. Installation transaction

The expected lifecycle is:

1. Resolve immutable supported-image evidence.
2. Initialize plain policy/state dependencies.
3. Capture original native state.
4. Install the narrowest hooks first-to-last in a documented order.
5. Publish runtime admission only after every required seam is ready.

If any step fails:

- close new admission;
- restore every attempted installation in reverse order;
- attempt every independent restoration rather than short-circuiting after the
  first error;
- preserve the first meaningful error;
- do not clear callbacks or dependencies for any hook that might remain live.

**ACTIVE RESEARCH:** The dirty working tree is converting multiple older
uninstall paths to checked, retryable teardown. The final cross-module contract
is not yet established as a clean merged baseline.

## 3. Restore, uninstall, and quarantine

A restoration request is successful only when the original owner/value/bytes
are positively restored.

On restoration failure:

- retain original callbacks and trampoline storage;
- retain module-base, object, and auxiliary-resource dependencies;
- retain hook bookkeeping needed for retry;
- reject reinstall/reconfiguration that would overlap the retained hook;
- quarantine or pin the runtime if unloading would invalidate a live callback;
- return/report failure to the caller rather than logging and continuing as if
  teardown completed.

Do not zero hook records, release textures/events/timers, or null function
pointers while a native callsite can still enter mod code.

**UNKNOWN:** Whether explicit `FreeLibrary`-style DLL unload is a supported
operation. Process detach cannot reliably veto an unload already in progress.
A supported explicit-unload policy would require a game-thread shutdown
handshake that proves hooks, timers, workers, actors, cameras, and native tasks
are drained before unmapping the DLL.

## 4. Native object leases

Readable memory is not identity. A native object may be replaced at the same
address or may outlive the role that made it safe to use.

A lease should preserve the identities needed by that subsystem, which may
include:

- character pointer and actor kind;
- component/controller/arbiter pointer;
- position, renderer, wrapper, or camera pointer;
- input source identity and generation;
- party/roster slot and role generation;
- session token and protocol generation;
- native task/skill pointer, slot, and sequence;
- scene or render-state owner.

Every mutation boundary revalidates the applicable lease. Pointer mismatch,
generation change, failed observation, foreign slot ownership, or unsupported
native state closes admission.

## 5. Actor control leases

Local and LAN control adapters temporarily take a party member out of native AI
control. The lease must bind the exact actor and input/session generation.

Teardown order is generally:

1. stop accepting input/actions;
2. quiesce movement and gameplay edges;
3. end or drain camera/presentation work;
4. remove seat-specific cameras/render ownership;
5. restore native AI/control in reverse acquisition order;
6. clear role/runtime publication.

If camera or task cleanup is not proven, retaining the actor lease is safer than
restoring AI or deleting the actor beneath a live callback/task.

## 6. Camera and view ownership

Camera state is both local presentation and a native-object lifetime concern.
A view lease may contain the prior camera basis, render state, scene slot, and
owner actor.

Rules supported by current work:

- capture before mutation;
- mutate only an owner-exact slot;
- reassert only while the actor/task/session lease remains exact;
- restore only if the current slot is still owned by that lease;
- a foreign slot value is `UNKNOWN/BUSY`, not permission to overwrite it;
- retain the saved basis if restoration fails so a later retry remains possible;
- release cameras before actor/AI teardown.

## 7. Asynchronous native tasks

Native skills, actions, scripts, timers, animation tasks, and projectiles may
continue after the input or network transaction that initiated them.

**PROVEN RULE:** A successful native start can create a lifetime obligation
before an observer reports the task active. Record a conservative start lease
from the returned native identity whenever available.

**PROVEN RULE:** Observation failure is not completion. Retire a task only after
a positive terminal observation for the exact actor/task/slot/sequence, or
through another separately verified native completion contract.

During authority loss or teardown:

- close new task admission immediately;
- retain the actor and required hooks/callbacks;
- continue containment of client-local gameplay consequences;
- keep required time/camera/presentation ownership;
- poll/drain on the game thread;
- release only after the last exact task is terminal and view/native state is
  restored.

Do not write task-private completion fields or invoke an assumed cancel routine
without exact native evidence.

**ACTIVE RESEARCH:** LAN Character-skill handoff, Spirit presentation, native Tal
action drain, ranged-prime timers, and host/client disconnect ordering are being
hardened in the current dirty tree.

## 8. Thread ownership

Transport threads may:

- receive/send packets;
- validate lengths and wire structure;
- perform sequence/session checks;
- timestamp and enqueue plain data;
- publish synchronized status.

Transport threads must not:

- dereference actor, renderer, camera, skill, inventory, or world pointers;
- call native game methods;
- remove actors or restore AI;
- write render or combat state.

Native work occurs only on a verified game/render-thread callback. Shared data
crossing the boundary must be bounded, synchronized, and free of native pointers.

## 9. Disconnect and timeout

The intended fail-closed sequence is:

1. invalidate network admission and stop new input;
2. discard unauthenticated/future wire frames;
3. retain and drain already-started native work;
4. contain local damage/resource/world consequences while draining;
5. restore view/combat/time state;
6. release remote actors/cameras/control leases;
7. clear replicas and return to a known cleanroom baseline;
8. permit reconnect/reinitialize only after successful cleanup.

No partially connected session should be preserved as a valid new authority
generation.

**ACTIVE RESEARCH:** The exact host and client task-drain gates and all
cross-module uninstall error propagation are not yet a merged, fully accepted
contract.

## 10. Persistence boundary

Native pointers, hook records, session tokens, actor generations, and task
leases are ephemeral process state and must not be serialized.

Persistent sidecars should be:

- pointer-free;
- versioned;
- bounded and validated;
- written only by their owning subsystem;
- restored independently from runtime native leases.

LAN cleanroom profiles must not mutate, copy, or transfer campaign saves.

## 11. Required lifecycle evidence

For a lifecycle-sensitive change, record:

- successful install and normal reverse uninstall;
- failure at each install stage and complete rollback;
- restore ownership mismatch and retry behavior;
- actor/input/view generation loss;
- disconnect during every admitted async task class;
- no native callback into cleared/unmapped state;
- reinitialize/reconnect only after cleanup;
- live teardown where native visual/gameplay behavior matters.

Compilation alone cannot establish any of these outcomes.
