/* Zip/deflate/TPF core: synthetic fixtures from make_modding_zip_fixtures.py. */
#include "modding/mod_zip.h"
#include "modding/mod_manifest.h"
#include "modding_zip_fixtures.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Buffer { uint8_t *bytes; size_t size, capacity; } Buffer;
static int sink(void *context, const void *bytes, size_t size) {
    Buffer *b = (Buffer *)context;
    if (b->size + size > b->capacity) {
        size_t capacity = (b->size + size) * 2u + 64u;
        uint8_t *grown = (uint8_t *)realloc(b->bytes, capacity);
        if (!grown) return 0;
        b->bytes = grown;
        b->capacity = capacity;
    }
    memcpy(b->bytes + b->size, bytes, size);
    b->size += size;
    return 1;
}

int SudekiMpModZipTests(void) {
    uint8_t *out = NULL;
    size_t size = 0, i;
    char error[128];
    /* Deflate: fixed, dynamic and stored blocks. */
    assert(SudekiMpModInflate(fixture_fixed, sizeof(fixture_fixed), 0, 0, &out, &size));
    assert(size == 24 && !memcmp(out, "hello hello hello sudeki", 24));
    free(out);
    assert(SudekiMpModInflate(fixture_dynamic, sizeof(fixture_dynamic), 0, 0, &out, &size));
    assert(size == FIXTURE_TEXT_SIZE && SudekiMpModZipCrc32(0, out, size) == FIXTURE_TEXT_CRC);
    {
        char line[64];
        snprintf(line, sizeof(line), "line %04d: the quick brown fox jumps over the lazy dog\n", 399);
        assert(!memcmp(out + size - strlen(line), line, strlen(line)));
    }
    free(out);
    assert(SudekiMpModInflate(fixture_stored, sizeof(fixture_stored), 0, 0, &out, &size));
    assert(size == 768);
    for (i = 0; i < size; ++i) assert(out[i] == (uint8_t)i);
    free(out);
    /* Truncated input and a size cap are refused. */
    assert(!SudekiMpModInflate(fixture_dynamic, sizeof(fixture_dynamic) / 2u, 0, 0, &out, &size));
    assert(!SudekiMpModInflate(fixture_dynamic, sizeof(fixture_dynamic), 0, 100, &out, &size));

    /* Python-written deflate zip with a folder prefix. */
    {
        SudekiMpModZip zip;
        long ini;
        assert(SudekiMpModZipRead(fixture_zip, sizeof(fixture_zip), NULL, 0, 0, &zip, error, sizeof(error)));
        ini = SudekiMpModZipFind(&zip, "MyMod/mod.ini");
        assert(ini >= 0 && zip.entries[ini].size == 37);
        assert(SudekiMpModZipFind(&zip, "a.png") >= 0); /* unique base name */
        assert(SudekiMpModZipFind(&zip, "nothing.png") < 0);
        SudekiMpModZipFree(&zip);
        assert(!SudekiMpModZipRead(fixture_zip, 10, NULL, 0, 0, &zip, error, sizeof(error)));
    }

    /* Writer -> reader round trip (stored, UTF-8 names, comment). */
    {
        Buffer b = {0};
        SudekiMpModZipWriter w;
        SudekiMpModZip zip;
        SudekiMpModZipWriterBegin(&w, sink, &b);
        assert(SudekiMpModZipWriterAdd(&w, "Größe/mod.ini", "Format=SudekiMP.Mod/1\r\n", 23));
        assert(SudekiMpModZipWriterAdd(&w, "Größe/textures/empty.png", "", 0));
        assert(SudekiMpModZipWriterFinish(&w, "exported"));
        assert(SudekiMpModZipRead(b.bytes, b.size, NULL, 0, 0, &zip, error, sizeof(error)));
        assert(zip.count == 2 && !strcmp(zip.entries[0].name, "Größe/mod.ini") &&
               zip.entries[0].size == 23 && !memcmp(zip.entries[0].data, "Format=", 7));
        assert(zip.entries[1].size == 0 && !strcmp(zip.comment, "exported"));
        SudekiMpModZipFree(&zip);
        free(b.bytes);
    }

    /* TexMod TPF: XOR layer, ZipCrypto + deflate, texmod.def, cp1252 comment. */
    {
        SudekiMpModTpf tpf;
        assert(SudekiMpModTpfRead(fixture_tpf, sizeof(fixture_tpf), &tpf, error, sizeof(error)));
        assert(tpf.count == 2 && tpf.missing == 1);
        assert(tpf.textures[0].key == 0xAC57BC5Du && tpf.textures[0].member >= 0);
        assert(tpf.textures[1].key == 2u && tpf.textures[1].member < 0);
        assert(tpf.zip.count == 1); /* texmod.def removed */
        {
            const SudekiMpModZipEntry *e = &tpf.zip.entries[tpf.textures[0].member];
            assert(e->size == 192 && !strcmp(SudekiMpModImageExtension(e->data, e->size), "dds"));
        }
        assert(!strcmp(tpf.author, "JotaraKujo1988"));
        assert(!strcmp(tpf.description, "A \xe2\x80\x9c" "cooler\xe2\x80\x9d blade"));
        SudekiMpModTpfFree(&tpf);
        /* A plain zip is not a TPF. */
        assert(!SudekiMpModTpfRead(fixture_zip, sizeof(fixture_zip), &tpf, error, sizeof(error)));
    }

    /* Manifest metadata setter. */
    {
        SudekiMpModManifest m = {0};
        char value[64];
        assert(SudekiMpModManifestCreate("Imported", &m));
        assert(SudekiMpModManifestSetMetadata(&m, "Author", "Jotara"));
        assert(SudekiMpModManifestGetValue(&m, "Mod", "Author", value, sizeof(value)) && !strcmp(value, "Jotara"));
        assert(!SudekiMpModManifestSetMetadata(&m, "Format", "x"));
        assert(!SudekiMpModManifestSetMetadata(&m, "Bad Key", "x"));
        assert(!SudekiMpModManifestSetMetadata(&m, "Author", "two\nlines"));
        SudekiMpModManifestFree(&m);
    }
    assert(!strcmp(SudekiMpModImageExtension((const uint8_t *)"\x89PNG\r\n\x1a\n", 8), "png"));
    assert(!strcmp(SudekiMpModImageExtension((const uint8_t *)"BM", 2), "bmp"));
    assert(!strcmp(SudekiMpModImageExtension((const uint8_t *)"\xff\xd8\xff", 3), "jpg"));
    return 0;
}
