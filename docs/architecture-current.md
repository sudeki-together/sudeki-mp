# Current architecture map

This document owns only the cross-system map: entry points, component boundaries,
runtime flow, state, threading, and persistence. It does not own feature status,
native reverse-engineering details, cleanroom instructions, LAN operator steps,
or the roadmap.

Detailed existing sources remain canonical for their domains:

- executable/image facts: `docs/executable.md`
- native functions and structures: `docs/engine-functions.md` and
  `docs/structures.md`
- combat/native tasks: `docs/combat.md`
- cleanroom behavior: `docs/cleanroom.md`
- LAN operation and current slice: `docs/lan-arena.md`
- cross-process combat proof edges: `docs/lan-arena-combat-graph.md`
- chronological findings/corrections: `docs/research-log.md`

Audit baseline: commit `b957afee197d` plus an uncommitted
LAN/skill/lifecycle working tree inspected on 2026-09-04. Dirty behavior is not
considered proven.

## Evidence labels

- **PROVEN:** directly supported by repository source, configuration, history,
  or maintained evidence.
- **INFERENCE:** best architectural interpretation of proven facts.
- **UNKNOWN:** unresolved evidence or human decision.

## Top-level flow

```text
profile/script or graphical launcher
        |
        v
raw PE32 launcher -- validate -> create suspended process -> inject DLL
        |
        v
SudekiMP_Initialize -- validate image/config -> initialize ABIs -> install hooks
        |
        v
native Sudeki game loop
  | engine/policy modules
  | exact-image hook/adaptation layer
  | cleanroom OR local-co-op OR LAN profile
  v
reverse-order teardown / retained quarantine on unproven restoration
```

**PROVEN:** The project supports one exact GOG PE32/x86 executable. Both launch
and runtime layers validate the selected image. Runtime patches are injected;
the executable is not patched on disk.

## Major components

| Component | Existing responsibility | Detailed source |
| --- | --- | --- |
| `src/loader/` | Raw injector, DLL entry/composition, configuration/profile validation, exported initialization | `docs/mod-loader.md` and source |
| `src/launcher/` | Graphical profile/launch/log/update-check UI | README and launcher source |
| `src/engine/` | Executable identity, policy/state models, combat/roster/view/economy foundations, narrow native ABIs | reverse-engineering index |
| `src/hooks/` | Exact-image call/pointer/byte/inline/vtable adapters and feature integration | runtime lifecycle and source/tests |
| `src/cleanroom/` | `testroom`, F8 research UI, native actor/dummy/tools integration | `docs/cleanroom.md` |
| `src/input/` | Bindings, local controller bridge protocol/receiver/hub | source and focused tests |
| `src/network/` | LAN wire/session/authority/shared-simulation/replica/interpolation policy | protocol contract and tests |
| `src/tools/` | Native helper/operator programs | source/tool docs |
| `tests/` | Standalone pure, protocol, hook, exact-image, and integration executables | `docs/testing.md` |
| `tools/` | Build/launch/loopback/package/Ghidra/research orchestration | build docs and scripts |

**PROVEN:** Several large integration units remain process-global C modules.
Directory modularity does not imply independently unloadable services.

## DLL initialization and hook layer

`src/loader/dllmain.c` is the composition root. It reads configuration, validates
closed profile combinations, initializes required native ABIs, and installs the
selected adapters.

Hooks capture original callbacks/state and are expected to verify image bytes,
targets, owners, vtables, and calling conventions. Feature admission then depends
on exact native object and generation leases.

See `runtime-lifecycle.md` for the durable safety contract. The dirty tree is
actively hardening failure-atomic uninstall; consult `status-matrix.md` before
claiming that every module satisfies that contract.

## Runtime state

**PROVEN:** State is primarily in process-global C structures:

- hook records and original callbacks;
- native object pointers plus actor/component/view/input generations;
- profile, role, and session state;
- snapshot/interpolation/presentation state;
- retained teardown/quarantine state;
- sidecar-derived configuration.

Readable memory alone is not treated as ownership. Mutating adapters revalidate
the relevant actor, component, session, and view lease.

## Cleanroom profile

The cleanroom launches Sudeki's shipped `testroom` and exposes bounded research
tools through native engine paths. It is save-free and exact-build gated.

This architecture map intentionally does not reproduce menu rows, spawn details,
cheat semantics, or validation history. Those belong in `docs/cleanroom.md` and
the status/evidence indexes.

## Local co-op profile

Local co-op runs multiple seats in one process:

```text
local inputs -> input hub -> actor-scoped control lease
                           -> native movement/action admission
native frames -> seat camera/cache ownership -> split composition/HUD adaptation
```

Control separation leases a companion from native AI; render/camera systems own
seat views; teardown removes presentation ownership before restoring AI.

**PROVEN:** Two-player components and a separate fixed-three/custom-menu
experiment exist. A complete campaign-safe local-co-op architecture is not
established. Status belongs in `status-matrix.md`.

## LAN profile

LAN arena runs two full-screen Sudeki processes and remains separate from local
split-screen profiles.

```text
Tal input ----\
               canonical host reducer -> native world consequences -> snapshots
Ailish input -/                                      |
                                                    v
                                      client interpolation/presentation
```

The current profile places the canonical native-world node in Tal's process and
the replica node in Ailish's process. Player identity, actor input authority,
simulation-node authority, actor presentation, and world-consequence authority
are represented as distinct domains.

The host admits actor-scoped contributions, runs canonical native gameplay
consequences, and publishes bounded frames. The client renders authenticated
newer frames and may run contained native presentation work without gaining
damage/resource/world authority.

Wire/session/timeout behavior and operator commands remain in
`docs/lan-arena.md`; action proof edges remain in
`docs/lan-arena-combat-graph.md`. Field-level truth remains in protocol source
and focused tests.

**INFERENCE:** The authority abstraction could later support a dedicated
canonical process. No dedicated server exists now.

## Threading boundary

**PROVEN:** Socket and controller workers may validate and queue bounded plain
data but must not dereference Sudeki objects. Native actor, camera, renderer,
skill, inventory, and world work occurs on verified game/render-thread seams.

Callbacks, timers, workers, and native async tasks may outlive their initiating
request; their code/object dependencies must remain available until positive
completion and restoration.

## Persistence boundary

Observed persistent classes are:

- runtime INI configuration;
- local roster sidecar;
- launcher preferences in platform application data;
- selected versioned/checksummed experimental sidecars;
- logs/evidence artifacts;
- native saves outside current LAN cleanroom authority.

Native pointers, hooks, session tokens, task leases, and actor generations are
ephemeral and must not be serialized. The current LAN cleanroom profile does not
read, write, copy, or transfer campaign saves.

## Testing architecture

The repository defines many standalone C test executables. It does not register
a canonical CTest suite at the audit baseline. Build scripts, individual Wine
commands, loopback harnesses, exact-image fixtures, selected Windows CI steps,
and live sessions provide different proof layers.

The proof boundaries and reporting format belong in `testing.md`.

## Unknown decisions

- **UNKNOWN:** Primary product direction: local co-op, LAN arena, or campaign
  multiplayer.
- **UNKNOWN:** Final canonical authority placement and trust model.
- **UNKNOWN:** Whether explicit DLL unload is supported.
- **UNKNOWN:** Public compatibility promises for executable, protocol, package,
  and sidecar versions.
- **UNKNOWN:** Public/legal policy for tracked native save fixtures.
