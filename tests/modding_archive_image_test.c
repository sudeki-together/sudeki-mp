#include "modding/mod_archive.h"
#include "modding/mod_image.h"
#include "engine/texture_mod_index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "archive/image:%d: %s\n", __LINE__, #expr); return 1; } } while (0)
static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (i * 8)); }

static size_t make_tga(uint8_t *data, int bottom) {
    const uint8_t top[] = {0,0,255,128, 0,255,0,255, 255,0,0,64, 255,255,255,0};
    memset(data, 0, 34); data[2] = 2; data[16] = 32; data[17] = (uint8_t)(bottom ? 8 : 0x28);
    put16(data + 12, 2); put16(data + 14, 2);
    if (bottom) { memcpy(data + 18, top + 8, 8); memcpy(data + 26, top, 8); }
    else memcpy(data + 18, top, 16);
    return 34;
}
static size_t make_dds(uint8_t *data, uint32_t format, uint32_t width, uint32_t height) {
    size_t bytes = format == 21 ? (size_t)width * height * 4 : (size_t)((width + 3) / 4) * ((height + 3) / 4) * (format == 0x31545844u ? 8 : 16);
    memset(data, 0, 128 + bytes); memcpy(data, "DDS ", 4); put32(data + 4, 124);
    put32(data + 8, 0x1007u | (format == 21 ? 8u : 0x80000u));
    put32(data + 12, height); put32(data + 16, width); put32(data + 20, format == 21 ? width * 4 : (uint32_t)bytes);
    put32(data + 76, 32); put32(data + 108, 0x1000);
    if (format == 21) {
        put32(data + 80, 0x41); put32(data + 88, 32); put32(data + 92, 0xff0000);
        put32(data + 96, 0xff00); put32(data + 100, 0xff); put32(data + 104, 0xff000000u);
    } else { put32(data + 80, 4); put32(data + 84, format); }
    return 128 + bytes;
}
static void make_sqx(uint8_t *data, unsigned format) {
    memset(data, 0, 2048); put16(data, 0xf800); put16(data + 2, 0x07e0);
    put32(data + 2044, 0x23u | (format << 8) | (1u << 16) | (2u << 20) | (2u << 24));
}

typedef struct FixtureResource { const uint8_t *data; size_t size; uint32_t key; } FixtureResource;
static uint8_t *make_baf(const FixtureResource *resources, size_t count, size_t *size, size_t *table, size_t *first_row) {
    size_t payload_size = 0, cursor, offset = 0, order[16], offsets[16];
    uint8_t *data;
    if (count > 16) return NULL;
    for (size_t i = 0; i < count; ++i) { offsets[i] = payload_size; order[i] = i; payload_size += resources[i].size; }
    for (size_t i = 0; i < count; ++i) for (size_t j = i + 1; j < count; ++j) {
        if (resources[order[j]].key < resources[order[i]].key) { size_t swap = order[i]; order[i] = order[j]; order[j] = swap; }
    }
    *table = payload_size; *size = payload_size + 1024 + count * 12 + 2060;
    data = (uint8_t *)calloc(*size, 1);
    if (!data) return NULL;
    for (size_t i = 0; i < count; ++i) { memcpy(data + offset, resources[i].data, resources[i].size); offset += resources[i].size; }
    cursor = payload_size; *first_row = 0;
    for (unsigned bucket = 0; bucket < 256; ++bucket) {
        uint32_t n = 0;
        for (size_t i = 0; i < count; ++i) if ((resources[i].key & 255u) == bucket) ++n;
        put32(data + cursor, n); cursor += 4;
        for (size_t ordinal = 0; ordinal < count; ++ordinal) {
            size_t i = order[ordinal];
            if ((resources[i].key & 255u) == bucket) {
                if (!*first_row) *first_row = cursor;
                put32(data + cursor, (uint32_t)offsets[i]); put32(data + cursor + 4, (uint32_t)resources[i].size); put32(data + cursor + 8, resources[i].key);
                cursor += 12;
            }
        }
    }
    put32(data + cursor, 0x04666162); put32(data + cursor + 4, (uint32_t)(1024 + count * 12)); put32(data + cursor + 8, (uint32_t)count);
    return data;
}

static int image_tests(void) {
    uint8_t data[4096], top[34];
    SudekiMpModImage image = {0};
    SudekiMpModImageInfo info = {0};
    char error[128];
    size_t size = make_tga(data, 1);
    CHECK(SudekiMpModImageInspect(data, size, &info, error, sizeof(error)));
    CHECK(info.width == 2 && info.height == 2 && info.key_known && info.texture_key == 0x52d377ccu);
    CHECK(SudekiMpModImageDecode(data, size, &image, error, sizeof(error)));
    CHECK(!memcmp(image.rgba, (const uint8_t[]){255,0,0,128, 0,255,0,255, 0,0,255,64, 255,255,255,0}, 16));
    SudekiMpModImageFree(&image);
    make_tga(top, 0); CHECK(SudekiMpModImageInspect(top, sizeof(top), &info, NULL, 0)); CHECK(info.texture_key == 0x52d377ccu);
    CHECK(!SudekiMpModImageInspect(top, 33, &info, error, sizeof(error)));
    top[16] = 16; CHECK(!SudekiMpModImageInspect(top, sizeof(top), &info, NULL, 0));
    /* A TGA's padding/footer can coincide with an SQX tag. Python tries TGA
     * first, so the tag must not change its catalog identity. */
    make_tga(data, 1); memset(data + 34, 0, 2048 - 34);
    put32(data + 2044, 0x02200c23u);
    CHECK(SudekiMpModImageInspect(data, 2048, &info, NULL, 0));
    CHECK(info.kind == SUDEKIMP_MOD_IMAGE_TGA && info.texture_key == 0x52d377ccu);

    /* RLE packets and both row/column origins. */
    memset(data, 0, 26); data[2] = 10; data[16] = 24; data[17] = 0x30; put16(data + 12, 2); put16(data + 14, 1);
    data[18] = 1; memcpy(data + 19, (const uint8_t[]){0,0,255, 0,255,0}, 6);
    CHECK(SudekiMpModImageDecode(data, 25, &image, NULL, 0));
    CHECK(!image.info.key_known && !memcmp(image.rgba, (const uint8_t[]){0,255,0,255, 255,0,0,255}, 8));
    SudekiMpModImageFree(&image);
    data[18] = 0x81; CHECK(SudekiMpModImageDecode(data, 22, &image, NULL, 0)); CHECK(image.rgba[0] == 255 && image.rgba[4] == 255); SudekiMpModImageFree(&image);
    data[18] = 0x82; CHECK(!SudekiMpModImageDecode(data, 22, &image, NULL, 0));
    data[18] = 1; CHECK(!SudekiMpModImageDecode(data, 24, &image, NULL, 0));
    put16(data + 12, 8192); put16(data + 14, 8192); data[18] = 0xff;
    CHECK(!SudekiMpModImageInspect(data, 22, &info, NULL, 0));

    size = make_dds(data, 0x31545844u, 4, 4);
    put16(data + 128, 0xf800); put16(data + 130, 0x07e0); put32(data + 132, 0xe4e4e4e4u);
    CHECK(SudekiMpModImageDecode(data, size, &image, NULL, 0));
    CHECK(!memcmp(image.rgba, (const uint8_t[]){255,0,0,255, 0,255,0,255, 170,85,0,255, 85,170,0,255}, 16));
    CHECK(image.info.texture_key == SudekiMpTexModCrc32(data + 128, 8)); SudekiMpModImageFree(&image);
    put16(data + 128, 0); put16(data + 130, 0xffff); put32(data + 132, 0xffffffffu);
    CHECK(SudekiMpModImageDecode(data, size, &image, NULL, 0)); CHECK(image.rgba[3] == 0); SudekiMpModImageFree(&image);
    CHECK(!SudekiMpModImageDecode(data, size - 1, &image, NULL, 0));
    put32(data + 112, 0x200); CHECK(!SudekiMpModImageInspect(data, size, &info, NULL, 0)); put32(data + 112, 0);
    put32(data + 84, 0x30315844u); CHECK(!SudekiMpModImageInspect(data, size, &info, NULL, 0));
    /* Imports preserve DDS bytes, so every declared mip must be present. */
    size = make_dds(data, 0x31545844u, 4, 4); put32(data + 8, 0xa1007u); put32(data + 28, 3);
    CHECK(!SudekiMpModImageInspect(data, size, &info, NULL, 0));
    memset(data + size, 0, 16);
    CHECK(SudekiMpModImageInspect(data, size + 16, &info, NULL, 0) && info.mip_levels == 3);
    CHECK(!SudekiMpModImageInspect(data, size + 15, &info, NULL, 0));
    put32(data + 28, 4); CHECK(!SudekiMpModImageInspect(data, size + 24, &info, NULL, 0));

    size = make_dds(data, 0x33545844u, 4, 4);
    for (unsigned i = 0; i < 8; ++i) data[128 + i] = (uint8_t)((i * 2) | ((i * 2 + 1) << 4));
    put16(data + 136, 0); put16(data + 138, 0xffff); put32(data + 140, 0xffffffffu);
    CHECK(SudekiMpModImageDecode(data, size, &image, NULL, 0));
    CHECK(image.rgba[0] == 170 && image.rgba[3] == 0 && image.rgba[15 * 4 + 3] == 255); SudekiMpModImageFree(&image);
    size = make_dds(data, 0x35545844u, 4, 4);
    data[128] = 255; data[129] = 0; data[130] = 2;
    put16(data + 136, 0xf800); put16(data + 138, 0x07e0);
    CHECK(SudekiMpModImageDecode(data, size, &image, NULL, 0)); CHECK(image.rgba[3] == 218); SudekiMpModImageFree(&image);
    data[128] = 0; data[129] = 100; data[130] = (uint8_t)(2u | (6u << 3) | (7u << 6)); data[131] = 1;
    CHECK(SudekiMpModImageDecode(data, size, &image, NULL, 0));
    CHECK(image.rgba[3] == 20 && image.rgba[7] == 0 && image.rgba[11] == 255); SudekiMpModImageFree(&image);

    /* Block edge clipping for dimensions smaller than four. */
    size = make_dds(data, 0x31545844u, 1, 2); put16(data + 128, 0xf800);
    CHECK(SudekiMpModImageDecode(data, size, &image, NULL, 0)); CHECK(image.info.width == 1 && image.rgba[4] == 255); SudekiMpModImageFree(&image);
    size = make_dds(data, 21, 2, 2); memcpy(data + 128, (const uint8_t[]){0,0,255,128, 0,255,0,255, 255,0,0,64, 255,255,255,0}, 16);
    CHECK(SudekiMpModImageDecode(data, size, &image, NULL, 0)); CHECK(image.info.texture_key == 0x52d377ccu && image.rgba[0] == 255); SudekiMpModImageFree(&image);
    /* DDS upload hashing omits padding between file rows. */
    memmove(data + 140, data + 136, 8); memset(data + 136, 0xab, 4); memset(data + 148, 0xcd, 4); put32(data + 20, 12);
    CHECK(SudekiMpModImageDecode(data, 152, &image, NULL, 0)); CHECK(image.info.texture_key == 0x52d377ccu && image.rgba[10] == 255); SudekiMpModImageFree(&image);
    put32(data + 20, 4); CHECK(!SudekiMpModImageInspect(data, 152, &info, NULL, 0));
    put32(data + 16, 0); CHECK(!SudekiMpModImageInspect(data, 152, &info, NULL, 0));

    make_sqx(data, 12); CHECK(SudekiMpModImageDecode(data, 2048, &image, NULL, 0));
    CHECK(image.info.kind == SUDEKIMP_MOD_IMAGE_SQX && image.rgba[0] == 255 && image.info.texture_key == SudekiMpTexModCrc32(data, 8)); SudekiMpModImageFree(&image);
    CHECK(!SudekiMpModImageInspect(data, 2047, &info, NULL, 0));
    make_sqx(data, 13); CHECK(!SudekiMpModImageInspect(data, 2048, &info, NULL, 0));
    /* 8-bit palettized SQX: 4x4 + 2x2 indices, then BGRA entries; the key is
     * the A8R8G8B8 expansion of level zero. */
    make_sqx(data, 11); put32(data + 2044, 0x23u | (11u << 8) | (2u << 16) | (2u << 20) | (2u << 24));
    for (unsigned n = 0; n < 20; ++n) data[n] = (uint8_t)(n < 16 ? n : 0);
    for (unsigned n = 0; n < 256; ++n) { data[20 + n * 4] = (uint8_t)n; data[21 + n * 4] = (uint8_t)(2 * n); data[22 + n * 4] = (uint8_t)(3 * n); data[23 + n * 4] = 255; }
    CHECK(SudekiMpModImageDecode(data, 2048, &image, NULL, 0));
    CHECK(image.info.d3d_format == 21 && image.info.width == 4 && image.info.key_known);
    CHECK(image.info.texture_key == SudekiMpTexModCrc32(data + 20, 64));
    CHECK(image.rgba[4] == 3 && image.rgba[5] == 2 && image.rgba[6] == 1 && image.rgba[7] == 255); SudekiMpModImageFree(&image);
    put32(data + 2044, 0x23u | (11u << 8) | (2u << 16) | (10u << 20) | (10u << 24));
    CHECK(!SudekiMpModImageInspect(data, 2048, &info, NULL, 0)); /* palette past the end */
    CHECK(!SudekiMpModImageInspect(NULL, 0, &info, NULL, 0));
    return 0;
}

/* HOM v5 with one texture-name chunk (kind 24): "Hero_spec!1", "Hero!2". */
static void make_hom(uint8_t *data) {
    static const char strings[] = "Hero_spec!1\0Hero!2\0";
    uint8_t *chunk = data + 2048;
    uint32_t chunk_size = 8 + 2 * 4 + (uint32_t)sizeof(strings) - 1;
    memset(data, 0, 4096); memcpy(data, "HOM\x05", 4); put32(data + 4, 1);
    put32(data + 16, (24u << 24) | chunk_size);
    put32(chunk, 2); put32(chunk + 4, (uint32_t)sizeof(strings) - 1);
    put32(chunk + 8, 0); put32(chunk + 12, 12);
    memcpy(chunk + 16, strings, sizeof(strings) - 1);
}
static void count_name(void *context, const char *name) {
    unsigned *seen = (unsigned *)context;
    /* Counts names in the expected order; a wrong name stops the count. */
    if (*seen == 0 ? !strcmp(name, "Hero_spec") : *seen == 1 && !strcmp(name, "Hero")) ++*seen;
    else *seen = 100;
}
static int cancel_now(void *context) { ++*(unsigned *)context; return 1; }
static int archive_tests(void) {
    static uint8_t model[4096];
    uint8_t tga[34], sqx[2048], junk[] = {1,2,3,4}, *data, *bad;
    unsigned seen = 0;
    const char names[] = "Hero!1\0Top.tga\0-Verdana_16\0Actor.HOM\0Hero1001.SQX\0Hero1110.SQX\0";
    SudekiMpModBlob blob = {names, sizeof(names)};
    SudekiMpModArchive archive = {0}, invalid = {0};
    SudekiMpModCatalog catalog = {0};
    SudekiMpModCatalog named = {0};
    SudekiMpModNames name_index = {0};
    char error[128];
    size_t size, table, row, resource_size;
    unsigned cancel_calls = 0, textures = 0, models = 0, unnamed = 0;
    FixtureResource resources[6];
    make_tga(tga, 1); make_sqx(sqx, 12); make_hom(model);
    CHECK(SudekiMpModHomTextureNames(model, sizeof(model), count_name, &seen) == 2 && seen == 2);
    CHECK(SudekiMpModHomTextureNames(junk, sizeof(junk), NULL, NULL) == -1);
    model[16 + 2] = 0xff; CHECK(SudekiMpModHomTextureNames(model, sizeof(model), NULL, NULL) == -1); /* chunk past the end */
    make_hom(model);
    resources[0] = (FixtureResource){tga, sizeof(tga), SudekiMpModResourceKey("Top.tga")};
    resources[1] = (FixtureResource){tga, sizeof(tga), SudekiMpModResourceKey("Verdana_16-0.tga")};
    resources[2] = (FixtureResource){sqx, sizeof(sqx), SudekiMpModResourceKey("Hero.SQX")};
    resources[3] = (FixtureResource){sqx, sizeof(sqx), 0x12345678};
    resources[4] = (FixtureResource){model, sizeof(model), SudekiMpModResourceKey("Actor.HOM")};
    resources[5] = (FixtureResource){sqx, sizeof(sqx), SudekiMpModResourceKey("Hero1001.SQX")};
    CHECK(resources[5].key == SudekiMpModResourceKey("Hero1110.SQX"));
    data = make_baf(resources, 6, &size, &table, &row); CHECK(data);
    bad = (uint8_t *)malloc(size); CHECK(bad); memcpy(bad, data, size);
    CHECK(SudekiMpModArchiveParse(data, size, &archive, error, sizeof(error)) && archive.count == 6);
    CHECK(archive.data == data && !archive.owned_data);
    CHECK(SudekiMpModArchiveResource(&archive, 0, &resource_size) && resource_size > 0);
    CHECK(!SudekiMpModArchiveResource(&archive, 6, &resource_size) && resource_size == 0);
    CHECK(SudekiMpModArchiveResourceByKey(&archive, resources[0].key, &resource_size) == data && resource_size == sizeof(tga));
    CHECK(!SudekiMpModArchiveResourceByKey(&archive, 0x01020304u, &resource_size) && resource_size == 0);
    CHECK(SudekiMpModCatalogBuild(&archive, 1, &blob, 1, &catalog, error, sizeof(error)));
    CHECK(catalog.count == 6);
    for (size_t i = 0; i < catalog.count; ++i) {
        const SudekiMpModCatalogEntry *e = catalog.entries + i;
        if (e->kind == SUDEKIMP_MOD_RESOURCE_TEXTURE) { ++textures; CHECK(e->width == (e->d3d_format == 21 ? 2u : 4u)); }
        else {
            ++models; CHECK(!strcmp(e->name, "Actor.HOM"));
            /* Hero_spec has no texture; Hero!2 resolves to Hero.SQX. */
            CHECK(e->preview_key == SudekiMpModResourceKey("Hero.SQX"));
        }
        if (!e->name[0]) ++unnamed;
        if (e->archive_key == resources[5].key) CHECK(e->name_candidates == 2 && !*e->name);
        if (e->archive_key == resources[1].key) CHECK(!strcmp(e->name, "Verdana_16-0.tga") && e->texture_key == 0x52d377ccu);
        if (e->archive_key == resources[2].key) CHECK(!strcmp(e->name, "Hero.SQX"));
    }
    CHECK(textures == 5 && models == 1 && unnamed == 2);
    /* The three-pass API accepts detached metadata, then yields the same rows
     * while borrowing only one mapped archive's bytes at a time. */
    {
        SudekiMpModArchive metadata = archive;
        metadata.data = NULL; metadata.size = 0;
        CHECK(SudekiMpModNamesBegin(&metadata, 1, &name_index, error, sizeof(error)));
        CHECK(!SudekiMpModCatalogBuildNamed(&archive, &name_index, &named, NULL, NULL, error, sizeof(error)));
        CHECK(SudekiMpModNamesHarvestBlob(&name_index, data, size, NULL, NULL, error, sizeof(error)));
        CHECK(SudekiMpModNamesHarvestBlob(&name_index, blob.data, blob.size, NULL, NULL, error, sizeof(error)));
        SudekiMpModNamesFinish(&name_index);
        CHECK(!SudekiMpModNamesHarvestBlob(&name_index, blob.data, blob.size, NULL, NULL, error, sizeof(error)));
        CHECK(SudekiMpModCatalogBuildNamed(&archive, &name_index, &named, NULL, NULL, error, sizeof(error)));
        CHECK(named.count == catalog.count);
        for (size_t i = 0; i < named.count; ++i) {
            CHECK(named.entries[i].archive_key == catalog.entries[i].archive_key &&
                named.entries[i].texture_key == catalog.entries[i].texture_key &&
                named.entries[i].name_candidates == catalog.entries[i].name_candidates &&
                named.entries[i].kind == catalog.entries[i].kind &&
                !strcmp(named.entries[i].name, catalog.entries[i].name));
        }
        SudekiMpModCatalogFree(&named);
        CHECK(!SudekiMpModCatalogBuildNamed(&archive, &name_index, &named, cancel_now, &cancel_calls, error, sizeof(error)));
        SudekiMpModNamesFree(&name_index);
        CHECK(!name_index.state);
    }
    CHECK(!memcmp(bad, data, size)); /* Parse/catalog never alter the archive. */
    SudekiMpModCatalogFree(&catalog);
    CHECK(!SudekiMpModCatalogBuildEx(&archive, 1, &blob, 1, &catalog, cancel_now, &cancel_calls, error, sizeof(error)));
    CHECK(cancel_calls && !catalog.entries && !catalog.count && strstr(error, "cancelled"));
    CHECK(!SudekiMpModArchiveParse(data, 2059, &invalid, error, sizeof(error)));
    put32(bad + size - 2060, 0); CHECK(!SudekiMpModArchiveParse(bad, size, &invalid, NULL, 0)); memcpy(bad, data, size);
    put32(bad + size - 2056, 1023); CHECK(!SudekiMpModArchiveParse(bad, size, &invalid, NULL, 0)); memcpy(bad, data, size);
    put32(bad + size - 2052, 7); CHECK(!SudekiMpModArchiveParse(bad, size, &invalid, NULL, 0)); memcpy(bad, data, size);
    put32(bad + table, 0xffffffffu); CHECK(!SudekiMpModArchiveParse(bad, size, &invalid, NULL, 0)); memcpy(bad, data, size);
    put32(bad + row, 0xfffffff0u); CHECK(!SudekiMpModArchiveParse(bad, size, &invalid, NULL, 0)); memcpy(bad, data, size);
    bad[row + 8] ^= 1; CHECK(!SudekiMpModArchiveParse(bad, size, &invalid, NULL, 0));
    SudekiMpModArchiveFree(&archive); free(bad); free(data);
    /* Two same-key records are rejected even when both fit their bucket. */
    resources[1].key = resources[0].key;
    data = make_baf(resources, 2, &size, &table, &row); CHECK(data);
    CHECK(!SudekiMpModArchiveParse(data, size, &invalid, error, sizeof(error)) && strstr(error, "Duplicate")); free(data);
    resources[0].key = 1; resources[1].key = 257;
    data = make_baf(resources, 2, &size, &table, &row); CHECK(data);
    put32(data + row + 8, 257); put32(data + row + 20, 1);
    CHECK(!SudekiMpModArchiveParse(data, size, &invalid, error, sizeof(error)) && strstr(error, "Unsorted")); free(data);
    data = make_baf(NULL, 0, &size, &table, &row); CHECK(data);
    CHECK(SudekiMpModArchiveParse(data, size, &archive, NULL, 0) && archive.count == 0);
    CHECK(SudekiMpModCatalogBuild(&archive, 1, NULL, 0, &catalog, NULL, 0) && catalog.count == 0);
    SudekiMpModCatalogFree(&catalog); SudekiMpModArchiveFree(&archive); free(data);
    /* Keep Python's exact explicit texture-extension spelling rule. */
    resources[0] = (FixtureResource){sqx, sizeof(sqx), SudekiMpModResourceKey("Mixed.SQX")};
    data = make_baf(resources, 1, &size, &table, &row); CHECK(data);
    CHECK(SudekiMpModArchiveParse(data, size, &archive, NULL, 0));
    blob.data = "Mixed.sQx"; blob.size = 9;
    CHECK(SudekiMpModCatalogBuild(&archive, 1, &blob, 1, &catalog, NULL, 0));
    CHECK(catalog.count == 1 && !catalog.entries[0].name[0] && !catalog.entries[0].name_candidates);
    SudekiMpModCatalogFree(&catalog); SudekiMpModArchiveFree(&archive); free(data);
    return 0;
}
int SudekiMpModArchiveImageTests(void) { return image_tests() || archive_tests(); }
#ifdef SUDEKIMP_MODDING_STANDALONE
int main(void) { int result = SudekiMpModArchiveImageTests(); if (!result) puts("Mod archive/image tests passed"); return result; }
#endif
