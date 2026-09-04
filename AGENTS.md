# SudekiMP agent map

This file is a durable navigation and safety contract for coding agents. It is
not a progress report and must not be used to record the current debugging
session.

## What this project is

SudekiMP is an exact-build, injected native-code mod and reverse-engineering
project for the 32-bit Windows release of *Sudeki*. It experiments with local
co-op, a cleanroom test arena, and direct-IP LAN multiplayer while preserving
and adapting the game's native systems wherever those systems are understood.

It is an active research prototype. Source presence is not proof that a feature
works, and an experimental launch profile is not a supported public release.

## Canonical and optional context

Read the smallest relevant set:

1. `docs/status-matrix.md` — canonical feature status and proof level.
2. `docs/architecture-current.md` — architecture that exists now.
3. `docs/runtime-lifecycle.md` — hook, object-lifetime, and teardown rules.
4. `docs/testing.md` — what each kind of test does and does not prove.
5. `docs/evidence-index.md` — claim vocabulary and evidence citation rules.

Use the existing detailed documents for their narrower domains:
`docs/executable.md`, `docs/engine-functions.md`, `docs/structures.md`,
`docs/combat.md`, `docs/cleanroom.md`, `docs/lan-arena.md`, and
`docs/lan-arena-combat-graph.md`. If an owner-approved roadmap is adopted later,
only that owner-populated document may define priority; never infer
implementation from it.

The large chronological `docs/research-log.md` remains the underlying evidence
ledger. Consult it through the evidence/status documents and relevant headings;
do not duplicate it into summary documents.

If `.agent-local/active-session.md` exists, it is optional private context for
the current machine/session only. The repository must remain understandable
without it. Never copy `.agent-local/` contents into tracked documentation,
issues, logs, or commits without explicit human approval and sanitization.

## Historical documents

`RECOVER.txt`, `docs/project-status.md`, `docs/milestones.md`, scoped handoff
documents, and old design documents record useful history. They are not
automatically current requirements. Prefer a newer canonical summary, then
follow its links to historical evidence. Date alone does not establish truth:
a newer unverified claim may be weaker than an older exact-image or live proof.

## Reading order by task

- Native hook or executable research: lifecycle, testing, executable,
  engine-functions/structures, then the exact subsystem evidence and source.
- LAN protocol or replica work: architecture, status matrix, testing,
  `docs/lan-arena.md`, LAN combat graph, then source.
- Local co-op or rendering: architecture, status matrix, lifecycle, cleanroom or
  local-co-op evidence, then source.
- Cleanroom feature: status matrix, cleanroom guide, testing, then native
  engine references.
- Roadmap or product-scope question: consult an owner-populated roadmap if one
  exists. Otherwise ask the project owner rather than deriving strategy from old
  milestones.

## Graphify workflow

This project has a local knowledge graph under `graphify-out/` when generated.
When the user types `/graphify`, use the installed Graphify skill or repository
instructions before doing anything else.

Preserve the existing repository Graphify workflow:

- For codebase questions, first run `graphify query "<question>"` when
  `graphify-out/graph.json` exists. Prefer its scoped subgraph over loading the
  complete report or starting with broad text searches.
- Use `graphify path "<A>" "<B>"` for relationships and
  `graphify explain "<concept>"` for focused concepts.
- If `graphify-out/wiki/index.md` exists, use it for broad navigation before raw
  source browsing.
- Read `graphify-out/GRAPH_REPORT.md` only for broad architecture review or when
  scoped query/path/explain results are insufficient.
- Dirty/generated graph files are expected and are not proof that source is
  wrong or a reason to skip an available graph.
- Skip Graphify only when the task concerns stale/incorrect graph output or the
  user explicitly asks not to use it.
- After an authorized code change, run `graphify update .` to keep the graph
  current (AST-only, no API cost). Do not run it during an inspection-only task.

The generated graph is a navigation index, not authority. Confirm important
results in source, focused tests, or maintained evidence documents.

## Evidence vocabulary

Use only the categories defined in `docs/evidence-index.md`:

- `CONFIRMED_STATIC`
- `CONFIRMED_EXACT_IMAGE`
- `CONFIRMED_TEST`
- `CONFIRMED_LIVE`
- `INFERENCE`
- `HYPOTHESIS`
- `RETIRED`
- `UNKNOWN`

Never upgrade a claim without recording the evidence. In particular:

- Source presence is not proof that a feature works.
- Compilation is not live gameplay proof.
- A pure test is not proof that a native hook installs.
- Exact-image hook installation is not live gameplay proof.
- A log proving that a call occurred is not necessarily visual or gameplay proof.
- Historical documents are not automatically current requirements.
- Use `UNKNOWN` rather than inventing a missing project decision.

## Exact-build safety policy

- Support only executable identities explicitly listed in the compatibility and
  executable documentation.
- Never install a hook on an unknown image, target, instruction sequence,
  vtable, pointer owner, or calling convention.
- Treat a new executable build as a separate research and validation project.
- Do not weaken validation merely to make an experiment launch.
- Do not reproduce or commit proprietary executable contents.

## Working-tree policy

- Inspect `git status` before work.
- Existing tracked and untracked changes belong to the user or another active
  task unless ownership is explicitly established.
- Do not reset, clean, overwrite, reformat, or silently incorporate unrelated
  changes.
- Coordinate file ownership before editing shared monolithic modules.
- Record ephemeral branch, dirty-file, live-test, and next-action state in
  optional `.agent-local/active-session.md`, never in this file.
- Dirty working-tree code is `UNVERIFIED_DIRTY_WORK` until its stated tests and
  acceptance evidence are recorded.

## Native hook and object rules

- One adapter owns each native patch seam. Share observers or callbacks rather
  than detouring the same call twice.
- Verify expected bytes, call targets, pointer ownership, ABI, and supported
  image before installation.
- Install transactionally. Roll back in reverse order.
- Do not clear original callbacks, trampolines, object leases, or module state
  while any live callsite may still enter them.
- A failed restoration is not successful teardown. Retain the dependencies,
  quarantine or retry, and report the failure.
- Readable native memory is not proof of identity. Require the applicable actor,
  component, view, input, and generation lease.
- Observation failure is unknown, not inactive.
- Do not force-cancel an asynchronous native task without a verified native
  cancellation contract. Drain it through a positively observed terminal state.

## Threading rules

- Socket or controller transport workers may parse, validate, sequence, and
  queue plain data. They must not dereference or mutate Sudeki objects.
- Native objects are observed and changed only on verified game/render-thread
  seams appropriate to that object.
- Callback lifetime must outlive every installed hook, timer, worker, or native
  task that can call it.
- Keep local-controller transport separate from LAN gameplay transport.

## Public and private context

Public documentation may contain project architecture, exact-build technical
research, sanitized evidence, and reproducible test procedures. It must not
contain credentials, private addresses or hostnames, VPN topology, runner
identity, personal paths, machine-specific prefixes, raw session tokens, user
saves, dumps, or proprietary game material.

Machine configuration, raw logs, packet captures, recordings, and active-agent
handoffs belong in optional, Git-ignored `.agent-local/`. Normal contributors
may not have this directory, and no public instruction may depend on it. Publish
only sanitized conclusions tied to an evidence record, and only after human
approval to move information out of `.agent-local/`.

## Before claiming that something works

State all of the following:

1. Exact feature and launch profile.
2. Exact code revision and whether the tree was dirty.
3. Evidence level reached.
4. Tests or live actions actually performed.
5. Expected result and observed result.
6. Unsupported cases and known failures.
7. Whether teardown/retry was exercised where relevant.

Do not write “fixed,” “working,” or “supported” when the strongest result is
only compilation or hook installation.

## Stop for a human decision

Stop and ask the project owner when work would require choosing or changing:

- the primary product direction or roadmap priority;
- the network trust or authority model;
- support for another executable build;
- a public compatibility or save-format promise;
- distribution of game-derived assets or save fixtures;
- a breaking protocol or persistence migration;
- ownership of a native global system not settled by evidence;
- an explicit-DLL-unload policy;
- a destructive Git or environment operation;
- a tradeoff that changes accepted gameplay semantics rather than implementing
  an already documented decision.

When the repository cannot answer, record `UNKNOWN` and request the decision.
