#ifndef SUDEKIMP_BLACKSMITH_READ_ADAPTER_H
#define SUDEKIMP_BLACKSMITH_READ_ADAPTER_H

#include "engine/blacksmith_read_model.h"

#include <windows.h>

typedef struct SudekiMpBlacksmithReadSeatRequest {
    uintptr_t actor;
    uint32_t character_id;
    uint32_t actor_generation;
    /* Set only after the caller has matched this exact tuple to the current
     * active-group/statehood lease. It is deliberately not inferred from a
     * party ordinal inside the Blacksmith adapter. */
    int active_group_proven;
} SudekiMpBlacksmithReadSeatRequest;

/* Read-only exact-image adapter. The caller must already have passed the
 * loader's supported-executable SHA/PE gate. It never retains a native
 * pointer in the returned model and never invokes a mutation or
 * UILayerBlackSmith method. */
BOOL SudekiMpBlacksmithReadAdapterInitialize(HMODULE game_module);
BOOL SudekiMpBlacksmithReadAdapterCapture(
    const SudekiMpBlacksmithReadSeatRequest *requests,
    uint32_t player_count,
    SudekiMpBlacksmithReadSnapshot *snapshot
);
void SudekiMpBlacksmithReadAdapterReset(void);

#endif
