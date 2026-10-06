# Archive resource redirection (`[ResourceSwap]`)

Status: `CONFIRMED_LIVE` for redirection and for the `TalosOnTal` profile's
rendering (2026-10-06); locomotion and attacks on a swapped body are open.

## What it does

Every mounted `.Baf` archive is an `XBafFileSystem` whose index holds 256
buckets of 12-byte entries `{offset, size, key}`, where
`key = checksum("NAME.EXT")` (upper-case ASCII; alternate add on even and
multiply on odd character positions, modulo 2^32). The same checksum of the
bare name (no `:NN` type suffix) is what SOL definitions store as
`<field>ResourceHash`; the `:NN` suffix is the resource type (41 model,
42 texture, 43 animation script, 44 class, 49 collision, 64 animation).

Two native lookups exist, each with exactly one caller (CONFIRMED_STATIC):

| RVA | VA | Contract |
| --- | --- | --- |
| `0x1BE100` | `0x5BE100` | find-by-key: EAX = archive, `[ESP+4]` = key, `RET 4`; in-bucket binary search |
| `0x1BDFA0` | `0x5BDFA0` | find-by-name: EAX = archive, `[ESP+4]` = `char*` (a `#hex` literal is a key); hashes through the archive's hash pointer at `+0x80C` then searches |

`src/hooks/resource_swap.c` owns both entries. Before the native search it
rewrites a listed key to its target key, or a listed name pointer to a
mod-owned replacement string. No data file changes; a target missing from
the archive fails the lookup exactly like a missing resource.

## Why

Hex-editing names inside SOL definitions fails for longer names: strings are
length-prefixed inside size-carrying blocks, and the game loads by hash
anyway. Redirecting at the archive lookup has no length limit and works for
every resource type.

## Configuration

```ini
[ResourceSwap]
TAL.HOM=TALOS.HOM
PC_TAL.ANI=PC_TALOS.ANI
```

Names are `NAME.EXT` as stored in the archive (case-insensitive). Host and
clients must carry the same table for consistent visuals. A model swap for a
character normally needs the matching animation bank, class file, collision
and sound script as well; a mismatched skeleton renders garbage.

## Profiles

`[ResourceSwap] Profile=TalosOnTal` applies a built-in set, overridable by a
`[ResourceSwap.TalosOnTal]` section and by plain `[ResourceSwap]` lines:

| Source | Target | Role |
| --- | --- | --- |
| `TAL.HOM` | `TALOS.HOM` | skeleton/effect container (the HOM named in the SOL) |
| `TAL_TAL_LORES_ARMOUR{A,2,3}.HOM` | `TALOS_TALOS.HOM` | body meshes, one per equipped armour |
| `PC_TAL.ANI` | `BOSS_TALOS.ANI` | animation bank; the player bank `PC_TALOS.ANI` has no walk/run clips |

## Evidence

- `CONFIRMED_STATIC`: lookup functions, index layout, hash algorithm
  (Ghidra on the supported image; forge cross-check of SOL hashes and
  archive keys for `TALOS`, `SFXBW041_RIVER`).
- `CONFIRMED_LIVE` (2026-10-06, Ailish host / Tal client, New Brightwater,
  stages mp52–mp56): redirects fire on both sides; with the `TalosOnTal` set
  Tal renders as Talos in both windows and frames flow. Swapping only the
  skeleton leaves the slot invisible (the armour body cannot bind); swapping
  the skeleton with Tal's own bank hangs the load until the runtime exits to
  the menu (mp57). The swapped slot can rotate and idle but not walk, run or
  attack: its idle sits on bank index 52 while the mod's Tal combo table
  treats 52 as a strong attack, and the host movement path uses Tal-specific
  selector tables (open item, see the plan ladder).
- The find-by-key function and the open-by-key method are exact-image
  verified by `lan_story_resource_file.c`; hooking them makes save
  preparation refuse with error 193 ("every player needs a matching local
  copy"). The hash-function seam avoids that.
- A swapped body without the authored hand locator reports no weapon
  attachment; the host publishes the weapon as hidden instead of refusing the
  frame (`lan_story_snapshot.c`).
