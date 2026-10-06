---
name: graph-findings
description: Record and recall pertinent SudekiMP findings in the project's Graphify graph. Use at the start of any native/engine investigation (to recall prior findings and dead ends) and whenever a session establishes, rules out, or corrects something another session would otherwise re-derive — native function semantics, ABI/calling conventions, data layouts, pause/lifetime/ownership behavior, live-probe results, or dead ends.
---

# Graph findings

Findings live in Graphify's work memory: `graphify-out/memory/*.md`, written by
`graphify save-result` and ingested into `graphify-out/graph.json` by the
AST-only `graphify update .`. `graphify-out/` is git-ignored, so findings stay
on this machine. The graph is navigation, not authority (AGENTS.md).

## Before investigating

1. `graphify query "<question in project vocabulary>"` — memory nodes appear
   as `Q: ...` with `src=graphify-out/memory/...`. Read the matching file.
2. `grep -il "<rva or symbol>" graphify-out/memory/*.md` for exact addresses
   the tokenizer may miss.
3. Treat a recalled finding as a lead: re-verify against source, the exact
   image, or a test before relying on it. Prefer `corrected` entries over the
   older answer they correct; skip anything recorded as `dead_end` unless you
   have new evidence.

## When to record

Record when the result is pertinent beyond this conversation:

- Verified native semantics: what a function/seam does, its callers, calling
  convention, register/stack contract, or exclusive-caller facts.
- Structure layouts and global identities that were confirmed.
- Live-probe or exact-image results, including negative ones.
- Hypotheses ruled out (`dead_end`) and earlier answers found wrong
  (`corrected`).

Do not record routine progress, build hashes, temporary paths, or anything
already captured verbatim in tracked docs — link the doc instead.

## How to record

```bash
python3 -m graphify save-result --type query \
  --outcome useful|dead_end|corrected [--correction "what is actually true"] \
  --question "<the question a future session would ask>" \
  --answer "<finding: mechanism, exact RVAs/offsets, evidence level, how verified, what is NOT shown, source files>" \
  --nodes "<existing graph node labels you cite>"
graphify update .
```

Then confirm with a `graphify query` that the `Q:` node is returned.

Answer contents:

- Use the evidence vocabulary from `docs/evidence-index.md`
  (`CONFIRMED_STATIC`, `CONFIRMED_EXACT_IMAGE`, `CONFIRMED_TEST`,
  `CONFIRMED_LIVE`, `INFERENCE`, `HYPOTHESIS`, `RETIRED`, `UNKNOWN`). State
  what was and was not shown. Never upgrade a claim beyond its evidence.
- Use supported-image RVAs/VAs and repository-relative source paths.
- Cite node labels that exist in the graph (check with `graphify explain`).

Never put in an answer: credentials, private addresses/hostnames, personal or
machine paths, `.agent-local/` contents or raw logs, user saves, or
proprietary executable bytes. Sanitize to conclusions.

## Limitations

- `graphify reflect` / `LESSONS.md` is not available in the installed
  version; read the memory files directly.
- The Graphify skill itself is not installed for Claude Code here; use the CLI
  workflow in AGENTS.md (`query`, `path`, `explain`, `update .`).
