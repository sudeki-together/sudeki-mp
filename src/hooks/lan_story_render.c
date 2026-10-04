#include "hooks/lan_story_render.h"
#include "engine/build_identity.h"
#include "hooks/call_hook.h"
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Saved-story render publication requires the supported x86 ABI"
#endif

enum {
    PRIMARY_CALL=0x28d45bu, PRIMARY_PREPARE=0x1d48c0u,
    SCENE_MANAGER=0x408d58u, WORLD_RENDER_GLOBAL=0x408d1cu
};
static uint8_t *base;
static SudekiMpRelativeCallHook primary_hook;
static void *original_prepare __attribute__((used));
static SudekiMpLanStoryRenderDispatch dispatcher;
static void *dispatch_context;
static volatile LONG callbacks,stopping;
static DWORD native_thread;
static BOOL installed,active;
static uint8_t *current_scene,*current_world;
static float current_delta;

static BOOL retain(DWORD error) {
    HMODULE self;
    (void)GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCWSTR)(uintptr_t)&SudekiMpLanStoryRenderUninstall,&self);
    SetLastError(error); return FALSE;
}
static BOOL readable(const void *pointer,size_t size) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t p=(uintptr_t)pointer;
    if(!pointer || !size || p>UINTPTR_MAX-size ||
        VirtualQuery(pointer,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD protection=m.Protect&0xffu;
    if(protection!=PAGE_READONLY && protection!=PAGE_READWRITE &&
        protection!=PAGE_WRITECOPY && protection!=PAGE_EXECUTE_READ &&
        protection!=PAGE_EXECUTE_READWRITE && protection!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return p+size<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL call_targets(const uint8_t *call,const void *target) {
    int32_t displacement;
    if(!readable(call,5u) || call[0]!=0xe8u) return FALSE;
    memcpy(&displacement,call+1u,4u);
    return call+5u+displacement==target;
}
static void primary_entry(void);
static BOOL hook_exact(void) {
    return primary_hook.installed &&
        call_targets(primary_hook.instruction,primary_entry);
}
static BOOL primary_owner(uint8_t *scene,uint8_t *world) {
    if(!base || !readable(base+SCENE_MANAGER,4u) ||
        !readable(base+WORLD_RENDER_GLOBAL,4u)) return FALSE;
    uint8_t *owner=*(uint8_t **)(base+SCENE_MANAGER);
    /* EDI is the exact render-world global used by 68D3F0, distinct from the
     * story registry's world global. Neither pointer is retained across frames. */
    return world==*(void **)(base+WORLD_RENDER_GLOBAL) &&
        readable(world,0x70u) && *(uint32_t *)(world+0x6cu)==0u &&
        readable(owner,0x44u) && *(void **)(owner+0x40u)==scene &&
        readable(scene,0x8cu) && scene[0x88u]!=0u;
}
BOOL SudekiMpLanStoryRenderWitness(void *unused) {
    (void)unused;
    return installed && active && native_thread==GetCurrentThreadId() &&
        InterlockedCompareExchange(&callbacks,0,0)==1 &&
        !InterlockedCompareExchange(&stopping,0,0) && hook_exact() &&
        current_delta==0.0f && primary_owner(current_scene,current_world);
}
__attribute__((noinline,used,force_align_arg_pointer))
static void handle_primary(uint8_t *scene,uint8_t *world,float delta) {
    DWORD entry_error=GetLastError();
    LONG depth=InterlockedIncrement(&callbacks);
    DWORD thread=GetCurrentThreadId();
    if(installed && depth==1 && !InterlockedCompareExchange(&stopping,0,0) &&
        (!native_thread || native_thread==thread) && hook_exact() &&
        delta==0.0f && primary_owner(scene,world)) {
        if(!native_thread) native_thread=thread;
        current_scene=scene; current_world=world; current_delta=delta;
        active=TRUE;
        if(dispatcher) dispatcher(dispatch_context);
        active=FALSE; current_scene=current_world=NULL; current_delta=0.0f;
    }
    InterlockedDecrement(&callbacks);
    SetLastError(entry_error);
}
__attribute__((naked,noinline,used))
static void primary_entry(void) {
    /* The native CALL carries EAX=scene, EDI=world and one stack float. Save
     * all integer registers/flags, pass copies to C, then enter the immutable
     * original with the untouched return address/argument. Native ret4 owns
     * the original stack cleanup; no second preparation or mod return exists. */
    __asm__ volatile("pushfl\n\tpushal\n\tpushl 40(%esp)\n\t"
        "pushl %edi\n\tpushl %eax\n\tcall _handle_primary\n\t"
        "addl $12,%esp\n\tpopal\n\tpopfl\n\tjmp *_original_prepare\n\t");
}
BOOL SudekiMpLanStoryRenderInstall(HMODULE image,
    SudekiMpLanStoryRenderDispatch dispatch,void *context) {
    static const uint8_t entry[]={0x56,0x8b,0xf0,0x80,0xbe,0x88,0,0,0,0,0x57,0x74,0x64};
    static const uint8_t argument[]={0xd9,0x44,0x24,0x20,0x8b,0x35};
    static const uint8_t call_prefix[]={0x8b,0x46,0x40,0x51,0xd9,0x1c,0x24,0xb3,0x01};
    static const uint8_t epilogue[]={0x5f,0x5e,0xc2,0x04,0x00};
    uint8_t *b=(uint8_t *)image;
    if(installed || primary_hook.installed || !image || !dispatch || active ||
        InterlockedCompareExchange(&callbacks,0,0) ||
        !SudekiMpCheckLoadedExecutable(image) ||
        memcmp(b+PRIMARY_PREPARE,entry,sizeof(entry)) ||
        memcmp(b+0x1d4931u,epilogue,sizeof(epilogue)) ||
        !call_targets(b+0x1d48f5u,b+0x1d51f0u) ||
        !call_targets(b+0x1d4904u,b+0x1d5300u) ||
        memcmp(b+0x28d448u,argument,sizeof(argument)) ||
        *(void **)(b+0x28d44eu)!=b+SCENE_MANAGER ||
        memcmp(b+PRIMARY_CALL-sizeof(call_prefix),call_prefix,sizeof(call_prefix)) ||
        !call_targets(b+PRIMARY_CALL,b+PRIMARY_PREPARE)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    base=b; dispatcher=dispatch; dispatch_context=context;
    original_prepare=b+PRIMARY_PREPARE;
    native_thread=0; active=FALSE; current_scene=current_world=NULL; current_delta=0.0f;
    InterlockedExchange(&stopping,0);
    if(!SudekiMpInstallRelativeCallHook(&primary_hook,b+PRIMARY_CALL,
        b+PRIMARY_PREPARE,primary_entry)) return FALSE;
    installed=TRUE; SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryRenderUninstall(void) {
    if(!installed && !primary_hook.installed) return TRUE;
    InterlockedExchange(&stopping,1);
    if(active || InterlockedCompareExchange(&callbacks,0,0) ||
        (native_thread && native_thread!=GetCurrentThreadId())) return retain(ERROR_BUSY);
    if(!SudekiMpRestoreRelativeCallHook(&primary_hook) ||
        InterlockedCompareExchange(&callbacks,0,0)) return retain(ERROR_BUSY);
    installed=FALSE; dispatcher=NULL; dispatch_context=NULL;
    current_scene=current_world=NULL; current_delta=0.0f; base=NULL; native_thread=0;
    /* The runtime guarantees native-thread quiescence. Keep the original
     * target immutable for an already-entered bridge's native tail jump. */
    SetLastError(ERROR_SUCCESS); return TRUE;
}
