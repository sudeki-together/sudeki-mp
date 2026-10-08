/* Synthetic event/lease fixture; no windows, installed hooks, input injection
 * or Sudeki image. Controller memory below is owned fixture data. */
#include <windows.h>
static BOOL fixture_is_window(HWND);
static DWORD fixture_window_thread(HWND,DWORD *);
static HWND fixture_focus(void);
static HHOOK fixture_install_hook(int,HOOKPROC,HINSTANCE,DWORD);
static BOOL fixture_unhook(HHOOK);
static LRESULT fixture_next_hook(HHOOK,int,WPARAM,LPARAM);
#define IsWindow fixture_is_window
#define GetWindowThreadProcessId fixture_window_thread
#define GetFocus fixture_focus
#define SetWindowsHookExW fixture_install_hook
#define UnhookWindowsHookEx fixture_unhook
#define CallNextHookEx fixture_next_hook
#include "../src/hooks/lan_story_input.c"
#undef IsWindow
#undef GetWindowThreadProcessId
#undef GetFocus
#undef SetWindowsHookExW
#undef UnhookWindowsHookEx
#undef CallNextHookEx
#include <assert.h>
#include <stdio.h>
static HWND fixture_window=(HWND)(uintptr_t)0x1234u,fixture_focused;
static DWORD fixture_window_pid,fixture_window_tid;
static BOOL fixture_window_valid,fixture_hook_ok,fixture_unhook_ok;
static unsigned fixture_hook_calls,fixture_unhook_calls,fixture_creation_calls,fixture_drift;
static BOOL fixture_creation_ok;
static IDirect3DDevice9 fixture_device,fixture_other_device;
static IDirect3DDevice9Vtbl fixture_device_vtable;
static HRESULT WINAPI fixture_creation(IDirect3DDevice9 *device,D3DDEVICE_CREATION_PARAMETERS *out) {
    assert(device==&fixture_device || device==&fixture_other_device);
    ++fixture_creation_calls; out->hFocusWindow=fixture_window;
    if(fixture_drift==1u) *(void **)(base+D3D_DEVICE_GLOBAL)=&fixture_other_device;
    if(fixture_drift==2u) fixture_device.lpVtbl=NULL;
    return fixture_creation_ok?S_OK:E_FAIL;
}
static BOOL fixture_is_window(HWND w) { return w==fixture_window && fixture_window_valid; }
static DWORD fixture_window_thread(HWND w,DWORD *pid) {
    if(w!=fixture_window) { *pid=0; return 0; }
    *pid=fixture_window_pid; return fixture_window_tid;
}
static HWND fixture_focus(void) { return fixture_focused; }
static HHOOK fixture_install_hook(int type,HOOKPROC callback,HINSTANCE module,DWORD thread) {
    assert(type==WH_CALLWNDPROC && callback==focus_messages && !module && thread==GetCurrentThreadId());
    ++fixture_hook_calls;
    if(fixture_drift==3u) *(void **)(base+D3D_DEVICE_GLOBAL)=&fixture_other_device;
    if(fixture_drift==4u) ++fixture_window_tid;
    return fixture_hook_ok?(HHOOK)(uintptr_t)0x4567u:NULL;
}
static BOOL fixture_unhook(HHOOK hook) {
    assert(hook==(HHOOK)(uintptr_t)0x4567u); ++fixture_unhook_calls;
    return fixture_unhook_ok;
}
static LRESULT fixture_next_hook(HHOOK hook,int code,WPARAM sent,LPARAM raw) {
    (void)hook; (void)code; (void)sent; (void)raw; return 0;
}
BOOL SudekiMpLogResearchEnabled(void) { return FALSE; }
void SudekiMpLogFormat(const char *format,...) { (void)format; }
static unsigned restore_calls;
BOOL SudekiMpRestorePointerHook(SudekiMpPointerHook *hook) {
    ++restore_calls;
    if(!hook->installed) return TRUE;
    if(*hook->slot!=hook->replacement_value) { SetLastError(ERROR_BUSY); return FALSE; }
    *hook->slot=hook->original_value; memset(hook,0,sizeof(*hook)); return TRUE;
}
typedef struct AvatarLease {
    void *entity;
    uint32_t generation,transaction;
    BOOL valid;
    unsigned calls;
} AvatarLease;
static BOOL avatar_exact(void *entity,uint32_t generation,uint32_t transaction,void *context) {
    AvatarLease *lease=context; ++lease->calls;
    return lease->valid && entity==lease->entity && generation==lease->generation &&
        transaction==lease->transaction;
}
static unsigned native_calls;
static void __attribute__((thiscall)) native_input_fixture(void *listener,void *event) {
    (void)listener; (void)event; ++native_calls;
}
static void host_cache_fence(void) {
    uint8_t controller[0x24cu]={0},other_controller[0x24cu]={0};
    uint8_t world[0x39cu]={0},group[0xd0u]={0},scene[0x174u]={0},title[0x1844u]={0};
    void *leader=(void *)0x1000;
    HostTitleIdentity before;
    base=VirtualAlloc(NULL,0x40a000u,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE); assert(base);
    assert(!SudekiMpLanStoryInputInstall((HMODULE)base,0) && GetLastError()==ERROR_INVALID_PARAMETER);
    *(void **)(base+CONTROLLER_GLOBAL)=controller; *(void **)controller=base+CONTROLLER_VT;
    *(void **)(controller+INPUT_SUBOBJECT)=base+INPUT_VT;
    *(float *)(controller+0x1b8u)=2.0f; *(int *)(controller+0x80u)=*(int *)(controller+0x84u)=2;
    *(void **)(base+0x408d10u)=world; *(void **)world=base+0x2c4c3cu;
    *(void **)(base+0x408d94u)=group; *(void **)group=base+0x2c6d30u;
    *(void **)(base+0x408d1cu)=scene; *(void **)(scene+0x170u)=title;
    *(void **)title=base+0x2cb1fcu; *(unsigned *)(title+0x44u)=5;
    assert(host_title_identity(base,&before) && host_title_unchanged(base,&before));
    /* Every native input cache category must already be neutral. Refusal
     * never repairs any fixture/native byte, including FilterNone's skipped Q. */
    const unsigned dirty[]={0x50,0x68,0x8c,0xbc,0xf4,0xfc,0x110,0x114,0x19f,
        0x1a0,0x1a4,0x1a8,0x1ac,0x1b0,0x1b4,0x1c9};
    for(unsigned i=0;i<sizeof(dirty)/sizeof(*dirty);++i) {
        controller[dirty[i]]=1;
        assert(!native_cache_neutral(controller) && !host_title_unchanged(base,&before));
        assert(controller[dirty[i]]==1); controller[dirty[i]]=0;
    }
    for(unsigned i=0;i<17u;++i) {
        *(uint32_t *)(controller+0x8cu+i*8u)=3; /* pending release is not empty */
        assert(!native_cache_neutral(controller)); *(uint32_t *)(controller+0x8cu+i*8u)=0;
    }
    *(float *)(controller+0x1b8u)=1; assert(!native_cache_neutral(controller));
    *(float *)(controller+0x1b8u)=NAN; assert(!native_cache_neutral(controller));
    *(float *)(controller+0x1b8u)=2;
    *(int *)(controller+0x84u)=0; assert(!native_cache_neutral(controller)); *(int *)(controller+0x84u)=2;
    *(int *)(controller+0x80u)=*(int *)(controller+0x84u)=4;
    assert(!native_cache_neutral(controller)); *(int *)(controller+0x80u)=*(int *)(controller+0x84u)=2;
    *(void **)(controller+ACTOR_TARGET)=leader; assert(!host_title_unchanged(base,&before));
    *(void **)(controller+ACTOR_TARGET)=NULL;
    const unsigned world_pointers[]={0xc,0x10,0x14,0x394};
    for(unsigned i=0;i<4u;++i) {
        *(void **)(world+world_pointers[i])=leader; assert(!host_title_unchanged(base,&before));
        *(void **)(world+world_pointers[i])=NULL;
    }
    world[0x399]=1; assert(!host_title_unchanged(base,&before)); world[0x399]=0;
    world[0x39a]=1; assert(!host_title_unchanged(base,&before)); world[0x39a]=0;
    for(unsigned i=0;i<4u;++i) {
        *(void **)(group+0x90u+i*0xcu)=leader; assert(!host_title_unchanged(base,&before));
        *(void **)(group+0x90u+i*0xcu)=NULL;
    }
    *(unsigned *)(group+0xcc)=1; assert(!host_title_unchanged(base,&before)); *(unsigned *)(group+0xcc)=0;
    *(unsigned *)(title+0x44)=13; assert(!host_title_unchanged(base,&before)); *(unsigned *)(title+0x44)=5;
    memcpy(other_controller,controller,sizeof(controller));
    *(void **)(base+CONTROLLER_GLOBAL)=other_controller;
    assert(!host_title_unchanged(base,&before)); *(void **)(base+CONTROLLER_GLOBAL)=controller;
    assert(host_title_unchanged(base,&before));
    /* Post-publication drift rolls the hook back without erasing queued data.
     * A foreign pointer owner retains the complete fence for explicit retry. */
    input_hook=(SudekiMpPointerHook){(void **)(base+INPUT_VT),base+INPUT_HANDLER,story_input,TRUE};
    *input_hook.slot=story_input; original_input=native_input_fixture;
    installed=retained=host_route=TRUE; host_controller=controller;
    native_thread=GetCurrentThreadId(); fault=callbacks=0;
    assert(host_install_published(&before));
    controller[0xbc]=1;
    assert(!host_install_published(&before) && restore_calls==1 && !retained && !host_route);
    assert(controller[0xbc]==1); controller[0xbc]=0;
    input_hook=(SudekiMpPointerHook){(void **)(base+INPUT_VT),base+INPUT_HANDLER,story_input,TRUE};
    *input_hook.slot=(void *)native_input_fixture;
    installed=retained=host_route=TRUE; host_controller=controller; native_thread=GetCurrentThreadId();
    assert(!host_install_published(&before) && restore_calls==2 && retained && host_route && input_hook.installed);
    *input_hook.slot=story_input;
    assert(SudekiMpLanStoryInputUninstall() && restore_calls==3 && !retained && !host_route);
    /* Simulate a successfully installed host fence, then a native loaded
     * leader target. No title/world predicate is borrowed for AI readiness. */
    input_hook=(SudekiMpPointerHook){(void **)(base+INPUT_VT),base+INPUT_HANDLER,story_input,TRUE};
    *input_hook.slot=story_input; original_input=native_input_fixture;
    installed=retained=host_route=TRUE; host_controller=controller;
    native_thread=GetCurrentThreadId(); fault=callbacks=0;
    *(void **)(controller+ACTOR_TARGET)=leader;
    assert(SudekiMpLanStoryInputHostFenceExact(controller,leader));
    assert(!SudekiMpLanStoryInputHostFenceExact(controller,(void *)0x2000));
    controller[0xbc]=1; assert(!SudekiMpLanStoryInputHostFenceExact(controller,leader)); controller[0xbc]=0;
    unsigned calls=native_calls; NativeEvent event={.action=0x2c,.value=1};
    story_input(controller+INPUT_SUBOBJECT,&event); assert(native_calls==calls && !controller[0x8c]);
    story_input(other_controller+INPUT_SUBOBJECT,&event);
    assert(native_calls==calls && fault && !SudekiMpLanStoryInputHostFenceExact(controller,leader));
    clear_sample(); host_route=FALSE; host_controller=NULL;
    installed=retained=native_thread=fault=0; memset(&input_hook,0,sizeof(input_hook));
    original_input=NULL; assert(VirtualFree(base,0,MEM_RELEASE)); base=NULL;
}
static void avatar_binding(void) {
    uint32_t controller_words[0x24cu/4u]={0}; uint8_t *controller=(uint8_t *)controller_words;
    void *hero=(void *)0x1000,*avatar=(void *)0x2000;
    AvatarLease lease={avatar,7,11,TRUE,0};
    base=VirtualAlloc(NULL,0x40a000u,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE); assert(base);
    *(void **)(base+CONTROLLER_GLOBAL)=controller;
    *(void **)controller=base+CONTROLLER_VT;
    *(void **)(controller+INPUT_SUBOBJECT)=base+INPUT_VT;
    *(void **)(controller+ACTOR_TARGET)=hero;
    input_hook=(SudekiMpPointerHook){(void **)(base+INPUT_VT),base+INPUT_HANDLER,story_input,TRUE};
    *input_hook.slot=story_input;
    installed=retained=1; native_thread=GetCurrentThreadId(); fault=callbacks=0;
    original_input=native_input_fixture;
    assert(SudekiMpLanStoryInputControllerExact(controller));
    assert(SudekiMpLanStoryInputExact(controller,hero));
    assert(!SudekiMpLanStoryInputExact(controller,avatar));
    /* Exercise the binding state separately from actual window focus, which
     * this synthetic fixture deliberately does not create or inject. */
    assert(arm_sample(controller,avatar,11,7,avatar_exact,&lease));
    assert(sample_target_exact(controller,avatar,11) && lease.calls==1);
    assert(*(void **)(controller+ACTOR_TARGET)==hero); /* no hero borrowed */
    assert(!sample_target_exact(controller,hero,11));
    assert(!sample_target_exact(controller,avatar,12));
    assert(lease.calls==1);
    sample_x=1; quick_pending=TRUE; melee_count=1;
    assert(arm_sample(controller,avatar,11,7,avatar_exact,&lease));
    assert(sample_x==1 && quick_pending && melee_count==1); /* frame rearm */
    lease.generation=8;
    float x=99,z=99;
    assert(!SudekiMpLanStoryInputSample(controller,avatar,11,&x,&z));
    assert(!x && !z && !sample_transaction && !sample_avatar_exact &&
        !sample_x && !quick_pending && !melee_count);
    lease.generation=7;
    assert(!sample_target_exact(controller,avatar,11)); /* cannot silently resume */
    assert(arm_sample(controller,avatar,11,7,avatar_exact,&lease));
    sample_x=1; quick_pending=TRUE; melee_count=1; quick_down=TRUE; melee_down=1;
    lease.generation=8; assert(arm_sample(controller,avatar,11,8,avatar_exact,&lease));
    assert(!sample_x && !quick_pending && !melee_count && quick_down && melee_down==1);
    assert(sample_target_exact(controller,avatar,11));
    AvatarLease replacement=lease;
    sample_z=1; assert(arm_sample(controller,avatar,11,8,avatar_exact,&replacement));
    assert(!sample_z); /* same address/generation with a new owner context */
    NativeEvent attack={.action=0x2c,.value=1};
    unsigned calls=replacement.calls;
    story_input(controller+INPUT_SUBOBJECT,&attack);
    assert(replacement.calls==calls+1 && !native_calls); /* controller hero differs */
    replacement.valid=FALSE; sample_x=1; melee_count=1;
    story_input(controller+INPUT_SUBOBJECT,&attack);
    assert(!native_calls && !sample_transaction && !sample_x && !melee_count);
    story_input(controller+INPUT_SUBOBJECT,&attack); assert(!native_calls);
    /* Unrelated native listener owners retain the original behavior. */
    story_input(controller+INPUT_SUBOBJECT+4u,&attack); assert(native_calls==1);
    assert(arm_sample(controller,avatar,11,8,avatar_exact,&lease));
    assert(arm_sample(controller,hero,12,0,NULL,NULL));
    assert(!sample_avatar_exact && !sample_avatar_generation &&
        sample_target_exact(controller,hero,12) && !sample_target_exact(controller,avatar,12));
    assert(!SudekiMpLanStoryInputArmAvatar(controller,avatar,0,12,avatar_exact,&lease));
    assert(!sample_transaction);
    clear_sample(); installed=retained=native_thread=0; memset(&input_hook,0,sizeof(input_hook));
    original_input=NULL; assert(VirtualFree(base,0,MEM_RELEASE)); base=NULL;
}
static void seed_window(void) {
    fixture_window_valid=fixture_hook_ok=fixture_unhook_ok=fixture_creation_ok=TRUE;
    fixture_window_pid=GetCurrentProcessId(); fixture_window_tid=GetCurrentThreadId();
    fixture_focused=NULL; fixture_drift=0;
    fixture_hook_calls=fixture_unhook_calls=fixture_creation_calls=0;
    memset(&fixture_device_vtable,0,sizeof(fixture_device_vtable));
    fixture_device_vtable.GetCreationParameters=fixture_creation;
    fixture_device.lpVtbl=fixture_other_device.lpVtbl=&fixture_device_vtable;
    *(void **)(base+D3D_DEVICE_GLOBAL)=&fixture_device;
    input_window=NULL; focus_hook=NULL; focused=FALSE;
    memset(&input_window_owner,0,sizeof(input_window_owner));
    native_thread=GetCurrentThreadId(); clear_sample();
}
static void window_enrollment(void) {
    uint32_t controller_words[0x24cu/4u]={0}; uint8_t *controller=(uint8_t *)controller_words;
    void *hero=(void *)0x1000,*avatar=(void *)0x2000;
    AvatarLease lease={avatar,7,11,TRUE,0};
    base=VirtualAlloc(NULL,0x40a000u,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE); assert(base);
    *(void **)(base+CONTROLLER_GLOBAL)=controller; *(void **)controller=base+CONTROLLER_VT;
    *(void **)(controller+INPUT_SUBOBJECT)=base+INPUT_VT; *(void **)(controller+ACTOR_TARGET)=hero;
    input_hook=(SudekiMpPointerHook){(void **)(base+INPUT_VT),base+INPUT_HANDLER,story_input,TRUE};
    *input_hook.slot=story_input; installed=retained=1; fault=callbacks=0;
    seed_window();
    assert(SudekiMpLanStoryInputArmAvatar(controller,avatar,7,11,avatar_exact,&lease));
    assert(input_window==fixture_window && focus_hook && !focused && fixture_hook_calls==1);
    assert(fixture_creation_calls>=2 && window_exact());
    sample_x=sample_z=1; float x=5,z=6;
    assert(SudekiMpLanStoryInputSample(controller,avatar,11,&x,&z) && x==0 && z==0);
    assert(!sample_x && !sample_z); /* background enrollment does not admit movement */
    assert(ensure_focus_hook() && fixture_hook_calls==1); /* no duplicate hook */
    CWPSTRUCT message={.hwnd=fixture_window,.message=WM_SETFOCUS};
    fixture_focused=fixture_window; focus_messages(0,0,(LPARAM)&message); assert(focused);
    sample_x=1; assert(SudekiMpLanStoryInputSample(controller,avatar,11,&x,&z) && x==1 && z==0);
    message.message=WM_KILLFOCUS; fixture_focused=NULL; quick_pending=TRUE; melee_count=1;
    focus_messages(0,0,(LPARAM)&message);
    assert(!focused && !sample_x && !quick_pending && !melee_count);
    assert(SudekiMpLanStoryInputSample(controller,avatar,11,&x,&z) && !x && !z);
    *(void **)(base+D3D_DEVICE_GLOBAL)=&fixture_other_device;
    assert(!ensure_focus_hook() && focus_hook && !sample_transaction && !focused);
    assert(SudekiMpLanStoryInputUninstall() && !focus_hook && !input_window);
    for(unsigned failure=0;failure<9u;++failure) {
        seed_window();
        switch(failure) {
        case 0: fixture_creation_ok=FALSE; break;
        case 1: fixture_window_valid=FALSE; break;
        case 2: ++fixture_window_pid; break;
        case 3: ++fixture_window_tid; break;
        case 4: fixture_drift=1; break;
        case 5: fixture_drift=2; break;
        case 6: fixture_device_vtable.GetCreationParameters=(DeviceCreationParameters)(void *)controller; break;
        case 7: *(void **)(base+D3D_DEVICE_GLOBAL)=NULL; break;
        case 8: fixture_device.lpVtbl=NULL; break;
        }
        assert(!ensure_focus_hook() && !focus_hook && !input_window && !fixture_hook_calls);
    }
    seed_window(); fixture_hook_ok=FALSE;
    assert(!ensure_focus_hook() && !focus_hook && !input_window && fixture_hook_calls==1);
    for(unsigned drift=3u;drift<=4u;++drift) {
        seed_window(); fixture_drift=drift;
        assert(!ensure_focus_hook() && !focus_hook && !input_window && !focused);
        assert(fixture_hook_calls==1 && fixture_unhook_calls==1);
    }
    seed_window(); fixture_drift=3; fixture_unhook_ok=FALSE;
    assert(!ensure_focus_hook() && focus_hook && input_window && !focused);
    assert(input_window_owner.device==&fixture_device && !window_exact());
    assert(!ensure_focus_hook() && fixture_hook_calls==1); /* cannot overwrite retained failed cleanup */
    fixture_unhook_ok=TRUE;
    input_hook=(SudekiMpPointerHook){(void **)(base+INPUT_VT),base+INPUT_HANDLER,story_input,TRUE};
    *input_hook.slot=story_input; installed=retained=1;
    assert(SudekiMpLanStoryInputUninstall() && !focus_hook && !input_window);
    assert(VirtualFree(base,0,MEM_RELEASE)); base=NULL;
}
int main(void) {
    NativeEvent attack={.action=0x2c,.value=1};
    sample_melee(&attack,TRUE,TRUE,10); assert(take_melee(10)==1 && !take_melee(10));
    sample_melee(&attack,TRUE,TRUE,11); assert(!take_melee(11)); /* repeat, not another press */
    clear_sample(); sample_melee(&attack,TRUE,TRUE,12); assert(!take_melee(12));
    attack.value=0; sample_melee(&attack,FALSE,TRUE,13);
    attack.value=1; sample_melee(&attack,FALSE,TRUE,14);
    sample_melee(&attack,TRUE,TRUE,15); assert(!take_melee(15)); /* focus/lease regain */
    attack.value=0; sample_melee(&attack,TRUE,TRUE,16);
    for(unsigned i=0;i<6;++i) {
        attack.action=0x2c+i%3; attack.value=0; sample_melee(&attack,TRUE,TRUE,20+i);
        attack.value=1; sample_melee(&attack,TRUE,TRUE,20+i);
    }
    assert(melee_count==4); /* bounded, ordered, overflow doesn't invent input */
    assert(take_melee(30)==1 && take_melee(30)==2 && take_melee(30)==3 && take_melee(30)==1);
    assert(!take_melee(30));
    attack.action=0x2c; attack.value=0; sample_melee(&attack,TRUE,TRUE,40);
    attack.value=1; sample_melee(&attack,TRUE,TRUE,40); assert(!take_melee(291));
    attack.value=0; sample_melee(&attack,TRUE,TRUE,UINT32_MAX-10u);
    attack.value=1; sample_melee(&attack,TRUE,TRUE,UINT32_MAX-10u);
    assert(take_melee(4)==1); /* unsigned clock wrap */
    attack.value=0; sample_melee(&attack,TRUE,TRUE,50);
    attack.value=1; sample_melee(&attack,TRUE,TRUE,50); clear_sample(); assert(!take_melee(50));
    NativeEvent block={.action=0x31,.value=1};
    sample_melee(&block,TRUE,FALSE,51); assert(!take_melee(51));
    sample_melee(&block,TRUE,TRUE,52); assert(!take_melee(52)); /* reopening gate is not a press */
    block.value=0; sample_melee(&block,TRUE,TRUE,53);
    block.value=1; sample_melee(&block,TRUE,TRUE,54); assert(take_melee(54)==4);
    NativeEvent e={.action=0x19,.value=1};
    sample_quick(&e,TRUE); assert(quick_pending && quick_down);
    quick_pending=FALSE; sample_quick(&e,TRUE); assert(!quick_pending);
    clear_sample(); sample_quick(&e,TRUE); assert(!quick_pending); /* new lease, held key */
    e.value=0; sample_quick(&e,FALSE); assert(!quick_down);
    e.value=1; sample_quick(&e,FALSE); assert(!quick_pending && quick_down);
    sample_quick(&e,TRUE); assert(!quick_pending); /* regain focus cannot open Q */
    e.value=0; sample_quick(&e,TRUE); e.value=1; sample_quick(&e,TRUE); assert(quick_pending);
    e.action=0x28; e.value=0; sample_quick(&e,TRUE); assert(quick_down); /* other input is not Q release */
    clear_sample(); assert(!quick_pending && quick_down);
    native_thread=GetCurrentThreadId(); sample_controller=(void *)1; sample_actor=(void *)2; sample_transaction=3;
    sample_x=sample_z=look_x=look_y=1; SudekiMpLanStoryInputMuteMovement();
    assert(!sample_x && !sample_z && !look_x && !look_y);
    assert(sample_controller==(void *)1 && sample_actor==(void *)2 && sample_transaction==3);
    attack.value=0; sample_melee(&attack,TRUE,TRUE,60);
    attack.value=1; sample_melee(&attack,TRUE,TRUE,60); SudekiMpLanStoryInputMuteMovement();
    assert(!take_melee(60)); sample_melee(&attack,TRUE,TRUE,61); assert(!take_melee(61));
    avatar_binding();
    host_cache_fence();
    window_enrollment();
    puts("story input edges, avatar leases, native host fence and exact D3D window enrollment/background neutrality passed (synthetic)"); return 0;
}
