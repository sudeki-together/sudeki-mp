/* Synthetic native ownership/activation only. No game constructors, tasks,
 * damage, rendering or native hooks run in this fixture. */
#include "../src/hooks/lan_story_cast.c"
#include <assert.h>
#include <stdio.h>
static uint8_t actors[4][0x100],skills[4][0x80],speed[0x30],front[0x178],quick[0x2a];
static BOOL identity=TRUE,lease_exact=TRUE,known=TRUE,lineage_idle=TRUE;
static BOOL instance_idle[4]={TRUE,TRUE,TRUE,TRUE},spirit_active,healthy=TRUE;
static BOOL native_active[4],accept_skill=TRUE,hold_active=TRUE;
static int rejection;
static unsigned uses,timing,enters,advances,drains;
static unsigned routing_on,routing_off;
static BOOL routing_ok=TRUE,effects_live,missiles_live,neutral_scope=TRUE;
BOOL SudekiMpLanCastContextSetTaskRouting(SudekiMpLanCastTaskEnter enter,SudekiMpLanCastTaskLeave leave) {
    assert((enter==NULL)==(leave==NULL));
    if(!routing_ok || !lineage_idle) return FALSE;
    if(enter) { assert(enter==enter_task && leave==SudekiMpLeaveSpiritInstance); ++routing_on; }
    else ++routing_off;
    return TRUE;
}
BOOL SudekiMpLeaveSpiritInstance(uint32_t cookie) { assert(cookie==13); return TRUE; }
BOOL SudekiMpObserveSpiritInstanceScope(SudekiMpSpiritInstance *out) {
    memset(out,0,sizeof(*out)); out->generation=neutral_scope?0:3; return known;
}
BOOL SudekiMpLanPartyEffectLifetimeRetains(void) { return effects_live; }
BOOL SudekiMpLanPartyProjectileLifetimeRetains(void) { return missiles_live; }
void SudekiMpLogWrite(const char *line) { (void)line; }
static int ix(void *a) { for(unsigned i=0;i<4;++i) if(a==actors[i]) return (int)i; return -1; }
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *r) {
    return identity && r==&owners;
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
    memset(o,0,sizeof(*o)); o->idle=o->body_idle=instance_idle[s->generation-1]; return TRUE;
}
BOOL SudekiMpObserveSpiritInstanceActivity(const SudekiMpSpiritInstance *s,BOOL *a) {
    (void)s; *a=spirit_active; return known;
}
BOOL SudekiMpBeginSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *s,uint16_t seq) {
    assert(s==&caster[2].instance && seq==1); ++timing; return TRUE;
}
uint32_t SudekiMpEnterSpiritInstance(const SudekiMpSpiritInstance *s) {
    assert(task_routed); /* Isolation precedes ALL private native entry. */
    if(s) assert(s==&caster[2].instance);
    ++enters; return 13;
}
BOOL SudekiMpSkillActivationRoutingHealthy(void) { return healthy; }
SudekiMpSkillActivationResult SudekiMpActivateCharacterSkillSlot(void *a,int slot) {
    assert(a==actors[2] && slot==3 && submitting==2 && authority(2)); ++uses;
    if(!accept_skill) return (SudekiMpSkillActivationResult){.status=SUDEKIMP_SKILL_ACTIVATION_VALIDATION_REJECTED,
        .validation_result=rejection};
    assert(enter_skill(skills[2],slot,TRUE)==13);
    native_active[2]=hold_active;
    return (SudekiMpSkillActivationResult){.status=SUDEKIMP_SKILL_ACTIVATION_STARTED,.skill=skills[2]};
}
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
uint8_t SudekiMpLanPartyActorType(unsigned c) { return (uint8_t)(c+1); }
static void reset(void) {
    memset(caster,0,sizeof(caster)); memset(&owners,0,sizeof(owners));
    memset(skills,0,sizeof(skills)); memset(speed,0,sizeof(speed));
    memset(front,0,sizeof(front)); memset(quick,0,sizeof(quick)); front[0x8c]=1;
    memset(native_active,0,sizeof(native_active));
    identity=lease_exact=known=lineage_idle=healthy=accept_skill=hold_active=TRUE;
    spirit_active=FALSE; bound=initialized=TRUE; stopping=fault=initializing=FALSE;
    submitting=-1; lifetime=9; native_thread=GetCurrentThreadId();
    owners.available_mask=12; owners.leader_character=3;
    for(unsigned i=2;i<4;++i) {
        caster[i].actor=owners.actors[i]=actors[i]; caster[i].instance.generation=i+1;
        *(void **)(skills[i]+0x10)=actors[i]; instance_idle[i]=TRUE;
    }
    *(void **)((uint8_t *)image+0x408da0)=speed;
    *(void **)((uint8_t *)image+0x408d1c)=front;
    *(void **)((uint8_t *)image+0x3c2f84)=quick;
    uses=timing=enters=advances=drains=0; last_service=0;
    rejection=0;
    routing_on=routing_off=0; task_routed=effects_live=missiles_live=FALSE;
    routing_ok=neutral_scope=TRUE;
}
int main(void) {
    image=VirtualAlloc(NULL,0x409000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE); assert(image);
    SudekiMpControlUpdateDispatchWitness w={.dispatch_serial=5,.service_post_original_exact=TRUE};
    SudekiMpLanStoryNativeRoster r={0}; r.actors[2]=actors[2];
    SudekiMpLanPartyLease key={.token=21,.generation=8,.seat=2};
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
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_UNAVAILABLE);
    reset(); healthy=FALSE;
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_RETAINED && fault);
    reset(); SudekiMpLanStoryCastRequestStop(); assert(!SudekiMpLanStoryCastReady());
    assert(SudekiMpLanStoryCastSubmit(&w,&r,&key,3)==SUDEKIMP_STORY_ACTION_UNAVAILABLE && !uses);
    VirtualFree(image,0,MEM_RELEASE);
    puts("story cast synthetic sparse admission/retained task/drain tests passed (no gameplay)");
    return 0;
}
