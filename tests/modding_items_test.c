/* Item definitions and text table: synthetic field trees only. */
#include "modding/mod_items.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "items:%d: %s\n", __LINE__, #expr); return 1; } } while (0)

typedef struct Buf { uint8_t data[8192]; size_t size; } Buf;
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
/* Opens a record; close() patches its size. */
static size_t open_record(Buf *b, const char *name) {
    size_t at = b->size, n = strlen(name) + 1;
    b->size += 4; memcpy(b->data + b->size, name, n); b->size += n;
    return at;
}
static void close_record(Buf *b, size_t at, int leaf) { put32(b->data + at, (uint32_t)(b->size - at) | (leaf ? 0x80000000u : 0u)); }
static void leaf(Buf *b, const char *name, const void *value, size_t size) {
    size_t at = open_record(b, name);
    memcpy(b->data + b->size, value, size); b->size += size;
    close_record(b, at, 1);
}
static void string_field(Buf *b, const char *name, const char *value) {
    size_t at = open_record(b, name);
    uint32_t length = (uint32_t)strlen(value) + 1;
    leaf(b, "length", &length, 4);
    leaf(b, "string", value, length);
    close_record(b, at, 0);
}
static void item(Buf *b, const char *record, const char *type, uint16_t name_id,
                 const char *model, const char *icon, const char *look) {
    size_t at = open_record(b, record), common;
    string_field(b, "Item type", type);
    common = open_record(b, "Item Common Data");
    string_field(b, "Item Type", type);
    string_field(b, "Item Model", model);
    leaf(b, "Item Name", &name_id, 2);
    string_field(b, "UI Icon Filename", icon);
    { size_t equip = open_record(b, "Equippable Properties"); string_field(b, "Appearance Change Model", look); close_record(b, equip, 0); }
    close_record(b, common, 0);
    close_record(b, at, 0);
}

int SudekiMpModItemsTests(void) {
    static Buf manager, text;
    SudekiMpModItems items = {0};
    char error[128];
    size_t root;
    /* Text table: one group of three strings (one with a trailing space and a non-ASCII letter). */
    static const char *const strings[] = {"Runic Blade", "Mojo", "R\xc3\xa9ka "};
    {
        uint8_t *t = text.data;
        size_t group = 4 + 12, entries = group, chars = entries + 3 * 8, used = 0;
        t[0] = 3; t[2] = 1; /* version 3, one group */
        t[4] = 37; t[6] = 3; put32(t + 8, (uint32_t)group); /* id 37, 3 entries */
        for (int i = 0; i < 3; ++i) {
            const char *s = strings[i];
            uint16_t id = (uint16_t)(0x16ee + i * 8), n = 0;
            for (size_t c = 0; s[c]; ++c) {
                uint16_t code = (uint8_t)s[c];
                if (code == 0xc3) { code = (uint16_t)(0xc0 | ((uint8_t)s[++c] & 63)); }
                t[chars + used + n * 2] = (uint8_t)code; t[chars + used + n * 2 + 1] = (uint8_t)(code >> 8); ++n;
            }
            t[entries + i * 8] = (uint8_t)id; t[entries + i * 8 + 1] = (uint8_t)(id >> 8);
            t[entries + i * 8 + 2] = (uint8_t)n; put32(t + entries + i * 8 + 4, (uint32_t)used);
            used += n * 2u;
        }
        put32(t + 12, (uint32_t)(chars - group + used));
        text.size = chars + used;
    }
    manager.size = 0;
    root = open_record(&manager, "CItemManager");
    item(&manager, "Item 004", "WeaponTal", 0x16ee, "W005_MRCHOPPY.HOM:41", "SUI_W005_MRCHOPPY.SQX:42", "");
    item(&manager, "Item 007", "WeaponTal", 0x16f6, "W007_MOJO.HOM:41", "SUI_W007_MOJO.SQX:42", "");
    item(&manager, "Item 101", "ArmourDarkBuki", 0x16fe, "", "SUI_ARMOUR_NICO.SQX:42", "BUKI_SHADOW.HOM:41");
    item(&manager, "Item 600", "Quest", 0x16ee, "KEY.HOM:41", "SUI_KEY.SQX:42", "");
    item(&manager, "Item 132", "Armour", 0x16ee, "", "SUI_ARMOUR_KAZEL.SQX:42", "");
    close_record(&manager, root, 0);

    CHECK(SudekiMpModItemsIsManager(manager.data, manager.size));
    CHECK(SudekiMpModItemsParse(manager.data, manager.size, text.data, text.size, &items, error, sizeof(error)));
    CHECK(items.count == 3); /* the quest item and the hero-less armour are left out */
    CHECK(items.items[0].id == 4 && items.items[0].kind == SUDEKIMP_MOD_ITEM_WEAPON && items.items[0].hero == 0);
    CHECK(!strcmp(items.items[0].name, "Runic Blade") && !strcmp(items.items[0].model, "W005_MRCHOPPY.HOM"));
    CHECK(!strcmp(items.items[0].icon, "SUI_W005_MRCHOPPY.SQX") && !strcmp(items.items[0].picture, "SUI_W005_FULL.SQX"));
    CHECK(!strcmp(items.items[1].name, "Mojo") && !strcmp(items.items[1].model, "W007_MOJO.HOM"));
    CHECK(items.items[2].kind == SUDEKIMP_MOD_ITEM_ARMOUR && items.items[2].hero == 2 && items.items[2].shadow);
    CHECK(!strcmp(items.items[2].model, "BUKI_SHADOW.HOM") && !items.items[2].picture[0]);
    CHECK(!strcmp(items.items[2].name, "R\xc3\xa9ka")); /* UTF-8, trailing space trimmed */
    SudekiMpModItemsFree(&items);

    /* Without text, names are empty; a truncated tree is refused. */
    CHECK(SudekiMpModItemsParse(manager.data, manager.size, NULL, 0, &items, error, sizeof(error)) && items.count == 3 && !items.items[0].name[0]);
    SudekiMpModItemsFree(&items);
    put32(manager.data + 4 + 13, 0x7fffff00u); /* first item record: size past the end */
    CHECK(!SudekiMpModItemsParse(manager.data, manager.size, NULL, 0, &items, error, sizeof(error)) && !items.count);
    CHECK(!SudekiMpModItemsParse(text.data, text.size, NULL, 0, &items, error, sizeof(error)));
    return 0;
}
