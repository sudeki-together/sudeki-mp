#include "hooks/archive_mods.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "engine/texture_mod_index.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Archive mods require the supported x86 ABI"
#endif

enum {
    MOUNT_CALL_RVA = 0x1bd45c, MOUNT_RETURN_RVA = 0x1bd462, MOUNT_CALL_LENGTH = 6,
    IAT_CREATE_FILE = 0x29a0ac, IAT_SET_POINTER = 0x29a0b0, IAT_READ_FILE = 0x29a0dc,
    ARCHIVE_VTABLE = 0x2dce3c, ARCHIVE_INNER_VTABLE = 0x2dce70, CHECKSUM_RVA = 0x3a4a0,
    NEW_ARRAY_RVA = 0x24884e, DELETE_ARRAY_RVA = 0x248916,
    ARCHIVE_SIZE = 0x834, BUCKET_ROWS = 0x10, BUCKET_COUNTS = 0x410, ARCHIVE_HASH = 0x810, ARCHIVE_HANDLE = 0x814,
    MAX_ARCHIVES = 64, MAX_FILES = 65536, MAX_BUCKET_ROWS = 1u << 20
};
#define VIRTUAL_BASE 0x80000000u
#define VIRTUAL_LIMIT 0xfff00000u
/* new[] (0x24884E) and delete[] (0x248916): MOV EDI,EDI; PUSH EBP; MOV EBP,ESP;
 * POP EBP; JMP operator new/delete. The archive parser allocates bucket rows
 * with new[] and its destructor (0x1BD390) frees them with delete[]. */
static const uint8_t new_array_bytes[11] = {0x8b, 0xff, 0x55, 0x8b, 0xec, 0x5d, 0xe9, 0xa1, 0xfc, 0xff, 0xff};
static const uint8_t delete_array_bytes[11] = {0x8b, 0xff, 0x55, 0x8b, 0xec, 0x5d, 0xe9, 0x2e, 0xfb, 0xff, 0xff};
/* 0x1BD449.. PUSH 0; PUSH 0x8000000; PUSH 3; PUSH 0; PUSH 1; PUSH 0x80000000; PUSH EDI */
static const uint8_t mount_args[19] = {0x6a, 0x00, 0x68, 0x00, 0x00, 0x00, 0x08, 0x6a, 0x03, 0x6a, 0x00,
    0x6a, 0x01, 0x68, 0x00, 0x00, 0x00, 0x80, 0x57};

typedef void *(__cdecl *NewArray)(size_t);
typedef void (__cdecl *DeleteArray)(void *);
typedef DWORD (WINAPI *SetPointerFunction)(HANDLE, LONG, PLONG, DWORD);
typedef BOOL (WINAPI *ReadFunction)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);

typedef struct FileMod {
    uint32_t key, size;
    char name[SUDEKIMP_MOD_NAME_MAX + 1], mod[64];
    wchar_t *path, add_archive[64];
    HANDLE file;
    int mapped;
} FileMod;
typedef struct Mapping { uint32_t start, size; FileMod *file; } Mapping;
typedef struct Archive {
    uint8_t *object;
    HANDLE handle;
    char name[64];
    Mapping *maps;
    unsigned count;
    uint32_t pending;    /* virtual position set by the last SetFilePointer */
    DWORD pending_thread;
    int has_pending;
} Archive;

static FileMod *files;
static unsigned file_count, file_capacity;
static Archive archives[MAX_ARCHIVES];
static volatile LONG archive_count;
static uint8_t *base;
static SudekiMpInlineHook mount_hook;
static SudekiMpPointerHook set_pointer_hook, read_hook;
static SetPointerFunction real_set_pointer;
static ReadFunction real_read;
void *SudekiMpArchiveModsCreateSlot __attribute__((used));
void *SudekiMpArchiveModsMountReturn __attribute__((used));

unsigned SudekiMpArchiveModsCount(void) { return file_count; }

static BOOL readable(const void *p, size_t n) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t a = (uintptr_t)p;
    if (!p || !n || a > UINTPTR_MAX - n || VirtualQuery(p, &m, sizeof(m)) != sizeof(m) || m.State != MEM_COMMIT ||
        (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || a + n > (uintptr_t)m.BaseAddress + m.RegionSize) return FALSE;
    return TRUE;
}

static uint32_t u32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void set32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }

int SudekiMpArchiveModsAdd(uint32_t key, const char *name, const wchar_t *path, const char *mod, const wchar_t *add_archive) {
    FileMod *f = NULL;
    if (base || !name || !path || !mod) return 0;
    for (unsigned i = 0; i < file_count; ++i) if (files[i].key == key) { f = &files[i]; free(f->path); break; } /* later mod wins */
    if (!f) {
        if (file_count >= MAX_FILES) return 0;
        if (file_count == file_capacity) {
            unsigned capacity = file_capacity ? file_capacity * 2u : 32u;
            void *p = realloc(files, capacity * sizeof(*files));
            if (!p) return 0;
            files = (FileMod *)p;
            file_capacity = capacity;
        }
        f = &files[file_count++];
    }
    memset(f, 0, sizeof(*f));
    f->key = key;
    f->file = INVALID_HANDLE_VALUE;
    strncpy(f->name, name, sizeof(f->name) - 1u);
    strncpy(f->mod, mod, sizeof(f->mod) - 1u);
    wcsncpy(f->add_archive, add_archive && *add_archive ? add_archive : L"SOLData.baf", 63);
    if (!(f->path = _wcsdup(path))) { --file_count; return 0; }
    return 1;
}

static Archive *find_archive(HANDLE handle) {
    LONG count = archive_count;
    MemoryBarrier();
    for (LONG i = 0; i < count; ++i) if (archives[i].handle == handle) return &archives[i];
    return NULL;
}

/* XBafFileLoader::Read holds the archive lock across both calls, so the
 * pending position is per archive and needs no lock of its own. */
static DWORD WINAPI hook_set_pointer(HANDLE handle, LONG low, PLONG high, DWORD method) {
    Archive *a = handle && handle != INVALID_HANDLE_VALUE ? find_archive(handle) : NULL;
    if (a) {
        a->has_pending = 0;
        if (method == FILE_BEGIN && (!high || !*high) && (uint32_t)low >= VIRTUAL_BASE) {
            a->pending = (uint32_t)low;
            a->pending_thread = GetCurrentThreadId();
            a->has_pending = 1;
            SetLastError(NO_ERROR);
            return (DWORD)low;
        }
    }
    return real_set_pointer(handle, low, high, method);
}

static BOOL serve(Archive *a, uint32_t position, void *buffer, DWORD length, DWORD *got) {
    DWORD read = 0;
    BOOL ok = TRUE;
    for (unsigned i = 0; i < a->count; ++i) {
        Mapping *m = &a->maps[i];
        if (position >= m->start && position - m->start < m->size) {
            uint32_t offset = position - m->start, available = m->size - offset;
            OVERLAPPED at;
            memset(&at, 0, sizeof(at));
            at.Offset = offset; /* positional read: no shared file pointer */
            ok = real_read(m->file->file, buffer, length < available ? length : available, &read, &at);
            break;
        }
    }
    if (read < length) memset((uint8_t *)buffer + read, 0, length - read);
    if (got) *got = read;
    return ok;
}

static BOOL WINAPI hook_read(HANDLE handle, LPVOID buffer, DWORD length, LPDWORD got, LPOVERLAPPED overlapped) {
    Archive *a = handle && handle != INVALID_HANDLE_VALUE && !overlapped ? find_archive(handle) : NULL;
    if (a && a->has_pending && a->pending_thread == GetCurrentThreadId()) {
        uint32_t position = a->pending;
        a->has_pending = 0;
        a->pending = position + length;
        return serve(a, position, buffer, length, got);
    }
    if (a) a->has_pending = 0;
    return real_read(handle, buffer, length, got, overlapped);
}

static BOOL open_file(FileMod *f) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (f->file != INVALID_HANDLE_VALUE) return TRUE;
    if (!GetFileAttributesExW(f->path, GetFileExInfoStandard, &data) || data.nFileSizeHigh ||
        !data.nFileSizeLow || data.nFileSizeLow > 0x7ff00000u) return FALSE;
    f->file = CreateFileW(f->path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f->file == INVALID_HANDLE_VALUE) return FALSE;
    f->size = data.nFileSizeLow;
    return TRUE;
}

static BOOL add_mapping(Archive *a, FileMod *f, uint32_t *next, uint32_t *start) {
    uint32_t span = (f->size + 2047u) & ~2047u;
    Mapping *m;
    if (*next > VIRTUAL_LIMIT - span) return FALSE;
    if (!(m = (Mapping *)realloc(a->maps, (a->count + 1u) * sizeof(*m)))) return FALSE;
    a->maps = m;
    a->maps[a->count].start = *start = *next;
    a->maps[a->count].size = f->size;
    a->maps[a->count].file = f;
    ++a->count;
    *next += span;
    return TRUE;
}

static int name_matches(const char *archive, const wchar_t *wanted) {
    wchar_t wide[64];
    if (!MultiByteToWideChar(CP_ACP, 0, archive, -1, wide, 64)) return 0;
    return !_wcsicmp(wide, wanted);
}

/* Mount thread, right after the native CreateFileA; nothing has looked the
 * archive up yet (it is not registered until the mount returns). */
static void __attribute__((used, noinline)) observe_mount(uint8_t *archive, HANDLE handle, const char *path) {
    DWORD saved = GetLastError();
    Archive *a;
    uint32_t next = VIRTUAL_BASE;
    unsigned replaced = 0, added = 0, failed = 0;
    const char *slash;
    if (!base || handle == INVALID_HANDLE_VALUE || !handle || archive_count >= MAX_ARCHIVES ||
        !readable(archive, ARCHIVE_SIZE) || u32(archive) != (uint32_t)(uintptr_t)(base + ARCHIVE_VTABLE) ||
        u32(archive + 4) != (uint32_t)(uintptr_t)(base + ARCHIVE_INNER_VTABLE)) goto done;
    a = &archives[archive_count];
    memset(a, 0, sizeof(*a));
    slash = path && readable(path, 1) ? path : "?";
    for (const char *p = slash; *p; ++p) if (*p == '\\' || *p == '/') slash = p + 1;
    strncpy(a->name, slash, sizeof(a->name) - 1u);
    if (u32(archive + ARCHIVE_HASH) != (uint32_t)(uintptr_t)(base + CHECKSUM_RVA)) {
        SudekiMpLogFormat("archive_mods event=archive_skipped archive=%s reason=hash_function\r\n", a->name);
        goto done;
    }
    for (unsigned i = 0; i < file_count; ++i) {
        FileMod *f = &files[i];
        unsigned bucket = f->key & 255u;
        uint32_t rows = u32(archive + BUCKET_COUNTS + bucket * 4u), start;
        uint8_t *table = (uint8_t *)(uintptr_t)u32(archive + BUCKET_ROWS + bucket * 4u);
        long row;
        if (rows > MAX_BUCKET_ROWS || (rows && !readable(table, rows * 12u))) { ++failed; continue; }
        row = SudekiMpArchiveBucketFind(table, rows, f->key);
        if (row < 0 && !name_matches(a->name, f->add_archive)) continue;
        if (!open_file(f) || !add_mapping(a, f, &next, &start)) {
            ++failed;
            SudekiMpLogFormat("archive_mods event=file_failed mod=%s name=%s archive=%s\r\n", f->mod, f->name, a->name);
            continue;
        }
        if (row >= 0) {
            set32(table + (uint32_t)row * 12u, start);
            set32(table + (uint32_t)row * 12u + 4u, f->size);
            ++replaced;
        } else {
            uint8_t *grown = (uint8_t *)((NewArray)(void *)(base + NEW_ARRAY_RVA))((rows + 1u) * 12u);
            if (!grown) { --a->count; ++failed; continue; }
            SudekiMpArchiveBucketInsert(grown, table, rows, start, f->size, f->key);
            set32(archive + BUCKET_ROWS + bucket * 4u, (uint32_t)(uintptr_t)grown);
            set32(archive + BUCKET_COUNTS + bucket * 4u, rows + 1u);
            if (table) ((DeleteArray)(void *)(base + DELETE_ARRAY_RVA))(table);
            ++added;
        }
        f->mapped = 1;
        SudekiMpLogFormat("archive_mods event=%s mod=%s name=%s key=0x%08lX archive=%s size=%lu\r\n",
            row >= 0 ? "replace" : "add", f->mod, f->name, (unsigned long)f->key, a->name, (unsigned long)f->size);
    }
    SudekiMpLogFormat("archive_mods event=mount archive=%s replaced=%u added=%u failed=%u\r\n", a->name, replaced, added, failed);
    if (a->count) {
        a->object = archive;
        a->handle = handle;
        MemoryBarrier();
        InterlockedIncrement(&archive_count); /* publish after the record is complete */
    }
done:
    SetLastError(saved);
}

/* Replaces "CALL [CreateFileA]" (6 bytes): the seven pushed arguments are
 * already on the stack, EBP = archive, EDI = path. Call through the same slot
 * (stdcall pops the arguments), observe with the result, resume at 0x1BD462. */
static void __attribute__((naked, noinline)) mount_stub(void) {
    __asm__ volatile(
        "mov _SudekiMpArchiveModsCreateSlot,%eax; call *(%eax);"
        "pushal; mov %esp,%esi; and $-16,%esp; sub $16,%esp;"
        "mov %ebp,(%esp); mov 28(%esi),%eax; mov %eax,4(%esp); mov %edi,8(%esp); call _observe_mount;"
        "mov %esi,%esp; popal; jmp *_SudekiMpArchiveModsMountReturn");
}

BOOL SudekiMpArchiveModsUninstall(void) {
    if (!base) return TRUE;
    if (mount_hook.installed && !SudekiMpRestoreInlineHook(&mount_hook)) return FALSE;
    if (archive_count) {
        /* Patched index rows point at virtual offsets: the read hooks must stay. */
        SudekiMpLogFormat("archive_mods event=uninstall_refused reason=archives_patched archives=%ld\r\n", (long)archive_count);
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (read_hook.installed && !SudekiMpRestorePointerHook(&read_hook)) return FALSE;
    if (set_pointer_hook.installed && !SudekiMpRestorePointerHook(&set_pointer_hook)) return FALSE;
    base = NULL;
    return TRUE;
}

BOOL SudekiMpArchiveModsInstall(HMODULE image) {
    uint8_t *b = (uint8_t *)image, call[MOUNT_CALL_LENGTH] = {0xff, 0x15};
    void *create_slot = b + IAT_CREATE_FILE;
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    FARPROC set_pointer = kernel ? GetProcAddress(kernel, "SetFilePointer") : NULL;
    FARPROC read_file = kernel ? GetProcAddress(kernel, "ReadFile") : NULL;
    if (base || !b || !SudekiMpCheckLoadedExecutable(image)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    if (!file_count) { SetLastError(ERROR_SUCCESS); return TRUE; } /* nothing to own */
    memcpy(call + 2, &create_slot, 4);
    if (!readable(b + MOUNT_CALL_RVA - sizeof(mount_args), sizeof(mount_args) + MOUNT_CALL_LENGTH) ||
        memcmp(b + MOUNT_CALL_RVA - sizeof(mount_args), mount_args, sizeof(mount_args)) ||
        memcmp(b + MOUNT_CALL_RVA, call, sizeof(call)) ||
        memcmp(b + NEW_ARRAY_RVA, new_array_bytes, sizeof(new_array_bytes)) ||
        memcmp(b + DELETE_ARRAY_RVA, delete_array_bytes, sizeof(delete_array_bytes)) ||
        !set_pointer || !read_file || *(FARPROC *)(b + IAT_SET_POINTER) != set_pointer ||
        *(FARPROC *)(b + IAT_READ_FILE) != read_file) {
        SudekiMpLogFormat("archive_mods event=refused reason=unexpected_image_or_import\r\n");
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    real_set_pointer = (SetPointerFunction)(void *)set_pointer;
    real_read = (ReadFunction)(void *)read_file;
    SudekiMpArchiveModsCreateSlot = create_slot;
    SudekiMpArchiveModsMountReturn = b + MOUNT_RETURN_RVA;
    base = b; /* observers check it; set before the first hook can run */
    if (!SudekiMpInstallPointerHook(&set_pointer_hook, (void **)(b + IAT_SET_POINTER), (const void *)set_pointer, (const void *)(uintptr_t)hook_set_pointer) ||
        !SudekiMpInstallPointerHook(&read_hook, (void **)(b + IAT_READ_FILE), (const void *)read_file, (const void *)(uintptr_t)hook_read) ||
        !SudekiMpInstallInlineHook(&mount_hook, b + MOUNT_CALL_RVA, call, MOUNT_CALL_LENGTH, (const void *)(uintptr_t)mount_stub)) {
        DWORD error = GetLastError();
        if (read_hook.installed) (void)SudekiMpRestorePointerHook(&read_hook);
        if (set_pointer_hook.installed) (void)SudekiMpRestorePointerHook(&set_pointer_hook);
        base = NULL;
        SetLastError(error);
        return FALSE;
    }
    SudekiMpLogFormat("archive_mods event=installed seams=mount_call:0x1bd45c,iat:SetFilePointer,ReadFile files=%u\r\n", file_count);
    return TRUE;
}
