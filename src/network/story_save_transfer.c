#include "network/story_save_transfer.h"
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* Body layouts (little endian, unused bytes zero and checked):
 * offer: "SVOF" id fish_size bunny_size chunk_bytes(16) chunk_count fish_sha bunny_sha
 * chunk: "SVCH" id index length(16) 0(16) data[448] */
enum {
    OFFER_ID = 4, OFFER_FISH = 8, OFFER_BUNNY = 12, OFFER_CHUNK = 16, OFFER_COUNT = 18,
    OFFER_FISH_SHA = 22, OFFER_BUNNY_SHA = 54, OFFER_END = 86,
    CHUNK_ID = 4, CHUNK_INDEX = 8, CHUNK_LENGTH = 12, CHUNK_PAD = 14, CHUNK_DATA = 16,
    CHUNK_END = CHUNK_DATA + SUDEKIMP_SAVE_TRANSFER_CHUNK_BYTES
};
_Static_assert((unsigned)CHUNK_END <= (unsigned)SUDEKIMP_SAVE_TRANSFER_BODY_BYTES, "chunk fits a lobby body");
_Static_assert((unsigned)OFFER_END <= (unsigned)SUDEKIMP_SAVE_TRANSFER_BODY_BYTES, "offer fits a lobby body");

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { for (unsigned i = 0; i < 4u; ++i) p[i] = (uint8_t)(v >> (8u * i)); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static BOOL zero(const uint8_t *p, size_t n) { for (size_t i = 0; i < n; ++i) if (p[i]) return FALSE; return TRUE; }
static void refuse(SudekiMpSaveTransferRefusal *why, SudekiMpSaveTransferRefusal value) { if (why) *why = value; }
static uint32_t chunk_length(uint32_t index) {
    uint32_t start = index * SUDEKIMP_SAVE_TRANSFER_CHUNK_BYTES;
    uint32_t left = SUDEKIMP_SAVE_TRANSFER_TOTAL_BYTES - start;
    return left < SUDEKIMP_SAVE_TRANSFER_CHUNK_BYTES ? left : SUDEKIMP_SAVE_TRANSFER_CHUNK_BYTES;
}

BOOL SudekiMpSaveTransferHash(const uint8_t *bytes, size_t size, uint8_t out[32]) {
    BCRYPT_ALG_HANDLE algorithm = NULL; BCRYPT_HASH_HANDLE hash = NULL; BOOL ok = FALSE;
    if (!out || (!bytes && size) || size > 0xffffffffu) return FALSE;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0) return FALSE;
    if (BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0) >= 0 &&
        (!size || BCryptHashData(hash, (PUCHAR)(uintptr_t)bytes, (ULONG)size, 0) >= 0) &&
        BCryptFinishHash(hash, out, 32, 0) >= 0) ok = TRUE;
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return ok;
}
static BOOL identity_of(const uint8_t *stream, SudekiMpSaveTransferIdentity *out) {
    return SudekiMpSaveTransferHash(stream, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES, out->fish_sha256) &&
        SudekiMpSaveTransferHash(stream + SUDEKIMP_SAVE_TRANSFER_FISH_BYTES,
            SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES, out->bunny_sha256);
}
static BOOL identity_valid(const SudekiMpSaveTransferIdentity *identity) {
    return identity && !zero(identity->fish_sha256, 32) && !zero(identity->bunny_sha256, 32);
}

BOOL SudekiMpSaveTransferSenderBegin(SudekiMpSaveTransferSender *sender, uint32_t transfer_id,
    const uint8_t *fish, size_t fish_size, const uint8_t *bunny, size_t bunny_size,
    const SudekiMpSaveTransferIdentity *published, SudekiMpSaveTransferRefusal *why) {
    SudekiMpSaveTransferIdentity actual;
    refuse(why, SUDEKIMP_SAVE_TRANSFER_OK);
    if (!sender || !transfer_id || !fish || !bunny || !identity_valid(published)) {
        refuse(why, SUDEKIMP_SAVE_TRANSFER_STATE); return FALSE;
    }
    memset(sender, 0, sizeof(*sender));
    if (fish_size != SUDEKIMP_SAVE_TRANSFER_FISH_BYTES || bunny_size != SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES) {
        refuse(why, SUDEKIMP_SAVE_TRANSFER_BAD_SIZE); return FALSE;
    }
    uint8_t *stream = malloc(SUDEKIMP_SAVE_TRANSFER_TOTAL_BYTES);
    if (!stream) { refuse(why, SUDEKIMP_SAVE_TRANSFER_NO_MEMORY); return FALSE; }
    memcpy(stream, fish, fish_size);
    memcpy(stream + fish_size, bunny, bunny_size);
    if (!identity_of(stream, &actual) || memcmp(&actual, published, sizeof(actual))) {
        free(stream); refuse(why, SUDEKIMP_SAVE_TRANSFER_HASH_MISMATCH); return FALSE;
    }
    sender->transfer_id = transfer_id; sender->stream = stream; sender->identity = actual;
    return TRUE;
}
void SudekiMpSaveTransferSenderEnd(SudekiMpSaveTransferSender *sender) {
    if (!sender) return;
    free(sender->stream);
    memset(sender, 0, sizeof(*sender));
}
BOOL SudekiMpSaveTransferSenderOffer(const SudekiMpSaveTransferSender *sender,
    uint8_t body[SUDEKIMP_SAVE_TRANSFER_BODY_BYTES]) {
    if (!sender || !sender->stream || !sender->transfer_id || !body) return FALSE;
    memset(body, 0, SUDEKIMP_SAVE_TRANSFER_BODY_BYTES);
    memcpy(body, "SVOF", 4);
    put32(body + OFFER_ID, sender->transfer_id);
    put32(body + OFFER_FISH, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES);
    put32(body + OFFER_BUNNY, SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES);
    put16(body + OFFER_CHUNK, SUDEKIMP_SAVE_TRANSFER_CHUNK_BYTES);
    put32(body + OFFER_COUNT, SUDEKIMP_SAVE_TRANSFER_CHUNKS);
    memcpy(body + OFFER_FISH_SHA, sender->identity.fish_sha256, 32);
    memcpy(body + OFFER_BUNNY_SHA, sender->identity.bunny_sha256, 32);
    return TRUE;
}
BOOL SudekiMpSaveTransferSenderNext(SudekiMpSaveTransferSender *sender,
    uint8_t body[SUDEKIMP_SAVE_TRANSFER_BODY_BYTES], BOOL *done) {
    if (done) *done = FALSE;
    if (!sender || !sender->stream || !body || !done || sender->next_chunk >= SUDEKIMP_SAVE_TRANSFER_CHUNKS) return FALSE;
    uint32_t index = sender->next_chunk, length = chunk_length(index);
    memset(body, 0, SUDEKIMP_SAVE_TRANSFER_BODY_BYTES);
    memcpy(body, "SVCH", 4);
    put32(body + CHUNK_ID, sender->transfer_id);
    put32(body + CHUNK_INDEX, index);
    put16(body + CHUNK_LENGTH, (uint16_t)length);
    memcpy(body + CHUNK_DATA, sender->stream + (size_t)index * SUDEKIMP_SAVE_TRANSFER_CHUNK_BYTES, length);
    *done = ++sender->next_chunk == SUDEKIMP_SAVE_TRANSFER_CHUNKS;
    return TRUE;
}

BOOL SudekiMpSaveTransferReceiverOffer(SudekiMpSaveTransferReceiver *receiver,
    const uint8_t body[SUDEKIMP_SAVE_TRANSFER_BODY_BYTES],
    const SudekiMpSaveTransferIdentity *published, SudekiMpSaveTransferRefusal *why) {
    refuse(why, SUDEKIMP_SAVE_TRANSFER_OK);
    if (!receiver || !body || !identity_valid(published)) { refuse(why, SUDEKIMP_SAVE_TRANSFER_STATE); return FALSE; }
    if (memcmp(body, "SVOF", 4) || !get32(body + OFFER_ID) ||
        !zero(body + OFFER_END, SUDEKIMP_SAVE_TRANSFER_BODY_BYTES - OFFER_END)) {
        refuse(why, SUDEKIMP_SAVE_TRANSFER_BAD_BODY); return FALSE;
    }
    if (get32(body + OFFER_FISH) != SUDEKIMP_SAVE_TRANSFER_FISH_BYTES ||
        get32(body + OFFER_BUNNY) != SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES ||
        get16(body + OFFER_CHUNK) != SUDEKIMP_SAVE_TRANSFER_CHUNK_BYTES ||
        get32(body + OFFER_COUNT) != SUDEKIMP_SAVE_TRANSFER_CHUNKS) {
        refuse(why, SUDEKIMP_SAVE_TRANSFER_BAD_SIZE); return FALSE;
    }
    if (memcmp(body + OFFER_FISH_SHA, published->fish_sha256, 32) ||
        memcmp(body + OFFER_BUNNY_SHA, published->bunny_sha256, 32)) {
        refuse(why, SUDEKIMP_SAVE_TRANSFER_WRONG_IDENTITY); return FALSE;
    }
    uint8_t *stream = malloc(SUDEKIMP_SAVE_TRANSFER_TOTAL_BYTES);
    if (!stream) { refuse(why, SUDEKIMP_SAVE_TRANSFER_NO_MEMORY); return FALSE; }
    SudekiMpSaveTransferReceiverEnd(receiver);
    receiver->transfer_id = get32(body + OFFER_ID);
    receiver->stream = stream;
    receiver->identity = *published;
    return TRUE;
}
SudekiMpSaveTransferStatus SudekiMpSaveTransferReceiverChunk(SudekiMpSaveTransferReceiver *receiver,
    const uint8_t body[SUDEKIMP_SAVE_TRANSFER_BODY_BYTES], SudekiMpSaveTransferRefusal *why) {
    refuse(why, SUDEKIMP_SAVE_TRANSFER_OK);
    if (!receiver || !body || !receiver->stream || receiver->complete) {
        refuse(why, SUDEKIMP_SAVE_TRANSFER_STATE); return SUDEKIMP_SAVE_TRANSFER_REFUSED;
    }
    uint32_t index = get32(body + CHUNK_INDEX), length = get16(body + CHUNK_LENGTH);
    if (memcmp(body, "SVCH", 4) || get16(body + CHUNK_PAD) || index >= SUDEKIMP_SAVE_TRANSFER_CHUNKS ||
        length != chunk_length(index) ||
        !zero(body + CHUNK_DATA + length, SUDEKIMP_SAVE_TRANSFER_BODY_BYTES - CHUNK_DATA - length)) {
        refuse(why, SUDEKIMP_SAVE_TRANSFER_BAD_BODY); return SUDEKIMP_SAVE_TRANSFER_REFUSED;
    }
    if (get32(body + CHUNK_ID) != receiver->transfer_id) {
        refuse(why, SUDEKIMP_SAVE_TRANSFER_WRONG_TRANSFER); return SUDEKIMP_SAVE_TRANSFER_REFUSED;
    }
    if (index != receiver->next_chunk) {
        refuse(why, SUDEKIMP_SAVE_TRANSFER_OUT_OF_ORDER); return SUDEKIMP_SAVE_TRANSFER_REFUSED;
    }
    memcpy(receiver->stream + (size_t)index * SUDEKIMP_SAVE_TRANSFER_CHUNK_BYTES, body + CHUNK_DATA, length);
    if (++receiver->next_chunk < SUDEKIMP_SAVE_TRANSFER_CHUNKS) return SUDEKIMP_SAVE_TRANSFER_PROGRESS;
    SudekiMpSaveTransferIdentity actual;
    if (!identity_of(receiver->stream, &actual) || memcmp(&actual, &receiver->identity, sizeof(actual))) {
        SudekiMpSaveTransferReceiverEnd(receiver);
        refuse(why, SUDEKIMP_SAVE_TRANSFER_HASH_MISMATCH); return SUDEKIMP_SAVE_TRANSFER_REFUSED;
    }
    receiver->complete = TRUE;
    return SUDEKIMP_SAVE_TRANSFER_COMPLETE;
}
BOOL SudekiMpSaveTransferReceiverRecords(const SudekiMpSaveTransferReceiver *receiver,
    const uint8_t **fish, const uint8_t **bunny) {
    if (!receiver || !receiver->complete || !receiver->stream || !fish || !bunny) return FALSE;
    *fish = receiver->stream;
    *bunny = receiver->stream + SUDEKIMP_SAVE_TRANSFER_FISH_BYTES;
    return TRUE;
}
void SudekiMpSaveTransferReceiverEnd(SudekiMpSaveTransferReceiver *receiver) {
    if (!receiver) return;
    free(receiver->stream);
    memset(receiver, 0, sizeof(*receiver));
}
unsigned SudekiMpSaveTransferReceiverPermille(const SudekiMpSaveTransferReceiver *receiver) {
    if (!receiver || !receiver->stream) return 0u;
    if (receiver->complete) return 1000u;
    return (unsigned)(((uint64_t)receiver->next_chunk * 1000u) / SUDEKIMP_SAVE_TRANSFER_CHUNKS);
}

/* ---- staging ---- */
static BOOL path_join(wchar_t out[MAX_PATH], const wchar_t *a, const wchar_t *b) {
    int n = _snwprintf(out, MAX_PATH, L"%ls\\%ls", a, b);
    return n > 0 && n < MAX_PATH;
}
static BOOL plain_directory(const wchar_t *path) {
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) &&
        !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
}
static BOOL read_exact(const wchar_t *path, uint8_t *buffer, DWORD size) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    BY_HANDLE_FILE_INFORMATION info; DWORD got = 0;
    BOOL ok = GetFileInformationByHandle(file, &info) && !info.nFileSizeHigh && info.nFileSizeLow == size &&
        !(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) &&
        ReadFile(file, buffer, size, &got, NULL) && got == size;
    CloseHandle(file);
    return ok;
}
static BOOL file_matches(const wchar_t *path, DWORD size, const uint8_t sha[32], uint8_t *scratch) {
    uint8_t actual[32];
    return read_exact(path, scratch, size) && SudekiMpSaveTransferHash(scratch, size, actual) && !memcmp(actual, sha, 32);
}
static BOOL write_new(const wchar_t *path, const uint8_t *bytes, DWORD size) {
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    DWORD written = 0;
    BOOL ok = WriteFile(file, bytes, size, &written, NULL) && written == size && FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok) DeleteFileW(path);
    return ok;
}
static BOOL stage_file(const wchar_t *dir, const wchar_t *name, const uint8_t *bytes, DWORD size,
    const uint8_t sha[32], uint8_t *scratch) {
    wchar_t final_path[MAX_PATH], temp_name[64], temp_path[MAX_PATH];
    if (!path_join(final_path, dir, name)) return FALSE;
    if (file_matches(final_path, size, sha, scratch)) return TRUE; /* same verified content */
    if (GetFileAttributesW(final_path) != INVALID_FILE_ATTRIBUTES) { SetLastError(ERROR_FILE_EXISTS); return FALSE; }
    _snwprintf(temp_name, 64, L"%ls.%08lx.tmp", name, (unsigned long)GetCurrentProcessId());
    temp_name[63] = 0;
    if (!path_join(temp_path, dir, temp_name)) return FALSE;
    DeleteFileW(temp_path);
    if (!write_new(temp_path, bytes, size) || !file_matches(temp_path, size, sha, scratch) ||
        !MoveFileExW(temp_path, final_path, MOVEFILE_WRITE_THROUGH)) {
        DWORD error = GetLastError(); DeleteFileW(temp_path); SetLastError(error); return FALSE;
    }
    return file_matches(final_path, size, sha, scratch);
}
BOOL SudekiMpSaveTransferStage(const wchar_t *root, const uint8_t *fish, const uint8_t *bunny,
    const SudekiMpSaveTransferIdentity *identity, wchar_t out_dir[MAX_PATH]) {
    wchar_t name[17], dir[MAX_PATH];
    uint8_t actual[32];
    if (!root || !fish || !bunny || !identity_valid(identity) || !out_dir || !plain_directory(root)) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    if (!SudekiMpSaveTransferHash(fish, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES, actual) ||
        memcmp(actual, identity->fish_sha256, 32) ||
        !SudekiMpSaveTransferHash(bunny, SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES, actual) ||
        memcmp(actual, identity->bunny_sha256, 32)) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    for (unsigned i = 0; i < 8u; ++i) _snwprintf(name + i * 2u, 3, L"%02x", identity->fish_sha256[i]);
    name[16] = 0;
    if (!path_join(dir, root, name)) { SetLastError(ERROR_FILENAME_EXCED_RANGE); return FALSE; }
    if (!CreateDirectoryW(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) return FALSE;
    if (!plain_directory(dir)) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    uint8_t *scratch = malloc(SUDEKIMP_SAVE_TRANSFER_FISH_BYTES);
    if (!scratch) { SetLastError(ERROR_OUTOFMEMORY); return FALSE; }
    BOOL ok = stage_file(dir, L"sudeki.fish", fish, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES, identity->fish_sha256, scratch) &&
        stage_file(dir, L"fluffy.bunny", bunny, SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES, identity->bunny_sha256, scratch);
    DWORD error = GetLastError();
    free(scratch);
    if (ok) wcscpy(out_dir, dir);
    SetLastError(ok ? ERROR_SUCCESS : error);
    return ok;
}

/* ---- reserved native save folders ---- */
static const char marker_header[] = "SudekiMP received save\r\n";
static BOOL marker_path(wchar_t out[MAX_PATH], const wchar_t *dir) { return path_join(out, dir, L"sudekimp-received.txt"); }
static BOOL slot_dir(wchar_t out[MAX_PATH], const wchar_t *root, unsigned slot) {
    int n = _snwprintf(out, MAX_PATH, L"%ls\\SAVESLOT%04u", root, slot);
    return n > 0 && n < MAX_PATH;
}
static void hex(char *out, const uint8_t *bytes, unsigned count) {
    static const char digits[] = "0123456789abcdef";
    for (unsigned i = 0; i < count; ++i) { out[i * 2u] = digits[bytes[i] >> 4]; out[i * 2u + 1u] = digits[bytes[i] & 15u]; }
    out[count * 2u] = 0;
}
/* The small record stores its own folder ("SAVESLOTdddd\\", zero padded) at
 * 0x200; the native list loads from that stored folder (CONFIRMED_LIVE
 * 2026-10-16: a copy left in another folder failed the catalog row match). */
enum { BUNNY_PATH = 0x200u, BUNNY_PATH_BYTES = 0x10u }; /* 0x210.. holds other record data */
static BOOL stored_slot(const uint8_t *bunny, unsigned *slot) {
    const uint8_t *p = bunny + BUNNY_PATH; unsigned value = 0;
    /* NUL-terminated; bytes after the terminator are left-over buffer
     * contents in real records (SAVESLOT0000 carries d7 75) and are kept. */
    if (memcmp(p, "SAVESLOT", 8) || p[12] != '\\' || p[13]) return FALSE;
    for (unsigned i = 8; i < 12u; ++i) { if (p[i] < '0' || p[i] > '9') return FALSE; value = value * 10u + (p[i] - '0'); }
    *slot = value; return TRUE;
}
/* Only the four folder digits change; every other byte stays the host's. */
static void set_stored_slot(uint8_t *bunny, unsigned slot) {
    slot %= 10000u;
    for (int i = 11; i >= 8; --i) { bunny[BUNNY_PATH + (unsigned)i] = (uint8_t)('0' + slot % 10u); slot /= 10u; }
}
static unsigned marker_text(char *out, size_t capacity, const SudekiMpSaveTransferIdentity *identity, unsigned original) {
    char fish_hex[65], bunny_hex[65];
    hex(fish_hex, identity->fish_sha256, 32); hex(bunny_hex, identity->bunny_sha256, 32);
    int n = _snprintf(out, capacity, "%sfish=%s\r\nbunny=%s\r\nslot=%04u\r\n", marker_header, fish_hex, bunny_hex, original);
    return n > 0 && (size_t)n < capacity ? (unsigned)n : 0u;
}
/* TRUE when dir carries a SudekiMP marker; *age_seconds from its write time,
 * *original from its slot line (0xffffffff when absent). */
static BOOL marked_detail(const wchar_t *dir, uint64_t *age_seconds, char *text_out, size_t text_capacity) {
    wchar_t path[MAX_PATH]; char text[256]; DWORD got = 0; FILETIME written, now; BOOL ok;
    if (!marker_path(path, dir)) return FALSE;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    ok = ReadFile(file, text, sizeof(text) - 1u, &got, NULL) && got >= sizeof(marker_header) - 1u &&
        !memcmp(text, marker_header, sizeof(marker_header) - 1u) && GetFileTime(file, NULL, NULL, &written);
    CloseHandle(file);
    if (!ok) return FALSE;
    text[got] = 0;
    if (text_out && text_capacity) { strncpy(text_out, text, text_capacity - 1u); text_out[text_capacity - 1u] = 0; }
    if (age_seconds) {
        GetSystemTimeAsFileTime(&now);
        uint64_t a = ((uint64_t)written.dwHighDateTime << 32) | written.dwLowDateTime;
        uint64_t b = ((uint64_t)now.dwHighDateTime << 32) | now.dwLowDateTime;
        *age_seconds = b > a ? (b - a) / 10000000u : 0u;
    }
    return TRUE;
}
static BOOL marked(const wchar_t *dir, uint64_t *age_seconds) { return marked_detail(dir, age_seconds, NULL, 0); }
/* A reserved folder holds the published pair when: its marker names exactly
 * this identity and an original slot; the main record hashes to the published
 * value; the small record names this folder, and with the original folder
 * put back hashes to the published value. */
static BOOL holds(const wchar_t *dir, unsigned folder_slot, const SudekiMpSaveTransferIdentity *identity, uint8_t *scratch) {
    wchar_t fish_path[MAX_PATH], bunny_path[MAX_PATH]; char text[256], expected[256];
    uint8_t bunny[SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES], actual[32]; unsigned stored = 0, original = 0;
    if (!marked_detail(dir, NULL, text, sizeof(text))) return FALSE;
    const char *line = strstr(text, "slot=");
    if (!line || sscanf(line + 5, "%4u", &original) != 1 || original > 9999u) return FALSE;
    unsigned length = marker_text(expected, sizeof(expected), identity, original);
    if (!length || strcmp(text, expected)) return FALSE;
    if (!path_join(fish_path, dir, L"sudeki.fish") || !path_join(bunny_path, dir, L"fluffy.bunny") ||
        !file_matches(fish_path, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES, identity->fish_sha256, scratch) ||
        !read_exact(bunny_path, bunny, sizeof(bunny)) || !stored_slot(bunny, &stored) || stored != folder_slot) return FALSE;
    set_stored_slot(bunny, original);
    return SudekiMpSaveTransferHash(bunny, sizeof(bunny), actual) && !memcmp(actual, identity->bunny_sha256, 32);
}
static void remove_ours(const wchar_t *dir) {
    wchar_t path[MAX_PATH];
    if (path_join(path, dir, L"sudeki.fish")) DeleteFileW(path);
    if (path_join(path, dir, L"fluffy.bunny")) DeleteFileW(path);
    if (marker_path(path, dir)) DeleteFileW(path);
    RemoveDirectoryW(dir); /* fails, and keeps the folder, if anything else is in it */
}
BOOL SudekiMpSaveTransferFindImported(const wchar_t *save_root, const SudekiMpSaveTransferIdentity *identity, unsigned *slot) {
    wchar_t dir[MAX_PATH];
    if (!save_root || !identity_valid(identity) || !slot || !plain_directory(save_root)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    uint8_t *scratch = malloc(SUDEKIMP_SAVE_TRANSFER_FISH_BYTES);
    if (!scratch) { SetLastError(ERROR_OUTOFMEMORY); return FALSE; }
    BOOL found = FALSE;
    for (unsigned n = SUDEKIMP_SAVE_TRANSFER_FIRST_SLOT; n <= SUDEKIMP_SAVE_TRANSFER_LAST_SLOT && !found; ++n)
        if (slot_dir(dir, save_root, n) && plain_directory(dir) && holds(dir, n, identity, scratch)) { *slot = n; found = TRUE; }
    free(scratch);
    SetLastError(found ? ERROR_SUCCESS : ERROR_FILE_NOT_FOUND);
    return found;
}
BOOL SudekiMpSaveTransferImport(const wchar_t *save_root, const uint8_t *fish, const uint8_t *bunny,
    const SudekiMpSaveTransferIdentity *identity, unsigned *slot) {
    wchar_t dir[MAX_PATH], path[MAX_PATH]; char text[256];
    uint8_t local_bunny[SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES], local_hash[32];
    unsigned free_slot = 0u, original = 0u;
    if (!save_root || !fish || !bunny || !identity_valid(identity) || !slot || !plain_directory(save_root)) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    uint8_t actual[32];
    if (!SudekiMpSaveTransferHash(fish, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES, actual) || memcmp(actual, identity->fish_sha256, 32) ||
        !SudekiMpSaveTransferHash(bunny, SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES, actual) || memcmp(actual, identity->bunny_sha256, 32) ||
        !stored_slot(bunny, &original)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    if (SudekiMpSaveTransferFindImported(save_root, identity, slot)) return TRUE;
    uint8_t *scratch = malloc(SUDEKIMP_SAVE_TRANSFER_FISH_BYTES);
    if (!scratch) { SetLastError(ERROR_OUTOFMEMORY); return FALSE; }
    for (unsigned n = SUDEKIMP_SAVE_TRANSFER_FIRST_SLOT; n <= SUDEKIMP_SAVE_TRANSFER_LAST_SLOT && !free_slot; ++n)
        if (slot_dir(dir, save_root, n) && GetFileAttributesW(dir) == INVALID_FILE_ATTRIBUTES) free_slot = n;
    BOOL ok = FALSE; DWORD error = ERROR_DISK_FULL;
    unsigned length = marker_text(text, sizeof(text), identity, original);
    if (free_slot) {
        memcpy(local_bunny, bunny, sizeof(local_bunny));
        set_stored_slot(local_bunny, free_slot);
    }
    if (free_slot && length && SudekiMpSaveTransferHash(local_bunny, sizeof(local_bunny), local_hash) &&
        slot_dir(dir, save_root, free_slot) && CreateDirectoryW(dir, NULL)) {
        ok = marker_path(path, dir) && write_new(path, (const uint8_t *)text, length) &&
            stage_file(dir, L"sudeki.fish", fish, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES, identity->fish_sha256, scratch) &&
            stage_file(dir, L"fluffy.bunny", local_bunny, SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES, local_hash, scratch) &&
            holds(dir, free_slot, identity, scratch);
        error = GetLastError();
        if (!ok) remove_ours(dir);
    } else if (free_slot) error = GetLastError();
    free(scratch);
    if (ok) *slot = free_slot;
    SetLastError(ok ? ERROR_SUCCESS : error);
    return ok;
}
BOOL SudekiMpSaveTransferExpire(const wchar_t *save_root, uint64_t max_age_seconds, unsigned *removed) {
    wchar_t dir[MAX_PATH]; uint64_t age = 0;
    if (removed) *removed = 0u;
    if (!save_root || !plain_directory(save_root)) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    for (unsigned n = SUDEKIMP_SAVE_TRANSFER_FIRST_SLOT; n <= SUDEKIMP_SAVE_TRANSFER_LAST_SLOT; ++n) {
        if (!slot_dir(dir, save_root, n) || !plain_directory(dir) || !marked(dir, &age) || age <= max_age_seconds) continue;
        remove_ours(dir);
        if (removed && GetFileAttributesW(dir) == INVALID_FILE_ATTRIBUTES) ++*removed;
    }
    return TRUE;
}
