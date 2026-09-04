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

## VFX status

The soul launcher at preferred VA `0x0052b0b0` creates an effect wrapper through
preferred VA `0x00417da0`, but both the selected resource ID and the behavior of
the spawned resource remain dynamic. Absence of a direct damage call in the
launcher is not proof that the resource is presentation-only. VFX replication
therefore remains deferred until a live host trace captures the exact resource
IDs and read-only analysis proves that those resources have no collision,
damage, script, camera, time, or lifecycle side effects.

## Required test seams

- Event journal: wraparound, bounded history, repeated cues, stale/duplicate
  rejection, wrong-skill rejection, malformed cue IDs, and reset/disconnect.
- Client adapter: injected `GetSound`/`PlayCue` function table, exact enum-to-
  constant mapping, one-shot delivery, and fail-closed behavior when the sound
  singleton or exact-image verification is unavailable.
- Exact image: verify the function RVAs and prologue bytes above, including the
  relocated `GetSound` singleton operand, before any hook or replay call;
  exercise complete install rollback and uninstall.
- Safety: keep a static denylist test showing that the presentation adapter has
  no call edge to the Spirit manager, stage dispatcher, native attack path,
  game-speed ownership, camera ownership, or damage APIs.
