# Testing and evidence hierarchy

This document owns the test/evidence boundary and result-reporting contract. It
does not duplicate individual test cases, build instructions, or live research
logs. SudekiMP tests undocumented native behavior at several distinct levels; a
claim must state which level was actually reached.

## 1. Build/compile checks

Examples include compiling the DLL, launchers, helpers, and test executables.

They prove:

- source and declared dependencies compile for the selected toolchain;
- link-time symbols and calling-convention declarations are internally usable;
- selected static warnings are absent.

They do **not** prove:

- that a hook matches the retail executable;
- that the native ABI interpretation is correct at runtime;
- that the game launches;
- that animation, camera, input, damage, or teardown looks/behaves correctly.

## 2. Pure and policy tests

These exercise modules without a live game image: state machines, authority
rules, layouts, roster policy, handoff decisions, interpolation math, and other
bounded logic.

They prove deterministic behavior for the tested inputs and invariants.

They do **not** prove that native pointers, callsites, objects, timing, or thread
boundaries match the game.

## 3. Protocol tests

These cover packet validation, encode/decode, versioning, sequencing, malformed
input, authority fields, action journals, snapshot rules, and interpolation.

They prove the tested wire contract between project modules.

They do **not** prove:

- socket behavior under a real network;
- native host admission or client presentation;
- cryptographic authenticity or Internet safety;
- that a structurally valid native selector/state is safe to apply.

## 4. Synthetic hook tests

These construct controlled mapped memory/callsites and exercise hook install,
redirect, ordering, rollback, restoration, and retry behavior.

They prove the generic or feature-specific hook policy under the synthetic
fixture.

They do **not** prove that the expected bytes or calling context exist in the
supported retail executable.

## 5. Exact supported-image tests

These map or inspect the user-supplied supported `SUDEKI.exe` and verify exact
bytes, targets, exports, vtables, signatures, install scope, and restoration.

They prove that the tested hook/native seam matches that exact image and that
the tested install/restore procedure works without executing normal gameplay.

They do **not** prove:

- that the hook is reached in the expected gameplay state;
- native object lifetime across a real frame or transition;
- correct visible animation, camera, audio, UI, collision, or combat;
- safe disconnect or process teardown during an asynchronous task.

Exact-image hook installation is not live gameplay proof.

## 6. Loopback/integration tests

LAN and controller harnesses may start helpers or two isolated processes and
exercise transport/session integration.

They can prove bounded end-to-end message flow, role/session setup, cleanup,
and selected state convergence in the tested environment.

They do **not** automatically prove two-machine behavior, adverse-network
behavior, presentation quality, or campaign safety.

## 7. Live in-game acceptance

A live proof records an exact revision/profile/environment and a human-observed
or instrumented gameplay outcome.

A useful live record includes:

- clean or dirty revision identity;
- executable identity and protocol/profile;
- participants/actors/map;
- exact action sequence;
- expected and observed result;
- relevant logs/screenshots/video identifiers;
- timeout, disconnect, teardown, or retry behavior;
- limitations and negative observations.

Live proof is bounded. “Ailish moved once in testroom” does not prove campaign
movement, another actor bank, reconnect, or teardown.

## Actual execution mechanisms

The repository uses CMake to define many standalone test executables. No CTest
`add_test` registration was observed at the audit baseline, so `ctest` is not
the canonical runner.

Tests are invoked through combinations of:

- Linux cross-build scripts;
- individual Wine commands;
- shell loopback/launcher harnesses;
- native Windows build scripts;
- a selective, manually triggered Windows forge workflow;
- exact-image tests requiring a legitimate user-supplied executable;
- explicit live game sessions.

Do not say “the test suite passed” unless the exact command list is recorded.

## Evidence result format

For every claimed verification, record:

```text
Revision: <commit and dirty-tree identifier>
Profile: <closed launch/config profile>
Environment class: <Windows/Wine; no private machine identity>
Command/test: <exact reproducible command or target>
Expected: <bounded expected result>
Observed: <bounded observed result>
Evidence level: <category from evidence-index.md>
Artifacts: <sanitized repository evidence or private local evidence ID>
Limitations: <untested cases and known failures>
```

## Merge and release gates

> **HUMAN DECISION REQUIRED:** The repository does not currently establish one
> mandatory, comprehensive merge gate.

The owner should define separate minimum gates for:

- pure/policy-only changes;
- protocol changes;
- generic hook changes;
- exact-image adapter changes;
- local co-op runtime changes;
- LAN runtime/lifecycle changes;
- launcher/package changes;
- public release candidates.

Until that decision exists, agents must report exactly what they ran and must
not convert a selected green subset into a global “all tests pass” claim.

## Recommended status update rule

Update `status-matrix.md` only after the evidence record exists. Preserve the
lowest status when:

- the code is dirty/uncommitted;
- exact-image coverage is missing;
- a live behavior is only inferred from logs;
- rollback/disconnect has not been exercised;
- evidence from two documents conflicts.
