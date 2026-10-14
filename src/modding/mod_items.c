#include "modding/mod_items.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Field tree: u32 header (bit 31 = leaf, low bits = record size including the
 * header), NUL-terminated field name, then the leaf value or child records.
 * String values are a container holding "length" and "string" leaves. */
typedef struct Field { const uint8_t *data; size_t size; } Field; /* children or value */

static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static int fail(char *error, size_t capacity, const char *message) {
    if (error && capacity) snprintf(error, capacity, "%s", message);
    return 0;
}

/* Finds the direct child called name. leaf: 1 leaf, 0 container, -1 either. */
static int child(Field parent, const char *name, int leaf, Field *out) {
    size_t cursor = 0, length = strlen(name);
    while (parent.size - cursor >= 5) {
        uint32_t header = u32(parent.data + cursor), size = header & 0x7fffffffu;
        const uint8_t *record = parent.data + cursor, *end;
        if (size < 5 || size > parent.size - cursor) return 0;
        end = memchr(record + 4, 0, size - 4);
        if (!end) return 0;
        if ((size_t)(end - record - 4) == length && !memcmp(record + 4, name, length) &&
            (leaf < 0 || leaf == (int)(header >> 31))) {
            out->data = end + 1;
            out->size = size - (size_t)(end + 1 - record);
            return 1;
        }
        cursor += size;
    }
    return 0;
}
static int path(Field root, const char *const *names, size_t count, int leaf, Field *out) {
    for (size_t i = 0; i < count; ++i)
        if (!child(root, names[i], i + 1 == count ? leaf : 0, &root)) return 0;
    *out = root;
    return 1;
}
/* A string container's text, without the ":NN" resource-kind suffix. */
static void string_at(Field root, const char *const *names, size_t count, char *out, size_t capacity) {
    Field holder, value;
    size_t length = 0;
    *out = 0;
    if (!path(root, names, count, 0, &holder) || !child(holder, "string", 1, &value)) return;
    while (length < value.size && value.data[length] && value.data[length] != ':' && length + 1 < capacity) {
        out[length] = (char)value.data[length];
        ++length;
    }
    out[length] = 0;
}

/* Text table: u16 version, u16 group count; per group (u16 id, u16 count,
 * u32 offset, u32 size); a group holds count entries (u16 text id, u16
 * characters, u32 byte offset) followed by UTF-16LE strings. */
static void text_lookup(const uint8_t *text, size_t size, unsigned id, char *out, size_t capacity) {
    unsigned groups;
    *out = 0;
    if (!text || size < 4) return;
    groups = u16(text + 2);
    if ((size_t)groups * 12u > size - 4) return;
    for (unsigned g = 0; g < groups; ++g) {
        const uint8_t *group = text + 4 + g * 12u;
        unsigned count = u16(group + 2);
        uint32_t offset = u32(group + 4);
        size_t base;
        if (offset > size || (size_t)count * 8u > size - offset) continue;
        base = offset + (size_t)count * 8u;
        for (unsigned i = 0; i < count; ++i) {
            const uint8_t *entry = text + offset + i * 8u;
            size_t chars = u16(entry + 2), start = base + u32(entry + 4), used = 0;
            if (u16(entry) != id) continue;
            if (start > size || chars * 2u > size - start) return;
            for (size_t c = 0; c < chars; ++c) {
                uint32_t code = u16(text + start + c * 2u);
                char encoded[4];
                size_t n;
                if (code >= 0xd800u && code < 0xdc00u && c + 1 < chars) {
                    uint32_t low = u16(text + start + (c + 1) * 2u);
                    if (low >= 0xdc00u && low < 0xe000u) { code = 0x10000u + ((code - 0xd800u) << 10) + (low - 0xdc00u); ++c; }
                }
                if (code < 0x80u) { encoded[0] = (char)code; n = 1; }
                else if (code < 0x800u) { encoded[0] = (char)(0xc0u | (code >> 6)); encoded[1] = (char)(0x80u | (code & 63u)); n = 2; }
                else if (code < 0x10000u) { encoded[0] = (char)(0xe0u | (code >> 12)); encoded[1] = (char)(0x80u | ((code >> 6) & 63u)); encoded[2] = (char)(0x80u | (code & 63u)); n = 3; }
                else { encoded[0] = (char)(0xf0u | (code >> 18)); encoded[1] = (char)(0x80u | ((code >> 12) & 63u)); encoded[2] = (char)(0x80u | ((code >> 6) & 63u)); encoded[3] = (char)(0x80u | (code & 63u)); n = 4; }
                if (used + n + 1 > capacity) break;
                memcpy(out + used, encoded, n);
                used += n;
            }
            while (used && out[used - 1] == ' ') --used; /* "Veshchenega " */
            out[used] = 0;
            return;
        }
    }
}

int SudekiMpModItemsIsManager(const void *data, size_t size) {
    return data && size >= 18 && !memcmp((const uint8_t *)data + 4, "CItemManager", 13);
}

static int hero_of(const char *type, int *shadow) {
    static const char *const heroes[] = {"Tal", "Alice", "Buki", "Elco"};
    const char *rest = type;
    if (!strncmp(rest, "Weapon", 6)) rest += 6;
    else if (!strncmp(rest, "Armour", 6)) rest += 6;
    else return -1;
    *shadow = !strncmp(rest, "Dark", 4);
    if (*shadow) rest += 4;
    for (int h = 0; h < 4; ++h)
        if (!strcmp(rest, heroes[h])) return h;
    return -1;
}

int SudekiMpModItemsParse(const void *manager, size_t manager_size,
    const void *text, size_t text_size, SudekiMpModItems *items,
    char *error, size_t capacity) {
    Field top = {(const uint8_t *)manager, manager_size}, root;
    size_t cursor = 0, used = 0, allocated = 0;
    if (!items) return fail(error, capacity, "No item output");
    memset(items, 0, sizeof(*items));
    if (!SudekiMpModItemsIsManager(manager, manager_size) || !child(top, "CItemManager", 0, &root))
        return fail(error, capacity, "Not the item manager resource");
    while (root.size - cursor >= 5) {
        uint32_t header = u32(root.data + cursor), size = header & 0x7fffffffu;
        const uint8_t *record = root.data + cursor, *end;
        Field item, name_field;
        SudekiMpModItem entry;
        char type[48];
        static const char *const type_path[] = {"Item type"};
        static const char *const model_path[] = {"Item Common Data", "Item Model"};
        static const char *const icon_path[] = {"Item Common Data", "UI Icon Filename"};
        static const char *const look_path[] = {"Item Common Data", "Equippable Properties", "Appearance Change Model"};
        static const char *const name_path[] = {"Item Common Data", "Item Name"};
        if (size < 5 || size > root.size - cursor) { SudekiMpModItemsFree(items); return fail(error, capacity, "Truncated item record"); }
        end = memchr(record + 4, 0, size - 4);
        cursor += size;
        if (!end || (header >> 31) || end - record - 4 < 6 || memcmp(record + 4, "Item ", 5)) continue;
        item.data = end + 1; item.size = size - (size_t)(end + 1 - record);
        memset(&entry, 0, sizeof(entry));
        entry.id = (unsigned)strtoul((const char *)record + 9, NULL, 10);
        string_at(item, type_path, 1, type, sizeof(type));
        entry.hero = hero_of(type, &entry.shadow);
        if (entry.hero < 0) continue;
        entry.kind = type[0] == 'W' ? SUDEKIMP_MOD_ITEM_WEAPON : SUDEKIMP_MOD_ITEM_ARMOUR;
        if (entry.kind == SUDEKIMP_MOD_ITEM_WEAPON) string_at(item, model_path, 2, entry.model, sizeof(entry.model));
        else string_at(item, look_path, 3, entry.model, sizeof(entry.model));
        string_at(item, icon_path, 2, entry.icon, sizeof(entry.icon));
        /* Weapons also have a large menu picture: SUI_Wnnn_FULL. */
        if (entry.kind == SUDEKIMP_MOD_ITEM_WEAPON && !strncmp(entry.icon, "SUI_W", 5) &&
            strlen(entry.icon) >= 8)
            snprintf(entry.picture, sizeof(entry.picture), "SUI_%.4s_FULL.SQX", entry.icon + 4);
        if (path(item, name_path, 2, 1, &name_field) && name_field.size >= 2)
            text_lookup((const uint8_t *)text, text ? text_size : 0, u16(name_field.data), entry.name, sizeof(entry.name));
        if (!entry.model[0] && !entry.icon[0]) continue;
        if (used == allocated) {
            size_t grown_size = allocated ? allocated * 2 : 64;
            SudekiMpModItem *grown = (SudekiMpModItem *)realloc(items->items, grown_size * sizeof(*grown));
            if (!grown) { SudekiMpModItemsFree(items); return fail(error, capacity, "Not enough memory for items"); }
            items->items = grown; allocated = grown_size;
        }
        items->items[used++] = entry;
        items->count = used;
    }
    if (error && capacity) *error = 0;
    return 1;
}
void SudekiMpModItemsFree(SudekiMpModItems *items) {
    if (!items) return;
    free(items->items);
    memset(items, 0, sizeof(*items));
}
