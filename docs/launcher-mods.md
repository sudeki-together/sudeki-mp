# Launcher Mods tab

Status: `PARTIAL` / `EXPERIMENTAL`. The texture workflow is implemented and
`CONFIRMED_TEST` on synthetic fixtures. A read-only scan of the supported install also passed under Wine. Native
Windows acceptance and launcher-to-game texture Apply/Revert are `UNKNOWN`. Existing
loader live evidence in [mod-packages.md](mod-packages.md) does not establish
live proof for this new UI.

## Using the tab

Choose the game folder on **Play**, then open **Mods**. The Mods tab is the
full-width *Mod workshop*; the SudekiMP options that used to sit on this tab
(skip startup movies, Quick Menu speed, story test boost) are on **Tools**.

The selector lists folders containing `mod.ini` in case-insensitive folder
order, including disabled packages, and shows each position: later folders
override earlier ones. **New mod…** creates a named package; when the mod
root is empty, the initial package is **My Mods**. The configured
`[Mods] Folder` remains the existing default `mods`; My Mods is a package
inside it. **Load this mod** saves `[Mod] Enabled` in the selected package.
There is no global switch in the tab: `[Mods] Enable` keeps its default
(`true`), and when `SudekiMP.ini` turns it off the workshop says so.
**Open folder** opens the selected package, or the mods folder (created if
missing) when none is selected.

The mod list is re-read whenever the Mods tab is shown and whenever the
launcher window is reactivated (for example after copying a mod in from
Explorer); **Rescan game** is only needed for the game archives. A mod copied
into another mod's folder (one level too deep) is never loaded by the game;
the workshop says which folder it is in. The search box matches resource
names and also texture or archive keys in hex (`4FADDA1E`), as used by
converted TexMod packages.

Select a character and category, then a thumbnail. Search filters resource
names within that selection. Original metadata identifies the archive,
resource name (or an unnamed archive key), texture CRC, size and pixel format.
Unknown and unmatched textures remain visible under **World / Other → Other
textures**. Known `.HOM` resources appear under **Models (beta)**.

Drag one PNG, DDS, TGA, BMP or JPG onto the drop zone, or use **Browse…**.
Browse is available because desktop drag-and-drop delivery varies under Wine.
Size/aspect changes produce a warning and remain allowed. Invalid or
unreadable input disables Apply. WIC provides PNG/JPG/BMP previews; when its
codec is unavailable, a structurally valid image may still be applied without
a preview. This fallback is less comprehensive than a successful codec decode.

**Apply** copies the image unchanged into the selected mod and saves its
texture-key entry. It does not launch the game. Generated payload names keep
the resource name where known and add a unique suffix, so a failed manifest
save cannot overwrite the previous replacement. If the input changes after
preview, review the refreshed preview before applying again.

**Revert** removes this resource's texture CRC entry and matching `[Files]`
name/archive-key aliases from the selected package. Copied payloads remain on
disk; it does not delete arbitrary package files. Another enabled package can
still replace the same resource. **Export PNG** saves a decoded original for
editing; original previews and exports are from the base archives, even when
other packages override them. Identical base textures share a content CRC,
so applying one also affects the other resources with that CRC.

Changes take effect on the next game launch. Game archives are only read.
Package comments, unrelated entries, metadata and other sections are preserved;
manifest output is ASCII or UTF-16LE with a BOM. Unsupported/malformed
manifests cannot be edited through the tab.

## Packages: import, export and load order

- **Import…** (or drop the file on the tab) takes a zipped mod or a TexMod
  `.tpf`. A zip must hold `mod.ini` at its top or inside one top folder (or
  contain exactly one `.tpf`); every name is checked first and a zip with an
  unsafe name (`..`, a drive, an absolute path) imports nothing. A `.tpf` is
  converted in the launcher (XOR layer, TexMod's ZipCrypto password, deflate,
  `texmod.def`): textures are named after their game resource when the scan
  knows the key, else `0xKEY`; Author/Description come from the package
  comment. The C core matched `tools/sudekimod.py` on the COOLER Runic Blade
  package (5 textures, identical sizes).
- **Export…** writes the selected package folder to a zip (stored, UTF-8
  names) that Import reads back on another PC.
- **▲/▼** move the selected package in the load order and rewrite
  `mods\load-order.txt` (see [mod-packages.md](mod-packages.md)); the list
  shows that order.

## Scan

The catalogue appears as soon as the archives are indexed and named (about
4.2 s on the supported install under Wine); thumbnails then stream in on 8
decode threads, the selected character/category first, and follow the
selection if it changes mid-scan. A full cold scan finished in 12.3–13.6 s, 5.3 s
from the thumbnail cache (`CONFIRMED_TEST`, 2026-10-07).

## Owner live test (2026-10-07)

On a throwaway game copy launched from the merged launcher, the owner
installed the converted COOLER Runic Blade package, loaded a save where Tal
holds the Runic Blade and saw it recoloured in the Talos fight
(`CONFIRMED_LIVE`; log: 4 of 5 keys replaced, see
[mod-packages.md](mod-packages.md)). The same test found that the GUI passed
Cleanroom/LAN arena game options as `--game-arg=<token>` while the loader
forwarded them literally, so the game never saw `-Level testroom` and the
cleanroom menu refused to install (`cleanroom_menu_error=87`); the loader now
strips the prefix. Drag-and-drop Apply of a single texture from the tab is
still not owner-confirmed in game.

## Checking a mod in the test room

Choose the **Cleanroom** profile on Play; a **Start as** list (Ailish, Tal,
Elco, Buki) appears beside it and the test room starts as that hero, so a
skin or texture change can be checked straight away (F8 for the sandbox
tools, Combat Mode to draw weapons). The test room only provides starter
weapons; to see another weapon, load a save that owns it.

## Implementation and grouping

The scan runs on one background worker, one archive mapped at a time.
Thumbnails are decoded by 8 threads that share that mapping, in batches of
512 resources; only the scan worker adds them to the image list. 80×80
thumbnails are cached under `%LOCALAPPDATA%\SudekiMP\thumbs`. On the
supported install under Wine (`CONFIRMED_TEST`, 2026-10-07): first scan
26.1 s with one decode thread, 15.0–15.3 s with eight, 6.5 s from the cache.
Grouping comes from `data/mod-groups.ini`, written from the game's own naming
scheme (`INFERENCE`): `W001`–`W011` Tal, `W012`–`W022` Ailish, `W023`–`W035`
and `W052`+ Elco, `W036`–`W051` Buki (`W049` is Kazel's sword), and the
outfit sets `Tal_Armour*`, `Ailish_LP*`, `Buki_V*`, `Elco_V*` plus the
Merged textures.

`src/modding/` is a portable C core for BAF indexing/key lookup, global name
harvesting, catalog construction, TGA/SQX/DDS inspection and RGBA decoding,
manifest editing/validation, and name-pattern grouping. It has no Win32 UI or
game-process dependency. Players do not need Python. `tools/sudekimod.py`
remains the reference/developer tool; the game loader and package format are
unchanged.

The worker scan retains archive index metadata, harvests names one mapped file
at a time, then builds thumbnails and a ready image list one archive at a time. The UI
takes ownership of the completed list instead of assembling it on the main
thread. It checks archive
size/mtime between passes and reports progress. Cancellation and window close
drain workers before their contexts are freed. Thumbnails are generated from
the player's install and cached under `%LOCALAPPDATA%\SudekiMP\thumbs`, keyed
by archive path, size, last-write time and resource key. No game images are
shipped. The cache and original bytes are not tracked repository inputs.

`data/mod-groups.ini` ships beside the GUI as `mod-groups.ini`. Its public
resource-name patterns were approved by the owner on 2026-10-07. They are
`INFERENCE` from names, not established item relationships. Sections use
`[Tal.BodyFace]`, `[Ailish.Weapons]`, etc.; `Pattern1=...` entries support `*`
and `?`, case-insensitively. The first matching rule wins. Broad hero fallback
rules follow specific category rules. Models always use the Models category.

Phase B's item → model → texture relationship remains `UNKNOWN`. No native
item or `.HOM` relationship is assumed by the grouping code. This work neither
changes native item behavior nor uses a speculative link to choose replacements.
The initial name rules may leave many weapons/armour in Other textures.

Model Apply/Browse remain disabled pending the owner's Talos-on-Tal live
`[Files]` confirmation. There is no OBJ/FBX conversion or rendered model preview.
Existing model entries can be inspected and reverted. Whole-resource editing
remains available through the established developer tool; its loader evidence
and limitations are documented separately in mod-packages.md.

## Verification record (2026-10-07)

Revision: `d08c667934b16bf37af997a74e4a4aa113389ce9` plus dirty launcher/core/test
changes. See the source/test fingerprint below. Profile: launcher only, with
synthetic archives/images; no game process or injection. Environment: Linux
host tests and 32-bit MinGW executables running in an isolated Wine prefix.

Commands/targets actually exercised:

- `tools/test-modding-core.sh`: `SudekiMP.ModdingCoreTest`, C/Python catalog
  and UTF-16 manifest round trip, and all 12 `tests/sudekimod_test.py` tests.
- MinGW CMake targets `SudekiMP.BetaLauncher`, `SudekiMP.ModdingCoreTest`,
  `SudekiMP.LauncherTabsTest`, `SudekiMP.LauncherModsUiTest`, and
  `SudekiMP.TextureModIndexTest`.
- Wine execution of `SudekiMP.ModdingCoreTest.exe`,
  `SudekiMP.TextureModIndexTest.exe`, `SudekiMP.LauncherTabsTest.exe`, and
  `SudekiMP.LauncherModsUiTest.exe <synthetic-fixture-directory>`.

Expected/observed: the bounded assertions passed. Core fixtures cover index
extents/duplicates/order, raw/RLE TGA orientation, DXT1/3/5 colors and alpha,
DDS mip extents/pitch, key/name resolution and ambiguity, cancellation,
Unicode manifest preservation, duplicate aliases, validation, and grouping.
The UI harness covers scan/search, the Browse dialog, a delivered
`WM_DROPFILES` to the drop-zone child, WIC PNG export with pixel round trip,
PNG/BMP/JPG previews, size warnings, exact payload copying, Apply rollback
with a deliberately blocked staging file, Revert, enable controls, unsupported
format refusal, Unicode mod folders, model Apply gating, and close during scan.
The tab harness retains Play control geometry, profile and directory values
through repeated switches. Source compiles without warnings in the selected
MinGW targets. Evidence level: `CONFIRMED_TEST`; not `CONFIRMED_LIVE` gameplay.

A bounded read-only scan also ran against the local supported install
(executable SHA-256 matches [executable.md](executable.md)). It found 6,525
textures and named models. The named Tal face resource decoded to 256×256
DXT1 with CRC `0x66283847`; Python independently reported 5,240 textures and
the same Tal face dimensions, format and CRC. After moving image-list assembly
to the worker, the warm-cache run's longest measured main-thread dispatch batch
was 16 ms. This measures this environment/selection; it is not a general
performance guarantee. No game process was started and no game archive was
written. Test mods/configuration and runtime cache stayed outside the install.
The full scan/result/preview is `CONFIRMED_TEST`; archive metadata agreement is
`CONFIRMED_STATIC`, not gameplay proof.

The synthetic UI fixture is reproducible with:

```sh
python3 tests/make_modding_ui_fixture.py <empty-test-directory>
```

Run `SudekiMP.LauncherModsUiTest.exe` with that directory's Windows path. This
harness changes its synthetic mod folders and settings; generate a fresh fixture
for each run. Fixture generation requires Python/Pillow on the developer's
machine; the launcher itself has no such dependency.

Source/test fingerprint (SHA-256): `349dc2ccd6989d761276d5cf1937f7e1939992f572ee1e0c220096a69563d621`. Computed over sorted
relative filenames followed by NUL, file bytes, and NUL: `CMakeLists.txt`,
`data/mod-groups.ini`, launcher source/header, `src/modding/*.[ch]`,
`tests/modding*` files, both launcher UI tests, and the fixture generator.

For a developer's read-only install scan, use the harness with two arguments:
`SudekiMP.LauncherModsUiTest.exe <game-directory> <test-ini-path>`. The test INI
must set `[Mods] Folder` to an existing absolute test-mod folder outside the
game install. Keep this configuration and any local paths in ignored private
context. This mode scans/decodes and closes; it does not apply a replacement.

## Remaining owner acceptance

1. Native Windows: verify launch/build check, update, profiles, stop/log flows;
   exercise Browse and an actual file-manager drop, including closing/rescanning.
2. Wine: try an actual desktop drag/drop and verify large-category interactions
   with the owner's selected packages. The bounded full-install scan and Tal
   preview are confirmed above; delivered synthetic drop messages do not prove
   a desktop/file-manager bridge.
3. On the supported game/profile, Apply a visible Tal texture, start the game,
   observe the change, Revert, restart, and observe the original. Record revision,
   dirty state, profile, environment and visual result per AGENTS.md. Test later
   package precedence and disabled packages where relevant.
4. Confirm the existing Talos-on-Tal model test before enabling model Apply.
   Research item/model/texture links with evidence before Phase B grouping.

Open-folder behavior and unavailable-WIC fallback have not been exercised by
the automated harness. Other installations/selections, packaged update downloads, native Windows
behavior and live gameplay remain unverified. No public support
or game-compatibility promise is added by these tests.
