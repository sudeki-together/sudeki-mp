#include "engine/sha256.h"

#include <wincrypt.h>

BOOL SudekiMpSha256File(const wchar_t *path, char output_hex[65]) {
    static const char hex[] = "0123456789abcdef";
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    HANDLE file = INVALID_HANDLE_VALUE;
    BYTE buffer[64 * 1024];
    BYTE digest[32];
    DWORD digest_size = sizeof(digest);
    DWORD bytes_read;
    DWORD index;
    BOOL success = FALSE;

    if (path == NULL || output_hex == NULL) {
        return FALSE;
    }
    output_hex[0] = '\0';

    if (!CryptAcquireContextW(
            &provider,
            NULL,
            NULL,
            PROV_RSA_AES,
            CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        goto cleanup;
    }
    if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        goto cleanup;
    }

    file = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        NULL
    );
    if (file == INVALID_HANDLE_VALUE) {
        goto cleanup;
    }

    do {
        if (!ReadFile(file, buffer, sizeof(buffer), &bytes_read, NULL)) {
            goto cleanup;
        }
        if (bytes_read != 0 && !CryptHashData(hash, buffer, bytes_read, 0)) {
            goto cleanup;
        }
    } while (bytes_read != 0);

    if (!CryptGetHashParam(hash, HP_HASHVAL, digest, &digest_size, 0) ||
        digest_size != sizeof(digest)) {
        goto cleanup;
    }

    for (index = 0; index < digest_size; ++index) {
        output_hex[index * 2] = hex[digest[index] >> 4];
        output_hex[index * 2 + 1] = hex[digest[index] & 0x0f];
    }
    output_hex[64] = '\0';
    success = TRUE;

cleanup:
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
    if (hash != 0) {
        CryptDestroyHash(hash);
    }
    if (provider != 0) {
        CryptReleaseContext(provider, 0);
    }
    return success;
}
