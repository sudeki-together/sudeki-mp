# Saved-story co-op (#54): research digests for the remaining work

Companion to `docs/coop-story-plan-54.md`. Each section below is the verified
research for one remaining item of the §10 chart: what the game does today,
the mechanism with its evidence level, the smallest correct design, the open
research gates, the decisions that belong to the project owner, dependencies,
and effort. The plan's chart links here; this file holds the detail so the
chart stays readable.

How these were produced (2026-10-18): one read-only researcher per item wrote
a draft; one adversarial reviewer per item tried to refute every claim against
the cited source, RVAs, plan sections, issues and private live logs; every
draft came back "needs fixes" (3–10 wrong claims, 5–9 overclaims and 7–12
missing points each); one fixer per item then re-verified each correction and
applied it, recording the points that stayed disputed. Nothing here was built
or run in the game: the strongest level any section reaches without saying
otherwise is CONFIRMED_STATIC or CONFIRMED_TEST, and `UNKNOWN` marks what the
repository cannot answer yet. Owner decisions are listed, never made.

Evidence vocabulary: `docs/evidence-index.md`. Line numbers refer to commit
9081425 unless a section says otherwise.

## Contents

- F1 — Client cluster follows the host (combat state)
- F2 — Spectator camera for downed players (D3)
- G — Lead vs ownership: drop the death/lead hold, keep controls and cameras off the native lead
- early — Early effects before control binding and the control handoff stall
- I — Saves beyond the #72 transfer
- J — New-game co-op (NG)
- Fkeys — Client menu: F-keys unreachable on the client
- skills75 — Client menu pages that change character state beyond weapons, armour and items (#75 skills)
- void73 — #73 Remote player falls through the void when the host streams his area out: warp to the leader instead
- hold70 — #70 departure hold and warning at host transitions
- paused82 — #82 Objects in areas the host is not in are paused; chests do not block the remote player
- doors67 — #67 remote door use, client PVS doorway, reveal-door alpha; #66 chest follow-ups
- interiors83 — #83 More than one interior in use at the same time
- H1rest — Farmhouse sword chest against D11; more shops in the door table
- S0priv — D12 S0: keep the host's conversations private beyond shopkeepers
- S1 — Story-progress mirror and quest notifications (D12 S1)
- S2 — Real conversation scripts on the client, shared writes blocked (D12 S2)
- S34 — D12 S3 flavour talk for everyone and S4 side quests by any player
- tutor80 — #80 clients spectate the trainee during tutorials (and the #79 remainder)
- birds81 — #81 Flock of birds frozen on the client: same-identifier scenery is left out of the world relay
- breakables — Breakables the host smashed still stand on the client
- xp65 — #65 XP award and distribution in co-op, and level-up of client characters
- bright59 — #59 intermittent overbright flashes on enemy surfaces
- gates — Special scripted barrier variants and the remaining barrier families
- receipt — D1 death receipts follow monster type and death style; no intermittent loss
- creep88 — #88 idle creep, static side: native writers and the anchor seam

### F1 — Client cluster follows the host (combat state)

**State today.** Not started (`docs/coop-story-plan-54.md:6796`). Since fc43d78 the host has published its cluster fields every 100 ms: `cluster_valid`, hash, committed sector cm+0x2C, `in_combat`, `flag36` and `suspended` (`src/hooks/lan_story_sigils.h:131-139`, `src/hooks/lan_story_runtime.c:3380-3397`). No client code reads them (`:3442-3452`). 199956d already removed the `cm+0x24` swap. Music, spawns, barriers, weapon draw and damage already come from the host; what remains is the client's own CClusterManager.

**What the game does (evidence).**
- The contained client runs the cluster update every frame: 0x435120 at 0x477E01, ahead of the held dispatch at 0x477E24 (CONFIRMED_STATIC). Held-dispatch counts reach about one million (CONFIRMED_LIVE). The comment at `src/hooks/lan_story_spawn_trace.c:341-348` is stale.
- 0x4352D0 has two callers with verified bytes: 0x43513B (skipped while cm+0x30 is non-zero) and 0x4CF655 (end of the door traverse) (CONFIRMED_STATIC).
- 0x4352D0 copies the leader's sector into cm+0x2E, resolves the cluster (0x509440), and recomputes the danger bits and cm+0x34 (0x4361F0, via 0x4748C0's three sources). It commits cm+0x24/+0x2C on a cluster or region change, then runs exit 0x435FE0 and entry 0x4360E0. A region byte of 0xCE–0xFE dereferences a null region (CONFIRMED_STATIC).
- Exit and entry broadcast through the registry (0x43F3B0) to the spawn-row handler 0x53D4D0. Its pause/resume calls are journalled by `src/hooks/lan_story_client.c:465-510`, which faults on any unjournalled entity. Both also call the listener list: count cm+0x10, data cm+0x18, connected by 0x5B9EF0 at 0x4230C7 (CONFIRMED_STATIC).
- missiles.xsb load/unload and listener vtbl+0xC fire when the current cluster's +0x44 bit 3 changes. The comparison uses locals, not cm+0x34 (CONFIRMED_STATIC).
- Zone teardown 0x507A10 zeroes cm+0x24 and sets cm+0x2E=0xFFFF but leaves cm+0x28. Reset 0x435100 clears cm+0x20/+0x30/+0x34. The GEL exports 0x4358C0/0x435C70/0x435AA0/0x435E20 broadcast outside this seam (CONFIRMED_STATIC).
- Live, the client is on Town_Cluster in about 251k of 260k samples (hash 0 otherwise); cm+0x34 and cm+0x30 stay 0. The host visits at least 16 clusters, fights in about 66 logs and is suspended in about 2.8k samples. No log contains a `cluster_script_held` line (CONFIRMED_LIVE, private live logs, conclusions only).

**Smallest correct design.**
1. Fix the comment. Delete the unreferenced `cluster_update()`, `cluster_entry`, `spawn_trace_cluster_fn` and `find_cluster()`.
2. One default-off adapter in `lan_story_client.c` owns rel32 hooks at 0x43513B and 0x4CF655 (the second only as a safety net), installed after the existing holds. Contracts: the byte windows, the bodies of 0x4352D0/0x4361F0/0x509440/0x435FE0/0x4360E0, and vtable slot 0x6C7B3C. It refuses to install while the zone-trace entry hook is present.
3. Feed the host fields as plain data from the `SudekiMpLanPartyGetStoryEffects` block. Missing or stale data is unknown.
4. The bridge (stdcall, RET 4, AL) passes the call through unless it is on the client, `trigger_held` is set, `trigger_fault` is clear, it runs on the game thread, and cm and the group are exact.
5. Return AL=0 without calling when host data is stale, invalid or suspended, the zone differs, or nothing resolves.
6. Resolve with `zone()`/`cluster_exact()`/`cluster_for_sector()` (`src/hooks/lan_story_activity.c:84-131`) plus a host-hash match. Keep the region < 0xCE check.
7. Per call:
   - Swap the leader sector at member+0x40.
   - Save cm+0x10/+0x14/+0x18 and data[0], then write a count of 0.
   - Call the native function.
   - Restore only what is unchanged; otherwise set a sticky fault.
   - Write cm+0x34 from the host.
8. Fallback: a passive mirror that clears cm+0x28 itself before the follow-unload.
9. Restore the hooks before the pause release; quarantine a failed restore.

**Open gates.**
- G1: nothing inside the call touches member+0x40. 0x4015B0/0x4015E0 and 0x509440 are clear; 0x507DC0, 0x487A10 and the registry handlers remain (Ghidra).
- G2: no listener connect or disconnect runs inside the call. At count 0 a connect overwrites data[0] and a disconnect leaves a dangling entry. Close with a call graph; if it fails, use the fallback.
- Journal: every row that 0x53D4D0 reaches is in `entity_pause[]`. Otherwise hold the broadcast at 0x436074/0x436177.
- G3: name the listener classes at 0x4CCBF0 and spawn-group +0x148 (Ghidra).
- G4: add cm+0x20 to the r54 sample. cm+0x35 is already in raw24 and mostly 1.
- G5/G6: find which client paths reach the cm+0x34 readers, and check whether the client's spawn-point and row bits match the host's.
- G7: check missiles.xsb reference counts against the E1 shot-audio executor (`src/hooks/lan_story_character_audio.c:856-887`).
- G8: a counter for whether the client reaches 0x4CF620.
- Owner gate: castle-road ambush, sampled faster than 1 s.

**Owner decisions.**
- Whether F1 is wanted, and at what priority.
- Which combat presentation, if any, the client gets (native 0x424480 also arms and cures).
- Whether interactions on the client's own game are refused while the host fights.
- Which sector drives the client when its character is in another cluster of the host's zone.
- Teardown: resume the native cluster scripts or keep holding them.
- The switch default for the first gate.

**Depends on.** lan_story_client containment; host-driven spawns, barriers and music (the listeners stay muted); E1 shot audio (same bank); H, D11 and D12 (cm+0x34 readers, client-run scripts). G may redefine the host sector (UNKNOWN). F2 is independent.

**Route relevance and effort.** Low to moderate: F1 gives Phase F's metric, correct combat gating on the client's own game, and the first live run of the held scripts. No proven route step needs it. Medium effort with no protocol work; the risk is in G1, G2 and the journal.

### F2 — Spectator camera for downed players (D3)

**State today.** Nothing detects a downed member or moves a downed player's camera. The only spectator code serves seats whose character is not in the party yet; they show the host's view (`src/network/lan_story_frame.h:40-46`, `src/hooks/lan_story_menu.c:354-361`). No source tests the downed bit. Private live logs, conclusions only, CONFIRMED_LIVE: when the host lead Ailish went down, the death hold kept her in slot 0 for 136 s, and she came back at 1 HP at combat end. When the client's Tal went down while Ailish was alive (pre-#71 build), Tal came back at 1 HP about 46 s later, as the last monster died. Neither camera was recorded; staying on the downed body is INFERENCE. With everyone down, game over follows in about 1 s; the client returns to the title (older builds).

**What the game does (evidence).**
- The downed state is ConcussedStatusEffect (vtable 0x6CBF40). Enter (0x4C0900) sets arbiter+0x50 bit 0x10000. Update (0x4C0AB0) re-calls 0x424160 only for the lead and never expires. Remove (0x4C0B10) sets HitPoints (CStats+0x2C) to 1.0, then clears the bit at 0x4C0C54. CONFIRMED_STATIC; the 1.0 matches the live revives.
- In 0x4D31C0 a party victim in the group still counts itself alive (0x424310), so it is knocked out at 0x4D328A (CONFIRMED_STATIC; "always" is INFERENCE). For party members, 0x4D3530 sets 0x8000 only when nobody is alive and *(0x808DD0)+0xA18 bit 0 is set, which raises GameOverBad via 0x424220. 0x4DA8A0 also sets 0x8000 (CONFIRMED_STATIC; the writer search is incomplete).
- 0x424160 hands off the lead (0x4241C9) only when the lead goes down, then moves the arbiter+0x60 0x4 flag to the new lead. The host death hold skips that hand-off (`src/hooks/lan_story_death_hold.c:11-30,63-80`; `src/hooks/lan_story_runtime.c:1889-1903,3699,3716`), so a downed lead re-applies the flag to itself about 4 times a second (CONFIRMED_STATIC, CONFIRMED_LIVE).
- Every known revive ends in Remove: through CureConcussedPCs 0x424DA0 (called by 0x424480 at combat end, INFERENCE plus live timing; 0x424530; the host lead's door use 0x4CEDE0 → 0x4CEFB0), or directly from the export 0x4BDC10 (CONFIRMED_STATIC).
- Only 0x42A370 makes the camera follow a new lead. It creates the target with 0x535040 on *(0x7C2F30)+0x4C and adds a reference before each 0x4E84C0 install (slot 1, then slot 0). It drops the creation reference at 0x42A777. Zone change 0x4379B0, 0x436A30, 0x4298C0 and 0x437170 also write the slots (CONFIRMED_STATIC).

**Smallest correct design.**
1. Host: prove each member's arbiter (vtable 0x6CC9AC, +0x10 == actor). Publish each member as alive, downed, dead or unknown (protocol bump). Unknown never counts as alive.
2. Pure state machine: start on the own body, cycle with Next/Prev over D3's members, hold on unknown, return on revive.
3. Client adapter: the sole LAN-story writer of the "default" slots, below every existing override. Create the target on the 0x7C2F30 list, verify vtable 0x6D42CC and +0x20, retain the originals, install slot 1 then slot 0, drop the creation reference, then Reseat (`src/hooks/lan_story_client_camera.c:166-180`).
4. Exit: restore the originals only if our target is still installed, otherwise release our own references. On a failed restore, retain the module and retry.
5. Widen `camera_owner`/`camera_ready` (`src/hooks/lan_story_local_control.c:380-409,425-432`) under a lease. Add a Prev/Next edge and an input mute (`src/hooks/lan_story_input.h:29-33,54-56`).
6. Host mode for a downed lead held in slot 0, cycled from the Prev/Next stub the host already owns (`src/hooks/lan_story_control.c:62-68,214-215`). A separate guard holds forwarded input for a downed remote body; STORY_BODY_BUSY_FLAGS (`:45`) stays unchanged.
7. Pure, codec and exact-image tests; then an owner gate.

**Open gates.**
- 0x535040 ABI and reference count: Wine exact-image test.
- *(0x7C2F30) versus the 0x809D7C manager: find the writers of both globals.
- Is slot 0 an OffsetTarget after Reseat? Research54 slot dump.
- 0xA18 flag and 0x424530 trigger: static callers.
- Do Prev/Next presses arrive while downed? Input trace.
- Current-build view, a far target and game over: owner-run captures.

**Owner decisions.**
- Cycle key and any hint.
- Start view: own body or a living member.
- Host half now, or inside G.
- A downed field, or HP 0 as a proxy (HP is truncated, and a revive writes exactly 1.0).
- Members in other areas or houses (D11); precedence against #80.
- Whether spectators see cast cameras.
- Client behaviour at game over (D6, Phase I).

**Depends on.** G (host camera; enemies idle while the downed lead is held), the #71 client camera, a protocol bump, #80 and J/NG spectate, the door and remote-interior modules (a host door use revives everyone; zone changes clear targets), H1b/D11, and #57's downed-party test (UNKNOWN).

**Route relevance and effort.** Medium. Tal plays alone until Ailish joins (Mungo first is INFERENCE, plan §10 E3); after that any knockout triggers this, and D3 requires it. The client half is medium effort: one protocol field, a state machine, one retarget adapter and one input edge. The host half is small under the death hold and large inside G.

### G — Lead vs ownership: drop the death/lead hold, keep controls and cameras off the native lead

**State today.** Not started (plan §10). The only lead policy is the hold in `src/hooks/lan_story_death_hold.c`. It is installed on both seats (`lan_story_runtime.c:3699`), but it is armed only on the host, and only while a client is observing (`lan_story_runtime.c:1882`, `1889-1902`). While armed, it skips the Previous rotation at the down and death sites (0x4241C9, 0x42428C). It also skips the scripted pending-lead rotation at 0x4231E4 when the pending lead is a remote player's actor. It does not cover the lead-removal rotation at 0x423433. The pad switches are blocked separately (`lan_story_control.c:29`, `62-68`). No test covers the hold. All saved-story modules assume host = native lead = controller target.

The private live logs (conclusions only) show no host lead change and no hold event in this run set. The client's only lead change is its own selection switch (CONFIRMED_LIVE). The #54 runs of 2026-10-09 came before the hold and show what a native lead change does: control moved to Tal, the observer lost the party, the host went to first person, and run 1 crashed (CONFIRMED_LIVE).

**What the game does (evidence).**
- Every lead change goes through 0x4237B0(group, old, new). Its callers are 0x4232F4, 0x423FFD and 0x4240FD, and they ignore its result. It writes controller+0x88 and +0x248, the auto-pickup front, the action states, the arbiters, the targeter, the AI modes (through 0x4EF700) and the camera (through 0x42A370). It writes nothing in three cases: the new front is NULL, it has the same identity as the old one, or 0x423750 refuses it. CONFIRMED_STATIC.
- When it rotates onto a leased actor, 0x4EF700 puts the host's character in AI mode 1 and writes controller+0x84 = 0 through 0x4295B0. The write is CONFIRMED_STATIC; that it turns the pad off is INFERENCE.
- The group update retries Next every frame until slot 0 holds the pending lead at group+0xC0 (0x4231CC-0x423224). Retail does this too. CONFIRMED_STATIC.
- Every story lease release requires controller+0x248 to be the recorded host (`control_separation.c:8025-8063`, `8555-8572`). So no lease can be released while the controller target is on another character. CONFIRMED_STATIC; #54 run 1 matches.
- Both enemy "lead" selectors read slot 0: 0x5B6B10, called at 0x5B7598 and 0x5B791B. CONFIRMED_STATIC. That enemies retarget the new lead after a native KO rotation is INFERENCE.
- Five functions install camera targets through 0x4E84C0, including the cinematic paths 0x436A30, 0x4379B0 and 0x437170. CONFIRMED_STATIC. So it is only INFERENCE that skipping 0x42A370 keeps the host camera on the host's character.
- Several systems key on slot 0: triggers, interactions, the cluster, lead-only mode, the UI move lock 0x40AFD0 (20 call sites), the skill path 0x427BF0, lead-only XP and house entry. CONFIRMED_STATIC.

**Smallest correct design.**
1. Split the model. Add host_character next to leader_character. Key these on host_character: the observer, the leases, the host binding (`lan_story_host_control.c:126-142`, `347-366`, `420-425`), the client fences (`lan_story_runtime.c:2413-2416`, `2471-2479`) and the host-local modules. Pure tests cover leader ≠ host.
2. Add one adapter on the entry of 0x4237B0. It checks the 13 entry bytes and the `ret 0xC` exits, and installs transactionally. It passes through to native code in five cases: a single player, old = NULL, inside our own `call_next` scopes, cases where native code writes nothing anyway, and unproved identity. Otherwise it runs a lead-only variant that writes nothing. An image test covers it.
3. Retire the three hold hooks in one transaction. Guard the move-lock transfer at 0x4241D4.
4. Add per-player guards for the UI lock, the skill path, interactions and lead-only mode by extending their existing owners.
5. If the owner wants a lead return, add it through the existing `call_next` (`lan_story_host_control.c:317`).
Never hook inside the hashed bodies of 0x423F60/0x424060.

**Open gates.**
- Classify the 334 slot-0 references and the 69 controller references. Also settle the meaning of controller+0x88 and identify the listeners. Static work.
- Trace what re-installs the follow camera after a cinematic, and why the host went to first person in #54. Static work first, then a live read of the camera target slots.
- Find whether KO or scripted rotations can fire on the client, with a client probe at the four call sites.
- In single player, run a lead-only rotation with Ailish down. Expected: slot 0 = Tal, the controller stays on Ailish, the camera is unchanged, and enemies keep fighting.
- Time the scripted lead change at the ambush and in the tutorial, with a hardware watch on group+0xC0.

**Owner decisions.**
- Lead return: does the lead go back to the host's character, and when?
- Who may start story houses and scenes, since D11/D12 say "host lead"?
- #80 when Ailish is a client's character.
- Lead-only rewards.
- Which lead the AI companions follow.
- What happens when a client disconnects while leading, and which lead a save stores.
- Whether to send a separate host seat and lead seat on the wire.
- Whether F2 comes first.
- HUD order.
- Whether pad switching stays blocked.

**Depends on.** C (accepted), F2, F1, H1a/H1b, I, J, #80.

**Route relevance and effort.** Medium-high today, high for J. On the J route Ailish's tutorial sets her as lead, and today's hold would stop her from becoming lead (INFERENCE from the #80 research). Mungo comes before Ailish joins, so it does not depend on G. Effort is large. The adapter itself is medium; the model split and the per-player guards carry the risk (plan §7 rates G high risk). At least three owner gates.

### early — Early effects before control binding and the control handoff stall

**State today.** Nothing for this item is built. At HEAD 9081425 the host publishes no saved-story VFX (party, Spirit, world, missile and drop effects) until `bind_casters` succeeds. That bind waits for a READY remote control slot, and READY needs the client's ACK. In the 2026-10-18 castle-ambush run (Ailish host, Tal client) the client waited about 295 s on `pending_input_arm`, and the host published no effects for about 305 s. Likely cause: the story input adapter takes its window from `GetActiveWindow()`, and the client window was not active (INFERENCE). Live evidence: private live logs, conclusions only.

**What the game does (evidence).**
- Publication needs an area capture, `cast_attempted` and `CaptureVisuals` (bound, visuals owned, exact roster). The bind needs a ready, non-draining slot, a known roster, `host_binding_ready`, story policy and an idle swap. CONFIRMED_STATIC: `src/hooks/lan_story_runtime.c:240-251,2117-2133,2011-2016,3371-3377`, `src/hooks/lan_story_cast.c:328-340`.
- `pending_input_arm` with local control ready leaves two failing checks in `SudekiMpLanStoryInputArm`: `ensure_focus_hook()`, or the first-arm scale check (`controller+0x1D0` bit 0, `+0x1B8` outside 0..4). CONFIRMED_STATIC: `lan_story_runtime.c:2603-2620`, `src/hooks/lan_story_local_control.c:105-120,318-323`, `src/hooks/lan_story_input.c:305-320`. Which one failed is INFERENCE. Activation clears both: WM_SETFOCUS (RVA 0x28D6F9) and WM_KILLFOCUS (RVA 0x28D753) reach the retail reset RVA 0x276C0 through RVA 0x28C432 (`src/hooks/lan_arena_window_policy.c:166`). That reset writes `+0x1D0=0xF` and `+0x1B8=2.0f` (CONFIRMED_STATIC).
- `ensure_focus_hook()` installs only when `GetActiveWindow()` belongs to the native thread and process. Once installed it checks window identity only and never reinstalls (CONFIRMED_STATIC, `lan_story_input.c:148-157,281-296`). That Wine clears the active window when X focus leaves is INFERENCE.
- In that run the adapter installed after the placement script left a non-game window focused, and one desktop sample during the stall agreed. The first admitted attack came 75 ms after the ACK (CONFIRMED_LIVE timing; the owner's click as the cause is INFERENCE).
- In 20 of 39 initial handoffs from 2026-10-16 to 10-18 the client waited over 5 s on this reason, up to 618 s. Eleven ended with an attack press 25–181 ms after the ACK. A same-day relaunch armed in 27 ms with no press and no known activation (CONFIRMED_LIVE, mixed revisions; that run's cause is UNKNOWN).
- Before the bind the host fired 109 party shots and the client got no VFX packet; the bind came about 107 s into a 176 s ambush (CONFIRMED_LIVE). Casts drain only on a party swap or teardown (CONFIRMED_STATIC, `lan_story_runtime.c:1593-1603,1724,3232`).

**Smallest correct design.**
1. Step 0, diagnostic: log once per change which InputArm sub-check failed.
2. Step 1, in the one input owner (`lan_story_input.c:148-157`): take the window from the D3D9 device (RVA 0x3C31DC, `hFocusWindow`, as at `src/ui/title_menu_view.c:335,543`). Keep `window_exact`, the focus tracking and the uninstall order. An unfocused client arms but samples zero and admits nothing (`lan_story_input.c:223-233,329`). Add a re-validation rule and a log line; no new detour. Test: `tests/story_quick_input_test.c`.
3. Step 2 (owner-approved, later): also bind for a watching client (OBSERVING, transport confirmed, catch-up complete, not draining). Keep every existing condition and guard, and the READY gate on remote actions (`lan_story_runtime.c:2189-2197`). Fixture: `tests/story_cast_admission_test.c`.

**Open gates.**
- G1: name the failed predicate with the step 0 log or an owner-approved memory read. A title-bar click cannot tell the two apart. Never fight the owner for focus or move focus straight between the games.
- G2: pin the window-identity contract. The field at RVA 0x34E6A0 belongs to a structure with three writers, and its lifetime is UNKNOWN.
- G3 (the owner's required check): find whether host-AI skill or Spirit use reaches the routed entries. Those entries reject callers without authority (`src/engine/skill_activation_abi.c:106`, `lan_story_cast.c:147-152,198,212`).
- G4: measure the `all_idle()` deferral; a non-deferred bind failure closes admission until the next swap or teardown (`lan_story_cast.c:538-563`). G5: bind cost. G6: other writers of `+0x1B8`. G7: the 27 ms relaunch.

**Owner decisions.**
- Step 1 makes a client READY before its window is clicked. Approve, or keep requiring one activation?
- Should the activating click remain Tal's first attack?
- Step 2: is it still wanted now that Step 1 covers the stall, should it come earlier for the NG route, and what is its spectator scope (NG, D3, #80)?
- When a READY client is unfocused, does its character stand or fall back to follower AI (D2/D8)?
- Whether the placement script activates the client, whether to build a diagnostic DLL, and recording this in §9.

**Depends on.** #57 and the shots work, E1/H world effects (same gate), J and F (Step 2), #80, #36. `lan_arena_window_policy.c` owns the focus seams. BackgroundAudio patches separate bytes in the WM_KILLFOCUS branch.

**Route relevance and effort.** High on both routes. Step 1 should remove the current-route wait if G1 confirms the focus hook (HYPOTHESIS). Binding-change drains still interrupt control. A seat-locked NG spectator never reaches READY, so the acceptance route needs Step 2. Effort: Step 1 small, Step 2 small to medium (mostly G3), the stage split large.

### I — Saves beyond the #72 transfer

**State today.** Only the #72 join transfer exists. A joiner without the host's save receives the hash-checked pair over the lobby (version 12) into a marker-tagged `SAVESLOT9900..9999` folder (`src/network/story_save_transfer.c:355-392`). CONFIRMED_TEST plus one automated self-run; the private live logs (conclusions only) show that one transfer and no other. Not built: re-send on host save, the client save guard, both prompts, the launcher button, uninstall cleanup, the one-copy rule, per-player sidecars. Loot pending at session end is dropped (`src/hooks/lan_story_runtime.c:3104-3105`).

**What the game does (evidence).**
- The writer RVA 0xFF030 (stdcall, RET 4, EAX 0 on success) has one caller, VA 0x481757 in the write step 0x81680, which ignores its status (CONFIRMED_STATIC, byte scan). Status 0x27 deletes the new folder (VA 0x4FFDE2); 0xE leaves it empty.
- A save creates the lowest free `SAVESLOT%04d` (0xFE210). An overwrite first deletes the selected folder in the confirm handler 0x84560 (VA 0x4845DE; delete-only at 0x484612), so its number can be reused (CONFIRMED_STATIC; reuse INFERENCE).
- 0x84560 serves every save-page mode: 0 title Load, 2 in-game save (opened by event 0x1B), 3 quit menu (VA 0x49FA26, 0x49E05D, 0x563D0B). The mode 2 destructor frees the native save list through 0xFEFC0 (VA 0x4804BA), so 0xFEFC0 marks a close, not a save (CONFIRMED_STATIC): RETIRED as the re-send seam.
- `SudekiMpLanStoryLoadPrepare` never invalidates that list; the only invalidation follows a title import (`src/hooks/lan_story_load.c:386-450`, `src/ui/title_lobby.c:733`). The insert 0x101280 fills the new row only when rows already exist (VA 0x5012E5). CONFIRMED_STATIC.
- No save hook runs in saved_story: the story LoadGameSave stage's only caller is the uninstalled area-intent adapter (`src/hooks/lan_story_area_intent.c:261`), the vote is default off (`src/loader/dllmain.c:1602`), and `src/hooks/lan_arena_campaign_guard.c` serves only the party and arena runtimes. CONFIRMED_STATIC.
- STATE has five free bytes (`src/network/title_lobby.c:452`); the lobby UI runs on the game thread (`src/hooks/title_multiplayer.c:693,739`). CONFIRMED_STATIC.
- Expiry runs once per process that opens the multiplayer page, for that profile only (`src/ui/title_lobby.c:711-718,787`); the same-machine client profile is refreshed from the real save folder and never pruned (`src/launcher/beta_launcher.c:792-834`). CONFIRMED_STATIC.
- SubtractMoney (0x21CB0) has no floor; AddMoney (0x21C50) may show a popup. CONFIRMED_STATIC.

**Smallest correct design.**
1. One owner: a saved_story startup registration in `src/hooks/save_book_intercept.c`, shared with its LoadGameSave stage and installed beside `SudekiMpInstallLanStoryLoad` (`src/loader/dllmain.c:1294`). It patches the 0x84F10 entry and the calls at 0x81757, 0x845DE and 0x84612 after exact byte and rel32 checks, transactionally, refusing if another detour holds 0x84F10.
2. Client guard: in an active session with the role client or unknown, skip SaveMenuShow with a notice, return nonzero from the writer call, skip both deletes. With no session, pass through.
3. Host re-send: on EAX 0, find the new folder by write time inside the call, cross-checked with the list row when present; a new worker hashes the pair; a new SESSION_SAVE frame carries revision, folder and hashes.
4. Client receive: request each revision, import through a file worker into a fresh reserved folder, then remove that session's older copy (never zero). Invalidate the native list at the title before a rejoin.
5. A marker state machine (session, state, owner process, times), both prompts in the title lobby, and a launcher clear for marked folders in both profiles.
6. Sidecars only after O4, O5, O11 and O13, with SubtractMoney clamped.

**Open gates.**
- A blocked SaveMenuShow must not hang its script: a live client probe.
- Writer thread, statuses and folder identification, overwrite included: a logged host save and overwrite on a test prefix.
- A second extra file in a save folder: one live list and load.
- Client behaviour on host loss, which places the disconnect prompt: static trace, then live.
- Native reload and game over in co-op (D3); a co-op host save in single player.
- Inventory primitive contracts and item categories: Wine image tests.
- The #72 owner gate, including its disconnect prompt.

**Owner decisions.**
- Where and when each prompt appears; what Yes keeps; what "co-op slot" means.
- What "only one temp copy" means; the expiry clock.
- Uninstall cleanup without an uninstaller; whether `SAVESLOT99xx` is D6's temp area and stays visible.
- The breaking lobby change for session saves.
- Sidecar semantics, categories and location (Phase I has them travel with the save; #72 allows only the two records).
- The client's blocked-save notice; whether the guard covers the quit-menu Load page.

**Depends on.** H save books, D9 grant serials, D11 shops, D12 S1, J (a new game's first host save is the only rejoin source), D3/F/G reloads, the lobby and launcher.

**Route relevance and effort.** Low to medium. J is played in one sitting, so only the guard matters directly; re-send matters once a client rejoins. The #72 gate needs the disconnect prompt. Effort is large but separable: guard small to medium; re-send, receive, state machine and prompts medium; launcher small; sidecars large.

### J — New-game co-op (NG)

**State today.** Not started (§10, `docs/coop-story-plan-54.md:6800`). The lobby has no new-game destination (`src/network/title_lobby.h:16`, `:35-36`). A multiplayer New Game is rewritten to `testroom.zone` (`src/hooks/lobby_gameplay.c:103-116`), and every story readiness proof needs the saved-load roots (`src/hooks/lan_story_runtime.c:486-494`). Reusable: spectator seats and the closed lighthouse recruit (last live proof 2026-10-03, older revision).

**What the game does (evidence).**

- Front end, CONFIRMED_STATIC: 0x4A1900 only stores the selection and submits `FrontEndTimeoutLoopStop` (0x4A1928). `SimpleFrontEndMenuSelect` follows at 0x4A0FED and reaches `StartNewGame` through `GEL_ExecuteFunction` 0x425E70 → 0x425EA0 → 0x5C37B0 (0x425F03). Whether StartNewGame is a root or a child: UNKNOWN.
- GEX hashes are FNV-1a over the lowercased name, CONFIRMED_STATIC (StartNewGame 0x96D5F775, StartLoadedGame|S 0xC04DA6A9 = `HASH_START`, `lan_story_task_trace.c:26`).
- StartNewGame, CONFIRMED_STATIC: ForceFullyFaded, ClearPersistantDataRegion (0x4FEAC0, root manager +0x14; saves use +0x18, `lan_story_load.c:117`), StartGame('illuminacastle'), OnGameStart. StartTestGame also clears that folder on every Test Room start.
- Castle OnMain 0xB83A752C, submitted at zone state 3 (0x50B0E8). Behind IsStoryBlockBefore(1000) it runs AddCamera('default'), Tal's party add, RunTalTutorial, IC_GameIntroTSA (fade-in, movie) and SetStoryBlock(1000); otherwise it runs UpdateWaypoint. Calls CONFIRMED_STATIC, branches INFERENCE.
- Intro movie fma02_ic, 224.8 s, CONFIRMED_STATIC. PlayMovieSkippable reaches the 0x504D90 hook (CONFIRMED_LIVE, private live logs, conclusions only). That retail PlayMovie blocks the host's game thread for its full length is INFERENCE.
- Lobby start, CONFIRMED_STATIC: it aborts when it is not departure-safe 90 s after its last phase change (`src/network/title_lobby.c:715-716`).
- Recruit at block 1300/1400: the town cluster spawns NPC_Ailish and LighthouseTSA turns her into PC 3, CONFIRMED_STATIC. The closed LAN route needs that NPC on the client (`lan_story_recruit.c:418-435`). On an NG client it never exists (held cluster scripts, ally/monster-only spawn follow): INFERENCE.
- Route end, CONFIRMED_STATIC: ICExt_TalAilishHome (block 2000) plays FMA04A (156 s) and runs RemovePC(Ailish).

**Smallest correct design.**

1. Lobby: a NEWGAME destination (SLB1 v13) behind a default-off switch. Fixed Tal-only preview, host seat Tal, the client locked to a `(later)` character.
2. `lobby_gameplay` third mode: pass `StartNewGame` through and install the story runtime without a save. Widen every `saved_game` branch (`:66`, `:106`, `:271-279`, `:292`, `:303-305`, `:321`, `:356-377`).
3. NG generation in the task journal. Either a new call-site seam at 0x425E7C or 0x425F03, or (HYPOTHESIS) the existing create observers binding hash 0x96D5F775. Five consumers accept the NG root.
4. Client containment. Admission (`lan_story_task_trace.h:16-49`) WAITs castle OnMain from NG dispatch, then the full pause. Every root before the pause is classified, so nothing untracked is quarantined. The reveal is explicit, because the native fade-in sits in the held script.
5. An observe-only movie callback on the existing 0x504D90 owner publishes a movie event (B1 bump). The host intro waits for departure-safe, unless the owner changes the timers.
6. Recruit: one exact NPC entry in the D2/E2 follow (`story_spawned_actor.h:13-16`) through 0x4762B0. The closed route then runs unchanged.
7. Checkpoints C0–C5 (castle, cinematic, zones, recruit, return, member leaving). Mismatch: hold and log, never force.

**Open gates.**

- R1: StartNewGame task shape and StartGame binding. Close with Ghidra or a host-only read-only task log.
- R13: PlayMovie binding. The skip hook's passthrough line logs name and skippable flag.
- R14: census of the roots between NG dispatch and the pause.
- R15: does native quit or a zone unload retire a held OnMain? Detach refuses a pending wait (`lan_story_task_trace.h:45-49`). Close with an exact-image test plus a live quit and retry.
- R5: is the D6/#72 folder clear of +0x14?
- R6: host movie against the 10 s lease and the 90 s lobby timers.
- R9: does castle re-entry replay the opening on a client below block 1000?
- R12: regression of the lighthouse route from a pre-recruit save on current code.
- R3, R4, R7, R8, R10, R11: hold-point state, intro groups, NPC census, story filter, cameras, #80 case 2.

**Owner decisions.**

- SLB1 v13 (breaking) and the default-off switch.
- Host fixed to Tal.
- Client intro movie: own copy, hold card or skip.
- Intro NPCs.
- Recruit variant (replicate the NPC, or spawn PC_AILISH directly).
- D12 S1 mechanism and timing for the client's story block.
- E3 test access.
- Interim rule for Ailish's tutorial.
- Spectator HUD and camera, and Buki/Elco seats.
- Rejoin before the first host save.
- Seat after Ailish's RemovePC, and FMA04A.
- Hold the intro or change the lobby timers.
- Owner of the new submit seam.

**Depends on.** B1 presentation message, D2/E2 spawn follow, E1–E3, F2, G, #80, #87 (no Tal-host / Ailish-client acceptance yet; status matrix rows 87 and 99), I/#72, D12 S1, #42/A6.

**Route relevance and effort.** J is the route's own starting condition (`docs/coop-story-plan-54.md:72-75`), and with a pre-Mungo save one of only two ways to reach Mungo. The client spectates up to the lighthouse, then plays Ailish through her tutorial and the ambush until RemovePC at the castle. Large: 16 research gates, three staged owner gates, hard dependencies on E3, F2, #80/G and #87.

### Fkeys — Client menu: F-keys unreachable on the client

**State today.** HEAD 9081425, clean tree. On the client's native Main Menu, M, Esc and F3 come only from the mod's key poll (`src/hooks/lan_story_main_menu.c:447-474`). F1, F2 and F4 never reach it. The 2026-10-15 note (plan §9, `:5009-5011`) blamed the swallowed gameplay listener, which is true only for M.

**What the game does (evidence).**
- The UI listener (CUIScene, RVA 0xA190) maps ac_PrevChar/ac_NextChar (F1/F2 in the test configuration) to menu commands 4/5, and ac_GUI_Cmd0/Cmd1 (F3/F4) to 10/11. CONFIRMED_STATIC.
- The filter at RVA 0x9FD0 drops commands 1-5, 10 and 11 unless scene +0x8D, +0x8E or +0x61 is set (VA 0x40A070). 0x40AFD0 writes +0x8D, and the menu open calls it at RVA 0x956F9. The client skips that call (`lan_story_main_menu.c:63-69`, `:361-362`), and its install line reads `menu_pause=skipped`. So the client loses Space, Numpad0, Esc/B and F1-F4, but keeps Enter, the arrows and keys 1-4. The mechanism is CONFIRMED_STATIC; that it is the live cause is INFERENCE.
- Live on the client: M/Esc back, F3 More Info (on 2026-10-15 builds) and the Enter equip are CONFIRMED_LIVE (private live logs, conclusions only; #75). Keys 1-4 are CONFIRMED_STATIC only.
- F1/F2 change the character only through the two wrapped calls 0x93B24/0x93B47 into 0x96300. Weapons/armour choice (0x16F110), retail Armour and Advancement (state 3) forward them to the Main Menu (ESI = *(0x7C2F78), call 0x51F810). CONFIRMED_STATIC.
- With ArmourChoice on (the default), the Armor icon builds the Weapons layer, so retail Armour (0x167360) is unreachable (`armour_menu.c:15-17`, `:519-520`). The adapter is installed in every retained run.
- On Items, F4 sorts the consumables in browse state 0 (0x421F60) and acts as Back in states 3 and 4. F3 in state 0 switches mode, likely to the description (INFERENCE).
- The host gate refuses only the characters of OBSERVING peers (`lan_story_runtime.c:2280-2292`). CONFIRMED_STATIC. In host logs from 2026-10-16 on, every change to Tal's armour was his own EQUIP_ARMOUR request. Logs from 2026-10-14/15 show the gate skipping host cycles. CONFIRMED_LIVE.
- The mod's F1 story swap (`lan_story_runtime.c:1740-1788`, called at `:3313`) is not suppressed under native menus. CONFIRMED_STATIC; that it runs during the client's menu is INFERENCE.
- Native gameplay F1/F2 cannot fire on either seat. The client's input wrapper never calls 0x277B0 (`lan_story_input.c:195-262`), and the host turns the switch calls 0x28464/0x28480 into a bare return (`lan_story_control.c:62-68`). CONFIRMED_STATIC.

**Smallest correct design.**
1. No new hook. Extend `SudekiMpLanStoryMainMenuPollKeys` after the prompt and suppress checks (`:464-470`). Deliver a single kind-5 press through `deliver_active` (`:427-440`), as M/Esc/F3 already do.
2. Add a pure admission table in `src/network/lan_story_menu_policy.[ch]`:
   - F4 only on Items 0x1770B0 state 0.
   - F3 also on Items state 0, if the owner approves.
   - F1/F2 only on the Main Menu (state 3/5), the Weapons layer (+0x48 == 2) and Advancement (+0x1080 == 3). Advancement needs readability to 0x1084, since today only 0x4C is checked (`:432`).
   - Refuse F1/F2 while the client owns one slot. Under D10 that keeps today's behaviour: no cycle and no page rebuild.
3. In `poll_story_swap`, while a native menu or shop is shown, update `story_key_down` and return.
4. Tests:
   - Pure: StoryMenuPolicyTest cases.
   - Live: F4 sorts only Tal's inventory, the host logs no swap request, and M/Esc/F3 are unchanged.

**Open gates.**
- G1, read-only, only when the owner is not playing: read *(*(0x808D1C)+0x8D/+0x8E/+0x61) on both seats. Or set a hardware breakpoint at 0x40A015: it must hit for Enter but not for F2. This closes the cause.
- G2: press F1 in each seat's native menu and look for the swap notice or a host swap request.
- G3: confirm statically Items mode 2, Options sub-states 4/7, and that 0x421F60 only reorders.
- G4: find the game's binding load site (UNKNOWN). The launcher already parses PlayerOptions (`src/launcher/player_options.h:45`).
- G5: after O1, run the host negative check (F1/F2 never land on an OBSERVING Tal), plus a join/drop-window check.

**Owner decisions.**
- O1: does the #75 rule stand, given that #74's acceptance, the 2026-10-16 gate step 3 and #87 item 4 expect the host to equip Tal? Does "connected" include JOINING, PENDING and DRAINING?
- O2: which keys does a client get: F1/F2, F3 on Items (D10 keeps read-only pages), F4 sort, Options F3/F4?
- O3: should the F1 story swap be suppressed under native menus, or moved to another key?
- O4: should the client get parity for Space, Numpad0 and B?
- O5: keys 1-4 already move the client's Items quick slots. Keep that, or block it?

**Depends on.** #75 and D10; #87 and #74; the ArmourChoice adapter, which owns dispatch slot RVA 0x93CFC. The input swallow and the host switch lock must stay in place. Coordinate the edit to the shared `lan_story_runtime.c`. No dependency on #72.

**Route relevance and effort.** Low for #54: two characters, one slot each, and F3 already works on Weapons. O1 is medium, because it decides whether #87 item 4 can be tested. The F1 swap becomes a hazard once Buki or Elco join. The effort is small: about 20-30 lines, one pure table and a 3-line guard, with no wire change.

### skills75 — Client menu pages that change character state beyond weapons, armour and items (#75 skills)

**State today.** Nothing handles advancement yet. The adapter (`src/hooks/lan_story_main_menu.c`) covers weapons, armour and items. Of the ten Main Menu pages, only page 9, UILayerAdvancementMenu, writes other character state: it spends Advancement Points (AP) on Health, SP, Power and Essence levels and on skills. Nothing blocks it on a client, so a purchase there would change only the paused replica (INFERENCE: never seen live, and no such event appears in the private live logs, conclusions only). The party frame carries current HP/SP only (`src/network/lan_story_frame.h:83`). Level, XP, AP, max HP/SP and ability levels are not replicated. Clients see the host's XP popups (CONFIRMED_LIVE, #65), and by static reading nothing of a level-up. D10 is silent on page 9.

**What the game does (evidence).**
- Page 9 (vtable RVA 0x2D8A3C, handler 0x1726C0) uses 0/2 to confirm, 1/3 to leave, 4/5 or 6/7 to move, and 8/9 to toggle Yes/No. The UI filter 0x9FD0 always forwards 0, 6–9 and 0xC–0xF. It drops 1–5, 0xA and 0xB unless a pause flag is set, and the client skips the call that sets it (`lan_story_main_menu.c:361`). Back therefore comes only from the adapter's M/Esc poll (`:471-472`). CONFIRMED_STATIC. Which keys send 4/5 is UNKNOWN.
- The only purchase writer is the commit 0x1754B0 (EAX = layer, plain RET). Five "state == 8" sites call it: 0x17296E, 0x1732C3, 0x174381, 0x175576 and 0x175621. It checks gate 0xDA1D0, subtracts the integer cost (table +0xCC) from the float AP at stats+0x78, and calls 0xDA070. Nothing else calls 0xDA070. CONFIRMED_STATIC.
- 0xDA070 stores the int16 level at experience+0x114+id*2. For ids 1–16 it also applies the level delta, in either direction, to max HP/SP (clamping the current value), Power or Essence. It ignores a level below 0 or above the table maximum. CONFIRMED_STATIC.
- Gate 0xDA1D0 is read-only. It checks class, character level, the per-level requirement 0x156B10 (9999 from level 11, so attributes cap at 10), the prerequisite, the maximum level and floor(AP/cost). CONFIRMED_STATIC. Skill maxima are runtime data, so "max level 1" is INFERENCE.
- The name table at RVA 0x33CA28 maps Level, ExperiencePointValue and AdvancementPoints to the floats at +0x44, +0x74 and +0x78. CONFIRMED_STATIC.
- AddExperienceToParty 0xD99B0 feeds four fixed slots with no downed check. Unless group+0xD4 is set, it calls DoLevelUp 0xD9DB0, which changes Level and AP only. The effect, the `level_up` sound and the tutorial hints (0x17E860) stay host-local. CONFIRMED_STATIC; live distribution UNKNOWN (#65).
- While a client is OBSERVING, the host menu cannot select its character (`src/hooks/lan_story_runtime.c:2280-2291`, CONFIRMED_STATIC; own-character scope CONFIRMED_LIVE, `docs/status-matrix.md:103`). So nobody spends that character's AP.

**Smallest correct design.**
1. Optional interim guard (owner decision): skip the client's commit at the five sites and show a notice.
2. The host sends a progression snapshot as an additive optional message: level, XP, AP, the 41 levels, and max HP/SP/Power/Essence. Each character is first proven by its own vtable (`lan_story_runtime.c:2274`) and its component back-pointers.
3. The client converges by calling 0xDA070 for each differing ability and writing the floats. It then re-reads +0x114 and +0x30/+0x38; current HP/SP cannot be compared, because the mirror (`lan_story_runtime.c:775-793`) rewrites it. It never calls AddExperience or DoLevelUp. A mismatch logs `progression_divergent`.
4. An open page 9 in state 3 refreshes in the native cursor order (0x1727CF–0x1727DA). In any other state the refresh waits.
5. A new request kind, ADVANCE (7), carries slot = ability id and extra = target level. A stub at the five sites checks the vtable, the accept byte +0x1588 and the row +0x158C, then sends the request instead of committing.
6. The host applies the request in the control seam: gate, cost, 0xDA070, then a re-read. A silent no-op returns UNAVAILABLE.
7. Exact-image tests cover 0xDA070 and 0xDA1D0 (including decreases), the site guards and the table guards (RVA 0x3D27B8–0x3D27CC).

**Open gates.**
- #65: read the remote character's level, XP and AP around kills, including with one member down.
- Open page 9 on the client and log the vtable and which keys arrive.
- Run the refresh routines outside the handler on an exact-image fixture.
- Find whether the menu opens in combat, and what group+0xD4 means.
- Static: other writers of +0x78, +0x114 and ui+0x170, the save layout, and the effect resource at VA 0x7CB824.
- Owner gate on the Tal client: level up, buy Health and a skill, then cast the skill.

**Owner decisions.**
- Who has authority over progression?
- Block page 9 on clients until requests exist?
- Send the request at the commit or at Yes?
- Allow spending in combat?
- What level-up feedback and hints do clients get?
- Overwrite a divergent baseline, or only log it?
- Approve the new message.
- Where does progression persist?
- Who spends the AP of a client that has left?
- Does D10 cover page 9?

**Depends on.** #72/D6 with the Phase I sidecars, #65, #87 (types 0x13–0x16 fall back to Tal on the page), casting through 0xB4BC0 (C3/#57), the D12 S4 rewards, the cosmetic whitelist, the F-key filter work, and shared ownership of the menu, runtime, control and session modules.

**Route relevance and effort.** Medium: nothing blocks, but neither a Tal nor an Ailish client can grow. Effort is medium to large: the request is about the size of USE_ITEM, and the interim guard takes about a day.

### void73 — #73 Remote player falls through the void when the host streams his area out: warp to the leader instead

**State today.** Nothing handles an outdoor retirement. The only void rescue covers H1b shop visits (`src/hooks/lan_story_remote_interior.c:524-529`) and never fired in recorded live logs. Per-body area membership is a read-only host diagnostic that nothing acts on (`src/hooks/lan_story_runtime.c:298-365`). Two live falls, no recovery (CONFIRMED_LIVE, private live logs, conclusions only): on 2026-10-14 Tal, left in Illumina_Countryside_NW, fell for over 230 s once Ailish's next boundary switch retired NW; on 2026-10-17 he walked off a current tent floor and fell over 3500 units.

**What the game does (evidence).**
- Retirement rule (CONFIRMED_STATIC). 0x5A70, reached from SwitchZoneNOW (RVA 0x7990) and SwitchMainZone (RVA 0x6380), for an unloaded target unloads world+0x394 with no current-zone check (0x5ABA), then its links except the current zone (0x5B01), then loads the target. A link shared with the target is unloaded and requested again.
- Other unload paths (CONFIRMED_STATIC). UnloadZone (0x7BB0), RemoveLastActiveZone (0x7BD0) and UnloadNexus (0x7CA0) also set state 0; the observer hooks none (`src/hooks/lan_story_observer.c:11-13`). Route use: UNKNOWN.
- The unload runs inside the call (CONFIRMED_STATIC); ground gone that same frame is INFERENCE. Live, Tal's area read state 0, no data, no collision, and he fell at about 8.1 units/s, about 60 units from Ailish (CONFIRMED_LIVE).
- Native catch-up (CONFIRMED_STATIC). The formation snap 0xB3690 pops a follower more than 30 units away (A9=1); our C3 gate skips every A9=1 snap of a leased body (`src/hooks/story_ai_movement.c:66-97`). About two skips a second during both falls (CONFIRMED_LIVE); that they are the formation snap is INFERENCE, since five A9=1 requesters exist.
- Warp primitive (CONFIRMED_STATIC). 0xF30A0 at speed 0 sets position and facing and zeroes velocities; A9=0 requests pass C3 natively.
- Detection (CONFIRMED_STATIC). The collision pre-pass writes 0xFFFF to movement+0xBC (0xC44DE); a hit replaces it. Membership is contracted for the post-controller dispatch only (`src/hooks/lan_story_area_membership.h:32-38`).

**Smallest correct design.**
1. A host-only module on the post-controller dispatch (`runtime.c:3263`, same seat gate), reading membership itself each frame; no zone calls or snap-flag writes.
2. Retired-area check, covering every unload path: a leased remote member (not the lead, not in an H1b visit) whose own area is positively state 0, or has no zone data (+0x18 in state 1), is warped. A failed read is UNKNOWN: no warp.
3. Void backstop: +0xBC at 0xFFFF for about 0.4 s and about 4 units below the last ground, not paused, world not loading, 2 s cooldown; not held back by the post-switch windows (`observer.c:73`, `:1157-1163`).
4. Warp: 0xF30A0(actor+0x98, {lead sector, spot, lead facing}, A9=0, A8=1, 0) after node validation and an image hash. Spot: the lead's formation goal (0xB2B10) if the story formation is proven, else a point behind the lead, inside the lead's current area, never a story house (D11). Fallback `place()`.
5. Optional: a host-only call in `zone_switch` (`observer.c:439`) after the H1b evict records the doomed set for step 2.
6. Client: no protocol change; a local jump over about 8 units sets `reseat_pending` (`runtime.c:724-751`).
7. Tests: pure doomed-set and detector tests; exact-image tests of 0x5A70 and the 0xF30A0 immediate path.

**Open gates.**
- 0xF30A0 side effects on a leased actor: decompile, image test.
- A lease proof inside the script callback, and frames with no exact roster after a switch: live trace of a staged NW-to-Hub retirement.
- Who issues the skipped snaps, and why none before the fall (HYPOTHESIS: NW's sector record, or no follow state): gdb hardware break on 0xF30A0.
- Live warp against `place()`: ground, no residual fall, input next frame, no client refusal, no hitch.
- The story party's formation goal (slots, slopes).
- Backstop thresholds over route play: zero false positives.
- Survey the route's scripted teleports and unhooked unloads.
- The #73 owner gate.

**Owner decisions.**
- Presentation: pop, fade or banner (the last two need a host signal).
- Warp ahead of the unload even mid-action, or only once falling; abort or delay interactions (a delay is #70's hold).
- The destination when the lead is inside a story house (D11).
- Carry remote players on lead-only scripted teleports (#37)?
- Go or no-go on the residency spike (layer 3).
- Backstop for the host's body and AI companions too?
- Downed remote bodies (F2) in a retiring area.

**Depends on.** The pre-switch slot shared with #70; H1b (evict order, phases, snap flag; #83, #85, #86); C3; the H1c reseat; the client's permanent epoch refusal; G; F2, #82, #37; J.

**Route relevance and effort.** Medium-high (INFERENCE): once Ailish has joined and the client plays Tal, the route crosses several streamed areas, and a player left two areas behind falls with no recovery. Layer 1 is medium: one module of 300–400 lines, a client detector, tests, the gdb trace, one or two owner runs; the residency spike is large.

### hold70 — #70 departure hold and warning at host transitions

**State today.** No hold or warning exists; plan §10 lists #70 as filed with no work (docs/coop-story-plan-54.md:6802). A host house entry does not move clients: the split keeps them outside (D11, plan:68; CONFIRMED_LIVE for one story house). Clients get no warning for host doors, boundaries or scenes. The zone banner runs only on clients, for their own entry (src/hooks/lan_story_runtime.c:3690, src/hooks/lan_story_remote_interior.c:839-842). Nothing hooks the pre-OnAction chain in the story profile (src/loader/dllmain.c:1683-1687). Both premises in the issue fail: the press never reaches CDoor::ActivateFromScript, and refusing a boundary zone call could strand a paused loading screen.

**What the game does (evidence).**
- CDoor::ActivateFromScript (RVA 0xCE3A0) has no native caller. Only the script door helpers call it. CONFIRMED_STATIC.
- The door script runs UIStartTSA (RVA 0xB9C0), a fade and DisableGamepad, then HidePartyMembers and EnterTemporaryZone (CONFIRMED_STATIC; src/hooks/lan_story_task_trace.c:1023-1042). TEMP starts about 2 s after task creation (CONFIRMED_LIVE, 13 presses, private live logs). A hold must therefore come before the task exists.
- RVA 0xD951 (0xD7A0 calling 0xCCD0) is the only place an OnAction message is built. 0xCCD0's other caller, 0xD183, builds only OnEntry/OnExit (CONFIRMED_STATIC). That the live Kamo tasks came this way is INFERENCE until G1.
- Skipping one kind-2 build leaves no native residue: 0xD76D clears the latch, and candidates are rebuilt every frame (CONFIRMED_STATIC; G2).
- The latch manager+0x1D8 has native writers at 0xC667, 0xD76D, 0xDB135 and 0xDB16A, plus mod writers at src/hooks/control_separation.c:8396,8462 (CONFIRMED_STATIC).
- While the UI-active predicate holds, front messages are re-queued at 0xC970 and the contact builder 0xCFD0 builds no OnEntry/OnExit (CONFIRMED_STATIC). Whether a visible message box sets that predicate is UNKNOWN.
- Route doglegs call only SwitchZone, a background load. The observer passes it through (src/hooks/lan_story_observer.c:373-384; CONFIRMED_STATIC). Host dogleg tasks precede it live (CONFIRMED_LIVE).
- The string message-box exports reach the Show gate at RVA 0xABB80 (CONFIRMED_STATIC).

**Smallest correct design.**
1. One host interaction-dispatch adapter owns the call sites RVA 0xD75B, 0xD951 and 0xCAEB, with interaction_provenance as an observer. It is the only mod writer of +0x1D8 and absorbs Phase H's shadow latch (plan:182).
2. Hold at 0xD951 when the source is the front, the event is type 2, the trigger is in a door table built at runtime from local game data, and policy asks for a hold. Skip 0xCCD0 and record trigger, front, generation, world, epoch, serial and deadline.
3. Release at 0xD75B when the deadline passes or all players are ready or near, and only if the held trigger is still a kind-1 candidate with unchanged identities. Set +0x1D8 inside that call and pass that build once. 0xD76D clears the latch either way, so a refusal discards. Hide any host box first (G11).
4. Discard on leaving the volume, a lead, epoch or world change, combat, a menu or teardown. Never replay.
5. Pure policy from transition_vote: no veto, and silence releases. Tested.
6. Story frame 10 to 11: a hold record and a "travel ready" action.
7. Banners go through the zone-banner owner, extended to the host with a string overload.
8. Boundaries and scenes get a banner only. Never refuse or replay a zone call.

**Open gates.**
- G1: hardware breakpoints at 0xD951, then 0xCAEB, at the Kamo door.
- G2: in a research build, skip once and release once. Expect no task, no TSA and the latch at 0.
- G3: decode dispatcher passes 0 and 1 for overlapping candidates.
- G4: find what the gate's state checks and CUIScene+0x174 mean.
- G11: does a visible box set the UI-active predicate? Does a deferred contact survive?
- G6, G9, G12: remote-body boundary tasks, the walk-in list, and a committed rule for counting transition triggers.

**Owner decisions.**
- Under D11, hold house presses, show a banner only, or hold only when a member would be moved. The same applies to ExitInterior (#86).
- N, R, which humans count, the ready key, early end, walk-away.
- Latch re-arm or direct build, and the latch owner.
- The protocol bump; banner medium and wording.
- Combat or a menu during a hold; walk-in transitions.
- Whether a hold without veto fits D2.

**Depends on.** Phase H adapter, zone-banner owner on the host, STEP-owner accessor, transition_vote, #73, H1b eviction, #85, #86, Phase G, #87.

**Route relevance and effort.** Low for the door hold: house doors on the route leave clients outside, and before Ailish joins the client only spectates. Medium for the boundary banner, which feeds #73. Effort is medium. Most of the risk is in G3, G4 and G11.

### paused82 — #82 Objects in areas the host is not in are paused; chests do not block the remote player

**State today.** Research only; no code addresses #82 (§10, `docs/coop-story-plan-54.md:6802`, owner priority). On the host, a spawned object in a cluster the leader has left holds one native cluster pause reference (entity+0x2B), unless it is DontEverPause, of type 0x0C or 0x1A, or has no cluster; for a chest that switches its collider off. The activity lease (`src/hooks/lan_story_activity.c`) wakes only NPCEntity spawns in a remote player's cluster (:267) and has no test. H1a steps a remote opener's paused chest (`src/hooks/lan_story_remote_use.c:356-383`, 5 s abort), but every logged step had the host in the same cluster.

**What the game does (evidence).**
- CONFIRMED_STATIC: the cluster manager update 0x35120 runs 0x352D0, which resolves the leader's sector; 0x35FE0 deactivates the old active cluster (+0x44 mask 0x4, event type 1), 0x360E0 activates the new one, and listener 0x13D4D0 calls pause 0x13D540 or resume 0x13D5B0 when spawn+0x60 matches. Script exports and 0xCF620 (door transit) also change it.
- CONFIRMED_STATIC: 0x13D540 does not exclude chests (type 0x1C). Node pause 0x1060C0 increments entity+0x2B; at the zero crossing 0x13DED0 unschedules the usable's update and CPosition pause 0x110D00 zeroes collider +0xF4. CONFIRMED_LIVE (#82): chest +0x2B=1, zero treasure updates in 4 s. INFERENCE: that reference is the cluster one.
- CONFIRMED_STATIC: the collider stays enrolled; movement 0x336B0 → 0x32320 and queries 0x1C90A0, 0x1C9740, 0x1C9E00, 0x34C20 skip +0xF4 = 0. INFERENCE: this is the missing collision.
- CONFIRMED_STATIC: entity+0x2B is shared (zone-load stages in 0x109800, 0x13DC00, helper 0x186C90, TEMP helpers); SetCollision 0xCB1A0 and 0xCAD60 write +0xF4 and CPosition+0x100 whatever the pause; colliders are created lazily (0x13DE20), and 0x110D20 does nothing without one. Chest code has no direct +0xF4 writer; CDoor writes its own.
- Private live logs, conclusions only: the opened chest is E3_NB_ShdCamp in all three sessions (CONFIRMED_LIVE). Host near: stepped with paused=0, finished. Far: three NPC leases in Tal's cluster reached refs=0 after one resume (CONFIRMED_LIVE); the open stalled on a build that, by INFERENCE, had no stepping. No collision data.
- CONFIRMED_LIVE (plan :6614-6625): Tal's host body moves through native 0x32320; his contact with walls or props is UNKNOWN.

**Smallest correct design.**
1. No new hooks: extend `lan_story_activity.c` on its existing seams (`lan_story_runtime.c:2102` service; releases :1579, :1850, :1954; exit and uninstall :3836-3908).
2. Per-class identity replaces the NPC filter (:267), `entity_exact` (:133-146) and the NPC node-slot check (:159-160).
3. Option C, collision only: acquire only when the cluster's mask 0x4 is clear, +0x2B == 1, CPosition+0x8C is set, +0xF4 == 0, +0x100 is 0 or 1 and the collider is enrolled once (`src/hooks/lan_story_collision_owner.c:79-100`); call 0x110D20 and confirm. Forget the lease when native resumes or +0xF4/+0x100 change under it; on release call 0x110D00 only if both are still ours.
4. Option L, wake: add COpenObject 0x2D514C (sub-node 0x2D51C4) to `transition_exact`; it fits the existing sub-node rule (:180-185), so no new function hashes. H1a stepping becomes a fallback.
5. Doors and breakables after G3; a door under Option C follows CPosition+0x100.
6. A failed non-NPC check skips that object only; re-read mask 0x4 every service.
7. Never wake clusters natively: a non-current woken cluster is never deactivated.
8. G7: the module's first image test.

**Open gates.**
- G1: read-only host probe of that chest near and far on one build (+0x2B, +0x100, +0x8C, +0xF4, enrolment, cluster mask); then Tal walks into it.
- G2: TempExteriorProbe profile; NpcActivityPause for the chest with caller_rva 0x13D504 or 0x13D518.
- G3: component chains and +0xC0 of chest, door, breakable.
- G4 (Option L): no host prompt from 0xB6850 for a chest woken only near Tal.
- G5, G6, G8: Tal's melee reach to breakables; +0x8C of a never-active chest; a paused-cluster remote open completing by stepping.
- G9–G10: route scripts calling SetCollision or cluster exports; whether spawn+0x82 is saved.

**Owner decisions.**
- Per class: wake natively (L) or collision only (C)?
- Scope: the remote player's cluster, or a radius across clusters?
- May remote players smash breakables in paused areas (D5/D9 drops)?
- Doors in paused areas: work for Tal, or only block (H1b, #67)?
- The acceptance spot, and #82's priority among the §10 items.

**Depends on.** H1a stepping. The split TEMP path (lead in a story house, Tal outside) keeps the exterior live but, by INFERENCE, cluster-paused; with keep_live=0, 0x10B500 zone-suspends it and neither option undoes that. #73 zone unload (`lan_story_activity.c:282-290`). #83 and the uninstalled `lan_story_collision_query.c` grid seam. D5/D9 loot. #84/#88 contacts. Phase G per-player streaming.

**Route relevance and effort.** Medium to low: only when Tal is in another cluster from the host leader (host at the border or in a story house); no route blocker unless the owner gates it. Effort: small (Option C, chests), small to medium (Option L), medium (doors, breakables); native cluster waking is large.

### doors67 — #67 remote door use, client PVS doorway, reveal-door alpha; #66 chest follow-ups

**State today.** Research only at 9081425. Doors the host lead opens show on the client. DoorEntity is world kind 0x0F8C, the client copies the door's collision byte, and the host gives remote characters the door camera (owner acceptance 2026-10-13/14, status-matrix rows 100–101). A remote player cannot open a door: H1a handles chests only. The client never calls SetPVSDoorState. No client spawn of the door burst SFXA203 (kind 240) is logged; the 24 kind-239 spawns in the private live logs are the chest effect. Ailish and Elco send no use press. #66: one clean remote chest open was accepted, and the prompt is an estimate.

**What the game does (evidence).**
- CDoor (vtable 0x2D637C, DoorEntity+0x3B0) has its node at +0x18 and its listener at +0x3C (RVA 0x14B0B8–0x14B0CC). CONFIRMED_STATIC.
- A player use is host-wide. Begin 0xCEFB0 cures party concussion, suspends the combat cluster and overrides every monster and NPC AI. The traverse 0xCEA30 sets the host camera (RVA 0x38500) and walks the formation when group+0xD0 <= 0. CONFIRMED_STATIC.
- State 6 (0xCE8CF) waits only for the members 0xCF450 admits. It times out after 3.0 (RVA 0x2D8A34) and pops the formation (0xCFC10). CONFIRMED_STATIC.
- ActivateFromScript (0xCE3A0, export 4) opens the door without the front, camera, AI or UI. Its second argument is the side bit (INFERENCE). CONFIRMED_STATIC.
- 0xCE300 opens a named PVS door from three paths and starts a 600-update timer. Use and script both wait on it; when the callback clears it is UNKNOWN. CONFIRMED_STATIC. The paused client copy never runs it (CONFIRMED_LIVE, plan :4482).
- 0xD9A0 refuses use in combat or during a fade. H1a calls the listener directly (lan_story_remote_use.c:458) and skips that gate. A rattle does not prove a lock. CONFIRMED_STATIC.
- One usable at a time (0x3C3B38). Door prompt is flag 4 or 8, after 0xB5E40's trigger check; H1a's hint carries 4 and 0x200 only (lan_story_frame.c:142). CONFIRMED_STATIC.
- A native traverse needs eleven more front-copy redirects, including 0xCF5ED. Three of them (0xCEC1C, 0xCEC78, 0xCF4BC) fail the 40-byte rule. CONFIRMED_STATIC.
- The route has five DoorEntities and no levers (CONFIRMED_LIVE). Lock exports are RET stubs (0xA2900). REVEAL_DOOR is unused (INFERENCE). Door alpha: codec CONFIRMED_TEST, client apply source only.

**Smallest correct design.**
1. Add a door kind to the H1a adapter, which owns the front and controller hooks. Guards: exact DoorEntity/CDoor identity, state 0, CInvisible dispelled, no current usable, the opener's arbiter bits, and combat/fade checks if O4 says so.
2. Hint 3 = flag 8: raise the validator and the story frame version, and add a stand-in for 0xB5E40.
3. Give Ailish and Elco a use press (G10).
4. Path S (O1): the scoped side test, then ActivateFromScript(1, side). Close it once the opener is clear, or on timeout or teardown. Never close a door the host script opened.
5. Path N (O1): eleven copy sites and hooks on update, +0x5C, +0x60 and +0x64. Skip the host camera and formation calls and return 1 at 0xCE8CF. The inline writes are O2.
6. PVS, only if G4 finds a named door: SetPVSDoorState from the client door-apply block (lan_story_world.c:2678-2707); close it on retire, zone change and teardown.
7. #66: the native volume test instead of the estimate (G8).

**Open gates.**
- G1 live: Ailish uses the TallGate with Tal 8 units back. Read CDoor+0x74, group+0xD0 and Tal's arbiter.
- G2 static: exact bytes for the three failing sites and the skip sites; what 0xDC720 does.
- G3 host gdb (hardware breakpoints): who calls +0x5C and +0x38.
- G4 read-only: PVS name, timer, +0x70 and +0x73 on the five doors.
- G5 owner: does the door burst appear on the client?
- G6–G12: effect source, paused doors, volume contract, invisible doors, ranged input, shimmer, non-front candidates.

**Owner decisions.**
- O1: path S or path N.
- O2: which host-wide effects apply for a remote opener. Decision (a) covers chests only.
- O3: keep one use at a time. O4: the combat/fade gate for remote players.
- O5: what a press at a locked door or a story-house door does.
- O6: a wire bump for PVS timing. O7: hide the opener's HUD?
- Still open: the shimmer, and the first chest gate under D11.

**Depends on.** The H1a adapter; lan_story_frame.c; lan_story_world.c and lan_story_runtime.c (shared, coordinate first); #82; later, Phase G.

**Route relevance and effort.** On the route (Ailish host, Tal client; plan :6789), Tal is a melee seat and already reaches the use attempt (lan_story_runtime.c:2164-2177). The gap is Tal opening a gate himself. With Ailish or Elco as the client there is no use path at all (#87). Path S is small, about one session. Path N is large, two to three sessions plus gdb gates. PVS costs nothing unless G4 finds a named door.

### interiors83 — #83 More than one interior in use at the same time

**State today.** One house at a time, by design (D11): members enter only the house in use or the lead's shop, a loading member ends when the lead enters another house, and an empty background house is unloaded (`src/hooks/lan_story_remote_interior.c:192-196, 255-277, 442-443, 571-583`; CONFIRMED_STATIC). Shop visits are CONFIRMED_LIVE (§9, 2026-10-17). The hang behind #83 is CONFIRMED_LIVE (private live logs, conclusions only; owner report in the issue). No hardware watch yet.

**What the game does (evidence).**
- The sector (CAiTracking +0x40) is written by the setter 0x185370 (store 0x1853CA), reached by walking (0xE67C8), placement 0xB27C0 (which also bulk-copies +0x40..+0x5B at 0xB280E), the marker path 0xC2DC0 and an invalid-sector reseed in 0x185150. CONFIRMED_STATIC; completeness UNKNOWN.
- Only the native lead's floor contact runs the zone walk (call at 0xE67A5, its only direct reference). It writes world+0x10 when the touched zone is not current; touching the current zone clears the lock byte +0x39E, so LockActiveZone cannot pin it. CONFIRMED_STATIC.
- CWorld::Update demotes the old current zone to state 2 only when the new zone lacks interior flag 0x02 (0x573B-0x577F), so two swapping interiors both stay at state 4. CONFIRMED_STATIC; as the incident's cause, INFERENCE.
- In the incident Kamo became current in state 4 while the lead was in the tall tent; her tracking named Kamo's region (0x0D01), her movement sector stayed the tent's (0x4201), and the exit unloaded Kamo instead of the tent. CONFIRMED_LIVE; the link to the endless load is INFERENCE.
- Every host background load of Kamo made a state-4 descriptor current for one sample (12-14 ms) with the lead outside. CONFIRMED_LIVE; writer UNKNOWN, not the walk.
- Enrolled floors share one grid (CONFIRMED_EXACT_IMAGE, invented floors, `docs/story-independent-areas.md:95-103`); a member stands on a background house floor (CONFIRMED_LIVE). Navigation is per region (0xEF5D0). Walls and props crossing houses: INFERENCE.
- One global active renderer (RVA 0x3C300C); the host re-activates the current zone's after each background load (`lan_story_remote_interior.c:497`, CONFIRMED_LIVE); the native site that switched it is UNKNOWN.
- Scene, observer and module each hold one interior (`src/network/lan_party_story.h:9-37`; `src/hooks/lan_story_observer.c:41-55`; `lan_story_remote_interior.c:217-220`); the observer's LoadZone hook evicts visits and bumps the epoch (`lan_story_observer.c:303-310, 385-391, 440`); the area policy is two areas (`src/engine/story_area.h:6-12`). CONFIRMED_STATIC.

**Smallest correct design.** Under option (a), the owner's choice, pinning each member's zone:
1. One host-only adapter owns the walk call at 0xE67A5: verify image, bytes 0xE679C-0xE67AA (world operand after relocation), 0x5A30 and slot 0x2C8688; transactional install, quarantine on failed restore. Skip the walk only when the proven record's descriptor is a held house other than the lead's area; fail closed on an unproven record; never write world+0x10/+0x0C/+0x39E; log a held house that is current or in state 4.
2. The same adapter guards the sector at 0xE67C8 or at 0x185370 (after G1; the setter covers four paths, not the bulk copy): members move their sector only within their own house.
3. A small residency set in module and observer. A second house cannot come from SwitchZoneNOW (it unloads world+0x394); LoadZone only after G3 and once the observer treats our own LoadZone as a background load. One load at a time; widen the two-area policy.
4. Lead zone calls keep other houses only after G4; streaming still ends visits unless the uninstalled eviction-reservation adapter (`src/hooks/lan_story_area_eviction.h:6-18`) is adopted; lead exit with #86.
5. Scene v3 with more than one interior; exactness still checks only the current descriptor.
6. Collision scoping (existing, uninstalled `lan_story_collision_query`) before overlapping houses coexist; steps 1-2 stay for non-query paths.
7. Native-executing image test; live tent-plus-Kamo gate in both orders.

**Open gates.**
- G1: host hardware watch on the lead's sector and world+0x10/+0x0C during a research background load and a tent walk; needs owner consent (reproduces a known hang).
- G2: log the contact descriptor per frame in that run.
- G3: LoadZone's final state, renderer and current-zone effect (0x109800 statically, then live).
- G4: read the exterior's linked-zone list at runtime.
- G5-G9, from the G1 run or read-only probes: foreign cluster scripts, renderer site, coordinator cost, background NPCs (#82), which interiors overlap.

**Owner decisions.**
- Priority (§10 owner priority; D11 one house; 2026-10-16 "expand the engine").
- Option (a) or (b); required combinations; residency cap.
- Scene v3 protocol break; interim cross-house collision; hiding members in other houses.
- Step 1 now as a guard; consent for G1.

**Depends on.** #86, #85, #82, #73/#70, the collision coordinator (#24/#25), more D11 shops.

**Route relevance and effort.** Low: the #54 route never needs two interiors. Residual (INFERENCE, unseen since the policy change): a house still loading when the lead uses another door could become current beside hers and outlast the 15 s drain; step 1 alone does not cover that swap. Effort: steps 1-2 small to medium, 3-5 medium to large, 6 large; option (b) very large.

### H1rest — Farmhouse sword chest against D11; more shops in the door table

**State today.** At 9081425 the door table and the shopkeeper table each hold one row: LNBr_Kamo_shop with NPC_Kamo_NB (`src/hooks/lan_story_remote_interior.c:55-67`, `src/hooks/lan_story_shopkeepers.h:12-14`). When the lead is in any other house, members stay outside (`:571-573`). H1a's remote chest open is accepted live once, outdoors at E3_NB_ShdCamp (plan 6563-6567). The owner's first chest gate is the farmhouse sword chest (plan 6483). D11 keeps that story house lead-only (plan 68), and §10 lists the conflict as an owner decision (plan 6798). The castle Kamo is kept out only by its absence from both tables. Only the keeper table has a pure test (`tests/story_presentation_privacy_test.c:12-20`).

**What the game does (evidence).**
- The chest is Boss_Chest_Mungo in LICo_Frappe_Farm. The door opens only once the story block reaches 1200, which Mungo's death sets; the block is the int16 at RVA 0x3C2FEC. No interior script writes story state: the writes 1115, 2250 and 7255 come from the outdoor FARM_HOUSE cluster script. CONFIRMED_STATIC, SOLWORLDM.gex 0x5FAF4, 0x68533, 0x14FD0; RVA 0xFD9C0.
- EnterTemporaryZone (RVA 0x64B0) refuses any target whose state is non-zero (0x64EE). It stores the front's position as the return point (CONFIRMED_STATIC). For the farmhouse, the live return points cluster at the lead's door spot, and the marker matches the arrival within 0.03, as at Kamo (CONFIRMED_LIVE; private live logs, conclusions only). Members are still placed at a measured outside spot (`lan_story_remote_interior.c:282`, `:417`).
- The 2026-10-14 take-along freeze (plan 4891-4911, CONFIRMED_LIVE once) is the only farmhouse take-along in the kept logs. At that point the split mask was empty: Tal, about 39 units from Ailish, had no lease. INFERENCE: the empty mask (`src/hooks/lan_story_runtime.c:1801-1835`, `src/hooks/lan_story_split.c:602-605`) chose the take-along, not the door trigger the plan supposes (HYPOTHESIS, plan 4905-4906).
- The client holds its own trigger dispatch and cluster scripts all session (`src/hooks/lan_story_client.c:28-32`, `:238-295`; CONFIRMED_STATIC), so it never switches room cameras (INFERENCE).
- The castle Kamo writes story only at the castle start. Its exit sets block 1050 and starts the alarm only below block 1050, and its award advances 'Muster at the gates' only at quest stage 0. Later talks just open the shop. CONFIRMED_STATIC, GEX 0x5948F, 0x5BD76.
- The other route interiors with merchants are two inns. Their rest charges 25 florins, heals the group and moves the player who rests. Traders and smiths stand outdoors (CONFIRMED_STATIC). No kept log shows a visit to any of them.

**Smallest correct design.**
1. A pure header lists the barred houses: LICa_Kamo_Shop with its door and NPC names. Do not call them excluded houses: that name is already taken (`src/hooks/lan_story_world.h:25`).
2. Enforce the list inside door_for() and both keeper lookups. Leave the client's area-state follow unguarded (`:762-766`), because a native take-along needs it.
3. Move doors[] into a pure header and extend the privacy test. A door-table test asserts: no barred name in any case, a door row for every keeper zone, NULL cameras allowed, and no lead-present-only admission while the lead is outside.
4. Option A: a farmhouse row reachable only through the same-house-as-lead branch (`:583`). It needs min_block 1200 (≥), the marker, a measured outside spot and FrappeHallCam. The chest uses H1a, and D6 copies its loot once.
5. Option B: retest first, logging the lease and the mask.
6. Inn rows are data with walk-in visits. The rest becomes a host-applied request.

**Open gates.**
- G1 (live, A): Tal opens Boss_Chest_Mungo while Ailish is inside; one open, and the loot is copied once. Needs a save where the chest is still closed (UNKNOWN whether one is kept).
- G2: does Tal's host body fire the host's triggers or the FARM_HOUSE writes? Close with a read-only hbreak on RVA 0xFD9C0 while Tal walks the porch alone.
- G3 (live, B): retest, logging mask, lease and distance.
- G4 (live): each new row's arrival and outside spot.

**Owner decisions.**
- First chest gate. A: remote farmhouse entry, amending D11. B: native take-along. C: accept E3_NB_ShdCamp.
- If A: lead inside only, or solo? Room cameras?
- Which inns join, and is a walk-in visit enough? Who pays for a rest, whom it heals, and what becomes of receipt item 708?
- Should the outdoor merchants become per-player shops, and in which D12 stage?
- Castle Kamo: barred for good, or only while its writes can run? Should the host's Kamo talk stay public? No plan record names this exclusion.
- A take-along into a barred house: evict the member, or let the client follow?

**Depends on.** H1a; H1b (shared module: coordinate edits); #82, #83, #85, #86; D6/D9; D12 S1/S3/S4; NG; Phase I.

**Route relevance and effort.** The acceptance route needs no chest and no shop. In NG the castle Kamo writes come before Ailish joins (INFERENCE), so barring it is a small guard. Option A is medium, solo entry large, option B medium (cause unproven), option C none. Inn rows are small; inn services are medium to large.

### S0priv — D12 S0: keep the host's conversations private beyond shopkeepers

**State today.** Each tick the host captures CSpeech, UIDialog and the bars into one record for every observing peer (`src/hooks/lan_story_snapshot.c:124-137`, `src/network/lan_party_session.c:2365-2389`). The only filter is per line: a line spoken by the one table shopkeeper (`NPC_Kamo_NB`) makes the conversation private and it is sent as the idle record. A line by any other readable speaker makes it public again (`src/hooks/lan_story_cinematic.c:349-371`, `src/hooks/lan_story_shopkeepers.h:12-14,33-37`). Every other host conversation reaches clients with text and voice. CONFIRMED_STATIC; the step is CONFIRMED_TEST (`tests/story_presentation_privacy_test.c:22-35`). No issue tracks this; #80 overlaps.

**What the game does (evidence).**
- Host Kamo talk was classified private on its first line, and the client, in the same shop with its own talk closed, presented only idle records (2026-10-17). A non-shopkeeper host line was voiced and drawn on the client (2026-10-18). CONFIRMED_LIVE (private live logs, conclusions only; no visual or owner check).
- An unreadable speaker makes the frame unknown: nothing is sent and the privacy state is kept (`lan_story_cinematic.c:349-350,392-393`). CONFIRMED_STATIC.
- ActivateLine (RVA 0x13910) first clears the line (0x13500: speaker +0x3BC and listener +0x3C8 nulled, mode +0xC8=2). It assigns them only when b4 (+0x478) is clear, and finds a missing speaker by name (0x13CF0) or refuses. A b4 line hands its text to the dialog box (0x153F0) and returns CSpeech to mode 2 (0x13C43–0x13C4C), so it is never a captured line. AskQuestion (0x13730) passes its sixth argument as b4. UIDialog-only text boxes are never captured either (`lan_story_cinematic.c:316-318`). CONFIRMED_STATIC.
- Span markers: CSpeech+0x47D (0x132C0, set and cleared by Start/EndInteractionScript, `src/hooks/lan_story_shop.c:150-157`); UIDialog+0x6B8 bit 0x80 (set by Start 0xA4650; End 0xA4690 leaves (old&0x1E)|0x40). CSpeech is embedded with a 0x480-byte span (RVA 0x76862–0x7689E). CONFIRMED_STATIC.
- Clients act only on LINE_ACTIVE, LETTERBOX_PRESENT and CUE_PRESENT (`src/hooks/lan_story_runtime.c:667-668,2798`; `src/ui/story_cinematic_view.c:68-70`). CONFIRMED_STATIC.
- Spectators apply the host view every frame (`lan_story_runtime.c:677-678`), and the speech camera is a selected named camera (`src/hooks/lan_story_client_camera.h:48-53`), so a private talk's framing probably reaches them. INFERENCE; live UNKNOWN.
- A private census: about 39 of 370 conversation roots change story, party or lead; all can reach bars, 17 speak first. INFERENCE.

**Smallest correct design.**
1. A pure classifier beside the shopkeeper table (IDLE, PENDING, PRIVATE, PUBLIC) replaces `SudekiMpLanStorySpeechPrivateStep`.
2. A span starts PENDING. Bars or a public-table NPC make it PUBLIC; any other NPC makes it PRIVATE; PC-only lines keep the state. PRIVATE may become PUBLIC, never back. Only PUBLIC sends the full record; withheld lines are never sent later.
3. A gap, or a first look inside a span, gives PENDING; PRIVATE survives gaps. A span ends only on an observed idle frame with +0x47D and bit 0x80 clear.
4. Reads only, in `speech_owner`'s double read: CSpeech to 0x480, listener, +0x47D.
5. One identity routine for speaker and listener from `speaker_keeper` (`lan_story_cinematic.c:284-315`); PCs by roster pointer or canonical name (`src/hooks/lan_story_world.c:449-454`); unreadable is unknown.
6. Reuse the `talk_codes` guards (`lan_story_shop.c:175-206`); if they fail, the owner's failure default applies. No text in logs. Pure and exact-image tests.
7. With S1: a root observer in `src/hooks/lan_story_task_trace.c`, which owns the task-create seams (RVA 0x1C38F2, 0x1C4DB8) and logs root hashes (:674-678).

**Open gates.**
- G1 live: what the listener holds for Say and Ask (log participant names).
- G2 static: is the CSpeech entity the roster actor?
- G3: speaker names of public roots beyond Kamo.
- G4: are the bars up on the first line of cutscene-first scenes?
- G5 live: do the markers span a whole Kamo visit? The 2026-10-18 capture split one visit in two (INFERENCE).
- G6: client-started host talk; 0x524320 as trigger eligibility is HYPOTHESIS.
- G7: branch order of the 17 talk-first roots. G8: which roots are #80 tutorials.
- Client checks need the client's own talk closed and the client outside its own visit (`lan_story_runtime.c:832,1331,2880`). The §9 owner check (plan line 6724) stays open.

**Owner decisions.**
- D12 already makes flavour talk private and story scenes shared. Open: detect story scenes by bars alone, or bars plus a short story-NPC table? Do house-travel, zone/spawn-starting or world-map talk count? Default when classification fails, including talk with no NPC participant?
- Spectators with no character yet (NG, #80): host camera only, nothing, or text too?
- No host side-quest talk on clients until S1, or ship with S1?
- Table as names, hashes, or derived at run time?
- Story talk in a story house for players outside (D11)?
- #80 tutorial text as public (it needs a new capture anyway)?

**Depends on.** S1 (notifications, root observer); #80; D2; coordinated edits to the shared `lan_story_runtime.c`.

**Route relevance and effort.** Moderate on the §10 route: every host flavour line reaches Tal's game today (CONFIRMED_STATIC), while most route story scenes show bars from their first line (INFERENCE). Small: 1–2 days plus one live gate, 2–3 with G1/G3; no new hook, no protocol change.

### S1 — Story-progress mirror and quest notifications (D12 S1)

**State today.** Research only (9081425, clean tree); nothing built. Clients receive neither the story block nor quest stages (CONFIRMED_STATIC). `MSG_STORY_BLOCK` (`lan_party_session.c:23`) is the guard input. In four paired sessions the host's block was 1500 and the client's 1400 (CONFIRMED_LIVE; private live logs, conclusions only). No client sample follows the host's 2000. No issue tracks S1.

**What the game does (evidence).**
- SetStoryBlock (0xFD9C0) posts only to the Zone collection. Each zone loads or releases its story-ranged records (0x10A730, 0x10AAC0) and submits no script. The save loader (0x100622) and `-Story` (0x10525A) also call it. Other readers are zone, prop, spawn and loot filters. CONFIRMED_STATIC.
- A client's Kamo reads that client's block (`lan_story_shop.c:371`, `:814`). Stock tiers start at 2200 (`tests/sol_shop_stock_image_test.c:67`), so on the route only the T2 greeting differs (≥2000). CONFIRMED_STATIC. The client stocked from 1400 (CONFIRMED_LIVE). Host divergence at Kamo is unobserved.
- QuestLogSetStage (0xB0400) checks only the upper bound and marks the quest page new. −3 means an unknown name (0xB03F1). Stored stages are −2, −1 and 0..n−1. CONFIRMED_STATIC.
- The quest page indexes by stage unchecked (0x162B5A) and rebuilds only when it opens. A write while it is open could read out of bounds (INFERENCE).
- In the QL_* wrappers (CONFIRMED_STATIC), Start shows 5708 plus a chime that ducks battle music. Update shows 5709, or completes the quest past its last stage (0x36029). Complete shows 5707 and gives XP, and Solo gives it to the front player only. QL_NM_* is silent. Boxes carry no quest id, and a keypress hides them at once.
- The box hash (+0x1A0) changes only when Show accepts (0xABC12), so box polling is HYPOTHESIS.
- Route events (CONFIRMED_STATIC): the ambush sets 2000 with no box (0x25A5B). The lighthouse sets 1400, completes 'Find Princess Ailish' and starts 'Escort Ailish home' (0x723A0). Home sets 2010, shows a completion box and gives XP (0x54E29).

**Smallest correct design.**
1. A pure `lan_story_progress` codec. It carries epoch, revision, tick, sequence, a questlog digest, the block, per-entry stages validated to {−2, −1, 0..n−1}, and a ring of 8 notifications. Add `MSG_STORY_PROGRESS`, raise the kind bound (`lan_party_session.c:1308`) and the profile 26→27 (`:33`).
2. An observe-only host capture every service, beside `lan_story_runtime.c:3356` and outside the 100 ms area gate. The box and string guards get a host-side init, since the banner module is client-only. A change notifies only when proven (box hash or opcode-adapter observer).
3. Client apply on the native thread:
   - refuse on a digest mismatch;
   - defer unless the menu is positively seen closed (`SudekiMpLanStoryMainMenuVisible` FALSE can mean unknown);
   - otherwise do what 0xB0400 does.
4. S1a mirror: keep the last host block per epoch, with no XP-style freshness window, for Kamo's two reads. No client block write before O1.
5. Notifications: a bounded queue into `lan_story_zone_banner.c`, the one box owner. The chime goes through `SudekiMpStoryCharacterAudioPlaySpot`, with no second 0x17090 detour.
6. Tests: codec, digest refusal, NM silence, no join replay, deferral, exact-image guards, then an owner gate.

**Open gates.**
- G1: what the zone story lists hold. Decompile their filler, then a host log at the ambush.
- G2: whether a client block write spawns or runs scripts. Client spawn trace plus `lan_story_task_trace.c`.
- G3: classifier. Host log of stage delta, box hash and tick at the ambush, at a completion and at an NB start, including refusals and keypresses.
- G4: the client's quest page shows a deferred write on reopen. Owner check.
- G5: decode `PlaySpotCue|S` and confirm the client's bank has the three cues.
- G6–G9: first snapshot after the D6 load; host stages at join; attribution source; a replaced questlog.mao.

**Owner decisions.**
- O1: the client's story block. Mirror only, raw write, or native SetStoryBlock?
- O2: how "who did it" is shown and localised, and what "(character only)" means (UNKNOWN).
- O3: whether clients get the default quest page.
- O4: queue policy and replay at join.
- O5: whether the world-map mask is mirrored.
- O6: test point and pairing (§1 host Tal, or live Ailish host / Tal client).
- O7: whether a box the host refused counts as silent.
- O8: whether S1 waits for S0.

**Depends on.** D6/#72; the zone-banner and spot-cue owners; H1b Kamo; the D10 menu gate; S0 privacy; #65 XP; D2/H1a for attribution; coordination of the protocol bump. S2–S4 and §10 E3 reuse the host block. Agree ownership of `lan_story_runtime.c` before editing it.

**Route relevance and effort.** Medium. Not a movement or combat gate, but D12 is binding. After the host passes 2000, Tal's Kamo greets at the stale tier and Tal's journal stays as it was at join. S1a about 3–5 days plus an owner gate; S1b 1–2 days of research; chime and attribution 1–2 days (all INFERENCE).

### S2 — Real conversation scripts on the client, shared writes blocked (D12 S2)

**State today.** Research only (9081425, clean tree); nothing built. Kamo's talk on Tal's game is a hand-coded CSpeech/UIDialog replay (`lan_story_shop.c:457-667`). Greeting, shop, chat, farewell hold, speech camera and Kamo's turn are CONFIRMED_LIVE (§9, 2026-10-17); gestures UNKNOWN (none seen on the farewell). No game script runs on a client: its trigger dispatch call (RVA 0x77E24) is held (`lan_story_client.c:30`, `:233-258`), and containment needs an idle scheduler and an unchanged task counter (`:457-463`). The shared step seam's admission has no consumer (`lan_story_task_trace.h:16`); the opcode slots are host-only (`lan_story_runtime.c:2128`, `:3316`; `lan_story_cast.c:455`). #28 forbids uncontrolled client script execution; no S2 issue exists.

**What the game does (evidence).**
- The dispatch 0x40C6E0 submits OnAction(theTrigger = Kamo, theActivator = *(record+0x34)) through 0x5C37B0 (EAX descriptor, seven stack arguments, RET 0x1C), passing manager *(gel+0x30), context *(gel+0x34) and rate trunc(speed × 1024). CONFIRMED_STATIC (0x40C984-0x40CAEB).
- The manager update 0x5C32A0 advances each task's clock, walks the list with next = *(task+8), budgets the steps and retires on result 1. Its only caller is the GEL node update (0x425E36), and dead-handle cleanup 0x5C3A00 runs only from it. CONFIRMED_STATIC.
- Retire 0x5C3AC0 also runs from kill-by-handle 0x425F40, an entity's retire-all 0x4FCB40 and code unload 0x5C3620 (CONFIRMED_STATIC). These skip EndInteractionScript's undo of AI control, banks and dialog (INFERENCE).
- Kamo's closure: 31 functions, 52 bindings, no opcode-0x29 children (CONFIRMED_STATIC, scratch decoder). Submissions inside natives are unaudited (65 call sites of 0x5C37B0).
- StartInteractionScript refuses unless group slot 0 (GetGroupPlayers, then GetPlayerGroupByPosition(0)) is thePlayer. It overrides both actors' AI (0x4EC350 writes controller state only for the controller's own entity), loads bank 5 for both and yields until both are loaded. CONFIRMED_STATIC. Tal's bank on a client: UNKNOWN.
- Both actors turn, then wait on AiAtTargetDirection, an AI goal slot nothing updates on the paused client (INFERENCE).
- Today's drain answers whatever choice is highlighted (`lan_story_shop.c:622-632`, CONFIRMED_STATIC). Seen live only after the farewell; never from an open question.
- Admission sees every step, including the client's own menu tasks (private live logs, conclusions only). A WAIT on an untracked task quarantines the registration (`lan_story_task_trace.h:31-38`). CONFIRMED_TEST was last recorded on dirty 4370296 (plan:5778-5786).

**Smallest correct design.**
1. Kamo only, default off, the replay as fallback. Decode and pin the closure at install.
2. One client-side admission consumer in task trace: lineage tasks go through the policy, other tracked tasks RUN. New task-trace APIs: owned step, method receivers, pre-step stack read, post-step REWRITE and ANSWER. Each one amends a module contract.
3. Submit with the corrected arguments and run_now 0. One lineage per root, with exact task-count accounting shared with containment.
4. A supervised stepper under manager+8, with next = *(task+8). Re-check the runtime globals and the writable opcode table every frame.
5. Policy: UI, CSpeech, shop and read natives ALLOW, GetGroupPlayers included. Actor writes RUN on Kamo only. On the player's replica, ANSWER them (no turn, no AI override, bank loaded) pending O1 and RB-A. Anything else WAITs and quarantines.
6. REWRITE the front check to the local character. ANSWER Kamo's facing wait from the local turn, capped at 1 s.
7. Never release the world pause while a lineage task lives. Refuse a submit during a follow or a house reload. Re-prove Kamo every frame; quarantine if he is lost.
8. Drain by a positively selected exit answer, never the highlighted one. Use no kill path.

**Open gates.**
- RB-A, client log-and-run build: wait sites, Tal's bank, AI goal slots, task delta.
- RB-B, host log-only run of vanilla Kamo: clock, budget, REWRITE value type.
- Static: method receivers by vtable; UIDialog cursor kinds for exit answers; 0x4EC350 on a replica; trigger listeners 0x526610 and the +0x294 guard; entity replacement under a parked task.
- Live: handle-node growth over 20 conversations.

**Owner decisions.**
- O1: player-side effects (turn, AI override, bank), and on whose screen.
- O2: host disconnect mid-conversation. O3: an unknown native or a lost entity: hold or close, and how long a house exit may be held.
- O4: remove or keep the replay. O5: gestures required? O6: a host cutscene during a private conversation. O7: Kamo only, or every shopkeeper.
- O8: amending the shared task-trace contracts. O9: handle-node growth. O10: native kill as a last resort.

**Depends on.** The S0 gesture owner check; S1 for the story block (soft); the task-trace and containment owners; the shop lease and house holds (#83, #86); D2/G for the front rule; #87 for other pairings; #28.

**Route relevance and effort.** Low to medium. Kamo already works through the replay; the §1 route needs no client script (INFERENCE). S2 is the engine for S3 and S4. About 3 weeks (INFERENCE): 3-4 days of research builds and static gates, 8-11 of implementation, 2-3 of tests, then the owner gate.

### S34 — D12 S3 flavour talk for everyone and S4 side quests by any player

**State today.** Nothing for S3, S4 or S0–S2 is built. No source replicates the story block, quest stages or NPC flags; the only client read is the Kamo shop's own frozen block (`src/hooks/lan_story_shop.c:371-372`, `:814`). The hand-coded Kamo talk is the only client conversation (CONFIRMED_LIVE, plan = `docs/coop-story-plan-54.md`:6732). Host talk privacy covers Kamo (`src/hooks/lan_story_shopkeepers.h:12-14`) and viewers inside their own shop visit (`src/network/lan_party_story.c:110-120`), both CONFIRMED_TEST (plan:6724, :6727). Private live logs (conclusions only): no client conversation on 2026-10-18; quest changes UNKNOWN (nothing logs them).

**What the game does (evidence).**
- An OnEntry trigger fires only for the front character, who becomes theActivator (CONFIRMED_STATIC, RVA 0x123D30, 0x124320).
- OnClusterEntry has no activator; its argument is "first visit" (CONFIRMED_STATIC, RVA 0x1394A0). Slot-0 cluster tracking is CONFIRMED_LIVE on the client (plan:120), INFERENCE on the host.
- NPC flags are a 5-slot CEntityLocalData component saved with the zone (CONFIRMED_STATIC, RVA 0xCB7C0, 0x10BEB0).
- On a missing key GetKeyValue adds one without a bound check (RVA 0xCB850); RemoveKey and IsKeySet index slot -1 (RVA 0xCB750, 0xCB4C0). CONFIRMED_STATIC.
- QuestLogSetStage refuses stage ≥ entry+0x74. It sets menu bit 0x10 when the stage is ≥ 0 and byte +8 is clear (RVA 0xB0400). CONFIRMED_STATIC; what byte +8 means is INFERENCE.
- AddExperience pays one character (RVA 0xD9BF0); AddExperienceToParty pays 4 members, or 1 when a flag bit is set (RVA 0xD99B0); SubtractMoney has no floor (RVA 0x21CB0). CONFIRMED_STATIC.
- The 120 NPC roots split into 47 S3, 41 S4, 13 services, 9 story and 10 special (census, INFERENCE). 20 of 54 quests have several writer roots, e.g. 0x6DE10317 from eight Cargo_Quest_Stuff NPCs (CONFIRMED_STATIC).
- Host-matched NPCs on a client follow host frames, and the paused client runs no AI (`src/hooks/lan_story_world.c:227-231`, `:248-253`; plan:6803). CONFIRMED_STATIC/LIVE.
- The client save path is unguarded: the saved-story start returns at `src/loader/dllmain.c:1317`, before any save hook. CONFIRMED_STATIC. Whether a client can reach the save menu is UNKNOWN.

**Smallest correct design.**
1. Build on S0–S2; S2 installs the opcode adapter on the client seat (host-only today, `src/hooks/lan_story_runtime.c:2117-2129`).
2. Admit a root before it runs, by its static native closure. Story and special roots get the host-lead banner.
3. Prime mirrored state natively: quest stages within QuestLogSetStage's bound; the story block by direct write or a swap through the single routing slot (`src/hooks/lan_arena_cast_context.c:276-287`); flags by host snapshot, rewriting the proven 5 slots, never a missing key.
4. Add one policy callback in `dispatch()` (`src/hooks/lan_arena_cast_context.c:548-568`). Actor natives touch only the leased NPC. The client suspends that NPC's host-frame apply during the talk and turns it with the local Kamo-style path. Anything else is logged and neutralised, never force-cancelled.
5. S3 flags stay local per game, with no lease; they stay unsaved only once the Phase I save guard exists.
6. S4 host leases key on NPC, quest index and flag; LEASE_GRANT carries the values the root reads. Host talks take leases through a veto on the enqueue hook (RVA 0xD951) owned by `src/hooks/interaction_provenance.c`, not installed in saved-story today.
7. Commits are idempotent per session, conversation and sequence number. The host applies them: XP through AddExperienceToParty or AddExperience, money after checking its own purse, quest items as one host copy, florins and consumables through the D9 copy path.

**Open gates.**
- Flag entity identity and component offset: zone census, Ghidra.
- Story-block readers (RVA 0x10A070, 0x149EB0) before write vs swap: Ghidra.
- Per-root read/write sets; scripted animation on the leased actor only: census, S1 observer.
- Host veto keeps the trigger manager consistent (RVA 0xD9A0): Ghidra, live banner.
- Natives outside a GEL task; the AddExperienceToParty flag bit: Ghidra, host build (#65).
- Client NPC pose during a private talk: pure test, live Yemi talk.
- Client reach of the save menu (RVA 0x84F10, call RVA 0x81757): Ghidra, Phase I guard.

**Owner decisions.**
- Quest items: one host copy only, or display copies too?
- Rewards from the host's own vanilla quests copied to every game?
- Flavour flags committed to the host, or kept per game?
- Lease scope: per NPC or per quest?
- Payments and hand-ins: speaker's game or host?
- Special roots host-lead only? Client OnEntry flavour talk?
- Committed per-root table, or computed at runtime (D11)?
- Flag commits for zones the host has not loaded?
- Different NPC facing per screen acceptable?

**Depends on.** S0, S1, S2; the D9 loot copy; Phase I saves (save guard, re-send, sidecars); #65; D2/G lead rules; #28; #83; #88.

**Route relevance and effort.** Low for #54 acceptance: route story writes come from front-only triggers and host-lead clusters; before Ailish joins (block 1400, census INFERENCE) the client has no character. Yemi is the one S3 touch point. Effort large (INFERENCE): S3 2–3 weeks, S4 3–4 weeks, gates 1–1.5 weeks, excluding S0–S2.

### tutor80 — #80 clients spectate the trainee during tutorials (and the #79 remainder)

**State today.** Nothing for #80 is built at 9081425. In Ailish's weapon training (Ailish host, Tal client, 2026-10-16; CONFIRMED_LIVE, private live logs, conclusions only), the existing cinematic share gives the client the host camera for the whole letterboxed intro (lines 1–7, about 39 s). The client then returns to its own camera for the about 84 s `BS_AilishTrain` span and replays the three spawn-group starts, but the five barrels, the tutorial chest and the dispel cart exist only on the host. Case 2 (client trainee) and case 3 (uncontrolled trainee) have never run. #79: the alpha relay moved a same-identity chest's shimmer only before the tutorial; no fade to 255 or marker hide has been seen live.

**What the game does (evidence).**
- The Hilltop cluster entry `0xB3A537B8` runs `runailishtutorial` after a `GetPC(3)` test and `IsStoryBlockBefore(1500)`; first it sets block 1500 and encounter timer 5000 (SOLWORLDM.gex logical 0x68D8C–0x68DD2). The castle cues `BS_TalTraining` and runs `RunTalTutorial` under `IsStoryBlockBefore(1000)` (0x66CF7–0x66DB0). CONFIRMED_STATIC; what the `GetPC(3)` test checks is INFERENCE.
- `AilishTutorialTSA` makes Ailish the pending lead (0x5EE8B) and turns switching off. At its end it sets UI lead-only and overrides Tal's AI. The tutorial parks Tal, cues `BS_AilishTrain` (its only reference), runs four stages and ends on `BS_IllumCountry`. The dispel callback removes the barrier itself once the host's cart is dispelled (0x5F6CC–0x5F764). CONFIRMED_STATIC.
- `SetAsLeadPlayer` (RVA 0x24E40) stores the pending lead at group+0xC0, and the group update rotates at RVA 0x231E4. Our death hold skips that rotation for a client-driven lead without clearing it (`src/hooks/lan_story_death_hold.c:17-25,49-55`). CONFIRMED_STATIC; that Ailish never leads in case 2 is INFERENCE.
- Group+0xD7 is set and cleared inside each stage (six sites, the last at 0x55C84) and also by a spin-attack hint, so it is not a clean signal (CONFIRMED_STATIC; clearing at each stage's end is INFERENCE). Fourteen D7-gated dispatchers fire only for slot 0; RVA 0x17EB70 fires for any actor (CONFIRMED_STATIC).
- Stage checks read the host's input listener, quick menu, camera and dispel state. The story actions have no reveal or dispel request (`src/network/lan_story_handoff.h:71-93`). CONFIRMED_STATIC.
- The share publishes host matrices only while a letterbox is up and the host view is valid (`src/hooks/lan_story_runtime.c:667-691`, `src/hooks/lan_story_replica.c:256-295,617-646`). The client still renders its own world and skips host-only identities (`src/hooks/lan_story_world.c:2371-2375`). CONFIRMED_STATIC.

**Smallest correct design.**
1. A pure predicate: from the host score (an unknown sample keeps the state), host lead, local character, area and epoch, compute active, trainee and spectate. `BS_AilishTrain` maps to Ailish.
2. Host: feed it from the effects capture (`src/hooks/lan_story_runtime.c:3392-3397`); research builds also log D7/D6 and stage-callback edges.
3. Host hold: add a tutorial term to the three remote admission predicates (`:1481-1499`) for non-trainee players. Ownership is retained, and resuming needs a fresh packet. AI override edges and the lease drain/rebind stay native.
4. Client: compute `shared` before `WorldPrepare`, then clear `independent_view` and set `apply_host_view` (`:655-656,677-678`). Keep sharing through frames without a valid view. On exit, restore through `ReplicaRestoreView` and request a reseat (`:724-751`). Hide the combo HUD while shared.
5. Client input: send zero movement and no melee, block or ranged input, keeping the heartbeat (`:2741-2762`). This is new: today's share relies on the host's native hold.
6. Exit on a positive score change, an area or epoch change, a lead change, or retire (`:3632,3851`). No hook, timer or wire change.

**Open gates.**
- G1/G9: a live case-1 rehearsal from a walked start, logging score, D7/D6 and callback edges; check the about 2-minute share and its reseat.
- G4: case 2 in co-op (needs #87). Expect `scripted_lead_held` to repeat; check the dispel stage and the barrier removal.
- G5: static decode of every D7 reader and of how reveal and dispel are issued.
- G6: decode the `GetPC(3)` test and the NG story blocks to find which leg triggers the tutorial.
- G10: find which same-identity chest the host publishes, then run the owner's fade-to-255 check.

**Owner decisions.**
- O1: spectate without the tutorial objects, or present them (reverses #80's scope)?
- O2: case 2: wait for G and task bridging, or depart from "the trainee's player plays"?
- O3: case 3: may the host control the trainee (conflicts with D2)?
- O4: M/Q menus during spectate.
- O5: a derived score signal or a wire field.
- O6: are Elco's and Buki's tutorials in scope?
- O7: confirm that non-trainee bodies are held.
- O8: relay or omit the host's task list and hints.

**Depends on.** G (case 2), F2 (host camera on a client's trainee), #87, #90 (client aim), E3 (same handler) and J (leg UNKNOWN).

**Route relevance and effort.** Case 1 is on the tested return leg. Four starts are live: the two walked runs completed, and the two `TestStartZone` runs stalled. There it is a presentation defect, not a blocker. On the NG route Ailish's tutorial is case 2, which may stall before the barrier goes (INFERENCE). Effort: case 1 small to medium; case 2 large; presenting the objects medium.

### birds81 — #81 Flock of birds frozen on the client: same-identifier scenery is left out of the world relay

**State today.** Not in plan §9/§10; nothing built. At HEAD 9081425 (clean tree) the world relay matches scenery by kind and native identifier only. While two same-identifier copies are resident, each machine leaves both out, and on Tal's paused game they never move. The flock `GO_ICO_dogleg_birds` (3fdb16e3) has three authored placements: A in the New Brightwater–SE connector (over the tunnel), B in countryside SE, C in countryside NW (over the castle-road ambush).

**What the game does (evidence).**
- Drop rule: a second same-identifier scenery target removes the first, later ones are skipped, party characters still refuse the frame (CONFIRMED_STATIC, `src/hooks/lan_story_world.c:985-1013`). The issue's "16 times" is a per-session log cap (:1003); catalog dumps stop at 8 (:675), pose lines at 600 (:694).
- In the issue's session A was relayed until B registered, within the first half minute; for about four hours, until B unloaded, neither catalog held 3fdb16e3, then A rejoined through `client_rebind` (CONFIRMED_LIVE, private live logs, conclusions only).
- The client world is held paused (`src/hooks/lan_story_client.c:18-23`) and the ambient adapter steps only renderers no registry entity owns (`src/hooks/lan_story_ambient.h:12-15`, `src/hooks/lan_story_world.c:284-309`) (CONFIRMED_STATIC). At the rejoin A's client clip time was within a second of its value at the drop; native stepping grows it far past the 600-frame clip (CONFIRMED_LIVE). That A is the flock the owner saw, after B arrived: INFERENCE.
- Host view after B: UNKNOWN. The host stepped A while Ailish was in New Brightwater; once she entered SE the connector was background state 2, and #82 found host entities outside the leader's area paused.
- For looping clips the host sends phase, written as the client's row time (CONFIRMED_STATIC, :1067, :2727).
- Other duplicates per frame: SavePoint at the tutorial start (four clips, selectors 0-3); SavePoint, a road sign and `GO_ICo_Falling_Rock` at the castle-road start (CONFIRMED_LIVE). About 11 entities in all, five of them rocks: INFERENCE.
- Hidden hazard: if each machine holds a different single copy, the client binds them and writes the host copy's position into its own (CONFIRMED_STATIC, :2384-2414, :2676-2677; not seen live).
- Frames need strictly increasing kind and identifier and every actor field has a checked meaning, so a second key is a protocol change (CONFIRMED_STATIC, `src/network/lan_story_world_frame.c:35-95`).
- A scenery-to-authored-spawn resolver exists with no production caller and refuses while any resident zone loads (CONFIRMED_TEST, `src/hooks/lan_story_collision_owner.c:211-273`).

**Smallest correct design.**
1. Stage 1, a cosmetic client clock: client `catalog()` keeps up to 16 fully proven dropped scenery targets.
2. Each frame, inside the apply bracket, a kept target qualifies only with kind 0x0F8F, one channel and clip, selector 0, state exactly 0, rate in (0,120], not hidden, not in `client_bound`, and `target_still`/`target_chain_still` before and after. A failure skips that target only.
3. Write only time = phase + rate × `SudekiMpLanStoryAmbientFrameDelta` through the fenced VisualTimeSet (RVA 0x223180), with read-back. No other write, host or wire change, or new hook; `lan_story_ambient.c` stays sole owner of RVA 0x222B50.
4. Hand-back: a rebound target is relay-driven only; test that frame for double driving and identity refusals.
5. Stage 2, if wanted: a sticky wire identifier from the authored spawn row, today's drop for scenery without a row, a frame reserve as for monsters (:937), the telescope check (:2650) on `native_identifier`, a version bump.

**Open gates.**
- G1 (live, read-only): at the tunnel, Ailish in SE, read A's row time (entity+0x44, +0xB4, +0x10, +0x98 channel array, first dword = row, +8/+0xC) on both machines, with the relayed roof flock as control; the owner names the flock and checks Ailish's view.
- G2: clip data for the rocks and road signs.
- G3, only for a native-Update variant: audit RVA 0x220540 and the child updates under 0x2228D0.
- G4 (Stage 2): identical, stable keys on both machines; a client-side witness and cached-table contract.
- G5: log every duplicate, widen the dump cap.
- G6: peak frame size with duplicates back (57 of 64 seen).

**Owner decisions.**
- May a contained client run a local clock on host-owned scenery? No D1-D12 covers it.
- Which fix: the local clock (out of phase with the host, blind to host hiding and #82 pausing), the placement identity (changes the wire identifier), or both in stages.
- Scope: dropped duplicates or all unmatched single-clip scenery; SavePoint clocked or frozen.
- Stage 2: version bump, frame-budget policy, closing the binding hazard now.

**Depends on.** Nothing pending (E2 is committed at 9081425). Stage 2 reuses the resolver and the placement contract (`src/engine/story_placement.h:8-11`). Related: #42, #82, #79/#80.

**Route relevance and effort.** Cosmetic, no route gate, visible on every #54 run. Stage 1 small (about 60-100 lines, one pure test, G1/G2, one owner look); Stage 2 medium to large, with relay regression risk.

### breakables — Breakables the host smashed still stand on the client

**State today.** Nothing in the mod carries a breakable's destruction to a client; this explains the §9 UNKNOWN (docs/coop-story-plan-54.md:6452-6458). In the tutorial-area run the host's Ailish smashed two authored `Brk_Barrel_Trade_Light`. Each left the host registry about 1.4 s after breaking; the client's copies (same index and name) were still there 1100 s later (CONFIRMED_LIVE, private live logs, conclusions only). Loot from a host smash does reach clients (D9). No issue is filed; #34 holds the shared-breakables contract. The runs used builds of the dirty tree on d437693; 9081425 changes no breakable code.

**What the game does (evidence).**
- Identity: `BreakableEntity` (constructor RVA 0x13B060, called only from 0x16BAD0), kind 0x0F84, CBreakableObject at entity+0x70C, phase at component+0x7C. CONFIRMED_STATIC (src/hooks/lan_story_objects.c:98-117).
- Use path, front only: 0x150FF0 starts the use and loads animation 0xD on the front's model (0xB5EC0); 0x150D70 sets phase 4, rolls loot (0x130570, 0x1FAB0) and expects action 0x19, whose end sets phase 5 (0x150E80). CONFIRMED_STATIC; phase 3→4 CONFIRMED_LIVE for three breaks.
- Damage path: the death handler 0xD31C0 (called from 0xD21D0 and 0xD39A0) writes death action 0x17..0x1A and has its own loot roll; the node update then sets phase 5, bypassing 0x150D70. CONFIRMED_STATIC. Five shot tutorial barrels dropped nothing, but neither did that run's two smashed barrels (CONFIRMED_LIVE).
- Removal: 0x151130 clears collision bit 2 at a death-action start; 0xD3530 (from action end 0xD3080, or directly from 0xD39A0) sets arbiter bit 0x8000, unregisters entity+0x50 from the interaction manager and defers the registry removal only if combat+0x72 bit 0x20 is set. CONFIRMED_STATIC; that barrels take it is INFERENCE.
- Mod: the world catalog has no breakable branch and poses only the shared intersection (src/hooks/lan_story_world.c:449-524, 2371-2375); despawns cover monsters and allies only (src/network/lan_story_effect_state.h:28-35); placement, story_breakable and BREAK loot have no runtime caller. CONFIRMED_STATIC.
- Client: one session re-created a tent pot 994 times, once per client enter of LNBr_TallTent02 (1030 enters; host none). CONFIRMED_LIVE; the loop as cause is INFERENCE.

**Smallest correct design.**
1. Host, read-only, on the post-controller dispatch (src/hooks/lan_story_runtime.c:499-523): phase 4 or 5 on the entity an initial-snapshot row binds (spawn+0x78), under the `entity()` fences, is a destroy. A hook on slot 0x2C86DC in `lan_story_loot_trace.c` (this vtable's owner) only if G1 needs it.
2. Gone: absent from a readable registry on two ticks while its zone is resident (src/hooks/lan_story_spawn_events.h:250-260); absence alone is UNKNOWN, never published.
3. Key: the authored Zone::Spawn placement, never index, pointer, position or ResourceName; spawn-group barrels stay with #80.
4. Wire: the full tombstone set (retired per G4), idempotent and late-join safe, beside `despawned[]` within the chunk budget (src/network/lan_story_effect_state.c:3-4), version bump (D1).
5. Client, game thread: exact key match in a zone resident on the client; entity and collision fences; hide the render object, clear collision bit 2, keep the use prompt off; re-apply after every local re-creation.
6. Never run 0x150FF0, 0x150D70, 0xD31C0, 0x1FAB0 or 0x130570 on a client. Pure and image tests, bounded logs.

**Open gates.**
- G1: does a dispatch see phase 4/5 before removal on both paths; who starts action 0x19 (UNKNOWN). Close with Ghidra callers of the entity action dispatch, or hardware breakpoints on 0x151130, 0xD3530 and 0xD31C0 during one smash and one shot.
- G2/G3: Zone::Spawn +0x78 after removal; equal host and client keys. Close with diagnostic logs and a diagnostic Match.
- G4: vanilla respawn after unload, re-entry, save and reload.
- G5: which collision bit the client camera reads; prompt of a hidden copy; DespawnEntity's death-animation branch is HYPOTHESIS.
- G6: remote damage breaks and drops; the loot trace cannot see a 0xD31C0 drop.
- G7: owner eye check. G8: what the host shows before removal. G9: the client tent enter/exit loop.

**Owner decisions.**
- New issue or #34.
- Vanish at destroy or at removal, or the native effect on clients.
- Persistence after G4 (reload, save transfer, late join).
- Priority of remote smashing (#34).
- Effects field or reliable lobby link (D1).
- Loot rule for remote damage breaks (D5/D9).
- Fix the G9 loop first or tolerate re-apply.
- Nothing extra for the tutorial barrels (#80)?

**Depends on.** #34; `lan_story_loot_trace.c`; `lan_story_objects.c` and story_placement; the effects protocol (D1); `lan_story_spawn_trace.c` / `lan_story_spawn_events.h` (coordinate first); #80; D3/D9; #71 and #42; `lan_story_remote_interior.c`; H1a for remote smashing.

**Route relevance and effort.** Low to medium: Trade barrels, pots and crates stand on the castle/countryside border and in New Brightwater. Smashing is optional, loot already arrives, and stale copies block nobody because client bodies follow host positions (INFERENCE); the desync is visual and not a §10 gate. Replication: medium, about 400-700 lines in five or six files, plus G1-G5 and G9. Remote smashing is a separate medium-to-large item.

### xp65 — #65 XP award and distribution in co-op, and level-up of client characters

**State today.** Research only at 9081425. The host runs the native award, split and level-up unchanged and unhooked; neither peer has logged an XP trace. Clients award no XP (INFERENCE). They get only current HP/SP and the XP popups, so their level, XP, AP and max stats stay at the values they loaded with. Nobody can spend a remote player's host-side AP. #65 is open; docs/combat.md has no XP section.

**What the game does (evidence).**
- Native death 0xD31C0 pays only when its type switch (0xD32E4) admits the victim (party type 0x1D is excluded) and slot 0 resolves (0xD3315). Take-hit enters it at 0 HP (0xD27C7), and so does a deferred kill (0xD39EE, via 0xD1AB0). CONFIRMED_STATIC.
- The reward 0xD9C40 takes no killer. Types 0x19 and 3 pay CStats+0x74 through 0xD99B0 to every filled slot, downed or paused members included. A level-up happens at once only out of combat (0xD9A1C). Allies pay nothing. CONFIRMED_STATIC.
- SetOnlyLeadPCAwardedXP (export 0x307F0) limits that to slot 0. Retail scripts set it in the Shadani Mo trials (GEX 0xB7396), the robot fight (0xD04F0) and the Olivitess challenge. CONFIRMED_STATIC; off-route (INFERENCE).
- At combat end the listener 0x24480 runs DoLevelUpChecks 0x24EF0. DoLevelUp 0xD9DB0 raises Level and AP only; the 'level_up' cue goes through EmitterPlay 0x28AEB0. CONFIRMED_STATIC. In two host sessions at the castle-road ambush, two level-up effects started inside the one-second sample in which in_combat went from 1 to 0: CONFIRMED_LIVE (private live logs, conclusions only).
- Only the front's kills charge Spirit Strike points (0xD3375). CONFIRMED_STATIC.
- GEX AddXP and the quest wrappers pay the whole party; three Solo sites pay slot 0. Route grants: castle-road entry 30, FarmerTSA 30, Ailish_TetsuTSA 20, oar 25, Mungo 50 (0x69D3A), Lighthouse 5. Scripts turn the popup byte (0x409DBC+0x80) off at the hilltop (0x3F782) and on after Mungo (0x6A294). CONFIRMED_STATIC.
- Clients keep monster copies at 1 HP or more (story_monster_damage_guard.h:65-70; CONFIRMED_LIVE, 56 hits, plan:4431-4433) and hold scripts (lan_story_client.c:18-23, 233-290). Only hp/sp travel (lan_story_frame.h:83), applied at lan_story_runtime.c:775-792. The level-up effect is not whitelisted (lan_story_world_resources.h:5-116). CONFIRMED_STATIC.
- The AP purchase 0x1754B0 (it calls 0xDA070 with the index in ESI) is not hooked. The host menu skips an observing peer's character (lan_story_runtime.c:2280-2290). CONFIRMED_STATIC.

**Smallest correct design.**
1. No host hook on the award path (its natives are also script exports); observe by sampling.
2. Write the rule into docs/combat.md; update status-matrix :94 after G1; correct the 0x409DBC class name in a new research-log entry.
3. Add an observe-only probe in Research54 sample_party (lan_story_research54.c:332-360). It logs Level, XP, AP, max HP/SP, group+0xD4 and the lead-only bit on both peers, plus a host `story_xp captured` line.
4. O2: an additive host→client progression snapshot. The party-resources mirror applies it with named SetCharacterNumberStat writes, max before current. No client AddExperience or DoLevelUp beyond the bounded recruit replay (lan_story_recruit_setup.c:207-217).
5. O3: one seam at 0x1754B0 in the main-menu adapter. The client sends a request through queue_client_action (lan_story_runtime.c:2306); the host buys under register-ABI contracts.
6. O4: whitelist SFXLU001_LEVEL_UP; play the cue through the character-audio owner.
7. J: carry the host's popup byte if G10 finds drift.

**Open gates.**
- G1 live: the probe, plus one castle-road wave with kills by each source and with a member downed. Then one AddXP site reachable from the current saves (UNKNOWN which).
- G2 static: the class behind type 3 (Mungo's kill XP).
- G3/G12: the Advancement page and whether a client can reach it; one convergence rule shared with skills75.
- G4 live: the HUD bar overfills when HP exceeds the local max.
- G5 static: what 0x17E860 and the CStatDisplay+0x1B0 flag do.
- G6 static: which deaths take 0xD1AB0.
- G7 closed statically (2026-10-19): on the supported GOG image
  (`docs/executable.md`), missile initializer 0x586E10 resets packet+0x30 to
  the firing entity at 0x58729A–0x5872AB. The attack-area path 0x4CC150 stores
  the casting character there too. CONFIRMED_STATIC; see the hit-feed research
  in `enemy-ai-task.md`. Live attribution through the shared HP observer
  remains a watch-build gate.
- G8–G10: quest floaties, simultaneous kills (plan:4359), and whether the popup byte persists.

**Owner decisions.**
- O1: keep the native rule for co-op.
- O2: should client progression follow the host, or keep the save's values?
- O3: AP spending: by host request, locally, or blocked now.
- O4: the level-up effect and cue on clients.
- O5: after G and D12 S4, does Solo and trial XP go to the front or to the human player?
- O6: Spirit Strike points from the front's kills only, or from any human's kills.
- O7: the host save as the only progression source (D6, D9).
- O8: is Mungo or Lighthouse quest XP a D12 S4 change the host applies and notifies?

**Depends on.** G (lead rotation), D12 S1/S4, I/#72, #75/#87/D10, E3 test access, J, the D1 clamp, and skills75 (same page, snapshot and feedback: build once).

**Route relevance and effort.** Medium-low as a blocker, medium for faithfulness. On the tested route (Ailish host / Tal client) the client's Tal keeps the save's level while the host's Tal levels up (INFERENCE). On the acceptance route Ailish joins at level 3 after Mungo and misses that XP (INFERENCE). Closing #65 is small: docs, one probe and one owner wave. O2+O3 are medium and shared with skills75; the register-ABI proof is the risk.

### bright59 — #59 intermittent overbright flashes on enemy surfaces

**State today.** The owner's report is the only evidence: in the 2026-10-11 New Brightwater co-op test an enemy's surface briefly turned very bright several times, like a strong flashlight. Screen, enemy, trigger and vanilla baseline are UNKNOWN. The issue has no comments and no §10 row. The plan scheduled it with #57 but never researched it (plan:2514-2528, 2577). The sighting most likely came from that night's road-ambush sessions (INFERENCE). This section rests on static and log research at 9081425 only.

**What the game does (evidence).**
- Native flash: hit routine 0x4D21D0 arms 0x4DFFB0 only when the type's "Should flash on hit" bit (combat+0x72 bit 0x4) is set and HP changed. The length is "Flash duration" (combat+0x6C, loader 0x4D1940). The only other arm is FlashEntity 0x4E0190, a script export. CONFIRMED_STATIC. Whether Kurian and Grotesque types set the bit: UNKNOWN.
- Shape: 0x4DFAB0 rises to a clamped 1.0 in about 83 ms, then fades linearly. Colour = intensity × CCombatGlobals (0x808DD0) +0x9F0..+0x9F8. Constructor 0x42F8C0 defaults to opaque red, and loader 0x4302E0 loads RGB only. CONFIRMED_STATIC; the loaded RGB is UNKNOWN.
- Client lease: copies skip the native entity update. So the client replays host HP drops through DoDirectDamage, and ticks 0x4DFAB0 itself after each successful present (lan_story_world.c:2822, 2904-2950, 2959-3017; lan_story_runtime.c:1361-1364). dt is wall-clock time, capped at 50 ms, and set to 16 ms after gaps of 200 ms or more. A slot ends 5 s after the last replay or on lost identity. The wire carries no flash state. CONFIRMED_STATIC. The flash can never exceed the native peak. It can merge hits, hold through present gaps (lengthening it), or stay lit if its slot drops mid-flash. CONFIRMED_STATIC consequence; none observed.
- Gaps happen: the last session had eighteen 0.56–0.78 s gaps before the fight, and one 0.8 s gap 2.7 s after the only monster hits. CONFIRMED_LIVE (private live logs, conclusions only).
- Hit effect: the replay's generic record (0x585530: no hit resource, type 0, no impact flag) makes 0x4D29D0 pick a hit-matrix default at the copy's root. On the client SFXHT000 appeared at root height. The host's SFXHT201 never reached the client (excluded by design, tests/lan_arena_spirit_visual_host_test.c:118-120). The host also showed SFXHT000 with no monster HP drop. CONFIRMED_STATIC + CONFIRMED_LIVE (samples). Whether any of it looks overbright: UNKNOWN.
- Lighting: 0x41F350 fades scene-wide without overshoot. The 10-10/11 client logs show five targets and no replayed light calls. CONFIRMED_STATIC + CONFIRMED_LIVE. Not a per-enemy cause.
- The aimed-fire capture failure (about 2 s) was fixed at 9081425 (story_aimed_flash.h; CONFIRMED_TEST, live view pending). Its role in #59: HYPOTHESIS.

**Smallest correct design.**
1. Add an observe-only research switch, off by default. On both peers, log per proven combatant: flash state (anim+0x68), lease dt, present gaps, slots dropped while lit, nearby hit effects (host-only included) and light changes. On the host, log the damage record's resource, type, flags and impact point inside the existing 0x4D21D0 observer, after widening its party-only witness (lan_arena_hit_feedback.c:89-100; the arena shares this observer). Add a pure classifier test.
2. If host and client timelines match and vanilla shows the same tint, it is the native flash: change nothing.
3. Fix B: exempt a lit flash from the 5 s cap. If gaps lengthen flashes, advance by the measured gap. Never write intensity.
4. Fix A: a host per-hit journal. The client then builds a native record with the host's resource and impact point and runs it through 0x4D21D0 under the existing guard. Needs new 0x585530/0x585960 contract items and a protocol bump.
5. Fixes C–E only if their gates fire: a host flash serial; projectile retire on observed removal; a light-group bound of 8 (lan_story_sky.c:215, 535).

**Open gates.**
- G1: the owner's account (screen, enemy, action, colour, duration).
- G2: a paired capture with step 1 plus window recordings, classifying each bright event.
- G3: a vanilla recording of the same encounter.
- G4: the loaded RGB and per-type +0x6C/+0x72, read live or parsed from data.
- G5: the hit-matrix entries, and the fields of the host's attack records.
- G6: material colour outputs on Kurian clips.
- G7: retire latency after the lightning view check.
- G8: counts of held or stranded flashes.

**Owner decisions.**
- The G1 details, and whether to run G2–G4.
- Whether to accept the native flash and close #59 if vanilla matches.
- Should the client show the host's attack-specific hit effects? That changes presentation accepted under #55.
- Should Fix A or C get a protocol bump?
- Does #59 block route sign-off? Do gap-held flashes belong to #59 or to #78?

**Depends on.** #57; #55 (regression); E1 hit sparks; the #58/#63 vanilla runs; the D5 flash gate (no live flash acceptance recorded); #78; the lightning view check; the owners of the 0x4D21D0 adapter, damage guard, gesture hash and sky replay.

**Route relevance and effort.** Medium-high for the Illumina → New Brightwater → Illumina sign-off: the owner named #59 among the last observed problems. Cosmetic only. Capture is small to medium: one observer, one paired session, one vanilla run. B and E are small, C is medium, A is medium to large.

### gates — Special scripted barrier variants and the remaining barrier families

**State today.** One adapter, `src/hooks/lan_story_gates.h`, replicates gates made by native InternalSpawnGates (RVA 75F90) for six named families (lines 75-87). The host captures them every 100 ms into the effects message (`src/hooks/lan_story_runtime.c:3380-3397`). The client creates and retires replicas inside ClientResidencyCall (`lan_story_runtime.c:3443-3452`). CONFIRMED_LIVE (private live logs, conclusions only): the castle-road group HC_NW_Road_AklorianGate was created on both games in 5 sessions and retired on both in 4. The Hilltop group HC_SE_Boss_AklorianGate was created in 4 and retired in 2. Every session had serial 1. The 2026-10-11 "New Brightwater vine" (plan:3389-3398) is most likely this castle-road group, because NewBrightwater has no gate template (INFERENCE).

**What the game does (evidence).**
- Only HC_AklorianBarrier is configured on the route. The other five families belong to Devil_Belch, Nassaria_Grotto, Outpost, Temple_of_Mo and Transentine_Research_Center. CONFIRMED_STATIC for the 82 field values in SOLData.baf. Assigning each field to the following zone record is INFERENCE, supported by each of the five prefixes landing on its own zone (plan:3265-3287).
- Every route group runs HC_AklorianBarrier.sol: road 1 point, farm 3, Hilltop 2, the NB and Beach barriers 1 each (CONFIRMED_STATIC, BAF records). Hilltop OnClusterEntry and AilishTutorialTSA spawn the Hilltop gate. MungoDeathTSA, TetsuTSA, the tutorial dispel paths and HC_SE_Boss_Random's handlers retire it (CONFIRMED_STATIC, SOLWORLDM.gex). The live Hilltop sessions were the tutorial (INFERENCE: only tutorial groups started).
- Kill (CONFIRMED_STATIC): KillSpawnGroup RVA 75870 tail-jumps to 74500, which calls point kill 156160 per point. The main branch removes the entity from the registry at once (3E840), queues entity+2C on the ResourceManager remove queue (registry+25C) and clears the point (156100). Two branches skip the queue: an entity the loader is still processing, and the entity+144 bit 4 path with entity+90 flag 4000 or 10000.
- Pump 3E250 runs only if it can enter registry+14C. It destroys an entry only when slot 18 (13DD40) reports ready. For the gate class, slot 1C is the return-false stub RVA 51310, so gates always take the destroy branch. Each erase makes the next entry wait one pass (CONFIRMED_STATIC).
- The lag: until destruction the lease still holds the object. Host capture fails (`lan_story_gates.h:207,235`), effects carry gates_valid=0 (`src/hooks/lan_story_area_follow.c:419`), and the client keeps its replica (`lan_story_gates.h:287`). The path is CONFIRMED_STATIC. Six samples are CONFIRMED_LIVE but approximate: 0 to 4.5 s host lag, then about 50 to 260 ms to client removal. The cause of the 2026-10-11 3.6 s figure is INFERENCE.
- Repeat race: creator 737E0 refuses only while point 0 has flag 800, and point kill clears that flag at once. A respawn can therefore meet a queued old occurrence, and the host stays unknown (`lan_story_gates.h:202-203`). UNKNOWN live.
- Ship and bridge barriers are ordinary spawn groups off the route. Their starts are replayed (`lan_story_spawn_trace.c:242-285`), but nothing replicates their retirement (`src/hooks/story_spawned_actor.h:17-20`).
- Teardown: `gates_uninstall` only unbinds (`lan_story_gates.h:155-160`). Replicas keep their update-node pause count and their points, so later creation is refused (lines 308-309).

**Smallest correct design.**
1. Host predicate in `lan_story_gates.h`, used at lines 207 and 235. A gate is retired only when all hold: the lease link is exact; the object is absent from a readable non-empty registry; the point is unlinked with flag 800 clear; group lease and zone hold; entity+2C appears once in the queue with count and data unchanged; this held on two captures in a row; the call is on gate_thread. Then unbind and publish. Anything else, both bypass branches included, stays unknown.
2. Extend `gate_native_code` with the chain identities in `src/hooks/lan_story_recruit.c:21-39`, the tail jump at 758E1, and vtable 2D5B44 slots 18 and 1C.
3. A respawn then uses the existing path (`lan_story_gates.h:196-205`); add a test.
4. Follow continues past a lease it could not retire (`lan_story_gates.h:294`).
5. After O2: retire owned replicas when the host leaves OBSERVING, and refuse uninstall while one is live.
6. Tick logging, and new cases in `tests/story_spawn_timing_image_test.c:553-617`.

**Open gates.**
- G1 live probe of one host retirement.
- G2 frames around removal.
- G3 static audit of other kill paths and the bypass branches.
- G4 repeats. G5 area change. G6 disconnect and rejoin.
- G7 Mungo (blocked on E3).
- G8 gex conditionals.
- G9 portcullis (H/#67). G10 movement blockers (C/D2).
- G11 which threads write the queue.

**Owner decisions.**
- O1 fix the lag now or later, and the tolerance.
- O2 after a disconnect, play on or return to the D6 prompt.
- O3 Mungo test access.
- O4 off-route families and spawn-group barriers.
- O5 blockers triggered by a remote player.
- O6 rename the vine in the plan and status-matrix.md:83.
- O7 may "retired" mean queued for deletion?
- O8 may a probe enter the native critical section?

**Depends on.** The E2 death-stall fix (committed in 9081425, live check pending); the recruit owner, for a shared queue scan; `src/hooks/lan_story_area_finalise.c`, which owns the pump hook; J or a pre-Mungo save; H/#67; D6/I; C/D2.

**Route relevance and effort.** Medium. The route's gates are the supported family and already work; their lifecycle remains open. The core fix is small (about a day with tests); it becomes medium with teardown and live checks.

### receipt — D1 death receipts follow monster type and death style; no intermittent loss

**State today.** At 9081425 the host logs `death_emitted` only when the native animation emitter (RVA 0xE2810) returns an exact, registered effect that matches the leased monster's authored death entry (`src/hooks/lan_story_spawn_events.h:107-145`). We re-read 14 retained host/client run pairs (2026-10-12 to 10-18; private live logs, conclusions only) and found no loss after a receipt. The host logged 360 receipts and the client 351 presentations and 351 created visuals. The other 9 are farm copies the client never built. `docs/status-matrix.md:90` still calls the miss intermittent, and the 10-13 audit named the wrong monsters (`docs/coop-story-plan-54.md:4434-4440`).

**What the game does (evidence).**
- Ambush types: styles 2/4 always receipt (337/337), styles 1/3 never (0/74). Gate guards: 0/4. Farm/beach 662fb26c and 47e00867, styles 2/4: 21/21. Type 5b14180b (farm and gazebo randoms) never receipted (0/73, including 12 style-2/4 deaths). Those 10-12 builds had no refusal log, so the cause is UNKNOWN. CONFIRMED_LIVE (log re-read).
- The audit joined `despawn_recorded` on the despawn counter, not on `identity=` (`src/hooks/lan_story_spawn_trace.c:654-669`). The real misses were spawns 9 and 0xA, both style 3. CONFIRMED_LIVE for the counts; INFERENCE for how the join went wrong.
- 279 `death_receipt_refused` lines, all style 0 (spawn-in and hit effects). No unleased, refused, departed_returned or create_unobserved lines. CONFIRMED_LIVE.
- Death selection (RVA 0xD31C0) puts styles 1/3 on damage byte +0x62 = 2 and styles 2/4 on +0x62 = 3. CONFIRMED_STATIC.
- The emitter writes NULL when the event's type byte selects an entry with no name (RVA 0xE42F0 returns 0, or entry+0x2C is 0x7FFFF), when type 0xC0 finds its queue at component+0x120 empty, or when the renderer check (RVA 0x21BC80) returns 0. Entry +0x59 bit 0 and type 0xC0 create an attached effect (RVA 0xBCE90). CONFIRMED_STATIC (exact-image disassembly).
- The receipt's first gate returns silently on a NULL, non-exact or unregistered effect (`lan_story_spawn_events.h:108-109`). So a missing refused line does not prove the death had no effect. CONFIRMED_STATIC (source).
- That the ambush and guard 0x17/0x19 entries have no effect is INFERENCE. No style-1/3 emission reached the identity or resource checks, and the client never created a style-1/3 visual (`src/hooks/story_death_visual.h:41-46`).
- Without a receipt, the client copy still enters the host's death clip: 51/51, with 8 unsampled. CONFIRMED_LIVE, clip ids sampled at about 4 Hz (`src/hooks/lan_story_world.c:1477-1478`). The owner saw both guard deaths match (`docs/coop-story-plan-54.md:6760`).
- E2 departure is not the cause. Departure follows the receipt by 199-301 ms, and the client presents 101-173 ms after the receipt. CONFIRMED_LIVE on log ticks; that both processes share one clock is INFERENCE.

**Smallest correct design.**
1. No replication change, and no invented client effects. Only spawn-event-leased actors have receipts (`lan_story_spawn_events.h:111,158-163`).
2. Docs: add a §9 entry correcting 4434-4440 and 4458-4461. Status-matrix row 90: a receipt is expected only when native emission creates an independent authored effect; 5b14180b stays open.
3. Optional observe-only lines, with no new seam and no wire change:
   - In the receipt observer: one bounded line for a leased actor with style > 0 whose emission is NULL or not exact. The existing adapter (`src/hooks/lan_arena_spirit_visual_host.c:1050-1093`) may pass the event index through.
   - At the host death transition (`lan_story_spawn_events.h:244`): a classifier prediction, NONE/INDEPENDENT/ATTACHED/UNKNOWN.
   - On the client: one line per serial when Prepare refuses style > 0 at retirement (`lan_story_spawn_events.h:334-345`).
4. Audits join on `identity=`.
5. Tests: NULL and no-effect emissions, and the retirement refusal. Prepare's refusals are already CONFIRMED_TEST (`tests/story_spawn_timing_image_test.c:257-270`).

**Open gates.**
- G1: read entries 0x17-0x1A (+0x2C, +0x59) and the death-clip event bytes of one resident monster per ambush type and of a guard. A read-only memory read, or one run with step 3, closes it.
- G2: 5b14180b. Close it with step 3 on a farm/gazebo kill run, or with the G1 read on that type.
- G3: owner check of style-1/3 kills on both screens. Any other corpse visual goes to #63.
- G4: D7 enemies (Grotesques, Mungo) once E3/J make them reachable.
- G5: retirement fallback for style 2/4, checked during the E2 live check.

**Owner decisions.**
- Close the D1 hunt with the docs correction now, or after G1/G3.
- Close D1 with 5b14180b UNKNOWN, or require G2 first.
- Whether attached death effects are in scope if any turn up (#55 refused them).
- Docs only, or docs plus step 3.

**Depends on.** The E2 departure change, committed in 9081425. `docs/coop-story-plan-54.md:6762` still records it as CONFIRMED_TEST with the live check pending; its live run can double as the D1 check. Also: the single emitter adapter, agreed file ownership, #63, #62, the #55 rule, and D7/E3.

**Route relevance and effort.** Low. On the tested fights, every authored-effect death reaches the client, and ordinary deaths show the host's clip. Still open: 5b14180b near the farmhouse, which the route visits (`docs/coop-story-plan-54.md:6794`), and at the gazebo, where route membership is UNKNOWN. Effort: docs under an hour; step 3 with tests about half a day; G1 and G2 one short owner session each.

### creep88 — #88 idle creep, static side: native writers and the anchor seam

**State today.** All creep is measured on host native bodies, and the client copies follow them (CONFIRMED_LIVE):
- Sunder-rah (NPCEntity): about 0.22 mm/s, with a 3.9 mm sawtooth (#88).
- The gate guards (AllyEntity): 0.57 and 1.64 mm/s over 800 s.
- Ailish at Kamo's desk: about 2.4 cm/s, with a 5.3 mm push-out at 4.3 Hz (#84).

Vanilla Tal (2.4–6.0 mm/s) and Ailish (2.4 mm/s) also creep on castle-road ground (CONFIRMED_LIVE, plan §9 2026-10-13). Vanilla NPCs and allies are UNKNOWN. `src/hooks/story_idle_contact.c` covers party members only. On 2026-10-13 the owner left #69 open; #88 reopens it.

**What the game does (evidence).**
- Corrected addresses (CONFIRMED_STATIC):
  - 0x336E7 calls the solver 0x32320 (return 0x336EC);
  - the solver calls 0x110BD0 at 0x329FD (return 0x32A02);
  - 0x110BD0 calls 0x3050 at 0x110C4C when the body is unparented (return 0x110C51). The parented call returns at 0x110C40.

  Plan lines 4409-4410 and #69 give these addresses 0x10000 too high. `docs/research-log.md:7046-7051` calls the 0x110C51 sample parented, which contradicts the code.
- The NPC (0x14C8A0), Ally (0x13EF20) and Monster (0x1460A0) constructors build the same components. All three run 0xC3200 and the same collision solve (CONFIRMED_STATIC; `src/hooks/story_spawned_actor.h:5-10`). Their movement mode and collider flags are UNKNOWN.
- On type-1 terrain, gravity is vertical, and the gravity pass ends at the first hit that is not type 2 (0x3291B).
- The tiny-penetration branch of 0x33010 adds pen/2 + 0.01 along the tilted normal, with no horizontal filter (0x33282-0x332E1). The two sibling branches drop parts under 0.01 (0x334FD, 0x335C4). The plan's slope-gravity explanation is RETIRED for type-1 terrain (CONFIRMED_STATIC).
- That this ratchet causes the creep is INFERENCE. The size fits the Kamo ramp: about 0.58 mm per event. At New Brightwater the recorded normal predicts +z, but the creep is −z (`docs/research-log.md:7718-7720`; plan 4410-4411).
- The wall pop comes from the same routine acting on a wall normal (CONFIRMED_STATIC, 0x334A8). That the pop is 0.01 × the collider radius is INFERENCE.
- The idle AI paths 0xF4BB0 and 0xF4250 do not write CPosition directly (CONFIRMED_STATIC). Some of their callees are unexamined (UNKNOWN).
- The idle-override tracker in 0xC3200 runs only under a native AI hold or mode (CONFIRMED_STATIC).
- No mod writer moves a host NPC or ally (CONFIRMED_STATIC). The adapter logs its line once per process, so the line proves only that at least one correction happened (`src/hooks/lan_story_control.c:190-193`; private live logs, conclusions only).

**Smallest correct design.**
1. The seam is 0x33288, already site 0 of `story_idle_contact.c`. No new hook.
2. For an admitted idle body, zero the normal's x and z (native ESP+0x14 and +0x1C) before 0x33292. The push becomes vertical, and gravity re-grounds the body in place. NPCs and allies keep the 0.01 margin until G1 shows their mode.
3. The separation and backside branches stay native.
4. The NPC/ally admission runs on the host thread only and requires:
   - vtable 0x2D65A0 or 0x2D55D4 (Monster 0x2D5FA0 shares the tick, so this test is what excludes it);
   - registry membership and no remote lease;
   - a delta from gravity only;
   - arbiter mask 0x081812C8 plus Dying, Dead and BeingHit;
   - CPosition +0x94 == 0, which is stricter than today's adapter;
   - terrain type 1, the owner's slope bound, and the current zone.
5. For party members, widen the slope bound and add the zeroing. Ailish was in mode 0 at the desk, and today's ranged rule refuses mode 0 (`src/hooks/story_idle_contact.c:112-118`). So #84 also needs a velocity-mode case.
6. The client is unchanged. Add StoryIdleContactImageTest cases first (CONFIRMED_TEST), then run the live probe.

**Open gates.**
- G1: a 10-minute vanilla probe of Sunder-rah, two more NPCs, the guards, Tal and Ailish, recording mode, flags, radii, AI hold and arbiter state. Repeat with the DLL loaded and co-op off.
- G2: a bounded hardware watch. It closes when the branch hits at 0x33288, 0x334A8 and 0x33567 match the creep's size and sign.
- G3: the writers of the 27 mm step and the 48 mm rise. Candidates: 0x32CE0, 0xC41B0, and the callers of 0xC3500 and 0x3050.
- G4: the slope limit and the floor record types (0xC5CA0, 0xE6740).
- G5: log why the adapter let Tal's New Brightwater creep through.
- G6: write exact-image contracts for the NPC and ally layouts.
- G7: correct the addresses in plan §9 and #69.

**Owner decisions.**
- Leave the creep native, anchor idle actors, or fix a mod system (#88 a/b/c).
- Who is anchored: NPCs, allies, monsters, AI followers, the host lead.
- Whether this applies to saved-story co-op only.
- The slope bound, and whether type-2 floors count.
- What to do about the #84 step-back.
- The acceptance threshold: 1 mm per 10 minutes.

**Depends on.** #69 and #84 (same seam), E2 ally records, and H1b/#82 zone residency. #38 is only a proposal.

**Route relevance and effort.** Low to medium. All the cases are on the #54 route, but most of the creep is too small to see. It blocks no route gate unless the owner adds #88's acceptance. Effort is medium: one or two live sessions, 150–250 adapter lines plus tests, then live acceptance.

## Appendix: review points not accepted as written

Each fixer re-checked every reviewer correction. The points below were kept, narrowed or rejected after that check; the outcome says what the evidence showed.

### F1

- Wrong #1 says G1's callee list must include 0x507A10 as a cluster-manager writer reachable during unload. → Applied the main correction: 0x507A10 (the Zone destructor) writes cm+0x24=0 and cm+0x2E=0xFFFF and leaves cm+0x28; this is CONFIRMED_STATIC from the decompile. Not applied to G1. G1 covers the leader field member+0x40 inside the 0x4352D0 call, nothing shows 0x507A10 running synchronously inside that call, and it does not touch member+0x40. The point is recorded in the lifetime rule (design step 8, resolver re-proof) instead.
- Overclaim #7 says muting the CBattleScores listener is belt-and-braces, because 0x439AD0 is idempotent. → Partly disputed. The decompile confirms that 0x439AD0 stores the danger byte at +0x7C and changes +0x48 only when the byte differs (CONFIRMED_STATIC). But the native 0x4352D0 listener loop passes the client's own cluster danger bit, computed from the client's spawn bits through 0x4748C0, not the host flag that lan_story_music.c applies. The call is idempotent only when the two agree. The mute stays a requirement unless G6 shows the client's danger bits mirror the host (INFERENCE).
- Missing #8 says every client sample's raw24 bytes at cm+0x34..+0x36 read 00.01.01, so cm+0x35=1 on the client. → Corrected: a recount of all retained client logs finds cm+0x35=1 in about 251.7k samples and 0 in about 8.2k Town_Cluster samples (CONFIRMED_LIVE, private live logs, conclusions only). The narrowing of G4 to cm+0x20 and the SetRandomEncounterTimer callers is applied.
- Missing #4 calls 0x404C50/0x405F40 a lock pair. → Not confirmed. The decompile shows 0x404C50 building a local record (flag 0x80000000, cleared fields, 0x4049C0) and 0x405F40 freeing it when heap-owned. Neither touches shared state or a lock. They are listed as local record constructor/destructor at INFERENCE. The review's other G1 additions (0x4015B0/0x4015E0, 0x507DC0) are applied; 0x4015B0/0x4015E0 write only the TPtr record and pointee+4 (CONFIRMED_STATIC).
- Wrong #2: the host has in_combat=1 in 67 logs. → Recount: 66 retained host logs (the five rotation files plus about 61 archives), about 17k samples. The logs are still growing during live play, so the result says 'about 66'. The qualitative correction (far more than five logs) is applied.
- Wrong #4 refers to line numbers 'in the current dirty worktree'. → At HEAD 9081425 the worktree is clean apart from an untracked docs file, and the review's lines (lan_story_runtime.c:3380-3397 host publish, 3442-3452 client apply) hold at HEAD. The lines are applied and every 'dirty tree' wording from the draft is dropped. The anchor in design step 3 is the SudekiMpLanPartyGetStoryEffects call.
- Overclaim #3 says the zone trace installs at DLL attach, with camera trace = !skill_camera_routing_enabled, gated by SUDEKIMP_ZONE_TRACE. → Applied, with a refinement. The install runs in SudekiMP_Initialize (dllmain.c:3039-3045). zone_transition_trace_enabled is also set by several prototype switches (dllmain.c:1678-1682), not only the environment variable, so the conflict is wider than SUDEKIMP_ZONE_TRACE alone. Whether any supported story profile enables it with skill camera routing off is UNKNOWN.

### F2

- Missing point 2 names 0x437170 at 0x4372DB as a retail creator on the 0x7C2F30 list. → Partly wrong. SetCameraTarget creates its target on this+0x4C (LEA EDI,[EBX+0x4C] at 0x4371F8, call 0x535040 at 0x437205). 0x4372DB and 0x437313 are releases on *(0x7C2F30)+0x4C. 0x42A370 does create on *(0x7C2F30)+0x4C (0x42A58C/0x42A5B6, exact-image bytes). The substance is applied: use the 0x7C2F30 list or prove the globals equal. Equality stays UNKNOWN, and is an open gate.
- Wrong entry 1 says the combat_input lines for a downed Tal in the two 2026-10-10 phase-B logs are three lines. → Conclusion confirmed, count not. The private live logs (conclusions only) show six such lines in one run and one in the other, all logged after Ailish was already down. In the client-down window of the earlier 2026-10-10 run, no combat_input for Tal was logged at all. The draft's 'never captured' was wrong and is corrected; the client-down case is CONFIRMED_LIVE on a pre-#71 build.
- Missing point 1: 'a protocol HP byte of 1 is the revive signature'. → Not adopted as stated. HP is a uint32 field (src/network/lan_story_frame.h:82), not a byte. A living member can legitimately hold 1 HP, so 1 right after 0 is consistent with a revive but not unique. The mechanism is applied: Remove 0x4C0B10 writes HitPoints = 1.0, CONFIRMED_STATIC, and this matches both live revives.
- Missing point 4: a remote actor's door use executed on the host would revive everyone mid-combat. → Narrowed. 0x4CEDE0 acts on the group lead, and remote shop visits use the host's trigger, SwitchZoneNOW and position-setter path (src/hooks/lan_story_remote_interior.h:14-24), not native CDoor use. So only the host lead's native door use is known to revive (CONFIRMED_STATIC chain). That remote visits do not revive is INFERENCE. The zone change of a remote visit still clears camera targets (0x4379B0), so it is recorded as a yield case and a dependency.
- Wrong entry 2: the 0xA18 flag only decides anything in 0x4D3530. → Mostly applied. For a victim in the group the 0x4D31C0 test cannot fail, because the victim counts itself; that it always holds is INFERENCE resting on no lone 0x8000 writer. For a kind-0x1D actor outside the player group, the 0xA18 bit still decides between knockout and 0x4DC2B0 (CONFIRMED_STATIC). The gate is scoped to both cases.
- Overclaim 8 describes 0x4BCF50 as a CStatusEffectManager vtable slot at 0x6D4AC0. → Not confirmed. The decompile shows a fastcall initializer that allocates every status effect and stores the ConcussedStatusEffect (constructor 0x4C08B0) at manager+0x4C (CONFIRMED_STATIC). The vtable-slot location is left UNKNOWN; the result calls it an initializer, not a factory.
- Overclaims 1 and 5 cite line numbers for HEAD d437693 with a dirty tree, and plan line 6778. → Applied at 9081425 (clean tree): src/hooks/lan_story_runtime.c:1882 early return, :1889-1903 arming, :3699 install, :3714-3716 host-only control_attempted. The plan's §10 E3 row is now docs/coop-story-plan-54.md:6795. Not a dispute of substance.
- Overclaim 1: within 0x4D3530, 0x8000 is set for every non-party kind. → Applied with precision. 0x4D3530 groups 13 kinds (1,5,8,9,0xA,0xB,0xE,0x13-0x16,0x1D,0x23) behind the living-member and 0xA18 tests, and sets 0x8000 unconditionally for every other kind. A second writer, 0x4DA8A0, was found. Reading 0x8000 as 'dead party member' is lowered to INFERENCE.

### G

- Overclaim on fact 15: the review says 0x4231A9 sits in 0x423080's one-shot init branch, gated by group+0xD5==0 and 0x809D78!=0. → Correction rejected. The disassembly shows the +0xD5 block ends at 0x4230D3, before the call. 0x4231A9 is in the separate queued-member branch (0x4230DA-0x4231C7), gated by +0x5C nonzero, +0x5C == +0x58 and every queued entry ready. The draft's 'load branch' was also imprecise. The fact now names the queued-member branch (CONFIRMED_STATIC). Treating it as the save-load build stays INFERENCE.
- Wrong entry on the hold counts: the review gives 61 installs / 55 arms on the host and 55 installs / 0 arms on client1, against the draft's 55/54. → Partly applied. Installs on both seats (lan_story_runtime.c:3699) and host-only arming (lan_story_runtime.c:1882, 3713-3716) are confirmed and stated. The exact counts are not stable: the private logs are still growing on the live pair, and they read 62/56 on the host and 56/0 on the client when the fold ran. The result gives no fixed counts: installed on every launch, armed on the host whenever a client was observing, never armed on the client. Zero lead_kept and zero scripted_lead_held is confirmed.
- Overclaim on fact 9: the review says 'cannot be released' is INFERENCE over an incomplete path list, because StoryReleaseBeforeWorldChange and StoryEnd were not examined. → Re-verified and restored to CONFIRMED_STATIC, with a stated condition. Every story release site was checked: story_control_drain (8502-8547), StoryLoadDrain (8581-8618) and StoryReleaseBeforeWorldChange (8621-8650). Each requires an identity in which +0x248 equals the recorded host. StoryEnd (8659-8666) refuses while any lease is held. The claim now reads 'while controller+0x248 is not the recorded host'.
- Wrong entry on host_binding_changed: the review says 'every remote lease' is too strong, because exterior-retained leases (ExteriorNativeBound) are exempt and the reason may be roster_unknown or scene_changed. → Partly applied. The drain-reason order (lan_story_runtime.c:1933-1940) and the exterior exemption are confirmed and stated. However, ExteriorNativeBound itself requires leader == locked (lan_story_host_control.c:425), so a native lead change also defeats the exemption. The result says a lead change drains each lease, and the logged reason is roster_unknown, scene_changed or host_binding_changed.
- Missing point on retail +0xC0: 'a pending remote lead that fails 0x423750 ... blocks host swaps indefinitely even in retail'. → Applied with a narrower scope. SetAsLeadPlayer stores the pending lead only after 0x423750 accepts it (0x424E6E-0x424E75). A later 0x423750 failure therefore needs the arbiter flags to change after the store. The retail retry-forever case is stated generally: Next never brings the pending lead to slot 0, for example because its preconditions keep failing.
- Not raised by the review; found during the fold: the draft said no source names 0x4237B0 ('grep for 0x237B0 found nothing'). → Corrected. talos_companion_membership_abi.c:206/1551 checks that 0x4232F4 calls 0x4237B0, and 1578-1580 checks that 0x423433 calls 0x423F60. The check runs only under the Talos staging-observation flag (dllmain.c:2758). An entry hook on 0x4237B0 is compatible with it; a call-site hook at 0x423433 is not. Also corrected: the plan §10 G row is at docs/coop-story-plan-54.md:6797, not 6778.

### early

- Missing point: 'G3 can already be answered ... no title-time activation can install the hook ... G3's proposed explanation for the fast arms is refuted.' → Applied in part.
- Confirmed: the input adapter and client Observe exist only after lobby gameplay prepare (src/hooks/lan_story_runtime.c:3273-3275,3727-3729; src/hooks/lobby_gameplay.c:244), after the placement script's activations in both 2026-10-18 runs. So a title-time activation cannot install the hook itself.
- Not refuted: an earlier activation still matters if the client window remains its thread's active window at the first Observe after install.
The old G3 is closed as posed, and its remaining question moved into the relaunch gate (now G7).
- Missing point: 'a bind failure that is not deferred closes cast admission permanently for the session.' → Corrected. The failure path keeps the image and sets fault=stopping (src/hooks/lan_story_cast.c:550), and TryBind refuses (556-562). But a later drain_story_casts on a party-swap handoff or host teardown calls SudekiMpLanStoryCastUninstall, which sets image to NULL (lan_story_cast.c:731-763), and clears cast_attempted (src/hooks/lan_story_runtime.c:1593-1603,1724,3232). A later READY then reinstalls. Admission is closed until the next swap or teardown, not for the whole session (CONFIRMED_STATIC).
- Missing point: relaunch counterexample timing (wall-clock mapping via CLOCK_MONOTONIC, plus or minus 1 s; script returned about 6 s before the runtime install). → Kept, with the precision marked as dependent on the private session record:
- The tick arithmetic matches the ambush run's mapping: adapter install about 6.5 s before the arm.
- The session record shows a 15 s sleep and an audio check after the placement script.
- The monotonic mapping itself was not re-derived here.
The conclusion, that this run bears on the root cause (G7), does not depend on the exact seconds.
- Wrong-claim correction: 'pending_presentation_invalid per-episode bounds reach about 190-260 ms.' → Applied with re-derived bounds. The count is 21 episodes before the ACK; a 22nd comes after it. From surrounding timestamps, the first episode is bounded at about 261 ms and the others at 180 ms or less. Some timestamp sources are not mutually monotonic, so these are upper bounds, not durations.
- Missing point (G4 refinement): 'Routed activations without authority are rejected (skill_activation_abi.c:104-106; lan_story_cast.c:147-152,193-197,209-213,241-245).' → Confirmed, with refs corrected. The file is src/engine/skill_activation_abi.c: line 104 calls the routing entry and line 106 is the rejection. The authority checks are at lan_story_cast.c:147-152, 198 and 212, and native_use's check is at 246. An image scan also found no absolute data reference to RVA 0xB4810, which supports the 'two direct callers' claim.
- Missing point: 'RVA 0x34E6A0 is a field (+0x30) of a structure at RVA 0x34E670 passed to RVA 0x1DC9D0, not a standalone HWND global.' → Confirmed and extended. The field has three writers, at RVA 0x28BE3D, 0x28DAA0 and 0x28DE7C, and each copies app+0x0C. Its lifetime is still UNKNOWN, so the D3D9 hFocusWindow route stays preferred.
- Wrong-claim correction about source state and line bases ('InputArm is HEAD 2583, working tree 2610'; visual host lines are working-tree numbers). → Superseded by the task's fixed basis, HEAD 9081425 on a clean tree. Every citation was re-based to that commit, for example: pending_input_arm at src/hooks/lan_story_runtime.c:2620, the cast trigger at 2117-2133, the input install at 3727-3729, the client ACK at 2671-2674, and the visual host session arm at src/hooks/lan_arena_spirit_visual_host.c:1736-1740.
- Wrong-claim correction: 'Only WM_SETFOCUS sets focused=TRUE.' → Applied, with one nuance: at hook install, focused is also initialised from GetFocus()==window (src/hooks/lan_story_input.c:153). After install only WM_SETFOCUS sets it.

### I

- Wrong #0: 0xFEFC0 is not just a destructor path; mode 2 is the in-game save page, so every close frees the cache and the section 9 wording is closer to right. → Applied. Confirmed statically: 0x80270 stores its stack argument at +0x460 (VA 0x4803E7); 0x9DD80 case 0xC builds a fresh mode 2 page at VA 0x49E05D, reached from event 0x1B through 0x9C930 and 0x9E560; modes 0 and 3 at VA 0x49FA26 and 0x563D0B; the destructor test at VA 0x4804A4. 'Every close' is kept at INFERENCE: a fresh page is built per open, but the close-to-delete path was not traced. Retiring it as the write seam stands.
- Wrong #1: Prepare never invalidates; an in-game import needs its own invalidation, 'or the row match fails as it did in section 9 (catalog_or_row)'. → Applied and G5 closed statically (src/hooks/lan_story_load.c:386-450, src/ui/title_lobby.c:733). The section 9 catalog_or_row failure came from a copy left under another folder name, not from a stale list, so it is not evidence for this case; that a stale list would also fail the row match is INFERENCE. The design invalidates at the title after Leave, since the call refuses outside IDLE/FAILED; that Cancel returns to IDLE is left as part of G11.
- Wrong #2: no save_book_intercept hook runs in saved_story; the story installer needs startup with no world; lan_arena_campaign_guard.c is a second owner of 0x84F10/0x101690. → Applied; every caller confirmed by grep at HEAD 9081425. That the campaign guard never runs in a saved_story process is INFERENCE (lobby_gameplay.c:244 picks the story runtime for a saved destination; dllmain.c:1295-1296 disables the testroom). The review's 'no live log anywhere records that block' could be checked only against the private story logs (none found); arena-profile logs were not checked.
- Wrong #3: STATE cannot carry the session-save identity (5 free bytes). → Applied (src/network/title_lobby.c:11, :15, :449-452, :491-501). The design now uses a new SESSION_SAVE frame or a wire-size change, both under O10.
- Wrong #4: no lobby UI thread exists; hashing and import already run on the game thread; in-game receive needs new code. → Applied. The review's ui/title_lobby.c line numbers are about ten lines low at HEAD 9081425; re-derived as import 719-741, service 742-784, UiPoll early return 787, expiry/service calls 802-803, provide 1016-1023, UiBackground 1052-1056.
- Overclaim #3: lan_story_runtime.c lines should be cited at HEAD (the review says persisted=0 is 3066 at HEAD, 3095 dirty). → Neither number matches HEAD 9081425: the persisted=0 line is src/hooks/lan_story_runtime.c:3105 inside clear_story_loot (:3104). Cited with the symbol. Other runtime citations were also re-derived (Leave/host-loss 2985-2997, prompt clear 2851-2857).
- Overclaim #1: SetSavePoint's callees 0x12E90 and 0x15E0 were not inspected. → Inspected in this fold: 0x12E90 zeroes a three-dword link and 0x15E0 unlinks a list node, neither does file work. The pointer resolver 0x1BF4E0 it also calls was not read, so 'writes no save files' stays INFERENCE for SetSavePoint.
- Missing #1: with no cached rows the selected row has no folder, 'exactly the J new-game case'. → Guard confirmed (VA 0x5012E5-0x5012EC). Narrowed: a J host may already have other saves, so the NULL-rows case is a save folder with no saves (INFERENCE), not J as such. This fold also found that an overwrite can reuse the deleted folder's number (0xFE210 takes the lowest free), so the design identifies the new folder by write time rather than by a number diff or the cache row alone.
- Missing #2: 0x84560 serves all three page modes; a blanket fail-closed backstop would block single-player deletes. → Applied as a pass-through rule outside an active co-op session. The class membership and single caller (VA 0x481E40 in 0x81830) are CONFIRMED_STATIC; that the mode 0 title page reaches the delete state was not traced, so the single-player impact stays INFERENCE. The rel32/abs32 scan's only other hit for 0x484560 is the demangler string '`EH' at VA 0x69BBEC, not a pointer.
- Overclaim on the scratch-note record offsets (+0x1390, +0x1398, +0x16B8, +0x1914). → Lowered to HYPOTHESIS; the reviewer-checked copies stay CONFIRMED_STATIC. Not re-decompiled in this fold.

### J

- Wrong #1: 0x4A1900 does not submit StartNewGame; it submits FrontEndTimeoutLoopStop at 0x4A1928, and StartNewGame comes via SimpleFrontEndMenuSelect (0x4A0FED) → ExecuteSelectedMenuItem → GEL_ExecuteFunction. → Confirmed and applied. The objdump bytes show MOV EAX,0x6CB0C8 ('FrontEndTimeoutLoopStop') before the call at 0x4A1928, and MOV EAX,0x6CB0AC ('SimpleFrontEndMenuSelect') before the call at 0x4A0FED. The GEX decode of 0x78D7B0DC and 0xD7303D78, the export 0x425E70 = CGELEngine::GEL_ExecuteFunction, and the calls 0x425E7C → 0x425EA0 and 0x425F03 → 0x5C37B0 all match. Root vs child, and synchronous or not, stay UNKNOWN (R1).
- Wrong #2: the J2/J3 bracket around 0xA0FD5 → 0x4A1900 cannot capture the StartNewGame root, so a new seam is needed. → Confirmed and applied, with one addition. Besides 0x4A0FED and 0x425F03, the narrower 0x425E7C call inside GEL_ExecuteFunction is a candidate. A no-new-seam variant is kept as a HYPOTHESIS: the existing create observers bind hash 0x96D5F775 after lobby_gameplay opens the generation, which needs R1. Seam ownership became O13. 17 direct callers of 0x425EA0 were confirmed by a linear sweep.
- Wrong #3: O7 is not open; D12 decided that story progress is host-applied and synced (S1). → Confirmed (plan line 69) and applied. O7 now asks only for the mechanism (native SetStoryBlock or data only) and the timing relative to J. J4 no longer says the block stays stale by design.
- Wrong #4: O3 and O12 are already decided by the NG decision and the acceptance route. → Confirmed (plan lines 70, 72-75) and applied: O12 removed, and O3 narrowed to early rejoin (before the first host save) and Buki/Elco seats. The reviewer's further residual, 'client-side ClearPersistantDataRegion placement', was not kept as an owner decision. StartTestGame already clears that folder on every Test Room start on every peer (CONFIRMED_STATIC). Only keeping the D6 temp area and the #72 slot outside it is new work, an engineering gate (R5).
- Wrong #5: allied guards on the outbound 1100 invasion are UNKNOWN; the live E2 acceptance is the return-leg AklorianAmbush. → Applied, with a nuance. At HEAD, plan §9 (lines 6751-6760) attributes the client guards to spawn group HC_NW_GATEGUARDS 'when the castle road loads', proven live from post-recruit saves on the castle-road ambush. The ambush completion script (0xE2CAD6C6) sets 2000 and plays A001_Sguard idle animations, consistent with the reviewer. Whether HC_NW_GATEGUARDS also spawns at 1100 depends on its zone-data story range, not checked: UNKNOWN, not 'absent'.
- Wrong #6: only status-matrix row 87 is PARTIAL (ordinary shots), and row 99 is the IMPLEMENTED/LIVE_PROVEN Ailish-host route. → Confirmed (docs/status-matrix.md:87, :99) and applied. Remote Ailish skills are now UNKNOWN, not PARTIAL.
- Overclaim: the ~225 s host block is INFERENCE, because the GEL PlayMovie|S binding to 0x504D90 is unproven. → Applied: the duration is CONFIRMED_STATIC, the block is INFERENCE. Partly narrowed: SudekiStartup plays FMA01_poem through PlayMovieSkippable|S, and the 0x504D90 hook skipped it repeatedly on both seats in the private live logs, so at least that GEL movie native reaches 0x504D90 despite no direct xref (CONFIRMED_LIVE, conclusions only). The decoded branch is confirmed: hash 0x4878A16F is FNV-1a of 'isdevmode', and IsDevMode is the return-false export 0x451310.
- Overclaim: the 'default' camera add in the castle main is an unresolved hash 0x3967D9FB (INFERENCE). → Resolved further than the review. The GEX hash is FNV-1a over the lowercased name, verified on several known names, and 0x3967D9FB = 'addcamera|ss'. The AddCamera('default') call at 0x66D45 inside the held castle main is CONFIRMED_STATIC. That the contained client lacks that camera remains INFERENCE (R10).
- Overclaim: 'UpdateWaypoint after SetStoryBlock(1000)' was not decodable. → Corrected both ways. With hash resolution, 0xDEF215EB = UpdateWaypoint sits at 0x66DC1, the target of the IsStoryBlockBefore(1000) jump-if-false at 0x66CFF. It is the re-entry branch, not a step after SetStoryBlock(1000). The branch reading is INFERENCE. This supports the castle re-entry gate R9.
- Overclaim: the lan_story_runtime.c line numbers are against an older HEAD; say so. → HEAD is now 9081425, which includes the edits the reviewer saw as dirty. Every citation was re-checked and re-cited at 9081425: spectator admission 2966-2971, poll_network 3060-3065, observe_recruitment 1527-1573, service_client_recruitment 2224-2254, the lobby_gameplay lines, ui/title_lobby.c:501, and plan §10 at 6795-6801.
- Overclaims on spectating evidence, the broadcast meaning of 0x43F3B0, the 'message pump' wording, the StartSpawnGroupSequence variants, the cinematic capture, and 'PC is Tal' being understated. → All checked and applied. Spectating is UNKNOWN on current code: the private 2026-10-03 record shows the spectator froze at Ailish's introduction, and row 99 lists the limit. 0x4FD9C0 posts a type-4 message through 0x43F3B0; 'broadcast' is INFERENCE. 'Message pump' is INFERENCE. 0x47C/0x47B use StartSpawnGroupSequence|SRNN (0x4B5832BD), 0x52F uses |RN. The cinematic capture on IC_GameIntroTSA is INFERENCE until R2. 'PC 2 is Tal' is now CONFIRMED_STATIC by constant use.
- Missing points (lobby 90 s timers, fade at the hold point, quarantine of untracked roots, held-task teardown, castle re-entry, StartTestGame already clearing the folder, the third lobby mode, the J7 +0x6C and update-suppression dependencies, the homecoming RemovePC and FMA04A). → Each was verified at HEAD and added. Lobby timers: network/title_lobby.c:715-722. Fade: StartNewGame ForceFullyFaded, with the fade-in inside IC_GameIntroTSA. Quarantine: lan_story_task_trace.h:31-38. Detach: :45-49. Re-entry: UpdateWaypoint branch. StartTestGame clear: CONFIRMED_STATIC. Lobby branches: listed lines. J7: lan_story_spawn_events.h:41-46, :350-355, :384-385; story_spawned_actor.h:13-16; lan_story_recruit.c:432-435. Homecoming: 0x0CC60D5C plus the FMA04A header, 4689 frames, about 156 s. One draft detail was dropped on re-reading 0x423280: the '+0xCC is the party count' claim. That field is incremented before the notification loop and is not shown to be the count.

### Fkeys

- Wrong #1: the dirty tree on d437693 (7 hunks in lan_story_runtime.c; the plan's 2026-10-17 entry and section 10 row uncommitted). → The point was correct for d437693 but has been overtaken. HEAD 9081425 is clean and commits those hunks, the 2026-10-17 entry (docs/coop-story-plan-54.md:6743-6745) and the section 10 row (now :6801). The dirty-tree statement is removed, and every lan_story_runtime.c citation is re-based to 9081425 (shift of +36/+37 after :1109; e.g. poll_story_swap :1740, menu_slot_allowed :2280, the poll_story_swap call :3313, the main-menu install :3754).
- Wrong #2: the retail Armour page is dead code with the ArmourChoice adapter installed. → Confirmed and applied, with one qualifier. It is dead only while ArmourChoice is on (the default) and the adapter installed: armour_menu.c:488 returns early when the key is off, and dllmain.c:1122-1125 treats a refusal as non-fatal. The adapter is installed in every retained run (CONFIRMED_LIVE). The design no longer admits retail Armour.
- Wrong #3: the quoted request_result log format. → Confirmed and applied. The literal format is 'story_cast event=request_result player=1 character=2 request=N kind=6 slot=100|101 outcome=1' (lan_story_runtime.c:2220 at 9081425). Kind 6 = EQUIP_ARMOUR (lan_story_handoff.h:71-93). The rotated host logs hold 10 such lines, paired with 10 host_equip_armour lines (lan_story_control.c:521).
- Overclaim #1: the client F3 has no f3_info event in any retained client log, so F3's live proof rests on the #75 comment alone. → Partly disputed. The client key list is now labelled CONFIRMED_STATIC, as the review asked. But older retained client logs from 2026-10-15 builds contain five f3_info deliveries to the Weapons handler 0x16F110. F3 is therefore CONFIRMED_LIVE from logs and #75 for those builds, and not re-exercised in the 2026-10-16 to 2026-10-18 logs (private live logs, conclusions only).
- Overclaim #6, and the draft: no cycle_skipped/cycle_redirected/open_moved lines in any retained log (118 files). → Corrected beyond the review. The count holds for the rotated logs from 2026-10-16 on. But older retained host logs from 2026-10-14/15, with scope=owned_slots, contain 7 cycle_skipped and 5 cycle_redirected lines. A permitted cycle logs nothing (lan_story_main_menu.c:112-117), so absence of traces never proved absence of cycling. The window is restated as 2026-10-16 08:26 to 2026-10-18 11:38, with the growing live file excluded.
- Missing: on the host, the native 0x40AFD0(1) pauses the controller listeners during the menu, which closes the native gameplay F1/F2 consumer there. → Recorded with a stronger closure. The host story runtime replaces the controller update's native Previous/Next switch calls RVA 0x28464/0x28480 with a bare return (lan_story_control.c:29-30, :62-68, :214-215; installed for the host at lan_story_runtime.c:3713-3717): CONFIRMED_STATIC. The menu-time listener pause stays INFERENCE, from the adapter's comment (lan_story_main_menu.c:63-68).
- Missing: G1 refinement, and the flags live on CUIScene with ESI = *(0x808D1C). → Confirmed and applied. The jump table VA 0x40A068 = {forward 0x40A015, drop 0x40A02A}, and 0x40A02A is also the join point after a forwarded call. The scene identity is now CONFIRMED_STATIC: the listener rebases ESI by -8 to the CUIScene start before each filter call (e.g. VA 0x40A2B8); the CUIScene constructor stores that start at 0x808D1C (VA 0x408CA6); 0x40AFD0 writes +0x8D on it (VA 0x40B025).
- Missing: G2 is closable statically (Armour and Advancement load ESI = *(0x7C2F78) before 0x51F810). → Confirmed by disassembly and applied: VA 0x567387 to 0x56738F and VA 0x5727C1 to 0x5727C9; 0x51F810 calls [ESI]+0x2C (VA 0x51F82B-0x51F835). This fold also confirmed that Advancement forwards only in state 3 (state table VA 0x5728FC, command table VA 0x572914). The old G2 is dropped and the fact raised to CONFIRMED_STATIC.
- Missing: F4 in Items states 3 and 4, and F3 sort in state 3. → Confirmed from the Items handler RVA 0x1770B0 and applied. In states 3 and 4, command 11 joins the Back/Cancel path (mode switch 0x177850); in state 3, command 10 sorts. The design admits F4 only in state 0.
- Missing: the gate refuses only OBSERVING peers; whether a character is assigned during JOINING/PENDING is UNKNOWN. → Resolved statically. Story admission records the character while the peer is still FREE (lan_party_session.c:3313-3326), and player_character_locked returns it for any phase (:408-411). So during JOINING, PENDING and DRAINING a human client's character is not refused on the host (CONFIRMED_STATIC). Whether that counts as 'connected' is added to O1.
- Overclaim #3: whether the 2026-10-17 claim was the owner's words or an agent's reading. → Applied. The #74 comment of 2026-10-17 22:37 attributes it to the owner, the plan entry (:6743-6745) is silent on it, and #87 lists it under 'Covered'. The authorship and which run it describes are both UNKNOWN.
- Overclaim #5: the wider scan for +0x170 writers. → Not re-run in this fold. The claim stays CONFIRMED_STATIC within the draft's scanned set. The review's partial whole-section scan is reported as supporting, with untraced hits remaining.
- Not raised by the review: the draft's evidence that the client's controller services run while its menu is shown (the menu_input/menu_service lines). → Corrected. Those lines come from service_quick_menu and menu_frame (lan_story_runtime.c:2402-2436, :2809, :2859-2862), not from service(), where poll_story_swap runs (:3313). Supporting evidence is now the 2026-10-15 menu-open hold lines from service_client_control, which runs in the same service pass under the same condition (:2651-2652, :3350). The claim stays INFERENCE until G2.

### skills75

- wrong[0]: 'Only the handler and two update sites use 0x174370' (the review's correction to facts[2]). → Applied, with the call-site count corrected. The inline state stores are confirmed: 1 at 0x172984, 2 at 0x1732D9, 3 at 0x17558C and 5 at 0x175637. But 0x174370 has nine direct call sites, not 'the handler and two update sites': four in the handler (0x17271D, 0x172762, 0x17282A, 0x172859), three in the update (0x1733D6, 0x173443, 0x173661) and two in the listener (0x1755EC, 0x175600). Fact 3 records the full list (CONFIRMED_STATIC, call scan of the supported image).
- missing[2]: 'AddExperience 0xD9BF0 and DoLevelUp 0xD9DB0 both gate on group+0xD4==0 and level < 30'. → Partly wrong; only the supported part is kept. AddExperience (0xD9C0A) and AddExperienceToParty (0xD9A1C) test group+0xD4 and level < 30 before calling DoLevelUp. DoLevelUp itself only requires next level < 31 (0xD9DE9) and loops over several levels. CGroupPlayers::DoLevelUpChecks (0x24EF0) has no +0xD4 test. The four-fixed-slots and no-downed-check parts are confirmed and added (fact 13).
- Overclaim on facts[12]: 0x17E860 'is a general level-up notice with a Tal-first-time special case'. → The two branches are confirmed, but the characterisation is not supported. With group+0xD7 set, the routine fires only for the actor in group slot 0, with key 'TutorialPCLevelUp' (VA 0x6D8D68). With it clear, it fires only for Tal (type 0x23) reaching level 2, with key 'HINT_LevelUp' (VA 0x6D8D58). Both branches are tutorial/hint keyed. The result describes it as a tutorial/hint trigger with two conditions (CONFIRMED_STATIC for the branches). What it shows, and the meaning of group+0xD7, are INFERENCE/UNKNOWN, with gate 3 open.
- Overclaim on state_today: the 0x9FD0 filter 'upgrades reachability of page 9 to CONFIRMED_STATIC'. → Kept at a lower level. The filter table and the Advancement handler tables are CONFIRMED_STATIC. Commands 0 and 6-9 are forwarded whether or not the +0x8D/+0x8E/+0x61 flags are set, so the page's confirm, list, toggle and ring inputs reach a client's menu. Whether +0x8E/+0x61 are set on the client is UNKNOWN, and page 9 has never been opened on a client. Ring navigation to pages 0-2 there is CONFIRMED_LIVE. A client reaching and using page 9 therefore stays INFERENCE until gate 2.
- missing[4]: 'The inlined commit at 0x1732C3 is only reachable from state 1, so it can never see state 8'. → Lowered to INFERENCE. The branch is entered only in state 1, but it re-reads layer+0x1080 after the virtual calls vt+0x34 (0x1725F0) and vt+0x10 (0x172C30). No state store was found in 0x172C30, but that does not prove the site can never see 8. The site stays in the five-hook design so all sites share one invariant.
- missing[8]: DoLevelUp's effect is gated by actor+0x58 != 0 plus IsPlayerCluster, and its sound by VA 0x808D1C and VA 0x7C30EC. → Confirmed and extended. The export table names 0x35740 CClusterManager::IsPlayerCluster. The review missed that actor+0x58 == 0 skips the sound as well as the effect (0xD9EFE jumps to 0xDA042), and that 0x17E860 runs on both paths. Fact 14 and design P4 record this.

### void73

- Wrong #3: 0xEC904 passes A9 from a register shared with the preceding 0xEA310 call, so it is a candidate A9=1 requester the gdb gate must include. → Correction rejected for 0xEC904. The ten-site enumeration, 0xF293E (A9=0, A8=1) and 0xF01D0 (A8=1) were confirmed and applied. But at 0xEC904, ESI is zeroed at RVA 0xEC8D5. 0xEA310 is a register-argument routine that reads ESI as its index and never writes it, and no branch enters RVA 0xEC8D7-0xEC904. So 0xEC904 pushes A9=0 (with A8=1, speed 0.0): it passes C3 natively and is not a skipped-snap candidate (CONFIRMED_STATIC, objdump). The gdb gate keeps the draft's five A9=1 requesters.
- Missing #3: PopToNamedLocation refuses a {0xFFFF, pos, facing} record, so the warp must carry a real sector. → Applied in part. 0xFBBD0 does return 0 when either byte of the resolved sector is 0xFF, and PopToNamedLocation then skips 0xF30A0 (RVA 0xF644C). That is the script validating a named location, not a contract of 0xF30A0. On the immediate path (speed 0, A9=0) the record's sector is copied to node+0x84 and reset to 0xFFFF by 0xF3200, and nothing in 0xF30A0, 0xF50C0, 0xF3200 or 0xF5030 reads it in between (CONFIRMED_STATIC). The design still passes the lead's real sector, as the formation snap does (RVA 0xB3937-0xB397C), and gate 2 checks the remaining leaf callees.
- Missing #2 parenthetical: the state-1 +0x18 rule is 'the same rule in 0x4EF5D0'. → Not confirmed for 0xEF5D0. It returns NULL for any state below 2 (RVA 0xEF5F9-0xEF5FC) before its +0x18 selection, so that branch is unreachable there. The +0x18 rule is applied as membership's own rule (src/hooks/lan_story_area_membership.c:116-117). The main point, do not misfire on a loading area, is applied.
- Missing #6: the backstop's 'no world loading' condition should reuse HOLD_AFTER_SWITCH_MS/background_switch_tick instead of a second notion of loading. → Applied in part. The design reuses the observer's signals (SudekiMpLanStoryObserverWorldLoading, Holding) rather than a new notion. But the 20 s HOLD_AFTER_SWITCH window (observer.c:73, 644) and the 5 s IdentityUnknown window (observer.c:1157-1163) both open at every SwitchZoneNOW (observer.c:360). A retirement fall starts at that same switch (live onset within one 265 ms sample). Gating the backstop on those windows would leave the first 5-20 s of a fall unhandled, so they must not suppress it (INFERENCE).
- Missing #7: an owner decision on the predictive warp while H1b has the snap flag off, and on a member whose visit the evict finishes in the same callback. → Placed as design and a gate rather than an owner decision. The warp does not use the snap flag, since it calls 0xF30A0 directly, so the flag state does not matter. A member the evict places at a door inside a doomed descriptor would fall, so he must be covered. His tracking sector is stale right after place() (0x3050 writes CPosition only; INFERENCE), so the next-dispatch retired-area check, not the in-callback predicate, covers him. No gameplay choice remains beyond the existing 'when to warp' owner decision.
- Not raised by the review: draft fact 1 said 'Outdoor area retirement has one native path' (SwitchZoneNOW). → Corrected by the fixer. 0x5A70 also has a caller in CWorld::SwitchMainZone (RVA 0x639F). The script exports UnloadZone (RVA 0x7BB0), RemoveLastActiveZone (RVA 0x7BD0) and UnloadNexus (RVA 0x7CA0) set zones to state 0 directly, and the observer hooks none of them; the setter has 19 call sites in all (CONFIRMED_STATIC, objdump plus export table). Route use is UNKNOWN. The design therefore makes the per-frame retired-area check primary and the SwitchZoneNOW predictive step optional.
- Overclaim on the plan citation (section 10 row cited at the dirty-copy line 6783). → Re-cited at HEAD 9081425: docs/coop-story-plan-54.md:6802, text 'Filed; no work since 2026-10-15 beyond research lists'. All other plan citations were re-checked at 9081425 (section 9 H1b text at 6602-6647).

### hold70

- Wrong #6: the world frame version 12 is uncommitted dirty work on d437693, so 'world frame version 12' is UNVERIFIED_DIRTY_WORK. → Correction rejected for the cited revision. At HEAD 9081425 the tree is clean and src/network/lan_story_world_frame.h:10 defines version 12. Version 11 is at d437693; the bump came in with commit 9081425 and is recorded as owner-approved in plan §9 (2026-10-18 E2 entry). Kept 'world frame 12, CONFIRMED_STATIC at 9081425'. The review's naming point was adopted: the design now says story frame 10 to 11 (lan_story_frame.h:8) and leaves the world frame unchanged.
- Missing point: the task trace observes the handler hash through an inline hook on the SOL submit 0x1C37B0 (lan_story_task_trace.c:15, create log at 690-694). → Mechanism corrected, conclusion kept. The task trace does not inline-hook 0x1C37B0. It installs relative-call hooks on the task-create calls at RVA 0x1C38F2 (inside 0x1C38D0, which 0x1C37B0 calls at 0x1C3831) and RVA 0x1C4DB8, and on the load-time submit sites 0x100939 and 0x1009DF (lan_story_task_trace.c:15-16, 1808, 1882). The create log is at lines 674-678. The review's conclusion stands: the create line proves the OnAction task, not the 0xCAEB call site, and G1's 0xCAEB breakpoint is the closure.
- Overclaim: fact 12's exact counts (195 handlers; 52/56/43/17/8/2/4) versus the review's independent 149 (40/50/34/13/5/0). → Neither total confirmed; both depend on classification rules that are not committed. My direct-call-edge scan confirms: EnterInterior|SRS 39 OnAction + 1 OnEntry (as the review says); SwitchZone|S 16 OnEntry + 17 OnExit + 4 non-trigger callers. The review counted 16 Trigger1 OnExit; the extra OnExit caller was not identified. Further direct callers: EnterInterior|SRSS (7 OnAction, 1 OnExit), EnterInterior|SRSB (7 OnAction), and ExitInterior variants. Direct edges are kept as CONFIRMED_STATIC; transitive totals were dropped and an open gate (G12) was added.
- Missing point: the contact builder 0xCFD0 'skips message creation entirely (not re-queue)' while UI is active. → Partly confirmed. The decompile shows that no OnEntry/OnExit message is built while (CUIScene+0x174 != 0) && (0x9E680() || *(RVA 0x408DA4)+0x80 == 2) (CONFIRMED_STATIC). However, contact entries whose +0x18 == 1 are not removed from the contact list in that state, so the event is probably delivered on a later pass (INFERENCE from the loop structure, not 'skipped entirely'). Recorded as a fact at INFERENCE level, with gate G11.
- Missing point: 0x49E680 reads '*(0x3C2F88)+0xB8 not in {3,4,0x11,0x12}', and because the banner Show gate needs that same layer with +0xB8 == 1, a host banner may itself flip the predicate. → Polarity corrected and the risk lowered to HYPOTHESIS. RVA 0x9E680 returns true when the layer state is in {3,4,0x11,0x12}, or when *(RVA 0x3C2F78)+0x29 or *(RVA 0x3C2F84)+0xFC/+0x29 is set. Show requires +0xB8 == 1, which makes the layer clause false while a box can open. Whether a shown box sets the other inputs or CUIScene+0x174 is UNKNOWN. The design adds a guard (hide the box before release) and gate G11 rather than asserting the risk.
- Wrong #7: the 56 ms arbiter-change and position-freeze claim is not supported by the host logs. → Correction confirmed and applied. In one sampled Kamo press, arb50/arb58 are unchanged across the create line, and the lead was already stationary before it. The claim is now UNKNOWN. The timing claim was lowered to the bracketed range of about 1.8-2.3 s over 13 presses, with 18 creates in 11 sessions (not six presses at two-decimal precision).
- Overclaim: G5 is listed as open although the string overloads' routing is already CONFIRMED_STATIC. → Confirmed. 0xAC610 calls 0xAC580, which calls 0xABB80; 0xAC630 calls 0xAC580; 0xAC4B0 calls 0xABB80 with the caller's mode; 0xAC6A0 passes mode 6 or 0xE. None has a native caller (data/export references only). G5 is narrowed to the live host-side box availability check.

### paused82

- Wrong: the draft's line numbers for dirty files (lan_story_world.c:2707, dllmain.c:1179-1187, lan_story_spawn_trace.c:341-348) should be HEAD 2658, 1165-1173 and 340-347. → Partly rejected. Those HEAD numbers are for d437693. At the requested HEAD 9081425 (which committed that work) the draft's lan_story_world.c:2707, src/loader/dllmain.c:1179-1195 and lan_story_spawn_trace.c:341-349 are correct, re-read in the clean tree. The review is right that the plan's §10 row and the lan_story_runtime.c seams had moved: now docs/coop-story-plan-54.md:6802 and lan_story_runtime.c:2102 (service), :1579-1580 (Service with mask 0 in the drain), :1850-1851 (ReleaseBeforeWorldChange), :1954-1956 (ReleaseForLoad). Applied.
- Wrong: the H1b dependency. Lead entry into a TEMP suspends the exterior through 0x10B500, which neither 0x13D5B0 nor Option C undoes, and the plan says lead entry parks and rejoins members, so it is UNKNOWN whether Tal can be outside while the host is inside. → Partly confirmed. 0x10B500 at 0x6527/0x6595 and its +0xF4 write at 0x10B521 are CONFIRMED_STATIC, and with keep_live=0 neither option undoes it (applied). But the saved-story host installs lan_story_split (lan_story_runtime.c:3720-3721), which installs the TEMP exterior adapter (lan_story_split.c:743-745) and keeps the exterior live when the lead enters with members outside (lan_story_split.c:662-683); docs/story-independent-areas.md:700-722 records CONFIRMED_LIVE that the outside player walks and exterior NPCs resume. The 'parks and rejoins' lines (plan 6721, 6725) concern a member inside a shop when the lead enters, not one waiting outside. So the scenario exists (D11), and the draft's cluster-pause INFERENCE stands for the kept-live case. docs/story-independent-areas.md:644-647 ('story runtime does not install it') is stale on this point.
- Missing: script DeactivateCluster exports 0x35AA0 and 0x35E20 'also call 0x35FE0'. → Corrected. Only 0x35E20 calls 0x35FE0; 0x35AA0 clears mask 0x4, broadcasts and calls the listeners inline. The conclusion holds: scripts can pause a cluster without the leader moving. The door-transit finish 0xCF620 decrementing manager+0x30 and calling 0x352D0 is confirmed, and *(0x409D78) is the cluster manager per lan_story_spawn_trace.c:341-349.
- Missing: 'Zone streaming 0x109800 adds references equal to *(0x408DA0)+0x2A to newly loaded entities and later removes them.' → Confirmed with a refinement. In 0x109800 one stage adds *(0x408DA0)+0x2A references per new entity (0x109A29-0x109A7F) and another removes one (0x109B0A). The entity method 0x13DC00 also adds one plus that count at creation. Recorded as CONFIRMED_STATIC; the net count for any one chest at runtime is UNKNOWN until G1.
- Missing: CGenericEntity::SetCollision can be called by scripts to make a paused object solid or not. → Writes confirmed (CONFIRMED_STATIC: 0xCB1A0 writes collider +0xF4 and CPosition+0x100 regardless of pause). It has no direct caller and no absolute reference in the image, so that scripts call it is INFERENCE from the export, not confirmed; which route scripts do is gate G9.

### doors67

- Wrong: 'cluster update 0x4352D0 ... callers 0x43513B and 0xCF655' mixes VAs and RVAs. → Confirmed and applied: the RVAs are 0x352D0, 0x3513B and 0x35120 (SizeOfImage 0x45F000; two call sites in the full disassembly). The draft also had three VAs the review did not flag, now normalised: 0x438500 is RVA 0x38500 (SetStaticCameraPosition, export 1097), 0x4E93B0 is RVA 0xE93B0, and 0x58D8C0 is RVA 0x18D8C0. Plan line 180 keeps its VAs; the plan was not edited (read-only task).
- Wrong: kind 239 is SFXE007_CHEST_OPEN, not SFXA203. → Confirmed and applied. Extra finding: the shift came from 8f7ec42 (2026-10-15), which inserted SFXM005_HEAL into the world list. Before that, SFXA203 was 239, which is where the draft's number came from (docs/coop-story-plan-54.md:4819-4836, 'last kind 239'). Every kept runtime.log* file postdates 8f7ec42. G5 is reframed as 'no client spawn proven'.
- Overclaim: 'use candidates ... admitted for the front only' and '0xD330 appends a candidate only for the front'. → Partly applied. Front-only admission at the candidate append is withdrawn, because 0x124320 admits other players through volume+0x144 bit 0x10. Whether 0xD330 appends them is UNKNOWN and is now gate G12. The native dispatch 0xD6F0->0xD7A0 is still front-only (docs/coop-story-plan-54.md:4565-4574, static trace 2026-10-13), so a remote use still needs the scoped front. That part of the draft stands.
- Overclaim: cited lan_story_world.c lines 488-495, 1456-1462 and 2658-2707 are dirty-tree lines; HEAD d437693 has them at 467/1406/2606/2630. → Moot at HEAD 9081425. The E2 v12 work is committed there, and the cited blocks sit at exactly 488-495, 1456-1462, 2658-2668 and 2678-2707 (checked). The review's d437693 numbers no longer apply. Separately, and not raised by the review: the draft's lan_story_runtime.c numbers (2100, 2128, 2141) were stale at HEAD and are corrected (ranged loop 2134-2160, melee loop 2162-2187, use attempt 2177). The door camera range is corrected to 110-169.
- Overclaim: 'SFXAR005/006 have 39 zone references each'; the review counts 41, and says REVEAL_DOOR has 0 text hits in SOLData.baf. → Count dropped from the result. A case-insensitive search finds 42 occurrences of each SFXAR005/006 name in SOLData.baf and none in SOLWORLDM.gex, so neither 39 nor 41 'zone references' is reproducible. The review's 0 text hits for REVEAL_DOOR is a case-sensitive count. Case-insensitively there are 3, all in a resource-name list or the resource's own model and animation data. The conclusion stands: no zone or script names it (INFERENCE).
- Overclaim: alpha_apply count 1424 is stale (now 1463). → Applied and extended. The count is now 1463, all kind 0x0F9C. The log stops after 64 lines per process (lan_story_world.c:2660-2663), and two client runs reached that cap. The draft also called the door alpha branch CONFIRMED_TEST. Only the wire codec is tested (tests/lan_party_session_test.c:1592-1593); no test covers the client apply branch, so that branch is lowered to source only.
- Missing: path S latency on PVS doors, a scripted open lagging up to ~600 updates that 'the host-lead cycle never uses'. → Not applied as stated. The use path waits on the same CDoor+0x130 timer, in states 1, 2 and 3/4 (0xCE76A, 0xCE7B4, 0xCE82C), just as the script path does in state 0xB (0xCE957). 0xCE300 sets the timer on both paths, and the callback registered by 0x1B640 (table RVA 0x2CC644, pointing at 0xCE2E0) clears it. So the delay is a property of any PVS-named door, not a path S risk. It is recorded as a fact and folded into G4.
- Missing: LockDoor/UnlockDoor/SetAllDoorsLocked are stubs, 'so nothing at runtime can toggle CDoor+0x73 bit 1; the locked state is data-static'. → Partly applied. All four lock exports resolve to the RET stub 0xA2900 (confirmed). The 'nothing at runtime can toggle' conclusion is not supported: other writers of the usable flag byte exist (0xB5A3D, 0xB5AB5, 0xB5BAC, 0xB5BCB, 0xB622D, 0xB623F, 0xB679E). Whether any of them changes bit 1 is recorded as UNKNOWN.
- Missing: kept logs hold 0 host door_collision events and 0 door_camera engagements, so the acceptance rests on 2026-10-13/14. → Applied in part. There are no host 'story_door_camera event=engaged' lines and two client door_collision changes, and the acceptance does rest on status-matrix rows 100-101. '0 host door_collision events' is not evidence of anything: the source has only a client-side door_collision log (lan_story_world.c:2704).
- Missing: 0xB5E40 requires the owner's trigger component '(entity+0x50)->+0x44 bit 1' and confirms G8's candidate. → Applied with a correction: the test is mask 0x01 at component+0x44 (0xB5E4A), not bit 1. Confirmed that entity+0x50 = entity+0x35C (RVA 0x14B41C) and that 0xD9A0 sets the mask on the candidate it processes.
- Missing: path N should also hook +0x64 (0xCEF80), whose caller is as UNKNOWN as +0x5C. → Applied, with callers found. The opener's arbiter update 0xDBDC0 calls the current usable's +0x60. 0xDBE40 (referenced at RVA 0x2CCA18) calls the current usable's +0x64. The callers of +0x5C (0xCEED0, which runs the end 0xCF620 at state 7) and +0x38 (0xCE5C0) remain UNKNOWN (G3).
- Not in the review (found during the fold): route_relevance said the host plays Tal as the lead. → Corrected. The acceptance route is Ailish host / Tal client (docs/coop-story-plan-54.md:6789; status-matrix row 99). Tal already reaches the H1a use attempt. The ranged-seat gap therefore affects the #87 pairings (Ailish or Elco as client), not the route pairing.

### interiors83

- Wrong #6: 'docs/coop-story-plan-54.md is cited and is modified in the worktree; the file is dirty.' → True only for the earlier d437693 snapshot the draft cited. At HEAD 9081425 there are no tracked changes (only an untracked docs/coop-story-research-54.md, which this item does not cite). The plan's diff from d437693 to 9081425 starts at line 6743, so every cited plan line below that is unchanged. All citations are now anchored to 9081425, and the draft's 'none of the cited files is dirty' is replaced by 'HEAD 9081425, clean tree'.
- Wrong #9: '0x109F2A is HYPOTHESIS; the live state-4 transient points to the state-4 setter's activation at 0x10B1A4; the real activation site is UNKNOWN.' → Main correction accepted and applied. 0x109F2A sits in 0x109800's descriptor+0x30 == 1 branch; SwitchZoneNOW sets +0x30 = 2 (0x5B18), and that branch instead calls 0x110510 and sets render state 1 (0x109E4B-0x109E66). The site stays UNKNOWN, and 0x109F2A is at best a HYPOTHESIS. The review mislabels the case, though. The setter's jump table at RVA 0x10B2C4 sends state 3 to 0x10B142, whose activation call is 0x10B1A4, and state 4 to 0x10AFF5, whose activation call is 0x10B000. Both appear among the G6 candidates with the correct case. The 0x109F27 store is described as having an UNKNOWN target, because its stack slot is reused as a loop index (0x1099AE).
- Missing: 'Every walk switch into a foreign house, as in the incident, re-runs that house's OnMain on the host.' → Kept with a narrower scope. Cases 3 and 4 of the setter do push the OnMain string (0x10B010, 0x10B1BC), so a switch that moves a resident house from state 2 to 3 or 4 reaches it; that the script runs is INFERENCE. But the setter returns at once when the state is unchanged (0x10AD8B-0x10AD8D), and a switch into a flagged zone never demotes the old current zone (0x573B-0x573F). Repeated flips between two zones already in state 4, as in the incident, therefore do not re-run it. Recorded under facts (rendering) and G5.
- Overclaim: 'The state-2 path also posts event 0x43F3B0 and may call 0x4174D0 (0x10AD70 case 2; 0x5741-0x579E).' → Accepted with more precision. The state-change event (RVA 0x3F3B0) is posted for every state change, both by the setter (0x10ADC4) and by the Update demotion (0x577A), not only for state 2. The 0x174D0 call is confirmed only for a change from state 1 to state 2 with descriptor+0x4C set (0x10B116-0x10B122; 0x5786-0x5792). The bit-0x80 marking applies only to objects that already have bit 0x40 (0x10D220).
- Wrong #2: the invalid-sector path seeds 'from a position lookup (0x431F10)'. → Accepted. The address is a VA; the RVA is 0x31F10 (call at 0x185207), and the result is written with that RVA. The rest of the correction is confirmed: 0x185370 has three direct callers (0xB2832, 0x1850EF, 0x185212), and 0x185150 is reached from 0x1113B7 and reseeds only an invalid sector.
- Wrong #1: 0x185370 'uses EDI as the implicit tracking register, not ECX'. → Confirmed (0xB2830 MOV EDI,EBP before the call at 0xB2832; 0x1850E2 MOV EDI,ECX before 0x1850EF; 0x185210 MOV EDI,EBX). The draft had claimed ECX only for 0x1850E0, which is also correct (0x1850E2). Both ABIs are now stated separately, and the setter seam option in design step 2 uses the EDI ABI.

### H1rest

- Overclaim: 'none of the files this item touches are dirty' hid that lan_story_runtime.c and the plan were dirty (review at d437693). → Moot at HEAD 9081425: the tree is clean apart from the untracked research doc. All citations were re-pinned at HEAD; the runtime.c lines moved by +6 to +36 (house camera 742-751, HP mirror 775-781, split mask 1801-1835 and 2111-2116), and the §10 H row is plan 6798. The AGENTS.md coordination rule for the shared runtime and remote-interior modules still applies and is listed under dependencies.
- Overclaim: 'Kamo needed the camera in its row, which suggests the client's house scripts do not run' has no plan or source evidence either way. → The review is wrong about the evidence. src/hooks/lan_story_client.c installs holds on the trigger sweep (RVA 0xCFD0 via 0x3373F), the trigger dispatch (RVA 0xC6E0 via 0x77E24) and cluster entry/exit script submission (0x3543F, 0x35527), from the client's pause acquisition until native exit (:28-32, :238-295, :715-742, :1451-1453). src/hooks/lan_story_client_camera.c:254-256 also states that the client holds cluster scripts. The hooks are now CONFIRMED_STATIC. That the farmhouse room cameras never switch on the client stays INFERENCE: the kept client logs show no cluster_script_held line. The draft's G2 RVAs (0x123D30, 0xD330) were not re-checked and are replaced by the cited hooks.
- Missing: a take-along into the castle Kamo is possible whenever the mask is empty, for example with the NG seat-locked spectator at the castle start. → The general point stands (an empty mask leads to the native whole-party entry; INFERENCE from the 2026-10-14 log) and is kept as O7 and a design rule. The example is weak. At the NG castle start the party is Tal alone, and the client's seat is locked to Ailish, who has not joined yet (plan 70-75), so there is no member body to take along. On later castle visits the castle Kamo's writes no longer fire: block past 1050 and quest stage past 0 (CONFIRMED_STATIC gating; the block value is INFERENCE).
- Review line references: remote_interior.c:580 (same-house filter), :585-586 (lead-near refusal), :716 (client admission), and the §10 row at plan 6788. → Re-pinned at HEAD 9081425: the one-house and lead filter is :583, the lead-near refusal :588-591 (client :725), the client admission starts at :711, and the §10 H row is plan 6798. The substance of each point was confirmed.
- Overclaim: many GEX details were accepted only on the draft's word. → Partly closed in this fold, by disassembling the owner's own GEX. Now CONFIRMED_STATIC:
- NPC_Innkeeper_Talking (0x7F647-0x7F8BF: 25 florins, fade, GEN_InnHeal, PopToNamedLocation to STAIRS_AILOC);
- GEN_InnHeal (0x31BF5-0x31C92);
- NPC_Bunny_Receipt (0x7D29A-0x7D340);
- NB_RestAtInn's price (0x7D1FB);
- the alarm sequence (0x583D0-0x588A8, using StartTSA|B, not UIStartTSA|B);
- StartTrading|N (0x36E29-0x36F27) and StartBlackSmithShopping|N (0x36DA5-0x36DDD);
- the castle Kamo T1b and T1c shop-only paths.
Still from the draft pass only: Galex (0x87AEF-0x87DE0), the trader cave doors (0x3FE43-0x3FFA5, 0x41270), the castle Kamo heartbeat (0x5B9CE), CamTrig1/2 camera targets, and the smith name ids 9431 and 9118.
- Wrong: the freeze cause is labelled UNKNOWN, although the 2026-10-14 log carries a cause candidate (an empty mask). → Accepted and extended.
- The private log shows only three eligibility lines, below the 32-line trace cap (src/hooks/lan_story_split.c:701-703), so the empty mask is not an artefact of the cap.
- The client's lease was still at the offer stage.
- The last host sample put Tal's body about 39 units from Ailish.
This contradicts the plan's door-trigger HYPOTHESIS (plan 4905-4906); the cause is INFERENCE. The post-exit offer refusal stays UNKNOWN.

### S0priv

- Review wrong[0]: only serial 2 is evidenced on the client (client audio and overlay line=2 flags=0, no area_hidden), so flags=0 came from the host. Serial 1 is UNKNOWN because the client's shop was open (in_shop=LNBr_Kamo_shop). → Partly rejected. Host-side private classification of both 2026-10-18 spans is CONFIRMED_LIVE. However, the review's 'in_shop' field is the client's shop zone (SudekiMpLanStoryRemoteInteriorClientShopZone, lan_story_runtime.c:2805), not shop_open() (lan_story_runtime.c:92). The real suppression is the client's own Kamo talk lease, which per the private live logs stayed open from before the first host span until the host left the shop; the host exit and the client's interior_exit align within about 0.1 s. The client's line=2 flags=0 trace came about 7 s after the host's second span had ended, when the record is idle whatever the privacy state. So the client side is UNKNOWN for both 2026-10-18 spans. Client-side evidence does exist from the 2026-10-17 evening session: the client, in the same shop as the lead with its own talk closed and no area_hidden, presented flags=0 for both private line serials within about 0.3 s of the host lines (CONFIRMED_LIVE, logs only).
- Review missing[1] (and the draft's state_today and fact 2): an active line with a NULL speaker arises from b4!=0 (isChoice, via AskQuestion forwarding) or from the 0x413CF0 fallback, and a choice line with an empty speaker makes the conversation public. → Corrected beyond both. The AskQuestion forwarding (RVA 0x13730: ActivateLine(..., TRUE, TRUE, 5th, 6th), with the 7th going to the ask box 0xA3D90) is confirmed. But for a b4 line, ActivateLine hands the text to the UIDialog box (0x153F0) and then calls 0x13500 again (RVA 0x13C43-0x13C4C, ECX=this per disassembly). That returns mode +0xC8 to 2 and nulls both TPtrs before any capture. So b4 lines are never captured as lines: they keep the privacy state and their text is never sent. ActivateLine never leaves an active line with a NULL speaker, because it resolves one through 0x13CF0 or refuses the line. A NULL speaker in mid-line would need the TPtr to be cleared, which is UNKNOWN. CONFIRMED_STATIC (Ghidra and objdump, this fold).
- Review overclaim[2]: fact 9's speaker-argument counts (theTrigger ~2597, PC locals ~236, p_Entity 98, null 45, theActivator 6 roots) are unreproduced. The reviewer's recount gives theTrigger 1827 + theEntity 657, p_Entity 96, PC locals ~300, theActivator 10 sites in 6 roots, and no null-speaker sites. → Neither exact set was reproduced in this fold; the census data is private and not committed. Kept at INFERENCE as a qualitative claim: the NPC trigger entity speaks at most sites, PC locals at a few hundred, and the activator in about 6 roots, which both decodes agree on. The unreproduced null-speaker count is dropped.

### S1

- Wrong #4 (seam line numbers): the review's working-tree numbers, area/XP publish at lan_story_runtime.c:3370-3380, banner service at 3406, XP consume at 3430-3431. → Neither the draft's numbers nor the review's match HEAD 9081425, which commits the shot-audio work. Re-cited at HEAD: area gate at 3371, 100 ms area/XP block at 3380-3391 (XP publish at 3388-3390), banner service at 3416, XP consume at 3438-3441, door cue at 3412-3414. The review's point that lan_story_spawn_trace.c:642 is a different-revision number does not apply at HEAD, where :642 is correct.
- Wrong #3 (chime path): lan_story_character_audio.c is 'dirty and under concurrent edit', and lan_arena_spirit_audio.c has no uncommitted changes. → The technical correction is confirmed and applied: PlaySpot (lan_story_character_audio.c:719-756) calls the guarded EmitterPlay and avoids 0x17090, door cues use it (lan_story_runtime.c:1019-1022, 3412-3414), and the three cue names are confirmed at SOLWORLDM.gex 0x35ED7, 0x35FF5 and 0x3608E. The file-state part does not hold at HEAD 9081425: the tree is clean, and the shot-audio executor was committed in 9081425. It is kept only as an ownership-coordination dependency.
- Wrong #2 (timing): the client's last sample at ms=143022/153019 precedes the host's 2000 at ms=173029/170038. → The conclusion is applied: those two sessions do not show a simultaneous 2000 against 1400. But ms= is time since that group's tracking started (lan_story_spawn_trace.c:639), not a shared clock. The ordering holds only because both sides track the same ambush group, which is INFERENCE. The 'clock=' field name is confirmed (:638).
- Missing #1: every client save load, the D6 join included, already takes the native SetStoryBlock path that O1/G1 treat as risky. → The native call at 0x100622 (inside the save-load function 0x100280, with SetStoryBlock(save+0x1390)) and the '-Story' call at 0x10525A are CONFIRMED_STATIC and added. That the D6 join load uses 0x100280 is INFERENCE. A load-time call, with zones mostly not yet resident (INFERENCE), does not show that a mid-session write on a client with resident zones is safe. So this is recorded as input to O1, G1 and G6, not as evidence that removes the risk.
- Missing #7: lan_story_client.c:271-275 records as CONFIRMED_STATIC that cluster updates submit OnClusterEntry/Exit outside the paused scheduler. → At HEAD the source comment states this behaviour but carries no evidence tag at that site. It is cited as the reason the client cluster-script hold exists. The hold itself (lan_story_client.c:266-291) is CONFIRMED_STATIC; the native behaviour stays part of G2.
- Missing #8: the installed opcode-slot adapter could observe QL_* against QL_NM_* calls exactly. → Added as an alternative classifier, but as HYPOTHESIS. dispatch() (lan_arena_cast_context.c:548-567) wraps opcodes 0x27/0x28/0x29 per thread. Whether it can identify the callee hash before the call, and whether doing so keeps one adapter owner per seam, is not shown. Part of G3.

### S2

- Overclaim: '§10 lists D12 S1–S4 as the next H work' holds only in the dirty working-tree plan; the committed d437693 §10 row H reads 'shops/talk host-only'. → Not applied at HEAD. The review compared against d437693. At HEAD 9081425 the committed §10 row H (docs/coop-story-plan-54.md:6798) lists 'D12 S1–S4' in its next-step column, so the draft's statement holds at the cited commit. The d437693 text ('shops/talk host-only', its plan:6755) is superseded.
- Wrong #1: retire 0x5C3AC0 has five callers, so 'the game does have native cancellation paths' and the cancellation gate and O3 should start from 0x425F40. → The caller list is confirmed and applied (call-site search: 0x5C33A4, 0x5C3995, 0x425F71, 0x4FCBC0, 0x5C368A; 0x4FCB40 is the vtable slot at VA 0x6CD164; 0x425F40 callers 0x4FCC23, 0x4FCE35, 0x539220). The conclusion is applied only in part. These are native kill paths (CONFIRMED_STATIC), but they are not shown to be a verified cancellation contract for a conversation task. They retire without running EndInteractionScript's undo (AI override count, banks, dialog, CSpeech interacting flag), so a kill would leave that state behind (INFERENCE). The gate now starts from these paths, and their use is recorded as owner decision O10, not adopted. The review's name for 0x4FCE00 ('CGELEntity::RestartHeartbeat') was not verified and is not used.
- Missing: AiOverrideControl/AiDefaultControl call 0x4EC350, which writes controller +0x84/+0x258 and calls 0x4290D0 etc. when the entity is the controller's current entity; StartInteractionScript calls AiOverrideControl(thePlayer), and the design allowed it as ACTOR without an audit against input containment. → The code facts are confirmed (0x4F60D0, 0x4F6100, 0x4EC350 with its controller branch at 0x4EC3D3-0x4EC466; call at logical 0x37E30) and the design now ANSWERs player-side AI control. The implied consequence is disputed. The controller branch is taken only when the AI's entity equals the controller's +0x248 entity, and the client's containment requires that to be the save's leader (lan_story_runtime.c:495-498; lan_story_input.c:298-302), not the client's own character. So on today's client the branch is probably not taken (INFERENCE). The goal clears and override count on the replica's AI still happen, and that effect is an open gate.
- Missing: design step 7 contradicts itself (ACTOR allows AiSetTargetDirection on theActivator against O1's default of no player turn), and the facing ANSWER does not cover the first AiAtTargetDirection(theActivator) loop at logical 0x8C5B5-0x8C5CF. → The contradiction is confirmed and fixed: theActivator's AiSetTargetDirection and the other player-side actor natives are now ANSWERed by default. The second half is only partly right. The draft's step 7 did cover the player's wait ('the player's own turn and wait follow O1, default ANSWER TRUE with no turn'), but the policy table in step 5 did not list it. Step 7 now names both loops explicitly.
- Wrong #7: 'an opcode-slot refusal has to restore pc (-1) and return 2' is labelled CONFIRMED_STATIC; 'the source contract says the opposite: the opcode-table hooks are too late to safely yield'. → The evidence level is lowered to INFERENCE (untested), as the review asks. 'Says the opposite' overstates it. The contract (lan_arena_cast_context.c:21-22) says a plain yield at the opcode slots is unsafe because pc has already advanced, which is the draft's reason for needing a rewind. It neither proves nor disproves that a rewind would be safe. The design does not use opcode-slot refusals on the client, so the point does not change the design.
- Line citations in the review (lan_story_runtime.c:2816-2822; client log line numbers; plan:5780-5786, 6738, 6721) and the draft's d437693-dirty numbers. → Re-cited at HEAD 9081425. The UI-task allowance is at lan_story_runtime.c:2822-2832, and the host-only gate for service_story_controls is at :3316 (call :3347, cast install :2128). The plan lines 5778-5786, 6721 and 6738 hold at HEAD, and the shop, client and task-trace line numbers did not move (those files are unchanged since d437693). Log line numbers are not reproduced, by the publication rules; the conclusions were checked in the private live logs: the interior-exit drain happened during the farewell hold, and a parent-0 menu task was created and adopted on the client.

### S34

- Wrong #1: 'Today a client can open the native save menu and write a local save, so client-local flavour flags CAN persist on the client's disk.' → Partly applied. Confirmed and applied: the client save path is unguarded in the saved-story profile. dllmain.c:1317 returns before the save-book config (:1602) and other installs (:3066), save_book_intercept.c:481 is a pass-through prototype, and the guard is a Phase I next step (plan:183, :6799). The draft's 'client save path is blocked' is withdrawn. Not confirmed: that a client can actually reach the save menu. The vanilla save-book path is a world interaction, and the client trigger dispatch is held (lan_story_client.c:238), so this path is INFERENCE closed. Reachability is recorded as UNKNOWN with new gate G15, and the guard is added as an S3 dependency.
- Wrong #2: 'There is no Cargo root in the census ... drop the Cargo set.' → Correction partly wrong. There is no root named Cargo. But 'Cargo' is a shared GEX helper, Cargo_Quest_Stuff, called by the eight roots that write quest 0x6DE10317 (Galex, Kilk, Kweeg, LightHouseJim, Merther, Sasan_Miko, Teebor, VergerSherry), and all eight are in the 41-root S4 set (census rerun). The label is kept, reworded as 'the eight Cargo_Quest_Stuff roots', and the duplicate Kilk mention is removed.
- Wrong #5: plan citations. Review gives D6 :63, the host-Kamo row :6724, §10 row I :6780, and calls :6779 row H correct. → D6 :63 and :6724 confirmed and applied. At HEAD 9081425 the §10 rows are H = plan:6798 and I = plan:6799; :6779/:6780 fall inside the 2026-10-18 error-87 §9 entry. Citations use :6798/:6799.
- Overclaim: 'Save header read is RVA 0xFF6B7' should be 'the only story-block access is a WRITE of DAT_7C2FEC into the save buffer'. → Both are partly right. At RVA 0xFF6B7 the builder reads the int16 global (movsx) and stores it at save buffer +0x1390. Reworded as 'the save builder copies the block into the save buffer at +0x1390 (RVA 0xFF6B7)'. A scan of RVA 0xFF030..0xFFA00 finds no other block reference.
- Overclaim: 'Before Ailish joins at block 1400': the plan has no such number; source not cited, unverified. → Source found. The static GEX conversation census records the lighthouse trigger writing story block 1400 with Ailish joining. Kept as INFERENCE (census decode, not in the plan) and labelled so.
- Missing: the 'gate' events in client logs (story_shop event=gate) are not among the adapter's phase names; identify them before drawing conclusions. → Resolved. They are the prompt input-gate transitions logged by service_shop (lan_story_runtime.c:2800-2804), capped at 64, not conversation events. The 'no client conversation' conclusion stands at CONFIRMED_LIVE for those runs (log evidence).

### tutor80

- Wrong #1: 0x57EB70 has no slot-0 check; only 0x57DC50 and 0x4B5330 (and by shape 0x57DDB0, 0x4B5450) compare with group+0x90. → Applied for 0x57EB70: objdump shows D7, VA 0x808D9C and actor+0x94 tests but no slot-0 compare (RVA 0x17EB8C-0x17EBB5). The review's 'only' is too narrow, though. The same lea of group+0x90 follows the D7 test at ten more sites, 0x57DF24..0x57EA5C, making fourteen slot-0-gated sites. Three further D7 readers (0x4C9E2C, 0x4CA7CB, 0x4CAB71) are unclassified. Fact rewritten accordingly, with the compare at the ten sites kept at INFERENCE.
- Wrong #2: BS_TalTraining is the score cued at Tal's tutorial start; retire it only as a unique detector. → Applied. Confirmed: the IsStoryBlockBefore(1000) test at 0x66CF7 branches to 0x66DC1. The branch covers the BS_TalTraining push at 0x66D05, the cue at 0x66D0A, RunTalTutorial at 0x66DA5 and SetStoryBlock(1000) at 0x66DB0. The other three references are at 0x3D222, 0x6827C and 0x69FBA.
- Wrong #3: the dispel callback despawns HC_AklorianBarrier itself; runailishtutorial's clean-up is only a fallback (logical ~0x5F4A0..0x5F4B8). → Substance applied: the despawn at 0x5F764 runs after the cart's CInvisible component reports IsDispelled. The review's offset for the fallback is wrong: 0x5F4A4..0x5F4BD is the Formation-weight restore. The fallback is the call at 0x5F496, behind IsSpawnGroupActive(0x205) at 0x5F482 and a stage-variable test at 0x5F472.
- Wrong #4: the intro letterbox spans lines 1-7 (about 39 s), and one client share pair covers it. → Applied after checking the private live logs: line 1 has flags 31 and line 7 has flags 0. The score is cued as the intro ends, and there is one active=1/active=0 pair on the client.
- Wrong #5: Tal's cited movement came after both dialogue holds; movement between the intro's end and line 8 is UNKNOWN. → Applied. The first movement after the intro follows the line-9 hold release. No moving=1 event exists between the intro's end and line 8. The hold durations of about 3.4 s and 14.4 s are confirmed.
- Wrong #6: a sixth SetTutorialModeEventsEnabled site (false) at 0x55C84. → Applied. A full scan finds six Tutorial_SetupTutorial sites plus the two HINT sites. The fold also found that 0x55C84 sits right after the stage's wait-loop exit, so D7 probably drops at each stage's end (INFERENCE). This replaces the draft's 'never cleared' reading.
- Wrong #7: SetUILeadOnly(true) sits at the end of AilishTutorialTSA, not after SetPlayerSwitching. → Applied. SetUILeadOnly(true) is at 0x5F2EA, just before SwitchActiveCamera, CameraReturnToDefault, EndTSA, AiOverrideControl(Tal) and TSAFadeIn. SetAsLeadPlayer at 0x5EE8B and SetPlayerSwitching(false) at 0x5EE93 are confirmed.
- Wrong #8: the AiSetStateData|PSNSN arguments are (Tal, Formation, 0, weight, 0.0) and later weight 1.0. → Applied. Confirmed by decode, and 'weight' = 0x50A09EB9 in the gex dictionary. The stage-name hashes are confirmed.
- Overclaim: the #79 alpha relay is live only before the tutorial; the client's chest may have been omitted as ambiguous. → Lowered to 'mechanically live before the tutorial only'. The second run's pulse was corrected to 0 then about 36 to 64. The review's ambiguity explanation does not hold: the host catalog still lists a single 0x6AA4C14B after the second chest was added, so the both-sides omission did not happen. The client's 64-line alpha log budget also still had room in the tutorial window. Which entity the host publishes stays UNKNOWN (G10).
- Overclaim: the +0xC0 store is not visible in SetAsLeadPlayer itself. → Disputed. Objdump of the supported image shows SetAsLeadPlayer (RVA 0x24E40) itself doing a lea of group+0xC0 and calling the pointer-assign helper 0x401750, at RVA 0x24E77-0x24E7F, once 0x423750 accepts the actor. Kept at CONFIRMED_STATIC with the condition stated. The consumer at RVA 0x231CC-0x231E4 is also confirmed.
- Overclaim: the host does not capture its view every frame; it captures every controller tick when capturable. → Applied: capture happens on ticks at least 15 ms apart (lan_story_runtime.c:3476-3484), only when movement capture succeeds, and an unobservable camera leaves view.valid at 0 (lan_story_snapshot.c:159-161). The fold also found that today's cinematic predicate requires view.valid (lan_story_runtime.c:667-668), so a missing view ends the share. Design step 4 now keeps a tutorial share through such frames.
- Overclaim: 'walking from the slot-11 save triggers it (CONFIRMED_LIVE, 4 runs)'. → Applied. Four starts are CONFIRMED_LIVE. Two runs completed: one walked from the save, and one placed at the border with no zone key that walked in. Two TestStartZone runs stalled before HC_SE_DISPELL. The slot number is not re-verified here and is dropped.
- Overclaim: J's pre-join spectate covering Tal's castle tutorial is stated as fact. → Applied: now INFERENCE. It assumes the host plays Tal. J is not started (plan section 10).
- Overclaim: Ailish has no native skip, unlike ICExt_TalTutorialSkipper (the IsDevMode gate is probable). → Applied and sharpened. IsDevMode is called at 0x552F1 (not 0x552FA), and the first skipper reference is at 0x55306 (not 0x55307), inside the IsDevMode()==true branch. The second reference (0x55363) is gated by a script flag. Whether players get a native skip is UNKNOWN; the O2 wording was changed.
- Overclaim: line references; the comment is at lines 35-38, and the design refs match neither HEAD (3355/3592/3811) nor the current tree. → Applied for the comment (35-38, define at 39). The review's 'HEAD' numbers are those of the parent d437693. At HEAD 9081425 (clean) the music capture call is at :3395 and the resets are at :3632 and :3851. All runtime refs were re-anchored at 9081425, and the draft's dirty-tree note was dropped.
- Missing: story-block gates; on NG the tutorial fires on the first Hilltop entry with block < 1500, i.e. the outbound leg if the route crosses that cluster. → Gates applied (1500 for Ailish, 1000 for Tal, plus the boss's 1200 and SetRandomEncounterTimer 5000/1). The outbound-leg conclusion is disputed: a GetPC(3) validity test precedes IsStoryBlockBefore(1500) (logical 0x68D8C-0x68D91). Mungo, on the outbound leg, is fought in the same Hilltop handler, so the route does cross that cluster, and whether the tutorial can fire there depends on that test. The leg is UNKNOWN; G6 was reframed.
- Missing: dispatcher coverage, and that a held non-lead trainee can still raise tutorial events. → Applied in the facts (fourteen slot-0 sites, 0x57EB70 for any actor), in G5, and in the case-2 fact.
- Missing: duplicate chest identity during the tutorial, and whether the #79 relay can reach the client's chest. → Applied with a correction: the host registry holds two such chests, but its catalog lists one (no ambiguous omission). Added to the facts, G10 and O1.
- Missing: total share length (~39 s plus ~84 s) and behaviour across view-invalid gaps. → Applied to G9 and design step 4.
- Missing: the host's task list, hints and cue audio are not relayed; no owner decision on this. → Applied: a new fact (CONFIRMED_STATIC by absence) and new owner decision O8.
- Missing: log the sixth site's effect in G1 before concluding that D7 never clears. → Applied to G1 and fact 3.
- Missing: how the tutorial hold interacts with native AI edges and the existing hold reasons. → Applied. New CONFIRMED_LIVE fact: the host drained and re-bound Tal's lease twice at the intro's end. Design step 3 now gates admission only and leaves the AI edges and the drain/re-bind native, combining with native_held and the front-use lock.
- Missing: the G4 expectation should note the climbing hold counter and that the TSA continues. → Applied. +0xC0 is never cleared by the hold, the log repeats every 600 holds (lan_story_death_hold.c:96-98), and the script discards SetAsLeadPlayer's result (INFERENCE).

### birds81

- wrong[0]: 'After B registers, no birds instance is relayed' needs a residency qualifier; A rejoins after B unloads. → Confirmed and applied. In the private live logs, B unloads on both machines near the end of the session (about four hours after it registered). A client catalog of 4 then lists 3fdb16e3, and identity_subset matched=3 is followed by client_rebind count=4 previous=3, so A matched the host frame again. The rejoin was added to the facts, and a Stage 1 hand-back step was added to the design.
- wrong[1]: the host view after B is unsupported as 'probably animating'; record UNKNOWN with #82 as the stronger hypothesis. → Partly applied. 'Probably animating' was removed and the host view is now UNKNOWN. #82 was not adopted as the stronger hypothesis. The same logs show the host stepping A natively (local_time 0 to 459.9) while Ailish was current in New Brightwater, a zone other than A's. Only after she entered SE was the connector logged current=0, native state 2. Both 'paused' and 'still stepping' are kept as HYPOTHESIS, to be decided by G1.
- wrong[2]: the G1 probe chain treats renderer+0x98 as the row. → Confirmed and applied. target() reads channels=*(renderer+0x98) and rows[c]=*(channels+c*36) (src/hooks/lan_story_world.c:578-583), and the Ghidra view of RVA 0x222370 dereferences +0x98 twice. G1 now includes the channel-array dereference.
- wrong[3]: SavePoint is described as 'selectors 0/1/2, states 0/1/128'. → Confirmed and applied. The tally across the live run logs shows selector/state pairs 0/0, 1/1, 2/0, 2/128, 3/1 and 3/65, so selectors 0-3 and states 0, 1, 65 and 128.
- overclaim[0]: 'No spare field exists for a secondary key'; the reviewer says ActorValid does not reject nonzero hit_points for scenery. → The reformulation was applied: no field's validated meaning admits a secondary key without a protocol change. The reviewer's supporting reason is wrong, however. ActorValid rejects nonzero hit_points for every non-combatant, non-door kind including scenery (src/network/lan_story_world_frame.c:43; the same rule at d437693, before E2), and it rejects alpha without the ALPHA flag (:52). The conclusion, a protocol bump, stands either way.
- overclaim[1]: 'Both A and B are frozen on Tal's screen' is asserted as fact. → Applied. That both are visibly frozen, and that A is the reported flock, are now INFERENCE. New supporting evidence was added: A's client clip time was within a second of its drop-time value when it rejoined about four hours later (CONFIRMED_LIVE, two catalog readings). B's own state is not observed.
- overclaim[2]: the ambient counter range was a selected stretch. → Confirmed and applied. Between B's arrival and the 300-window ambient cap, excluded is 750-2265 and owned is 20 or 22; across the session, excluded is 0-6424 and owned 0-44.
- overclaim[3]: attributing the owner's observation to the duplicate drop is INFERENCE. → Applied. Tick stamps show B registered within the first half minute of the issue's session and stayed resident for about four hours, so the drop covers almost the whole session. When the owner looked is still UNKNOWN.
- overclaim[4]: 0x220540 is not called directly by Update 0x222B50. → Confirmed from the review's Ghidra output. 0x222B50 calls 0x221D70, 0x222370, 0x221C90, 0x222510, 0x2202B0, 0x2226C0 and 0x2228D0, and 0x220540 is reached per bit inside 0x222370. Applied.
- overclaim[5]: 'Falling_Rock x5 dropped' is a reconstruction. → Applied. Five registry_added positions are CONFIRMED_LIVE, the ambiguous line proves only that at least two copies exist, and all five (and the total of about 11) being left out each frame is INFERENCE.
- missing[3]: no CONFIRMED_LIVE visual exists that relay-driven scenery animates; add a relayed control to G1. → Partly disputed. The plan does hold a CONFIRMED_LIVE visual for the same apply loop on doors (TallGate opened on Tal's screen, docs/coop-story-plan-54.md:4505-4509). For the scenery kind there is none, only counters that the roof flock 096ea0e9 follows the wire. The control was added to G1, and the scenery visual is kept as INFERENCE.
- missing[4]: concrete merge hazard with the uncommitted E2 diff in catalog() and ApplyPrepared, plus the frame version 11 to 12. → Dropped as moot. At 9081425 the E2 work is committed (git log; frame version 12 at src/network/lan_story_world_frame.h:10) and the worktree is clean. The dependency now only asks for file ownership to be agreed on the shared relay.
- missing[0], [1], [2], [5], [6], [7]: wire phase semantics; #82 in owner decision (a); hand-back test; the 8-dump catalog cap; the owner principle for entity-owned local clocks; the Stage 2 resolver's refusal during zone loading. → All verified and added:
- Phase semantics at :1067/:2727.
- The catalog-dump cap at :675, with the host's 8th dump early in the session.
- The ambient scope at src/hooks/lan_story_ambient.h:12-15, with no D1-D12 coverage.
- The resolver's refusal on a loading region at src/hooks/lan_story_collision_owner.c:243-247, image test :330-333.

Also added: the resolver's READY-witness requirement and its 'per-candidate/hot-query unsupported' contract (src/hooks/lan_story_collision_owner.h:49-50), with the client Prepare path lacking a witness, as a G4 item and an owner decision.

### breakables

- Overclaim: the #80 comment of 2026-10-16 09:18 is only shown truncated by tea, so the BrkD spawn-group claim stands on the host log, not on the cited comment. → Not upheld. With --comments, tea prints the full 09:18 comment. It names spawn group HC_SE_AILISHTUTBARRELS (enabled=1) and says five BrkD_AilishTutBarrel were added to the host registry only and the client never got them. The citation is kept, and the host log's start_sequence2/group_state lines are added as corroboration (CONFIRMED_LIVE).
- Missing point 0: the combat-component death path seam is 'combat slot 0x0D1AB0 (vtable 0x2C87A0)'. → Partly confirmed. The chain 0xD1AB0 → 0xD39A0 → 0xD31C0 and → 0xD3530 is CONFIRMED_STATIC: the call is at 0xD1AF8 when byte +0xA8 is set, and 0xD39A0 is the only caller found. 0xD1AB0 is stored at 0x2C87A0, but 0x2C87A0 has no static xref, and the word at 0x2C8798 looks like an RTTI locator, which suggests the table starts at 0x2C879C with 0xD1AB0 at slot +4 (INFERENCE). Which BreakableEntity component owns that table is UNKNOWN: the +0x7F0 component is built by a sub-constructor not traced in this fold. Recorded as UNKNOWN in the facts and in G1.
- Wrong 1: the 994 pot re-creations are not UNKNOWN; each remote-interior enter/exit cycle re-adds the tent's three objects ('registry_changed added=3 removed=3'). → Mostly applied. Counts confirmed in the private logs: 1030 client_enter of LNBr_TallTent02, 1029 client_exit, 994 pot re-adds, host 0 enters. The exact 'added=3 removed=3' line appears 770 times, not on every cycle, so 'most cycles' is what the log supports. The loop as the driver is kept at INFERENCE. The cause of the loop itself remains UNKNOWN (G9). That the loop belongs to H1b is INFERENCE from the module (lan_story_remote_interior.c:882, 897).
- Overclaim 3: the dirty scope is understated; git status shows 28 modified tracked and 12 untracked files. → Applied in substance, but the counts cannot be re-checked. At 9081425 the tree is clean, so the dirty set at the time of the live runs cannot be recovered. What can be shown: 42 files changed between d437693 and 9081425, including lan_story_world.c, lan_story_runtime.c, lan_story_spawn_trace.c, lan_story_spawn_events.h and story_spawned_actor.h, and a diff search finds no breakable code among them (CONFIRMED_STATIC). The tree of the tested binaries is recorded as UNKNOWN.
- Wrong 6: 'no loot on damage' is CONFIRMED_LIVE only for the five BrkD barrels, which may have an empty table. → Applied, with one addition. The two use-path smashes in the same 2026-10-16 run also submitted no drop. So that run cannot tell 'no roll on the damage path' from 'empty or failed roll'. 0xD31C0's own roll (0x130570, 0x1FAB0) is CONFIRMED_STATIC.

### xp65

- missing[6]: 0x4307F0 (XP-mode setter) and 0x41EF70 have zero references in the image, so retail has no diminishing XP and always pays 4 slots; G6 can be closed. The draft had the same 'always 4' as INFERENCE. → Wrong on both sides for 0x4307F0. The export table names RVA 0x307F0 SetOnlyLeadPCAwardedXP(bool); it writes bit 2 of the option byte RVA 0x408DD0+0xA18 (0x307F0-0x3080C). SOLWORLDM.gex calls it from 9 sites: SM_EnterTrials sets 1 at 0xB7396, SM_ExitTrials clears it at 0xB74EE, TR_RobotFight_Start sets 1 at 0xD04F0, plus the DB_Olivitess_* and health-check scripts. Each is paired with SetPCsCanDie (0x307B0, bit 0). Call and raw-pointer searches miss it because script natives bind by export name. Retail therefore pays only slot 0 during those fights; fact corrected. For 0x1EF70 (GetExperienceModifierForMonster, also exported), no code reference and no GEX call site exist in either module, so 'unused' is kept as INFERENCE. G6 now covers the deferred death path.
- wrong[0]: the effects were not 'same tick'; the 1 s gate is at src/hooks/lan_story_research54.c:379. → The timing correction is confirmed against the private live logs and applied. In both sessions the effects began between the last in_combat=1 sample and the first in_combat=0 sample, and the arbiter +0x58 changes fall in the same millisecond or about 300 ms earlier. Line refs adjusted: at 9081425 the 1000 ms gate is :363 and the cluster log line is :378-381.
- wrong[3]: the draft's line numbers belong to HEAD d437693 and a dirty tree. → At 9081425 (clean) the reviewer's numbers are the correct ones (menu_slot_allowed :2280-2290; queue_client_action declared at :2292, defined at :2306); applied. The resource mirror moved as well, to src/hooks/lan_story_runtime.c:775-792 (draft :769-786), and the whitelist macro spans :5-116.
- wrong[1]: the quest wrappers pay AddXP when XPaward != 0, and the castle-road 'Muster' completion with 0 pays nothing. → Call sites and counts verified: QL_CompleteQuest 48 sites, QL_NM_CompleteQuest 18; AddXP at 0x360D0/0x362CA; Mungo 50 at 0x69D3A; Lighthouse 5 at 0x723A0; castle-road 0 at 0x68276. Applied. The branch sense (pay only when non-zero) rests on the disassembler's reading of opcodes 0x0E/0x26 and is kept as INFERENCE.
- missing[2]: the script floaty toggles explain why no XP popups appear during the Mungo fight on either peer. → The toggle sites are verified: HC_SE_BossTSAStartup, called from the SE hilltop cluster entry, turns popups off at 0x3F782; TetsuTSA turns them on at 0x6A294, after its 50 XP grant at 0x69D3A. The popup entry 0xAFB90 returns early when +0x80 is 0. The no-popup outcome has never been observed, because Mungo is unreachable from the current saves (docs/coop-story-plan-54.md:6795), so it stays INFERENCE.
- missing[8] and missing[7]: AddXP yields between members (sleep 0); 0xD1AB0 is a per-update deferred-kill path. → Opcode 0x25 with a 0 argument at GEX 0x3AE61 is verified, but its sleep/yield meaning is the disassembler's naming, so the claim is INFERENCE. 0xD1AB0 is referenced from one data-table entry and takes a timing argument; 'per-update' is INFERENCE and the setter of combat+0xA8 is UNKNOWN. Both applied at those levels.
- wrong[2] and missing[0]: the shipped client recruit replay already calls AddExperience and GiveAbility; skills75 P2 calls 0xDA070 on the client while xp65 forbade it; the owner needs one rule. → The recruit replay calls are confirmed (src/hooks/lan_story_recruit_setup.c:207-217, contracts :13-17, header :22-28) and the carve-out is applied. The skills75 conflict is confirmed but not resolved here. The fold keeps a hard rule only for new client AddExperience/DoLevelUp calls, and leaves 'ability levels by raw copy or by 0xDA070' as design gate G12, to be settled once for both items. The authority question is owner decision O2, which is skills75 decision 1.
- Fold findings beyond the review: the draft marked the 0x28AEB0 'level_up' call as an INFERENCE cue or UI trigger, and said no mod code reaches the level-up path. → 0x28AEB0 is EmitterPlay (src/hooks/lan_story_character_audio.c:38; docs/coop-story-plan-54.md:6772), so 'level_up' is a sound cue (CONFIRMED_STATIC), and design step 6 routes it through the character-audio owner. The cleanroom engine also calls the native combat-end listener 0x24480 from the tools combat toggle (src/cleanroom/engine.c:3551-3600; src/hooks/lan_story_menu.c:156, :223). A tool-forced combat end therefore runs native DoLevelUpChecks; added as a fact. It awards no XP.

### bright59

- Missing point: with a Flash duration of 0 or less, 0x4DFFB0 stores rate 1000, so 'a type authored with duration 0 effectively never shows a flash'. → Partly wrong. The 1000.0 fallback (0x6E35F0) is confirmed, but it only sets the decay rate. The rise is a separate +12·dt phase in 0x4DFAB0, independent of the rate, up to the 1.0 clamp, so the roughly 83 ms rise still shows and only the fade becomes one tick. Kept as: the flash rises normally, then ends within one tick. CONFIRMED_STATIC.
- Overclaim: '0x4DFA40 is itself slot +0x40 of two other animation vtables (0x6C8434 and 0x6C84AC)'. → Partly confirmed. A byte scan shows 0x4DFA40 stored as data at 0x6C8434 and 0x6C84AC, and called directly from 0x4E1942. 0x4E1930 is stored at 0x6C8544 (0x6C8504 + 0x40) and called from 0x5884D2, and 0x5884C0 is referenced at 0x6D54A4. That 0x6C8434 and 0x6C84AC are +0x40 slots of sibling animation vtables was not verified, because vtable bases were not established. It is kept as INFERENCE.
- Overclaim: 0x553240 calls the slot through entity+0x39C, while the NPC-gesture comment says NPCEntity+0x3A4. → Not a contradiction. The decompile shows 0x553240 calling this+0x39C, vtable +0x40, and 0x528FF0(this+0xCA8). The gesture module checks gma == entity+0x3A4 (lan_story_npc_gesture.c:11, :275). The chain is consistent if 0x553240's this is a sub-object 8 bytes into the entity; that is INFERENCE, and the result now says so.
- Hitch correction: '19 during the pre-play servicing phase'. → The hitch numbers were reproduced exactly: eighteen gaps of 0.56-0.78 s and one of 6.3 s, then one of 0.8 s during play about 2.7 s after the hit, with four 799 ms monster_gap reports, and 89/131/35 across all 10-10/11 client logs. They were applied. Wording changed: the client had already reached input_ready earlier in that session, and the block shows spectator waiting/not_ready/presented states and VFX 'servicing'. It is therefore described as a client servicing/resync period before the fight, not as pre-play.
- Fact 7 overclaim cites the 500 ms inactivity exit at lan_story_world.c:2946. → The exit exists, but at HEAD 9081425 it is line 2940. Applied with the corrected line, and Fix B was rewritten as only exempting a still-lit flash from the 5 s cap.
- Missing point: 'the result's counts cannot be reproduced from any subset; future claims should name the exact files'. → The counts problem is agreed and corrected. The draft's 16/15/13 light-target counts were not reproduced: the five 10-11 roster sessions give 5/5/4, and all 10-10/11 client sessions give 47/55/16 plus 8 and 1 for two further targets. Exact private file names are kept out of this public section under the sanitization rule, so the sets are named by date and session instead (for example 'all client sessions of 2026-10-10/11', 'the last New Brightwater session').
- Missing owner decision: whether to accept per-run flatpak filesystem exposure or a tracked tools/ghidra script for cross-reference reports. → Not adopted as a #59 owner decision. The 'only caller' and export questions were answered without Ghidra filesystem exposure, by a read-only rel32 call/jmp and VA/RVA pattern scan over the hash-verified image (scratch only). Neither method excludes computed calls. A tracked xref report script would be an ordinary tooling change; if the owner wants a policy on flatpak exposure, that is theirs to set.
- Draft (not raised by the review): 'Only with an explicit +0x54 and flag 0x4 does it use the attack's resource and the impact position'. → Corrected while verifying 0x4D29D0. An explicit +0x54 selects the resource, and +0x67 bit 0x4 selects the impact position (+4..+0xC) instead of the entity matrix. They are independent conditions. Also, without +0x54 the table branch can choose 'HitMatrix FX', 'HitMatrix Fail FX' or no effect.
- Draft (not raised by the review): the issue quote, and 'unfinished work by another engineer' for the aimed-fire capture fix. → The draft's quotation was not verbatim, so the issue text is now paraphrased. At HEAD 9081425 the aimed-fire fix (story_aimed_flash.h, lan_arena_spirit_visual_host.c) is committed. It is recorded as CONFIRMED_TEST with the live view pending (plan:6770, 6795), not as dirty work. Draft references to d437693 and to a dirty tree were replaced with 9081425 on a clean tree.

### gates

- Wrong entry: 'leftover replicas would run the native heartbeat again after teardown', because gates_uninstall clears leases and restores the heartbeat hook. → Partly upheld.

Upheld: the draft's reason was wrong. The creator scope covers only setup that completes during creation. After that, an owned lease with gate_actor passing suppresses the heartbeat (lan_story_gates.h:131-140), and gates_uninstall clears the leases and restores the call hook (155-159).

Refuted: the consequence the review drew. The heartbeat call site 72E77 sits inside the gate setup callback 72E30. That callback sets state 3, calls the heartbeat once, then calls its own vtable slot 0 with argument 1 (0x472E7C-0x472E88; CONFIRMED_STATIC bytes, self-delete reading INFERENCE). A replica whose setup completed before teardown does not re-enter it.

Kept: completed replicas stay without a native heartbeat after teardown (INFERENCE). Only a replica whose setup was still pending at teardown would run the native heartbeat (INFERENCE).
- Missing point: the 3E250 slot-0x1C branch removes a queue entry without destroying the object, so a gate taking it would leave the host 'unknown' indefinitely and the fix would not help. → Not applicable to gates.

The general description of 3E250 was incomplete and has been corrected: slot 0x1C, and slot 8 with 1 when 0x1C is non-zero, with the erase in both branches.

The gate resource class that gate_family requires at entity+2C (vtable 2D5B44, lan_story_gates.h:81) has slot 0x1C = RVA 51310. That is a shared return-false stub, already a 3-byte identity at lan_story_recruit.c:31. Gate entries therefore always take the factory-destroy branch (CONFIRMED_STATIC, vtable read from the supported image).

Whether slot 8(1) destroys the object for other classes is UNKNOWN and does not matter here; the claim 'without destroying' is unverified. Design step 3 now checks slot 0x1C in gate_native_code, so the assumption fails closed.

### receipt

- Overclaim: the entry index passed to RVA 0xE42F0 is a register argument lost in the decompile, so 'current action entry' rests on live matches (INFERENCE). → Partly superseded. Exact-image disassembly (RVA 0xE28FE-0xE2903) shows EAX=component and ECX=the animation event record's type byte (component+0xF8 + 4*event + 2). So the entry is chosen by the event's own byte (CONFIRMED_STATIC), and the draft's 'current action entry' was imprecise and has been replaced. That this byte equals the death action for the death clip's emission event remains INFERENCE, supported by 337/337 live matches against Prepare's style+0x16 entry.
- Overclaim: the departure code has live log evidence, so 'CONFIRMED_TEST, live check pending' alone is wrong. → The records conflict. At 9081425, docs/coop-story-plan-54.md:6762 and the §10 E row (:6795) say CONFIRMED_TEST with the live check pending. The 9081425 commit message lists the departure fix among its CONFIRMED_LIVE items. The private live logs show 45 departure lines in two runs, with no departed_returned or refused lines. Kept: the logged departure sequence is CONFIRMED_LIVE (log), the plan's state is quoted as recorded, and no owner acceptance is recorded in the plan. Reconciling the plan with the commit message is outside this item.
- Missing: lan_story_world.c is dirty (M) and belongs in the file-ownership list. → Stale at HEAD 9081425: the worktree is clean because the E2 work was committed. lan_story_world.c stays in the coordination list as a shared module that carries the consumer chain (1471-1474, 2805-2811, 2970-2977), not as a dirty file. The draft's 'dirty d437693', 'story_death_visual.h is dirty' and 'spirit visual host being edited' were corrected the same way.
- Missing: name build ids 5cfbd199 (10-13 212033Z run) and ac1f23ae so that 'runs built with it' can be checked. → Applied at a lower evidence level. The logs carry no build identity (only the game SHA-256 and the module base). Which build each run used rests on docs/coop-story-plan-54.md:4440, :4450, :4469 and on log dates against ff94f5a (2026-10-14), where both logs first enter tracked source: INFERENCE.
- Overclaim: an attached effect from RVA 0xBCE90 probably passes fx_exact, because both creation paths write the same offsets. → Not settled. The attached factory (RVA 0x18C90 via 0x18140) and the independent one (RVA 0x18A80 via 0x18300) share helpers 0x17EB0 and 0x18830, and both get the same post-creation writes in RVA 0xE2810. The class each one writes was not traced. Kept as UNKNOWN, with INFERENCE that it probably passes. The proposed first-gate line would show which.
- Missing (G2 owner decision): the 12 unexplained 5b14180b misses are 'near the farmhouse, a location on the #54 route per status-matrix rows 88-89'. → Corrected. Rows 88-89 describe the farmhouse spawn work, not route membership. The 12 split into 7 in HC_NW_Farm_Random and 5 in HC_SE_Gazebo_Random. That the route visits the farmhouse is cited from docs/coop-story-plan-54.md:6794 (the D6 farmhouse sword chest). Whether the route passes the gazebo is UNKNOWN. The owner decision was added as stated.
- Overclaim: the cited clip script does not show '51/51 after death', and the method is weak (the host's first dead clip is sometimes a hit clip). → Re-verified with a stricter join: a client sample after the host's first dead sample must carry a clip that the host showed only while dead. Result: 51/51, with 8 unsampled. The host's per-actor sample interval has a median of 264 ms (about 4 Hz, not 1 Hz). Stated as clip-id equality at discrete samples, not visual equality.
- Not in the review: whether the emitter has other silent NULL paths besides the action entry and the 0xC0 queue. → Added as a fact during the fold. At RVA 0xE2980-0xE29B2, creation needs actor+0x44 → +0xB4 → +8 → renderer +0x14 to exist, and its slot-6 call (vtable 0x2DF8EC, RVA 0x21BC80, a field read through renderer+8 → +0x1C → +0x14) to return nonzero. Otherwise *out is NULL. CONFIRMED_STATIC; what the field means is UNKNOWN. It is one more reason why a missing refused line does not prove a death had no effect.

### creep88

- Missing point (parent handling): the publication 0x110BD0 'converts a +0x94 == 4 body through the parent inverse'. → Partly confirmed:
- The solver treats +0x94 of 0 or 4 as the own-world-matrix path (0x32507-0x32522).
- The publication takes the unparented call only for 0 (0x110BD3).
- For 4, though, it computes parent = +0x94 - 4 = 0 and reads +0x8C from it (0x110BE1-0x110BF3). That reads through a null parent; it is not a valid parent-inverse conversion.
Recorded as CONFIRMED_STATIC for the code path. Whether 4 occurs at publication is UNKNOWN. The design's +0x94 == 0 rule stands.
- Overclaim: the idle-contact line proves '>= 1 admitted correction in 46 of 60 host sessions'. → The wording correction is applied (once per process, lan_story_control.c:190-193). Private live logs, conclusions only: at read time the line was in 47 of 60 host session logs, with the install line in 57. The logs grow while the owner plays, so the count is given as 'at read time'.
- Overclaim: SetDisableMove's callers, '0x24160 twice, a party-switch path'. → The call-site count is confirmed: 0xB007 in 0xAFD0, and 0x241E2 and 0x2420D in 0x24160, plus the export. The 'party-switch' role of 0x24160 was not confirmed, so the roles are recorded as UNKNOWN. 0xAFD0 also toggles arbiter +0x60 bit 4 (CONFIRMED_STATIC).
- Overclaim: 0x187600 is slot 0 of the vtable at 0x2D91F4, 'whose single instance (RVA 0x34D780) is referenced from 0x186E10'. → Confirmed: 0x187600 is referenced from 0x187938 and from table data at 0x2D91F4, and that table is referenced from data at 0x34D780. Not checked: the reference from 0x186E10. The lowering is applied on the strength of docs/engine-functions.md:583, which lists 0x187600 and 0x187710 as missile movement controllers. The root-motion path is UNKNOWN, and the 0xC3500 accumulator is CONFIRMED_STATIC.
- Missing direction check: the recorded normal is 'at the NB test-start spot (Tal -47.40,-4.81,352.20)'. → The numbers are confirmed: the normal (0.0407, 0.99872, 0.030) at docs/research-log.md:7718-7720, and the creep of -0.00009 Z / +0.00001 X at docs/coop-story-plan-54.md:4410-4411. That the 2026-10-12 normal came from the same triangle Tal stood on during the 2026-10-13 capture is INFERENCE: plan lines 4392-4395 call it 'the reported New Brightwater slope', and no triangle id was recorded. The sign disagreement is kept as an open point for G2, not as proof of a different writer.
