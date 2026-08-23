# Cleanroom

Sudeki's supported GOG build ships an internal `testroom` level and a
`MON_TrainingDummy.sol` monster resource. SudekiMP enters that room through the game's own
startup arguments; it does not copy or rebuild `SOLData.baf`.

Launch it with:

```bash
./tools/continue-research.sh --cleanroom
```

`--test-arena` remains an alias for older recovery notes. The launcher supplies
`-Level testroom -DT 1 -Ailish 1`, so Ailish is the only initial party member
and remains the protected lead.

## Menu

- F8: open or close the cleanroom menu
- Up/Down: select an entry
- Enter: spawn/despawn the selected entry
- Escape: close the menu

The entries are Tal, Buki, Elco, Ailish, Training Dummy, Combat Mode, Camera
Mode, Split Screen P2, Infinite SP, Infinite Spirit, Infinite Jetpack, and
Close. Ailish is shown as `LEAD LOCKED` and cannot be removed.
Playable characters use Sudeki's native `InternalSpawnPC`/`RemovePC` path so
its party and formation systems keep ownership. Character names are
constructed through Sudeki's own 12-byte, reference-backed `ResourceName`
implementation; the mod does not fabricate or persist these engine-owned
objects. The dummy uses the native `MON_TrainingDummy.sol` resource and appears
at the initial Ailish cleanroom anchor, which is the room's test center.

Combat Mode invokes the native `CGroupPlayers` combat-state transition rather
than writing its state byte or merely arming character arbiters. Camera Mode
invokes Sudeki's native first-/third-person camera transition. Both routes are
exact-build checked and report their engine-owned state in the menu. Camera
Mode is principally useful with the ranged characters Ailish and Elco.

Infinite SP and Infinite Spirit default to `ENABLED` in this cleanroom mode.
They use Sudeki's shipped `NoSpNeeded()` and `NoSspNeeded()` developer paths,
which bypass the matching affordability check and deduction. Infinite Spirit
also uses the native `GetSsp()`/`SetSsp()` route to keep the party-shared meter
at its confirmed maximum of `200`, so its HUD display remains full. Disabling a
menu entry clears only its corresponding developer flag. Unloading the mod
restores both flags to the values captured before cleanroom installation.

Infinite Jetpack also defaults to `ENABLED`. Elco's live actor owns a
`CElcoAbility*` at `actor+0x104`; its native maximum and current fuel are the
floats at `ability+0x68` and `ability+0x6C`. The maintenance pass calls the
exact exported `CElcoAbility::SetFuel(float)` only when current fuel is below
the existing native maximum. It does not enlarge the tank, patch the fuel
drain rate, or retain a game-owned pointer across actor replacement. Disabling
the option simply stops refilling.

## Training loadout initialization

Once Sudeki's inventory and item database are live, cleanroom mode invokes the
game's shipped `FillInventory()` developer function once. This populates the
native inventory tables with all authored items, including every weapon for
Tal, Ailish, Elco, and Buki. It intentionally also supplies consumables and
other inventory items; it does not create parallel mod-owned item records.

All eight main-party Spirit Strikes are enabled through the native
`SpiritStrikeEnable(-1)` path. The original unlock mask is captured and
restored when the cleanroom hook unloads. The party-shared Spirit meter remains
the same native meter described above.

Dynamic cleanroom actors do not always receive the ordinary campaign's
equipment/stat initialization. On first seeing each actor instance, the mod:

- reads current and maximum HP/SP through `GetCharacterNumberStat()`;
- repairs a maximum only when it is non-finite or non-positive while its
  corresponding current value is positive;
- leaves valid authored statistics unchanged; and
- calls `CCharacterWeapon::SetWeapon()` only when the native weapon component
  has no current inventory item.

The corresponding global `SOLData` item IDs are Tal `0`, Ailish `12` (Royal
Sceptre), Elco `24` (Mk1 Pistol), and Buki `36` (Zesiro). Live testing proved
that `CCharacterWeapon::SetWeapon(int)` does **not** accept those global IDs;
it accepts an index in inventory category `5`. `FillInventory()` orders that
category as Ailish, Elco, Tal, then Buki, so the starter slots passed to the
native function are Tal `24`, Ailish `0`, Elco `12`, and Buki `36`. Supplying
the global IDs had visibly equipped Ailish's weapon on Tal and Tal's weapon on
Elco. Static archive inspection found character-specific weapon families
(`WeaponTal`, `WeaponAlice`, `WeaponElco`, and `WeaponBuki`) but no weapon-level
requirement. The `Level Requirement` records found in `SOLData.baf` belong to
ability advancement data.

Actions remain visibly pending until Sudeki's entity lookup confirms the new
state. A timed-out operation reports `FAILED` rather than issuing another
blind request. Confirmed despawns play the original procedural cue documented
in `assets/audio/cleanroom/`; no Sudeki audio is copied into the repository.

If a playable actor is spawned after Combat Mode is already enabled, its
native stats/weapon initialization is followed by one exact-checked replay of
Sudeki's existing `CGroupPlayers` combat transition (`RVA 0x00024480`). This
arms only the newly observed actor through the engine's normal party path; it
does not toggle the group out of combat, write the combat-state byte, or
replace weapon/animation logic. The focused log records this as
`combat_mode phase=refresh` with the actor name. This path exists specifically
for cleanroom actors added after combat begins and remains pending live visual
acceptance for Elco's ranged pose and weapon attachment.

The menu is exact-build and cleanroom-command-line gated. It refuses to install
alongside the player-input trace. In cleanroom mode, the control-separation and
split-screen modules instead own the shared controller-update and frame-end
hooks; the menu registers update/render observers with them. This prevents two
modules from patching the same call or vtable slot.

## Split-screen Player 2 toggle

`Split Screen P2` defaults to `DISABLED`. Spawn at least one additional party
member and toggle it from F8. Player 1 remains Ailish/the native front
character; Player 2 becomes the first non-front active party member. Enabling
the entry requests Sudeki's native AI override, dual cached cameras, per-player
HUD ownership, controller-relative movement, the 10-unit separation guard,
weak attack, and independent Camera 2 controls. Disabling it restores the exact
Player 2 character's native AI and releases Camera 2 before returning to one
view.

The current input source is the Linux joystick bridge used for the Razer pad.
A badge is drawn near the top of the right viewport while multiplayer is
requested:

- `P2 JOINING`: no second character has completed the native AI override yet;
- `P2 RAZER`: the character and camera are active, but no current bridge packet
  has arrived; and
- `P2 READY`: the Player 2 character is active and the Razer bridge is sending
  valid packets within the configured timeout.

The launcher starts the bridge automatically when the configured joystick node
(default `/dev/input/js0`) is readable. If the pad is missing, the cleanroom
still launches and reports the waiting state. The badge therefore represents
live input readiness, not merely the requested split-screen state.

## Current validation state

Live tests have confirmed the native room, Tal/Ailish startup creation, the
Ailish-first launch, F8 overlay, all three dynamic party-member spawns and
despawns, repeat spawns, native formation behavior, and the training-dummy
resource. The first dynamic spawn exposed and precisely identified an
incorrect 32-byte inline-string assumption: `PC_Tal` put its final letters in a
field Sudeki dereferenced as a pointer. That unsafe layout has been removed.
The current build uses the native 12-byte constructor and reference cleanup,
and exact-image checks cover both internal entry points.

The centered dummy placement, native Combat Mode/Camera Mode controls, and
cleanroom-only infinite-resource controls are prepared and pass the PE32 build
plus exact supported-image signature regression. Their focused live comparison
is pending. Full native inventory initialization, all Spirit Strike unlocks,
invalid maximum-stat repair, and missing starter-weapon setup pass the same
build and exact-image checks; live HUD/projectile/menu verification is pending.

Split-screen activation, controller movement, and the independent right-stick
Camera 2 have now been confirmed live with Ailish as Player 1 and Tal as Player
2. Tal's native world model moves correctly. While armed but stationary, the
current build also commits Camera 2's horizontal forward vector through the
same native position-orientation routine used by Sudeki movement; this avoids
inventing or replaying animation clips.

The first ranged-model isolation experiment is rejected. Invoking Sudeki's
native first-/third-person presentation refresh every Player 2 render exposed
Ailish's body but left its world animation static and prevented her wand shots.
Static analysis then found the lower boundary: the ranged component retains
both its first-person wrapper at `+0x160` and its saved world wrapper at
`+0x164`, while the character position selects the attached wrapper at
`+0xB4`. The cleanroom now tests a render-only borrow. On Player 2's render
window only, it substitutes the retained world wrapper, hides the first-person
render object, exposes the world render object, and restores the exact pointer
and flags before frame end. It does not invoke the gameplay transition or
alter either wrapper's reference count.

The first live render-only borrow preserved gameplay ownership and exposed the
full body, but the retained wrapper remained in its bind/T-pose. The staff
still followed Ailish's aim, proving that actor/attachment transforms were
alive while the body's animation slots were not. A first attempt to mirror
only channel-2 blend at the exact clock callsite also remained T-posed and is
rejected.

The cleanroom also exposed a separate ranged-readiness boundary. With Combat
Mode enabled, the Ailish lead could move but could not fire normally until a
Skill Strike had first been activated; normal fire then worked. This means
the group combat transition does not by itself complete the native ranged
actor arm/control cycle. The current prototype reuses the exact native UI
transition at RVA `0x0000AFD0`, holds it for the already validated 75 ms
game-thread interval, and exits through the native false transition on combat
entry and after a verified Player 2 ownership handoff. It remains an
exact-build-gated, cleanroom-only experiment and does not write animation,
weapon, arbiter, camera, or time state. Live Tal-P1/Ailish-P2 acceptance is
still required.

Sudeki's own debug-animation display revealed the complete low-level model
interface: `+0x100/+0xFC` get/set animation selection, `+0x108/+0x104` get/set
rate, `+0x110/+0x10C` get/set time, `+0x118/+0x114` get/set channel state, and
`+0x148/+0x144` get/set blend. The focused replacement reads those values from
the active first-person wrapper and writes them to every submodel of the saved
world wrapper immediately before Player 2 rendering. It never advances the
animation clock, runs the component state machine, or replays gameplay events.
The next comparison run captured both presentation modes. Native third-person
Ailish used high-level `ANIMID_MISSILE_COMBO3` (`0x87`) on channel `4`, with
world selector `55`, for the controller weak attack. Ailish as the ranged P1
instead used first-person/strafe states such as `0x8E`, `0xC2`, and `0xC3` on
different channels. Directly resolving those first-person states against the
world wrapper produces valid animation timing but the wrong body clips. Camera
2 can therefore render and animate the complete body; the unresolved work is
an authored first-person-action to third-person-presentation translation (or
an independent world presentation controller), not geometry visibility.

The same run exposed an intermittent HUD ownership regression after changing
the two controlled character roles: Player 1 data could appear in both
bottom-right dials. A transition-only diagnostic now records the native HUD
source role, desired character, and resolved party slot independently for each
viewport. This is observation-only pending a reproduction; it does not guess
at another HUD patch.

For Tal Player 1/Ailish Player 2, the next focused build assigns ranged
presentation per viewport. In combat, Camera 2 uses a provisional eye-level
first-person transform and renders Ailish's retained arms/weapon wrapper;
Camera 1 continues to render her complete world body. Outside combat, Camera 2
restores the third-person orbit it preserved on entry. The render-window swap
is reversible and does not call Sudeki's global first-person mode. Live
confirmation is pending.

That Player-2-first-person branch is now rejected by live evidence. Enabling
combat with Ailish as Player 2 reached the raw first-person wrapper attachment
and then crashed the native model renderer at RVA `0x0021BF17` on a null
internal table entry. Camera 2 therefore remains third-person and keeps the
world wrapper for the stable fallback. Observer-side full-body presentation
for a ranged Player 1 remains active, with its animation-resource lookup fixed
to prefer the opposite authored bank instead of a coincidentally valid
first-person handle. The next test is limited to crash-free P2 combat entry and
the P1 Ailish pose seen from Tal/Buki's viewport.

Quick Menu and Spirit Strike testing exposed two further global presentation
assumptions. The Quick Menu was drawn into both cached viewports. Pure Land
replaced the one native camera several times, which caused Camera 2 to be
released and recreated from transient cinematic states; the user observed the
two perspectives briefly imposed on one another. Camera 2 is now retained
through native Player 1 camera changes, and every render-only swap restores the
exact native state active for that frame. While Player 1's Quick Menu is open,
only Player 1's frame is refreshed and the last clean Player 2 world frame is
preserved. Live validation of these two presentation fixes is pending. Global
Quick Menu slow-time and Spirit Strike slow-time remain separate combat-system
work; this pass does not claim Player 2 can continue simulating through them.

The follow-up Pure Land trace refined that diagnosis. During the strike,
Sudeki's global native camera remained Player 1's normal camera, the left scene
slot contained Player 1's render state, and the right scene slot contained the
independent Player 2 render state. Camera 2 was never released. Ailish's
first-person render object was already hidden (`RO flags & 4`), while her saved
world model was attached and visible. The remaining faded/mirrored camera image
therefore comes from Pure Land's shared screen-space/temporal presentation
history: alternating two cameras through one native history buffer contaminates
the following camera frame.

Freezing Player 2's cached image during a Spirit Strike was considered only as
a diagnostic containment policy and was explicitly rejected: local co-op
requires Player 2 to keep moving, fighting, and seeing those actions live. The
current source therefore leaves both viewport feeds active. The required fix
has now been narrowed to Sudeki's one global motion-blur history owner. An
exact-gated Player-1-caster experiment skipped the shared blur composite and
screenshot capture only during Player 2 renders. It preserved Player 2's world
render path but was rejected by the live results described below. Player 2
casting and concurrent effects still require per-context temporal/effect
history. Sudeki's existing movement, camera controls, combat actors, animation,
damage, and cleanup remain native.

The first live run exposed a callback ABI mismatch in the experimental hook
and crashed on the first suppressed frame. Native RVA `0x001DE0B0` cleans one
four-byte callback argument with `ret 4`; the original wrapper emitted plain
`ret`. The corrected build accepts that flag, emits matching stack cleanup, and
passes disassembly plus exact-image regression. This failed run is retained as
evidence, not counted as viewport-isolation success; corrected live validation
was still required. That corrected run did not crash, but it stalled Pure Land
at manager state `10`. The screenshot callback writes its required completion
byte at `callback+0x08`; suppressing the callback prevented that transition.
The policy was rejected. A replacement preserved the native Player 1 screenshot
completion and skipped only already-completed Player 2 history callbacks. Pure
Land completed under that policy, but Tal's Moon Wolf later deadlocked the
native render-job queue: Wine's main thread stopped at the
`EnterCriticalSection` call at RVA `0x001DFAF9`, targeting the queue lock at VA
`0x00804BC0`, immediately after the Player 2 history callback was suppressed.
Ghidra confirms RVA `0x001DFAD0` invokes queued callbacks while holding that
lock and moves completed jobs through the same queue family. The cleanroom and
real-time skill launchers therefore no longer enable either suppression policy.
The next experiment preserves the complete native callback lifecycle and gives
Player 2 a separate native history target. The target is created with the
exact-build `_RenderTarget` factory at RVA `0x001F6C70`, then the motion-blur
slot (`+0x10`) and screenshot-callback slot (`+0x04`) are swapped only while
their original native callbacks execute. No callback is skipped and the
completion byte remains engine-owned. The prototype retains the native wrapper
until process exit because its destructor ABI is not yet confirmed. Live
Moon Wolf and Pure Land acceptance is still required; until then, any visual
or stability result is provisional.

### First-person-to-world animation translation trace prepared

The ranged readiness prime completed its native UI transition but did not
change the observer-side pose. The next narrow pass therefore leaves all
native animation resources and selectors untouched and records the exact
translation decision made by `sync_ranged_animation_model()` for each active
channel: animation ID, both authored resource handles, target/source world
lookup results, the first-person selector, and the chosen world selector.
The trace is emitted only when a channel's decision changes for the current
component, so an electric-weapon animation cycle can be correlated without a
per-frame log flood. This is diagnostic-only; it does not write native
resources, animation IDs, or authored tables.

Acceptance question: does Ailish's idle/electric sequence fail because the
opposite authored bank resolves to the wrong world clip, or because the world
wrapper has no corresponding handle and the code falls back to a first-person
selector? A live cleanroom capture is still required before choosing a
semantic action-to-world mapping.

The live capture answered the first half of that question. Ailish's ranged
first-person IDs `0x8C`, `0x8E`, `0xC1`, `0xC2`, and `0xC3` all had a sentinel
second authored handle (`0x0007FFFF`); the remaining handle failed the world
wrapper lookup (`source_lookup=-1`). The copy therefore retained the local
first-person selectors `2`, `4`, `8`, `9`, and `10` as world selectors. That
is why the other viewport shows an animated but semantically wrong idle/weapon
cycle instead of a combat pose. This is a confirmed resource-coverage and
translation failure, not a missing readiness prime.

The next experiment must capture the same Ailish weapon actions while she is
native world-controlled (Tal as Player 1, Ailish as Player 2). That will give
us the authored third-person animation IDs/selectors to correlate before any
mapping is installed.

The control case is now confirmed. With Tal as Player 1 and Ailish as Player
2, the `U` weak attack stayed on Ailish's native world wrapper and selected
`ANIMID_MISSILE_COMBO3` (`0x87`) with world selector `55`. Therefore the
resource and animation data exist; the failure is limited to the observer
copy performed when Ailish is Player 1. The eventual bridge must translate
that first-person action state to the native world action rather than reuse
the first-person selector.

## Ranged observer vertical-aim checkpoint

All findings in this checkpoint apply to the exact supported executable,
SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

**Rejected:** pitching Ailish's complete observer-side world root is not an
aim-pose solution. It rotates the model from her feet as one rigid "seahorse"
object rather than articulating the waist, torso, and arms. The write is
disabled.

**Confirmed:** a 13-sample neutral/up/down named-locator audit resolved only
`WeaponFollow`, `Staff1`, and `WeaponParent`; all three matrices stayed
unchanged with pitch. Waist/backbone/shoulder/clavicle/arm/wrist names were not
present. A separate 15-sequence audit spanning camera pitch `-0.52360` through
`+0.26599` found the first-person and saved-world renderer slot-3 configuration
and logical channel 4 invariant. Normal fire uses first-person channels `0`
and `2`, so neither the locators nor slot 3/channel 4 is the continuous
vertical-aim seam.

Static analysis also confirms that authored state details can carry an
`Upper body` flag at `StateDetails+0x59` bit `0x08`, but exhaustive exact-image
review has not found a runtime consumer. It is metadata evidence only, not a
safe field to patch.

**Open next pass:** inventory the already-loaded first-person and world
resources for aim IDs `0x97` (`MISSILE_AIM_CIRCLE`), `0x98`
(`MISSILE_AIM_STRAIGHT`), and `0x99` (`MISSILE_AIM_STRAFE`) once, read-only.
Do not invoke their state-machine path or write a selector/pose. This pass asks
only whether Sudeki already loaded a distinct authored world-side upper-body
aim resource that the observer presentation can reuse.
