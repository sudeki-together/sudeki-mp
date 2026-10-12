#include "hooks/lan_story_shots.h"
#include "hooks/lan_arena_ranged_aim.h"
#include "engine/weapon_activation_abi.h"
#include "engine/skill_activation_abi.h"
#include "engine/log.h"
#include "hooks/lan_story_cast.h"
#include "hooks/lan_party_projectile_lifetime.h"
#include <math.h>
#include <string.h>

static uint8_t *base;
static DWORD thread;
static volatile LONG admission;
static BOOL installed;
static struct { void *actor;uint32_t epoch,generation,receipt;float target[3]; } remote_aim[4];
static float actor_position[4][3];
static SudekiMpLanStoryNativeRoster owner;
static uint32_t generations[4];
static SudekiMpLanStoryShots journal[4];
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a+n>=a && VirtualQuery(p,&m,sizeof(m)) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL emission_owner_exact(void *actor,void *manager,unsigned c) {
    if(!installed || !InterlockedCompareExchange(&admission,0,0) || !thread || thread!=GetCurrentThreadId() ||
        c>=4 || !generations[c] || owner.actors[c]!=actor ||
        *(void **)(base+0x408d10u)!=owner.world ||
        *(void **)(base+0x408d94u)!=owner.group ||
        *(void **)(base+0x408da4u)!=owner.controller) return FALSE;
    uint8_t *world=owner.world,*group=owner.group,*controller=owner.controller,*a=actor,*m=manager;
    if(!readable(world,0x39bu) || !readable(group,0xd0u) || !readable(controller,0x24cu) ||
        *(void **)(world+0xcu)!=owner.descriptor || *(void **)(world+0x14u) ||
        !world[0x39au] || (!world[0x399u] &&
            (!readable(owner.descriptor,0x38u) || *(uint32_t *)((uint8_t *)owner.descriptor+0x34u)!=3u)) ||
        *(void **)(controller+0x248u)!=owner.actors[owner.leader_character] ||
        !readable(a,0xc4u) || *(void **)a!=base+(c==1u?0x2d66fcu:0x2d555cu) ||
        *(void **)(a+0x94u)!=owner.ai[c] || !readable(owner.ai[c],0x14u) ||
        *(void **)((uint8_t *)owner.ai[c]+0x10u)!=actor ||
        *(void **)(a+0xbcu)!=manager || !readable(m,0xe0u) ||
        *(void **)m!=base+0x2d4c8cu || *(void **)(m+0x10u)!=actor) return FALSE;
    unsigned count=*(unsigned *)(group+0xccu),mask=0;
    if(!count || count>4u) return FALSE;
    for(unsigned i=0;i<count;++i) {
        void *member=*(void **)(group+0x90u+i*12u); unsigned k=0;
        for(;k<4 && (!member || member!=owner.actors[k]);++k) {}
        if(k==4 || (mask&(1u<<k)) || (!i && k!=owner.leader_character)) return FALSE;
        mask|=1u<<k;
    }
    return mask==owner.available_mask;
}
static BOOL directed(void *actor,float target[3]) {
    unsigned c=actor==owner.actors[1]?1u:actor==owner.actors[3]?3u:4u;
    if(c>=4u || !target || !remote_aim[c].receipt || remote_aim[c].actor!=actor ||
        remote_aim[c].epoch!=owner.epoch || remote_aim[c].generation!=generations[c] ||
        (int32_t)(GetTickCount()-remote_aim[c].receipt)>100 || c==owner.leader_character ||
        !readable(actor,0xc0u) ||
        !emission_owner_exact(actor,*(void **)((uint8_t *)actor+0xbcu),c)) return FALSE;
    memcpy(target,remote_aim[c].target,12);return TRUE;
}
BOOL SudekiMpLanStoryShotsAim(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,unsigned c,const SudekiMpLanStoryRanged *input,uint32_t receipt) {
    if(!installed || thread!=GetCurrentThreadId() || c>=4u) return FALSE;
    memset(&remote_aim[c],0,sizeof(remote_aim[c]));
    if(!input) return TRUE;
    if(!r || (c!=1u && c!=3u) || c==r->leader_character || !receipt ||
        (int32_t)(GetTickCount()-receipt)>100 || !SudekiMpLanStoryRangedValid(input) ||
        input->fence.character!=c || input->fence.epoch!=r->epoch ||
        input->fence.actor_generation!=generations[c] || owner.epoch!=r->epoch ||
        owner.actors[c]!=r->actors[c] || !SudekiMpLanStoryObserverRosterStillExact(w,r)) return FALSE;
    float distance=0;
    for(unsigned i=0;i<3;++i) {
        float offset=input->target[i]-100.0f*input->direction[i]-actor_position[c][i];
        distance+=offset*offset;
    }
    /* Story orbit uses the native exploration distance, up to 30 units.
     * Bound the supplied camera origin to that actor; no remote positions. */
    if(!isfinite(distance) || distance>32.0f*32.0f) return FALSE;
    remote_aim[c].actor=r->actors[c];remote_aim[c].epoch=r->epoch;
    remote_aim[c].generation=generations[c];remote_aim[c].receipt=receipt;
    memcpy(remote_aim[c].target,input->target,12);return TRUE;
}
static void emitted(void *actor,void *manager,const float origin[3],const float direction[3]) {
    unsigned c=actor==owner.actors[1]?1u:actor==owner.actors[3]?3u:4u;
    SudekiMpElcoWeaponObservation weapon; SudekiMpCharacterSkillState skill;
    if(!emission_owner_exact(actor,manager,c) ||
        !SudekiMpObserveCharacterSkill(actor,&skill) || skill.active ||
        !SudekiMpObserveRangedWeapon(actor,c==1u?0x0eu:0x01u,&weapon) ||
        !isfinite(weapon.charge) || weapon.charge<weapon.required_charge ||
        weapon.charge<0 || weapon.charge>65535.0f/256.0f || journal[c].latest==UINT32_MAX) return;
    SudekiMpLanStoryShot event={.sequence=journal[c].latest+1u,.host_tick=GetTickCount(),
        .item=weapon.item,.pre_charge_q8=(uint16_t)lroundf(weapon.charge*256.0f)};
    memcpy(event.origin,origin,sizeof(event.origin));
    if(!SudekiMpLanAimNormalize(direction,event.direction) ||
        !emission_owner_exact(actor,manager,c) || !SudekiMpLanStoryShotsAppend(&journal[c],c,&event)) return;
    SudekiMpLanPartyEffectOwner namespace;
    if(SudekiMpLanStoryCastShotOwner(actor,&namespace)) {
        SudekiMpLanPartyProjectileTag tag={namespace.session,event.sequence,1u,(uint8_t)c,event.item,actor};
        if(!SudekiMpLanPartyProjectileLifetimeCaptureOwned(&tag,&namespace,manager))
            SudekiMpLogWrite("story_shots event=projectile_capture_failed\r\n");
    }
    SudekiMpLogFormat("story_shots event=host_emission character=%u generation=%lu sequence=%lu item=%u tick=%lu\r\n",
        c,(unsigned long)generations[c],(unsigned long)event.sequence,event.item,(unsigned long)event.host_tick);
}
BOOL SudekiMpLanStoryShotsInstall(HMODULE image) {
    if(base || installed || !image) return FALSE;
    base=(uint8_t *)image;
    if(!SudekiMpLanAimObserveDirectedEmissionsInstall(image,emitted,directed)) { base=NULL; return FALSE; }
    installed=TRUE; return TRUE;
}
void SudekiMpLanStoryShotsCloseAdmission(void) {
    InterlockedExchange(&admission,0); /* retained hook becomes a no-op */
    memset(remote_aim,0,sizeof(remote_aim));
}
BOOL SudekiMpLanStoryShotsCapture(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene,
    SudekiMpLanStoryFrame *frame) {
    SudekiMpLanStoryNativeRoster fresh;
    if(!installed || !frame || (thread && thread!=GetCurrentThreadId()) ||
        !SudekiMpLanStoryFrameMatchesScene(frame,scene) ||
        !SudekiMpLanStoryObserverRoster(controller,w,scene,&fresh)) {
        InterlockedExchange(&admission,0); return FALSE;
    }
    thread=GetCurrentThreadId();
    BOOL same_world=owner.epoch==fresh.epoch && owner.world==fresh.world &&
        owner.descriptor==fresh.descriptor && owner.group==fresh.group;
    for(unsigned c=0;c<4;++c) {
        if(!same_world || owner.actors[c]!=fresh.actors[c] ||
            generations[c]!=frame->actors[c].generation) memset(&journal[c],0,sizeof(journal[c]));
        actor_position[c][0]=frame->actors[c].x;actor_position[c][1]=frame->actors[c].y;actor_position[c][2]=frame->actors[c].z;
        generations[c]=frame->actors[c].generation;
        frame->actors[c].shots=journal[c];
    }
    owner=fresh;
    if(!SudekiMpLanStoryFrameValid(frame) || !SudekiMpLanStoryObserverRosterStillExact(w,&fresh)) {
        InterlockedExchange(&admission,0); return FALSE;
    }
    SudekiMpLanAimActors(fresh.actors[1],fresh.actors[3]);
    InterlockedExchange(&admission,1); return TRUE;
}
BOOL SudekiMpLanStoryShotsUninstall(void) {
    InterlockedExchange(&admission,0);
    if(!installed) return TRUE;
    if(thread && thread!=GetCurrentThreadId()) { SetLastError(ERROR_BUSY); return FALSE; }
    if(!SudekiMpLanAimUninstall()) return FALSE;
    installed=FALSE; base=NULL; thread=0;
    memset(&owner,0,sizeof(owner)); memset(journal,0,sizeof(journal)); memset(generations,0,sizeof(generations));
    return TRUE;
}

BOOL SudekiMpLanStoryShotsAppendVisuals(const SudekiMpLanStoryNativeRoster *r,SudekiMpStoryVfx *state) {
    if(!r || !state || !installed || thread!=GetCurrentThreadId()) return FALSE;
    if(!state->observed) return TRUE;
    return SudekiMpLanStoryProjectileVisuals(state->stream,r->actors,state);
}
