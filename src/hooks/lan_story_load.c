#include "hooks/lan_story_load.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Saved story loading requires the supported 32-bit native ABI"
#endif

/* This owns both accepted title/save-page load calls. It does not call the export
 * with a folder suffix, replace catalog initialization, or skip StartLoadedGame.
 * Native index resolution remains separate from local catalog display order. */
enum {
    TITLE_VT=0x2cb1fc, PAGE_VT=0x2ca89c, LIST_VT=0x2d8610,
    ITEM_VT=0x2d8630, LIST_ARRAY_VT=0x2d8640,
    STRING_VT=0x2dcc8c, STRING_ARRAY_VT=0x2dccc4,
    LOAD_SITE=0xa0f98, LOAD_NATIVE=0x100160,
    PAGE_LOAD_SITE=0x848df, PAGE_LOAD_NATIVE=0x100280,
    CATALOG_STATE=0x34b320, CATALOG_RECORDS=0x34b324,
    CATALOG_COUNT=0x34b32c, CATALOG_REQUEST=0x34b32e, CATALOG_SELECTED=0x34b328,
    ROOT_MANAGER=0x408db0, STRING_TABLE=0x409e44, UI_SCENE=0x408d1c,
    MAX_NATIVE_RECORDS=1024, RECORD_BYTES=0x2c8
};
typedef void (__attribute__((thiscall)) *SelectRow)(void *,int);
typedef void (__attribute__((thiscall)) *SetPageState)(void *,unsigned);
/* 005001A0 cleans one stack argument (RET 4), returns validity in AL. */
typedef unsigned (__attribute__((stdcall)) *ValidateSave)(void *);
typedef HANDLE (WINAPI *NativeOpenFile)(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
static uint8_t *base;
static SudekiMpRelativeCallHook load_hook, page_load_hook;
static SudekiMpInlineHook reader_open_hook;
static NativeOpenFile original_open;
static void *reader_continue __attribute__((used));
static CRITICAL_SECTION lease_lock;
static BOOL lease_lock_ready;
static DWORD native_thread;
static volatile LONG phase, result_code, callbacks, installed;
static SudekiMpSaveLease *file_lease;
static uint8_t reviewed_record[RECORD_BYTES];
static unsigned reviewed_slot;
static SudekiMpSaveFingerprint reviewed_fingerprint;
static uint8_t *title_owner, *page_owner, *selected_record;
static int selected_index=-1;
static BOOL confirmation, final_seen;
static unsigned manual_observations;
static uint32_t load_attempt;
static SudekiMpStoryLoadRoute accepted_route;
static BOOL native_called;

static void release_files(void) {
    EnterCriticalSection(&lease_lock);
    SudekiMpSaveLeaseRelease(&file_lease);
    LeaveCriticalSection(&lease_lock);
}

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || !VirtualQuery(p,&m,sizeof(m)) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    if(access!=PAGE_READONLY && access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
        access!=PAGE_EXECUTE_READ && access!=PAGE_EXECUTE_READWRITE &&
        access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL thread_exact(void) { return native_thread && native_thread==GetCurrentThreadId(); }
static unsigned state(void) { return (unsigned)InterlockedCompareExchange(&phase,0,0); }
static BOOL title_identity(const void *owner) {
    return base && readable(owner,0x1844) && *(void *const *)owner==base+TITLE_VT;
}
static BOOL title_foreground(const void *owner) {
    uint8_t *scene=*(uint8_t **)(base+UI_SCENE);
    return readable(scene,0x174) && *(void **)(scene+0x170)==owner;
}
static BOOL cached_page_identity(uint8_t *page) {
    return title_identity(title_owner) && readable(page,0x464) &&
        *(void **)page==base+PAGE_VT && *(void **)(title_owner+0xb4)==page;
}
static BOOL page_identity(uint8_t *page) {
    return cached_page_identity(page) &&
        *(void **)(title_owner+0xac)==page && *(unsigned *)(title_owner+0x4c)==2 &&
        *(unsigned *)(page+0xac)==2;
}
static void failed(DWORD code,const char *reason) {
    InterlockedExchange(&result_code,(LONG)code);
    InterlockedExchange(&phase,SUDEKIMP_STORY_LOAD_FAILED);
    SudekiMpLogFormat("story_load event=validation_failed reason=%s error=%lu native_cancel=none\r\n",reason,(unsigned long)code);
}

/* Read the canonical native StringTable with its proven inline/external string
 * representation. No native methods or native globals are changed here. */
static BOOL root_matches(void) {
    uint8_t *manager=*(uint8_t **)(base+ROOT_MANAGER);
    uint8_t *table=*(uint8_t **)(base+STRING_TABLE);
    if(!file_lease || !readable(manager,0x1c) || !readable(table,0x14) ||
        *(void **)table!=base+STRING_VT || *(void **)(table+4)!=base+STRING_ARRAY_VT)
        return FALSE;
    int index=*(int *)(manager+0x18);
    unsigned count=*(unsigned *)(table+8),capacity=*(unsigned *)(table+12);
    uint32_t **entries=*(uint32_t ***)(table+0x10);
    if(index<0 || (unsigned)index>=count || count>capacity || capacity>1000000u ||
        !readable(entries,(size_t)count*sizeof(*entries))) return FALSE;
    uint32_t *entry=entries[index];
    if(!readable(entry,8)) return FALSE;
    unsigned length=entry[0]&0x7fffffffu;
    const char *text=(entry[0]&0x80000000u)?(char *)(entry+1):*(char **)(entry+1);
    /* The native reader uses a 260-byte path buffer for root + folder + fish. */
    if(!length || length+32>=MAX_PATH || !readable(text,length+1) ||
        text[length] || memchr(text,0,length) || (text[length-1]!='\\' && text[length-1]!='/')) return FALSE;
    wchar_t path[MAX_PATH];
    if(!MultiByteToWideChar(CP_ACP,MB_ERR_INVALID_CHARS,text,-1,path,MAX_PATH)) return FALSE;
    return SudekiMpSaveLeaseMatchesRoot(file_lease,path);
}

static BOOL catalog_match(int *index,uint8_t **record) {
    if(!root_matches() || *(unsigned *)(base+CATALOG_STATE)!=5 ||
        (*(uint8_t *)(base+CATALOG_REQUEST)&3u)) return FALSE;
    unsigned count=*(uint16_t *)(base+CATALOG_COUNT);
    uint8_t *records=*(uint8_t **)(base+CATALOG_RECORDS);
    if(!count || count>MAX_NATIVE_RECORDS || !readable(records,(size_t)count*RECORD_BYTES)) return FALSE;
    char folder[32];
    int n=snprintf(folder,sizeof(folder),"SAVESLOT%04u\\",reviewed_slot);
    if(n<=0 || (size_t)n>=sizeof(folder)) return FALSE;
    int found=-1;
    for(unsigned i=0;i<count;++i) {
        uint8_t *r=records+i*RECORD_BYTES;
        const char *end=memchr(r,0,0x20);
        if(!end || (unsigned)(end-(const char *)r)!=(unsigned)n || lstrcmpiA((char *)r,folder)) continue;
        /* A renamed/backup directory may retain this same stored path. Two
         * catalog records resolving to the reviewed pair are ambiguous. */
        if(found>=0) return FALSE;
        found=(int)i;
        if(r[0x2c4] || memcmp(r+0x20,reviewed_record+0x20,0x29c)) return FALSE;
    }
    if(found<0) return FALSE;
    *index=found; *record=records+(unsigned)found*RECORD_BYTES; return TRUE;
}

static BOOL selected_row(uint8_t *page,int catalog_index,uint8_t **list_out,int *row_out) {
    uint8_t *list=*(uint8_t **)(page+0xb4);
    if(!readable(list,0x110) || *(void **)list!=base+LIST_VT ||
        *(void **)(list+0xbc)!=base+LIST_ARRAY_VT) return FALSE;
    unsigned count=*(unsigned *)(list+0xc0),capacity=*(unsigned *)(list+0xc4);
    uint8_t **items=*(uint8_t ***)(list+0xc8);
    if(!count || count>MAX_NATIVE_RECORDS || count>capacity || capacity>MAX_NATIVE_RECORDS ||
        !readable(items,(size_t)count*sizeof(*items))) return FALSE;
    int found=-1;
    for(unsigned i=0;i<count;++i) {
        if(!readable(items[i],0x8c) || *(void **)items[i]!=base+ITEM_VT) return FALSE;
        int value=*(int *)(items[i]+0x84);
        if(value<0 || value>=*(uint16_t *)(base+CATALOG_COUNT)) return FALSE;
        if(value==catalog_index) { if(found>=0) return FALSE; found=(int)i; }
    }
    if(found<0) return FALSE;
    *list_out=list; *row_out=found; return TRUE;
}

__attribute__((noinline,used,force_align_arg_pointer))
static HANDLE __cdecl reader_open(LPCSTR name,DWORD access,DWORD share,
    LPSECURITY_ATTRIBUTES security,DWORD creation,DWORD flags,HANDLE template_file) {
    InterlockedIncrement(&callbacks);
    BOOL handled=FALSE;
    HANDLE opened=INVALID_HANDLE_VALUE;
    DWORD entry_error=GetLastError(),error=entry_error;
    EnterCriticalSection(&lease_lock);
    /* The catalog worker also reads through this exact native seam. Only the
     * immutable Win32 file lease is shared; no worker dereferences game data. */
    if(access==GENERIC_READ && !share && !security && creation==OPEN_EXISTING &&
        flags==(FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN) && !template_file) {
        char local[MAX_PATH]; wchar_t wide[MAX_PATH];
        unsigned length=0; BOOL terminated=FALSE;
        while(length<MAX_PATH && readable(name?name+length:NULL,1)) {
            local[length]=name[length];
            if(!local[length]) { terminated=TRUE; break; }
            ++length;
        }
        if(length && terminated &&
            MultiByteToWideChar(CP_ACP,MB_ERR_INVALID_CHARS,local,-1,wide,MAX_PATH)) {
            handled=SudekiMpSaveLeaseOpenNativeRead(file_lease,wide,&opened);
            error=GetLastError();
        }
    }
    if(!handled) {
        SetLastError(entry_error);
        opened=original_open(name,access,share,security,creation,flags,template_file);
        error=GetLastError();
    }
    LeaveCriticalSection(&lease_lock);
    InterlockedDecrement(&callbacks); SetLastError(error); return opened;
}
__attribute__((naked)) static void reader_open_entry(void) {
    /* Replaces FF15 CreateFileA with JMP. Seven original arguments are already
     * on the stack; preserve its stdcall stack effect and continuation. */
    __asm__ volatile("call _reader_open\n\tadd $28,%esp\n\tjmp *_reader_continue");
}

static DWORD native_load(SudekiMpStoryLoadRoute route,uintptr_t argument) {
    DWORD out;
    void *fn=base+(route==SUDEKIMP_STORY_LOAD_ROUTE_PAGE_RECORD?PAGE_LOAD_NATIVE:LOAD_NATIVE);
    /* Title helper takes a sorted index in EAX; page helper takes the actual
     * native catalog record in EAX. Both return EAX with no stack arguments. */
    __asm__ volatile("call *%[fn]" : "=a"(out) : "0"(argument),[fn]"r"(fn)
        : "ecx","edx","cc","memory");
    return out;
}

static BOOL final_owner(SudekiMpStoryLoadRoute route,void *owner) {
    if(!thread_exact()) return FALSE;
    if(route==SUDEKIMP_STORY_LOAD_ROUTE_TITLE_INDEX)
        return owner==title_owner && title_identity(owner) &&
            *(unsigned *)(title_owner+0x44)==0 && *(unsigned *)(title_owner+0x48)==13;
    /* 4813C0 changes the accepted page to state27. 484650 closes it before
     * calling 500280: page+29 and page+AC are already zero, while its accepted
     * index/flag and the title's exact cached-page identity remain intact. */
    /* The preceding native script/loading-screen pump may advance the title.
     * Use the exact page bound during confirmation and its positive closed
     * state, not the title's formerly active-child selection fields. */
    return route==SUDEKIMP_STORY_LOAD_ROUTE_PAGE_RECORD && owner==page_owner &&
        cached_page_identity(page_owner) &&
        !page_owner[0x29] && page_owner[0x4a] && *(unsigned *)(page_owner+0x34)==2 &&
        *(unsigned *)(page_owner+0xac)==0 && *(unsigned *)(page_owner+0xa8)==27;
}

__attribute__((noinline,used,force_align_arg_pointer))
static DWORD __cdecl load_guard(void *owner,uintptr_t argument,SudekiMpStoryLoadRoute route) {
    InterlockedIncrement(&callbacks);
    DWORD result;
    unsigned current=state();
    if(current==SUDEKIMP_STORY_LOAD_IDLE || (final_seen && !file_lease &&
        (current==SUDEKIMP_STORY_LOAD_RETURNED || current==SUDEKIMP_STORY_LOAD_FAILED))) {
        /* An unprepared manual Continue retains its original native behavior.
         * Bound diagnostics contain no save name, path or file content. */
        BOOL observe=manual_observations<16;
        if(observe) {
            ++manual_observations;
            SudekiMpLogFormat("story_load event=manual_native_begin route=%u\r\n",route);
        }
        result=native_load(route,argument);
        if(observe) SudekiMpLogFormat("story_load event=manual_native_return route=%u result=%lu world_ready=unknown\r\n",route,(unsigned long)result);
    } else if(current==SUDEKIMP_STORY_LOAD_LOADING) {
        /* The reader pumps native callbacks. A nested title dispatch is not
         * permission to free the outer reader's files or replace its result. */
        result=0x80004005u;
        SudekiMpLogWrite("story_load event=reentrant_load_rejected lease=retained\r\n");
    } else {
        int actual=-1; uint8_t *record=NULL;
        BOOL own_final=final_owner(route,owner);
        BOOL okay=current==SUDEKIMP_STORY_LOAD_FADING && own_final && confirmation &&
            selected_index>=0 && catalog_match(&actual,&record) &&
            actual==selected_index && record==selected_record;
        if(okay && route==SUDEKIMP_STORY_LOAD_ROUTE_TITLE_INDEX)
            okay=argument==(uintptr_t)(unsigned)actual && *(int *)(title_owner+0x1820)==actual;
        else if(okay)
            okay=argument==(uintptr_t)record && *(int *)(page_owner+0xb8)==actual &&
                *(int *)(base+CATALOG_SELECTED)==actual;
        if(own_final) { final_seen=TRUE; accepted_route=route; }
        if(!okay) { result=0x80004005u; failed(ERROR_INVALID_STATE,"accepted_load_identity"); }
        else {
            InterlockedExchange(&phase,SUDEKIMP_STORY_LOAD_LOADING);
            native_called=TRUE;
            SudekiMpLogFormat("story_load event=reviewed_native_begin attempt=%lu route=%u index=%d folder_slot=%u window=same\r\n",
                (unsigned long)load_attempt,route,actual,reviewed_slot);
            result=native_load(route,argument);
            InterlockedExchange(&result_code,(LONG)result);
            /* Keep LOADING published through lease retirement; a poller must
             * not mistake native return for a fully retired file boundary. */
            release_files();
            InterlockedExchange(&phase,result?SUDEKIMP_STORY_LOAD_FAILED:SUDEKIMP_STORY_LOAD_RETURNED);
            SudekiMpLogFormat("story_load event=reviewed_native_return attempt=%lu route=%u result=%lu files_retired=1 world_ready=unknown\r\n",
                (unsigned long)load_attempt,route,(unsigned long)result);
        }
        /* The native file reads have returned, or were positively rejected.
         * Native GEL/world task lifetimes are owned by the story runtime. */
        if(own_final && !okay) release_files();
    }
    InterlockedDecrement(&callbacks); return result;
}
__attribute__((naked)) static void load_entry(void) {
    __asm__ volatile("push $1\n\tpush %eax\n\tpush %ebx\n\tcall _load_guard\n\tadd $12,%esp\n\tret");
}
__attribute__((naked)) static void page_load_entry(void) {
    __asm__ volatile("push $2\n\tpush %eax\n\tpush %edi\n\tcall _load_guard\n\tadd $12,%esp\n\tret");
}

BOOL SudekiMpInstallLanStoryLoad(HMODULE game) {
    if(!game || base || !SudekiMpCheckLoadedExecutable(game)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    wchar_t path[MAX_PATH]; SudekiMpBuildCheck check;
    DWORD n=GetModuleFileNameW(game,path,MAX_PATH);
    if(!n || n>=MAX_PATH || !SudekiMpCheckExecutableFile(path,&check) || !check.hash_matches || !check.pe_matches) {
        SetLastError(ERROR_BAD_EXE_FORMAT); return FALSE;
    }
    uint8_t *b=(uint8_t *)game;
    static const uint8_t selection[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x0c,0x53,0x56,0x8b,0xf1,0x8b,0x96,0xc0,0,0,0};
    static const uint8_t page_state[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,8,0x56,0x8b,0xf1};
    static const uint8_t accept[]={0x51,0x8b,0x86,0xa8,0,0,0,0x83,0xc0,0xee,0x53,0x57};
    static const uint8_t validate[]={0x81,0xec,4,1,0,0,0x55,0x8b,0xac,0x24,0x0c,1,0,0};
    static const uint8_t before_call[]={0x8b,0x83,0x20,0x18,0,0};
    static const uint8_t record_reader[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x81,0xec,4,6,0,0};
    static const uint8_t close_page[]={0x56,0x8b,0xf1,0x57};
    static const uint8_t closed_page[]={0xc7,0x86,0xac,0,0,0,0,0,0,0,0x5e,0xc3};
    static const uint8_t page_update[]={0x6a,0x1b,0x8b,0xce,0xe8,0xa7,0x28,0,0,0x56,0xe8,0x81,0x32,0,0};
    uint8_t page_before_call[]={0xa3,0,0,0,0,0x69,0xc0,0xc8,2,0,0,0x03,0x05,0,0,0,0};
    void *selected_slot=b+CATALOG_SELECTED,*records_slot=b+CATALOG_RECORDS;
    memcpy(page_before_call+1,&selected_slot,4); memcpy(page_before_call+13,&records_slot,4);
    uint8_t reader_call[]={0xff,0x15,0,0,0,0};
    void *reader_slot=b+0x29a0ac;
    memcpy(reader_call+2,&reader_slot,4);
    FARPROC open_address=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"CreateFileA");
    if(memcmp(b+0x15e2d0,selection,sizeof(selection)) || memcmp(b+0x83c70,page_state,sizeof(page_state)) ||
        memcmp(b+0x84560,accept,sizeof(accept)) || memcmp(b+0x1001a0,validate,sizeof(validate)) ||
        memcmp(b+LOAD_SITE-6,before_call,sizeof(before_call)) ||
        memcmp(b+PAGE_LOAD_NATIVE,record_reader,sizeof(record_reader)) ||
        memcmp(b+PAGE_LOAD_SITE-sizeof(page_before_call),page_before_call,sizeof(page_before_call)) ||
        memcmp(b+0x84260,close_page,sizeof(close_page)) ||
        memcmp(b+0x842b2,closed_page,sizeof(closed_page)) ||
        memcmp(b+0x813c0,page_update,sizeof(page_update)) ||
        *(void **)(b+PAGE_VT+8)!=b+0x810a0 || *(void **)(b+PAGE_VT+0x2c)!=b+0x81830 ||
        *(void **)(b+PAGE_VT+0x44)!=b+0x84260 ||
        *(void **)(b+PAGE_VT+0x58)!=b+0x84e30 || *(void **)(b+STRING_VT+0x10)!=b+0x1b9c00 ||
        memcmp(b+0x1bcad7,reader_call,sizeof(reader_call)) || !open_address ||
        *(FARPROC *)reader_slot!=open_address) {
        SetLastError(ERROR_BAD_EXE_FORMAT); return FALSE;
    }
    if(!InitializeCriticalSectionAndSpinCount(&lease_lock,4000)) return FALSE;
    lease_lock_ready=TRUE;
    base=b;
    _Static_assert(sizeof(original_open)==sizeof(open_address),"Win32 function pointer size");
    memcpy(&original_open,&open_address,sizeof(original_open));
    reader_continue=b+0x1bcadd;
    if(!SudekiMpInstallInlineHook(&reader_open_hook,b+0x1bcad7,reader_call,sizeof(reader_call),reader_open_entry) ||
        !SudekiMpInstallRelativeCallHook(&load_hook,b+LOAD_SITE,b+LOAD_NATIVE,load_entry) ||
        !SudekiMpInstallRelativeCallHook(&page_load_hook,b+PAGE_LOAD_SITE,b+PAGE_LOAD_NATIVE,page_load_entry)) goto rollback;
    InterlockedExchange(&installed,1); return TRUE;
rollback: {
        DWORD error=GetLastError();
        (void)SudekiMpUninstallLanStoryLoad();
        SetLastError(error); return FALSE;
    }
}

BOOL SudekiMpUninstallLanStoryLoad(void) {
    if(!base) return TRUE;
    if(state()!=SUDEKIMP_STORY_LOAD_IDLE || file_lease || InterlockedCompareExchange(&callbacks,0,0)) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    BOOL okay=SudekiMpRestoreRelativeCallHook(&page_load_hook);
    if(!SudekiMpRestoreRelativeCallHook(&load_hook)) okay=FALSE;
    if(!SudekiMpRestoreInlineHook(&reader_open_hook)) okay=FALSE;
    if(!okay || InterlockedCompareExchange(&callbacks,0,0)) {
        HMODULE self;
        (void)GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCWSTR)&load_hook,&self);
        SetLastError(ERROR_BUSY); return FALSE;
    }
    InterlockedExchange(&installed,0);
    /* Match the existing native adapters: original-call dependencies remain
     * immutable through process exit, even after successful hook restoration. */
    return TRUE;
}

BOOL SudekiMpLanStoryLoadPrepare(SudekiMpSaveCatalog *catalog,unsigned index,
    const SudekiMpSaveFingerprint *fingerprint) {
    if(!InterlockedCompareExchange(&installed,0,0) || !thread_exact() || state()!=SUDEKIMP_STORY_LOAD_IDLE ||
        file_lease || InterlockedCompareExchange(&callbacks,0,0)) { SetLastError(ERROR_BUSY); return FALSE; }
    if(load_attempt==UINT32_MAX) { SetLastError(ERROR_ARITHMETIC_OVERFLOW); return FALSE; }
    if(!fingerprint) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    /* Retain the exact candidate passed to Acquire, not the caller's mutable
     * review buffer after acquisition. Acquire independently rehashes files. */
    SudekiMpSaveFingerprint candidate=*fingerprint;
    SudekiMpSaveLease *acquired=NULL;
    if(!lease_lock_ready) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    /* Serialize the first pin and its publication with native open attempts;
     * a catalog worker must not choose exclusive sharing in that interval. */
    EnterCriticalSection(&lease_lock);
    BOOL acquired_ok=SudekiMpSaveCatalogAcquire(catalog,index,&candidate,&acquired);
    DWORD acquired_error=GetLastError();
    if(acquired_ok) file_lease=acquired;
    LeaveCriticalSection(&lease_lock);
    if(!acquired_ok) { SetLastError(acquired_error); return FALSE; }
    if(!SudekiMpSaveLeaseRecord(file_lease,reviewed_record,&reviewed_slot)) {
        release_files(); return FALSE;
    }
    if(reviewed_slot!=candidate.folder_slot) {
        release_files(); SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    reviewed_fingerprint=candidate;
    title_owner=page_owner=selected_record=NULL; selected_index=-1;
    confirmation=final_seen=native_called=FALSE; accepted_route=SUDEKIMP_STORY_LOAD_ROUTE_NONE;
    ++load_attempt; InterlockedExchange(&result_code,0);
    InterlockedExchange(&phase,SUDEKIMP_STORY_LOAD_PREPARED);
    SudekiMpLogFormat("story_load event=prepared folder_slot=%u files=pinned\r\n",reviewed_slot);
    return TRUE;
}
BOOL SudekiMpLanStoryLoadNeedsTitle(void) { return state()==SUDEKIMP_STORY_LOAD_PREPARED; }
BOOL SudekiMpLanStoryLoadArmTitle(void *owner) {
    if(!thread_exact() || state()!=SUDEKIMP_STORY_LOAD_PREPARED || !title_identity(owner) ||
        !title_foreground(owner) || *(unsigned *)((uint8_t *)owner+0x44)!=5) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    uint8_t *page=*(uint8_t **)((uint8_t *)owner+0xb4);
    if(!readable(page,0x464) || *(void **)page!=base+PAGE_VT || page[0x29] || page[0x4a]) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    title_owner=owner; page_owner=page;
    InterlockedExchange(&phase,SUDEKIMP_STORY_LOAD_WAITING_PAGE); return TRUE;
}

void SudekiMpLanStoryLoadServiceTitle(void *owner) {
    if(!InterlockedCompareExchange(&installed,0,0) || !title_identity(owner)) return;
    if(!native_thread && *(unsigned *)((uint8_t *)owner+0x44)==5 && title_foreground(owner))
        native_thread=GetCurrentThreadId();
    if(!thread_exact() || state()!=SUDEKIMP_STORY_LOAD_WAITING_PAGE || owner!=title_owner) return;
    unsigned title_state=*(unsigned *)(title_owner+0x44);
    if(title_state==5 || title_state==6 || title_state==7) return;
    if(title_state!=8 || !page_identity(page_owner)) { failed(ERROR_INVALID_STATE,"native_page_identity"); return; }
    unsigned page_state=*(unsigned *)(page_owner+0xa8);
    /* 1..9 include resource creation, asynchronous catalog progress and the
     * native list fade. A cold catalog legitimately traverses 3..7. */
    if(!confirmation && page_state>=1 && page_state<=9) return;
    if((!confirmation && page_state!=10) || (confirmation && page_state!=18) ||
        !page_owner[0x29] || !page_owner[0x48] || !page_owner[0x49] || page_owner[0x4a]) {
        failed(ERROR_INVALID_STATE,"native_page_state"); return;
    }
    int index=-1,row=-1; uint8_t *record=NULL,*list=NULL;
    if(!catalog_match(&index,&record) || !selected_row(page_owner,index,&list,&row)) {
        failed(ERROR_FILE_INVALID,"catalog_or_row"); return;
    }
    if(!confirmation) {
        ((SelectRow)(base+0x15e2d0))(list,row);
        if(*(int *)(list+0x94)!=row || !(((ValidateSave)(base+0x1001a0))(record)&0xffu) || record[0x2c4]) {
            failed(ERROR_FILE_INVALID,"native_save_validation"); return;
        }
        selected_index=index; selected_record=record;
        /* Preserve native validation and state-entry work. The lobby already
         * confirmed this exact file pair; this is the native confirmation state. */
        ((SetPageState)(base+0x83c70))(page_owner,18);
        if(*(unsigned *)(page_owner+0xa8)!=18) { failed(ERROR_INVALID_STATE,"confirmation_state"); return; }
        SudekiMpLogFormat("story_load event=native_selection index=%d folder_slot=%u\r\n",index,reviewed_slot);
        confirmation=TRUE; return;
    }
    if(index!=selected_index || record!=selected_record || *(int *)(list+0x94)!=row) {
        failed(ERROR_INVALID_STATE,"confirmation_selection"); return;
    }
    uint8_t *frontend=*(uint8_t **)(base+0x408d8c);
    if(!readable(frontend,0x59)) { failed(ERROR_INVALID_STATE,"frontend_owner"); return; }
    InterlockedExchange(&phase,SUDEKIMP_STORY_LOAD_FADING);
    void *accept=base+0x84560;
    /* Native helper: ESI=the title-owned load page, no stack arguments. */
    __asm__ volatile("call *%[fn]" : : "S"(page_owner),[fn]"r"(accept) : "eax","ecx","edx","cc","memory");
    if(!page_identity(page_owner) || !page_owner[0x4a] || *(int *)(page_owner+0xb8)!=index ||
        *(unsigned *)(page_owner+0xa8)!=25) failed(ERROR_INVALID_STATE,"accepted_page_state");
}

SudekiMpStoryLoadState SudekiMpLanStoryLoadPoll(DWORD *result) {
    SudekiMpStoryLoadState value=(SudekiMpStoryLoadState)state();
    if(result) *result=(DWORD)InterlockedCompareExchange(&result_code,0,0);
    return value;
}
BOOL SudekiMpLanStoryLoadGetResult(SudekiMpStoryLoadResult *out) {
    if(!out || !thread_exact() || !InterlockedCompareExchange(&installed,0,0) ||
        InterlockedCompareExchange(&callbacks,0,0)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    SudekiMpStoryLoadResult next={0};
    next.attempt=load_attempt; next.state=SudekiMpLanStoryLoadPoll(&next.result);
    next.route=accepted_route; next.folder_slot=reviewed_slot; next.native_index=selected_index;
    next.native_called=native_called;
    EnterCriticalSection(&lease_lock); next.files_retired=file_lease==NULL; LeaveCriticalSection(&lease_lock);
    *out=next; return TRUE;
}
BOOL SudekiMpLanStoryLoadGetFingerprint(uint32_t attempt,SudekiMpSaveFingerprint *out) {
    SudekiMpStoryLoadResult result;
    if(!out || !attempt || !SudekiMpLanStoryLoadGetResult(&result) ||
        result.attempt!=attempt || result.state!=SUDEKIMP_STORY_LOAD_RETURNED ||
        !result.native_called || !result.files_retired || result.result ||
        (result.route!=SUDEKIMP_STORY_LOAD_ROUTE_TITLE_INDEX &&
         result.route!=SUDEKIMP_STORY_LOAD_ROUTE_PAGE_RECORD) ||
        result.folder_slot!=reviewed_fingerprint.folder_slot) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    *out=reviewed_fingerprint; return TRUE;
}
BOOL SudekiMpLanStoryLoadCancel(void) {
    unsigned current=state();
    if(current==SUDEKIMP_STORY_LOAD_IDLE) return TRUE;
    if(!thread_exact() || InterlockedCompareExchange(&callbacks,0,0) || current==SUDEKIMP_STORY_LOAD_LOADING ||
        current==SUDEKIMP_STORY_LOAD_FADING) { SetLastError(ERROR_BUSY); return FALSE; }
    if(current!=SUDEKIMP_STORY_LOAD_PREPARED && !final_seen) {
        /* An aborted native page must be observed closed back at the title.
         * No synthetic cancellation or discarded confirmation is allowed. */
        if(!title_identity(title_owner) || !title_foreground(title_owner) ||
            *(unsigned *)(title_owner+0x44)!=5 || !cached_page_identity(page_owner) ||
            page_owner[0x29] || page_owner[0x4a]) { SetLastError(ERROR_BUSY); return FALSE; }
    }
    release_files();
    memset(&reviewed_fingerprint,0,sizeof(reviewed_fingerprint));
    title_owner=page_owner=selected_record=NULL; selected_index=-1;
    confirmation=final_seen=native_called=FALSE; accepted_route=SUDEKIMP_STORY_LOAD_ROUTE_NONE;
    InterlockedExchange(&phase,SUDEKIMP_STORY_LOAD_IDLE); return TRUE;
}
