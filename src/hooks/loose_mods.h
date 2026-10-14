#ifndef SUDEKIMP_LOOSE_MODS_H
#define SUDEKIMP_LOOSE_MODS_H
#include <windows.h>

/* Loose game-folder files from mod packages ([Files] names "sound/<file>" and
 * "movies/<file>", docs/mod-packages.md): XACT wave/sound banks, speech and
 * Bink movies, which live beside the .Baf archives rather than inside them.
 *
 * The adapter owns six "CALL [import]" sites (6 bytes each) whose first stack
 * argument is the path being opened, and swaps that argument for the mod
 * file's path when its sound\... or movies\... tail is overridden:
 *   0x28870B            Sx streamed speech banks, sound\Speech\ (CreateFileA)
 *   0x28878C            Sx streamed wave banks, sound\ (CreateFileA)
 *   0x2890DE            Sx in-memory wave banks and .xsb sound banks (CreateFileA)
 *   0x256C49            CRT open, under fopen (CreateFileA)
 *   0x1BF383, 0x1BF3C9  movie header check (CreateFileA) and BinkOpen
 * Other paths pass through unchanged. The CreateFileA import slot itself is
 * not touched (lan_story_load checks it). Startup existence probes
 * (0x28D205) are left alone. */
int SudekiMpLooseModsAdd(const char *name, const wchar_t *path, const char *mod);
BOOL SudekiMpLooseModsInstall(HMODULE image);
BOOL SudekiMpLooseModsUninstall(void);
#endif
