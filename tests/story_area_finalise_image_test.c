/* Most modes map a NON-executable image with synthetic bridge originals.
 * pvs-native modes execute selected native finalisation/queue/callback routines
 * with synthetic resource dependencies. No real loading, scheduler, renderer,
 * scripts or gameplay executes. Separate processes preserve sticky history. */
#include "hooks/call_hook.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned installs,restores,fail_install,fail_restore;
static BOOL pi(SudekiMpPointerHook *,void **,const void *,const void *);
static BOOL pr(SudekiMpPointerHook *);
static BOOL ci(SudekiMpRelativeCallHook *,uint8_t *,const void *,const void *);
static BOOL cr(SudekiMpRelativeCallHook *);
static BOOL ii(SudekiMpInlineHook *,uint8_t *,const uint8_t *,size_t,const void *);
static BOOL ir(SudekiMpInlineHook *);
static DWORD test_thread(void);
#define SudekiMpInstallPointerHook pi
#define SudekiMpRestorePointerHook pr
#define SudekiMpInstallRelativeCallHook ci
#define SudekiMpRestoreRelativeCallHook cr
#define SudekiMpInstallInlineHook ii
#define SudekiMpRestoreInlineHook ir
#define GetCurrentThreadId test_thread
#include "../src/hooks/lan_story_area_finalise.c"
#undef GetCurrentThreadId
#undef SudekiMpInstallPointerHook
#undef SudekiMpRestorePointerHook
#undef SudekiMpInstallRelativeCallHook
#undef SudekiMpRestoreRelativeCallHook
#undef SudekiMpInstallInlineHook
#undef SudekiMpRestoreInlineHook
static BOOL pi(SudekiMpPointerHook *h,void **p,const void *e,const void *r) {
    if(++installs==fail_install) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpInstallPointerHook(h,p,e,r);
}
static BOOL pr(SudekiMpPointerHook *h) {
    if(++restores==fail_restore) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpRestorePointerHook(h);
}
static BOOL ci(SudekiMpRelativeCallHook *h,uint8_t *p,const void *e,const void *r) {
    if(++installs==fail_install) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpInstallRelativeCallHook(h,p,e,r);
}
static BOOL cr(SudekiMpRelativeCallHook *h) {
    if(++restores==fail_restore) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpRestoreRelativeCallHook(h);
}
static BOOL ii(SudekiMpInlineHook *h,uint8_t *p,const uint8_t *e,size_t n,const void *r) {
    if(++installs==fail_install) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpInstallInlineHook(h,p,e,n,r);
}
static BOOL ir(SudekiMpInlineHook *h) {
    if(++restores==fail_restore) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpRestoreInlineHook(h);
}
static BOOL disturb,witness_ok=TRUE;
static DWORD test_thread(void) {
    DWORD t=GetCurrentThreadId();
    if(disturb) {SetLastError(0x9999);__asm__ volatile("fninit; fld1; pxor %%xmm0,%%xmm0; pxor %%xmm7,%%xmm7" ::: "memory");}
    return t;
}
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(const SudekiMpControlUpdateDispatchWitness *w) {
    return witness_ok && w && w->native_thread_id==GetCurrentThreadId();
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(f!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(f,NULL),got=0;
    uint8_t *raw=malloc(size);assert(raw && ReadFile(f,raw,size,&got,NULL) && got==size);CloseHandle(f);
    IMAGE_DOS_HEADER *dos=(void *)raw;IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(b);memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);IMAGE_SECTION_HEADER *s=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        assert(s[i].PointerToRawData<=size && s[i].SizeOfRawData<=size-s[i].PointerToRawData);
        assert(s[i].VirtualAddress<=nt->OptionalHeader.SizeOfImage && s[i].SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s[i].VirtualAddress);
        memcpy(b+s[i].VirtualAddress,raw+s[i].PointerToRawData,s[i].SizeOfRawData);
    }
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    for(unsigned offset=0;offset<reloc.Size;) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(void *)(block+1);unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfff);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {assert(rva<=nt->OptionalHeader.SizeOfImage-4);*(uint32_t *)(b+rva)+=(uint32_t)delta;}
        }
        offset+=block->SizeOfBlock;
    }
    free(raw);return b;
}
static uint8_t *mapped;
static HMODULE image;
static int coordinator;
static uint8_t native_manager[0x500],native_resource[0x140];
static SudekiMpControlUpdateDispatchWitness witness;
static Record snapshot[SUDEKIMP_AREA_FINALISERS];
static unsigned observed_count,run_result __attribute__((used));
static void *active_task __attribute__((used));
static void *resource_for_submit __attribute__((used));
static unsigned action,fast_enqueue,free_task,nesting;
static uint8_t native_descriptor[0x54];
static void *visibility_node __attribute__((used));
static void *other_node;
static void *owned_node_expected;
typedef void (__cdecl *NodeFreeFn)(void *);
static NodeFreeFn volatile invoke_node_free;
static unsigned pvs_mode,free_calls;
static void run_action(void) __attribute__((used,noinline));
static unsigned enqueue_calls __attribute__((used)),run_calls __attribute__((used));
static unsigned delete_calls __attribute__((used)),update_calls __attribute__((used));
static void *enqueued_task __attribute__((used)),*enqueued_resource __attribute__((used)),*enqueued_manager __attribute__((used));
static unsigned update_argument __attribute__((used));
typedef unsigned char (__attribute__((thiscall)) *RunFn)(void *);
typedef void * (__attribute__((thiscall)) *DeleteFn)(void *,unsigned);
typedef void (__attribute__((thiscall)) *UpdateFn)(void *,unsigned);
static RunFn volatile invoke_run;
static DeleteFn volatile invoke_delete;
static UpdateFn volatile invoke_update;
static void update(void) {invoke_update(native_manager+4,0x1234abcd);}
static void check_refused(void) {
    Record out;memset(&out,0x5a,sizeof(out));Record before=out;unsigned n=777;
    assert(!SudekiMpLanStoryAreaFinaliseSnapshot(image,&coordinator,&witness,&out,1,&n));
    assert(n==777 && !memcmp(&out,&before,sizeof(out)));
}
static void take(void) {
    assert(SudekiMpLanStoryAreaFinaliseSnapshot(image,&coordinator,&witness,snapshot,SUDEKIMP_AREA_FINALISERS,&observed_count));
}
static void __attribute__((used,noinline)) delete_action(void) {
    if(free_task) {assert(VirtualFree(active_task,0,MEM_RELEASE));free_task=0;}
    check_refused(); /* Before destructor-return observer, even if memory is gone. */
}
static void __attribute__((used,noinline)) update_action(void) {
    assert(update_argument==0x1234abcd);check_refused();
    if(action&1) (void)invoke_run(active_task);
    if(action&2) (void)invoke_delete(active_task,1);
    if(action&4) {if(!nesting) {nesting=1;update();nesting=0;}}
    check_refused(); /* Destruction alone is insufficient while outer dispatch runs. */
}
static void __attribute__((used,noinline)) enqueue_action(void) {
    if(fast_enqueue) {
        update();Record *r=by_task((uintptr_t)active_task,TRUE);
        assert(r && r->destroyed && r->submitting);check_refused();
        assert(!SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,r->ticket));
    }
}
#define CALL_TEST(fn) "pushfl; pushal; mov %esp,%ebp; and $-16,%esp; sub $16,%esp; cld; call _" fn "; mov %ebp,%esp; popal; popfl;"
static void __attribute__((naked,noinline)) fake_enqueue(void) {
    __asm__ volatile("incl _enqueue_calls; mov %esi,_enqueued_task; mov %edi,_enqueued_resource;"
        "push %eax; mov 8(%esp),%eax; mov %eax,_enqueued_manager; pop %eax;"
        CALL_TEST("enqueue_action") "stc; ret $4");
}
static void __attribute__((naked,noinline)) fake_run(void) {
    __asm__ volatile("incl _run_calls;" CALL_TEST("run_action") "mov $0xabcdee00,%eax; or _run_result,%eax; stc; ret");
}
static void __attribute__((naked,noinline)) fake_delete(void) {
    __asm__ volatile("incl _delete_calls;" CALL_TEST("delete_action") "mov %ecx,%eax; stc; ret $4");
}
static void __attribute__((naked,noinline)) fake_update(void) {
    __asm__ volatile("incl _update_calls; push %eax; mov 8(%esp),%eax; mov %eax,_update_argument; pop %eax;"
        CALL_TEST("update_action") "mov $0x44556677,%eax; stc; ret $4");
}
/* ABI-only originals do not perform fixture C calls that would legitimately
 * modify native FP/SSE state. Observer calls deliberately disturb that state. */
static void __attribute__((naked,noinline)) abi_enqueue(void) {__asm__ volatile("stc; ret $4");}
static void __attribute__((naked,noinline)) abi_delete(void) {__asm__ volatile("mov %ecx,%eax; stc; ret $4");}
static void __attribute__((naked,noinline)) abi_update(void) {__asm__ volatile("mov $0x44556677,%eax; stc; ret $4");}
static void __attribute__((naked,noinline)) abi_run(void) {__asm__ volatile("mov $0xabcdee00,%eax; or _run_result,%eax; stc; ret");}
static void __attribute__((naked,noinline)) abi_passthrough(void) {__asm__ volatile("stc; ret");}
static void __attribute__((naked,noinline)) abi_ret8(void) {__asm__ volatile("stc; ret $8");}
static void __attribute__((naked,noinline)) abi_construct(void) {__asm__ volatile("mov 4(%esp),%eax; stc; ret $16");}
static void __cdecl fake_node_free(void *node) {
    assert(node);++free_calls;
    if(owned_node_expected) {
        assert(pending_node((uintptr_t)owned_node_expected));
        assert(free_calls==1?node!=owned_node_expected:node==owned_node_expected);
    }
    assert(VirtualFree(node,0,MEM_RELEASE));check_refused();
}
static void __attribute__((naked,noinline)) scope(unsigned end __attribute__((unused))) {
    __asm__ volatile("push %ebx; push %ebp; mov $_native_resource,%ebx; mov $_native_descriptor,%ebp;"
        "cmpl $0,12(%esp); jne 1f; call _pvs_begin_entry; jmp 2f;"
        "1: call _pvs_end_entry; 2: pop %ebp; pop %ebx; ret");
}
static void __attribute__((naked,noinline)) publish(void *node __attribute__((unused)),
    void *callback_object __attribute__((unused)),void *argument __attribute__((unused))) {
    __asm__ volatile("mov 4(%esp),%eax; mov 8(%esp),%ecx; mov 12(%esp),%edx;"
        "push %ecx; push $0; push $0; push $0; push $0; call _node_publish_entry; lea 20(%esp),%esp; ret");
}
static void run_action(void) {
    if(!pvs_mode) return;
    if(pvs_mode==4) {scope(1);return;}
    if(pvs_mode==5) {scope(0);scope(1);return;}
    scope(0);publish(visibility_node,native_descriptor+0x48,native_descriptor);
    if(pvs_mode==2) {
        invoke_node_free(visibility_node);
        Record *r=running_resource((uintptr_t)native_resource);
        assert(r && r->pvs_retired && r->pvs_submitting);check_refused();
    }
    if(pvs_mode==3) publish(other_node,native_descriptor+0x48,native_descriptor);
    scope(1);
}
static void originals(void) {
    resource_for_submit=native_resource;
    invoke_run=(RunFn)(uintptr_t)run_entry;invoke_delete=(DeleteFn)(uintptr_t)delete_entry;
    invoke_update=(UpdateFn)(uintptr_t)update_entry;
    enqueue_original=(void *)fake_enqueue;run_original=(void *)fake_run;
    delete_original=(void *)fake_delete;update_original=(void *)fake_update;
    pvs_begin_original=pvs_end_original=node_publish_original=(void *)(uintptr_t)abi_passthrough;
    node_free_original=(void *)(uintptr_t)fake_node_free;
    invoke_node_free=(NodeFreeFn)(uintptr_t)node_free_entry;
}
static void __attribute__((naked,noinline)) submit(void *task __attribute__((unused))) {
    __asm__ volatile("push %esi; push %edi; mov 12(%esp),%esi; mov _resource_for_submit,%edi;"
        "push $_native_manager; call _enqueue_entry; pop %edi; pop %esi; ret");
}
static DWORD WINAPI producer(void *p) {submit(p);return 0;}
static void faults(void) {
    assert(image_exact(image));
    const unsigned mismatch[]={0x3cd4e,ENQUEUE,RUN,DESTROY,0x3cf55,0x3e2f7,0x3e481,0x132022,TASK_VT,UPDATE_SLOT,
        PVS_BEGIN_SITE,PVS_END_SITE,NODE_PUBLISH_SITE,0x20a1d0,NODE_FREE_SITE,
        RETIRE_COPY_SITE,RETIRE_ENQUEUE_SITE,RETIRE_DELETE_SLOT,RETIRE_DELETE,0x107a93,0x10de19,0x497e,0x2cde80,0x2cde84,
        0x107aab,0x107b01,0x3e3e6,0x3e43c,0x3e44e,0x107a07,
        RESOURCE_CONSTRUCT_SITE,RESOURCE_BODY_SITE,RESOURCE_CONSTRUCT,RESOURCE_BODY,0x1076ab,0x1076b0,0x1076b8};
    for(unsigned i=0;i<sizeof(mismatch)/sizeof(*mismatch);++i) {
        mapped[mismatch[i]]^=1;assert(!SudekiMpLanStoryAreaFinaliseInstall(image));mapped[mismatch[i]]^=1;assert(!base);
    }
    for(unsigned i=1;i<=13;++i) {
        installs=restores=0;fail_install=i;
        assert(!SudekiMpLanStoryAreaFinaliseInstall(image) && !base);
        fail_install=0;assert(image_exact(image));
    }
    for(unsigned i=1;i<=13;++i) {
        installs=restores=0;assert(SudekiMpLanStoryAreaFinaliseInstall(image));
        fail_restore=i;assert(!SudekiMpLanStoryAreaFinaliseUninstall() && base && enqueue_original && update_original);
        fail_restore=0;restores=0;assert(SudekiMpLanStoryAreaFinaliseUninstall() && !base && image_exact(image));
    }
    installs=restores=0;fail_install=8;fail_restore=2;
    assert(!SudekiMpLanStoryAreaFinaliseInstall(image) && base && run_original);
    fail_install=fail_restore=0;assert(SudekiMpLanStoryAreaFinaliseUninstall() && image_exact(image));
    for(unsigned i=0;i<13;++i) {
        assert(SudekiMpLanStoryAreaFinaliseInstall(image));
        void **slot=i==0?delete_hook.slot:i==1?run_hook.slot:i==8?retire_delete_hook.slot:update_hook.slot;
        unsigned site=i==3?ENQUEUE:i==4?NODE_FREE_SITE:i==5?PVS_BEGIN_SITE:i==6?PVS_END_SITE:
            i==9?RETIRE_COPY_SITE:i==10?RETIRE_ENQUEUE_SITE:i==11?RESOURCE_CONSTRUCT_SITE:
            i==12?RESOURCE_BODY_SITE:NODE_PUBLISH_SITE;
        uint8_t saved[4];void *saved_pointer=NULL;
        if(i>=3 && i!=8) {memcpy(saved,mapped+site+1,4);mapped[site+1]^=0x40;}
        else {saved_pointer=*slot;*slot=(void *)0x12345678;}
        assert(!SudekiMpLanStoryAreaFinaliseUninstall() && base);
        if(i>=3 && i!=8) memcpy(mapped+site+1,saved,4);else *slot=saved_pointer;
        assert(SudekiMpLanStoryAreaFinaliseUninstall() && image_exact(image));
    }
    assert(SudekiMpLanStoryAreaFinaliseInstall(image));
    assert(SudekiMpLanStoryAreaFinaliseAttach(image,&coordinator));
    assert(!SudekiMpLanStoryAreaFinaliseUninstall());
    assert(SudekiMpLanStoryAreaFinaliseDetach(image,&coordinator,NULL));
    assert(SudekiMpLanStoryAreaFinaliseUninstall() && image_exact(image));
}
static void resource_birth(uintptr_t root) {
    resource_event(RESOURCE_BEGIN,root,0);resource_event(RESOURCE_END,root,root);
}
static void start(void) {
    assert(SudekiMpLanStoryAreaFinaliseInstall(image));
    assert(SudekiMpLanStoryAreaFinaliseAttach(image,&coordinator));originals();
    witness.native_thread_id=GetCurrentThreadId();witness.dispatch_serial=1;witness.service_post_original_exact=TRUE;
    resource_birth((uintptr_t)native_resource-4);
}
static uint64_t one(void) {take();assert(observed_count==1);return snapshot[0].ticket;}
static void normal(void) {
    start();active_task=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(active_task);
    HANDLE worker=CreateThread(NULL,0,producer,active_task,0,NULL);assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);CloseHandle(worker);
    uint64_t first=one();assert(snapshot[0].task==(uintptr_t)active_task && snapshot[0].resource==(uintptr_t)native_resource);
    Record unchanged;memset(&unchanged,0x5a,sizeof(unchanged));Record before=unchanged;unsigned n=99;
    assert(!SudekiMpLanStoryAreaFinaliseSnapshot(image,&coordinator,&witness,&unchanged,0,&n));
    assert(n==99 && !memcmp(&unchanged,&before,sizeof(before)));
    assert(!SudekiMpLanStoryAreaFinaliseSnapshot(image,native_resource,&witness,&unchanged,1,&n));
    assert(n==99 && !memcmp(&unchanged,&before,sizeof(before)));
    assert(snapshot[0].manager==(uintptr_t)native_manager && !snapshot[0].runs && !snapshot[0].submitting);
    assert(enqueued_task==active_task && enqueued_resource==native_resource && enqueued_manager==native_manager);
    assert(!SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,first));
    action=1;run_result=0;update();one();assert(snapshot[0].runs==1 && !snapshot[0].terminal_result);
    action=3;run_result=1;free_task=1;update();one();
    assert(snapshot[0].runs==2 && snapshot[0].terminal_result && snapshot[0].destroyed && !snapshot[0].destroying);
    assert(!callbacks && !dispatch_depth && !unknown);
    witness_ok=FALSE;check_refused();assert(!SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,first));witness_ok=TRUE;
    /* Reuse the identical opaque address before acknowledging the old record.
     * Neither the observer nor synthetic originals dereference this freed task. */
    submit(active_task);take();assert(observed_count==2);
    uint64_t second=snapshot[1].ticket;assert(second>first && snapshot[1].task==snapshot[0].task);
    action=2;update();take();assert(snapshot[1].destroyed && !snapshot[1].runs && !snapshot[1].terminal_result);
    assert(SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,first));
    assert(!SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,first));
    assert(SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,second));
    action=3;fast_enqueue=1;submit(active_task);fast_enqueue=0;
    uint64_t third=one();assert(third>second && snapshot[0].destroyed && !snapshot[0].submitting);
    assert(SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,third));
    action=4;update();assert(!dispatch_depth && !unknown);take();assert(!observed_count);
    assert(!SudekiMpLanStoryAreaFinaliseDetach(image,&coordinator,&witness)); /* Root still live. */
    assert(!SudekiMpLanStoryAreaFinaliseUninstall() && base && update_original && installed);
    assert(!SudekiMpLanStoryAreaFinaliseInstall(image));
}
static unsigned abi_kind __attribute__((used));
static uint32_t abi_flags __attribute__((used));
static void *abi_target __attribute__((used));
static uint32_t regs[9] __attribute__((used)),stack_before __attribute__((used));
static void __attribute__((naked,noinline)) invoke_abi(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,_stack_before; mov $0x11111111,%eax; mov _active_task,%ecx;"
        "mov $0x33333333,%edx; mov $0x44444444,%ebx; mov $0x55555555,%ebp;"
        "mov $0x66666666,%esi; mov $0x77777777,%edi;"
        "cmpl $0,_abi_kind; jne 1f; mov _active_task,%esi; mov $_native_resource,%edi; push $_native_manager;"
        "1: cmpl $2,_abi_kind; jne 2f; push $1;"
        "2: cmpl $3,_abi_kind; jne 3f; mov $_native_manager+4,%ecx; push $0x1234abcd;"
        "3: cmpl $4,_abi_kind; je 4f; cmpl $5,_abi_kind; jne 5f;"
        "4: mov $_native_resource,%ebx; mov $_native_descriptor,%ebp;"
        "5: cmpl $6,_abi_kind; jne 6f; mov _visibility_node,%eax; mov $_native_descriptor,%edx;"
        "push $_native_descriptor+0x48; push $0; push $0; push $0; push $0;"
        "6: cmpl $7,_abi_kind; jne 7f; push _visibility_node;"
        "7: cmpl $8,_abi_kind; jne 10f; mov $_native_resource,%ebx; mov _active_task,%ebp;"
        "push $_native_resource+0x11c; lea 4(%ebp),%eax; push %eax; mov $0x11111111,%eax;"
        "10: cmpl $9,_abi_kind; jne 11f; mov $_native_manager,%esi; mov _active_task,%ebp;"
        "mov $_native_manager+0x25c,%eax; push $0; push $0;"
        "11: cmpl $10,_abi_kind; jne 12f; push $1;"
        "12: cmpl $11,_abi_kind; jne 13f; push $3; push $2; push $1; push _active_task;"
        "13: std; stc; call *_abi_target; pushfl; pop _abi_flags; cmpl $6,_abi_kind; jne 8f; lea 20(%esp),%esp;"
        "8: cmpl $7,_abi_kind; jne 9f; lea 4(%esp),%esp;"
        "9: mov %eax,_regs; mov %ecx,_regs+4; mov %edx,_regs+8;"
        "mov %ebx,_regs+12; mov %ebp,_regs+16; mov %esi,_regs+20; mov %edi,_regs+24;"
        "push _abi_flags; pop _regs+28; mov %esp,_regs+32; popal; popfl; ret");
}
static void __attribute__((force_align_arg_pointer)) abi(unsigned kind) {
    uint8_t before[512] __attribute__((aligned(16)))={0},after[512] __attribute__((aligned(16)))={0};
    uint16_t control=0x077f;uint32_t mxcsr=0x3f80;
    void (*entries[])(void)={enqueue_entry,run_entry,delete_entry,update_entry,pvs_begin_entry,pvs_end_entry,node_publish_entry,node_free_entry,
        retire_copy_entry,retire_enqueue_entry,retire_delete_entry,resource_construct_entry,resource_body_entry};
    abi_kind=kind;abi_target=(void *)(uintptr_t)entries[kind];
    disturb=TRUE;SetLastError(0x7788);
    __asm__ volatile("fninit; fldcw %1; ldmxcsr %2; fldpi; fld1; fldl2t; pcmpeqd %%xmm0,%%xmm0; pcmpeqd %%xmm7,%%xmm7; fxsave %0"
        :"=m"(before):"m"(control),"m"(mxcsr):"memory");
    invoke_abi();__asm__ volatile("fxsave %0; fninit":"=m"(after)::"memory");disturb=FALSE;
    assert(GetLastError()==0x7788 && !memcmp(before,after,288));
    assert(regs[0]==(kind==1?0xabcdee00u:kind==2 || kind==10 || kind==11?(uintptr_t)active_task:kind==3?0x44556677u:
        kind==6?(uintptr_t)visibility_node:kind==9?(uintptr_t)(native_manager+0x25c):0x11111111u));
    assert(regs[1]==(uintptr_t)(kind==3?native_manager+4:active_task));
    assert(regs[2]==(kind==6?(uintptr_t)native_descriptor:0x33333333));
    assert(regs[3]==(kind==4 || kind==5 || kind==8?(uintptr_t)native_resource:0x44444444));
    assert(regs[4]==(kind==4 || kind==5?(uintptr_t)native_descriptor:kind==8 || kind==9?(uintptr_t)active_task:0x55555555));
    assert(regs[5]==(kind==0?(uintptr_t)active_task:kind==9?(uintptr_t)native_manager:0x66666666u) &&
        regs[6]==(kind==0?(uintptr_t)native_resource:0x77777777u));
    assert((regs[7]&0x401)==0x401 && regs[8]==stack_before);
}
static DWORD WINAPI game_handoff(void *unused) {
    (void)unused;witness.native_thread_id=GetCurrentThreadId();
    action=3;run_result=1;update();uint64_t ticket=one();
    assert(game_thread==GetCurrentThreadId() && game_thread!=startup_thread && snapshot[0].destroyed);
    assert(SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,ticket));
    resource_event(RESOURCE_DESTROY_BEGIN,(uintptr_t)native_resource-4,0);
    resource_event(RESOURCE_DESTROY_END,(uintptr_t)native_resource-4,0);
    assert(SudekiMpLanStoryAreaResourceAcknowledge(image,&coordinator,&witness,resources[0].generation));
    assert(SudekiMpLanStoryAreaFinaliseDetach(image,&coordinator,&witness));return 0;
}
static DWORD WINAPI foreign_update(void *unused) {(void)unused;update();return 0;}
static DWORD WINAPI foreign_free(void *node) {invoke_node_free(node);return 0;}
static void *allocate_node(void *address) {
    void *node=VirtualAlloc(address,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(node && (!address || node==address));return node;
}
static void pvs_lifetime(void) {
    start();active_task=allocate_node(NULL);visibility_node=allocate_node(NULL);submit(active_task);
    pvs_mode=1;action=3;run_result=1;free_task=1;update();
    uint64_t first=one();assert(snapshot[0].destroyed && snapshot[0].terminal_result && !snapshot[0].pvs_submitting);
    assert(snapshot[0].pvs_descriptor==(uintptr_t)native_descriptor && snapshot[0].pvs_node==(uintptr_t)visibility_node);
    assert(!snapshot[0].pvs_retired && !snapshot[0].pvs_retiring && !callbacks && !dispatch_depth);
    assert(!SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,first));
    /* A reload can queue the SAME callback object/descriptor. Its different
     * node must never discharge the initial-load record. */
    other_node=allocate_node(NULL);publish(other_node,native_descriptor+0x48,native_descriptor);
    invoke_node_free(other_node);one();assert(!snapshot[0].pvs_retired && !unknown);
    assert(!SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,first));
    invoke_node_free(visibility_node);one();assert(snapshot[0].pvs_retired && !unknown && free_calls==2);
    /* Reuse a freed node address while its OLD receipt remains unconsumed. */
    visibility_node=allocate_node(visibility_node);submit(active_task);
    pvs_mode=2;update();take();assert(observed_count==2);
    uint64_t second=snapshot[1].ticket;assert(second>first && snapshot[1].pvs_node==snapshot[0].pvs_node);
    assert(snapshot[1].pvs_retired && !snapshot[1].pvs_submitting && !unknown && free_calls==3);
    assert(SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,first));
    assert(SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,second));
    /* No-PVS initialization is enclosed by Run, not an invented queue node. */
    submit(active_task);pvs_mode=4;update();uint64_t third=one();
    assert(!snapshot[0].pvs_descriptor && !snapshot[0].pvs_node && snapshot[0].destroyed);
    assert(SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,third));
    take();assert(!observed_count && !callbacks && !unknown);
}
/* Native-connected case: real ZoneFinalise::Run -> stage4 -> PVS submission,
 * real queue allocation/publication/drain -> real descriptor callback. Only
 * dependencies (resource objects, init/notify, allocator and manager) are fake. */
static uint8_t connected_world[0x3a0],connected_zone[0x130],connected_submit[0xd0];
static void *connected_resource_vt[16],*connected_graphics_vt[52],*connected_pvs_vt[32];
static void *connected_graphics,*connected_pvs,*connected_queue_entry __attribute__((used));
static void *connected_callback_vt[1];
static float connected_bounds[6]={-4,-6,-8,4,6,8};
static unsigned connected_init,connected_notify,connected_allocs;
static void *connected_nodes[2];
static void __attribute__((thiscall)) connected_name(void *self,void *out) {
    assert(self==connected_zone+4);memset(out,0,12);
}
static void * __attribute__((thiscall)) connected_get_bounds(void *self) {
    assert(self==&connected_graphics);return connected_bounds;
}
static void * __attribute__((thiscall)) connected_find(void *self,unsigned id) {
    assert(self==&connected_graphics && id==0x1a535650);return &connected_pvs;
}
static void __attribute__((naked,noinline)) connected_queue(void *object __attribute__((unused)),
    void *callback_object __attribute__((unused)),void *argument __attribute__((unused))) {
    __asm__ volatile("push %esi; push %edi; mov 12(%esp),%esi; xor %edi,%edi;"
        "mov 16(%esp),%eax; mov 20(%esp),%edx; push %edx; push %eax; push $0; push $0; push $0;"
        "call *_connected_queue_entry; pop %edi; pop %esi; ret");
}
static void __attribute__((thiscall)) connected_submit_pvs(void *self,const float *position,void *callback_object,void *argument) {
    assert(self==&connected_pvs && position && callback_object==native_descriptor+0x48 && argument==native_descriptor);
    connected_queue(connected_submit,callback_object,argument);
}
static void * __cdecl connected_allocate(size_t size) {
    assert(size==12 && connected_allocs<2);void *p=allocate_node(NULL);
    connected_nodes[connected_allocs++]=p;return p;
}
static void __cdecl connected_unexpected(void) {assert(!"Unexpected native resource dependency");abort();}
static void __attribute__((naked,noinline)) connected_copy_bounds(void) {
    __asm__ volatile("push %esi; push %edi; mov %ecx,%esi; mov %eax,%edi; mov $6,%ecx; cld; rep movsl; pop %edi; pop %esi; ret");
}
static void __attribute__((used,noinline)) connected_init_action(void) {
    ++connected_init;assert(!records[0].pvs_retired && records[0].pvs_node==(uintptr_t)connected_nodes[0]);
}
static void __attribute__((used,noinline)) connected_notify_action(void) {++connected_notify;}
static void __attribute__((naked,noinline)) connected_init_stub(void) {__asm__ volatile(CALL_TEST("connected_init_action") "ret");}
static void __attribute__((naked,noinline)) connected_notify_stub(void) {__asm__ volatile(CALL_TEST("connected_notify_action") "ret $4");}
static void pvs_native(BOOL cancel) {
    start();
    resource_birth((uintptr_t)connected_zone);
    /* Restore real inline continuations after synthetic originals() setup. */
    pvs_begin_original=pvs_begin_hook.trampoline;pvs_end_original=pvs_end_hook.trampoline;
    node_publish_original=node_publish_hook.trampoline;run_original=mapped+RUN;
    resource_for_submit=connected_zone+4;connected_queue_entry=mapped+0x20ba70;
    static const unsigned sites[]={0x109d05,0x109fe0,0x109fe9,0x1097cf,0x1097d6,0x1097e5,0x1097ec,
        0x20ba8e,0x20ba95,0x20bafd};
    static const unsigned targets[]={0x10a020,0x10d620,0x10a070,0x10a070,0x10d620,0x10a070,0x10d620,
        0x1f8880,0x2484fa,0x2097b0};
    void *replacements[]={(void *)(uintptr_t)connected_copy_bounds,(void *)(uintptr_t)connected_notify_stub,
        (void *)(uintptr_t)connected_init_stub,(void *)(uintptr_t)connected_init_stub,
        (void *)(uintptr_t)connected_notify_stub,(void *)(uintptr_t)connected_init_stub,
        (void *)(uintptr_t)connected_notify_stub,(void *)(uintptr_t)connected_unexpected,
        (void *)(uintptr_t)connected_allocate,(void *)(uintptr_t)connected_unexpected};
    SudekiMpRelativeCallHook dependencies[10]={{0}};
    for(unsigned i=0;i<10;++i)
        assert(SudekiMpInstallRelativeCallHook(&dependencies[i],mapped+sites[i],mapped+targets[i],replacements[i]));
    const unsigned ranges[][2]={{RUN,0x20},{0x1097a0,0x865},{0x20ba70,0xa4},{0x20a170,0x89}};
    DWORD previous[4],ignored;
    for(unsigned i=0;i<4;++i) assert(VirtualProtect(mapped+ranges[i][0],ranges[i][1],PAGE_EXECUTE_READWRITE,&previous[i]));
    assert(FlushInstructionCache(GetCurrentProcess(),mapped,SUDEKIMP_EXPECTED_IMAGE_SIZE));
    *(void **)(mapped+0x408d10)=connected_world;*(void **)(mapped+0x408d58)=NULL;*(void **)(mapped+0x409d80)=NULL;
    memset(mapped+0x3ade64,0,15*4);*(unsigned *)(mapped+0x407194)=0;
    *(void **)(connected_world+0x10)=native_manager;*(void **)(connected_world+0x58)=native_descriptor;
    connected_resource_vt[3]=(void *)(uintptr_t)connected_name;connected_resource_vt[11]=mapped+0x109800;
    connected_graphics_vt[0x6c/4]=(void *)(uintptr_t)connected_get_bounds;
    connected_graphics_vt[0xcc/4]=(void *)(uintptr_t)connected_find;
    connected_pvs_vt[0x7c/4]=(void *)(uintptr_t)connected_submit_pvs;
    connected_callback_vt[0]=mapped+0x1097a0;
    connected_graphics=connected_graphics_vt;connected_pvs=connected_pvs_vt;
    *(void **)(connected_zone+4)=connected_resource_vt;*(void **)(connected_zone+0x6c)=&connected_graphics;
    connected_zone[0x128]=4;*(unsigned *)(native_descriptor+0x34)=2;
    *(void **)(native_descriptor+0x48)=connected_callback_vt;
    *(unsigned *)(connected_submit+8)=0x80000000;
    active_task=allocate_node(NULL);((void **)active_task)[4]=connected_zone;submit(active_task);
    action=3;free_task=1;update();uint64_t ticket=one();
    assert(snapshot[0].resource==(uintptr_t)(connected_zone+4) && snapshot[0].destroyed && snapshot[0].terminal_result);
    assert(snapshot[0].pvs_descriptor==(uintptr_t)native_descriptor && snapshot[0].pvs_node==(uintptr_t)connected_nodes[0]);
    assert(!snapshot[0].pvs_retired && !snapshot[0].pvs_submitting && connected_zone[0x128]==5);
    assert(connected_allocs==1 && !connected_init && !connected_notify && !unknown);
    assert(!SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,ticket));
    /* Actual second queue node with the SAME descriptor/callback, submitted
     * outside the finaliser's scope. It is dispatched first by native LIFO. */
    connected_queue(connected_submit,native_descriptor+0x48,native_descriptor);
    owned_node_expected=connected_nodes[0];
    assert(connected_allocs==2 && connected_nodes[0]!=connected_nodes[1]);
    if(cancel) *(unsigned *)(native_descriptor+0x34)=0;
    void (__cdecl * volatile drain_native)(void)=(void (__cdecl *)(void))(uintptr_t)(mapped+0x20a170);
    *(unsigned *)(mapped+0x407194)=1;drain_native();one();
    assert(!free_calls && !snapshot[0].pvs_retired && !connected_init && !connected_notify);
    *(unsigned *)(mapped+0x407194)=0;drain_native();one();
    assert(!unknown && !callbacks && !dispatch_depth && free_calls==2 && snapshot[0].pvs_retired);
    assert(!*(void **)(mapped+0x3c370c));
    assert(connected_init==(cancel?0u:2u) && connected_notify==(cancel?0u:2u));
    assert(SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,ticket));
    take();assert(!observed_count);
    for(unsigned i=10;i>0;--i) assert(SudekiMpRestoreRelativeCallHook(&dependencies[i-1]));
    for(unsigned i=4;i>0;--i) assert(VirtualProtect(mapped+ranges[i-1][0],ranges[i-1][1],previous[i-1],&ignored));
    /* Journal remains installed/pinned by design; no live-uninstall claim.
     * Mapped image remains allocated until this isolated process exits. */
}
static Retirement retirement_snapshot[SUDEKIMP_AREA_FINALISERS];
static unsigned retirement_count;
static void retirement_take(void) {
    assert(SudekiMpLanStoryAreaRetirementSnapshot(image,&coordinator,&witness,retirement_snapshot,
        SUDEKIMP_AREA_FINALISERS,&retirement_count));
}
static void retirement_refused(void) {
    Retirement out,before;memset(&out,0x5a,sizeof(out));before=out;unsigned n=777;
    assert(!SudekiMpLanStoryAreaRetirementSnapshot(image,&coordinator,&witness,&out,1,&n));
    assert(n==777 && !memcmp(&out,&before,sizeof(out)));
}
static void retirement_copy(uintptr_t r) {
    resource_birth((uintptr_t)native_resource);
    resource_event(RESOURCE_DESTROY_BEGIN,(uintptr_t)native_resource,0);
    retirement_event(COPY_BEGIN,r,(uintptr_t)native_resource,(uintptr_t)native_resource+0x11c,r+4,0);
    retirement_refused();
    retirement_event(COPY_END,r,(uintptr_t)native_resource,0,0,0);
    resource_event(RESOURCE_DESTROY_END,(uintptr_t)native_resource,0);
}
static void retirement_submit(uintptr_t r) {
    retirement_event(RETIRE_SUBMIT_BEGIN,r,0,(uintptr_t)native_manager,(uintptr_t)native_manager+0x25c,0);
    retirement_refused();retirement_event(RETIRE_SUBMIT_END,r,0,(uintptr_t)native_manager,0,0);
}
static void retirement_delete(uintptr_t r) {
    retirement_event(RETIRE_DELETE_BEGIN,r,0,1,(uintptr_t)(mapped+0x3e3ef),(uintptr_t)native_manager+0x25c);
    retirement_refused();retirement_event(RETIRE_DELETE_END,r,0,1,0,0);
}
static DWORD WINAPI retirement_foreign(void *unused) {
    (void)unused;retirement_copy(0x11220000);return 0;
}
static void retirement_normal(void) {
    start();uintptr_t r=0x11220000;retirement_copy(r);retirement_take();assert(retirement_count==1);
    uint64_t first=retirement_snapshot[0].ticket;
    assert(!SudekiMpLanStoryAreaRetirementAcknowledge(image,&coordinator,&witness,first));
    retirement_submit(r);retirement_take();assert(retirement_snapshot[0].submitted && !retirement_snapshot[0].drained);
    assert(!SudekiMpLanStoryAreaFinaliseDetach(image,&coordinator,&witness));
    event(UPDATE_BEGIN,(uintptr_t)native_manager,0,0);retirement_delete(r);
    assert(retirements[0].destroyed && !retirements[0].drained);
    assert(!SudekiMpLanStoryAreaRetirementAcknowledge(image,&coordinator,&witness,first));
    event(UPDATE_END,(uintptr_t)native_manager,0,0);retirement_take();assert(retirement_snapshot[0].drained);
    /* Same opaque remover address, new lifecycle, old receipt still retained. */
    retirement_copy(r);retirement_submit(r);retirement_take();assert(retirement_count==2);
    uint64_t second=retirement_snapshot[1].ticket;assert(second>first && !retirement_snapshot[1].drained);
    Retirement before;memset(&before,0x5a,sizeof(before));Retirement out=before;unsigned n=777;
    assert(!SudekiMpLanStoryAreaRetirementSnapshot(image,&coordinator,&witness,&out,1,&n));
    assert(n==777 && !memcmp(&out,&before,sizeof(out)));
    witness.service_post_original_exact=FALSE;retirement_refused();witness.service_post_original_exact=TRUE;
    assert(!SudekiMpLanStoryAreaRetirementAcknowledge(image,&out,&witness,first));
    assert(SudekiMpLanStoryAreaRetirementAcknowledge(image,&coordinator,&witness,first));
    assert(!SudekiMpLanStoryAreaRetirementAcknowledge(image,&coordinator,&witness,second));
    event(UPDATE_BEGIN,(uintptr_t)native_manager,0,0);event(UPDATE_BEGIN,(uintptr_t)native_manager,0,0);
    retirement_delete(r);event(UPDATE_END,(uintptr_t)native_manager,0,0);retirement_refused();
    event(UPDATE_END,(uintptr_t)native_manager,0,0);
    assert(SudekiMpLanStoryAreaRetirementAcknowledge(image,&coordinator,&witness,second));
    assert(!SudekiMpLanStoryAreaRetirementAcknowledge(image,&coordinator,&witness,second));
    retirement_take();assert(!retirement_count && !unknown && !callbacks && !dispatch_depth);
    assert(!SudekiMpLanStoryAreaFinaliseDetach(image,&coordinator,&witness)); /* Resource history unconsumed. */
}
static void retirement_fault(unsigned mode) {
    start();uintptr_t r=0x11220000;retirement_copy(r);
    if(mode==0) retirement_copy(r);
    else if(mode==1) {
        retirement_event(RETIRE_SUBMIT_BEGIN,r,0,(uintptr_t)native_manager,(uintptr_t)native_manager+0x260,0);
        retirement_event(RETIRE_SUBMIT_END,r,0,(uintptr_t)native_manager,0,0);
    } else if(mode==2) {retirement_submit(r);retirement_delete(r);} /* No manager update. */
    else if(mode==3 || mode==4) {
        retirement_submit(r);event(UPDATE_BEGIN,(uintptr_t)native_manager,0,0);
        retirement_event(RETIRE_DELETE_BEGIN,r,0,mode==3?0:1,(uintptr_t)(mapped+(mode==4?0x3e3ed:0x3e3ef)),(uintptr_t)native_manager+0x25c);
        retirement_event(RETIRE_DELETE_END,r,0,1,0,0);event(UPDATE_END,(uintptr_t)native_manager,0,0);
    } else if(mode==5) {
        HANDLE t=CreateThread(NULL,0,retirement_foreign,NULL,0,NULL);assert(t);
        assert(WaitForSingleObject(t,5000)==WAIT_OBJECT_0);CloseHandle(t);
    } else if(mode==6) {
        for(unsigned i=1;i<=SUDEKIMP_AREA_FINALISERS;++i) retirement_copy(r+32*i);
    } else if(mode==7) {
        retirement_event(COPY_BEGIN,r+32,(uintptr_t)native_resource,(uintptr_t)native_resource+0x120,r+36,0);
        retirement_event(COPY_END,r+32,(uintptr_t)native_resource,0,0,0);
    } else assert(!"Unknown retirement fault");
    /* Once unknown is latched, dispatch history deliberately stops advancing;
     * a later return cannot manufacture a clean drain. Callback counts still
     * balance so native passthrough does not leak callback nesting. */
    assert(unknown && !callbacks && dispatch_depth==((mode==3 || mode==4)?1u:0u));retirement_refused();
    assert(!SudekiMpLanStoryAreaRetirementAcknowledge(image,&coordinator,&witness,retirements[0].ticket));
    assert(!SudekiMpLanStoryAreaFinaliseDetach(image,&coordinator,&witness));
}
static void resource_refused(void) {
    Resource out,before;memset(&out,0x5a,sizeof(out));before=out;unsigned n=777;
    assert(!SudekiMpLanStoryAreaResourceSnapshot(image,&coordinator,&witness,&out,1,&n));
    assert(n==777 && !memcmp(&out,&before,sizeof(out)));
}
static void capture_refused(uintptr_t root) {
    uint64_t untouched=987;
    assert(!SudekiMpLanStoryAreaResourceCapture(image,root,&untouched) && untouched==987);
}
static void resource_capture(void) {
    uintptr_t root=0x23450000;uint64_t generation=987;
    assert(!SudekiMpLanStoryAreaResourceJournalReady(image));capture_refused(root);
    start();assert(SudekiMpLanStoryAreaResourceJournalReady(image));
    assert(!SudekiMpLanStoryAreaResourceCapture(NULL,(uintptr_t)native_resource-4,&generation) && generation==987);
    assert(!SudekiMpLanStoryAreaResourceCapture(image,(uintptr_t)native_resource-4,NULL));
    capture_refused(0);capture_refused(root);
    resource_event(RESOURCE_BEGIN,root,0);capture_refused(root);
    resource_event(RESOURCE_END,root,root);
    assert(SudekiMpLanStoryAreaResourceCapture(image,root,&generation));
    uint64_t first=generation;
    resource_event(RESOURCE_DESTROY_BEGIN,root,0);capture_refused(root);
    resource_event(RESOURCE_DESTROY_END,root,0);capture_refused(root);
    resource_birth(root);assert(SudekiMpLanStoryAreaResourceCapture(image,root,&generation) && generation>first);
    assert(SudekiMpLanStoryAreaResourceAcknowledge(image,&coordinator,&witness,first));
    assert(SudekiMpLanStoryAreaResourceCapture(image,root,&first) && first==generation);
    assert(!SudekiMpLanStoryAreaFinaliseUninstall());
    assert(!SudekiMpLanStoryAreaResourceJournalReady(image));
    /* Closing admission is not permission to discard already-admitted work. */
    assert(SudekiMpLanStoryAreaResourceCapture(image,root,&first) && first==generation);
}
static void resource_normal(void) {
    start();uintptr_t root=(uintptr_t)native_resource-4,cleanup=0x88770000;
    uint64_t first=resources[0].generation;active_task=(void *)0x11220000;submit(active_task);
    resource_event(RESOURCE_DESTROY_BEGIN,root,0);resource_refused();
    retirement_event(COPY_BEGIN,cleanup,root,root+0x11c,cleanup+4,0);
    retirement_event(COPY_END,cleanup,root,0,0,0);retirement_submit(cleanup);
    resource_event(RESOURCE_DESTROY_END,root,0);
    uint64_t cleanup_ticket=retirements[0].ticket;assert(retirements[0].resource_generation==first);
    resource_birth(root);uint64_t second=resources[1].generation;assert(second>first);
    submit((void *)0x11220100);take();assert(observed_count==2);
    uint64_t job1=snapshot[0].ticket,job2=snapshot[1].ticket;
    assert(snapshot[0].resource_generation==first && snapshot[1].resource_generation==second);
    Resource seen[2];unsigned n=777;
    assert(!SudekiMpLanStoryAreaResourceSnapshot(image,&coordinator,&witness,seen,1,&n) && n==777);
    assert(SudekiMpLanStoryAreaResourceSnapshot(image,&coordinator,&witness,seen,2,&n) && n==2);
    assert(seen[0].resource==seen[1].resource && seen[0].generation!=seen[1].generation &&
        seen[0].phase==SUDEKIMP_AREA_RESOURCE_BODY_RETURNED && seen[1].phase==SUDEKIMP_AREA_RESOURCE_LIVE);
    assert(!SudekiMpLanStoryAreaResourceAcknowledge(image,&coordinator,&witness,first));
    invoke_delete(active_task,1);assert(SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,job1));
    assert(!SudekiMpLanStoryAreaResourceAcknowledge(image,&coordinator,&witness,first));
    event(UPDATE_BEGIN,(uintptr_t)native_manager,0,0);retirement_delete(cleanup);event(UPDATE_END,(uintptr_t)native_manager,0,0);
    assert(SudekiMpLanStoryAreaRetirementAcknowledge(image,&coordinator,&witness,cleanup_ticket));
    assert(SudekiMpLanStoryAreaResourceAcknowledge(image,&coordinator,&witness,first));
    assert(!SudekiMpLanStoryAreaResourceAcknowledge(image,&coordinator,&witness,second));
    active_task=(void *)0x11220100;invoke_delete(active_task,1);
    assert(SudekiMpLanStoryAreaFinaliseAcknowledge(image,&coordinator,&witness,job2));
    resource_event(RESOURCE_DESTROY_BEGIN,root,0);resource_event(RESOURCE_DESTROY_END,root,0);
    assert(SudekiMpLanStoryAreaResourceAcknowledge(image,&coordinator,&witness,second));
    assert(SudekiMpLanStoryAreaResourceSnapshot(image,&coordinator,&witness,seen,2,&n) && !n);
    assert(!unknown && !callbacks && SudekiMpLanStoryAreaFinaliseDetach(image,&coordinator,&witness));
}
static DWORD WINAPI foreign_resource_return(void *root) {
    resource_event(RESOURCE_END,(uintptr_t)root,(uintptr_t)root);return 0;
}
static void resource_fault(unsigned mode) {
    start();uintptr_t root=0x23450000;
    if(mode==0) resource_birth((uintptr_t)native_resource-4);
    else if(mode==1) {resource_event(RESOURCE_BEGIN,root,0);resource_event(RESOURCE_END,root,root+4);}
    else if(mode==2) {resource_event(RESOURCE_DESTROY_BEGIN,root,0);resource_event(RESOURCE_DESTROY_END,root,0);}
    else if(mode==3) {
        resource_event(RESOURCE_BEGIN,root,0);HANDLE t=CreateThread(NULL,0,foreign_resource_return,(void *)root,0,NULL);assert(t);
        assert(WaitForSingleObject(t,5000)==WAIT_OBJECT_0);CloseHandle(t);
    } else if(mode==4) {for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) resource_birth(root+0x1000*i);}
    else if(mode==5) {resource_for_submit=(void *)(root+4);submit((void *)0x11220000);assert(enqueue_calls==1);}
    else if(mode==6) {
        root=(uintptr_t)native_resource-4;
        retirement_event(COPY_BEGIN,0x88770000,root,root+0x11c,0x88770004,0);
        retirement_event(COPY_END,0x88770000,root,0,0,0);
    } else if(mode==7) {
        root=(uintptr_t)native_resource-4;active_task=(void *)0x11220000;submit(active_task);
        resource_event(RESOURCE_DESTROY_BEGIN,root,0);resource_event(RESOURCE_DESTROY_END,root,0);
        resource_birth(root);action=1;update();assert(run_calls==1 && !records[0].runs);
    } else assert(!"Unknown resource fault");
    assert(unknown && !callbacks);resource_refused();check_refused();retirement_refused();
    assert(!SudekiMpLanStoryAreaResourceJournalReady(image));
    capture_refused((uintptr_t)native_resource-4);
    assert(!SudekiMpLanStoryAreaResourceAcknowledge(image,&coordinator,&witness,resources[0].generation));
}
int main(int argc,char **argv) {
    assert(argc==3);wchar_t path[1024];assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    mapped=map_image(path);image=(HMODULE)mapped;
    if(!strcmp(argv[2],"normal")) {faults();normal();}
    else if(!strcmp(argv[2],"resource")) resource_normal();
    else if(!strcmp(argv[2],"resource-capture")) resource_capture();
    else if(!strncmp(argv[2],"resource-fault-",15)) resource_fault((unsigned)atoi(argv[2]+15));
    else if(!strcmp(argv[2],"retirement")) retirement_normal();
    else if(!strncmp(argv[2],"retirement-fault-",17)) retirement_fault((unsigned)atoi(argv[2]+17));
    else if(!strcmp(argv[2],"retirement-abi")) {
        start();active_task=(void *)0x11220000;
        retire_copy_original=retire_enqueue_original=(void *)(uintptr_t)abi_ret8;
        retire_delete_original=(void *)(uintptr_t)abi_delete;
        resource_birth((uintptr_t)native_resource);resource_event(RESOURCE_DESTROY_BEGIN,(uintptr_t)native_resource,0);
        abi(8);abi(9);resource_event(RESOURCE_DESTROY_END,(uintptr_t)native_resource,0);
        retirement_take();assert(retirement_count==1 && retirement_snapshot[0].submitted && !unknown);
        /* The standalone caller is deliberately not native manager dispatch;
         * its deletion must quarantine while still preserving the entire ABI. */
        abi(10);assert(unknown && !callbacks && !retirements[0].destroyed);retirement_refused();
    }
    else if(!strcmp(argv[2],"resource-abi")) {
        start();active_task=(void *)0x11220000;
        resource_construct_original=(void *)(uintptr_t)abi_construct;resource_body_original=(void *)(uintptr_t)abi_passthrough;
        abi(11);Resource *r=resource_phase((uintptr_t)active_task,SUDEKIMP_AREA_RESOURCE_LIVE);assert(r && !unknown);
        uint64_t generation=r->generation;
        assert(!SudekiMpLanStoryAreaResourceAcknowledge(image,&coordinator,&witness,generation));
        abi(12);assert(!unknown && !callbacks && r->phase==SUDEKIMP_AREA_RESOURCE_BODY_RETURNED);
        assert(SudekiMpLanStoryAreaResourceAcknowledge(image,&coordinator,&witness,generation));
        assert(!SudekiMpLanStoryAreaResourceAcknowledge(image,&coordinator,&witness,generation));
    }
    else if(!strcmp(argv[2],"abi")) {
        start();active_task=(void *)0x12345000;enqueue_original=(void *)abi_enqueue;
        delete_original=(void *)abi_delete;update_original=(void *)abi_update;
        run_original=(void *)(uintptr_t)abi_run;
        abi(0);abi(3);event(UPDATE_BEGIN,(uintptr_t)native_manager,0,0);abi(1);abi(2);event(UPDATE_END,(uintptr_t)native_manager,0,0);
        one();assert(snapshot[0].destroyed && snapshot[0].runs==1 && !unknown && !callbacks);
    } else if(!strcmp(argv[2],"handoff")) {
        start();active_task=(void *)0x12345000;submit(active_task);
        HANDLE worker=CreateThread(NULL,0,game_handoff,NULL,0,NULL);assert(worker);
        assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);CloseHandle(worker);
        assert(!unknown && !callbacks && !dispatch_depth);
    } else if(!strcmp(argv[2],"wrong-thread")) {
        start();update();assert(!unknown);
        HANDLE worker=CreateThread(NULL,0,foreign_update,NULL,0,NULL);assert(worker);
        assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);CloseHandle(worker);
        assert(unknown && update_calls==2 && !callbacks);check_refused();
    } else if(!strcmp(argv[2],"overflow")) {
        start();for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) submit((void *)(uintptr_t)(0x1000+16*i));
        take();assert(observed_count==SUDEKIMP_AREA_FINALISERS);submit((void *)0x98760000);
        assert(unknown && enqueue_calls==SUDEKIMP_AREA_FINALISERS+1);check_refused();
    } else if(!strcmp(argv[2],"unknown")) {
        start();submit((void *)0x12345000);submit((void *)0x12345000);
        assert(unknown && enqueue_calls==2);check_refused();
    } else if(!strcmp(argv[2],"pvs")) pvs_lifetime();
    else if(!strcmp(argv[2],"pvs-abi")) {
        start();active_task=(void *)0x12345000;visibility_node=(void *)0x98765000;
        enqueue_original=(void *)abi_enqueue;delete_original=(void *)abi_delete;update_original=(void *)abi_update;
        node_free_original=(void *)(uintptr_t)abi_passthrough;
        abi(0);abi(3);event(UPDATE_BEGIN,(uintptr_t)native_manager,0,0);
        event(RUN_BEGIN,(uintptr_t)active_task,0,0);abi(4);abi(6);abi(5);
        event(RUN_END,(uintptr_t)active_task,0,0);abi(2);event(UPDATE_END,(uintptr_t)native_manager,0,0);abi(7);
        one();assert(snapshot[0].pvs_retired && snapshot[0].destroyed && !unknown && !callbacks);
    } else if(!strcmp(argv[2],"pvs-unknown")) {
        start();active_task=(void *)0x12345000;visibility_node=allocate_node(NULL);other_node=allocate_node(NULL);
        submit(active_task);pvs_mode=3;action=3;run_result=1;update();
        assert(unknown && !callbacks && run_calls==1 && delete_calls==1);check_refused();
        invoke_node_free(visibility_node);invoke_node_free(other_node);assert(free_calls==2 && !callbacks);
    } else if(!strcmp(argv[2],"pvs-wrong-thread")) {
        start();active_task=(void *)0x12345000;visibility_node=allocate_node(NULL);
        submit(active_task);pvs_mode=1;action=3;run_result=1;update();one();assert(!unknown);
        HANDLE worker=CreateThread(NULL,0,foreign_free,visibility_node,0,NULL);assert(worker);
        assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);CloseHandle(worker);
        assert(unknown && !callbacks && free_calls==1);check_refused();
    } else if(!strcmp(argv[2],"pvs-native")) pvs_native(FALSE);
    else if(!strcmp(argv[2],"pvs-native-cancel")) pvs_native(TRUE);
    else if(!strcmp(argv[2],"pvs-missing")) {
        start();active_task=(void *)0x12345000;submit(active_task);pvs_mode=5;action=3;run_result=1;update();
        assert(unknown && records[0].pvs_submitting && !records[0].pvs_node && !callbacks);check_refused();
    }
    else assert(!"Unknown test mode");
    assert(base && !SudekiMpLanStoryAreaFinaliseUninstall() && !SudekiMpLanStoryAreaFinaliseInstall(image));
    printf("StoryAreaFinaliseImageTest: PASS (%s; %s; live teardown and area readiness unproven)\n",argv[2],
        !strncmp(argv[2],"pvs-native",10)?"native-connected finalise/queue with synthetic resources":"exact seams, synthetic lifecycle/ABI");
    return 0;
}
