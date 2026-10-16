/* H1a remote chest use: exact retail image, synthetic party group. Only the
 * retail handle copy (0x15B0) executes; no treasure, usable or script runs. */
#include "../src/hooks/lan_story_remote_use.c"
#include "engine/build_identity.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
void SudekiMpLogFormat(const char *format,...) { va_list a; va_start(a,format); vprintf(format,a); va_end(a); }
void SudekiMpLogWrite(const char *message) { fputs(message,stdout); }
static uint8_t *mapped;
static size_t image_size;
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
static void *call_target(uint32_t rva) {
    int32_t rel; memcpy(&rel,mapped+rva+1u,4u); return mapped+rva+5u+rel;
}
/* Handles are {pointer, previous, next}; the pointee keeps its list head at +4. */
static uint8_t ailish[0x20],tal[0x20],grp[0xd8];
static void *copy(void *source) {
    static uint32_t destination[3];
    uintptr_t a=(uintptr_t)destination,c=(uintptr_t)source;
    __asm__ volatile("call *%[entry]" : "+a"(a),"+c"(c) : [entry] "r"((void *)front_copy_stub) : "edx","memory","cc");
    void *got=(void *)(uintptr_t)destination[0];
    /* Release like retail 0x15E0 would: unlink the destination from its list. */
    uint8_t *owner=got; if(owner && *(uint32_t **)(owner+4)==destination) *(uint32_t *)(owner+4)=destination[2];
    memset(destination,0,sizeof(destination));
    return got;
}
static unsigned controller_skips(void) {
    unsigned eax_out;
    /* Push the one stack argument; a skip returns with RET 4 to here. */
    __asm__ volatile("push $1\n\tcall *%[stub]" : "=a"(eax_out) : [stub] "r"((void *)controller_stub) : "ecx","edx","memory","cc");
    return eax_out;
}
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024]; assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    mapped=map_image(path);
    uint8_t *pristine=malloc(image_size); memcpy(pristine,mapped,image_size);
    /* A changed guard byte refuses the whole install. */
    mapped[RVA_LISTENER+2]^=1; assert(!SudekiMpLanStoryRemoteUseInstall((HMODULE)mapped) && !installed);
    mapped[RVA_LISTENER+2]^=1;
    mapped[front_sites[5]-3]^=0xff;
    {   /* the party-group load is checked only within the 40 preceding bytes */
        BOOL refused=!SudekiMpLanStoryRemoteUseInstall((HMODULE)mapped);
        if(!refused) assert(SudekiMpLanStoryRemoteUseUninstall());
    }
    memcpy(mapped,pristine,image_size);
    assert(SudekiMpLanStoryRemoteUseInstall((HMODULE)mapped) && installed);
    for(unsigned i=0;i<FRONT_SITES;++i) assert(call_target(front_sites[i])==(void *)front_copy_stub);
    for(unsigned i=0;i<CONTROLLER_SITES;++i) assert(call_target(controller_sites[i])==(void *)controller_stub);
    assert(*(void **)(mapped+RVA_NODE_UPDATE_SLOT)==(void *)update_wrapper);
    assert(*(void **)(mapped+RVA_FINISH_SLOT)==(void *)finish_wrapper);
    /* Front copy: party slot 0 = Ailish (front), slot 1 = Tal. */
    *(void **)(mapped+RVA_GROUP)=grp;
    *(void **)(grp+GROUP_FRONT)=ailish; *(void **)(grp+GROUP_FRONT+GROUP_SLOT)=tal;
    assert(copy(grp+GROUP_FRONT)==ailish);                  /* no scope: native front */
    assert(scope_begin(tal));
    assert(copy(grp+GROUP_FRONT)==tal);                     /* scoped: the opener */
    assert(copy(grp+GROUP_FRONT+GROUP_SLOT)==tal);          /* other handles untouched */
    assert(!scope_begin(tal));                              /* no nesting */
    remote_use_scope_thread=GetCurrentThreadId()+1;         /* another thread: native */
    assert(copy(grp+GROUP_FRONT)==ailish);
    remote_use_scope_thread=GetCurrentThreadId();
    assert(controller_skips()==GetCurrentThreadId());       /* skipped (RET 4) inside the scope */
    scope_end();
    assert(copy(grp+GROUP_FRONT)==ailish);
    assert(!scope_begin(ailish));                           /* the front itself needs no scope */
    assert(!remote_use_scope_thread && !remote_use_scope_front && !remote_use_scope_slot);
    /* Uninstall restores every byte it changed. */
    assert(SudekiMpLanStoryRemoteUseUninstall() && !installed);
    *(void **)(mapped+RVA_GROUP)=*(void **)(pristine+RVA_GROUP);
    assert(!memcmp(mapped,pristine,image_size));
    printf("StoryRemoteUseImageTest: PASS (16 front sites, 3 controller sites, node slot; scoped front copy; restore)\n");
    return 0;
}
