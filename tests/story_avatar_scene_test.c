/* Pure codec/policy isolation. No session negotiation, native party mutation,
 * avatar creation, pause ownership or gameplay is exercised. */
#include "network/lan_story_world_frame.h"
#include "network/lan_story_handoff.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define REGULAR SUDEKIMP_LAN_STORY_POLICY_REGULAR
#define DEV SUDEKIMP_LAN_STORY_POLICY_DEV_AVATARS

static SudekiMpLanStoryScene scene(void) {
    SudekiMpLanStoryScene s={.epoch=7,.revision=11,.observed_tick=100,
        .phase=SUDEKIMP_LAN_STORY_READY,.leader_seat=SUDEKIMP_LAN_STORY_NO_SEAT};
    strcpy(s.world,"newbrightwater"); return s;
}
static SudekiMpLanStoryFrame frame(void) {
    SudekiMpLanStoryFrame f={.epoch=7,.revision=11,.sequence=3,.host_tick=100,
        .leader_character=SUDEKIMP_LAN_STORY_NO_SEAT};
    return f;
}
static SudekiMpLanStoryFrame hero_frame(void) {
    SudekiMpLanStoryFrame f=frame(); f.available_mask=4; f.leader_character=2;
    f.actors[2]=(SudekiMpLanStoryActor){.generation=1,.character=2,.native_pose=1,
        .hp=100,.sp=50,.facing_z=1};
    return f;
}
static void scene_policy(void) {
    SudekiMpLanStoryScene s=scene();
    for(unsigned mask=0;mask<32;++mask) for(unsigned leader=0;leader<6;++leader) {
        s.available_mask=(uint8_t)mask; s.leader_seat=(uint8_t)leader;
        BOOL regular=mask<16 && leader<4 && (mask&(1u<<leader));
        BOOL dev=regular || (!mask && leader==4);
        assert(!!SudekiMpLanStorySceneValid(&s)==!!regular);
        assert(!!SudekiMpLanStorySceneValidForPolicy(&s,REGULAR)==!!regular);
        assert(!!SudekiMpLanStorySceneValidForPolicy(&s,DEV)==!!dev);
        assert(!SudekiMpLanStorySceneValidForPolicy(&s,(SudekiMpLanStoryPolicy)2));
    }
    s=scene(); s.inside_mask=1; assert(!SudekiMpLanStorySceneValidForPolicy(&s,DEV));
    s=scene(); strcpy(s.temporary,"lnbr_church"); assert(!SudekiMpLanStorySceneValidForPolicy(&s,DEV));
    s=scene(); s.world[0]=0; assert(!SudekiMpLanStorySceneValidForPolicy(&s,DEV));
    s=scene(); s.world[63]='x'; assert(!SudekiMpLanStorySceneValidForPolicy(&s,DEV));
    s=scene(); s.epoch=0; assert(!SudekiMpLanStorySceneValidForPolicy(&s,DEV));
    s=scene(); s.revision=0; assert(!SudekiMpLanStorySceneValidForPolicy(&s,DEV));
    for(unsigned phase=0;phase<SUDEKIMP_LAN_STORY_READY;++phase) {
        s=scene(); s.phase=(uint8_t)phase;
        assert(SudekiMpLanStorySceneValid(&s) && SudekiMpLanStorySceneValidForPolicy(&s,DEV));
    }

    uint8_t bytes[SUDEKIMP_LAN_STORY_WIRE_SIZE],regular_bytes[sizeof(bytes)];
    s=scene(); assert(!SudekiMpLanStorySceneEncode(&s,bytes,sizeof(bytes)));
    assert(SudekiMpLanStorySceneEncodeForPolicy(&s,bytes,sizeof(bytes),DEV));
    SudekiMpLanStoryScene cache=scene(),saved;
    cache.available_mask=4; cache.leader_seat=2; saved=cache;
    assert(!SudekiMpLanStorySceneDecode(bytes,sizeof(bytes),&cache));
    assert(!memcmp(&cache,&saved,sizeof(cache)));
    assert(!SudekiMpLanStorySceneDecodeForPolicy(bytes,sizeof(bytes),&cache,REGULAR));
    assert(!memcmp(&cache,&saved,sizeof(cache)));
    assert(SudekiMpLanStorySceneDecodeForPolicy(bytes,sizeof(bytes),&cache,DEV));
    assert(!memcmp(&cache,&s,sizeof(s)));
    cache=saved; bytes[14]=0;
    assert(!SudekiMpLanStorySceneDecodeForPolicy(bytes,sizeof(bytes),&cache,DEV));
    assert(!memcmp(&cache,&saved,sizeof(cache)));
    bytes[14]=4; bytes[143]=1;
    assert(!SudekiMpLanStorySceneDecodeForPolicy(bytes,sizeof(bytes),&cache,DEV));
    assert(!memcmp(&cache,&saved,sizeof(cache)));
    s=saved;
    assert(SudekiMpLanStorySceneEncode(&s,regular_bytes,sizeof(regular_bytes)));
    assert(SudekiMpLanStorySceneEncodeForPolicy(&s,bytes,sizeof(bytes),DEV));
    assert(!memcmp(bytes,regular_bytes,sizeof(bytes)));
    assert(SudekiMpLanStorySceneDecode(bytes,sizeof(bytes),&cache));
    assert(!memcmp(&cache,&s,sizeof(s)));
}
static void scene_freshness(void) {
    SudekiMpLanStoryScene a=scene(),b=a;
    assert(SudekiMpLanStorySceneAdvancesForPolicy(NULL,&a,DEV));
    assert(!SudekiMpLanStorySceneAdvances(NULL,&a));
    assert(!SudekiMpLanStorySceneAdvancesForPolicy(&a,&b,DEV));
    ++b.observed_tick;
    assert(SudekiMpLanStorySceneAdvancesForPolicy(&a,&b,DEV));
    assert(!SudekiMpLanStorySceneAdvancesForPolicy(&a,&b,REGULAR));
    ++b.revision; /* A new revision cannot describe unchanged contents. */
    assert(!SudekiMpLanStorySceneAdvancesForPolicy(&a,&b,DEV));
    strcpy(b.world,"illumina_countryside_hub");
    assert(!SudekiMpLanStorySceneAdvancesForPolicy(&a,&b,DEV));
    ++b.epoch; assert(SudekiMpLanStorySceneAdvancesForPolicy(&a,&b,DEV));
    b.observed_tick=a.observed_tick-1;
    assert(!SudekiMpLanStorySceneAdvancesForPolicy(&a,&b,DEV));
    b=a; ++b.revision; ++b.observed_tick; b.available_mask=4; b.leader_seat=2;
    assert(SudekiMpLanStorySceneAdvancesForPolicy(&a,&b,DEV));
    assert(!SudekiMpLanStorySceneAdvancesForPolicy(&a,&b,REGULAR));
    a=b; ++b.revision; ++b.observed_tick; b.available_mask=0; b.leader_seat=4;
    assert(SudekiMpLanStorySceneAdvancesForPolicy(&a,&b,DEV));
    strcpy(b.temporary,"lnbr_church");
    assert(!SudekiMpLanStorySceneAdvancesForPolicy(&a,&b,DEV));
}
static void frame_policy(void) {
    SudekiMpLanStoryScene s=scene(); SudekiMpLanStoryFrame f=frame();
    assert(SudekiMpLanStoryFrameValidForPolicy(&f,DEV));
    assert(!SudekiMpLanStoryFrameValid(&f));
    assert(!SudekiMpLanStoryFrameValidForPolicy(&f,REGULAR));
    assert(!SudekiMpLanStoryFrameValidForPolicy(&f,(SudekiMpLanStoryPolicy)2));
    assert(SudekiMpLanStoryFrameMatchesSceneForPolicy(&f,&s,DEV));
    assert(!SudekiMpLanStoryFrameMatchesScene(&f,&s));
    ++s.revision; assert(!SudekiMpLanStoryFrameMatchesSceneForPolicy(&f,&s,DEV));
    for(unsigned leader=0;leader<6;++leader) {
        f=frame(); f.leader_character=(uint8_t)leader;
        assert(!!SudekiMpLanStoryFrameValidForPolicy(&f,DEV)==(leader==4));
    }
    f=frame(); f.available_mask=16; assert(!SudekiMpLanStoryFrameValidForPolicy(&f,DEV));
    f=frame(); f.combat_mode=2; assert(!SudekiMpLanStoryFrameValidForPolicy(&f,DEV));
    for(unsigned c=0;c<4;++c) {
        f=frame(); f.actors[c].generation=1; assert(!SudekiMpLanStoryFrameValidForPolicy(&f,DEV));
        f=frame(); f.actors[c].locomotion.blend[2]=NAN; assert(!SudekiMpLanStoryFrameValidForPolicy(&f,DEV));
        f=frame(); f.actors[c].shots.events[3].sequence=1; assert(!SudekiMpLanStoryFrameValidForPolicy(&f,DEV));
    }
    f=frame(); f.view.matrix[0]=1; assert(!SudekiMpLanStoryFrameValidForPolicy(&f,DEV));
    uint8_t bytes[SUDEKIMP_LAN_STORY_FRAME_MAX_SIZE],regular_bytes[sizeof(bytes)]; size_t size=99,other=0;
    f=frame(); assert(!SudekiMpLanStoryFrameEncode(&f,bytes,sizeof(bytes),&size) && !size);
    assert(SudekiMpLanStoryFrameEncodeForPolicy(&f,bytes,sizeof(bytes),&size,DEV));
    assert(size==SUDEKIMP_LAN_STORY_FRAME_HEADER_SIZE && !bytes[20] && bytes[21]==4 && !bytes[22]);
    SudekiMpLanStoryFrame cache=hero_frame(),saved=cache;
    assert(!SudekiMpLanStoryFrameDecode(bytes,size,&cache));
    assert(!memcmp(&cache,&saved,sizeof(cache)));
    assert(!SudekiMpLanStoryFrameDecodeForPolicy(bytes,size,&cache,REGULAR));
    assert(!memcmp(&cache,&saved,sizeof(cache)));
    assert(SudekiMpLanStoryFrameDecodeForPolicy(bytes,size,&cache,DEV));
    assert(!memcmp(&cache,&f,sizeof(f)));
    const unsigned bad_offsets[]={20,21,22,23,25};
    const uint8_t bad_values[]={16,0,1,2,1};
    for(unsigned i=0;i<sizeof(bad_offsets)/sizeof(bad_offsets[0]);++i) {
        unsigned at=bad_offsets[i]; uint8_t prior=bytes[at]; bytes[at]=bad_values[i]; cache=saved;
        assert(!SudekiMpLanStoryFrameDecodeForPolicy(bytes,size,&cache,DEV));
        assert(!memcmp(&cache,&saved,sizeof(cache))); bytes[at]=prior;
    }
    cache=saved; assert(!SudekiMpLanStoryFrameDecodeForPolicy(bytes,size+1,&cache,DEV));
    assert(!memcmp(&cache,&saved,sizeof(cache)));
    f=hero_frame();
    assert(SudekiMpLanStoryFrameEncode(&f,regular_bytes,sizeof(regular_bytes),&other));
    assert(SudekiMpLanStoryFrameEncodeForPolicy(&f,bytes,sizeof(bytes),&size,DEV));
    assert(size==other && !memcmp(bytes,regular_bytes,size));
    assert(SudekiMpLanStoryFrameDecode(bytes,size,&cache));
    assert(!memcmp(&cache,&f,sizeof(f)));
}
static void paired_world_and_interpolation(void) {
    SudekiMpLanStoryScene s=scene(); SudekiMpLanStoryFrame f=frame();
    SudekiMpLanStoryWorldFrame w={.epoch=f.epoch,.revision=f.revision,.host_tick=f.host_tick,
        .sequence=f.sequence,.count=4};
    for(unsigned p=0;p<4;++p) w.actors[p]=(SudekiMpLanStoryWorldActor){
        .kind=SUDEKIMP_LAN_STORY_WORLD_ALLY_KIND,.submodels=1,
        .identifier=SudekiMpLanStoryWorldAvatarIdentifier(p),.generation=p+1,
        .animation_sequence=1,.bank_fingerprint=17,.forward={0,0,1}};
    assert(SudekiMpLanStoryWorldFrameMatchesForPolicy(&w,&f,DEV));
    assert(SudekiMpLanStoryWorldFrameMatchesSceneForPolicy(&w,&s,DEV));
    assert(!SudekiMpLanStoryWorldFrameMatches(&w,&f));
    assert(!SudekiMpLanStoryWorldFrameMatchesScene(&w,&s));
    assert(!SudekiMpLanStoryWorldFrameMatchesForPolicy(&w,&f,(SudekiMpLanStoryPolicy)2));
    ++w.sequence; assert(!SudekiMpLanStoryWorldFrameMatchesForPolicy(&w,&f,DEV)); --w.sequence;
    ++w.epoch; assert(!SudekiMpLanStoryWorldFrameMatchesSceneForPolicy(&w,&s,DEV)); --w.epoch;
    w.count=1; w.actors[0].kind=SUDEKIMP_LAN_STORY_WORLD_PC_KIND;
    w.actors[0].identifier=SudekiMpLanStoryWorldCharacterIdentifier(2);
    assert(SudekiMpLanStoryWorldFrameValid(&w));
    assert(!SudekiMpLanStoryWorldFrameMatchesForPolicy(&w,&f,DEV)); /* No hidden canonical hero. */
    w.count=0; assert(SudekiMpLanStoryWorldFrameMatchesForPolicy(&w,&f,DEV));

    SudekiMpLanStoryFrame next=f,out=hero_frame(),saved=out;
    ++next.sequence; next.host_tick+=100;
    assert(!SudekiMpLanStoryFrameInterpolate(&f,&next,150,&out));
    assert(!memcmp(&out,&saved,sizeof(out)));
    assert(SudekiMpLanStoryFrameInterpolateForPolicy(&f,&next,150,&out,DEV));
    assert(out.host_tick==150 && out.sequence==f.sequence && !out.available_mask && out.leader_character==4);
    assert(SudekiMpLanStoryFrameInterpolateForPolicy(&f,&next,200,&out,DEV));
    assert(!memcmp(&out,&next,sizeof(out)));
    out=saved; ++next.revision;
    assert(!SudekiMpLanStoryFrameInterpolateForPolicy(&f,&next,150,&out,DEV));
    assert(!memcmp(&out,&saved,sizeof(out)));
    next=hero_frame(); ++next.sequence; next.host_tick+=100;
    assert(!SudekiMpLanStoryFrameInterpolateForPolicy(&f,&next,150,&out,DEV));
    assert(!memcmp(&out,&saved,sizeof(out)));
}
static void control_policy(void) {
    SudekiMpLanStoryScene s=scene();
    SudekiMpLanStoryControlFence f={.epoch=s.epoch,.revision=s.revision,
        .transaction=1,.actor_generation=9,.player=1,.character=SUDEKIMP_STORY_CHARACTER_ALLY};
    assert(SudekiMpLanStoryControlMatchesSceneForPolicy(&f,&s,DEV));
    assert(!SudekiMpLanStoryControlMatchesScene(&f,&s));
    assert(!SudekiMpLanStoryControlMatchesSceneForPolicy(&f,&s,REGULAR));
    assert(!SudekiMpLanStoryControlMatchesSceneForPolicy(&f,&s,(SudekiMpLanStoryPolicy)2));
    for(unsigned c=0;c<4;++c) {
        f.character=(uint8_t)c; assert(!SudekiMpLanStoryControlMatchesSceneForPolicy(&f,&s,DEV));
    }
    f.character=SUDEKIMP_STORY_CHARACTER_ALLY;
    f.player=0; assert(!SudekiMpLanStoryControlMatchesSceneForPolicy(&f,&s,DEV));
    f.player=1; ++f.epoch; assert(!SudekiMpLanStoryControlMatchesSceneForPolicy(&f,&s,DEV));
    f.epoch=s.epoch; ++f.revision; assert(!SudekiMpLanStoryControlMatchesSceneForPolicy(&f,&s,DEV));
    f.revision=s.revision-1; assert(SudekiMpLanStoryControlMatchesSceneForPolicy(&f,&s,DEV));
    f.actor_generation=0; assert(!SudekiMpLanStoryControlMatchesSceneForPolicy(&f,&s,DEV));
    f.actor_generation=9; s.available_mask=12; s.leader_seat=2; f.character=3;
    assert(SudekiMpLanStoryControlMatchesScene(&f,&s));
    assert(SudekiMpLanStoryControlMatchesSceneForPolicy(&f,&s,DEV));
    f.character=2;
    assert(!SudekiMpLanStoryControlMatchesScene(&f,&s));
    assert(!SudekiMpLanStoryControlMatchesSceneForPolicy(&f,&s,DEV));
}
int main(void) {
    scene_policy(); scene_freshness(); frame_policy(); paired_world_and_interpolation(); control_policy();
    puts("story avatar scene/frame policy: PASS (pure codecs, regular isolation, zero-hero exterior and atomic rejection; no native gameplay)");
    return 0;
}
