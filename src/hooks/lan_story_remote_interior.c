#include "hooks/lan_story_runtime.h"
#include "hooks/lan_story_remote_interior.h"
#include "hooks/lan_story_rejoin.h"

#include "engine/build_identity.h"
#include "engine/log.h"
#include "hooks/lan_story_area_follow.h"
#include "hooks/lan_story_split.h"
#include "hooks/lan_story_sky.h"
#include "hooks/lan_story_zone_banner.h"
#include "cleanroom/engine.h"

#include <math.h>
#include <string.h>

enum {
    RVA_POSITION_SET=0x3050u, RVA_POSITION_VTABLE=0x2cdefcu,
    RVA_REGISTRY=0x409d8cu,
    RVA_SNAP_EXPORT=0xb2c30u, RVA_SNAP_FLAG=0x3207e1u,
    RESIDENT_SETTLE_MS=1000u,
    /* The native door scripts fade to black over 1.5 s before the area
     * changes (FadeToBlack 1.5, ShowLoadingIcon; CONFIRMED_LIVE trace of the
     * lead's TEMP doors 2026-10-16). The member's client fades on its own
     * trigger contact; the host moves the member only after that fade. */
    DOOR_FADE_MS=1600u, LOAD_TIMEOUT_MS=15000u, CLIENT_RETRY_MS=3000u,
    CLIENT_ENTRY_TIMEOUT_MS=10000u,
    /* Members waiting for the lead's own entry into their house to load it. */
    REJOIN_WAIT_MS=10000u, REJOIN_SETTLE_MS=150u
};
/* Horizontal trigger reach and re-arm distance (world units). The Kamo
 * return point stands 0.92 from the entry trigger. */
static const float TRIGGER_REACH=0.9f,TRIGGER_REARM=1.6f,TRIGGER_HEIGHT=2.5f;
/* Two party colliders side by side (radii 0.5-0.6, CONFIRMED_LIVE host read
 * 2026-10-17) plus a margin: a rejoining member never lands in the lead. */
static const float REJOIN_CLEAR=1.2f;
/* FLD [ECX+0x18]; FLD [EDX]; FUCOMPP; FNSTSW AX; TEST AH,0x44; JP +0x1e */
static const uint8_t setter_entry[14]={0xd9,0x41,0x18,0xd9,0x02,0xda,0xe9,0xdf,0xe0,0xf6,0xc4,0x44,0x7a,0x1e};
/* AiPCFormationSetSnapEnabled(bool): mov al,[esp+4]; mov [abs],al */
static const uint8_t snap_entry[5]={0x8a,0x44,0x24,0x04,0xa2};
typedef void (__attribute__((fastcall)) *PositionSet)(void *,const float *);
typedef void (__cdecl *SnapSet)(int);

typedef struct Door {
    const char *zone,*entry,*exit,*marker;
    float inside[3],outside[3];
    /* The door scripts' spot cue (exact case) and the zone-name banner
     * string (DisplayInteriorZoneName; 0 = none). */
    const char *enter_cue,*exit_cue;
    uint16_t name_text;
    /* The static camera position its cluster entry script sets (NULL: none). */
    const char *camera;
} Door;
/* The lead's native arrival through the door (NB_Kamo_Shop_Start_Pos;
 * CONFIRMED_LIVE 2026-10-17, host poll: (0.080, 0.109, 1.005)) and the
 * native return point the entry stored (live 2026-10-16). */
static const Door doors[]={
    {"LNBr_Kamo_shop","NB_Kamo_Shop_Entry_Trig","NB_Kamo_Shop_Door_Exit_Trig","NB_Kamo_Shop_Start_Pos",
        {0.08f,0.11f,1.0f},{-18.8f,-2.8f,340.1f},
        /* NB_Kamo_Shop_Entry/_Door_Exit OnAction -> EnterInterior|SRS /
         * ExitInterior default cue; DisplayInteriorZoneName's Kamo row
         * (CONFIRMED_STATIC, SOLWORLDM.gex 2026-10-17). */
        "generic_wooden_door","generic_wooden_door",5656u,
        /* LNBr_Kamo_shopDefault_Cluster__OnClusterEntry: SetCameraStaticPosition(
         * 'default', NB_KAMO_SHOP_CAM, 0, false, 0); eye (4.874, 4.153, -0.704)
         * in LNBr_Kamo_shop.cameras (CONFIRMED_STATIC; host CONFIRMED_LIVE). */
        "NB_Kamo_Shop_Cam"},
};
enum { DOORS=sizeof(doors)/sizeof(doors[0]) };

static uint8_t *base;
static BOOL ready;
static BOOL snap_changed,lights_held;
static uint8_t snap_before;
static const struct Door *door_for(const char *zone);

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL position_of(const void *entity,float out[3]) {
    const uint8_t *e=entity,*p;
    if(!readable(e,0x48u) || !readable(p=*(const uint8_t *const *)(e+0x44u),0x24u) ||
        *(void *const *)p!=base+RVA_POSITION_VTABLE || *(const void *const *)(p+0x10u)!=e) return FALSE;
    memcpy(out,p+0x18u,12u);
    return isfinite(out[0]) && isfinite(out[1]) && isfinite(out[2]);
}
static BOOL entity_named(const uint8_t *e,const char *name) {
    const uint32_t *r;
    if(!readable(e,0x48u) || !readable(r=*(const uint32_t *const *)(e+0x38u),8u)) return FALSE;
    const char *s=(const char *)(uintptr_t)r[1];
    size_t n=strlen(name);
    return readable(s,n+1u) && !_strnicmp(s,name,n) && !s[n];
}
/* Registry entity by ResourceName text. A cached pointer is re-proved by
 * registry membership and name each call; a full name scan (one memory
 * query per entity) runs at most every two seconds per name. */
static const uint8_t *find_entity(const char *name,const uint8_t *cached,DWORD *last_scan,uint32_t now) {
    const uint8_t *owner=readable(base+RVA_REGISTRY,4u)?*(const uint8_t *const *)(base+RVA_REGISTRY):NULL;
    if(!readable(owner,0x40u)) return NULL;
    unsigned count=*(const uint32_t *)(owner+0x34u);
    const uint8_t *const *entries=*(const uint8_t *const *const *)(owner+0x3cu);
    if(!count || count>8192u || !readable(entries,count*sizeof(*entries))) return NULL;
    if(cached) for(unsigned i=0;i<count;++i) if(entries[i]==cached) {
        if(entity_named(cached,name)) return cached;
        break;
    }
    if(*last_scan && now-*last_scan<2000u) return NULL;
    *last_scan=now?now:1u;
    for(unsigned i=0;i<count;++i) if(entity_named(entries[i],name)) return entries[i];
    return NULL;
}
static BOOL reach(const float a[3],const float b[3],float *distance) {
    float dx=a[0]-b[0],dz=a[2]-b[2];
    *distance=sqrtf(dx*dx+dz*dz);
    return fabsf(a[1]-b[1])<TRIGGER_HEIGHT;
}
static BOOL place(void *who,const float xyz[3]) {
    uint8_t *p;
    if(!readable(who,0x48u) || !readable(p=*(uint8_t **)((uint8_t *)who+0x44u),0x24u) ||
        *(void **)p!=base+RVA_POSITION_VTABLE || *(void **)(p+0x10u)!=who) return FALSE;
    ((PositionSet)(base+RVA_POSITION_SET))(p,xyz);
    return TRUE;
}
/* Zone render data (zone data object, descriptor+0x14, at +0x68): the
 * stdcall activator 0x510250 makes a zone's renderer the render scene's
 * (render+0x08, with its light handlers at +0xA4/+0xA8) and unregisters the
 * previously active one (global 0x7C300C). A house loading in the
 * background activates itself, and its unload (0x510510) leaves the slot
 * empty: the exterior's characters lost their lighting (CONFIRMED_LIVE
 * 2026-10-16, watchpoint on render+0x08: cleared from UnloadZone). */
enum { RVA_ACTIVATE_RENDER=0x110250u, RVA_ACTIVE_RENDER=0x3c300cu, RVA_SCENE=0x408d58u, RVA_WORLD=0x408d10u };
typedef void (__attribute__((stdcall)) *ActivateRender)(void *);
/* A zone's data object (descriptor +0x14): the named zone, or the current
 * zone when NULL (then only if it is named `current_name`, when given). */
static uint8_t *zone_data(const char *zone_or_null,const char *current_name) {
    uint8_t *w=readable(base+RVA_WORLD,4u)?*(uint8_t **)(base+RVA_WORLD):NULL,*d=NULL;
    if(!readable(w,0x60u)) return NULL;
    if(!zone_or_null) {
        d=*(uint8_t **)(w+0x0cu);
        const char *n=readable(d,0x38u)?*(const char **)(d+0x24u):NULL;
        if(current_name && (!readable(n,1u) || _stricmp(n,current_name))) return NULL;
    } else {
        unsigned count=*(unsigned *)(w+0x54u); uint8_t *table=*(uint8_t **)(w+0x50u);
        if(!count || count>512u || !readable(table,count*0x54u)) return NULL;
        for(unsigned i=0;i<count && !d;++i) {
            const char *n=*(const char **)(table+i*0x54u+0x24u);
            if(readable(n,1u) && !_stricmp(n,zone_or_null)) d=table+i*0x54u;
        }
    }
    return readable(d,0x38u)?*(uint8_t **)(d+0x14u):NULL;
}
static uint8_t *zone_render(const char *zone_or_null) {
    uint8_t *data=zone_data(zone_or_null,NULL);
    uint8_t *rd=readable(data,0x74u)?data+0x68u:NULL;
    return rd && *(uint32_t *)(rd+8u)<=2u && *(void **)(rd+4u)?rd:NULL;
}
static BOOL activate_render(uint8_t *rd,const char *why) {
    static const uint8_t entry[8]={0x53,0x8b,0x5c,0x24,0x08,0x55,0x8b,0x2d};
    if(!rd || !readable(base+RVA_ACTIVATE_RENDER,12u) || memcmp(base+RVA_ACTIVATE_RENDER,entry,sizeof(entry)) ||
        *(uint8_t **)(base+RVA_ACTIVATE_RENDER+8u)!=base+RVA_SCENE || !*(void **)(base+RVA_SCENE) ||
        !readable(base+RVA_ACTIVE_RENDER,4u)) return FALSE;
    uint8_t *before=*(uint8_t **)(base+RVA_ACTIVE_RENDER);
    if(before==rd && *(uint32_t *)(rd+8u)==2u) return TRUE;
    ((ActivateRender)(base+RVA_ACTIVATE_RENDER))(rd);
    SudekiMpLogFormat("story_remote_interior event=render_activated why=%s data=%p previous=%p state=%lu\r\n",
        why,rd,before,(unsigned long)*(uint32_t *)(rd+8u));
    return *(uint8_t **)(base+RVA_ACTIVE_RENDER)==rd;
}
/* The lead's current zone renders and lights the host's view. */
static void keep_exterior_render(const char *why) {
    uint8_t *mine=zone_render(NULL);
    if(mine && readable(base+RVA_ACTIVE_RENDER,4u) && *(uint8_t **)(base+RVA_ACTIVE_RENDER)!=mine)
        (void)activate_render(mine,why);
}
static BOOL snap_exact(void) {
    return readable(base+RVA_SNAP_EXPORT,10u) && !memcmp(base+RVA_SNAP_EXPORT,snap_entry,sizeof(snap_entry)) &&
        *(uint8_t **)(base+RVA_SNAP_EXPORT+5u)==base+RVA_SNAP_FLAG && readable(base+RVA_SNAP_FLAG,1u);
}
static void snap_off(void) {
    if(snap_changed || !snap_exact()) return;
    snap_before=base[RVA_SNAP_FLAG];
    ((SnapSet)(base+RVA_SNAP_EXPORT))(0);
    snap_changed=TRUE;
}
static void snap_restore(void) {
    if(!snap_changed || !snap_exact()) return;
    /* A native write since we turned it off is the newest intent: keep it. */
    if(!base[RVA_SNAP_FLAG]) ((SnapSet)(base+RVA_SNAP_EXPORT))(snap_before?1:0);
    snap_changed=FALSE;
}
/* Per-member shop visits (owner decision 2026-10-17: story houses stay
 * vanilla, the lead goes in alone; any player may walk into a shop on their
 * own, several at once; players in the same shop see each other, D11).
 * One house at a time (issue #83): a member may enter only the house already
 * in use by other members, or, with the lead inside, the lead's own shop. */
enum { IDLE, LOADING, INSIDE };
typedef struct Member {
    unsigned phase;
    const Door *door;
    void *actor;
    uint32_t epoch;
    DWORD since,resident_since,exit_at,exit_scan,stale_since,rejoin_until;
    /* Inside before the lead's own entry: where the member stood (the house
     * reload parks the body outside; their screen keeps them here). */
    float rejoin_at[3];
    BOOL rejoin_pose;
    DWORD rejoin_ready_at;
    BOOL exit_armed;
    BOOL away_inside,away_return,away_unparked;
    BOOL in_scene;   /* this member's bit is in the observer's remote mask */
    const uint8_t *exit_entity;
} Member;
static Member members[4];
static BOOL entry_armed[4];
static unsigned away_return_refusals;
static const uint8_t *entry_entities[DOORS];
static DWORD entry_scans[DOORS];
static const Door *house;        /* the house in use by members, or NULL */
static BOOL house_loaded_by_us;  /* we background-loaded it (lead outside) */
static BOOL house_draining;      /* no members left; unload pending (still loading) */
static DWORD house_draining_since;
static uint8_t members_mask(unsigned phase_filter) {
    uint8_t m=0;
    for(unsigned c=0;c<4u;++c) if(members[c].phase!=IDLE && (!phase_filter || members[c].phase==phase_filter)) m|=(uint8_t)(1u<<c);
    return m;
}
/* Arrival / return spots fanned out so members do not stand in each other. */
static void member_spot(const float base_xyz[3],unsigned c,float out[3]) {
    static const float dx[4]={0.0f,0.7f,-0.7f,0.0f},dz[4]={0.0f,0.3f,0.3f,0.6f};
    out[0]=base_xyz[0]+dx[c&3u]; out[1]=base_xyz[1]; out[2]=base_xyz[2]+dz[c&3u];
}
/* The background house's own script lighting (e.g. the Kamo shop ambient
 * 0.09) is captured only while it loads; the queue is applied if the lead
 * joins that house, dropped when it is released. Exterior light calls after
 * the load are never held (review 2026-10-17). */
static BOOL lights_capturing;
static void lights_stop_capture(void) {
    if(lights_capturing) { SudekiMpLanStorySkyHoldLighting(FALSE); lights_capturing=FALSE; }
}
static void lights_release(BOOL apply) {
    lights_stop_capture();
    if(lights_held) { SudekiMpLanStorySkyReleaseHeld(apply); lights_held=FALSE; }
}
static BOOL evicting;            /* inside evict(): the lead is changing area */
static void house_forget(const char *why,int unloaded) {
    SudekiMpLogFormat("story_remote_interior event=house_released zone=%s reason=%s unloaded=%d\r\n",house->zone,why,unloaded);
    lights_release(FALSE);
    SudekiMpLanStoryObserverNoteResidentHouse(NULL);
    SudekiMpLanStoryObserverSetRemoteVisit(NULL);
    house=NULL; house_loaded_by_us=house_draining=FALSE;
}
/* The house is no longer used by any member: restore the shared state. A
 * house the lead is in (current) is the native exit's to unload. A house
 * still loading (state 1) cannot be unloaded yet: keep it (draining) and
 * retry every host frame, holding its lighting until it is gone. */
static void house_release(const char *why) {
    if(!house || members_mask(0)) return;
    snap_restore();
    /* Nobody stays in a background house: interiors share coordinates, and
     * an empty resident house captured the lead's sector inside another
     * house (CONFIRMED_LIVE 2026-10-16: Kamo became current mid tent visit,
     * then the lead was left in an unloaded sector). */
    int unloaded=house_loaded_by_us?SudekiMpLanStoryAreaUnloadResident(house->zone):-1;
    if(unloaded==0) {
        /* The house is still loading: its own load-script lighting has not
         * run yet. Keep capturing until the drain unloads it (or its bound
         * ends), then drop it. Only a lead's zone call (evict) releases at
         * once: everything after it belongs to the lead's new area. */
        if(evicting) lights_release(FALSE);
        if(!house_draining) {
            SudekiMpLogFormat("story_remote_interior event=house_draining zone=%s reason=%s\r\n",house->zone,why);
            house_draining_since=GetTickCount(); if(!house_draining_since) house_draining_since=1u;
        }
        house_draining=TRUE; return;
    }
    if(unloaded>0) keep_exterior_render("house_unloaded");
    house_forget(why,unloaded);
}
static void finish(unsigned c,const char *reason,BOOL outside) {
    Member *m=&members[c]; BOOL placed=FALSE; float spot[3];
    if(m->phase==IDLE) return;
    SudekiMpLanStorySplitReleaseMember(c,reason); /* re-hide before any move */
    if(m->phase==INSIDE && outside && m->door) { member_spot(m->door->outside,c,spot); placed=place(m->actor,spot); }
    if(m->in_scene) SudekiMpLanStoryObserverClearRemoteMembers((uint8_t)(1u<<c));
    SudekiMpLogFormat("story_remote_interior event=end zone=%s character=%u phase=%u reason=%s placed_outside=%u\r\n",
        m->door?m->door->zone:"-",c,m->phase,reason,placed);
    if(m->away_unparked && m->phase==LOADING) (void)SudekiMpLanStorySplitParkMember(c);
    memset(m,0,sizeof(*m)); m->phase=IDLE;
    entry_armed[c]=FALSE;
    house_release(reason);
}
/* Observer callback, native thread, before another zone call runs (not a
 * background load of the members' house, not the lead entering it). The
 * native exit of a house unloads it, so members inside leave with the lead;
 * members still loading are ended without a move. */
static void evict(void) {
    /* The lead is changing area: the visit house's held load lighting is
     * irrelevant now, and nothing the lead's new area does may be held. */
    lights_release(FALSE);
    evicting=TRUE;
    for(unsigned c=0;c<4u;++c) finish(c,"zone_call",TRUE);
    evicting=FALSE;
}

/* Observer callback, native thread, just before the lead's native TEMP into
 * the house members use while it is background-resident (the native entry
 * cannot enter a loaded house; CONFIRMED_LIVE 2026-10-17 it hung with the
 * exterior released). The members' bodies go to the door outside, the
 * house is unloaded and the native entry loads it as in vanilla; once the
 * lead is inside, the members are placed back (the lead's-shop path). They
 * stay marked inside in the scene throughout, so their clients keep their
 * own local house. A member already walking out (exit touched) leaves. When
 * the house cannot be unloaded nothing is changed: the native entry is then
 * a no-op (state 2) and the lead can use the door again. */
static void lead_entry(const char *zone) {
    float stood[4][3];
    if(!house || !zone || _stricmp(house->zone,zone) || !house_loaded_by_us || house_draining ||
        !SudekiMpLanStoryAreaResidentUnloadable(house->zone)) return;
    for(unsigned c=0;c<4u;++c)
        if(members[c].phase==INSIDE && !position_of(members[c].actor,stood[c])) {
            SudekiMpLogFormat("story_remote_interior event=lead_entry_refused zone=%s character=%u reason=actor_unproved\r\n",zone,c);
            return;
        }
    DWORD now=GetTickCount(); if(!now) now=1u;
    unsigned moved=0,left=0;
    for(unsigned c=0;c<4u;++c) {
        Member *m=&members[c]; float spot[3];
        if(m->phase==IDLE) continue;
        SudekiMpLanStorySplitReleaseMember(c,"lead_entering");
        if(m->phase==INSIDE) { member_spot(m->door->outside,c,spot); (void)place(m->actor,spot); moved|=1u<<c; }
        if(m->phase==INSIDE && m->exit_at) {
            /* Already leaving: the visit ends here, outside. */
            if(m->in_scene) SudekiMpLanStoryObserverClearRemoteMembers((uint8_t)(1u<<c));
            SudekiMpLogFormat("story_remote_interior event=end zone=%s character=%u phase=%u reason=exit_trigger_lead_entering placed_outside=1\r\n",
                zone,c,m->phase);
            memset(m,0,sizeof(*m)); m->phase=IDLE; entry_armed[c]=FALSE; left|=1u<<c;
            continue;
        }
        BOOL stayed=m->phase==INSIDE;
        m->phase=LOADING; m->since=now; m->resident_since=m->exit_at=m->exit_scan=m->stale_since=0;
        m->exit_armed=FALSE; m->exit_entity=NULL; m->rejoin_until=now+REJOIN_WAIT_MS; if(!m->rejoin_until) m->rejoin_until=1u;
        m->rejoin_pose=stayed; m->rejoin_ready_at=0;
        if(stayed) {
            memcpy(m->rejoin_at,stood[c],sizeof(m->rejoin_at));
            SudekiMpLogFormat("story_remote_interior event=rejoin_recorded zone=%s character=%u at=%.2f,%.2f,%.2f\r\n",
                zone,c,(double)stood[c][0],(double)stood[c][1],(double)stood[c][2]);
        }
    }
    int unloaded=SudekiMpLanStoryAreaUnloadResident(zone);
    /* The native load runs the house's own script lighting again. */
    lights_release(FALSE);
    if(unloaded>0) {
        keep_exterior_render("lead_entering");
        house_loaded_by_us=FALSE;
        SudekiMpLanStoryObserverNoteResidentHouse(NULL);
    }
    SudekiMpLogFormat("story_remote_interior event=lead_entering zone=%s moved=%u left=%u unloaded=%d policy=members_rejoin_scene_kept\r\n",
        zone,moved,left,unloaded);
    if(!members_mask(0)) house_release("lead_entering");
}

/* Host, native thread: a member parked outside by the lead's entry is
 * published where they stood (their own screen keeps them in the house)
 * until placed back. Only for the proved actor and scene epoch. */
BOOL SudekiMpLanStoryRemoteInteriorHeldPose(unsigned c,const void *actor,uint32_t epoch,float out[3]) {
    if(!ready || c>=4u || !out) return FALSE;
    const Member *m=&members[c];
    if(m->phase!=LOADING || !m->rejoin_until || !m->rejoin_pose || !actor || actor!=m->actor || epoch!=m->epoch) return FALSE;
    memcpy(out,m->rejoin_at,sizeof(m->rejoin_at));
    return TRUE;
}
/* Host, native thread: the lead is entering `zone`, the house this module
 * keeps background-resident for a visit, and its entry will park and
 * rejoin (lead_entry's own preconditions). A member inside that visit (not
 * leaving, not already parked), or one outside, may keep steering through
 * the lead's door script. */
BOOL SudekiMpLanStoryRemoteInteriorHostLeadEntryMember(const SudekiMpLanStoryNativeRoster *r,unsigned c,const char *zone) {
    if(!ready || !r || c>=4u || c==r->leader_character || !zone || !house || _stricmp(house->zone,zone) ||
        !house_loaded_by_us || house_draining || !SudekiMpLanStoryAreaResidentUnloadable(house->zone)) return FALSE;
    const Member *m=&members[c];
    if(m->phase==IDLE) return TRUE;
    return m->phase==INSIDE && m->in_scene && m->door==house && m->actor && m->actor==r->actors[c] &&
        m->epoch==r->epoch && !m->exit_at && !m->rejoin_pose;
}
/* Host, native thread: `c` is settled inside this module's background
 * visit (its own house while the lead is elsewhere): its body is its own,
 * not part of the lead's modal (e.g. the lead's NPC conversation outside). */
BOOL SudekiMpLanStoryRemoteInteriorHostVisitSettled(const SudekiMpLanStoryNativeRoster *r,unsigned c) {
    if(!ready || !r || c>=4u || c==r->leader_character || !house || !house_loaded_by_us || house_draining) return FALSE;
    const Member *m=&members[c];
    return m->phase==INSIDE && m->in_scene && m->door==house && m->actor && m->actor==r->actors[c] &&
        m->epoch==r->epoch && !m->exit_at && !m->rejoin_pose;
}
BOOL SudekiMpLanStoryRemoteInteriorMemberParked(unsigned c) {
    return ready && c<4u && members[c].phase==LOADING && members[c].rejoin_until && members[c].rejoin_pose;
}
BOOL SudekiMpLanStoryRemoteInteriorInitialize(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!b || !SudekiMpCheckLoadedExecutable(image) || !readable(b+RVA_POSITION_SET,sizeof(setter_entry)) ||
        memcmp(b+RVA_POSITION_SET,setter_entry,sizeof(setter_entry))) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    base=b; ready=TRUE;
    memset(members,0,sizeof(members)); memset(entry_armed,0,sizeof(entry_armed));
    for(unsigned c=0;c<4u;++c) members[c].phase=IDLE;
    house=NULL; house_loaded_by_us=house_draining=lights_capturing=lights_held=snap_changed=FALSE;
    SudekiMpLanStoryObserverSetRemoteInteriorEvict(evict);
    SudekiMpLanStoryObserverSetRemoteInteriorLeadEntry(lead_entry);
    return TRUE;
}

/* A member placed inside (scene marks them inside; epoch kept). */
static BOOL member_enter(unsigned c,uint32_t now,const float *at) {
    Member *m=&members[c]; float spot[3];
    snap_off();
    if(m->away_return) {
        if(!SudekiMpLanStorySplitUnparkMember(c)) { finish(c,"away_unpark_refused",FALSE);return FALSE; }
        m->away_unparked=TRUE;
    }
    if(at) memcpy(spot,at,sizeof(spot)); else member_spot(m->door->inside,c,spot);
    if(!place(m->actor,spot)) { finish(c,"place_refused",FALSE); return FALSE; }
    if(!SudekiMpLanStoryObserverSetRemoteInterior(m->door->zone,(uint8_t)(1u<<c))) {
        member_spot(m->door->outside,c,spot); (void)place(m->actor,spot);
        m->phase=LOADING; /* not inside: finish must not clear the scene */
        finish(c,"scene_refused",FALSE); return FALSE;
    }
    m->phase=INSIDE; m->in_scene=TRUE; m->away_unparked=FALSE;
    if(m->away_return) m->away_inside=TRUE;
    m->rejoin_until=0; m->rejoin_pose=FALSE; m->rejoin_ready_at=0;
    m->since=now?now:1u; m->exit_armed=FALSE; m->exit_entity=NULL; m->exit_scan=0; m->exit_at=0;
    SudekiMpLogFormat("story_remote_interior event=inside zone=%s character=%u members=%u snap_was=%u\r\n",
        m->door->zone,c,(unsigned)members_mask(INSIDE),(unsigned)snap_before);
    return TRUE;
}
enum { MEMBER_STALE_MS=15000u };
static void member_service(unsigned c,const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *scene,uint8_t remote_characters,uint8_t input_fresh,BOOL lead_inside,uint32_t now) {
    Member *m=&members[c];
    /* The character, its actor and the exterior lifetime must stay the same;
     * an unproved actor is never moved. */
    if(scene->epoch!=m->epoch || !(roster->available_mask&(1u<<c)) || roster->actors[c]!=m->actor) {
        finish(c,"roster_changed",FALSE); return;
    }
    if(c==roster->leader_character) { finish(c,"became_lead",TRUE); return; }
    if(!(remote_characters&(1u<<c))) { finish(c,"player_left",TRUE); return; }
    BOOL lead_here=lead_inside && !_stricmp(m->door->zone,scene->temporary);
    if(m->phase==LOADING) {
        /* A rejoin also spans the lead's own house load. */
        if(now-m->since>(m->rejoin_until?2u*LOAD_TIMEOUT_MS:LOAD_TIMEOUT_MS)) { finish(c,"load_timeout",FALSE); return; }
        /* The lead went into another house: one house at a time (#83). */
        if(lead_inside && !lead_here) { finish(c,"lead_in_other_house",FALSE); return; }
        if(lead_here && m->rejoin_until && m->rejoin_pose) {
            /* Back where the member stood once the lead's entry made the
             * house current and placed the lead (no door fade: on the
             * member's own screen they never left). */
            float lead_at[3],spot[3];
            BOOL lead_known=position_of(roster->actors[roster->leader_character],lead_at) &&
                fabsf(lead_at[1]-m->door->inside[1])<TRIGGER_HEIGHT;
            if(SudekiMpLanStoryAreaZoneState(m->door->zone)!=3 || !lead_known ||
                !SudekiMpLanStoryObserverNativeRosterExact(roster)) { m->rejoin_ready_at=0; return; }
            if(!m->rejoin_ready_at) { m->rejoin_ready_at=now?now:1u; return; }
            if(now-m->rejoin_ready_at<REJOIN_SETTLE_MS) return;
            unsigned source=SudekiMpLanStoryRejoinSpot(m->rejoin_at,lead_at,REJOIN_CLEAR,spot);
            if(source==SUDEKIMP_REJOIN_INVALID) member_spot(m->door->inside,c,spot);
            DWORD parked=now-m->since;
            if(member_enter(c,now,spot))
                SudekiMpLogFormat("story_remote_interior event=rejoined zone=%s character=%u source=%s at=%.2f,%.2f,%.2f parked_ms=%lu\r\n",
                    members[c].door->zone,c,source==SUDEKIMP_REJOIN_RECORDED?"recorded":source==SUDEKIMP_REJOIN_CLEARED?"cleared":"fan",
                    (double)spot[0],(double)spot[1],(double)spot[2],(unsigned long)parked);
            return;
        }
        if(lead_here) {
            /* The lead's shop is current (ZoneState reports current as 3):
             * no load, place after the fade. */
            if(now-m->since<DOOR_FADE_MS || SudekiMpLanStoryAreaZoneState(m->door->zone)!=3 ||
                !SudekiMpLanStoryObserverNativeRosterExact(roster)) return;
            (void)member_enter(c,now,NULL); return;
        }
        /* Rejoining: the lead's own entry is loading this house (state 1,
         * then current). If it did not take, back to a background visit;
         * the member leaves the scene until placed again (its client
         * leaves its house rather than show the body outside). */
        if(m->rejoin_until) {
            int house_state=SudekiMpLanStoryAreaZoneState(m->door->zone);
            if((int32_t)(now-m->rejoin_until)<0 && (house_state==1 || now-m->since<2000u)) return;
            m->rejoin_until=0; m->rejoin_pose=FALSE; m->rejoin_ready_at=0;
            if(m->in_scene) { SudekiMpLanStoryObserverClearRemoteMembers((uint8_t)(1u<<c)); m->in_scene=FALSE; }
            SudekiMpLogFormat("story_remote_interior event=rejoin_ended zone=%s character=%u house_state=%d policy=background_visit\r\n",
                m->door->zone,c,house_state);
        }
        int state=SudekiMpLanStoryAreaBackgroundLoad(m->door->zone);
        if(state<0) { finish(c,"load_refused",FALSE); return; }
        if(state>0 && !house_loaded_by_us) {
            /* Background-resident for this visit: ours to unload. While it
             * is still loading (state 1), capture its script lighting until
             * it is resident and settled; a house already resident (e.g.
             * left by an earlier drain) has run its script, so nothing is
             * captured (only the exterior's own calls would be). */
            house_loaded_by_us=TRUE;
            SudekiMpLanStoryObserverNoteResidentHouse(m->door->zone);
            if(state==1 && !lights_held) { SudekiMpLanStorySkyHoldLighting(TRUE); lights_held=lights_capturing=TRUE; }
        }
        if(state!=2) { m->resident_since=0; return; }
        /* The house's load activated its own renderer; the lead stays out. */
        keep_exterior_render("background_house_loaded");
        if(!m->resident_since) { m->resident_since=now?now:1u; return; }
        if(now-m->resident_since<RESIDENT_SETTLE_MS) return;
        lights_stop_capture();
        if(now-m->since<DOOR_FADE_MS || !SudekiMpLanStoryObserverNativeRosterExact(roster)) return;
        (void)member_enter(c,now,NULL); return;
    }
    /* INSIDE */
    {   /* Resident (2), current while the lead visits (ZoneState reports 3),
         * or 1 for the frames the native swap marks it loading. */
        int state=SudekiMpLanStoryAreaZoneState(m->door->zone);
        if(state<1 || state>4) { finish(c,"house_not_resident",TRUE); return; }
    }
    if(scene->phase==SUDEKIMP_LAN_STORY_READY && !(scene->inside_mask&(1u<<c)) &&
        now-m->since>1000u) { finish(c,"scene_cleared",TRUE); return; }
    /* Seen by the lead in her own shop (D11); hidden natively elsewhere. */
    if(lead_here) (void)SudekiMpLanStorySplitShowMember(roster,c);
    else SudekiMpLanStorySplitReleaseMember(c,"lead_elsewhere");
    /* An away body already inside the lead's shop stays under native AI;
     * only lead eviction can take it out. Return clears this flag. */
    if(m->away_inside) {m->exit_at=0;m->stale_since=0;return;}
    /* A client that cannot present the house sends no input: end the visit
     * (placed outside) rather than hold an unplayable member inside. */
    if(input_fresh&(1u<<c)) m->stale_since=0;
    else if(!m->stale_since) m->stale_since=now?now:1u;
    else if(now-m->stale_since>MEMBER_STALE_MS) { finish(c,"client_not_following",TRUE); return; }
    /* Exit touched: leave once the member's client has faded out. */
    if(m->exit_at) { if(now-m->exit_at>=DOOR_FADE_MS) finish(c,"exit_trigger",TRUE); return; }
    float at[3],trigger[3],distance;
    if(!position_of(m->actor,at)) return;
    if(at[1]<m->door->inside[1]-6.0f) {
        /* Fell out of the interior: back to the arrival spot. */
        float spot[3]; member_spot(m->door->inside,c,spot);
        if(place(m->actor,spot)) SudekiMpLogFormat("story_remote_interior event=rescued zone=%s character=%u y=%.1f\r\n",m->door->zone,c,(double)at[1]);
        return;
    }
    m->exit_entity=find_entity(m->door->exit,m->exit_entity,&m->exit_scan,now);
    if(!m->exit_entity || !position_of(m->exit_entity,trigger)) {
        /* Unknown exit trigger: a point just behind the arrival spot. */
        memcpy(trigger,m->door->inside,sizeof(trigger)); trigger[2]+=1.2f;
    }
    if(!reach(at,trigger,&distance)) return;
    if(distance>TRIGGER_REARM) m->exit_armed=TRUE;
    if(m->exit_armed && distance<TRIGGER_REACH) {
        m->exit_at=now?now:1u;
        SudekiMpLogFormat("story_remote_interior event=exit_touched zone=%s character=%u\r\n",m->door->zone,c);
    }
}

static BOOL away_body_standing(const SudekiMpLanStoryNativeRoster *r,unsigned c) {
    if(!r || c>=4u || !(r->available_mask&(1u<<c)) || !r->actors[c]) return FALSE;
    const uint8_t *ai=r->ai[c],*actor=r->actors[c],*arb;
    return readable(ai,0x48) && readable(actor,0x98) && *(void *const *)(ai+0x10)==actor &&
        *(void *const *)(actor+0x94)==ai && !(*(uint32_t *)(ai+0x44)&0x58) &&
        readable(arb=*(const uint8_t *const *)(actor+0x90),0x54) &&
        *(void *const *)(arb+0x10)==actor && !(*(uint32_t *)(arb+0x50)&0x10000);
}
void SudekiMpLanStoryRemoteInteriorAwayDrained(const SudekiMpLanStoryNativeRoster *r,
    const SudekiMpLanStoryScene *s,unsigned c) {
    if(!ready || !r || !s || c>=4u || c==r->leader_character || !SudekiMpLanStorySplitActive()) return;
    Member *m=&members[c];
    /* member_enter may have updated the observer after this frame's scene
     * snapshot. The coordinator's visit state must not be parked again. */
    if(m->phase==INSIDE || m->phase==LOADING || m->away_return) {
        if(m->phase==INSIDE && m->actor==r->actors[c] && m->epoch==s->epoch) {
            m->away_inside=TRUE;m->exit_at=0;
        }
        return;
    }
    if(!(s->inside_mask&(1u<<c)) && away_body_standing(r,c)) (void)SudekiMpLanStorySplitParkMember(c);
}
void SudekiMpLanStoryRemoteInteriorCancelAwayReturn(unsigned c) {
    /* Only this synchronous lead-shop wait, never a native background task. */
    if(c<4u && members[c].away_return && members[c].phase==LOADING)
        finish(c,"away_return_cancelled",FALSE);
}
int SudekiMpLanStoryRemoteInteriorAwayReturn(const SudekiMpLanStoryNativeRoster *r,
    const SudekiMpLanStoryScene *s,unsigned c,uint32_t now) {
    if(!ready || !r || !s || c>=4u || c==r->leader_character) return -1;
    Member *m=&members[c];
    if(!SudekiMpLanStorySplitMemberParked(c) && !(m->phase==INSIDE && m->away_inside) &&
        !(m->away_return && m->phase==LOADING)) return 1;
    if(s->phase!=SUDEKIMP_LAN_STORY_READY) return -1;
    if(SudekiMpLanStoryObserverHolding()) return 0;
    if(!SudekiMpLanStoryObserverNativeRosterExact(r)) return -1;
    if(m->phase==INSIDE && m->away_inside && m->actor==r->actors[c] && m->epoch==s->epoch) {
        if(SudekiMpLanStorySplitMemberParked(c)) {
            snap_off();
            if(!SudekiMpLanStorySplitUnparkMember(c) || SudekiMpLanStorySplitMemberParked(c)) return 0;
        }
        if(SudekiMpLanStorySplitShowMember(r,c)<=0) return 0;
        m->away_inside=m->away_return=FALSE;return 1;
    }
    if(m->away_return && m->phase==LOADING) return 0;
    if(!SudekiMpLanStorySplitMemberParked(c)) return 1;
    if(!away_body_standing(r,c)) {
        if(away_return_refusals<32u) {++away_return_refusals;
            SudekiMpLogFormat("story_remote_interior event=away_return_refused character=%u reason=body_not_standing\r\n",c);}
        return -1;
    }
    if(!SudekiMpLanStorySplitActive() || house_draining || m->phase!=IDLE) return -1;
    BOOL lead_inside=r->leader_character<4u && (s->inside_mask&(1u<<r->leader_character));
    const Door *door=lead_inside?door_for(s->temporary):NULL;
    if(!door) return SudekiMpLanStorySplitUnparkMember(c)?1:-1; /* story house: return outside */
    if(house && house!=door) return -1;
    *m=(Member){.phase=LOADING,.door=door,.actor=r->actors[c],.epoch=s->epoch,.since=now?now:1u,.away_return=TRUE};
    if(!house) {house=door;house_loaded_by_us=FALSE;SudekiMpLanStoryObserverSetRemoteVisit(door->zone);}
    SudekiMpLogFormat("story_remote_interior event=away_return_entry zone=%s character=%u\r\n",door->zone,c);
    return 0; /* presence remains away until member_enter positively completes */
}
const char *SudekiMpLanStoryRemoteInteriorPlaceName(const char *zone) {
    if(zone && door_for(zone)) return "Kamo's shop";
    if(zone && !_stricmp(zone,"LICo_Frappe_Farm")) return "the farmhouse";
    return "the house";
}
/* This gate governs new entries only. Existing visits still drain/exit,
 * including visits accepted before PRESENCE changed. */
static BOOL entry_member_admitted(unsigned c,const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *scene,uint8_t remote_characters) {
    return SudekiMpLanStoryRuntimeCharacterAwayState(c)==0 && members[c].phase==IDLE &&
        c!=roster->leader_character && (remote_characters&(1u<<c)) &&
        (roster->available_mask&(1u<<c)) && !(scene->inside_mask&(1u<<c));
}
void SudekiMpLanStoryRemoteInteriorHostService(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *scene,uint8_t remote_characters,uint8_t input_fresh,uint32_t now) {
    if(!ready || !roster || !scene || roster->leader_character>=4u) return;
    BOOL lead_inside=scene->temporary[0] && (scene->inside_mask&(1u<<roster->leader_character));
    if(house_draining) {
        /* A released house that was still loading: unload once it can be;
         * past the load bound, leave it to the native world. */
        int unloaded=SudekiMpLanStoryAreaUnloadResident(house->zone);
        if(unloaded!=0) { if(unloaded>0) keep_exterior_render("house_unloaded"); house_forget("drained",unloaded); }
        else if(now-house_draining_since>LOAD_TIMEOUT_MS) house_forget("drain_timeout",0);
        return;
    }
    /* The lead joined the members' house: its lighting is now the lead's. */
    if(house && lead_inside && !_stricmp(house->zone,scene->temporary)) {
        lights_release(TRUE);
        if(house_loaded_by_us) {
            (void)activate_render(zone_render(house->zone),"lead_joined");
            house_loaded_by_us=FALSE;
        }
    }
    /* Native code may rewrite the formation snap flag while it is ours: keep
     * the latest native value as the one to restore, and keep it off. */
    if(snap_changed && snap_exact() && base[RVA_SNAP_FLAG]) {
        snap_before=base[RVA_SNAP_FLAG]; ((SnapSet)(base+RVA_SNAP_EXPORT))(0);
        SudekiMpLogWrite("story_remote_interior event=snap_rewritten policy=restore_latest\r\n");
    }
    for(unsigned c=0;c<4u;++c) if(members[c].phase!=IDLE) member_service(c,roster,scene,remote_characters,input_fresh,lead_inside,now);
    if(scene->phase!=SUDEKIMP_LAN_STORY_READY || SudekiMpLanStoryObserverHolding() || house_draining) return;
    /* Story houses stay vanilla: with the lead in a house that is not a
     * table shop, members stay outside. */
    if(lead_inside && !door_for(scene->temporary)) return;
    float lead_at[3]; BOOL lead_pos=position_of(roster->actors[roster->leader_character],lead_at);
    for(unsigned c=0;c<4u;++c) {
        float at[3],trigger[3],distance;
        if(!entry_member_admitted(c,roster,scene,remote_characters) || !position_of(roster->actors[c],at)) continue;
        BOOL close_by=FALSE; const Door *hit=NULL;
        for(unsigned d=0;d<DOORS;++d) {
            /* One house at a time (#83): the house in use, or the lead's. */
            if(house && house!=&doors[d]) continue;
            if(lead_inside && _stricmp(doors[d].zone,scene->temporary)) continue;
            entry_entities[d]=find_entity(doors[d].entry,entry_entities[d],&entry_scans[d],now);
            if(!entry_entities[d] || !position_of(entry_entities[d],trigger) || !reach(at,trigger,&distance)) continue;
            if(distance<TRIGGER_REARM) close_by=TRUE;
            if(distance<TRIGGER_REACH && !hit) {
                /* The lead about to use this door herself: a background load
                 * now (state 1) would make her native entry a no-op. */
                float lead_d;
                if(!house && !lead_inside && lead_pos && reach(lead_at,trigger,&lead_d) && lead_d<4.0f) continue;
                hit=&doors[d];
            }
        }
        if(!close_by) entry_armed[c]=TRUE;
        if(!hit || !entry_armed[c]) continue;
        Member *m=&members[c];
        memset(m,0,sizeof(*m));
        m->phase=LOADING; m->door=hit; m->actor=roster->actors[c]; m->epoch=scene->epoch; m->since=now?now:1u;
        if(!house) { house=hit; house_loaded_by_us=FALSE; SudekiMpLanStoryObserverSetRemoteVisit(hit->zone); }
        SudekiMpLogFormat("story_remote_interior event=entry zone=%s character=%u at=%.2f,%.2f,%.2f lead_inside=%u\r\n",
            hit->zone,c,(double)at[0],(double)at[1],(double)at[2],(unsigned)lead_inside);
    }
}
/* Runtime teardown, native thread: every member back outside, shared state
 * restored, a background house unloaded (or left to the native world when it
 * cannot be), the client fade balanced. */
static void client_reset(void);
void SudekiMpLanStoryRemoteInteriorReset(void) {
    if(!ready) return;
    for(unsigned c=0;c<4u;++c) finish(c,"teardown",TRUE);
    if(house) {
        int unloaded=house_loaded_by_us?SudekiMpLanStoryAreaUnloadResident(house->zone):-1;
        if(unloaded>0) keep_exterior_render("house_unloaded");
        house_forget("teardown",unloaded);
    }
    snap_restore(); lights_release(FALSE);
    SudekiMpLanStoryObserverClearRemoteInterior();
    SudekiMpLanStoryObserverSetRemoteVisit(NULL);
    SudekiMpLanStoryObserverNoteResidentHouse(NULL);
    client_reset();
    ready=FALSE;
}

static BOOL client_entered,client_want,client_reseat,client_exit_held;
static const Door *door_for(const char *zone);
/* Client door fade (native exports, cdecl): FadeToBlack(float) 0x1A9E0,
 * FadeFromBlack(float) 0x1AA60, Show/HideLoadingIcon() 0x103F10/0x103F50
 * (a counter; one show, one hide). */
enum { RVA_FADE_TO=0x1a9e0u, RVA_FADE_FROM=0x1aa60u, RVA_ICON_SHOW=0x103f10u, RVA_ICON_HIDE=0x103f50u,
    RVA_FADER=0x408d50u, RVA_ICON_COUNT=0x3c3070u, CLIENT_FADE_TIMEOUT_MS=8000u, CLIENT_SETTLE_MS=400u };
typedef void (__cdecl *FadeCall)(float);
typedef void (__cdecl *IconCall)(void);
static BOOL fade_exact(void) {
    static const uint8_t to[3]={0x51,0x8b,0x0d},from[2]={0x8b,0x0d},icon[1]={0xa1};
    return readable(base+RVA_FADE_TO,7u) && !memcmp(base+RVA_FADE_TO,to,3) && *(uint8_t **)(base+RVA_FADE_TO+3u)==base+RVA_FADER &&
        readable(base+RVA_FADE_FROM,6u) && !memcmp(base+RVA_FADE_FROM,from,2) && *(uint8_t **)(base+RVA_FADE_FROM+2u)==base+RVA_FADER &&
        readable(base+RVA_ICON_SHOW,6u) && !memcmp(base+RVA_ICON_SHOW,icon,1) && *(uint8_t **)(base+RVA_ICON_SHOW+1u)==base+RVA_ICON_COUNT &&
        readable(base+RVA_ICON_HIDE,6u) && !memcmp(base+RVA_ICON_HIDE,icon,1) && *(uint8_t **)(base+RVA_ICON_HIDE+1u)==base+RVA_ICON_COUNT &&
        base[RVA_ICON_SHOW+5u]==0x40u && base[RVA_ICON_HIDE+5u]==0x48u;
}
enum { FADE_NONE, FADE_OUT };
static unsigned client_fade;
static DWORD fade_started;
static BOOL fade_from_inside,fade_entry_armed,fade_exit_armed,fade_predicted;
/* After a fade-out ended by its timeout, the "host moved" fade stays quiet
 * until the wanted or local side changes: no endless black-out loop while
 * the two disagree for long (fail closed, the player keeps seeing). */
static BOOL fade_quiet,fade_quiet_want,fade_quiet_inside,fade_seen_want,fade_seen_inside;
static const uint8_t *client_entry_entities[DOORS],*client_exit_entity;
static DWORD client_entry_scans[DOORS],client_exit_scan,settled_since;
/* One-shot door cue for the runtime to play (native audio owner) and the
 * pending zone-name banner of the house this client just entered. */
static const char *client_cue;
static uint16_t banner_text;
static DWORD banner_armed_at;
static void banner_drop(void) { banner_text=0; SudekiMpLanStoryZoneBannerCancel(); }
static BOOL client_fade_out(const char *why,BOOL inside) {
    if(client_fade!=FADE_NONE || !fade_exact()) return FALSE;
    banner_drop();
    ((FadeCall)(base+RVA_FADE_TO))(1.5f);
    ((IconCall)(base+RVA_ICON_SHOW))();
    client_fade=FADE_OUT; fade_started=GetTickCount(); if(!fade_started) fade_started=1u;
    fade_from_inside=inside; settled_since=0; fade_predicted=strcmp(why,"host_moved")!=0;
    SudekiMpLogFormat("story_remote_interior event=client_fade_out why=%s inside=%u\r\n",why,(unsigned)inside);
    return TRUE;
}
static void client_fade_in(const char *why,float seconds) {
    if(client_fade!=FADE_OUT || !fade_exact()) return;
    ((FadeCall)(base+RVA_FADE_FROM))(seconds);
    ((IconCall)(base+RVA_ICON_HIDE))();
    client_fade=FADE_NONE;
    if(!strcmp(why,"timeout") || !strcmp(why,"watchdog")) {
        fade_quiet=TRUE; fade_quiet_want=fade_seen_want; fade_quiet_inside=fade_seen_inside;
    }
    SudekiMpLogFormat("story_remote_interior event=client_fade_in why=%s after_ms=%lu\r\n",why,(unsigned long)(GetTickCount()-fade_started));
}
/* Fade out the moment this player's character touches a door trigger the
 * host will act on (same reach/re-arm as the host), so the area change
 * happens behind black. Fade in once the local area settled. */
static void client_fade_service(const SudekiMpLanStoryScene *host,const SudekiMpLanStoryScene *native,
    unsigned chosen,BOOL want,uint32_t now) {
    static const SudekiMpCleanroomActor types[4]={SUDEKIMP_CLEANROOM_BUKI,SUDEKIMP_CLEANROOM_ELCO,
        SUDEKIMP_CLEANROOM_TAL,SUDEKIMP_CLEANROOM_AILISH};
    BOOL inside=native->phase==SUDEKIMP_LAN_STORY_READY && native->temporary[0];
    fade_seen_want=want; fade_seen_inside=inside;
    if(fade_quiet && (want!=fade_quiet_want || inside!=fade_quiet_inside)) fade_quiet=FALSE;
    if(client_fade==FADE_OUT) {
        BOOL changed=inside!=fade_from_inside && want==inside && native->phase==SUDEKIMP_LAN_STORY_READY;
        if(changed) { if(!settled_since) settled_since=now?now:1u; }
        else settled_since=0;
        if(changed && now-settled_since>=CLIENT_SETTLE_MS && now-fade_started>=1500u) {
            client_fade_in(inside?"entered":"left",inside?1.0f:0.5f);
            /* The door script shows the zone name once the fade-in is done. */
            const Door *entered=inside?door_for(native->temporary):NULL;
            if(entered && entered->name_text) { banner_text=entered->name_text; banner_armed_at=now?now:1u; }
        }
        else if(fade_predicted && want==fade_from_inside && now-fade_started>=4000u)
            client_fade_in("host_declined",0.5f);   /* our door touch, no host move */
        else if(now-fade_started>=CLIENT_FADE_TIMEOUT_MS)
            client_fade_in("timeout",0.5f);
        return;
    }
    if(chosen>=4u || native->phase!=SUDEKIMP_LAN_STORY_READY) return;
    float at[3],trigger[3],distance;
    void *actor=SudekiMpCleanroomEngineActorEntity(types[chosen]);
    if(!actor || !position_of(actor,at)) return;
    /* The host's own move (if this client missed the touch) still fades. */
    if(want!=inside) { if(!fade_quiet) client_fade_out("host_moved",inside); return; }
    if(!inside) {
        /* Same admission as the host: with an interior in use (by the lead
         * or by members) only that same shop; never a story house. */
        BOOL close_by=FALSE; const Door *hit=NULL;
        float lead_at[3]; BOOL lead_pos=host->leader_seat<4u &&
            position_of(SudekiMpCleanroomEngineActorEntity(types[host->leader_seat]),lead_at);
        for(unsigned d=0;d<DOORS;++d) {
            if(host->temporary[0] && _stricmp(doors[d].zone,host->temporary)) continue;
            client_entry_entities[d]=find_entity(doors[d].entry,client_entry_entities[d],&client_entry_scans[d],now);
            if(!client_entry_entities[d] || !position_of(client_entry_entities[d],trigger) || !reach(at,trigger,&distance)) continue;
            if(distance<TRIGGER_REARM) close_by=TRUE;
            if(distance<TRIGGER_REACH) {
                /* The host refuses while the lead stands at this door with no
                 * interior in use (her own entry would be a native no-op). */
                float lead_d;
                if(!host->temporary[0] && lead_pos && reach(lead_at,trigger,&lead_d) && lead_d<4.0f) continue;
                if(!hit) hit=&doors[d];
            }
        }
        if(!close_by) fade_entry_armed=TRUE;
        if(hit && fade_entry_armed) {
            fade_entry_armed=FALSE;
            if(client_fade_out("entry_trigger",FALSE)) client_cue=hit->enter_cue;
        }
        fade_exit_armed=FALSE; client_exit_entity=NULL;
        return;
    }
    const Door *d=door_for(native->temporary);
    if(!d) return;
    client_exit_entity=find_entity(d->exit,client_exit_entity,&client_exit_scan,now);
    if(!client_exit_entity || !position_of(client_exit_entity,trigger)) { memcpy(trigger,d->inside,sizeof(trigger)); trigger[2]+=1.2f; }
    if(!reach(at,trigger,&distance)) return;
    if(distance>TRIGGER_REARM) fade_exit_armed=TRUE;
    if(fade_exit_armed && distance<TRIGGER_REACH) {
        fade_exit_armed=FALSE;
        if(client_fade_out("exit_trigger",TRUE)) client_cue=d->exit_cue;
    }
}
static DWORD client_last_call;
/* The interior this client entered, kept so a late or missing area report
 * never reads as "leave" (CONFIRMED_LIVE 2026-10-17: the in-out loop). */
static char entered_zone[SUDEKIMP_STORY_AREA_NAME],entered_marker[SUDEKIMP_STORY_AREA_NAME];
static const Door *door_for(const char *zone) {
    for(unsigned d=0;d<DOORS;++d) if(!_stricmp(doors[d].zone,zone)) return &doors[d];
    return NULL;
}
/* The interior to enter for the host scene's temporary zone: a known door,
 * or the zone and marker of the host's own last native entry (area state). */
static BOOL client_target(const SudekiMpLanStoryScene *host,const SudekiMpStoryAreaState *area,
    const char **zone,const char **marker) {
    const Door *d=host && host->temporary[0]?door_for(host->temporary):NULL;
    if(d) { *zone=d->zone; *marker=d->marker; return TRUE; }
    if(host && host->temporary[0] && area && area->temporary_zone[0] && area->temporary_marker[0] &&
        !_stricmp(area->temporary_zone,host->temporary)) {
        *zone=area->temporary_zone; *marker=area->temporary_marker; return TRUE;
    }
    if(host && host->temporary[0] && entered_zone[0] && !_stricmp(entered_zone,host->temporary)) {
        *zone=entered_zone; *marker=entered_marker; return TRUE;
    }
    return FALSE;
}
static BOOL client_wants(const SudekiMpLanStoryScene *host,const SudekiMpStoryAreaState *area,unsigned chosen) {
    const char *z,*m;
    return host && chosen<4u && host->phase==SUDEKIMP_LAN_STORY_READY && host->temporary[0] &&
        (host->inside_mask&(1u<<chosen)) && client_target(host,area,&z,&m);
}
/* Consecutive local entries that the native refused or never confirmed;
 * after CLIENT_ENTRY_LIMIT the client stops trying (and stops re-fading)
 * until the host stops marking it inside. Fail closed, logged once. */
enum { CLIENT_ENTRY_LIMIT=3u, CLIENT_WAIT_LIMIT=10u };
static unsigned client_entry_failures,client_entry_waits;
static BOOL client_gave_up,client_world_ok=TRUE;
static void client_give_up(const char *zone,const char *why) {
    if(client_gave_up) return;
    client_gave_up=TRUE;
    SudekiMpLogFormat("story_remote_interior event=client_enter_gave_up zone=%s reason=%s failures=%u waits=%u policy=stay_outside_host_ends_visit\r\n",
        zone?zone:"-",why,client_entry_failures,client_entry_waits);
}
/* client_confirmed: this client's local entry settled inside entered_zone
 * (READY, native temporary = entered_zone); kept until it leaves. Only an
 * unconfirmed entry can time out, so a transient non-READY sample never
 * reads as "never arrived" for a client already inside (review 2026-10-17). */
static BOOL client_confirmed;
static DWORD client_entry_at,client_unsettled_since,client_exit_unready_since;
static unsigned client_exit_refusals,client_exit_waits;
/* A wanted visit that never settles, READY or not, gives up after this. */
enum { CLIENT_GIVE_UP_MS=45000u };
static void client_left(void) {
    banner_drop();
    client_entered=client_confirmed=FALSE; entered_zone[0]=entered_marker[0]=0;
    client_exit_refusals=client_exit_waits=0;
}
void SudekiMpLanStoryRemoteInteriorClientService(const SudekiMpLanStoryScene *host,
    const SudekiMpStoryAreaState *area,const SudekiMpLanStoryScene *native,unsigned chosen,uint32_t now) {
    if(!ready || !host || !native) return;
    /* Only a READY host scene decides; a missed or loading frame keeps the
     * previous decision (it is unknown, not "leave"). */
    BOOL want=host->phase==SUDEKIMP_LAN_STORY_READY?client_wants(host,area,chosen):client_want;
    /* A native modal of this client (its own shop screen) is open in the
     * house: no fade or exit under it; the owner closes it first. */
    if(client_exit_held && !want && client_entered) {
        static BOOL held_logged;
        if(!held_logged) { held_logged=TRUE; SudekiMpLogWrite("story_remote_interior event=client_exit_held reason=native_modal\r\n"); }
        client_want=want; return;
    }
    if(!want) { client_entry_failures=client_entry_waits=0; client_gave_up=FALSE; client_unsettled_since=0; }
    else client_exit_refusals=client_exit_waits=0;
    if(want || !client_entered) client_exit_unready_since=0;
    client_world_ok=!want || !native->world[0] || !strcmp(native->world,host->world);
    client_fade_service(host,native,chosen,client_gave_up || !client_world_ok?FALSE:want,now);
    client_want=want;
    BOOL native_ready=native->phase==SUDEKIMP_LAN_STORY_READY;
    BOOL inside=native_ready && native->temporary[0]!=0;
    BOOL in_entered=inside && entered_zone[0] && !_stricmp(native->temporary,entered_zone);
    if(client_entered && in_entered && !client_confirmed) {
        client_confirmed=TRUE; client_unsettled_since=0;
        SudekiMpLogFormat("story_remote_interior event=client_enter_confirmed zone=%s after_ms=%lu\r\n",
            entered_zone,(unsigned long)(now-client_entry_at));
    }
    /* A local entry that settled late (after its unconfirmed timeout, or
     * after giving up) while the host still wants this player there: it is
     * this client's to leave again. */
    if(want && in_entered && !client_entered) {
        client_entered=client_confirmed=TRUE; client_gave_up=FALSE;
        client_entry_failures=client_entry_waits=0; client_unsettled_since=0;
        SudekiMpLogFormat("story_remote_interior event=client_enter_late zone=%s policy=adopt\r\n",entered_zone);
    }
    /* Zone-name banner once the fade-in is over and this client is settled
     * in the house it entered (vanilla: after Fadein(1.0) completes). */
    if(banner_text) {
        if(client_fade==FADE_NONE && client_confirmed && in_entered && now-banner_armed_at>=1100u) {
            SudekiMpLanStoryZoneBannerRequest(banner_text,now); banner_text=0;
        } else if(now-banner_armed_at>5000u) {
            banner_text=0;
            SudekiMpLogWrite("story_remote_interior event=zone_banner_expired policy=skip\r\n");
        }
    }
    /* Whole-visit bound, READY or not (a client stuck loading still ends). */
    if(want && !client_confirmed && !client_gave_up) {
        if(!client_unsettled_since) client_unsettled_since=now?now:1u;
        else if(now-client_unsettled_since>=CLIENT_GIVE_UP_MS)
            client_give_up(entered_zone[0]?entered_zone:host->temporary,"never_settled");
    }
    /* One accepted entry that does not settle in time is retried (counted). */
    if(want && client_entered && !client_confirmed && now-client_entry_at>=CLIENT_ENTRY_TIMEOUT_MS) {
        client_entered=FALSE; client_last_call=now?now:1u;
        if(++client_entry_failures>=CLIENT_ENTRY_LIMIT) client_give_up(entered_zone,"unconfirmed");
        else SudekiMpLogWrite("story_remote_interior event=client_enter_unconfirmed policy=retry\r\n");
    }
    if(!native_ready) {
        /* The host ended the visit while this client's area is not READY:
         * hold the exit for a bound, then stop holding (the area follower
         * resumes; a later READY settle inside entered_zone still exits). */
        if(!want && client_entered) {
            if(!client_exit_unready_since) client_exit_unready_since=now?now:1u;
            else if(now-client_exit_unready_since>=CLIENT_ENTRY_TIMEOUT_MS) {
                client_entered=client_confirmed=FALSE; client_exit_unready_since=0;
                SudekiMpLogFormat("story_remote_interior event=client_exit_unready zone=%s policy=release_hold\r\n",
                    entered_zone[0]?entered_zone:"-");
            }
        }
        return;
    }
    static BOOL last_inside;
    /* The local area settled into or out of a house: reseat the camera. */
    if(inside!=last_inside) { last_inside=inside; client_reseat=TRUE; }
    if(client_last_call && now-client_last_call<CLIENT_RETRY_MS) return;
    if(want && !inside && !client_entered && !client_gave_up && !strcmp(native->world,host->world)) {
        const char *zone=NULL,*marker=NULL;
        if(!client_target(host,area,&zone,&marker)) return;
        int r=SudekiMpLanStoryAreaEnterTemporary(zone,marker);
        client_last_call=now?now:1u;
        SudekiMpLogFormat("story_remote_interior event=client_enter zone=%s marker=%s result=%d\r\n",zone,marker,r);
        if(r>0) {
            client_entered=TRUE; client_confirmed=FALSE; client_entry_at=now?now:1u;
            if(zone!=entered_zone) { strncpy(entered_zone,zone,sizeof(entered_zone)-1u); entered_zone[sizeof(entered_zone)-1u]=0; }
            if(marker!=entered_marker) { strncpy(entered_marker,marker,sizeof(entered_marker)-1u); entered_marker[sizeof(entered_marker)-1u]=0; }
        } else if(r<0 && ++client_entry_failures>=CLIENT_ENTRY_LIMIT) client_give_up(zone,"refused");
        else if(!r && ++client_entry_waits>=CLIENT_WAIT_LIMIT) client_give_up(zone,"not_ready");
    } else if(want && !inside && !client_entered && !client_gave_up && strcmp(native->world,host->world)) {
        client_last_call=now?now:1u;
        if(++client_entry_waits>=CLIENT_WAIT_LIMIT) client_give_up(host->temporary,"world_mismatch");
    } else if(want && inside) {
        client_entry_failures=client_entry_waits=0;
    } else if(!want && (client_entered || in_entered)) {
        int r=inside?SudekiMpLanStoryAreaExitTemporary():1;
        client_last_call=now?now:1u;
        SudekiMpLogFormat("story_remote_interior event=client_exit native_inside=%u result=%d\r\n",(unsigned)inside,r);
        if(r>0) client_left();
        else if((r<0 && ++client_exit_refusals>=CLIENT_ENTRY_LIMIT) || (!r && ++client_exit_waits>=CLIENT_WAIT_LIMIT)) {
            /* Fail closed: stop holding the follower and stop retrying; the
             * local interior stays as it is (logged once). */
            SudekiMpLogFormat("story_remote_interior event=client_exit_gave_up zone=%s refusals=%u waits=%u policy=release_hold\r\n",
                entered_zone[0]?entered_zone:"-",client_exit_refusals,client_exit_waits);
            client_left();
        }
    }
}
/* Every client frame, independent of host-scene availability: a fade-out
 * never outlives its timeout (balanced FadeFromBlack + HideLoadingIcon). */
void SudekiMpLanStoryRemoteInteriorClientTick(uint32_t now) {
    if(ready && client_fade==FADE_OUT && now-fade_started>=CLIENT_FADE_TIMEOUT_MS) client_fade_in("watchdog",0.5f);
}
static void client_reset(void) {
    if(client_fade==FADE_OUT) client_fade_in("teardown",0.0f);
    client_fade=FADE_NONE;
    client_entered=client_want=client_reseat=client_gave_up=FALSE; client_entry_failures=client_entry_waits=0;
    client_world_ok=TRUE;
    client_left(); client_last_call=client_entry_at=client_unsettled_since=client_exit_unready_since=0;
    fade_quiet=FALSE; client_cue=NULL; banner_drop(); client_exit_held=FALSE;
}
const void *SudekiMpLanStoryRemoteInteriorHostHouseData(void) {
    if(!ready || !house || !house_loaded_by_us || house_draining || SudekiMpLanStoryAreaZoneState(house->zone)!=2) return NULL;
    return zone_data(house->zone,NULL);
}
const void *SudekiMpLanStoryRemoteInteriorClientShopData(void) {
    if(!ready || !client_entered || !client_confirmed || !entered_zone[0] || !door_for(entered_zone)) return NULL;
    return zone_data(NULL,entered_zone);
}
const char *SudekiMpLanStoryRemoteInteriorClientHouseCamera(void) {
    return SudekiMpLanStoryRemoteInteriorClientShopData()?door_for(entered_zone)->camera:NULL;
}
const char *SudekiMpLanStoryRemoteInteriorClientShopZone(void) {
    return SudekiMpLanStoryRemoteInteriorClientShopData()?door_for(entered_zone)->zone:NULL;
}
BOOL SudekiMpLanStoryRemoteInteriorClientWantsOut(void) {
    return ready && client_entered && !client_want;
}
void SudekiMpLanStoryRemoteInteriorClientHoldExit(BOOL hold) { client_exit_held=hold; }
const void *SudekiMpLanStoryRemoteInteriorEntityNamed(const char *name,const void *cached,DWORD *last_scan,uint32_t now) {
    return ready && name && last_scan?find_entity(name,cached,last_scan,now):NULL;
}
BOOL SudekiMpLanStoryRemoteInteriorEntityPosition(const void *entity,float out[3]) {
    return ready && out && position_of(entity,out);
}
BOOL SudekiMpLanStoryRemoteInteriorClientTakeDoorCue(const char **cue) {
    if(!client_cue || !cue) return FALSE;
    *cue=client_cue; client_cue=NULL; return TRUE;
}
BOOL SudekiMpLanStoryRemoteInteriorClientTakeReseat(void) {
    BOOL r=client_reseat; client_reseat=FALSE; return r;
}
/* The area follower stands down only while this client holds (or is about
 * to make) a local entry it can make: not after giving up, and not while
 * its world differs from the host's (the follower must catch up first). */
BOOL SudekiMpLanStoryRemoteInteriorClientHolds(void) {
    return client_entered || (client_want && !client_gave_up && client_world_ok);
}
