#ifndef SUDEKIMP_STORY_RESIDENCY_CALL_H
#define SUDEKIMP_STORY_RESIDENCY_CALL_H

/* Synchronous policy only; native adapters must supply fresh owner, thread,
 * pause, registry and task proofs. No timestamp or permission survives return.
 * The operation reports whether it actually entered residency-changing native
 * code, even if that native operation failed. Observation-only calls cannot
 * authorize adoption. Re-entry is refused through every callback. */
typedef struct SudekiMpStoryResidencyCallState {
    int active, failed;
} SudekiMpStoryResidencyCallState;
typedef int (*SudekiMpStoryResidencyStep)(void *context);

static inline int SudekiMpStoryResidencyCallRun(
    SudekiMpStoryResidencyCallState *state, void *context,
    SudekiMpStoryResidencyStep before, SudekiMpStoryResidencyStep operation,
    SudekiMpStoryResidencyStep adopt, SudekiMpStoryResidencyStep after) {
    if(!state || state->active || state->failed || !before || !operation || !adopt || !after)
        return 0;
    state->active=1;
    if(!before(context)) { state->active=0; return 0; }
    int entered=operation(context);
    int adopted=!entered || adopt(context);
    /* Always observe the postcondition, including failed/no-op native calls. */
    int exact=after(context);
    state->failed=!adopted || !exact;
    state->active=0;
    return !state->failed;
}

#endif
