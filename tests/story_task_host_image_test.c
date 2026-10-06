/* Exact saved-story task hooks and admission sharing. Earlier cases use
 * synthetic constructor/step consumers. Admission cases additionally execute
 * the real submission wrapper/VM/scheduler with fixture tasks, tiny bytecode and real
 * fixture critical sections. The inspect mode also executes real global-binding
 * resolution/dispatch and the empty-world RemoveAllZones path. Script/catalogs,
 * tasks and empty world are synthetic; no native construction, retirement,
 * populated world, skill, load or resource lifetime is exercised. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/hooks/lan_story_task_trace.c"
static uint8_t *mapped;
static size_t image_size;
void SudekiMpLogWrite(const char *s) { (void)s; }
void SudekiMpLogFormat(const char *s,...) { (void)s; }
unsigned SudekiMpLobbyGameplayStoryExitStatus(void) { return 0; }
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(f!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(f,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(f,raw,size,&got,NULL) && got==size); CloseHandle(f);
    IMAGE_DOS_HEADER *dos=(void *)raw; IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    image_size=nt->OptionalHeader.SizeOfImage;
    uint8_t *b=VirtualAlloc(NULL,image_size,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(b); memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=image_size && s->SizeOfRawData<=image_size-s->VirtualAddress);
        memcpy(b+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    for(unsigned offset=0;offset<reloc.Size;) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(uint16_t *)(block+1);
        for(unsigned i=0;i<(block->SizeOfBlock-sizeof(*block))/2;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfffu);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) { assert(rva<=image_size-4); *(uint32_t *)(b+rva)+=(uint32_t)delta; }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw); return b;
}

static unsigned made,stepped;
static void **expected_cell;
static void created(uint32_t hash,void **cell) {
    assert(hash==0x1234u && cell==expected_cell && callbacks==1);
    assert(!SudekiMpLanStoryTaskHostDetach(created,cast_step));
    ++made; SetLastError(99);
}
static int __attribute__((fastcall)) native_fixture(void *thread,void *edx) {
    assert(thread==(void *)0x1234 && edx==(void *)0x5678);
    ++stepped; SetLastError(77); return 2;
}
static int __attribute__((fastcall)) step_fixture(void *thread,void *edx) {
    assert(callbacks==1 && current_thread==thread);
    assert(!SudekiMpLanStoryTaskHostDetach(created,step_fixture));
    return native_fixture(thread,edx);
}
static DWORD WINAPI foreign(void *unused) {
    (void)unused;
    assert(!SudekiMpLanStoryTaskHostExact((HMODULE)mapped));
    assert(!SudekiMpLanStoryTaskHostDetach(created,step_fixture)); return 0;
}
static BOOL no_owner(void *actor,uint8_t kind,uint64_t *session,uint8_t *type) {
    (void)actor; (void)kind; (void)session; (void)type; return FALSE;
}
static uint8_t gate_engine[0x34],gate_manager[0x100a0],gate_runtime[0x18];
static uint8_t *gate_threads[2],gate_code[]={6,0,0,0x80,0x3f,7,1,10};
static uint32_t gate_stacks[2][8],gate_handles[2][2];
static unsigned gate_decisions,gate_retirements,gate_routes;
static BOOL gate_block=TRUE,gate_route_yield,gate_retire_fail,gate_reenter,gate_invalid,gate_retire_in_run,gate_retire_fault;
static int gate_owner;
static SudekiMpStoryTaskAdmissionDecision decide(const void *owner,const SudekiMpStoryTaskAdmissionView *v);
static BOOL retired(const void *owner,const SudekiMpStoryTaskAdmissionView *v) {
    assert(owner==&gate_owner && callbacks && !v->thread && v->tracked && v->waiting);
    assert(v->task_id && v->load_generation==123);++gate_retirements;
    assert(!SudekiMpLanStoryTaskAdmissionDetach((HMODULE)mapped,owner,decide,retired));
    if(gate_retire_fault) fault("fixture_retirement_observer_fault");
    SetLastError(999);return !gate_retire_fail;
}
static SudekiMpStoryTaskAdmissionDecision decide(const void *owner,const SudekiMpStoryTaskAdmissionView *v) {
    assert(owner==&gate_owner && callbacks && v->thread==current_thread);++gate_decisions;
    assert(!SudekiMpLanStoryTaskAdmissionDetach((HMODULE)mapped,owner,decide,retired));
    if(gate_reenter) {
        gate_reenter=FALSE;assert(story_step(v->thread,NULL)==2);
    }
    SetLastError(999);
    if(gate_invalid) return SUDEKIMP_STORY_TASK_UNKNOWN;
    if(!v->tracked) return SUDEKIMP_STORY_TASK_WAIT; /* No made-up identity. */
    assert(v->load_generation==123 && v->task_id && v->function_hash==0x1234);
    return gate_block && v->thread==gate_threads[0]?SUDEKIMP_STORY_TASK_WAIT:SUDEKIMP_STORY_TASK_RUN;
}
static void route_created(uint32_t hash,void **out) {(void)out;assert(hash==0x1234);}
static int __attribute__((fastcall)) route_step(void *t,void *d) {
    ++gate_routes;
    if(gate_retire_in_run) {
        uint32_t id=story_retire_begin(gate_manager,t);assert(id);
        DWORD old;assert(VirtualProtect(t,0x1000,PAGE_NOACCESS,&old));
        story_retire_end(id,t);return 0;
    }
    return gate_route_yield?2:original_step(t,d);
}
static void prepare_fixture_task(unsigned n) {
    memset(gate_threads[n],0,0x50);memset(gate_stacks[n],0,sizeof(gate_stacks[n]));
    *(void **)(gate_threads[n]+0x20)=gate_stacks[n]+8;
    *(unsigned *)(gate_threads[n]+0x24)=8;*(unsigned *)(gate_threads[n]+0x40)=1;
    *(unsigned *)(gate_threads[n]+0xc)=n?5:0;
    gate_handles[n][0]=(uint32_t)(uintptr_t)gate_threads[n];gate_handles[n][1]=1;
}
static void new_fixture_task(unsigned n) {
    prepare_fixture_task(n);
    void *out=gate_handles[n];callbacks=1;
    story_created(0x1234,gate_manager,&out);assert(!callbacks);
    assert(find_task(gate_threads[n],FALSE));
}
/* The real immediate wrapper calls the shared constructor bridge, but native
 * allocation itself is replaced with our fixture handle/list publication. */
static void *__attribute__((stdcall,force_align_arg_pointer)) create_fixture(
    void *manager,void **out,unsigned flags,void *context,uintptr_t unused) {
    (void)unused;
    assert(manager==gate_manager && callbacks==1 && flags==0 && !context);
    *out=gate_handles[0];
    *(void **)(gate_manager+0x48)=gate_threads[0];
    *(void **)(gate_threads[0]+8)=gate_threads[1];
    return out;
}
static void __attribute__((noipa)) run_immediate(void) {
    typedef void **(__attribute__((thiscall)) *Submit)(void *,void *,void **,
        uint32_t,const uint32_t *,unsigned,BOOL,uint8_t *,uint32_t *);
    const uint32_t args=0;uint8_t success=0;uint32_t result=0xabcdef12u;
    void *out=NULL;RawFunction saved_create=original_create;
    original_create=(RawFunction)create_fixture;
    void **returned=((Submit)(mapped+0x1c38d0))(NULL,gate_manager,&out,
        0x1234,&args,0,TRUE,&success,&result);
    original_create=saved_create;
    assert(returned==&out && out==gate_handles[0] && success==1);
    assert(gate_handles[0][1]==1 && result==0xabcdef12u);
    assert(find_task(gate_threads[0],FALSE) && admission_waiters==1);
    assert(!callbacks && !current_thread && !*(void **)(gate_manager+0x10080));
    assert(!*(unsigned *)(gate_threads[0]+0xc) && !*(unsigned *)(gate_threads[0]+0x28));
    /* We reached this caller while the script remains pending. A script wait
     * does NOT suspend the caller's next C-level reload/quit operation. */
}
/* The retail scheduler calls back through patched image memory. Keep this
 * opaque to interprocedural C analysis: an inlined asm CALL alone let GCC
 * cache our static counters across those otherwise invisible callbacks. */
static void __attribute__((noipa)) run_scheduler(void) {
    unsigned result;void *entry=mapped+0x1c32a0,*manager=gate_manager;
    __asm__ volatile("push $0x3dcccccd; push $1; call *%2"
        : "=a"(result),"+D"(manager) : "r"(entry) : "ecx","edx","cc","memory");
    assert((result&255)==1 && !callbacks && !current_thread);
}
static DWORD WINAPI gate_foreign(void *unused) {
    (void)unused;assert(story_step(gate_threads[0],NULL)==2);return 0;
}
static BOOL admission_cases(const char *mode) {
    uint8_t *saved_engine=*(void **)(mapped+GEL_GLOBAL),*saved_runtime=*(void **)(mapped+RUNTIME_GLOBAL);
    uint32_t saved_shift=*(uint32_t *)(mapped+0x323fc8),saved_tick=*(uint32_t *)(mapped+0x409e50);
    uint8_t saved_disabled=mapped[0x3c30b3];
    *(void **)(mapped+GEL_GLOBAL)=gate_engine;*(void **)(gate_engine+0x30)=gate_manager;
    *(void **)(mapped+RUNTIME_GLOBAL)=gate_runtime;*(void **)(gate_runtime+0x14)=gate_code;
    *(uint32_t *)(mapped+0x323fc8)=0;*(uint32_t *)(mapped+0x409e50)=0;mapped[0x3c30b3]=0;
    load_manager=gate_manager;status.load_generation=123;original_step=(StepFunction)(mapped+STEP);
    InitializeCriticalSection((CRITICAL_SECTION *)(gate_manager+8));
    gate_threads[0]=VirtualAlloc(NULL,0x1000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    gate_threads[1]=VirtualAlloc(NULL,0x1000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(gate_threads[0] && gate_threads[1]);prepare_fixture_task(0);new_fixture_task(1);
    SudekiMpPointerHook locks[2]={{0}};const unsigned imports[]={0x29a068,0x29a064};
    void *lock_functions[]={(void *)EnterCriticalSection,(void *)LeaveCriticalSection};
    for(unsigned i=0;i<2;++i) assert(SudekiMpInstallPointerHook(&locks[i],(void **)(mapped+imports[i]),
        *(void **)(mapped+imports[i]),lock_functions[i]));
    assert(SudekiMpLanStoryTaskHostAttach((HMODULE)mapped,route_created,route_step));
    assert(!SudekiMpLanStoryTaskAdmissionAttach((HMODULE)mapped,NULL,decide,retired));
    assert(SudekiMpLanStoryTaskAdmissionAttach((HMODULE)mapped,&gate_owner,decide,retired));
    assert(!SudekiMpLanStoryTaskAdmissionAttach((HMODULE)mapped,&gate_owner,decide,retired));
    assert(!SudekiMpLanStoryTaskTraceUninstall() && installed);
    run_immediate();assert(gate_decisions==1 && gate_routes==0 && !trace_fault);
    uint8_t before[0x50];memcpy(before,gate_threads[0],sizeof(before));
    for(unsigned i=0;i<3;++i) {SetLastError(55);assert(story_step(gate_threads[0],NULL)==2 && GetLastError()==55);}
    assert(!memcmp(before,gate_threads[0],sizeof(before)) && gate_routes==0 && admission_waiters==1);
    assert(!SudekiMpLanStoryTaskAdmissionDetach((HMODULE)mapped,&gate_owner,decide,retired));
    /* Actual scheduler observes native yield=2 and continues to the next task.
     * Both clocks advance, but the blocked task's PC/value stack stay put. */
    *(void **)(gate_manager+0x48)=gate_threads[0];*(void **)(gate_threads[0]+8)=gate_threads[1];
    *(unsigned *)(gate_manager+0x10094)=*(unsigned *)(gate_manager+0x10098)=1;
    run_scheduler();assert(admission_waiters==1 && gate_routes==1);
    assert(!*(unsigned *)(gate_threads[0]+0xc) && !*(unsigned *)(gate_threads[0]+0x28));
    assert(*(unsigned *)(gate_threads[1]+0xc)==7 && *(unsigned *)(gate_threads[1]+0x28)==1 && gate_stacks[1][7]==1);
    assert(*(float *)(gate_threads[0]+0x44)>0 && *(float *)(gate_threads[1]+0x44)>0);
    gate_block=FALSE;gate_route_yield=TRUE;
    assert(story_step(gate_threads[0],NULL)==2 && admission_waiters==1 && !*(unsigned *)(gate_threads[0]+0xc));
    gate_route_yield=FALSE;run_scheduler();
    assert(!admission_waiters && *(unsigned *)(gate_threads[0]+0xc)==5 && gate_stacks[0][7]==0x3f800000);
    assert(*(unsigned *)(gate_threads[1]+0xc)==8 && !*(unsigned *)(gate_threads[1]+0x28));
    assert(!SudekiMpLanStoryTaskAdmissionDetach((HMODULE)mapped,gate_manager,decide,retired));
    assert(SudekiMpLanStoryTaskAdmissionDetach((HMODULE)mapped,&gate_owner,decide,retired));
    assert(SudekiMpLanStoryTaskAdmissionAttach((HMODULE)mapped,&gate_owner,decide,retired));
    gate_block=TRUE;assert(story_step(gate_threads[0],NULL)==2 && admission_waiters==1);
    if(!strcmp(mode,"unknown")) {
        assert(story_step((void *)0x1234,NULL)==2 && admission_fault);
    } else if(!strcmp(mode,"decision")) {
        gate_invalid=TRUE;assert(story_step(gate_threads[0],NULL)==2 && admission_fault);
    } else if(!strcmp(mode,"identity")) {
        ++status.load_generation;assert(story_step(gate_threads[0],NULL)==2 && admission_fault);
    } else if(!strcmp(mode,"retire-in-run")) {
        gate_retire_in_run=gate_retire_fail=TRUE;gate_block=FALSE;
        assert(story_step(gate_threads[0],NULL)==0); /* Never replace an executed result. */
        assert(admission_fault && admission_waiters==1 && gate_retirements==1);
    } else if(!strcmp(mode,"reentry")) {
        gate_reenter=TRUE;assert(story_step(gate_threads[0],NULL)==2 && admission_fault);
    } else if(!strcmp(mode,"thread")) {
        unsigned seen=gate_decisions;HANDLE worker=CreateThread(NULL,0,gate_foreign,NULL,0,NULL);assert(worker);
        assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);CloseHandle(worker);
        assert(admission_fault && gate_decisions==seen);
    } else {
        gate_retire_fail=!strcmp(mode,"retire-fail");
        gate_retire_fault=!strcmp(mode,"retire-fault");
        BOOL prior_fault=!strcmp(mode,"retire-prior-fault");
        assert(gate_retire_fail || gate_retire_fault || prior_fault || !strcmp(mode,"normal"));
        uint32_t id=story_retire_begin(gate_manager,gate_threads[0]);assert(id && callbacks==1);
        DWORD old;assert(VirtualProtect(gate_threads[0],0x1000,PAGE_NOACCESS,&old));
        if(prior_fault) fault("fixture_retirement_prior_fault");
        SetLastError(66);story_retire_end(id,gate_threads[0]);
        assert(GetLastError()==66 && !callbacks && gate_retirements==(prior_fault?0u:1u));
        /* Retirement callback uses copied identity, not freed/unreadable body. */
        if(gate_retire_fail || gate_retire_fault || prior_fault) {
            Task *pending=find_task(gate_threads[0],FALSE);
            assert(admission_fault && admission_waiters==1 && pending && pending->id==id && pending->admission_waiting);
        } else {
            assert(!admission_waiters && !find_task(gate_threads[0],FALSE));
            assert(VirtualProtect(gate_threads[0],0x1000,old,&old));new_fixture_task(0);
            assert(find_task(gate_threads[0],FALSE)->id>id);
        }
    }
    if(strcmp(mode,"normal")) {
        assert(admission_fault && !SudekiMpLanStoryTaskAdmissionDetach((HMODULE)mapped,&gate_owner,decide,retired));
        assert(!SudekiMpLanStoryTaskTraceUninstall() && installed);
        /* Keep image, imports, callback dependencies and fixture storage until
         * isolated process exit. This is NOT successful native live teardown. */
        return TRUE;
    }
    assert(SudekiMpLanStoryTaskAdmissionDetach((HMODULE)mapped,&gate_owner,decide,retired));
    assert(SudekiMpLanStoryTaskHostDetach(route_created,route_step));
    for(unsigned i=2;i>0;--i) assert(SudekiMpRestorePointerHook(&locks[i-1]));
    DeleteCriticalSection((CRITICAL_SECTION *)(gate_manager+8));
    *(void **)(mapped+GEL_GLOBAL)=saved_engine;*(void **)(mapped+RUNTIME_GLOBAL)=saved_runtime;
    *(uint32_t *)(mapped+0x323fc8)=saved_shift;*(uint32_t *)(mapped+0x409e50)=saved_tick;mapped[0x3c30b3]=saved_disabled;
    assert(VirtualFree(gate_threads[0],0,MEM_RELEASE) && VirtualFree(gate_threads[1],0,MEM_RELEASE));
    return FALSE;
}
static uint8_t inspect_code[]={0x27,0x34,0x12,0,0,7,1,10};
static uint32_t inspect_functions[1][4],inspect_bindings[2][7],inspect_keys[2][3];
static uintptr_t inspect_table[8];
static uint8_t inspect_world[0x3a0];
static BOOL inspect_expected=TRUE,inspect_hold=TRUE,inspect_probe=TRUE;
static unsigned inspect_kind=SUDEKIMP_STORY_INSTRUCTION_NATIVE,inspect_calls;
static uint32_t inspect_rva=0x7950;
static const SudekiMpStoryTaskAdmissionView *inspect_borrowed;
static SudekiMpStoryTaskAdmissionDecision inspect_decide(const void *owner,
    const SudekiMpStoryTaskAdmissionView *v) {
    SudekiMpStoryTaskInstruction out,before;memset(&out,0xa5,sizeof(out));before=out;
    assert(owner==&gate_owner && v->tracked);++inspect_calls;
    SudekiMpStoryTaskAdmissionView copied=*v;
    assert(!SudekiMpLanStoryTaskInspectInstruction((HMODULE)mapped,owner,&copied,&out));
    assert(!memcmp(&out,&before,sizeof(out)));
    assert(!SudekiMpLanStoryTaskInspectInstruction((HMODULE)mapped,gate_manager,v,&out));
    BOOL ok=SudekiMpLanStoryTaskInspectInstruction((HMODULE)mapped,owner,v,&out);
    if(v->thread==gate_threads[0]) {
        assert(ok==inspect_expected);
        if(ok) {
            assert(out.kind==inspect_kind && out.offset==*(unsigned *)(gate_threads[0]+0xc));
            if(out.kind==SUDEKIMP_STORY_INSTRUCTION_NATIVE)
                assert(out.call_hash==0x1234 && out.native_rva==inspect_rva && !out.argument_count);
            if(out.kind==SUDEKIMP_STORY_INSTRUCTION_COMPILED)
                assert(out.call_hash==0x1234 && out.script_offset==5 && !out.native_rva);
        } else assert(!memcmp(&out,&before,sizeof(out)));
    } else assert(ok && out.kind==SUDEKIMP_STORY_INSTRUCTION_OTHER);
    inspect_borrowed=v; /* Test only: later API refusal must not dereference it. */
    SetLastError(999);
    /* Probe cases intentionally hold their fixture even after failed reads.
     * Integration below decides by the resolved target, NOT task identity. */
    BOOL wait=inspect_probe?v->thread==gate_threads[0]:
        ok && out.kind==SUDEKIMP_STORY_INSTRUCTION_NATIVE && out.native_rva==0x7950;
    return inspect_hold && wait?SUDEKIMP_STORY_TASK_WAIT:SUDEKIMP_STORY_TASK_RUN;
}
static BOOL inspect_retired(const void *owner,const SudekiMpStoryTaskAdmissionView *v) {
    (void)owner;(void)v;assert(0);return FALSE;
}
static void inspect_wait(void) {
    uint8_t before[0x50],world_before[sizeof(inspect_world)];
    uint32_t stack_before[8];memcpy(before,gate_threads[0],sizeof(before));
    memcpy(world_before,inspect_world,sizeof(world_before));
    memcpy(stack_before,gate_stacks[0],sizeof(stack_before));
    SetLastError(55);assert(story_step(gate_threads[0],NULL)==2 && GetLastError()==55);
    assert(!memcmp(before,gate_threads[0],sizeof(before)) &&
        !memcmp(stack_before,gate_stacks[0],sizeof(stack_before)) &&
        !memcmp(world_before,inspect_world,sizeof(world_before)) && !trace_fault && admission_waiters==1);
}
static void inspect_cases(void) {
    uint8_t *saved_image=malloc(image_size);assert(saved_image);memcpy(saved_image,mapped,image_size);
    memset(gate_manager,0,sizeof(gate_manager));
    *(void **)(mapped+GEL_GLOBAL)=gate_engine;*(void **)(gate_engine+0x30)=gate_manager;
    *(void **)(mapped+RUNTIME_GLOBAL)=gate_manager+0x20;*(void **)(mapped+0x3c3108)=gate_manager;
    *(uint32_t *)(mapped+0x323fc8)=0;*(uint32_t *)(mapped+0x409e50)=0;mapped[0x3c30b3]=0;
    *(void **)(mapped+0x408d10)=inspect_world;
    *(unsigned *)(gate_manager+0x20)=8;*(void **)(gate_manager+0x24)=inspect_table;
    *(void **)(gate_manager+0x28)=inspect_functions;
    *(unsigned *)(gate_manager+0x2c)=*(unsigned *)(gate_manager+0x30)=1;
    *(void **)(gate_manager+0x34)=inspect_code;
    *(void **)(gate_manager+0x38)=*(void **)(gate_manager+0x3c)=inspect_code+sizeof(inspect_code);
    *(uintptr_t *)(gate_manager+0x6c)=(uintptr_t)mapped-0x400000u;
    *(unsigned *)(gate_manager+0x70)=*(unsigned *)(gate_manager+0x74)=2;
    *(void **)(gate_manager+0x78)=inspect_bindings;*(void **)(gate_manager+0x7c)=inspect_keys;
    /* A same-hash object method occupies the first probe; global lookup must
     * continue to namespace zero and use its bounded binding index. */
    inspect_keys[0][0]=inspect_keys[1][0]=0x1234;inspect_keys[0][1]=1;inspect_keys[1][2]=1;
    *(void **)(gate_manager+0x80+0x1234*4)=inspect_keys[0];
    *(void **)(gate_manager+0x80+0x1235*4)=inspect_keys[1];
    inspect_bindings[0][0]=0x4a2740;inspect_bindings[1][0]=0x407950;
    inspect_bindings[0][3]=inspect_bindings[1][3]=0; /* native cdecl, void, no arguments */
    inspect_functions[0][1]=0x9999;inspect_functions[0][2]=5;
    memset(inspect_world,0xa5,sizeof(inspect_world));
    *(unsigned *)(inspect_world+0x50)=*(unsigned *)(inspect_world+0x54)=0;
    InitializeCriticalSection((CRITICAL_SECTION *)(gate_manager+8));
    SudekiMpPointerHook locks[2]={{0}};const unsigned imports[]={0x29a068,0x29a064};
    void *lock_functions[]={(void *)EnterCriticalSection,(void *)LeaveCriticalSection};
    for(unsigned i=0;i<2;++i) assert(SudekiMpInstallPointerHook(&locks[i],(void **)(mapped+imports[i]),
        *(void **)(mapped+imports[i]),lock_functions[i]));
    load_manager=gate_manager;status.load_generation=123;original_step=(StepFunction)(mapped+STEP);
    for(unsigned i=0;i<2;++i) {
        gate_threads[i]=VirtualAlloc(NULL,0x1000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        assert(gate_threads[i]);new_fixture_task(i);
    }
    SudekiMpStoryTaskInstruction out,before;memset(&out,0xa5,sizeof(out));before=out;
    assert(!SudekiMpLanStoryTaskInspectInstruction((HMODULE)mapped,&gate_owner,NULL,&out));
    assert(!memcmp(&out,&before,sizeof(out)));
    assert(SudekiMpLanStoryTaskAdmissionAttach((HMODULE)mapped,&gate_owner,inspect_decide,inspect_retired));
    for(unsigned i=0;i<3;++i) inspect_wait();
    assert(!SudekiMpLanStoryTaskInspectInstruction((HMODULE)mapped,&gate_owner,inspect_borrowed,&out));
    assert(!memcmp(&out,&before,sizeof(out)));
    assert(!SudekiMpLanStoryTaskAdmissionDetach((HMODULE)mapped,&gate_owner,inspect_decide,inspect_retired));
    /* Compiled precedence even with the same native hash; no compiled body is
     * executed in this probe. A changed native target is re-read, not cached. */
    inspect_table[4]=(uintptr_t)inspect_functions[0];inspect_functions[0][1]=0x1234;
    inspect_kind=SUDEKIMP_STORY_INSTRUCTION_COMPILED;inspect_wait();
    inspect_table[4]=0;inspect_kind=SUDEKIMP_STORY_INSTRUCTION_NATIVE;
    inspect_bindings[1][0]=0x4a2740;inspect_rva=0xa2740;inspect_wait();
    inspect_bindings[1][0]=0x407950;inspect_rva=0x7950;
    inspect_code[0]=0x28;inspect_kind=SUDEKIMP_STORY_INSTRUCTION_METHOD;inspect_wait();
    uint8_t child_code[]={0x29,0x34,0x12,0,0,0,0,0,0};
    *(void **)(gate_manager+0x34)=child_code;
    *(void **)(gate_manager+0x38)=*(void **)(gate_manager+0x3c)=child_code+sizeof(child_code);
    inspect_kind=SUDEKIMP_STORY_INSTRUCTION_CHILD;inspect_wait();
    *(void **)(gate_manager+0x34)=inspect_code;
    *(void **)(gate_manager+0x38)=*(void **)(gate_manager+0x3c)=inspect_code+sizeof(inspect_code);
    inspect_code[0]=0x29;inspect_expected=FALSE;inspect_wait(); /* truncated child operand */
    inspect_code[0]=0x27;inspect_kind=SUDEKIMP_STORY_INSTRUCTION_NATIVE;
    uint8_t manager_before[sizeof(gate_manager)];memcpy(manager_before,gate_manager,sizeof(gate_manager));
    inspect_expected=FALSE;
    for(unsigned bad=0;bad<18;++bad) {
        switch(bad) {
        case 0: *(void **)(mapped+RUNTIME_GLOBAL)=gate_runtime;break;
        case 1: *(void **)(mapped+0x3c3108)=gate_runtime;break;
        case 2: *(void **)(gate_manager+0x38)=inspect_code+4;break; /* truncated operand */
        case 3: *(unsigned *)(gate_threads[0]+0xc)=UINT32_MAX;break;
        case 4: *(void **)(gate_manager+0x3c)=inspect_code;break;
        case 5: *(unsigned *)(gate_manager+0x20)=3;break;
        case 6: *(unsigned *)(gate_manager+0x2c)=2;break;
        case 7: inspect_table[4]=1;break;
        case 8: inspect_keys[1][2]=2;break;
        case 9: inspect_bindings[1][3]=2;break;
        case 10: inspect_bindings[1][6]=17u<<16;break;
        case 11: inspect_bindings[1][0]=0x3fffff;break;
        case 12: *(unsigned *)(gate_manager+0x70)=3;break;
        case 13: *(void **)(gate_manager+0x80+0x1235*4)=inspect_keys[1]+1;break;
        case 14: *(void **)(gate_manager+0x78)=(void *)0xfffffff0u;break;
        case 15:
            inspect_functions[0][1]=0x9999;
            for(unsigned i=0;i<8;++i) inspect_table[i]=(uintptr_t)inspect_functions[0];
            break;
        case 16:
            for(unsigned i=0;i<16384;++i) *(void **)(gate_manager+0x80+i*4)=inspect_keys[0];
            break;
        case 17: inspect_table[4]=(uintptr_t)inspect_functions[0];inspect_functions[0][2]=8;break;
        }
        inspect_wait();
        memcpy(gate_manager,manager_before,sizeof(gate_manager));
        *(void **)(mapped+RUNTIME_GLOBAL)=gate_manager+0x20;*(void **)(mapped+0x3c3108)=gate_manager;
        *(unsigned *)(gate_threads[0]+0xc)=0;inspect_keys[1][2]=1;
        inspect_bindings[1][0]=0x407950;inspect_bindings[1][3]=inspect_bindings[1][6]=0;
        memset(inspect_table,0,sizeof(inspect_table));inspect_functions[0][1]=0x1234;inspect_functions[0][2]=5;
    }
    inspect_expected=TRUE;inspect_probe=FALSE;
    /* Real scheduler runs the unrelated task while the cleanup instruction,
     * its value stack and every empty-world byte remain untouched. */
    uint8_t world_before[sizeof(inspect_world)];memcpy(world_before,inspect_world,sizeof(world_before));
    *(void **)(gate_manager+0x48)=gate_threads[0];*(void **)(gate_threads[0]+8)=gate_threads[1];
    *(unsigned *)(gate_manager+0x10094)=*(unsigned *)(gate_manager+0x10098)=1;
    run_scheduler();assert(!memcmp(world_before,inspect_world,sizeof(world_before)));
    assert(!*(unsigned *)(gate_threads[0]+0xc) && !*(unsigned *)(gate_threads[0]+0x28));
    assert(*(unsigned *)(gate_threads[1]+0xc)==7 && gate_stacks[1][7]==1);
    inspect_hold=FALSE;run_scheduler(); /* actual VM -> resolver -> dispatcher -> RemoveAllZones */
    assert(!admission_waiters && *(unsigned *)(gate_threads[0]+0xc)==5);
    assert(*(unsigned *)(gate_threads[0]+0x28)==1 && gate_stacks[0][7]==0);
    assert(*(unsigned *)(gate_threads[1]+0xc)==8 && !*(unsigned *)(gate_threads[1]+0x28));
    for(unsigned i=0xc;i<=0x18;i+=4) *(unsigned *)(world_before+i)=0;
    world_before[0x39d]=1;assert(!memcmp(world_before,inspect_world,sizeof(world_before)));
    assert(inspect_calls==30 && !trace_fault && !admission_fault);
    assert(SudekiMpLanStoryTaskAdmissionDetach((HMODULE)mapped,&gate_owner,inspect_decide,inspect_retired));
    for(unsigned i=2;i>0;--i) assert(SudekiMpRestorePointerHook(&locks[i-1]));
    DeleteCriticalSection((CRITICAL_SECTION *)(gate_manager+8));
    const unsigned globals[]={GEL_GLOBAL,RUNTIME_GLOBAL,0x3c3108,0x323fc8,0x409e50,0x408d10};
    for(unsigned i=0;i<sizeof(globals)/sizeof(globals[0]);++i)
        memcpy(mapped+globals[i],saved_image+globals[i],4);
    mapped[0x3c30b3]=saved_image[0x3c30b3];
    assert(!memcmp(mapped,saved_image,image_size));free(saved_image);
    assert(VirtualFree(gate_threads[0],0,MEM_RELEASE) && VirtualFree(gate_threads[1],0,MEM_RELEASE));
}
int main(int argc,char **argv) {
    assert(argc==2 || argc==3); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024)); mapped=map_image(path);
    assert(SudekiMpLanStoryTaskTraceInstall((HMODULE)mapped));
    assert(!SudekiMpLanStoryTaskHostExact((HMODULE)mapped)); /* no native thread yet */
    native_thread=GetCurrentThreadId();
    assert(SudekiMpLanStoryTaskHostExact((HMODULE)mapped));
    assert(SudekiMpLanStoryTaskHostAttach((HMODULE)mapped,created,step_fixture));
    assert(!SudekiMpLanStoryTaskHostAttach((HMODULE)mapped,created,step_fixture));
    void *cell=NULL; expected_cell=&cell; original_step=native_fixture;
    callbacks=1; SetLastError(55);
    story_created(0x1234,NULL,&cell);
    assert(!callbacks && made==1 && GetLastError()==55);
    assert(story_step((void *)0x1234,(void *)0x5678)==2 && stepped==1 && GetLastError()==77);
    assert(!callbacks && !current_thread);
    assert(!SudekiMpLanStoryTaskTraceUninstall() && installed && cast_created && cast_step);
    HANDLE worker=CreateThread(NULL,0,foreign,NULL,0,NULL); assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0); CloseHandle(worker);
    assert(SudekiMpLanStoryTaskHostDetach(created,step_fixture));
    assert(story_step((void *)0x1234,(void *)0x5678)==2 && stepped==2);
    /* Real cast lineage installer shares the existing four story sites. */
    const SudekiMpLanCastTaskHost host={SudekiMpLanStoryTaskHostExact,
        SudekiMpLanStoryTaskHostAttach,SudekiMpLanStoryTaskHostDetach};
    uint8_t calls[4][5]; const unsigned sites[]={CREATE_DIRECT,CREATE_CHILD,STEP_IMMEDIATE,STEP_SCHEDULED};
    for(unsigned i=0;i<4;++i) memcpy(calls[i],mapped+sites[i],5);
    assert(SudekiMpInstallLanCastContextWithTaskHost((HMODULE)mapped,no_owner,&host));
    assert(SudekiMpLanCastContextPoll());
    for(unsigned i=0;i<4;++i) assert(!memcmp(calls[i],mapped+sites[i],5));
    assert(!SudekiMpLanStoryTaskTraceUninstall());
    assert(SudekiMpUninstallLanCastContext() && !cast_created && !cast_step);
    for(unsigned i=0;i<4;++i) assert(!memcmp(calls[i],mapped+sites[i],5));
    if(argc==3 && !strcmp(argv[2],"inspect")) {
        inspect_cases();assert(SudekiMpLanStoryTaskTraceUninstall());
        assert(VirtualFree(mapped,0,MEM_RELEASE));
        puts("story instruction inspection: PASS (real VM/global binding/empty-world cleanup; selective wait and bounded lookup; no gameplay)");
        return 0;
    }
    if(admission_cases(argc==3?argv[2]:"normal")) {
        printf("story task admission: PASS (%s; native fixture scheduler/VM; quarantined dependencies retained; no gameplay)\n",argv[2]);
        return 0;
    }
    assert(SudekiMpLanStoryTaskTraceUninstall());
    VirtualFree(mapped,0,MEM_RELEASE);
    puts("story shared task owner/admission: PASS (native fixture scheduler/VM, unrelated task advances, sharing and retirement; no gameplay)");
    return 0;
}
