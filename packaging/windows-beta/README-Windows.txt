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

This package starts with all research and co-op prototypes disabled. It is a
safe loader/injection beta, not yet a general Windows local-co-op release.
The title roster experiment is intentionally off until its Windows native-page
path has passed live acceptance. This package therefore does not yet show the
local co-op character-lock screen when you choose New Game.

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
