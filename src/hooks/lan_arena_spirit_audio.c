#include "hooks/lan_arena_spirit_audio.h"

#include "engine/log.h"
#include "hooks/call_hook.h"

#include <stdint.h>
#include <string.h>

enum {
    RVA_SOUND_PLAY_CUE = 0x00017090u,
    RVA_SOUND_GET = 0x000170b0u,
    RVA_SOUND_GLOBAL = 0x00408d40u,
    SOUND_PLAY_CUE_HOOK_LENGTH = 6u
};

static const uint8_t expected_sound_play_cue_body[] = {
    0x8bu, 0x44u, 0x24u, 0x04u,
    0x6au, 0x00u, 0x6au, 0x00u, 0x6au, 0x01u, 0x6au, 0x00u,
    0x50u, 0x8bu, 0x41u, 0x18u,
    0xe8u, 0x0bu, 0x3eu, 0x27u, 0x00u,
    0xc2u, 0x04u, 0x00u
};

typedef void *(__attribute__((cdecl)) *SoundGetFunction)(void);

static SudekiMpInlineHook sound_play_cue_hook;
static SudekiMpLanArenaSpiritActiveWitness spirit_active_witness;
static void *spirit_active_witness_context;
static HMODULE trace_game_module;
static SudekiMpLanArenaSpiritAudioEvent
    observed_events[SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY];
static CRITICAL_SECTION event_lock;
static BOOL event_lock_initialized;
static uint32_t event_count;
static uint32_t event_write_index;
static uint32_t event_dropped_count;
static uint32_t event_sequence;
static DWORD trace_started_at_ms;
static BOOL trace_clock_started;
static volatile LONG trace_admission;
static volatile LONG binding_generation;
static BOOL trace_module_pinned;

static BOOL read_cue_byte(
    const char *cue,
    size_t index,
    unsigned char *value
) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t base;
    uintptr_t address;
    DWORD protection;
    SIZE_T bytes_read = 0u;
    if (cue == NULL || value == NULL) return FALSE;
    base = (uintptr_t)cue;
    address = base + index;
    if (address < base) return FALSE;
    if (VirtualQuery(
            (const void *)address,
            &information,
            sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & PAGE_GUARD) != 0u) return FALSE;
    protection = information.Protect & 0xffu;
    if (protection != PAGE_READONLY &&
        protection != PAGE_READWRITE &&
        protection != PAGE_WRITECOPY &&
        protection != PAGE_EXECUTE_READ &&
        protection != PAGE_EXECUTE_READWRITE &&
        protection != PAGE_EXECUTE_WRITECOPY) return FALSE;
    return ReadProcessMemory(
            GetCurrentProcess(),
            (const void *)address,
            value,
            1u,
            &bytes_read) &&
        bytes_read == 1u;
}

static BOOL readable_region(const void *pointer, size_t length) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t address = (uintptr_t)pointer;
    uintptr_t end;
    uintptr_t region_end;
    DWORD protection;
    if (pointer == NULL || length == 0u || address > UINTPTR_MAX - length) {
        return FALSE;
    }
    end = address + length;
    if (VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & PAGE_GUARD) != 0u) return FALSE;
    protection = information.Protect & 0xffu;
    if (protection != PAGE_READONLY && protection != PAGE_READWRITE &&
        protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ &&
        protection != PAGE_EXECUTE_READWRITE &&
        protection != PAGE_EXECUTE_WRITECOPY) return FALSE;
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    return region_end >= (uintptr_t)information.BaseAddress && end <= region_end;
}

static BOOL copy_readable_cue(
    const char *cue,
    char output[SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_CUE_CAPACITY],
    uint8_t *length
) {
    size_t index;
    unsigned char value;
    if (cue == NULL || output == NULL || length == NULL) return FALSE;
    for (index = 0u;
         index < SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_CUE_CAPACITY;
         ++index) {
        if (!read_cue_byte(cue, index, &value)) return FALSE;
        if (value == 0u) {
            if (index == 0u) return FALSE;
            output[index] = '\0';
            *length = (uint8_t)index;
            return TRUE;
        }
        /* Refuse control characters and log delimiters. This keeps the
         * observation readable even when PlayCue receives a bad pointer or
         * a non-name buffer. Valid XACT cue names are printable ASCII. */
        if (value < 0x20u || value > 0x7eu || value == '=' ||
            value == '\\' || value == '"') return FALSE;
        output[index] = (char)value;
    }
    return FALSE;
}

static uint32_t next_event_sequence(void) {
    ++event_sequence;
    if (event_sequence == 0u) ++event_sequence;
    return event_sequence;
}

static LONG advance_binding_generation_locked(void) {
    LONG generation = (LONG)((uint32_t)binding_generation + 1u);
    if (generation == 0) generation = 1;
    InterlockedExchange(&binding_generation, generation);
    return generation;
}

static void reset_event_ring_locked(void) {
    ZeroMemory(observed_events, sizeof(observed_events));
    event_count = 0u;
    event_write_index = 0u;
    event_dropped_count = 0u;
    event_sequence = 0u;
    trace_started_at_ms = 0u;
    trace_clock_started = FALSE;
}

static BOOL record_cue_locked(
    const char *cue,
    int native_state,
    SudekiMpLanArenaSpiritAudioEvent *logged_event
) {
    SudekiMpLanArenaSpiritAudioEvent event;
    uint32_t write_index;
    DWORD now_ms;

    ZeroMemory(&event, sizeof(event));
    if (!copy_readable_cue(cue, event.cue, &event.cue_length)) return FALSE;
    now_ms = GetTickCount();

    if (!trace_clock_started) {
        trace_started_at_ms = now_ms;
        trace_clock_started = TRUE;
    }
    event.sequence = next_event_sequence();
    event.elapsed_ms = (uint32_t)(now_ms - trace_started_at_ms);
    event.native_state = native_state;
    write_index = event_write_index;
    observed_events[write_index] = event;
    event_write_index = (write_index + 1u) %
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY;
    if (event_count < SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY) {
        ++event_count;
    } else {
        ++event_dropped_count;
    }
    if (logged_event != NULL) *logged_event = event;
    return TRUE;
}

static void log_recorded_cue(
    const SudekiMpLanArenaSpiritAudioEvent *event
) {
    if (event == NULL) return;
    SudekiMpLogFormat(
        "lan_arena_spirit_audio event=cue state=observed sequence=%lu "
        "elapsed_ms=%lu native_state=%d cue=%s "
        "policy=host_only_bounded_observation_no_replay\r\n",
        (unsigned long)event->sequence,
        (unsigned long)event->elapsed_ms,
        (int)event->native_state,
        event->cue);
}

static void invoke_sound_play_cue_entry(
    void *sound,
    const char *cue,
    void *entry
) {
    uintptr_t this_register = (uintptr_t)sound;
    if (entry == NULL) return;
    /* CSound::PlayCue is native x86 thiscall: ECX owns `sound`, its one stack
     * argument is callee-cleaned. Keep this bridge explicit because GCC's C
     * frontend does not reliably honor `thiscall` on a non-C++ method. */
    __asm__ volatile(
        "pushl %1\n\t"
        "call *%2"
        : "+c"(this_register)
        : "r"(cue), "r"(entry)
        : "eax", "edx", "memory", "cc");
}

static void invoke_original_sound_play_cue(void *sound, const char *cue) {
    invoke_sound_play_cue_entry(
        sound, cue, sound_play_cue_hook.trampoline);
}

static const char *semantic_cue_name(SudekiMpLanArenaSpiritAudioCue cue) {
    if (cue == SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START) {
        return "spiritstrike_start";
    }
    return NULL;
}

BOOL SudekiMpLanArenaSpiritAudioReplayWithApi(
    SudekiMpLanArenaSpiritAudioCue cue,
    const SudekiMpLanArenaSpiritAudioReplayApi *api
) {
    const char *cue_name = semantic_cue_name(cue);
    void *sound;
    if (api == NULL || api->get_sound == NULL || api->play_cue == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (cue_name == NULL) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    sound = api->get_sound(api->context);
    if (sound == NULL) {
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }
    if (!api->play_cue(api->context, sound, cue_name)) {
        if (GetLastError() == ERROR_SUCCESS) SetLastError(ERROR_GEN_FAILURE);
        return FALSE;
    }
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL SudekiMpLanArenaSpiritAudioReplayImageMatches(HMODULE game_module) {
    const uint8_t *base = (const uint8_t *)game_module;
    uint32_t sound_global_operand;
    uint32_t expected_sound_global;
    if (base == NULL || base[RVA_SOUND_GET] != 0xa1u ||
        base[RVA_SOUND_GET + 5u] != 0xc3u) return FALSE;
    memcpy(&sound_global_operand, base + RVA_SOUND_GET + 1u,
        sizeof(sound_global_operand));
    expected_sound_global = (uint32_t)(uintptr_t)(
        base + RVA_SOUND_GLOBAL);
    return sound_global_operand == expected_sound_global &&
        memcmp(base + RVA_SOUND_PLAY_CUE,
            expected_sound_play_cue_body,
            sizeof(expected_sound_play_cue_body)) == 0;
}

static void *native_get_sound(void *context) {
    uint8_t *base = (uint8_t *)context;
    SoundGetFunction get_sound =
        (SoundGetFunction)(base + RVA_SOUND_GET);
    return get_sound();
}

static BOOL native_play_cue(
    void *context,
    void *sound,
    const char *cue
) {
    uint8_t *base = (uint8_t *)context;
    void *native_audio;
    if (base == NULL || !readable_region(sound, 0x1cu)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    native_audio = *(void **)((uint8_t *)sound + 0x18u);
    if (native_audio == NULL) {
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }
    invoke_sound_play_cue_entry(sound, cue, base + RVA_SOUND_PLAY_CUE);
    return TRUE;
}

BOOL SudekiMpLanArenaSpiritAudioReplayLocalCue(
    HMODULE game_module,
    SudekiMpLanArenaSpiritAudioCue cue
) {
    SudekiMpLanArenaSpiritAudioReplayApi api;
    if (!SudekiMpLanArenaSpiritAudioReplayImageMatches(game_module)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    api.context = (void *)game_module;
    api.get_sound = native_get_sound;
    api.play_cue = native_play_cue;
    return SudekiMpLanArenaSpiritAudioReplayWithApi(cue, &api);
}

static void __attribute__((fastcall)) observe_sound_play_cue(
    void *sound,
    void *unused_edx,
    const char *cue
) {
    SudekiMpLanArenaSpiritAudioEvent logged_event;
    LONG captured_generation;
    BOOL captured_admission;
    int native_state = 0;
    BOOL active = FALSE;
    BOOL recorded = FALSE;

    (void)unused_edx;
    captured_generation = InterlockedCompareExchange(
        &binding_generation, 0, 0);
    captured_admission = InterlockedCompareExchange(
        &trace_admission, 0, 0) != 0;
    if (captured_admission) {
        EnterCriticalSection(&event_lock);
        /* An old callback may have sampled admission immediately before a
         * logical unbind/rebind. The generation check makes that callback
         * ineligible for the new ring even though the permanent detour still
         * forwards it to native audio. */
        if (InterlockedCompareExchange(&trace_admission, 0, 0) != 0 &&
            captured_generation == InterlockedCompareExchange(
                &binding_generation, 0, 0) &&
            spirit_active_witness != NULL) {
            active = spirit_active_witness(
                spirit_active_witness_context, &native_state);
            if (active && native_state != 0) {
                recorded = record_cue_locked(
                    cue, native_state, &logged_event);
            }
        }
        LeaveCriticalSection(&event_lock);
    }
    invoke_original_sound_play_cue(sound, cue);
    /* Disk-backed trace logging must never delay Sudeki's native cue submit. */
    if (recorded) log_recorded_cue(&logged_event);
}

BOOL SudekiMpLanArenaSpiritAudioTraceImageMatches(HMODULE game_module) {
    const uint8_t *base = (const uint8_t *)game_module;
    if (base == NULL) return FALSE;
    return memcmp(base + RVA_SOUND_PLAY_CUE,
        expected_sound_play_cue_body,
        sizeof(expected_sound_play_cue_body)) == 0;
}

BOOL SudekiMpInstallLanArenaSpiritAudioTrace(
    HMODULE game_module,
    SudekiMpLanArenaSpiritActiveWitness active_witness,
    void *witness_context
) {
    uint8_t *base = (uint8_t *)game_module;
    HMODULE pinned_module = NULL;
    DWORD error;
    BOOL first_physical_install;
    if (base == NULL || active_witness == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    first_physical_install = !sound_play_cue_hook.installed;
    if (first_physical_install &&
        !SudekiMpLanArenaSpiritAudioTraceImageMatches(game_module)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (!first_physical_install &&
        (trace_game_module != game_module ||
         sound_play_cue_hook.target != base + RVA_SOUND_PLAY_CUE)) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (!trace_module_pinned) {
        if (!GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_PIN,
                (LPCSTR)(uintptr_t)&SudekiMpInstallLanArenaSpiritAudioTrace,
                &pinned_module)) {
            return FALSE;
        }
        trace_module_pinned = TRUE;
    }
    if (!event_lock_initialized) {
        InitializeCriticalSection(&event_lock);
        event_lock_initialized = TRUE;
    }

    EnterCriticalSection(&event_lock);
    if (InterlockedCompareExchange(&trace_admission, 0, 0) != 0) {
        LeaveCriticalSection(&event_lock);
        SetLastError(ERROR_ALREADY_EXISTS);
        return FALSE;
    }
    InterlockedExchange(&trace_admission, 0);
    reset_event_ring_locked();
    (void)advance_binding_generation_locked();
    spirit_active_witness = active_witness;
    spirit_active_witness_context = witness_context;

    if (first_physical_install && !SudekiMpInstallInlineHook(
            &sound_play_cue_hook,
            base + RVA_SOUND_PLAY_CUE,
            expected_sound_play_cue_body,
            SOUND_PLAY_CUE_HOOK_LENGTH,
            observe_sound_play_cue)) {
        error = GetLastError();
        LeaveCriticalSection(&event_lock);
        SetLastError(error);
        return FALSE;
    }
    if (first_physical_install) trace_game_module = game_module;
    /* Publication is last. The permanent lock was initialized and the hook's
     * trampoline field was populated before target bytes became reachable. */
    InterlockedExchange(&trace_admission, 1);
    LeaveCriticalSection(&event_lock);
    SudekiMpLogFormat(
        "lan_arena_spirit_audio event=%s state=active "
        "rva=0x00017090 capacity=64 cue_capacity=96 "
        "physical_hook=process_lifetime_pinned "
        "policy=host_only_exact_image_observation_no_native_spirit_calls\r\n",
        first_physical_install ? "install" : "rebind");
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL SudekiMpUninstallLanArenaSpiritAudioTrace(void) {
    if (!sound_play_cue_hook.installed || !event_lock_initialized) {
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    EnterCriticalSection(&event_lock);
    if (InterlockedCompareExchange(&trace_admission, 0, 0) != 0) {
        /* This is a logical unbind only. The process-lifetime pinned detour,
         * trampoline, lock, ring, and old witness/context remain valid so an
         * entrant delayed before the lock can always forward safely. */
        InterlockedExchange(&trace_admission, 0);
        (void)advance_binding_generation_locked();
    }
    LeaveCriticalSection(&event_lock);
    SudekiMpLogWrite(
        "lan_arena_spirit_audio event=unbind state=inactive "
        "physical_hook=process_lifetime_pinned passthrough=native "
        "policy=no_runtime_trampoline_or_lock_reclamation\r\n");
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL SudekiMpLanArenaSpiritAudioTraceInstalled(void) {
    return InterlockedCompareExchange(&trace_admission, 0, 0) != 0;
}

size_t SudekiMpLanArenaSpiritAudioTraceSnapshot(
    SudekiMpLanArenaSpiritAudioEvent *events,
    size_t capacity,
    uint32_t *dropped_count
) {
    size_t available;
    size_t copied;
    size_t start;
    size_t index;
    if (!event_lock_initialized || (events == NULL && capacity != 0u)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0u;
    }
    EnterCriticalSection(&event_lock);
    available = (size_t)event_count;
    copied = capacity < available ? capacity : available;
    start = (event_write_index +
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY - event_count) %
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY;
    if (copied < available) {
        start = (start + available - copied) %
            SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY;
    }
    for (index = 0u; index < copied; ++index) {
        events[index] = observed_events[(start + index) %
            SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY];
    }
    if (dropped_count != NULL) *dropped_count = event_dropped_count;
    LeaveCriticalSection(&event_lock);
    SetLastError(ERROR_SUCCESS);
    return copied;
}
