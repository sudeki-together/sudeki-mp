/* Executes retail target scoring and intrusive target replacement against
 * inert world-query candidates. No campaign, combat task or renderer runs. */
#include "hooks/story_target_refresh.h"
#include "engine/build_identity.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *image,*targeter;
static uint8_t actor[0x140],position[0x110],arbiter[0x64],ai[0x170],world[0x100],collision[0x100];
static uint8_t enemies[2][0x140],positions[2][0x110],arbiters[2][0x64];
static void *type_vtable[5];
static unsigned query_count=2,queries;
static BOOL memory(const void *p,size_t n,BOOL write) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || !VirtualQuery(p,&m,sizeof(m)) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return !write || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size); CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;
    IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *base=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
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
    assert(FlushInstructionCache(GetCurrentProcess(),base,nt->OptionalHeader.SizeOfImage));
    free(raw); return base;
}

__attribute__((naked,noinline)) static void assign(void *slot __attribute__((unused)),
    void *node __attribute__((unused)),void *entry __attribute__((unused))) {
    __asm__ volatile("movl 4(%esp), %eax\n\tmovl 8(%esp), %edx\n\tjmp *12(%esp)\n\t");
}
__attribute__((naked,noinline)) static void old_refresh(void *tg __attribute__((unused)),
    void *entry __attribute__((unused))) {
    __asm__ volatile("pushl %esi\n\tmovl 8(%esp), %esi\n\tcall *12(%esp)\n\tpopl %esi\n\tret\n\t");
}
static unsigned __attribute__((thiscall)) kind(void *self) { (void)self; return 1; }
static void __stdcall position_ready(void *self) { assert(self==position); }
static float *__stdcall normalize(float *out,const float *in) {
    float n=sqrtf(in[0]*in[0]+in[1]*in[1]+in[2]*in[2]);
    for(unsigned i=0;i<3;++i) out[i]=n>0?in[i]/n:0;
    return out;
}
static unsigned __stdcall query(void *owner,const float *point,float range,
    unsigned capacity,uint8_t *slots,unsigned mask) {
    assert(owner==collision && point[0]==0 && point[1]==0 && point[2]==0);
    assert(range==10.0f && capacity==15u && mask==2u); ++queries;
    for(unsigned i=0;i<query_count;++i) assign(slots+12u*i,enemies[i],image+0x1750u);
    return query_count;
}
static void jump(unsigned rva,void *replacement,uint8_t saved[5]) {
    memcpy(saved,image+rva,5u); image[rva]=0xe9;
    int32_t d=(int32_t)((uint8_t *)replacement-(image+rva+5)); memcpy(image+rva+1,&d,4u);
    assert(FlushInstructionCache(GetCurrentProcess(),image+rva,5u));
}
static BOOL refresh(float x,float z,BOOL *did) {
    return SudekiMpStoryTargetRefresh(image,actor,world,x,z,did,memory);
}
static void image_contract(void) {
    const struct { const char *name; unsigned rva,size; } regions[]={
        {"STORY_TARGET_ACQUIRE",STORY_TARGET_ACQUIRE,0x504},
        {"STORY_TARGET_COMMIT",STORY_TARGET_COMMIT,0x95},
        {"STORY_TARGET_EXCLUDE",STORY_TARGET_EXCLUDE,0x38}};
    assert(SudekiMpStoryTargetImageExact(image,memory));
    for(unsigned k=0;k<3;++k) {
        for(unsigned i=0;i<regions[k].size;++i) {
            image[regions[k].rva+i]^=1;
            assert(!SudekiMpStoryTargetImageExact(image,memory));
            image[regions[k].rva+i]^=1;
        }
        printf("%s: exact bytes and every-byte rejection passed\n",regions[k].name);
    }
    assert(FlushInstructionCache(GetCurrentProcess(),image,0x45f000u));
}
int main(int argc,char **argv) {
    wchar_t path[32768]; assert(argc==2);
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768)); image=map_image(path);
    image_contract(); targeter=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(targeter);
    *(void **)targeter=image+0x2d4b24u; *(void **)(targeter+0x10)=actor;
    *(void **)(actor+0xac)=targeter; *(void **)(actor+0x44)=position;
    *(void **)(actor+0x90)=arbiter; *(void **)(actor+0x94)=ai;
    *(void **)position=image+0x2cdefcu; *(void **)(position+0x10)=actor;
    *(float *)(position+0x50)=1;
    *(void **)arbiter=image+0x2cc9acu; *(void **)(arbiter+0x10)=actor;
    *(int16_t *)(ai+0x16a)=1; targeter[0x84]=3; /* AI target retained. */
    *(uint16_t *)(targeter+0x66)=0x55a0; /* 90 degrees, native half-float. */
    *(uint16_t *)(targeter+0x68)=0x4900; /* 10-unit search. */
    *(uint16_t *)(targeter+0x6a)=0x4500; /* 5-unit near range. */
    *(uint32_t *)(targeter+0x7c)=2;
    *(void **)(image+STORY_TARGET_WORLD)=world;
    *(void **)(image+STORY_TARGET_COLLISION)=collision; type_vtable[4]=kind;
    for(unsigned i=0;i<2;++i) {
        *(void **)(enemies[i]+0x44)=positions[i];
        *(void **)(enemies[i]+0x90)=arbiters[i];
        *(void **)(enemies[i]+0x2c)=type_vtable;
        *(float *)(positions[i]+0x18)=i? -3.0f:3.0f;
    }
    uint8_t query_saved[5],position_saved[5];
    jump(0x34c20u,query,query_saved); jump(0x110d40u,position_ready,position_saved);
    void *normalize_saved=*(void **)(image+0x29a2b0u);
    *(void **)(image+0x29a2b0u)=normalize;
    /* Protect host input/lead/lock identities from accidental access. */
    uint32_t globals[]={0x408da4u,0x408d94u,0x3c2fcdu,0x3c3b44u};
    uint32_t values[4];
    for(unsigned i=0;i<4;++i) { values[i]=*(uint32_t *)(image+globals[i]); *(uint32_t *)(image+globals[i])=0x12345678u; }
    assign(targeter+0x54,enemies[1],image+0x1750u);
    *(float *)(targeter+0x48)=1;
    old_refresh(targeter,image+0xb9ab0u);
    assert(*(void **)(targeter+0x54)==enemies[1] && !queries);
    BOOL did=FALSE;
    assert(refresh(1,0,&did) && did && *(void **)(targeter+0x54)==enemies[0]);
    assert(!*(void **)(enemies[1]+4) && *(void **)(enemies[0]+4)==targeter+0x54);
    assert(refresh(-1,0,&did) && did && *(void **)(targeter+0x54)==enemies[1]);
    assert(!*(void **)(enemies[0]+4) && *(void **)(enemies[1]+4)==targeter+0x54);
    assert(refresh(0,0,&did) && did && *(void **)(targeter+0x54)==enemies[1]);
    *(float *)(targeter+0x48)=0;
    assert(refresh(0,0,&did) && did && *(void **)(targeter+0x54)==enemies[0]);
    assert(targeter[0x84]==3 && *(int16_t *)(ai+0x16a)==1);
    for(unsigned i=0;i<4;++i) assert(*(uint32_t *)(image+globals[i])==0x12345678u);
    /* Native dead-target filtering and native no-candidate retirement. */
    *(uint32_t *)(arbiters[0]+0x50)=0x8000; *(uint32_t *)(arbiters[1]+0x50)=0x4000;
    assert(refresh(1,0,&did) && did && !*(void **)(targeter+0x54));
    *(uint32_t *)(arbiters[0]+0x50)=0; *(uint32_t *)(arbiters[1]+0x50)=0;
    assert(refresh(1,0,&did) && did); query_count=0;
    assert(refresh(1,0,&did) && did && !*(void **)(targeter+0x54)); query_count=2;
    /* Exclusions remain native; do not replace native scoring with ours. */
    uint8_t excluded[12]={0}; *(void **)excluded=enemies[0];
    *(int *)(targeter+0x88)=1; *(void **)(targeter+0x90)=excluded;
    assert(refresh(1,0,&did) && did && !*(void **)(targeter+0x54));
    *(int *)(targeter+0x88)=0;
    unsigned before=queries;
    targeter[0x84]=1; assert(refresh(-1,0,&did) && !did); targeter[0x84]=3;
    arbiter[0x58]=0x20; assert(refresh(-1,0,&did) && !did); arbiter[0x58]=0;
    *(uint32_t *)(arbiter+0x50)=0x400000; assert(refresh(-1,0,&did) && !did);
    *(uint32_t *)(arbiter+0x50)=0x10; assert(refresh(-1,0,&did) && !did);
    *(uint32_t *)(arbiter+0x50)=0; arbiter[0x60]=4;
    assert(refresh(-1,0,&did) && !did); arbiter[0x60]=0;
    assert(queries==before && *(float *)(targeter+0x48)==1);
    assert(!refresh(NAN,0,&did) && !did); assert(!refresh(2,0,&did));
    *(void **)(targeter+0x10)=NULL; assert(!refresh(1,0,&did)); *(void **)(targeter+0x10)=actor;
    *(void **)targeter=image; assert(!refresh(1,0,&did)); *(void **)targeter=image+0x2d4b24;
    *(void **)(position+0x10)=NULL; assert(!refresh(1,0,&did)); *(void **)(position+0x10)=actor;
    *(void **)(image+STORY_TARGET_WORLD)=actor; assert(!refresh(1,0,&did)); *(void **)(image+STORY_TARGET_WORLD)=world;
    *(void **)(image+STORY_TARGET_COLLISION)=NULL; assert(!refresh(1,0,&did));
    *(void **)(image+STORY_TARGET_COLLISION)=collision;
    *(uint32_t *)(collision+0x80)=8193; assert(!refresh(1,0,&did));
    *(uint32_t *)(collision+0x80)=1; assert(!refresh(1,0,&did));
    *(uint32_t *)(collision+0x80)=0;
    *(int *)(targeter+0x88)=-1; assert(!refresh(1,0,&did)); *(int *)(targeter+0x88)=0;
    *(void **)(targeter+0x58)=actor; assert(!refresh(1,0,&did)); *(void **)(targeter+0x58)=NULL;
    DWORD protection; assert(VirtualProtect(targeter,4096,PAGE_READONLY,&protection));
    assert(!refresh(1,0,&did)); assert(VirtualProtect(targeter,4096,protection,&protection));
    assert(queries==before);
    assert(refresh(1,0,&did) && did);
    assign(targeter+0x54,NULL,image+0x1750u);
    assert(!*(void **)(enemies[0]+4) && !*(void **)(enemies[1]+4));
    memcpy(image+0x34c20u,query_saved,5); memcpy(image+0x110d40u,position_saved,5);
    *(void **)(image+0x29a2b0u)=normalize_saved;
    for(unsigned i=0;i<4;++i) *(uint32_t *)(image+globals[i])=values[i];
    assert(SudekiMpStoryTargetImageExact(image,memory));
    VirtualFree(targeter,0,MEM_RELEASE); VirtualFree(image,0,MEM_RELEASE);
    puts("story target refresh: native heading/scoring/retirement, ABI, intrusive links and owner guards passed (no gameplay)");
    return 0;
}
