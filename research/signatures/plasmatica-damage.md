# Plasmatica native damage signatures

These contexts apply only to Sudeki executable SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`, image base `0x00400000`.

They document the confirmed native collision-to-HP path. They are not patches and are not yet used by the runtime mod.

## `Possible_ApplyCollisionDamage`

- RVA / VA: `0x00138870` / `0x00538870`
- Exact context, unique in this executable:

```text
83 EC 78 53 55 8B AC 24 84 00 00 00 D9 45 54 56 D9 EE 57 DA E9 DF E0 F6 C4 44
```

## `Possible_DispatchDamageStructure`

- RVA / VA: `0x000DAB50` / `0x004DAB50`
- Exact context, unique in this executable:

```text
56 8B F0 E8 C8 B5 0A 00 8B 4F 50 F6 C1 02 76 36 8B 57 58 83 E2 0F 80 FA 03
```

Relocation-tolerant form for the relative call:

```text
56 8B F0 E8 ?? ?? ?? ?? 8B 4F 50 F6 C1 02 76 36 8B 57 58 83 E2 0F 80 FA 03
```

## `Possible_ApplyDamageToCharacter`

- RVA / VA: `0x000D21D0` / `0x004D21D0`
- Exact context, unique in this executable:

```text
55 8B EC 83 E4 F8 81 EC CC 00 00 00 53 56 57 8B 7D 08 C7 44 24 2C 00 00 00 00
```

## Confirmed chain

```text
collision geometry bridge (RVA 0x00032A80)
  -> CCollisionDamage callback (RVA 0x00138870)
  -> DamageStructure dispatcher (RVA 0x000DAB50)
  -> target combat damage application (RVA 0x000D21D0)
  -> current HP store at combat-data +0x2C
```
