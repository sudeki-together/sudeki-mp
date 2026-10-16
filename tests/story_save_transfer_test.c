/* #72 save transfer core: synthetic records only (no user saves, no sockets,
 * no native game). Proves the codec, exact-size and identity protections,
 * strict ordering, tamper detection and the verified staging writer. */
#include "network/story_save_transfer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static uint8_t *fish, *bunny;
static SudekiMpSaveTransferIdentity identity;

static void make_records(void) {
    fish = malloc(SUDEKIMP_SAVE_TRANSFER_FISH_BYTES); bunny = malloc(SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES);
    assert(fish && bunny);
    uint32_t x = 0x12345678u;
    for (unsigned i = 0; i < SUDEKIMP_SAVE_TRANSFER_FISH_BYTES; ++i) { x = x * 1664525u + 1013904223u; fish[i] = (uint8_t)(x >> 24); }
    for (unsigned i = 0; i < SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES; ++i) bunny[i] = (uint8_t)(i * 7u + 3u);
    memcpy(bunny + 0x200, "SAVESLOT0011\\", 14); /* stored folder: NUL-terminated, trailing bytes kept */
    assert(SudekiMpSaveTransferHash(fish, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES, identity.fish_sha256));
    assert(SudekiMpSaveTransferHash(bunny, SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES, identity.bunny_sha256));
}
static unsigned transfer(SudekiMpSaveTransferSender *s, SudekiMpSaveTransferReceiver *r, long tamper_at) {
    uint8_t body[SUDEKIMP_SAVE_TRANSFER_BODY_BYTES]; BOOL done = FALSE; unsigned frames = 0;
    SudekiMpSaveTransferRefusal why;
    SudekiMpSaveTransferStatus status = SUDEKIMP_SAVE_TRANSFER_PROGRESS;
    while (!done) {
        assert(SudekiMpSaveTransferSenderNext(s, body, &done));
        if (tamper_at >= 0 && frames == (unsigned)(tamper_at / SUDEKIMP_SAVE_TRANSFER_CHUNK_BYTES))
            body[16 + tamper_at % SUDEKIMP_SAVE_TRANSFER_CHUNK_BYTES] ^= 0x40u;
        status = SudekiMpSaveTransferReceiverChunk(r, body, &why);
        ++frames;
        if (!done) assert(status == SUDEKIMP_SAVE_TRANSFER_PROGRESS);
    }
    if (tamper_at >= 0) { assert(status == SUDEKIMP_SAVE_TRANSFER_REFUSED && why == SUDEKIMP_SAVE_TRANSFER_HASH_MISMATCH); }
    else assert(status == SUDEKIMP_SAVE_TRANSFER_COMPLETE);
    return frames;
}

int main(void) {
    SudekiMpSaveTransferRefusal why;
    uint8_t body[SUDEKIMP_SAVE_TRANSFER_BODY_BYTES];
    make_records();
    assert(SUDEKIMP_SAVE_TRANSFER_CHUNKS == 5856u); /* 2,623,488 bytes / 448 exactly */

    /* Sender protections. */
    SudekiMpSaveTransferSender sender; memset(&sender, 0, sizeof(sender));
    assert(!SudekiMpSaveTransferSenderBegin(&sender, 7, fish, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES - 1u, bunny,
        SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES, &identity, &why) && why == SUDEKIMP_SAVE_TRANSFER_BAD_SIZE);
    assert(!SudekiMpSaveTransferSenderBegin(&sender, 7, fish, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES, bunny,
        SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES + 1u, &identity, &why) && why == SUDEKIMP_SAVE_TRANSFER_BAD_SIZE);
    SudekiMpSaveTransferIdentity other = identity; other.bunny_sha256[5] ^= 1u;
    assert(!SudekiMpSaveTransferSenderBegin(&sender, 7, fish, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES, bunny,
        SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES, &other, &why) && why == SUDEKIMP_SAVE_TRANSFER_HASH_MISMATCH);
    assert(!SudekiMpSaveTransferSenderBegin(&sender, 0, fish, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES, bunny,
        SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES, &identity, &why));
    assert(SudekiMpSaveTransferSenderBegin(&sender, 7, fish, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES, bunny,
        SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES, &identity, &why));
    assert(SudekiMpSaveTransferSenderOffer(&sender, body));

    /* Receiver offer protections. */
    SudekiMpSaveTransferReceiver receiver; memset(&receiver, 0, sizeof(receiver));
    assert(!SudekiMpSaveTransferReceiverOffer(&receiver, body, &other, &why) && why == SUDEKIMP_SAVE_TRANSFER_WRONG_IDENTITY);
    uint8_t bad[SUDEKIMP_SAVE_TRANSFER_BODY_BYTES];
    memcpy(bad, body, sizeof(bad)); bad[8] ^= 1u;   /* fish size */
    assert(!SudekiMpSaveTransferReceiverOffer(&receiver, bad, &identity, &why) && why == SUDEKIMP_SAVE_TRANSFER_BAD_SIZE);
    memcpy(bad, body, sizeof(bad)); bad[16] ^= 1u;  /* chunk geometry */
    assert(!SudekiMpSaveTransferReceiverOffer(&receiver, bad, &identity, &why) && why == SUDEKIMP_SAVE_TRANSFER_BAD_SIZE);
    memcpy(bad, body, sizeof(bad)); bad[200] = 1u;  /* trailing bytes must be zero */
    assert(!SudekiMpSaveTransferReceiverOffer(&receiver, bad, &identity, &why) && why == SUDEKIMP_SAVE_TRANSFER_BAD_BODY);
    memcpy(bad, body, sizeof(bad)); bad[0] = 'X';
    assert(!SudekiMpSaveTransferReceiverOffer(&receiver, bad, &identity, &why) && why == SUDEKIMP_SAVE_TRANSFER_BAD_BODY);
    assert(SudekiMpSaveTransferReceiverOffer(&receiver, body, &identity, &why));

    /* Chunk protections on the first chunk. */
    BOOL done = FALSE;
    assert(SudekiMpSaveTransferSenderNext(&sender, body, &done) && !done);
    memcpy(bad, body, sizeof(bad)); bad[4] ^= 1u;
    assert(SudekiMpSaveTransferReceiverChunk(&receiver, bad, &why) == SUDEKIMP_SAVE_TRANSFER_REFUSED && why == SUDEKIMP_SAVE_TRANSFER_WRONG_TRANSFER);
    memcpy(bad, body, sizeof(bad)); bad[8] = 1u;    /* index 1 before 0 */
    assert(SudekiMpSaveTransferReceiverChunk(&receiver, bad, &why) == SUDEKIMP_SAVE_TRANSFER_REFUSED && why == SUDEKIMP_SAVE_TRANSFER_OUT_OF_ORDER);
    memcpy(bad, body, sizeof(bad)); bad[12] ^= 1u;  /* wrong length */
    assert(SudekiMpSaveTransferReceiverChunk(&receiver, bad, &why) == SUDEKIMP_SAVE_TRANSFER_REFUSED && why == SUDEKIMP_SAVE_TRANSFER_BAD_BODY);
    memcpy(bad, body, sizeof(bad)); bad[14] = 1u;   /* padding */
    assert(SudekiMpSaveTransferReceiverChunk(&receiver, bad, &why) == SUDEKIMP_SAVE_TRANSFER_REFUSED && why == SUDEKIMP_SAVE_TRANSFER_BAD_BODY);
    /* A full chunk fills the body exactly (16 + 448 = 464): nothing trails it. */
    assert(SudekiMpSaveTransferReceiverChunk(&receiver, body, &why) == SUDEKIMP_SAVE_TRANSFER_PROGRESS);
    assert(SudekiMpSaveTransferReceiverChunk(&receiver, body, &why) == SUDEKIMP_SAVE_TRANSFER_REFUSED && why == SUDEKIMP_SAVE_TRANSFER_OUT_OF_ORDER); /* duplicate */

    /* Complete the stream; verified records are byte-identical. */
    unsigned frames = 1u + transfer(&sender, &receiver, -1);
    assert(frames == SUDEKIMP_SAVE_TRANSFER_CHUNKS && SudekiMpSaveTransferReceiverPermille(&receiver) == 1000u);
    const uint8_t *got_fish, *got_bunny;
    assert(SudekiMpSaveTransferReceiverRecords(&receiver, &got_fish, &got_bunny));
    assert(!memcmp(got_fish, fish, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES) && !memcmp(got_bunny, bunny, SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES));
    assert(!SudekiMpSaveTransferSenderNext(&sender, body, &done));

    /* Staging: reserved root, content-named folder, verified, idempotent. */
    wchar_t temp[MAX_PATH], root[MAX_PATH], dir[MAX_PATH], path[MAX_PATH];
    assert(GetTempPathW(MAX_PATH, temp));
    _snwprintf(root, MAX_PATH, L"%lsSudekiMpSaveTransferTest-%lu", temp, (unsigned long)GetCurrentProcessId());
    CreateDirectoryW(root, NULL);
    assert(SudekiMpSaveTransferStage(root, got_fish, got_bunny, &identity, dir));
    assert(wcsstr(dir, root) == dir && wcslen(dir) == wcslen(root) + 17u);
    assert(SudekiMpSaveTransferStage(root, got_fish, got_bunny, &identity, dir)); /* same content reused */
    assert(!SudekiMpSaveTransferStage(root, got_fish, got_bunny, &other, dir)); /* hashes must match */
    _snwprintf(path, MAX_PATH, L"%ls\\fluffy.bunny", dir);
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    assert(file != INVALID_HANDLE_VALUE);
    DWORD written = 0; uint8_t junk = 0xee; assert(WriteFile(file, &junk, 1, &written, NULL)); CloseHandle(file);
    assert(!SudekiMpSaveTransferStage(root, got_fish, got_bunny, &identity, dir) && GetLastError() == ERROR_FILE_EXISTS);
    DeleteFileW(path);
    _snwprintf(path, MAX_PATH, L"%ls\\sudeki.fish", dir); DeleteFileW(path);
    RemoveDirectoryW(dir); RemoveDirectoryW(root);
    SudekiMpSaveTransferReceiverEnd(&receiver);
    SudekiMpSaveTransferSenderEnd(&sender);

    /* Import into the reserved native slots: an unmarked folder (a player's
     * own save, even in the reserved range) is never used or removed. */
    {
        wchar_t save_root[MAX_PATH], player_dir[MAX_PATH], player_file[MAX_PATH], marker[MAX_PATH];
        _snwprintf(save_root, MAX_PATH, L"%lsSudekiMpImportTest-%lu", temp, (unsigned long)GetCurrentProcessId());
        assert(CreateDirectoryW(save_root, NULL));
        _snwprintf(player_dir, MAX_PATH, L"%ls\\SAVESLOT9900", save_root); assert(CreateDirectoryW(player_dir, NULL));
        _snwprintf(player_file, MAX_PATH, L"%ls\\sudeki.fish", player_dir);
        HANDLE own = CreateFileW(player_file, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
        assert(own != INVALID_HANDLE_VALUE); WriteFile(own, "player", 6, &written, NULL); CloseHandle(own);
        unsigned slot = 0, again = 0, second = 0, removed = 9;
        assert(SudekiMpSaveTransferImport(save_root, fish, bunny, &identity, &slot) && slot == 9901u);
        assert(SudekiMpSaveTransferImport(save_root, fish, bunny, &identity, &again) && again == 9901u);
        /* The copy's stored folder names the reserved folder; everything else
         * is the host's bytes, and the pair is found by the published identity. */
        {   wchar_t staged[MAX_PATH]; uint8_t copy[SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES]; unsigned found = 0;
            _snwprintf(staged, MAX_PATH, L"%ls\\SAVESLOT9901\\fluffy.bunny", save_root);
            HANDLE f = CreateFileW(staged, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            DWORD got = 0; assert(f != INVALID_HANDLE_VALUE && ReadFile(f, copy, sizeof(copy), &got, NULL) && got == sizeof(copy)); CloseHandle(f);
            /* Only the four folder digits differ from the host's record. */
            assert(!memcmp(copy + 0x200, "SAVESLOT9901\\", 14) && !memcmp(copy, bunny, 0x208) &&
                !memcmp(copy + 0x20c, bunny + 0x20c, sizeof(copy) - 0x20c) && copy[0x20e] == bunny[0x20e]);
            assert(SudekiMpSaveTransferFindImported(save_root, &identity, &found) && found == 9901u);
            /* One changed byte outside the folder field: no longer the host's save. */
            f = CreateFileW(staged, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL); assert(f != INVALID_HANDLE_VALUE);
            uint8_t flipped = (uint8_t)(copy[0x300] ^ 1u); SetFilePointer(f, 0x300, NULL, FILE_BEGIN);
            assert(WriteFile(f, &flipped, 1, &got, NULL)); CloseHandle(f);
            assert(!SudekiMpSaveTransferFindImported(save_root, &identity, &found));
            f = CreateFileW(staged, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL); assert(f != INVALID_HANDLE_VALUE);
            SetFilePointer(f, 0x300, NULL, FILE_BEGIN); assert(WriteFile(f, &copy[0x300], 1, &got, NULL)); CloseHandle(f);
            assert(SudekiMpSaveTransferFindImported(save_root, &identity, &found) && found == 9901u);
            /* A record without a well-formed stored folder is refused. */
            uint8_t odd[SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES]; memcpy(odd, bunny, sizeof(odd)); odd[0x20c] = '/';
            SudekiMpSaveTransferIdentity odd_identity = identity;
            assert(SudekiMpSaveTransferHash(odd, sizeof(odd), odd_identity.bunny_sha256));
            assert(!SudekiMpSaveTransferImport(save_root, fish, odd, &odd_identity, &found));
        }
        uint8_t *fish2 = malloc(SUDEKIMP_SAVE_TRANSFER_FISH_BYTES); assert(fish2);
        memcpy(fish2, fish, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES); fish2[99] ^= 1u;
        SudekiMpSaveTransferIdentity identity2 = identity;
        assert(SudekiMpSaveTransferHash(fish2, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES, identity2.fish_sha256));
        assert(!SudekiMpSaveTransferImport(save_root, fish2, bunny, &identity, &second)); /* bytes must match */
        assert(SudekiMpSaveTransferImport(save_root, fish2, bunny, &identity2, &second) && second == 9902u);
        /* Expiry: fresh markers stay; an eight-day-old marker goes; the
         * player's unmarked folder and file stay. */
        assert(SudekiMpSaveTransferExpire(save_root, 7u * 86400u, &removed) && removed == 0u);
        _snwprintf(marker, MAX_PATH, L"%ls\\SAVESLOT9902\\sudekimp-received.txt", save_root);
        HANDLE m = CreateFileW(marker, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        assert(m != INVALID_HANDLE_VALUE);
        FILETIME now; GetSystemTimeAsFileTime(&now);
        uint64_t t = (((uint64_t)now.dwHighDateTime << 32) | now.dwLowDateTime) - 8ull * 86400ull * 10000000ull;
        FILETIME old_time = {(DWORD)t, (DWORD)(t >> 32)};
        assert(SetFileTime(m, NULL, NULL, &old_time)); CloseHandle(m);
        assert(SudekiMpSaveTransferExpire(save_root, 7u * 86400u, &removed) && removed == 1u);
        wchar_t check[MAX_PATH];
        _snwprintf(check, MAX_PATH, L"%ls\\SAVESLOT9902", save_root); assert(GetFileAttributesW(check) == INVALID_FILE_ATTRIBUTES);
        _snwprintf(check, MAX_PATH, L"%ls\\SAVESLOT9901\\sudeki.fish", save_root); assert(GetFileAttributesW(check) != INVALID_FILE_ATTRIBUTES);
        assert(GetFileAttributesW(player_file) != INVALID_FILE_ATTRIBUTES);
        /* Clean up the fixture: our folder through expiry, then the player's. */
        _snwprintf(marker, MAX_PATH, L"%ls\\SAVESLOT9901\\sudekimp-received.txt", save_root);
        m = CreateFileW(marker, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        assert(m != INVALID_HANDLE_VALUE && SetFileTime(m, NULL, NULL, &old_time)); CloseHandle(m);
        assert(SudekiMpSaveTransferExpire(save_root, 7u * 86400u, &removed) && removed == 1u);
        DeleteFileW(player_file); RemoveDirectoryW(player_dir); assert(RemoveDirectoryW(save_root));
        free(fish2);
    }

    /* Tampered stream is refused at completion and drops the bytes. */
    assert(SudekiMpSaveTransferSenderBegin(&sender, 9, fish, SUDEKIMP_SAVE_TRANSFER_FISH_BYTES, bunny,
        SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES, &identity, &why));
    assert(SudekiMpSaveTransferSenderOffer(&sender, body) && SudekiMpSaveTransferReceiverOffer(&receiver, body, &identity, &why));
    (void)transfer(&sender, &receiver, 1234567);
    assert(!SudekiMpSaveTransferReceiverRecords(&receiver, &got_fish, &got_bunny) && !receiver.stream);
    SudekiMpSaveTransferSenderEnd(&sender);
    free(fish); free(bunny);
    printf("story save transfer: exact sizes, published identity, ordered chunks, tamper refusal, verified staging, reserved-slot import and marker-only expiry passed\n");
    return 0;
}
