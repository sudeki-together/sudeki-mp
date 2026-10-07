# Texture mods (`[TextureMods]`, `SudekiMP.Mod/1`)

Status: `IMPLEMENTED` / `EXPERIMENTAL`. Replacement is `CONFIRMED_LIVE` for the
Clean 4x Font example (2026-10-07, see Evidence). Other textures, TPF packs in
game, and teardown are not yet exercised live.

SudekiMP loads texture replacements from mod folders. It replaces the game's
textures as they are loaded, so no game file changes. Existing TexMod packages
(`.tpf`) convert without changes because the key is TexMod's.

## Package format

A mod is a folder under the game's `mods` folder:

```text
<Sudeki>\mods\CleanFont4x\
    mod.ini
    textures\Verdana_16-0.tga.dds
```

```ini
[Mod]
Format=SudekiMP.Mod/1
Name=Clean 4x Font
Version=1
Author=...
Description=...
Enabled=true

[Textures]
; 0xKEY = file, relative to this folder
0xAC57BC5D=textures/Verdana_16-0.tga.dds
```

- `Format` is required. A loader refuses any major version other than `1`.
  Later minor versions (`SudekiMP.Mod/1.x`) must stay readable by a `/1` loader.
- `Enabled=false` keeps the mod installed but inactive. Folders whose names
  start with `.` or `_` are skipped. `mods\_dump` is the dump output.
- **Key**: TexMod's texture hash. It is the CRC-32 of the original texture's
  level-0 pixel bytes as the game uploads them: reflected polynomial
  `0xEDB88320`, initial value `0xFFFFFFFF`, **no final inversion**.
  The byte count is `bits(format) * width * height / 8` (DXT1 is 4 bits,
  DXT3/5 is 8). Keys are written `0x` plus up to 8 hex digits.
- **File**: a relative path. Absolute paths, drives, empty components, `.`,
  `..` and `:` are refused. Any image the game's own `d3dx9_30` loader reads
  works: DDS, PNG, TGA, BMP or JPG. Size and format come from the file, and a
  full mip chain is built when the file has fewer levels. Prefer DDS: it
  decodes fastest and does not depend on Wine's WIC codecs.
- The manifest is ASCII, or UTF-16LE with a BOM when it holds other characters
  (`GetPrivateProfile*W` reads both).
- Mods load in case-insensitive folder-name order. A later mod overrides an
  earlier one for the same key, and the override is counted in the install
  log line.

Keys identify content, not names. Textures with identical pixels share a key
and one replacement. The catalog finds 117 such shared keys among the 5240
TGA/SQX textures of the supported build, for example `Verdana_18-0.tga` and
an unnamed copy in `Fonts.baf`.

## Runtime (`src/hooks/texture_mods.c`)

```ini
[TextureMods]
Enable=true        ; default true; nothing installs without an enabled texture
Folder=mods        ; relative to the game folder, or absolute
Dump=false         ; write every decoded texture once to <Folder>\_dump\0xKEY.dds
```

The adapter installs for every launch profile, right after `SudekiMP.ini` is
resolved. It installs nothing unless an enabled mod lists a texture or
`Dump=true`, and a refusal there is logged but is not fatal. It rolls back on
every init failure path that rolls back the accelerator cache.

Seam (`CONFIRMED_STATIC`, Ghidra on the supported image):

| RVA | Role |
| --- | --- |
| `0x1D8300` | Texture decode dispatcher. ESI = 0x3C-byte texture record, one stack argument, `RET 4`, AL = success. It selects the codec by the low two bits of the texture descriptor: type 0 `0x1F54D0` (DXT size math, SQX by `INFERENCE`), type 1 `0x1F5B50` (TGA; the only user of `"%s.tga"` / `"%s.%d.tga"`), type 2 `0x1F4880` (`UNKNOWN` format). |
| `0x1F3780` | The single `IDirect3DDevice9::CreateTexture` wrapper (device vtable `+0x5C`). |
| `0x1F3400` | Stores the created texture at `record+4`, the format at `+8`, the level count at `+0x2A`, and level-0 width/height at `+0x34/+0x36`. |
| `0x1F3AE0` | Textures larger than the device limit become a tile set at `record+4`. |
| `0x3C31DC` | `IDirect3DDevice9*` global. |

The adapter owns the dispatcher entry (`81 4E 20 00 01 00 00`, replayed in the
trampoline). After a successful decode it:

1. checks that `record+4` has the concrete `IDirect3DTexture9` vtable, learned
   once from a 1×1 managed texture on the same device. Tile sets, cube
   textures and stale pointers are skipped, never called;
2. locks level 0 read-only and computes the key over the contiguous bytes, as
   TexMod does;
3. on a match, reads the file, builds the replacement with
   `D3DXCreateTextureFromFileInMemoryEx` (managed pool, size and format from
   the file, full mip chain), stores it at `record+4` and releases the
   original.

The record keeps the original format, level count and dimensions, so code that
computes UVs from `+0x34/+0x36` (for example the font glyph table) is
unaffected by a larger replacement. If some path reads dimensions from the D3D
texture instead, an upscaled replacement would sample the wrong region there:
`UNKNOWN`, to be checked live.

Threading: the observer runs on the decoding thread, which is the same thread
that just created and filled the texture through the same device. A reload of
a record that another thread is already drawing is a theoretical race shared
with the native store (`INFERENCE`).

Teardown restores the entry bytes. The index, mod paths and trampoline stay
allocated because a decode thread may still be inside the stub. Replacements
belong to the game once swapped and are released by its normal texture
release.

Multiplayer: purely local and visual. Peers do not need the same mods.

Address space: the game is 32-bit. A 2048² A8R8G8B8 sheet with mips is about
22 MB in the managed pool, so very large packs can exhaust memory.

## Tools

`tools/sudekimod.py` needs only the Python standard library; `--dds` needs
Pillow.

| Command | Purpose |
| --- | --- |
| `convert-tpf MOD.tpf OUT [--game DIR \| --catalog keys.tsv] [--dds]` | TexMod package → mod folder. Removes the TPF XOR layer (`0x3FA43FA4`), decrypts the ZipCrypto archive with TexMod's fixed password, reads `texmod.def` (`0xKEY\|file`), and falls back to walking local headers when the central directory is damaged. The zip comment becomes Author/Description. With a catalog, files are named after their archive resource. |
| `catalog --game DIR [--out keys.tsv]` | Lists every TGA (32-bit, uncompressed) and SQX (DXT1/3/5) texture in the game's `.baf` archives with its key and, where harvested, its resource name. Read-only, about one minute. |
| `build SRC OUT --game DIR [--dds]` | Builds a mod from images named after the texture they replace: `NAME.EXT.<img>` (for example `Verdana_16-0.tga.png`) or `0xKEY.<img>`. |
| `validate MOD` | Checks the manifest, keys, paths and image headers. |

Catalog keys for SQX assume the game uploads level 0 unchanged (DXT is
uploaded compressed): `INFERENCE` until `Dump=true` confirms a known SQX key.
On the supported build 4699 of 5240 textures get a unique name. The rest are
unreferenced by any harvested string.

## Example mod: Clean 4x Font

`tools/make_clean_font_mod.py --game DIR --font Verdana.ttf --out mods/CleanFont4x`
redraws every `Fonts.baf` glyph sheet at 4× (512² → 2048²) and writes a ready
mod folder. It needs numpy and Pillow, plus a font you are licensed to use. It
does not ship with the project because the output derives from the game's
glyph layout and from the supplied font.

- `Fonts.baf` holds two glyph descriptors (`CONFIRMED_STATIC`; both parse to
  exactly their resource size). Each is `Verdana` with lfHeight −21 (16 pt) or
  −24 (18 pt): a LOGFONT-like header, kerning pairs, then 14-byte glyph
  records `{char, page, x, y, w, h, 0, a, advance, c}`. The pen advance is
  `a + w + c`.
- Sheets are matched to descriptors by the share of their alpha inside the
  glyph cells (≥ 99.5%). Five sheets match: `Verdana_16-0.tga`,
  `Verdana_18-0.tga`, and three unnamed sheets, one of which is identical to
  18-0. That gives four keys.
- The original style is copied exactly: flat `0xEA` fill, the glyph swept one
  pixel right and down in `0xC2`, and a `0x20` outline two pixels wide and one
  pixel tall. Each glyph is rendered at 4× and placed by minimising the error
  against the original body in its cell. Glyphs the font lacks (`∩`, `≡` in
  the Verdana tested) keep the original cell, smoothly enlarged.
- `--preview out.png` draws sample text through the game's glyph table from
  both sheets with bilinear sampling, as the GPU scales the HUD.

## Evidence

- `CONFIRMED_STATIC`: dispatcher, codec selection, CreateTexture wrapper,
  record fields and device global (Ghidra, supported image). The entry bytes
  were checked against the supported executable (SHA-256 `8ceb1d3c…`).
- `CONFIRMED_STATIC` (offline): the community TPF `SymphonyUI_1.tpf` (2026-10-06)
  holds one 2048² PNG keyed `0xAC57BC5D`. That equals the key above for
  `Fonts.baf` `Verdana_16-0.tga` in top-down order. The file is bottom-up;
  raw order gives `0x2B58CD46`, and the standard CRC-32 with final inversion
  matches no sheet. This fixes the key definition and shows the TGA codec
  uploads A8R8G8B8 rows top-down.
- `CONFIRMED_TEST`: `SudekiMP.TextureModIndexTest` (CRC vector, key/path/line
  parsing, override order; host and Wine) and `tests/sudekimod_test.py`
  (synthetic encrypted TPF round trip, central-directory fallback, manifest
  safety, DDS writer, descriptor parser).
- `CONFIRMED_LIVE` (2026-10-07, single player, `[TitleMenu] Enabled=true Scope=entry`,
  dirty tree on `codex/shared-simulation`, DLL `5e20a595…`): two instances were
  run side by side with the same save area, one with the Clean 4x Font mod and
  one with `[TextureMods] Enable=false`. The log recorded
  `event=replace key=0xAC57BC5D` (Verdana_16-0) and `key=0x78534B58`
  (Verdana_18-0), both 512² → 2048² with 12 levels. Owner screenshots of the
  same Martin Finchey subtitle show identical layout, wrapping and highlight
  colours with smoother glyph edges and no garbling. So the font path takes UVs
  from the record dimensions and a larger replacement is safe there.
  `Dump=true` wrote 607 textures, and 400 of their keys match the offline
  catalog, SQX DXT included. That confirms the catalog's SQX key rule.
- Log caveat: with `[TitleMenu] Enabled=false` the loader closes the log after
  startup, so replace/dump lines from the decode thread are lost even though
  replacement and dump still happen.
- Not yet exercised: a converted TPF in game, non-font replacements, teardown
  or uninstall, and very large packs.
