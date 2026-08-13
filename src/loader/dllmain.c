#include "engine/build_identity.h"
#include "engine/log.h"
#include "hooks/pattern_scan.h"
#include "hooks/quick_menu.h"
#include "hooks/skill_trace.h"

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUDEKIMP_INIT_OK 0u
#define SUDEKIMP_INIT_BAD_PATH 1u
#define SUDEKIMP_INIT_BAD_BUILD 2u
#define SUDEKIMP_INIT_BAD_SIGNATURE 3u
#define SUDEKIMP_INIT_PATCH_FAILED 4u
#define SUDEKIMP_INIT_TRACE_FAILED 5u
#define SUDEKIMP_INIT_BAD_CONFIG 6u

static HMODULE dll_module;

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static BOOL read_config_boolean(
    const wchar_t *path,
    const wchar_t *section,
    const wchar_t *key
) {
    wchar_t value[16];

    GetPrivateProfileStringW(section, key, L"false", value,
        (DWORD)(sizeof(value) / sizeof(value[0])), path);
    return _wcsicmp(value, L"true") == 0 ||
        _wcsicmp(value, L"yes") == 0 ||
        _wcsicmp(value, L"on") == 0 ||
        wcscmp(value, L"1") == 0;
}

static BOOL read_config_float(
    const wchar_t *path,
    const wchar_t *section,
    const wchar_t *key,
    float default_value,
    float minimum,
    float maximum,
    float *result
) {
    wchar_t value[32];
    wchar_t default_text[32];
    wchar_t *end = NULL;
    double parsed;

    if (result == NULL) {
        return FALSE;
    }
    _snwprintf(
        default_text,
        sizeof(default_text) / sizeof(default_text[0]),
        L"%.3f",
        (double)default_value
    );
    default_text[(sizeof(default_text) / sizeof(default_text[0])) - 1] = L'\0';
    GetPrivateProfileStringW(
        section,
        key,
        default_text,
        value,
        (DWORD)(sizeof(value) / sizeof(value[0])),
        path
    );
    parsed = wcstod(value, &end);
    if (end == value || *end != L'\0' || parsed < minimum || parsed > maximum) {
        return FALSE;
    }
    *result = (float)parsed;
    return TRUE;
}

static BOOL get_text_section(
    HMODULE module,
    const uint8_t **section_base,
    size_t *section_size
) {
    const uint8_t *base = (const uint8_t *)module;
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
    const IMAGE_NT_HEADERS32 *nt;
    const IMAGE_SECTION_HEADER *section;
    WORD index;

    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        return FALSE;
    }
    nt = (const IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return FALSE;
    }

    section = IMAGE_FIRST_SECTION(nt);
    for (index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
        if (memcmp(section[index].Name, ".text", 5) == 0) {
            *section_base = base + section[index].VirtualAddress;
            *section_size = section[index].Misc.VirtualSize;
            return TRUE;
        }
    }
    return FALSE;
}

DWORD WINAPI SudekiMP_Initialize(void *unused) {
    static const uint8_t activation_pattern[] = {
        0xa1, 0, 0, 0, 0,
        0x8b, 0x35, 0, 0, 0, 0,
        0xc7, 0x40, 0x24, 0x01, 0x00, 0x00, 0x00
    };
    static const char activation_mask[] = "x????xx????xxxxxxx";
    wchar_t game_path[MAX_PATH];
    wchar_t config_path[MAX_PATH];
    HMODULE game_module = GetModuleHandleW(NULL);
    SudekiMpBuildCheck build;
    const uint8_t *text_base;
    size_t text_size;
    SudekiMpPatternResult pattern_result;
    BOOL patch_enabled;
    BOOL trace_enabled;
    BOOL animation_speed_enabled;
    BOOL camera_speed_enabled;
    float plasmatica_animation_speed = 1.0f;
    float plasmatica_camera_speed = 1.0f;

    (void)unused;
    if (GetModuleFileNameW(NULL, game_path, MAX_PATH) == 0) {
        return SUDEKIMP_INIT_BAD_PATH;
    }
    if (!SudekiMpLogOpenBesideGame(game_path)) {
        return SUDEKIMP_INIT_BAD_PATH;
    }

    SudekiMpLogWrite("SudekiMP 0.1.0\r\n");
    SudekiMpLogWrite("event=process_attach\r\n");
    SudekiMpLogFormat("module_base=0x%08lx\r\n", (unsigned long)(uintptr_t)game_module);

    if (!SudekiMpCheckExecutableFile(game_path, &build)) {
        SudekiMpLogWrite("status=hash_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_BUILD;
    }
    SudekiMpLogFormat("game_sha256=%s\r\n", build.actual_sha256);

    if (!build.hash_matches || !build.pe_matches ||
        !SudekiMpCheckLoadedExecutable(game_module)) {
        SudekiMpLogWrite("status=unsupported_build\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_BUILD;
    }

    if (!get_text_section(game_module, &text_base, &text_size)) {
        SudekiMpLogWrite("status=text_section_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_SIGNATURE;
    }

    pattern_result = SudekiMpFindPattern(
        text_base,
        text_size,
        activation_pattern,
        activation_mask,
        sizeof(activation_pattern)
    );
    SudekiMpLogFormat("quick_menu_signature_matches=%lu\r\n",
        (unsigned long)pattern_result.match_count);
    if (pattern_result.match_count != 1 || pattern_result.address == NULL) {
        SudekiMpLogWrite("status=signature_mismatch\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_SIGNATURE;
    }
    SudekiMpLogFormat("quick_menu_signature_rva=0x%08lx\r\n",
        (unsigned long)(pattern_result.address - (const uint8_t *)game_module));

    if (dll_module == NULL ||
        GetModuleFileNameW(dll_module, config_path, MAX_PATH) == 0) {
        SudekiMpLogWrite("status=config_path_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_PATH;
    }
    {
        wchar_t *slash = wcsrchr(config_path, L'\\');
        if (slash != NULL) {
            slash[1] = L'\0';
        } else {
            config_path[0] = L'\0';
        }
    }
    if ((size_t)lstrlenW(config_path) + 13u < MAX_PATH) {
        lstrcatW(config_path, L"SudekiMP.ini");
    }
    patch_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnableQuickMenuNormalSpeed"
    );
    trace_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnablePlasmaticaTrace"
    );
    animation_speed_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnablePlasmaticaAnimationSpeed"
    );
    camera_speed_enabled = read_config_boolean(
        config_path,
        L"SudekiMP",
        L"EnablePlasmaticaCameraSpeed"
    );
    if (animation_speed_enabled && !read_config_float(
            config_path,
            L"SudekiMP",
            L"PlasmaticaAnimationSpeed",
            1.5f,
            0.25f,
            4.0f,
            &plasmatica_animation_speed)) {
        SudekiMpLogWrite("plasmatica_animation_speed_config=invalid\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }
    if (camera_speed_enabled && !read_config_float(
            config_path,
            L"SudekiMP",
            L"PlasmaticaCameraSpeed",
            1.5f,
            0.25f,
            4.0f,
            &plasmatica_camera_speed)) {
        SudekiMpLogWrite("plasmatica_camera_speed_config=invalid\r\n");
        SudekiMpLogWrite("status=config_error\r\n");
        SudekiMpLogClose();
        return SUDEKIMP_INIT_BAD_CONFIG;
    }

    SudekiMpLogFormat("quick_menu_patch_requested=%s\r\n",
        patch_enabled ? "true" : "false");
    if (patch_enabled) {
        if (!SudekiMpEnableQuickMenuNormalSpeed(pattern_result.address)) {
            SudekiMpLogFormat("quick_menu_patch_error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite("quick_menu_patch_applied=false\r\n");
            SudekiMpLogWrite("status=patch_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_PATCH_FAILED;
        }
        SudekiMpLogWrite("quick_menu_patch_applied=true\r\n");
    } else {
        SudekiMpLogWrite("quick_menu_patch_applied=false\r\n");
    }
    SudekiMpLogFormat("plasmatica_trace_requested=%s\r\n",
        trace_enabled ? "true" : "false");
    SudekiMpLogFormat(
        "plasmatica_animation_speed_requested=%s multiplier_bits=0x%08lx\r\n",
        animation_speed_enabled ? "true" : "false",
        (unsigned long)float_bits(plasmatica_animation_speed)
    );
    SudekiMpLogFormat(
        "plasmatica_camera_speed_requested=%s multiplier_bits=0x%08lx\r\n",
        camera_speed_enabled ? "true" : "false",
        (unsigned long)float_bits(plasmatica_camera_speed)
    );
    if (trace_enabled || animation_speed_enabled || camera_speed_enabled) {
        if (!SudekiMpInstallSkillTrace(
                game_module,
                animation_speed_enabled ? plasmatica_animation_speed : 1.0f,
                camera_speed_enabled ? plasmatica_camera_speed : 1.0f)) {
            SudekiMpLogFormat("plasmatica_trace_error=%lu\r\n",
                (unsigned long)GetLastError());
            SudekiMpLogWrite("plasmatica_trace_applied=false\r\n");
            SudekiMpLogWrite("status=trace_error\r\n");
            SudekiMpLogClose();
            return SUDEKIMP_INIT_TRACE_FAILED;
        }
        SudekiMpLogWrite("plasmatica_trace_applied=true\r\n");
    } else {
        SudekiMpLogWrite("plasmatica_trace_applied=false\r\n");
    }
    SudekiMpLogWrite("status=ready\r\n");
    if (!trace_enabled && !animation_speed_enabled && !camera_speed_enabled) {
        SudekiMpLogClose();
    }
    return SUDEKIMP_INIT_OK;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        dll_module = instance;
        DisableThreadLibraryCalls(instance);
    } else if (reason == DLL_PROCESS_DETACH) {
        SudekiMpLogClose();
    }
    return TRUE;
}
