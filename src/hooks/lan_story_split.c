#include "hooks/lan_story_split.h"
#include "hooks/lan_story_observer.h"
#include "hooks/lan_story_temp_exterior.h"
#include "hooks/lan_story_world.h"
#include "hooks/call_hook.h"
#include "cleanroom/engine.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "engine/cast_light_abi.h"
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Story split areas require the supported x86 ABI"
#endif
enum { WORLD=0x408d10, GROUP=0x408d94, LEAD_ONLY=0x24720, FULL_PARTY=0x24850,
    NODE_PAUSE=0x1060c0, NODE_RESUME=0x106100, MAX_EXEMPT=3,
    /* Main world scene animation walk: CALL 0x5D4820 at 0x40A662 (EAX=scene,
     * dt pushed; the caller forces dt=0 while the game pause count is set).
     * 0x5D64D0 is the native per-object update (thiscall, ret 4): hidden
     * skip, frame-stamp dedupe (object+0x32 vs 0x7C3150), parent chain. */
    SCENE_CALL=0xa662, SCENE_WALK=0x1d4820, OBJECT_UPDATE=0x1d64d0, FRAME_STAMP=0x3c3150,
    RENDERER_VTABLE=0x2df8ec };
typedef void (__attribute__((thiscall)) *GroupCall)(void *);
static uint8_t *base;
static SudekiMpInlineHook lead_hook,full_hook;
static SudekiMpRelativeCallHook scene_hook;
void *SudekiMpLanStorySplitSceneWalk __attribute__((used));
void *SudekiMpLanStorySplitObjectUpdate __attribute__((used));
static uint32_t animate_driven,animate_already,animate_invalid,animate_hidden; static DWORD animate_logged; static unsigned animate_logs;
void *SudekiMpLanStorySplitNodePause __attribute__((used));
void *SudekiMpLanStorySplitNodeResume __attribute__((used));
static DWORD native_thread;
static BOOL host_ready,active,settled_inside,attached,observer_set;
static uint8_t outside_mask,leader;
static char temporary[48];
static const void *temporary_data;
static uint8_t *exempt_group,*exempt_actor[MAX_EXEMPT],exempt_added[MAX_EXEMPT];
static unsigned exempt_count,traces;
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_READWRITE || access==PAGE_EXECUTE_READWRITE ||
        access==PAGE_READONLY || access==PAGE_EXECUTE_READ;
}
static BOOL thread_exact(void) {
    DWORD t=GetCurrentThreadId();
    if(!native_thread) native_thread=t;
    return native_thread==t;
}
static unsigned character_of(const void *actor) {
    static const SudekiMpCleanroomActor types[4]={SUDEKIMP_CLEANROOM_BUKI,
        SUDEKIMP_CLEANROOM_ELCO,SUDEKIMP_CLEANROOM_TAL,SUDEKIMP_CLEANROOM_AILISH};
    for(unsigned c=0;c<4u;++c) if(actor && SudekiMpCleanroomEngineActorEntity(types[c])==actor) return c;
    return 4u;
}
/* Native single-node pause (EAX=node) and resume (ESI=node), node=entity+8. */
static void __attribute__((naked,noinline)) node_pause(void *node __attribute__((unused))) {
    __asm__ volatile("pushal; mov 36(%esp),%eax; call *_SudekiMpLanStorySplitNodePause; popal; ret");
}
static void __attribute__((naked,noinline)) node_resume(void *node __attribute__((unused))) {
    __asm__ volatile("pushal; mov 36(%esp),%esi; call *_SudekiMpLanStorySplitNodeResume; popal; ret");
}
static BOOL group_member(const uint8_t *g,const uint8_t *actor) {
    if(!readable(g,0xd4)) return FALSE;
    unsigned count=*(const unsigned *)(g+0xcc);
    if(!count || count>4u) return FALSE;
    for(unsigned slot=1;slot<count;++slot)
        if(*(uint8_t *const *)(g+0x90+slot*0xc)==actor) return TRUE;
    return FALSE;
}
static void __attribute__((thiscall)) lead_only(void *group) {
    uint8_t *g=group;
    int before=readable(g,0xd4)?*(int *)(g+0xd0):-1;
    /* Native lead-only (depth 0->1) adds one pause reference per follower in
     * the party-slot loop and a second one in the global character-list loop
     * for type-2 followers (live: refs 0->2). Sample each outside actor's
     * counter before the native call so exactly the added references are
     * removed, no more: a reference the actor already held stays. */
    uint8_t *actors[4]={0}; uint8_t refs_before[4]={0};
    BOOL eligible=before==0 && readable(g,0xd4) && thread_exact() && host_ready && outside_mask &&
        !exempt_count && g==*(uint8_t **)(base+GROUP);
    if(eligible) {
        unsigned count=*(unsigned *)(g+0xcc);
        for(unsigned slot=1;slot<count && slot<4u;++slot) {
            uint8_t *actor=*(uint8_t **)(g+0x90+slot*0xc);
            unsigned c=character_of(actor);
            if(c>=4u || !(outside_mask&(1u<<c)) || !readable(actor,0x30)) continue;
            actors[slot]=actor; refs_before[slot]=actor[0x2b];
        }
    }
    ((GroupCall)lead_hook.trampoline)(group);
    if(!eligible || !readable(g,0xd4) || *(int *)(g+0xd0)!=1) return;
    for(unsigned slot=1;slot<4u && exempt_count<MAX_EXEMPT;++slot) {
        uint8_t *actor=actors[slot];
        if(!actor || !readable(actor,0x30) || actor[0x2b]<=refs_before[slot]) continue;
        unsigned added=actor[0x2b]-refs_before[slot];
        for(unsigned k=0;k<added;++k) node_resume(actor+8);
        exempt_actor[exempt_count]=actor; exempt_added[exempt_count]=(uint8_t)added; ++exempt_count;
        SudekiMpLogFormat("lan_story_split event=lead_only_exempt character=%u refs_before=%u added=%u refs_after=%u\r\n",
            character_of(actor),refs_before[slot],added,actor[0x2b]);
    }
    if(exempt_count) exempt_group=g;
}
static void __attribute__((thiscall)) full_party(void *group) {
    uint8_t *g=group;
    if(exempt_count && thread_exact() && readable(g,0xd4) && *(int *)(g+0xd0)==1) {
        if(g==exempt_group) {
            for(unsigned i=0;i<exempt_count;++i) {
                uint8_t *actor=exempt_actor[i]; unsigned added=exempt_added[i];
                if(group_member(g,actor) && readable(actor,0x30) && actor[0x2b]+added<250u) {
                    /* Restore exactly the references removed at lead-only so
                     * native full-party releases a balanced counter. */
                    for(unsigned k=0;k<added;++k) node_pause(actor+8);
                    SudekiMpLogFormat("lan_story_split event=lead_only_restore character=%u restored=%u refs_before_release=%u\r\n",
                        character_of(actor),added,actor[0x2b]);
                } else SudekiMpLogFormat("lan_story_split event=lead_only_restore_skipped reason=member_changed\r\n");
            }
        } else SudekiMpLogFormat("lan_story_split event=lead_only_restore_skipped reason=group_changed\r\n");
        exempt_count=0; exempt_group=NULL;
    }
    ((GroupCall)full_hook.trampoline)(group);
}
/* Native per-object animation update (ECX=object, stack dt, ret 4). */
static void __attribute__((naked,noinline)) object_update(void *object __attribute__((unused)),uint32_t dt_bits __attribute__((unused))) {
    __asm__ volatile("push %ebp; mov %esp,%ebp; pushal; mov 12(%ebp),%eax; push %eax; mov 8(%ebp),%ecx;"
        "call *_SudekiMpLanStorySplitObjectUpdate; popal; pop %ebp; ret");
}
/* After the native main-scene walk: outside actors live in lists the walk no
 * longer visits (the exterior's sectors are not active for the TEMP camera)
 * or are flagged hidden by lead-only, so their animation clock and the root
 * motion that moves them stop. Run the native per-object update for the host
 * capture catalog with the same dt. The frame stamp keeps natively visited
 * objects from updating twice; bit 0x20 ("update while hidden") is raised only
 * for the duration of the call. */
static void __attribute__((used,noinline)) scene_updated(uint32_t dt_bits) {
    if(!active || !base || !thread_exact()) return;
    SudekiMpLanStoryWorldAnimateTarget t[SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS];
    unsigned n=SudekiMpLanStoryWorldSplitAnimateTargets(t,SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS);
    uint16_t stamp=*(const uint16_t *)(base+FRAME_STAMP);
    for(unsigned i=0;i<n;++i) {
        uint8_t *o=t[i].object;
        if(t[i].character<4u && !(outside_mask&(1u<<t[i].character))) continue;
        if(!readable(o,0xd0) || *(void **)(o+0x14)!=t[i].renderer || !readable(t[i].renderer,4) ||
            *(void **)t[i].renderer!=base+RENDERER_VTABLE) { ++animate_invalid; continue; }
        if(*(uint16_t *)(o+0x32)==stamp) { ++animate_already; continue; }
        uint32_t flags=*(uint32_t *)(o+0x34); BOOL raise=(flags&4u) && !(flags&0x20u);
        if(raise) { *(uint32_t *)(o+0x34)=flags|0x20u; ++animate_hidden; }
        object_update(o,dt_bits);
        if(raise) *(uint32_t *)(o+0x34)&=~0x20u;
        ++animate_driven;
    }
    DWORD now=GetTickCount();
    if(SudekiMpLogResearchEnabled() && now-animate_logged>=2000u && animate_logs<300u) {
        animate_logged=now; ++animate_logs;
        SudekiMpLogFormat("lan_story_split event=animate targets=%u driven=%lu already=%lu hidden=%lu invalid=%lu\r\n",
            n,(unsigned long)animate_driven,(unsigned long)animate_already,(unsigned long)animate_hidden,(unsigned long)animate_invalid);
        animate_driven=animate_already=animate_hidden=animate_invalid=0;
    }
}
/* Replaces the relative call: runs the native walk unchanged (EAX=scene, dt
 * consumed by its ret 4), then the split pass, then returns as the callee. */
static void __attribute__((naked,noinline)) scene_entry(void) {
    __asm__ volatile("push %ebp; mov %esp,%ebp; pushal;"
        "mov 8(%ebp),%eax; push %eax; mov -4(%ebp),%eax; call *_SudekiMpLanStorySplitSceneWalk;"
        "mov 8(%ebp),%eax; push %eax; call _scene_updated; add $4,%esp;"
        "popal; pop %ebp; ret $4");
}
static BOOL same_name(const char *a,const char *b) {
    for(;;++a,++b) {
        char x=*a,y=*b;
        if(x>='A'&&x<='Z') x=(char)(x-'A'+'a');
        if(y>='A'&&y<='Z') y=(char)(y-'A'+'a');
        if(x!=y) return FALSE;
        if(!x) return TRUE;
    }
}
static BOOL begin_temp(const char *name,unsigned lead) {
    if(active || !attached || !host_ready || lead>=4u || !(outside_mask&~(1u<<lead)) ||
        !thread_exact()) return FALSE;
    size_t n=0; while(n<sizeof(temporary)-1u && readable(name+n,1) && name[n]) ++n;
    if(!n || name[n]) return FALSE;
    memcpy(temporary,name,n); temporary[n]=0;
    active=TRUE; settled_inside=FALSE; leader=(uint8_t)lead; temporary_data=NULL;
    SudekiMpLogFormat("lan_story_split event=begin temporary=%s leader=%u outside=%u\r\n",temporary,lead,outside_mask);
    return TRUE;
}
static BOOL begin_exit(void) {
    if(!active) return FALSE;
    SudekiMpLogFormat("lan_story_split event=exit_begin temporary=%s\r\n",temporary);
    return TRUE;
}
static void settled(BOOL inside) {
    settled_inside=inside; temporary_data=NULL;
    if(inside) {
        const uint8_t *w=*(uint8_t **)(base+WORLD),*d=w&&readable(w,0x10)?*(const uint8_t *const *)(w+0xc):NULL;
        if(d && readable(d,0x38) && *(const uint32_t *)(d+0x34)==4u) temporary_data=*(const void *const *)(d+0x14);
    }
    SudekiMpLogFormat("lan_story_split event=settled inside=%u temporary_data=%p world_light_adoptions=%u foreign_light_skips=%u light_ready_failure=%u light_fault_site=%u light_fault_line=%u\r\n",inside,temporary_data,SudekiMpCastLightWorldAdoptions(),SudekiMpCastLightForeignSkips(),SudekiMpCastLightReadyFailure(),SudekiMpCastLightFaultSite(),SudekiMpCastLightFaultLine());
}
static void ended(BOOL vanilla) {
    SudekiMpLogFormat("lan_story_split event=ended vanilla=%u temporary=%s\r\n",vanilla,temporary);
    active=settled_inside=FALSE; temporary_data=NULL; memset(temporary,0,sizeof(temporary));
}
static const SudekiMpLanStoryObserverSplit observer_split={begin_temp,begin_exit,settled,ended};
static BOOL keep_exterior(void *context,const SudekiMpLanStoryTempExteriorEntry *e) {
    (void)context;
    BOOL keep=active && !settled_inside && same_name(e->destination,temporary);
    SudekiMpLogFormat("lan_story_split event=exterior_entry ticket=%llu exterior=%s destination=%s keep_live=%u\r\n",
        (unsigned long long)e->ticket,e->exterior,e->destination,keep);
    return keep;
}
static void exit_skipped(void *context,const SudekiMpLanStoryTempExteriorReceipt *r) {
    (void)context;
    SudekiMpLogFormat("lan_story_split event=exterior_exit ticket=%llu result=%s exterior=%s elapsed_ms=%lu tracked=%lu moved=%lu raised=%lu lowered=%lu terrain_enabled=%u,%u resume=skipped\r\n",
        (unsigned long long)r->ticket,r->result==SUDEKIMP_STORY_TEMP_EXTERIOR_EXIT_BALANCED?"balanced":"mismatch",
        r->exterior,(unsigned long)r->elapsed_ms,(unsigned long)r->tracked,(unsigned long)r->moved,
        (unsigned long)r->disable_raised,(unsigned long)r->disable_lowered,
        (unsigned)r->terrain_enabled_entry,(unsigned)r->terrain_enabled_exit);
}
static const SudekiMpLanStoryTempExteriorConsumer exterior_consumer={keep_exterior,exit_skipped,NULL};
void SudekiMpLanStorySplitSetEligibility(BOOL ready,uint8_t mask) {
    if(!base || !thread_exact()) return;
    if(traces<32u && (ready!=host_ready || mask!=outside_mask)) {
        ++traces; SudekiMpLogFormat("lan_story_split event=eligibility ready=%u outside=%u\r\n",ready,mask);
    }
    host_ready=ready; outside_mask=(uint8_t)(mask&15u);
}
BOOL SudekiMpLanStorySplitActive(void) {return active;}
const void *SudekiMpLanStorySplitTemporaryData(void) {return active&&settled_inside?temporary_data:NULL;}
BOOL SudekiMpLanStorySplitUninstall(void) {
    if(!base) return TRUE;
    if(active || exempt_count) {SetLastError(ERROR_BUSY); return FALSE;}
    if(observer_set) {
        if(!SudekiMpLanStoryObserverSetSplit(NULL)) {SetLastError(ERROR_BUSY); return FALSE;}
        observer_set=FALSE;
    }
    BOOL ok=SudekiMpRestoreRelativeCallHook(&scene_hook);
    ok=SudekiMpRestoreInlineHook(&full_hook) && ok;
    ok=SudekiMpRestoreInlineHook(&lead_hook) && ok;
    if(attached) {
        if(!SudekiMpLanStoryTempExteriorDetach()) ok=FALSE; else attached=FALSE;
    }
    if(!attached && !SudekiMpLanStoryTempExteriorUninstall()) ok=FALSE;
    if(!ok) {SetLastError(ERROR_BUSY); return FALSE;}
    base=NULL; native_thread=0; host_ready=FALSE; outside_mask=0;
    return TRUE;
}
BOOL SudekiMpLanStorySplitInstall(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    static const uint8_t lead_bytes[10]={0x83,0xec,0x14,0x83,0xb9,0xd0,0,0,0,0};
    static const uint8_t full_bytes[6]={0x83,0xec,0x10,0x53,0x8b,0xd9};
    static const uint8_t pause_head[7]={0x57,0x8b,0xf8,0x80,0x7f,0x23,0x00};
    static const uint8_t resume_head[5]={0xfe,0x4e,0x23,0x75,0x16};
    if(base || !b || !SudekiMpCheckLoadedExecutable(image) ||
        !readable(b+NODE_PAUSE,7) || memcmp(b+NODE_PAUSE,pause_head,7) ||
        !readable(b+NODE_RESUME,5) || memcmp(b+NODE_RESUME,resume_head,5)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    base=b; SudekiMpLanStorySplitNodePause=b+NODE_PAUSE; SudekiMpLanStorySplitNodeResume=b+NODE_RESUME;
    DWORD error=ERROR_SUCCESS;
    if(!SudekiMpLanStoryTempExteriorInstall(image)) error=GetLastError();
    else if(!SudekiMpLanStoryTempExteriorAttach(&exterior_consumer)) error=ERROR_INVALID_STATE;
    else attached=TRUE;
    if(!error && !SudekiMpInstallInlineHook(&lead_hook,b+LEAD_ONLY,lead_bytes,sizeof(lead_bytes),(const void *)(uintptr_t)lead_only))
        error=GetLastError();
    if(!error && !SudekiMpInstallInlineHook(&full_hook,b+FULL_PARTY,full_bytes,sizeof(full_bytes),(const void *)(uintptr_t)full_party))
        error=GetLastError();
    static const uint8_t walk_head[6]={0x56,0x8b,0xf0,0x80,0xbe,0x88};   /* PUSH ESI; MOV ESI,EAX; CMP byte ptr [ESI+0x88],0 */
    static const uint8_t update_head[6]={0x55,0x8b,0xec,0x83,0xe4,0xf8};  /* PUSH EBP; MOV EBP,ESP; AND ESP,-8 */
    if(!error && (!readable(b+SCENE_WALK,6) || memcmp(b+SCENE_WALK,walk_head,6) ||
        !readable(b+OBJECT_UPDATE,6) || memcmp(b+OBJECT_UPDATE,update_head,6))) error=ERROR_INVALID_DATA;
    if(!error) {
        SudekiMpLanStorySplitSceneWalk=b+SCENE_WALK; SudekiMpLanStorySplitObjectUpdate=b+OBJECT_UPDATE;
        if(!SudekiMpInstallRelativeCallHook(&scene_hook,b+SCENE_CALL,b+SCENE_WALK,(const void *)(uintptr_t)scene_entry))
            error=GetLastError();
    }
    if(!error) {
        if(SudekiMpLanStoryObserverSetSplit(&observer_split)) observer_set=TRUE;
        else error=ERROR_INVALID_STATE;
    }
    if(error) {
        (void)SudekiMpLanStorySplitUninstall();
        SetLastError(error?error:ERROR_INVALID_STATE); return FALSE;
    }
    SudekiMpLogWrite("lan_story_split event=installed seams=temp_exterior,lead_only,full_party,scene_walk\r\n");
    return TRUE;
}
