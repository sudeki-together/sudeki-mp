# Sudeki executable

## Identity

| Field | Confirmed value |
| --- | --- |
| Filename | `SUDEKI.exe` |
| Size | 3,837,952 bytes |
| SHA256 | `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94` |
| Format | PE32 Windows GUI executable, Intel i386 |
| PE timestamp | 2014-04-15 07:17:07 |
| Image base | `0x00400000` |
| Entry RVA | `0x0024841B` |
| Entry VA | `0x0064841B` |
| Image size | `0x0045F000` |
| GOG build ID | `50303954381148403` |
| Full baseline manifest | `research/hashes/sudeki-gog-1.0-build-50303954381148403.sha256` |

## Sections

| Name | RVA | Raw size | Role |
| --- | ---: | ---: | --- |
| `.text` | `0x00001000` | `0x00298600` | executable code |
| `.rdata` | `0x0029A000` | `0x00082600` | read-only data, imports, exports |
| `.data` | `0x0031D000` | `0x00043600` | initialized writable data |
| `.rsrc` | `0x00414000` | approximately 8 KiB | resources |
| `.reloc` | `0x00416000` | `0x00048A00` raw | relocations |

## Direct imports

`KERNEL32.dll`, `USER32.dll`, `XINPUT1_2.dll`, `WINMM.dll`, `d3dx9_30.dll`, `GDI32.dll`, `SHELL32.dll`, `DINPUT8.dll`, `d3d9.dll`, `DSOUND.dll`, and local `binkw32.dll`.

Timing-relevant imports include `QueryPerformanceCounter`, `QueryPerformanceFrequency`, `GetTickCount`, `timeGetTime`, `Sleep`, and wait APIs. The Quick Menu path identified in the current build does not call one of these APIs directly; it changes a `CGameSpeed` mode consumed by the main frame-time path.

The executable exports 1,378 named C++ entries and names its export image `GOGWINSOLM.exe`. Several exported `CGameSpeed` functions provide strong anchors even though the two Quick Menu state-change methods themselves are internal.

`localization/SUDEKI_en.dat` is byte-identical to the installed English `SUDEKI.exe`. The German, Spanish, French, and Italian `.dat` variants have different hashes and must be treated as different executable builds if language switching replaces the main executable.

7-Zip reports a PE checksum warning. The installed executable is nevertheless stable across repeated SHA256 checks and matches both baseline copies. Record the warning when creating the Ghidra project; do not “repair” the PE checksum.

## Ghidra rule

Import only the executable with the SHA256 above. Use image base `0x00400000`. Keep all initial names provisional until supported by cross-references and an experiment. No static or runtime address discovered against another build may be applied to this executable without signature and behavior validation.

Ghidra 12.1.2 is installed per-user through Flathub. Full auto-analysis succeeded using language/compiler `x86:LE:32:default:windows`; the saved project is outside the repository at `/home/wander/Games/SudekiMP/analysis/SudekiGOG_1_0_50303954381148403`.

The import reported three non-fatal conditions: the original `WINSOLM.pdb` was unavailable, Windows system DLLs were not imported, and Ghidra found conflicting data at the beginning of the PE relocation area. Analysis and project save nevertheless completed successfully. The hash-gated script `tools/ghidra/QuickMenuReport.java` independently reproduced the Quick Menu instructions, constants, and frame-loop relationships documented here.
