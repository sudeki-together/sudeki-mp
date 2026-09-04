#include "hooks/lan_arena_owner_view.h"

#include <stddef.h>
#include <string.h>

enum {
    CAMERA_MODE_CAMERA_MEMBER_OFFSET = 0x0cu,
    CAMERA_MEMBER_TO_CAMERA_OFFSET = 0x2cu,
    CAMERA_RENDER_STATE_OFFSET = 0x34u,
    SCENE_MANAGER_RENDERER_OFFSET = 0x40u,
    SCENE_RENDERER_RENDER_STATE_SLOT_OFFSET = 0x7cu,
    RENDER_STATE_BASIS_OFFSET = 0x90u,
    RENDER_STATE_READ_SIZE = 0xd0u
};

static BOOL readable_memory(const void *pointer, size_t length) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t address = (uintptr_t)pointer;
    if (pointer == NULL || length == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u ||
        address + length < address ||
        address + length >
            (uintptr_t)information.BaseAddress + information.RegionSize) {
        return FALSE;
    }
    return TRUE;
}

static BOOL writable_memory(void *pointer, size_t length) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t address = (uintptr_t)pointer;
    DWORD protection;
    if (!readable_memory(pointer, length) ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        address + length < address ||
        address + length >
            (uintptr_t)information.BaseAddress + information.RegionSize) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    return protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

static BOOL resolve_owner_view(
    void *camera_mode_pointer,
    void *scene_manager_pointer,
    void **camera_out,
    void **render_state_out,
    void **scene_renderer_out,
    void ***scene_slot_out
) {
    uint8_t *camera_mode = (uint8_t *)camera_mode_pointer;
    uint8_t *scene_manager = (uint8_t *)scene_manager_pointer;
    uint8_t *camera_member;
    uint8_t *camera;
    uint8_t *render_state;
    uint8_t *scene_renderer;
    void **scene_slot;

    if (camera_out == NULL || render_state_out == NULL ||
        scene_renderer_out == NULL || scene_slot_out == NULL ||
        !readable_memory(camera_mode,
            CAMERA_MODE_CAMERA_MEMBER_OFFSET + sizeof(camera_member)) ||
        !readable_memory(scene_manager,
            SCENE_MANAGER_RENDERER_OFFSET + sizeof(scene_renderer))) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    camera_member = *(uint8_t **)(camera_mode +
        CAMERA_MODE_CAMERA_MEMBER_OFFSET);
    scene_renderer = *(uint8_t **)(scene_manager +
        SCENE_MANAGER_RENDERER_OFFSET);
    if ((uintptr_t)camera_member < CAMERA_MEMBER_TO_CAMERA_OFFSET ||
        !readable_memory(scene_renderer,
            SCENE_RENDERER_RENDER_STATE_SLOT_OFFSET + sizeof(scene_slot))) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    camera = camera_member - CAMERA_MEMBER_TO_CAMERA_OFFSET;
    scene_slot = (void **)(scene_renderer +
        SCENE_RENDERER_RENDER_STATE_SLOT_OFFSET);
    if (!readable_memory(camera,
            CAMERA_RENDER_STATE_OFFSET + sizeof(render_state)) ||
        !readable_memory(scene_slot, sizeof(*scene_slot))) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    render_state = *(uint8_t **)(camera + CAMERA_RENDER_STATE_OFFSET);
    if (!readable_memory(render_state, RENDER_STATE_READ_SIZE) ||
        *scene_slot != render_state) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    *camera_out = camera;
    *render_state_out = render_state;
    *scene_renderer_out = scene_renderer;
    *scene_slot_out = scene_slot;
    return TRUE;
}

static BOOL exact_owner_view(
    const SudekiMpLanArenaOwnerViewLease *lease,
    void *camera_mode,
    void *scene_manager,
    BOOL require_basis_write
) {
    void *camera;
    void *render_state;
    void *scene_renderer;
    void **scene_slot;
    uint8_t *basis;

    if (lease == NULL || !lease->valid ||
        camera_mode != lease->camera_mode ||
        scene_manager != lease->scene_manager ||
        !resolve_owner_view(camera_mode, scene_manager,
            &camera, &render_state, &scene_renderer, &scene_slot) ||
        camera != lease->camera || render_state != lease->render_state ||
        scene_renderer != lease->scene_renderer ||
        scene_slot != lease->scene_render_state_slot) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    basis = (uint8_t *)render_state + RENDER_STATE_BASIS_OFFSET;
    if (!readable_memory(basis, sizeof(lease->owner_basis)) ||
        (require_basis_write &&
         !writable_memory(basis, sizeof(lease->owner_basis)))) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    return TRUE;
}

void SudekiMpLanArenaOwnerViewClear(
    SudekiMpLanArenaOwnerViewLease *lease
) {
    if (lease != NULL) memset(lease, 0, sizeof(*lease));
}

BOOL SudekiMpLanArenaOwnerViewCapture(
    SudekiMpLanArenaOwnerViewLease *lease,
    void *camera_mode,
    void *scene_manager
) {
    SudekiMpLanArenaOwnerViewLease captured;
    void *camera;
    void *render_state;
    void *scene_renderer;
    void **scene_slot;

    if (lease == NULL || lease->valid) {
        SetLastError(lease != NULL ? ERROR_BUSY : ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!resolve_owner_view(camera_mode, scene_manager,
            &camera, &render_state, &scene_renderer, &scene_slot)) {
        return FALSE;
    }
    if (!writable_memory(scene_slot, sizeof(*scene_slot)) ||
        !writable_memory((uint8_t *)render_state +
            RENDER_STATE_BASIS_OFFSET, sizeof(captured.owner_basis))) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    memset(&captured, 0, sizeof(captured));
    captured.camera_mode = camera_mode;
    captured.camera = camera;
    captured.render_state = render_state;
    captured.scene_manager = scene_manager;
    captured.scene_renderer = scene_renderer;
    captured.scene_render_state_slot = scene_slot;
    memcpy(captured.owner_basis,
        (uint8_t *)render_state + RENDER_STATE_BASIS_OFFSET,
        sizeof(captured.owner_basis));
    captured.refresh_revision = 1u;
    captured.valid = TRUE;
    *lease = captured;
    return TRUE;
}

BOOL SudekiMpLanArenaOwnerViewService(
    SudekiMpLanArenaOwnerViewLease *lease,
    void *camera_mode,
    void *scene_manager,
    SudekiMpLanArenaOwnerViewBoundary boundary
) {
    uint8_t *basis;
    uint32_t next_revision;
    if (lease == NULL || !lease->valid ||
        boundary < SUDEKIMP_LAN_ARENA_OWNER_VIEW_VERIFY_BEFORE_RENDER ||
        boundary > SUDEKIMP_LAN_ARENA_OWNER_VIEW_RETIRE) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!exact_owner_view(lease, camera_mode, scene_manager,
            boundary ==
                SUDEKIMP_LAN_ARENA_OWNER_VIEW_REASSERT_AFTER_REMOTE_MUTATION ||
            boundary == SUDEKIMP_LAN_ARENA_OWNER_VIEW_RETIRE)) {
        return FALSE;
    }
    basis = (uint8_t *)lease->render_state + RENDER_STATE_BASIS_OFFSET;
    switch (boundary) {
    case SUDEKIMP_LAN_ARENA_OWNER_VIEW_VERIFY_BEFORE_RENDER:
        /* Deliberately no matrix write: local mouse input may have updated
         * this frame since the previous owner-render publication. */
        return TRUE;
    case SUDEKIMP_LAN_ARENA_OWNER_VIEW_REFRESH_AFTER_OWNER_RENDER:
        memcpy(lease->owner_basis, basis, sizeof(lease->owner_basis));
        next_revision = lease->refresh_revision + 1u;
        lease->refresh_revision = next_revision != 0u ? next_revision : 1u;
        return TRUE;
    case SUDEKIMP_LAN_ARENA_OWNER_VIEW_REASSERT_AFTER_REMOTE_MUTATION:
        memcpy(basis, lease->owner_basis, sizeof(lease->owner_basis));
        return memcmp(basis, lease->owner_basis,
            sizeof(lease->owner_basis)) == 0;
    case SUDEKIMP_LAN_ARENA_OWNER_VIEW_RETIRE:
        memcpy(basis, lease->owner_basis, sizeof(lease->owner_basis));
        if (memcmp(basis, lease->owner_basis,
                sizeof(lease->owner_basis)) != 0) {
            SetLastError(ERROR_WRITE_FAULT);
            return FALSE;
        }
        SudekiMpLanArenaOwnerViewClear(lease);
        return TRUE;
    default:
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
}
