SUDEKIMP WINDOWS BETA
=====================

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
beta launcher, which verifies the game build before it launches that loader.

Before first launch, this beta checks that SUDEKI.exe is the supported GOG
build. It never changes SUDEKI.exe, the game archives, or your save files.

This package starts in a safe single-player configuration. To run the focused
Windows local-co-op beta, connect one XInput controller, use **Test XInput
controller…** to confirm its slot, tick **Windows local co-op beta**, and then
launch. The current beta reserves XInput slot 0 for Player 2, opens the
co-op roster path, enables split presentation and Talos party restoration, and
keeps the game's native slot-0 controller route from also moving Player 1.
Player 1 remains keyboard/mouse. This is a focused input-and-lifecycle beta,
not a complete co-op playthrough or online multiplayer release.

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

The **Get latest beta** button opens the public package page in your browser.
Download and extract the ZIP yourself, then replace only this `SudekiMP`
folder. This beta deliberately does not use an unsigned PowerShell self-updater:
manual replacement is clearer, preserves your `SudekiMP.ini`, and never changes
SUDEKI.exe, game data, or save files. Close Sudeki before replacing the folder.
