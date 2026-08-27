#include "hooks/blacksmith_read_adapter.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

enum {
    IMAGE_SIZE = 0x00410000u,
    RVA_RUNE_MANAGER_GLOBAL = 0x00408d60u,
    RVA_ITEM_MANAGER_GLOBAL = 0x00408d80u,
    RVA_INVENTORY_GLOBAL = 0x00408d84u,
    RVA_LOCALIZATION_MANAGER_GLOBAL = 0x00409e0cu,
    RVA_BLACKSMITH_INVENTORY_GLOBAL = 0x00409dccu,
    RVA_RUNE_EFFECT_TABLE = 0x00361218u,
    RVA_INVENTORY_VTABLE = 0x002ca378u,
    RVA_ITEM_MANAGER_VTABLE = 0x002c693cu,
    RVA_BLACKSMITH_INVENTORY_VTABLE = 0x002ca474u,
    RVA_RUNE_MANAGER_VTABLE = 0x002ca48cu,
    RVA_ITEM_WEAPON_VTABLE = 0x002d3a28u,
    RVA_ITEM_WEAPON_SOCKET_BANK = 0x001307b0u,
    RVA_ITEM_WEAPON_CLASS = 0x001e8240u,
    RVA_LOCALIZATION_VTABLE = 0x002dcca4u,
    RVA_LOCALIZATION_LOOKUP = 0x001b9c00u
};

typedef union AlignedBytes {
    void *alignment;
    uint8_t bytes[0x200u];
} AlignedBytes;

static AlignedBytes inventory_object;
static AlignedBytes item_manager_object;
static AlignedBytes rune_manager_object;
static AlignedBytes catalog_object;
static AlignedBytes localization_object;
static AlignedBytes item_definition;
static AlignedBytes rune_definition;
static AlignedBytes actor_objects[2];
static AlignedBytes equipment_components[2];
static AlignedBytes category_records[2];
static AlignedBytes socket_record;
static AlignedBytes catalog_node;
static AlignedBytes catalog_payload;
static AlignedBytes localization_entry;
static void *category_pointers[2];
static void *rune_pointers[1];
static void *socket_pointers[1];
static void *localization_pointers[1];
static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static void put_pointer(void *target, const void *value) {
    memcpy(target, &value, sizeof(value));
}

static void put_u32(void *target, uint32_t value) {
    memcpy(target, &value, sizeof(value));
}

static void put_i32(void *target, int32_t value) {
    memcpy(target, &value, sizeof(value));
}

static void put_i16(void *target, int16_t value) {
    memcpy(target, &value, sizeof(value));
}

static void put_float(void *target, float value) {
    memcpy(target, &value, sizeof(value));
}

static void put_utf16(void *target, const char *text) {
    uint16_t *output = (uint16_t *)target;

    while (*text != '\0') {
        *output++ = (uint16_t)(unsigned char)*text++;
    }
    *output = 0u;
}

static void initialize_fixture(uint8_t *image) {
    static const uint8_t bank_stub[] = {
        0x31u, 0xc0u, 0xc2u, 0x04u, 0x00u
    };
    static const uint8_t class_stub[] = {
        0xb8u, 0x01u, 0x00u, 0x00u, 0x00u, 0xc3u
    };
    uint32_t player_index;

    memset(&inventory_object, 0, sizeof(inventory_object));
    memset(&item_manager_object, 0, sizeof(item_manager_object));
    memset(&rune_manager_object, 0, sizeof(rune_manager_object));
    memset(&catalog_object, 0, sizeof(catalog_object));
    memset(&localization_object, 0, sizeof(localization_object));
    memset(&item_definition, 0, sizeof(item_definition));
    memset(&rune_definition, 0, sizeof(rune_definition));
    memset(actor_objects, 0, sizeof(actor_objects));
    memset(equipment_components, 0, sizeof(equipment_components));
    memset(category_records, 0, sizeof(category_records));
    memset(&socket_record, 0, sizeof(socket_record));
    memset(&catalog_node, 0, sizeof(catalog_node));
    memset(&catalog_payload, 0, sizeof(catalog_payload));
    memset(&localization_entry, 0, sizeof(localization_entry));

    put_pointer(inventory_object.bytes, image + RVA_INVENTORY_VTABLE);
    put_pointer(item_manager_object.bytes, image + RVA_ITEM_MANAGER_VTABLE);
    put_pointer(rune_manager_object.bytes, image + RVA_RUNE_MANAGER_VTABLE);
    put_pointer(catalog_object.bytes,
        image + RVA_BLACKSMITH_INVENTORY_VTABLE);
    put_pointer(localization_object.bytes,
        image + RVA_LOCALIZATION_VTABLE);
    put_pointer(image + RVA_INVENTORY_GLOBAL, inventory_object.bytes);
    put_pointer(image + RVA_ITEM_MANAGER_GLOBAL, item_manager_object.bytes);
    put_pointer(image + RVA_RUNE_MANAGER_GLOBAL, rune_manager_object.bytes);
    put_pointer(image + RVA_BLACKSMITH_INVENTORY_GLOBAL,
        catalog_object.bytes);
    put_pointer(image + RVA_LOCALIZATION_MANAGER_GLOBAL,
        localization_object.bytes);

    put_pointer(image + RVA_ITEM_WEAPON_VTABLE + 0x30u,
        image + RVA_ITEM_WEAPON_SOCKET_BANK);
    put_pointer(image + RVA_ITEM_WEAPON_VTABLE + 0x34u,
        image + RVA_ITEM_WEAPON_CLASS);
    memcpy(image + RVA_ITEM_WEAPON_SOCKET_BANK,
        bank_stub, sizeof(bank_stub));
    memcpy(image + RVA_ITEM_WEAPON_CLASS,
        class_stub, sizeof(class_stub));
    put_pointer(image + RVA_LOCALIZATION_VTABLE + 0x10u,
        image + RVA_LOCALIZATION_LOOKUP);

    memset(inventory_object.bytes + 0x10u, 0xffu, 0x11au);
    inventory_object.bytes[0x10u] = 0u;
    category_pointers[0] = category_records[0].bytes;
    category_pointers[1] = category_records[1].bytes;
    put_pointer(inventory_object.bytes + 0x0cu, category_pointers);
    put_u32(inventory_object.bytes + 0x12cu, 2u);
    put_u32(category_records[0].bytes + 0x08u, 4u);
    put_i16(category_records[0].bytes + 0x0eu, -1);
    put_u32(category_records[1].bytes + 0x08u, 5u);
    put_i16(category_records[1].bytes + 0x0eu, -1);

    put_pointer(item_manager_object.bytes + 0x0cu,
        item_definition.bytes);
    put_pointer(item_definition.bytes, image + RVA_ITEM_WEAPON_VTABLE);
    put_u32(item_definition.bytes + 0x14u, 0u);
    put_i32(item_definition.bytes + 0x50u, 0);
    put_u32(item_definition.bytes + 0xe0u, 1u);
    socket_pointers[0] = socket_record.bytes;
    put_pointer(item_definition.bytes + 0xe8u, socket_pointers);
    put_float(item_definition.bytes + 0xecu, 0.10f);
    put_float(item_definition.bytes + 0xf0u, 2.0f);
    put_i32(item_definition.bytes + 0xf4u, 10);
    put_i32(socket_record.bytes + 0x08u, -1);

    put_u32(rune_manager_object.bytes + 0x14u, 1u);
    rune_pointers[0] = rune_definition.bytes;
    put_pointer(rune_manager_object.bytes + 0x1cu, rune_pointers);
    put_u32(rune_definition.bytes + 0x04u, 1u);
    put_u32(rune_definition.bytes + 0x08u, 0u);
    put_u32(rune_definition.bytes + 0x14u, UINT32_C(0x80000000));
    put_utf16(rune_definition.bytes + 0x18u, "POWER RUNE");
    put_u32(rune_definition.bytes + 0x94u, UINT32_C(0x80000000));
    put_utf16(rune_definition.bytes + 0x98u, "POWER");
    put_float(image + RVA_RUNE_EFFECT_TABLE + 2u * sizeof(float), 3.0f);

    put_u32(catalog_object.bytes + 0x0cu, 1u);
    put_pointer(catalog_object.bytes + 0x10u, catalog_node.bytes);
    put_pointer(catalog_object.bytes + 0x14u, catalog_node.bytes);
    put_pointer(catalog_node.bytes, catalog_payload.bytes);
    put_i32(catalog_payload.bytes, 0);
    put_i32(catalog_payload.bytes + 4u, 100);

    put_u32(localization_object.bytes + 0x08u, 1u);
    localization_pointers[0] = localization_entry.bytes;
    put_pointer(localization_object.bytes + 0x10u,
        localization_pointers);
    put_u32(localization_entry.bytes, UINT32_C(0x80000000));
    put_utf16(localization_entry.bytes + 4u, "IRON SWORD");

    for (player_index = 0u; player_index < 2u; ++player_index) {
        put_pointer(actor_objects[player_index].bytes + 0xe0u,
            equipment_components[player_index].bytes);
        put_pointer(equipment_components[player_index].bytes + 0x18u,
            item_definition.bytes);
    }
}

int main(void) {
    uint8_t *image = (uint8_t *)VirtualAlloc(
        NULL, IMAGE_SIZE, MEM_RESERVE | MEM_COMMIT,
        PAGE_EXECUTE_READWRITE);
    SudekiMpBlacksmithReadSeatRequest requests[2];
    SudekiMpBlacksmithReadSnapshot first;
    SudekiMpBlacksmithReadSnapshot second;
    uint64_t catalog_fingerprint;

    CHECK(image != NULL);
    if (image == NULL) return 1;
    initialize_fixture(image);
    memset(requests, 0, sizeof(requests));
    requests[0].actor = (uintptr_t)actor_objects[0].bytes;
    requests[0].character_id = 0x23u;
    requests[0].actor_generation = 7u;
    requests[0].active_group_proven = 1;
    requests[1].actor = (uintptr_t)actor_objects[1].bytes;
    requests[1].character_id = 0x01u;
    requests[1].actor_generation = 9u;
    requests[1].active_group_proven = 1;

    CHECK(SudekiMpBlacksmithReadAdapterInitialize((HMODULE)image));
    CHECK(SudekiMpBlacksmithReadAdapterCapture(requests, 2u, &first));
    CHECK(first.valid && first.player_count == 2u);
    CHECK(first.component_count == 1u);
    CHECK(first.components[0].component_id == 0u);
    CHECK(strcmp(first.components[0].name, "POWER RUNE") == 0);
    CHECK(first.seats[0].equipment_count == 1u);
    CHECK(first.seats[0].equipment[0].item_id == 0u);
    CHECK(strcmp(first.seats[0].equipment[0].name, "IRON SWORD") == 0);
    CHECK(first.seats[0].equipment[0].sockets[0].occupant_valid);
    CHECK(first.seats[0].equipment[0].sockets[0].occupant_component_id == 0);
    CHECK(fabsf(first.seats[0].equipment[0].primary_stat - 26.0f) < 0.001f);
    CHECK(fabsf(first.seats[0].equipment[0].secondary_percent - 10.0f) <
        0.001f);
    catalog_fingerprint = first.catalog_fingerprint;

    inventory_object.bytes[0x10u] = 0xffu;
    CHECK(SudekiMpBlacksmithReadAdapterCapture(requests, 2u, &second));
    CHECK(second.catalog_fingerprint == catalog_fingerprint);
    CHECK(second.inventory_fingerprint != first.inventory_fingerprint);
    CHECK(second.seats[0].equipment[0].sockets[0].occupant_component_id == -1);
    CHECK(fabsf(second.seats[0].equipment[0].primary_stat - 20.0f) < 0.001f);

    put_pointer(catalog_node.bytes + 8u, catalog_node.bytes);
    CHECK(!SudekiMpBlacksmithReadAdapterCapture(requests, 2u, &second));
    CHECK(!second.valid);
    put_pointer(catalog_node.bytes + 8u, NULL);
    put_u32(rune_manager_object.bytes + 0x14u, 129u);
    CHECK(!SudekiMpBlacksmithReadAdapterCapture(requests, 2u, &second));
    put_u32(rune_manager_object.bytes + 0x14u, 1u);
    put_pointer(inventory_object.bytes, image + RVA_ITEM_MANAGER_VTABLE);
    CHECK(!SudekiMpBlacksmithReadAdapterCapture(requests, 2u, &second));

    SudekiMpBlacksmithReadAdapterReset();
    CHECK(VirtualFree(image, 0u, MEM_RELEASE));
    if (failures != 0) {
        fprintf(stderr, "blacksmith read adapter checks failed: %d\n",
            failures);
        return 1;
    }
    puts("blacksmith read adapter checks passed");
    return 0;
}
