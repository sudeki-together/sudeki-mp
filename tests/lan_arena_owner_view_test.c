#include "hooks/lan_arena_owner_view.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct OwnerViewFixture {
    uint8_t camera_mode[0x10];
    uint8_t camera[0x38];
    uint8_t scene_manager[0x44];
    uint8_t scene_renderer[0x80];
    uint8_t render_state[0xd0];
} OwnerViewFixture;

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s (error=%lu)\n", \
            __FILE__, __LINE__, #condition, (unsigned long)GetLastError()); \
        ++failures; \
    } \
} while (0)

static float *basis(OwnerViewFixture *fixture) {
    return (float *)(fixture->render_state + 0x90u);
}

static float *translation(OwnerViewFixture *fixture) {
    return (float *)(fixture->render_state + 0xc0u);
}

static void set_basis(OwnerViewFixture *fixture, float seed) {
    unsigned int index;
    for (index = 0u; index < 12u; ++index) {
        basis(fixture)[index] = seed + (float)index;
    }
}

static int basis_equals(const OwnerViewFixture *fixture, float seed) {
    const float *values = (const float *)(fixture->render_state + 0x90u);
    unsigned int index;
    for (index = 0u; index < 12u; ++index) {
        if (values[index] != seed + (float)index) return 0;
    }
    return 1;
}

static void prepare_fixture(OwnerViewFixture *fixture) {
    void *camera_member;
    void *camera_render_state;
    void *scene_renderer;
    void *scene_slot_value;
    memset(fixture, 0, sizeof(*fixture));
    camera_member = fixture->camera + 0x2cu;
    camera_render_state = fixture->render_state;
    scene_renderer = fixture->scene_renderer;
    scene_slot_value = fixture->render_state;
    memcpy(fixture->camera_mode + 0x0cu,
        &camera_member, sizeof(camera_member));
    memcpy(fixture->camera + 0x34u,
        &camera_render_state, sizeof(camera_render_state));
    memcpy(fixture->scene_manager + 0x40u,
        &scene_renderer, sizeof(scene_renderer));
    memcpy(fixture->scene_renderer + 0x7cu,
        &scene_slot_value, sizeof(scene_slot_value));
    set_basis(fixture, 10.0f);
    translation(fixture)[0] = 101.0f;
    translation(fixture)[1] = 102.0f;
    translation(fixture)[2] = 103.0f;
}

static void test_first_render_refresh_survives_between_render_mutation(void) {
    OwnerViewFixture fixture;
    SudekiMpLanArenaOwnerViewLease lease;
    uint32_t revision;
    memset(&lease, 0, sizeof(lease));
    prepare_fixture(&fixture);

    CHECK(SudekiMpLanArenaOwnerViewCapture(
        &lease, fixture.camera_mode, fixture.scene_manager));
    CHECK(lease.valid && lease.refresh_revision == 1u);

    /* Input rotates the real local camera before RenderStart. VERIFY must not
     * copy the previous frame's basis back over that fresh mouse movement. */
    set_basis(&fixture, 20.0f);
    CHECK(SudekiMpLanArenaOwnerViewService(
        &lease, fixture.camera_mode, fixture.scene_manager,
        SUDEKIMP_LAN_ARENA_OWNER_VIEW_VERIFY_BEFORE_RENDER));
    CHECK(basis_equals(&fixture, 20.0f));

    /* Native RenderStart is the trusted local-owner publication boundary. */
    CHECK(SudekiMpLanArenaOwnerViewService(
        &lease, fixture.camera_mode, fixture.scene_manager,
        SUDEKIMP_LAN_ARENA_OWNER_VIEW_REFRESH_AFTER_OWNER_RENDER));
    revision = lease.refresh_revision;
    CHECK(revision == 2u);

    /* The primary component/CSkill update occurs between first and second
     * RenderStart. Simulate that mutation and deliberately DO NOT refresh at
     * the second call: adopting 30 here would turn restoration into a no-op. */
    set_basis(&fixture, 30.0f);
    translation(&fixture)[0] = 201.0f;
    CHECK(lease.owner_basis[0] == 20.0f);
    CHECK(SudekiMpLanArenaOwnerViewService(
        &lease, fixture.camera_mode, fixture.scene_manager,
        SUDEKIMP_LAN_ARENA_OWNER_VIEW_REASSERT_AFTER_REMOTE_MUTATION));
    CHECK(basis_equals(&fixture, 20.0f));
    CHECK(translation(&fixture)[0] == 201.0f);

    /* A later mouse rotation becomes the new lease instead of being pinned
     * to either the activation basis or the preceding rendered frame. */
    set_basis(&fixture, 40.0f);
    CHECK(SudekiMpLanArenaOwnerViewService(
        &lease, fixture.camera_mode, fixture.scene_manager,
        SUDEKIMP_LAN_ARENA_OWNER_VIEW_REFRESH_AFTER_OWNER_RENDER));
    set_basis(&fixture, 50.0f);
    CHECK(SudekiMpLanArenaOwnerViewService(
        &lease, fixture.camera_mode, fixture.scene_manager,
        SUDEKIMP_LAN_ARENA_OWNER_VIEW_REASSERT_AFTER_REMOTE_MUTATION));
    CHECK(basis_equals(&fixture, 40.0f));
    CHECK(lease.refresh_revision == revision + 1u);
}

static void test_exact_slot_rejection_is_retryable(void) {
    OwnerViewFixture fixture;
    SudekiMpLanArenaOwnerViewLease lease;
    void *foreign = (void *)(uintptr_t)0x12345678u;
    void *owner;
    memset(&lease, 0, sizeof(lease));
    prepare_fixture(&fixture);
    CHECK(SudekiMpLanArenaOwnerViewCapture(
        &lease, fixture.camera_mode, fixture.scene_manager));
    set_basis(&fixture, 60.0f);
    memcpy(fixture.scene_renderer + 0x7cu, &foreign, sizeof(foreign));
    CHECK(!SudekiMpLanArenaOwnerViewService(
        &lease, fixture.camera_mode, fixture.scene_manager,
        SUDEKIMP_LAN_ARENA_OWNER_VIEW_RETIRE));
    CHECK(lease.valid);
    CHECK(basis_equals(&fixture, 60.0f));

    owner = fixture.render_state;
    memcpy(fixture.scene_renderer + 0x7cu, &owner, sizeof(owner));
    CHECK(SudekiMpLanArenaOwnerViewService(
        &lease, fixture.camera_mode, fixture.scene_manager,
        SUDEKIMP_LAN_ARENA_OWNER_VIEW_RETIRE));
    CHECK(!lease.valid);
    CHECK(basis_equals(&fixture, 10.0f));
}

static void test_camera_identity_change_never_overwrites_foreign_view(void) {
    OwnerViewFixture fixture;
    SudekiMpLanArenaOwnerViewLease lease;
    void *foreign_member;
    void *owner_member;
    memset(&lease, 0, sizeof(lease));
    prepare_fixture(&fixture);
    CHECK(SudekiMpLanArenaOwnerViewCapture(
        &lease, fixture.camera_mode, fixture.scene_manager));
    set_basis(&fixture, 70.0f);
    foreign_member = fixture.camera + 0x2du;
    memcpy(fixture.camera_mode + 0x0cu,
        &foreign_member, sizeof(foreign_member));
    CHECK(!SudekiMpLanArenaOwnerViewService(
        &lease, fixture.camera_mode, fixture.scene_manager,
        SUDEKIMP_LAN_ARENA_OWNER_VIEW_REASSERT_AFTER_REMOTE_MUTATION));
    CHECK(lease.valid && basis_equals(&fixture, 70.0f));

    owner_member = fixture.camera + 0x2cu;
    memcpy(fixture.camera_mode + 0x0cu,
        &owner_member, sizeof(owner_member));
    CHECK(SudekiMpLanArenaOwnerViewService(
        &lease, fixture.camera_mode, fixture.scene_manager,
        SUDEKIMP_LAN_ARENA_OWNER_VIEW_RETIRE));
    CHECK(!lease.valid && basis_equals(&fixture, 10.0f));
}

int main(void) {
    test_first_render_refresh_survives_between_render_mutation();
    test_exact_slot_rejection_is_retryable();
    test_camera_identity_change_never_overwrites_foreign_view();
    if (failures != 0) {
        fprintf(stderr, "%d LAN arena owner-view test(s) failed\n", failures);
        return 1;
    }
    puts("LAN arena owner-view tests passed");
    return 0;
}
