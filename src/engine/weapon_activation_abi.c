#include "engine/weapon_activation_abi.h"

#include <stddef.h>
#include <math.h>
#include <string.h>

#include "engine/log.h"

#if !defined(__GNUC__) || !defined(__i386__)
#error "Weapon activation ABI requires 32-bit GCC assembly support"
#endif

enum {
    RVA_SET_WEAPON = 0x000d8790u,
    RVA_SET_WEAPON_ITEM = 0x000d7c10u,
    RVA_INVENTORY_LOOKUP = 0x00021ce0u,
    RVA_INVENTORY_COUNT = 0x00021e80u,
    RVA_INVENTORY_GLOBAL = 0x00408d84u,
    RVA_ITEM_DATABASE_GLOBAL = 0x00408d80u,
    CHARACTER_WEAPON_OFFSET = 0xc0u,
    WEAPON_CURRENT_ITEM_OFFSET = 0x268u,
    WEAPON_PENDING_ITEM_OFFSET = 0x26cu,
    ITEM_ID_OFFSET = 0x14u,
    SUPPORTED_IMAGE_SIZE = 0x0045f000u
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
static const uint8_t expected_set_weapon_item_entry[] = {
    0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x83, 0xec,
    0x14, 0x53, 0x56, 0x8b, 0x75, 0x08, 0x33, 0xd2
};
typedef void (__attribute__((thiscall)) *SetWeaponFunction)(
    void *weapon, void *item
);
typedef void *(__attribute__((thiscall)) *InventoryLookupFunction)(
    void *inventory, int category, int slot
);
static HMODULE native_module;
static SetWeaponFunction native_set_weapon;
static InventoryLookupFunction native_inventory_lookup;
static void *native_inventory_count __attribute__((used));

BOOL SudekiMpWeaponFamily(unsigned int resource_type,
    unsigned int *category, unsigned int *starter_item_id) {
    unsigned int c, id;
    if (category == NULL || starter_item_id == NULL) return FALSE;
    switch (resource_type) {
    case 0x23u: c = 4u; id = 0u; break;  /* Tal */
    case 0x01u: c = 5u; id = 12u; break; /* Ailish */
    case 0x05u: c = 6u; id = 36u; break; /* Buki */
    case 0x0eu: c = 7u; id = 24u; break; /* Elco */
    default: return FALSE;
    }
    *category = c;
    *starter_item_id = id;
    return TRUE;
}

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
    void **type_table;
    uint8_t *type_method;
    unsigned int resource_type, starter;
    size_t hero;
    static const uint32_t identities[][4] = {
        {0x2d5010u, 0x2d5054u, 0x139ad0u, 0x23u},
        {0x2d555cu, 0x2d55a0u, 0x1e8240u, 0x01u},
        {0x2d5a88u, 0x2d5accu, 0x22c0e0u, 0x05u},
        {0x2d66fcu, 0x2d6740u, 0x14d730u, 0x0eu}
    };
    if (native_module == NULL || character == NULL || weapon == NULL ||
        inventory == NULL || category == NULL ||
        !readable_memory((uint8_t *)character + CHARACTER_WEAPON_OFFSET,
            sizeof(void *))) {
        return FALSE;
    }
    *weapon = *(void **)((uint8_t *)character + CHARACTER_WEAPON_OFFSET);
    *inventory = *(void **)((uint8_t *)native_module + RVA_INVENTORY_GLOBAL);
    if (*weapon == NULL || *inventory == NULL ||
        !readable_memory(*weapon, WEAPON_PENDING_ITEM_OFFSET + sizeof(void *)) ||
        !readable_memory(*inventory, 0x130u) ||
        *(void **)((uint8_t *)*weapon + 0x10u) != character) {
        return FALSE;
    }
    if (!readable_memory((uint8_t *)character + 0x2cu, sizeof(void *)))
        return FALSE;
    type_table = *(void ***)((uint8_t *)character + 0x2cu);
    if (!readable_memory(type_table, 5u * sizeof(void *)) ||
        (uintptr_t)type_table < (uintptr_t)native_module ||
        (uintptr_t)type_table + 20u > (uintptr_t)native_module + SUPPORTED_IMAGE_SIZE)
        return FALSE;
    type_method = (uint8_t *)type_table[4];
    for (hero = 0u; hero < 4u; ++hero) {
        if (*(void **)character == (uint8_t *)native_module + identities[hero][0] &&
            (void *)type_table == (uint8_t *)native_module + identities[hero][1] &&
            type_method == (uint8_t *)native_module + identities[hero][2]) break;
    }
    if (hero == 4u) return FALSE;
    /* The four retail resource-type methods are exact, side-effect-free
     * mov eax, imm32; ret leaves. Do not execute an arbitrary virtual call. */
    if ((uintptr_t)type_method < (uintptr_t)native_module + 0x1000u ||
        (uintptr_t)type_method + 6u > (uintptr_t)native_module + 0x299600u ||
        !readable_memory(type_method, 6u) || type_method[0] != 0xb8u ||
        type_method[5] != 0xc3u) return FALSE;
    memcpy(&resource_type, type_method + 1u, sizeof(resource_type));
    if (resource_type != identities[hero][3]) return FALSE;
    return SudekiMpWeaponFamily(resource_type, category, &starter);
}

static BOOL item_matches_family(void *item, unsigned int category) {
    unsigned int first, id;
    void *database;
    switch (category) {
    case 4u: first = 0u; break;
    case 5u: first = 12u; break;
    case 6u: first = 36u; break;
    case 7u: first = 24u; break;
    default: return FALSE;
    }
    if (!readable_memory(item, ITEM_ID_OFFSET + 4u)) return FALSE;
    id = *(uint32_t *)((uint8_t *)item + ITEM_ID_OFFSET);
    if (id < first || id >= first + 12u) return FALSE;
    database = *(void **)((uint8_t *)native_module + RVA_ITEM_DATABASE_GLOBAL);
    return readable_memory(database, 12u + (id + 1u) * 4u) &&
        *(void **)((uint8_t *)database + 12u + id * 4u) == item;
}

static float positive_half(uint16_t half) {
    unsigned int exponent = (half >> 10u) & 31u;
    if ((half & 0x8000u) || exponent == 31u) return NAN;
    return exponent == 0u ? ldexpf((float)(half & 1023u), -24) :
        ldexpf(1.0f + (float)(half & 1023u) / 1024.0f, (int)exponent - 15);
}

float SudekiMpRapidWeaponRechargeAmount(uint16_t rate_half, uint16_t charge_half,
    uint8_t flags, float cooldown, float delta) {
    float rate = positive_half(rate_half), charge = positive_half(charge_half);
    if (!isfinite(rate) || rate > 1000.0f || !isfinite(charge) || charge >= 100.0f ||
        !(flags & 3u) || !isfinite(cooldown) || cooldown != 0.0f ||
        !isfinite(delta) || delta <= 0.0f || delta > 0.25f) return 0.0f;
    return rate * delta;
}

uint32_t SudekiMpRapidWeaponCycleMs(unsigned int item_id, float frames,
    float delta) {
    /* Proton Phaser's exact native FP resource is five frames. The original
     * Ailish research supplied the dispatch/terminal-boundary model, but her
     * faster action journal is not accepted in this checkout: do not change
     * her existing cadence. B8 is reload duration, never this shot interval. */
    if (item_id != 24u || frames != 5.0f ||
        !isfinite(delta) || delta <= 0.0f || delta > 0.25f) return 0u;
    return (uint32_t)((0.1f + frames / 24.0f + 2.0f * delta) * 1000.0f + 0.5f);
}

/* Retail 4c69a3: ESI=CMissileManager, float stack argument, callee ret 4. */
__attribute__((naked, noinline))
static void call_native_recharge(void *target __attribute__((unused)),
    void *combat __attribute__((unused)), float amount __attribute__((unused))) {
    __asm__ volatile(
        "pushl %ebp\n\tmovl %esp, %ebp\n\tpushl %esi\n\t"
        "movl 12(%ebp), %esi\n\tpushl 16(%ebp)\n\tcall *8(%ebp)\n\t"
        "popl %esi\n\tpopl %ebp\n\tret\n\t");
}

BOOL SudekiMpServiceRemoteRapidWeapon(void *character, void *local_character,
    float delta, uint32_t *repeat_ms) {
    static const uint8_t recharge_entry[] = {
        0x51,0x8b,0x56,0x60,0x85,0xd2,0x0f,0x84,0x9f,0,0,0,
        0x0f,0xb7,0x82,0xba,0,0,0
    };
    uint8_t *base = (uint8_t *)native_module, *actor = character;
    uint8_t *combat, *record, *component, *arbiter, *controller, *item;
    uint8_t *wrapper, *renderer, *bank, *entries, *resource;
    uint8_t **rows;
    void *weapon, *inventory, *local_weapon, *local_inventory;
    unsigned int category, local_category, count, i, id;
    uint32_t interval;
    float amount, frames;
    if (repeat_ms == NULL) return FALSE;
    *repeat_ms = 0u;
    if (!base || actor == local_character || !readable_memory(actor, 0x138u) ||
        !character_weapon_context(actor, &weapon, &inventory, &category) ||
        !character_weapon_context(local_character, &local_weapon, &local_inventory,
            &local_category) || category != 7u) return FALSE;
    controller = *(uint8_t **)(base + 0x408da4u);
    if (!readable_memory(controller, 0x24cu) ||
        *(void **)(controller + 0x248u) != local_character) return FALSE;
    item = *(uint8_t **)((uint8_t *)weapon + WEAPON_CURRENT_ITEM_OFFSET);
    if (!item_matches_family(item, category) ||
        *(void **)((uint8_t *)weapon + WEAPON_PENDING_ITEM_OFFSET) != NULL) return FALSE;
    id = *(uint32_t *)(item + ITEM_ID_OFFSET);
    if (id != 24u) return FALSE;
    combat = *(uint8_t **)(actor + 0xbcu);
    component = *(uint8_t **)(actor + 0x134u);
    arbiter = *(uint8_t **)(actor + 0x90u);
    if (!readable_memory(combat, 0xe4u) || *(void **)combat != base + 0x2d4c8cu ||
        *(void **)(combat + 0x10u) != actor ||
        !readable_memory(component, 0x168u) ||
        *(void **)component != base + 0x2d5464u ||
        *(void **)(component + 0x10u) != actor ||
        *(float *)(component + 0x50u) != 1.0f ||
        *(float *)(component + 0x54u) != 1.0f ||
        !readable_memory(arbiter, 0x64u) || *(void **)(arbiter + 0x10u) != actor ||
        (arbiter[0x58u] & 0xf0u) != 0x20u) return FALSE;
    record = *(uint8_t **)(combat + 0x60u);
    rows = *(uint8_t ***)(combat + 0x4cu);
    count = *(unsigned int *)(combat + 0x44u);
    if (!count || count > 64u || !readable_memory(rows, count * sizeof(void *)) ||
        !readable_memory(record, 0xc4u) || *(uint32_t *)(record + 8u) != id ||
        *(uint32_t *)(record + 0x9cu) != 140u) return FALSE;
    for (i = 0u; i < count && rows[i] != record; ++i) {}
    if (i == count) return FALSE;
    wrapper = *(uint8_t **)(component + 0x160u);
    if (!readable_memory(wrapper, 0x14u)) return FALSE;
    renderer = *(uint8_t **)(wrapper + 0x10u);
    if (!readable_memory(renderer, 0x0cu) ||
        *(void **)renderer != base + 0x2df8ecu) return FALSE;
    bank = *(uint8_t **)(renderer + 8u);
    if (!readable_memory(bank, 0x24u)) return FALSE;
    entries = *(uint8_t **)(bank + 0x20u);
    if (!readable_memory(entries, 3u * 28u)) return FALSE;
    resource = *(uint8_t **)(entries + 2u * 28u);
    if (!readable_memory(resource, 8u) ||
        *(uint32_t *)(base + 0x339be0u) != 0x3dcccccdu ||
        *(double *)(base + 0x2e35d0u) != 24.0) return FALSE;
    frames = *(float *)(resource + 4u);
    interval = SudekiMpRapidWeaponCycleMs(id, frames, delta);
    if (!interval) return FALSE;
    /* Unknown/active speed effects retain the previous conservative path;
     * never apply normal-speed cadence or a second recharge to them. */
    {
        uint8_t *status = *(uint8_t **)(actor + 0xa8u);
        if (status) {
            if (!readable_memory(status, 0x54u) ||
                *(void **)status != base + 0x2d4abcu ||
                *(void **)(status + 0x10u) != actor) return FALSE;
            for (i = 0u; i < 2u; ++i) {
                uint8_t *effect = *(uint8_t **)(status + (i ? 0x50u : 0x44u));
                if (!readable_memory(effect, 0x50u) ||
                    *(void **)effect != base + (i ? 0x2cbf54u : 0x2cbf18u) ||
                    effect[0x4cu] != 0u) return FALSE;
            }
        }
    }
    if (memcmp(base + 0xc8b90u, recharge_entry, sizeof(recharge_entry)) != 0 ||
        memcmp(base + 0xc8c3bu, "\x59\xc2\x04\x00", 4u) != 0) return FALSE;
    amount = SudekiMpRapidWeaponRechargeAmount(*(uint16_t *)(record + 0xb6u),
        *(uint16_t *)(record + 0xbau), record[0xbcu],
        *(float *)(record + 0xc0u), delta);
    /* No calls or yielding between validation and the sole native mutation. */
    if (*(void **)(controller + 0x248u) != local_character ||
        *(void **)(actor + 0xbcu) != combat ||
        *(void **)(combat + 0x60u) != record ||
        *(void **)((uint8_t *)weapon + WEAPON_CURRENT_ITEM_OFFSET) != item) return FALSE;
    if (amount > 0.0f) call_native_recharge(base + 0xc8b90u, combat, amount);
    *repeat_ms = interval;
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
    if (memcmp(base + RVA_SET_WEAPON_ITEM, expected_set_weapon_item_entry,
            sizeof(expected_set_weapon_item_entry)) != 0 ||
        base[RVA_SET_WEAPON + 0x29u] != 0xe8u) {
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return FALSE;
    }
    {
        int32_t displacement;
        memcpy(&displacement, base + RVA_SET_WEAPON + 0x2au, 4u);
        if ((int64_t)RVA_SET_WEAPON + 0x2eu + displacement != RVA_SET_WEAPON_ITEM) {
            SetLastError(ERROR_BAD_EXE_FORMAT);
            return FALSE;
        }
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
    native_set_weapon = (SetWeaponFunction)(base + RVA_SET_WEAPON_ITEM);
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

BOOL SudekiMpWeaponActivationAbiInitialized(void) {
    return native_module != NULL;
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
        if (!item_matches_family(item, category)) {
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

BOOL SudekiMpEnsureCharacterStarterWeapon(void *character) {
    SudekiMpWeaponQuickList list;
    unsigned int slot, starter;
    if (!SudekiMpDescribeCharacterWeapons(character, &list)) return FALSE;
    for (slot = 0u; slot < list.row_count; ++slot)
        if (list.rows[slot].equipped) return TRUE;
    switch (list.inventory_category) {
    case 4u: starter = 0u; break;
    case 5u: starter = 12u; break;
    case 6u: starter = 36u; break;
    case 7u: starter = 24u; break;
    default: return FALSE;
    }
    for (slot = 0u; slot < list.row_count; ++slot) {
        if (*(uint32_t *)((uint8_t *)list.rows[slot].native_item + ITEM_ID_OFFSET) == starter) {
            SudekiMpWeaponActivationResult result =
                SudekiMpActivateCharacterWeapon(character, list.rows[slot].slot);
            return result.status == SUDEKIMP_WEAPON_ACTIVATION_STARTED;
        }
    }
    return FALSE;
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
    if (count < 0 || count > SUDEKIMP_WEAPON_ACTIVATION_MAX_ROWS ||
        slot >= (unsigned int)count) {
        result.status = SUDEKIMP_WEAPON_ACTIVATION_INVALID_SELECTION;
        return result;
    }
    result.slot = slot;
    result.expected_item = native_inventory_lookup(inventory, (int)category,
        (int)slot);
    if (!item_matches_family(result.expected_item, category)) {
        result.status = SUDEKIMP_WEAPON_ACTIVATION_NOT_AVAILABLE;
        return result;
    }
    result.observed_item = *(void **)((uint8_t *)weapon + WEAPON_CURRENT_ITEM_OFFSET);
    if (result.observed_item == result.expected_item) {
        result.status = SUDEKIMP_WEAPON_ACTIVATION_STARTED;
        return result;
    }
    /* Native armed swaps can defer their model handoff. Never replace or
     * repeatedly restart a pending native transaction. */
    if (*(void **)((uint8_t *)weapon + WEAPON_PENDING_ITEM_OFFSET) != NULL) {
        result.status = SUDEKIMP_WEAPON_ACTIVATION_UNVERIFIED;
        return result;
    }
    {
        void *fresh_weapon, *fresh_inventory;
        unsigned int fresh_category;
        if (!character_weapon_context(character, &fresh_weapon, &fresh_inventory,
                &fresh_category) || fresh_weapon != weapon ||
            fresh_inventory != inventory || fresh_category != category ||
            native_inventory_lookup(inventory, (int)category, (int)slot) != result.expected_item ||
            !item_matches_family(result.expected_item, category)) return result;
    }
    native_set_weapon(weapon, result.expected_item);
    result.observed_item = *(void **)((uint8_t *)weapon +
        WEAPON_CURRENT_ITEM_OFFSET);
    result.status = result.observed_item == result.expected_item ?
        SUDEKIMP_WEAPON_ACTIVATION_STARTED :
        SUDEKIMP_WEAPON_ACTIVATION_UNVERIFIED;
    SudekiMpLogFormat("weapon_activation event=equip category=%u slot=%u item_id=%lu status=%s\r\n",
        category, slot,
        (unsigned long)*(uint32_t *)((uint8_t *)result.expected_item + ITEM_ID_OFFSET),
        SudekiMpWeaponActivationStatusName(result.status));
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
