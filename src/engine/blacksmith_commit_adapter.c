#include "engine/blacksmith_commit_adapter.h"

#include <string.h>

enum {
    SUDEKI_PREFERRED_IMAGE_BASE = 0x00400000u,
    RVA_SUBTRACT_MONEY = 0x00021cb0u,
    RVA_EQUIPMENT_BYTE_WRITE = 0x000220c0u,
    RVA_LOW_LEVEL_MUTATION = 0x00130730u,
    RVA_COMPATIBILITY = 0x001307e0u,
    RVA_SOCKET_READ = 0x00130850u,
    RVA_RUNE_MANAGER_GLOBAL = 0x00408d60u,
    RVA_INVENTORY_GLOBAL = 0x00408d84u
};

static const uint8_t subtract_money_entry[] = {
    0x8b, 0x44, 0x24, 0x04, 0x29, 0x81, 0x34, 0x01,
    0x00, 0x00, 0xc2, 0x04, 0x00
};
static const uint8_t equipment_byte_write_entry[] = {
    0x8b, 0x44, 0x24, 0x04, 0x83, 0xf8, 0x35, 0x77,
    0x0e, 0x8d, 0x14, 0x42, 0x03, 0xd0, 0x88, 0x4c,
    0x32, 0x10, 0xb0, 0x01, 0xc2, 0x04, 0x00, 0x57,
    0x8d, 0x78, 0x9c, 0x83, 0xff, 0x27, 0x5f, 0x77,
    0x0e, 0x8d, 0x14, 0x42, 0x03, 0xd0, 0x88, 0x4c,
    0x32, 0x86, 0xb0, 0x01, 0xc2, 0x04, 0x00, 0x32,
    0xc0, 0xc2, 0x04, 0x00
};
static const uint8_t mutation_prefix[] = {
    0x53, 0x8b, 0x5c, 0x24, 0x08, 0x55, 0x8b, 0x2d
};
static const uint8_t mutation_suffix[] = {
    0x56, 0x8b, 0xf0, 0x3b, 0xbe, 0xe0, 0x00, 0x00,
    0x00, 0x73, 0x5e, 0x83, 0xc9, 0xff, 0x3b, 0xd9,
    0x75, 0x34
};
static const uint8_t mutation_call_path[] = {
    0x57, 0x8b, 0xcb, 0xe8, 0x56, 0x00, 0x00, 0x00,
    0x84, 0xc0, 0x74, 0x17, 0x8b, 0x46, 0x14, 0x50,
    0x8a, 0xcb, 0x8b, 0xd7, 0x8b, 0xf5, 0xe8, 0x23,
    0x19, 0xef, 0xff, 0x5e, 0x5d, 0xb0, 0x01, 0x5b,
    0xc2, 0x04, 0x00
};
static const uint8_t compatibility_prefix[] = {
    0x53, 0x8b, 0x5c, 0x24, 0x08, 0x57, 0x3b, 0x9e,
    0xe0, 0x00, 0x00, 0x00, 0x73, 0x5b, 0x85, 0xc9,
    0x78, 0x57, 0xa1
};
static const uint8_t compatibility_suffix[] = {
    0x3b, 0x48, 0x14, 0x73, 0x4d, 0x8b, 0x40, 0x1c,
    0x8b, 0x3c, 0x88, 0x85, 0xff, 0x74, 0x43, 0x8b,
    0x8e, 0xe8, 0x00, 0x00, 0x00, 0x8b, 0x14, 0x99,
    0x80, 0x7a, 0x0c, 0x00, 0x75, 0x34, 0x8b, 0x06,
    0x8b, 0x50, 0x34, 0x8b, 0xce, 0xff, 0xd2, 0x8b,
    0xc8, 0x8b, 0xc7, 0xe8, 0xd9, 0x21, 0x00, 0x00,
    0x3b, 0xc1, 0x75, 0x1e, 0x8b, 0xcf, 0xe8, 0x2e,
    0xca, 0xf5, 0xff, 0x8b, 0x16, 0x8b, 0xf8, 0x8b,
    0x42, 0x30, 0x53, 0x8b, 0xce, 0xff, 0xd0, 0x3b,
    0xf8, 0x75, 0x07, 0x5f, 0xb0, 0x01, 0x5b, 0xc2,
    0x04, 0x00, 0x5f, 0x32, 0xc0, 0x5b, 0xc2, 0x04,
    0x00
};
static const uint8_t socket_read_prefix[] = {
    0x56, 0x8b, 0xf0, 0x3b, 0x8e, 0xe0, 0x00, 0x00,
    0x00, 0x73, 0x3f, 0x8b, 0x86, 0xe8, 0x00, 0x00,
    0x00, 0x8b, 0x14, 0x88, 0x8b, 0x42, 0x08, 0x83,
    0xf8, 0xff, 0x75, 0x31, 0x8b, 0x46, 0x14, 0x8b,
    0x15
};
static const uint8_t socket_read_suffix[] = {
    0x83, 0xf8, 0x35, 0x77, 0x0c, 0x8d, 0x14, 0x42,
    0x03, 0xd0, 0x0f, 0xbe, 0x44, 0x0a, 0x10, 0x5e,
    0xc3
};
static const uint8_t socket_read_second_domain[] = {
    0x8d, 0x70, 0x9c, 0x83, 0xfe, 0x27, 0x77, 0x0c,
    0x8d, 0x14, 0x42, 0x03, 0xd0, 0x0f, 0xbe, 0x44,
    0x0a, 0x86, 0x5e, 0xc3, 0x83, 0xc8, 0xff, 0x5e,
    0xc3
};

static int bytes_match(
    const uint8_t *image,
    size_t image_size,
    size_t offset,
    const uint8_t *expected,
    size_t expected_size
) {
    return image != NULL && expected != NULL &&
        offset <= image_size && expected_size <= image_size - offset &&
        memcmp(image + offset, expected, expected_size) == 0;
}

static int relocated_entry_matches(
    const uint8_t *image,
    size_t image_size,
    size_t offset,
    const uint8_t *prefix,
    size_t prefix_size,
    uintptr_t expected_address,
    const uint8_t *suffix,
    size_t suffix_size
) {
    uint32_t encoded_address;

    if (expected_address > UINT32_MAX ||
        !bytes_match(
            image, image_size, offset, prefix, prefix_size) ||
        offset > image_size ||
        prefix_size > image_size - offset ||
        sizeof(encoded_address) > image_size - offset - prefix_size) {
        return 0;
    }
    memcpy(
        &encoded_address,
        image + offset + prefix_size,
        sizeof(encoded_address)
    );
    return encoded_address == (uint32_t)expected_address &&
        bytes_match(
            image,
            image_size,
            offset + prefix_size + sizeof(encoded_address),
            suffix,
            suffix_size
        );
}

int SudekiMpBlacksmithCommitLoadedSignaturesMatch(
    const uint8_t *image,
    size_t image_size,
    uintptr_t loaded_image_base
) {
    uintptr_t inventory_global;
    uintptr_t rune_manager_global;

    if (loaded_image_base > UINT32_MAX ||
        loaded_image_base > UINT32_MAX - RVA_INVENTORY_GLOBAL) {
        return 0;
    }
    inventory_global = loaded_image_base + RVA_INVENTORY_GLOBAL;
    rune_manager_global = loaded_image_base + RVA_RUNE_MANAGER_GLOBAL;
    return bytes_match(
            image,
            image_size,
            RVA_SUBTRACT_MONEY,
            subtract_money_entry,
            sizeof(subtract_money_entry)) &&
        bytes_match(
            image,
            image_size,
            RVA_EQUIPMENT_BYTE_WRITE,
            equipment_byte_write_entry,
            sizeof(equipment_byte_write_entry)) &&
        relocated_entry_matches(
            image,
            image_size,
            RVA_LOW_LEVEL_MUTATION,
            mutation_prefix,
            sizeof(mutation_prefix),
            inventory_global,
            mutation_suffix,
            sizeof(mutation_suffix)) &&
        bytes_match(
            image,
            image_size,
            RVA_LOW_LEVEL_MUTATION + 0x52u,
            mutation_call_path,
            sizeof(mutation_call_path)) &&
        relocated_entry_matches(
            image,
            image_size,
            RVA_COMPATIBILITY,
            compatibility_prefix,
            sizeof(compatibility_prefix),
            rune_manager_global,
            compatibility_suffix,
            sizeof(compatibility_suffix)) &&
        relocated_entry_matches(
            image,
            image_size,
            RVA_SOCKET_READ,
            socket_read_prefix,
            sizeof(socket_read_prefix),
            inventory_global,
            socket_read_suffix,
            sizeof(socket_read_suffix)) &&
        bytes_match(
            image,
            image_size,
            RVA_SOCKET_READ + 0x36u,
            socket_read_second_domain,
            sizeof(socket_read_second_domain));
}

int SudekiMpBlacksmithCommitSignaturesMatch(
    const uint8_t *image,
    size_t image_size
) {
    return SudekiMpBlacksmithCommitLoadedSignaturesMatch(
        image,
        image_size,
        SUDEKI_PREFERRED_IMAGE_BASE
    );
}

static uint32_t advance_nonzero(uint32_t value) {
    ++value;
    if (value == 0u) {
        ++value;
    }
    return value;
}

static int valid_snapshot(
    const SudekiMpBlacksmithSharedSnapshot *snapshot
) {
    return snapshot != NULL && snapshot->world_generation != 0u &&
        snapshot->catalog_generation != 0u &&
        snapshot->inventory_generation != 0u &&
        snapshot->economy_generation != 0u;
}

static int same_snapshot(
    const SudekiMpBlacksmithSharedSnapshot *left,
    const SudekiMpBlacksmithSharedSnapshot *right
) {
    return left->world_generation == right->world_generation &&
        left->catalog_generation == right->catalog_generation &&
        left->inventory_generation == right->inventory_generation &&
        left->economy_generation == right->economy_generation;
}

static int supported_equipment_id(uint32_t item_id) {
    return item_id <= 0x35u || (item_id >= 100u && item_id <= 139u);
}

static int same_stable_authority(
    const SudekiMpBlacksmithCommitTicket *ticket,
    const SudekiMpBlacksmithCommitObservation *observation
) {
    return observation->actor_resolved &&
        observation->character_id == ticket->character_id &&
        observation->actor_generation == ticket->actor_generation &&
        observation->merchant_resolved &&
        observation->merchant_id == ticket->merchant_id &&
        observation->merchant_generation == ticket->merchant_generation &&
        observation->equipment_resolved &&
        observation->equipment_is_equipped_by_actor &&
        observation->equipment_item_id ==
            ticket->selection.equipment_item_id &&
        observation->equipment_socket_bank ==
            ticket->selection.socket_bank &&
        observation->component_resolved &&
        observation->component_item_id ==
            ticket->selection.component_item_id;
}

static int immutable_commit_facts_unchanged(
    const SudekiMpBlacksmithCommitTicket *ticket,
    const SudekiMpBlacksmithCommitObservation *observation
) {
    return valid_snapshot(&observation->snapshot) &&
        observation->snapshot.world_generation ==
            ticket->snapshot.world_generation &&
        observation->snapshot.catalog_generation ==
            ticket->snapshot.catalog_generation &&
        same_stable_authority(ticket, observation) &&
        observation->catalog_match_count == 1u &&
        observation->catalog_price == ticket->quoted_cost &&
        !observation->save_or_load_active &&
        !observation->native_blacksmith_modal_active &&
        observation->socket_byte_resolved &&
        observation->equipment_storage_supported &&
        observation->authored_component_id == -1 &&
        observation->slot_unlocked &&
        observation->component_compatible &&
        observation->equipment_socket_count >= 1u &&
        observation->equipment_socket_count <=
            SUDEKIMP_BLACKSMITH_NATIVE_SOCKET_COUNT &&
        observation->equipment_socket_count >
            ticket->selection.socket_index &&
        ticket->selection.socket_index <
            SUDEKIMP_BLACKSMITH_NATIVE_SOCKET_COUNT;
}

static SudekiMpBlacksmithCommitAdapterStatus set_status(
    SudekiMpBlacksmithCommitAdapter *adapter,
    SudekiMpBlacksmithCommitAdapterStatus status
) {
    adapter->last_status = status;
    return status;
}

static SudekiMpBlacksmithCommitAdapterStatus quarantine(
    SudekiMpBlacksmithCommitAdapter *adapter,
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    const SudekiMpBlacksmithCommitTicket *ticket
) {
    adapter->state = SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_QUARANTINED;
    adapter->last_ticket_serial = ticket == NULL ? 0u : ticket->serial;
    if (coordinator != NULL && ticket != NULL) {
        (void)SudekiMpBlacksmithShadowResolveCommitTicket(
            coordinator,
            ticket->serial,
            SUDEKIMP_BLACKSMITH_COMMIT_AMBIGUOUS,
            NULL
        );
    }
    return set_status(
        adapter,
        SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED
    );
}

static SudekiMpBlacksmithCommitAdapterStatus reject_without_mutation(
    SudekiMpBlacksmithCommitAdapter *adapter,
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    const SudekiMpBlacksmithCommitTicket *ticket,
    const SudekiMpBlacksmithCommitObservation *observation,
    SudekiMpBlacksmithCommitAdapterStatus rejection
) {
    SudekiMpBlacksmithShadowResult result;

    result = SudekiMpBlacksmithShadowResolveCommitTicket(
        coordinator,
        ticket->serial,
        SUDEKIMP_BLACKSMITH_COMMIT_NOT_APPLIED,
        &observation->snapshot
    );
    if (result != SUDEKIMP_BLACKSMITH_SHADOW_APPLIED) {
        return quarantine(adapter, coordinator, ticket);
    }
    adapter->state = SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_IDLE;
    adapter->last_ticket_serial = ticket->serial;
    return set_status(adapter, rejection);
}

static SudekiMpBlacksmithCommitAdapterStatus validate_preflight(
    const SudekiMpBlacksmithCommitTicket *ticket,
    const SudekiMpBlacksmithCommitObservation *observation
) {
    if (!valid_snapshot(&observation->snapshot) ||
        !same_snapshot(&ticket->snapshot, &observation->snapshot)) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_STALE_GENERATION;
    }
    if (observation->save_or_load_active) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_LIFECYCLE_UNSAFE;
    }
    if (observation->native_blacksmith_modal_active) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_NATIVE_MODAL_ACTIVE;
    }
    if (!observation->actor_resolved ||
        observation->character_id != ticket->character_id ||
        observation->actor_generation != ticket->actor_generation) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_ACTOR_STALE;
    }
    if (!observation->merchant_resolved ||
        observation->merchant_id != ticket->merchant_id ||
        observation->merchant_generation != ticket->merchant_generation) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_MERCHANT_STALE;
    }
    if (!supported_equipment_id(ticket->selection.equipment_item_id) ||
        !observation->equipment_resolved ||
        !observation->equipment_is_equipped_by_actor ||
        observation->equipment_item_id !=
            ticket->selection.equipment_item_id ||
        observation->equipment_socket_bank !=
            ticket->selection.socket_bank) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_EQUIPMENT_STALE;
    }
    if (ticket->selection.component_item_id > INT8_MAX ||
        !observation->component_resolved ||
        observation->component_item_id !=
            ticket->selection.component_item_id) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_COMPONENT_UNAVAILABLE;
    }
    if (observation->catalog_match_count == 0u) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_CATALOG_MISSING;
    }
    if (observation->catalog_match_count != 1u) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_CATALOG_DUPLICATE;
    }
    if (observation->catalog_price != ticket->quoted_cost) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_PRICE_CHANGED;
    }
    if (ticket->quoted_cost == 0u) {
        return
            SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_ZERO_COST_UNSUPPORTED;
    }
    if (ticket->selection.socket_bank != 0 &&
        ticket->selection.socket_bank != 1) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_INVALID_SOCKET;
    }
    if (!observation->socket_byte_resolved ||
        observation->equipment_socket_count == 0u ||
        observation->equipment_socket_count >
            SUDEKIMP_BLACKSMITH_NATIVE_SOCKET_COUNT ||
        ticket->selection.socket_index >=
            SUDEKIMP_BLACKSMITH_NATIVE_SOCKET_COUNT ||
        ticket->selection.socket_index >=
            observation->equipment_socket_count) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_INVALID_SOCKET;
    }
    if (!observation->equipment_storage_supported ||
        observation->authored_component_id != -1 ||
        !observation->slot_unlocked) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_SLOT_LOCKED;
    }
    if (observation->socket_byte != SUDEKIMP_BLACKSMITH_EMPTY_SOCKET) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_SOCKET_OCCUPIED;
    }
    if (observation->forbidden_duplicate_present) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_DUPLICATE_COMPONENT;
    }
    if (!observation->component_compatible) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_INCOMPATIBLE;
    }
    if (observation->shared_money < ticket->quoted_cost) {
        return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_INSUFFICIENT_FUNDS;
    }
    return SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_VERIFIED;
}

void SudekiMpBlacksmithCommitAdapterInitialize(
    SudekiMpBlacksmithCommitAdapter *adapter
) {
    if (adapter == NULL) {
        return;
    }
    memset(adapter, 0, sizeof(*adapter));
    adapter->state = SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_DISABLED;
    adapter->last_status = SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_DISABLED;
}

int SudekiMpBlacksmithCommitAdapterSetEnabled(
    SudekiMpBlacksmithCommitAdapter *adapter,
    int enabled
) {
    if (adapter == NULL ||
        adapter->state == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_EXECUTING ||
        adapter->state == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_QUARANTINED) {
        return 0;
    }
    adapter->state = enabled ? SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_IDLE :
        SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_DISABLED;
    adapter->last_status = enabled ?
        SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_NO_CLAIM :
        SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_DISABLED;
    return 1;
}

SudekiMpBlacksmithCommitAdapterStatus SudekiMpBlacksmithCommitAdapterExecute(
    SudekiMpBlacksmithCommitAdapter *adapter,
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    const SudekiMpBlacksmithCommitBackend *backend
) {
    SudekiMpBlacksmithCommitTicket ticket;
    SudekiMpBlacksmithCommitObservation before;
    SudekiMpBlacksmithCommitObservation after_mutation;
    SudekiMpBlacksmithCommitObservation after_debit;
    SudekiMpBlacksmithCommitReadResult read_result;
    SudekiMpBlacksmithLowLevelMutationResult mutation_result;
    SudekiMpBlacksmithDebitResult debit_result;
    SudekiMpBlacksmithCommitAdapterStatus preflight;
    SudekiMpBlacksmithShadowResult resolution;
    uint32_t expected_money;

    if (adapter == NULL || coordinator == NULL || backend == NULL ||
        backend->is_game_thread == NULL || backend->resolve == NULL ||
        backend->call_low_level_mutation == NULL ||
        backend->subtract_money_once == NULL) {
        return adapter == NULL ?
            SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_INVALID_ARGUMENT :
            set_status(
                adapter,
                SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_INVALID_ARGUMENT
            );
    }
    if (adapter->state == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_DISABLED) {
        return set_status(
            adapter,
            SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_DISABLED
        );
    }
    if (adapter->state == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_QUARANTINED) {
        return set_status(
            adapter,
            SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED
        );
    }
    if (adapter->state == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_EXECUTING) {
        return set_status(
            adapter,
            SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_BUSY
        );
    }
    if (!backend->is_game_thread(backend->context)) {
        return set_status(
            adapter,
            SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_WRONG_THREAD
        );
    }
    if (coordinator->commit_lane_state !=
            SUDEKIMP_BLACKSMITH_COMMIT_LANE_CLAIMED ||
        !SudekiMpBlacksmithShadowGetCommitTicket(coordinator, &ticket)) {
        return set_status(
            adapter,
            SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_NO_CLAIM
        );
    }

    adapter->state = SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_EXECUTING;
    adapter->last_ticket_serial = ticket.serial;
    memset(&before, 0, sizeof(before));
    read_result = backend->resolve(
        backend->context, &ticket, &before);
    if (read_result != SUDEKIMP_BLACKSMITH_COMMIT_READ_RESOLVED) {
        return quarantine(adapter, coordinator, &ticket);
    }
    preflight = validate_preflight(&ticket, &before);
    if (preflight ==
            SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_LIFECYCLE_UNSAFE ||
        preflight ==
            SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_NATIVE_MODAL_ACTIVE ||
        preflight == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_ACTOR_STALE ||
        preflight == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_MERCHANT_STALE ||
        (preflight ==
            SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_STALE_GENERATION &&
         before.snapshot.world_generation !=
            ticket.snapshot.world_generation)) {
        return quarantine(adapter, coordinator, &ticket);
    }
    if (preflight != SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_VERIFIED) {
        return reject_without_mutation(
            adapter, coordinator, &ticket, &before, preflight);
    }

    mutation_result = backend->call_low_level_mutation(
        backend->context, &ticket);
    memset(&after_mutation, 0, sizeof(after_mutation));
    read_result = backend->resolve(
        backend->context, &ticket, &after_mutation);
    if (read_result != SUDEKIMP_BLACKSMITH_COMMIT_READ_RESOLVED) {
        return quarantine(adapter, coordinator, &ticket);
    }
    if (mutation_result ==
            SUDEKIMP_BLACKSMITH_LOW_LEVEL_MUTATION_RETURNED_FALSE &&
        immutable_commit_facts_unchanged(&ticket, &after_mutation) &&
        same_snapshot(&ticket.snapshot, &after_mutation.snapshot) &&
        after_mutation.socket_byte == before.socket_byte &&
        after_mutation.shared_money == before.shared_money) {
        return reject_without_mutation(
            adapter,
            coordinator,
            &ticket,
            &after_mutation,
            SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_MUTATION_REJECTED
        );
    }
    if (mutation_result !=
            SUDEKIMP_BLACKSMITH_LOW_LEVEL_MUTATION_RETURNED_TRUE ||
        !immutable_commit_facts_unchanged(&ticket, &after_mutation) ||
        after_mutation.snapshot.inventory_generation ==
            ticket.snapshot.inventory_generation ||
        after_mutation.snapshot.economy_generation !=
            ticket.snapshot.economy_generation ||
        before.socket_byte != SUDEKIMP_BLACKSMITH_EMPTY_SOCKET ||
        after_mutation.socket_byte !=
            (uint8_t)ticket.selection.component_item_id ||
        after_mutation.shared_money != before.shared_money) {
        return quarantine(adapter, coordinator, &ticket);
    }

    /* CInventory::SubtractMoney is unchecked. This is deliberately the only
     * debit call, and it happens only after the exact socket byte is proven. */
    debit_result = backend->subtract_money_once(
        backend->context, &ticket, ticket.quoted_cost);
    if (debit_result != SUDEKIMP_BLACKSMITH_DEBIT_CALLED_ONCE) {
        return quarantine(adapter, coordinator, &ticket);
    }
    memset(&after_debit, 0, sizeof(after_debit));
    read_result = backend->resolve(
        backend->context, &ticket, &after_debit);
    if (read_result != SUDEKIMP_BLACKSMITH_COMMIT_READ_RESOLVED) {
        return quarantine(adapter, coordinator, &ticket);
    }
    expected_money = before.shared_money - ticket.quoted_cost;
    if (!immutable_commit_facts_unchanged(&ticket, &after_debit) ||
        after_debit.snapshot.inventory_generation !=
            after_mutation.snapshot.inventory_generation ||
        after_debit.snapshot.economy_generation ==
            after_mutation.snapshot.economy_generation ||
        after_debit.socket_byte !=
            (uint8_t)ticket.selection.component_item_id ||
        after_debit.shared_money != expected_money) {
        return quarantine(adapter, coordinator, &ticket);
    }

    resolution = SudekiMpBlacksmithShadowResolveCommitTicket(
        coordinator,
        ticket.serial,
        SUDEKIMP_BLACKSMITH_COMMIT_VERIFIED,
        &after_debit.snapshot
    );
    if (resolution != SUDEKIMP_BLACKSMITH_SHADOW_APPLIED) {
        return quarantine(adapter, coordinator, &ticket);
    }
    adapter->state = SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_IDLE;
    adapter->verified_commit_count = advance_nonzero(
        adapter->verified_commit_count);
    return set_status(
        adapter,
        SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_VERIFIED
    );
}

const char *SudekiMpBlacksmithCommitAdapterStatusName(
    SudekiMpBlacksmithCommitAdapterStatus status
) {
    static const char *const names[] = {
        "disabled",
        "invalid_argument",
        "wrong_thread",
        "no_claim",
        "busy",
        "lifecycle_unsafe",
        "native_modal_active",
        "stale_generation",
        "actor_stale",
        "merchant_stale",
        "equipment_stale",
        "component_unavailable",
        "catalog_missing",
        "catalog_duplicate",
        "price_changed",
        "zero_cost_unsupported",
        "invalid_socket",
        "slot_locked",
        "socket_occupied",
        "duplicate_component",
        "incompatible",
        "insufficient_funds",
        "mutation_rejected",
        "verified",
        "quarantined"
    };

    if ((unsigned int)status >= sizeof(names) / sizeof(names[0])) {
        return "unknown";
    }
    return names[status];
}
