#include "hooks/lan_story_area_follow.h"

#include "engine/build_identity.h"
#include "engine/log.h"
#include "hooks/lan_story_spawn_trace.h"
#include "hooks/lan_story_sky.h"
#include "hooks/lan_story_client.h"

#include <string.h>

enum {
    RVA_WORLD_GLOBAL=0x408d10u,
    RVA_SWITCH_ZONE_NOW=0x7990u,   /* ?SwitchZoneNOW@@YAXPBD@Z (cdecl) */
    RVA_ENTER_ZONE=0x7970u,        /* ?EnterZone@@YAXPBD@Z (cdecl) */
    DESC_STRIDE=0x54u, DESC_NAME=0x24u, DESC_STATE=0x34u,
    WORLD_CURRENT=0x0cu, WORLD_LOADED=0x10u, WORLD_PENDING=0x14u,
    WORLD_TABLE=0x50u, WORLD_TABLE_COUNT=0x54u, WORLD_TARGET=0x394u, WORLD_ARMED=0x39bu,
    RETRY_MS=750u
};
typedef void (__cdecl *ZoneCall)(const char *);

/* Exact entries (no relocations): MOV EAX,[ESP+4]; PUSH ESI ... */
static const uint8_t switch_entry[5]={0x8b,0x44,0x24,0x04,0x56};
static const uint8_t enter_entry[5]={0x8b,0x44,0x24,0x04,0x56};

static uint8_t *base;
static volatile LONG calling;
static uint32_t last_switch_at,last_enter_at;
static char last_switch[SUDEKIMP_STORY_AREA_NAME],last_enter[SUDEKIMP_STORY_AREA_NAME];
static unsigned logs;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL name_of(const uint8_t *desc,char out[SUDEKIMP_STORY_AREA_NAME]) {
    const char *name;
    memset(out,0,SUDEKIMP_STORY_AREA_NAME);
    if(!desc) return TRUE;
    if(!readable(desc,DESC_STRIDE) || !readable(name=*(const char *const *)(desc+DESC_NAME),1u)) return FALSE;
    BOOL span=readable(name,SUDEKIMP_STORY_AREA_NAME);
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_NAME;++i) {
        if(!span && !readable(name+i,1u)) return FALSE;
        char c=name[i];
        if(!c) return i>0u;
        if(c<0x20 || c>0x7e || i+1u==SUDEKIMP_STORY_AREA_NAME) return FALSE;
        out[i]=c;
    }
    return FALSE;
}
static uint8_t *world_now(unsigned *count,uint8_t **table) {
    uint8_t *world;
    if(!base || !readable(base+RVA_WORLD_GLOBAL,4u)) return NULL;
    world=*(uint8_t **)(base+RVA_WORLD_GLOBAL);
    if(!readable(world,0x3a0u)) return NULL;
    *count=*(unsigned *)(world+WORLD_TABLE_COUNT); *table=*(uint8_t **)(world+WORLD_TABLE);
    if(!*count || *count>512u || !readable(*table,*count*DESC_STRIDE)) return NULL;
    return world;
}
static BOOL in_table(const uint8_t *desc,const uint8_t *table,unsigned count) {
    return !desc || (desc>=table && desc<table+count*DESC_STRIDE && !((size_t)(desc-table)%DESC_STRIDE));
}
/* Descriptor by authored name in the world table (case-insensitive, as native lookup). */
static uint8_t *find(const char *name,uint8_t *table,unsigned count) {
    char n[SUDEKIMP_STORY_AREA_NAME];
    for(unsigned i=0;i<count;++i) {
        uint8_t *d=table+i*DESC_STRIDE;
        if(name_of(d,n) && n[0] && !_stricmp(n,name)) return d;
    }
    return NULL;
}

BOOL SudekiMpLanStoryAreaFollowInitialize(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!b || !SudekiMpCheckLoadedExecutable(image) ||
        memcmp(b+RVA_SWITCH_ZONE_NOW,switch_entry,sizeof(switch_entry)) ||
        memcmp(b+RVA_ENTER_ZONE,enter_entry,sizeof(enter_entry))) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    base=b; return TRUE;
}

/* Latest native TEMP destination (house). Only the loading window matters:
 * once the house is current (state 4) the report is held anyway, and a
 * zero-state exit target already grants no load on the client. */
static char temporary_latest[SUDEKIMP_STORY_AREA_NAME];
static unsigned withheld_logs;
void SudekiMpLanStoryAreaNoteTemporary(const char *name) {
    memset(temporary_latest,0,sizeof(temporary_latest));
    if(!name) return;
    for(unsigned i=0;;++i) {
        if(i+1u>=SUDEKIMP_STORY_AREA_NAME || !readable(name+i,1u) ||
            (name[i] && (name[i]<0x20 || name[i]>0x7e))) {
            memset(temporary_latest,0,sizeof(temporary_latest)); return;
        }
        if(!(temporary_latest[i]=name[i])) return;
    }
}
BOOL SudekiMpLanStoryAreaCapture(SudekiMpStoryAreaState *out) {
    unsigned count; uint8_t *table,*world=world_now(&count,&table);
    if(!out || !world) return FALSE;
    memset(out,0,sizeof(*out)); out->observed_tick=GetTickCount();
    uint8_t *current=*(uint8_t **)(world+WORLD_CURRENT),*target=*(uint8_t **)(world+WORLD_TARGET);
    if(!in_table(current,table,count) || !in_table(target,table,count) ||
        !name_of(current,out->current) || !name_of(target,out->target)) return FALSE;
    if(out->target[0] && temporary_latest[0] && !_stricmp(out->target,temporary_latest)) {
        if(withheld_logs<16u) { ++withheld_logs; SudekiMpLogFormat("story_area_capture event=target_withheld zone=%s reason=temporary_interior\r\n",out->target); }
        memset(out->target,0,sizeof(out->target));
    }
    for(unsigned i=0;i<count && out->count<SUDEKIMP_STORY_AREA_MAX;++i) {
        uint8_t *d=table+i*DESC_STRIDE; uint32_t state=*(uint32_t *)(d+DESC_STATE);
        if(!state) continue;
        if(state>4u || !name_of(d,out->entries[out->count].name)) return FALSE;
        out->entries[out->count++].state=(uint8_t)state;
    }
    return SudekiMpStoryAreaStateValid(out);
}

static uint32_t last_call_at;
uint32_t SudekiMpLanStoryAreaFollowLastCall(void) { return last_call_at; }
static void call_zone(unsigned rva,const char *name) {
    /* Return the old area's lighting before native residency can replace it. */
    if(!SudekiMpLanStorySkyReleaseLighting()) return;
    last_call_at=GetTickCount(); if(!last_call_at) last_call_at=1u;
    InterlockedExchange(&calling,1);
    ((ZoneCall)(base+rva))(name);
    InterlockedExchange(&calling,0);
}
BOOL SudekiMpLanStoryAreaFollowCalling(void) { return InterlockedCompareExchange(&calling,0,0)!=0; }

static BOOL replay_spawn(void *context) {
    const SudekiMpStorySpawnGroup *group=context;
    InterlockedExchange(&calling,1);
    BOOL entered=SudekiMpLanStorySpawnReplay(group);
    InterlockedExchange(&calling,0);
    return entered;
}
void SudekiMpLanStoryAreaFollow(const SudekiMpStoryAreaState *host,uint32_t now,
    const SudekiMpControlUpdateDispatchWitness *w) {
    (void)w;
    unsigned count; uint8_t *table,*world=world_now(&count,&table);
    if(!world || !SudekiMpStoryAreaStateExterior(host) || !*(world+WORLD_ARMED)) return;
    uint8_t *current=*(uint8_t **)(world+WORLD_CURRENT),*target=*(uint8_t **)(world+WORLD_TARGET);
    char mine_current[SUDEKIMP_STORY_AREA_NAME],mine_target[SUDEKIMP_STORY_AREA_NAME];
    if(!in_table(current,table,count) || !in_table(target,table,count) ||
        !name_of(current,mine_current) || !name_of(target,mine_target) || !mine_current[0] ||
        *(uint32_t *)(current+DESC_STATE)!=3u) return;
    /* Temporary travel belongs to the per-character transition owner. Never
     * background-load a host interior for a character left outside, or pull
     * a locally admitted interior view back out with an ordinary EnterZone. */
    /* Exiting a temporary interior leaves WORLD_TARGET naming that house
     * after its state has returned to zero. Capture omits zero-state entries.
     * A target name alone must not start a new client load of the old house. */
    BOOL target_active=FALSE;
    for(unsigned i=0;i<host->count;++i)
        if(host->entries[i].state>=1u && host->entries[i].state<=3u &&
            !_stricmp(host->entries[i].name,host->target)) {target_active=TRUE;break;}
    /* 1. Follow only a positively loading/resident host background target.
     * SwitchZoneNOW also unloads our previous target. */
    if(target_active && _stricmp(host->target,mine_target)) {
        uint8_t *d=find(host->target,table,count);
        if(d && !*(uint32_t *)(d+DESC_STATE) && d!=current &&
            (_stricmp(last_switch,host->target) || now-last_switch_at>=RETRY_MS)) {
            memcpy(last_switch,host->target,sizeof(last_switch)); last_switch_at=now;
            if(logs<64u) { ++logs; SudekiMpLogFormat("story_area_follow event=switch target=%s from_target=%s current=%s\r\n",
                host->target,mine_target[0]?mine_target:"-",mine_current); }
            call_zone(RVA_SWITCH_ZONE_NOW,host->target);
            return;
        }
    }
    /* 2. Current zone: once the host's current zone is resident here, mark it
     * loaded; the native world update promotes it as it would on the host. */
    if(_stricmp(host->current,mine_current) && !*(void **)(world+WORLD_PENDING)) {
        uint8_t *d=find(host->current,table,count);
        uint32_t state=d?*(uint32_t *)(d+DESC_STATE):0u;
        /* The host's current zone is not resident here (e.g. the host jumped
         * or the report started after its background target moved on):
         * background-load it first with the same native request. */
        if(d && !state && d!=current && *(uint8_t **)(world+WORLD_TARGET)!=d &&
            (_stricmp(last_switch,host->current) || now-last_switch_at>=RETRY_MS)) {
            memcpy(last_switch,host->current,sizeof(last_switch)); last_switch_at=now;
            if(logs<64u) { ++logs; SudekiMpLogFormat("story_area_follow event=switch target=%s reason=host_current_not_resident current=%s\r\n",
                host->current,mine_current); }
            call_zone(RVA_SWITCH_ZONE_NOW,host->current);
            return;
        }
        if(d && (state==2u || state==3u) && *(uint8_t **)(world+WORLD_LOADED)!=d &&
            (_stricmp(last_enter,host->current) || now-last_enter_at>=RETRY_MS)) {
            memcpy(last_enter,host->current,sizeof(last_enter)); last_enter_at=now;
            if(logs<64u) { ++logs; SudekiMpLogFormat("story_area_follow event=enter zone=%s from=%s state=%lu\r\n",
                host->current,mine_current,(unsigned long)state); }
            call_zone(RVA_ENTER_ZONE,host->current);
        }
    }
}

int SudekiMpLanStoryAreaGoTo(const char *zone,uint32_t now) {
    unsigned count; uint8_t *table,*world=world_now(&count,&table);
    static uint32_t last_at; static unsigned step_logs;
    if(!zone || !zone[0] || !world || !*(world+WORLD_ARMED)) return 0;
    uint8_t *d=find(zone,table,count);
    if(!d) return 0;
    if(*(uint8_t **)(world+WORLD_CURRENT)==d) return 2;
    if(last_at && now-last_at<RETRY_MS) return 1;
    uint32_t state=*(uint32_t *)(d+DESC_STATE);
    if(!state && *(uint8_t **)(world+WORLD_TARGET)!=d) {
        last_at=now?now:1u;
        if(step_logs<16u) { ++step_logs; SudekiMpLogFormat("story_area_goto event=switch zone=%s\r\n",zone); }
        call_zone(RVA_SWITCH_ZONE_NOW,zone); return 1;
    }
    if((state==2u || state==3u) && !*(void **)(world+WORLD_PENDING) && *(uint8_t **)(world+WORLD_LOADED)!=d) {
        last_at=now?now:1u;
        if(step_logs<16u) { ++step_logs; SudekiMpLogFormat("story_area_goto event=enter zone=%s\r\n",zone); }
        call_zone(RVA_ENTER_ZONE,zone); return 1;
    }
    return 1;
}

/* Research (H1b, [StoryAreas] ResearchBackgroundZone): once, on the host,
 * background-load one named zone with the native SwitchZoneNOW while the
 * current zone stays as it is, then log its native residency once a second.
 * The question is whether a house descriptor reaches resident state 2
 * without becoming current. Observation only after the one native call. */
static char research_zone[SUDEKIMP_STORY_AREA_NAME];
static uint32_t research_first,research_issued,research_logged;
static unsigned research_logs;
void SudekiMpLanStoryAreaSetResearchZone(const char *zone) {
    memset(research_zone,0,sizeof(research_zone));
    if(zone) strncpy(research_zone,zone,sizeof(research_zone)-1u);
    research_first=research_issued=research_logged=0; research_logs=0;
}
void SudekiMpLanStoryAreaResearchService(uint32_t now) {
    if(!research_zone[0] || research_logs>=120u) return;
    unsigned count; uint8_t *table,*world=world_now(&count,&table);
    if(!world || !*(world+WORLD_ARMED)) return;
    uint8_t *d=find(research_zone,table,count);
    if(!d) { if(!research_logs++) SudekiMpLogFormat("story_area_research event=unknown_zone zone=%s\r\n",research_zone); return; }
    if(!research_first) { research_first=now?now:1u; return; }
    if(!research_issued && now-research_first>8000u) {
        uint32_t state=*(uint32_t *)(d+DESC_STATE);
        uint8_t *current=*(uint8_t **)(world+WORLD_CURRENT);
        if(state || current==d || *(void **)(world+WORLD_PENDING) || *(uint8_t **)(world+WORLD_TARGET)==d) {
            ++research_logs; research_issued=now?now:1u;
            SudekiMpLogFormat("story_area_research event=skipped zone=%s state=%lu current=%u pending=%u\r\n",research_zone,
                (unsigned long)state,(unsigned)(current==d),(unsigned)(*(void **)(world+WORLD_PENDING)!=NULL));
            return;
        }
        research_issued=now?now:1u;
        SudekiMpLogFormat("story_area_research event=switch_zone_now zone=%s\r\n",research_zone);
        call_zone(RVA_SWITCH_ZONE_NOW,research_zone);
    }
    if(research_issued && now-research_logged>=1000u) {
        research_logged=now?now:1u; ++research_logs;
        uint8_t *current=*(uint8_t **)(world+WORLD_CURRENT),*target=*(uint8_t **)(world+WORLD_TARGET);
        char cn[SUDEKIMP_STORY_AREA_NAME]="-",tn[SUDEKIMP_STORY_AREA_NAME]="-";
        if(current) (void)name_of(current,cn);
        if(target) (void)name_of(target,tn);
        SudekiMpLogFormat("story_area_research event=state zone=%s state=%lu req30=%lu links=%lu current=%s target=%s pending=%u t=%lu\r\n",
            research_zone,(unsigned long)*(uint32_t *)(d+DESC_STATE),(unsigned long)*(uint32_t *)(d+0x30u),
            (unsigned long)*(uint32_t *)(d+0x3cu),cn,tn,(unsigned)(*(void **)(world+WORLD_PENDING)!=NULL),
            (unsigned long)(now-research_issued));
    }
}
/* Research (H1b client, [StoryAreas] ResearchEnterTemporary=ZONE,MARKER):
 * once, on the client, enter that house locally with the native
 * CWorld::EnterTemporaryZone(name, ResourceName marker) (RVA 0x64B0,
 * thiscall), as the authored interior helper does; then log the client's
 * current zone. While armed the ordinary follower stands down (it would
 * return the client to the host's current zone). */
enum { RVA_ENTER_TEMPORARY=0x64b0u };
static char research_temp_zone[SUDEKIMP_STORY_AREA_NAME],research_temp_marker[64];
static uint32_t research_temp_first,research_temp_done,research_temp_logged; static unsigned research_temp_logs;
void SudekiMpLanStoryAreaSetResearchTemporary(const char *zone,const char *marker) {
    memset(research_temp_zone,0,sizeof(research_temp_zone)); memset(research_temp_marker,0,sizeof(research_temp_marker));
    if(zone && marker) { strncpy(research_temp_zone,zone,sizeof(research_temp_zone)-1u); strncpy(research_temp_marker,marker,sizeof(research_temp_marker)-1u); }
    research_temp_first=research_temp_done=research_temp_logged=0; research_temp_logs=0;
}
BOOL SudekiMpLanStoryAreaResearchTemporaryArmed(void) { return research_temp_zone[0]!=0; }
typedef void (__attribute__((thiscall)) *EnterTemporary)(void *,const char *,const void *);
typedef void (__attribute__((thiscall)) *ExitTemporary)(void *);
enum { RVA_EXIT_TEMPORARY=0x6710u };
/* The story observer owns inline hooks on both temporary exports (one
 * adapter per seam); calling through them is the intended path. Accept the
 * native entry bytes or a JMP into this DLL. */
static BOOL temporary_entry_ok(unsigned rva,const uint8_t *entry,size_t size) {
    if(readable(base+rva,size) && !memcmp(base+rva,entry,size)) return TRUE;
    if(!readable(base+rva,5u) || base[rva]!=0xe9u) return FALSE;
    int32_t rel; memcpy(&rel,base+rva+1u,4u);
    const void *target=base+rva+5u+rel; HMODULE self=NULL,owner=NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)(uintptr_t)&temporary_entry_ok,&self);
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)target,&owner);
    return self && owner==self;
}
static const char *enter_temporary_zone;
static void enter_temporary_use(void *resource,void *context) {
    ((EnterTemporary)(base+RVA_ENTER_TEMPORARY))(context,enter_temporary_zone,resource);
}
int SudekiMpLanStoryAreaEnterTemporary(const char *zone,const char *marker) {
    static const uint8_t entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x24};
    unsigned count; uint8_t *table,*world=world_now(&count,&table);
    if(!zone || !marker || !world || !*(world+WORLD_ARMED) || *(void **)(world+WORLD_PENDING)) return 0;
    uint8_t *d=find(zone,table,count);
    /* Native entry refuses a target that is not unloaded (0x64E3). */
    if(!d || *(uint32_t *)(d+DESC_STATE) || *(uint8_t **)(world+WORLD_CURRENT)==d ||
        !temporary_entry_ok(RVA_ENTER_TEMPORARY,entry,sizeof(entry))) return -1;
    if(!SudekiMpLanStorySkyReleaseLighting()) return 0;
    last_call_at=GetTickCount(); if(!last_call_at) last_call_at=1u;
    enter_temporary_zone=zone;
    InterlockedExchange(&calling,1);
    BOOL ok=SudekiMpLanStoryWithResourceName(marker,0x35u,enter_temporary_use,world);
    InterlockedExchange(&calling,0);
    enter_temporary_zone=NULL;
    return ok?1:-1;
}
int SudekiMpLanStoryAreaExitTemporary(void) {
    static const uint8_t entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x81,0xec,0x84,0,0,0};
    unsigned count; uint8_t *table,*world=world_now(&count,&table);
    if(!world || !*(world+WORLD_ARMED) || *(void **)(world+WORLD_PENDING)) return 0;
    uint8_t *current=*(uint8_t **)(world+WORLD_CURRENT);
    if(!in_table(current,table,count) || !current || *(uint32_t *)(current+DESC_STATE)!=4u ||
        !temporary_entry_ok(RVA_EXIT_TEMPORARY,entry,sizeof(entry))) return -1;
    if(!SudekiMpLanStorySkyReleaseLighting()) return 0;
    last_call_at=GetTickCount(); if(!last_call_at) last_call_at=1u;
    InterlockedExchange(&calling,1);
    ((ExitTemporary)(base+RVA_EXIT_TEMPORARY))(world);
    InterlockedExchange(&calling,0);
    return 1;
}
/* ?UnloadZone@@YAXPBD@Z (RVA 0x7BB0, cdecl): state setter 0x50AD70 with
 * state 0 on the named descriptor. Bytes: mov eax,[esp+4]; mov ecx,[world]. */
enum { RVA_UNLOAD_ZONE=0x7bb0u };
/* -1 refused, 0 not now, 1 already unloaded, 2 ready to unload. */
static int unload_check(const char *zone) {
    static const uint8_t entry[6]={0x8b,0x44,0x24,0x04,0x8b,0x0d};
    unsigned count; uint8_t *table,*world=world_now(&count,&table);
    if(!zone || !world || !readable(base+RVA_UNLOAD_ZONE,10u) || memcmp(base+RVA_UNLOAD_ZONE,entry,sizeof(entry)) ||
        *(uint8_t **)(base+RVA_UNLOAD_ZONE+6u)!=base+RVA_WORLD_GLOBAL) return -1;
    uint8_t *d=find(zone,table,count);
    if(!d) return -1;
    uint32_t state=*(uint32_t *)(d+DESC_STATE);
    /* Only a resident, non-current, non-pending house. */
    if(state!=2u || *(uint8_t **)(world+WORLD_CURRENT)==d || *(uint8_t **)(world+WORLD_LOADED)==d ||
        *(uint8_t **)(world+WORLD_PENDING)==d) return state?0:1;
    return 2;
}
BOOL SudekiMpLanStoryAreaResidentUnloadable(const char *zone) { return unload_check(zone)==2; }
int SudekiMpLanStoryAreaUnloadResident(const char *zone) {
    int check=unload_check(zone);
    if(check!=2) return check;
    SudekiMpLogFormat("story_area event=unload_resident zone=%s\r\n",zone);
    ((ZoneCall)(base+RVA_UNLOAD_ZONE))(zone);
    return 1;
}
int SudekiMpLanStoryAreaBackgroundLoad(const char *zone) {
    unsigned count; uint8_t *table,*world=world_now(&count,&table);
    if(!zone || !world || !*(world+WORLD_ARMED)) return -1;
    uint8_t *d=find(zone,table,count);
    if(!d) return -1;
    uint32_t state=*(uint32_t *)(d+DESC_STATE);
    if(state) return (int)state;
    uint8_t *current=*(uint8_t **)(world+WORLD_CURRENT),*target=*(uint8_t **)(world+WORLD_TARGET);
    if(current==d || *(void **)(world+WORLD_PENDING) || !in_table(target,table,count)) return -1;
    /* SwitchZoneNOW unloads the previous background target (and its
     * links): never take the host's own streaming target, loading or
     * resident. Fail closed; the visit is refused. */
    if(target && target!=d && *(uint32_t *)(target+DESC_STATE)!=0u) {
        char busy[SUDEKIMP_STORY_AREA_NAME]="-"; (void)name_of(target,busy);
        SudekiMpLogFormat("story_area event=background_load_refused zone=%s reason=streaming_target_in_use target=%s state=%lu\r\n",
            zone,busy,(unsigned long)*(uint32_t *)(target+DESC_STATE));
        return -1;
    }
    char previous[SUDEKIMP_STORY_AREA_NAME]="-";
    if(target) (void)name_of(target,previous);
    /* A house loaded for a remote character is not a background target the
     * client should follow (it enters that house itself, which needs it
     * unloaded). */
    SudekiMpLanStoryAreaNoteTemporary(zone);
    SudekiMpLogFormat("story_area event=background_load zone=%s previous_target=%s previous_state=%lu\r\n",
        zone,previous,target?(unsigned long)*(uint32_t *)(target+DESC_STATE):0ul);
    call_zone(RVA_SWITCH_ZONE_NOW,zone);
    return (int)*(uint32_t *)(d+DESC_STATE);
}
void SudekiMpLanStoryAreaResearchClientService(uint32_t now) {
    if(!research_temp_zone[0] || research_temp_logs>=60u) return;
    unsigned count; uint8_t *table,*world=world_now(&count,&table);
    if(!world || !*(world+WORLD_ARMED)) return;
    if(!research_temp_first) { research_temp_first=now?now:1u; return; }
    if(!research_temp_done && now-research_temp_first>12000u) {
        research_temp_done=now?now:1u;
        SudekiMpLogFormat("story_area_research event=client_enter_temporary zone=%s marker=%s\r\n",research_temp_zone,research_temp_marker);
        int r=SudekiMpLanStoryAreaEnterTemporary(research_temp_zone,research_temp_marker);
        if(!r) { research_temp_done=0; return; }
        SudekiMpLogFormat("story_area_research event=client_enter_returned ok=%u\r\n",(unsigned)(r>0));
    }
    if(research_temp_done && now-research_temp_logged>=1000u) {
        research_temp_logged=now?now:1u; ++research_temp_logs;
        uint8_t *current=*(uint8_t **)(world+WORLD_CURRENT); char cn[SUDEKIMP_STORY_AREA_NAME]="-";
        if(current) (void)name_of(current,cn);
        SudekiMpLogFormat("story_area_research event=client_state current=%s state=%lu pending=%u t=%lu\r\n",cn,
            current?(unsigned long)*(uint32_t *)(current+DESC_STATE):0ul,(unsigned)(*(void **)(world+WORLD_PENDING)!=NULL),
            (unsigned long)(now-research_temp_done));
    }
}
int SudekiMpLanStoryAreaZoneState(const char *zone) {
    unsigned count; uint8_t *table,*world=world_now(&count,&table);
    uint8_t *d=world && zone && zone[0]?find(zone,table,count):NULL;
    if(!d) return -1;
    return *(uint8_t **)(world+WORLD_CURRENT)==d?3:(int)*(uint32_t *)(d+DESC_STATE);
}

BOOL SudekiMpLanStoryEffectsCapture(SudekiMpStoryEffectState *out) {
    unsigned count; uint8_t *table,*world=world_now(&count,&table);
    if(!out || !world) return FALSE;
    uint8_t *current=*(uint8_t **)(world+WORLD_CURRENT);
    memset(out,0,sizeof(*out)); out->observed_tick=GetTickCount();
    if(!in_table(current,table,count) || !name_of(current,out->current) || !out->current[0]) return FALSE;
    out->group_count=(uint8_t)SudekiMpLanStorySpawnTraceGroups(out->groups,SUDEKIMP_STORY_SPAWN_GROUP_MAX);
    SudekiMpLanStorySkyCapture(out);
    SudekiMpLanStorySpawnDespawnCapture(out->despawned);
    SudekiMpLanStorySpawnEffectsCapture(out);
    return SudekiMpLanStorySpawnEventsCapture(out) && SudekiMpStoryEffectStateValid(out);
}
BOOL SudekiMpLanStoryEffectsInCurrentArea(const SudekiMpStoryEffectState *host) {
    unsigned count; uint8_t *table,*world=world_now(&count,&table);
    if(!world || !SudekiMpStoryEffectStateValid(host) || !*(world+WORLD_ARMED)) return FALSE;
    uint8_t *current=*(uint8_t **)(world+WORLD_CURRENT); char name[SUDEKIMP_STORY_AREA_NAME];
    return in_table(current,table,count) && name_of(current,name) && name[0] && !_stricmp(name,host->current);
}
void SudekiMpLanStoryEffectsFollowGroups(const SudekiMpStoryEffectState *host,
    const SudekiMpControlUpdateDispatchWitness *w) {
    if(!SudekiMpLanStoryEffectsInCurrentArea(host)) return;
    /* 0. Spawn groups (#54): once standing in the host's current zone, start
     * each host-started group sequence this client has not started yet. */
    {
        static SudekiMpStorySpawnGroup replayed[SUDEKIMP_STORY_SPAWN_GROUP_MAX];
        static unsigned replayed_count;
        static uint32_t replayed_epoch;
        if(replayed_epoch!=host->epoch) {replayed_count=0;replayed_epoch=host->epoch;}
        for(unsigned g=0;g<host->group_count;++g) {
            unsigned r=0;
            for(;r<replayed_count && !SudekiMpStorySpawnGroupSame(&replayed[r],&host->groups[g]);++r) {}
            if(r<replayed_count) continue;
            {   /* Already started natively here (e.g. during this client's own load). */
                SudekiMpStorySpawnGroup local[SUDEKIMP_STORY_SPAWN_GROUP_MAX];
                unsigned n=SudekiMpLanStorySpawnTraceGroups(local,SUDEKIMP_STORY_SPAWN_GROUP_MAX),k=0;
                for(;k<n && !SudekiMpStorySpawnGroupSame(&local[k],&host->groups[g]);++k) {}
                if(k<n) continue;
            }
            SudekiMpStorySpawnGroup group=host->groups[g];
            if(SudekiMpLanStoryClientResidencyCall(w,replay_spawn,&group)) {
                /* Retry refused calls; merely seeing a group grants no lease. */
                SudekiMpStorySpawnGroup local[SUDEKIMP_STORY_SPAWN_GROUP_MAX];
                unsigned n=SudekiMpLanStorySpawnTraceGroups(local,SUDEKIMP_STORY_SPAWN_GROUP_MAX),k=0;
                for(;k<n && !SudekiMpStorySpawnGroupSame(&local[k],&group);++k) {}
                if(k<n) {
                    if(replayed_count==SUDEKIMP_STORY_SPAWN_GROUP_MAX) {
                        memmove(replayed,replayed+1,(SUDEKIMP_STORY_SPAWN_GROUP_MAX-1u)*sizeof(*replayed)); --replayed_count;
                    }
                    replayed[replayed_count++]=group;
                }
            }
            return;
        }
    }
}
