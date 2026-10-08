#include "engine/texture_mod_index.h"
#include <stdlib.h>
#include <string.h>

enum { MAX_ENTRIES = 1u << 20, MAX_POOL = 64u << 20, MAX_DIMENSION = 16384 };

static uint32_t crc_table[256];
static int crc_ready;

static void crc_init(void) {
    for (uint32_t i = 0; i < 256u; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) c = (c & 1u) ? 0xedb88320u ^ (c >> 1) : c >> 1;
        crc_table[i] = c;
    }
    crc_ready = 1;
}

uint32_t SudekiMpTexModCrc32(const void *data, size_t size) {
    const unsigned char *p = (const unsigned char *)data;
    uint32_t crc = 0xffffffffu;
    if (!crc_ready) crc_init();
    while (size--) crc = crc_table[(crc ^ *p++) & 0xffu] ^ (crc >> 8);
    return crc; /* TexMod leaves out the final inversion */
}

unsigned SudekiMpTexModFormatBits(uint32_t format) {
    switch (format) {
    case 21: case 22: case 32: case 31: case 35: case 33: case 34: return 32; /* A8R8G8B8 X8R8G8B8 A8B8G8R8 A2R10G10B10 X8B8G8R8 A2B10G10R10 G16R16 */
    case 20: return 24;                                                       /* R8G8B8 */
    case 23: case 24: case 25: case 26: case 29: case 30: case 51: return 16; /* R5G6B5 X1R5G5B5 A1R5G5B5 A4R4G4B4 X4R4G4B4 A8R3G3B2 A8L8 */
    case 27: case 28: case 50: case 52: return 8;                             /* R3G3B2 A8 L8 A4L4 */
    case 0x31545844u: return 4;                                               /* DXT1 */
    case 0x32545844u: case 0x33545844u: case 0x34545844u: case 0x35545844u: return 8; /* DXT2..5 */
    default: return 0;
    }
}

size_t SudekiMpTexModLevelBytes(uint32_t format, uint32_t width, uint32_t height) {
    unsigned bits = SudekiMpTexModFormatBits(format);
    if (!bits || !width || !height || width > MAX_DIMENSION || height > MAX_DIMENSION) return 0;
    return (size_t)bits * width * height / 8u;
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int SudekiMpTextureModParseKey(const char *text, uint32_t *key) {
    uint32_t value = 0;
    size_t digits = 0;
    if (!text || !key || text[0] != '0' || (text[1] != 'x' && text[1] != 'X')) return 0;
    for (const char *p = text + 2; *p; ++p, ++digits) {
        int d = hex_digit(*p);
        if (d < 0 || digits >= 8) return 0;
        value = (value << 4) | (uint32_t)d;
    }
    if (!digits) return 0;
    *key = value;
    return 1;
}

int SudekiMpTextureModSafePath(const char *path) {
    size_t length, component = 0;
    if (!path || !*path) return 0;
    length = strlen(path);
    if (length >= SUDEKIMP_TEXTURE_MOD_PATH_MAX || path[0] == '/' || path[0] == '\\') return 0;
    for (size_t i = 0; i <= length; ++i) {
        unsigned char c = (unsigned char)path[i];
        if (c == 0 || c == '/' || c == '\\') {
            const char *start = path + i - component;
            if (!component || (component == 1 && start[0] == '.') ||
                (component == 2 && start[0] == '.' && start[1] == '.')) return 0;
            component = 0;
            continue;
        }
        if (c < 32 || c == 127 || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return 0;
        ++component;
    }
    return 1;
}

static int is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

int SudekiMpTextureModParseLine(const char *line, uint32_t *key, char *path, size_t capacity) {
    char name[16];
    const char *a, *eq, *b, *end;
    size_t n;
    if (!line || !key || !path || !capacity) return -1;
    a = line;
    while (is_space(*a)) ++a;
    if (!*a || *a == ';' || *a == '#') return 0;
    eq = strchr(a, '=');
    if (!eq) return -1;
    n = (size_t)(eq - a);
    while (n && is_space(a[n - 1])) --n;
    if (!n || n >= sizeof(name)) return -1;
    memcpy(name, a, n);
    name[n] = 0;
    if (!SudekiMpTextureModParseKey(name, key)) return -1;
    b = eq + 1;
    while (is_space(*b)) ++b;
    end = b + strlen(b);
    while (end > b && is_space(end[-1])) --end;
    n = (size_t)(end - b);
    if (!n || n >= capacity || n >= SUDEKIMP_TEXTURE_MOD_PATH_MAX) return -1;
    memcpy(path, b, n);
    path[n] = 0;
    return SudekiMpTextureModSafePath(path) ? 1 : -1;
}

int SudekiMpTextureModIndexAdd(SudekiMpTextureModIndex *index, uint32_t key, uint32_t mod, const char *path) {
    size_t length;
    if (!index || index->finished || !SudekiMpTextureModSafePath(path)) return 0;
    length = strlen(path) + 1u;
    if (index->count >= MAX_ENTRIES || index->pool_used + length > MAX_POOL) return 0;
    if (index->count == index->capacity) {
        size_t capacity = index->capacity ? index->capacity * 2u : 64u;
        void *p = realloc(index->entries, capacity * sizeof(*index->entries));
        if (!p) return 0;
        index->entries = (SudekiMpTextureModEntry *)p;
        index->capacity = capacity;
    }
    if (index->pool_used + length > index->pool_capacity) {
        size_t capacity = index->pool_capacity ? index->pool_capacity : 4096u;
        void *p;
        while (capacity < index->pool_used + length) capacity *= 2u;
        p = realloc(index->pool, capacity);
        if (!p) return 0;
        index->pool = (char *)p;
        index->pool_capacity = capacity;
    }
    memcpy(index->pool + index->pool_used, path, length);
    index->entries[index->count].key = key;
    index->entries[index->count].mod = mod;
    index->entries[index->count].sequence = (uint32_t)index->count;
    index->entries[index->count].path = (uint32_t)index->pool_used;
    index->pool_used += length;
    ++index->count;
    return 1;
}

static int compare_entries(const void *a, const void *b) {
    const SudekiMpTextureModEntry *x = (const SudekiMpTextureModEntry *)a, *y = (const SudekiMpTextureModEntry *)b;
    if (x->key != y->key) return x->key < y->key ? -1 : 1;
    return x->sequence < y->sequence ? -1 : x->sequence > y->sequence;
}

void SudekiMpTextureModIndexFinish(SudekiMpTextureModIndex *index) {
    size_t out = 0;
    if (!index || index->finished) return;
    if (index->count) qsort(index->entries, index->count, sizeof(*index->entries), compare_entries);
    for (size_t i = 0; i < index->count; ++i) {
        if (i + 1u < index->count && index->entries[i + 1u].key == index->entries[i].key) {
            ++index->overridden;
            continue;
        }
        index->entries[out++] = index->entries[i];
    }
    index->count = out;
    index->finished = 1;
}

const SudekiMpTextureModEntry *SudekiMpTextureModIndexFind(const SudekiMpTextureModIndex *index, uint32_t key) {
    size_t lo = 0, hi;
    if (!index || !index->finished) return NULL;
    hi = index->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2u;
        if (index->entries[mid].key == key) return &index->entries[mid];
        if (index->entries[mid].key < key) lo = mid + 1u; else hi = mid;
    }
    return NULL;
}

const char *SudekiMpTextureModIndexPath(const SudekiMpTextureModIndex *index, const SudekiMpTextureModEntry *entry) {
    return index && entry ? index->pool + entry->path : NULL;
}

void SudekiMpTextureModIndexFree(SudekiMpTextureModIndex *index) {
    if (!index) return;
    free(index->entries);
    free(index->pool);
    memset(index, 0, sizeof(*index));
}

uint32_t SudekiMpModResourceKey(const char *name) {
    uint32_t value = 0;
    for (unsigned i = 0; name[i]; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 'A');
        value = (i & 1u) ? value * c : value + c;
    }
    return value;
}

static int resource_name_valid(const char *name) {
    size_t n = strlen(name), dots = 0;
    if (!n || n > SUDEKIMP_MOD_NAME_MAX || name[0] == '.' || name[n - 1] == '.') return 0;
    for (size_t i = 0; i < n; ++i) {
        char c = name[i];
        if (c == '.') ++dots;
        else if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    }
    return dots == 1;
}

int SudekiMpModFileParseLine(const char *line, uint32_t *key, char *name, size_t name_capacity,
    char *path, size_t path_capacity) {
    const char *a, *eq, *b, *end;
    size_t n;
    if (!line || !key || !name || !path || !name_capacity || !path_capacity) return -1;
    a = line;
    while (is_space(*a)) ++a;
    if (!*a || *a == ';' || *a == '#') return 0;
    eq = strchr(a, '=');
    if (!eq) return -1;
    n = (size_t)(eq - a);
    while (n && is_space(a[n - 1])) --n;
    if (!n || n >= name_capacity || n > SUDEKIMP_MOD_NAME_MAX) return -1;
    memcpy(name, a, n);
    name[n] = 0;
    if (strchr(name, '/') || strchr(name, '\\')) {
        if (!SudekiMpModLooseNameValid(name)) return -1;
        for (char *c = name; *c; ++c) if (*c == '/') *c = '\\';
        *key = 0; /* loose file: matched by path, not archive key */
    } else if (!SudekiMpTextureModParseKey(name, key)) {
        if (!resource_name_valid(name)) return -1;
        *key = SudekiMpModResourceKey(name);
    }
    b = eq + 1;
    while (is_space(*b)) ++b;
    end = b + strlen(b);
    while (end > b && is_space(end[-1])) --end;
    n = (size_t)(end - b);
    if (!n || n >= path_capacity || n >= SUDEKIMP_TEXTURE_MOD_PATH_MAX) return -1;
    memcpy(path, b, n);
    path[n] = 0;
    return SudekiMpTextureModSafePath(path) ? 1 : -1;
}

static uint32_t row_key(const uint8_t *row) {
    return (uint32_t)row[8] | (uint32_t)row[9] << 8 | (uint32_t)row[10] << 16 | (uint32_t)row[11] << 24;
}

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

long SudekiMpArchiveBucketFind(const uint8_t *rows, uint32_t count, uint32_t key) {
    uint32_t lo = 0, hi = count;
    if (!rows) return -1;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u, k = row_key(rows + mid * 12u);
        if (k == key) return (long)mid;
        if (k < key) lo = mid + 1u; else hi = mid;
    }
    return -1;
}

void SudekiMpArchiveBucketInsert(uint8_t *dst, const uint8_t *src, uint32_t count,
    uint32_t offset, uint32_t size, uint32_t key) {
    uint32_t at = 0;
    while (at < count && row_key(src + at * 12u) < key) ++at;
    if (at) memcpy(dst, src, at * 12u);
    put32(dst + at * 12u, offset);
    put32(dst + at * 12u + 4u, size);
    put32(dst + at * 12u + 8u, key);
    if (count > at) memcpy(dst + (at + 1u) * 12u, src + at * 12u, (count - at) * 12u);
}

static int loose_prefix(const char *p) {
    static const char *const folders[] = {"sound", "movies"};
    for (unsigned f = 0; f < 2u; ++f) {
        size_t n = strlen(folders[f]), i;
        for (i = 0; i < n; ++i) {
            char c = p[i];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            if (c != folders[f][i]) break;
        }
        if (i == n && (p[n] == '\\' || p[n] == '/')) return (int)n + 1;
    }
    return 0;
}

int SudekiMpModLooseNameValid(const char *name) {
    int prefix;
    if (!name || !(prefix = loose_prefix(name)) || !name[prefix]) return 0;
    return SudekiMpTextureModSafePath(name) && strlen(name) <= SUDEKIMP_MOD_NAME_MAX;
}

int SudekiMpModLooseRelative(const char *path, char *rel, size_t capacity) {
    const char *found = NULL;
    size_t n;
    if (!path || !rel || !capacity) return 0;
    for (const char *p = path; *p; ++p)
        if ((p == path || p[-1] == '\\' || p[-1] == '/') && loose_prefix(p) && p[loose_prefix(p)]) found = p;
    if (!found || (n = strlen(found)) >= capacity) return 0;
    for (size_t i = 0; i <= n; ++i) rel[i] = found[i] == '/' ? '\\' : found[i];
    return 1;
}

long SudekiMpModOrderRank(const char *order, size_t length, const char *folder) {
    size_t at = 0, folder_length;
    long rank = 0;
    if (!order || !folder || !folder[0]) return -1;
    folder_length = strlen(folder);
    if (length >= 3 && (unsigned char)order[0] == 0xef && (unsigned char)order[1] == 0xbb &&
        (unsigned char)order[2] == 0xbf) at = 3;
    while (at < length) {
        size_t start = at, end, i;
        while (at < length && order[at] != '\n' && order[at] != '\r') ++at;
        end = at;
        while (at < length && (order[at] == '\n' || order[at] == '\r')) ++at;
        while (start < end && (order[start] == ' ' || order[start] == '\t')) ++start;
        while (end > start && (order[end - 1] == ' ' || order[end - 1] == '\t')) --end;
        if (start == end || order[start] == '#' || order[start] == ';') continue;
        if (end - start == folder_length) {
            for (i = 0; i < folder_length; ++i) {
                unsigned char a = (unsigned char)order[start + i], b = (unsigned char)folder[i];
                if (a >= 'A' && a <= 'Z') a = (unsigned char)(a + 32);
                if (b >= 'A' && b <= 'Z') b = (unsigned char)(b + 32);
                if (a != b) break;
            }
            if (i == folder_length) return rank;
        }
        ++rank;
    }
    return -1;
}
