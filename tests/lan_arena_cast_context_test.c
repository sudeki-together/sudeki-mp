#include <stdio.h>
#include <stdarg.h>
#include "../src/hooks/lan_arena_cast_context.c"

static int failures;
#define CHECK(x) do { if(!(x)) { printf("FAIL %d: %s\n",__LINE__,#x); ++failures; } } while(0)
void SudekiMpLogFormat(const char *format,...) {
    if(!strncmp(format,"lan_cast_ui ",12)) SetLastError(999);
}
static uint8_t *image;
static uint8_t actors[2][0xe0], skills[2][0x78], scripts[2][0x40], threads[4][0x50];
static uint32_t handles[4][2];
static uint64_t session=7;
static unsigned int expected_actor;
static BOOL spawn_child, nested;
static BOOL refuse_enter, refuse_leave, nested_step, step_dispatch, retained_binding;
static BOOL check_starting_task;
static void *routed_actor, *saved_route[16];
static unsigned int route_depth, step_calls;
static uint32_t route_enter(const SudekiMpLanCastOwner *owner) {
    if(refuse_enter) { SetLastError(ERROR_BUSY); return 0; }
    CHECK(route_depth<16);
    saved_route[route_depth++]=routed_actor;
    routed_actor=owner ? owner->actor:NULL;
    CHECK(!owner || (owner->session==7 && owner->cast_id));
    return route_depth;
}
static BOOL route_leave(uint32_t cookie) {
    CHECK(cookie==route_depth && route_depth);
    if(refuse_leave) return FALSE;
    routed_actor=saved_route[--route_depth];
    return TRUE;
}
static int __attribute__((fastcall)) fake_step(void *thread,void *edx) {
    void *expected=thread==threads[3] ? NULL:actors[expected_actor];
    CHECK(edx==(void *)0x13579);
    CHECK(routed_actor==expected);
    if(check_starting_task) {
        void *handle=NULL,*actual_thread=NULL;
        BOOL starting=current_cast && current_cast->launching && current_cast->owner.kind==1;
        CHECK(SudekiMpLanCastContextStartingSkillTask(actors[expected_actor],7,
            skills[expected_actor],&handle,&actual_thread)==starting);
        if(starting) {
            CHECK(handle==handles[expected_actor] && actual_thread==thread);
            CHECK(!SudekiMpLanCastContextStartingSkillTask(actors[expected_actor],8,
                skills[expected_actor],&handle,&actual_thread));
            CHECK(!SudekiMpLanCastContextStartingSkillTask(actors[expected_actor^1u],7,
                skills[expected_actor],&handle,&actual_thread));
            CHECK(!SudekiMpLanCastContextStartingSkillTask(actors[expected_actor],7,
                skills[expected_actor^1u],&handle,&actual_thread));
        }
    }
    ++step_calls;
    ++*(uint32_t *)((uint8_t *)thread+0xc); /* Native fetch side effect. */
    if(nested_step) {
        nested_step=FALSE;
        CHECK(task_step(threads[3],edx)==93);
        CHECK(routed_actor==expected);
    }
    if(step_dispatch) {
        step_dispatch=FALSE;
        CHECK(dispatch(thread,edx,2)==93);
    }
    CHECK(!SudekiMpLanCastContextSetTaskRouting(NULL,NULL));
    SetLastError(1234); return 93;
}
static void __attribute__((stdcall)) camera_binding(void *skill) {
    SudekiMpLanCastOwner owner;
    CHECK(SudekiMpLanCastContextCurrent(&owner));
    CHECK(skill==skills[0] && owner.actor==actors[0]);
    if(spawn_child) {
        void *cell=handles[2];
        spawn_child=FALSE;
        handles[2][0]=(uint32_t)(uintptr_t)threads[2]; handles[2][1]=1;
        created_task(0x9876,&cell);
    }
}
static unsigned int skill_update_calls;
static void __attribute__((thiscall)) fake_skill_update(void *node,float delta) {
    SudekiMpLanCastOwner owner;
    CHECK(node==skills[0]+0x18 && delta==0.125f && routed_actor==actors[0]);
    CHECK(SudekiMpLanCastContextCurrentRetained(&owner) && owner.kind==1);
    CHECK(!SudekiMpLanCastContextActorDrained(actors[1],7)); /* No retirement mid-scope. */
    ++skill_update_calls;
    skills[0][0x6c]=0;
    skill_camera_end(skills[0]); /* Native completion launches its own child. */
    SetLastError(4321);
}
static uint32_t * __attribute__((regparm(1),stdcall,noinline)) fake_constructor(
    uint32_t hash,void *manager,void **out,void *log,uint32_t mode,uint32_t rate) {
    CHECK(hash==0x76543210 && manager==(void *)11 && log==(void *)22);
    CHECK(mode==33 && rate==44);
    handles[0][0]=(uint32_t)(uintptr_t)threads[0]; handles[0][1]=1;
    *out=handles[0]; return handles[0];
}
__attribute__((naked,noinline)) static void fake_submit(void) {
    __asm__ volatile("movl 4(%esp),%eax\n\tmovl %eax,(%esi)\n\tmovl %esi,%eax\n\tret $4\n\t");
}
static void __attribute__((noinline,used)) check_aux_owner(void) {
    SudekiMpLanCastOwner owner;
    CHECK(SudekiMpLanCastContextCurrent(&owner));
    CHECK(owner.actor==actors[expected_actor] && owner.kind==1);
}
__attribute__((naked,noinline)) static void fake_aux_submit(void) {
    __asm__ volatile("call _check_aux_owner\n\tmovl 4(%esp),%eax\n\tmovl %eax,(%esi)\n\t"
        "movl %esi,%eax\n\tret $4\n\t");
}
__attribute__((naked,noinline,regparm(3))) static void **invoke_aux(
    void *script __attribute__((unused)),void *skill __attribute__((unused)),
    void **out __attribute__((unused))) {
    __asm__ volatile("pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tmovl %eax,%edi\n\t"
        "movl %edx,%ebx\n\tmovl %ecx,%esi\n\tpushl $0x76543210\n\tcall _skill_started\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tret\n\t");
}
__attribute__((naked,noinline,regparm(3))) static void invoke_ui(
    void *ui __attribute__((unused)),void *entry __attribute__((unused)),
    float *out __attribute__((unused))) {
    __asm__ volatile("pushl %esi\n\tpushl %edi\n\tmovl %eax,%esi\n\tmovl %ecx,%edi\n\t"
        "fld1\n\tcall *%edx\n\tfstps (%edi)\n\tpopl %edi\n\tpopl %esi\n\tret\n\t");
}
/* Exercise the real register-preserving root bridge, not just its C helpers. */
__attribute__((naked,noinline,regparm(3))) static void **invoke_root(
    void *script __attribute__((unused)),void **out __attribute__((unused)),
    void *argument __attribute__((unused))) {
    __asm__ volatile("pushl %esi\n\tpushl %edi\n\tmovl %eax,%edi\n\tmovl %edx,%esi\n\t"
        "pushl %ecx\n\tcall _skill_root\n\tpopl %edi\n\tpopl %esi\n\tret\n\t");
}

static BOOL witness(void *actor,uint8_t kind,uint64_t *token,uint8_t *type) {
    if(kind!=1 && kind!=2) return FALSE;
    if(actor!=actors[0] && actor!=actors[1]) return FALSE;
    *token=session; *type=actor==actors[0] ? 5:14; return TRUE;
}
static void create(unsigned int id,uint32_t hash) {
    void *cell=handles[id];
    handles[id][0]=(uint32_t)(uintptr_t)threads[id]; handles[id][1]=1;
    created_task(hash,&cell);
}
static int __attribute__((fastcall)) binding(void *thread,void *edx) {
    SudekiMpLanCastOwner owner={0};
    (void)edx;
    if(thread==threads[3]) {
        CHECK(!SudekiMpLanCastContextCurrent(&owner));
    } else {
        if(retained_binding) {
            CHECK(!SudekiMpLanCastContextCurrent(&owner));
            CHECK(SudekiMpLanCastContextCurrentRetained(&owner));
        } else CHECK(SudekiMpLanCastContextCurrent(&owner));
        CHECK(owner.actor==actors[expected_actor]);
        CHECK(owner.session==(retained_binding ? session-1:session) && owner.cast_id!=0);
        if(spawn_child) { spawn_child=FALSE; create(2,0x123); CHECK(handles[2][1]==2); }
        if(nested) {
            unsigned int cookie;
            nested=FALSE;
            CHECK(opcode28(threads[3],NULL)==93);
            CHECK(SudekiMpLanCastContextCurrent(&owner));
            /* Even an unrelated root invoked from a binding masks the parent. */
            cookie=begin_root(NULL,1);
            CHECK(cookie && !SudekiMpLanCastContextCurrent(&owner));
            end_root(cookie);
            CHECK(SudekiMpLanCastContextCurrent(&owner));
        }
    }
    SetLastError(1234);
    return 93;
}
static void fixture(void) {
    unsigned int i;
    static const uint32_t sites[]={ROOT_SKILL,ROOT_SPIRIT,CREATE_DIRECT,CREATE_CHILD,
        SKILL_CAMERA_START_CALL,SKILL_CAMERA_END_CALL,STEP_IMMEDIATE,STEP_SCHEDULED,SKILL_STARTED};
    static const uint32_t targets[]={SUBMIT,SUBMIT,CREATE,CREATE,SKILL_CAMERA_START,SKILL_CAMERA_END,STEP,STEP,SUBMIT};
    memcpy(image+SKILL_STARTED-sizeof(skill_started_prefix),skill_started_prefix,sizeof(skill_started_prefix));
    memcpy(image+SUBMIT,submit_prefix,sizeof(submit_prefix));
    memcpy(image+0x9e560,ui_recompute_prefix,sizeof(ui_recompute_prefix));
    memcpy(image+0x9e566,"\x8b\xe5\x5d\xc3",4); /* Native prologue/epilogue ABI. */
    memcpy(image+CREATE,create_prefix,sizeof(create_prefix));
    memcpy(image+STEP,step_prefix,sizeof(step_prefix));
    memcpy(image+SKILL_UPDATE,skill_update_prefix,sizeof(skill_update_prefix));
    memcpy(image+SKILL_UPDATE+0x67,skill_update_tail,sizeof(skill_update_tail));
    *(void **)(image+SKILL_UPDATE_VTABLE+4)=image+SKILL_UPDATE;
    for(i=0;i<sizeof(sites)/sizeof(sites[0]);++i) {
        int32_t delta=(int32_t)(targets[i]-(sites[i]+5));
        image[sites[i]]=0xe8; memcpy(image+sites[i]+1,&delta,4);
    }
    for(i=0;i<3;++i) *(void **)(image+OPCODE_SLOT+4*i)=image+opcode_rvas[i];
    for(i=0;i<2;++i) {
        const uint8_t camera_tail[]={0x81,0xec,0xac,0,0,0,0x53,0x56,0x57};
        uint8_t *camera=image+(i ? SKILL_CAMERA_END:SKILL_CAMERA_START);
        uint32_t address=(uint32_t)(uintptr_t)(image+0x408d94);
        camera[0]=0xa1; memcpy(camera+1,&address,4);
        memcpy(camera+5,camera_tail,sizeof(camera_tail));
        *(void **)(actors[i]+0x68)=scripts[i];
        *(void **)(actors[i]+0xd8)=skills[i];
        *(void **)(scripts[i]+0x10)=actors[i];
        *(void **)(skills[i]+0x10)=actors[i];
        *(void **)(skills[i]+0x18)=image+SKILL_UPDATE_VTABLE;
    }
}
static void setup(void) {
    CHECK(SudekiMpInstallLanCastContext((HMODULE)image,witness));
    CHECK(SudekiMpLanCastContextPoll());
    original_opcodes[0]=original_opcodes[1]=original_opcodes[2]=binding;
}
int main(void) {
    unsigned int a,b,i;
    SudekiMpLanCastOwner owner;
    image=VirtualAlloc(NULL,0x45f000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if(!image) return 1;
    fixture(); setup();
    {
        typedef uint32_t * (__attribute__((regparm(1),stdcall)) *CreateBridge)(
            uint32_t,void *,void **,void *,uint32_t,uint32_t);
        CreateBridge bridge=(CreateBridge)create_task;
        void *out=NULL;
        original_submit=fake_submit;
        CHECK(invoke_root(scripts[0],&out,(void *)0x76543210)==&out);
        CHECK(out==(void *)0x76543210 && !depth && !current_cast);
        CHECK(!SudekiMpLanCastContextDrained()); /* Await native CSkill acknowledgement. */
        skills[0][0x6c]=1;
        CHECK(SudekiMpLanCastContextPoll());
        skills[0][0x6c]=0;
        CHECK(SudekiMpLanCastContextPoll());
        original_create=(RawFunction)fake_constructor;
        skills[0][0x6c]=1;
        a=begin_root(scripts[0],1);
        CHECK(bridge(0x76543210,(void *)11,&out,(void *)22,33,44)==handles[0]);
        CHECK(out==handles[0] && handles[0][1]==2 && casts[0].tasks==1);
        end_root(a); CHECK(SudekiMpLanCastContextPoll());
        handles[0][0]=0; skills[0][0x6c]=0;
        CHECK(SudekiMpLanCastContextPoll());
    }
    CHECK(!SudekiMpLanCastContextCurrent(&owner));
    skills[0][0x6c]=1;
    a=begin_root(scripts[0],1); CHECK(a && current_cast && depth==1);
    create(0,0xabcd); CHECK(handles[0][1]==2);
    {
        void *empty=NULL;
        created_task(0xdeadbeef,&empty);
        CHECK(!lineage_fault && current_cast->empty_launches==1 && current_cast->tasks==1);
        CHECK(!SudekiMpLanCastContextDrained());
    }
    end_root(a); CHECK(!depth && !current_cast);
    b=begin_root(scripts[1],2); CHECK(b && current_cast);
    create(1,0xeeee); end_root(b);
    CHECK(casts[0].owner.cast_id!=casts[1].owner.cast_id);
    CHECK(!SudekiMpUninstallLanCastContext()); /* Both still live. */
    expected_actor=0; spawn_child=TRUE; nested=TRUE;
    CHECK(opcode29(threads[0],NULL)==93 && GetLastError()==1234);
    CHECK(!current_cast && !depth && !lineage_fault);
    expected_actor=1; CHECK(opcode27(threads[1],NULL)==93);
    expected_actor=0; CHECK(opcode28(threads[2],NULL)==93);
    /* Different completion orders; a finished parent cannot retire its child. */
    handles[0][0]=0; CHECK(SudekiMpLanCastContextPoll());
    CHECK(handles[0][1]==1 && casts[0].tasks==1 && casts[1].tasks==1);
    CHECK(!SudekiMpLanCastContextDrained());
    handles[1][0]=0; CHECK(SudekiMpLanCastContextPoll());
    CHECK(!casts[1].owner.cast_id && casts[0].owner.cast_id);
    CHECK(SudekiMpLanCastContextActorDrained(actors[1],session));
    CHECK(!SudekiMpLanCastContextActorDrained(actors[0],session));
    CHECK(!SudekiMpLanCastContextActorDrained(actors[0],session+1));
    CHECK(!SudekiMpLanCastContextActorDrained(NULL,session));
    CHECK(!SudekiMpLanCastContextActorDrained(actors[1],0));
    CHECK(!SudekiMpUninstallLanCastContext());
    handles[2][0]=0; CHECK(SudekiMpLanCastContextPoll());
    CHECK(casts[0].owner.cast_id && !SudekiMpLanCastContextDrained());
    skills[0][0x6c]=0; CHECK(SudekiMpLanCastContextPoll());
    CHECK(handles[2][1]==1 && !casts[0].owner.cast_id);
    CHECK(SudekiMpLanCastContextDrained());
    CHECK(SudekiMpLanCastContextActorDrained(actors[0],session));
    CHECK(SudekiMpUninstallLanCastContext());

    /* Optional UI attribution cannot change native state or consume x87.
     * It rejects a changed seam, and retains the trampoline on failed restore. */
    setup();
    {
        uint8_t ui[0xc0]={0}, before[0xc0];
        float value=0;
        *(void **)ui=image+0x2caf9c;
        *(void **)(image+0x3c2f88)=ui;
        *(uint32_t *)(ui+0x54)=3;
        memcpy(before,ui,sizeof(ui));
        image[0x9e562]^=1;
        CHECK(!SudekiMpLanCastContextEnableUiTrace() && !ui_trace_hook.installed);
        image[0x9e562]^=1;
        CHECK(SudekiMpLanCastContextEnableUiTrace());
        CHECK(!SudekiMpLanCastContextEnableUiTrace());
        SetLastError(732);
        invoke_ui(ui,image+0x9e560,&value);
        CHECK(value==1.0f && GetLastError()==732 && ui_trace_events==1);
        CHECK(!memcmp(ui,before,sizeof(ui)));
        *(void **)ui=NULL;
        invoke_ui(ui,image+0x9e560,&value);
        CHECK(ui_trace_events==1); /* Foreign UI is not attributed. */
        image[0x9e560]=0x90;
        CHECK(!SudekiMpUninstallLanCastContext() && ui_trace_trampoline && cast_image);
        image[0x9e560]=0xe9;
        CHECK(SudekiMpUninstallLanCastContext() && !ui_trace_trampoline);
        CHECK(!memcmp(image+0x9e560,ui_recompute_prefix,sizeof(ui_recompute_prefix)));
        *(void **)(image+0x3c2f88)=NULL;
    }

    setup(); skills[0][0x6c]=1;
    a=begin_root(scripts[0],1); create(0,1); end_root(a);
    CHECK(SudekiMpLanCastContextPoll());
    /* Session/actor replacement cannot borrow an old task's owner. */
    current_cast=&casts[0]; ++session;
    CHECK(!SudekiMpLanCastContextCurrent(&owner)); --session;
    *(void **)(actors[0]+0x68)=NULL;
    CHECK(!SudekiMpLanCastContextCurrent(&owner));
    *(void **)(actors[0]+0x68)=scripts[0]; current_cast=NULL;
    handles[0][0]=(uint32_t)(uintptr_t)threads[3];
    CHECK(!SudekiMpLanCastContextPoll() && handles[0][1]==2);
    CHECK(!SudekiMpUninstallLanCastContext());
    handles[0][0]=0; skills[0][0x6c]=0; ++session;
    CHECK(SudekiMpUninstallLanCastContext() && handles[0][1]==1);
    --session; /* Disconnected native lifetime can drain without new authority. */

    setup(); skills[0][0x6c]=1; a=begin_root(scripts[0],1);
    {
        uint32_t invalid_handle[2]={0,1};
        void *cell=invalid_handle;
        created_task(0x1234,&cell);
        CHECK(lineage_fault); /* Null task in a non-null result is not an empty launch. */
    }
    end_root(a);
    (void)SudekiMpLanCastContextPoll(); skills[0][0x6c]=0;
    CHECK(SudekiMpUninstallLanCastContext());

    setup(); skills[0][0x6c]=1; a=begin_root(scripts[0],1);
    create(0,11); end_root(a);
    original_skill_camera[0]=original_skill_camera[1]=camera_binding;
    skill_camera_start(skills[0]);
    CHECK(casts[0].skill_active_seen && !casts[0].skill_cleaned);
    handles[0][0]=0;
    CHECK(SudekiMpLanCastContextPoll() && !SudekiMpLanCastContextDrained());
    skills[0][0x6c]=0; spawn_child=TRUE;
    skill_camera_end(skills[0]);
    CHECK(casts[0].skill_cleaned && casts[0].tasks==1 && handles[2][1]==2);
    CHECK(!SudekiMpLanCastContextDrained());
    expected_actor=0; CHECK(opcode28(threads[2],NULL)==93);
    handles[2][0]=0;
    CHECK(SudekiMpLanCastContextDrained() && SudekiMpUninstallLanCastContext());

    /* Full VM-step routing happens before fetch, unlike opcode attribution.
     * Prove both casters, nested neutral work, transient entry retry, retained
     * native lifetime after disconnect, and stale-handle rejection. */
    setup(); original_step=fake_step;
    CHECK(!SudekiMpLanCastContextSetTaskRouting(route_enter,NULL));
    CHECK(SudekiMpLanCastContextSetTaskRouting(route_enter,route_leave));
    a=begin_root(scripts[0],1); create(0,11);
    expected_actor=0; check_starting_task=TRUE;
    CHECK(task_step(threads[0],(void *)0x13579)==93);
    end_root(a);
    CHECK(task_step(threads[0],(void *)0x13579)==93); /* No longer a starting task. */
    check_starting_task=FALSE; step_calls=0;
    skills[0][0x6c]=1; CHECK(SudekiMpLanCastContextPoll());
    skills[0][0x6c]=0; CHECK(SudekiMpLanCastContextPoll());
    CHECK(casts[0].skill_cleaned && casts[0].tasks==1); /* Task still pins lifetime. */
    b=begin_root(scripts[1],2); create(1,22); end_root(b);
    CHECK(!SudekiMpLanCastContextSetTaskRouting(NULL,NULL));
    expected_actor=0; nested_step=TRUE;
    CHECK(task_step(threads[0],(void *)0x13579)==93 && GetLastError()==1234);
    CHECK(step_calls==2 && !routed_actor && !route_depth && !task_scope_count && !depth);
    expected_actor=1; CHECK(task_step(threads[1],(void *)0x13579)==93);
    {
        uint32_t ip=*(uint32_t *)(threads[1]+0xc);
        unsigned int calls=step_calls;
        refuse_enter=TRUE;
        CHECK(task_step(threads[1],(void *)0x13579)==2);
        CHECK(*(uint32_t *)(threads[1]+0xc)==ip && step_calls==calls);
        CHECK(!current_cast && !depth && !task_scope_count && !route_depth);
        refuse_enter=FALSE;
        CHECK(task_step(threads[1],(void *)0x13579)==93);
        CHECK(*(uint32_t *)(threads[1]+0xc)==ip+1 && step_calls==calls+1);
        handles[1][0]=(uint32_t)(uintptr_t)threads[3];
        CHECK(task_step(threads[1],(void *)0x13579)==2 && step_calls==calls+1);
        handles[1][0]=(uint32_t)(uintptr_t)threads[1];
        ++session; retained_binding=TRUE; spawn_child=TRUE; step_dispatch=TRUE;
        CHECK(task_step(threads[1],(void *)0x13579)==93); /* Drain old context, no new action. */
        CHECK(handles[2][1]==2 && tasks[2].cast_id==tasks[1].cast_id);
        retained_binding=FALSE; --session;
    }
    handles[1][0]=0; CHECK(SudekiMpLanCastContextPoll());
    CHECK(!SudekiMpLanCastContextActorDrained(actors[1],session));
    handles[2][0]=0; CHECK(SudekiMpLanCastContextPoll());
    CHECK(SudekiMpLanCastContextActorDrained(actors[1],session));
    handles[0][0]=0; CHECK(SudekiMpLanCastContextDrained());
    CHECK(SudekiMpLanCastContextSetTaskRouting(NULL,NULL));
    CHECK(SudekiMpUninstallLanCastContext());

    /* Native UpdateMgr cleanup uses the same actor scope and owns any child
     * it creates after the root task completed. Exercise the installed ABI. */
    setup(); original_skill_update=fake_skill_update;
    CHECK(SudekiMpLanCastContextSetTaskRouting(route_enter,route_leave));
    original_skill_camera[1]=camera_binding;
    skills[0][0x6c]=1; a=begin_root(scripts[0],1); create(0,33); end_root(a);
    CHECK(SudekiMpLanCastContextPoll());
    handles[0][0]=0; CHECK(SudekiMpLanCastContextPoll());
    refuse_enter=TRUE;
    ((SkillUpdateFunction)*(void **)(image+SKILL_UPDATE_VTABLE+4))(skills[0]+0x18,0.125f);
    CHECK(!skill_update_calls && skills[0][0x6c]);
    refuse_enter=FALSE; spawn_child=TRUE;
    ((SkillUpdateFunction)*(void **)(image+SKILL_UPDATE_VTABLE+4))(skills[0]+0x18,0.125f);
    CHECK(skill_update_calls==1 && GetLastError()==4321 && !depth && !route_depth);
    CHECK(casts[0].skill_cleaned && handles[2][1]==2);
    CHECK(!SudekiMpLanCastContextActorDrained(actors[0],session));
    handles[2][0]=0; CHECK(SudekiMpLanCastContextDrained());
    CHECK(SudekiMpLanCastContextSetTaskRouting(NULL,NULL));
    CHECK(SudekiMpUninstallLanCastContext());

    /* Native thread memory may be reused between frame polls. The old pinned
     * handle must positively report null before a different handle can own
     * that address. Keep cast retirement deferred until its children drain. */
    setup();
    a=begin_root(scripts[0],2); create(0,11); end_root(a);
    {
        void *out=handles[1];
        uint32_t first=tasks[0].cast_id;
        handles[0][0]=0; /* Native terminal, before the next Poll. */
        b=begin_root(scripts[1],2); /* begin_root would Poll: simulate nested construction below instead. */
        end_root(b);
        CHECK(handles[0][1]==1);
        a=begin_root(scripts[0],2); create(0,22);
        handles[0][0]=0;
        handles[1][0]=(uint32_t)(uintptr_t)threads[0]; handles[1][1]=1;
        created_task(33,&out);
        CHECK(!lineage_fault && handles[0][1]==1 && handles[1][1]==2);
        CHECK(tasks[0].handle==handles[1] && tasks[0].thread==threads[0]);
        CHECK(tasks[0].cast_id!=first && current_cast->tasks==1);
        end_root(a);
        handles[1][0]=0;
        CHECK(SudekiMpLanCastContextDrained());
    }
    CHECK(SudekiMpUninstallLanCastContext());
    setup();
    a=begin_root(scripts[0],2); create(0,44);
    {
        void *out=handles[1];
        handles[1][0]=(uint32_t)(uintptr_t)threads[0]; handles[1][1]=1;
        created_task(55,&out); /* Old pinned handle is STILL live. */
        CHECK(lineage_fault && handles[0][1]==2 && handles[1][1]==1);
    }
    end_root(a); handles[0][0]=0;
    CHECK(SudekiMpUninstallLanCastContext());

    /* OnSkillStarted inherits its own actor's existing root before +6c=1.
     * Both actors can have retained lineages; no last-caster inference. */
    setup();
    for(i=0;i<2;++i) {
        skills[i][0x6c]=0;
        a=begin_root(scripts[i],1); create(i,71+i); end_root(a);
    }
    for(i=0;i<2;++i) {
        void *out=NULL;
        uint32_t id=casts[i].owner.cast_id;
        expected_actor=i; original_submit=fake_aux_submit;
        CHECK(invoke_aux(scripts[i],skills[i],&out)==&out && out==(void *)0x76543210);
        a=begin_skill_started(scripts[i],skills[i]);
        CHECK(SudekiMpLanCastContextCurrent(&owner) && owner.actor==actors[i] && owner.cast_id==id);
        create(i+2,81+i); end_root(a);
        CHECK(casts[i].owner.cast_id==id && casts[i].tasks==2 && casts[i].roots==1);
        skills[i][0x6c]=1;
    }
    CHECK(SudekiMpLanCastContextPoll());
    handles[0][0]=handles[1][0]=0;
    skills[0][0x6c]=skills[1][0x6c]=0;
    CHECK(SudekiMpLanCastContextPoll());
    CHECK(!SudekiMpLanCastContextActorDrained(actors[0],session));
    CHECK(!SudekiMpLanCastContextActorDrained(actors[1],session));
    handles[2][0]=0;
    CHECK(SudekiMpLanCastContextPoll() && SudekiMpLanCastContextActorDrained(actors[0],session));
    CHECK(!SudekiMpLanCastContextActorDrained(actors[1],session));
    handles[3][0]=0;
    CHECK(SudekiMpUninstallLanCastContext());
    /* No root, nested foreign script, and mismatched owner cannot borrow a cast. */
    setup();
    a=begin_skill_started(scripts[0],skills[0]);
    CHECK(!SudekiMpLanCastContextCurrent(&owner)); end_root(a);
    a=begin_root(scripts[0],1);
    b=begin_skill_started(scripts[1],skills[1]);
    CHECK(!SudekiMpLanCastContextCurrent(&owner)); end_root(b);
    CHECK(SudekiMpLanCastContextCurrent(&owner) && owner.actor==actors[0]); end_root(a);
    a=begin_skill_started(scripts[1],skills[0]);
    CHECK(lineage_fault && !SudekiMpLanCastContextCurrent(&owner)); end_root(a);
    skills[0][0x6c]=1; (void)SudekiMpLanCastContextPoll();
    skills[0][0x6c]=0;
    CHECK(SudekiMpUninstallLanCastContext());
    /* An altered installed auxiliary seam retains its live dependencies. */
    setup(); image[SKILL_STARTED]=0x90;
    CHECK(!SudekiMpUninstallLanCastContext() && original_submit && cast_image);
    image[SKILL_STARTED]=0xe8;
    CHECK(SudekiMpUninstallLanCastContext());

    /* Every patch seam rejects alteration and rolls earlier installations back. */
    {
        const uint32_t sites[]={CREATE_DIRECT,CREATE_CHILD,ROOT_SKILL,ROOT_SPIRIT,SUBMIT,CREATE,
            SKILL_CAMERA_START_CALL,SKILL_CAMERA_END_CALL,SKILL_CAMERA_START,SKILL_CAMERA_END,
            SKILL_CAMERA_START+1,SKILL_CAMERA_END+5,STEP_IMMEDIATE,STEP_SCHEDULED,STEP,STEP+20,
            SKILL_UPDATE,SKILL_UPDATE+0x67,SKILL_UPDATE+0x6c,SKILL_UPDATE_VTABLE+4,
            SKILL_STARTED,SKILL_STARTED-1,SKILL_STARTED-sizeof(skill_started_prefix)};
        for(i=0;i<sizeof(sites)/sizeof(sites[0]);++i) {
            image[sites[i]]^=1;
            CHECK(!SudekiMpInstallLanCastContext((HMODULE)image,witness));
            CHECK(!cast_image);
            image[sites[i]]^=1;
            CHECK(SudekiMpInstallLanCastContext((HMODULE)image,witness));
            CHECK(SudekiMpUninstallLanCastContext());
        }
    }
    setup();
    *(void **)(image+OPCODE_SLOT)=NULL;
    CHECK(!SudekiMpUninstallLanCastContext() && cast_image && owner_witness);
    *(void **)(image+OPCODE_SLOT)=opcode27;
    CHECK(SudekiMpUninstallLanCastContext());
    for(i=0;i<3;++i) CHECK(*(void **)(image+OPCODE_SLOT+4*i)==image+opcode_rvas[i]);
    setup();
    image[STEP_IMMEDIATE]=0x90;
    CHECK(!SudekiMpUninstallLanCastContext() && original_step && cast_image);
    image[STEP_IMMEDIATE]=0xe8;
    CHECK(SudekiMpUninstallLanCastContext());

    /* A failed native context restoration cannot free callbacks or rewrite an
     * already executed opcode as a retry. End this test with quarantined state;
     * process exit, not a fake cancellation/reset API, releases the fixture. */
    setup(); original_step=fake_step;
    CHECK(SudekiMpLanCastContextSetTaskRouting(route_enter,route_leave));
    a=begin_root(scripts[0],2); create(0,11); end_root(a);
    expected_actor=0; refuse_leave=TRUE;
    CHECK(task_step(threads[0],(void *)0x13579)==93);
    CHECK(task_route_fault && lineage_fault && task_scope_count==1 && depth==1 && route_depth==1);
    i=step_calls;
    CHECK(task_step(threads[0],(void *)0x13579)==2 && step_calls==i);
    CHECK(!SudekiMpUninstallLanCastContext() && task_enter && task_leave && original_step);
    CHECK(!SudekiMpLanCastContextSetTaskRouting(NULL,NULL));
    printf("Cast context tests: %s\n",failures ? "FAIL":"PASS");
    return failures ? 1:0;
}
