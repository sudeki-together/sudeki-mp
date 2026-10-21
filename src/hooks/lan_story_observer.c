#include "hooks/lan_story_observer.h"
#include "hooks/lan_story_area_follow.h"
#include "hooks/call_hook.h"
#include "cleanroom/engine.h"
#include "engine/build_identity.h"
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
static DWORD vanilla_transition;
static BOOL vanilla_transition_pending;
static DWORD native_thread;
static unsigned call_depth;
static BOOL foreign_thread,exhausted,installed;
static SudekiMpLanStoryScene observed,published;
static void *last_world,*last_descriptor,*last_group,*last_controller,*last_actors[4];
static BOOL last_exact;
static const SudekiMpLanStoryObserverSplit *split_owner;
static BOOL split_active,split_transition,split_exiting;
/* H1b remote interior: characters standing in a background-resident house
 * while the lead stays outside (observed.temporary names the house). */
static uint8_t remote_mask;
static void (*remote_evict)(void);
static void (*remote_lead_entry)(const char *zone);
/* A house the remote-interior owner keeps background-resident (state 2). */
static char resident_house[64];
static BOOL name_copy(char out[64],const char *name);
/* A member visit in progress (loading or inside) for this house, set by the
 * remote-interior owner; zone calls other than this house evict it. */
static char visit_zone[64];
static BOOL split_began_call;
static BOOL remote_zone_is(const char *name) {
    char copy[64];
    return name && name_copy(copy,name) &&
        ((remote_mask && !strcmp(copy,observed.temporary)) || (visit_zone[0] && !strcmp(copy,visit_zone)));
}
/* The exterior stays live while characters are in a temporary only through
 * a split (lead inside) or a remote interior (members inside). */
static void set_retained(void) {
    observed.exterior_retained=(uint8_t)(observed.phase==SUDEKIMP_LAN_STORY_READY &&
        observed.temporary[0] && (split_active || remote_mask)?1u:0u);
}
/* A one-sample native party miss during a background zone load is not a
 * scene replacement (H1b: the house load cleared the exact proof for one
 * frame and the new epoch froze every client). */
static DWORD background_switch_tick,hold_started,hold_ended;
static BOOL holding;
/* H1b client: this client's own house entry/exit (area owner call) is not a
 * scene replacement; the native transition's non-exact samples keep the
 * epoch. Cleared at the next exact sample or after a bound. */
static DWORD local_transition;
static BOOL local_began_call;
static char local_prev_temporary[64];
enum { HOLD_AFTER_SWITCH_MS=20000u, HOLD_LIMIT_MS=2000u };
static unsigned last_logged_revision;
static uint64_t last_dispatch_serial;
/* Only the opt-in helper raises required. An ordinary authored door keeps
 * its existing fallback policy; a test request must never fall back to a
 * whole-party TEMP transition when the split owner declines. */
static BOOL test_temp_required,test_temp_admitted,test_temp_dispatched;
/* Diagnostics only. A bounded synchronous copy is not a retained native
 * ResourceName, a transition ticket, or permission to replay a zone call. */
enum { TEMP_JOURNAL_LIMIT=64, TEMP_RESOURCE_TEXT=128 };
static unsigned temporary_records;
static BOOL temporary_overflow_logged;
typedef struct TemporaryResourceCopy {
    BOOL fields_valid,text_valid;
    uint32_t kind,identifier,references;
    char text[TEMP_RESOURCE_TEXT];
} TemporaryResourceCopy;
typedef struct TemporaryWorldCopy {
    BOOL exact,context_valid;
    void *descriptor,*pending,*returning,*departed;
    uint32_t state,pending_state,return_bits[6];
    uint16_t sector;
    uint8_t ready,placed;
    char current[64],destination[64];
} TemporaryWorldCopy;
typedef struct TemporaryJournal {
    unsigned serial,kind,depth;
    uint32_t epoch;
    BOOL name_valid,null_name;
    char world[64],destination[64];
    TemporaryResourceCopy resource;
    TemporaryWorldCopy before;
} TemporaryJournal;

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
static TemporaryResourceCopy temporary_resource_copy(const void *address) {
    TemporaryResourceCopy out={0};
    SudekiMpResourceName r;
    if(!readable(address,sizeof(r))) return out;
    memcpy(&r,address,sizeof(r));
    out.fields_valid=TRUE; out.kind=r.encoded_kind; out.identifier=r.identifier;
    /* Exact image 5B96A0 constructs backing {references,char *}; 5B96DB
     * stores the separately allocated text at backing+4. Never log pointers
     * or increment references: the native caller owns this synchronous read. */
    uint32_t *reference=r.text_reference;
    if(r.identifier==0x7ffffu || !readable(reference,8u)) return out;
    uint32_t count=reference[0];
    const char *text=(const char *)(uintptr_t)reference[1];
    if(!count || count==UINT32_MAX || !text) return out;
    out.references=count;
    for(unsigned i=0;i<TEMP_RESOURCE_TEXT;++i) {
        if(!readable(text+i,1)) break;
        unsigned char c=(unsigned char)text[i];
        if(!c) {
            if(i && readable(address,sizeof(r)) && !memcmp(address,&r,sizeof(r)) &&
                readable(reference,8u) && reference[0]==count &&
                reference[1]==(uint32_t)(uintptr_t)text &&
                readable(text,i+1u) && !memcmp(out.text,text,i+1u)) out.text_valid=TRUE;
            break;
        }
        /* Authored marker/path alphabet, preserved verbatim. Unexpected or
         * overlong text is UNKNOWN, never truncated into a usable marker. */
        if(!((c>='A'&&c<='Z') || (c>='a'&&c<='z') || (c>='0'&&c<='9') ||
            c=='_' || c=='-' || c=='.' || c==':' || c=='/' || c=='\\')) break;
        out.text[i]=(char)c;
    }
    if(!out.text_valid) memset(out.text,0,sizeof(out.text));
    return out;
}
static BOOL temporary_descriptor(const uint8_t *table,unsigned count,
    const void *descriptor,uint32_t *state,char name[64]) {
    if(!descriptor) { *state=UINT32_MAX; return TRUE; }
    uintptr_t start=(uintptr_t)table,p=(uintptr_t)descriptor;
    if(p<start || (p-start)%0x54u || (p-start)/0x54u>=count) return FALSE;
    const uint8_t *d=(const uint8_t *)descriptor;
    *state=*(const uint32_t *)(d+0x34u);
    return name_copy(name,*(const char *const *)(d+0x24u));
}
static TemporaryWorldCopy temporary_world_copy(void *recipient) {
    TemporaryWorldCopy out={0};
    out.state=out.pending_state=UINT32_MAX;
    uint8_t *w=(uint8_t *)recipient;
    if(!base || !readable(base+0x408d10u,4u) ||
        recipient!=*(void **)(base+0x408d10u) || !readable(w,0x39bu)) return out;
    uint8_t *table=*(uint8_t **)(w+0x50u);
    unsigned count=*(unsigned *)(w+0x54u);
    /* The native FindZone walk is a contiguous array of 0x54-byte entries.
     * Bound diagnostics independently of whatever native code accepts. */
    if(!count || count>4096u || !readable(table,(size_t)count*0x54u)) return out;
    out.descriptor=*(void **)(w+0x0cu); out.pending=*(void **)(w+0x14u);
    out.returning=*(void **)(w+0x10u);
    out.departed=*(void **)(w+0x394u);
    if(!temporary_descriptor(table,count,out.descriptor,&out.state,out.current) ||
        !temporary_descriptor(table,count,out.pending,&out.pending_state,out.destination)) return out;
    memcpy(out.return_bits,w+0x28u,sizeof(out.return_bits));
    /* Native temporary entry copies the lead's CAiTracking packed sector;
     * exit consumes it in the return-placement packet. This is not a camera
     * selector, and a copied sector is not proof of valid ground collision. */
    memcpy(&out.sector,w+0x40u,sizeof(out.sector));
    out.ready=w[0x399u]; out.placed=w[0x39au]; out.context_valid=TRUE;
    for(unsigned i=0;i<6u;++i)
        if((out.return_bits[i]&0x7f800000u)==0x7f800000u) out.context_valid=FALSE;
    out.exact=recipient==*(void **)(base+0x408d10u) &&
        table==*(void **)(w+0x50u) && count==*(unsigned *)(w+0x54u) &&
        out.descriptor==*(void **)(w+0x0cu) && out.pending==*(void **)(w+0x14u);
    return out;
}
static void temporary_journal_begin(TemporaryJournal *j,unsigned kind,uint32_t source_epoch,
    void *world,const char *name,const void *resource) {
    memset(j,0,sizeof(*j));
    AcquireSRWLockExclusive(&state_lock);
    if(temporary_records==TEMP_JOURNAL_LIMIT) {
        if(!temporary_overflow_logged) {
            temporary_overflow_logged=TRUE;
            SudekiMpLogWrite("lan_story event=temporary_journal status=capacity_reached limit=64 policy=observation_only\r\n");
        }
    } else {
        j->serial=++temporary_records; j->kind=kind; j->depth=call_depth?call_depth-1u:0u;
        j->epoch=source_epoch;
        memcpy(j->world,observed.world,sizeof(j->world));
        j->null_name=name==NULL;
        /* Existing zone hooks bind the native thread. A conflicting callback
         * is logged UNKNOWN and still reaches its untouched native original. */
        if(!foreign_thread && native_thread==GetCurrentThreadId() && call_depth) {
            j->before=temporary_world_copy(world);
            if(kind==TEMP) {
                j->name_valid=name_copy(j->destination,name);
                j->resource=temporary_resource_copy(resource);
            }
        }
    }
    ReleaseSRWLockExclusive(&state_lock);
}
static void temporary_journal_end(const TemporaryJournal *j,void *world) {
    if(!j->serial) return;
    AcquireSRWLockExclusive(&state_lock);
    TemporaryWorldCopy after={0}; TemporaryResourceCopy retained={0};
    if(!foreign_thread && native_thread==GetCurrentThreadId()) {
        after=temporary_world_copy(world);
        if(j->kind==TEMP && after.exact)
            retained=temporary_resource_copy((uint8_t *)world+0x44u);
    }
    const char *result="unknown";
    if(j->kind==TEMP) {
        if(j->resource.fields_valid && (j->null_name || j->resource.identifier==0x7ffffu))
            result="native_early_reject";
        else if(j->before.exact && after.exact && j->before.state==3u &&
            j->before.descriptor==after.descriptor && !j->before.pending && after.pending &&
            !after.ready && !after.placed && after.context_valid && j->name_valid &&
            !strcmp(j->destination,after.destination) && j->resource.text_valid &&
            retained.text_valid && retained.identifier==j->resource.identifier &&
            (retained.kind&0x1fffu)==((j->resource.kind&0x1fb5u)|0x35u) &&
            !strcmp(j->resource.text,retained.text)) result="accepted_pending";
    } else if(j->before.exact && j->before.state!=4u) result="native_noop";
    else if(j->before.exact && after.exact && j->before.state==4u &&
        after.departed==j->before.descriptor && !after.descriptor && !after.pending && !after.returning &&
        !after.placed) result="exit_started";
    SudekiMpLogFormat("lan_story event=temporary_journal serial=%u method=%u source_epoch=%lu after_epoch=%lu depth=%u result=%s before_exact=%u after_exact=%u world=%s source=%s target=%s source_state=%lu pending_state=%lu before_flags=%u,%u after_flags=%u,%u completion=unobserved policy=observation_only\r\n",
        j->serial,j->kind,(unsigned long)j->epoch,(unsigned long)observed.epoch,j->depth,result,
        j->before.exact,after.exact,j->world,j->before.current,j->destination,
        (unsigned long)j->before.state,(unsigned long)after.pending_state,
        j->before.ready,j->before.placed,after.ready,after.placed);
    SudekiMpLogFormat("lan_story event=temporary_resource serial=%u resource_fields=%u resource_text_valid=%u resource_kind=%08lx resource_id=%08lx resource_refs=%lu resource_text=%s retained_text_valid=%u retained_kind=%08lx retained_id=%08lx retained_text=%s policy=observation_only\r\n",
        j->serial,
        j->resource.fields_valid,j->resource.text_valid,(unsigned long)j->resource.kind,
        (unsigned long)j->resource.identifier,(unsigned long)j->resource.references,j->resource.text,
        retained.text_valid,(unsigned long)retained.kind,(unsigned long)retained.identifier,retained.text);
    /* Entry writes the exterior return context; exit consumes the stored
     * pre-call context. These are exact float bits, not the interior arrival. */
    const TemporaryWorldCopy *context=j->kind==TEMP?&after:&j->before;
    SudekiMpLogFormat("lan_story event=temporary_return_context serial=%u captured=%u finite=%u phase=%s position_bits=%08lx,%08lx,%08lx direction_bits=%08lx,%08lx,%08lx sector=%u completion=unobserved policy=observation_only\r\n",
        j->serial,context->exact,context->context_valid,j->kind==TEMP?"after_entry":"before_exit",
        (unsigned long)context->return_bits[0],(unsigned long)context->return_bits[1],
        (unsigned long)context->return_bits[2],(unsigned long)context->return_bits[3],
        (unsigned long)context->return_bits[4],(unsigned long)context->return_bits[5],context->sector);
    ReleaseSRWLockExclusive(&state_lock);
}
static void increment(uint32_t *value) {
    if(*value==UINT32_MAX) exhausted=TRUE;
    else ++*value;
}
static void unknown_scene(unsigned phase) {
    observed.phase=(uint8_t)phase;
    observed.available_mask=0; observed.inside_mask=0; observed.exterior_retained=0;
    observed.leader_seat=SUDEKIMP_LAN_STORY_NO_SEAT;
}
/* Native EnterTemporaryZone (0x64B0) returns at once unless its target is
 * unloaded (0x64E3). A TEMP call that cannot enter must not begin a split or
 * a new epoch. */
static BOOL temp_target_enterable(const char *name) {
    char want[64],dn[64];
    uint8_t *w=base && readable(base+0x408d10u,4u)?*(uint8_t **)(base+0x408d10u):NULL;
    if(!name_copy(want,name) || !readable(w,0x60u)) return TRUE; /* unknown: native decides */
    unsigned count=*(unsigned *)(w+0x54u); uint8_t *table=*(uint8_t **)(w+0x50u);
    if(!count || count>512u || !readable(table,count*0x54u)) return TRUE;
    for(unsigned i=0;i<count;++i) {
        uint8_t *d=table+i*0x54u; const char *n=*(const char **)(d+0x24u);
        if(!readable(n,1u) || !name_copy(dn,n) || strcmp(dn,want)) continue;
        return !*(uint32_t *)(d+0x34u);
    }
    return TRUE;
}
static uint32_t begin_zone(unsigned kind,const char *name) {
    AcquireSRWLockExclusive(&state_lock);
    DWORD thread=GetCurrentThreadId();
    if(native_thread && native_thread!=thread) foreign_thread=TRUE;
    else native_thread=thread;
    ++call_depth;
    if(kind==TEMP && test_temp_required) test_temp_admitted=FALSE;
    uint32_t source_epoch=observed.epoch;
    if(remote_mask && kind!=SWITCH && !(kind==TEMP && remote_zone_is(name))) {
        /* The evict callback did not end it: the remote interior cannot
         * survive a lead/world change. Vanilla path below. */
        SudekiMpLogFormat("lan_story event=remote_interior_abandoned kind=%u temporary=%s mask=%u\r\n",
            kind,observed.temporary,remote_mask);
        remote_mask=0; memset(observed.temporary,0,sizeof(observed.temporary));
        observed.inside_mask=0;
    }
    /* Split-area travel keeps the exterior epoch: only the lead's area moves.
     * Anything else (or a declined/unknown request) is vanilla whole-party. */
    if(kind==TEMP && call_depth==1u && name && !temp_target_enterable(name)) {
        /* Native no-op: keep the scene (and any split) exactly as it is. */
        SudekiMpLogFormat("lan_story event=temp_not_enterable temporary=%s epoch=%lu policy=native_noop\r\n",
            name,(unsigned long)observed.epoch);
        ReleaseSRWLockExclusive(&state_lock);
        return source_epoch;
    }
    if(kind==TEMP && split_owner && !split_active && call_depth==1u &&
        published.phase==SUDEKIMP_LAN_STORY_READY && published.revision &&
        observed.phase==SUDEKIMP_LAN_STORY_READY &&
        (!observed.temporary[0] || remote_zone_is(name)) &&
        observed.leader_seat<4u && (observed.available_mask&~(1u<<observed.leader_seat)) &&
        name && split_owner->begin_temp(name,observed.leader_seat)) {
        char copy[64];
        if(name_copy(copy,name)) {
            memcpy(observed.temporary,copy,sizeof(copy));
            /* H1b: members already standing in this house stay inside. */
            observed.inside_mask=(uint8_t)((1u<<observed.leader_seat)|remote_mask);
            split_active=split_transition=TRUE; split_exiting=FALSE;
            observed.exterior_retained=1u; split_began_call=TRUE;
            if(test_temp_required) test_temp_admitted=TRUE;
            SudekiMpLogFormat("lan_story event=split_begin epoch=%lu temporary=%s inside=%u available=%u policy=exterior_epoch_retained\r\n",
                (unsigned long)observed.epoch,observed.temporary,observed.inside_mask,observed.available_mask);
            ReleaseSRWLockExclusive(&state_lock);
            return source_epoch;
        }
        if(split_owner->ended) split_owner->ended(TRUE);
    }
    if(kind==TEMP && test_temp_required) {
        --call_depth;
        ReleaseSRWLockExclusive(&state_lock);
        return source_epoch;
    }
    if(kind==EXIT && split_active && !split_exiting && call_depth==1u &&
        split_owner && split_owner->begin_exit()) {
        memset(observed.temporary,0,sizeof(observed.temporary));
        observed.inside_mask=0; split_transition=split_exiting=TRUE;
        SudekiMpLogFormat("lan_story event=split_exit_begin epoch=%lu policy=exterior_epoch_retained\r\n",
            (unsigned long)observed.epoch);
        ReleaseSRWLockExclusive(&state_lock);
        return source_epoch;
    }
    if(split_active) {
        split_active=split_transition=split_exiting=FALSE;
        if(split_owner && split_owner->ended) split_owner->ended(TRUE);
        SudekiMpLogFormat("lan_story event=split_abandoned kind=%u policy=vanilla_epoch\r\n",kind);
    }
    if(kind==SWITCH) { background_switch_tick=GetTickCount(); if(!background_switch_tick) background_switch_tick=1u; }
    if((kind==TEMP || kind==EXIT) && call_depth==1u && !split_active && SudekiMpLanStoryAreaFollowCalling()) {
        memcpy(local_prev_temporary,observed.temporary,sizeof(local_prev_temporary));
        local_began_call=(kind==TEMP);
        memset(observed.temporary,0,sizeof(observed.temporary));
        if(kind==TEMP && !name_copy(observed.temporary,name)) unknown_scene(SUDEKIMP_LAN_STORY_UNKNOWN);
        else unknown_scene(SUDEKIMP_LAN_STORY_LOADING);
        local_transition=GetTickCount()|1u;
        SudekiMpLogFormat("lan_story event=local_temporary method=%u epoch=%lu temporary=%s policy=epoch_retained\r\n",
            kind,(unsigned long)observed.epoch,observed.temporary);
        ReleaseSRWLockExclusive(&state_lock);
        return source_epoch;
    }
    if(kind==SWITCH || (kind==ENTER && SudekiMpLanStoryAreaFollowCalling())) {
        /* SwitchZoneNOW only requests a background load of the target
         * (FUN_00405a70: target state 0->1, world+0x399 cleared); the current
         * zone stays active and playable, and the native world swaps current
         * later as the party walks in, which the sampler sees as a zone walk.
         * Not a scene replacement (#42). */
        SudekiMpLogFormat("lan_story event=%s epoch=%lu target=%s current_world=%s policy=%s\r\n",
            kind==SWITCH?"zone_switch_requested":"zone_enter_followed",(unsigned long)observed.epoch,
            name?name:"",observed.world,kind==SWITCH?"background_load":"host_area_follow");
        ReleaseSRWLockExclusive(&state_lock);
        return source_epoch;
    }
    if(remote_mask) {
        SudekiMpLogFormat("lan_story event=remote_interior_abandoned kind=%u temporary=%s mask=%u policy=vanilla_whole_party\r\n",
            kind,observed.temporary,remote_mask);
        remote_mask=0;
    }
    vanilla_transition_pending=kind==TEMP || kind==EXIT;
    if(vanilla_transition_pending) vanilla_transition=GetTickCount();
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
    return source_epoch;
}
static void end_zone(void) {
    AcquireSRWLockExclusive(&state_lock);
    if(call_depth) --call_depth;
    ReleaseSRWLockExclusive(&state_lock);
}
/* Called on the native thread before a whole-world zone call runs its
 * original (#42): the native load decides at its start whether every party
 * member can travel, so held controls must be returned before it begins. */
static void (*world_change_hook)(void);
static void before_world_change(void) {
    void (*hook)(void)=world_change_hook;
    if(hook && call_depth==0u && (!native_thread || native_thread==GetCurrentThreadId())) hook();
}
BOOL SudekiMpLanStoryObserverSetWorldChangeHook(void (*hook)(void)) {
    world_change_hook=hook; return TRUE;
}
/* H1b: members standing in a remote interior leave it before any zone call
 * except a background load of that same house. */
static void before_area_change(const char *switch_target) {
    void (*evict)(void)=remote_evict;
    char name[64];
    if((!remote_mask && !visit_zone[0]) || !evict || call_depth ||
        (native_thread && native_thread!=GetCurrentThreadId())) return;
    if(switch_target && name_copy(name,switch_target) &&
        ((remote_mask && !strcmp(name,observed.temporary)) || (visit_zone[0] && !strcmp(name,visit_zone)))) return;
    evict();
}
static void __cdecl zone_set(const char *s) { before_area_change(NULL); before_world_change(); begin_zone(SET,s); ((ZoneCall)hooks[SET].trampoline)(s); end_zone(); }
static void __cdecl zone_enter(const char *s) { before_area_change(NULL); before_world_change(); begin_zone(ENTER,s); ((ZoneCall)hooks[ENTER].trampoline)(s); end_zone(); }
static void __cdecl zone_switch(const char *s) { before_area_change(s); begin_zone(SWITCH,s); ((ZoneCall)hooks[SWITCH].trampoline)(s); end_zone(); }
static void __cdecl zone_load(const char *s) { before_area_change(NULL); begin_zone(LOAD,s); ((ZoneCall)hooks[LOAD].trampoline)(s); end_zone(); }
static void __attribute__((thiscall)) zone_main(void *w,const char *s) { before_area_change(NULL); before_world_change(); begin_zone(MAIN,s); ((MainCall)hooks[MAIN].trampoline)(w,s); end_zone(); }
static const struct {const char *name; unsigned rva,size; uint8_t bytes[42];} test_temp_guards[]={
    {"TestTemporaryEntryBody",0x64b9,42,{0x53,0x56,0x8b,0x75,0x0c,0x8b,0x06,0x83,0xe0,0xb5,0x83,0xc8,0x35,0x89,0x06,
        0x8b,0x45,0x08,0x57,0x8b,0xd9,0x85,0xc0,0x0f,0x84,0x31,0x02,0,0,0x81,0x7e,0x04,0xff,0xff,0x07,0,0x0f,0x84,0x24,0x02,0,0}},
    {"TestTemporaryUnloadedTarget",0x64e3,21,{0x50,0x53,0xe8,0xc6,0xf4,0xff,0xff,0x85,0xc0,0x74,0x0a,0x83,0x78,0x34,0,
        0x0f,0x85,0x0f,0x02,0,0}},
    {"TestTemporaryResourceCopy",0x6628,10,{0x8d,0x4b,0x44,0x8b,0xc6,0xe8,0x8e,0xe5,0xff,0xff}},
    {"TestTemporaryReturnABI",0x6707,9,{0x5f,0x5e,0x5b,0x8b,0xe5,0x5d,0xc2,0x08,0}},
    {"TestTemporaryResourceRetain",0x1b9676,16,{0x8b,0x7f,0x08,0x89,0x7e,0x08,0x8b,0xc6,0x85,0xff,0x74,0x02,0xff,0x07,0x5f,0xc3}}
};
/* H1b: the lead's native TEMP into the house members use while the host
 * keeps it background-resident (state 2). The native entry cannot enter a
 * loaded house: an earlier pass let it through by reading the house as
 * unloaded for that call, and the pending handler (0x405B90) then released
 * the exterior's zone object and waited for a load that never came
 * (CONFIRMED_LIVE 2026-10-17: lead paused at the door, the exterior gone
 * except loose objects). The remote-interior owner now moves its members out
 * and unloads the house first, so the native entry loads it as in vanilla. */
static void lead_entry_release(const char *s) {
    char name[64];
    void (*release)(const char *)=remote_lead_entry;
    if(!release || !resident_house[0] || call_depth || !name_copy(name,s) || strcmp(name,resident_house) ||
        (native_thread && native_thread!=GetCurrentThreadId())) return;
    release(s);
}
/* H1b: the last native TEMP entry's zone and arrival marker text, so
 * clients can enter the same interior locally (copied synchronously; the
 * caller owns the ResourceName). Native thread only. */
static char last_temp_zone[64],last_temp_marker[TEMP_RESOURCE_TEXT];
static void note_temporary_marker(const char *s,const void *r) {
    char zone[64]; TemporaryResourceCopy m=temporary_resource_copy(r);
    if(!s || !m.text_valid || !readable(s,1u)) return;
    size_t n=strnlen(s,sizeof(zone)-1u); memcpy(zone,s,n); zone[n]=0;
    memcpy(last_temp_zone,zone,sizeof(zone)); memcpy(last_temp_marker,m.text,sizeof(last_temp_marker));
}
BOOL SudekiMpLanStoryObserverLastTemporary(char zone[40],char marker[40]) {
    size_t a=strnlen(last_temp_zone,sizeof(last_temp_zone)),b=strnlen(last_temp_marker,sizeof(last_temp_marker));
    memset(zone,0,40); memset(marker,0,40);
    if(!a || !b || a>=40u || b>=40u) return FALSE;
    memcpy(zone,last_temp_zone,a); memcpy(marker,last_temp_marker,b);
    return TRUE;
}
static void __attribute__((thiscall)) zone_temp(void *w,const char *s,const void *r) {
    TemporaryJournal journal;
    lead_entry_release(s);
    before_area_change(s);
    note_temporary_marker(s,r);
    uint32_t source_epoch=begin_zone(TEMP,s);
    if(test_temp_required && !test_temp_admitted) return;
    SudekiMpLanStoryAreaNoteTemporary(s);
    temporary_journal_begin(&journal,TEMP,source_epoch,w,s,r);
    if(test_temp_required) test_temp_dispatched=TRUE;
    split_began_call=split_began_call && split_active;
    BOOL began=split_began_call; split_began_call=FALSE;
    BOOL local_began=local_began_call; local_began_call=FALSE;
    ((TempCall)hooks[TEMP].trampoline)(w,s,r);
    if(local_began && readable(w,0x18u) && !*(void **)((uint8_t *)w+0x14u)) {
        /* This client's own entry did not take: back to the scene it had
         * (the next exact sample republishes it, same epoch). */
        AcquireSRWLockExclusive(&state_lock);
        memcpy(observed.temporary,local_prev_temporary,sizeof(observed.temporary));
        local_transition=0;
        SudekiMpLogFormat("lan_story event=local_temporary_rolled_back temporary=%s policy=native_noop\r\n",s?s:"");
        ReleaseSRWLockExclusive(&state_lock);
    }
    if(began && readable(w,0x18u) && !*(void **)((uint8_t *)w+0x14u)) {
        /* The native entry did not take (no pending zone): undo this
         * split without an epoch change; members keep their house. */
        AcquireSRWLockExclusive(&state_lock);
        if(split_active && split_transition) {
            split_active=split_transition=split_exiting=FALSE;
            if(!remote_mask) memset(observed.temporary,0,sizeof(observed.temporary));
            observed.inside_mask=remote_mask;
            set_retained();
            SudekiMpLogFormat("lan_story event=split_rolled_back temporary=%s policy=native_noop\r\n",s?s:"");
            ReleaseSRWLockExclusive(&state_lock);
            if(split_owner && split_owner->ended) split_owner->ended(TRUE);
        } else ReleaseSRWLockExclusive(&state_lock);
    }
    temporary_journal_end(&journal,w); end_zone();
}
static void __attribute__((thiscall)) zone_exit(void *w) {
    TemporaryJournal journal;
    before_area_change(NULL);
    uint32_t source_epoch=begin_zone(EXIT,NULL);
    temporary_journal_begin(&journal,EXIT,source_epoch,w,NULL,NULL);
    ((ExitCall)hooks[EXIT].trampoline)(w);
    temporary_journal_end(&journal,w); end_zone();
}

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

static void publish_ready_membership(uint8_t mask,uint8_t lead) {
    observed.phase=SUDEKIMP_LAN_STORY_READY;
    vanilla_transition_pending=FALSE;
    observed.available_mask=mask; observed.leader_seat=lead;
    /* Vanilla TEMP places only the lead. Additional members require an
     * explicit visit or this READY epoch's independent region observation. */
    observed.inside_mask=(uint8_t)(!observed.temporary[0]?0u:
        split_active?((1u<<lead)|(remote_mask&mask)):remote_mask?(remote_mask&mask):
        ((1u<<lead)|(observed.inside_mask&mask)));
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
    unsigned count=0,miss=0,miss_slot=0;
    if(readable(world,0x39bu)) descriptor=*(uint8_t **)(world+0x0cu);
    /* Diagnostic only (#42): first party check that fails while a world is named. */
    if(!readable(world,0x39bu)) miss=1;
    else if(!readable(descriptor,0x38u)) miss=2;
    else if(*(void **)(world+0x14u)) miss=3;
    else if(!world[0x39au] || (!world[0x399u] && *(uint32_t *)(descriptor+0x34u)!=3u)) miss=4;
    else if(*(uint32_t *)(descriptor+0x34u)!=(observed.temporary[0] && (!remote_mask || split_active)?4u:3u)) miss=5;
    else if(!observed.world[0]) miss=6;
    else if(!readable(group,0xd0u)) miss=7;
    else if(!readable(controller,0x24cu) || controller!=*(void **)(base+0x408da4u)) miss=8;
    else if(!SudekiMpCleanroomEngineWorldReady()) miss=9;
    if(readable(world,0x39bu) && readable(descriptor,0x38u) &&
        /* world+0x399 is cleared for the whole background load that
         * SwitchZoneNOW starts; the active current zone stays playable (#42). */
        !*(void **)(world+0x14u) && world[0x39au] &&
        (world[0x399u] || *(uint32_t *)(descriptor+0x34u)==3u) &&
        *(uint32_t *)(descriptor+0x34u)==(observed.temporary[0] && (!remote_mask || split_active)?4u:3u) &&
        observed.world[0] && readable(group,0xd0u) && readable(controller,0x24cu) &&
        controller==*(void **)(base+0x408da4u) &&
        SudekiMpCleanroomEngineWorldReady()) {
        count=*(unsigned *)(group+0xccu);
        if(!count || count>4u) miss=10;
        if(count && count<=4u) {
            void *heroes[4];
            for(unsigned i=0;i<4u;++i) heroes[i]=SudekiMpCleanroomEngineActorEntity(actors[i]);
            exact=TRUE;
            for(unsigned slot=0;slot<count;++slot) {
                uint8_t *actor=*(uint8_t **)(group+0x90u+slot*0x0cu);
                unsigned seat=0;
                for(;seat<4u && (!actor || actor!=heroes[seat]);++seat) {}
                if(seat==4u || (mask&(1u<<seat)) || !readable(actor,0x98u)) {
                    miss=seat==4u?11u:12u; miss_slot=slot; exact=FALSE; break; }
                uint8_t *ai=*(uint8_t **)(actor+0x94u);
                if(!readable(ai,0x16cu) || *(void **)(ai+0x10u)!=actor) {
                    miss=13; miss_slot=slot; exact=FALSE; break; }
                found[seat]=actor; mask|=(uint8_t)(1u<<seat);
                if(!slot) lead=(uint8_t)seat;
            }
            exact=exact && lead<4u && *(void **)((uint8_t *)controller+0x248u)==found[lead] &&
                count==*(unsigned *)(group+0xccu) && world==*(void **)(base+0x408d10u) &&
                descriptor==*(void **)(world+0x0cu) && group==*(void **)(base+0x408d94u) &&
                controller==*(void **)(base+0x408da4u) &&
                SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w);
            if(!exact && !miss) miss=lead<4u &&
                *(void **)((uint8_t *)controller+0x248u)!=found[lead]?14u:15u;
        }
    }
    {
        static unsigned last_miss; static DWORD last_miss_tick;
        DWORD now=GetTickCount();
        if(exact) last_miss=0;
        else if(observed.world[0] && (miss!=last_miss || now-last_miss_tick>=3000u)) {
            last_miss=miss; last_miss_tick=now;
            SudekiMpLogFormat("lan_story event=party_miss check=%u slot=%u count=%u world=%s descriptor=%p kind=%lu ready=%u,%u target=%p lead_actor=%p split=%u,%u\r\n",
                miss,miss_slot,readable(group,0xd0u)?*(unsigned *)(group+0xccu):0u,observed.world,descriptor,
                readable(descriptor,0x38u)?(unsigned long)*(uint32_t *)(descriptor+0x34u):0ul,
                readable(world,0x39bu)?world[0x399u]:0u,readable(world,0x39bu)?world[0x39au]:0u,
                readable(controller,0x24cu)?*(void **)((uint8_t *)controller+0x248u):NULL,
                lead<4u?found[lead]:NULL,split_active,split_transition);
        }
    }
    {
        /* Background-load hold: keep publishing the last READY scene,
         * unchanged, for a bounded native party miss while a zone loads in
         * the background. last_* stay at the last exact proof, so a real
         * replacement is still detected on the next exact sample; roster
         * consumers fail closed meanwhile. */
        DWORD now=GetTickCount();
        BOOL loading_window=background_switch_tick && now-background_switch_tick<HOLD_AFTER_SWITCH_MS;
        if(!exact && last_exact && !split_active && !split_transition && loading_window &&
            observed.phase==SUDEKIMP_LAN_STORY_READY && published.revision &&
            observed.epoch==published.epoch && (!hold_started || now-hold_started<HOLD_LIMIT_MS)) {
            if(!hold_started) {
                hold_started=now?now:1u; holding=TRUE;
                SudekiMpLogFormat("lan_story event=background_hold epoch=%lu revision=%lu miss=%u\r\n",
                    (unsigned long)published.epoch,(unsigned long)published.revision,miss);
            }
            /* Content and revision unchanged; a fresh tick keeps the session
             * re-sending it (clients treat a 1 s old scene as stale). */
            published.observed_tick=GetTickCount();
            *out=published;
            ReleaseSRWLockExclusive(&state_lock); return TRUE;
        }
        if(hold_started) {
            SudekiMpLogFormat("lan_story event=background_hold_end exact=%u held_ms=%lu\r\n",
                exact,(unsigned long)(now-hold_started));
            hold_started=0; holding=FALSE; hold_ended=now?now:1u;
        }
    }
    if(split_transition && !exact) {
        /* Native lead travel is settling. The exterior and its other
         * occupants are unchanged, so keep publishing the READY split scene;
         * roster consumers still fail closed until native exactness returns. */
        set_retained();
        if(!published.revision || !SudekiMpLanStorySceneSame(&published,&observed))
            increment(&observed.revision);
        observed.observed_tick=GetTickCount();
        if(exhausted || !SudekiMpLanStorySceneValid(&observed)) {
            ReleaseSRWLockExclusive(&state_lock); return FALSE;
        }
        published=observed; *out=observed; last_dispatch_serial=w->dispatch_serial;
        ReleaseSRWLockExclusive(&state_lock); return TRUE;
    }
    /* Inside a split the current descriptor legitimately alternates between
     * exterior and TEMP; it is not a world replacement. Outside a split, a
     * descriptor change in the same world with the same party (walking from
     * one zone into the next, e.g. New Brightwater -> ws_country_se_to_bright_ne)
     * is not one either: the actors, group and controller are unchanged, so
     * it is the same scene (#42). A new epoch here made every
     * control fence and the client presentation stale with no way back. */
    BOOL same_party=last_exact && exact && world==last_world && group==last_group &&
        controller==last_controller && !memcmp(found,last_actors,sizeof(found));
    BOOL zone_walk=same_party && !split_active && descriptor!=last_descriptor;
    BOOL replaced=last_exact && !same_party;
    if(local_transition) {
        if(exact) {
            SudekiMpLogFormat("lan_story event=local_temporary_settled exact=1 epoch=%lu temporary=%s\r\n",
                (unsigned long)observed.epoch,observed.temporary);
            local_transition=0;
        } else if(GetTickCount()-local_transition>20000u) {
            /* Fail closed: the scene stays non-READY (unknown below) until
             * an exact sample; the bound only ends this record. */
            SudekiMpLogFormat("lan_story event=local_temporary_unsettled epoch=%lu temporary=%s policy=scene_unknown\r\n",
                (unsigned long)observed.epoch,observed.temporary);
            local_transition=0;
        }
        /* The party is the same while the native transition settles. */
        replaced=FALSE;
    }
    if(zone_walk) {
        /* No forced revision: nothing a peer sees changed (the session only
         * accepts a new revision with a visible scene change), so the scene,
         * its control fences and its frames simply continue. */
        SudekiMpLogFormat("lan_story event=zone_walk epoch=%lu revision=%lu descriptor=%p->%p policy=same_scene\r\n",
            (unsigned long)observed.epoch,(unsigned long)observed.revision,last_descriptor,descriptor);
    }
    if(replaced) {
        /* A scripted party removal can finish a split inside its TEMP (the
         * castle arrival removes Ailish). Abandon the exterior lease, not
         * this positively observed native destination. Never infer TEMP
         * identity from a readable descriptor or an old zone name alone. */
        BOOL replaced_temporary=FALSE;
        if(split_active && exact && observed.temporary[0] &&
            world==last_world && group==last_group && controller==last_controller) {
            TemporaryWorldCopy current=temporary_world_copy(world);
            replaced_temporary=current.exact && current.descriptor==descriptor &&
                !current.pending && current.state==4u &&
                !strcmp(current.current,observed.temporary);
        }
        increment(&observed.epoch);
        if(remote_mask) {
            SudekiMpLogFormat("lan_story event=remote_interior_abandoned kind=replaced temporary=%s mask=%u\r\n",
                observed.temporary,remote_mask);
            remote_mask=0;
            if(!replaced_temporary) memset(observed.temporary,0,sizeof(observed.temporary));
        }
        if(split_active) {
            split_active=split_transition=split_exiting=FALSE;
            if(split_owner && split_owner->ended) split_owner->ended(TRUE);
            if(!replaced_temporary) memset(observed.temporary,0,sizeof(observed.temporary));
            observed.inside_mask=0; /* Re-prove membership from the new lead. */
            if(!replaced_temporary && exact && *(uint32_t *)(descriptor+0x34u)==4u)
                exact=FALSE; /* A foreign/unproved TEMP never publishes READY. */
            SudekiMpLogFormat("lan_story event=split_abandoned kind=replaced policy=vanilla_epoch temporary_retained=%u\r\n",
                (unsigned)replaced_temporary);
        }
    }
    if(exact) {
        /* Drop incompatible explicit visits before publishing membership. */
        if(remote_mask && (!(mask&remote_mask) || (remote_mask&(1u<<lead)))) {
            SudekiMpLogFormat("lan_story event=remote_interior_dropped mask=%u available=%u lead=%u\r\n",
                remote_mask,mask,lead);
            remote_mask=0;
            if(!split_active) memset(observed.temporary,0,sizeof(observed.temporary));
        }
        publish_ready_membership(mask,lead);
        set_retained();
        if(split_transition) {
            BOOL leaving=split_exiting;
            split_transition=FALSE;
            if(split_owner && split_owner->settled) split_owner->settled(!leaving);
            if(leaving) {
                split_active=split_exiting=FALSE;
                if(split_owner && split_owner->ended) split_owner->ended(FALSE);
            }
            SudekiMpLogFormat("lan_story event=split_settled epoch=%lu inside=%u temporary=%s\r\n",
                (unsigned long)observed.epoch,observed.inside_mask,observed.temporary);
        }
    } else unknown_scene(observed.world[0]?SUDEKIMP_LAN_STORY_LOADING:SUDEKIMP_LAN_STORY_UNKNOWN);
    set_retained();
    if(!published.revision || !SudekiMpLanStorySceneSame(&published,&observed))
        increment(&observed.revision);
    observed.observed_tick=GetTickCount();
    if(exhausted || !SudekiMpLanStorySceneValid(&observed)) {
        ReleaseSRWLockExclusive(&state_lock); return FALSE;
    }
    last_exact=exact; last_world=world; last_descriptor=descriptor;
    last_group=group; last_controller=controller; memcpy(last_actors,found,sizeof(found));
    published=observed; *out=observed; last_dispatch_serial=w->dispatch_serial;
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

BOOL SudekiMpLanStoryObserverSetSplit(const SudekiMpLanStoryObserverSplit *split) {
    AcquireSRWLockExclusive(&state_lock);
    BOOL ok=!split_active && !call_depth &&
        (!split || (split->begin_temp && split->begin_exit));
    if(ok) split_owner=split;
    ReleaseSRWLockExclusive(&state_lock);
    return ok;
}
/* The host's current zone is the temporary one only when the lead is inside
 * (vanilla whole party, or a split); a remote interior keeps the exterior. */
static BOOL lead_inside(const SudekiMpLanStoryScene *s) {
    return s->temporary[0] && s->leader_seat<4u && (s->inside_mask&(1u<<s->leader_seat));
}
static BOOL roster_identity_locked(void *controller,
    const SudekiMpLanStoryScene *scene,SudekiMpLanStoryNativeRoster *out) {
    static const SudekiMpCleanroomActor types[4]={SUDEKIMP_CLEANROOM_BUKI,
        SUDEKIMP_CLEANROOM_ELCO,SUDEKIMP_CLEANROOM_TAL,SUDEKIMP_CLEANROOM_AILISH};
    SudekiMpLanStoryNativeRoster r={0};
    if(!base || !installed || !last_exact || call_depth ||
        exhausted || foreign_thread || native_thread!=GetCurrentThreadId() ||
        !SudekiMpLanStorySceneValid(scene) || scene->phase!=SUDEKIMP_LAN_STORY_READY ||
        scene->revision!=published.revision || !SudekiMpLanStorySceneSame(scene,&published))
        return FALSE;
    uint8_t *world=*(uint8_t **)(base+0x408d10u);
    uint8_t *group=*(uint8_t **)(base+0x408d94u);
    if(world!=last_world || group!=last_group || controller!=last_controller ||
        controller!=*(void **)(base+0x408da4u) || !readable(world,0x39bu) ||
        !readable(group,0xd0u) || !readable(controller,0x24cu) ||
        *(void **)(world+0x14u) || !world[0x39au] ||
        !SudekiMpCleanroomEngineWorldReady()) return FALSE;
    uint8_t *descriptor=*(uint8_t **)(world+0x0cu);
    if(!world[0x399u] && (!readable(descriptor,0x38u) || *(uint32_t *)(descriptor+0x34u)!=3u)) return FALSE;
    /* Walking into the next zone of the same world swaps the descriptor
     * during the native frame, before the next observer sample publishes it
     * as a revision (#42). Identity consumers (cast instances, lights, host
     * control) must not fail in that gap: the world, group, controller and
     * every party actor below are still proved exactly. Inside a split the
     * descriptor must still match (split_hold_identity_locked owns that). */
    if((descriptor!=last_descriptor && (split_active || split_transition)) ||
        !readable(descriptor,0x38u) ||
        *(uint32_t *)(descriptor+0x34u)!=(lead_inside(scene)?4u:3u)) return FALSE;
    unsigned count=*(unsigned *)(group+0xccu);
    if(!count || count>4u) return FALSE;
    void *heroes[4];
    for(unsigned c=0;c<4u;++c) heroes[c]=SudekiMpCleanroomEngineActorEntity(types[c]);
    for(unsigned i=0;i<count;++i) {
        uint8_t *actor=*(uint8_t **)(group+0x90u+i*0x0cu);
        unsigned c=0;
        for(;c<4u && (!actor || actor!=heroes[c]);++c) {}
        if(c>=4u || (r.available_mask&(1u<<c)) || !readable(actor,0x98u)) return FALSE;
        uint8_t *ai=*(uint8_t **)(actor+0x94u);
        if(!readable(ai,0x16cu) || *(void **)(ai+0x10u)!=actor) return FALSE;
        r.actors[c]=actor; r.ai[c]=ai; r.available_mask|=(uint8_t)(1u<<c);
        if(!i) r.leader_character=(uint8_t)c;
    }
    if(r.available_mask!=scene->available_mask || r.leader_character!=scene->leader_seat ||
        memcmp(r.actors,last_actors,sizeof(r.actors)) ||
        *(void **)((uint8_t *)controller+0x248u)!=r.actors[r.leader_character] ||
        count!=*(unsigned *)(group+0xccu) || world!=*(void **)(base+0x408d10u) ||
        group!=*(void **)(base+0x408d94u) || descriptor!=*(void **)(world+0x0cu) ||
        controller!=*(void **)(base+0x408da4u)) return FALSE;
    for(unsigned c=0;c<4u;++c) if(r.actors[c] &&
        (SudekiMpCleanroomEngineActorEntity(types[c])!=r.actors[c] ||
         *(void **)((uint8_t *)r.actors[c]+0x94u)!=r.ai[c])) return FALSE;
    r.epoch=scene->epoch; r.revision=scene->revision;
    r.world=world; r.descriptor=descriptor; r.group=group; r.controller=controller;
    *out=r; return TRUE;
}
static BOOL roster_locked(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryScene *scene,SudekiMpLanStoryNativeRoster *out) {
    if(!w || !w->service_post_original_exact || !w->dispatch_serial ||
        last_dispatch_serial!=w->dispatch_serial ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        !roster_identity_locked(controller,scene,out) ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) return FALSE;
    out->dispatch_serial=w->dispatch_serial; return TRUE;
}
static BOOL same_native_roster(const SudekiMpLanStoryNativeRoster *r,
    const SudekiMpLanStoryNativeRoster *fresh) {
    return r->epoch==fresh->epoch && r->revision==fresh->revision &&
        r->available_mask==fresh->available_mask && r->leader_character==fresh->leader_character &&
        r->world==fresh->world && r->descriptor==fresh->descriptor &&
        r->group==fresh->group && r->controller==fresh->controller &&
        !memcmp(r->actors,fresh->actors,sizeof(r->actors)) && !memcmp(r->ai,fresh->ai,sizeof(r->ai));
}
BOOL SudekiMpLanStoryObserverRoster(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene,
    SudekiMpLanStoryNativeRoster *out) {
    if(!out) return FALSE;
    AcquireSRWLockShared(&state_lock);
    BOOL ok=roster_locked(controller,w,scene,out);
    ReleaseSRWLockShared(&state_lock); return ok;
}
BOOL SudekiMpLanStoryObserverRosterStillExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {
    SudekiMpLanStoryNativeRoster fresh;
    if(!w || !r || r->dispatch_serial!=w->dispatch_serial) return FALSE;
    AcquireSRWLockShared(&state_lock);
    BOOL ok=roster_locked(r->controller,w,&published,&fresh) && same_native_roster(r,&fresh);
    ReleaseSRWLockShared(&state_lock); return ok;
}
/* Same exterior lifetime (epoch) and the same native world/group/
 * controller/actor/AI objects. Revision and current descriptor change with
 * split-area occupancy, during and after a split; membership, leader and
 * actor changes still fail. */
static BOOL same_native_identity(const SudekiMpLanStoryNativeRoster *r,
    const SudekiMpLanStoryNativeRoster *fresh) {
    return r->epoch==fresh->epoch && r->available_mask==fresh->available_mask &&
        r->leader_character==fresh->leader_character && r->world==fresh->world &&
        r->group==fresh->group && r->controller==fresh->controller &&
        !memcmp(r->actors,fresh->actors,sizeof(r->actors)) && !memcmp(r->ai,fresh->ai,sizeof(r->ai));
}
/* During the split transition hold the area is not native-ready, but the
 * party objects are unchanged: prove them directly from native globals. */
static BOOL split_hold_identity_locked(const SudekiMpLanStoryNativeRoster *r) {
    static const SudekiMpCleanroomActor types[4]={SUDEKIMP_CLEANROOM_BUKI,
        SUDEKIMP_CLEANROOM_ELCO,SUDEKIMP_CLEANROOM_TAL,SUDEKIMP_CLEANROOM_AILISH};
    if(!split_active || !split_transition || !base || exhausted || foreign_thread ||
        native_thread!=GetCurrentThreadId() || r->epoch!=observed.epoch ||
        *(void **)(base+0x408d10u)!=r->world || *(void **)(base+0x408d94u)!=r->group ||
        *(void **)(base+0x408da4u)!=r->controller || !readable(r->group,0xd0u) ||
        !readable(r->controller,0x24cu) ||
        *(void **)((uint8_t *)r->controller+0x248u)!=r->actors[r->leader_character<4u?r->leader_character:0]) return FALSE;
    unsigned count=*(unsigned *)((uint8_t *)r->group+0xccu),mask=0;
    if(!count || count>4u) return FALSE;
    for(unsigned i=0;i<count;++i) {
        void *actor=*(void **)((uint8_t *)r->group+0x90u+i*0x0cu);
        unsigned c=0;
        for(;c<4u && (!actor || actor!=r->actors[c]);++c) {}
        if(c>=4u || (mask&(1u<<c)) || SudekiMpCleanroomEngineActorEntity(types[c])!=actor ||
            !readable(actor,0x98u) || *(void **)((uint8_t *)actor+0x94u)!=r->ai[c]) return FALSE;
        mask|=1u<<c;
    }
    return mask==r->available_mask;
}
/* TRUE only while a whole-world zone change is pending (scene LOADING with a
 * named world, no split). Consumers use it to tell "unknown during a load"
 * from a real identity mismatch (#42). */
BOOL SudekiMpLanStoryObserverWorldLoading(void) {
    if(!installed) return FALSE;
    AcquireSRWLockShared(&state_lock);
    BOOL loading=observed.phase==SUDEKIMP_LAN_STORY_LOADING && observed.world[0] &&
        !observed.temporary[0] && !split_active && !split_transition;
    ReleaseSRWLockShared(&state_lock);
    return loading;
}
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *r) {
    SudekiMpLanStoryNativeRoster fresh;
    if(!r || !r->dispatch_serial) return FALSE;
    AcquireSRWLockShared(&state_lock);
    BOOL ok=roster_identity_locked(r->controller,&published,&fresh) &&
        (same_native_roster(r,&fresh) || same_native_identity(r,&fresh));
    if(!ok) ok=split_hold_identity_locked(r);
    ReleaseSRWLockShared(&state_lock); return ok;
}

static BOOL test_temp_code_exact(void) {
    const SudekiMpInlineHook *h=&hooks[TEMP];
    uint8_t patch[9]={0xe9,0,0,0,0,0x90,0x90,0x90,0x90},trampoline[14];
    int32_t displacement;
    MEMORY_BASIC_INFORMATION memory;
    if(!base || !installed || !SudekiMpCheckLoadedExecutable((HMODULE)base) ||
        !h->installed || h->target!=base+rvas[TEMP] || h->length!=9 ||
        memcmp(h->original,entries[TEMP],9) || !readable(h->target,9) || !readable(h->trampoline,14)) return FALSE;
    displacement=(int32_t)((uintptr_t)zone_temp-(uintptr_t)(h->target+5));
    memcpy(patch+1,&displacement,4);
    memcpy(trampoline,entries[TEMP],9);trampoline[9]=0xe9;
    displacement=(int32_t)((uintptr_t)(h->target+9)-((uintptr_t)h->trampoline+14));
    memcpy(trampoline+10,&displacement,4);
    if(memcmp(h->replacement,patch,9) || memcmp(h->target,patch,9) ||
        memcmp(h->trampoline,trampoline,14) ||
        VirtualQuery(h->trampoline,&memory,sizeof(memory))!=sizeof(memory) ||
        !(memory.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))) return FALSE;
    for(unsigned i=0;i<sizeof(test_temp_guards)/sizeof(*test_temp_guards);++i)
        if(!readable(base+test_temp_guards[i].rva,test_temp_guards[i].size) ||
            memcmp(base+test_temp_guards[i].rva,test_temp_guards[i].bytes,test_temp_guards[i].size)) return FALSE;
    return TRUE;
}
/* Under state_lock, on the established native thread. Positive result is a
 * fresh identity proof, not a retained pointer or a native task admission. */
static int test_temp_ready_locked(const char *zone,SudekiMpLanStoryNativeRoster *r,uint8_t **target) {
    if(!native_thread || native_thread!=GetCurrentThreadId()) return 0;
    if(!base || !installed || exhausted || foreign_thread || !test_temp_code_exact() ||
        !split_owner || !split_owner->test_ready) return -1;
    if(call_depth || split_active || split_transition ||
        observed.temporary[0] || published.temporary[0] || !last_dispatch_serial ||
        !roster_identity_locked(last_controller,&published,r) ||
        !SudekiMpLanStorySceneSame(&observed,&published)) return 0;
    uint8_t *world=r->world,*descriptor=r->descriptor;
    if(!readable(world,0x3a0) || !world[0x399] || !world[0x39a] || !world[0x39b] ||
        *(uint32_t *)(descriptor+0x34)!=3u || *(void **)(world+0x14)) return 0;
    unsigned count=*(unsigned *)(world+0x54);
    uint8_t *table=*(uint8_t **)(world+0x50);
    if(!count || count>512u || !readable(table,count*0x54u)) return -1;
    char name[64];uint32_t state;
    if(!temporary_descriptor(table,count,descriptor,&state,name) || state!=3) return -1;
    *target=NULL;
    for(unsigned i=0;i<count;++i) {
        uint8_t *d=table+i*0x54u;
        if(!temporary_descriptor(table,count,d,&state,name)) return -1;
        if(!strcmp(name,zone)) {
            if(*target) return -1;
            *target=d;
        }
    }
    if(!*target || *(uint32_t *)(*target+0x34)!=0u || *target==descriptor) return -1;
    uint8_t *actor=r->actors[r->leader_character],*position,*tracking;
    if(!readable(actor,0x98) || !readable(position=*(uint8_t **)(actor+0x44),0x5c) ||
        *(void **)position!=base+0x2cdefc ||
        !readable(tracking=*(uint8_t **)(actor+0x74),0x42) ||
        *(uint16_t *)(tracking+0x40)==0xffffu) return 0;
    for(unsigned i=0;i<3;++i)
        if((*(uint32_t *)(position+0x18+i*4)&0x7f800000u)==0x7f800000u ||
            (*(uint32_t *)(position+0x50+i*4)&0x7f800000u)==0x7f800000u) return 0;
    return split_owner->test_ready(zone,r->leader_character)?1:0;
}

int SudekiMpLanStoryObserverTestEnterTemporary(const char *zone,const char *marker) {
    char name[64],marker_check[64],authored[64];
    SudekiMpLanStoryNativeRoster before,after;
    uint8_t *target=NULL,*again=NULL;
    SudekiMpResourceName resource={0};
    if(!name_copy(name,zone) || !name_copy(marker_check,marker)) return -1;
    memcpy(authored,marker,strlen(marker_check)+1u); /* Preserve authored case. */
    AcquireSRWLockExclusive(&state_lock);
    int ready=test_temp_dispatched?-1:test_temp_ready_locked(name,&before,&target);
    ReleaseSRWLockExclusive(&state_lock);
    if(ready!=1) return ready;
    if(!SudekiMpCleanroomEngineResourceNameFromText(&resource,authored)) {
        SudekiMpLogWrite("story_test_temp event=refused reason=resource_constructor\r\n");
        return -1;
    }
    TemporaryResourceCopy resource_copy=temporary_resource_copy(&resource);
    if(!resource_copy.text_valid || strcmp(resource_copy.text,authored)) {
        SudekiMpCleanroomEngineReleaseResourceName(&resource);
        SudekiMpLogWrite("story_test_temp event=refused reason=resource_identity\r\n");
        return -1;
    }
    AcquireSRWLockExclusive(&state_lock);
    ready=test_temp_dispatched?-1:test_temp_ready_locked(name,&after,&again);
    if(ready==1 && (!same_native_roster(&before,&after) || target!=again)) ready=0;
    if(ready==1) {test_temp_required=TRUE;test_temp_admitted=FALSE;}
    ReleaseSRWLockExclusive(&state_lock);
    if(ready!=1) {
        SudekiMpCleanroomEngineReleaseResourceName(&resource);
        return ready;
    }
    /* Same adapter and same journal/split callbacks as the authored door.
     * Required admission suppresses vanilla fallback if its owner declines. */
    zone_temp(before.world,name,&resource);
    test_temp_required=FALSE;
    TemporaryWorldCopy result=temporary_world_copy(before.world);
    TemporaryResourceCopy retained=temporary_resource_copy((uint8_t *)before.world+0x44);
    BOOL accepted=test_temp_dispatched && result.exact && result.descriptor==before.descriptor &&
        result.pending==target && result.state==3u && !result.ready && !result.placed &&
        retained.text_valid && retained.identifier==resource_copy.identifier &&
        !strcmp(retained.text,authored) &&
        *(void **)((uint8_t *)before.world+0x4c)==resource.text_reference;
    /* 0x662D -> 0x4BC0 -> 0x1B9630 retains a distinct world reference.
     * Release this caller's reference; never retain a stack ResourceName. */
    SudekiMpCleanroomEngineReleaseResourceName(&resource);
    SudekiMpLogFormat("story_test_temp event=%s zone=%s marker=%s native_entered=%u result=%s retry=%s\r\n",
        test_temp_dispatched?"dispatched":"deferred",name,authored,test_temp_dispatched,
        accepted?"accepted_pending":"unknown",test_temp_dispatched?"forbidden":"allowed");
    return test_temp_dispatched?1:0;
}

BOOL SudekiMpLanStoryObserverTestTemporarySettled(const char *zone) {
    char name[64],current[64];uint32_t state;
    SudekiMpLanStoryNativeRoster roster;
    if(!name_copy(name,zone)) return FALSE;
    AcquireSRWLockShared(&state_lock);
    BOOL ok=test_temp_dispatched && split_active && !split_transition && !split_exiting &&
        !strcmp(observed.temporary,name) && !strcmp(published.temporary,name) &&
        roster_identity_locked(last_controller,&published,&roster);
    if(ok) {
        uint8_t *world=roster.world,*table=*(uint8_t **)(world+0x50);
        unsigned count=*(unsigned *)(world+0x54);
        ok=count && count<=512u && readable(table,count*0x54u) &&
            temporary_descriptor(table,count,roster.descriptor,&state,current) &&
            state==4u && !strcmp(current,name);
    }
    ReleaseSRWLockShared(&state_lock);
    return ok;
}

BOOL SudekiMpLanStoryObserverUninstall(void) {
    if(!base) return TRUE;
    AcquireSRWLockExclusive(&state_lock);
    if(call_depth || foreign_thread || (native_thread && native_thread!=GetCurrentThreadId()) ||
        remote_mask || visit_zone[0] || resident_house[0]) {
        /* A member visit owns native state: its owner drains first. */
        ReleaseSRWLockExclusive(&state_lock); SetLastError(ERROR_BUSY); return FALSE;
    }
    world_change_hook=NULL;
    BOOL ok=TRUE; DWORD error=ERROR_SUCCESS;
    for(unsigned n=HOOK_COUNT;n>0;--n) if(!SudekiMpRestoreInlineHook(&hooks[n-1])) {
        if(ok) error=GetLastError();
        ok=FALSE;
    }
    if(ok) {
        split_owner=NULL; split_active=split_transition=split_exiting=FALSE;
        memset(resident_house,0,sizeof(resident_house));
        remote_mask=0; remote_evict=NULL; remote_lead_entry=NULL; vanilla_transition=0; vanilla_transition_pending=FALSE; background_switch_tick=hold_started=hold_ended=0; holding=FALSE; local_transition=0;
        memset(visit_zone,0,sizeof(visit_zone)); split_began_call=local_began_call=FALSE;
        memset(last_temp_zone,0,sizeof(last_temp_zone)); memset(last_temp_marker,0,sizeof(last_temp_marker));
        base=NULL; installed=FALSE; native_thread=0; exhausted=FALSE;
        memset(&observed,0,sizeof(observed));
        memset(&published,0,sizeof(published));
        last_world=last_descriptor=last_group=last_controller=NULL;
        memset(last_actors,0,sizeof(last_actors)); last_exact=FALSE; last_logged_revision=0;
        last_dispatch_serial=0;
        test_temp_required=test_temp_admitted=test_temp_dispatched=FALSE;
        temporary_records=0; temporary_overflow_logged=FALSE;
    }
    ReleaseSRWLockExclusive(&state_lock);
    if(!ok) SetLastError(error);
    return ok;
}

BOOL SudekiMpLanStoryObserverSetRemoteInterior(const char *zone,uint8_t mask) {
    char name[64];
    if(!name_copy(name,zone)) return FALSE;
    AcquireSRWLockExclusive(&state_lock);
    BOOL joining=split_active && !split_transition && !split_exiting && !strcmp(observed.temporary,name);
    BOOL same_house=remote_mask && !strcmp(observed.temporary,name);
    /* Only the area fields may differ from the published scene (a second
     * member in the same pass, a pending revision): never the exterior. */
    BOOL ok=base && installed && !call_depth && !exhausted && !foreign_thread &&
        native_thread==GetCurrentThreadId() && (!split_active || joining) && !split_transition &&
        observed.phase==SUDEKIMP_LAN_STORY_READY && published.revision &&
        published.phase==SUDEKIMP_LAN_STORY_READY && observed.epoch==published.epoch &&
        !memcmp(observed.world,published.world,sizeof(observed.world)) &&
        observed.available_mask==published.available_mask && observed.leader_seat==published.leader_seat &&
        mask && !(mask&~15u) && !(mask&~observed.available_mask) &&
        observed.leader_seat<4u && !(mask&(1u<<observed.leader_seat)) &&
        (!observed.temporary[0] || joining || same_house);
    if(ok) {
        memcpy(observed.temporary,name,sizeof(name));
        remote_mask=(uint8_t)(joining||same_house?(remote_mask|mask):mask);
        observed.inside_mask=(uint8_t)(split_active?((1u<<observed.leader_seat)|remote_mask):remote_mask);
        set_retained();
        SudekiMpLogFormat("lan_story event=remote_interior_begin epoch=%lu temporary=%s added=%u members=%u inside=%u available=%u policy=exterior_epoch_retained\r\n",
            (unsigned long)observed.epoch,observed.temporary,mask,remote_mask,observed.inside_mask,observed.available_mask);
    }
    ReleaseSRWLockExclusive(&state_lock);
    return ok;
}
void SudekiMpLanStoryObserverClearRemoteInterior(void) {
    AcquireSRWLockExclusive(&state_lock);
    if(remote_mask) {
        SudekiMpLogFormat("lan_story event=remote_interior_end epoch=%lu temporary=%s inside=%u\r\n",
            (unsigned long)observed.epoch,observed.temporary,remote_mask);
        remote_mask=0;
        if(split_active) observed.inside_mask=(uint8_t)(observed.leader_seat<4u?1u<<observed.leader_seat:0u);
        else { memset(observed.temporary,0,sizeof(observed.temporary)); observed.inside_mask=0; }
        set_retained();
    }
    ReleaseSRWLockExclusive(&state_lock);
}
void SudekiMpLanStoryObserverSetRemoteInteriorEvict(void (*evict)(void)) { remote_evict=evict; }
void SudekiMpLanStoryObserverSetRemoteInteriorLeadEntry(void (*release)(const char *zone)) { remote_lead_entry=release; }
BOOL SudekiMpLanStoryObserverHolding(void) {
    AcquireSRWLockShared(&state_lock);
    /* Plus a short grace: consumers re-prove their own bindings on the
     * first exact samples after the native load settles. */
    BOOL h=holding || (hold_ended && GetTickCount()-hold_ended<1500u);
    ReleaseSRWLockShared(&state_lock);
    return h;
}
BOOL SudekiMpLanStoryObserverPublished(SudekiMpLanStoryScene *out) {
    if(!out) return FALSE;
    AcquireSRWLockShared(&state_lock);
    BOOL ok=base && installed && published.revision && published.phase==SUDEKIMP_LAN_STORY_READY &&
        SudekiMpLanStorySceneValid(&published);
    if(ok) *out=published;
    ReleaseSRWLockShared(&state_lock);
    return ok;
}
DWORD SudekiMpLanStoryObserverBackgroundSwitchTick(void) {
    AcquireSRWLockShared(&state_lock);
    DWORD tick=installed && native_thread==GetCurrentThreadId() && background_switch_tick &&
        GetTickCount()-background_switch_tick<20000u?background_switch_tick:0;
    ReleaseSRWLockShared(&state_lock);return tick;
}
BOOL SudekiMpLanStoryObserverSwitchMember(unsigned c,SudekiMpLanStorySwitchMember *out) {
    AcquireSRWLockShared(&state_lock);
    BOOL ok=FALSE;
    if(out && c<4u && base && installed && !foreign_thread && native_thread==GetCurrentThreadId() &&
        readable(base+0x408d10u,4u) && *(void **)(base+0x408d10u)==last_world) {
        uint8_t *actor=last_actors[c],*ai,*position,*mode;
        if(readable(actor,0x98u) && readable(ai=*(uint8_t **)(actor+0x94u),0x16cu) &&
            *(void **)(ai+0x10u)==actor && readable(position=*(uint8_t **)(actor+0x44u),0x24u) &&
            *(void **)(position+0x10u)==actor && readable(mode=*(uint8_t **)(ai+0x3cu),0xcu)) {
            memcpy(out->position,position+0x18u,sizeof(out->position));
            out->pause_refs=actor[0x2bu];out->ai_override=*(int16_t *)(ai+0x16au);out->ai_mode=mode[0xbu];ok=TRUE;
        }
    }
    ReleaseSRWLockShared(&state_lock);return ok;
}
BOOL SudekiMpLanStoryObserverIdentityUnknown(void) {
    AcquireSRWLockShared(&state_lock);
    BOOL vanilla=installed && vanilla_transition_pending && observed.phase!=SUDEKIMP_LAN_STORY_READY;
    BOOL recent=vanilla && GetTickCount()-vanilla_transition<20000u;
    BOOL background=installed && background_switch_tick && GetTickCount()-background_switch_tick<5000u;
    ReleaseSRWLockShared(&state_lock);
    /* EXIT looks like a world load after clearing the temporary name. Its
     * general loading witness must not bypass this transition's deadline. */
    if(vanilla) return recent;
    return background || SudekiMpLanStoryObserverWorldLoading() || SudekiMpLanStoryObserverHolding();
}

BOOL SudekiMpLanStoryObserverSetInsideMembership(uint32_t epoch,uint32_t revision,uint8_t mask) {
    AcquireSRWLockExclusive(&state_lock);
    BOOL ok=installed && native_thread==GetCurrentThreadId() && !call_depth && !split_active && !remote_mask &&
        observed.phase==SUDEKIMP_LAN_STORY_READY && observed.temporary[0] && observed.leader_seat<4u &&
        epoch==observed.epoch && revision==observed.revision && !(mask&~observed.available_mask) &&
        (mask&(1u<<observed.leader_seat));
    if(ok && (observed.inside_mask|mask)!=observed.inside_mask) {
        observed.inside_mask|=mask; increment(&observed.revision); published=observed;
    }
    ReleaseSRWLockExclusive(&state_lock); return ok;
}
void SudekiMpLanStoryObserverNoteResidentHouse(const char *zone) {
    AcquireSRWLockExclusive(&state_lock);
    memset(resident_house,0,sizeof(resident_house));
    if(zone && !name_copy(resident_house,zone)) memset(resident_house,0,sizeof(resident_house));
    ReleaseSRWLockExclusive(&state_lock);
}
void SudekiMpLanStoryObserverClearRemoteMembers(uint8_t mask) {
    AcquireSRWLockExclusive(&state_lock);
    uint8_t left=(uint8_t)(remote_mask&~mask);
    if(remote_mask && left!=remote_mask) {
        SudekiMpLogFormat("lan_story event=remote_interior_members_left epoch=%lu temporary=%s left=%u remaining=%u\r\n",
            (unsigned long)observed.epoch,observed.temporary,(unsigned)(remote_mask&mask),(unsigned)left);
        remote_mask=left;
        if(split_active) observed.inside_mask=(uint8_t)((observed.leader_seat<4u?1u<<observed.leader_seat:0u)|left);
        else if(!left) { memset(observed.temporary,0,sizeof(observed.temporary)); observed.inside_mask=0; }
        else observed.inside_mask=left;
        set_retained();
    }
    ReleaseSRWLockExclusive(&state_lock);
}
void SudekiMpLanStoryObserverSetRemoteVisit(const char *zone) {
    AcquireSRWLockExclusive(&state_lock);
    memset(visit_zone,0,sizeof(visit_zone));
    if(zone && !name_copy(visit_zone,zone)) memset(visit_zone,0,sizeof(visit_zone));
    ReleaseSRWLockExclusive(&state_lock);
}
BOOL SudekiMpLanStoryObserverRemoteHouseIs(const char *zone) {
    char name[64];
    if(!zone || !name_copy(name,zone)) return FALSE;
    AcquireSRWLockShared(&state_lock);
    BOOL is=(visit_zone[0] && !strcmp(visit_zone,name)) || (resident_house[0] && !strcmp(resident_house,name));
    ReleaseSRWLockShared(&state_lock);
    return is;
}
