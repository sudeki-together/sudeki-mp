#include "engine/story_loot_sidecar.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int path_valid(const char *path) {
    static const char suffix[]=".smploot";
    if(!path) return 0;
    size_t n=strlen(path),tail=sizeof(suffix)-1;
    /* Reject alternate data streams and device paths. Drive-prefix ':' is
     * the only colon permitted. Integration still owns the save directory. */
    if(n<=tail || n>MAX_PATH-48 || strcmp(path+n-tail,suffix)!=0 ||
        !strncmp(path,"\\\\.\\",4) || !strncmp(path,"\\\\?\\",4)) return 0;
    for(size_t i=0;i<n;++i) if(path[i]==':' && !(i==1 &&
        ((path[0]>='A' && path[0]<='Z') || (path[0]>='a' && path[0]<='z')))) return 0;
    return 1;
}
SudekiMpStoryLootFileResult SudekiMpStoryLootReadFile(const char *path,const uint8_t expected[32],
    SudekiMpStoryLootState *out) {
    if(!path_valid(path) || !expected || !out) return SUDEKIMP_STORY_LOOT_FILE_INVALID;
    HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(file==INVALID_HANDLE_VALUE) {
        DWORD error=GetLastError();
        return error==ERROR_FILE_NOT_FOUND?SUDEKIMP_STORY_LOOT_FILE_MISSING:SUDEKIMP_STORY_LOOT_FILE_ERROR;
    }
    BY_HANDLE_FILE_INFORMATION info; LARGE_INTEGER size; DWORD got=0;
    int valid=GetFileInformationByHandle(file,&info) &&
        !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) &&
        GetFileSizeEx(file,&size) && size.QuadPart>=SUDEKIMP_STORY_LOOT_HEADER_SIZE &&
        size.QuadPart<=SUDEKIMP_STORY_LOOT_MAX_SIZE;
    uint8_t *bytes=valid?malloc((size_t)size.QuadPart):NULL;
    SudekiMpStoryLootFileResult result=SUDEKIMP_STORY_LOOT_FILE_INVALID;
    if(valid && !bytes) result=SUDEKIMP_STORY_LOOT_FILE_ERROR;
    else if(bytes) {
        if(!ReadFile(file,bytes,(DWORD)size.QuadPart,&got,NULL) || got!=(DWORD)size.QuadPart)
            result=SUDEKIMP_STORY_LOOT_FILE_ERROR;
        else if(SudekiMpStoryLootDecode(bytes,got,expected,out)) result=SUDEKIMP_STORY_LOOT_FILE_OK;
    }
    free(bytes); CloseHandle(file); return result;
}
SudekiMpStoryLootFileResult SudekiMpStoryLootWriteFile(const char *path,const SudekiMpStoryLootState *s) {
    if(!path_valid(path) || !SudekiMpStoryLootValid(s)) return SUDEKIMP_STORY_LOOT_FILE_INVALID;
    /* Single writer lock created exclusively in the same directory. Never
     * truncate another writer's temporary or remove an unowned lock. */
    char lock_path[MAX_PATH],temp_path[MAX_PATH];
    snprintf(lock_path,sizeof(lock_path),"%s.lock",path);
    HANDLE lock=CreateFileA(lock_path,GENERIC_WRITE,0,NULL,CREATE_NEW,
        FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,NULL);
    if(lock==INVALID_HANDLE_VALUE) return SUDEKIMP_STORY_LOOT_FILE_ERROR;
    HANDLE file=INVALID_HANDLE_VALUE;
    for(unsigned attempt=0;attempt<32 && file==INVALID_HANDLE_VALUE;++attempt) {
        snprintf(temp_path,sizeof(temp_path),"%s.%08lx.%08lx.%u.tmp",path,
            (unsigned long)GetCurrentProcessId(),(unsigned long)GetTickCount(),attempt);
        file=CreateFileA(temp_path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(file==INVALID_HANDLE_VALUE && GetLastError()!=ERROR_FILE_EXISTS && GetLastError()!=ERROR_ALREADY_EXISTS) break;
    }
    SudekiMpStoryLootFileResult result=SUDEKIMP_STORY_LOOT_FILE_ERROR;
    if(file!=INVALID_HANDLE_VALUE) {
        uint8_t *bytes=malloc(SUDEKIMP_STORY_LOOT_MAX_SIZE); size_t n=0; DWORD wrote=0;
        int ok=bytes && SudekiMpStoryLootEncode(s,bytes,SUDEKIMP_STORY_LOOT_MAX_SIZE,&n) &&
            WriteFile(file,bytes,(DWORD)n,&wrote,NULL) && wrote==n && FlushFileBuffers(file);
        if(!CloseHandle(file)) ok=0;
        free(bytes);
        if(ok) {
            DWORD attributes=GetFileAttributesA(path);
            if(attributes==INVALID_FILE_ATTRIBUTES && GetLastError()==ERROR_FILE_NOT_FOUND) {
                /* No REPLACE flag: a concurrently created target wins. */
                ok=MoveFileExA(temp_path,path,MOVEFILE_WRITE_THROUGH)!=FALSE;
            } else if(attributes!=INVALID_FILE_ATTRIBUTES &&
                !(attributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))) {
                /* Never overwrite corrupt, foreign-save or newer sidecars. */
                SudekiMpStoryLootState prior;
                ok=SudekiMpStoryLootReadFile(path,s->save_identity,&prior)==SUDEKIMP_STORY_LOOT_FILE_OK &&
                    SudekiMpStoryLootAdvances(&prior,s);
                if(ok) ok=ReplaceFileA(path,temp_path,NULL,0,NULL,NULL)!=FALSE;
            } else ok=0;
        }
        if(ok) result=SUDEKIMP_STORY_LOOT_FILE_OK;
        else (void)DeleteFileA(temp_path); /* only this call's CREATE_NEW file */
    }
    CloseHandle(lock); return result;
}
