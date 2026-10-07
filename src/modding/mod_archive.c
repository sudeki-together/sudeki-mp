#include "modding/mod_archive.h"
#include "modding/mod_image.h"
#include "engine/texture_mod_index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { FOOTER_BYTES = 2060, MAX_RECORDS = 1u << 20, MAX_NAMES = 1u << 20 };
static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static int fail(char *error, size_t capacity, const char *message) {
    if (error && capacity) snprintf(error, capacity, "%s", message);
    return 0;
}
static int compare_key(const void *a, const void *b) {
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

int SudekiMpModArchiveParse(const void *input, size_t size, SudekiMpModArchive *archive, char *error, size_t capacity) {
    const uint8_t *data = (const uint8_t *)input;
    SudekiMpModArchiveRecord *records = NULL;
    uint32_t *keys = NULL, table_size, declared;
    size_t footer, cursor, table_start, count = 0;
    const char *message;
    if (!archive || !data || size < FOOTER_BYTES) return fail(error, capacity, "Truncated BAF");
    footer = size - FOOTER_BYTES;
    if (u32(data + footer) != 0x04666162u) return fail(error, capacity, "Unsupported BAF footer");
    table_size = u32(data + footer + 4); declared = u32(data + footer + 8);
    if (table_size < 1024 || table_size > footer || declared > MAX_RECORDS ||
        table_size != (uint64_t)1024 + (uint64_t)declared * 12)
        return fail(error, capacity, "Invalid BAF index extent or count");
    table_start = cursor = footer - table_size;
    if (declared) {
        records = (SudekiMpModArchiveRecord *)malloc((size_t)declared * sizeof(*records));
        keys = (uint32_t *)malloc((size_t)declared * sizeof(*keys));
        if (!records || !keys) { message = "Not enough memory for BAF index"; goto bad; }
    }
    for (uint32_t bucket = 0; bucket < 256; ++bucket) {
        uint32_t n;
        if (footer - cursor < 4) { message = "Truncated BAF bucket count"; goto bad; }
        n = u32(data + cursor); cursor += 4;
        if (n > declared - count || n > (footer - cursor) / 12) { message = "BAF bucket crosses index extent"; goto bad; }
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t offset = u32(data + cursor), bytes = u32(data + cursor + 4), key = u32(data + cursor + 8);
            cursor += 12;
            if ((uint64_t)offset + bytes > table_start || (key & 255u) != bucket) { message = "Invalid BAF resource extent or bucket"; goto bad; }
            if (i && records[count - 1].key >= key) {
                message = records[count - 1].key == key ? "Duplicate BAF resource key" : "Unsorted BAF resource bucket"; goto bad;
            }
            records[count].offset = offset; records[count].size = bytes; records[count].key = key;
            keys[count++] = key;
        }
    }
    if (cursor != footer || count != declared) { message = "BAF index extent/count mismatch"; goto bad; }
    if (count > 1) qsort(keys, count, sizeof(*keys), compare_key);
    for (size_t i = 1; i < count; ++i) if (keys[i] == keys[i - 1]) { message = "Duplicate BAF resource key"; goto bad; }
    free(keys);
    archive->data = data; archive->size = size; archive->records = records; archive->count = count; archive->owned_data = NULL;
    if (error && capacity) *error = 0;
    return 1;
bad:
    free(records); free(keys); return fail(error, capacity, message);
}
int SudekiMpModArchiveOpen(const char *path, SudekiMpModArchive *archive, char *error, size_t capacity) {
    FILE *stream;
    long length;
    uint8_t *data;
    if (!path || !archive) return fail(error, capacity, "No archive path or output");
    stream = fopen(path, "rb");
    if (!stream) return fail(error, capacity, "Cannot open BAF");
    if (fseek(stream, 0, SEEK_END) || (length = ftell(stream)) < 0 || fseek(stream, 0, SEEK_SET)) {
        fclose(stream); return fail(error, capacity, "Cannot determine BAF size");
    }
    if ((unsigned long)length < FOOTER_BYTES) { fclose(stream); return fail(error, capacity, "Truncated BAF"); }
    data = (uint8_t *)malloc((size_t)length);
    if (!data) { fclose(stream); return fail(error, capacity, "Not enough memory for BAF"); }
    if (fread(data, 1, (size_t)length, stream) != (size_t)length) {
        fclose(stream); free(data); return fail(error, capacity, "Cannot read BAF");
    }
    fclose(stream);
    if (!SudekiMpModArchiveParse(data, (size_t)length, archive, error, capacity)) { free(data); return 0; }
    archive->owned_data = data; return 1;
}
const uint8_t *SudekiMpModArchiveResource(const SudekiMpModArchive *archive, size_t index, size_t *size) {
    const SudekiMpModArchiveRecord *record;
    if (size) *size = 0;
    if (!archive || index >= archive->count || !archive->data || !archive->records) return NULL;
    record = archive->records + index;
    if ((uint64_t)record->offset + record->size > archive->size) return NULL;
    if (size) *size = record->size;
    return archive->data + record->offset;
}
const uint8_t *SudekiMpModArchiveResourceByKey(const SudekiMpModArchive *archive, uint32_t key, size_t *size) {
    size_t footer, cursor;
    if (size) *size = 0;
    if (!archive || !archive->data || archive->size < FOOTER_BYTES) return NULL;
    footer = archive->size - FOOTER_BYTES;
    if (u32(archive->data + footer + 4) > footer) return NULL;
    cursor = footer - u32(archive->data + footer + 4);
    for (uint32_t bucket = 0; bucket <= (key & 255u); ++bucket) {
        uint32_t count;
        if (cursor > footer || footer - cursor < 4) return NULL;
        count = u32(archive->data + cursor); cursor += 4;
        if (count > (footer - cursor) / 12) return NULL;
        if (bucket == (key & 255u)) {
            long found = SudekiMpArchiveBucketFind(archive->data + cursor, count, key);
            const uint8_t *row;
            uint32_t offset, bytes;
            if (found < 0) return NULL;
            row = archive->data + cursor + (size_t)found * 12;
            offset = u32(row); bytes = u32(row + 4);
            if ((uint64_t)offset + bytes > archive->size) return NULL;
            if (size) *size = bytes;
            return archive->data + offset;
        }
        cursor += (size_t)count * 12;
    }
    return NULL;
}
void SudekiMpModArchiveFree(SudekiMpModArchive *archive) {
    if (!archive) return;
    free(archive->records); free(archive->owned_data); memset(archive, 0, sizeof(*archive));
}

typedef struct Candidate { uint32_t key; char name[SUDEKIMP_MOD_RESOURCE_NAME_MAX]; } Candidate;
typedef struct Harvest {
    uint32_t *keys;
    size_t key_count;
    Candidate *names;
    size_t count, capacity;
    SudekiMpModCancelCheck cancel;
    void *cancel_context;
    int cancelled;
    int finished;
} Harvest;
static int cancelled(Harvest *h) {
    if (h->cancel && h->cancel(h->cancel_context)) h->cancelled = 1;
    return h->cancelled;
}
static unsigned char upper(unsigned char c) { return c >= 'a' && c <= 'z' ? (unsigned char)(c - 'a' + 'A') : c; }
static int alpha(unsigned char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
static int digit(unsigned char c) { return c >= '0' && c <= '9'; }
static int word(unsigned char c) { return alpha(c) || digit(c) || c == '_'; }
static int stem_char(unsigned char c) { return word(c) || c == '-'; }
static int key_exists(const Harvest *h, uint32_t key) {
    size_t lo = 0, hi = h->key_count;
    while (lo < hi) { size_t mid = lo + (hi - lo) / 2; if (h->keys[mid] < key) lo = mid + 1; else hi = mid; }
    return lo < h->key_count && h->keys[lo] == key;
}
static int add_name(Harvest *h, const uint8_t *stem, size_t length, const char *suffix) {
    Candidate c = {0};
    size_t suffix_length = strlen(suffix), wanted;
    if (!length || length + suffix_length >= sizeof(c.name)) return 1;
    memcpy(c.name, stem, length); memcpy(c.name + length, suffix, suffix_length + 1);
    c.key = SudekiMpModResourceKey(c.name);
    if (!key_exists(h, c.key)) return 1;
    if (h->count == MAX_NAMES) return 0;
    if (h->count == h->capacity) {
        Candidate *names;
        wanted = h->capacity ? h->capacity * 2 : 128;
        names = (Candidate *)realloc(h->names, wanted * sizeof(*names));
        if (!names) return 0;
        h->names = names; h->capacity = wanted;
    }
    h->names[h->count++] = c; return 1;
}
static int extension(const uint8_t *p, const char *ext) {
    return upper(p[0]) == (unsigned char)ext[0] && upper(p[1]) == (unsigned char)ext[1] && upper(p[2]) == (unsigned char)ext[2];
}
static int texture_extension(const uint8_t *p, const char *ext) {
    /* Python's explicit-name pattern permits all-upper or all-lower texture
     * extensions; preserving this prevents extra spellings resolving a key. */
    return !memcmp(p, ext, 3) ||
        (p[0] == (uint8_t)(ext[0] + 'a' - 'A') && p[1] == (uint8_t)(ext[1] + 'a' - 'A') && p[2] == (uint8_t)(ext[2] + 'a' - 'A'));
}
static int harvest_blob(Harvest *h, const void *input, size_t size) {
    const uint8_t *data = (const uint8_t *)input;
    if (!data && size) return 0;
    for (size_t i = 0; i < size; ++i) {
        if (!(i & 65535u) && cancelled(h)) return 0;
        /* Explicit names may appear after arbitrary binary bytes. Match the
         * same 2..120-character stem as the Python catalog, plus .HOM. */
        if (data[i] == '.' && size - i >= 4 &&
            (texture_extension(data + i + 1, "TGA") || texture_extension(data + i + 1, "SQX") || extension(data + i + 1, "HOM")) &&
            (size - i == 4 || !word(data[i + 4]))) {
            size_t start = i;
            while (start && i - start < 120 && stem_char(data[start - 1])) --start;
            if (i - start >= 2 && !add_name(h, data + start, i - start + 4, "")) return 0;
        }
        if (!alpha(data[i])) continue;
        /* Font descriptors store Verdana_16\0; four possible page names. */
        if (!i || !word(data[i - 1])) {
            size_t end = i;
            while (end < size && alpha(data[end]) && end - i <= 32) ++end;
            if (end - i >= 3 && end - i <= 32 && end < size && data[end] == '_') {
                size_t number = ++end;
                while (end < size && digit(data[end]) && end - number <= 2) ++end;
                if (end - number >= 1 && end - number <= 2 && end < size && !data[end]) {
                    for (unsigned page = 0; page < 4; ++page) {
                        char suffix[16]; snprintf(suffix, sizeof(suffix), "-%u.tga", page);
                        if (!add_name(h, data + i, end - i, suffix)) return 0;
                    }
                }
            }
        }
        if (i && stem_char(data[i - 1])) continue;
        /* Material references omit their extension, sometimes followed by !N.
         * A dot following the stem excludes a bare match, as in Python. */
        {
            size_t end = i;
            while (end < size && stem_char(data[end]) && end - i <= 120) ++end;
            if (end - i >= 3 && end - i <= 120 && (end == size || (!stem_char(data[end]) && data[end] != '.'))) {
                if (!add_name(h, data + i, end - i, ".SQX") || !add_name(h, data + i, end - i, ".TGA")) return 0;
            }
        }
    }
    return !cancelled(h);
}
static int name_compare_ci(const char *a, const char *b) {
    while (*a && *b) { unsigned char x = upper((unsigned char)*a++), y = upper((unsigned char)*b++); if (x != y) return (x > y) - (x < y); }
    return (*a != 0) - (*b != 0);
}
static int candidate_compare(const void *a, const void *b) {
    const Candidate *x = (const Candidate *)a, *y = (const Candidate *)b;
    int order = (x->key > y->key) - (x->key < y->key);
    if (!order) order = name_compare_ci(x->name, y->name);
    if (!order) order = strcmp(x->name, y->name); /* Python keeps lexicographically first spelling. */
    return order;
}
static size_t names_for_key(const Harvest *h, uint32_t key, unsigned *count) {
    size_t lo = 0, hi = h->count, first, end;
    while (lo < hi) { size_t mid = lo + (hi - lo) / 2; if (h->names[mid].key < key) lo = mid + 1; else hi = mid; }
    first = end = lo;
    while (end < h->count && h->names[end].key == key) ++end;
    *count = (unsigned)(end - first); return first;
}
static int hom_name(const char *name) {
    size_t length = strlen(name);
    return length >= 4 && name[length - 4] == '.' && extension((const uint8_t *)name + length - 3, "HOM");
}
static int catalog_texture(const uint8_t *data, size_t size) {
    /* Do not allocate/decode previews for unrelated archive resources. The
     * Python catalog recognizes only raw 32-bit TGA or aligned SQX. */
    return data && ((size >= 18 && !data[1] && data[2] == 2 && data[16] == 32) ||
        (size >= 2048 && !(size % 2048) && (u32(data + size - 4) & 255u) == 0x23u));
}

int SudekiMpModNamesBegin(const SudekiMpModArchive *archives, size_t archive_count,
    SudekiMpModNames *names, char *error, size_t capacity) {
    Harvest *h;
    size_t total = 0;
    if (!names || (!archives && archive_count)) return fail(error, capacity, "Invalid name-index input");
    for (size_t a = 0; a < archive_count; ++a) {
        if (archives[a].count > MAX_RECORDS || total > MAX_RECORDS - archives[a].count ||
            (!archives[a].records && archives[a].count)) return fail(error, capacity, "Invalid or excessive name-index records");
        total += archives[a].count;
    }
    h = (Harvest *)calloc(1, sizeof(*h));
    if (!h) return fail(error, capacity, "Not enough memory for name index");
    if (total) {
        h->keys = (uint32_t *)malloc(total * sizeof(*h->keys));
        if (!h->keys) { free(h); return fail(error, capacity, "Not enough memory for archive keys"); }
        for (size_t a = 0; a < archive_count; ++a) for (size_t r = 0; r < archives[a].count; ++r) h->keys[h->key_count++] = archives[a].records[r].key;
    }
    if (h->key_count > 1) qsort(h->keys, h->key_count, sizeof(*h->keys), compare_key);
    names->state = h;
    if (error && capacity) *error = 0;
    return 1;
}
int SudekiMpModNamesHarvestBlob(SudekiMpModNames *names, const void *data, size_t size,
    SudekiMpModCancelCheck cancel, void *context, char *error, size_t capacity) {
    Harvest *h;
    if (!names || !(h = (Harvest *)names->state) || h->finished || (!data && size)) return fail(error, capacity, "Invalid or finished name index");
    h->cancel = cancel; h->cancel_context = context; h->cancelled = 0;
    if (!harvest_blob(h, data, size)) return fail(error, capacity, h->cancelled ? "Catalog scan cancelled" : "Too many resource names or insufficient memory");
    if (error && capacity) *error = 0;
    return 1;
}
void SudekiMpModNamesFinish(SudekiMpModNames *names) {
    Harvest *h;
    size_t unique = 0;
    if (!names || !(h = (Harvest *)names->state) || h->finished) return;
    if (h->count > 1) qsort(h->names, h->count, sizeof(*h->names), candidate_compare);
    for (size_t n = 0; n < h->count; ++n) {
        if (unique && h->names[n].key == h->names[unique - 1].key && !name_compare_ci(h->names[n].name, h->names[unique - 1].name)) continue;
        h->names[unique++] = h->names[n];
    }
    h->count = unique; h->finished = 1;
}
void SudekiMpModNamesFree(SudekiMpModNames *names) {
    Harvest *h;
    if (!names || !(h = (Harvest *)names->state)) return;
    free(h->keys); free(h->names); free(h); names->state = NULL;
}
int SudekiMpModCatalogBuildNamed(const SudekiMpModArchive *archive,
    const SudekiMpModNames *names, SudekiMpModCatalog *catalog,
    SudekiMpModCancelCheck cancel, void *context, char *error, size_t capacity) {
    Harvest h;
    SudekiMpModCatalogEntry *entries = NULL;
    size_t used = 0;
    if (!archive || !archive->data || (archive->count && !archive->records) ||
        archive->count > MAX_RECORDS || !names || !names->state || !catalog)
        return fail(error, capacity, "Invalid named-catalog input");
    h = *(const Harvest *)names->state;
    if (!h.finished) return fail(error, capacity, "Finish names before building a catalog");
    h.cancel = cancel; h.cancel_context = context; h.cancelled = 0;
    if (archive->count) {
        entries = (SudekiMpModCatalogEntry *)calloc(archive->count, sizeof(*entries));
        if (!entries) return fail(error, capacity, "Not enough memory for resource catalog");
    }
    for (size_t r = 0; r < archive->count; ++r) {
        SudekiMpModImageInfo info = {0};
        SudekiMpModCatalogEntry entry = {0};
        size_t bytes, first;
        const uint8_t *payload = SudekiMpModArchiveResource(archive, r, &bytes);
        unsigned candidates;
        uint32_t key = archive->records[r].key;
        if (cancelled(&h)) { free(entries); return fail(error, capacity, "Catalog scan cancelled"); }
        first = names_for_key(&h, key, &candidates);
        entry.resource_index = r; entry.archive_key = key; entry.name_candidates = candidates;
        if (catalog_texture(payload, bytes) && SudekiMpModImageInspect(payload, bytes, &info, NULL, 0) && info.key_known &&
            (info.kind == SUDEKIMP_MOD_IMAGE_SQX ||
             (info.kind == SUDEKIMP_MOD_IMAGE_TGA && bytes >= 18 && payload[2] == 2 && payload[16] == 32))) {
            entry.kind = SUDEKIMP_MOD_RESOURCE_TEXTURE; entry.texture_key = info.texture_key;
            entry.width = info.width; entry.height = info.height; entry.d3d_format = info.d3d_format;
        } else if (candidates == 1 && hom_name(h.names[first].name)) entry.kind = SUDEKIMP_MOD_RESOURCE_MODEL;
        else continue;
        if (candidates == 1) memcpy(entry.name, h.names[first].name, sizeof(entry.name));
        entries[used++] = entry;
    }
    catalog->entries = entries; catalog->count = used;
    if (error && capacity) *error = 0;
    return 1;
}

int SudekiMpModCatalogBuildEx(const SudekiMpModArchive *archives, size_t archive_count,
    const SudekiMpModBlob *extra, size_t extra_count, SudekiMpModCatalog *catalog,
    SudekiMpModCancelCheck cancel, void *cancel_context, char *error, size_t capacity) {
    Harvest h = {0};
    SudekiMpModCatalogEntry *entries = NULL;
    size_t total = 0, used = 0;
    const char *message = "Not enough memory for resource catalog";
    if (!catalog || (!archives && archive_count) || (!extra && extra_count)) return fail(error, capacity, "Invalid catalog input");
    h.cancel = cancel; h.cancel_context = cancel_context;
    for (size_t a = 0; a < archive_count; ++a) {
        if (archives[a].count > MAX_RECORDS || total > MAX_RECORDS - archives[a].count ||
            (!archives[a].records && archives[a].count) || !archives[a].data) {
            message = "Invalid or excessive catalog archive records"; goto bad;
        }
        total += archives[a].count;
    }
    if (total) {
        h.keys = (uint32_t *)malloc(total * sizeof(*h.keys));
        entries = (SudekiMpModCatalogEntry *)calloc(total, sizeof(*entries));
        if (!h.keys || !entries) goto bad;
        for (size_t a = 0; a < archive_count; ++a) for (size_t r = 0; r < archives[a].count; ++r) h.keys[h.key_count++] = archives[a].records[r].key;
    }
    if (h.key_count > 1) qsort(h.keys, h.key_count, sizeof(*h.keys), compare_key);
    for (size_t a = 0; a < archive_count; ++a) if (!harvest_blob(&h, archives[a].data, archives[a].size)) goto bad;
    for (size_t b = 0; b < extra_count; ++b) if (!harvest_blob(&h, extra[b].data, extra[b].size)) goto bad;
    if (h.count > 1) qsort(h.names, h.count, sizeof(*h.names), candidate_compare);
    /* Remove duplicate spellings before counting distinct names for each key. */
    {
        size_t unique = 0;
        for (size_t n = 0; n < h.count; ++n) {
            if (unique && h.names[n].key == h.names[unique - 1].key && !name_compare_ci(h.names[n].name, h.names[unique - 1].name)) continue;
            h.names[unique++] = h.names[n];
        }
        h.count = unique;
    }
    for (size_t a = 0; a < archive_count; ++a) {
        for (size_t r = 0; r < archives[a].count; ++r) {
            SudekiMpModImageInfo info = {0};
            SudekiMpModCatalogEntry entry = {0};
            size_t bytes, first;
            const uint8_t *payload = SudekiMpModArchiveResource(archives + a, r, &bytes);
            unsigned candidates;
            uint32_t key = archives[a].records[r].key;
            if (cancelled(&h)) goto bad;
            first = names_for_key(&h, key, &candidates);
            entry.archive_index = a; entry.resource_index = r; entry.archive_key = key; entry.name_candidates = candidates;
            /* Exact Python texture catalog contract: raw 32-bit TGA, or SQX.
             * DDS/RLE replacements are decoded but are not guessed originals. */
            if (catalog_texture(payload, bytes) && SudekiMpModImageInspect(payload, bytes, &info, NULL, 0) && info.key_known &&
                (info.kind == SUDEKIMP_MOD_IMAGE_SQX ||
                 (info.kind == SUDEKIMP_MOD_IMAGE_TGA && bytes >= 18 && payload[2] == 2 && payload[16] == 32))) {
                entry.kind = SUDEKIMP_MOD_RESOURCE_TEXTURE; entry.texture_key = info.texture_key;
                entry.width = info.width; entry.height = info.height; entry.d3d_format = info.d3d_format;
            } else if (candidates == 1 && hom_name(h.names[first].name)) entry.kind = SUDEKIMP_MOD_RESOURCE_MODEL;
            else continue;
            if (candidates == 1) memcpy(entry.name, h.names[first].name, sizeof(entry.name));
            entries[used++] = entry;
        }
    }
    free(h.keys); free(h.names);
    catalog->entries = entries; catalog->count = used;
    if (error && capacity) *error = 0;
    return 1;
bad:
    if (h.cancelled) message = "Catalog scan cancelled";
    free(entries); free(h.keys); free(h.names); return fail(error, capacity, message);
}
int SudekiMpModCatalogBuild(const SudekiMpModArchive *archives, size_t archive_count,
    const SudekiMpModBlob *extra, size_t extra_count, SudekiMpModCatalog *catalog, char *error, size_t capacity) {
    return SudekiMpModCatalogBuildEx(archives, archive_count, extra, extra_count, catalog, NULL, NULL, error, capacity);
}
void SudekiMpModCatalogFree(SudekiMpModCatalog *catalog) {
    if (!catalog) return;
    free(catalog->entries); memset(catalog, 0, sizeof(*catalog));
}
