# Feature status matrix

This document has one purpose: it is the proposed canonical summary of current
feature status and evidence. It does not replace subsystem guides, the research
log, architecture documentation, or the roadmap.

Audit baseline: commit `b957afee197d` on `codex/shared-simulation`, with a large
uncommitted LAN/skill/lifecycle working tree inspected on 2026-09-04. No build or
live test was run as part of this documentation draft.

## Controlled vocabulary

Every row has exactly one implementation state:

- `IMPLEMENTED` — the bounded intended implementation exists; limitations may
  remain.
- `PARTIAL` — an end-to-end slice exists, but required behavior is missing.
- `FOUNDATION` — policy, data, or ABI groundwork exists without a complete slice.
- `PLANNED` — intention is documented without an established implementation.
- `RETIRED` — deliberately disabled, rejected, or superseded.
- `UNKNOWN` — repository evidence cannot establish the state.

Every row has exactly one maturity/support value:

- `SUPPORTED`
- `EXPERIMENTAL`
- `RESEARCH-ONLY`
- `NOT-SUPPORTED`
- `UNKNOWN`

Evidence uses only:

- `CODE_EXISTS`
- `POLICY_TESTED`
- `PROTOCOL_TESTED`
- `EXACT_IMAGE_TESTED`
- `LIVE_PROVEN`
- `HISTORICAL_CLAIM`
- `UNVERIFIED_DIRTY_WORK`
- `NONE` when no evidence tag applies

Multiple evidence tags may apply. `LIVE_PROVEN` means only the exact bounded
scenario recorded by the cited evidence; it does not generalize to another
profile, actor, map, revision, transition, or teardown case.

## Matrix

| Feature | Profile | Implementation state | Maturity/support | Evidence | Last known verification | Known limitations | Current relevance | Next proof required |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Supported executable identity | All injected profiles | IMPLEMENTED | SUPPORTED | POLICY_TESTED, EXACT_IMAGE_TESTED | README and supported-image tests through 2026-09-03 history | Exactly one GOG PE32/x86 build | Mandatory foundation | Repeat exact-image coverage for changed signatures; treat a new image as separate research |
| Raw launcher and DLL injection | All runtime profiles | IMPLEMENTED | EXPERIMENTAL | CODE_EXISTS, EXACT_IMAGE_TESTED, HISTORICAL_CLAIM | README and build documents, 2026-09-01 | Windows/Wine-specific; user supplies the game; product is still a prototype | Mandatory foundation | Packaged launch, failure rollback, and stop acceptance on supported environments |
| Graphical beta launcher | Launcher | IMPLEMENTED | EXPERIMENTAL | CODE_EXISTS, POLICY_TESTED, HISTORICAL_CLAIM | README and Windows CI references, 2026-09-01 | Launcher/core/protocol versions differ; launcher maturity does not imply multiplayer maturity | Active tooling | Define release gate and verify packaged launch/stop/log/update flows |
| Common hook mechanisms | Multiple | IMPLEMENTED | RESEARCH-ONLY | CODE_EXISTS, POLICY_TESTED, EXACT_IMAGE_TESTED, UNVERIFIED_DIRTY_WORK | Existing call/pointer/image tests; teardown hardening inspected 2026-09-04 | Not every feature adapter historically propagated restore failure | Critical | Failure-injection coverage for every live owner and retryable teardown path |
| Reverse-engineering toolchain | Research | IMPLEMENTED | SUPPORTED | CODE_EXISTS, HISTORICAL_CLAIM | Ghidra reports and research ledger through 2026-09-04 | Exact-build-specific; confidence is claim-specific | Ongoing | Index each claim to its source/report/test and correction history |
| Cleanroom `testroom` launch | Cleanroom | IMPLEMENTED | EXPERIMENTAL | CODE_EXISTS, EXACT_IMAGE_TESTED, LIVE_PROVEN | Cleanroom guide, including working-tree updates on 2026-09-04 | Research environment, not campaign gameplay | High | Maintain exact launch/exit/reload acceptance tied to revisions |
| F8 cleanroom menu | Cleanroom; LAN host subset | PARTIAL | EXPERIMENTAL | CODE_EXISTS, EXACT_IMAGE_TESTED, LIVE_PROVEN | Cleanroom guide and research ledger | Rows have different proof levels; LAN runtime owns some lifecycle rows | High | Row-by-row action, failure, and teardown matrix |
| Native party actor spawn/remove | Cleanroom | IMPLEMENTED | RESEARCH-ONLY | CODE_EXISTS, EXACT_IMAGE_TESTED, LIVE_PROVEN | Cleanroom guide | Restricted to known actors/resources and validated lifecycle states | Research foundation | Repeat spawn/remove/rebuild and negative cases after lifecycle changes |
| Training dummy | Cleanroom; LAN arena | IMPLEMENTED | EXPERIMENTAL | CODE_EXISTS, LIVE_PROVEN | Cleanroom guide and historical live records | Fixed research target; not general enemy replication | High for arena tests | Damage, targeting, respawn, and disconnect convergence |
| Cleanroom resource/skill cheats | Cleanroom; LAN host | PARTIAL | EXPERIMENTAL | CODE_EXISTS, EXACT_IMAGE_TESTED, LIVE_PROVEN, HISTORICAL_CLAIM, UNVERIFIED_DIRTY_WORK | Mixed committed and dirty cleanroom evidence, 2026-09-04 | Party breadth, availability, and live proof differ by row; LAN requires canonical ownership | Test enablement | Explicit per-row live and restoration verification |
| Local controller bridge/input hub | Local co-op | IMPLEMENTED | EXPERIMENTAL | CODE_EXISTS, POLICY_TESTED, LIVE_PROVEN | README and local-co-op history | Device/environment-dependent bridge; separate from LAN gameplay | Relevant to local mode | Device-neutral packaging and disconnect/reconnect matrix |
| Two-player control separation | Local co-op | PARTIAL | EXPERIMENTAL | CODE_EXISTS, POLICY_TESTED, EXACT_IMAGE_TESTED, LIVE_PROVEN | README and research history | Campaign transitions, native globals, and ranged presentation remain incomplete | Maintained experiment | Uninterrupted encounter plus transition and reverse-teardown acceptance |
| Two-player split compositor | Local co-op | PARTIAL | EXPERIMENTAL | CODE_EXISTS, EXACT_IMAGE_TESTED, LIVE_PROVEN | README and local rendering history | Menus, cinematics, HUD/global ownership, and performance constraints remain | Maintained experiment | Full encounter and special-camera lifecycle proof |
| Fixed-three local renderer | Fixed-three local | PARTIAL | RESEARCH-ONLY | CODE_EXISTS, POLICY_TESTED, HISTORICAL_CLAIM | Later local-co-op history; current status is not consolidated | P4 unsupported; not campaign-wide; fragmented documentation | Separate experiment | Canonical exact-profile live record tied to one revision |
| Custom per-seat QuickMenu | Fixed-three local | PARTIAL | RESEARCH-ONLY | CODE_EXISTS, POLICY_TESTED, HISTORICAL_CLAIM | Local development history | Custom presentation exists; full category/action/native parity is unestablished | Separate experiment | Category-by-category native action and concurrent-seat live proof |
| Native QuickMenu virtualization | Legacy/local experiments | PARTIAL | RESEARCH-ONLY | CODE_EXISTS, LIVE_PROVEN, HISTORICAL_CLAIM | README and research ledger | Native QuickMenu is global; time, camera, and owner behavior remain constrained | Research reference | Classify every retained path as maintained, superseded, or retired |
| Roster UI and role sidecar | Local co-op | PARTIAL | EXPERIMENTAL | CODE_EXISTS, POLICY_TESTED, LIVE_PROVEN | README, 2026-09-01 | Global sidecar is not a demonstrated per-save role contract; actor availability can lag selection | Local-mode foundation | Restart/load/transition matrix and persistence policy decision |
| Party-atomic transition | Local co-op | PARTIAL | RESEARCH-ONLY | CODE_EXISTS, POLICY_TESTED, HISTORICAL_CLAIM | README and transition research | One native world and authored tasks constrain safe interruption; travel-vote seam remains disabled | Future campaign work | Full temporary/persistent transition acceptance without native-task corruption |
| Travel voting | Local co-op | RETIRED | NOT-SUPPORTED | CODE_EXISTS, HISTORICAL_CLAIM | README | Existing hook occurs after native approach/script begins and cannot safely veto it | Historical evidence | Prove a target-specific pre-action defer/replay seam before reconsideration |
| Wallet/economy foundations | Local co-op design | FOUNDATION | RESEARCH-ONLY | CODE_EXISTS, POLICY_TESTED, HISTORICAL_CLAIM | Personal-wallet design | Native mutation and persistence integration are incomplete or intentionally inert | Deferred | Human scope decision, then exact native transaction proof |
| Expanded Talos encounter | Encounter research | FOUNDATION | RESEARCH-ONLY | CODE_EXISTS, POLICY_TESTED, LIVE_PROVEN, HISTORICAL_CLAIM | README and encounter design/trace evidence | Spawn-after-transition path was retired after a pure-virtual failure | Specialized research | Native-safe staging/carry and complete lifecycle proof |
| LAN handshake/session transport | LAN arena | IMPLEMENTED | EXPERIMENTAL | CODE_EXISTS, POLICY_TESTED, PROTOCOL_TESTED, LIVE_PROVEN, UNVERIFIED_DIRTY_WORK | LAN guide through 2026-09-03; dirty LA22 changes inspected 2026-09-04 | Direct IP only; trust model and final protocol are not settled | Core current work | Final-version malformed/timeout/reconnect matrix and two-machine acceptance |
| LAN shared-simulation authority model | LAN arena | PARTIAL | RESEARCH-ONLY | CODE_EXISTS, POLICY_TESTED, UNVERIFIED_DIRTY_WORK | Shared-simulation commits through `b957afe`; dirty changes inspected 2026-09-04 | Canonical node currently runs in host process; dedicated node absent | Core current work | Human authority decision and end-to-end consequence validation |
| LAN Tal/Ailish movement | LAN arena | PARTIAL | EXPERIMENTAL | CODE_EXISTS, POLICY_TESTED, LIVE_PROVEN | LAN guide and live research records | Fixed roles/map; client collision, idle, and presentation required repeated tuning | Core playable slice | Repeatable two-machine movement, wall, idle, latency, and disconnect acceptance |
| LAN weak attacks and Tal actions | LAN arena | PARTIAL | EXPERIMENTAL | CODE_EXISTS, POLICY_TESTED, PROTOCOL_TESTED, LIVE_PROVEN | LAN combat graph, 2026-09-03 | Not all combo transitions or gameplay consequences are represented | Core current work | Complete native-result graph, hit consequences, retirement, and disconnect tests |
| LAN Ailish ranged combat | LAN arena | PARTIAL | RESEARCH-ONLY | CODE_EXISTS, HISTORICAL_CLAIM, UNVERIFIED_DIRTY_WORK | Dirty LAN work and prior live observations, 2026-09-04 | Aim, hold cadence, projectile, resource, impact, and presentation are not fully joined | High | End-to-end canonical admission, projectile consequence, and replica presentation proof |
| LAN skills and Spirit | LAN arena | PARTIAL | RESEARCH-ONLY | CODE_EXISTS, POLICY_TESTED, PROTOCOL_TESTED, UNVERIFIED_DIRTY_WORK | Dirty protocol/runtime/replica work inspected 2026-09-04 | Async task lifetime, camera, audio, non-caster motion, disconnect, and teardown remain active research | Highest current work | Lifecycle and exact-image matrix followed by two-process live acceptance |
| LAN damage and enemy convergence | LAN arena | FOUNDATION | RESEARCH-ONLY | CODE_EXISTS, POLICY_TESTED | LAN guide and combat graph | Complete hit, stagger, enemy action, spawn/despawn, and recovery convergence are absent | Required for playability | Canonical damage/enemy event and snapshot acceptance |
| LAN pause/multiplayer panel | LAN arena | PARTIAL | RESEARCH-ONLY | CODE_EXISTS, EXACT_IMAGE_TESTED, UNVERIFIED_DIRTY_WORK | Dirty implementation and rollback tests inspected 2026-09-04 | UI quality, native pause interaction, reconnect, and simulation continuity are not final | Active UX/tooling | Visual acceptance plus reconnect and no-desync proof |
| Campaign LAN multiplayer | Campaign | PLANNED | NOT-SUPPORTED | NONE | Explicitly excluded by current LAN guide | Saves, quests, dialogue, shops, loot, transitions, and arbitrary maps are unsynchronized | Long-term priority unknown | Human roadmap decision and state-ownership design |
| Discovery/matchmaking/NAT traversal | LAN/online | PLANNED | NOT-SUPPORTED | NONE | Explicitly outside the current direct-IP milestone | Direct-IP LAN/VPN only | Deferred | Human product and network-scope decision |
| Four-player runtime | Local or LAN | FOUNDATION | NOT-SUPPORTED | CODE_EXISTS, POLICY_TESTED, HISTORICAL_CLAIM | Selected design/policy foundations only | No complete P4 camera, input, renderer, or runtime | Deferred | Stable lower-seat foundation and human priority decision |
| Explicit DLL unload | Runtime lifecycle | UNKNOWN | UNKNOWN | UNVERIFIED_DIRTY_WORK | Active lifecycle review inspected 2026-09-04 | Process detach cannot itself veto unload; async tasks/timers may remain | Safety-critical | Human support policy plus game-thread shutdown-handshake proof |

## Maintenance contract

Update a row only when its evidence record identifies the revision, profile,
procedure, result, and limitations. If evidence conflicts, preserve the more
conservative status and link the conflict rather than choosing the most
optimistic statement.
