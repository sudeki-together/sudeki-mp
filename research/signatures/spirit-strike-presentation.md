# Spirit Strike presentation-only replication

These findings apply only to Sudeki executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`,
image base `0x00400000`.

The purpose of this note is to separate presentation calls that a LAN client may
replay from native Spirit Strike calls that own gameplay, time, camera, or
character state. The analysis was produced read-only by
`tools/ghidra/SpiritStrikePresentationReport.java`.

## First safe semantic event: audio cue

The narrowest verified presentation-only entry is `CSound::PlayCue`:

```c
CSound *__cdecl GetSound(void);                         /* RVA 0x000170b0 */
void __thiscall CSound::PlayCue(CSound *, const char *); /* RVA 0x00017090 */
```

`GetSound` returns the singleton stored at preferred VA `0x00808d40`.
`PlayCue` takes the cue name in its only stack argument and submits it through
the XACT-backed audio path at preferred VA `0x0068aeb0`. Its exact body does
not call Spirit management, combat, camera, time, or entity mutation code.

Exact `PlayCue` bytes:

```text
8B 44 24 04 6A 00 6A 00 6A 01 6A 00 50 8B 41 18 E8 0B 3E 27 00 C2 04 00
```

Relocation-tolerant form:

```text
8B 44 24 04 6A 00 6A 00 6A 01 6A 00 50 8B 41 18 E8 ?? ?? ?? ?? C2 04 00
```

Exact `GetSound` bytes:

```text
A1 40 8D 80 00 C3
```

The four-byte `A1` operand has a `HIGHLOW` relocation at RVA `0x000170b1`.
The live exact-image gate therefore verifies the opcode and return byte, then
requires the decoded operand to equal `module_base + 0x00408d40`; comparing the
preferred-base bytes verbatim rejects every ASLR-loaded process. Relocation-
tolerant form:

```text
A1 ?? ?? ?? ?? C3
```

The actor mapper also exposes this hookable boundary:

```c
void __thiscall CCharacterSoundMapper::GEL_PlayCue(
    CCharacterSoundMapper *, const char *, bool, float, bool);
/* RVA 0x00004e20 */
```

It delegates to preferred VA `0x004fc200`, which applies actor/distance audio
parameters before reaching the same XACT submission family. It is suitable for
host observation, but the minimum client adapter should use the simpler global
`CSound::PlayCue` call until a stable local actor-to-sound-mapper lookup is
verified.

Names found in `sound/TalSoundScript.XSB` include the following Spirit-related
cue candidates:

```text
Tal_strikevox_1
strike_ailish_attack_floorcircle
strike_spirit_morph2ailish
strike_Tal_spellalice
strike_Tal_attackatmos
strike_Tal_morphtoTal
strike_Tal_spell
strike_Tal_spell_moon
strike_Tal_spell_sparks
strike_Tal_strike
spirit_strike_init_1
spirit_strike_init_2
spirit_strike_init_3
spirit_strike_init_4
strike_spirit_travel_split_01
TAL_SS_strikevocal
ALICE_SS_initiate_floorc
ALICE_SS_initiate_start
ALICE_SS_morph2alice
ALICE_SS_morph2spirit
ALICE_SS_travel_T2A
ALICE_SS_travel_T2A_2
ALICE_SS_waitloop
TAL_SS_Aliceheal
TAL_SS_attackatmos
TAL_SS_morphtoTAL
TAL_SS_spell
TAL_SS_spellmoon
TAL_SS_spellsparks
TAL_SS_strike
strike_spirit_travel_split
```

Fresh LAN-host traces of both variants established the same opening order:
`spiritstrike_start`, then `stop_ailish`, `stop_alexine`, `stop_buki`,
`stop_elco`, `stop_kazel`, and `stop_tal`. Only `spiritstrike_start` is an
obvious presentation replay candidate. The stop cues can mutate native mixer
state beyond the one remote transaction and therefore remain trace-only.
Network packets carry a validated START enum, never a raw string; the client
maps that enum to the compiled-in `spiritstrike_start` constant.

## Bounded host event journal

LA22 encodes an event as:

```c
struct SpiritAudioEventWire {
    uint16_t event_sequence;
    uint16_t skill_sequence;
    uint8_t cue_id;
};
```

Eight events plus a count byte consume 41 bytes. With the current measured
worst-case arena packet of 705 bytes, this remains within the 768-byte packet
cap at 746 bytes.

The host records only the allowlisted start cue observed while its native
Spirit transaction is active. The client plays an event only when its skill
sequence exactly matches the active replicated Tal Spirit transaction, its
event sequence is fresh, and its cue ID is START. Each matching event is played
once through the replica's local `CSound::PlayCue`. A replica that missed the
whole active interval may admit a structurally valid monotonic retained entry,
but its cursor consumes that entry without replay when the exact transaction is
no longer current; this prevents both delayed sound and a permanent snapshot
rejection loop. Disconnect, arena reset, or session-generation loss clears the
journal and deduplication state. The shared reducer rejects stale, rewritten,
duplicate-append, or ambiguous modular journal transitions before interpolation;
delivery consumes the authenticated snapshot directly and is independent of
the replica render clock.

## Client animation handoff

Spirit animation replication does not invoke the Spirit manager or start a
second gameplay task. The client applies authenticated Tal presentation
channels only after actor-local combat readiness is established.

Ailish readiness includes the world fallback where `component+0x164` is null,
`CPosition+0xb4` owns the attached world wrapper, and `component+0x160` owns
the distinct first-person wrapper. The native `0x001888f0` / `0x00188a90`
model-switch path resets animation state before reporting an already-active
model as a no-op. Client code may call it only for a fully proved opposite
topology. Desired topology skips it and unknown topology fails closed. Native
weapon reattachment and visibility retain separate exact preconditions,
writable-target admission, and postconditions.

## Unsafe native paths

None of these functions may be invoked by the client presentation adapter:

| Function | RVA | Reason |
| --- | ---: | --- |
| Spirit manager activation | `0x0000fba0` | Owns participants, global speed, camera, control leases, and lifecycle events. |
| `CSpiritStrikeManager::FireSoul` | `0x00010570` | Mutates manager-owned soul slots at `manager+0xb0`. |
| Strike stage start | `0x00010c20` | Dispatches the resource's arbitrary `Power Up Script`. |
| `CEntityAttacks::StartSpiritStrikeAttack` | `0x000cc080` | Enters native attack creation and damage-capable execution. |
| Soul launch | `0x0012b0b0` | Creates an SFX entity but mutates manager-owned soul runtime; spawned resource behavior is not yet proven harmless. |

The Tal Spirit scripts confirm why script replay is unsafe. Their call graph
contains speed and light effects, render-camera changes, animation locks,
weapon visibility, native Spirit attack creation, and, in the spell path,
`ModifyEntitiesHP`.

## Fixed opening VFX: `SFXSS250_Initiate`

The earlier blanket restriction to audio-only replay is narrowed by the fixed
opening resource below. This remains dirty research work on `53623a0a2ad4`,
reviewed 2026-09-04. Source and native/resource inspection establish the bounded
`CONFIRMED_STATIC` claims in this section. The owner's subsequent visual
observation in the dirty Tal-host/Ailish-client LAN test confirmed client rings,
slightly delayed, with all later effects still absent. That bounded visual
result does not establish summon presentation or lifecycle acceptance. Current
test and live records belong in `docs/research-log.md` and
`docs/status-matrix.md`.

The authored Tal Spirit sequence in `SOLData.baf` plays
`SFXSS250_Initiate_Camera`, enters `ANIMID_SUMMONER_CHARGE`, waits, then calls
`PlaySfx(Tal, "SFXSS250_Initiate")` before the subsequent
`ANIMID_SUMMONER_WAIT` phase. The authored `IsPlaying(Tal)` wait concerns
animation; it does not prove a wait for effect completion. Only the fixed
`PlaySfx` resource is admitted by the client adapter. Its fixed native backing
request is `SFXSS250_Initiate.HOM`, identifier `0x3cef3b8f`. The resource is a
ring/floor-pattern opening effect with particles/meshes and authored sounds.
The prior claim that this resource alone supplied the overhead summon was not
established; the later strike resources contain the named Tal Spirit mesh and
skeletal hierarchy. Its exact duration is `UNKNOWN` pending a native clip-field
decoder. The former 40-frame claim confused the final listed sound-event frame
with clip duration. Read-only
resource inspection found no damage, collision, script, camera, actor-spawn,
or missile references in that resource. The camera and gameplay calls in the
surrounding script are not part of this resource closure.

### Exact native ABI

All addresses below are RVAs in the supported image identified above.
`src/hooks/lan_arena_spirit_vfx.c` owns the native adapter and byte preflight.

| Boundary | RVA | Verified contract used by the adapter |
| --- | ---: | --- |
| `GetSFXManager` | `0x00019770` | `A1` singleton load followed by `C3`; relocated operand must equal `module_base + 0x00408d48`. |
| `CSFXManager::PlaySfx` | `0x00018de0` | x86 `thiscall`, seven stack dwords, `ret 0x1c`: actor `TPtr*`, `ResourceName*`, follow flag, real-time flag, and three floats. |
| `CSFXManager::PreCacheEffect` | `0x00019540` | x86 `thiscall`, one `ResourceName*`, `ret 4`; adds one cache reference and may begin loading. |
| `CSFXManager::UnCacheEffect` | `0x00019650` | x86 `thiscall`, one `ResourceName*`, `ret 4`; releases one cache reference. |

The preflight verifies the complete `PlaySfx` entry body and selected exact
PreCache/UnCache entry and tail signatures. Live manager admission also requires
its two exact vtable identities at `module_base + 0x002c62a0` and
`module_base + 0x002c62a8`, and writable cache storage. The fixed call uses the
authored defaults `followCharacter=false`, `realTime=false`, and `x=y=z=0`.
No packet selects a resource string, resource identifier, native address, or
these flags.

`PlaySfx` synchronously reads the transient actor `TPtr`; the non-follow path
copies the actor transform into `SfxSetupMatrix`. `SfxSetup` retains its own
ResourceName references. The adapter releases its temporary ResourceName after
the call and never retains an effect pointer. Native effect-parent observation
uses Sudeki's weak observer mechanism, whose actor teardown clears the observer.
The admission witness re-resolves Tal and verifies the position/transform,
attached render wrapper, render callback, effect-parent observer chain, and
animation source before native entry.

### Resource prewarm and retirement

`CONFIRMED_STATIC`: the extensionless script name and the archive file have
different identifiers. The generic ResourceName constructor at RVA
`0x001b9440`, using kind `0x7f`, uppercases the text before the alternating
add/multiply hash at RVA `0x0003a4a0`. `SFXSS250_Initiate` produces
`0x15fef04d`, which is absent from the supported `SOLData.baf` archive.
`SFXSS250_Initiate.HOM` produces `0x3cef3b8f`; its trailing archive-index entry
at offset `0x3153808c` describes HOM data at `0x233f7000`, size `0xc068`,
containing the expected authored effect name.

Native PreCache at RVA `0x00019540` passes the supplied identifier directly
to the asynchronous loader without converting the text to a backing filename.
The normal typed gfx preload at RVA `0x0003f800` instead routes kind `0x29`
through RVA `0x001b9510`: strip an existing extension, append `.hom`, uppercase,
then hash. `PlaySFXWithAll` at RVA `0x00018e70` selects that same kind `0x29`.
Therefore the fixed direct-PreCache adapter must request the `.HOM` identity.
`construct_initiate_resource` now uses the fixed `.HOM` literal and rejects
any constructed identifier other than `0x3cef3b8f`. The shared generic
`SudekiMpCleanroomEngineResourceNameFromText` helper is unchanged.

This corrects the initial extensionless pre-cache request: its cache reference
could be acquired without loading the intended file. A reference-count increase
alone does not prove a resource request can complete. The corrected request
still requires independent loaded/no-pending readiness and live visual
acceptance.

`CSFXManager` has 64 cache slots at stride `0x18`: pending object at `+0x410`,
loaded object at `+0x414`, resource identifier at `+0x41c`, and reference count
at `+0x424` for slot zero. `PrepareTalInitiate` samples the exact manager and
slot state, calls PreCache once, and requires a unique matching slot with the
expected one-reference increase. Later calls only poll that acquired lease.
An ambiguous acquisition poisons the lease and cannot trigger another acquire
for that manager identity. `READY` requires a loaded object and no pending
object; replay freshly checks the same manager/resource/slot before entry.

Correction to the preliminary preload interpretation: RVA `0x00018760`
resolves the fixed `SpecialEffect` factory, not the requested authored SFX.
The `SfxSetup` constructor at RVA `0x00017eb0` can load the requested resource
asynchronously. PreCache is therefore a timing/readiness prewarm, not a
requirement that makes ordinary `PlaySfx` semantically valid.

The ready synchronous spawn may release its pre-cache reference while the
effect continues. The retail caller at RVA `0x000be9e0` demonstrates PreCache
at `0x000beb61`, synchronous effect creation through `0x00017da0` at
`0x000bea98`, then UnCache at `0x000beaa7`. The adapter follows that ownership
pattern: after PlaySfx entry it releases the proved cache reference once when
the manager/slot witness remains exact. Call entry always consumes the replay
event, even if a later observation fails. If the same manager remains current
but cleanup cannot be verified, `RELEASE_PENDING` retains the exact acquired
reference. Explicit Release or the next Prepare retries that cleanup before
another acquire; neither may replay the consumed event. Release can retry only
when native UnCache was not entered, so the reference cannot be released twice.
Only a changed manager identity is forgotten without dereferencing or releasing
through the stale manager.

Native `SpecialEffect` construction sets auto-retirement bit `0x04`; completion
event `7` reaches the generic handler at RVA `0x00131d60` and retirement path at
`0x00131df0`. The handleless replay preserves that native retirement contract.
The mod does not install an effect callback, retain an effect handle, force a
completion field, or invoke an effect destructor.

### Client admission and acceptance boundary

`stage_client_spirit_vfx` in `src/hooks/lan_arena_client_replica.c` consumes the
existing authenticated LA22 START journal using a separate cursor from audio.
It records the session, accepted host tick, Tal actor/generation, event sequence,
and Spirit sequence. `SudekiMpLanArenaClientReplicaServiceSpiritVfx` runs after
the first successful visible-transform publication before native `RenderStart`.
It waits until the render clock reaches the accepted tick and Tal has the exact
active Spirit sequence, actor-local readiness, and selector `75` with two
channels. Selectors `113`/`114`, transaction retirement, replacement actors,
and authority loss cannot trigger a late opening effect.

Pre-cache readiness polling is bounded and stops once ready until replay or
invalidation; native PlaySfx revalidates that cached readiness at entry. A
rejected pre-entry call may retry while the same opening phase remains valid.
Once native PlaySfx is entered, the event is consumed once regardless of visible
success. Native-call barriers cover cache operations and replay, preventing
reentrant actor release or reset from clearing live call dependencies. Pending
events and owned pre-cache references participate in replica reset/session
cleanup; already-created finite effects remain engine-owned.

There is no new VFX wire event or protocol bump. Mapping START to this fixed
opening resource implements one local semantic effect, not a trace of every
native SFX emission. The owner observed client rings with a small delay; the
summon and later effects remain absent. Timing relative to the charge, repeat
casts, actor retirement, disconnect, and teardown still need acceptance. A
`state=fired` log proves native call entry only.

## Excluded VFX paths

`CSpiritStrikeManager::FireSoul` at RVA `0x00010570` mutates manager-owned soul
slots beginning at `manager+0xb0`. The soul launcher at RVA `0x0012b0b0`
selects a dynamic resource, creates an effect/entity through RVA `0x00017da0`,
and stores the resulting pointer at `soul+0x40`. Those ownership facts do not
prove the selected resource is presentation-only; absence of a direct damage
call in the launcher is insufficient.

The opening allowlist excludes `SFXSS251_Initiate_Loop_Wait`, the later
`SFXSS112`, `SFXSS800`, `SFXSS252`, `SFXSS801`, and `SFXSS802` resources, and
the damage-capable `SFXSS300_Tal_Spirit_Strike`. Later loop, travel, morph,
and attack presentation requires separate resource closure, semantic admission,
and native retirement evidence. Raw native resource IDs must never be
serialized, and the dynamic soul launcher remains excluded.

## Required test seams

- Event journal: wraparound, bounded history, repeated cues, stale/duplicate
  rejection, wrong-skill rejection, malformed cue IDs, and reset/disconnect.
- Client adapter: injected `GetSound`/`PlayCue` function table, exact enum-to-
  constant mapping, one-shot delivery, and fail-closed behavior when the sound
  singleton or exact-image verification is unavailable.
- Opening VFX: `tests/lan_arena_spirit_vfx_test.c` exercises fixed arguments,
  acquire-once/poll behavior, exact reference-count witnesses, stable poison,
  replay rejection before entry, one release after entry, manager drift, and
  exact-image signature mutation/relocation checks. These are deterministic
  seam and image checks, not visual acceptance.
- Exact image: verify the function RVAs and prologue bytes above, including the
  relocated `GetSound` and `GetSFXManager` singleton operands, before any hook
  or replay call; exercise complete install rollback and uninstall.
- Runtime ordering: `tests/lan_arena_runtime_hook_test.c` covers VFX service
  after successful visible-transform publication and before `RenderStart`.
  Live repeated-cast and disconnect/retirement acceptance remains separate.
- Safety: keep a static denylist test showing that the presentation adapter has
  no call edge to the Spirit manager, stage dispatcher, native attack path,
  game-speed ownership, camera ownership, or damage APIs.

## LA23 parent-free visual roster closure (2026-09-04)

This section supersedes the LA22 opening-only admission above for the new
roster adapter, not for the retained legacy START wrapper. It is native/static
and deterministic-test evidence; it does not establish live visual parity.

The generic entity event handler at RVA `0x00131d60` is not the complete
animation-event route. `PlaySFXWithAll` additionally calls RVA `0x00131c50`,
which weakly observes the source actor's graphics component at effect `+0x3d4`
and subscribes listener `effect+0x148` to the effect's graphics event source
`effect+0x288`. Its vtable is RVA `0x002d3cfc`; callback RVA `0x00131d20`
forwards authored events to the actor graphics dispatcher at RVA `0x0018a900`.
Consequently, ordinary actor-parented `PlaySfx` is not admitted for the attack
and show-player resources simply because the generic entity handles completion.

The matrix factory route at RVA `0x00017da0` never installs that parent/listener.
Its exact custom ABI is EAX=force-ready flag, ECX=loop flag, then six stack
dwords: manager, matrix pointer, ResourceName by value (three dwords), scale.
It returns the factory-created SpecialEffect pointer and pops `0x18` bytes.
The adapter supplies force-ready=0, freshly witnessed native cache readiness,
and loop=1. This is the native general looping-effect path: it also sets the
effect's loop flag during finalization. Complete host-roster absence owns
retirement instead of relying on guessed durations. It does not call FireSoul,
install a player parent, or clear gameplay ownership bits.

The graphics event pump at RVA `0x0018abf0` calls registered listeners and
dispatches typed events. Its typed helpers also only visit listeners, except
type 23, which reaches global RVA `0x00037930` through `0x0018a8c0`.
That exceptional global route is not a sound-only route and remains excluded.
Replay checks exact SpecialEffect/component identity, a NULL effect parent,
an unsubscribed actor-forwarding listener (`effect+0x150 == 0`), and exactly
one known embedded sound listener before synchronization. The first LA23 live
test disproved the earlier zero-listener assumption: factory initialization
registers `effect+0x300`, owned by the sound component `effect+0x2e8`, with
the effect graphics source `effect+0x288`. Constructor RVA `0x000fbea0` sets
listener vtable `0x002cd0f4`; initializer `0x000fc0b0` installs both directions
of that subscription. Both exact owner identities, singleton counts, and
source/listener array entries must match. Missing or foreign listeners fail
closed; arbitrary nonzero listener counts are not admitted.

That listener's generic callback at vtable `+4` is RVA `0x0003b870`, a no-op
`ret 8`. Its active typed callbacks are the native sound paths at
`0x000fc100`/`0x000fc1b0` and local sound-volume setter `0x000fc1f0`; its other
typed slots are no-ops. Startup preflight verifies all fourteen event callback
targets. The admitted clip descriptors below contain no type 23. Thus
attackHit, effectTrigger, ObjectLeave, and showPlayer have no player/soul
listener to mutate; native embedded sound playback remains possible without
actor event forwarding. Complete audiovisual parity still needs live proof.

### Selected clip descriptor inventory

For each fixed HOM backing resource, the selected clip's lowercase-name hash
is followed by native duration at `+4` and event-channel count at `+0x0c`.
Descriptors begin at `+0x54`, each `{name_hash, type, parameter_count,
key_count}` followed by `parameter_count` eight-byte parameter descriptors.
This layout was checked against the binary descriptors, not just embedded
description text. RVA `0x00223180` independently reads the selected clip's
`+4` field as duration. These are native animation-time units, not seconds or
the last sound-event frame. The table describes the factory-selected clip zero.

| Resource number | Duration | Typed event channels |
| --- | ---: | --- |
| 250 initiate | 100 | 22 sound |
| 251 wait | 125 | 2 sound |
| 112 small floor | 110 | none |
| 800 transfer | 300 | 22 sound; 17 ObjectLeave |
| 252 morph | 90 | 22 sound |
| 801 link | 100 | none |
| 802 end | 60 | 22 sound; 7 showPlayer |
| 300 Tal strike one | 220 | 1 attackHit; 10 effectTrigger; 22 sound |
| 350 Tal strike two | 230 | 2 sound |
| 110 invulnerable loop | 100 | none |
| 111 return | 100 | none |
| 900 generic initiate | 180 | 22 sound |
| 351 Tal hit character | 180 | 2 sound |

Event name hashes: sound `afd310f9`, ObjectLeave `9e189bae`, showPlayer
`4cbfff50`, attackHit `e21c025d`, effectTrigger `fecefd8f`. The same sound-name
hash has type 2 or 22 in these assets; neither is the excluded type 23.
Resource 351's type-2 sound channel uses name hash `3ba2de27`.

SHA-256 of each full indexed HOM resource, for local evidence reproduction:

| Resource | SHA-256 |
| --- | --- |
| 250 | `2db5d97e73e3d871909287d8d6e6abb60455ad46e922094beb432f361e106136` |
| 251 | `fadd5da453f78bea8015e5085a74e0a8ffe403864cbda056c9e961a2b5c3511b` |
| 112 | `e8262d48aea0e9bda0cb9c72ddc08c50edc6e9e300b168665728db8908402842` |
| 800 | `a5214385411fc83b24eaab12f6bbe7fc582b561e014b1619d5052f159589e91e` |
| 252 | `26e1bc0d119930c6a00d1225e03d2e7831dc1bdf3c6e893177db51472e0863d4` |
| 801 | `80459686432f0f5a712aecb1781cf20b273cac998c43ddc995a3cf098e885098` |
| 802 | `3492c6d1b5a6118ee60a9a66378c4a162c6d48be4bc0d0f36f1082ed488e9ffe` |
| 300 | `2a2532aa9befc300a68364ad722e8b193f18889935e31fdc94deeedc01bab57d` |
| 350 | `5cac43a0c0b3e77bc9362db60369e55b744e8e1614b21e4ceb105dbf09757228` |
| 110 | `44530d1482124bab5bf4354a737120708c115cd48919b9197e51f7bf8ef101ce` |
| 111 | `2c3fdd3d257888a466b9ff638e5135113edfeae931854bdac3b1f6a65c144550` |
| 900 | `82bdcc5ed28e5e245ff0497ba5319e8af1f93f879bde4f91fcb77d2e37d0f955` |
| 351 | `4b317e5009b49a3a0e861d4d0ddaf4c25200b6eea4ac15bc90eb3fdabe8da99a` |

The live retail Tal charge uses `SFXSS900_generic_initate.HOM` (authored
spelling `initate`), backing identifier `62dcc5a3`, rather than the historical
sample-script opening 250. Its selected clip hash is `53da206f`; duration is
180 and its single descriptor is `{afd310f9, 22, 2, 5}`: sound, two parameter
descriptors, five keys. There is no type 23 or gameplay-event channel. The
charge animation definition references this effect with attach=false,
stop-on-animation-end=false, and loop=false. LA23 kind 12 `GENERIC_INITIATE`
replays the observed host lifetime through the same parent-free owned-clone
path, rather than guessing a duration from those authored flags. Fixed cache
tables and lifecycle loops use the protocol's named last-kind constant.
Focused tests verify the exact name/hash, load-before-spawn, at-most-once
spawn, removal/retirement, final cache release, and rejection above the bound.

The second Tal strike also emitted the exact typed extensionless request
`SFXSS351_Tal_Hit_Character` (identifier `6696ab0a`) during the live owned
Spirit interval. Its canonical fixed backing `SFXSS351_Tal_Hit_Character.HOM`
has identifier `aeec0c83`; clip hash `a7d282f0`, duration 180, and the sole
descriptor `{3ba2de27, 2, 2, 1}`. The single authored sound key is at native
frame 49. There is no type 23 or gameplay-event channel. Kind 13
`TAL_STRIKE_HIT` admits only this fixed resource under the existing isolated
sound-listener and host-roster lifetime rules. The new kind has the same
focused name/hash/cache/spawn/retire tests as 900; live client acceptance is
still required. Generic material-hit effects and status/boost resources are
not included by this bounded admission.

### Owned weak lifetime and pose

Native weak assignment RVA `0x00001750` takes EAX=stable 12-byte observer,
EDX=entity, and pops no stack arguments. It removes any old link and inserts
at entity `+4`. Passing NULL detaches and clears the node. Entity destruction
RVA `0x0013db00` tails into `0x00004d30`, which nulls all three fields of every
observer before freeing storage. The module is pinned before registering any
static node. Observer storage is never copied or moved while registered.
Exact head/backlink identity is checked; ambiguous post-spawn ownership
quarantines the backend, consumes the identity, and blocks dependent cleanup.

RVA `0x00131df0` is native queued retirement, stdcall one entity argument,
`ret 4`, not an immediate free. It is idempotent through retirement bit `0x08`.
The adapter calls it only for exact owned SpecialEffects. After entry, a failed
weak detach retries only detach, never another retirement call. Caches remain
owned until all effects/nodes are quiescent. A nulled node is consumed and never
respawned for that instance; UNKNOWN rosters are never interpreted as empty.

Owned root transforms use RVA `0x00110a40` (EAX=matrix, stack=CPosition,
`ret 4`) followed by stdcall RVA `0x00110f90` (CPosition, `ret 4`). Matrices use
D3DX row-vector XYZW quaternion convention, basis-row positive scale, and
translation entries 12–14. Exact entity/position/component ownership and the
kind-specific installed backing identifier are required before pose changes.
The render object's inline transform/version range is writable, and the
wrapper's model interface (`+0x0c`), animation interface (`+0x10`), and render
object's model backlink (`+0x14`) must agree for the exact concrete renderer.
Wrapper construction RVA `0x00123420`, concrete ANM query `0x0021ba40`, and
render binding `0x001d6460` establish this identity; renderer `+8` is instead
the backing model object. An unexpected render-driven scene-parent hierarchy
is rejected rather than entering its additional native update contract.
Renderer phase uses exact vtable RVA `0x002df8ec`, setter slot `+0x10c` /
RVA `0x00223180`, thiscall `(channel, submodel, time, force)`, `ret 0x10`.
The setter changes renderer state/dirty bits without dispatching authored
events. Channel zero and factory-selected clip zero are validated throughout;
native model/channel/clip pointers are bounded before use. Client and host use
the same 256-submodel upper bound, following native helper loops; that bound is
defensive, not a claim that any admitted HOM actually has 256 submodels.

Focused tests cover exact-image mutation/relocation, quaternion/scale mapping,
load-before-spawn, repeated/UNKNOWN rosters, weak invalidation, immutable
identity rejection, failed-retirement and failed-detach retries, no resurrected
old instances, native-null consumption, and reentrant service/reset rejection.
The native-shape fixture also covers the exact embedded singleton listener,
missing/extra/foreign listeners, wrong source backlinks, wrong owner/vtable,
and forbidden actor-parent/subscription state. The zero-listener live failure
blocked both phase synchronization and retirement. Subsequent live checks of
the corrected singleton contract are recorded below.

### Bounded live retirement follow-up

`CONFIRMED_LIVE`, experimental DXVK LAN host/client profile, dirty work based
on revision `53623a0`: after the singleton-listener correction, one observed
Tal strike visual (kind 8, first instance) logged one spawn and one retirement
entry, with no roster rejection or pending retirement. Non-pausing read-only
state inspection after the cast found every client visual slot and weak node
cleared, while the exact native entity-manager retirement queue count was
zero. This confirms native queue drainage and module-observer detachment for
that cast. It does not measure physical allocator reuse, establish all-kind
visual parity, or exercise disconnect during an active effect.

The retail-opening build subsequently completed variants one, two, one again:
each opening kind 12 and summon kind 8/9/8 spawned and retired once, with clear
client slots and empty native retirement queues afterward. The final secondary
effect build (`4f16a50502ae81cd32bcb5147f3ef4ba02e46bbf732d88e064ad6884c7119ac6`)
then completed variant two with opening 12, summon 9 and two character effects
13. All four client instances spawned and retired once without rejection or
pending cleanup. A six-second non-pausing post-cast sample found the host
registry, client owned slots and both native retirement queues empty. These are
bounded native lifecycle proofs, not user pixel/timing parity; active pose/phase
comparison and disconnect during playback remain unproved. Full run details
are in the owner-approved LA23 entry in `docs/research-log.md`.

### Retail opening forward-only phase correction

The owner's eight-orb reference concerns visible particles, not emitter count.
Read-only live sampling found 204 client backward phase steps during one
7.5-second opening, versus zero on the host, despite only one opening spawn.
The adapter's absolute correction was repeatedly undoing native progression.
`VisualPhaseCorrection` now skips equal or backward corrections only for the
finite retail opening kind 12; initial and later forward catch-up remain
allowed. Other kinds, including authored loops, preserve the prior absolute
host-phase behavior. Native channel selector/phase validation precedes writes;
complete host-roster absence still owns retirement. This is not a generic
network-clock, speed, pause or gameplay-authority change. Selected policy tests
pass; the research ledger records the bounded capture and subsequent live
acceptance status. A small lead over interpolated host phase is possible.

Corrected live sampling recorded zero backward steps on both peers (previous
client count 204). The owner confirmed the eight-orb flicker was resolved.
The client reached approximately 181.92 native units while the host ended near
180 before retirement, so this evidence is monotonic playback and accepted
visual quality, not identical phase. Opening/summon spawn and retirement logs
remained one per instance without rejection.
