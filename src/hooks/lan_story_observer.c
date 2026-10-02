#include "hooks/lan_story_observer.h"
#include "hooks/call_hook.h"
#include "cleanroom/engine.h"
#include "engine/log.h"
#include <string.h>

/* Entry bytes/calling conventions match the existing exact-build zone trace.
 * This adapter deliberately installs no placement/formation/camera hooks. */
enum { SET=0,ENTER,SWITCH,LOAD,MAIN,TEMP,EXIT,HOOK_COUNT };
static const unsigned rvas[HOOK_COUNT]={0x7910u,0x7970u,0x7990u,0x7b80u,
    0x6380u,0x64b0u,0x6710u};
static const uint8_t entries[HOOK_COUNT][12]={
    {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x51},
    {0x8b,0x44,0x24,0x04,0x56},
    {0x8b,0x44,0x24,0x04,0x56},
    {0x8b,0x44,0x24,0x04,0x8b,0x0d,0x10,0x8d,0x80,0x00},
    {0x8b,0x44,0x24,0x04,0x56,0x8b,0xf1},
    {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x24},
    {0x55,0x8b,0xec,0x83,0xe4,0xf0,0x81,0xec,0x84,0,0,0}
};
static const size_t lengths[HOOK_COUNT]={7,5,5,10,7,9,12};
typedef void (__cdecl *ZoneCall)(const char *);
typedef void (__attribute__((thiscall)) *MainCall)(void *,const char *);
typedef void (__attribute__((thiscall)) *TempCall)(void *,const char *,const void *);
typedef void (__attribute__((thiscall)) *ExitCall)(void *);
static uint8_t *base;
static SudekiMpInlineHook hooks[HOOK_COUNT];
static SRWLOCK state_lock=SRWLOCK_INIT;
static DWORD native_thread;
static unsigned call_depth;
static BOOL foreign_thread,exhausted,installed;
static SudekiMpLanStoryScene observed,published;
static void *last_world,*last_descriptor,*last_group,*last_controller,*last_actors[4];
static BOOL last_exact;
static unsigned last_logged_revision;

static BOOL readable(const void *p,size_t size) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t a=(uintptr_t)p;
    return p && size && a+size>=a && VirtualQuery(p,&m,sizeof(m)) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+size<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL name_copy(char out[64],const char *name) {
    char copy[64]={0};
    if(!name) return FALSE;
    for(unsigned i=0;i<64u;++i) {
        if(!readable(name+i,1)) return FALSE;
        unsigned char c=(unsigned char)name[i];
        if(!c) { if(!i) return FALSE; memcpy(out,copy,64); return TRUE; }
        if(c>='A' && c<='Z') c=(unsigned char)(c-'A'+'a');
        if(!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='-'))
            return FALSE;
        copy[i]=(char)c;
    }
    return FALSE;
}
static void increment(uint32_t *value) {
    if(*value==UINT32_MAX) exhausted=TRUE;
    else ++*value;
}
static void unknown_scene(unsigned phase) {
    observed.phase=(uint8_t)phase;
    observed.available_mask=0;
    observed.leader_seat=SUDEKIMP_LAN_STORY_NO_SEAT;
}
static void begin_zone(unsigned kind,const char *name) {
    AcquireSRWLockExclusive(&state_lock);
    DWORD thread=GetCurrentThreadId();
    if(native_thread && native_thread!=thread) foreign_thread=TRUE;
    else native_thread=thread;
    ++call_depth;
    increment(&observed.epoch);
    unknown_scene(SUDEKIMP_LAN_STORY_LOADING);
    if(kind==SET || kind==ENTER || kind==SWITCH || kind==MAIN) {
        memset(observed.world,0,sizeof(observed.world));
        memset(observed.temporary,0,sizeof(observed.temporary));
        if(!name_copy(observed.world,name)) unknown_scene(SUDEKIMP_LAN_STORY_UNKNOWN);
    } else if(kind==TEMP) {
        memset(observed.temporary,0,sizeof(observed.temporary));
        if(!name_copy(observed.temporary,name)) unknown_scene(SUDEKIMP_LAN_STORY_UNKNOWN);
    } else if(kind==EXIT) {
        /* SetZoneNow also calls this as cleanup. Clearing the temporary name
         * is harmless there; never claim the exterior is ready at this edge. */
        memset(observed.temporary,0,sizeof(observed.temporary));
    }
    SudekiMpLogFormat("lan_story event=zone_begin method=%u epoch=%lu world=%s temporary=%s depth=%u thread=%lu foreign_thread=%u policy=observation_only\r\n",
        kind,(unsigned long)observed.epoch,observed.world,observed.temporary,call_depth,
        (unsigned long)thread,foreign_thread);
    ReleaseSRWLockExclusive(&state_lock);
}
static void end_zone(void) {
    AcquireSRWLockExclusive(&state_lock);
    if(call_depth) --call_depth;
    ReleaseSRWLockExclusive(&state_lock);
}
static void __cdecl zone_set(const char *s) { begin_zone(SET,s); ((ZoneCall)hooks[SET].trampoline)(s); end_zone(); }
static void __cdecl zone_enter(const char *s) { begin_zone(ENTER,s); ((ZoneCall)hooks[ENTER].trampoline)(s); end_zone(); }
static void __cdecl zone_switch(const char *s) { begin_zone(SWITCH,s); ((ZoneCall)hooks[SWITCH].trampoline)(s); end_zone(); }
static void __cdecl zone_load(const char *s) { begin_zone(LOAD,s); ((ZoneCall)hooks[LOAD].trampoline)(s); end_zone(); }
static void __attribute__((thiscall)) zone_main(void *w,const char *s) { begin_zone(MAIN,s); ((MainCall)hooks[MAIN].trampoline)(w,s); end_zone(); }
static void __attribute__((thiscall)) zone_temp(void *w,const char *s,const void *r) { begin_zone(TEMP,s); ((TempCall)hooks[TEMP].trampoline)(w,s,r); end_zone(); }
static void __attribute__((thiscall)) zone_exit(void *w) { begin_zone(EXIT,NULL); ((ExitCall)hooks[EXIT].trampoline)(w); end_zone(); }

BOOL SudekiMpLanStoryObserverInstall(HMODULE module) {
    const void *replacements[HOOK_COUNT]={zone_set,zone_enter,zone_switch,zone_load,zone_main,zone_temp,zone_exit};
    if(!module || base) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    uint8_t expected[HOOK_COUNT][12];
    memcpy(expected,entries,sizeof(expected));
    /* LoadZone's absolute world-global operand is relocated by the loader. */
    uint32_t world_global=(uint32_t)(uintptr_t)((uint8_t *)module+0x408d10u);
    memcpy(expected[LOAD]+6,&world_global,4);
    for(unsigned i=0;i<HOOK_COUNT;++i)
        if(!readable((uint8_t *)module+rvas[i],lengths[i]) ||
            memcmp((uint8_t *)module+rvas[i],expected[i],lengths[i])) {
            SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
    base=(uint8_t *)module; observed.epoch=1; observed.revision=0;
    unknown_scene(SUDEKIMP_LAN_STORY_UNKNOWN);
    for(unsigned i=0;i<HOOK_COUNT;++i)
        if(!SudekiMpInstallInlineHook(&hooks[i],base+rvas[i],expected[i],lengths[i],replacements[i])) {
            DWORD error=GetLastError();
            (void)SudekiMpLanStoryObserverUninstall();
            SetLastError(error); return FALSE;
        }
    installed=TRUE;
    return TRUE;
}

BOOL SudekiMpLanStoryObserverSample(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,SudekiMpLanStoryScene *out) {
    static const SudekiMpCleanroomActor actors[4]={SUDEKIMP_CLEANROOM_BUKI,
        SUDEKIMP_CLEANROOM_ELCO,SUDEKIMP_CLEANROOM_TAL,SUDEKIMP_CLEANROOM_AILISH};
    if(!out || !base || !installed || !w || !w->service_post_original_exact ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) return FALSE;
    AcquireSRWLockExclusive(&state_lock);
    DWORD thread=GetCurrentThreadId();
    if(native_thread && native_thread!=thread) foreign_thread=TRUE;
    else native_thread=thread;
    if(exhausted || foreign_thread || call_depth) {
        ReleaseSRWLockExclusive(&state_lock); return FALSE;
    }
    uint8_t *world=*(uint8_t **)(base+0x408d10u);
    uint8_t *group=*(uint8_t **)(base+0x408d94u);
    uint8_t *descriptor=NULL;
    void *found[4]={0};
    uint8_t mask=0,lead=SUDEKIMP_LAN_STORY_NO_SEAT;
    BOOL exact=FALSE;
    unsigned count=0;
    if(readable(world,0x39bu)) descriptor=*(uint8_t **)(world+0x0cu);
    if(readable(world,0x39bu) && readable(descriptor,0x38u) &&
        !*(void **)(world+0x14u) && world[0x399u] && world[0x39au] &&
        *(uint32_t *)(descriptor+0x34u)==(observed.temporary[0]?4u:3u) &&
        observed.world[0] && readable(group,0xd0u) && readable(controller,0x24cu) &&
        controller==*(void **)(base+0x408da4u) &&
        SudekiMpCleanroomEngineWorldReady()) {
        count=*(unsigned *)(group+0xccu);
        if(count && count<=4u) {
            void *heroes[4];
            for(unsigned i=0;i<4u;++i) heroes[i]=SudekiMpCleanroomEngineActorEntity(actors[i]);
            exact=TRUE;
            for(unsigned slot=0;slot<count;++slot) {
                uint8_t *actor=*(uint8_t **)(group+0x90u+slot*0x0cu);
                unsigned seat=0;
                for(;seat<4u && (!actor || actor!=heroes[seat]);++seat) {}
                if(seat==4u || (mask&(1u<<seat)) || !readable(actor,0x98u)) { exact=FALSE; break; }
                uint8_t *ai=*(uint8_t **)(actor+0x94u);
                if(!readable(ai,0x16cu) || *(void **)(ai+0x10u)!=actor) { exact=FALSE; break; }
                found[seat]=actor; mask|=(uint8_t)(1u<<seat);
                if(!slot) lead=(uint8_t)seat;
            }
            exact=exact && lead<4u && *(void **)((uint8_t *)controller+0x248u)==found[lead] &&
                count==*(unsigned *)(group+0xccu) && world==*(void **)(base+0x408d10u) &&
                descriptor==*(void **)(world+0x0cu) && group==*(void **)(base+0x408d94u) &&
                controller==*(void **)(base+0x408da4u) &&
                SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w);
        }
    }
    BOOL replaced=last_exact && (!exact || world!=last_world || descriptor!=last_descriptor ||
        group!=last_group || controller!=last_controller || memcmp(found,last_actors,sizeof(found)));
    if(replaced) increment(&observed.epoch);
    if(exact) {
        observed.phase=SUDEKIMP_LAN_STORY_READY;
        observed.available_mask=mask; observed.leader_seat=lead;
    } else unknown_scene(observed.world[0]?SUDEKIMP_LAN_STORY_LOADING:SUDEKIMP_LAN_STORY_UNKNOWN);
    if(!published.revision || !SudekiMpLanStorySceneSame(&published,&observed))
        increment(&observed.revision);
    observed.observed_tick=GetTickCount();
    if(exhausted || !SudekiMpLanStorySceneValid(&observed)) {
        ReleaseSRWLockExclusive(&state_lock); return FALSE;
    }
    last_exact=exact; last_world=world; last_descriptor=descriptor;
    last_group=group; last_controller=controller; memcpy(last_actors,found,sizeof(found));
    published=observed; *out=observed;
    if(last_logged_revision!=observed.revision) {
        last_logged_revision=observed.revision;
        SudekiMpLogFormat("lan_story event=native_scene epoch=%lu revision=%lu phase=%u available=%u lead=%u world=%s temporary=%s group=%p controller=%p count=%u\r\n",
            (unsigned long)observed.epoch,(unsigned long)observed.revision,observed.phase,mask,lead,
            observed.world,observed.temporary,group,controller,count);
        if(exact) for(unsigned seat=0;seat<4u;++seat) if(found[seat]) {
            uint8_t *ai=*(uint8_t **)((uint8_t *)found[seat]+0x94u);
            uint8_t *mode=*(uint8_t **)(ai+0x3cu);
            SudekiMpLogFormat("lan_story event=native_member seat=%u actor=%p ai=%p override=%d mode=%p mode_flag=%d controller_target=%d\r\n",
                seat,found[seat],ai,*(int16_t *)(ai+0x16au),mode,
                readable(mode,0x0cu)?mode[0x0bu]:-1,seat==lead);
        }
    }
    ReleaseSRWLockExclusive(&state_lock); return TRUE;
}

BOOL SudekiMpLanStoryObserverUninstall(void) {
    if(!base) return TRUE;
    AcquireSRWLockExclusive(&state_lock);
    if(call_depth || foreign_thread || (native_thread && native_thread!=GetCurrentThreadId())) {
        ReleaseSRWLockExclusive(&state_lock); SetLastError(ERROR_BUSY); return FALSE;
    }
    BOOL ok=TRUE; DWORD error=ERROR_SUCCESS;
    for(unsigned n=HOOK_COUNT;n>0;--n) if(!SudekiMpRestoreInlineHook(&hooks[n-1])) {
        if(ok) error=GetLastError();
        ok=FALSE;
    }
    if(ok) {
        base=NULL; installed=FALSE; native_thread=0; exhausted=FALSE;
        memset(&observed,0,sizeof(observed));
        memset(&published,0,sizeof(published));
        last_world=last_descriptor=last_group=last_controller=NULL;
        memset(last_actors,0,sizeof(last_actors)); last_exact=FALSE; last_logged_revision=0;
    }
    ReleaseSRWLockExclusive(&state_lock);
    if(!ok) SetLastError(error);
    return ok;
}
