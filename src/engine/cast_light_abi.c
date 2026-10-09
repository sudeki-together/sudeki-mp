#include "engine/cast_light_abi.h"
#include "hooks/call_hook.h"
#include <math.h>
#include <string.h>

/* Native evidence: ctor 76cd0(EAX), init 1f210(ECX), Update(args*) 1f350,
 * deleting dtor 77110(ECX,flags). Init seeds a permanent baseline effect but
 * does NOT register a new node. SystemSingletons startup 77c53/77c66 separately
 * calls SetUpdatePeriod(2.0f), EAX=this/stack float, ret4. Repeat that ONCE.
 * Add/Remove/SetRate remain native: their
 * receiver and effect handles persist across yields in the owning GEL task.
 * Only the update's GPU publication is suppressed for private managers;
 * the existing world-draw adapter selects the viewer's observed RGB.
 * Never copy an active stack, reset it at cast end, or reschedule a tick. */
enum { CL_GLOBAL=0x408d7c, CL_VT=0x2ca030, CL_SIZE=0x6c,
    CL_CTOR=0x76cd0, CL_INIT=0x1f210, CL_DELETE=0x77110,
    CL_UPDATE=0x1f350, CL_PUBLISH_CALL=0x1f53a, CL_PUBLISH=0x1e2910,
    CL_REMOVE=0x1f5c0, CL_ARRAY_VT=0x2ca06c, CL_GROUP=0x404cac,
    CL_SCHEDULER=0x409e14, CL_DEVICE=0x3c31dc, CL_PERIOD=0x1061d0, CL_MAX=4 };
typedef void (__attribute__((regparm(1))) *LightCtor)(void *);
typedef void (__attribute__((thiscall)) *LightInit)(void *);
typedef void *(__attribute__((thiscall)) *LightDelete)(void *,unsigned int);
typedef void (__attribute__((thiscall)) *LightUpdate)(void *,void *);
typedef void (__attribute__((thiscall)) *LightRemove)(void *,int);
typedef struct LightEntry {
    uint32_t key;
    uint8_t *object;
    void *actor;
    uint64_t session;
    SudekiMpCastLightWitness retained;
    BOOL initialized, destroyed, restore_pending;
    int baseline_id;
    float baseline[3];
} LightEntry;
static uint8_t *light_image,*world_light;
static void *scheduler,*scheduler_vtable;
static LightEntry light_entries[CL_MAX];
static uint32_t selected_key;
static DWORD light_thread;
static unsigned int light_operation,light_update_depth;
static BOOL light_fault;
static LightEntry *volatile publishing __attribute__((used));
static LightCtor light_ctor;
static LightInit light_init;
static LightDelete light_delete;
static LightUpdate light_update_original;
static LightRemove light_remove;
static void (*light_period)(void);
static void *light_publish_original __attribute__((used));
static SudekiMpPointerHook light_update_hook;
static SudekiMpRelativeCallHook light_publish_hook;

static BOOL lm_memory(const void *p,size_t size,BOOL write) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t start=(uintptr_t)p;
    return p && size && VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) && start+size>=start &&
        start+size<=(uintptr_t)m.BaseAddress+m.RegionSize &&
        (!write || (m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)));
}
static BOOL lm_bytes(uint8_t *b,uint32_t rva,const void *bytes,size_t size) {
    return lm_memory(b+rva,size,FALSE) && !memcmp(b+rva,bytes,size);
}
static BOOL lm_address(uint8_t *b,uint32_t rva,uint32_t target) {
    return lm_memory(b+rva,4,FALSE) && *(void **)(b+rva)==b+target;
}
static BOOL lm_call(uint8_t *b,uint32_t rva,uint32_t target) {
    int32_t d;
    if(!lm_memory(b+rva,5,FALSE) || b[rva]!=0xe8) return FALSE;
    memcpy(&d,b+rva+1,4); return (int64_t)rva+5+d==target;
}
static BOOL light_image_exact(uint8_t *b) {
    return b && lm_bytes(b,CL_CTOR,"\xd9\xe8\x33\xc9\xd9\x58\x10",7) &&
        lm_address(b,0x76d09,CL_VT) && lm_address(b,0x76d1f,CL_GLOBAL) &&
        lm_address(b,0x76d26,CL_ARRAY_VT) &&
        lm_bytes(b,0x76d2a,"\x89\x48\x60\x89\x48\x64\x89\x48\x68\xc3",10) &&
        lm_bytes(b,CL_INIT,"\x51\x56\x8b\xf1\x80\x7e\x22\x00",8) &&
        lm_call(b,0x1f226,0x1342e0) && lm_call(b,0x1f24c,0x106050) &&
        lm_address(b,0x1f253,CL_GROUP) && lm_call(b,0x1f26b,0x1f280) &&
        lm_call(b,0x1f28d,0x162d60) && lm_call(b,0x1f2df,0x1f550) &&
        lm_bytes(b,CL_UPDATE,"\x55\x8b\xec\x83\xe4\xf8\x83\xec\x24\x8b\x45\x08\xd9\x40\x0c",15) &&
        lm_bytes(b,0x1f512,"\x8d\x74\x24\x24\xd9\x41\x3c\xbf\x07\x00\x00\x00",12) &&
        lm_call(b,CL_PUBLISH_CALL,CL_PUBLISH) && lm_bytes(b,0x1f545,"\xc2\x04\x00",3) &&
        lm_address(b,CL_VT,CL_DELETE) && lm_address(b,CL_VT+4,CL_UPDATE) &&
        lm_bytes(b,CL_DELETE,"\x56\x57\x8b\xf9\x8d\x47\x5c",7) &&
        lm_call(b,0x7711d,0x162d60) && lm_address(b,0x7712e,CL_GLOBAL) &&
        lm_call(b,0x77136,0x4d30) && lm_call(b,0x7714b,0x134330) &&
        lm_bytes(b,0x77164,"\xc2\x04\x00",3) &&
        lm_bytes(b,CL_REMOVE,"\x55\x8b\xec\x83\xe4\xf8\x8b\x45\x08\x83\xec\x0c",12) &&
        lm_call(b,0x1f5db,0x1f6f0) && lm_call(b,0x1f613,0x24844f) &&
        lm_call(b,0x1f628,0x38e80) && lm_bytes(b,0x1f655,"\xc2\x04\x00",3) &&
        lm_bytes(b,0x1f740,"\xd9\x44\x24\x04\xd9\x59\x30\xc2\x04\x00",10) &&
        lm_bytes(b,0x1f750,"\xa1",1) && lm_address(b,0x1f751,CL_GLOBAL) &&
        lm_bytes(b,0x1f755,"\xc3",1) &&
        lm_bytes(b,CL_PERIOD,"\x83\xec\x08\xd9\x44\x24\x0c\x56",8) &&
        lm_call(b,0x106266,0x134280) && lm_call(b,0x106272,0x134330) &&
        lm_call(b,0x106281,0x1342e0) && lm_bytes(b,0x10629c,"\xc2\x04\x00",3) &&
        lm_bytes(b,0x77c53,"\x8d\x86\xd4\x06\x00\x00",6) &&
        lm_call(b,0x77c66,CL_PERIOD) && lm_address(b,0x77c43,0x2c3bfc) &&
        lm_bytes(b,0x2c3bfc,"\x00\x00\x00\x40",4);
}
static BOOL lm_rgb(const float rgb[3]) {
    for(unsigned int i=0;i<3;++i) if(!isfinite(rgb[i]) || rgb[i]<0 || rgb[i]>1) return FALSE;
    return TRUE;
}
static LightEntry *lm_find(uint32_t key) {
    for(unsigned int i=0;i<CL_MAX;++i) if(key && light_entries[i].key==key) return &light_entries[i];
    return NULL;
}
static BOOL lm_object(uint8_t *p) {
    return lm_memory(p,CL_SIZE,TRUE) && *(void **)p==light_image+CL_VT;
}
static BOOL lm_stack(uint8_t *p) {
    uint32_t count,capacity;
    uint8_t **items;
    uint64_t handles=0;
    if(!lm_object(p) || *(void **)(p+0x5c)!=light_image+CL_ARRAY_VT ||
        !isfinite(*(float *)(p+0x30)) || *(float *)(p+0x30)<0 ||
        !lm_rgb((float *)(p+0x34)) || !lm_rgb((float *)(p+0x40)) || !lm_rgb((float *)(p+0x50))) return FALSE;
    count=*(uint32_t *)(p+0x60); capacity=*(uint32_t *)(p+0x64); items=*(uint8_t ***)(p+0x68);
    if(!count || count>64 || capacity<count || capacity>128 || !lm_memory(items,capacity*4,FALSE)) return FALSE;
    for(unsigned int i=0;i<count;++i) {
        uint32_t id;
        if(!lm_memory(items[i],16,FALSE) || (id=*(uint32_t *)items[i])>=64 ||
            (handles & ((uint64_t)1<<id)) || !lm_rgb((float *)(items[i]+4))) return FALSE;
        handles|=(uint64_t)1<<id;
    }
    return TRUE;
}
/* Everything lm_owner proves except the caller's retained-actor witness. */
static BOOL lm_owner_struct(LightEntry *e) {
    return e && e->initialized && !e->destroyed && !e->restore_pending &&
        e->retained && lm_stack(e->object) &&
        *(int16_t *)(e->object+0x20)>=0 && !e->object[0x22] &&
        !memcmp(e->object+0x50,e->baseline,12) &&
        ***(int ***)(e->object+0x68)==e->baseline_id &&
        !memcmp(**(uint8_t ***)(e->object+0x68)+4,e->baseline,12);
}
static BOOL (*light_unknown)(void);
static unsigned transient_skips;
void SudekiMpCastLightSetUnknownWitness(BOOL (*unknown)(void)) { light_unknown=unknown; }
/* The retained witness cannot answer (e.g. native world load pending) while
 * every structural proof still holds: identity is unknown, not mismatched. */
static BOOL lm_owner_unknown(LightEntry *e) {
    return light_unknown && !light_fault && lm_owner_struct(e) &&
        !e->retained(e->actor,e->session) && light_unknown();
}
BOOL SudekiMpCastLightTransient(void) {
    if(!light_image || light_fault || !light_unknown || !light_unknown()) return FALSE;
    for(unsigned int i=0;i<CL_MAX;++i) if(light_entries[i].key && lm_owner_struct(&light_entries[i]))
        return TRUE;
    return FALSE;
}
unsigned SudekiMpCastLightTransientSkips(void) { return transient_skips; }
static BOOL lm_owner(LightEntry *e) {
    return e && e->initialized && !e->destroyed && !e->restore_pending &&
        e->retained && e->retained(e->actor,e->session) && lm_stack(e->object) &&
        *(int16_t *)(e->object+0x20)>=0 && !e->object[0x22] &&
        !memcmp(e->object+0x50,e->baseline,12) &&
        ***(int ***)(e->object+0x68)==e->baseline_id &&
        !memcmp(**(uint8_t ***)(e->object+0x68)+4,e->baseline,12);
}
static BOOL lm_hooks(void) {
    int32_t displacement;
    if(!light_update_hook.installed || !light_publish_hook.installed ||
        !lm_memory(light_update_hook.slot,4,FALSE) ||
        *light_update_hook.slot!=light_update_hook.replacement_value ||
        !lm_memory(light_publish_hook.instruction,5,FALSE) ||
        light_publish_hook.instruction[0]!=0xe8) return FALSE;
    memcpy(&displacement,light_publish_hook.instruction+1,4);
    return displacement==light_publish_hook.replacement_displacement;
}
static BOOL lm_world(void) {
    return light_image && light_thread==GetCurrentThreadId() && lm_object(world_light) &&
        lm_memory(light_image+CL_SCHEDULER,4,FALSE) &&
        *(void **)(light_image+CL_SCHEDULER)==scheduler && lm_memory(scheduler,8,FALSE) &&
        *(void **)scheduler==scheduler_vtable;
}
static unsigned world_light_adoptions,foreign_light_skips,light_fault_site;
static unsigned light_fault_line;
unsigned SudekiMpCastLightFaultLine(void){return light_fault_line;}
unsigned SudekiMpCastLightFaultSite(void){return light_fault_site;}
unsigned SudekiMpCastLightForeignSkips(void) {return foreign_light_skips;}
unsigned SudekiMpCastLightWorldAdoptions(void) {return world_light_adoptions;}
/* An area switch (e.g. a split-area TEMP interior) legitimately installs its
 * own native world light. With no instance light selected and no operation
 * in progress, adopt the validated native object as the new baseline rather
 * than faulting; an instance light selected across a switch still fails. */
static void lm_adopt_world(void) {
    void *current;
    if(!light_image || light_fault || light_operation || selected_key ||
        light_thread!=GetCurrentThreadId() || !lm_memory(light_image+CL_GLOBAL,4,FALSE)) return;
    current=*(void **)(light_image+CL_GLOBAL);
    if(current && current!=world_light && lm_object(current)) {
        world_light=current; ++world_light_adoptions;
    }
}
static unsigned light_ready_failure;
unsigned SudekiMpCastLightReadyFailure(void) {return light_ready_failure;}
BOOL SudekiMpCastLightTransitionReady(uint32_t key) {
    LightEntry *previous=lm_find(selected_key),*next=lm_find(key);
    if(!light_image) return TRUE;
    lm_adopt_world();
    unsigned code=light_fault?1:light_operation?2:!lm_world()?3:!lm_hooks()?4:
        (selected_key && !lm_owner(previous))?5:(key && !lm_owner(next))?6:
        *(void **)(light_image+CL_GLOBAL)!=(previous ? previous->object:world_light)?7:0;
    if(code) {
        if(!light_ready_failure) light_ready_failure=code*10u+(selected_key?1u:0u);
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    return TRUE;
}
void SudekiMpCastLightTransitionCommit(uint32_t key) {
    LightEntry *e=lm_find(key);
    if(!light_image) return;
    *(void **)(light_image+CL_GLOBAL)=e ? e->object:world_light;
    selected_key=key;
}
static void __attribute__((naked,noinline)) light_publish_bridge(void) {
    /* Exact native call ABI: ESI=RGB, EDI=group, no stack arguments. */
    __asm__ volatile("pushfl\n\tcmpl $0,_publishing\n\tje 1f\n\tpopfl\n\tret\n\t"
        "1: popfl\n\tjmp *_light_publish_original\n\t");
}
static void __attribute__((thiscall)) light_update(void *object,void *args) {
    LightEntry *e=NULL;
    DWORD error=GetLastError();
    for(unsigned int i=0;i<CL_MAX;++i) if(light_entries[i].key && light_entries[i].object==object) e=&light_entries[i];
    /* A second native area's light (split-area TEMP interior or the retained
     * exterior). The light that is the current native global becomes the
     * world baseline; any other valid native light is left un-updated, as
     * vanilla suspension of an inactive area would. Instance lights and
     * malformed objects keep the fail-closed path below. */
    if(!e && object!=world_light && !light_update_depth && !light_operation && !light_fault &&
        !selected_key && light_thread==GetCurrentThreadId() && lm_object(object)) {
        if(lm_memory(light_image+CL_GLOBAL,4,FALSE) && *(void **)(light_image+CL_GLOBAL)==object) {
            world_light=object; ++world_light_adoptions;
        } else {
            ++foreign_light_skips; SetLastError(error); return;
        }
    }
    /* World load pending: leave an instance light un-updated (as vanilla
     * suspension would) instead of a sticky fault; resumes once proved (#42). */
    if(e && !light_update_depth && !light_operation && !selected_key && lm_world() &&
        lm_owner_unknown(e)) { ++transient_skips; SetLastError(error); return; }
    if(light_update_depth || light_operation || light_fault || !lm_world() ||
        !SudekiMpCastLightTransitionReady(selected_key) ||
        (e ? !lm_owner(e):object!=world_light) || !lm_memory(args,16,FALSE) ||
        !isfinite(*(float *)((uint8_t *)args+12)) || *(float *)((uint8_t *)args+12)<0) {
        if(!light_fault_site) light_fault_site=(light_update_depth?1u:0u)|(light_operation?2u:0u)|
            (!lm_world()?4u:0u)|(e?8u:0u)|(object==world_light?16u:0u)|(lm_object(object)?32u:0u)|
            (selected_key?64u:0u)|(*(void **)(light_image+CL_GLOBAL)==object?128u:0u)|256u;
        (light_fault_line?0:(light_fault_line=221)),light_fault=TRUE; return;
    }
    ++light_update_depth; publishing=e;
    SetLastError(error); light_update_original(object,args); error=GetLastError();
    publishing=NULL; --light_update_depth;
    if(e && !lm_owner(e)) (light_fault_line?0:(light_fault_line=226)),light_fault=TRUE;
    SetLastError(error);
}
__attribute__((naked,noinline)) static void __attribute__((regparm(2)))
schedule_light(void (*function)(void) __attribute__((unused)),void *object __attribute__((unused))) {
    __asm__ volatile("movl %eax,%ecx\n\tmovl %edx,%eax\n\tpushl $0x40000000\n\tcall *%ecx\n\tret\n\t");
}
BOOL SudekiMpInitializeCastLightAbi(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(light_image || !light_image_exact(b)) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    light_image=b; light_thread=GetCurrentThreadId();
    world_light=*(void **)(b+CL_GLOBAL); scheduler=*(void **)(b+CL_SCHEDULER);
    if(!lm_object(world_light) || !lm_memory(scheduler,8,FALSE)) goto failed;
    scheduler_vtable=*(void **)scheduler;
    light_ctor=(LightCtor)(b+CL_CTOR); light_init=(LightInit)(b+CL_INIT);
    light_delete=(LightDelete)(b+CL_DELETE); light_remove=(LightRemove)(b+CL_REMOVE);
    light_period=(void (*)(void))(b+CL_PERIOD);
    light_update_original=(LightUpdate)(b+CL_UPDATE); light_publish_original=b+CL_PUBLISH;
    if(SudekiMpInstallRelativeCallHook(&light_publish_hook,b+CL_PUBLISH_CALL,
        light_publish_original,light_publish_bridge) &&
        SudekiMpInstallPointerHook(&light_update_hook,(void **)(b+CL_VT+4),
            light_update_original,light_update)) return TRUE;
failed:
    { DWORD error=GetLastError(); SudekiMpResetCastLightAbi(); SetLastError(error); }
    return FALSE;
}
BOOL SudekiMpCreateCastLight(uint32_t key,void *actor,uint64_t session,SudekiMpCastLightWitness retained) {
    LightEntry *e=NULL;
    uint8_t *device;
    if(!key || !actor || !session || !retained || !retained(actor,session) || lm_find(key) ||
        !light_image || selected_key || !SudekiMpCastLightTransitionReady(0) || !lm_stack(world_light) ||
        *(uint32_t *)(world_light+0x60)!=1 || memcmp(world_light+0x34,world_light+0x50,12) ||
        memcmp(light_image+CL_GROUP,world_light+0x50,12)) return FALSE;
    device=*(void **)(light_image+CL_DEVICE);
    if(!lm_memory(device,4,FALSE) || !lm_memory(*(void **)device,0x17c,FALSE)) return FALSE;
    for(unsigned int i=0;i<CL_MAX;++i) if(!light_entries[i].key) { e=&light_entries[i]; break; }
    if(!e || !(e->object=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,CL_SIZE))) return FALSE;
    e->key=key; e->actor=actor; e->session=session; e->retained=retained;
    ++light_operation;
    light_ctor(e->object); /* publishes its own singleton */
    if(*(void **)(light_image+CL_GLOBAL)!=e->object || !lm_object(e->object)) goto retained;
    light_init(e->object); /* baseline insertion and native rate; not registration */
    e->initialized=TRUE;
    e->restore_pending=TRUE;
    if(*(void **)(light_image+CL_GLOBAL)!=e->object || !lm_stack(e->object)) goto retained;
    memcpy(e->baseline,e->object+0x50,12);
    e->baseline_id=***(int ***)(e->object+0x68);
    if(*(int16_t *)(e->object+0x20)!=-1 || e->object[0x22]) goto retained;
    schedule_light(light_period,e->object);
    if(*(void **)(light_image+CL_GLOBAL)!=e->object ||
        *(int16_t *)(e->object+0x20)<0) goto retained;
    *(void **)(light_image+CL_GLOBAL)=world_light; e->restore_pending=FALSE;
    --light_operation;
    return lm_owner(e);
retained:
    --light_operation; (light_fault_line?0:(light_fault_line=281)),light_fault=TRUE; SetLastError(ERROR_INVALID_DATA); return FALSE;
}
BOOL SudekiMpReadCastLight(uint32_t key,float current[3],float baseline[3]) {
    LightEntry *e=lm_find(key);
    if(!current || !baseline || !lm_world() || !lm_hooks() || light_fault || !lm_owner(e)) return FALSE;
    memcpy(current,e->object+0x34,12); memcpy(baseline,e->baseline,12); return TRUE;
}
BOOL SudekiMpCastLightDrained(uint32_t key) {
    LightEntry *e=lm_find(key);
    if(!e) return TRUE; /* This instance did not opt in. */
    return lm_world() && lm_hooks() && !light_fault && lm_owner(e) && *(uint32_t *)(e->object+0x60)==1 &&
        !memcmp(e->object+0x34,e->baseline,12) && !memcmp(e->object+0x40,e->baseline,12);
}
BOOL SudekiMpDestroyCastLight(uint32_t key) {
    LightEntry *e=lm_find(key);
    if(!e) return TRUE;
    if(!lm_world() || selected_key || light_operation || light_update_depth) return FALSE;
    if(e->destroyed) {
        if(!e->restore_pending || *(void **)(light_image+CL_GLOBAL)!=NULL) return FALSE;
        *(void **)(light_image+CL_GLOBAL)=world_light; e->restore_pending=FALSE;
    } else {
        if(!SudekiMpCastLightTransitionReady(0) || !SudekiMpCastLightDrained(key)) return FALSE;
        ++light_operation;
        *(void **)(light_image+CL_GLOBAL)=e->object;
        /* PtrArray dtor frees storage, NOT the LightSetting pointees. Remove
         * the sole verified baseline using its real native handle first. */
        light_remove(e->object,e->baseline_id);
        if(*(uint32_t *)(e->object+0x60)!=0) {
            --light_operation; (light_fault_line?0:(light_fault_line=309)),light_fault=TRUE; return FALSE;
        }
        light_delete(e->object,0); e->destroyed=TRUE; e->restore_pending=TRUE;
        --light_operation;
        if(*(void **)(light_image+CL_GLOBAL)!=NULL) return FALSE;
        *(void **)(light_image+CL_GLOBAL)=world_light; e->restore_pending=FALSE;
    }
    HeapFree(GetProcessHeap(),0,e->object); memset(e,0,sizeof(*e)); return TRUE;
}
BOOL SudekiMpResetCastLightAbi(void) {
    BOOL restored=TRUE;
    if(!light_image) return TRUE;
    if(light_thread!=GetCurrentThreadId() || selected_key || light_operation || light_update_depth) return FALSE;
    for(unsigned int i=0;i<CL_MAX;++i) if(light_entries[i].key) return FALSE;
    if(!SudekiMpRestorePointerHook(&light_update_hook)) restored=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&light_publish_hook)) restored=FALSE;
    if(!restored) return FALSE; /* Retain ALL callbacks until every seam restored. */
    light_image=world_light=NULL; scheduler=scheduler_vtable=NULL; light_thread=0;
    light_ctor=NULL; light_init=NULL; light_delete=NULL; light_remove=NULL; light_period=NULL;
    light_update_original=NULL; light_publish_original=NULL; publishing=NULL; light_fault=FALSE;
    return TRUE;
}
