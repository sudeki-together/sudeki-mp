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
