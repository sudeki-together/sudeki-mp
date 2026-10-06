/* Retail image is hash checked, relocated and initially NON-executable.
 * Bridge/ABI tests use synthetic continuations. The final bounded test runs
 * native non-active/last-active cleanup and named load/unload control flow with synthetic
 * resources, lookup and state-change calls. No resource destruction/loading or gameplay runs. */
#include "hooks/call_hook.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned installs,restores,fail_install,fail_restore;
static BOOL call_install(SudekiMpRelativeCallHook *,uint8_t *,const void *,const void *);
static BOOL call_restore(SudekiMpRelativeCallHook *);
static BOOL inline_install(SudekiMpInlineHook *,uint8_t *,const uint8_t *,size_t,const void *);
static BOOL inline_restore(SudekiMpInlineHook *);
static DWORD test_thread(void);
#define SudekiMpInstallRelativeCallHook call_install
#define SudekiMpRestoreRelativeCallHook call_restore
#define SudekiMpInstallInlineHook inline_install
#define SudekiMpRestoreInlineHook inline_restore
#define GetCurrentThreadId test_thread
#include "../src/hooks/lan_story_area_eviction.c"
#undef GetCurrentThreadId
#undef SudekiMpInstallRelativeCallHook
#undef SudekiMpRestoreRelativeCallHook
#undef SudekiMpInstallInlineHook
#undef SudekiMpRestoreInlineHook
static BOOL call_install(SudekiMpRelativeCallHook *h,uint8_t *p,const void *e,const void *r) {
    if(++installs==fail_install) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpInstallRelativeCallHook(h,p,e,r);
}
static BOOL call_restore(SudekiMpRelativeCallHook *h) {
    if(++restores==fail_restore) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpRestoreRelativeCallHook(h);
}
static BOOL inline_install(SudekiMpInlineHook *h,uint8_t *p,const uint8_t *e,size_t n,const void *r) {
    if(++installs==fail_install) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpInstallInlineHook(h,p,e,n,r);
}
static BOOL inline_restore(SudekiMpInlineHook *h) {
    if(++restores==fail_restore) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpRestoreInlineHook(h);
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
    assert(f!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(f,NULL),got=0;
    uint8_t *raw=malloc(size);assert(raw && ReadFile(f,raw,size,&got,NULL) && got==size);CloseHandle(f);
    IMAGE_DOS_HEADER *dos=(void *)raw;IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(b);memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
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
        uint16_t *items=(void *)(block+1);unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfff);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4);*(uint32_t *)(b+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw);return b;
}
static uint8_t *mapped,world[0x3a0],descriptors[3*0x54];
static SudekiMpStoryAreas policy;
static SudekiMpStoryAreaRef outside,inside;
static SudekiMpControlUpdateDispatchWitness witness;
static uint64_t ticket;
static unsigned original_calls __attribute__((used)),original_state __attribute__((used));
static void *original_descriptor __attribute__((used)),*bridge_descriptor __attribute__((used));
static unsigned bridge_state __attribute__((used));
static uint32_t regs[9] __attribute__((used)),stack_before __attribute__((used));
static unsigned nonactive_passes __attribute__((used)),nonactive_skips __attribute__((used));
static void __attribute__((naked,noinline)) fake_nonactive_pass(void) {
    __asm__ volatile("pushfl; incl _nonactive_passes; popfl; mov $3,%ecx; ret");
}
static void __attribute__((naked,noinline)) fake_nonactive_skip(void) {
    __asm__ volatile("pushfl; incl _nonactive_skips; popfl; ret");
}
static void __attribute__((naked,noinline)) invoke_nonactive(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,_stack_before;"
        "mov $0x11111111,%eax; mov $0x22222222,%ecx; mov $0x33333333,%edx;"
        "mov _bridge_descriptor,%ebx; mov $0x55555555,%ebp; mov $0x66666666,%esi; mov $0x77777777,%edi;"
        "std; stc; call _nonactive_entry; mov %eax,_regs; mov %ecx,_regs+4; mov %edx,_regs+8;"
        "mov %ebx,_regs+12; mov %ebp,_regs+16; mov %esi,_regs+20; mov %edi,_regs+24;"
        "pushfl; pop _regs+28; mov %esp,_regs+32; popal; popfl; ret");
}
static void __attribute__((naked,noinline)) fake_original(void) {
    __asm__ volatile("incl _original_calls; mov %eax,_original_state;"
        "mov 4(%esp),%eax; mov %eax,_original_descriptor; mov $0xabcdef12,%eax; stc; ret $4");
}
static void __attribute__((naked,noinline)) invoke(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,_stack_before; push _bridge_descriptor;"
        "mov _bridge_state,%eax; mov $0x22222222,%ecx; mov $0x33333333,%edx;"
        "mov $0x44444444,%ebx; mov $0x55555555,%ebp; mov $0x66666666,%esi; mov $0x77777777,%edi;"
        "std; stc; call _entry; mov %eax,_regs; mov %ecx,_regs+4; mov %edx,_regs+8;"
        "mov %ebx,_regs+12; mov %ebp,_regs+16; mov %esi,_regs+20; mov %edi,_regs+24;"
        "pushfl; pop _regs+28; mov %esp,_regs+32; popal; popfl; ret");
}
static void __attribute__((force_align_arg_pointer,noinline)) abi(void *p,BOOL blocked) {
    uint8_t before[512] __attribute__((aligned(16)))={0},after[512] __attribute__((aligned(16)))={0};
    uint16_t control=0x077f;uint32_t mxcsr=0x3f80;unsigned calls=original_calls;
    bridge_descriptor=p;bridge_state=0;disturb=TRUE;SetLastError(0x7788);
    __asm__ volatile("fninit; fldcw %1; ldmxcsr %2; fldpi; fld1; fldl2t;"
        "pcmpeqd %%xmm0,%%xmm0; pcmpeqd %%xmm7,%%xmm7; fxsave %0"
        : "=m"(before) : "m"(control),"m"(mxcsr) : "memory");
    invoke();
    __asm__ volatile("fxsave %0; fninit" : "=m"(after) : : "memory");
    disturb=FALSE;
    assert(GetLastError()==0x7788 && !memcmp(before,after,160) && !memcmp(before+160,after+160,128));
    assert(regs[0]==(blocked?0u:0xabcdef12u));
    for(unsigned i=1;i<7;++i) assert(regs[i]==0x11111111u*(i+1));
    assert((regs[7]&0x401)==0x401 && regs[8]==stack_before);
    assert(original_calls==calls+(blocked?0u:1u));
    if(!blocked) assert(original_state==0 && original_descriptor==p);
}
static void fixture(void) {
    installs=restores=fail_install=fail_restore=0;witness_ok=TRUE;
    assert(!base && SudekiMpLanStoryAreaEvictionInstall((HMODULE)mapped));
    memset(world,0,sizeof(world));memset(descriptors,0,sizeof(descriptors));memset(&policy,0,sizeof(policy));
    *(void **)world=mapped+WORLD_VT;*(void **)(world+0x50)=descriptors;*(unsigned *)(world+0x54)=3;
    *(void **)(mapped+WORLD)=world;
    for(unsigned i=0;i<3;++i) *(void **)(descriptors+i*0x54)=mapped+DESC_VT;
    *(const char **)(descriptors+0x24)="brightwater";
    *(const char **)(descriptors+0x54+0x24)="church";
    *(const char **)(descriptors+0xa8+0x24)="neighbor";
    *(unsigned *)(descriptors+0x34)=3;
    assert(SudekiMpStoryAreasInitialize(&policy,10));
    assert(SudekiMpStoryAreaLoad(&policy,"brightwater","",&outside));
    assert(SudekiMpStoryAreaReady(&policy,outside));
    assert(SudekiMpStoryAreaLoad(&policy,"brightwater","church",&inside));
    witness=(SudekiMpControlUpdateDispatchWitness){.dispatch_serial=9,.service_post_original_exact=1,
        .native_thread_id=GetCurrentThreadId()};
    original=(void *)(uintptr_t)fake_original;original_calls=0;
    nonactive_original=(void *)(uintptr_t)fake_nonactive_pass;
    nonactive_next=(void *)(uintptr_t)fake_nonactive_skip;
}
static void __attribute__((force_align_arg_pointer,noinline)) bulk_abi(void *p,BOOL blocked) {
    uint8_t before[512] __attribute__((aligned(16)))={0},after[512] __attribute__((aligned(16)))={0};
    uint16_t control=0x077f;uint32_t mxcsr=0x3f80;
    unsigned passes=nonactive_passes,skips=nonactive_skips;
    bridge_descriptor=p;disturb=TRUE;SetLastError(0x7788);
    __asm__ volatile("fninit; fldcw %1; ldmxcsr %2; fldpi; fld1; fldl2t;"
        "pcmpeqd %%xmm0,%%xmm0; pcmpeqd %%xmm7,%%xmm7; fxsave %0"
        : "=m"(before) : "m"(control),"m"(mxcsr) : "memory");
    invoke_nonactive();
    __asm__ volatile("fxsave %0; fninit" : "=m"(after) : : "memory");
    disturb=FALSE;
    assert(GetLastError()==0x7788 && !memcmp(before,after,160) && !memcmp(before+160,after+160,128));
    for(unsigned i=0;i<7;++i)
        assert(regs[i]==(i==1?3u:i==3?(uint32_t)(uintptr_t)p:0x11111111u*(i+1)));
    assert((regs[7]&0x401)==0x401 && regs[8]==stack_before);
    assert(nonactive_passes==passes+!blocked && nonactive_skips==skips+!!blocked);
}
static void reserve(void) {
    assert(SudekiMpLanStoryAreaEvictionReserve((HMODULE)mapped,&witness,&policy,inside,descriptors+0x54,&ticket));
    assert(!SudekiMpStoryAreaRetireBegin(&policy,inside));
}
static void release(void) {
    assert(SudekiMpLanStoryAreaEvictionRelease((HMODULE)mapped,&witness,ticket));
}
static void finish(void) {
    assert(SudekiMpLanStoryAreaEvictionUninstall(&witness));
    assert(!base && !original && !nonactive_original && !nonactive_next && !callbacks);
    *(void **)(mapped+WORLD)=NULL;
    assert(image_exact((HMODULE)mapped));
}
static void normal(void) {
    fixture();abi(descriptors,FALSE);bulk_abi(descriptors,FALSE);reserve();
    uint8_t world_before[sizeof(world)],table_before[sizeof(descriptors)];
    memcpy(world_before,world,sizeof(world));memcpy(table_before,descriptors,sizeof(descriptors));
    abi(descriptors+0x54,TRUE);abi(descriptors,FALSE);abi(descriptors+0xa8,FALSE);
    bulk_abi(descriptors+0x54,TRUE);bulk_abi(descriptors,FALSE);bulk_abi(descriptors+0xa8,FALSE);
    assert(!memcmp(world_before,world,sizeof(world)) && !memcmp(table_before,descriptors,sizeof(descriptors)));
    *(unsigned *)(descriptors+0x54+0x34)=1;abi(descriptors+0x54,TRUE);
    *(unsigned *)(descriptors+0x54+0x34)=2;abi(descriptors+0x54,TRUE);
    uint64_t outer=0;
    assert(SudekiMpLanStoryAreaEvictionReserve((HMODULE)mapped,&witness,&policy,outside,descriptors,&outer));
    abi(descriptors,TRUE);abi(descriptors+0xa8,FALSE);
    bulk_abi(descriptors,TRUE);bulk_abi(descriptors+0xa8,FALSE);
    assert(call_exact(mapped,0x5b1f,CHANGE_STATE) && call_exact(mapped,0x5b6c,CHANGE_STATE));
    assert(SudekiMpLanStoryAreaEvictionRelease((HMODULE)mapped,&witness,outer));
    abi(descriptors,FALSE);bulk_abi(descriptors,FALSE);release();
    abi(descriptors+0x54,FALSE);bulk_abi(descriptors+0x54,FALSE);
    uint64_t old=ticket;
    assert(!SudekiMpLanStoryAreaEvictionRelease((HMODULE)mapped,&witness,old));
    reserve();assert(ticket>old);release();finish();
    fixture();reserve();assert(ticket>old);
    assert(!SudekiMpLanStoryAreaEvictionUninstall(&witness) && stopping && base);
    abi(descriptors+0x54,TRUE);bulk_abi(descriptors+0x54,TRUE);release();finish();
}
static void refused(void) {
    uint64_t out=887;
    assert(!SudekiMpLanStoryAreaEvictionReserve((HMODULE)mapped,&witness,&policy,inside,descriptors+0x54,&out));
    assert(out==887);
}
static void malformed(void) {
    fixture();witness_ok=FALSE;refused();witness_ok=TRUE;
    witness.service_post_original_exact=0;refused();witness.service_post_original_exact=1;
    witness.dispatch_serial=0;refused();witness.dispatch_serial=9;
    *(void **)world=NULL;refused();*(void **)world=mapped+WORLD_VT;
    *(void **)(descriptors+0x54)=NULL;refused();*(void **)(descriptors+0x54)=mapped+DESC_VT;
    *(unsigned *)(world+0x54)=4097;refused();*(unsigned *)(world+0x54)=3;
    *(const char **)(descriptors+0x24)="church";refused();*(const char **)(descriptors+0x24)="brightwater";
    *(const char **)(descriptors+0xa8+0x24)=NULL;refused();*(const char **)(descriptors+0xa8+0x24)="neighbor";
    *(unsigned *)(descriptors+0x54+0x34)=5;refused();*(unsigned *)(descriptors+0x54+0x34)=0;
    memset(policy.areas[inside.slot].temporary,'a',SUDEKIMP_STORY_AREA_NAME);refused();
    memset(policy.areas[inside.slot].temporary,0,SUDEKIMP_STORY_AREA_NAME);
    memcpy(policy.areas[inside.slot].temporary,"church",7);
    uint64_t saved=serial;serial=UINT64_MAX;refused();serial=saved;
    uint64_t pins[SUDEKIMP_STORY_AREA_PINS];
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_PINS;++i)
        assert(SudekiMpStoryAreaRetain(&policy,inside,&pins[i]));
    refused();
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_PINS;++i)
        assert(SudekiMpStoryAreaRelease(&policy,inside,pins[i]));
    reserve();refused();witness_ok=FALSE;
    assert(!SudekiMpLanStoryAreaEvictionRelease((HMODULE)mapped,&witness,ticket));
    witness_ok=TRUE;release();finish();
}
static void fault_cleanup(void) {
    assert(unknown && !callbacks);
    assert(!SudekiMpLanStoryAreaEvictionRelease((HMODULE)mapped,&witness,ticket));
    assert(!SudekiMpLanStoryAreaEvictionUninstall(&witness) && base && original);
    /* Fixture only, after all synthetic calls returned. No production reset. */
    for(unsigned i=0;i<LIMIT;++i) if(reservations[i].ticket) {
        assert(SudekiMpStoryAreaRelease(reservations[i].policy,reservations[i].area,reservations[i].pin));
        memset(&reservations[i],0,sizeof(reservations[i]));
    }
    unknown=FALSE;finish();
}
static DWORD WINAPI foreign(void *unused) {
    (void)unused;assert(hold(descriptors+0xa8,0));return 0;
}
static void faults(void) {
    fixture();reserve();*(unsigned *)(world+0x54)=2;
    abi(descriptors+0xa8,TRUE);bulk_abi(descriptors+0xa8,TRUE);
    *(unsigned *)(world+0x54)=3;fault_cleanup();
    fixture();reserve();*(const char **)(descriptors+0x54+0x24)="replacement";
    abi(descriptors+0x54,TRUE);fault_cleanup();
    fixture();reserve();assert(hold(descriptors+0x54,1));fault_cleanup();
    fixture();reserve();uint8_t saved=mapped[NONACTIVE];mapped[NONACTIVE]^=1;
    bulk_abi(descriptors+0xa8,TRUE);mapped[NONACTIVE]=saved;fault_cleanup();
    fixture();reserve();saved=mapped[NAMED];mapped[NAMED]^=1;
    abi(descriptors+0xa8,TRUE);mapped[NAMED]=saved;fault_cleanup();
    const unsigned last_sites[]={LAST_NEIGHBOR,LAST_ROOT};
    for(unsigned i=0;i<2;++i) {
        fixture();reserve();saved=mapped[last_sites[i]];mapped[last_sites[i]]^=1;
        abi(descriptors+0xa8,TRUE);mapped[last_sites[i]]=saved;fault_cleanup();
    }
    fixture();reserve();HANDLE h=CreateThread(NULL,0,foreign,NULL,0,NULL);assert(h);
    assert(WaitForSingleObject(h,5000)==WAIT_OBJECT_0);CloseHandle(h);fault_cleanup();
    fixture();reserve();*(void **)(mapped+WORLD)=NULL;
    abi(descriptors+0xa8,TRUE);*(void **)(mapped+WORLD)=world;fault_cleanup();
}
static void restoration(void) {
    for(unsigned i=1;i<=6;++i) {
        installs=restores=0;fail_install=i;fail_restore=0;
        assert(!SudekiMpLanStoryAreaEvictionInstall((HMODULE)mapped) && !base);
    }
    for(unsigned stage=2;stage<=6;++stage) for(unsigned failure=1;failure<=6;++failure) {
        if(7-failure>=stage) continue; /* This reverse target was not installed. */
        installs=restores=0;fail_install=stage;fail_restore=failure;
        assert(!SudekiMpLanStoryAreaEvictionInstall((HMODULE)mapped) && base && original && stopping);
        assert(restores==6 && !SudekiMpLanStoryAreaEvictionInstall((HMODULE)mapped));
        fail_install=fail_restore=0;assert(SudekiMpLanStoryAreaEvictionUninstall(NULL));
    }
    for(unsigned i=1;i<=6;++i) {
        fixture();reserve();release();restores=0;fail_restore=i;
        assert(!SudekiMpLanStoryAreaEvictionUninstall(&witness) && base && original && nonactive_original && restores==6);
        fail_restore=0;finish();
    }
    static const unsigned sites[]={FIRST,NEIGHBOR,NAMED,NONACTIVE,LAST_NEIGHBOR,LAST_ROOT};
    for(unsigned i=0;i<6;++i) {
        fixture();reserve();release();unsigned at=sites[i];
        restores=0;uint8_t saved=mapped[at];mapped[at]=0x90;
        assert(!SudekiMpLanStoryAreaEvictionUninstall(&witness) && base && restores==6 && mapped[at]==0x90);
        mapped[at]=saved;finish();
    }
}
static void image_refusals(void) {
    static const unsigned gates[]={FIRST-3,FIRST,FIRST+5,NEIGHBOR-3,NEIGHBOR,NEIGHBOR+5,
        CHANGE_STATE,CHANGE_STATE+21,0x10b2bf,NONACTIVE-7,NONACTIVE,NONACTIVE+5,
        NONACTIVE+9,NONACTIVE_NEXT,NONACTIVE_NEXT+12,0x636a,0x637d,
        0x7bb0,0x7bb6,0x7bba,0x7bbc,0x7bc1,0x7bc4,NAMED,NAMED+5,
        0x7bd0,0x7bd8,0x7bdc,0x7be2,0x7bf0,0x7c06,0x7c0e,LAST_NEIGHBOR-3,
        LAST_NEIGHBOR,LAST_NEIGHBOR+5,LAST_ROOT-3,LAST_ROOT,LAST_ROOT+5};
    for(unsigned i=0;i<sizeof(gates)/sizeof(gates[0]);++i) {
        mapped[gates[i]]^=1;assert(!SudekiMpLanStoryAreaEvictionInstall((HMODULE)mapped) && !base);
        mapped[gates[i]]^=1;
    }
    *(void **)(mapped+WORLD)=world;
    assert(!SudekiMpLanStoryAreaEvictionInstall((HMODULE)mapped) && !base);
    *(void **)(mapped+WORLD)=NULL;
}
static HANDLE handoff_ready,handoff_continue;
static DWORD WINAPI native_handoff(void *unused) {
    (void)unused;
    witness.native_thread_id=GetCurrentThreadId();
    /* The first native event, even without reservations, must bind this
     * thread rather than the suspended-startup installation thread. */
    abi(descriptors,FALSE);
    bulk_abi(descriptors,FALSE);
    assert(native_thread==GetCurrentThreadId() && native_thread!=startup_thread);
    reserve();abi(descriptors+0x54,TRUE);bulk_abi(descriptors+0x54,TRUE);
    assert(SetEvent(handoff_ready));
    assert(WaitForSingleObject(handoff_continue,5000)==WAIT_OBJECT_0);
    assert(stopping); /* Foreign teardown closed admission, but did not clear ownership. */
    abi(descriptors+0x54,TRUE);bulk_abi(descriptors+0x54,TRUE);release();finish();return 0;
}
static void handoff(void) {
    fixture();assert(startup_thread==GetCurrentThreadId() && !native_thread);
    handoff_ready=CreateEventA(NULL,TRUE,FALSE,NULL);
    handoff_continue=CreateEventA(NULL,TRUE,FALSE,NULL);
    assert(handoff_ready && handoff_continue);
    DWORD id=0;HANDLE h=CreateThread(NULL,0,native_handoff,NULL,0,&id);assert(h);
    assert(WaitForSingleObject(handoff_ready,5000)==WAIT_OBJECT_0);
    assert(native_thread==id && !unknown);
    SudekiMpControlUpdateDispatchWitness local=witness;
    local.native_thread_id=GetCurrentThreadId();
    uint64_t rejected=887;
    assert(!SudekiMpLanStoryAreaEvictionReserve((HMODULE)mapped,&local,&policy,outside,descriptors,&rejected));
    assert(rejected==887 && !SudekiMpLanStoryAreaEvictionRelease((HMODULE)mapped,&local,ticket));
    assert(!SudekiMpLanStoryAreaEvictionUninstall(&local) && base && original && !unknown);
    assert(SetEvent(handoff_continue));
    assert(WaitForSingleObject(h,5000)==WAIT_OBJECT_0);
    CloseHandle(h);CloseHandle(handoff_ready);CloseHandle(handoff_continue);
    assert(!base && !original && !native_thread && image_exact((HMODULE)mapped));
}
static unsigned native_logs __attribute__((used)),native_resets __attribute__((used));
static unsigned native_audio __attribute__((used)),native_cache __attribute__((used));
static unsigned native_requests __attribute__((used)),native_queries __attribute__((used));
static void *native_descriptor __attribute__((used));
static void __attribute__((naked,noinline)) native_log_stub(void) {
    __asm__ volatile("incl _native_logs; ret $8");
}
static void __attribute__((naked,noinline)) native_reset_stub(void) {
    __asm__ volatile("incl _native_resets; mov %eax,_native_descriptor; ret");
}
static void __attribute__((naked,noinline)) native_audio_stub(void) {
    __asm__ volatile("incl _native_audio; ret $4");
}
static void __attribute__((naked,noinline)) native_cache_stub(void) {
    __asm__ volatile("incl _native_cache; ret");
}
static void __attribute__((naked,noinline)) native_request_stub(void) {
    __asm__ volatile("incl _native_requests; xor %eax,%eax; ret");
}
static void __attribute__((naked,noinline)) native_query_stub(void) {
    __asm__ volatile("incl _native_queries; xor %eax,%eax; ret");
}
static void native_bulk_control_flow(void) {
    /* Execute only the native loop with empty/synthetic resident resources.
     * Every outbound call reachable in this fixture is stubbed. Other paths
     * (native object destruction, pending jobs, renderer, scripts) stay unrun.
     * This verifies real loop continuation, state3 exemption and tail writes,
     * not successful resource cleanup or whole-area retention. */
    static const unsigned sites[]={0x620d,0x6217,0x6224,0x622b,0x62f9};
    static const unsigned targets[]={0x3f3b0,0x10aca0,0x10d010,0x10d7e0,0x1bb600};
    void *stubs[]={(void *)native_log_stub,(void *)native_reset_stub,(void *)native_audio_stub,
        (void *)native_cache_stub,(void *)native_request_stub};
    SudekiMpRelativeCallHook native_calls[5]={{0}};
    SudekiMpPointerHook descriptor_query={0};
    uint8_t before_code[0x1ee];memcpy(before_code,mapped+0x6190,sizeof(before_code));
    for(unsigned i=0;i<5;++i)
        assert(SudekiMpInstallRelativeCallHook(&native_calls[i],mapped+sites[i],mapped+targets[i],stubs[i]));
    assert(SudekiMpInstallPointerHook(&descriptor_query,(void **)(mapped+DESC_VT),
        *(void **)(mapped+DESC_VT),(void *)native_query_stub));
    DWORD prior;
    assert(VirtualProtect(mapped+0x6190,sizeof(before_code),PAGE_EXECUTE_READWRITE,&prior));
    assert(FlushInstructionCache(GetCurrentProcess(),mapped+0x6190,sizeof(before_code)));
    uint32_t resident_fixture=0,pending_fixture=0;
    for(unsigned held_state=0;held_state<=4;++held_state)
    for(unsigned unheld_state=0;unheld_state<=4;++unheld_state) {
        fixture();
        nonactive_original=nonactive_hook.trampoline;nonactive_next=mapped+NONACTIVE_NEXT;
        *(unsigned *)(descriptors+0x54+0x34)=held_state;
        *(unsigned *)(descriptors+0xa8+0x34)=unheld_state;
        for(unsigned i=0;i<3;++i) {
            *(void **)(descriptors+i*0x54+0x14)=&resident_fixture;
            *(void **)(descriptors+i*0x54+0x18)=&pending_fixture;
        }
        *(void **)(world+0xc)=descriptors;*(void **)(world+0x10)=descriptors+0x54;
        *(void **)(world+0x14)=descriptors+0x54;*(void **)(world+0x18)=descriptors;
        reserve();
        uint8_t expected_world[sizeof(world)],expected_table[sizeof(descriptors)];
        memcpy(expected_world,world,sizeof(world));memcpy(expected_table,descriptors,sizeof(descriptors));
        BOOL removed=unheld_state!=0 && unheld_state!=3;
        if(removed) {
            *(unsigned *)(expected_table+0xa8+0x34)=0;
            *(void **)(expected_table+0xa8+0x10)=NULL;
            *(void **)(expected_table+0xa8+0x14)=NULL;
            *(void **)(expected_table+0xa8+0x18)=&resident_fixture;
        }
        /* These native global writes intentionally remain unfiltered. A
         * reservation is not permission to leave a pending-world context
         * unowned. Assert the limitation rather than hiding it in the test. */
        *(void **)(expected_world+0x14)=NULL;*(void **)(expected_world+0x18)=NULL;
        expected_world[0x39d]=1;
        native_logs=native_resets=native_audio=native_cache=native_requests=native_queries=0;
        native_descriptor=NULL;
        ((void (__stdcall *)(void *))(uintptr_t)(mapped+0x6190))(world);
        assert(!memcmp(expected_world,world,sizeof(world)));
        assert(!memcmp(expected_table,descriptors,sizeof(descriptors)));
        assert(native_logs==!!removed && native_resets==!!removed && native_audio==!!removed);
        assert(native_cache==!!removed && native_queries==!!removed && native_requests==(unheld_state==1));
        assert(native_descriptor==(removed?(void *)(descriptors+0xa8):NULL));
        release();finish();
    }
    assert(SudekiMpRestorePointerHook(&descriptor_query));
    for(unsigned i=5;i>0;--i) assert(SudekiMpRestoreRelativeCallHook(&native_calls[i-1]));
    assert(!memcmp(before_code,mapped+0x6190,sizeof(before_code)));
    DWORD ignored;assert(VirtualProtect(mapped+0x6190,sizeof(before_code),prior,&ignored));
}
static void *named_lookup_result;
static const char *named_lookup_text;
static unsigned named_lookups;
static void * __stdcall named_lookup_stub(void *owner,const char *name) {
    assert(owner==world && name==named_lookup_text);++named_lookups;
    return named_lookup_result;
}
static void invoke_named(const char *name,void *result,unsigned calls) {
    named_lookup_text=name;named_lookup_result=result;named_lookups=0;
    unsigned before=original_calls;
    ((void (__cdecl *)(const char *))(uintptr_t)(mapped+0x7bb0))(name);
    assert(named_lookups==1 && original_calls==before+calls);
    if(calls) assert(original_state==0 && original_descriptor==result);
}
static void native_named_control_flow(void) {
    /* Actual exported wrapper and installed reservation bridge. Only its
     * lookup and eventual state change are synthetic. This does not test
     * native string resolution or execute actual resource destruction. */
    uint8_t before[0x1e];memcpy(before,mapped+0x7bb0,sizeof(before));
    DWORD prior;assert(VirtualProtect(mapped+0x7bb0,sizeof(before),PAGE_EXECUTE_READWRITE,&prior));
    assert(FlushInstructionCache(GetCurrentProcess(),mapped+0x7bb0,sizeof(before)));
    for(unsigned state=0;state<=4;++state) {
        fixture();*(unsigned *)(descriptors+0x54+0x34)=state;reserve();
        SudekiMpRelativeCallHook lookup={0};
        assert(SudekiMpInstallRelativeCallHook(&lookup,mapped+0x7bbc,mapped+0x59b0,(void *)named_lookup_stub));
        uint8_t saved_world[sizeof(world)],saved_table[sizeof(descriptors)];
        memcpy(saved_world,world,sizeof(world));memcpy(saved_table,descriptors,sizeof(descriptors));
        invoke_named("church",descriptors+0x54,0);
        invoke_named("neighbor",descriptors+0xa8,1);
        invoke_named("missing",NULL,0);
        uint64_t outer=0;
        assert(SudekiMpLanStoryAreaEvictionReserve((HMODULE)mapped,&witness,&policy,outside,descriptors,&outer));
        invoke_named("brightwater",descriptors,0);invoke_named("church",descriptors+0x54,0);
        assert(SudekiMpLanStoryAreaEvictionRelease((HMODULE)mapped,&witness,outer));
        release();assert(original_calls==1); /* No unload request is replayed by release. */
        invoke_named("church",descriptors+0x54,1);
        invoke_named("brightwater",descriptors,1);
        assert(!memcmp(saved_world,world,sizeof(world)) && !memcmp(saved_table,descriptors,sizeof(descriptors)));
        assert(SudekiMpRestoreRelativeCallHook(&lookup));finish();
        assert(!memcmp(before,mapped+0x7bb0,sizeof(before)));
    }
    DWORD ignored;assert(VirtualProtect(mapped+0x7bb0,sizeof(before),prior,&ignored));
}
static uint32_t load_context_at_change[2] __attribute__((used));
static void __attribute__((naked,noinline)) load_state_stub(void) {
    __asm__ volatile("incl _original_calls; mov %eax,_original_state;"
        "mov 4(%esp),%eax; mov %eax,_original_descriptor;"
        "mov 0x2c(%eax),%edx; mov %edx,_load_context_at_change;"
        "mov 0x30(%eax),%edx; mov %edx,_load_context_at_change+4; ret $4");
}
static void invoke_load(const char *name,uint8_t *result) {
    uint8_t expected_world[sizeof(world)],expected_table[sizeof(descriptors)];
    memcpy(expected_world,world,sizeof(world));memcpy(expected_table,descriptors,sizeof(descriptors));
    if(result) {
        size_t offset=(size_t)(result-descriptors);
        assert(offset==0 || offset==0x54 || offset==0xa8);
        *(uint32_t *)(expected_table+offset+0x2c)=0;
        *(uint32_t *)(expected_table+offset+0x30)=0;
    }
    named_lookup_text=name;named_lookup_result=result;named_lookups=0;
    load_context_at_change[0]=load_context_at_change[1]=0x99999999;
    unsigned before=original_calls;
    ((void (__cdecl *)(const char *))(uintptr_t)(mapped+0x7b80))(name);
    assert(named_lookups==1 && original_calls==before+(result!=NULL));
    if(result) {
        assert(original_state==1 && original_descriptor==result);
        assert(!load_context_at_change[0] && !load_context_at_change[1]);
    } else assert(load_context_at_change[0]==0x99999999 && load_context_at_change[1]==0x99999999);
    assert(!memcmp(expected_world,world,sizeof(world)));
    assert(!memcmp(expected_table,descriptors,sizeof(descriptors)));
}
static void native_load_control_flow(void) {
    /* The native named LOAD is not an idempotent ensure-resident operation.
     * It clears descriptor load-context fields BEFORE unconditionally requesting
     * state1, including when the descriptor is already loading/ready. The
     * coordinator must reuse its existing load/lifetime for later entrants.
     * Eviction reservations intentionally do not intercept loading. This
     * fixture executes only the wrapper: lookup and state change are stubs,
     * so the assertions do not establish async completion or foreground safety. */
    uint8_t before[0x27];memcpy(before,mapped+0x7b80,sizeof(before));
    DWORD prior;assert(VirtualProtect(mapped+0x7b80,sizeof(before),PAGE_EXECUTE_READWRITE,&prior));
    assert(FlushInstructionCache(GetCurrentProcess(),mapped+0x7b80,sizeof(before)));
    for(unsigned state=0;state<=4;++state) {
        fixture();*(unsigned *)(descriptors+0x54+0x34)=state;reserve();
        uint64_t outer=0;
        assert(SudekiMpLanStoryAreaEvictionReserve((HMODULE)mapped,&witness,&policy,outside,descriptors,&outer));
        SudekiMpRelativeCallHook lookup={0},change={0};
        assert(SudekiMpInstallRelativeCallHook(&lookup,mapped+0x7b8c,mapped+0x59b0,(void *)named_lookup_stub));
        assert(SudekiMpInstallRelativeCallHook(&change,mapped+0x7ba1,mapped+CHANGE_STATE,(void *)load_state_stub));
        for(unsigned repeat=0;repeat<2;++repeat) {
            for(unsigned i=0;i<3;++i) {
                *(uint32_t *)(descriptors+i*0x54+0x2c)=0x12345678u+repeat;
                *(uint32_t *)(descriptors+i*0x54+0x30)=0x87654321u+repeat;
            }
            invoke_load("missing",NULL);
            invoke_load("church",descriptors+0x54);
            invoke_load("brightwater",descriptors);
            invoke_load("neighbor",descriptors+0xa8);
            assert(!unknown && !callbacks);
        }
        assert(original_calls==6);
        assert(SudekiMpRestoreRelativeCallHook(&change));
        assert(SudekiMpRestoreRelativeCallHook(&lookup));
        assert(SudekiMpLanStoryAreaEvictionRelease((HMODULE)mapped,&witness,outer));
        release();finish();assert(!memcmp(before,mapped+0x7b80,sizeof(before)));
    }
    DWORD ignored;assert(VirtualProtect(mapped+0x7b80,sizeof(before),prior,&ignored));
}
static unsigned last_calls[3];
static void last_clear(uint8_t *d) {
    if(*(unsigned *)(d+0x34)) {
        *(unsigned *)(d+0x34)=0;*(void **)(d+0x10)=NULL;
        *(void **)(d+0x18)=*(void **)(d+0x14);*(void **)(d+0x14)=NULL;
    }
}
static void __attribute__((used,noinline,force_align_arg_pointer)) last_record(void *p,unsigned state) {
    assert(state==0 && (uintptr_t)p>=(uintptr_t)descriptors &&
        (uintptr_t)p<(uintptr_t)(descriptors+sizeof(descriptors)));
    unsigned offset=(unsigned)((uint8_t *)p-descriptors);assert(!(offset%0x54));
    ++last_calls[offset/0x54];last_clear(p);
}
static void __attribute__((naked,noinline)) last_state_stub(void) {
    __asm__ volatile("push %eax; push 8(%esp); call _last_record; add $8,%esp; ret $4");
}
static void native_last_active_control_flow(void) {
    /* Execute the actual exported loop and installed admission bridges. State
     * changes below are synthetic mutations, not native resource destruction.
     * Cover every current/last descriptor pairing, including identical ones,
     * no current, no last, empty neighbor list, and repeated requests. */
    uint8_t before[0x5a];memcpy(before,mapped+0x7bd0,sizeof(before));
    DWORD prior;assert(VirtualProtect(mapped+0x7bd0,sizeof(before),PAGE_EXECUTE_READWRITE,&prior));
    assert(FlushInstructionCache(GetCurrentProcess(),mapped+0x7bd0,sizeof(before)));
    for(unsigned last=0;last<3;++last) for(unsigned current=0;current<4;++current)
    for(unsigned state=0;state<=4;++state) for(unsigned mask=0;mask<4;++mask) {
        fixture();original=(void *)(uintptr_t)last_state_stub;
        uint8_t *neighbors[]={descriptors,descriptors+0x54,descriptors+0xa8};
        *(void **)(world+0xc)=current<3?neighbors[current]:NULL;
        *(void **)(world+0x394)=neighbors[last];
        for(unsigned i=0;i<3;++i) {
            *(unsigned *)(neighbors[i]+0x34)=state;
            *(void **)(neighbors[i]+0x10)=neighbors[i];
            *(void **)(neighbors[i]+0x14)=neighbors[i]+4;
            *(void **)(neighbors[i]+0x18)=neighbors[i]+8;
        }
        *(unsigned *)(neighbors[last]+0x3c)=3;
        *(void **)(neighbors[last]+0x44)=neighbors;
        uint64_t outer=0;
        if(mask&1) assert(SudekiMpLanStoryAreaEvictionReserve((HMODULE)mapped,&witness,&policy,outside,descriptors,&outer));
        if(mask&2) reserve();
        uint8_t expected_world[sizeof(world)],expected_table[sizeof(descriptors)];
        memcpy(expected_world,world,sizeof(world));memcpy(expected_table,descriptors,sizeof(descriptors));
        unsigned expected_calls[3]={0};
        for(unsigned i=0;i<3;++i) if(i!=current && !(mask&(1u<<i))) {
            ++expected_calls[i];last_clear(expected_table+i*0x54);
        }
        if(!(mask&(1u<<last))) {++expected_calls[last];last_clear(expected_table+last*0x54);}
        for(unsigned repeat=0;repeat<2;++repeat) {
            memset(last_calls,0,sizeof(last_calls));
            ((void (__cdecl *)(void))(uintptr_t)(mapped+0x7bd0))();
            assert(!unknown && !callbacks && !memcmp(last_calls,expected_calls,sizeof(last_calls)));
            assert(!memcmp(world,expected_world,sizeof(world)) && !memcmp(descriptors,expected_table,sizeof(descriptors)));
        }
        /* The last root is still considered when there are no neighbors. */
        *(unsigned *)(neighbors[last]+0x3c)=0;
        memset(last_calls,0,sizeof(last_calls));
        ((void (__cdecl *)(void))(uintptr_t)(mapped+0x7bd0))();
        for(unsigned i=0;i<3;++i) assert(last_calls[i]==(unsigned)(i==last && !(mask&(1u<<last))));
        *(void **)(world+0x394)=NULL;memset(last_calls,0,sizeof(last_calls));
        ((void (__cdecl *)(void))(uintptr_t)(mapped+0x7bd0))();
        assert(!last_calls[0] && !last_calls[1] && !last_calls[2]);
        if(mask&2) release();
        if(mask&1) assert(SudekiMpLanStoryAreaEvictionRelease((HMODULE)mapped,&witness,outer));
        /* Releasing a reservation does not replay prior unload requests. */
        assert(!last_calls[0] && !last_calls[1] && !last_calls[2]);
        *(void **)(world+0x394)=neighbors[last];
        ((void (__cdecl *)(void))(uintptr_t)(mapped+0x7bd0))();
        assert(last_calls[last]==1);
        finish();assert(!memcmp(before,mapped+0x7bd0,sizeof(before)));
    }
    DWORD ignored;assert(VirtualProtect(mapped+0x7bd0,sizeof(before),prior,&ignored));
}
int main(int argc,char **argv) {
    assert(argc==2);wchar_t path[1024];assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    mapped=map_image(path);assert(!SudekiMpLanStoryAreaEvictionInstall(NULL));
    assert(image_exact((HMODULE)mapped));image_refusals();restoration();normal();malformed();faults();handoff();
    native_bulk_control_flow();native_named_control_flow();native_load_control_flow();native_last_active_control_flow();
    assert(image_exact((HMODULE)mapped));assert(VirtualFree(mapped,0,MEM_RELEASE));
    puts("StoryAreaEvictionImageTest: PASS (six exact seams, ABI/lifecycle, native last-active/bulk/named wrappers with synthetic state changes; no live travel)");
    return 0;
}
