#ifndef SUDEKIMP_PROFILE_FOLDER_H
#define SUDEKIMP_PROFILE_FOLDER_H
#include <windows.h>
/* Second game on one Windows profile ([TitleMenu] AllowSecondInstance plus
 * [TitleMenu] ProfileFolder=<folder>): the game's roaming AppData lookups
 * (SHGetSpecialFolderPathA with CSIDL_APPDATA, IAT slot RVA 0x29a1e8; used for
 * Save, Cache and PlayerOptions.xml) answer with <folder> instead, so two
 * games never write the same save working files. Other folder ids pass
 * through. The launcher seeds <folder>\Sudeki from the real profile. */
BOOL SudekiMpProfileFolderInstall(HMODULE game,const wchar_t *folder);
BOOL SudekiMpProfileFolderUninstall(void);
#endif
