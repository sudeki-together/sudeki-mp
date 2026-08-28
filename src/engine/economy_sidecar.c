#include "engine/economy_sidecar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

typedef struct SudekiMpEconomySidecarHeader {
    uint8_t magic[8];
    uint32_t version;
    uint32_t payload_size;
    uint8_t native_save_identity[32];
    uint32_t checksum;
} SudekiMpEconomySidecarHeader;

static const uint8_t economy_sidecar_magic[8] = {
    'S', 'M', 'P', 'E', 'C', 'O', 'N', '1'
};

static uint32_t checksum_bytes(const uint8_t *bytes, size_t size) {
    uint32_t checksum = 2166136261u;
    size_t index;

    for (index = 0u; index < size; ++index) {
        checksum ^= bytes[index];
        checksum *= 16777619u;
    }
    return checksum;
}

size_t SudekiMpEconomySidecarSize(void) {
    return sizeof(SudekiMpEconomySidecarHeader) +
        sizeof(SudekiMpEconomyCoordinatorSnapshot);
}

int SudekiMpEconomySidecarEncode(
    const SudekiMpEconomyCoordinatorSnapshot *snapshot,
    uint8_t *bytes,
    size_t capacity,
    size_t *written
) {
    SudekiMpEconomySidecarHeader header;
    size_t required = SudekiMpEconomySidecarSize();

    if (snapshot == NULL || bytes == NULL || written == NULL ||
        snapshot->schema_version != SUDEKIMP_ECONOMY_COORDINATOR_SCHEMA_VERSION ||
        snapshot->generation == 0u || capacity < required) {
        return 0;
    }
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, economy_sidecar_magic, sizeof(header.magic));
    header.version = SUDEKIMP_ECONOMY_SIDECAR_VERSION;
    header.payload_size = (uint32_t)sizeof(*snapshot);
    memcpy(header.native_save_identity, snapshot->native_save_identity, 32u);
    memcpy(bytes, &header, sizeof(header));
    memcpy(bytes + sizeof(header), snapshot, sizeof(*snapshot));
    header.checksum = checksum_bytes(bytes + sizeof(header), sizeof(*snapshot));
    memcpy(bytes, &header, sizeof(header));
    *written = required;
    return 1;
}

int SudekiMpEconomySidecarDecode(
    const uint8_t *bytes,
    size_t size,
    const uint8_t expected_native_save_identity[32],
    SudekiMpEconomyCoordinatorSnapshot *snapshot
) {
    SudekiMpEconomySidecarHeader header;
    size_t required = SudekiMpEconomySidecarSize();

    if (bytes == NULL || expected_native_save_identity == NULL ||
        snapshot == NULL || size != required) {
        return 0;
    }
    memcpy(&header, bytes, sizeof(header));
    if (memcmp(header.magic, economy_sidecar_magic, sizeof(header.magic)) != 0 ||
        header.version != SUDEKIMP_ECONOMY_SIDECAR_VERSION ||
        header.payload_size != sizeof(*snapshot) ||
        memcmp(header.native_save_identity, expected_native_save_identity, 32u) != 0 ||
        header.checksum != checksum_bytes(bytes + sizeof(header),
            sizeof(*snapshot))) {
        return 0;
    }
    memcpy(snapshot, bytes + sizeof(header), sizeof(*snapshot));
    return snapshot->schema_version == SUDEKIMP_ECONOMY_COORDINATOR_SCHEMA_VERSION &&
        snapshot->generation != 0u &&
        memcmp(snapshot->native_save_identity, expected_native_save_identity,
            32u) == 0;
}

int SudekiMpEconomySidecarWriteAtomic(
    const char *path,
    const SudekiMpEconomyCoordinatorSnapshot *snapshot
) {
    size_t path_length;
    size_t written;
    size_t required = SudekiMpEconomySidecarSize();
    char *temporary_path;
    uint8_t *bytes;
    FILE *file;
    int success = 0;
    int closed = 0;

    if (path == NULL || snapshot == NULL || path[0] == '\0') {
        return 0;
    }
    path_length = strlen(path);
    if (path_length > (size_t)-1 - 5u) {
        return 0;
    }
    temporary_path = (char *)malloc(path_length + 5u);
    bytes = (uint8_t *)malloc(required);
    if (temporary_path == NULL || bytes == NULL) {
        free(temporary_path);
        free(bytes);
        return 0;
    }
    memcpy(temporary_path, path, path_length);
    memcpy(temporary_path + path_length, ".tmp", 5u);
    if (!SudekiMpEconomySidecarEncode(snapshot, bytes, required, &written) ||
        written != required) {
        free(temporary_path);
        free(bytes);
        return 0;
    }
    file = fopen(temporary_path, "wb");
    if (file != NULL) {
        int written_ok = fwrite(bytes, 1u, written, file) == written &&
            fflush(file) == 0;
        int close_ok = fclose(file) == 0;
        closed = 1;
        if (written_ok && close_ok) {
            success = MoveFileExA(temporary_path, path,
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
        }
    }
    if (file != NULL && !closed) {
        (void)fclose(file);
    }
    if (!success) {
        (void)DeleteFileA(temporary_path);
    }
    free(temporary_path);
    free(bytes);
    return success;
}

int SudekiMpEconomySidecarReadFile(
    const char *path,
    const uint8_t expected_native_save_identity[32],
    SudekiMpEconomyCoordinatorSnapshot *snapshot
) {
    uint8_t *bytes;
    FILE *file;
    size_t required = SudekiMpEconomySidecarSize();
    int success = 0;

    if (path == NULL || expected_native_save_identity == NULL ||
        snapshot == NULL || path[0] == '\0') {
        return 0;
    }
    bytes = (uint8_t *)malloc(required);
    if (bytes == NULL) {
        return 0;
    }
    file = fopen(path, "rb");
    if (file != NULL && fread(bytes, 1u, required, file) == required &&
        fgetc(file) == EOF) {
        success = SudekiMpEconomySidecarDecode(bytes, required,
            expected_native_save_identity, snapshot);
    }
    if (file != NULL) {
        (void)fclose(file);
    }
    free(bytes);
    return success;
}
