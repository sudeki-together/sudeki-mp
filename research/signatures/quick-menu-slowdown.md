# Quick Menu slowdown signatures

Executable SHA256: `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`

These patterns are documentation and future runtime-hook anchors. They are not patches.

## Activation request

- Function RVA: `0x00098EC0`
- Signature RVA: `0x00098EE5`
- File offset: `0x000982E5`
- Key instruction RVA: `0x00098EF0`
- Exact bytes (one match in this executable):

```text
A1 A0 8D 80 00 8B 35 1C 8D 80 00 C7 40 24 01 00 00 00
```

- Relocation-tolerant candidate:

```text
A1 ?? ?? ?? ?? 8B 35 ?? ?? ?? ?? C7 40 24 01 00 00 00
```

The key instruction writes requested speed mode `1` to `[CGameSpeed+0x24]`.

## Deactivation request

- Function RVA: `0x00099180`
- Signature RVA: `0x000991AB`
- File offset: `0x000985AB`
- Key instruction RVA: `0x000991B6`
- Exact bytes (one match in this executable):

```text
A1 A0 8D 80 00 8B 2D 88 2F 7C 00 89 58 24
```

- Relocation-tolerant candidate:

```text
A1 ?? ?? ?? ?? 8B 2D ?? ?? ?? ?? 89 58 24
```

The key instruction writes requested speed mode `0` to `[CGameSpeed+0x24]`; `ebx` was cleared earlier in the method.

## Validation notes

- Exact-pattern match counts were measured against the vanilla executable with a binary-safe search: one activation match and one deactivation match.
- Absolute addresses in the exact forms are subject to PE relocation at runtime. A scanner should operate on the loaded module's `.text` section and use the wildcard forms.
- During the 2026-08-12 Wine trace, `d3dx9_30.dll` occupied the preferred `0x00400000` base and Sudeki loaded at `0x79CC0000`. Adding RVAs produced runtime activation VA `0x79D58EF0` and close VA `0x79D591B6`.
- Before using either candidate in code, verify the decoded target operand is `+0x24` and the surrounding control flow matches the documented function.
