/* Exact supported-image PVS queue experiment, NOT a runtime observer.
 * Executes native queue submission (no position/preload work) and drain
 * against synthetic renderer objects/callbacks. Node storage is really freed.
 * Also executes native finalisation control flow with synthetic resources and
 * initialization stubs. No real rendering, loading, scripts or gameplay runs.
 * In particular, callback receipt alone is NOT a room-readiness witness. */
#include "engine/build_identity.h"
#include "hooks/call_hook.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "This exact-image experiment requires x86 GCC"
#endif
enum { SUBMIT=0x20ba70, DRAIN=0x20a170, QUEUE=0x3c370c,
    COUNTER=0x407194, OBJECTS=0x3ade64, MAX_NODES=16 };
enum { NONE, NESTED, SET_PENDING, APPEND_PENDING };
typedef struct Subject {
    void **vtable;
    unsigned id,action,active,entered_counter,empty_on_entry;
    struct Subject *child;
} Subject;
typedef struct Allocation { void *address; unsigned live; } Allocation;
static uint8_t *mapped,submission_object[0xd0],pending_object[0x80];
static uint32_t pending_rows[6];
static void *pending_row_pointer;
static Subject subjects[3];
static void *callback_table[1];
static Allocation allocations[MAX_NODES];
static unsigned allocation_count,free_count,depth,maximum_depth,events[64],event_count;
static void *submit_entry __attribute__((used));
static uint8_t final_world[0x3a0],final_zone[0x130],final_descriptor[0x54];
static unsigned final_init_count __attribute__((used)),final_notify_count __attribute__((used));
static unsigned final_events __attribute__((used)),final_init_order __attribute__((used));
static unsigned final_notify_order __attribute__((used)),final_notify_value __attribute__((used));
static unsigned final_init_phase __attribute__((used));
static void *final_init_data __attribute__((used)),*final_notify_descriptor __attribute__((used));
static float final_bounds[6]={-4,-6,-8,4,6,8};
static void *final_resource_table[0x30],*final_graphics_table[0x34];
static void *final_pvs_table[0x20],*final_callback_table[1];
static void *final_graphics,*final_pvs;
static unsigned final_has_pvs,final_submit_count,final_lookup_count,final_free_count;
static void note(unsigned value) { assert(event_count<64);events[event_count++]=value; }
static void __cdecl unexpected(void) { assert(!"Unexpected native dependency");abort(); }
static void * __cdecl allocate_stub(size_t size) {
    assert(size==12 && allocation_count<MAX_NODES);
    void *p=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(p);
    allocations[allocation_count++]=(Allocation){p,1};return p;
}
static void __cdecl free_stub(void *p) {
    Allocation *record=NULL;
    for(unsigned i=0;i<allocation_count;++i) if(allocations[i].live && allocations[i].address==p) {
        assert(!record);record=&allocations[i];
    }
    assert(record);
    void **node=p;
    if(node[0]==final_descriptor+0x48) {
        assert(node[1]==final_descriptor && final_init_count==1 && final_notify_count==1);
        assert(VirtualFree(p,0,MEM_RELEASE));record->live=0;++free_count;++final_free_count;
    } else {
        Subject *s=node[1];
        assert(node[0]==s && s>=subjects && s<subjects+3 && !s->active);
        unsigned id=s->id; /* Copy while alive; never read node after VirtualFree. */
        assert(VirtualFree(p,0,MEM_RELEASE));record->live=0;++free_count;note(id*10+3);
    }
}
static void __attribute__((naked,noinline)) submit_native(
    void *object __attribute__((unused)),void *callback __attribute__((unused)),
    void *argument __attribute__((unused))) {
    __asm__ volatile("push %esi; push %edi; mov 12(%esp),%esi; xor %edi,%edi;"
        "mov 16(%esp),%eax; mov 20(%esp),%edx;"
        "push %edx; push %eax; push $0; push $0; push $0;"
        "call *_submit_entry; pop %edi; pop %esi; ret");
}
static void queue_subject(Subject *s) {
    unsigned before=allocation_count;
    submit_native(submission_object,s,s);
    assert(allocation_count==before+1);
    void **node=*(void ***)(mapped+QUEUE);
    assert(node==allocations[before].address && node[0]==s && node[1]==s);
}
static void drain(void) { ((void (__cdecl *)(void))(mapped+DRAIN))(); }
static void __attribute__((thiscall)) callback(void *self,void *argument) {
    Subject *s=self;assert(self==argument && s>=subjects && s<subjects+3 && !s->active);
    ++s->active;++depth;if(depth>maximum_depth)maximum_depth=depth;
    s->entered_counter=*(unsigned *)(mapped+COUNTER);
    s->empty_on_entry=*(void **)(mapped+QUEUE)==NULL;note(s->id*10+1);
    if(s->action==NESTED) {
        assert(s->child);queue_subject(s->child);drain();
        /* The nested node has been freed while this callback still owns
         * its detached outer node. An empty global list is not quiescence. */
        assert(!*(void **)(mapped+QUEUE) && s->active && depth==1 && free_count==1);
    } else if(s->action==SET_PENDING || s->action==APPEND_PENDING) {
        *(unsigned *)(mapped+COUNTER)=1; /* Synthetic new work, not live loader state. */
        if(s->action==APPEND_PENDING) {assert(s->child);queue_subject(s->child);}
    }
    note(s->id*10+2);--depth;--s->active;
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
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s->SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s->VirtualAddress);
        memcpy(b+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
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
static void reset_case(void) {
    assert(!depth && !*(void **)(mapped+QUEUE));
    for(unsigned i=0;i<allocation_count;++i) assert(!allocations[i].live);
    memset(allocations,0,sizeof(allocations));memset(subjects,0,sizeof(subjects));
    allocation_count=free_count=maximum_depth=event_count=0;
    memset(mapped+OBJECTS,0,15*4);*(unsigned *)(mapped+COUNTER)=0;
    memset(submission_object,0,sizeof(submission_object));
    *(unsigned *)(submission_object+8)=0x80000000u; /* Skip native preload dependency. */
    for(unsigned i=0;i<3;++i) {subjects[i].vtable=callback_table;subjects[i].id=i+1;}
}
static void expect(const unsigned *wanted,unsigned count) {
    assert(event_count==count && !memcmp(events,wanted,count*sizeof(*wanted)));
    assert(free_count==allocation_count && !depth && !*(void **)(mapped+QUEUE));
    for(unsigned i=0;i<allocation_count;++i) assert(!allocations[i].live);
}
static void readiness_gates(void) {
    reset_case();queue_subject(&subjects[0]);queue_subject(&subjects[1]);
    void *head=*(void **)(mapped+QUEUE);
    const unsigned pending[]={1,2,UINT32_MAX};
    for(unsigned i=0;i<3;++i) {
        *(unsigned *)(mapped+COUNTER)=pending[i];drain();
        assert(*(void **)(mapped+QUEUE)==head && !event_count && !free_count);
    }
    *(unsigned *)(mapped+COUNTER)=0;
    memset(pending_object,0,sizeof(pending_object));memset(pending_rows,0,sizeof(pending_rows));
    pending_row_pointer=pending_rows;
    *(void **)(pending_object+0x70)=&pending_row_pointer;
    *(uint16_t *)(pending_object+0x3c)=3;pending_rows[4]=1;
    for(unsigned slot=0;slot<15;++slot) {
        *(void **)(mapped+OBJECTS+slot*4)=pending_object;
        drain();assert(*(void **)(mapped+QUEUE)==head && !event_count && !free_count);
        *(void **)(mapped+OBJECTS+slot*4)=NULL;
    }
    /* All three rows now clear: the native loop drains the LIFO queue. */
    pending_rows[4]=0;*(void **)(mapped+OBJECTS+14*4)=pending_object;drain();
    const unsigned order[]={21,22,23,11,12,13};expect(order,6);
    assert(maximum_depth==1 && !subjects[1].empty_on_entry && subjects[0].empty_on_entry);
}
static void nested_retirement(void) {
    reset_case();subjects[0].action=NESTED;subjects[0].child=&subjects[1];
    queue_subject(&subjects[0]);drain();
    const unsigned order[]={11,21,22,23,12,13};expect(order,6);
    assert(maximum_depth==2 && subjects[0].empty_on_entry && subjects[1].empty_on_entry);
}
static void batch_is_not_per_callback_ready(void) {
    reset_case();subjects[0].action=SET_PENDING;
    queue_subject(&subjects[1]);queue_subject(&subjects[0]);drain();
    const unsigned order[]={11,12,13,21,22,23};expect(order,6);
    assert(!subjects[0].entered_counter && subjects[1].entered_counter==1);
    assert(*(unsigned *)(mapped+COUNTER)==1);
    reset_case();subjects[0].action=APPEND_PENDING;subjects[0].child=&subjects[2];
    queue_subject(&subjects[0]);drain();
    const unsigned appended[]={11,12,13,31,32,33};expect(appended,6);
    assert(subjects[2].entered_counter==1 && *(unsigned *)(mapped+COUNTER)==1);
}
/* Native ZoneFinalise::Run calls the resource's stage method before testing
 * phase 5; it is NOT a read-only readiness query. This fixture executes that
 * real pair, then the real PVS callback through the real queue. It deliberately
 * has no actors, terrain, resource manager, GPU or running script engine. */
static void __attribute__((thiscall)) final_name(void *self,void *out) {
    assert(self==final_zone+4);memset(out,0,12);
}
static unsigned char __attribute__((thiscall)) final_no_collision(void *self) {
    assert(self==final_zone+0x54);return 0;
}
static void * __attribute__((thiscall)) final_get_bounds(void *self) {
    assert(self==&final_graphics);return final_bounds;
}
static void * __attribute__((thiscall)) final_find_pvs(void *self,unsigned identifier) {
    assert(self==&final_graphics && identifier==0x1a535650u);++final_lookup_count;
    return final_has_pvs?&final_pvs:NULL;
}
static void __attribute__((thiscall)) final_submit(void *self,const float *position,
    void *callback_object,void *argument) {
    assert(self==&final_pvs && callback_object==final_descriptor+0x48 && argument==final_descriptor);
    assert(position && position[0]==0 && position[1]==0 && position[2]==0);
    /* This virtual dependency is synthetic; its actual native queue submission
     * remains exercised. No PVS resource loading or renderer work is claimed. */
    ++final_submit_count;submit_native(submission_object,callback_object,argument);
}
static void __attribute__((naked,noinline)) final_copy_bounds(void) {
    __asm__ volatile("push %esi; push %edi; mov %ecx,%esi; mov %eax,%edi;"
        "mov $6,%ecx; cld; rep movsl; pop %edi; pop %esi; ret");
}
static void __attribute__((naked,noinline)) final_init(void) {
    __asm__ volatile("mov %eax,_final_init_data; incl _final_init_count; incl _final_events;"
        "mov _final_events,%eax; mov %eax,_final_init_order;"
        "movzbl _final_zone+0x128,%eax; mov %eax,_final_init_phase; ret");
}
static void __attribute__((naked,noinline)) final_notify(void) {
    __asm__ volatile("mov %edi,_final_notify_descriptor; mov 4(%esp),%eax;"
        "mov %eax,_final_notify_value; incl _final_notify_count; incl _final_events;"
        "mov _final_events,%eax; mov %eax,_final_notify_order; ret $4");
}
static void native_finalisation(void) {
    enum { FINALISE=0x14a640, ADVANCE=0x109800, AREA_CALLBACK=0x1097a0,
        WORLD=0x408d10, GPU=0x408d58, SCORE=0x409d80 };
    /* Verify the native class methods before supplying synthetic vtables. */
    assert(*(void **)(mapped+0x2c89ac)==mapped+FINALISE);
    assert(*(void **)(mapped+0x2cdd14)==mapped+ADVANCE);
    static const unsigned sites[]={0x109d05,0x109fe0,0x109fe9,
        0x1097cf,0x1097d6,0x1097e5,0x1097ec};
    static const unsigned targets[]={0x10a020,0x10d620,0x10a070,
        0x10a070,0x10d620,0x10a070,0x10d620};
    void *stubs[]={(void *)final_copy_bounds,(void *)final_notify,(void *)final_init,
        (void *)final_init,(void *)final_notify,(void *)final_init,(void *)final_notify};
    SudekiMpRelativeCallHook hooks[7]={{0}};
    static const unsigned entries[]={FINALISE,AREA_CALLBACK};
    static const unsigned lengths[]={0x20,0x865}; /* Callback + complete stage routine. */
    uint8_t before_finalise[0x20],before_advance[0x865];
    uint8_t *before[]={before_finalise,before_advance};DWORD protections[2],ignored;
    for(unsigned i=0;i<2;++i) memcpy(before[i],mapped+entries[i],lengths[i]);
    for(unsigned i=0;i<7;++i)
        assert(SudekiMpInstallRelativeCallHook(&hooks[i],mapped+sites[i],mapped+targets[i],stubs[i]));
    for(unsigned i=0;i<2;++i) {
        assert(VirtualProtect(mapped+entries[i],lengths[i],PAGE_EXECUTE_READWRITE,&protections[i]));
        assert(FlushInstructionCache(GetCurrentProcess(),mapped+entries[i],lengths[i]));
    }
    void *world_before=*(void **)(mapped+WORLD),*gpu_before=*(void **)(mapped+GPU);
    void *score_before=*(void **)(mapped+SCORE);
    *(void **)(mapped+WORLD)=final_world;*(void **)(mapped+GPU)=NULL;*(void **)(mapped+SCORE)=NULL;
    final_resource_table[3]=(void *)final_name;
    final_resource_table[11]=mapped+ADVANCE;
    final_graphics_table[0x6c/4]=(void *)final_get_bounds;
    final_graphics_table[0xcc/4]=(void *)final_find_pvs;
    final_pvs_table[0x7c/4]=(void *)final_submit;
    final_callback_table[0]=mapped+AREA_CALLBACK;
    void *collision_table[]={(void *)final_no_collision};
    typedef unsigned char (__attribute__((thiscall)) *Run)(void *);
    Run run=(Run)(mapped+FINALISE);
    for(unsigned pvs=0;pvs<=1;++pvs) for(unsigned promotes=0;promotes<=1;++promotes) {
        reset_case();memset(final_world,0,sizeof(final_world));memset(final_zone,0,sizeof(final_zone));
        memset(final_descriptor,0,sizeof(final_descriptor));
        final_graphics=final_graphics_table;final_pvs=final_pvs_table;
        final_has_pvs=pvs;final_submit_count=final_lookup_count=final_free_count=0;
        final_init_count=final_notify_count=final_events=final_init_order=final_notify_order=0;
        final_notify_value=final_init_phase=0;final_init_data=final_notify_descriptor=NULL;
        *(void **)(final_world+0x10)=subjects; /* Unrelated original foreground. */
        *(void **)(final_world+0x58)=final_descriptor;
        *(void **)(final_zone+4)=final_resource_table;
        *(void **)(final_zone+0x54)=collision_table;
        *(void **)(final_zone+0x6c)=&final_graphics;
        *(unsigned *)(final_descriptor+0x34)=2; /* Skip unrelated resource-state notification. */
        *(void **)(final_descriptor+0x48)=final_callback_table;
        final_descriptor[0x50]=(uint8_t)(promotes?2:0);
        void *task[6]={0};task[4]=final_zone;
        /* Empty synthetic stages 0..3; stage 3 intentionally creates no floor. */
        for(unsigned stage=0;stage<4;++stage) {
            assert(!run(task));assert(final_zone[0x128]==stage+1);
            assert(!final_events && !allocation_count);
            /* The stack-recovered pointer at native 509A90 is the descriptor,
             * not the world. All four stages return its resource to pending. */
            assert(!*(void **)(final_descriptor+0x14));
            assert(*(void **)(final_descriptor+0x18)==final_zone);
            assert(!*(void **)(final_world+0x14) && !*(void **)(final_world+0x18));
        }
        assert(run(task)); /* Actual finalisation reports terminal now. */
        assert(final_zone[0x128]==5 && final_lookup_count==1 && final_submit_count==pvs);
        assert(*(void **)(final_descriptor+0x14)==final_zone && !*(void **)(final_descriptor+0x18));
        assert(!*(unsigned *)(final_descriptor+0x30) && !*(unsigned *)(final_descriptor+0x2c));
        assert(*(void **)(final_world+0x10)==subjects && !final_world[0x399]);
        if(pvs) {
            assert(!final_events && allocation_count==1 && !free_count && *(void **)(mapped+QUEUE));
            *(unsigned *)(mapped+COUNTER)=1;drain();
            assert(!final_events && !free_count && *(void **)(mapped+QUEUE));
            *(unsigned *)(mapped+COUNTER)=0;drain();
            assert(final_free_count==1 && free_count==1 && !*(void **)(mapped+QUEUE));
            assert(final_init_order==1 && final_notify_order==2 && final_init_phase==5);
            assert(*(void **)(final_world+0x10)==(promotes?(void *)final_descriptor:(void *)subjects));
            assert(final_world[0x399]==promotes);
        } else {
            assert(!allocation_count && !*(void **)(mapped+QUEUE));
            assert(final_notify_order==1 && final_init_order==2 && final_init_phase==4);
        }
        assert(final_events==2 && final_init_count==1 && final_notify_count==1);
        assert(final_init_data==final_zone && final_notify_descriptor==final_descriptor && final_notify_value==1);
        assert(!*(void **)(final_zone+0x2c)); /* Still NO floor: not playability proof. */
        /* A cancelled/already-released resource also returns true at phase 5.
         * The finaliser predicate does not distinguish that from successful setup. */
        final_zone[0x129]=5;
        uint8_t descriptor_before[0x54];memcpy(descriptor_before,final_descriptor,sizeof(descriptor_before));
        assert(run(task) && final_events==2 && final_zone[0x128]==5);
        assert(!memcmp(descriptor_before,final_descriptor,sizeof(descriptor_before)));
    }
    reset_case();
    *(void **)(mapped+WORLD)=world_before;*(void **)(mapped+GPU)=gpu_before;*(void **)(mapped+SCORE)=score_before;
    for(unsigned i=7;i>0;--i) assert(SudekiMpRestoreRelativeCallHook(&hooks[i-1]));
    for(unsigned i=0;i<2;++i) {
        assert(!memcmp(before[i],mapped+entries[i],lengths[i]));
        assert(VirtualProtect(mapped+entries[i],lengths[i],protections[i],&ignored));
        MEMORY_BASIC_INFORMATION info;
        assert(VirtualQuery(mapped+entries[i],&info,sizeof(info))==sizeof(info) && info.Protect==protections[i]);
    }
}
int main(int argc,char **argv) {
    assert(argc==2);wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    mapped=map_image(path);assert(!*(void **)(mapped+QUEUE));
    void *objects_before[15];memcpy(objects_before,mapped+OBJECTS,sizeof(objects_before));
    unsigned counter_before=*(unsigned *)(mapped+COUNTER);
    uint8_t submit_before[0xa4],drain_before[0x89];
    memcpy(submit_before,mapped+SUBMIT,sizeof(submit_before));
    memcpy(drain_before,mapped+DRAIN,sizeof(drain_before));
    static const unsigned sites[]={0x20ba8e,0x20ba95,0x20bafd,0x20a1e5};
    static const unsigned targets[]={0x1f8880,0x2484fa,0x2097b0,0x24844f};
    void *stubs[]={(void *)unexpected,(void *)allocate_stub,(void *)unexpected,(void *)free_stub};
    SudekiMpRelativeCallHook hooks[4]={{0}};
    for(unsigned i=0;i<4;++i)
        assert(SudekiMpInstallRelativeCallHook(&hooks[i],mapped+sites[i],mapped+targets[i],stubs[i]));
    DWORD submit_protection,drain_protection,ignored;
    assert(VirtualProtect(mapped+SUBMIT,sizeof(submit_before),PAGE_EXECUTE_READWRITE,&submit_protection));
    assert(VirtualProtect(mapped+DRAIN,sizeof(drain_before),PAGE_EXECUTE_READWRITE,&drain_protection));
    assert(FlushInstructionCache(GetCurrentProcess(),mapped+SUBMIT,sizeof(submit_before)));
    assert(FlushInstructionCache(GetCurrentProcess(),mapped+DRAIN,sizeof(drain_before)));
    submit_entry=mapped+SUBMIT;callback_table[0]=(void *)callback;
    readiness_gates();nested_retirement();batch_is_not_per_callback_ready();reset_case();
    native_finalisation();
    for(unsigned i=4;i>0;--i) assert(SudekiMpRestoreRelativeCallHook(&hooks[i-1]));
    assert(!memcmp(submit_before,mapped+SUBMIT,sizeof(submit_before)));
    assert(!memcmp(drain_before,mapped+DRAIN,sizeof(drain_before)));
    assert(VirtualProtect(mapped+SUBMIT,sizeof(submit_before),submit_protection,&ignored));
    assert(VirtualProtect(mapped+DRAIN,sizeof(drain_before),drain_protection,&ignored));
    memcpy(mapped+OBJECTS,objects_before,sizeof(objects_before));
    *(unsigned *)(mapped+COUNTER)=counter_before;submit_entry=NULL;
    assert(VirtualFree(mapped,0,MEM_RELEASE));
    puts("StoryAreaPvsImageTest: PASS (native finalisation/queue/drain, synthetic dependencies, real node freeing; not playable-area proof)");
    return 0;
}
