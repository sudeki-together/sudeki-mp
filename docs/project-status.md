# SudekiMP project-status checkpoint

Updated: 2026-08-20 (America/New_York)

This is the short planning checkpoint for future agents. Read it together with
`RECOVER.txt`, `docs/milestones.md`, and `docs/research-log.md`. Percentages are
human planning estimates, not automated coverage measurements. **Touched**
means useful research or a prototype exists; it does not mean the feature is
safe enough for ordinary play.

## Overall estimate

- About **70% of the roadmap has been touched**.
- About **45% has reached a concrete proof or milestone**.
- About **30–35% is production-ready toward stable local co-op**.
- About **20–25% of the complete 2–4-player plus online vision is done**.

## Foundational roadmap

| Area | Estimate | Current classification |
| --- | ---: | --- |
| GOG installer inspection | 100% | Complete |
| Vanilla baseline and hashes | 100% | Complete |
| Static reverse engineering | 75% | Extensive and inherently ongoing |
| Quick Menu slow-time removal | 100% | Confirmed |
| DLL/launcher/hook framework | 100% | Working and exact-build gated |
| Plasmatica end-to-end reverse engineering | 90% | Core flow confirmed |
| Direct real-time combat controls | 75% | Skills, consumables, and Spirit Strike proven |
| Animation and caster-protection research | 65% | Native behavior understood; presentation remains |
| Human/AI character-control switching | 90% | Mechanism and reversible AI override proven |
| Local two-player control | 80% | Independent movement, attacks, and controller input proven |
| Cameras and separation | 70% | Split-screen and dual cameras work; containment remains |
| Combined multiplayer combat | 45% | Major systems coexist; no stable full encounter yet |
| Three/four players | 5% | Considered, not implemented |
| Doors, cutscenes, and world progression | 25% | Behaviors observed; no general solution yet |
| Native online networking | 0% | Deliberately deferred |

## What has actually been proven

- Sudeki accepts a second independently controlled party character in one
  process.
- Native AI can be disabled and restored without removing that character's
  movement, targeting, or attack rails.
- Keyboard/mouse Player 1 and a Linux-controller Player 2 can submit input
  simultaneously.
- Two independently assigned camera views can be composited side by side.
- Skills can execute while shared simulation remains at normal speed.
- Skill-camera effects can be routed and isolated at several confirmed camera
  phases, although general containment is unfinished.
- SudekiMP can create native-looking title/menu pages and reuse resident,
  game-owned character portrait textures without extracting game assets.
- The Wine story-intro crash is resolved by an exact-build-gated accelerator
  resource cache; the full orange intro played successfully.

These are engineering proofs. They do not yet make a cohesive build that a
player can start and use normally for hours.

## Current integration frontier

### Deferred live-validation memory

Do not mark the roster handoff complete until this exact manual check is run:

1. Use the Tal-only save with the sidecar set to `Mode=Coop` and a distinct
   selected Ailish/Tal pair.
2. Confirm the log reports the selected Ailish role as `waiting` while she is
   absent and Tal continues under native single-player control.
3. Enter the point where Ailish joins the party.
4. Confirm availability changes to both actors present, split-screen activates,
   the selected role tuple is applied, and the new party pointers are locked.
5. Cross-check Tal's movement, HUD, camera, AI state, and role-lock cleanup
   after a level/party transition.

This is intentionally preserved as an open live-validation item for a later
session. The code/build/Graphify work is complete for this checkpoint, but the
runtime result must not be inferred from static evidence.

### Planned story-mode recruitment harness

The next validation helper should operate on a real Tal-only story save rather
than spawning Ailish in the cleanroom. In a research-only mode, it may clear
hostile entities, use a validated native door/teleport boundary to shorten the
route, submit the normal interaction input, and let the Ailish recruitment
dialogue/cutscene complete normally. The trace must snapshot the active party,
party pointers, resource types, controller target, AI mode, HUD bindings,
cameras, and roster availability before and after the recruitment event.

This harness can accelerate the route to the real party-arrival event, but it
must not claim equivalence to cleanroom spawning or skip the native recruitment
script. It remains disabled by default until the exact story door/interaction
and cutscene-completion boundaries are identified.

The current primary track is a pre-game co-op roster/role-lock flow, replacing
unsafe arbitrary mid-game swapping:

1. New Game opens a Single Player/Co-op choice.
2. Connected players select Ailish, Tal, Buki, or Elco.
3. Each player locks one distinct character for the game/save.
4. A character becomes controllable when the story makes that character
   available.
5. The locked tuple must configure gameplay ownership, input routing, native
   AI state, HUD ownership, and camera ownership atomically.
6. Save persistence needs end-to-end validation.

The native-style page, fade/font rendering, runtime-generated capsules, and
four resident character heads have been demonstrated. The remaining work is
to finish player-ready/lock presentation and connect the saved selection to
runtime ownership without relying on manual F8/F10 setup.

## Order after roster integration

1. Confirm Tal's HUD, movement, camera, and AI ownership survive initialization
   and every supported lifecycle transition.
2. Finish Tal's visible running animation while he moves during Pure Land.
3. Complete one uninterrupted two-player combat encounter.
4. Finish skill-camera startup, cinematic, and restoration containment.
5. Resolve ranged-character observer presentation, including locomotion,
   firing, aiming, weapon attachment, and the vanilla-compatible held pose
   during first-person-only reloads.
6. Build direct combat loadouts before attempting full Quick Menu state/UI
   virtualization.
7. Add an optional shared-focus camera as an alternative to split-screen.
8. Handle doors, party transitions, cutscenes, quests, and scripted sequences.
9. Expand to three/four players only after two-player play is stable.
10. Begin host-authoritative networking only after local multiplayer is stable.

## Parallel project tracks

- **Sudeki Together front end:** native-looking Single Player/Co-op choice,
  player-ready states, character portraits, immutable session roles, and save
  persistence.
- **SudekiForge:** future read/write tooling for models, skeletons, animations,
  levels, and user-owned archive resources. It should not block the current
  runtime co-op proof unless authored third-person animation becomes necessary.

When reporting progress, keep the four percentages above separate. A feature
may be heavily researched and still be far from production-ready.

## Autonomous research-launch preference

For future unattended research, skip all startup movies by default so the
budget goes to the requested lifecycle trace. The existing
`SkipStartupMovies=true` option only skips `Publisher.bik`, `ClimaxLogo.bik`,
and `TWIMTBP.bik`; it deliberately leaves the orange story movie for explicit
intro-compatibility tests. A dedicated all-movies research override should be
used or added before the next autonomous launch.

## 2026-08-20 roster lifecycle observation checkpoint

The first automated observation pass used an isolated headless Gamescope
window and sent one external `Escape` only to skip the opening story movie.
That path reliably reached the normal title controller without changing the
game installation. The same launch workflow should be reused for future
unattended research.

Confirmed sequence:

1. The title controller reaches its normal state after the movie skip.
2. Activating New Game transitions to an independent roster subpage with a
   native-compatible UI-layer contract (`state=6`, then roster takeover).
3. The roster page owns a separate controller and renderer state; it is not
   merely a second copy of the title button list.
4. Four character portraits resolve through resident Load Game widget
   resources. The live trace identified Ailish, Tal, Buki, and Elco material,
   resident texture, GPU texture, and icon objects. No copyrighted files are
   extracted or copied.
5. The observation run did not apply gameplay ownership, AI, controller, HUD,
   or camera changes. It stopped before committing a roster choice or loading
   a game, so the New Game -> party-finalized boundary remains open.

Useful exact observations from the supported executable
(`8ceb1d3c...bb94`):

- Front-end controller vtable RVA: `0x002cb2b4`.
- Roster action hook entry RVA: `0x000a0360`.
- Save-page action trace RVA: `0x000898a0`.
- Save-page input trace RVA: `0x0008d970`.
- Save-entry update trace RVA: `0x0008c710`.
- Portrait selector trace RVA: `0x0015c070`.
- Runtime roster rows are created from resource IDs `0x63`-`0x67`.
- Portrait resource indices observed: Ailish `0x116`, Tal `0x115`, Buki
  `0x117`, Elco `0x118`.

Confidence is high for the page/controller and resident-portrait boundaries,
and low for the later gameplay ownership event because it was intentionally
not reached in this pass. Next observation should select Co-op, capture the
character-lock actions, then follow the first level-load/party initialization
without enabling split-screen or AI overrides.

## 2026-08-20 co-op lock and New Game transition observation

The follow-up isolated run successfully navigated the roster page and selected
Co-op.  The observed sequence was:

1. Roster navigation advanced through the four character pages.
2. Ailish/Tal was accepted as a distinct pair.
3. `split_screen_render event=co_op_roster` recorded types `0x01` and `0x23`.
4. The sidecar roster profile was saved.
5. Runtime-created roster rows and resident portrait objects were released.
6. The native roster page was restored to its prior state.
7. Native `StartNewGame` was replayed.

After the replay, the isolated frame remained black and no party, level-load,
HUD, camera, or controller-initialization trace appeared.  This is now a
specific transition boundary to investigate: the roster lock itself succeeds,
but the first post-lock New Game presentation does not reach observable
gameplay initialization in the current research launch.  No ownership, AI,
split-screen, HUD, or camera writes were made by this pass.
