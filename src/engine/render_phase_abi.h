#ifndef SUDEKIMP_RENDER_PHASE_ABI_H
#define SUDEKIMP_RENDER_PHASE_ABI_H

void SudekiMpCallRenderPhase(
    void *renderer,
    void *world_context,
    void *function
);

void SudekiMpCallRenderPhaseWithFloat(
    void *renderer,
    void *world_context,
    unsigned int value_bits,
    void *function
);

#endif
