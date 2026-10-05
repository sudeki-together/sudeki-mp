/* Inert, hash-checked retail image and synthetic material/job graphs.
 * Exercises production validation and a test-owned Win32 critical section;
 * no retail resource, renderer, task, callback or gameplay method executes. */
#include "../src/hooks/lan_story_material.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *mapped;
static uint8_t child[0x2c],resource[0x30],manager[8],materials[2][0x40],description[0x14];
static uint8_t wrappers[2][8],backing_data[0x3c],jobs[2][0x1c],foreign_data[0x1000];
static void *originals[1],*overrides[1],*textures[2][1],*companion[2][1];
static uint8_t override_flags[1];
static CRITICAL_SECTION *queue_lock;
static HANDLE locked,release_lock;
void SudekiMpLogFormat(const char *format,...) { (void)format; }
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size); CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;
    IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *base=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
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
    free(raw); return base;
}

static void put(void *p,unsigned offset,void *value) { memcpy((uint8_t *)p+offset,&value,4); }
static void u32(void *p,unsigned offset,uint32_t value) { memcpy((uint8_t *)p+offset,&value,4); }
static void setup(BOOL with_texture,BOOL with_override) {
    memset(child,0,sizeof(child)); memset(resource,0,sizeof(resource)); memset(manager,0,sizeof(manager));
    memset(materials,0,sizeof(materials)); memset(description,0,sizeof(description));
    memset(wrappers,0,sizeof(wrappers)); memset(backing_data,0,sizeof(backing_data));
    memset(jobs,0,sizeof(jobs)); memset(mapped+0x363ee8,0,32*4);
    mapped[0x3c3137]=1;
    put(child,0,mapped+0x2dec74); put(child,8,mapped+0x2dedbc);
    put(child,0xc,mapped+0x2dedd8); put(child,0x10,resource); put(resource,0x2c,manager);
    if(!with_texture) return;
    u32(manager,0,1); put(manager,4,originals); originals[0]=materials[0];
    u32(description,4,with_override?2:1); u32(description,0xc,1);
    put(backing_data,4,foreign_data); u32(backing_data,0x20,with_override?0x20000:0x10000);
    u32(backing_data,0x24,with_override?2:1);
    for(unsigned i=0;i<2;++i) {
        put(materials[i],0,mapped+0x2deb7c); put(materials[i],4,description);
        put(materials[i],8,textures[i]); put(materials[i],0x10,companion[i]); u32(materials[i],0x14,1);
        textures[i][0]=wrappers[i]; put(wrappers[i],0,mapped+0x2dd80c); put(wrappers[i],4,backing_data);
    }
    if(with_override) {
        overrides[0]=materials[1]; override_flags[0]=2;
        put(child,0x20,overrides); put(child,0x24,override_flags);
    }
}
static void *foreign_bucket(unsigned bucket) {
    for(unsigned i=0;i<sizeof(foreign_data);++i)
        if(resource_bucket(foreign_data+i)==bucket) return foreign_data+i;
    assert(0); return NULL;
}
static void job(unsigned index,void *backing_pointer) {
    uint8_t *node=jobs[index]; memset(node,0,0x1c);
    put(node,0,mapped+0x2dd7ac); u32(node,0xc,1); put(node,0x10,backing_pointer);
    unsigned bucket=resource_bucket(backing_pointer);
    put(node,0x18,*(void **)(mapped+0x363ee8+bucket*4));
    put(mapped,0x363ee8+bucket*4,node);
}
static BOOL check(void) {
    return SudekiMpLanStoryMaterialOwnerExact((HMODULE)mapped,child,resource);
}
static void refused(DWORD error) {
    assert(!check()); assert(GetLastError()==error);
    /* Every inspection releases its borrowed recursion level. */
    assert(queue_lock->RecursionCount==0);
}
static DWORD WINAPI hold_lock(void *unused) {
    (void)unused; EnterCriticalSection(queue_lock); assert(SetEvent(locked));
    assert(WaitForSingleObject(release_lock,5000)==WAIT_OBJECT_0);
    LeaveCriticalSection(queue_lock); return 0;
}
static DWORD WINAPI foreign_thread(void *unused) {
    (void)unused; assert(!check());
    assert(!SudekiMpLanStoryMaterialInitialize((HMODULE)mapped)); return 0;
}
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024)); mapped=map_image(path);
    /* New synchronization/layout evidence participates in exact-build gating. */
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i) {
        mapped[codes[i].rva]^=1;
        assert(!SudekiMpLanStoryMaterialInitialize((HMODULE)mapped));
        assert(GetLastError()==ERROR_BAD_EXE_FORMAT && !verified_image);
        mapped[codes[i].rva]^=1;
    }
    assert(SudekiMpLanStoryMaterialInitialize((HMODULE)mapped));
    queue_lock=(CRITICAL_SECTION *)(mapped+0x364148); InitializeCriticalSection(queue_lock);
    setup(FALSE,FALSE); assert(check());
    /* Empty graphs depend on no queue; textured graphs ignore unrelated jobs,
     * including a valid node in exactly the same hash bucket. */
    for(unsigned i=0;i<32;++i) {
        setup(FALSE,FALSE); job(0,foreign_bucket(i)); assert(check());
        setup(TRUE,FALSE); job(0,foreign_bucket(i)); assert(check());
        assert(queue_lock->RecursionCount==0);
    }
    setup(TRUE,TRUE); assert(check()); /* retirement preserves retained original */
    uint8_t saved_backing[sizeof(backing_data)],saved_materials[sizeof(materials)];
    memcpy(saved_backing,backing_data,sizeof(backing_data)); memcpy(saved_materials,materials,sizeof(materials));
    for(unsigned i=0;i<1000;++i) assert(check());
    assert(!memcmp(saved_backing,backing_data,sizeof(backing_data)));
    assert(!memcmp(saved_materials,materials,sizeof(materials)));
    setup(TRUE,FALSE); job(0,backing_data); refused(ERROR_IO_PENDING);
    /* Search past a hash collision to find an owned pending callback. */
    job(1,foreign_bucket(resource_bucket(backing_data))); refused(ERROR_IO_PENDING);
    setup(TRUE,FALSE); u32(backing_data,0x20,0x14000); refused(ERROR_IO_PENDING);
    setup(TRUE,FALSE); u32(backing_data,0x20,0x11000); refused(ERROR_IO_PENDING);
    setup(TRUE,FALSE); put(backing_data,4,NULL); job(0,backing_data); refused(ERROR_IO_PENDING);
    setup(TRUE,FALSE); assert(check()); /* next fresh inspection recovers */
    setup(TRUE,FALSE); put(backing_data,4,NULL); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); u32(backing_data,0x20,0x12000); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); u32(backing_data,0x20,0); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); u32(backing_data,0x24,0); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); u32(backing_data,0x24,0xffff); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); u32(backing_data,0x20,0x7fff0000); refused(ERROR_INVALID_DATA);
    setup(TRUE,TRUE); u32(backing_data,0x24,1); refused(ERROR_INVALID_DATA);
    setup(TRUE,TRUE); u32(backing_data,0x20,0x10000); refused(ERROR_INVALID_DATA);
    setup(TRUE,TRUE); u32(description,4,1); refused(ERROR_INVALID_DATA);
    setup(TRUE,TRUE); u32(description,4,UINT32_MAX); refused(ERROR_INVALID_DATA);
    setup(TRUE,TRUE); textures[1][0]=wrappers[0]; refused(ERROR_INVALID_DATA);
    setup(TRUE,TRUE); overrides[0]=originals[0]; refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); put(backing_data,0xc,foreign_data); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); mapped[0x3c3137]=0; refused(ERROR_INVALID_STATE);
    setup(TRUE,FALSE); mapped[0x3c3137]=2; refused(ERROR_INVALID_STATE);
    setup(TRUE,FALSE); job(0,foreign_bucket(resource_bucket(backing_data)));
    put(jobs[0],0,NULL); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); job(0,foreign_bucket(resource_bucket(backing_data)));
    put(jobs[0],0x18,jobs[0]); refused(ERROR_INVALID_DATA); /* bounded cycle */
    setup(TRUE,FALSE); job(0,foreign_bucket(resource_bucket(backing_data)));
    put(jobs[0],0x18,(void *)1); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); job(0,foreign_bucket(resource_bucket(backing_data)));
    put(jobs[0],0x10,foreign_bucket((resource_bucket(backing_data)+1)&31)); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE); job(0,foreign_bucket(resource_bucket(backing_data)));
    u32(jobs[0],0xc,0); refused(ERROR_INVALID_DATA);
    setup(TRUE,FALSE);
    locked=CreateEventW(NULL,TRUE,FALSE,NULL); release_lock=CreateEventW(NULL,TRUE,FALSE,NULL);
    assert(locked && release_lock);
    HANDLE worker=CreateThread(NULL,0,hold_lock,NULL,0,NULL); assert(worker);
    assert(WaitForSingleObject(locked,5000)==WAIT_OBJECT_0);
    DWORD start=GetTickCount(); assert(!check() && GetLastError()==ERROR_IO_PENDING);
    assert(GetTickCount()-start<1000); /* cannot wait for the worker */
    assert(SetEvent(release_lock)); assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);
    CloseHandle(worker); CloseHandle(locked); CloseHandle(release_lock); assert(check());
    worker=CreateThread(NULL,0,foreign_thread,NULL,0,NULL); assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0); CloseHandle(worker);
    assert(check() && queue_lock->RecursionCount==0);
    DeleteCriticalSection(queue_lock); VirtualFree(mapped,0,MEM_RELEASE);
    puts("story material exact-image/resource-specific readiness tests passed (no native gameplay)");
    return 0;
}
