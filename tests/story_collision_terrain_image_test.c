/* Supported-image native movement candidate/triangle-query experiment.
 * Uses real query/filter/list traversal/distance/hit-storage code, invented
 * triangle data and grid owners, synthetic lifetime events, and small matrix
 * import substitutes. Native removal/re-enrollment executes against fixtures.
 * No running game, real terrain loading, movement solver,
 * native resource keepalive or gameplay is established by this fixture. */
#include "hooks/lan_story_collision_query.h"
#include "engine/build_identity.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "The supported terrain experiment requires x86 GCC"
#endif
void TestStoryCollisionBorn(void *);
void TestStoryCollisionDeleted(void *);
enum { IMAGE_SIZE=0x45f000, SOURCES=503, QUERY=0x1c90a0, TRANSFORM_IMPORT=0x29a2b4,
    NORMAL_IMPORT=0x29a2bc, COLLISION_SYSTEM=0x408dd4,
    LEVELS=0x362638, CANDIDATES=0x362660, GEOMETRY_QUEUE=0x362ef0 };
#define PTR(b,o) (*(void **)((uint8_t *)(b)+(o)))
#define U32(b,o) (*(uint32_t *)((uint8_t *)(b)+(o)))
#define U16(b,o) (*(uint16_t *)((uint8_t *)(b)+(o)))
#define F32(b,o) (*(float *)((uint8_t *)(b)+(o)))
typedef uint32_t (__stdcall *Query)(void *,const float *,float,void *,void *);
typedef struct Terrain {
    uint8_t model[0x50],leaf[0x18];
    float vertices[9]; uint16_t primitive[5],index;
} Terrain;
static uint8_t *mapped,grid[0x15b0],sources[SOURCES][0x110];
static void *grid_items[SOURCES];
static uint8_t scratch[0x10c],pairs[4*0x10],output[0x20],contacts[8][0x60];
static void *contact_slots[8];
static Terrain floors[2];
static SudekiMpStoryAreas policy;
static SudekiMpLanStoryCollisionQuerySet set;
static SudekiMpLanStoryCollisionSourceArea entries[SOURCES];
static unsigned born,transform_calls;
static void __attribute__((noinline)) native_math_begin(void *saved) {
    /* This mapped image has no CRT startup/error-handler installation. Use
     * the native math wrapper's standard masked, 53-bit x87 environment.
     * Testing other control modes or CRT error handling is out of scope. */
    const uint16_t control=0x027f;
    __asm__ volatile("fxsave (%0); fninit; fldcw %1" :: "r"(saved),"m"(control) : "memory");
}
static void __attribute__((noinline)) native_math_end(const void *saved) {
    __asm__ volatile("fxrstor (%0)" :: "r"(saved) : "memory");
}
static LONG CALLBACK native_fault(EXCEPTION_POINTERS *e) {
    uintptr_t pc=(uintptr_t)e->ExceptionRecord->ExceptionAddress;
    fprintf(stderr,"native fixture exception: code=%08lx mapped-rva=%08lx operation=%lu address=%08lx\n",
        (unsigned long)e->ExceptionRecord->ExceptionCode,
        (unsigned long)(pc-(uintptr_t)mapped),
        (unsigned long)(e->ExceptionRecord->NumberParameters>0?
            e->ExceptionRecord->ExceptionInformation[0]:0),
        (unsigned long)(e->ExceptionRecord->NumberParameters>1?
            e->ExceptionRecord->ExceptionInformation[1]:0));
    fflush(stderr); return EXCEPTION_CONTINUE_SEARCH;
}
static float position[3];
static void *grid_init_entry __attribute__((used)),*suspend_entry __attribute__((used)),
    *resume_entry __attribute__((used));
static uint8_t system_data[0x8c],descriptors[2][0x54],resources[2][0x130],empty_spawns[0x20];
static void *registered_sources[8];
static void __attribute__((naked,noinline)) grid_initialize(void *g __attribute__((unused))) {
    __asm__ volatile("push %esi; mov 8(%esp),%esi; push $0x40000000;"
        "push $0x41800000; call *_grid_init_entry; pop %esi; ret");
}
static void __attribute__((naked,noinline)) suspend_terrain(void *d __attribute__((unused))) {
    __asm__ volatile("mov 4(%esp),%eax; jmp *_suspend_entry");
}
static void __attribute__((naked,noinline)) resume_terrain(void *d __attribute__((unused))) {
    __asm__ volatile("push %edi; mov 8(%esp),%edi; call *_resume_entry; pop %edi; ret");
}
static float *__attribute__((stdcall,force_align_arg_pointer)) transform_normal(float *out,
    const float *in,const float *m) {
    float x=in[0],y=in[1],z=in[2];
    out[0]=x*m[0]+y*m[4]+z*m[8]; out[1]=x*m[1]+y*m[5]+z*m[9];
    out[2]=x*m[2]+y*m[6]+z*m[10]; return out;
}
static float *__attribute__((stdcall,force_align_arg_pointer)) transform(float *out,
    const float *in,const float *m) {
    float x=in[0],y=in[1],z=in[2];
    float w=x*m[3]+y*m[7]+z*m[11]+m[15];
    assert(isfinite(w) && w!=0.0f);
    out[0]=(x*m[0]+y*m[4]+z*m[8]+m[12])/w;
    out[1]=(x*m[1]+y*m[5]+z*m[9]+m[13])/w;
    out[2]=(x*m[2]+y*m[6]+z*m[10]+m[14])/w;
    ++transform_calls; return out;
}
static void identity(void *p) {
    memset(p,0,64); float *m=p; m[0]=m[5]=m[10]=m[15]=1.0f;
}
static void terrain(unsigned i,float y) {
    Terrain *t=&floors[i];
    memset(t,0,sizeof(*t));
    const float v[9]={-4,y,-4,4,y,-4,0,y,4}; memcpy(t->vertices,v,sizeof(v));
    U16(t->leaf,0)=3; U16(t->leaf,2)=1; F32(t->leaf,4)=10.0f;
    U32(t->leaf,20)=0;
    t->primitive[0]=4; t->primitive[1]=0; t->primitive[2]=1; t->primitive[3]=2;
    t->primitive[4]=0x0800u+(uint16_t)i;
    PTR(t->model,0x2c)=t->leaf; PTR(t->model,0x30)=t->vertices;
    PTR(t->model,0x38)=t->primitive; PTR(t->model,0x40)=&t->index;
}
static void refresh_set(unsigned count) {
    set.count=count; set.grid=grid; set.sources=entries;
    PTR(grid,0x15a0)=grid_items; U32(grid,0x15a4)=count; U32(grid,0x15a8)=SOURCES;
    for(unsigned i=0;i<count;++i) {
        grid_items[i]=sources[i]; entries[i].source=sources[i];
        SudekiMpLanStoryCollisionStamp stamp;
        assert(SudekiMpLanStoryCollisionLifetimeObserve((HMODULE)mapped,sources[i],&stamp));
        entries[i].incarnation=stamp.incarnation; set.lifetime_revision=stamp.revision;
    }
}
static void fixture(unsigned count) {
    for(unsigned i=0;i<born;++i) TestStoryCollisionDeleted(sources[i]);
    born=0;
    memset(grid,0,sizeof(grid)); memset(sources,0,sizeof(sources));
    grid_initialize(grid);
    memset(scratch,0,sizeof(scratch)); memset(pairs,0,sizeof(pairs));
    memset(output,0,sizeof(output)); memset(contacts,0,sizeof(contacts));
    memset(&policy,0,sizeof(policy)); memset(&set,0,sizeof(set)); memset(entries,0,sizeof(entries));
    assert(SudekiMpStoryAreasInitialize(&policy,73));
    assert(SudekiMpStoryAreaLoad(&policy,"fixture_world","",&set.areas[0]));
    assert(SudekiMpStoryAreaReady(&policy,set.areas[0]));
    assert(SudekiMpStoryAreaLoad(&policy,"fixture_world","fixture_room",&set.areas[1]));
    assert(SudekiMpStoryAreaReady(&policy,set.areas[1]));
    terrain(0,0); terrain(1,0.25f);
    PTR(grid,0x15ac)=scratch; PTR(scratch,0x100)=pairs; U32(scratch,0x104)=4;
    for(unsigned i=0;i<8;++i) contact_slots[i]=contacts[i];
    PTR(output,4)=contact_slots;
    for(unsigned i=0;i<count;++i) {
        PTR(sources[i],8)=grid; sources[i][0xf2]=1; sources[i][0xf4]=1;
        sources[i][0xf5]=1; F32(sources[i],0xc)=F32(sources[i],0x2c)=10.0f;
        U32(sources[i],0x44)=0x20000; U32(sources[i],0x48)=4;
        identity(sources[i]+0x60); identity(sources[i]+0xa0);
        TestStoryCollisionBorn(sources[i]); ++born;
    }
    refresh_set(count);
    position[0]=position[2]=0; position[1]=0.5f; transform_calls=0;
}
static void chain(BOOL reverse) {
    PTR(grid,0x1c)=sources[reverse?set.count-1:0];
    for(unsigned i=0;i<set.count;++i) {
        unsigned next=reverse?i-1:i+1;
        PTR(sources[i],0)=next<set.count?sources[next]:NULL;
        PTR(sources[i],4)=(!reverse && i)?sources[i-1]:
            (reverse && i+1<set.count)?sources[i+1]:(void *)(grid+0x1c);
    }
}
static void prepare_basic(BOOL reverse) {
    fixture(4);
    PTR(sources[0],0x4c)=floors[0].model; PTR(sources[1],0x4c)=floors[1].model;
    entries[0].area_slot=entries[2].area_slot=0;
    entries[1].area_slot=entries[3].area_slot=1;
    for(unsigned i=2;i<4;++i) {
        U32(sources[i],0x44)=4; U32(sources[i],0x48)=0x20004;
        sources[i][0xf4]=0; /* Query body does not become somebody else's floor. */
    }
    chain(reverse);
}
static uint32_t raw(unsigned source) {
    return ((Query)(uintptr_t)(mapped+QUERY))(grid,position,1.0f,sources[source],output);
}
static void own_floor(unsigned area) {
    uint32_t hits=UINT32_MAX;
    assert(SudekiMpLanStoryCollisionQueryRun(&policy,&set,set.areas[area],sources[area+2],
        position,1.0f,output,&hits)==SUDEKIMP_STORY_COLLISION_COMPLETE);
    assert(hits==1 && U32(output,0x10)==1 && U32(grid,0x10)==2);
    assert(PTR(contacts[0],0x30)==sources[area] && U32(contacts[0],0x20)==1);
    for(unsigned v=0;v<3;++v) assert(F32(contacts[0],0x38+12*v)==(area?0.25f:0.0f));
    assert(transform_calls);
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_PINS;++i) assert(!policy.pins[i].serial);
}
static void native_floors(void) {
    for(unsigned order=0;order<2;++order) {
        prepare_basic(order!=0);
        assert(raw(2)==2 && U32(grid,0x10)==4);
        assert(PTR(contacts[0],0x30)!=PTR(contacts[1],0x30));
        for(unsigned i=0;i<12;++i) { own_floor(i%2); }
        /* A legitimate empty result after complete geometry enumeration is
         * distinct from incomplete query status. The other area is not a
         * fallback floor even when its overlapping terrain remains enabled. */
        sources[0][0xf4]=0;
        uint32_t hits=123;
        assert(SudekiMpLanStoryCollisionQueryRun(&policy,&set,set.areas[0],sources[2],
            position,1.0f,output,&hits)==SUDEKIMP_STORY_COLLISION_COMPLETE && !hits);
        own_floor(1); sources[0][0xf4]=1; own_floor(0);
        position[1]=8.0f;
        hits=123;
        assert(SudekiMpLanStoryCollisionQueryRun(&policy,&set,set.areas[0],sources[2],
            position,1.0f,output,&hits)==SUDEKIMP_STORY_COLLISION_COMPLETE && !hits);
        /* This miss passes through native leaf and triangle distance tests. */
    }
}
static void native_capacity(void) {
    fixture(SOURCES);
    for(unsigned i=0;i<500;++i) { entries[i].area_slot=1; sources[i][0xf4]=0; }
    entries[500].area_slot=entries[501].area_slot=0; entries[502].area_slot=1;
    PTR(sources[500],0x4c)=floors[0].model;
    for(unsigned i=501;i<503;++i) {
        U32(sources[i],0x44)=4; U32(sources[i],0x48)=0x20004; sources[i][0xf4]=0;
    }
    chain(FALSE);
    assert(raw(501)==0 && U32(grid,0x10)==500); /* own terrain was starved */
    uint32_t hits=UINT32_MAX;
    assert(SudekiMpLanStoryCollisionQueryRun(&policy,&set,set.areas[0],sources[501],
        position,1.0f,output,&hits)==SUDEKIMP_STORY_COLLISION_COMPLETE);
    assert(hits==1 && U32(grid,0x10)==2 && PTR(contacts[0],0x30)==sources[500]);
    hits=UINT32_MAX;
    assert(SudekiMpLanStoryCollisionQueryRun(&policy,&set,set.areas[1],sources[502],
        position,1.0f,output,&hits)==SUDEKIMP_STORY_COLLISION_INCOMPLETE);
    assert(hits==UINT32_MAX && U32(grid,0x10)==500);
    /* A native saturated zero-hit buffer is not published as absent terrain. */
}
static void capture_grid_enrollment(void) {
    /* Rebuild only copied fixture ownership, never repair native membership. */
    unsigned count=U32(grid,0x15a4),published=0;
    assert(count<=4);
    for(unsigned i=0;i<4;++i) {
        unsigned matches=0;
        for(unsigned j=0;j<count;++j) if(grid_items[j]==sources[i]) ++matches;
        assert(matches<=1);
        if(!matches) continue;
        SudekiMpLanStoryCollisionStamp stamp;
        assert(SudekiMpLanStoryCollisionLifetimeObserve((HMODULE)mapped,sources[i],&stamp));
        entries[published].source=sources[i]; entries[published].area_slot=i%2;
        entries[published].incarnation=stamp.incarnation;
        set.lifetime_revision=stamp.revision; ++published;
    }
    assert(published==count); set.count=count;
}
static void native_terrain_enrollment(void) {
    for(unsigned first=0;first<2;++first) {
        prepare_basic(first!=0);
        memset(system_data,0,sizeof(system_data)); memset(descriptors,0,sizeof(descriptors));
        memset(resources,0,sizeof(resources)); memset(empty_spawns,0,sizeof(empty_spawns));
        memset(registered_sources,0,sizeof(registered_sources));
        PTR(system_data,0x30)=grid; U32(system_data,0x80)=4; U32(system_data,0x84)=8;
        PTR(system_data,0x88)=registered_sources;
        PTR(mapped,COLLISION_SYSTEM)=system_data;
        for(unsigned i=0;i<4;++i) registered_sources[i]=sources[i];
        for(unsigned a=0;a<2;++a) {
            PTR(descriptors[a],0x14)=resources[a]; U32(descriptors[a],0x34)=a?4:3;
            PTR(resources[a],0x2c)=sources[a]; PTR(resources[a],0x118)=empty_spawns;
        }
        uint8_t descriptor_before[sizeof(descriptors)],resource_before[sizeof(resources)];
        memcpy(descriptor_before,descriptors,sizeof(descriptors));
        memcpy(resource_before,resources,sizeof(resources));
        for(unsigned n=0;n<16;++n) {
            unsigned a=(n+first)%2; own_floor(0); own_floor(1);
            suspend_terrain(descriptors[a]);
            assert(!sources[a][0xf2] && !sources[a][0xf4] && !U32(sources[a],0x44));
            assert(U32(sources[a],0x10c)==0x20000);
            assert(U32(system_data,0x80)==3 && U32(grid,0x15a4)==3);
            uint32_t hits=123; uint8_t prior_output[sizeof(output)];
            memcpy(prior_output,output,sizeof(output));
            assert(SudekiMpLanStoryCollisionQueryRun(&policy,&set,set.areas[a],sources[a+2],
                position,1.0f,output,&hits)==SUDEKIMP_STORY_COLLISION_NOT_RUN);
            assert(hits==123 && !memcmp(prior_output,output,sizeof(output)));
            capture_grid_enrollment();
            assert(SudekiMpLanStoryCollisionQueryRun(&policy,&set,set.areas[a],sources[a+2],
                position,1.0f,output,&hits)==SUDEKIMP_STORY_COLLISION_COMPLETE && !hits);
            own_floor(1-a);
            /* The floor was removed even though descriptor and resource bytes
             * are intact. Residency alone does not preserve floor collision. */
            assert(!memcmp(descriptor_before,descriptors,sizeof(descriptors)));
            assert(!memcmp(resource_before,resources,sizeof(resources)));
            resume_terrain(descriptors[a]);
            assert(sources[a][0xf2]==1 && sources[a][0xf4]==1 && U32(sources[a],0x44)==0x20000);
            assert(U32(system_data,0x80)==4 && U32(grid,0x15a4)==4);
            assert(PTR(grid,0x18)==sources[a]); /* native pending grid placement */
            capture_grid_enrollment(); own_floor(a);
            assert(!PTR(grid,0x18) && !sources[a][0x1c]);
            own_floor(1-a);
        }
    }
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
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    void *exception_handler=AddVectoredExceptionHandler(1,native_fault); assert(exception_handler);
    mapped=map_image(path); uint8_t *before=malloc(IMAGE_SIZE); assert(before);
    memcpy(before,mapped,IMAGE_SIZE);
    assert(SudekiMpLanStoryCollisionLifetimeInstall((HMODULE)mapped));
    assert(SudekiMpLanStoryCollisionQueryInstall((HMODULE)mapped));
    static const unsigned pages[]={0x31000,0x33000,0x38000,0x70000,0x10b000,0x110000,
        0x1b9000,0x1c8000,0x1c9000,0x1cb000,0x1cd000,0x1ce000,0x24d000,0x28f000,
        0x290000}; /* sqrt wrapper's finite-value classification helper */
    DWORD protection[sizeof(pages)/sizeof(pages[0])];
    for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);++i) {
        assert(VirtualProtect(mapped+pages[i],0x1000,PAGE_EXECUTE_READ,&protection[i]));
        assert(FlushInstructionCache(GetCurrentProcess(),mapped+pages[i],0x1000));
    }
    assert(U32(mapped,0x412c80)==0); /* native CRT scalar memset path */
    PTR(mapped,TRANSFORM_IMPORT)=(void *)(uintptr_t)transform;
    PTR(mapped,NORMAL_IMPORT)=(void *)(uintptr_t)transform_normal;
    grid_init_entry=mapped+0x1c8430; suspend_entry=mapped+0x10b500; resume_entry=mapped+0x10b4b0;
    uint8_t math_before[512] __attribute__((aligned(16)));
    native_math_begin(math_before);
    puts("native terrain: overlapping floors"); fflush(stdout); native_floors();
    puts("native terrain: candidate saturation"); fflush(stdout); native_capacity();
    puts("native terrain: removal/re-enrollment"); fflush(stdout); native_terrain_enrollment();
    native_math_end(math_before);
    uint16_t math_control;
    __asm__ volatile("fnstcw %0" : "=m"(math_control));
    assert(math_control==U16(math_before,0));
    assert(SudekiMpLanStoryCollisionQueryUninstall());
    for(unsigned i=0;i<born;++i) TestStoryCollisionDeleted(sources[i]);
    assert(SudekiMpLanStoryCollisionLifetimeUninstall());
    memcpy(mapped+TRANSFORM_IMPORT,before+TRANSFORM_IMPORT,4);
    memcpy(mapped+NORMAL_IMPORT,before+NORMAL_IMPORT,4);
    memcpy(mapped+COLLISION_SYSTEM,before+COLLISION_SYSTEM,4);
    memcpy(mapped+LEVELS,before+LEVELS,7*4);
    memcpy(mapped+CANDIDATES,before+CANDIDATES,500*4);
    memcpy(mapped+GEOMETRY_QUEUE,before+GEOMETRY_QUEUE,1000*4);
    assert(!memcmp(before,mapped,IMAGE_SIZE));
    for(unsigned i=sizeof(pages)/sizeof(pages[0]);i>0;--i) {
        DWORD ignored; MEMORY_BASIC_INFORMATION m;
        assert(VirtualProtect(mapped+pages[i-1],0x1000,protection[i-1],&ignored));
        assert(VirtualQuery(mapped+pages[i-1],&m,sizeof(m)) && m.Protect==protection[i-1]);
    }
    free(before); assert(VirtualFree(mapped,0,MEM_RELEASE)); mapped=NULL;
    assert(RemoveVectoredExceptionHandler(exception_handler));
    puts("StoryCollisionTerrainImageTest: PASS (native scoped triangles and terrain removal/re-enrollment; synthetic geometry/owners, no gameplay or native retention)");
    return 0;
}
