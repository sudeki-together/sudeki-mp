/* Synthetic load/file-lease/catalog states only. No native loader or real save
 * files. Reload tests exercise actual adapter policy and reader routing with
 * recording substitutes, not native menu/exit/teardown or real filesystem pins. */
#include "../src/hooks/lan_story_load.c"
#include <assert.h>
struct SudekiMpSaveLease {unsigned slot;};
static struct SudekiMpSaveLease fake_lease;
static SudekiMpSaveFingerprint accepted;
static BOOL acquire_ok=TRUE,record_ok=TRUE;
static unsigned releases;
static BOOL root_ok=TRUE,verify_ok=TRUE,lobby_active=TRUE,exit_drained;
static unsigned exit_status,lease_reads,native_reads;
static SudekiMpSaveCatalog catalog;
BOOL SudekiMpRestoreRelativeCallHook(SudekiMpRelativeCallHook *hook) {
    (void)hook;assert(!"reserved loader must refuse uninstall before restoration");return FALSE;
}
BOOL SudekiMpRestoreInlineHook(SudekiMpInlineHook *hook) {
    (void)hook;assert(!"reserved loader must refuse uninstall before restoration");return FALSE;
}
unsigned SudekiMpLobbyGameplayStoryExitStatus(void) {return exit_status;}
BOOL SudekiMpLobbyGameplayStoryExitDrained(void) {return exit_drained && exit_status==1;}
BOOL SudekiMpLobbyGameplayActive(void) {return lobby_active;}
void SudekiMpLogFormat(const char *format,...) {(void)format;}
void SudekiMpLogWrite(const char *text) {(void)text;assert(!"native load path must not execute");}
BOOL SudekiMpSaveLeaseOpenNativeRead(const SudekiMpSaveLease *lease,const wchar_t *path,HANDLE *out) {
    if(!lease) return FALSE;
    assert(lease==&fake_lease);
    if(wcscmp(path,L"C:\\fixture\\SAVESLOT0005\\sudeki.fish")) return FALSE;
    ++lease_reads;*out=(HANDLE)0x1234;SetLastError(77);return TRUE;
}
BOOL SudekiMpSaveLeaseMatchesRoot(const SudekiMpSaveLease *lease,const wchar_t *root) {
    assert(lease==&fake_lease);return root_ok && !wcscmp(root,L"C:\\fixture\\");
}
BOOL SudekiMpSaveCatalogVerify(SudekiMpSaveCatalog *c,unsigned index,const SudekiMpSaveFingerprint *fp) {
    assert(c==&catalog && index==0 && !memcmp(fp,&accepted,sizeof(*fp)));return verify_ok;
}
static HANDLE WINAPI open_fixture(LPCSTR name,DWORD access,DWORD share,
    LPSECURITY_ATTRIBUTES security,DWORD creation,DWORD flags,HANDLE other) {
    (void)name;(void)access;(void)share;(void)security;(void)creation;(void)flags;(void)other;
    ++native_reads;SetLastError(88);return (HANDLE)0x5678;
}
BOOL SudekiMpSaveCatalogAcquire(SudekiMpSaveCatalog *c,unsigned index,
    const SudekiMpSaveFingerprint *fp,SudekiMpSaveLease **out) {
    assert(c==&catalog && index==0 && fp && out && !*out);
    if(!acquire_ok) {SetLastError(ERROR_FILE_INVALID);return FALSE;}
    assert(!memcmp(fp,&accepted,sizeof(*fp))); *out=&fake_lease; return TRUE;
}
BOOL SudekiMpSaveLeaseRecord(const SudekiMpSaveLease *lease,uint8_t record[0x2c8],unsigned *slot) {
    assert(lease==&fake_lease);
    if(!record_ok) return FALSE;
    memset(record,0,0x2c8); *slot=lease->slot; return TRUE;
}
void SudekiMpSaveLeaseRelease(SudekiMpSaveLease **lease) {if(*lease) ++releases; *lease=NULL;}
static void refused(uint32_t attempt) {
    SudekiMpSaveFingerprint out,saved;
    memset(&out,0xa5,sizeof(out)); saved=out;
    assert(!SudekiMpLanStoryLoadGetFingerprint(attempt,&out));
    assert(!memcmp(&out,&saved,sizeof(out)));
}
static DWORD WINAPI wrong_thread(void *unused) {(void)unused;refused(load_attempt);return 0;}
static int reload_owner,foreign_owner;
static uint64_t held_ticket;
static DWORD WINAPI wrong_reload_thread(void *unused) {
    (void)unused;uint32_t next=999;uint64_t ticket=999;
    assert(!SudekiMpLanStoryLoadReserveReload(&reload_owner,load_attempt,&catalog,0,&accepted,&ticket) && ticket==999);
    assert(!SudekiMpLanStoryLoadCancelReload(&reload_owner,held_ticket));
    assert(!SudekiMpLanStoryLoadPromoteReload(&reload_owner,held_ticket,&next) && next==999);return 0;
}
static void reservation_cases(void) {
    /* A synthetic successful current load. Its fingerprint must remain intact
     * while reserving a different pair for the next load. */
    phase=SUDEKIMP_STORY_LOAD_RETURNED;native_called=final_seen=TRUE;
    accepted_route=SUDEKIMP_STORY_LOAD_ROUTE_TITLE_INDEX;result_code=0;
    reviewed_fingerprint=accepted;reviewed_slot=accepted.folder_slot;
    SudekiMpSaveFingerprint previous=accepted,caller=accepted;
    accepted.fish_sha256[0]^=0xff;caller=accepted;
    uint32_t source=load_attempt,next=999;uint64_t ticket=999;
    base=VirtualAlloc(NULL,0x420000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(base);
    uint8_t root_manager[0x1c]={0},strings[0x14]={0};
    struct {uint32_t length;char text[16];} root={0x8000000bu,"C:\\fixture\\"};
    void *entries[]={&root};
    *(void **)(base+ROOT_MANAGER)=root_manager;*(void **)(base+STRING_TABLE)=strings;
    *(void **)strings=base+STRING_VT;*(void **)(strings+4)=base+STRING_ARRAY_VT;
    *(unsigned *)(strings+8)=*(unsigned *)(strings+12)=1;*(void ***)(strings+16)=entries;
    uint8_t *records=VirtualAlloc(NULL,0x1000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(records);
    strcpy((char *)records,"SAVESLOT0005\\");
    *(void **)(base+CATALOG_RECORDS)=records;*(unsigned *)(base+CATALOG_STATE)=5;
    *(uint16_t *)(base+CATALOG_COUNT)=1;*(int *)(base+CATALOG_SELECTED)=0;
    assert(!SudekiMpLanStoryLoadReserveReload(&reload_owner,source+1,&catalog,0,&caller,&ticket) && ticket==999);
    root_ok=FALSE;
    assert(!SudekiMpLanStoryLoadReserveReload(&reload_owner,source,&catalog,0,&caller,&ticket) && ticket==999 && !reload.files);
    root_ok=TRUE;*(int *)(base+CATALOG_SELECTED)=-1;
    assert(!SudekiMpLanStoryLoadReserveReload(&reload_owner,source,&catalog,0,&caller,&ticket) && ticket==999);
    *(int *)(base+CATALOG_SELECTED)=0;memcpy(records+RECORD_BYTES,records,RECORD_BYTES);
    *(uint16_t *)(base+CATALOG_COUNT)=2;
    assert(!SudekiMpLanStoryLoadReserveReload(&reload_owner,source,&catalog,0,&caller,&ticket));
    *(uint16_t *)(base+CATALOG_COUNT)=1;records[0x20]=1;
    assert(!SudekiMpLanStoryLoadReserveReload(&reload_owner,source,&catalog,0,&caller,&ticket));records[0x20]=0;
    exit_status=1;
    assert(!SudekiMpLanStoryLoadReserveReload(&reload_owner,source,&catalog,0,&caller,&ticket));exit_status=0;
    assert(SudekiMpLanStoryLoadReserveReload(&reload_owner,source,&catalog,0,&caller,&ticket) && ticket!=999);
    held_ticket=ticket;memset(&caller,0xee,sizeof(caller));
    assert(!memcmp(&reload.fingerprint,&accepted,sizeof(accepted)) && reload.files==&fake_lease);
    SudekiMpSaveFingerprint observed;
    assert(SudekiMpLanStoryLoadGetFingerprint(source,&observed) && !memcmp(&observed,&previous,sizeof(previous)));
    assert(load_attempt==source && !file_lease && state()==SUDEKIMP_STORY_LOAD_RETURNED);
    uint64_t cancelled=ticket;unsigned before_cancel=releases;
    assert(SudekiMpLanStoryLoadCancelReload(&reload_owner,ticket) && releases==before_cancel+1 && !reload.files);
    assert(SudekiMpLanStoryLoadGetFingerprint(source,&observed) && !memcmp(&observed,&previous,sizeof(previous)));
    assert(SudekiMpLanStoryLoadReserveReload(&reload_owner,source,&catalog,0,&accepted,&ticket) && ticket>cancelled);
    held_ticket=ticket;assert(!SudekiMpLanStoryLoadCancelReload(&reload_owner,cancelled));
    assert(!SudekiMpLanStoryLoadCancel() && !SudekiMpUninstallLanStoryLoad());
    assert(!SudekiMpLanStoryLoadCancelReload(&foreign_owner,ticket));
    assert(!SudekiMpLanStoryLoadPromoteReload(&reload_owner,ticket,&next) && next==999);
    assert(!SudekiMpLanStoryLoadReserveReload(&reload_owner,source,&catalog,0,&accepted,&held_ticket) && held_ticket==ticket);
    HANDLE worker=CreateThread(NULL,0,wrong_reload_thread,NULL,0,NULL);assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);CloseHandle(worker);
    callbacks=1;assert(!SudekiMpLanStoryLoadCancelReload(&reload_owner,ticket));callbacks=0;
    original_open=open_fixture;
    assert(reader_open("C:\\fixture\\SAVESLOT0005\\sudeki.fish",GENERIC_READ,0,NULL,OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,NULL)==(HANDLE)0x1234 && GetLastError()==77);
    assert(reader_open("unrelated",GENERIC_READ,0,NULL,OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,NULL)==(HANDLE)0x5678 && GetLastError()==88);
    assert(lease_reads==1 && native_reads==1 && !callbacks);
    /* A verified native return alone is insufficient while runtime restoration
     * still needs the old fingerprint. Old lobby cancellation must retire it. */
    exit_status=2;assert(!SudekiMpLanStoryLoadCancel());
    exit_status=1;assert(!SudekiMpLanStoryLoadCancel());exit_drained=TRUE;
    assert(!SudekiMpLanStoryLoadPromoteReload(&reload_owner,ticket,&next) && next==999);
    unsigned before_releases=releases;
    assert(SudekiMpLanStoryLoadCancel() && state()==SUDEKIMP_STORY_LOAD_IDLE && reload.source_retired);
    assert(releases==before_releases && reload.files==&fake_lease);refused(source);
    assert(!SudekiMpLanStoryLoadPromoteReload(&reload_owner,ticket,&next) && next==999);lobby_active=FALSE;
    /* Destroy the old catalog's readability before transfer: no delayed native
     * pointer may be touched. The reservation contains only copied data/files. */
    DWORD old;assert(VirtualProtect(records,0x1000,PAGE_NOACCESS,&old));
    assert(SudekiMpLanStoryLoadPromoteReload(&reload_owner,ticket,&next) && next==source+1);
    assert(releases==before_releases && !reload.files && !reload.ticket && file_lease==&fake_lease);
    assert(state()==SUDEKIMP_STORY_LOAD_PREPARED && promoted_reload && !memcmp(&reviewed_fingerprint,&accepted,sizeof(accepted)));
    assert(!SudekiMpLanStoryLoadPromoteReload(&reload_owner,ticket,&next));
    assert(!SudekiMpLanStoryLoadPrepare(&catalog,0,&previous) && file_lease==&fake_lease && promoted_reload);
    verify_ok=FALSE;assert(!SudekiMpLanStoryLoadPrepare(&catalog,0,&accepted) && promoted_reload);verify_ok=TRUE;
    assert(SudekiMpLanStoryLoadPrepare(&catalog,0,&accepted) && load_attempt==source+1 && !promoted_reload);
    assert(!SudekiMpLanStoryLoadPrepare(&catalog,0,&accepted));
    assert(SudekiMpLanStoryLoadCancel() && releases==before_releases+1 && !file_lease);
    assert(VirtualFree(records,0,MEM_RELEASE) && VirtualFree(base,0,MEM_RELEASE));base=NULL;
}
int main(void) {
    InitializeCriticalSection(&lease_lock); lease_lock_ready=TRUE;
    installed=1; native_thread=GetCurrentThreadId();
    accepted.folder_slot=fake_lease.slot=5;
    for(unsigned i=0;i<32;++i) {accepted.fish_sha256[i]=(uint8_t)i;accepted.bunny_sha256[i]=(uint8_t)(128+i);}
    refused(0); refused(1);
    acquire_ok=FALSE;
    assert(!SudekiMpLanStoryLoadPrepare(&catalog,0,&accepted) && !load_attempt && !file_lease);
    acquire_ok=TRUE; record_ok=FALSE;
    assert(!SudekiMpLanStoryLoadPrepare(&catalog,0,&accepted) && !load_attempt && !file_lease && releases==1);
    record_ok=TRUE; fake_lease.slot=6;
    assert(!SudekiMpLanStoryLoadPrepare(&catalog,0,&accepted) && !load_attempt && !file_lease && releases==2);
    fake_lease.slot=5;
    SudekiMpSaveFingerprint caller=accepted;
    assert(SudekiMpLanStoryLoadPrepare(&catalog,0,&caller) && load_attempt==1);
    memset(&caller,0xff,sizeof(caller)); refused(1);
    for(unsigned s=SUDEKIMP_STORY_LOAD_PREPARED;s<=SUDEKIMP_STORY_LOAD_FAILED;++s) {
        phase=(LONG)s; refused(1); /* even RETURNED lacks positive native proof */
    }
    phase=SUDEKIMP_STORY_LOAD_RETURNED; native_called=TRUE;
    accepted_route=SUDEKIMP_STORY_LOAD_ROUTE_TITLE_INDEX; refused(1); /* file lease still pinned */
    release_files(); result_code=1; refused(1); result_code=0;
    callbacks=1; refused(1); callbacks=0;
    installed=0; refused(1); installed=1;
    refused(0); refused(2);
    assert(!SudekiMpLanStoryLoadGetFingerprint(1,NULL));
    HANDLE worker=CreateThread(NULL,0,wrong_thread,NULL,0,NULL); assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0); CloseHandle(worker);
    SudekiMpSaveFingerprint output;
    assert(SudekiMpLanStoryLoadGetFingerprint(1,&output) && !memcmp(&output,&accepted,sizeof(output)));
    accepted_route=SUDEKIMP_STORY_LOAD_ROUTE_PAGE_RECORD;
    assert(SudekiMpLanStoryLoadGetFingerprint(1,&output));
    accepted_route=SUDEKIMP_STORY_LOAD_ROUTE_NONE; refused(1);
    accepted_route=SUDEKIMP_STORY_LOAD_ROUTE_TITLE_INDEX;
    final_seen=TRUE; assert(SudekiMpLanStoryLoadCancel()); refused(1);
    SudekiMpSaveFingerprint zero={0}; assert(!memcmp(&reviewed_fingerprint,&zero,sizeof(zero)));
    assert(SudekiMpLanStoryLoadPrepare(&catalog,0,&accepted) && load_attempt==2); refused(1); refused(2);
    assert(SudekiMpLanStoryLoadCancel());
    reservation_cases();
    DeleteCriticalSection(&lease_lock);
    puts("story load fingerprint/reload reservation tests passed (synthetic leases/catalogs, no native load or real saves)"); return 0;
}
