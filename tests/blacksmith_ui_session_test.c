#include "engine/blacksmith_ui_session.h"

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

static SudekiMpBlacksmithReadSnapshot make_read_model(
    const uint32_t characters[2],
    const uint32_t generations[2]
) {
    SudekiMpBlacksmithReadSnapshot model;
    uint32_t player_index;
    uint32_t item_index;
    uint32_t component_index;

    memset(&model, 0, sizeof(model));
    model.player_count = 2u;
    model.component_count = 8u;
    model.catalog_fingerprint = UINT64_C(0x1111222233334444);
    model.inventory_fingerprint = UINT64_C(0xaaaabbbbccccdddd);
    model.valid = 1;
    for (component_index = 0u;
         component_index < model.component_count; ++component_index) {
        SudekiMpBlacksmithReadComponent *component =
            &model.components[component_index];
        component->component_id = component_index;
        component->price = (int32_t)(100u + component_index * 25u);
        component->kind = 1u;
        component->bank = 1u;
        component->effect_class = 1u;
        component->effect = 2.0f + (float)component_index;
        component->definition_valid = 1;
        component->effect_valid = 1;
        snprintf(component->name, sizeof(component->name),
            "RUNE %lu", (unsigned long)component_index);
        strcpy(component->effect_name, "POWER");
    }
    for (player_index = 0u; player_index < 2u; ++player_index) {
        SudekiMpBlacksmithReadSeat *seat = &model.seats[player_index];
        seat->character_id = characters[player_index];
        seat->actor_generation = generations[player_index];
        seat->equipment_count = player_index == 0u ? 8u : 7u;
        seat->equipped_index = 0u;
        seat->valid = 1;
        for (item_index = 0u;
             item_index < seat->equipment_count; ++item_index) {
            SudekiMpBlacksmithReadEquipment *equipment =
                &seat->equipment[item_index];
            equipment->item_id = player_index * 100u + item_index;
            equipment->category_id = 4u + player_index;
            equipment->item_class = 1u;
            equipment->socket_count = 2u;
            equipment->base_stat = 10.0f;
            equipment->primary_scale = 2.0f;
            equipment->primary_stat = 20.0f;
            equipment->secondary_percent = 5.0f;
            equipment->equipped = item_index == 0u;
            equipment->stats_valid = 1;
            snprintf(equipment->name, sizeof(equipment->name),
                "ITEM %lu", (unsigned long)equipment->item_id);
            equipment->sockets[0].authored_component_id = -1;
            equipment->sockets[0].occupant_component_id = -1;
            equipment->sockets[0].bank = 1u;
            strcpy(equipment->sockets[0].occupant_name, "EMPTY");
            equipment->sockets[1] = equipment->sockets[0];
        }
    }
    return model;
}

static void test_exact_start_gate(void) {
    enum {
        IMAGE_SIZE = 0x0030d41cu,
        RVA_START = 0x00092c40u,
        RVA_ACTIVE = 0x00092c60u,
        RVA_WORLD_SCENE_GLOBAL = 0x00408d1cu,
        RVA_BLACKSMITH_LAYER_GLOBAL = 0x003c2f74u,
        RVA_ACTIVE_EXPORT = 0x0030d414u,
        RVA_START_EXPORT = 0x0030d418u
    };
    static const unsigned char start[] = {
        0xa1, 0x1c, 0x8d, 0x80, 0x00, 0x85, 0xc0
    };
    static const unsigned char active[] = {
        0xa1, 0x74, 0x2f, 0x7c, 0x00, 0x85, 0xc0
    };
    unsigned char *image = (unsigned char *)calloc(1u, IMAGE_SIZE);
    uint32_t value;

    CHECK(image != NULL);
    if (image == NULL) return;
    memcpy(image + RVA_START, start, sizeof(start));
    memcpy(image + RVA_ACTIVE, active, sizeof(active));
    value = RVA_ACTIVE;
    memcpy(image + RVA_ACTIVE_EXPORT, &value, sizeof(value));
    value = RVA_START;
    memcpy(image + RVA_START_EXPORT, &value, sizeof(value));
    CHECK(SudekiMpBlacksmithUiStartSignaturesMatch(image, IMAGE_SIZE));
    image[RVA_START + 1u] ^= 0xffu;
    CHECK(!SudekiMpBlacksmithUiStartSignaturesMatch(image, IMAGE_SIZE));
    image[RVA_START + 1u] ^= 0xffu;
    value = RVA_START + 1u;
    memcpy(image + RVA_START_EXPORT, &value, sizeof(value));
    CHECK(!SudekiMpBlacksmithUiStartSignaturesMatch(image, IMAGE_SIZE));
    CHECK(!SudekiMpBlacksmithUiStartSignaturesMatch(
        image, RVA_START_EXPORT));

    /* A loaded ASLR image has relocated A1 operands but unchanged EAT RVAs
     * and stable instruction tails. */
    value = (uint32_t)(uintptr_t)(image + RVA_WORLD_SCENE_GLOBAL);
    memcpy(image + RVA_START + 1u, &value, sizeof(value));
    value = (uint32_t)(uintptr_t)(image + RVA_BLACKSMITH_LAYER_GLOBAL);
    memcpy(image + RVA_ACTIVE + 1u, &value, sizeof(value));
    value = RVA_START;
    memcpy(image + RVA_START_EXPORT, &value, sizeof(value));
    CHECK(SudekiMpBlacksmithUiLoadedStartSignaturesMatch(
        image, IMAGE_SIZE, (uintptr_t)image));
    CHECK(!SudekiMpBlacksmithUiStartSignaturesMatch(image, IMAGE_SIZE));
    image[RVA_ACTIVE + 6u] ^= 0x01u;
    CHECK(!SudekiMpBlacksmithUiLoadedStartSignaturesMatch(
        image, IMAGE_SIZE, (uintptr_t)image));
    free(image);
}

static void test_independent_seat_navigation_and_close(void) {
    SudekiMpBlacksmithUiSession session;
    SudekiMpBlacksmithUiSnapshot snapshot;
    const uint32_t characters[2] = {0x23u, 0x01u};
    const uint32_t generations[2] = {7u, 9u};
    SudekiMpBlacksmithReadSnapshot model =
        make_read_model(characters, generations);
    uint32_t p2_revision;

    SudekiMpBlacksmithUiSessionInitialize(&session);
    CHECK(SudekiMpBlacksmithUiSessionBegin(
        &session, characters, generations, 1234u, &model, 100u));
    CHECK(SudekiMpBlacksmithUiSessionGetSnapshot(&session, &snapshot));
    CHECK(snapshot.active && snapshot.seats[0].open &&
        snapshot.seats[1].open);
    CHECK(snapshot.shared_money == 1234u &&
        snapshot.shared_money_valid);
    CHECK(!snapshot.merchant_target_resolved &&
        !snapshot.native_commit_enabled);

    p2_revision = snapshot.seats[1].revision;
    CHECK(SudekiMpBlacksmithUiSessionApplyInput(
        &session, 0u, SUDEKIMP_BLACKSMITH_UI_INPUT_DOWN));
    CHECK(SudekiMpBlacksmithUiSessionApplyInput(
        &session, 0u, SUDEKIMP_BLACKSMITH_UI_INPUT_RIGHT));
    CHECK(SudekiMpBlacksmithUiSessionApplyInput(
        &session, 0u, SUDEKIMP_BLACKSMITH_UI_INPUT_NEXT_PAGE));
    CHECK(SudekiMpBlacksmithUiSessionGetSnapshot(&session, &snapshot));
    CHECK(snapshot.seats[0].cursor == 0u &&
        snapshot.seats[0].category == 0u &&
        snapshot.seats[0].page == 1u);
    CHECK(snapshot.seats[1].cursor == 0u &&
        snapshot.seats[1].category == 0u &&
        snapshot.seats[1].page == 0u &&
        snapshot.seats[1].revision == p2_revision);

    CHECK(SudekiMpBlacksmithUiSessionApplyInput(
        &session, 1u, SUDEKIMP_BLACKSMITH_UI_INPUT_UP));
    CHECK(SudekiMpBlacksmithUiSessionApplyInput(
        &session, 1u, SUDEKIMP_BLACKSMITH_UI_INPUT_LEFT));
    CHECK(SudekiMpBlacksmithUiSessionGetSnapshot(&session, &snapshot));
    CHECK(snapshot.seats[1].cursor == 0u &&
        snapshot.seats[1].category == 1u);
    CHECK(snapshot.seats[0].cursor == 0u);

    CHECK(SudekiMpBlacksmithUiSessionApplyInput(
        &session, 0u, SUDEKIMP_BLACKSMITH_UI_INPUT_PREVIEW));
    CHECK(SudekiMpBlacksmithUiSessionGetSnapshot(&session, &snapshot));
    CHECK(snapshot.seats[0].notice ==
        SUDEKIMP_BLACKSMITH_UI_NOTICE_COMMIT_DISABLED);
    CHECK(!snapshot.native_commit_enabled);

    CHECK(SudekiMpBlacksmithUiSessionApplyInput(
        &session, 0u, SUDEKIMP_BLACKSMITH_UI_INPUT_CLOSE));
    CHECK(SudekiMpBlacksmithUiSessionGetSnapshot(&session, &snapshot));
    CHECK(!snapshot.seats[0].open && snapshot.seats[1].open);
    CHECK(SudekiMpBlacksmithUiSessionApplyInput(
        &session, 1u, SUDEKIMP_BLACKSMITH_UI_INPUT_CLOSE));
    CHECK(!SudekiMpBlacksmithUiSessionGetSnapshot(&session, &snapshot));
    CHECK(!snapshot.active);
}

static void test_money_refresh_and_overlay_fail_safe(void) {
    SudekiMpBlacksmithUiSession session;
    SudekiMpBlacksmithUiSnapshot snapshot;
    const uint32_t characters[2] = {0x23u, 0x01u};
    const uint32_t generations[2] = {11u, 12u};
    SudekiMpBlacksmithReadSnapshot model =
        make_read_model(characters, generations);
    uint32_t p1_revision;
    uint32_t p2_revision;

    SudekiMpBlacksmithUiSessionInitialize(&session);
    CHECK(SudekiMpBlacksmithUiSessionBegin(
        &session, characters, generations, 500u, &model, 200u));
    CHECK(SudekiMpBlacksmithUiSessionGetSnapshot(&session, &snapshot));
    p1_revision = snapshot.seats[0].revision;
    p2_revision = snapshot.seats[1].revision;
    CHECK(SudekiMpBlacksmithUiSessionObserveMoney(&session, 450u));
    CHECK(SudekiMpBlacksmithUiSessionGetSnapshot(&session, &snapshot));
    CHECK(snapshot.shared_money == 450u);
    CHECK(snapshot.economy_generation == 2u);
    CHECK(snapshot.seats[0].revision > p1_revision);
    CHECK(snapshot.seats[1].revision > p2_revision);
    CHECK(!SudekiMpBlacksmithUiSessionService(
        &session, 1200u));

    SudekiMpBlacksmithUiSessionInitialize(&session);
    CHECK(SudekiMpBlacksmithUiSessionBegin(
        &session, characters, generations, 500u, &model, 500u));
    SudekiMpBlacksmithUiSessionReportOverlay(&session, 1);
    CHECK(SudekiMpBlacksmithUiSessionService(&session, 5000u));
    CHECK(SudekiMpBlacksmithUiSessionDropPlayer(&session, 1u));
    CHECK(SudekiMpBlacksmithUiSessionGetSnapshot(&session, &snapshot));
    CHECK(snapshot.seats[0].open && !snapshot.seats[1].open);
}

static void test_read_model_generations_and_stable_selection(void) {
    SudekiMpBlacksmithUiSession session;
    SudekiMpBlacksmithUiSnapshot snapshot;
    const uint32_t characters[2] = {0x23u, 0x01u};
    const uint32_t generations[2] = {21u, 22u};
    SudekiMpBlacksmithReadSnapshot model =
        make_read_model(characters, generations);
    SudekiMpBlacksmithReadEquipment equipment_swap;
    SudekiMpBlacksmithReadComponent component_swap;
    uint32_t p1_revision;
    uint32_t p2_revision;

    SudekiMpBlacksmithUiSessionInitialize(&session);
    CHECK(SudekiMpBlacksmithUiSessionBegin(
        &session, characters, generations, 900u, &model, 1000u));
    CHECK(SudekiMpBlacksmithUiSessionApplyInput(
        &session, 0u, SUDEKIMP_BLACKSMITH_UI_INPUT_DOWN));
    CHECK(SudekiMpBlacksmithUiSessionGetSnapshot(&session, &snapshot));
    CHECK(snapshot.seats[0].selected_equipment_index == 1u);
    p1_revision = snapshot.seats[0].revision;
    p2_revision = snapshot.seats[1].revision;

    equipment_swap = model.seats[0].equipment[0];
    model.seats[0].equipment[0] = model.seats[0].equipment[1];
    model.seats[0].equipment[1] = equipment_swap;
    model.seats[0].equipped_index = 1u;
    model.inventory_fingerprint++;
    CHECK(SudekiMpBlacksmithUiSessionObserveReadModel(&session, &model));
    CHECK(SudekiMpBlacksmithUiSessionGetSnapshot(&session, &snapshot));
    CHECK(snapshot.inventory_generation == 2u);
    CHECK(snapshot.seats[0].selected_equipment_index == 0u);
    CHECK(snapshot.seats[0].cursor == 0u);
    CHECK(snapshot.seats[0].revision > p1_revision);
    CHECK(snapshot.seats[1].revision > p2_revision);

    CHECK(SudekiMpBlacksmithUiSessionApplyInput(
        &session, 0u, SUDEKIMP_BLACKSMITH_UI_INPUT_NEXT_PAGE));
    CHECK(SudekiMpBlacksmithUiSessionApplyInput(
        &session, 0u, SUDEKIMP_BLACKSMITH_UI_INPUT_NEXT_PAGE));
    CHECK(SudekiMpBlacksmithUiSessionApplyInput(
        &session, 0u, SUDEKIMP_BLACKSMITH_UI_INPUT_DOWN));
    component_swap = model.components[0];
    model.components[0] = model.components[1];
    model.components[1] = component_swap;
    model.catalog_fingerprint++;
    CHECK(SudekiMpBlacksmithUiSessionObserveReadModel(&session, &model));
    CHECK(SudekiMpBlacksmithUiSessionGetSnapshot(&session, &snapshot));
    CHECK(snapshot.catalog_generation == 2u);
    CHECK(snapshot.seats[0].selected_component_index == 0u);
    CHECK(snapshot.seats[0].page == 2u &&
        snapshot.seats[0].category == 0u &&
        snapshot.seats[0].cursor == 0u);

    model.seats[0].equipment[0] = model.seats[0].equipment[1];
    model.seats[0].equipment_count = 1u;
    model.seats[0].equipped_index = 0u;
    model.inventory_fingerprint++;
    CHECK(SudekiMpBlacksmithUiSessionObserveReadModel(&session, &model));
    CHECK(SudekiMpBlacksmithUiSessionGetSnapshot(&session, &snapshot));
    CHECK(snapshot.seats[0].selected_equipment_index == 0u);
    CHECK(snapshot.seats[0].selected_socket_index == 0u);
    CHECK(snapshot.seats[0].selected_component_index == 0u);
}

int main(void) {
    test_exact_start_gate();
    test_independent_seat_navigation_and_close();
    test_money_refresh_and_overlay_fail_safe();
    test_read_model_generations_and_stable_selection();
    if (failures != 0) {
        fprintf(stderr, "blacksmith UI session checks failed: %d\n", failures);
        return 1;
    }
    puts("blacksmith UI session checks passed");
    return 0;
}
