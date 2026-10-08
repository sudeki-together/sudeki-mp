/* Supported-image signatures plus fixture native/COM ownership calls.
 * Does not load real assets or establish visual acceptance. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/hooks/title_portraits.c"
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


static uint8_t owner[0x48],scene[0x174],backend[COUNT][0x3c];
static uint8_t token[COUNT][0x18],record[COUNT][0x1c],job[COUNT][0x14];
static void *resident[COUNT];
static IDirect3DTexture9 textures[COUNT];
static IDirect3DDevice9 device,other_device;
static IDirect3DTexture9Vtbl texture_vtable;
static IDirect3DDevice9Vtbl device_vtable;
static unsigned creates,waits,destroys,device_gets,releases;
static BOOL fail_gpu,wrong_device;
static ULONG STDMETHODCALLTYPE device_release(IDirect3DDevice9 *self) {
    assert(self==&device || self==&other_device); ++releases; return 1;
}
static HRESULT STDMETHODCALLTYPE texture_device(IDirect3DTexture9 *self,IDirect3DDevice9 **out) {
    assert(self>=textures && self<textures+COUNT); ++device_gets;
    *out=wrong_device?&other_device:&device; return S_OK;
}
static D3DRESOURCETYPE STDMETHODCALLTYPE texture_type(IDirect3DTexture9 *self) {
    (void)self; return fail_gpu?D3DRTYPE_SURFACE:D3DRTYPE_TEXTURE;
}
static HRESULT STDMETHODCALLTYPE texture_desc(IDirect3DTexture9 *self,UINT level,D3DSURFACE_DESC *out) {
    (void)self; assert(level==0); memset(out,0,sizeof(*out)); out->Width=out->Height=128; return S_OK;
}
static void * __attribute__((cdecl)) create_fixture(const TextureName *name,unsigned flags,
    unsigned residency,void **head,void *callback,unsigned tag) {
    unsigned i=creates++%COUNT;
    static const char *const expected_files[]={
        "SUI_PORTRAIT_BUKI.sqx","SUI_PORTRAIT_ELCO.sqx","SUI_PORTRAIT_TAL.sqx",
        "SUI_PORTRAIT_AILISH.sqx","SUI_PORTRAIT_BOSS_TALOSMERGED.sqx"};
    char actual_file[128];
    assert(name->identifier==UINT32_MAX && !strchr(name->name,'.'));
    assert(snprintf(actual_file,sizeof(actual_file),"%s.sqx",name->name)>0);
    assert(!strcmp(actual_file,expected_files[i]));
    assert(flags==0x20 && residency==0x80 && head==&portraits[i].pending && !callback && !tag);
    assert(!*head && !portraits[i].resident);
    resident[i]=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE); assert(resident[i]);
    *(void **)resident[i]=base+RESIDENT_VT; *(void **)((uint8_t *)resident[i]+4)=backend[i];
    memset(backend[i],0,0x3c); *(void **)(backend[i]+4)=&textures[i]; *(uint16_t *)(backend[i]+0x24)=1;
    memset(token[i],0,0x18); *(void **)token[i]=base+TOKEN_VT;
    *(void **)(token[i]+0xc)=head; *(void **)(token[i]+0x10)=job[i]; *(void **)(token[i]+0x14)=record[i];
    memset(record[i],0,0x1c); *(void **)record[i]=token[i]; *(void **)(record[i]+4)=head;
    *(void **)(record[i]+8)=resident[i]; *(uint32_t *)(record[i]+0x14)=0x20000002;
    *(void **)(job[i]+0x10)=backend[i]; *head=token[i]; return resident[i];
}
static void __attribute__((thiscall)) wait_fixture(void *self) {
    unsigned i=(unsigned)((uint8_t *)self-(uint8_t *)token)/sizeof(token[0]); assert(i<COUNT);
    assert(pending_exact(&portraits[i]) && !portraits[i].ready); ++waits;
    portraits[i].pending=NULL;
    memset(token[i],0xdd,sizeof(token[i])); /* Deleted token must never be reread. */
}
static void * __attribute__((thiscall)) destroy_fixture(void *self,unsigned flags) {
    assert(flags==1); unsigned found=COUNT;
    for(unsigned i=0;i<COUNT;++i) if(resident[i]==self) { found=i; break; }
    assert(found<COUNT && resident_exact(&portraits[found]) && !portraits[found].pending);
    ++destroys; assert(VirtualFree(self,0,MEM_RELEASE)); resident[found]=NULL; return self;
}
static void fixture(void) {
    *(void **)owner=base+TITLE_VT; *(uint32_t *)(owner+0x44)=5;
    *(void **)(scene+0x170)=owner; *(void **)(base+SCENE)=scene;
    *(void **)(base+DEVICE)=&device; base[TEXTURES_ACTIVE]=1;
    device_vtable.Release=device_release; device.lpVtbl=other_device.lpVtbl=&device_vtable;
    texture_vtable.GetDevice=texture_device; texture_vtable.GetType=texture_type;
    texture_vtable.GetLevelDesc=texture_desc;
    for(unsigned i=0;i<COUNT;++i) textures[i].lpVtbl=&texture_vtable;
    native_create=create_fixture; native_wait=wait_fixture; native_destroy=destroy_fixture;
}
static DWORD WINAPI foreign_thread(void *ignored) {
    (void)ignored; assert(!SudekiMpTitlePortraitsService((HMODULE)base,owner,scene,&device));
    assert(!SudekiMpTitlePortraitsResolve((HMODULE)base,owner,scene,&device,5));
    assert(!SudekiMpTitlePortraitsRelease()); return 0;
}
int wmain(int argc,wchar_t **argv) {
    assert(argc==2); uint8_t *image=map_image(argv[1]);
    assert(SudekiMpTitlePortraitsImageMatches((HMODULE)image));
    const unsigned regions[]={CREATE,DESTROY,WAIT,0x1d6a90,0x1d7000,0x1d7320,0x15c0e0,
        0x2d5708,RESIDENT_VT,TOKEN_VT+4};
    for(unsigned i=0;i<sizeof(regions)/sizeof(regions[0]);++i) {
        image[regions[i]]^=1; assert(!SudekiMpTitlePortraitsImageMatches((HMODULE)image)); image[regions[i]]^=1;
    }
    assert(initialize((HMODULE)image)); fixture();
    *(uint32_t *)(owner+0x44)=6;
    assert(!SudekiMpTitlePortraitsService((HMODULE)image,owner,scene,&device) && !creates);
    *(uint32_t *)(owner+0x44)=5;
    assert(SudekiMpTitlePortraitsService((HMODULE)image,owner,scene,&device));
    assert(creates==5 && waits==5 && !destroys && device_gets==releases && SudekiMpTitlePortraitsRetains());
    for(unsigned c=0;c<4;++c) assert(SudekiMpTitlePortraitsResolve((HMODULE)image,owner,scene,&device,c)==&textures[c]);
    assert(SudekiMpTitlePortraitsResolve((HMODULE)image,owner,scene,&device,5)==&textures[4]);
    assert(!SudekiMpTitlePortraitsResolve((HMODULE)image,owner,scene,&device,4));
    assert(!SudekiMpTitlePortraitsResolve((HMODULE)image,owner,scene,&device,99));
    assert(SudekiMpTitlePortraitsService((HMODULE)image,owner,scene,&device) && creates==5);
    fail_gpu=TRUE; assert(!SudekiMpTitlePortraitsResolve((HMODULE)image,owner,scene,&device,5)); fail_gpu=FALSE;
    wrong_device=TRUE; assert(!SudekiMpTitlePortraitsResolve((HMODULE)image,owner,scene,&device,5)); wrong_device=FALSE;
    assert(device_gets==releases);
    *(void **)(scene+0x170)=NULL; assert(!SudekiMpTitlePortraitsResolve((HMODULE)image,owner,scene,&device,5));
    *(void **)(scene+0x170)=owner;
    HANDLE thread=CreateThread(NULL,0,foreign_thread,NULL,0,NULL); assert(thread);
    assert(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0); CloseHandle(thread);
    *(void **)(base+DEVICE)=&other_device; assert(!SudekiMpTitlePortraitsRelease() && destroys==0);
    *(void **)(base+DEVICE)=&device;
    base[DESTROY]^=1;
    assert(!SudekiMpTitlePortraitsRelease() && destroys==0);
    assert(!SudekiMpTitlePortraitsService((HMODULE)image,owner,scene,&device));
    base[DESTROY]^=1;
    *(void **)resident[0]=NULL; assert(!SudekiMpTitlePortraitsRelease() && SudekiMpTitlePortraitsRetains());
    *(void **)resident[0]=base+RESIDENT_VT;
    /* Title is already retired: release must not inspect owner/scene. */
    title_owner=(void *)1; title_scene=(void *)1;
    assert(SudekiMpTitlePortraitsRelease() && destroys==5 && !SudekiMpTitlePortraitsRetains());
    assert(SudekiMpTitlePortraitsRelease());
    assert(SudekiMpTitlePortraitsService((HMODULE)image,owner,scene,&device) && creates==10 && waits==10);
    assert(SudekiMpTitlePortraitsRelease() && destroys==10 && device_gets==releases);
    assert(VirtualFree(image,0,MEM_RELEASE)); puts("title portraits exact-image and native ownership fixture PASS"); return 0;
}
