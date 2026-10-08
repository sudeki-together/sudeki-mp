/* Supported-image validation plus isolated gameplay native ownership fixture.
 * Native creation/COM calls are substituted; this does not load/draw assets. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "engine/build_identity.h"
#include "../src/hooks/lan_story_avatar_portrait.c"
static size_t image_size;
void SudekiMpLogFormat(const char *s,...) { (void)s; }
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

static uint8_t backend[0x3c],token[0x18],record[0x1c],job[0x14];
static void *resident;
static IDirect3DTexture9 texture;
static IDirect3DDevice9 device,other_device;
static IDirect3DTexture9Vtbl texture_vtable;
static IDirect3DDevice9Vtbl device_vtable;
static SudekiMpLanStoryAvatarSpawnObservation observations[4];
static unsigned creates,waits,destroys,device_gets,releases,observe_calls;
static BOOL wrong_device,retire_during_wait;
static void *const world=(void *)0x100;
BOOL SudekiMpLanStoryAvatarSpawnObserve(unsigned player,uint32_t epoch,uint32_t generation,
    SudekiMpLanStoryAvatarSpawnObservation *out) {
    ++observe_calls;
    assert(player<4 && out);
    *out=observations[player];
    return out->epoch==epoch && out->generation==generation;
}
static ULONG STDMETHODCALLTYPE device_release(IDirect3DDevice9 *self) {
    assert(self==&device || self==&other_device); ++releases; return 1;
}
static HRESULT STDMETHODCALLTYPE texture_device(IDirect3DTexture9 *self,IDirect3DDevice9 **out) {
    assert(self==&texture); ++device_gets; *out=wrong_device?&other_device:&device; return S_OK;
}
static D3DRESOURCETYPE STDMETHODCALLTYPE texture_type(IDirect3DTexture9 *self) {
    assert(self==&texture); return D3DRTYPE_TEXTURE;
}
static HRESULT STDMETHODCALLTYPE texture_desc(IDirect3DTexture9 *self,UINT level,D3DSURFACE_DESC *out) {
    assert(self==&texture && !level); memset(out,0,sizeof(*out)); out->Width=out->Height=128; return S_OK;
}
static void * __attribute__((cdecl)) create_fixture(const TextureName *name,unsigned flags,
    unsigned residency,void **head,void *callback,unsigned tag) {
    assert(!resident && name->identifier==UINT32_MAX &&
        !strcmp(name->name,"SUI_PORTRAIT_BOSS_TALOSMERGED"));
    assert(flags==0x20 && residency==0x80 && head==&portrait.pending && !*head && !callback && !tag);
    assert(!SudekiMpLanStoryAvatarPortraitRelease()); ++creates;
    resident=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE); assert(resident);
    *(void **)resident=base+RESIDENT_VT; *(void **)((uint8_t *)resident+4)=backend;
    memset(backend,0,sizeof(backend)); *(void **)(backend+4)=&texture; *(uint16_t *)(backend+0x24)=1;
    memset(token,0,sizeof(token)); *(void **)token=base+TOKEN_VT;
    *(void **)(token+0xc)=head; *(void **)(token+0x10)=job; *(void **)(token+0x14)=record;
    memset(record,0,sizeof(record)); *(void **)record=token; *(void **)(record+4)=head;
    *(void **)(record+8)=resident; *(uint32_t *)(record+0x14)=0x20000002;
    *(void **)(job+0x10)=backend; *head=token; return resident;
}
static void __attribute__((thiscall)) wait_fixture(void *self) {
    assert(self==token && pending_exact() && !portrait.ready); ++waits;
    portrait.pending=NULL; memset(token,0xdd,sizeof(token));
    if(retire_during_wait) *(void **)(base+WORLD)=NULL;
}
static void * __attribute__((thiscall)) destroy_fixture(void *self,unsigned flags) {
    assert(flags==1 && self==resident && resident_exact() && !portrait.pending);
    ++destroys; assert(VirtualFree(self,0,MEM_RELEASE)); resident=NULL; return self;
}
static void fixture(void) {
    *(void **)(base+WORLD)=world; *(void **)(base+DEVICE)=&device; base[TEXTURES_ACTIVE]=1;
    device_vtable.Release=device_release; device.lpVtbl=other_device.lpVtbl=&device_vtable;
    texture_vtable.GetDevice=texture_device; texture_vtable.GetType=texture_type;
    texture_vtable.GetLevelDesc=texture_desc; texture.lpVtbl=&texture_vtable;
    native_create=create_fixture; native_wait=wait_fixture; native_destroy=destroy_fixture;
    for(unsigned p=0;p<4;++p) observations[p]=(SudekiMpLanStoryAvatarSpawnObservation){
        .seat=p,.world=world,.epoch=22,.generation=p+1,.actor=(void *)(uintptr_t)(0x200+p),.ready=TRUE};
}
static BOOL service(unsigned p) {
    return SudekiMpLanStoryAvatarPortraitService((HMODULE)base,world,22,p,p+1,
        (void *)(uintptr_t)(0x200+p),&device);
}
static void *resolve(unsigned p) {
    return SudekiMpLanStoryAvatarPortraitResolve((HMODULE)base,world,22,p,p+1,
        (void *)(uintptr_t)(0x200+p),&device);
}
static DWORD WINAPI foreign_thread(void *ignored) {
    (void)ignored; assert(!service(0) && !resolve(0) && !SudekiMpLanStoryAvatarPortraitRelease()); return 0;
}
int wmain(int argc,wchar_t **argv) {
    assert(argc==2); uint8_t *image=map_image(argv[1]);
    assert(!SudekiMpLanStoryAvatarPortraitService((HMODULE)image,world,22,0,1,(void *)0x200,&device));
    assert(!base && !native_thread && !creates); /* No ready spawn/thread proof. */
    assert(initialize((HMODULE)image)); fixture();
    observations[0].ready=FALSE; assert(!service(0) && !creates); observations[0].ready=TRUE;
    observations[0].unknown=TRUE; assert(!service(0) && !creates); observations[0].unknown=FALSE;
    observations[0].generation++; assert(!service(0) && !creates); observations[0].generation--;
    assert(service(0) && creates==1 && waits==1 && device_gets==releases);
    assert(SudekiMpLanStoryAvatarPortraitRetains());
    /* The title provider has a completely separate instance and release path. */
    assert(!SudekiMpTitlePortraitsRetains() && SudekiMpTitlePortraitsRelease());
    assert(SudekiMpLanStoryAvatarPortraitRetains() && resident);
    for(unsigned p=0;p<4;++p) assert(service(p) && resolve(p)==&texture);
    assert(creates==1 && waits==1);
    observations[2].actor=(void *)0xdead; assert(!resolve(2)); observations[2].actor=(void *)0x202;
    observations[1].world=(void *)0xdead; assert(!resolve(1)); observations[1].world=world;
    observations[3].unknown=TRUE; assert(!resolve(3)); observations[3].unknown=FALSE;
    assert(!SudekiMpLanStoryAvatarPortraitResolve((HMODULE)base,world,23,0,1,(void *)0x200,&device));
    wrong_device=TRUE; assert(!resolve(0)); wrong_device=FALSE; assert(device_gets==releases);
    HANDLE thread=CreateThread(NULL,0,foreign_thread,NULL,0,NULL); assert(thread);
    assert(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0); CloseHandle(thread);
    *(void **)(base+DEVICE)=&other_device;
    assert(!resolve(0) && !SudekiMpLanStoryAvatarPortraitRelease() && !destroys);
    *(void **)(base+DEVICE)=&device;
    base[DESTROY]^=1; assert(!service(0) && !SudekiMpLanStoryAvatarPortraitRelease()); base[DESTROY]^=1;
    *(void **)resident=NULL; assert(!SudekiMpLanStoryAvatarPortraitRelease()); *(void **)resident=base+RESIDENT_VT;
    /* Opaque retired world/actor addresses are never dereferenced by Release. */
    *(void **)(base+WORLD)=NULL; assert(!resolve(0));
    unsigned before=observe_calls; assert(SudekiMpLanStoryAvatarPortraitRelease());
    assert(observe_calls==before && destroys==1 && !SudekiMpLanStoryAvatarPortraitRetains());
    fixture(); retire_during_wait=TRUE;
    assert(!service(0) && creates==2 && waits==2 && SudekiMpLanStoryAvatarPortraitRetains());
    before=observe_calls; assert(SudekiMpLanStoryAvatarPortraitRelease());
    assert(observe_calls==before && destroys==2); retire_during_wait=FALSE;
    fixture(); assert(service(0) && resolve(0)==&texture);
    assert(SudekiMpLanStoryAvatarPortraitRelease() && destroys==3 && device_gets==releases);
    assert(!SudekiMpTitlePortraitsRetains() && VirtualFree(image,0,MEM_RELEASE));
    puts("avatar portrait supported-image and independent ownership fixture PASS"); return 0;
}
