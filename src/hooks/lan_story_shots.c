#include "hooks/lan_story_shots.h"
#include "hooks/lan_arena_ranged_aim.h"
#include "engine/weapon_activation_abi.h"
#include "engine/skill_activation_abi.h"
#include "engine/log.h"
#include <math.h>
#include <string.h>

static uint8_t *base;
static DWORD thread;
static volatile LONG admission;
static BOOL installed;
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
        !world[0x399u] || !world[0x39au] ||
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
    SudekiMpLogFormat("story_shots event=host_emission character=%u generation=%lu sequence=%lu item=%u tick=%lu\r\n",
        c,(unsigned long)generations[c],(unsigned long)event.sequence,event.item,(unsigned long)event.host_tick);
}
BOOL SudekiMpLanStoryShotsInstall(HMODULE image) {
    if(base || installed || !image) return FALSE;
    base=(uint8_t *)image;
    if(!SudekiMpLanAimObserveEmissionsInstall(image,emitted)) { base=NULL; return FALSE; }
    installed=TRUE; return TRUE;
}
void SudekiMpLanStoryShotsCloseAdmission(void) {
    InterlockedExchange(&admission,0); /* retained hook becomes a no-op */
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
