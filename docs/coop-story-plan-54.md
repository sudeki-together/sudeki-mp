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
| D9 | **Per-game party inventory (owner, 2026-10-14).** Every player's game keeps its own vanilla shared party inventory and purse; nothing is pooled across machines. Loot — florins, consumables, materials and chest contents picked up off the ground or from dead enemies/chests — is **copied into every player's inventory** (the host observes its native `AddItem` at the pickup/treasure sites and clients add the same item natively, with the native pickup popup). **Shop purchases count only for the buyer**; shop/script `AddItem` sites are never replicated. Equipment state (what each character wears) replicates as state so every game shows the same models/animation banks; each player chooses their own character's equipment from their own menu (request to host). Consequence for D6: a client's inventory lives in its own game, so a save transfer from the host must not erase it — the mod's per-player ledger/sidecar re-applies it after loading the host's save. |
| D10 | **Client menu scope (owner, 2026-10-15).** A client's M menu edits only its own character: equipment and consumables are requests the host executes natively on that character; consumable use from a client offers **no target choice** (self only — the host keeps vanilla targeting). Weapon info (F3) and the other read-only pages stay available. |
| D11 | **Houses and shops (owner, 2026-10-16/17).** Story houses stay vanilla: the host lead goes in alone, the others wait outside (no client entry; host-spectate of story houses is a later feature). Any player may walk into a **shop** (door table; first: `LNBr_Kamo_shop`) on their own, several at once, one house at a time (#83 tracks different houses at once). Players in the **same shop see each other** (2026-10-17, replaces the first "instanced" rule). Kamo is used by each player on their own game: the interact prompt is computed by that player's game (no protocol change), Kamo's greeting plays **only for that player**, then that player's shop opens; buying and selling use that game's own purse (D9), worn equipment cannot be sold, and purchases last for the session until per-player saves exist (D6 sidecar). Nothing about a purchase is sent to the host; later use/equip requests already carry the item (D10). Kamo keeps the vanilla choice menu (Shop / Chat / Goodbye, asked again after shopping, ending on Goodbye); the shop stock is read at runtime from each player's own verified game files (nothing game-derived is committed); delivery is staged: prompt and shop screen first, then the greeting conversation (owner, 2026-10-17). |
| D12 | **Co-op conversations and quests (owner, 2026-10-17: "go ahead with these").** Replaces D4's "one conversation at a time, others spectate" for NPC talk. Flavour talk and shops run on the initiating player's **own game**, as their own character, seen only by them. Side quests may be started by **any player on their own game**; every shared change (quest stage, story progress, quest items, rewards) is a request the **host applies and syncs to everyone**. Story scenes and cutscenes are started by the **host lead only**; everyone watches. Quest rewards: florins and consumables are copied to every game like loot; quest items stay a **single copy on the host** (touches D5/D9). **Everyone is notified** of quest changes, with who did it and a "(character only)" tag; quests the game updates silently stay silent. A second player at a busy quest NPC is told who is talking; flavour NPCs and shops can be used by several players at once. New protocol messages (story-progress sync, change requests) and clients asking the host to change quest state are approved. Staged: S0 privacy plus Kamo camera/facing/gestures (gestures behind a switch until the native animation update is audited), S1 story-progress and quest-log sync plus notifications, S2 real conversation scripts on the player's own game with shared writes blocked, S3 flavour talk for everyone, S4 side quests with host-applied results. |
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
| C5 | Native press cadence (1→2→3→0) through the same snapshot shape as 0x42891F. Originally optional; owner requested the remaining rewrite on 2026-10-11. | Combo windows match local play; revised Tal play-test pending |
| C6 | Owner gate: the owner plays remote Tal through the castle-road ambush, solo vs co-op comparison | Owner confirms no "el reverso" |

## 6. Phase D: enemies and loot (D5, plus freeze fixes)

| Step | Work | Acceptance |
|---|---|---|
| D1 | Client: clamp replayed damage so a copy's HP stays > 0; never run 0x4D31C0 on a client copy; remove copies only through host despawn follow. Removes the 0x400 lift risk and phantom loot/XP. | Research54/log: no client DroppedItem from kills; copies never stuck dying |
| D2 | Spawn identity: use the native entry index (point+0x7A low 11 bits), pass count and type, plus the host spawn serial (B1). Log and exclude collisions. Clients spawn **on host spawn events**, not their own timers or random choice (consistent with D5 "one real"). | Pure tests for identity/collision; owner gate: no spawn-set divergence over two waves |
| D3 | **Host drops on every screen**: host captures DroppedItem creation (identity, item, quantity, position) into the presentation message; client presents the real drop at the same place. Pickup is executed on the host for whichever character collects it. | Owner gate: the client sees exactly the host's drops and can collect them |
| D4 | **Monster freeze fixes**: transform-only fallback for monsters whose pose/clip is unsupported (keep position and facing, skip channels); monsters never refuse the whole world frame during clip preparation | Research54: no `monster_gap` > 300 ms outside load; owner gate |
| D5 | Hit feedback: prove or fix the flash (0x4DFAB0) and stat-display (0x528FF0) lease live; stagger stays pose-driven | Owner gate: red flash, bar and numbers visible on the client |
| D6 | **Loot rule (D9 supersedes the original character-ledger rule):** copy host-collected ground/enemy/chest loot into each joined player’s native inventory and purse. Exclude shop/script grants; retain native stack limits and report caps. Each game keeps its own purchases and consumption. The older character-wallet foundation stays inactive; save/sidecar recovery belongs to I. | Source classification, delivery/dedupe and exact-image tests; ordinary pickups, farmhouse sword chest/equip, active Items/Weapons refresh, host-only shop purchases, Q self-use healing/control and chest florin credit accepted live. M self-use healing/effects/refresh/control, zero-host-stock use and Tal-collected ground florins also accepted live; no chest repeat through Kamo entry/return. The five scoped route checks have recorded passes; broader unload/re-interaction/reconnect dedupe remains follow-up coverage. |

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


### 2026-10-11 — automatic combat-barrier replication candidate

The owner resumed the six-family barrier plan after checkpoint `fc43d78`.
The earlier capture now establishes the host vine's complete lifetime:
opening clip, holding clip, closing clip, then native removal. The client
never created a placed vine during that capture. This is instrumented
`CONFIRMED_LIVE` evidence of the missing instance, not visual acceptance of
the new candidate.

`CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE`: native `InternalSpawnGates`
resolves the authored spawn group and clones the current zone's gate resource.
The six listed families share this path. Their setup heartbeats select opening
and looping animations. Spawn-point weak references identify placed actors
separately from the resource template. See the combat-gate entry in
`docs/research-log.md` for the native lifecycle and field evidence.

The candidate adds complete host gate-group liveness to the presentation
message. Native client creation uses the matching zone resource and authored
placement; host world snapshots drive animation and transforms. Per-occurrence
identities distinguish repeated same-resource gates. Native weak observers
track destruction, and host retirement ends each replica after the closing
animation. Only owned client gate heartbeats are suppressed; creation and
removal stay inside the existing scoped residency checks. Unknown observations
do not mean that a gate ended. The six allowlisted zone families share this
implementation; special ship, bridge and scripted variants remain unaudited.

Session protocol is **22**, presentation codec **3**; both peers must update
together. The presentation occupies three bounded datagrams and retains atomic
assembly, scene fencing and mixed-version refusal. This follows the adopted
D1 versioning decision; no save-format migration is introduced.

Dirty candidate based on `fc43d782b95fb52a786b35439c6bffd29cda44c8`, DLL SHA256
`c3d7e257b73578acf633bb59605eb3fd88ae90f8767861e14479f5d0913d3b2b`:

- `SudekiMP.StorySpawnTimingImageTest` passes, including named
  **`gate_native_code`**, 21 byte-check mutation refusals, native weak-reference
  destruction, address reuse, per-instance identity, heartbeat ownership,
  pending-setup retirement refusal, spawn-point reset and hook restoration.
- `SudekiMP.StoryEffectTransportTest` passes, including gate roster validation,
  unknown versus empty observations and the three-chunk transport.
- `SudekiMP.SkillTraceImageTest` passes, including the named
  `SudekiMpLanHitImageMatches` and `SudekiMpLanAimImageMatches` checks.
- `SudekiMP.StoryResidencyCallTest` and `SudekiMP.LanPartySessionTest` pass.

These are `CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE` results. The matching pair
was reloaded with its existing configuration; both native gate hooks installed
and client presentation/input admission resumed without a residency refusal.
That establishes live installation/readiness, not combat-barrier acceptance.

The first candidate encounter subsequently produced a placed vine on both
peers at identical authored coordinates. Both entered the opening and holding
clips; the client instance had one pause reference and completed native scene
setup. Its native creation reported no fault and containment remained admitted.
This is `CONFIRMED_LIVE` instrumented creation/animation evidence only; ending,
removal and the owner's visual judgment are still pending.

**Owner gate OPEN:** start the New Brightwater ambush and compare the doorway
on both screens. The vines must appear at the same doorway, animate through
opening/hold/closing and clear when combat ends. Check passage afterward.
A read-only capture is prepared for this comparison; gameplay inputs remain
with the owner. Repeated encounters, area changes, disconnect/teardown and the
other five families still need live lifecycle checks. Special scripted gates
remain a separate research pass. Phase D1 remains queued behind this detour.

### 2026-10-11 — New Brightwater vine acceptance; Spirit Strike audio follow-up

The owner reports the barrier correction is fixed. This accepts the visible
New Brightwater vine behavior on the paired candidate above, not the other
five families or special scripted gates. `CONFIRMED_LIVE` capture observed
opening, hold and closing selectors on both placed actors, followed by removal
on both. Client registry removal was about 3.6 seconds later than host removal;
the owner accepted the visible result, but this does not establish simultaneous
native retirement or a tested passage/transition/disconnect contract. Preserve
that timing as a lifecycle follow-up.

The owner separately reports missing Tal Spirit Strike sounds. Audio cause and
affected screen are not yet established; investigate character cues, global
Spirit startup cues and summon/effect sounds before changing playback. The
accepted vine correction remains in the current dirty candidate.

The owner clarifies that only Tal's client is missing the Spirit atmosphere,
echoing and roar; the host is audible. A 59-second read-only paired sound-pool
capture (`CONFIRMED_LIVE` observation, same candidate) finds native host cues
`spiritstrike_start`, `ALICE_SS_initiate_floorc`, `spirit_strike_init_1` through
`_4`, `roar`, `strike_spirit_travel_split`, `TAL_SS_attackatmos`,
`TAL_SS_strike` and `TAL_SS_morphtoTAL`, with none of those on the client.
`TAL_SS_strikevocal` and ordinary Tal attack cues are present on both.

`CONFIRMED_STATIC`: the saved-story character relay filters to party actors'
sound components. The legacy arena separately observes and relays the global
Spirit startup cue, but that observer is not bound by saved-story casts.
Summon replicas retain an isolated native sound listener, while their renderer
clock and their native entity/audio updates are distinct paths. A second
read-only capture is prepared to establish the client summon's sound-bank,
origin and authored event state during another owner cast. Those details are
still `UNKNOWN`; no audio correction or audible acceptance is claimed yet.

### 2026-10-11 — Tal Spirit audio bank and startup candidate

**Cause (`CONFIRMED_LIVE` + `CONFIRMED_STATIC`):** the second paired capture
finds both host Spirit resources bound to `TalSoundScript.xsb`; their client
copies use `sfx_generic.xsb` instead. Both peers have the same authored cue
names. The client sound origin reaches the correct presented position.
Native `PlaySFXWithAll` copies the caster's sound-bank name after its effect
factory; the parent-free client factory omitted that step. The existing
paused-effect updater already delivers native sound events.

The candidate restores that native bank binding before the first replica
phase seek for the two confirmed Tal resources, with native reference-count
ownership and hearing radius. It also binds the existing global Spirit sound
observer to the retained saved-story caster and sends only the witnessed
`spiritstrike_start` sequence/timestamp. The client consumes it once; duplicate
and late packets cannot replay it. SMP4 protocol is **23** (both peers together).
The accepted visual playback, camera and barrier behavior is retained.

**Validation:** dirty `fc43d78`, candidate SHA256
`981306ee1c295059a1a9d93435fb6356a24904fbe8fc7d3a072d362a457d4093`.
DLL builds; seven focused checks pass: `StoryCharacterAudioImageTest`,
`StoryCharacterAudioTransportTest`, `LanArenaSpiritAudioTest` (supported image),
`SpiritVisualTimingImageTest`, `LanArenaSpiritVfxTest`,
`StoryCastReadinessImageTest`, `StoryCastAdmissionTest`. New named byte guards
`SoundBankNameId`, `SoundBankSet`, `EmitterBankSet`, `ManagerBankFind` pass
including every-byte mutation refusal; existing named character-audio and
visual timing guards also pass. Native bank setter execution proves initial
binding, shared fallback replacement, balanced references, repeat idempotence,
hearing radius and refusal to release a final fallback reference. Cast tests
cover native scope, session/character/sequence fences and original emission
clock; transport/replay tests cover all clients, stale and repeated startup.

**Owner gate pending:** cast Tal's Spirit Strike from his client twice and
listen for startup, initiation, roar/atmosphere, impact and return cues on both
screens. Confirm the existing smooth animation, ending fade and normal control
return. Logs may prove native cues started but cannot establish audible quality.
Other roster Spirit resources and a disconnect/zone change during these sounds
remain unaccepted. No commit, push, merge or public build is claimed.

### 2026-10-11 — failed cast setup disables client effects; recovery candidate

**Owner regression report:** Tal cannot start his Spirit Strike and Ailish's
effects are absent on his client. The preceding audio candidate is **not
accepted**. Its host never completed shared cast binding: initialization
returned `ERROR_BUSY` at the formerly combined native-namespace stage, rolled
back completely, then permanently closed admission. Host effect capture and
publication depend on that same binding. The new Spirit sound observer was
never installed, so this run cannot establish its audible behavior.

**Correction:** retry a busy native camera-banking/construction boundary only
after complete successful rollback, with a one-second backoff and fresh roster,
menu and idle proofs. Invalid or unknown failures and retained native work
still stop admission. Each namespace operation now has a separate diagnostic
stage and caster index. Effect readiness logging reports actual binding and
continues to report health changes after the startup sample budget. The exact
busy substep in the earlier run remains `UNKNOWN`; the original native camera
can outlive skill-body idle, but that specific trigger is not proven by its log.

**Validation:** dirty `fc43d78`, protocol 23, recovery candidate SHA256
`deff1a5fbb6a0705ade45e5194bb05b24f2ef7baedcb0b69ff8c9c4bcf2eb3dd`.
Eight focused tests pass: the preceding seven plus `SpiritInstanceAbiTest`.
`StoryCastAdmissionTest` now runs the actual binding and rollback coordinator
against inert dependencies: second-caster busy refusal, clean rollback,
delayed retry, recovered cast/VFX readiness, fresh identity/menu checks,
invalid/unknown errors and failed restoration. This is `CONFIRMED_TEST`, not
native constructor or gameplay proof. Named audio, visual timing and cast
readiness image checks pass unchanged. Accepted barrier edits are preserved.

**Owner gate:** verify live binding and publication before asking for another
Tal Spirit Strike, then check startup, effects, audio and control return.
Distinguish Ailish's regular shots from individual skills/Spirit Strikes:
the latter still have incomplete saved-story effect coverage under #57.
This candidate alone does not establish those missing effects or full-roster
acceptance. No commit, push or merge.

After the owner focused the loaded client, this recovery candidate completed
live shared binding (`mask=12`, local Ailish), installed the Spirit sound
observer, and published host VFX successfully; the client adopted the stream
and reported ready. This is bounded `CONFIRMED_LIVE` setup/transport evidence,
not cast, visible-effect or audible acceptance. No busy setup refusal occurred
in the fresh run, so live retry remains unexercised. The read-only paired
capture awaits Tal's cast and identification/repetition of the missing Ailish
move.

### 2026-10-11 — Witch’s Kiss and ordinary skill effects across the roster

**Owner clarification:** the missing Ailish action is Witch’s Kiss, an ordinary
skill. The paired recovery capture shows Tal’s Spirit Strike activation and
body completion, and Ailish’s Witch’s Kiss running on the host. It does not
establish audible acceptance of the preceding Spirit audio change.

**Cause confirmed:** the saved-story visual witness admitted Tal casts and
ordinary Ailish/Elco weapon shots, but excluded other casters’ skill effects.
The closed resource list also omitted Witch’s Kiss and its shared startup.
The host capture contains both resources while the client contains neither.

**Candidate:** admit ordinary skill effects by exact roster caster and retained
emission-time cast sequence; preserve that source for asynchronous effects,
projectiles and late impacts. Add 62 original model descriptors covering the
four six-slot skill families, multi-part attacks, shared startup/targeting and
status descendants. Shipped animation-state roots and backing model metadata
were audited; camera models are excluded. Replicas use the existing isolated
sound-only cosmetic factory. They cannot forward damage, healing, status or
missile events into a client actor. Native actor sound-bank inheritance is
applied to these skill replicas. Demand-loaded skill cache leases are released
following complete host withdrawal and successful local retirement/detachment;
unknown observations and failed cleanup retain ownership.

**Validation:** dirty `fc43d78`, protocol 23, candidate SHA256
`4181a024b84d299c75f28957ed4bcf069e9048f5254ebd116fd13d9c8e364f87`.
Fourteen focused suites pass: `SpiritInstanceAbiTest`,
`StoryCharacterAudioImageTest`, `StoryCharacterAudioTransportTest`,
`LanArenaSpiritAudioTest`, `SpiritVisualTimingImageTest`,
`LanArenaSpiritVfxTest`, `LanArenaSpiritVisualHostTest`,
`LanPartyProjectileLifetimeTest`, `StoryProjectileVisualTest`,
`StorySkillVfxCodecTest`, `StoryVfxDispatchTest`,
`StoryCastReadinessImageTest`, `StoryCastAdmissionTest`, and
`StoryRangedTransportTest`.
The affected host/projectile fixtures passed again after the final scope guard.
Native checks use the supported image, including named sound-bank and visual
clock byte checks. Tests cover independent four-caster sequences, delayed
native finalization for every caster, late skill projectile identity, all 62
resource lifecycles, unknown/failed-retirement lease retention, wrong owners,
regular shots/shields and unchanged legacy codec rejection. The real four-peer
loopback fixture delivers all four independent skill streams to every client
and refuses client-side publication. No wire layout or
protocol version changed; the saved-story closed kind set is extended.

**Owner gate pending:** compare Witch’s Kiss on Ailish’s host and Tal’s client:
startup/sigil, main spell, target effects and natural completion; repeat once.
Recheck ordinary shots and Tal’s Spirit Strike for regressions. The common
implementation covers all four ordinary skill families, but each move’s visual
completeness, sustained status effects and concurrent casts remain unaccepted
until play-tested under #57. This does not establish missing full-roster Spirit
Strike resources or audio, effects created before observation, or live
transition/disconnect cleanup. Keep the full-party Buki/Elco gate open. No
commit, push, merge or public release.

After reloading this candidate, shared casting bound successfully and the
client adopted the effect stream. A read-only paired capture records one
Witch’s Kiss with both startup and main models present in both games. Their
animation data advance, both use `ALICESoundScript.xsb`, and both retire after
the host ends them: sampled startup duration 2.91 seconds and main duration
14.56 seconds on each peer (50 ms sampling; these are observations, not fixed
lifetimes). Neither resource remains afterward; the capture reported no read
errors. This is `CONFIRMED_LIVE` native replication/lifetime evidence. Owner
visual/audio acceptance, repeat-cast and full-roster checks remain pending.

### 2026-10-11 — remote idle lift report, read-only investigation

**Owner report:** Ailish appears to lift and settle in Tal's window, and Tal
does the same in Ailish's window. Keep this distinct from the previously
corrected movement-to-idle transition. The owner completed a paired idle and
brief movement capture; the recorded settled states were combat stances.

**Evidence:** unchanged dirty `fc43d78`, protocol 23, roster candidate
`4181a024b84d299c75f28957ed4bcf069e9048f5254ebd116fd13d9c8e364f87`.
The read-only capture contains 3,454 paired samples with no read errors.
During the initial stationary interval Tal's world-height range is below
0.000008 units. Ailish's range is about 0.0041 units and originates on the
host; it is not a client-only correction. Settled idle clocks advance without
an unexpected restart. A second bounded capture examines final model-space
bone matrices over several loops (575 samples per game, no read errors).
Root transforms stay constant; hips move while lower leg-chain endpoints
remain nearly planted. No substantial vertical discontinuity appears at the
captured loop boundaries. These are `CONFIRMED_LIVE` measurements, not visual
acceptance or a claim that the owner's reported difference is resolved.

Tal uses the original lock-on idle, selector 17, 46 frames at 12 frames/second,
in both the paired games and the existing native reference. At closely matched
phases, all 79 bone translations closely match that reference. The reference
clock is stationary, so this establishes only a pose comparison, not a native
full-cycle or frame-pacing comparison. Projected Ailish uses original armed
idle selector 20 and the accepted single-frame aiming overlay; the host's
first-person arms are not a third-person body baseline.

**Still open:** identify whether the visible difference occurs during settled
combat idle, the transition after movement, or weapons-away idle. A running
native comparison remains necessary to establish full-cycle visual parity.
No animation, position or clock correction was justified by this capture;
no production code, build, reload, commit, push or acceptance gate changed.

The owner then clarified that the visible movement happens in settled combat
idle and can appear to move the whole world in Ailish's view. A separate
read-only camera pass found a concrete lead: Ailish's first-person camera
repeatedly rises and falls by about 0.0041 units with her host world height,
despite zero movement demand. Tal's camera-height range remains below
0.000009 units. Camera orientation stays effectively constant. The pass has
934 validated host camera samples and 996 client/reference samples; 62 host
samples failed validation and are excluded, not interpreted as an inactive
camera. This narrows the investigation to host position/camera movement;
it does not establish the cause or prove that vanilla differs. The existing
native reference is paused and contains Tal only. A fresh read-only comparison
is armed for the owner to load Ailish and stand aiming in that native window.

**Follow-up source trace:** a bounded native height watchpoint caught both the
upward excursion and return through the original collision publication path:
RVA `32320` -> callsite `329FD` -> `110BD0` -> position setter `3050`.
The trace detached successfully after eight writes. A separate 696-sample
read-only movement capture shows zero requested/current speed, zero horizontal
and secondary movement accumulators, and ordinary animation speed factors.
Ailish's first-person movement uses the native direct-delta integration branch;
Tal uses the other branch. These observations narrow the source but do not
establish that the mod introduced the collision behavior. The native comparison
still has only paused Tal, so a running Ailish comparison remains the next gate.
No collision clamp, animation flattening or camera patch has been applied.

**Running native comparison and post-walk check:** the fresh native research
profile now supplies 816 consecutive neutral Ailish samples over 42.87 seconds
with an advancing native idle clock. Her height varies by about 0.000010 units
and camera height by 0.000017 units at the loaded New Brightwater location.
This is a running reference, but the ground differs from the multiplayer
pre-ambush location. Both versions use the same native first-person movement
branch and gravity value; no incorrect movement mode has been established.
After the owner walks multiplayer Ailish to another nearby point, 126 seconds
of settled samples retain a 0.00409-unit height range. A temporary bad landing
is therefore less likely; terrain and native update timing remain uncontrolled.
A native comparison at the established pre-ambush start is prepared and logging,
awaiting the owner's save load and aiming sample. No production fix or visual
acceptance is claimed.

### 2026-10-11 — ranged idle terrain-contact candidate

The owner clarified that the steady view is the **native Ailish same-ground
comparison**, not the multiplayer correction. Keep the multiplayer owner gate
open. Walking after test placement did not remove the measured multiplayer
height cycle; the native reference looks steady on the same road.

**Cause and scope:** exact-image inspection and eight bounded native contact
observations identify the small-penetration branch at RVA `33288`. For barely
touching terrain it adds `0.01` normalized collision units, despite penetration
below `0.00001`. Ailish's vertical radius converts that margin to about `0.00725`
world units; the remaining downward step explains the observed roughly `0.0041`
lift. The surrounding solver at `32320` skips small movement deltas. The
first-person gravity step crosses that threshold at longer frame times; this
timing explanation is an `INFERENCE` from the native math and passive capture,
not a controlled native frame-rate experiment.

**Candidate:** dirty `fc43d78`, protocol 23, DLL SHA256
`54cd58a1dfbb8b2128fc9c0b7c48b0962c18e57cbc0ff4b5b808291cc00e9c3e`.
A host-only adapter reduces the extra clearance to the existing
small-penetration tolerance for a current terrain contact, almost-upward normal,
stationary Ailish/Elco first-person movement and exact roster/control ownership.
It retains the original query, gravity and swept collision response. It does
not retain a terrain pointer or treat last frame's ground flag as proof.
Movement, casts, hits, steep surfaces, parented positions, larger penetrations
and uncertain ownership use the native path. Native research and client
profiles do not install this adapter.

**Validation:** five focused suites pass: `StoryIdleContactImageTest`,
`StoryAiMovementImageTest`, `StoryPlayerMovementImageTest`,
`StoryMeleeAdmissionTest` and `StoryCastAdmissionTest`. The new exact-image suite
checks `IDLE_CONTACT_CALL`, `IDLE_CONTACT_FRAME`, `IDLE_CONTACT_NORMAL`,
`IDLE_CONTACT_END`, `IDLE_CONTACT_THRESHOLD`, `IDLE_CONTACT_MARGIN` and
`IDLE_CONTACT_HALF` by name, including every-byte rejection. It also checks
register/flag/x87/SSE/LastError preservation, thread and identity refusal,
install rollback, restore failure/retry, and the original contact routine
over synthetic geometry. The synthetic tiny-contact output changes from
`1.009999037` to `1.000008941`; tested air, larger penetration, movement and
nonterrain responses match the original. This is `CONFIRMED_EXACT_IMAGE` and
`CONFIRMED_TEST`, not whole-game collision or visual acceptance.

**Owner gate:** reload the candidate pair; leave Ailish aiming still for
20 seconds, walk and settle again, then fire and check normal movement and hit
reactions. Compare her view and Tal's view of her. Record an advancing-game
height/camera capture and the owner's visible result separately. Elco and
other terrain/lifecycle cases remain unaccepted. No commit, push or merge.

The candidate pair has now been reloaded with the same test configuration;
the native comparison remains unchanged. A fresh read-only actor/camera
capture is armed. Loading and gameplay inputs remain with the owner, and the
multiplayer visible-result gate is still pending.

### 2026-10-11 — idle-contact first candidate failed acceptance

The owner reports that the lift-and-settle remains. The first candidate is
**not accepted**: the capture retains roughly `0.0041` world-unit idle height
and camera excursions, despite short steady intervals.

A bounded hardware-breakpoint trace identifies the missed native case.
The corrected `33288` path uses its small margin, but terrain penetration of
`0.000011444` crosses the `1e-5` branch boundary into the adjacent positive
separation path at `334A8`, which still adds approximately `0.01`.
Fourteen sampled contacts take the corrected path; two take that neighboring
path. All have zero movement demand. This is `CONFIRMED_LIVE` evidence of
the remaining branch, not a failure of roster admission. Software breakpoints
inside checked code invalidate byte guards; the preliminary software-breakpoint
admission observations were discarded and replaced by hardware observations.

The revised candidate covers both branches. The additional branch admits only
shallow depth from `1e-5` through `1e-4`, with both a current near-up terrain
normal and a near-up normalized separation vector. It clears the full bounded
overlap before adding the small margin, so the reduced clearance cannot leave
the actor below that surface. Deeper penetration and the backside response
remain native. Existing ownership, movement, cast and hit guards are unchanged.

Validation first ran in a separate executable-only harness after automatic
review rejected an unvalidated runtime expansion. All eight named contracts
pass, including new `IDLE_CONTACT_SEPARATION`, with ABI checks for both hooks,
two-hook install rollback and restore retry. The original native routine now
passes synthetic contacts on both sides of the boundary at both native single
and extended x87 precision. The observed-depth fixture changes its result
from `1.009994268` to `1.000010014`; larger penetration, air, movement and
nonterrain continue matching native. Integration was then approved and all
five repository suites passed again.

Revised DLL SHA256:
`7f200dba1ece5c364ea782ef700e0c71731f6e796ed001a5665c9842be045103`.
Dirty `fc43d78`, protocol 23. The same multiplayer owner gate remains open;
retest settled aim, walking/stopping and firing in both views. Elco, other
terrain and live lifecycle checks remain unaccepted. No commit, push or merge.

The revised pair is now reloaded and a fresh read-only capture is armed.
The client still needs owner focus/startup and the pair needs to enter the
loaded world before a new idle measurement is valid. No second-candidate
visual result has been claimed.


### 2026-10-11 — C5 grouped native input candidate; owner gate reopened

The owner explicitly requests the remaining C5 rewrite before moving on.
The earlier Tal acceptance applies to the separate-edge implementation; it
is not acceptance of this new candidate. C3/C4 and C6 keep their accepted
scope. D1 remains next after this C5 owner gate. The second ranged idle-contact
candidate is parked without a new owner result.

CONFIRMED_STATIC / CONFIRMED_TEST, dirty `fc43d78`: client Tal and Buki now
sample weak/strong/sweep together once per controller dispatch. Each button
uses native press/held/released/neutral states (`1→2→3→0`). Simultaneous
buttons remain in one snapshot. Melee no longer waits for the skill-action
result outbox, and the old edge-request path no longer executes melee in the
saved-story runtime. Skills and Spirit Strikes retain their existing outbox.

A bounded 16-change history accompanies each melee datagram (228-byte payload).
The host consumes each change at most once in order, supplies steady held or
neutral states between changes, and rejects stale input and retired leases.
Focus/menu cancellation removes pending history; a host input fence requires
neutral before rearming. Loss beyond the history/250 ms/eight-world-frame
bounds drops input rather than replaying old attacks. Saved-story gameplay
profile **21** requires the new channel; profiles 19/20 are rejected by the
handshake. Outer session protocol remains 23, with no save-format change.

CONFIRMED_EXACT_IMAGE: the new ABI adapter enters the retail combat-only join
at RVA `DB189`, with the original prologue's register/stack shape. It leaves
interaction globals untouched, preserves block priority, and lets native
combat/combos decide admission. It installs no additional patch. Tal/Buki
actor, arbiter, block, scene and control ownership remain validated; ranged
and special input modes are excluded. The named byte checks
`MeleeSnapshotSelection`, `MeleeSnapshotReturn`, `MeleeSnapshotReject`,
`SudekiMpStoryInteractionInputImageExact` and `SudekiMpStoryBlockImageExact`
pass, including mutation refusals. The isolated retail router selected the
expected attack for all 64 button combinations while preserving all tested
host latch/availability values. Held input did not repeat attacks.

The runtime DLL builds. Ten focused executables pass:
`StoryQuickInputTest`, `StoryInteractionGuardImageTest`,
`StoryMeleeTransportTest`, `StoryMeleeAdmissionTest`,
`StoryActionTransportTest`, `StoryActionOutboxTest`,
`StoryBlockTransportTest`, `StoryRangedTransportTest`,
`StoryCastAdmissionTest`, and `LanPartySessionTest`.
The tests cover bounded loss/reordering, duplicate/expiry/cancellation,
Tal/Buki transport and admission, simultaneous-button priority, host latch
preservation, stack balance, block priority and old-profile rejection.
Candidate DLL SHA256:
`266217fbdb80eb0e05753096529cab105915ee7aedef4dbf54379576fc833287`.

**Owner gate OPEN:** reload both peers with this candidate, then compare
client Tal's light/heavy taps, hold/release, HHL plus extra light follow-ups,
simultaneous M1/M2 and block/roll followed by attacking. Check for missed,
extra or delayed attacks, and that Q/Spirit still works. The owner retains
all gameplay commands. The candidate has not been deployed or play-tested;
no new gameplay acceptance, full-roster result, higher-latency timing parity,
or live disconnect/transition result is claimed. Buki's full-party check
remains under #57. No Phase D work, commit, push or merge in this pass.

### 2026-10-12 — owner-requested protected death/loot observation

The owner requests closing all games, reloading the pair at the established
castle-road approach, and protecting the party while Ailish starts combat.
This is a baseline observation for D1, not acceptance of D1 or the revised C5.
The reloaded candidate includes the C5 grouped input implementation.

Added disabled-by-default `[StoryAreas] TestPartyInvulnerable` for this test.
Only the saved-story host acquires the existing native party protection leases
on a witnessed game-thread dispatch. It does not change enemy health or game
speed. Partial acquisition retains cleanup responsibility; teardown retries
restoration of the previous protection setting. Client and native-reference
profiles do not acquire these test leases. Original launch configuration was
restored after startup; the running host retains its explicit test request.

CONFIRMED_TEST: runtime build and `StoryControlStatusTest`,
`CleanroomEngineTest`, `StoryMonsterDamageGuardImageTest`,
`StoryMonsterDamageFlushImageTest`, `StoryLootTraceImageTest` pass. Coverage
includes disabled/client/foreign-thread refusal, unavailable-state refusal,
partial acquisition, restoration retry and preserving prior protection.
Dirty `fc43d78` candidate SHA256:
`ee78eade13e22dc38f36645c17f2c9328f3eb9ef333a83b0d633453ee2b8cd9a`.

CONFIRMED_LIVE (startup only): both peers loaded; test placement reports two
actors placed without refusal. Host Tal and Ailish each have an acquired native
invulnerability reference and flag. A bounded read-only capture observes party
HP/protection, monster copies and DroppedItem objects on both peers alongside
the runtime's damage/despawn logs. Registry sampling is not an exhaustive
native death-entry or XP-award trace. Combat, loot comparison, revised C5 input
acceptance and live protection cleanup remain pending the owner's test.

### 2026-10-12 — first protected encounter: four host drops, none on client

Owner reports at least four visible drops. CONFIRMED_LIVE (bounded capture):
four distinct DroppedItem instances coexist on the host; none are observed
on the client. All 32 host despawn records match the client's 32 follow records
by identity, serial and death style; every client follow finds its copy, and
all 32 death-effect creations report success. All 37 logged damage replays
retain positive client HP (minimum 1), exact identity proof and no pending
flag restoration. No monster copies remain in the latest paired sample.

This supports the existing nonlethal replay and host-removal path for this
encounter. It also reproduces D3's missing real-drop presentation on the client.
It is not complete D1 acceptance: native death-entry and XP-award absence were
not exhaustively traced, and C5's revised input gate remains unaccepted. The
capture remains running; no gameplay, runtime or configuration change was
made during this inspection.

### 2026-10-12 — farmhouse missing-enemy correction; owner gate open

Owner confirms the missing-enemy encounter was the farmhouse and requests
broader enemy visibility. Investigation found a shared spawn-command loss:
Start2 selected sequence one on the host, but the client always replayed
Start1, which selects zero. The prior capture contains no client copies for
the host's farmhouse monsters. This detour addresses that failure without
claiming D1, D2 or the revised C5 owner gate complete.

The command path now carries all three native start forms, sequence selection,
pass count, scope and repeated-start serial. It preserves native resource
loading; there is no farmhouse-specific or enemy-type whitelist. Named byte
checks and actual default/selected native argument routing pass, along with
`StorySpawnTimingImageTest`, `StoryEffectTransportTest` and
`LanPartySessionTest`. See the matching research-log entry for evidence and
scoped-call test limits. Dirty `fc43d78` candidate SHA256:
`d881825b122f9b96a77a75291358660199b7c06d295fe2ca0b1a41701743471e`.
Saved-story gameplay profile 22 / presentation codec 4 require both peers.

**Owner gate OPEN:** replay the farmhouse fight from the usual protected test
setup. Confirm the enemies appear, animate, take host-authored damage and
leave both views when defeated. Compare monster identities and removals in
the paired capture. Then recheck the ambush and repeated encounters. Other
areas, bosses and scoped/random group lifecycles are not accepted by these
automated tests. Full D2 event-driven spawning remains separate work; D3's
missing real client drops remains reproduced. No commit, push or merge.

The candidate pair is now reloaded. CONFIRMED_LIVE (startup only): both peers
loaded, two actors placed, and host Tal/Ailish native protection references
and flags are present. Original launch INIs have been restored. A fresh
bounded read-only capture is armed. The owner has been asked to repeat the
farmhouse fight; no corrected encounter acceptance is claimed yet.

### 2026-10-12 — farmhouse spawn pause corrected; identity mismatch remains

CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST: `SpawnAttachPauseRelease` checks the
native SoloSpawnSetup attachment chain and its release of one initialization
pause reference. A completed, witnessed native job may change references from
2 to 1 while becoming scheduled. The client now accounts for that exact delta
without releasing its own world-pause reference. `StorySpawnTimingImageTest`,
`StoryClientRegistryTest` and `StoryResidencyCallTest` pass, including named
byte mutations, deferred completion and reference ownership. No new hook.

CONFIRMED_LIVE (owner control check): Tal remains controllable while farmhouse
enemies appear. The owner also suspects enemies vanished during combat; that
specific visual symptom is not established by the capture. The paired capture
does establish a separate D2 failure: both peers create fifteen monsters, but
four host removal records find no matching client identity and four client
copies remain after the host has none. Independent native spawn-point choices
still feed different portable identities. **D2 remains OPEN**; this is not a
passed farmhouse encounter or all-game enemy coverage.

### 2026-10-12 — host world cosmetics and nearby loot candidate

Implemented a saved-story host observer for enemy missile models, the audited
Kurian attack/block effect family, native item-drop models and pickup effects.
The client uses the existing parent-free cosmetic renderer with host transforms,
phases and stable shared visual instance IDs. It creates no native missile,
damage callback, DroppedItem gameplay object or local loot roll. Native weak
references distinguish destruction/address reuse and retain failed cleanup.
World observation gates presentation publication, independently of party casting
initialization. Unknown or overflowing observations do not publish an empty set.

Nearby, living, leased remote characters in the same ordinary area may invoke
the host's existing pickup routine for an observed drop. The host keeps native
inventory limits and consumption; presentation disappears after collection.
This uses a one-unit center-distance check; precise native collision-volume
boundary parity remains unverified. D6's individual reward ledger is unchanged.
The catalog includes 79 resolved item models, 22 encounter attack/block/missile
models and six pickup effects. Two unresolved item model identifiers remain
unadmitted; other enemy effect families require their own resource evidence.

CONFIRMED_TEST: eight suites pass: `StoryWorldVisualImageTest`,
`StoryProjectileVisualTest`, `StorySkillVfxCodecTest`, `LanArenaSpiritVfxTest`,
`LanArenaSpiritVisualHostTest`, `LanPartySessionTest`, `StoryCastAdmissionTest`,
`StoryVfxDispatchTest`. The new exact-image test executes native weak assignment,
destruction and the pickup decision/call path with inert inventory/render/consume
endpoints; it does not prove real rendering or real inventory changes. Named
checks `WorldVisualWeakDestruction` and `WorldDropCollect` reject every tested
byte/pointer mutation. Shared IDs, reused addresses, positive removal, unknown
and capacity failure, cap refusal/cooldown, dead/far collectors and single
consumption are covered. Existing casting and weapon-shot regressions pass.

Dirty `fc43d78` candidate SHA256:
`39d640898a5a8da0a01d48071b169343eb2fe2e01f9693f104cc15f6486d2a28`.
Saved-story gameplay profile 23 requires both peers; wire envelope and saves
are unchanged. **D3/effects owner gate OPEN:** compare host and Tal views for
real drops, nearby collection, enemy ranged shots and blocking visuals. Recheck
normal attacks/skills, cleanup and area changes. D1, D2 and revised C5 remain
separate open gates. No commit, push or merge.

### 2026-10-12 — farmhouse frozen/invisible-looking enemies: D2 reproduced

Owner reports enemies frozen in mid-air or never becoming visible on the current
protected candidate. CONFIRMED_LIVE (read-only native state, not a screenshot):
over twenty paired samples with both game frame counters advancing, all fifteen
host monsters change pose. Nine client copies receive matching world updates;
six (three scarecrows and three gnats) retain identical positions and all animation
channels. All fifteen client models have render objects, and the six frozen
objects have the renderer hidden bit clear. Native SoloSpawnSetup jobs complete
with the corrected pause accounting; party/world presentation continues. This
rules out the earlier whole-client spawn pause failure for this sample.

The frozen copies have no host identity match because current identity hashes
include locally randomized spawn-point indices. 7 of the nine matching IDs
also refer to different native entry indices across peers, so matching counts
alone are insufficient acceptance evidence. A stalled early pose can explain
an invisible-looking model, but that visual interpretation remains INFERENCE.
No missing model-resource failure was observed in this sample.

**D2 owner gate FAILED / remains OPEN.** Correct individual host spawn events,
entry/pass/occurrence identity and client creation before another farmhouse
acceptance run. Do not unpause client AI, force visibility, pair by proximity,
or treat group-start replay as sufficient. No production source change or
reload was made during this diagnosis. The separate D3 loot/effect candidate
and its pending pickup/block acceptance are unchanged.

### 2026-10-12 — D2 individual host spawn implementation, owner gate pending

Implemented in the dirty `fc43d78` worktree for saved-story LAN. Each observed
native SoloSpawnSetup construction records the host's group, selected point,
low-11-bit entry index, remaining pass count, resource, placement and a unique
monotonic occurrence serial. The monster's wire identity is that serial;
reusing a point/type/entry in another wave cannot reuse its identity. Native
intrusive weak observers retain group and actor lifetime, including asynchronous
loading. Serial conflicts and journal overflow fail closed and are not published
as an empty or truncated roster.

The authenticated effects stream now carries a complete active/pending spawn
roster (maximum 64). The client invokes the native single-spawn creation path
for each previously unseen occurrence. Its spawn-group update is suppressed,
so it no longer makes independent random choices or advances spawn timers.
Complete host absence removes only completed native jobs; pending work drains.
The older point-search despawn shortcut is removed. Spawn sigils share the new
occurrence identity while retaining host-observed end timing. Automatic combat
gates keep their existing separate lifecycle adapter.

CONFIRMED_EXACT_IMAGE: supported executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Native RVA `0x762B0` accepts the group in EDI with no stack arguments and returns
without popping arguments. It owns allocation, SoloSpawnSetup construction,
resource request, point weak assignment, packed entry flags and pass advance.
The existing constructor/step/destructor adapter remains the single seam owner.
Named contracts `SpawnEventChooseEntry`, `SpawnEventResourceName`,
`SpawnEventCreateAndAttach`, `SpawnEventWeakAssign`, `SpawnEventWeakDestroy`,
`SpawnEventMonsterDestroy`, `SpawnEventMonsterBaseDestroy`,
`SpawnEventGroupDestroy`, `SpawnEventGroupBaseDestroy`,
`SpawnEventEntityBaseDestroy` and `SpawnEventGroupUpdateABI` all pass, including
mutation refusal for every checked byte. The fixture executes native creation,
constructor and weak cleanup with inert allocation/resource endpoints; it does
not execute gameplay or demonstrate rendering.

CONFIRMED_TEST: six suites pass: StorySpawnTimingImageTest,
StoryEffectTransportTest, LanPartySessionTest, StoryWorldVisualImageTest,
StoryCastAdmissionTest and StoryVfxDispatchTest. Coverage includes duplicate
replay, entry/pass packing, point reuse across waves, pending-load retirement,
conflicting serial rejection, positive destruction, unknown preservation,
64-entry atomic transport, lost/reordered/stale packets, and older peer refusal.
Existing spawn-pause, sigil, gate, death-effect, cast and world-cosmetic checks
remain passing. Build and whitespace checks pass.

Saved-story gameplay profile 24 and effects codec 5 require both peers to update;
the outer envelope and save format are unchanged. Candidate DLL SHA256:
`ce1d4e49d4c02aede1678a2c4c9122876b4c4609167731820ed95bcd99827e42`.
The protected pair was reopened and read-only occurrence/pose logging armed.
**D2 remains OPEN at its owner gate:** verify the ambush and farmhouse through
at least two waves, equal host/client occurrence sets and entry/type/point
choices, advancing poses, no missing/frozen/leftover enemies, sigil endings and
continued Tal control. Other areas, exceptional non-group spawns, reconnect
while a native load is pending, and runtime teardown failure still need live
coverage. This does not establish all-game enemy coverage or close D1, D3 loot
acceptance, or the revised C5 gate. No commit, push or merge.

### 2026-10-12 — Tal loot observation during D2 testing

Owner reports an item appearing in Tal's play-test. CONFIRMED_LIVE (native
pickup result): the host recorded the remote Tal character collecting item
204, with successful native return and the drop consumed. This establishes
one client pickup of host-owned loot on the dirty D2 candidate; it does not
identify the killing blow or establish a separate client-side loot roll.
Both host party members retain the test invulnerability reference. The ambush
has twenty-two matching host/client construction descriptors so far; this is
structural evidence only. Farmhouse/two-wave visual acceptance and broader
loot/reward acceptance remain open. Read-only logging was restarted after the
previous bounded capture expired. No code change or reload for this observation.

### 2026-10-12 — D2 farmhouse delayed health-bar/death presentation candidate

Owner reports delayed client health-bar cleanup and death effects after returning
to the farmhouse. CONFIRMED_LIVE (observation, previous dirty D2 candidate):
matched farmhouse spawn/removal observations are within one 0.2-second sampling
interval; final removal is not the measured multi-second bottleneck there.
CONFIRMED_STATIC: client damage intentionally reserves positive HP, while death
cosmetics were only replayed at final host object removal. The recorded fatal-hit
to-removal intervals span several seconds. Ambush transport delays seen separately
are not explained by this farmhouse finding and remain a follow-up.

The candidate separates presentation from removal. The existing native animation
emission adapter observes the matched monster's independent authored death effect;
its spawn occurrence retains that receipt. World snapshots now carry host death,
model visibility and the observed death style. The client creates the visual once
at the presented pose, keeps the native object until complete host absence, and
suppresses duplicate creation at retirement. No client native death, loot roll,
XP award or async task cancellation is introduced. Pending native loads still drain.

Health/name widgets use the native hidden state after feedback updates; floating
killing-hit numbers and local number settings remain separate. Exact-image native
UI entries are RVA `0x15C2D0` and `0x15C230`, thiscall/no stack arguments/RET,
through owned CStatDisplay `+0xD0` and `+0x12C`. The full native `0x129360` cleanup
also clears floating numbers, so replaying that whole routine would regress #55.

CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST: eight suites pass:
StorySpawnTimingImageTest, StoryMonsterDamageGuardImageTest,
StoryMonsterDamageFlushImageTest, LanPartySessionTest,
LanArenaSpiritVisualHostTest, StoryWorldVisualImageTest,
StoryCastAdmissionTest and StoryVfxDispatchTest. The six named health-widget
entry/return/vtable checks and eleven existing spawn contracts reject every checked
byte mutation. Native widget propagation preserves queued damage numbers and HP;
death tests cover unrelated effects, the still-present host actor, wrong identity,
repeated receipts, and no second effect at final removal. The resource test endpoint
was corrected to initialize native `0x411730`'s complete 20-byte ESI output handle,
including its ResourceName record at `+0xC`; the prior incomplete fixture depended
on stack contents. These fixture results do not prove rendered timing.

Revision: dirty `fc43d78` on the existing shared-simulation worktree; supported
executable SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Candidate DLL SHA256 `3df9c68d3d42d6034290cb93f85858c4dce3d3d45fe75129aa7d8eb544f86ad4`.
Saved-story gameplay profile 25 / world frame version 9 reject the prior profile;
record sizes, effects codec 5, outer envelope and save format are unchanged.

**Owner gate OPEN:** compare ordinary and chunk-producing farmhouse kills on both
screens: health bar clears promptly, killing-hit numbers remain visible when enabled,
death effects start with the host and do not repeat at removal, models disappear
with the host, later waves and Tal control continue. The ambush and other enemy
families still need coverage. No broader D2/D3 acceptance, commit, push or merge.

The pair was reloaded with this candidate. Both native spawn adapters installed;
host party invulnerability and pre-ambush placement were confirmed. Original
configuration files were restored after startup. Fresh read-only health/death
logging is active for the owner gate; this is readiness, not visual acceptance.

CONFIRMED_LIVE (native observation, same candidate): the first ambush pass
records 22 host-authored death emissions and 22 matching client creations,
77–113 ms later by the paired native clocks, with no duplicate occurrence.
For sampled dying actors, client health/name widgets are hidden within one
0.2-second sample interval; native floating-number state 2 remains observable
while those widgets are hidden. No spawn/solo fault was sampled. This is
ambush structural/timing evidence, not farmhouse or visual owner acceptance.

### 2026-10-12 — client left outside freezes on temporary house entry

Owner reports Tal's world freezing when Ailish enters the farmhouse or another
house, then world motion returning without client movement after she exits.
The earlier farmhouse health/death presentation gate is still unconfirmed.
CONFIRMED_LIVE (observed failure): the outdoor zone follower requested the host's
state-4 temporary interior through SwitchZoneNOW. The client entered a loading
scene and advanced its local epoch; presentation resumed on return, while local
control remained pending. CONFIRMED_STATIC: the retained control selection
requires the original native party epoch. A zone residency report does not
transfer a character into a temporary interior.

The candidate restricts the ordinary follower to positively observed state-3
host and local zones. A state-4 entry, missing current descriptor or ambiguous
name report grants no outdoor zone call. The runtime uses the same report check
before releasing exterior effects. Temporary travel remains with its existing
per-character owner; the control epoch check is preserved. No native pause
reference, task or retained control lease is forcibly cleared.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: StoryAreaFollowImageTest,
StoryAreaStateTest, StorySceneAreaTest and StoryAreaTest pass. SwitchZoneNOW
(RVA 0x7990) and EnterZone (RVA 0x7970) byte guards are checked by name and reject
mutations. A hash-verified image fixture with recording zone-call stubs covers
host interior entry while the client stays outside, an already resident interior,
return to the same exterior, ordinary outdoor background load/entry, lighting
release refusal and an admitted local interior. It executes no real loading or
renderer. Protocol/profile and save format are unchanged.

Revision: dirty fc43d78; candidate SHA256 0e4dcfd057c57dc366a4a0d95cd873d5fc6dbd0aed9c0ced9b3e778e0dc15179.
**Owner gate OPEN:** leave Tal outside and moving while Ailish enters/exits the
farmhouse, repeat at another house, and verify his world and movement throughout
and after return. Then recheck farmhouse health-bar/death timing; the owner could
not confirm that preceding gate. No broader D2/D3 acceptance, commit, push or merge.

### 2026-10-12 — one delayed death after Tal's Spirit Strike

Owner noticed one enemy missing the other enemies' death cue. CONFIRMED_LIVE
(native observation, current house-transition candidate): the first ambush's
four actors reach zero host HP in the same 0.2-second sample. Three emit death
visuals together; the fourth emits 1.902 seconds later on the host, then the
client presents its matching receipt 86 ms later. That actor continues pose
channel 31 to its end before switching to death channel 20. All four receipts
arrive; this observation does not show a lost client death message. Why the host
continues that animation, and whether vanilla does the same, remain UNKNOWN.
No death-timing override is applied. House-entry/return and farmhouse death
visual acceptance remain open.

Follow-up: CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE identifies the actor as a
Kurian Trooper playing its combo slash. The shipped animation definition marks
all four death transitions from that slash `No interrupt`; the native transition
selector honors that flag. Its 42-frame clip at the observed 24 frames/second,
plus the authored approximately 0.15-second blend, accounts for the observed
1.902-second delay (INFERENCE from matching native data and live timing).
This supersedes the unknown animation-rule cause above. A matching untouched
vanilla play-test remains UNKNOWN. Details: research-log entry
“Kurian Trooper delayed death follows the authored combo transition.”
No gameplay timing change or owner-gate closure.

### 2026-10-12 — owner requests immediate Kurian death interruption; broader audit #62

The owner explicitly requested changing the identified wait. The saved-story
host now has a narrowly scoped candidate: a leased Kurian Trooper at zero HP,
native dead state and selected death action may leave COMBO3 before completion.
The native death predicate, transition blend, effect emission, loot and removal
still execute; the client follows the existing host receipt. Living actors,
client replicas, other creatures and other animations retain native rules.
No shared animation asset is edited. Broader creature coverage is tracked in
[issue #62](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/62).

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: StorySpawnTimingImageTest executes the
retail transition selector and death predicate and passes all four death types,
negative ownership/liveness cases and teardown retry. The named
KurianDeathTransitionSelect contract rejects every checked byte mutation.
StoryWorldVisualImageTest, StoryMonsterDamageGuardImageTest and
StoryMonsterDamageFlushImageTest also pass. Dirty fc43d78; candidate SHA256
4202b15dc1ef56613387efaad2a6dbfc84003f4b926801642cb63340f3ccd79e.
Protocol/profile and save format are unchanged.

**Owner gate OPEN:** kill a Kurian during its combo slash with an ordinary
attack or Spirit Strike. Verify the slash stops promptly, host/client death
effects agree, no attack continues after death, loot is not duplicated and the
next wave proceeds. This is not all-creature acceptance or closure of the
separate house-entry and farmhouse death-presentation gates.

The pair was reloaded with this candidate: both DLL hashes and saved-story
startup markers match; the host's Kurian exception and both spawn adapters
installed. Pre-ambush test placement reports two actors placed, none refused.
Original configurations were restored after startup. Read-only death/pose
logging is armed. This is installation/readiness evidence, not visual acceptance.

### 2026-10-12 — Kurian result accepted; health-bar comparison deferred to #63

CONFIRMED_LIVE (owner observation): the owner reports the Kurian death issue
resolved on the preceding candidate and authorizes moving on. This closes that
reported symptom's visual gate. The captured run records hook installation but
no override-admission event, so the specific interrupted branch remains backed
by the exact-image fixture rather than a new live admission trace.
The broader creature audit remains open under #62.

Per owner request, [issue #63](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/63)
tracks enemies reaching zero HP and health-bar cleanup versus vanilla, including
ordinary hits, skills, Spirit Strikes, simultaneous deaths and later waves.
Those comparisons are not silently accepted by the Kurian sign-off.

The active task returns to house travel: Tal stays functional while Ailish is
inside, but Tal and his world freeze when she exits. The house round-trip gate
remains open.

### 2026-10-12 — house-return follower rejects a retired target

CONFIRMED_LIVE / CONFIRMED_STATIC: after the host returns outside, its current
and loaded exterior are state 3, but its target still names the now-unloaded
farmhouse (state 0). That target is absent from the transmitted residency list.
The client accepted the name alone, requested the farmhouse, entered state 4
and lost ordinary exterior presentation/control while the host stayed outside.
The prior state-4 entry guard therefore did not cover the return path.

The follower now requires a matching host residency entry in states 1–3 before
loading a background target. A retired target cannot authorize a new load.
Following a positively resident host current exterior still works when its
background target is stale. Existing local/host exterior checks, native
pause/control epochs and asynchronous ownership rules remain in force.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: the retained-house-target regression
fails against the preceding implementation and passes with this correction.
StoryAreaFollowImageTest, StoryAreaStateTest, StorySceneAreaTest and StoryAreaTest
pass. The exact-image fixture checks SwitchZoneNOW and EnterZone by name and
rejects mutations of every checked byte. Ordinary outdoor load/entry and the
missing-current-exterior fallback are also covered.

Dirty fc43d78; saved-story profile 25/world 9/effects 5 unchanged. Candidate DLL
SHA256 ba6989b68a869c32551c74257d64bd8e6b86337771ebf0cc5f6ea5f568b1f05f.
**Owner gate OPEN:** leave Tal outside while Ailish enters, stays in, and exits
the farmhouse; verify Tal's movement and world updates throughout. Repeat with
another house and ordinary outdoor travel. Compilation and fixture results do
not close this live gate or the separate #63 death-presentation comparison.

Both test windows have reloaded with the matching candidate hash and successful
saved-story startup. Native spawn adapters installed, pre-ambush placement
reports two actors placed and none refused, and the host's Ailish/Tal both have
one test invulnerability reference. Original configurations were restored after
startup. Read-only area/party capture is running for the owner's round trip.

### 2026-10-12 — client native XP popup candidate

The owner clarified that the requested text above a dead enemy is the XP
reward. This adds that presentation to the existing host-owned death path;
client gameplay death and local XP/loot generation remain prohibited by D1.
The reported 128/64/0 numbers are not established as XP or corrected by this
change; their damage/health comparison remains under #63.

CONFIRMED_STATIC / CONFIRMED_LIVE: native monster rewards call the separate XP
popup manager after awarding experience. The observed host pool contains used
XP popups, while the corresponding client pool remains unused. The candidate
captures the host's actual active native popups, including displayed value,
position, height and authored lifetime, and sends an optional cosmetic snapshot.
Clients invoke only the native popup entry, once per observed pool occurrence.
Late snapshots retain elapsed lifetime; wrong-area, stale-scene and unknown
owner observations cannot start a popup. Native display enable is respected.
Ordinary damage-number and host death-effect paths remain independent.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: StoryXpImageTest and
StoryXpTransportTest pass, including all three observing clients, native ABI,
pool reuse, duplicate suppression, authored duration, expiry, full-pool retry,
wrong-area refusal and named byte checks XpPopup, XpFreeSlot, XpSetupHead and
XpSetupReturn. LanPartySessionTest, StoryAreaFollowImageTest and
StoryMonsterDamageFlushImageTest also pass. The rendering leaf is a recording
endpoint in the exact-image fixture; real rendered XP acceptance is UNKNOWN.

Dirty fc43d78; candidate SHA256
4ed6a92f9162ebc2003d0d2a505f709ffcc13033da1e81409eb4c71886f108f8.
The XP packet is additive; saved-story profile 25/world 9/effects 5 and saves
are unchanged. Older peers can ignore it without granting gameplay authority.
The running pair still uses the preceding house-return candidate.
**Owner gate OPEN after reload:** ordinary and simultaneous kills show the
host's XP values once, at the same death locations, and finish naturally on the
client; damage numbers, death effects, loot and subsequent waves remain normal.
The house-entry/return gate is still awaiting explicit owner confirmation.


### 2026-10-12 — Tal lateral idle drift beside scenery

Owner reports Tal shifting sideways and correcting on both screens. A paired
five-minute capture shows roughly 0.006 world units of X excursion, repeating
about once per second with neutral movement input. This is actual host actor
position movement; client replication follows it.

CONFIRMED_LIVE / CONFIRMED_STATIC: bounded hardware observations identify native
collision publication as the writer. The small terrain-contact branch adds
0.01 normalized clearance along a gentle sloped normal. This repeatedly nudges
Tal toward adjacent type-2 scenery. That contact eventually produces about
-0.0101 normalized X correction, or -0.0061 world units at Tal's collider scale.
The original ranged-only idle correction excluded Tal and this slightly steeper
floor. No game values were changed during observation; all watches detached.

The candidate extends the existing two guarded seams to Tal/Buki's native
velocity mode and gently sloped terrain (normal Y >= 0.998). Ranged actors keep
their separate integrated-delta bound. Current host roster/control ownership,
neutral movement, shallow terrain contact and ordinary-world/cast guards remain
required. Wall/prop response, deeper contact, air and intentional movement retain
their native paths. There is no network smoothing or position-lock change.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: five suites pass: StoryIdleContactImageTest,
StoryAiMovementImageTest, StoryPlayerMovementImageTest, StoryMeleeAdmissionTest
and StoryCastAdmissionTest. The added regression fails against the preceding
adapter. Synthetic slope geometry executes the original contact routine for all
four actors at both x87 precisions; the measured small-branch lateral push falls
from about 0.00039954 to 0.000000439 normalized X units. All eight named native
byte contracts, ABI preservation, rollback/restore retry and existing contact
refusals pass. This is fixture evidence, not live visual acceptance.

Dirty 199956d; saved-story profile 25/world 9/effects 5 unchanged. Built candidate
SHA256 ed275320c18cd12ec733d45fb34b84b2ff79dad5229f0670e5d3944a6f1c1fe6.
**Owner gate OPEN after reload:** idle at the reported sloped spot beside scenery
and verify the sideways/reset cycle on both screens is gone; walk away and back,
stop, block/dodge and attack; check gentle slopes and walls remain traversable
and solid. Other terrain/roster live coverage remains unverified. Candidate is
not deployed. The separate house-return and XP popup owner gates remain open.

Owner requested reload: both games now load the matching lateral-contact candidate,
with successful saved-story startup and native placement at the reported New
Brightwater location. Host test invulnerability is confirmed for Ailish and Tal.
Read-only position capture is running. Initial samples still contain gradual
neutral-input position drift; full stability is not established. The owner
visual gate remains OPEN. This build also contains the unaccepted XP popup
candidate; its separate kill-test gate remains open.

### 2026-10-13 — owner gates on the Tal drift candidate; house return fails again

Pair reloaded on the `ed275320…` candidate (dirty `199956d`; the five idle-contact
suites re-run and pass). Owner results on the castle-road route:

- **C5 revised Tal input: accepted** ("1 is good"); follow-up tracked as #64.
- **XP popups: seen on the client** ("yeah I can see them"); the XP award and
  party distribution question is #65. No change expected.
- **Tal lateral idle drift: not observed** at the owner's current spot. The
  reported New Brightwater slope is still untested; gate stays open.
- **House return: FAILED again** after a couple more farmhouse exits.

CONFIRMED_LIVE / CONFIRMED_STATIC root cause of the new failure: the client's
`story_area_follow event=switch target=LICo_Frappe_Farm` fired while the host was
*entering* the house. The native TEMP transition background-loads the house like
an ordinary target: the host report carried `target=LICo_Frappe_Farm` with a
residency entry at state 1–3, which is exactly what the follower treats as a
positively loading outdoor target (the 2026-10-12 guard only covers the
zero-state exit case). The client called SwitchZoneNOW, dropped 35→12 bound
actors, lost its party binding (`native_scene … lead=4 count=0`),
`pause_invariant_failed`, and froze. The `temporary=` name arrives on the
separate presentation channel, so it cannot order against the area report.

Fix (host side, no wire change): `SudekiMpLanStoryAreaNoteTemporary` records the
name the native TEMP export is entering (from the existing zone observer, before
the native call); `SudekiMpLanStoryAreaCapture` withholds that name as the
background target (`story_area_capture event=target_withheld`). Residency entries
are unchanged, so the client still holds on the state-4 house as before.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: StoryAreaFollowImageTest gains the
loading-house capture case (target captured, then withheld after the TEMP note,
withheld report moves nothing, note release/replace); it, StoryAreaStateTest,
StorySceneAreaTest and StoryAreaTest pass. DLL build passes (pre-existing
warnings only). Candidate SHA256
`9c4d632b6838bec916a949f115a50f791ea9dde757a84a86fb94731440a1a20f`, not deployed.
**Owner gate OPEN after reload:** Tal outside, Ailish enters/stays/exits the
farmhouse several times in a row, then another house; Tal keeps moving and his
world keeps updating throughout. The New Brightwater idle-drift check and the
simultaneous-kill XP check remain open.

### 2026-10-13 — house entry/return accepted; chests and scripted doors reported

CONFIRMED_LIVE (owner): "I think the farm is good" after repeated farmhouse round
trips with Tal outside on candidate `9c4d632b…`. Host log shows
`story_area_capture event=target_withheld zone=LICo_Frappe_Farm` on each entry;
the client issued no `switch` and no `pause_invariant_failed`. House-return gate
closed for the tested route; other houses remain unverified.

New owner reports (Phase H scope, D4): chests can only be opened by the host's
Ailish and an opened chest is not shown opened on the client; doors a host
scene transition opens stay closed on the client, and Tal walks through the
closed door. CONFIRMED_STATIC: chests (kind 0x0f9c, vtable 0x2d5200) and door
triggers (kind 0x0fa6, vtable 0x2d5088) are world-registry entries on both
peers with matching identifiers, but `entity_identity` (lan_story_world.c)
admits only scenery, monsters, NPCs, allies and party actors, so their state
is never replicated. Tracked as #66 (chests) and #67 (doors). Ordering against
Phases D–G is an owner decision.

### 2026-10-13 — E1 Shooter missiles: owner sees them; the missile entity is not replicated

Owner demonstration on the castle-road ambush (candidate `9c4d632b…`). From the
log marker: the host spawned 42 `Missile` entities (vtable 0x2D5A10, kind 0x0F97)
and 91 `SpecialEffect`s; the client registry gained 0 missiles and 122
`SpecialEffect`s. CONFIRMED_LIVE: what the client shows for a Shooter shot is the
missile's effects, carried by the existing effects channel; the MissileEntity
itself (position, impact, collision) exists on the host only. Owner judges the
result visually good. E1 is therefore reduced to a verification item — impact
position/timing and any missile-model cases — not new replication work, unless
the acceptance route shows otherwise. E2 (allied guards) is unchanged: two
`ALLY_NW_Guard` (kind 0x0F82) on the host, none on the client.

### 2026-10-13 — Tal lateral idle drift: owner gate passed at New Brightwater

Pair reloaded at the reported New Brightwater slope (test start leader
-42.37,-5.03,350.35 / Tal -47.40,-4.81,352.20) on candidate `9c4d632b…` (same
idle-contact code as `ed275320…`). Owner: "no drift as far as I can tell".
CONFIRMED_LIVE (read-only 50 ms capture, 711 neutral-input samples / 36 s): the
sideways snap-and-reset cycle is gone — zero adjacent X steps above 0.002 on
either peer (the 2026-10-12 capture had the ~0.006 cycle about once a second).
Host and client agree to 0.0001. A slow neutral creep remains (X 0.017, Z 0.139
over 36 s, ≈4 mm/s, not visible to the owner); whether vanilla Tal creeps the
same way on this slope is UNKNOWN and is not part of this gate. Gate closed for
the reported spot; other terrain remains unverified.

### 2026-10-13 — slow neutral slope creep is native

Owner then saw both characters move over a longer wait at the New Brightwater
slope. Host hardware watchpoint on Tal's Z (12 hits, detached): every write is
the native collision publication (`CPosition::SetPosition` RVA 0x13050, caller
RVA 0x120C51, from the movement solver near RVA 0x436EC), each frame moving Tal
≈ −0.00009 Z / +0.00001 X. Ailish creeps the same way (−0.023 Z / 6 s vs Tal
−0.025 Z / 6 s), so it is a slope/solver property, not the idle-contact adapter.
CONFIRMED_LIVE (vanilla): Codex's 2026-10-12 same-ground capture of the
*unmodified* game shows native Tal creeping 2.4–6.0 mm/s and native Ailish
2.4 mm/s while stationary with the clip clock advancing, on the castle-road
ground; the co-op pair measured 0.8–1.8 mm/s there and ≈4 mm/s at the NB slope.
INFERENCE: the creep is retail behaviour (gravity resolved along a walkable
slope) and scales with the slope. Removing it is a gameplay-semantics change
(owner decision). The sideways snap cycle fixed earlier was a separate, mod-era
visible artefact and stays fixed.

### 2026-10-13 — Phase D kill audit (castle-road ambush, candidate `9c4d632b…`)

Read-only captures plus both runtime logs from a log marker; owner fought both
waves with Spirit Strikes and some Ailish shots, invulnerability on.

- **D2 spawn parity — CONFIRMED_LIVE:** host 22 monsters (16 Levy, 4 Trooper,
  2 Shooter), client 22 identical copies; one group start, same selection; 18
  host despawns, 18 client follow-ups all found. One capture sample (of 3784)
  showed host 0 / client 10 monsters during a host registry rebuild; harmless.
- **D1 nonlethal replay — CONFIRMED_LIVE:** 56 host hits replayed, all proven,
  copies clamped to 1 HP and removed only by host despawn; no client loot or
  gameplay death. XP: 18 popups presented once each.
- **D1 death presentation gap:** 16 `death_emitted` receipts for 18 deaths.
  Identities 7 (style 2) and 0xC (style 4) reached 0 HP on the host but the
  4E2810 observer receipt never matched, so their client copies vanished on
  despawn with no death animation (owner did not notice). Suspect: Spirit
  Strike kills use a different death effect. Added an observe-only
  `death_receipt_refused` log (serial, style, prepared, effect kind/resource vs
  expected) to `lan_story_spawn_events.h`; rebuild `5cfbd199…` for the repeat.
- **D3 — owner:** drops appeared on Tal's screen; Tal and Ailish each collected
  some; pickups execute on the host (2 logged, items 204/341) and the host gets
  the notice. Visible-and-collectible accepted; *credit* to the collector is D6.
- Client: four ~100 ms frame hitches in the big wave; no faults.
- Owner asks for remote players to be "full persons" (chests, doors, pickups
  credited to them): Phase H / D6 — ordering decision pending.

### 2026-10-13 — D1 death receipts: Spirit Strike run reproduces nothing

Repeat ambush on candidate `5cfbd199…` (adds the observe-only
`death_receipt_refused` log), owner killing with Tal's Spirit Strike only.
CONFIRMED_LIVE: 32 monsters on each peer, 32 host despawns, **32 `death_emitted`
receipts, 32 client `death_presented`** — no missing death animation. Every
refused receipt (32 monsters × 1–10) was a non-death emission 0.3–33 s before the
death: the spawn-in effect (`7e8a6f3f` / `af48a1bf`) and hit effects
(`9a1ba193`, `37a3bf5f`, `dee1c453`), all with death style 0 as intended. Death
visual assets this run: `4f0291d3` (styles 2/4), `162ab413` (style 4).
Conclusion: the earlier 2-of-18 gap is intermittent and happens before the
resource check — identity lease, component back-link or thread/arming — none of
which were logged. Next build logs those exits for leased monsters. Spirit
Strike kills are not the cause (HYPOTHESIS retired).

### 2026-10-13 — owner reorders: Phase H (doors, chests, pickups) before E/F/G

Owner decision after the kill audit: remote players must be "full persons" —
start Phase H now, doors first (#67), then chests (#66), then pickups credited
to the collector (D6 rule). D5 (#63) and the rest of D6 follow; E/F/G after H.
D2 spawn parity and D3 visible/collectible drops are accepted; D1 keeps the
intermittent death-receipt hunt open with the widened log (build `ac1f23ae…`).

### 2026-10-13 — H1 doors: native door model and live cycle (research, no code)

CONFIRMED_STATIC (Ghidra, exact build) and CONFIRMED_LIVE (read-only polling of
all resident doors on both peers while the owner used the road's TallGate):
`DoorEntity` (vtable 0x2D6430, node 0x2D6454, kind 0x0F8C) embeds `CPosition`
at +0x150 (→+0x44), `CStats` at +0x260, `CDoor` at +0x3B0 (vtable 0x2D637C) and
the animation component at +0x4F0 (vtable 0x2C8504, →+0x58). `CDoor` state is
+0x74; the player-use cycle on the host ran 0→2→3→5 (door anim 0x17 into +0x60,
collision byte position+0x100 cleared) →6→7 (close anim 0x18) →0 in ≈4 s; the
script path is `CDoor::ActivateFromScript` 0→0xB→0xC→0xD / 0xD→0xE→0xF→0.
The client's three doors share identifiers and model topology (renderer
0x2DF8EC, 4 channels, 1 submodel) but their entity nodes are paused and `CDoor`
never left state 0 — the door stays closed on Tal's screen; he passes through
because the host simulates his position with the host's collision already off.
Fix direction chosen: admit `DoorEntity` to the world catalog as a new kind so
the door's animation pose follows the host like NPC/scenery poses (no client
state machine); PVS portal state (`CSceneManager::SetPVSDoorState` RVA 0x1B6C0)
is a follow-up if the open doorway renders wrong on the client.

### 2026-10-13 — H1 doors candidate: DoorEntity joins the world catalog

`SUDEKIMP_LAN_STORY_WORLD_DOOR_KIND` 0x0F8C added to the world frame (world
version 9→10; validator accepts the kind with no flags/hit points); the world
sync admits `DoorEntity` (vtable 0x2D6430 / node 0x2D6454) with its embedded
CPosition at +0x150, four channels and bank entry zero authored (like scenery).
A malformed door is skipped, never refusing the frame. No client state machine,
collision or PVS change: only the door model's pose follows the host.
CONFIRMED_TEST: LanPartySessionTest (wire version, door kind accept/refuse
cases), StoryShotsTest, SkillTraceImageTest pass; DLL build passes.
CONFIRMED_LIVE (startup): on candidate `5392ea2f…` both peers catalogue the
same three doors (Portcullis, TallGate, BigDoglegDoor; 3/7/3 animations), no
stall, no owner change. **Owner gate OPEN:** open the road TallGate with Ailish
and watch it on Tal's screen; then the castle portcullis.

### 2026-10-13 — H1 door pose accepted; door camera requested

CONFIRMED_LIVE (owner, candidate `5392ea2f…`): "he walked through and the door
actually opened" — the TallGate opened on Tal's screen as Ailish used it. The
door-pose replication is accepted for the tested doors. Owner follow-up (H1b):
Tal's camera should take the door's authored static camera during his
walk-through, as the game forces for the lead (CDoor "Static camera to use from
front/back", +0x78/+0x84; "Player traverse distance" +0x9C). Research next:
how the native lead path applies that camera (state 3/4 helper RVA 0xCEA30 and
the traverse helpers) and the client camera facility to reuse.

### 2026-10-13 — H1b door camera: native mechanism (research, no code)

CONFIRMED_STATIC (Ghidra, exact build): when the lead traverses a door, the
CDoor state 3/4 helper (RVA 0xCEA30) picks the authored camera ResourceName at
CDoor+0x84 (front) or +0x78 (back; flags bit 2) and calls
`CCameraManager::SetStaticCameraPosition(mgr 0x3C2F30, "default", &name, 0.0,
false, 0.0)` (RVA 0x38500): the named camera slot ("default" = the player
camera) resolves the camera resource (RVA 0x3E540 → 0x108750 via the resource
manager 0x408D10) and starts a `Camera::StaticPositionTransitionTarget`
transition on the camera controller 0x3D4B30 (RVA 0x58D510 → 0x4E79A0). The
player-use cycle ends it at state 10 via RVA 0x4E93B0 on the camera's state
object (+0x40, kind 0x13). Live door camera names: TallGate front/back resource
ids 0x0135AF49/0x0135AF48, Portcullis back only (front = none 0x7FFFF),
traverse distance +0x9C = 5.0 (TallGate/Portcullis) / 2.5 (BigDoglegDoor).
Constraint: the client camera is the mod's mathematical view
(`SudekiMpLanStoryLocalControlPresent`, no native camera tick), so the native
static transition cannot be reused there. Direction: build a `SudekiMpLanStoryView`
from the door's camera resource (position/orientation fields still to be read
from the transition target's methods) and present it through the existing
cast-view path while the remote character is within the door's traverse
distance of an open door; front/back chosen by the character's side.

H1b design (proposed, not started): host-computed door camera for remote
characters. `Camera::StaticPositionTransitionTarget` (vtable 0x2D9764, slot 0 =
RVA 0x18B5C0) copies the camera resource's +0x8/+0xC/+0x10 into the camera
position and, unless resource+0x2C is set, keeps the player as look target —
so a door camera is "fixed authored position, look at the character". Host:
per remote character, when a catalogued door's CDoor state is non-zero and the
character is within +0x9C of the door position, resolve the front/back camera
resource (side from the door forward), build a look-at `SudekiMpLanStoryView`
(projection copied from the host render camera) and publish it through the
cast-view channel as a new kind 3 (validator currently caps kind at 2; client
presents any non-zero kind, kind 0 releases). Unknowns: the camera resource
lookup on the host (engine `SudekiMpResourceLookup` resolves proxies of type
41 only; the camera type id and the native lookup RVA 0x3E540/0x108750 need an
exact-byte check), the resource's own look target when +0x2C is set, and the
front/back mapping (calibrate against the lead's choice live). Estimate: one
to two sessions with two owner gates.

### 2026-10-13 — Phase H trace: native interaction system (static, exact build)

Owner direction: after the door camera, trace the interaction system so remote
players can act as real players. CONFIRMED_STATIC (Ghidra decompiles of the
plan's RVAs; the mod's `interaction_provenance` observer already names them):
- Trigger manager global 0x408D24: up to 15 candidate records (+0x20, stride
  0x1C: kind at +0, owner at +4, usable/trigger entity at +0x10, flags at
  +0x19/+0x18, state at +0x20 = 2 while interacting), press latch +0x1D8, flag
  byte +0x1D9 (bit1 = candidate has OnAction, bit0 = usable, bit3/4/6 misc),
  last-contact +0x1D4.
- RVA 0xD330 (per-frame scan) collects usables/triggers around the native
  **front** character (group players 0x408D94) and writes +0x1D9 flags.
- RVA 0xD6F0 → 0xD7A0 (dispatch): for each candidate, with the latch set and
  the source actor equal to the current front actor (RVA 0x1122C0 accessor),
  gate 0xD9A0 checks OnAction presence, the front arbiter's busy bit 0x80,
  combat state (cluster manager +0x34) and the UI state, then marks the
  candidate state 2 and 0xCCD0 builds the SOL message ("OnAction" / "OnEntry" /
  "OnExit") carrying the acting actor pointer (+0xC) and a "source is front"
  bool (+0x40), queued through 0x1B9EF0.
- Weak attack press writes the latch (0xDB11A) for the front actor only; the
  mod's melee admission refuses remote presses there (story_interaction_guard).
Consequence: everything a chest, door or NPC does is a script message whose
actor argument is the front character. A remote "player" needs (1) candidate
discovery around the remote actor (the provenance module has a read-only
nearby-usable query seam) and (2) a host-executed dispatch with the remote
actor as the message actor and as the "front" for that scoped call — the
"scoped interacting-actor swap" of plan D4. Door use by a remote actor would
then run the native door traverse for that actor, and the door camera call
(0x38500) during that scope must be redirected to the client (H1b), not applied
to the host camera. Next: run the existing provenance trace on the host while
the owner uses a chest, a door and an NPC to confirm the live chain.

### 2026-10-13 — H1b door camera: live chain captured (host, TallGate)

CONFIRMED_LIVE (gdb hardware breakpoints on the host, two passes): per pass
`CDoor` traverse helper (RVA 0xCEA30, door pointer on the stack) →
`CCameraManager::SetStaticCameraPosition("default", &camera, 0.0, false, 0.0)`
(RVA 0x38500, called from RVA 0xCEAEF) → `Camera::StaticPositionTransitionTarget`
start (RVA 0x18B5C0). Side rule: flags +0x73 bit 2 clear → the +0x84 camera
("from back" slot; TallGate id 0x0135AF48), bit 2 set → the +0x78 camera
("from front"; id 0x0135AF49). Camera resource record (0x30 bytes, vtable RVA
0xCDD70): +0x08 position, +0x14 look direction (unit), +0x20 ResourceName
{kind 0x0FBA, id, refs}, +0x2C flag 1 = fixed direction (these do not track the
player). TallGate: af48 at (-121.6, 9.6, -69.8) looking (-0.69,-0.19,0.70);
af49 at (-134.4, 11.1, -56.1) looking (0.60,-0.42,-0.68); both aim at the gate
(dot 0.99). Blend time 0. So Tal's door view is fully determined by: which door,
which side, the two authored records — no native camera needed on the client.

### 2026-10-13 — H1b door camera candidate (host-computed, cast-view kind 3)

New `src/network/lan_story_door_view.[ch]` (pure: side selection — the camera
standing on the character's side of the door plane, as the lead path chooses —
and the camera-to-world view from the record's position/direction, or a look
at the character's +1.8 target for non-fixed records) and
`src/hooks/lan_story_door_camera.[ch]` (host: exact-image guards on the
static-camera lookup RVA 0x3E540 entry and SetStaticCameraPosition's call to it
at RVA 0xE7DD9; the lookup is invoked as the game does — EAX=-1, ResourceName
by value with a NULL reference pointer, ESP restored — and the returned record
is proved by its vtable 0x2CDD70 and id). For a non-leader character inside an
open door's traverse zone the host rewrites that character's kind-0 cast view
to kind 3 with the door view and the host camera's projection; release at
traverse+2.0 or 1.5 s after the door returns to state 0. Cast-view codec admits
kind 3 (rejects 4). Installed with the cast module once a remote player is
ready; uninstalled with it.
CONFIRMED_TEST: StoryDoorViewTest (TallGate numbers, both sides, single
camera, tracking record, invalid inputs), StoryCastViewTest, LanPartySessionTest
pass; DLL build passes. Candidate `2d34815d…` deployed to the pair.
**Owner gate OPEN:** Ailish opens the TallGate; Tal walks through while it is
open; Tal's camera should take the gate camera on his side and return after.

**2026-10-13 host crash on the door-camera candidate `2d34815d…`** (owner: "I just
crashed"): host access violation at EIP 0xFFFFFFFF with ESI/EDI holding the
camera ResourceName kind/id — the mod's static-camera lookup thunk. Cause: the
inline-asm output was not early-clobber, so the compiler placed the call
target in EAX, which the thunk then set to -1 (the native type selector) before
`call`. Fix: `"=&a"` output, call target held in ECX (a stack-relative memory operand
would be mis-addressed after the pushes — caught in the object dump), kind/id
pinned to EDI/ESI, plus a research log line before each lookup. No native state was
written before the fault (the lookup had not run). Rebuilt; redeploy pending.

### 2026-10-14 — H1b door camera: works mechanically; the real complaint is the client camera itself

CONFIRMED_LIVE (candidate `7b5407fc…`): host `story_door_camera event=engaged
character=2 camera=1 state=5 distance=2.25` → client `story_cast_view
event=client character=2 kind=3` → `released distance=4.76 state=0`. Owner: "it
kinda works" — accepted as mechanism, not as feel. Owner's actual frustration:
Tal's camera "doesn't have the same boundaries as Ailish's — movement
limitations based on how close the character is to a wall". That is the
documented limit of the client's mathematical orbit view
(`SudekiMpLanStoryLocalControlPresent`: no native collision avoidance, return
steering or spring smoothing). Direction to research (H1c): drive a native
Exploration camera on the client for the remote character (precedent: the
2026-08-27 Player 2 native camera bridge — `CCameraManager::SetCameraTarget`
RVA 0x37170 + `SetCameraState(Exploration)` RVA 0x37530 — whose authored
obstruction logic pulled Camera 2 in at walls), ticking it despite the paused
client world, with the door/cast views layered on top as today.

### 2026-10-14 — owner decision: client native camera (H1c, #71) before chests

Owner: "camera first. I want him to have as close to the real thing as
possible if not 1-1." Research starts on driving a native Exploration camera
for the remote character on the client; door/cast views stay as overrides.

### 2026-10-14 — H1c client native camera: research and candidate

CONFIRMED_LIVE (client inspection): the client's "default" camera already
targets Tal (slot 1 GameObjectTarget = PC_TAL, slot 0 OffsetTarget) but sits in
`Camera::CameraInterpolationState` (vt 0x2DB9A4 / data 0x2DB998, kind 0x13)
while the host's is in Exploration (state vt 0x2D9854 / `ExplorationStateData`
0x2CA694); the speed global (0x408DA0) is paused (+0x28 = 1, count +0x2A = 1),
so the scheduled camera update (node vtable slot 1, RVA 0xE7660) never runs —
it returns at once while `speed+0x28` is set. CONFIRMED_STATIC: that update
takes a timing block (dt at +0xC), runs the state's gate (slot 0x20) and update
(slot 0xC → Exploration solver RVA 0x47C190 → 0x47B630 / slots 0x50, 0x80),
advances fades and the camera velocity; `CCamera::InputEvent` (RVA 0xE85F0)
accepts actions 0x69/0x6A with owner 0 when camera+0x107 is set and the UI
flag (0x408D8C → +0x56) is clear; `CCameraManager::SetCameraState` (RVA
0x37530, name, "Exploration", 0.0, false, 99999.9) selects Exploration as the
retail bootstrap at RVA 0x4CF831 does, after `CanSetCameraState` (RVA 0x37C30).
Candidate: new `lan_story_client_camera.[ch]` — per client presentation with
no cast/door override: request Exploration when the state class differs
(rate limited), feed the focused right stick as native 0x69/0x6A events,
clear the speed pause byte around one call of the camera's own update with
the real frame dt (≤ 0.1 s), restore it, then
`SudekiMpLanStoryLocalControlAdoptNative` copies the resulting render matrix
and projection into the local view bookkeeping (no camera write). Any refusal
falls back to the mathematical view. Default on; `[StoryAreas]
ClientMathCamera=true` keeps the old view. Exact-image guards on all five
entries. CONFIRMED_TEST: StoryClientCameraImageTest (every guarded byte
mutation refused; malformed/unnamed leases never tick). DLL build passes.
Candidate `fea89fb8…`. **Owner gate OPEN:** Tal's camera should now behave
like Ailish's — pulls in at walls, native return steering and spring, right
stick orbit — with the door/cast views still appearing on top.

### 2026-10-14 — H1c first live result: "that's not right" — two defects found

Candidate `fea89fb8…`/`ec74ee32…` live: the client camera did reach Exploration
(`state_vt=2d9854`, ticking) but the owner rejected the result. Diagnosis from
live reads and logs:
1. **Door view never released on the client.** `SudekiMpStoryCastViewPush`
   rejected every later kind-0 ("normal") sample as a reorder because its cast
   sequence (0) was lower than the door view's (0x8001); the client kept the
   gate camera while Tal walked 12 m away. Fix: the reorder rule applies within
   one cast family only (`s->kind==last->kind`).
2. **Native camera aimed at a stale point.** Under the pause nothing refreshes
   the camera targets: `Camera::GameObjectTarget` caches the actor's live
   CPosition in its slot-1 refresh (RVA 0x19A290) and `Camera::OffsetTarget`
   recomposes its matrix from the wrapped target (RVA 0x19A0D0); on the client
   both still held New Brightwater coordinates. The manager's gated per-frame
   method (vtable slot 3, RVA 0x36BD0 → 0x1196C0/0x134D90) is the native
   driver (decompile pending). Fix: the tick refreshes the camera's two target
   slots, inner then outer, through their own vtable slot 1 (exact classes),
   before the camera update.
3. Gating: the host publishes kind-0 cast views continuously; the native branch
   treated any cast sample as an override and never ran after the first one.
   Fix: only a *valid* cast view or an active local cast overrides.
Rebuilt; StoryClientCameraImageTest, StoryCastViewTest, LanPartySessionTest pass.

### 2026-10-14 — H1c second live result: walls yes, doors no

Candidate `73521b51…`: owner confirms the native camera now respects walls
(world geometry) but not doors — "I can move the camera past the door to see
the area we just left". CONFIRMED_LIVE (read-only): every door's collision
object (CPosition+0x8C) carries an activation byte at +0xF4 that is 1 on the
host and 0 on the client for all three doors; the same holds for most scenery
colliders (host 10 of 22 active, client 1 of 22); NPC colliders are 0 on both.
The host object also carries built per-face bytes at +0x30..+0x40 that the
client's lacks, so activation is a registration step, not a flag: it is
scheduled entity work that the client's native pause never ran. The CDoor
state machine toggles the same byte (0 at state 0xC open, 1 at 0xE close), so
door openness and camera solidity are the same mechanism. Thirteen writers of
+0xF4 found by byte scan; the cluster at RVA 0x1105xx–0x110Dxx (collision
system, next to the position publication 0x110BD0) is the likely registration
path (decompile in progress). Direction: activate the client copies' colliders
through the native registration once per catalogued door/scenery (exact
seams), or tick those entity nodes once under a lifted pause, then mirror the
host's open/closed byte with the door pose.

### 2026-10-14 — H1d door/scenery collision on the client (candidate)

CONFIRMED_STATIC: retail RVA 0x110D20 (`fastcall`, CPosition) copies
CPosition+0x100 (collision enabled) into the collision object's activation
byte (+0x8C → +0xF4); RVA 0x110D00 clears it; the CDoor state machine writes
both at open (0xC) and close (0xE). Candidate: the world frame's door record
carries the host's CPosition+0x100 as `hit_points` 0/1 (validator: doors 0 or
1); the client apply mirrors that byte on its door copy and then runs 0x110D20
for every catalogued door and scenery target (exact entry bytes guarded at
world init), so the client's camera collision sees the same solids as the
host's. CONFIRMED_TEST: LanPartySessionTest (door 0/1 accepted, 2 refused),
StoryMonsterDamageFlushImageTest, StoryShotsTest pass; DLL build passes.
Candidate `76e128a9…`. **Owner gate:** Tal's camera stops at a closed door
(gate) like Ailish's, and passes while the door is open.

### 2026-10-14 — H1c/H1d accepted: client native camera with doors

Owner on candidate `c0ef40a3…`: "that's better!" then, after the hand-back fix,
"the other side of the door moment is gone". CONFIRMED_LIVE (owner) for the
tested route: Tal's camera is the native Exploration camera (walls, corners,
orbit), door colliders follow the host's open/closed byte and stay open while
Tal is still in the doorway, the door view (kind 3) layers on top and, on
hand-back, the client re-enters Exploration via `CanSetCameraState` /
`SetCameraState("default","Exploration",0,false,99999.9)` so the camera is
re-seated behind Tal instead of recovering from behind the door. Mathematical
view remains the fallback (`[StoryAreas] ClientMathCamera=true`). Open
follow-ups: the manager's own target-refresh driver (RVA 0x36BD0 →
0x1196C0/0x134D90) as the canonical path instead of per-target slot-1 calls;
scenery colliders activated only for catalogued scenery; other routes/doors.

### 2026-10-14 — H2 chests (#66): first trace

CONFIRMED_LIVE (owner opened `ICa_Chest_General1d` with Ailish, Tal watching;
read-only pollers on both peers): chests are a distinct entity class (vtable
0x2D5200, node 0x2D5224, kind 0x0F9C) with the door-like layout — CPosition at
+0x150 (→+0x44), CStats +0x260, animation component vt 0x2C8504 at +0x3A4
(→+0x58), renderer 0x2DF8EC, 4 channels, 1 submodel — plus an "OpenObject"
usable component at +0x688 (vt 0x2D514C; bases 0x2D51BC/+4, 0x2D51C4/+0x18,
0x2D51E4/+0x3C) serialising "Tsa Overrides chest operation" (+0x9C), "Distance
player uses object from" (+0x98 = 1.0), "Gel script to run" (+0x78, empty on
this chest) and `CurrentState` (+0x74, "Closed"…). Opening on the host:
+0x74 0 → 4 (approach, player position copied into +0x54..+0x5C) → 5 → 8 (open,
fields cleared) over ≈4.5 s; the chest entity is paused on the host until used
and permanently paused on the client, whose copy never changed. The loot path
is not observed by the mod (no `CInventory::AddItem` RVA 0x217E0 /
`AddItemToInventory` RVA 0x22150 hook), so what the owner received is UNKNOWN;
decompiles of the entity update, the usable state machine and the Treasure
class are in progress. Plan for #66: (a) admit chest kind 0x0F9C to the world
catalog so the open animation follows the host like doors; (b) observe the
host inventory-add path for the D6 ledger; (c) remote open = Phase H dispatch.

### 2026-10-14 — H2 chests: reward path, item names, candidate

CONFIRMED_STATIC: the chest usable component's state machine is RVA 0x1849D0
(base at +0x18, state = +0x5C there = component +0x74): 1→3→4 plays the open
animation (anim id 0x52 into +0x48) and walks the player in; 2/7 run the
optional GEL script (+0x60/+0x78) and the reward helper RVA 0x185090: if the
component's item id (+0xB8) is not −1, `CInventory::CanAddItem` then
`CInventory::AddItem(item, 1, 1)` (RVA 0x217E0, thiscall on the party
inventory 0x408D84) → state 8 (open). Item display names come from the item
table 0x408D80 (def +0x50 = localisation token) through the localisation
manager 0x409E0C (the blacksmith adapter's path). CONFIRMED_LIVE: the chest
the owner opened (`ICa_Chest_General1d`) held item 320 = "100 Florins"; the
others in range: 321 "150 Florins", 318 "50 Florins", 243 "Orb of Vigor".
Candidate `770bff81…`: (a) chest kind 0x0F9C joins the world catalog (pose
follows the host; validator plain pose); (b) host inventory observer
`lan_story_inventory.[ch]`: relative-call hooks on the six exact
`CInventory::AddItem` call sites (three AddItemToInventory exports, script/shop
RVA 0x89DA4, pickup RVA 0x1338DB, chest RVA 0x1850BD) → thiscall stubs that log
and tail-call the original; observe-only, host only. LanPartySessionTest passes
(chest kind, no flags/hit points). **Owner gate:** after reload the save's
chest is closed again — open it with Ailish while Tal watches: the lid should
open on Tal's screen and the host log should show `story_inventory
event=add_item site=chest item=320 quantity=1`.

### 2026-10-14 — H2 chest lid accepted; glow missing; loot observed

CONFIRMED_LIVE (owner, candidate `770bff81…`): the chest lid opened on Tal's
screen ("it did in fact open on Tal's side"); the host observer logged
`story_inventory event=add_item site=chest item=320 quantity=1 show=1`
(100 Florins). Missing on the client: the chest's open glow — the host spawned
a `SpecialEffect` (identifier cf2b3c97) at the chest position; one-off world
effects are not carried to the client (only spawn sigils and cast visuals are).
Next: find the chest's effect spawn path and resource, then carry such host
world effects (asset, position, lifetime) through the presentation channel.

### 2026-10-14 — H2 chest/door effects: resources identified, whitelisted

CONFIRMED_LIVE (host registry poll on the previous candidate, one chest
opening): opening a chest spawns two `SpecialEffect`s (vt 0x2D3C7C) with no
retained name text — `SFXA205_OPEN_CHEST` (resource id 0x62d4dab3, placed at
the chest, ~4 s) and `SFXE007_CHEST_OPEN` (0x81bd29e3, attached, render wrapper
present). Opening a door spawned `SFXA203_OPEN_DOORS` (0x7148157b, ~4 s). The
closed-chest shimmer sitting on each unopened chest is `SFXAR006_10_REVEALED`
(0x3026f213). None of them carries the effect skip flag (`+0x3E0` bit 3), so
the only reason the client never saw them is the closed resource whitelist
(`SUDEKIMP_STORY_WORLD_RESOURCES`): `lan_story_world_visuals.h` dropped them
with kind 0. The identifiers were resolved by hashing every resource name found
in the game data with the retained-text hash (`SudekiMpSpiritVisualKindForTyped
Resource`: upper-case `NAME.hom`, alternate add/multiply) — the earlier
"identifier cf2b3c97" note was a misread. Candidate `deea724c…` adds the three
open effects to the whitelist (source mask 1; 110 world kinds, last kind 239);
StorySkillVfxCodecTest, StoryCastViewTest, StoryClientCameraImageTest and
LanPartySessionTest pass. **Owner gate:** open the chest again (reload reset
it) — Tal should now see the open glow; a door opening should show its burst on
his side as well. Not yet carried: the closed-chest shimmer (looping; would
need the loop playback path) — add only if the owner wants it.

### 2026-10-14 — H2 chest glow accepted

CONFIRMED_LIVE (owner, candidate `deea724c…`): "Tal saw the glow this time."
Host log: `story_world_visual event=observed source=1 kind=238` (SFXE007) and
`kind=237` (SFXA205) alongside `story_inventory event=add_item site=chest
item=320`. #66 part 1 (host-opened chest shown to clients) is complete; part 2
(remote player opens, loot credited to the opener) is Phase H / D6.

### 2026-10-14 — Owner observations after a treasure pickup; inventory decision (D9)

CONFIRMED_LIVE (owner, candidate `deea724c…` / `19931be`): Ailish opened a
treasure object holding **Hukin's Fate** (item 3; host observer
`add_item site=chest item=3`). (1) Tal's player never saw the item: the
client's own `CInventory` is never updated — only the host's game owns the real
pickup. (2) The client cannot open the M game menu (held closed by design: it
would operate on the client's stale local save). (3) When the host equipped
Hukin's Fate on Tal from the M menu, **Tal's client froze until the old sword
was re-equipped** — HYPOTHESIS: the host's frames reference the new weapon's
model/animation bank, which the client's Tal (still holding the old sword) does
not have, so frames are refused until equipment matches; no equipment state is
replicated for party members today. Owner decision recorded as D9 (per-game
vanilla inventory; loot copies to everyone; shop purchases personal; equipment
replicates as state). Order agreed: equipment replication first (freeze is
host-triggerable at any time), then H1 chest use, then the inventory loot copy
and the client menu. Live chain for H1 captured the same session: the chest use
entry (RVA 0x184EC0) is invoked from RVA 0xB5D9E under the script VM, i.e. the
`OnAction` script calls a native export; `0x295B0(controller,0)` follows from
RVA 0x184F83.

### 2026-10-14 — #74 equipment freeze: cause confirmed, grant candidate

CONFIRMED_LIVE + CONFIRMED_STATIC: the party frame already carries each
character's equipped weapon (`weapon_item_plus_one`, host `lan_story_snapshot.c`
`equipped_item`), and the client's `SudekiMpLanStoryReplicaPrepareEquipment`
(`lan_story_replica.c`) equips it natively from the client's **own** weapon
rows. Hukin's Fate was never in the client's inventory, so the lookup set
`ERROR_NOT_FOUND` — the client logged exactly `lan_story event=present_refused
step=prepare error=1168` (×12) and refused every frame until the host re-equipped
a sword the client owned. Candidate `aa5389e0…` (D5 "equipment goes to its
owner"): new engine helper `SudekiMpGrantCharacterWeaponItem` (same exact
`CInventory::AddItem` image proof, item-table identity, weapon-family and
pending-swap guards as the test-room grant; `show=1` for the native notice);
the replica adapter grants the missing item once per host epoch/generation,
re-describes the rows and continues with the ordinary equip. StoryWeaponPose,
StoryVfxDispatch, StoryCastView, StoryClientCameraImage and LanPartySession
pass. **Owner gate:** Ailish opens the treasure object and equips Hukin's Fate on
Tal from the host menu — no freeze; client log `equipment_granted … granted=1`
and `weapon_grant item=3`; Tal's model changes on both screens. Not covered:
armour (no armour state in the frame yet; #40), unequip (no validated contract).

### 2026-10-14 — Farmhouse entry: two host paths; one freezes the client

CONFIRMED_LIVE (two owner round trips, candidates `aa5389e0…` then `b3c2755a…`):
(a) **Split path** — host logs `host_control_blocked reason=split_transition_hold`,
Tal's body stays outside, control returns at once (client `native_binding_
acknowledged` → `host_confirmed_movement`); only the brief hold is visible
("the same minor freeze he has always had … acceptable for now").
(b) **Take-along path** — host logs `zone_begin method=5 … temporary=
lico_frappe_farm`, then `host_control_drain reason=roster_unknown` and
`load_drained seat=2`; Tal's host body is popped inside beside Ailish (live
read: both at −166,4.4,−70, AI override 0, AI mode byte 1, arbiter idle); after
the host leaves (`zone_begin … temporary=` empty) the offer chain reaches
"offer_attempt" and then refuses silently every frame, so the client stays in
`waiting_for_host_offer` and goes spectator: the "froze when I went inside"
report. Which path runs depends on where Tal stands when Ailish enters
(HYPOTHESIS: inside the door trigger → native take-along). Candidate
`b3c2755a…` adds rate-limited names for the formerly silent refusals
(`offer_begin_refused`, `offer_lease_refused`, `offer_acquire_*`;
`story_native_control event=acquire_refused reason=generation_stale|
slot_not_empty|body_not_idle|identity_drift|ai_override|ai_mode`). Next: owner
reproduces (b) with Tal beside Ailish at the door; fix follows the named reason.

### 2026-10-14 — #74 equipment grant accepted

CONFIRMED_LIVE (owner, candidate `b3c2755a…`): Ailish equipped Hukin's Fate on
Tal from the host menu; "he didn't freeze". Client log: `weapon_grant item=3
category=4 policy=equipment_follows_character` → `lan_story_replica
event=equipment_granted character=2 … item=3 granted=1` → `equipment_requested
… status=0` (started). Weapons only; armour has no frame state yet (#40).
Owner direction for the menu (#75): every player gets the M menu for **their
own character only** — the host must no longer switch to another human's
character (F-keys) and equip for them; AI companions remain the host's.

### 2026-10-14 — #75 M-menu scope: native seams and first candidate

CONFIRMED_LIVE (gdb watch/traps on the host while the owner used the menu):
the Main Menu object is `*(0x3C2F78)` (vtable RVA 0x2CAE0C), its selected
party slot is `ui+0x170` indexing `group+0x90+slot*0xC`; the F-key character
switch is one routine, RVA 0x96300 (EAX = ui, pushed direction, RET 4: ±1
modulo `group+0xCC`, then preview/page rebuild), called from the menu input
handler at RVA 0x93B24 (next) and 0x93B47 (previous); the menu opens through
vtable slot RVA 0x2CAE4C → RVA 0x956D0 (sets slot 0, "OnMainMenuActivate",
`0x40AFD0(1)` pause) requested by the gameplay HUD state machine
(`*(0x3C2F88)`, requested state 0xB, RVA 0x9DD80 case 0xB); the Weapons layer
confirms an equip through one call site, RVA 0x16F1A4 → 0x1706D0 (layer on the
stack, RET 4; rows at widgets+0x88C, selected +0x858, item id at row+0x84).
The mod's armour page (#40) did call the native armour equip for Tal three
times; its visible effect on a non-front character is UNKNOWN.
Candidate `bfdcee75…`: `lan_story_main_menu.[ch]` wraps both
cycle calls so a player only lands on slots they own (host: own character and
AI companions, never a connected client's character; client: own only — pure
policy in `lan_story_menu_policy.[ch]`, tested), moves the selection to the
player's slot after open, and on the client turns the equip call into a host
request (`SUDEKIMP_STORY_ACTION_EQUIP_WEAPON`, slot = weapon item id of the
fence character's family; host `SudekiMpLanStoryControlEquipWeapon` grants the
item if missing then equips natively). StoryMainMenuImageTest (exact guards),
StoryMenuPolicyTest, StoryActionTransportTest, LanPartySessionTest and
StoryClientCameraImageTest pass. Not yet: opening the M menu on the client
(the native key path is still being traced), armour, skills pages.
**Owner gate (host side):** open M as Ailish — F-keys must no longer reach Tal;
equipping Ailish's own weapons works as before.

### 2026-10-14 — #75 client M menu: open works; presentation stops in menu render mode

CONFIRMED_LIVE (client candidates `e3d6f7a5…` → `1a388156…`): the client's M
press (native action 0x18, sampled like Q's 0x19) delivers the gameplay HUD
command 0xF and the native Main Menu opens on Tal's own page; equip presses on
the Weapons page reach the intercept (native "equip" fires on every row
selection, not only on confirm). What then breaks: the client's render-world
mode (`*(0x408D1C)+0x6C`) goes 0→1 for the menu, the primary world-render
branch (CALL 0x28D45B → 0x1D48C0, the only seam the client presents from) is
skipped, no frame is presented, and after 250 ms the control layer reports
`pending_presentation_stale`, clears input and the host offer decays
("waiting for host sync"; equip requests refused; the menu could not be closed
because the close key travels through the swallowed gameplay listener).
Ruled out along the way (CONFIRMED_LIVE, poller): the menu's entity-pause pass
(RVA 0xFD610) and disable-move/listener pause (RVA 0xAFD0) — both now skipped
on the client — and the speed-pause count (a count of 1 is the client's normal
owned pause). Candidate `1a388156…` adds the M toggle (menu input handler
kind 5 / command 1 = back). Next: present while the render mode is 1 (candidate
seams: force the primary branch for the client, or drive presentation from the
effects seam which keeps running), so control, input and the equip request
journal stay alive with the menu open.

### 2026-10-14 — #75 client menu hold candidate

CONFIRMED_STATIC (frame renderer RVA 0x28D3F0): the world is rendered only when
`*(0x408D1C)+0x6C == 0`; any full-screen UI mode draws UI only, so no mod seam
in the world render runs while the Main Menu is up and "the world behind the
menu" has no vanilla visual. Candidate `514e0b77…`: while the client's Main Menu
is shown, `service_client_control` no longer treats the (necessarily) stale
presentation as pending and `queue_client_action` accepts requests on the last
presented frame — control lease, muted input and the equip request journal stay
alive; M toggles close (menu input kind 5/command 1). On close the newest host
frame applies at once. **Owner gate:** open M on Tal, wait ~10 s, equip another
Tal weapon, close with M, move — no sync wait; host `story_main_menu
event=host_equip`; model switch on both screens.

### 2026-10-15 — #75 client menu: hold, UI-task adoption, request window

CONFIRMED_LIVE (client candidates `2c5a1d5e…` → `36b7f38f…`, owner round trips):
with the native Main Menu shown the client's world-render branch is skipped,
so (1) the control service must hold the lease through an invalid *and* stale
presentation sample and an unready local view, and the dispatch must not call
`invalidate_presentation()` for the missing render attempt; (2) the menu's open
("OnMainMenuActivate", preview model) advances the native task-created counter
by one, which the containment read as `pause_invariant_failed scheduler=0
tasks=52->53` → UNKNOWN; `SudekiMpLanStoryClientAllowUiTasks` now lets Service
adopt a small counter advance while the menu is shown (+3 s) when GEL time,
trigger queue, registry and roster are unchanged (`ui_tasks_adopted`); (3) the
M key is a gameplay-listener action the client swallows, so the close is the
mod's own: a focus-gated raw press delivers the active UI's "back" (menu input
handler kind 5 / command 1: sub-layer → page list, page list → leave, state 6);
the earlier admitted-only sampling lost presses once the lease decayed; (4) the
client's equip requests arrived at the host but were answered EXPIRED because
the acknowledged frame cannot advance in the menu — `story_action_current`
exempts `EQUIP_WEAPON` from the eight-frame window (fence currency kept).
Repeated open/close cycles accepted in the log of `991a3c4b…`; equip through
the client menu is the open gate on `36b7f38f…`. Known: F-keys do not reach the
client menu (same swallowed listener; harmless with one owned slot); the host
logs `menu_pause=native` (unchanged behaviour).

### 2026-10-15 — #75: menu key device ownership, equip currency, items decision

CONFIRMED_LIVE (client `36b7f38f…`): after menu actions on the client (equip
row, F3 info) the native UI re-owns the controller listener's device, so the
mod's input hook — which samples only events from the owned device — stopped
seeing the M key (one `key` line per session) and the menu could not be left;
`77ba8b80…` samples the main-menu action for any device. Equip requests reached
the host but were answered EXPIRED because `story_action_current` also demands
a fresh client scene observation, which a client in its menu cannot publish;
EQUIP now needs only the control fence (same fence, control READY, no frame
ahead of the host). Owner reports item use from the client menu does not heal:
it runs on the paused replica only — recorded as decision D10 (client item use
= host request, self only). Gates open: F3 → M → M → M leaves the menu; equip
from the client menu changes Tal's weapon on both screens (`host_equip`).

### 2026-10-15 — #75 client weapon equip through the host: working

CONFIRMED_LIVE (host/client logs + live memory, candidate `3fbee5dc…`): a weapon
confirmed on Tal's own Weapons page becomes `EQUIP_WEAPON` (item id), reaches
the host (client-side frame expiry exempted for this kind; host currency = control
fence + READY only), and the host equips it with the **Main Menu's own equip
core** — tracked handle from the actor (RVA 0x2000: `8B 09 89 08 C7 40 04 …`),
item definition in ESI, context 0x34BE70 in EDI, generic apply-item-to-character
RVA 0x12DF60 (consumes the handle) — `weapon_equip item=0 applied=1`; the frame
then carries the weapon and the client replica equips it (`equipment_requested
item=0 status=0`); live, host and client Tal both hold item 0. The combat
quick-activation path tried first (`SudekiMpActivateCharacterWeapon`) reported
STARTED but left an idle AI-driven Tal on his old weapon. The three native
routines (item lookup, `0x12E1E0` can-use, `0x12DF60` apply, then
`CInventory::ReduceItem`) are also the Items page's use path, so USE_ITEM (D10,
self only) reuses them. Pending owner visual confirmation of the swap.

### 2026-10-15 — #75 client weapon equip accepted

CONFIRMED_LIVE (owner, candidate `3fbee5dc…`): equipping the training sword from
Tal's own menu changed Tal's weapon on both games ("the mini-Tal in the main
menu had not changed … when I switch a weapon I can see that swap on Ailish's
screen"). Known cosmetic gap: the client menu's 3D preview is rebuilt only by the
native equip (which the client now skips) — rebuild it after the replica equips
while the menu is shown. Next: USE_ITEM (D10, self only) on the same native
apply path; the client Items page intercept at the Items-page use handler
(RVA 0x179A50) call site.

### 2026-10-15 — #75 / D10 item use candidate

CONFIRMED_STATIC (Items page use handler RVA 0x179A50 and its callers): the
Items layer confirms through one call site (RVA 0x17761D, layer on the stack,
RET 4); the handler maps the selected row (layer+0x124 index, rows at +0x158,
row+0x84 = category-2 row index) to the item definition through the inventory's
category lookup (RVA 0x21CE0, thiscall(inventory, 2, row)), builds the target
handle, runs can-use (RVA 0x12E1E0, 0 = usable), apply (RVA 0x12DF60, context
0x34BE70) and `CInventory::ReduceItem(id,1)`. Consumables are CItemConsumable
(vtable RVA 0x2D3B20, category word +0x88 = 2). Candidate `be181080…`:
`USE_ITEM` action kind (item id = slot | extra<<8; the extra byte is zero for
every other kind), client Items-page intercept → request for the player's own
character (no target), host `SudekiMpUseItemOnCharacter` = the same three
native calls with exact-image checks on all four entries and the Items page's
three call sites, client reduces its own inventory by one on STARTED. Transport,
session, menu policy and image suites pass. **Owner gate:** heal Tal from his
own Items page — HP rises on both screens, count −1 on both. Not covered: a
consumable the host does not own (purchase case, D9) — logged as stage 3/… and
refused; quantity check deliberately omitted for this cut.

### 2026-10-15 — #75 / D10 item use: host crash, cause and fix

CONFIRMED_LIVE (two runs, candidate `be181080…`): the first client item use
(`use_requested a=204 b=1`, Tal healing himself from his own Items page)
killed the **host**, not the client. Wine's backtrace: page fault at the
handle-make entry (RVA 0x2000, `mov ecx,[ecx]`) with ECX = EDX = 0, EBX =
the item definition. The client then lost the host and left to the title
through its normal native-exit path (`story_native_exit` → `story_session_ended`),
which is what looked like a client crash.

Cause (CONFIRMED_STATIC in the compiled DLL): `SudekiMpUseItemOnCharacter`
runs two inline-asm blocks, can-use then apply, and both named
`&actor_pointer` as an EDX *input*. GCC therefore assumed EDX survived the
first block, but the native can-use call (MS ABI, 0x12E1E0) clobbers EDX and
ECX, so the apply block passed a null address to handle-make. The one-block
equip path never had the problem. Fix: both blocks take the item and the
actor address as in-out operands (`"+b"`, `"+d"`) and reload them between the
blocks; the rebuilt code reloads EBX/EDX from the stack before the apply call.
Both native calls return with `ret 0xC`, so the stack contract was already
right. Candidate `715c6800…` staged on both games. **Owner gate unchanged:**
heal Tal from his own Items page — HP rises on both screens, count −1 on both;
expect host `item_use item=… applied=1` and `host_use … outcome=1`, client
`use_applied`.

### 2026-10-15 — #75 / D10 item use accepted; client confirm prompt candidate

CONFIRMED_LIVE (owner, candidate `715c6800…`): Tal healed himself from his
own Items page. Host `item_use item=204 applied=1` / `host_use … outcome=1`,
client `use_applied item=204 local_reduced=1`; HP rose on both screens.

Owner rule (2026-10-15): a client's Items page must not show the party-target
view at all (D10: self only). Instead the list confirm opens a second
"are you sure" prompt.

CONFIRMED_STATIC (Items layer input handler RVA 0x1770B0, vtable 0x2D8BBC
slot +0x2C; mode word layer+0x8C): list mode 0, confirm commands 0/2 → when a
consumable is highlighted (layer+0x200 = definition, set by the list refresh
RVA 0x178390 through the category lookup) the handler plays the confirm sound
and calls the mode switch (RVA 0x177850, ESI = layer, EDI = new mode = 5)
from RVA 0x177166; mode 5 is the party-target view whose confirm is the use
handler (RVA 0x179A50, already intercepted) and whose commands 6–9 move the
portrait cursor (`*(ui+0x218)+0x30C4`). The native pause-menu yes/no
(`SUI_Pause_YesNo`, "Are you sure you want to quit?") is private to the
pause layer (owners RVA 0x1DC70 / 0x1C920), not a reusable dialog, so the
prompt is mod-drawn.

Candidate `07d9ca2d…`: new seam in the Main Menu adapter — the list-confirm
call at RVA 0x177166 (guards: `mov edi,5` at 0x177104, the sound call
context with base-relative operands, `mov esi,ebx`, the `mov al,1` epilogue)
is redirected on every seat; on a client it records the highlighted
consumable and the player's own slot as a prompt and skips the mode switch,
so the Items list stays where it is. The runtime draws the prompt with the
title-view text renderer ("Use this item on yourself?", Yes/No, hint line).
Keys while the prompt is shown: Enter/Space decide, Left/Right/Tab switch,
Esc/M cancel; a native confirm arriving while the prompt is shown decides
it too, and a 300 ms suppress window stops the same key from re-opening it.
The prompt drops itself when the Items layer stops being the active UI or the
highlighted row changes. Host unchanged (native party-target view). Image
suite (12 seams) and menu policy test pass. **Owner gate:** on Tal, Items →
confirm a potion: no party view, a yes/no prompt appears over the menu; Yes
heals as before (both screens, count −1), No/Esc leaves the list untouched.
Unknown until seen: whether the overlay draws while the native menu owns the
frame (first draw result is logged as `confirm_render`).

### 2026-10-15 — confirm prompt accepted as working; framed dialog; item-use feedback (#76)

CONFIRMED_LIVE (owner, candidate `07d9ca2d…`): the client Items confirm opened
the mod prompt instead of the party view, and Yes healed as before. Owner
feedback: the prompt was bare text and the world name tags ("Host"/"Guest")
showed through the menu. Candidate `3d106933…`: the overlay renderer gained
framed boxes (dialog panel + rest/focused buttons, diffuse-only quads like the
letterbox bars) and the prompt is a centred dialog with two lit buttons and a
hint line; name tags are not drawn while the client's native menu is shown.
Owner gate: dialog looks like a window over the menu, lit button follows
Left/Right, no stray tags.

Owner question: Tal shows no "use" moment on his own screen when he uses an
item, while Ailish does on the host. CONFIRMED_STATIC: the consumable heal
method (RVA 0x12E7B0, CItemConsumable vtable +0x20) attaches the item's
visual effect to each target through the attachment factory RVA 0xBCE90
(resource from the definition +0xE0) and fires the script callback
`PCItemUseCallback` plus a timed script job; none of that reaches the joiner's
replica. Filed as #76 with a host-authoritative proposal (client attaches the
same effect locally on STARTED; clip replication if the callback animates).

### 2026-10-15 — client-side presentation of an accepted item use (#76 candidate)

Owner (framed dialog accepted visually): the Items list quantity only updated
after leaving and re-entering the menu, and Tal should "fire the same
ItemUseCallback" as the host's character.

Candidate `ed03a5b9…` (D10 unchanged: the host still decides): when the
client's USE_ITEM result is STARTED, the client now runs the native apply on
its own replica with the can-use check skipped (`SudekiMpUseItemOnCharacter(…,
validated=TRUE)`): the CItemConsumable method attaches the item effect
(0xBCE90), fires `PCItemUseCallback`, and `CInventory::ReduceItem` lowers the
local count — the separate local reduce is now the fallback only. The stat
change it makes locally is overwritten by the replicated party state. Then,
if the Items page is the active UI, the page is rebuilt the way a category
switch does (RVA 0x178820, stdcall-like RET 4, exact entry bytes) and the
party panel is refreshed (RVA 0x15B570, EAX = its current cursor, RET 4).
Logs: client `item_use item=… applied=1 validated=1`, `use_applied …
local_apply=1 refreshed=1`, `story_main_menu event=items_refreshed`.
**Owner gate:** Tal uses a potion from his own menu: quantity drops at once
in the list, Tal's own screen shows the item effect/use moment the host's
character gets, no containment complaint (`pause_invariant`/`ui_tasks`) and
no crash. Unknown: whether the callback runs cleanly in the contained client
script VM — watch the client log.

### 2026-10-15 — item-use animation A/B, part 1 (host, outside the menu)

Test aid added: `[StoryAreas] TestStartLeaderHealth=<fraction>` lowers the
host leader's HP after the test-start placement (cleanroom resource setter;
logged `story_test_start event=leader_health hp=300->150 ok=1`).

CONFIRMED_LIVE (owner, host as Ailish, Healing Ointment used **outside** the
M menu, i.e. the in-game quick item path): the host logged a one-shot base
clip on character 3 — `base_clip_change selector=4->4 state=1->65 time=260
rate=24` followed by `selector=4->1 state=65->0` — and the client mirrored
the heal (`party_resources character=3 hp=150->300`). That one-shot is the
use animation. Part 2 (same use from the M menu, then on Tal) follows after a
reload at half health.

### 2026-10-15 — item-use animation A/B, part 2: what the client misses

CONFIRMED_LIVE (host as Ailish, Healing Ointment from the M menu and outside
it; host/client wire diagnostics `pc_base_wire` / `pc_base_apply`, candidate
`5d864d36…`): the use moment on the host is (a) a short base-channel
one-shot (Ailish: selector 4 for ~0.2 s, then selector 14, clip `ba938095`,
20 frames at 24/s, completion bit 0x40 then back to idle), (b) a
`SpecialEffect` created ~0.7 s later by the scheduled `PCItemUseCallback`
job, parented to the character (`SFXM005_HEAL`, resource id 0xa1707ff3,
alive ~2 s). The body clip **is** on the wire and the client **applied** it
(`pc_base_apply … selector=14 supported=1`, local row followed within one
frame), so the missing part on the joiner's screen is the heal effect: it is
not whitelisted, so the world-cosmetic capture skipped it. The client's own
local apply of an accepted use plays the clip only through the host (the
script job never runs in the contained client), so the same fix covers Tal.

Candidate `9aad8d50…`: `SFXM005_HEAL` added to the world-cosmetic whitelist
(source mask 1). Open question for the gate: the effect is parented to the
character with a zero local transform, so the capture must deliver the
composed world matrix (`RVA_WORLD_MATRIX` on the effect's position
component); if the client shows it at the origin or not at all, the
parented case needs an owner-actor attachment like the shield visuals.
**Owner gate:** Ailish uses an ointment (any path): Tal's screen shows the
heal sparkle on Ailish plus the short clip; then Tal uses one from his menu:
both screens show the sparkle on Tal.

### 2026-10-15 — remote item use: the native action path (#76 research)

CONFIRMED_STATIC / CONFIRMED_LIVE (candidates `ef59f3e5…` → `3d9…` series):
the quick item menu does not call the Items-page apply. It runs can-use,
then hands the item to the character's "use item" action: RVA 0xDC110
(arbiter, item definition, party slot on the stack, RET 0xC, AL = started)
checks arbiter+0x50 against 0x29BE6F0 / 0x400000 and +0x60 bit 0, stores
the definition at action+0x44 and a target handle (group slot) at
action+0x48 with kind 0x9A at +0x40 (action object = `*(character+0x9C)`,
RVA 0x58A390), then RVA 0x4DA5B0 raises arbiter 0x800|0x20 and sets the
+0xB4 component state to 7. The keyframe routine RVA 0x58A480 applies the
item (RVA 0x12DF60, ESI = definition, EAX = handle copy) and sets the
"applied" byte action+0x54; the end handler RVA 0x58A3C0 (per-class
"animation ended" virtuals such as RVA 0x153ED10 for kind 0x9A) applies
only if that byte is still 0, then clears 0x800|0x20.

Live on the host for the remote-controlled Tal: the action starts
(`item_action started=1`), 0x821 is raised, the keyframe routine runs at
+0.3 s (`applied=1`, kind cleared) and the action ends at +0.6 s, but
Tal's host HP (stats+0x2C) stays at 200 across every sample and no
SFXM005_HEAL entity appears. The mod's remote move path first interrupted
the action with a zero-speed submission; that is fixed (busy mask includes
0x20 and the move path returns held without any native call), yet the
keyframe apply still changes nothing. Open question: why the identical
apply call heals when the mod makes it directly but not from the action's
keyframe for this body. Interim (`host_use_target` / `item_use …
hp_after` logs added): the host starts the action for the animation and
applies the item directly once (no second count reduction).

Client quick menu: the items page is category 5 (CONFIRMED_LIVE); a row
confirm there now becomes a USE_ITEM request for the player's own
character (`story_quick_menu event=closed reason=item_queued`).

### 2026-10-15 — remote item use accepted (M and Q menus)

CONFIRMED_LIVE (owner, candidate `3c…` = commit below): Tal uses a Healing
Ointment from his own M menu and from the Q menu; both screens show the use
animation, the heal and the SFXM005_HEAL sparkle. Root cause of the long
"animation but no heal": the native quick menu runs can-use (RVA 0x12E1E0)
immediately before starting the action, and the apply that follows (keyframe
RVA 0x58A480 or direct) only takes effect after that call; the host now runs
can-use with the same handle contract before RVA 0xDC110. Interim direct
apply removed; the Items-page apply stays as the fallback when the action
cannot start. Also fixed on the way: the remote move path no longer submits
anything while arbiter 0x20 is set (the zero-speed submission interrupted the
action), and the client quick menu's item page is category 5.

### 2026-10-15 — D6 per-game loot candidate (owner gate OPEN)

Revision: `4370296` plus the D6 working-tree changes; saved-story profile **26**.
D9 governs this step: every game keeps its own native party inventory and purse.
The older four-character `personal_wallet`/entitlement design is not activated.
Native merchant listings do not deplete: catalog `+4` is price, not stock;
BUY RVA `0x89D70` adds at `0x89DA4` then debits the local purse. The brief's
`0x489DF0` is the **VA of SELL**, not a purchase RVA. Client shop access is H.

CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE: the existing inventory adapter remains
the only owner of six `CInventory::AddItem` callsites. Pickup `0x1338DB` and
chest `0x1850BD` pass `(inventory,item,1,1)` after native capacity checks; the
three exports and shop `0x89DA4` are excluded. Ground and enemy drops use the
same pickup source. Kind 39 money follows AddItem → AddMoney `0x21C50` with
native denomination and 99,999 cap. Native item category 40 is also admitted.
The client validates inventory, item-manager, definition and category ownership
before a single native AddItem; native popups and caps remain native.

The world-drop collector's literal byte check overlapped the inventory
observer. It now recognizes the original call or this adapter's exact installed
replacement, with all surrounding bytes retained. The image regression proves
both accepted states and rejects a foreign redirect. Transactional install,
reverse rollback, failed restoration retention and retry, and game-thread-only
binding/retirement are tested by name.

CONFIRMED_TEST: independent grant/ACK messages carry immutable source, serial,
predecessor, item and quantity. A bounded per-peer outbox retries its head until
the client's native outcome is committed; lost/repeated messages cannot repeat
AddItem. Native preparation precedes claiming the serial. Scene/control changes
do not discard grants. Recipient leases are checked atomically; host capture
retains a plain-data FIFO during backpressure. Unknown native outcomes, changed
recipients or disconnect with pending grants explicitly quarantine delivery.
This does **not** establish crash/restart/reconnect recovery; that belongs to I.
Mixed profile 25/26 peers are refused at handshake.

D9 also exposed a double debit: remote item use reduced both host and client
inventories. The remote host action/fallback now preserves the host stack; the
requesting client's accepted-result path still spends one local item. This
ownership change needs the live count check below. Host native menus retain
normal native spending.

Validation performed on the dirty candidate, supported image SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`:

- MinGW `SudekiMP` build passes (existing native-ABI pedantic warnings).
- GE-Proton Wine: `SudekiMP.StoryInventoryImageTest`,
  `SudekiMP.StoryWorldVisualImageTest`, `SudekiMP.StoryMainMenuImageTest`,
  `SudekiMP.StoryLootGrantTransportTest`, `SudekiMP.StoryActionTransportTest`,
  `SudekiMP.LanPartySessionTest` all exit 0. Run the three `*ImageTest.exe`
  targets with the supported `SUDEKI.exe` path as their sole argument; run
  the other three without arguments. Exact command records are retained in
  the private D6 test artifact. Named native checks actually ran; compilation
  alone is not counted as an image test.
- Inventory fixture executes retail AddItem for consumables, category 40 and
  florins, checks caps, six-source inclusion/exclusion, unknown outcomes,
  single-use leases and foreign-thread refusal. Loopback covers delivery to
  multiple clients, immutable retries, missing ACKs, queue capacity, stale
  leases and scene/disconnect fences. These are not live gameplay acceptance.
- Read-only observation of the preceding live pair agrees with the required
  inventory/database/category layout. It does not prove candidate behavior.

**Owner gate — stop here after staging the candidate.** Ailish host / Tal
client, existing New Brightwater save and castle-road approach; logs freshly
rotated. Start health remains 0.5 for both characters for heal checks.

1. On Tal, open M → Items and note ointment count. Ailish collects one ointment.
   Tal's count must rise once without closing/reopening the page; check the
   host's count too. Log: one `story_loot event=queued` and one matching
   `event=granted`, with `refreshed=1` on the active Items page.
2. Collect a chest reward. Both inventories gain its item once; waiting,
   closing/reopening M, and moving areas must not award it again.
3. Collect a florin drop. Each game's own purse gains the native denomination
   once, independently respecting its cap. Tal must still be able to collect
   a host-authored drop by approaching it.
4. Tal uses one ointment from M, then Q when hurt. His count drops once per
   accepted use; Ailish's host count stays unchanged; heal/animation/effect
   remain correct. Separately check host-zero-stock/client-positive-stock use.
5. Ailish buys an ointment: only her game's count/money changes. The inverse
   **Tal-client purchase gate is deferred to H client shops**; no live client
   checkout is claimed by source exclusion tests.

Deployment observation: candidate `db74b2d7…` is staged on both peers with
matching changed DLL hashes and fresh logs. Both startup checks pass; host
placed both characters at the castle-road approach with half health, the
inventory observer installed, and the client reached confirmed movement.
These are startup observations only. The first pickup/menu owner gate is open.
Graph findings were recorded and recalled; AST update completed with the
existing native-C parser limitations.

Pending: all five visual/gameplay checks, live teardown with pending loot,
full-roster/full-campaign coverage, two-machine/native-Windows runs and Phase I
persistence. No step-completion commit or push before owner acceptance.

### 2026-10-15 — D6 pickup counter accepted; chest gate next

CONFIRMED_LIVE, bounded to saved-story profile26, Ailish host / Tal client on
castle road, Linux/GE-Proton Wine, `4370296` plus D6 dirty candidate
`db74b2d7…`. The owner reports Tal's item counter increasing during the requested
open-M → Items pickup check. Paired logs independently show eight host pickup
grants (serials1–8), each applied once on the client, with matching item and
quantity, native count +1, completion acknowledged and `refreshed=1` every time.
Item204 specifically increases from4 to5 on both games. No duplicate grant,
cap, capture uncertainty, delivery quarantine or waiting state appears in this
sample. Evidence artifact: `d6-pickup-menu-20261015`.

The ordinary-pickup / active-Items-page gate is accepted. No chest source or
florin grant appears in this capture; their gates remain open. Next: Ailish
opens one chest; both games must receive its reward once, with no repeat after
waiting or reopening Items. Personal item-use counts, shop exclusion live,
remote collection, pending-loot teardown and persistent recovery retain their
previous limits. No code change, rebuild, reload, commit or push for this review.

### 2026-10-15 — D6 sword chest accepted; open Weapons refresh missing

CONFIRMED_LIVE on the same profile26 Ailish-host/Tal-client dirty `4370296`
candidate `db74b2d7…`: the owner reports Ailish collected the farmhouse sword,
Tal received it after reopening Weapons, and the equipment test worked.
The paired trace records one chest grant (serial9, source2, item3, quantity1),
native count0→1 in each game and one client completion, followed by the host's
successful Tal equip. All nine grants in the sample match without duplicates.
Evidence artifact: `d6-chest-weapon-20261015`. This accepts this chest reward
and equip scenario; repeat-after-area-change, florins and spending gates remain.

The open Weapons page did **not** refresh: the grant's `refreshed=0` and
`items_refresh_refused` agree with the owner's required reopen. Loot currently
calls the Items-only refresh regardless of the shown page. Extend the existing
menu adapter with a guarded native Weapons refresh; do not change weapon-family
restrictions or award the reward again. The owner defers using joined players'
display names in weapon-acquisition notices to
[#77](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/77).

### 2026-10-15 — D6 automatic Weapons refresh candidate

Revision `4370296` plus preserved D6 working-tree changes, profile26. Reward
completion now dispatches to the active Items or Weapons page. Weapons uses
the native list rebuild, restores the highlighted weapon by item ID and updates
its details. More Info and transition states defer until browsing; no native
pointer is retained for that delay. The existing armour adapter validates its
own shared hooks; its armour layer is excluded. No second hook, inventory grant
or equip is introduced by this refresh. See the research log's corresponding
entry for the native functions, layout and evidence boundaries.

Validation on the supported executable from `docs/executable.md`, GE-Proton
Wine: `SudekiMP.StoryMainMenuImageTest`,
`SudekiMP.ArmourMenuRefreshImageTest`, and `SudekiMP.StoryMainMenuRefreshTest`
all exit0; each takes the supported `SUDEKI.exe` as its sole argument. The first
two execute named original/owned native byte and ownership checks, including
foreign detours and restoration. The third executes synthetic UI bodies behind
retained native prologues: selection after row insertion, deferred refresh,
ownership changes, armour exclusion and closing the menu are covered for both
original and owned entries. This is CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST,
not live automatic-refresh acceptance.

**Owner gate after the next paired reload:** keep Tal's M → Weapons page open,
highlight an existing weapon, and collect the farmhouse sword with Ailish.
The new sword must appear once without reopening, the highlight must stay on
the same weapon, and the new sword must still equip on both screens. If testing
a reward during F3 More Info, return to the list: it must be current without
leaving Weapons. Existing Items-page refresh must remain intact. No completion
commit or push; D6's florin and independent-spending checks remain open.

Reload observation: at the owner's request, candidate `be910918…` replaced
`db74b2d7…` on both peers; the tested source manifest and both deployed hashes
match. The old pair exited, logs were rotated, both new DLLs report `status=ok`,
and the original configuration files were restored after loading the temporary
invulnerable test setup. The host reached its lobby; the client is waiting for
a real window-focus click before startup continues. Joining, placement and the
automatic Weapons refresh remain unconfirmed on this run. No gameplay input
was automated. This is a deployment observation, not owner acceptance.

### D6 repeatable farmhouse start — 2026-10-15, test aid

Owner requested a paired reload that loads New Brightwater, puts Ailish inside
the farmhouse at her previous spot, and keeps Tal at his previous exterior
spot. The optional host test-start configuration now accepts
`TestStartTemporaryZone`, `TestStartTemporaryMarker`, and
`TestStartTemporaryLeader` in addition to the existing exterior start. It waits
for an outside remote-control lease, places the exterior pair, then uses the
existing observer/split owner to request native TEMP with the authored arrival
marker. Only the leader receives the final interior position after positively
observed settlement. An unknown or timed-out transition is never retried or
force-cancelled. Normal saves and player-initiated travel are unchanged.

The DLL builds and `SudekiMP.StoryTestStartTest` passes its synthetic placement,
readiness, stale-identity, once-only entry, and follower-exclusion checks.
`SudekiMP.StoryTestTemporaryImageTest` passes eleven named checks against the
supported executable, including the five native byte guards, the owned
entry/trampoline, actual TEMP entry, resource retention, admission refusal,
separate settlement, and no retry after an uncertain return.
`SudekiMP.StoryTemporaryContextTest` still passes. The native fixture also
confirms that full-party mode at depth zero leaves follower counters untouched.
These are CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE.
This test convenience does not accept the automatic Weapons refresh or
complete D6.

The first reload stayed pending. After client focus established Tal's control,
the test-start preflight still rejected its configuration. A focused regression
test reproduced rejection of readable copy-on-write name memory; the new test
name predicate now accepts it without changing the existing native-object
reader. `SudekiMP.StorySplitPreflightTest` passes all six readable protections
and rejects guarded/inaccessible memory, invalid names and invalid admission.

Reload observation: candidate `6ce135bb…` is deployed with matching hashes on
both peers, and the original configuration files are restored. Logs confirm
both exterior placements, one accepted native farmhouse entry with the authored
marker, settled interior state, and final placement of only Ailish. Tal stayed
at the requested exterior point throughout entry and then moved in response to
client input. Native invulnerability leases were confirmed for both. This is
CONFIRMED_LIVE placement/control evidence; the Weapons refresh owner gate and
D6 remain open. Window focus was activated; no gameplay input was automated.

### 2026-10-15 — D6 automatic Weapons refresh accepted

CONFIRMED_LIVE, saved-story profile26, Ailish host / Tal client on the
farmhouse test route, Linux/GE-Proton Wine, revision `4370296` plus preserved
D6 dirty changes, candidate `6ce135bb…`. The owner confirms the acquired sword
automatically appears in Tal's open Weapons menu without reopening it. The
paired capture agrees: one chest reward, native count0→1 in both games, one
completed client grant and a successful active Weapons refresh. Evidence
artifact: `d6-weapons-refresh-accepted-20261015`.

This accepts automatic reward visibility in the open Weapons list. Highlight
preservation and the More Info return path retain their exact-image/synthetic
test evidence; neither was separately accepted in this live check. Chest
repeat-after-area-change, florins, personal consumption, host shop exclusion
and pending-loot teardown remain open. Earlier D3 live acceptance already
establishes that either player can collect host-authored drops; recheck Tal's
collection with D6 florin credit. Next owner check: collect a florin drop and
verify each game's purse gains its denomination once, then test Tal's personal
ointment use without debiting Ailish's inventory. No code change, rebuild,
reload, commit or push for this acceptance record.

### 2026-10-15 — D6 host shop purchase exclusion accepted

CONFIRMED_LIVE, saved-story profile26, Ailish host / Tal client at New
Brightwater's nearby shop, Linux/GE-Proton Wine, revision `4370296` plus
preserved D6 dirty changes, candidate `6ce135bb…`. The owner reports that
buying from the shop did not update Tal. The read-only inventory capture
confirms two purchases, first two ointments and then one: Ailish's game changes
from4→6→7 ointments and365→265→215 florins; Tal's game remains at4 ointments
and365 florins throughout. The native inventory observer identifies both
transactions as shop purchases with the matching quantities, and no loot-copy
grant is emitted. Evidence artifact: `d6-host-shop-exclusion-20261015`.

This accepts buyer-only host purchases for this item/shop scenario. It does
not accept client checkout (Phase H), selling, other merchants or personal
consumption. Next: Tal uses one ointment through M and another through Q
while hurt; only his own stock may decrease, with native healing and effects.
The host-zero-stock/client-positive-stock variant, florin drops, extended chest
dedupe and pending-loot teardown remain open. The pair was reloaded at the
native save position with automatic test teleports disabled; party
invulnerability remains enabled. No gameplay input was automated. No code
change, rebuild, commit or push for this acceptance record.

### 2026-10-15 — D6 item-use preparation without teleporting

The native save restored full HP while test invulnerability remained enabled,
so it could not establish the requested healing/count check. The test-start
adapter now accepts valid `TestStartLeaderHealth` / `TestStartFollowerHealth`
fractions without `TestStartLeader` coordinates. This explicit health-only
mode clears placement/TEMP configuration and never calls position, zone-load
or TEMP seams. Malformed nonempty position configuration does not silently
opt into this mode. Defaults remain inactive.

After the normal ready-world delay, the existing host game-thread service
checks the exact roster and matching actor, changes HP through the existing
native resource setter, preserves SP and checks the result. It consumes the
one-shot before mutation, including uncertain partial setter outcomes.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE, dirty revision `4370296`, candidate
`95ab5574…`: DLL build and `SudekiMP.StoryTestStartTest` pass, including
once-only health, unchanged positions, no area/TEMP calls, readiness, actor
replacement, invalid configuration and uncertain setter cases.
`SudekiMP.SkillTraceImageTest <supported SUDEKI.exe>` exits0; its named
`GetCharacterNumberStat` and `SetCharacterNumberStat` byte checks actually ran.
That target also needed the existing inventory adapter added to its source
list to satisfy the D6 shared pickup-call validation dependency; this was a
test-link correction, not a new runtime hook.

The next owner gate remains personal consumption: start Tal at one quarter of
his saved HP at the native save position, with invulnerability retained, then
use an ointment through M and another through Q. Record fresh counts after
reload; previous shop purchases were accepted in their own capture and are
not presumed persisted. No item-use acceptance follows from this test aid.

### 2026-10-15 — Kamo interiors during a pending client handoff

**CONFIRMED_LIVE failure / CONFIRMED_STATIC cause:** saved-story profile26,
revision `4370296` with preserved D6 dirty work. Before Ailish entered the
New Brightwater Kamo shop, Tal's client remained at `pending_input_arm` and
had not acknowledged control. The host had acquired Tal's native lease, but
its exterior mask required READY and stayed zero. Native TEMP therefore ran
with `keep_live=0`, paused the exterior, and changed the scene epoch; the
client then stopped presenting at `native_presentation_changed`. Host frames
continued. This was not evidence of a missing shop asset. Initial focus-hook
setup is a plausible input-arm blocker; the old log does not identify the
specific failed predicate.

The correction separates acquired exterior ownership from acknowledged input
permission. An exact acquired, non-draining remote actor retains its exterior
during PREPARE as well as READY. Its native follower AI movement/snap is also
suppressed under the acquired ownership proof, so it cannot follow the host
while waiting for input. Player steering, attacks, activity and nearby loot
collection still require the existing READY/ACK gates. No native hook or
protocol format changed; stale owner, connection, thread, scene and native
identity checks remain.

The generic TEMP path covers the six authored Kamo interiors:
`LNBr_Kamo_shop`, `LICa_Kamo_Shop`, `LSMoB04_Kamo_shop`, `LTCi_Kamo_Shop`,
`DCRe_Kamo_Shop`, and `DCCi_Kamo_Shop`. These are regression inputs, not a
new destination allowlist. Supported archive level metadata contains these
six entries among 53 interiors; each corresponding zone resource exists.

**CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE:** DLL build and seven focused suites
pass: `StorySplitControlTest`, `StorySplitPreflightTest`, `StorySceneAreaTest`,
`StoryMeleeAdmissionTest`, `StoryAiMovementImageTest`, `StoryPlayerTurnImageTest`,
and `StoryTempExteriorImageTest` (normal and mismatch modes). Tests cover
PREPARE without input permission, ACK, lost readiness, invalid/draining leases,
multiple clients, all six shop names with four exterior character choices,
and retained scene epochs across entry/exit. The six named native contracts
`AI_MOVE_FRAME`, `AI_MOVE_CHOICE`, `AI_MOVE_PLAYER_CACHE`, `AI_SNAP_FRAME`,
`AI_SNAP_CHOICE`, `AI_SNAP_CLEANUP` actually executed with every-byte rejection.
Native TEMP body tests use a synthetic world and inert leaf callees; they do
not prove live behavior at all shops. A test-fixture name-padding error was
corrected before the final scene suite passed.

**Follow-up live result:** the delayed-ACK entry retained the exterior and the
same epoch; the client reached READY and continued presenting outdoor frames.
Opening Kamo's shop then exposed a separate control failure: the native script
raised Ailish's AI override from zero to one and used controller filter mode
two. The shared host binding test rejected that legitimate modal ownership
and drained Tal. The world continued presenting, but Tal lost control. A host
ointment purchase was recorded as a shop grant, with no copied loot grant;
the subsequent inventory observation was host five/client four, with purses
315/365. These are post-purchase values, not a new before/after capture.

The follow-up correction leaves global host readiness unchanged. A positively
owned, settled exterior character retains its independent lease through the
lead's interaction transition. Input additionally needs ordinary native host
control or the positively identified native shop modal; unknown transition
state retains AI suppression but grants no movement, cast or item request.
Fresh native residency and actor ownership are rechecked between dispatches;
no copied dispatch witness, native filter write or new patch seam is used.
New acquisition, party swaps and initial caster construction keep their prior
host readiness gates.

**CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE:** the follow-up DLL build,
`StorySplitControlTest`, `StorySplitPreflightTest`, and `StoryHostFilterTest`
pass. The last suite executes the named `IsShopActive` supported-image check
and rejects all 42 individual byte mutations. Tests distinguish retained
ownership from input permission, require READY/ACK, refuse changed roster,
split, world, thread, component and filter ownership, and close permission
immediately on entry/exit gaps. A synthetic residency fixture was moved to
private writable memory after its initial copy-on-write mapping was correctly
refused. That fixture change did not cover the executable singleton slots;
the subsequent owner test exposed the missing case described below.

**Owner recheck failed / mapped-page cause confirmed:** with the shop open,
some effects continued but Tal and outdoor NPCs stopped. The log again shows
Tal's lease draining on the lead's override change, releasing all 14 remote
NPC activity leases. Closing the shop restored control. Read-only live checks
found the split settled and its exemption retained, but the new residency
snapshot empty. Windows reports all three executable singleton slots (world,
party group and controller) as committed `PAGE_WRITECOPY` image pages; the
new capture had incorrectly applied its narrower heap access check to them.
The world, descriptor and zone-name objects themselves were ordinary private
writable pages. No global world pause or transport stall was established.

The correction admits readable copy-on-write access only for those three
fixed slots within the verified executable's original allocation. Native
object ownership checks remain unchanged. The settled diagnostic now reports
residency capture outcome, refusal stage and the three page protections.
**CONFIRMED_TEST:** the rebuilt DLL and `StorySplitPreflightTest` pass, including
named `SplitImageGlobalsReadable`: real `FILE_MAP_COPY` singleton slots before
and after a private write, full settled/capture success, and guarded,
inaccessible, wrong-allocation and unmapped refusals. The previous private
image fixture had missed this live page type. The owner subsequently accepted
Tal and NPC movement with the shop open on candidate `ab57cf75…`; earlier
pure checks alone had not established that live acceptance.

**Remaining owner report:** Tal stays in his own area, but his control is
interrupted when Ailish enters or leaves another area. This is independent
client continuity, not the host's own loading or doorway duration. The captured entry holds input for about 4.2 seconds:
about two seconds before TEMP begins, a short native transition, then about
two seconds after settled exterior residency. Tal's own pause clears shortly
after settlement, leaving roughly 1.85 seconds of avoidable host-filter wait.
The same run also contains a three-second buying-screen opening filter hold;
no client movement was attempted in that particular opening window. No exit
is present in that capture, so its cause and duration remain unmeasured.

The next scoped correction separates exterior steering from action admission.
An exactly owned, positively settled exterior body may move and turn once its
own native pause count is zero, without borrowing the lead's modal filter.
A genuine pause returns a held result before target, dodge or even neutral
movement submission. Native world/body gates, connection, READY/ACK and fresh
input remain mandatory. Attacks, block, ranged fire, items and casts retain
the previous positive modal admission. No pause counter or host filter is
written. The change does not grant control across unknown residency, before
TEMP entry, or after the split's exit proof has been retired.

**CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE**, dirty revision `4370296`, candidate
`0aaf5b94…`: DLL build, `StorySplitControlTest`, `StoryMeleeAdmissionTest`,
`StoryHostFilterTest <supported EXE>` and `StoryPlayerTurnImageTest <supported
EXE>` pass. The named `NativeNodePauseCounterTest`,
`NativeNodePauseCounterIncrement`, `NativeNodeResumeCounterDecrementAndZero`,
`IsShopActive` and six player-turn/movement byte contracts actually execute.
Policy tests distinguish steering from action permission, preserve pause
references without target/dodge/movement calls, refuse stale ownership and
resume on zero. An initial movement-vector assertion used nonbinary decimal
components; the fixture now uses exactly representable components, retaining
the exact equality and native-call-count assertions. No production change
was needed for that fixture correction. These checks do not prove live delay
reduction or safe control across an unobserved exit.

**Owner follow-up on `0aaf5b94…`:** slightly improved, but the requested
behavior is that a client remaining outside keeps control while the host
changes areas. The new exit capture confirms a 575ms authoritative steering
hold. Tal's client remains READY, continues presenting, and sends fresh
nonzero movement; the host has the same actor/fence/transaction but gates it
on its own input filter after return. Native Tal pause is zero during the
sampled hold. Client and host positions both plateau. The measured packet
window has a maximum gap of39ms, so this is not a matching network outage.

The new entry is not an already-controlled-client acceptance sample: Tal
finishes initial input admission after Ailish is inside, with no movement
packets before entry. The code also invalidates presentation on global scene
revision changes while waiting for a matching party/world pair, and clears
input binding as a consequence. This is a separate short re-arm path; do not
claim that startup capture measures an established client's interruption.

The correction extends only the exact retained exterior identity across
a completed host return. It requires the same kept-exterior entry ticket, a
matching balanced exit receipt, a positive ordinary-world settlement, and the
same world/descriptor/data/party/leader/actor/AI/epoch. The return proof expires
within five seconds and retires when host input readiness returns, on a new
transition, or on identity loss. Genuine native transition/pause gaps remain
held. The action permission still requires its prior positive modal contract.
Client input binding may also survive a brief unrelated-area revision gap,
with the same established native owner and original fresh receipt; it sends
no new movement/actions and does not replay or relabel old media during that
gap. Fresh matched frames are still required for normal input/presentation.

**CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE**, dirty revision `4370296`, candidate
`3c35be26…`: DLL build and four focused suites pass: `StorySplitControlTest`,
`StorySplitPreflightTest`, `StoryHostFilterTest <supported EXE>`, and
`StoryTempExteriorImageTest <supported EXE>` in both normal and mismatch modes.
Named `SplitReturnedResidency` tests cover all four exterior character choices,
balanced receipts, settlement, identity replacement, expiry and ordinary-ready
retirement. `ReturnedExteriorSteering` refuses stale owners and native pauses
and leaves action admission unchanged. The client gap tests cover the original
250ms receipt horizon, initial PREPARE refusal, fresh identity/fence/connection
checks, and changed own-area rejection. The pending branch sends no input and
consumes transient combat/menu edges, including ranged presses.

The supported-image checks actually executed by name:
`TempReturnEntryCurrentDescriptorBeforeKeep`, `TempReturnExitZeroRegister`,
`TempReturnExitClearsPendingAndCurrent`,
`TempReturnExitOrdinaryDescriptorBeforeReceipt`, `IsShopActive`,
`NativeNodePauseCounterTest`, `NativeNodePauseCounterIncrement`, and
`NativeNodeResumeCounterDecrementAndZero`. Native body fixtures use synthetic
worlds and do not establish uninterrupted live travel. The pre-entry script
filter and genuine native transition/pause gaps remain unproven for independent
steering; this candidate does not broadly bypass them.

**Owner gate open:** establish Tal's control first, then keep him outside and
try moving him while Ailish visits and returns from Kamo. Confirm the
stationary client's control continuity separately from the host's transition.
Movement during the open shop is accepted; independent travel continuity
still needs the next candidate's live check. Also verify a delayed client handoff can
finish with the host already inside. The other five shops, other roster
members and disconnect during a shop stay still need live coverage. A draining
third participant can conservatively close the remaining exterior binding;
multi-client disconnect/recovery is not established by this two-player test. Resume
the D6 personal-item and florin checks after this gate; no D6 acceptance is
claimed from these tests.

### 2026-10-15 — Independent client travel continuity remains open

The owner reports candidate `3c35be26…` greatly reduces the exit freeze but
does not remove it; entry still stops Tal until Ailish arrives. This is not
acceptance of uninterrupted exterior control. In the paired saved-story
profile26 capture, two entry holds last2159ms and2093ms; two return holds
last46ms and43ms. The second entry begins after established client control.
Tal remains READY, sends movement, and the host continues publishing exterior
worlds while the client presents normally through the long pre-entry hold.
All sampled Tal pause counts in that long interval are zero. The generic
native TEMP entry begins only about two seconds after the hold starts.

The completed-return proof is admitted on both exits. The client binding
retention also runs on entry/return without another initial input-arm cycle.
However, withholding movement during the missing-pair interval produces
entry packet gaps of136ms and117ms, exceeding the existing100ms freshness
limit. These are separate from the long pre-entry host-filter hold.

Research identifies the authored generic `EnterInterior|SRSBS` helper: it
disables the host controller, waits for the fade, then invokes the native
interior entry. The next correction requires positive live helper/task,
program, controller and world identity before exempting an existing exterior
player from that host filter. A fresh-movement-only path across an unchanged
client area revision is also being tested. Neither path may revive stale
media, widen action admission, or bypass an unknown native pause. The residual
exit pause is now statically traced to `EnableFastLoad|B`, which calls native
PauseGame/UnpauseGame and pauses the global clock as well as entity scheduling.
A later stationary trip repeats a2118ms entry hold and42ms exit hold, with
packets continuing and no stale frames. The short hardware watch expired
without a transition, so there is no live caller-stack attribution from it.
Owner gate remains continuous Tal movement while Ailish enters and leaves,
with Tal outside.


Candidate `ea987d31…` implements that positive pre-entry task proof and fresh
movement-only continuity. **CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE:** DLL and
seven focused suites pass on dirty `4370296`: `StorySplitControlTest`,
`StoryMovementContinuityTest`, `StoryHostFilterTest`, `SpiritInstanceAbiTest`,
`SkillTraceImageTest`, `StoryInteriorTravelTest` and `StoryTaskHostImageTest`.
The interior suite actually runs named `StepOneOpcode`,
`CompiledCallPushReturnScope`, `ReturnPopScope`, `PushArgumentStack`,
`UIStartTSAFlag`, `UIInTSAFlag` and all seven `Script_<hash>` body checks,
including every-byte mutation refusal. Synthetic cases exercise the shared
STEP observer, own/foreign filters and TSA, child return scopes, stale
program/task/world, pre-owner startup, native entry retirement and teardown.
`ClientRevisionGapMotion`, `InteriorApproachSteering`,
`InteriorFilterObservation`, `IsShopActive` and the three native pause-counter
contracts also ran by name. The transport suite uses real loopback packets.

Fixture failures were retained: the initial whole-image COW mapping could not
accept the hook helper's write-protection request. Merely protecting a shared
view as WRITECOPY did not isolate its writes on the test runner. The final
fixture uses a genuine `FILE_MAP_COPY` singleton slice and checks that a
separate shared view stays unchanged. Production guards were not weakened.
A synthetic timing sample measures about0.93us per ordinary step observation
and44–99us per active proof with2/64 descriptors; live opcode cost is not yet
measured. The native retail VM/gameplay is not executed by the new proof test.

**OWNER GATE STILL OPEN:** establish Tal's control before moving Ailish, leave
him outside, and try moving continuously through her entry and return.
The new task lifecycle and revision-gap movement diagnostics are bounded.
Short global FastLoad pauses, presentation starvation, other shops/characters
and multi-client recovery remain separate unaccepted cases. No claim of zero
freeze or D6 acceptance is made; personal item-use/florins remain next after
this control gate.


### 2026-10-15 — First settled dispatch submitted a stop

Owner feedback on `ea987d31…`: much shorter, still not seamless; gate remains
open. On the later entry after established client control, the exact approach
proof arms and lasts until TEMP begins. No two-second scripted hold remains
there. The explicit settled steering hold is21ms, while client presentations
are109.8ms apart. That21ms does not include earlier native roster/capture gaps.
One recorded exit holds steering51ms. The first entry begins before initial
client READY and still holds2148ms; its unarmed proof ends with generic
`lost_exactness`, whose failing stage remains unknown.

CONFIRMED_STATIC / CONFIRMED_LIVE: an unknown TEMP roster clears the outside
eligibility mask. On the first exact settled dispatch, exterior steering is
queried before the mask is repopulated. Fresh input is replaced with zero;
only the end of that dispatch restores eligibility. Native SetSpeed
(RVA `DB070`) makes zero input a blended stop, not a no-op. Host and client
subsequently show brief idle-to-run transitions (about122ms/105ms). Attribution
of their full duration to that single stop is an INFERENCE, not a measurement
of the setter's exclusive effect.

Candidate `bce72ec0…` adds a fresh eligibility pass before per-peer steering.
It requires the current READY scene/roster, observing connected peer, unchanged
assignment/connection/fence, acquired undrained unswapped actor and exact native
lease. PREPARE can retain residency but gains no input permission. The existing
post-service recomputation remains; native pause and action gates are unchanged.
CONFIRMED_TEST: DLL build, `StorySplitControlTest` and `StoryHostFilterTest`
pass. Named `SettledEligibilityPrepass` covers first settled recovery, PREPARE,
unknown/loading/null/changed scenes, stale/disconnected peers/native leases,
swaps and unchanged input/readiness state. Named host-filter, shop and all
three native pause-counter image checks actually ran. Two independent source
reviews found no remaining blocker in this correction.

OWNER GATE: establish Tal's control before Ailish travels, then keep moving him
outside through her entry and return. This correction removes an avoidable
stop submission; the roughly110ms client matching-frame gap and genuine native
FastLoad pause remain separate work. Do not claim seamless travel or advance
D6 on build/test evidence alone.

### 2026-10-15 — Kamo transition improvement accepted with residual flicker

CONFIRMED_LIVE (owner report), saved-story profile26, dirty revision `4370296`,
candidate `bce72ec0…`: the owner reports that the interruption is now a
"miniscule flicker" and "much more acceptable", while explicitly saying it is
not seamless. Record this as qualified acceptance for continuing the D6
checks, not as removal of every transition pause. The residual flicker stays
open for follow-up; its duration is not measured by this visual report.

The paired runtime logs contain two entries after established client control
and two returns. Both entry approach proofs arm and end at native TEMP; no
explicit entry steering hold is logged, and the entry input windows contain
moving frames with zero held/stale frames. Return steering holds are 41 ms and
42 ms. All three logged client revision gaps report movement sent. One entry
still has a 115.3 ms presentation interval. No post-startup READY loss or host
drain is recorded. These observations support the improvement, but absence
of an explicit entry hold does not exclude native capture/scheduling gaps.

The earlier entry before initial client READY, other shops/roster members,
live observer overhead and multi-client disconnect/recovery remain unproven.
The native FastLoad pause and matching-frame gap are not declared resolved.
No new code, build, reload or gameplay input accompanies this acceptance note.

Next is Tal's personal item use through M and then Q: healing and effects
should work, each use should consume only his own stock, and Ailish's stock
should remain unchanged. Establish current counts rather than reusing a
pre-reload baseline. Then verify that a collected florin reward credits each
player's purse exactly once. D6 remains open until its own checks pass.

### 2026-10-15 — Residual flicker: independent camera continuation candidate

The owner prefers removing the remaining flicker before returning to D6. The
previous improvement remains accepted, but this follow-up owner gate is open.
CONFIRMED_STATIC: a missing current-revision party/world pair skips the entire
presentation callback, including the client's owned camera. Its next update
can receive the accumulated interval, capped at 100 ms. The latest tested
entries retain Tal's running clip, so the former injected idle transition is
not observed there. Camera catch-up contributing to the visible flicker is an
INFERENCE; the separate 41–42 ms host simulation holds remain measured limits.

Dirty `4370296`, candidate `f919367d…`, adds camera-only continuation during an
already established unchanged-area revision gap. A shared clock serves normal
and gap frames, rejects duplicate ticks and avoids deferred catch-up after a
refused gap frame. Current peer, control fence, native roster, selected actor,
camera ownership, current-revision normal cast selector and non-cinematic
dialogue must all be positively observed. The original 250 ms receipt limit
remains. Menus, casts, recruitment, changed identities and unknown state refuse
continuation. The native preflight permits already active Exploration only;
it cannot request a camera-state change. Complete target validation now
precedes camera input and target refresh. Fresh camera-look input retains its
original timestamps while gameplay input is discarded during the gap.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: DLL build and three focused suites pass:
`StorySplitControlTest`, `StoryQuickInputTest` and
`StoryClientCameraImageTest <supported EXE>`. Actual named checks include
`RevisionGapCameraClock`, `RevisionGapCameraAdmission`, `CameraGapInput`,
`CameraContinuityNoMutation`, `CameraTargetPreflightBeforeWrites`, and native
byte guards `CameraNodeUpdate`, `CameraInputEvent`, `SetCameraState`,
`CanSetCameraState`, `NodeVtableUpdateSlot` with every checked byte mutation
refused. These are synthetic runtime/native-ownership fixtures and exact-image
guards; they do not execute a live camera solve or establish visual acceptance.

OWNER GATE: establish Tal's control, then keep him outside while Ailish enters
and leaves Kamo. Compare the brief interruption and try orbiting Tal's camera
through travel. Logs must show actual camera continuation; fresh cast/dialogue
packets may restrict when it can run. No stale actor/effect replay or native
world-pause bypass is implemented. The new pair still needs owner acceptance;
seamless motion, other shops/roster and multi-client recovery remain unproven.
D6 personal item use and florin checks wait for this follow-up.

### 2026-10-15 — Entry imperceptible; small exit bump still under investigation

CONFIRMED_LIVE (owner report), profile26, dirty `4370296`, candidate
`f919367d…`: the owner cannot really see the entry interruption anymore but
still notices a slight bump on exit. This accepts the observed entry
improvement; it does not establish seamless return or complete D6.

The two measured returns retain Tal's running clip, fresh input and ownership,
with explicit host steering holds of 48 ms and 44 ms. One host sample
positively records Tal's native pause count at one; the client subsequently
shows that paused position. Neither exit enters the client's missing-pair
state. The camera-only gap callback logs no successful application in this
run: its two entry attempts are refused. Therefore the reported improvement
must not be credited to successful camera-only continuation. Shared clock and
look-input changes are present, but their exclusive contribution is unmeasured.

CONFIRMED_STATIC: the first single pair after a revision change is shown at
its newest tick. Later pairs restore the interpolation delay and may hold that
already displayed tick until the delayed timeline catches up. Whether this
adds a visible jump/hold to the measured native pause is still a HYPOTHESIS.
Changing the buffer ramp would trade a hold for slower playback; no such
behavior change is made without evidence.

A bounded read-only capture is prepared on the existing pair to compare the
displayed host tick, frame receipt, revision and buffer with Tal's native
position/pause and the client camera clock/view. No reload, native mutation or
gameplay input accompanies this pass. OWNER GATE: one Kamo entry/return with
Tal moving outside near the exit, then inspect the timing capture. D6 remains
paused at this follow-up; other shop/roster/multi-client coverage stays open.

### 2026-10-15 — Return timing measured; camera dip also occurs while idle

CONFIRMED_LIVE, same profile26/dirty `4370296` candidate `f919367d…`:
the completed read-only capture includes one entry and return with Tal nearly
stationary. Return has a 42 ms explicit host steering hold. Its displayed
source tick advances by 88 ms, then remains unchanged for at least 40.5 ms;
the first later advance is sampled 48.6 ms after the new-revision sample.
Entry similarly advances by 115 ms and first progresses again after 65 ms.
This confirms the timestamp jump/rebuffer behavior, without proving its
visible contribution during running. Both transitions have missing-pair frames
in this pass; their camera-only callbacks again refuse. These observations do
not replace the earlier pass where return had no missing-pair frames.

The return camera eye drops approximately 0.01022 world units while Tal's
height changes only 0.00000334. Four similar downward camera steps also occur
outside travel, with no revision change. All five observed steps coincide
with camera-clock increments of 23–25 ms; ordinary shorter increments have
only minute height changes. The offset persists after each step. This is a
measured timing correlation, not yet proof of a specific native solver fault.
The rebuffer hold cannot alone explain the same-revision camera steps.
Native camera timing/solver research continues before another correction.
No new source change, build, reload or gameplay input accompanies this capture.
D6 item-use and florin acceptance remains pending.

### 2026-10-15 — Small camera-height correction candidate

CONFIRMED_EXACT_IMAGE: the Exploration solver computes a spring correction
from elapsed time squared, clamps it to the remaining height error, then
discards corrections below approximately 0.0099 world units. Its original
apply continuation still performs the collision check. This explains a
mechanism consistent with the measured centimetre-sized steps at longer
client camera intervals; it does not establish that all return interruption
comes from the camera. A later target-velocity capture had zero target
velocities and no comparable dip, so stale velocity was not established as
the cause. Native timing has no fixed 45 Hz floor.

Dirty `4370296`, saved-story profile26, candidate `2d6c862c…` adds a guarded
continuation at RVA `158026` for small, finite, nonzero corrections during the
owned client's Exploration camera update. Fresh camera/state/data/target,
selected-actor, native caller/context/geometry and game-thread proofs are
required. Accepted corrections enter the original apply/collision path at
`15805C`. Cast cameras, unowned calls, ordinary corrections and invalid values
retain the native branch. No global constant, elapsed-time cap, substep or
world-pause bypass is introduced. Checked teardown precedes retirement of its
dependencies and retains the module on restoration failure.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: the DLL builds and all three focused
executables actually pass: `StoryClientCameraHeightTest`,
`StoryClientCameraImageTest`, and `StorySplitControlTest`. New named byte
checks are `HeightCaller`, `HeightEntryAndErrorClamp`, `HeightCutoff`,
`HeightApplyCollision`, `HeightSpring`, `HeightConstants` and
`HeightCutoffRelocations`; every checked-byte mutation is refused. Execution
checks cover signed tiny corrections, original outside-scope behavior, x87
stack/control, registers, flags and LastError preservation, fresh identity and
thread refusal, final-patch failure and restoration retry. Existing camera
preflight and revision-gap clock/admission checks also pass. The new fixture
stops before collision/full-world execution and does not prove arbitrary
concurrent teardown safety; runtime teardown remains on the game-thread seam.

OWNER GATE: at the same save point, check Kamo entry and return with Tal
outside, then ordinary idle, movement and camera orbit near obstacles. Look
for the remaining small vertical bump and any new camera drift or collision
problem. Visual acceptance is pending. Native FastLoad pauses and the
new-revision rebuffer are separate measured limits; zero-freeze continuity
and D6 item-use/florin acceptance remain unproven.

### 2026-10-15 — Residual entry/exit hitch deferred to #78

At the owner's request, created [#78: revisit the small client hitch on nearby
host area transitions](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/78).
This covers both host entry and return while the client stays in its current
area; “exit bump” was the earlier shorthand. The issue records reproduction,
measured contributors, the tested camera-height candidate and remaining owner
acceptance. Further hitch investigation is deferred. The latest candidate is
not marked visually accepted, and seamless continuity remains unproven. No
code change, build, reload or gameplay input accompanies this issue record.

### 2026-10-15 — D6 personal item-use gate resumed

After deferring the transition hitch to #78, the owner requests the next D6
check. The existing profile26 pair, dirty `4370296` / candidate `2d6c862c…`,
was verified and a bounded read-only inventory/health capture started. Fresh
stable readings show four Healing Ointments and 365 florins in each game,
Tal at 100 HP and Ailish at 300 HP. No reload or teleport was needed.

OWNER GATE: Tal uses one ointment through M, closes M, then uses another
through Q. His stock should change 4→3→2, the host's should remain four, and
both uses should heal with normal effects. Actions and visual acceptance are
pending; baseline observation alone does not pass this gate. Florin collection
follows this check.

### 2026-10-15 — D6 Q debit without heal; local item replay freezes client

The owner reports an ointment consumed through Q without applying, followed
by a client stuck at host world sync. CONFIRMED_LIVE on candidate `2d6c862c…`:
the first M request debits the client 4→3 and heals Tal 100→350 on the host,
then on the client. The intervening Q attempt debits 3→2 with HP unchanged
and no matching host request/result. A second M request debits 2→1 and heals
to 600. The host inventory remains four and both purses remain 365 throughout.
The completed read-only capture has no unknown inventory/health samples.

After that second M result, the local native apply creates an extra entity;
the next containment check reports both entity and scheduler mismatch and
retains the paused UNKNOWN state. The Q hooks are still installed and retained,
but their menu-owner and trace count remain zero. The Q bypass is therefore
unresolved; the absence of a request is established, not its originating path.
The two M heals do not pass the full gate because safe continued play fails.

Dirty `4370296`, profile26, candidate `3b8f95fe…` removes native item-script
execution from accepted client results. The client debits its inventory once;
the existing host resource, animation and heal-effect replication paths own
presentation. It does not force-clear a busy actor or relax containment.
`StoryItemResultTest`, `StoryMainMenuImageTest` and
`StoryWorldVisualImageTest` actually pass. The first executes the runtime result
consumer with spies for accepted/duplicate/rejected/mismatched results, absent
ownership, peer/actor replacement and failed bookkeeping without script fallback.
Named image checks include `UseCall`, `ListConfirmCall`,
`WorldVisualWeakDestruction` and `WorldDropCollect` (native/owned observer call,
foreign redirect refusal). DLL build and diff check pass. These tests do not
prove live healing effects, Q routing or recovery from the previous paused run.

OWNER GATE after reload: open Q and leave the item selection visible without
consuming, so its actual route can be observed. Recheck M healing, debit,
replicated effects and continued control after the correction; Q consumption
and florins remain pending. The separate transition hitch stays deferred to #78.

### 2026-10-15 — D6 Q healing and continued control accepted

Saved-story profile26, dirty `4370296`, candidate `3b8f95fe…`, same New
Brightwater pair. The owner confirms: “he heals and stays controllable” and
requests moving on. CONFIRMED_LIVE: the highlighted Items category is owned by
the Q adapter; confirmation queues item204, receives one accepted host result,
and debits client stock 3→2. Tal's HP changes 100→350 on both games. Host stock
stays four and both purses stay 365. The result records local reduction only,
with presentation assigned to host replication. The native host item action
reaches its applied and completed states; no client containment failure is
recorded in this captured interval.

This passes Q healing, independent consumption and continued control for the
tested selection. It does not identify the originating route of the earlier
unowned-Q debit, establish visual-effect parity, or replace a corrected M-path
recheck. Host-zero-stock/client-positive-stock use remains untested. No new
code, build, reload or teleport is needed for this acceptance.

**Next owner gate: shared florins.** A bounded read-only inventory capture is
running on the existing pair. Collect a money drop with either character and
wait after collection. Both purses should gain the native denomination once;
the pickup/treasure grant must have one matching client application and no
later duplicate. Baseline is 365 florins in each game. D6 remains open; the
residual nearby-area hitch remains deferred to #78.

### 2026-10-15 — D6 chest florins credited once to both purses

CONFIRMED_LIVE on saved-story profile26, dirty `4370296`, candidate
`3b8f95fe…`. The owner completes the money-collection check. Both purses change
365→515, a full 150-florin credit to each game. Native source classification
is chest/treasure (source2), item321, quantity1. One host grant is queued and
one matching client grant completes without a cap hit. The read-only capture
records exactly one money change per game, no unknown samples, and no repeat
credit for the following 27 seconds. The same run has no client containment
failure. This establishes chest florin sharing and absence of an immediate
duplicate; loose ground-money pickup, area-return/reconnect dedupe and live
currency-cap behavior were not exercised.

**Next owner gate: corrected M-menu self-use.** On the same pair, Tal has two
Healing Ointments and 350 HP; the host has four ointments. A fresh bounded
read-only capture is prepared. Use one ointment through Tal's M Items page,
close the menu and move. Expect client stock2→1, host stock4 unchanged,
host-authoritative healing on both games, visible healing effects and continued
control. Q healing/control is already accepted; its earlier unowned-route
failure remains a separate unknown. No build, reload, teleport or gameplay
input was performed for the florin check. D6 remains open.

### 2026-10-15 — D6 M self-use debit and healing verified

CONFIRMED_LIVE, saved-story profile26, dirty `4370296`, candidate
`3b8f95fe…`, same pair. After the owner reports completing M self-use, one
accepted request for item204 reduces Tal's client stock2→1, leaves host stock4
unchanged, and heals Tal350→600 on both games. Both purses remain515. The active
Items page reports a successful refresh. Client native apply remains disabled;
`SFXM005_HEAL.HOM` is observed advancing through the replicated effect path.
No unknown inventory samples or containment failure occur in this interval;
the subsequent presentation reports roster, scheduler and entity checks valid,
input admitted and control phase5.

The owner subsequently confirms “Both looked normal” for healing effects and
movement after closing M. This passes M self-use healing, personal debit,
effects, active Items refresh and continued control for the tested Tal route.

**Next owner gate: host stock zero, client stock positive.** The owner sells
Ailish's four Healing Ointments at Kamo, closes the shop, then uses Tal's last
ointment through Q. Log the host4→0 and client1 retained before use, followed
by client1→0, a successful heal and no host stock requirement. The existing
bounded inventory/HP capture remains active. Loose ground-money pickup and
extended chest dedupe also remain. No code change, build, reload, teleport or
automated gameplay input accompanies this observation.

### 2026-10-15 — D6 zero-host-stock test prepared with missing health

The owner sold the four host ointments. CONFIRMED_LIVE: host stock4→0 and
purse515→655; client stock stays1 and purse stays515. Tal is full at600 HP,
and the owner cannot use another ointment; the two refused requests leave
client stock unchanged. The zero-host-stock healing check is not yet passed.

The existing health-only test routine was re-armed for this test, with exact
image/configuration/party checks and placement disabled. Its host game-thread
service reports Tal600→150, and a fresh read-only capture confirms150 on both
games, Ailish300 unchanged, and the zero-host/one-client inventory setup intact.
No reload, teleport, native action from the debugger, or direct native-health
write was performed. This is private test preparation, not a new gameplay
feature or a general live configuration API.

OWNER GATE: use Tal's last ointment through Q, then move. Expect successful
healing with client1→0 and host0 unchanged; the fresh bounded capture is active.

### 2026-10-15 — D6 client item use with an empty host inventory accepted

The owner confirms “used. it works.” CONFIRMED_LIVE, saved-story profile26,
dirty `4370296`, candidate `3b8f95fe…`: one accepted Q request heals Tal150→400
on both games while client ointment stock1→0 and host stock remains0. Client
bookkeeping reduces once without native script replay. Host/client purses
remain655/515 and Ailish remains300 HP. There are no unknown inventory samples
or client containment failures in the captured interval. This passes the
host-zero-stock/client-positive-stock case for the tested Healing Ointment.

The earlier host sale is also isolated: host stock4→0 and purse515→655, while
the client retains its item and515 florins. The chest reward still has exactly
one host queue and one client grant after M opening/closing and a logged Kamo
entry and completed return. That covers nearby shop travel without a repeat
chest award; full zone unload, re-interaction and reconnect are not claimed.

**Next owner gate: loose ground florins collected by Tal.** The same bounded
read-only capture remains active, with host655/client515 as the new purse
baseline. Collect one loose money pickup on Tal, wait ten seconds and verify
one native pickup-source grant credits its denomination once to each purse.
D6 stays open; no code change, build, reload or teleport accompanies acceptance.

### 2026-10-15 — D6 controlled ground-florin test preparation

The owner requested a nearby native drop instead of searching for a random
drop or combat chest. On the same dirty candidate and running pair, the exact
image and named native export/ABI, existing hook ownership and game-thread
roster checks passed. One native 150-florin bag was created near Tal through
the game's own drop function. Native setup completed without a direct purse
grant; host655/client515 remained unchanged. The uncollected bag subsequently
disappeared before the owner pickup check. This is preparation evidence, not
acceptance of ground-florin sharing or its client visual.

A fresh single bag is now prepared with bounded read-only capture. Native
setup completion and the matching client cosmetic instance were both observed;
the two purses remain unchanged before collection. Tal should collect it by
proximity; expect host655→805 and client515→665 once,
with the bag disappearing on both screens. Owner acceptance remains pending.
No production code, build, reload, teleport, commit or push is part of this setup.

### 2026-10-15 — D6 Tal ground-florin pickup credited once to both purses

The owner reports “done” after collecting the prepared bag and waiting.
CONFIRMED_LIVE, saved-story profile26, dirty `4370296`, candidate `3b8f95fe…`:
the native pickup records Tal (character35), item321, quantity1 and consumed1.
One pickup-source grant (source1, serial2) credits host655→805 and client515→665.
The independent purse capture records exactly one increase of150 for each game;
a fresh observation55 seconds later confirms the same balances. The real host
drop and its matching client cosmetic instance are both removed.

This passes the loose-ground money sharing gate for Tal's proximity pickup,
including client presentation and removal as observed in native state. It does
not rely on direct purse writes or a chest grant. Owner screen-by-screen
visual comparison is not separately reported. No new production code, build,
reload, teleport, commit or push accompanies this acceptance.

**Additional coverage, not a new route-acceptance gate:** full area unload,
chest re-interaction and reconnect were not exercised. The original five D6
owner checks required moving areas without a repeat award; the observed Kamo
entry/return already covers that scoped check. Asking for a full unload as
the mandatory next gate unnecessarily expanded it. The five scoped checks
now have recorded results, with the Tal-client shop direction expressly
deferred to H. Broader coverage remains a follow-up and is not claimed passed.
Persistence recovery belongs to Phase I. Any later travel check starts from
host805/client665 unless new legitimate loot or spending changes the baseline.

### 2026-10-15 — D6 travel-test clarification

The recorded authored route out of New Brightwater leads through
`WS_Country_SE_to_Bright_NE` toward `Illumina_Countryside_SE`, the countryside
containing Frappe Farm (see the countryside-to-church mapping in the research
log). A visible loading screen alone does not prove a full world unload.
No travel was performed for this clarification. The owner does not need an
extra full-unload trip to pass the already completed ground-florin check.

### 2026-10-16 — Armour relay and live menu preview (#40, #74) — candidate, owner gate OPEN

Unattended work (owner direction 2026-10-16: "work unattended on things you can
do fully alone"). Revision `a706c1e` plus uncommitted changes; nothing pushed.

CONFIRMED_LIVE read-only memory scan (both games of the live pair, image
relocated to 0x7ABD0000 under Wine): armour items are ids 100..139 (class
vtable RVA 0x2D39B8, item class +0x88 = 0x12..0x20); CEquipped =
`*(character+0xE0)`, vtable RVA 0x2D4EF4, owner backpointer +0x10, worn armour
+0x18, its id +0x1C. Tal wears 101 and owns 100 and 101 in both games; Ailish
wears 105. CONFIRMED_STATIC: native armour equip RVA 0x160580 takes the armour
in EAX and CEquipped on the stack, RET 4; with +0x18 cleared it skips the rune
copy (the armour-choice rule).

Found while tracing: on the client the armour page's Enter reached the Main
Menu equip intercept, which admitted only weapon ids, so armour equips were
silently refused. And while the client's menu is shown the engine skips its
world presentation, so the replica cannot follow a relayed weapon until the
menu closes: that, not a missing refresh, is why the menu preview stayed stale.

Candidate (DLL `75e3027e…`):
- Party frame version 9: actor byte 97 carries the worn armour (id+1; 0 =
  not observed, never "unequip"). Host fills it from CEquipped; the client
  replica equips it natively once per epoch/generation/armour, granting the
  item first if this game does not own it (D9), and never blocks the frame.
- New request kind EQUIP_ARMOUR (slot = armour id 100..139, same fence-only
  transport rules as EQUIP_WEAPON). Host checks the class for the character,
  grants a missing armour once, equips natively (`host_equip_armour`).
- On a STARTED weapon or armour result the client equips its own character
  with the same native routine its menu would have used and refreshes the
  open page and 3D preview (`equip_result … local=1 refreshed=1`).
- Tests: story frame codec (armour byte bounds, round trip, reserved bytes),
  action transport (armour request), SkillTrace image test (armour equip entry
  and its +0x18 compare refuse a changed byte); Main Menu image/refresh,
  inventory image, loot grant, item result suites still pass.

**Owner gate (Ailish host, Tal client, both games freshly launched):**
1. Tal opens M → Armor and equips the other armour (100 ↔ 101). Expected: the
   preview in Tal's open menu changes at once; Ailish sees Tal's new armour;
   after closing, Tal's own screen shows it too.
2. Tal opens M → Weapons and swaps weapon. Expected: the menu preview now
   updates while the menu is still open (previously stale).
3. Ailish opens her M → Tal → Armor and switches Tal's armour. Expected: Tal's
   screen shows the change without a freeze.
Logs: client `story_main_menu event=equip_result`, `lan_story_replica
event=armour`; host `host_equip_armour`, `armour_equip … applied=1`.

### 2026-10-16 — H1 remote chest use: static research and proposed design (no code)

CONFIRMED_STATIC (Ghidra decompiles + disassembly, supported image). How a
chest is opened natively, and what it does to "the player":
- Candidates: trigger-volume callback RVA 0x123D30 tests an entity against the
  volume (RVA 0x124320, third argument "entity is front") and calls the scan
  RVA 0xD330, which appends a candidate {kind, target, trigger entity} to the
  interaction manager (`*(0x408D24)`, up to 15 at +0x20, stride 0x1C).
- Per frame, RVA 0xCFD0 → 0xD6F0 → 0xD7A0 walks the candidates; with the press
  latch (+0x1D8, written by the front's weak-attack press) the gate RVA 0xD9A0
  checks OnAction presence, the front arbiter busy bit 0x80, combat and UI
  state, then RVA 0xCCD0 builds the SOL message (+0xC trigger entity, event
  string, kind, +0x40 "source is front") and queues it (RVA 0x1B9EF0). The
  message carries no actor: the script and the natives it calls use the front.
- The chest's OnAction script calls the treasure use export RVA 0x184EC0
  (data reference RVA 0x2D51A0). It admits only if RVA 0xB60A0 accepts the
  **front character's position and facing** against the chest, sets the
  front arbiter bit 0x200 and switches the controller off (RVA 0x295B0(0) at
  0x184F7E). The treasure state machine RVA 0x1849D0 then turns the front
  toward the chest (RVA 0xB67B0 → turn RVA 0xB6E50), plays and waits on its
  use animation (RVA 0xB5EC0/0xB5F30 → 0xE0EA0/0xE0F30), and in states 2/7
  clears 0x200 on the front, switches the controller back on (0x184B69 /
  0x184CA0) and awards the contents (picked up by the D6 loot copy).
- The front is read from the party group `*(0x408D94)` at these sites in that
  call graph: 0x184F43, 0x184B2E, 0x184C5E, 0xB60C1, 0xB6306, 0xB67B3,
  0xB68AE, 0xB5E63, 0xB5EC3, 0xB5F33.

Consequences: a host-proxy open (send OnAction as if Ailish used it) would
freeze **Ailish's** controls, turn and animate **Ailish**, and refuse unless
Ailish faces the chest; it would not let Tal open a chest Ailish is away from
(the owner's complaint of 2026-10-14).

Proposed design (HYPOTHESIS until built and played):
1. Input: no new client path. The client already sends Tal's weak-attack
   press. On the host, before submitting it as melee, check for a usable in
   front of Tal (treasure entities in the world registry within reach, unopened,
   and the 0xB60A0 facing test evaluated for Tal); if found, the press becomes
   a use, exactly the native rule for the front player.
2. Dispatch: build the chest's OnAction message with RVA 0xCCD0 for its
   trigger entity (the same message the native dispatch would queue).
3. Scoped identity: while that treasure component's use export and state
   machine run, the ten front reads above resolve to Tal (call-site hooks on
   their handle copies, scoped by the active component, or a shadow group with
   slots swapped for the duration of those calls), so Tal is the one checked,
   turned, animated and flagged 0x200.
4. The controller on/off calls are skipped while the opener is remote (the
   host keeps control); Tal's own control is held by the 0x200 bit, which the
   remote move path already respects.
5. Doors are a separate case: a door traverse moves its user, so doors need
   their own trace before reusing this.

Open questions for the owner before building: (a) while Tal opens a chest,
should Ailish keep full control (proposed) or pause like single player; (b)
which chest on the route is the first gate (the farmhouse sword chest was
used for D6).

### 2026-10-16 — #72 host save transfer to a joiner: built and run end to end

Unattended work; revision `a706c1e` plus uncommitted changes, DLL `d0536c98…`.
Protections are the owner's from #72.

What was built:
- `src/network/story_save_transfer.c`: policy-neutral core. Only the two
  native records travel, as one stream (main record then small record), at
  their fixed sizes (2,621,952 and 1,536 bytes; 26 of 26 saves in two
  prefixes, CONFIRMED_LIVE). An offer must name exactly the hashes the lobby
  already published; 448-byte chunks are accepted strictly in order for one
  transfer id; the received bytes must hash to the published pair before they
  are released. Staging writes through temporary names, re-reads and re-hashes.
- Lobby (version 12): two frames (client save request, host save data) over
  the existing exact-build TCP lobby. The host serves only its verified,
  currently selected pair; a client accepts an offer only after it asked and
  only for the published identity. Tests: offline protections (foreign
  request ignored, unsolicited offer ignored, tampered chunk refused, a new
  destination retires the pair) and a real-socket loopback moving the full
  pair through the worker in about 1.2 s.
- Import: a received pair goes to a reserved, marker-tagged folder
  `SAVESLOT9900..9999`; an unmarked folder (a player's own save, even in that
  range) is never written, reused or removed. Seven-day expiry removes only
  marked folders' own three files. Expiry runs once when the lobby opens.
- Lobby UI: the host provides its reviewed pair after selecting a save; a
  joiner without a local copy requests it, shows progress, cannot get Ready
  until it has it, imports it, then refreshes the game's save list.
- The stale `TitleLobbyProtocolTest` expectations (destination before
  selection, credential reclaim after departure) were updated to the current
  lobby contract; it passes again, five runs in a row.

CONFIRMED_STATIC + CONFIRMED_LIVE findings on the way:
- The small record stores its own folder as a NUL-terminated string in the 16
  bytes at 0x200 (`SAVESLOT0011\`; bytes after the terminator are left-over
  buffer contents, e.g. `d7 75` in slot 0000). The native list keys rows by
  that stored folder and loads from it: a copy left under another folder name
  failed `story_load … reason=catalog_or_row`. The import therefore changes
  only the four folder digits; the marker records the original folder; a
  reserved copy is accepted only when its main record hashes to the published
  value and its small record, with the original digits restored, does too.
- The game caches its save list once (`*(RVA 0x34B320)` state 5). Its own
  invalidation routine RVA 0xFEFC0 (used by native saving, callers RVA 0x804A0
  and 0x164AA0) frees it so the next Load page rebuilds; the import calls it
  through an exact-byte, title-thread, no-load-in-progress guard.

CONFIRMED_LIVE end-to-end self-run (no gameplay input; the automated lobby
selects, readies and starts): the client's copy of save 11 was moved out of
its save folder; the client logged `save_check local_copy=0`, received the
pair, `save_received imported=1 reserved_slot=9900`, `native_catalog_
invalidated state=5->0`, `saved_pair_prepared host_folder=11
local_folder=9900`, loaded and entered the game; the host loaded; the client
presented host frames and took control of Tal. The client's own slot 11 was
restored afterwards; the reserved copy stays (it expires after seven days).

Not built yet (#72 scope): re-send when the host saves during the session,
the client save-menu guard, the next-launch keep/discard prompt, the launcher
import/export button and uninstall cleanup. Owner gate still needed on two
machines or a joiner prefix with no save at all.

### 2026-10-16 — Client Q-menu weapon swap; H1 owner answer; #79 filed

- **Q-menu weapons (client):** the owner reported that M-menu weapon swaps
  worked for Tal but Q-menu swaps did nothing. The client log showed
  confirms on the weapons page as `category=2 limit=0`, so the press was
  swallowed. The Q-menu adapter (`src/hooks/lan_story_quick_menu.c`) now
  resolves a weapons row the way the native page does: the character's
  weapon inventory category (Buki 7, Elco 6, Tal 4, Ailish 5; read from the
  live item table), item lookup 0x21CE0, weapon vtable check, and the
  character's own 12-id weapon range. It then queues EQUIP_WEAPON, which uses
  the existing host relay and client result path. Evidence: CONFIRMED_TEST
  (focused suites pass; full build clean). Live gate OPEN: the owner should
  swap Tal's weapon from the Q menu.
- **Build fixes:** a706c1e left four test targets unlinkable (the shared
  world-visuals header now calls the inventory module; the outbox test needed
  stubs for the client result effects). The test targets were fixed;
  product code is unchanged.
- **H1 decision (a):** while one player opens a chest, the other players keep
  full control of their own characters. Only the opener's character is held
  by the chest-open sequence. Chest choice for the first live gate: pending
  from the owner.
- **#79 filed:** in some scripted scenes (Ailish's weapon training) the client
  sees a chest's glow but not the chest or the training obstacles.
  HYPOTHESIS: script-spawned objects are absent from the client's world while
  their SpecialEffect is replicated.
- **#79 invisible chest, cause found (CONFIRMED_LIVE):** `HC_TutorialInvChest`
  is loaded in both worlds. Ailish's dispel fades its render-object alpha
  (object+0x13) from 0 to 255, removes the firefly marker
  `SFXAR006_10_REVEALED` and plays `SFXAR005_30_REVEALED`; the client's own
  script never runs it. Writing alpha 255 on the client made the chest visible
  to Tal, and the hidden render flag on his firefly effect removed the
  fireflies. Candidate fix (CONFIRMED_TEST only): world wire v11 carries
  render alpha for scenery, doors and chests (flag 0x40, actor byte 127); when
  a chest or door goes from invisible to visible, the client hides reveal
  markers within 1 unit (visual flags only, like spawn sigils). Live gate
  OPEN: reload before the dispel and repeat it. The weapon-training
  obstacles are not yet checked.
- **Client frozen while the host smashes or opens (owner report):** remote
  steering required the host controller in its normal mode. Native front use
  animations (chest 0x184EC0/0x1849D0, breakable 0x150FF0/0x150B20/0x150DF0,
  usable 0x19C470/0x19C010) switch it off with 0x295B0(0) and set the front
  arbiter bit 0x200 (CONFIRMED_STATIC). Live log: Tal held about 2.4 s per
  smash and 4.7 s per chest. Dialogue and the script gamepad export switch it
  off without 0x200, so they still hold remote players. Candidate fix
  (CONFIRMED_TEST only): a read-only front-use lock check admits remote
  steering, actions and turn leases during such an animation; swap, recruit
  and menu gates are unchanged. Live gate OPEN.
- **Tutorials (owner decision):** in Ailish's weapon training,
  `HC_TutorialInvChest` and its marker exist only in the host's world
  (registry diff, CONFIRMED_LIVE): the tutorial script creates them. Clients
  will spectate Ailish's view during her tutorials instead of having objects
  created in their worlds (#80). Detecting a tutorial is UNKNOWN. The same diff
  showed two barrels the host smashed still standing on the client
  (UNKNOWN cause, not yet filed).
- **Tutorial stall traced to the test-start teleport (CONFIRMED_LIVE):** after
  the hidden chest, the next stage (`HC_SE_Dispell`) never started in co-op or
  single player when the party was placed by TestStartLeader/TestStartZone. A
  vanilla run (observed by an external memory poller) and a modded
  single-player run reached on foot both completed. Narrowed further: the
  teleport is fine; `TestStartZone` (background `SwitchZoneNOW` load of the
  zone) is the cause. A start at the countryside border without a zone key,
  walking in, completes; a zone-key start west of the `HC_SE_zone_audio`
  trigger still stalls. Background-loaded zone entities register with
  refs=0 versus 1 normally; the missing setup is UNKNOWN. A test-tool
  limitation, not a co-op gameplay bug.
  Full stage sequence and the end signal (music back to `BS_IllumCountry`)
  are on #80.

### 2026-10-16 — H1 scope and owner decisions

Owner decisions for H1 (remote interaction), 2026-10-16:
- (a) While a remote player opens a chest, the other players keep full
  control of their own characters; only the opener is held.
- Doors are in H1 scope: a remote player can go inside through a house door
  **alone**. The host's player stays outside with full control, the reverse
  of the accepted case where the host is inside and the client outside. This
  needs new engine work: the host keeps the interior loaded and simulates the
  remote character there while its own view stays outside.
- First chest gate: the farmhouse sword chest (the D6 chest).
- NPC talk and shops for remote players come later.

### 2026-10-16 — H1a remote chest: seams for a scoped front (research, no code)

CONFIRMED_STATIC (supported image), extending the H1 research above:
- The front is always the handle at `party_group+0x90` (party group
  `*(0x408D94)`; party slot N is `+0x90+N*0xC`). Every front read in the chest
  call graph is `mov ecx,[0x408D94]; add ecx,0x90; lea eax,[esp+..];
  call 0x15B0`, where 0x15B0/0x15E0 are the generic intrusive-handle copy and
  release helpers (ECX = source handle, EAX = destination). Sites include
  0x184F53, 0x184B3E, 0x184C75 (treasure), 0xB67C4, 0xB68BE, 0xB5E74, 0xB5F44
  and the reads in 0xB60A0, 0xB6300 and 0xB5EC0.
- The treasure use export (RVA 0x184EC0) and the treasure state machine (RVA
  0x1849D0) are reached only through data references RVA 0x2D51A0 and
  0x2D51C8, the treasure component's vtable slots.
- The controller off/on calls are 0x295B0(0) at 0x184F7E and 0x295B0(1) at
  0x184B69 and 0x184CA0. 0x295B0 writes controller+0x84.
- The message builder RVA 0xCCD0 takes (manager, target handle, trigger
  entity, event name, kind, source). It allocates a 0x44-byte SOL message (+0xC
  trigger entity, +0x30 kind, +0x40 "source is the front") and queues it with
  RVA 0x1B9EF0. The dispatcher RVA 0xD7A0 calls it with the candidate's target
  (`candidate[1]-4`), trigger (`candidate[4]`) and "OnAction" for kind 2. The
  gate RVA 0xD9A0 needs the press latch (manager+0x1D8) and refuses while the
  front's arbiter has bit 0x80.
- Candidates are 0x1C bytes at manager+0x20 (count +0xC, at most 15), added
  by the scan RVA 0xD330 from the trigger-volume callback RVA 0x123D30.

Design (HYPOTHESIS until built and played):
1. Scoped front: relative-call hooks on the listed `call 0x15B0` sites only.
   When a game-thread scope for a remote opener is active and ECX is
   `party_group+0x90`, substitute that player's own linked slot handle. Outside
   the scope they pass through unchanged.
2. Scope windows: our own facing check (0xB60A0) and message build (0xCCD0)
   for the press, then the treasure vtable slots (use export and state
   machine) while that component's open belongs to the remote opener.
3. The three controller calls are skipped inside the scope, so the host keeps
   control. The opener is held by bit 0x200 on their own arbiter, which the
   remote move path already respects.
4. Press: the host turns the remote player's weak-attack press into a use when
   an unopened chest's trigger is within reach and the facing check passes for
   that player.

Open before building: a live read of a real candidate (target and trigger
values) at a chest the host stands at. Gate ordering: the farmhouse sword
chest is inside the farmhouse, so the chest gate needs the remote player
inside first (H1 doors, or the native take-along path, whose 2026-10-14
client freeze was never retested).

### 2026-10-16 — H1a remote chest use: built; owner sees a single clean open

Code: `src/hooks/lan_story_remote_use.c` (new adapter), host press hook in
`lan_story_runtime.c`, frame v10 use hint (byte 98), client icon apply in the
replica. Live findings on the way (CONFIRMED_LIVE unless marked):
- gdb on the treasure use export: the native open is dispatched by the
  interaction manager to the usable listener RVA 0xB5C30 (`treasure+0x3C`,
  args chest, actor, kind 2), then 0xB5CE0, then the use slot. No script call.
  The host now calls that listener for the remote player inside a scope that
  redirects the 16 front-handle copies to the opener's party slot, and skips
  the three controller switches.
- The open loop: the chest writes its use animation id into the opener's
  arbiter action fields; they are recorded at start and restored at the end.
- The second animation: the treasure finish handler (vtable +0x60, 0x185000)
  runs from the opener's own actor update, outside the update scope, and wrote
  the next action into the front (the host's character). It is now wrapped in
  the same scope; the wrapper keeps ESI, which 0xB5E60 reads as its action id.
- Paused areas: chests outside the host leader's area are paused and get no
  updates (zero in 4 s by gdb), so the open stalled. The host steps a remote
  opener's chest when it is paused or skipped for 0.5 s; stepping more often
  replayed the animation. A 5 s watchdog releases a stalled open.
- The bottom-left prompt is a CUIScene context-menu flag (set 0xB210, clear
  0xB230 on `*(0x408D1C)`). The native idle update picks flag 4 when
  treasure+0x73 bit 1 is set, otherwise 0x200; the host showed 0x84 with
  Ailish at E3_NB_ShdCamp (the hand "open" prompt). A first candidate always
  set 0x200: the client logged it, but the owner did not see the hand prompt.
  The host now sends the native choice (frame v10 byte 98: 1 = flag 4,
  2 = 0x200). The client's UI clears the flag (natively the usable re-sets
  it every frame), so the client re-asserts it while the hint is on. The
  prompt range is an approximation (3 units, about 70 degrees); natively it
  is the chest's trigger volume. Owner (build 93e4af07): "it stays on now".
- Owner result (build ce4fd823, E3_NB_ShdCamp next to the host): Tal opens
  the chest once, gets the loot (also copied by D6) and returns to idle; the
  host keeps control. Open: the remote player still walks through chests in a
  paused area (#82); one startup where the client refused frames after
  following the border warp (cause UNKNOWN, logs kept privately); doors (H1b).

### 2026-10-16 — H1b solo door entry: engine constraint (research, no code)

CONFIRMED_STATIC (exports and decompile, supported image): building doors
call `CWorld::EnterTemporaryZone(name, ResourceName)` (export RVA 0x64B0) and
`CWorld::ExitTemporaryZone()` (RVA 0x6710); the main world stays registered
(live 2026-08-21: Kamo, church). EnterTemporaryZone releases the current
zone's renderer, makes the temporary descriptor the world's current zone
(world+0x14 / state 4), stores the FRONT's position and inverted facing as
the return point (world+0x28..0x40) and pauses the front (+0x2B). The world
renders and simulates one current zone. A host whose own view stays outside
therefore cannot also make an interior current for a remote player, and
background loading is known to leave zones inert (TestStartZone finding).

Consequence for the owner decision "Tal goes in alone": any form needs a
change outside H1a's pattern:
- A. The client simulates its player inside its own loaded interior. The host
  parks that character outside; interior pickups and chests become client
  claims that the host validates. This changes the authority model (owner
  decision required by AGENTS.md).
- B. The native take-along: the whole party enters together (simplest; the
  host goes in too).
- C. The host's world goes into the interior with the remote player while the
  host's own character waits outside (the host player sees the interior).
- D. Decouple the host view and camera from the current zone (Phase G scale);
  UNKNOWN whether the renderer allows it at all.
Decision pending.

### 2026-10-16 — H1b research: a house can be resident while the host stays outside

Owner direction: expand the engine rather than accept the one-zone limit
("the engine was made for the original Xbox's memory"). Research path: zone
bookkeeping, what a background zone lacks, hard limits, then live tests.

CONFIRMED_LIVE (Ailish inside the Kamo shop, host memory): three zones were
resident at once: NewBrightwater state 3 (main, still loaded), LNBr_Kamo_shop
state 4 (current house; interior coordinates near the origin, e.g. 0.26,
0.09, -1.81) and WS_Country_SE_to_Bright_NE state 2. The native return point
outside the door was -18.8, -2.8, 340.1.

CONFIRMED_STATIC: `SwitchZoneNOW` looks the name up in the world's descriptor
table (houses included) and, for state 0, calls 0x5A70: it unloads the
previous background target (world+0x394) and its linked zones, then sets the
new one loading. The per-descriptor state setter is 0x10AD70 (new state in
EAX). The single background slot is bookkeeping, not a hard limit.

CONFIRMED_LIVE (research build 36573472, `[StoryAreas]
ResearchBackgroundZone=LNBr_Kamo_shop`, host outside): one SwitchZoneNOW
took the shop to state 1, then state 2 (resident) within 1 s and held there,
with NewBrightwater still current. Tal's host body, moved externally to
0.30, 0.40, -1.50, settled on the shop floor (y 0.10) within 0.4 s, so the
background house's floor collision is active. Within 0.8 s he was moved back
beside Ailish (16.04, 9.58, 371.8). A hardware watch on the repeated move
caught the writer: CPosition::SetPosition called from 0x110BD0 (parent-matrix
set) inside the movement/collision step 0x32320 (called per flagged body from
0x336B0), which integrates the movement body's own position (+0x20..0x28)
and writes it back. So it is not a party catch-up: INFERENCE, the external
write moved only CPosition, not the movement body.

Follow-up (research build 404b6a7a, `ResearchPlaceFollower=0.30,0.40,-1.50`,
host game thread, native setter 0x3050 once the shop was resident 2 s):
Tal settled at 0.30, 0.32, -1.50, then within 1 s stood at 16.76, 9.60,
372.77, about 2 units behind Ailish (16.12, 9.57, 370.81) where a follower
stands. So it is not a timing race. The movement component is actor+0x80 (vtable
0x2C8644). A two-phase hardware watch on its +0xAC (both the placement and
the return) hit the same writer, 0xC3200 (CONFIRMED_LIVE + STATIC): a
last-safe-position tracker. While the character stands on accepted ground it
copies CPosition into +0xAC..+0xB4. On other ground (a surface-type test via
actor+0x130 -> +0xF8 -> byte +2) it counts time in +0xB8, and past a limit
the character is put back at the recorded spot. A 50 ms poll in the
single-player stage then showed that the tracker ACCEPTS the house floor
(safe spot 0.30, 0.10, -1.50; timer 0; surface type 2; sector 0x0D01), and
that the character was moved about 0.4 s later to 17.00, 9.61, 373.00, about
2.3 units from the leader; the tracker only copied that. The mover is the
player-party formation snap in 0xB3690 (CONFIRMED_STATIC): with the global
flag 0x3207E1 set (export `AiPCFormationSetSnapEnabled(bool)`, RVA 0xB2C30),
a member in a follow AI state (0x0C/0x11), without blocking AI flags, whose
squared distance from the leader exceeds 900 (30 units) is teleported beside
the leader (0xF30A0 on member+0x98). Remote Tal 110 units away was not
snapped during the chest test (not in a follow state then).

CONFIRMED_LIVE (single-player stage, 2026-10-16): with the snap flag cleared,
Tal moved into the background-resident Kamo shop stayed on its floor
(0.30, 0.10, -1.50) for the whole 5 s sample while the host's current zone
remained NewBrightwater. The flag was restored afterwards.

So the host side of "go inside alone" is feasible with native pieces:
background-load the house (SwitchZoneNOW, state 2), keep formation snap off
while a remote player is inside (the native export; it also stops AI
companions catching up, which scripts already toggle), place the player at
the house's arrival point, and the reverse on exit. Open: walking around
inside under remote control; NPCs/objects of a non-current house; the client
side (Tal's own game loading and entering the house); exit; more than one
house at once; eviction of the background slot by outdoor streaming.

Client side (same run): moving Tal into a zone his own game does not have
loaded failed the client's roster invariant (`pause_invariant_failed
roster=0`) and stopped its presentation (the owner saw a freeze). Expected
for this experiment: a real feature must load the house on the client
before the host moves him. The research switches are off in the stage.
Not yet tested: the house's NPCs/scripts while not current; the client side
(Tal's own game entering the house); exit and return.

### 2026-10-16 — H1b: native house entry/exit arguments captured

CONFIRMED_LIVE (gdb on the host while Ailish left and re-entered the Kamo
shop): `CWorld::ExitTemporaryZone()` (RVA 0x6710, no arguments) and
`CWorld::EnterTemporaryZone("LNBr_Kamo_shop", marker)` (RVA 0x64B0); the
ResourceName marker carries id 0x8F6191ED = `NB_Kamo_Shop_Start_Pos` (the
name hash). Both are called from the SOL script VM (the authored interior
helper), not directly from the door trigger. The game data names doors and
markers systematically (`<town>_Kamo_Shop_Entry_Trig`, `…_Exit_Trig`,
`…_Start_Pos`, with NB_/CC_/CR_ town prefixes), so house doors can be mapped
by name. Next (client side): the client enters the house locally with the
same zone and marker when its own character goes in, while its presentation
accepts that character inside and the host's character outside.

### 2026-10-16 — H1b co-op research: host side holds; our sync layer blocks the client

Research builds 08c36c77 / cb8037f6 (switches: host `ResearchBackgroundZone`,
`ResearchPlaceFollower`, `ResearchSnapOff`; client `ResearchEnterTemporary=
LNBr_Kamo_shop,NB_Kamo_Shop_Start_Pos`), co-op pair:
- CONFIRMED_LIVE host: with formation snap off (native export), Tal stayed in
  the background-resident Kamo shop on its floor (y 0.08..0.10) while the
  host's current zone stayed NewBrightwater. The co-op host matches the
  single-player result.
- CONFIRMED_LIVE client: Tal's game stopped presenting as soon as the host
  background-loaded the shop. The host's scene observer counts the
  background SwitchZoneNOW as a zone change (`zone_begin method=1`, epoch
  +1). The client refuses frames from a new epoch until its own zone matches
  (`spectator state=5 native_presentation_changed`), and the research build
  had stood the client follower down. So the client never reached its own
  house entry. A first attempt was also refused by our own guard: the story
  observer owns an inline hook on EnterTemporaryZone, which the guard now
  accepts.
- Owner (earlier run): the host's camera stopped following Ailish while Tal
  was in the shop. UNKNOWN cause; INFERENCE: the host camera frames the party.

Design work before the next live test (our layer, not the engine):
1. The host scene epoch must not change for a background house load made
   for a remote player (or the client must accept that change without
   re-matching its own zone).
2. Client order: enter the house locally first, then accept host frames that
   put its character in that house while the host's character is outside
   (the reverse of the existing "host inside, client outside" foreign path).
3. Host camera: keep framing the host's own character, not the party.
4. Keep formation snap off only while a remote player is in a house; restore
   on exit.

### 2026-10-17 — H1b shop visits: Kamo on each player's own game, rejoin, dialogue scope

Dirty tree on 9f1d265; committed with this entry. Evidence per item:

- **Shop visits (D11).** Tal enters and leaves `LNBr_Kamo_shop` on his own; both players see each other inside; the host lead entering with Tal inside reloads the house without the hang (lead entry parks and rejoins the member). Door cue, zone banner and interior music on the client; the client camera eye equals the host's house camera. CONFIRMED_LIVE (owner and logs).
- **Kamo on Tal's game.** Talk prompt at the native reach (3.1); Tal's own shop with stock read from his verified script file, buying and selling with his own purse; Kamo's vanilla greeting menu (Shop / Chat / Goodbye, re-ask after the shop, farewell) run through CSpeech/UIDialog on the client. CONFIRMED_LIVE (owner: "it works"; log shows greeting → shop twice → chat → farewell → restore). Not yet: speech camera, Kamo turning to Tal, line gestures (S0, D12).
- **Gramophone examinable** on the client: music and text box. The guard that refused it read the string table's base-class vtable (0x2CA1B8); the constructed table is the derived class 0x2CA1B0 (CONFIRMED_STATIC + CONFIRMED_LIVE). Fixed; log shows the box and sound on Tal's game.
- **Host's own Kamo conversation kept off clients** (D11): the speech capture classifies the speaker by ResourceName and sends the idle record. CONFIRMED_TEST; live owner check open.
- **Rejoin at the spot the member stood** when the lead enters (held pose published in party and world frames, steering and actions frozen only while parked, restore 150 ms after the house is current). CONFIRMED_TEST; live owner check open.
- **Tal keeps steering** during the lead's door script into a background-resident shop (approach proof accepts a resident destination only with the lead-entry member contract) and during the lead's NPC conversation outside (settled visit member). CONFIRMED_TEST; live owner check open.
- **Interim dialogue scope** (until D12 S0): a host conversation is hidden from a player inside their own shop visit; the game's colour tags are decoded and drawn as coloured spans. CONFIRMED_TEST; owner on the live build: "text seems fine for now" (the widescreen span offset fix is in the next build).
- **Desk vibration at Kamo's counter**: native ramp slide plus collision push-out plus an idle root-motion step (CONFIRMED_LIVE probe of the host body; vanilla UNKNOWN). Tracked in #84.

Tests: full Wine suite 196–198 of 208 (the known non-pass set: probes, launcher GUI/tabs/mods fixtures, argument-taking image tests run separately and passing: StoryInteriorTravelTest, SolShopStockImageTest).

## 10. Remaining work (owner review 2026-10-15)

Against the original phases, with the #54 route (castle road ↔ New
Brightwater, Ailish host / Tal client) as the yardstick:

| Area | State | Next step |
|---|---|---|
| A groundwork, B protocol split, C remote control parity, D1–D5 enemies/drops | Accepted live (see §9 and the status matrix) | — |
| **D6 loot rule (D9)** | Ordinary pickups, active Items/Weapons refresh, farmhouse sword chest and Tal equip, buyer-only host shop purchases, Q self-use healing/control without host debit, and chest/ground florins to both purses once accepted live | M self-use healing/effects/refresh/control and zero-host-stock use also accepted; no chest repeat through Kamo travel. Five scoped owner checks have passed; full-unload/re-interaction/reconnect dedupe remains additional coverage. Selection preservation/More Info have automated evidence only. Client checkout still requires H. No persistent recovery yet (I). |
| E1 missiles, E2 allied guards, E3 Mungo | E2 partially seen in world frames; E1/E3 open | Research-first items; no owner gate yet |
| F combat state + spectating | Not started | Client cluster follow, fallen-character spectate |
| G lead vs ownership | Not started (death/lead hold still in place) | Decouple host controller/camera from the native lead |
| **H interactions (D4)** | H1 remote door/chest use open (#66/#67 follow-up); shops/talk host-only | Scoped front-actor swap at the interaction seams (0x40D330/0x40D6F0/0x40D9A0/0x40CCD0) |
| **I saves (D6)** | #72 filed, nothing built | Host→joiner save transfer with protections, then the rest of Phase I |
| J new-game co-op | Not started | — |
| Client menu follow-ups | Weapon preview stale after relayed equip; armour relay (#40/#74); F-keys unreachable on the client menu | Small adapter items |
| Void fall (#73), departure hold (#70), slope slide (#69), client camera walls (#71) | Issues filed | Owner priority |
| Bosses, full campaign, two-machine/native-Windows runs | Untested | Owner priority |

Owner rule restated for D6: **items picked up** (ground drops, enemy drops,
chests) are cloned to every player's inventory; **items a player buys** stay
with that player. Nothing else is shared.
