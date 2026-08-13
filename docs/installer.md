# Installer and vanilla-baseline report

Date: 2026-08-12
Host: Linux, America/New_York
Status: Phase 0 complete; ready for Ghidra

## Offline installer set

The user-supplied files are kept in the ignored `gog-installer/` directory.

| Filename | Size (bytes) | SHA256 |
| --- | ---: | --- |
| `setup_sudeki_1.0_(13212).exe` | 911,712 | `e63df62af8fc36eb294a13f05bbf5f84137e504d529d07b1c1cf8bcc13bb0883` |
| `setup_sudeki_1.0_(13212)-1.bin` | 4,294,064,126 | `3105d7ded8b424b0c3ea26e5e8af153319ec0719e7e33eaae53171f231e35119` |
| `setup_sudeki_1.0_(13212)-2.bin` | 933,072,864 | `06b9dfa5eee55af2708f1db7703badb2c99107abbae93d16594d69ed5c7700b5` |

The `.exe` is a PE32/i386 GOG Inno Setup installer with an Authenticode certificate table. Its metadata identifies Sudeki `1.0`, GOG package label `13212`, game ID `1207664353`, and build ID `50303954381148403`. Its PE creation timestamp is reported as 2019-01-04 08:09:33. Cryptographic signature verification was not performed because `osslsigncode` and `pesign` were unavailable.

## Installation method

7-Zip identifies the PE and Inno metadata but does not assemble the two external `.bin` payloads. `innoextract` was unavailable. The supported Inno installer was therefore run non-interactively under Wine with `/VERYSILENT`, `/SUPPRESSMSGBOXES`, `/NORESTART`, and an explicit fresh destination.

The installation completed successfully, including its bundled DirectX and Visual C++ redistributable steps. No default Wine prefix was used.

- Runner: GE-Proton 11-3, reporting Wine 11.0 (Staging)
- Dedicated 32-bit prefix: `/home/wander/Games/sudeki-offline-prefix`
- Installation log: `/home/wander/Games/sudeki-offline-prefix/drive_c/sudeki-offline-install.log`
- Initial install staging tree: `/home/wander/Games/SudekiMP/staging` (renamed after verification)

## Confirmed GOG build

| Field | Value |
| --- | --- |
| Name | Sudeki |
| Version | `1.0` |
| Installer package label | `13212` |
| GOG game ID | `1207664353` |
| GOG build ID | `50303954381148403` |
| GOG client ID | `49964622653196442` |
| Language | English (`en-US`) |
| Primary executable | `SUDEKI.exe` |

## Major installed files

| Path | Size (bytes) | SHA256 |
| --- | ---: | --- |
| `SUDEKI.exe` | 3,837,952 | `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94` |
| `SOLData.baf` | 827,656,192 | `9e604ec2b4736a99e16a4c76dee5842665cd3d138db44a8a694f105fa7ed360c` |
| `Fonts.baf` | 8,017,920 | `4c906e0ebf24edf2f222201ca6296bd187a75a1d3614d1d909be13a01b108677` |
| `controller.baf` | 724,992 | `587c5fd3a17782767e139daef4ede3d22f9f4dd6362125a78b67fb74b946c5ea` |
| `Data/SOLWORLDM.gex` | 1,848,586 | `e36a5974f9aedea5b5b428fe2445cf496c52911ff01d4934ea8ab8124abf1ff9` |
| `Data/WINSOLM.gex` | 331,759 | `7502aaa1a9904d164f2a64d660c7d54b9ed88fe98c03077723b2f37f07a01277` |

The installation contains 394 files and occupies approximately 5.6 GiB. Large additional resources include Bink movies under `movies/`, XACT banks under `sound/`, and language data under `localization/`.

## Vanilla and working baselines

- Read-only vanilla tree: `/home/wander/Games/SudekiMP/vanilla`
- Writable working tree: `/home/wander/Games/SudekiMP/working`
- Verification manifest: `research/hashes/sudeki-gog-1.0-build-50303954381148403.sha256`
- Manifest SHA256: `9eb4c5282f6d46d6dcfe755b3e6b6bd74ebe1375d1f58d1b649e478d0073ca9e`

Both trees verified `394/394` files against the same manifest. They are on Btrfs and the working tree was created with copy-on-write reflinks. Write permission was removed recursively from the vanilla tree. A second full verification after the launch test also passed `394/394`.

The working tree is owner-readable/writable only. Permission hardening did not alter its contents; it again verified `394/394` afterward.

## Wine launch baseline

The custom `wine` wrapper launches GE-Proton directly without Proton's normal prefix bootstrap. The first game launch therefore failed because Wine's `wined3d.dll` could not find GE-Proton's bundled 32-bit `libvkd3d` runtime DLLs. The following prefix-only files were copied from GE-Proton 11-3 into `drive_c/windows/system32/`:

- `libvkd3d-1.dll`
- `libvkd3d-shader-1.dll`
- `libvkd3d-utils-1.dll`

No game file was changed. After that Wine-prefix correction, the read-only vanilla executable reached the main menu at 1920×1080. Automated gameplay, saving/loading, controller, combat, Quick Menu, Skill Strike, Spirit Strike, cutscene, and character-switching tests have not yet been performed and must not be reported as passing.

The process was terminated after capturing the title screen because synthetic keyboard/window-close events were not accepted by its fullscreen DirectInput window. Fullscreen teardown temporarily returned the user's graphical session to a TTY; the Wayland session recovered and no game/Wine process remained. Treat this as a Wine/compositor baseline issue, not a mod issue. Do not automatically launch fullscreen again without user coordination. All vanilla hashes remained unchanged afterward.

## Boundary

No executable, archive, resource, configuration file in the game tree, address, RVA, hook, or patch was modified. Phase 0 ends here.
