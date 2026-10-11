/* Exact retail seam guards + actual argument adapter on inert ownership.
 * No scene or gameplay is advanced by this test. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/engine/build_identity.c"
#include "../src/engine/sha256.c"
#include "../src/hooks/lan_story_ambient.c"
static BOOL owner_valid=TRUE;
static void *expected_parent,*expected_child;
unsigned SudekiMpLanStoryWorldOwnedRenderers(void **out,unsigned max) { (void)out;(void)max;return 0; }
BOOL SudekiMpLanStoryWorldTrailOwner(void *parent,void *child,unsigned slot) {
    return owner_valid && parent==expected_parent && child==expected_child && slot==1u;
}
BOOL SudekiMpLogResearchEnabled(void) { return FALSE; }
void SudekiMpLogWrite(const char *line) { (void)line; }
void SudekiMpLogFormat(const char *format,...) { (void)format; }
/* Exercise the actual naked shim, with the native return address and an
 * inert continuation instead of running a renderer. Capture the stack and
 * registers before the continuation is allowed to clobber them. */
static void *abi_return __attribute__((used));
static uint32_t abi_result[9] __attribute__((used));
static void __attribute__((naked,noinline,used)) abi_resume(void) {
    __asm__ volatile("cld; pop %edi; pop %esi; pop %ebx; pop %ebp; ret");
}
static void __attribute__((naked,noinline)) abi_continuation(void) {
    __asm__ volatile("mov %eax,_abi_result; mov %ecx,_abi_result+4; mov %edx,_abi_result+8;"
        "mov %ebx,_abi_result+12; mov %ebp,_abi_result+16; mov %esi,_abi_result+20;"
        "mov %edi,_abi_result+24; mov 4(%esp),%eax; mov %eax,_abi_result+28;"
        "mov 8(%esp),%eax; mov %eax,_abi_result+32; ret $8");
}
static void __attribute__((naked,noinline)) abi_invoke(void) {
    __asm__ volatile("push %ebp; push %ebx; push %esi; push %edi;"
        "mov $1,%esi; mov $1,%edi; mov $2,%ecx; mov $3,%eax; mov $4,%edx;"
        "mov $5,%ebx; mov $6,%ebp; push $0; push $7; push _abi_return; jmp _trail_stub");
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


int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[MAX_PATH];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,MAX_PATH));
    base=map_image(path); assert(trail_image_exact(base));
    for(unsigned i=0;i<sizeof(trail_checks)/sizeof(trail_checks[0]);++i) {
        for(unsigned j=0;j<trail_checks[i].size;++j) {
            uint8_t *p=base+trail_checks[i].rva+j; *p^=1;
            assert(!trail_image_exact(base)); *p^=1; assert(trail_image_exact(base));
        }
        printf("PASS %s RVA=%08x retail + every-byte mutation (%u)\n",
            trail_checks[i].name,trail_checks[i].rva,trail_checks[i].size);
    }
    void **method=(void **)(base+0x2dfb14u+8u); *method+=1;
    assert(!trail_image_exact(base)); *method-=1;
    expected_parent=(void *)1;expected_child=(void *)2;trail_hook.installed=TRUE;
    SudekiMpLanStoryAmbientSetActive(TRUE);
    frame_valid=TRUE;frame_seen=*(uint16_t *)(base+FRAME_STAMP);
    float dt=.025f;memcpy(&frame_dt_bits,&dt,4); uint32_t value=0;
    trail_delta(expected_parent,expected_child,1,base+0x222a68,&value);assert(value==frame_dt_bits);
    value=0;owner_valid=FALSE;
    trail_delta(expected_parent,expected_child,1,base+0x222a68,&value);assert(!value);owner_valid=TRUE;
    trail_delta(expected_parent,expected_child,0,base+0x222a68,&value);assert(!value);
    trail_delta(expected_parent,expected_child,1,base+0x222a69,&value);assert(!value);
    active_thread^=1;trail_delta(expected_parent,expected_child,1,base+0x222a68,&value);assert(!value);active_thread^=1;
    value=0x3c800000;trail_delta(expected_parent,expected_child,1,base+0x222a68,&value);assert(value==0x3c800000);
    value=0;SudekiMpLanStoryAmbientSetActive(FALSE);
    trail_delta(expected_parent,expected_child,1,base+0x222a68,&value);assert(!value);
    puts("PASS exact owner/caller/thread/zero-delta/activity restrictions (synthetic; no rendering)");
    SudekiMpLanStoryAmbientSetActive(TRUE); trail_trampoline=(void *)(uintptr_t)abi_continuation;
    abi_return=base+0x222a68;
    uint8_t saved_return[5]; memcpy(saved_return,abi_return,5);
    uint32_t jump=(uint32_t)(uintptr_t)abi_resume-(uint32_t)(uintptr_t)abi_return-5u;
    *(uint8_t *)abi_return=0xe9; memcpy((uint8_t *)abi_return+1,&jump,4);
    assert(FlushInstructionCache(GetCurrentProcess(),abi_return,5));
    abi_invoke();
    const uint32_t expected[9]={3,2,4,5,6,1,1,7,frame_dt_bits};
    assert(!memcmp(abi_result,expected,sizeof(expected)));
    owner_valid=FALSE; abi_invoke(); assert(!abi_result[8]);
    memcpy(abi_return,saved_return,5);
    puts("PASS actual trail shim register/stack ABI, delta replacement and refusal");
    VirtualFree(base,0,MEM_RELEASE);return 0;
}
