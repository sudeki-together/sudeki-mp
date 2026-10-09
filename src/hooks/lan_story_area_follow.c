#include "hooks/lan_story_area_follow.h"

#include "engine/build_identity.h"
#include "engine/log.h"

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

BOOL SudekiMpLanStoryAreaCapture(SudekiMpStoryAreaState *out) {
    unsigned count; uint8_t *table,*world=world_now(&count,&table);
    if(!out || !world) return FALSE;
    memset(out,0,sizeof(*out)); out->observed_tick=GetTickCount();
    uint8_t *current=*(uint8_t **)(world+WORLD_CURRENT),*target=*(uint8_t **)(world+WORLD_TARGET);
    if(!in_table(current,table,count) || !in_table(target,table,count) ||
        !name_of(current,out->current) || !name_of(target,out->target)) return FALSE;
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
    last_call_at=GetTickCount(); if(!last_call_at) last_call_at=1u;
    InterlockedExchange(&calling,1);
    ((ZoneCall)(base+rva))(name);
    InterlockedExchange(&calling,0);
}
BOOL SudekiMpLanStoryAreaFollowCalling(void) { return InterlockedCompareExchange(&calling,0,0)!=0; }

void SudekiMpLanStoryAreaFollow(const SudekiMpStoryAreaState *host,uint32_t now) {
    unsigned count; uint8_t *table,*world=world_now(&count,&table);
    if(!world || !SudekiMpStoryAreaStateValid(host) || !*(world+WORLD_ARMED)) return;
    uint8_t *current=*(uint8_t **)(world+WORLD_CURRENT),*target=*(uint8_t **)(world+WORLD_TARGET);
    char mine_current[SUDEKIMP_STORY_AREA_NAME],mine_target[SUDEKIMP_STORY_AREA_NAME];
    if(!in_table(current,table,count) || !in_table(target,table,count) ||
        !name_of(current,mine_current) || !name_of(target,mine_target) || !mine_current[0]) return;
    /* 1. Background load: follow the host's target (native also unloads our previous target). */
    if(host->target[0] && _stricmp(host->target,mine_target)) {
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

int SudekiMpLanStoryAreaZoneState(const char *zone) {
    unsigned count; uint8_t *table,*world=world_now(&count,&table);
    uint8_t *d=world && zone && zone[0]?find(zone,table,count):NULL;
    if(!d) return -1;
    return *(uint8_t **)(world+WORLD_CURRENT)==d?3:(int)*(uint32_t *)(d+DESC_STATE);
}
