#ifndef SUDEKIMP_ARCHIVE_MODS_H
#define SUDEKIMP_ARCHIVE_MODS_H
#include <windows.h>
#include <stdint.h>

/* Whole-file archive resources from mod packages ([Files] in mod.ini,
 * docs/mod-packages.md): models, animation banks, SOL definitions, textures,
 * zones, collision, font descriptors... anything stored in a mounted .Baf.
 *
 * Mount (RVA 0x1BD400) parses an archive's index with the CRT and then opens
 * its read handle with CALL [CreateFileA] at RVA 0x1BD45C; EBP is the
 * XBafFileSystem object there and its index is complete. This adapter owns
 * that call: after the native open it points each listed resource's index
 * row at a virtual offset (>= 0x80000000) with the mod file's size, or inserts
 * a row for a name no archive holds (into the AddToArchive archive, using the
 * game's own new[]/delete[] so the archive destructor stays valid).
 *
 * Every resource read is XBafFileLoader::Read (RVA 0x1BD290): SetFilePointer
 * then ReadFile on the archive handle under the archive's lock. The adapter
 * owns those two IAT slots (0x29A0B0, 0x29A0DC) and serves reads that land in
 * a virtual range from the mod file; every other handle passes straight
 * through. No byte inside the code ranges lan_story_resource_file.c hashes is
 * changed, and the CreateFileA slot it checks is untouched.
 *
 * Gameplay data changes simulation: host and clients must install the same
 * [Files] mods. Once any archive is patched the read hooks are pinned. */
int SudekiMpArchiveModsAdd(uint32_t key, const char *name, const wchar_t *path, const char *mod,
    const wchar_t *add_archive);
BOOL SudekiMpArchiveModsInstall(HMODULE image);
BOOL SudekiMpArchiveModsUninstall(void);
unsigned SudekiMpArchiveModsCount(void);
#endif
