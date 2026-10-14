#include "hooks/loose_mods.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "engine/texture_mod_index.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Loose file mods require the supported x86 ABI"
#endif

enum { SITE_COUNT = 6, CALL_LENGTH = 6, MAX_LOOSE = 4096, MAX_OPEN_LOGS = 64, LOGS_PER_FILE = 2,
    IAT_CREATE_FILE = 0x29a0ac, IAT_BINK_OPEN = 0x29a294 };

typedef struct LooseFile { char rel[SUDEKIMP_MOD_NAME_MAX + 1], mod[64], *path; unsigned logs; } LooseFile;
typedef struct Site { uint32_t rva, slot; const char *role; } Site;
static const Site sites[SITE_COUNT] = {
    {0x28870b, IAT_CREATE_FILE, "sx_stream_speech"}, {0x28878c, IAT_CREATE_FILE, "sx_stream_bank"},
    {0x2890de, IAT_CREATE_FILE, "sx_bank"}, {0x256c49, IAT_CREATE_FILE, "crt_open"},
    {0x1bf383, IAT_CREATE_FILE, "movie_check"}, {0x1bf3c9, IAT_BINK_OPEN, "bink_open"},
};

static LooseFile *loose;
static unsigned loose_count, loose_capacity;
static SudekiMpInlineHook hooks[SITE_COUNT];
static uint8_t *base;
static volatile LONG open_logs;

static BOOL readable(const void *p, size_t n) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t a = (uintptr_t)p;
    if (!p || !n || a > UINTPTR_MAX - n || VirtualQuery(p, &m, sizeof(m)) != sizeof(m) || m.State != MEM_COMMIT ||
        (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || a + n > (uintptr_t)m.BaseAddress + m.RegionSize) return FALSE;
    return TRUE;
}

int SudekiMpLooseModsAdd(const char *name, const wchar_t *path, const char *mod) {
    LooseFile *f = NULL;
    char ansi[MAX_PATH];
    BOOL lossy = FALSE;
    if (base || !name || !path || !mod || !SudekiMpModLooseNameValid(name)) return 0;
    /* The game opens these with ANSI APIs; the mod path must survive the code page. */
    if (!WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, path, -1, ansi, MAX_PATH, NULL, &lossy) || lossy) {
        SudekiMpLogFormat("loose_mods event=refused mod=%s name=%s reason=path_not_ansi\r\n", mod, name);
        return 0;
    }
    for (unsigned i = 0; i < loose_count; ++i) if (!_stricmp(loose[i].rel, name)) { f = &loose[i]; free(f->path); break; }
    if (!f) {
        if (loose_count >= MAX_LOOSE) return 0;
        if (loose_count == loose_capacity) {
            unsigned capacity = loose_capacity ? loose_capacity * 2u : 16u;
            void *p = realloc(loose, capacity * sizeof(*loose));
            if (!p) return 0;
            loose = (LooseFile *)p;
            loose_capacity = capacity;
        }
        f = &loose[loose_count++];
    }
    memset(f, 0, sizeof(*f));
    strncpy(f->rel, name, sizeof(f->rel) - 1u);
    for (char *c = f->rel; *c; ++c) if (*c == '/') *c = '\\';
    strncpy(f->mod, mod, sizeof(f->mod) - 1u);
    if (!(f->path = _strdup(ansi))) { --loose_count; return 0; }
    return 1;
}

/* Caller's thread, at the open; returns the path to pass on. */
static const char *__attribute__((used, noinline)) redirect_path(const char *path, unsigned site) {
    DWORD saved = GetLastError();
    const char *result = path;
    char rel[SUDEKIMP_MOD_NAME_MAX + 1];
    size_t n = 0;
    if (base && path && readable(path, 1)) {
        while (n < MAX_PATH && readable(path + n, 1) && path[n]) ++n;
        if (n < MAX_PATH && SudekiMpModLooseRelative(path, rel, sizeof(rel))) {
            for (unsigned i = 0; i < loose_count; ++i) {
                if (_stricmp(loose[i].rel, rel)) continue;
                result = loose[i].path;
                if (loose[i].logs < LOGS_PER_FILE) {
                    ++loose[i].logs;
                    SudekiMpLogFormat("loose_mods event=redirect site=%s mod=%s name=%s\r\n", sites[site].role, loose[i].mod, loose[i].rel);
                }
                break;
            }
            if (result == path && InterlockedIncrement(&open_logs) <= MAX_OPEN_LOGS)
                SudekiMpLogFormat("loose_mods event=open site=%s name=%s\r\n", sites[site].role, rel);
        }
    }
    SetLastError(saved);
    return result;
}

/* Jumped to in place of "CALL [slot]": the arguments are on the stack and
 * [ESP] is the path. Swap it if overridden, call through the same import
 * slot (stdcall pops the arguments), then resume after the 6-byte call. */
#define SITE_STUB(N) \
    void *SudekiMpLooseSlot##N __attribute__((used)); \
    void *SudekiMpLooseReturn##N __attribute__((used)); \
    static void __attribute__((naked, noinline)) site_stub_##N(void) { \
        __asm__ volatile( \
            "pushal; mov %esp,%esi; and $-16,%esp; sub $16,%esp;" \
            "mov 32(%esi),%eax; mov %eax,(%esp); movl $" #N ",4(%esp); call _redirect_path;" \
            "mov %eax,32(%esi); mov %esi,%esp; popal;" \
            "mov _SudekiMpLooseSlot" #N ",%eax; call *(%eax); jmp *_SudekiMpLooseReturn" #N); \
    }
SITE_STUB(0) SITE_STUB(1) SITE_STUB(2) SITE_STUB(3) SITE_STUB(4) SITE_STUB(5)
static void *const stubs[SITE_COUNT] = {(void *)(uintptr_t)site_stub_0, (void *)(uintptr_t)site_stub_1,
    (void *)(uintptr_t)site_stub_2, (void *)(uintptr_t)site_stub_3, (void *)(uintptr_t)site_stub_4,
    (void *)(uintptr_t)site_stub_5};
static void **const slot_vars[SITE_COUNT] = {&SudekiMpLooseSlot0, &SudekiMpLooseSlot1, &SudekiMpLooseSlot2,
    &SudekiMpLooseSlot3, &SudekiMpLooseSlot4, &SudekiMpLooseSlot5};
static void **const return_vars[SITE_COUNT] = {&SudekiMpLooseReturn0, &SudekiMpLooseReturn1, &SudekiMpLooseReturn2,
    &SudekiMpLooseReturn3, &SudekiMpLooseReturn4, &SudekiMpLooseReturn5};

BOOL SudekiMpLooseModsUninstall(void) {
    if (!base) return TRUE;
    for (unsigned i = SITE_COUNT; i-- > 0;)
        if (hooks[i].installed && !SudekiMpRestoreInlineHook(&hooks[i])) return FALSE;
    /* Paths and stubs stay: a thread may still be inside a redirected open. */
    base = NULL;
    return TRUE;
}

BOOL SudekiMpLooseModsInstall(HMODULE image) {
    uint8_t *b = (uint8_t *)image, call[SITE_COUNT][CALL_LENGTH];
    if (base || !b || !SudekiMpCheckLoadedExecutable(image)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    if (!loose_count) { SetLastError(ERROR_SUCCESS); return TRUE; } /* nothing to own */
    for (unsigned i = 0; i < SITE_COUNT; ++i) {
        void *slot = b + sites[i].slot;
        call[i][0] = 0xff; call[i][1] = 0x15;
        memcpy(call[i] + 2, &slot, 4);
        if (!readable(b + sites[i].rva, CALL_LENGTH) || memcmp(b + sites[i].rva, call[i], CALL_LENGTH)) {
            SudekiMpLogFormat("loose_mods event=refused reason=unexpected_site site=%s\r\n", sites[i].role);
            SetLastError(ERROR_INVALID_DATA);
            return FALSE;
        }
        *slot_vars[i] = slot;
        *return_vars[i] = b + sites[i].rva + CALL_LENGTH;
    }
    base = b; /* observers check it; set before the first site can run */
    for (unsigned i = 0; i < SITE_COUNT; ++i) {
        if (!SudekiMpInstallInlineHook(&hooks[i], b + sites[i].rva, call[i], CALL_LENGTH, stubs[i])) {
            DWORD error = GetLastError();
            while (i-- > 0) (void)SudekiMpRestoreInlineHook(&hooks[i]);
            base = NULL;
            SetLastError(error);
            return FALSE;
        }
    }
    SudekiMpLogFormat("loose_mods event=installed sites=%u files=%u\r\n", (unsigned)SITE_COUNT, loose_count);
    return TRUE;
}
