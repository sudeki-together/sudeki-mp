SUDEKIMP WINDOWS LAUNCHER
========================

This archive contains one SudekiMP folder. It is not a game installer and it
does not include Sudeki itself.

1. Install the supported GOG release of Sudeki.
2. Copy the included "SudekiMP" folder beside SUDEKI.exe. Example:

   C:\GOG Games\Sudeki\SUDEKI.exe
   C:\GOG Games\Sudeki\SudekiMP\Launch SudekiMP.cmd

3. Double-click "Launch SudekiMP.cmd" inside the SudekiMP folder. It opens the
   standalone SudekiMP Beta Launcher. Paste the folder containing `SUDEKI.exe`
   or use Browse, then verify and launch. The folder is remembered locally.

Do not double-click SudekiMP.Launcher.exe by itself. It is a console loader
which requires the game and DLL paths. `Launch SudekiMP.cmd` opens the native
`SudekiMP.LauncherGUI.exe`, which verifies the game build before it launches
that loader.

Before first launch, this beta checks that SUDEKI.exe is the supported GOG
build. It never changes SUDEKI.exe, the game archives, or your save files.

Choose a profile in the launcher: **Local co-op (2 players)**,
**Multiplayer (title menu lobby)**, **Cleanroom**, or **Safe launch**.
Multiplayer adds a Multiplayer entry to the game's title screen: one player
hosts a lobby, the others join by IP, and the lobby starts every player's game
itself. Its **Mode** list picks the lobby kind: **Test room** (save-free) or
**Saved story** (the campaign from the host's save). This is experimental. The **LAN arena host** and **LAN arena client** test
profiles (and the LAN IP/Port fields) appear only after ticking **Developer
mode** on the Tools tab. Cleanroom shows a **Start as** list (Ailish, Tal,
Elco, Buki): the test room starts as that hero, handy for checking a mod's
look straight away. Local co-op
reserves XInput slot 0 for Player 2 while Player 1 remains keyboard/mouse.
LAN arena is a save-free, direct-IP, host-authoritative Tal/Ailish experiment;
host and client each run a separate full-screen Sudeki process. Cleanroom is
also save-free. Talos research profiles are intentionally not exposed.

The **Enable cleanroom sandbox tools (F8)** checkbox applies only to the
Cleanroom profile and defaults on. Press F8 there to spawn/remove party actors
and the Training Dummy, toggle combat/camera modes, and use the cleanroom
inventory and infinite-meter conveniences. Turning it off still launches the
save-free testroom without installing the menu. It never enables these tools
in campaign or LAN profiles.

The launcher can stop only the game session it started and still tracks. It can
open `SudekiMP.log` and export a local support folder containing the runtime
log, active INI, and launcher summary. Log upload is not implemented; share the
folder manually when requested.

Update checks are optional. **Check for updates on startup** only reads the
official small manifest and prompts when a different version exists. The
launcher never installs an update silently or changes Sudeki game data/saves.

LICENSE AND ATTRIBUTION
-----------------------

SudekiMP is licensed under AGPL-3.0-or-later. See the included LICENSE and
NOTICE files. Redistributions must preserve the attribution:

    SudekiMP was originally created by wander.

CO-OP SAVE FIXTURES
-------------------

The beta launcher includes **Install co-op save fixtures…**. It is an explicit,
recoverable test-data action: it moves `%APPDATA%\Sudeki\Save` to a timestamped
folder under `%APPDATA%\Sudeki\SudekiMP-Backups`, creates a fresh Save folder,
then copies the included co-op fixtures into that fresh folder. It never deletes
the archived folder or changes files inside the game installation. Cancel leaves
your saves untouched. Restore an old save set by closing Sudeki, removing the
fresh `%APPDATA%\Sudeki\Save` folder, and renaming the desired archived `Save-*`
folder back to `Save`.

The **Test XInput controller…** button opens a console-only diagnostic. With an
XInput-compatible controller connected, it reports the detected Windows slot
and live button/stick values. The first Windows co-op launch supports slot 0
only; the launcher’s checkbox reserves that slot for Player 2. If the probe
reports another slot, do not start co-op yet—send the output so the profile can
be adjusted deliberately.

If the launcher reports an error, keep the command window open and include
the text from it plus SudekiMP.log (beside SUDEKI.exe) in a bug report.

OPTIONAL MUSIC AND MANUAL BETA DOWNLOADS
----------------------------------------

The native launcher uses the Sudeki Together crest as its application icon and
includes a **Play project music** button. That button alone fetches the public
music catalog and track over HTTPS, caches it under `%LOCALAPPDATA%\SudekiMP`,
and plays it inside the launcher. The MP3 is not packaged with the beta.
The **Developer: wander** button opens https://git.unfilteredrealm.com/wander.

The **Check for updates** button reads the public launcher manifest and, after
confirmation, opens the public package page in your browser.
Download and extract the ZIP yourself, then replace only this `SudekiMP`
folder. This beta deliberately does not use an unsigned PowerShell self-updater:
manual replacement is clearer, preserves your `SudekiMP.ini`, and never changes
SUDEKI.exe, game data, or save files. Close Sudeki before replacing the folder.

MODS TAB (EXPERIMENTAL)

Choose your game folder on Play, then open Mods. Select a mod, character,
category and texture; use Browse or drop an image, then Apply. Apply only saves
into the mod folder. Start the game again to see the replacement. Revert removes
this resource's entries from the selected mod. Other enabled mods may still
replace it. Export PNG saves the base original for editing.

The selector shows alphabetical folder load order; later enabled packages win.
My Mods is the initial package inside the configured mods folder. Grouping uses
editable name patterns in mod-groups.ini and may leave items under World/Other
or Other textures. No game images are included, and Python is not required.
Model Apply is disabled pending the owner-run live test; there is no OBJ/FBX
converter. Native Windows/live game acceptance for this new editor is pending.
