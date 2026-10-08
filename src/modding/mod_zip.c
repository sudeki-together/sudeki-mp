#include "modding/mod_zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- errors */

static int fail(char *error, size_t capacity, const char *message) {
    if (error && capacity) snprintf(error, capacity, "%s", message);
    return 0;
}

/* ---------------------------------------------------------------- CRC-32 */

static uint32_t crc_table[256];
static int crc_ready;

static void crc_init(void) {
    uint32_t i, j, c;
    if (crc_ready) return;
    for (i = 0; i < 256u; ++i) {
        c = i;
        for (j = 0; j < 8u; ++j) c = (c & 1u) ? 0xedb88320u ^ (c >> 1) : c >> 1;
        crc_table[i] = c;
    }
    crc_ready = 1;
}

uint32_t SudekiMpModZipCrc32(uint32_t crc, const void *data, size_t size) {
    const uint8_t *p = (const uint8_t *)data;
    crc_init();
    crc = ~crc;
    while (size--) crc = crc_table[(crc ^ *p++) & 0xffu] ^ (crc >> 8);
    return ~crc;
}

static uint32_t crc_byte(uint32_t crc, uint8_t byte) {
    return crc_table[(crc ^ byte) & 0xffu] ^ (crc >> 8);
}

/* ---------------------------------------------------------------- inflate */

typedef struct Huffman {
    uint16_t counts[16];
    uint16_t symbols[288];
} Huffman;

typedef struct Inflate {
    const uint8_t *in;
    size_t in_size, in_at;
    uint32_t bits, bit_count;
    uint8_t *out;
    size_t out_size, out_capacity, max_size;
    int error;
} Inflate;

static uint32_t take_bits(Inflate *s, unsigned need) {
    uint32_t value;
    while (s->bit_count < need) {
        if (s->in_at >= s->in_size) { s->error = 1; return 0; }
        s->bits |= (uint32_t)s->in[s->in_at++] << s->bit_count;
        s->bit_count += 8u;
    }
    value = s->bits & ((need == 32u) ? 0xffffffffu : ((1u << need) - 1u));
    s->bits >>= need;
    s->bit_count -= need;
    return value;
}

static int build_huffman(Huffman *h, const uint8_t *lengths, unsigned count) {
    uint16_t offsets[16];
    unsigned i;
    int left = 1;
    memset(h->counts, 0, sizeof(h->counts));
    for (i = 0; i < count; ++i) h->counts[lengths[i]]++;
    h->counts[0] = 0;
    for (i = 1; i < 16u; ++i) {
        left <<= 1;
        left -= h->counts[i];
        if (left < 0) return 0; /* over-subscribed */
    }
    offsets[1] = 0;
    for (i = 1; i < 15u; ++i) offsets[i + 1] = (uint16_t)(offsets[i] + h->counts[i]);
    for (i = 0; i < count; ++i)
        if (lengths[i]) h->symbols[offsets[lengths[i]]++] = (uint16_t)i;
    return 1;
}

static int decode_symbol(Inflate *s, const Huffman *h) {
    int code = 0, first = 0, index = 0;
    unsigned length;
    for (length = 1; length < 16u; ++length) {
        int count;
        code |= (int)take_bits(s, 1);
        if (s->error) return -1;
        count = h->counts[length];
        if (code - count < first) return h->symbols[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    s->error = 1;
    return -1;
}

static int put_byte(Inflate *s, uint8_t byte) {
    if (s->out_size == s->out_capacity) {
        size_t capacity = s->out_capacity ? s->out_capacity * 2u : 65536u;
        uint8_t *grown;
        if (capacity > s->max_size) capacity = s->max_size;
        if (capacity <= s->out_size) { s->error = 1; return 0; }
        grown = (uint8_t *)realloc(s->out, capacity);
        if (!grown) { s->error = 1; return 0; }
        s->out = grown;
        s->out_capacity = capacity;
    }
    s->out[s->out_size++] = byte;
    return 1;
}

static const uint16_t length_base[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t length_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3,
    3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t distance_base[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97,
    129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385,
    24577};
static const uint8_t distance_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7,
    7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static int inflate_codes(Inflate *s, const Huffman *lengths, const Huffman *distances) {
    for (;;) {
        int symbol = decode_symbol(s, lengths);
        if (symbol < 0) return 0;
        if (symbol < 256) {
            if (!put_byte(s, (uint8_t)symbol)) return 0;
        } else if (symbol == 256) {
            return 1;
        } else {
            unsigned length, distance;
            int d;
            symbol -= 257;
            if (symbol >= 29) return 0;
            length = length_base[symbol] + take_bits(s, length_extra[symbol]);
            d = decode_symbol(s, distances);
            if (d < 0 || d >= 30) return 0;
            distance = distance_base[d] + take_bits(s, distance_extra[d]);
            if (s->error || distance > s->out_size) return 0;
            while (length--)
                if (!put_byte(s, s->out[s->out_size - distance])) return 0;
        }
    }
}

static int inflate_stored(Inflate *s) {
    unsigned length, check;
    s->bits = 0;
    s->bit_count = 0; /* to the byte boundary */
    if (s->in_at + 4u > s->in_size) return 0;
    length = (unsigned)s->in[s->in_at] | ((unsigned)s->in[s->in_at + 1] << 8);
    check = (unsigned)s->in[s->in_at + 2] | ((unsigned)s->in[s->in_at + 3] << 8);
    s->in_at += 4u;
    if ((length ^ 0xffffu) != check || s->in_at + length > s->in_size) return 0;
    while (length--)
        if (!put_byte(s, s->in[s->in_at++])) return 0;
    return 1;
}

static int inflate_fixed(Inflate *s) {
    static Huffman lengths, distances;
    static int ready;
    if (!ready) {
        uint8_t l[288];
        unsigned i;
        for (i = 0; i < 144u; ++i) l[i] = 8;
        for (; i < 256u; ++i) l[i] = 9;
        for (; i < 280u; ++i) l[i] = 7;
        for (; i < 288u; ++i) l[i] = 8;
        build_huffman(&lengths, l, 288);
        for (i = 0; i < 30u; ++i) l[i] = 5;
        build_huffman(&distances, l, 30);
        ready = 1;
    }
    return inflate_codes(s, &lengths, &distances);
}

static int inflate_dynamic(Inflate *s) {
    static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2,
        14, 1, 15};
    uint8_t lengths[320];
    Huffman code_lengths, literal, distance;
    unsigned nlen, ndist, ncode, i;
    nlen = take_bits(s, 5) + 257u;
    ndist = take_bits(s, 5) + 1u;
    ncode = take_bits(s, 4) + 4u;
    if (s->error || nlen > 286u || ndist > 30u) return 0;
    memset(lengths, 0, sizeof(lengths));
    for (i = 0; i < ncode; ++i) lengths[order[i]] = (uint8_t)take_bits(s, 3);
    if (s->error || !build_huffman(&code_lengths, lengths, 19)) return 0;
    for (i = 0; i < nlen + ndist;) {
        int symbol = decode_symbol(s, &code_lengths);
        unsigned repeat;
        uint8_t value = 0;
        if (symbol < 0) return 0;
        if (symbol < 16) { lengths[i++] = (uint8_t)symbol; continue; }
        if (symbol == 16) {
            if (!i) return 0;
            value = lengths[i - 1];
            repeat = 3u + take_bits(s, 2);
        } else if (symbol == 17) {
            repeat = 3u + take_bits(s, 3);
        } else {
            repeat = 11u + take_bits(s, 7);
        }
        if (s->error || i + repeat > nlen + ndist) return 0;
        while (repeat--) lengths[i++] = value;
    }
    if (!lengths[256]) return 0;
    if (!build_huffman(&literal, lengths, nlen) ||
        !build_huffman(&distance, lengths + nlen, ndist)) return 0;
    return inflate_codes(s, &literal, &distance);
}

int SudekiMpModInflate(const uint8_t *in, size_t in_size, size_t expected_size,
    size_t max_size, uint8_t **out, size_t *out_size) {
    Inflate s;
    int last;
    memset(&s, 0, sizeof(s));
    s.in = in;
    s.in_size = in_size;
    s.max_size = max_size ? max_size : (size_t)256u * 1024u * 1024u;
    if (expected_size && expected_size <= s.max_size) {
        s.out = (uint8_t *)malloc(expected_size);
        if (s.out) s.out_capacity = expected_size;
    }
    do {
        unsigned type;
        last = (int)take_bits(&s, 1);
        type = take_bits(&s, 2);
        if (s.error) break;
        if (type == 0u) { if (!inflate_stored(&s)) s.error = 1; }
        else if (type == 1u) { if (!inflate_fixed(&s)) s.error = 1; }
        else if (type == 2u) { if (!inflate_dynamic(&s)) s.error = 1; }
        else s.error = 1;
    } while (!last && !s.error);
    if (s.error) {
        free(s.out);
        return 0;
    }
    *out = s.out ? s.out : (uint8_t *)malloc(1);
    *out_size = s.out_size;
    return *out != NULL;
}

/* ---------------------------------------------------------------- ZipCrypto */

typedef struct ZipCrypto { uint32_t k0, k1, k2; } ZipCrypto;

static void crypto_update(ZipCrypto *c, uint8_t byte) {
    c->k0 = crc_byte(c->k0, byte);
    c->k1 = (c->k1 + (c->k0 & 0xffu)) * 134775813u + 1u;
    c->k2 = crc_byte(c->k2, (uint8_t)(c->k1 >> 24));
}

static void crypto_init(ZipCrypto *c, const uint8_t *password, size_t length) {
    size_t i;
    crc_init();
    c->k0 = 0x12345678u;
    c->k1 = 0x23456789u;
    c->k2 = 0x34567890u;
    for (i = 0; i < length; ++i) crypto_update(c, password[i]);
}

static void crypto_decrypt(ZipCrypto *c, uint8_t *data, size_t size) {
    size_t i;
    for (i = 0; i < size; ++i) {
        uint16_t temp = (uint16_t)((c->k2 | 2u) & 0xffffu);
        uint8_t plain = (uint8_t)(data[i] ^ (uint8_t)(((uint32_t)temp * (temp ^ 1u)) >> 8));
        crypto_update(c, plain);
        data[i] = plain;
    }
}

/* ---------------------------------------------------------------- text */

static uint16_t read16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int utf8_valid(const uint8_t *s, size_t n) {
    size_t i = 0, extra, k;
    while (i < n) {
        uint8_t c = s[i];
        if (c < 0x80u) extra = 0;
        else if ((c & 0xe0u) == 0xc0u) extra = 1;
        else if ((c & 0xf0u) == 0xe0u) extra = 2;
        else if ((c & 0xf8u) == 0xf0u) extra = 3;
        else return 0;
        if (extra >= n - i) return 0;
        for (k = 1; k <= extra; ++k)
            if ((s[i + k] & 0xc0u) != 0x80u) return 0;
        i += extra + 1u;
    }
    return 1;
}

static size_t put_utf8(char *out, size_t capacity, size_t at, uint32_t c) {
    if (c < 0x80u) { if (at + 1u < capacity) out[at++] = (char)c; }
    else if (c < 0x800u) {
        if (at + 2u < capacity) { out[at++] = (char)(0xc0u | (c >> 6)); out[at++] = (char)(0x80u | (c & 0x3fu)); }
    } else if (at + 3u < capacity) {
        out[at++] = (char)(0xe0u | (c >> 12));
        out[at++] = (char)(0x80u | ((c >> 6) & 0x3fu));
        out[at++] = (char)(0x80u | (c & 0x3fu));
    }
    return at;
}

/* Windows tools write UTF-8 or cp1252 (and cp437 zip names): keep valid
 * UTF-8, otherwise map single bytes (0x80-0x9F through cp1252). */
static void to_utf8(const uint8_t *in, size_t n, char *out, size_t capacity) {
    static const uint16_t cp1252[32] = {0x20ac, 0x81, 0x201a, 0x192, 0x201e, 0x2026, 0x2020,
        0x2021, 0x2c6, 0x2030, 0x160, 0x2039, 0x152, 0x8d, 0x17d, 0x8f, 0x90, 0x2018, 0x2019,
        0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 0x2dc, 0x2122, 0x161, 0x203a, 0x153, 0x9d,
        0x17e, 0x178};
    size_t at = 0, i;
    if (!capacity) return;
    if (utf8_valid(in, n)) {
        size_t copy = n < capacity - 1u ? n : capacity - 1u;
        while (copy && (in[copy] & 0xc0u) == 0x80u && copy < n) --copy; /* whole characters */
        memcpy(out, in, copy);
        out[copy] = 0;
        return;
    }
    for (i = 0; i < n; ++i) {
        uint32_t c = in[i];
        if (c >= 0x80u && c < 0xa0u) c = cp1252[c - 0x80u];
        at = put_utf8(out, capacity, at, c);
    }
    out[at] = 0;
}

/* ---------------------------------------------------------------- reader */

static void zip_clear(SudekiMpModZip *zip) {
    size_t i;
    for (i = 0; zip->entries && i < zip->count; ++i) free(zip->entries[i].data);
    free(zip->entries);
    memset(zip, 0, sizeof(*zip));
}

void SudekiMpModZipFree(SudekiMpModZip *zip) {
    if (zip) zip_clear(zip);
}

static int add_entry(SudekiMpModZip *zip, const uint8_t *name, size_t name_length, int utf8,
    const uint8_t *data, size_t compressed, size_t uncompressed, unsigned method, unsigned flags,
    uint32_t crc, const uint8_t *password, size_t password_length, size_t *total,
    size_t max_total, char *error, size_t capacity) {
    SudekiMpModZipEntry *entry;
    uint8_t *plain = NULL, *payload;
    size_t payload_size = compressed;
    if (zip->count >= SUDEKIMP_MOD_ZIP_MAX_ENTRIES) return fail(error, capacity, "too many entries");
    if (name_length == 0 || name_length > SUDEKIMP_MOD_ZIP_NAME_MAX)
        return fail(error, capacity, "an entry name is empty or too long");
    if (uncompressed > max_total || *total > max_total - uncompressed)
        return fail(error, capacity, "the package is larger than allowed");
    entry = (SudekiMpModZipEntry *)realloc(zip->entries, (zip->count + 1u) * sizeof(*entry));
    if (!entry) return fail(error, capacity, "out of memory");
    zip->entries = entry;
    entry = &zip->entries[zip->count];
    memset(entry, 0, sizeof(*entry));
    if (utf8) {
        size_t copy = name_length;
        memcpy(entry->name, name, copy);
        entry->name[copy] = 0;
    } else {
        to_utf8(name, name_length, entry->name, sizeof(entry->name));
    }
    entry->directory = name[name_length - 1] == '/' || name[name_length - 1] == '\\';
    if (entry->directory) { ++zip->count; return 1; }
    if (flags & 1u) {
        ZipCrypto crypto;
        if (!password) return fail(error, capacity, "the package is encrypted");
        if (compressed < 12u) return fail(error, capacity, "an encrypted entry is truncated");
        plain = (uint8_t *)malloc(compressed ? compressed : 1u);
        if (!plain) return fail(error, capacity, "out of memory");
        memcpy(plain, data, compressed);
        crypto_init(&crypto, password, password_length);
        crypto_decrypt(&crypto, plain, compressed);
        payload = plain + 12;
        payload_size = compressed - 12u;
    } else {
        payload = (uint8_t *)(uintptr_t)data;
    }
    if (method == 0u) {
        entry->data = (uint8_t *)malloc(payload_size ? payload_size : 1u);
        if (!entry->data) { free(plain); return fail(error, capacity, "out of memory"); }
        memcpy(entry->data, payload, payload_size);
        entry->size = payload_size;
    } else if (method == 8u) {
        if (!SudekiMpModInflate(payload, payload_size, uncompressed, max_total - *total,
                &entry->data, &entry->size)) {
            free(plain);
            return fail(error, capacity, "an entry could not be decompressed");
        }
    } else {
        free(plain);
        return fail(error, capacity, "an entry uses an unsupported compression method");
    }
    free(plain);
    if (crc && SudekiMpModZipCrc32(0, entry->data, entry->size) != crc) {
        free(entry->data);
        entry->data = NULL;
        return fail(error, capacity, "an entry failed its checksum (wrong password or damaged)");
    }
    *total += entry->size;
    ++zip->count;
    return 1;
}

static int read_central(const uint8_t *bytes, size_t size, const uint8_t *password,
    size_t password_length, size_t max_total, SudekiMpModZip *zip, char *error, size_t capacity) {
    size_t eocd = 0, at, i, total = 0;
    uint32_t cd_offset, cd_size;
    unsigned entries, comment_length;
    int found = 0;
    if (size < 22u) return fail(error, capacity, "not a zip archive");
    for (at = size - 22u;; --at) {
        if (read32(bytes + at) == 0x06054b50u) { eocd = at; found = 1; break; }
        if (at == 0 || size - at > 22u + 65535u) break;
    }
    if (!found) return fail(error, capacity, "no zip directory");
    entries = read16(bytes + eocd + 10);
    cd_size = read32(bytes + eocd + 12);
    cd_offset = read32(bytes + eocd + 16);
    comment_length = read16(bytes + eocd + 20);
    if (eocd + 22u + comment_length <= size)
        to_utf8(bytes + eocd + 22, comment_length, zip->comment, sizeof(zip->comment));
    if ((size_t)cd_offset > size || (size_t)cd_size > size - cd_offset)
        return fail(error, capacity, "damaged zip directory");
    at = cd_offset;
    for (i = 0; i < entries; ++i) {
        unsigned flags, method, name_length, extra_length, entry_comment, local_name, local_extra;
        uint32_t crc, compressed, uncompressed, local;
        size_t data;
        if (at + 46u > size || read32(bytes + at) != 0x02014b50u)
            return fail(error, capacity, "damaged zip directory");
        flags = read16(bytes + at + 8);
        method = read16(bytes + at + 10);
        crc = read32(bytes + at + 16);
        compressed = read32(bytes + at + 20);
        uncompressed = read32(bytes + at + 24);
        name_length = read16(bytes + at + 28);
        extra_length = read16(bytes + at + 30);
        entry_comment = read16(bytes + at + 32);
        local = read32(bytes + at + 42);
        if (at + 46u + name_length > size || (size_t)local + 30u > size ||
            read32(bytes + local) != 0x04034b50u)
            return fail(error, capacity, "damaged zip entry");
        local_name = read16(bytes + local + 26);
        local_extra = read16(bytes + local + 28);
        data = (size_t)local + 30u + local_name + local_extra;
        if (data > size || (size_t)compressed > size - data)
            return fail(error, capacity, "damaged zip entry");
        if (!add_entry(zip, bytes + at + 46, name_length, (flags & 0x800u) != 0u, bytes + data,
                compressed, uncompressed, method, flags, crc, password, password_length,
                &total, max_total, error, capacity))
            return 0;
        at += 46u + name_length + extra_length + entry_comment;
    }
    return 1;
}

/* TPFs with a damaged directory: walk the local headers instead. */
static int read_local(const uint8_t *bytes, size_t size, const uint8_t *password,
    size_t password_length, size_t max_total, SudekiMpModZip *zip, char *error, size_t capacity) {
    size_t at = 0, total = 0;
    while (at + 30u <= size) {
        unsigned flags, method, name_length, extra_length;
        uint32_t crc, compressed, uncompressed;
        size_t data;
        if (read32(bytes + at) != 0x04034b50u) { ++at; continue; }
        flags = read16(bytes + at + 6);
        method = read16(bytes + at + 8);
        crc = read32(bytes + at + 14);
        compressed = read32(bytes + at + 18);
        uncompressed = read32(bytes + at + 22);
        name_length = read16(bytes + at + 26);
        extra_length = read16(bytes + at + 28);
        data = at + 30u + name_length + extra_length;
        if ((flags & 8u) || data > size || (size_t)compressed > size - data) break;
        if (!add_entry(zip, bytes + at + 30, name_length, (flags & 0x800u) != 0u, bytes + data,
                compressed, uncompressed, method, flags, crc, password, password_length,
                &total, max_total, error, capacity))
            return 0;
        at = data + (compressed ? compressed : 1u);
    }
    return zip->count ? 1 : fail(error, capacity, "no readable zip entries");
}

int SudekiMpModZipRead(const uint8_t *bytes, size_t size, const uint8_t *password,
    size_t password_length, size_t max_total, SudekiMpModZip *zip, char *error,
    size_t error_capacity) {
    if (!zip) return 0;
    memset(zip, 0, sizeof(*zip));
    if (!bytes) return fail(error, error_capacity, "no data");
    if (!max_total) max_total = (size_t)512u * 1024u * 1024u;
    if (read_central(bytes, size, password, password_length, max_total, zip, error,
            error_capacity))
        return 1;
    {
        char comment[sizeof(zip->comment)];
        memcpy(comment, zip->comment, sizeof(comment));
        zip_clear(zip);
        memcpy(zip->comment, comment, sizeof(comment));
    }
    if (read_local(bytes, size, password, password_length, max_total, zip, error,
            error_capacity))
        return 1;
    zip_clear(zip);
    return 0;
}

static int same_name(const char *a, const char *b) {
    for (;; ++a, ++b) {
        unsigned char x = (unsigned char)*a, y = (unsigned char)*b;
        if (x == '\\') x = '/';
        if (y == '\\') y = '/';
        if (x >= 'A' && x <= 'Z') x = (unsigned char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (unsigned char)(y + 32);
        if (x != y) return 0;
        if (!x) return 1;
    }
}

static const char *base_name(const char *name) {
    const char *slash = name, *p;
    for (p = name; *p; ++p)
        if (*p == '/' || *p == '\\') slash = p + 1;
    return slash;
}

long SudekiMpModZipFind(const SudekiMpModZip *zip, const char *name) {
    size_t i;
    long found = -1;
    if (!zip || !name) return -1;
    for (i = 0; i < zip->count; ++i)
        if (!zip->entries[i].directory && same_name(zip->entries[i].name, name)) return (long)i;
    for (i = 0; i < zip->count; ++i)
        if (!zip->entries[i].directory &&
            same_name(base_name(zip->entries[i].name), base_name(name))) {
            if (found >= 0) return -1; /* ambiguous */
            found = (long)i;
        }
    return found;
}

/* ---------------------------------------------------------------- writer */

static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

void SudekiMpModZipWriterBegin(SudekiMpModZipWriter *w, SudekiMpModZipSink sink, void *context) {
    memset(w, 0, sizeof(*w));
    w->sink = sink;
    w->context = context;
}

static int emit(SudekiMpModZipWriter *w, const void *bytes, size_t size) {
    if (w->failed || !w->sink(w->context, bytes, size)) { w->failed = 1; return 0; }
    w->offset += size;
    return 1;
}

int SudekiMpModZipWriterAdd(SudekiMpModZipWriter *w, const char *name, const void *data,
    size_t size) {
    uint8_t local[30], *central;
    size_t name_length = name ? strlen(name) : 0;
    uint32_t crc;
    if (w->failed || !name_length || name_length > SUDEKIMP_MOD_ZIP_NAME_MAX ||
        size > 0xffffffffu || w->offset > 0xffffffffu || w->count >= 65535u) {
        w->failed = 1;
        return 0;
    }
    crc = SudekiMpModZipCrc32(0, data, size);
    memset(local, 0, sizeof(local));
    put32(local, 0x04034b50u);
    put16(local + 4, 20);
    put16(local + 6, 0x800u); /* UTF-8 names */
    put32(local + 14, crc);
    put32(local + 18, (uint32_t)size);
    put32(local + 22, (uint32_t)size);
    put16(local + 26, (unsigned)name_length);
    if (w->central_size + 46u + name_length > w->central_capacity) {
        size_t capacity = (w->central_capacity + 46u + name_length) * 2u;
        uint8_t *grown = (uint8_t *)realloc(w->central, capacity);
        if (!grown) { w->failed = 1; return 0; }
        w->central = grown;
        w->central_capacity = capacity;
    }
    central = w->central + w->central_size;
    memset(central, 0, 46);
    put32(central, 0x02014b50u);
    put16(central + 4, 20);
    put16(central + 6, 20);
    put16(central + 8, 0x800u);
    put32(central + 16, crc);
    put32(central + 20, (uint32_t)size);
    put32(central + 24, (uint32_t)size);
    put16(central + 28, (unsigned)name_length);
    put32(central + 42, (uint32_t)w->offset);
    memcpy(central + 46, name, name_length);
    w->central_size += 46u + name_length;
    ++w->count;
    return emit(w, local, sizeof(local)) && emit(w, name, name_length) &&
        (size == 0 || emit(w, data, size));
}

int SudekiMpModZipWriterFinish(SudekiMpModZipWriter *w, const char *comment) {
    uint8_t eocd[22];
    size_t comment_length = comment ? strlen(comment) : 0;
    uint64_t cd_offset = w->offset;
    int ok;
    if (comment_length > 65535u) comment_length = 65535u;
    ok = !w->failed && cd_offset <= 0xffffffffu &&
        emit(w, w->central, w->central_size);
    memset(eocd, 0, sizeof(eocd));
    put32(eocd, 0x06054b50u);
    put16(eocd + 8, (unsigned)w->count);
    put16(eocd + 10, (unsigned)w->count);
    put32(eocd + 12, (uint32_t)w->central_size);
    put32(eocd + 16, (uint32_t)cd_offset);
    put16(eocd + 20, (unsigned)comment_length);
    ok = ok && emit(w, eocd, sizeof(eocd)) && (!comment_length || emit(w, comment, comment_length));
    free(w->central);
    w->central = NULL;
    w->central_size = w->central_capacity = 0;
    return ok && !w->failed;
}

/* ---------------------------------------------------------------- TexMod */

/* The ZipCrypto password every TexMod TPF uses (as published by OpenTexMod/uMod). */
static const uint8_t tpf_password[42] = {
    0x73, 0x2A, 0x63, 0x7D, 0x5F, 0x0A, 0xA6, 0xBD, 0x7D, 0x65, 0x7E, 0x67, 0x61, 0x2A,
    0x7F, 0x7F, 0x74, 0x61, 0x67, 0x5B, 0x60, 0x70, 0x45, 0x74, 0x5C, 0x22, 0x74, 0x5D,
    0x6E, 0x6A, 0x73, 0x41, 0x77, 0x6E, 0x46, 0x47, 0x77, 0x49, 0x0C, 0x4B, 0x46, 0x6F};

static int parse_hex(const char *s, size_t n, uint32_t *value) {
    size_t i = 0;
    uint64_t v = 0;
    if (n >= 2u && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) i = 2;
    if (i == n) return 0;
    for (; i < n; ++i) {
        char c = s[i];
        unsigned digit = c >= '0' && c <= '9' ? (unsigned)(c - '0') :
            c >= 'a' && c <= 'f' ? (unsigned)(c - 'a' + 10) :
            c >= 'A' && c <= 'F' ? (unsigned)(c - 'A' + 10) : 16u;
        if (digit > 15u) return 0;
        v = v * 16u + digit;
        if (v > 0xffffffffu) return 0;
    }
    *value = (uint32_t)v;
    return 1;
}

static char *def_text(const uint8_t *data, size_t size) {
    char *text;
    size_t i, at = 0;
    if (size >= 2u && ((data[0] == 0xffu && data[1] == 0xfeu) || (data[0] == 0xfeu && data[1] == 0xffu))) {
        int big = data[0] == 0xfeu;
        text = (char *)malloc(size * 2u + 1u);
        if (!text) return NULL;
        for (i = 2; i + 1u < size; i += 2u) {
            uint32_t c = big ? ((uint32_t)data[i] << 8) | data[i + 1] : ((uint32_t)data[i + 1] << 8) | data[i];
            if (c) at = put_utf8(text, size * 2u + 1u, at, c);
        }
        text[at] = 0;
        return text;
    }
    text = (char *)malloc(size * 3u + 1u);
    if (!text) return NULL;
    to_utf8(data, size, text, size * 3u + 1u);
    return text;
}

int SudekiMpModTpfRead(const uint8_t *bytes, size_t size, SudekiMpModTpf *tpf, char *error,
    size_t capacity) {
    static const uint8_t mask[4] = {0xa4, 0x3f, 0xa4, 0x3f};
    uint8_t *plain;
    size_t i, words;
    long def;
    char *text, *line;
    if (!tpf) return 0;
    memset(tpf, 0, sizeof(*tpf));
    if (!bytes || size < 30u) return fail(error, capacity, "not a TexMod package");
    plain = (uint8_t *)malloc(size);
    if (!plain) return fail(error, capacity, "out of memory");
    words = size / 4u * 4u;
    for (i = 0; i < words; ++i) plain[i] = (uint8_t)(bytes[i] ^ mask[i & 3u]);
    for (; i < size; ++i) plain[i] = (uint8_t)(bytes[i] ^ mask[0]);
    if (read32(plain) != 0x04034b50u) {
        free(plain);
        return fail(error, capacity, "not a TexMod package (no zip after the XOR layer)");
    }
    if (!SudekiMpModZipRead(plain, size, tpf_password, sizeof(tpf_password), 0, &tpf->zip,
            error, capacity)) {
        free(plain);
        return 0;
    }
    free(plain);
    def = SudekiMpModZipFind(&tpf->zip, "texmod.def");
    if (def < 0) {
        SudekiMpModTpfFree(tpf);
        return fail(error, capacity, "texmod.def is missing");
    }
    text = def_text(tpf->zip.entries[def].data, tpf->zip.entries[def].size);
    /* texmod.def itself is not a texture: drop it from the members. */
    free(tpf->zip.entries[def].data);
    memmove(&tpf->zip.entries[def], &tpf->zip.entries[def + 1],
        (tpf->zip.count - (size_t)def - 1u) * sizeof(tpf->zip.entries[0]));
    --tpf->zip.count;
    if (!text) {
        SudekiMpModTpfFree(tpf);
        return fail(error, capacity, "out of memory");
    }
    for (line = text; *line;) {
        char *end = line, *bar;
        size_t key_length;
        while (*end && *end != '\n' && *end != '\r') ++end;
        bar = memchr(line, '|', (size_t)(end - line));
        if (bar) {
            char *k = line, *m = bar + 1, *m_end = end;
            uint32_t key;
            while (k < bar && (*k == ' ' || *k == '\t')) ++k;
            key_length = (size_t)(bar - k);
            while (key_length && (k[key_length - 1] == ' ' || k[key_length - 1] == '\t')) --key_length;
            while (m < m_end && (*m == ' ' || *m == '\t')) ++m;
            while (m_end > m && (m_end[-1] == ' ' || m_end[-1] == '\t')) --m_end;
            if (parse_hex(k, key_length, &key) && m_end > m &&
                (size_t)(m_end - m) <= SUDEKIMP_MOD_ZIP_NAME_MAX) {
                SudekiMpModTpfTexture *grown = (SudekiMpModTpfTexture *)realloc(tpf->textures,
                    (tpf->count + 1u) * sizeof(*grown));
                if (!grown) { free(text); SudekiMpModTpfFree(tpf); return fail(error, capacity, "out of memory"); }
                tpf->textures = grown;
                grown = &tpf->textures[tpf->count++];
                grown->key = key;
                memcpy(grown->member_name, m, (size_t)(m_end - m));
                grown->member_name[m_end - m] = 0;
                grown->member = SudekiMpModZipFind(&tpf->zip, grown->member_name);
                if (grown->member < 0) ++tpf->missing;
            }
        }
        line = end;
        while (*line == '\n' || *line == '\r') ++line;
    }
    free(text);
    if (!tpf->count) {
        SudekiMpModTpfFree(tpf);
        return fail(error, capacity, "texmod.def lists no textures");
    }
    {
        /* Comment: first line author, the rest description (as sudekimod.py). */
        const char *c = tpf->zip.comment, *nl = strpbrk(c, "\r\n");
        size_t author = nl ? (size_t)(nl - c) : strlen(c);
        if (author >= sizeof(tpf->author)) author = sizeof(tpf->author) - 1u;
        memcpy(tpf->author, c, author);
        tpf->author[author] = 0;
        if (nl) {
            size_t d = 0;
            while (*nl == '\r' || *nl == '\n') ++nl;
            for (; *nl && d + 1u < sizeof(tpf->description); ++nl)
                tpf->description[d++] = (*nl == '\r' || *nl == '\n') ? ' ' : *nl;
            tpf->description[d] = 0;
        }
    }
    return 1;
}

void SudekiMpModTpfFree(SudekiMpModTpf *tpf) {
    if (!tpf) return;
    zip_clear(&tpf->zip);
    free(tpf->textures);
    memset(tpf, 0, sizeof(*tpf));
}

const char *SudekiMpModImageExtension(const uint8_t *b, size_t n) {
    if (n >= 4u && b[0] == 'D' && b[1] == 'D' && b[2] == 'S' && b[3] == ' ') return "dds";
    if (n >= 8u && b[0] == 0x89u && b[1] == 'P' && b[2] == 'N' && b[3] == 'G') return "png";
    if (n >= 2u && b[0] == 'B' && b[1] == 'M') return "bmp";
    if (n >= 3u && b[0] == 0xffu && b[1] == 0xd8u && b[2] == 0xffu) return "jpg";
    return "tga";
}
