---
name: sudekimp-research
description: Continue, debug, test, document, or review the SudekiMP reverse-engineering and local co-op project. Use for work involving the exact GOG Sudeki build, Wine/Ghidra research, runtime hooks, cleanroom tools, split-screen cameras and HUD, independent player input, Skill Strikes, Spirit Strikes, combat timing, save baselines, or SudekiForge handoff decisions.
---

# SudekiMP Research

Resume SudekiMP from evidence instead of rediscovering the engine or guessing at offsets. Keep experiments exact-build-gated, reversible, documented, and narrow.

## Resume the project

1. Work from the repository root. Read `RECOVER.txt`, `README.md`, `docs/milestones.md`, and recent `git log --oneline` plus `git status --short` before changing anything.
2. Read [references/project-map.md](references/project-map.md) and load only the project documents relevant to the requested subsystem.
3. If `graphify-out/graph.json` exists, run `graphify query "<current task>"` before broad source searches. Use `graphify explain` for one concept and `graphify path` for a suspected relationship.
4. Treat the dirty worktree as user-owned. Do not reset, discard, or overwrite unrelated work.
5. Identify the current claim as confirmed, strong hypothesis, open hypothesis, or rejected experiment before designing the next pass.

## Execute one research pass

1. State one concrete question and one acceptance test.
2. Prefer static inspection and existing logs before adding a hook. Use `rg` for source navigation and query the graph first when available.
3. Gate every live hook on the supported executable hash and the smallest useful byte signature or exact native object/vtable identity.
4. Preserve native behavior wherever possible: use Sudeki's existing managers, control APIs, animation events, resources, and cleanup paths instead of recreating gameplay logic.
5. Make the change disabled by default. Focused launch modes may enable it temporarily and must restore generated configuration afterward.
6. Build and run proportionate automated checks. Use [references/runtime-and-validation.md](references/runtime-and-validation.md) for canonical commands and environment boundaries.
7. Launch Sudeki only when the user is available for a visual or input-dependent acceptance test. The agent may inspect logs, hashes, disassembly, and automated test output without the user.
8. Stop after the stated fact is established or falsified. Do not roll an unconfirmed pass into a broader architecture change.

## Record evidence

For every reverse-engineered result, record the exact executable SHA256, RVA, useful signature, function/global/structure name, discovery method, confirming experiment, related callers or fields, confidence, and whether the experiment is current or rejected.

Keep confirmed facts separate from hypotheses. A crash, stall, wrong pose, or visual corruption is evidence; retain its cause and rejection rationale instead of presenting the pass as progress that worked.

Update the narrow subsystem document and `docs/research-log.md`. Update `docs/milestones.md`, `README.md`, and `RECOVER.txt` only when their checkpoint summaries actually changed. Run `graphify update .` after source changes when a graph exists.

## Project boundaries

- Never add Sudeki installers, executables, archives, extracted assets, saves, or copyrighted game data to Git.
- Never patch an unknown executable or edit the only clean game installation.
- Keep the vanilla tree read-only and use the dedicated working installation and Wine prefix.
- Do not implement online networking before stable local co-op ownership and simulation behavior exist.
- Treat shared SSP, scene actors, transitions, temporal effects, cameras, HUD, and target-selection state as global until evidence proves they are per-player.
- For multiplayer presentation, distinguish simulation ownership, gameplay camera ownership, render-camera state, viewport composition, model presentation, HUD ownership, and post-effect history. They are separate engine boundaries.

## Handoff standard

End a work session with: what changed, automated checks, live result or pending user action, exact next experiment, known risks, and the safe resume command. Do not claim a visual acceptance test from logs alone.
