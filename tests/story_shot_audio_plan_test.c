#include "network/lan_story_shot_audio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Pure planner cases (spec section 8.1 plus check #4, #11, #14, #16). */
enum { TOKEN=0x51, EPOCH=7, REVISION=3, GENERATION=9, STREAM=0x1234 };
#define PLAYED SUDEKIMP_SHOT_AUDIO_PLAYED
#define SILENT SUDEKIMP_SHOT_AUDIO_SILENT
#define DEFER SUDEKIMP_SHOT_AUDIO_DEFER
#define K105 SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK105_PROJECTILE02
#define K106 SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK106_PROJECTILE03

static SudekiMpStoryShotAudioAction acts[SUDEKIMP_SHOT_AUDIO_MAX_ACTIONS];
static unsigned passed;
static void pass(const char *name) { ++passed; printf("PASS %s\n",name); }

static void journal(SudekiMpLanStoryFrame *f,uint32_t host_tick) {
    memset(f,0,sizeof(*f));
    f->epoch=EPOCH; f->revision=REVISION; f->host_tick=host_tick; f->sequence=1;
    f->available_mask=0xf; f->leader_character=2;
    for(unsigned c=0;c<4u;++c) { f->actors[c].generation=GENERATION; f->actors[c].character=(uint8_t)c; }
}
static void shoot(SudekiMpLanStoryFrame *f,unsigned c,uint32_t tick,uint8_t item) {
    SudekiMpLanStoryShots *s=&f->actors[c].shots;
    SudekiMpLanStoryShot e={.sequence=s->latest+1u,.host_tick=tick,.item=item,.pre_charge_q8=256};
    e.origin[0]=(float)e.sequence; e.origin[1]=2.0f; e.origin[2]=(float)c; e.direction[2]=1.0f;
    assert(SudekiMpLanStoryShotsAppend(s,c,&e));
}
static void roster(SudekiMpStoryVfx *v,uint64_t stream,uint32_t epoch,uint32_t tick) {
    memset(v,0,sizeof(*v));
    v->epoch=epoch; v->revision=REVISION; v->tick=tick; v->sequence=1; v->stream=stream;
    v->observed=1; strcpy(v->current,"test_area");
}
static SudekiMpLanArenaSpiritVfxSnapshot *entry(SudekiMpStoryVfx *v,uint32_t instance,unsigned kind,
    unsigned owner,uint32_t emitted,float x) {
    assert(v->count<SUDEKIMP_STORY_VFX_CAPACITY);
    SudekiMpLanArenaSpiritVfxSnapshot *e=&v->entries[v->count++];
    memset(e,0,sizeof(*e));
    e->instance_sequence=instance; e->kind=(uint8_t)kind; e->owner_actor_type=(uint8_t)owner;
    e->emitted_host_tick=emitted; e->position[0]=x; e->position[1]=1.0f; e->position[2]=-4.0f;
    e->rotation_xyzw[3]=1.0f; e->scale[0]=e->scale[1]=e->scale[2]=1.0f;
    return e;
}
static SudekiMpStoryShotAudioInput input(uint32_t now,const SudekiMpLanStoryFrame *j,const SudekiMpStoryVfx *v) {
    if(v && v->observed) assert(SudekiMpStoryVfxValid(v)); /* Fixtures must be real rosters. */
    return (SudekiMpStoryShotAudioInput){.now=now,.namespace_token=TOKEN,.local_character=2,
        .available_mask=0xf,.journal=j,.vfx=v};
}
static unsigned step_max(SudekiMpStoryShotAudioPlan *p,const SudekiMpStoryShotAudioInput *in,unsigned max) {
    memset(acts,0xcd,sizeof(acts));
    unsigned n=SudekiMpStoryShotAudioPlanStep(p,in,acts,max);
    assert(n<=max);
    return n;
}
static unsigned step(SudekiMpStoryShotAudioPlan *p,const SudekiMpStoryShotAudioInput *in) {
    return step_max(p,in,SUDEKIMP_SHOT_AUDIO_MAX_ACTIONS);
}
static void commit_all(SudekiMpStoryShotAudioPlan *p,unsigned n,unsigned result) {
    for(unsigned i=0;i<n;++i) SudekiMpStoryShotAudioPlanCommit(p,&acts[i],result);
}
static void is(unsigned i,unsigned type,uint32_t key) {
    assert(acts[i].type==type && acts[i].key==key);
}
static void fresh(SudekiMpStoryShotAudioPlan *p) { SudekiMpStoryShotAudioPlanReset(p); }

static void seeding_and_exactly_once(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpLanStoryFrame f; fresh(&p);
    journal(&f,1000); shoot(&f,3,900,12); shoot(&f,3,950,12); shoot(&f,3,990,17);
    SudekiMpStoryShotAudioInput in=input(1,&f,NULL);
    assert(step(&p,&in)==0); /* Fresh history (<=250 ms) is seeded, not replayed. */
    assert(step(&p,&in)==0);
    shoot(&f,3,1000,17);
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,4);
    assert(acts[0].character==3 && acts[0].item==17 && acts[0].emitted_tick==1000 && !acts[0].local &&
        !acts[0].vfx_kind && acts[0].position[0]==4.0f && acts[0].position[1]==2.0f && acts[0].position[2]==3.0f);
    commit_all(&p,1,PLAYED);
    assert(step(&p,&in)==0); /* Retransmitted journal: nothing. */
    f.host_tick=1010; f.sequence=2;
    assert(step(&p,&in)==0);
    pass("cursor seeding gives no historical replay; one FIRE per sequence; retransmit gives none");
}
static void one_per_character_per_step(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpLanStoryFrame f; fresh(&p);
    journal(&f,1000);
    SudekiMpStoryShotAudioInput in=input(1,&f,NULL);
    assert(step(&p,&in)==0);
    shoot(&f,3,990,12); shoot(&f,3,995,12); shoot(&f,1,1000,24);
    assert(step(&p,&in)==2);
    is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,1); assert(acts[0].character==1 && acts[0].item==24);
    is(1,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,1); assert(acts[1].character==3 && acts[1].item==12);
    commit_all(&p,2,PLAYED);
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,2); assert(acts[0].character==3);
    commit_all(&p,1,PLAYED);
    assert(step(&p,&in)==0);
    pass("at most one FIRE per character per step, in character order, then the next sequence");
}
static void namespace_reseeds(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpLanStoryFrame f; fresh(&p);
    journal(&f,1000); shoot(&f,3,1000,12);
    SudekiMpStoryShotAudioInput in=input(1,&f,NULL);
    assert(step(&p,&in)==0);
    /* Actor generation change: history of the new incarnation is seeded. */
    shoot(&f,3,1000,12); f.actors[3].generation=GENERATION+1;
    assert(step(&p,&in)==0);
    shoot(&f,3,1000,12);
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3); commit_all(&p,1,PLAYED);
    /* Journal epoch change. */
    shoot(&f,3,1000,12); f.epoch=EPOCH+1;
    assert(step(&p,&in)==0);
    shoot(&f,3,1000,12);
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,5); commit_all(&p,1,PLAYED);
    /* Lease/session namespace token change. */
    shoot(&f,3,1000,12); in.namespace_token=TOKEN+1;
    assert(step(&p,&in)==0);
    shoot(&f,3,1000,12);
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,7); commit_all(&p,1,PLAYED);
    /* Unavailable locally, then available again: seeded, nothing replays. */
    in.available_mask=0x7; shoot(&f,3,1000,12);
    assert(step(&p,&in)==0);
    in.available_mask=0xf;
    assert(step(&p,&in)==0);
    shoot(&f,3,1000,12);
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,9); commit_all(&p,1,PLAYED);
    /* Host frame mask without the shooter. */
    f.available_mask=0x7; shoot(&f,3,1000,12);
    assert(step(&p,&in)==0);
    f.available_mask=0xf;
    assert(step(&p,&in)==0);
    pass("generation, journal epoch, lease token and availability changes reseed to latest");
}
static void journal_restart_reseeds(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpLanStoryFrame f; fresh(&p);
    journal(&f,1000);
    SudekiMpStoryShotAudioInput in=input(1,&f,NULL);
    assert(step(&p,&in)==0);
    for(unsigned i=0;i<6u;++i) {
        shoot(&f,3,1000,12);
        assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,i+1u); commit_all(&p,1,PLAYED);
    }
    /* An older view of the same journal (a lower frame sequence; latest
     * below the cursor) keeps the cursor: the newer frame never replays. */
    SudekiMpLanStoryFrame newest=f; newest.sequence=f.sequence+10u;
    in.journal=&newest; assert(step(&p,&in)==0);
    SudekiMpLanStoryFrame older=newest; older.sequence=newest.sequence-1u;
    older.actors[3].shots.latest-=1u; older.actors[3].shots.count-=1u;
    in.journal=&older; assert(step(&p,&in)==0);
    in.journal=&newest; assert(step(&p,&in)==0);
    in.journal=&f; f.sequence=newest.sequence+1u;
    /* Host journal reset (world/group/actor pointer) with no namespace change,
     * in a newer frame. */
    memset(&f.actors[3].shots,0,sizeof(f.actors[3].shots));
    shoot(&f,3,1000,12); shoot(&f,3,1000,12);
    assert(f.actors[3].shots.latest==2);
    assert(step(&p,&in)==0); /* latest < cursor: reseed, play nothing. */
    shoot(&f,3,1000,12); ++f.sequence;
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3);
    pass("journal restart below the cursor reseeds instead of going silent; an older view never replays");
}
static void defer_and_silent(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpLanStoryFrame f; fresh(&p);
    journal(&f,1000);
    SudekiMpStoryShotAudioInput in=input(1,&f,NULL);
    assert(step(&p,&in)==0);
    shoot(&f,3,1000,12);
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,1); commit_all(&p,1,DEFER);
    shoot(&f,3,1100,14); f.host_tick=1100;
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,1); commit_all(&p,1,DEFER);
    f.host_tick=1250;
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,1); commit_all(&p,1,77u); /* unknown = DEFER */
    f.host_tick=1251; /* Shot 1 is now older than 250 ms of host clock. */
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,2); assert(acts[0].item==14);
    commit_all(&p,1,SILENT);
    assert(step(&p,&in)==0); /* SILENT commits the sequence. */
    f.host_tick=1300;
    assert(step(&p,&in)==0);
    pass("DEFER retries until 250 ms of host clock, then the next shot; SILENT commits");
}
static void local_flag_source(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpLanStoryFrame f; fresh(&p);
    journal(&f,1000); f.leader_character=1; /* Host leader is not the local character. */
    SudekiMpStoryShotAudioInput in=input(1,&f,NULL); in.local_character=3;
    assert(step(&p,&in)==0);
    shoot(&f,1,1000,24); shoot(&f,3,1000,12);
    assert(step(&p,&in)==2);
    assert(acts[0].character==1 && !acts[0].local && acts[1].character==3 && acts[1].local==1);
    commit_all(&p,2,PLAYED);
    in.local_character=4; shoot(&f,1,1000,24); shoot(&f,3,1000,12);
    assert(step(&p,&in)==2 && !acts[0].local && !acts[1].local);
    commit_all(&p,2,PLAYED);
    in.local_character=1; shoot(&f,1,1000,24); shoot(&f,3,1000,12);
    assert(step(&p,&in)==2 && acts[0].local==1 && !acts[1].local);
    pass("local flag comes only from input local_character, never from the host leader");
}
static void travel_binding(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpLanStoryFrame f; SudekiMpStoryVfx v; fresh(&p);
    journal(&f,1000);
    roster(&v,STREAM,EPOCH,1000);
    SudekiMpStoryShotAudioInput in=input(1,&f,&v);
    assert(step(&p,&in)==0);
    /* Shots of both characters; the Ailish shot at 980 binds instance 10. */
    shoot(&f,3,980,17); shoot(&f,1,980,24);
    assert(step(&p,&in)==2); commit_all(&p,2,PLAYED); /* The two FIREs. */
    v.tick=1000; v.sequence=2;
    /* Never travel: flashes (incl. every aimed-flash kind), hits, magic wall. */
    static const unsigned ailish_never[]={35,36,37,38,39,40,41,42,SUDEKIMP_LAN_STORY_VFX_MAGIC_WALL_HIT};
    static const unsigned elco_never[]={52,53,54,55,56,57,58,SUDEKIMP_LAN_STORY_VFX_MAGIC_WALL_HIT};
    uint32_t instance=1;
    for(unsigned i=0;i<sizeof(ailish_never)/sizeof(ailish_never[0]);++i)
        entry(&v,instance++,ailish_never[i],SUDEKIMP_LAN_ARENA_AILISH_TYPE,990,0.0f);
    for(unsigned i=0;i<sizeof(elco_never)/sizeof(elco_never[0]);++i)
        entry(&v,instance++,elco_never[i],SUDEKIMP_LAN_ARENA_ELCO_TYPE,990,0.0f);
    assert(step(&p,&in)==0);
    /* Projectile kinds bind to the nearest journal shot of the same character. */
    entry(&v,40,43,SUDEKIMP_LAN_ARENA_AILISH_TYPE,1000,5.0f);
    entry(&v,41,66,SUDEKIMP_LAN_ARENA_ELCO_TYPE,1000,6.0f);
    assert(step(&p,&in)==2);
    is(0,SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,40);
    assert(acts[0].character==3 && acts[0].item==17 && acts[0].vfx_kind==43 && acts[0].emitted_tick==1000 &&
        acts[0].position[0]==5.0f && !acts[0].local);
    is(1,SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,41); assert(acts[1].character==1 && acts[1].item==24);
    commit_all(&p,2,PLAYED);
    assert(step(&p,&in)==0); /* Live and unmoved. */
    /* Window: shot at 1000 binds VFX emitted 1100 (+100) and 970 (-30), not 1101 or 969. */
    fresh(&p); journal(&f,1000); roster(&v,STREAM,EPOCH,1100);
    assert(step(&p,&in)==0); shoot(&f,3,1000,12);
    assert(step(&p,&in)==1); commit_all(&p,1,PLAYED);
    v.tick=1101;
    entry(&v,50,43,SUDEKIMP_LAN_ARENA_AILISH_TYPE,1101,0.0f);
    entry(&v,51,44,SUDEKIMP_LAN_ARENA_AILISH_TYPE,1100,0.0f);
    entry(&v,52,45,SUDEKIMP_LAN_ARENA_AILISH_TYPE,969,0.0f);
    entry(&v,53,46,SUDEKIMP_LAN_ARENA_AILISH_TYPE,970,0.0f);
    assert(step(&p,&in)==2); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,51); is(1,SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,53);
    /* A shot of the other character never binds; nor a journal of another epoch. */
    fresh(&p); journal(&f,1000); roster(&v,STREAM,EPOCH,1000);
    assert(step(&p,&in)==0); shoot(&f,1,1000,24);
    assert(step(&p,&in)==1); commit_all(&p,1,PLAYED);
    entry(&v,60,43,SUDEKIMP_LAN_ARENA_AILISH_TYPE,1000,0.0f);
    assert(step(&p,&in)==0);
    entry(&v,61,59,SUDEKIMP_LAN_ARENA_ELCO_TYPE,1000,0.0f);
    f.epoch=EPOCH+1;
    assert(step(&p,&in)==0);
    pass("travel only with a journal shot of the same character in -30..+100 ms; never flash or hit kinds");
}
static void unbound_travel_expires(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpLanStoryFrame f; SudekiMpStoryVfx v; fresh(&p);
    journal(&f,1000); roster(&v,STREAM,EPOCH,1000);
    entry(&v,10,43,SUDEKIMP_LAN_ARENA_AILISH_TYPE,1000,0.0f);
    SudekiMpStoryShotAudioInput in=input(1,NULL,&v);
    assert(step(&p,&in)==0); /* No journal yet: pending. */
    v.tick=1200; in.journal=&f;
    assert(step(&p,&in)==0); /* Seeds the journal; no shot. */
    shoot(&f,3,1000,12); f.host_tick=1200;
    assert(step(&p,&in)==2); /* Bound at age 200 (and the FIRE of the new shot). */
    is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,1); is(1,SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,10);
    commit_all(&p,1,PLAYED); SudekiMpStoryShotAudioPlanCommit(&p,&acts[1],DEFER);
    v.tick=1201;
    assert(step(&p,&in)==0); /* Age 201: decided silent, even with the binding present. */
    v.tick=1150;
    assert(step(&p,&in)==0);
    /* Never bound: silent after 200 ms, a late shot changes nothing. */
    fresh(&p); journal(&f,1000); roster(&v,STREAM,EPOCH,1000);
    entry(&v,10,43,SUDEKIMP_LAN_ARENA_AILISH_TYPE,1000,0.0f);
    in.journal=&f;
    assert(step(&p,&in)==0);
    v.tick=1150; assert(step(&p,&in)==0);
    v.tick=1201; assert(step(&p,&in)==0);
    shoot(&f,3,1000,12); v.tick=1150; f.host_tick=1150;
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,1);
    /* First seen already older than 200 ms. */
    fresh(&p); journal(&f,1000); shoot(&f,3,1000,12); roster(&v,STREAM,EPOCH,1201);
    entry(&v,10,43,SUDEKIMP_LAN_ARENA_AILISH_TYPE,1000,0.0f);
    assert(step(&p,&in)==0);
    pass("an unbound travel goes silent after 200 ms of VFX host clock");
}
static void move_retire(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpLanStoryFrame f; SudekiMpStoryVfx v; fresh(&p);
    journal(&f,1000); roster(&v,STREAM,EPOCH,1000);
    SudekiMpStoryShotAudioInput in=input(1,&f,&v);
    assert(step(&p,&in)==0);
    shoot(&f,3,1000,12);
    assert(step(&p,&in)==1); commit_all(&p,1,PLAYED);
    SudekiMpLanArenaSpiritVfxSnapshot *e=entry(&v,10,43,SUDEKIMP_LAN_ARENA_AILISH_TYPE,1000,1.0f);
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,10); commit_all(&p,1,PLAYED);
    v.tick=1016; v.sequence=2;
    assert(step(&p,&in)==0); /* Same position bits: no MOVE. */
    e->position[0]=2.5f;
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_MOVE,10);
    assert(acts[0].position[0]==2.5f && acts[0].character==3 && acts[0].vfx_kind==43);
    commit_all(&p,1,PLAYED);
    assert(step(&p,&in)==0);
    e->position[0]=3.0f;
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_MOVE,10); commit_all(&p,1,DEFER);
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_MOVE,10); commit_all(&p,1,SILENT);
    assert(step(&p,&in)==0);
    v.count=0; memset(v.entries,0,sizeof(v.entries));
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,10); assert(acts[0].vfx_kind==43);
    assert(step(&p,&in)==0);
    entry(&v,10,43,SUDEKIMP_LAN_ARENA_AILISH_TYPE,1000,3.0f);
    assert(step(&p,&in)==0); /* Back again: resurrection, silent. */
    /* A decided (never live) entry leaves without a RETIRE. */
    entry(&v,11,43,SUDEKIMP_LAN_ARENA_AILISH_TYPE,1016,3.0f);
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,11); commit_all(&p,1,SILENT);
    v.count=0; memset(v.entries,0,sizeof(v.entries));
    assert(step(&p,&in)==0);
    pass("MOVE only on a position change; RETIRE only for a live entry absent from a fresh roster");
}
static void stale_roster(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpStoryVfx v; fresh(&p);
    roster(&v,STREAM,EPOCH,1000);
    entry(&v,10,K105,255,1000,1.0f); entry(&v,11,K106,255,1000,2.0f);
    SudekiMpStoryShotAudioInput in=input(100,NULL,&v);
    assert(step(&p,&in)==4); commit_all(&p,4,PLAYED);
    SudekiMpStoryShotAudioInput none=input(350,NULL,NULL);
    assert(step(&p,&none)==0); /* 250 ms without a fresh roster: still retained. */
    SudekiMpStoryVfx unobserved=v; unobserved.observed=0; unobserved.count=0;
    memset(unobserved.entries,0,sizeof(unobserved.entries));
    SudekiMpStoryShotAudioInput unknown=input(300,NULL,&unobserved);
    assert(step(&p,&unknown)==0); /* Unknown is not an empty deletion roster. */
    none.now=351;
    assert(step(&p,&none)==2); is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,10); is(1,SUDEKIMP_SHOT_AUDIO_RETIRE,11);
    assert(step(&p,&none)==0);
    /* The same instances return still young: never replayed. A newer one plays. */
    v.tick=1100; in.now=400;
    assert(step(&p,&in)==0);
    entry(&v,12,K105,255,1100,3.0f);
    assert(step(&p,&in)==2); is(0,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,12); is(1,SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,12);
    pass("stale roster over 250 ms retires everything; nothing replays afterwards");
}
static void resurrection_and_order(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpStoryVfx v; fresh(&p);
    roster(&v,STREAM,EPOCH,1000);
    entry(&v,30,K105,255,1000,3.0f); entry(&v,10,K106,255,990,1.0f); entry(&v,20,K105,255,995,2.0f);
    SudekiMpStoryShotAudioInput in=input(1,NULL,&v);
    assert(step(&p,&in)==6);
    is(0,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,10); is(1,SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,10);
    is(2,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,20); is(3,SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,20);
    is(4,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,30); is(5,SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,30);
    assert(acts[0].character==4 && !acts[0].item && acts[0].vfx_kind==K106 && acts[0].emitted_tick==990);
    commit_all(&p,6,PLAYED);
    /* Older unseen instance: resurrection, ignored. Retire then moves then new cues. */
    v.count=0; memset(v.entries,0,sizeof(v.entries));
    entry(&v,5,K105,255,1000,0.0f);
    entry(&v,30,K105,255,1000,3.5f);
    entry(&v,20,K105,255,995,2.5f);
    entry(&v,40,K106,255,1000,4.0f);
    assert(step(&p,&in)==5);
    is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,10);
    is(1,SUDEKIMP_SHOT_AUDIO_MOVE,20); is(2,SUDEKIMP_SHOT_AUDIO_MOVE,30);
    is(3,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,40); is(4,SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,40);
    /* Same instance with another identity is a new identity: retired, never restarted. */
    commit_all(&p,5,PLAYED);
    v.entries[1].emitted_host_tick=999;
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,30);
    assert(step(&p,&in)==0);
    pass("resurrection ignored; deterministic order: retires, moves, then new cues ascending");
}
static void world_first_sight_and_atomicity(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpStoryVfx v; fresh(&p);
    roster(&v,STREAM,EPOCH,1250);
    entry(&v,10,K105,255,1000,1.0f); /* age 250: plays */
    entry(&v,11,K106,255,999,2.0f);  /* age 251: late join, silent */
    SudekiMpStoryShotAudioInput in=input(1,NULL,&v);
    assert(step(&p,&in)==2);
    is(0,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,10); is(1,SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,10);
    assert(acts[0].position[0]==1.0f && acts[1].position[0]==1.0f);
    /* Fire PLAYED, travel DEFER: only travel is retried. */
    SudekiMpStoryShotAudioPlanCommit(&p,&acts[0],PLAYED); SudekiMpStoryShotAudioPlanCommit(&p,&acts[1],DEFER);
    v.entries[0].position[0]=1.5f; v.sequence=2;
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,10); assert(acts[0].position[0]==1.5f);
    commit_all(&p,1,PLAYED);
    assert(step(&p,&in)==0); /* Retransmit: nothing. */
    /* Fire DEFER, travel PLAYED: fire retried at its first-sight point. */
    fresh(&p); roster(&v,STREAM,EPOCH,1000);
    entry(&v,10,K105,255,1000,1.0f);
    assert(step(&p,&in)==2);
    SudekiMpStoryShotAudioPlanCommit(&p,&acts[0],DEFER); SudekiMpStoryShotAudioPlanCommit(&p,&acts[1],PLAYED);
    v.tick=1100; v.entries[0].position[0]=4.0f;
    assert(step(&p,&in)==2);
    is(0,SUDEKIMP_SHOT_AUDIO_MOVE,10); assert(acts[0].position[0]==4.0f);
    is(1,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,10); assert(acts[1].position[0]==1.0f);
    commit_all(&p,2,PLAYED);
    /* While fire was pending, `position` held its first-sight point, so the
     * travel target is re-sent once after fire resolves, then adopted. */
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_MOVE,10); assert(acts[0].position[0]==4.0f);
    commit_all(&p,1,PLAYED);
    assert(step(&p,&in)==0);
    /* Both DEFER; the projectile moves; travel then starts where it is, and
     * the still-pending fire keeps the first-sight point. */
    fresh(&p); roster(&v,STREAM,EPOCH,1000);
    entry(&v,10,K105,255,1000,1.0f);
    assert(step(&p,&in)==2); commit_all(&p,2,DEFER);
    v.tick=1050; v.entries[0].position[0]=2.0f;
    assert(step(&p,&in)==2);
    is(0,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,10); assert(acts[0].position[0]==1.0f);
    is(1,SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,10); assert(acts[1].position[0]==2.0f);
    SudekiMpStoryShotAudioPlanCommit(&p,&acts[0],DEFER); SudekiMpStoryShotAudioPlanCommit(&p,&acts[1],PLAYED);
    v.tick=1100;
    assert(step(&p,&in)==2);
    is(0,SUDEKIMP_SHOT_AUDIO_MOVE,10); assert(acts[0].position[0]==2.0f);
    is(1,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,10); assert(acts[1].position[0]==1.0f);
    /* Both DEFER until age 250, then decided; each cue is exactly once. */
    fresh(&p); roster(&v,STREAM,EPOCH,1000);
    entry(&v,10,K106,255,1000,1.0f);
    assert(step(&p,&in)==2); commit_all(&p,2,DEFER);
    v.tick=1250; assert(step(&p,&in)==2); commit_all(&p,2,DEFER);
    v.tick=1251; assert(step(&p,&in)==0);
    v.count=0; memset(v.entries,0,sizeof(v.entries));
    assert(step(&p,&in)==0); /* Never live: no RETIRE. */
    /* Fire SILENT, travel PLAYED; then absence retires the instance. */
    fresh(&p); roster(&v,STREAM,EPOCH,1000);
    entry(&v,10,K106,255,1000,1.0f);
    assert(step(&p,&in)==2);
    SudekiMpStoryShotAudioPlanCommit(&p,&acts[0],SILENT); SudekiMpStoryShotAudioPlanCommit(&p,&acts[1],PLAYED);
    assert(step(&p,&in)==0);
    v.count=0; memset(v.entries,0,sizeof(v.entries));
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,10);
    /* Fire PLAYED, travel SILENT: fire is live (stopped on RETIRE), no MOVE. */
    fresh(&p); roster(&v,STREAM,EPOCH,1000);
    entry(&v,10,K106,255,1000,1.0f);
    assert(step(&p,&in)==2);
    SudekiMpStoryShotAudioPlanCommit(&p,&acts[0],PLAYED); SudekiMpStoryShotAudioPlanCommit(&p,&acts[1],SILENT);
    v.entries[0].position[0]=9.0f;
    assert(step(&p,&in)==0);
    v.count=0; memset(v.entries,0,sizeof(v.entries));
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,10);
    pass("world first sight <=250 ms emits WORLD_FIRE and WORLD_TRAVEL, >250 ms silent; separate commit state");
}
static void world_kind_allow_list(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpStoryVfx v; fresh(&p);
    roster(&v,STREAM,EPOCH,1000);
    static const unsigned others[]={
        SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK100_FIRE_EFFECT,SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK101_PROJECTILE,
        SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK102_BLOCK_INTO,SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK103_BLOCK_IDLE,
        SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK104_BLOCK_OUT,SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK107_FIRELARGE,
        SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK108_FIRELARGE_PROJECTILE,SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK109_MINE_DROP,
        SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK110_MINE_IDLE,SUDEKIMP_LAN_STORY_VFX_WORLD_I315_MONEY};
    for(unsigned i=0;i<sizeof(others)/sizeof(others[0]);++i) entry(&v,i+1u,others[i],255,1000,0.0f);
    SudekiMpStoryShotAudioInput in=input(1,NULL,&v);
    assert(step(&p,&in)==0);
    entry(&v,20,K105,255,1000,0.0f); entry(&v,21,K106,255,1000,0.0f);
    assert(step(&p,&in)==4);
    is(0,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,20); is(2,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,21);
    assert(acts[0].vfx_kind==K105 && acts[2].vfx_kind==K106);
    pass("world kinds allow-listed to SFXKK105_PROJECTILE02 and SFXKK106_PROJECTILE03 with owner 255");
}
static void seen_overflow(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpStoryVfx v; fresh(&p);
    roster(&v,STREAM,EPOCH,1000);
    for(unsigned i=1;i<=SUDEKIMP_STORY_VFX_CAPACITY;++i) entry(&v,i,K105,255,1000,(float)i);
    SudekiMpStoryShotAudioInput in=input(1,NULL,&v);
    unsigned total=0,n;
    while((n=step(&p,&in))) { commit_all(&p,n,PLAYED); total+=n; }
    assert(total==2u*SUDEKIMP_STORY_VFX_CAPACITY);
    /* Instance 1 leaves and 65 arrives; with no room for the RETIRE the seen
     * table stays full: 65 is ignored silently. */
    memmove(v.entries,v.entries+1,(SUDEKIMP_STORY_VFX_CAPACITY-1u)*sizeof(v.entries[0]));
    --v.count; entry(&v,65,K106,255,1000,65.0f);
    assert(!p.overflow_logged);
    assert(SudekiMpStoryShotAudioPlanStep(&p,&in,NULL,0)==0);
    assert(p.overflow_logged==1);
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,1);
    assert(step(&p,&in)==0); /* 65 stays silent. */
    v.entries[SUDEKIMP_STORY_VFX_CAPACITY-1u].instance_sequence=66;
    assert(step(&p,&in)==2);
    is(0,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,66); is(1,SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,66);
    pass("seen overflow ignores the entry silently with no crash");
}
static void bounded_actions(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpLanStoryFrame f; SudekiMpStoryVfx v; fresh(&p);
    journal(&f,1000); roster(&v,STREAM,EPOCH,1000);
    entry(&v,10,K105,255,1000,1.0f);
    SudekiMpStoryShotAudioInput in=input(1,&f,&v);
    assert(step(&p,&in)==2); commit_all(&p,2,PLAYED);
    v.count=0; memset(v.entries,0,sizeof(v.entries));
    entry(&v,11,K106,255,1000,2.0f); shoot(&f,3,1000,12);
    assert(step_max(&p,&in,1)==1); is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,10);
    assert(step_max(&p,&in,2)==2); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,1); is(1,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,11);
    commit_all(&p,2,PLAYED);
    assert(step_max(&p,&in,2)==1); is(0,SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,11);
    commit_all(&p,1,PLAYED);
    assert(step_max(&p,&in,2)==0);
    /* A RETIRE that does not fit keeps its entry; nothing is lost. */
    v.count=0; memset(v.entries,0,sizeof(v.entries));
    assert(SudekiMpStoryShotAudioPlanStep(&p,&in,NULL,0)==0);
    assert(step_max(&p,&in,1)==1); is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,11);
    assert(step_max(&p,&in,1)==0);
    pass("a bounded action buffer keeps priority order and retries the remainder");
}
static void vfx_namespace_changes(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpLanStoryFrame f; SudekiMpStoryVfx v; fresh(&p);
    journal(&f,1000); shoot(&f,3,1000,12); roster(&v,STREAM,EPOCH,1000);
    entry(&v,10,K105,255,1000,1.0f); entry(&v,12,43,SUDEKIMP_LAN_ARENA_AILISH_TYPE,1000,1.0f);
    SudekiMpStoryShotAudioInput in=input(1,&f,&v);
    assert(step(&p,&in)==3); commit_all(&p,3,PLAYED);
    /* New stream: retire everything, then the restarted instance numbering plays. */
    roster(&v,STREAM+1,EPOCH,1000); entry(&v,1,K106,255,1000,1.0f);
    assert(step(&p,&in)==4);
    is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,10); is(1,SUDEKIMP_SHOT_AUDIO_RETIRE,12);
    is(2,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,1); is(3,SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,1);
    commit_all(&p,4,PLAYED);
    /* New epoch (spec F, as the native visual follow): retire, forget newest. */
    v.epoch=EPOCH+1;
    assert(step(&p,&in)==3);
    is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,1); is(1,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,1); is(2,SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,1);
    commit_all(&p,3,PLAYED);
    /* Same stream, new revision: retire, keep the newest watermark (no replay). */
    v.revision=REVISION+1;
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,1);
    assert(step(&p,&in)==0);
    /* Lease token change (spec F): everything retired and forgotten, newest
     * included, as the native visual follow resets; a still-young entry is a
     * first sight again. The journal is reseeded (namespace_reseeds). */
    roster(&v,STREAM+1,EPOCH,1000); entry(&v,2,K106,255,1000,1.0f);
    assert(step(&p,&in)==2); commit_all(&p,2,PLAYED);
    in.namespace_token=TOKEN+9;
    assert(step(&p,&in)==3);
    is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,2); is(1,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,2); is(2,SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,2);
    commit_all(&p,3,PLAYED);
    /* A namespace change whose retires do not fit adopts nothing until they all did. */
    entry(&v,3,K105,255,1000,1.0f);
    assert(step(&p,&in)==2); commit_all(&p,2,PLAYED);
    in.namespace_token=TOKEN+10;
    assert(step_max(&p,&in,1)==1); is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,2);
    assert(step_max(&p,&in,1)==1); is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,3);
    assert(step(&p,&in)==4);
    is(0,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,2); is(2,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,3);
    commit_all(&p,4,PLAYED);
    roster(&v,STREAM+2,EPOCH,1000); entry(&v,1,K105,255,1000,1.0f);
    assert(step_max(&p,&in,1)==1); is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,2);
    assert(step_max(&p,&in,1)==1); is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,3);
    assert(step(&p,&in)==2); is(0,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,1);
    pass("VFX stream/epoch/revision and lease changes retire live cues before anything new");
}
static void commit_robustness(void) {
    SudekiMpStoryShotAudioPlan p; SudekiMpStoryVfx v; fresh(&p);
    roster(&v,STREAM,EPOCH,1000); entry(&v,10,K105,255,1000,1.0f);
    SudekiMpStoryShotAudioInput in=input(1,NULL,&v);
    assert(step(&p,&in)==2);
    SudekiMpStoryShotAudioAction bad=acts[0];
    bad.key=11; SudekiMpStoryShotAudioPlanCommit(&p,&bad,PLAYED);
    bad=acts[0]; bad.vfx_kind=K106; SudekiMpStoryShotAudioPlanCommit(&p,&bad,PLAYED);
    bad=acts[0]; bad.type=SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL; SudekiMpStoryShotAudioPlanCommit(&p,&bad,PLAYED);
    bad=acts[0]; bad.type=99; SudekiMpStoryShotAudioPlanCommit(&p,&bad,PLAYED);
    bad=acts[0]; bad.type=SUDEKIMP_SHOT_AUDIO_PARTY_FIRE; bad.character=3; SudekiMpStoryShotAudioPlanCommit(&p,&bad,PLAYED);
    SudekiMpStoryShotAudioPlanCommit(&p,NULL,PLAYED); SudekiMpStoryShotAudioPlanCommit(NULL,&acts[0],PLAYED);
    assert(SudekiMpStoryShotAudioPlanStep(NULL,&in,acts,4)==0 && SudekiMpStoryShotAudioPlanStep(&p,NULL,acts,4)==0 &&
        SudekiMpStoryShotAudioPlanStep(&p,&in,NULL,4)==0);
    assert(step(&p,&in)==2); /* Still pending: nothing above changed state. */
    is(0,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE,10); is(1,SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL,10);
    commit_all(&p,2,PLAYED); commit_all(&p,2,SILENT); /* A repeated commit is ignored. */
    v.entries[0].position[0]=1.25f;
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_MOVE,10); commit_all(&p,1,PLAYED);
    v.count=0; memset(v.entries,0,sizeof(v.entries));
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,10);
    /* A late SILENT never demotes a live world fire (it must still be retired). */
    entry(&v,11,K105,255,1000,1.0f);
    assert(step(&p,&in)==2);
    SudekiMpStoryShotAudioAction fire=acts[0];
    SudekiMpStoryShotAudioPlanCommit(&p,&acts[0],PLAYED); SudekiMpStoryShotAudioPlanCommit(&p,&acts[1],SILENT);
    SudekiMpStoryShotAudioPlanCommit(&p,&fire,SILENT);
    v.count=0; memset(v.entries,0,sizeof(v.entries));
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,11);
    /* Nor a live party travel: it keeps following and is retired. */
    SudekiMpLanStoryFrame f; journal(&f,1000); in.journal=&f;
    assert(step(&p,&in)==0); shoot(&f,3,1000,12);
    assert(step(&p,&in)==1); commit_all(&p,1,PLAYED);
    SudekiMpLanArenaSpiritVfxSnapshot *e=entry(&v,12,43,SUDEKIMP_LAN_ARENA_AILISH_TYPE,1000,1.0f);
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL,12);
    commit_all(&p,1,PLAYED); commit_all(&p,1,SILENT);
    e->position[0]=2.0f;
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_MOVE,12); commit_all(&p,1,PLAYED);
    v.count=0; memset(v.entries,0,sizeof(v.entries));
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_RETIRE,12);
    /* A stale FIRE commit never moves the cursor back (no replay); a
     * commit for a character that is not a shooter is ignored. */
    shoot(&f,3,1000,12);
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,2); fire=acts[0]; commit_all(&p,1,PLAYED);
    shoot(&f,3,1000,12);
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,3); commit_all(&p,1,PLAYED);
    SudekiMpStoryShotAudioPlanCommit(&p,&fire,PLAYED);
    assert(step(&p,&in)==0);
    fire.character=0; fire.key=50; SudekiMpStoryShotAudioPlanCommit(&p,&fire,PLAYED);
    fire.character=3; f.actors[3].generation=GENERATION+1;
    assert(step(&p,&in)==0); /* Reseeded at 3. */
    shoot(&f,3,1000,12);
    assert(step(&p,&in)==1); is(0,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE,4);
    pass("malformed, foreign, stale and repeated commits change nothing");
}
int main(void) {
    seeding_and_exactly_once();
    one_per_character_per_step();
    namespace_reseeds();
    journal_restart_reseeds();
    defer_and_silent();
    local_flag_source();
    travel_binding();
    unbound_travel_expires();
    move_retire();
    stale_roster();
    resurrection_and_order();
    world_first_sight_and_atomicity();
    world_kind_allow_list();
    seen_overflow();
    bounded_actions();
    vfx_namespace_changes();
    commit_robustness();
    printf("story_shot_audio_plan_test: %u cases passed\n",passed);
    return 0;
}
