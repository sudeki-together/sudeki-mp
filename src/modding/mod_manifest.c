#include "modding/mod_manifest.h"
#include "engine/texture_mod_index.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MANIFEST_MAX = 16u << 20 };

typedef struct Span { const char *text; size_t length; } Span;
typedef struct Line { size_t start, end, next; Span text; } Line;
typedef struct Buffer { char *text; size_t length, capacity; } Buffer;
typedef struct Identity { int kind; uint32_t key; char name[SUDEKIMP_MOD_NAME_MAX + 1]; } Identity;
enum { ID_METADATA, ID_TEXTURE, ID_ARCHIVE, ID_LOOSE };

static int space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static unsigned char lower(unsigned char c) { return c >= 'A' && c <= 'Z' ? (unsigned char)(c + 'a' - 'A') : c; }
static Span trim(Span s) {
    while (s.length && space(*s.text)) { ++s.text; --s.length; }
    while (s.length && space(s.text[s.length - 1u])) --s.length;
    return s;
}
static int equal(Span a, const char *b) {
    if (a.length != strlen(b)) return 0;
    for (size_t i = 0; i < a.length; ++i) if (lower((unsigned char)a.text[i]) != lower((unsigned char)b[i])) return 0;
    return 1;
}
static int copy_span(Span s, char *out, size_t capacity) {
    if (!out || s.length >= capacity) return 0;
    memcpy(out, s.text, s.length);
    out[s.length] = 0;
    return 1;
}
static int next_line(const SudekiMpModManifest *m, size_t *at, Line *line) {
    size_t end;
    if (*at >= m->length) return 0;
    line->start = *at;
    end = *at;
    while (end < m->length && m->text[end] != '\r' && m->text[end] != '\n') ++end;
    line->end = end;
    if (end < m->length && m->text[end++] == '\r' && end < m->length && m->text[end] == '\n') ++end;
    line->next = end;
    line->text.text = m->text + line->start;
    line->text.length = line->end - line->start;
    *at = end;
    return 1;
}
static int section(Span line, Span *name) {
    line = trim(line);
    if (line.length < 3u || line.text[0] != '[' || line.text[line.length - 1u] != ']') return 0;
    name->text = line.text + 1u;
    name->length = line.length - 2u;
    *name = trim(*name);
    for (size_t i = 0; i < name->length; ++i) if (name->text[i] == '[' || name->text[i] == ']') return 0;
    return name->length != 0;
}
static int assignment(Span line, Span *name, Span *value) {
    const char *eq;
    line = trim(line);
    if (!line.length || line.text[0] == ';' || line.text[0] == '#') return 0;
    eq = (const char *)memchr(line.text, '=', line.length);
    if (!eq) return 0;
    name->text = line.text;
    name->length = (size_t)(eq - line.text);
    *name = trim(*name);
    value->text = eq + 1;
    value->length = line.length - (size_t)(value->text - line.text);
    *value = trim(*value);
    return name->length != 0;
}
static int append(Buffer *b, const void *text, size_t length) {
    size_t capacity;
    char *new_text;
    if (length > MANIFEST_MAX - b->length) return 0;
    if (b->length + length + 1u > b->capacity) {
        capacity = b->capacity ? b->capacity : 256u;
        while (capacity < b->length + length + 1u) capacity *= 2u;
        new_text = (char *)realloc(b->text, capacity);
        if (!new_text) return 0;
        b->text = new_text;
        b->capacity = capacity;
    }
    if (length) memcpy(b->text + b->length, text, length);
    b->length += length;
    b->text[b->length] = 0;
    return 1;
}
/* Reject overlong UTF-8, embedded NUL, surrogate values and values above the
 * Unicode range. Successful decoding always advances at least one byte. */
static int utf8_next(const char *s, size_t length, size_t *at, uint32_t *value) {
    unsigned char c;
    unsigned count;
    uint32_t v, minimum;
    if (*at >= length) return 0;
    c = (unsigned char)s[(*at)++];
    if (!c) return 0;
    if (c < 0x80u) { *value = c; return 1; }
    if (c >= 0xc2u && c <= 0xdfu) { count = 1; v = c & 31u; minimum = 0x80u; }
    else if (c >= 0xe0u && c <= 0xefu) { count = 2; v = c & 15u; minimum = 0x800u; }
    else if (c >= 0xf0u && c <= 0xf4u) { count = 3; v = c & 7u; minimum = 0x10000u; }
    else return 0;
    if (count > length - *at) return 0;
    while (count--) {
        c = (unsigned char)s[(*at)++];
        if ((c & 0xc0u) != 0x80u) return 0;
        v = (v << 6) | (c & 63u);
    }
    if (v < minimum || v > 0x10ffffu || (v >= 0xd800u && v <= 0xdfffu)) return 0;
    *value = v;
    return 1;
}
static int valid_utf8(const char *s, size_t length, int single_line) {
    size_t at = 0;
    uint32_t c;
    while (at < length) {
        if (!utf8_next(s, length, &at, &c)) return 0;
        if (single_line && (c < 32u || c == 127u)) return 0;
    }
    return 1;
}
static int append_utf8(Buffer *b, uint32_t c) {
    char bytes[4]; size_t n;
    if (!c || c > 0x10ffffu || (c >= 0xd800u && c <= 0xdfffu)) return 0;
    if (c < 0x80u) { bytes[0] = (char)c; n = 1; }
    else if (c < 0x800u) { bytes[0] = (char)(0xc0u | (c >> 6)); bytes[1] = (char)(0x80u | (c & 63u)); n = 2; }
    else if (c < 0x10000u) {
        bytes[0] = (char)(0xe0u | (c >> 12)); bytes[1] = (char)(0x80u | ((c >> 6) & 63u));
        bytes[2] = (char)(0x80u | (c & 63u)); n = 3;
    } else {
        bytes[0] = (char)(0xf0u | (c >> 18)); bytes[1] = (char)(0x80u | ((c >> 12) & 63u));
        bytes[2] = (char)(0x80u | ((c >> 6) & 63u)); bytes[3] = (char)(0x80u | (c & 63u)); n = 4;
    }
    return append(b, bytes, n);
}
static int usable(const SudekiMpModManifest *m) {
    return m && m->text && m->length <= MANIFEST_MAX && valid_utf8(m->text, m->length, 0);
}

int SudekiMpModManifestLoad(const void *bytes, size_t size, SudekiMpModManifest *m) {
    const uint8_t *raw = (const uint8_t *)bytes;
    Buffer b = {0};
    int utf16 = 0;
    if (!m || (!raw && size) || size > MANIFEST_MAX) return 0;
    if (size >= 2u && raw[0] == 0xffu && raw[1] == 0xfeu) {
        utf16 = 1;
        if (size & 1u) return 0;
        for (size_t at = 2; at < size; at += 2u) {
            uint32_t c = raw[at] | (uint32_t)raw[at + 1u] << 8;
            if (c >= 0xd800u && c <= 0xdbffu) {
                uint32_t tail;
                if (at + 3u >= size) goto fail;
                at += 2u;
                tail = raw[at] | (uint32_t)raw[at + 1u] << 8;
                if (tail < 0xdc00u || tail > 0xdfffu) goto fail;
                c = 0x10000u + ((c - 0xd800u) << 10) + tail - 0xdc00u;
            }
            if (!append_utf8(&b, c)) goto fail;
        }
    } else {
        for (size_t i = 0; i < size; ++i) if (!raw[i] || raw[i] >= 128u) return 0;
        if (!append(&b, raw, size)) goto fail;
    }
    if (!b.text && !append(&b, "", 0)) goto fail;
    m->text = b.text;
    m->length = b.length;
    m->utf16 = utf16;
    return 1;
fail:
    free(b.text);
    return 0;
}

int SudekiMpModManifestCreate(const char *name, SudekiMpModManifest *m) {
    Buffer b = {0};
    static const char first[] = "; SudekiMP mod package (docs/mod-packages.md)\r\n[Mod]\r\nFormat=SudekiMP.Mod/1\r\nName=";
    static const char last[] = "\r\nVersion=1\r\nEnabled=true\r\n\r\n[Textures]\r\n\r\n[Files]\r\n";
    if (!m || !name || !*name || !valid_utf8(name, strlen(name), 1)) return 0;
    if (!append(&b, first, sizeof(first) - 1u) || !append(&b, name, strlen(name)) || !append(&b, last, sizeof(last) - 1u)) {
        free(b.text); return 0;
    }
    m->text = b.text; m->length = b.length; m->utf16 = 0;
    return 1;
}

int SudekiMpModManifestEncode(const SudekiMpModManifest *m, uint8_t **bytes, size_t *size) {
    Buffer b = {0};
    size_t at = 0;
    uint32_t c;
    int utf16;
    if (!bytes || !size || !usable(m)) return 0;
    *bytes = NULL; *size = 0;
    utf16 = m->utf16;
    for (size_t i = 0; i < m->length; ++i) if ((unsigned char)m->text[i] >= 128u) utf16 = 1;
    if (!utf16) {
        if (!append(&b, m->text, m->length)) return 0;
    } else {
        static const uint8_t bom[] = {0xff, 0xfe};
        if (!append(&b, bom, 2)) goto fail;
        while (at < m->length) {
            uint8_t encoded[4]; size_t n = 2;
            if (!utf8_next(m->text, m->length, &at, &c)) goto fail;
            if (c < 0x10000u) { encoded[0] = (uint8_t)c; encoded[1] = (uint8_t)(c >> 8); }
            else {
                uint32_t high, low;
                c -= 0x10000u; high = 0xd800u | (c >> 10); low = 0xdc00u | (c & 1023u);
                encoded[0] = (uint8_t)high; encoded[1] = (uint8_t)(high >> 8);
                encoded[2] = (uint8_t)low; encoded[3] = (uint8_t)(low >> 8); n = 4;
            }
            if (!append(&b, encoded, n)) goto fail;
        }
    }
    *bytes = (uint8_t *)b.text; *size = b.length;
    return 1;
fail:
    free(b.text); return 0;
}

void SudekiMpModManifestFree(SudekiMpModManifest *m) {
    if (m) { free(m->text); memset(m, 0, sizeof(*m)); }
}

static int file_identity(const char *name, Identity *id) {
    char line[SUDEKIMP_MOD_NAME_MAX + 16], path[16];
    uint32_t key;
    if (!name || strlen(name) > SUDEKIMP_MOD_NAME_MAX ||
        !valid_utf8(name, strlen(name), 1) ||
        snprintf(line, sizeof(line), "%s=x", name) < 0 ||
        SudekiMpModFileParseLine(line, &key, id->name, sizeof(id->name), path, sizeof(path)) != 1) return 0;
    /* Parsing trims the left side and splits '='. A writer must accept exactly
     * one identity rather than accidentally accepting an assignment as name. */
    if (strlen(name) != strlen(id->name)) return 0;
    for (size_t i = 0; name[i]; ++i) if ((name[i] == '/' ? '\\' : name[i]) != id->name[i]) return 0;
    id->kind = strchr(id->name, '\\') ? ID_LOOSE : ID_ARCHIVE;
    id->key = key;
    return 1;
}
static int matches(Span name, const Identity *id) {
    char text[SUDEKIMP_MOD_NAME_MAX + 1];
    uint32_t key;
    Identity parsed;
    if (id->kind == ID_METADATA) return equal(name, id->name);
    if (!copy_span(name, text, sizeof(text))) return 0;
    if (id->kind == ID_TEXTURE) return SudekiMpTextureModParseKey(text, &key) && key == id->key;
    if (!file_identity(text, &parsed) || parsed.kind != id->kind) return 0;
    if (id->kind == ID_ARCHIVE) return parsed.key == id->key;
    return equal((Span){parsed.name, strlen(parsed.name)}, id->name);
}
static const char *newline(const SudekiMpModManifest *m, size_t *length) {
    for (size_t i = 0; i < m->length; ++i) {
        if (m->text[i] == '\r') { *length = i + 1u < m->length && m->text[i + 1u] == '\n' ? 2u : 1u; return m->text + i; }
        if (m->text[i] == '\n') { *length = 1; return m->text + i; }
    }
    *length = 2; return "\r\n";
}
static int insert_entry(Buffer *out, const char *name, const char *value, const char *eol, size_t eol_size) {
    if (out->length && out->text[out->length - 1u] != '\r' && out->text[out->length - 1u] != '\n' && !append(out, eol, eol_size)) return 0;
    return append(out, name, strlen(name)) && append(out, "=", 1) && append(out, value, strlen(value)) && append(out, eol, eol_size);
}
static int edit(SudekiMpModManifest *m, const char *target, const Identity *id, const char *name, const char *value) {
    Buffer out = {0};
    size_t at = 0, eol_size;
    const char *eol;
    Line line;
    int in_target = 0, found = 0, inserted = 0;
    if (!usable(m)) return 0;
    eol = newline(m, &eol_size);
    while (next_line(m, &at, &line)) {
        Span sec, lhs, rhs;
        if (section(line.text, &sec)) {
            if (in_target && !inserted && value) {
                if (!insert_entry(&out, name, value, eol, eol_size)) goto fail;
                inserted = 1;
            }
            in_target = equal(sec, target);
            if (in_target) found = 1;
        } else if (in_target && assignment(line.text, &lhs, &rhs) && matches(lhs, id)) continue;
        if (!append(&out, m->text + line.start, line.next - line.start)) goto fail;
    }
    if (value && !inserted) {
        if (!found) {
            if (out.length && out.text[out.length - 1u] != '\r' && out.text[out.length - 1u] != '\n' && !append(&out, eol, eol_size)) goto fail;
            if (out.length && !append(&out, eol, eol_size)) goto fail;
            if (!append(&out, "[", 1) || !append(&out, target, strlen(target)) || !append(&out, "]", 1) || !append(&out, eol, eol_size)) goto fail;
        }
        if (!insert_entry(&out, name, value, eol, eol_size)) goto fail;
    }
    if (!out.text && !append(&out, "", 0)) goto fail;
    free(m->text); m->text = out.text; m->length = out.length;
    return 1;
fail:
    free(out.text); return 0;
}

int SudekiMpModManifestSetEnabled(SudekiMpModManifest *m, int enabled) {
    Identity id = {ID_METADATA, 0, "Enabled"};
    return edit(m, "Mod", &id, "Enabled", enabled ? "true" : "false");
}
int SudekiMpModManifestSetMetadata(SudekiMpModManifest *m, const char *name, const char *value) {
    Identity id = {ID_METADATA, 0, ""};
    size_t i, n = name ? strlen(name) : 0;
    if (!n || n >= sizeof(id.name)) return 0;
    for (i = 0; i < n; ++i)
        if (!((name[i] >= 'A' && name[i] <= 'Z') || (name[i] >= 'a' && name[i] <= 'z') ||
              (name[i] >= '0' && name[i] <= '9'))) return 0;
    {
        Span probe = {name, n};
        if (equal(probe, "Format") || equal(probe, "Enabled")) return 0;
    }
    if (value && !valid_utf8(value, strlen(value), 1)) return 0;
    memcpy(id.name, name, n + 1);
    return edit(m, "Mod", &id, name, value);
}
int SudekiMpModManifestSetTexture(SudekiMpModManifest *m, uint32_t key, const char *relative) {
    Identity id = {ID_TEXTURE, 0, ""};
    char name[16];
    if (relative && (!SudekiMpTextureModSafePath(relative) || !valid_utf8(relative, strlen(relative), 1))) return 0;
    id.key = key;
    snprintf(name, sizeof(name), "0x%08lX", (unsigned long)key);
    return edit(m, "Textures", &id, name, relative);
}
int SudekiMpModManifestSetFile(SudekiMpModManifest *m, const char *name, const char *relative) {
    Identity id;
    if (!file_identity(name, &id) || (relative && (!SudekiMpTextureModSafePath(relative) || !valid_utf8(relative, strlen(relative), 1)))) return 0;
    return edit(m, "Files", &id, id.name, relative);
}

static int get(const SudekiMpModManifest *m, const char *target, const Identity *id, Span *value) {
    Line line;
    size_t at = 0;
    int in_target = 0, found = 0;
    if (!usable(m)) return 0;
    while (next_line(m, &at, &line)) {
        Span sec, lhs, rhs;
        if (section(line.text, &sec)) in_target = equal(sec, target);
        else if (in_target && assignment(line.text, &lhs, &rhs) && matches(lhs, id)) {
            char path[SUDEKIMP_TEXTURE_MOD_PATH_MAX];
            /* The runtime ignores rejected paths. An earlier valid mapping
             * remains effective when a later line for that identity is bad. */
            if (id->kind != ID_METADATA && (!copy_span(rhs, path, sizeof(path)) || !SudekiMpTextureModSafePath(path))) continue;
            *value = rhs; found = 1;
        }
    }
    return found;
}
int SudekiMpModManifestGetValue(const SudekiMpModManifest *m, const char *target,
    const char *name, char *value, size_t capacity) {
    Identity id = {ID_METADATA, 0, ""};
    Span found;
    if (!target || !name || strlen(name) >= sizeof(id.name)) return 0;
    strcpy(id.name, name);
    return get(m, target, &id, &found) && copy_span(found, value, capacity);
}
int SudekiMpModManifestGetTexture(const SudekiMpModManifest *m, uint32_t key, char *relative, size_t capacity) {
    Identity id = {ID_TEXTURE, 0, ""};
    Span found;
    char path[SUDEKIMP_TEXTURE_MOD_PATH_MAX];
    id.key = key;
    return get(m, "Textures", &id, &found) && copy_span(found, path, sizeof(path)) &&
        SudekiMpTextureModSafePath(path) && copy_span(found, relative, capacity);
}
int SudekiMpModManifestGetFile(const SudekiMpModManifest *m, const char *name, char *relative, size_t capacity) {
    Identity id;
    Span found;
    char path[SUDEKIMP_TEXTURE_MOD_PATH_MAX];
    return file_identity(name, &id) && get(m, "Files", &id, &found) && copy_span(found, path, sizeof(path)) &&
        SudekiMpTextureModSafePath(path) && copy_span(found, relative, capacity);
}

typedef struct Seen { Identity id; size_t line; } Seen;
static void report(SudekiMpModManifestValidation *r, SudekiMpModManifestDiagnostic callback,
    void *context, size_t line, int warning, const char *message) {
    if (warning) ++r->warnings; else ++r->errors;
    if (callback) callback(context, line, warning, message);
}
static int same_identity(const Identity *a, const Identity *b) {
    if (a->kind != b->kind) return 0;
    return a->kind == ID_LOOSE ? equal((Span){a->name, strlen(a->name)}, b->name) : a->key == b->key;
}
static size_t identity_hash(const Identity *id) {
    uint32_t h = 2166136261u ^ (uint32_t)id->kind;
    if (id->kind == ID_LOOSE) {
        for (const char *p = id->name; *p; ++p) h = (h ^ lower((unsigned char)*p)) * 16777619u;
    } else {
        for (unsigned shift = 0; shift < 32; shift += 8u) h = (h ^ ((id->key >> shift) & 255u)) * 16777619u;
    }
    return (size_t)h;
}
static Seen *seen_slot(Seen *table, size_t capacity, const Identity *id) {
    size_t at = identity_hash(id) & (capacity - 1u);
    while (table[at].line && !same_identity(&table[at].id, id)) at = (at + 1u) & (capacity - 1u);
    return table + at;
}
static Seen *seen_find(Seen **table, size_t *capacity, size_t count, const Identity *id) {
    if (!*capacity || count >= *capacity / 2u) {
        size_t grown_capacity = *capacity ? *capacity * 2u : 64u;
        Seen *grown = (Seen *)calloc(grown_capacity, sizeof(*grown));
        if (!grown) return NULL;
        for (size_t i = 0; i < *capacity; ++i) if ((*table)[i].line)
            *seen_slot(grown, grown_capacity, &(*table)[i].id) = (*table)[i];
        free(*table); *table = grown; *capacity = grown_capacity;
    }
    return seen_slot(*table, *capacity, id);
}
static int format_valid(Span value) {
    const char expected[] = "SudekiMP.Mod/1";
    size_t n = sizeof(expected) - 1u;
    if (value.length < n || !equal((Span){value.text, n}, expected)) return 0;
    return value.length == n || value.text[n] == '.';
}
static int pow2(uint32_t n) { return n && !(n & (n - 1u)); }

int SudekiMpModManifestValidate(const SudekiMpModManifest *m,
    SudekiMpModManifestInspect inspect, SudekiMpModManifestDiagnostic diagnostic,
    void *context, SudekiMpModManifestValidation *result) {
    SudekiMpModManifestValidation r = {0};
    Seen *seen = NULL;
    size_t seen_count = 0, seen_capacity = 0, at = 0, number = 0;
    Span current = {"", 0}, format = {"", 0};
    size_t format_line = 0;
    Line line;
    if (!usable(m)) { report(&r, diagnostic, context, 0, 0, "Manifest is not valid UTF-8 text"); goto done; }
    while (next_line(m, &at, &line)) {
        Span s = trim(line.text), sec, lhs, rhs;
        char text[SUDEKIMP_MOD_NAME_MAX + SUDEKIMP_TEXTURE_MOD_PATH_MAX + 32];
        char name[SUDEKIMP_MOD_NAME_MAX + 1], path[SUDEKIMP_TEXTURE_MOD_PATH_MAX];
        Identity id = {0};
        uint32_t key, width = 0, height = 0;
        int parsed, texture;
        ++number;
        if (!s.length || s.text[0] == ';' || s.text[0] == '#') continue;
        if (section(s, &sec)) { current = sec; continue; }
        if (s.text[0] == '[') { report(&r, diagnostic, context, number, 0, "Malformed section header"); continue; }
        if (equal(current, "Mod")) {
            if (!assignment(s, &lhs, &rhs)) { report(&r, diagnostic, context, number, 0, "Expected metadata name=value"); continue; }
            if (equal(lhs, "Format")) {
                if (format_line) report(&r, diagnostic, context, number, 1, "Duplicate Format metadata");
                format = rhs; format_line = number;
            }
            continue;
        }
        texture = equal(current, "Textures");
        if (!texture && !equal(current, "Files")) continue;
        if (!copy_span(s, text, sizeof(text))) parsed = -1;
        else if (texture) parsed = SudekiMpTextureModParseLine(text, &key, path, sizeof(path));
        else parsed = SudekiMpModFileParseLine(text, &key, name, sizeof(name), path, sizeof(path));
        if (parsed != 1) {
            report(&r, diagnostic, context, number, 0, texture ? "Expected 0xKEY=safe/relative/image" : "Expected NAME.EXT, 0xKEY or sound/movies name=safe/relative/file");
            continue;
        }
        id.kind = texture ? ID_TEXTURE : strchr(name, '\\') ? ID_LOOSE : ID_ARCHIVE;
        id.key = key;
        if (!texture) strcpy(id.name, name);
        {
            Seen *entry = seen_find(&seen, &seen_capacity, seen_count, &id);
            if (!entry) { report(&r, diagnostic, context, number, 0, "Out of memory validating entries"); goto done; }
            if (entry->line) {
                char message[100];
                snprintf(message, sizeof(message), "Duplicate entry (previous line %lu; later entry wins)", (unsigned long)entry->line);
                report(&r, diagnostic, context, number, 1, message);
            } else {
                entry->id = id; ++seen_count;
                if (texture) ++r.textures; else ++r.files;
            }
            entry->line = number;
        }
        if (inspect) {
            int status = inspect(context, path, texture, &width, &height);
            if (status != SUDEKIMP_MOD_FILE_OK) {
                const char *message = status == SUDEKIMP_MOD_FILE_EMPTY ? "Replacement file is empty" :
                    status == SUDEKIMP_MOD_FILE_BAD_IMAGE ? "Replacement is not a readable DDS/PNG/TGA/BMP/JPG image" : "Replacement file does not exist or cannot be read";
                report(&r, diagnostic, context, number, 0, message);
            } else if (texture && width && height && (!pow2(width) || !pow2(height)))
                report(&r, diagnostic, context, number, 1, "Image dimensions are not powers of two; the loader may resample it");
        }
    }
    if (!format_valid(format)) report(&r, diagnostic, context, format_line, 0, "[Mod] Format must be SudekiMP.Mod/1 (or a compatible minor version)");
done:
    free(seen);
    if (result) *result = r;
    return r.errors == 0;
}
