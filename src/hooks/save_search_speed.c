#include "hooks/save_search_speed.h"

#include "engine/build_identity.h"
#include "engine/log.h"
#include "hooks/call_hook.h"

#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Save search speed requires the 32-bit Windows target"
#endif

enum {
    RVA_WAIT_LOAD = 0x83d9du,        /* FLD dword [EBX+0x390] in FUN_00483cd0 */
    RVA_WAIT_STORE = 0x83da3u,       /* FSTP dword [EBX+0x394] (resume) */
    RVA_STEP_CALL = 0x81246u,        /* CALL FUN_00500e30 in FUN_004810a0 */
    RVA_CATALOG_STEP = 0x100e30u,    /* FUN_00500e30: cdecl, returns AL */
    RVA_CATALOG_STATE = 0x34b320u,   /* 1 begin, 2 count, 3 read, 5 done */
    MAX_STEPS_PER_TICK = 512u
};

static const uint8_t wait_load_bytes[6] = {0xd9,0x83,0x90,0x03,0x00,0x00};

typedef uint8_t (__cdecl *CatalogStep)(void);

static SudekiMpInlineHook wait_hook;
static SudekiMpRelativeCallHook step_hook;
static CatalogStep catalog_step;
static volatile uint32_t *catalog_state;
static unsigned logged;
float save_search_wait_seconds __attribute__((used));
void *save_search_wait_resume __attribute__((used));

/* Replaces FLD dword [EBX+0x390]; the native FSTP to +0x394 follows. */
__attribute__((naked)) static void wait_load(void) {
    __asm__ volatile(
        "flds _save_search_wait_seconds\n\t"
        "jmp *_save_search_wait_resume\n\t");
}

static uint8_t __cdecl step_until_done(void) {
    DWORD start = GetTickCount();
    unsigned steps = 0;
    uint8_t done = 0;
    while (steps < MAX_STEPS_PER_TICK) {
        uint32_t before = *catalog_state;
        done = catalog_step();
        ++steps;
        if (done) break;
        /* Only the active catalog phases make progress; anything else is
         * left to the native per-tick behaviour. */
        if (before != 1u && before != 2u && before != 3u && *catalog_state == before) break;
    }
    if (logged < 20u) {
        ++logged;
        SudekiMpLogFormat("save_search event=scan steps=%u done=%u catalog=%lu ms=%lu\r\n",
            steps, (unsigned)done, (unsigned long)*catalog_state,
            (unsigned long)(GetTickCount() - start));
    }
    return done;
}

BOOL SudekiMpSaveSearchSpeedInstall(HMODULE game_module, float wait_seconds) {
    uint8_t *base = (uint8_t *)game_module;
    if (wait_hook.installed || step_hook.installed) return TRUE;
    if (!base || !SudekiMpCheckLoadedExecutable(game_module) ||
        !(wait_seconds >= 0.0f && wait_seconds <= 10.0f)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    save_search_wait_seconds = wait_seconds;
    save_search_wait_resume = base + RVA_WAIT_STORE;
    catalog_step = (CatalogStep)(void *)(base + RVA_CATALOG_STEP);
    catalog_state = (volatile uint32_t *)(base + RVA_CATALOG_STATE);
    if (!SudekiMpInstallRelativeCallHook(&step_hook, base + RVA_STEP_CALL,
            base + RVA_CATALOG_STEP, (const void *)step_until_done)) {
        DWORD error = GetLastError();
        SudekiMpLogFormat("save_search event=install_failed seam=step error=%lu\r\n", (unsigned long)error);
        SetLastError(error);
        return FALSE;
    }
    if (!SudekiMpInstallInlineHook(&wait_hook, base + RVA_WAIT_LOAD, wait_load_bytes,
            sizeof(wait_load_bytes), (const void *)wait_load)) {
        DWORD error = GetLastError();
        SudekiMpLogFormat("save_search event=install_failed seam=wait error=%lu\r\n", (unsigned long)error);
        if (!SudekiMpRestoreRelativeCallHook(&step_hook))
            SudekiMpLogWrite("save_search event=rollback_failed seam=step\r\n");
        SetLastError(error);
        return FALSE;
    }
    SudekiMpLogFormat("save_search event=installed wait=%.2f step_call_rva=%#x wait_rva=%#x\r\n",
        (double)wait_seconds, RVA_STEP_CALL, RVA_WAIT_LOAD);
    return TRUE;
}

BOOL SudekiMpSaveSearchSpeedUninstall(void) {
    /* Reverse order. The stub and step_until_done stay in the image, so a
     * game-thread call already past either seam still completes. */
    BOOL ok = TRUE;
    if (wait_hook.installed && !SudekiMpRestoreInlineHook(&wait_hook)) ok = FALSE;
    if (step_hook.installed && !SudekiMpRestoreRelativeCallHook(&step_hook)) ok = FALSE;
    return ok;
}
