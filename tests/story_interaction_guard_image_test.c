/* Exact-image early native input branches, inert actors/trigger manager.
 * Never load a campaign, dispatch a script, or execute an attack task. */
#include "hooks/story_interaction_guard.h"
#include "engine/build_identity.h"
#include "engine/arbiter_combat_input.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *image;
static uint8_t actor[0xac],arbiter[0x64],trigger[0x1da],interaction[0x64],mode[0x84];
static unsigned submissions;
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || !VirtualQuery(p,&m,sizeof(m)) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    return a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
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
static void submit(unsigned kind) {
    ++submissions;
    SudekiMpSubmitArbiterCombatInput(image+0xdb0e0,arbiter,kind==1,kind==2,kind==3,0,0,0);
}
static BOOL guarded(unsigned kind) {
    if(!SudekiMpStoryInteractionInputImageExact(image,readable) ||
        !SudekiMpStoryMeleeInteractionClear(image,actor,kind,readable)) return FALSE;
    submit(kind); return TRUE;
}
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024)); image=map_image(path);
    assert(SudekiMpStoryInteractionInputImageExact(image,readable));
    for(unsigned i=0;i<0xa9;++i) {
        image[0xdb0e0+i]^=1;
        assert(!SudekiMpStoryInteractionInputImageExact(image,readable));
        image[0xdb0e0+i]^=1;
    }
    assert(FlushInstructionCache(GetCurrentProcess(),image+0xdb0e0,0xa9));
    *(void **)(image+0x408d24)=trigger;
    *(void **)trigger=image+0x2c5458; *(void **)(trigger+8)=image+0x2c5460;
    *(void **)(trigger+0x10)=image+0x2c5470;
    *(void **)(actor+0xa8)=interaction; *(void **)(arbiter+0x10)=actor;
    *(void **)interaction=image+0x2d4abc; *(void **)(interaction+0x10)=actor;
    *(void **)(interaction+0x60)=mode; *(void **)mode=image+0x2cbfcc;
    /* Native combat flags zero: retail returns after the early global branch.
     * Demonstrate the leak even when no actual attack can be accepted. */
    trigger[0x1d9]=2; submit(1); assert(trigger[0x1d8]==1);
    trigger[0x1d8]=0; unsigned before=submissions;
    assert(!guarded(1) && submissions==before && !trigger[0x1d8] && trigger[0x1d9]==2);
    /* Preserve a host press even after proximity bit2 has disappeared. */
    trigger[0x1d9]=0; trigger[0x1d8]=1;
    assert(!guarded(1) && submissions==before && trigger[0x1d8]==1);
    trigger[0x1d8]=0;
    for(unsigned kind=1;kind<=3;++kind) assert(guarded(kind));
    /* Active interaction branch advances on ALL kinds, not just weak. */
    mode[0x4c]=1;
    for(unsigned kind=1;kind<=3;++kind) {
        *(uint32_t *)(mode+0x80)=0; submit(kind); assert(*(uint32_t *)(mode+0x80)==1);
        before=submissions; assert(!guarded(kind) && submissions==before);
        assert(*(uint32_t *)(mode+0x80)==1);
    }
    mode[0x4c]=0; trigger[0x1d8]=1; trigger[0x1d9]=2;
    assert(guarded(2) && guarded(3)); assert(trigger[0x1d8]==1 && trigger[0x1d9]==2);
    /* Exhaust every manager flag byte; policy reads without mutation. */
    for(unsigned flags=0;flags<256;++flags) for(unsigned latch=0;latch<3;++latch) {
        trigger[0x1d9]=(uint8_t)flags; trigger[0x1d8]=(uint8_t)latch;
        for(unsigned kind=1;kind<=3;++kind)
            assert(SudekiMpStoryMeleeInteractionClear(image,actor,kind,readable)==
                (kind!=1 || (!(flags&2) && !latch)));
        assert(trigger[0x1d9]==flags && trigger[0x1d8]==latch);
    }
    trigger[0x1d8]=trigger[0x1d9]=0;
    for(unsigned part=0;part<3;++part) {
        unsigned offset=part==0?0:part==1?8:16;
        void *old=*(void **)(trigger+offset); *(void **)(trigger+offset)=image;
        assert(!guarded(1)); *(void **)(trigger+offset)=old;
    }
    *(void **)(interaction+0x10)=NULL;
    for(unsigned kind=1;kind<=3;++kind) assert(!guarded(kind));
    *(void **)(interaction+0x10)=actor;
    *(void **)(interaction+0x60)=NULL; assert(!guarded(1)); *(void **)(interaction+0x60)=mode;
    *(void **)mode=image; assert(!guarded(2)); *(void **)mode=image+0x2cbfcc;
    *(void **)interaction=image; assert(!guarded(3)); *(void **)interaction=image+0x2d4abc;
    *(void **)(image+0x408d24)=NULL; assert(!guarded(1)); *(void **)(image+0x408d24)=trigger;
    assert(!guarded(0) && !guarded(4));
    /* Retail explicitly supports no interaction component; only its global
     * weak branch then needs containment. */
    *(void **)(actor+0xa8)=NULL; assert(guarded(1));
    assert(!SudekiMpStoryMeleeInteractionClear(image,NULL,1,readable));
    VirtualFree(image,0,MEM_RELEASE);
    puts("story interaction guard exact-image/early-native-branch tests passed (no campaign or attack tasks)");
    return 0;
}
