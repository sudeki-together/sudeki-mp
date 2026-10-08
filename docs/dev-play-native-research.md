# Dev Play native contract checkpoint

Reviewed 2026-10-07 against supported SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Revision: `2854c47fb544e5ebbd7e1fbf7a2cda5e333c6bc8` plus uncommitted Dev Play
research files. This is a bounded research record, not gameplay acceptance.

## Damage observation

**CONFIRMED_STATIC:** `tools/ghidra/DevPlayAvatarContractReport.java` checks the
supported file hash before decompiling the receiver, weapon, spawn and camera
functions. Instruction inspection confirms:

| Function RVA | Contract |
| --- | --- |
| `0xD1B00` | Three stack arguments: source combat component, target entity, damage packet; callee removes twelve bytes. Prepares the strike and conditionally reaches accepted damage. |
| `0xD21D0` | Target combat component in ECX, packet on stack; native accepted-damage receiver. |
| `0xD95C0` | Copies damage minimum/range into packet `+0x28/+0x2C`; packet `+0x3C` selects the equipped item. With no item, it uses weapon component `+0x334/+0x338`. |
| `0xD7C10` | Native `thiscall(weapon_component, item)` equip path; writes the item at weapon `+0x268` and configures model/animation/attachment state. |
| `0xD92D0` | Weapon presentation-state transition, taking the component and state on the stack. It is not an item constructor or an equip-item setter. |

Correction to the proposed experiment: `0xD92D0` does not populate a missing
weapon item. Its write at `0xD9491` clears `+0x268` before passing a deferred
item to `0xD7C10`. Also, a null item alone does not prove zero damage: the
native packet builder has the component-damage fallback described above.
Talos's actual fallback values and resulting accepted packet require a live
comparison with Tal on the same target.

`src/hooks/lan_story_talos_damage_probe.c` owns only six direct call operands:
`D3A78/DABBC/1046E8 -> D1B00` and `D3B61/D3EDA/DAB91 -> D21D0`.
It leaves both native entry points intact. Existing host hit feedback and
client containment may subsequently own `D21D0`; forwarding still reaches
that owner. Install only at quiescent startup after the loader's supported
file-hash check, before those entry owners. The loader opt-in is
`[DevPlay] Enabled=true` together with `MeleeTrace=true`; use it on the host
for authoritative damage evidence.

The stubs preserve general registers, flags, x87/SSE state, the original
arguments and the native return address before forwarding. They copy source,
target, caller, packet `+0x14/+0x28/+0x3C`, packet flags, current weapon item,
fallback damage fields and target HP where exact class/component-owner gates
pass. Unrecognized entities/components remain unknown. Observations are
bounded and never write gameplay state or equip an item. Accepted-entry
logging observes HP before that call; it does not establish HP loss afterward.

**CONFIRMED_EXACT_IMAGE:**
`tests/lan_story_talos_damage_probe_image_test.c` was cross-compiled with
`-std=c11 -O2 -Wall -Wextra -Werror` and run under Wine against the supported
user-supplied executable. It maps inert PE sections without running gameplay.
All six callsites/targets and both entry signatures matched. Tests passed for
each callsite opcode/target mismatch, entry mismatch, normal install/restore,
duplicate-install rejection, foreign-owner restoration refusal with independent
restoration of the other five seams, exact-owner retry, repeated teardown and
reinstall. Both native entry sequences remained unchanged. Reproduction:
`SudekiMP.StoryTalosDamageProbeImageTest.exe "<supported SUDEKI.exe>"`.

Not proved: gameplay reachability, register-state preservation under actual
gameplay, each injected installation-write failure, live Tal/Talos packet
comparison, HP reduction, item compatibility with Talos, or disconnect/unload
behavior. No native equip experiment is enabled. The existing
`weapon_activation_abi.c` deliberately admits only exact retail hero families;
Talos is not one of those families. An equip experiment must separately prove
item database ownership, Talos's weapon component and attachment topology,
native swap/task drain, and restoration.

## Avatar creation and camera

**CONFIRMED_STATIC:** `InternalSpawnPC` (`0xB1B00`) and `SpawnEntity`
(`0xB20D0`) return no actor. They dispatch asynchronous `EntitySetup` work
through `0xB1900`. The public `SpawnEntity` argument names a resource, not a
new instance identity. A second `GetGenericEntity` lookup cannot distinguish
several same-resource avatars.

The shared completion path `0xB1530` identifies hero resources and, with the
exported spawn routes' final index `-1`, calls `CGroupPlayers::Add` at callsite
`0xB15DB`. Thus the proposed direct `InternalSpawnPC` call does not establish
an outside-party duplicate hero. The internal completion branch skips party
insertion for values below `-1`, but no supported adapter for that complex
internal spawn ABI has been established. Duplicate heroes remain unsupported.

`lan_story_world.c` already provides the relevant registry capture pattern:
entity-manager global `0x409D8C`, count `+0x34`, entries `+0x3C`, with a second
owner/count/array check. Comparing captured sets can identify newly registered
instances. It does not prove native initialization completed. The existing
`lan_story_task_trace.c` owns the `EntitySetup` constructor/completion/destructor
seams and records diagnostic actor identities; an avatar consumer must share
that owner and extend its lease evidence instead of patching it again.

**CONFIRMED_EXACT_IMAGE:** `lan_story_avatar_spawn.c` now consumes a bounded
observer published by that same constructor/completion/destructor owner. A
caller brackets the established `SpawnEntity("ALLY_TALOS", ...)` export with
`AvatarSpawnBegin/End`, carrying seat, epoch and monotonic request generation.
The constructor return identifies the job; the original completion call's
actor argument identifies the instance. Up to four pending requests may finish
in any order. No complex `B1900` call, resource-name rewrite, party insertion,
second detour, cancellation or name-based actor lookup is introduced.

`AvatarSpawnObserve` repeats the saved-load generation, native world pointer,
entity-manager membership, full captured party roster, Ally class/resource and
eight component class/backlink checks. It rejects preexisting actors and actors
claimed by another request. Readiness additionally requires completion return,
the deleting job destructor's return, and the settled arbiter spawn flag. This
is a bounded actor identity witness; independent input, camera and scene leases
remain necessary, and descendant-script quiescence is not asserted. Unknown
records and callback dependencies remain until positively witnessed native
world destruction. The owner refuses uninstall while the observer is attached.

`tests/story_avatar_spawn_image_test.c` runs the real relocated supported-image
constructor through the installed native hook, then fixture completion and
destructor bodies through the existing ABI bridges. Tests passed for three
same-resource jobs completing out of order, no readiness before destructor
return, unreadable deleted-job storage, settled-state waiting, separate actor
identities, repeated actor rejection, changed component owner, changed world,
party mutation, foreign-thread rejection, callsite mismatch, failed native-exit
witness, monotonic generation reuse and retained unknown records. Both the
native export's resource loading and actual gameplay completion remain untested.
Reproduction: `SudekiMP.StoryAvatarSpawnImageTest.exe "<supported SUDEKI.exe>"`.

**CONFIRMED_TEST:** The optional Dev Play world resolver assigns per-player
portable ally IDs `0xFFFFFF00..0xFFFFFF03` only after a fresh spawn lease resolves
the already validated native Ally. Its target retains the native resource ID
separately and rechecks it; no native name/hash changes occur. The resolver is
immutable once host/client catalog use starts and repeats during capture,
readback, and native pose-write ownership checks. A missing or stale resolver
result refuses the complete batch. With the resolver absent, the legacy native
resource identity path and version-7 wire format remain unchanged. Only the
separate Dev Play profile may configure this route.

`tests/story_avatar_world_test.c` passed with a fixture resolver: four actors
sharing one native resource ID map to four distinct portable IDs independent of
registry order; changed world/epoch/generation and invalid player/generation are
rejected; duplicate portable identities fail frame validation; hero identities
cannot use these ally IDs; all four records survive wire encode/decode. Native
capture/presentation and an actual host/client mirror pair remain untested.

**CONFIRMED_STATIC:** `CCameraManager::SetCameraTarget` (`0x37170`) takes a
camera name and `GELPointer`, resolves an actor, creates/retains its
`GameObjectTarget` (`0x135040`), installs it in camera slot `+0xB8`, and installs
an `OffsetTarget` in `+0xB4`. `0xE84C0` manages old/new target references and
notifies the active camera state. The existing story local-view contract
requires the roster's native leader hero to own this target chain. Merely
changing a view anchor does not create an avatar-owned native camera.

The exact camera-manager constructor `0x36890` publishes the same owner into
globals `0x409D7C` and `0x3C2F30`, with primary vtable `0x2C7B80`. Its target list
starts at `+0x4C`; `MatrixTarget` head is list `+0`, with links `+0x70/+0x74` and
vtable `0x2D43BC`. Creator `0x134FB0` takes three stack arguments (list, output,
matrix), returns with twelve-byte cleanup and supplies one retained reference.
Target installer `0xE84C0` consumes its incoming retained reference, retains the
camera slot and passes a retained argument to the state callback. Simple assumed
reference-count deltas do not cover that callback's additional target ownership.

`DevPlayCameraStateReport.java` identifies native `Camera::Exploration` vtable
`0x2D9854`, target-change virtual slot `+0x40 -> 0x7EC80`, and its callback's
target/reference operations. State data uses vtable `0x2CA694`; the callback can
release and clear its additional target at `+0x470`. The callback's `0x7BB90`
path invokes Exploration virtual `+0x24 -> 0x134610`, which updates the render
state reached through camera `+0x34`: point fields `+0xC0/+0xC4/+0xC8` and dirty
serial `+0x2C`. A camera adapter must account for that native mutation before
retaining its initial view. These static contracts help validate a separate
adapter, but do not prove host-avatar camera behavior.

`split_screen_render.c` contains a separate-camera construction and
`SetCameraTarget` integration to study, but does not by itself validate host
Talos control, restoration during transitions, or an independent story-avatar
camera. These cases remain **UNKNOWN** and are not enabled by this probe.

## Title portraits

**CONFIRMED_STATIC:** `cleanroom/menu.c` resolves a borrowed GPU texture only
through a validated `UIElementCycleIcon -> cSOLMaterial -> cResidentD3DTexture`
chain. Its established title route borrows four Load Game widgets from the
page at title controller `+0xB4`, icon array `+0xA4`, after initialization and
owner/anchor validation. `0x15C070` selects an initialized native resource-table
entry; it does not create an anchor or guarantee title residency.

Those stock widgets are not guaranteed resident when the independent picker
opens. The new `title_portraits.c` adapter therefore requests independent native
resident wrappers through the same lower-level texture loader; it does not
activate, hide, or destroy a Load Game page. `DevPlayTitlePortraitReport.java`
and supported-image instruction inspection establish:

| Function RVA | Contract |
| --- | --- |
| `0x1D92E0` | Six-argument cdecl loader: `{identifier,name}` descriptor, request flags, residency flags, pending-list address, completion callback, callback tag. Accepts the explicit resource-name path. |
| `0x1D6A90` | Constructs an eight-byte `cResidentD3DTexture` wrapper (vtable `0x2DD80C`), retains its backend, and starts residency. |
| `0x1D7000` | Creates a native request record and links a wait token to the supplied pending-list address. |
| `0x1D6C70` | Wait token virtual `+4`; drains native texture work through `0x1D9940`. |
| `0x1D7320` | Completion unlinks/deletes the wait token and explicitly skips a null callback; it does not delete the caller's retained resident wrapper. |
| `0x1D6B30` | Resident deleting destructor: releases residency, then backend ownership, then wrapper storage. |
| `0x1F54D0` | The string-name SQX decoder appends `.sqx` using the format at `0x2D5708`, then opens that file through `0x1E3620`. The input name must omit the extension. |

The adapter supplies the extensionless native names `SUI_PORTRAIT_BUKI`,
`SUI_PORTRAIT_ELCO`, `SUI_PORTRAIT_TAL`, `SUI_PORTRAIT_AILISH`, and the
owner-specified `SUI_PORTRAIT_BOSS_TALOSMERGED`, with the stock CycleIcon request flags
`0x20` and residency flags `0x80`. The last resource is separate from the hero
type-to-portrait table; the default branch of that table is not Talos artwork.
No arbitrary texture-global write, asset extraction, copied pixels, additional
native widget, replacement native vtable, or mod completion callback is used.

The title update owner calls `Service` only under its current title lease.
Each native request is attempted once per acquired owner tuple and drained via
the verified wait token, with resident/token/request/backlink checks. The
adapter retains the native wrapper independently of the title page. Resolve
rechecks the active title/scene/device tuple, resident class, backend reference,
and COM texture type/device/dimensions, returning only a current-paint borrowed
texture. `GetDevice`'s temporary reference is balanced. Release does not read
retired title/scene pointers, and refuses a changed device, native signature,
wrapper, or unknown pending request while retaining dependencies for retry.

**CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST:**
`tests/title_portraits_image_test.c` passes whole-function normalized signatures
for the loader, resident constructor/destructor, wait, request, completion and
stock caller, plus mutations of each relevant entry/vtable. Native creation,
wait/destruction and COM methods are fixtures: these verify the five resulting
filenames after the native suffix, reject a changed suffix format, and verify the
argument ABI, one request per acquisition, pending-list ownership, no deleted
token reread, all five character mappings, stale scene/device rejection,
foreign-thread rejection, balanced temporary COM references, refused release
and exact-owner retry, release after the title owner is retired, and repeated
acquisition/release. The test does not execute the retail asset loader or draw
portraits. Reproduction:
`SudekiMP.TitlePortraitsImageTest.exe "<supported SUDEKI.exe>"`.

**CONFIRMED_LIVE, bounded:** the first staged run acquired all five wrappers
but returned no GPU texture. Read-only observations after the title transition
found no retained portrait records. Static follow-up identified the duplicate
`.SQX.sqx` suffix in that candidate; the corrected extensionless descriptors
and explicit filename regression pass the fixture. The corrected host run
subsequently validated all five native GPU textures at 64 by 64 pixels,
including Talos Merged. Visible picker acceptance is a separate check. Bounded
per-character logs now distinguish backend texture, COM type/method/device,
and dimension failures and record the first validated GPU texture.

## Gameplay avatar portrait ownership

`lan_story_avatar_portrait.c` uses a separate native resident wrapper for the
Talos stats card. The loader, wait and destructor established above operate on
the texture subsystem and device; they have no title-page or world callback.
Only the pure supported-image signature validator is shared with the title
adapter. The two providers have independent retained instances and release
paths. Neither calls the other's acquisition or destruction functions.

Service first requires a fresh, ready `AvatarSpawnObserve` result on the
verified native game thread, then admits one world/epoch/device scope. It
requests the extensionless Talos Merged name once, drains the verified pending
token, and repeats spawn/world/device validation after that native work. Each
per-player paint resolution requires a new exact local spawn observation;
several Talos cards may borrow the one retained texture. A scope change refuses
until explicit release. Release uses its own wrapper, native functions, device
and thread only; it never dereferences a retired actor or world.

**CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST:**
`tests/story_avatar_portrait_image_test.c` passes against the supported image,
using fixture native creation/COM calls. Coverage includes four separately
validated avatars sharing one texture, title release leaving gameplay ownership
intact, stale/unknown spawn identity, foreign thread/device refusal, signature
and wrapper refusal with exact-owner retry, world loss during native wait,
release after world retirement without a spawn/actor read, no first-thread
claim without spawn proof, repeated acquisition and balanced COM references.
Reproduction: `SudekiMP.StoryAvatarPortraitImageTest.exe "<supported SUDEKI.exe>"`.
Native gameplay loading, rendering, and integration teardown remain **UNKNOWN**
until the staged runtime executes them.
