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
static const SudekiMpLanStoryObserverSplit *split_owner;
static BOOL split_active,split_transition,split_exiting;
static unsigned last_logged_revision;
static uint64_t last_dispatch_serial;
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
    observed.available_mask=0; observed.inside_mask=0;
    observed.leader_seat=SUDEKIMP_LAN_STORY_NO_SEAT;
}
static uint32_t begin_zone(unsigned kind,const char *name) {
    AcquireSRWLockExclusive(&state_lock);
    DWORD thread=GetCurrentThreadId();
    if(native_thread && native_thread!=thread) foreign_thread=TRUE;
    else native_thread=thread;
    ++call_depth;
    uint32_t source_epoch=observed.epoch;
    /* Split-area travel keeps the exterior epoch: only the lead's area moves.
     * Anything else (or a declined/unknown request) is vanilla whole-party. */
    if(kind==TEMP && split_owner && !split_active && call_depth==1u &&
        published.phase==SUDEKIMP_LAN_STORY_READY && published.revision &&
        observed.phase==SUDEKIMP_LAN_STORY_READY && !observed.temporary[0] &&
        observed.leader_seat<4u && (observed.available_mask&~(1u<<observed.leader_seat)) &&
        name && split_owner->begin_temp(name,observed.leader_seat)) {
        char copy[64];
        if(name_copy(copy,name)) {
            memcpy(observed.temporary,copy,sizeof(copy));
            observed.inside_mask=(uint8_t)(1u<<observed.leader_seat);
            split_active=split_transition=TRUE; split_exiting=FALSE;
            SudekiMpLogFormat("lan_story event=split_begin epoch=%lu temporary=%s inside=%u available=%u policy=exterior_epoch_retained\r\n",
                (unsigned long)observed.epoch,observed.temporary,observed.inside_mask,observed.available_mask);
            ReleaseSRWLockExclusive(&state_lock);
            return source_epoch;
        }
        if(split_owner->ended) split_owner->ended(TRUE);
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
static void __cdecl zone_set(const char *s) { begin_zone(SET,s); ((ZoneCall)hooks[SET].trampoline)(s); end_zone(); }
static void __cdecl zone_enter(const char *s) { begin_zone(ENTER,s); ((ZoneCall)hooks[ENTER].trampoline)(s); end_zone(); }
static void __cdecl zone_switch(const char *s) { begin_zone(SWITCH,s); ((ZoneCall)hooks[SWITCH].trampoline)(s); end_zone(); }
static void __cdecl zone_load(const char *s) { begin_zone(LOAD,s); ((ZoneCall)hooks[LOAD].trampoline)(s); end_zone(); }
static void __attribute__((thiscall)) zone_main(void *w,const char *s) { begin_zone(MAIN,s); ((MainCall)hooks[MAIN].trampoline)(w,s); end_zone(); }
static void __attribute__((thiscall)) zone_temp(void *w,const char *s,const void *r) {
    TemporaryJournal journal;
    uint32_t source_epoch=begin_zone(TEMP,s);
    temporary_journal_begin(&journal,TEMP,source_epoch,w,s,r);
    ((TempCall)hooks[TEMP].trampoline)(w,s,r);
    temporary_journal_end(&journal,w); end_zone();
}
static void __attribute__((thiscall)) zone_exit(void *w) {
    TemporaryJournal journal;
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
    if(split_transition && !exact) {
        /* Native lead travel is settling. The exterior and its other
         * occupants are unchanged, so keep publishing the READY split scene;
         * roster consumers still fail closed until native exactness returns. */
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
     * exterior and TEMP; it is not a world replacement. */
    BOOL replaced=last_exact && (!exact || world!=last_world ||
        (!split_active && descriptor!=last_descriptor) ||
        group!=last_group || controller!=last_controller || memcmp(found,last_actors,sizeof(found)));
    if(replaced) {
        increment(&observed.epoch);
        if(split_active) {
            split_active=split_transition=split_exiting=FALSE;
            if(split_owner && split_owner->ended) split_owner->ended(TRUE);
            memset(observed.temporary,0,sizeof(observed.temporary));
            SudekiMpLogFormat("lan_story event=split_abandoned kind=replaced policy=vanilla_epoch\r\n");
        }
    }
    if(exact) {
        observed.phase=SUDEKIMP_LAN_STORY_READY;
        observed.available_mask=mask; observed.leader_seat=lead;
        /* Whole native party shares the host's current area unless a split
         * owner moved only the lead. */
        observed.inside_mask=(uint8_t)(!observed.temporary[0]?0u:
            split_active?(1u<<lead):mask);
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
        *(void **)(world+0x14u) || !world[0x399u] || !world[0x39au] ||
        !SudekiMpCleanroomEngineWorldReady()) return FALSE;
    uint8_t *descriptor=*(uint8_t **)(world+0x0cu);
    if(descriptor!=last_descriptor || !readable(descriptor,0x38u) ||
        *(uint32_t *)(descriptor+0x34u)!=(scene->temporary[0]?4u:3u)) return FALSE;
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
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *r) {
    SudekiMpLanStoryNativeRoster fresh;
    if(!r || !r->dispatch_serial) return FALSE;
    AcquireSRWLockShared(&state_lock);
    BOOL ok=roster_identity_locked(r->controller,&published,&fresh) &&
        (same_native_roster(r,&fresh) || same_native_identity(r,&fresh));
    if(!ok) ok=split_hold_identity_locked(r);
    ReleaseSRWLockShared(&state_lock); return ok;
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
        split_owner=NULL; split_active=split_transition=split_exiting=FALSE;
        base=NULL; installed=FALSE; native_thread=0; exhausted=FALSE;
        memset(&observed,0,sizeof(observed));
        memset(&published,0,sizeof(published));
        last_world=last_descriptor=last_group=last_controller=NULL;
        memset(last_actors,0,sizeof(last_actors)); last_exact=FALSE; last_logged_revision=0;
        last_dispatch_serial=0;
        temporary_records=0; temporary_overflow_logged=FALSE;
    }
    ReleaseSRWLockExclusive(&state_lock);
    if(!ok) SetLastError(error);
    return ok;
}
