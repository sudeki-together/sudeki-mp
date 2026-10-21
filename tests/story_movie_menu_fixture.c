/* Real MenuNative frame/pause checks, with synthetic native owners only. */
#include "hooks/lan_party_menu_native.h"
BOOL __attribute__((externally_visible)) SudekiMpLanPartyMenuNativePauseExact(BOOL *paused);
BOOL __attribute__((externally_visible)) SudekiMpLanPartyMenuNativeOwnsPause(void);
BOOL __attribute__((externally_visible)) SudekiMpLanPartyMenuNativeObserveOwnedPause(BOOL *paused);
#include "../src/hooks/lan_party_menu_native.c"
#include <assert.h>

void __attribute__((externally_visible)) StoryMovieTestMenuCreate(void) {
    game_base=VirtualAlloc(NULL,SUDEKIMP_EXPECTED_IMAGE_SIZE,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(game_base);
    memcpy(game_base+NATIVE_PAUSE,(uint8_t[]){0x55,0x8b,0xec,0x83,0xe4,0xf8,
        0x83,0xec,0x0c,0x53,0x56,0x57,0x8b,0x3d},14);
    *(void **)(game_base+NATIVE_PAUSE+14)=game_base+TASK_REGISTRY;
    game_base[0xfd75d]=0xa1;*(void **)(game_base+0xfd75e)=game_base+SPEED_GLOBAL;
    game_base[0xfd769]=0xe8;*(int32_t *)(game_base+0xfd76a)=PAUSE_REF-(0xfd769+5);
    memcpy(game_base+PAUSE_REF,(uint8_t[]){0x0f,0xb6,0xc9,0xf7,0xd9,0x1b,0xc9,
        0x81,0xe1,0x02,0x00,0xff,0xff,0x81,0xc1,0xff,0xff,0x00,0x00,
        0x66,0x01,0x48,0x2a,0x0f,0x95,0xc1,0x88,0x48,0x28},29);
    memcpy(game_base+0xfd7a9,"\x5f\x5e\x5b\x8b\xe5\x5d\xc3",7);
    pause_lease=(PauseLease){.speed=game_base+0x1000,.registry=game_base+0x2000,
        .scheduler=game_base+0x3000,.world=game_base+0x4000,.expected_count=1,.phase=PAUSE_OWNED};
    *(void **)(game_base+SPEED_GLOBAL)=pause_lease.speed;
    *(void **)(game_base+TASK_REGISTRY)=pause_lease.registry;
    *(void **)(game_base+TASK_SCHEDULER)=pause_lease.scheduler;
    *(void **)(game_base+WORLD_TASK)=pause_lease.world;
    pause_lease.speed[0x28]=1;*(uint16_t *)(pause_lease.speed+0x2a)=1;
    uint8_t *screen=game_base+0x5000;
    *(void **)(game_base+QUIT_GLOBAL)=screen;*(void **)screen=game_base+QUIT_VTABLE;
    open_hook.instruction=game_base+0x6000;open_hook.installed=TRUE;
    render_hook.instruction=game_base+0x7000;render_hook.installed=TRUE;
    open_hook.instruction[0]=render_hook.instruction[0]=0xe8;
    *(int32_t *)(open_hook.instruction+1)=(uint8_t *)open_entry-(open_hook.instruction+5);
    *(int32_t *)(render_hook.instruction+1)=(uint8_t *)render_entry-(render_hook.instruction+5);
    ready=TRUE;native_thread=GetCurrentThreadId();
    assert(native_pause_exact());
    assert(hook_exact(&open_hook,open_entry) && hook_exact(&render_hook,render_entry));
    PauseLease observed;uint16_t count;
    assert(observe_pause(&observed,&count));
    assert(lease_identity(&observed) && count==pause_lease.expected_count);
}
void __attribute__((externally_visible)) StoryMovieTestMenuDispatch(void (*callback)(void)) {
    assert(!frame_active && callback);menu_frame=callback;handle_frame();
    assert(!frame_active && !callbacks);menu_frame=NULL;
}
void __attribute__((externally_visible)) StoryMovieTestMenuDestroy(void) {
    assert(!frame_active && !callbacks);assert(VirtualFree(game_base,0,MEM_RELEASE));
    game_base=NULL;ready=FALSE;memset(&pause_lease,0,sizeof(pause_lease));
}
