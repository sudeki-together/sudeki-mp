#define COBJMACROS
#include "hooks/texture_mods.h"
#include "hooks/call_hook.h"
#include "hooks/archive_mods.h"
#include "hooks/loose_mods.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "engine/texture_mod_index.h"
#include <d3d9.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Texture mods require the supported x86 ABI"
#endif

enum {
    DECODE_RVA = 0x1d8300, DECODE_LENGTH = 7, RECORD_SIZE = 0x3c, RECORD_TEXTURE = 0x04,
    DEVICE_GLOBAL_RVA = 0x3c31dc,
    MAX_MODS = 256, MAX_FILE_BYTES = 256u << 20, MAX_SECTION_WCHARS = 4u << 20,
    DUMP_SLOTS = 1u << 15, MAX_LOG_LINES = 2000
};
/* 0x5D8300 texture decode dispatcher entry:
 *   81 4E 20 00 01 00 00   OR dword ptr [ESI+0x20],0x100
 * Position-independent, so the trampoline replays it unchanged. */
static const uint8_t decode_bytes[DECODE_LENGTH] = {0x81, 0x4e, 0x20, 0x00, 0x01, 0x00, 0x00};

typedef HRESULT (WINAPI *CreateTextureFromMemoryEx)(IDirect3DDevice9 *, const void *, UINT, UINT, UINT, UINT,
    DWORD, D3DFORMAT, D3DPOOL, DWORD, DWORD, D3DCOLOR, void *, PALETTEENTRY *, IDirect3DTexture9 **);
typedef HRESULT (WINAPI *SaveTextureToFileW)(const wchar_t *, DWORD, IDirect3DBaseTexture9 *, const PALETTEENTRY *);
#define D3DX_DEFAULT_VALUE ((UINT)-1)
#define D3DXIFF_DDS_VALUE 4u

typedef struct ModInfo { wchar_t *root; char name[64]; } ModInfo;

static SudekiMpInlineHook decode_hook;
void *SudekiMpTextureModsTrampoline __attribute__((used));
static uint8_t *base;
static SudekiMpTextureModIndex index_table;
static ModInfo mods[MAX_MODS];
static unsigned mod_count;
static BOOL dump_enabled;
static wchar_t dump_root[MAX_PATH];
static CreateTextureFromMemoryEx create_from_memory;
static SaveTextureToFileW save_to_file;
static void *volatile texture_vtable;
static CRITICAL_SECTION dump_lock;
static uint32_t *dump_seen; /* open addressing, key+1 so zero means empty */
static volatile LONG decoded, replaced, failed, skipped, log_lines;

unsigned SudekiMpTextureModsCount(void) { return (unsigned)index_table.count; }
BOOL SudekiMpTextureModsActive(void) { return mod_count != 0u || dump_enabled; }

static BOOL readable(const void *p, size_t n) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t a = (uintptr_t)p;
    if (!p || !n || a > UINTPTR_MAX - n || VirtualQuery(p, &m, sizeof(m)) != sizeof(m) || m.State != MEM_COMMIT ||
        (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || a + n > (uintptr_t)m.BaseAddress + m.RegionSize) return FALSE;
    return TRUE;
}

/* DXT1..DXT5: pitch counts 4x4 block rows. */
static BOOL is_block_format(uint32_t format) {
    return (format & 0x00ffffffu) == 0x00545844u && (format >> 24) >= '1' && (format >> 24) <= '5';
}

static BOOL log_budget(void) { return InterlockedIncrement(&log_lines) <= MAX_LOG_LINES; }

/* The decode thread already created this texture through the same device; a
 * 1x1 managed texture gives the concrete IDirect3DTexture9 vtable so a tile
 * set, cube texture or stale pointer at record+4 is never called into. */
static IDirect3DDevice9 *game_device(void) {
    IDirect3DDevice9 *device = *(IDirect3DDevice9 **)(base + DEVICE_GLOBAL_RVA);
    void **vtable;
    if (!readable(device, sizeof(void *))) return NULL;
    vtable = *(void ***)device;
    if (!readable(vtable, 0x60)) return NULL;
    return device;
}

static void *learn_texture_vtable(IDirect3DDevice9 *device) {
    IDirect3DTexture9 *probe = NULL;
    void *vtable = texture_vtable;
    if (vtable) return vtable;
    if (FAILED(IDirect3DDevice9_CreateTexture(device, 1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &probe, NULL)) || !probe) return NULL;
    vtable = *(void **)probe;
    IDirect3DTexture9_Release(probe);
    (void)InterlockedCompareExchangePointer((void *volatile *)&texture_vtable, vtable, NULL);
    SudekiMpLogFormat("texture_mods event=texture_vtable value=%p\r\n", vtable);
    return texture_vtable;
}

static BOOL dump_first_time(uint32_t key) {
    BOOL first = FALSE;
    uint32_t slot = (key * 2654435761u) & (DUMP_SLOTS - 1u);
    if (!dump_seen) return FALSE;
    EnterCriticalSection(&dump_lock);
    for (unsigned probe = 0; probe < DUMP_SLOTS; ++probe, slot = (slot + 1u) & (DUMP_SLOTS - 1u)) {
        if (dump_seen[slot] == key + 1u) break;
        if (!dump_seen[slot]) { dump_seen[slot] = key + 1u; first = TRUE; break; }
    }
    LeaveCriticalSection(&dump_lock);
    return first;
}

static void dump_texture(uint32_t key, IDirect3DTexture9 *texture, const D3DSURFACE_DESC *desc) {
    wchar_t path[MAX_PATH];
    HRESULT hr = E_FAIL;
    if (!dump_first_time(key)) return;
    if (save_to_file && _snwprintf(path, MAX_PATH, L"%ls\\0x%08lX.dds", dump_root, (unsigned long)key) > 0) {
        path[MAX_PATH - 1] = 0;
        hr = save_to_file(path, D3DXIFF_DDS_VALUE, (IDirect3DBaseTexture9 *)texture, NULL);
    }
    if (log_budget())
        SudekiMpLogFormat("texture_mods event=dump key=0x%08lX width=%u height=%u format=0x%lx levels=%lu saved=%d\r\n",
            (unsigned long)key, desc->Width, desc->Height, (unsigned long)desc->Format,
            (unsigned long)IDirect3DTexture9_GetLevelCount(texture), SUCCEEDED(hr));
}

static void *read_file(const wchar_t *path, DWORD *size) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER length;
    void *data = NULL;
    DWORD got = 0;
    if (file == INVALID_HANDLE_VALUE) return NULL;
    if (GetFileSizeEx(file, &length) && length.QuadPart > 0 && length.QuadPart <= MAX_FILE_BYTES &&
        (data = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)length.QuadPart)) != NULL &&
        (!ReadFile(file, data, (DWORD)length.QuadPart, &got, NULL) || got != (DWORD)length.QuadPart)) {
        HeapFree(GetProcessHeap(), 0, data);
        data = NULL;
    }
    CloseHandle(file);
    if (data) *size = got;
    return data;
}

static IDirect3DTexture9 *load_replacement(IDirect3DDevice9 *device, const SudekiMpTextureModEntry *entry) {
    wchar_t relative[SUDEKIMP_TEXTURE_MOD_PATH_MAX], path[MAX_PATH];
    const char *utf8 = SudekiMpTextureModIndexPath(&index_table, entry);
    IDirect3DTexture9 *texture = NULL;
    DWORD size = 0;
    void *data;
    HRESULT hr;
    if (!create_from_memory || entry->mod >= mod_count ||
        !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, relative, SUDEKIMP_TEXTURE_MOD_PATH_MAX) ||
        _snwprintf(path, MAX_PATH, L"%ls\\%ls", mods[entry->mod].root, relative) <= 0) return NULL;
    path[MAX_PATH - 1] = 0;
    for (wchar_t *p = path; *p; ++p) if (*p == L'/') *p = L'\\';
    if (!(data = read_file(path, &size))) {
        if (log_budget()) SudekiMpLogFormat("texture_mods event=file_missing mod=%s file=%s\r\n", mods[entry->mod].name, utf8);
        return NULL;
    }
    /* Size and format from the file, full mip chain (generated when the file
     * has fewer levels), managed pool like the native textures. */
    hr = create_from_memory(device, data, size, D3DX_DEFAULT_VALUE, D3DX_DEFAULT_VALUE, D3DX_DEFAULT_VALUE, 0,
        D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_DEFAULT_VALUE, D3DX_DEFAULT_VALUE, 0, NULL, NULL, &texture);
    HeapFree(GetProcessHeap(), 0, data);
    if (FAILED(hr) || !texture) {
        if (log_budget()) SudekiMpLogFormat("texture_mods event=decode_failed mod=%s file=%s hr=0x%08lx\r\n", mods[entry->mod].name, utf8, (unsigned long)hr);
        return NULL;
    }
    return texture;
}

/* Decode thread, right after the native dispatcher returned. The record was
 * filled by this thread in this call; record+4 is published only through it. */
static void __attribute__((used, noinline)) observe_decode(uint8_t *record, int ok) {
    DWORD saved = GetLastError();
    IDirect3DTexture9 *texture, *replacement;
    IDirect3DDevice9 *device;
    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT locked;
    const SudekiMpTextureModEntry *entry;
    size_t bytes, rows;
    uint32_t key;
    if (!ok || !base || !readable(record, RECORD_SIZE)) goto done;
    texture = *(IDirect3DTexture9 **)(record + RECORD_TEXTURE);
    if (!readable(texture, sizeof(void *)) || !(device = game_device()) || !learn_texture_vtable(device) ||
        *(void **)texture != texture_vtable) { InterlockedIncrement(&skipped); goto done; }
    if (FAILED(IDirect3DTexture9_GetLevelDesc(texture, 0, &desc)) ||
        !(bytes = SudekiMpTexModLevelBytes((uint32_t)desc.Format, desc.Width, desc.Height))) { InterlockedIncrement(&skipped); goto done; }
    rows = is_block_format((uint32_t)desc.Format) ? (desc.Height + 3u) / 4u : desc.Height;
    if (FAILED(IDirect3DTexture9_LockRect(texture, 0, &locked, NULL, D3DLOCK_READONLY))) { InterlockedIncrement(&skipped); goto done; }
    if (locked.Pitch <= 0 || (size_t)locked.Pitch * rows < bytes || !locked.pBits) {
        IDirect3DTexture9_UnlockRect(texture, 0);
        InterlockedIncrement(&skipped);
        goto done;
    }
    key = SudekiMpTexModCrc32(locked.pBits, bytes); /* TexMod reads the bytes contiguously */
    IDirect3DTexture9_UnlockRect(texture, 0);
    InterlockedIncrement(&decoded);
    if (dump_enabled) dump_texture(key, texture, &desc);
    if (!(entry = SudekiMpTextureModIndexFind(&index_table, key))) goto done;
    if (!(replacement = load_replacement(device, entry))) { InterlockedIncrement(&failed); goto done; }
    *(IDirect3DTexture9 **)(record + RECORD_TEXTURE) = replacement;
    IDirect3DTexture9_Release(texture);
    InterlockedIncrement(&replaced);
    if (log_budget()) {
        D3DSURFACE_DESC now;
        if (FAILED(IDirect3DTexture9_GetLevelDesc(replacement, 0, &now))) memset(&now, 0, sizeof(now));
        SudekiMpLogFormat("texture_mods event=replace key=0x%08lX mod=%s file=%s from=%ux%u/0x%lx to=%ux%u/0x%lx levels=%lu replaced=%ld\r\n",
            (unsigned long)key, mods[entry->mod].name, SudekiMpTextureModIndexPath(&index_table, entry),
            desc.Width, desc.Height, (unsigned long)desc.Format, now.Width, now.Height, (unsigned long)now.Format,
            (unsigned long)IDirect3DTexture9_GetLevelCount(replacement), (long)replaced);
    }
done:
    SetLastError(saved);
}

/* Entry stub: replay the call with the same stack argument and ESI, then
 * observe with ESI = record and AL = the native result, which is returned
 * unchanged (pushal/popal) to the original caller with its RET 4. */
static void __attribute__((naked, noinline)) decode_stub(void) {
    __asm__ volatile(
        "pushl 4(%esp); call *_SudekiMpTextureModsTrampoline;"
        "pushal; mov %esp,%ebp; and $-16,%esp; sub $16,%esp;"
        "mov %esi,(%esp); movzbl %al,%eax; mov %eax,4(%esp); call _observe_decode;"
        "mov %ebp,%esp; popal; ret $4");
}

static BOOL config_bool(const wchar_t *path, const wchar_t *section, const wchar_t *key, BOOL fallback) {
    wchar_t value[16];
    if (!GetPrivateProfileStringW(section, key, L"", value, 16, path) || !value[0]) return fallback;
    if (!_wcsicmp(value, L"true") || !_wcsicmp(value, L"1") || !_wcsicmp(value, L"yes") || !_wcsicmp(value, L"on")) return TRUE;
    if (!_wcsicmp(value, L"false") || !_wcsicmp(value, L"0") || !_wcsicmp(value, L"no") || !_wcsicmp(value, L"off")) return FALSE;
    return fallback;
}

static wchar_t *read_section(const wchar_t *path, const wchar_t *section) {
    for (DWORD capacity = 16384; capacity <= MAX_SECTION_WCHARS; capacity *= 4u) {
        wchar_t *buffer = (wchar_t *)malloc(capacity * sizeof(wchar_t));
        DWORD n;
        if (!buffer) return NULL;
        n = GetPrivateProfileSectionW(section, buffer, capacity, path);
        if (n < capacity - 2u) return buffer;
        free(buffer);
    }
    return NULL;
}

static void load_mod(const wchar_t *root, const wchar_t *folder_name) {
    wchar_t manifest[MAX_PATH], format[64], name[64];
    wchar_t *section, *line;
    unsigned added = 0, files_added = 0, rejected = 0, mod;
    if (mod_count >= MAX_MODS || _snwprintf(manifest, MAX_PATH, L"%ls\\mod.ini", root) <= 0) return;
    manifest[MAX_PATH - 1] = 0;
    if (GetFileAttributesW(manifest) == INVALID_FILE_ATTRIBUTES) return;
    GetPrivateProfileStringW(L"Mod", L"Name", folder_name, name, 64, manifest);
    mod = mod_count;
    if (!WideCharToMultiByte(CP_UTF8, 0, name, -1, mods[mod].name, sizeof(mods[mod].name), NULL, NULL)) strcpy(mods[mod].name, "?");
    GetPrivateProfileStringW(L"Mod", L"Format", L"", format, 64, manifest);
    if (_wcsnicmp(format, L"SudekiMP.Mod/1", 14) || (format[14] && format[14] != L'.')) {
        SudekiMpLogFormat("texture_mods event=mod_refused mod=%s reason=format\r\n", mods[mod].name);
        return;
    }
    if (!config_bool(manifest, L"Mod", L"Enabled", TRUE)) {
        SudekiMpLogFormat("texture_mods event=mod_disabled mod=%s\r\n", mods[mod].name);
        return;
    }
    if (!(mods[mod].root = _wcsdup(root))) return;
    /* [Textures]: 0xKEY = image (TexMod-compatible content key). */
    if ((section = read_section(manifest, L"Textures")) != NULL) {
        for (line = section; *line; line += wcslen(line) + 1u) {
            char utf8[SUDEKIMP_TEXTURE_MOD_PATH_MAX + 32], path[SUDEKIMP_TEXTURE_MOD_PATH_MAX];
            uint32_t key;
            int parsed;
            if (!WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), NULL, NULL)) { ++rejected; continue; }
            parsed = SudekiMpTextureModParseLine(utf8, &key, path, sizeof(path));
            if (parsed > 0 && SudekiMpTextureModIndexAdd(&index_table, key, mod, path)) ++added;
            else if (parsed) ++rejected;
        }
        free(section);
    }
    /* [Files]: NAME.EXT (or 0xARCHIVEKEY) = file, a whole archive resource;
     * sound/<file> or movies/<file> = file, a loose game-folder file. */
    if ((section = read_section(manifest, L"Files")) != NULL) {
        wchar_t add_to[64];
        GetPrivateProfileStringW(L"Mod", L"AddToArchive", L"SOLData.baf", add_to, 64, manifest);
        for (line = section; *line; line += wcslen(line) + 1u) {
            char utf8[SUDEKIMP_TEXTURE_MOD_PATH_MAX + SUDEKIMP_MOD_NAME_MAX + 32], name[SUDEKIMP_MOD_NAME_MAX + 1],
                path[SUDEKIMP_TEXTURE_MOD_PATH_MAX];
            wchar_t relative[SUDEKIMP_TEXTURE_MOD_PATH_MAX], full[MAX_PATH];
            uint32_t key;
            int parsed;
            if (!WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), NULL, NULL)) { ++rejected; continue; }
            parsed = SudekiMpModFileParseLine(utf8, &key, name, sizeof(name), path, sizeof(path));
            if (!parsed) continue;
            if (parsed < 0 || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, relative, SUDEKIMP_TEXTURE_MOD_PATH_MAX) ||
                _snwprintf(full, MAX_PATH, L"%ls\\%ls", root, relative) <= 0) { ++rejected; continue; }
            full[MAX_PATH - 1] = 0;
            for (wchar_t *p = full; *p; ++p) if (*p == L'/') *p = L'\\';
            if (strchr(name, '\\') ? SudekiMpLooseModsAdd(name, full, mods[mod].name)
                : SudekiMpArchiveModsAdd(key, name, full, mods[mod].name, add_to)) ++files_added;
            else ++rejected;
        }
        free(section);
    }
    ++mod_count;
    SudekiMpLogFormat("texture_mods event=mod_loaded mod=%s textures=%u files=%u rejected_lines=%u\r\n", mods[mod].name, added, files_added, rejected);
}

static int compare_names(const void *a, const void *b) { return _wcsicmp(*(wchar_t *const *)a, *(wchar_t *const *)b); }

/* Every subfolder of the mods folder with a mod.ini, in case-insensitive name
 * order; a later mod overrides an earlier one for the same key. */
static void load_mods(const wchar_t *folder) {
    wchar_t pattern[MAX_PATH], *names[MAX_MODS];
    unsigned count = 0;
    WIN32_FIND_DATAW found;
    HANDLE search;
    if (_snwprintf(pattern, MAX_PATH, L"%ls\\*", folder) <= 0) return;
    pattern[MAX_PATH - 1] = 0;
    if ((search = FindFirstFileW(pattern, &found)) == INVALID_HANDLE_VALUE) return;
    do {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || found.cFileName[0] == L'.' || found.cFileName[0] == L'_') continue;
        if (count < MAX_MODS && (names[count] = _wcsdup(found.cFileName)) != NULL) ++count;
    } while (FindNextFileW(search, &found));
    FindClose(search);
    qsort(names, count, sizeof(names[0]), compare_names);
    for (unsigned i = 0; i < count; ++i) {
        wchar_t root[MAX_PATH];
        if (_snwprintf(root, MAX_PATH, L"%ls\\%ls", folder, names[i]) > 0) { root[MAX_PATH - 1] = 0; load_mod(root, names[i]); }
        free(names[i]);
    }
}

static BOOL resolve_folder(HMODULE image, const wchar_t *config_path, wchar_t *folder) {
    wchar_t setting[MAX_PATH], game[MAX_PATH], *slash;
    GetPrivateProfileStringW(L"Mods", L"Folder", L"mods", setting, MAX_PATH, config_path);
    if (setting[0] && (setting[1] == L':' || setting[0] == L'\\' || setting[0] == L'/')) {
        wcsncpy(folder, setting, MAX_PATH - 1);
        folder[MAX_PATH - 1] = 0;
        return TRUE;
    }
    if (!GetModuleFileNameW(image, game, MAX_PATH) || !(slash = wcsrchr(game, L'\\'))) return FALSE;
    *slash = 0;
    if (_snwprintf(folder, MAX_PATH, L"%ls\\%ls", game, setting) <= 0) return FALSE;
    folder[MAX_PATH - 1] = 0;
    return TRUE;
}

BOOL SudekiMpTextureModsUninstall(void) {
    if (!SudekiMpLooseModsUninstall()) return FALSE;
    if (!SudekiMpArchiveModsUninstall()) return FALSE;
    if (!base) return TRUE;
    if (decode_hook.installed && !SudekiMpRestoreInlineHook(&decode_hook)) return FALSE;
    SudekiMpLogFormat("texture_mods event=uninstalled decoded=%ld replaced=%ld failed=%ld skipped=%ld\r\n",
        (long)decoded, (long)replaced, (long)failed, (long)skipped);
    /* Index, mod roots, dump table and trampoline stay: a decode thread may
     * still be inside the stub. */
    base = NULL;
    return TRUE;
}

BOOL SudekiMpTextureModsInstall(HMODULE image, const wchar_t *config_path) {
    uint8_t *b = (uint8_t *)image;
    wchar_t folder[MAX_PATH];
    HMODULE d3dx;
    if (base || !b || !config_path || !SudekiMpCheckLoadedExecutable(image)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    if (!config_bool(config_path, L"Mods", L"Enable", TRUE) || !resolve_folder(image, config_path, folder)) {
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    dump_enabled = config_bool(config_path, L"Mods", L"DumpTextures", FALSE);
    load_mods(folder);
    SudekiMpTextureModIndexFinish(&index_table);
    /* Archive files are independent of the texture seam; a refusal leaves
     * the archives untouched and is reported, not fatal. */
    if (!SudekiMpArchiveModsInstall(image))
        SudekiMpLogFormat("archive_mods event=install_failed error=%lu\r\n", (unsigned long)GetLastError());
    if (!SudekiMpLooseModsInstall(image))
        SudekiMpLogFormat("loose_mods event=install_failed error=%lu\r\n", (unsigned long)GetLastError());
    if (!index_table.count && !dump_enabled) { SetLastError(ERROR_SUCCESS); return TRUE; } /* nothing to own */
    /* The game imports d3dx9_30, so it is already mapped; use its loader so
     * DDS, PNG, TGA, BMP and JPG replacements decode exactly as D3DX does. */
    if (!(d3dx = GetModuleHandleW(L"d3dx9_30.dll")) ||
        !(create_from_memory = (CreateTextureFromMemoryEx)(void *)GetProcAddress(d3dx, "D3DXCreateTextureFromFileInMemoryEx"))) {
        /* Optional visual feature: report it and leave the game unhooked. */
        SudekiMpLogFormat("texture_mods event=refused reason=d3dx9_30_loader_missing\r\n");
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    save_to_file = (SaveTextureToFileW)(void *)GetProcAddress(d3dx, "D3DXSaveTextureToFileW");
    if (dump_enabled) {
        InitializeCriticalSection(&dump_lock);
        dump_seen = (uint32_t *)calloc(DUMP_SLOTS, sizeof(uint32_t));
        if (_snwprintf(dump_root, MAX_PATH, L"%ls\\_dump", folder) <= 0) dump_root[0] = 0;
        dump_root[MAX_PATH - 1] = 0;
        (void)CreateDirectoryW(folder, NULL);
        (void)CreateDirectoryW(dump_root, NULL);
    }
    if (!readable(b + DECODE_RVA, DECODE_LENGTH) || memcmp(b + DECODE_RVA, decode_bytes, DECODE_LENGTH)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (!SudekiMpInstallInlineHook(&decode_hook, b + DECODE_RVA, decode_bytes, DECODE_LENGTH, (const void *)(uintptr_t)decode_stub)) return FALSE;
    SudekiMpTextureModsTrampoline = decode_hook.trampoline;
    base = b;
    SudekiMpLogFormat("texture_mods event=installed seam=texture_decode:0x1d8300 mods=%u textures=%u overridden=%u dump=%d\r\n",
        mod_count, (unsigned)index_table.count, (unsigned)index_table.overridden, dump_enabled);
    return TRUE;
}
