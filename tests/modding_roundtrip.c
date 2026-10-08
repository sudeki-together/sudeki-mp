/* Synthetic interoperability helper; not shipped in the launcher package. */
#include "modding/mod_archive.h"
#include "modding/mod_image.h"
#include "modding/mod_manifest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    long length;
    unsigned char *data;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) || (length = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) { fclose(f); return NULL; }
    data = malloc((size_t)length + 1);
    if (!data || fread(data, 1, (size_t)length, f) != (size_t)length) { free(data); fclose(f); return NULL; }
    fclose(f); *size = (size_t)length;
    return data;
}
int main(int argc, char **argv) {
    int ok = 0;
    if (argc >= 3 && !strcmp(argv[1], "catalog")) {
        SudekiMpModArchive archive = {0};
        SudekiMpModCatalog catalog = {0};
        SudekiMpModBlob blob = {0};
        unsigned char *data = NULL;
        char error[160];
        size_t i;
        if (argc == 4) { data = read_file(argv[3], &blob.size); blob.data = data; if (!data) return 1; }
        if (SudekiMpModArchiveOpen(argv[2], &archive, error, sizeof(error)) &&
            SudekiMpModCatalogBuild(&archive, 1, &blob, data ? 1 : 0, &catalog, error, sizeof(error))) {
            for (i = 0; i < catalog.count; ++i) {
                const SudekiMpModCatalogEntry *e = &catalog.entries[i];
                if (e->kind == SUDEKIMP_MOD_RESOURCE_TEXTURE)
                    printf("0x%08X\t%s\t0x%08X\t%u\t%u\t%s\n", e->texture_key, e->name,
                           e->archive_key, e->width, e->height, SudekiMpModImageFormatName(e->d3d_format));
            }
            ok = 1;
        } else fprintf(stderr, "%s\n", error);
        SudekiMpModCatalogFree(&catalog); SudekiMpModArchiveFree(&archive); free(data);
    } else if (argc == 4 && !strcmp(argv[1], "manifest")) {
        SudekiMpModManifest manifest = {0};
        unsigned char *data, *encoded = NULL;
        size_t size = 0, encoded_size = 0;
        FILE *f;
        data = read_file(argv[2], &size);
        if (data && SudekiMpModManifestLoad(data, size, &manifest) &&
            SudekiMpModManifestSetTexture(&manifest, 0x12, "textures/new.tga") &&
            SudekiMpModManifestSetFile(&manifest, "BAR.HOM", "files/new.hom") &&
            SudekiMpModManifestSetEnabled(&manifest, 0) &&
            SudekiMpModManifestEncode(&manifest, &encoded, &encoded_size)) {
            f = fopen(argv[3], "wb");
            if (f) { ok = fwrite(encoded, 1, encoded_size, f) == encoded_size; if (fclose(f)) ok = 0; }
        }
        SudekiMpModManifestFree(&manifest); free(data); free(encoded);
    }
    return ok ? 0 : 1;
}
