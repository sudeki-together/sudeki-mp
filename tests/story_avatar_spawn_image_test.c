/* Exact supported-image hook installation and real constructor execution.
 * Completion/destruction bodies and actors are fixtures: this proves shared
 * observer dispatch and identity isolation, not native gameplay readiness. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/hooks/lan_story_task_trace.c"
#include "hooks/lan_story_avatar_spawn.h"
#include "hooks/lan_story_avatar_party.h"
static uint8_t *mapped;
static size_t image_size;
static unsigned exit_status;
void SudekiMpLogWrite(const char *s) { (void)s; }
void SudekiMpLogFormat(const char *s,...) { (void)s; }
unsigned SudekiMpLobbyGameplayStoryExitStatus(void) { return exit_status; }
BOOL SudekiMpSpiritInstanceFilterNoneEntryExact(HMODULE image) { (void)image; return FALSE; }
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(const SudekiMpControlUpdateDispatchWitness *w) { (void)w; return FALSE; }
BOOL SudekiMpLanStoryInputControllerExact(void *c) { (void)c; return FALSE; }
BOOL SudekiMpLanStoryObserverRosterStillExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) { (void)w; (void)r; return FALSE; }
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

static uint8_t world[0x39b],manager[0x40],group[0xd0],actors[5][0x138],components[5][8][0x180];
static uint8_t *jobs[4];
static void *catalogue[16];
static uint32_t name_ref[2];
static const char name[]="ALLY_TALOS";
static uint32_t placement[7]={2,0x3f800000,0x40000000,0x40400000,0,0,0x3f800000};
static unsigned complete_calls,destroy_calls;
static void *expected_job,*expected_actor;
static const unsigned component_offsets[]={0x44,0x80,0x8c,0x90,0x94,0xa8,0xac,0xb8};
static const unsigned component_vtables[]={0x2cdefc,0x2c8644,0x2d48d4,0x2cc9ac,0x2d4924,0x2d4abc,0x2d4b24,0x2d4bd4};
static void make_actor(unsigned i) {
    uint8_t *a=actors[i]; memset(a,0,sizeof(actors[i]));
    *(void **)a=mapped+0x2d55d4; *(void **)(a+8)=mapped+0x2d55f8;
    *(void **)(a+0x2c)=mapped+0x2d5618;
    *(uint32_t *)(a+0x30)=0xf82; *(uint32_t *)(a+0x34)=0x123456;
    *(void **)(a+0x38)=name_ref;
    for(unsigned c=0;c<8;++c) {
        *(void **)(a+component_offsets[c])=components[i][c];
        *(void **)components[i][c]=mapped+component_vtables[c];
        *(void **)(components[i][c]+0x10)=a;
    }
}
static void fixture_world(void) {
    memset(manager,0,sizeof(manager)); memset(group,0,sizeof(group));
    catalogue[0]=group; /* An unrelated preexisting actor identity. */
    *(void **)(mapped+0x408d10)=world;
    *(void **)(mapped+0x409d8c)=manager; *(void **)(manager+0x3c)=catalogue;
    *(uint32_t *)(manager+0x34)=1;
    *(void **)(mapped+0x408d94)=group; *(uint32_t *)(group+0xcc)=1;
    *(void **)(group+0x90)=group;
    name_ref[0]=50; name_ref[1]=(uint32_t)(uintptr_t)name;
    native_thread=GetCurrentThreadId(); status.load_generation=123; load_manager=manager;
    for(unsigned i=0;i<5;++i) make_actor(i);
    for(unsigned i=0;i<4;++i) {
        jobs[i]=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE); assert(jobs[i]);
    }
}
/* Execute the real relocated retail constructor through its installed hook.
 * It consumes by-value ResourceName tuples; fixture references stay positive. */
static void __attribute__((noipa)) construct(unsigned seat) {
    void *job=jobs[seat],*point=placement,*entry=mapped+SPAWN_CTOR;
    uint32_t *ref=name_ref;
    __asm__ volatile("push $-1; push $1; push $0; push $0; push $0;"
        "push $0; push $0; push $0; push %3; push $0x123456; push $0xf82; call *%2"
        : "+a"(point),"+S"(job) : "r"(entry),"r"(ref) : "ecx","edx","cc","memory");
    assert(point==jobs[seat] && !callbacks);
}
static void __attribute__((cdecl,noipa)) complete_fixture(void *actor,void *point,
    uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t f,
    uint32_t g,uint32_t h,uint32_t i,uint32_t generic,int party) {
    assert(actor==expected_actor && point==(uint8_t *)expected_job+0x1c);
    assert(!a && !b && !c && !d && !e && !f && !g && !h && !i && generic==1 && party==-1);
    assert(callbacks==1 && !SudekiMpLanStoryAvatarSpawnShutdown());
    ++complete_calls; SetLastError(345);
}
static void *__attribute__((thiscall,noipa)) destroy_fixture(void *job,unsigned flags) {
    assert(job==expected_job && flags==1 && callbacks==1);
    DWORD old; assert(VirtualProtect(job,4096,PAGE_NOACCESS,&old));
    ++destroy_calls; SetLastError(678); return job;
}
/* Completion bridge gets job=ESI-1c and actual actor from the native call's
 * first argument. Replace only the body behind the already-tested bridge. */
static void __attribute__((noipa)) complete(unsigned seat,unsigned actor_index) {
    expected_job=jobs[seat]; expected_actor=actors[actor_index];
    ((uint8_t *)expected_job)[0x54]=4;
    catalogue[(*(uint32_t *)(manager+0x34))++]=expected_actor;
    RawFunction saved=original_spawn_complete; original_spawn_complete=(RawFunction)complete_fixture;
    void *point=(uint8_t *)expected_job+0x1c,*entry=story_spawn_complete,*actor=expected_actor;
    __asm__ volatile("push $-1; push $1; push $0; push $0; push $0; push $0;"
        "push $0; push $0; push $0; push $0; push $0; push %0; push %2; call *%1; add $52,%%esp"
        : "+S"(point) : "r"(entry),"r"(actor) : "eax","ecx","edx","cc","memory");
    original_spawn_complete=saved; assert(GetLastError()==345 && !callbacks);
}
static void __attribute__((noipa)) destroy(unsigned seat) {
    expected_job=jobs[seat]; RawFunction saved=original_spawn_destroy;
    original_spawn_destroy=(RawFunction)destroy_fixture;
    void *result,*job=expected_job,*entry=story_spawn_destroy;
    __asm__ volatile("push $1; call *%2" : "=a"(result),"+c"(job) : "r"(entry) : "edx","cc","memory");
    original_spawn_destroy=saved; assert(result==expected_job && GetLastError()==678 && !callbacks);
}
static SudekiMpLanStoryAvatarSpawnObservation observation(unsigned seat,unsigned generation) {
    SudekiMpLanStoryAvatarSpawnObservation o;
    assert(SudekiMpLanStoryAvatarSpawnObserve(seat,77,generation,&o)); return o;
}
static DWORD WINAPI wrong_thread(void *unused) {
    (void)unused; SudekiMpLanStoryAvatarSpawnObservation o;
    assert(!SudekiMpLanStoryAvatarSpawnObserve(0,77,1,&o));
    assert(!SudekiMpLanStoryAvatarSpawnBegin((HMODULE)mapped,3,77,1)); return 0;
}
static void verified_exit(void) {
    uint32_t load=status.load_generation;
    assert(!SudekiMpLanStoryAvatarSpawnShutdown());
    assert(!SudekiMpLanStoryAvatarSpawnWorldExited(77,load,world));
    exit_status=0; assert(!SudekiMpLanStoryTaskTraceForgetExitedWorld());
    assert(!SudekiMpLanStoryAvatarSpawnWorldExited(77,load,world));
    exit_status=1; assert(SudekiMpLanStoryTaskTraceForgetExitedWorld());
    assert(SudekiMpLanStoryAvatarSpawnWorldExited(77,load,world));
    assert(!SudekiMpLanStoryAvatarSpawnWorldExited(77,load,NULL));
    assert(!SudekiMpLanStoryAvatarSpawnWorldExited(78,load,world));
    assert(!SudekiMpLanStoryAvatarSpawnWorldExited(77,load+1,world));
    assert(!SudekiMpLanStoryAvatarSpawnWorldExited(77,load,manager));
    assert(SudekiMpLanStoryAvatarSpawnShutdown());
    assert(!SudekiMpLanStoryAvatarSpawnWorldExited(77,load,world));
}
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024)); mapped=map_image(path);
    assert(SudekiMpLanStoryAvatarPartyImageMatches((HMODULE)mapped));
    assert(SudekiMpLanStoryTaskTraceInstall((HMODULE)mapped));
    /* Real startup hook composition, before load/thread enrollment. */
    assert(!native_thread && !status.load_generation);
    assert(SudekiMpLanStoryTaskTraceAddCallImageExact((HMODULE)mapped));
    assert(SudekiMpLanStoryAvatarPartyImageMatches((HMODULE)mapped));
    int32_t hooked_add; memcpy(&hooked_add,mapped+ADD_CALL+1,4);
    mapped[ADD_CALL+1]^=1;
    assert(!SudekiMpLanStoryTaskTraceAddCallImageExact((HMODULE)mapped));
    assert(!SudekiMpLanStoryAvatarPartyImageMatches((HMODULE)mapped));
    memcpy(mapped+ADD_CALL+1,&hooked_add,4);
    int32_t native_add=(int32_t)(ADD-ADD_CALL-5); memcpy(mapped+ADD_CALL+1,&native_add,4);
    assert(!SudekiMpLanStoryAvatarPartyImageMatches((HMODULE)mapped)); /* Restored behind its owner. */
    memcpy(mapped+ADD_CALL+1,&hooked_add,4);
    mapped[ADD_CALL]=0xe9; assert(!SudekiMpLanStoryAvatarPartyImageMatches((HMODULE)mapped)); mapped[ADD_CALL]=0xe8;
    RawFunction retained_add=original_add; original_add=NULL;
    assert(!SudekiMpLanStoryAvatarPartyImageMatches((HMODULE)mapped)); original_add=retained_add;
    mapped[0x23230]^=1; assert(!SudekiMpLanStoryAvatarPartyImageMatches((HMODULE)mapped)); mapped[0x23230]^=1;
    assert(SudekiMpLanStoryAvatarPartyImageMatches((HMODULE)mapped));
    fixture_world();
    assert(SudekiMpLanStoryTaskTraceEntitySetupExact((HMODULE)mapped));
    uint8_t saved=mapped[SPAWN_COMPLETE_CALL]; mapped[SPAWN_COMPLETE_CALL]^=1;
    assert(!SudekiMpLanStoryAvatarSpawnBegin((HMODULE)mapped,0,77,1));
    mapped[SPAWN_COMPLETE_CALL]=saved;
    for(unsigned seat=0;seat<3;++seat) {
        assert(SudekiMpLanStoryAvatarSpawnBegin((HMODULE)mapped,seat,77,1));
        assert(!SudekiMpLanStoryAvatarSpawnBegin((HMODULE)mapped,3,77,1));
        construct(seat); assert(SudekiMpLanStoryAvatarSpawnEnd(seat,77,1));
        SudekiMpLanStoryAvatarSpawnObservation o=observation(seat,1);
        assert(o.construction_exact && !o.ready && !o.unknown && !o.actor);
    }
    HANDLE worker=CreateThread(NULL,0,wrong_thread,NULL,0,NULL); assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0); CloseHandle(worker);
    assert(!SudekiMpLanStoryTaskTraceUninstall());
    /* Same resource, three different actors; finish in a different order. */
    complete(2,2); assert(!observation(2,1).ready); destroy(2);
    assert(observation(2,1).ready && observation(2,1).actor==actors[2]);
    assert(!SudekiMpLanStoryAvatarSpawnGroupBegin(group,77,SUDEKIMP_AVATAR_SPAWN_GROUP_ADD,actors[2]));
    complete(0,0); destroy(0);
    *(uint32_t *)(components[0][3]+0x50)=0x400;
    assert(!observation(0,1).ready && !observation(0,1).unknown);
    *(uint32_t *)(components[0][3]+0x50)=0;
    assert(observation(0,1).ready && observation(0,1).actor==actors[0]);
    complete(1,1); destroy(1);
    assert(observation(1,1).ready && observation(1,1).actor==actors[1]);
    assert(complete_calls==3 && destroy_calls==3);
    /* Native-operation receipts are fixtures here: no native party mutation
     * API is executed. Default leases still reject unannounced group drift. */
    BOOL changed=TRUE; SudekiMpLanStoryAvatarSpawnObservation pending;
    assert(!SudekiMpLanStoryAvatarSpawnGroupBegin(group,77,SUDEKIMP_AVATAR_SPAWN_GROUP_ADD,actors[4]));
    assert(SudekiMpLanStoryAvatarSpawnGroupBegin(group,77,SUDEKIMP_AVATAR_SPAWN_GROUP_ADD,actors[0]));
    assert(!SudekiMpLanStoryAvatarSpawnObserve(0,77,1,&pending));
    assert(!SudekiMpLanStoryAvatarSpawnBegin((HMODULE)mapped,3,77,1));
    assert(!SudekiMpLanStoryAvatarSpawnGroupEnd(manager,&changed));
    assert(SudekiMpLanStoryAvatarSpawnGroupEnd(group,&changed) && !changed); /* Native veto. */
    assert(observation(0,1).ready);
    assert(SudekiMpLanStoryAvatarSpawnGroupBegin(group,77,SUDEKIMP_AVATAR_SPAWN_GROUP_ADD,actors[0]));
    *(void **)(group+0x9c)=actors[0]; *(uint32_t *)(group+0xcc)=2;
    assert(SudekiMpLanStoryAvatarSpawnGroupEnd(group,&changed) && changed);
    assert(observation(0,1).ready && observation(1,1).ready && observation(2,1).ready);
    assert(!SudekiMpLanStoryAvatarSpawnGroupBegin(group,77,SUDEKIMP_AVATAR_SPAWN_GROUP_REMOVE,actors[0]));
    assert(!SudekiMpLanStoryAvatarSpawnGroupBegin(group,77,SUDEKIMP_AVATAR_SPAWN_GROUP_REMOVE,group));
    assert(SudekiMpLanStoryAvatarSpawnGroupBegin(group,77,SUDEKIMP_AVATAR_SPAWN_GROUP_ROTATE,actors[0]));
    *(void **)(group+0x90)=actors[0]; *(void **)(group+0x9c)=group;
    assert(SudekiMpLanStoryAvatarSpawnGroupEnd(group,&changed) && changed);
    assert(SudekiMpLanStoryAvatarSpawnGroupBegin(group,77,SUDEKIMP_AVATAR_SPAWN_GROUP_REMOVE,group));
    *(void **)(group+0x9c)=NULL; *(uint32_t *)(group+0xcc)=1;
    assert(SudekiMpLanStoryAvatarSpawnGroupEnd(group,&changed) && changed);
    assert(observation(0,1).ready && observation(1,1).ready && observation(2,1).ready);
    *(void **)(components[1][1]+0x10)=actors[0];
    assert(observation(1,1).unknown && !observation(1,1).ready);
    *(void **)(components[1][1]+0x10)=actors[1];
    assert(observation(1,1).unknown); /* Identity fault does not heal. */
    assert(observation(0,1).ready && observation(2,1).ready);
    assert(SudekiMpLanStoryAvatarSpawnBegin((HMODULE)mapped,3,77,1));
    construct(3); assert(SudekiMpLanStoryAvatarSpawnEnd(3,77,1));
    complete(3,0); destroy(3); /* Another job cannot borrow an existing seat actor. */
    assert(observation(3,1).unknown && !observation(3,1).ready);
    *(void **)(mapped+0x408d10)=manager;
    assert(observation(2,1).unknown && !observation(2,1).ready);
    *(void **)(mapped+0x408d10)=world;
    assert(observation(2,1).unknown);
    verified_exit();
    /* Reuse requires a new seat generation. No constructor is unknown, never
     * a timeout permitting another unobserved native spawn. */
    status.load_generation=124; load_manager=manager;
    assert(!SudekiMpLanStoryAvatarSpawnBegin((HMODULE)mapped,0,77,1));
    assert(SudekiMpLanStoryAvatarSpawnBegin((HMODULE)mapped,0,77,2));
    assert(!SudekiMpLanStoryAvatarSpawnWorldExited(77,123,world));
    assert(!SudekiMpLanStoryAvatarSpawnEnd(0,77,2));
    assert(observation(0,2).unknown);
    assert(!SudekiMpLanStoryAvatarSpawnBegin((HMODULE)mapped,0,77,3));
    verified_exit();
    status.load_generation=125; load_manager=manager;
    DWORD old; assert(VirtualProtect(jobs[0],4096,PAGE_READWRITE,&old));
    memset(jobs[0],0,0x58);
    assert(SudekiMpLanStoryAvatarSpawnBegin((HMODULE)mapped,0,77,3));
    construct(0); assert(SudekiMpLanStoryAvatarSpawnEnd(0,77,3));
    *(void **)(group+0x90)=actors[4];
    complete(0,4); destroy(0);
    assert(observation(0,3).unknown && !observation(0,3).ready);
    verified_exit();
    assert(SudekiMpLanStoryTaskTraceUninstall());
    assert(SudekiMpLanStoryAvatarPartyImageMatches((HMODULE)mapped));
    for(unsigned i=0;i<4;++i) assert(VirtualFree(jobs[i],0,MEM_RELEASE));
    assert(VirtualFree(mapped,0,MEM_RELEASE));
    puts("story avatar spawn: PASS (exact image, real ctor, fixture completion/destructor dispatch; no gameplay)");
    return 0;
}
