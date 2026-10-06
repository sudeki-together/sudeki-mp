/* Exact-image experiment: actual shared game-update scheduler, area entity
 * suspension/resumption and intrusive list operations; synthetic descriptors,
 * generic spawn entities and virtual callbacks. No loading, terrain, AI,
 * scripts, camera, or live-area retention is exercised. This is NOT an adapter
 * that skips TEMP suspension or an authorization to adjust native counters. */
#include "engine/build_identity.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "The supported scheduler experiment requires x86 GCC"
#endif
enum { IMAGE_SIZE=0x45f000, CLOCK_SLOT=0x409e14, WORLD_SLOT=0x408d10,
    SUSPEND=0x10b820, RESUME=0x10b560, UPDATE=0x134180, ENTITIES=4 };
#define PTR(b,o) (*(void **)((uint8_t *)(b)+(o)))
#define U32(b,o) (*(uint32_t *)((uint8_t *)(b)+(o)))
#define I16(b,o) (*(int16_t *)((uint8_t *)(b)+(o)))
#define F32(b,o) (*(float *)((uint8_t *)(b)+(o)))
typedef struct Area { uint8_t descriptor[0x54],resource[0x130],spawns[0x20],rows[2*0x90]; } Area;
typedef struct Entity { uint8_t body[0x140],component[0x80]; } Entity;
typedef struct Sample { uint32_t begin,end,delta; float seconds; } Sample;
typedef void (__stdcall *AreaCall)(void *);
static uint8_t *mapped,clock_state[0x20],lists[2][0x18],world[0x3a0];
static Area areas[2];
static Entity entities[ENTITIES];
static void *node_vtable[6],*update_entry __attribute__((used));
static unsigned ticks[ENTITIES],pauses[ENTITIES],resumes[ENTITIES],scales[ENTITIES];
static Sample samples[ENTITIES];
static unsigned pause_during_update=ENTITIES;
static unsigned entity_index(const void *node) {
    for(unsigned i=0;i<ENTITIES;++i) if(node==entities[i].body+8) return i;
    assert(!"unknown synthetic node"); return 0;
}
static void __attribute__((thiscall,force_align_arg_pointer)) node_update(void *node,const Sample *s) {
    unsigned i=entity_index(node);
    assert(!entities[i].body[0x2b] && !I16(entities[i].component,0x74));
    assert(s && s->end>s->begin && s->end<=U32(clock_state,0x10));
    assert(s->delta==s->end-s->begin && s->delta<=1024);
    assert(s->seconds>0.0f && s->seconds<1.0f);
    samples[i]=*s; ++ticks[i];
    if(pause_during_update==i) {
        pause_during_update=ENTITIES;
        ((AreaCall)(uintptr_t)(mapped+SUSPEND))(areas[i/2].descriptor);
    }
}
static void __attribute__((thiscall,force_align_arg_pointer)) node_pause(void *node) {
    unsigned i=entity_index(node); assert(!entities[i].body[0x2b]); ++pauses[i];
}
static void __attribute__((thiscall,force_align_arg_pointer)) node_resume(void *node) {
    unsigned i=entity_index(node); assert(!entities[i].body[0x2b]); ++resumes[i];
}
static void __attribute__((thiscall,force_align_arg_pointer)) node_scale(void *node,float factor) {
    assert(factor==1.0f); ++scales[entity_index(node)];
}
static void __attribute__((naked,noinline)) advance(unsigned amount __attribute__((unused)),
    void *clock __attribute__((unused))) {
    __asm__ volatile("push %edi; mov 8(%esp),%eax; mov 12(%esp),%edi;"
        "call *_update_entry; pop %edi; ret");
}
static void area_suspend(unsigned i) { assert(i<2); ((AreaCall)(uintptr_t)(mapped+SUSPEND))(areas[i].descriptor); }
static void area_resume(unsigned i) { assert(i<2); ((AreaCall)(uintptr_t)(mapped+RESUME))(areas[i].descriptor); }
static unsigned enrollment(unsigned i) {
    unsigned count=0;
    for(unsigned list=0;list<2;++list) {
        uint8_t *previous=NULL,*node=PTR(lists[list],0x10);
        unsigned visited=0;
        while(node) {
            assert(++visited<=ENTITIES);
            unsigned n=entity_index(node);
            assert(PTR(node,0x18)==previous && !entities[n].body[0x2b]);
            assert((I16(entities[n].body,0x28)==0)==(list==0));
            if(n==i) ++count;
            previous=node; node=PTR(node,0x1c);
        }
    }
    assert(count<=1); return count;
}
static void check_area(unsigned area,unsigned depth) {
    for(unsigned i=2*area;i<2*area+2;++i) {
        assert(entities[i].body[0x2b]==depth);
        assert(I16(entities[i].component,0x74)==(int)depth);
        assert(enrollment(i)==(depth==0 && I16(entities[i].body,0x28)!=-1));
    }
}
static void step(unsigned count) {
    for(unsigned n=0;n<count;++n) {
        uint32_t before=U32(clock_state,0x10);
        uint8_t world_before[sizeof(world)]; memcpy(world_before,world,sizeof(world));
        advance(1,clock_state);
        assert(U32(clock_state,0x10)==before+1024);
        assert(!memcmp(world_before,world,sizeof(world)));
        for(unsigned i=0;i<ENTITIES;++i) (void)enrollment(i);
    }
}
static void reset_fixture(void) {
    memset(clock_state,0,sizeof(clock_state)); memset(lists,0,sizeof(lists));
    memset(areas,0,sizeof(areas)); memset(entities,0,sizeof(entities));
    memset(ticks,0,sizeof(ticks)); memset(pauses,0,sizeof(pauses));
    memset(resumes,0,sizeof(resumes)); memset(scales,0,sizeof(scales));
    memset(samples,0,sizeof(samples)); memset(world,0xa5,sizeof(world));
    pause_during_update=ENTITIES;
    F32(clock_state,4)=1.0f; U32(clock_state,0x14)=0x10000;
    PTR(clock_state,0x18)=lists[0]; PTR(clock_state,0x1c)=lists[1];
    F32(lists[0],0)=F32(lists[1],0)=1.0f;
    U32(lists[0],4)=U32(lists[1],4)=1;
    PTR(mapped,CLOCK_SLOT)=clock_state; PTR(mapped,WORLD_SLOT)=world;
    PTR(world,0xc)=areas[0].descriptor; PTR(world,0x10)=areas[0].descriptor;
    for(unsigned a=0;a<2;++a) {
        PTR(areas[a].descriptor,0x14)=areas[a].resource;
        U32(areas[a].descriptor,0x34)=a?4:3;
        PTR(areas[a].resource,0x118)=areas[a].spawns;
        U32(areas[a].spawns,0x10)=2;
        PTR(areas[a].spawns,0x18)=areas[a].rows;
        for(unsigned j=0;j<2;++j) {
            unsigned i=2*a+j;
            PTR(areas[a].rows,j*0x90+0x78)=entities[i].body+0x2c;
            PTR(entities[i].body,8)=node_vtable;
            I16(entities[i].body,0x28)=(int16_t)j;
            entities[i].body[0x2b]=1;
            PTR(entities[i].body,0x58)=entities[i].component;
            PTR(entities[i].component,0x10)=entities[i].body;
            I16(entities[i].component,0x74)=1;
        }
    }
}
static void shared_clock(unsigned first) {
    reset_fixture(); area_resume(first); step(4);
    assert(ticks[2*first]==4 && ticks[2*first+1]==3);
    assert(!ticks[2*(1-first)] && !ticks[2*(1-first)+1]);
    area_resume(1-first); check_area(0,0); check_area(1,0);
    unsigned previous[ENTITIES]; memcpy(previous,ticks,sizeof(previous));
    for(unsigned n=0;n<12;++n) {
        /* The scheduler never selects a different world or advances a second
         * area clock. Changing only this fixture's foreground identity cannot
         * stop enrolled nodes from either spawn catalog. */
        PTR(world,0xc)=areas[n%2].descriptor;
        PTR(world,0x10)=areas[n%2].descriptor;
        step(1);
        assert(samples[0].end==samples[2].end);
        if(n) assert(samples[1].end==samples[3].end);
    }
    assert(ticks[0]-previous[0]==12 && ticks[2]-previous[2]==12);
    assert(ticks[2*first+1]-previous[2*first+1]==12);
    assert(ticks[2*(1-first)+1]-previous[2*(1-first)+1]==11);
    for(unsigned i=0;i<ENTITIES;++i) assert(resumes[i]==1 && scales[i]==1 && !pauses[i]);
}
static void independent_balanced_suspension(unsigned suspended) {
    reset_fixture(); area_resume(0); area_resume(1); step(4);
    unsigned other=1-suspended, before[ENTITIES]; memcpy(before,ticks,sizeof(before));
    area_suspend(suspended); check_area(suspended,1); check_area(other,0);
    step(3);
    area_suspend(suspended); check_area(suspended,2); step(3);
    area_resume(suspended); check_area(suspended,1); step(3);
    for(unsigned i=2*suspended;i<2*suspended+2;++i)
        assert(ticks[i]==before[i] && pauses[i]==1 && resumes[i]==1);
    for(unsigned i=2*other;i<2*other+2;++i) assert(ticks[i]==before[i]+9);
    area_resume(suspended); check_area(suspended,0); step(3);
    for(unsigned i=2*suspended;i<2*suspended+2;++i)
        assert(ticks[i]==before[i]+3 && resumes[i]==2 && scales[i]==2);
    /* Repeated independent lifecycle, no duplicate list enrollment. */
    for(unsigned n=0;n<16;++n) {
        unsigned a=n%2; area_suspend(a); step(2); area_resume(a); step(2);
        check_area(0,0); check_area(1,0);
    }
}
static void mutation_inside_native_dispatch(unsigned i) {
    reset_fixture(); area_resume(0); area_resume(1); step(4);
    unsigned other=1-i/2,before[ENTITIES]; memcpy(before,ticks,sizeof(before));
    pause_during_update=i; step(1); assert(pause_during_update==ENTITIES);
    check_area(i/2,1); check_area(other,0);
    unsigned stopped[2]={ticks[2*(i/2)],ticks[2*(i/2)+1]};
    step(4);
    assert(ticks[2*(i/2)]==stopped[0] && ticks[2*(i/2)+1]==stopped[1]);
    for(unsigned j=2*other;j<2*other+2;++j) assert(ticks[j]==before[j]+5);
    area_resume(i/2); step(2); check_area(0,0); check_area(1,0);
}
static void unscheduled_nodes(void) {
    reset_fixture(); I16(entities[1].body,0x28)=-1;
    area_resume(0); area_resume(1); check_area(0,0); step(4);
    assert(!ticks[1] && resumes[1]==1 && !scales[1]);
    area_suspend(0); area_resume(0); check_area(0,0); step(4);
    assert(!ticks[1] && pauses[1]==1 && resumes[1]==2 && !scales[1]);
    assert(ticks[0]==8 && ticks[2]==8 && ticks[3]==7);
}
static void resume_is_not_idempotent(void) {
    reset_fixture(); area_resume(0); step(2);
    /* Native exit resume after a skipped entry pause underflows, even though
     * the nodes remain enrolled. This negative fixture is not used by runtime. */
    area_resume(0);
    for(unsigned i=0;i<2;++i) {
        assert(entities[i].body[0x2b]==255 && I16(entities[i].component,0x74)==-1);
        assert(resumes[i]==1 && scales[i]==1 && !pauses[i]);
    }
    /* Deliberately do not dispatch malformed synthetic nodes. */
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(f!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(f,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(f,raw,size,&got,NULL) && got==size);
    assert(CloseHandle(f));
    IMAGE_DOS_HEADER *dos=(void *)raw; IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    assert(nt->OptionalHeader.SizeOfImage==IMAGE_SIZE);
    uint8_t *b=VirtualAlloc(NULL,IMAGE_SIZE,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE); assert(b);
    memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *s=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        assert(s[i].PointerToRawData<=size && s[i].SizeOfRawData<=size-s[i].PointerToRawData);
        assert(s[i].VirtualAddress<=IMAGE_SIZE && s[i].SizeOfRawData<=IMAGE_SIZE-s[i].VirtualAddress);
        memcpy(b+s[i].VirtualAddress,raw+s[i].PointerToRawData,s[i].SizeOfRawData);
    }
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    assert(reloc.VirtualAddress<=IMAGE_SIZE && reloc.Size<=IMAGE_SIZE-reloc.VirtualAddress);
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    for(unsigned offset=0;offset<reloc.Size;) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(void *)(block+1); unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfff);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) { assert(rva<=IMAGE_SIZE-4); U32(b,rva)+=(uint32_t)delta; }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw); return b;
}
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    mapped=map_image(path); uint8_t *before=malloc(IMAGE_SIZE); assert(before);
    memcpy(before,mapped,IMAGE_SIZE);
    /* Unmodified native text. Executable pages contain only the selected
     * scheduler/list/scale and area entity suspend/resume routines. The fixture
     * omits sound/crate/render/weapon components, so those paths are not run. */
    static const unsigned pages[]={0x106000,0x10b000,0x134000,0x199000};
    DWORD protection[4];
    for(unsigned i=0;i<4;++i) {
        assert(VirtualProtect(mapped+pages[i],0x1000,PAGE_EXECUTE_READ,&protection[i]));
        assert(FlushInstructionCache(GetCurrentProcess(),mapped+pages[i],0x1000));
    }
    node_vtable[1]=(void *)(uintptr_t)node_update;
    node_vtable[2]=(void *)(uintptr_t)node_pause;
    node_vtable[3]=(void *)(uintptr_t)node_resume;
    node_vtable[5]=(void *)(uintptr_t)node_scale;
    update_entry=mapped+UPDATE;
    for(unsigned i=0;i<2;++i) { shared_clock(i); independent_balanced_suspension(i); }
    for(unsigned i=0;i<ENTITIES;++i) mutation_inside_native_dispatch(i);
    unscheduled_nodes(); resume_is_not_idempotent();
    PTR(mapped,CLOCK_SLOT)=PTR(before,CLOCK_SLOT); PTR(mapped,WORLD_SLOT)=PTR(before,WORLD_SLOT);
    assert(!memcmp(before,mapped,IMAGE_SIZE));
    for(unsigned i=4;i>0;--i) {
        DWORD ignored; MEMORY_BASIC_INFORMATION m;
        assert(VirtualProtect(mapped+pages[i-1],0x1000,protection[i-1],&ignored));
        assert(VirtualQuery(mapped+pages[i-1],&m,sizeof(m)) && m.Protect==protection[i-1]);
    }
    update_entry=NULL; free(before); assert(VirtualFree(mapped,0,MEM_RELEASE)); mapped=NULL;
    puts("StoryAreaSchedulerImageTest: PASS (native shared clock/list dispatch and balanced entity suspension; synthetic owners, no live area playability)");
    return 0;
}
