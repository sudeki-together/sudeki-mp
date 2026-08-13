#include "hooks/skill_trace.h"

#include "engine/log.h"
#include "hooks/call_hook.h"

#include <stdint.h>
#include <string.h>

#if defined(__GNUC__) && defined(__i386__)
#define SUDEKIMP_FASTCALL __attribute__((fastcall))
#else
#error "Skill tracing requires the 32-bit GCC-compatible fastcall bridge"
#endif

typedef uint8_t SkillBool;
/*
 * Hook wrappers use the conventional x86 fastcall bridge: Sudeki's ECX `this`
 * value is the first register argument, EDX is an ignored bridge argument, and
 * the original thiscall stack arguments remain in their original positions.
 */
typedef SkillBool (SUDEKIMP_FASTCALL *SkillUseFunction)(void *, void *, int);
typedef void (SUDEKIMP_FASTCALL *StopRumbleFunction)(void *, void *);
typedef SkillBool (SUDEKIMP_FASTCALL *AnimationStateFunction)(
    void *, void *, const char *, uint8_t, uint8_t
);
typedef SkillBool (SUDEKIMP_FASTCALL *AnimationDirectStringFunction)(
    void *, void *, const char *, uint8_t, uint8_t, void *
);
typedef SkillBool (SUDEKIMP_FASTCALL *AnimationDirectResourceFunction)(
    void *, void *, const void *, uint8_t, uint8_t, void *
);
typedef SkillBool (SUDEKIMP_FASTCALL *MissileFunction)(void *, void *, int);
typedef void (SUDEKIMP_FASTCALL *FloatFunction)(void *, void *, float);
typedef int (SUDEKIMP_FASTCALL *ScriptCallOpcodeFunction)(void *, void *);
typedef int (*ScriptBindingInvokeFunction)(void);

enum {
    RVA_CSKILL_USE = 0x000b4810u,
    RVA_CSKILL_USE_CALL = 0x000998a1u,
    RVA_CSKILL_STOP_RUMBLE = 0x000b50d0u,
    RVA_CSKILL_STOP_RUMBLE_CALL = 0x000b4f23u,
    RVA_PLAY_ANIMATION_STATE = 0x0000f2e0u,
    RVA_PLAY_ANIMATION_DIRECT_STRING = 0x0000f310u,
    RVA_PLAY_ANIMATION_DIRECT_RESOURCE = 0x0000f380u,
    RVA_PUSH_ANIMATION_STATE = 0x0000f3f0u,
    RVA_PUSH_ANIMATION_DIRECT_STRING = 0x0000f420u,
    RVA_PUSH_ANIMATION_DIRECT_RESOURCE = 0x0000f480u,
    RVA_MISSILE_PLAY_ANIMATION_DIRECT = 0x0003af80u,
    RVA_MISSILE_PUSH_ANIMATION_DIRECT = 0x0003aff0u,
    RVA_FIRE_MISSILE_SCRIPTED = 0x000c89c0u,
    RVA_FIRE_MISSILE_SCRIPTED_WITH_ANIMATION = 0x000c8a00u,
    RVA_DO_DIRECT_DAMAGE = 0x000d3ae0u,
    RVA_SET_ANIMATION_SPEED = 0x000e0460u,
    RVA_SCRIPT_CALL_OPCODE = 0x001c4970u,
    RVA_SCRIPT_CALL_OPCODE_SLOT = 0x00323fa0u,
    RVA_SCRIPT_METHOD_OPCODE = 0x001c4b10u,
    RVA_SCRIPT_METHOD_OPCODE_SLOT = 0x00323fa4u,
    RVA_SCRIPT_METHOD_BINDING_CALL = 0x001c4c2fu,
    RVA_SCRIPT_BINDING_INVOKE = 0x002351c0u,
    RVA_CNEW_MISSILE_AIMING_ANIMATION_VTABLE = 0x002d5464u,
    RVA_SCRIPT_MANAGER_GLOBAL = 0x003c3108u,
    RVA_SCRIPT_RUNTIME_GLOBAL = 0x003c310cu,
    PLASMATICA_SCRIPT_START = 0x000aac9fu,
    PLASMATICA_SCRIPT_END = 0x000aaf67u,
    PLASMATICA_IS_PLAYING_IP = 0x000aaea4u,
    PLASMATICA_IS_PLAYING_HASH = 0x890f6eb1u,
    IS_PLAYING_GET_COMPONENT_IP = 0x00003e38u,
    IS_PLAYING_TSA_IS_PLAYING_IP = 0x00003e3du,
    TARGETING_MODE_POLL_IP = 0x00038650u,
    FIRST_ARBITER_EVENT_POLL_IP = 0x000aae2cu,
    SECOND_ARBITER_EVENT_POLL_IP = 0x000aae60u,
    ANIMATION_ACTIVE_POLL_IP = 0x00003f8fu,
    PLASMATICA_PUSH_ANIMATION_IP = 0x00004195u,
    HASH_TSA_PUSH_ANIMATION_STATE = 0xd36ce8f5u,
    HASH_ANIMID_SKILL_02 = 0xb0242a96u,
    HASH_TSA_PLAY_CAMERA = 0xf69c244au,
    HASH_GET_CURRENT_TSA_ANIMATION = 0xb6171bb6u,
    HASH_TEST_CAMERA_COLLISION = 0x8232c4cau,
    HASH_SET_RENDER_CAMERA = 0x61f821bdu,
    HASH_FIRE_MISSILE_SCRIPTED = 0xf907b96bu,
    HASH_DO_DIRECT_DAMAGE = 0xd13c5fe6u,
    HASH_MODIFY_HIT_POINTS = 0x68687a89u,
    HASH_HIT_ENTITY = 0x2d759b0du,
    HASH_CAUSE_POISON = 0x1e18f52au
};

typedef enum TraceExportIndex {
    TRACE_EXPORT_DIRECT_DAMAGE,
    TRACE_EXPORT_FIRE_MISSILE,
    TRACE_EXPORT_FIRE_MISSILE_ANIMATED,
    TRACE_EXPORT_ANIMATION_SPEED,
    TRACE_EXPORT_PLAY_DIRECT_RESOURCE,
    TRACE_EXPORT_PLAY_DIRECT_STRING,
    TRACE_EXPORT_MISSILE_PLAY_DIRECT,
    TRACE_EXPORT_PLAY_STATE_GAME,
    TRACE_EXPORT_PLAY_STATE_MISSILE,
    TRACE_EXPORT_PUSH_DIRECT_RESOURCE,
    TRACE_EXPORT_PUSH_DIRECT_STRING,
    TRACE_EXPORT_MISSILE_PUSH_DIRECT,
    TRACE_EXPORT_PUSH_STATE_GAME,
    TRACE_EXPORT_PUSH_STATE_MISSILE,
    TRACE_EXPORT_COUNT
} TraceExportIndex;

static HMODULE trace_game_module;
static volatile LONG trace_active;
static volatile LONG trace_event_number;
static volatile LONG trace_receiver_poll_count;
static volatile LONG trace_last_stack_after_dispatch = -1;
static volatile LONG trace_primary_thread;
static volatile LONG trace_targeting_poll_count;
static volatile LONG trace_first_arbiter_poll_count;
static volatile LONG trace_second_arbiter_poll_count;
static volatile LONG trace_animation_active_poll_count;
static volatile LONG trace_get_current_animation_count;
static void *trace_skill_object;
static void *trace_animation_object;
static float trace_animation_multiplier = 1.0f;
static float trace_previous_animation_multiplier = 1.0f;
static BOOL trace_animation_speed_applied;
static volatile LONG trace_animation_binding_pending;
static DWORD trace_start_tick;

static SudekiMpRelativeCallHook use_call_hook;
static SudekiMpRelativeCallHook stop_rumble_call_hook;
static SudekiMpExportHook export_hooks[TRACE_EXPORT_COUNT];
static SudekiMpPointerHook script_call_opcode_hook;
static SudekiMpPointerHook script_method_opcode_hook;
static SudekiMpRelativeCallHook script_binding_invoke_hook;

static SkillUseFunction original_skill_use;
static StopRumbleFunction original_stop_rumble;
static AnimationStateFunction original_play_animation_state;
static AnimationDirectStringFunction original_play_direct_string;
static AnimationDirectResourceFunction original_play_direct_resource;
static AnimationStateFunction original_push_animation_state;
static AnimationDirectStringFunction original_push_direct_string;
static AnimationDirectResourceFunction original_push_direct_resource;
static AnimationDirectStringFunction original_missile_play_direct;
static AnimationDirectStringFunction original_missile_push_direct;
static MissileFunction original_fire_missile;
static MissileFunction original_fire_missile_animated;
static FloatFunction original_direct_damage;
static FloatFunction original_animation_speed;
static ScriptCallOpcodeFunction original_script_call_opcode;
static ScriptCallOpcodeFunction original_script_method_opcode;
static ScriptBindingInvokeFunction original_script_binding_invoke;

static LONG next_event(void);
static uint32_t float_bits(float value);
static BOOL read_trace_memory(
    const void *source,
    void *destination,
    SIZE_T size
);

static DWORD trace_elapsed_milliseconds(void) {
    return GetTickCount() - trace_start_tick;
}

static const char *camera_call_name(uint32_t call_hash) {
    switch (call_hash) {
        case HASH_TSA_PLAY_CAMERA:
            return "tsa_play_camera";
        case HASH_GET_CURRENT_TSA_ANIMATION:
            return "get_current_tsa_animation";
        case HASH_SET_RENDER_CAMERA:
            return "set_render_camera";
        default:
            return NULL;
    }
}

static BOOL is_camera_method(uint32_t method_hash) {
    return method_hash == HASH_TEST_CAMERA_COLLISION ||
        method_hash == HASH_SET_RENDER_CAMERA;
}

/*
 * RVA 0x001C4C2F calls the script/native binding dispatcher with a private
 * x86 convention: ECX is the binding record, EAX is the argument count, and
 * three stack arguments are removed by the callee.  The second stack argument
 * is the resolved native object.  A normal C wrapper would lose EAX, so this
 * bridge preserves both registers and reproduces the original stack contract.
 */
static void __attribute__((used, noinline))
trace_animation_binding_object(void *native_object) {
    void *vtable = NULL;
    float previous_multiplier = 0.0f;
    uint32_t previous_bits;
    void *expected_vtable;

    if (InterlockedExchange(&trace_animation_binding_pending, 0) == 0) {
        return;
    }

    expected_vtable = (uint8_t *)trace_game_module +
        RVA_CNEW_MISSILE_AIMING_ANIMATION_VTABLE;
    if (!read_trace_memory(native_object, &vtable, sizeof(vtable))) {
        SudekiMpLogFormat(
            "plasmatica_animation_speed=native_object_read_failed object=0x%08lx error=%lu\r\n",
            (unsigned long)(uintptr_t)native_object,
            (unsigned long)GetLastError()
        );
        return;
    }
    if (vtable != expected_vtable) {
        SudekiMpLogFormat(
            "plasmatica_animation_speed=native_object_rejected object=0x%08lx vtable=0x%08lx expected_vtable=0x%08lx reason=unexpected_vtable\r\n",
            (unsigned long)(uintptr_t)native_object,
            (unsigned long)(uintptr_t)vtable,
            (unsigned long)(uintptr_t)expected_vtable
        );
        return;
    }
    if (!read_trace_memory(
            (const uint8_t *)native_object + 0x48,
            &previous_multiplier,
            sizeof(previous_multiplier))) {
        SudekiMpLogFormat(
            "plasmatica_animation_speed=multiplier_read_failed object=0x%08lx error=%lu\r\n",
            (unsigned long)(uintptr_t)native_object,
            (unsigned long)GetLastError()
        );
        return;
    }

    previous_bits = float_bits(previous_multiplier);
    if ((previous_bits & 0x7f800000u) == 0x7f800000u ||
        previous_multiplier <= 0.0f || previous_multiplier > 16.0f) {
        SudekiMpLogFormat(
            "plasmatica_animation_speed=multiplier_rejected object=0x%08lx previous_bits=0x%08lx\r\n",
            (unsigned long)(uintptr_t)native_object,
            (unsigned long)previous_bits
        );
        return;
    }

    trace_animation_object = native_object;
    trace_previous_animation_multiplier = previous_multiplier;
    original_animation_speed(native_object, NULL, trace_animation_multiplier);
    trace_animation_speed_applied = TRUE;
    SudekiMpLogFormat(
        "plasmatica_animation_speed=applied elapsed_ms=%lu object=0x%08lx vtable=0x%08lx previous_bits=0x%08lx multiplier_bits=0x%08lx animation=ANIMID_SKILL_02\r\n",
        (unsigned long)trace_elapsed_milliseconds(),
        (unsigned long)(uintptr_t)native_object,
        (unsigned long)(uintptr_t)vtable,
        (unsigned long)previous_bits,
        (unsigned long)float_bits(trace_animation_multiplier)
    );
}

static int __attribute__((naked, used)) trace_script_binding_invoke(void) {
    __asm__ __volatile__(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "subl $12, %esp\n\t"
        "movl %eax, -4(%ebp)\n\t"
        "movl %ecx, -8(%ebp)\n\t"
        "pushl 12(%ebp)\n\t"
        "call _trace_animation_binding_object\n\t"
        "addl $4, %esp\n\t"
        "pushl 16(%ebp)\n\t"
        "pushl 12(%ebp)\n\t"
        "pushl 8(%ebp)\n\t"
        "movl -4(%ebp), %eax\n\t"
        "movl -8(%ebp), %ecx\n\t"
        "call *_original_script_binding_invoke\n\t"
        "movl %eax, -12(%ebp)\n\t"
        "movl -12(%ebp), %eax\n\t"
        "leave\n\t"
        "ret $12\n\t"
    );
}

static BOOL is_damage_method(uint32_t method_hash) {
    return method_hash == HASH_DO_DIRECT_DAMAGE ||
        method_hash == HASH_MODIFY_HIT_POINTS ||
        method_hash == HASH_HIT_ENTITY ||
        method_hash == HASH_CAUSE_POISON;
}

static LONG next_repetitive_method_poll(uint32_t instruction_offset) {
    switch (instruction_offset) {
        case TARGETING_MODE_POLL_IP:
            return InterlockedIncrement(&trace_targeting_poll_count);
        case FIRST_ARBITER_EVENT_POLL_IP:
            return InterlockedIncrement(&trace_first_arbiter_poll_count);
        case SECOND_ARBITER_EVENT_POLL_IP:
            return InterlockedIncrement(&trace_second_arbiter_poll_count);
        case ANIMATION_ACTIVE_POLL_IP:
            return InterlockedIncrement(&trace_animation_active_poll_count);
        default:
            return 0;
    }
}

static BOOL read_trace_memory(
    const void *source,
    void *destination,
    SIZE_T size
) {
    SIZE_T bytes_read = 0;
    if (source == NULL || destination == NULL || size == 0) {
        return FALSE;
    }
    return ReadProcessMemory(
        GetCurrentProcess(), source, destination, size, &bytes_read
    ) && bytes_read == size;
}

static void trace_compiled_binding(uint32_t call_hash, LONG poll) {
    uint32_t manager = 0;
    uint32_t table_size = 0;
    uint32_t buckets = 0;
    uint32_t bucket = 0;
    uint32_t entry = 0;
    uint32_t entry_words[4] = {0, 0, 0, 0};
    uint32_t probe;
    BOOL found = FALSE;

    if (poll != 1 || !read_trace_memory(
            (const uint8_t *)trace_game_module + RVA_SCRIPT_MANAGER_GLOBAL,
            &manager,
            sizeof(manager)) || manager == 0) {
        return;
    }

    if (!read_trace_memory(
            (const void *)(uintptr_t)(manager + 0x20u),
            &table_size,
            sizeof(table_size)) || table_size == 0 ||
        !read_trace_memory(
            (const void *)(uintptr_t)(manager + 0x24u),
            &buckets,
            sizeof(buckets)) || buckets == 0) {
        return;
    }
    bucket = call_hash & (table_size - 1u);
    for (probe = 0; probe < table_size; ++probe) {
        uint32_t slot = buckets + bucket * sizeof(uint32_t);
        if (!read_trace_memory(
                (const void *)(uintptr_t)slot, &entry, sizeof(entry)) ||
            entry == 0 || !read_trace_memory(
                (const void *)(uintptr_t)entry,
                entry_words,
                sizeof(entry_words))) {
            break;
        }
        if (entry_words[1] == call_hash) {
            found = TRUE;
            break;
        }
        bucket = (bucket + 1u) & (table_size - 1u);
    }

    SudekiMpLogFormat(
        "skill_trace event=compiled_binding sequence=%ld hash=0x%08lx manager=0x%08lx table_size=0x%08lx buckets=0x%08lx bucket=0x%08lx probes=%lu found=%u entry=0x%08lx entry_words=%08lx,%08lx,%08lx,%08lx bytecode_target=0x%08lx\r\n",
        (long)next_event(),
        (unsigned long)call_hash,
        (unsigned long)manager,
        (unsigned long)table_size,
        (unsigned long)buckets,
        (unsigned long)bucket,
        (unsigned long)(probe + 1u),
        (unsigned int)found,
        (unsigned long)entry,
        (unsigned long)entry_words[0],
        (unsigned long)entry_words[1],
        (unsigned long)entry_words[2],
        (unsigned long)entry_words[3],
        (unsigned long)(found ? entry_words[2] : 0)
    );
}

static int SUDEKIMP_FASTCALL trace_script_method_opcode(
    void *thread,
    void *ignored_edx
) {
    uint32_t instruction_offset = 0;
    uint32_t method_hash = 0;
    uint32_t stack_pointer = 0;
    uint32_t stack_count = 0;
    uint32_t stack_words[4] = {
        0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu
    };
    uint32_t stack_pointer_after = 0;
    uint32_t top_after = 0xffffffffu;
    LONG poll = InterlockedCompareExchange(&trace_receiver_poll_count, 0, 0);
    LONG repetitive_poll = 0;
    BOOL primary_thread = FALSE;
    BOOL damage_method = FALSE;
    BOOL observed_method = FALSE;
    BOOL sampled_wait_call = FALSE;
    BOOL camera_method = FALSE;
    BOOL landmark_method = FALSE;
    BOOL should_log = FALSE;
    BOOL animation_binding_candidate = FALSE;
    int handler_result;

    if (InterlockedCompareExchange(&trace_active, 0, 0) != 0 &&
        thread != NULL) {
        const uint8_t *runtime = *(const uint8_t **)(
            (const uint8_t *)trace_game_module + RVA_SCRIPT_RUNTIME_GLOBAL
        );
        read_trace_memory(
            (const uint8_t *)thread + 0x0c,
            &instruction_offset,
            sizeof(instruction_offset)
        );
        if (runtime != NULL) {
            const uint8_t *bytecode = *(const uint8_t **)(runtime + 0x14);
            if (bytecode != NULL) {
                read_trace_memory(
                    bytecode + instruction_offset,
                    &method_hash,
                    sizeof(method_hash)
                );
            }
        }
        primary_thread = (uint32_t)(uintptr_t)thread ==
            (uint32_t)InterlockedCompareExchange(&trace_primary_thread, 0, 0);
        damage_method = is_damage_method(method_hash);
        if (primary_thread || damage_method) {
            observed_method = TRUE;
            read_trace_memory(
                (const uint8_t *)thread + 0x20,
                &stack_pointer,
                sizeof(stack_pointer)
            );
            read_trace_memory(
                (const uint8_t *)thread + 0x28,
                &stack_count,
                sizeof(stack_count)
            );
            read_trace_memory(
                (const void *)(uintptr_t)stack_pointer,
                stack_words,
                sizeof(stack_words)
            );
            sampled_wait_call =
                instruction_offset == IS_PLAYING_GET_COMPONENT_IP ||
                instruction_offset == IS_PLAYING_TSA_IS_PLAYING_IP;
            camera_method = primary_thread && is_camera_method(method_hash);
            landmark_method = primary_thread &&
                ((instruction_offset == PLASMATICA_PUSH_ANIMATION_IP &&
                    method_hash == HASH_TSA_PUSH_ANIMATION_STATE) ||
                 method_hash == HASH_FIRE_MISSILE_SCRIPTED);
            if (primary_thread) {
                repetitive_poll = next_repetitive_method_poll(
                    instruction_offset
                );
            }
            should_log = damage_method ||
                (sampled_wait_call &&
                    (poll <= 3 || poll % 50 == 0)) ||
                (repetitive_poll != 0 &&
                    (repetitive_poll <= 3 || repetitive_poll % 50 == 0)) ||
                camera_method || landmark_method;
            animation_binding_candidate = primary_thread &&
                instruction_offset == PLASMATICA_PUSH_ANIMATION_IP &&
                method_hash == HASH_TSA_PUSH_ANIMATION_STATE &&
                stack_words[3] == HASH_ANIMID_SKILL_02 &&
                trace_animation_multiplier != 1.0f &&
                !trace_animation_speed_applied;
        }
    }

    (void)ignored_edx;
    if (animation_binding_candidate) {
        InterlockedExchange(&trace_animation_binding_pending, 1);
    }
    handler_result = original_script_method_opcode(thread, NULL);
    if (animation_binding_candidate) {
        InterlockedExchange(&trace_animation_binding_pending, 0);
    }

    if (observed_method) {
        read_trace_memory(
            (const uint8_t *)thread + 0x20,
            &stack_pointer_after,
            sizeof(stack_pointer_after)
        );
        read_trace_memory(
            (const void *)(uintptr_t)stack_pointer_after,
            &top_after,
            sizeof(top_after)
        );
        if (instruction_offset == FIRST_ARBITER_EVENT_POLL_IP ||
            instruction_offset == SECOND_ARBITER_EVENT_POLL_IP) {
            should_log = should_log || top_after != 0;
        } else if (instruction_offset == TARGETING_MODE_POLL_IP ||
                   instruction_offset == ANIMATION_ACTIVE_POLL_IP ||
                   instruction_offset == IS_PLAYING_TSA_IS_PLAYING_IP) {
            should_log = should_log || top_after == 0;
        }
    }

    if (should_log) {
        SudekiMpLogFormat(
            "skill_trace event=%s sequence=%ld elapsed_ms=%lu poll=%ld repetitive_poll=%ld thread=0x%08lx primary=%u ip=0x%08lx hash=0x%08lx stack=0x%08lx count=%lu words=%08lx,%08lx,%08lx,%08lx stack_after=0x%08lx top_after=0x%08lx\r\n",
            damage_method ? "damage_method" : "object_method",
            (long)next_event(),
            (unsigned long)trace_elapsed_milliseconds(),
            (long)poll,
            (long)repetitive_poll,
            (unsigned long)(uintptr_t)thread,
            (unsigned int)primary_thread,
            (unsigned long)instruction_offset,
            (unsigned long)method_hash,
            (unsigned long)stack_pointer,
            (unsigned long)stack_count,
            (unsigned long)stack_words[0],
            (unsigned long)stack_words[1],
            (unsigned long)stack_words[2],
            (unsigned long)stack_words[3],
            (unsigned long)stack_pointer_after,
            (unsigned long)top_after
        );
    }
    return handler_result;
}

static int SUDEKIMP_FASTCALL trace_script_call_opcode(
    void *thread,
    void *ignored_edx
) {
    uint32_t instruction_offset = 0;
    uint32_t call_hash = 0;
    uint32_t stack_pointer = 0;
    uint32_t stack_count = 0;
    uint32_t stack_words[4] = {0, 0, 0, 0};
    uint32_t receiver_words[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t nested_words[4] = {0, 0, 0, 0};
    BOOL receiver_read = FALSE;
    BOOL nested_read = FALSE;
    BOOL primary_thread = FALSE;
    const char *camera_name = NULL;
    LONG camera_poll = 0;
    DWORD camera_before_ms = 0;
    LONG poll = 0;
    int handler_result;

    if (InterlockedCompareExchange(&trace_active, 0, 0) != 0 &&
        thread != NULL) {
        read_trace_memory(
            (const uint8_t *)thread + 0x0c,
            &instruction_offset,
            sizeof(instruction_offset)
        );
        const uint8_t *runtime = *(const uint8_t **)(
            (const uint8_t *)trace_game_module + RVA_SCRIPT_RUNTIME_GLOBAL
        );
        if (runtime != NULL) {
            const uint8_t *bytecode = *(const uint8_t **)(runtime + 0x14);
            if (bytecode != NULL) {
                read_trace_memory(
                    bytecode + instruction_offset,
                    &call_hash,
                    sizeof(call_hash)
                );
            }
        }
    }

    if (instruction_offset >= PLASMATICA_SCRIPT_START &&
        instruction_offset < PLASMATICA_SCRIPT_END) {
        InterlockedCompareExchange(
            &trace_primary_thread,
            (LONG)(uintptr_t)thread,
            0
        );
    }
    primary_thread = thread != NULL &&
        (uint32_t)(uintptr_t)thread ==
        (uint32_t)InterlockedCompareExchange(&trace_primary_thread, 0, 0);
    camera_name = camera_call_name(call_hash);
    if (camera_name != NULL && primary_thread) {
        read_trace_memory(
            (const uint8_t *)thread + 0x20,
            &stack_pointer,
            sizeof(stack_pointer)
        );
        read_trace_memory(
            (const uint8_t *)thread + 0x28,
            &stack_count,
            sizeof(stack_count)
        );
        read_trace_memory(
            (const void *)(uintptr_t)stack_pointer,
            stack_words,
            sizeof(stack_words)
        );
        if (call_hash == HASH_GET_CURRENT_TSA_ANIMATION) {
            camera_poll = InterlockedIncrement(
                &trace_get_current_animation_count
            );
        }
        camera_before_ms = trace_elapsed_milliseconds();
    }

    if (instruction_offset == PLASMATICA_IS_PLAYING_IP &&
        call_hash == PLASMATICA_IS_PLAYING_HASH) {
        poll = InterlockedIncrement(&trace_receiver_poll_count);
        trace_compiled_binding(call_hash, poll);
        read_trace_memory(
            (const uint8_t *)thread + 0x20,
            &stack_pointer,
            sizeof(stack_pointer)
        );
        read_trace_memory(
            (const uint8_t *)thread + 0x28,
            &stack_count,
            sizeof(stack_count)
        );
        read_trace_memory(
            (const void *)(uintptr_t)stack_pointer,
            stack_words,
            sizeof(stack_words)
        );
        receiver_read = read_trace_memory(
            (const void *)(uintptr_t)stack_words[0],
            receiver_words,
            sizeof(receiver_words)
        );
        if (receiver_read) {
            nested_read = read_trace_memory(
                (const void *)(uintptr_t)receiver_words[0],
                nested_words,
                sizeof(nested_words)
            );
        }
    }

    (void)ignored_edx;
    handler_result = original_script_call_opcode(thread, NULL);

    if (camera_name != NULL && primary_thread) {
        uint32_t camera_stack_after = 0;
        uint32_t camera_top_after = 0xffffffffu;

        read_trace_memory(
            (const uint8_t *)thread + 0x20,
            &camera_stack_after,
            sizeof(camera_stack_after)
        );
        read_trace_memory(
            (const void *)(uintptr_t)camera_stack_after,
            &camera_top_after,
            sizeof(camera_top_after)
        );
        SudekiMpLogFormat(
            "skill_trace event=camera_call sequence=%ld name=%s elapsed_before_ms=%lu elapsed_after_ms=%lu poll=%ld thread=0x%08lx ip=0x%08lx hash=0x%08lx stack=0x%08lx count=%lu words=%08lx,%08lx,%08lx,%08lx stack_after=0x%08lx top_after=0x%08lx\r\n",
            (long)next_event(),
            camera_name,
            (unsigned long)camera_before_ms,
            (unsigned long)trace_elapsed_milliseconds(),
            (long)camera_poll,
            (unsigned long)(uintptr_t)thread,
            (unsigned long)instruction_offset,
            (unsigned long)call_hash,
            (unsigned long)stack_pointer,
            (unsigned long)stack_count,
            (unsigned long)stack_words[0],
            (unsigned long)stack_words[1],
            (unsigned long)stack_words[2],
            (unsigned long)stack_words[3],
            (unsigned long)camera_stack_after,
            (unsigned long)camera_top_after
        );
    }

    if (poll != 0) {
        uint32_t result_stack_pointer = 0;
        uint32_t stack_after_dispatch = 0xffffffffu;
        LONG previous_stack_value;
        BOOL should_log;

        read_trace_memory(
            (const uint8_t *)thread + 0x20,
            &result_stack_pointer,
            sizeof(result_stack_pointer)
        );
        read_trace_memory(
            (const void *)(uintptr_t)result_stack_pointer,
            &stack_after_dispatch,
            sizeof(stack_after_dispatch)
        );
        previous_stack_value = InterlockedExchange(
            &trace_last_stack_after_dispatch,
            (LONG)stack_after_dispatch
        );
        should_log = poll <= 3 || poll % 50 == 0 ||
            previous_stack_value != (LONG)stack_after_dispatch;
        if (should_log) {
            SudekiMpLogFormat(
                "skill_trace event=is_playing_receiver sequence=%ld poll=%ld thread=0x%08lx stack=0x%08lx count=%lu arg=0x%08lx stack_after_dispatch=0x%08lx object_read=%u object=%08lx,%08lx,%08lx,%08lx,%08lx,%08lx,%08lx,%08lx nested_read=%u nested=%08lx,%08lx,%08lx,%08lx\r\n",
                (long)next_event(),
                (long)poll,
                (unsigned long)(uintptr_t)thread,
                (unsigned long)stack_pointer,
                (unsigned long)stack_count,
                (unsigned long)stack_words[0],
                (unsigned long)stack_after_dispatch,
                (unsigned int)receiver_read,
                (unsigned long)receiver_words[0],
                (unsigned long)receiver_words[1],
                (unsigned long)receiver_words[2],
                (unsigned long)receiver_words[3],
                (unsigned long)receiver_words[4],
                (unsigned long)receiver_words[5],
                (unsigned long)receiver_words[6],
                (unsigned long)receiver_words[7],
                (unsigned int)nested_read,
                (unsigned long)nested_words[0],
                (unsigned long)nested_words[1],
                (unsigned long)nested_words[2],
                (unsigned long)nested_words[3]
            );
        }
    }
    return handler_result;
}

static BOOL is_plasmatica(const void *skill_data) {
    static const wchar_t expected[] = L"Plasmatica";
    wchar_t actual[sizeof(expected) / sizeof(expected[0])];
    SIZE_T bytes_read = 0;

    if (skill_data == NULL || !ReadProcessMemory(
            GetCurrentProcess(),
            (const uint8_t *)skill_data + 0x14,
            actual,
            sizeof(actual),
            &bytes_read) ||
        bytes_read != sizeof(actual)) {
        return FALSE;
    }
    return memcmp(actual, expected, sizeof(expected)) == 0;
}

static void copy_safe_string(const char *source, char *destination, size_t size) {
    size_t index;
    SIZE_T bytes_read;

    if (destination == NULL || size == 0) {
        return;
    }
    destination[0] = '\0';
    if (source == NULL) {
        return;
    }
    for (index = 0; index + 1 < size; ++index) {
        bytes_read = 0;
        if (!ReadProcessMemory(
                GetCurrentProcess(), source + index, destination + index, 1,
                &bytes_read) || bytes_read != 1) {
            destination[index] = '\0';
            return;
        }
        if (destination[index] == '\0') {
            return;
        }
    }
    destination[size - 1] = '\0';
}

static LONG next_event(void) {
    return InterlockedIncrement(&trace_event_number);
}

static SkillBool SUDEKIMP_FASTCALL trace_skill_use(
    void *self,
    void *ignored_edx,
    int slot
) {
    void *skill_data = NULL;
    SkillBool result;

    if (self != NULL && slot >= 0 && slot < 6) {
        skill_data = *(void **)((uint8_t *)self + 0x3c + (size_t)slot * 4u);
    }
    if (is_plasmatica(skill_data)) {
        trace_skill_object = self;
        trace_animation_object = NULL;
        trace_animation_speed_applied = FALSE;
        InterlockedExchange(&trace_event_number, 0);
        InterlockedExchange(&trace_receiver_poll_count, 0);
        InterlockedExchange(&trace_last_stack_after_dispatch, -1);
        InterlockedExchange(&trace_primary_thread, 0);
        InterlockedExchange(&trace_targeting_poll_count, 0);
        InterlockedExchange(&trace_first_arbiter_poll_count, 0);
        InterlockedExchange(&trace_second_arbiter_poll_count, 0);
        InterlockedExchange(&trace_animation_active_poll_count, 0);
        InterlockedExchange(&trace_get_current_animation_count, 0);
        InterlockedExchange(&trace_animation_binding_pending, 0);
        trace_start_tick = GetTickCount();
        InterlockedExchange(&trace_active, 1);
        SudekiMpLogFormat(
            "skill_trace event=begin elapsed_ms=%lu this=0x%08lx slot=%d skill_data=0x%08lx\r\n",
            (unsigned long)trace_elapsed_milliseconds(),
            (unsigned long)(uintptr_t)self,
            slot,
            (unsigned long)(uintptr_t)skill_data
        );
    }

    (void)ignored_edx;
    result = original_skill_use(self, NULL, slot);
    if (trace_skill_object == self) {
        void *task_handle = *(void **)((uint8_t *)self + 0x74);
        SudekiMpLogFormat(
            "skill_trace event=use_return elapsed_ms=%lu result=%u active=%u task=0x%08lx\r\n",
            (unsigned long)trace_elapsed_milliseconds(),
            (unsigned int)result,
            (unsigned int)*(uint8_t *)((uint8_t *)self + 0x6c),
            (unsigned long)(uintptr_t)task_handle
        );
        if (!result) {
            trace_skill_object = NULL;
            InterlockedExchange(&trace_animation_binding_pending, 0);
            InterlockedExchange(&trace_active, 0);
        }
    }
    return result;
}

static void SUDEKIMP_FASTCALL trace_stop_rumble(void *self, void *ignored_edx) {
    (void)ignored_edx;
    original_stop_rumble(self, NULL);
    if (InterlockedCompareExchange(&trace_active, 0, 0) != 0 &&
        self == trace_skill_object) {
        if (trace_animation_speed_applied && trace_animation_object != NULL) {
            original_animation_speed(
                trace_animation_object,
                NULL,
                trace_previous_animation_multiplier
            );
            SudekiMpLogFormat(
                "plasmatica_animation_speed=restored elapsed_ms=%lu object=0x%08lx multiplier_bits=0x%08lx\r\n",
                (unsigned long)trace_elapsed_milliseconds(),
                (unsigned long)(uintptr_t)trace_animation_object,
                (unsigned long)float_bits(trace_previous_animation_multiplier)
            );
            trace_animation_object = NULL;
            trace_animation_speed_applied = FALSE;
        }
        SudekiMpLogFormat(
            "skill_trace event=end elapsed_ms=%lu this=0x%08lx events=%ld\r\n",
            (unsigned long)trace_elapsed_milliseconds(),
            (unsigned long)(uintptr_t)self,
            (long)InterlockedCompareExchange(&trace_event_number, 0, 0)
        );
        trace_skill_object = NULL;
        InterlockedExchange(&trace_animation_binding_pending, 0);
        InterlockedExchange(&trace_active, 0);
    }
}

static SkillBool log_animation_state(
    const char *event,
    AnimationStateFunction original,
    void *self,
    const char *name,
    uint8_t first,
    uint8_t second
) {
    char safe_name[128];
    if (InterlockedCompareExchange(&trace_active, 0, 0) != 0) {
        copy_safe_string(name, safe_name, sizeof(safe_name));
        SudekiMpLogFormat(
            "skill_trace event=%s sequence=%ld this=0x%08lx name=%s flags=%u,%u\r\n",
            event,
            (long)next_event(),
            (unsigned long)(uintptr_t)self,
            safe_name,
            (unsigned int)first,
            (unsigned int)second
        );
    }
    return original(self, NULL, name, first, second);
}

static SkillBool log_animation_direct_string(
    const char *event,
    AnimationDirectStringFunction original,
    void *self,
    const char *name,
    uint8_t first,
    uint8_t second,
    void *resource_out
) {
    char safe_name[128];
    if (InterlockedCompareExchange(&trace_active, 0, 0) != 0) {
        copy_safe_string(name, safe_name, sizeof(safe_name));
        SudekiMpLogFormat(
            "skill_trace event=%s sequence=%ld this=0x%08lx name=%s flags=%u,%u out=0x%08lx\r\n",
            event,
            (long)next_event(),
            (unsigned long)(uintptr_t)self,
            safe_name,
            (unsigned int)first,
            (unsigned int)second,
            (unsigned long)(uintptr_t)resource_out
        );
    }
    return original(self, NULL, name, first, second, resource_out);
}

static SkillBool log_animation_direct_resource(
    const char *event,
    AnimationDirectResourceFunction original,
    void *self,
    const void *resource,
    uint8_t first,
    uint8_t second,
    void *resource_out
) {
    if (InterlockedCompareExchange(&trace_active, 0, 0) != 0) {
        SudekiMpLogFormat(
            "skill_trace event=%s sequence=%ld this=0x%08lx resource=0x%08lx flags=%u,%u out=0x%08lx\r\n",
            event,
            (long)next_event(),
            (unsigned long)(uintptr_t)self,
            (unsigned long)(uintptr_t)resource,
            (unsigned int)first,
            (unsigned int)second,
            (unsigned long)(uintptr_t)resource_out
        );
    }
    return original(self, NULL, resource, first, second, resource_out);
}

#define DEFINE_STATE_WRAPPER(name, label, original) \
    static SkillBool SUDEKIMP_FASTCALL name( \
        void *self, void *ignored_edx, const char *animation, \
        uint8_t first, uint8_t second \
    ) { \
        (void)ignored_edx; \
        return log_animation_state( \
            label, original, self, animation, first, second \
        ); \
    }

#define DEFINE_STRING_WRAPPER(name, label, original) \
    static SkillBool SUDEKIMP_FASTCALL name( \
        void *self, void *ignored_edx, const char *animation, \
        uint8_t first, uint8_t second, void *resource_out \
    ) { \
        (void)ignored_edx; \
        return log_animation_direct_string( \
            label, original, self, animation, first, second, resource_out \
        ); \
    }

#define DEFINE_RESOURCE_WRAPPER(name, label, original) \
    static SkillBool SUDEKIMP_FASTCALL name( \
        void *self, void *ignored_edx, const void *resource, \
        uint8_t first, uint8_t second, void *resource_out \
    ) { \
        (void)ignored_edx; \
        return log_animation_direct_resource( \
            label, original, self, resource, first, second, resource_out \
        ); \
    }

DEFINE_STATE_WRAPPER(trace_play_state, "play_animation_state", original_play_animation_state)
DEFINE_STATE_WRAPPER(trace_push_state, "push_animation_state", original_push_animation_state)
DEFINE_STRING_WRAPPER(trace_play_direct_string, "play_animation_direct", original_play_direct_string)
DEFINE_STRING_WRAPPER(trace_push_direct_string, "push_animation_direct", original_push_direct_string)
DEFINE_STRING_WRAPPER(trace_missile_play_direct, "missile_play_animation_direct", original_missile_play_direct)
DEFINE_STRING_WRAPPER(trace_missile_push_direct, "missile_push_animation_direct", original_missile_push_direct)
DEFINE_RESOURCE_WRAPPER(trace_play_direct_resource, "play_animation_resource", original_play_direct_resource)
DEFINE_RESOURCE_WRAPPER(trace_push_direct_resource, "push_animation_resource", original_push_direct_resource)

static SkillBool SUDEKIMP_FASTCALL trace_fire_missile(
    void *self,
    void *ignored_edx,
    int missile_id
) {
    (void)ignored_edx;
    if (InterlockedCompareExchange(&trace_active, 0, 0) != 0) {
        SudekiMpLogFormat(
            "skill_trace event=fire_missile sequence=%ld this=0x%08lx missile_id=%d\r\n",
            (long)next_event(), (unsigned long)(uintptr_t)self, missile_id
        );
    }
    return original_fire_missile(self, NULL, missile_id);
}

static SkillBool SUDEKIMP_FASTCALL trace_fire_missile_animated(
    void *self,
    void *ignored_edx,
    int missile_id
) {
    (void)ignored_edx;
    if (InterlockedCompareExchange(&trace_active, 0, 0) != 0) {
        SudekiMpLogFormat(
            "skill_trace event=fire_missile_animated sequence=%ld this=0x%08lx missile_id=%d\r\n",
            (long)next_event(), (unsigned long)(uintptr_t)self, missile_id
        );
    }
    return original_fire_missile_animated(self, NULL, missile_id);
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void SUDEKIMP_FASTCALL trace_direct_damage(
    void *self,
    void *ignored_edx,
    float amount
) {
    (void)ignored_edx;
    if (InterlockedCompareExchange(&trace_active, 0, 0) != 0) {
        SudekiMpLogFormat(
            "skill_trace event=direct_damage sequence=%ld this=0x%08lx amount_bits=0x%08lx\r\n",
            (long)next_event(),
            (unsigned long)(uintptr_t)self,
            (unsigned long)float_bits(amount)
        );
    }
    original_direct_damage(self, NULL, amount);
}

static void SUDEKIMP_FASTCALL trace_animation_speed(
    void *self,
    void *ignored_edx,
    float multiplier
) {
    (void)ignored_edx;
    if (InterlockedCompareExchange(&trace_active, 0, 0) != 0) {
        SudekiMpLogFormat(
            "skill_trace event=animation_speed sequence=%ld this=0x%08lx multiplier_bits=0x%08lx\r\n",
            (long)next_event(),
            (unsigned long)(uintptr_t)self,
            (unsigned long)float_bits(multiplier)
        );
    }
    original_animation_speed(self, NULL, multiplier);
}

typedef struct ExportHookSpec {
    uint32_t slot_rva;
    uint32_t expected_function_rva;
    const void *replacement;
} ExportHookSpec;

static BOOL install_export_hooks(void) {
    const ExportHookSpec specs[TRACE_EXPORT_COUNT] = {
        {0x0030c570u, RVA_DO_DIRECT_DAMAGE, trace_direct_damage},
        {0x0030c698u, RVA_FIRE_MISSILE_SCRIPTED, trace_fire_missile},
        {0x0030c69cu, RVA_FIRE_MISSILE_SCRIPTED_WITH_ANIMATION, trace_fire_missile_animated},
        {0x0030ceb4u, RVA_SET_ANIMATION_SPEED, trace_animation_speed},
        {0x0030d3d0u, RVA_PLAY_ANIMATION_DIRECT_RESOURCE, trace_play_direct_resource},
        {0x0030d3d4u, RVA_PLAY_ANIMATION_DIRECT_STRING, trace_play_direct_string},
        {0x0030d3d8u, RVA_MISSILE_PLAY_ANIMATION_DIRECT, trace_missile_play_direct},
        {0x0030d3dcu, RVA_PLAY_ANIMATION_STATE, trace_play_state},
        {0x0030d3e0u, RVA_PLAY_ANIMATION_STATE, trace_play_state},
        {0x0030d3e4u, RVA_PUSH_ANIMATION_DIRECT_RESOURCE, trace_push_direct_resource},
        {0x0030d3e8u, RVA_PUSH_ANIMATION_DIRECT_STRING, trace_push_direct_string},
        {0x0030d3ecu, RVA_MISSILE_PUSH_ANIMATION_DIRECT, trace_missile_push_direct},
        {0x0030d3f0u, RVA_PUSH_ANIMATION_STATE, trace_push_state},
        {0x0030d3f4u, RVA_PUSH_ANIMATION_STATE, trace_push_state}
    };
    size_t index;

    for (index = 0; index < TRACE_EXPORT_COUNT; ++index) {
        if (!SudekiMpInstallExportHook(
                &export_hooks[index],
                trace_game_module,
                specs[index].slot_rva,
                specs[index].expected_function_rva,
                specs[index].replacement)) {
            return FALSE;
        }
    }
    return TRUE;
}

BOOL SudekiMpInstallSkillTrace(HMODULE game_module, float plasmatica_speed) {
    uint8_t *base;

    if (game_module == NULL || trace_game_module != NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    trace_game_module = game_module;
    trace_animation_multiplier = plasmatica_speed;
    trace_start_tick = 0;
    base = (uint8_t *)game_module;

    original_skill_use = (SkillUseFunction)(base + RVA_CSKILL_USE);
    original_stop_rumble = (StopRumbleFunction)(base + RVA_CSKILL_STOP_RUMBLE);
    original_play_animation_state = (AnimationStateFunction)(base + RVA_PLAY_ANIMATION_STATE);
    original_play_direct_string = (AnimationDirectStringFunction)(base + RVA_PLAY_ANIMATION_DIRECT_STRING);
    original_play_direct_resource = (AnimationDirectResourceFunction)(base + RVA_PLAY_ANIMATION_DIRECT_RESOURCE);
    original_push_animation_state = (AnimationStateFunction)(base + RVA_PUSH_ANIMATION_STATE);
    original_push_direct_string = (AnimationDirectStringFunction)(base + RVA_PUSH_ANIMATION_DIRECT_STRING);
    original_push_direct_resource = (AnimationDirectResourceFunction)(base + RVA_PUSH_ANIMATION_DIRECT_RESOURCE);
    original_missile_play_direct = (AnimationDirectStringFunction)(base + RVA_MISSILE_PLAY_ANIMATION_DIRECT);
    original_missile_push_direct = (AnimationDirectStringFunction)(base + RVA_MISSILE_PUSH_ANIMATION_DIRECT);
    original_fire_missile = (MissileFunction)(base + RVA_FIRE_MISSILE_SCRIPTED);
    original_fire_missile_animated = (MissileFunction)(base + RVA_FIRE_MISSILE_SCRIPTED_WITH_ANIMATION);
    original_direct_damage = (FloatFunction)(base + RVA_DO_DIRECT_DAMAGE);
    original_animation_speed = (FloatFunction)(base + RVA_SET_ANIMATION_SPEED);
    original_script_call_opcode = (ScriptCallOpcodeFunction)(
        base + RVA_SCRIPT_CALL_OPCODE
    );
    original_script_method_opcode = (ScriptCallOpcodeFunction)(
        base + RVA_SCRIPT_METHOD_OPCODE
    );
    original_script_binding_invoke = (ScriptBindingInvokeFunction)(
        base + RVA_SCRIPT_BINDING_INVOKE
    );

    if (!SudekiMpInstallRelativeCallHook(
            &use_call_hook,
            base + RVA_CSKILL_USE_CALL,
            original_skill_use,
            trace_skill_use) ||
        !SudekiMpInstallRelativeCallHook(
            &stop_rumble_call_hook,
            base + RVA_CSKILL_STOP_RUMBLE_CALL,
            original_stop_rumble,
            trace_stop_rumble) ||
        !SudekiMpInstallPointerHook(
            &script_call_opcode_hook,
            (void **)(base + RVA_SCRIPT_CALL_OPCODE_SLOT),
            original_script_call_opcode,
            trace_script_call_opcode) ||
        !SudekiMpInstallPointerHook(
            &script_method_opcode_hook,
            (void **)(base + RVA_SCRIPT_METHOD_OPCODE_SLOT),
            original_script_method_opcode,
            trace_script_method_opcode) ||
        !SudekiMpInstallRelativeCallHook(
            &script_binding_invoke_hook,
            base + RVA_SCRIPT_METHOD_BINDING_CALL,
            original_script_binding_invoke,
            trace_script_binding_invoke) ||
        !install_export_hooks()) {
        SudekiMpUninstallSkillTrace();
        return FALSE;
    }

    SudekiMpLogWrite("skill_trace_install=success\r\n");
    return TRUE;
}

void SudekiMpUninstallSkillTrace(void) {
    size_t index;

    for (index = TRACE_EXPORT_COUNT; index > 0; --index) {
        SudekiMpRestoreExportHook(&export_hooks[index - 1]);
    }
    SudekiMpRestoreRelativeCallHook(&script_binding_invoke_hook);
    SudekiMpRestorePointerHook(&script_method_opcode_hook);
    SudekiMpRestorePointerHook(&script_call_opcode_hook);
    SudekiMpRestoreRelativeCallHook(&stop_rumble_call_hook);
    SudekiMpRestoreRelativeCallHook(&use_call_hook);
    trace_skill_object = NULL;
    trace_animation_object = NULL;
    trace_animation_speed_applied = FALSE;
    trace_animation_multiplier = 1.0f;
    InterlockedExchange(&trace_animation_binding_pending, 0);
    InterlockedExchange(&trace_primary_thread, 0);
    InterlockedExchange(&trace_active, 0);
    trace_start_tick = 0;
    trace_game_module = NULL;
}
