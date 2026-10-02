#include "engine/weapon_activation_abi.h"

#include <stddef.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

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

static BOOL elco_item(unsigned id) {
    return id == 24u || id == 26u || id == 27u || id == 30u ||
        id == 31u || id == 34u || id == 35u;
}

static uint8_t *ranged_weapon_record(void *character,uint8_t type,
    SudekiMpElcoWeaponObservation *out) {
    uint8_t *base = (uint8_t *)native_module, *actor = character;
    uint8_t *manager, *weapon, *item, *record, **rows;
    unsigned count, i, id;
    SudekiMpElcoWeaponObservation s;
    if (!base || !out || (type!=0x0eu && type!=0x01u) ||
        !readable_memory(actor, 0x138u) ||
        *(void **)actor != base + (type==0x0eu?0x2d66fcu:0x2d555cu)) return NULL;
    weapon = *(uint8_t **)(actor + 0xc0u);
    manager = *(uint8_t **)(actor + 0xbcu);
    if (!readable_memory(weapon, 0x270u) ||
        *(void **)weapon != base + 0x2d4d3cu ||
        *(void **)(weapon + 0x10u) != actor ||
        *(void **)(weapon + 0x26cu) != NULL ||
        !readable_memory(manager, 0xe4u) ||
        *(void **)manager != base + 0x2d4c8cu ||
        *(void **)(manager + 0x10u) != actor) return NULL;
    item = *(uint8_t **)(weapon + 0x268u);
    if (!item_matches_family(item, type==0x0eu?7u:5u)) return NULL;
    id = *(uint32_t *)(item + 0x14u);
    if (type==0x0eu ? !elco_item(id) : (id<12u || id>=24u)) return NULL;
    record = *(uint8_t **)(manager + 0x60u);
    count = *(unsigned *)(manager + 0x44u);
    rows = *(uint8_t ***)(manager + 0x4cu);
    if (!count || count > 64u || !readable_memory(rows, count * 4u) ||
        !readable_memory(record, 0xc4u) || *(uint32_t *)(record + 8u) != id)
        return NULL;
    for (i = 0; i < count && rows[i] != record; ++i) {}
    if (i == count) return NULL;
    s.item = (uint8_t)id; s.stage = manager[0xe0u];
    s.charge = positive_half(*(uint16_t *)(record + 0xbau));
    s.required_charge = positive_half(*(uint16_t *)(record + 0xb4u));
    s.reload_seconds = *(float *)(record + 0xc0u);
    if (s.stage > 6u || !isfinite(s.charge) || s.charge > 100.0f ||
        !isfinite(s.required_charge) || s.required_charge <= 0.0f ||
        s.required_charge > 100.0f || !isfinite(s.reload_seconds) ||
        s.reload_seconds < 0.0f || s.reload_seconds > 60.0f ||
        *(void **)(actor + 0xbcu) != manager ||
        *(void **)(manager + 0x60u) != record ||
        *(void **)(weapon + 0x268u) != item) return NULL;
    *out = s;
    return record;
}

BOOL SudekiMpObserveElcoWeapon(void *character, SudekiMpElcoWeaponObservation *out) {
    return ranged_weapon_record(character,0x0eu,out) != NULL;
}

BOOL SudekiMpObserveElcoWeaponEmission(void *character,
    SudekiMpElcoWeaponObservation *out) {
    SudekiMpElcoWeaponObservation observed;
    uint8_t *record, *manager;
    if (!out || !(record=ranged_weapon_record(character,0x0eu,&observed)))
        return FALSE;
    manager=*(uint8_t **)((uint8_t *)character+0xbcu);
    /* Retail c6de0 admits a record as stage 1, with +5c=record and
     * +58=record+0c. Direct fire c89f0 emits in stage 1; animation-driven
     * fire c7140 emits in stage 2. c74e3 obtains direction before c7754
     * spends charge. The ready-to-start stages (0/6) cannot witness this. */
    if ((observed.stage!=1u && observed.stage!=2u) ||
        *(void **)(manager+0x5cu)!=record ||
        *(void **)(manager+0x58u)!=record+0x0cu ||
        observed.charge<observed.required_charge || observed.reload_seconds!=0.0f)
        return FALSE;
    *out=observed;
    return TRUE;
}

BOOL SudekiMpObserveRangedWeapon(void *character,uint8_t type,
    SudekiMpElcoWeaponObservation *out) {
    return ranged_weapon_record(character,type,out)!=NULL;
}

BOOL SudekiMpElcoWeaponReady(const SudekiMpElcoWeaponObservation *s) {
    return s && elco_item(s->item) && (s->stage == 0u || s->stage == 6u) &&
        isfinite(s->charge) && s->charge >= 0.0f && s->charge <= 100.0f &&
        isfinite(s->required_charge) && s->required_charge > 0.0f &&
        s->required_charge <= 100.0f && s->charge >= s->required_charge &&
        isfinite(s->reload_seconds) && s->reload_seconds == 0.0f;
}

static BOOL set_ranged_presentation_resources(void *character,uint8_t type,void *local_actor,uint8_t item,
    uint16_t charge_q8, uint16_t reload_ms, BOOL preserve_reload_edge) {
    SudekiMpElcoWeaponObservation observed;
    uint8_t *record, *controller;
    MEMORY_BASIC_INFORMATION page;
    float charge;
    uint32_t bits, exponent;
    uint16_t half;
    if (charge_q8 > 25600u || reload_ms > 60000u || !native_module) return FALSE;
    controller = *(uint8_t **)((uint8_t *)native_module + 0x408da4u);
    if (!readable_memory(controller, 0x24cu) ||
        !local_actor || *(void **)(controller + 0x248u) != local_actor) return FALSE;
    record = ranged_weapon_record(character,type,&observed);
    if (!record || observed.item != item ||
        !VirtualQuery(record, &page, sizeof(page)) ||
        !(page.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)))
        return FALSE;
    charge = charge_q8 / 256.0f;
    memcpy(&bits, &charge, 4u);
    exponent = (bits >> 23u) & 255u;
    /* Q8 is either zero or >=1/256: every nonzero value is a normal half.
     * Retail conversion truncates the mantissa in the same way. */
    half = charge_q8 ? (uint16_t)(((exponent - 112u) << 10u) |
        ((bits >> 13u) & 1023u)) : 0u;
    *(uint16_t *)(record + 0xbau) = half;
    /* Retail CMissileManager::Update (RVA c69fb..c6a9d) queues C3 and
     * performs refill cleanup only when a POSITIVE timer crosses zero during
     * its own update. Writing a received zero directly loses that event and
     * leaves the FP C2 animation/lease busy indefinitely. Preserve at most
     * one millisecond for the next native update, including repeated zero
     * snapshots. Do not manufacture an edge for an already-zero timer, or
     * invoke/cancel native tasks from this resource writer. */
    *(float *)(record + 0xc0u) = preserve_reload_edge && !reload_ms &&
        observed.reload_seconds > 0.0f ?
        fminf(observed.reload_seconds, 1.0f / 1024.0f) : reload_ms / 1000.0f;
    return TRUE;
}

BOOL SudekiMpSetElcoPresentationResources(void *character, uint8_t item,
    uint16_t charge_q8, uint16_t reload_ms) {
    return set_ranged_presentation_resources(character,0x0eu,character,item,charge_q8,reload_ms,FALSE);
}

BOOL SudekiMpSyncElcoPresentationResources(void *character, uint8_t item,
    uint16_t charge_q8, uint16_t reload_ms) {
    return set_ranged_presentation_resources(character,0x0eu,character,item,charge_q8,reload_ms,TRUE);
}
BOOL SudekiMpSetObservedElcoPresentationResources(void *character,void *local_actor,
    uint8_t item,uint16_t charge_q8,uint16_t reload_ms,BOOL preserve_reload_edge) {
    void *weapon,*inventory;
    unsigned category;
    if(!local_actor || local_actor==character ||
        !character_weapon_context(local_actor,&weapon,&inventory,&category)) return FALSE;
    return set_ranged_presentation_resources(character,0x0eu,local_actor,item,charge_q8,
        reload_ms,preserve_reload_edge);
}

BOOL SudekiMpSetRangedPresentationResources(void *character,uint8_t type,
    void *local_actor,uint8_t item,uint16_t charge,uint16_t reload,BOOL preserve) {
    void *weapon,*inventory;
    unsigned category;
    if(!local_actor || !character_weapon_context(local_actor,&weapon,&inventory,&category))
        return FALSE;
    return set_ranged_presentation_resources(character,type,local_actor,item,
        charge,reload,preserve);
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
    /* Items 24 and 27 share the exact five-frame native FP resource. The original
     * Ailish research supplied the dispatch/terminal-boundary model, but her
     * faster action journal is not accepted in this checkout: do not change
     * her existing cadence. B8 is reload duration, never this shot interval. */
    if ((item_id != 24u && item_id != 27u) || frames != 5.0f ||
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
    if (id != 24u && id != 27u) return FALSE;
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

static BOOL visibility_writable(void *pointer,size_t size) {
    MEMORY_BASIC_INFORMATION m;
    return readable_memory(pointer,size) && VirtualQuery(pointer,&m,sizeof(m)) &&
        (m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}

__attribute__((naked, noinline))
static void call_idle_weapon_toggle(void *weapon,void *method) {
    (void)weapon; (void)method;
    __asm__ volatile(
        "pushl %esi\n\t"
        "movl 8(%esp), %esi\n\t"
        "call *12(%esp)\n\t"
        "popl %esi\n\t"
        "ret\n\t");
}

static uint32_t attachment_name_hash(const char *name) {
    uint32_t hash=0;
    while(*name) {
        unsigned char c=(unsigned char)*name++;
        if(c>='A' && c<='Z') c+=(unsigned char)('a'-'A');
        hash=c^(hash*33u);
    }
    return hash;
}

static BOOL attachment_name_exact(uint8_t *weapon,unsigned offset,const char *name) {
    unsigned size=*(uint32_t *)(weapon+offset),length=(unsigned)strlen(name);
    const char *text=(size&0x80000000u)?(char *)weapon+offset+4:
        *(const char **)(weapon+offset+4);
    return (size&0x7fffffffu)==length && readable_memory(text,length+1) &&
        !memcmp(text,name,length+1);
}

/* Bound the exact CModelInstance locator lookup and bind matrices used by
 * 21bce0/21bd40. This is a transition-only check, not a cross-frame cache. */
static BOOL idle_weapon_locators(uint8_t *base,uint8_t *instance,int *hand,int *hip) {
    uint8_t *data,*header,*records,*names_handle,*names;
    unsigned count;
    if(!readable_memory(instance,0x10) || *(void **)instance!=base+0x2df8ec ||
        *(void **)(base+0x2df8ec+0x24)!=base+0x21bd40 ||
        *(void **)(base+0x2df8ec+0x28)!=base+0x21bce0) return FALSE;
    data=*(uint8_t **)(instance+8);
    if(!readable_memory(data,0x54)) return FALSE;
    header=*(uint8_t **)(data+0x1c); names_handle=*(uint8_t **)(data+0x24);
    if(!readable_memory(header,0x18) || !readable_memory(names_handle,4)) return FALSE;
    count=*(unsigned *)(header+0x14); records=*(uint8_t **)(data+0x50);
    names=*(uint8_t **)names_handle;
    if(!count || count>256 || !readable_memory(records,count*0x50)) return FALSE;
    *hand=*hip=-1;
    for(unsigned i=0;i<count;++i) {
        uint8_t *record=records+i*0x50;
        unsigned index=*(unsigned *)record;
        if(index>4095 || !readable_memory(names,(index+1)*8)) return FALSE;
        uint32_t hash=*(uint32_t *)(names+index*8);
        if(hash==attachment_name_hash("WeaponLoc_Rhand")) *hand=(int)i;
        if(hash==attachment_name_hash("WeaponLoc_leg")) *hip=(int)i;
        for(unsigned j=0;j<16;++j)
            if(!isfinite(*(float *)(record+0x10+j*4))) return FALSE;
        if(hash==attachment_name_hash("WeaponLoc_Rhand") ||
            hash==attachment_name_hash("WeaponLoc_leg")) {
            for(unsigned row=0;row<3;++row) {
                const float *v=(const float *)(record+0x10+row*16);
                float norm=v[0]*v[0]+v[1]*v[1]+v[2]*v[2];
                if(!isfinite(norm) || norm<0.000001f) return FALSE;
            }
        }
    }
    return *hand>=0 && *hip>=0 && *hand!=*hip;
}

BOOL SudekiMpRestoreElcoInterruptedIdleWeapon(void *character) {
    static const uint8_t toggle_entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,
        0x8b,0x86,0xac,0x03,0,0};
    uint8_t *base=(uint8_t *)native_module,*actor=character,*weapon,*position,*arbiter;
    uint8_t *model,*wrapper,*instance,*body,*slot,*gun_wrapper,*gun,*attachment,*scene;
    void *record,*inventory; unsigned category; int hand,hip;
    if(!base || !character_weapon_context(actor,&record,&inventory,&category) ||
        category!=7 || !readable_memory(record,0x3b9)) return FALSE;
    weapon=record;
    /* The caller has already proved the session, actor and world-animation
     * lease. Settled actors do not need another full native graph walk. */
    if(!(weapon[0x3b8]&0x20)) return TRUE;
    if(!readable_memory(actor,0x138) || !visibility_writable(weapon,0x3b9) ||
        *(void **)weapon!=base+0x2d4d3c || (weapon[0x3b8]&4) ||
        *(unsigned *)(weapon+0x330)!=3 || *(void **)(weapon+0x26c) ||
        *(void **)(weapon+0x204) ||
        !item_matches_family(*(void **)(weapon+0x268),7) ||
        memcmp(base+0xd8300,toggle_entry,sizeof(toggle_entry)) ||
        memcmp(base+0xd83af,"\xe8\x7c\x02\x00\x00",5) ||
        memcmp(base+0xd83fb,"\xe8\x30\x02\x00\x00",5) ||
        /* Exact native OnAnimationChange pending-idle cleanup edge. */
        memcmp(base+0xd9956,"\xf6\x86\xb8\x03\x00\x00\x20\x74\x05\xe8\x9c\xe9\xff\xff",14) ||
        !attachment_name_exact(weapon,0x270,"WeaponLoc_Rhand") ||
        !attachment_name_exact(weapon,0x2b0,"WeaponLoc_leg")) return FALSE;
    position=*(uint8_t **)(actor+0x44); arbiter=*(uint8_t **)(actor+0x90);
    model=*(uint8_t **)(actor+0x134); slot=weapon+0x40;
    if(!visibility_writable(position,0x104) || *(void **)position!=base+0x2cdefc ||
        *(void **)(position+0x10)!=actor || *(void **)(position+0x94) ||
        !readable_memory(arbiter,0x64) || *(void **)(arbiter+0x10)!=actor ||
        (*(uint32_t *)(arbiter+0x50)&0x400000) || (arbiter[0x60]&2) ||
        !readable_memory(model,0x168) || *(void **)(model+0x10)!=actor ||
        *(void **)(actor+0x130)!=model || *(void **)(weapon+0x3ac)!=model+4 ||
        *(void **)(slot+0x94)!=position+4 || *(void **)slot!=base+0x2cdefc ||
        slot[0x101]!=0 || slot[0x102]!=1) return FALSE;
    wrapper=*(uint8_t **)(position+0xb4); gun_wrapper=*(uint8_t **)(slot+0xb4);
    if(!readable_memory(wrapper,0x14) || !readable_memory(gun_wrapper,0x14) ||
        wrapper==*(void **)(model+0x160) ||
        (*(void **)(model+0x164) && wrapper!=*(void **)(model+0x164))) return FALSE;
    instance=*(uint8_t **)(wrapper+0xc); body=*(uint8_t **)(wrapper+8);
    gun=*(uint8_t **)(gun_wrapper+8); attachment=*(uint8_t **)(slot+0x8c);
    scene=*(uint8_t **)(base+0x408dd4);
    if(*(void **)(wrapper+0x10)!=instance || !idle_weapon_locators(base,instance,&hand,&hip) ||
        *(int *)(slot+0xac)!=hand ||
        !visibility_writable(*(void **)(position+0x8c),0x110) ||
        !visibility_writable(body,0xd0) || *(void **)body!=base+0x2dd700 ||
        *(void **)(body+0x18) ||
        !visibility_writable(gun,0xd0) || *(void **)gun!=base+0x2dd700 ||
        *(void **)(gun+0x18)!=body || !readable_memory(*(void **)(gun+0x38),64) ||
        !visibility_writable(attachment,0x110) || attachment[0xf2]!=1 ||
        !readable_memory(scene,4) || *(void **)scene!=base+0x2c7ae8) return FALSE;
    /* Already-parented weapon + scene-category 1: native cleanup changes the
     * locator/matrices, but cannot reparent or mutate the scene registry. It
     * also clears its own temporary-draw bit, exactly like interruption. */
    call_idle_weapon_toggle(weapon,base+0xd8300);
    return *(void **)(actor+0xc0)==weapon && *(void **)(weapon+0x10)==actor &&
        *(void **)(actor+0x44)==position && *(void **)(position+0xb4)==wrapper &&
        *(void **)(slot+0x94)==position+4 && *(void **)(slot+0xb4)==gun_wrapper &&
        *(int *)(slot+0xac)==hip && !(weapon[0x3b8]&0x20);
}

BOOL SudekiMpInitializeSheathedWeaponVisibility(void *character) {
    static const uint8_t entry[]={0x8a,0x44,0x24,0x04,0x8b,0x91,0x04,0x02,
        0,0,0x02,0xc0,0x32,0x81,0xb8,0x03,0,0};
    uint8_t *base=(uint8_t *)native_module,*actor=character,*weapon,*position,*arbiter;
    uint8_t *wrappers[2]={0},*objects[2]={0}; void *inventory,*record;
    unsigned category,count;
    typedef void (__attribute__((thiscall)) *Show)(void *,int);
    if(!base || !character_weapon_context(character,&record,&inventory,&category)) return FALSE;
    /* Retail Ailish has no sheathe locator and intentionally hides her staff
     * outside combat. Do not make a floating unattached staff visible. */
    if(category==5) return TRUE;
    weapon=record; count=category==6?2:1;
    position=*(uint8_t **)(actor+0x44); arbiter=*(uint8_t **)(actor+0x90);
    if(memcmp(base+0xd7e30,entry,sizeof(entry)) ||
        !visibility_writable(weapon,0x3b9) || !readable_memory(position,0xb8) ||
        *(void **)(position+0x10)!=actor || !readable_memory(arbiter,0x64) ||
        *(void **)(arbiter+0x10)!=actor || (*(uint32_t *)(arbiter+0x50)&0x400000) ||
        (arbiter[0x60]&2) || (weapon[0x3b8]&4) || *(uint32_t *)(weapon+0x330)!=3 ||
        *(void **)(weapon+0x26c) ||
        !item_matches_family(*(void **)(weapon+0x268),category)) return FALSE;
    for(unsigned i=0;i<count;++i) {
        uint8_t *slot=weapon+(i?0x150:0x40);
        wrappers[i]=*(uint8_t **)(slot+0xb4);
        if(!readable_memory(wrappers[i],0x14) ||
            *(void **)(slot+0x94)!=position+4 || *(int *)(slot+0xac)<0) return FALSE;
        objects[i]=*(uint8_t **)(wrappers[i]+8);
        if(!visibility_writable(objects[i],0x38) ||
            *(void **)objects[i]!=base+0x2dd700 ||
            /* SetVisible uses wrapper+8, not CPosition+8c (a distinct
             * attachment object). It never traverses or mutates that link. */
            /* No unverified renderer callback during startup. */
            (*(uint32_t *)(objects[i]+0x34)&0x4000000)) return FALSE;
    }
    if(count==1 && *(void **)(weapon+0x204)) return FALSE;
    ((Show)(base+0xd7e30))(weapon,1);
    if(*(void **)(actor+0xc0)!=weapon || *(void **)(weapon+0x10)!=actor ||
        !(weapon[0x3b8]&2)) return FALSE;
    for(unsigned i=0;i<count;++i) {
        uint8_t *slot=weapon+(i?0x150:0x40);
        if(*(void **)(slot+0xb4)!=wrappers[i] ||
            *(void **)(wrappers[i]+8)!=objects[i] ||
            (*(uint32_t *)(objects[i]+0x34)&4)) return FALSE;
    }
    return TRUE;
}

static BOOL exact_launch_option(const char *command, const char *option) {
    const char *found;
    size_t length = strlen(option);
    if (command == NULL) return FALSE;
    found = strstr(command, option);
    return found != NULL &&
        (found == command || isspace((unsigned char)found[-1])) &&
        (found[length] == '\0' || isspace((unsigned char)found[length]));
}

BOOL SudekiMpGrantTestroomCharacterWeapons(void *character, const char *command) {
    typedef void (__attribute__((thiscall)) *AddItemFunction)(void *, int, int, int);
    static const uint8_t add_entry[] = {0x83,0xec,0x08,0x53,0x55,0x56,0x57,0x8b,0xf1};
    uint8_t *base = (uint8_t *)native_module;
    void *weapon, *inventory, *database, *items[12] = {0};
    SudekiMpWeaponQuickList owned;
    unsigned int category, first, i, j, added = 0u, available = 0u;
    if (!exact_launch_option(command, "-Level testroom") ||
        !exact_launch_option(command, "-DT 1") || !base ||
        !character_weapon_context(character, &weapon, &inventory, &category))
        return FALSE;
    /* Other heroes retain their current kit. This opt-in does not silently
     * broaden old Tal/Ailish regression profiles or grant consumables/money. */
    if (category != 6u && category != 7u) return TRUE;
    if (*(void **)((uint8_t *)weapon + WEAPON_PENDING_ITEM_OFFSET) != NULL ||
        !SudekiMpDescribeCharacterWeapons(character, &owned)) return FALSE;
    first = category == 6u ? 36u : 24u;
    database = *(void **)(base + RVA_ITEM_DATABASE_GLOBAL);
    if (!readable_memory(database, 12u + (first + 12u) * 4u) ||
        /* Exact supported-image export slot for CInventory::AddItem. Checking
         * its RVA works for both loaded and independently mapped test images. */
        *(uint32_t *)(base + 0x30c064u) != 0x217e0u ||
        base[0x217e0u] != 0xa1u ||
        *(void **)(base + 0x217e1u) != base + RVA_ITEM_DATABASE_GLOBAL ||
        memcmp(base + 0x217e5u, add_entry, sizeof(add_entry)) != 0 ||
        memcmp(base + 0x21be80u, "\x8b\x81\x88\x00\x00\x00\xc3", 7u) != 0 ||
        *(void **)(base + 0x2d3a30u) != base + 0x21be80u) return FALSE;
    /* Preflight the whole sparse family before the first native mutation.
     * Item type uses CItem's exact data getter, not an arbitrary virtual call. */
    for (i = 0u; i < 12u; ++i) {
        items[i] = *(void **)((uint8_t *)database + 12u + (first + i) * 4u);
        if (items[i] == NULL) continue;
        if (!item_matches_family(items[i], category) ||
            !readable_memory(items[i], 0x8cu) ||
            *(uint32_t *)((uint8_t *)items[i] + ITEM_ID_OFFSET) != first + i ||
            *(void **)items[i] != base + 0x2d3a28u ||
            *(uint32_t *)((uint8_t *)items[i] + 0x88u) != category) return FALSE;
        ++available;
    }
    if (available == 0u) return FALSE;
    for (i = 0u; i < 12u; ++i) {
        void *fresh_weapon, *fresh_inventory;
        unsigned int fresh_category;
        if (items[i] == NULL) continue;
        for (j = 0u; j < owned.row_count; ++j)
            if (owned.rows[j].native_item == items[i]) break;
        if (j < owned.row_count) continue;
        if (!character_weapon_context(character, &fresh_weapon,
                &fresh_inventory, &fresh_category) || fresh_weapon != weapon ||
            fresh_inventory != inventory || fresh_category != category ||
            *(void **)(base + RVA_ITEM_DATABASE_GLOBAL) != database ||
            *(void **)((uint8_t *)database + 12u + (first + i) * 4u) != items[i] ||
            !item_matches_family(items[i], category) ||
            *(void **)((uint8_t *)weapon + WEAPON_PENDING_ITEM_OFFSET) != NULL)
            return FALSE;
        ((AddItemFunction)(base + 0x217e0u))(inventory, (int)(first + i), 1, 0);
        if (!SudekiMpDescribeCharacterWeapons(character, &owned)) return FALSE;
        for (j = 0u; j < owned.row_count; ++j)
            if (owned.rows[j].native_item == items[i]) break;
        if (j == owned.row_count) return FALSE;
        ++added;
    }
    if (added) SudekiMpLogFormat(
        "training_weapons category=%u available=%u added=%u scope=testroom_only policy=native_add_missing_preserve_equipment\r\n",
        category, available, added);
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

BOOL SudekiMpWeaponActivationPending(void *character, BOOL *pending) {
    void *weapon, *inventory;
    unsigned int category;
    if (!pending) return FALSE;
    *pending = FALSE;
    if (!character_weapon_context(character, &weapon, &inventory, &category) ||
        !readable_memory((uint8_t *)weapon,
            WEAPON_PENDING_ITEM_OFFSET + sizeof(void *)) ||
        *(void **)((uint8_t *)weapon + 0x10u) != character) return FALSE;
    (void)inventory;
    (void)category;
    *pending = *(void **)((uint8_t *)weapon + WEAPON_PENDING_ITEM_OFFSET) != NULL;
    return TRUE;
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
