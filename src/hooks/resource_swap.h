#ifndef SUDEKIMP_RESOURCE_SWAP_H
#define SUDEKIMP_RESOURCE_SWAP_H
#include <windows.h>
#include <stdint.h>

/* Runtime archive resource redirection ([ResourceSwap] in the ini):
 *   TAL.HOM=TALOS.HOM
 * Every mounted .Baf is an XBafFileSystem whose index is 256 buckets of
 * 12-byte entries {offset,size,key}, key = checksum("NAME.EXT") (upper-case
 * ASCII, alternate add/multiply). Two lookups exist, each with one caller:
 *   0x5BE100 find-by-key  (EAX=archive, [ESP+4]=key, RET 4)
 *   0x5BDFA0 find-by-name (EAX=archive, [ESP+4]=char*, '#hex' literal, RET 4)
 * This adapter owns both entries and rewrites the key (or the name pointer,
 * to a mod-owned replacement string) before the native search runs. No data
 * file is modified; a missing target simply fails the lookup like a missing
 * resource. Host and clients must use the same table for consistent visuals.
 * Install before archives are mounted (DLL load). */
BOOL SudekiMpResourceSwapInstall(HMODULE image,const wchar_t *config_path);
BOOL SudekiMpResourceSwapUninstall(void);
unsigned SudekiMpResourceSwapCount(void);
/* Built-in named profiles (e.g. "TalosOnTal"); ini [ResourceSwap] Profile=
 * selects one at load, and a later lobby may apply one before the world
 * loads (the hook is installed at DLL load; the table is plain data). */
BOOL SudekiMpResourceSwapApplyProfile(const char *name);
void SudekiMpResourceSwapClear(void);
uint32_t SudekiMpResourceChecksum(const char *name);
#endif
