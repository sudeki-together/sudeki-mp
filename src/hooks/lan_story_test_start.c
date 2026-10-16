#include "hooks/lan_story_test_start.h"

#include "engine/build_identity.h"
#include "engine/log.h"
#include "hooks/lan_story_area_follow.h"
#include "hooks/lan_story_split.h"
#include "cleanroom/engine.h"
#include <math.h>

#include <stdint.h>
#include <string.h>

enum { RVA_POSITION_SET=0x3050u, RVA_POSITION_VTABLE=0x2cdefcu, SETTLE_MS=1500u };
/* FLD [ECX+0x18]; FLD [EDX]; FUCOMPP; FNSTSW AX; TEST AH,0x44; JP +0x1e */
static const uint8_t setter_entry[14]={0xd9,0x41,0x18,0xd9,0x02,0xda,0xe9,0xdf,0xe0,0xf6,0xc4,0x44,0x7a,0x1e};
typedef void (__attribute__((fastcall)) *PositionSet)(void *,const float *);

static uint8_t *base;
static float leader_xyz[3],follower_xyz[3];
static BOOL pending,health_only;
static DWORD ready_since,zone_since;
static char start_zone[40];
static char temporary_zone[40],temporary_marker[96];
static float temporary_leader_xyz[3];
static unsigned temporary_phase;
static DWORD temporary_since;
static SudekiMpLanStoryNativeRoster temporary_seed;
static float leader_health_fraction,follower_health_fraction;
void SudekiMpLanStoryTestStartSetLeaderHealth(float fraction) {
    leader_health_fraction=isfinite(fraction) && fraction>0.0f && fraction<1.0f?fraction:0.0f;
}
void SudekiMpLanStoryTestStartSetFollowerHealth(float fraction) {
    follower_health_fraction=isfinite(fraction) && fraction>0.0f && fraction<1.0f?fraction:0.0f;
}
void SudekiMpLanStoryTestStartSetZone(const char *zone) {
    memset(start_zone,0,sizeof(start_zone));
    if(zone) strncpy(start_zone,zone,sizeof(start_zone)-1u);
}
BOOL SudekiMpLanStoryTestStartSetTemporary(const char *zone,const char *marker,const float leader[3]) {
    memset(temporary_zone,0,sizeof(temporary_zone));
    memset(temporary_marker,0,sizeof(temporary_marker));
    if(!zone || !zone[0] || strlen(zone)>=sizeof(temporary_zone) ||
        !marker || !marker[0] || strlen(marker)>=sizeof(temporary_marker) || !leader ||
        !isfinite(leader[0]) || !isfinite(leader[1]) || !isfinite(leader[2])) return FALSE;
    strcpy(temporary_zone,zone); strcpy(temporary_marker,marker);
    memcpy(temporary_leader_xyz,leader,sizeof(temporary_leader_xyz));
    return TRUE;
}

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}

BOOL SudekiMpLanStoryTestStartConfigure(HMODULE image,const float leader[3],const float follower[3]) {
    uint8_t *b=(uint8_t *)image;
    if(!b || !leader || !follower || !SudekiMpCheckLoadedExecutable(image) ||
        memcmp(b+RVA_POSITION_SET,setter_entry,sizeof(setter_entry))) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    base=b; memcpy(leader_xyz,leader,sizeof(leader_xyz)); memcpy(follower_xyz,follower,sizeof(follower_xyz));
    pending=TRUE; health_only=FALSE; ready_since=0; zone_since=0; temporary_phase=0; temporary_since=0;
    memset(&temporary_seed,0,sizeof(temporary_seed));
    SudekiMpLogFormat("story_test_start event=armed leader=%.1f,%.1f,%.1f follower=%.1f,%.1f,%.1f\r\n",
        (double)leader[0],(double)leader[1],(double)leader[2],
        (double)follower[0],(double)follower[1],(double)follower[2]);
    return TRUE;
}

BOOL SudekiMpLanStoryTestStartConfigureHealthOnly(HMODULE image) {
    /* This mode must not inherit even a partially armed placement/TEMP.
     * Configuration is startup-only, before any native task can be entered. */
    base=NULL; pending=health_only=FALSE; ready_since=zone_since=0;
    temporary_phase=0; temporary_since=0;
    memset(leader_xyz,0,sizeof(leader_xyz)); memset(follower_xyz,0,sizeof(follower_xyz));
    memset(start_zone,0,sizeof(start_zone)); memset(temporary_zone,0,sizeof(temporary_zone));
    memset(temporary_marker,0,sizeof(temporary_marker));
    memset(temporary_leader_xyz,0,sizeof(temporary_leader_xyz));
    memset(&temporary_seed,0,sizeof(temporary_seed));
    if(!image || !SudekiMpCheckLoadedExecutable(image) ||
        (leader_health_fraction<=0.0f && follower_health_fraction<=0.0f)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    base=(uint8_t *)image; health_only=pending=TRUE;
    SudekiMpLogFormat("story_test_start event=health_only_armed leader_fraction=%.2f follower_fraction=%.2f placement=disabled\r\n",
        (double)leader_health_fraction,(double)follower_health_fraction);
    return TRUE;
}

static BOOL place(void *actor,const float xyz[3]) {
    uint8_t *position;
    if(!readable(actor,0x48u) || !readable(position=*(uint8_t **)((uint8_t *)actor+0x44u),0x24u) ||
        *(void **)position!=base+RVA_POSITION_VTABLE) return FALSE;
    ((PositionSet)(base+RVA_POSITION_SET))(position,xyz);
    return TRUE;
}

static void apply_health(const SudekiMpLanStoryNativeRoster *roster) {
    static const SudekiMpCleanroomActor types[4]={SUDEKIMP_CLEANROOM_BUKI,SUDEKIMP_CLEANROOM_ELCO,
        SUDEKIMP_CLEANROOM_TAL,SUDEKIMP_CLEANROOM_AILISH};
    for(unsigned c=0;c<4u;++c) if(roster->available_mask&(1u<<c)) {
        float fraction=c==roster->leader_character?leader_health_fraction:follower_health_fraction;
        if(fraction<=0.0f) continue;
        float hp=0.0f,sp=0.0f,observed_hp=0.0f,observed_sp=0.0f;
        BOOL ok=FALSE; float target=0.0f;
        if(roster->actors[c] && SudekiMpLanStoryObserverNativeRosterExact(roster) &&
            SudekiMpCleanroomEngineActorEntity(types[c])==roster->actors[c] &&
            SudekiMpCleanroomEngineActorResources(types[c],&hp,&sp) && hp>1.0f &&
            SudekiMpLanStoryObserverNativeRosterExact(roster) &&
            SudekiMpCleanroomEngineActorEntity(types[c])==roster->actors[c]) {
            target=floorf(hp*fraction); if(target<1.0f) target=1.0f;
            /* The setter may change HP before reporting an SP failure. The
             * caller consumes the one-shot before entering, so never halve
             * again merely because the final observation failed. */
            ok=SudekiMpCleanroomEngineSetActorResources(types[c],target,sp) &&
                SudekiMpCleanroomEngineActorResources(types[c],&observed_hp,&observed_sp) &&
                observed_hp==target && observed_sp==sp &&
                SudekiMpLanStoryObserverNativeRosterExact(roster) &&
                SudekiMpCleanroomEngineActorEntity(types[c])==roster->actors[c];
        }
        SudekiMpLogFormat("story_test_start event=%s fraction=%.2f character=%u hp=%.0f->%.0f ok=%u mode=%s\r\n",
            c==roster->leader_character?"leader_health":"follower_health",(double)fraction,c,(double)hp,(double)target,
            (unsigned)ok,health_only?"health_only":"placement");
    }
}

static void service_temporary(const SudekiMpLanStoryNativeRoster *roster,DWORD now) {
    if(now-temporary_since>60000u) {
        pending=FALSE;
        /* Stop this test aid only. Never cancel an in-flight native TEMP. */
        SudekiMpLogFormat("story_test_start event=temporary_timeout zone=%s phase=%u native_task=retained\r\n",
            temporary_zone,temporary_phase);
        return;
    }
    /* The original actor identities must survive the native transition. An
     * observer-exact roster is required again at the actual placement. TEMP
     * preserves its exterior epoch; the earlier exterior teleport may change
     * epochs while its ordinary position-based zone swap settles. */
    if(roster->world!=temporary_seed.world || roster->group!=temporary_seed.group ||
        roster->leader_character!=temporary_seed.leader_character ||
        roster->available_mask!=temporary_seed.available_mask ||
        (temporary_phase==2u && roster->epoch!=temporary_seed.epoch) ||
        memcmp(roster->actors,temporary_seed.actors,sizeof(roster->actors)) ||
        !SudekiMpLanStoryObserverNativeRosterExact(roster)) return;
    if(temporary_phase==1u) {
        /* The exterior position setter precedes native collision/tracking.
         * Let the sector and current exterior settle before native TEMP
         * records its return context. Split admission also waits for Tal's
         * remote lease; absence is not permission to move the whole party. */
        if(now-temporary_since<SETTLE_MS || SudekiMpLanStoryAreaZoneState(start_zone)!=3) return;
        int result=SudekiMpLanStoryObserverTestEnterTemporary(temporary_zone,temporary_marker);
        if(result<0) {
            pending=FALSE;
            SudekiMpLogFormat("story_test_start event=temporary_refused zone=%s\r\n",temporary_zone);
        } else if(result>0) {
            temporary_seed.epoch=roster->epoch;
            temporary_phase=2u;
            SudekiMpLogFormat("story_test_start event=temporary_pending zone=%s\r\n",temporary_zone);
        }
        return;
    }
    if(!SudekiMpLanStoryObserverTestTemporarySettled(temporary_zone)) return;
    pending=FALSE;
    BOOL placed=place(roster->actors[roster->leader_character],temporary_leader_xyz);
    SudekiMpLogFormat("story_test_start event=temporary_placed zone=%s leader=%u placed=%u xyz=%.2f,%.2f,%.2f followers=unchanged\r\n",
        temporary_zone,roster->leader_character,(unsigned)placed,(double)temporary_leader_xyz[0],
        (double)temporary_leader_xyz[1],(double)temporary_leader_xyz[2]);
}

void SudekiMpLanStoryTestStartService(const SudekiMpLanStoryNativeRoster *roster) {
    if(!pending || !base || !roster || roster->leader_character>=4u ||
        !(roster->available_mask&(1u<<roster->leader_character))) { ready_since=0; return; }
    DWORD now=GetTickCount();
    if(!health_only && temporary_phase) { service_temporary(roster,now); return; }
    /* Let the load fade and first party placement settle before moving. */
    if(!ready_since) { ready_since=now?now:1u; return; }
    if(now-ready_since<SETTLE_MS) return;
    if(health_only) {
        if(!SudekiMpLanStoryObserverNativeRosterExact(roster)) return;
        pending=FALSE;
        apply_health(roster);
        return;
    }
    if(temporary_zone[0] && !SudekiMpLanStorySplitTestCanEnterTemporary(
        temporary_zone,roster->leader_character)) return;
    if(start_zone[0]) {
        /* Background-load the zone with the native SwitchZoneNOW, then place
         * the party inside it once resident: the native position-based swap
         * makes it current, as walking in does. */
        int state=SudekiMpLanStoryAreaZoneState(start_zone);
        if(!zone_since) zone_since=now?now:1u;
        if(state<0 || now-zone_since>60000u) {
            pending=FALSE;
            SudekiMpLogFormat("story_test_start event=zone_refused zone=%s state=%d\r\n",start_zone,state);
            return;
        }
        if(state<2) {
            if(!state) (void)SudekiMpLanStoryAreaGoTo(start_zone,now);
            ready_since=now?now:1u; return;
        }
        if(now-ready_since<500u) return;
        SudekiMpLogFormat("story_test_start event=zone_resident zone=%s state=%d\r\n",start_zone,state);
    }
    if(!SudekiMpLanStoryObserverNativeRosterExact(roster)) return;
    pending=FALSE;
    unsigned placed=0,refused=0;
    for(unsigned c=0;c<4u;++c) if(roster->available_mask&(1u<<c)) {
        const float *xyz=c==roster->leader_character?leader_xyz:follower_xyz;
        if(place(roster->actors[c],xyz)) ++placed; else ++refused;
    }
    SudekiMpLogFormat("story_test_start event=placed leader=%u placed=%u refused=%u\r\n",
        roster->leader_character,placed,refused);
    if(temporary_zone[0] && start_zone[0] && !refused) {
        temporary_seed=*roster; temporary_since=now?now:1u;
        temporary_phase=1u; pending=TRUE;
        SudekiMpLogFormat("story_test_start event=temporary_armed zone=%s marker=%s\r\n",
            temporary_zone,temporary_marker);
    }
    apply_health(roster);
}
