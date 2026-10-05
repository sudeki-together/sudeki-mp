/* Exact saved-story task hooks with synthetic constructor/step consumers.
 * No native task, world, skill or scheduler executes. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/hooks/lan_story_task_trace.c"
static uint8_t *mapped;
static size_t image_size;
void SudekiMpLogWrite(const char *s) { (void)s; }
void SudekiMpLogFormat(const char *s,...) { (void)s; }
unsigned SudekiMpLobbyGameplayStoryExitStatus(void) { return 0; }
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

static unsigned made,stepped;
static void **expected_cell;
static void created(uint32_t hash,void **cell) {
    assert(hash==0x1234u && cell==expected_cell && callbacks==1);
    assert(!SudekiMpLanStoryTaskHostDetach(created,cast_step));
    ++made; SetLastError(99);
}
static int __attribute__((fastcall)) native_fixture(void *thread,void *edx) {
    assert(thread==(void *)0x1234 && edx==(void *)0x5678);
    ++stepped; SetLastError(77); return 2;
}
static int __attribute__((fastcall)) step_fixture(void *thread,void *edx) {
    assert(callbacks==1 && current_thread==thread);
    assert(!SudekiMpLanStoryTaskHostDetach(created,step_fixture));
    return native_fixture(thread,edx);
}
static DWORD WINAPI foreign(void *unused) {
    (void)unused;
    assert(!SudekiMpLanStoryTaskHostExact((HMODULE)mapped));
    assert(!SudekiMpLanStoryTaskHostDetach(created,step_fixture)); return 0;
}
static BOOL no_owner(void *actor,uint8_t kind,uint64_t *session,uint8_t *type) {
    (void)actor; (void)kind; (void)session; (void)type; return FALSE;
}
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024)); mapped=map_image(path);
    assert(SudekiMpLanStoryTaskTraceInstall((HMODULE)mapped));
    assert(!SudekiMpLanStoryTaskHostExact((HMODULE)mapped)); /* no native thread yet */
    native_thread=GetCurrentThreadId();
    assert(SudekiMpLanStoryTaskHostExact((HMODULE)mapped));
    assert(SudekiMpLanStoryTaskHostAttach((HMODULE)mapped,created,step_fixture));
    assert(!SudekiMpLanStoryTaskHostAttach((HMODULE)mapped,created,step_fixture));
    void *cell=NULL; expected_cell=&cell; original_step=native_fixture;
    callbacks=1; SetLastError(55);
    story_created(0x1234,NULL,&cell);
    assert(!callbacks && made==1 && GetLastError()==55);
    assert(story_step((void *)0x1234,(void *)0x5678)==2 && stepped==1 && GetLastError()==77);
    assert(!callbacks && !current_thread);
    assert(!SudekiMpLanStoryTaskTraceUninstall() && installed && cast_created && cast_step);
    HANDLE worker=CreateThread(NULL,0,foreign,NULL,0,NULL); assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0); CloseHandle(worker);
    assert(SudekiMpLanStoryTaskHostDetach(created,step_fixture));
    assert(story_step((void *)0x1234,(void *)0x5678)==2 && stepped==2);
    /* Real cast lineage installer shares the existing four story sites. */
    const SudekiMpLanCastTaskHost host={SudekiMpLanStoryTaskHostExact,
        SudekiMpLanStoryTaskHostAttach,SudekiMpLanStoryTaskHostDetach};
    uint8_t calls[4][5]; const unsigned sites[]={CREATE_DIRECT,CREATE_CHILD,STEP_IMMEDIATE,STEP_SCHEDULED};
    for(unsigned i=0;i<4;++i) memcpy(calls[i],mapped+sites[i],5);
    assert(SudekiMpInstallLanCastContextWithTaskHost((HMODULE)mapped,no_owner,&host));
    assert(SudekiMpLanCastContextPoll());
    for(unsigned i=0;i<4;++i) assert(!memcmp(calls[i],mapped+sites[i],5));
    assert(!SudekiMpLanStoryTaskTraceUninstall());
    assert(SudekiMpUninstallLanCastContext() && !cast_created && !cast_step);
    for(unsigned i=0;i<4;++i) assert(!memcmp(calls[i],mapped+sites[i],5));
    assert(SudekiMpLanStoryTaskTraceUninstall());
    VirtualFree(mapped,0,MEM_RELEASE);
    puts("story shared task owner exact-image/synthetic dispatch, coexistence and teardown tests passed (no gameplay)");
    return 0;
}
