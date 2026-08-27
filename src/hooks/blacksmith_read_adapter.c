#include "hooks/blacksmith_read_adapter.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Blacksmith read adapter requires the 32-bit Windows target"
#endif

typedef int (__attribute__((thiscall))
    *ItemSocketBankFunction)(void *item_definition, uint32_t socket_index);
typedef int (__attribute__((thiscall))
    *ItemClassFunction)(void *item_definition);

enum {
    RVA_RUNE_MANAGER_GLOBAL = 0x00408d60u,
    RVA_ITEM_MANAGER_GLOBAL = 0x00408d80u,
    RVA_INVENTORY_GLOBAL = 0x00408d84u,
    RVA_LOCALIZATION_MANAGER_GLOBAL = 0x00409e0cu,
    RVA_BLACKSMITH_INVENTORY_GLOBAL = 0x00409dccu,
    RVA_RUNE_EFFECT_TABLE = 0x00361218u,
    RVA_ITEM_ARMOUR_VTABLE = 0x002d39b8u,
    RVA_ITEM_WEAPON_VTABLE = 0x002d3a28u,
    RVA_ITEM_ARMOUR_SOCKET_BANK = 0x00130ad0u,
    RVA_ITEM_ARMOUR_CLASS = 0x0013ef10u,
    RVA_ITEM_WEAPON_SOCKET_BANK = 0x001307b0u,
    RVA_ITEM_WEAPON_CLASS = 0x001e8240u,
    RVA_LOCALIZATION_VTABLE = 0x002dcca4u,
    RVA_LOCALIZATION_LOOKUP = 0x001b9c00u,
    RVA_INVENTORY_VTABLE = 0x002ca378u,
    RVA_ITEM_MANAGER_VTABLE = 0x002c693cu,
    RVA_BLACKSMITH_INVENTORY_VTABLE = 0x002ca474u,
    RVA_RUNE_MANAGER_VTABLE = 0x002ca48cu,
    ITEM_DEFINITION_LIMIT = 999u,
    RUNE_DEFINITION_LIMIT = 128u,
    INVENTORY_CATEGORY_LIMIT = 64u,
    INVENTORY_ENTRY_LIMIT = 999u,
    BLACKSMITH_CATALOG_LIMIT = 256u,
    INVENTORY_AUGMENTATION_A_OFFSET = 0x10u,
    INVENTORY_AUGMENTATION_A_SIZE = 0xa2u,
    INVENTORY_AUGMENTATION_B_OFFSET = 0xb2u,
    INVENTORY_AUGMENTATION_B_SIZE = 0x78u
};

typedef struct NativeReadContext {
    uint8_t *inventory;
    uint8_t *item_manager;
    uint8_t *rune_manager;
    uint8_t *blacksmith_inventory;
    uint8_t *localization_manager;
} NativeReadContext;

static uint8_t *game_base;

static BOOL readable_memory(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    return end >= start && end <= region_end;
}

static BOOL executable_memory(const void *pointer) {
    MEMORY_BASIC_INFORMATION information;
    DWORD protection;

    if (pointer == NULL ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    return protection == PAGE_EXECUTE ||
        protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

static uint64_t hash_start(void) {
    return UINT64_C(1469598103934665603);
}

static uint64_t hash_bytes(
    uint64_t hash,
    const void *bytes,
    size_t size
) {
    const uint8_t *cursor = (const uint8_t *)bytes;
    size_t index;

    for (index = 0u; index < size; ++index) {
        hash ^= cursor[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t hash_u32(uint64_t hash, uint32_t value) {
    return hash_bytes(hash, &value, sizeof(value));
}

static uint64_t hash_pointer(uint64_t hash, const void *pointer) {
    uint32_t value = (uint32_t)(uintptr_t)pointer;
    return hash_u32(hash, value);
}

static void fallback_label(
    char label[SUDEKIMP_BLACKSMITH_READ_LABEL_CAPACITY],
    const char *prefix,
    uint32_t identifier
) {
    wsprintfA(label, "%s %lu", prefix, (unsigned long)identifier);
}

static BOOL copy_upper_utf16(
    char destination[SUDEKIMP_BLACKSMITH_READ_LABEL_CAPACITY],
    const uint16_t *source
) {
    uint32_t index;

    if (destination == NULL || source == NULL) {
        return FALSE;
    }
    for (index = 0u;
         index + 1u < SUDEKIMP_BLACKSMITH_READ_LABEL_CAPACITY;
         ++index) {
        uint16_t value;
        char output;

        if (!readable_memory(source + index, sizeof(*source))) {
            destination[0] = '\0';
            return FALSE;
        }
        value = source[index];
        if (value == 0u) {
            destination[index] = '\0';
            return index != 0u;
        }
        if (value >= 'a' && value <= 'z') {
            output = (char)(value - ('a' - 'A'));
        } else if ((value >= 'A' && value <= 'Z') ||
                (value >= '0' && value <= '9') || value == ' ' ||
                value == '-' || value == ':' || value == '/' ||
                value == '(' || value == ')') {
            output = (char)value;
        } else {
            output = '?';
        }
        destination[index] = output;
    }
    destination[SUDEKIMP_BLACKSMITH_READ_LABEL_CAPACITY - 1u] = '\0';
    return TRUE;
}

static BOOL copy_native_string(
    char destination[SUDEKIMP_BLACKSMITH_READ_LABEL_CAPACITY],
    const uint8_t *definition,
    size_t flag_offset,
    size_t storage_offset
) {
    const uint16_t *text;
    uint32_t flags;

    if (!readable_memory(definition + flag_offset, sizeof(flags)) ||
        !readable_memory(definition + storage_offset, sizeof(text))) {
        return FALSE;
    }
    flags = *(const uint32_t *)(definition + flag_offset);
    if ((flags & UINT32_C(0x80000000)) != 0u) {
        text = (const uint16_t *)(definition + storage_offset);
    } else {
        text = *(const uint16_t *const *)(definition + storage_offset);
    }
    return copy_upper_utf16(destination, text);
}

static BOOL read_global_pointer(uint32_t rva, uint8_t **pointer) {
    if (game_base == NULL || pointer == NULL ||
        !readable_memory(game_base + rva, sizeof(*pointer))) {
        return FALSE;
    }
    *pointer = *(uint8_t **)(game_base + rva);
    return *pointer != NULL;
}

static BOOL resolve_context(NativeReadContext *context) {
    uint32_t rune_count;
    uint8_t **rune_definitions;

    if (context == NULL) {
        return FALSE;
    }
    ZeroMemory(context, sizeof(*context));
    if (!read_global_pointer(RVA_INVENTORY_GLOBAL, &context->inventory) ||
        !read_global_pointer(RVA_ITEM_MANAGER_GLOBAL,
            &context->item_manager) ||
        !read_global_pointer(RVA_RUNE_MANAGER_GLOBAL,
            &context->rune_manager) ||
        !read_global_pointer(RVA_BLACKSMITH_INVENTORY_GLOBAL,
            &context->blacksmith_inventory) ||
        !readable_memory(context->inventory, 0x138u) ||
        !readable_memory(context->item_manager, 0x10u) ||
        !readable_memory(context->rune_manager, 0x20u) ||
        !readable_memory(context->blacksmith_inventory, 0x18u) ||
        *(void **)context->inventory !=
            game_base + RVA_INVENTORY_VTABLE ||
        *(void **)context->item_manager !=
            game_base + RVA_ITEM_MANAGER_VTABLE ||
        *(void **)context->rune_manager !=
            game_base + RVA_RUNE_MANAGER_VTABLE ||
        *(void **)context->blacksmith_inventory !=
            game_base + RVA_BLACKSMITH_INVENTORY_VTABLE) {
        return FALSE;
    }
    rune_count = *(const uint32_t *)(context->rune_manager + 0x14u);
    rune_definitions = *(uint8_t ***)(context->rune_manager + 0x1cu);
    if (rune_count == 0u || rune_count > RUNE_DEFINITION_LIMIT ||
        !readable_memory(rune_definitions,
            rune_count * sizeof(*rune_definitions))) {
        return FALSE;
    }
    (void)read_global_pointer(RVA_LOCALIZATION_MANAGER_GLOBAL,
        &context->localization_manager);
    return TRUE;
}

static uint8_t *resolve_item_definition(
    const NativeReadContext *context,
    uint32_t item_id
) {
    uint8_t **slot;
    uint8_t *definition;

    if (context == NULL || item_id >= ITEM_DEFINITION_LIMIT) {
        return NULL;
    }
    slot = (uint8_t **)(context->item_manager + 0x0cu + item_id * 4u);
    if (!readable_memory(slot, sizeof(*slot))) {
        return NULL;
    }
    definition = *slot;
    if (!readable_memory(definition, 0xf8u) ||
        *(const uint32_t *)(definition + 0x14u) != item_id) {
        return NULL;
    }
    return definition;
}

static uint8_t *resolve_rune_definition(
    const NativeReadContext *context,
    uint32_t component_id
) {
    uint32_t count;
    uint8_t **array;
    uint8_t *definition;

    if (context == NULL) {
        return NULL;
    }
    count = *(const uint32_t *)(context->rune_manager + 0x14u);
    array = *(uint8_t ***)(context->rune_manager + 0x1cu);
    if (count == 0u || count > RUNE_DEFINITION_LIMIT ||
        component_id >= count || !readable_memory(
            array + component_id, sizeof(*array))) {
        return NULL;
    }
    definition = array[component_id];
    return readable_memory(definition, 0x9cu) ? definition : NULL;
}

static void copy_item_name(
    const NativeReadContext *context,
    const uint8_t *definition,
    uint32_t item_id,
    char name[SUDEKIMP_BLACKSMITH_READ_LABEL_CAPACITY]
) {
    int32_t token = *(const int32_t *)(definition + 0x50u);
    void **vtable;
    uint32_t count;
    uint8_t **array;
    uint8_t *entry;
    const uint16_t *text;
    uint32_t flags;

    /* 0x005B9C00 performs no token bounds check. Reproduce only its read
     * semantics after exact vtable/function identity and full backing-store
     * validation; never call the unbounded native method. */
    if (token >= 0 && context->localization_manager != NULL &&
        readable_memory(context->localization_manager, 0x14u)) {
        vtable = *(void ***)context->localization_manager;
        count = *(const uint32_t *)(context->localization_manager + 0x08u);
        array = *(uint8_t ***)(context->localization_manager + 0x10u);
        if (vtable == (void **)(game_base + RVA_LOCALIZATION_VTABLE) &&
            readable_memory(vtable, 0x14u) &&
            vtable[0x10u / sizeof(void *)] ==
                game_base + RVA_LOCALIZATION_LOOKUP &&
            count <= 4096u && (uint32_t)token < count &&
            readable_memory(array, count * sizeof(*array))) {
            entry = array[token];
            if (readable_memory(entry, 8u)) {
                flags = *(const uint32_t *)entry;
                text = (flags & UINT32_C(0x80000000)) != 0u ?
                    (const uint16_t *)(entry + 4u) :
                    *(const uint16_t **)(entry + 4u);
                if (copy_upper_utf16(name, text)) {
                    return;
                }
            }
        }
    }
    fallback_label(name, "ITEM", item_id);
}

static uint32_t rune_effect_class(uint32_t kind) {
    if (kind >= 1u && kind <= 5u) return 1u;
    if (kind >= 6u && kind <= 13u) return 2u;
    return 0u;
}

static BOOL read_component_definition(
    const NativeReadContext *context,
    uint32_t component_id,
    int32_t price,
    SudekiMpBlacksmithReadComponent *component
) {
    uint8_t *definition;
    uint32_t variant;
    uint32_t effect_index;
    const float *effect;

    ZeroMemory(component, sizeof(*component));
    component->component_id = component_id;
    component->price = price;
    definition = resolve_rune_definition(context, component_id);
    if (definition == NULL) {
        fallback_label(component->name, "RUNE", component_id);
        return FALSE;
    }
    component->kind = *(const uint32_t *)(definition + 0x04u);
    variant = *(const uint32_t *)(definition + 0x08u);
    component->effect_class = rune_effect_class(component->kind);
    component->bank = component->effect_class == 1u ? variant : 1u;
    component->definition_valid = component->effect_class != 0u;
    if (!copy_native_string(component->name, definition, 0x14u, 0x18u)) {
        fallback_label(component->name, "RUNE", component_id);
    }
    if (!copy_native_string(component->effect_name,
            definition, 0x94u, 0x98u)) {
        lstrcpynA(component->effect_name, "EFFECT",
            SUDEKIMP_BLACKSMITH_READ_LABEL_CAPACITY);
    }
    if (!component->definition_valid || variant > 1u) {
        return component->definition_valid;
    }
    effect_index = component->kind * 2u + variant;
    effect = (const float *)(game_base + RVA_RUNE_EFFECT_TABLE +
        effect_index * sizeof(float));
    if (readable_memory(effect, sizeof(*effect)) && isfinite(*effect)) {
        component->effect = *effect;
        component->effect_valid = 1;
    }
    return component->definition_valid && component->effect_valid;
}

static int map_character_category(uint32_t character_id) {
    /* Exact 0x0043F430 table results plus four, as consumed by 0x0048FA50. */
    switch (character_id) {
    case 0x23u: return 4; /* Tal -> 0 */
    case 0x01u: return 5; /* Ailish -> 1 */
    case 0x05u: return 6; /* Buki -> 2 */
    case 0x0eu: return 7; /* Elco -> 3 */
    default: return -1;
    }
}

static int32_t effective_component_id(
    const NativeReadContext *context,
    uint32_t item_id,
    uint32_t socket_index,
    int32_t authored_component_id
) {
    const int8_t *stored;

    if (authored_component_id != -1) {
        return authored_component_id;
    }
    if (socket_index >= SUDEKIMP_BLACKSMITH_READ_MAX_SOCKETS) {
        return -1;
    }
    if (item_id < 0x36u) {
        stored = (const int8_t *)(context->inventory + 0x10u +
            item_id * 3u + socket_index);
    } else if (item_id >= 100u && item_id < 140u) {
        stored = (const int8_t *)(context->inventory +
            item_id * 3u - 0x7au + socket_index);
    } else {
        return -1;
    }
    return readable_memory(stored, sizeof(*stored)) ? *stored : -1;
}

static BOOL capture_equipment(
    const NativeReadContext *context,
    uint8_t *definition,
    uint32_t category_id,
    int equipped,
    SudekiMpBlacksmithReadEquipment *equipment
) {
    void **vtable;
    ItemClassFunction get_class;
    ItemSocketBankFunction get_bank;
    uint8_t **socket_array;
    uint32_t socket_count;
    uint32_t socket_index;
    float primary_effect = 0.0f;
    float secondary_effect = 0.0f;
    int32_t base_stat;
    float secondary_base;
    BOOL occupant_effects_complete = TRUE;

    if (context == NULL || definition == NULL || equipment == NULL) {
        return FALSE;
    }
    ZeroMemory(equipment, sizeof(*equipment));
    equipment->item_id = *(const uint32_t *)(definition + 0x14u);
    equipment->category_id = category_id;
    equipment->equipped = equipped != 0;
    copy_item_name(context, definition, equipment->item_id, equipment->name);

    vtable = *(void ***)definition;
    if (!readable_memory(vtable, 0x38u)) {
        return FALSE;
    }
    get_bank = (ItemSocketBankFunction)vtable[0x30u / sizeof(void *)];
    get_class = (ItemClassFunction)vtable[0x34u / sizeof(void *)];
    if (!((vtable == (void **)(game_base + RVA_ITEM_WEAPON_VTABLE) &&
              (const void *)get_bank ==
                  game_base + RVA_ITEM_WEAPON_SOCKET_BANK &&
              (const void *)get_class ==
                  game_base + RVA_ITEM_WEAPON_CLASS) ||
            (vtable == (void **)(game_base + RVA_ITEM_ARMOUR_VTABLE) &&
              (const void *)get_bank ==
                  game_base + RVA_ITEM_ARMOUR_SOCKET_BANK &&
              (const void *)get_class ==
                  game_base + RVA_ITEM_ARMOUR_CLASS)) ||
        !executable_memory((const void *)get_bank) ||
        !executable_memory((const void *)get_class)) {
        return FALSE;
    }
    equipment->item_class = (uint32_t)get_class(definition);
    socket_count = *(const uint32_t *)(definition + 0xe0u);
    socket_array = *(uint8_t ***)(definition + 0xe8u);
    if (socket_count > SUDEKIMP_BLACKSMITH_READ_MAX_SOCKETS ||
        (socket_count != 0u && !readable_memory(
            socket_array, socket_count * sizeof(*socket_array)))) {
        return FALSE;
    }
    equipment->socket_count = socket_count;
    for (socket_index = 0u; socket_index < socket_count; ++socket_index) {
        SudekiMpBlacksmithReadSocket *socket =
            &equipment->sockets[socket_index];
        SudekiMpBlacksmithReadComponent occupant;
        uint8_t *record = socket_array[socket_index];

        if (!readable_memory(record, 0x0du)) {
            return FALSE;
        }
        socket->authored_component_id = *(const int32_t *)(record + 0x08u);
        socket->locked = *(const uint8_t *)(record + 0x0cu) != 0u;
        socket->bank = (uint32_t)get_bank(definition, socket_index);
        socket->occupant_component_id = effective_component_id(
            context, equipment->item_id, socket_index,
            socket->authored_component_id);
        if (socket->occupant_component_id >= 0 &&
            read_component_definition(context,
                (uint32_t)socket->occupant_component_id, 0, &occupant)) {
            socket->occupant_valid = 1;
            socket->occupant_effect_class = occupant.effect_class;
            socket->occupant_effect = occupant.effect;
            lstrcpynA(socket->occupant_name, occupant.name,
                SUDEKIMP_BLACKSMITH_READ_LABEL_CAPACITY);
            if (occupant.effect_class == 1u) {
                primary_effect += occupant.effect;
            } else if (occupant.effect_class == 2u) {
                secondary_effect += occupant.effect;
            }
        } else {
            lstrcpynA(socket->occupant_name,
                socket->occupant_component_id == -1 ? "EMPTY" : "INVALID",
                SUDEKIMP_BLACKSMITH_READ_LABEL_CAPACITY);
            if (socket->occupant_component_id != -1) {
                occupant_effects_complete = FALSE;
            }
        }
    }

    secondary_base = *(const float *)(definition + 0xecu);
    equipment->primary_scale = *(const float *)(definition + 0xf0u);
    base_stat = *(const int32_t *)(definition + 0xf4u);
    equipment->base_stat = (float)base_stat;
    equipment->primary_stat = (equipment->base_stat + primary_effect) *
        equipment->primary_scale;
    equipment->secondary_percent =
        (secondary_base + secondary_effect) * 100.0f;
    equipment->stats_valid = isfinite(secondary_base) &&
        isfinite(equipment->primary_scale) &&
        isfinite(equipment->primary_stat) &&
        isfinite(equipment->secondary_percent) &&
        occupant_effects_complete;
    return TRUE;
}

static BOOL capture_catalog(
    const NativeReadContext *context,
    SudekiMpBlacksmithReadSnapshot *snapshot
) {
    uint32_t native_count;
    uint32_t index;
    uint8_t *node;
    uint8_t *tail;
    uint8_t *previous = NULL;
    uint8_t *seen_nodes[BLACKSMITH_CATALOG_LIMIT];
    uint8_t *seen_payloads[BLACKSMITH_CATALOG_LIMIT];
    int32_t seen_component_ids[BLACKSMITH_CATALOG_LIMIT];
    uint32_t display_count = 0u;
    BOOL incomplete = FALSE;
    uint64_t hash = hash_start();

    native_count = *(const uint32_t *)(context->blacksmith_inventory + 0x0cu);
    node = *(uint8_t **)(context->blacksmith_inventory + 0x10u);
    tail = *(uint8_t **)(context->blacksmith_inventory + 0x14u);
    if (native_count > BLACKSMITH_CATALOG_LIMIT) {
        return FALSE;
    }
    if ((native_count == 0u && (node != NULL || tail != NULL)) ||
        (native_count != 0u && (node == NULL || tail == NULL))) {
        return FALSE;
    }
    hash = hash_pointer(hash, context->blacksmith_inventory);
    hash = hash_pointer(hash, context->rune_manager);
    hash = hash_u32(hash, native_count);
    for (index = 0u; index < native_count; ++index) {
        uint8_t *payload;
        int32_t component_id;
        int32_t price;
        uint32_t seen_index;
        SudekiMpBlacksmithReadComponent component;

        if (!readable_memory(node, 0x0cu)) {
            return FALSE;
        }
        payload = *(uint8_t **)node;
        if (!readable_memory(payload, 8u) ||
            *(uint8_t **)(node + 8u) != previous) {
            return FALSE;
        }
        component_id = *(const int32_t *)payload;
        price = *(const int32_t *)(payload + 4u);
        for (seen_index = 0u; seen_index < index; ++seen_index) {
            if (seen_nodes[seen_index] == node ||
                seen_payloads[seen_index] == payload ||
                seen_component_ids[seen_index] == component_id) {
                return FALSE;
            }
        }
        seen_nodes[index] = node;
        seen_payloads[index] = payload;
        seen_component_ids[index] = component_id;
        hash = hash_u32(hash, (uint32_t)component_id);
        hash = hash_u32(hash, (uint32_t)price);
        if (component_id >= 0 && component_id <= INT8_MAX && price >= 0 &&
            read_component_definition(context, (uint32_t)component_id,
                price, &component)) {
            if (display_count < SUDEKIMP_BLACKSMITH_READ_MAX_COMPONENTS) {
                snapshot->components[display_count++] = component;
            } else {
                incomplete = TRUE;
            }
        } else {
            incomplete = TRUE;
        }
        previous = node;
        node = *(uint8_t **)(previous + 4u);
    }
    if (node != NULL || (native_count != 0u && previous != tail)) {
        return FALSE;
    }
    snapshot->component_count = display_count;
    snapshot->catalog_truncated = incomplete;
    snapshot->catalog_fingerprint = hash;
    return TRUE;
}

static BOOL capture_inventory_fingerprint(
    const NativeReadContext *context,
    uint64_t *fingerprint
) {
    uint32_t category_count;
    uint8_t **categories;
    uint32_t category_index;
    uint64_t hash = hash_start();

    category_count = *(const uint32_t *)(context->inventory + 0x12cu);
    categories = *(uint8_t ***)(context->inventory + 0x0cu);
    if (category_count > INVENTORY_CATEGORY_LIMIT ||
        (category_count != 0u && !readable_memory(
            categories, category_count * sizeof(*categories)))) {
        return FALSE;
    }
    hash = hash_pointer(hash, context->inventory);
    hash = hash_pointer(hash, context->item_manager);
    hash = hash_u32(hash, category_count);
    for (category_index = 0u;
         category_index < category_count; ++category_index) {
        uint8_t *category = categories[category_index];
        int16_t last_index;
        uint32_t entry_count;
        const uint8_t *entries;
        uint32_t entry_index;

        if (!readable_memory(category, 0x10u)) {
            return FALSE;
        }
        last_index = *(const int16_t *)(category + 0x0eu);
        entry_count = last_index < 0 ? 0u : (uint32_t)last_index + 1u;
        entries = *(const uint8_t **)(category + 0x04u);
        if (entry_count > INVENTORY_ENTRY_LIMIT ||
            (entry_count != 0u && !readable_memory(
                entries, entry_count * 4u))) {
            return FALSE;
        }
        hash = hash_u32(hash, *(const uint32_t *)(category + 0x08u));
        hash = hash_u32(hash, entry_count);
        for (entry_index = 0u; entry_index < entry_count; ++entry_index) {
            hash = hash_bytes(hash, entries + entry_index * 4u, 4u);
        }
    }
    if (!readable_memory(context->inventory +
            INVENTORY_AUGMENTATION_A_OFFSET,
            INVENTORY_AUGMENTATION_A_SIZE) ||
        !readable_memory(context->inventory +
            INVENTORY_AUGMENTATION_B_OFFSET,
            INVENTORY_AUGMENTATION_B_SIZE)) {
        return FALSE;
    }
    hash = hash_bytes(hash,
        context->inventory + INVENTORY_AUGMENTATION_A_OFFSET,
        INVENTORY_AUGMENTATION_A_SIZE);
    hash = hash_bytes(hash,
        context->inventory + INVENTORY_AUGMENTATION_B_OFFSET,
        INVENTORY_AUGMENTATION_B_SIZE);
    *fingerprint = hash;
    return TRUE;
}

static uint8_t *find_inventory_category(
    const NativeReadContext *context,
    uint32_t category_id
) {
    uint32_t count = *(const uint32_t *)(context->inventory + 0x12cu);
    uint8_t **categories = *(uint8_t ***)(context->inventory + 0x0cu);
    uint32_t index;

    if (count > INVENTORY_CATEGORY_LIMIT ||
        (count != 0u && !readable_memory(
            categories, count * sizeof(*categories)))) {
        return NULL;
    }
    for (index = 0u; index < count; ++index) {
        if (!readable_memory(categories[index], 0x10u)) {
            return NULL;
        }
        if (*(const uint32_t *)(categories[index] + 0x08u) == category_id) {
            return categories[index];
        }
    }
    return NULL;
}

static BOOL capture_seat(
    const NativeReadContext *context,
    const SudekiMpBlacksmithReadSeatRequest *request,
    SudekiMpBlacksmithReadSeat *seat,
    uint64_t *inventory_fingerprint
) {
    uint8_t *actor = (uint8_t *)request->actor;
    uint8_t *equipment_component;
    uint8_t *equipped_definition;
    uint8_t *category;
    const uint8_t *entries;
    int16_t last_index;
    uint32_t entry_count;
    uint32_t entry_index;
    int category_id = map_character_category(request->character_id);

    if (category_id < 0 || request->actor == 0u ||
        !request->active_group_proven ||
        request->actor_generation == 0u ||
        !readable_memory(actor, 0xe4u)) {
        return FALSE;
    }
    ZeroMemory(seat, sizeof(*seat));
    seat->character_id = request->character_id;
    seat->actor_generation = request->actor_generation;
    seat->equipped_index = UINT32_MAX;
    equipment_component = *(uint8_t **)(actor + 0xe0u);
    if (!readable_memory(equipment_component, 0x1cu)) {
        return FALSE;
    }
    equipped_definition = *(uint8_t **)(equipment_component + 0x18u);
    if (equipped_definition != NULL) {
        uint32_t equipped_id;
        if (!readable_memory(equipped_definition, 0xf8u)) {
            return FALSE;
        }
        equipped_id = *(const uint32_t *)(equipped_definition + 0x14u);
        if (resolve_item_definition(context, equipped_id) !=
                equipped_definition ||
            !capture_equipment(context, equipped_definition,
                (uint32_t)category_id, 1, &seat->equipment[0])) {
            return FALSE;
        }
        seat->equipment_count = 1u;
        seat->equipped_index = 0u;
        *inventory_fingerprint = hash_u32(
            *inventory_fingerprint, equipped_id);
    } else {
        *inventory_fingerprint = hash_u32(
            *inventory_fingerprint, UINT32_MAX);
    }

    category = find_inventory_category(context, (uint32_t)category_id);
    if (category == NULL) {
        return FALSE;
    }
    last_index = *(const int16_t *)(category + 0x0eu);
    entry_count = last_index < 0 ? 0u : (uint32_t)last_index + 1u;
    entries = *(const uint8_t **)(category + 0x04u);
    if (entry_count > INVENTORY_ENTRY_LIMIT ||
        (entry_count != 0u && !readable_memory(entries, entry_count * 4u))) {
        return FALSE;
    }
    for (entry_index = 0u; entry_index < entry_count; ++entry_index) {
        int16_t signed_id = *(const int16_t *)(entries + entry_index * 4u);
        uint32_t item_id;
        uint8_t *definition;
        uint32_t existing;

        if (signed_id < 0) continue;
        item_id = (uint32_t)signed_id;
        for (existing = 0u; existing < seat->equipment_count; ++existing) {
            if (seat->equipment[existing].item_id == item_id) break;
        }
        if (existing < seat->equipment_count) continue;
        definition = resolve_item_definition(context, item_id);
        if (definition == NULL) continue;
        if (seat->equipment_count <
                SUDEKIMP_BLACKSMITH_READ_MAX_EQUIPMENT) {
            if (!capture_equipment(context, definition,
                    (uint32_t)category_id, 0,
                    &seat->equipment[seat->equipment_count])) {
                return FALSE;
            }
            ++seat->equipment_count;
        } else {
            seat->equipment_truncated = 1;
        }
    }
    seat->valid = 1;
    *inventory_fingerprint = hash_pointer(
        *inventory_fingerprint, actor);
    *inventory_fingerprint = hash_u32(
        *inventory_fingerprint, request->character_id);
    *inventory_fingerprint = hash_u32(
        *inventory_fingerprint, request->actor_generation);
    return TRUE;
}

BOOL SudekiMpBlacksmithReadAdapterInitialize(HMODULE game_module) {
    if (game_module == NULL || game_base != NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    game_base = (uint8_t *)game_module;
    return TRUE;
}

BOOL SudekiMpBlacksmithReadAdapterCapture(
    const SudekiMpBlacksmithReadSeatRequest *requests,
    uint32_t player_count,
    SudekiMpBlacksmithReadSnapshot *snapshot
) {
    NativeReadContext context;
    uint64_t inventory_fingerprint;
    uint32_t player_index;

    if (game_base == NULL || requests == NULL || snapshot == NULL ||
        player_count == 0u ||
        player_count > SUDEKIMP_BLACKSMITH_READ_MAX_PLAYERS) {
        return FALSE;
    }
    ZeroMemory(snapshot, sizeof(*snapshot));
    if (!resolve_context(&context) ||
        !capture_catalog(&context, snapshot) ||
        !capture_inventory_fingerprint(&context, &inventory_fingerprint)) {
        ZeroMemory(snapshot, sizeof(*snapshot));
        return FALSE;
    }
    for (player_index = 0u; player_index < player_count; ++player_index) {
        if (!capture_seat(&context, &requests[player_index],
                &snapshot->seats[player_index],
                &inventory_fingerprint)) {
            ZeroMemory(snapshot, sizeof(*snapshot));
            return FALSE;
        }
    }
    snapshot->player_count = player_count;
    snapshot->inventory_fingerprint = inventory_fingerprint;
    snapshot->valid = 1;
    return TRUE;
}

void SudekiMpBlacksmithReadAdapterReset(void) {
    game_base = NULL;
}
