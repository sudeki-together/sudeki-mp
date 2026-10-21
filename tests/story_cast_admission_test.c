/* Synthetic native ownership/activation only. No game constructors, tasks,
 * damage, rendering or native hooks run in this fixture. */
#include "../src/hooks/lan_story_cast.c"
#include "../src/network/lan_story_frame.c"
#include <assert.h>
#include <stdio.h>
static uint8_t actors[4][0x100],skills[4][0x80],speed[0x30],front[0x178],quick[0x2a];
static BOOL identity=TRUE,lease_exact=TRUE,known=TRUE,lineage_idle=TRUE;
static BOOL lighting_tail;
static BOOL instance_idle[4]={TRUE,TRUE,TRUE,TRUE},spirit_active,healthy=TRUE;
static BOOL native_active[4],accept_skill=TRUE,hold_active=TRUE;
static int rejection;
static unsigned uses,timing,enters,advances,drains,spirits;
static BOOL spirit_healthy=TRUE,accept_spirit=TRUE,observe_spirit_start=TRUE;
static int spirit_rejection;
static unsigned routing_on,routing_off;
static BOOL routing_ok=TRUE,effects_live,missiles_live,neutral_scope=TRUE;
static unsigned roster_checks,leaves;
static BOOL probe_transition,enter_ok=TRUE,leave_ok=TRUE;
static SudekiMpSpiritCameraView camera_observation;
static BOOL camera_known=TRUE,camera_changes_owner;
static unsigned expected_camera_character=2;
BOOL SudekiMpObserveSpiritInstanceView(const SudekiMpSpiritInstance *s,SudekiMpSpiritCameraView *out) {
    assert(s==&caster[expected_camera_character].instance);
    if(!camera_known) return FALSE;
    *out=camera_observation;
    if(camera_changes_owner) identity=FALSE;
    return TRUE;
}
static void check_transition(void) {
    assert(transition_roster_proved);
    unsigned before=roster_checks;
    for(unsigned i=0;i<8;++i) assert(retained_actor(actors[2],9));
    assert(roster_checks==before); /* One proof per synchronous transition. */
    assert(!retained_actor(actors[2],10) && !retained_actor(actors[0],9));
    DWORD thread=native_thread; native_thread^=1;
    assert(!retained_actor(actors[2],9)); native_thread=thread;
    assert(!enter_task(NULL) && !leave_task(13)); /* No recursive reuse. */
}
BOOL SudekiMpLanCastContextSetTaskRouting(SudekiMpLanCastTaskEnter enter,SudekiMpLanCastTaskLeave leave) {
    assert((enter==NULL)==(leave==NULL));
    if(!routing_ok || !lineage_idle) return FALSE;
    if(enter) { assert(enter==enter_task && leave==leave_task); ++routing_on; }
    else ++routing_off;
    return TRUE;
}
BOOL SudekiMpLeaveSpiritInstance(uint32_t cookie) {
    assert(cookie==13); ++leaves; if(probe_transition) check_transition(); return leave_ok;
}
BOOL SudekiMpObserveSpiritInstanceScope(SudekiMpSpiritInstance *out) {
    memset(out,0,sizeof(*out)); if(!neutral_scope) *out=caster[2].instance; return known;
}
BOOL SudekiMpLogResearchEnabled(void) { return FALSE; }
BOOL SudekiMpObserveSpiritInstanceManagerTicks(const SudekiMpSpiritInstance *s,uint32_t *ticks) {
    (void)s; *ticks=0; return known;
}
BOOL SudekiMpObserveSpiritInstanceUpdates(const SudekiMpSpiritInstance *s,uint32_t *c,uint32_t *v) {
    (void)s; *c=*v=0; return known;
}
BOOL SudekiMpLanPartyEffectLifetimeRetains(void) { return effects_live; }
BOOL SudekiMpLanPartyProjectileLifetimeRetains(void) { return missiles_live; }
void SudekiMpLogWrite(const char *line) { (void)line; }
static int ix(void *a) { for(unsigned i=0;i<4;++i) if(a==actors[i]) return (int)i; return -1; }
static BOOL epoch_proof;static uint32_t current_epoch;
BOOL SudekiMpLanStoryObserverPublished(SudekiMpLanStoryScene *out) {
    if(!epoch_proof)return FALSE;
    *out=(SudekiMpLanStoryScene){.epoch=current_epoch,.revision=2,.phase=SUDEKIMP_LAN_STORY_READY};return TRUE;
}
static BOOL identity_unknown;
BOOL SudekiMpLanStoryObserverIdentityUnknown(void) {return identity_unknown;}
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *r) {
    ++roster_checks;
    return identity && (epoch_proof?r->epoch==current_epoch:r==&owners);
}
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(const SudekiMpControlUpdateDispatchWitness *w) {
    return w && w->dispatch_serial==5;
}
BOOL SudekiMpLanStoryControlExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *k) {
    return lease_exact && w && r && k && k->token==21 && k->generation==8 && k->seat==2;
}
BOOL SudekiMpObserveCharacterSkill(void *a,SudekiMpCharacterSkillState *s) {
    int i=ix(a); if(!known || i<0) return FALSE;
    *s=(SudekiMpCharacterSkillState){.skill=skills[i],.active=native_active[i]}; return TRUE;
}
BOOL SudekiMpCleanroomEngineRangedCombatPrimePending(void) { return FALSE; }
BOOL SudekiMpCleanroomEngineSpiritPresentationState(int *state) { *state=0; return known; }
BOOL SudekiMpLanCastContextActorDrained(void *a,uint64_t life) { return ix(a)>=0 && life==9 && lineage_idle; }
BOOL SudekiMpLanCastContextDrained(void) { return lineage_idle; }
BOOL SudekiMpObserveSpiritInstance(const SudekiMpSpiritInstance *s,SudekiMpSpiritInstanceState *o) {
    if(!known || s->generation<1 || s->generation>4) return FALSE;
    memset(o,0,sizeof(*o)); o->state=spirit_active?7:0; o->body_idle=instance_idle[s->generation-1]; o->idle=o->body_idle && !lighting_tail; return TRUE;
}
BOOL SudekiMpObserveSpiritInstanceActivity(const SudekiMpSpiritInstance *s,BOOL *a) {
    (void)s; *a=spirit_active; return known;
}
BOOL SudekiMpBeginSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *s,uint16_t seq) {
    assert(s==&caster[2].instance && seq==1); ++timing; return TRUE;
}
static uint8_t aim_phase;
static uint16_t aim_remaining;
BOOL SudekiMpObserveSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *s,uint16_t seq,
    uint8_t *phase,uint16_t *ms) {
    assert(s==&caster[2].instance && seq==1);
    *phase=aim_phase; *ms=aim_remaining; return known;
}
uint32_t SudekiMpEnterSpiritInstance(const SudekiMpSpiritInstance *s) {
    assert(task_routed); /* Isolation precedes ALL private native entry. */
    if(s) assert(s==&caster[2].instance);
    ++enters; if(probe_transition) check_transition(); return enter_ok?13:0;
}
BOOL SudekiMpSkillActivationRoutingHealthy(void) { return healthy; }
BOOL SudekiMpSpiritActivationRoutingHealthy(void) { return spirit_healthy; }
BOOL SudekiMpResolveSpiritStrikeId(unsigned type,unsigned variant,int *strike) {
    if(!strike || variant<1 || variant>2 || type!=0x23) return FALSE;
    *strike=(int)variant-1; return TRUE;
}
SudekiMpSpiritActivationResult SudekiMpActivateCharacterSpirit(void *a,unsigned variant) {
    assert(a==actors[2] && variant>=1 && variant<=2 && submitting==2 && submitting_spirit);
    uint64_t life; uint8_t type; void *manager=NULL;
    assert(!root_owner(a,2,&life,&type)); /* Retail root is a later manager tick. */
    assert(!all_idle() && namespaces_idle()); /* registration stays closed */
    assert(!enter_spirit(a,4,FALSE,&manager)); /* foreign strike ID */
    assert(enter_spirit(a,(int)variant-1,FALSE,&manager)==13);
    assert(manager==caster[2].instance.manager);
    if(!accept_spirit) return (SudekiMpSpiritActivationResult){
        .status=SUDEKIMP_SPIRIT_ACTIVATION_VALIDATION_REJECTED,.validation_result=spirit_rejection};
    assert(enter_spirit(a,(int)variant-1,TRUE,&manager)==13);
    ++spirits; spirit_active=observe_spirit_start; instance_idle[2]=!observe_spirit_start;
    return (SudekiMpSpiritActivationResult){.status=SUDEKIMP_SPIRIT_ACTIVATION_STARTED,
        .strike_id=(int)variant-1,.activation_result=1};
}
SudekiMpSkillActivationResult SudekiMpActivateCharacterSkillSlot(void *a,int slot) {
    assert(a==actors[2] && slot==3 && submitting==2 && authority(2)); ++uses;
    if(!accept_skill) return (SudekiMpSkillActivationResult){.status=SUDEKIMP_SKILL_ACTIVATION_VALIDATION_REJECTED,
        .validation_result=rejection};
    assert(enter_skill(skills[2],slot,TRUE)==13);
    native_active[2]=hold_active;
    return (SudekiMpSkillActivationResult){.status=SUDEKIMP_SKILL_ACTIVATION_STARTED,.skill=skills[2]};
}
static SudekiMpLanCastOwner visual_lineage;
static BOOL visual_lineage_known;
BOOL SudekiMpLanCastContextCurrentRetained(SudekiMpLanCastOwner *out) {
    if(!visual_lineage_known) return FALSE;
    *out=visual_lineage; return TRUE;
}
static SudekiMpLanPartyEffectOwner visual_projectile_owner;
static BOOL visual_projectile_scope;
BOOL SudekiMpLanPartyProjectileLifetimeCurrent(SudekiMpLanPartyEffectOwner *out) {
    if(!visual_projectile_scope) return FALSE;
    *out=visual_projectile_owner;return TRUE;
}
BOOL SudekiMpLanPartyEffectLifetimeCurrent(SudekiMpLanPartyEffectOwner *out) { return effect_owner(NULL,out); }
BOOL SudekiMpLanCastContextPoll(void) { return known; }
BOOL SudekiMpLanPartyEffectLifetimePoll(void) { return known; }
BOOL SudekiMpLanPartyProjectileLifetimePoll(void) { return known; }
BOOL SudekiMpAdvanceSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *s,float seconds) {
    (void)s; assert(seconds>=0 && seconds<=.1f); ++advances; return known;
}
BOOL SudekiMpDrainSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *s) { (void)s; ++drains; return known; }
BOOL SudekiMpLanPartyProjectileLifetimeRequestRetire(void) { assert(shutdown_drained()); return TRUE; }
BOOL SudekiMpLanPartyEffectLifetimeRequestRetire(void) { assert(shutdown_drained()); return TRUE; }
void SudekiMpLogFormat(const char *format,...) { (void)format; }
uint8_t SudekiMpLanPartyActorType(unsigned c) { const uint8_t types[]={5,14,35,1};return c<4?types[c]:0; }
static void reset(void) {
    memset(caster,0,sizeof(caster)); memset(&owners,0,sizeof(owners));
    memset(skills,0,sizeof(skills)); memset(speed,0,sizeof(speed));
    memset(front,0,sizeof(front)); memset(quick,0,sizeof(quick)); front[0x8c]=1;
    memset(native_active,0,sizeof(native_active));
    identity=lease_exact=known=lineage_idle=healthy=accept_skill=hold_active=TRUE;
    spirit_active=lighting_tail=FALSE; bound=initialized=TRUE; stopping=fault=initializing=retire_superseded=FALSE;
    submitting=-1; submitting_spirit=FALSE; lifetime=9; native_thread=GetCurrentThreadId();
    owners.available_mask=12; owners.leader_character=3;
    for(unsigned i=2;i<4;++i) {
        caster[i].actor=owners.actors[i]=actors[i]; caster[i].instance.generation=i+1;
        caster[i].instance.manager=skills[i];
        *(void **)(skills[i]+0x10)=actors[i]; instance_idle[i]=TRUE;
    }
    *(void **)((uint8_t *)image+0x408da0)=speed;
    *(void **)((uint8_t *)image+0x408d1c)=front;
    *(void **)((uint8_t *)image+0x3c2f84)=quick;
    uses=timing=enters=advances=drains=0; last_service=0;
    rejection=0;
    routing_on=routing_off=0; task_routed=effects_live=missiles_live=FALSE;
    routing_ok=neutral_scope=TRUE;
    transition_roster_proved=probe_transition=FALSE; enter_ok=leave_ok=TRUE; roster_checks=leaves=0;
    spirits=0; spirit_healthy=accept_spirit=observe_spirit_start=TRUE; spirit_rejection=0;
    camera_known=TRUE; camera_changes_owner=FALSE;
    camera_observation=(SudekiMpSpiritCameraView){.kind=1,
        .matrix={1,0,0,0,0,1,0,0,0,0,1,0,10,20,30,1},.projection={1,.1f,1000}};
}
static SudekiMpLanArenaSpiritAudioEvent audio_events[4];
size_t SudekiMpLanArenaSpiritAudioTraceSnapshot(SudekiMpLanArenaSpiritAudioEvent *out,size_t capacity,uint32_t *dropped) {
    assert(capacity>=4 && !dropped);memcpy(out,audio_events,sizeof(audio_events));return 4;
}
static void cast_audio_publication(void) {
    reset();audio_owned=TRUE;caster[2].sequence=5;spirit_active=TRUE;
    int state;uint64_t session;uint16_t sequence;uint8_t character;uint32_t tick;
    assert(!audio_witness(NULL,&state,&session,&sequence,&character));
    neutral_scope=FALSE;
    assert(audio_witness(NULL,&state,&session,&sequence,&character));
    assert(state==7 && session==9 && sequence==5 && character==2);
    identity=FALSE;assert(!audio_witness(NULL,&state,&session,&sequence,&character));identity=TRUE;
    known=FALSE;assert(!audio_witness(NULL,&state,&session,&sequence,&character));known=TRUE;
    spirit_active=FALSE;assert(!audio_witness(NULL,&state,&session,&sequence,&character));spirit_active=TRUE;
    for(unsigned i=0;i<4;++i) audio_events[i]=(SudekiMpLanArenaSpiritAudioEvent){
        .session_token=9,.owner_seat=2,.skill_sequence=5,.observed_tick=100,.cue="spiritstrike_start"};
    audio_events[0].session_token=8;audio_events[1].owner_seat=3;audio_events[2].skill_sequence=4;
    SudekiMpLanStoryCastCaptureAudioStart(2,&sequence,&tick);assert(sequence==5 && tick==100);
    audio_events[3].observed_tick=200; /* Same cast cannot restart the one-shot. */
    SudekiMpLanStoryCastCaptureAudioStart(2,&sequence,&tick);assert(sequence==5 && tick==100);
    ++caster[2].sequence;
    SudekiMpLanStoryCastCaptureAudioStart(2,&sequence,&tick);assert(sequence==5 && tick==100);
    ++audio_events[3].skill_sequence;
    SudekiMpLanStoryCastCaptureAudioStart(2,&sequence,&tick);assert(sequence==6 && tick==200);
    identity=FALSE;SudekiMpLanStoryCastCaptureAudioStart(2,&sequence,&tick);assert(!sequence && !tick);
    identity=TRUE;audio_owned=FALSE;
    puts("PASS Spirit startup exact scoped native witness, character/session/cast fences, emission timestamp and duplicate suppression");
}
static void cast_camera_publication(void) {
    SudekiMpStoryCastView view,before;
    /* The same observed native choice must reach the client for an ordinary
     * skill and a Spirit Strike, including the obstruction/default branch. */
    for(unsigned character=0;character<4;++character) for(unsigned spirit=0;spirit<2;++spirit) {
        reset(); expected_camera_character=character;
        owners.available_mask=15; owners.leader_character=(character+1u)%4u;
        for(unsigned i=0;i<4;++i) {
            caster[i].actor=owners.actors[i]=actors[i]; caster[i].instance.generation=i+1;
            caster[i].sequence=(uint16_t)(7+i*10); caster[i].started=TRUE;
        }
        unsigned sequence=caster[character].sequence;
        camera_observation.matrix[12]=(float)(100+character);
        caster[character].started=!spirit; caster[character].spirit_started=spirit;
        for(unsigned kind=0;kind<=2;++kind) {
            camera_observation.kind=kind;
            assert(SudekiMpLanStoryCastCaptureView(character,&view) && view.kind==kind);
            assert(view.stream==9 && view.cast_sequence==sequence && view.fence.character==character);
            assert(SudekiMpLanStoryViewValid(&view.view));
            if(kind) {
                assert(view.view.camera_serial==sequence*4+kind);
                assert(!memcmp(view.view.matrix,camera_observation.matrix,sizeof(view.view.matrix)));
                assert(!memcmp(view.view.projection,camera_observation.projection,sizeof(view.view.projection)));
            } else assert(!view.view.valid && !view.view.camera_serial);
        }
        before=view; camera_known=FALSE;
        assert(!SudekiMpLanStoryCastCaptureView(character,&view) && !memcmp(&view,&before,sizeof(view)));
        camera_known=TRUE; camera_changes_owner=TRUE;
        assert(!SudekiMpLanStoryCastCaptureView(character,&view) && !memcmp(&view,&before,sizeof(view)));
        camera_changes_owner=FALSE; identity=TRUE;
        camera_observation.matrix[0]=NAN;
        assert(!SudekiMpLanStoryCastCaptureView(character,&view) && !memcmp(&view,&before,sizeof(view)));
        camera_observation.matrix[0]=1;
        /* Drained casts publish normal even if a retained camera still has
         * an old selection. A following cast has a new camera serial. */
        caster[character].started=caster[character].spirit_started=FALSE;
        assert(SudekiMpLanStoryCastCaptureView(character,&view) && !view.kind && !view.view.valid);
        ++caster[character].sequence; caster[character].started=!spirit; caster[character].spirit_started=spirit;
        assert(SudekiMpLanStoryCastCaptureView(character,&view) && view.view.camera_serial==(sequence+1)*4+2);
        before=view; stopping=TRUE;
        assert(!SudekiMpLanStoryCastCaptureView(character,&view) && !memcmp(&view,&before,sizeof(view)));
        stopping=FALSE; fault=TRUE; assert(!SudekiMpLanStoryCastCaptureView(character,&view)); fault=FALSE;
        owners.available_mask&=~(1u<<character);
        assert(!SudekiMpLanStoryCastCaptureView(character,&view)); owners.available_mask=15;
        owners.actors[character]=actors[(character+1)%4];
        assert(!SudekiMpLanStoryCastCaptureView(character,&view)); owners.actors[character]=actors[character];
        caster[character].actor=NULL; assert(!SudekiMpLanStoryCastCaptureView(character,&view));
        caster[character].actor=actors[character];
        assert(!SudekiMpLanStoryCastCaptureView(4,&view) && !SudekiMpLanStoryCastCaptureView(UINT_MAX,&view));
        assert(!memcmp(&view,&before,sizeof(view)));
        owners.leader_character=character; assert(!SudekiMpLanStoryCastCaptureView(character,&view));
    }
    puts("PASS all four characters ordinary/Spirit native camera choices, obstruction fallback, completion, recast and unknown ownership");
}
#include "story_cast_binding_fixture.h"
int main(void) {
    image=VirtualAlloc(NULL,0x409000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE); assert(image);
    cast_binding_lifecycle();
    HMODULE epoch_image=image;reset();epoch_proof=TRUE;owners.epoch=5;current_epoch=6;
    SudekiMpLanStoryNativeRoster fresh=owners;fresh.epoch=6;
    SudekiMpControlUpdateDispatchWitness ew={.dispatch_serial=5,.service_post_original_exact=TRUE};
    identity_unknown=TRUE;
    SudekiMpLanStoryNativeRoster wrong=fresh;wrong.actors[2]=(void *)1;
    assert(SudekiMpLanStoryCastService(&ew,&wrong) && !fault && owners.epoch==5 && bound);
    identity_unknown=FALSE;
    assert(SudekiMpLanStoryCastSuperseded());
    fresh.actors[2]=(void *)1;assert(!rebase_owners(&fresh));fresh=owners;fresh.epoch=6;
    effects_live=TRUE;unsigned proved=roster_checks;
    assert(SudekiMpLanStoryCastService(&ew,&fresh) && !fault && owners.epoch==6 && bound && retire_superseded);
    assert(roster_checks>proved && !SudekiMpLanStoryCastSuperseded());
    assert(!SudekiMpLanStoryCastUninstall() && bound); /* retained tail */
    effects_live=FALSE;assert(SudekiMpLanStoryCastService(&ew,&fresh) && !bound && image==epoch_image);
    assert(SudekiMpLanStoryCastTryBind(&ew,&fresh) && bound && owners.epoch==6);
    caster[2].spirit_started=TRUE;assert(!SudekiMpLanStoryCastUninstall() && bound);
    caster[2].spirit_started=FALSE;assert(SudekiMpLanStoryCastUninstall() && !bound);
    image=epoch_image;epoch_proof=FALSE;
    cast_camera_publication();
    cast_audio_publication();
    SudekiMpControlUpdateDispatchWitness w={.dispatch_serial=5,.service_post_original_exact=TRUE};
    SudekiMpLanStoryNativeRoster r={0}; r.actors[2]=actors[2];
    SudekiMpLanPartyLease key={.token=21,.generation=8,.seat=2};
    reset(); task_routed=probe_transition=TRUE;
    SudekiMpLanCastOwner retained={.actor=actors[2],.session=9,.cast_id=1,.kind=2};
    assert(enter_task(&retained)==13 && roster_checks==1 && !transition_roster_proved);
    /* The script runs with no cached proof. Identity lost during it cannot
     * reuse entry's proof for either a witness or the exit transition. */
    identity=FALSE; assert(!retained_actor(actors[2],9));
    assert(!leave_task(13) && !leaves && !transition_roster_proved);
    identity=TRUE; unsigned before=roster_checks;
    assert(leave_task(13) && leaves==1 && roster_checks==before+1 && !transition_roster_proved);
    enter_ok=FALSE; assert(!enter_task(&retained) && !transition_roster_proved);
    leave_ok=FALSE; assert(!leave_task(13) && !transition_roster_proved);
    before=enters; retained.session=10; assert(!enter_task(&retained) && enters==before);
    retained.session=9; retained.kind=3; assert(!enter_task(&retained) && enters==before);
    retained.kind=2; native_thread^=1; assert(!enter_task(&retained) && !leave_task(13));
    reset(); caster[2].sequence=1;
    visual_lineage=(SudekiMpLanCastOwner){.actor=actors[2],.session=9,.cast_id=1,.kind=1};
    visual_lineage_known=TRUE;
    uint64_t visual_session;uint16_t visual_sequence;uint32_t visual_tick;uint8_t visual_type;
    /* Synchronous native Use emits before started() publishes its result. */
    assert(!caster[2].started && visual_owner(NULL,NULL,&visual_session,&visual_sequence,&visual_tick,&visual_type));
    assert(visual_session==9 && visual_sequence==1 && visual_type==0);
    visual_lineage.session=10;assert(!visual_owner(NULL,NULL,&visual_session,&visual_sequence,&visual_tick,&visual_type));
    visual_lineage.session=9;visual_lineage.kind=3;
    assert(!visual_owner(NULL,NULL,&visual_session,&visual_sequence,&visual_tick,&visual_type));
    visual_lineage_known=FALSE;
    for(unsigned c=0;c<4;++c) {
        reset(); owners.available_mask=15;
        for(unsigned i=0;i<4;++i) { owners.actors[i]=caster[i].actor=actors[i];caster[i].instance.generation=i+1; }
        caster[c].sequence=10+c;
        visual_lineage=(SudekiMpLanCastOwner){.actor=actors[c],.session=9,.cast_id=2,.kind=1};
        visual_lineage_known=TRUE;
        assert(visual_owner(NULL,NULL,&visual_session,&visual_sequence,&visual_tick,&visual_type));
        assert(visual_sequence==10+c && visual_type==(c==2?0:SudekiMpLanPartyActorType(c)));
        assert(effect_owner(NULL,&visual_projectile_owner));
        visual_lineage_known=FALSE;visual_projectile_scope=TRUE;
        caster[c].sequence+=10; /* New cast must not relabel a delayed impact. */
        assert(visual_owner(NULL,NULL,&visual_session,&visual_sequence,&visual_tick,&visual_type) && visual_sequence==10+c);
        ++visual_projectile_owner.generation;
        assert(!visual_owner(NULL,NULL,&visual_session,&visual_sequence,&visual_tick,&visual_type));
        --visual_projectile_owner.generation;visual_projectile_owner.session=10;
        assert(!visual_owner(NULL,NULL,&visual_session,&visual_sequence,&visual_tick,&visual_type));
        visual_projectile_scope=FALSE;
    }
    reset(); caster[2].started=TRUE; caster[2].skill=skills[2]; caster[2].sequence=1;
    native_active[2]=TRUE; aim_phase=SUDEKIMP_SKILL_TARGET_AIMING; aim_remaining=100;
    assert(SudekiMpLanStoryCastAiming(actors[2]) && !SudekiMpLanStoryCastBodyDrained(actors[2]));
    aim_phase=SUDEKIMP_SKILL_TARGET_RELEASED; assert(!SudekiMpLanStoryCastAiming(actors[2]));
    aim_phase=SUDEKIMP_SKILL_TARGET_PENDING; assert(!SudekiMpLanStoryCastAiming(actors[2]));
    aim_phase=SUDEKIMP_SKILL_TARGET_AIMING; aim_remaining=0; assert(!SudekiMpLanStoryCastAiming(actors[2]));
    aim_remaining=100; known=FALSE; assert(!SudekiMpLanStoryCastAiming(actors[2])); known=TRUE;
    identity=FALSE; assert(!SudekiMpLanStoryCastAiming(actors[2])); identity=TRUE;
    caster[2].spirit_started=TRUE; assert(!SudekiMpLanStoryCastAiming(actors[2])); caster[2].spirit_started=FALSE;
    stopping=TRUE; assert(!SudekiMpLanStoryCastAiming(actors[2])); stopping=FALSE;
    caster[2].skill=skills[3]; assert(!SudekiMpLanStoryCastAiming(actors[2]));
    reset(); assert(SudekiMpLanStoryCastReady());
    for(unsigned i=0;i<1000;++i) assert(SudekiMpLanStoryCastService(&w,&r));
    assert(!task_routed && !routing_on && !enters); advances=0;
    assert(!authority(0) && !authority(1) && !authority(2) && authority(3));
    assert(!enter_skill(skills[2],3,TRUE)); /* no remote admission outside Submit */
    identity=FALSE; assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_UNAVAILABLE);
    identity=TRUE; lease_exact=FALSE;
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_UNAVAILABLE);
    lease_exact=TRUE; speed[0x28]=1;
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_BUSY);
    speed[0x28]=0; speed[0x2a]=1;
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_BUSY);
    speed[0x2a]=0; *(int *)(speed+0x20)=7;
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_BUSY);
    *(int *)(speed+0x20)=2; /* a bare global mode is not cast ownership */
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_BUSY);
    *(int *)(speed+0x20)=0;
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,6)==SUDEKIMP_STORY_ACTION_UNAVAILABLE);
    known=FALSE; assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_BUSY);
    known=TRUE; lineage_idle=FALSE;
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_BUSY);
    lineage_idle=TRUE; spirit_active=TRUE;
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_BUSY);
    spirit_active=FALSE; assert(!uses && !timing);
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_STARTED);
    assert(uses==1 && timing==1 && caster[2].started && caster[2].active_seen && submitting==-1);
    assert(task_routed && routing_on==1 && !routing_off);
    assert(SudekiMpLanStoryCastLocalNoncaster(actors[3]));
    assert(!SudekiMpLanStoryCastLocalNoncaster(actors[2]));
    front[0x8d]=1; assert(!SudekiMpLanStoryCastLocalNoncaster(actors[3])); front[0x8d]=0;
    quick[0x29]=1; assert(!SudekiMpLanStoryCastLocalNoncaster(actors[3])); quick[0x29]=0;
    assert(!authority(2) && !SudekiMpLanStoryCastDrained(actors[2]));
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_BUSY && uses==1);
    /* Socket ownership loss closes new admission but does not discard a
     * retained native descendant or stop its authored timer. */
    lease_exact=FALSE;
    SudekiMpLanCastOwner task={.actor=actors[2],.session=9,.cast_id=1,.kind=1};
    assert(enter_task(&task)==13); task.session=10; assert(!enter_task(&task));
    assert(SudekiMpLanStoryCastService(&w,&r) && advances==2);
    native_active[2]=FALSE; lineage_idle=FALSE;
    assert(SudekiMpLanStoryCastService(&w,&r) && caster[2].started);
    lineage_idle=TRUE;
    effects_live=TRUE;
    assert(SudekiMpLanStoryCastService(&w,&r) && !caster[2].started && SudekiMpLanStoryCastDrained(actors[2]));
    assert(task_routed && !routing_off);
    effects_live=FALSE; missiles_live=TRUE;
    assert(SudekiMpLanStoryCastService(&w,&r) && task_routed && !routing_off);
    missiles_live=FALSE;
    assert(SudekiMpLanStoryCastService(&w,&r) && !task_routed && routing_off==1);
    /* A positive native body drain restores ordinary movement while its
     * retained lighting tail still excludes another cast or teardown. */
    lighting_tail=TRUE;
    assert(SudekiMpLanStoryCastBodyDrained(actors[2]) && !SudekiMpLanStoryCastDrained(actors[2]));
    assert(!all_idle());
    instance_idle[2]=FALSE; assert(!SudekiMpLanStoryCastBodyDrained(actors[2])); instance_idle[2]=TRUE;
    known=FALSE; assert(!SudekiMpLanStoryCastBodyDrained(actors[2])); known=TRUE;
    identity=FALSE; assert(!SudekiMpLanStoryCastBodyDrained(actors[2])); identity=TRUE;
    lineage_idle=FALSE; assert(!SudekiMpLanStoryCastBodyDrained(actors[2])); lineage_idle=TRUE;
    lighting_tail=FALSE; assert(SudekiMpLanStoryCastDrained(actors[2]));
    /* Failed enable never reaches a private callback or starts its timer. */
    reset(); routing_ok=FALSE;
    submitting=2; assert(!enter_skill(skills[2],3,TRUE) && !enters && !timing); submitting=-1;
    /* Failed restoration cannot masquerade as an idle fast path. */
    reset(); task_routed=TRUE; routing_ok=FALSE;
    assert(!SudekiMpLanStoryCastService(&w,&r) && task_routed && fault);
    reset(); task_routed=TRUE; neutral_scope=FALSE;
    assert(!SudekiMpLanStoryCastService(&w,&r) && task_routed && fault);
    reset(); hold_active=FALSE;
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_STARTED);
    assert(SudekiMpLanStoryCastService(&w,&r) && caster[2].started); /* unknown is NOT idle */
    reset(); accept_skill=FALSE;
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_UNAVAILABLE && uses==1 && !timing);
    reset(); accept_skill=FALSE; rejection=1;
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_NO_SP && uses==1 && !timing);
    rejection=2;
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_BUSY);
    reset(); healthy=FALSE;
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_RETAINED && fault);
    reset(); SudekiMpLanStoryCastRequestStop(); assert(!SudekiMpLanStoryCastReady());
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_UNAVAILABLE && !uses);
    reset(); uint64_t life=0; uint8_t type=0; void *manager=NULL;
    assert(!root_owner(actors[2],2,&life,&type));
    assert(!enter_spirit(actors[2],0,TRUE,&manager));
    assert(SudekiMpLanStoryCastSubmitSpirit(&w,&r,&key,0)==SUDEKIMP_STORY_ACTION_UNAVAILABLE);
    assert(SudekiMpLanStoryCastSubmitSpirit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_UNAVAILABLE);
    lease_exact=FALSE;
    assert(SudekiMpLanStoryCastSubmitSpirit(&w,&r,&key,1)==SUDEKIMP_STORY_ACTION_UNAVAILABLE);
    lease_exact=TRUE; speed[0x28]=1;
    assert(SudekiMpLanStoryCastSubmitSpirit(&w,&r,&key,1)==SUDEKIMP_STORY_ACTION_BUSY);
    speed[0x28]=0; *(int *)(speed+0x20)=2;
    assert(SudekiMpLanStoryCastSubmitSpirit(&w,&r,&key,1)==SUDEKIMP_STORY_ACTION_BUSY);
    *(int *)(speed+0x20)=0; native_active[3]=TRUE;
    assert(SudekiMpLanStoryCastSubmitSpirit(&w,&r,&key,1)==SUDEKIMP_STORY_ACTION_BUSY);
    native_active[3]=FALSE; lineage_idle=FALSE;
    assert(SudekiMpLanStoryCastSubmitSpirit(&w,&r,&key,1)==SUDEKIMP_STORY_ACTION_BUSY);
    lineage_idle=TRUE; known=FALSE;
    assert(SudekiMpLanStoryCastSubmitSpirit(&w,&r,&key,1)==SUDEKIMP_STORY_ACTION_BUSY);
    known=TRUE; assert(!spirits && !enters);
    assert(SudekiMpLanStoryCastSubmitSpirit(&w,&r,&key,1)==SUDEKIMP_STORY_ACTION_STARTED);
    assert(spirits==1 && caster[2].spirit_started && caster[2].spirit_active_seen && task_routed);
    assert(!submitting_spirit && submitting==-1 && !authority(2) && !all_idle());
    assert(!SudekiMpLanStoryCastDrained(actors[2]));
    assert(SudekiMpLanStoryCastSubmitSpirit(&w,&r,&key,2)==SUDEKIMP_STORY_ACTION_BUSY && spirits==1);
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_BUSY);
    /* Disconnect/stop revokes admission while the native Spirit and its
     * descendants retain their original namespace until positive drain. */
    lease_exact=FALSE; SudekiMpLanStoryCastRequestStop();
    /* Retail F900 -> 10C20 -> ROOT_SPIRIT 10DE5 runs on a later tick,
     * after request admission has ended. Only that retained manager scope
     * may create the accepted cast's root, including during disconnect. */
    assert(!root_owner(actors[2],2,&life,&type));
    neutral_scope=FALSE;
    known=FALSE; assert(!root_owner(actors[2],2,&life,&type)); known=TRUE;
    identity=FALSE; assert(!root_owner(actors[2],2,&life,&type)); identity=TRUE;
    spirit_active=FALSE; assert(!root_owner(actors[2],2,&life,&type)); spirit_active=TRUE;
    assert(root_owner(actors[2],2,&life,&type) && life==9 && type==0x23);
    assert(!root_owner(actors[2],2,&life,&type)); /* No second accepted root. */
    neutral_scope=TRUE;
    task=(SudekiMpLanCastOwner){.actor=actors[2],.session=9,.cast_id=2,.kind=2};
    assert(enter_task(&task)==13 && SudekiMpLanStoryCastService(&w,&r));
    spirit_active=FALSE; instance_idle[2]=TRUE; lineage_idle=FALSE;
    assert(SudekiMpLanStoryCastService(&w,&r) && caster[2].spirit_started && task_routed);
    lineage_idle=TRUE; effects_live=TRUE;
    assert(SudekiMpLanStoryCastService(&w,&r) && !caster[2].spirit_started && task_routed);
    effects_live=FALSE;
    assert(SudekiMpLanStoryCastService(&w,&r) && !task_routed && routing_off==1);
    reset(); observe_spirit_start=FALSE;
    assert(SudekiMpLanStoryCastSubmitSpirit(&w,&r,&key,2)==SUDEKIMP_STORY_ACTION_STARTED);
    assert(SudekiMpLanStoryCastService(&w,&r) && caster[2].spirit_started && !all_idle());
    reset(); accept_spirit=FALSE; spirit_rejection=1;
    assert(SudekiMpLanStoryCastSubmitSpirit(&w,&r,&key,1)==SUDEKIMP_STORY_ACTION_NO_SP && !spirits);
    spirit_rejection=2;
    assert(SudekiMpLanStoryCastSubmitSpirit(&w,&r,&key,1)==SUDEKIMP_STORY_ACTION_BUSY && !spirits);
    reset(); spirit_healthy=FALSE;
    assert(SudekiMpLanStoryCastSubmitSpirit(&w,&r,&key,1)==SUDEKIMP_STORY_ACTION_RETAINED);
    assert(fault && caster[2].spirit_started && !submitting_spirit && submitting==-1);
    VirtualFree(image,0,MEM_RELEASE);
    puts("story cast synthetic sparse admission/retained task/drain tests passed (no gameplay)");
    return 0;
}

BOOL SudekiMpCheckLoadedExecutable(HMODULE m) {return m!=NULL;}
BOOL SudekiMpLanStoryRealtimeExact(HMODULE m) {return m!=NULL;}

int SudekiMpRouteSpiritInstanceRenderCamera(void *m,const char *n,unsigned *k) {(void)m;(void)n;(void)k;assert(!"unexpected camera selection");return 0;}
