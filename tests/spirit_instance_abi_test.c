#include <stdio.h>
#include <windows.h>
static unsigned int observed_memory_queries;
static SIZE_T WINAPI counted_virtual_query(LPCVOID p,PMEMORY_BASIC_INFORMATION m,SIZE_T n) {
    ++observed_memory_queries;
    return VirtualQuery(p,m,n);
}
#define VirtualQuery counted_virtual_query
#define SUDEKIMP_CAST_INSTANCE_PROBE 1
#include "../src/engine/spirit_instance_abi.c"
#undef VirtualQuery

static int failures;
#define CHECK(x) do { if(!(x)) { printf("FAIL %d: %s error=%lu\n",__LINE__,#x,(unsigned long)GetLastError()); ++failures; } } while(0)
static uint8_t *image;
static uint8_t original_manager[MANAGER_SIZE], original_camera[CAMERA_SIZE];
static uint8_t actor_manager[CAST_GATE_OFFSET+1], replacement_actor_manager[CAST_GATE_OFFSET+1];
static unsigned int created_managers,created_cameras,deleted_managers,deleted_cameras,deleted_souls;
static BOOL idle=TRUE, foreign_camera_delete, foreign_manager_delete;
static unsigned int updated_cameras,updated_souls,updated_managers;
static BOOL nested_updates;
static BOOL corrupt_update_slot;
static SudekiMpSpiritInstance *test_instances;
static uint8_t unrelated_soul[SOUL_SIZE];
static uint8_t caster_actors[2][0x134], caster_states[2][0x134];
static uint8_t caster_skills[2][0x78];
static BOOL starting_task_known;
static void **starting_task_handle;
BOOL SudekiMpLanCastContextActorDrained(void *actor,uint64_t session) {
    for(unsigned int i=0;i<MAX_INSTANCES;++i)
        if(actor && session && entries[i].caster==actor &&
            entries[i].caster_session==session)
            return !starting_task_known;
    return FALSE;
}
BOOL SudekiMpLanCastContextStartingSkillTask(void *actor,uint64_t session,
    void *skill,void **handle,void **thread) {
    if(!starting_task_known || actor!=entries[1].caster || session!=entries[1].caster_session ||
        skill!=entries[1].skill || !starting_task_handle || !*starting_task_handle) return FALSE;
    *handle=starting_task_handle; *thread=*starting_task_handle; return TRUE;
}
static uint8_t fake_ui[0xe4],fake_ui_root[0x178],foreign_ui[0xe4];
static uint8_t fake_controller[0x260],foreign_controller[0x260];
static BOOL local_witness_valid=TRUE;
static volatile uint32_t input_bridge_eax __attribute__((used)),input_bridge_edi __attribute__((used));
static volatile float input_bridge_fpu __attribute__((used));
static unsigned int expected_lock_ui=1;
static volatile uint32_t bridge_esi __attribute__((used)),bridge_ebx __attribute__((used)),bridge_ebp __attribute__((used));
static volatile uint32_t ui_recompute_calls __attribute__((used));
static void __attribute__((naked,noinline)) fake_ui_recompute(void) {
    __asm__ volatile("incl _ui_recompute_calls\n\tret\n\t");
}
static void __attribute__((naked,noinline,regparm(2))) invoke_skill_ui_bridge(
    void *skill __attribute__((unused)),BOOL acquire __attribute__((unused))) {
    __asm__ volatile("pushl %esi\n\tpushl %ebx\n\tpushl %edi\n\tpushl %ebp\n\t"
        "movl %eax,%ebx\n\tmovl %eax,%edi\n\tmovl $-1,%ebp\n\t"
        "movl _image,%esi\n\tmovl 0x3c2f88(%esi),%esi\n\t"
        "testl %edx,%edx\n\tjz 1f\n\tcall _remote_skill_ui_acquire_bridge\n\tjmp 2f\n\t"
        "1: call _remote_skill_ui_release_bridge\n\t2: popl %ebp\n\tpopl %edi\n\tpopl %ebx\n\tpopl %esi\n\tret\n\t");
}
static void __attribute__((naked,noinline)) fake_ui_resume(void) {
    __asm__ volatile("movl %esi,_bridge_esi\n\tmovl %ebx,_bridge_ebx\n\tmovl %ebp,_bridge_ebp\n\tret\n\t");
}
static void __attribute__((naked,noinline)) fake_input_resume(void) {
    __asm__ volatile("movl %eax,_input_bridge_eax\n\tmovl %edi,_input_bridge_edi\n\t"
        "movl %ebx,_bridge_ebx\n\tfstps _input_bridge_fpu\n\tret\n\t");
}
static void __attribute__((naked,noinline,regparm(2))) invoke_skill_input_bridge(
    void *skill __attribute__((unused)),BOOL acquire __attribute__((unused))) {
    __asm__ volatile("pushl %ebx\n\tpushl %edi\n\tmovl %eax,%ebx\n\tmovl %eax,%edi\n\t"
        "movl _image,%eax\n\tmovl 0x408da4(%eax),%eax\n\tfld1\n\t"
        "testl %edx,%edx\n\tjz 1f\n\tcall _remote_skill_input_acquire_bridge\n\tjmp 2f\n\t"
        "1: call _remote_skill_input_release_bridge\n\t2: popl %edi\n\tpopl %ebx\n\tret\n\t");
}
static void __attribute__((naked,noinline,regparm(2))) invoke_state_ui_bridge(
    void *state __attribute__((unused)),BOOL acquire __attribute__((unused))) {
    __asm__ volatile("pushl %edi\n\tmovl %eax,%edi\n\tfld1\n\t"
        "testl %edx,%edx\n\tjz 1f\n\tcall _remote_state_ui_acquire_bridge\n\tjmp 2f\n\t"
        "1: call _remote_state_ui_release_bridge\n\t2: popl %edi\n\tret\n\t");
}
static void __attribute__((naked,noinline)) fake_ui_acquire_native(void) {
    __asm__ volatile("movl _image,%esi\n\tmovl 0x3c2f88(%esi),%esi\n\tmovl $1,%ebx\n\t"
        "incl 0x54(%esi)\n\tjmp _fake_ui_resume\n\t");
}
static void __attribute__((naked,noinline)) fake_ui_release_native(void) {
    __asm__ volatile("movl _image,%esi\n\tmovl 0x3c2f88(%esi),%esi\n\t"
        "decl 0x54(%esi)\n\tjmp _fake_ui_resume\n\t");
}
static void __attribute__((naked,noinline,regparm(1))) invoke_ui_bridge(BOOL acquire __attribute__((unused))) {
    __asm__ volatile("pushl %esi\n\tpushl %ebx\n\tpushl %ebp\n\tmovl $77,%ebx\n\tmovl $-1,%ebp\n\t"
        "testl %eax,%eax\n\tjz 1f\n\tcall _remote_ui_acquire_bridge\n\tjmp 2f\n\t"
        "1: call _remote_ui_release_bridge\n\t2: popl %ebp\n\tpopl %ebx\n\tpopl %esi\n\tret\n\t");
}
static unsigned int participant_locks, participant_unlocks, ready_calls;
static BOOL caster_witness(void *actor,uint64_t session) {
    return session==17 && (actor==caster_actors[0] || actor==caster_actors[1]);
}
static BOOL local_input_witness(void *actor,uint64_t session) {
    return local_witness_valid && session==17 && actor==caster_actors[0];
}
static uint32_t __attribute__((noinline,used)) fake_lock_record(void *state,uint32_t ui,uint32_t mode) {
    CHECK(ui==expected_lock_ui && mode==2);
    CHECK(state==caster_states[0] || state==caster_states[1]);
    ++participant_locks; ((uint8_t *)state)[0x131]=1;
    ((uint8_t *)state)[0x133]&=~8;
    if(ui) { ++*(uint32_t *)(fake_ui+0x54); ((uint8_t *)state)[0x133]|=8; }
    return 1;
}
static void * __attribute__((naked,noinline)) fake_lock(void) {
    __asm__ volatile("pushl 8(%esp)\n\tpushl 8(%esp)\n\tpushl %edi\n\t"
        "call _fake_lock_record\n\taddl $12,%esp\n\tret $8\n\t");
}
static uint32_t __attribute__((naked,noinline,regparm(3))) invoke_lock(
    void *s __attribute__((unused)),uint32_t ui __attribute__((unused)),uint32_t mode __attribute__((unused))) {
    __asm__ volatile("pushl %edi\n\tmovl %eax,%edi\n\tpushl %ecx\n\tpushl %edx\n\t"
        "call _participant_lock_bridge\n\tpopl %edi\n\tret\n\t");
}
static uint32_t __attribute__((regparm(1),stdcall)) fake_unlock(void *state,uint32_t mode) {
    CHECK(mode==2 && (state==caster_states[0] || state==caster_states[1]));
    ++participant_unlocks; ((uint8_t *)state)[0x131]=4;
    if(((uint8_t *)state)[0x133]&8) --*(uint32_t *)(fake_ui+0x54);
    return 1;
}
static void __attribute__((noinline,used)) fake_ready_record(void *manager) {
    CHECK(*(void **)(image+MANAGER_GLOBAL)==manager);
    ++ready_calls; *(uint32_t *)((uint8_t *)manager+0x5c)=10;
}
static void __attribute__((naked,noinline)) fake_ready_tail(void) {
    __asm__ volatile("pushl %edi\n\tcall _fake_ready_record\n\taddl $4,%esp\n\t"
        "popl %edi\n\tpopl %esi\n\tret $4\n\t");
}
static BOOL witness(void) { return idle; }
static void __attribute__((noinline,used)) *fake_manager_construct(void *object) {
    CHECK(globals_exact(original_manager,original_camera));
    *(void **)object=image+MANAGER_VTABLE;
    *(int16_t *)((uint8_t *)object+0x20)=-1;
    *(void **)(image+MANAGER_GLOBAL)=object;
    ++created_managers; return object;
}
static void __attribute__((noinline,used)) *fake_camera_construct(void *object) {
    CHECK(*(void **)(image+CAMERA_GLOBAL)==original_camera);
    *(void **)object=image+CAMERA_VTABLE;
    *(int16_t *)((uint8_t *)object+0x20)=-1;
    *(void **)(image+CAMERA_GLOBAL)=object;
    ++created_cameras; return object;
}
#define CTOR_STUB(name,target) \
static void * __attribute__((naked,noinline)) name(void) { __asm__ volatile( \
    "pushl %esi\n\tcall _" #target "\n\taddl $4,%esp\n\tret\n\t"); }
CTOR_STUB(fake_manager,fake_manager_construct)
CTOR_STUB(fake_camera,fake_camera_construct)
static void __attribute__((stdcall)) fake_manager_init(void *object) {
    unsigned int i;
    CHECK(*(void **)(image+MANAGER_GLOBAL)==object);
    for(i=0;i<4;++i) {
        void *s=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,SOUL_SIZE);
        CHECK(s!=NULL); if(!s) return;
        *(void **)s=image+SOUL_VTABLE;
        *(int16_t *)((uint8_t *)s+0x20)=-1;
        *(void **)((uint8_t *)object+0xb0+4*i)=s;
    }
}
static void __attribute__((regparm(1))) fake_camera_init(void *object) {
    CHECK(*(void **)(image+CAMERA_GLOBAL)==object);
}
static void __attribute__((naked,noinline)) fake_period(void) {
    __asm__ volatile("cmpl $0,4(%esp)\n\tjne 1f\n\tmovw $0,0x20(%eax)\n\t1: ret $4\n\t");
}
static void * __attribute__((thiscall)) fake_camera_delete(void *object,unsigned int flags) {
    CHECK(flags==0 && *(void **)object==image+CAMERA_VTABLE);
    CHECK(*(void **)(image+CAMERA_GLOBAL)==object);
    *(void **)object=NULL;
    *(void **)(image+CAMERA_GLOBAL)=foreign_camera_delete ? (void *)0x1234:NULL;
    ++deleted_cameras; return object;
}
static void * __attribute__((thiscall)) fake_manager_delete(void *object,unsigned int flags) {
    unsigned int i;
    CHECK(flags==0 && *(void **)object==image+MANAGER_VTABLE);
    CHECK(*(void **)(image+MANAGER_GLOBAL)==object);
    for(i=0;i<4;++i) CHECK(!*(void **)((uint8_t *)object+0xb0+i*4));
    *(void **)object=NULL;
    *(void **)(image+MANAGER_GLOBAL)=foreign_manager_delete ? (void *)0x5678:NULL;
    ++deleted_managers; return object;
}
static void * __attribute__((thiscall)) fake_soul_delete(void *object,unsigned int flags) {
    CHECK(flags==1 && *(void **)object==image+SOUL_VTABLE);
    HeapFree(GetProcessHeap(),0,object); ++deleted_souls; return NULL;
}
static void __attribute__((thiscall)) fake_camera_update(void *object,float delta) {
    CHECK(delta==0.125f);
    ++updated_cameras;
    CHECK(*(void **)(image+CAMERA_GLOBAL)==object);
    CHECK(!SudekiMpUninstallSpiritInstanceUpdates());
    if(nested_updates && object==test_instances[0].camera) {
        NativeUpdate soul=(NativeUpdate)*(void **)(image+SOUL_VTABLE+4);
        soul(entries[1].souls[0],delta);
        CHECK(globals_exact(test_instances[0].manager,test_instances[0].camera));
        soul(unrelated_soul,delta);
        CHECK(globals_exact(test_instances[0].manager,test_instances[0].camera));
    }
    if(corrupt_update_slot) *(void **)(image+CAMERA_GLOBAL)=(void *)0xdeadbeef;
}
static void __attribute__((thiscall)) fake_soul_update(void *object,float delta) {
    unsigned int i,j;
    CHECK(delta==0.125f);
    ++updated_souls;
    if(object==unrelated_soul) {
        CHECK(globals_exact(original_manager,original_camera)); return;
    }
    for(i=0;i<4;++i) for(j=0;j<4;++j) if(entries[i].souls[j]==object) {
        CHECK(globals_exact(entries[i].identity.manager,entries[i].identity.camera)); return;
    }
    CHECK(FALSE);
}
static void __attribute__((thiscall)) fake_manager_update(void *object,float delta) {
    CHECK(delta==0.125f);
    CHECK(*(void **)(image+MANAGER_GLOBAL)==object);
    ++updated_managers;
    if(object==test_instances[0].manager) {
        camera_update(test_instances[1].camera,delta);
        CHECK(globals_exact(test_instances[0].manager,test_instances[0].camera));
        manager_update(original_manager,delta);
        CHECK(globals_exact(test_instances[0].manager,test_instances[0].camera));
    }
}
static void put_call(uint32_t site,uint32_t target) {
    int32_t delta=(int32_t)(target-site-5);
    image[site]=0xe8; memcpy(image+site+1,&delta,4);
}
static void fixture(void) {
    image[0xf5e0]=0x51; image[0xf5e1]=0xa1;
    *(void **)(image+0xf5e2)=image+MANAGER_GLOBAL;
    memcpy(image+0xf5ea,"\xd9\x80\xa8\x00\x00\x00",6);
    memcpy(image+0x10a79,"\xd9\x82\xa8\x00\x00\x00",6);
    memcpy(image+0x1002d,"\xd9\x86\xa8\x00\x00\x00",6);
    memcpy(image+0x1008c,"\xd9\x96\xa8\x00\x00\x00",6);
    memcpy(image+0x111c3,"\xd9\x99\xa8\x00\x00\x00",6);
    memcpy(image+0x111e6,"\xd9\x99\xa8\x00\x00\x00",6);
    {
        const uint8_t add[]={0xbb,1,0,0,0,0x01,0x5e,0x54};
        const uint8_t sub[]={0x01,0x6e,0x54};
        const uint8_t lock[]={0x80,0xa7,0x33,1,0,0,0xf7,0x38,0x5c,0x24,0x10,0x74,0x1e};
        const uint8_t unlock[]={0xf6,0x87,0x33,1,0,0,8};
        image[0x9c528]=0xc7; image[0x9c529]=0x45; image[0x9c52a]=0;
        *(void **)(image+0x9c52b)=image+UI_VTABLE;
        image[0x9c5dd]=0x89; image[0x9c5de]=0x2d;
        *(void **)(image+0x9c5df)=image+UI_GLOBAL;
        image[0x100d1]=image[0x10fe6]=0x8b;
        image[0x100d2]=image[0x10fe7]=0x35;
        *(void **)(image+0x100d3)=*(void **)(image+0x10fe8)=image+UI_GLOBAL;
        memcpy(image+0x100d7,add,sizeof(add)); memcpy(image+0x10fec,sub,sizeof(sub));
        put_call(0x100df,0x9e560); put_call(0x10fef,0x9e560);
        {
            const uint8_t load[]={0x8b,0xb1,0x74,1,0,0}, inc[]={0xff,0x46,0x54};
            const uint8_t after_acquire[]={0x8b,0x45,8,0x8b,0x4c,0x83,0x3c};
            const uint8_t after_release[]={0x8b,0x47,0x10,0x8b,0x40,0x60};
            memcpy(image+0xb492e,load,sizeof(load)); memcpy(image+0xb4f38,load,sizeof(load));
            memcpy(image+0xb4944,inc,sizeof(inc)); memcpy(image+0xb4f4e,sub,sizeof(sub));
            put_call(0xb4947,0x9e560); put_call(0xb4f51,0x9e560);
            memcpy(image+0xb494c,after_acquire,sizeof(after_acquire));
            memcpy(image+0xb4f56,after_release,sizeof(after_release));
            const uint8_t clear[]={0x83,0xa0,0xd0,1,0,0,0xfd};
            const uint8_t set[]={0x83,0x88,0xd0,1,0,0,2};
            const uint8_t reader[]={0x8b,0x8f,0xa4,1,0,0};
            const uint8_t test[]={0xf6,0xc1,2,0x0f,0x84,0xfe,0,0,0};
            image[0xb4835]=image[0xb4e8c]=0xa1;
            *(void **)(image+0xb4836)=*(void **)(image+0xb4e8d)=image+CONTROLLER_GLOBAL;
            image[0xb483a]=0xd9; image[0xb483b]=0xe8;
            memcpy(image+0xb483c,clear,7); memcpy(image+0xb4e91,set,7);
            image[0xb4843]=image[0xb4e98]=0xa1;
            *(void **)(image+0xb4844)=*(void **)(image+0xb4e99)=image+0x409d78;
            memcpy(image+0x278d9,reader,sizeof(reader)); memcpy(image+0x279ce,test,sizeof(test));
            *(void **)(image+0x2c9f84)=image+0x277b0;
            *(void **)(image+CONTROLLER_GLOBAL)=fake_controller;
            *(void **)fake_controller=*(void **)foreign_controller=image+CONTROLLER_VTABLE;
            *(void **)(fake_controller+0x248)=*(void **)(foreign_controller+0x248)=caster_actors[0];
        }
        memcpy(image+0xe459d,lock,sizeof(lock)); memcpy(image+0xe46c6,unlock,sizeof(unlock));
        image[0xe45aa]=image[0xe46cd]=0xa1;
        *(void **)(image+0xe45ab)=*(void **)(image+0xe46ce)=image+UI_ROOT_GLOBAL;
        put_call(0xe45bc,0x9e560); put_call(0xe46e1,0x9e560);
        memcpy(image+0xe45c1,"\x80\x8f\x33\x01\x00\x00\x08",7);
        memcpy(image+0xe45c8,"\x5e\xb0\x01\x5b\x59\xc2\x08\x00",8);
        memcpy(image+0xe46e6,"\xb0\x01\x5f\x5e\xc2\x04\x00",7);
        *(void **)fake_ui=*(void **)foreign_ui=image+UI_VTABLE;
        *(void **)(image+UI_GLOBAL)=fake_ui;
        *(void **)fake_ui_root=image+0x1004;
        *(void **)(fake_ui_root+0x174)=fake_ui;
        *(void **)(image+UI_ROOT_GLOBAL)=fake_ui_root;
    }
    const uint8_t unlocked_reader[]={0x84,0x90,0xac,0,0,0};
    const uint8_t unlocked_writer[]={0xc6,0x80,0xac,0,0,0,0xff};
    const uint8_t mc[]={0xd9,0xe8,0x33,0xc0,0x83,0xc9,0xff,0xd9,0x5e,0x10};
    const uint8_t cc[]={0xd9,0xe8,0x66,0xc7,0x46,0x22,1,0,0xd9,0x5e,0x10};
    const uint8_t mi[]={0x83,0xec,8,0x53,0x55,0x8b,0x6c,0x24,0x14};
    const uint8_t ci[]={0x83,0xec,8,0x56,0x8b,0xf0,0x80,0x7e,0x22,0};
    const uint8_t d[]={0x56,0x8b,0xf1};
    const uint8_t period[]={0x83,0xec,8,0xd9,0x44,0x24,0x0c,0x56};
    const uint8_t cu[]={0x89,0x4c,0x24,0x04,0xe9,0x07,0,0,0};
    const uint8_t su[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x81,0xec,0xb4,0,0,0,
        0x53,0x8b,0xd9,0x80,0x7b,0x48,0};
    {
        const uint8_t test[]={0xf6,0x81,0x18,0x0a,0,0,8};
        const uint8_t set[]={0x80,0x88,0x18,0x0a,0,0,8};
        const uint8_t clear[]={0x80,0xa0,0x18,0x0a,0,0,0xf7};
        const uint32_t readers[]={0x10a1c,0xb4bcf};
        const uint32_t setters[]={0x10156,0xb4b68};
        unsigned int k;
        image[0x2f8d8]=0x89; image[0x2f8d9]=0x2d;
        *(void **)(image+0x2f8da)=image+CAST_GATE_GLOBAL;
        image[0x2f8de]=0xc7; image[0x2f8df]=0x45; image[0x2f8e0]=0;
        *(void **)(image+0x2f8e1)=image+CAST_GATE_VTABLE;
        for(k=0;k<2;++k) {
            image[readers[k]]=0x8b; image[readers[k]+1]=0x0d;
            *(void **)(image+readers[k]+2)=image+CAST_GATE_GLOBAL;
            memcpy(image+readers[k]+6,test,sizeof(test));
            image[setters[k]]=0xa1;
            *(void **)(image+setters[k]+1)=image+CAST_GATE_GLOBAL;
            memcpy(image+setters[k]+5,set,sizeof(set));
        }
        image[0x11114]=image[0xb47f3]=0xa1;
        *(void **)(image+0x11115)=*(void **)(image+0xb47f4)=image+CAST_GATE_GLOBAL;
        memcpy(image+0x11125,clear,sizeof(clear)); memcpy(image+0xb47f8,clear,sizeof(clear));
        *(void **)actor_manager=*(void **)replacement_actor_manager=image+CAST_GATE_VTABLE;
        *(void **)(image+CAST_GATE_GLOBAL)=actor_manager;
    }
    memcpy(image+0x9b856,unlocked_reader,sizeof(unlocked_reader));
    memcpy(image+0x113bc,unlocked_writer,sizeof(unlocked_writer));
    memcpy(image+MANAGER_CTOR,mc,sizeof(mc)); memcpy(image+CAMERA_CTOR,cc,sizeof(cc));
    memcpy(image+MANAGER_INIT,mi,sizeof(mi)); memcpy(image+CAMERA_INIT,ci,sizeof(ci));
    memcpy(image+MANAGER_DELETE,d,sizeof(d)); memcpy(image+CAMERA_DELETE,d,sizeof(d));
    memcpy(image+SOUL_DELETE,d,sizeof(d));
    put_call(0x78d0d,MANAGER_CTOR); put_call(0x78d18,CAMERA_CTOR);
    put_call(0x79c53,MANAGER_INIT); put_call(0x79c5e,CAMERA_INIT);
    put_call(MANAGER_DELETE+5,0x792e0); put_call(CAMERA_DELETE+3,0x119e0);
    put_call(SOUL_DELETE+5,0xef60);
    *(void **)(image+MANAGER_CTOR+0x40)=image+MANAGER_GLOBAL;
    *(void **)(image+CAMERA_CTOR+0x97)=image+CAMERA_GLOBAL;
    *(void **)(image+MANAGER_VTABLE)=image+MANAGER_DELETE;
    *(void **)(image+CAMERA_VTABLE)=image+CAMERA_DELETE;
    *(void **)(image+SOUL_VTABLE)=image+SOUL_DELETE;
    *(void **)(image+update_slots[0])=image+update_rvas[0];
    *(void **)(image+update_slots[1])=image+update_rvas[1];
    *(void **)(image+update_slots[2])=image+update_rvas[2];
    memcpy(image+update_rvas[0],cu,sizeof(cu));
    memcpy(image+update_rvas[1],su,sizeof(su));
    memcpy(image+0x11be0,"\x55\x8b\xec\x83\xe4\xf0\x81\xec\xf4\x00\x00\x00"
        "\x53\x8b\x5d\x08\x8b\x83\xa0\x01\x00\x00\x83\xe8\x02\x56\x57"
        "\x0f\x85\x1e\x02\x00\x00",33);
    memcpy(image+0x12ae03,"\x56\x57\x89\x5c\x24\x24\x0f\x84\x39\x02\x00\x00",12);
    memcpy(image+0x11e1f,"\x5f\x5e\x5b\x8b\xe5\x5d\xc2\x04\x00",9);
    memcpy(image+0x12b048,"\x5f\x5e\x5b\x8b\xe5\x5d\xc2\x04\x00",9);
    {
        const uint8_t mt[]={0,0x56,0x57,0x8b,0xf9,0x74,7,0xc6,5};
        const uint8_t ret4[]={0xc2,4,0};
        image[0xf900]=0x80; image[0xf901]=0x3d;
        *(void **)(image+0xf902)=image+0x408d34;
        memcpy(image+0xf906,mt,sizeof(mt));
        *(void **)(image+0xf90f)=image+0x408d34;
        memcpy(image+0xf921,ret4,3); memcpy(image+0xf977,ret4,3);
        put_call(0xf969,0x10c20);
    }
    memcpy(image+0x1061d0,period,sizeof(period));
    put_call(0x106266,0x134280); put_call(0x106272,0x134330); put_call(0x106281,0x1342e0);
    put_call(0x79df7,0x1061d0);
    put_call(0xfcd6,0xe4460); put_call(0x10f36,0xe45d0);
    {
        const uint8_t lock[]={0x51,0x80,0xbf,0x30,1,0,0,0xbf};
        const uint8_t unlock[]={0x8a,0x4c,0x24,4,0x56,0x57,0x8b,0xf8};
        const uint8_t start[]={0xc7,0x40,0x24,3,0,0,0,0x8b,0xc7};
        const uint8_t end[]={0xc7,0x47,0x5c,0x0a,0,0,0,0x5f,0x5e,0xc2,4,0};
        memcpy(image+0xe4460,lock,sizeof(lock)); memcpy(image+0xe45d0,unlock,sizeof(unlock));
        memcpy(image+0xf914,readiness_body,sizeof(readiness_body));
        image[0xf95b]=0xa1; *(void **)(image+0xf95c)=image+0x408da0;
        memcpy(image+0xf960,start,sizeof(start)); memcpy(image+0xf96e,end,sizeof(end));
    }
    *(void **)original_manager=image+MANAGER_VTABLE;
    *(void **)original_camera=image+CAMERA_VTABLE;
    *(void **)(image+MANAGER_GLOBAL)=original_manager;
    *(void **)(image+CAMERA_GLOBAL)=original_camera;
}
static void setup(void) {
    CHECK(SudekiMpInitializeSpiritInstanceAbi((HMODULE)image,witness));
    CHECK(SudekiMpInstallSpiritInstanceUpdates());
    CHECK(!SudekiMpInstallSpiritInstanceUpdates());
    original_updates[0]=fake_camera_update; original_updates[1]=fake_soul_update;
    original_updates[2]=fake_manager_update;
    native_period_setter=fake_period;
    manager_ctor=fake_manager; camera_ctor=fake_camera;
    manager_init=fake_manager_init; camera_init=fake_camera_init;
    manager_delete=fake_manager_delete; camera_delete=fake_camera_delete; soul_delete=fake_soul_delete;
}
static DWORD WINAPI foreign_thread(void *argument) {
    SudekiMpSpiritInstance instance={0};
    (void)argument;
    CHECK(!SudekiMpCreateSpiritInstance(&instance));
    CHECK(!SudekiMpResetSpiritInstanceAbi());
    CHECK(!SudekiMpEnableSpiritInstanceCastGates());
    CHECK(!SudekiMpEnterSpiritInstance(NULL));
    return 0;
}
static uint8_t named_test_manager[0x60],named_scene_manager[0x44],named_scene[0x80];
static uint8_t named_base[6][NAMED_CAMERA_SIZE];
static unsigned int named_add_calls,named_delete_calls,named_fail_at;
static BOOL named_remove_blocked;
static BOOL named_block_after_delete;
static BOOL __attribute__((thiscall)) fake_named_add(void *manager,const char *name,const char *config) {
    unsigned int i;
    uint8_t *p;
    CHECK(manager==named_test_manager && !strcmp(config,"default"));
    ++named_add_calls;
    if(named_add_calls==named_fail_at) return FALSE;
    for(i=0;i<NAMED_SLOTS && *named_slot(i);++i) {}
    CHECK(i<NAMED_SLOTS);
    if(i==NAMED_SLOTS) return FALSE;
    p=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,NAMED_CAMERA_SIZE);
    CHECK(p!=NULL); if(!p) return FALSE;
    *(void **)p=image+NAMED_CAMERA_VTABLE;
    *(void **)(p+8)=image+0x2cce6c;
    *(void **)(p+0x34)=p+0x70; /* Unique fixture render-state identity. */
    strcpy((char *)p+NAMED_CAMERA_NAME,name);
    *named_slot(i)=p;
    return TRUE;
}
static void __attribute__((thiscall)) fake_named_remove(void *manager,const char *name) {
    unsigned int slot=0;
    void *camera=named_lookup(name,&slot);
    CHECK(manager==named_test_manager && camera);
    if(named_remove_blocked || !camera) return;
    HeapFree(GetProcessHeap(),0,camera); *named_slot(slot)=NULL; ++named_delete_calls;
    if(named_block_after_delete) named_remove_blocked=TRUE;
}
static void named_fixture(void) {
    unsigned int i;
    const char *names[]={"default","SpeechCamera","SpiritCam","TSACam","SkillCam","InitCam"};
    const struct { uint32_t rva; const char *bytes; size_t count; } code[]={
        {0x36c10,"\x83\xec\x14\x53\x55\x8b\x6c\x24\x20",9},
        {0x36cd4,"\x68\x08\x01\x00\x00",5}, {0x36cf1,"\x89\x74\xbb\x24",4},
        {0x36d3b,"\x8b\x16\x8b\x42\x04\x8b\xce\xff\xd0",9},
        {0x36d80,"\xc6\x46\x60\x00",4},
        {0x36de0,"\x53\x55\x8b\x6c\x24\x0c\x56\x8b\xd9\x57\x33\xf6\x8d\x7b\x24",15},
        {0x36e39,"\x8b\x4c\xb3\x24\x8b\x11\x8b\x42\x08\xff\xd0",11},
        {0x36e58,"\xc7\x44\xb3\x24\x00\x00\x00\x00",8},
        {0x36ed0,"\x53\x8b\x5c\x24\x08\x55\x8b\xe9",8},
        {0x36ee5,"\x8d\x7d\x24\x8b\x07\x85\xc0\x74\x11\x83\xc0\x4c",12},
        {0x36eff,"\x46\x83\xc7\x04\x83\xfe\x0a\x72\xe0",9}
    };
    for(i=0;i<sizeof(code)/sizeof(code[0]);++i) memcpy(image+code[i].rva,code[i].bytes,code[i].count);
    put_call(0x36cd9,0x2484fa); put_call(0x36ce6,0xe7110); put_call(0x36d54,0x1061d0);
    put_call(0x36d72,0x249580); put_call(0x36d8b,0xe8360);
    put_call(0x36dfb,0x24ae0e); put_call(0x36ef3,0x24ae0e);
    memcpy(image+0x36fbc,"\x8b\x43\x20",3);
    memcpy(image+0x370d8,"\x8b\x4c\x24\x1c\x8b\x41\x40\x8b\x4a\x34\x89\x48\x7c",13);
    memcpy(image+0x370ea,"\x89\x53\x20",3);
    memcpy(image+0x370f0,"\x88\x8a\x05\x01\x00\x00",6);
    memcpy(image+0x374fa,"\x8b\x75\x20",3);
    memcpy(image+0xe7161,"\xc7\x45\x08",3); *(void **)(image+0xe7164)=image+0x2cce6c;
    memcpy(image+0xe7660,"\x55\x8b\xec\x83\xe4\xc0\xa1",7);
    *(void **)(image+0xe7667)=image+0x408da0;
    memcpy(image+0xe768d,"\x8b\x55\x08\xd9\x5c\x24\x54\x8d\x73\xf8",10);
    memcpy(image+0xe7948,"\xc2\x04\x00",3); *(void **)(image+0x2cce70)=image+0xe7660;
    memcpy(image+0x11823,"\xc7\x46\x30",3); *(void **)(image+0x11826)=image+0x2c5660;
    memcpy(image+0x1182a,"\xc7\x46\x44",3); *(void **)(image+0x1182d)=image+0x2c5674;
    memcpy(image+0x121a0,"\x56\x8b\xf1\x83\xbe\x98\x01\x00\x00\x00",10);
    memcpy(image+0x121ad,"\x8d\x7e\xbc",3); put_call(0x121b0,0x12060);
    memcpy(image+0x121bd,"\xb0\x01\x5e\xc3",4);
    memcpy(image+0x11ff0,"\x8b\x44\x24\x08\x56\x8b\xf1",7);
    memcpy(image+0x12000,"\x8d\x7e\xd0",3); put_call(0x1204c,0x12060);
    memcpy(image+0x12052,"\x5e\xc2\x08\x00",4);
    *(void **)(image+0x2c5678)=image+0x121a0; *(void **)(image+0x2c566c)=image+0x11ff0;
    memset(named_test_manager,0,sizeof(named_test_manager));
    *(void **)named_test_manager=image+NAMED_MANAGER_VTABLE;
    *(void **)(image+NAMED_MANAGER_GLOBAL)=named_test_manager;
    for(i=0;i<6;++i) {
        memset(named_base[i],0,NAMED_CAMERA_SIZE);
        *(void **)named_base[i]=image+NAMED_CAMERA_VTABLE;
        *(void **)(named_base[i]+8)=image+0x2cce6c;
        *(void **)(named_base[i]+0x34)=named_base[i]+0x70;
        strcpy((char *)named_base[i]+NAMED_CAMERA_NAME,names[i]);
        *(void **)(named_test_manager+0x24+4*i)=named_base[i];
    }
    *(void **)(named_test_manager+0x20)=named_base[0];
    *(void **)(image+0x408d58)=named_scene_manager;
    *(void **)named_scene_manager=image+0x2c66b8;
    *(void **)(named_scene_manager+0x40)=named_scene;
    *(void **)(named_scene+0x7c)=named_base[0]+0x70;
    named_add_calls=named_delete_calls=named_fail_at=0;
    named_remove_blocked=named_block_after_delete=FALSE;
}
static unsigned int selection_update_count;
static SudekiMpSpiritInstance selection_expected_scope;
static void __attribute__((thiscall)) fake_named_update(void *node,void *args) {
    SudekiMpSpiritInstance observed;
    CHECK(node && args==&selection_update_count);
    CHECK(SudekiMpObserveSpiritInstanceScope(&observed));
    CHECK(observed.generation==selection_expected_scope.generation);
    ++selection_update_count;
}
static unsigned char __attribute__((thiscall)) fake_camera_ready(void *member) {
    SudekiMpSpiritInstance observed;
    CHECK(member && update_depth && SudekiMpObserveSpiritInstanceScope(&observed));
    CHECK(observed.generation==selection_expected_scope.generation);
    Entry *e=generation_entry(observed.generation);
    CHECK(named_lookup("InitCam",NULL)==(e ? e->named_cameras[0]:named_originals[0]));
    ++selection_update_count;
    SetLastError(91);
    return 0x7b;
}
static void __attribute__((thiscall)) fake_camera_animation(void *member,void *source,uint32_t event) {
    CHECK(source==&selection_update_count && event==0x11223344);
    CHECK(fake_camera_ready(member)==0x7b);
}
static void selection_tests(void) {
    SudekiMpSpiritInstance pair[2]={{0}},scope={0};
    uint32_t a,b,n;
    unsigned int kind=99;
    void *saved,*render;
    const uint32_t sites[]={0x36fbc,0x370d8,0x370ea,0x370f0,0x374fa,
        0xe7161,0xe7164,0xe7660,0xe7667,0xe768d,0xe7948,0x2cce70,
        0x11823,0x11826,0x1182a,0x1182d,0x121a0,0x121ad,0x121b0,0x121bd,
        0x11ff0,0x12000,0x1204c,0x12052,0x2c5678,0x2c566c};
    setup(); named_fixture();
    for(unsigned int i=0;i<sizeof(sites)/sizeof(sites[0]);++i) {
        image[sites[i]]^=1; CHECK(!SudekiMpSpiritInstanceCameraSelectionAbiReady());
        image[sites[i]]^=1; CHECK(SudekiMpSpiritInstanceCameraSelectionAbiReady());
    }
    /* Native factory already has separate exact-image/idle-live coverage.
     * Bind the synthetic registry and use controlled constructor doubles. */
    named_manager=named_test_manager;
    named_originals[0]=named_base[5]; named_original_slots[0]=5;
    named_originals[1]=named_base[4]; named_original_slots[1]=4;
    for(unsigned int k=0;k<2;++k)
        memcpy(named_original_names[k],(uint8_t *)named_originals[k]+NAMED_CAMERA_NAME,NAMED_NAME_SIZE);
    named_add=fake_named_add; named_remove=fake_named_remove;
    for(unsigned int i=0;i<2;++i) {
        CHECK(SudekiMpCreateSpiritInstance(&pair[i]));
        *(void **)((uint8_t *)pair[i].camera+0x30)=image+0x2c5660;
        *(void **)((uint8_t *)pair[i].camera+0x44)=image+0x2c5674;
        CHECK(SudekiMpBindSpiritInstanceCaster(&pair[i],caster_actors[i],i ? 14:5,17,caster_witness));
        CHECK(SudekiMpEnableSpiritInstanceNamedCameras(&pair[i]));
    }
    CHECK(!SudekiMpEnableSpiritInstanceRemoteCameraSelection(&pair[1]));
    CHECK(SudekiMpInstallSpiritInstanceNamedCameraUpdates());
    CHECK(!SudekiMpInstallSpiritInstanceNamedCameraUpdates());
    CHECK(!SudekiMpEnableSpiritInstanceRemoteCameraSelection(&pair[1]));
    CHECK(SudekiMpEnableSpiritInstanceRemoteUi(&pair[1]));
    CHECK(SudekiMpEnableSpiritInstanceRemoteCameraSelection(&pair[1]));
    CHECK(!SudekiMpEnableSpiritInstanceRemoteCameraSelection(&pair[1]));
    CHECK(SudekiMpEnableSpiritInstanceCastGates());
    render=*(void **)(named_scene+0x7c);
    CHECK(SudekiMpObserveSpiritInstanceScope(&scope) && !scope.generation);
    CHECK(SudekiMpRouteSpiritInstanceRenderCamera(named_manager,"SkillCam",&kind)==0 && kind==99);
    a=SudekiMpEnterSpiritInstance(&pair[0]); CHECK(a);
    /* Simulate local native SetRenderCamera; its render view remains native. */
    *(void **)(named_test_manager+0x20)=entries[0].named_cameras[1];
    *(void **)(named_scene+0x7c)=*(void **)((uint8_t *)entries[0].named_cameras[1]+0x34);
    b=SudekiMpEnterSpiritInstance(&pair[1]); CHECK(b);
    CHECK(*(void **)(named_test_manager+0x20)==entries[1].named_cameras[0]);
    CHECK(SudekiMpRouteSpiritInstanceRenderCamera(named_manager,"SkillCam",&kind)==1 && kind==2);
    CHECK(*(void **)(named_test_manager+0x20)==entries[1].named_cameras[1]);
    CHECK(*(void **)(named_scene+0x7c)==*(void **)((uint8_t *)entries[0].named_cameras[1]+0x34));
    CHECK(SudekiMpRouteSpiritInstanceRenderCamera(named_manager,"",&kind)==1 && kind==2);
    CHECK(SudekiMpRouteSpiritInstanceRenderCamera(named_manager,NULL,&kind)==1 && kind==2);
    CHECK(SudekiMpRouteSpiritInstanceRenderCamera(original_manager,"InitCam",&kind)==-1 && kind==2);
    CHECK(SudekiMpRouteSpiritInstanceRenderCamera(named_manager,"SpiritCam",&kind)==-1 && kind==2);
    n=SudekiMpEnterSpiritInstance(NULL); CHECK(n);
    CHECK(*(void **)(named_test_manager+0x20)==entries[0].named_cameras[1]);
    /* A nested local/neutral request changes the real view. Leaving the
     * remote scope must preserve this latest view, not its old saved pointer. */
    *(void **)(named_test_manager+0x20)=named_base[0]; *(void **)(named_scene+0x7c)=render;
    CHECK(SudekiMpLeaveSpiritInstance(n));
    CHECK(*(void **)(named_test_manager+0x20)==entries[1].named_cameras[1]);
    CHECK(!SudekiMpLeaveSpiritInstance(a));
    CHECK(SudekiMpLeaveSpiritInstance(b));
    CHECK(*(void **)(named_test_manager+0x20)==named_base[0]);
    CHECK(SudekiMpLeaveSpiritInstance(a));
    named_update_original=fake_named_update; selection_update_count=0;
    a=SudekiMpEnterSpiritInstance(&pair[0]); CHECK(a);
    selection_expected_scope=pair[1];
    named_camera_update((uint8_t *)entries[1].named_cameras[1]+8,&selection_update_count);
    CHECK(!update_fault && selection_update_count==1);
    CHECK(SudekiMpObserveSpiritInstanceScope(&scope) && scope.generation==pair[0].generation);
    selection_expected_scope=(SudekiMpSpiritInstance){0};
    named_camera_update(named_base[0]+8,&selection_update_count);
    CHECK(!update_fault && selection_update_count==2);
    CHECK(SudekiMpLeaveSpiritInstance(a));
    spirit_camera_ready_original=fake_camera_ready;
    spirit_camera_animation_original=fake_camera_animation;
    /* Completion outside a task/update still configures the owning private
     * camera, not the retail shared pair. A nested other caster is restored. */
    selection_expected_scope=pair[1];
    CHECK(spirit_camera_ready((uint8_t *)pair[1].camera+0x44)==0x7b && GetLastError()==91);
    CHECK(!scope_depth && !named_generation && !selection_generation && selection_update_count==3);
    a=SudekiMpEnterSpiritInstance(&pair[0]); CHECK(a);
    spirit_camera_animation((uint8_t *)pair[1].camera+0x30,&selection_update_count,0x11223344);
    CHECK(!update_fault && selection_update_count==4 && named_generation==pair[0].generation);
    CHECK(SudekiMpObserveSpiritInstanceScope(&scope) && scope.generation==pair[0].generation);
    CHECK(SudekiMpLeaveSpiritInstance(a));
    *(void **)(original_camera+0x44)=image+0x2c5674;
    selection_expected_scope=(SudekiMpSpiritInstance){0};
    b=SudekiMpEnterSpiritInstance(&pair[1]); CHECK(b);
    CHECK(spirit_camera_ready(original_camera+0x44)==0x7b);
    CHECK(!update_fault && selection_update_count==5 && named_generation==pair[1].generation);
    CHECK(SudekiMpLeaveSpiritInstance(b));
    b=SudekiMpEnterSpiritInstance(&pair[1]); CHECK(b);
    saved=*(void **)(named_test_manager+0x20);
    *(void **)(named_test_manager+0x20)=entries[0].named_cameras[0];
    CHECK(!SudekiMpLeaveSpiritInstance(b) && scope_depth==1);
    CHECK(!SudekiMpDestroySpiritInstance(&pair[1]));
    *(void **)(named_test_manager+0x20)=saved; /* Fixture repair only. */
    *(void **)(named_scene+0x7c)=(void *)1;
    CHECK(!SudekiMpLeaveSpiritInstance(b) && scope_depth==1);
    *(void **)(named_scene+0x7c)=render;
    CHECK(SudekiMpRouteSpiritInstanceRenderCamera(named_manager,"default",&kind)==1 && kind==0);
    CHECK(*(void **)(named_test_manager+0x20)==entries[1].named_cameras[0]);
    CHECK(SudekiMpLeaveSpiritInstance(b));
    for(unsigned int i=2;i>0;--i) CHECK(SudekiMpDestroySpiritInstance(&pair[i-1]));
    /* Foreign callback must retain the adapter on teardown and allow retry. */
    *(void **)(image+0x2cce70)=(void *)1;
    *(void **)(image+0x2c5678)=(void *)1;
    *(void **)(image+0x2c566c)=(void *)1;
    CHECK(!SudekiMpResetSpiritInstanceAbi() && named_update_original && instance_image);
    *(void **)(image+0x2cce70)=named_camera_update;
    *(void **)(image+0x2c5678)=spirit_camera_ready;
    CHECK(!SudekiMpResetSpiritInstanceAbi() && spirit_camera_animation_original &&
        spirit_camera_ready_original && named_update_original && instance_image);
    CHECK(*(void **)(image+0x2cce70)==image+0xe7660 && *(void **)(image+0x2c5678)==image+0x121a0);
    CHECK(!SudekiMpInstallSpiritInstanceNamedCameraUpdates());
    *(void **)(image+0x2c566c)=spirit_camera_animation;
    CHECK(SudekiMpResetSpiritInstanceAbi());
    CHECK(*(void **)(image+0x2cce70)==image+0xe7660 && !named_update_original && !selection_scene);
    CHECK(*(void **)(image+0x2c5678)==image+0x121a0 && *(void **)(image+0x2c566c)==image+0x11ff0);
    CHECK(!spirit_camera_ready_original && !spirit_camera_animation_original);
}
static void named_tests(void) {
    SudekiMpSpiritInstance pair[2]={{0}},stale;
    unsigned int i,j;
    uint32_t a,b,n;
    void *observed,*saved;
    setup(); named_fixture();
    CHECK(SudekiMpSpiritInstanceNamedCameraAbiReady());
    {
        const uint32_t sites[]={0x36c10,0x36cd4,0x36cd9,0x36ce6,0x36cf1,0x36d3b,
            0x36d54,0x36d72,0x36d80,0x36d8b,0x36de0,0x36dfb,0x36e39,0x36e58,0x36ed0,0x36ee5,0x36ef3,0x36eff};
        for(i=0;i<sizeof(sites)/sizeof(sites[0]);++i) {
            image[sites[i]]^=1; CHECK(!SudekiMpSpiritInstanceNamedCameraAbiReady());
            image[sites[i]]^=1; CHECK(SudekiMpSpiritInstanceNamedCameraAbiReady());
        }
    }
    for(i=0;i<2;++i) {
        CHECK(SudekiMpCreateSpiritInstance(&pair[i]));
        CHECK(SudekiMpBindSpiritInstanceCaster(&pair[i],caster_actors[i],i ? 14:5,17,caster_witness));
    }
    /* Bind the real registry contract but reject capacity BEFORE any native
     * creation. Only then replace construction/destruction with test doubles. */
    for(i=6;i<9;++i) {
        uint8_t *p=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,NAMED_CAMERA_SIZE);
        CHECK(p!=NULL); if(!p) return;
        *(void **)p=image+NAMED_CAMERA_VTABLE;
        snprintf((char *)p+NAMED_CAMERA_NAME,NAMED_NAME_SIZE,"Occupied%u",i);
        *(void **)(named_test_manager+0x24+4*i)=p;
    }
    CHECK(!SudekiMpEnableSpiritInstanceNamedCameras(&pair[0]) && GetLastError()==ERROR_NOT_ENOUGH_MEMORY);
    CHECK(named_manager==named_test_manager && !entries[0].named_ready);
    for(i=6;i<9;++i) { HeapFree(GetProcessHeap(),0,*named_slot(i)); *named_slot(i)=NULL; }
    named_add=fake_named_add; named_remove=fake_named_remove;
    idle=FALSE; CHECK(!SudekiMpEnableSpiritInstanceNamedCameras(&pair[0])); idle=TRUE;
    CHECK(!named_add_calls);
    /* One allocation, then failure: clean rollback without releasing caster. */
    named_fail_at=2;
    CHECK(!SudekiMpEnableSpiritInstanceNamedCameras(&pair[0]));
    CHECK(named_add_calls==2 && named_delete_calls==1 && pair[0].generation);
    CHECK(!entries[0].named_cameras[0] && !entries[0].named_cameras[1]);
    named_fail_at=0;
    for(i=0;i<2;++i) CHECK(SudekiMpEnableSpiritInstanceNamedCameras(&pair[i]));
    CHECK(!SudekiMpEnableSpiritInstanceNamedCameras(&pair[0]));
    CHECK(named_registry_exact() && !named_generation);
    entries[0].named_creation_uncertain=TRUE; /* Inject unknown factory outcome. */
    CHECK(!SudekiMpEnterSpiritInstance(&pair[1]));
    CHECK(!SudekiMpObserveSpiritInstanceNamedCamera(&pair[1],1,&observed));
    CHECK(!SudekiMpDestroySpiritInstance(&pair[0]));
    CHECK(!SudekiMpEnableSpiritInstanceNamedCameras(&pair[0]));
    CHECK(!SudekiMpResetSpiritInstanceAbi());
    entries[0].named_creation_uncertain=FALSE; /* Fixture repair only, never production recovery. */
    CHECK(SudekiMpEnableSpiritInstanceCastGates());
    a=SudekiMpEnterSpiritInstance(&pair[0]); CHECK(a);
    CHECK(named_lookup("InitCam",NULL)==entries[0].named_cameras[0]);
    CHECK(named_lookup("SkillCam",NULL)==entries[0].named_cameras[1]);
    CHECK(*(void **)(named_test_manager+0x20)==named_base[0]);
    CHECK(*(void **)(named_scene+0x7c)==named_base[0]+0x70); /* No render takeover. */
    actor_manager[CAST_GATE_OFFSET]|=CAST_BUSY_BIT;
    /* Prove disjoint mutable state, not merely disjoint lookup results. */
    ((uint8_t *)named_lookup("SkillCam",NULL))[0x100]=42;
    b=SudekiMpEnterSpiritInstance(&pair[1]); CHECK(b);
    CHECK(named_lookup("SkillCam",NULL)==entries[1].named_cameras[1]);
    CHECK(((uint8_t *)named_lookup("SkillCam",NULL))[0x100]==0);
    ((uint8_t *)named_lookup("SkillCam",NULL))[0x100]=73;
    n=SudekiMpEnterSpiritInstance(NULL); CHECK(n);
    CHECK(named_lookup("InitCam",NULL)==named_base[5]);
    CHECK(named_lookup("SkillCam",NULL)==named_base[4]);
    CHECK(SudekiMpLeaveSpiritInstance(n));
    CHECK(((uint8_t *)named_lookup("SkillCam",NULL))[0x100]==73);
    CHECK(!SudekiMpLeaveSpiritInstance(a)); /* Strict LIFO, nothing renamed. */
    CHECK(named_generation==pair[1].generation);
    CHECK(SudekiMpLeaveSpiritInstance(b));
    CHECK(((uint8_t *)named_lookup("SkillCam",NULL))[0x100]==42);
    n=SudekiMpEnterSpiritInstance(&pair[0]); CHECK(n); CHECK(SudekiMpLeaveSpiritInstance(n));
    /* A damaged namespace must fail BEFORE busy banks or globals change. */
    ((uint8_t *)entries[1].named_cameras[0])[NAMED_CAMERA_NAME]='X';
    CHECK(!SudekiMpEnterSpiritInstance(&pair[1]));
    CHECK(scope_depth==1 && named_generation==pair[0].generation &&
        (actor_manager[CAST_GATE_OFFSET]&CAST_BUSY_BIT));
    memcpy((uint8_t *)entries[1].named_cameras[0]+NAMED_CAMERA_NAME,entries[1].named_names[0],NAMED_NAME_SIZE);
    saved=*(void **)(image+NAMED_MANAGER_GLOBAL);
    *(void **)(image+NAMED_MANAGER_GLOBAL)=original_manager;
    CHECK(!SudekiMpLeaveSpiritInstance(a) && scope_depth==1);
    CHECK(!SudekiMpDestroySpiritInstance(&pair[0]));
    *(void **)(image+NAMED_MANAGER_GLOBAL)=saved;
    actor_manager[CAST_GATE_OFFSET]&=~CAST_BUSY_BIT;
    CHECK(SudekiMpLeaveSpiritInstance(a));
    CHECK(named_lookup("InitCam",NULL)==named_base[5]);
    stale=pair[0]; ++stale.generation;
    CHECK(!SudekiMpObserveSpiritInstanceNamedCamera(&stale,1,&observed) && !observed);
    CHECK(!SudekiMpObserveSpiritInstanceNamedCamera(&pair[0],0,&observed));
    CHECK(!SudekiMpObserveSpiritInstanceNamedCamera(&pair[0],3,&observed));
    for(i=0;i<2;++i) for(j=0;j<2;++j) {
        CHECK(SudekiMpObserveSpiritInstanceNamedCamera(&pair[i],j+1,&observed));
        CHECK(observed==entries[i].named_cameras[j]);
    }
    /* Foreign actor component/name/slot cannot borrow a retained camera. */
    *(void **)(caster_actors[0]+0x130)=caster_states[1];
    CHECK(!SudekiMpObserveSpiritInstanceNamedCamera(&pair[0],1,&observed));
    *(void **)(caster_actors[0]+0x130)=caster_states[0];
    saved=*named_slot(entries[0].named_slots[0]); *named_slot(entries[0].named_slots[0])=named_base[1];
    CHECK(!SudekiMpEnterSpiritInstance(&pair[0]));
    *named_slot(entries[0].named_slots[0])=saved;
    *(void **)(named_test_manager+0x20)=saved;
    CHECK(!SudekiMpDestroySpiritInstance(&pair[0]));
    *(void **)(named_test_manager+0x20)=named_base[0];
    *(void **)(named_scene+0x7c)=*(void **)((uint8_t *)saved+0x34);
    CHECK(!SudekiMpDestroySpiritInstance(&pair[0]));
    *(void **)(named_scene+0x7c)=named_base[0]+0x70;
    *(void **)((uint8_t *)saved+0x30)=(void *)1;
    CHECK(!SudekiMpDestroySpiritInstance(&pair[0]));
    *(void **)((uint8_t *)saved+0x30)=NULL;
    named_remove_blocked=TRUE;
    CHECK(!SudekiMpDestroySpiritInstance(&pair[0]) && !SudekiMpResetSpiritInstanceAbi());
    CHECK(named_delete_calls==1);
    named_remove_blocked=FALSE;
    named_block_after_delete=TRUE;
    CHECK(!SudekiMpDestroySpiritInstance(&pair[0]));
    CHECK(named_delete_calls==2 && entries[0].named_cameras[0] && !entries[0].named_cameras[1]);
    CHECK(!SudekiMpObserveSpiritInstanceNamedCamera(&pair[0],1,&observed));
    CHECK(!SudekiMpEnterSpiritInstance(&pair[0]));
    CHECK(!scope_depth && globals_exact(original_manager,original_camera));
    named_block_after_delete=named_remove_blocked=FALSE;
    CHECK(SudekiMpDestroySpiritInstance(&pair[0]));
    CHECK(SudekiMpDestroySpiritInstance(&pair[1]));
    CHECK(named_delete_calls==5);
    CHECK(SudekiMpResetSpiritInstanceAbi());
    CHECK(!named_manager && !named_generation);
    CHECK(!SudekiMpObserveSpiritInstanceNamedCamera(&stale,1,&observed));
}
static void *persistent_test_task_actor;
static BOOL persistent_test_task(void *actor,uint64_t session) {
    return session==17 && actor==persistent_test_task_actor;
}
static void persistent_ui_tests(void) {
    unsigned int direction;
    for(direction=0;direction<2;++direction) {
        unsigned int l=direction,r=1-direction;
        SudekiMpSpiritInstance forbidden={0};
        uint8_t *saved;
        setup();
        caster_states[0][0x131]=caster_states[1][0x131]=direction ? 4:0;
        caster_states[0][0x133]=caster_states[1][0x133]=8; /* Native cleanup leaves this bit stale. */
        caster_skills[0][0x6c]=caster_skills[1][0x6c]=0;
        *(void **)(fake_controller+0x248)=caster_actors[l];
        *(uint32_t *)(fake_ui+0x54)=3;
        CHECK(!SudekiMpBindRemoteCharacterSkillUi(caster_actors[l],caster_actors[r],r ? 14:5,
            18,caster_witness,persistent_test_task));
        CHECK(SudekiMpBindRemoteCharacterSkillUi(caster_actors[l],caster_actors[r],r ? 14:5,
            17,caster_witness,persistent_test_task));
        CHECK(SudekiMpRemoteCharacterSkillUiHealthy());
        CHECK(caster_states[r][0x133]==8 && *(uint32_t *)(fake_ui+0x54)==3);
        CHECK(!SudekiMpCreateSpiritInstance(&forbidden));
        CHECK(!SudekiMpUninstallSpiritInstanceUpdates());
        CHECK(!remote_skill_ui_transition(caster_skills[l],fake_ui,1));
        ++*(uint32_t *)(fake_ui+0x54); /* The local cast keeps its own lock. */
        CHECK(remote_skill_input_transition(caster_skills[r],fake_controller,TRUE));
        CHECK(remote_skill_ui_transition(caster_skills[r],fake_ui,1));
        CHECK(!SudekiMpUnbindRemoteCharacterSkillUi());
        CHECK(!SudekiMpResetSpiritInstanceAbi());
        CHECK(persistent_skill_ui.remote_skill_ui_acquired);
        CHECK(*(uint32_t *)(fake_ui+0x54)==4 && !update_fault);
        /* Same remote actor's unrelated/Spirit CState is NOT a CSkill lock. */
        persistent_test_task_actor=NULL;
        caster_states[r][0x131]=1;
        caster_states[r][0x133]&=~8; /* Native e459d clears it before the hook. */
        CHECK(!remote_state_ui_transition(caster_states[r],TRUE));
        persistent_test_task_actor=caster_actors[r];
        CHECK(remote_state_ui_transition(caster_states[r],TRUE));
        CHECK(!persistent_skill_ui.remote_ui_acquired); /* No Spirit manager ownership. */
        caster_states[r][0x131]=4;
        CHECK(remote_state_ui_transition(caster_states[r],FALSE));
        persistent_test_task_actor=NULL;
        /* Foreign UI/actor replacements cannot release our outstanding lease. */
        saved=*(void **)(image+UI_GLOBAL);
        *(void **)(image+UI_GLOBAL)=foreign_ui;
        CHECK(remote_skill_ui_transition(caster_skills[r],foreign_ui,-1));
        CHECK(update_fault && persistent_skill_ui.remote_skill_ui_acquired);
        CHECK(!SudekiMpRemoteCharacterSkillUiHealthy());
        *(void **)(image+UI_GLOBAL)=saved; update_fault=FALSE;
        if(direction) --*(uint32_t *)(fake_ui+0x54); /* Local first. */
        CHECK(remote_skill_ui_transition(caster_skills[r],fake_ui,-1));
        CHECK(remote_skill_input_transition(caster_skills[r],fake_controller,FALSE));
        if(!direction) --*(uint32_t *)(fake_ui+0x54); /* Remote first. */
        CHECK(*(uint32_t *)(fake_ui+0x54)==3 && !update_fault);
        CHECK(SudekiMpUnbindRemoteCharacterSkillUi());
        CHECK(!persistent_skill_ui.caster && !persistent_skill_task);
        CHECK(SudekiMpResetSpiritInstanceAbi());
        CHECK(*(void **)(image+MANAGER_GLOBAL)==original_manager &&
            *(void **)(image+CAMERA_GLOBAL)==original_camera);
    }
    *(void **)(fake_controller+0x248)=caster_actors[0];
    *(uint32_t *)(fake_ui+0x54)=0;
}
static void shared_ssp_tests(void) {
    SudekiMpSpiritInstance pair[2]={{0}},extra={0};
    uint32_t a,b,neutral;
    float *original,*first,*second;
    setup();
    caster_states[0][0x131]=caster_states[1][0x131]=0;
    for(unsigned int i=0;i<2;++i) {
        CHECK(SudekiMpCreateSpiritInstance(&pair[i]));
        CHECK(SudekiMpBindSpiritInstanceCaster(&pair[i],caster_actors[i],i ? 14:5,17,caster_witness));
    }
    original=(float *)(original_manager+0xa8);
    first=(float *)((uint8_t *)pair[0].manager+0xa8);
    second=(float *)((uint8_t *)pair[1].manager+0xa8);
    *original=200.0f;
    CHECK(SudekiMpSpiritInstanceSharedSspAbiReady());
    image[0x1008c]^=1;
    CHECK(!SudekiMpSpiritInstanceSharedSspAbiReady() && !SudekiMpEnableSpiritInstanceSharedSsp());
    image[0x1008c]^=1;
    CHECK(SudekiMpEnableSpiritInstanceSharedSsp());
    CHECK(*first==200 && *second==200 && *original==200);
    CHECK(!SudekiMpEnableSpiritInstanceSharedSsp() && !SudekiMpCreateSpiritInstance(&extra));
    a=SudekiMpEnterSpiritInstance(&pair[0]); CHECK(a); *first-=15;
    b=SudekiMpEnterSpiritInstance(&pair[1]); CHECK(b && *second==185); *second-=100;
    neutral=SudekiMpEnterSpiritInstance(NULL); CHECK(neutral && *original==85);
    *original+=5; /* Native unrelated reward inside nested neutral work. */
    CHECK(SudekiMpLeaveSpiritInstance(neutral) && *second==90);
    CHECK(SudekiMpLeaveSpiritInstance(b) && *first==90); /* Not stale 185. */
    *first-=15;
    CHECK(SudekiMpLeaveSpiritInstance(a) && *original==75 && !scope_depth);
    *original+=20; /* Native reward outside any private callback. */
    a=SudekiMpEnterSpiritInstance(&pair[0]); CHECK(a && *first==95);
    *first=NAN;
    CHECK(!SudekiMpLeaveSpiritInstance(a) && scope_depth==1 && *original==95);
    *first=95;
    CHECK(SudekiMpLeaveSpiritInstance(a));
    *second=123; /* Unknown write to a dormant private manager is not a grant. */
    CHECK(!SudekiMpEnterSpiritInstance(&pair[1]) && !scope_depth && *original==95);
    CHECK(!SudekiMpDestroySpiritInstance(&pair[1]));
    *second=90;
    b=SudekiMpEnterSpiritInstance(&pair[1]); CHECK(b && *second==95);
    CHECK(SudekiMpLeaveSpiritInstance(b));
    *original=200;
    a=SudekiMpEnterSpiritInstance(&pair[0]); CHECK(a && *first==200); *first-=100;
    CHECK(SudekiMpLeaveSpiritInstance(a) && *original==100);
    b=SudekiMpEnterSpiritInstance(&pair[1]); CHECK(b && *second==100); *second-=100;
    CHECK(SudekiMpLeaveSpiritInstance(b) && *original==0); /* One pool, not 200 each. */
    for(unsigned int i=0;i<2;++i) CHECK(SudekiMpDestroySpiritInstance(&pair[i]));
    CHECK(SudekiMpResetSpiritInstanceAbi() && !shared_ssp_enabled);
}
static void inactive_native_update_tests(void) {
    SudekiMpSpiritInstance instance={0};
    DWORD camera_protect,soul_protect,ignored;
    uint32_t cookie,cameras,souls;
    setup();
    CHECK(SudekiMpCreateSpiritInstance(&instance));
    Entry *e=find(&instance);
    CHECK(e && object_exact(instance.camera,CAMERA_SIZE,CAMERA_VTABLE));
    /* Execute only the fully checked no-op paths in the synthetic image.
     * Active/substituted updates retain the existing context-routing tests. */
    CHECK(VirtualProtect(image+0x11000,0x1000,PAGE_EXECUTE_READ,&camera_protect));
    CHECK(VirtualProtect(image+0x12a000,0x2000,PAGE_EXECUTE_READ,&soul_protect));
    original_updates[0]=(NativeUpdate)(image+update_rvas[0]);
    original_updates[1]=(NativeUpdate)(image+update_rvas[1]);
    cookie=next_scope_cookie;
    SetLastError(1234);
    camera_update(instance.camera,0.125f);
    CHECK(GetLastError()==1234 && !update_fault && !scope_depth && !update_depth);
    CHECK(next_scope_cookie==cookie && globals_exact(original_manager,original_camera));
    SetLastError(2345);
    soul_update(e->souls[0],0.125f);
    CHECK(GetLastError()==2345 && !update_fault && !scope_depth && !update_depth);
    CHECK(next_scope_cookie==cookie && globals_exact(original_manager,original_camera));
    CHECK(SudekiMpObserveSpiritInstanceUpdates(&instance,&cameras,&souls));
    CHECK(cameras==1 && souls==1);
    CHECK(VirtualProtect(image+0x11000,0x1000,camera_protect,&ignored));
    CHECK(VirtualProtect(image+0x12a000,0x2000,soul_protect,&ignored));
    original_updates[0]=fake_camera_update;
    original_updates[1]=fake_soul_update;
    /* State zero is insufficient when the callback is not the exact body. */
    camera_update(instance.camera,0.125f);
    CHECK(next_scope_cookie>cookie && !update_fault && !scope_depth);
    CHECK(SudekiMpDestroySpiritInstance(&instance));
    CHECK(SudekiMpResetSpiritInstanceAbi());
}

static uint8_t rebind_actors[4][0x134],rebind_states[4][0x134],rebind_skills[4][0x78];
static BOOL rebind_witness(void *actor,uint64_t session) {
    for(unsigned i=0;i<4;++i) if(session==17 && actor==rebind_actors[i]) return TRUE;
    return FALSE;
}
static void rebind_local_tests(void) {
    SudekiMpSpiritInstance instances[4]={{0}};
    const uint8_t types[4]={0x23,1,5,14};
    Entry before[4];
    setup(); named_fixture();
    CHECK(SudekiMpEnableSpiritInstanceNamedCameraBanking());
    named_manager=named_test_manager;
    named_originals[0]=named_base[5]; named_original_slots[0]=5;
    named_originals[1]=named_base[4]; named_original_slots[1]=4;
    for(unsigned k=0;k<2;++k)
        memcpy(named_original_names[k],(uint8_t *)named_originals[k]+NAMED_CAMERA_NAME,NAMED_NAME_SIZE);
    named_add=fake_named_add; named_remove=fake_named_remove;
    *(void **)(fake_controller+0x248)=rebind_actors[0];
    for(unsigned i=0;i<4;++i) {
        *(void **)rebind_actors[i]=image+0x1200+i*4;
        *(void **)rebind_states[i]=image+0x1300+i*4;
        *(void **)(rebind_actors[i]+0x130)=rebind_states[i];
        *(void **)(rebind_states[i]+0x10)=rebind_actors[i];
        *(void **)(rebind_actors[i]+0xd8)=rebind_skills[i];
        *(void **)rebind_skills[i]=image+0x2cbad0;
        *(void **)(rebind_skills[i]+0x10)=rebind_actors[i];
        *(void **)(rebind_skills[i]+0x18)=image+0x2cbadc;
        CHECK(SudekiMpCreateSpiritInstance(&instances[i]));
        CHECK(SudekiMpBindSpiritInstanceCaster(&instances[i],rebind_actors[i],types[i],17,rebind_witness));
        CHECK(SudekiMpEnableSpiritInstanceNamedCameras(&instances[i]));
        if(i) {
            CHECK(SudekiMpEnableSpiritInstanceRemoteUi(&instances[i]));
            CHECK(SudekiMpEnableSpiritInstanceRemoteSkillUi(&instances[i]));
            CHECK(SudekiMpEnableSpiritInstanceRemoteSkillInput(&instances[i],rebind_actors[0],rebind_witness));
        }
    }
    CHECK(SudekiMpInstallSpiritInstanceNamedCameraUpdates());
    for(unsigned i=1;i<4;++i) CHECK(SudekiMpEnableSpiritInstanceRemoteCameraSelection(&instances[i]));
    CHECK(SudekiMpEnableSpiritInstanceSkillTargeting());
    for(unsigned i=0;i<4;++i) CHECK(SudekiMpConfigureSpiritInstanceSkillTiming(&instances[i],FALSE));
    memcpy(before,entries,sizeof(before));
    /* Metadata cannot authorize a controller switch. */
    CHECK(!SudekiMpRebindSpiritInstanceLocalOwner(&instances[2],rebind_actors[2],rebind_witness));
    CHECK(!memcmp(before,entries,sizeof(before)));
    *(void **)(fake_controller+0x248)=rebind_actors[2]; /* Simulated native handoff. */
    entries[1].remote_skill_input_acquired=TRUE;
    CHECK(!SudekiMpRebindSpiritInstanceLocalOwner(&instances[2],rebind_actors[2],rebind_witness));
    entries[1].remote_skill_input_acquired=FALSE;
    CHECK(!memcmp(before,entries,sizeof(before)));
    *(void **)(image+UI_GLOBAL)=foreign_ui;
    CHECK(!SudekiMpRebindSpiritInstanceLocalOwner(&instances[2],rebind_actors[2],rebind_witness));
    *(void **)(image+UI_GLOBAL)=fake_ui;
    CHECK(!memcmp(before,entries,sizeof(before)));
    *(void **)(named_test_manager+0x20)=entries[0].named_cameras[0];
    *(void **)(named_scene+0x7c)=*(void **)((uint8_t *)entries[0].named_cameras[0]+0x34);
    CHECK(!SudekiMpRebindSpiritInstanceLocalOwner(&instances[2],rebind_actors[2],rebind_witness));
    *(void **)(named_test_manager+0x20)=named_base[0];
    *(void **)(named_scene+0x7c)=named_base[0]+0x70;
    CHECK(!memcmp(before,entries,sizeof(before)));
    for(unsigned selected=0;selected<4;++selected) {
        *(void **)(fake_controller+0x248)=rebind_actors[selected];
        CHECK(SudekiMpRebindSpiritInstanceLocalOwner(&instances[selected],rebind_actors[selected],rebind_witness));
        for(unsigned i=0;i<4;++i) {
            CHECK(entries[i].identity.generation==instances[i].generation);
            CHECK(entries[i].caster==rebind_actors[i]);
            CHECK(entries[i].remote_ui==(i!=selected));
            CHECK(entries[i].remote_skill_ui==(i!=selected));
            CHECK(entries[i].remote_skill_input==(i!=selected));
            CHECK(entries[i].remote_camera_selection==(i!=selected));
            CHECK(entries[i].local_actor==(i!=selected?rebind_actors[selected]:NULL));
        }
        CHECK(globals_exact(original_manager,original_camera));
        CHECK(*(void **)(named_test_manager+0x20)==named_base[0]);
        CHECK(*(void **)(named_scene+0x7c)==named_base[0]+0x70);
        CHECK(!update_fault && !scope_depth && !selection_generation);
    }
    for(unsigned i=4;i>0;--i) CHECK(SudekiMpDestroySpiritInstance(&instances[i-1]));
    CHECK(SudekiMpResetSpiritInstanceAbi());
    *(void **)(fake_controller+0x248)=caster_actors[0];
}

int main(void) {
    SudekiMpSpiritInstance instances[4]={{0}}, extra={0}, stale;
    unsigned int i,j;
    {
        const uint8_t types[4]={0x23,1,5,14};
        for(i=0;i<4;++i) {
            for(j=0;j<8;++j) CHECK(strike_matches_caster(types[i],j)==(j/2==i));
            CHECK(!strike_matches_caster(types[i],8));
            CHECK(!strike_matches_caster(types[i],UINT_MAX));
        }
        CHECK(!strike_matches_caster(0,0));
        CHECK(!strike_matches_caster(0x7f,6));
    }
    image=VirtualAlloc(NULL,0x45f000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!image) return 1;
    fixture();
    image[0x9b858]^=1;
    CHECK(!SudekiMpInitializeSpiritInstanceAbi((HMODULE)image,witness));
    image[0x9b858]^=1;
    image[0x113be]^=1;
    CHECK(!SudekiMpInitializeSpiritInstanceAbi((HMODULE)image,witness));
    image[0x113be]^=1;
    image[0xb4948]^=1;
    CHECK(!SudekiMpInitializeSpiritInstanceAbi((HMODULE)image,witness));
    image[0xb4948]^=1;
    image[0xb4f4f]^=1;
    CHECK(!SudekiMpInitializeSpiritInstanceAbi((HMODULE)image,witness));
    image[0xb4f4f]^=1;
    setup();
    idle=FALSE; CHECK(!SudekiMpCreateSpiritInstance(&extra)); CHECK(!extra.generation);
    idle=TRUE;
    *(uint32_t *)(original_manager+0x5c)=10;
    CHECK(!SudekiMpCreateSpiritInstance(&extra));
    *(uint32_t *)(original_manager+0x5c)=0;
    *(uint32_t *)(original_camera+0x1a0)=1;
    CHECK(!SudekiMpCreateSpiritInstance(&extra));
    *(uint32_t *)(original_camera+0x1a0)=0;
    for(i=0;i<4;++i) {
        const uint8_t masks[4]={0,0x30,0x40,0xff};
        original_manager[MANAGER_UNLOCKS]=masks[i];
        original_manager[MANAGER_UNLOCKS-1]=0x55;
        original_manager[MANAGER_UNLOCKS+1]=0x66;
        CHECK(SudekiMpCreateSpiritInstance(&instances[i]));
        CHECK(((uint8_t *)instances[i].manager)[MANAGER_UNLOCKS]==masks[i]);
        CHECK(original_manager[MANAGER_UNLOCKS]==masks[i]);
        CHECK(!((uint8_t *)instances[i].manager)[MANAGER_UNLOCKS-1]);
        CHECK(!((uint8_t *)instances[i].manager)[MANAGER_UNLOCKS+1]);
        CHECK(globals_exact(original_manager,original_camera));
        for(j=0;j<i;++j) {
            CHECK(instances[i].manager!=instances[j].manager);
            CHECK(instances[i].camera!=instances[j].camera);
            CHECK(instances[i].generation!=instances[j].generation);
            CHECK(entries[i].souls[0]!=entries[j].souls[0]);
        }
    }
    {
        NativeUpdate camera=(NativeUpdate)*(void **)(image+CAMERA_VTABLE+4);
        NativeUpdate soul=(NativeUpdate)*(void **)(image+SOUL_VTABLE+4);
        uint32_t cameras,souls;
        SudekiMpSpiritInstanceState state;
        BOOL active=TRUE;
        observed_memory_queries=0;
        CHECK(SudekiMpObserveSpiritInstanceActivity(&instances[0],&active) && !active);
        unsigned activity_queries=observed_memory_queries;
        observed_memory_queries=0;
        CHECK(SudekiMpObserveSpiritInstance(&instances[0],&state) && state.idle && !state.state);
        CHECK(activity_queries==2 && observed_memory_queries>=activity_queries+6);
        printf("Activity observation: %u memory queries vs %u for full drain observation\n",
            activity_queries,observed_memory_queries);
        *(uint32_t *)((uint8_t *)instances[0].manager+0x5c)=10;
        CHECK(SudekiMpObserveSpiritInstanceActivity(&instances[0],&active) && active);
        *(uint32_t *)((uint8_t *)instances[0].manager+0x98)=4;
        CHECK(SudekiMpObserveSpiritInstance(&instances[0],&state) && state.state==10 &&
            state.strike_id==4 && !state.idle);
        CHECK(SudekiMpObserveSpiritInstance(&instances[1],&state) && state.idle && !state.state);
        *(uint32_t *)((uint8_t *)instances[0].manager+0x5c)=0;
        *(uint32_t *)((uint8_t *)instances[0].camera+0x1a0)=1;
        CHECK(SudekiMpObserveSpiritInstance(&instances[0],&state) && !state.idle && state.camera_active);
        CHECK(SudekiMpObserveSpiritInstanceActivity(&instances[0],&active) && !active);
        /* Manager inactivity cannot authorize releasing a camera or soul. */
        *(uint32_t *)((uint8_t *)instances[0].camera+0x1a0)=0;
        ((uint8_t *)entries[0].souls[0])[0x48]=1;
        CHECK(SudekiMpObserveSpiritInstanceActivity(&instances[0],&active) && !active);
        CHECK(SudekiMpObserveSpiritInstance(&instances[0],&state) && !state.body_idle);
        ((uint8_t *)entries[0].souls[0])[0x48]=0;
        void *saved_vtable=*(void **)instances[0].manager;
        *(void **)instances[0].manager=NULL; active=TRUE;
        CHECK(!SudekiMpObserveSpiritInstanceActivity(&instances[0],&active) && active);
        *(void **)instances[0].manager=saved_vtable;
        saved_vtable=*(void **)instances[0].camera; *(void **)instances[0].camera=NULL;
        CHECK(!SudekiMpObserveSpiritInstanceActivity(&instances[0],&active) && active);
        *(void **)instances[0].camera=saved_vtable;
        DWORD saved_owner=owner_thread; owner_thread=0;
        CHECK(!SudekiMpObserveSpiritInstanceActivity(&instances[0],&active) && active);
        owner_thread=saved_owner;
        update_fault=TRUE;
        CHECK(!SudekiMpObserveSpiritInstanceActivity(&instances[0],&active) && active);
        update_fault=FALSE;
        stale=instances[0]; ++stale.generation;
        CHECK(!SudekiMpObserveSpiritInstance(&stale,&state));
        CHECK(!SudekiMpObserveSpiritInstanceActivity(&stale,&active) && active);
        CHECK(!SudekiMpObserveSpiritInstanceActivity(&instances[0],NULL));
        CHECK(!SudekiMpObserveSpiritInstance(&instances[0],NULL));
        test_instances=instances; nested_updates=TRUE;
        CHECK(SudekiMpScheduleIdleSpiritInstanceProbe(&instances[0]));
        CHECK(!SudekiMpScheduleIdleSpiritInstanceProbe(&instances[0]));
        camera(instances[0].camera,0.125f);
        nested_updates=FALSE;
        CHECK(updated_cameras==1 && updated_souls==2);
        CHECK(globals_exact(original_manager,original_camera) && !scope_depth && !update_depth);
        CHECK(SudekiMpObserveSpiritInstanceUpdates(&instances[0],&cameras,&souls));
        CHECK(cameras==1 && souls==0);
        CHECK(SudekiMpObserveSpiritInstanceUpdates(&instances[1],&cameras,&souls));
        CHECK(cameras==0 && souls==1);
        soul(unrelated_soul,0.125f); /* Unrelated native update remains untouched. */
        camera(original_camera,0.125f);
        CHECK(updated_cameras==2 && updated_souls==3);
        CHECK(SudekiMpScheduleSpiritInstanceManager(&instances[0]));
        CHECK(!SudekiMpScheduleSpiritInstanceManager(&instances[0]));
        CHECK(*(int16_t *)((uint8_t *)instances[0].manager+0x20)==0);
        CHECK(*(int16_t *)((uint8_t *)instances[1].manager+0x20)==-1);
        manager_update(instances[0].manager,0.125f);
        CHECK(updated_managers==2 && updated_cameras==3);
        CHECK(globals_exact(original_manager,original_camera) && !scope_depth && !update_depth);
        CHECK(SudekiMpObserveSpiritInstanceManagerTicks(&instances[0],&cameras) && cameras==1);
        CHECK(SudekiMpObserveSpiritInstanceManagerTicks(&instances[1],&cameras) && cameras==0);
        idle=FALSE;
        CHECK(!SudekiMpScheduleSpiritInstanceManager(&instances[1]));
        idle=TRUE;
        CHECK(*(int16_t *)((uint8_t *)instances[1].manager+0x20)==-1);
        CHECK(!SudekiMpUninstallSpiritInstanceUpdates());
    }
    CHECK(!SudekiMpCreateSpiritInstance(&extra));
    CHECK(!SudekiMpCreateSpiritInstance(&instances[0]));
    CHECK(!SudekiMpResetSpiritInstanceAbi());
    {
        uint32_t a,b,neutral, stack[16];
        a=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(a);
        CHECK(globals_exact(instances[0].manager,instances[0].camera));
        b=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(b && b!=a);
        CHECK(globals_exact(instances[1].manager,instances[1].camera));
        *(uint32_t *)((uint8_t *)instances[0].manager+0x5c)=10;
        *(uint32_t *)((uint8_t *)instances[1].manager+0x5c)=2;
        CHECK(!SudekiMpLeaveSpiritInstance(a)); /* LIFO; no state copied between casts. */
        CHECK(!SudekiMpCreateSpiritInstance(&extra));
        CHECK(!SudekiMpDestroySpiritInstance(&instances[0]));
        CHECK(!SudekiMpResetSpiritInstanceAbi());
        neutral=SudekiMpEnterSpiritInstance(NULL); CHECK(neutral);
        CHECK(globals_exact(original_manager,original_camera));
        CHECK(SudekiMpLeaveSpiritInstance(neutral));
        CHECK(globals_exact(instances[1].manager,instances[1].camera));
        *(void **)(image+CAMERA_GLOBAL)=(void *)0xdeadbeef;
        CHECK(!SudekiMpLeaveSpiritInstance(b));
        CHECK(scope_depth==2 && !SudekiMpResetSpiritInstanceAbi());
        *(void **)(image+CAMERA_GLOBAL)=instances[1].camera;
        CHECK(SudekiMpLeaveSpiritInstance(b));
        CHECK(globals_exact(instances[0].manager,instances[0].camera));
        CHECK(SudekiMpLeaveSpiritInstance(a));
        CHECK(globals_exact(original_manager,original_camera));
        CHECK(*(uint32_t *)((uint8_t *)instances[0].manager+0x5c)==10);
        CHECK(*(uint32_t *)((uint8_t *)instances[1].manager+0x5c)==2);
        *(uint32_t *)((uint8_t *)instances[0].manager+0x5c)=0;
        *(uint32_t *)((uint8_t *)instances[1].manager+0x5c)=0;
        CHECK(!SudekiMpLeaveSpiritInstance(a)); /* Old cookie never revives a scope. */
        for(i=0;i<16;++i) { stack[i]=SudekiMpEnterSpiritInstance(&instances[i%4]); CHECK(stack[i]); }
        CHECK(!SudekiMpEnterSpiritInstance(NULL));
        for(i=16;i>0;--i) CHECK(SudekiMpLeaveSpiritInstance(stack[i-1]));
        CHECK(!scope_depth && globals_exact(original_manager,original_camera));
    }
    {
        HANDLE thread=CreateThread(NULL,0,foreign_thread,NULL,0,NULL);
        CHECK(thread!=NULL);
        if(thread) { WaitForSingleObject(thread,10000); CloseHandle(thread); }
    }
    /* Busy, referenced, replaced and forged handles must not free anything. */
    *(uint32_t *)((uint8_t *)instances[0].manager+0x5c)=10;
    CHECK(!SudekiMpDestroySpiritInstance(&instances[0]));
    *(uint32_t *)((uint8_t *)instances[0].manager+0x5c)=0;
    *(void **)((uint8_t *)instances[0].camera+0x28)=(void *)1;
    CHECK(!SudekiMpDestroySpiritInstance(&instances[0]));
    *(void **)((uint8_t *)instances[0].camera+0x28)=NULL;
    /* A null TPtr target with stale intrusive links is not a drained record. */
    *(void **)((uint8_t *)instances[0].camera+0x58)=(void *)1;
    CHECK(!SudekiMpDestroySpiritInstance(&instances[0]));
    *(void **)((uint8_t *)instances[0].camera+0x58)=NULL;
    *(void **)((uint8_t *)instances[0].manager+0x64)=(void *)1;
    CHECK(!SudekiMpDestroySpiritInstance(&instances[0]));
    *(void **)((uint8_t *)instances[0].manager+0x64)=NULL;
    *(void **)((uint8_t *)entries[0].souls[0]+0x40)=(void *)1;
    CHECK(!SudekiMpDestroySpiritInstance(&instances[0]));
    *(void **)((uint8_t *)entries[0].souls[0]+0x40)=NULL;
    stale=instances[0]; ++stale.generation;
    CHECK(!SudekiMpDestroySpiritInstance(&stale));
    CHECK(!deleted_cameras && !deleted_managers && !deleted_souls);
    {
        unsigned int before=updated_cameras;
        uint32_t cameras,souls,cookie=next_scope_cookie+1;
        corrupt_update_slot=TRUE;
        camera_update(instances[0].camera,0.125f);
        CHECK(update_fault && scope_depth==1 && updated_cameras==before+1);
        CHECK(!SudekiMpEnterSpiritInstance(&instances[1]));
        CHECK(!SudekiMpObserveSpiritInstanceUpdates(&instances[0],&cameras,&souls));
        CHECK(!SudekiMpDestroySpiritInstance(&instances[0]));
        CHECK(!SudekiMpResetSpiritInstanceAbi());
        camera_update(instances[0].camera,0.125f);
        CHECK(updated_cameras==before+1); /* No duplicate tick after failed restore. */
        /* Emulate the foreign owner returning our slot. Only then can the
         * retained scope unwind; fault remains latched until full reset. */
        *(void **)(image+CAMERA_GLOBAL)=instances[0].camera;
        CHECK(SudekiMpLeaveSpiritInstance(cookie));
        CHECK(update_fault && !scope_depth && !update_depth);
        corrupt_update_slot=FALSE;
    }
    stale=instances[0];
    CHECK(SudekiMpDestroySpiritInstance(&instances[0]));
    CHECK(!SudekiMpDestroySpiritInstance(&stale));
    CHECK(SudekiMpCreateSpiritInstance(&instances[0]));
    CHECK(stale.generation!=instances[0].generation);
    CHECK(!SudekiMpDestroySpiritInstance(&stale));
    /* Both foreign-slot failure points retain native lifetime and retry only
     * the restoration, never the destructor or already-deleted children. */
    foreign_camera_delete=TRUE;
    CHECK(!SudekiMpDestroySpiritInstance(&instances[1]));
    j=deleted_cameras;
    CHECK(!SudekiMpDestroySpiritInstance(&instances[1]) && deleted_cameras==j);
    CHECK(!SudekiMpResetSpiritInstanceAbi());
    *(void **)(image+CAMERA_GLOBAL)=NULL; foreign_camera_delete=FALSE;
    CHECK(SudekiMpDestroySpiritInstance(&instances[1]) && deleted_cameras==j);
    CHECK(globals_exact(original_manager,original_camera));
    foreign_manager_delete=TRUE;
    CHECK(!SudekiMpDestroySpiritInstance(&instances[2]));
    j=deleted_managers;
    CHECK(!SudekiMpDestroySpiritInstance(&instances[2]) && deleted_managers==j);
    *(void **)(image+MANAGER_GLOBAL)=NULL; foreign_manager_delete=FALSE;
    CHECK(SudekiMpDestroySpiritInstance(&instances[2]) && deleted_managers==j);
    CHECK(SudekiMpDestroySpiritInstance(&instances[3]));
    CHECK(SudekiMpDestroySpiritInstance(&instances[0]));
    CHECK(created_cameras==deleted_cameras && created_managers==deleted_managers);
    CHECK(deleted_souls==4*deleted_managers);
    CHECK(SudekiMpResetSpiritInstanceAbi());
    CHECK(address(image+update_slots[0],image+update_rvas[0]));
    CHECK(address(image+update_slots[1],image+update_rvas[1]));
    CHECK(address(image+update_slots[2],image+update_rvas[2]));
    setup();
    native_participant_lock=fake_lock; native_participant_unlock=fake_unlock;
    native_ready_tail=fake_ready_tail;
    for(i=0;i<2;++i) {
        *(void **)caster_actors[i]=image+0x1000+i*4;
        *(void **)caster_states[i]=image+0x1100+i*4;
        *(void **)(caster_actors[i]+0x130)=caster_states[i];
        *(void **)(caster_states[i]+0x10)=caster_actors[i];
        *(void **)(caster_actors[i]+0xd8)=caster_skills[i];
        *(void **)caster_skills[i]=image+0x2cbad0;
        *(void **)(caster_skills[i]+0x10)=caster_actors[i];
        *(void **)(caster_skills[i]+0x18)=image+0x2cbadc;
        CHECK(SudekiMpCreateSpiritInstance(&instances[i]));
        CHECK(!SudekiMpBindSpiritInstanceCaster(&instances[i],caster_actors[i],i ? 14:5,18,caster_witness));
        CHECK(!SudekiMpBindSpiritInstanceCaster(&instances[i],caster_actors[i],0x7f,17,caster_witness));
        if(i) CHECK(!SudekiMpBindSpiritInstanceCaster(&instances[i],caster_actors[0],5,17,caster_witness));
        CHECK(SudekiMpBindSpiritInstanceCaster(&instances[i],caster_actors[i],i ? 14:5,17,caster_witness));
        CHECK(!SudekiMpBindSpiritInstanceCaster(&instances[i],caster_actors[1-i],i ? 5:14,17,caster_witness));
        {
            BOOL active=TRUE;
            CHECK(SudekiMpObserveSpiritInstanceActivity(&instances[i],&active) && !active);
            *(void **)(caster_actors[i]+0x130)=NULL; active=TRUE;
            CHECK(!SudekiMpObserveSpiritInstanceActivity(&instances[i],&active) && active);
            *(void **)(caster_actors[i]+0x130)=caster_states[i];
        }
        {
            SudekiMpSpiritInstance resolved={0};
            CHECK(SudekiMpResolveSpiritInstanceCaster(caster_actors[i],17,&resolved));
            CHECK(resolved.manager==instances[i].manager && resolved.camera==instances[i].camera &&
                resolved.generation==instances[i].generation);
            CHECK(!SudekiMpResolveSpiritInstanceCaster(caster_actors[i],18,&resolved));
            CHECK(!SudekiMpResolveSpiritInstanceCaster(NULL,17,&resolved));
            CHECK(!SudekiMpResolveSpiritInstanceCaster(caster_actors[i],17,NULL));
            *(void **)(caster_actors[i]+0x130)=NULL;
            CHECK(!SudekiMpResolveSpiritInstanceCaster(caster_actors[i],17,&resolved));
            *(void **)(caster_actors[i]+0x130)=caster_states[i];
        }
    }
    {
        uint32_t a,b,again,neutral;
        actor_manager[CAST_GATE_OFFSET]=0xa5|CAST_BUSY_BIT;
        CHECK(!SudekiMpEnableSpiritInstanceCastGates()); /* No adoption of an existing cast. */
        actor_manager[CAST_GATE_OFFSET]=0xa5;
        CHECK(SudekiMpEnableSpiritInstanceCastGates());
        CHECK(!SudekiMpEnableSpiritInstanceCastGates());
        CHECK(!SudekiMpCreateSpiritInstance(&extra));
        a=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(a);
        actor_manager[CAST_GATE_OFFSET]|=CAST_BUSY_BIT; /* Native start A. */
        b=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(b);
        CHECK(actor_manager[CAST_GATE_OFFSET]==0xa5); /* B's validator is not blocked by A. */
        actor_manager[CAST_GATE_OFFSET]|=CAST_BUSY_BIT; /* Native start B. */
        again=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(again);
        CHECK(actor_manager[CAST_GATE_OFFSET]==(0xa5|CAST_BUSY_BIT));
        actor_manager[CAST_GATE_OFFSET]&=~CAST_BUSY_BIT; /* A completes inside B. */
        CHECK(SudekiMpLeaveSpiritInstance(again));
        CHECK(actor_manager[CAST_GATE_OFFSET]==(0xa5|CAST_BUSY_BIT)); /* B stays busy. */
        CHECK(SudekiMpLeaveSpiritInstance(b));
        CHECK(actor_manager[CAST_GATE_OFFSET]==0xa5); /* Do not restore A's obsolete saved byte. */
        CHECK(SudekiMpLeaveSpiritInstance(a));
        CHECK(actor_manager[CAST_GATE_OFFSET]==(0xa5|CAST_BUSY_BIT)); /* Global union still includes B. */
        CHECK(!SudekiMpDestroySpiritInstance(&instances[1]));
        a=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(a);
        again=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(again);
        actor_manager[CAST_GATE_OFFSET]&=~CAST_BUSY_BIT;
        CHECK(SudekiMpLeaveSpiritInstance(again));
        CHECK(!(actor_manager[CAST_GATE_OFFSET]&CAST_BUSY_BIT)); /* Same-owner recursion. */
        neutral=SudekiMpEnterSpiritInstance(NULL); CHECK(neutral);
        actor_manager[CAST_GATE_OFFSET]|=CAST_BUSY_BIT; /* Independent neutral native work. */
        actor_manager[CAST_GATE_OFFSET]^=0x20; /* Other bits are never banked or rewound. */
        CHECK(SudekiMpLeaveSpiritInstance(neutral));
        CHECK(actor_manager[CAST_GATE_OFFSET]==0x85);
        CHECK(SudekiMpLeaveSpiritInstance(a));
        CHECK(actor_manager[CAST_GATE_OFFSET]==(0x85|CAST_BUSY_BIT));
        a=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(a);
        CHECK(actor_manager[CAST_GATE_OFFSET]==0x85);
        neutral=SudekiMpEnterSpiritInstance(NULL); CHECK(neutral);
        CHECK(actor_manager[CAST_GATE_OFFSET]==(0x85|CAST_BUSY_BIT));
        actor_manager[CAST_GATE_OFFSET]&=~CAST_BUSY_BIT;
        CHECK(SudekiMpLeaveSpiritInstance(neutral)); CHECK(SudekiMpLeaveSpiritInstance(a));
        CHECK(actor_manager[CAST_GATE_OFFSET]==0x85 && !cast_gate_union());
        actor_manager[CAST_GATE_OFFSET]|=CAST_BUSY_BIT; /* Unrouted writer must fail closed. */
        CHECK(!SudekiMpEnterSpiritInstance(&instances[0]));
        CHECK(!scope_depth && globals_exact(original_manager,original_camera));
        CHECK(!cast_gate_union() && (actor_manager[CAST_GATE_OFFSET]&CAST_BUSY_BIT));
        actor_manager[CAST_GATE_OFFSET]&=~CAST_BUSY_BIT;
        a=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(a);
        actor_manager[CAST_GATE_OFFSET]|=CAST_BUSY_BIT;
        *(void **)(caster_actors[1]+0x130)=caster_states[0];
        CHECK(!SudekiMpEnterSpiritInstance(&instances[1]));
        CHECK(scope_depth==1 && !entries[0].cast_busy); /* Failed entry hasn't captured or hidden A. */
        *(void **)(caster_actors[1]+0x130)=caster_states[1];
        *(void **)(image+CAST_GATE_GLOBAL)=replacement_actor_manager;
        CHECK(!SudekiMpLeaveSpiritInstance(a)); /* Readable replacement isn't the retained owner. */
        CHECK(scope_depth==1 && globals_exact(instances[0].manager,instances[0].camera));
        CHECK(replacement_actor_manager[CAST_GATE_OFFSET]==0);
        *(void **)(image+CAST_GATE_GLOBAL)=actor_manager;
        actor_manager[CAST_GATE_OFFSET]&=~CAST_BUSY_BIT;
        CHECK(SudekiMpLeaveSpiritInstance(a));
        CHECK(actor_manager[CAST_GATE_OFFSET]==0x85 && !cast_gate_union());
        {
            HANDLE thread=CreateThread(NULL,0,foreign_thread,NULL,0,NULL);
            CHECK(thread!=NULL);
            if(thread) { WaitForSingleObject(thread,10000); CloseHandle(thread); }
        }
    }
    for(i=0;i<2;++i) {
        uint32_t scope=SudekiMpEnterSpiritInstance(&instances[i]); CHECK(scope);
        CHECK(invoke_lock(caster_states[1-i],1,2)==0);
        CHECK(invoke_lock(caster_states[i],1,2)==1);
        CHECK(SudekiMpLeaveSpiritInstance(scope));
        *(uint32_t *)((uint8_t *)instances[i].manager+0x5c)=2;
        *(uint32_t *)((uint8_t *)instances[i].manager+0x98)=i ? 6:4;
        /* Retail reverse test: Elco's manager still lists Buki first. */
        *(void **)((uint8_t *)instances[i].manager+0x60)=caster_actors[0];
        *(void **)((uint8_t *)instances[i].manager+0x78)=caster_actors[1-i];
    }
    CHECK(participant_locks==2 && caster_states[0][0x131]==1 && caster_states[1][0x131]==1);
    manager_update(instances[0].manager,0.125f); CHECK(ready_calls==0);
    caster_states[0][0x131]=2;
    manager_update(instances[0].manager,0.125f); CHECK(ready_calls==1);
    CHECK(*(uint32_t *)((uint8_t *)instances[0].manager+0x5c)==10);
    CHECK(*(uint32_t *)((uint8_t *)instances[1].manager+0x5c)==2);
    CHECK(*(void **)((uint8_t *)instances[0].manager+0x78)==caster_actors[1]);
    for(i=0;i<2;++i) {
        uint32_t scope;
        if(i) {
            caster_states[1][0x131]=2; manager_update(instances[1].manager,0.125f);
            CHECK(ready_calls==2 && !update_fault);
            CHECK(*(void **)((uint8_t *)instances[1].manager+0x60)==caster_actors[0]);
        }
        scope=SudekiMpEnterSpiritInstance(&instances[i]); CHECK(scope);
        CHECK(participant_unlock(caster_states[1-i],2)==0);
        CHECK(participant_unlock(caster_states[i],2)==1);
        CHECK(SudekiMpLeaveSpiritInstance(scope));
        if(!i) CHECK(caster_states[1][0x131]==1 && entries[1].caster_lock_owned);
        *(uint32_t *)((uint8_t *)instances[i].manager+0x5c)=0;
        *(void **)((uint8_t *)instances[i].manager+0x60)=NULL;
        *(void **)((uint8_t *)instances[i].manager+0x78)=NULL;
        CHECK(SudekiMpDestroySpiritInstance(&instances[i]));
    }
    CHECK(ready_calls==2 && participant_unlocks==2 && !update_fault);
    CHECK(SudekiMpResetSpiritInstanceAbi());
    setup();
    native_participant_lock=fake_lock; native_participant_unlock=fake_unlock;
    for(i=0;i<2;++i) {
        CHECK(SudekiMpCreateSpiritInstance(&instances[i]));
        CHECK(SudekiMpBindSpiritInstanceCaster(&instances[i],caster_actors[i],i ? 14:5,17,caster_witness));
    }
    {
        uint32_t remote,local,neutral;
        *(uint32_t *)(fake_ui+0x54)=3; /* Existing unrelated UI locks are not ours. */
        CHECK(SudekiMpEnableSpiritInstanceRemoteUi(&instances[1]));
        CHECK(!SudekiMpEnableSpiritInstanceRemoteUi(&instances[1]));
        {
            void *resume_a=state_ui_acquire_resume,*resume_r=state_ui_release_resume;
            uint8_t before[0x134];
            state_ui_acquire_resume=state_ui_release_resume=fake_input_resume;
            for(j=0;j<2;++j) {
                caster_states[1][0x131]=2; caster_states[1][0x133]&=~8;
                memcpy(before,caster_states[1],sizeof(before));
                remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
                invoke_state_ui_bridge(caster_states[1],TRUE);
                CHECK(!update_fault && entries[1].remote_state_ui_acquired && !quiescent(&entries[1]));
                CHECK(input_bridge_edi==(uint32_t)(uintptr_t)caster_states[1] && input_bridge_fpu==1.0f);
                CHECK(!memcmp(before,caster_states[1],sizeof(before)) && *(uint32_t *)(fake_ui+0x54)==3);
                CHECK(SudekiMpLeaveSpiritInstance(remote));
                CHECK(!SudekiMpDestroySpiritInstance(&instances[1]));
                local=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(local);
                CHECK(!remote_state_ui_transition(caster_states[0],TRUE));
                ++*(uint32_t *)(fake_ui+0x54); /* Simulate the untouched local native acquisition. */
                CHECK(SudekiMpLeaveSpiritInstance(local));
                if(j) --*(uint32_t *)(fake_ui+0x54); /* Either local/remote cleanup order. */
                remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
                caster_states[1][0x131]=4;
                invoke_state_ui_bridge(caster_states[1],FALSE);
                CHECK(input_bridge_fpu==1.0f && !entries[1].remote_state_ui_acquired && !update_fault);
                CHECK(*(uint32_t *)(fake_ui+0x54)==(j ? 3:4));
                CHECK(SudekiMpLeaveSpiritInstance(remote));
                if(!j) --*(uint32_t *)(fake_ui+0x54);
            }
            remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
            caster_states[1][0x131]=1;
            CHECK(remote_state_ui_transition(caster_states[1],TRUE));
            CHECK(remote_state_ui_transition(caster_states[1],TRUE) && update_fault); /* duplicate */
            update_fault=FALSE;
            *(void **)(image+UI_GLOBAL)=foreign_ui; caster_states[1][0x131]=4;
            CHECK(remote_state_ui_transition(caster_states[1],FALSE) && update_fault && entries[1].remote_state_ui_acquired);
            *(void **)(image+UI_GLOBAL)=fake_ui; update_fault=FALSE;
            caster_states[1][0x133]|=8;
            invoke_state_ui_bridge(caster_states[1],FALSE); /* Never decrement unknown native UI ownership. */
            CHECK(update_fault && entries[1].remote_state_ui_acquired && *(uint32_t *)(fake_ui+0x54)==3);
            caster_states[1][0x133]&=~8; update_fault=FALSE;
            CHECK(remote_state_ui_transition(caster_states[1],FALSE));
            CHECK(SudekiMpLeaveSpiritInstance(remote));
            remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
            caster_states[1][0x131]=1; CHECK(remote_state_ui_transition(caster_states[1],TRUE));
            neutral=SudekiMpEnterSpiritInstance(NULL); CHECK(neutral);
            caster_states[1][0x131]=4;
            CHECK(remote_state_ui_transition(caster_states[1],FALSE) && update_fault);
            CHECK(entries[1].remote_state_ui_acquired && *(uint32_t *)(fake_ui+0x54)==3);
            update_fault=FALSE; CHECK(SudekiMpLeaveSpiritInstance(neutral));
            CHECK(remote_state_ui_transition(caster_states[1],FALSE));
            CHECK(SudekiMpLeaveSpiritInstance(remote));
            state_ui_acquire_resume=resume_a; state_ui_release_resume=resume_r;
        }
        CHECK(!SudekiMpEnableSpiritInstanceRemoteSkillUi(&instances[0])); /* Local is not eligible. */
        caster_skills[1][0x6c]=1;
        CHECK(!SudekiMpEnableSpiritInstanceRemoteSkillUi(&instances[1]));
        caster_skills[1][0x6c]=0;
        CHECK(SudekiMpEnableSpiritInstanceRemoteSkillUi(&instances[1]));
        CHECK(!SudekiMpEnableSpiritInstanceRemoteSkillUi(&instances[1]));
        CHECK(!SudekiMpEnableSpiritInstanceRemoteSkillInput(&instances[0],caster_actors[0],local_input_witness));
        CHECK(!SudekiMpEnableSpiritInstanceRemoteSkillInput(&instances[1],caster_actors[1],caster_witness));
        CHECK(!SudekiMpEnableSpiritInstanceRemoteSkillInput(&instances[1],caster_actors[0],NULL));
        caster_skills[1][0x6c]=1;
        CHECK(!SudekiMpEnableSpiritInstanceRemoteSkillInput(&instances[1],caster_actors[0],local_input_witness));
        caster_skills[1][0x6c]=0;
        local_witness_valid=FALSE;
        CHECK(!SudekiMpEnableSpiritInstanceRemoteSkillInput(&instances[1],caster_actors[0],local_input_witness));
        local_witness_valid=TRUE;
        *(void **)(fake_controller+0x248)=caster_actors[1];
        CHECK(!SudekiMpEnableSpiritInstanceRemoteSkillInput(&instances[1],caster_actors[0],local_input_witness));
        *(void **)(fake_controller+0x248)=caster_actors[0];
        CHECK(SudekiMpEnableSpiritInstanceRemoteSkillInput(&instances[1],caster_actors[0],local_input_witness));
        CHECK(!SudekiMpEnableSpiritInstanceRemoteSkillInput(&instances[1],caster_actors[0],local_input_witness));
        skill_input_resumes[0]=skill_input_resumes[1]=fake_input_resume;
        {
            const uint8_t code[]={0x80,0x7c,0x24,4,0,0x75,0x16,0xd9,5,0,0,0,0,
                0x83,0x89,0xd0,1,0,0,2,0xd9,0x99,0xd8,1,0,0,0xc2,4,0,
                0xa1,0,0,0,0,0x83,0xa1,0xd0,1,0,0,0xfd,0xd9,0x80,0,0xa,0,0,
                0xd9,0x99,0xd8,1,0,0,0xc2,4,0};
            SkillTargetingFunction target=(SkillTargetingFunction)(image+0x29570);
            DWORD previous_protection,ignored_protection;
            DWORD filter_protection,edge_protection;
            for(unsigned int k=0;k<2;++k) {
                skill_filter_entry(k,image+skill_filter_sites[k]);
                image[skill_filter_sites[k]+13]=0xe8;
                *(int32_t *)(image+skill_filter_sites[k]+14)=0x290d0-(skill_filter_sites[k]+18);
                memcpy(image+skill_filter_sites[k]+18,"\x5e\xc3",2);
            }
            memcpy(image+0x290d0,"\xff\x86\x88\x00\x00\x00\xc3",7);
            CHECK(VirtualProtect(image+0x8ac0,0x40,PAGE_EXECUTE_READWRITE,&filter_protection));
            CHECK(VirtualProtect(image+0x290d0,7,PAGE_EXECUTE_READWRITE,&edge_protection));
            memcpy(image+0x29570,code,sizeof(code));
            memcpy(image+0x29610,target_predicate_body,sizeof(target_predicate_body));
            *(void **)(image+0x29579)=image+0x2e35cc;
            *(void **)(image+0x2958e)=image+CAST_GATE_GLOBAL;
            *(float *)(image+0x2e35cc)=-1.f;
            CHECK(VirtualProtect(image+0x29570,sizeof(code),PAGE_EXECUTE_READWRITE,&previous_protection));
            image[0x29598]^=1;
            CHECK(!SudekiMpEnableSpiritInstanceSkillTargeting());
            image[0x29598]^=1;
            image[0x8aed]^=1;
            CHECK(!SudekiMpEnableSpiritInstanceSkillTargeting());
            CHECK(!skill_targeting_hook.installed && !skill_filter_hooks[0].installed);
            image[0x8aed]^=1;
            CHECK(SudekiMpEnableSpiritInstanceSkillTargeting());
            CHECK(!SudekiMpEnableSpiritInstanceSkillTargeting());
            {
                ControllerFilterFunction none=(ControllerFilterFunction)(image+0x8ac0);
                ControllerFilterFunction all=(ControllerFilterFunction)(image+0x8ae0);
                *(uint32_t *)(fake_controller+0x84)=2; *(uint32_t *)(fake_controller+0x88)=0;
                remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
                none(fake_controller); all(fake_controller);
                CHECK(*(uint32_t *)(fake_controller+0x84)==2 && !*(uint32_t *)(fake_controller+0x88));
                local=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(local);
                none(fake_controller);
                CHECK(!*(uint32_t *)(fake_controller+0x84) && *(uint32_t *)(fake_controller+0x88)==1);
                CHECK(SudekiMpLeaveSpiritInstance(local));
                all(fake_controller); /* A peer cannot release a local casting filter. */
                CHECK(!*(uint32_t *)(fake_controller+0x84) && *(uint32_t *)(fake_controller+0x88)==1);
                local_witness_valid=FALSE; none(fake_controller);
                CHECK(update_fault && *(uint32_t *)(fake_controller+0x88)==1);
                local_witness_valid=TRUE; update_fault=FALSE;
                *(uint32_t *)(foreign_controller+0x84)=2; all(foreign_controller);
                CHECK(update_fault && *(uint32_t *)(foreign_controller+0x84)==2);
                update_fault=FALSE; CHECK(SudekiMpLeaveSpiritInstance(remote));
                all(fake_controller);
                CHECK(*(uint32_t *)(fake_controller+0x84)==1 && *(uint32_t *)(fake_controller+0x88)==2);
            }
            *(uint32_t *)(fake_controller+0x1d0)=11;
            *(float *)(fake_controller+0x1d8)=19.f;
            remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
            target(fake_controller,1); target(fake_controller,0);
            CHECK(*(uint32_t *)(fake_controller+0x1d0)==11 && *(float *)(fake_controller+0x1d8)==19.f);
            local=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(local);
            target(fake_controller,1);
            CHECK(*(uint32_t *)(fake_controller+0x1d0)==9);
            CHECK(SudekiMpLeaveSpiritInstance(local));
            target(fake_controller,0); /* Remote cleanup cannot unlock local targeting. */
            CHECK(*(uint32_t *)(fake_controller+0x1d0)==9);
            local_witness_valid=FALSE;
            target(fake_controller,0);
            CHECK(update_fault && *(uint32_t *)(fake_controller+0x1d0)==9);
            local_witness_valid=TRUE; update_fault=FALSE;
            CHECK(SudekiMpLeaveSpiritInstance(remote));
            target(fake_controller,0); /* Unrelated native setter remains native. */
            CHECK(*(uint32_t *)(fake_controller+0x1d0)==11 && *(float *)(fake_controller+0x1d8)==-1.f);
            remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
            target(fake_controller,1); target(fake_controller,0);
            CHECK(*(uint32_t *)(fake_controller+0x1d0)==11 && *(float *)(fake_controller+0x1d8)==-1.f);
            *(uint32_t *)(foreign_controller+0x1d0)=0x59;
            target(foreign_controller,1);
            CHECK(update_fault && *(uint32_t *)(foreign_controller+0x1d0)==0x59);
            update_fault=FALSE;
            CHECK(SudekiMpLeaveSpiritInstance(remote));
            CHECK(VirtualProtect(image+0x29570,sizeof(code),previous_protection,&ignored_protection));
            CHECK(VirtualProtect(image+0x8ac0,0x40,filter_protection,&ignored_protection));
            CHECK(VirtualProtect(image+0x290d0,7,edge_protection,&ignored_protection));
        }
        {
            void *tasks[2]={caster_skills[0],caster_skills[1]};
            uint8_t phase=0; uint16_t remaining=0;
            SkillTargetingFunction target=(SkillTargetingFunction)(image+0x29570);
            SkillTargetPredicate wait=(SkillTargetPredicate)(image+0x29610);
            DWORD protection,ignored;
            CHECK(VirtualProtect(image+0x29570,0x100,PAGE_EXECUTE_READWRITE,&protection));
            *(float *)(actor_manager+0xa00)=3.f;
            for(unsigned int k=0;k<2;++k) {
                *(void **)(caster_skills[k]+0x74)=&tasks[k];
                CHECK(SudekiMpConfigureSpiritInstanceSkillTiming(&instances[k],FALSE));
                CHECK(SudekiMpBeginSpiritInstanceSkillTiming(&instances[k],1));
            }
            remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
            /* Synchronous native startup has not yet published CSkill+74.
             * Only the constructor-pinned executing task may bridge it. */
            *(void **)(caster_skills[1]+0x74)=NULL;
            starting_task_handle=&tasks[1]; starting_task_known=TRUE;
            target(fake_controller,1); CHECK(wait(fake_controller));
            CHECK(!update_fault);
            starting_task_known=FALSE;
            CHECK(!SudekiMpBeginSpiritInstanceSkillTiming(&instances[1],1));
            *(void **)(caster_skills[1]+0x74)=&tasks[0];
            CHECK(!SudekiMpBeginSpiritInstanceSkillTiming(&instances[1],1));
            *(void **)(caster_skills[1]+0x74)=&tasks[1];
            CHECK(SudekiMpBeginSpiritInstanceSkillTiming(&instances[1],1));
            CHECK(*(float *)(fake_controller+0x1d8)==-1.f);
            CHECK(SudekiMpLeaveSpiritInstance(remote));
            CHECK(!quiescent(&entries[1]));
            CHECK(SudekiMpAdvanceSpiritInstanceSkillTiming(&instances[1],.25f));
            CHECK(SudekiMpObserveSpiritInstanceSkillTiming(&instances[1],1,&phase,&remaining));
            CHECK(phase==SUDEKIMP_SKILL_TARGET_AIMING && remaining==2750);
            /* Native Use may start its script before the success observer:
             * adopting the prepared ID must preserve the existing wait. */
            CHECK(SudekiMpBeginSpiritInstanceSkillTiming(&instances[1],1));
            CHECK(SudekiMpObserveSpiritInstanceSkillTiming(&instances[1],1,&phase,&remaining));
            CHECK(phase==SUDEKIMP_SKILL_TARGET_AIMING && remaining==2750);
            tasks[1]=NULL;
            CHECK(!SudekiMpBeginSpiritInstanceSkillTiming(&instances[1],1));
            tasks[1]=caster_skills[1];
            CHECK(!SudekiMpAdvanceSpiritInstanceSkillTiming(&instances[1],NAN));
            CHECK(!SudekiMpBeginSpiritInstanceSkillTiming(&instances[1],2));
            local=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(local);
            target(fake_controller,1); CHECK(wait(fake_controller));
            CHECK(*(float *)(fake_controller+0x1d8)==3.f);
            CHECK(SudekiMpLeaveSpiritInstance(local));
            for(unsigned int k=0;k<11;++k)
                CHECK(SudekiMpAdvanceSpiritInstanceSkillTiming(&instances[1],.25f));
            remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
            CHECK(!wait(fake_controller)); target(fake_controller,0);
            CHECK(*(float *)(fake_controller+0x1d8)==3.f); /* no cross-caster release */
            CHECK(SudekiMpLeaveSpiritInstance(remote));
            local=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(local);
            CHECK(wait(fake_controller));
            *(float *)(fake_controller+0x1d8)=0.f;
            CHECK(!wait(fake_controller)); target(fake_controller,0);
            CHECK(SudekiMpLeaveSpiritInstance(local));
            {
                void *retired_thread=NULL;
                CHECK(SudekiMpBeginSpiritInstanceSkillTiming(&instances[1],2));
                *(void **)(caster_skills[1]+0x74)=&retired_thread;
                starting_task_known=TRUE;
                remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
                target(fake_controller,1); CHECK(wait(fake_controller) && !update_fault);
                starting_task_known=FALSE;
                CHECK(!SudekiMpBeginSpiritInstanceSkillTiming(&instances[1],2));
                *(void **)(caster_skills[1]+0x74)=&tasks[1];
                CHECK(SudekiMpBeginSpiritInstanceSkillTiming(&instances[1],2));
                target(fake_controller,0);
                CHECK(SudekiMpLeaveSpiritInstance(remote));
            }
            /* Replica expiration cannot release a script; only the admitted
             * host phase does. Duplicate/stale packets cannot restart a wait. */
            entries[0].timing_replica=TRUE;
            CHECK(SudekiMpApplySpiritInstanceSkillTiming(&instances[0],2,1,0));
            local=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(local);
            target(fake_controller,1); CHECK(wait(fake_controller));
            CHECK(SudekiMpLeaveSpiritInstance(local));
            CHECK(SudekiMpApplySpiritInstanceSkillTiming(&instances[0],2,2,1200));
            CHECK(!SudekiMpApplySpiritInstanceSkillTiming(&instances[0],1,3,0));
            CHECK(!SudekiMpApplySpiritInstanceSkillTiming(&instances[0],2,1,0));
            CHECK(!SudekiMpApplySpiritInstanceSkillTiming(&instances[0],2,2,0));
            CHECK(!SudekiMpApplySpiritInstanceSkillTiming(&instances[0],2,4,0));
            CHECK(!SudekiMpApplySpiritInstanceSkillTiming(&instances[0],3,1,0));
            local=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(local);
            *(float *)(fake_controller+0x1d8)=-1.f;
            CHECK(wait(fake_controller));
            CHECK(SudekiMpLeaveSpiritInstance(local));
            CHECK(SudekiMpApplySpiritInstanceSkillTiming(&instances[0],2,3,0));
            local=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(local);
            CHECK(!wait(fake_controller));
            tasks[0]=NULL; CHECK(wait(fake_controller) && update_fault);
            tasks[0]=caster_skills[0]; update_fault=FALSE;
            target(fake_controller,0);
            CHECK(SudekiMpLeaveSpiritInstance(local));
            CHECK(SudekiMpApplySpiritInstanceSkillTiming(&instances[0],3,2,500));
            local=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(local);
            target(fake_controller,1); CHECK(wait(fake_controller));
            CHECK(SudekiMpLeaveSpiritInstance(local));
            CHECK(SudekiMpDrainSpiritInstanceSkillTiming(&instances[0]));
            CHECK(!SudekiMpApplySpiritInstanceSkillTiming(&instances[0],3,3,0));
            CHECK(SudekiMpAdvanceSpiritInstanceSkillTiming(&instances[0],.25f));
            CHECK(entries[0].targeting_phase==2);
            CHECK(SudekiMpAdvanceSpiritInstanceSkillTiming(&instances[0],.25f));
            local=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(local);
            CHECK(!wait(fake_controller)); target(fake_controller,0);
            CHECK(SudekiMpLeaveSpiritInstance(local));
            for(unsigned int k=0;k<2;++k) {
                entries[k].timing_configured=FALSE;
                *(void **)(caster_skills[k]+0x74)=NULL;
            }
            CHECK(VirtualProtect(image+0x29570,0x100,protection,&ignored));
        }
        for(j=0;j<2;++j) {
            *(uint32_t *)(fake_controller+0x1d0)=0x57;
            remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
            invoke_skill_input_bridge(caster_skills[1],TRUE);
            CHECK(entries[1].remote_skill_input_acquired && !quiescent(&entries[1]));
            CHECK(*(uint32_t *)(fake_controller+0x1d0)==0x57 && input_bridge_fpu==1.0f);
            CHECK(input_bridge_eax==(uint32_t)(uintptr_t)fake_controller &&
                bridge_ebx==(uint32_t)(uintptr_t)caster_skills[1] && input_bridge_edi==bridge_ebx);
            CHECK(SudekiMpLeaveSpiritInstance(remote));
            CHECK(!SudekiMpDestroySpiritInstance(&instances[1]));
            local=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(local);
            invoke_skill_input_bridge(caster_skills[0],TRUE);
            CHECK(*(uint32_t *)(fake_controller+0x1d0)==0x55 && input_bridge_fpu==1.0f);
            if(j) invoke_skill_input_bridge(caster_skills[0],FALSE);
            CHECK(SudekiMpLeaveSpiritInstance(local));
            remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
            invoke_skill_input_bridge(caster_skills[1],FALSE);
            CHECK(!entries[1].remote_skill_input_acquired);
            /* Remote completion must neither unlock a still-casting local
             * player nor disable one whose own cleanup completed first. */
            CHECK(*(uint32_t *)(fake_controller+0x1d0)==(j ? 0x57:0x55));
            CHECK(SudekiMpLeaveSpiritInstance(remote));
            if(!j) invoke_skill_input_bridge(caster_skills[0],FALSE);
            CHECK(*(uint32_t *)(fake_controller+0x1d0)==0x57 && !update_fault);
        }
        /* Prior unrelated disabled input is not enabled by remote cleanup. */
        *(uint32_t *)(fake_controller+0x1d0)=0x55;
        remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
        invoke_skill_input_bridge(caster_skills[1],TRUE);
        local_witness_valid=FALSE;
        invoke_skill_input_bridge(caster_skills[1],FALSE);
        CHECK(update_fault && entries[1].remote_skill_input_acquired && *(uint32_t *)(fake_controller+0x1d0)==0x55);
        local_witness_valid=TRUE; update_fault=FALSE; /* Fixture-only repair. */
        *(void **)(image+CONTROLLER_GLOBAL)=foreign_controller;
        *(uint32_t *)(foreign_controller+0x1d0)=0x59;
        invoke_skill_input_bridge(caster_skills[1],FALSE);
        CHECK(update_fault && entries[1].remote_skill_input_acquired && *(uint32_t *)(foreign_controller+0x1d0)==0x59);
        *(void **)(image+CONTROLLER_GLOBAL)=fake_controller; update_fault=FALSE;
        invoke_skill_input_bridge(caster_skills[1],FALSE);
        CHECK(*(uint32_t *)(fake_controller+0x1d0)==0x55 && !entries[1].remote_skill_input_acquired);
        CHECK(SudekiMpLeaveSpiritInstance(remote));
        invoke_skill_input_bridge(caster_skills[1],TRUE); /* Missing routed owner cannot touch local input. */
        CHECK(update_fault && !entries[1].remote_skill_input_acquired && *(uint32_t *)(fake_controller+0x1d0)==0x55);
        update_fault=FALSE;
        remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
        invoke_skill_input_bridge(caster_skills[1],TRUE);
        invoke_skill_input_bridge(caster_skills[1],TRUE);
        CHECK(update_fault && entries[1].remote_skill_input_acquired);
        update_fault=FALSE;
        invoke_skill_input_bridge(caster_skills[1],FALSE);
        invoke_skill_input_bridge(caster_skills[1],FALSE);
        CHECK(update_fault && !entries[1].remote_skill_input_acquired && *(uint32_t *)(fake_controller+0x1d0)==0x55);
        update_fault=FALSE;
        CHECK(SudekiMpLeaveSpiritInstance(remote));
        skill_input_resumes[0]=image+0xb4843; skill_input_resumes[1]=image+0xb4e98;
        CHECK(!remote_ui_transition(TRUE)); /* No scope: native passthrough. */
        remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
        CHECK(!SudekiMpEnableSpiritInstanceRemoteUi(&instances[0])); /* Not idle. */
        expected_lock_ui=0;
        CHECK(invoke_lock(caster_states[1],1,2)==1);
        CHECK(!(caster_states[1][0x133]&8) && *(uint32_t *)(fake_ui+0x54)==3);
        CHECK(remote_ui_transition(TRUE));
        CHECK(entries[1].remote_ui_acquired && !quiescent(&entries[1]));
        CHECK(*(uint32_t *)(fake_ui+0x54)==3);
        local=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(local);
        expected_lock_ui=1;
        CHECK(invoke_lock(caster_states[0],1,2)==1);
        CHECK(!remote_ui_transition(TRUE)); /* Local recomputation remains native. */
        ++*(uint32_t *)(fake_ui+0x54); /* Model that native explicit acquisition. */
        CHECK(*(uint32_t *)(fake_ui+0x54)==5 && entries[1].remote_ui_acquired);
        neutral=SudekiMpEnterSpiritInstance(NULL); CHECK(neutral);
        CHECK(!remote_ui_transition(FALSE)); /* Unrelated cleanup remains native. */
        CHECK(SudekiMpLeaveSpiritInstance(neutral));
        CHECK(SudekiMpLeaveSpiritInstance(local));
        CHECK(participant_unlock(caster_states[1],2)==1);
        CHECK(remote_ui_transition(FALSE));
        CHECK(!entries[1].remote_ui_acquired && *(uint32_t *)(fake_ui+0x54)==5);
        CHECK(SudekiMpLeaveSpiritInstance(remote));
        local=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(local);
        CHECK(participant_unlock(caster_states[0],2)==1);
        CHECK(!remote_ui_transition(FALSE)); --*(uint32_t *)(fake_ui+0x54);
        CHECK(SudekiMpLeaveSpiritInstance(local));
        CHECK(*(uint32_t *)(fake_ui+0x54)==3 && !update_fault);
        /* Two complete cycles: no count drift, including a zero native count. */
        *(uint32_t *)(fake_ui+0x54)=0;
        ui_resumes[0]=ui_resumes[1]=fake_ui_resume;
        ui_trampolines[0]=fake_ui_acquire_native;
        ui_trampolines[1]=fake_ui_release_native;
        for(j=0;j<2;++j) {
            remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
            invoke_ui_bridge(TRUE);
            CHECK(entries[1].remote_ui_acquired && bridge_esi==(uint32_t)(uintptr_t)fake_ui && bridge_ebx==1 && bridge_ebp==UINT_MAX);
            invoke_ui_bridge(FALSE);
            CHECK(!entries[1].remote_ui_acquired && bridge_esi==(uint32_t)(uintptr_t)fake_ui && bridge_ebx==77 && bridge_ebp==UINT_MAX);
            CHECK(SudekiMpLeaveSpiritInstance(remote));
            CHECK(*(uint32_t *)(fake_ui+0x54)==0 && !update_fault);
        }
        invoke_ui_bridge(TRUE); /* Neutral passthrough executes native delta. */
        CHECK(*(uint32_t *)(fake_ui+0x54)==1 && bridge_ebx==1);
        invoke_ui_bridge(FALSE);
        CHECK(*(uint32_t *)(fake_ui+0x54)==0 && bridge_ebx==77);
        for(j=0;j<2;++j) ui_trampolines[j]=ui_hooks[j].trampoline;
        ui_resumes[0]=image+0x100e4; ui_resumes[1]=image+0x10ff4;
        /* Ordinary skill entry/cleanup has its OWN obligation, and must not
         * change a local or unrelated UI counter or invoke its recomputation.
         * Execute both bridge paths, including explicit native-call replay. */
        native_ui_recompute=fake_ui_recompute;
        skill_ui_resumes[0]=skill_ui_resumes[1]=fake_ui_resume;
        ui_recompute_calls=0;
        *(uint32_t *)(fake_ui+0x54)=3;
        for(j=0;j<2;++j) {
            remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
            invoke_skill_ui_bridge(caster_skills[1],TRUE);
            CHECK(entries[1].remote_skill_ui_acquired && !quiescent(&entries[1]));
            CHECK(!entries[1].remote_ui_acquired && ui_recompute_calls==j*2);
            CHECK(bridge_esi==(uint32_t)(uintptr_t)fake_ui &&
                bridge_ebx==(uint32_t)(uintptr_t)caster_skills[1] && bridge_ebp==UINT_MAX);
            CHECK(SudekiMpLeaveSpiritInstance(remote));
            CHECK(!SudekiMpDestroySpiritInstance(&instances[1]));
            local=SudekiMpEnterSpiritInstance(&instances[0]); CHECK(local);
            invoke_skill_ui_bridge(caster_skills[0],TRUE);
            CHECK(*(uint32_t *)(fake_ui+0x54)==4 && ui_recompute_calls==j*2+1);
            CHECK(SudekiMpLeaveSpiritInstance(local));
            remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
            invoke_skill_ui_bridge(caster_skills[1],FALSE);
            CHECK(!entries[1].remote_skill_ui_acquired && *(uint32_t *)(fake_ui+0x54)==4);
            CHECK(SudekiMpLeaveSpiritInstance(remote));
            /* Neutral/unbound native cleanup is unchanged, including EBP=-1. */
            invoke_skill_ui_bridge(caster_skills[0],FALSE);
            CHECK(*(uint32_t *)(fake_ui+0x54)==3 && ui_recompute_calls==j*2+2);
        }
        remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
        invoke_skill_ui_bridge(caster_skills[1],TRUE);
        *(void **)(caster_actors[1]+0xd8)=caster_skills[0];
        invoke_skill_ui_bridge(caster_skills[1],FALSE);
        CHECK(update_fault && entries[1].remote_skill_ui_acquired && *(uint32_t *)(fake_ui+0x54)==3);
        *(void **)(caster_actors[1]+0xd8)=caster_skills[1];
        update_fault=FALSE; /* Fixture repair, never a production recovery path. */
        invoke_skill_ui_bridge(caster_skills[1],FALSE);
        CHECK(SudekiMpLeaveSpiritInstance(remote));
        invoke_skill_ui_bridge(caster_skills[1],TRUE); /* Unscoped remote must NOT mutate native UI. */
        CHECK(update_fault && !entries[1].remote_skill_ui_acquired && *(uint32_t *)(fake_ui+0x54)==3);
        update_fault=FALSE;
        remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
        invoke_skill_ui_bridge(caster_skills[1],TRUE);
        invoke_skill_ui_bridge(caster_skills[1],TRUE); /* Duplicate doesn't acquire twice. */
        CHECK(update_fault && entries[1].remote_skill_ui_acquired && ui_recompute_calls==4);
        update_fault=FALSE;
        invoke_skill_ui_bridge(caster_skills[1],FALSE);
        CHECK(SudekiMpLeaveSpiritInstance(remote));
        CHECK(!update_fault && *(uint32_t *)(fake_ui+0x54)==3);
        *(uint32_t *)(fake_ui+0x54)=0;
        native_ui_recompute=image+0x9e560;
        skill_ui_resumes[0]=image+0xb494c; skill_ui_resumes[1]=image+0xb4f56;
        /* Unknown/replaced UI owner: no counter mutation, no lease retirement. */
        remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
        CHECK(remote_ui_transition(TRUE));
        *(void **)(image+UI_GLOBAL)=foreign_ui;
        CHECK(remote_ui_transition(FALSE) && update_fault && entries[1].remote_ui_acquired);
        CHECK(*(uint32_t *)(foreign_ui+0x54)==0 && *(uint32_t *)(fake_ui+0x54)==0);
        *(void **)(image+UI_GLOBAL)=fake_ui;
        update_fault=FALSE; /* Test-only repair; production never clears an active fault. */
        CHECK(remote_ui_transition(FALSE));
        CHECK(remote_ui_transition(FALSE) && update_fault); /* Duplicate release quarantines. */
        update_fault=FALSE;
        CHECK(SudekiMpLeaveSpiritInstance(remote));
        remote=SudekiMpEnterSpiritInstance(&instances[1]); CHECK(remote);
        CHECK(remote_ui_transition(TRUE));
        CHECK(remote_ui_transition(TRUE) && update_fault && entries[1].remote_ui_acquired);
        update_fault=FALSE; CHECK(remote_ui_transition(FALSE));
        CHECK(SudekiMpLeaveSpiritInstance(remote));
    }
    CHECK(SudekiMpDestroySpiritInstance(&instances[0]));
    CHECK(SudekiMpDestroySpiritInstance(&instances[1]));
    CHECK(skill_targeting_hook.installed && native_skill_targeting);
    image[0x29571]^=1;
    image[0x8ae1]^=1;
    CHECK(!SudekiMpResetSpiritInstanceAbi() && skill_targeting_hook.installed && native_skill_targeting && instance_image);
    image[0x29571]^=1;
    CHECK(!SudekiMpResetSpiritInstanceAbi() && !skill_targeting_hook.installed &&
        skill_filter_hooks[1].installed && native_skill_filters[1] && instance_image);
    image[0x8ae1]^=1;
    CHECK(SudekiMpResetSpiritInstanceAbi());
    CHECK(!skill_filter_hooks[0].installed && !skill_filter_hooks[1].installed &&
        !native_skill_filters[0] && !native_skill_filters[1]);
    /* A foreign hook blocks reset; original callbacks survive a partial
     * reverse restoration and reset can be retried without reinstalling. */
    setup();
    *(void **)(image+update_slots[0])=(void *)0x1234;
    CHECK(!SudekiMpResetSpiritInstanceAbi());
    CHECK(instance_image==image && original_updates[0]==fake_camera_update);
    CHECK(!SudekiMpInitializeSpiritInstanceAbi((HMODULE)image,witness));
    *(void **)(image+update_slots[0])=camera_update;
    CHECK(SudekiMpResetSpiritInstanceAbi());
    setup();
    image[0xfcd7]^=1;
    CHECK(!SudekiMpResetSpiritInstanceAbi() && native_participant_lock && instance_image);
    image[0xfcd7]^=1;
    CHECK(SudekiMpResetSpiritInstanceAbi());
    setup();
    image[0x100d2]^=1;
    CHECK(!SudekiMpResetSpiritInstanceAbi() && ui_trampolines[0] && instance_image);
    image[0x100d2]^=1;
    CHECK(SudekiMpResetSpiritInstanceAbi());
    for(i=0;i<2;++i) {
        uint32_t site=i ? 0xb4e91:0xb483c;
        setup();
        image[site+1]^=1;
        CHECK(!SudekiMpResetSpiritInstanceAbi());
        CHECK(instance_image==image && skill_input_hooks[i].installed &&
            skill_input_resumes[i] && original_updates[0]==fake_camera_update);
        image[site+1]^=1;
        CHECK(SudekiMpResetSpiritInstanceAbi());
        CHECK(!instance_image && !skill_input_hooks[i].installed);
    }
    for(i=0;i<2;++i) {
        uint32_t site=i ? 0xe46c6:0xe45aa;
        setup(); image[site+1]^=1;
        CHECK(!SudekiMpResetSpiritInstanceAbi() && state_ui_hooks[i].installed && state_ui_trampolines[i]);
        image[site+1]^=1;
        CHECK(SudekiMpResetSpiritInstanceAbi() && !instance_image);
    }
    persistent_ui_tests();
    shared_ssp_tests();
    named_tests();
    selection_tests();
    inactive_native_update_tests();
    rebind_local_tests();
    {
        const uint32_t sites[]={MANAGER_CTOR,CAMERA_CTOR,MANAGER_INIT,CAMERA_INIT,
            MANAGER_DELETE,CAMERA_DELETE,SOUL_DELETE,0x78d0d,0x78d18,0x79c53,0x79c5e,
            MANAGER_CTOR+0x40,CAMERA_CTOR+0x97,MANAGER_VTABLE,CAMERA_VTABLE,SOUL_VTABLE,
            MANAGER_DELETE+5,CAMERA_DELETE+3,SOUL_DELETE+5,
            CAMERA_VTABLE+4,SOUL_VTABLE+4,0x11bd0,0x12adf0,
            0x11be0,0x11bf0,0x11bfb,0x11e1f,0x12ae03,0x12ae09,0x12b048,
            MANAGER_VTABLE+4,0xf900,0xf902,0xf906,0xf90f,0xf921,0xf977,0xf969,
            0x1061d0,0x106266,0x106272,0x106281,0x79df7,
            0xfcd6,0x10f36,0xe4460,0xe45d0,0xf914,0xf95b,0xf95c,0xf960,0xf96e,
            0x2f8d8,0x2f8da,0x2f8de,0x2f8e1,0x10a1c,0x10a1e,0x10a22,
            0xb4bcf,0xb4bd1,0xb4bd5,0x10156,0x10157,0x1015b,
            0xb4b68,0xb4b69,0xb4b6d,0x11114,0x11115,0x11125,0xb47f3,0xb47f4,0xb47f8,
            0x9c528,0x9c52b,0x9c5dd,0x9c5df,0x100d1,0x100d3,0x100d7,0x100df,
            0x10fe6,0x10fe8,0x10fec,0x10fef,0xe459d,0xe45aa,0xe45ab,0xe45bc,
            0xe46c6,0xe46cd,0xe46ce,0xe46e1,0xe45c1,0xe45c8,0xe46e6,
            0xb4835,0xb4836,0xb483a,0xb483c,0xb4843,0xb4844,
            0xb4e8c,0xb4e8d,0xb4e91,0xb4e98,0xb4e99,0x278d9,0x279ce,0x2c9f84};
        for(i=0;i<sizeof(sites)/sizeof(sites[0]);++i) {
            image[sites[i]]^=1;
            CHECK(!SudekiMpInitializeSpiritInstanceAbi((HMODULE)image,witness));
            image[sites[i]]^=1;
            CHECK(SudekiMpInitializeSpiritInstanceAbi((HMODULE)image,witness));
            CHECK(SudekiMpResetSpiritInstanceAbi());
        }
    }
    VirtualFree(image,0,MEM_RELEASE);
    printf("Spirit instance ABI tests: %s\n",failures ? "FAIL":"PASS");
    return failures ? 1:0;
}
