/* Execute retail skill readiness and validation on inert actor components.
 * No native Use, constructor, task, damage, resource change or gameplay runs.
 * The fixture skips availability/SP checks solely to isolate readiness; the
 * production adapter continues to use every native validator unchanged. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/build_identity.h"
#include "engine/skill_activation_abi.h"
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

int main(int argc,char **argv) {
    wchar_t path[32768];
    assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768));
    uint8_t *image=map_image(path),actor[0xdc]={0},arb[0x80]={0};
    uint8_t skill[0x78]={0},data[0x98]={0},gate[0xa20]={0},ai[0x180]={0},combat[0x78]={0};
    *(void **)(actor+0x90)=arb; *(void **)(actor+0x94)=ai;
    *(void **)(actor+0xd8)=skill; *(void **)(arb+0x10)=actor;
    *(void **)(actor+0xa4)=combat; *(void **)(combat+0x10)=actor;
    *(void **)(ai+0x10)=actor; *(void **)(skill+0x10)=actor;
    *(void **)(skill+0x3c)=data;
    *(void **)(image+0x408dd0)=gate;
    image[0x34a8b0]=1; image[0x3c2fcc]=1;
    SudekiMpSkillValidateFunction validate=(SudekiMpSkillValidateFunction)(image+0xb4bc0);
    typedef uint8_t (__attribute__((regparm(1))) *Ready)(void *);
    Ready ready=(Ready)(image+0xdb9b0);
    *(uint32_t *)(arb+0x50)=3; *(uint32_t *)(arb+0x58)=2; arb[0x60]=2;
    /* Player/leased-AI count is not a readiness predicate. */
    for(unsigned count=0;count<3;++count) {
        *(uint16_t *)(ai+0x16a)=(uint16_t)count;
        assert(ready(arb) && validate(skill,0)==0);
    }
    /* Both a native action lock and the packed action lanes deny a cast,
     * even with no attacking flag. They must never be force-cleared. */
    combat[0x72]=8; assert(!ready(arb) && validate(skill,0)==2); combat[0x72]=0;
    for(unsigned shift=0;shift<=18;shift+=9) {
        *(uint32_t *)(combat+0x5c)=42u<<shift;
        assert(!ready(arb) && validate(skill,0)==2);
        *(uint32_t *)(combat+0x5c)=54u<<shift;
        assert(!ready(arb) && validate(skill,0)==2);
    }
    *(uint32_t *)(combat+0x5c)=0;
    *(uint32_t *)(arb+0x58)=1; assert(!ready(arb) && validate(skill,0)==2);
    *(uint32_t *)(arb+0x58)=3; assert(!ready(arb) && validate(skill,0)==3);
    *(uint32_t *)(arb+0x58)=2;
    *(uint32_t *)(arb+0x50)=0x4003; assert(!ready(arb) && validate(skill,0)==2);
    *(uint32_t *)(arb+0x50)=1; assert(!ready(arb) && validate(skill,0)==3);
    *(uint32_t *)(arb+0x50)=3; arb[0x60]=0; assert(!ready(arb) && validate(skill,0)==3);
    arb[0x60]=2; assert(ready(arb) && validate(skill,0)==0);
    gate[0xa18]=8; assert(validate(skill,0)==4); gate[0xa18]=0;
    image[0x34a8b0]=0; assert(validate(skill,0)==5);
    VirtualFree(image,0,MEM_RELEASE);
    puts("Story native skill readiness: PASS (inert components; no gameplay cast)");
    return 0;
}
