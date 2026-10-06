/* Retail PE is relocated and hash checked. Most cases use synthetic originals.
 * The publication cases additionally execute the native ZoneRequest body with
 * synthetic lookup/name dependencies; no real resource loading/gameplay runs. */
#include "hooks/call_hook.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned installs,restores,fail_install,fail_restore;
static BOOL pointer_install(SudekiMpPointerHook *,void **,const void *,const void *);
static BOOL pointer_restore(SudekiMpPointerHook *);
static BOOL call_install(SudekiMpRelativeCallHook *,uint8_t *,const void *,const void *);
static BOOL call_restore(SudekiMpRelativeCallHook *);
static BOOL inline_install(SudekiMpInlineHook *,uint8_t *,const uint8_t *,size_t,const void *);
static BOOL inline_restore(SudekiMpInlineHook *);
static DWORD test_thread(void);
#define SudekiMpInstallPointerHook pointer_install
#define SudekiMpRestorePointerHook pointer_restore
#define SudekiMpInstallRelativeCallHook call_install
#define SudekiMpRestoreRelativeCallHook call_restore
#define SudekiMpInstallInlineHook inline_install
#define SudekiMpRestoreInlineHook inline_restore
#define GetCurrentThreadId test_thread
#include "../src/hooks/lan_story_area_task.c"
#undef GetCurrentThreadId
#undef SudekiMpInstallPointerHook
#undef SudekiMpRestorePointerHook
#undef SudekiMpInstallRelativeCallHook
#undef SudekiMpRestoreRelativeCallHook
#undef SudekiMpInstallInlineHook
#undef SudekiMpRestoreInlineHook
static BOOL inline_install(SudekiMpInlineHook *h,uint8_t *p,const uint8_t *e,size_t n,const void *r) {
    if(++installs==fail_install) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpInstallInlineHook(h,p,e,n,r);
}
static BOOL inline_restore(SudekiMpInlineHook *h) {
    if(++restores==fail_restore) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpRestoreInlineHook(h);
}
static BOOL pointer_install(SudekiMpPointerHook *h,void **p,const void *e,const void *r) {
    if(++installs==fail_install) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpInstallPointerHook(h,p,e,r);
}
static BOOL pointer_restore(SudekiMpPointerHook *h) {
    if(++restores==fail_restore) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpRestorePointerHook(h);
}
static BOOL call_install(SudekiMpRelativeCallHook *h,uint8_t *p,const void *e,const void *r) {
    if(++installs==fail_install) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpInstallRelativeCallHook(h,p,e,r);
}
static BOOL call_restore(SudekiMpRelativeCallHook *h) {
    if(++restores==fail_restore) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpRestoreRelativeCallHook(h);
}
static BOOL disturb,witness_ok=TRUE;
static DWORD test_thread(void) {
    DWORD value=GetCurrentThreadId();
    if(disturb) {
        SetLastError(0x9999);
        __asm__ volatile("fninit; fld1; pxor %%xmm0,%%xmm0; pxor %%xmm7,%%xmm7" : : : "memory");
    }
    return value;
}
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(const SudekiMpControlUpdateDispatchWitness *w) {
    return witness_ok && w && w->native_thread_id==GetCurrentThreadId();
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(f!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(f,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(f,raw,size,&got,NULL) && got==size); CloseHandle(f);
    IMAGE_DOS_HEADER *dos=(void *)raw; IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(b); memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *s=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        assert(s[i].PointerToRawData<=size && s[i].SizeOfRawData<=size-s[i].PointerToRawData);
        assert(s[i].VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s[i].SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s[i].VirtualAddress);
        memcpy(b+s[i].VirtualAddress,raw+s[i].PointerToRawData,s[i].SizeOfRawData);
    }
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    for(unsigned offset=0;offset<reloc.Size;) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(void *)(block+1); unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfff);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4); *(uint32_t *)(b+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw); return b;
}
static uint8_t *mapped;
static uint8_t world[0x3a0],descriptors[0xa8],*request;
static BOOL request_live;
static BOOL free_on_destroy __attribute__((used));
static SudekiMpStoryAreas policy;
static SudekiMpStoryAreaRef outside,inside;
static SudekiMpControlUpdateDispatchWitness witness;
static HANDLE worker,go,done;
static unsigned command;
static void *command_request;
static uint64_t ticket;
static BOOL run_abi,fast_worker;
static BOOL native_body;
static unsigned publication_fault_mode;
static uint8_t loaded_resource[0x130],manager[0x300];
static void *loaded_table[8];
static uintptr_t test_interface __attribute__((used)),test_resource __attribute__((used));
static unsigned returned_type __attribute__((used)),lookup_count __attribute__((used));
static unsigned keycopy_count __attribute__((used));
static void *retarget_descriptor __attribute__((used));
static int resource_consumer;
static uint64_t resource_generation;
static void *construct_resource_entry,*destroy_resource_entry;
static SudekiMpInlineHook resource_stubs[2];
static DWORD resource_page_protection;
static void __attribute__((naked,noinline)) return_only(void) {__asm__ volatile("ret");}
static volatile LONG native_run_calls,native_destroy_calls,native_enqueue_calls;
static void __attribute__((naked,noinline)) fake_run(void) {
    __asm__ volatile("lock incl _native_run_calls; mov $0xabcdef12,%eax; stc; ret");
}
static void __attribute__((used,noinline)) destroy_action(void) {
    assert(request_live && VirtualFree(request,0,MEM_RELEASE)); request_live=FALSE;
}
static void __attribute__((naked,noinline)) fake_destroy(void) {
    __asm__ volatile("cmpl $0,_free_on_destroy; je 1f; pushfl; pushal;"
        "mov %esp,%ebp; and $-16,%esp; sub $16,%esp; cld; call _destroy_action;"
        "mov %ebp,%esp; popal; popfl;"
        "1: lock incl _native_destroy_calls; mov %ecx,%eax; stc; ret $4");
}
static void __attribute__((used,noinline)) enqueue_action(void) {
    ++native_enqueue_calls;
    if(fast_worker) {
        command=1;command_request=request;SetEvent(go);assert(WaitForSingleObject(done,5000)==WAIT_OBJECT_0);
        SudekiMpLanStoryAreaTaskReceipt r;
        assert(SudekiMpLanStoryAreaTaskRead((HMODULE)mapped,ticket,&r));
        assert(r.phase==SUDEKIMP_AREA_JOB_DESTROYED && r.submitting);
        assert(!SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));
    }
}
static void __attribute__((naked,noinline)) fake_enqueue(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; and $-16,%esp; sub $16,%esp;"
        "cld; call _enqueue_action; mov %ebp,%esp; popal; popfl; stc; ret");
}
static void *bridge_target __attribute__((used)),*bridge_request __attribute__((used));
static unsigned bridge_kind __attribute__((used));
static uint32_t regs[9] __attribute__((used)),stack_before __attribute__((used));
static void __attribute__((naked,noinline)) invoke_bridge(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,_stack_before;"
        "mov $0x11111111,%eax; mov _bridge_request,%ecx; mov $0x33333333,%edx;"
        "mov $0x44444444,%ebx; mov $0x55555555,%ebp; mov $0x66666666,%esi; mov $0x77777777,%edi;"
        "cmpl $3,_bridge_kind; jne 3f; mov _bridge_request,%ebx; mov 0x10(%ebx),%esi;"
        "mov _test_interface,%edi; mov _test_resource,%ebp;"
        "3: "
        "cmpl $0,_bridge_kind; jne 1f; mov _bridge_request,%eax;"
        "1: cmpl $2,_bridge_kind; jne 2f; push $1;"
        "2: std; stc; call *_bridge_target; mov %eax,_regs; mov %ecx,_regs+4; mov %edx,_regs+8;"
        "mov %ebx,_regs+12; mov %ebp,_regs+16; mov %esi,_regs+20; mov %edi,_regs+24;"
        "pushfl; pop _regs+28; mov %esp,_regs+32; popal; popfl; ret");
}
/* Windows thread entries guarantee four-byte, not GCC's assumed sixteen-byte,
 * stack alignment. The test's FXSAVE buffers need an explicit realignment. */
static void __attribute__((force_align_arg_pointer,noinline)) abi(unsigned kind,void *p) {
    uint8_t before[512] __attribute__((aligned(16)))={0},after[512] __attribute__((aligned(16)))={0};
    uint16_t control=0x077f; uint32_t mxcsr=0x3f80;
    bridge_kind=kind; bridge_request=p;
    bridge_target=(void *)(uintptr_t)(kind==0?enqueue_entry:kind==1?run_entry:kind==2?destroy_entry:publish_entry);
    disturb=TRUE;SetLastError(0x7788);
    __asm__ volatile("fninit; fldcw %1; ldmxcsr %2; fldpi; fld1; fldl2t;"
        "pcmpeqd %%xmm0,%%xmm0; pcmpeqd %%xmm7,%%xmm7; fxsave %0"
        : "=m"(before) : "m"(control),"m"(mxcsr) : "memory");
    invoke_bridge();
    __asm__ volatile("fxsave %0; fninit" : "=m"(after) : : "memory");
    disturb=FALSE;
    assert(GetLastError()==0x7788 && !memcmp(before,after,160) && !memcmp(before+160,after+160,128));
    if(kind==3) {
        assert(regs[0]==0x11111111 && regs[1]==(uintptr_t)(descriptors+0x54));
        assert(regs[2]==0x33333333 && regs[3]==(uintptr_t)p && regs[4]==test_resource &&
            regs[5]==(uintptr_t)(descriptors+0x54) && regs[6]==test_interface);
    } else {
        assert(regs[0]==(kind==1?0xabcdef12u:(uintptr_t)p) && regs[1]==(uintptr_t)p);
        for(unsigned i=2;i<7;++i) assert(regs[i]==0x11111111u*(i+1));
    }
    assert((regs[7]&0x401)==0x401 && regs[8]==stack_before);
}
static void simple_call(unsigned kind,void *p) {
    bridge_kind=kind;bridge_request=p;
    bridge_target=(void *)(uintptr_t)(kind==1?run_entry:destroy_entry);
    invoke_bridge();
}
static void __attribute__((naked,noinline)) native_run(void *p __attribute__((unused))) {
    __asm__ volatile("mov 4(%esp),%ecx; jmp _run_entry");
}
static DWORD WINAPI work(void *unused) {
    (void)unused;
    for(;;) {
        assert(WaitForSingleObject(go,5000)==WAIT_OBJECT_0);
        if(!command) break;
        if(command==2) {
            EnterCriticalSection((CRITICAL_SECTION *)(mapped+QUEUE_LOCK));
            SetEvent(done); assert(WaitForSingleObject(go,5000)==WAIT_OBJECT_0);
            LeaveCriticalSection((CRITICAL_SECTION *)(mapped+QUEUE_LOCK)); SetEvent(done); continue;
        }
        void *p=command_request;
        EnterCriticalSection((CRITICAL_SECTION *)(mapped+QUEUE_LOCK));
        *(void **)(mapped+ACTIVE)=p;
        LeaveCriticalSection((CRITICAL_SECTION *)(mapped+QUEUE_LOCK));
        if(command==6) {
            event(RUN_BEGIN,p,0);
            if(publication_fault_mode==6) event(RUN_END,p,0);
            if(publication_fault_mode==5) *(void **)(mapped+ACTIVE)=NULL;
            publication(publication_fault_mode==4?mapped:p,
                publication_fault_mode==1?descriptors:descriptors+0x54,
                publication_fault_mode==2?test_interface+8:publication_fault_mode==7?0:test_interface,
                publication_fault_mode==3?UINTPTR_MAX:test_resource);
            if(!publication_fault_mode) publication(p,descriptors+0x54,test_interface,test_resource);
            if(publication_fault_mode==5) *(void **)(mapped+ACTIVE)=p;
            if(publication_fault_mode!=6) event(RUN_END,p,0);
        }
        else if(command==5) {event(RUN_BEGIN,p,0);abi(3,p);event(RUN_END,p,0);}
        else if(native_body) native_run(p);
        else if(run_abi) abi(1,p); else simple_call(1,p);
        if(command==3) {SetEvent(done);assert(WaitForSingleObject(go,5000)==WAIT_OBJECT_0);}
        EnterCriticalSection((CRITICAL_SECTION *)(mapped+QUEUE_LOCK));
        if(run_abi) abi(2,p); else simple_call(2,p);
        *(void **)(mapped+ACTIVE)=NULL;
        LeaveCriticalSection((CRITICAL_SECTION *)(mapped+QUEUE_LOCK));
        SetEvent(done);
    }
    return 0;
}
static void fixture(void) {
    installs=restores=fail_install=fail_restore=0; witness_ok=TRUE;
    assert(!base && SudekiMpLanStoryAreaTaskInstall((HMODULE)mapped));
    InitializeCriticalSection((CRITICAL_SECTION *)(mapped+QUEUE_LOCK));
    go=CreateEventA(NULL,FALSE,FALSE,NULL);done=CreateEventA(NULL,FALSE,FALSE,NULL);assert(go&&done);
    command=0;worker=CreateThread(NULL,0,work,NULL,0,NULL);assert(worker);
    *(HANDLE *)(mapped+WORKER_HANDLE)=worker; mapped[WORKER_RUN]=1;
    memset(world,0,sizeof(world));memset(descriptors,0,sizeof(descriptors));memset(&policy,0,sizeof(policy));
    *(void **)world=mapped+0x2c4c3c;*(void **)(world+0x50)=descriptors;*(unsigned *)(world+0x54)=2;
    *(void **)(mapped+WORLD)=world;
    for(unsigned i=0;i<2;++i) *(void **)(descriptors+i*0x54)=mapped+0x2c82a4;
    *(const char **)(descriptors+0x24)="brightwater";
    *(const char **)(descriptors+0x54+0x24)="church";
    assert(SudekiMpStoryAreasInitialize(&policy,10));
    assert(SudekiMpStoryAreaLoad(&policy,"brightwater","",&outside));
    assert(SudekiMpStoryAreaReady(&policy,outside));
    assert(SudekiMpStoryAreaLoad(&policy,"brightwater","church",&inside));
    witness=(SudekiMpControlUpdateDispatchWitness){.dispatch_serial=9,.service_post_original_exact=1,
        .native_thread_id=GetCurrentThreadId()};
    request=VirtualAlloc(NULL,0x1000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(request);
    request_live=TRUE;free_on_destroy=FALSE;*(void **)request=mapped+TASK_VT;
    *(uint16_t *)(request+8)=0x401;*(unsigned *)(request+0xc)=0x41;
    *(void **)(request+0x10)=descriptors+0x54;
    enqueue_original=(void *)(uintptr_t)fake_enqueue;run_original=(void *)(uintptr_t)fake_run;
    destroy_original=(void *)(uintptr_t)fake_destroy;
    run_abi=fast_worker=native_body=FALSE; native_run_calls=native_destroy_calls=native_enqueue_calls=0;
    test_resource=(uintptr_t)loaded_resource;test_interface=test_resource+4;
}
static void arm(void) {
    assert(SudekiMpLanStoryAreaTaskArm((HMODULE)mapped,&witness,&policy,inside,descriptors+0x54,&ticket));
    assert(!SudekiMpStoryAreaRetireBegin(&policy,inside));
}
static SudekiMpLanStoryAreaTaskReceipt read_phase(unsigned phase) {
    SudekiMpLanStoryAreaTaskReceipt r={0};
    assert(SudekiMpLanStoryAreaTaskRead((HMODULE)mapped,ticket,&r) && r.phase==phase);
    assert(r.ticket==ticket && SudekiMpStoryAreaRefSame(r.area,inside)); return r;
}
static void submit(BOOL abi_test) {
    *(unsigned *)(descriptors+0x54+0x34)=1;
    if(abi_test) abi(0,request);
    else {event(ENQUEUE_BEGIN,request,0);enqueue_action();event(ENQUEUE_END,request,0);}
}
static void dispatch(void) {
    command=1;command_request=request;SetEvent(go);assert(WaitForSingleObject(done,5000)==WAIT_OBJECT_0);
}
static void finish(void) {
    assert(SudekiMpLanStoryAreaTaskUninstall());
    command=0;SetEvent(go);assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);
    CloseHandle(worker);CloseHandle(go);CloseHandle(done);
    DeleteCriticalSection((CRITICAL_SECTION *)(mapped+QUEUE_LOCK));
    *(HANDLE *)(mapped+WORKER_HANDLE)=INVALID_HANDLE_VALUE;*(void **)(mapped+WORLD)=NULL;mapped[WORKER_RUN]=0;
    assert(!callbacks && !worker_copy && !base);
    if(request_live) assert(VirtualFree(request,0,MEM_RELEASE));
    request=NULL;request_live=FALSE;
}
static void normal_and_fast(void) {
    fixture();arm();read_phase(SUDEKIMP_AREA_JOB_ARMED);
    assert(!SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));
    submit(TRUE);read_phase(SUDEKIMP_AREA_JOB_SUBMITTED);
    assert(!SudekiMpLanStoryAreaTaskDisarm((HMODULE)mapped,ticket));
    run_abi=TRUE;dispatch();
    SudekiMpLanStoryAreaTaskReceipt r=read_phase(SUDEKIMP_AREA_JOB_DESTROYED);
    assert(r.executed && !r.submitting && native_run_calls==1 && native_destroy_calls==1 && native_enqueue_calls==1);
    assert(policy.areas[inside.slot].phase==SUDEKIMP_STORY_AREA_LOADING);
    assert(SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));
    assert(!SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));
    SudekiMpLanStoryAreaTaskReceipt unchanged=r;
    assert(!SudekiMpLanStoryAreaTaskRead((HMODULE)mapped,ticket,&r));
    assert(!memcmp(&r,&unchanged,sizeof(r)));
    uint64_t old=ticket; *(unsigned *)(descriptors+0x54+0x34)=0;arm();assert(ticket>old);
    fast_worker=TRUE;submit(FALSE);r=read_phase(SUDEKIMP_AREA_JOB_DESTROYED);assert(!r.submitting);
    assert(SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));finish();
    fixture();arm();assert(ticket>old);assert(SudekiMpLanStoryAreaTaskDisarm((HMODULE)mapped,ticket));finish();
}
static void canceled_and_nonterminal(void) {
    fixture();arm();submit(FALSE);
    command=3;command_request=request;SetEvent(go);assert(WaitForSingleObject(done,5000)==WAIT_OBJECT_0);
    assert(read_phase(SUDEKIMP_AREA_JOB_RETURNED).executed);
    assert(!SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));
    SetEvent(go);assert(WaitForSingleObject(done,5000)==WAIT_OBJECT_0);
    assert(SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));finish();
    fixture();arm();submit(FALSE);
    event(DESTROY_BEGIN,request,1);read_phase(SUDEKIMP_AREA_JOB_DESTROYING);
    assert(!SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));
    event(DESTROY_END,request,1);
    assert(!read_phase(SUDEKIMP_AREA_JOB_DESTROYED).executed);
    assert(SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));finish();
    fixture();arm();submit(FALSE);abi(2,request);
    assert(!read_phase(SUDEKIMP_AREA_JOB_DESTROYED).executed && native_destroy_calls==1);
    assert(SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));finish();
    fixture();arm();
    assert(!SudekiMpLanStoryAreaTaskUninstall() && base && stopping);
    assert(SudekiMpLanStoryAreaTaskDisarm((HMODULE)mapped,ticket));finish();
}
static void freed_before_observation(void) {
    /* The real post-destructor bridge must not dereference the freed request,
     * including when destruction precedes the enqueue function's return. */
    fixture();arm();free_on_destroy=TRUE;fast_worker=TRUE;submit(FALSE);
    assert(!request_live && !readable(request,1));
    assert(read_phase(SUDEKIMP_AREA_JOB_DESTROYED).executed);
    assert(SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));finish();
    fixture();arm();submit(FALSE);free_on_destroy=TRUE;simple_call(2,request);
    assert(!request_live && !readable(request,1));
    assert(!read_phase(SUDEKIMP_AREA_JOB_DESTROYED).executed);
    assert(SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));finish();
}
static void refuse_arm(void) {
    uint64_t untouched=987;
    assert(!SudekiMpLanStoryAreaTaskArm((HMODULE)mapped,&witness,&policy,inside,descriptors+0x54,&untouched));
    assert(untouched==987);
}
static void malformed(void) {
    fixture();witness_ok=FALSE;refuse_arm();witness_ok=TRUE;
    *(unsigned *)(descriptors+0x54+0x34)=1;refuse_arm();*(unsigned *)(descriptors+0x54+0x34)=0;
    *(const char **)(descriptors+0x24)="church";refuse_arm();*(const char **)(descriptors+0x24)="brightwater";
    *(unsigned *)(world+0x54)=4097;refuse_arm();*(unsigned *)(world+0x54)=2;
    *(const char **)(descriptors+0x24)=NULL;refuse_arm();*(const char **)(descriptors+0x24)="brightwater";
    uint64_t saved=serial;serial=UINT64_MAX;refuse_arm();serial=saved;
    arm();refuse_arm();assert(SudekiMpLanStoryAreaTaskDisarm((HMODULE)mapped,ticket));finish();
}
static void restore_and_queue_gate(void) {
    for(unsigned i=1;i<=4;++i) {
        installs=restores=0;fail_install=i;fail_restore=0;
        assert(!SudekiMpLanStoryAreaTaskInstall((HMODULE)mapped) && !base);
    }
    fail_install=0;
    /* Failed rollback keeps every remaining original/owner available. */
    for(unsigned stage=2;stage<=4;++stage) for(unsigned failure=2;failure<=4;++failure) {
        if(5-failure>=stage) continue; /* Restore target not installed yet. */
        installs=restores=0;fail_install=stage;fail_restore=failure;
        assert(!SudekiMpLanStoryAreaTaskInstall((HMODULE)mapped));
        assert(base && stopping && enqueue_original && run_original && destroy_original && publish_continue);
        assert(restores==4 && !SudekiMpLanStoryAreaTaskInstall((HMODULE)mapped));
        fail_install=fail_restore=0;assert(SudekiMpLanStoryAreaTaskUninstall() && !base);
    }
    for(unsigned i=1;i<=4;++i) {
        fixture();restores=0;fail_restore=i;
        assert(!SudekiMpLanStoryAreaTaskUninstall() && base && enqueue_original && run_original && destroy_original);
        fail_restore=0;finish();
    }
    fixture(); command=2;SetEvent(go);assert(WaitForSingleObject(done,5000)==WAIT_OBJECT_0);
    assert(!SudekiMpLanStoryAreaTaskUninstall() && enqueue_hook.installed && run_hook.installed && destroy_hook.installed);
    SetEvent(go);assert(WaitForSingleObject(done,5000)==WAIT_OBJECT_0);
    *(void **)(mapped+ACTIVE)=request;assert(!SudekiMpLanStoryAreaTaskUninstall());*(void **)(mapped+ACTIVE)=NULL;
    finish();
    /* A foreign patch is not overwritten, and each independent restoration
     * is still attempted. Retry is possible once ownership is returned. */
    for(unsigned slot=0;slot<4;++slot) {
        fixture();restores=0;
        void **owner_slot=(void **)(mapped+TASK_VT+(slot==1?4:0));
        void *saved=*owner_slot;
        if(slot==3) mapped[PUBLISH_SITE]=0x90;
        else if(slot) *owner_slot=(void *)(uintptr_t)0x12345678;
        else mapped[ENQUEUE_SITE]=0x90;
        assert(!SudekiMpLanStoryAreaTaskUninstall() && restores==4 && base);
        if(slot==3) {assert(mapped[PUBLISH_SITE]==0x90);mapped[PUBLISH_SITE]=0xe9;}
        else if(slot) {assert(*owner_slot==(void *)(uintptr_t)0x12345678);*owner_slot=saved;}
        else {assert(mapped[ENQUEUE_SITE]==0x90);mapped[ENQUEUE_SITE]=0xe8;}
        finish();
    }
    fixture();arm();assert(SudekiMpLanStoryAreaTaskDisarm((HMODULE)mapped,ticket));
    HANDLE alternate=NULL;
    assert(DuplicateHandle(GetCurrentProcess(),worker,GetCurrentProcess(),&alternate,0,FALSE,DUPLICATE_SAME_ACCESS));
    *(HANDLE *)(mapped+WORKER_HANDLE)=alternate;
    assert(!SudekiMpLanStoryAreaTaskUninstall() && base && worker_copy);
    *(HANDLE *)(mapped+WORKER_HANDLE)=worker;assert(CloseHandle(alternate));finish();
}
static void fault(void) {
    assert(unknown);SudekiMpLanStoryAreaTaskReceipt r={.ticket=888},before=r;
    assert(!SudekiMpLanStoryAreaTaskRead((HMODULE)mapped,ticket,&r) && !memcmp(&r,&before,sizeof(r)));
    assert(!SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));
    assert(!SudekiMpLanStoryAreaTaskUninstall() && base);
    assert(!SudekiMpLanStoryAreaTaskInstall((HMODULE)mapped));
    /* Fixture-only cleanup AFTER fake callbacks returned. No production reset. */
    assert(!callbacks);unknown=FALSE;
    for(unsigned i=0;i<WATCHES;++i) if(watches[i].value.ticket) {
        assert(SudekiMpStoryAreaRelease(watches[i].policy,watches[i].value.area,watches[i].pin));
        memset(&watches[i],0,sizeof(watches[i]));
    }
    finish();
}
static DWORD WINAPI foreign(void *p) {event(ENQUEUE_BEGIN,p,0);event(ENQUEUE_END,p,0);return 0;}
static void unknowns(void) {
    fixture();arm();submit(FALSE);submit(FALSE);fault();
    fixture();arm();submit(FALSE);event(RUN_BEGIN,request,0);event(RUN_END,request,0);fault();
    fixture();arm();submit(FALSE);event(DESTROY_BEGIN,request,0);event(DESTROY_END,request,0);fault();
    fixture();arm();request[8]=0;submit(FALSE);fault();
    fixture();arm();*(unsigned *)(world+0x54)=1;submit(FALSE);fault();
    fixture();arm();HANDLE h=CreateThread(NULL,0,foreign,request,0,NULL);assert(h);
    assert(WaitForSingleObject(h,5000)==WAIT_OBJECT_0);CloseHandle(h);fault();
}
static void image_refusals(void) {
    static const unsigned gates[]={0x2956d0,0x2956d7,0x1bb20b,0x1bb214,0x1bb238,
        0x1bb250,0x1bb259,0x1bb28b,0x1bb283,0x10d4b0,0x10d4c8,0x10d4de,0x10d500,0x72340,TASK_VT,
        0x10d5c7,0x10d5d4,0x10d5e0,0x10d5ea,0x10d5ef,PUBLISH_SITE};
    for(unsigned i=0;i<sizeof(gates)/sizeof(gates[0]);++i) {
        mapped[gates[i]]^=1;assert(!SudekiMpLanStoryAreaTaskInstall((HMODULE)mapped) && !base);
        mapped[gates[i]]^=1;
    }
    *(HANDLE *)(mapped+WORKER_HANDLE)=NULL;
    assert(!SudekiMpLanStoryAreaTaskInstall((HMODULE)mapped) && !base);
    *(HANDLE *)(mapped+WORKER_HANDLE)=INVALID_HANDLE_VALUE;
    static const unsigned globals[]={WORLD,ACTIVE,QUEUE};
    for(unsigned i=0;i<sizeof(globals)/sizeof(globals[0]);++i) {
        *(void **)(mapped+globals[i])=mapped;
        assert(!SudekiMpLanStoryAreaTaskInstall((HMODULE)mapped) && !base);
        *(void **)(mapped+globals[i])=NULL;
    }
    assert(image_exact((HMODULE)mapped));
}
static void publication_abi(void) {
    fixture();arm();submit(FALSE);
    publish_continue=(void *)(uintptr_t)return_only;
    command=5;command_request=request;SetEvent(go);assert(WaitForSingleObject(done,5000)==WAIT_OBJECT_0);
    SudekiMpLanStoryAreaTaskReceipt r=read_phase(SUDEKIMP_AREA_JOB_DESTROYED);
    assert(r.published && r.published_descriptor==(uintptr_t)(descriptors+0x54) &&
        r.published_interface==test_interface && r.published_resource==test_resource &&
        r.published_resource_generation==resource_generation);
    assert(*(uintptr_t *)(descriptors+0x54+0x10)==test_interface &&
        *(uintptr_t *)(descriptors+0x54+0x18)==test_resource);
    assert(SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));finish();
}
static void publication_faults(void) {
    for(unsigned i=0;i<8;++i) {
        fixture();arm();submit(FALSE);publication_fault_mode=i;
        command=6;command_request=request;SetEvent(go);assert(WaitForSingleObject(done,5000)==WAIT_OBJECT_0);
        assert(native_destroy_calls==1);fault();
    }
    fixture();arm();submit(FALSE);
    publication(request,descriptors+0x54,test_interface,test_resource);fault(); /* Wrong thread/phase. */
    fixture();arm();
    publication(mapped,mapped,test_interface,test_resource);assert(!unknown); /* Unwatched native job. */
    assert(SudekiMpLanStoryAreaTaskDisarm((HMODULE)mapped,ticket));finish();
}
/* These stubs replace only the four dependency calls in the native request.
 * Lookup returns a fixture holder; the actual native type check, stores,
 * early returns, Run bridge and publication bridge all execute. */
static void __attribute__((naked,noinline)) name_stub(void) {__asm__ volatile("ret $4");}
static void __attribute__((naked,noinline)) key_stub(void) {
    __asm__ volatile("movl $0x33,(%edi); movl $123,4(%edi); movl $0,8(%edi); ret");
}
static void __attribute__((naked,noinline)) lookup_stub(void) {
    __asm__ volatile("incl _lookup_count; movl $0x33,4(%esi); movl $123,8(%esi);"
        "movl $0,12(%esi); mov _test_interface,%eax; mov %eax,16(%esi); mov %esi,%eax; ret $16");
}
static void __attribute__((naked,noinline)) keycopy_stub(void) {
    __asm__ volatile("incl _keycopy_count; mov (%eax),%edx; mov %edx,(%ecx);"
        "mov 4(%eax),%edx; mov %edx,4(%ecx); mov 8(%eax),%edx; mov %edx,8(%ecx);"
        "mov _retarget_descriptor,%edx; test %edx,%edx; je 1f; mov %edx,0x10(%ebx);"
        "1: mov %ecx,%eax; ret");
}
static void __attribute__((naked,noinline)) type_stub(void) {__asm__ volatile("mov _returned_type,%eax; ret");}
static unsigned yield_count;
static BOOL WINAPI yield_stub(void) {
    ++yield_count;*(unsigned *)(descriptors+0x54+0x34)=0;return TRUE;
}
static void native_publication(void) {
    for(unsigned mode=0;mode<5;++mode) {
        fixture();arm();submit(FALSE);native_body=TRUE;run_original=mapped+RUN_NATIVE;
        memset(manager,0,sizeof(manager));memset(loaded_resource,0,sizeof(loaded_resource));
        memset(loaded_table,0,sizeof(loaded_table));loaded_table[4]=(void *)(uintptr_t)type_stub;
        *(void **)(loaded_resource+4)=loaded_table;*(void **)(mapped+0x409d8c)=manager;
        InitializeCriticalSection((CRITICAL_SECTION *)(manager+0x14c));
        returned_type=mode==1?0x29:0x33;lookup_count=keycopy_count=yield_count=0;
        retarget_descriptor=mode==4?descriptors:NULL;
        if(mode==2) *(unsigned *)(descriptors+0x54+0x34)=0;
        if(mode==3) *(void **)(descriptors+0x54+0x18)=loaded_resource;
        SudekiMpRelativeCallHook deps[4]={0};SudekiMpPointerHook imports[3]={0};
        const unsigned sites[]={0x10d57f,0x10d58e,0x10d5a4,0x10d5ea};
        const unsigned targets[]={0x49c0,0x3eb80,0x11730,0x4bc0};
        void *stubs[]={(void *)(uintptr_t)name_stub,(void *)(uintptr_t)key_stub,
            (void *)(uintptr_t)lookup_stub,(void *)(uintptr_t)keycopy_stub};
        for(unsigned i=0;i<4;++i)
            assert(SudekiMpInstallRelativeCallHook(&deps[i],mapped+sites[i],mapped+targets[i],stubs[i]));
        const unsigned slots[]={0x29a068,0x29a064,0x29a094};
        void *apis[]={(void *)(uintptr_t)EnterCriticalSection,(void *)(uintptr_t)LeaveCriticalSection,
            (void *)(uintptr_t)yield_stub};
        for(unsigned i=0;i<3;++i)
            assert(SudekiMpInstallPointerHook(&imports[i],(void **)(mapped+slots[i]),*(void **)(mapped+slots[i]),apis[i]));
        DWORD old,discard;assert(VirtualProtect(mapped+0x10d000,0x1000,PAGE_EXECUTE_READWRITE,&old));
        assert(FlushInstructionCache(GetCurrentProcess(),mapped+0x10d000,0x1000));
        dispatch();
        SudekiMpLanStoryAreaTaskReceipt r={0};
        if(mode==4) {
            assert(unknown && !callbacks && !watches[0].value.published);
            assert(*(uintptr_t *)(descriptors+0x54+0x10)==test_interface &&
                *(uintptr_t *)(descriptors+0x18)==test_resource &&
                !*(uintptr_t *)(descriptors+0x54+0x18));
        } else {
            r=read_phase(SUDEKIMP_AREA_JOB_DESTROYED);
            assert(r.executed && !r.submitting && native_destroy_calls==1);
        }
        if(mode<2) {
            assert(r.published && r.published_descriptor==(uintptr_t)(descriptors+0x54) &&
                r.published_interface==test_interface && r.published_resource==(mode==1?0:test_resource) &&
                r.published_resource_generation==(mode==1?0:resource_generation));
            assert(*(uintptr_t *)(descriptors+0x54+0x10)==test_interface &&
                *(uintptr_t *)(descriptors+0x54+0x18)==r.published_resource);
            assert(lookup_count==1 && keycopy_count==1 && !yield_count);
        } else if(mode<4) assert(!r.published && !r.published_resource && !r.published_resource_generation &&
            !lookup_count && !keycopy_count &&
            yield_count==(mode==3?1u:0u));
        assert(policy.areas[inside.slot].phase==SUDEKIMP_STORY_AREA_LOADING);
        assert(VirtualProtect(mapped+0x10d000,0x1000,old,&discard));
        for(unsigned i=4;i-->0;) assert(SudekiMpRestoreRelativeCallHook(&deps[i]));
        for(unsigned i=3;i-->0;) assert(SudekiMpRestorePointerHook(&imports[i]));
        DeleteCriticalSection((CRITICAL_SECTION *)(manager+0x14c));*(void **)(mapped+0x409d8c)=NULL;
        if(mode==4) fault();
        else {assert(SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));finish();}
    }
}
/* Link the real resource observer, with only the broad native constructor and
 * destructor bodies replaced by ABI-matched fixture dependencies. The actual
 * production root bridges and cross-owner locking execute. */
static void root_capture_refused(uintptr_t address) {
    uint64_t untouched=987;
    assert(!SudekiMpLanStoryAreaResourceCapture((HMODULE)mapped,address,&untouched) && untouched==987);
}
static void *WINAPI construct_resource(void *self,unsigned a,unsigned b,unsigned c) {
    assert(!a && !b && !c);root_capture_refused((uintptr_t)self);return self;
}
static void __attribute__((thiscall)) destroy_resource(void *self) {
    root_capture_refused((uintptr_t)self);
}
static void root_birth(void) {
    typedef void *(WINAPI *Construct)(void *,unsigned,unsigned,unsigned);
    assert(((Construct)construct_resource_entry)(loaded_resource,0,0,0)==loaded_resource);
    uint64_t previous=resource_generation;
    assert(SudekiMpLanStoryAreaResourceCapture((HMODULE)mapped,(uintptr_t)loaded_resource,&resource_generation));
    assert(resource_generation>previous);
}
static void root_die(void) {
    typedef void (__attribute__((thiscall)) *Destroy)(void *);
    ((Destroy)destroy_resource_entry)(loaded_resource);
    root_capture_refused((uintptr_t)loaded_resource);
}
static void root_ack(uint64_t generation) {
    assert(SudekiMpLanStoryAreaResourceAcknowledge((HMODULE)mapped,&resource_consumer,&witness,generation));
}
static void resource_start(void) {
    assert(!SudekiMpLanStoryAreaResourceJournalReady((HMODULE)mapped));
    fixture();refuse_arm();finish(); /* Missing owner cannot create a watch. */
    assert(SudekiMpLanStoryAreaFinaliseInstall((HMODULE)mapped));
    fixture();refuse_arm();finish(); /* Installed without a consumer also refuses. */
    assert(SudekiMpLanStoryAreaFinaliseAttach((HMODULE)mapped,&resource_consumer));
    assert(SudekiMpLanStoryAreaResourceJournalReady((HMODULE)mapped));
    int32_t displacement;
    memcpy(&displacement,mapped+0x3cc4e,4);construct_resource_entry=mapped+0x3cc52+displacement;
    memcpy(&displacement,mapped+0x1076b4,4);destroy_resource_entry=mapped+0x1076b8+displacement;
    assert(SudekiMpInstallInlineHook(&resource_stubs[0],mapped+0x107430,mapped+0x107430,6,
        (void *)(uintptr_t)construct_resource));
    assert(SudekiMpInstallInlineHook(&resource_stubs[1],mapped+0x107a10,mapped+0x107a10,6,
        (void *)(uintptr_t)destroy_resource));
    assert(VirtualProtect(mapped+0x107000,0x1000,PAGE_EXECUTE_READWRITE,&resource_page_protection));
    assert(FlushInstructionCache(GetCurrentProcess(),mapped+0x107000,0x1000));
    root_capture_refused((uintptr_t)loaded_resource);root_birth();
}
static void published_request(void) {
    fixture();arm();submit(FALSE);publish_continue=(void *)(uintptr_t)return_only;
    command=5;command_request=request;SetEvent(go);assert(WaitForSingleObject(done,5000)==WAIT_OBJECT_0);
}
static void resource_linkage(void) {
    published_request();uint64_t first=resource_generation;
    assert(read_phase(SUDEKIMP_AREA_JOB_DESTROYED).published_resource_generation==first);
    root_die();root_birth();
    SudekiMpLanStoryAreaResourceReceipt roots[2];unsigned count=0;
    assert(SudekiMpLanStoryAreaResourceSnapshot((HMODULE)mapped,&resource_consumer,&witness,roots,2,&count) && count==2);
    assert(roots[0].resource==roots[1].resource && roots[0].generation!=roots[1].generation);
    /* Coordinator consumes old root history, then acknowledges it. The request
     * must still return its original copied generation, never the replacement. */
    root_ack(first);
    assert(read_phase(SUDEKIMP_AREA_JOB_DESTROYED).published_resource_generation==first);
    assert(SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));finish();
    published_request();
    assert(read_phase(SUDEKIMP_AREA_JOB_DESTROYED).published_resource_generation==resource_generation);
    assert(SudekiMpLanStoryAreaTaskRelease((HMODULE)mapped,ticket));finish();
    for(unsigned missing=0;missing<2;++missing) {
        fixture();arm();submit(FALSE);
        if(missing) root_die();else {test_resource+=0x1000;test_interface=test_resource+4;}
        publication_fault_mode=8;command=6;command_request=request;
        SetEvent(go);assert(WaitForSingleObject(done,5000)==WAIT_OBJECT_0);
        assert(native_destroy_calls==1 && !watches[0].value.published &&
            !watches[0].value.published_resource_generation && !watches[0].value.published_resource);
        assert(!SudekiMpStoryAreaRetireBegin(&policy,inside));fault();
        if(missing) {root_ack(resource_generation);root_birth();}
    }
}
static void resource_finish(void) {
    root_die();root_ack(resource_generation);
    assert(SudekiMpLanStoryAreaFinaliseDetach((HMODULE)mapped,&resource_consumer,&witness));
    assert(!SudekiMpLanStoryAreaResourceJournalReady((HMODULE)mapped));
    root_capture_refused((uintptr_t)loaded_resource);
    for(unsigned i=2;i-->0;) assert(SudekiMpRestoreInlineHook(&resource_stubs[i]));
    DWORD discard;assert(VirtualProtect(mapped+0x107000,0x1000,resource_page_protection,&discard));
    assert(!SudekiMpLanStoryAreaFinaliseUninstall());
    /* Native callbacks have occurred: observer and mapped image stay alive
     * until isolated process exit, even though the fixture history is empty. */
}
static void resource_link_fault(BOOL missing_consumer) {
    if(missing_consumer) {root_die();root_ack(resource_generation);}
    fixture();arm();submit(FALSE);
    void *saved=*(void **)(mapped+0x2c89ac);
    if(missing_consumer) assert(SudekiMpLanStoryAreaFinaliseDetach((HMODULE)mapped,&resource_consumer,&witness));
    else *(void **)(mapped+0x2c89ac)=mapped; /* Foreign patch after request admission. */
    publication_fault_mode=8;command=6;command_request=request;
    SetEvent(go);assert(WaitForSingleObject(done,5000)==WAIT_OBJECT_0);
    assert(native_destroy_calls==1 && !watches[0].value.published &&
        !watches[0].value.published_resource_generation && !watches[0].value.published_resource);
    assert(!SudekiMpStoryAreaRetireBegin(&policy,inside));fault();
    *(void **)(mapped+0x2c89ac)=saved;
    assert(!SudekiMpLanStoryAreaResourceJournalReady((HMODULE)mapped));
    root_capture_refused((uintptr_t)loaded_resource);
    assert(!SudekiMpLanStoryAreaFinaliseUninstall()); /* Do not reset unknown live history. */
}
int main(int argc,char **argv) {
    assert(argc==2 || argc==3);wchar_t path[1024];assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    mapped=map_image(path);assert(!SudekiMpLanStoryAreaTaskInstall(NULL));
    assert(image_exact((HMODULE)mapped));
    image_refusals();resource_start();
    if(argc==3) {
        assert(!strcmp(argv[2],"missing-consumer") || !strcmp(argv[2],"unknown-resource"));
        resource_link_fault(!strcmp(argv[2],"missing-consumer"));
        puts("StoryAreaTaskImageTest: PASS (resource owner lost after admission; request retains policy and refuses receipt)");return 0;
    }
    restore_and_queue_gate();normal_and_fast();canceled_and_nonterminal();
    freed_before_observation();malformed();unknowns();
    publication_abi();publication_faults();native_publication();resource_linkage();
    assert(image_exact((HMODULE)mapped));resource_finish();
    puts("StoryAreaTaskImageTest: PASS (threaded/native request publication bound to real resource journal; synthetic lookup/root bodies; no real loading)");return 0;
}
