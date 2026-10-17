/* Exact supported EXE/GEX contracts plus synthetic task/world execution.
 * The real shared story_step observer is called with a controlled native-step
 * spy. No retail VM, gameplay, filter, loading, pause or resource code runs.
 * Script bytes are read from the owner's supplied asset, never embedded. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "engine/sha256.h"
#include "../src/hooks/lan_story_task_trace.c"
static uint8_t *mapped,*mapped_shared;
static uint8_t shared_singletons[0x1000];
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
    HANDLE mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_EXECUTE_READWRITE,0,(DWORD)image_size,NULL);
    assert(mapping);
    uint8_t *shared=MapViewOfFile(mapping,FILE_MAP_WRITE,0,0,image_size);
    assert(shared);
    uint8_t *b=shared; memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=image_size && s->SizeOfRawData<=image_size-s->VirtualAddress);
        memcpy(b+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    b=MapViewOfFile(mapping,FILE_MAP_WRITE|FILE_MAP_EXECUTE,0,0,image_size);assert(b);
    /* Reserve an image-sized address selection, then immediately remap three
     * aligned slices. Actual FILE_MAP_COPY is essential: on this Wine build
     * only changing a shared view's protection to WRITECOPY still writes its
     * shared backing. Hook code needs writable-view protection conversion. */
    assert(UnmapViewOfFile(b));
    assert(MapViewOfFileEx(mapping,FILE_MAP_WRITE|FILE_MAP_EXECUTE,0,0,0x400000,b)==b);
    assert(MapViewOfFileEx(mapping,FILE_MAP_COPY|FILE_MAP_EXECUTE,0,0x400000,0x10000,b+0x400000)==b+0x400000);
    assert(MapViewOfFileEx(mapping,FILE_MAP_WRITE|FILE_MAP_EXECUTE,0,0x410000,
        image_size-0x410000,b+0x410000)==b+0x410000);
    mapped_shared=shared;assert(CloseHandle(mapping));
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
    DWORD protection;
    /* Hook targets must be executable; singleton data stays genuine COW. */
    assert(VirtualProtect(b+0x408000,0x1000,PAGE_WRITECOPY,&protection));
    memcpy(shared_singletons,mapped_shared+0x408000,sizeof(shared_singletons));
    free(raw); return b;
}

static uint8_t engine[0x34],manager[0x1008c],thread[0x50],other[0x50];
static uint8_t world[0x3a0],group[0xd4],controller[0x24c],ui[0x64],leader[0x98];
static uint8_t descriptors[3][0x54];
static uint32_t args[16],returns[8],binding[7],binding_lookup[3];
static uint8_t *code,*functions;
static uintptr_t *function_table;
static unsigned function_count,code_size;
static const char destination[]="LNBr_Kamo_shop";
static const uint32_t destination_hash=0xc9b78499u;
static unsigned spy_calls,filter_calls;
static int spy_result;
static BOOL spy_notify,spy_wrong_controller,spy_double_notify,spy_reenter;
static uint8_t *read_file(const wchar_t *path,DWORD *size) {
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(f!=INVALID_HANDLE_VALUE);*size=GetFileSize(f,NULL);DWORD got=0;
    uint8_t *bytes=malloc(*size);assert(bytes && ReadFile(f,bytes,*size,&got,NULL) && got==*size);
    assert(CloseHandle(f));return bytes;
}
static void program(const wchar_t *path) {
    char sha[65];assert(SudekiMpSha256File(path,sha));
    assert(!strcmp(sha,"e36a5974f9aedea5b5b428fe2445cf496c52911ff01d4934ea8ab8124abf1ff9"));
    DWORD size;uint8_t *raw=read_file(path,&size);
    assert(size>20 && !memcmp(raw,"Version:0004",12));
    function_count=*(uint32_t *)(raw+16);unsigned slots=*(uint32_t *)(raw+12);
    unsigned start=20u+function_count*16u+8u+slots*4u;
    assert(start<size && function_count==6117u && slots==0x4000u);
    functions=malloc(function_count*16u);function_table=calloc(slots,4);
    assert(functions && function_table);memcpy(functions,raw+20,function_count*16u);
    for(unsigned i=0;i<function_count;++i) {
        uint32_t *f=(uint32_t *)(functions+i*16u);unsigned length=f[3]&0x7fffffffu;
        assert(f[2]<size-start && length<=size-start-f[2]);
        if(f[2]+length>code_size) code_size=f[2]+length;
        unsigned slot=f[1]&(slots-1u);
        while(function_table[slot]) slot=(slot+1u)&(slots-1u);
        function_table[slot]=(uintptr_t)f;
    }
    code=malloc(code_size);assert(code);memcpy(code,raw+start,code_size);free(raw);
    *(unsigned *)(manager+0x20)=slots;*(void **)(manager+0x24)=function_table;
    *(void **)(manager+0x28)=functions;*(unsigned *)(manager+0x2c)=function_count;
    *(unsigned *)(manager+0x30)=function_count;
    *(void **)(manager+0x34)=code;*(void **)(manager+0x38)=code+code_size;*(void **)(manager+0x3c)=code+code_size;
    *(uintptr_t *)(manager+0x6c)=(uintptr_t)mapped;
    *(unsigned *)(manager+0x70)=1;*(unsigned *)(manager+0x74)=1;
    *(void **)(manager+0x78)=binding;*(void **)(manager+0x7c)=binding_lookup;
    binding[0]=0xb9c0;binding[6]=1u<<16;binding_lookup[0]=0x91a6a1e9u;
    *(void **)(manager+0x80+(0x91a6a1e9u&0x3fffu)*4u)=binding_lookup;
    *(void **)(engine+0x30)=manager;
    *(void **)(mapped+GEL_GLOBAL)=engine;*(void **)(mapped+0x3c3108)=manager;
    *(void **)(mapped+RUNTIME_GLOBAL)=manager+0x20;
    load_manager=manager;status.load_generation=17;
    assert(interior_program_exact(manager,FALSE));
}
static void fixture(void) {
    interior_clear("fixture_reset");interior_foreign=0;trace_fault=0;interior_step_scope=NULL;
    memset(world,0,sizeof(world));memset(group,0,sizeof(group));memset(controller,0,sizeof(controller));
    memset(ui,0,sizeof(ui));memset(descriptors,0,sizeof(descriptors));memset(thread,0,sizeof(thread));
    memset(other,0,sizeof(other));memset(tasks,0,sizeof(tasks));
    memset(args,0,sizeof(args));memset(returns,0,sizeof(returns));
    *(void **)(mapped+0x408d10)=world;*(void **)(mapped+GROUP_GLOBAL)=group;
    *(void **)(mapped+0x408da4)=controller;*(void **)(mapped+0x408d1c)=ui;
    *(void **)world=mapped+0x2c4c3c;world[0x39a]=1; /* Ordinary state3 may have ready byte0. */
    *(void **)(world+0xc)=descriptors[0];*(void **)(world+0x50)=descriptors;
    *(unsigned *)(world+0x54)=2;
    for(unsigned i=0;i<3;++i) *(void **)descriptors[i]=mapped+0x2c82a4;
    *(const char **)(descriptors[0]+0x24)="NewBrightwater";
    *(const char **)(descriptors[1]+0x24)=destination;
    *(const char **)(descriptors[2]+0x24)="other";
    *(unsigned *)(descriptors[0]+0x34)=3;*(void **)(descriptors[0]+0x14)=leader;
    *(void **)controller=mapped+0x2c9f5c;*(void **)(controller+0x2c)=mapped+0x2c9f84;
    *(void **)(controller+0x248)=leader;*(void **)(group+0x90)=leader;*(unsigned *)(group+0xcc)=2;
    *(void **)ui=mapped+0x2c5210;
    *(unsigned *)(thread+0xc)=0x3171a;*(unsigned *)(thread+0x34)=0x5bca8fb4u;
    *(unsigned *)(thread+0x10)=1;*(unsigned *)(thread+0x14)=8;*(void **)(thread+0x18)=returns;
    returns[0]=0x31602; /* Actual generic caller's compiled-call return PC. */
    *(void **)(thread+0x1c)=args;*(void **)(thread+0x20)=args+11;
    *(unsigned *)(thread+0x24)=16;*(unsigned *)(thread+0x28)=5;
    args[11]=0xebf4131au;args[12]=1;args[13]=0x20a8f277;args[14]=0x622;args[15]=destination_hash;
    Task *t=find_task(thread,TRUE);assert(t);
    *t=(Task){.thread=thread,.manager=manager,.id=71,.hash=0xe8ff6c6f,
        .generation=17,.occupied=TRUE,.used=TRUE};
    spy_result=0;spy_notify=TRUE;spy_wrong_controller=spy_double_notify=spy_reenter=FALSE;
    callbacks=0;current_thread=NULL;
}
static int __attribute__((fastcall)) step_spy(void *value,void *unused) {
    (void)unused;++spy_calls;
    uint8_t *th=value;unsigned pc=*(unsigned *)(th+0xc);
    assert(callbacks && current_thread==value);
    SudekiMpLanStoryInteriorTravel denied;
    assert(!SudekiMpLanStoryTaskInteriorTravel(&denied)); /* No authority within callback. */
    if(spy_result) return spy_result;
    if(pc==0x3176c) { ui[0x60]=1;*(unsigned *)(th+0xc)=pc+5u; }
    else if(pc==0x5856) {
        if(spy_reenter) {
            *(unsigned *)(other+0xc)=0x3176c;
            assert(story_step(other,NULL)==0);
        }
        ++filter_calls;
        *(unsigned *)(controller+0x80)=0;*(unsigned *)(controller+0x84)=0;
        if(spy_notify) SudekiMpLanStoryTaskInteriorFilterObserved(spy_wrong_controller?world:controller,0);
        if(spy_double_notify) SudekiMpLanStoryTaskInteriorFilterObserved(controller,0);
        *(unsigned *)(th+0xc)=pc+5u;
    } else *(unsigned *)(th+0xc)=pc+1u;
    SetLastError(123);return 0;
}
static void step(unsigned pc) {
    *(unsigned *)(thread+0xc)=pc;SetLastError(42);
    assert(story_step(thread,NULL)==spy_result);
    assert(!callbacks && !current_thread && !interior_step_scope);
    assert(GetLastError()==(spy_result?42u:123u));
}
static void tsa(void) {
    args[11]=1;step(0x3176c);
}
static void disable(void) {
    *(unsigned *)(thread+0x10)=2;returns[1]=0x31822;step(0x5856);
}
static void arm(void) {
    fixture();step(0x3171a);assert(interior.candidate && !interior.disabled);
    tsa();assert(interior.tsa);disable();assert(interior.disabled);
}
static BOOL proof(void) {
    SudekiMpLanStoryInteriorTravel value;
    memset(&value,0xa5,sizeof(value));SudekiMpLanStoryInteriorTravel before=value;
    BOOL ok=SudekiMpLanStoryTaskInteriorTravel(&value);
    if(ok) {
        assert(value.task==71 && value.load_generation==17 && value.destination_hash==destination_hash);
        assert(value.world==world && value.descriptor==descriptors[0] && value.group==group);
        assert(value.controller==controller && value.leader==leader && value.destination==descriptors[1]);
        assert(!strcmp(value.destination_name,destination));
    } else assert(!memcmp(&value,&before,sizeof(value)));
    return ok;
}
static void native_contracts(void) {
    static const char *names[]={"StepOneOpcode","CompiledCallPushReturnScope","ReturnPopScope",
        "PushArgumentStack","UIStartTSAFlag","UIInTSAFlag"};
    assert(interior_native_exact());
    for(unsigned n=0;n<sizeof(interior_native_bodies)/sizeof(*interior_native_bodies);++n) {
        const InteriorBody *b=&interior_native_bodies[n];
        for(unsigned i=0;i<b->size;++i) {
            mapped[b->rva+i]^=1;assert(!interior_native_exact());mapped[b->rva+i]^=1;
        }
        assert(interior_native_exact());printf("InteriorTravel.%s: PASS (supported image, every byte refusal)\n",names[n]);
    }
    for(unsigned n=0;n<sizeof(interior_scripts)/sizeof(*interior_scripts);++n) {
        const InteriorScript *s=&interior_scripts[n];
        for(unsigned i=0;i<s->size;++i) {
            code[s->offset+i]^=1;assert(!interior_program_exact(manager,FALSE));code[s->offset+i]^=1;
        }
        printf("InteriorTravel.Script_%08lx: PASS (supported asset, every byte refusal)\n",(unsigned long)s->hash);
    }
}
static DWORD WINAPI foreign_filter(void *unused) {
    (void)unused;SudekiMpLanStoryTaskInteriorFilterObserved(controller,0);return 0;
}
static void cases(void) {
    arm();assert(proof());
    *(unsigned *)(controller+0x80)=1;assert(proof()); /* Native pending switch has not applied yet. */
    uint8_t world_before[sizeof(world)],group_before[sizeof(group)],actor_before[sizeof(leader)];
    memcpy(world_before,world,sizeof(world));memcpy(group_before,group,sizeof(group));memcpy(actor_before,leader,sizeof(leader));
    for(unsigned i=0;i<3;++i) assert(proof());
    assert(!memcmp(world_before,world,sizeof(world)) && !memcmp(group_before,group,sizeof(group)) &&
        !memcmp(actor_before,leader,sizeof(leader)));
    *(unsigned *)(thread+0x10)=1;*(unsigned *)(thread+0xc)=0x31822;assert(proof());
    *(unsigned *)(thread+0x10)=2;returns[1]=0x31844;*(unsigned *)(thread+0xc)=0x316a0;assert(proof());
    *(unsigned *)(thread+0x10)=1;step(0x31871);assert(!proof() && !interior.candidate);
    puts("InteriorTravel.OwnDisableThenPreTempOnly: PASS (synthetic observer execution)");

    fixture();ui[0x60]=1;step(0x3171a);assert(!interior.candidate);
    fixture();args[12]=0;step(0x3171a);assert(!interior.candidate);
    fixture();args[15]=0x1234;step(0x3171a);assert(!interior.candidate);
    fixture();*(unsigned *)(descriptors[1]+0x34)=3;step(0x3171a);assert(!interior.candidate);
    fixture();*(unsigned *)(descriptors[0]+0x34)=4;step(0x3171a);assert(!interior.candidate);
    fixture();*(void **)(world+0x14)=descriptors[1];step(0x3171a);assert(!interior.candidate);
    fixture();*(unsigned *)(world+0x54)=3;*(const char **)(descriptors[2]+0x24)=destination;
    step(0x3171a);assert(!interior.candidate);
    fixture();returns[0]--;step(0x3171a);assert(!interior.candidate);
    fixture();step(0x3171a);args[11]=0;step(0x3176c);assert(!interior.candidate);
    puts("InteriorTravel.ExistingTsaDestinationAndArgumentsRefuse: PASS");

    fixture();step(0x3171a);tsa();spy_notify=FALSE;disable();assert(!proof());
    fixture();step(0x3171a);tsa();spy_wrong_controller=TRUE;disable();assert(!proof());
    fixture();step(0x3171a);tsa();spy_double_notify=TRUE;disable();assert(!proof());
    fixture();step(0x3171a);tsa();spy_reenter=TRUE;disable();assert(!proof());
    fixture();step(0x3171a);tsa();spy_result=2;disable();assert(!proof() && interior.candidate);
    spy_result=0;disable();assert(proof());
    arm();SudekiMpLanStoryTaskInteriorFilterObserved(controller,1);assert(!proof());
    arm();SudekiMpLanStoryTaskInteriorFilterObserved(controller,0);assert(!proof());
    arm();HANDLE worker=CreateThread(NULL,0,foreign_filter,NULL,0,NULL);assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);assert(CloseHandle(worker));assert(!proof());
    puts("InteriorTravel.NativeFilterCompletionAndForeignOwnerRefuse: PASS");

    arm();find_task(thread,FALSE)->id++;assert(!proof());
    arm();find_task(thread,FALSE)->generation++;assert(!proof());
    arm();find_task(thread,FALSE)->retirement_entered=TRUE;assert(!proof());
    arm();find_task(thread,FALSE)->terminal=TRUE;assert(!proof());
    arm();returns[0]++;assert(!proof());
    arm();*(unsigned *)(thread+0x10)=0;assert(!proof());
    arm();status.load_generation++;assert(!proof());status.load_generation--;
    arm();*(void **)(manager+0x34)=code+1;assert(!proof());*(void **)(manager+0x34)=code;
    arm();code[0x317be]^=1;assert(!proof());code[0x317be]^=1;
    arm();ui[0x60]=0;assert(!proof());
    arm();returns[1]=0x31844;*(unsigned *)(thread+0xc)=0x40000;assert(!proof());
    arm();*(unsigned *)(controller+0x84)=1;assert(!proof());
    arm();*(unsigned *)(controller+0x84)=2;assert(!proof());
    arm();*(unsigned *)(controller+0x80)=2;assert(!proof());
    arm();*(void **)(controller+0x248)=world;assert(!proof());
    arm();*(void **)(world+0x14)=descriptors[1];assert(!proof());
    arm();*(unsigned *)(descriptors[1]+0x34)=1;assert(!proof());
    arm();*(const char **)(descriptors[1]+0x24)="different";assert(!proof());
    arm();*(unsigned *)(other+0xc)=0x3176c;assert(story_step(other,NULL)==0);assert(!proof());
    puts("InteriorTravel.StaleTaskProgramFrameWorldAndTsaRefuse: PASS");

    /* H1b: a destination background-resident for a co-op visit (state 2)
     * arms like an unloaded one; it may unload (2 -> 0) mid-proof; loading,
     * current and unloading destinations are still refused. */
    fixture();*(unsigned *)(descriptors[1]+0x34)=2;step(0x3171a);assert(interior.candidate);
    arm();*(unsigned *)(descriptors[1]+0x34)=2;assert(proof());
    *(unsigned *)(descriptors[1]+0x34)=0;assert(proof());
    *(unsigned *)(descriptors[1]+0x34)=1;assert(!proof());
    arm();*(unsigned *)(descriptors[1]+0x34)=4;assert(!proof());
    fixture();*(unsigned *)(descriptors[1]+0x34)=1;step(0x3171a);assert(!interior.candidate);
    fixture();*(unsigned *)(descriptors[1]+0x34)=4;step(0x3171a);assert(!interior.candidate);
    puts("InteriorTravel.ResidentDestinationArms: PASS");
}
static void benchmark(void) {
    LARGE_INTEGER frequency,start,end;assert(QueryPerformanceFrequency(&frequency));
    fixture();*(unsigned *)(thread+0xc)=1;
    InteriorStep observation;
    assert(QueryPerformanceCounter(&start));
    for(unsigned i=0;i<100000u;++i) interior_step_begin(thread,&observation);
    assert(QueryPerformanceCounter(&end));
    printf("InteriorTravel.BenchOrdinaryStep: %.3f us/call (100000 calls, no candidate)\n",
        (double)(end.QuadPart-start.QuadPart)*1e6/(double)frequency.QuadPart/100000.0);
    for(unsigned count=2;count<=64;count*=32) {
        uint8_t *catalog=calloc(count,0x54);char (*names)[48]=calloc(count,48);assert(catalog && names);
        fixture();memcpy(catalog,descriptors,sizeof(descriptors[0])*2u);
        for(unsigned i=2;i<count;++i) {
            *(void **)(catalog+i*0x54)=mapped+0x2c82a4;
            snprintf(names[i],48,"fixture_room_%03u",i);
            *(void **)(catalog+i*0x54+0x24)=names[i];
        }
        *(void **)(world+0xc)=catalog;*(void **)(world+0x50)=catalog;*(unsigned *)(world+0x54)=count;
        step(0x3171a);tsa();disable();
        SudekiMpLanStoryInteriorTravel v;assert(SudekiMpLanStoryTaskInteriorTravel(&v));
        assert(QueryPerformanceCounter(&start));
        for(unsigned i=0;i<10000u;++i) assert(SudekiMpLanStoryTaskInteriorTravel(&v));
        assert(QueryPerformanceCounter(&end));
        printf("InteriorTravel.BenchActiveGet: %.3f us/call (%u descriptors, 10000 calls)\n",
            (double)(end.QuadPart-start.QuadPart)*1e6/(double)frequency.QuadPart/10000.0,count);
        interior_clear("benchmark_done");free(names);free(catalog);
    }
}
int main(int argc,char **argv) {
    assert(argc==3 || (argc==4 && !strcmp(argv[3],"--bench")));wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));mapped=map_image(path);
    BOOL installed_ok=SudekiMpLanStoryTaskTraceInstall((HMODULE)mapped);
    if(!installed_ok) {
        DWORD failure=GetLastError();MEMORY_BASIC_INFORMATION q;
        assert(VirtualQuery(mapped+RETIRE,&q,sizeof(q))==sizeof(q));
        fprintf(stderr,"InteriorTravel.InstallFailure: error=%lu loaded=%u spawn_body=%u code_protect=%lx code_type=%lx globals=%p/%u/%ld\n",
            (unsigned long)failure,SudekiMpCheckLoadedExecutable((HMODULE)mapped),spawn_destroy_body_exact(mapped),
            (unsigned long)q.Protect,(unsigned long)q.Type,image_base,installed,(long)callbacks);
    }
    assert(installed_ok);
    assert(!native_thread);SudekiMpLanStoryTaskInteriorFilterObserved(NULL,0);
    assert(!interior_foreign);native_thread=GetCurrentThreadId();
    puts("InteriorTravel.PreOwnerFrontendFilterIgnored: PASS");
    assert(SudekiMpLanStoryTaskHostExact((HMODULE)mapped));
    assert(MultiByteToWideChar(CP_UTF8,0,argv[2],-1,path,1024));program(path);
    native_contracts();original_step=step_spy;cases();
    MEMORY_BASIC_INFORMATION memory;
    assert(VirtualQuery(mapped+0x408d10,&memory,sizeof(memory))==sizeof(memory));
    assert((memory.Protect&0xffu)==PAGE_WRITECOPY && memory.Type==MEM_MAPPED);
    arm();assert(proof());
    assert(!memcmp(shared_singletons,mapped_shared+0x408000,sizeof(shared_singletons)));
    assert(*(void **)(mapped+0x408d10)!=*(void **)(mapped_shared+0x408d10));
    puts("InteriorTravel.CopyOnWriteImageSingletons: PASS (FILE_MAP_COPY singleton slice, shared view unchanged)");
    if(argc==4) benchmark();
    assert(SudekiMpLanStoryTaskTraceUninstall());assert(!interior.candidate);
    assert(UnmapViewOfFile(mapped+0x410000));assert(UnmapViewOfFile(mapped+0x400000));
    assert(UnmapViewOfFile(mapped));assert(UnmapViewOfFile(mapped_shared));free(code);free(functions);free(function_table);
    printf("InteriorTravel: PASS (%u synthetic steps, %u synthetic native filter returns; no gameplay)\n",spy_calls,filter_calls);
    return 0;
}
