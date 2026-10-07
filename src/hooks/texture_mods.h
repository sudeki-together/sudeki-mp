#ifndef SUDEKIMP_TEXTURE_MODS_H
#define SUDEKIMP_TEXTURE_MODS_H
#include <windows.h>

/* Runtime texture replacement from mod packages ([TextureMods] in the ini;
 * format in docs/texture-mods.md).
 *
 * Every decoded game texture passes through the decode dispatcher at RVA
 * 0x1D8300 (ESI = 0x3C-byte texture record, one stack argument, RET 4,
 * AL = success); it selects the SQX, TGA or third codec, and the codec stores
 * the created IDirect3DTexture9* at record+4 (0x5F3400) and fills its levels.
 * This adapter owns that entry. After a successful decode it hashes level 0
 * with the TexMod-compatible key; on a match it builds the replacement with
 * the game's own d3dx9_30 loader and swaps it into record+4, releasing the
 * original. The record keeps the original dimensions/format fields. Tiled and
 * cube textures (record+4 is not a plain texture) are left alone.
 *
 * Installs nothing unless at least one enabled mod lists a texture, or
 * Dump=true (writes every decoded texture once as <key>.dds for finding keys).
 * Purely visual and local: peers need not run the same mods. */
BOOL SudekiMpTextureModsInstall(HMODULE image, const wchar_t *config_path);
BOOL SudekiMpTextureModsUninstall(void);
unsigned SudekiMpTextureModsCount(void);
#endif
