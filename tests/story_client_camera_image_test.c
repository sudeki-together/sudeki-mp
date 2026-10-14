#include "engine/build_identity.h"
#include "hooks/lan_story_client_camera.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Exact-image guards of the client native camera (#71): every checked byte of
 * the camera update, input event, SetCameraState/CanSetCameraState entries and
 * the node vtable slot refuses mutation; a malformed lease never ticks. */
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
int main(int argc,char **argv) {
    assert(argc==2);wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[1],-1,path,1024));
    uint8_t *image=map_image(path);
    assert(SudekiMpLanStoryClientCameraInstall((HMODULE)image));
    assert(!SudekiMpLanStoryClientCameraInstall((HMODULE)image)); /* once */
    assert(SudekiMpLanStoryClientCameraUninstall());
    const struct {const char *name;unsigned rva,count;} guards[]={
        {"CameraNodeUpdate",0xe7660u,12u},{"CameraInputEvent",0xe85f0u,12u},
        {"SetCameraState",0x37530u,12u},{"CanSetCameraState",0x37c30u,12u},{"NodeVtableUpdateSlot",0x2cce70u,4u}};
    for(unsigned i=0;i<sizeof(guards)/sizeof(*guards);++i) {
        for(unsigned j=0;j<guards[i].count;++j) {
            image[guards[i].rva+j]^=1u;
            assert(!SudekiMpLanStoryClientCameraInstall((HMODULE)image));
            image[guards[i].rva+j]^=1u;
            assert(SudekiMpLanStoryClientCameraInstall((HMODULE)image) && SudekiMpLanStoryClientCameraUninstall());
        }
        printf("native guard %s: supported bytes pass, every checked byte mutation refused\n",guards[i].name);
    }
    assert(SudekiMpLanStoryClientCameraInstall((HMODULE)image));
    /* A lease that is not the exact default camera never reaches native code. */
    SudekiMpLanArenaOwnerViewLease lease={0};
    assert(!SudekiMpLanStoryClientCameraTick(&lease,0.016f,0,0));
    assert(!SudekiMpLanStoryClientCameraTick(NULL,0.016f,0,0));
    uint8_t camera[0x110]={0},render[0xe0]={0},manager[0x60]={0};
    *(void **)camera=image+0x2cce5cu;*(void **)(camera+8)=image+0x2cce6cu;*(void **)(camera+0x34)=render;
    *(void **)render=image+0x2dd638u;*(void **)manager=image+0x2c7b80u;*(void **)(manager+0x20)=camera;
    lease.valid=TRUE;lease.camera=camera;lease.render_state=render;
    *(void **)(image+0x409d7cu)=manager;
    /* Not in Exploration and not named "default": refused before any state request. */
    assert(!SudekiMpLanStoryClientCameraTick(&lease,0.016f,0,0));
    strcpy((char *)(camera+0x4c),"other");
    assert(!SudekiMpLanStoryClientCameraTick(&lease,0.016f,0,0));
    assert(SudekiMpLanStoryClientCameraUninstall());
    puts("story client camera exact-image guards and lease refusals passed (no native tick)");
    VirtualFree(image,0,MEM_RELEASE);return 0;
}
