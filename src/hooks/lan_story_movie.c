#include "hooks/lan_story_movie.h"
#include "hooks/lan_arena_startup_movie_skip.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <string.h>

enum { LOOP_CALL=0x104e1a, MOVIE_TICK=0x1bf1d0, MOVIE_STOP=0x1bf450,
    MOVIE_SLOT=0x3c30ec, WORKER_A=0x324004, WORKER_B=0x324008 };
typedef unsigned char (__attribute__((cdecl,regparm(0))) *NativeMovieStep)(void);
static uint8_t *base;
static SudekiMpRelativeCallHook loop_hook;
static SudekiMpLanPartySession *movie_session;
static SudekiMpStoryMovie current_movie;
static SudekiMpLanPartyLease current_connection,played_connection;
static uint32_t played_serial;
static DWORD native_thread;
static volatile LONG closing,active_calls;
static BOOL installed,observer_attached,client_playing;
static void *native_handle;
static unsigned movie_logs,deferred_mask;
static uint32_t deferred_serial;
static BOOL readable(const void *pointer,size_t n) {
    MEMORY_BASIC_INFORMATION m;uintptr_t p=(uintptr_t)pointer;
    return pointer && n && p<=UINTPTR_MAX-n && VirtualQuery(pointer,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        p+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL target(const uint8_t *p,const void *to) {
    int32_t d;if(!readable(p,5) || *p!=0xe8)return FALSE;
    memcpy(&d,p+1,4);return p+5+d==to;
}
static BOOL connection_same(const SudekiMpLanPartyLease *a,const SudekiMpLanPartyLease *b) {
    return a->token && a->token==b->token && a->generation==b->generation && a->seat==b->seat;
}
static void trace_event(const char *event,const SudekiMpStoryMovie *movie) {
    if(movie_logs<64) { ++movie_logs;
        SudekiMpLogFormat("story_movie event=%s movie=FMA04A serial=%lu epoch=%lu revision=%lu role=%s elapsed_ms=%lu ms=%lu\r\n",
            event,(unsigned long)movie->serial,(unsigned long)movie->epoch,
            (unsigned long)movie->revision,
            SudekiMpLanPartyLocalSeat(movie_session)?"client":"host",
            (unsigned long)movie->elapsed_ms,(unsigned long)GetTickCount());
    }
}
static void trace(const char *event) { trace_event(event,&current_movie); }
static void trace_deferred(const char *reason,const SudekiMpStoryMovie *event,unsigned bit) {
    if(deferred_serial!=event->serial) { deferred_serial=event->serial;deferred_mask=0; }
    if(!(deferred_mask&bit)) { deferred_mask|=bit;trace_event(reason,event); }
}
static BOOL castle_movie(const char *name) {
    /* Native splits drive/path/extension. Admit exactly this basename, not a
     * substring or a network-supplied path. */
    char leaf[32];size_t len=0;
    if(!name)return FALSE;
    for(size_t i=0;i<260;++i) {
        if(!readable(name+i,1))return FALSE;
        char c=name[i];
        if(!c) { leaf[len]=0;return !strcmp(leaf,"fma04a") || !strcmp(leaf,"fma04a.bik"); }
        if(c=='/' || c=='\\' || c==':') {len=0;continue;}
        if(len+1>=sizeof(leaf))return FALSE;
        leaf[len++]=(c>='A' && c<='Z')?(char)(c-'A'+'a'):c;
    }
    return FALSE;
}
static BOOL host_begin(const char *name) {
    if(!installed || InterlockedCompareExchange(&closing,0,0) || !native_thread ||
        native_thread!=GetCurrentThreadId() || SudekiMpLanPartyLocalSeat(movie_session) ||
        !castle_movie(name) || InterlockedCompareExchange(&active_calls,1,0)) return FALSE;
    if(!SudekiMpLanPartyMovieBegin(movie_session,GetTickCount(),&current_movie)) {
        InterlockedExchange(&active_calls,0);return FALSE;
    }
    trace("host_start");return TRUE;
}
static void host_end(void) {
    SudekiMpLanPartyMovieEnd(movie_session,&current_movie,GetTickCount());
    trace("host_end");InterlockedExchange(&active_calls,0);
}
static BOOL client_end_requested(void) {
    SudekiMpStoryMovie event;SudekiMpLanPartyLease connection;
    return InterlockedCompareExchange(&closing,0,0) ||
        !SudekiMpLanPartyMovieGet(movie_session,GetTickCount(),&connection,&event) ||
        !connection_same(&current_connection,&connection) ||
        !SudekiMpStoryMovieSame(&current_movie,&event) || !event.active ||
        event.elapsed_ms>=SUDEKIMP_STORY_MOVIE_LIMIT_MS;
}
static unsigned char __attribute__((cdecl,regparm(0))) movie_tick(void) {
    /* This callsite is inside MoviePlay, after native message/input pumping.
     * Stop uses the same synchronous close as native EOF and skip cleanup.
     * No worker accesses the codec, renderer, audio or thread handles. */
    if(client_playing && native_thread==GetCurrentThreadId()) {
        void *handle=*(void **)(base+MOVIE_SLOT);
        if(!native_handle)native_handle=handle;
        if(handle && handle==native_handle && client_end_requested()) {
            trace("client_host_end_or_connection_loss");
            return ((NativeMovieStep)(base+MOVIE_STOP))();
        }
    }
    return ((NativeMovieStep)(base+MOVIE_TICK))();
}
BOOL SudekiMpLanStoryMovieImageMatches(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    static const uint8_t skip[]={0x80,0x7d,0x0c,0x00,0x74,0x3e};
    static const uint8_t stop_tail[]={0x5f,0x5e,0xb0,0x01,0x5b,0xc3};
    static const uint8_t loop_tail[]={0x75,0x80};
    static const uint8_t stop_entry[]={0x53,0x33,0xdb,0x56,0x57,0x3b,0xc3};
    return b && SudekiMpCheckLoadedExecutable(image) &&
        readable(b+MOVIE_SLOT,4) && readable(b+WORKER_A,8) &&
        !memcmp(b+0x104dd6,skip,sizeof(skip)) &&
        !memcmp(b+0x104e4f,loop_tail,sizeof(loop_tail)) &&
        !memcmp(b+0x1bf455,stop_entry,sizeof(stop_entry)) &&
        !memcmp(b+0x1bf4d8,stop_tail,sizeof(stop_tail)) &&
        b[MOVIE_STOP]==0xa1 && *(void **)(b+MOVIE_STOP+1)==b+MOVIE_SLOT &&
        target(b+LOOP_CALL,b+MOVIE_TICK) && target(b+0x104cf1,b+MOVIE_STOP) &&
        target(b+0x104e8e,b+0x104cf0) && target(b+0x104d9c,b+0x104c10);
}
static BOOL retain(void) {
    HMODULE self;
    (void)GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCWSTR)(uintptr_t)&SudekiMpLanStoryMovieUninstall,&self);
    SetLastError(ERROR_BUSY);return FALSE;
}
BOOL SudekiMpLanStoryMovieInstall(HMODULE image,SudekiMpLanPartySession *s) {
    if(installed || loop_hook.installed || observer_attached || !s ||
        !SudekiMpLanStoryMovieImageMatches(image)) {SetLastError(ERROR_INVALID_DATA);return FALSE;}
    base=(uint8_t *)image;movie_session=s;native_thread=0;played_serial=0;movie_logs=0;
    deferred_serial=deferred_mask=0;
    memset(&played_connection,0,sizeof(played_connection));
    InterlockedExchange(&closing,0);
    if(!SudekiMpInstallRelativeCallHook(&loop_hook,base+LOOP_CALL,base+MOVIE_TICK,movie_tick))return FALSE;
    if(!SudekiMpLanArenaMovieObserve(image,host_begin,host_end)) {
        (void)SudekiMpLanStoryMovieUninstall();return FALSE;
    }
    observer_attached=TRUE;installed=TRUE;return TRUE;
}
void SudekiMpLanStoryMovieClose(void) {InterlockedExchange(&closing,1);}
BOOL SudekiMpLanStoryMovieUninstall(void) {
    SudekiMpLanStoryMovieClose();
    if(InterlockedCompareExchange(&active_calls,0,0) ||
        (native_thread && native_thread!=GetCurrentThreadId()))return retain();
    if(observer_attached && !SudekiMpLanArenaMovieUnobserve())return retain();
    observer_attached=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&loop_hook))return retain();
    installed=FALSE;movie_session=NULL;native_thread=0;base=NULL;return TRUE;
}
BOOL SudekiMpLanStoryMovieService(BOOL ready,BOOL (*prepare)(void)) {
    if(!installed || InterlockedCompareExchange(&closing,0,0))return FALSE;
    if(!native_thread)native_thread=GetCurrentThreadId();
    if(native_thread!=GetCurrentThreadId() || !SudekiMpLanPartyLocalSeat(movie_session))return FALSE;
    SudekiMpStoryMovie event;SudekiMpLanPartyLease connection;
    if(!SudekiMpLanPartyMovieGet(movie_session,GetTickCount(),&connection,&event))return FALSE;
    if(!connection_same(&played_connection,&connection)) {
        played_connection=connection;played_serial=0;deferred_serial=deferred_mask=0;
    }
    if(!event.active || event.serial==played_serial)return event.active;
    if(event.elapsed_ms>SUDEKIMP_STORY_MOVIE_LATE_MS) {
        played_serial=event.serial;trace_event("client_late_start",&event);
        return TRUE; /* never start a stale clip from its beginning */
    }
    if(!ready) {trace_deferred("client_not_ready",&event,1u);return TRUE;}
    if(!prepare || !prepare()) {trace_deferred("client_prepare_deferred",&event,2u);return TRUE;}
    /* MoviePlay suspends these two native background workers. Never enter
     * from either worker or while another native movie owns the global. */
    HANDLE a=*(HANDLE *)(base+WORKER_A),b=*(HANDLE *)(base+WORKER_B);
    DWORD ta=GetThreadId(a),tb=GetThreadId(b);
    if(!ta || !tb || ta==native_thread || tb==native_thread) {
        trace_deferred("client_worker_refused",&event,4u);return TRUE;
    }
    if(*(void **)(base+MOVIE_SLOT)) {trace_deferred("client_movie_busy",&event,8u);return TRUE;}
    if(!target(base+LOOP_CALL,movie_tick)) {trace_deferred("client_hook_changed",&event,16u);return TRUE;}
    if(InterlockedCompareExchange(&active_calls,1,0)) {
        trace_deferred("client_call_busy",&event,32u);return TRUE;
    }
    current_movie=event;current_connection=connection;played_serial=event.serial;
    client_playing=TRUE;native_handle=NULL;trace("client_start");
    BOOL result=SudekiMpLanArenaMoviePlayInstalled("FMA04A");
    trace(!result?"client_play_refused":native_handle?"client_return":"client_asset_unavailable");
    client_playing=FALSE;native_handle=NULL;InterlockedExchange(&active_calls,0);
    return TRUE; /* discard this pre-movie dispatch; resume from fresh observations */
}
