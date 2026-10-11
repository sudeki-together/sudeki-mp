# Co-op story plan (#54): owner-approved implementation plan

Status: **owner-approved plan, 2026-10-10.** Under AGENTS.md this is the owner-populated document
that sets priority for #54 work on `codex/shared-simulation`. It does not prove that anything works.
Evidence words follow `docs/evidence-index.md`. Addresses are VAs on the supported image
(RVA = VA − 0x400000).

The executing agent (Codex) works through the phases in order, records progress in
[§9 Progress log](#9-progress-log), and stops at every **owner gate**. A later agent continues from
the progress log.

---

## 0. Rules for the executing agent

1. Read `AGENTS.md` first and follow it, especially: evidence vocabulary, exact-build safety,
   one adapter per native seam, transactional install and reverse rollback, game-thread-only
   native access, and the "before claiming that something works" checklist.
2. **Working tree.** At plan adoption the branch carries uncommitted #54 work belonging to the owner,
   plus the research module `src/hooks/lan_story_research54.c/.h` and its gated call sites.
   **Step 0 (answered by the owner, 2026-10-10): no baseline commit.** Leave the existing uncommitted
   work uncommitted; commit only the files each plan step changes (stage paths explicitly, never `git add -A`).
   Never reset, clean, stash or reformat the existing work.
3. **Commits.** Use the repository identity `heywander <127889726+heywander@users.noreply.github.com>`.
   Make one commit per completed step, with the issue number in the subject. Do not push unless the owner asks.
4. **Graph.** Before broad searches, run `graphify query "<question>"` (see AGENTS.md). After each
   code change, run `graphify update .`. Record verified native findings with the `graph-findings` workflow.
5. **Validators are part of the image contract.** Before patching any native byte, search the
   source for every check that reads it, **by symbolic RVA name as well as by literal address**.
   Known checks that a patch must not break:

   | Range | Checked by |
   |---|---|
   | 0x4DAE80, 14 bytes | `party_native_entries_exact` (control_separation.c), before every story control lease |
   | 0x4DB0E0..0x4DB188, hashed | `story_interaction_guard.h` |
   | 0x5114D0, 12 bytes | `lan_story_world.c`, `lan_story_replica.c` |
   | 0x5114D0, 15 bytes | control_separation.c |
   | 0x4B6E50, 8 bytes | control_separation.c, when skill isolation is enabled |
   | 0x427CF0, 16 bytes | controller update vtable hook |

   A research build once put an entry hook on 0x4DAE80. That made the host refuse every remote
   control lease (CONFIRMED_LIVE). Observe 0x4DAE80 at its call sites 0x428E3F/0x428E5E/0x4F4D7F instead.
6. **Proof levels.** A Wine image test proves image contracts only. Live behaviour needs an
   **owner gate**: build, run the Wine tests, write the exact live procedure into the progress log,
   then stop and ask the owner to play it. Never mark a step "working" from compilation or install alone.
7. **Private context.** Stage scripts, logs and machine data live in Git-ignored `.agent-local/`.
   Never copy them into tracked files.
8. **Research build.** Keep `lan_story_research54` (default off: `[StoryAreas] Research54=true`
   plus `ResearchDiagnostics`). Use it to measure each acceptance criterion below. It is observe-only;
   new seams must not reuse its hook sites without moving the observer.

---

## 1. Owner decisions (binding for this plan)

| # | Decision |
|---|---|
| D1 | Split the story area-state message (1434 of 1440 payload bytes used) into an **area** message (zone residency) and a **presentation** message (sky, light, despawns and the new fields below). Each message gets its own sequence. Protocol version bump. Per datagram ≤ 1468 bytes, no IP fragmentation; any number of messages. |
| D2 | The native **lead passes around freely**, by retail rules (KO rotation, scripted `SetAsLeadPlayer`). Every player always controls **their own** character and keeps their own camera, the host included. Leader-only systems get per-player rules. Built after D8. |
| D3 | **Native game over**: only when every party member is down. A downed player **spectates**: their camera cycles through every party member, alive or down (bodies), AI companions included. A revive returns control and camera. |
| D4 | **Everyone can interact**, with no character cloning. Doors and chests by anyone (scoped interacting-actor swap). NPC talk **one conversation at a time**; others spectate it. Shops per player. Saves host-only. |
| D5 | **One real loot drop, visible and pickable on every screen; no phantom drops.** Deaths and loot rolls happen once, in the host world. Client copies never run native death. Host drops are sent to clients. Pickup rule: florins, consumables and materials credited **in full to every character**; keys and quest/story items stay one shared copy; character equipment goes to its owner. Native stack limits unchanged; the ledger logs every cap hit. |
| D6 | **Save transfer.** On join the host's current save goes to the client's temp area and the session loads from it, so clients don't need an identical save. Every host save replaces the temp copy (checksum-verified; partial transfers discarded). On session end or disconnect the client asks "Disconnected. Would you like to save?": Yes copies it into a co-op slot (never over the player's own saves without asking; backup first), No deletes it. After a client crash, the same question comes at next launch, asked once; closing it counts as No. Only one temp copy exists. Unclaimed copies auto-delete after 7 days. The launcher gets "clear pending co-op saves". Uninstall removes the temp folder. The client's native save menu stays blocked. Transfer goes over the reliable lobby (TCP) link. |
| D7 | Enemy scope: Kurians (Levy, Trooper, Shooter) and Grotesques, **Shooter missiles**, **allied guards**, and the **Mungo** boss (`BOSS_MUNGOMOFO`, shield variant). |
| D8 | Remote control parity. Every frame the host refreshes each remote player's character **auto-target** (its own CTargeter) from **that player's** facing; remote turning uses the **player** turn path; movement keeps being sent during attacks like the native controller. **No lock-on** (Tal has none in normal play). Deferred until needed: remote block; a per-player auto-target-off setting. |
| NG | **New-game co-op.** The host can start a new game; clients start a new game together and stay in sync through the intro by presenting the host's (client world contained, host cinematic shown). Each client seat is **locked to its character** and spectates until that character joins the party, then takes control. Checkpoints re-prove client/host agreement (zone, cinematic, roster). |

**Acceptance route (end of plan):** new game; the host plays **Tal** from the start; the client
spectates, seat locked to **Ailish**. Tal travels to **New Brightwater**, fighting Kurians, Shooters
and missiles, with allied guards, and the **Mungo** boss. When Ailish joins, the client takes her.
The party returns to the **castle**.

---

## 2. Root causes this plan fixes (sanitized evidence)

### 2.1 Remote Tal turns away from enemies on every swing

* **Mechanism (CONFIRMED_STATIC).**
  1. Combo dispatch 0x4D0730 (block 0x4D0798–0x4D07D5) reads the attacker's **CTargeter at actor+0xAC**
     and takes its current-target TPtr (+0x54).
  2. When the combo's face flag is set, it calls 0x4B6F80 → 0x4B6F20 (face point: target position minus own position) → 0x4B6E50 on the turn component.
  3. The turn component is **actor+0x8C**, vtable 0x6D48D4; it is not the targeter.
* **Measured (CONFIRMED_LIVE, castle-road ambush, local vs remote Tal).**

  | | Local Tal | Remote Tal |
  |---|---|---|
  | Attack facing within 45° of the stick heading | 96 % | 45 % |
  | Attack facing 135–180° away | 0 % | 28 % |
  | Target changes | 225 in 207 s | 16 in 158 s |

  The native controller keeps the lead's targeter current. Nothing feeds a remote follower's targeter.
* **Contributing (CONFIRMED_LIVE).**
  * With `ai+0x16A = 1`, every remote turn step goes through the AI branches 0x4B749F/0x4B74DD,
    never the player branch 0x4B75AC. The native lead also takes the AI branch whenever the game raises the counter.
  * The AI movement node 0x4F4BB0 calls `SetSpeed(0)` (0x4DB070, call 0x4F4DE3) every frame for the remote actor.
  * The mod withholds movement while the body is busy: about 60–85 % of frames during fights.
* **Ruled out (CONFIRMED_LIVE).** Packet gaps (20–61 ms). The 0x4DAE80 argument shape: identical to native 0x428E5E.
  Client presentation of Tal.
* **Native press cadence (CONFIRMED_LIVE).** 1→2→3→0 through 0x42891F. Held states (2, including block)
  arrive through 0x4DA816. The mod sends a single 1 with block, next and prev all 0.
* **Measurement bug.** The existing `tal_facing_away` diagnostic in `control_separation.c` reads its target from
  actor+0x8C+0x54. That is the turn component; use actor+0xAC.

### 2.2 Client enemies, loot and presentation

| Problem | Cause | Evidence |
|---|---|---|
| All monsters freeze, then snap | Whole-frame refusal while a host clip resource streams in (`clip_resource_prepared`) | CONFIRMED_LIVE |
| One monster frozen until it attacks | Pose-unsupported monster omitted from the apply set (no longer firing after the binding admission; the hazard remains) | CONFIRMED_LIVE (history) / CONFIRMED_STATIC |
| Enemies idle after the host is down | The held downed lead stays in slot 0; the "lead" selector (0x5B76C0 mode 0xB) finds nothing; AI decision target empty 87 % of samples vs 48 % before | CONFIRMED_LIVE + STATIC |
| Stagger differs | Reaction requests (0x4D20F0 → combat+0x5C) are consumed in the paused entity tick; the client only gets pose | CONFIRMED_STATIC |
| No flash, bar or numbers | Flash 0x4DFAB0 and stat display 0x528FF0 tick only in the paused entity update; the existing lease is not proven live | CONFIRMED_STATIC / UNKNOWN live |
| Sigil weaker and shorter | `fx_tick` (`lan_story_spawn_trace.c`) passes the scheduler list as the timing block (dt ≈ 1e-38); fixed 6 s client hide vs host release on the monster's animation change | CONFIRMED_STATIC + LIVE |
| Phantom loot, missing real loot | A replayed killing hit runs native death 0x4D31C0 on the client copy, which rolls loot locally. Host drops (DroppedItem, kind 0x0F8D, vtable 0x6D3E68) are not replicated. | CONFIRMED_LIVE (counts), static search for the replication path |
| Client never "in combat" | Client cluster manager runs, but the leader membership sector `*(u16*)(*(slot0+0x74)+0x40)` stays stale (Town_Cluster) while the host is on Castle_Road, in combat | CONFIRMED_LIVE |
| Missiles invisible | MissileEntity (0x6D5A10, kind 0x0F97) spawns on the host only | CONFIRMED_LIVE |
| Spawn sets diverge | Client spawn groups run their own timers and random point choice; (group, point, type) identity can repeat | CONFIRMED_LIVE / STATIC |

Detailed static research (class layouts, entry bytes, call chains) is in the graph memory (`graphify
query`) and in the research log. Re-verify every address before use.

---

## 3. Phase A: groundwork and debt (no protocol change)

| Step | Work | Acceptance |
|---|---|---|
| A1 | Fix the sigil timing in `lan_story_spawn_trace.c` `fx_tick`: pass a `{0,0,0,dt}` timing block to 0x5319B0 with a real capped dt | Image/pure test of the call shape; owner gate: sigil and ring strength and duration match the host in the ambush cutscene |
| A2 | Fix the `tal_facing_away` diagnostic offset (actor+0xAC) | Builds; research log shows real targets |
| A3 | Light manager: S2 found the native light node already ticks on the client. Remove the manual tick and the direct call to 0x41F350, which bypasses the cast-light seam owner. Keep the light target write only if still needed, with an ownership check and an exit restore. | Owner gate: lighting matches; no seam bypass in source |
| A4 | Sigil hide: restrict to proven sigil SpecialEffects (today it hides any new SpecialEffect) | Pure test of the predicate |
| A5 | Re-prove the arbiter 0x400 lift in `FlushMonsterDamage` (vtable/identity after DoDirectDamage); superseded by step D1 (Phase D), but keep it safe until then | Image test |
| A6 | Residency re-baseline window (`AreaFollowNoteSpawn` keeps the proof open through combat): close it to the spawn calls the mod itself caused | Pure test |
| A7 | **Enemy spawn effect, as one item**: the ring/circles on the ground (TorusEmitter particle) and the sigil floating in mid-air (SpecialEffectEntity, update 0x5319B0) where enemies appear. (a) Tick the sigil with real dt (A1). (b) Keep the particle manager advancing through short not-ready gaps, so the ring doesn't stall at each spawn. (c) Show it during cutscenes as on the host. (d) End it when the **host's** sigil ends (sigil-end field, B1), replacing the fixed 6 s client hide. (e) Hide only proven sigils (A4). | Owner gate: in the castle-road ambush intro and in each later wave, ring and sigil match the host in strength, position and duration |

## 4. Phase B: protocol split (D1)

| Step | Work | Acceptance |
|---|---|---|
| B1 | New **presentation** message: move sky, light calls and despawns out of area state. Add fields: host cluster hash, host current sector (`cm+0x2C`), in-combat (`cm+0x34`), `cm+0x36`, suspension (`cm+0x30 != 0`); sigil end per spawn; spawn serial; host drop records (Phase D). Own sequence, version bump, per-message size asserts ≤ datagram limit. | Codec tests (round trip, truncation, stale sequence, version mismatch); `story_area_state_test` updated; both peers refuse mixed versions |
| B2 | Area message keeps zone residency only | Same tests; owner gate: zone follow still works (castle road ↔ New Brightwater) |

## 5. Phase C: remote control parity (D8), the biggest player-facing fix

Static research first (record each in the graph): which native routine refreshes the lead's
CTargeter target each frame (candidates: targeter node 0x4B8200 → 0x4B9AB0 → 0x4BA1C0, gated by
`targeter+0x84 & 2` and the controller/lock globals; the controller's own call at 0x428031). Can it run
for a non-lead actor with that actor's facing, without touching `controller+0x248` or 0x400000?

| Step | Work | Acceptance (measure with Research54, then owner gate) |
|---|---|---|
| C1 | **Auto-target refresh** per frame for each leased remote actor, from that player's facing, through the native targeting routine. One adapter, game thread only, leased actors only. | Remote swings within 45° of the stick ≥ 90 % (local 96 %); reversed (135–180°) ≤ 3 %; target-change rate comparable to local |
| C2 | **Player turn path** for leased actors: route the 0x4B73D0 decision to the player step 0x4B7B90, call-scoped (hold `ai+0x16A` at 0 only around the turn step and the mod's 0x4DAE80 call, or patch the branch test for leased actors only; choose with static proof) | Research54: remote Tal takes `turn_player`, 0 AI turn steps outside game-raised override windows |
| C3 | **Gate the AI movement node** for leased actors: no `SetSpeed(0)` at 0x4F4DE3, no AI moves at 0x4F4D7F, no AI snaps at 0x4F5185/0x4F51C5 | Research54: `ai_set_speed` ≈ 0 for the remote actor |
| C4 | **Movement during attacks** like the native controller: keep calling 0x4DAE80 every frame with the stick and let the arbiter's CanMove decide. Stop with blend `SetSpeed(0,1.0)`, not set-speed-immediate. | Research54: held-frame share during fights near local levels; owner gate: Tal controllable mid-combo |
| C5 | Optional, low priority: native press cadence (1→2→3→0) through the same snapshot shape as 0x42891F | Combo windows match local play |
| C6 | Owner gate: the owner plays remote Tal through the castle-road ambush, solo vs co-op comparison | Owner confirms no "el reverso" |

## 6. Phase D: enemies and loot (D5, plus freeze fixes)

| Step | Work | Acceptance |
|---|---|---|
| D1 | Client: clamp replayed damage so a copy's HP stays > 0; never run 0x4D31C0 on a client copy; remove copies only through host despawn follow. Removes the 0x400 lift risk and phantom loot/XP. | Research54/log: no client DroppedItem from kills; copies never stuck dying |
| D2 | Spawn identity: use the native entry index (point+0x7A low 11 bits), pass count and type, plus the host spawn serial (B1). Log and exclude collisions. Clients spawn **on host spawn events**, not their own timers or random choice (consistent with D5 "one real"). | Pure tests for identity/collision; owner gate: no spawn-set divergence over two waves |
| D3 | **Host drops on every screen**: host captures DroppedItem creation (identity, item, quantity, position) into the presentation message; client presents the real drop at the same place. Pickup is executed on the host for whichever character collects it. | Owner gate: the client sees exactly the host's drops and can collect them |
| D4 | **Monster freeze fixes**: transform-only fallback for monsters whose pose/clip is unsupported (keep position and facing, skip channels); monsters never refuse the whole world frame during clip preparation | Research54: no `monster_gap` > 300 ms outside load; owner gate |
| D5 | Hit feedback: prove or fix the flash (0x4DFAB0) and stat-display (0x528FF0) lease live; stagger stays pose-driven | Owner gate: red flash, bar and numbers visible on the client |
| D6 | **Loot rule** (economy activation): wire native pickups through the existing economy foundation (`personal_wallet`, `story_loot`, entitlement ledger, sidecars), with credit-all for florins/consumables/materials, shared keys/quest items, equipment to its owner, and cap-hit logging. This is the activation milestone described in `docs/personal-wallet-design.md`. | Pure tests for the rule table; image tests for the pickup seam; owner gate |

## 7. Phase E–J

| Phase | Work | Acceptance |
|---|---|---|
| **E. Enemy scope (D7)** | E1 missiles: replicate MissileEntity presentation (position/clip, spawn/despawn) from the host. E2 allied guards (kind 0x0F82) in world frames. E3 **Mungo**: static research (class, likely BossEntity 0x6D69EC; spawn path; shield phase; boss HUD), then replicate. | Owner gate per item on the route |
| **F. Combat state + spectating (D3)** | F1 client cluster follows the host: call-scoped host sector into the client's 0x4352D0 (single call at 0x43513B in 0x435120; verify bytes and that 0x4CF655 is the only other caller); then retire the `cm+0x24` swap in spawn trace. F2 spectator camera for downed players: cycle all party members (alive or down) on client and host (`camera_target_abi.c`); return on revive. | Research54: client cluster hash/in-combat match the host; owner gate |
| **G. Lead vs ownership (D2)** | Remove the death/lead hold. Let native rotation and `SetAsLeadPlayer` run. Decouple the host controller and camera from the native lead, so each player keeps their own character and camera. Apply leader-only systems per player (area streaming, cutscene camera). **Needs C first.** High risk: global controller/camera ownership. | Owner gate: Ailish down → Tal becomes lead, enemies keep fighting, nobody's controls move; scripted lead change on time |
| **H. Interactions (D4)** | Doors/chests/pickups by any player: per-player request to the host; scoped interacting-actor identity at 0x40D330, 0x40D6F0, 0x40D9A0, 0x40CCD0 plus a shadowed +0x1D8/+0x1D9 latch (global 0x808D24). NPC talk one conversation at a time, others spectate (first prove the native dialogue system is single). Leader-only script exports act on the initiator during their interaction. Shops: per-player merchant checkout (existing design). | Owner gate per interaction type |
| **I. Saves (D6)** | Join transfer, push on every host save, temp area, disconnect prompt, crash prompt at next launch, 7-day expiry, launcher button, uninstall cleanup, client save-menu guard (0x484F10 fail-closed, backstop at the call 0x481757). Sidecars travel with the save. Size/checksum/exact-build checks. | Pure tests (protocol, expiry, prompt state); owner gate: join without an identical save; disconnect → keep/discard |
| **J. New-game co-op (NG)** | Host "New Game" in the multiplayer flow; clients start a new game in containment and present the host intro; seat locked to a character; spectate until that character joins; checkpoints re-prove zone/cinematic/roster. | Owner gate: the acceptance route |

## 8. Out of scope

Remote block, a per-player auto-target-off setting, remote lock-on, stack-limit raises, bosses beyond
Mungo, campaigns beyond the castle ↔ New Brightwater loop.

---

## 9. Progress log

Append one row per completed step. A step is "done" only with its acceptance met at the stated evidence level.

| Date | Step | Commit | Tests run (result) | Evidence reached | Owner gate / next |
|---|---|---|---|---|---|
| 2026-10-10 | Plan adopted | (uncommitted) | — | — | Step 0 answered: no baseline commit; start Phase A |
| 2026-10-09 (owner local) | A1 — effect timing and cutscene clock issue resolved | Commit containing this acceptance entry (clock ownership and regression); surrounding prototype remains uncommitted | SpawnTimingImage, EffectClockImage, RangedProjection, InteractionGuardImage: PASS; old-code ownership regression: expected FAIL | CONFIRMED_EXACT_IMAGE + CONFIRMED_TEST + CONFIRMED_LIVE (owner) | Owner: “yep that works”; A2 next when work resumes; A4/A7 remaining requirements unchanged |
| 2026-10-09 (owner local) | A2 — correct target diagnostic | Commit containing this A2 acceptance entry | DLL build: PASS; InteractionGuardImage: PASS; live diagnostic/target-trace correlation: PASS | CONFIRMED_STATIC + CONFIRMED_EXACT_IMAGE + CONFIRMED_LIVE | Actual target node matches the independent trace; A3 next |
| 2026-10-09 (owner local) | A3 — native lighting tick and target ownership | Commit containing this acceptance record; implementation remains in the dirty prototype | DLL build, StorySkyLightImageTest, CastLightAbiTest: PASS; native light advancement and convergence observed | CONFIRMED_STATIC + CONFIRMED_EXACT_IMAGE + CONFIRMED_TEST + CONFIRMED_LIVE (owner) | Owner accepts zone darkening and authorizes moving on; A4 next |
| 2026-10-09 (owner local) | A4 — proven-sigil-only hide | Commit containing this predicate/test and acceptance entry; spawn integration remains in the dirty prototype | DLL build, StorySpawnSigilTest, StorySpawnTimingImageTest: PASS | CONFIRMED_STATIC + CONFIRMED_TEST + CONFIRMED_EXACT_IMAGE | Stated pure-predicate acceptance met; A5 next; A7 retains the visual/lifetime gate |

### A1 owner gate not passed — 2026-10-09 (owner local date)

**Not completed or committed.** Base revision `ff2d280`, with the owner's existing
uncommitted #54 work and the A1 changes. No baseline commit, staging, reset or stash.
The plan remains untracked. A2 and later steps have not started.

- Change: `fx_tick` always passes `{0,0,0,dt}` to the existing native update;
  finite positive dt is capped at 0.1 seconds. Invalid/nonpositive dt does not tick.
  Removed the scheduler-list substitution. No native patch site or signature changed.
- Added `SudekiMP.StorySpawnTimingImageTest`. It executes retail 0x5319B0 on an
  inert effect whose animation component records the timing argument. It checks
  the call shape, cap, invalid deltas, misleading scheduler-list values, native
  pause ownership, registry absence, entry bytes and both vtable identities.
- Build: `cmake --build build/mingw32 --target SudekiMP
  SudekiMP.StorySpawnTimingImageTest SudekiMP.StoryEffectClockImageTest
  SudekiMP.StoryInteractionGuardImageTest` passed with the existing MinGW
  pedantic pointer-conversion warnings in the spawn module.
- Wine: all three named image tests passed against the SHA256-verified supported
  executable from `docs/executable.md`. Invocation for each was
  `wine build/mingw32/bin/SudekiMP.<TestName>.exe <supported-image>`.
  The new test also failed against the preserved pre-A1 source, as expected,
  because the native callback received scheduler-list fields instead of zeroes.
- Evidence reached: **CONFIRMED_EXACT_IMAGE** for this bounded timing call and
  **CONFIRMED_TEST** for the synthetic timing observations. The owner subsequently
  reported the visual mismatches below; visual parity is not proven.
  Runtime teardown/retry and effect retirement were not exercised by the new test.
- `git diff --check` passed. Graphify AST update completed (reported existing
  partial-parser warnings); the native finding was recorded through graph-findings.
- Candidate DLL SHA256:
  `4f884e2d9d7f5193c055c4d95171b95050cfe1a7fcde44618996f6b91e45d77e`.
  After the owner reported ready, both test DLLs were staged and hash-verified;
  previous DLLs and logs were preserved. The previous client ignored normal
  window close and was stopped for the requested restart. Both peers launched
  with Research54 installed. The owner requested a repeat, then reported the
  visual gate not met as detailed below.
  Private build/test artifacts and preparation instructions are retained locally;
  no machine paths or raw logs are included here.

**Owner procedure / next action:**

1. Preparation completed: both peers use the candidate hash above and the
   saved-story test pair has been launched for the owner.
2. Use host Ailish / client Tal at the castle-road ambush approach, with
   `[StoryAreas] Research54=true` and `ResearchDiagnostics=true` on both peers.
3. Trigger the ambush and watch the entire intro without skipping. Compare the
   same ground ring/circles and floating sigil on both screens: strength,
   position, continuous animation, appearance time and disappearance time.
4. Observe the next waves as well and report each mismatch, including whether
   the effect stalls at another enemy spawn or disappears during the cinematic.
5. Record the owner's observed outcome here before calling A1 done or committing
   it. Remain at this owner gate until the result is settled.

The fixed six-second hide, broad SpecialEffect admission, particle gaps and
cutscene presentation are still later A4/A7 work. Host-driven termination needs
B1. A1's full visual acceptance therefore remains open; a timing-test pass must
not be reported as completion of the combined spawn effect.

**Owner observation after repeat (same A1 candidate): CONFIRMED_LIVE, owner visual
report, saved-story host Ailish / client Tal, castle-road ambush.**

- During the ambush cutscene, the client does not show the complete summon
  animation. A white orb appears just above the backs of the spawning enemies'
  heads. The owner suspects an incomplete effect; the orb's identity and cause
  remain **UNKNOWN**.
- At the next enemy encounter, outside the cutscene, the client visibly shows
  the summon effect in the distance as the enemies prepare to attack. This is
  a positive observation for that later occurrence, not acceptance of all waves
  or measured strength, position and duration parity.
- Hit sparks/blood-like impact effects appear near enemies' feet and linger
  longer than in vanilla gameplay before eventually disappearing. Record both
  position and lifetime as regressions to check alongside the spawn effect.

**Gate outcome:** A1 remains incomplete and uncommitted. Preserve the tested
timing correction; investigate the cutscene/later-wave difference and impact
effect lifetime before requesting another visual acceptance. A7 must retain
both intro and later-wave checks; hit-effect position/lifetime must also be
checked so an apparent summon improvement cannot hide another effect regression.


### A1 follow-up investigation — 2026-10-09 (owner local date)

**Still at the failed A1 owner gate.** Base `ff2d280` plus the preserved dirty
worktree; no baseline commit, staging of Git files, reset, stash or completed-step
commit. The owner requested investigation of the incomplete cutscene summon and
feet-level lingering hit effects.

- **CONFIRMED_EXACT_IMAGE:** the real object update at `0x5D64D0` stamps an object
  on its first visit, including a zero-delta visit, and skips a second visit in
  the same frame. A positive manual tick cannot recover the lost animation time.
  The new exact-image fixture executes that path with an inert renderer callback;
  the next frame advances once and the hidden-object guard remains intact.
- **CONFIRMED_STATIC + CONFIRMED_TEST:** the ambient renderer exclusion list also
  included SpecialEffectEntity models, although their poses are not replicated.
  The correction excludes proven effect primary/update classes from that list,
  allowing the existing ambient adapter to supply time on their first native
  visit. A fixture with 129 effects followed by a monster also verifies effects
  cannot crowd the monster out of the bounded exclusion list. Mismatched classes
  retain the conservative behavior. The fixture fails against the preserved
  pre-correction source and passes against the correction.
- Corrected the misleading `fx_animate` frame-stamp comment and the `fx_exact`
  readable extent before accessing the effect flags. No new native hook, frame
  stamp write, native destruction call or protocol field was introduced.
- **CONFIRMED_STATIC:** client monster feedback currently replays a damage amount
  through `DoDirectDamage`; it has no transmitted contact position. Reproducing
  the host's contact point remains unresolved; this clock correction does not
  claim to repair hit placement.
- Added bounded, observe-only Research54 effect snapshots on its existing
  game-thread seam: resource identity/name, pause/active flags, native frame
  stamp, animation clock and local/world position. Added explicit manager
  rejection reasons and raw clock bits to the existing particle seam. Manager
  admission limits are unchanged; stale node detail is cleared at manager proof.
  The orb's asset identity and the later rejection's exact cause remain UNKNOWN
  until the next owner reproduction is captured.

**Validation:** built the DLL and passed `SudekiMP.StorySpawnTimingImageTest`,
`SudekiMP.StoryEffectClockImageTest`, `SudekiMP.StoryRangedProjectionTest`, and
`SudekiMP.StoryInteractionGuardImageTest` under Wine. The effect-clock test also
covers manager rejection reasons and invalid memory/array/clock cases. Native
check readers were audited by symbolic names and literal RVAs; their byte ranges
were not patched by this change. `git diff --check` passed. Graph findings and the
AST graph were updated. These checks prove code/image behavior, not visual parity.

**Next owner gate procedure:** use the same two-instance saved-story ambush
setup with Research54 enabled on both sides. Watch the complete intro without
skipping, comparing the ground ring and floating sigil on host and client. Then
watch the next wave from a distance. During combat, make a single hit and pause
attacking long enough to compare the impact's origin and disappearance; continue
through a later wave to capture any particle stall. Report the intro, later-wave
and impact outcomes separately. Inspect the new effect identities and manager
rejection reasons before changing asset admission or clock bounds. A4/A7 still
need proven-sigil-only hiding and host-driven termination; the six-second hide
has not been replaced. Do not accept A1 or advance the plan from these tests alone.

The revised host/client test pair is prepared and open for this owner gate.
Acceptance remains pending; no completed-step commit was made.


### A1 resolved — owner acceptance, 2026-10-09 (owner local date)

The owner replayed the revised two-instance saved-story candidate and confirmed:
“yep that works. save it as we resolved and it may come in handy later for other
cutscenes”. This supersedes the pending/failed A1 gate entries above for the
reported animation-clock issue. **CONFIRMED_LIVE: owner visual acceptance.**
The validated build was based on `ff2d280` plus the preserved dirty #54 worktree;
its four focused tests and old-code negative control are recorded above.

The reusable mechanism and limits are saved in
[Paused story and cutscene effect clocks](story-effect-clock.md). Preserve native
frame stamps and deliver time on the first renderer visit. SpecialEffect models
must not be mistaken for replicated actor poses in ambient-clock exclusions.

The focused commit saves the renderer-ownership correction, its regression and
this acceptance record. The earlier timing correction/test and investigation
instrumentation remain with the existing uncommitted prototype because their
spawn/research foundation is also uncommitted; no baseline work is swept into
this commit. The full accepted changes remain saved in this worktree.

A1 is resolved for the tested report. The owner's acceptance is not evidence of
unimplemented protocol fields or every other cutscene. A4's precise effect
classification, A7/B1's host-driven termination, and hit-contact replication
retain their separate scope. No later phase was started in this save-only turn.


### A2 completed — 2026-10-09 (owner local date)

Corrected `tal_facing_away` to read the native targeter at `actor+0xAC`, not
`actor+0x8C` (the turn component). Before reading the current target node at
`targeter+0x54`, `party_component` re-proves the `CTargeter` vtable, owner backlink
and readable extent through `+0x58`. `target_valid=1` distinguishes a valid null
or non-null target from an unknown component (`target_valid=0`). The reported
pointer is the native target node, not a resolved monster/entity identity.

**Acceptance met:** DLL build and `StoryInteractionGuardImageTest` passed.
In the existing two-instance saved-story ambush setup (host Ailish, client Tal,
ResearchDiagnostics/Research54 enabled), bounded client direction reversals
produced a corrected `tal_facing_away` message with a non-null target and
`target_valid=1`. The independent Research54 Tal samples immediately before and
after it reported the same node. A valid empty-target message was also observed
before combat. **CONFIRMED_LIVE**, log correlation on `8497eac` plus the preserved
dirty prototype and A2 correction; this is a diagnostic acceptance, not a new
visual acceptance or a facing-behavior fix. See `docs/structures.md`, CTargeter,
for the established component layout. No new native hook or byte patch; teardown
behavior unchanged. Raw logs and the correlation record remain private.

The focused commit includes the corrected diagnostic block and this progress
entry. Research54 wiring and unrelated #54 prototype changes remain uncommitted;
no baseline commit, reset, stash or push. The graph records the correction and
live verification. **Next: A3 light-manager ownership cleanup and its owner gate.**

### A3 accepted — 2026-10-09 owner local date

On `fd70bbe` plus the preserved dirty prototype, the sky follower no longer
calls native light Update (`0x41F350`) or supplies a second clock. The native
scheduler owns interpolation and publication. Host target/rate following remains
because the contained client does not execute the host's AddLightEffect script.

The target override validates the current singleton, primary vtable, original
update slot, named `light_update_head` bytes, scene identity, writable memory and
game thread. A hooked update slot, including the cast-light adapter's slot,
closes admission; this client-only path cannot write a selected private manager.
Rate-only changes are handled alongside the target. Current RGB and the native
effect stack are untouched. The lease saves local values and conditionally
restores them before a followed area switch, observed departure or shutdown.
Newer native writes are preserved; unknown ownership retains the lease for retry.
Runtime teardown now respects a failed sky uninstall.

Validation: DLL build **PASS**; `StorySkyLightImageTest` **PASS** on the supported
image (native hook install/restore, target/rate ownership, invalid observations,
unchanged current RGB, foreign writers, wrong thread, read-only memory and
restore retry); existing `CastLightAbiTest` **PASS**. Both literal-address and
named-symbol source checks confirm no remaining sky call to native light Update.
Evidence: **CONFIRMED_STATIC + CONFIRMED_EXACT_IMAGE + CONFIRMED_TEST**. These
checks do not establish visible lighting parity, live area/exit restoration or
reconnect behavior. The reusable finding is recorded in the local graph.

Bounded live observation on the same dirty candidate: the client current RGB
advanced after the host target changed and converged to the host's ambush target
`(0.60, 0.75, 1.10)` with no manual lighting tick present. **CONFIRMED_LIVE** for
native advancement and target convergence. The owner subsequently accepted the
zone darkening and authorized moving on. This does not claim a separate live
return-to-normal, disconnect or restoration test.

**Owner gate accepted:** the owner reports that the lighting when the zone
darkens is acceptable and explicitly says to move on. "Fades" referred to the
gradual brightness transition; no additional visual requirement is imposed.
The acceptance commit records this result. The implementation and regression
remain in the existing dirty prototype: the sky module was already untracked
and depends on uncommitted area-state types and integration. Do not sweep that
prototype into a baseline commit. No reset, stash or push. **Next: A4**, restrict
sigil hiding to proven sigils; its stated acceptance is a pure predicate test.


### A4 completed — proven-sigil-only hide (2026-10-09 owner local date)

The original six-second branch admitted every newly discovered SpecialEffect.
It now requires the current entity's verified SpecialEffect/update identities,
registry membership, animation component and back-pointer, plus the explicit
resource predicate in `src/hooks/story_spawn_sigil.h`. `fx_hide` repeats that
proof immediately before changing visibility. Unrelated effects retain their
existing animation/update path and cannot enter the sigil timeout branch.

**CONFIRMED_STATIC:** the supported archive name/index checksum identifies
`SFXKK001_SPAWN_EFFECT.HOM` as `0x7e8a6f3f`; the retained ResourceName is at
entity `+0x29C` (low seven kind bits `0x29`, identifier at `+0x2A0`). Existing
SpecialEffect component ownership checks are also used by the Spirit visual
adapter and Research54. **CONFIRMED_LIVE**, observation only on the A3 dirty
candidate: both host and client ambush sigils carried that identifier. The same
class also contained normal hit (`0xb51df833`), item collection (`0x4e2c1433`)
and level-up (`0xe3293133`) resources. Neither class identity alone nor a
resource name containing "spawn" establishes an admitted sigil.

On `7e63fb9` plus the preserved prototype/A4 edits, DLL build **PASS**;
`StorySpawnSigilTest` **PASS** (known sigil, ResourceName flags, every wrong kind,
other observed effects, unknown and neighboring identifiers);
`StorySpawnTimingImageTest` **PASS** on the supported executable. Its additional
fixture exercises the production timed-hide path: a normal hit remains visible
and tracked after six seconds; the verified sigil hides; changed component
ownership, absent registry membership and wrong kind refuse the write. Existing
native call-shape, frame-stamp and byte-identity checks still pass. No native
bytes were patched by A4; literal addresses and named validators were checked.
Evidence: **CONFIRMED_TEST + CONFIRMED_EXACT_IMAGE**, not a new live visual test.

The stated pure-predicate acceptance is met. Only the proved ambush resource is
admitted; further assets require evidence. The six-second timeout remains until
A7/B1 supplies host-driven termination. Full spawn visuals and later-wave parity
remain at the A7 owner gate. The commit saves the independent predicate and pure
regression target; call-site integration and the extended image fixture remain
with the pre-existing dirty spawn prototype, without a baseline commit. The
running A3 pair was read only and was not restarted with A4. **Next: A5**.


### A5 completed — damage arbiter reproof (2026-10-09 owner local date)

On `fd7ef88` plus the preserved dirty prototype/A5 edits, the temporary `0x400`
lift now captures and re-proves the registered MonsterEntity, its native
identifier, replica epoch/generation, CCharacterArbiter vtable and owner, and
CCombat vtable and owner. The exact-image guard uses the native combat slot at
entity `+0xA4`, which is also what DoDirectDamage uses to dispatch the hit.
Registry storage is freshly read after the native call and before each queued
hit. No traversal memory cache crosses damage, and re-entrant calls are refused.

Only the lifted bit is restored, and only if the same arbiter action state
remains. Other native flag changes survive. A retired/replaced target receives
no restoration, feedback lease or diagnostic pointer walk. If observation or
write access becomes unavailable, the restoration lease remains pending; further
damage and world teardown wait for successful restoration or proved retirement.
Stale generations cannot spend an old queued HP delta. HP diagnostics also
revalidate the current stats component rather than retaining its old pointer.
This hardens the existing replay mechanism; Phase D1 will clamp replayed
damage above zero, prevent client-native death and follow host despawns.

Validation: DLL build **PASS**; `StoryMonsterDamageGuardImageTest` **PASS**;
`StoryMonsterDamageFlushImageTest` **PASS**; existing `StoryRangedProjectionTest`
**PASS**. The image fixtures hash-check the supported executable and execute its
retail DoDirectDamage gate and DamageStructure setup. Hit effects, reference-list
binding and allocating cleanup use inert test endpoints. Cases cover every byte
in the named entry/hit-call/return validators; the native blocked and lifted
paths; unchanged versus changed state; entity/component/vtable/owner/native-ID
and generation changes; registry removal, replacement and storage relocation;
read-only/inaccessible memory and restore retry; wrong thread; re-entry; and
teardown refusal/retry. Production feedback from a retired target is discarded.
Named-symbol and literal-address searches include the existing skill-trace
export adapter; no production native patch or second hook was added.

Evidence: **CONFIRMED_STATIC + CONFIRMED_EXACT_IMAGE + CONFIRMED_TEST**.
The A5 image-test acceptance is met. These fixtures do not prove live hit
feedback, lethal-hit behavior, full combat parity or arbitrary native allocator
reuse with an indistinguishable identity. The running A3 pair was not restarted
or modified for A5; there is no new live-play claim.

The focused commit saves the independent guard, image fixture, standalone CMake
target and this entry. Flush integration and its extra CMake test target remain
with the pre-existing uncommitted world-damage prototype. No baseline commit,
reset, stash or push. The reusable finding is saved in the local graph.
**Next: A6**, restrict the residency re-baseline window to mod-caused spawn calls.


### A5 owner play-test accepted — 2026-10-09 owner local date

The owner tested the A5 candidate (`04a31d5` plus the preserved dirty #54
prototype) in the saved-story host-Ailish/client-Tal setup. They report that
repeated hits, killing blows and the next wave seem to work normally and accept
this step. **CONFIRMED_LIVE**, owner report for that bounded regression check.
The client log snapshot contains 233 replayed-hit entries, all with
`identity_proof=1` and `restore_pending=0`; no pending restoration is recorded in
those entries. This does not prove every possible lifetime edge or full combat
presentation parity.

The owner also reports visible damage without floating damage numbers, and
applicable enemies disappearing instead of exploding into chunks on the client.
Some blood-like trails may fail to expire, but the owner is uncertain about that
symptom. These remain open in [follow-up #55](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/55),
with host/client comparison and expiry checks. Numbers relate to Phase D5;
death presentation must respect Phase D1's host-owned deaths/despawns. A5
acceptance does not resolve these visual issues. Root causes are **UNKNOWN**;
trail non-expiry still needs confirmation.

A separate host crash and one stalled client handoff occurred during reloads
before the accepted run; their logs were preserved privately. This acceptance
does not establish startup/crash reliability. No further gameplay code changed
for this acceptance record. **Next: A6**, when the owner moves on.


### #55 researched before A6 — 2026-10-10

At the owner's request, the A5 visual follow-up was investigated before resuming
Phase A. [Research record](story-combat-presentation-55.md) separates supported
native/source findings from live observations and unconfirmed causes.

The accepted-run particle log reaches 129–318 instances and refuses the entire
cosmetic update at the 128-instance guard. This is a demonstrated interruption
of particle advancement; which blood resource lingered and the reason for the
accumulation remain unknown. Damage-number activation has additional native
state/UI gates absent from the current logs. Death replay lacks host fatal-hit
metadata and directly follows despawns, without a proved chunk presentation
path. No visual issue is claimed fixed and #55 remains open. These findings
inform A7 and Phase D without changing their acceptance requirements.

No new live test, reload or gameplay change was performed for this research.
**Next: A6**, authorized by the owner's request to resume the plan.


### A6 completed — synchronous residency accounting (2026-10-10)

On `7fffda7` plus the preserved dirty #54 prototype, combat no longer refreshes
`AreaFollowLastCall`: `AreaFollowNoteSpawn` and its damage/tick call sites are
removed. Only a real native zone export sets that timestamp. The pre-existing
asynchronous area-load permission remains separate; A6 does not claim to
replace that area lifecycle with an entity creation journal.

The new `SudekiMpLanStoryClientResidencyCall` brackets replayed group starts,
group/effect update batches, queued damage and host despawn follow. It proves
containment before entry and accounts the registry/task result immediately on
the same native stack. The adapter requires a current post-controller witness
or the existing exact render/effects witness. It refuses nesting, recruitment,
exit and wrong-thread entry. No timestamp or unused adoption permission survives
return. Failed postconditions close admission and preserve the pause debt.

A callback must report actual entry to a native operation, even when the native
operation fails. Tracked but watched/idle/invalid groups now return false;
no-op despawn and refused damage paths do likewise. Observation-only calls
cannot re-baseline. Refused group starts remain retryable. Post-call adoption
re-proves the current registry and native owners; surviving entities must retain
their exact pause/schedule identity. It cannot rebase a changed survivor, change
the world/party/descriptor or advance the GEL clock. No traversal memory cache
crosses the native operation. New entities must carry an inherited pause.

Validation: DLL build **PASS**; `StoryResidencyCallTest` **PASS** both natively
and under Wine. Its cases cover 15,000 idle observations, unrelated registry/task
changes between calls, actual creation/removal, native failure with residual
allocation, no-op mutation, wrong owner/thread, nesting at every callback stage,
postcondition failure and closed-admission retry. `StorySpawnTimingImageTest`
**PASS** on the supported image, including the production regression that
watched, zero-delta, natively scheduled and invalid groups grant no adoption;
its native sigil call shape, frame-stamp and hide predicates still pass.
`StoryMonsterDamageFlushImageTest` **PASS**, including actual-entry versus
refusal/re-entry/restore-only results. Named validators and literal native
addresses were checked; A6 changes no native byte patches or calling conventions.

Evidence: **CONFIRMED_STATIC + CONFIRMED_TEST + CONFIRMED_EXACT_IMAGE**. The
stated pure-test acceptance is met. No live reload, combat, area transition or
teardown acceptance was performed on this candidate; stricter containment may
expose a previously hidden native mutation and must be checked during the next
owner test. The #55 presentation defects are not claimed fixed.

The focused commit saves the policy, client adapter hunks, public adapter API,
pure test/target and this entry. Runtime/spawn/damage integration and the extended
image fixtures remain with their pre-existing dirty dependencies. Existing
uncommitted work was preserved; no baseline commit, reset, stash or push. The
research findings and A6 contract are saved in the local graph.

**Next: A7**, the combined ground-ring and airborne-sigil work. Its owner gate
still requires host/client strength, position and duration to match in the
ambush intro and later waves, including host-driven termination (B1 dependency).
The observed #55 particle-instance rejection must be considered alongside short
not-ready gaps; raising its bound alone is not a proved expiry fix.


### Owner-requested #55 fixes — candidate at owner gate (2026-10-10)

The owner interrupted the next planned step with “now fix them”: missing client
numbers, missing death chunks, and suspected non-expiring hit/blood effects.
Implementation and evidence are in
[the #55 report](story-combat-presentation-55.md#implementation-and-pre-acceptance-smoke-test).
This is not A7 completion. Base `12e5767`, dirty #54 work preserved; DLL SHA-256
`9d05e92778091aa4fb4e194a4c0d9f27516075726d5cc936df4c36cc89a98b26`.
No baseline commit, staging, reset, stash or push was performed.

Candidate changes: host-eligible native number display, nonlethal client damage
with host-derived displayed amount, authored visual-only death effects selected
by host death style, full native hit-effect completion, and validated particle
capacity. Both peers use session protocol 18/world frame 8. Death styles use the
last six area-body bytes without exceeding the accepted datagram cap; B1 still
must split the messages before adding the planned fields.

The live run found two A6 integration gaps beyond the earlier pure test:
journaled SoloSpawnSetup callbacks and deferred animation-owner sigil cleanup.
Both now account only the native operation executing on their own proved stack.
The latter admits only its effect's removal. No combat timestamp was restored.
Native byte validators were checked by name and address; the cleanup hook uses
`0x4E1870`, preserving the `0x531DF0` body checked by other adapters.

**Tests:** DLL build and all seven focused suites PASS: SpawnTimingImage,
EffectClockImage, MonsterDamageGuardImage, MonsterDamageFlushImage,
StoryAreaState, LanPartySession and StoryResidencyCall. Exact-image coverage
includes the real visual-call ABI, native completion path, journal constructor,
callback identity, native destructor retirement, delayed cleanup wrapper,
unchanged native-retirement bytes and refusal to uninstall pending jobs.
These are not live visual or full game-unload acceptance.

**Live smoke evidence:** saved-story Ailish host/Tal client, castle-road ambush.
Numbers visibly appear. The final candidate replayed 43 hits, followed four
host despawns and accounted their four deferred sigil releases without a
containment failure. All six sampled hit-effect identities were subsequently
removed; 4,200 cosmetic updates were admitted without refusal. Later-wave
monsters appeared. All four sampled deaths were ordinary style-3 deaths with
no authored independent effect; chunk-producing finishers and two complete
later waves remain **UNKNOWN**. No owner acceptance has been given.

**Exact owner procedure:**

1. With the paired candidate, trigger the castle-road ambush and hit enemies
   repeatedly. Compare host/client floating numbers, bar and flash; leave the
   game's number-display setting enabled.
2. Perform a finisher that visibly breaks a host enemy into chunks. Confirm
   the client shows the same kind of effect instead of only disappearing.
   An ordinary collapsing death does not test this branch.
3. Kill several enemies, stop attacking and watch the hit/blood trails finish.
   Repeat through at least two later waves. Look for effects accumulating at
   feet, frozen effects, disappearing numbers or a client that stops following.
4. Report the three observations. Keep #55 open and this candidate uncommitted
   until that gate is settled. Then resume A7, including its B1 host-termination
   dependency and ambush-intro/later-wave visual comparison.


### #55 accepted by the owner; separate follow-ups recorded (2026-10-10)

**CONFIRMED_LIVE (owner):** the owner reports that client damage numbers appear,
lingering hit effects finish and disappear, and death chunks appear. The owner
explicitly accepts all three #55 symptoms as resolved. Tested candidate remains
`12e5767` plus the dirty prototype, DLL
`9d05e92778091aa4fb4e194a4c0d9f27516075726d5cc936df4c36cc89a98b26`, on the saved-story
Ailish-host/Tal-client pair; both staged DLLs were checked against that hash.
Earlier seven-suite results remain applicable: no implementation changed during
acceptance recording. No additional route/wave count or teardown result is
inferred from this feedback. #55 may close; this is not A7 acceptance.

The focused acceptance commit saves only owned, separable changes: native
helper headers, number/nonlethal guards, particle capacity with its regression,
and client residency API/adapter hunks, together with this evidence. Existing
#54 spawn/world/network integration and its dependent image fixtures stay
uncommitted. This follows the earlier scoped-commit approach; the commit alone
is not the complete played candidate. No baseline commit, reset, stash or push.
The exact scoped commit was also built separately from the dirty prototype;
its DLL build, `StoryEffectClockImageTest`, `StoryMonsterDamageGuardImageTest`
and `StoryResidencyCallTest` pass. This checks the commit subset independently,
without broadening the owner-accepted live scope.

The following observations are retained for follow-up, with no cause asserted
and no change to the agreed phase order:

| Owner observation | Next discriminating check |
|---|---|
| Enemy block pose appears on client; accompanying animated effect does not | Compare the same blocked hit on host/client; identify the native block effect and its creation/completion path. Preserve the existing block pose. |
| Ailish sweep has no visible blue wave | Establish vanilla first-person and external-view behavior, then compare host/client action and effect creation. Do not assume the wave should be visible from every camera. |
| Tal cannot activate skills or strikes | Capture the exact attempted action, input admission, availability/resource gates and host result. Inspect alongside Phase C's remote control work without assuming rotation is the cause. |
| Tal becomes very slow after the last enemy dies; an item and pickup flare appear | The owner retracts the initial freeze diagnosis. Correlate combat exit, movement/control state, native time/speed and pickup processing; distinguish actor-only slowdown from whole-game slowdown. Pickup causation is unproved. |

**Next on the agreed plan: A7**, with B1 still required for host-driven sigil
termination. These new observations have not been researched or fixed in this
acceptance pass. The issue report and graph retain #55 acceptance separately
from these open follow-ups.

### B1/B2 and A7 host ending — candidate awaiting owner gate (2026-10-10)

B1/B2 now have a candidate in the existing `codex/shared-simulation` worktree
(`decbbc5` plus the retained dirty prototype and this step). No baseline commit,
stash, reset or push. These steps are **not owner-accepted** yet; Phase C remains
behind the gate below.

**CONFIRMED_TEST:** the party envelope and saved-story profile are version 19.
The area channel carries only zone residency, with its own sequence and a
421-byte body (449-byte datagram). `lan_story_effect_state` is a separate
presentation snapshot containing groups, sky/fog, lighting, despawns, read-only
cluster state, per-spawn sigil receipts, and the drop-record foundation for
Phase D. Its 2,193-byte snapshot uses two application chunks; the largest
complete datagram is 1,176 bytes. Native drop capture/application is still
Phase D work. The existing dialogue-presentation channel is unchanged.

The effect receiver assembles a complete snapshot before publishing it, checks
scene epoch/revision, and uses its own sequence floor. Area admission no longer
mistakes a newer unrelated packet for a newer area report. Native effect
following also requires the host's current zone to be current locally; missing
effect chunks do not stop zone residency following.

**CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST:** A7's six-second hide is removed.
The existing SoloSpawnSetup adapter passively records spawn occurrences on both
peers without extending the client's native-work allowance. A matched host
sigil end hides only the proven sigil's renderer, preserving the native owner
handle and later cleanup. The effect-retirement adapter observes the existing
animation-owner release, the animation-transition release at VA `0x4E2162`,
and the completed-effect tail at VA `0x531AD3`. It records end only after the
original retirement returns, without reading retired native storage. The
`0x531DF0` body remains unchanged for `lifetime_image_exact`,
`SudekiMpLanArenaSpiritVfxVisualImageMatches`, and the jetpack validator.

Validation performed: DLL build; `StoryAreaStateTest`,
`StoryEffectTransportTest`, `LanPartySessionTest`, `StorySpawnTimingImageTest`,
`StorySkyLightImageTest`, `StoryEffectClockImageTest`,
`StoryMonsterDamageGuardImageTest`, `StoryMonsterDamageFlushImageTest`,
`StoryResidencyCallTest`, and `SkillTraceImageTest`. All passed. The new tests
exercise round trips, truncation, mixed versions at both receiving roles,
reordering, missing chunks, stale snapshots, scene fencing, publisher integration,
duplicate freshness, exact-site rejection/restoration, native address reuse,
old-wave end rejection (including a missed local retirement), and preserved owner handles. Native tests use the
supported executable identity; they do not establish visual parity.

Candidate DLL SHA256:
`22f76d10dd271cfc97fee9268a815bf58abcad563f149c02ca5c5ff08605b345`.
Both saved-story test roles were staged with that DLL. Startup and owner results
are recorded separately below; staging alone is not gameplay proof.

**Owner gate — required before Phase C:**

1. On the saved-story Ailish-host/Tal-client pair, enter the castle-road ambush.
   Compare the ground circles and airborne sigil on both screens during the
   intro: strength, position, continuous animation, and disappearance.
2. Finish the fight through its later waves. Each spawn effect should end with
   the host, with no six-second linger, premature disappearance from an older
   wave, or stalled ring. Check that the already-accepted damage numbers,
   ordinary hit effects and death chunks still appear and finish normally.
3. Travel castle road → New Brightwater → castle road. Confirm both sides
   follow the zones and their lighting, without a stuck load or effects from
   the previous area appearing in the new one.

Known limits: the wire retains 32 sigil receipts and 16 future drop records;
local observation has bounded receipt/identity tables and refuses unknown
ownership. This candidate does not prove arbitrary campaign coverage, save
transfer, reconnect during a spawn, or whole-game teardown. Cluster fields are
observed only; host-driven combat-state following remains Phase F. The block
flash, Ailish sweep, Tal skills/strikes, and reported slowdown remain separate
follow-ups recorded above. #55's owner acceptance remains intact.

**CONFIRMED_LIVE (startup only):** both roles loaded the final candidate DLL,
the spawn adapter installed on each side, and the client reached
`input_ready=1` after host-confirmed movement. The client needed a separate
restart after a blank window before reaching the lobby; the cause is unknown
and no gameplay/end result is inferred from that retry. The ambush intro,
later waves and zone round trip are still awaiting the owner. Stop here.

### B1/B2 owner follow-up — client performance and numbers (2026-10-10)

**CONFIRMED_LIVE (owner report):** other effects are showing, combat numbers
are missing, and the client is substantially less smooth than the host. This
does not establish the full intro/later-wave timing and zone-round-trip gate.
The earlier #55 acceptance remains historical evidence; number visibility in
this newer run is an unresolved regression.

**CONFIRMED_LIVE (instrumented):** at the same saved ambush approach, client
presentation preflight averaged a median 15.12 ms in the preceding build and
4.78 ms in the new candidate (20 settled reporting windows each). Material
validation fell from approximately 11.9 ms to 1.5 ms; world application remained
approximately 3.4 ms. These are component timings, not an overall FPS or combat
smoothness acceptance. The change reuses OS memory-range/protection answers
only within one read-only material inspection, resetting them at each call
and texture-job lock boundary. Graph ownership, values, resource readiness and
all native byte checks still run; no material-validity result survives a call.

**CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE:** DLL build,
`StoryMaterialImageTest`, `StoryMonsterDamageGuardImageTest`, and
`StoryMonsterDamageFlushImageTest` passed. Added material checks cover changed
ownership, read-only/no-access permissions, cross-region spans, and recovery
between calls; existing resource-job and foreign-thread checks still pass.
The damage-feedback `flash_entry` and `stat_entry` were also checked by name
against the supported executable and match exactly.

**UNKNOWN:** the cause of missing combat numbers. A read-only live probe
confirmed the client show-numbers option is enabled, but no combat monster was
present in that sample. Bounded hit diagnostics now record host eligibility,
local display admission/timer/state and the native show-numbers option. No
number fix is claimed from these diagnostics.

Candidate DLL SHA256:
`78c489c11acb75d3e8a776f422d4875fa7a79317e6cf883bfeef75ef14a966d7`.
Both roles loaded it and the client reached `input_ready=1`. Owner next checks
movement/camera smoothness and a few combat hits, then reports number visibility.
Keep Phase C and the full A7/B1/B2 owner gate pending.

**CONFIRMED_LIVE (owner follow-up):** on this candidate the owner reports
“the lag is better” and that the client is “matching the frames of the host”.
This accepts the reported smoothness improvement for the tested pair; it is
an owner observation, not a measured FPS equality. Combat-number visibility
was not reported in this follow-up and remains unresolved. A7 is the last
Phase A item; its full spawn-effect comparison and B2's zone round trip remain
the pending checks before Phase C1 remote auto-target refresh.

### A7 accepted — owner confirmation (2026-10-10)

**CONFIRMED_LIVE (owner):** the owner confirms circles and sigils match the
host through the ambush intro and later waves on the current saved-story pair
(candidate `78c489c11acb75d3e8a776f422d4875fa7a79317e6cf883bfeef75ef14a966d7`).
This completes A7's visual owner gate and Phase A's listed steps. It supersedes
the pending A7 visual checks above. The client smoothness improvement is also
owner-accepted. Missing client combat numbers remain an explicitly requested
regression fix; B2's castle-road/New Brightwater zone round trip is still
unconfirmed. Do not treat these confirmations as Phase C authorization or
broader campaign acceptance.

### Client damage numbers confirmed — next gate B2 (2026-10-10)

**CONFIRMED_LIVE (owner):** the owner confirms “damage numbers are there for
the client”. The current candidate remains
`78c489c11acb75d3e8a776f422d4875fa7a79317e6cf883bfeef75ef14a966d7`;
no further number-behavior change was made during this investigation.
The sampled run recorded 129 replayed hits. Read-only observation found the
client's native number state advancing and its numeric UI containing the
host damage value; a game-window capture showed a visible `-64` on the client.
The earlier missing-number report's cause was not established, so this is
current-run visual confirmation, not proof of a newly repaired intermittent bug.

With the owner's explicit intro/later-wave circles-and-sigils confirmation,
A7 and the listed Phase A steps are complete. B1/B2 implementation is already
staged and tested as recorded above. **Next owner gate: B2**, travel castle
road → New Brightwater → castle road, checking both games follow the zones and
lighting without a stuck load or effects from the previous area. Then resume
the plan at C1 remote auto-target refresh; do not skip the B2 gate.

### B2 accepted — proceed to C1 (2026-10-10)

**CONFIRMED_LIVE (owner report):** based on the testing to date, both games
load and follow the correct area, neither gets stuck loading, and lighting
changes correctly, including the castle-road scene. The owner has not noticed
effects carried over from the previous area and reports spells such as Ailish’s
fireball expire on time. The owner accepts the B2 gate and explicitly authorizes
C1. This records the reported testing, not a newly instrumented round trip.

Revision remains `decbbc5` with the existing dirty prototype; candidate DLL
`78c489c11acb75d3e8a776f422d4875fa7a79317e6cf883bfeef75ef14a966d7`.
A7 and B2 owner gates are passed. C1 begins with exact-image research of the
native auto-target routine and actor ownership. No C1 behavior or acceptance
result is claimed yet; broader campaign and teardown coverage remain unproven.

### C1 candidate — native melee auto-target refresh (2026-10-10)

Revision: `decbbc5` plus the existing dirty prototype and this C1 candidate.
DLL SHA256: `2dcbaa394ca4082acc7480985f950110ef546d216b66ae7e4fa6d54ac189e6ed`. No baseline commit or unrelated staging.

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE:** `0x4B9AB0` retains an existing
bit-1 target under positive AI override. C1 uses its native ordinary melee
leaf `0x4BA1C0` with the remote actor's own targeter and accepted heading.
Native scoring, filtering, exclusions and intrusive target replacement remain
intact. The plan's candidate controller call `0x428031` is a turn call, not
target selection. See `docs/engine-functions.md` for the verified contract.

Each ordinary-world remote control update refreshes targeting before movement
can be held by an attack. A neutral/held frame retains the accepted heading.
Native actor/key/generation/roster/game-thread leases and named image contracts
are rechecked; ranged, disabled targeting, UI, skill and invalid-body ownership
are respected. No new patch seam, native-global swap, AI override change or
new lock-on is introduced. Research54 adds bounded target-refresh observations.

Validation: DLL build, `StoryTargetRefreshImageTest`, and
`StoryInteractionGuardImageTest` passed against the supported executable.
The new fixture executes actual native scoring/commit with inert candidates,
reproduces the old retained opposite target, and checks opposite headings,
neutral heading retention, native filtering/exclusions, reference retirement,
foreign ownership, write protection and every-byte contract rejection. It does
not execute real world enumeration or gameplay.

**C1 owner gate — pending:** on the saved-story Ailish-host/Tal-client pair,
play Tal through the castle-road ambush. While attacking, deliberately change
stick direction between enemies on different sides; try moving and stationary
combos through the later waves. Report whether swings choose the intended side
or still turn toward an old/opposite enemy. Research54 must then be checked
against C1's >=90% within45-degree and <=3% reversed-swing targets and the local
target-change reference. These targets have not yet been measured for C1.
Do not mark C1 complete or proceed to C2 from startup/test results alone.
C2 turn-path and C3/C4 movement behavior may still contribute to remaining
rotation/movement symptoms; no whole-fix claim is made.

**C1 startup correction:** the first candidate above refused the remote control
lease because its new guard compared the observer's story-world pointer
(`0x808D10`) with the distinct collision-query manager (`0x808DD4`).
This is a rejected startup, not gameplay acceptance. The corrected guard
proves the story world separately, bounds the collision manager's registration
vectors, and rechecks both identities after native selection. The regression
fixture now deliberately uses separate world/manager objects and rejects
missing or malformed collision vectors. Replacement DLL: `428782b29d0aed62003487f11b4d97d62ebf5c328391ed6fba67416f78fb2828`.

**CONFIRMED_EXACT_IMAGE:** corrected distinct-world/manager regression passed.
**CONFIRMED_LIVE (startup/instrumentation only):** both roles loaded the
replacement DLL, the client reached `input_ready=1` with host-confirmed movement,
and Research54 observed Tal's native target-refresh call completing. This does
not establish the swing-direction percentages or visual parity. C1's owner
combat gate remains pending; C2 has not started. Graph findings and AST index
were updated. No commit, reset, stash or unrelated staging was performed.

### 2026-10-10 — C1 owner feedback and deferred measurements

**CONFIRMED_LIVE (owner qualitative feedback):** the owner reports C1 “seems fine” and explicitly asks to keep a return ticket, then continue. [Issue #56](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/56) records the unmeasured swing-angle and target-change acceptance checks, including a recheck after C2–C4. The C1 numerical thresholds remain **UNKNOWN**; this is permission to advance to C2, not a claim that those thresholds passed. The accepted candidate remains the corrected `428782b2` DLL above. C2 native turn-path research is now authorized.

### 2026-10-10 — C2 candidate, owner gate pending

Native player turning now uses two scoped branch decisions: the turn step
and CanTurn. They apply only to the exact active remote lease with the mod's
one AI hold; native game override counts and other callers keep their original
behavior. No AI-counter swap or host controller/lead change. The mod's movement
call is scoped so it receives the same native player CanTurn gates. C3/C4 have
not been implemented. Research54 turn-path samples now include AI count/mode.

**CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST:** DLL, StoryPlayerTurnImageTest,
SkillTraceImageTest, StoryTargetRefreshImageTest and StoryInteractionGuardImageTest
passed. C2 checks real native turn interpolation, caller/owner/thread rejection,
attack gates, observer coexistence, machine-state preservation and hook rollback/
restoration retry. The extra StoryMeleeAdmissionTest BUSY assertion fails
identically against pre-C2 source; tracked as pre-existing test debt in the
research log, not a passing test. Candidate DLL `d6fa911cce14057a51148d70f3828ad7e2555c00c84f10d544ed3fa7124c288b`.

**Owner gate:** play remote Tal through the ambush, steer between enemies on
both sides, and try stationary and moving attacks. Check for unwanted opposite
turns and whether turning follows your input naturally. Review Research54 for
`turn_player` and zero unexpected AI turn steps while the lease is admitted,
separating game-raised overrides and revoked/loading/menu states. Startup logs
alone do not pass this encounter gate. Stop here before C3.

**Live startup limitation:** both roles loaded the candidate DLL but remained
at blank windows before the lobby. A read-only host stack capture places the
wait in Wine's X11 `wait_grab_pointer` / `set_focus`, before the saved-story
runtime and C2 turn hooks install. A host-only restart reproduced this wait.
The owner has been asked to release the other application's mouse capture.
No client input readiness, live C2 turn counts or encounter acceptance is
claimed yet. The native test candidate remains available for the owner gate.

**CONFIRMED_LIVE (startup and bounded turn-path observation):** after the
desktop wait cleared and the owner's requested pair reload, both roles loaded
the candidate and the client reached `input_ready=1`. In 7 complete
settled Research54 windows (14005 ms, excluding the mixed handoff window),
remote Tal recorded 820 player turn steps and zero AI turn steps. Turn
samples retain `ai_override=1`, `ai_mode=0`: the native counter was not cleared.
Earlier and mixed windows are excluded from this settled slice. This does
not establish zero unexpected AI turns over the whole encounter. Owner visual
acceptance remains pending. C2 is ready for its owner gate; C3 has not started.


### 2026-10-10 — C2 owner feedback: single attacks versus committed combos

**CONFIRMED_LIVE (owner qualitative feedback):** on the C2 candidate above,
the owner can switch direction/target during single attacks; switching becomes
harder once committed to a combo. The owner considers this consistent with
vanilla gameplay. Record C2's visual check as qualitatively accepted.

**INFERENCE:** this observation fits the native CanTurn attack/combo restrictions
preserved and exercised by the C2 exact-image test. It is not a measured
side-by-side vanilla comparison. C1's swing-angle and target-change measurements
remain open in [issue #56](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/56).
C2's full-encounter zero-unexpected-AI-turn check remains unmeasured; the bounded
820-player/zero-AI-step slice above is the current instrumentation evidence.
Do not treat qualitative acceptance as proof of either numerical criterion.

**Next listed step: C3**, suppress AI movement-node commands for the actively
leased remote actor (AI moves, speed resets and snaps). C4 separately covers
movement during attacks. Neither has been implemented here. This entry records
feedback only; no runtime change, rebuild, reload or completed-step commit.


### 2026-10-10 — C3 candidate, owner gate pending

Dirty `decbbc5` plus preserved prototype and this C3 candidate. DLL SHA256
`f7b333d1ae16c00aed5316f7bf5c6dfb98fcbcc30ee87441264680e7cf5b7615`.

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE:** the leased actor now takes the
AI movement node's native player-cache path, bypassing its move and speed-reset
commands. A second gate skips navigation-adjusted AI snap writes while keeping
reference cleanup and fade completion. Explicit scripted placements remain
native. Both gates require the exact active actor/input/session/world lease,
count 1/mode 0, and the witnessed native thread. No AI-counter or controller
swap. See the C3 contract in `docs/engine-functions.md`.

**CONFIRMED_TEST:** DLL build, StoryAiMovementImageTest,
StoryPlayerTurnImageTest, SkillTraceImageTest, StoryTargetRefreshImageTest and
StoryInteractionGuardImageTest passed. The new test executes supported-image
branches and cleanup with inert world/render/movement endpoints; it checks
native fallbacks, fade completion, ABI and transactional restoration. This is
not visual acceptance. Existing melee-admission fixture debt remains unchanged.

**C3 owner gate — pending:** on the reloaded Ailish-host/Tal-client pair,
move Tal, release the stick, and change direction between enemies. Try single
attacks and stationary/moving combos through later waves. Look for unwanted
stops, pullbacks or sudden facing/position snaps; check the host and intro still
behave normally. Research54 should show `ai_move_bypassed` while the remote
lease is admitted and approximately zero `ai_set_speed` / AI movement calls in
those same intervals. Separate cutscene/game overrides and revoked leases.
A bypass counter alone does not prove visible smoothness or a snap was attempted.
C4 movement during attacks has not started. Stop here for owner feedback.

No completed-step commit, baseline commit, reset, stash or unrelated staging.
C1 numerical parity remains open in issue #56; C2 qualitative acceptance stands.


**CONFIRMED_LIVE (C3 startup/instrumentation only):** both roles loaded the
candidate DLL and the client reached acknowledged `input_ready=1`. Across
20 complete settled Research54 windows (40038 ms), after excluding the first
mixed admission window, Tal recorded 6101 AI movement bypasses, zero AI
SetSpeed calls and zero AI move calls. C2 also recorded 6101 player turns and
zero AI turns in this slice. No AI snap bypass was recorded in this slice, so live snap suppression
remains unproven. This is bounded startup
instrumentation, not an encounter-wide measurement or visible smoothness proof.
The client initially waited before its lobby; it progressed after the verified
host window was temporarily minimized and client focus restored, without a
code/config change. Both test windows were restored. **C3 owner gate is ready;
visual acceptance is pending. Stop before C4.**

### 2026-10-10 — C3 play-test combat reports; candidate for recheck

The owner reports a large shield-bearing beast sliding during its explosion
animation, with chunks appearing at the earlier location; delayed enemy attacks
on the client; and Tal unable to use skills/strikes. Clarification: the menu
opens, but selecting the move does nothing. Which screen showed the beast's
slide and the menu input method remain unconfirmed. This is additional feedback,
not C3 visual acceptance. C4 has not started.

**CONFIRMED_STATIC / CONFIRMED_LIVE (diagnosis):** capture failure in Ailish's
ranged animation mapping started a global 3000 ms monster exclusion. Three
such failures align with three host/client gap bursts (14 per-monster gaps on
each side); ordinary host party updates continued. Both Grotesque enemies lose
updates during these bursts, and one despawns before updates resume.
**INFERENCE:** these holes can explain delayed attacks and contribute to the
beast mismatch; they do not prove every reported delay or the exact explosion
emitter. The death-effect helper uses the client's render matrix, while despawn
messages carry identity/style without a host terminal transform. One Grotesque
despawn has style zero, so do not attribute its explosion to that helper.

**Candidate change:** remove the global timed exclusion and retry a refused
capture on the next observation. Exact ownership/pose validation and existing
individual monster exclusions remain. D4's broader transform-only fallback
and its no-gap acceptance criterion are still outstanding; this is a narrow
repair to a demonstrated failure, not D4 completion.

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST:** sky's redundant
`SetLightingChangeRate` hook (RVA `0x1F740`) conflicted with cast-light's named
`light_image_exact` guard. Remove that hook; rate/target state capture and the
client restoration lease remain. No native guard is weakened. Casting setup
now logs individual stages instead of the combined `gates_timing` label.
The exact-image coexistence test fails on the prior adapter and passes after
the correction, including corrupt-byte refusal and independent restoration.
See `docs/engine-functions.md`, story sky / cast-light coexistence.

**CONFIRMED_STATIC (remaining casting work):** ordinary skills already have
an authenticated host request path. Spirit Strikes do not: the contained client
menu only queues ordinary skill rows, the host action dispatcher lacks a Spirit
branch, and the story cast adapter restricts Spirit activation to the native
leader. Implementing remote Spirit requires retained native task/SSP/camera/
effect ownership and host/client presentation checks, not just removing that
restriction. No Spirit routing was enabled in this candidate. The report
snapshot contains no ordinary-skill requests, so the initialization repair alone
does not prove menu submission or execution. Pointer forwarding is also
deliberately excluded by the current contained menu; the owner's input method
must be checked before assigning the remaining selection failure to casting.

**Validation:** dirty `decbbc5` plus preserved prototype and this correction,
DLL SHA256 `6e6885e93d091de1fd4e24c55c13597e1ce6d7d576e3387e07ce3bf9aa08a5e6`.
DLL build, StorySkyLightImageTest, CastLightAbiTest, StoryCastAdmissionTest and
StoryQuickMenuImageTest pass. **CONFIRMED_LIVE (startup only):** both roles load
this DLL, the host reports `story_cast event=bound` for the sparse Ailish/Tal
party, and the client reaches acknowledged `input_ready=1`. The earlier C2 and
C3 runs had the casting bind failure; this was not introduced by C3's AI gate.

**Owner recheck:** attempt an ordinary Tal Skill Strike with enough SP and
confirm action/effects on both screens; compare enemy attacks and both beast
explosions through the ambush/later waves; recheck zone darkening. Record the
input device if menu selection still does nothing. Remote Spirit request
implementation and the beast's exact terminal-transform contract remain open.
C3's own movement/snap gate is still pending. No completed-step or baseline
commit, reset, stash or unrelated staging; stop before C4.

**Additional live observation during the recheck:** one client Q selection now
closes as `skill_queued`, reaches host Tal slot 3, enters/leaves retained task
routing, and returns native validation 2 with no `Use` call. This establishes
menu-to-host delivery for that attempt, not successful casting. The native
validator at RVA `0xB4BC0` maps 2 to actor not ready; readiness predicate
`0xDB9B0` checks combat/arming state, blocking arbiter flags and `0xD0250`.
The snapshots do not identify which exact predicate failed at the call boundary.
Recheck while Tal is armed and idle in combat, after the current action has
finished. Do not bypass native readiness or retry the same request automatically.


### 2026-10-10 — Tal skill readiness and remote Spirit request/retention

The owner explicitly requested the remaining skill rejection and missing remote
Spirit Strike path be resolved before continuing C3/C4.

**CONFIRMED_STATIC / CONFIRMED_TEST:** the contained Q menu now maps Spirit
rows' native character-specific IDs to variants 1/2 and queues them through the
existing authenticated action journal. The host dispatches one native Spirit
activation under the current control fence and retained caster namespace.
Duplicate requests replay results without reactivating. Native SSP, availability,
combat/readiness and busy checks remain; the existing NO_SP result now also
represents Spirit SSP and displays a distinct client message. Packet layouts and
kind IDs are unchanged. Ordinary native validation 2 now reports BUSY instead
of the less specific UNAVAILABLE; requests are never silently retried.

**CONFIRMED_EXACT_IMAGE:** StoryCastReadinessImageTest executes the supported
retail readiness predicates and validator on inert components. It verifies that
combat action locks/modes reject skills while the AI override count itself does
not. The readiness helper at RVA 0xD0250 reads the combat component, not the
arbiter; the caller's register change establishes that distinction. See the
named native contracts in docs/engine-functions.md. No readiness/SP bypass was
added to production.

**CONFIRMED_LIVE / owner failure report (first candidate):** Tal's Q skill
selection reaches the host and passes readiness, then returns validation 1
(insufficient SP) without Use. A subsequent Spirit variant 1 returns native
validation 0 / activation 1. The owner sees it fire but Tal remains frozen and
reports severe visual lag. Host traces show no retained cast root or completion.
Bounded native-frame intervals fall from approximately 58 to 22 frames/second;
this is one run, not a controlled performance comparison.

**CONFIRMED_STATIC / CONFIRMED_TEST (correction):** Spirit manager Update
(RVA 0xF900) launches its power-up script later through 0x10C20 / ROOT_SPIRIT
0x10DE5. The initial candidate incorrectly required short request authority at
that later root. The corrected path authorizes one root from the accepted cast
only, re-proving its retained actor and active manager/camera generation. It
survives disconnect/stop to permit native completion, without admitting another
player action. The new delayed-root regression fails before this correction
and passes afterward. Unknown activity, wrong scope, duplicate roots and
incomplete body/task/effect retirement remain closed. A bounded research trace
records native Spirit state and ticks for the recheck.

**Validation:** dirty decbbc5 plus the preserved prototype and this change.
DLL SHA256 a90196858abeba64e8965d52a7cc1619bcbb57a28f89e5c80648dcf5190a1574.
DLL build and StoryCastAdmissionTest, StoryQuickMenuImageTest,
StoryActionTransportTest, SpiritActivationAbiTest, SpiritInstanceAbiTest,
SkillTraceImageTest and StoryCastReadinessImageTest pass. These are focused
synthetic/exact-image/loopback checks, not completed gameplay acceptance.
The corrected pair is reloaded and has reached client input readiness.

**Owner gate pending:** repeat the same Tal Spirit Strike; confirm the full
move on both screens, control returning afterward and whether the lag ends.
Also attempt an ordinary Skill Strike whose cost fits current SP. Successful
ordinary Use, Spirit completion and performance recovery are not yet proved.
C3 movement/snap acceptance remains pending; C4 has not started. No completed
step or baseline commit, reset, stash, unrelated staging or push.

### 2026-10-10 — Spirit movement recovered; visual/performance recheck

**Owner report / CONFIRMED_LIVE:** Tal can move after the corrected Spirit
activation. The retained root, descendants and body now positively drain and
task routing turns off. The owner still reports slow visual start/end and
missing client Spirit presentation (Tal stands still or may disappear).
Do not treat movement recovery as both-screen Spirit acceptance.

**CONFIRMED_LIVE / INFERENCE:** client preparation refuses Tal's material
animation at the summon onset. The prior trace identifies material token 2,
but not its property arguments. Host frame intervals lengthen while ordinary
controller service and pose capture stay approximately steady. Missing native
material properties and per-script namespace swapping are correction candidates,
not yet proved to explain all visual/performance symptoms.

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST:** material
preflight now accepts native inline color properties (argument 0, members 0/1)
and the second alpha scalar (argument 3, member 1), retaining complete owner,
material, curve and code validation. StoryMaterialOutputImageTest reproduces
the old refusal and passes after the change using actual retail getters on
inert materials. It rejects unknown properties, wrong owners and shader
definition replacement. Bounded refusal diagnostics include property arguments.

The cast-task adapter now lets the exact native STEP waiting prefix
(RVA 0x1C41D0) run once without namespace swapping when its finite current clock
is strictly before its deadline. It returns before opcode fetch or callbacks.
Due, equal, nonfinite, stale-owner and substituted-step cases retain the prior
checked path. Focused tests exercise the literal prefix and those boundaries;
bounded timing counters will measure how much work this removes during play.
No cache spans native callbacks and no task is force-completed.

**Validation:** dirty decbbc5 plus the preserved prototype and these changes.
DLL SHA256 3ccb3bff069e9bc421f68acf4b1777030d9ed9959ffe21fac4416ef208d0d8e9.
DLL build and StoryMaterialOutputImageTest, StoryMaterialImageTest,
StoryRangedProjectionTest, LanCastContextTest and SkillTraceImageTest pass.
The named STEP/step_prefix guard and material getter RVAs were checked; exact
native guard bytes were not relaxed. Synthetic/exact-image checks do not prove
rendered Spirit effects, frame improvement or a successful ordinary Skill.

**Owner gate remains:** repeat Tal's Spirit in combat and compare the summon,
strike and recovery on both screens, including smoothness and restored control.
Also retain the ordinary Skill check with sufficient SP from the preceding
entry. C3 remains open and C4 has not started.


### 2026-10-10 — C3 follow-up: Spirit cosmetic transport and transition cost candidate

**Owner evidence:** the preceding candidate still started slowly and showed
Spirit effects only on the host. The prior movement freeze remains resolved by
owner report. No new ordinary Skill success with sufficient SP was supplied.
C4 has not started.

**Changed:** saved-story now passively captures Tal's native Spirit emissions
through the existing shared effect observer and sends an additive cosmetic
message. The existing client renderer replays the closed resource kinds with
host transforms, phase, stable instance identities and positive retirement.
The message is fenced by connection, scene, area, stream and sequence; unknown
or missing observations never mean an empty roster. Native client creation,
retirement and cache release use the existing residency pre/postcondition seam.
No client Spirit manager, gameplay script, damage or SSP activation is added.
Existing packet layouts and protocol profile are unchanged; older peers ignore
the optional message. Other characters' Spirit effects are not claimed here.

The owner explicitly authorized the previously rejected file change: task
namespace entry and exit each prove the full native roster once, reusing that
proof only within their synchronous transition. The proof is cleared before
the original script executes; exit must acquire a fresh proof. Existing actor,
generation, component, native global and byte checks remain. Regression tests
cover wrong ownership/thread, recursive calls, entry/exit failure and identity
loss between entry and exit. Split entry/native/exit timing is now bounded in
the research diagnostic; actual smoothness still requires the owner.

Material control argument0/member3 was the observed rejection. Exact retail
research establishes that an absent shader constant can return NULL without
allocation when its authored two-bit field is already nonzero. Admit only that
proved branch (both selected and original material); retain refusal for shader
definition replacement. The real getter is tested on inert materials for all
four shader members and all nonzero field values. Whether the reported live
material takes this branch remains to be checked in the new run.

**Validation:** dirty decbbc5, DLL SHA256 912bbcf1d81c82390636ca9fbab1ebbe408f87cfc0d2fd7acd8d096c11d16b04.
Ten focused suites passed: StoryCastAdmissionTest, StoryMaterialOutputImageTest,
StoryMaterialImageTest, StoryEffectTransportTest, LanCastContextTest,
LanArenaSpiritVisualHostTest, LanArenaSpiritVfxTest, StoryResidencyCallTest and
SkillTraceImageTest and StoryVfxDispatchTest. Named native checks include D3DMaterialGetProperty202E40,
BaseMaterialGetProperty1E6ED0 and ShaderConstantAbsent2032C0/20347F/20348C.
The transport fixture now avoids publishing duplicate GetTickCount observations;
production's strict scene clock was unchanged. Evidence is CONFIRMED_TEST and
CONFIRMED_EXACT_IMAGE, not rendered gameplay acceptance.

**Owner gate:** repeat Tal's Spirit Strike and compare summon, strike and end
on both screens; confirm startup/recovery speed and that movement returns.
Retain the sufficient-SP ordinary Skill check. Do not start C4 yet.


Startup follow-up for this candidate: host publication was observed, then the
client correctly rejected a presentation-only paused-roster query made from the
controller seam. Corrected the integration to use that dispatch's fresh native
scene and to acquire the retained roster/pause proof inside ClientResidencyCall
with its controller witness. StoryVfxDispatchTest exercises the actual runtime
helper with fake native/transport APIs: scene/area refusal, missing/unknown
rosters, witness propagation, failed cleanup and retry all pass. This is a
boundary integration correction, not a relaxation of the native pause proof.
Spirit state publication is independent of the unrelated sky/spawn snapshot.


Final startup verification: both roles loaded the candidate hash above. Host
cast ownership bound; the client reached input_ready=1 and received the empty
host visual roster, with native playback/adoption reporting success. This
proves startup wiring only. No Spirit cast or visual/smoothness acceptance was
performed by the agent; the owner gate remains pending. A prior delayed reload
refused cast initialization at cast_lighting and was preserved as a failed
startup, without changing its native guard.


### 2026-10-10 — C3 owner feedback and repeatable cast testing

**Owner feedback:** the client still lags during the cast phase, then the
strike appears. Startup and recovery smoothness are not accepted; C3 remains
at its owner gate and C4 has not started.

At the owner's request, reloaded the same candidate at the pre-trigger
checkpoint for repeatable tests using the existing host F8 menu. Combat Mode,
Infinite SP and Infinite Spirit (SSP) each received native enable confirmation;
Tal's SP refill was also confirmed. These are session testing options, not a
change to normal gameplay defaults. No source or DLL change was needed.
Repeat Tal's ordinary Skill Strike and Spirit Strike before entering the
ambush, comparing the cast phase, effect appearance and return to movement.

### 2026-10-10 — C3 startup animation and Spirit camera investigation

**Owner feedback:** Tal's client model stops instead of animating the start of
the strike; client casting lags and appears slower than the host; authored
Spirit camera angles do not appear for the client caster.

**Findings:** startup material preflight rejects Tal's native shader-definition
replacement and consequently holds the whole world presentation. The prior
safe NULL-output allowance does not cover this branch. Cast namespace
transitions still have substantial measured overhead. The client Spirit
camera playback path is absent; existing playback covers the effect roster
and regular character view. These are separate problems, not a completed
camera or performance fix. See the same-date C3 investigation in
research-log.md and the material contract in engine-functions.md.

**Status:** investigation only, current DLL and testing settings retained. No
new visual acceptance, C3 owner gate stays open and C4 is not started. The next
candidate needs startup/recovery comparison and clear/obstructed native Spirit
camera cases, with return to the client's normal view after completion.


### 2026-10-10 — C3 startup freeze / transition overhead candidate

Implemented the exact SOL shader replacement required by Tal's startup pose,
including native derived-definition cleanup, and reduced repeated OS memory
queries inside each Spirit namespace transition. All native identity and
ownership checks remain; no validation is carried across script execution.
Eleven focused suites passed, including the named exact native byte checks,
retail shader creation/release/retry and failure/recovery tests. The transition
fixture drops from100 to12 memory queries per entry/exit pair; live speed is
not inferred from that count. See the same-date research-log / engine-functions
entries. Dirty decbbc5; candidate DLL SHA256
`1c485e64a67fbafdc920a716d01127f9b5abcd81df0c81c8dcdeffff72154120`.

**Owner gate remains open:** compare Tal's startup animation, summon/strike
visibility, recovery speed and movement after completion on both screens.
Repeat once to check cleanup. Keep the sufficient-SP ordinary Skill check.
Client native Spirit camera playback remains a separate missing path; it is
not implemented or claimed fixed here. C4 is not started. Existing uncommitted
work is preserved; no baseline commit or reset was performed.


Candidate startup verified: both roles loaded the tested DLL; host cast
ownership bound and client input reached ready. Existing host Combat Mode,
Infinite SP and Infinite Spirit options were enabled and confirmed for this
session. The owner began a cast during setup; logs show its native progression,
but no visual acceptance was provided. The tools menu is closed and client
focus restored. Keep the C3 owner gate open.


### 2026-10-10 — C3 owner rejection and next startup/recovery candidate

The owner still sees frozen arms at startup, a stepping/clipping summon,
4–5 seconds of recovery after Tal reappears and missing ending effects.
Prior candidate is not accepted. The startup refusal moved to member4;
preflight now proves the whole closed member3..5 SOL family. Summon snapshots
now follow the controller cadence instead of the100ms area branch. Movement
and basic melee admit positively drained native body/tasks while lighting
finishes; new casts and cleanup still require full drain. A bounded summon
phase trace supports the still-open ending-effect investigation.

Dirty decbbc5; DLL SHA256 `cdc3ad8cbcfcc706812234728d54096ad097d022f1bc25947e0ef4a5667e2e62`.
Build and12 focused suites passed, including named native byte checks and
all six material output orders with native remapping/release. See the dated
research-log entry for evidence and limits. No visual acceptance is claimed.

**Owner gate:** repeat Tal's Spirit Strike twice and compare the arm-opening
pose, summon smoothness, ending effects and movement recovery on both screens.
Retain the sufficient-SP ordinary Skill Strike check. Client native Spirit
camera playback remains absent. C3 stays open; C4 has not started. Testing
uses the requested Combat Mode, Infinite SP and Infinite Spirit session options.


Candidate startup verified: both roles loaded the tested DLL, host cast
ownership bound, and client input plus empty visual-roster playback reached
ready. Combat Mode, Infinite SP and Infinite Spirit were enabled through the
existing host menu and visually checked. Client focus restored; the owner
began a cast during setup. The last host screenshot still showed the tools
overlay, so no further setup input was sent while the owner tested.
Findings were saved to the local graph, the AST index updated, and the new
entry recalled. These are startup/testing checks, not cast visual acceptance.


### 2026-10-10 — C3 opening freeze persists: textured-material candidate

Owner rejects startup parity: Tal pauses during the opening on the client,
while the host keeps animating. Logs show incoming frames continue, but a
second material (token5, flags80000101) holds presentation through selector75.
The previous closure omitted its authored texture field.

The next candidate preserves authored texture fields while admitting native
constant expansion only into empty fields. Exact-image tests cover eight
original texture layouts, all six output orders and27 reachable definitions,
including texture/color preservation and native cleanup. Build plus
StoryMaterialImageTest, StoryMaterialOutputImageTest and SkillTraceImageTest
passed; all eleven named shader byte guards still reject mutation.
Dirty decbbc5, DLL SHA256 `86f1df631bd268533d099d4c7268dc9f745dbcc9dcacc5ae26e37dae841bc850`.

**Owner gate remains open:** compare the entire opening arm animation on
both screens, repeat a second Spirit Strike, and retain the summon/end/control
checks. Native client Spirit camera playback is still absent. C4 is not
started. This entry records a tested candidate, not visual resolution.


The immediate live test rejected the 27-definition candidate too: another Tal
material uses flags80800501. The owner confirms that Tal and the whole client
world stop together and resume at the next animation phase. Host frames keep
arriving; atomic client presentation is refusing the opening material graph.

The replacement candidate additionally proves the native alternate layout
and extra authored textures, preserving them during color expansion. Tests now
cover64 original layouts,216 definitions and six output orders each, with
optional-array ownership and complete native cleanup. The three focused
suites and named native byte checks pass. Bounded full-material inventory
prevents the first rejected output from hiding the other authored layouts.
Dirty decbbc5, DLL SHA256 `fd3971b332e40d1a7f43a5af38152e77cb1ce5af8f146b54e08840b08c1adc99`.
Owner gate stays open: confirm Tal AND the world keep animating throughout
startup on the client, then repeat once. Full ending/camera parity remains open.


Full inventory then identified the last remaining bit13 case at token1:
flags80802505, member4. That retained flag is now proved as well. Final tests
cover128 original layouts,432 definitions and all six output orders; every
one of Tal's seven observed originals is covered. The three focused suites
and eleven named shader byte guards pass. Current dirty decbbc5 DLL SHA256
`6de2a94f8ed64d817f30fed2196038af2d7748c922b757f6b014ce0eb180bf21`. The216-definition intermediate candidate was
not accepted. The same Tal-plus-world opening-motion owner gate remains open.

Final candidate startup and two owner-triggered Spirit attempts reached live
diagnostic verification. Both opening phases lasted about 5.25 seconds; all ten
sampled client presentation records during those intervals report successful
world application, with incoming frames advancing and no material-graph
refusal. Both roles loaded the tested DLL. Combat Mode, Infinite SP and
Infinite Spirit are enabled; the host tools overlay is closed and client
focus restored. These observations establish continued frame application,
not visual smoothness or complete Spirit parity. Owner confirmation of Tal
and world motion remains pending; C3 stays open and C4 remains untouched.

**Owner follow-up:** opening freeze resolved; the Spirit animation now appears
on both client and host and is substantially smoother. Opening-motion acceptance
is complete. The owner still observes lingering effects at the end; investigate
their lifetime separately. Full C3 acceptance and client Spirit camera parity
remain open. C4 has not started.

### 2026-10-10 — C3 ending-flash candidate

Owner clarification: light/particles around returning Tal remain bright,
then cut out instead of fading. Exact native timing research confirms a
client loop versus host one-shot mismatch for Tal's observed summon.
The candidate uses native one-shot renderer playback while retaining the
existing host-owned effect lifetime. The native fixture reproduces the old
wrap and proves terminal sampling after the correction; four focused checks
pass, including three named full native routine guards with mutation refusal.
Dirty `decbbc5`, DLL SHA256
`91d02523267ca3912efed41f6d70415576862d0ec8983545aec4d08a45845d1d`.

**Owner gate:** compare the return flash on both screens: it should fade
through its ending without replaying the bright beginning or cutting out.
Repeat a second Spirit Strike, retain the accepted opening motion, and check
movement afterward. Ending acceptance is pending. C3 remains open; client
Spirit camera parity and the sufficient-SP ordinary Skill check remain open.

Candidate live diagnostics: two owner-triggered casts reached native one-shot
completion `0x41`, held sample `219.999893` through the host's ending hold,
applied no backward end seek, and retired once each. No material-graph refusal
was recorded. Both peers loaded the tested candidate, all three test options
are enabled, host overlays are closed and client focus restored. These are
native timing/cleanup observations; owner confirmation of the visible fade
is still required.


### 2026-10-10 — C3 ending light accepted; client Spirit camera next

CONFIRMED_LIVE owner acceptance: the ending light is now good after the
owner replayed the sequence through its end. This closes the visible
return-flash fade gate on the tested dirty `decbbc5` candidate, DLL SHA256
`91d02523267ca3912efed41f6d70415576862d0ec8983545aec4d08a45845d1d`.
Opening animation and both-peer effect visibility remain accepted. The
owner also asked whether the ending switched scenes or reloaded; the agent
did not observe that event, so its nature is UNKNOWN.

The owner authorizes client Spirit Strike camera work next. C3 remains
open for native camera selection/obstruction parity and restoration to the
client's ordinary view, plus the sufficient-SP ordinary Skill check. C4
has not started.


### 2026-10-10 — C3 client Spirit camera candidate

Owner selected camera parity after accepting the ending light fade.
CONFIRMED_STATIC: the host's private Tal camera already follows native
selection, while the saved-story client always presented its orbit view.
The candidate observes that retained private camera and sends its geometry
only to Tal's acknowledged owner. Selection/end changes follow the same
presented world timeline; native obstruction choices remain authoritative.
The client preserves its orbit, holds unknown camera observations, and
restores on an observed ending or confirmed presentation-owner change.
Host cutscenes take priority through an explicit restoration before borrowing
the view. See the native identities and transport/lifecycle limits in
`docs/engine-functions.md`, Remote Tal Spirit camera observation.

Dirty `decbbc5`, DLL SHA256 `a2d4b18918737674372b00c08d0fbd715745a2565298eddce9dccc7e602cc1f1`.
Build and seven focused checks pass: SpiritInstanceAbiTest,
StoryCastViewTest, StoryCastViewNativeTest, StoryVfxDispatchTest,
StoryActionTransportTest, StoryEffectTransportTest and SkillTraceImageTest.
Five new native instruction checks were verified by name/RVA and every-byte
mutation refusal. Tests cover interpolation/cuts, no early start/end, packet
loss/reordering, stale/wrong control/actor leases, scene revision changes,
foreign or read-only camera restoration with retry, repeated casts and Quit
destruction. Initial fixture gaps (registry membership, scene heartbeat rules
and C structure padding comparison) were corrected without relaxing native
or transport guards. These establish CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST,
not rendered camera acceptance.

**Owner gate:** cast Tal's Spirit Strike in open space and compare the authored
camera movement with native behavior; repeat near an obstruction where native
play suppresses the cinematic. Check both start and ending transitions, return
to the previous orbit heading/distance, movement afterward, a second cast and
the following host cutscene. The host player's camera should keep its own
view. C3 remains open, including the sufficient-SP ordinary Skill check; C4
is not started.


Camera candidate startup verified: both peers loaded the tested DLL, host
cast ownership bound, and the client reached input-ready state. A native
default-camera observation was captured on the host and received/applied on
the client. Combat Mode, Infinite SP and Infinite Spirit are enabled. Host
overlays were visually confirmed closed and client focus restored. This is
startup/baseline evidence only; no cinematic camera visual acceptance is
claimed. Stop at the camera owner gate above. The new findings and prior
ending-light acceptance are saved in the graph and recalled successfully.


### 2026-10-10 — C3 client Spirit camera accepted; ending spiral follow-up

CONFIRMED_LIVE owner acceptance on the dirty `decbbc5` camera candidate
(DLL SHA256 `a2d4b18918737674372b00c08d0fbd715745a2565298eddce9dccc7e602cc1f1`):
client Spirit Strike camera angles are correct, including the native edge
guard. This closes the owner camera-angle/obstruction gate for the tested
Tal saved-story route. It does not establish untested disconnect behavior.

The owner separately reports flicker in the ending pair of lights spiraling
down to the ground. That effect remains unresolved; the previously accepted
return-flash fade remains accepted. Preserve the live run before investigating
playback and particle clocks. The apparent host map change at combat end has
not been visually observed by the agent; examine scene/area/cinematic evidence
before attributing it to a reload. C3 remains open, including the sufficient-SP
ordinary Skill completion/control check. C4 has not started.


### 2026-10-10 — C3 ending spiral particle-clock correction

CONFIRMED_LIVE diagnostic evidence on the accepted camera candidate: the
owner confirms flicker only on the client. A read-only paired capture of one
owner cast sampled 262 summon observations per peer. Both main renderers
reached one-shot completion at `219.999893`. The host's custom particle
clocks held that sample; 21 client observations contained a near-full-duration
particle delta after wrapping to approximately `0.6` and returning to the
held end. The client fallback custom clock mistook an intentional model hold
for a stopped clock. Native model playback then supplied the ending sample
again. This is distinct from the earlier corrected main-renderer loop.

The correction collects only native-driven particle instances under retained
Tal summon weak leases, with exact root, child, resource slot and BONA timing
ownership. The main manager independently confirms unique membership before
excluding those instances from the supplemental custom clock. Native particle
aging, host-owned lifetime and unrelated custom clocks remain in place. The
list is freshly observed and consumed on the same verified game-thread seam;
it is never retained. Failed ownership is unknown and rejects that cosmetic
advance. No new hooks or protocol changes are introduced.

CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST: five focused checks pass:
SpiritVisualTimingImageTest, StoryEffectClockImageTest, LanArenaSpiritVfxTest,
SkillTraceImageTest and StoryVfxDispatchTest. The native fixture reproduces
the particle wrap/jump and verifies stable held samples after exclusion;
independent clocks, invalid/duplicate ownership, resource mismatch, weak
destruction, retained retirement and unchanged outputs on failure are covered.
Four added byte checks are tested by name/RVA and every-byte mutation:
VisualChildClockDispatch `2228D0`, ParticleBoneTimeBinding `1F9F70`,
ParticleBoneQuery `1FA070`, ParticleModelTimeSet `231860`. Existing timing
guards remain tested. Dirty `decbbc5`, DLL SHA256
`ff13b600dd2da7fa4b3792fe3fe2394e230418511348df4ad8e0880688af381c`.

The reported combat-ending map event is now narrowed by saved diagnostics:
the host briefly enabled letterbox/cinematic mode and returned to normal
within roughly half a second; the client followed that transition. Recorded
area and scene epoch did not change and no additional load was recorded.
This supports a brief scripted camera transition, not a recorded map reload;
the agent still did not visually observe the original event.

**Owner gate:** replay the ending on both screens; the descending pair of
lights should stop flickering, retain the accepted fade and camera, and clear
with the host. Repeat once and check ordinary hit particles still expire.
Visual acceptance of this correction is pending. C3 remains open, including
the sufficient-SP ordinary Skill completion/control check. C4 has not started.


### 2026-10-10 — C3 spiral accepted; ordinary skills and melee presentation

CONFIRMED_LIVE owner acceptance: ending spiral flicker is gone on the tested
dirty `decbbc5` correction, DLL SHA256
`ff13b600dd2da7fa4b3792fe3fe2394e230418511348df4ad8e0880688af381c`.
The owner also confirms ordinary skills now fire. This does not close their
full parity gate: Shin Splitter lacks dust and sword trails on the client;
ordinary primary/secondary swings and combos also lack client sword trails.
The initial report placed the missing directional/range cone on the host;
the owner subsequently clarified it is missing on the client. Tal cannot
move during the short skill aiming window. Investigate shared native presentation and
phase-specific input ownership so other skills inherit the proper behavior.
C3 remains open; C4 has not started. Owner handles game commands and controls.


CONFIRMED_LIVE observation, same pre-candidate build: the owner clarified
that the missing orange cone is on the **client**, superseding the earlier
host-side report. The owner selected Shin Splitter from Q and confirmed it
fires. Passive paired observation recorded host trail accumulation and bounds
changing while the client's remained unchanged during replicated swing clips.
The host created the Shin Splitter dust and aiming arc; neither was observed
on the client. This proves the missing native presentations in that capture,
not a visual acceptance of the correction below.

CONFIRMED_STATIC correction: the contained client gives body children zero
delta, freezing native trail history independently of replicated body pose.
The existing ambient adapter now changes only the native trail child's zero
delta after a fresh same-frame actor/model/registry/child/resource proof.
It leaves the body animation and gameplay step contained. Separately, exact
ordinary-cast lineage and two verified resources now enter the story cosmetic
stream: Shin Splitter dust and its attack-zone arc. Client clones use the
existing isolated listener, host transforms, one-shot playback and host
retirement. Completed model-driven particle clocks retain the accepted
spiral hold behavior. LA42/SMP4 packet layouts and kind admission are unchanged.

Remote movement now follows the shared native ordinary-skill AIMING phase
with positive remaining time. Other skills using that phase inherit this
movement rule. Native execution flags and strict cast/drain ownership remain
in force; other skill-specific visuals still require closed resource admission.

CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST: the candidate builds with MinGW.
Eleven focused executable targets pass under Wine: StoryTrailClockImageTest,
StoryTrailOwnerImageTest, StorySkillVfxCodecTest, StoryCastAdmissionTest,
StoryMeleeAdmissionTest, StoryVfxDispatchTest, LanArenaSpiritVfxTest,
SpiritVisualTimingImageTest, LanArenaSpiritVisualHostTest, LanArenaProtocolTest
and LanPartySessionTest (each prefixed `SudekiMP.` in the build).
The image tests use the supported executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
The two new checks are verified by name/RVA plus every-byte mutation:
TrailUpdateEntry `225760` and TrailChildDispatch `222A50`. The actual x86 shim
passes inert stack/register argument tests. Rejected ownership, phase-bound
movement, ordinary synchronous emissions, codec isolation, effect lifecycle
and model-driven particle holds are covered. Existing protocol regressions
pass. In-game hook teardown/retry and visual parity of this candidate are
not yet tested.

Candidate: dirty `decbbc5`, DLL SHA256
`e8259d3dfa392749b083a3390f14b2eb17ae44fa724e1a8099655cb71d62ec45`.
The candidate is saved separately; the running pair was not reloaded or
changed. No game input was injected. The owner retains commands and controls.

**Owner gate:** after loading this candidate on both peers, compare M1, M2
and a combo for sword trails. Select Shin Splitter while armed and idle:
compare its dust, sword trail and orange aiming arc, and walk/turn during its
short aiming window. Check that the arc follows Tal, clears with the host,
and execution/control recovery still match. Repeat the skill once; then
confirm the accepted Spirit Strike camera, ending fade and spiral remain
unchanged. C3 stays open until the owner accepts this pass. C4 has not started.


### 2026-10-10 — C3 Shin Splitter accepted; ordinary skill camera candidate

CONFIRMED_LIVE owner report: Shin Splitter fires and the previously reported
effect/aiming issues are fixed. Both staged peers carry the skill-presentation
candidate, dirty `decbbc5`, DLL SHA256
`e8259d3dfa392749b083a3390f14b2eb17ae44fa724e1a8099655cb71d62ec45`.
The owner now requests ordinary skill cinematic angles with the same native
obstruction/edge behavior accepted for Spirit Strikes. This acceptance is
specific to the reported Shin Splitter test; it is not a new blanket claim
for all weapons, skills or maps.

CONFIRMED_STATIC root cause: the host already routes ordinary skills through
the retained native named-camera namespace, but story camera capture was
explicitly limited to `spirit_started`. It published normal-camera kind 0
for an ordinary skill even when the native observer selected InitCam or
SkillCam. A new regression fails on that exact case before the correction.

The correction permits either retained ordinary-skill or Spirit activity to
publish the observed native selection. It preserves native obstruction/default
choices, host-owned start/end timing, camera geometry and the existing client
orbit restoration. It does not choose angles from a cast timer. This is the
shared Tal skill path, not a Shin Splitter exception. Existing message 37,
actor/scene/control fences, UNKNOWN handling, cutscene priority and host
camera ownership remain unchanged. No new native hooks or byte guards.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: five focused targets pass under Wine
following a successful MinGW build: `SudekiMP.StoryCastAdmissionTest`,
`SudekiMP.StoryCastViewTest`, `SudekiMP.StoryCastViewNativeTest`,
`SudekiMP.SpiritInstanceAbiTest` and `SudekiMP.SkillTraceImageTest`.
The new fixture checks ordinary and Spirit default/InitCam/SkillCam selection,
geometry, completion, recast, changed/unknown ownership, invalid geometry and
unchanged outputs on refusal. Existing checks cover timeline cuts, transport,
orbit restoration with retry and native camera ownership. The supported
retail image SHA256 remains
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Existing native checks passed by name/RVA and every-byte mutation:
`CCameraCtor.RenderStateSize` E72BF, `CCameraCtor.RenderStateAllocate` E72D1,
`CCameraCtor.RenderStateConstruct` E72DD, `CCameraCtor.RenderStateResult` E72E2,
and `CCameraCtor.RenderStateStore` E72FD.

New candidate: dirty `decbbc5`, DLL SHA256
`32239b59ddd0acdda4a20fa36412d2a61b5614f8ef73f71a76c4ecee625dfe4b`.
Saved separately with an owner-run reload helper. The agent did not reload,
stage into the running pair or inject game controls. In-game appearance and
live teardown/retry of this candidate remain untested.

**Owner gate:** load the candidate on both peers, then cast Shin Splitter in
open space and near an obstruction/edge where native play suppresses the
cinematic. Check the authored skill angles, native fallback, return to the
previous camera heading/distance, aiming movement and effect cleanup. Repeat
once; verify the accepted Spirit camera/edge behavior and the host player's
independent view remain intact. C3 remains open; C4 has not started.


### 2026-10-10 — C3 Shin Splitter camera accepted; roster coverage clarified

CONFIRMED_LIVE owner report: the ordinary-skill camera correction worked for
Shin Splitter. This accepts the reported test of the dirty `decbbc5` candidate,
DLL SHA256 `32239b59ddd0acdda4a20fa36412d2a61b5614f8ef73f71a76c4ecee625dfe4b`.
It does not establish every other skill, obstruction case, character or map.

CONFIRMED_STATIC coverage: the camera gate applies to ordinary skills and
Spirit Strikes, with no Shin Splitter slot check. Other Tal skills using the
same native InitCam/SkillCam selection path inherit it, but their live results
remain UNKNOWN. Story capture selects `caster[2]`; camera packet validation
and host publication also require character 2. Ailish, Buki and Elco are
therefore not covered by this client camera replication feature. Extending
capture, validation and per-owner routing and testing each character remains
necessary before claiming full-roster camera parity. No implementation scope
was changed during this clarification; C3 remains open and C4 has not started.


### 2026-10-10 — C3 full-roster Skill and Spirit camera candidate

Owner priority: extend the accepted native cinematic camera behavior to Tal,
Ailish, Buki and Elco, for both ordinary Skill Strikes and Spirit Strikes.
This pass extends camera coverage in the saved-story direct-IP profile. It
preserves each skill's authored choice of cinematic or normal camera; it does
not add a cinematic to a move that has none. Other characters' effect assets
and all individual skills' live behavior are not established by this pass.

CONFIRMED_STATIC: the prior Tal-only restriction existed in three places:
retained cast capture, camera-message validation and host peer publication.
Capture now takes a character and observes that character's retained native
camera bank, cast activity and sequence. It refuses an absent/stale actor and
the host-local character. Runtime publication visits each roster slot
independently, so an unknown or missing observation cannot suppress another
character's camera. Both cast families use the same path.

Camera packets accept the existing valid character range. The host sends each
observation only to the ready peer whose acknowledged control fence owns that
character. It cannot relabel one character's camera for another player. Scene,
lease, actor-generation and control-transaction fences remain in force.
Native obstruction/default selection, unknown-observation holds, host-cutscene
priority and prior-orbit restoration remain unchanged. No native hook, byte
signature, global ownership rule or wire layout was added or changed.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: MinGW builds the candidate and seven
focused executable targets pass under Wine: `SudekiMP.StoryCastAdmissionTest`,
`SudekiMP.StoryCastViewTest`, `SudekiMP.StoryCastViewNativeTest`,
`SudekiMP.StoryVfxDispatchTest`, `SudekiMP.SpiritInstanceAbiTest`,
`SudekiMP.SkillTraceImageTest` and `SudekiMP.LanPartySessionTest`.
Coverage includes every roster character with both cast families; per-owner
native default/InitCam/SkillCam observation; sparse/unknown captures; three
simultaneous client camera streams with every character as host; independent
end/restore; stale fences; release and reacquisition of ownership; restoration
refusal/retry; and private native camera namespaces with no host-camera writes.
The ownership test reacquires the same character; live character reassignment
is not claimed. The loopback fixture was corrected to respect the existing
scene heartbeat tick/revision contract; no production validation was relaxed.

The supported retail executable SHA256 is
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Existing native checks pass by name/RVA against that image and every-byte
mutation: `CCameraCtor.RenderStateSize` E72BF,
`CCameraCtor.RenderStateAllocate` E72D1,
`CCameraCtor.RenderStateConstruct` E72DD,
`CCameraCtor.RenderStateResult` E72E2 and
`CCameraCtor.RenderStateStore` E72FD.

Candidate: dirty `decbbc5`, DLL SHA256
`6c392e1e0a91f48816d428949e92c69178bd644fafd4dff1659968f5f55b78c0`.
It is saved separately with an owner-run reload helper. The agent did not
reload or stage into the running pair, inject game controls, or change the
modmenu. Automated evidence is not full-roster visual acceptance. Live
teardown/retry, transitions and character reassignment remain untested for
this candidate.

**Owner gate:** load the candidate on both peers. For each available roster
character, use an ordinary skill and a Spirit Strike in open space and near
an obstruction/edge where native play suppresses its cinematic. Compare the
native camera choice, timing and return to the previous orbit; repeat a cast
and check that the host player's view stays independent. Skills without an
authored cinematic should keep the normal view. Check camera restoration
across a later area/owner change when that route is available. Record each
character and move tested; untested moves remain UNKNOWN. C3 remains open;
C4 has not started.


### 2026-10-10 — C3 roster camera and attack-effect verification tracked in #57

At the owner's request, opened [issue #57](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/57)
to check Tal, Ailish, Buki and Elco across ordinary Skill Strikes, Spirit
Strikes, regular M1/M2 attacks and combos. It records camera/fallback checks
separately from effect visibility, timing, targeting markers, trails,
projectiles, impacts and cleanup on the casting client, host and other
observers. Individual moves and weapons must be recorded; untested or locked
moves remain pending.

CONFIRMED_STATIC: the latest extension changes camera coverage. Effect
replication has a separate closed resource set, including shared and named
Tal/Buki/Elco Spirit assets plus Shin Splitter and its targeting arc. Existing
asset support and trail-clock handling do not prove complete roster effect
parity. Prior owner acceptance of the specific Tal tests is retained; this
issue is not full-roster visual acceptance. No implementation, running game,
DLL or modmenu was changed in this issue-recording pass. C3 remains open;
C4 has not started.


### 2026-10-10 — C3 native single-player comparison prepared

The owner requests a logged base-game comparison: load New Brightwater,
place the party at the same pre-ambush spot, and let the owner start combat.
Added default-off `[StoryAreas] NativeResearch54=true` in the loader. This
exclusive launch profile installs the existing service-only controller observer,
read-only scene/roster and Research54 observers, and the existing one-time test
placement. It starts no lobby or network session and installs no remote-control
leases, C1/C2/C3 gameplay adapters, cast-routing or client presentation. Combat,
party leadership and movement remain native. Save files are copied into a
separate runtime prefix; the original saves and multiplayer pair are unchanged.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: MinGW build and
`SudekiMP.SkillTraceImageTest` pass, including the existing service-only
observer/lifetime tests and named native checks. No new patch seam or byte
signature is introduced. The first startup was refused before gameplay because
area-follow initialization checked `RVA_SWITCH_ZONE_NOW` (7990) and
`RVA_ENTER_ZONE` (7970) after the scene observer had hooked them. Initialization
now checks those native entries before installing their observer, matching the
established runtime order; guards remain intact and the failed run is retained.

CONFIRMED_LIVE startup only: the corrected candidate reports
`profile=native_research54 network=false party=native movement=native`, and
Research54 plus scene observations are arriving at the native title screen.
Dirty `decbbc5`, DLL SHA256
`60c60d5cc109ccd0d627de455a8aa6493717aa5a4203202ac26cf109522a61d4`;
supported executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
The owner loads the save and triggers combat. Post-load placement, combat
comparison and live teardown/retry are not yet accepted. C3's movement gate
remains pending; C4 has not started. Full-roster camera/effect checks remain
tracked separately in issue #57.


Native comparison follow-up: the owner loaded New Brightwater. Native logging
paused while the game lost focus to the chat; after focus returned, the target
area became resident and the existing test placement reported two actors
placed with zero refusals. This confirms post-load placement and continuing
instrumentation, not combat parity. The owner now controls combat. Native
leadership initially remains Ailish as stored in the save; Tal should be
selected through the game's normal controls for the remote-Tal comparison.

### 2026-10-10 — C3 native combat comparison; movement differences identified

CONFIRMED_LIVE instrumentation: the native-only candidate above captured
approximately 181 seconds of combat with Tal selected. Excluding the scripted
intro and mixed summary windows, Tal has zero observed AI speed resets. A
62.199-second later combat sample records 3,719 native controller movement
calls while attacks are processed. Native movement requests include speed
parameters 1.0, 1.5 and 1.8; these are not measured world speeds.

CONFIRMED_STATIC: the current remote movement adapter still suppresses input
when its body-idle gate fails, caps the submitted speed at 1.0, and uses an
immediate stop. The skill-aiming exception does not establish ordinary melee
movement parity. These differences belong with C4's native movement work;
native speed-selection conditions still require exact-image research.

An older co-op melee capture supports C3's AI suppression: two complete
summary windows record 232 bypasses and no AI speed resets, while movement
submission stops during the sampled attack sequence. It predates the latest
presentation changes and is not a matched replay. Scripted and Spirit periods
must not be counted as unwanted AI interference or general melee holds.
The current co-op log contains no combat for a fresh comparison. No visual
parity, input-latency figure, or C3 owner acceptance is claimed. C3 remains
open; C4 has not started. No gameplay source or running game was changed.

### 2026-10-10 — C4 native movement candidate; owner gate pending

The owner asks to fix the differences from the native comparison. C4 is now
authorized and prepared; C3 is not retroactively marked accepted. Dirty
`decbbc5` plus preserved prototype, candidate DLL SHA256
`711bdab26c99a2eb58c87f25327f5bb3a8e2019a71c94fe47487654ce9564cf3`.

Ordinary attack movement reaches native Move/CanMove each admitted frame
instead of being held merely because an attack is active. Native combo
restrictions remain. Key release uses blended native SetSpeed; lifecycle
stops retain their separate behavior. Native input scaling and lateral
shaping survive client sampling, camera transformation, transport and host
submission. Target refresh still gets unit direction. The saved-story
gameplay profile is now 20; both peers need this candidate. Packet layout,
save format and arena protocol are unchanged.

**CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE:** build and eight focused tests pass:
`StoryPlayerMovementImageTest`, `StoryMeleeAdmissionTest`,
`StoryActionTransportTest`, `LanPartySessionTest`, `SkillTraceImageTest`,
`StoryPlayerTurnImageTest`, `StoryAiMovementImageTest`, `StoryQuickInputTest`.
The new image test executes the actual native controller for 22 input cases,
checks native combo/cast arbitration and blended stopping, and verifies all
eight named C4 byte contracts and mutation refusals. See the research log for
the detailed proof and boundary corrections. No live visual acceptance or
held-share percentage is inferred from these tests.

**Owner gate — pending:** on matching host/client candidates, compare Tal with
the native run: move and reverse direction through M1/M2 and a combo, release
movement while moving, and check walk/run and diagonal speeds. Some native
combo phases should still restrict translation. Check Shin Splitter aiming
movement and Spirit Strike recovery, and that the host view stays independent.
Capture a sustained moving fight to compare Research54's new
`held_moving_frames / moving_frames` measure, alongside C3 AI speed/snap
diagnostics. C1's numeric facing checks remain in #56; full-roster
camera/effect acceptance remains in #57. Beast explosion-position and delayed
enemy-attack checks remain outstanding.

Candidate and pass-specific change/test records are saved locally. The owner
retains game commands; no reload, teleport, input or staging was performed.
No commit, baseline commit, reset, stash or unrelated staging. Stop here for
the owner gate; do not begin C5 or mark C3/C4 complete from build results.

C4 reload follow-up: the owner requested the reload. Both multiplayer roles
now run the candidate above with their existing configuration preserved.
Both report successful saved-story startup; the client reaches
`host_confirmed_movement`, character 2 (Tal), `input_ready=1`. Prior logs and
DLLs were retained, and both windows returned to their previous positions.
**CONFIRMED_LIVE startup/control handoff only.** No combat input was injected;
C3/C4 visual acceptance and the held-share comparison remain pending.

### 2026-10-10 — C4 owner run: movement admission matches; smoothness unresolved

On the C4 candidate above, the owner's encounter spans approximately 208
seconds in the sampled combat-state log. Excluding the scripted intro and
the ending Spirit Strike, 36 complete input windows cover 180.296 seconds:
9,525 fresh moving-input frames, **zero mod-held moving frames**, and zero
stale-input frames. Complete call-summary windows in the ordinary-combat
interval contain 9,320 remote Move submissions and 9,997 AI-movement bypasses,
with no Tal movement-AI speed resets, AI turns or AI snaps. Input and call
summary windows have different boundaries; their totals are not one-to-one.
Requested speeds include 1.0, 1.5 and 1.8, matching the native comparison's
observed values. **CONFIRMED_LIVE instrumentation** supports the C3 AI gate
and C4 removal of extra attack holds; native CanMove still owns translation.

Smoothness is not accepted: during that interval, one-second samples of the
native frame counter give median update rates of approximately 56/s on the
host and 35/s on the client. These are update-counter measurements, not
rendered FPS, matched animation/position measurements, or proof of the
slowdown's cause. The separate native comparison game remains open. The
Spirit Strike activates and reaches body-drained; this run contains no
ordinary Skill request and does not re-accept Shin Splitter or stopping feel.
No code, game input, reload or configuration change was performed for this
comparison. Raw snapshots and analysis remain private. Owner visual acceptance
is still pending; retain the client rate difference for follow-up.

### 2026-10-10 — C4 client rate investigation; validation optimization prepared

The owner authorizes investigating and improving the rate gap. Existing combat
timings show approximately 8 ms per frame in world presentation and another
8 ms in cosmetic-effect dispatch. A bounded CPU sample after combat locates
approximately 47% of client-main-thread on-CPU samples in repeated readable-
memory checks from the containment and particle modules. This is evidence of
local validation overhead, not proof that networking or rendering costs zero.

Both complete validation walks now reuse page-access answers only within one
uninterrupted read-only scope. Each object's identity and values are still
checked. The scope clears before native execution and on every failure. No
update frequency, native ownership or protocol changes. Dirty `decbbc5`,
candidate DLL SHA256
`cf66be37913a5bda489226093328ffdbff9a2b2b8a6be314c6bf485352994c53`.

Build and four focused tests pass: `StoryClientRegistryTest`,
`StoryEffectClockImageTest`, `StorySpawnTimingImageTest`, `SkillTraceImageTest`.
Synthetic page queries fall from 528 to 3 for 512 registry entries and 320 to
3 for 318 particle instances. Ownership/permission refusal, retry and existing
clock/hook checks still pass. See the research log for exact evidence limits.

**Owner gate — pending:** candidate ready for the next reload. Compare client
and host update pacing at the same checkpoint and during sustained combat,
then check regular trails, hit effects, sigils and the Spirit ending lights.
Retest an area transition when available. Query reduction is not live FPS or
visual acceptance. The running pair is still the previous C4 candidate;
no game commands or reload were performed. C3/C4 remain open; C5 has not begun.

### 2026-10-10 — Validation optimization reloaded; checkpoint improvement measured

The owner requested the reload. Both multiplayer roles now run the validation
candidate above; staged hashes match, existing INIs are unchanged, both roles
report successful saved-story startup, and Tal reaches
`host_confirmed_movement` / `input_ready=1`. Prior DLLs and logs were preserved,
and the windows were returned to their prior positions. The native comparison
game was left running. No combat input was injected.

**CONFIRMED_LIVE instrumentation:** comparing settled pre-ambush checkpoint
windows before and after the reload, median native update-counter rates are
34.4 to 74.3/s on the client and 61.6 to 60.6/s on the host. Client cosmetic
dispatch's median per-window mean falls from 6.78 to 0.39 ms; replicated-world
application falls from 7.77 to 6.90 ms. The party/world buffers stay populated.
This confirms a substantial local validation-cost reduction in this scene;
it does not measure rendered FPS or establish matched movement/animation.

The comparison uses 11 seconds before and 22 seconds after, excluding loading
and a transient startup interruption. The interruption recovered before the
measurement; its cause was not established. No new combat was captured at
this checkpoint. **Owner gate remains open:** sustained combat pacing, regular
trails/hit effects, sigils, Spirit ending lights and later area containment
still need acceptance on this candidate. C3/C4 remain open; C5 has not begun.

### 2026-10-10 — Optimized candidate: owner combat run measured

The subsequent owner encounter now supplies combat evidence for the same
candidate. Excluding the intro and final Spirit Strike, approximately 180
seconds yield median native update-counter rates of **66.7/s client, 55.8/s
host**, compared with 35.1/s and 56.2/s in the prior owner encounter. Client
cosmetic-dispatch median per-window mean falls from approximately 7.83 to
0.41 ms. These are separate owner runs, not identical input replays or
rendered-FPS measurements. **CONFIRMED_LIVE instrumentation:** the prior client
update-rate deficit is absent in this combat sample.

Thirty-five complete host input windows contain 7,303 moving-input frames,
zero mod-held moving frames and one stale-input frame. Party/world buffers
remain populated; their target delay still ranges from 50 to 66 ms. Transient
`not_ready` presentation reports also remain (41 in the selected new interval,
29 in the old one); these counts do not establish their duration or visible
impact. The final Spirit activates and reaches body-drained. No visual or
effect correctness is inferred from those counters. C3/C4 owner visual
acceptance and area-transition containment remain open; no C5 work started.

### 2026-10-10 — C3/C4 motion/effects accepted; music remains before wrap-up

**CONFIRMED_LIVE owner feedback:** the owner considers current motion and
effects fine and accepts the tested C3/C4 encounter behavior. Defer the broader
vanilla-versus-multiplayer feel of attacking monsters to the explicitly
requested [issue #58](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/58).
Numeric targeting checks (#56), whole-roster verification (#57), and later
area-transition containment are not established by this acceptance.

The owner asks to wrap up this detour only after resolving the longstanding
music bug: the client continues playing countryside exploration music when
the encounter has triggered danger music on the host. This is an encounter
music-transition defect, not a Tal-specific soundtrack or a request to bind
music to the host's character. Investigate and fix the native transition,
then stop for the owner's audio acceptance. Do not begin C5 or Phase D here.

### 2026-10-10 — encounter danger music candidate; audio owner gate pending

Dirty `decbbc5` plus the preserved prototype. Owner-authorized music repair
and D1's presentation-message/version split: session protocol **20**, effect
codec **2**, 2269-byte atomic state in two datagrams (each at most 1208 bytes).
Older protocol peers reject each other. No baseline commit, reset, stash or
unrelated staging. Candidate DLL SHA256: `3750f02c4e3919b2f6470323a675b5b5f85260eed582bcba27e383a077d59e3c`.

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE:** `CBattleScores` owns area score
selection and the danger transition. Its cluster listener at VA `0x439AD0`
changes the battle flag and intensity; its ordinary Update advances music.
The contained client does not receive the native cluster notifications. A
read-only post-combat comparison additionally found the host using
`BS_IllumCountry` while the client retained `BS_Brightwater` (**CONFIRMED_LIVE
observation**, not a new listening acceptance).

The new saved-story adapter captures the actual native score, danger state,
intensity and variation curve. With a fresh same-area game-thread witness,
it uses native `BattleScoreLoad` for a changed, registered score and the
native audio-only battle listener for danger entry/exit. Native playback,
volume/fade clocks and cue progression remain native. The cluster variation
curve must also follow: the contained client's ramp duration was zero, which
would otherwise divide by zero on entering danger. No combat gameplay script
or character ownership changes are introduced.

**CONFIRMED_TEST:** MusicImage, EffectTransport, LanPartySession and
ResidencyCall pass under Wine. MusicImage checks all named Load, Reset,
Encounter, Update and EmitterLoad contracts, relocation/vtable mutations,
executes the native scalar danger-entry/exit listener, and refuses wrong
thread, witness, area, bank/cue owners and read-only targets. Transport covers
round-trip music state, atomic/reordered/lost/stale/wrong-scene delivery,
malformed music state and previous-version peers. It does not execute a
sound device or bank load and does not prove audible parity.

**Owner gate:** reload the saved-story pair; listen before the ambush, start
the intro, continue through later waves, then finish combat. The client
should use the same countryside score and enter/leave danger music when the
game does on the host. Confirm no restart on every state packet, no hanging
danger music and no new motion/effects hitch. Record native score/transition
logs separately from the owner's audible result. No C5 or Phase D advance.


**Live candidate prepared:** both saved-story peers reloaded with the
candidate and unchanged launch settings. Read-only native state before the
ambush reports `BS_IllumCountry`, peaceful intensity 0.4 and matching
variation parameters on both peers; the client no longer retains the
Brightwater score in this observation. This is state agreement, not audible
combat-transition acceptance. The owner has been asked to check danger entry
and return to exploration; that result remains pending. The independent
native comparison instance was left running.


### 2026-10-10 — Tal combo HUD and casting initialization during ambush lighting

Owner clarified that the missing meter means native attack timing and M1/M2
combo icons. This is separate from floating damage numbers. **CONFIRMED_STATIC:**
the older test-room HUD adapter was never called by saved-story presentation;
the story frame carried no accepted combo UI state. The client intentionally
keeps native gameplay combat off, so enabling that global is not a UI fix.

The new saved-story observer records Tal's accepted native weak/strong cue,
mistiming, window marker and reset on the host. It preserves the original
native calls and proves the current roster and native thread. Actor wire bytes
94–95 now carry a validated, contiguous three-icon row and marker; zero means
unknown. Story frame version 8 / LAN version 21 refuse older peers, with no
packet size increase. The contained client presents the existing native HUD
widget for its living, armed Tal in host combat, yields to menus/cinematics/cast
views, and releases it when combat ends. Duplicate state does not restart the
native blink clock. No client combo admission or damage simulation is added.

**Casting investigation correction:** the recorded Spirit requests were
unavailable because cast binding had already failed at `cast_lighting`, while
Ailish was still alive. Her later KO does not establish the cause. The ambush
has active native light effects and a blue gain of 1.1; private cast-light
creation incorrectly required one neutral effect and channels no greater than
1.0. Native Init seeds a new private manager from the currently published
world RGB. The adapter now accepts finite nonnegative native gains and a
valid active world stack, requires publication/current agreement, and leaves
the world's effects and RGB intact. It still enforces image, scheduler,
thread, caster, lifetime and hook ownership. A separate KO restriction has
not been demonstrated or ruled out by a fresh owner test.

**CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE:** eight focused Wine tests pass:
CastLightAbi, StoryComboHudImage, StoryShots, StoryActionTransport,
StoryEffectTransport, StoryCastAdmission, StorySkyLightImage and
SkillTraceImage. ComboHudImage checks each new named byte guard, executes
native reset/window paths and native icon refresh with inert GUI fixtures,
verifies BukiReplicaNativeImageMatches with the hooks installed, and exercises
owner refusal, repeated-frame blink preservation and failed-restore retry.
CastLightAbi creates/drains a private light while a three-effect, above-one
world remains byte-identical. These tests do not prove visible timing or
casting with a downed party member. Candidate remains uncommitted on the
existing dirty revision; no baseline commit or unrelated work was staged.

**Owner gate:** after reloading the saved-story pair, use Tal's M1/M2 attacks,
a three-input combo, and an intentionally late input. Check icons/timing,
reset after interruption, and hiding through Q/cinematics and combat exit.
Then cast Spirit Strike with adequate SSP first while Ailish is alive and
again while she is down, with Tal armed and idle. Both screens should complete
the accepted startup/summon/ending and return control. The encounter-music
entry/exit audio gate remains pending. Do not advance the plan from these
fixture results.


**Live candidate ready (owner result pending):** both saved-story peers now
initialize the combo adapter. The host successfully binds casting during the
ambush with RGB (0.6, 0.75, 1.1), and the contained client's native HUD is in
melee mode with an empty row before accepted attacks. Client input reaches
the confirmed-ready phase. This demonstrates initialization under the formerly
rejected lighting, not Spirit completion with a downed Ailish or visible
combo timing acceptance. The owner controls all gameplay test inputs.


### 2026-10-10 — Owner explicitly closes C3 for the tested castle-road route

The owner confirms C3 can be considered done. **C3 is complete for the tested
remote-Tal castle-road encounter:** the movement-node gate prevents AI from
competing with the remote player's movement. The earlier measured combat
sample recorded 9,997 AI-movement bypasses with no observed Tal AI speed
resets, turns or snaps; the owner subsequently accepted motion and effects.
The C3/C4 acceptance entry above remains the source for tested C4 behavior;
this does not close all of Phase C.

Retain the accepted Tal detour results: Spirit startup and effects on both
screens, return-light fade, ending spiral flicker correction, native Spirit
camera/edge guard, Shin Splitter effects/aiming movement and its skill camera.
These are the specific owner-tested cases, not whole-roster certification.
The latest combo timing/M1-M2 HUD candidate, Spirit Strike with Ailish down,
and danger-music entry/exit still need explicit owner results. Full-roster
checks (#57), broader attack-feel comparison (#58), numeric targeting (#56),
and the earlier beast explosion-position/delayed-enemy-attack rechecks remain
separate follow-ups. No C5, C6 or Phase D completion is implied.


### 2026-10-10 — Immediate HUD/music checks accepted; downed-Ailish test deferred

**CONFIRMED_LIVE owner feedback:** the owner accepts the other immediate
checks from the C3 recap, including the native M1/M2 timing/combo display and
encounter danger-music behavior. The owner explicitly forgot to check Tal's
Spirit Strike with Ailish down and requests that it remain a later issue
check. Added an unchecked regression to existing [issue #57](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/57), with an Ailish-alive
baseline, living/armed/idle client Tal, sufficient SSP, host activation,
both-screen presentation and control recovery. The issue was read back to
verify the update; its remaining roster checklist stays open.

C3 remains complete for the tested route. C4's tested movement-during-attacks
behavior was already accepted alongside it. **Next unfinished plan item is
C5**, optional/low-priority native attack press cadence and combo-window
parity; C6 is the final remote-Tal ambush/turning owner gate before Phase D.
No C5 implementation or C6 acceptance is claimed by this follow-up. The
owner's immediate-check acceptance does not close untested roster cases,
numeric targeting, broader combat-feel comparison or the other issue backlog.


### 2026-10-11 — C6 recognized as already completed; C5 clarification

The owner identifies C6 as the native-versus-co-op castle-road comparison
already performed and accepted. **C6 is complete for that tested remote-Tal
route**, based on the earlier encounter and owner acceptance; no repeat of
the same gate is required. This qualitative acceptance does not close #56's
separate quantitative targeting measurements or extend to untested routes.

C5 remains optional and unimplemented. The current story melee path submits
individual weak/strong/sweep press events to the native arbiter. C5 concerns
matching the native per-frame button-state cadence, including held/released
states, to check tap/hold and combo-window parity. Restoring the native combo
HUD does not implement this input change. No new combo defect is asserted
from the owner's question. If this optional work is deferred, the next
required work is Phase D (enemies and loot), starting at D1. No Phase D work
was performed in this clarification.

### 2026-10-11 — C5 reproduction started; native input routing isolated

The owner requested a C5 reproduction. The supported-image reproduction in
`tests/story_interaction_guard_image_test.c` now executes the retail combat
input router with an inert downstream attack observer. On this dirty branch,
**CONFIRMED_EXACT_IMAGE** for melee mode: `1,2,2,2,3,0` submits exactly one
attack request for each of weak, strong and sweep. Held/released states alone
do not submit another melee attack. A simultaneous weak+strong snapshot
submits only weak; all three pressed select sweep. Native priority is
sweep > weak > strong. Separate weak and strong submissions produce two
requests; actual downstream combo admission is deliberately not exercised.

**CONFIRMED_STATIC:** the current client queues separate rising edges and
waits for the current action result before sending the next. It therefore
does not preserve simultaneous native snapshots, and request/result latency
can delay a later queued press. The live size and gameplay effect of that
delay remain **UNKNOWN**. The input router also clears the shared interaction
latch on any attack release (**CONFIRMED_EXACT_IMAGE**); the current
press-only interaction guard must not be reused for held/release snapshots.

Passed: `StoryInteractionGuardImageTest`, `StoryQuickInputTest` and
`StoryActionTransportTest`. No runtime DLL, native hook or protocol change
was made. A read-only capture was started on the existing accepted pair for
owner-controlled single taps, holding/releasing M1, an M1/M2 combo and a
simultaneous M1+M2 press. **C5 remains open at the reproduction owner gate**;
no new live combo defect, fix or timing parity is claimed. C6 remains accepted
for its previously tested route; Phase D has not begun.


### 2026-10-11 — Buki melee/HUD and Ailish/Elco ordinary shots candidate

The owner extends the current combat work to Buki and explicitly requests
client firing as well as everyone seeing Ailish/Elco shots. The owner selects
New Brightwater for the first check and will supply a full-party save later.
C5 cadence remains at its existing reproduction gate; Phase D has not begun.

**CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE**, dirty `decbbc5` candidate:
Tal/Buki now share saved-story melee admission and the native combo HUD, with
separate actor histories, exact native component ownership and both Buki claw
attachments required for her display. The seven HUD guards are checked by
name; `BukiReplicaNativeImageMatches` still accepts the installed hooks.
Existing shared trail ownership already covers both melee characters.

A new additive ranged-intent message carries press/held state and camera aim
for client Ailish/Elco. The host validates the current actor/control lease,
scene, frame acknowledgment and input freshness before entering native combat.
Native weapon logic owns firing, spread, collision and damage. Clients receive
parent-free visual copies of the host's observed projectile transforms/phases
and lifetimes; they do not create native damaging missiles. The existing
verified emission seam and weak projectile-lifetime observer are reused.
Ordinary projectile and associated effect resources have closed actor-specific
IDs in the saved-story VFX codec; legacy arena profiles retain their limits.

Passed eleven focused checks: `StoryComboHudImageTest`,
`StoryMeleeAdmissionTest`, `StoryActionTransportTest`, `StoryQuickInputTest`,
`StoryEmissionImageTest`, `StorySkillVfxCodecTest`, `StoryRangedTransportTest`,
`StoryProjectileVisualTest`, `LanPartyProjectileLifetimeTest`,
`LanArenaSpiritVisualHostTest` and `LanArenaSpiritVfxTest`. Four-peer UDP tests
cover both ranged shooters and a third observing client, release, duplicate,
expiry and changed-control rejection. Synthetic projectile cases cover current
transform/phase, shared monotonic visual IDs, retirement, address reuse and
unknown/foreign-owner rejection. Exact-image checks cover native hook bytes,
installation/restoration and retained lifetime faults. These do not prove
visible firing or gameplay parity.

**Owner gate OPEN:** first in New Brightwater, verify host-fired Ailish shots
on the client; then use Ailish on the client to check aiming, taps/held fire,
hits and visible shots on both screens. Check that release/menu entry stops
firing and effects expire. With the later full-party save, verify Buki's
M1/M2 attacks, combos, timing/icons and trails, then Elco's firing and visuals.
All-weapon cadence, first-person presentation, muzzle/impact completeness,
close-range short-lived shots and real four-window visibility still need live
coverage; #57 remains the roster-wide effects/camera checklist. No gameplay
input or owner acceptance is supplied by the automated checks.


Candidate startup exposed a preload regression before acceptance: a historical
Elco resource name had no loadable backing model. Native loading cleared its
cache slot; the retained lease correctly refused to guess cleanup success,
which blocked the client's area change. Speculative ordinary-weapon preload
is now limited to the starter effects; other resources require an actual host
emission. Native cleanup/ownership checks remain intact. The original candidate
is not accepted; repeat startup/area-follow and owner firing on the correction.

Corrected startup check: both processes now report the same current/loaded
countryside area, host effects publication is active, and client presentation,
input and effects service are ready. One intervening launch caught an early
scene-identity change and closed input; an unchanged-candidate retry completed.
That startup race is not claimed fixed. The Ailish firing/visibility owner gate
is pending; Buki/Elco remain deferred to the owner's full-party save.


### 2026-10-11 — Ailish sustained firing stance candidate

Owner reports snapping on the observing client during host Ailish's ordinary
shots. The FP-to-body adapter lowered her weapon to combat idle between shot
clips. Ailish already has an authored one-frame FirePose.

**CONFIRMED_STATIC / CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE**, dirty `decbbc5`
candidate: projected FP idle now uses that upper-body stance on channel4.
The existing ranged-pose adapter also owns the verified base-sampler call in
saved-story mode, placing the same stance beneath recoil before native blend
composition. Shot clocks, walking legs, translation, damage and firing input
remain native. Other actions and attached host world bodies retain their
existing projection. This change is scoped to Ailish; Elco needs his later
full-party check. No inferred trigger timeout or protocol change is used.

Four focused checks pass: `LanArenaRangedAimTest`, `StoryRangedProjectionTest`,
`StoryEmissionImageTest` and `RangedWorldPoseTest`. They cover the inter-shot
stance, partial recoil, three sampler channels, native legs/translation and
clocks, stale ownership, and rejection of unrelated actions. The named
`SudekiMpLanAimImageMatches` guard is exercised against the supported image,
including altered call-target rejection, install/restore and retained foreign
callsite failure. Compilation and these checks are not visual acceptance.

**Owner gate OPEN:** repeat Ailish taps and held fire, stop firing, walk while
aiming, then leave weapon mode. On the client, the staff should remain aimed
between shots, recoil should animate, walking should continue, and leaving
weapon mode should restore the normal pose. Check projectile visibility too.
The broader roster/firing owner gate above remains open.

### 2026-10-11 — Observing-client wand origin candidate

Owner screenshots show Ailish's ordinary shots/flash near her face while her
visible staff is lower. The existing cosmetic stream copied native host world
positions, including the first-person weapon's launch point, into the client's
different third-person weapon pose.

**CONFIRMED_STATIC / CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE**, dirty `decbbc5`
candidate: a read-only client observer resolves the displayed weapon's animated
`SFX` locator through its actual parent attachment. The native locator index
selects a locator record, whose bone index selects the current pose matrix;
the static bind matrix is not the animated firing point. Observation requires
the current post-controller witness, full client containment proof, exact
actor/model/weapon owners and equipped item. It invokes no native model update.

Only a local copy of ordinary ranged visuals is adjusted. A matching positive
host shot journal entry anchors the flash at the visible weapon. A projectile's
initial displacement is retained per visual identity and tapers smoothly over
two world units into the host trajectory. This is a cosmetic projection policy,
not a claim of native trajectory equivalence. Host hits/damage, impact positions,
phases, resources, instance identities and retirement remain authoritative.
Missing geometry or emission evidence leaves received visuals unchanged;
changed streams, scenes or actor generations invalidate the projection cache.
The shared Ailish/Elco path is implemented; Elco still awaits the full-party test.

Five focused checks pass: `StoryMuzzleTest`, `StoryEmissionImageTest`,
`LanArenaRangedAimTest`, `StoryRangedProjectionTest` and `RangedWorldPoseTest`.
`SudekiMpStoryMuzzleImageMatches` and `SudekiMpLanAimImageMatches` are exercised
by name against the supported image, with changed-byte/target rejection.
Synthetic geometry covers rotated weapons, nontrivial locator-to-bone mapping,
both ranged identities and rejected foreign/first-person/pending weapon owners.
Projection checks cover launch, convergence, impact preservation, tick wrap,
unknown data and namespace changes. These are not visual acceptance.

**Owner gate OPEN:** in New Brightwater, fire Ailish toward Tal's client camera,
then view from the side. Check tap/held fire, turning and walking: the flash and
shot should emerge from the visible wand, recoil should stay smooth, and hits
and effect expiry should agree with the host. The preceding stance gate and
broader client-firing/full-party checks remain open. C5 and Phase D are unchanged.

### 2026-10-11 — Ranged visual first-appearance ordering candidate

Owner confirms shots now emerge from the wand, but still reports a brief
face-level orb. This is partial acceptance of the origin correction; the
remaining visual defect keeps its owner gate open.

**CONFIRMED_STATIC / CONFIRMED_TEST:** the prior projection left coordinates
unchanged when the matching host shot journal had not arrived. The independent
VFX message could therefore create a projectile at the first-person origin,
then move that same identity after the party packet supplied its shot data.
Correcting an already-visible object cannot undo its first appearance.

The client now supplies a local geometry-ready mask to the existing visual
service. Unready ordinary shots reserve their identities without spawning or
synchronizing. The complete host roster still handles retirement and unknown
observations. A newer ready effect cannot prevent an older reserved shot from
appearing once prepared, and a shot that ends before readiness cannot later
resurrect. Existing corrected flashes retain their known displacement when a
fresh tip is temporarily unavailable. No native projectile is deleted, and
host firing, damage, impacts, phases and lifetime authority are unchanged.

Four focused checks pass on dirty `decbbc5`: `StoryMuzzleTest`,
`StoryVfxDispatchTest`, `LanArenaSpiritVfxTest` and `StoryEmissionImageTest`.
The real runtime dispatch fixture receives VFX before its shot journal, rejects
uncorrected playback, then retries the same packet at the corrected origin.
Visual-service fixtures cover concurrent newer effects, one spawn per identity,
temporary unknown geometry, retirement before readiness and no resurrection.
Named native image checks remain intact, including
`SudekiMpLanArenaSpiritVfxVisualImageMatches`,
`SudekiMpStoryMuzzleImageMatches` and `SudekiMpLanAimImageMatches`.

**Owner gate OPEN:** repeat a short Ailish burst toward Tal's camera and from
the side. Confirm the remaining face-level appearance is gone, shots still
emerge from the wand, and release/impact/expiry remain normal. Elco and the
broader full-party checks remain pending; C5 has not advanced.


### 2026-10-11 — Wand-origin acceptance and wall-impact fizzle candidate

**CONFIRMED_LIVE (owner report):** the owner accepts the preceding correction
for the remaining face-level ordinary-shot appearance. This closes that narrow
origin defect, not the broader stance, all-weapon or full-roster gates. The new
reported defect is a wall-impact fizzle visible on the host but missing on the
observing client.

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE:** native missile update RVA
`0x1867D0` submits environment-impact SFX through `0xDCD00` and positional setup
`0x18B90`. Resource getter `0x186190` reads the missile data's impact resource at
`+0x3C`. The observed Royal Sceptre and Xuric missile configurations select
`SFXHT104_MISS_BLADE2MAGIC.HOM` (`0x06D8E387`), absent from the closed cosmetic
resource table. Positional setup may finalize after its missile update returns;
only animation/script-parent paths previously retained presentation attribution.

Dirty `decbbc5` candidate adds this single observed magic-impact resource to
saved-story presentation. The existing exact native factory adapter retains a
positively witnessed ordinary ranged source through the native weak effect
identity. Late setup can therefore publish the impact after the synchronous
shooter scope ends. Unknown sources remain unowned; the native destructor and
positive host roster removal control retirement. No second hook, damaging client
missile, fabricated impact, fixed lifetime, or speculative resource prewarm is
introduced. Impact positions bypass muzzle projection. Existing arena and party
wire layouts and closed resource ranges are unchanged.

**CONFIRMED_TEST:** five focused checks pass: `LanArenaSpiritVisualHostTest`,
`LanArenaSpiritVfxTest`, `StorySkillVfxCodecTest`, `StoryMuzzleTest` and
`LanPartyProjectileLifetimeTest`. Named image checks include
`SudekiMpLanArenaSpiritVisualHostImageMatches` and
`SudekiMpLanArenaSpiritVfxVisualImageMatches`; existing factory/lifetime admission
also rejects a mutated native operand. The installed factory/finalizer fixture
exercises delayed positional completion for both ranged owner types, rejects
an unwitnessed visual source, drains pending ownership, and uses the actual
native weak destructor. Client tests cover resource lifecycle, message ownership,
legacy rejection and unchanged contact coordinates without a shot journal.
The host fixture initially reused an object after its base destructor without
reconstructing its vtable; correcting that fixture made its rerun pass.

**Owner gate PASSED — CONFIRMED_LIVE (owner report, 2026-10-11):** after the
New Brightwater wall-shot comparison, the owner accepted the observing-client
fizzle with “thats good” and requested both games be closed. This accepts the
tested Ailish ordinary-shot wall-impact presentation on the dirty candidate.
Other impact resources, Elco's live behavior, client firing and full-party checks
remain pending under the existing C5/#57 gates. C5 and Phase D have not advanced.


### 2026-10-11 — Deferred enemy brightness and Ailish incoming-hit observations

The owner reports a possible repeated, brief extreme brightening of an enemy's
surface, like a strong flashlight. [Issue #59](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/59)
records this as intermittent and unreproduced. Screen, enemy and trigger are
UNKNOWN; intended damage flash, effects lighting and material playback are
comparison leads, not established causes.

The owner also reports not seeing Ailish react to incoming hits on the client.
[Issue #57](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/57)
adds a separate incoming-hit verification item, covering idle, aiming/firing,
movement and recovery; compare host/native-selected flinch, displacement and
knockdown rather than requiring knockback on every hit. Installed resource data
names Ailish (internal Alice) front/back GetHit and knockdown entries. This does
not prove a particular hit selects them or that client playback is correct.
Source inspection finds the saved-story first-person/world semantic bridge and
its narrow idle/fire conversion; client damage containment means independent
local damage-triggered reactions cannot be assumed. Root cause remains UNKNOWN.

Both games remain closed at the owner's request. No new live reproduction,
implementation change or acceptance is claimed; earlier accepted wall-fizzle
and shot-origin fixes retain their limited scope. Resume these checks under
the existing C5/#57/#59 gates rather than treating them as already resolved.


### 2026-10-11 — Owner-scoped next research pass and route sign-off

Owner clarification: incoming-hit presentation should match the native reaction
selected for the attack, from a brief flinch/stagger to stronger knockback or
knockdown. The report is missing visible reactions on the client, not a request
for every attack to cause a knockdown.

The next focused research pass covers that Ailish reaction discrepancy (#57)
and the intermittent enemy-surface brightness report (#59). The owner's intended
acceptance route is **Illumina → New Brightwater → Illumina**. The owner believes
these are the last observed gameplay problems on this stretch; treat that as
the scope of the remaining reported work, not a completed regression pass.
After the focused checks/fixes, perform the complete route with owner acceptance.
Previously deferred whole-roster and comparison checks retain their own scope.
Games remain closed; no new test, fix or route sign-off occurred in this update.

### 2026-10-11 — Whole-roster incoming-hit candidate (#57)

Owner scope: fix incoming-hit presentation for Tal, Ailish, Buki and Elco.
The accepted saved-story build was reopened for an owner-controlled, read-only
capture. CONFIRMED_LIVE: Ailish's first-person hit semantic `0x36` advanced on
the host while body conversion refused that arms-only clip and retained the
previous client pose. Tal's sampled weak `0x2A` and stronger `0x2B` reactions
already reached the client with advancing body clocks. This does not establish
every incoming attack or knockdown.

CONFIRMED_EXACT_IMAGE: native damage selection at RVA `0xD2598` chooses `0x36`
while first person remains active; forced exit precedes that choice. The
candidate maps that arms-only flinch to the authored body weak flinch `0x2A`
for both ranged characters, retaining normalized phase and native duration.
The existing adapter still proves source and destination resources. Attached
body reactions for all four characters retain their native clips, timing and
blends, including distinct stronger/knockdown reactions. Damage authority,
control and network formats are unchanged.

CONFIRMED_TEST: `RangedWorldPoseTest`, `StoryRangedProjectionTest`,
`LanArenaHitFeedbackTest` and `SkillTraceImageTest` pass. The latter exercises
`SudekiMpLanHitImageMatches` and `SudekiMpLanAimImageMatches` by name, with native
install/restore and foreign-byte refusal. Projection regression covers both
ranged characters' hit entry, promotion and recovery, invalid resources, and
all-four attached-body preservation without host writes. An initial test-only
decimal equality assertion was replaced with a float tolerance; the production
candidate did not change during that correction.

Candidate: dirty `decbbc5`, DLL SHA256
`45f95cc8206d58d2cecac7dd75e863db85f64814f4bf3347c27acc6bce8d66cf`.
Reloaded both games at the owner's request with this candidate. Startup
verification confirms identical DLLs, unchanged settings, host cast/VFX binding
and client phase5 with replica/input ready. A fresh read-only reaction capture
is running; visible hit-reaction acceptance is still pending. Next gate:
compare Ailish aiming-hit startup/recovery on both
screens, repeat hits and transitions into stronger full-body reactions, and
recheck Tal. Buki/Elco require the deferred full-party save. #57 remains open;
#59 enemy brightness and the full return route remain separate pending checks.

### 2026-10-11 — Hit reactions accepted; ranged combat strafing candidate

The owner reports the reloaded hit-reaction candidate is good and requests
normal left/right combat stepping for Ailish and Elco, explicitly clarifying
that this means ordinary strafing while aiming. CONFIRMED_LIVE: acceptance of
the tested New Brightwater pair on the preceding `45f95cc8` candidate. The
read-only capture also sees Ailish's projected weak-flinch body clip on the
client. Buki/Elco and other incoming-attack families remain under #57's deferred
full-party verification; this does not close #59's enemy-brightness report.

The owner reproduced sideways movement with a forward-walking body. The
read-only capture confirms that the client used forward clips during both
left and right translation. Source cause: `project_ranged_translation` chose
only forward walk/run semantics `6/7`, regardless of movement direction.

CONFIRMED_EXACT_IMAGE: native RVA `0xE3D10` selects forward `6/7`, backward
`8/9`, left `A/B` and right `C/D` movement families. Ailish's loaded body bank
resolves those side/back clips, all with the same hierarchy as the existing
aim layer. The candidate selects the authored family relative to the host's
aim and observed displacement for both ranged characters, retains normalized
gait phase across direction changes, and scales differing clip lengths to
preserve the existing paired playback periods. Stop grace retains the last
direction. Ailish's aimed-recoil sampler now admits these exact body bases.
No new dodge, gameplay movement, damage, fire request or protocol is added.

CONFIRMED_TEST: `RangedWorldPoseTest`, `StoryRangedProjectionTest`,
`LanArenaRangedAimTest` and `SkillTraceImageTest` pass. Coverage includes both
ranged characters, four movement directions, varied headings, firing and idle
layers, reversals without restarting the gait, stop recovery, invalid inputs
and missing resources, plus the prior hit-reaction regressions. The named
`SudekiMpLanHitImageMatches` and `SudekiMpLanAimImageMatches` exact-image checks
also pass. Build retains the existing native-pointer pedantic warnings.

Candidate: dirty `decbbc5`, DLL SHA256
`46263f8d64e9a27422e46ca75b06036cbeeec8760dd9c0a7b09a646bae686216`.
Pair reload completed with matching candidate DLLs and unchanged settings.
Host cast/VFX binding and client replica/input phase5 are verified. A fresh
read-only capture is running; visual acceptance remains pending. Owner gate:
Ailish aims left/right, reverses direction, stops, and fires while strafing;
check legs, held weapon/recoil, projectile origin and hit recovery. Elco needs
the deferred full-party save. Keep this within the existing C5/#57 detour.

### 2026-10-11 — Ranged strafe timing rejected; authored-rate correction

The owner rejects the directional candidate's timing: sideways movement looks
like a bunny hop and is too fast. They point back to the Test Room work.
This is a failed visual gate, not acceptance of the preceding strafe candidate.

CONFIRMED_STATIC: the later Test Room path captures native directional clips,
rates, phases and blends (`host_capture_ailish_locomotion`), then replays those
observations (`apply_ailish_host_locomotion`). The saved-story FP-body projection
instead reused forward-run rates and scaled them by the side clip's length.
That retained the forward run's speed boost for every direction.

CONFIRMED_EXACT_IMAGE: RVA `0xE1560` reads the authored speed multiplier at
animation-definition `+0x54`; `0xE4F50` retains it in channel metadata. Native
`0xE3D10` blends walk/run cycle durations, then gives both channels that common
period. CONFIRMED_LIVE read-only resource observation: Ailish's forward run
multiplier is about1.298; her sideways and backward multipliers are1. At the
existing projection blend0.99, the side cycle should be0.835seconds; the rejected
candidate used about0.582seconds. This establishes the timing mismatch, not
visual acceptance of a replacement.

The correction resolves each ranged character's own authored multipliers and
clip lengths and uses the native paired-duration calculation. No character's
forward rate is borrowed for another direction. Direction-change phase,
independent firing/aim, incoming reactions and host movement remain unchanged.
The current projection still retains its established0.99run blend; a requested
owner-controlled movement capture will check that separate walking/run-blend
question. Do not claim analog gait blending is established.

CONFIRMED_TEST: `RangedWorldPoseTest`, `StoryRangedProjectionTest`,
`LanArenaRangedAimTest` and `SkillTraceImageTest` pass, including the named
`SudekiMpLanHitImageMatches` and `SudekiMpLanAimImageMatches` byte checks.
Cases cover authored directional cycle durations, paired phases, both ranged
characters, preserved firing/hit behavior, missing resources and invalid or
foreign authored-speed records. No new hook or protocol was introduced.

Prepared candidate: dirty `decbbc5`, DLL SHA256
`e53f58e808bee6e9ad0527e6347683fd7352372ac32946ae186f50be8246819b`.
Not reloaded or visually accepted yet. Await the requested movement sample,
then reload and check sustained left/right stepping, reversals and move-and-fire
with the owner. Elco remains a deferred full-party play-test under #57.


Owner follow-up: the owner had already performed the movement and had not yet
replied. The earlier directional-candidate capture contains that movement;
the later CMovementController blend probe missed it and recorded idle. Do not
claim that the blend probe validated walking/run blending or ask the owner to
repeat the old candidate solely because their reply was delayed.

The authored-rate correction above is now loaded on both games. Startup
verification confirms the same `e53f58e8` DLL, unchanged settings, host cast/VFX
binding and client phase5 with replica/input ready. A read-only capture is armed
for the new candidate's owner gate: sustained left/right stepping, reversals,
stop and firing while moving. Visual speed acceptance and native variable-blend
verification remain pending; no full-roster acceptance is implied.


### 2026-10-11 — Ranged strafe lower-body alignment remains open

The owner reports that the authored-rate candidate still looks wrong and
clarifies that Ailish's feet/legs point toward left/right travel rather than
remaining centered toward her aim. Do not count this as visual acceptance of
`e53f58e8`, or reduce it to another request for slower playback.

CONFIRMED_STATIC: the story adapter resolves the same directional body clips
as the Test Room table. This does not establish equivalent blending or pose
composition. The next investigation separates whole-body heading from the
leg pose and checks walk/run blending against the native reference. Root cause
is UNKNOWN. No new gameplay or pose correction has been applied in this pass.
A read-only state capture is running, with client-window video armed to begin
on an actual sideways clip so the owner's next test is captured. The owner
retains all gameplay commands. C5/#57's visual gate remains open; Elco's
full-party verification remains deferred.


Capture follow-up: the owner performed the test, and the motion-triggered video
and matching read-only samples captured left/right strafing with firing.
CONFIRMED_LIVE observation: during the straight sideways segment, the client's
body matrix follows its published heading and that heading agrees with the
host's horizontal aim. Later, the host itself turns its aim; the side-on frames
from that portion do not prove an incorrect whole-body rotation. The client
uses the expected directional clip pairs with advancing synchronized phases.
Its walk/run blend remains fixed at0.99. Native movement-controller gait fields
remain zero on the host's active first-person path even while translating, so
copying those zeros would not recover a native gait blend.

The visible stance complaint remains open. The capture narrows the question to
authored leg pose/blending and pose composition; it does not establish which
needs correction. Do not add an arbitrary body or hip counter-rotation or
claim another fix from this observation. No source or DLL change in this pass.


### 2026-10-11 — Test Room history exposes reversed story strafe selection

The owner directed the investigation back to commits predating New Brightwater.
CONFIRMED_STATIC: `bc56ed8` (September5, LAN movement/native weapon presentation)
introduced the Test Room's targeting-local/world conversion, directional gait,
and host-observed locomotion replay. `3d8dc64` (August31) separately stopped
reasserting Ailish's base pose over native blend progression, which its source
identifies as a cause of visible limp/stumble. `f9a0c5c` later reused the same
directional-gait helper for party control. These historical fixes are evidence
of mechanisms, not blanket visual acceptance of the current story path.

The recent story projection used the opposite lateral-animation mapping from
that established Test Room helper. CONFIRMED_EXACT_IMAGE: arbiter RVA `0xDAE80`
writes movement mode at CMovementController `+0x68`; `0xC3B70` maps mode2/3 to
movement state3/4 at `+0x54`; `0xE3D10` selects semantic A/B for state3 and C/D
for state4. For travel cross aim greater than zero, the Test Room submits mode2,
so story must select A/B, not C/D. This confirms a reversed lateral selector in
the recent story change. Merely seeing the expected set of clip IDs and a
correct overall heading in the earlier capture did not establish correct
left/right assignment. The prior isolated direction tests repeated that error.

Corrected the shared story semantic selection for Ailish and Elco. The change
only exchanges lateral families; existing authored timing, phase retention,
firing, aim and hit reactions remain under their existing owners. The separate
variable-blend question and direction-transition composition are not claimed
resolved by this correction.

CONFIRMED_TEST: the corrected native-right regression fails against the old
mapping and passes after the correction. All four focused suites pass:
`RangedWorldPoseTest`, `StoryRangedProjectionTest`, `LanArenaRangedAimTest`,
`SkillTraceImageTest`. The latter also compares story selection against the
historical Test Room helper across64 heading/travel combinations and exercises
the named `SudekiMpLanHitImageMatches` and `SudekiMpLanAimImageMatches` guards.
No native hook or protocol changed. Candidate is dirty `decbbc5`, DLL SHA256
`bbfb8f6bdb3ec02ee764c494740a121dcd98d52179ec01b5064f38eab4fe6ccf`.
Visual acceptance remains an owner gate: Ailish left/right steps, reversals,
stop, and firing while moving. Elco still needs the full-party test under #57.


The corrected candidate is now reloaded on both peers with matching DLLs and
unchanged settings. After a delayed client startup and a fresh paired lobby,
startup observation confirms host cast/VFX binding and client phase5 with
replica/input ready. Read-only state capture and movement-triggered client
video are armed for the owner gate; no gameplay controls are automated.

### 2026-10-11 — Strafe direction accepted; idle drift and missing shots

The owner accepts Ailish’s corrected lateral animation (“great now”). This
closes the bounded New Brightwater direction/stance gate, not Elco or the
whole-roster checks under #57. The earlier timed capture ended before the
accepted test, so this acceptance is owner testimony, not a captured video.
The owner also reports brief idle stepping every 10–12 seconds and ordinary
shots absent on both host and client.

CONFIRMED_LIVE: a fresh read-only capture shows short right-step clips at roughly
13-second intervals while the host stays in its aimed idle. Tiny ground drift
crosses the projection’s distance threshold because the anchor never expires.
The candidate limits accumulation to 250ms, retaining slow deliberate motion
and the existing 150ms stop grace. CONFIRMED_TEST: a 30-second drift regression
fails before this change and passes afterward for Ailish/Elco, with and without
firing; slow movement and independent recoil remain covered. Four focused
suites pass, including `SkillTraceImageTest` and the named
`SudekiMpLanHitImageMatches` / `SudekiMpLanAimImageMatches` checks. This candidate
has not yet received live acceptance.

The owner’s subsequent firing capture reaches native missile-manager stages1/2
and the first-person firing animation, but records no shot emission and no
charge expenditure. The native release-event investigation remains open;
missing shots must not be marked resolved by the strafe tests.

The follow-up capture and exact-image inspection narrow the firing failure:
the first-person clip contains the native type-1 release event, and the
missile listener remains registered. The failed run's attached first-person
GfxNode has a NULL registration at `+0x14`; native graphics update RVA
`0x000dfa40` skips its event pump in that state. An absent primary animation
effect is not evidence of a missing shot effect: these fire definitions have
no such effect resource and use the clip event. No forced shot or native
registration mutation was added.

The tested idle candidate has now been reloaded with unchanged pair settings.
Both peers reach the established ready state, and the host first-person node
now has a non-NULL registration. This is startup evidence, not proof of a
permanent firing repair. **Owner gate OPEN:** aim idle for 30 seconds, then
fire a host Ailish burst; verify no idle side-step and shots on both screens.
Read-only animation/weapon capture is armed. The cause of the earlier missing
registration and full-party ranged verification remain open under #57.

### 2026-10-11 — Native uppercut limit confirmed; client shot stalls reported

CONFIRMED_EXACT_IMAGE: native combo lookup RVA `0x000d04f0` requires
`CCombo+0x64 < 6` before appending an input. Matcher `0x00189f30` counts the
six input slots and compares them with the authored sequence length and
buttons. Admission `0x000d0640` also requires the native combo-window flag.
CONFIRMED_LIVE (read-only loaded definitions): Tal has heavy-heavy-light
sequences of lengths three, four, five and six, with the added buttons all
light. Thus the initial HHL can continue with three more L inputs; the seventh
input cannot extend that same native chain. No gameplay demonstration or
change to combo limits was needed for this finding. Current multiplayer
timing acceptance of those follow-ups is separate and unverified.

The owner confirms ordinary shots returned after reload, but reports periods
where all visible shots slow or stop in flight **on the client only**. This
does not establish a persistent repair of the lost first-person registration,
and no idle-twitch acceptance was explicitly given. CONFIRMED_STATIC: ordinary
projectiles and other cast/weapon effects share an eight-entry visual roster;
the host refuses an incomplete roster when that capacity is exceeded. Whether
that limit caused the reported stalls is HYPOTHESIS pending a focused paired
read-only trajectory/count capture. No capacity, protocol or timing changes
have been made for this report.

### 2026-10-11 — Client shot stalls: visual roster overflow confirmed

CONFIRMED_LIVE: the owner repeated sustained host Ailish firing in the saved-story
New Brightwater pair. On dirty revision `decbbc5` with the tested idle candidate,
the paired read-only capture observed up to eleven ordinary projectiles and
wall-impact effects, exceeding the shared eight-entry visual roster. Three
client projectile positions remained exactly unchanged for 2.903 seconds;
host projectiles continued moving during that interval. The population was
over capacity in 183 of its 184 samples. Another three simultaneous client
holds lasted 2.176 seconds with the same relationship. No capture identity/read
errors were recorded. This promotes the preceding capacity hypothesis to the
confirmed cause for this reproduction.

CONFIRMED_STATIC: `SudekiMpLanStoryProjectileVisuals` returns FALSE when effects
and missiles together exceed `SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY` (eight).
`publish_story_vfx` then publishes no new complete roster, leaving the client's
existing shots at their last received positions. Wall-impact visuals and
projectiles share this budget; ordinary sustained fire can exhaust it.
The capture reached eleven, not a measured worst case for the full roster.

The defect remains OPEN under #57. No gameplay code, packet layout, native
effect lifetime or shot behavior was changed during this diagnosis. The repair
must retain complete host-owned presence/retirement information under larger
effect populations; truncating the roster would incorrectly retire or lose
visuals. Re-test sustained fire into walls, open-space fire and overlapping
party effects after repair. No repair or play-test acceptance is claimed here.

### 2026-10-11 — Complete visual roster capacity repair prepared

CONFIRMED_STATIC / CONFIRMED_TEST: saved-story visual capture, missile identity
storage, client weak slots and muzzle projection now support 64 entries with a
64-bit readiness mask. Legacy LA42/SMP4 actor snapshots and the eight-entry
cosmetic v1 bytes remain unchanged. Larger story rosters use additive message
39 with at most four bounded fragments (maximum datagram 1,212 bytes). The
client publishes only a complete, validated, digest-matching roster; first
fragment receipt owns freshness, duplicate fragments cannot refresh it, and
newer updates can evict old incomplete assemblies after packet loss. Ordinary
shots, impacts and casts keep their native host identity/lifetime and existing
wand-origin projection. The retained model-clock exclusion list now covers
all client slots, preserving the accepted Spirit Strike tail hold.

The DLL builds on dirty revision `decbbc5`; twelve suites pass:
StoryEffectTransportTest, LanPartySessionTest, LanArenaProtocolTest,
LanArenaSpiritVisualHostTest, LanArenaSpiritVfxTest, StoryProjectileVisualTest,
StoryMuzzleTest, StoryVfxDispatchTest, SpiritVisualTimingImageTest,
StoryEmissionImageTest, SkillTraceImageTest and StoryEffectClockImageTest.
Regression coverage includes six wall impacts plus five advancing missiles,
64 entries and readiness bits above 31, delayed origins, complete retirement,
unknown observations, reordered/lost/corrupt fragments, assembly saturation,
scene/stream fences and returning to an empty legacy-sized roster. Exact-image
checks include `SudekiMpLanArenaSpiritVisualHostImageMatches`,
`SudekiMpLanArenaSpiritVfxReplayImageMatches`,
`SudekiMpLanArenaSpiritVfxVisualImageMatches`, `SudekiMpStoryMuzzleImageMatches`,
`SudekiMpLanHitImageMatches` and `SudekiMpLanAimImageMatches`; all pass on the
supported image. Native visual/particle timing mutation-refusal checks also
pass by their named seams. No new native patch seam was added.

**Owner gate OPEN:** repeat sustained Ailish fire into a wall and into open
space, viewed by Tal on the client; shots should keep moving continuously,
originate at the wand and fizzle at contact, then expire after release. The
synthetic/loopback tests do not establish live visual acceptance or full-party
maximum population. Bounds remain explicit: 64 combined visual entries and
the existing 32-effect host registry; exceeding a bound still refuses an
incomplete roster. Full-party overlap, the earlier first-person registration
loss and idle-twitch acceptance remain tracked under #57. No gameplay command
is automated.

The tested candidate was reloaded with unchanged pair settings. The client
required one startup retry before reaching the lobby; both processes now
reach saved-story readiness (host cast/VFX published, client phase 5 with
input selected and VFX ready). This is startup evidence only. Read-only
trajectory capture is armed for the owner gate above.

### 2026-10-11 — Projectile-stall owner acceptance; future combo idea

CONFIRMED_LIVE (owner report): the owner accepts the client projectile-stall
repair after the paired firing-check request on dirty revision `decbbc5` in
saved-story New Brightwater. The read-only capture observed up to twelve host
visuals and twelve client entries, with no identity/read errors; these counts
prove larger-roster presence, not a new motion-timing measurement. This closes
the bounded projectile-stall owner gate. Full-party overlap and the other #57
checks remain open; no broader roster or idle-twitch acceptance is inferred.

Future owner-requested idea: [Feature idea: spend SP to extend selected melee combos](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/60) (#60).
Charge SP for designated extra follow-ups on selected combos, using Tal's HHL
uppercut continuations as the motivating example. Eligible moves, cost, first
paid follow-up and whether to exceed the native six-input limit remain future
design decisions. This records the idea without implementing it or changing
the current plan's phase order.

### 2026-10-11 — Return to the plan: resume C5 owner check

The owner requests a return to the plan after accepting the projectile-stall
repair. C3/C4 and the C6 encounter gate retain their recorded acceptance;
C5's optional attack-input timing check remains open after the visual/ranged
detour. Earlier exact-image reproduction and the three passing C5 suites
establish native press/hold/release and simultaneous-button behavior, not
live remote combo-window parity. No new runtime change is made for this check.

A read-only client input queue/outbox capture and paired log capture are
running against the current tested DLL, with image identity and data bounds
checked. **Owner gate OPEN:** on client Tal, compare separate light/heavy taps,
holding and releasing light, HHL plus its extra light follow-ups, and a
simultaneous light/heavy press with native behavior. Record missed, delayed or
extra attacks; the owner controls all gameplay commands. A hold alone should
not create repeated melee requests. C5 is not accepted merely because a
request reaches the host. After acceptance or explicit deferral, the next
required plan step is D1 (client death containment and duplicate loot/XP
prevention). Broader #56–#59 verification and future idea #60 remain separate.

### 2026-10-11 — C5 Tal play-test accepted; capture reviewed

CONFIRMED_LIVE (owner report and read-only capture), saved-story New
Brightwater on dirty revision `decbbc5`: the owner reports the resumed Tal
check is good. All 70 newly observed melee queue entries correspond in order
and kind to 70 host native submissions and 70 client results, with no missing
or duplicate request IDs. Queue occupancy peaks at two. Queue-to-host native
submission delay is 12.5 ms median, 31 ms at the measured 95th percentile and
37 ms maximum in this same-machine pair. Six observed light-button holds
longer than 800 ms produce only their initial request, with no hold repeats.
All 81 logged native combo-display states match the client's presented
sequence, including HHL and the return to the empty display.

**C5 owner play-test accepted for this Tal check.** Outcome 6 means native
submission, not proof that every follow-up was admitted; the native HUD
records only three icons. These logs do not independently prove every
uppercut continuation, Buki, enemy-pressure behavior or higher-latency parity.
The owner report supplies the gameplay acceptance. The optional full native
snapshot/cadence rewrite was not implemented: ten observed same-tick
light/heavy pairs still produce separate submissions, unlike vanilla's
single-snapshot priority. This remains a documented parity limitation, not a
claim of exact simultaneous-button equivalence. No runtime change or new
automated test was needed for this review. The read-only capture is stopped;
the games remain running. C6 retains its prior acceptance. D1 is the next
required plan step; broader roster checks remain under #57.

### 2026-10-11 — Owner-requested Tal/Buki block and shield candidate

The owner reports client Tal cannot block and has no shield, and requests
blocking for both Tal and Buki before D1. CONFIRMED_STATIC: the saved-story
input path handled melee presses and ranged held fire but omitted native
block action `0x31` (controller `+0xA4`). The story visual observer also lacked
the shield owner witness, and its codec did not admit shield identities.

The candidate adds a separate bounded block-intent message (40), retaining
existing movement, attack and legacy packet layouts. Only a fresh acknowledged
Tal/Buki control lease may submit held input. The host services block before
movement and preserves held block when an attack is pressed; native combat
owns admission, defense and animation. Focus/menu/rebind clears local intent;
stale input releases it. Normal, world-load and pre-world-change lease drains
also request the native block ending before restoring AI. Shield appear/loop
effects use the existing finalization observer and complete story visual
roster, with exact actor/parent ownership and native retirement; no second
effect hook or locally damaging client action is added.

CONFIRMED_EXACT_IMAGE: named guards `BlockInputSelection`,
`BlockStartDispatch` and `BlockRelease` pass every-byte mutation refusal.
Retail input routing admits block states 1/2 and releases on 3 without touching
the host interaction latch. Native ending RVA `0x000dad60` uses EAX=arbiter,
requests CBlock state 7 only from states 1–4, and leaves native update/cleanup
in charge. Its isolated test also proves no dialogue advancement or global
latch mutation during ending. It does not run a real defensive encounter.

CONFIRMED_TEST on dirty `decbbc5`: thirteen focused suites pass:
StoryQuickInputTest, StoryMeleeAdmissionTest, StoryBlockTransportTest,
StoryInteractionGuardImageTest, StorySkillVfxCodecTest,
StoryEffectTransportTest, LanArenaSpiritVisualHostTest,
LanArenaSpiritVfxTest, LanArenaProtocolTest, LanPartySessionTest,
StoryRangedTransportTest, StoryActionTransportTest and
LanArenaRuntimeHookTest. Coverage includes held/release and menu/rebind
behavior, both melee owners, stale/duplicate/wrong-fence messages, wrong shield
owners, both shields reaching three observing clients, native byte guards,
and prior attack/ranged/protocol behavior. Runtime-hook fixtures are synthetic;
they do not establish live disconnect/load behavior.

**Owner gate OPEN:** on client Tal, hold and release block in combat; both
screens should show his native stance and shield, then its native ending.
Have an enemy hit the raised shield to verify native defense and feedback.
Check release followed by movement/attacks, repeated blocks, and opening a
menu or changing focus while held. Buki's full-party live check remains under
#57 with the owner's later supplied save. No live block acceptance is claimed.
D1 stays next after this owner-requested detour.

The tested block candidate is reloaded on both peers with unchanged settings.
The client required one retry before reaching the lobby. Host cast/VFX capture
is ready, and the client reaches control phase 5 with input admitted and VFX
ready. This establishes startup only. Read-only paired log capture is running
for the owner gate; no gameplay input was automated.

### 2026-10-11 — block follow-up: native Tal/Buki dodge candidate

Owner confirms Tal blocks, but reports that his natural dodge while blocking
is missing. This accepts the observed block action only; shield-hit feedback,
release/drain edge cases and Buki's full-party check remain open under #57.

CONFIRMED_STATIC: saved-story serviced held block then always entered ordinary
movement. The generic body-busy gate discarded direction while blocking. Test
Room instead chooses native dodge for held block plus direction. Saved-story
now makes that same choice through its exact sparse actor lease, before the
ordinary movement gate. Both Tal and Buki use the adapter. Directions retain
the existing story input validation and are normalized before submission.
Neutral input and native rejection consume this locomotion choice without a
subsequent walking/stop call. Native admission, cooldown, animation, physics
and the accepted dodge's block retirement remain game-owned; no new packet,
hook seam, client gameplay authority or forced cancellation is added.

CONFIRMED_EXACT_IMAGE: `DodgeAdmissionAndBlockHandoff` validates every byte of
RVA `0x000dae00` through its return branches and refuses every-byte mutations.
`SudekiMpArbiterDodgeImageMatches` and `SudekiMpBlockResetImageMatches` also
pass by name. The isolated supported-image test executes actual CDodge
non-idle rejection and the arbiter's admission gates. With downstream task
sinks isolated, accepted input passes the direction through ECX to CDodge and
requests CBlock state 0 only after acceptance. This proves the ABI/routing,
not live roll physics or animation. The game, rather than the adapter,
performs that accepted block handoff.

CONFIRMED_TEST on dirty `decbbc5`: StoryMeleeAdmissionTest,
StoryInteractionGuardImageTest, ArbiterCombatInputTest,
StoryPlayerMovementImageTest, StoryBlockTransportTest and
LanArenaRuntimeHookTest pass. Wrapper coverage includes both melee characters,
neutral/cooldown routing without ordinary movement, busy world/interaction/
cast containment, release returning to walking, and ownership refusal.
Runtime-hook fixtures remain synthetic. Initial test fixture assumptions
about a native rejection bit and x87 float equality were corrected before
these passes; native behavior was not changed to satisfy them.

**Owner gate OPEN:** with client Tal armed in combat, hold block and try the
native dodge directions. Verify the roll on both screens, repeated rolls at
native timing, then release and move/attack. Check that holding block alone
still shows the shield. Buki's equivalent full-party live check remains #57.
D1 remains queued after this detour; no live dodge acceptance is claimed.

The tested dodge candidate is reloaded on both peers with matching DLLs and
unchanged settings. The client needed one startup retry. The host reaches
bound cast/VFX capture; the client reaches admitted input/control phase 5 and
ready VFX. Read-only paired logging is active for the owner check. Startup
is confirmed; the owner has not yet accepted the dodge candidate.

### 2026-10-11 — shield timing and character-audio research follow-up

Owner reports shield startup feels slower than vanilla on both screens,
questions the low hum, and identifies missing Tal attack/action sounds on the
client. A first bounded detailed capture expired before the actions; its idle
samples establish nothing about those actions. The longer repeated capture
records a held block, release, ordinary attacks and combos on both peers.

CONFIRMED_LIVE (bounded observation, dirty `decbbc5`, saved-story pair): the
host cue tracker contains Tal's `BlockStart` while held and `BlockEnd` after
release, plus attack voices, sword whooshes, strain and combo cues. The client
Tal bank is loaded but its cue tracker contains none of these cues during the
matching pose sequence. This confirms the observed client audio gap. A native
cue handle does not establish audible gain; host hum audibility is still
unverified. Both application mixer streams were unmuted at 50%; no mixer
settings were changed.

The corresponding block body startup/hold playback rates match between host
and client (48/24 native frame units per second); the client selector changes
follow about 0.10 seconds later. This is a bounded presentation observation,
not a vanilla comparison or a measurement of shield-effect startup. The
probe's separate DLL-relative input/effect offsets were incorrect and those
fields are excluded from conclusions. The validated native actor/component
and cue-tracker observations above do not use those offsets. No gameplay or
native audio implementation was changed during this research pass.

CONFIRMED_STATIC: native CBlock dispatches actor-owned `BlockStart` and
`BlockEnd` separately from the shield model, retaining its sound handle for
native retirement. Tal/Buki native sound banks contain shield loop cues;
Tal's bank also contains attack voice/grunt cues. The current story visual
replicas do not supply this character-audio path. Follow-up under #57: relay
host-observed character audio with exact once-per-cue playback and native
loop retirement; compare shield-effect startup against vanilla before
changing timing. Owner visual acceptance of dodge and Buki remains separate
from log evidence that native dodge requests were accepted. D1 stays queued.

### 2026-10-11 — character-audio candidate and immediate block edges

Owner requested implementation of the shield/audio follow-up. On dirty
`decbbc5`, saved-story host observation now journals successful native actor
sound calls for each exact sparse-roster owner. Additive cosmetic message 41
carries a complete bounded per-character cue roster (64 cues); cue names and
native pointers are never transported. Client playback resolves the same
index/name digest in that character's locally loaded bank and uses the native
actor sound call. It neither opens client combat simulation nor duplicates
host audio. Native handle generation/retirement owns cue lifetime; duplicate
updates cannot restart a completed local cue. Unknown identity retains
cleanup obligations, while positively retired generations can be cleared
without dereferencing a bank/cue destroyed during a zone transition.

CONFIRMED_STATIC: block press/release previously sat behind the 15 ms movement
send gate. They now send on the current admitted dispatch, with unchanged
held-state heartbeat and native block/dodge admission. This removes that
avoidable wait, not the network trip or presentation buffer. No native shield
animation speed, audio volume, or combat semantics was changed. Host hum
loudness and overall vanilla shield-start parity still need the owner check.

CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST: 17 named native guards pass in
StoryCharacterAudioImageTest, including ActorPlayCue, EmitterPlay, BankPlay,
CueRegister, CueStop, CueRetire and attachment methods. The test checks the
ECX-component/ESI-output/RET16 bridge and detour, exact source ownership,
unknown/generation rejection, duplicate suppression, completed local cues
still present on host, expired-bank cleanup and closed admission. Four more
suites pass: StoryCharacterAudioTransportTest, StoryBlockTransportTest,
StoryMeleeAdmissionTest and LanArenaRuntimeHookTest. Transport tests include
all three observing clients, full capacity, malformed data, client-authority
refusal, stale/wrong leases, empty retirement and scene fencing. These are
bounded automated results, not audible or vanilla timing acceptance.

**Owner gate OPEN:** hold Tal's shield for about five seconds, release it,
block and dodge, then perform M1/M2 attacks and a combo. On both screens check
shield startup, low held hum, release/roll cleanup and attack voices/whooshes.
Repeat after moving away from the starting spot, then later check transitions,
disconnects and Buki/full-roster audio under #57. D1 remains queued after this
detour. The candidate must not be recorded as accepted until that check.


### 2026-10-11 — native audio comparison and client emitter correction

The owner confirmed shield and attack sounds in the separate native-reference
window. This does not accept the previous multiplayer candidate. Inspection
confirmed that the host was observing the appropriate character cues while
client native playback returned no handle: replicated actors had moved, but
their paused sound components retained their save-load positions outside the
native 45-unit hearing radius. The earlier automated bridge/transport tests
had not exercised this native distance rejection.

On dirty `decbbc5`, the shared client character-audio path now refreshes each
roster actor's sound origin and owned 3D attachment from its presented pose,
before new cues and while existing cues remain active. Authored sound locators
use already presented bone/body matrices. Host audio remains native; cue
selection, gain, distance culling and host event ownership are preserved.
Tal, Buki, Ailish and Elco use their own local banks on every observing client;
this includes native hurt/death/action requests and Tal/Buki shield start/end.
Failures now name the blocked playback check instead of silently disappearing.

CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST: StoryCharacterAudioImageTest passes
22 named native guards. New coverage includes ActorSoundTransform,
AttachmentSetPosition, AttachmentSetVelocity, SoundLocator and
AttachedSoundBuffer; AttachmentPosition now checks its complete leaf. The
fixture executes retail ActorPlayCue distance rejection before correction and
successful admission afterward, with only the lower audio-device call inert.
It exercises native attachment movement, authored locators, invalid-owner
refusal, all four character banks, native gains, duplicate suppression and
retired cue cleanup. StoryCharacterAudioTransportTest passes all four character
streams to all three clients, including identical cue IDs in different banks.
Neither fixture establishes audible full-roster acceptance.

**Owner gate OPEN:** when the owner returns, compare host and client shield hum,
release/roll cleanup, attacks and hurt sounds; repeat after moving away from
the starting point. Full-party Buki/Elco, ranged fire, individual skills/Spirit
Strikes, transitions and disconnect sound cleanup remain under #57. D1 remains
queued; no additional manual actions are requested while the owner is away.

Reference-fixture follow-up: the owner reported blacking out near King's Pass.
Only Tal was present in that native save at teleport; no completed area switch
was recorded in the bounded capture. Missing Ailish is a hypothesis, not a
proven cause or an accepted multiplayer transition defect. Recheck with a
story-appropriate party/save before changing transition behavior.


### 2026-10-11 — attack outbox stranded after an input interruption

The owner reported that Tal could move and block but could no longer attack
with the character-audio candidate. Read-only live inspection found the first
melee request retained by both transport journals with neither pending/taken
state nor a result, while the client runtime waited on a second request. This
explains the missing host attack calls; it does not establish that native audio
code caused the interruption.

On dirty `decbbc5`, host presence fencing and control revocation now record
EXPIRED for an untaken queued action. Already taken actions retain their native
outcome obligation. Clients preserve uncertain delivery, and a temporary
missing control offer retains the runtime outbox; only a positive replacement
connection/fence retires it. Same-ID retries recover the terminal result and
allow the next fresh attack. No wire layout, native attack rule, or audio
emitter behavior changed.

CONFIRMED_TEST: StoryActionOutboxTest reproduced premature abandonment before
the correction and passes afterward, including identical-ID recovery and
positive owner replacement. StoryActionTransportTest passes Tal/Buki requests
across presence changes, untaken expiry, taken-result preservation, later melee,
revocation, duplicates and scene/lease fencing. StoryMeleeAdmissionTest passes.
CONFIRMED_EXACT_IMAGE: StoryCharacterAudioImageTest still passes its 22 named
native checks and emitter/bank fixtures. These checks do not accept live combat.

**Owner gate OPEN:** after the requested reload, try M1/M2 and a combo, then
block/release and attack again. Switch focus away/back and repeat. Confirm
attack animation, hit consequences and sounds on both games; keep the existing
full-roster audio gate open. This detour does not advance the roadmap.


### 2026-10-11 — remote party movement-to-idle transition candidate

The owner accepted the preceding Tal attack recovery. This confirms the
reported attack regression only; it does not accept every focus-interruption,
shield/audio or full-roster case. Existing issue #57 already explicitly covers
Buki attacks, combos, block feedback, dodge, release recovery and cleanup with
a later full-party save, so no duplicate issue was created.

CONFIRMED_LIVE (observation, before correction): the requested host-Ailish
movement capture completed without read errors. Her client world body switched
directly between projected combat gait and idle with no intermediate idle
blend. The host was using first-person arms; its dormant third-person body
cannot serve as a native world-animation comparison for this capture.

On dirty `decbbc5`, projected Ailish/Elco movement now blends the gait pair
against authored idle in the existing native two-pair tree. It retains gait
and idle clocks, preserves aim/recoil, and promotes the advancing idle clock
when the fade finishes. The existing stop grace remains. The cosmetic fade
uses 100 ms, matching the supported image's ordinary transition duration at
RVA339BE0 referenced by native E3D10; live timing parity is not yet accepted.
Native third-person body blends are copied unchanged for all four characters.
Shared party interpolation now advances each stable animation channel and blend
subtree independently: an unrelated recoil edge or loop cannot hold the whole
movement fade. Changed resources/states remain discrete, and existing scene,
generation, model and owner fences still apply. No protocol or gameplay-authority
change, new native hook, or automated gameplay input was introduced.

CONFIRMED_TEST: StoryRangedProjectionTest covers Ailish/Elco start/stop fades in
all four directions while aiming/firing, retained idle phase and stale-target
refusal, plus unchanged native blends for all four roster identities.
StoryPoseInterpolationTest covers independent channel/subtree interpolation,
resource/state boundaries and scene/model/generation refusal across all four.
StoryActionOutboxTest and StoryActionTransportTest still pass. The DLL builds.
CONFIRMED_EXACT_IMAGE: SkillTraceImageTest passes, including the named
SudekiMpLanHitImageMatches and SudekiMpLanAimImageMatches checks. These five
passing suites do not establish visual or full-roster gameplay acceptance.

The pair was reloaded with matching candidate DLLs and unchanged configuration;
host control and client replica/input readiness were observed.
**Owner gate OPEN:** move Ailish on the host and watch her on Tal's client.
Check start/stop in forward, backward and both sideways directions, including
aiming and firing. She should ease into idle without a pose jump, foot hitch
or interrupted shot. Also confirm Tal still attacks and blocks/releases.
Tal/Buki/Elco remote movement and full-party combat remain later visual checks;
no roster-wide acceptance is implied. D1 remains queued behind the detour.


### 2026-10-11 — combat exit seals: missing runtime instance and broader inventory

The owner reports vines appearing at a combat doorway on the Ailish host but
missing from Tal's client, and requests coverage for the other barrier types.
CONFIRMED_LIVE (read-only observation): the host creates a placed
`HC_AklorianBarrier` GenericEntity at the nearby gate during combat. The client
has no corresponding placed instance. Both retain a separate same-name,
same-identifier resource template. This is a missing combat-barrier instance;
it is not established as a native CDoor animation defect. Capture of the
combat-end retirement remains pending the owner's completion.

CONFIRMED_STATIC: the world scenery catalog in `lan_story_world.c` matches
kind/identifier, deliberately omits ambiguous duplicates, and does not create
objects. Removing that protection or moving the resource template would not
supply a proven per-instance lifecycle.

The supported `SOLData.baf` (SHA256
`9e604ec2b4736a99e16a4c76dee5842665cd3d138db44a8a694f105fa7ed360c`)
contains 82 serialized `Gate Object File` fields. Thirteen are populated,
covering six distinct resources; the other 69 are empty. The associated
entity definitions identify these models:

| Gate resource | Authored model |
| --- | --- |
| `HC_AklorianBarrier` | `SFXE050_MAGIC_GATE` |
| `DB_Barrier_FireDoor` | `DB_FIREDOOR` |
| `NG_CombatBarrier` | `SFXNG005_COMBAT_DOORS` |
| `AO_StairSpikes1` | `SPTD_STAIR_SPIKES` |
| `TM_RockBarrier` | `SFXTM007_COMBAT_BLOCKER_GATES` |
| `TRC_GEN_Barrier` | `TRC_BARRIER` |

This inventory checks the zone field itself, so it includes spike gates whose
resource name does not contain “barrier.” All six have matching heartbeat and
despawn functions in `SOLWORLDM.gex`; the existing `inspect-sol-gex.pl` tool
finds spawn-group queries and retirement calls in their despawn functions.
These are static resource/script findings, not live acceptance of those areas.
Serialized resource references also require care: one populated vine field
uses a different resource hash despite the same displayed filename. Native
resource resolution must remain authoritative.

Additional authored candidates include `QGS_Barrier` (the same magic-gate
model), ship and bridge barriers, TRC placed variants, and `ROS_PathCombat_Gate`
doors. Their existence does not establish that they all use the automatic
combat-seal lifecycle. Audit their creation, animation and removal separately;
do not include every object with “barrier” in its name as a combat seal.

**Implementation scope:** preserve each host-owned placed barrier's identity,
transform, appearance/hold/ending animation and retirement for every observing
client. Cover the six zone-configured families and classify scripted variants.
Keep native host blocking/combat rules authoritative and preserve scene and
owner fences. Unknown or unsupported variants remain explicit follow-ups.
No new replication implementation, reload or visual acceptance is claimed by
this research entry. Work remains dirty on `decbbc5`; prior candidate and
owner gameplay control are preserved.


### 2026-10-11 — owner-requested unfinished checkpoint

The owner explicitly requests a commit and push of the accumulated work on
`codex/shared-simulation`, with no build and no merge. This supersedes the
initial no-baseline-commit instruction for this checkpoint only; it does not
mark any unfinished plan step or owner gate complete.

The checkpoint includes the current source, regression fixtures, native research
and progress log. The combat-seal inventory is recorded, but barrier replication
is still unimplemented. The latest party movement-to-idle candidate and remaining
full-roster checks retain their documented acceptance limits. Resume with the
barrier instance/lifecycle investigation, then the queued plan work.

No build or test execution is requested or performed for this checkpoint.
Earlier test and play-test results remain bounded to their recorded candidates.
Private captures, game files and generated build output remain excluded. This
is work in progress, not a release or a merge-ready completion claim.
