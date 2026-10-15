#include "hooks/lan_story_test_start.h"

#include "engine/build_identity.h"
#include "engine/log.h"
#include "hooks/lan_story_area_follow.h"
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
static BOOL pending;
static DWORD ready_since,zone_since;
static char start_zone[40];
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
    pending=TRUE; ready_since=0; zone_since=0;
    SudekiMpLogFormat("story_test_start event=armed leader=%.1f,%.1f,%.1f follower=%.1f,%.1f,%.1f\r\n",
        (double)leader[0],(double)leader[1],(double)leader[2],
        (double)follower[0],(double)follower[1],(double)follower[2]);
    return TRUE;
}

static BOOL place(void *actor,const float xyz[3]) {
    uint8_t *position;
    if(!readable(actor,0x48u) || !readable(position=*(uint8_t **)((uint8_t *)actor+0x44u),0x24u) ||
        *(void **)position!=base+RVA_POSITION_VTABLE) return FALSE;
    ((PositionSet)(base+RVA_POSITION_SET))(position,xyz);
    return TRUE;
}

void SudekiMpLanStoryTestStartService(const SudekiMpLanStoryNativeRoster *roster) {
    if(!pending || !base || !roster || roster->leader_character>=4u ||
        !(roster->available_mask&(1u<<roster->leader_character))) { ready_since=0; return; }
    DWORD now=GetTickCount();
    /* Let the load fade and first party placement settle before moving. */
    if(!ready_since) { ready_since=now?now:1u; return; }
    if(now-ready_since<SETTLE_MS) return;
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
    pending=FALSE;
    unsigned placed=0,refused=0;
    for(unsigned c=0;c<4u;++c) if(roster->available_mask&(1u<<c)) {
        const float *xyz=c==roster->leader_character?leader_xyz:follower_xyz;
        if(place(roster->actors[c],xyz)) ++placed; else ++refused;
    }
    SudekiMpLogFormat("story_test_start event=placed leader=%u placed=%u refused=%u\r\n",
        roster->leader_character,placed,refused);
    for(unsigned c=0;c<4u;++c) if(roster->available_mask&(1u<<c)) {
        static const SudekiMpCleanroomActor types[4]={SUDEKIMP_CLEANROOM_BUKI,SUDEKIMP_CLEANROOM_ELCO,
            SUDEKIMP_CLEANROOM_TAL,SUDEKIMP_CLEANROOM_AILISH};
        float fraction=c==roster->leader_character?leader_health_fraction:follower_health_fraction;
        if(fraction<=0.0f) continue;
        float hp=0.0f,sp=0.0f; BOOL ok=FALSE; float target=0.0f;
        if(SudekiMpCleanroomEngineActorResources(types[c],&hp,&sp) && hp>1.0f) {
            target=floorf(hp*fraction); if(target<1.0f) target=1.0f;
            ok=SudekiMpCleanroomEngineSetActorResources(types[c],target,sp);
        }
        SudekiMpLogFormat("story_test_start event=%s fraction=%.2f character=%u hp=%.0f->%.0f ok=%u\r\n",
            c==roster->leader_character?"leader_health":"follower_health",(double)fraction,c,(double)hp,(double)target,(unsigned)ok);
    }
}
