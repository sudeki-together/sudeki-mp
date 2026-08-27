#include "engine/blacksmith_commit_adapter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

typedef enum FakeMutationMode {
    FAKE_MUTATION_SUCCESS = 0,
    FAKE_MUTATION_RETURN_FALSE,
    FAKE_MUTATION_FALSE_WITH_WRITE,
    FAKE_MUTATION_SUCCESS_WITHOUT_WRITE,
    FAKE_MUTATION_WRONG_BYTE,
    FAKE_MUTATION_AMBIGUOUS
} FakeMutationMode;

typedef enum FakeDebitMode {
    FAKE_DEBIT_SUCCESS = 0,
    FAKE_DEBIT_WRONG_AMOUNT,
    FAKE_DEBIT_AMBIGUOUS
} FakeDebitMode;

typedef struct FakeBackend {
    SudekiMpBlacksmithCommitObservation observation;
    int on_game_thread;
    unsigned int resolve_calls;
    unsigned int fail_resolve_call;
    unsigned int mutation_calls;
    unsigned int debit_calls;
    unsigned int operation_sequence;
    unsigned int mutation_sequence;
    unsigned int debit_sequence;
    FakeMutationMode mutation_mode;
    FakeDebitMode debit_mode;
    int mutation_changes_money;
    int mutation_skips_inventory_generation;
    int debit_skips_economy_generation;
    int debit_changes_inventory_generation;
} FakeBackend;

typedef struct Fixture {
    SudekiMpBlacksmithShadowCoordinator coordinator;
    SudekiMpBlacksmithCommitAdapter adapter;
    SudekiMpBlacksmithCommitBackend backend;
    FakeBackend fake;
} Fixture;

enum {
    TEST_IMAGE_SIZE = 0x00130900u,
    TEST_RVA_SUBTRACT_MONEY = 0x00021cb0u,
    TEST_RVA_EQUIPMENT_BYTE_WRITE = 0x000220c0u,
    TEST_RVA_LOW_LEVEL_MUTATION = 0x00130730u,
    TEST_RVA_COMPATIBILITY = 0x001307e0u,
    TEST_RVA_SOCKET_READ = 0x00130850u,
    TEST_RVA_RUNE_MANAGER_GLOBAL = 0x00408d60u,
    TEST_RVA_INVENTORY_GLOBAL = 0x00408d84u
};

static uint8_t test_image[TEST_IMAGE_SIZE];

static void put_u32(uint8_t *target, uint32_t value) {
    memcpy(target, &value, sizeof(value));
}

static void construct_signature_image(uint32_t image_base) {
    static const uint8_t subtract_money[] = {
        0x8b,0x44,0x24,0x04,0x29,0x81,0x34,0x01,
        0x00,0x00,0xc2,0x04,0x00
    };
    static const uint8_t equipment_write[] = {
        0x8b,0x44,0x24,0x04,0x83,0xf8,0x35,0x77,
        0x0e,0x8d,0x14,0x42,0x03,0xd0,0x88,0x4c,
        0x32,0x10,0xb0,0x01,0xc2,0x04,0x00,0x57,
        0x8d,0x78,0x9c,0x83,0xff,0x27,0x5f,0x77,
        0x0e,0x8d,0x14,0x42,0x03,0xd0,0x88,0x4c,
        0x32,0x86,0xb0,0x01,0xc2,0x04,0x00,0x32,
        0xc0,0xc2,0x04,0x00
    };
    static const uint8_t mutation_prefix[] = {
        0x53,0x8b,0x5c,0x24,0x08,0x55,0x8b,0x2d
    };
    static const uint8_t mutation_suffix[] = {
        0x56,0x8b,0xf0,0x3b,0xbe,0xe0,0x00,0x00,
        0x00,0x73,0x5e,0x83,0xc9,0xff,0x3b,0xd9,
        0x75,0x34
    };
    static const uint8_t mutation_calls[] = {
        0x57,0x8b,0xcb,0xe8,0x56,0x00,0x00,0x00,
        0x84,0xc0,0x74,0x17,0x8b,0x46,0x14,0x50,
        0x8a,0xcb,0x8b,0xd7,0x8b,0xf5,0xe8,0x23,
        0x19,0xef,0xff,0x5e,0x5d,0xb0,0x01,0x5b,
        0xc2,0x04,0x00
    };
    static const uint8_t compatibility_prefix[] = {
        0x53,0x8b,0x5c,0x24,0x08,0x57,0x3b,0x9e,
        0xe0,0x00,0x00,0x00,0x73,0x5b,0x85,0xc9,
        0x78,0x57,0xa1
    };
    static const uint8_t compatibility_suffix[] = {
        0x3b,0x48,0x14,0x73,0x4d,0x8b,0x40,0x1c,
        0x8b,0x3c,0x88,0x85,0xff,0x74,0x43,0x8b,
        0x8e,0xe8,0x00,0x00,0x00,0x8b,0x14,0x99,
        0x80,0x7a,0x0c,0x00,0x75,0x34,0x8b,0x06,
        0x8b,0x50,0x34,0x8b,0xce,0xff,0xd2,0x8b,
        0xc8,0x8b,0xc7,0xe8,0xd9,0x21,0x00,0x00,
        0x3b,0xc1,0x75,0x1e,0x8b,0xcf,0xe8,0x2e,
        0xca,0xf5,0xff,0x8b,0x16,0x8b,0xf8,0x8b,
        0x42,0x30,0x53,0x8b,0xce,0xff,0xd0,0x3b,
        0xf8,0x75,0x07,0x5f,0xb0,0x01,0x5b,0xc2,
        0x04,0x00,0x5f,0x32,0xc0,0x5b,0xc2,0x04,
        0x00
    };
    static const uint8_t socket_prefix[] = {
        0x56,0x8b,0xf0,0x3b,0x8e,0xe0,0x00,0x00,
        0x00,0x73,0x3f,0x8b,0x86,0xe8,0x00,0x00,
        0x00,0x8b,0x14,0x88,0x8b,0x42,0x08,0x83,
        0xf8,0xff,0x75,0x31,0x8b,0x46,0x14,0x8b,
        0x15
    };
    static const uint8_t socket_suffix[] = {
        0x83,0xf8,0x35,0x77,0x0c,0x8d,0x14,0x42,
        0x03,0xd0,0x0f,0xbe,0x44,0x0a,0x10,0x5e,
        0xc3
    };
    static const uint8_t socket_second_domain[] = {
        0x8d,0x70,0x9c,0x83,0xfe,0x27,0x77,0x0c,
        0x8d,0x14,0x42,0x03,0xd0,0x0f,0xbe,0x44,
        0x0a,0x86,0x5e,0xc3,0x83,0xc8,0xff,0x5e,
        0xc3
    };

    memset(test_image, 0, sizeof(test_image));
    memcpy(test_image + TEST_RVA_SUBTRACT_MONEY,
        subtract_money, sizeof(subtract_money));
    memcpy(test_image + TEST_RVA_EQUIPMENT_BYTE_WRITE,
        equipment_write, sizeof(equipment_write));
    memcpy(test_image + TEST_RVA_LOW_LEVEL_MUTATION,
        mutation_prefix, sizeof(mutation_prefix));
    put_u32(
        test_image + TEST_RVA_LOW_LEVEL_MUTATION +
            sizeof(mutation_prefix),
        image_base + TEST_RVA_INVENTORY_GLOBAL);
    memcpy(
        test_image + TEST_RVA_LOW_LEVEL_MUTATION +
            sizeof(mutation_prefix) + sizeof(uint32_t),
        mutation_suffix,
        sizeof(mutation_suffix));
    memcpy(test_image + TEST_RVA_LOW_LEVEL_MUTATION + 0x52u,
        mutation_calls, sizeof(mutation_calls));
    memcpy(test_image + TEST_RVA_COMPATIBILITY,
        compatibility_prefix, sizeof(compatibility_prefix));
    put_u32(
        test_image + TEST_RVA_COMPATIBILITY +
            sizeof(compatibility_prefix),
        image_base + TEST_RVA_RUNE_MANAGER_GLOBAL);
    memcpy(
        test_image + TEST_RVA_COMPATIBILITY +
            sizeof(compatibility_prefix) + sizeof(uint32_t),
        compatibility_suffix,
        sizeof(compatibility_suffix));
    memcpy(test_image + TEST_RVA_SOCKET_READ,
        socket_prefix, sizeof(socket_prefix));
    put_u32(
        test_image + TEST_RVA_SOCKET_READ + sizeof(socket_prefix),
        image_base + TEST_RVA_INVENTORY_GLOBAL);
    memcpy(
        test_image + TEST_RVA_SOCKET_READ +
            sizeof(socket_prefix) + sizeof(uint32_t),
        socket_suffix,
        sizeof(socket_suffix));
    memcpy(test_image + TEST_RVA_SOCKET_READ + 0x36u,
        socket_second_domain, sizeof(socket_second_domain));
}

static void test_exact_signature_gate(void) {
    const uint32_t preferred_base = 0x00400000u;
    const uint32_t relocated_base = 0x51000000u;

    construct_signature_image(preferred_base);
    CHECK(SudekiMpBlacksmithCommitSignaturesMatch(
        test_image, sizeof(test_image)));
    CHECK(SudekiMpBlacksmithCommitLoadedSignaturesMatch(
        test_image, sizeof(test_image), preferred_base));
    CHECK(!SudekiMpBlacksmithCommitSignaturesMatch(
        test_image, TEST_RVA_SOCKET_READ));
    test_image[TEST_RVA_SUBTRACT_MONEY + 4u] ^= 1u;
    CHECK(!SudekiMpBlacksmithCommitSignaturesMatch(
        test_image, sizeof(test_image)));

    construct_signature_image(preferred_base);
    test_image[TEST_RVA_EQUIPMENT_BYTE_WRITE + 30u] ^= 1u;
    CHECK(!SudekiMpBlacksmithCommitSignaturesMatch(
        test_image, sizeof(test_image)));

    construct_signature_image(preferred_base);
    test_image[TEST_RVA_COMPATIBILITY + 60u] ^= 1u;
    CHECK(!SudekiMpBlacksmithCommitSignaturesMatch(
        test_image, sizeof(test_image)));

    construct_signature_image(preferred_base);
    test_image[TEST_RVA_SOCKET_READ + 0x40u] ^= 1u;
    CHECK(!SudekiMpBlacksmithCommitSignaturesMatch(
        test_image, sizeof(test_image)));

    construct_signature_image(relocated_base);
    CHECK(!SudekiMpBlacksmithCommitSignaturesMatch(
        test_image, sizeof(test_image)));
    CHECK(SudekiMpBlacksmithCommitLoadedSignaturesMatch(
        test_image, sizeof(test_image), relocated_base));
    test_image[TEST_RVA_LOW_LEVEL_MUTATION + 8u] ^= 1u;
    CHECK(!SudekiMpBlacksmithCommitLoadedSignaturesMatch(
        test_image, sizeof(test_image), relocated_base));
}

static uint16_t read_u16(const uint8_t *source) {
    uint16_t value;
    memcpy(&value, source, sizeof(value));
    return value;
}

static uint32_t read_u32(const uint8_t *source) {
    uint32_t value;
    memcpy(&value, source, sizeof(value));
    return value;
}

static uint8_t *map_pe_image(
    const char *path,
    size_t *mapped_size
) {
    FILE *file;
    long file_length;
    uint8_t *raw;
    uint8_t *image;
    size_t raw_size;
    uint32_t pe_offset;
    uint16_t section_count;
    uint16_t optional_size;
    size_t optional_offset;
    size_t sections_offset;
    uint32_t image_size;
    uint32_t header_size;
    uint16_t section_index;

    if (path == NULL || mapped_size == NULL) {
        return NULL;
    }
    *mapped_size = 0u;
    file = fopen(path, "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
        if (file != NULL) fclose(file);
        return NULL;
    }
    file_length = ftell(file);
    if (file_length <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    raw_size = (size_t)file_length;
    raw = (uint8_t *)malloc(raw_size);
    if (raw == NULL || fread(raw, 1u, raw_size, file) != raw_size) {
        free(raw);
        fclose(file);
        return NULL;
    }
    fclose(file);
    if (raw_size < 0x40u || raw[0] != 'M' || raw[1] != 'Z') {
        free(raw);
        return NULL;
    }
    pe_offset = read_u32(raw + 0x3cu);
    if (pe_offset > raw_size || raw_size - pe_offset < 24u ||
        memcmp(raw + pe_offset, "PE\0\0", 4u) != 0) {
        free(raw);
        return NULL;
    }
    section_count = read_u16(raw + pe_offset + 6u);
    optional_size = read_u16(raw + pe_offset + 20u);
    optional_offset = (size_t)pe_offset + 24u;
    sections_offset = optional_offset + optional_size;
    if (optional_size < 64u || optional_offset > raw_size ||
        optional_size > raw_size - optional_offset ||
        section_count == 0u || sections_offset > raw_size ||
        (size_t)section_count > (raw_size - sections_offset) / 40u ||
        read_u16(raw + optional_offset) != 0x10bu) {
        free(raw);
        return NULL;
    }
    image_size = read_u32(raw + optional_offset + 56u);
    header_size = read_u32(raw + optional_offset + 60u);
    if (image_size == 0u || image_size > 0x10000000u) {
        free(raw);
        return NULL;
    }
    image = (uint8_t *)calloc(1u, image_size);
    if (image == NULL) {
        free(raw);
        return NULL;
    }
    if (header_size > raw_size) header_size = (uint32_t)raw_size;
    if (header_size > image_size) header_size = image_size;
    memcpy(image, raw, header_size);
    for (section_index = 0u;
         section_index < section_count;
         ++section_index) {
        const uint8_t *section = raw + sections_offset +
            (size_t)section_index * 40u;
        uint32_t destination = read_u32(section + 12u);
        uint32_t source_size = read_u32(section + 16u);
        uint32_t source = read_u32(section + 20u);
        size_t copy_size = source_size;

        if (destination >= image_size || source >= raw_size) {
            continue;
        }
        if (copy_size > raw_size - source) copy_size = raw_size - source;
        if (copy_size > image_size - destination) {
            copy_size = image_size - destination;
        }
        memcpy(image + destination, raw + source, copy_size);
    }
    free(raw);
    *mapped_size = image_size;
    return image;
}

static void test_exact_game_image(const char *path) {
    uint8_t *image;
    size_t image_size;

    image = map_pe_image(path, &image_size);
    CHECK(image != NULL);
    if (image != NULL) {
        CHECK(SudekiMpBlacksmithCommitSignaturesMatch(image, image_size));
        image[TEST_RVA_LOW_LEVEL_MUTATION] ^= 1u;
        CHECK(!SudekiMpBlacksmithCommitSignaturesMatch(image, image_size));
        free(image);
    }
}

static int fake_is_game_thread(void *context) {
    return ((FakeBackend *)context)->on_game_thread;
}

static SudekiMpBlacksmithCommitReadResult fake_resolve(
    void *context,
    const SudekiMpBlacksmithCommitTicket *ticket,
    SudekiMpBlacksmithCommitObservation *observation
) {
    FakeBackend *fake = (FakeBackend *)context;

    (void)ticket;
    ++fake->resolve_calls;
    ++fake->operation_sequence;
    if (fake->resolve_calls == fake->fail_resolve_call) {
        return SUDEKIMP_BLACKSMITH_COMMIT_READ_FAILED;
    }
    *observation = fake->observation;
    return SUDEKIMP_BLACKSMITH_COMMIT_READ_RESOLVED;
}

static SudekiMpBlacksmithLowLevelMutationResult fake_mutate(
    void *context,
    const SudekiMpBlacksmithCommitTicket *ticket
) {
    FakeBackend *fake = (FakeBackend *)context;

    ++fake->mutation_calls;
    fake->mutation_sequence = ++fake->operation_sequence;
    if (fake->mutation_mode == FAKE_MUTATION_RETURN_FALSE) {
        return SUDEKIMP_BLACKSMITH_LOW_LEVEL_MUTATION_RETURNED_FALSE;
    }
    if (fake->mutation_mode == FAKE_MUTATION_AMBIGUOUS) {
        return SUDEKIMP_BLACKSMITH_LOW_LEVEL_MUTATION_AMBIGUOUS;
    }
    if (fake->mutation_mode != FAKE_MUTATION_SUCCESS_WITHOUT_WRITE) {
        fake->observation.socket_byte =
            fake->mutation_mode == FAKE_MUTATION_WRONG_BYTE ?
                (uint8_t)(ticket->selection.component_item_id + 1u) :
                (uint8_t)ticket->selection.component_item_id;
        if (!fake->mutation_skips_inventory_generation) {
            ++fake->observation.snapshot.inventory_generation;
        }
        fake->observation.forbidden_duplicate_present = 1;
    }
    if (fake->mutation_changes_money) {
        --fake->observation.shared_money;
    }
    return fake->mutation_mode == FAKE_MUTATION_FALSE_WITH_WRITE ?
        SUDEKIMP_BLACKSMITH_LOW_LEVEL_MUTATION_RETURNED_FALSE :
        SUDEKIMP_BLACKSMITH_LOW_LEVEL_MUTATION_RETURNED_TRUE;
}

static SudekiMpBlacksmithDebitResult fake_debit(
    void *context,
    const SudekiMpBlacksmithCommitTicket *ticket,
    uint32_t amount
) {
    FakeBackend *fake = (FakeBackend *)context;

    (void)ticket;
    ++fake->debit_calls;
    fake->debit_sequence = ++fake->operation_sequence;
    if (fake->debit_mode == FAKE_DEBIT_AMBIGUOUS) {
        return SUDEKIMP_BLACKSMITH_DEBIT_AMBIGUOUS;
    }
    if (fake->debit_mode == FAKE_DEBIT_WRONG_AMOUNT) {
        --fake->observation.shared_money;
    } else {
        fake->observation.shared_money -= amount;
    }
    if (!fake->debit_skips_economy_generation) {
        ++fake->observation.snapshot.economy_generation;
    }
    if (fake->debit_changes_inventory_generation) {
        ++fake->observation.snapshot.inventory_generation;
    }
    return SUDEKIMP_BLACKSMITH_DEBIT_CALLED_ONCE;
}

static void open_player(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index
) {
    CHECK(SudekiMpBlacksmithShadowPublishPlayer(
        coordinator,
        player_index,
        100u + player_index,
        1000u + player_index,
        1
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(SudekiMpBlacksmithShadowOpen(
        coordinator,
        player_index,
        900u,
        90u
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
}

static void prepare_player(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index,
    uint32_t equipment_item_id,
    uint32_t component_item_id,
    uint32_t cost
) {
    SudekiMpBlacksmithPlayerShadow *shadow =
        &coordinator->players[player_index];

    CHECK(SudekiMpBlacksmithShadowSetBuild(
        coordinator,
        player_index,
        shadow->session_serial,
        shadow->revision,
        equipment_item_id,
        component_item_id,
        0u,
        0
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(SudekiMpBlacksmithShadowSetQuote(
        coordinator,
        player_index,
        shadow->session_serial,
        shadow->revision,
        cost
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(SudekiMpBlacksmithShadowSetConfirmation(
        coordinator,
        player_index,
        shadow->session_serial,
        shadow->revision,
        1
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
}

static SudekiMpBlacksmithCommitTicket claim_player(
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    uint32_t player_index
) {
    SudekiMpBlacksmithCommitTicket ticket;
    SudekiMpBlacksmithPlayerShadow *shadow =
        &coordinator->players[player_index];

    memset(&ticket, 0, sizeof(ticket));
    CHECK(SudekiMpBlacksmithShadowClaimCommitTicket(
        coordinator,
        player_index,
        shadow->session_serial,
        shadow->revision,
        &ticket
    ) == SUDEKIMP_BLACKSMITH_SHADOW_COMMIT_TICKET_CLAIMED);
    return ticket;
}

static void configure_observation(
    FakeBackend *fake,
    const SudekiMpBlacksmithCommitTicket *ticket
) {
    memset(fake, 0, sizeof(*fake));
    fake->on_game_thread = 1;
    fake->observation.snapshot = ticket->snapshot;
    fake->observation.character_id = ticket->character_id;
    fake->observation.actor_generation = ticket->actor_generation;
    fake->observation.actor_resolved = 1;
    fake->observation.merchant_id = ticket->merchant_id;
    fake->observation.merchant_generation = ticket->merchant_generation;
    fake->observation.merchant_resolved = 1;
    fake->observation.equipment_item_id =
        ticket->selection.equipment_item_id;
    fake->observation.equipment_socket_count = 3u;
    fake->observation.equipment_socket_bank =
        ticket->selection.socket_bank;
    fake->observation.equipment_resolved = 1;
    fake->observation.equipment_is_equipped_by_actor = 1;
    fake->observation.component_item_id =
        ticket->selection.component_item_id;
    fake->observation.component_resolved = 1;
    fake->observation.catalog_match_count = 1u;
    fake->observation.catalog_price = ticket->quoted_cost;
    fake->observation.shared_money = 100u;
    fake->observation.socket_byte = SUDEKIMP_BLACKSMITH_EMPTY_SOCKET;
    fake->observation.socket_byte_resolved = 1;
    fake->observation.equipment_storage_supported = 1;
    fake->observation.authored_component_id = -1;
    fake->observation.slot_unlocked = 1;
    fake->observation.component_compatible = 1;
}

static SudekiMpBlacksmithCommitTicket initialize_fixture_custom(
    Fixture *fixture,
    uint32_t player_count,
    uint32_t first_equipment_item_id,
    uint32_t first_component_item_id,
    uint32_t first_cost
) {
    static const SudekiMpBlacksmithSharedSnapshot initial_snapshot = {
        10u, 20u, 30u, 40u
    };
    SudekiMpBlacksmithCommitTicket ticket;
    uint32_t player_index;

    memset(fixture, 0, sizeof(*fixture));
    SudekiMpBlacksmithShadowInitialize(&fixture->coordinator);
    CHECK(SudekiMpBlacksmithShadowPublishSharedSnapshot(
        &fixture->coordinator,
        &initial_snapshot
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    for (player_index = 0u; player_index < player_count; ++player_index) {
        open_player(&fixture->coordinator, player_index);
        prepare_player(
            &fixture->coordinator,
            player_index,
            first_equipment_item_id + player_index,
            first_component_item_id + player_index,
            first_cost + player_index
        );
    }
    ticket = claim_player(&fixture->coordinator, 0u);
    configure_observation(&fixture->fake, &ticket);
    fixture->backend.context = &fixture->fake;
    fixture->backend.is_game_thread = fake_is_game_thread;
    fixture->backend.resolve = fake_resolve;
    fixture->backend.call_low_level_mutation = fake_mutate;
    fixture->backend.subtract_money_once = fake_debit;
    SudekiMpBlacksmithCommitAdapterInitialize(&fixture->adapter);
    return ticket;
}

static SudekiMpBlacksmithCommitTicket initialize_fixture_with_cost(
    Fixture *fixture,
    uint32_t player_count,
    uint32_t first_cost
) {
    return initialize_fixture_custom(
        fixture, player_count, 0u, 0u, first_cost);
}

static SudekiMpBlacksmithCommitTicket initialize_fixture(
    Fixture *fixture,
    uint32_t player_count
) {
    return initialize_fixture_with_cost(fixture, player_count, 25u);
}

static void enable(Fixture *fixture) {
    CHECK(SudekiMpBlacksmithCommitAdapterSetEnabled(
        &fixture->adapter, 1));
}

static void test_default_off_and_game_thread_gate(void) {
    Fixture fixture;

    (void)initialize_fixture(&fixture, 1u);
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_DISABLED);
    CHECK(fixture.fake.resolve_calls == 0u);
    CHECK(fixture.fake.mutation_calls == 0u);
    CHECK(fixture.fake.debit_calls == 0u);
    CHECK(fixture.coordinator.commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_CLAIMED);

    enable(&fixture);
    fixture.fake.on_game_thread = 0;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_WRONG_THREAD);
    CHECK(fixture.fake.resolve_calls == 0u);
    CHECK(fixture.coordinator.commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_CLAIMED);
}

static void test_success_orders_mutation_before_one_debit(void) {
    Fixture fixture;
    SudekiMpBlacksmithCommitTicket ticket =
        initialize_fixture(&fixture, 1u);

    enable(&fixture);
    CHECK(ticket.selection.equipment_item_id == 0u);
    CHECK(ticket.selection.component_item_id == 0u);
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_VERIFIED);
    CHECK(fixture.fake.resolve_calls == 3u);
    CHECK(fixture.fake.mutation_calls == 1u);
    CHECK(fixture.fake.debit_calls == 1u);
    CHECK(fixture.fake.mutation_sequence < fixture.fake.debit_sequence);
    CHECK(fixture.fake.observation.socket_byte == 0u);
    CHECK(fixture.fake.observation.shared_money == 75u);
    CHECK(fixture.coordinator.commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_IDLE);
    CHECK(fixture.coordinator.shared_snapshot.world_generation == 10u);
    CHECK(fixture.coordinator.shared_snapshot.catalog_generation == 20u);
    CHECK(fixture.coordinator.shared_snapshot.inventory_generation == 31u);
    CHECK(fixture.coordinator.shared_snapshot.economy_generation == 41u);
    CHECK(fixture.adapter.verified_commit_count == 1u);
}

static void test_normal_preflight_rejections_do_not_mutate(void) {
    Fixture fixture;

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.observation.catalog_match_count = 2u;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_CATALOG_DUPLICATE);
    CHECK(fixture.fake.mutation_calls == 0u);
    CHECK(fixture.fake.debit_calls == 0u);
    CHECK(fixture.coordinator.commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_IDLE);
    CHECK(fixture.coordinator.players[0].needs_refresh);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.observation.slot_unlocked = 0;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_SLOT_LOCKED);
    CHECK(fixture.fake.mutation_calls == 0u);
    CHECK(fixture.fake.debit_calls == 0u);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.observation.forbidden_duplicate_present = 1;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_DUPLICATE_COMPONENT);
    CHECK(fixture.fake.mutation_calls == 0u);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.observation.component_compatible = 0;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_INCOMPATIBLE);
    CHECK(fixture.fake.mutation_calls == 0u);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.observation.shared_money = 24u;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_INSUFFICIENT_FUNDS);
    CHECK(fixture.fake.mutation_calls == 0u);

    (void)initialize_fixture_with_cost(&fixture, 1u, 0u);
    enable(&fixture);
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_ZERO_COST_UNSUPPORTED);
    CHECK(fixture.fake.mutation_calls == 0u);
    CHECK(fixture.fake.debit_calls == 0u);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.observation.socket_byte = 7u;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_SOCKET_OCCUPIED);
    CHECK(fixture.fake.mutation_calls == 0u);
    CHECK(fixture.fake.debit_calls == 0u);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.observation.equipment_socket_count = 4u;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_INVALID_SOCKET);
    CHECK(fixture.fake.mutation_calls == 0u);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.observation.authored_component_id = 3;
    CHECK(fixture.fake.observation.slot_unlocked);
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_SLOT_LOCKED);
    CHECK(fixture.fake.mutation_calls == 0u);

    (void)initialize_fixture_custom(&fixture, 1u, 0u, 128u, 25u);
    enable(&fixture);
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_COMPONENT_UNAVAILABLE);
    CHECK(fixture.fake.mutation_calls == 0u);

    (void)initialize_fixture_custom(&fixture, 1u, 0u, 127u, 25u);
    enable(&fixture);
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_VERIFIED);
    CHECK(fixture.fake.observation.socket_byte == 127u);
    CHECK(fixture.fake.mutation_calls == 1u);
    CHECK(fixture.fake.debit_calls == 1u);
}

static void test_false_mutation_is_proven_and_never_debited(void) {
    Fixture fixture;

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.mutation_mode = FAKE_MUTATION_RETURN_FALSE;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_MUTATION_REJECTED);
    CHECK(fixture.fake.resolve_calls == 2u);
    CHECK(fixture.fake.mutation_calls == 1u);
    CHECK(fixture.fake.debit_calls == 0u);
    CHECK(fixture.fake.observation.socket_byte ==
        SUDEKIMP_BLACKSMITH_EMPTY_SOCKET);
    CHECK(fixture.fake.observation.shared_money == 100u);
    CHECK(fixture.coordinator.commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_IDLE);
}

static void check_quarantined(const Fixture *fixture) {
    CHECK(fixture->adapter.state ==
        SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_QUARANTINED);
    CHECK(fixture->coordinator.commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_QUARANTINED);
    CHECK(fixture->coordinator.players[0].state ==
        SUDEKIMP_BLACKSMITH_SHADOW_QUARANTINED);
}

static void test_all_ambiguous_paths_quarantine_without_replay(void) {
    Fixture fixture;

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.fail_resolve_call = 1u;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED);
    check_quarantined(&fixture);
    CHECK(fixture.fake.mutation_calls == 0u);
    CHECK(fixture.fake.debit_calls == 0u);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.mutation_mode = FAKE_MUTATION_WRONG_BYTE;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED);
    check_quarantined(&fixture);
    CHECK(fixture.fake.debit_calls == 0u);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.debit_mode = FAKE_DEBIT_AMBIGUOUS;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED);
    check_quarantined(&fixture);
    CHECK(fixture.fake.debit_calls == 1u);
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED);
    CHECK(fixture.fake.debit_calls == 1u);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.debit_mode = FAKE_DEBIT_WRONG_AMOUNT;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED);
    check_quarantined(&fixture);
    CHECK(fixture.fake.debit_calls == 1u);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.mutation_mode = FAKE_MUTATION_FALSE_WITH_WRITE;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED);
    check_quarantined(&fixture);
    CHECK(fixture.fake.debit_calls == 0u);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.mutation_mode = FAKE_MUTATION_SUCCESS_WITHOUT_WRITE;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED);
    check_quarantined(&fixture);
    CHECK(fixture.fake.debit_calls == 0u);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.mutation_changes_money = 1;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED);
    check_quarantined(&fixture);
    CHECK(fixture.fake.debit_calls == 0u);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.mutation_skips_inventory_generation = 1;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED);
    check_quarantined(&fixture);
    CHECK(fixture.fake.debit_calls == 0u);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.debit_skips_economy_generation = 1;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED);
    check_quarantined(&fixture);
    CHECK(fixture.fake.debit_calls == 1u);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.debit_changes_inventory_generation = 1;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED);
    check_quarantined(&fixture);
    CHECK(fixture.fake.debit_calls == 1u);
}

static void test_generation_and_identity_changes_fail_closed(void) {
    Fixture fixture;

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    ++fixture.fake.observation.snapshot.catalog_generation;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_STALE_GENERATION);
    CHECK(fixture.coordinator.commit_lane_state ==
        SUDEKIMP_BLACKSMITH_COMMIT_LANE_IDLE);
    CHECK(fixture.coordinator.shared_snapshot.catalog_generation == 21u);
    CHECK(fixture.fake.mutation_calls == 0u);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    ++fixture.fake.observation.snapshot.world_generation;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED);
    check_quarantined(&fixture);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    ++fixture.fake.observation.actor_generation;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED);
    check_quarantined(&fixture);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.observation.save_or_load_active = 1;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED);
    check_quarantined(&fixture);

    (void)initialize_fixture(&fixture, 1u);
    enable(&fixture);
    fixture.fake.observation.native_blacksmith_modal_active = 1;
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED);
    check_quarantined(&fixture);
}

static void test_first_commit_wins_and_other_seat_must_reconfirm(void) {
    Fixture fixture;
    SudekiMpBlacksmithCommitTicket ignored;
    SudekiMpBlacksmithPlayerShadow *second;

    (void)initialize_fixture(&fixture, 2u);
    second = &fixture.coordinator.players[1];
    CHECK(SudekiMpBlacksmithShadowClaimCommitTicket(
        &fixture.coordinator,
        1u,
        second->session_serial,
        second->revision,
        &ignored
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_BUSY);

    enable(&fixture);
    CHECK(SudekiMpBlacksmithCommitAdapterExecute(
        &fixture.adapter,
        &fixture.coordinator,
        &fixture.backend
    ) == SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_VERIFIED);
    CHECK(second->needs_refresh);
    CHECK(!second->quote_valid);
    CHECK(!second->confirmed);
    CHECK(SudekiMpBlacksmithShadowClaimCommitTicket(
        &fixture.coordinator,
        1u,
        second->session_serial,
        second->revision,
        &ignored
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_REFRESH_REQUIRED);
    CHECK(SudekiMpBlacksmithShadowRefresh(
        &fixture.coordinator,
        1u,
        second->session_serial,
        second->revision,
        second->merchant_id,
        second->merchant_generation
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(SudekiMpBlacksmithShadowClaimCommitTicket(
        &fixture.coordinator,
        1u,
        second->session_serial,
        second->revision,
        &ignored
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_UNQUOTED);
    CHECK(SudekiMpBlacksmithShadowSetQuote(
        &fixture.coordinator,
        1u,
        second->session_serial,
        second->revision,
        26u
    ) == SUDEKIMP_BLACKSMITH_SHADOW_APPLIED);
    CHECK(SudekiMpBlacksmithShadowClaimCommitTicket(
        &fixture.coordinator,
        1u,
        second->session_serial,
        second->revision,
        &ignored
    ) == SUDEKIMP_BLACKSMITH_SHADOW_REJECTED_UNCONFIRMED);
}

int main(int argc, char **argv) {
    test_exact_signature_gate();
    if (argc > 1) {
        test_exact_game_image(argv[1]);
    }
    test_default_off_and_game_thread_gate();
    test_success_orders_mutation_before_one_debit();
    test_normal_preflight_rejections_do_not_mutate();
    test_false_mutation_is_proven_and_never_debited();
    test_all_ambiguous_paths_quarantine_without_replay();
    test_generation_and_identity_changes_fail_closed();
    test_first_commit_wins_and_other_seat_must_reconfirm();

    if (failures != 0) {
        fprintf(stderr, "%d blacksmith commit adapter test(s) failed\n",
            failures);
        return 1;
    }
    puts("blacksmith commit adapter tests passed");
    return 0;
}
