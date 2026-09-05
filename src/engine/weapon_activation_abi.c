#include "engine/weapon_activation_abi.h"

#include <stddef.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Weapon activation ABI requires 32-bit GCC assembly support"
#endif

enum {
    RVA_SET_WEAPON = 0x000d8790u,
    RVA_INVENTORY_LOOKUP = 0x00021ce0u,
    RVA_INVENTORY_COUNT = 0x00021e80u,
    RVA_INVENTORY_GLOBAL = 0x00408d84u,
    CHARACTER_WEAPON_OFFSET = 0xc0u,
    WEAPON_CURRENT_ITEM_OFFSET = 0x268u,
    /* CCharacterWeapon::SetWeapon's argument is an index in this single
     * global weapon inventory category. It is not an actor resource type or
     * a SOL item id. FillInventory orders the category Ailish, Elco, Tal,
     * then Buki; the native SetWeapon implementation performs the lookup. */
    INVENTORY_WEAPON_CATEGORY = 5u
};

static const uint8_t expected_set_weapon_entry[] = {
    0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x83,
    0xf8, 0xff, 0x75, 0x0c
};
static const uint8_t expected_inventory_lookup_entry[] = {
    /* The following absolute global operand is PE-relocated in a live
     * process; pin only the non-relocated function prologue. */
    0x53, 0x8b, 0x5c, 0x24, 0x08, 0x55, 0x56, 0x57,
    0x8b, 0x3d
};
static const uint8_t expected_inventory_count_entry[] = {
    0x8b, 0x91, 0x2c, 0x01, 0x00, 0x00, 0x53, 0x33,
    0xc0, 0x56
};
typedef void (__attribute__((thiscall)) *SetWeaponFunction)(
    void *weapon, int slot
);
typedef void *(__attribute__((thiscall)) *InventoryLookupFunction)(
    void *inventory, int category, int slot
);
static HMODULE native_module;
static SetWeaponFunction native_set_weapon;
static InventoryLookupFunction native_inventory_lookup;
static void *native_inventory_count __attribute__((used));

static BOOL readable_memory(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    return end >= start && end <= region_end;
}

__attribute__((naked, noinline))
static int call_inventory_category_count(void *inventory, int category) {
    (void)inventory;
    (void)category;
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %edi\n\t"
        "movl 8(%ebp), %ecx\n\t"
        "movl 12(%ebp), %edi\n\t"
        "call *_native_inventory_count\n\t"
        "popl %edi\n\t"
        "popl %ebp\n\t"
        "ret\n\t"
    );
}

static SudekiMpWeaponActivationResult empty_result(
    SudekiMpWeaponActivationStatus status
) {
    SudekiMpWeaponActivationResult result;
    ZeroMemory(&result, sizeof(result));
    result.status = status;
    return result;
}

static BOOL character_weapon_context(
    void *character,
    void **weapon,
    void **inventory,
    unsigned int *category
) {
    if (native_module == NULL || character == NULL || weapon == NULL ||
        inventory == NULL || category == NULL ||
        !readable_memory((uint8_t *)character + CHARACTER_WEAPON_OFFSET,
            sizeof(void *))) {
        return FALSE;
    }
    *weapon = *(void **)((uint8_t *)character + CHARACTER_WEAPON_OFFSET);
    *inventory = *(void **)((uint8_t *)native_module + RVA_INVENTORY_GLOBAL);
    if (*weapon == NULL || *inventory == NULL ||
        !readable_memory(*weapon, WEAPON_CURRENT_ITEM_OFFSET + sizeof(void *)) ||
        !readable_memory(*inventory, 0x130u) ||
        *(void **)((uint8_t *)*weapon + 0x10u) != character) {
        return FALSE;
    }
    *category = INVENTORY_WEAPON_CATEGORY;
    return TRUE;
}

BOOL SudekiMpInitializeWeaponActivationAbi(HMODULE game_module) {
    uint8_t *base = (uint8_t *)game_module;

    if (native_module != NULL || base == NULL) {
        SetLastError(ERROR_ALREADY_INITIALIZED);
        return FALSE;
    }
    if (memcmp(base + RVA_SET_WEAPON, expected_set_weapon_entry,
            sizeof(expected_set_weapon_entry)) != 0) {
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return FALSE;
    }
    if (memcmp(base + RVA_INVENTORY_LOOKUP, expected_inventory_lookup_entry,
            sizeof(expected_inventory_lookup_entry)) != 0) {
        SetLastError(ERROR_BAD_LENGTH);
        return FALSE;
    }
    if (memcmp(base + RVA_INVENTORY_COUNT, expected_inventory_count_entry,
            sizeof(expected_inventory_count_entry)) != 0) {
        SetLastError(ERROR_BAD_FORMAT);
        return FALSE;
    }
    if (!readable_memory(base + RVA_INVENTORY_GLOBAL, sizeof(void *))) {
        SetLastError(ERROR_NOACCESS);
        return FALSE;
    }
    native_module = game_module;
    native_set_weapon = (SetWeaponFunction)(base + RVA_SET_WEAPON);
    native_inventory_lookup = (InventoryLookupFunction)(
        base + RVA_INVENTORY_LOOKUP);
    native_inventory_count = base + RVA_INVENTORY_COUNT;
    return TRUE;
}

void SudekiMpResetWeaponActivationAbi(void) {
    native_module = NULL;
    native_set_weapon = NULL;
    native_inventory_lookup = NULL;
    native_inventory_count = NULL;
}

BOOL SudekiMpDescribeCharacterWeapons(void *character,
    SudekiMpWeaponQuickList *weapons) {
    void *weapon;
    void *inventory;
    unsigned int category;
    int count;
    unsigned int slot;

    if (weapons == NULL || !character_weapon_context(character, &weapon,
            &inventory, &category)) {
        return FALSE;
    }
    count = call_inventory_category_count(inventory, (int)category);
    if (count < 0 || (unsigned int)count >
        SUDEKIMP_WEAPON_ACTIVATION_MAX_ROWS) {
        return FALSE;
    }
    ZeroMemory(weapons, sizeof(*weapons));
    weapons->inventory_category = category;
    for (slot = 0u; slot < (unsigned int)count; ++slot) {
        void *item = native_inventory_lookup(inventory, (int)category,
            (int)slot);
        if (item == NULL || !readable_memory(item, sizeof(void *))) {
            ZeroMemory(weapons, sizeof(*weapons));
            return FALSE;
        }
        weapons->rows[slot].slot = slot;
        weapons->rows[slot].native_item = item;
        weapons->rows[slot].equipped =
            *(void **)((uint8_t *)weapon + WEAPON_CURRENT_ITEM_OFFSET) == item;
    }
    weapons->row_count = (unsigned int)count;
    return TRUE;
}

SudekiMpWeaponActivationResult SudekiMpActivateCharacterWeapon(
    void *character,
    unsigned int slot
) {
    SudekiMpWeaponActivationResult result = empty_result(
        SUDEKIMP_WEAPON_ACTIVATION_INVALID_CONTEXT);
    void *weapon;
    void *inventory;
    unsigned int category;
    int count;

    if (!character_weapon_context(character, &weapon, &inventory, &category)) {
        return result;
    }
    count = call_inventory_category_count(inventory, (int)category);
    if (count < 0 || slot >= (unsigned int)count) {
        result.status = SUDEKIMP_WEAPON_ACTIVATION_INVALID_SELECTION;
        return result;
    }
    result.slot = slot;
    result.expected_item = native_inventory_lookup(inventory, (int)category,
        (int)slot);
    if (result.expected_item == NULL ||
        !readable_memory(result.expected_item, sizeof(void *))) {
        result.status = SUDEKIMP_WEAPON_ACTIVATION_NOT_AVAILABLE;
        return result;
    }
    native_set_weapon(weapon, (int)slot);
    result.observed_item = *(void **)((uint8_t *)weapon +
        WEAPON_CURRENT_ITEM_OFFSET);
    result.status = result.observed_item == result.expected_item ?
        SUDEKIMP_WEAPON_ACTIVATION_STARTED :
        SUDEKIMP_WEAPON_ACTIVATION_UNVERIFIED;
    return result;
}

const char *SudekiMpWeaponActivationStatusName(
    SudekiMpWeaponActivationStatus status
) {
    switch (status) {
    case SUDEKIMP_WEAPON_ACTIVATION_STARTED: return "started";
    case SUDEKIMP_WEAPON_ACTIVATION_INVALID_CONTEXT: return "invalid_context";
    case SUDEKIMP_WEAPON_ACTIVATION_INVALID_SELECTION: return "invalid_selection";
    case SUDEKIMP_WEAPON_ACTIVATION_NOT_AVAILABLE: return "not_available";
    case SUDEKIMP_WEAPON_ACTIVATION_UNVERIFIED: return "unverified";
    default: return "unknown";
    }
}
