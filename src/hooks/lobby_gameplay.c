#include "hooks/lobby_gameplay.h"
#include "hooks/lan_party_runtime.h"
#include "hooks/lan_party_control.h"
#include "hooks/lan_story_runtime.h"
#include "hooks/lan_story_load.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/sha256.h"
#include "engine/log.h"
#include <string.h>
#include <wchar.h>

/* Exact-image + exact script evidence: SimpleFrontEndMenuSelect passes the
 * selected string to ExecuteSelectedMenuItem. A .zone name follows IsZoneFile,
 * StripZoneExt, StartTestGame, native StartGame and CWorld__OnTestLevel. Keep
 * that original script and the front end's state-10 fade/exit intact. */
static uint8_t *base;
static void *dispatch_original __attribute__((used));
static SudekiMpRelativeCallHook dispatch_hook, argument_hooks[5];
static const unsigned argument_sites[]={0x6b2d,0x6b6a,0x6ba6,0x6be2,0x6c1e};
static const unsigned argument_strings[]={0x2c4b30,0x2c4b34,0x2c4b40,0x2c4b48,0x2c4b50};
static const unsigned argument_seats[]={4,2,0,1,3};
static SudekiMpLobbyLaunchPlan plan;
static DWORD native_thread;
static void *title_owner, *room_world, *room_descriptor;
static volatile LONG active, running, loaded, callbacks;
static BOOL requested, dispatched, failed;
static BOOL runtime_attempted;
static BOOL saved_enabled,saved_game,testroom_enabled=TRUE;
static unsigned story_exit_status;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a+n>=a && VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL thread_exact(void) { return native_thread && native_thread==GetCurrentThreadId(); }
static BOOL title_identity(uint8_t **scene_out,uint8_t **resident_out) {
    if(!base || !thread_exact()) return FALSE;
    uint8_t *scene=*(uint8_t **)(base+0x408d1c),*resident;
    if(!readable(scene,0x178) || !readable(resident=*(void **)(scene+0x174),0xc0) ||
        *(void **)(resident+0x94)!=title_owner || !readable(title_owner,0x1844) ||
        *(void **)title_owner!=base+0x2cb1fc) return FALSE;
    if(scene_out) *scene_out=scene;
    if(resident_out) *resident_out=resident;
    return TRUE;
}
static BOOL quit_entry_exact(void) {
    static const uint8_t tail[]={0x85,0xc0,0x74,0x05,0xe9,0xbe,0xfe,0xff,0xff,0xc3};
    return base && base[0xa2740]==0xa1 &&
        *(void **)(base+0xa2741)==base+0x408d1c &&
        !memcmp(base+0xa2759,tail,sizeof(tail));
}
static BOOL retain_story_exit(void) {
    HMODULE self;
    (void)GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCWSTR)&story_exit_status,&self);
    SetLastError(ERROR_BUSY); return FALSE;
}
static BOOL room_identity(void) {
    if (!base || !thread_exact() || !dispatched || saved_game) return FALSE;
    uint8_t *w=*(uint8_t **)(base+0x408d10), *d;
    if (!readable(w,0x39d) || !readable(d=*(void **)(w+0xc),0x38)) return FALSE;
    const char *name=*(const char **)(d+0x24);
    return readable(name,9) && !lstrcmpiA(name,"testroom") &&
        (!room_world || (room_world==w && room_descriptor==d));
}
static BOOL room_ready(unsigned character) {
    if (!InterlockedCompareExchange(&active,0,0) || character!=plan.character[plan.seat] || !room_world || !room_identity()) return FALSE;
    uint8_t *w=room_world,*d=room_descriptor;
    return !*(void **)(w+0x14) && w[0x399] && w[0x39a] && *(uint32_t *)(d+0x34)==3u;
}
__attribute__((noinline,used,force_align_arg_pointer))
static const char *__cdecl argument_value(void *args,const char *key,void *caller) {
    InterlockedIncrement(&callbacks);
    const char *result=NULL;
    if (InterlockedCompareExchange(&active,0,0) && args==*(void **)(base+0x408d88) &&
        readable(args,0x1c) && room_identity()) {
        for (unsigned i=0;i<5;++i) if (caller==base+argument_sites[i]+5 && key==(const char *)base+argument_strings[i]) {
            room_world=*(void **)(base+0x408d10);
            room_descriptor=*(void **)((uint8_t *)room_world+0xc);
            result=(i==0 || argument_seats[i]==plan.character[plan.seat])?"1":"0";
            if (!i) SudekiMpLogFormat("lobby_gameplay event=native_testroom_setup seat=%u scope=exact_world\r\n",plan.seat);
            break;
        }
    }
    if (!result) {
        void *fn=base+0x22300;
        /* Native CInputArgs lookup: EDI=owner, one stack word, ret 4. */
        __asm__ volatile("push %[key]\n\tcall *%[fn]"
            : "=a"(result) : "D"(args),[key]"r"(key),[fn]"r"(fn) : "ecx","edx","cc","memory");
    }
    InterlockedDecrement(&callbacks); return result;
}
__attribute__((naked)) static void argument_entry(void) {
    __asm__ volatile("push (%esp)\n\tpush 8(%esp)\n\tpush %edi\n\tcall _argument_value\n\tadd $12,%esp\n\tret $4");
}
__attribute__((noinline,used,force_align_arg_pointer))
static const char *__cdecl selected_action(void *owner,const char *action) {
    InterlockedIncrement(&callbacks);
    if (thread_exact() && !saved_game && requested && !dispatched && owner==title_owner &&
        readable(owner,0x1844) && *(void **)owner==base+0x2cb1fc &&
        *(unsigned *)((uint8_t *)owner+0x44)==0 && *(unsigned *)((uint8_t *)owner+0x48)==10 &&
        readable(action,13) && !memcmp(action,"StartNewGame",13)) {
        dispatched=TRUE; InterlockedExchange(&running,1);
        action="testroom.zone";
        SudekiMpLogFormat("lobby_gameplay event=native_level_selected seat=%u process=%lu window=same\r\n",
            plan.seat,(unsigned long)GetCurrentProcessId());
    }
    InterlockedDecrement(&callbacks); return action;
}
__attribute__((naked)) static void selected_entry(void) {
    /* The native setter saves/restores ECX in addition to its usual callee
     * registers. Preserve that contract across our C selector too. */
    __asm__ volatile("push %ecx\n\tpush %eax\n\tpush %edi\n\tcall _selected_action\n\tadd $8,%esp\n\tpop %ecx\n\tjmp *_dispatch_original");
}
static BOOL scripts_exact(HMODULE game) {
    static const wchar_t *const names[]={L"Data\\SOLWORLDM.gex",L"Data\\WINSOLM.gex"};
    static const char *const hashes[]={
        "e36a5974f9aedea5b5b428fe2445cf496c52911ff01d4934ea8ab8124abf1ff9",
        "7502aaa1a9904d164f2a64d660c7d54b9ed88fe98c03077723b2f37f07a01277"};
    wchar_t path[MAX_PATH]; DWORD n=GetModuleFileNameW(game,path,MAX_PATH);
    if (!n || n>=MAX_PATH) return FALSE;
    wchar_t *tail=wcsrchr(path,L'\\'); if (!tail || (size_t)(tail-path)+24>=MAX_PATH) return FALSE;
    for (unsigned i=0;i<2;++i) { char hash[65]; wcscpy(tail+1,names[i]);
        if (!SudekiMpSha256File(path,hash) || strcmp(hash,hashes[i])) return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpInstallLobbyGameplay(HMODULE game) {
    if (!game || base || !SudekiMpCheckLoadedExecutable(game) || !scripts_exact(game)) return FALSE;
    uint8_t *b=(uint8_t *)game;
    static const uint8_t dispatch[]={0x51,0x56,0x50,0x8d,0xb7,0xe0,0x17,0x00,0x00};
    static const uint8_t lookup[]={0x53,0x8b,0x5c,0x24,0x08,0x56,0x33,0xf6};
    static const char *const keys[]={"-DT","-Tal","-Buki","-Elco","-Ailish"};
    static const uint8_t quit_tail[]={0x85,0xc0,0x74,0x05,0xe9,0xbe,0xfe,0xff,0xff,0xc3};
    if (memcmp(b+0xa1900,dispatch,sizeof(dispatch)) || memcmp(b+0x22300,lookup,sizeof(lookup)) ||
        b[0xa2740]!=0xa1 || *(void **)(b+0xa2741)!=b+0x408d1c ||
        memcmp(b+0xa2759,quit_tail,sizeof(quit_tail))) return FALSE;
    for (unsigned i=0;i<5;++i)
        if (memcmp(b+argument_strings[i],keys[i],strlen(keys[i])+1)) return FALSE;
    base=b; dispatch_original=b+0xa1900;
    if (!SudekiMpInstallRelativeCallHook(&dispatch_hook,b+0xa0fd5,dispatch_original,selected_entry)) goto fail;
    for (unsigned i=0;i<5;++i)
        if (!SudekiMpInstallRelativeCallHook(&argument_hooks[i],b+argument_sites[i],b+0x22300,argument_entry)) goto fail;
    if (!SudekiMpLanPartyControlSetTestroomProbe(room_ready)) goto fail;
    return TRUE;
fail:
    (void)SudekiMpUninstallLobbyGameplay(); return FALSE;
}
BOOL SudekiMpUninstallLobbyGameplay(void) {
    if (InterlockedCompareExchange(&active,0,0)) { SetLastError(ERROR_BUSY); return FALSE; }
    BOOL okay=TRUE;
    for (unsigned i=5;i>0;--i) if (!SudekiMpRestoreRelativeCallHook(&argument_hooks[i-1])) okay=FALSE;
    if (!SudekiMpRestoreRelativeCallHook(&dispatch_hook)) okay=FALSE;
    if (!okay || InterlockedCompareExchange(&callbacks,0,0)) {
        HMODULE self;
        (void)GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCWSTR)&dispatch_hook,&self);
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if (!SudekiMpLanPartyControlSetTestroomProbe(NULL)) return FALSE;
    /* Keep original callback dependencies immutable until process exit. */
    return TRUE;
}
static BOOL prepare_save(const SudekiMpLobbySavedGame *wanted) {
    SudekiMpSaveCatalog catalog;
    SudekiMpSaveFingerprint local;
    unsigned selected=~0u;
    if (!wanted || wanted->folder_slot>9999u || !SudekiMpSaveCatalogRefresh(&catalog)) return FALSE;
    /* Prefer the explicitly named folder. Matching files in another local
     * folder are also usable, but only one match may exist in that fallback. */
    for (unsigned i=0;i<catalog.count;++i) if (catalog.entries[i].folder_slot==wanted->folder_slot &&
        SudekiMpSaveCatalogFingerprint(&catalog,i,&local) &&
        !memcmp(local.fish_sha256,wanted->fish_sha256,32) &&
        !memcmp(local.bunny_sha256,wanted->bunny_sha256,32)) {
        selected=i; break;
    }
    if (selected==~0u) {
        if (catalog.truncated) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return FALSE; }
        for (unsigned i=0;i<catalog.count;++i) {
            SudekiMpSaveFingerprint candidate;
            if (!SudekiMpSaveCatalogFingerprint(&catalog,i,&candidate)) return FALSE;
            if (memcmp(candidate.fish_sha256,wanted->fish_sha256,32) ||
                memcmp(candidate.bunny_sha256,wanted->bunny_sha256,32)) continue;
            if (selected!=~0u) { SetLastError(ERROR_DUP_NAME); return FALSE; }
            selected=i; local=candidate;
        }
    }
    if (selected==~0u) { SetLastError(ERROR_FILE_NOT_FOUND); return FALSE; }
    /* Both peers inspect their fingerprint-matching local copy. A remote
     * advertised leader cannot override the native save's own metadata. */
    if(!catalog.entries[selected].party_known || catalog.entries[selected].leader!=wanted->leader ||
        catalog.entries[selected].party_count!=wanted->party_count ||
        catalog.entries[selected].party_mask!=wanted->party_mask ||
        memcmp(catalog.entries[selected].party_order,wanted->party_order,4u)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    if (!SudekiMpLanStoryLoadPrepare(&catalog,selected,&local)) return FALSE;
    SudekiMpLogFormat("lobby_gameplay event=saved_pair_prepared host_folder=%lu local_folder=%lu files=both_verified writes=0\r\n",
        (unsigned long)wanted->folder_slot,(unsigned long)local.folder_slot);
    return TRUE;
}
static BOOL prepare(const SudekiMpLobbyLaunchPlan *p,const SudekiMpLobbySavedGame *save) {
    if (!base || active || !SudekiMpLobbyLaunchPlanValid(p) ||
        (save?!saved_enabled:!testroom_enabled)) return FALSE;
    native_thread=GetCurrentThreadId(); plan=*p; title_owner=room_world=room_descriptor=NULL;
    saved_game=save!=NULL; runtime_attempted=FALSE;
    story_exit_status=0;
    requested=dispatched=failed=FALSE; InterlockedExchange(&running,0); InterlockedExchange(&loaded,0);
    InterlockedExchange(&active,1);
    SudekiMpLanPartyConfig c={.local_seat=p->seat,.host_ipv4=p->seat?p->host_ipv4:NULL,
        .port=p->port,.timeout_ms=10000,.lobby_members=p->members,
        .reserved_mask=p->reserved_mask,.assignment_enabled=1,.story_observation=save?2u:0u};
    memcpy(c.character,p->character,sizeof(c.character));
    memcpy(c.lobby_nonce,p->nonce,sizeof(c.lobby_nonce));
    for (unsigned i=0;i<32;++i) {
        const char *hex=SUDEKIMP_EXPECTED_SHA256; unsigned value=0;
        for(unsigned j=0;j<2;++j) { char ch=hex[i*2+j];value=value*16+(ch<='9'?ch-'0':ch-'a'+10); }
        c.game_hash[i]=(uint8_t)value;
    }
    if (save && !prepare_save(save)) {
        failed=TRUE;
        SudekiMpLogFormat("lobby_gameplay event=saved_pair_refused seat=%u error=%lu\r\n",p->seat,(unsigned long)GetLastError());
        return FALSE;
    }
    runtime_attempted=TRUE;
    if (!(save?SudekiMpInstallLanStoryRuntime((HMODULE)base,&c):SudekiMpInstallLanPartyRuntime((HMODULE)base,&c))) {
        failed=TRUE;
        SudekiMpLogFormat("lobby_gameplay event=prepare_failed seat=%u error=%lu\r\n",p->seat,(unsigned long)GetLastError());
        return FALSE;
    }
    SudekiMpLogFormat("lobby_gameplay event=prepared seat=%u revision=%lu process=%lu destination=%s\r\n",
        p->seat,(unsigned long)p->revision,(unsigned long)GetCurrentProcessId(),save?"saved_game":"testroom");
    return TRUE;
}
BOOL SudekiMpLobbyGameplayPrepare(const SudekiMpLobbyLaunchPlan *p) { return prepare(p,NULL); }
BOOL SudekiMpLobbyGameplayPrepareSaved(const SudekiMpLobbyLaunchPlan *p,const SudekiMpLobbySavedGame *save) {
    return save && prepare(p,save);
}
BOOL SudekiMpLobbyGameplayEnableSaved(BOOL enabled) {
    if (!base || active) { SetLastError(ERROR_BUSY); return FALSE; }
    saved_enabled=enabled; return TRUE;
}
BOOL SudekiMpLobbyGameplaySavedAvailable(void) { return saved_enabled; }
BOOL SudekiMpLobbyGameplayEnableTestroom(BOOL enabled) {
    if (!base || active) { SetLastError(ERROR_BUSY); return FALSE; }
    testroom_enabled=enabled; return TRUE;
}
BOOL SudekiMpLobbyGameplayTestroomAvailable(void) { return testroom_enabled; }
BOOL SudekiMpLobbyGameplaySavedGame(void) { return active && saved_game; }
void SudekiMpLobbyGameplayService(SudekiMpLobby *lobby) {
    if (!active || !thread_exact()) return;
    if (saved_game) SudekiMpLanStoryRuntimeLobbyService(lobby);
    else SudekiMpLanPartyRuntimeLobbyService(lobby);
}
unsigned SudekiMpLobbyGameplayPoll(unsigned *port) {
    if (active && saved_game && requested && thread_exact()) {
        if (SudekiMpLanStoryLoadPoll(NULL)==SUDEKIMP_STORY_LOAD_FAILED) failed=TRUE;
        SudekiMpLobbyGameplayStoryLoaded();
    }
    if(port) *port=active?(saved_game?SudekiMpLanStoryRuntimePort():SudekiMpLanPartyRuntimePort()):0;
    return !active?SUDEKIMP_LAUNCH_NEW:failed?SUDEKIMP_LAUNCH_FAILED:
        InterlockedCompareExchange(&loaded,0,0)?SUDEKIMP_LAUNCH_LOADED:SUDEKIMP_LAUNCH_PREPARED;
}
BOOL SudekiMpLobbyGameplayMatches(uint32_t r,uint64_t g) { return active && plan.revision==r && plan.generation==g; }
BOOL SudekiMpLobbyGameplayActive(void) { return InterlockedCompareExchange(&active,0,0)!=0; }
BOOL SudekiMpLobbyGameplayStarted(void) { return active && requested; }
BOOL SudekiMpLobbyGameplayRunning(void) { return !SudekiMpLobbyGameplayActive() || InterlockedCompareExchange(&running,0,0); }
void SudekiMpLobbyGameplayLoaded(void) {
    if (room_ready(plan.character[plan.seat]) && !InterlockedExchange(&loaded,1))
        SudekiMpLogFormat("lobby_gameplay event=loaded seat=%u process=%lu\r\n",plan.seat,(unsigned long)GetCurrentProcessId());
}
void SudekiMpLobbyGameplayStoryLoaded(void) {
    if (active && saved_game && dispatched && !failed && thread_exact() &&
        SudekiMpLanStoryLoadPoll(NULL)==SUDEKIMP_STORY_LOAD_RETURNED &&
        SudekiMpLanStoryRuntimeReady() && !InterlockedExchange(&loaded,1))
        SudekiMpLogFormat("lobby_gameplay event=loaded seat=%u process=%lu destination=saved_game native_and_replica_ready=1\r\n",
            plan.seat,(unsigned long)GetCurrentProcessId());
}
void SudekiMpLobbyGameplayRequestStart(void) { if(active && !failed && thread_exact()) requested=TRUE; }
BOOL SudekiMpLobbyGameplayNeedsTitle(void) { return active && requested && !dispatched && !title_owner; }
BOOL SudekiMpLobbyGameplayArmTitle(void *owner) {
    if (!SudekiMpLobbyGameplayNeedsTitle() || !thread_exact() || !readable(owner,0x1844) ||
        *(void **)owner!=base+0x2cb1fc || *(unsigned *)((uint8_t *)owner+0x44)!=5) return FALSE;
    if (saved_game && !SudekiMpLanStoryLoadArmTitle(owner)) return FALSE;
    title_owner=owner;
    if (saved_game) { dispatched=TRUE; InterlockedExchange(&running,1); }
    return TRUE;
}
unsigned SudekiMpLobbyGameplayStoryExitStatus(void) {
    return thread_exact()?story_exit_status:0u;
}
BOOL SudekiMpLobbyGameplayStoryExitDrained(void) {
    return thread_exact() && story_exit_status==1u && !runtime_attempted &&
        (!active || (saved_game && dispatched));
}
unsigned SudekiMpLobbyGameplayStoryExit(void) {
    uint8_t *scene,*resident,*world;
    if(!thread_exact()) { SetLastError(ERROR_INVALID_THREAD_ID); return 0; }
    /* A returned native quit has already cleared actors and world records.
     * Never re-read those former owners or repeat the native operation. */
    if(story_exit_status) return story_exit_status;
    if(!active || !saved_game || !dispatched || !quit_entry_exact() ||
        !title_identity(&scene,&resident) ||
        !readable(world=*(uint8_t **)(base+0x408d10),0x3a0)) {
        SetLastError(ERROR_INVALID_STATE); return 0;
    }
    SudekiMpStoryLoadState load=SudekiMpLanStoryLoadPoll(NULL);
    if(load==SUDEKIMP_STORY_LOAD_WAITING_PAGE || load==SUDEKIMP_STORY_LOAD_FADING ||
        load==SUDEKIMP_STORY_LOAD_LOADING) { SetLastError(ERROR_BUSY); return 0; }
    /* Native A2740 -> A2620 synchronously cleans the current world through
     * FEBF0 -> 6A10 before beginning the asynchronous frontend entrance.
     * Publish ENTERED before that call: its void return cannot be retried. */
    story_exit_status=2;
    SudekiMpLogWrite("lobby_gameplay event=story_native_exit state=entered window=same\r\n");
    ((void (__cdecl *)(void))(base+0xa2740))();
    uint8_t *after_scene,*after_resident;
    if(title_identity(&after_scene,&after_resident) && after_scene==scene && after_resident==resident &&
        *(void **)(base+0x408d10)==world && readable(world,0x3a0) &&
        *(unsigned *)((uint8_t *)title_owner+0x44)==1u &&
        *(unsigned *)(resident+0xbc)==0xbu &&
        !*(void **)(world+0x0c) && !*(void **)(world+0x10) &&
        !*(void **)(world+0x14) && !*(void **)(world+0x18) &&
        !*(uint32_t *)(world+0x394) && world[0x399]==1u && world[0x39a]==1u && world[0x39f]==1u) {
        story_exit_status=1;
        SudekiMpLogWrite("lobby_gameplay event=story_native_exit state=verified_return world_reset=1 frontend_entry=1 visible_intro_ready=unknown\r\n");
        SetLastError(ERROR_SUCCESS);
    } else {
        SudekiMpLogWrite("lobby_gameplay event=story_native_exit state=returned_unknown retry_native_quit=forbidden\r\n");
        retain_story_exit();
    }
    return story_exit_status;
}
BOOL SudekiMpLobbyGameplayCancel(void) {
    if (!active) return TRUE;
    if (!thread_exact()) return FALSE;
    /* Never destroy a world/script while native loading is outstanding. */
    if (saved_game) {
        SudekiMpStoryLoadState state=SudekiMpLanStoryLoadPoll(NULL);
        if(state==SUDEKIMP_STORY_LOAD_WAITING_PAGE || state==SUDEKIMP_STORY_LOAD_FADING ||
            state==SUDEKIMP_STORY_LOAD_LOADING) { SetLastError(ERROR_BUSY); return FALSE; }
    } else if ((title_owner && !dispatched) || (dispatched && !room_ready(plan.character[plan.seat]))) return FALSE;
    if (dispatched && !(saved_game && story_exit_status) && !title_identity(NULL,NULL)) return FALSE;
    if(saved_game && dispatched) {
        /* Runtime balances its client pause and enters the native quit in one
         * terminal UI transaction, then retires the remaining guards. */
        if(story_exit_status==2u) return retain_story_exit();
        if(!SudekiMpLanStoryRuntimeExitToTitle() || story_exit_status!=1u) return FALSE;
    } else if(runtime_attempted &&
        !(saved_game?SudekiMpUninstallLanStoryRuntime():SudekiMpUninstallLanPartyRuntime())) {
        return FALSE;
    }
    runtime_attempted=FALSE;
    /* The client render drain still proves its original load attempt while
     * restoring camera/paused world ownership. Retire that evidence only
     * after runtime teardown has positively completed. */
    if(saved_game && !SudekiMpLanStoryLoadCancel()) return FALSE;
    InterlockedExchange(&running,0); InterlockedExchange(&active,0);
    if (dispatched && !saved_game) {
        /* Native exported QuitToFrontEnd owns ordinary world/UI cleanup and
         * re-enters the animated title sequence at state 1 in this window.
         * Returning from the request is not proof the intro has finished. */
        void (__cdecl *quit)(void)=(void *)(base+0xa2740);
        SudekiMpLogWrite("lobby_gameplay event=return_to_intro state=requested runtime_cleanup=complete window=same\r\n");
        quit();
    }
    requested=dispatched=failed=saved_game=FALSE; title_owner=room_world=room_descriptor=NULL;
    SudekiMpLogWrite("lobby_gameplay event=cancelled runtime_cleanup=complete\r\n");
    return TRUE;
}
