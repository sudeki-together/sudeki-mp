# Evidence classification and claim index

This document owns claim classification and citation rules. It indexes evidence;
it does not replace `docs/research-log.md`, Ghidra reports, tests, raw artifacts,
or feature status. SudekiMP combines static reverse engineering, exact-image
adapters, pure tests, logs, and visual gameplay observations, and those sources
answer different questions.

## Categories

### `CONFIRMED_STATIC`

Use when a claim is established by reproducible inspection of the supported
executable, source, archive metadata, or disassembly.

Required citation:

- exact executable/build identity where native code is involved;
- symbol/RVA/structure field;
- Ghidra report, signature document, or source location;
- relevant bytes/call target described without reproducing proprietary binary
  contents beyond the project's established technical signature practice.

Does not prove runtime reachability, object lifetime, or visible behavior.

### `CONFIRMED_EXACT_IMAGE`

Use when a focused test against the user-supplied supported executable proves
the expected native seam, ownership checks, install behavior, and restoration
covered by that test.

Required citation:

- exact test target and revision;
- exact image identity;
- what mismatch and rollback cases were exercised.

Does not prove live gameplay behavior.

### `CONFIRMED_TEST`

Use when a deterministic pure, policy, protocol, synthetic-hook, or integration
test proves the bounded claim.

Required citation:

- test file/target;
- test case or asserted invariant;
- revision and command;
- whether the fixture is pure, synthetic, loopback, or exact-image.

Does not extend beyond tested inputs or establish native visual behavior.

### `CONFIRMED_LIVE`

Use when the exact behavior was observed in a real supported game process under
a recorded profile/revision.

Required citation:

- revision and dirty-tree state;
- profile, actors, map, protocol, and environment class;
- exact action and expected result;
- sanitized log/trace/screenshot/video evidence identifier;
- teardown/retry result where relevant;
- limitations of the observation.

A live proof remains scoped to that scenario. Visual proof and log-only proof
should be stated separately.

### `INFERENCE`

Use for the best explanation that joins confirmed facts but lacks a direct
proof. Cite every confirmed premise and state what observation would distinguish
the inference from alternatives.

Do not allow an inference to authorize a native write or public compatibility
claim.

### `HYPOTHESIS`

Use for a testable proposed explanation with incomplete supporting evidence.
Record the discriminating test. Do not describe it as “likely fixed” merely
because code was written around it.

### `RETIRED`

Use for a path deliberately disabled, superseded, or rejected. Record:

- the revision/path;
- the failure or architectural reason;
- the replacement, if any;
- the explicit condition required before reconsideration.

Code or documentation remaining in the tree does not reactivate it.

### `UNKNOWN`

Use when evidence or a human project decision is absent or conflicting.
`UNKNOWN` is the correct answer for undecided product direction, authority,
compatibility, threat model, legal policy, or native ownership.

## Relationship to feature status

Evidence and implementation state are separate axes:

- A `FOUNDATION` may be strongly `CONFIRMED_TEST`.
- An `EXPERIMENTAL` feature may have a narrow `CONFIRMED_LIVE` proof.
- An `IMPLEMENTED` module may still lack exact-image or live evidence.
- Dirty code is `UNVERIFIED DIRTY WORK` until the applicable evidence is rerun
  and recorded.

## Claim record format

Use this compact record in an index or a dedicated evidence section:

```text
Claim: <one bounded technical statement>
Classification: <one category above>
Feature/profile: <exact subsystem and closed profile>
Revision: <commit; add dirty identifier if applicable>
Supported image/protocol: <when relevant>
Primary evidence: <repository path + symbol/line/test/report>
Supporting evidence: <additional independent source>
Procedure: <reproducible static/test/live procedure>
Observed result: <what actually happened>
Does not prove: <explicit boundary>
Conflicts/corrections: <older claims or rejected alternatives>
Private artifacts: <local evidence ID only, never private path/data in public docs>
Last reviewed: <date and reviewer>
```

Do not assign a claim ID unless the claim has at least one concrete primary
evidence source. An ungrounded idea remains a hypothesis or roadmap question.

## Citation rules

Prefer stable, precise citations:

- repository-relative path plus function/symbol/test name;
- commit hash for historical source;
- RVA plus exact supported image identity for native code;
- focused test target and case name;
- research-log heading/date rather than a vague reference to the whole file;
- private artifact ID whose sanitized conclusion is recorded publicly.

Line numbers are useful during review but can drift. Pair them with a symbol,
heading, test name, or commit.

## Resolving conflicting evidence

1. Do not delete the older claim silently.
2. Identify whether the two claims concern the same revision/profile/state.
3. Prefer the evidence source that directly measures the disputed behavior.
4. Record the correction and why the earlier method was insufficient.
5. Downgrade the status to `UNKNOWN` if neither source resolves the conflict.
6. Update `status-matrix.md` only after the corrected record exists.

Newer is not automatically stronger. A recent unverified implementation is
weaker than an older bounded live proof for the older revision.

## Common invalid upgrades

Do not convert:

- “file exists” into “feature implemented”;
- “build passed” into “hook works”;
- “hook installed” into “gameplay works”;
- “log line appeared” into “visual result is correct”;
- “actor moved” into “collision/animation is synchronized”;
- “input was sent” into “native action was admitted”;
- “animation selector matched” into “damage/effects are authoritative”;
- “session token matched” into “network peer is cryptographically trusted”;
- “old milestone says next” into a current roadmap decision.

## Public versus private evidence

Public evidence should contain sanitized technical conclusions and reproducible
procedures. Keep raw logs, full paths, private addresses, process dumps, packet
captures, user saves, recordings, and machine identities in the private local
evidence index. Cite only a neutral artifact ID from public documentation.
