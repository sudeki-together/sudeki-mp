#include "engine/blacksmith_read_model.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static SudekiMpBlacksmithReadEquipment make_equipment(void) {
    SudekiMpBlacksmithReadEquipment equipment;

    memset(&equipment, 0, sizeof(equipment));
    equipment.item_id = 0u;
    equipment.item_class = 1u;
    equipment.socket_count = 2u;
    equipment.primary_scale = 2.0f;
    equipment.primary_stat = 24.0f;
    equipment.secondary_percent = 10.0f;
    equipment.stats_valid = 1;
    equipment.sockets[0].authored_component_id = -1;
    equipment.sockets[0].occupant_component_id = 3;
    equipment.sockets[0].occupant_effect_class = 1u;
    equipment.sockets[0].occupant_effect = 2.0f;
    equipment.sockets[0].occupant_valid = 1;
    equipment.sockets[0].bank = 0u;
    equipment.sockets[1].authored_component_id = -1;
    equipment.sockets[1].occupant_component_id = -1;
    equipment.sockets[1].bank = 1u;
    return equipment;
}

static SudekiMpBlacksmithReadComponent make_component(
    uint32_t id,
    uint32_t effect_class,
    uint32_t bank,
    float effect
) {
    SudekiMpBlacksmithReadComponent component;

    memset(&component, 0, sizeof(component));
    component.component_id = id;
    component.effect_class = effect_class;
    component.bank = bank;
    component.effect = effect;
    component.definition_valid = 1;
    component.effect_valid = 1;
    return component;
}

static void test_zero_ids_and_projection(void) {
    SudekiMpBlacksmithReadEquipment equipment = make_equipment();
    SudekiMpBlacksmithReadComponent component =
        make_component(0u, 1u, 0u, 5.0f);
    float primary;
    float secondary;

    CHECK(SudekiMpBlacksmithReadModelComponentCompatible(
        &equipment, 0u, &component));
    CHECK(SudekiMpBlacksmithReadModelProjectStats(
        &equipment, 0u, &component, &primary, &secondary));
    CHECK(fabsf(primary - 30.0f) < 0.001f);
    CHECK(fabsf(secondary - 10.0f) < 0.001f);
}

static void test_exact_compatibility_rejections(void) {
    SudekiMpBlacksmithReadEquipment equipment = make_equipment();
    SudekiMpBlacksmithReadComponent component =
        make_component(4u, 1u, 0u, 1.0f);

    equipment.sockets[0].locked = 1;
    CHECK(!SudekiMpBlacksmithReadModelComponentCompatible(
        &equipment, 0u, &component));
    equipment.sockets[0].locked = 0;
    equipment.sockets[0].authored_component_id = 2;
    CHECK(!SudekiMpBlacksmithReadModelComponentCompatible(
        &equipment, 0u, &component));
    equipment.sockets[0].authored_component_id = -1;
    component.bank = 1u;
    CHECK(!SudekiMpBlacksmithReadModelComponentCompatible(
        &equipment, 0u, &component));
    component.bank = 0u;
    component.effect_class = 2u;
    CHECK(!SudekiMpBlacksmithReadModelComponentCompatible(
        &equipment, 0u, &component));
    component.effect_class = 1u;
    component.component_id = 128u;
    CHECK(!SudekiMpBlacksmithReadModelComponentCompatible(
        &equipment, 0u, &component));
}

static void test_class_two_duplicate_rejection(void) {
    SudekiMpBlacksmithReadEquipment equipment = make_equipment();
    SudekiMpBlacksmithReadComponent component =
        make_component(7u, 2u, 1u, 0.25f);

    equipment.item_class = 2u;
    equipment.sockets[0].occupant_component_id = 7;
    equipment.sockets[0].occupant_effect_class = 2u;
    CHECK(!SudekiMpBlacksmithReadModelComponentCompatible(
        &equipment, 1u, &component));
    equipment.sockets[0].occupant_component_id = 6;
    CHECK(SudekiMpBlacksmithReadModelComponentCompatible(
        &equipment, 1u, &component));
}

int main(void) {
    test_zero_ids_and_projection();
    test_exact_compatibility_rejections();
    test_class_two_duplicate_rejection();
    if (failures != 0) {
        fprintf(stderr, "blacksmith read model checks failed: %d\n",
            failures);
        return 1;
    }
    puts("blacksmith read model checks passed");
    return 0;
}
