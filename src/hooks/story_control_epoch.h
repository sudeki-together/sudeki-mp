#ifndef SUDEKIMP_STORY_CONTROL_EPOCH_H
#define SUDEKIMP_STORY_CONTROL_EPOCH_H
#include <stdint.h>
static inline int SudekiMpStoryControlEpochMayRebind(uint32_t bound,uint32_t observed,
    int thread_exact,int boundary_exact,int leases_empty) {
    return bound && observed>bound && thread_exact && boundary_exact && leases_empty;
}
#endif
