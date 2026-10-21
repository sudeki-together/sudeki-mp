/* Uses the real world-owner registry from StoryWorldCapacityTest, no hooks. */
#include <windows.h>
static DWORD fixture_tick=1000;
#define GetTickCount() fixture_tick
#include "../src/hooks/lan_story_ambient.c"
#include <assert.h>
static void ambient_setup(void) {
    base=VirtualAlloc(NULL,0x400000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(base);
    frame_valid=FALSE;owned_tick=0;substituted=excluded=0;
    refresh_frame(GetTickCount());
    active=TRUE;active_tick=GetTickCount();float dt=.016f;memcpy(&frame_dt_bits,&dt,4);
}
void __attribute__((externally_visible)) test_ambient_capacity(void *late) {
    ambient_setup();assert(owned_count>128 && owns_renderer((uintptr_t)late));
    for(unsigned i=1;i<owned_count;++i)assert((uintptr_t)owned[i-1]<=(uintptr_t)owned[i]);
    uint32_t slot=0;observe((uintptr_t)late,&slot);assert(slot==0 && excluded==1);
    uintptr_t missing[]={(uintptr_t)owned[0]-1u,(uintptr_t)owned[owned_count-1u]+1u,(uintptr_t)owned[0]+1u};
    assert(missing[2]<(uintptr_t)owned[1]);
    for(unsigned i=0;i<3;++i) {
        unsigned prior_substituted=substituted,prior_excluded=excluded;
        assert(!owns_renderer(missing[i]));slot=0;observe(missing[i],&slot);
        assert(slot==frame_dt_bits && substituted==prior_substituted+1u && excluded==prior_excluded);
    }
    VirtualFree(base,0,MEM_RELEASE);base=NULL;
}
void __attribute__((externally_visible)) test_ambient_rebind(unsigned phase,void *a,void *b) {
    if(!phase)ambient_setup();
    DWORD tick=owned_tick;uint16_t stamp=frame_seen;
    uint32_t slot_a=0,slot_b=0;
    observe((uintptr_t)b,&slot_b);observe((uintptr_t)a,&slot_a);
    assert(owned_tick==tick && frame_seen==stamp && GetTickCount()==1000);
    if(!phase)assert(slot_a==0 && slot_b==frame_dt_bits);
    else {
        assert(slot_a==frame_dt_bits && slot_b==0);
        VirtualFree(base,0,MEM_RELEASE);base=NULL;
    }
}
