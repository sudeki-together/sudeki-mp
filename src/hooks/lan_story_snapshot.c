#include "hooks/lan_story_snapshot.h"
#include "cleanroom/engine.h"
#include "engine/skill_activation_abi.h"
#include "engine/weapon_activation_abi.h"
#include "network/lan_party_motion.h"
#include <math.h>
#include <string.h>

static const SudekiMpCleanroomActor types[4]={SUDEKIMP_CLEANROOM_BUKI,
    SUDEKIMP_CLEANROOM_ELCO,SUDEKIMP_CLEANROOM_TAL,SUDEKIMP_CLEANROOM_AILISH};
static const uint8_t actor_types[4]={SUDEKIMP_LAN_ARENA_BUKI_TYPE,
    SUDEKIMP_LAN_ARENA_ELCO_TYPE,SUDEKIMP_LAN_ARENA_TAL_TYPE,
    SUDEKIMP_LAN_ARENA_AILISH_TYPE};
typedef struct ActorObservation {
    void *position, *model, *weapon, *skill;
    uint8_t item,weapon_visible,attachment[2];
} ActorObservation;

static BOOL readable(const void *p,size_t size) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && size && a+size>=a && VirtualQuery(p,&m,sizeof(m)) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+size<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL capture_skill(void *actor,BOOL native_pose,void **skill_out) {
    SudekiMpCharacterSkillState skill;
    if(!SudekiMpObserveCharacterSkill(actor,&skill) || (!native_pose && skill.active) ||
        !readable(skill.skill,0x78u) ||
        *(void **)((uint8_t *)skill.skill+0x10u)!=actor ||
        (skill.active && (skill.slot<0 || skill.slot>=6))) return FALSE;
    void *task=*(void **)((uint8_t *)skill.skill+0x74u);
    /* Semantic locomotion still requires a terminal reference cell. The
     * external native-pose stream may observe a live, owned CSkill; this
     * grants the replica no permission to start or advance that task. */
    if(task && (!readable(task,8u) || !*((uint32_t *)task+1) ||
        (!native_pose && *(void **)task)))
        return FALSE;
    if(skill_out) *skill_out=skill.skill;
    return TRUE;
}
static BOOL equipped_item(void *actor,void **weapon_out,uint8_t *item_out) {
    SudekiMpWeaponQuickList before,after;
    BOOL pending=TRUE;
    unsigned equipped=0; uint8_t result=0;
    if(!weapon_out || !item_out || !readable(actor,0xc4u) ||
        !SudekiMpWeaponActivationPending(actor,&pending) || pending ||
        !SudekiMpDescribeCharacterWeapons(actor,&before)) return FALSE;
    uint8_t *weapon=*(uint8_t **)((uint8_t *)actor+0xc0u);
    if(!readable(weapon,0x270u) || *(void **)(weapon+0x10u)!=actor ||
        *(void **)(weapon+0x26cu)) return FALSE;
    void *current=*(void **)(weapon+0x268u);
    for(unsigned i=0;i<before.row_count;++i) if(before.rows[i].equipped) {
        void *item=before.rows[i].native_item;
        /* DescribeCharacterWeapons already proves database identity and the
         * actor's own weapon family for every row. Read that exact item ID. */
        if(++equipped>1u || item!=current || !readable(item,0x18u)) return FALSE;
        uint32_t id=*(uint32_t *)((uint8_t *)item+0x14u);
        if(id>=48u) return FALSE;
        result=(uint8_t)(id+1u);
    }
    if(!!current!=!!equipped || !SudekiMpDescribeCharacterWeapons(actor,&after) ||
        before.inventory_category!=after.inventory_category || before.row_count!=after.row_count ||
        *(void **)((uint8_t *)actor+0xc0u)!=weapon || *(void **)(weapon+0x268u)!=current ||
        *(void **)(weapon+0x26cu)) return FALSE;
    for(unsigned i=0;i<before.row_count;++i)
        if(before.rows[i].slot!=after.rows[i].slot ||
            before.rows[i].native_item!=after.rows[i].native_item ||
            before.rows[i].equipped!=after.rows[i].equipped) return FALSE;
    *weapon_out=weapon; *item_out=result; return TRUE;
}
static BOOL observe_actor(void *actor,BOOL native_pose,ActorObservation *out) {
    ActorObservation next={0};
    if(!out || !readable(actor,0x134u)) return FALSE;
    next.position=*(void **)((uint8_t *)actor+0x44u);
    next.model=*(void **)((uint8_t *)actor+0x130u);
    if(!readable(next.position,0x98u) || !readable(next.model,0x14u) ||
        *(void **)((uint8_t *)next.position+0x10u)!=actor ||
        *(void **)((uint8_t *)next.model+0x10u)!=actor) return FALSE;
    uintptr_t parent=*(uintptr_t *)((uint8_t *)next.position+0x94u);
    if((parent && parent!=4u) || !capture_skill(actor,native_pose,&next.skill) ||
        !equipped_item(actor,&next.weapon,&next.item)) return FALSE;
    if(!readable(next.weapon,0x3b9u)) return FALSE;
    next.weapon_visible=next.item ? ((((uint8_t *)next.weapon)[0x3b8u]>>1u)&1u):0u;
    if(next.item && !SudekiMpObserveCharacterWeaponAttachment(actor,next.attachment)) return FALSE;
    *out=next; return TRUE;
}
static BOOL same_actor(const ActorObservation *a,const ActorObservation *b) {
    return a->position==b->position && a->model==b->model && a->weapon==b->weapon &&
        a->skill==b->skill && a->item==b->item && a->weapon_visible==b->weapon_visible &&
        !memcmp(a->attachment,b->attachment,sizeof(a->attachment));
}
static BOOL capturable_world(BOOL native_poses,BOOL *combat_out) {
    BOOL combat=TRUE; int spirit=-1;
    if(!combat_out || !SudekiMpCleanroomEngineCombatMode(&combat) ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit)) return FALSE;
    /* A native pose stream observes actions; it never schedules them on the
     * client. Legacy semantic movement keeps its narrower idle contract. */
    if(!native_poses && (combat || spirit || SudekiMpCleanroomEngineRangedCombatPrimePending())) return FALSE;
    *combat_out=combat; return TRUE;
}
typedef struct ViewCaptureScope {
    const SudekiMpControlUpdateDispatchWitness *witness;
    const SudekiMpLanStoryNativeRoster *roster;
} ViewCaptureScope;
static BOOL view_capture_exact(void *context) {
    const ViewCaptureScope *scope=context;
    return scope && SudekiMpLanStoryObserverRosterStillExact(scope->witness,scope->roster);
}
static BOOL capture_view(SudekiMpLanStoryCinematicCapture *capture,uint32_t epoch,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryNativeRoster *roster,SudekiMpLanStoryView *out) {
    ViewCaptureScope scope={witness,roster};
    uint32_t serial=0; char name[SUDEKIMP_STORY_CAMERA_NAME_SIZE];
    return SudekiMpLanStoryCinematicCaptureView(capture,epoch,out,&serial,name,
        view_capture_exact,&scope);
}
void SudekiMpLanStoryCaptureReset(SudekiMpLanStoryCapture *capture) {
    if(capture) memset(capture,0,sizeof(*capture));
}
BOOL SudekiMpLanStoryCapturePresentation(SudekiMpLanStoryCapture *capture,
    void *controller,const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryScene *scene,const SudekiMpLanStoryFrame *party,
    SudekiMpLanStoryPresentation *out) {
    SudekiMpLanStoryNativeRoster roster;
    if(!capture || !out || !SudekiMpLanStoryFrameMatchesScene(party,scene) ||
        capture->sequence!=party->sequence || capture->last_tick!=party->host_tick ||
        !SudekiMpLanStoryObserverRoster(controller,witness,scene,&roster)) {
        SetLastError(ERROR_RETRY); return FALSE;
    }
    ViewCaptureScope scope={witness,&roster};
    return SudekiMpLanStoryCinematicCaptureSpeech(&capture->speech,party,out,
        view_capture_exact,&scope);
}
BOOL SudekiMpLanStoryCaptureMovement(SudekiMpLanStoryCapture *capture,
    SudekiMpLanPartySession *session,void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene,
    BOOL native_poses,uint32_t now,SudekiMpLanStoryFrame *out) {
    SudekiMpLanStoryNativeRoster roster;
    SudekiMpLanStoryFrame frame={0};
    ActorObservation actor_before[4]={{0}}; BOOL combat_before,combat_after;
    if(!capture || !session || !out || SudekiMpLanPartyLocalSeat(session)!=0u ||
        !SudekiMpLanStoryObserverRoster(controller,w,scene,&roster) || !capturable_world(native_poses,&combat_before)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    if(capture->sequence==UINT32_MAX) { SetLastError(ERROR_ARITHMETIC_OVERFLOW); return FALSE; }
    if(capture->sequence && ((int32_t)(now-capture->last_tick)<=0 ||
        scene->epoch<capture->epoch || scene->revision<capture->revision)) {
        SetLastError(ERROR_RETRY); return FALSE;
    }
    SudekiMpLanStoryCapture next=*capture;
    frame.epoch=scene->epoch; frame.revision=scene->revision; frame.host_tick=now;
    frame.sequence=capture->sequence+1u; frame.available_mask=scene->available_mask;
    frame.leader_character=scene->leader_seat; frame.combat_mode=(uint8_t)combat_before;
    /* A missing current view does not fabricate a camera. Receivers may keep
     * observing the scene, but cannot claim spectator playback readiness. */
    (void)capture_view(&next.cinematic,scene->epoch,w,&roster,&frame.view);
    for(unsigned c=0;c<4u;++c) {
        if(!(scene->available_mask&(1u<<c))) {
            next.actors[c]=NULL; memset(&next.motion[c],0,sizeof(next.motion[c])); continue;
        }
        BOOL fresh=next.epoch!=scene->epoch || next.actors[c]!=roster.actors[c];
        if(fresh) {
            if(next.generation[c]==UINT32_MAX) {
                SetLastError(ERROR_ARITHMETIC_OVERFLOW); return FALSE;
            }
            ++next.generation[c]; memset(&next.motion[c],0,sizeof(next.motion[c]));
        }
        if(!SudekiMpLanStoryObserverRosterStillExact(w,&roster) ||
            !observe_actor(roster.actors[c],native_poses,&actor_before[c])) {
            SetLastError(ERROR_BUSY); return FALSE;
        }
        float position[3],facing[2],hp,sp;
        SudekiMpCleanroomActorPresentation native;
        SudekiMpLanStoryActor *actor=&frame.actors[c];
        actor->generation=next.generation[c]; actor->character=(uint8_t)c;
        actor->weapon_item_plus_one=actor_before[c].item;
        actor->weapon_visible=actor_before[c].weapon_visible;
        memcpy(actor->weapon_attachment,actor_before[c].attachment,sizeof(actor->weapon_attachment));
        if(!SudekiMpCleanroomEngineActorPosition(types[c],position) ||
            !SudekiMpCleanroomEngineActorFacing(types[c],facing) ||
            !SudekiMpCleanroomEngineActorResources(types[c],&hp,&sp) ||
            !isfinite(hp) || !isfinite(sp) || hp<0 || sp<0 ||
            hp>SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE || sp>SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE) {
            SetLastError(ERROR_NOT_SUPPORTED); return FALSE;
        }
        actor->native_pose=native_poses?1u:0u;
        if(!native_poses && (!SudekiMpCleanroomEngineWorldMotion(types[c],&native) ||
            !SudekiMpLanPartyMotionObserve(actor_types[c],native.selector[0],&actor->animation_state) ||
            !SudekiMpLanPartyMotionCapture(actor_types[c],native.selector,native.state,
                native.rate,native.time,native.blend,fresh?NULL:&next.motion[c],&actor->locomotion))) {
            SetLastError(ERROR_NOT_SUPPORTED); return FALSE;
        }
        actor->x=position[0]; actor->y=position[1]; actor->z=position[2];
        actor->facing_x=facing[0]; actor->facing_z=facing[1];
        actor->hp=(uint32_t)hp; actor->sp=(uint32_t)sp;
        if(!SudekiMpLanStoryActorValid(actor,c)) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }
        next.actors[c]=roster.actors[c]; next.motion[c]=actor->locomotion;
    }
    if(!capturable_world(native_poses,&combat_after) || combat_before!=combat_after || !SudekiMpLanStoryFrameMatchesScene(&frame,scene) ||
        !SudekiMpLanStoryObserverRosterStillExact(w,&roster)) {
        SetLastError(ERROR_RETRY); return FALSE;
    }
    for(unsigned c=0;c<4u;++c) if(scene->available_mask&(1u<<c)) {
        ActorObservation after;
        if(!observe_actor(roster.actors[c],native_poses,&after) || !same_actor(&actor_before[c],&after)) {
            SetLastError(ERROR_RETRY); return FALSE;
        }
    }
    if(!SudekiMpLanStoryObserverRosterStillExact(w,&roster)) {
        SetLastError(ERROR_RETRY); return FALSE;
    }
    next.epoch=scene->epoch; next.revision=scene->revision;
    next.sequence=frame.sequence; next.last_tick=now;
    *capture=next; *out=frame; SetLastError(ERROR_SUCCESS); return TRUE;
}
