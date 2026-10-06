/* Supported-image deferred-area-cleanup experiment. Executes native vector
 * copy, resource-manager update, ZoneRemover progress/destruction, graphics
 * cleanup batching and deferred-list erasure. Allocations, vector resizing,
 * names and child/graphics objects are synthetic. No real world, renderer,
 * area destructor, resource load, save or live process is executed. */
#include "engine/build_identity.h"
#include "engine/story_area.h"
#include "hooks/call_hook.h"
#include "hooks/lan_story_area_finalise.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Native retirement test requires supported x86 GCC"
#endif
enum { UPDATE=0x3e250,ERASE=0x38e80,COPY=0x10ddb0,READY=0x107970,
    REMOVER_DELETE=0x1079c0,ROWS_FREE=0x10feb0,GFX_BATCH=0x196cd0,REMOVER_VT=0x2cde68 };
typedef struct Allocation {void *address;unsigned live,kind;} Allocation;
enum { POOL=1,REMOVER,CHILD,GFX,SOURCE };
static Allocation allocations[512];
static unsigned allocation_count,pool_frees,remover_frees,child_frees,gfx_frees;
static uint8_t *mapped,manager[0x300],speed[0x40];
static void *child_table[1],*gfx_table[1],*task_table[4];
static void *removers[2],*children[3],*pending[2];
static unsigned child_live[3],remover_live[2];
static unsigned updates,task_deleted,finish_task,in_pump,graphics_total;
static void *task[4];
static SudekiMpStoryAreas policy;
static SudekiMpStoryAreaRef outside,inside;
static uint64_t pin;
static void *copy_entry __attribute__((used));
static void *enqueue_entry __attribute__((used));
static void *construct_entry,*body_entry;
static unsigned body_id,body_first,body_count;
static int coordinator;
static SudekiMpControlUpdateDispatchWitness witness;
static SudekiMpLanStoryAreaRetirementReceipt observed[SUDEKIMP_AREA_FINALISERS];
static unsigned observed_count;
static uintptr_t sources[2];
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(const SudekiMpControlUpdateDispatchWitness *w) {
    return w && w->native_thread_id==GetCurrentThreadId();
}
static void unavailable(void) {
    SudekiMpLanStoryAreaRetirementReceipt out,before;memset(&out,0x5a,sizeof(out));before=out;unsigned n=777;
    assert(!SudekiMpLanStoryAreaRetirementSnapshot((HMODULE)mapped,&coordinator,&witness,&out,1,&n));
    assert(n==777 && !memcmp(&out,&before,sizeof(out)));
}
static void take(void) {
    assert(SudekiMpLanStoryAreaRetirementSnapshot((HMODULE)mapped,&coordinator,&witness,
        observed,SUDEKIMP_AREA_FINALISERS,&observed_count));
}
static void *allocate(unsigned kind) {
    assert(allocation_count<512);
    void *p=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(p);
    allocations[allocation_count++]=(Allocation){p,1,kind};return p;
}
static void release(void *p,unsigned kind) {
    Allocation *found=NULL;
    for(unsigned i=0;i<allocation_count;++i) if(allocations[i].live && allocations[i].address==p) {
        assert(!found && allocations[i].kind==kind);found=&allocations[i];
    }
    assert(found && VirtualFree(p,0,MEM_RELEASE));found->live=0;
}
static void retained(void) {
    assert(in_pump && !SudekiMpStoryAreaRetireBegin(&policy,inside));
    unavailable();
}
static void * __attribute__((thiscall)) child_delete(void *self,unsigned flags) {
    assert(flags==1);retained();unsigned id=3;
    for(unsigned i=0;i<3;++i) if(children[i]==self && child_live[i]) {id=i;break;}
    assert(id<3 && !*(unsigned *)((uint8_t *)self+0x48));
    if(id==0) assert(gfx_frees==graphics_total);
    child_live[id]=0;++child_frees;release(self,CHILD);return self;
}
static void * __attribute__((thiscall)) gfx_delete(void *self,unsigned flags) {
    assert(flags==1 && !*(unsigned *)((uint8_t *)self+0x24));retained();
    ++gfx_frees;release(self,GFX);return self;
}
static void __cdecl pool_free(void *p) {if(p) {++pool_frees;release(p,POOL);}}
static void __cdecl remover_free(void *p) {
    retained();unsigned id=2;
    for(unsigned i=0;i<2;++i) if(removers[i]==p && remover_live[i]) {id=i;break;}
    assert(id<2 && *(unsigned *)(manager+0x260)>0);
    BOOL listed=FALSE;
    for(unsigned i=0;i<*(unsigned *)(manager+0x260);++i) if(pending[i]==p) listed=TRUE;
    assert(listed); /* Native list erasure is AFTER this object's deletion. */
    if(id==0) assert(!child_live[0] && !child_live[1]);else assert(!child_live[2]);
    remover_live[id]=0;++remover_frees;release(p,REMOVER);
}
static unsigned char __attribute__((thiscall)) task_ready(void *self) {assert(self==task);return 1;}
static unsigned char __attribute__((thiscall)) task_run(void *self) {
    assert(self==task && in_pump);++updates;return (unsigned char)finish_task;
}
static void __attribute__((thiscall)) task_delete(void *self) {assert(self==task);++task_deleted;}
static DWORD WINAPI tick(void) {return 1;} /* Deterministic budget clock, not a game-time test. */
static void __attribute__((used,noinline)) resize_rows(unsigned *v,unsigned count) {
    unavailable(); /* Native copy has not returned; no publishable receipt. */
    assert(!v[0] && !v[1] && !v[2] && count<=2);
    v[0]=v[1]=count;v[2]=count?(uintptr_t)allocate(POOL):0;
}
static void __attribute__((used,noinline)) resize_graphics(unsigned *v,unsigned count) {
    assert(count<v[0]);void *old=(void *)(uintptr_t)v[2];
    void *next=count?allocate(POOL):NULL;
    if(count) memcpy(next,old,count*12);
    release(old,POOL);++pool_frees;v[0]=v[1]=count;v[2]=(uintptr_t)next;
}
#define STUB_SAVE "pushfl; pushal; mov %esp,%ebp; and $-16,%esp; sub $16,%esp; cld;"
#define STUB_RESTORE "mov %ebp,%esp; popal; popfl; ret"
static void __attribute__((naked,noinline)) rows_resize_stub(void) {
    __asm__ volatile(STUB_SAVE "mov 4(%ebp),%eax; mov %eax,(%esp); mov 0(%ebp),%eax;"
        "mov %eax,4(%esp); call _resize_rows;" STUB_RESTORE);
}
static void __attribute__((naked,noinline)) graphics_resize_stub(void) {
    __asm__ volatile(STUB_SAVE "mov 28(%ebp),%eax; mov %eax,(%esp); mov 0(%ebp),%eax;"
        "mov %eax,4(%esp); call _resize_graphics;" STUB_RESTORE);
}
static void __attribute__((naked,noinline)) key_copy_stub(void) {
    __asm__ volatile("mov (%eax),%edx; mov %edx,(%ecx); mov 4(%eax),%edx; mov %edx,4(%ecx);"
        "mov 8(%eax),%edx; mov %edx,8(%ecx); mov %ecx,%eax; ret");
}
static void __attribute__((naked,noinline)) copy_rows(void *destination __attribute__((unused)),
    void *source __attribute__((unused))) {
    __asm__ volatile("push %ebx; push %ebp; mov 12(%esp),%ebp; sub $4,%ebp;"
        "mov 16(%esp),%ebx; sub $0x11c,%ebx; push 16(%esp); push 16(%esp);"
        "call *_copy_entry; pop %ebp; pop %ebx; ret");
}
static void __attribute__((naked,noinline)) submit_remover(void *remover __attribute__((unused))) {
    __asm__ volatile("push %esi; push %ebp; sub $12,%esp; mov 24(%esp),%ebp; mov $_manager,%esi;"
        "lea 0x25c(%esi),%eax; mov %ebp,(%esp); lea (%esp),%edx; lea 4(%esp),%ecx;"
        "push %edx; push %ecx; call *_enqueue_entry; lea 12(%esp),%esp; pop %ebp; pop %esi; ret");
}
static void __attribute__((noipa)) pump(void) {
    assert(!in_pump);in_pump=1;
    typedef void (__attribute__((thiscall)) *Update)(void *,unsigned);
    ((Update)*(void **)(mapped+0x2c824c))(manager+4,16);
    in_pump=0;
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
static void *new_remover(unsigned id,unsigned first,unsigned count,uint8_t *parent) {
    uint8_t *r=allocate(REMOVER),*rows=allocate(SOURCE);
    *(void **)r=mapped+REMOVER_VT;
    unsigned *source=(void *)(parent+0x11c);source[0]=source[1]=count;source[2]=(uintptr_t)rows;
    for(unsigned i=0;i<count;++i) {*(unsigned *)(rows+16*i)=0x33;*(unsigned *)(rows+16*i+4)=100+i;
        *(void **)(rows+16*i+12)=children[first+i];}
    copy_rows(r+4,source);
    assert(*(unsigned *)(r+4)==count && *(unsigned *)(r+8)==count && !*(unsigned *)(r+0x10));
    uint8_t *copied=*(uint8_t **)(r+12);assert(copied!=rows);
    assert(!memcmp(copied,rows,16*count));
    sources[id]=(uintptr_t)parent;
    release(rows,SOURCE); /* Native copy owns a distinct fixture array. */
    removers[id]=r;remover_live[id]=1;return r;
}
static void *WINAPI construct_resource(void *self,unsigned a,unsigned b,unsigned c) {
    assert(!a && !b && !c);unavailable();return self;
}
static void __attribute__((thiscall)) destroy_resource_body(void *self) {
    unavailable();submit_remover(new_remover(body_id,body_first,body_count,self));
}
static void new_resource(unsigned id,unsigned first,unsigned count) {
    uint8_t *parent=allocate(SOURCE);
    typedef void *(WINAPI *Construct)(void *,unsigned,unsigned,unsigned);
    typedef void (__attribute__((thiscall)) *Body)(void *);
    assert(((Construct)construct_entry)(parent,0,0,0)==parent);
    body_id=id;body_first=first;body_count=count;
    ((Body)body_entry)(parent);release(parent,SOURCE);
}
static void scenario(unsigned graphics) {
    memset(manager,0,sizeof(manager));memset(speed,0,sizeof(speed));memset(&policy,0,sizeof(policy));
    assert(SudekiMpStoryAreasInitialize(&policy,1));
    assert(SudekiMpStoryAreaLoad(&policy,"brightwater","",&outside) && SudekiMpStoryAreaReady(&policy,outside));
    assert(SudekiMpStoryAreaLoad(&policy,"brightwater","church",&inside) && SudekiMpStoryAreaReady(&policy,inside));
    assert(SudekiMpStoryAreaRetain(&policy,inside,&pin));
    updates=task_deleted=finish_task=pool_frees=remover_frees=child_frees=gfx_frees=0;
    graphics_total=graphics;InitializeCriticalSection((CRITICAL_SECTION *)(manager+0x14c));
    for(unsigned i=0;i<3;++i) {children[i]=allocate(CHILD);*(void **)children[i]=child_table;child_live[i]=1;}
    unsigned *gv=(void *)((uint8_t *)children[0]+0x48);
    gv[0]=gv[1]=graphics;gv[2]=graphics?(uintptr_t)allocate(POOL):0;
    for(unsigned i=0;i<graphics;++i) {
        uint8_t *g=allocate(GFX);*(void **)g=gfx_table;*(unsigned *)(g+0x24)=1;
        *(void **)((uint8_t *)(uintptr_t)gv[2]+12*i+4)=g;
    }
    memset(pending,0,sizeof(pending));*(unsigned *)(manager+0x264)=2;*(void **)(manager+0x268)=pending;
    new_resource(0,0,2);new_resource(1,2,1);
    take();assert(observed_count==2);
    SudekiMpLanStoryAreaResourceReceipt roots[2];unsigned root_count=0;
    assert(SudekiMpLanStoryAreaResourceSnapshot((HMODULE)mapped,&coordinator,&witness,roots,2,&root_count) && root_count==2);
    assert(roots[1].generation>roots[0].generation);
    uint64_t tickets[2]={observed[0].ticket,observed[1].ticket};
    for(unsigned i=0;i<2;++i) {
        assert(roots[i].phase==SUDEKIMP_AREA_RESOURCE_BODY_RETURNED && roots[i].resource==sources[i] &&
            observed[i].resource_generation==roots[i].generation);
        assert(!SudekiMpLanStoryAreaResourceAcknowledge((HMODULE)mapped,&coordinator,&witness,roots[i].generation));
        assert(observed[i].resource==sources[i] && observed[i].remover==(uintptr_t)removers[i] &&
            observed[i].manager==(uintptr_t)manager && observed[i].copied && observed[i].submitted &&
            !observed[i].copying && !observed[i].submitting && !observed[i].drained && !observed[i].destroyed);
        assert(!SudekiMpLanStoryAreaRetirementAcknowledge((HMODULE)mapped,&coordinator,&witness,tickets[i]));
    }
    assert(!SudekiMpLanStoryAreaFinaliseDetach((HMODULE)mapped,&coordinator,&witness));
    memset(task,0,sizeof(task));task[0]=task_table;*(void **)(manager+0x30)=task;
    *(void **)(mapped+0x408da0)=speed;*(void **)(mapped+0x408d50)=NULL;*(void **)(mapped+0x408d28)=NULL;
    unsigned batches=(graphics+15)/16,rounds=batches+3;
    for(unsigned round=1;round<=rounds;++round) {
        pump();assert(updates==round && !task_deleted);
        take();assert(observed_count==2);
        assert(observed[0].drained==(round==rounds) && observed[1].drained==(round>=2));
        unsigned freed=round<=batches?(round*16<graphics?round*16:graphics):graphics;
        assert(gfx_frees==freed);
        if(round<=batches) assert(child_live[0] && *(unsigned *)((uint8_t *)removers[0]+0x10)==0);
        if(round==1) assert(!child_live[2] && remover_live[1]);
        if(round==2) assert(!remover_live[1]);
        assert(!SudekiMpStoryAreaRetireBegin(&policy,inside));
        if(round<rounds) assert(remover_live[0]);
    }
    assert(!remover_live[0] && !remover_live[1] && !*(unsigned *)(manager+0x260));
    assert(!pending[0] && !pending[1] && child_frees==3 && remover_frees==2 && gfx_frees==graphics);
    for(unsigned i=0;i<2;++i) {
        assert(SudekiMpLanStoryAreaRetirementAcknowledge((HMODULE)mapped,&coordinator,&witness,tickets[i]));
        assert(!SudekiMpLanStoryAreaRetirementAcknowledge((HMODULE)mapped,&coordinator,&witness,tickets[i]));
    }
    take();assert(!observed_count);
    for(unsigned i=0;i<2;++i)
        assert(SudekiMpLanStoryAreaResourceAcknowledge((HMODULE)mapped,&coordinator,&witness,roots[i].generation));
    /* The synthetic policy dependency is released only outside the entire
     * native update, after child cleanup, remover free and list erasure. */
    assert(SudekiMpStoryAreaRelease(&policy,inside,pin));
    assert(SudekiMpStoryAreaRetireBegin(&policy,inside) && SudekiMpStoryAreaRetireReturned(&policy,inside));
    finish_task=1;pump();assert(task_deleted==1 && !*(void **)(manager+0x30));
    DeleteCriticalSection((CRITICAL_SECTION *)(manager+0x14c));
    for(unsigned i=0;i<allocation_count;++i) assert(!allocations[i].live);
}
int main(int argc,char **argv) {
    assert(argc==2);wchar_t path[1024];assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    mapped=map_image(path);
    assert(*(void **)(mapped+REMOVER_VT+8)==mapped+REMOVER_DELETE && *(void **)(mapped+REMOVER_VT+0x18)==mapped+READY);
    assert(SudekiMpLanStoryAreaFinaliseInstall((HMODULE)mapped));
    assert(SudekiMpLanStoryAreaFinaliseAttach((HMODULE)mapped,&coordinator));
    int32_t displacement;memcpy(&displacement,mapped+0x107ab7,4);copy_entry=mapped+0x107abb+displacement;
    memcpy(&displacement,mapped+0x107b12,4);enqueue_entry=mapped+0x107b16+displacement;
    memcpy(&displacement,mapped+0x3cc4e,4);construct_entry=mapped+0x3cc52+displacement;
    memcpy(&displacement,mapped+0x1076b4,4);body_entry=mapped+0x1076b8+displacement;
    /* Constructor and full Zone destructor bodies have broad unrelated engine
     * dependencies; only their ABI-matched test dependencies are synthetic.
     * The production construction/destruction observation bridges still run. */
    SudekiMpInlineHook root_stubs[2]={0};
    assert(SudekiMpInstallInlineHook(&root_stubs[0],mapped+0x107430,mapped+0x107430,6,(void *)(uintptr_t)construct_resource));
    assert(SudekiMpInstallInlineHook(&root_stubs[1],mapped+0x107a10,mapped+0x107a10,6,(void *)(uintptr_t)destroy_resource_body));
    witness.native_thread_id=GetCurrentThreadId();witness.dispatch_serial=1;witness.service_post_original_exact=TRUE;
    child_table[0]=(void *)(uintptr_t)child_delete;gfx_table[0]=(void *)(uintptr_t)gfx_delete;
    task_table[1]=(void *)(uintptr_t)task_run;task_table[2]=(void *)(uintptr_t)task_ready;task_table[3]=(void *)(uintptr_t)task_delete;
    const unsigned sites[]={0x10ddc1,0x10ddf5,0x196d5d,0x10ff4b,0x1079fb};
    const unsigned targets[]={0x10f230,0x4bc0,0x196d70,0x248ddd,0x24844f};
    void *stubs[]={(void *)(uintptr_t)rows_resize_stub,(void *)(uintptr_t)key_copy_stub,
        (void *)(uintptr_t)graphics_resize_stub,(void *)(uintptr_t)pool_free,(void *)(uintptr_t)remover_free};
    SudekiMpRelativeCallHook hooks[5]={0};SudekiMpPointerHook imports[4]={0};
    for(unsigned i=0;i<5;++i) assert(SudekiMpInstallRelativeCallHook(&hooks[i],mapped+sites[i],mapped+targets[i],stubs[i]));
    const unsigned slots[]={0x29a084,0x29a068,0x29a064,0x29a088};
    void *apis[]={(void *)(uintptr_t)TryEnterCriticalSection,(void *)(uintptr_t)EnterCriticalSection,
        (void *)(uintptr_t)LeaveCriticalSection,(void *)(uintptr_t)tick};
    for(unsigned i=0;i<4;++i) assert(SudekiMpInstallPointerHook(&imports[i],(void **)(mapped+slots[i]),*(void **)(mapped+slots[i]),apis[i]));
    const unsigned ranges[][2]={{UPDATE,0x240},{ERASE,0x68},{0x107930,0xe0},{COPY,0x80},
        {ROWS_FREE,0xc0},{GFX_BATCH,0xa0},{0x52c50,3},{0x48b0,0xd1}};
    DWORD old[8],discard;
    for(unsigned i=0;i<8;++i) {assert(VirtualProtect(mapped+ranges[i][0],ranges[i][1],PAGE_EXECUTE_READWRITE,&old[i]));
        assert(FlushInstructionCache(GetCurrentProcess(),mapped+ranges[i][0],ranges[i][1]));}
    InitializeCriticalSection((CRITICAL_SECTION *)(mapped+0x362620));mapped[0x3c3112]=1;
    const unsigned counts[]={0,1,16,17,33};for(unsigned i=0;i<5;++i) scenario(counts[i]);
    DeleteCriticalSection((CRITICAL_SECTION *)(mapped+0x362620));mapped[0x3c3112]=0;
    for(unsigned i=5;i-->0;) assert(SudekiMpRestoreRelativeCallHook(&hooks[i]));
    for(unsigned i=2;i-->0;) assert(SudekiMpRestoreInlineHook(&root_stubs[i]));
    for(unsigned i=4;i-->0;) assert(SudekiMpRestorePointerHook(&imports[i]));
    for(unsigned i=8;i-->0;) assert(VirtualProtect(mapped+ranges[i][0],ranges[i][1],old[i],&discard));
    assert(SudekiMpLanStoryAreaFinaliseDetach((HMODULE)mapped,&coordinator,&witness));
    assert(!SudekiMpLanStoryAreaFinaliseUninstall());
    /* Observer/image dependencies stay alive until isolated process exit. */
    puts("StoryAreaRetirementImageTest: PASS (native cleanup batching/pump/erasure with observer; synthetic resources and policy pin; no live retention proof)");return 0;
}
