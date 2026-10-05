/* Synthetic load/file-lease states only. No native loader or real save files. */
#include "../src/hooks/lan_story_load.c"
#include <assert.h>
struct SudekiMpSaveLease {unsigned slot;};
static struct SudekiMpSaveLease fake_lease;
static SudekiMpSaveFingerprint accepted;
static BOOL acquire_ok=TRUE,record_ok=TRUE;
static unsigned releases;
static SudekiMpSaveCatalog catalog;
void SudekiMpLogFormat(const char *format,...) {(void)format;}
void SudekiMpLogWrite(const char *text) {(void)text;assert(!"native load path must not execute");}
BOOL SudekiMpSaveLeaseOpenNativeRead(const SudekiMpSaveLease *lease,const wchar_t *path,HANDLE *out) {
    (void)lease;(void)path;(void)out;assert(!"native file reader must not execute");return FALSE;
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
    DeleteCriticalSection(&lease_lock);
    puts("story load fingerprint trust-boundary tests passed (synthetic leases, no native load)"); return 0;
}
