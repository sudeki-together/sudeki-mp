#ifndef SUDEKIMP_LAN_STORY_RESIDENCY_H
#define SUDEKIMP_LAN_STORY_RESIDENCY_H

#include <windows.h>
#include <stdint.h>

typedef enum SudekiMpLanStoryResidencyState {
    SUDEKIMP_STORY_RESIDENCY_INVALID = 0,
    SUDEKIMP_STORY_RESIDENCY_READY,
    SUDEKIMP_STORY_RESIDENCY_NEEDS_LOAD,
    SUDEKIMP_STORY_RESIDENCY_WAITING,
    SUDEKIMP_STORY_RESIDENCY_LOADED
} SudekiMpLanStoryResidencyState;

typedef struct SudekiMpLanStoryResidencyReport {
    SudekiMpLanStoryResidencyState state;
    uint32_t dependency;
    uint32_t package_flags;
    uint8_t renderer_references;
    uint8_t bank_references;
    const char *reason;
} SudekiMpLanStoryResidencyReport;

typedef BOOL (*SudekiMpLanStoryResidencyExact)(void *context);

/* Exact-image validation only; no native mutation, hook, or thread binding. */
BOOL SudekiMpLanStoryResidencyInitialize(HMODULE image);

/* Read-only eligibility. The caller proves the current entity/renderer/bank
   relationship and selected host frame separately. No retained authorization. */
SudekiMpLanStoryResidencyState SudekiMpLanStoryResidencyInspect(
    void *renderer, void *expected_bank, unsigned selector,
    SudekiMpLanStoryResidencyReport *report);

/* One synchronous native dependency acquisition. Requires a fresh paused-world
   ownership callback before and after the call. No selector or animation setter
   is invoked. LOADED means the native renderer owns the new reference; its native
   destructor releases it. The caller must defer presentation until a new full
   preflight passes loaded-curve validation with a still-fresh host frame. WAITING never pumps asynchronous jobs. */
SudekiMpLanStoryResidencyState SudekiMpLanStoryResidencyAcquire(
    void *renderer, void *expected_bank, unsigned selector,
    SudekiMpLanStoryResidencyExact exact, void *context,
    SudekiMpLanStoryResidencyReport *report);

#endif
