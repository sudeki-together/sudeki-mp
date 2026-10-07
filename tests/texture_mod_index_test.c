#include "engine/texture_mod_index.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

int main(void) {
    uint32_t key = 0;
    char path[SUDEKIMP_TEXTURE_MOD_PATH_MAX];
    SudekiMpTextureModIndex index;

    /* Standard CRC-32 of "123456789" is 0xCBF43926; TexMod omits the final inversion. */
    CHECK(SudekiMpTexModCrc32("123456789", 9) == 0x340bc6d9u);
    CHECK(SudekiMpTexModCrc32("", 0) == 0xffffffffu);

    CHECK(SudekiMpTexModLevelBytes(21, 512, 512) == 512u * 512u * 4u);       /* A8R8G8B8 */
    CHECK(SudekiMpTexModLevelBytes(0x31545844u, 256, 256) == 256u * 256u / 2u); /* DXT1 */
    CHECK(SudekiMpTexModLevelBytes(0x35545844u, 256, 128) == 256u * 128u);    /* DXT5 */
    CHECK(SudekiMpTexModLevelBytes(0x3231564eu, 16, 16) == 0);                /* unknown */
    CHECK(SudekiMpTexModLevelBytes(21, 0, 16) == 0);

    CHECK(SudekiMpTextureModParseKey("0xAC57BC5D", &key) && key == 0xac57bc5du);
    CHECK(SudekiMpTextureModParseKey("0x1", &key) && key == 1u);
    CHECK(!SudekiMpTextureModParseKey("AC57BC5D", &key));
    CHECK(!SudekiMpTextureModParseKey("0x", &key));
    CHECK(!SudekiMpTextureModParseKey("0x123456789", &key));
    CHECK(!SudekiMpTextureModParseKey("0xG1", &key));

    CHECK(SudekiMpTextureModSafePath("textures/Verdana_16-0.dds"));
    CHECK(SudekiMpTextureModSafePath("a\\b.png"));
    CHECK(!SudekiMpTextureModSafePath("../x.dds"));
    CHECK(!SudekiMpTextureModSafePath("a/../x.dds"));
    CHECK(!SudekiMpTextureModSafePath("/abs.dds"));
    CHECK(!SudekiMpTextureModSafePath("C:x.dds"));
    CHECK(!SudekiMpTextureModSafePath("a//b.dds"));
    CHECK(!SudekiMpTextureModSafePath("a/./b.dds"));
    CHECK(!SudekiMpTextureModSafePath("a/"));
    CHECK(!SudekiMpTextureModSafePath(""));

    CHECK(SudekiMpTextureModParseLine("  0xAC57BC5D = textures/font.dds \r\n", &key, path, sizeof(path)) == 1);
    CHECK(key == 0xac57bc5du && !strcmp(path, "textures/font.dds"));
    CHECK(SudekiMpTextureModParseLine("; comment", &key, path, sizeof(path)) == 0);
    CHECK(SudekiMpTextureModParseLine("   ", &key, path, sizeof(path)) == 0);
    CHECK(SudekiMpTextureModParseLine("Verdana_16-0.tga=x.dds", &key, path, sizeof(path)) == -1);
    CHECK(SudekiMpTextureModParseLine("0x1=", &key, path, sizeof(path)) == -1);
    CHECK(SudekiMpTextureModParseLine("0x1=../escape.dds", &key, path, sizeof(path)) == -1);
    CHECK(SudekiMpTextureModParseLine("0x1 textures/x.dds", &key, path, sizeof(path)) == -1);

    memset(&index, 0, sizeof(index));
    CHECK(SudekiMpTextureModIndexAdd(&index, 7, 0, "a/seven.dds"));
    CHECK(SudekiMpTextureModIndexAdd(&index, 3, 0, "a/three.dds"));
    CHECK(SudekiMpTextureModIndexAdd(&index, 7, 1, "b/seven.png")); /* later mod wins */
    CHECK(!SudekiMpTextureModIndexAdd(&index, 9, 1, "../bad.dds"));
    CHECK(SudekiMpTextureModIndexFind(&index, 7) == NULL);           /* not finished */
    SudekiMpTextureModIndexFinish(&index);
    CHECK(index.count == 2 && index.overridden == 1);
    {
        const SudekiMpTextureModEntry *e = SudekiMpTextureModIndexFind(&index, 7);
        CHECK(e && e->mod == 1 && !strcmp(SudekiMpTextureModIndexPath(&index, e), "b/seven.png"));
        e = SudekiMpTextureModIndexFind(&index, 3);
        CHECK(e && !strcmp(SudekiMpTextureModIndexPath(&index, e), "a/three.dds"));
        CHECK(!SudekiMpTextureModIndexFind(&index, 4));
    }
    CHECK(!SudekiMpTextureModIndexAdd(&index, 5, 0, "late.dds"));   /* sealed */
    SudekiMpTextureModIndexFree(&index);

    if (failures) return 1;
    puts("texture_mod_index_test: ok");
    return 0;
}
