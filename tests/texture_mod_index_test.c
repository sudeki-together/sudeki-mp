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

    /* [Files] lines and archive buckets. */
    {
        char name[SUDEKIMP_MOD_NAME_MAX + 1];
        uint8_t src[36], dst[48];
        static const uint32_t keys[3] = {0x100u, 0x300u, 0x500u};
        CHECK(SudekiMpModResourceKey("Verdana_16-0.tga") == 0x02ae20dfu);
        CHECK(SudekiMpModResourceKey("TALOS.HOM") == SudekiMpModResourceKey("talos.hom"));
        CHECK(SudekiMpModFileParseLine(" TAL.HOM = files/TAL.HOM ", &key, name, sizeof(name), path, sizeof(path)) == 1);
        CHECK(!strcmp(name, "TAL.HOM") && key == SudekiMpModResourceKey("TAL.HOM") && !strcmp(path, "files/TAL.HOM"));
        CHECK(SudekiMpModFileParseLine("0x02AE20DF=files/raw.bin", &key, name, sizeof(name), path, sizeof(path)) == 1 && key == 0x02ae20dfu);
        CHECK(SudekiMpModFileParseLine("; note", &key, name, sizeof(name), path, sizeof(path)) == 0);
        CHECK(SudekiMpModFileParseLine("NODOT=files/x", &key, name, sizeof(name), path, sizeof(path)) == -1);
        CHECK(SudekiMpModFileParseLine("A.B.C=files/x", &key, name, sizeof(name), path, sizeof(path)) == -1);
        CHECK(SudekiMpModFileParseLine("BAD NAME.HOM=files/x", &key, name, sizeof(name), path, sizeof(path)) == -1);
        CHECK(SudekiMpModFileParseLine("TAL.HOM=../x", &key, name, sizeof(name), path, sizeof(path)) == -1);
        for (unsigned i = 0; i < 3; ++i) {
            uint32_t row[3] = {i * 10u, i * 20u + 1u, keys[i]};
            memcpy(src + i * 12u, row, 12u);
        }
        CHECK(SudekiMpArchiveBucketFind(src, 3, 0x300u) == 1);
        CHECK(SudekiMpArchiveBucketFind(src, 3, 0x400u) == -1);
        CHECK(SudekiMpArchiveBucketFind(NULL, 0, 0x400u) == -1);
        SudekiMpArchiveBucketInsert(dst, src, 3, 0x80000000u, 77u, 0x400u);
        CHECK(SudekiMpArchiveBucketFind(dst, 4, 0x400u) == 2);
        CHECK(SudekiMpArchiveBucketFind(dst, 4, 0x500u) == 3 && SudekiMpArchiveBucketFind(dst, 4, 0x100u) == 0);
        {
            uint32_t row[3];
            memcpy(row, dst + 24u, 12u);
            CHECK(row[0] == 0x80000000u && row[1] == 77u && row[2] == 0x400u);
        }
        SudekiMpArchiveBucketInsert(dst, src, 0, 1u, 2u, 0x9u); /* empty bucket */
        CHECK(SudekiMpArchiveBucketFind(dst, 1, 0x9u) == 0);
    }

    if (failures) return 1;
    puts("texture_mod_index_test: ok");
    return 0;
}
