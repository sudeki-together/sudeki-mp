#ifndef SUDEKIMP_STORY_SPAWN_SCHEDULE_GUARD_H
#define SUDEKIMP_STORY_SPAWN_SCHEDULE_GUARD_H
#include <windows.h>
#include <stdint.h>
/* The caller separately proves the active journaled native job and registry
 * membership. Its initialization may enable only its own schedule bit; the
 * actor identity and every owned pause reference must remain exact. A proved
 * terminal spawn attachment may consume one native initialization reference,
 * while retaining at least the client's own world reference. */
static inline BOOL SudekiMpStorySpawnScheduleFinished(uint8_t *entity,const uint8_t *allowed,
    const void *vtable,const void *update_vtable,uint32_t native_identifier,
    int expected_references,unsigned old_schedule,int native_pause_delta,BOOL (*readable)(const void *,size_t)) {
    return entity==allowed && (native_pause_delta==0 || native_pause_delta==-1) &&
        expected_references+native_pause_delta>0 && expected_references<=UINT8_MAX &&
        !old_schedule && readable(entity,0x145u) && *(void **)entity==vtable &&
        *(void **)(entity+8u)==update_vtable &&
        *(uint32_t *)(entity+0x34u)==native_identifier &&
        entity[0x2bu]==expected_references+native_pause_delta && (entity[0x144u]&4u);
}
#endif
