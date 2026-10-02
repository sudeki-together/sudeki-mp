#include "loader/lobby_launch.h"
#include "engine/build_identity.h"
#include "engine/sha256.h"
#include <bcrypt.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define LAUNCH_MAGIC 0x32424c53u
static HANDLE child_mapping;
static SudekiMpLobbyLaunchShared *child_shared;

static BOOL module_paths(wchar_t game[MAX_PATH],wchar_t dll[MAX_PATH],wchar_t launcher[MAX_PATH]) {
    HMODULE self=NULL;
    DWORD n;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)&child_mapping,&self)) return FALSE;
    n=GetModuleFileNameW(NULL,game,MAX_PATH);
    if (!n || n>=MAX_PATH) return FALSE;
    n=GetModuleFileNameW(self,dll,MAX_PATH);
    if (!n || n>=MAX_PATH) return FALSE;
    n=GetEnvironmentVariableW(L"SUDEKIMP_LAUNCHER_PATH",launcher,MAX_PATH);
    return n && n<MAX_PATH && !wcschr(game,L'"') && !wcschr(dll,L'"') &&
        !wcschr(launcher,L'"') && GetFileAttributesW(launcher)!=INVALID_FILE_ATTRIBUTES;
}
BOOL SudekiMpLobbyLaunchAvailable(void) {
    wchar_t game[MAX_PATH],dll[MAX_PATH],launcher[MAX_PATH];
    return module_paths(game,dll,launcher);
}
static BOOL valid_plan(const SudekiMpLobbyLaunchPlan *p) {
    if (!p->revision || !p->generation || p->seat>=4 || !(p->members&1u) ||
        p->members>15 || !(p->members&(1u<<p->seat)) ||
        (p->seat && (!p->port || !p->host_ipv4[0])) || p->host_ipv4[15]) return FALSE;
    for (unsigned i=0;i<4;++i)
        if ((p->seat ? i==p->seat : (p->members>>i)&1u)!=(p->nonce[i]!=0)) return FALSE;
    for (unsigned i=0;p->host_ipv4[i];++i)
        if ((p->host_ipv4[i]<'0' || p->host_ipv4[i]>'9') && p->host_ipv4[i]!='.') return FALSE;
    return TRUE;
}
BOOL SudekiMpLobbyLaunchOpen(const wchar_t *name,HANDLE *handle,SudekiMpLobbyLaunchShared **out) {
    if (!name || wcsncmp(name,L"Local\\SudekiMP-Lobby-",21) || wcslen(name)>80) return FALSE;
    for (unsigned i=21;name[i];++i)
        if (!((name[i]>=L'0' && name[i]<=L'9') || (name[i]>=L'a' && name[i]<=L'f') || name[i]==L'-')) return FALSE;
    HANDLE h=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name);
    if (!h) return FALSE;
    SudekiMpLobbyLaunchShared *s=MapViewOfFile(h,FILE_MAP_ALL_ACCESS,0,0,sizeof(*s));
    if (!s || s->magic!=LAUNCH_MAGIC || s->version!=1 || s->size!=sizeof(*s) || !s->owner_pid ||
        !valid_plan(&s->plan) || !wmemchr(s->game,0,MAX_PATH) || !wmemchr(s->dll,0,MAX_PATH) ||
        wcschr(s->game,L'"') || wcschr(s->dll,L'"') || s->dll_hash[64]) {
        if (s) UnmapViewOfFile(s);
        CloseHandle(h); return FALSE;
    }
    *handle=h; *out=s; return TRUE;
}
BOOL SudekiMpLobbyLaunchPrepare(SudekiMpLobbyLaunch *launch,const SudekiMpLobbyLaunchPlan *plan) {
    wchar_t game[MAX_PATH],dll[MAX_PATH],launcher[MAX_PATH],name[96],command[512];
    uint64_t random;
    if (!launch || launch->mapping || !valid_plan(plan) || !module_paths(game,dll,launcher) ||
        BCryptGenRandom(NULL,(PUCHAR)&random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)) return FALSE;
    _snwprintf(name,96,L"Local\\SudekiMP-Lobby-%lx-%016llx",(unsigned long)GetCurrentProcessId(),
        (unsigned long long)random);
    HANDLE h=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(SudekiMpLobbyLaunchShared),name);
    if (!h) return FALSE;
    if (GetLastError()==ERROR_ALREADY_EXISTS) { CloseHandle(h); return FALSE; }
    SudekiMpLobbyLaunchShared *s=MapViewOfFile(h,FILE_MAP_ALL_ACCESS,0,0,sizeof(*s));
    if (!s) { CloseHandle(h); return FALSE; }
    memset(s,0,sizeof(*s)); s->magic=LAUNCH_MAGIC; s->version=1; s->size=sizeof(*s);
    s->owner_pid=GetCurrentProcessId(); s->plan=*plan;
    wcscpy(s->game,game); wcscpy(s->dll,dll);
    if (!SudekiMpSha256File(dll,s->dll_hash)) goto fail;
    _snwprintf(command,512,L"\"%ls\" --lobby \"%ls\"",launcher,name);
    STARTUPINFOW startup={.cb=sizeof(startup)};
    PROCESS_INFORMATION process={0};
    if (!CreateProcessW(launcher,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process)) goto fail;
    CloseHandle(process.hThread);
    launch->mapping=h; launch->shared=s; launch->launcher=process.hProcess;
    return TRUE;
fail:
    UnmapViewOfFile(s); CloseHandle(h); return FALSE;
}
unsigned SudekiMpLobbyLaunchPoll(SudekiMpLobbyLaunch *launch,unsigned *port) {
    if (port) *port=0;
    if (!launch || !launch->shared) return SUDEKIMP_LAUNCH_NEW;
    if (WaitForSingleObject(launch->launcher,0)==WAIT_OBJECT_0) return SUDEKIMP_LAUNCH_FAILED;
    if (port) *port=(unsigned)InterlockedCompareExchange(&launch->shared->bound_port,0,0);
    return (unsigned)InterlockedCompareExchange(&launch->shared->state,0,0);
}
void SudekiMpLobbyLaunchCommand(SudekiMpLobbyLaunch *launch,unsigned command) {
    if (launch && launch->shared && command<=SUDEKIMP_LAUNCH_DETACH)
        InterlockedExchange(&launch->shared->command,(LONG)command);
}
BOOL SudekiMpLobbyLaunchRelease(SudekiMpLobbyLaunch *launch) {
    if (!launch || !launch->shared) return TRUE;
    if (InterlockedCompareExchange(&launch->shared->command,0,0)!=SUDEKIMP_LAUNCH_DETACH &&
        WaitForSingleObject(launch->launcher,0)!=WAIT_OBJECT_0) return FALSE;
    CloseHandle(launch->launcher); UnmapViewOfFile(launch->shared); CloseHandle(launch->mapping);
    memset(launch,0,sizeof(*launch)); return TRUE;
}
int SudekiMpLobbyLaunchChildConfig(HMODULE dll,SudekiMpLobbyLaunchPlan *plan) {
    wchar_t name[96],game_path[MAX_PATH],dll_path[MAX_PATH];
    DWORD n=GetEnvironmentVariableW(SUDEKIMP_LOBBY_LAUNCH_ENV,name,96);
    if (!n) return 0;
    if (n>=96 || child_shared || !SudekiMpLobbyLaunchOpen(name,&child_mapping,&child_shared)) return -1;
    n=GetModuleFileNameW(NULL,game_path,MAX_PATH);
    DWORD d=GetModuleFileNameW(dll,dll_path,MAX_PATH);
    char hash[65];
    if (!n || n>=MAX_PATH || !d || d>=MAX_PATH || child_shared->child_pid!=GetCurrentProcessId() ||
        _wcsicmp(game_path,child_shared->game) || _wcsicmp(dll_path,child_shared->dll) ||
        !SudekiMpSha256File(dll_path,hash) || strcmp(hash,child_shared->dll_hash)) return -1;
    *plan=child_shared->plan;
    return 1;
}
BOOL SudekiMpLobbyLaunchChildRunning(void) {
    if (!child_shared) return TRUE;
    LONG command=InterlockedCompareExchange(&child_shared->command,0,0);
    return command==SUDEKIMP_LAUNCH_RUN || command==SUDEKIMP_LAUNCH_DETACH;
}
void SudekiMpLobbyLaunchChildPort(unsigned port) {
    if (child_shared) InterlockedExchange(&child_shared->bound_port,(LONG)port);
}
void SudekiMpLobbyLaunchChildLoaded(void) {
    if (child_shared && SudekiMpLobbyLaunchChildRunning())
        InterlockedCompareExchange(&child_shared->state,SUDEKIMP_LAUNCH_LOADED,SUDEKIMP_LAUNCH_PREPARED);
}
