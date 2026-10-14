#include "ui/save_catalog.h"
#include <bcrypt.h>
#include <shlobj.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* Bounds on local read work, not claims about the native save-file format. */
#define FISH_LIMIT (16u * 1024u * 1024u)
#define BUNNY_LIMIT (64u * 1024u)
#define ENUMERATION_LIMIT 2048u

typedef HRESULT (WINAPI *GetAppDataPath)(HWND, int, HANDLE, DWORD, LPWSTR);
typedef struct SavePair {
    HANDLE root, folder, fish, bunny;
    SudekiMpSaveFileIdentity fish_identity, bunny_identity, root_identity, folder_identity;
} SavePair;

static BOOL fail(SudekiMpSaveCatalog *catalog, DWORD error, const char *message) {
    if(catalog) {
        snprintf(catalog->error,sizeof(catalog->error),"%s",message);
        catalog->error[sizeof(catalog->error)-1]='\0';
    }
    SetLastError(error?error:ERROR_INVALID_DATA);
    return FALSE;
}

static BOOL join(wchar_t out[MAX_PATH], const wchar_t *parent, const wchar_t *name) {
    size_t a=wcslen(parent),b=wcslen(name);
    if(!a || a+1u+b>=MAX_PATH) { SetLastError(ERROR_FILENAME_EXCED_RANGE); return FALSE; }
    memcpy(out,parent,a*sizeof(wchar_t)); out[a++]=L'\\';
    memcpy(out+a,name,(b+1u)*sizeof(wchar_t)); return TRUE;
}

/* Launcher "Local host + client": a second game on the same Windows profile
 * gets its own roaming AppData folder (the game's own lookup is redirected by
 * hooks/profile_folder.c); the save catalogue must read the same folder. */
static wchar_t appdata_override[MAX_PATH];
void SudekiMpSaveCatalogSetAppDataOverride(const wchar_t *path) {
    if(!path || !path[0] || wcslen(path)>=MAX_PATH) appdata_override[0]=0;
    else wcscpy(appdata_override,path);
}
static BOOL save_directory(wchar_t out[MAX_PATH]) {
    wchar_t appdata[MAX_PATH],sudeki[MAX_PATH];
    if(appdata_override[0]) return join(sudeki,appdata_override,L"Sudeki") && join(out,sudeki,L"Save");
    HMODULE shell=LoadLibraryW(L"shell32.dll");
    if(!shell) return FALSE;
    FARPROC address=GetProcAddress(shell,"SHGetFolderPathW");
    GetAppDataPath get_path=NULL;
    _Static_assert(sizeof(get_path)==sizeof(address),"Win32 procedure address size");
    memcpy(&get_path,&address,sizeof(get_path));
    HRESULT result=get_path?get_path(NULL,CSIDL_APPDATA,NULL,SHGFP_TYPE_CURRENT,appdata):E_FAIL;
    FreeLibrary(shell);
    if(FAILED(result)) { SetLastError(ERROR_PATH_NOT_FOUND); return FALSE; }
    return join(sudeki,appdata,L"Sudeki") && join(out,sudeki,L"Save");
}

static BOOL identify(HANDLE handle, BOOL directory, uint64_t limit,
    SudekiMpSaveFileIdentity *out) {
    BY_HANDLE_FILE_INFORMATION info;
    if(!GetFileInformationByHandle(handle,&info)) return FALSE;
    uint64_t size=((uint64_t)info.nFileSizeHigh<<32)|info.nFileSizeLow;
    if((info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) ||
        !!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=!!directory ||
        (!directory && (!size || size>limit))) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    memset(out,0,sizeof(*out));
    out->volume=info.dwVolumeSerialNumber; out->index_high=info.nFileIndexHigh;
    out->index_low=info.nFileIndexLow; out->size=size;
    out->created=info.ftCreationTime; out->modified=info.ftLastWriteTime;
    return TRUE;
}

static BOOL same_identity(const SudekiMpSaveFileIdentity *a,
    const SudekiMpSaveFileIdentity *b, BOOL content_metadata) {
    return a->volume==b->volume && a->index_high==b->index_high &&
        a->index_low==b->index_low && !CompareFileTime(&a->created,&b->created) &&
        (!content_metadata || (a->size==b->size &&
            !CompareFileTime(&a->modified,&b->modified)));
}

static HANDLE open_directory(const wchar_t *path, SudekiMpSaveFileIdentity *identity) {
    /* No delete sharing keeps the selected directory entry stable while read. */
    HANDLE h=CreateFileW(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(h!=INVALID_HANDLE_VALUE && !identify(h,TRUE,0,identity)) {
        DWORD error=GetLastError(); CloseHandle(h); SetLastError(error);
        return INVALID_HANDLE_VALUE;
    }
    return h;
}

static HANDLE open_save_file(const wchar_t *path, uint64_t limit,
    SudekiMpSaveFileIdentity *identity) {
    HANDLE h=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    if(h!=INVALID_HANDLE_VALUE && !identify(h,FALSE,limit,identity)) {
        DWORD error=GetLastError(); CloseHandle(h); SetLastError(error);
        return INVALID_HANDLE_VALUE;
    }
    return h;
}

static void close_pair(SavePair *p) {
    DWORD error=GetLastError();
    if(p->bunny!=INVALID_HANDLE_VALUE) CloseHandle(p->bunny);
    if(p->fish!=INVALID_HANDLE_VALUE) CloseHandle(p->fish);
    if(p->folder!=INVALID_HANDLE_VALUE) CloseHandle(p->folder);
    if(p->root!=INVALID_HANDLE_VALUE) CloseHandle(p->root);
    p->root=p->folder=p->fish=p->bunny=INVALID_HANDLE_VALUE;
    SetLastError(error);
}

static BOOL open_pair(const wchar_t *root, unsigned slot, SavePair *p) {
    wchar_t name[16],folder[MAX_PATH],path[MAX_PATH];
    memset(p,0,sizeof(*p));
    p->root=p->folder=p->fish=p->bunny=INVALID_HANDLE_VALUE;
    if(slot>9999u) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    swprintf(name,sizeof(name)/sizeof(name[0]),L"SAVESLOT%04u",slot);
    p->root=open_directory(root,&p->root_identity);
    if(p->root==INVALID_HANDLE_VALUE || !join(folder,root,name)) goto error;
    p->folder=open_directory(folder,&p->folder_identity);
    if(p->folder==INVALID_HANDLE_VALUE || !join(path,folder,L"sudeki.fish")) goto error;
    p->fish=open_save_file(path,FISH_LIMIT,&p->fish_identity);
    if(p->fish==INVALID_HANDLE_VALUE || !join(path,folder,L"fluffy.bunny")) goto error;
    p->bunny=open_save_file(path,BUNNY_LIMIT,&p->bunny_identity);
    if(p->bunny==INVALID_HANDLE_VALUE) goto error;
    return TRUE;
error:
    close_pair(p); return FALSE;
}

static BOOL canonical_slot(const wchar_t *name, unsigned *slot) {
    if(wcslen(name)!=12u || wcsncmp(name,L"SAVESLOT",8u)) return FALSE;
    unsigned value=0;
    for(unsigned i=8;i<12;++i) {
        if(name[i]<L'0' || name[i]>L'9') return FALSE;
        value=value*10u+(unsigned)(name[i]-L'0');
    }
    *slot=value; return TRUE;
}

static int entry_compare(const void *a,const void *b) {
    const SudekiMpSaveCatalogEntry *x=a,*y=b;
    if(!!x->sort_known!=!!y->sort_known) return x->sort_known?-1:1;
    if(x->sort_known && y->sort_known) {
        /* Native 500A20 sorts these three keys newest/highest first. Missing
         * or unsupported records and our bounded scan prevent index parity. */
        LONG order=CompareFileTime(&x->sort_start,&y->sort_start);
        if(order) return order<0?1:-1;
        order=CompareFileTime(&x->sort_directory_modified,&y->sort_directory_modified);
        if(order) return order<0?1:-1;
        if(x->sort_elapsed!=y->sort_elapsed) return x->sort_elapsed<y->sort_elapsed?1:-1;
    }
    return x->folder_slot<y->folder_slot?-1:x->folder_slot>y->folder_slot;
}

static void fallback_metadata(SudekiMpSaveCatalogEntry *entry) {
    SYSTEMTIME time;
    entry->leader=4u;
    memset(entry->party_order,4,sizeof(entry->party_order));
    snprintf(entry->label,sizeof(entry->label),"Save %04lu",(unsigned long)entry->folder_slot);
    snprintf(entry->location,sizeof(entry->location),"Location unavailable");
    if(FileTimeToSystemTime(&entry->fish.modified,&time) && time.wYear<=9999u &&
        time.wMonth<=12u && time.wDay<=31u && time.wHour<=23u && time.wMinute<=59u)
        snprintf(entry->saved_at,sizeof(entry->saved_at),"%04u-%02u-%02u %02u:%02u UTC",
            time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute);
    snprintf(entry->details[0],sizeof(entry->details[0]),"Native save details unavailable");
}

static uint32_t get32(const BYTE *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}

static BOOL display_location(char *out,size_t capacity,const wchar_t *wide,
    BOOL *ascii) {
    char text[512];
    int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,-1,
        text,sizeof(text),NULL,NULL);
    if(bytes<=0 || capacity<2u) return FALSE;
    size_t count=(size_t)bytes-1u;
    *ascii=FALSE;
    for(size_t i=0;i<count;++i) if((unsigned char)text[i]>126u || text[i]<32) {
        snprintf(out,capacity,"Localized location name"); return TRUE;
    }
    *ascii=TRUE;
    if(count>=capacity) count=capacity-1u;
    memcpy(out,text,count); out[count]='\0'; return TRUE;
}

static BOOL display_duration(const wchar_t *text,char out[32]) {
    unsigned hours=0,digits=0;
    const wchar_t *p=text;
    while(*p>=L'0' && *p<=L'9' && digits<6u) {
        hours=hours*10u+(unsigned)(*p++-L'0'); ++digits;
    }
    if(!digits || *p!=L':') return FALSE;
    ++p;
    if(wcslen(p)!=5u || p[0]<L'0' || p[0]>L'5' ||
        p[1]<L'0' || p[1]>L'9' || p[2]!=L':' ||
        p[3]<L'0' || p[3]>L'5' || p[4]<L'0' || p[4]>L'9' || p[5]) return FALSE;
    snprintf(out,32,"%02u:%c%c:%c%c",hours,(char)p[0],(char)p[1],(char)p[3],(char)p[4]);
    return TRUE;
}

static void native_metadata(SudekiMpSaveCatalogEntry *entry,HANDLE bunny) {
    /* Exact native writer/reader contract: 5BC900/5BCAA0 wrapper, 4FEB00
     * 0x400 payload, 4FE7B0 record copy/marker. 4FE090 rewrites path only.
     * The title at record+1B8 is authored display text, not a zone ID.
     * record+24 is NOT claimed to be save time; use filesystem time above. */
    BYTE bytes[0x600]; DWORD received=0;
    wchar_t title[128]; unsigned end=0;
    BOOL ascii=FALSE;
    if(entry->bunny.size!=sizeof(bytes) ||
        !ReadFile(bunny,bytes,sizeof(bytes),&received,NULL) || received!=sizeof(bytes) ||
        get32(bytes)!=0x400u || get32(bytes+0x200u+0x20u)!=15u) return;
    const BYTE *record=bytes+0x200u;
    for(;end<128u;++end) {
        unsigned c=(unsigned)record[0x1b8u+end*2u]|
            ((unsigned)record[0x1b9u+end*2u]<<8);
        title[end]=(wchar_t)c;
        if(!c) break;
        if((c<32u && c!='\r' && c!='\n') || (c>=127u && c<=159u)) return;
    }
    if(!end || end==128u) return;
    wchar_t *line=wcschr(title,L'\n');
    if(line) {
        *line++=L'\0';
        size_t first=wcslen(title);
        if(first && title[first-1u]==L'\r') title[first-1u]=L'\0';
    }
    if(!title[0] || wcschr(title,L'\r') ||
        !display_location(entry->location,sizeof(entry->location),title,&ascii)) return;
    entry->metadata_known=1;
    if(ascii) memcpy(entry->label,entry->location,sizeof(entry->label));
    if(line) (void)display_duration(line,entry->play_time);
    entry->sort_start.dwLowDateTime=get32(record+0x24u);
    entry->sort_start.dwHighDateTime=get32(record+0x28u);
    uint32_t elapsed_bits=get32(record+0x2cu);
    memcpy(&entry->sort_elapsed,&elapsed_bits,sizeof(entry->sort_elapsed));
    entry->sort_known=(uint8_t)(isfinite(entry->sort_elapsed) && entry->sort_elapsed>=0.f);
    snprintf(entry->details[0],sizeof(entry->details[0]),"Party details unavailable");
    /* 501750 writes portrait IDs, not canonical SMP4 character indices.
     * HP/SP are four little-endian u16 values at the start of each record. */
    unsigned count=record[0x2b8u];
    static const char *names[4]={"Tal","Ailish","Buki","Elco"};
    if(!count || count>4u) return;
    /* Native 501A70 writes group+90 (leader) first, then subsequent party
     * slots in order. Portrait IDs are not canonical multiplayer IDs. */
    static const uint8_t characters[4]={2u,3u,0u,1u};
    uint8_t order[4]={4u,4u,4u,4u},mask=0;
    BOOL party_known=TRUE;
    for(unsigned i=0;i<count;++i) {
        const BYTE *member=record+0x7cu+i*0x4cu;
        if(member[0x4bu] || member[8]>=4u) { party_known=FALSE; break; }
        unsigned character=characters[member[8]];
        if(mask&(1u<<character)) { party_known=FALSE; break; }
        order[i]=(uint8_t)character; mask|=(uint8_t)(1u<<character);
    }
    if(party_known) {
        entry->party_known=1u; entry->party_count=(uint8_t)count;
        entry->party_mask=mask; entry->leader=order[0];
        memcpy(entry->party_order,order,sizeof(order));
    }
    memset(entry->details,0,sizeof(entry->details));
    for(unsigned i=0;i<count;++i) {
        const BYTE *member=record+0x7cu+i*0x4cu;
        unsigned hp=member[0]|((unsigned)member[1]<<8);
        unsigned hp_max=member[2]|((unsigned)member[3]<<8);
        unsigned sp=member[4]|((unsigned)member[5]<<8);
        unsigned sp_max=member[6]|((unsigned)member[7]<<8);
        const char *name=!member[0x4bu] && member[8]<4u?names[member[8]]:"Companion";
        snprintf(entry->details[i],sizeof(entry->details[i]),"%s Lv %u - HP %u/%u  SP %u/%u",
            name,(unsigned)member[0x4au],hp,hp_max,sp,sp_max);
    }
}

BOOL SudekiMpSaveCatalogRefresh(SudekiMpSaveCatalog *catalog) {
    SudekiMpSaveCatalog *next;
    HANDLE root=INVALID_HANDLE_VALUE,find=INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW data;
    wchar_t search[MAX_PATH];
    DWORD error=ERROR_SUCCESS;
    BOOL ok=FALSE;
    if(!catalog) return fail(NULL,ERROR_INVALID_PARAMETER,"Invalid save catalog.");
    next=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*next));
    if(!next) return fail(catalog,ERROR_NOT_ENOUGH_MEMORY,"Unable to read the save list.");
    if(!save_directory(next->directory)) {
        error=GetLastError(); goto done;
    }
    root=open_directory(next->directory,&next->directory_identity);
    if(root==INVALID_HANDLE_VALUE) {
        error=GetLastError();
        if(error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND) ok=TRUE;
        goto done;
    }
    if(!join(search,next->directory,L"SAVESLOT*")) { error=GetLastError(); goto done; }
    find=FindFirstFileW(search,&data);
    if(find==INVALID_HANDLE_VALUE) {
        error=GetLastError(); if(error==ERROR_FILE_NOT_FOUND) ok=TRUE;
        goto done;
    }
    unsigned visited=0;
    for(;;) {
        unsigned slot;
        if(++visited>ENUMERATION_LIMIT) { next->truncated=TRUE; break; }
        if(canonical_slot(data.cFileName,&slot)) {
            if(next->count==SUDEKIMP_SAVE_CATALOG_MAX_ENTRIES) { next->truncated=TRUE; break; }
            SavePair pair;
            if(!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) ||
                (data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) ||
                !open_pair(next->directory,slot,&pair)) ++next->skipped;
            else {
                SudekiMpSaveCatalogEntry *entry=&next->entries[next->count];
                if(same_identity(&next->directory_identity,&pair.root_identity,FALSE)) {
                    entry->folder_slot=slot; entry->fish=pair.fish_identity;
                    entry->bunny=pair.bunny_identity;
                    entry->sort_directory_modified=pair.folder_identity.modified;
                    fallback_metadata(entry);
                    native_metadata(entry,pair.bunny);
                    ++next->count;
                } else ++next->skipped;
                close_pair(&pair);
            }
        }
        if(!FindNextFileW(find,&data)) {
            error=GetLastError();
            if(error!=ERROR_NO_MORE_FILES) goto done;
            break;
        }
    }
    qsort(next->entries,next->count,sizeof(next->entries[0]),entry_compare);
    ok=TRUE;
done:
    if(find!=INVALID_HANDLE_VALUE) FindClose(find);
    if(root!=INVALID_HANDLE_VALUE) CloseHandle(root);
    if(ok) { *catalog=*next; SetLastError(ERROR_SUCCESS); }
    HeapFree(GetProcessHeap(),0,next);
    return ok?TRUE:fail(catalog,error,"Unable to refresh saves. Please try again.");
}

static BOOL hash_file(HANDLE file, const SudekiMpSaveFileIdentity *expected,
    uint64_t limit, uint8_t output[32]) {
    BCRYPT_ALG_HANDLE algorithm=NULL;
    BCRYPT_HASH_HANDLE hash=NULL;
    PUCHAR object=NULL;
    DWORD object_size=0,returned=0,read=0;
    uint64_t total=0;
    BYTE buffer[64u*1024u],digest[32];
    SudekiMpSaveFileIdentity after;
    BOOL ok=FALSE;
    DWORD error=ERROR_CRC;
    LARGE_INTEGER zero; zero.QuadPart=0;
    if(!SetFilePointerEx(file,zero,NULL,FILE_BEGIN)) return FALSE;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,NULL,0)<0 ||
        BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,(PUCHAR)&object_size,
            sizeof(object_size),&returned,0)<0 || returned!=sizeof(object_size) ||
        !object_size || object_size>64u*1024u) goto done;
    object=HeapAlloc(GetProcessHeap(),0,object_size);
    if(!object) { error=ERROR_NOT_ENOUGH_MEMORY; goto done; }
    if(BCryptCreateHash(algorithm,&hash,object,object_size,NULL,0,0)<0) goto done;
    for(;;) {
        if(!ReadFile(file,buffer,sizeof(buffer),&read,NULL)) { error=GetLastError(); goto done; }
        if(!read) break;
        total+=read;
        if(total>limit || total>expected->size) { error=ERROR_FILE_INVALID; goto done; }
        if(BCryptHashData(hash,buffer,read,0)<0) goto done;
    }
    if(total!=expected->size || !identify(file,FALSE,limit,&after) ||
        !same_identity(expected,&after,TRUE)) { error=ERROR_FILE_INVALID; goto done; }
    if(BCryptFinishHash(hash,digest,sizeof(digest),0)<0) goto done;
    memcpy(output,digest,sizeof(digest)); ok=TRUE;
done:
    if(hash) BCryptDestroyHash(hash);
    if(object) HeapFree(GetProcessHeap(),0,object);
    if(algorithm) BCryptCloseAlgorithmProvider(algorithm,0);
    if(!ok) SetLastError(error);
    return ok;
}

BOOL SudekiMpSaveCatalogFingerprint(SudekiMpSaveCatalog *catalog,
    unsigned index, SudekiMpSaveFingerprint *fingerprint) {
    SavePair pair;
    SudekiMpSaveFingerprint next={0};
    wchar_t root[MAX_PATH];
    if(!catalog || !fingerprint || catalog->count>SUDEKIMP_SAVE_CATALOG_MAX_ENTRIES ||
        index>=catalog->count)
        return fail(catalog,ERROR_INVALID_PARAMETER,"Select a save first.");
    if(!save_directory(root) || wcsncmp(root,catalog->directory,MAX_PATH))
        return fail(catalog,ERROR_INVALID_STATE,"The save folder changed. Refresh the list.");
    const SudekiMpSaveCatalogEntry *entry=&catalog->entries[index];
    if(!open_pair(root,entry->folder_slot,&pair))
        return fail(catalog,GetLastError(),"Unable to read this save. Refresh and try again.");
    if(!same_identity(&catalog->directory_identity,&pair.root_identity,FALSE) ||
        !same_identity(&entry->fish,&pair.fish_identity,TRUE) ||
        !same_identity(&entry->bunny,&pair.bunny_identity,TRUE)) {
        close_pair(&pair);
        return fail(catalog,ERROR_FILE_INVALID,"This save changed. Refresh the list and select it again.");
    }
    next.folder_slot=entry->folder_slot;
    BOOL ok=hash_file(pair.fish,&entry->fish,FISH_LIMIT,next.fish_sha256) &&
        hash_file(pair.bunny,&entry->bunny,BUNNY_LIMIT,next.bunny_sha256);
    DWORD error=GetLastError();
    close_pair(&pair);
    if(!ok) return fail(catalog,error,"Unable to verify this save. Refresh and try again.");
    *fingerprint=next; catalog->error[0]='\0'; SetLastError(ERROR_SUCCESS); return TRUE;
}

BOOL SudekiMpSaveCatalogVerify(SudekiMpSaveCatalog *catalog,
    unsigned index, const SudekiMpSaveFingerprint *fingerprint) {
    SudekiMpSaveFingerprint actual;
    if(!fingerprint) return fail(catalog,ERROR_INVALID_PARAMETER,"Select a save first.");
    if(!SudekiMpSaveCatalogFingerprint(catalog,index,&actual)) return FALSE;
    if(actual.folder_slot!=fingerprint->folder_slot ||
        memcmp(actual.fish_sha256,fingerprint->fish_sha256,sizeof(actual.fish_sha256)) ||
        memcmp(actual.bunny_sha256,fingerprint->bunny_sha256,sizeof(actual.bunny_sha256)))
        return fail(catalog,ERROR_FILE_INVALID,"This save changed after review. Select it again.");
    return TRUE;
}

struct SudekiMpSaveLease {
    SavePair pair;
    unsigned folder_slot;
    BYTE record[0x2c8];
    wchar_t fish_path[MAX_PATH],bunny_path[MAX_PATH];
};

BOOL SudekiMpSaveCatalogAcquire(SudekiMpSaveCatalog *catalog,unsigned index,
    const SudekiMpSaveFingerprint *expected,SudekiMpSaveLease **out) {
    wchar_t root[MAX_PATH];
    SudekiMpSaveFingerprint actual={0};
    DWORD read=0,error=ERROR_FILE_INVALID;
    BYTE metadata[0x600];
    LARGE_INTEGER zero; zero.QuadPart=0;
    if(!catalog || !expected || !out || *out ||
        catalog->count>SUDEKIMP_SAVE_CATALOG_MAX_ENTRIES || index>=catalog->count)
        return fail(catalog,ERROR_INVALID_PARAMETER,"Select a save first.");
    const SudekiMpSaveCatalogEntry *entry=&catalog->entries[index];
    if(expected->folder_slot!=entry->folder_slot || !save_directory(root) ||
        wcsncmp(root,catalog->directory,MAX_PATH))
        return fail(catalog,ERROR_FILE_INVALID,"The selected save changed. Select it again.");
    SudekiMpSaveLease *lease=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*lease));
    if(!lease) return fail(catalog,ERROR_NOT_ENOUGH_MEMORY,"Unable to retain this save.");
    if(!open_pair(root,entry->folder_slot,&lease->pair)) goto bad;
    wchar_t folder_name[16],folder[MAX_PATH],path[MAX_PATH];
    swprintf(folder_name,16,L"SAVESLOT%04u",entry->folder_slot);
    if(!join(folder,root,folder_name) || !join(path,folder,L"sudeki.fish")) goto bad;
    DWORD path_length=GetFullPathNameW(path,MAX_PATH,lease->fish_path,NULL);
    if(!path_length || path_length>=MAX_PATH || !join(path,folder,L"fluffy.bunny")) goto bad;
    path_length=GetFullPathNameW(path,MAX_PATH,lease->bunny_path,NULL);
    if(!path_length || path_length>=MAX_PATH) goto bad;
    if(!same_identity(&catalog->directory_identity,&lease->pair.root_identity,FALSE) ||
        !same_identity(&entry->fish,&lease->pair.fish_identity,TRUE) ||
        !same_identity(&entry->bunny,&lease->pair.bunny_identity,TRUE)) { SetLastError(ERROR_FILE_INVALID); goto bad; }
    actual.folder_slot=entry->folder_slot;
    if(!hash_file(lease->pair.fish,&entry->fish,FISH_LIMIT,actual.fish_sha256) ||
        !hash_file(lease->pair.bunny,&entry->bunny,BUNNY_LIMIT,actual.bunny_sha256)) goto bad;
    if(memcmp(actual.fish_sha256,expected->fish_sha256,32) ||
        memcmp(actual.bunny_sha256,expected->bunny_sha256,32)) { SetLastError(ERROR_FILE_INVALID); goto bad; }
    /* This native adapter knows only the exact metadata shape researched for
     * this build. Catalog display fallback is deliberately more permissive. */
    if(entry->bunny.size!=sizeof(metadata) || entry->fish.size!=0x280200u) {
        SetLastError(ERROR_INVALID_DATA); goto bad;
    }
    /* The supported native reader allocates 0x280000 for the fish payload.
     * Catalog display may list other shapes; this loader refuses them. */
    if(!SetFilePointerEx(lease->pair.fish,zero,NULL,FILE_BEGIN) ||
        !ReadFile(lease->pair.fish,metadata,4,&read,NULL)) goto bad;
    if(read!=4 || get32(metadata)!=0x280000u) { SetLastError(ERROR_INVALID_DATA); goto bad; }
    if(!SetFilePointerEx(lease->pair.bunny,zero,NULL,FILE_BEGIN) ||
        !ReadFile(lease->pair.bunny,metadata,sizeof(metadata),&read,NULL) ||
        read!=sizeof(metadata)) goto bad;
    if(get32(metadata)!=0x400u || get32(metadata+0x220)!=15u || metadata[0x4b8]>4u) {
        SetLastError(ERROR_INVALID_DATA); goto bad;
    }
    memcpy(lease->record,metadata+0x200,sizeof(lease->record));
    lease->folder_slot=entry->folder_slot;
    *out=lease; catalog->error[0]='\0'; SetLastError(ERROR_SUCCESS); return TRUE;
bad:
    if(GetLastError()!=ERROR_SUCCESS) error=GetLastError();
    close_pair(&lease->pair); HeapFree(GetProcessHeap(),0,lease);
    return fail(catalog,error,"Unable to retain the reviewed save. Refresh and select it again.");
}

BOOL SudekiMpSaveLeaseMatchesRoot(const SudekiMpSaveLease *lease,
    const wchar_t *native_directory) {
    SudekiMpSaveFileIdentity root,fish,bunny;
    if(!lease || !native_directory || !*native_directory) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    HANDLE h=open_directory(native_directory,&root);
    if(h==INVALID_HANDLE_VALUE) return FALSE;
    BOOL ok=same_identity(&lease->pair.root_identity,&root,FALSE) &&
        identify(lease->pair.fish,FALSE,FISH_LIMIT,&fish) &&
        identify(lease->pair.bunny,FALSE,BUNNY_LIMIT,&bunny) &&
        same_identity(&lease->pair.fish_identity,&fish,TRUE) &&
        same_identity(&lease->pair.bunny_identity,&bunny,TRUE);
    CloseHandle(h);
    if(!ok) SetLastError(ERROR_FILE_INVALID);
    return ok;
}

BOOL SudekiMpSaveLeaseRecord(const SudekiMpSaveLease *lease,
    uint8_t record[0x2c8],unsigned *slot) {
    if(!lease || !record || !slot) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    memcpy(record,lease->record,sizeof(lease->record)); *slot=lease->folder_slot; return TRUE;
}

BOOL SudekiMpSaveLeaseOpenNativeRead(const SudekiMpSaveLease *lease,
    const wchar_t *path,HANDLE *opened) {
    wchar_t full[MAX_PATH];
    if(!lease || !path || !opened) return FALSE;
    DWORD n=GetFullPathNameW(path,MAX_PATH,full,NULL);
    if(!n || n>=MAX_PATH) return FALSE;
    const SudekiMpSaveFileIdentity *expected=NULL;
    if(!lstrcmpiW(full,lease->fish_path)) expected=&lease->pair.fish_identity;
    else if(!lstrcmpiW(full,lease->bunny_path)) expected=&lease->pair.bunny_identity;
    if(!expected) return FALSE;
    *opened=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    if(*opened==INVALID_HANDLE_VALUE) return TRUE;
    SudekiMpSaveFileIdentity actual;
    if(!identify(*opened,FALSE,FISH_LIMIT,&actual) || !same_identity(expected,&actual,TRUE)) {
        CloseHandle(*opened); *opened=INVALID_HANDLE_VALUE;
        SetLastError(ERROR_FILE_INVALID);
    }
    return TRUE;
}

void SudekiMpSaveLeaseRelease(SudekiMpSaveLease **lease) {
    if(!lease || !*lease) return;
    close_pair(&(*lease)->pair); HeapFree(GetProcessHeap(),0,*lease); *lease=NULL;
}
