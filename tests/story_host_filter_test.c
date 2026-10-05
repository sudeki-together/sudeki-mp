/* Synthetic lease/transition regression only. No retail image or game runs. */
#include "../src/hooks/lan_story_host_control.c"
#include <assert.h>
#include <stdio.h>

static BOOL roster_exact=TRUE,filter_exact=TRUE;
static unsigned none_calls,all_calls;
static uint8_t controller[0x250],quick[0x2a],quit[0x1c3],speed[0x2c];
BOOL SudekiMpSpiritInstanceFilterNoneEntryExact(HMODULE image) {
    return image==(HMODULE)base && filter_exact;
}
BOOL SudekiMpSpiritInstanceFilterAllEntryExact(HMODULE image) {
    return image==(HMODULE)base && filter_exact;
}
BOOL SudekiMpLanStoryObserverRosterStillExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {
    return roster_exact && w && r && r->controller==controller;
}
static void __attribute__((thiscall)) native_none(void *c) {
    assert(c==controller); ++none_calls;
    *(int *)(controller+0x80)=*(int *)(controller+0x84)=0;
}
static void __attribute__((thiscall)) native_all(void *c) {
    assert(c==controller); ++all_calls;
    *(int *)(controller+0x84)=1; /* native application may be asynchronous */
}
static void filters(int current,int pending) {
    *(int *)(controller+0x80)=current; *(int *)(controller+0x84)=pending;
}
static void jump(unsigned offset,Filter fn) {
    base[offset]=0xe9;
    int32_t displacement=(int32_t)((uintptr_t)fn-(uintptr_t)(base+offset+5));
    memcpy(base+offset+1,&displacement,4);
}
int main(void) {
    base=VirtualAlloc(NULL,0x409000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    assert(base);
    jump(FILTER_NONE,native_none); jump(FILTER_ALL,native_all);
    assert(FlushInstructionCache(GetCurrentProcess(),base,0x409000));
    *(void **)(base+0x408d68)=quit; *(void **)(base+0x3c2f84)=quick;
    *(void **)(base+0x408da0)=speed;
    SudekiMpControlUpdateDispatchWitness w={0};
    SudekiMpLanStoryNativeRoster r={.controller=controller,.world=(void *)1,
        .descriptor=(void *)2,.group=(void *)3};
    /* Repeated F8 combat toggles: priming restores All behind an owned None.
     * Closing retires with NO extra native call; staying open reacquires only
     * after the original pair and a clear native-menu boundary are observed. */
    filters(1,1);
    for(unsigned i=0;i<8;++i) {
        assert(input_filter(&w,&r,TRUE) && input_owner.owned);
        filters(1,1);
        unsigned calls=none_calls+all_calls;
        assert(input_filter(&w,&r,FALSE) && !input_owner.owned);
        assert(none_calls+all_calls==calls);
    }
    assert(input_filter(&w,&r,TRUE)); filters(1,1);
    unsigned calls=none_calls;
    assert(input_filter(&w,&r,TRUE) && input_owner.owned && none_calls==calls+1);
    assert(*(int *)(controller+0x80)==0 && *(int *)(controller+0x84)==0);
    /* A normal asynchronous release is still retained, not replayed. */
    assert(!input_filter(&w,&r,FALSE) && input_owner.releasing);
    calls=all_calls;
    assert(!input_filter(&w,&r,FALSE) && input_owner.owned && all_calls==calls);
    filters(1,1); assert(input_filter(&w,&r,FALSE) && !input_owner.owned);
    /* Pending-only restoration and a foreign mode cannot be repaired. */
    assert(input_filter(&w,&r,TRUE)); filters(0,1);
    assert(!input_filter(&w,&r,FALSE) && input_owner.owned && !input_owner.releasing);
    filters(2,2); assert(!input_filter(&w,&r,FALSE) && input_owner.owned);
    filters(1,1);
    roster_exact=FALSE; assert(!input_filter(&w,&r,FALSE) && input_owner.owned);
    roster_exact=TRUE; filter_exact=FALSE;
    assert(!input_filter(&w,&r,FALSE) && input_owner.owned); filter_exact=TRUE;
    r.group=(void *)4; assert(!input_filter(&w,&r,FALSE) && input_owner.owned);
    r.group=(void *)3;
    /* Native Q/pause ownership blocks reacquisition even after our lease
     * ended. Do not steal another native modal's restored input. */
    quick[0x29]=1; calls=none_calls;
    assert(!input_filter(&w,&r,TRUE) && !input_owner.owned && none_calls==calls);
    quick[0x29]=0; speed[0x28]=1;
    assert(!input_filter(&w,&r,TRUE) && !input_owner.owned && none_calls==calls);
    speed[0x28]=0; assert(input_filter(&w,&r,TRUE));
    /* Reopen while our own asynchronous release is pending. */
    assert(!input_filter(&w,&r,FALSE) && input_owner.releasing);
    assert(input_filter(&w,&r,TRUE) && input_owner.owned && !input_owner.releasing);
    filters(1,1); assert(input_filter(&w,&r,FALSE) && !input_owner.owned);
    VirtualFree(base,0,MEM_RELEASE); base=NULL;
    puts("story host filter lease regression tests passed (synthetic, no gameplay)");
    return 0;
}
