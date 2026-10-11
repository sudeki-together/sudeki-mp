/* Actual saved-story dispatch with synthetic transport/native boundaries.
 * Ensures the controller witness reaches residency before native playback. */
#define SudekiMpStoryMuzzleObserve test_muzzle_observe
#include "../src/hooks/lan_story_runtime.c"
#undef SudekiMpStoryMuzzleObserve
#include "../src/hooks/lan_story_muzzle.c"
#include <assert.h>
#include <stdio.h>
static SudekiMpStoryVfx packet={.epoch=1,.revision=2,.sequence=1,.stream=9,.current="road",.observed=1};
static SudekiMpLanStoryScene remote_scene={.epoch=1,.revision=2,.phase=SUDEKIMP_LAN_STORY_READY,
    .available_mask=12,.leader_seat=3,.world="world"};
static BOOL packet_ready=TRUE,area_ready=TRUE,reset_ready=TRUE,inside;
static unsigned creates,resets,operations,publishes,captures;
static unsigned camera_captures[4],camera_publishes[4],camera_mask=5;
static BOOL service_ready=TRUE;
static BOOL observing;
static SudekiMpLanPartyPeerStatus current_peer;
static uint64_t last_prepared;
static SudekiMpStoryVfx last_visuals;
static const SudekiMpControlUpdateDispatchWitness *expected_w;
void SudekiMpLogFormat(const char *format,...) {(void)format;}
BOOL SudekiMpLogResearchEnabled(void) {return FALSE;}
BOOL SudekiMpLanPartyPeerStatusGet(SudekiMpLanPartySession *s,unsigned seat,SudekiMpLanPartyPeerStatus *out) {
    (void)s;assert(seat==local_seat);*out=current_peer;return TRUE;
}
int SudekiMpLanStorySceneSame(const SudekiMpLanStoryScene *a,const SudekiMpLanStoryScene *b) {
    return a->epoch==b->epoch && a->revision==b->revision;
}
BOOL SudekiMpLanStoryFrameMatchesScene(const SudekiMpLanStoryFrame *f,const SudekiMpLanStoryScene *s) {
    return f->epoch==s->epoch && f->revision==s->revision && f->available_mask==s->available_mask;
}
BOOL SudekiMpLanStoryClientRosterExact(const SudekiMpLanStoryNativeRoster *r) {(void)r;assert(observing);return TRUE;}
BOOL test_muzzle_observe(HMODULE m,void *actor,uint8_t type,SudekiMpStoryMuzzle *out) {
    (void)m;assert(observing && actor==(void *)3 && type==1);
    *out=(SudekiMpStoryMuzzle){.valid=TRUE,.item=12,.position={10,1,30}};return TRUE;
}
BOOL SudekiMpLanStoryClientObserveDispatch(const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpLanStoryClientPresentation callback,void *context) {
    assert(w==expected_w && !inside && !observing);
    SudekiMpLanStoryNativeRoster r={0};r.actors[3]=(void *)3;
    observing=TRUE;BOOL result=callback(&r,&remote_scene,context);observing=FALSE;return result;
}
BOOL SudekiMpLanPartyGetStoryVfx(SudekiMpLanPartySession *s,const SudekiMpLanPartyLease *l,
    uint32_t now,SudekiMpStoryVfx *out) {
    (void)s;(void)l;(void)now;if(!packet_ready)return FALSE;*out=packet;return TRUE;
}
BOOL SudekiMpLanPartyGetStoryScene(SudekiMpLanPartySession *s,const SudekiMpLanPartyLease *l,
    uint32_t now,SudekiMpLanStoryScene *out) {(void)s;(void)l;(void)now;*out=remote_scene;return TRUE;}
unsigned SudekiMpLanPartyLocalCharacter(SudekiMpLanPartySession *s) {(void)s;return 2;}
int SudekiMpLanStorySceneCharacterAreaMatches(const SudekiMpLanStoryScene *s,unsigned c,
    const char *world,const char *temporary) {(void)c;return !strcmp(s->world,world)&&!strcmp(s->temporary,temporary);}
BOOL SudekiMpLanStoryEffectsInCurrentArea(const SudekiMpStoryEffectState *s) {
    return area_ready && !strcmp(s->current,"road");
}
BOOL SudekiMpLanStoryClientResidencyCall(const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpLanStoryClientResidencyOperation operation,void *context) {
    assert(w==expected_w && !inside);inside=TRUE;++operations;
    BOOL result=operation(context);inside=FALSE;return result;
}
BOOL SudekiMpLanStoryVfxServiceVisuals(HMODULE m,const SudekiMpStoryVfx *s,uint64_t serial,uint64_t prepared) {
    (void)m;assert(inside && !observing && serial && s->observed==packet.observed);
    ++creates;last_prepared=prepared;last_visuals=*s;
    uint64_t required=s->count==64u?UINT64_MAX:((UINT64_C(1)<<s->count)-1u);
    return service_ready && (prepared&required)==required;
}
BOOL SudekiMpLanArenaSpiritVfxResetVisuals(HMODULE m) {(void)m;assert(inside);++resets;return reset_ready;}
BOOL SudekiMpLanStoryAreaCapture(SudekiMpStoryAreaState *out) {
    memset(out,0,sizeof(*out));strcpy(out->current,"road");return area_ready;
}
BOOL SudekiMpLanStoryCastCaptureVisuals(SudekiMpStoryVfx *out) {++captures;*out=packet;return packet_ready;}
BOOL SudekiMpLanPartyPublishStoryVfx(SudekiMpLanPartySession *s,const SudekiMpStoryVfx *out) {
    assert(s==session && !strcmp(out->current,"road"));++publishes;return TRUE;
}
BOOL SudekiMpLanStoryCastCaptureView(unsigned character,SudekiMpStoryCastView *out) {
    assert(character<4); ++camera_captures[character];
    if(!(camera_mask&(1u<<character))) return FALSE;
    memset(out,0,sizeof(*out)); out->fence.character=(uint8_t)character;
    out->cast_sequence=(uint16_t)(10+character); return TRUE;
}
BOOL SudekiMpLanPartyPublishStoryCastView(SudekiMpLanPartySession *s,const SudekiMpStoryCastView *out) {
    assert(s==session && !strcmp(out->current,"road"));
    unsigned c=out->fence.character;
    assert(c<4 && (camera_mask&(1u<<c)) && out->cast_sequence==10+c);
    ++camera_publishes[c]; return TRUE;
}
int main(void) {
    session=(void *)1;
    SudekiMpControlUpdateDispatchWitness w={.dispatch_serial=5,.service_post_original_exact=TRUE};expected_w=&w;
    SudekiMpLanPartyPeerStatus peer={.lease={.token=7,.generation=3,.seat=1}};
    SudekiMpLanStoryScene native=remote_scene; uint32_t now=1000;
    follow_story_vfx(&w,&peer,now,&native);
    assert(creates==1 && operations==1 && story_vfx.active && story_vfx.serial==1);
    packet_ready=FALSE;follow_story_vfx(&w,&peer,now+=100,&native);assert(operations==1 && story_vfx.active);
    packet_ready=TRUE;area_ready=FALSE;follow_story_vfx(&w,&peer,now+=100,&native);assert(operations==1);
    area_ready=TRUE;packet.epoch=2;follow_story_vfx(&w,&peer,now+=100,&native);assert(operations==1);
    packet.epoch=1;strcpy(native.world,"other");follow_story_vfx(&w,&peer,now+=100,&native);assert(operations==1);
    native=remote_scene;packet.observed=0;++packet.sequence;follow_story_vfx(&w,&peer,now+=100,&native);
    assert(creates==2 && !resets); /* Unknown roster reaches renderer without forced removal. */
    packet.stream=10;reset_ready=FALSE;follow_story_vfx(&w,&peer,now+=100,&native);
    assert(resets==1 && creates==2 && story_vfx.stream==9 && story_vfx.active);
    reset_ready=TRUE;follow_story_vfx(&w,&peer,now+=100,&native);
    assert(resets==2 && creates==3 && story_vfx.stream==10 && story_vfx.serial==2);
    assert(reset_story_vfx(&w) && !story_vfx.active && resets==3);
    packet.observed=1; ++packet.sequence;
    follow_story_vfx(&w,&peer,now+=21,&native); assert(creates==4);
    follow_story_vfx(&w,&peer,now+=21,&native); assert(creates==4); /* same packet: no repeated seek */
    ++packet.sequence; follow_story_vfx(&w,&peer,now+=21,&native); assert(creates==5);
    ++packet.sequence; service_ready=FALSE;
    follow_story_vfx(&w,&peer,now+=21,&native); assert(creates==6 && !story_vfx.ready);
    service_ready=TRUE; follow_story_vfx(&w,&peer,now+=21,&native); assert(creates==7 && story_vfx.ready);
    cast_attempted=TRUE; captures=publishes=0;
    for(unsigned i=0;i<12;++i) publish_story_vfx(10000+i*21);
    assert(captures==12 && publishes==12); /* no hidden100ms area throttle */
    area_ready=FALSE; publish_story_vfx(11000); assert(captures==12 && publishes==12);
    area_ready=TRUE; packet_ready=FALSE; publish_story_vfx(11100); assert(captures==13 && publishes==12);
    for(unsigned c=0;c<4;++c) {
        assert(camera_captures[c]==13);
        assert(camera_publishes[c]==((camera_mask&(1u<<c))?13u:0u));
    }
    camera_mask=15; publish_story_vfx(11200);
    for(unsigned c=0;c<4;++c) assert(camera_publishes[c]==((5u&(1u<<c))?14u:1u));
    /* Real projection through the real runtime dispatch. Only transport and
     * native geometry/containment are synthetic. The VFX packet wins the
     * race; its matching party journal arrives on the following dispatch. */
    packet_ready=TRUE;packet.count=1;++packet.sequence;packet.tick=1016;
    packet.entries[0]=(SudekiMpLanArenaSpiritVfxSnapshot){.instance_sequence=1,.kind=44,
        .owner_actor_type=1,.emitted_host_tick=1000,.position={10,2,30},
        .rotation_xyzw={0,0,0,1},.scale={1,1,1}};
    current_peer=peer;current_peer.phase=SUDEKIMP_LAN_PARTY_OBSERVING;
    local_seat=1;runtime_thread=GetCurrentThreadId();effects_ready=saved_profile=TRUE;
    presentation_sample.valid=TRUE;presentation_sample.lease=peer.lease;
    presentation_sample.scene=remote_scene;presentation_sample.receipt=GetTickCount();
    history_count=1;history_lease=peer.lease;history_receipts[0]=GetTickCount();
    history[0]=(SudekiMpLanStoryFrame){.epoch=1,.revision=2,.available_mask=12};
    history[0].actors[3]=(SudekiMpLanStoryActor){.generation=2,.character=3,.weapon_item_plus_one=13};
    presented_generations[3]=2;now=GetTickCount();unsigned before=creates;
    follow_story_vfx(&w,&peer,now,&native);
    assert(creates==before+1 && !story_vfx.ready && last_prepared==0);
    history[0].actors[3].shots=(SudekiMpLanStoryShots){.latest=1,.count=1,
        .events={{.sequence=1,.host_tick=1000,.item=12,.origin={10,2,30},.direction={0,0,1}}}};
    follow_story_vfx(&w,&peer,now+=21,&native);
    assert(creates==before+2 && story_vfx.ready && last_prepared==1);
    assert(last_visuals.entries[0].position[1]==1 && last_visuals.entries[0].instance_sequence==1);
    follow_story_vfx(&w,&peer,now+=21,&native);assert(creates==before+2); /* Ready packet is not replayed. */
    puts("Story VFX and roster-camera controller dispatch: PASS (synthetic residency/transport; no rendering)");return 0;
}
