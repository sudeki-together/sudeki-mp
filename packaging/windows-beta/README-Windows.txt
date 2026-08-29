SUDEKIMP WINDOWS BETA
=====================

This archive contains one SudekiMP folder. It is not a game installer and it
does not include Sudeki itself.

1. Install the supported GOG release of Sudeki.
2. Copy the included "SudekiMP" folder beside SUDEKI.exe. Example:

   C:\GOG Games\Sudeki\SUDEKI.exe
   C:\GOG Games\Sudeki\SudekiMP\Launch SudekiMP.cmd

3. Double-click "Launch SudekiMP.cmd" inside the SudekiMP folder. The beta
   launcher asks for the folder containing `SUDEKI.exe` on its first run, then
   remembers it for later launches.

Do not double-click SudekiMP.Launcher.exe by itself. It is a console loader
which requires the game and DLL paths. `Launch SudekiMP.cmd` opens the beta
launcher, which verifies the game build before it launches that loader.

Before first launch, this beta checks that SUDEKI.exe is the supported GOG
build. It never changes SUDEKI.exe, the game archives, or your save files.

This package starts with all research and co-op prototypes disabled. It is a
safe loader/injection beta, not yet a general Windows local-co-op release.
The title roster experiment is intentionally off until its Windows native-page
path has passed live acceptance. This package therefore does not yet show the
local co-op character-lock screen when you choose New Game.

If the launcher reports an error, keep the command window open and include
the text from it plus SudekiMP.log (beside SUDEKI.exe) in a bug report.

OPTIONAL SELF-UPDATE
--------------------

The beta launcher includes an "Optional update" button. It is OFF by default.
Manual package downloads are recommended. If you explicitly choose update, it
downloads the latest unsigned beta package over HTTPS from the Sudeki Together
project server, confirms its required files, backs up the current SudekiMP
files, preserves your SudekiMP.ini, and changes only mod-side files. It never
changes SUDEKI.exe, game data, or save files. Close Sudeki before updating.
