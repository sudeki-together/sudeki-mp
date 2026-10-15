#include "engine/build_identity.h"
#include "hooks/lan_story_main_menu.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Exact-image guards of the M-menu scope adapter (#75): the character-cycle
 * entry and both of its call sites, the Weapons equip call site and the menu
 * open vtable slot refuse mutation; install/restore leaves every patched byte
 * identical; no callback is required to install. */
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size);assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size);CloseHandle(file);
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
static BOOL allow_all(void *actor) { (void)actor; return TRUE; }
int main(int argc,char **argv) {
    assert(argc==2);wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[1],-1,path,1024));
    uint8_t *image=map_image(path);
    const struct {const char *name;unsigned rva,count;} seams[]={
        {"CycleEntry",0x96300u,13u},{"CycleNextCall",0x93b24u,5u},{"CyclePreviousCall",0x93b47u,5u},
        {"EquipCall",0x16f1a4u,5u},{"MenuOpenEntry",0x956d0u,12u},{"MenuOpenSlot",0x2cae4cu,4u},
        {"OpenPauseCall",0x957f5u,5u},{"ClosePauseCall",0x95a14u,5u},
        {"OpenMoveCall",0x956f9u,5u},{"CloseMoveCall",0x959ffu,5u},{"UseCall",0x17761du,5u},
        {"ListConfirmCall",0x177166u,5u}};
    enum { SEAMS=12 };
    uint8_t before[SEAMS][13];
    for(unsigned i=0;i<SEAMS;++i) memcpy(before[i],image+seams[i].rva,seams[i].count);
    assert(SudekiMpLanStoryMainMenuInstall((HMODULE)image,allow_all,NULL,NULL));
    assert(!SudekiMpLanStoryMainMenuInstall((HMODULE)image,allow_all,NULL,NULL)); /* once */
    /* Installed: both cycle calls and the equip call were redirected, the open slot replaced. */
    assert(memcmp(image+0x93b25u,before[1]+1,4u) && memcmp(image+0x93b48u,before[2]+1,4u) &&
        memcmp(image+0x16f1a5u,before[3]+1,4u) && memcmp(image+0x2cae4cu,before[5],4u) &&
        memcmp(image+0x957f6u,before[6]+1,4u) && memcmp(image+0x95a15u,before[7]+1,4u) &&
        memcmp(image+0x956fau,before[8]+1,4u) && memcmp(image+0x95a00u,before[9]+1,4u) &&
        memcmp(image+0x17761eu,before[10]+1,4u) && memcmp(image+0x177167u,before[11]+1,4u));
    assert(image[0x93b24u]==0xe8u && image[0x93b47u]==0xe8u && image[0x16f1a4u]==0xe8u && image[0x177166u]==0xe8u);
    unsigned redirected=1,requests=1; SudekiMpLanStoryMainMenuCounters(&redirected,&requests);
    assert(!redirected && !requests);
    assert(SudekiMpLanStoryMainMenuUninstall());
    for(unsigned i=0;i<SEAMS;++i) assert(!memcmp(before[i],image+seams[i].rva,seams[i].count));
    for(unsigned i=0;i<sizeof(seams)/sizeof(*seams);++i) {
        for(unsigned j=0;j<seams[i].count;++j) {
            image[seams[i].rva+j]^=1u;
            assert(!SudekiMpLanStoryMainMenuInstall((HMODULE)image,NULL,NULL,NULL));
            image[seams[i].rva+j]^=1u;
            assert(SudekiMpLanStoryMainMenuInstall((HMODULE)image,NULL,NULL,NULL) && SudekiMpLanStoryMainMenuUninstall());
        }
    }
    for(unsigned i=0;i<SEAMS;++i) assert(!memcmp(before[i],image+seams[i].rva,seams[i].count));
    printf("story main menu image guards ok\n");
    return 0;
}
