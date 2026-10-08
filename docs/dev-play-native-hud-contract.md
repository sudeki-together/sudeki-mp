# Dev Play native player HUD contract

The candidate in `lan_story_avatar_native_hud.c` reuses the four existing
native portrait/bar widgets. It replaces the separate avatar-card draw path
for Dev Play. It is presentation code: party membership, actor control and
gameplay stats belong to their existing adapters.

Reviewed 2026-10-07 at `2854c47fb544` plus uncommitted Dev Play changes.
All native addresses below are RVAs for executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Static inspection and focused tests establish the contracts below. The corrected
optional-widget candidate also has a limited two-peer visual result recorded
below. **Owner visual acceptance remains pending**. Changing host/client values,
camera quality, stable control readiness and same-process retry remain separate
acceptance steps. One normal host End Session has completed on both peers.

## Widgets, sources and portraits

**CONFIRMED_STATIC:** the native HUD singleton at `0x3C2F9C` owns a portrait
group at `+0x10C`, four gizmos at `+0x138`, and four animated UI scenes at
`+0x148`. Constructor `0x181930` creates all four independently of current
party count. Each gizmo is `0xC00` bytes, with native slot index at `+0x32C`.
The adapter requires the initialized group, exact controller backlink, classes,
animation/node/model/render-owner chains and native hook ownership before
capturing any widget. Readability alone does not admit a pointer.

Widget zero presents the local player. Widgets one through three present the
other player indices in ascending order; absent players leave their assigned
widgets hidden. The authored primary/compact placements are retained. The
adapter does not rewrite native party count or expand its four-slot storage.
AI companions do not gain additional player rows.

The synchronous runtime callback supplies only copied names, character choices,
HP/SP values, scene IDs, generation, sequence and local receipt time. It
separately validates the current native roster. HUD identity retains the native
world/UI scene and session generation; authoritative status epoch/revision need
not equal a paused client's native observer IDs. `SudekiMpLanPartyGetAvatarStatus`
is the host-authoritative source. Client actor stats are not presumed replicated,
and the adapter neither forges a stats component nor writes gameplay HP/SP.

`fresh_row()` rejects absent, wrong-scene, zero-generation/sequence, older-than-
250-ms and malformed-value rows. HP requires a positive maximum; zero maximum
SP produces an empty SP bar. Rejected rows have their complete parent hidden
and enqueue no name/value text. Native status/tactic/highlight children are
hidden because the status record does not establish their values. The adapter
does not interpret the previously reported “A” badge as a player/AI marker.

Native `0x182230` updates the existing bar materials. Its cached UV scale is
half the normalized ratio, truncated to hundredths, rather than raw HP/SP.
Native `0x9930` copies supplied text into the UI scene's owned command queue.
Player names use each gizmo's authored coordinates; primary numeric values
and HP/SP labels use the existing primary placements.

Hero portraits use native selector `0x15C070`. Talos uses `0x15C0E0` with
the typed SQX resource ID `0xA9BBCF45`, corresponding to
`SUI_PORTRAIT_BOSS_TALOSMERGED.SQX`. The typed native constructor uses kind
42 and the full filename; the ID also matches the shipped archive index.
The request drains synchronously and must leave the icon resident with no
pending token. Its existing native widget owns the resulting material/texture.
No title texture is borrowed, no pixels are extracted, and no extra gameplay
texture cache is introduced. A failed request closes presentation admission
while retaining the widget lease.

## Optional children and patch ownership

**CONFIRMED_STATIC:** `0xA9060` constructs the icon at gizmo `+0xEC`, but
`0xAA170` never binds or registers it. Its admitted dormant tuple is the exact
icon/callback classes, anchor `FFFF`, load state zero, null owner/material/
pending token, requested/effective state 3 and bounded parent state. The adapter
preserves it and never invokes its state setter. Any partial or foreign tuple
still fails validation.

The icon at `+0x264` is different: it has a real node/anchor but can have no
material. `0x15BE70` permits an empty material name; visibility method
`0x15C020` operates on the node anchor. Anchored non-texture children may
therefore have a null material. Any present material must have the exact class;
portrait/chrome and bar materials remain mandatory. This corrects the initial
predicate that rejected valid native layouts.

The adapter is the sole owner of these eight relative calls and two vtable
slots. Native function entries and the existing name hook at `0xA9EB5` remain
intact.

| Patch sites | Purpose / original native target |
| --- | --- |
| `A97CB` | Ratios, `A9D40` |
| `A9608`, `A97B7`, `A5FFE` | Status, `AAC90` |
| `A5973`, `AA965`, `15B92C`, `181E43` | Portrait assignment, `AAB00` |
| `2D9004+18` | Group draw, `1814E0` |
| `2CB59C+04` | Gizmo update, `A95E0` |

Update/data hooks refresh owned widgets before native presentation consumes
their values; group draw queues the validated player text. Unowned widgets use
the original native callbacks. Installation checks the supported image and
signatures, installs transactionally, and rolls back in reverse order.

Unbind requires the same world/UI owner and restores captured requested states,
status bits and portrait selections. It does not dereference an old actor.
Failed restoration retains the callback, identity and widget records for retry;
uninstall refuses a retained binding. Post-exit forgetting additionally requires
a positive native-Quit witness and retirement of the captured HUD singleton:
`0xA57F0` is its verified NULL writer and synchronous destructor. A caller's
assertion alone cannot discard a still-live captured layer.

## Reproduction and evidence limits

**CONFIRMED_STATIC:** the SHA-gated, read-only
[`DevPlayHudBindingReport.java`](../tools/ghidra/DevPlayHudBindingReport.java)
reproduces constructors, binding/state functions, bar/text/portrait ABI and
patch sites. The resource constructor/archive findings are also covered by
`DevPlayHeroExclusionReport.java`. Generated proprietary output stays private.

**CONFIRMED_EXACT_IMAGE:**
[`StoryAvatarNativeHudTest`](../tests/story_avatar_native_hud_test.c) verifies
all eight calls and both vtable slots against the supported image, rollback on
a foreign final call, uninstall ownership/retry, and executes relocated native
text-copy and quantizing bar code with isolated queue/material fixtures.

**CONFIRMED_TEST:** the four-widget fixture covers local-first mapping,
independent names/ratios, zero SP, stale/invalid omission, owner/NOACCESS
rejection, generation change during portrait loading, partial restoration and
retry, and positive destruction. It now reproduces the dormant `+EC` and
material-less `+264` layout, rejects foreign optional-child states/materials,
and asserts that the unused icon receives no state call. Portrait loading and
GPU rendering are substituted in this fixture; these assertions do not prove
integrated native portrait appearance.

Build target: `SudekiMP.StoryAvatarNativeHudTest`; run its executable with the
supported `SUDEKI.exe` path as its sole argument in an isolated Wine prefix.
The 2026-10-07 strict `-Wall -Wextra -Werror` build and focused run passed.
The tested dirty source SHA256 is
`bec5921066d1fbb7c55ddb14d5ad3e3069d1d90a8f886bad624e17dd0cf63185`;
fixture SHA256 is
`4202db5033c56793857b1d15afcd729eb962727f6352cde888a660b148c15d12`.
These focused test results alone do not establish four-player live rendering,
combat/status icon support, or complete runtime teardown.

**CONFIRMED_LIVE**, bounded visual/read-only observation on 2026-10-07: dirty
candidate DLL SHA256
`85a275efe466ec270d92acf24463e0335205ce98675a3cc587ef9e4e594269a3`,
using the supported executable above, ran the two-player Dev Play saved-game
New Brightwater profile with both players choosing Talos. Private screenshot
evidence IDs `native-hud-host-final.png` and `native-hud-client-final.png` show
two native Talos portraits and native bars on each peer. The primary row reads
“Dev Host” on the host and “Dev Guest” on the client; each compact peer row
shows the other name. Both primary rows display 8000 HP and 999 SP. Separate
avatar cards and visible Tal/Ailish player rows are absent in these captures.

Both processes logged native HUD activation. Guarded read-only samples confirmed
all ten owned hooks, four matching captured widget tuples, local-first maps,
resident Talos portrait requests with no pending tokens, two visible rows and
two hidden unused rows. These samples establish ownership/state, not the
contents of unobserved frames. The screenshots supply the limited appearance
evidence; neither they nor the initial full bars prove changing replicated
HP/SP. Camera framing remains poor and control readiness intermittent: the
client capture itself shows the waiting-for-avatar-control message. Owner
acceptance is still pending.

**CONFIRMED_LIVE**, normal End Session in the same `85a275e` candidate: the
host ended the session through its gameplay menu. Both peer logs recorded
`story_native_exit state=verified_return`, `world_journal_forgotten` and
`runtime_cleanup=complete`. The client additionally recorded
`native_pause_released references_balanced=1 boundary=terminal_ui`.
Private screenshot evidence IDs `native-hud-exit-host.png` and
`native-hud-exit-client.png` show the native title presentation on both peers;
the client capture also shows title-menu choices. This proves the observed
normal cleanup/return sequence for this two-player run, not arbitrary failure
recovery or every teardown ordering. Starting another session in these same
processes has not been verified.
