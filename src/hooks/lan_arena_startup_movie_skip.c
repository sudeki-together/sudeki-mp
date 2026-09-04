#include "hooks/lan_arena_startup_movie_skip.h"

#include "engine/log.h"
#include "hooks/call_hook.h"

#include <stdint.h>
#include <string.h>

enum {
    RVA_MOVIE_PLAY = 0x00104d90u,
    MOVIE_PLAY_HOOK_LENGTH = 6u
};

static const uint8_t expected_movie_play_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u
};
static const char *const startup_movies[] = {
    "Publisher.bik",
    "ClimaxLogo.bik",
    "TWIMTBP.bik"
};

typedef BOOL (__attribute__((cdecl)) *MoviePlayFunction)(
    const char *movie_name,
    BOOL skippable
);
static SudekiMpInlineHook movie_play_hook;
static HMODULE installed_game_module;
static BOOL restore_quarantined;
static BOOL restore_failure_logged;
static BOOL containment_module_pinned;

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
        (information.Protect & PAGE_GUARD) != 0u) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    if (protection != PAGE_READONLY && protection != PAGE_READWRITE &&
        protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ &&
        protection != PAGE_EXECUTE_READWRITE &&
        protection != PAGE_EXECUTE_WRITECOPY) {
        return FALSE;
    }
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    return region_end >= (uintptr_t)information.BaseAddress && end <= region_end;
}

static const char *matched_startup_movie(const char *movie_name) {
    unsigned int index;

    for (index = 0u;
         movie_name != NULL &&
         index < sizeof(startup_movies) / sizeof(startup_movies[0]);
         ++index) {
        size_t length = strlen(startup_movies[index]) + 1u;
        if (readable_region(movie_name, length) &&
            memcmp(movie_name, startup_movies[index], length) == 0) {
            return startup_movies[index];
        }
    }
    return NULL;
}

BOOL SudekiMpLanArenaStartupMovieShouldSkip(const char *movie_name) {
    return matched_startup_movie(movie_name) != NULL;
}

static BOOL __attribute__((cdecl)) lan_arena_movie_play(
    const char *movie_name,
    BOOL skippable
) {
    MoviePlayFunction original_movie_play;
    const char *matched_movie = matched_startup_movie(movie_name);

    if (matched_movie != NULL) {
        SudekiMpLogFormat(
            "lan_arena_startup_movie event=skip movie=%s "
            "scope=local_lan_role\r\n",
            matched_movie);
        return TRUE;
    }

    original_movie_play =
        (MoviePlayFunction)movie_play_hook.trampoline;
    if (original_movie_play == NULL) return FALSE;
    return original_movie_play(movie_name, skippable);
}

BOOL SudekiMpLanArenaStartupMovieSkipImageMatches(HMODULE game_module) {
    const uint8_t *base = (const uint8_t *)game_module;

    if (base == NULL) return FALSE;
    return memcmp(
        base + RVA_MOVIE_PLAY,
        expected_movie_play_entry,
        sizeof(expected_movie_play_entry)) == 0;
}

BOOL SudekiMpInstallLanArenaStartupMovieSkip(HMODULE game_module) {
    uint8_t *base = (uint8_t *)game_module;

    if (base == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (movie_play_hook.installed) {
        SetLastError(restore_quarantined ? ERROR_BUSY : ERROR_ALREADY_EXISTS);
        return FALSE;
    }
    if (!SudekiMpLanArenaStartupMovieSkipImageMatches(game_module)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (!SudekiMpInstallInlineHook(
            &movie_play_hook,
            base + RVA_MOVIE_PLAY,
            expected_movie_play_entry,
            MOVIE_PLAY_HOOK_LENGTH,
            (const void *)lan_arena_movie_play)) {
        return FALSE;
    }
    installed_game_module = game_module;
    restore_quarantined = FALSE;
    restore_failure_logged = FALSE;
    SudekiMpLogFormat(
        "lan_arena_startup_movie event=install status=ready "
        "movie_play_rva=0x%08lx activation=before_game_thread_resume "
        "policy=three_known_logo_movies_only_nonlogo_native_passthrough\r\n",
        (unsigned long)RVA_MOVIE_PLAY);
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

static BOOL movie_play_patch_still_owned(void) {
    uint8_t expected_patch[MOVIE_PLAY_HOOK_LENGTH];
    int32_t displacement;

    if (!movie_play_hook.installed || movie_play_hook.target == NULL ||
        movie_play_hook.length != MOVIE_PLAY_HOOK_LENGTH) {
        return FALSE;
    }
    memset(expected_patch, 0x90, sizeof(expected_patch));
    expected_patch[0] = 0xe9u;
    displacement = (int32_t)(
        (const uint8_t *)lan_arena_movie_play -
        (movie_play_hook.target + 5u));
    memcpy(expected_patch + 1u, &displacement, sizeof(displacement));
    return memcmp(
        movie_play_hook.target,
        expected_patch,
        sizeof(expected_patch)) == 0;
}

static void retain_after_restore_failure(DWORD error) {
    HMODULE pinned_module = NULL;
    BOOL pinned = containment_module_pinned;

    if (error == ERROR_SUCCESS) error = ERROR_WRITE_FAULT;
    restore_quarantined = TRUE;
    if (!pinned) {
        pinned = GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCSTR)(uintptr_t)&SudekiMpUninstallLanArenaStartupMovieSkip,
            &pinned_module);
        if (pinned) containment_module_pinned = TRUE;
    }
    if (!restore_failure_logged) {
        restore_failure_logged = TRUE;
        SudekiMpLogFormat(
            "lan_arena_startup_movie event=restore state=quarantined "
            "win32_error=%lu module_pinned=%s "
            "policy=retain_callback_trampoline_and_target_for_retry\r\n",
            (unsigned long)error,
            pinned ? "true" : "false");
    }
    SetLastError(error);
}

BOOL SudekiMpUninstallLanArenaStartupMovieSkip(void) {
    DWORD error;

    if (!movie_play_hook.installed) {
        installed_game_module = NULL;
        restore_quarantined = FALSE;
        restore_failure_logged = FALSE;
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    if (!movie_play_patch_still_owned()) {
        retain_after_restore_failure(ERROR_BUSY);
        return FALSE;
    }
    if (!SudekiMpRestoreInlineHook(&movie_play_hook)) {
        error = GetLastError();
        retain_after_restore_failure(error);
        return FALSE;
    }
    installed_game_module = NULL;
    restore_quarantined = FALSE;
    restore_failure_logged = FALSE;
    SudekiMpLogWrite(
        "lan_arena_startup_movie event=uninstall status=restored\r\n");
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL SudekiMpLanArenaStartupMovieSkipInstalled(void) {
    return movie_play_hook.installed && installed_game_module != NULL;
}
