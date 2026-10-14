# Sudeki co-op save fixtures

This directory contains the stable save snapshot used for local co-op beta
testing. It includes slots `0000` through `0012` and the retained
`SAVESLOT0010.user-backup-20260827T1929` backup.

Each slot contains Sudeki's native `sudeki.fish` payload and `fluffy.bunny`
metadata. The files are intentionally binary fixtures: do not edit them or
overwrite a personal Wine/Windows save directory with them without first
making your own backup.

Scrubbed 2026-10-08 (owner-approved, for the public 0.5.0 release): each
`fluffy.bunny` carried 16 bytes of stale memory spelling part of the creating
PC's Windows profile path (`\<user>\AppData\`) inside a leftover buffer at
body offset 0x2A8 (file offset 0x4A8). Those bytes are now zero. The loader
(`SUDEKI.exe` 0x5BCAA0 / callback 0x4FE7B0) checks only the header size and
format version 15, and an edited slot loaded normally in game. The slot labels
are in Russian because the saves come from a Russian-language copy of the game.

They are included to reproduce current co-op test cases, not as game assets
or a replacement for a user-owned GOG installation.
