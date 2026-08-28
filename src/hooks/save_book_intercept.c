#include "hooks/save_book_intercept.h"

#include "engine/log.h"
#include "engine/player_statehood.h"
#include "engine/save_book_vote.h"
#include "hooks/call_hook.h"
#include "hooks/control_separation.h"
#include "hooks/split_screen_render.h"
#include "hooks/zone_transition_trace.h"
#include "input/bridge_receiver.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Save-book interception requires the 32-bit Windows target"
#endif

typedef void (__attribute__((cdecl)) *SaveMenuShowFunction)(void);
typedef void (__attribute__((cdecl)) *LoadGameSaveFunction)(int save_slot);

enum {
    RVA_SAVE_MENU_SHOW = 0x00084f10u,
    RVA_LOAD_GAME_SAVE = 0x00101690u,
    RVA_UI_SCENE_GLOBAL = 0x00408d1cu,
    RVA_INGAME_UI_CONTROLLER_GLOBAL = 0x003c2f88u,
    RVA_INGAME_UI_CONTROLLER_VTABLE = 0x002caf9cu,
    RVA_INGAME_UI_CONTROLLER_INPUT = 0x0009c930u,
    UI_SCENE_EVENT_RECEIVER_OFFSET = 0x170u,
    UI_EVENT_VTABLE_SLOT_OFFSET = 0x2cu,
    INGAME_UI_CONTROLLER_CURRENT_MODE_OFFSET = 0xb8u,
    INGAME_UI_CONTROLLER_NEXT_MODE_OFFSET = 0xbcu,
    INGAME_UI_CONTROLLER_GAMEPLAY_MODE = 1u,
    SAVE_MENU_SHOW_HOOK_LENGTH = 5u,
    LOAD_GAME_SAVE_HOOK_LENGTH = 7u
};

static const uint8_t save_menu_show_stable_tail[] = {
    0x85u, 0xc0u, 0x74u, 0x17u,
    0x8bu, 0x88u, 0x70u, 0x01u, 0x00u, 0x00u,
    0x85u, 0xc9u, 0x74u, 0x0du,
    0x8bu, 0x01u, 0x8bu, 0x50u, 0x2cu,
    0x6au, 0x00u, 0x6au, 0x00u, 0x6au, 0x1bu,
    0xffu, 0xd2u, 0xc3u
};

typedef struct SudekiMpSaveUiAuthority {
    void *scene;
    void *event_receiver;
    void *event_receiver_vtable;
    void *event_function;
    void *controller;
    unsigned int current_mode;
    unsigned int next_mode;
} SudekiMpSaveUiAuthority;

typedef struct SudekiMpPendingSaveBook {
    BOOL valid;
    int save_slot;
    DWORD game_thread_id;
    uint32_t source_generation;
    uintptr_t world_identity;
    SudekiMpSaveUiAuthority ui_authority;
    SudekiMpPlayerLease participant_leases[
        SUDEKIMP_SAVE_BOOK_VOTE_MAX_PLAYERS];
} SudekiMpPendingSaveBook;

static uint8_t *game_base;
static BOOL save_book_intercept_enabled;
static BOOL repeated_request_logged;
static SudekiMpInlineHook save_menu_show_hook;
static SaveMenuShowFunction original_save_menu_show;
static SudekiMpInlineHook load_game_save_hook;
void *load_game_save_trampoline;
static LoadGameSaveFunction original_load_game_save;
static SudekiMpSaveBookVote save_book_vote;
static SudekiMpPendingSaveBook pending_save_book;
static SudekiMpSaveBookNativeLifecycle native_save_lifecycle;
static BOOL native_save_lifecycle_active;
static BOOL native_save_split_latch_armed;
static BOOL save_book_owns_gameplay_suppression;
static DWORD native_save_game_thread_id;
static void *native_save_controller_identity;
static uint32_t native_save_serial;
static BOOL native_save_source_snapshot_valid;
static uint32_t native_save_source_generation;
static uintptr_t native_save_world_identity;

static BOOL readable_memory(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    if (size > UINTPTR_MAX - start) {
        return FALSE;
    }
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress +
        information.RegionSize;
    return end <= region_end;
}

static BOOL inspect_ui_controller(
    void **identity,
    unsigned int *current_mode,
    unsigned int *next_mode
) {
    uint8_t *controller;

    if (game_base == NULL || identity == NULL || current_mode == NULL ||
        next_mode == NULL ||
        !readable_memory(
            game_base + RVA_INGAME_UI_CONTROLLER_GLOBAL,
            sizeof(controller))) {
        return FALSE;
    }
    controller = *(uint8_t **)(
        game_base + RVA_INGAME_UI_CONTROLLER_GLOBAL);
    if (!readable_memory(
            controller,
            INGAME_UI_CONTROLLER_NEXT_MODE_OFFSET +
                sizeof(unsigned int)) ||
        *(void **)controller !=
            game_base + RVA_INGAME_UI_CONTROLLER_VTABLE) {
        return FALSE;
    }
    *identity = controller;
    *current_mode = *(unsigned int *)(
        controller + INGAME_UI_CONTROLLER_CURRENT_MODE_OFFSET);
    *next_mode = *(unsigned int *)(
        controller + INGAME_UI_CONTROLLER_NEXT_MODE_OFFSET);
    return TRUE;
}

static BOOL capture_ui_authority(SudekiMpSaveUiAuthority *authority) {
    uint8_t *scene;
    uint8_t *receiver;
    uint8_t *receiver_vtable;

    if (authority == NULL || game_base == NULL ||
        !readable_memory(game_base + RVA_UI_SCENE_GLOBAL, sizeof(scene))) {
        return FALSE;
    }
    ZeroMemory(authority, sizeof(*authority));
    if (!inspect_ui_controller(
            &authority->controller,
            &authority->current_mode,
            &authority->next_mode)) {
        return FALSE;
    }
    scene = *(uint8_t **)(game_base + RVA_UI_SCENE_GLOBAL);
    if (!readable_memory(
            scene,
            UI_SCENE_EVENT_RECEIVER_OFFSET + sizeof(receiver))) {
        return FALSE;
    }
    receiver = *(uint8_t **)(scene + UI_SCENE_EVENT_RECEIVER_OFFSET);
    if (receiver != authority->controller ||
        !readable_memory(receiver, sizeof(receiver_vtable))) {
        return FALSE;
    }
    receiver_vtable = *(uint8_t **)receiver;
    if (receiver_vtable != game_base + RVA_INGAME_UI_CONTROLLER_VTABLE ||
        !readable_memory(
            receiver_vtable,
            UI_EVENT_VTABLE_SLOT_OFFSET + sizeof(void *)) ||
        *(void **)(receiver_vtable + UI_EVENT_VTABLE_SLOT_OFFSET) !=
            game_base + RVA_INGAME_UI_CONTROLLER_INPUT) {
        return FALSE;
    }
    authority->scene = scene;
    authority->event_receiver = receiver;
    authority->event_receiver_vtable = receiver_vtable;
    authority->event_function = *(void **)(
        receiver_vtable + UI_EVENT_VTABLE_SLOT_OFFSET);
    return TRUE;
}

static BOOL ui_authority_matches(
    const SudekiMpSaveUiAuthority *retained
) {
    SudekiMpSaveUiAuthority current;

    return retained != NULL && capture_ui_authority(&current) &&
        current.scene == retained->scene &&
        current.event_receiver == retained->event_receiver &&
        current.event_receiver_vtable == retained->event_receiver_vtable &&
        current.event_function == retained->event_function &&
        current.controller == retained->controller &&
        current.current_mode == retained->current_mode &&
        current.next_mode == retained->next_mode;
}

static uint8_t active_human_mask(void) {
    const SudekiMpPlayerStatehood *statehood =
        SudekiMpPlayerStatehoodRuntime();
    uint8_t mask = 0u;
    uint32_t index;

    if (statehood == NULL) {
        return 0u;
    }
    for (index = 0u;
         index < SUDEKIMP_SAVE_BOOK_VOTE_MAX_PLAYERS;
         ++index) {
        const SudekiMpPlayerLease *lease = &statehood->players[index];

        if (lease->human_present && lease->actor != 0u &&
            lease->actor_generation != 0u &&
            (index != 1u ||
             (SudekiMpControlSeparationPlayerTwoRequested() &&
              SudekiMpControlSeparationPlayerTwoActive() &&
              SudekiMpControlSeparationInputReady()))) {
            mask |= (uint8_t)(1u << index);
        }
    }
    return mask;
}

static BOOL capture_participant_leases(
    uint8_t participant_mask,
    SudekiMpPlayerLease leases[SUDEKIMP_SAVE_BOOK_VOTE_MAX_PLAYERS]
) {
    const SudekiMpPlayerStatehood *statehood =
        SudekiMpPlayerStatehoodRuntime();
    uint32_t index;

    if (statehood == NULL || leases == NULL ||
        (participant_mask & 0x01u) == 0u) {
        return FALSE;
    }
    ZeroMemory(leases, sizeof(*leases) *
        SUDEKIMP_SAVE_BOOK_VOTE_MAX_PLAYERS);
    for (index = 0u;
         index < SUDEKIMP_SAVE_BOOK_VOTE_MAX_PLAYERS;
         ++index) {
        if ((participant_mask & (uint8_t)(1u << index)) == 0u) {
            continue;
        }
        leases[index] = statehood->players[index];
        if (!leases[index].human_present || leases[index].actor == 0u ||
            leases[index].actor_generation == 0u) {
            ZeroMemory(leases, sizeof(*leases) *
                SUDEKIMP_SAVE_BOOK_VOTE_MAX_PLAYERS);
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL retained_runtime_authority_current(
    uint8_t current_human_mask,
    BOOL require_ui_authority,
    const char **reason
) {
    const SudekiMpPlayerStatehood *statehood =
        SudekiMpPlayerStatehoodRuntime();
    uint32_t source_generation;
    uintptr_t world_identity;
    uint32_t index;

    if (!pending_save_book.valid || statehood == NULL) {
        if (reason != NULL) {
            *reason = "missing_pending_or_statehood";
        }
        return FALSE;
    }
    if (GetCurrentThreadId() != pending_save_book.game_thread_id) {
        if (reason != NULL) {
            *reason = "game_thread_changed";
        }
        return FALSE;
    }
    if (!SudekiMpZoneTransitionGetSourceSnapshot(
            &source_generation, &world_identity) ||
        source_generation != pending_save_book.source_generation ||
        world_identity != pending_save_book.world_identity) {
        if (reason != NULL) {
            *reason = "world_or_source_generation_changed";
        }
        return FALSE;
    }
    if (require_ui_authority &&
        !ui_authority_matches(&pending_save_book.ui_authority)) {
        if (reason != NULL) {
            *reason = "ui_dispatch_or_controller_authority_changed";
        }
        return FALSE;
    }
    if ((current_human_mask & 0x01u) == 0u ||
        !statehood->players[0].human_present ||
        statehood->players[0].actor !=
            pending_save_book.participant_leases[0].actor ||
        statehood->players[0].actor_generation !=
            pending_save_book.participant_leases[0].actor_generation) {
        if (reason != NULL) {
            *reason = "host_lease_changed";
        }
        return FALSE;
    }
    for (index = 1u;
         index < SUDEKIMP_SAVE_BOOK_VOTE_MAX_PLAYERS;
         ++index) {
        uint8_t bit = (uint8_t)(1u << index);
        const SudekiMpPlayerLease *retained =
            &pending_save_book.participant_leases[index];
        const SudekiMpPlayerLease *current = &statehood->players[index];

        if ((save_book_vote.participant_mask & bit) == 0u ||
            (current_human_mask & bit) == 0u) {
            continue;
        }
        if (!retained->human_present || !current->human_present ||
            current->actor != retained->actor ||
            current->actor_generation != retained->actor_generation) {
            if (reason != NULL) {
                *reason = "participant_roster_identity_changed";
            }
            return FALSE;
        }
    }
    return TRUE;
}

static void release_gameplay_suppression(void) {
    if (!save_book_owns_gameplay_suppression) {
        return;
    }
    save_book_owns_gameplay_suppression = FALSE;
    SudekiMpInputBridgeSetGameplaySuppressed(FALSE);
}

static void clear_pending_save_book(void) {
    ZeroMemory(&pending_save_book, sizeof(pending_save_book));
    SudekiMpSaveBookVoteReset(&save_book_vote);
    repeated_request_logged = FALSE;
}

static void reset_native_save_lifecycle(void) {
    ZeroMemory(&native_save_lifecycle, sizeof(native_save_lifecycle));
    native_save_lifecycle_active = FALSE;
    native_save_game_thread_id = 0u;
    native_save_controller_identity = NULL;
    native_save_serial = 0u;
    native_save_source_snapshot_valid = FALSE;
    native_save_source_generation = 0u;
    native_save_world_identity = 0u;
}

static void close_save_book_lifecycle(void) {
    BOOL close_split_modal = native_save_split_latch_armed;

    clear_pending_save_book();
    reset_native_save_lifecycle();
    release_gameplay_suppression();
    native_save_split_latch_armed = FALSE;
    if (close_split_modal) {
        SudekiMpSplitScreenNativeSaveModalClosed();
    }
}

static void cancel_pending_save_book(const char *reason) {
    if (!pending_save_book.valid) {
        return;
    }
    SudekiMpLogFormat(
        "save_book_vote event=cancel serial=%lu reason=%s "
        "participant_mask=0x%02x accepted_mask=0x%02x "
        "cancelled_mask=0x%02x "
        "policy=no_native_save_menu_continuation\r\n",
        (unsigned long)save_book_vote.serial,
        reason == NULL ? "unspecified" : reason,
        (unsigned int)save_book_vote.participant_mask,
        (unsigned int)save_book_vote.accepted_mask,
        (unsigned int)save_book_vote.cancelled_mask);
    /* A final-load vote is entered only after Sudeki's own save UI is open.
     * A veto abandons the deferred loader call; it must not close that native
     * menu or synthesize an unproven cancel event. */
    if (native_save_lifecycle_active && native_save_lifecycle.mode_seen) {
        clear_pending_save_book();
        release_gameplay_suppression();
        return;
    }
    close_save_book_lifecycle();
}

static BOOL save_menu_show_signature_matches(uint8_t *base) {
    uint32_t relocated_ui_scene_global;

    if (base == NULL || base[RVA_SAVE_MENU_SHOW] != 0xa1u ||
        memcmp(base + RVA_SAVE_MENU_SHOW + 5u,
            save_menu_show_stable_tail,
            sizeof(save_menu_show_stable_tail)) != 0) {
        return FALSE;
    }
    memcpy(&relocated_ui_scene_global,
        base + RVA_SAVE_MENU_SHOW + 1u,
        sizeof(relocated_ui_scene_global));
    return relocated_ui_scene_global ==
        (uint32_t)(uintptr_t)(base + RVA_UI_SCENE_GLOBAL);
}

static BOOL load_game_save_signature_matches(uint8_t *base) {
    uint32_t relocated_loading_flag;

    if (base == NULL ||
        memcmp(base + RVA_LOAD_GAME_SAVE,
            "\x80\x3d", 2u) != 0 ||
        base[RVA_LOAD_GAME_SAVE + 6u] != 0x00u) {
        return FALSE;
    }
    memcpy(&relocated_loading_flag,
        base + RVA_LOAD_GAME_SAVE + 2u,
        sizeof(relocated_loading_flag));
    return relocated_loading_flag ==
        (uint32_t)(uintptr_t)(base + 0x00409df8u);
}

static void begin_native_save_lifecycle(
    DWORD game_thread_id,
    void *controller,
    uint32_t serial,
    BOOL source_snapshot_valid,
    uint32_t source_generation,
    uintptr_t world_identity
) {
    SudekiMpSaveBookNativeLifecycleBegin(
        &native_save_lifecycle, GetTickCount());
    native_save_lifecycle_active = TRUE;
    native_save_game_thread_id = game_thread_id;
    native_save_controller_identity = controller;
    native_save_serial = serial;
    native_save_source_snapshot_valid = source_snapshot_valid;
    native_save_source_generation = source_generation;
    native_save_world_identity = world_identity;
}

static void __attribute__((cdecl)) intercept_save_menu_show(void) {
    DWORD entry_error;
    void *controller = NULL;
    unsigned int current_mode = 0u;
    unsigned int next_mode = 0u;
    uint32_t source_generation = 0u;
    uintptr_t world_identity = 0u;
    BOOL source_snapshot_valid;

    if (!save_book_intercept_enabled || original_save_menu_show == NULL) {
        if (original_save_menu_show != NULL) {
            original_save_menu_show();
        }
        return;
    }
    entry_error = GetLastError();
    source_snapshot_valid = SudekiMpZoneTransitionGetSourceSnapshot(
        &source_generation, &world_identity);
    if (!native_save_lifecycle_active &&
        inspect_ui_controller(&controller, &current_mode, &next_mode) &&
        SudekiMpSplitScreenNativeSaveModalOpening()) {
        native_save_split_latch_armed = TRUE;
        begin_native_save_lifecycle(
            GetCurrentThreadId(), controller, 0u,
            source_snapshot_valid, source_generation, world_identity);
        SudekiMpLogWrite(
            "save_book_vote event=native_menu_open status=pass_through "
            "policy=defer_only_final_LoadGameSave_confirmation\r\n");
    }
    original_save_menu_show();
    SetLastError(entry_error);
}

static BOOL defer_final_load_game_save(int save_slot) {
    SudekiMpPendingSaveBook request;
    SudekiMpSaveBookVoteResult result;
    uint8_t human_mask;
    void *controller = NULL;
    unsigned int current_mode = 0u;
    unsigned int next_mode = 0u;

    if (pending_save_book.valid) {
        SudekiMpLogWrite(
            "save_book_vote event=final_load status=blocked "
            "reason=vote_already_pending policy=no_second_LoadGameSave\r\n");
        return TRUE;
    }
    if (!native_save_lifecycle_active || !native_save_lifecycle.mode_seen ||
        GetCurrentThreadId() != native_save_game_thread_id) {
        return FALSE;
    }
    human_mask = active_human_mask();
    if ((human_mask & (uint8_t)~0x01u) == 0u) {
        return FALSE;
    }
    ZeroMemory(&request, sizeof(request));
    request.game_thread_id = GetCurrentThreadId();
    request.save_slot = save_slot;
    if ((human_mask & 0x01u) == 0u ||
        !SudekiMpZoneTransitionGetSourceSnapshot(
            &request.source_generation, &request.world_identity) ||
        request.source_generation != native_save_source_generation ||
        request.world_identity != native_save_world_identity ||
        !inspect_ui_controller(&controller, &current_mode, &next_mode) ||
        controller != native_save_controller_identity ||
        (current_mode != SUDEKIMP_SAVE_BOOK_NATIVE_UI_MODE &&
         next_mode != SUDEKIMP_SAVE_BOOK_NATIVE_UI_MODE) ||
        !capture_participant_leases(
            human_mask, request.participant_leases) ||
        SudekiMpInputBridgeGameplaySuppressed()) {
        SudekiMpLogWrite(
            "save_book_vote event=final_load status=pass_through "
            "reason=final_confirmation_authority_unproven\r\n");
        return FALSE;
    }
    request.ui_authority.controller = controller;
    request.ui_authority.current_mode = current_mode;
    request.ui_authority.next_mode = next_mode;
    SudekiMpSaveBookVoteReset(&save_book_vote);
    result = SudekiMpSaveBookVoteRequest(
        &save_book_vote, human_mask, GetTickCount());
    if (result != SUDEKIMP_SAVE_BOOK_VOTE_OPENED) {
        SudekiMpSaveBookVoteReset(&save_book_vote);
        return FALSE;
    }
    SudekiMpInputBridgeSetGameplaySuppressed(TRUE);
    if (!SudekiMpInputBridgeGameplaySuppressed()) {
        SudekiMpSaveBookVoteCancel(&save_book_vote, 0u);
        SudekiMpSaveBookVoteReset(&save_book_vote);
        SudekiMpLogWrite(
            "save_book_vote event=final_load status=pass_through "
            "reason=vote_or_freeze_acquire_failed\r\n");
        return FALSE;
    }
    save_book_owns_gameplay_suppression = TRUE;
    request.valid = TRUE;
    pending_save_book = request;
    SudekiMpLogFormat(
        "save_book_vote event=final_load status=waiting serial=%lu slot=%d "
        "game_thread=%lu source_generation=%lu world=0x%08lx "
        "ui_scene=0x%08lx ui_controller=0x%08lx "
        "ui_mode=%u/%u "
        "participant_mask=0x%02x accepted_mask=0x01 "
        "timeout_ms=%u overlay=awaiting "
        "policy=exact_LoadGameSave_suppressed_before_native_loader\r\n",
        (unsigned long)save_book_vote.serial,
        save_slot,
        (unsigned long)request.game_thread_id,
        (unsigned long)request.source_generation,
        (unsigned long)request.world_identity,
        (unsigned long)(uintptr_t)request.ui_authority.scene,
        (unsigned long)(uintptr_t)request.ui_authority.controller,
        request.ui_authority.current_mode,
        request.ui_authority.next_mode,
        (unsigned int)save_book_vote.participant_mask,
        SUDEKIMP_SAVE_BOOK_VOTE_TIMEOUT_MS);
    return TRUE;
}

static void __attribute__((cdecl)) intercept_load_game_save(int save_slot) {
    DWORD entry_error = GetLastError();

    if (original_load_game_save == NULL || !save_book_intercept_enabled ||
        !defer_final_load_game_save(save_slot)) {
        if (original_load_game_save != NULL) {
            original_load_game_save(save_slot);
        }
    }
    SetLastError(entry_error);
}

BOOL SudekiMpSaveBookGetVoteSnapshot(
    SudekiMpSaveBookVoteSnapshot *snapshot
) {
    DWORD now;

    if (snapshot == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    ZeroMemory(snapshot, sizeof(*snapshot));
    if (!pending_save_book.valid ||
        (save_book_vote.state !=
                SUDEKIMP_SAVE_BOOK_VOTE_AWAITING_VISIBLE &&
         save_book_vote.state != SUDEKIMP_SAVE_BOOK_VOTE_WAITING)) {
        return FALSE;
    }
    now = GetTickCount();
    snapshot->active = TRUE;
    snapshot->state = (unsigned int)save_book_vote.state;
    snapshot->serial = save_book_vote.serial;
    snapshot->remaining_ms = save_book_vote.state ==
            SUDEKIMP_SAVE_BOOK_VOTE_AWAITING_VISIBLE ?
        SUDEKIMP_SAVE_BOOK_VOTE_TIMEOUT_MS :
        SudekiMpSaveBookVoteRemainingMs(&save_book_vote, now);
    snapshot->participant_mask = save_book_vote.participant_mask;
    snapshot->accepted_mask = save_book_vote.accepted_mask;
    snapshot->cancelled_mask = save_book_vote.cancelled_mask;
    return TRUE;
}

BOOL SudekiMpSaveBookReportVoteOverlay(
    uint32_t serial,
    BOOL visible
) {
    SudekiMpSaveBookVoteResult result;
    SudekiMpSaveBookVoteState previous_state;
    DWORD now;

    if (!pending_save_book.valid) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    previous_state = save_book_vote.state;
    now = GetTickCount();
    result = SudekiMpSaveBookVoteReportOverlay(
        &save_book_vote, serial, visible != FALSE, now);
    if (result == SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_INVALID ||
        result == SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_BUSY ||
        result == SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_STALE) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (result == SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED_NOW) {
        cancel_pending_save_book("overlay_draw_failed");
        return TRUE;
    }
    if (visible &&
        previous_state == SUDEKIMP_SAVE_BOOK_VOTE_AWAITING_VISIBLE &&
        save_book_vote.state == SUDEKIMP_SAVE_BOOK_VOTE_WAITING) {
        SudekiMpLogFormat(
            "save_book_vote event=overlay status=visible serial=%lu "
            "countdown_ms=%u\r\n",
            (unsigned long)serial,
            SUDEKIMP_SAVE_BOOK_VOTE_TIMEOUT_MS);
    }
    return TRUE;
}

BOOL SudekiMpSaveBookRespondVote(
    uint32_t serial,
    unsigned int player_index,
    BOOL accept
) {
    SudekiMpSaveBookVoteResult result;

    if (!pending_save_book.valid) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    result = SudekiMpSaveBookVoteRespond(
        &save_book_vote, serial, player_index, accept != FALSE);
    if (result == SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_INVALID ||
        result == SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_BUSY ||
        result == SUDEKIMP_SAVE_BOOK_VOTE_REJECTED_STALE) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    SudekiMpLogFormat(
        "save_book_vote event=response serial=%lu player=%u "
        "response=%s participant_mask=0x%02x accepted_mask=0x%02x\r\n",
        (unsigned long)serial,
        player_index + 1u,
        accept ? "accept" : "veto",
        (unsigned int)save_book_vote.participant_mask,
        (unsigned int)save_book_vote.accepted_mask);
    if (result == SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED_NOW) {
        cancel_pending_save_book(player_index == 0u ?
            "host_veto" : "participant_veto");
    }
    return TRUE;
}

static void service_native_save_lifecycle(void) {
    SudekiMpSaveBookNativeLifecycleResult lifecycle_result;
    void *controller = NULL;
    unsigned int current_mode = 0u;
    unsigned int next_mode = 0u;
    BOOL inspection_valid;
    uint8_t human_mask;
    const char *reason = NULL;
    uint32_t source_generation = 0u;
    uintptr_t world_identity = 0u;
    BOOL source_changed = FALSE;

    if (!native_save_lifecycle_active) {
        return;
    }
    if (GetCurrentThreadId() != native_save_game_thread_id) {
        SudekiMpLogFormat(
            "save_book_vote event=native_lifecycle status=failed "
            "serial=%lu reason=game_thread_changed\r\n",
            (unsigned long)native_save_serial);
        if (pending_save_book.valid &&
            save_book_vote.state == SUDEKIMP_SAVE_BOOK_VOTE_REPLAYING) {
            (void)SudekiMpSaveBookVoteFinishReplay(
                &save_book_vote, save_book_vote.serial, 0);
        }
        close_save_book_lifecycle();
        return;
    }
    if (native_save_source_snapshot_valid) {
        source_changed =
            !SudekiMpZoneTransitionGetSourceSnapshot(
                &source_generation, &world_identity) ||
            source_generation != native_save_source_generation ||
            world_identity != native_save_world_identity;
    }
    if (source_changed) {
        lifecycle_result =
            SudekiMpSaveBookNativeLifecycleSourceChanged(
                &native_save_lifecycle);
        if (pending_save_book.valid &&
            save_book_vote.state == SUDEKIMP_SAVE_BOOK_VOTE_REPLAYING) {
            (void)SudekiMpSaveBookVoteFinishReplay(
                &save_book_vote, save_book_vote.serial,
                lifecycle_result ==
                    SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_CLOSED);
        }
        SudekiMpLogFormat(
            "save_book_vote event=native_lifecycle status=%s "
            "serial=%lu reason=world_or_source_generation_changed "
            "phase=%s\r\n",
            lifecycle_result == SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_CLOSED ?
                "closed" : "failed",
            (unsigned long)native_save_serial,
            native_save_lifecycle.mode_seen ?
                "post_mode_12_load_teardown" : "awaiting_mode_12");
        close_save_book_lifecycle();
        return;
    }
    if (pending_save_book.valid &&
        save_book_vote.state == SUDEKIMP_SAVE_BOOK_VOTE_REPLAYING) {
        human_mask = active_human_mask();
        if (!retained_runtime_authority_current(
                human_mask, FALSE, &reason)) {
            (void)SudekiMpSaveBookVoteFinishReplay(
                &save_book_vote, save_book_vote.serial, 0);
            SudekiMpLogFormat(
                "save_book_vote event=native_lifecycle status=failed "
                "serial=%lu reason=%s phase=awaiting_mode_12\r\n",
                (unsigned long)native_save_serial,
                reason == NULL ? "runtime_authority_changed" : reason);
            close_save_book_lifecycle();
            return;
        }
    }
    inspection_valid = inspect_ui_controller(
        &controller, &current_mode, &next_mode) &&
        controller == native_save_controller_identity;
    lifecycle_result = SudekiMpSaveBookNativeLifecycleUpdate(
        &native_save_lifecycle,
        inspection_valid,
        current_mode,
        next_mode,
        GetTickCount());
    if (lifecycle_result == SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_ENTERED) {
        if (pending_save_book.valid &&
            save_book_vote.state == SUDEKIMP_SAVE_BOOK_VOTE_REPLAYING) {
            (void)SudekiMpSaveBookVoteFinishReplay(
                &save_book_vote, save_book_vote.serial, 1);
            clear_pending_save_book();
        }
        /* The native UI now owns P1 input. Releasing the bridge suppression
         * starts its neutral fence; the split SAVE_BOOK latch remains armed
         * until the exact mode-12 close edge. */
        release_gameplay_suppression();
        SudekiMpLogFormat(
            "save_book_vote event=native_lifecycle status=entered "
            "serial=%lu ui_mode=%u/%u suppression=released_neutral_fenced "
            "split_latch=retained\r\n",
            (unsigned long)native_save_serial,
            current_mode,
            next_mode);
        return;
    }
    if (lifecycle_result == SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_CLOSED) {
        SudekiMpLogFormat(
            "save_book_vote event=native_lifecycle status=closed "
            "serial=%lu ui_mode=%u/%u stable_samples=%u\r\n",
            (unsigned long)native_save_serial,
            current_mode,
            next_mode,
            SUDEKIMP_SAVE_BOOK_NATIVE_CLOSE_STABLE_SAMPLES);
        close_save_book_lifecycle();
        return;
    }
    if (lifecycle_result ==
            SUDEKIMP_SAVE_BOOK_NATIVE_LIFECYCLE_ENTER_FAILED) {
        if (pending_save_book.valid &&
            save_book_vote.state == SUDEKIMP_SAVE_BOOK_VOTE_REPLAYING) {
            (void)SudekiMpSaveBookVoteFinishReplay(
                &save_book_vote, save_book_vote.serial, 0);
        }
        SudekiMpLogFormat(
            "save_book_vote event=native_lifecycle status=failed "
            "serial=%lu reason=mode_12_not_observed timeout_ms=%u "
            "inspection_valid=%s ui_mode=%u/%u policy=no_second_replay\r\n",
            (unsigned long)native_save_serial,
            SUDEKIMP_SAVE_BOOK_NATIVE_ENTER_TIMEOUT_MS,
            inspection_valid ? "true" : "false",
            current_mode,
            next_mode);
        close_save_book_lifecycle();
    }
}

void SudekiMpSaveBookService(void) {
    SudekiMpSaveBookVoteResult result;
    uint8_t human_mask;
    uint32_t serial;
    const char *reason = NULL;

    if (native_save_lifecycle_active) {
        service_native_save_lifecycle();
        if (!native_save_lifecycle_active || !pending_save_book.valid) {
            return;
        }
    }
    if (!pending_save_book.valid) {
        return;
    }
    human_mask = active_human_mask();
    if (!retained_runtime_authority_current(
            human_mask, FALSE, &reason)) {
        SudekiMpSaveBookVoteCancel(&save_book_vote, 0u);
        cancel_pending_save_book(reason);
        return;
    }
    result = SudekiMpSaveBookVoteUpdate(
        &save_book_vote, human_mask, GetTickCount());
    if (result == SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED_NOW ||
        save_book_vote.state == SUDEKIMP_SAVE_BOOK_VOTE_CANCELLED) {
        cancel_pending_save_book(
            save_book_vote.cancelled_mask == 0x01u ?
                "host_dropout" : "overlay_not_reported");
        return;
    }
    if (save_book_vote.state != SUDEKIMP_SAVE_BOOK_VOTE_READY) {
        return;
    }

    /* Revalidate after READY and before atomically claiming the sole replay. */
    human_mask = active_human_mask();
    if (!retained_runtime_authority_current(
            human_mask, FALSE, &reason)) {
        SudekiMpSaveBookVoteCancel(&save_book_vote, 0u);
        cancel_pending_save_book(reason);
        return;
    }
    serial = save_book_vote.serial;
    if (original_load_game_save == NULL ||
        SudekiMpSaveBookVoteBeginReplay(&save_book_vote, serial) !=
            SUDEKIMP_SAVE_BOOK_VOTE_REPLAY_STARTED) {
        SudekiMpSaveBookVoteCancel(&save_book_vote, 0u);
        cancel_pending_save_book("native_replay_claim_failed");
        return;
    }
    SudekiMpLogFormat(
        "save_book_vote event=commit serial=%lu phase=before_native "
        "slot=%d split_latch=retained "
        "policy=one_saved_LoadGameSave_trampoline_on_game_thread\r\n",
        (unsigned long)serial, pending_save_book.save_slot);
    original_load_game_save(pending_save_book.save_slot);
    SudekiMpLogFormat(
        "save_book_vote event=commit serial=%lu phase=after_native "
        "status=continued_once awaiting_native_load_teardown\r\n",
        (unsigned long)serial);
    (void)SudekiMpSaveBookVoteFinishReplay(&save_book_vote, serial, 1);
    clear_pending_save_book();
}

void SudekiMpSaveBookObserveNativeClosed(void) {
    if (pending_save_book.valid && !native_save_lifecycle_active) {
        SudekiMpSaveBookVoteCancel(&save_book_vote, 0u);
        cancel_pending_save_book("native_close_before_replay");
        return;
    }
    /* Exact UI-controller mode inspection owns closure. An external observer
     * may request a service sample, but cannot bypass the stable non-12 gate. */
    service_native_save_lifecycle();
}

BOOL SudekiMpSaveBookVoteActive(void) {
    return pending_save_book.valid &&
        (save_book_vote.state ==
                SUDEKIMP_SAVE_BOOK_VOTE_AWAITING_VISIBLE ||
         save_book_vote.state == SUDEKIMP_SAVE_BOOK_VOTE_WAITING ||
         save_book_vote.state == SUDEKIMP_SAVE_BOOK_VOTE_READY);
}

#if defined(SUDEKIMP_SAVE_BOOK_INTERCEPT_TESTING)
const void *SudekiMpSaveBookInterceptOriginalForTesting(void) {
    return (const void *)original_save_menu_show;
}

const void *SudekiMpSaveBookInterceptLoadGameSaveOriginalForTesting(void) {
    return (const void *)original_load_game_save;
}
#endif

BOOL SudekiMpInstallSaveBookIntercept(
    HMODULE game_module,
    BOOL enabled
) {
    uint8_t *base = (uint8_t *)game_module;
    uint8_t expected_save_menu_entry[SAVE_MENU_SHOW_HOOK_LENGTH];
    uint8_t expected_load_entry[LOAD_GAME_SAVE_HOOK_LENGTH];

    if (!enabled) {
        return TRUE;
    }
    if (base == NULL || game_base != NULL ||
        (uintptr_t)base > (uintptr_t)(UINT32_MAX - RVA_UI_SCENE_GLOBAL) ||
        !save_menu_show_signature_matches(base) ||
        !load_game_save_signature_matches(base)) {
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return FALSE;
    }
    SudekiMpSaveBookVoteInitialize(&save_book_vote);
    ZeroMemory(&pending_save_book, sizeof(pending_save_book));
    repeated_request_logged = FALSE;
    reset_native_save_lifecycle();
    native_save_split_latch_armed = FALSE;
    save_book_owns_gameplay_suppression = FALSE;
    game_base = base;
    memcpy(expected_save_menu_entry,
        base + RVA_SAVE_MENU_SHOW,
        sizeof(expected_save_menu_entry));
    if (!SudekiMpInstallInlineHook(
            &save_menu_show_hook,
            base + RVA_SAVE_MENU_SHOW,
            expected_save_menu_entry,
            sizeof(expected_save_menu_entry),
            intercept_save_menu_show)) {
        game_base = NULL;
        return FALSE;
    }
    original_save_menu_show = (SaveMenuShowFunction)
        save_menu_show_hook.trampoline;
    memcpy(expected_load_entry,
        base + RVA_LOAD_GAME_SAVE,
        sizeof(expected_load_entry));
    if (!SudekiMpInstallInlineHook(
            &load_game_save_hook,
            base + RVA_LOAD_GAME_SAVE,
            expected_load_entry,
            sizeof(expected_load_entry),
            intercept_load_game_save)) {
        (void)SudekiMpRestoreInlineHook(&save_menu_show_hook);
        original_save_menu_show = NULL;
        game_base = NULL;
        return FALSE;
    }
    load_game_save_trampoline = load_game_save_hook.trampoline;
    original_load_game_save = (LoadGameSaveFunction)load_game_save_trampoline;
    save_book_intercept_enabled = TRUE;
    SudekiMpLogWrite(
        "save_book_vote_install=success save_menu_show_rva=0x00084f10 "
        "load_game_save_rva=0x00101690 abi=cdecl_void_no_args_plus_cdecl_slot "
        "timeout_ms=10000 policy=pass_through_menu_defer_final_loader\r\n");
    return TRUE;
}

void SudekiMpUninstallSaveBookIntercept(void) {
    save_book_intercept_enabled = FALSE;
    if (pending_save_book.valid && !native_save_lifecycle_active) {
        cancel_pending_save_book("uninstall");
    } else if (native_save_lifecycle_active ||
               native_save_split_latch_armed ||
               save_book_owns_gameplay_suppression) {
        close_save_book_lifecycle();
    }
    (void)SudekiMpRestoreInlineHook(&load_game_save_hook);
    load_game_save_trampoline = NULL;
    original_load_game_save = NULL;
    (void)SudekiMpRestoreInlineHook(&save_menu_show_hook);
    original_save_menu_show = NULL;
    game_base = NULL;
    repeated_request_logged = FALSE;
    native_save_split_latch_armed = FALSE;
    save_book_owns_gameplay_suppression = FALSE;
    reset_native_save_lifecycle();
    ZeroMemory(&pending_save_book, sizeof(pending_save_book));
    SudekiMpSaveBookVoteInitialize(&save_book_vote);
}
