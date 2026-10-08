# Mod packages (`[Mods]`, `SudekiMP.Mod/1`)

Status: `IMPLEMENTED` / `EXPERIMENTAL`.

- `[Textures]` replacement is `CONFIRMED_LIVE` for the Clean 4x Font example
  and for a converted TexMod package (2026-10-07).
- A converted weapon pack is `CONFIRMED_LIVE` (2026-10-07, owner-observed):
  the COOLER Runic Blade TexMod package, installed as a mod folder through the
  launcher on a throwaway game copy, recoloured Tal's Runic Blade in the Talos
  fight. The log recorded 4 of its 5 keys replaced (`0x4383377F`,
  `0x4FADDA1E`, `0x8A57026F`, `0xE4E3CC34` = `W005_MrChoppy_env.SQX`); the
  fifth (`0x2C287194`, a large side-view icon) was not loaded in that session.
  Three of the replaced originals decode as 32-bit `A8R8G8B8` textures that
  the offline catalogue (`sudekimod.py catalog`, the launcher scan) does not
  list, so keys can match in game without appearing in the catalogue.
- `[Files]` replace and add are `CONFIRMED_LIVE` (2026-10-07): tinted font
  sheets were served from the mod file, see Evidence.
- Not yet exercised live: teardown, and a model or animation `[Files]` entry.

SudekiMP loads mods from folders, and no game file changes:

- `[Textures]` replaces textures as they are decoded, keyed by TexMod's hash,
  so existing TexMod packages (`.tpf`) convert without changes.
- `[Files]` replaces or adds whole archive resources: models (`.HOM`),
  animation banks (`.ANI`), SOL definitions, textures (`.SQX`/`.TGA`), font
  descriptors, zones and collision, that is anything inside a mounted `.baf`.

`[Files]` names `sound/<file>` and `movies/<file>` override loose game-folder
files: XACT wave banks (`.xwb`), sound banks (`.xsb`), speech
(`sound/Speech/...`) and Bink movies (`.bik`). These sit beside the archives
rather than inside them.

## Launcher editor

The graphical launcher's **Mods** tab (the Mod workshop) shows the player's
own archive textures by character and category, replaces a texture by drag
and drop or Browse (Apply/Revert), exports originals as PNG, and manages
packages: one **Load this mod** switch per package, ▲/▼ load order (written
to `mods\load-order.txt`), **Import…** for a zipped mod or a TexMod `.tpf`
(also by dropping the file on the tab) and **Export…** to a zip. Changes take
effect on the next game launch. Players do not need Python. See
[Launcher Mods tab](launcher-mods.md) for usage, grouping limits and the
verification record. Model Apply stays disabled until a model `[Files]`
replacement is confirmed in game.

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

[Files]
; NAME.EXT (as stored in the archive) or 0xARCHIVEKEY = file
TAL.HOM=files/TAL.HOM
MY_NEW_PROP.SQX=files/MY_NEW_PROP.SQX
sound/BS_brightwater.xwb=files/sound/BS_brightwater.xwb
movies/Publisher.bik=files/movies/Publisher.bik
```

- `Format` is required. A loader refuses any major version other than `1`.
  Later minor versions (`SudekiMP.Mod/1.x`) must stay readable by a `/1` loader.
- `AddToArchive=` (in `[Mod]`, default `SOLData.baf`) names the archive that
  receives `[Files]` names no mounted archive holds.
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
  earlier one for the same key or name, and texture overrides are counted in
  the install log line.
- `[Files]` names are `NAME.EXT` in ASCII letters, digits, `_` and `-`, with
  one dot and at most 120 characters. Matching is case-insensitive, because
  the archive key is the native checksum of the upper-cased name. A `0xKEY`
  form addresses a resource whose name is unknown.

Keys identify content, not names. Textures with identical pixels share a key
and one replacement. The catalog finds 117 such shared keys among the 5240
TGA/SQX textures of the supported build, for example `Verdana_18-0.tga` and
an unnamed copy in `Fonts.baf`.

## Runtime: loader and textures (`src/hooks/texture_mods.c`)

```ini
[Mods]
Enable=true          ; default true; nothing installs without an entry
Folder=mods          ; relative to the game folder, or absolute
DumpTextures=false   ; write every decoded texture once to <Folder>\_dump\0xKEY.dds
```

**Load order.** Every folder under `Folder` with a `mod.ini` loads; a later
mod wins when two replace the same key or file. By default the order is the
folder names (case-insensitive). An optional `mods\load-order.txt` overrides
it: one folder name per line, earliest first (UTF-8, `#`/`;` comments);
listed folders load first in that order, the rest after them by name. The log
records `texture_mods event=load_order listed=<n> total=<n>`
(`CONFIRMED_LIVE` 2026-10-07: a two-line file reversed the name order).

**Log.** While any mod is loaded (or `DumpTextures=true`), or the Main Menu
armour choice is installed, `SudekiMP.log` stays open after initialization in
every profile, so replacements and redirects during play are recorded (before
2026-10-07 it closed in profiles without a research feature, such as Safe
launch).

The loader runs for every launch profile, right after `SudekiMP.ini` is
resolved and before any archive mounts. Each seam installs only when it has
an entry (or `DumpTextures=true` for the texture seam). A refusal is logged but
is not fatal. It rolls back on
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

## Runtime: archive files (`src/hooks/archive_mods.c`)

All addresses below are `CONFIRMED_STATIC` (Ghidra on the supported image).

- **Mount:** `0x1BD400` parses an archive index with the C runtime (`0x1BDE70`:
  256 buckets of 12-byte `{offset, size, key}` rows, allocated with `new[]`
  `0x24884E`), then opens the read handle with `CALL [CreateFileA]` at
  `0x1BD45C`. At that call `EBP` is the `XBafFileSystem` object, whose index
  is complete and not yet registered with the manager. The destructor
  `0x1BD390` frees the rows with `delete[]` `0x248916`.
- **Reads:** opening a resource (`0x1BD560` by key, `0x1BD4E0` by name) copies
  the row into an `XBafFileLoader`: `+8` size, `+0xC` base, `+0x14` the archive
  handle, `+0x20` position. Every read is `0x1BD290`, which calls
  `SetFilePointer(handle, position, &0, FILE_BEGIN)` and then `ReadFile` on
  the archive handle while holding the archive lock. No other code reads
  archive handles.

The adapter owns the 6-byte mount call and the `SetFilePointer`/`ReadFile`
import slots (`0x29A0B0`, `0x29A0DC`):

1. After the native open, each listed name present in that archive gets its
   row pointed at a virtual offset (≥ `0x80000000`, 2 KiB aligned) with the mod
   file's size. A name no archive holds is inserted, in key order, into the
   `AddToArchive` archive. The game's own `new[]` grows the bucket and
   `delete[]` frees the old one, so the destructor stays valid.
2. `SetFilePointer` on a patched archive's handle into the virtual range
   records the position instead of seeking. The next `ReadFile` on that handle
   and thread is served from the mod file with a positional read, zero-filled
   past its end. Every other handle and read passes straight through.

Nothing inside the code ranges that `lan_story_resource_file.c` hashes is
changed. The `CreateFileA` slot that `lan_story_load.c` checks is untouched,
because the mount call is patched rather than the slot. Rows keep their
sorted, per-bucket invariants, so its row validation still holds.

- **Lifetime:** once any archive is patched the read hooks are pinned, because
  index rows and open loaders hold virtual offsets. Uninstall then restores
  only the mount call and reports `uninstall_refused`.
- **Multiplayer:** `[Files]` changes game data and can change simulation.
  Host and clients must install the same `[Files]` mods; nothing checks this
  yet (`UNKNOWN` policy, owner decision). `[ResourceSwap]` runs on top: it
  maps a name to another key before the lookup, so a swap can target an
  added resource.

## Runtime: loose sound and movie files (`src/hooks/loose_mods.c`)

All addresses are `CONFIRMED_STATIC`; the roles are `CONFIRMED_LIVE` from the
open log of 2026-10-07. The game builds each path with `sprintf` and opens it
directly. The adapter owns six `CALL [import]` sites (6 bytes each), where the
first stack argument is the path:

| RVA | Opens |
| --- | --- |
| `0x28870B` | Streamed speech banks, `sound\Speech\*.xwb` (`CreateFileA`, overlapped) |
| `0x28878C` | Streamed wave banks, `sound\*.xwb` |
| `0x2890DE` | In-memory wave banks and `.xsb` sound banks |
| `0x256C49` | C runtime open (under `fopen`) |
| `0x1BF383`, `0x1BF3C9` | Movie header check (`CreateFileA`), then `BinkOpen` on the same `%smovies\%s.bik` buffer |

At each site the stub takes the `sound\...` or `movies\...` tail of the
path. If a mod overrides it, the stub swaps the argument for the mod file's
ANSI path, then calls through the same import slot. Every other path passes
through unchanged, and the first 64 non-overridden opens are logged, which is
a handy list of what the game loads.

The `CreateFileA` import slot itself is untouched (`lan_story_load.c` checks
it). The startup probe at `0x28D205`, which checks whether `ClimaxLogo.bik`
exists, is left alone. Mod paths must survive the ANSI code page, or the
entry is refused. Sound and movie mods are local presentation, but a bank
whose cues differ from the original can make the game miss sounds.

## Tools

`tools/sudekimod.py` needs only the Python standard library; `--dds` needs
Pillow.

| Command | Purpose |
| --- | --- |
| `convert-tpf MOD.tpf OUT [--game DIR \| --catalog keys.tsv] [--dds]` | TexMod package → mod folder. Removes the TPF XOR layer (`0x3FA43FA4`), decrypts the ZipCrypto archive with TexMod's fixed password, reads `texmod.def` (`0xKEY\|file`), and falls back to walking local headers when the central directory is damaged. The zip comment becomes Author/Description. With a catalog, files are named after their archive resource. |
| `catalog --game DIR [--out keys.tsv]` | Lists every TGA (32-bit, uncompressed) and SQX (DXT1/3/5) texture in the game's `.baf` archives with its key and, where harvested, its resource name. Read-only, about one minute. |
| `build SRC OUT --game DIR [--dds]` | Builds a mod from images named after the texture they replace: `NAME.EXT.<img>` (for example `Verdana_16-0.tga.png`) or `0xKEY.<img>`. |
| `extract NAME.EXT OUT --game DIR` | Copies one archive resource out (read-only), as a starting point for a `[Files]` edit. |
| `add-file MOD NAME.EXT FILE` | Copies a file into `MOD/files/` and adds or replaces its `[Files]` line, creating `mod.ini` if needed. |
| `validate MOD` | Checks the manifest, texture keys, `[Files]` names, paths and image headers. |

Catalog keys for SQX assume the game uploads level 0 unchanged (DXT is
uploaded compressed): `INFERENCE` until `DumpTextures=true` confirms a known SQX key.
On the supported build 4699 of 5240 textures get a unique name. The rest are
unreferenced by any harvested string.

## Example mod: rainbow static intro

`tools/make_static_bink.py out.bik` writes an original Bink 1 (`BIKi`) video
of rainbow static (default 1920×1080, 30 fps, 4 s, about 59 MB). It is a minimal
intra-only writer: identity Huffman trees, two-colour pattern blocks, no audio.
It follows the public FFmpeg Bink demuxer and decoder, and each frame starts
with the byte offset of its first chroma plane, as the original movies do.

- FFmpeg decodes known content pixel-exactly on all three planes (`CONFIRMED_TEST`).
- The game's own `binkw32.dll`, driven by a small harness under Wine, opens it
  (120 frames, 1920×1080, 30/1) and decodes it within 2 levels of FFmpeg's RGB
  (`CONFIRMED_TEST`).
- It plays in game as the startup movies (`CONFIRMED_LIVE`, 2026-10-07).
- This writer only makes pattern-block video. For real footage, see "Your own
  movies" below.

One file can stand in for every startup movie:

```ini
[Files]
movies/Publisher.bik=files/movies/rainbow_static.bik
movies/ClimaxLogo.bik=files/movies/rainbow_static.bik
movies/TWIMTBP.bik=files/movies/rainbow_static.bik
```

The startup sequence (`0x68DEE0`) calls `PlayMovie` (`0x504D90`) for
`Publisher`, `ClimaxLogo` and `TWIMTBP` in turn. Each call blocks until its
movie ends or is skipped. The supported install ships only `ClimaxLogo.bik`;
the other two slots are skipped unless a mod supplies them. A window that has
not been activated waits inside `PlayMovie` (a `Sleep(500)` loop) and shows
white until it is clicked.

## Your own movies (MP4 and other video)

SudekiMP does not convert real video to Bink. Use RAD Game Tools' free **RAD
Video Tools**, which you download yourself from RAD's website; the project
does not redistribute it. It is a Windows program, and on Linux it runs under
Wine.

1. Convert your video to **Bink 1** (`.bik`), not Bink 2: the game's
   `binkw32.dll` only plays Bink 1. Match the original movies where you can:
   1920×1080 at 30 fps. Include audio if you want sound.
2. Add it to a mod under the movie name it replaces, for example the first
   startup logo:

   ```sh
   python3 tools/sudekimod.py add-file mods/MyIntro movies/Publisher.bik my_intro.bik
   ```

   Startup slots are `Publisher.bik`, `ClimaxLogo.bik` and `TWIMTBP.bik`. Story
   cutscenes are the `FMA*.bik` files in `movies/`, and the opening poem is
   `FMA01_poem.bik`.
3. Check the result with `ffprobe my_intro.bik`; it should report
   `binkvideo (BIKi)` or another Bink 1 revision. Then launch, and click the
   game window so the movie advances.

`UNKNOWN`: SudekiMP has only tested its own generated video in game, not a
RAD-converted one, a resolution other than 1920×1080, or a movie with an audio
track.

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
  one with mods disabled (`Enable=false`). The log recorded
  `event=replace key=0xAC57BC5D` (Verdana_16-0) and `key=0x78534B58`
  (Verdana_18-0), both 512² → 2048² with 12 levels. Owner screenshots of the
  same Martin Finchey subtitle show identical layout, wrapping and highlight
  colours with smoother glyph edges and no garbling. So the font path takes UVs
  from the record dimensions and a larger replacement is safe there.
  `Dump=true` (now `DumpTextures`) wrote 607 textures, and 400 of their keys match the offline
  catalog, SQX DXT included. That confirms the catalog's SQX key rule.
- Log caveat: with `[TitleMenu] Enabled=false` the loader closes the log after
  startup, so replace/dump lines from the decode thread are lost even though
  replacement and dump still happen.
- `CONFIRMED_LIVE` (2026-10-07, same profile): the converted
  `SymphonyUI_1.tpf` (PNG, not re-encoded) logged `event=replace
  key=0xAC57BC5D … to=2048x2048/0x33`. D3DX chose A8L8 for the greyscale
  sheet.
- `CONFIRMED_LIVE` (2026-10-07, `[Files]` test, branch `codex/texture-mods`):
  the log shows `archive_mods event=replace name=Verdana_16-0.tga
  archive=Fonts.baf` and `event=add name=MODTEST_18.TGA archive=Fonts.baf`
  (red- and green-tinted copies; `[ResourceSwap] Verdana_18-0.tga=MODTEST_18.TGA`)
  at mount, and no fault. The owner confirmed the tinted subtitles in game, so
  reads are served from the mod file for both a replaced and an added resource.
- `CONFIRMED_LIVE` (log, 2026-10-07, branch `codex/texture-mods`): with
  `SkipStartupMovies=false`, `movies\Publisher.bik` was redirected at both
  `movie_check` and `bink_open`. Every sound and speech bank the game loaded
  went through the owned sites.
- `CONFIRMED_LIVE` (2026-10-07, owner): the Rainbow Static Intro mod played the
  generated `rainbow_static.bik` in place of the startup movies after the
  window was activated. The silenced-music check is still pending.
- Not yet exercised: non-font replacements, model or animation files,
  teardown or uninstall, very large packs, and LAN with identical mods.
