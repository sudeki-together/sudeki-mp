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

typedef BOOL (__attribute__((cdecl,regparm(0))) *MoviePlayFunction)(
    const char *movie_name,
    BOOL skippable
);
static SudekiMpInlineHook movie_play_hook;
static HMODULE installed_game_module;
static BOOL restore_quarantined;
static BOOL restore_failure_logged;
static BOOL containment_module_pinned;
static BOOL skip_logos=TRUE,observer_owns_hook;
static SudekiMpMovieBegin movie_begin;
static SudekiMpMovieEnd movie_end;
static volatile LONG native_movie_calls;


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

/* Optional extra (saved-story SkipStartupMovies): the opening poem
 * cinematic. Case-insensitive suffix match so a path prefix also matches. */
static BOOL skip_intro_poem;
static unsigned passthrough_logs;
void SudekiMpLanArenaStartupMovieSkipIntroPoem(BOOL enabled) { skip_intro_poem=enabled; }
static const char *matched_intro_poem(const char *movie_name) {
    static const char poem[]="fma01_poem";
    size_t n=0,k=sizeof(poem)-1u;
    if (!skip_intro_poem || movie_name==NULL) return NULL;
    while (n<260u && readable_region(movie_name+n,1u) && movie_name[n]) ++n;
    if (n<k || n>=260u) return NULL;
    for (size_t at=0; at+k<=n; ++at) {
        size_t i=0;
        for (; i<k; ++i) {
            char c=movie_name[at+i];
            if (c>='A' && c<='Z') c=(char)(c-'A'+'a');
            if (c!=poem[i]) break;
        }
        if (i==k) return "FMA01_poem";
    }
    return NULL;
}

BOOL SudekiMpLanArenaStartupMovieShouldSkip(const char *movie_name) {
    return matched_startup_movie(movie_name) != NULL;
}

static BOOL __attribute__((cdecl,regparm(0))) lan_arena_movie_play(
    const char *movie_name,
    BOOL skippable
) {
    MoviePlayFunction original_movie_play;
    const char *matched_movie = skip_logos?matched_startup_movie(movie_name):NULL;
    if (matched_movie == NULL) matched_movie = matched_intro_poem(movie_name);

    if (matched_movie != NULL) {
        SudekiMpLogFormat(
            "lan_arena_startup_movie event=skip movie=%s "
            "scope=local_lan_role\r\n",
            matched_movie);
        return TRUE;
    }

    if (skip_intro_poem && passthrough_logs < 16u) {
        char copy[64]; size_t n=0;
        while (n+1u<sizeof(copy) && movie_name && readable_region(movie_name+n,1u) && movie_name[n]) {
            copy[n]=(movie_name[n]>=0x20 && movie_name[n]<0x7f)?movie_name[n]:'?'; ++n;
        }
        copy[n]=0; ++passthrough_logs;
        SudekiMpLogFormat("lan_arena_startup_movie event=passthrough movie=%s skippable=%d\r\n",copy,(int)skippable);
    }
    original_movie_play =
        (MoviePlayFunction)movie_play_hook.trampoline;
    if (original_movie_play == NULL) return FALSE;
    InterlockedIncrement(&native_movie_calls);
    BOOL observed=movie_begin && movie_begin(movie_name);
    BOOL result=original_movie_play(movie_name, skippable);
    if(observed && movie_end) movie_end();
    InterlockedDecrement(&native_movie_calls);
    return result;
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

    if(InterlockedCompareExchange(&native_movie_calls,0,0) || movie_begin) {
        retain_after_restore_failure(ERROR_BUSY);return FALSE;
    }
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

BOOL SudekiMpLanArenaMovieObserve(HMODULE image,SudekiMpMovieBegin begin,SudekiMpMovieEnd end) {
    if(!image || !begin || !end || movie_begin || restore_quarantined ||
        InterlockedCompareExchange(&native_movie_calls,0,0) ||
        (movie_play_hook.installed && installed_game_module!=image)) {
        SetLastError(ERROR_BUSY);return FALSE;
    }
    observer_owns_hook=!movie_play_hook.installed;
    if(observer_owns_hook) {
        if(!SudekiMpInstallLanArenaStartupMovieSkip(image)) { observer_owns_hook=FALSE;return FALSE; }
        skip_logos=FALSE;
    }
    movie_begin=begin;movie_end=end;return TRUE;
}
BOOL SudekiMpLanArenaMovieUnobserve(void) {
    if(InterlockedCompareExchange(&native_movie_calls,0,0)) { SetLastError(ERROR_BUSY);return FALSE; }
    if(!movie_begin) return TRUE;
    SudekiMpMovieBegin begin=movie_begin;SudekiMpMovieEnd end=movie_end;
    movie_begin=NULL;movie_end=NULL;
    if(observer_owns_hook && !SudekiMpUninstallLanArenaStartupMovieSkip()) {
        movie_begin=begin;movie_end=end;return FALSE;
    }
    observer_owns_hook=FALSE;skip_logos=TRUE;return TRUE;
}
BOOL SudekiMpLanArenaMoviePlayInstalled(const char *name) {
    if(!movie_begin || !installed_game_module || !movie_play_patch_still_owned() ||
        InterlockedCompareExchange(&native_movie_calls,0,0)) return FALSE;
    InterlockedIncrement(&native_movie_calls);
    /* Retail returns bool in AL, accepts two cdecl stack arguments. Local
     * client skip is disabled; the scoped movie-loop callback handles end. */
    BOOL result=((MoviePlayFunction)movie_play_hook.trampoline)(name,FALSE)&0xff;
    InterlockedDecrement(&native_movie_calls);return result;
}
