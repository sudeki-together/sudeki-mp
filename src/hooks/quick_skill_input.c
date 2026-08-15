#include "hooks/quick_skill_input.h"

#include "engine/log.h"
#include "engine/player_combat_context.h"
#include "hooks/call_hook.h"

#include <stdint.h>
#include <string.h>

#if defined(__GNUC__) && defined(__i386__)
#define SUDEKIMP_EAX_ARGUMENT __attribute__((regparm(1)))
#else
#error "Quick-skill input tracing requires 32-bit GCC regparm support"
#endif

/*
 * The native direct-activation helper receives its action ID in EAX rather
 * than through a conventional stack argument. GCC regparm(1) provides the
 * small bridge needed to observe and forward that register unchanged.
 */
typedef void (SUDEKIMP_EAX_ARGUMENT *QuickSkillActionFunction)(uint32_t action_id);
typedef int (__attribute__((regparm(2))) *QuickSkillValidateFunction)(
    void *skill,
    int slot
);
typedef int (__stdcall *SpiritStrikeFunction)(void *manager, int strike_id);

enum {
    RVA_QUICK_SKILL_ACTION_CALL = 0x00027acfu,
    RVA_QUICK_SKILL_ACTION = 0x00027bf0u,
    RVA_QUICK_SKILL_VALIDATE_CALL = 0x00027c8cu,
    RVA_QUICK_MENU_SKILL_VALIDATE_CALL = 0x00099867u,
    RVA_CSKILL_USE_VALIDATE_CALL = 0x000b4828u,
    RVA_QUICK_SKILL_VALIDATE = 0x000b4bc0u,
    RVA_QUICK_MENU_SPIRIT_VALIDATE_CALL = 0x000998b9u,
    RVA_QUICK_MENU_SPIRIT_ACTIVATE_CALL = 0x000998dcu,
    RVA_SPIRIT_STRIKE_VALIDATE = 0x00010940u,
    RVA_SPIRIT_STRIKE_ACTIVATE = 0x0000fba0u,
    RVA_SET_UI_ACTIVE = 0x0000afd0u,
    RVA_UI_MANAGER_GLOBAL = 0x00408d1cu,
    QUICK_SKILL_ACTION_FIRST = 0x7au,
    QUICK_SKILL_ACTION_LAST = 0x7fu
};

typedef struct SkillReadinessSnapshot {
    void *owner;
    void *actor_state;
    uint32_t flags_50;
    uint32_t state_58;
    uint32_t flags_60;
} SkillReadinessSnapshot;

static SudekiMpRelativeCallHook quick_skill_action_call_hook;
static SudekiMpRelativeCallHook quick_skill_validate_call_hook;
static SudekiMpRelativeCallHook quick_menu_skill_validate_call_hook;
static SudekiMpRelativeCallHook cskill_use_validate_call_hook;
static SudekiMpRelativeCallHook quick_menu_spirit_validate_call_hook;
static SudekiMpRelativeCallHook quick_menu_spirit_activate_call_hook;
static QuickSkillActionFunction original_quick_skill_action;
static QuickSkillValidateFunction original_quick_skill_validate;
static SpiritStrikeFunction original_spirit_strike_validate;
static SpiritStrikeFunction original_spirit_strike_activate;
static uint8_t *game_base;
static BOOL ranged_prototype_enabled;
static BOOL realtime_targeting_guard_enabled;
static BOOL ranged_transition_pending;
static UINT_PTR ranged_transition_timer;
static uint32_t ranged_transition_action_id;
static void *ranged_transition_skill;
static uint32_t current_action_id;
static void *current_action_skill;
static BOOL current_action_validation_seen;
static int current_action_validation_result;
static SkillReadinessSnapshot current_action_snapshot;

static void capture_skill_readiness(
    void *skill,
    SkillReadinessSnapshot *snapshot
) {
    uint8_t *owner;
    uint8_t *actor_state;

    memset(snapshot, 0, sizeof(*snapshot));
    if (skill == NULL) {
        return;
    }
    owner = *(uint8_t **)((uint8_t *)skill + 0x10);
    snapshot->owner = owner;
    if (owner == NULL) {
        return;
    }
    actor_state = *(uint8_t **)(owner + 0x90);
    snapshot->actor_state = actor_state;
    if (actor_state == NULL) {
        return;
    }
    snapshot->flags_50 = *(uint32_t *)(actor_state + 0x50);
    snapshot->state_58 = *(uint32_t *)(actor_state + 0x58);
    snapshot->flags_60 = *(uint32_t *)(actor_state + 0x60);
}

static void log_skill_readiness(
    const char *source,
    void *skill,
    int slot,
    int result,
    const SkillReadinessSnapshot *snapshot
) {
    SudekiMpLogFormat(
        "quick_skill_input event=readiness source=%s skill=0x%08lx slot=%d result=%d owner=0x%08lx actor_state=0x%08lx flags_50=0x%08lx state_58=0x%08lx state_58_low=%lu flags_60=0x%08lx\r\n",
        source,
        (unsigned long)(uintptr_t)skill,
        slot,
        result,
        (unsigned long)(uintptr_t)snapshot->owner,
        (unsigned long)(uintptr_t)snapshot->actor_state,
        (unsigned long)snapshot->flags_50,
        (unsigned long)snapshot->state_58,
        (unsigned long)(snapshot->state_58 & 0x0fu),
        (unsigned long)snapshot->flags_60
    );
}

static void set_native_ui_active(BOOL active) {
    void *ui_manager;
    void *function;

    ui_manager = *(void **)(game_base + RVA_UI_MANAGER_GLOBAL);
    function = game_base + RVA_SET_UI_ACTIVE;
    __asm__ volatile(
        "movl %0, %%esi\n\t"
        "pushl %1\n\t"
        "call *%2"
        :
        : "r"(ui_manager), "r"(active), "r"(function)
        : "eax", "ecx", "edx", "esi", "memory", "cc"
    );
}

static BOOL is_ranged_strafe_rejection(void) {
    return current_action_id >= QUICK_SKILL_ACTION_FIRST &&
        current_action_id <= QUICK_SKILL_ACTION_LAST &&
        !ranged_transition_pending &&
        current_action_validation_seen &&
        current_action_validation_result == 2 &&
        current_action_skill != NULL &&
        current_action_snapshot.actor_state != NULL &&
        (current_action_snapshot.flags_50 & 0x00400000u) != 0 &&
        (current_action_snapshot.flags_50 & 0x00000002u) != 0 &&
        (current_action_snapshot.flags_60 & 0x00000004u) == 0;
}

static BOOL is_idle_armed_state(const SkillReadinessSnapshot *snapshot) {
    return snapshot->actor_state != NULL &&
        (snapshot->flags_50 & 0x00000001u) != 0 &&
        (snapshot->flags_50 & 0x00000002u) != 0 &&
        (snapshot->flags_50 & 0x00400000u) == 0;
}

static void register_started_quick_skill(void) {
    void *owner;

    if (current_action_skill == NULL ||
        !current_action_validation_seen ||
        current_action_validation_result != 0 ||
        *(uint8_t *)((uint8_t *)current_action_skill + 0x6cu) == 0u) {
        return;
    }
    owner = *(void **)((uint8_t *)current_action_skill + 0x10u);
    SudekiMpCombatContextSkillStarted(owner, current_action_skill);
}

static void CALLBACK complete_ranged_transition(
    HWND window,
    UINT message,
    UINT_PTR timer_id,
    DWORD time
) {
    SkillReadinessSnapshot waited_snapshot;
    SkillReadinessSnapshot exited_snapshot;
    uint32_t action_id;
    void *skill;

    (void)window;
    (void)message;
    (void)time;
    KillTimer(NULL, timer_id);
    ranged_transition_timer = 0;
    action_id = ranged_transition_action_id;
    skill = ranged_transition_skill;

    capture_skill_readiness(skill, &waited_snapshot);
    log_skill_readiness(
        "prototype_ui_wait",
        skill,
        -1,
        -1,
        &waited_snapshot
    );
    set_native_ui_active(FALSE);
    capture_skill_readiness(skill, &exited_snapshot);
    log_skill_readiness(
        "prototype_ui_exit",
        skill,
        -1,
        -1,
        &exited_snapshot
    );

    ranged_transition_pending = FALSE;
    ranged_transition_action_id = 0;
    ranged_transition_skill = NULL;
    if (is_idle_armed_state(&exited_snapshot)) {
        current_action_id = action_id;
        current_action_skill = NULL;
        current_action_validation_seen = FALSE;
        current_action_validation_result = -1;
        SudekiMpLogWrite(
            "quick_skill_input event=ranged_transition_retry\r\n"
        );
        original_quick_skill_action(action_id);
        register_started_quick_skill();
        if (realtime_targeting_guard_enabled) {
            SudekiMpCombatContextsPollGame((HMODULE)game_base);
        }
    } else {
        SudekiMpLogWrite(
            "quick_skill_input event=ranged_transition_abort reason=not_idle_armed\r\n"
        );
    }
}

static int __attribute__((regparm(2))) trace_quick_skill_validate(
    void *skill,
    int slot
) {
    SkillReadinessSnapshot snapshot;
    capture_skill_readiness(skill, &snapshot);
    int result = original_quick_skill_validate(skill, slot);

    current_action_validation_seen = TRUE;
    current_action_validation_result = result;
    current_action_skill = skill;
    current_action_snapshot = snapshot;
    SudekiMpLogFormat(
        "quick_skill_input event=validate action=0x%02lx ordinal=%lu skill=0x%08lx slot=%d result=%d\r\n",
        (unsigned long)current_action_id,
        (unsigned long)(current_action_id - QUICK_SKILL_ACTION_FIRST),
        (unsigned long)(uintptr_t)skill,
        slot,
        result
    );
    log_skill_readiness("direct", skill, slot, result, &snapshot);
    return result;
}

static int __attribute__((regparm(2))) trace_quick_menu_skill_validate(
    void *skill,
    int slot
) {
    SkillReadinessSnapshot snapshot;
    int result;

    capture_skill_readiness(skill, &snapshot);
    result = original_quick_skill_validate(skill, slot);
    log_skill_readiness("quick_menu", skill, slot, result, &snapshot);
    return result;
}

static int __attribute__((regparm(2))) trace_cskill_use_validate(
    void *skill,
    int slot
) {
    SkillReadinessSnapshot snapshot;
    int result;

    capture_skill_readiness(skill, &snapshot);
    result = original_quick_skill_validate(skill, slot);
    log_skill_readiness("use_internal", skill, slot, result, &snapshot);
    return result;
}

static int __stdcall trace_spirit_strike_validate(
    void *manager,
    int strike_id
) {
    int result = original_spirit_strike_validate(manager, strike_id);
    SudekiMpLogFormat(
        "spirit_strike_input event=quick_menu_validate manager=0x%08lx strike_id=%d result=%d\r\n",
        (unsigned long)(uintptr_t)manager,
        strike_id,
        result
    );
    return result;
}

static int __stdcall trace_spirit_strike_activate(
    void *manager,
    int strike_id
) {
    int result;
    SudekiMpLogFormat(
        "spirit_strike_input event=quick_menu_activate_begin manager=0x%08lx strike_id=%d\r\n",
        (unsigned long)(uintptr_t)manager,
        strike_id
    );
    result = original_spirit_strike_activate(manager, strike_id);
    SudekiMpLogFormat(
        "spirit_strike_input event=quick_menu_activate_end strike_id=%d result=%d\r\n",
        strike_id,
        result
    );
    return result;
}

static void SUDEKIMP_EAX_ARGUMENT trace_quick_skill_action(uint32_t action_id) {
    SkillReadinessSnapshot entered_snapshot;
    const char *guard_reason;

    current_action_id = action_id;
    current_action_skill = NULL;
    current_action_validation_seen = FALSE;
    current_action_validation_result = -1;
    if (action_id >= QUICK_SKILL_ACTION_FIRST &&
        action_id <= QUICK_SKILL_ACTION_LAST) {
        SudekiMpLogFormat(
            "quick_skill_input event=pressed action=0x%02lx ordinal=%lu\r\n",
            (unsigned long)action_id,
            (unsigned long)(action_id - QUICK_SKILL_ACTION_FIRST)
        );
    } else {
        SudekiMpLogFormat(
            "quick_skill_input event=unexpected_action action=0x%08lx\r\n",
            (unsigned long)action_id
        );
    }
    if (realtime_targeting_guard_enabled &&
        action_id >= QUICK_SKILL_ACTION_FIRST &&
        action_id <= QUICK_SKILL_ACTION_LAST) {
        SudekiMpCombatContextsPollGame((HMODULE)game_base);
    }
    if (realtime_targeting_guard_enabled &&
        action_id >= QUICK_SKILL_ACTION_FIRST &&
        action_id <= QUICK_SKILL_ACTION_LAST &&
        !SudekiMpCombatContextCanStartSkill(0u, &guard_reason)) {
        SudekiMpLogFormat(
            "realtime_skill_combat event=player_skill_rejected player=1 ordinal=%lu reason=%s policy=fail_safe_native_targeting_serialization\r\n",
            (unsigned long)(action_id - QUICK_SKILL_ACTION_FIRST),
            guard_reason
        );
        return;
    }
    original_quick_skill_action(action_id);
    register_started_quick_skill();
    if (realtime_targeting_guard_enabled) {
        SudekiMpCombatContextsPollGame((HMODULE)game_base);
    }
    if (ranged_prototype_enabled && is_ranged_strafe_rejection()) {
        SudekiMpLogWrite(
            "quick_skill_input event=ranged_transition_begin method=native_ui_cycle\r\n"
        );
        set_native_ui_active(TRUE);
        capture_skill_readiness(current_action_skill, &entered_snapshot);
        log_skill_readiness(
            "prototype_ui_enter",
            current_action_skill,
            -1,
            -1,
            &entered_snapshot
        );
        ranged_transition_pending = TRUE;
        ranged_transition_action_id = action_id;
        ranged_transition_skill = current_action_skill;
        ranged_transition_timer = SetTimer(
            NULL,
            0,
            75,
            complete_ranged_transition
        );
        if (ranged_transition_timer == 0) {
            ranged_transition_pending = FALSE;
            ranged_transition_action_id = 0;
            ranged_transition_skill = NULL;
            set_native_ui_active(FALSE);
            SudekiMpLogWrite(
                "quick_skill_input event=ranged_transition_abort reason=timer_error\r\n"
            );
        } else {
            SudekiMpLogFormat(
                "quick_skill_input event=ranged_transition_scheduled timer=0x%08lx delay_ms=75\r\n",
                (unsigned long)ranged_transition_timer
            );
        }
    }
    SudekiMpLogFormat(
        "quick_skill_input event=complete action=0x%02lx ordinal=%lu validation_seen=%u validation_result=%d\r\n",
        (unsigned long)action_id,
        (unsigned long)(action_id - QUICK_SKILL_ACTION_FIRST),
        (unsigned int)current_action_validation_seen,
        current_action_validation_result
    );
}

BOOL SudekiMpInstallQuickSkillInputTrace(
    HMODULE game_module,
    BOOL enable_ranged_prototype,
    BOOL enable_realtime_targeting_guard
) {
    uint8_t *base;

    if (game_module == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    base = (uint8_t *)game_module;
    game_base = base;
    ranged_prototype_enabled = enable_ranged_prototype;
    realtime_targeting_guard_enabled = enable_realtime_targeting_guard;
    original_quick_skill_action = (QuickSkillActionFunction)(
        base + RVA_QUICK_SKILL_ACTION
    );
    original_quick_skill_validate = (QuickSkillValidateFunction)(
        base + RVA_QUICK_SKILL_VALIDATE
    );
    original_spirit_strike_validate = (SpiritStrikeFunction)(
        base + RVA_SPIRIT_STRIKE_VALIDATE
    );
    original_spirit_strike_activate = (SpiritStrikeFunction)(
        base + RVA_SPIRIT_STRIKE_ACTIVATE
    );
    if (!SudekiMpInstallRelativeCallHook(
            &quick_skill_action_call_hook,
            base + RVA_QUICK_SKILL_ACTION_CALL,
            original_quick_skill_action,
            trace_quick_skill_action) ||
        !SudekiMpInstallRelativeCallHook(
            &quick_skill_validate_call_hook,
            base + RVA_QUICK_SKILL_VALIDATE_CALL,
            original_quick_skill_validate,
            trace_quick_skill_validate) ||
        !SudekiMpInstallRelativeCallHook(
            &quick_menu_skill_validate_call_hook,
            base + RVA_QUICK_MENU_SKILL_VALIDATE_CALL,
            original_quick_skill_validate,
            trace_quick_menu_skill_validate) ||
        !SudekiMpInstallRelativeCallHook(
            &cskill_use_validate_call_hook,
            base + RVA_CSKILL_USE_VALIDATE_CALL,
            original_quick_skill_validate,
            trace_cskill_use_validate) ||
        !SudekiMpInstallRelativeCallHook(
            &quick_menu_spirit_validate_call_hook,
            base + RVA_QUICK_MENU_SPIRIT_VALIDATE_CALL,
            original_spirit_strike_validate,
            trace_spirit_strike_validate) ||
        !SudekiMpInstallRelativeCallHook(
            &quick_menu_spirit_activate_call_hook,
            base + RVA_QUICK_MENU_SPIRIT_ACTIVATE_CALL,
            original_spirit_strike_activate,
            trace_spirit_strike_activate)) {
        SudekiMpUninstallQuickSkillInputTrace();
        original_quick_skill_action = NULL;
        original_quick_skill_validate = NULL;
        original_spirit_strike_validate = NULL;
        original_spirit_strike_activate = NULL;
        return FALSE;
    }

    SudekiMpLogWrite("quick_skill_input_trace_install=success\r\n");
    SudekiMpLogFormat(
        "quick_skill_input_ranged_prototype=%s\r\n",
        ranged_prototype_enabled ? "enabled" : "disabled"
    );
    SudekiMpLogFormat(
        "quick_skill_input_realtime_targeting_guard=%s policy=serialize_native_global_target_selection_allow_cross_player_execution_overlap\r\n",
        realtime_targeting_guard_enabled ? "enabled" : "disabled"
    );
    return TRUE;
}

void SudekiMpUninstallQuickSkillInputTrace(void) {
    if (ranged_transition_timer != 0) {
        KillTimer(NULL, ranged_transition_timer);
    }
    SudekiMpRestoreRelativeCallHook(&quick_menu_spirit_activate_call_hook);
    SudekiMpRestoreRelativeCallHook(&quick_menu_spirit_validate_call_hook);
    SudekiMpRestoreRelativeCallHook(&cskill_use_validate_call_hook);
    SudekiMpRestoreRelativeCallHook(&quick_menu_skill_validate_call_hook);
    SudekiMpRestoreRelativeCallHook(&quick_skill_validate_call_hook);
    SudekiMpRestoreRelativeCallHook(&quick_skill_action_call_hook);
    original_quick_skill_action = NULL;
    original_quick_skill_validate = NULL;
    original_spirit_strike_validate = NULL;
    original_spirit_strike_activate = NULL;
    game_base = NULL;
    ranged_prototype_enabled = FALSE;
    realtime_targeting_guard_enabled = FALSE;
    ranged_transition_pending = FALSE;
    ranged_transition_timer = 0;
    ranged_transition_action_id = 0;
    ranged_transition_skill = NULL;
    current_action_id = 0;
    current_action_skill = NULL;
    current_action_validation_seen = FALSE;
    current_action_validation_result = -1;
}
