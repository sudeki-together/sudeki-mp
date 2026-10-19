# Task brief: client step-away (saved-story co-op, #54), stage 1

Branch `codex/shared-simulation`. Read `AGENTS.md` first and follow it: exact-image
guards, one adapter per native seam, socket/transport workers never touch native
objects, evidence vocabulary from `docs/evidence-index.md`, and never write
"works" or "fixed" for something that only compiled or passed a pure test.
Context: `docs/coop-story-plan-54.md` §9 entry "2026-10-18 — #87 Buki as
client: weapon ids fixed; combat menu rules; client step-away decided", and
`docs/coop-story-research-54.md` sections F1, F2, G and early.

## What the owner decided (2026-10-18)

In vanilla, pressing M during combat does not open the Main Menu; it pauses
the game (native branch in the controller update RVA 0x27CF0: when
CGroupPlayers::InCombat, group+0xD4, RVA 0x4FA0, is set, M calls 0x29410
instead of the menu toggle 0xA0E0). Multiplayer cannot pause, so:

1. A **client** who presses M while the **host's world is in combat** steps
   away. The game's own party AI takes that character, the world keeps
   running, and when combat ends the AI keeps following the host, like any
   AI follower. Pressing M again, in or out of combat, returns control.
   Out of combat M opens the Main Menu exactly as today.
2. While stepped away the player sees a banner
   `<Character>: AI-controlled - press M to return` (ASCII only; the overlay
   font has no em dash). It stays visible during host cutscenes, on the top bar.
3. The Esc (SudekiMP party) menu gets an **"Away - watch the party"** button
   for clients, usable in or out of combat (this takes precedence over any
   combat condition: the host admits a step-away at any time), which does the
   same step-away and reads **"Return to Play"** while away. Opening the Esc menu never pauses
   the world.
4. The **host's** own M in combat stops pausing the world and shows the
   notice `Can't step away as host yet`. Host step-away comes with phase G
   (the host's character is the native lead, and the native AI never drives
   the lead). Do not build host step-away here.
5. Combat rules already in the tree (keep them): weapon changes and item use
   are allowed in and out of combat (Q menu, real time under SudekiMP), armour
   only out of combat (`SudekiMpStoryActionAllowedInCombat`,
   `src/network/lan_story_handoff.h`).
6. The #75 rule stands: only the player edits their own character; the
   host's Main Menu skips characters another player controls, including a
   stepped-away player's character.
7. Saved-story protocol `STORY_GAMEPLAY_PROFILE` goes from 26 to 27 for this
   (mixed builds refuse at hello). Approved.
8. Defaults (owner-approved recommendations):
   - Host combat state unknown or stale on the client: M does nothing and
     shows `Waiting for the host...`.
   - While stepped away: the Q menu is unavailable; F1/F2 are reserved for
     spectating (stage 2), so the F1 character swap does not run.
   - Disconnect while stepped away: the player rejoins in control.
   - Returning to a downed body is allowed (until F2 exists).
   - A return that does not reach control within 5 s drops back to AI with
     `Couldn't take control - press M to try again`.
   - An AI-driven stepped-away body never starts a remote house entry by
     itself (guard the H1b remote-interior trigger for away members).
   - Other players see `(AI)` after the stepped-away player's name tag.
   - Ini switch `[SudekiMP] StoryStepAway`, default on.

## Stage 1 deliverables

### 1.1 Pure policy module (no runtime wiring)
New `src/network/lan_story_step_away.{h,c}` plus `tests/story_step_away_test.c`
and CMake targets. Plain data in and out; callers pass `now`.

- **Host-combat judge.** YES only from a fresh story frame with
  `combat_mode==1` (the host's group+0xD4, `lan_story_snapshot.c`,
  `lan_story_frame.h`); NO only from a fresh frame with `combat_mode==0`;
  UNKNOWN otherwise. Fresh = valid, age -16..250 ms (the window
  `present()` already uses), scene READY with matching epoch and revision,
  `combat_mode<=1`. Do **not** use the effects `in_combat` field (cluster
  manager +0x34) as a YES source: it was seen stuck at 1 for about 10 minutes
  after a fight and at 0 while combat was forced. Log it only.
- **M decision**, in this order: press within 300 ms of the Main Menu, a shop
  or an examine box last being visible → ignore; phase REQUESTING,
  HANDING_OVER or AWAY → RETURN (unless the Esc party menu is open);
  RETURNING → ignore; Main Menu visible → today's close; not owned or not
  admitted → nothing; host combat YES with Q, a shop, an examine box or a
  host cinematic open → ignore; YES → STEP_AWAY; NO → open the Main Menu as
  today; UNKNOWN → refuse with the waiting notice.
- **Client phase machine:** NONE, REQUESTING, HANDING_OVER, AWAY, RETURNING,
  with the 5 s return timeout back to AWAY and the banner text for each phase
  (`Stepping away - waiting for the host...`, `<C>: handing over to AI...`,
  `<C>: AI-controlled - press M to return`, `Returning control...`).
- **Host admission:** accept a step-away **in or out of combat**: the combat
  judgement is client-side only and decides whether M steps away or opens the
  Main Menu, while the Esc "Away" button steps away at any time (owner's
  rule; PRESENCE carries only the away bit, so the host cannot and need not
  tell M from Esc). Accept when the member is in the lead's area, no swap
  involves that seat, and the seat is in a stable state (not PREPARE,
  ACQUIRING or WAIT_ACK). A return is always accepted. A request that does
  not change the away bit must be a no-op (no revision bump: every revision
  bump fences every peer's inputs).
- **Refusal codes:** "not in the lead's area" gets its own code and notice
  (`Too far from the party to step away`), distinct from swap BUSY and
  STALE, so the client never loops on a refusal.
- Tests: full truth tables for the judge (fresh, stale, -17 ms, 251 ms,
  epoch/revision mismatch, scene not READY, combat_mode 2), every M decision
  row, every phase transition including the timeout, and host admission.

### 1.2 Wire: PRESENCE command with an AWAY bit
Reuse the existing `SudekiMpLanPartyCommand` PRESENCE kind
(`src/network/lan_party_session.h/.c`). Name the value bits
`SUDEKIMP_LAN_PARTY_PRESENCE_MENU=0x01`, `..._AWAY=0x02`. Add
`SudekiMpLanPartyStoryCommandAdmissible()`: SWAP, or PRESENCE from a client
seat (player 1..3, target 4) **without** the MENU bit. Use it at both the
receive allowlist and `QueueCommand`. Bump `STORY_GAMEPLAY_PROFILE` 26 → 27.
Change nothing else (command and presence sizes, envelope version, lobby
version, every handoff/frame/effects codec stay). The host confirms through
the existing MSG_PRESENCE (`ownership.away`, `control[p]`, `ack_request`,
`ack_result`). The F1 swap shares `ack_request/ack_result[seat]`: keep the
swap's pending id and the step-away's pending id separate so one notice never
prints the other's result. Tests: `tests/lan_party_session_test.c` profile
26 rejected / 27 accepted, admissible cases, and a transport test for
request, duplicate, loss and ack.

### 1.3 Host: step-away, drain, offer refusal, return
In the host controller dispatch (`src/hooks/lan_story_runtime.c`,
`service_story_ownership` / `service_story_controls`, game thread):

- No host-side combat check: see 1.1 host admission.
- On an admitted PRESENCE/AWAY, set `story_presence.ownership.away` for that
  seat (the pure ownership policy in `src/network/party_ownership.c` already
  models `away`, and Disconnect clears it), and drain the seat's control
  lease with reason `stepped_away` through the existing single owner
  (`SudekiMpLanStoryControlDrain`). Do not add any AI override write or hook.
- While `away` is set, the offer path refuses the seat with
  `offer_stepped_away`, so the native party AI drives the body. Put that
  check **first** in the offer-trace ternary: trace reasons are deduplicated
  per player and share one budget, so it must log exactly once.
- Return: clear `away`; the unchanged offer path (Begin, NextLease,
  Acquire, PREPARE, ACK, READY) hands control back.
- **Fix the epoch-locked handoff** (required): once story control is bound,
  `SudekiMpLanPartyControlStoryBegin` only succeeds when the roster epoch
  equals the bound epoch, and the binding is cleared only at uninstall, so
  after any host zone/world load every new offer fails with
  `offer_begin_refused` (CONFIRMED_STATIC; seen live after a load drain). This
  already breaks reconnects and would break every return after a door,
  house or area load. Rebind safely on a positively observed new epoch with
  no lease held, and cover it with a test.
- The background-load and split-transition hold branches skip the drain
  chain; a step-away accepted during them must drain once the hold ends, and
  the client must not show HANDING_OVER while it still drives the body.
- The death/lead hold counts any observing peer with a character as a driver.
  Decide (and document) that a stepped-away player does not keep the
  dead-lead hold armed, so a downed host lead with AI-only allies does not
  stall the fight; if that needs an owner call, stop and ask.
- Guard the H1b remote-interior entry (`src/hooks/lan_story_remote_interior.c`)
  so an away member's AI body never starts a remote house entry.
- Tests: `tests/party_ownership_test.c` cases (away during ACQUIRING, while
  HUMAN, AcquireBegin refused while away, Disconnect clears away), the epoch
  rebind, and the no-op request.

### 1.4 Client: M routing, intent loop, notices
In `service_quick_menu` (`lan_story_runtime.c`), between
`SudekiMpLanStoryInputTakeMainMenu` and `SudekiMpLanStoryMainMenuRequestOpen`,
call the M decision. On the client the native M path is already closed (the
input adapter never forwards listener events), so no new hook is needed. The
return press arrives while the client is not offered, so it must use the
focus-only raw edge (`menu_pending_raw`), which must survive `InputClear`.
Every non-OPEN decision drains the raw edge. A revision-gap discard must not
eat the second press during REQUESTING. Intent loop on the controller tick:
queue PRESENCE AWAY/0 when the intent differs from the presence bit;
STALE requeues fresh, BUSY retries after 250 ms, refusals reset the intent and
show a 4 s notice. While the phase is not NONE: Q unowned, F1 swap not run.
Record the presented frame's `combat_mode` with its receipt in `present()`.

### 1.5 Own-body camera while away
The native own-body camera keeps running while not offered. Admit look axes
for the own controller and actor while away so the player can orbit, gated
like today's orbit (not while the Esc party menu or Q is open). Single owner
of the controller listener stays `lan_story_input.c`.

### 1.6 Banner
New `src/ui/story_away_banner.{h,c}` on the existing MenuNative frame
callback (no new hook), modelled on `src/ui/story_cinematic_view.c`. Read the
screen size from the native globals the name tags already use
(`lan_story_name_tags.c`), not `GetRenderTarget` (that would add a reference
every frame unless released). ASCII text, centred, top of screen, no overlap
with subtitles, the roster strip or the confirm box. The `(AI)` name-tag
suffix for other players goes through `lan_story_name_tags.c`. Pure layout
test `tests/story_away_banner_test.c`.

### 1.7 Esc menu and host M
- Saved-story Esc menu (`src/ui/party_menu.c`, saved-story branch): add
  "Away - watch the party" / "Return to Play" for clients, driving the same
  intent as M. The four-seat party runtime already has this flow
  (`lan_party_runtime.c` away loop, `lan_party_host_control.c` want_control,
  `ui/party_menu.c`); reuse it rather than duplicating it.
- Host: in combat, stop the M press from reaching the native pause 0x29410 on
  the host in saved-story co-op, and show the notice. Find the exact seam;
  one adapter; exact-image guard.

## Out of scope for this task
Stage 2 (spectate cycle with F1/F2 for stepped-away and downed players,
shared with F2), stage 3 (host "Pause Party" / client "Request Pause"), host
step-away (G), F1 client cluster following, and AI skill/Spirit use by the
away body.

## Acceptance
1. Build all targets; the Wine suite passes except the known non-pass set
   (probes, Launcher*, argument-taking image tests run separately).
2. New pure and transport tests pass; one mutation per new guard shows the
   test catches it.
3. Live gates (owner, castle-road ambush, Tal host / client character):
   step away in a fight (client log decision STEP_AWAY, host drain reason
   stepped_away, one `offer_stepped_away` line, the AI fights); combat ends
   while away (AI follows, banner stays); M to return in and out of combat
   (new handoff, measure press-to-control time); M out of combat still opens
   the Main Menu, and closing it never steps away; stale host state shows the
   waiting notice; disconnect and reconnect while away; host cutscene while
   away; host door, house and world load while away, then return; host M in
   combat no longer freezes the clients; Esc Away button both ways.
4. Add a §9 entry to `docs/coop-story-plan-54.md` with the evidence level
   reached for each item. Run `graphify update .` after code changes.
   Commit only when the owner says, as `heywander` with the noreply address.
