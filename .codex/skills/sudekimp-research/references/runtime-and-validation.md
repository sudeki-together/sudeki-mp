# Runtime and validation

## Environment

- Repository: `/var/home/wander/Documents/Projects/sudeki-mp`
- Dedicated Wine prefix and user-supplied game files live outside Git.
- Ghidra analysis is external to the repository; `tools/ghidra/` contains original repeatable report scripts only.
- The live game is a 32-bit Windows process under Wine. Preserve x86 calling conventions, callee stack cleanup, register arguments, reference counts, and vtable identity exactly.

## Start with read-only checks

```bash
git status --short
git log -8 --oneline --decorate
tools/continue-research.sh --check
```

Use `graphify query`, `graphify path`, or `graphify explain` before broad browsing when `graphify-out/graph.json` exists.

## Build and automated validation

```bash
tools/build-linux.sh
git diff --check
```

Run the smallest relevant repository-built Wine test and exact supported-image installation/restoration regression. Do not substitute a successful build for a live visual result.

## Focused live modes

Use `tools/continue-research.sh --safe` for an unmodified research launch. Use only the focused mode documented for the active subsystem, such as `--cleanroom`, `--controller-bridge-test`, or `--realtime-skill-coop-test`. The helper owns temporary config changes and should restore disabled defaults on exit.

Do not relaunch repeatedly while the user is distracted or unavailable. Before a visual test, give a short checklist of exactly what to do and observe. After the user closes the game, inspect the relevant log before changing code.

If the game stalls or becomes unsafe to interact with, use:

```bash
tools/stop-sudeki.sh
```

## Evidence labels

- **Confirmed:** exact static or runtime evidence directly establishes the fact.
- **Strong hypothesis:** multiple observations support it, but the decisive test is missing.
- **Open hypothesis:** plausible research direction with incomplete evidence.
- **Rejected:** tested behavior failed acceptance, corrupted state, crashed, stalled, or was superseded.

Never silently reuse a rejected approach. Explain what new evidence makes a revised pass materially different.
