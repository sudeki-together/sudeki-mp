#include "engine/spirit_instance_abi.h"
#include "hooks/call_hook.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Spirit instances require the verified x86 native ABI"
#endif

enum { MANAGER_SIZE=0xc4, CAMERA_SIZE=0x1f0, SOUL_SIZE=0x130,
    MANAGER_GLOBAL=0x408d30, CAMERA_GLOBAL=0x408d38,
    MANAGER_VTABLE=0x2ca30c, CAMERA_VTABLE=0x2c5630, SOUL_VTABLE=0x2c5544,
    MANAGER_CTOR=0x79250, CAMERA_CTOR=0x117b0,
    MANAGER_INIT=0xf610, CAMERA_INIT=0x11900, MANAGER_UNLOCKS=0xac,
    MANAGER_DELETE=0x79600, CAMERA_DELETE=0x118c0, SOUL_DELETE=0xefc0,
    MAX_INSTANCES=4, CAST_GATE_GLOBAL=0x408dd0,
    CAST_GATE_VTABLE=0x2c7ad0, CAST_GATE_OFFSET=0xa18, CAST_BUSY_BIT=8,
    UI_GLOBAL=0x3c2f88, UI_VTABLE=0x2caf9c, UI_ROOT_GLOBAL=0x408d1c,
    CONTROLLER_GLOBAL=0x408da4, CONTROLLER_VTABLE=0x2c9f5c,
    NAMED_MANAGER_GLOBAL=0x409d7c, NAMED_MANAGER_VTABLE=0x2c7b80,
    NAMED_CAMERA_VTABLE=0x2cce5c, NAMED_CAMERA_SIZE=0x108,
    NAMED_CAMERA_NAME=0x4c, NAMED_NAME_SIZE=21, NAMED_SLOTS=10 };
typedef void *(*RawConstructor)(void);
typedef void (__attribute__((stdcall)) *ManagerInit)(void *);
typedef void (__attribute__((regparm(1))) *CameraInit)(void *);
typedef void *(__attribute__((thiscall)) *SpiritDeleteFunction)(void *,unsigned int);
typedef void (__attribute__((thiscall)) *NativeUpdate)(void *,float);
typedef struct Entry {
    SudekiMpSpiritInstance identity;
    void *souls[4];
    BOOL manager_constructed, manager_initialized, camera_constructed;
    BOOL destroying, camera_restore_pending, manager_restore_pending;
    void *saved_manager, *saved_camera;
    uint32_t camera_ticks, soul_ticks, manager_ticks;
    void *caster, *state_component, *caster_vtable, *state_vtable;
    uint64_t caster_session;
    SudekiMpSpiritCasterWitness caster_witness;
    BOOL caster_lock_owned;
    uint8_t caster_type;
    uint8_t cast_busy;
    uint32_t shared_ssp_last;
    BOOL remote_ui, remote_ui_acquired;
    BOOL remote_skill_ui, remote_skill_ui_acquired;
    void *skill, *skill_vtable;
    BOOL remote_skill_input, remote_skill_input_acquired;
    BOOL remote_state_ui_acquired;
    void *local_controller, *local_actor, *local_actor_vtable;
    SudekiMpSpiritCasterWitness local_actor_witness;
    BOOL named_ready, named_creation_uncertain;
    void *named_cameras[2];
    unsigned int named_slots[2];
    char named_names[2][NAMED_NAME_SIZE];
} Entry;
typedef BOOL (__attribute__((thiscall)) *NamedCameraAdd)(void *,const char *,const char *);
typedef void (__attribute__((thiscall)) *NamedCameraRemove)(void *,const char *);
static NamedCameraAdd named_add;
static NamedCameraRemove named_remove;
static void *named_manager,*named_originals[2];
static unsigned int named_original_slots[2];
static char named_original_names[2][NAMED_NAME_SIZE];
static const char named_parked[2][NAMED_NAME_SIZE]={"MP_BaseInit","MP_BaseSkill"};
static uint32_t named_generation;
static uint8_t *instance_image;
static SudekiMpSpiritInstanceIdleWitness idle_witness;
static SudekiMpSpiritInputOwnerWitness input_owner_witness;
static DWORD owner_thread;
static unsigned int operation_depth;
static uint32_t next_generation;
static void *primary_manager, *primary_camera;
static uint32_t next_scope_cookie;
static unsigned int scope_depth;
static struct {
    uint32_t cookie, generation;
    void *saved_manager, *saved_camera, *manager, *camera;
} scopes[16];
static Entry entries[MAX_INSTANCES];
/* Borrowed actor/component identities only. Never a constructed/scheduled
 * Spirit instance and never published through ResolveSpiritInstanceCaster. */
static Entry persistent_skill_ui;
static SudekiMpSpiritCasterWitness persistent_skill_task;
static uint8_t *cast_gate_manager;
static uint8_t neutral_cast_busy;
static BOOL shared_ssp_enabled;
static RawConstructor manager_ctor, camera_ctor;
static ManagerInit manager_init;
static CameraInit camera_init;
static SpiritDeleteFunction manager_delete, camera_delete, soul_delete;
static SudekiMpPointerHook update_hooks[3];
static NativeUpdate original_updates[3];
static unsigned int update_depth;
static BOOL update_fault;
static const uint32_t update_slots[3]={CAMERA_VTABLE+4,SOUL_VTABLE+4,MANAGER_VTABLE+4};
static const uint32_t update_rvas[3]={0x11bd0,0x12adf0,0xf900};
typedef void (*RawPeriodSetter)(void);
static RawPeriodSetter native_period_setter;
static RawConstructor native_participant_lock __attribute__((used));
typedef uint32_t (__attribute__((regparm(1),stdcall)) *NativeParticipantUnlock)(void *,uint32_t);
static NativeParticipantUnlock native_participant_unlock;
static void *native_ready_tail __attribute__((used));
static SudekiMpRelativeCallHook participant_hooks[2];
static SudekiMpInlineHook ui_hooks[2];
static void *ui_trampolines[2] __attribute__((used));
static void *ui_resumes[2] __attribute__((used));
static uint8_t *ui_owner,*ui_root;
static void *ui_root_vtable;
static SudekiMpInlineHook skill_ui_hooks[2];
static void *skill_ui_resumes[2] __attribute__((used));
static void *native_ui_recompute __attribute__((used));
static SudekiMpInlineHook skill_input_hooks[2];
static void *skill_input_resumes[2] __attribute__((used));
static SudekiMpInlineHook state_ui_hooks[2];
static void *state_ui_trampolines[2] __attribute__((used));
static void *state_ui_acquire_resume __attribute__((used));
static void *state_ui_release_resume __attribute__((used));
static const uint8_t readiness_body[]={
    0x8b,0x47,0x5c,0x83,0xe8,2,0x74,8,0x48,0x74,0x3c,0x5f,0x5e,0xc2,4,0,
    0x33,0xd2,0x8d,0x77,0x60,0x8d,0xa4,0x24,0,0,0,0,0x8b,6,0x85,0xc0,
    0x74,0x1c,0x8b,0x88,0x30,1,0,0,0x8a,0x89,0x31,1,0,0,0x80,0xf9,2,
    0x74,0x0b,0x80,0xf9,3,0x74,6,0x80,0x78,0x2b,0,0x76,0x23,0x42,
    0x83,0xc6,0x0c,0x83,0xfa,4,0x7c,0xd5};

static BOOL memory(const void *p,size_t n,BOOL write) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t a=(uintptr_t)p;
    return p && n && VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) && a+n>=a &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize &&
        (!write || (m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|
                              PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)));
}
static BOOL bytes(uint8_t *p,const uint8_t *expected,size_t n) {
    return memory(p,n,FALSE) && !memcmp(p,expected,n);
}
static BOOL address(uint8_t *p,void *expected) {
    return memory(p,4,FALSE) && *(void **)p==expected;
}
static BOOL call(uint8_t *b,uint32_t site,uint32_t target) {
    int32_t displacement;
    if(!memory(b+site,5,FALSE) || b[site]!=0xe8) return FALSE;
    memcpy(&displacement,b+site+1,4);
    return (int64_t)site+5+displacement==target;
}
static BOOL update_image_exact(uint8_t *b) {
    static const uint8_t cam[]={0x89,0x4c,0x24,0x04,0xe9,0x07,0,0,0};
    static const uint8_t soul[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x81,0xec,0xb4,0,0,0,
        0x53,0x8b,0xd9,0x80,0x7b,0x48,0};
    static const uint8_t manager_head[]={0x80,0x3d};
    static const uint8_t manager_tail[]={0,0x56,0x57,0x8b,0xf9,0x74,7,0xc6,5};
    static const uint8_t ret4[]={0xc2,4,0};
    return b && bytes(b+update_rvas[0],cam,sizeof(cam)) &&
        bytes(b+update_rvas[1],soul,sizeof(soul)) &&
        bytes(b+0xf900,manager_head,sizeof(manager_head)) &&
        address(b+0xf902,b+0x408d34) &&
        bytes(b+0xf906,manager_tail,sizeof(manager_tail)) &&
        address(b+0xf90f,b+0x408d34) &&
        bytes(b+0xf921,ret4,sizeof(ret4)) && bytes(b+0xf977,ret4,sizeof(ret4)) &&
        call(b,0xf969,0x10c20) &&
        address(b+update_slots[0],b+update_rvas[0]) &&
        address(b+update_slots[1],b+update_rvas[1]) &&
        address(b+update_slots[2],b+update_rvas[2]);
}
static BOOL cast_gate_image_exact(uint8_t *b) {
    static const uint8_t test[]={0xf6,0x81,0x18,0x0a,0,0,8};
    static const uint8_t set[]={0x80,0x88,0x18,0x0a,0,0,8};
    static const uint8_t clear[]={0x80,0xa0,0x18,0x0a,0,0,0xf7};
    static const uint8_t ctor_global[]={0x89,0x2d};
    static const uint8_t ctor_table[]={0xc7,0x45,0};
    static const uint8_t mov_ecx[]={0x8b,0x0d};
    return bytes(b+0x2f8d8,ctor_global,sizeof(ctor_global)) &&
        address(b+0x2f8da,b+CAST_GATE_GLOBAL) &&
        bytes(b+0x2f8de,ctor_table,sizeof(ctor_table)) &&
        address(b+0x2f8e1,b+CAST_GATE_VTABLE) &&
        bytes(b+0x10a1c,mov_ecx,sizeof(mov_ecx)) && address(b+0x10a1e,b+CAST_GATE_GLOBAL) &&
        bytes(b+0x10a22,test,sizeof(test)) &&
        bytes(b+0xb4bcf,mov_ecx,sizeof(mov_ecx)) && address(b+0xb4bd1,b+CAST_GATE_GLOBAL) &&
        bytes(b+0xb4bd5,test,sizeof(test)) &&
        b[0x10156]==0xa1 && address(b+0x10157,b+CAST_GATE_GLOBAL) && bytes(b+0x1015b,set,sizeof(set)) &&
        b[0xb4b68]==0xa1 && address(b+0xb4b69,b+CAST_GATE_GLOBAL) && bytes(b+0xb4b6d,set,sizeof(set)) &&
        b[0x11114]==0xa1 && address(b+0x11115,b+CAST_GATE_GLOBAL) && bytes(b+0x11125,clear,sizeof(clear)) &&
        b[0xb47f3]==0xa1 && address(b+0xb47f4,b+CAST_GATE_GLOBAL) && bytes(b+0xb47f8,clear,sizeof(clear));
}
static BOOL shared_ssp_image_exact(uint8_t *b) {
    /* GetSSP, native validation, native activation debit, and SetSSP all
     * access the scalar +a8. Do not infer this from constructor layout. */
    return bytes(b+0xf5e0,(const uint8_t *)"\x51\xa1",2) &&
        address(b+0xf5e2,b+MANAGER_GLOBAL) &&
        bytes(b+0xf5ea,(const uint8_t *)"\xd9\x80\xa8\x00\x00\x00",6) &&
        bytes(b+0x10a79,(const uint8_t *)"\xd9\x82\xa8\x00\x00\x00",6) &&
        bytes(b+0x1002d,(const uint8_t *)"\xd9\x86\xa8\x00\x00\x00",6) &&
        bytes(b+0x1008c,(const uint8_t *)"\xd9\x96\xa8\x00\x00\x00",6) &&
        bytes(b+0x111c3,(const uint8_t *)"\xd9\x99\xa8\x00\x00\x00",6) &&
        bytes(b+0x111e6,(const uint8_t *)"\xd9\x99\xa8\x00\x00\x00",6);
}
BOOL SudekiMpSpiritInstanceSharedSspAbiReady(void) {
    return instance_image && shared_ssp_image_exact(instance_image);
}
static BOOL image_exact(uint8_t *b) {
    static const uint8_t m_ctor[]={0xd9,0xe8,0x33,0xc0,0x83,0xc9,0xff,0xd9,0x5e,0x10};
    static const uint8_t c_ctor[]={0xd9,0xe8,0x66,0xc7,0x46,0x22,1,0,0xd9,0x5e,0x10};
    static const uint8_t m_init[]={0x83,0xec,8,0x53,0x55,0x8b,0x6c,0x24,0x14};
    static const uint8_t c_init[]={0x83,0xec,8,0x56,0x8b,0xf0,0x80,0x7e,0x22,0};
    static const uint8_t del[]={0x56,0x8b,0xf1};
    static const uint8_t period[]={0x83,0xec,8,0xd9,0x44,0x24,0x0c,0x56};
    static const uint8_t lock[]={0x51,0x80,0xbf,0x30,1,0,0,0xbf};
    static const uint8_t unlock[]={0x8a,0x4c,0x24,4,0x56,0x57,0x8b,0xf8};
    static const uint8_t ready_start[]={0xc7,0x40,0x24,3,0,0,0,0x8b,0xc7};
    static const uint8_t ready_end[]={0xc7,0x47,0x5c,0x0a,0,0,0,0x5f,0x5e,0xc2,4,0};
    static const uint8_t unlocked_reader[]={0x84,0x90,0xac,0,0,0};
    static const uint8_t unlocked_writer[]={0xc6,0x80,0xac,0,0,0,0xff};
    static const uint8_t ui_load[]={0x8b,0x35};
    static const uint8_t ui_add[]={0xbb,1,0,0,0,0x01,0x5e,0x54};
    static const uint8_t ui_sub[]={0x01,0x6e,0x54};
    static const uint8_t ui_ctor[]={0xc7,0x45,0};
    static const uint8_t ui_store[]={0x89,0x2d};
    static const uint8_t lock_ui[]={0x80,0xa7,0x33,1,0,0,0xf7,0x38,0x5c,0x24,0x10,0x74,0x1e};
    static const uint8_t unlock_ui[]={0xf6,0x87,0x33,1,0,0,8};
    static const uint8_t skill_ui_load[]={0x8b,0xb1,0x74,1,0,0};
    static const uint8_t skill_ui_add[]={0xff,0x46,0x54};
    static const uint8_t skill_acquire_tail[]={0x8b,0x45,8,0x8b,0x4c,0x83,0x3c};
    static const uint8_t skill_release_tail[]={0x8b,0x47,0x10,0x8b,0x40,0x60};
    static const uint8_t input_clear[]={0x83,0xa0,0xd0,1,0,0,0xfd};
    static const uint8_t input_set[]={0x83,0x88,0xd0,1,0,0,2};
    static const uint8_t input_reader[]={0x8b,0x8f,0xa4,1,0,0};
    static const uint8_t input_test[]={0xf6,0xc1,2,0x0f,0x84,0xfe,0,0,0};
    return update_image_exact(b) && cast_gate_image_exact(b) && bytes(b+MANAGER_CTOR,m_ctor,sizeof(m_ctor)) &&
        b[0xb4835]==0xa1 && address(b+0xb4836,b+CONTROLLER_GLOBAL) &&
        b[0xb483a]==0xd9 && b[0xb483b]==0xe8 && bytes(b+0xb483c,input_clear,7) &&
        b[0xb4e8c]==0xa1 && address(b+0xb4e8d,b+CONTROLLER_GLOBAL) && bytes(b+0xb4e91,input_set,7) &&
        b[0xb4843]==0xa1 && address(b+0xb4844,b+0x409d78) &&
        b[0xb4e98]==0xa1 && address(b+0xb4e99,b+0x409d78) &&
        bytes(b+0x278d9,input_reader,sizeof(input_reader)) && bytes(b+0x279ce,input_test,sizeof(input_test)) &&
        (address(b+0x2c9f84,b+0x277b0) ||
            (input_owner_witness && input_owner_witness((HMODULE)b))) &&
        bytes(b+0x9c528,ui_ctor,sizeof(ui_ctor)) && address(b+0x9c52b,b+UI_VTABLE) &&
        bytes(b+0x9c5dd,ui_store,sizeof(ui_store)) && address(b+0x9c5df,b+UI_GLOBAL) &&
        bytes(b+0x100d1,ui_load,sizeof(ui_load)) && address(b+0x100d3,b+UI_GLOBAL) &&
        bytes(b+0x100d7,ui_add,sizeof(ui_add)) && call(b,0x100df,0x9e560) &&
        bytes(b+0x10fe6,ui_load,sizeof(ui_load)) && address(b+0x10fe8,b+UI_GLOBAL) &&
        bytes(b+0x10fec,ui_sub,sizeof(ui_sub)) && call(b,0x10fef,0x9e560) &&
        bytes(b+0xb492e,skill_ui_load,sizeof(skill_ui_load)) &&
        bytes(b+0xb4f38,skill_ui_load,sizeof(skill_ui_load)) &&
        bytes(b+0xb4944,skill_ui_add,sizeof(skill_ui_add)) && call(b,0xb4947,0x9e560) &&
        bytes(b+0xb4f4e,ui_sub,sizeof(ui_sub)) && call(b,0xb4f51,0x9e560) &&
        bytes(b+0xb494c,skill_acquire_tail,sizeof(skill_acquire_tail)) &&
        bytes(b+0xb4f56,skill_release_tail,sizeof(skill_release_tail)) &&
        bytes(b+0xe459d,lock_ui,sizeof(lock_ui)) && b[0xe45aa]==0xa1 &&
        address(b+0xe45ab,b+UI_ROOT_GLOBAL) && call(b,0xe45bc,0x9e560) &&
        bytes(b+0xe45c1,(const uint8_t *)"\x80\x8f\x33\x01\x00\x00\x08",7) &&
        bytes(b+0xe45c8,(const uint8_t *)"\x5e\xb0\x01\x5b\x59\xc2\x08\x00",8) &&
        bytes(b+0xe46c6,unlock_ui,sizeof(unlock_ui)) && b[0xe46cd]==0xa1 &&
        address(b+0xe46ce,b+UI_ROOT_GLOBAL) && call(b,0xe46e1,0x9e560) &&
        bytes(b+0xe46e6,(const uint8_t *)"\xb0\x01\x5f\x5e\xc2\x04\x00",7) &&
        bytes(b+0x9b856,unlocked_reader,sizeof(unlocked_reader)) &&
        bytes(b+0x113bc,unlocked_writer,sizeof(unlocked_writer)) &&
        bytes(b+CAMERA_CTOR,c_ctor,sizeof(c_ctor)) &&
        bytes(b+MANAGER_INIT,m_init,sizeof(m_init)) &&
        bytes(b+CAMERA_INIT,c_init,sizeof(c_init)) &&
        bytes(b+MANAGER_DELETE,del,sizeof(del)) &&
        bytes(b+CAMERA_DELETE,del,sizeof(del)) &&
        bytes(b+SOUL_DELETE,del,sizeof(del)) &&
        bytes(b+0x1061d0,period,sizeof(period)) &&
        call(b,0x106266,0x134280) && call(b,0x106272,0x134330) &&
        call(b,0x106281,0x1342e0) && call(b,0x79df7,0x1061d0) &&
        call(b,0xfcd6,0xe4460) && call(b,0x10f36,0xe45d0) &&
        bytes(b+0xe4460,lock,sizeof(lock)) && bytes(b+0xe45d0,unlock,sizeof(unlock)) &&
        bytes(b+0xf914,readiness_body,sizeof(readiness_body)) &&
        b[0xf95b]==0xa1 && address(b+0xf95c,b+0x408da0) &&
        bytes(b+0xf960,ready_start,sizeof(ready_start)) &&
        bytes(b+0xf96e,ready_end,sizeof(ready_end)) &&
        call(b,0x478d0d-0x400000,MANAGER_CTOR) &&
        call(b,0x478d18-0x400000,CAMERA_CTOR) &&
        call(b,0x79c53,MANAGER_INIT) && call(b,0x79c5e,CAMERA_INIT) &&
        call(b,MANAGER_DELETE+5,0x792e0) && call(b,CAMERA_DELETE+3,0x119e0) &&
        call(b,SOUL_DELETE+5,0xef60) &&
        address(b+MANAGER_CTOR+0x40,b+MANAGER_GLOBAL) &&
        address(b+CAMERA_CTOR+0x97,b+CAMERA_GLOBAL) &&
        address(b+MANAGER_VTABLE,b+MANAGER_DELETE) &&
        address(b+CAMERA_VTABLE,b+CAMERA_DELETE) &&
        address(b+SOUL_VTABLE,b+SOUL_DELETE) &&
        memory(b+MANAGER_GLOBAL,4,TRUE) && memory(b+CAMERA_GLOBAL,4,TRUE);
}

/* Constructor this is ESI, not ECX. Both return the same object in EAX.
 * The wrapper owns ESI and preserves the compiler's callee-saved contract. */
__attribute__((naked,noinline)) static void * __attribute__((regparm(2)))
construct(RawConstructor function __attribute__((unused)),void *object __attribute__((unused))) {
    __asm__ volatile("pushl %esi\n\tmovl %edx,%esi\n\tcall *%eax\n\tpopl %esi\n\tret\n\t");
}

static BOOL boundary(void) {
    if(!instance_image || !idle_witness || operation_depth || scope_depth || update_depth ||
        (owner_thread && owner_thread!=GetCurrentThreadId()) || !idle_witness()) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if(!owner_thread) owner_thread=GetCurrentThreadId();
    return TRUE;
}
static BOOL object_exact(void *object,size_t size,uint32_t vtable) {
    return memory(object,size,TRUE) && *(void **)object==instance_image+vtable;
}
static BOOL globals_exact(void *manager,void *camera) {
    return address(instance_image+MANAGER_GLOBAL,manager) &&
        address(instance_image+CAMERA_GLOBAL,camera);
}
static BOOL originals(void **manager,void **camera) {
    if(!memory(instance_image+MANAGER_GLOBAL,4,TRUE) ||
        !memory(instance_image+CAMERA_GLOBAL,4,TRUE)) return FALSE;
    *manager=*(void **)(instance_image+MANAGER_GLOBAL);
    *camera=*(void **)(instance_image+CAMERA_GLOBAL);
    return object_exact(*manager,MANAGER_SIZE,MANAGER_VTABLE) &&
        object_exact(*camera,CAMERA_SIZE,CAMERA_VTABLE) &&
        *(uint32_t *)((uint8_t *)*manager+0x5c)==0 &&
        *(uint32_t *)((uint8_t *)*camera+0x1a0)==0;
}
static Entry *find(const SudekiMpSpiritInstance *instance) {
    unsigned int i;
    if(!instance || !instance->generation) return NULL;
    for(i=0;i<MAX_INSTANCES;++i) {
        Entry *e=&entries[i];
        if(e->identity.generation==instance->generation &&
            e->identity.manager==instance->manager && e->identity.camera==instance->camera)
            return e;
    }
    return NULL;
}

static BOOL quiescent(const Entry *e);
static BOOL caster_exact(const Entry *e);

static BOOL cast_gate_exact(void) {
    return address(instance_image+CAST_GATE_GLOBAL,cast_gate_manager) &&
        object_exact(cast_gate_manager,CAST_GATE_OFFSET+1,CAST_GATE_VTABLE);
}
static uint8_t cast_gate_union(void) {
    unsigned int i;
    uint8_t busy=neutral_cast_busy;
    for(i=0;i<MAX_INSTANCES;++i) if(entries[i].identity.generation) busy|=entries[i].cast_busy;
    return busy;
}
static Entry *generation_entry(uint32_t generation) {
    unsigned int i;
    for(i=0;i<MAX_INSTANCES;++i)
        if(generation && entries[i].identity.generation==generation) return &entries[i];
    return NULL;
}
static BOOL ssp_read(void *manager,uint32_t *bits) {
    float value;
    if(!object_exact(manager,MANAGER_SIZE,MANAGER_VTABLE) || !memory((uint8_t *)manager+0xa8,4,TRUE)) return FALSE;
    memcpy(bits,(uint8_t *)manager+0xa8,4); memcpy(&value,bits,4);
    return isfinite(value) && value>=0.0f;
}
static BOOL shared_ssp_transition_ready(uint32_t generation) {
    Entry *previous=scope_depth ? generation_entry(scopes[scope_depth-1].generation):NULL;
    Entry *next=generation ? generation_entry(generation):NULL;
    uint32_t bits;
    if(!shared_ssp_enabled) return TRUE;
    if(!ssp_read(primary_manager,&bits) || (generation && (!next || !caster_exact(next))) ||
        (previous && (!caster_exact(previous) || !ssp_read(previous->identity.manager,&bits)))) return FALSE;
    /* A dormant private context cannot gain/lose resources behind the scope
     * owner. The original manager CAN change normally between native calls. */
    if(next && next!=previous && (!ssp_read(next->identity.manager,&bits) || bits!=next->shared_ssp_last))
        return FALSE;
    return TRUE;
}
static void shared_ssp_transition_commit(uint32_t generation) {
    Entry *previous=scope_depth ? generation_entry(scopes[scope_depth-1].generation):NULL;
    Entry *next=generation ? generation_entry(generation):NULL;
    if(!shared_ssp_enabled) return;
    if(previous) {
        memcpy(&previous->shared_ssp_last,(uint8_t *)previous->identity.manager+0xa8,4);
        memcpy((uint8_t *)primary_manager+0xa8,&previous->shared_ssp_last,4);
    }
    if(next) {
        memcpy(&next->shared_ssp_last,(uint8_t *)primary_manager+0xa8,4);
        memcpy((uint8_t *)next->identity.manager+0xa8,&next->shared_ssp_last,4);
    }
}

BOOL SudekiMpSpiritInstanceNamedCameraAbiReady(void) {
    uint8_t *b=instance_image;
    /* Constructors/destructors remain native. No new camera-manager hook,
     * no competing SetRenderCamera detour, and no shallow object copies. */
    return b && bytes(b+0x36c10,(const uint8_t *)"\x83\xec\x14\x53\x55\x8b\x6c\x24\x20",9) &&
        bytes(b+0x36cd4,(const uint8_t *)"\x68\x08\x01\x00\x00",5) &&
        call(b,0x36cd9,0x2484fa) && call(b,0x36ce6,0xe7110) &&
        bytes(b+0x36cf1,(const uint8_t *)"\x89\x74\xbb\x24",4) &&
        bytes(b+0x36d3b,(const uint8_t *)"\x8b\x16\x8b\x42\x04\x8b\xce\xff\xd0",9) &&
        call(b,0x36d54,0x1061d0) && call(b,0x36d72,0x249580) &&
        bytes(b+0x36d80,(const uint8_t *)"\xc6\x46\x60\x00",4) &&
        call(b,0x36d8b,0xe8360) &&
        bytes(b+0x36de0,(const uint8_t *)"\x53\x55\x8b\x6c\x24\x0c\x56\x8b\xd9\x57\x33\xf6\x8d\x7b\x24",15) &&
        call(b,0x36dfb,0x24ae0e) &&
        bytes(b+0x36e39,(const uint8_t *)"\x8b\x4c\xb3\x24\x8b\x11\x8b\x42\x08\xff\xd0",11) &&
        bytes(b+0x36e58,(const uint8_t *)"\xc7\x44\xb3\x24\x00\x00\x00\x00",8) &&
        bytes(b+0x36ed0,(const uint8_t *)"\x53\x8b\x5c\x24\x08\x55\x8b\xe9",8) &&
        bytes(b+0x36ee5,(const uint8_t *)"\x8d\x7d\x24\x8b\x07\x85\xc0\x74\x11\x83\xc0\x4c",12) &&
        call(b,0x36ef3,0x24ae0e) &&
        bytes(b+0x36eff,(const uint8_t *)"\x46\x83\xc7\x04\x83\xfe\x0a\x72\xe0",9);
}

static void **named_slot(unsigned int slot) {
    return (void **)((uint8_t *)named_manager+0x24+4*slot);
}
static BOOL named_registry_exact(void) {
    unsigned int i,j;
    if(!named_manager || !address(instance_image+NAMED_MANAGER_GLOBAL,named_manager) ||
        !object_exact(named_manager,0x60,NAMED_MANAGER_VTABLE)) return FALSE;
    for(i=0;i<NAMED_SLOTS;++i) {
        uint8_t *p=*named_slot(i);
        if(!p) continue;
        if(!object_exact(p,NAMED_CAMERA_SIZE,NAMED_CAMERA_VTABLE) ||
            !memchr(p+NAMED_CAMERA_NAME,0,NAMED_NAME_SIZE)) return FALSE;
        for(j=0;j<i;++j) {
            uint8_t *other=*named_slot(j);
            if(other && (p==other || !_stricmp((char *)p+NAMED_CAMERA_NAME,
                    (char *)other+NAMED_CAMERA_NAME))) return FALSE;
        }
    }
    return TRUE;
}
static BOOL named_camera_exact(void *camera,unsigned int slot,const char *name) {
    return slot<NAMED_SLOTS && *named_slot(slot)==camera &&
        object_exact(camera,NAMED_CAMERA_SIZE,NAMED_CAMERA_VTABLE) &&
        !memcmp((uint8_t *)camera+NAMED_CAMERA_NAME,name,NAMED_NAME_SIZE);
}
static BOOL named_namespace_exact(void) {
    unsigned int i,k;
    if(!named_manager) return TRUE;
    if(!named_registry_exact()) return FALSE;
    for(k=0;k<2;++k) if(!named_camera_exact(named_originals[k],named_original_slots[k],
        named_generation ? named_parked[k]:named_original_names[k])) return FALSE;
    for(i=0;i<MAX_INSTANCES;++i) {
        Entry *e=&entries[i];
        if(e->named_creation_uncertain) return FALSE;
        if(e->named_ready && (!e->named_cameras[0] || !e->named_cameras[1])) return FALSE;
        if(!e->named_cameras[0] && !e->named_cameras[1]) continue;
        if(!e->identity.generation || !caster_exact(e)) return FALSE;
        for(k=0;k<2;++k) if(e->named_cameras[k] && !named_camera_exact(e->named_cameras[k],e->named_slots[k],
            named_generation==e->identity.generation ? named_original_names[k]:e->named_names[k])) return FALSE;
    }
    return !named_generation || (generation_entry(named_generation) && generation_entry(named_generation)->named_ready);
}
/* Preflight and commit are deliberately separate: busy-bit banking may reject
 * a scope too. Once all checks pass, these bounded copies make no callbacks. */
static BOOL named_transition_ready(uint32_t generation) {
    Entry *next=generation_entry(generation);
    if(!named_manager) return TRUE;
    if(!named_namespace_exact() || (next &&
        (next->named_cameras[0] || next->named_cameras[1]) && !next->named_ready)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    return TRUE;
}
static void named_transition_commit(uint32_t generation) {
    Entry *previous=generation_entry(named_generation),*next=generation_entry(generation);
    unsigned int k;
    if(!named_manager) return;
    if(next && !next->named_ready) next=NULL;
    for(k=0;k<2;++k) {
        if(previous) memcpy((uint8_t *)previous->named_cameras[k]+NAMED_CAMERA_NAME,
            previous->named_names[k],NAMED_NAME_SIZE);
        memcpy((uint8_t *)named_originals[k]+NAMED_CAMERA_NAME,
            next ? named_parked[k]:named_original_names[k],NAMED_NAME_SIZE);
        if(next) memcpy((uint8_t *)next->named_cameras[k]+NAMED_CAMERA_NAME,
            named_original_names[k],NAMED_NAME_SIZE);
    }
    named_generation=next ? next->identity.generation:0;
}
static void *named_lookup(const char *name,unsigned int *slot) {
    unsigned int i;
    for(i=0;i<NAMED_SLOTS;++i) {
        uint8_t *p=*named_slot(i);
        if(p && !_stricmp((char *)p+NAMED_CAMERA_NAME,name)) {
            if(slot) *slot=i;
            return p;
        }
    }
    return NULL;
}
static BOOL named_unselected(const Entry *e) {
    unsigned int k;
    uint8_t *scene_manager,*scene;
    if(named_generation || !named_namespace_exact() ||
        !memory(instance_image+0x408d58,4,FALSE)) return FALSE;
    scene_manager=*(void **)(instance_image+0x408d58);
    if(!memory(scene_manager,0x44,FALSE)) return FALSE;
    scene=*(void **)(scene_manager+0x40);
    if(!memory(scene,0x80,FALSE)) return FALSE;
    /* Check BOTH objects before the first destruction. A selected camera or
     * any live intrusive GELPointer pins the entire retained native lease. */
    for(k=0;k<2;++k) if(e->named_cameras[k]) {
        uint8_t *p=e->named_cameras[k];
        void *render=*(void **)(p+0x34);
        if(!render || *(void **)((uint8_t *)named_manager+0x20)==p ||
            *(void **)(scene+0x7c)==render || *(void **)(p+0x30)) return FALSE;
    }
    return TRUE;
}
static BOOL named_retire(Entry *e) {
    unsigned int k;
    if(e->named_creation_uncertain) return FALSE;
    if(!e->named_cameras[0] && !e->named_cameras[1]) return TRUE;
    if(!named_unselected(e)) return FALSE;
    e->named_ready=FALSE; /* Partial retirement must never be entered/observed. */
    for(k=2;k>0;--k) if(e->named_cameras[k-1]) {
        if(!named_unselected(e)) return FALSE;
        named_remove(named_manager,e->named_names[k-1]);
        /* Never guess that a replaced/non-null slot was our successful delete. */
        if(!named_registry_exact() || *named_slot(e->named_slots[k-1])) return FALSE;
        e->named_cameras[k-1]=NULL;
    }
    e->named_ready=FALSE;
    return TRUE;
}

BOOL SudekiMpEnableSpiritInstanceNamedCameras(const SudekiMpSpiritInstance *instance) {
    Entry *e=find(instance);
    unsigned int i,k,free_slots=0;
    void *manager;
    if(!e || !boundary() || update_fault || !quiescent(e) || !caster_exact(e) ||
        e->named_ready || e->named_creation_uncertain || e->named_cameras[0] || e->named_cameras[1] ||
        !SudekiMpSpiritInstanceNamedCameraAbiReady() || !memory(instance_image+NAMED_MANAGER_GLOBAL,4,FALSE)) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    manager=*(void **)(instance_image+NAMED_MANAGER_GLOBAL);
    if(!named_manager) {
        named_manager=manager;
        if(!named_registry_exact()) { named_manager=NULL; SetLastError(ERROR_INVALID_DATA); return FALSE; }
        named_originals[0]=named_lookup("InitCam",&named_original_slots[0]);
        named_originals[1]=named_lookup("SkillCam",&named_original_slots[1]);
        if(!named_originals[0] || !named_originals[1] ||
            named_lookup(named_parked[0],NULL) || named_lookup(named_parked[1],NULL)) {
            named_manager=NULL; SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
        for(k=0;k<2;++k) memcpy(named_original_names[k],
            (uint8_t *)named_originals[k]+NAMED_CAMERA_NAME,NAMED_NAME_SIZE);
        named_add=(NamedCameraAdd)(instance_image+0x36c10);
        named_remove=(NamedCameraRemove)(instance_image+0x36de0);
    }
    if(manager!=named_manager || !named_namespace_exact()) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    for(i=0;i<NAMED_SLOTS;++i) if(!*named_slot(i)) ++free_slots;
    if(free_slots<2) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE; }
    for(k=0;k<2;++k) {
        snprintf(e->named_names[k],NAMED_NAME_SIZE,"MP%08lx%c",(unsigned long)e->identity.generation,k ? 'S':'I');
        if(named_lookup(e->named_names[k],NULL)) { SetLastError(ERROR_ALREADY_EXISTS); return FALSE; }
    }
    ++operation_depth;
    for(k=0;k<2;++k) {
        BOOL added;
        if(!named_namespace_exact()) break;
        for(i=0;i<NAMED_SLOTS && *named_slot(i);++i) {}
        if(i==NAMED_SLOTS) break;
        e->named_slots[k]=i;
        e->named_creation_uncertain=TRUE;
        added=named_add(named_manager,e->named_names[k],"default");
        /* Pin the expected native slot before considering any failure. If a
         * callback replaced the registry, never pretend an allocation was
         * rolled back or clear its owning Spirit instance. */
        if(!memory(named_manager,0x60,FALSE)) break;
        e->named_cameras[k]=*named_slot(i);
        if(!named_registry_exact() || (e->named_cameras[k] &&
            !named_camera_exact(e->named_cameras[k],i,e->named_names[k]))) break;
        e->named_creation_uncertain=FALSE;
        if(!added || !e->named_cameras[k]) break;
    }
    e->named_ready=k==2 && named_namespace_exact();
    if(!e->named_ready) (void)named_retire(e); /* Keep failed cleanup for Destroy. */
    --operation_depth;
    if(!e->named_ready) SetLastError(ERROR_INVALID_DATA);
    return e->named_ready;
}

BOOL SudekiMpObserveSpiritInstanceNamedCamera(const SudekiMpSpiritInstance *instance,
    unsigned int kind,void **camera) {
    Entry *e=find(instance);
    if(camera) *camera=NULL;
    if(!camera || !e || !e->named_ready || e->destroying || update_fault || operation_depth ||
        kind<1 || kind>2 || owner_thread!=GetCurrentThreadId() || !named_namespace_exact()) return FALSE;
    *camera=e->named_cameras[kind-1];
    return TRUE;
}
/* Validate all participants before changing either a bank or native memory.
 * Never restore a saved byte: an inner A -> B -> A call may have changed A's
 * state, and the seven unrelated actor-manager bits remain native-owned. */
static BOOL cast_gate_transition(uint32_t next_generation,BOOL outside) {
    Entry *previous=NULL,*next=NULL;
    uint32_t previous_generation=scope_depth ? scopes[scope_depth-1].generation:0;
    uint8_t native_busy,next_busy;
    if(!cast_gate_manager) return TRUE;
    if(!cast_gate_exact() ||
        (previous_generation && (!(previous=generation_entry(previous_generation)) || !caster_exact(previous))) ||
        (next_generation && (!(next=generation_entry(next_generation)) || !caster_exact(next)))) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    native_busy=cast_gate_manager[CAST_GATE_OFFSET]&CAST_BUSY_BIT;
    if(!scope_depth) {
        /* An unscoped writer is not permission to discard another cast. */
        if(native_busy!=cast_gate_union()) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    } else if(previous) previous->cast_busy=native_busy;
    else neutral_cast_busy=native_busy;
    next_busy=outside ? cast_gate_union():(next ? next->cast_busy:neutral_cast_busy);
    cast_gate_manager[CAST_GATE_OFFSET]=(cast_gate_manager[CAST_GATE_OFFSET]&~CAST_BUSY_BIT)|next_busy;
    return TRUE;
}

BOOL SudekiMpEnableSpiritInstanceCastGates(void) {
    unsigned int i,count=0;
    uint8_t *manager;
    if(!boundary() || cast_gate_manager || update_fault || !primary_manager ||
        !globals_exact(primary_manager,primary_camera) ||
        !memory(instance_image+CAST_GATE_GLOBAL,4,FALSE)) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    manager=*(void **)(instance_image+CAST_GATE_GLOBAL);
    if(!object_exact(manager,CAST_GATE_OFFSET+1,CAST_GATE_VTABLE) ||
        (manager[CAST_GATE_OFFSET]&CAST_BUSY_BIT)) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    for(i=0;i<MAX_INSTANCES;++i) if(entries[i].identity.generation) {
        if(!quiescent(&entries[i]) || !caster_exact(&entries[i])) {
            SetLastError(ERROR_BUSY); return FALSE;
        }
        ++count;
    }
    if(!count) { SetLastError(ERROR_BUSY); return FALSE; }
    cast_gate_manager=manager; neutral_cast_busy=0;
    return TRUE;
}

BOOL SudekiMpEnableSpiritInstanceSharedSsp(void) {
    uint32_t bits;
    unsigned int count=0;
    if(!boundary() || shared_ssp_enabled || update_fault ||
        !shared_ssp_image_exact(instance_image) || !globals_exact(primary_manager,primary_camera) ||
        !ssp_read(primary_manager,&bits)) return FALSE;
    for(unsigned int i=0;i<MAX_INSTANCES;++i) if(entries[i].identity.generation) {
        if(!quiescent(&entries[i]) || !caster_exact(&entries[i])) return FALSE;
        ++count;
    }
    if(!count) return FALSE;
    for(unsigned int i=0;i<MAX_INSTANCES;++i) if(entries[i].identity.generation) {
        entries[i].shared_ssp_last=bits;
        memcpy((uint8_t *)entries[i].identity.manager+0xa8,&bits,4);
    }
    shared_ssp_enabled=TRUE;
    return TRUE;
}

BOOL SudekiMpInitializeSpiritInstanceAbi(HMODULE image,SudekiMpSpiritInstanceIdleWitness witness) {
    uint8_t *b=(uint8_t *)image;
    if(instance_image || !witness || !image_exact(b)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    instance_image=b; idle_witness=witness;
    manager_ctor=(RawConstructor)(b+MANAGER_CTOR); camera_ctor=(RawConstructor)(b+CAMERA_CTOR);
    manager_init=(ManagerInit)(b+MANAGER_INIT); camera_init=(CameraInit)(b+CAMERA_INIT);
    manager_delete=(SpiritDeleteFunction)(b+MANAGER_DELETE); camera_delete=(SpiritDeleteFunction)(b+CAMERA_DELETE);
    soul_delete=(SpiritDeleteFunction)(b+SOUL_DELETE);
    native_period_setter=(RawPeriodSetter)(b+0x1061d0);
    native_participant_lock=(RawConstructor)(b+0xe4460);
    native_participant_unlock=(NativeParticipantUnlock)(b+0xe45d0);
    native_ready_tail=b+0xf95b;
    return TRUE;
}

BOOL SudekiMpInitializeSpiritInstanceAbiWithInputOwner(HMODULE image,
    SudekiMpSpiritInstanceIdleWitness idle,SudekiMpSpiritInputOwnerWitness input_owner) {
    if(instance_image || input_owner_witness || !input_owner || !input_owner(image)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    input_owner_witness=input_owner;
    if(SudekiMpInitializeSpiritInstanceAbi(image,idle)) return TRUE;
    input_owner_witness=NULL;
    return FALSE;
}

BOOL SudekiMpCreateSpiritInstance(SudekiMpSpiritInstance *instance) {
    Entry *e=NULL;
    void *saved_manager,*saved_camera;
    unsigned int i,j;
    BOOL ok=FALSE;
    if(shared_ssp_enabled || persistent_skill_ui.caster || !instance || instance->generation || instance->manager || instance->camera ||
        !boundary() || cast_gate_manager || !originals(&saved_manager,&saved_camera) || next_generation==UINT_MAX) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if(primary_manager && (saved_manager!=primary_manager || saved_camera!=primary_camera)) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    for(i=0;i<MAX_INSTANCES;++i) if(entries[i].identity.generation &&
        (entries[i].destroying || !quiescent(&entries[i]))) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    for(i=0;i<MAX_INSTANCES;++i) if(!entries[i].identity.generation) { e=&entries[i]; break; }
    if(!e) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE; }
    e->identity.manager=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,MANAGER_SIZE);
    e->identity.camera=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,CAMERA_SIZE);
    if(!e->identity.manager || !e->identity.camera) {
        if(e->identity.camera) HeapFree(GetProcessHeap(),0,e->identity.camera);
        if(e->identity.manager) HeapFree(GetProcessHeap(),0,e->identity.manager);
        memset(e,0,sizeof(*e)); SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE;
    }
    e->identity.generation=++next_generation;
    primary_manager=saved_manager; primary_camera=saved_camera;
    *instance=e->identity; /* Publish the lifetime BEFORE any native constructor. */
    ++operation_depth;
    e->manager_constructed=TRUE;
    if(construct(manager_ctor,e->identity.manager)!=e->identity.manager ||
        !object_exact(e->identity.manager,MANAGER_SIZE,MANAGER_VTABLE) ||
        !globals_exact(e->identity.manager,saved_camera)) goto finished;
    manager_init(e->identity.manager);
    e->manager_initialized=TRUE;
    for(i=0;i<4;++i) {
        e->souls[i]=*(void **)((uint8_t *)e->identity.manager+0xb0+4*i);
        if(!object_exact(e->souls[i],SOUL_SIZE,SOUL_VTABLE)) goto finished;
        for(j=0;j<i;++j) if(e->souls[i]==e->souls[j]) goto finished;
    }
    if(!globals_exact(e->identity.manager,saved_camera)) goto finished;
    e->camera_constructed=TRUE;
    if(construct(camera_ctor,e->identity.camera)!=e->identity.camera ||
        !object_exact(e->identity.camera,CAMERA_SIZE,CAMERA_VTABLE) ||
        !globals_exact(e->identity.manager,e->identity.camera)) goto finished;
    camera_init(e->identity.camera);
    ok=globals_exact(e->identity.manager,e->identity.camera) &&
        *(uint32_t *)((uint8_t *)e->identity.manager+0x5c)==0 &&
        *(uint32_t *)((uint8_t *)e->identity.camera+0x1a0)==0 &&
        object_exact(saved_manager,MANAGER_SIZE,MANAGER_VTABLE);
    if(ok) {
        /* The constructor starts with no strikes unlocked. This is immutable
         * progress metadata for this idle-boundary test instance, not cast
         * state or a new grant. Preserve locked bits and leave the original
         * manager/resources untouched; never copy task/participant pointers. */
        ((uint8_t *)e->identity.manager)[MANAGER_UNLOCKS]=
            ((const uint8_t *)saved_manager)[MANAGER_UNLOCKS];
    }
finished:
    /* Only restore a slot still owned by this construction. An unexpected
     * native owner is not ours to overwrite. Retain the objects on failure. */
    if(address(instance_image+MANAGER_GLOBAL,e->identity.manager))
        *(void **)(instance_image+MANAGER_GLOBAL)=saved_manager;
    if(address(instance_image+CAMERA_GLOBAL,e->identity.camera))
        *(void **)(instance_image+CAMERA_GLOBAL)=saved_camera;
    ok=ok && globals_exact(saved_manager,saved_camera);
    --operation_depth;
    if(!ok) SetLastError(ERROR_INVALID_DATA);
    return ok;
}

static BOOL quiescent(const Entry *e) {
    const uint8_t *m=e->identity.manager,*c=e->identity.camera;
    unsigned int i;
    if(!e->manager_constructed || !e->manager_initialized || !e->camera_constructed || e->caster_lock_owned || e->cast_busy || e->remote_ui_acquired || e->remote_skill_ui_acquired || e->remote_skill_input_acquired || e->remote_state_ui_acquired ||
        !object_exact((void *)m,MANAGER_SIZE,MANAGER_VTABLE) ||
        !object_exact((void *)c,CAMERA_SIZE,CAMERA_VTABLE) ||
        *(uint32_t *)(m+0x5c) || *(uint32_t *)(c+0x1a0) ||
        *(void **)(m+0x28) || *(void **)(c+0x28)) return FALSE;
    /* Intrusive references are live native obligations, not just raw values. */
    for(i=0x44;i<0x5c;i+=4) if(*(uint32_t *)(m+i)) return FALSE;
    for(i=0x60;i<0x90;i+=4) if(*(uint32_t *)(m+i)) return FALSE;
    for(i=0x54;i<0x90;i+=4) if(*(uint32_t *)(c+i)) return FALSE;
    for(i=0;i<4;++i) {
        const uint8_t *s=e->souls[i];
        if(*(void **)(m+0xb0+4*i)!=s ||
            !object_exact((void *)s,SOUL_SIZE,SOUL_VTABLE) ||
            *(void **)(s+0x28) || *(void **)(s+0x40) || s[0x48]) return FALSE;
    }
    return TRUE;
}

static BOOL caster_exact(const Entry *e) {
    uint8_t *a=e->caster,*s=e->state_component;
    return a && e->caster_witness && e->caster_witness(a,e->caster_session) &&
        memory(a,0x134,FALSE) && *(void **)a==e->caster_vtable &&
        *(void **)(a+0x130)==s && memory(s,0x134,FALSE) &&
        *(void **)s==e->state_vtable && *(void **)(s+0x10)==a;
}
static BOOL remote_ui_exact(void) {
    return object_exact(ui_owner,0xe4,UI_VTABLE) &&
        address(instance_image+UI_GLOBAL,ui_owner) &&
        address(instance_image+UI_ROOT_GLOBAL,ui_root) &&
        memory(ui_root,0x178,FALSE) && *(void **)ui_root==ui_root_vtable &&
        *(void **)(ui_root+0x174)==ui_owner;
}
BOOL SudekiMpEnableSpiritInstanceRemoteUi(const SudekiMpSpiritInstance *instance) {
    Entry *e=find(instance);
    uint8_t *u,*r;
    if(!e || !boundary() || update_fault || !quiescent(e) || !caster_exact(e) ||
        e->remote_ui || !ui_hooks[0].installed || !ui_hooks[1].installed ||
        !state_ui_hooks[0].installed || !state_ui_hooks[1].installed ||
        !memory(instance_image+UI_GLOBAL,4,FALSE) ||
        !memory(instance_image+UI_ROOT_GLOBAL,4,FALSE)) return FALSE;
    u=*(void **)(instance_image+UI_GLOBAL); r=*(void **)(instance_image+UI_ROOT_GLOBAL);
    if(!object_exact(u,0xe4,UI_VTABLE) || !memory(r,0x178,FALSE) ||
        *(void **)(r+0x174)!=u || (ui_owner && (!remote_ui_exact() || ui_owner!=u)) ||
        (((uint8_t *)e->state_component)[0x131]!=0 && ((uint8_t *)e->state_component)[0x131]!=4))
        return FALSE;
    ui_owner=u; ui_root=r; ui_root_vtable=*(void **)r;
    e->remote_ui=TRUE;
    return TRUE;
}
static BOOL skill_ui_exact(const Entry *e,void *skill) {
    return caster_exact(e) && skill==e->skill &&
        *(void **)((uint8_t *)e->caster+0xd8)==skill && memory(skill,0x78,FALSE) &&
        *(void **)skill==e->skill_vtable &&
        *(void **)((uint8_t *)skill+0x18)==instance_image+0x2cbadc &&
        *(void **)((uint8_t *)skill+0x10)==e->caster && ((uint8_t *)skill)[0x6c]<=1;
}
BOOL SudekiMpEnableSpiritInstanceRemoteSkillUi(const SudekiMpSpiritInstance *instance) {
    Entry *e=find(instance);
    uint8_t *skill;
    if(!e || !boundary() || update_fault || !quiescent(e) || !caster_exact(e) ||
        !e->remote_ui || e->remote_skill_ui || !remote_ui_exact() ||
        !skill_ui_hooks[0].installed || !skill_ui_hooks[1].installed) return FALSE;
    skill=*(void **)((uint8_t *)e->caster+0xd8);
    if(!memory(skill,0x78,FALSE) || *(void **)(skill+0x10)!=e->caster ||
        *(void **)(skill+0x18)!=instance_image+0x2cbadc || skill[0x6c]) return FALSE;
    e->skill=skill; e->skill_vtable=*(void **)skill;
    e->remote_skill_ui=TRUE;
    return TRUE;
}
static BOOL skill_input_exact(const Entry *e,void *controller) {
    return controller==e->local_controller &&
        (address(instance_image+0x2c9f84,instance_image+0x277b0) ||
            (input_owner_witness && input_owner_witness((HMODULE)instance_image))) &&
        object_exact(controller,0x24c,CONTROLLER_VTABLE) &&
        address(instance_image+CONTROLLER_GLOBAL,controller) &&
        e->local_actor && e->local_actor!=e->caster && e->local_actor_witness &&
        e->local_actor_witness(e->local_actor,e->caster_session) &&
        memory(e->local_actor,0x134,FALSE) && *(void **)e->local_actor==e->local_actor_vtable &&
        *(void **)((uint8_t *)controller+0x248)==e->local_actor;
}
BOOL SudekiMpEnableSpiritInstanceRemoteSkillInput(const SudekiMpSpiritInstance *instance,
    void *local_actor,SudekiMpSpiritCasterWitness local_witness) {
    Entry *e=find(instance);
    void *controller;
    if(!e || !boundary() || update_fault || !quiescent(e) || !e->remote_skill_ui ||
        !skill_ui_exact(e,e->skill) || ((uint8_t *)e->skill)[0x6c] || e->remote_skill_input ||
        !skill_input_hooks[0].installed || !skill_input_hooks[1].installed ||
        !local_actor || local_actor==e->caster || !local_witness ||
        !local_witness(local_actor,e->caster_session) || !memory(local_actor,0x134,FALSE) ||
        !memory(instance_image+CONTROLLER_GLOBAL,4,FALSE)) return FALSE;
    controller=*(void **)(instance_image+CONTROLLER_GLOBAL);
    if(!object_exact(controller,0x24c,CONTROLLER_VTABLE) ||
        *(void **)((uint8_t *)controller+0x248)!=local_actor) return FALSE;
    e->local_controller=controller; e->local_actor=local_actor;
    e->local_actor_vtable=*(void **)local_actor; e->local_actor_witness=local_witness;
    e->remote_skill_input=TRUE;
    return TRUE;
}

BOOL SudekiMpBindRemoteCharacterSkillUi(void *local_actor,void *remote_actor,
    uint8_t remote_type,uint64_t session,SudekiMpSpiritCasterWitness retained,
    SudekiMpSpiritCasterWitness skill_task) {
    Entry e={0};
    uint8_t *a=remote_actor,*s,*k,*u,*r;
    void *manager,*camera,*controller;
    unsigned int i;
    if(!boundary() || update_fault || persistent_skill_ui.caster || !session ||
        !retained || !skill_task || !local_actor || local_actor==remote_actor ||
        !retained(local_actor,session) || !retained(remote_actor,session) ||
        !originals(&manager,&camera) || !memory(a,0x134,FALSE) ||
        !memory(local_actor,0x134,FALSE) ||
        (remote_type!=0x23 && remote_type!=1 && remote_type!=5 && remote_type!=14) ||
        !skill_ui_hooks[0].installed || !skill_ui_hooks[1].installed ||
        !skill_input_hooks[0].installed || !skill_input_hooks[1].installed ||
        !state_ui_hooks[0].installed || !state_ui_hooks[1].installed) return FALSE;
    for(i=0;i<MAX_INSTANCES;++i) if(entries[i].identity.generation) return FALSE;
    s=*(void **)(a+0x130); k=*(void **)(a+0xd8);
    u=*(void **)(instance_image+UI_GLOBAL); r=*(void **)(instance_image+UI_ROOT_GLOBAL);
    controller=*(void **)(instance_image+CONTROLLER_GLOBAL);
    /* Native unlock leaves +133 bit8 set after decrementing UI. Idle modes
     * 0/4 return before that decrement; the next lock clears bit8 at e459d
     * BEFORE our acquisition seam. Do not interpret the stale idle bit as an
     * outstanding UI lease or clear it ourselves. Active modes still reject. */
    if(!memory(s,0x134,FALSE) || *(void **)(s+0x10)!=a ||
        (s[0x131]!=0 && s[0x131]!=4) ||
        !memory(k,0x78,FALSE) || *(void **)(k+0x10)!=a || k[0x6c] ||
        *(void **)(k+0x18)!=instance_image+0x2cbadc ||
        !object_exact(u,0xe4,UI_VTABLE) || !memory(r,0x178,FALSE) ||
        *(void **)(r+0x174)!=u ||
        !object_exact(controller,0x24c,CONTROLLER_VTABLE) ||
        *(void **)((uint8_t *)controller+0x248)!=local_actor) return FALSE;
    e.caster=a; e.caster_vtable=*(void **)a; e.state_component=s;
    e.state_vtable=*(void **)s; e.caster_type=remote_type; e.caster_session=session;
    e.caster_witness=retained; e.skill=k; e.skill_vtable=*(void **)k;
    e.local_actor=local_actor; e.local_actor_vtable=*(void **)local_actor;
    e.local_actor_witness=retained; e.local_controller=controller;
    e.saved_manager=manager; e.saved_camera=camera;
    e.remote_ui=e.remote_skill_ui=e.remote_skill_input=TRUE;
    ui_owner=u; ui_root=r; ui_root_vtable=*(void **)r;
    persistent_skill_ui=e; persistent_skill_task=skill_task;
    return TRUE;
}
BOOL SudekiMpRemoteCharacterSkillUiHealthy(void) {
    Entry *e=&persistent_skill_ui;
    return !update_fault && (!e->caster || (GetCurrentThreadId()==owner_thread &&
        skill_ui_exact(e,e->skill) && remote_ui_exact() &&
        skill_input_exact(e,e->local_controller) &&
        globals_exact(e->saved_manager,e->saved_camera)));
}
BOOL SudekiMpUnbindRemoteCharacterSkillUi(void) {
    Entry *e=&persistent_skill_ui;
    if(!e->caster) return TRUE;
    if(!boundary() || e->remote_skill_ui_acquired || e->remote_skill_input_acquired ||
        e->remote_state_ui_acquired || !SudekiMpRemoteCharacterSkillUiHealthy() ||
        ((uint8_t *)e->skill)[0x6c]) { SetLastError(ERROR_BUSY); return FALSE; }
    memset(e,0,sizeof(*e)); persistent_skill_task=NULL;
    return TRUE;
}
static BOOL ui_context_exact(const Entry *e) {
    return e==&persistent_skill_ui ? globals_exact(e->saved_manager,e->saved_camera):
        globals_exact(e->identity.manager,e->identity.camera);
}
BOOL SudekiMpBindSpiritInstanceCaster(const SudekiMpSpiritInstance *instance,
    void *actor,uint8_t actor_type,uint64_t session,SudekiMpSpiritCasterWitness witness) {
    Entry *e=find(instance);
    uint8_t *a=actor,*s;
    unsigned int i;
    if(!e || !boundary() || !quiescent(e) || update_fault || e->caster || !session || !witness ||
        !witness(actor,session) || !memory(a,0x134,FALSE) ||
        (actor_type!=0x23 && actor_type!=1 && actor_type!=5 && actor_type!=0xe)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    for(i=0;i<MAX_INSTANCES;++i) if(entries[i].identity.generation && entries[i].caster==actor) {
        SetLastError(ERROR_ALREADY_EXISTS); return FALSE;
    }
    s=*(void **)(a+0x130);
    if(!memory(s,0x134,FALSE) || *(void **)(s+0x10)!=a) return FALSE;
    e->caster=actor; e->state_component=s; e->caster_session=session;
    e->caster_type=actor_type;
    e->caster_witness=witness; e->caster_vtable=*(void **)a; e->state_vtable=*(void **)s;
    return TRUE;
}

BOOL SudekiMpDestroySpiritInstance(SudekiMpSpiritInstance *instance) {
    Entry *e=find(instance);
    unsigned int i;
    if(!e || !boundary() || (cast_gate_manager &&
        (!cast_gate_exact() || (cast_gate_manager[CAST_GATE_OFFSET]&CAST_BUSY_BIT)!=cast_gate_union()))) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if(shared_ssp_enabled && !e->destroying) {
        uint32_t bits;
        if(!ssp_read(e->identity.manager,&bits) || bits!=e->shared_ssp_last) {
            SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
    }
    if(!e->destroying) {
        if(!originals(&e->saved_manager,&e->saved_camera) ||
            e->saved_manager==e->identity.manager || e->saved_camera==e->identity.camera ||
            !quiescent(e)) { SetLastError(ERROR_BUSY); return FALSE; }
        if(!named_retire(e)) { SetLastError(ERROR_BUSY); return FALSE; }
        e->destroying=TRUE;
    }
    if(!object_exact(e->saved_manager,MANAGER_SIZE,MANAGER_VTABLE) ||
        !object_exact(e->saved_camera,CAMERA_SIZE,CAMERA_VTABLE)) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    ++operation_depth;
    /* A completed destructor must never run twice when restoration was held
     * up by a foreign singleton owner. Retry only the outstanding restoration. */
    if(e->camera_restore_pending) {
        if(!address(instance_image+CAMERA_GLOBAL,NULL)) goto retained;
        *(void **)(instance_image+CAMERA_GLOBAL)=e->saved_camera;
        e->camera_restore_pending=FALSE;
    }
    if(e->manager_restore_pending) {
        if(!address(instance_image+MANAGER_GLOBAL,NULL)) goto retained;
        *(void **)(instance_image+MANAGER_GLOBAL)=e->saved_manager;
        e->manager_restore_pending=FALSE;
    }
    if(!globals_exact(e->saved_manager,e->saved_camera)) goto retained;
    /* Native destructors unregister UpdateNodes, detach intrusive references,
     * and release native resources. Do not free these objects as byte buffers.
     * The two outer allocations belong to us; f610's four souls do not. */
    if(e->camera_constructed) {
        *(void **)(instance_image+CAMERA_GLOBAL)=e->identity.camera;
        camera_delete(e->identity.camera,0);
        e->camera_constructed=FALSE;
        e->camera_restore_pending=TRUE;
        if(!address(instance_image+CAMERA_GLOBAL,NULL)) goto retained;
        *(void **)(instance_image+CAMERA_GLOBAL)=e->saved_camera;
        e->camera_restore_pending=FALSE;
    }
    for(i=4;i>0;--i) {
        if(!e->souls[i-1]) continue;
        if(!globals_exact(e->saved_manager,e->saved_camera)) goto retained;
        soul_delete(e->souls[i-1],1);
        e->souls[i-1]=NULL;
        *(void **)((uint8_t *)e->identity.manager+0xb0+4*(i-1))=NULL;
    }
    if(e->manager_constructed) {
        if(!globals_exact(e->saved_manager,e->saved_camera)) goto retained;
        *(void **)(instance_image+MANAGER_GLOBAL)=e->identity.manager;
        manager_delete(e->identity.manager,0);
        e->manager_constructed=FALSE;
        e->manager_restore_pending=TRUE;
        if(!address(instance_image+MANAGER_GLOBAL,NULL)) goto retained;
        *(void **)(instance_image+MANAGER_GLOBAL)=e->saved_manager;
        e->manager_restore_pending=FALSE;
    }
    if(!globals_exact(e->saved_manager,e->saved_camera)) goto retained;
    HeapFree(GetProcessHeap(),0,e->identity.camera);
    HeapFree(GetProcessHeap(),0,e->identity.manager);
    memset(e,0,sizeof(*e)); memset(instance,0,sizeof(*instance));
    --operation_depth;
    return TRUE;
retained:
    --operation_depth;
    SetLastError(ERROR_INVALID_DATA);
    return FALSE;
}

BOOL SudekiMpResetSpiritInstanceAbi(void) {
    unsigned int i;
    if(!instance_image) return TRUE;
    if(operation_depth || scope_depth || update_depth || (owner_thread && owner_thread!=GetCurrentThreadId())) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    for(i=0;i<MAX_INSTANCES;++i) if(entries[i].identity.generation) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if(cast_gate_manager && (!cast_gate_exact() || neutral_cast_busy ||
        (cast_gate_manager[CAST_GATE_OFFSET]&CAST_BUSY_BIT))) { SetLastError(ERROR_BUSY); return FALSE; }
    if(!SudekiMpUnbindRemoteCharacterSkillUi() || !SudekiMpUninstallSpiritInstanceUpdates()) return FALSE;
    cast_gate_manager=NULL; neutral_cast_busy=0;
    shared_ssp_enabled=FALSE;
    instance_image=NULL; idle_witness=NULL; input_owner_witness=NULL; owner_thread=0;
    primary_manager=primary_camera=NULL;
    manager_ctor=camera_ctor=NULL; manager_init=NULL; camera_init=NULL;
    manager_delete=camera_delete=soul_delete=NULL;
    native_period_setter=NULL;
    native_participant_lock=NULL; native_participant_unlock=NULL; native_ready_tail=NULL;
    ui_owner=ui_root=NULL; ui_root_vtable=NULL;
    memset(ui_trampolines,0,sizeof(ui_trampolines)); memset(ui_resumes,0,sizeof(ui_resumes));
    memset(skill_ui_resumes,0,sizeof(skill_ui_resumes)); native_ui_recompute=NULL;
    memset(skill_input_resumes,0,sizeof(skill_input_resumes));
    memset(state_ui_trampolines,0,sizeof(state_ui_trampolines));
    state_ui_acquire_resume=state_ui_release_resume=NULL;
    update_fault=FALSE;
    named_manager=NULL; named_add=NULL; named_remove=NULL; named_generation=0;
    memset(named_originals,0,sizeof(named_originals));
    memset(named_original_names,0,sizeof(named_original_names));
    /* Generations do not reset: an old external handle cannot match a new one. */
    return TRUE;
}

uint32_t SudekiMpEnterSpiritInstance(const SudekiMpSpiritInstance *instance) {
    Entry *e=instance ? find(instance):NULL;
    void *manager,*camera,*expected_manager,*expected_camera;
    unsigned int n=scope_depth;
    if(!instance_image || !owner_thread || GetCurrentThreadId()!=owner_thread ||
        operation_depth || n==16 || next_scope_cookie==UINT_MAX ||
        (instance && (update_fault || !e || e->destroying || !e->manager_initialized || !e->camera_constructed))) {
        SetLastError(ERROR_BUSY); return 0;
    }
    expected_manager=n ? scopes[n-1].manager:primary_manager;
    expected_camera=n ? scopes[n-1].camera:primary_camera;
    manager=e ? e->identity.manager:primary_manager;
    camera=e ? e->identity.camera:primary_camera;
    if(!globals_exact(expected_manager,expected_camera) ||
        !object_exact(expected_manager,MANAGER_SIZE,MANAGER_VTABLE) ||
        !object_exact(expected_camera,CAMERA_SIZE,CAMERA_VTABLE) ||
        !object_exact(manager,MANAGER_SIZE,MANAGER_VTABLE) ||
        !object_exact(camera,CAMERA_SIZE,CAMERA_VTABLE)) {
        SetLastError(ERROR_INVALID_DATA); return 0;
    }
    if(!shared_ssp_transition_ready(e ? e->identity.generation:0) ||
        !named_transition_ready(e ? e->identity.generation:0) ||
        !cast_gate_transition(e ? e->identity.generation:0,FALSE)) return 0;
    shared_ssp_transition_commit(e ? e->identity.generation:0);
    named_transition_commit(e ? e->identity.generation:0);
    scopes[n].saved_manager=expected_manager; scopes[n].saved_camera=expected_camera;
    scopes[n].manager=manager; scopes[n].camera=camera;
    scopes[n].generation=e ? e->identity.generation:0;
    scopes[n].cookie=++next_scope_cookie;
    ++scope_depth;
    *(void **)(instance_image+MANAGER_GLOBAL)=manager;
    *(void **)(instance_image+CAMERA_GLOBAL)=camera;
    return scopes[n].cookie;
}

BOOL SudekiMpLeaveSpiritInstance(uint32_t cookie) {
    unsigned int n=scope_depth ? scope_depth-1:0;
    SudekiMpSpiritInstance identity;
    if(!instance_image || !cookie || !scope_depth || scopes[n].cookie!=cookie ||
        owner_thread!=GetCurrentThreadId() || operation_depth) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    identity=(SudekiMpSpiritInstance){scopes[n].manager,scopes[n].camera,scopes[n].generation};
    if((identity.generation && !find(&identity)) ||
        !globals_exact(scopes[n].manager,scopes[n].camera) ||
        !object_exact(scopes[n].manager,MANAGER_SIZE,MANAGER_VTABLE) ||
        !object_exact(scopes[n].camera,CAMERA_SIZE,CAMERA_VTABLE) ||
        !object_exact(scopes[n].saved_manager,MANAGER_SIZE,MANAGER_VTABLE) ||
        !object_exact(scopes[n].saved_camera,CAMERA_SIZE,CAMERA_VTABLE)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    if(!shared_ssp_transition_ready(n ? scopes[n-1].generation:0) ||
        !named_transition_ready(n ? scopes[n-1].generation:0) ||
        !cast_gate_transition(n ? scopes[n-1].generation:0,n==0)) return FALSE;
    shared_ssp_transition_commit(n ? scopes[n-1].generation:0);
    named_transition_commit(n ? scopes[n-1].generation:0);
    *(void **)(instance_image+MANAGER_GLOBAL)=scopes[n].saved_manager;
    *(void **)(instance_image+CAMERA_GLOBAL)=scopes[n].saved_camera;
    memset(&scopes[n],0,sizeof(scopes[n])); --scope_depth;
    return TRUE;
}

/* Retail f900's verified continuation expects its two saved registers and
 * EDI=this. Reuse its native clock request, start call and state10 transition
 * verbatim. Do not forge participant states or edit their intrusive pointers. */
static void __attribute__((naked,noinline,thiscall)) ready_manager(
    void *object __attribute__((unused)),float delta __attribute__((unused))) {
    __asm__ volatile("pushl %esi\n\tpushl %edi\n\tmovl %ecx,%edi\n\tjmp *_native_ready_tail\n\t");
}
static BOOL strike_matches_caster(uint8_t actor_type,uint32_t strike) {
    /* Native10c20 resolves the script caster from strike98 through the same
     * character mapping as f1a0. Participant60 is NOT that identity: it can
     * remain the party leader while Elco casts. Retain its native TPtr. */
    static const uint8_t caster_types[4]={0x23,1,5,0xe};
    return strike<8 && caster_types[strike/2]==actor_type;
}
static void caster_manager_update(Entry *e,float delta) {
    uint8_t *m=e->identity.manager,*a=e->caster,*s=e->state_component;
    uint32_t strike=*(uint32_t *)(m+0x98);
    if(*(uint32_t *)(m+0x5c)!=2) { original_updates[2](m,delta); return; }
    if(!caster_exact(e) || !strike_matches_caster(e->caster_type,strike) ||
        !e->caster_lock_owned) {
        update_fault=TRUE; return;
    }
    instance_image[0x408d34]=0; /* Same first operation as native f900. */
    if(s[0x131]!=2 && s[0x131]!=3 && !a[0x2b]) return;
    ready_manager(m,delta);
}

static Entry *scoped_participant_owner(void) {
    SudekiMpSpiritInstance identity;
    if(!scope_depth || !scopes[scope_depth-1].generation) return NULL;
    identity=(SudekiMpSpiritInstance){scopes[scope_depth-1].manager,
        scopes[scope_depth-1].camera,scopes[scope_depth-1].generation};
    return find(&identity);
}
/* Hook BEFORE native ADD and UI recomputation, not afterward: the latter can
 * close the local Q menu. Only the six-byte absolute MOV is displaced, so the
 * passthrough trampoline contains no unrelocated relative call. No counter is
 * reset, banked, or written by this adapter, including on validation failure.
 * The skipped acquisition is an asynchronous obligation until matching native
 * completion. Unrelated and local-caster work takes the original path. */
static BOOL __attribute__((noinline,used)) remote_ui_transition(BOOL acquire) {
    Entry *e=scoped_participant_owner();
    if(!e || !e->remote_ui) return FALSE;
    if(update_fault || GetCurrentThreadId()!=owner_thread || !caster_exact(e) ||
        !globals_exact(e->identity.manager,e->identity.camera) || !remote_ui_exact() ||
        e->remote_ui_acquired==acquire) {
        update_fault=TRUE;
        /* Fail closed without touching an unknown UI owner or inventing a
         * second acquisition/release. Keep the outstanding lease for drain. */
        return TRUE;
    }
    e->remote_ui_acquired=acquire;
    return TRUE;
}
static void __attribute__((naked,noinline)) remote_ui_acquire_bridge(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tpushl $1\n\tcall _remote_ui_transition\n\taddl $4,%esp\n\t"
        "testl %eax,%eax\n\tjz 1f\n\tpopal\n\tpopfl\n\t"
        "movl _instance_image,%esi\n\tmovl 0x3c2f88(%esi),%esi\n\tmovl $1,%ebx\n\t"
        "jmp *_ui_resumes\n\t1: popal\n\tpopfl\n\tjmp *_ui_trampolines\n\t");
}
static void __attribute__((naked,noinline)) remote_ui_release_bridge(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tpushl $0\n\tcall _remote_ui_transition\n\taddl $4,%esp\n\t"
        "testl %eax,%eax\n\tjz 1f\n\tpopal\n\tpopfl\n\t"
        "movl _instance_image,%esi\n\tmovl 0x3c2f88(%esi),%esi\n\t"
        "jmp *_ui_resumes+4\n\t1: popal\n\tpopfl\n\tjmp *_ui_trampolines+4\n\t");
}
/* CSkill has a separate balanced UI acquisition. Full Use and native cleanup
 * must already be routed by the coordinator. Never attribute an unscoped or
 * foreign CSkill to whichever Spirit happens to be active. */
static BOOL __attribute__((noinline,used)) remote_skill_ui_transition(void *skill,void *ui,int delta) {
    Entry *e=scoped_participant_owner();
    unsigned int i;
    if(!scope_depth && persistent_skill_ui.caster && persistent_skill_ui.skill==skill)
        e=&persistent_skill_ui;
    if(!e || !e->remote_skill_ui) {
        for(i=0;i<MAX_INSTANCES;++i) if(entries[i].identity.generation &&
                entries[i].remote_skill_ui && entries[i].skill==skill) {
            update_fault=TRUE; return TRUE; /* Known remote owner escaped its scope. */
        }
        return FALSE;
    }
    if(update_fault || GetCurrentThreadId()!=owner_thread || !skill_ui_exact(e,skill) ||
        !ui_context_exact(e) || !remote_ui_exact() ||
        ui!=ui_owner || (delta!=1 && delta!=-1) || e->remote_skill_ui_acquired==(delta==1)) {
        update_fault=TRUE; return TRUE;
    }
    e->remote_skill_ui_acquired=delta==1;
    return TRUE;
}
/* Each displaced range is exactly ADD/INC plus CALL. Do NOT execute its
 * generic trampoline: the copied E8 would have an unrelocated displacement.
 * Explicitly replay those two verified instructions for local/unrelated work,
 * including the ESI native ABI, then resume after the original call. Remote
 * work skips both before any UI mutation/side effect, preserving registers. */
static void __attribute__((naked,noinline)) remote_skill_ui_acquire_bridge(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tpushl $1\n\tpushl %esi\n\tpushl %ebx\n\t"
        "call _remote_skill_ui_transition\n\taddl $12,%esp\n\ttestl %eax,%eax\n\tjz 1f\n\t"
        "popal\n\tpopfl\n\tjmp *_skill_ui_resumes\n\t"
        "1: popal\n\tpopfl\n\tincl 0x54(%esi)\n\tcall *_native_ui_recompute\n\t"
        "jmp *_skill_ui_resumes\n\t");
}
static void __attribute__((naked,noinline)) remote_skill_ui_release_bridge(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tpushl %ebp\n\tpushl %esi\n\tpushl %edi\n\t"
        "call _remote_skill_ui_transition\n\taddl $12,%esp\n\ttestl %eax,%eax\n\tjz 1f\n\t"
        "popal\n\tpopfl\n\tjmp *_skill_ui_resumes+4\n\t"
        "1: popal\n\tpopfl\n\taddl %ebp,0x54(%esi)\n\tcall *_native_ui_recompute\n\t"
        "jmp *_skill_ui_resumes+4\n\t");
}
/* CSkill::Use clears controller+1d0 bit2 before UI acquisition. The native
 * event handler's this is controller+2c: its +1a4 test is this same bit and
 * discards action events before frame-update isolation can help. Cleanup ORs
 * it back. Neither operation belongs to the host's local controller when the
 * caster is remote. Suppress both before mutation, retaining a balanced lease
 * so remote cleanup cannot accidentally unlock a concurrent local action. */
static BOOL __attribute__((noinline,used)) remote_skill_input_transition(void *skill,void *controller,BOOL acquire) {
    Entry *e=scoped_participant_owner();
    unsigned int i;
    if(!scope_depth && persistent_skill_ui.caster && persistent_skill_ui.skill==skill)
        e=&persistent_skill_ui;
    if(!e || !e->remote_skill_input) {
        for(i=0;i<MAX_INSTANCES;++i) if(entries[i].identity.generation &&
                entries[i].remote_skill_input && entries[i].skill==skill) {
            update_fault=TRUE; return TRUE;
        }
        return FALSE;
    }
    if(update_fault || GetCurrentThreadId()!=owner_thread || !skill_ui_exact(e,skill) ||
        !ui_context_exact(e) || !skill_input_exact(e,controller) ||
        e->remote_skill_input_acquired==acquire) {
        update_fault=TRUE; return TRUE;
    }
    e->remote_skill_input_acquired=acquire;
    return TRUE;
}
static void __attribute__((naked,noinline)) remote_skill_input_acquire_bridge(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tpushl $1\n\tpushl %eax\n\tpushl %ebx\n\t"
        "call _remote_skill_input_transition\n\taddl $12,%esp\n\ttestl %eax,%eax\n\tjz 1f\n\t"
        "popal\n\tpopfl\n\tjmp *_skill_input_resumes\n\t"
        "1: popal\n\tpopfl\n\tandl $-3,0x1d0(%eax)\n\tjmp *_skill_input_resumes\n\t");
}
static void __attribute__((naked,noinline)) remote_skill_input_release_bridge(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tpushl $0\n\tpushl %eax\n\tpushl %edi\n\t"
        "call _remote_skill_input_transition\n\taddl $12,%esp\n\ttestl %eax,%eax\n\tjz 1f\n\t"
        "popal\n\tpopfl\n\tjmp *_skill_input_resumes+4\n\t"
        "1: popal\n\tpopfl\n\torl $2,0x1d0(%eax)\n\tjmp *_skill_input_resumes+4\n\t");
}
/* Script-driven CState locks acquire UI independently of CSkill::Use and the
 * Spirit participant loop. The live Cybernetica root reached this exact pair.
 * Native character locking has ALREADY executed: suppress only the remote
 * caster's UI acquisition and +133 bit8. Its native unlock then sees bit8=0
 * and cannot decrement a local menu/cast lock. Do not patch global counters.
 * Keep a separate obligation through the positively observed native unlock. */
static BOOL __attribute__((noinline,used)) remote_state_ui_transition(void *state,BOOL acquire) {
    Entry *e=scoped_participant_owner();
    unsigned int i;
    if(!scope_depth && persistent_skill_ui.caster && persistent_skill_ui.state_component==state) {
        BOOL attributed=persistent_skill_task && persistent_skill_task(
            persistent_skill_ui.caster,persistent_skill_ui.caster_session);
        if(attributed) e=&persistent_skill_ui;
        else if(persistent_skill_ui.remote_state_ui_acquired) {
            update_fault=TRUE; return TRUE; /* Retain an escaped cleanup obligation. */
        }
    }
    if(!e || !e->remote_ui || e->state_component!=state) {
        for(i=0;i<MAX_INSTANCES;++i) if(entries[i].identity.generation &&
            entries[i].state_component==state && entries[i].remote_state_ui_acquired) {
            update_fault=TRUE; return TRUE; /* A retained remote lock escaped its owner. */
        }
        return FALSE;
    }
    if(update_fault || GetCurrentThreadId()!=owner_thread || !caster_exact(e) ||
        !ui_context_exact(e) || !remote_ui_exact() ||
        (((uint8_t *)state)[0x133]&8) ||
        (acquire && (e->remote_state_ui_acquired ||
            ((uint8_t *)state)[0x131]<1 || ((uint8_t *)state)[0x131]>3)) ||
        (!acquire && ((uint8_t *)state)[0x131]!=0 && ((uint8_t *)state)[0x131]!=4)) {
        update_fault=TRUE; return TRUE;
    }
    /* A ui=0 lock (including our Spirit startup) did not acquire this lease.
     * Its native unlock is still valid, but must not create a UI decrement. */
    e->remote_state_ui_acquired=acquire;
    return TRUE;
}
static void __attribute__((naked,noinline)) remote_state_ui_acquire_bridge(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tpushl $1\n\tpushl %edi\n\t"
        "call _remote_state_ui_transition\n\taddl $8,%esp\n\ttestl %eax,%eax\n\tjz 1f\n\t"
        "popal\n\tpopfl\n\tjmp *_state_ui_acquire_resume\n\t"
        "1: popal\n\tpopfl\n\tjmp *_state_ui_trampolines\n\t");
}
static void __attribute__((naked,noinline)) remote_state_ui_release_bridge(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tpushl $0\n\tpushl %edi\n\t"
        "call _remote_state_ui_transition\n\taddl $8,%esp\n\ttestl %eax,%eax\n\tjz 1f\n\t"
        "popal\n\tpopfl\n\tjmp *_state_ui_release_resume\n\t"
        "1: popal\n\tpopfl\n\tjmp *_state_ui_trampolines+4\n\t");
}
static uint32_t __attribute__((naked,noinline,regparm(3))) call_participant_lock(
    void *component __attribute__((unused)),uint32_t ui __attribute__((unused)),
    uint32_t mode __attribute__((unused))) {
    __asm__ volatile("pushl %edi\n\tmovl %eax,%edi\n\tpushl %ecx\n\tpushl %edx\n\t"
        "call *_native_participant_lock\n\tpopl %edi\n\tret\n\t");
}
static uint32_t __attribute__((noinline,used)) participant_lock(void *s,uint32_t ui,uint32_t mode) {
    Entry *e=scoped_participant_owner();
    uint32_t result;
    if(!e && scope_depth && scopes[scope_depth-1].generation) {
        update_fault=TRUE; return 0;
    }
    if(!e || !e->caster) return call_participant_lock(s,ui,mode);
    if(update_fault || GetCurrentThreadId()!=owner_thread || !caster_exact(e) ||
        !globals_exact(e->identity.manager,e->identity.camera) || ui!=1 || mode!=2) {
        update_fault=TRUE; return 0;
    }
    if(s!=e->state_component) return 0;
    if(e->caster_lock_owned) { update_fault=TRUE; return 0; }
    if(e->remote_ui && !remote_ui_exact()) { update_fault=TRUE; return 0; }
    /* ui=0 also prevents native +133 bit8 from being acquired. The existing
     * native unlock therefore cannot decrement an unrelated local UI lock. */
    result=call_participant_lock(s,e->remote_ui ? 0:ui,mode);
    e->caster_lock_owned=(result & 0xff)!=0;
    return result;
}
static void __attribute__((naked,noinline)) participant_lock_bridge(void) {
    __asm__ volatile("pushl %ecx\n\tpushl %edx\n\tpushl 16(%esp)\n\tpushl 16(%esp)\n\t"
        "pushl %edi\n\tcall _participant_lock\n\taddl $12,%esp\n\tpopl %edx\n\tpopl %ecx\n\tret $8\n\t");
}
static uint32_t __attribute__((regparm(1),stdcall)) participant_unlock(void *s,uint32_t mode) {
    Entry *e=scoped_participant_owner();
    uint32_t result;
    if(!e && scope_depth && scopes[scope_depth-1].generation) {
        update_fault=TRUE; return 0;
    }
    if(!e || !e->caster) return native_participant_unlock(s,mode);
    if(update_fault || GetCurrentThreadId()!=owner_thread || !caster_exact(e) ||
        !globals_exact(e->identity.manager,e->identity.camera) || mode!=2) {
        update_fault=TRUE; return 0;
    }
    if(s!=e->state_component || !e->caster_lock_owned) return 0;
    result=native_participant_unlock(s,mode);
    if((result & 0xff) || ((uint8_t *)s)[0x131]==0 || ((uint8_t *)s)[0x131]==4)
        e->caster_lock_owned=FALSE;
    return result;
}

/* The callbacks are thiscall Update(float), including CSpiritCam's exact
 * ECX-to-stack thunk. CSoul instances share this function with other native
 * models, so match the constructor-owned object, not merely the class vtable. */
static void update_instance(void *object,float delta,unsigned int kind) {
    Entry *owner=NULL;
    unsigned int i,j;
    uint32_t cookie;
    DWORD error=GetLastError(), result_error;
    for(i=0;i<MAX_INSTANCES;++i) {
        Entry *e=&entries[i];
        if(!e->identity.generation) continue;
        if(kind==0 && e->identity.camera==object) owner=e;
        if(kind==1) for(j=0;j<4;++j) if(e->souls[j]==object) owner=e;
        if(kind==2 && e->identity.manager==object) owner=e;
    }
    if(!owner && !scope_depth && !cast_gate_manager) {
        original_updates[kind](object,delta);
        return;
    }
    /* Failure never routes an owned tick through another caster. Retain the
     * objects/scopes and make the failure observable; do not force cleanup. */
    if(update_fault || GetCurrentThreadId()!=owner_thread ||
        (owner && (owner->destroying || !owner->manager_initialized ||
            !owner->camera_constructed ||
            !object_exact(object,kind==2 ? MANAGER_SIZE:(kind ? SOUL_SIZE:CAMERA_SIZE),
                kind==2 ? MANAGER_VTABLE:(kind ? SOUL_VTABLE:CAMERA_VTABLE))))) {
        update_fault=TRUE; return;
    }
    cookie=SudekiMpEnterSpiritInstance(owner ? &owner->identity:NULL);
    if(!cookie) { update_fault=TRUE; return; }
    ++update_depth;
    SetLastError(error);
    if(kind==2 && owner && owner->caster) caster_manager_update(owner,delta);
    else original_updates[kind](object,delta);
    result_error=GetLastError();
    --update_depth;
    if(owner) {
        uint32_t *ticks=kind==2 ? &owner->manager_ticks:(kind ? &owner->soul_ticks:&owner->camera_ticks);
        if(*ticks!=UINT_MAX) ++*ticks;
    }
    if(!SudekiMpLeaveSpiritInstance(cookie)) update_fault=TRUE;
    SetLastError(result_error);
}
static void __attribute__((thiscall)) camera_update(void *object,float delta) {
    update_instance(object,delta,0);
}
static void __attribute__((thiscall)) soul_update(void *object,float delta) {
    update_instance(object,delta,1);
}
static void __attribute__((thiscall)) manager_update(void *object,float delta) {
    /* Retail f900 ignores delta, but still consumes its stack argument (ret4). */
    update_instance(object,delta,2);
}

BOOL SudekiMpInstallSpiritInstanceUpdates(void) {
    NativeUpdate replacements[3]={camera_update,soul_update,manager_update};
    const uint32_t ui_sites[2]={0x100d1,0x10fe6};
    void *ui_bridges[2]={remote_ui_acquire_bridge,remote_ui_release_bridge};
    unsigned int i;
    if(!boundary() || update_hooks[0].installed || update_hooks[1].installed || update_hooks[2].installed ||
        participant_hooks[0].installed || participant_hooks[1].installed ||
        ui_hooks[0].installed || ui_hooks[1].installed ||
        skill_ui_hooks[0].installed || skill_ui_hooks[1].installed ||
        state_ui_hooks[0].installed || state_ui_hooks[1].installed ||
        skill_input_hooks[0].installed || skill_input_hooks[1].installed || !image_exact(instance_image)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    for(i=0;i<3;++i) if(!address(instance_image+update_slots[i],instance_image+update_rvas[i])) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    for(i=0;i<3;++i) original_updates[i]=(NativeUpdate)(instance_image+update_rvas[i]);
    for(i=0;i<3;++i) if(!SudekiMpInstallPointerHook(&update_hooks[i],
            (void **)(instance_image+update_slots[i]),original_updates[i],replacements[i])) {
        DWORD error=GetLastError();
        if(!SudekiMpUninstallSpiritInstanceUpdates()) return FALSE;
        SetLastError(error); return FALSE;
    }
    if(!SudekiMpInstallRelativeCallHook(&participant_hooks[0],instance_image+0xfcd6,
            native_participant_lock,participant_lock_bridge) ||
        !SudekiMpInstallRelativeCallHook(&participant_hooks[1],instance_image+0x10f36,
            native_participant_unlock,participant_unlock)) {
        DWORD error=GetLastError();
        if(!SudekiMpUninstallSpiritInstanceUpdates()) return FALSE;
        SetLastError(error); return FALSE;
    }
    ui_resumes[0]=instance_image+0x100e4; ui_resumes[1]=instance_image+0x10ff4;
    for(i=0;i<2;++i) {
        uint8_t expected[6]={0x8b,0x35,0,0,0,0};
        void *slot=instance_image+UI_GLOBAL;
        memcpy(expected+2,&slot,4);
        if(!SudekiMpInstallInlineHook(&ui_hooks[i],instance_image+ui_sites[i],expected,6,ui_bridges[i])) {
            DWORD error=GetLastError();
            if(!SudekiMpUninstallSpiritInstanceUpdates()) return FALSE;
            SetLastError(error); return FALSE;
        }
        ui_trampolines[i]=ui_hooks[i].trampoline;
    }
    native_ui_recompute=instance_image+0x9e560;
    skill_ui_resumes[0]=instance_image+0xb494c; skill_ui_resumes[1]=instance_image+0xb4f56;
    for(i=0;i<2;++i) {
        uint32_t site=i ? 0xb4f4e:0xb4944;
        uint8_t expected[8]={0,0x46,0x54,0xe8,0,0,0,0};
        int32_t relative=(int32_t)(0x9e560-site-8);
        expected[0]=i ? 0x01:0xff; if(i) expected[1]=0x6e;
        memcpy(expected+4,&relative,4);
        if(!SudekiMpInstallInlineHook(&skill_ui_hooks[i],instance_image+site,expected,8,
                i ? remote_skill_ui_release_bridge:remote_skill_ui_acquire_bridge)) {
            DWORD error=GetLastError();
            if(!SudekiMpUninstallSpiritInstanceUpdates()) return FALSE;
            SetLastError(error); return FALSE;
        }
    }
    skill_input_resumes[0]=instance_image+0xb4843; skill_input_resumes[1]=instance_image+0xb4e98;
    for(i=0;i<2;++i) {
        uint8_t expected[7]={0x83,0xa0,0xd0,1,0,0,0xfd};
        if(i) { expected[1]=0x88; expected[6]=2; }
        if(!SudekiMpInstallInlineHook(&skill_input_hooks[i],instance_image+(i ? 0xb4e91:0xb483c),expected,7,
                i ? remote_skill_input_release_bridge:remote_skill_input_acquire_bridge)) {
            DWORD error=GetLastError();
            if(!SudekiMpUninstallSpiritInstanceUpdates()) return FALSE;
            SetLastError(error); return FALSE;
        }
    }
    state_ui_acquire_resume=instance_image+0xe45c8;
    state_ui_release_resume=instance_image+0xe46e6;
    for(i=0;i<2;++i) {
        uint8_t expected[7]={0xa1,0,0,0,0};
        void *slot=instance_image+UI_ROOT_GLOBAL;
        memcpy(expected+1,&slot,4);
        if(i) memcpy(expected,"\xf6\x87\x33\x01\x00\x00\x08",7);
        if(!SudekiMpInstallInlineHook(&state_ui_hooks[i],instance_image+(i ? 0xe46c6:0xe45aa),
            expected,i ? 7:5,i ? remote_state_ui_release_bridge:remote_state_ui_acquire_bridge)) {
            DWORD error=GetLastError();
            if(!SudekiMpUninstallSpiritInstanceUpdates()) return FALSE;
            SetLastError(error); return FALSE;
        }
        state_ui_trampolines[i]=state_ui_hooks[i].trampoline;
    }
    return TRUE;
}

BOOL SudekiMpUninstallSpiritInstanceUpdates(void) {
    BOOL restored=TRUE;
    unsigned int i;
    if(persistent_skill_ui.caster || update_depth || scope_depth || operation_depth ||
        (owner_thread && owner_thread!=GetCurrentThreadId())) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    /* Objects must be gone first: registered native nodes can still call us. */
    for(i=0;i<MAX_INSTANCES;++i) if(entries[i].identity.generation) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    for(i=2;i>0;--i) if(!SudekiMpRestoreInlineHook(&state_ui_hooks[i-1])) restored=FALSE;
    for(i=2;i>0;--i) if(!SudekiMpRestoreInlineHook(&skill_input_hooks[i-1])) restored=FALSE;
    for(i=2;i>0;--i) if(!SudekiMpRestoreInlineHook(&skill_ui_hooks[i-1])) restored=FALSE;
    for(i=2;i>0;--i) if(!SudekiMpRestoreInlineHook(&ui_hooks[i-1])) restored=FALSE;
    for(i=2;i>0;--i) if(!SudekiMpRestoreRelativeCallHook(&participant_hooks[i-1])) restored=FALSE;
    for(i=3;i>0;--i) if(!SudekiMpRestorePointerHook(&update_hooks[i-1])) restored=FALSE;
    if(!restored) return FALSE;
    memset(original_updates,0,sizeof(original_updates));
    return TRUE;
}

BOOL SudekiMpObserveSpiritInstanceUpdates(const SudekiMpSpiritInstance *instance,
    uint32_t *camera_ticks,uint32_t *soul_ticks) {
    Entry *e=find(instance);
    if(!e || !camera_ticks || !soul_ticks || update_fault ||
        GetCurrentThreadId()!=owner_thread || e->destroying ||
        !object_exact(e->identity.manager,MANAGER_SIZE,MANAGER_VTABLE) ||
        !object_exact(e->identity.camera,CAMERA_SIZE,CAMERA_VTABLE)) return FALSE;
    *camera_ticks=e->camera_ticks; *soul_ticks=e->soul_ticks;
    return TRUE;
}

__attribute__((naked,noinline)) static void __attribute__((regparm(2)))
schedule_node(RawPeriodSetter function __attribute__((unused)),void *object __attribute__((unused))) {
    __asm__ volatile("movl %eax,%ecx\n\tmovl %edx,%eax\n\tpushl $0\n\tcall *%ecx\n\tret\n\t");
}
BOOL SudekiMpScheduleSpiritInstanceManager(const SudekiMpSpiritInstance *instance) {
    Entry *e=find(instance);
    void *manager,*camera;
    if(!e || !boundary() || !quiescent(e) || e->destroying || update_fault ||
        !update_hooks[2].installed || !originals(&manager,&camera) ||
        manager!=primary_manager || camera!=primary_camera ||
        *(int16_t *)((uint8_t *)e->identity.manager+0x20)!=-1 ||
        ((uint8_t *)e->identity.manager)[0x23]) return FALSE;
    ++operation_depth;
    schedule_node(native_period_setter,e->identity.manager);
    --operation_depth;
    if(!globals_exact(manager,camera) ||
        *(int16_t *)((uint8_t *)e->identity.manager+0x20)!=0) {
        update_fault=TRUE; SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpObserveSpiritInstanceManagerTicks(const SudekiMpSpiritInstance *instance,uint32_t *ticks) {
    Entry *e=find(instance);
    uint32_t c,s;
    if(!ticks || !SudekiMpObserveSpiritInstanceUpdates(instance,&c,&s)) return FALSE;
    *ticks=e->manager_ticks;
    return TRUE;
}
BOOL SudekiMpObserveSpiritInstance(const SudekiMpSpiritInstance *instance,
    SudekiMpSpiritInstanceState *state) {
    Entry *e=find(instance);
    uint8_t *m,*c;
    if(!state || !e || !instance_image || !owner_thread ||
        owner_thread!=GetCurrentThreadId() || update_fault || e->destroying ||
        !e->manager_initialized || !e->camera_constructed ||
        (e->caster && !caster_exact(e))) return FALSE;
    m=e->identity.manager; c=e->identity.camera;
    if(!object_exact(m,MANAGER_SIZE,MANAGER_VTABLE) ||
        !object_exact(c,CAMERA_SIZE,CAMERA_VTABLE)) return FALSE;
    *state=(SudekiMpSpiritInstanceState){*(uint32_t *)(m+0x5c),
        *(uint32_t *)(m+0x98),*(uint32_t *)(c+0x1a0)!=0,quiescent(e)};
    return TRUE;
}
BOOL SudekiMpResolveSpiritInstanceCaster(void *actor,uint64_t session,
    SudekiMpSpiritInstance *instance) {
    unsigned int i;
    if(!instance || !actor || !session || !instance_image || !owner_thread ||
        GetCurrentThreadId()!=owner_thread || update_fault) return FALSE;
    for(i=0;i<MAX_INSTANCES;++i) {
        Entry *e=&entries[i];
        if(e->identity.generation && e->caster==actor && e->caster_session==session) {
            if(e->destroying || !caster_exact(e) ||
                !object_exact(e->identity.manager,MANAGER_SIZE,MANAGER_VTABLE) ||
                !object_exact(e->identity.camera,CAMERA_SIZE,CAMERA_VTABLE)) return FALSE;
            *instance=e->identity;
            return TRUE;
        }
    }
    return FALSE;
}
#if defined(SUDEKIMP_CAST_INSTANCE_PROBE)
BOOL SudekiMpScheduleIdleSpiritInstanceProbe(const SudekiMpSpiritInstance *instance) {
    Entry *e=find(instance);
    unsigned int i;
    void *nodes[5];
    if(!e || !boundary() || !quiescent(e) || e->destroying || update_fault ||
        !update_hooks[0].installed || !update_hooks[1].installed) return FALSE;
    nodes[0]=e->identity.camera;
    for(i=0;i<4;++i) nodes[i+1]=e->souls[i];
    for(i=0;i<5;++i) if(*(int16_t *)((uint8_t *)nodes[i]+0x20)!=-1 ||
        ((uint8_t *)nodes[i])[0x23]) return FALSE;
    ++operation_depth;
    for(i=0;i<5;++i) {
        schedule_node(native_period_setter,nodes[i]);
        if(*(int16_t *)((uint8_t *)nodes[i]+0x20)!=0) {
            --operation_depth; SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
    }
    --operation_depth;
    return TRUE;
}
#endif
