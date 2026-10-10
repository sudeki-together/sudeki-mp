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
