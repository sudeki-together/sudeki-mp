#include "hooks/lan_arena_collision_debug.h"

#include "engine/build_identity.h"
#include "engine/log.h"

#include <stdint.h>
#include <string.h>

typedef void (__cdecl *NativeCollisionDebugToggleFunction)(void);

enum {
    RVA_TOGGLE_COLLISION_SPHERES = 0x000e6a10u,
    RVA_TOGGLE_COLLISION_MESH = 0x000e6a20u,
    RVA_TOGGLE_SPHERE_TREES = 0x000e6a30u,
    RVA_COLLISION_SPHERES_FLAG = 0x003c2fdbu,
    RVA_COLLISION_MESH_FLAG = 0x003c2fe0u,
    RVA_SPHERE_TREES_FLAG = 0x003c2fe1u,
    COLLISION_DEBUG_FLAG_COUNT = 3u
};

typedef struct CollisionDebugBinding {
    NativeCollisionDebugToggleFunction toggle;
    uint8_t *flag;
} CollisionDebugBinding;

static uint8_t *game_base;
static CollisionDebugBinding bindings[COLLISION_DEBUG_FLAG_COUNT];
static uint8_t original_flags[COLLISION_DEBUG_FLAG_COUNT];
static unsigned int active_mode;
static BOOL f9_was_down;
static BOOL operational;

static BOOL reject_install(
    DWORD error,
    const char *reason,
    unsigned int binding_index
) {
    SudekiMpLogFormat(
        "lan_arena_collision_debug event=install result=rejected "
        "reason=%s binding=%u error=%lu "
        "policy=fail_closed_no_debug_state_retained\r\n",
        reason == NULL ? "unknown" : reason,
        binding_index,
        (unsigned long)error);
    SetLastError(error);
    return FALSE;
}

static BOOL readable_memory(const void *pointer, size_t length) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start = (uintptr_t)pointer;
    uintptr_t finish = start + length;
    DWORD protection;

    if (pointer == NULL || length == 0u || finish < start ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0u ||
        finish > (uintptr_t)information.BaseAddress + information.RegionSize) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    return protection == PAGE_READONLY || protection == PAGE_READWRITE ||
        protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE ||
        protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

static BOOL writable_memory(void *pointer, size_t length) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start = (uintptr_t)pointer;
    uintptr_t finish = start + length;
    DWORD protection;

    if (!readable_memory(pointer, length) ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        finish > (uintptr_t)information.BaseAddress + information.RegionSize) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    return protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

static BOOL executable_memory(const void *pointer, size_t length) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start = (uintptr_t)pointer;
    uintptr_t finish = start + length;
    DWORD protection;

    if (!readable_memory(pointer, length) ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        finish > (uintptr_t)information.BaseAddress + information.RegionSize) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

static BOOL supported_module_header(uint8_t *base) {
    const IMAGE_DOS_HEADER *dos;
    const IMAGE_NT_HEADERS32 *nt;

    if (!readable_memory(base, sizeof(IMAGE_DOS_HEADER))) return FALSE;
    dos = (const IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        (uint32_t)dos->e_lfanew > SUDEKIMP_EXPECTED_IMAGE_SIZE -
            sizeof(IMAGE_NT_HEADERS32) ||
        !readable_memory(base + dos->e_lfanew,
            sizeof(IMAGE_NT_HEADERS32))) return FALSE;
    nt = (const IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE &&
        nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
        nt->FileHeader.TimeDateStamp == SUDEKIMP_EXPECTED_TIMESTAMP &&
        nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
        nt->OptionalHeader.SizeOfImage == SUDEKIMP_EXPECTED_IMAGE_SIZE;
}

static BOOL native_toggle_signature(
    uint8_t *entry,
    uint8_t *expected_flag
) {
    static const uint8_t stable_prefix[] = {0x80u, 0x3du};
    static const uint8_t stable_middle[] = {
        0x00u, 0x0fu, 0x94u, 0xc0u, 0xa2u
    };
    uint32_t first_operand;
    uint32_t second_operand;
    uint32_t expected_operand = (uint32_t)(uintptr_t)expected_flag;

    if (!executable_memory(entry, 16u) ||
        memcmp(entry, stable_prefix, sizeof(stable_prefix)) != 0 ||
        memcmp(entry + 6u, stable_middle, sizeof(stable_middle)) != 0 ||
        entry[15] != 0xc3u) return FALSE;
    memcpy(&first_operand, entry + 2u, sizeof(first_operand));
    memcpy(&second_operand, entry + 11u, sizeof(second_operand));
    return first_operand == expected_operand &&
        second_operand == expected_operand;
}

static BOOL set_native_flag(unsigned int index, uint8_t wanted) {
    CollisionDebugBinding *binding;

    if (index >= COLLISION_DEBUG_FLAG_COUNT || wanted > 1u) return FALSE;
    binding = &bindings[index];
    if (binding->toggle == NULL || !writable_memory(binding->flag, 1u) ||
        *binding->flag > 1u) return FALSE;
    if (*binding->flag == wanted) return TRUE;
    binding->toggle();
    return *binding->flag == wanted;
}

static BOOL mode_flags(unsigned int mode, uint8_t wanted[3]) {
    if (wanted == NULL || mode >
            SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_SPHERES_AND_MESH) {
        return FALSE;
    }
    wanted[0] = mode != SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF ? 1u : 0u;
    wanted[1] = mode ==
        SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_SPHERES_AND_MESH ? 1u : 0u;
    wanted[2] = 0u;
    return TRUE;
}

static const char *mode_name(unsigned int mode) {
    switch (mode) {
    case SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF:
        return "off";
    case SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_SPHERES:
        return "spheres";
    case SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_SPHERES_AND_MESH:
        return "spheres_and_mesh";
    default:
        return "invalid";
    }
}

static unsigned int next_mode(unsigned int mode) {
    if (mode == SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF) {
        return SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_SPHERES;
    }
    if (mode == SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_SPHERES) {
        return SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_SPHERES_AND_MESH;
    }
    return SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF;
}

static BOOL apply_mode(unsigned int mode) {
    uint8_t wanted[COLLISION_DEBUG_FLAG_COUNT];
    unsigned int index;

    if (!mode_flags(mode, wanted)) return FALSE;
    /* Disable in reverse dependency order, then enable in forward order. */
    for (index = COLLISION_DEBUG_FLAG_COUNT; index > 0u; --index) {
        unsigned int current = index - 1u;
        if (wanted[current] == 0u &&
            !set_native_flag(current, 0u)) return FALSE;
    }
    for (index = 0u; index < COLLISION_DEBUG_FLAG_COUNT; ++index) {
        if (wanted[index] != 0u &&
            !set_native_flag(index, wanted[index])) return FALSE;
    }
    active_mode = mode;
    return TRUE;
}

static BOOL advance_mode(void) {
    unsigned int previous = active_mode;
    unsigned int requested = next_mode(previous);

    if (!operational || !apply_mode(requested)) {
        DWORD error = GetLastError();
        if (error == ERROR_SUCCESS) error = ERROR_INVALID_DATA;
        (void)apply_mode(SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF);
        operational = FALSE;
        active_mode = SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF;
        SudekiMpLogFormat(
            "lan_arena_collision_debug event=state result=rejected "
            "previous=%s requested=%s error=%lu "
            "policy=fail_closed_restore_off_no_simulation_write\r\n",
            mode_name(previous), mode_name(requested),
            (unsigned long)error);
        SetLastError(error);
        return FALSE;
    }
    SudekiMpLogFormat(
        "lan_arena_collision_debug event=state result=applied "
        "previous=%s current=%s key=F9 "
        "native_flags=%u,%u,%u "
        "policy=native_debug_presentation_only_no_simulation_write\r\n",
        mode_name(previous), mode_name(active_mode),
        (unsigned int)*bindings[0].flag,
        (unsigned int)*bindings[1].flag,
        (unsigned int)*bindings[2].flag);
    return TRUE;
}

static BOOL process_owns_foreground(void) {
    HWND window = GetForegroundWindow();
    DWORD process_id = 0u;

    if (window == NULL || GetWindowThreadProcessId(window, &process_id) == 0u) {
        return FALSE;
    }
    return process_id == GetCurrentProcessId();
}

static BOOL consume_edge(BOOL foreground, BOOL key_down, BOOL *was_down) {
    BOOL rising;

    if (was_down == NULL) return FALSE;
    rising = foreground && key_down && !*was_down;
    *was_down = key_down;
    return rising;
}

BOOL SudekiMpInstallLanArenaCollisionDebug(HMODULE game_module) {
    uint8_t *base = (uint8_t *)game_module;
    const uintptr_t function_rvas[COLLISION_DEBUG_FLAG_COUNT] = {
        RVA_TOGGLE_COLLISION_SPHERES,
        RVA_TOGGLE_COLLISION_MESH,
        RVA_TOGGLE_SPHERE_TREES
    };
    const uintptr_t flag_rvas[COLLISION_DEBUG_FLAG_COUNT] = {
        RVA_COLLISION_SPHERES_FLAG,
        RVA_COLLISION_MESH_FLAG,
        RVA_SPHERE_TREES_FLAG
    };
    unsigned int index;

    if (base == NULL) {
        return reject_install(ERROR_INVALID_PARAMETER,
            "null_game_module", COLLISION_DEBUG_FLAG_COUNT);
    }
    if (game_base != NULL) {
        return reject_install(ERROR_ALREADY_EXISTS,
            "already_installed", COLLISION_DEBUG_FLAG_COUNT);
    }
    if (!supported_module_header(base)) {
        return reject_install(ERROR_BAD_EXE_FORMAT,
            "unsupported_image_header", COLLISION_DEBUG_FLAG_COUNT);
    }
    ZeroMemory(bindings, sizeof(bindings));
    ZeroMemory(original_flags, sizeof(original_flags));
    for (index = 0u; index < COLLISION_DEBUG_FLAG_COUNT; ++index) {
        uint8_t *entry = base + function_rvas[index];
        uint8_t *flag = base + flag_rvas[index];
        if (!writable_memory(flag, 1u) ||
            !native_toggle_signature(entry, flag)) {
            ZeroMemory(bindings, sizeof(bindings));
            ZeroMemory(original_flags, sizeof(original_flags));
            return reject_install(ERROR_BAD_EXE_FORMAT,
                "native_toggle_preflight", index);
        }
        _Static_assert(
            sizeof(bindings[index].toggle) == sizeof(entry),
            "32-bit native function pointers must match object pointers");
        memcpy(&bindings[index].toggle, &entry,
            sizeof(bindings[index].toggle));
        bindings[index].flag = flag;
        original_flags[index] = *flag;
        if (original_flags[index] > 1u) {
            ZeroMemory(bindings, sizeof(bindings));
            ZeroMemory(original_flags, sizeof(original_flags));
            return reject_install(ERROR_INVALID_DATA,
                "non_boolean_native_flag", index);
        }
    }
    game_base = base;
    active_mode = SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF;
    operational = TRUE;
    f9_was_down =
        (GetAsyncKeyState(VK_F9) & (SHORT)0x8000) != 0;
    if (!apply_mode(SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF)) {
        DWORD error = GetLastError();
        if (error == ERROR_SUCCESS) error = ERROR_INVALID_DATA;
        SudekiMpLogFormat(
            "lan_arena_collision_debug event=install result=rejected "
            "reason=off_baseline_failed error=%lu "
            "policy=restore_original_flags_and_uninstall\r\n",
            (unsigned long)error);
        SudekiMpUninstallLanArenaCollisionDebug();
        SetLastError(error);
        return FALSE;
    }
    SudekiMpLogFormat(
        "lan_arena_collision_debug event=install result=success key=F9 "
        "cycle=off,spheres,spheres_and_mesh "
        "native_toggle_rvas=0x000e6a10,0x000e6a20,0x000e6a30 "
        "native_flag_rvas=0x003c2fdb,0x003c2fe0,0x003c2fe1 "
        "original_flags=%u,%u,%u sphere_trees_hotkey=disabled "
        "policy=exact_image_relocation_aware_presentation_only\r\n",
        (unsigned int)original_flags[0],
        (unsigned int)original_flags[1],
        (unsigned int)original_flags[2]);
    return TRUE;
}

void SudekiMpLanArenaCollisionDebugServiceHotkey(void) {
    BOOL key_down;

    if (game_base == NULL || !operational) return;
    key_down = (GetAsyncKeyState(VK_F9) & (SHORT)0x8000) != 0;
    if (consume_edge(process_owns_foreground(), key_down, &f9_was_down)) {
        (void)advance_mode();
    }
}

unsigned int SudekiMpLanArenaCollisionDebugMode(void) {
    return game_base != NULL && operational ? active_mode :
        SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF;
}

void SudekiMpUninstallLanArenaCollisionDebug(void) {
    unsigned int index;
    BOOL restored = TRUE;

    if (game_base != NULL) {
        for (index = COLLISION_DEBUG_FLAG_COUNT; index > 0u; --index) {
            unsigned int current = index - 1u;
            if (!set_native_flag(current, original_flags[current])) {
                restored = FALSE;
            }
        }
        SudekiMpLogFormat(
            "lan_arena_collision_debug event=uninstall result=%s "
            "restored_flags=%u,%u,%u policy=reverse_order_exact_restore\r\n",
            restored ? "success" : "rejected",
            bindings[0].flag == NULL ? 0u :
                (unsigned int)*bindings[0].flag,
            bindings[1].flag == NULL ? 0u :
                (unsigned int)*bindings[1].flag,
            bindings[2].flag == NULL ? 0u :
                (unsigned int)*bindings[2].flag);
    }
    game_base = NULL;
    ZeroMemory(bindings, sizeof(bindings));
    ZeroMemory(original_flags, sizeof(original_flags));
    active_mode = SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF;
    f9_was_down = FALSE;
    operational = FALSE;
}

#if defined(SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_TESTING)
BOOL SudekiMpLanArenaCollisionDebugAdvanceForTesting(void) {
    return advance_mode();
}

BOOL SudekiMpLanArenaCollisionDebugConsumeEdgeForTesting(
    BOOL foreground,
    BOOL key_down,
    BOOL *was_down
) {
    return consume_edge(foreground, key_down, was_down);
}
#endif
