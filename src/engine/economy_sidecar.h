#ifndef SUDEKIMP_ECONOMY_SIDECAR_H
#define SUDEKIMP_ECONOMY_SIDECAR_H

#include <stddef.h>
#include <stdint.h>

#include "engine/economy_coordinator.h"

/* The runtime save adapter owns the filesystem operation. This codec supplies
 * the versioned, checksummed bytes it must atomically write beside the native
 * save, after the native save identity has been hashed. */
enum { SUDEKIMP_ECONOMY_SIDECAR_VERSION = 1u };

size_t SudekiMpEconomySidecarSize(void);
int SudekiMpEconomySidecarEncode(
    const SudekiMpEconomyCoordinatorSnapshot *snapshot,
    uint8_t *bytes,
    size_t capacity,
    size_t *written
);
int SudekiMpEconomySidecarDecode(
    const uint8_t *bytes,
    size_t size,
    const uint8_t expected_native_save_identity[32],
    SudekiMpEconomyCoordinatorSnapshot *snapshot
);
/* A future save-boundary adapter supplies the sidecar path only after the
 * native save itself has completed. Write uses a same-directory temporary and
 * an atomic replace; read accepts one exact codec payload only. */
int SudekiMpEconomySidecarWriteAtomic(
    const char *path,
    const SudekiMpEconomyCoordinatorSnapshot *snapshot
);
int SudekiMpEconomySidecarReadFile(
    const char *path,
    const uint8_t expected_native_save_identity[32],
    SudekiMpEconomyCoordinatorSnapshot *snapshot
);

#endif
