# Dev Play native HUD and camera investigation

## Scope and evidence

On 2026-10-07 the owner reported that the vanilla bars still showed Tal and
Ailish while both players had chosen Talos, that camera acquisition was delayed,
and that the cameras were unreliable. The added avatar cards do not rebind the
native HUD. The owner requested research into using the existing bars and
improving the camera path. Native HUD reuse is a proposed replacement for the
separate-card presentation, not an implemented or accepted change.

Baseline: `2854c47fb544` plus uncommitted Dev Play and pre-existing research
changes. The inspected two-player New Brightwater run used DLL SHA256
`730523aee47131ec7c6439144502f0a06d0a9920b05c40ee4d3053079fbacff4`.
Native addresses below are RVAs for supported executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
This investigation made no gameplay-code changes, input injections or live
native writes. Existing source changes newer than the staged DLL are identified
separately. No new gameplay acceptance is claimed.

## Why the vanilla bars still show heroes

**CONFIRMED_STATIC:** the stock HUD reads `CGroupPlayers`, rather than the
Dev Play player/avatar assignments. Its primary numeric HP/SP source is party
slot zero; each portrait gizmo has a party index at `+0x32C`. Adding a separate
status overlay cannot change those sources. Keeping native heroes as AI
companions likewise does not remove their native HUD entries.

The existing split-screen adapter provides a useful, bounded precedent:

| Native consumer | Actor-source callsite |
| --- | --- |
| Main HP/SP numbers | `0x181517` |
| Gizmo HP/SP ratios | `0xA9D5B` |
| Gizmo character name | `0xA9E15` |
| Gizmo status effects | `0xAACAB` |
| Broad portrait assignment | `0xAAB3A`, in `0xAAB00` |

`SudekiMpSplitScreenHudPartySourceDispatch` redirects the first four consumers
to existing party members without writing the party order. Historical live
hero-to-hero name, HP/SP and portrait results are recorded in
[engine-functions.md](engine-functions.md#viewport-hud-character-ownership).
That is not proof of outside-party Talos binding. The story profile also owns
the name-lookup call at `0xA9EB5` in `lan_story_name_tags.c`; a Dev Play adapter
must compose with it instead of installing a second owner.

### Reusing the existing four widgets

**CONFIRMED_STATIC:** construction at `0x181930` allocates and binds four
`0xC00`-byte portrait gizmos regardless of the current party count, with one
primary and three compact authored placements. Initialization at `0x181470`
waits for all four UI scenes. The gameplay HUD singleton at `0x3C2F9C` owns the
embedded group at `+0x10C` and gizmo pointers at `+0x138`.

Thus a four-player HUD need not add a fifth native slot. However, `0xAAB00`
still gates portrait lookup on native party count (`group+0xCC`) and gizmo
index. Redirecting the four existing data calls does not make every peer row
visible. The narrow `0xAAA90` helper shows/hides the portrait and bar children;
its contract is not a general HUD destruction or replacement operation.
It is also not proof that the complete row's text and status widgets are
hidden; those need their own visibility contract.

The pointer-copy helper `0x15B0` is generic: it links a 12-byte temporary
intrusive observer into the chosen actor's list at `actor+4`, without testing
party membership. `0x15E0` unlinks it. This does not keep the actor alive and
does not establish an owned actor reference. An outside-party source bridge
must retain a separately validated spawn/registry generation and keep every
native temporary inside its proven game-thread lifetime.

**INFERENCE:** the best reuse candidate is a presentation-only player-to-widget
map: local player in the primary widget, other players in the remaining three,
with native heroes left in the actual story party as AI. This requires a
coherent visible-count, actor-source, portrait, name and auxiliary-label
binding. It does not authorize changing party order, assigning the controller
to Talos, or expanding the four-slot structure. Showing four players plus
every AI companion simultaneously would need another presentation design.

### Portraits and auxiliary labels are separate bindings

**CONFIRMED_STATIC:** the ordinary type-to-portrait mapper (`0x3F430`) does
not provide a Talos entry for this actor; its observed resource type selects
enum zero/Tal. `0x15C070` selects from the existing hero resource table.
`0x15C0E0` loads a numeric resource ID, not arbitrary text: its loader descriptor
has a null name, and its string argument is released afterward. Passing
`SUI_PORTRAIT_BOSS_TALOSMERGED` as text with an invalid ID is not a valid shortcut.

The native Talos texture is already loadable through the separately owned
portrait provider, but installing it in `UIElementCycleIcon` still needs a
proven numeric-ID lookup or the widget's material/texture ownership contract.
The prior broad-refresh experiment caused asynchronous blank art and party
presentation churn; it is not the recommended per-frame update route.

`0xA9CD0` also fills a secondary label at gizmo `+0x2E0` through native AI
resource selection (`0x12BB60`). That source is outside the four existing HUD
data hooks. `HUD_%d_AI`/`HUD_%d_AISG` asset names exist, but they do not establish
the meaning of the exact visible "A" badge in the owner's screenshot.
`0xAA910` changes portrait states, not a proven human/AI badge setter. Human
rows need a demonstrated label binding; do not infer it from those names.

### Host-authoritative values must reach the native presentation

**CONFIRMED_STATIC:** avatar world records currently carry pose and animation,
not HP/SP. `SudekiMpLanPartyGetAvatarStatus` feeds `render_avatar_stats()` plain
host-authoritative values; that path does not update the client's Talos stats
component. Simply redirecting native HUD actor reads to a client avatar would
therefore not establish synchronized damage or resource bars.

A reuse adapter needs a proven numeric/ratio presentation seam for fresh
host-authoritative status, along with generation, scene, timeout and retirement
handling. It must not forge a stats object or mutate native gameplay stats just
to paint the HUD. Native status-effect icons likewise need a separately proven
source; the current small avatar-status message contains only name and HP/SP.

`0xA9D40` reads the component values and computes ratios; `0x182230` is the
native bar setter. The native division has no zero-maximum guard, so a
presentation adapter must handle finite/range validation and maximum SP zero
explicitly (empty SP bar). The current callback follows the intercepted native
quit-render call; widget reuse needs a verified seam before the affected HUD
reads and draws. Broad refresh
`0xA6450` also dispatches status events and writes actor state, so it is not a
general-purpose presentation refresh API.

## Camera and startup findings

### Loading is announced before avatar readiness

**CONFIRMED_STATIC**, with supporting **CONFIRMED_LIVE** log ordering:
native save loading and Dev Play avatar readiness are separate stages.
`SpawnEntity` (`0xB20D0`) schedules native EntitySetup work. The avatar adapter
waits for the matching construction/completion/destructor observations and for
arbiter initialization flag `0x400` to clear. Client pause admission waits for
all configured local avatar instances; a complete authenticated party/world
frame and control offer then permit local camera binding.

The host's current `runtime_ready`/lobby-loaded publication can precede those
avatar requirements. In the inspected run, loaded was reported before roughly
three one-second host summaries with unavailable world capture, followed by the
two ready Talos records. The initialization clip reached native time 72 at rate
24. These observations support an approximately three-second initialization
interval, not a measured duration for every stage or a timer contract.

**INFERENCE:** a single presentation handoff after the required avatar, camera,
and control preparation would avoid revealing the old hero view and then
switching late. It must preserve the native initialization checks and let their
tasks continue. It must not wait on a network acknowledgement that itself
depends on the lobby's loaded message; the existing handshake needs an explicit
dependency audit before changing the gate. In particular, host scene/world
frames and PREPARE offers must continue before awaiting client control ACKs:
the client needs those publications to bind its camera and arm control. Gating
them on all-peer readiness would create a cycle. Background input ownership can
be prepared without requiring that every window have foreground focus.

### Input ownership can remain uninitialized

**CONFIRMED_LIVE:** the inspected client's input hook was installed with no
reported hook fault, but its retained input window and focus hook were both
unset. Read-only Win32 enumeration identified a visible game window owned by
the same native runtime thread. `InputArmAvatar` remained pending. Mouse orbit
also depends on successful input enrollment.

**INFERENCE:** `ensure_focus_hook()` uses only `GetActiveWindow()` to find its
window, which can miss a background window. The precise failing predicate was
not instrumented: a `SetWindowsHookEx` failure also clears the retained window.
Per-predicate diagnostics are needed before attributing the failure solely to
active-window discovery.

**INFERENCE:** the existing native D3D device's
`GetCreationParameters().hFocusWindow` is a better source for window identity;
`title_menu_view.c` already uses that route. An implementation still needs
device, process, thread and window validation around enrollment. Background
sampling must remain neutral, and enrollment must not steal focus.

The first Start attempt also returned `ERROR_NOT_READY` (21). Host input
installation uses the active-window check and strict neutral-cache preflight;
the current diagnostic does not distinguish which failed. Held Enter is an
unproved hypothesis. `service_start()` incorrectly describes every failed
saved-game preparation as a missing matching save. A later fix should report
the actual failure and retry only a proven transient readiness condition.

### Host and client use different camera behavior

**CONFIRMED_STATIC:** `story_input` captures mouse actions `0x69/0x6A` and
suppresses the original character input handler. Client `apply_presented_frame`
consumes `InputOrbit`; host `service_host_avatar` consumes movement and
`AvatarCameraNativeDirection` but never consumes the captured orbit input.
Native target following therefore does not provide host manual mouse look.

The client's `frame_camera()` reconstructs a view from a subset of native
Exploration configuration and writes the render view while native gameplay is
paused. It has no collision/raycast step and does not execute the native camera
solver. The host instead keeps the native camera running with replaced target
slots. This is a substantial host/client behavior difference, not merely a
portrait or selection defect.

The client follows interpolated poses with a 50-66 ms buffer. Sampled logs also
recorded presentation/effects cost. Their contribution to the owner's specific
camera symptoms is **INFERENCE**, not measured visual causality. No latency or
camera-quality acceptance is established by those counters.

### Native targets need Talos-specific framing and lifecycle proof

**CONFIRMED_STATIC:** the current `avatar_matrix()` supplies identity rotation
plus the actor's raw position. The same `MatrixTarget` occupies both camera
target slots (`+0xB4/+0xB8`). It contains neither actor facing nor a per-actor
vertical offset. The client's mathematical framing adds a configuration look-at
height, but does not derive it from Talos's dimensions.

**CONFIRMED_LIVE:** both inspected processes were in Exploration, with their
owned MatrixTarget in both slots and target positions matching their local
Talos. The native stats camera-offset fields `+0x54/+0x58/+0x5C` were `(0,0,0)`
for Talos and `(0,1.8,0)` for Tal. The ordinary native `SetCameraTarget` path
uses those stats offsets. Changing to a native actor target alone therefore
does not supply appropriate Talos framing.

The native actor-follow route is more capable than a position-only matrix:

| Native operation | Proven contract |
| --- | --- |
| `GameObjectTarget` creation, `0x135040` | Generic actor-keyed cache; `EDI=manager+0x4C`, stack output pointer plus by-value 12-byte entity handle, callee pops 16. Consumes the input handle and returns one target reference; its own observer is at `+0x20`. No hero-membership predicate. |
| Target position/transform, `0x19A300` / `0x19A500` | Resolves the attached actor's `CPosition`; the transform includes facing. |
| `OffsetTarget` creation, `0x135100` | `EAX=manager+0x4C`, stack output pointer, retained source target and matrix, callee pops 12. Consumes a source reference, owns a wrapped-source reference and returns one target reference. |
| Slot installer, `0xE84C0` | `ESI=camera`, stack retained target and slot, callee pops 8. Retains the installed slot, consumes the supplied argument and can notify the active state. |
| Zero-reference release, `0x135340` | `EDI=typed-list owner`, `EDX=target`; removes the target from its manager list and destroys it. |

These are **CONFIRMED_STATIC** ABI findings, not permission to call them as
ordinary C functions or proof of full camera integration. Authored Talos data
also provides CharacterHeight 1.8, HpBarHeight 2.2 and TargetHeight 1.0, while
CameraYOffset is zero. Those are explicit framing inputs to evaluate; they are
not measured model bounds or proof of the correct camera anchor.

**INFERENCE:** an actor-keyed `GameObjectTarget` plus a separately owned native
`OffsetTarget` is the strongest next follow-camera candidate. It preserves
actor transforms and native target ownership without assigning a saved hero
to the player. Actor-aware camera-state branches and native controller/global
dependencies still require audit.

**UNKNOWN:** native camera-only stepping on paused clients. The ordinary
`CCamera::Update` at `0xE7660` exits while the native game pause is set.
Exploration helpers `0x7C190`, `0x7CBB0` and `0x7CCD0` are not yet proven
isolated camera APIs; calling them directly would require their own ABI,
collision and side-effect proof. There is no established drop-in native tick
for the current client presentation.

**CONFIRMED_STATIC:** camera state installation (`0xE79A0`) can replace state
and data pointers and destroy the old data. Newer source accepts certain
registered host Exploration/Combat/BossCombat transitions after fresh ownership
checks; that source was not in the inspected DLL and must not be credited to
its live behavior. Initial binding still requires Exploration. Cinematic,
first-person, foreign-target and additional-target cases remain unresolved;
paused client views have stricter state ownership.

## Recommended proof sequence

1. Prove a native-HUD presentation binding for one independent Talos actor:
   primary HP/SP, bars, name, portrait and status must all refer to the same
   spawn generation. Leave the actual story party and controller target
   unchanged. Then prove all four player slots, duplicates and disconnects.
2. Prepare input-window ownership in the background and make loading-to-avatar
   presentation one coherent handoff, after auditing handshake dependencies.
   Preserve neutral input, native spawn completion and retryable cleanup.
3. Prove a native camera-only update and look-input route with native collision
   and Talos framing. On clients it must coexist with the owned gameplay pause
   without advancing actors, AI, combat or world consequences. Simply resuming
   the client world is not an acceptable camera implementation.
4. Test both windows for orbit, approach to walls, movement, combat transitions,
   focus loss/regain, loading and exit/retry. Only after those checks should the
   old separate cards be removed from the accepted native-HUD profile.

This sequence is a research recommendation. Native HUD replacement, reliable
client control, camera parity and Talos damage are still incomplete.

## Reproduction references

- `tools/ghidra/DevPlayHudBindingReport.java`: bounded construction, actor-link,
  count/visibility, label and portrait-resource investigation. Its read-only
  supported-image run completed with all requested functions resolved.
- `tools/ghidra/HudOwnershipReport.java` and `HudPortraitBindingReport.java`:
  earlier native construction, source and portrait research.
- `tools/ghidra/DevPlayCameraStateReport.java`: supported-image target creators,
  getters, state transitions and update/look helpers. The read-only run
  completed for three camera states and 27 dependencies without unresolved
  functions. These decompilation results are static evidence, not runtime tests.
- `src/hooks/split_screen_render.c`, `lan_story_name_tags.c`:
  existing HUD source and name hook ownership.
- `src/hooks/lan_story_avatar_camera.c`, `lan_story_runtime.c`,
  `lan_story_input.c`, `lan_story_avatar_spawn.c`: current staged-path design
  and newer explicitly identified candidates.
- `src/network/lan_story_world_frame.h`, `lan_party_session.c`,
  `src/ui/story_avatar_stats_view.c`: pose/status separation and value freshness.

The exact-image research reports are read-only and hash-gated. Raw executable
output, memory samples, assets and machine/session details remain private.

The sanitized findings were posted and their stored bodies verified in
[Dev Play tracker #43](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/43#issuecomment-232)
and [damage issue #41](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/41#issuecomment-233).
The AST graph was refreshed after the research-script changes (13,733 nodes,
43,431 edges). Its existing partial C-parser warnings remain; the graph is a
navigation aid, not evidence that native behavior works.
