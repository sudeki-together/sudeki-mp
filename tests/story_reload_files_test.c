/* Real Win32 file/BCrypt lease test using synthetic, test-created save-shaped
 * files. Only SHGetFolderPath is substituted with a newly created temp root.
 * Native catalog, prior load and lobby-exit evidence are fixtures; no retail
 * code, real user save or game process is opened or executed. */
#include <assert.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "../src/hooks/lan_story_load.c"
static wchar_t fixture_root[MAX_PATH];
static HRESULT WINAPI fixture_appdata(HWND window,int folder,HANDLE token,DWORD flags,LPWSTR out) {
    assert(!window && folder==CSIDL_APPDATA && !token && flags==SHGFP_TYPE_CURRENT);
    wcscpy(out,fixture_root);return S_OK;
}
static FARPROC WINAPI fixture_proc(HMODULE module,LPCSTR name) {
    assert(module && !strcmp(name,"SHGetFolderPathW"));
    HRESULT (WINAPI *fn)(HWND,int,HANDLE,DWORD,LPWSTR)=fixture_appdata;
    FARPROC result;_Static_assert(sizeof(fn)==sizeof(result),"Win32 function pointer");
    memcpy(&result,&fn,sizeof(result));return result;
}
#define GetProcAddress fixture_proc
#include "../src/ui/save_catalog.c"
#undef GetProcAddress
void SudekiMpLogFormat(const char *format,...) {(void)format;}
void SudekiMpLogWrite(const char *text) {(void)text;assert(!"native load must not execute");}
static BOOL drained,active_lobby=TRUE;
static unsigned exit_result;
unsigned SudekiMpLobbyGameplayStoryExitStatus(void) {return exit_result;}
BOOL SudekiMpLobbyGameplayStoryExitDrained(void) {return drained && exit_result==1;}
BOOL SudekiMpLobbyGameplayActive(void) {return active_lobby;}
static void write_fixture(const wchar_t *path,void *bytes,DWORD size) {
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    assert(file!=INVALID_HANDLE_VALUE);DWORD written=0;
    assert(WriteFile(file,bytes,size,&written,NULL) && written==size && CloseHandle(file));
}
static void assert_pinned(const wchar_t *fish,const wchar_t *bunny) {
    const wchar_t *paths[]={fish,bunny};
    for(unsigned i=0;i<2;++i) {
        HANDLE file=CreateFileW(paths[i],GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
        assert(file==INVALID_HANDLE_VALUE && GetLastError()==ERROR_SHARING_VIOLATION);
        assert(!DeleteFileW(paths[i]));
        assert(GetLastError()==ERROR_SHARING_VIOLATION || GetLastError()==ERROR_ACCESS_DENIED);
    }
}
int main(void) {
    wchar_t temp[MAX_PATH],sudeki[MAX_PATH],save[MAX_PATH],slot[MAX_PATH],fish[MAX_PATH],bunny[MAX_PATH];
    assert(GetTempPathW(MAX_PATH,temp) && GetTempFileNameW(temp,L"srr",0,fixture_root));
    /* Only the file just created by GetTempFileName is replaced with a directory. */
    assert(DeleteFileW(fixture_root) && CreateDirectoryW(fixture_root,NULL));
    assert(join(sudeki,fixture_root,L"Sudeki") && CreateDirectoryW(sudeki,NULL));
    assert(join(save,sudeki,L"Save") && CreateDirectoryW(save,NULL));
    assert(join(slot,save,L"SAVESLOT0005") && CreateDirectoryW(slot,NULL));
    assert(join(fish,slot,L"sudeki.fish") && join(bunny,slot,L"fluffy.bunny"));
    uint8_t *payload=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,0x280200);assert(payload);
    *(uint32_t *)payload=0x280000;write_fixture(fish,payload,0x280200);HeapFree(GetProcessHeap(),0,payload);
    uint8_t metadata[0x600]={0};*(uint32_t *)metadata=0x400;*(uint32_t *)(metadata+0x220)=15;
    strcpy((char *)metadata+0x200,"SAVESLOT0005\\");write_fixture(bunny,metadata,sizeof(metadata));
    SudekiMpSaveCatalog *catalog=calloc(1,sizeof(*catalog));assert(catalog);
    SudekiMpSaveFingerprint fingerprint;
    assert(SudekiMpSaveCatalogRefresh(catalog) && catalog->count==1 && catalog->entries[0].folder_slot==5);
    assert(SudekiMpSaveCatalogFingerprint(catalog,0,&fingerprint));
    InitializeCriticalSection(&lease_lock);lease_lock_ready=TRUE;installed=1;native_thread=GetCurrentThreadId();
    base=VirtualAlloc(NULL,0x420000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(base);
    uint8_t manager[0x1c]={0},strings[0x14]={0},records[RECORD_BYTES];
    char native_root[MAX_PATH];wchar_t slash_root[MAX_PATH];
    assert(swprintf(slash_root,MAX_PATH,L"%ls\\",save)>0);
    assert(WideCharToMultiByte(CP_ACP,0,slash_root,-1,native_root,MAX_PATH,NULL,NULL));
    uint32_t string[2]={(uint32_t)strlen(native_root),(uint32_t)(uintptr_t)native_root};void *entries[]={string};
    *(void **)(base+ROOT_MANAGER)=manager;*(void **)(base+STRING_TABLE)=strings;
    *(void **)strings=base+STRING_VT;*(void **)(strings+4)=base+STRING_ARRAY_VT;
    *(unsigned *)(strings+8)=*(unsigned *)(strings+12)=1;*(void ***)(strings+16)=entries;
    memcpy(records,metadata+0x200,sizeof(records));*(void **)(base+CATALOG_RECORDS)=records;
    *(unsigned *)(base+CATALOG_STATE)=5;*(uint16_t *)(base+CATALOG_COUNT)=1;*(int *)(base+CATALOG_SELECTED)=0;
    load_attempt=7;phase=SUDEKIMP_STORY_LOAD_RETURNED;native_called=final_seen=TRUE;
    accepted_route=SUDEKIMP_STORY_LOAD_ROUTE_TITLE_INDEX;reviewed_fingerprint=fingerprint;reviewed_slot=5;
    int consumer;uint64_t ticket=999;
    SudekiMpSaveFingerprint wrong=fingerprint;wrong.fish_sha256[0]^=0xff;
    assert(!SudekiMpLanStoryLoadReserveReload(&consumer,7,catalog,0,&wrong,&ticket) && ticket==999 && !reload.files);
    assert(SudekiMpLanStoryLoadReserveReload(&consumer,7,catalog,0,&fingerprint,&ticket));
    assert_pinned(fish,bunny);
    HANDLE original_fish=reload.files->pair.fish,original_bunny=reload.files->pair.bunny;
    char fish_ansi[MAX_PATH];assert(WideCharToMultiByte(CP_ACP,0,fish,-1,fish_ansi,MAX_PATH,NULL,NULL));
    HANDLE reader=reader_open(fish_ansi,GENERIC_READ,0,NULL,OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    assert(reader!=INVALID_HANDLE_VALUE);DWORD header=0,got=0;
    assert(ReadFile(reader,&header,4,&got,NULL) && got==4 && header==0x280000 && CloseHandle(reader));
    assert(!SudekiMpLanStoryLoadCancel());exit_result=1;assert(!SudekiMpLanStoryLoadCancel());
    drained=TRUE;assert(SudekiMpLanStoryLoadCancel());assert_pinned(fish,bunny);active_lobby=FALSE;
    /* No native catalog selection survives into the next attempt. */
    *(void **)(base+CATALOG_RECORDS)=(void *)1;*(int *)(base+CATALOG_SELECTED)=-1;
    uint32_t attempt=0;assert(SudekiMpLanStoryLoadPromoteReload(&consumer,ticket,&attempt) && attempt==8);
    assert(file_lease->pair.fish==original_fish && file_lease->pair.bunny==original_bunny);
    assert_pinned(fish,bunny);
    assert(SudekiMpLanStoryLoadPrepare(catalog,0,&fingerprint) && load_attempt==8 && !promoted_reload);
    assert_pinned(fish,bunny);
    assert(SudekiMpLanStoryLoadCancel() && !file_lease);
    HANDLE writer=CreateFileW(fish,GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(writer!=INVALID_HANDLE_VALUE && CloseHandle(writer));
    assert(DeleteFileW(fish) && DeleteFileW(bunny));
    assert(RemoveDirectoryW(slot) && RemoveDirectoryW(save) && RemoveDirectoryW(sudeki) && RemoveDirectoryW(fixture_root));
    assert(VirtualFree(base,0,MEM_RELEASE));base=NULL;DeleteCriticalSection(&lease_lock);free(catalog);
    puts("story reload real-file lease: PASS (synthetic temporary files; pins survive cancellation/promotion/adoption; no native load or user saves)");
    return 0;
}
