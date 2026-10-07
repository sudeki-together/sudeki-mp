#include "hooks/armour_menu.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Armour menu requires the supported x86 ABI"
#endif

/* RVAs on the supported image (CONFIRMED_STATIC, docs/armour-choice.md). */
enum {
    MENU_CASE2_SLOT = 0x93cfc,   /* Main Menu dispatch jump table [2] (Armor icon) */
    MENU_CASE2 = 0x93c01, MENU_CONTINUE = 0x93cc5, MENU_FAIL = 0x93cc3,
    WEAPONS_CTOR = 0x16dbd0,     /* ESI = 0x15C bytes, returns EAX = layer */
    WEAPONS_DTOR_SLOT = 0x2d8908, WEAPONS_DTOR = 0x16dcf0,
    LIST_FILL = 0x16fc60, EQUIP = 0x1706d0, DETAILS = 0x16f5b0,
    LIST_CLEAR = 0x17d720, LIST_ADD = 0x17d7a0, LIST_SELECT = 0x17d3c0,
    STRING_ASSIGN = 0x1b9fc0, SET_TEXT = 0x11f2b0, ARMOUR_EQUIP = 0x160580, SET_ICON = 0x15c0e0,
    GAME_NEW = 0x2484fa, GAME_FREE_ARRAY = 0x248916,
    REF_COPY = 0x15b0, PREVIEW_SETUP = 0x160f90, PREVIEW_UPDATE = 0x161b50, UI_PREVIEW = 0x178, UI_PREVIEW_FLAG = 0x230,
    UI_GLOBAL = 0x3c2f78, PARTY_REFS = 0x408d94, INVENTORY = 0x408d84, ITEM_DB = 0x408d80,
    LOCALIZER = 0x409e0c, TEXT_MANAGER = 0x3c3034, ROW_COLOURS = 0x33542c,
    /* Layer / widget block (layer+0x8C) offsets used by the Weapons page. */
    LAYER_WIDGETS = 0x8c, W_LIST = 0x7d0, W_LIST_SELECTED = 0x858, W_LIST_ROWS = 0x88c, ROW_ITEM = 0x84,
    W_HINT_SOURCE = 0x268, W_HINT_HANDLE = 0x6b8, W_TITLE_BASE = 0x330,
    W_STAT_A = 0x478, W_STAT_B = 0x4b8, W_RATE_LABEL = 0x410, W_RATE_VALUE = 0x4d0,
    W_ICON = 0x000, W_SECTION_LABELS = 0x3d0, ITEM_ICON = 0x78, TEXT_ID = 0x4, TEXT_STRING = 0x38,
    /* Item / character fields. */
    ITEM_ID = 0x14, ITEM_NAME = 0x50, ITEM_USED_ON = 0x84, CHAR_EQUIPPED = 0xe0, EQUIPPED_ARMOUR = 0x18, CHAR_TYPE_COMPONENT = 0x2c,
    MAX_ITEMS = 999, MAX_ROWS = 64
};

static const uint8_t fill_bytes[6] = {0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8};
static const uint8_t equip_bytes[8] = {0x83, 0xec, 0x18, 0x53, 0x8b, 0x5c, 0x24, 0x20};
static const uint8_t details_bytes[6] = {0x55, 0x8b, 0xec, 0x83, 0xe4, 0xc0};
static const struct { uint32_t rva; uint8_t bytes[6]; } helpers[] = {
    {LIST_CLEAR, {0x53, 0x55, 0x8d, 0x86, 0xb0, 0x00}}, {LIST_ADD, {0x83, 0xec, 0x08, 0x53, 0x8b, 0x5c}},
    {LIST_SELECT, {0x53, 0x55, 0x56, 0x8b, 0xf1, 0x8b}}, {STRING_ASSIGN, {0x55, 0x8b, 0x6c, 0x24, 0x08, 0x85}},
    {SET_TEXT, {0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8}}, {ARMOUR_EQUIP, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x55}},
    {GAME_NEW, {0x8b, 0xff, 0x55, 0x8b, 0xec, 0x83}}, {SET_ICON, {0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8}},
    {REF_COPY, {0x8b, 0x11, 0x8b, 0xca, 0x89, 0x10}}, {PREVIEW_SETUP, {0x83, 0xec, 0x18, 0x8b, 0x54, 0x24}},
    {PREVIEW_UPDATE, {0x53, 0x8b, 0x5c, 0x24, 0x08, 0x83}}, {GAME_FREE_ARRAY, {0x8b, 0xff, 0x55, 0x8b, 0xec, 0x5d}},
    {WEAPONS_CTOR, {0x51, 0x53, 0x57, 0xc7, 0x46, 0x04}},
};

typedef void (__stdcall *ListAdd)(void *, int, void *, int, uint32_t);
typedef void (__attribute__((thiscall)) *ListSelect)(void *, int);
typedef void (__stdcall *SetText)(void *, uint32_t, const wchar_t *);
typedef void (__stdcall *LayerFunction)(void *);
typedef void (__cdecl *FreeArray)(void *);
typedef int (__attribute__((thiscall)) *Method0)(void *);
typedef int (__attribute__((thiscall)) *Method1)(void *, void *);
typedef const wchar_t *(__attribute__((thiscall)) *Localize)(void *, int);

static uint8_t *base;
static BOOL debug_labels;
void *volatile SudekiMpArmourMenuLayer __attribute__((used));
void *SudekiMpArmourMenuNew __attribute__((used)), *SudekiMpArmourMenuCtor __attribute__((used));
void *SudekiMpArmourMenuContinue __attribute__((used)), *SudekiMpArmourMenuFail __attribute__((used));
void *SudekiMpArmourMenuFillTrampoline __attribute__((used)), *SudekiMpArmourMenuEquipTrampoline __attribute__((used));
void *SudekiMpArmourMenuDetailsTrampoline __attribute__((used)), *SudekiMpArmourMenuDtor __attribute__((used));
void *SudekiMpArmourMenuRefCopy __attribute__((used)), *SudekiMpArmourMenuPreviewSetup __attribute__((used));
static SudekiMpPointerHook case_hook, dtor_hook;
static SudekiMpInlineHook fill_hook, equip_hook, details_hook;

static BOOL readable(const void *p, size_t n) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t a = (uintptr_t)p;
    if (!p || !n || a > UINTPTR_MAX - n || VirtualQuery(p, &m, sizeof(m)) != sizeof(m) || m.State != MEM_COMMIT ||
        (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || a + n > (uintptr_t)m.BaseAddress + m.RegionSize) return FALSE;
    return TRUE;
}
static uint8_t *ptr_at(const void *p) { return readable(p, 4) ? *(uint8_t *const *)p : NULL; }
static void *method(void *object, unsigned slot) {
    uint8_t *vtable = ptr_at(object);
    return vtable && readable(vtable + slot * 4u, 4) ? *(void **)(vtable + slot * 4u) : NULL;
}

/* ---- native helpers with register arguments ---- */
static void list_clear(void *list) {
    void *f = base + LIST_CLEAR;
    __asm__ volatile("push %%esi; mov %%eax,%%esi; call *%%edx; pop %%esi" : "+a"(list), "+d"(f) : : "ecx", "memory");
}
/* dst: native string {flags, data}; flags bit 31 = inline storage. */
static void string_assign(void *dst, const wchar_t *src) {
    void *f = base + STRING_ASSIGN;
    __asm__ volatile("push %%edi; mov %%eax,%%edi; push %%ecx; call *%%edx; pop %%edi" : "+a"(dst), "+c"(src), "+d"(f) : : "memory");
}
static void armour_equip(void *item, void *equipped) {
    void *f = base + ARMOUR_EQUIP;
    __asm__ volatile("push %%ecx; call *%%edx" : "+a"(item), "+c"(equipped), "+d"(f) : : "memory");
}
/* 0x55C0E0: EDI = icon widget, stack {descriptor[3], 0}, RET 0x10; the
 * descriptor's third word is a counted reference the callee takes over. */
static void set_icon(uint8_t *widget, const uint32_t descriptor[3]) {
    void *f = base + SET_ICON;
    __asm__ volatile("push %%edi; mov %%eax,%%edi; push $0; push 8(%%ecx); push 4(%%ecx); push (%%ecx); call *%%edx; pop %%edi"
        : "+a"(widget), "+c"(descriptor), "+d"(f) : : "memory");
}
static const wchar_t *string_text(uint8_t *s) {
    if (!readable(s, 8)) return NULL;
    return (*(uint32_t *)s & 0x80000000u) ? (const wchar_t *)(s + 4) : (const wchar_t *)ptr_at(s + 4);
}
static void set_text(uint8_t *widgets, unsigned handle_offset, const wchar_t *text) {
    if (!readable(widgets + handle_offset, 4) || !text) return;
    ((SetText)(void *)(base + SET_TEXT))(ptr_at(base + TEXT_MANAGER), *(uint32_t *)(widgets + handle_offset), text);
}
static void set_widget_state(uint8_t *widget, uint32_t state) {
    Method0 refresh = (Method0)method(widget, 5);
    if (!refresh || !readable(widget, 0x40)) return;
    *(uint32_t *)(widget + 0x1c) = state; /* 0 shown, 2 hidden */
    refresh(widget);
}

/* The menu's 3D character is a separate preview model that the game builds
 * when the menu opens or the character changes (Main Menu 0x496300): copy
 * the party weak reference onto the stack, UIModelManager::Setup(ui+0x178,
 * ref) (0x560F90, RET 0x10, releases the ref; loads body, armour skin from
 * the equipped armour, then weapon), copy the ui+0x230 flag, then
 * 0x561B50(manager). Re-run it so the preview shows the new armour. */
static void refresh_preview(uint8_t *ui, void *party_ref) {
    uint8_t *manager = ui + UI_PREVIEW;
    __asm__ volatile(
        "sub $12,%%esp; mov %%esp,%%eax; call *_SudekiMpArmourMenuRefCopy;"
        "push %%ebx; call *_SudekiMpArmourMenuPreviewSetup"
        : "+c"(party_ref), "+b"(manager) : : "eax", "edx", "memory");
    manager[0xb] = ui[UI_PREVIEW_FLAG];
    ((LayerFunction)(void *)(base + PREVIEW_UPDATE))(manager);
}

/* ---- game state ---- */
static void *current_party_ref(void) {
    uint8_t *ui = ptr_at(base + UI_GLOBAL), *refs = ptr_at(base + PARTY_REFS);
    int index;
    if (!ui || !refs || !readable(ui + 0x170, 4)) return NULL;
    index = *(int *)(ui + 0x170);
    return index >= 0 && index <= 15 ? refs + (index * 3 + 0x24) * 4 : NULL;
}
static uint8_t *current_character(void) {
    uint8_t *ui = ptr_at(base + UI_GLOBAL), *refs = ptr_at(base + PARTY_REFS);
    int index;
    if (!ui || !refs || !readable(ui + 0x170, 4)) return NULL;
    index = *(int *)(ui + 0x170);
    if (index < 0 || index > 15) return NULL;
    return ptr_at(refs + (index * 3 + 0x24) * 4);
}
/* Armour item class this character may wear: the exact pairs of
 * CItemArmour::CanUse (0x530080). Heroes wear their own armour, shadow
 * counterparts (types 8-11) the Dark armour, merged forms (0x13-0x16) the
 * Merged armour. -1 for any other character. */
static int wearable_armour_type(uint8_t *character) {
    static const int pairs[][2] = {
        {1, 0x14}, {5, 0x15}, {0x23, 0x13}, {0xe, 0x16},
        {8, 0x18}, {9, 0x19}, {10, 0x1a}, {0xb, 0x17},
        {0x13, 0x1c}, {0x14, 0x1d}, {0x15, 0x1e}, {0x16, 0x1b},
    };
    Method0 type = (Method0)method(character + CHAR_TYPE_COMPONENT, 4);
    int character_type;
    if (!type) return -1;
    character_type = type(character + CHAR_TYPE_COMPONENT);
    for (unsigned i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i)
        if (pairs[i][0] == character_type) return pairs[i][1];
    return -1;
}
static uint8_t *item_by_id(int id) {
    uint8_t *db = ptr_at(base + ITEM_DB);
    return db && id >= 0 && id < MAX_ITEMS ? ptr_at(db + 0xc + id * 4) : NULL;
}
static uint8_t *equipped_component(uint8_t *character) { return character ? ptr_at(character + CHAR_EQUIPPED) : NULL; }
static uint8_t *equipped_armour(uint8_t *character) {
    uint8_t *equipped = equipped_component(character);
    return equipped ? ptr_at(equipped + EQUIPPED_ARMOUR) : NULL;
}
static int item_type(uint8_t *item) {
    Method0 type = (Method0)method(item, 2);
    return type ? type(item) : -1;
}
static BOOL armour_type_for(int type, int wearable) { return wearable >= 0 && type == wearable; }

/* Owned armour the character can wear: inventory categories are keyed by item type. */
static unsigned owned_armour(int wearable, int ids[MAX_ROWS]) {
    uint8_t *inventory = ptr_at(base + INVENTORY), *categories;
    unsigned rows = 0;
    int count;
    if (!inventory || !readable(inventory + 0x12c, 4) || wearable < 0) return 0;
    count = *(int *)(inventory + 0x12c);
    categories = ptr_at(inventory + 0xc);
    if (count <= 0 || count > 256 || !readable(categories, count * 4u)) return 0;
    {
        int wanted = wearable;
        for (int c = 0; c < count; ++c) {
            uint8_t *category = ptr_at(categories + c * 4), *entries;
            int entries_count;
            if (!readable(category, 0x10) || *(int *)(category + 8) != wanted) continue;
            entries_count = *(int16_t *)(category + 0xe) + 1;
            entries = ptr_at(category + 4);
            if (entries_count <= 0 || entries_count > MAX_ROWS || !readable(entries, entries_count * 4u)) continue;
            for (int e = 0; e < entries_count && rows < MAX_ROWS; ++e) {
                int id = *(int16_t *)(entries + e * 4);
                uint8_t *item = item_by_id(id);
                BOOL seen = FALSE;
                for (unsigned r = 0; r < rows; ++r) if (ids[r] == id) seen = TRUE;
                if (item && !seen && armour_type_for(item_type(item), wearable)) ids[rows++] = id;
            }
        }
    }
    return rows;
}

static void armour_details(uint8_t *layer);

/* Replaces the Weapons list fill (0x56FC60) for the armour layer. */
static void __attribute__((used, noinline)) armour_fill(uint8_t *layer) {
    DWORD saved = GetLastError();
    uint8_t *widgets = ptr_at(layer + LAYER_WIDGETS), *character = current_character(), *current;
    uint32_t *colours = (uint32_t *)(base + ROW_COLOURS);
    int ids[MAX_ROWS], hero, selected = -1;
    unsigned rows;
    if (!widgets) goto done;
    list_clear(widgets + W_LIST);
    hero = character ? wearable_armour_type(character) : -1;
    current = equipped_armour(character);
    rows = owned_armour(hero, ids);
    for (unsigned r = 0; r < rows; ++r) {
        uint8_t *item = item_by_id(ids[r]);
        /* Native string: {flags, data | inline storage}; locals reserve 60 bytes. */
        uint32_t text[16] = {0x80000000u, 0};
        const wchar_t *name = NULL;
        void *localizer = ptr_at(base + LOCALIZER);
        Localize localize = (Localize)method(localizer, 4);
        if (item == current) selected = (int)r;
        if (localize && *(int *)(item + ITEM_NAME) != -1) name = localize(localizer, *(int *)(item + ITEM_NAME));
        if (name) string_assign(text, name);
        ((ListAdd)(void *)(base + LIST_ADD))(widgets + W_LIST, ids[r], text, -1, colours[item == current ? 6 : 0]);
        if (!(text[0] & 0x80000000u) && text[1]) ((FreeArray)(void *)(base + GAME_FREE_ARRAY))((void *)(uintptr_t)text[1]);
    }
    ((ListSelect)(void *)(base + LIST_SELECT))(widgets + W_LIST, selected);
    armour_details(layer);
    SudekiMpLogFormat("armour_menu event=fill armour_type=%d owned=%u equipped_row=%d\r\n", hero, rows, selected);
done:
    SetLastError(saved);
}

/* Replaces the Weapons equip (0x5706D0) for the armour layer. */
static void __attribute__((used, noinline)) armour_select(uint8_t *layer) {
    DWORD saved = GetLastError();
    uint8_t *widgets = ptr_at(layer + LAYER_WIDGETS), *character = current_character(), *equipped, *rows, *row, *item;
    int index, hero;
    if (!widgets || !character || !readable(widgets + W_LIST_SELECTED, 4)) goto done;
    index = *(int *)(widgets + W_LIST_SELECTED);
    rows = ptr_at(widgets + W_LIST_ROWS);
    if (index < 0 || index >= MAX_ROWS || !rows || !(row = ptr_at(rows + index * 4)) || !readable(row + ROW_ITEM, 4)) goto done;
    item = item_by_id(*(int *)(row + ROW_ITEM));
    hero = wearable_armour_type(character);
    equipped = equipped_component(character);
    if (!item || !equipped || item == equipped_armour(character) || !armour_type_for(item_type(item), hero)) goto done;
    /* Same rule as CItemArmour::CanUse (0x530080), which takes a character
     * weak reference rather than the object: usage 1 or 2 ("Item Used On")
     * and an armour class of this hero (checked above). */
    if (!readable(item + ITEM_USED_ON, 4) || (*(int *)(item + ITEM_USED_ON) != 1 && *(int *)(item + ITEM_USED_ON) != 2)) {
        SudekiMpLogFormat("armour_menu event=refused item=%d reason=cannot_use\r\n", *(int *)(item + ITEM_ID));
        goto done;
    }
    /* Each armour keeps its own runes. The native equip copies the outgoing
     * armour's runes (built-in and socketed) into the incoming armour's free
     * sockets without clearing them, which suits one-way story upgrades but
     * would duplicate runes on free swapping. With no current armour it skips
     * that step; socketed runes stay stored per armour ID in the inventory. */
    *(uint8_t **)(equipped + EQUIPPED_ARMOUR) = NULL;
    armour_equip(item, equipped);
    {
        uint8_t *ui = ptr_at(base + UI_GLOBAL);
        void *party_ref = current_party_ref();
        ui[0x180] = 1; /* as the native weapon equip does */
        if (party_ref && ptr_at(party_ref) == character && readable(ui + UI_PREVIEW, 0x40)) refresh_preview(ui, party_ref);
    }
    SudekiMpLogFormat("armour_menu event=equip armour_type=%d item=%d\r\n", hero, *(int *)(item + ITEM_ID));
    armour_fill(layer);
    ((ListSelect)(void *)(base + LIST_SELECT))(widgets + W_LIST, index);
done:
    SetLastError(saved);
}

/* The Weapons page's section label group shares one text ID across four
 * entries created in layout order: Enchantments, then the weapon-only
 * Lethality, Strength and Critical (CONFIRMED_LIVE order). Keep the first and
 * blank the rest, so any localisation works. */
static void hide_weapon_labels(uint8_t *widgets) {
    uint8_t *manager = ptr_at(base + TEXT_MANAGER), *entries;
    unsigned count, seen = 0;
    int16_t id;
    if (!manager || !readable(manager + 0x8, 0xc) || !readable(widgets + W_SECTION_LABELS + 0x28, 4)) return;
    id = (int16_t)*(uint32_t *)(widgets + W_SECTION_LABELS + 0x28);
    count = *(uint32_t *)(manager + 0x8);
    entries = ptr_at(manager + 0x10);
    if (count > 4096 || !readable(entries, count * 4u)) return;
    for (unsigned i = 0; i < count; ++i) {
        uint8_t *entry = ptr_at(entries + i * 4);
        if (!readable(entry, TEXT_STRING + 8) || *(int16_t *)(entry + TEXT_ID) != id) continue;
        if (seen++) string_assign(entry + TEXT_STRING, L"");
    }
}

/* Native details (name, description, rune Enchantments), then the armour's
 * own UI icon and no weapon-only stats. */
static void armour_details(uint8_t *layer) {
    uint8_t *widgets, *rows, *row, *item = NULL;
    int index;
    ((LayerFunction)SudekiMpArmourMenuDetailsTrampoline)(layer);
    if (!(widgets = ptr_at(layer + LAYER_WIDGETS))) return;
    set_text(widgets, W_STAT_A, L"");
    set_text(widgets, W_STAT_B, L"");
    set_widget_state(widgets + W_RATE_LABEL, 2);
    set_widget_state(widgets + W_RATE_VALUE, 2);
    hide_weapon_labels(widgets);
    /* The Weapons page picks its icon from a per-ID weapon table; armour uses
     * the item's "UI Icon Filename" descriptor, as the Armour page does. */
    if (readable(widgets + W_LIST_SELECTED, 4) && (index = *(int *)(widgets + W_LIST_SELECTED)) >= 0 && index < MAX_ROWS &&
        (rows = ptr_at(widgets + W_LIST_ROWS)) && (row = ptr_at(rows + index * 4)) && readable(row + ROW_ITEM, 4))
        item = item_by_id(*(int *)(row + ROW_ITEM));
    if (item && readable(item + ITEM_ICON, 12)) {
        uint32_t descriptor[3];
        descriptor[0] = *(uint32_t *)(item + ITEM_ICON) & 0x1fffu;
        descriptor[1] = *(uint32_t *)(item + ITEM_ICON + 4);
        descriptor[2] = *(uint32_t *)(item + ITEM_ICON + 8);
        if (descriptor[2] && readable((void *)(uintptr_t)descriptor[2], 4)) ++*(uint32_t *)(uintptr_t)descriptor[2];
        set_icon(widgets + W_ICON, descriptor);
    }
    if (debug_labels) { /* Research: name every static label so a screenshot maps them. */
        static const unsigned labels[] = {0x3d0, 0x5d0, 0x610, 0x650, 0x6d0, 0x790};
        for (unsigned i = 0; i < sizeof(labels) / sizeof(labels[0]); ++i) {
            wchar_t text[16];
            _snwprintf(text, 16, L"#%03X", labels[i]);
            text[15] = 0;
            set_text(widgets, labels[i] + 0x28, text);
        }
    }
}
static void __attribute__((used, noinline)) armour_details_entry(uint8_t *layer) {
    DWORD saved = GetLastError();
    armour_details(layer);
    SetLastError(saved);
}

/* A new armour layer: retitle and rehint it through the game's own strings. */
static void __attribute__((used, noinline)) armour_layer_created(uint8_t *layer) {
    DWORD saved = GetLastError();
    uint8_t *widgets = ptr_at(layer + LAYER_WIDGETS);
    SudekiMpArmourMenuLayer = layer;
    if (widgets) {
        string_assign(widgets + W_TITLE_BASE, L"ARMOR");
        string_assign(widgets + W_HINT_SOURCE, L"View and equip armor from the list.");
        set_text(widgets, W_HINT_HANDLE, string_text(widgets + W_HINT_SOURCE));
    }
    SudekiMpLogFormat("armour_menu event=open layer=%p\r\n", layer);
    SetLastError(saved);
}

/* Jump-table target for the Armor icon: the Weapons layer, remembered. Runs
 * in the dispatch frame (EBX = Main Menu); continues at its common tail. */
static void __attribute__((naked, noinline)) case_stub(void) {
    __asm__ volatile(
        "push $0x15c; call *_SudekiMpArmourMenuNew; add $4,%esp;"
        "test %eax,%eax; jz 1f;"
        "mov %eax,%esi; call *_SudekiMpArmourMenuCtor;"
        "pushal; mov %esp,%ebp; and $-16,%esp; sub $16,%esp; mov %eax,(%esp); call _armour_layer_created;"
        "mov %ebp,%esp; popal; jmp *_SudekiMpArmourMenuContinue;"
        "1: jmp *_SudekiMpArmourMenuFail");
}
#define LAYER_STUB(name, target, trampoline) \
    static void __attribute__((naked, noinline)) name(void) { \
        __asm__ volatile( \
            "mov 4(%esp),%eax; cmp _SudekiMpArmourMenuLayer,%eax; jne 1f;" \
            "push %ebp; mov %esp,%ebp; and $-16,%esp; sub $16,%esp; mov %eax,(%esp); call _" #target ";" \
            "mov %ebp,%esp; pop %ebp; ret $4;" \
            "1: jmp *_" #trampoline); \
    }
LAYER_STUB(fill_stub, armour_fill, SudekiMpArmourMenuFillTrampoline)
LAYER_STUB(equip_stub, armour_select, SudekiMpArmourMenuEquipTrampoline)
LAYER_STUB(details_stub, armour_details_entry, SudekiMpArmourMenuDetailsTrampoline)
/* Destructor slot (thiscall): forget the layer before the native destructor. */
static void __attribute__((naked, noinline)) dtor_stub(void) {
    __asm__ volatile(
        "cmp _SudekiMpArmourMenuLayer,%ecx; jne 1f; movl $0,_SudekiMpArmourMenuLayer;"
        "1: jmp *_SudekiMpArmourMenuDtor");
}

static BOOL config_enabled(const wchar_t *path, const wchar_t *key, BOOL fallback) {
    wchar_t value[16];
    if (!GetPrivateProfileStringW(L"Menus", key, L"", value, 16, path) || !value[0]) return fallback;
    return !(!_wcsicmp(value, L"false") || !_wcsicmp(value, L"0") || !_wcsicmp(value, L"no") || !_wcsicmp(value, L"off"));
}

BOOL SudekiMpArmourMenuUninstall(void) {
    if (!base) return TRUE;
    if (case_hook.installed && !SudekiMpRestorePointerHook(&case_hook)) return FALSE;
    if (details_hook.installed && !SudekiMpRestoreInlineHook(&details_hook)) return FALSE;
    if (equip_hook.installed && !SudekiMpRestoreInlineHook(&equip_hook)) return FALSE;
    if (fill_hook.installed && !SudekiMpRestoreInlineHook(&fill_hook)) return FALSE;
    /* The destructor slot stays until no armour layer can exist. */
    if (SudekiMpArmourMenuLayer) { SetLastError(ERROR_BUSY); return FALSE; }
    if (dtor_hook.installed && !SudekiMpRestorePointerHook(&dtor_hook)) return FALSE;
    base = NULL;
    return TRUE;
}

BOOL SudekiMpArmourMenuInstall(HMODULE image, const wchar_t *config_path) {
    uint8_t *b = (uint8_t *)image;
    if (base || !b || !config_path || !SudekiMpCheckLoadedExecutable(image)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    if (!config_enabled(config_path, L"ArmourChoice", TRUE)) { SetLastError(ERROR_SUCCESS); return TRUE; }
    debug_labels = config_enabled(config_path, L"ArmourChoiceLabelProbe", FALSE);
    for (unsigned i = 0; i < sizeof(helpers) / sizeof(helpers[0]); ++i) {
        if (!readable(b + helpers[i].rva, 6) || memcmp(b + helpers[i].rva, helpers[i].bytes, 6)) {
            SudekiMpLogFormat("armour_menu event=refused reason=helper rva=0x%lx\r\n", (unsigned long)helpers[i].rva);
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }
    }
    if (*(void **)(b + MENU_CASE2_SLOT) != b + MENU_CASE2 || *(void **)(b + WEAPONS_DTOR_SLOT) != b + WEAPONS_DTOR ||
        memcmp(b + LIST_FILL, fill_bytes, sizeof(fill_bytes)) || memcmp(b + EQUIP, equip_bytes, sizeof(equip_bytes)) ||
        memcmp(b + DETAILS, details_bytes, sizeof(details_bytes))) {
        SudekiMpLogFormat("armour_menu event=refused reason=unexpected_seam\r\n");
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    SudekiMpArmourMenuNew = b + GAME_NEW;
    SudekiMpArmourMenuCtor = b + WEAPONS_CTOR;
    SudekiMpArmourMenuContinue = b + MENU_CONTINUE;
    SudekiMpArmourMenuFail = b + MENU_FAIL;
    SudekiMpArmourMenuDtor = b + WEAPONS_DTOR;
    SudekiMpArmourMenuRefCopy = b + REF_COPY;
    SudekiMpArmourMenuPreviewSetup = b + PREVIEW_SETUP;
    base = b;
    if (!SudekiMpInstallInlineHook(&fill_hook, b + LIST_FILL, fill_bytes, sizeof(fill_bytes), (const void *)(uintptr_t)fill_stub)) goto fail;
    SudekiMpArmourMenuFillTrampoline = fill_hook.trampoline;
    if (!SudekiMpInstallInlineHook(&equip_hook, b + EQUIP, equip_bytes, sizeof(equip_bytes), (const void *)(uintptr_t)equip_stub)) goto fail;
    SudekiMpArmourMenuEquipTrampoline = equip_hook.trampoline;
    if (!SudekiMpInstallInlineHook(&details_hook, b + DETAILS, details_bytes, sizeof(details_bytes), (const void *)(uintptr_t)details_stub)) goto fail;
    SudekiMpArmourMenuDetailsTrampoline = details_hook.trampoline;
    if (!SudekiMpInstallPointerHook(&dtor_hook, (void **)(b + WEAPONS_DTOR_SLOT), b + WEAPONS_DTOR, (const void *)(uintptr_t)dtor_stub)) goto fail;
    if (!SudekiMpInstallPointerHook(&case_hook, (void **)(b + MENU_CASE2_SLOT), b + MENU_CASE2, (const void *)(uintptr_t)case_stub)) goto fail;
    SudekiMpLogFormat("armour_menu event=installed seams=menu_case2:0x93cfc,weapons_dtor:0x2d8908,fill:0x16fc60,equip:0x1706d0,details:0x16f5b0 label_probe=%d\r\n", debug_labels);
    return TRUE;
fail:
    {
        DWORD error = GetLastError();
        if (case_hook.installed) (void)SudekiMpRestorePointerHook(&case_hook);
        if (dtor_hook.installed) (void)SudekiMpRestorePointerHook(&dtor_hook);
        if (details_hook.installed) (void)SudekiMpRestoreInlineHook(&details_hook);
        if (equip_hook.installed) (void)SudekiMpRestoreInlineHook(&equip_hook);
        if (fill_hook.installed) (void)SudekiMpRestoreInlineHook(&fill_hook);
        base = NULL;
        SetLastError(error);
        return FALSE;
    }
}
