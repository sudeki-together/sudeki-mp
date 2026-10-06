/* Inert supported image plus synthetic traversal/ABI fixtures. No retail
 * collision, geometry, scripts, game constructors or running game execute. */
#include "hooks/call_hook.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned install_calls,fail_install,restore_calls,fail_restore;
void TestStoryCollisionBorn(void *);
void TestStoryCollisionDeleted(void *);
static BOOL test_install(SudekiMpInlineHook *,uint8_t *,const uint8_t *,size_t,const void *);
static BOOL test_restore(SudekiMpInlineHook *);
static DWORD test_thread(void);
#define SudekiMpInstallInlineHook test_install
#define SudekiMpRestoreInlineHook test_restore
#define GetCurrentThreadId test_thread
#include "../src/hooks/lan_story_collision_query.c"
#undef GetCurrentThreadId
#undef SudekiMpInstallInlineHook
#undef SudekiMpRestoreInlineHook
static BOOL test_install(SudekiMpInlineHook *h,uint8_t *p,const uint8_t *e,size_t n,const void *r) {
    if(++install_calls==fail_install) { SetLastError(ERROR_WRITE_FAULT); return FALSE; }
    return SudekiMpInstallInlineHook(h,p,e,n,r);
}
static BOOL test_restore(SudekiMpInlineHook *h) {
    if(++restore_calls==fail_restore || fail_restore==UINT32_MAX) { SetLastError(ERROR_WRITE_FAULT); return FALSE; }
    return SudekiMpRestoreInlineHook(h);
}
static BOOL disturb;
static DWORD test_thread(void) {
    DWORD id=GetCurrentThreadId();
    if(disturb) {
        SetLastError(0xdead);
        __asm__ volatile("fninit; fld1; pxor %%xmm0,%%xmm0; pxor %%xmm7,%%xmm7" : : : "memory");
    }
    return id;
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size); CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;
    IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *base=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(base); memcpy(base,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s->SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s->VirtualAddress);
        memcpy(base+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    uintptr_t delta=(uintptr_t)base-nt->OptionalHeader.ImageBase;
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    unsigned offset=0;
    while(offset<reloc.Size) {
        IMAGE_BASE_RELOCATION *block=(void *)(base+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(uint16_t *)(block+1);
        unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfffu);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4);
                *(uint32_t *)(base+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw); return base;
}

static uint8_t *mapped;
enum { SOURCE_COUNT=502 };
static uint8_t grid[0x15ac],nodes[SOURCE_COUNT][0x110],hit_buffer[0x20];
static void *grid_items[SOURCE_COUNT];
static SudekiMpLanStoryCollisionSourceArea entries[SOURCE_COUNT];
static SudekiMpStoryAreas policy;
static SudekiMpLanStoryCollisionQuerySet set;
static uint32_t hit_result,native_calls,mode;
static BOOL sources_born;
static float position[3];
static void *probe_source __attribute__((used)),*probe_grid __attribute__((used));
static void *probe_caller __attribute__((used));
static uint32_t seen_regs[9] __attribute__((used)),probe_stack __attribute__((used));
static uint32_t branch_taken __attribute__((used));
static uint8_t capacity_taken __attribute__((used));
static void __attribute__((naked,noinline,used)) admit_return(void) {
    __asm__ volatile("setge _capacity_taken; movl $1,_branch_taken; ret");
}
static void __attribute__((naked,noinline,used)) skip_return(void) {
    __asm__ volatile("movl $2,_branch_taken; ret");
}
static void __attribute__((naked,noinline,used)) probe(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,_probe_stack; sub $0x24,%esp;"
        "mov _probe_caller,%eax; mov %eax,0x1c(%esp);"
        "mov _probe_source,%eax; mov _probe_grid,%esi; mov $0x11111111,%ecx;"
        "mov $0x22222222,%edx; mov $0x33333333,%ebx; mov $0x44444444,%ebp;"
        "mov $1,%edi; xor %ecx,%ecx; mov $0x11111111,%ecx; std; stc; call _admission_entry;"
        "mov %eax,_seen_regs; mov %ecx,_seen_regs+4; mov %edx,_seen_regs+8;"
        "mov %ebx,_seen_regs+12; mov %ebp,_seen_regs+16; mov %esi,_seen_regs+20;"
        "mov %edi,_seen_regs+24; pushfl; pop _seen_regs+28; lea 0x24(%esp),%esp; mov %esp,_seen_regs+32;"
        "popal; popfl; ret");
}
extern void fake_complete(void);
static void __attribute__((naked,noinline,used)) probe_completion(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,_probe_stack;"
        "push $0x33333333; push $0x44444444; push $0x55555555; push $0x66666666;"
        "mov $_hit_buffer,%edi; mov $0x22222222,%edx; mov $0x11111111,%ecx;"
        "xor %eax,%eax; std; stc; jmp _completion_entry;"
        ".globl _fake_complete\n_fake_complete:\n"
        "mov %eax,_seen_regs; mov %ecx,_seen_regs+4; mov %edx,_seen_regs+8;"
        "mov %ebx,_seen_regs+12; mov %ebp,_seen_regs+16; mov %esi,_seen_regs+20;"
        "mov %edi,_seen_regs+24; pushfl; pop _seen_regs+28; mov %esp,_seen_regs+32;"
        "popal; popfl; ret");
}
static void continuations(void) {
    admit_resume=(void *)(uintptr_t)admit_return;
    skip_resume=(void *)(uintptr_t)skip_return;
    complete_resume=(void *)(uintptr_t)fake_complete;
}
static void check_fp(BOOL completion) {
    uint8_t before[512] __attribute__((aligned(16)))={0};
    uint8_t after[512] __attribute__((aligned(16)))={0};
    uint16_t control=0x077f; uint32_t mxcsr=0x3f80;
    SetLastError(0x7788); disturb=TRUE;
    __asm__ volatile("fninit; fldcw %1; ldmxcsr %2; fldpi; fld1; fldl2t;"
        "pcmpeqd %%xmm0,%%xmm0; pcmpeqd %%xmm7,%%xmm7; fxsave %0"
        : "=m"(before) : "m"(control),"m"(mxcsr) : "memory");
    if(completion) probe_completion(); else probe();
    __asm__ volatile("fxsave %0; fninit" : "=m"(after) : : "memory");
    disturb=FALSE;
    assert(GetLastError()==0x7788);
    assert(!memcmp(before,after,160) && !memcmp(before+160,after+160,128));
    assert(seen_regs[8]==probe_stack && (seen_regs[7]&0x400)); /* native DF preserved */
    assert(seen_regs[1]==(completion || branch_taken==2?0x11111111:*(uint32_t *)((uint8_t *)probe_grid+0x10)));
    assert(seen_regs[2]==0x22222222 && seen_regs[3]==0x33333333 && seen_regs[4]==0x44444444);
    if(completion) {
        assert(seen_regs[0]==*(uint32_t *)(hit_buffer+0x10));
        assert(seen_regs[5]==0x55555555 && seen_regs[6]==0x66666666 && (seen_regs[7]&1));
    } else {
        assert(seen_regs[0]==(uintptr_t)probe_source && seen_regs[5]==(uintptr_t)probe_grid);
        assert(seen_regs[6]==1);
        if(branch_taken==2) assert(seen_regs[7]&1);
        else assert(capacity_taken==(*(uint32_t *)((uint8_t *)probe_grid+0x10)>=500));
    }
}
static SudekiMpLanStoryCollisionQueryResult run(unsigned slot) {
    return SudekiMpLanStoryCollisionQueryRun(&policy,&set,set.areas[slot],
        nodes[slot?0:501],position,1.25f,hit_buffer,&hit_result);
}
static uint32_t __stdcall fake_query(void *g,const float *p,float radius,void *source,void *out) {
    assert(g==grid && p==position && radius==1.25f && source && out==hit_buffer);
    ++native_calls;
    assert(active && retained());
    assert(!SudekiMpStoryAreaRetireBegin(&policy,set.areas[0]));
    assert(!SudekiMpStoryAreaRetireBegin(&policy,set.areas[1]));
    assert(!SudekiMpLanStoryCollisionQueryUninstall());
    assert(run(0)==SUDEKIMP_STORY_COLLISION_NOT_RUN); /* no nested global scratch */
    if(mode==5) entries[0].area_slot=0; /* caller data is not the hot-loop table */
    *(uint32_t *)(grid+0x10)=0;
    for(unsigned i=0;i<SOURCE_COUNT;++i) {
        probe_grid=grid; probe_source=nodes[i]; branch_taken=0;
        probe(); assert(branch_taken==1 || branch_taken==2);
        if(branch_taken==1) {
            if(capacity_taken) break;
            ++*(uint32_t *)(grid+0x10);
        }
    }
    if(mode==1) {
        probe_source=hit_buffer; probe_grid=grid; probe();
        assert(branch_taken==1 && incomplete); /* unknown retained, never dropped */
    }
    if(mode==2) ++*(uint32_t *)(grid+0x15a8);
    if(mode==3) ++policy.areas[1].ref.lifetime;
    if(mode==6) *(void **)(nodes[0]+8)=hit_buffer;
    if(mode==7) { TestStoryCollisionDeleted(nodes[0]); TestStoryCollisionBorn(nodes[0]); }
    if(mode==4) return 2; /* native geometry scratch bailout: no normal return */
    *(uint32_t *)(hit_buffer+0x10)=*(uint32_t *)(grid+0x10);
    probe_completion(); assert(normal_return);
    return *(uint32_t *)(hit_buffer+0x10);
}
static void setup(void) {
    assert(!active && !retained());
    if(sources_born) for(unsigned i=0;i<SOURCE_COUNT;++i) TestStoryCollisionDeleted(nodes[i]);
    for(unsigned i=0;i<SOURCE_COUNT;++i) TestStoryCollisionBorn(nodes[i]);
    sources_born=TRUE;
    memset(&policy,0,sizeof(policy)); memset(&set,0,sizeof(set)); memset(grid,0,sizeof(grid));
    memset(nodes,0,sizeof(nodes)); memset(hit_buffer,0,sizeof(hit_buffer));
    assert(SudekiMpStoryAreasInitialize(&policy,73));
    assert(SudekiMpStoryAreaLoad(&policy,"fixture_world","",&set.areas[0]));
    assert(SudekiMpStoryAreaReady(&policy,set.areas[0]));
    assert(SudekiMpStoryAreaLoad(&policy,"fixture_world","fixture_room",&set.areas[1]));
    assert(SudekiMpStoryAreaReady(&policy,set.areas[1]));
    for(unsigned i=0;i<SOURCE_COUNT;++i) {
        grid_items[i]=nodes[SOURCE_COUNT-1-i]; /* enrollment need not be sorted */
        entries[i].source=nodes[i]; entries[i].area_slot=i<500?1:0;
        SudekiMpLanStoryCollisionStamp stamp;
        assert(SudekiMpLanStoryCollisionLifetimeObserve((HMODULE)mapped,nodes[i],&stamp));
        entries[i].incarnation=stamp.incarnation; set.lifetime_revision=stamp.revision;
        *(void **)(nodes[i]+8)=grid; nodes[i][0xf2]=1; nodes[i][0xf4]=1;
    }
    *(void **)(grid+0x15a0)=grid_items; *(unsigned *)(grid+0x15a4)=SOURCE_COUNT;
    *(unsigned *)(grid+0x15a8)=SOURCE_COUNT;
    set.grid=grid; set.count=SOURCE_COUNT; set.sources=entries;
    native_query=fake_query; mode=native_calls=0; hit_result=0xa5a5a5a5; continuations();
    probe_caller=mapped+0x1c90f7;
}
static void no_run(void) {
    assert(run(0)==SUDEKIMP_STORY_COLLISION_NOT_RUN);
    assert(!native_calls && hit_result==0xa5a5a5a5 && !active && !retained());
}
static DWORD WINAPI foreign(void *unused) {
    (void)unused;
    assert(run(0)==SUDEKIMP_STORY_COLLISION_NOT_RUN);
    assert(!SudekiMpLanStoryCollisionQueryUninstall());
    probe_grid=grid; probe_source=nodes[0]; probe(); assert(branch_taken==1);
    return 0;
}
static void lifecycle(void) {
    IMAGE_DOS_HEADER *dos=(void *)mapped; LONG offset=dos->e_lfanew;
    dos->e_lfanew=0x7fffffff; install_calls=0;
    assert(!SudekiMpLanStoryCollisionQueryInstall((HMODULE)mapped) && !install_calls);
    dos->e_lfanew=offset;
    for(unsigned step=1;step<=2;++step) {
        install_calls=restore_calls=0; fail_install=step;
        assert(!SudekiMpLanStoryCollisionQueryInstall((HMODULE)mapped));
        assert(!base && !installed && !admission_hook.installed && !completion_hook.installed);
        assert(!memcmp(mapped+ADMISSION,admission_bytes,sizeof(admission_bytes)));
        assert(!memcmp(mapped+0x1c96ff,completion_bytes,sizeof(completion_bytes)));
    }
    fail_install=0;
    const unsigned changes[]={ADMISSION,ADMISSION+3,RESUME,RESUME+5,NEXT,NEXT+4,0x1c8880,
        0x1c8887,0x1c8a23,0x1c8a26,QUERY,0x1c90f2,0x1c96ff,0x1c970c};
    for(unsigned i=0;i<sizeof(changes)/sizeof(*changes);++i) {
        mapped[changes[i]]^=1; install_calls=0;
        assert(!SudekiMpLanStoryCollisionQueryInstall((HMODULE)mapped) && !install_calls);
        mapped[changes[i]]^=1;
    }
    for(unsigned step=1;step<=2;++step) {
        assert(SudekiMpLanStoryCollisionQueryInstall((HMODULE)mapped));
        restore_calls=0; fail_restore=step;
        assert(!SudekiMpLanStoryCollisionQueryUninstall() && base && native_query && admit_resume && complete_resume);
        assert(restore_calls==2); /* independent restorations attempted */
        assert(!SudekiMpLanStoryCollisionQueryInstall((HMODULE)mapped));
        fail_restore=0; assert(SudekiMpLanStoryCollisionQueryUninstall());
    }
    assert(SudekiMpLanStoryCollisionQueryInstall((HMODULE)mapped));
    uint8_t owned[9]; memcpy(owned,mapped+ADMISSION,sizeof(owned)); mapped[ADMISSION]=0xcc;
    assert(!SudekiMpLanStoryCollisionQueryUninstall() && admission_hook.installed && base);
    assert(!completion_hook.installed);
    memcpy(mapped+ADMISSION,owned,sizeof(owned)); assert(SudekiMpLanStoryCollisionQueryUninstall());
    /* Install rollback failure retains the already installed dependency. */
    install_calls=restore_calls=0; fail_install=2; fail_restore=2;
    assert(!SudekiMpLanStoryCollisionQueryInstall((HMODULE)mapped));
    assert(base && completion_hook.installed && !admission_hook.installed);
    fail_install=fail_restore=0; assert(SudekiMpLanStoryCollisionQueryUninstall());
}
static void policy_and_query(void) {
    setup(); assert(run(0)==SUDEKIMP_STORY_COLLISION_COMPLETE);
    assert(hit_result==2 && native_calls==1 && !retained() && !active);
    /* 500 foreign entries precede the two own-area sources, including floor:
     * filtering after admission could not recover them. This fixture only
     * models native admission order; it does not execute retail geometry. */
    setup(); assert(run(1)==SUDEKIMP_STORY_COLLISION_INCOMPLETE);
    assert(hit_result==0xa5a5a5a5 && !retained()); /* conservative exact-capacity */
    setup(); entries[4].source=entries[3].source; no_run();
    setup(); entries[4].area_slot=2; no_run();
    setup(); ++entries[4].incarnation; no_run();
    setup(); --set.lifetime_revision; no_run();
    setup(); set.count=8193; no_run();
    setup(); set.count=501; no_run();
    setup(); entries[501].area_slot=1; no_run();
    setup(); ++set.areas[1].lifetime; no_run();
    setup(); ++set.areas[0].session; no_run();
    setup(); policy.areas[1].phase=SUDEKIMP_STORY_AREA_LOADING; no_run();
    setup(); grid_items[3]=grid_items[2]; no_run();
    setup(); *(void **)(nodes[3]+8)=hit_buffer; no_run();
    setup(); nodes[3][0xf2]=0; no_run();
    setup(); *(uint32_t *)(grid+0x15a8)=501; no_run();
    setup(); position[0]=NAN; no_run(); position[0]=0;
    setup(); void *guard=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS); assert(guard);
    set.sources=guard; no_run(); set.sources=entries;
    set.grid=guard; no_run(); set.grid=grid;
    grid_items[0]=guard; no_run();
    assert(SudekiMpLanStoryCollisionQueryRun(&policy,&set,set.areas[0],nodes[501],guard,
        1.25f,hit_buffer,&hit_result)==SUDEKIMP_STORY_COLLISION_NOT_RUN);
    assert(VirtualFree(guard,0,MEM_RELEASE));
    setup();
    assert(SudekiMpLanStoryCollisionQueryRun(&policy,&set,set.areas[0],nodes[501],position,
        NAN,hit_buffer,&hit_result)==SUDEKIMP_STORY_COLLISION_NOT_RUN);
    assert(SudekiMpLanStoryCollisionQueryRun(&policy,&set,set.areas[0],nodes[501],position,
        -1,hit_buffer,&hit_result)==SUDEKIMP_STORY_COLLISION_NOT_RUN);
    assert(SudekiMpLanStoryCollisionQueryRun(&policy,&set,set.areas[0],nodes[501],position,
        1.25f,NULL,&hit_result)==SUDEKIMP_STORY_COLLISION_NOT_RUN);
    assert(!native_calls && hit_result==0xa5a5a5a5 && !retained());
    setup(); uint64_t pins[SUDEKIMP_STORY_AREA_PINS];
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_PINS-1;++i)
        assert(SudekiMpStoryAreaRetain(&policy,set.areas[0],&pins[i]));
    no_run(); /* first area pin acquired, second fails; first rolled back */
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_PINS-1;++i)
        assert(SudekiMpStoryAreaRelease(&policy,set.areas[0],pins[i]));
    setup(); mode=1; assert(run(0)==SUDEKIMP_STORY_COLLISION_INCOMPLETE);
    assert(hit_result==0xa5a5a5a5 && !retained());
    setup(); mode=2; assert(run(0)==SUDEKIMP_STORY_COLLISION_INCOMPLETE);
    assert(hit_result==0xa5a5a5a5 && !retained());
    setup(); mode=4; assert(run(0)==SUDEKIMP_STORY_COLLISION_INCOMPLETE);
    assert(hit_result==0xa5a5a5a5 && !retained());
    setup(); mode=5; assert(run(0)==SUDEKIMP_STORY_COLLISION_COMPLETE);
    assert(hit_result==2 && !retained());
    setup(); mode=6; assert(run(0)==SUDEKIMP_STORY_COLLISION_INCOMPLETE);
    assert(hit_result==0xa5a5a5a5 && !retained());
    setup(); mode=7; assert(run(0)==SUDEKIMP_STORY_COLLISION_INCOMPLETE);
    assert(hit_result==0xa5a5a5a5 && !retained()); /* same-address replacement */
    setup(); mode=3; assert(run(0)==SUDEKIMP_STORY_COLLISION_INCOMPLETE);
    assert(hit_result==0xa5a5a5a5 && retained() && !active);
    assert(run(0)==SUDEKIMP_STORY_COLLISION_NOT_RUN);
    assert(!SudekiMpLanStoryCollisionQueryUninstall() && base && retained());
    --policy.areas[1].ref.lifetime; assert(SudekiMpLanStoryCollisionQueryUninstall());
    assert(!retained());
}
static void abi(void) {
    assert(SudekiMpLanStoryCollisionQueryInstall((HMODULE)mapped)); setup();
    native_thread=GetCurrentThreadId(); /* only direct synthetic ABI probes bypass Run */
    probe_grid=grid; probe_source=nodes[0];
    check_fp(FALSE); assert(branch_taken==1); /* no scope => untouched native admission */
    normal_return=FALSE; check_fp(TRUE); assert(!normal_return); /* unscoped epilogue */
    assert(prepare(&policy,&set,set.areas[0],nodes[501])); active=TRUE; incomplete=FALSE;
    check_fp(FALSE); assert(branch_taken==2 && !incomplete);
    probe_source=nodes[501]; check_fp(FALSE); assert(branch_taken==1);
    *(uint32_t *)(grid+0x10)=500; check_fp(FALSE); assert(capacity_taken);
    probe_source=hit_buffer; check_fp(FALSE); assert(branch_taken==1 && incomplete);
    incomplete=FALSE; probe_source=nodes[501]; probe_grid=hit_buffer;
    check_fp(FALSE); assert(branch_taken==1 && incomplete);
    probe_grid=grid;
    probe_caller=mapped+0x1c97ac; probe_source=nodes[0]; incomplete=FALSE;
    check_fp(FALSE); assert(branch_taken==1 && incomplete); /* generic query remains unfiltered */
    probe_caller=mapped+0x1c90f7;
    *(uint32_t *)(hit_buffer+0x10)=123; normal_return=FALSE;
    check_fp(TRUE); assert(normal_return);
    HANDLE worker=CreateThread(NULL,0,foreign,NULL,0,NULL); assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0); CloseHandle(worker);
    active=FALSE; assert(release_scope());
    assert(SudekiMpLanStoryCollisionQueryUninstall());
}
static DWORD WINAPI first_native_thread(void *unused) {
    (void)unused; setup();
    assert(!native_thread); /* installation thread is NOT the native game thread */
    assert(run(0)==SUDEKIMP_STORY_COLLISION_COMPLETE && hit_result==2);
    assert(native_thread==GetCurrentThreadId());
    assert(SudekiMpLanStoryCollisionQueryUninstall());
    assert(SudekiMpLanStoryCollisionLifetimeUninstall());
    return 0;
}
static void quarantine(void) {
    assert(SudekiMpLanStoryCollisionQueryInstall((HMODULE)mapped)); setup();
    assert(run(0)==SUDEKIMP_STORY_COLLISION_COMPLETE);
    fail_restore=UINT32_MAX; restore_calls=0;
    assert(!SudekiMpLanStoryCollisionQueryUninstall() && restore_calls==2);
    assert(admission_hook.installed && completion_hook.installed && lifetime_attached && stopping);
    unsigned calls=native_calls;
    assert(run(0)==SUDEKIMP_STORY_COLLISION_NOT_RUN && native_calls==calls);
    assert(!SudekiMpLanStoryCollisionLifetimeUninstall()); /* dependent remains attached */
    fail_restore=0; assert(SudekiMpLanStoryCollisionQueryUninstall());
}
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024)); mapped=map_image(path);
    assert(!SudekiMpLanStoryCollisionQueryInstall(NULL));
    assert(!SudekiMpLanStoryCollisionQueryInstall((HMODULE)mapped)); /* lifetime prerequisite */
    assert(SudekiMpLanStoryCollisionLifetimeInstall((HMODULE)mapped));
    lifecycle(); assert(SudekiMpLanStoryCollisionQueryInstall((HMODULE)mapped));
    HANDLE worker=CreateThread(NULL,0,first_native_thread,NULL,0,NULL); assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0); CloseHandle(worker);
    sources_born=FALSE; /* previous fixture journal positively uninstalled */
    assert(SudekiMpLanStoryCollisionLifetimeInstall((HMODULE)mapped));
    assert(SudekiMpLanStoryCollisionQueryInstall((HMODULE)mapped));
    policy_and_query(); abi(); quarantine();
    assert(!base && !installed && !active && !retained());
    assert(!memcmp(mapped+ADMISSION,admission_bytes,sizeof(admission_bytes)));
    assert(!memcmp(mapped+0x1c96ff,completion_bytes,sizeof(completion_bytes)));
    assert(SudekiMpLanStoryCollisionLifetimeUninstall());
    VirtualFree(mapped,0,MEM_RELEASE);
    puts("story collision query exact-image/synthetic pre-capacity, x87/SSE, pin and teardown tests passed (no native gameplay)");
    return 0;
}
