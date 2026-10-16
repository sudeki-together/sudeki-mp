/* Pure policy/codec test for per-character story scene areas. No native
 * code, network session or gameplay is exercised. */
#include "network/lan_party_story.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static SudekiMpLanStoryScene ready(uint32_t epoch,uint32_t revision,uint32_t tick,
    const char *temporary,uint8_t inside) {
    SudekiMpLanStoryScene s; memset(&s,0,sizeof(s));
    s.epoch=epoch; s.revision=revision; s.observed_tick=tick;
    s.phase=SUDEKIMP_LAN_STORY_READY; s.available_mask=12u; s.leader_seat=2u; /* Tal 2, Ailish 3 */
    strcpy(s.world,"newbrightwater");
    if(temporary) strcpy(s.temporary,temporary);
    s.inside_mask=inside;
    return s;
}
static void validity(void) {
    SudekiMpLanStoryScene s=ready(1,1,10,NULL,0); assert(SudekiMpLanStorySceneValid(&s));
    s=ready(1,1,10,"lnbr_church",8u); assert(SudekiMpLanStorySceneValid(&s));
    s=ready(1,1,10,"lnbr_church",12u); assert(SudekiMpLanStorySceneValid(&s)); /* vanilla whole party */
    s=ready(1,1,10,"lnbr_church",0u); assert(!SudekiMpLanStorySceneValid(&s)); /* nobody inside */
    s=ready(1,1,10,NULL,8u); assert(!SudekiMpLanStorySceneValid(&s));           /* inside nowhere */
    s=ready(1,1,10,"lnbr_church",1u); assert(!SudekiMpLanStorySceneValid(&s));  /* unavailable */
    s=ready(1,1,10,"lnbr_church",0x18u); assert(!SudekiMpLanStorySceneValid(&s));
    s=ready(1,1,10,NULL,0); s.phase=SUDEKIMP_LAN_STORY_LOADING; s.available_mask=0;
    s.leader_seat=SUDEKIMP_LAN_STORY_NO_SEAT; assert(SudekiMpLanStorySceneValid(&s));
    s.inside_mask=4u; assert(!SudekiMpLanStorySceneValid(&s));
}
static void codec(void) {
    uint8_t wire[SUDEKIMP_LAN_STORY_WIRE_SIZE]; SudekiMpLanStoryScene in=ready(3,7,99,"lnbr_church",8u),out;
    assert(SUDEKIMP_LAN_STORY_WIRE_SIZE==144u);
    assert(SudekiMpLanStorySceneEncode(&in,wire,sizeof(wire)) && wire[143]==8u);
    assert(SudekiMpLanStorySceneDecode(wire,sizeof(wire),&out) && !memcmp(&in,&out,sizeof(in)));
    assert(!SudekiMpLanStorySceneDecode(wire,143u,&out));
    wire[143]=0u; assert(!SudekiMpLanStorySceneDecode(wire,sizeof(wire),&out)); /* temporary without occupant */
    wire[143]=0x10u; assert(!SudekiMpLanStorySceneDecode(wire,sizeof(wire),&out));
    SudekiMpLanStoryScene a=ready(3,7,99,"lnbr_church",8u),b=a; b.inside_mask=12u;
    assert(!SudekiMpLanStorySceneSame(&a,&b));
}
static void advances(void) {
    SudekiMpLanStoryScene outside=ready(5,1,100,NULL,0);
    SudekiMpLanStoryScene ailish_in=ready(5,2,200,"lnbr_church",8u);
    SudekiMpLanStoryScene tal_in=ready(5,2,200,"lnbr_church",4u);
    SudekiMpLanStoryScene both_in=ready(5,3,300,"lnbr_church",12u);
    SudekiMpLanStoryScene ailish_out=ready(5,3,300,NULL,0);
    /* One player enters/exits while the other stays outside: same epoch. */
    assert(SudekiMpLanStorySceneAdvances(&outside,&ailish_in));
    assert(SudekiMpLanStorySceneAdvances(&outside,&tal_in));
    assert(SudekiMpLanStorySceneAdvances(&ailish_in,&ailish_out));
    /* Second player joins the occupied room: temporary unchanged, mask grows. */
    assert(SudekiMpLanStorySceneAdvances(&ailish_in,&both_in));
    /* Exterior emptied from a split, or whole-party travel: new epoch required. */
    SudekiMpLanStoryScene whole=ready(5,2,200,"lnbr_church",12u);
    assert(!SudekiMpLanStorySceneAdvances(&outside,&whole));
    whole.epoch=6; assert(SudekiMpLanStorySceneAdvances(&outside,&whole));
    SudekiMpLanStoryScene from_whole=ready(6,3,300,NULL,0);
    assert(!SudekiMpLanStorySceneAdvances(&whole,&from_whole));
    from_whole.epoch=7; assert(SudekiMpLanStorySceneAdvances(&whole,&from_whole));
    /* A different exterior is never an in-epoch change. */
    SudekiMpLanStoryScene moved=ready(5,2,200,NULL,0); strcpy(moved.world,"illumina_countryside_hub");
    assert(!SudekiMpLanStorySceneAdvances(&outside,&moved));
    /* Same revision repeat still needs identical content and newer tick. */
    SudekiMpLanStoryScene repeat=ailish_in; repeat.observed_tick=201;
    assert(SudekiMpLanStorySceneAdvances(&ailish_in,&repeat));
    repeat.inside_mask=12u; assert(!SudekiMpLanStorySceneAdvances(&ailish_in,&repeat));
}
static void areas(void) {
    SudekiMpLanStoryScene s=ready(5,2,200,"lnbr_church",8u);
    char w[SUDEKIMP_LAN_STORY_NAME_SIZE],t[SUDEKIMP_LAN_STORY_NAME_SIZE];
    assert(SudekiMpLanStorySceneCharacterArea(&s,3u,w,t) && !strcmp(w,"newbrightwater") && !strcmp(t,"lnbr_church"));
    assert(SudekiMpLanStorySceneCharacterArea(&s,2u,w,t) && !strcmp(w,"newbrightwater") && !t[0]);
    assert(!SudekiMpLanStorySceneCharacterArea(&s,0u,w,t) && !SudekiMpLanStorySceneCharacterArea(&s,4u,w,t));
    assert(SudekiMpLanStorySceneCharacterAreaMatches(&s,2u,"newbrightwater",""));
    assert(!SudekiMpLanStorySceneCharacterAreaMatches(&s,2u,"newbrightwater","lnbr_church"));
    assert(SudekiMpLanStorySceneCharacterAreaMatches(&s,3u,"newbrightwater","lnbr_church"));
    assert(!SudekiMpLanStorySceneSameArea(&s,2u,3u));
    s=ready(5,3,300,"lnbr_church",12u); assert(SudekiMpLanStorySceneSameArea(&s,2u,3u));
    s=ready(5,3,300,NULL,0); assert(SudekiMpLanStorySceneSameArea(&s,2u,3u));
    s.phase=SUDEKIMP_LAN_STORY_LOADING; s.available_mask=0; s.leader_seat=SUDEKIMP_LAN_STORY_NO_SEAT;
    assert(!SudekiMpLanStorySceneCharacterArea(&s,2u,w,t));
}
static void kamo_split_scenes(void) {
    static const struct {const char *world,*temporary;} shops[]={
        {"newbrightwater","lnbr_kamo_shop"},
        {"illuminacastle","lica_kamo_shop"},
        {"shadani_mo","lsmob04_kamo_shop"},
        {"transentia_city","ltci_kamo_shop"},
        {"crystal_reef","dcre_kamo_shop"},
        {"cyantine_citadel","dcci_kamo_shop"}
    };
    for(unsigned i=0;i<sizeof(shops)/sizeof(*shops);++i) {
        SudekiMpLanStoryScene before=ready(19,1,100,NULL,0);
        before.leader_seat=3u; /* Ailish enters; Tal retains the exterior. */
        memset(before.world,0,sizeof(before.world));
        strcpy(before.world,shops[i].world);
        SudekiMpLanStoryScene inside=before,after,decoded;
        inside.revision=2;inside.observed_tick=200;inside.inside_mask=8u;
        strcpy(inside.temporary,shops[i].temporary);
        after=before;after.revision=3;after.observed_tick=300;
        uint8_t wire[SUDEKIMP_LAN_STORY_WIRE_SIZE];
        const SudekiMpLanStoryScene *scenes[]={&before,&inside,&after};
        for(unsigned j=0;j<3u;++j) {
            assert(SudekiMpLanStorySceneValid(scenes[j]));
            assert(SudekiMpLanStorySceneEncode(scenes[j],wire,sizeof(wire)));
            assert(SudekiMpLanStorySceneDecode(wire,sizeof(wire),&decoded));
            assert(!memcmp(scenes[j],&decoded,sizeof(decoded)));
            assert(decoded.epoch==before.epoch);
            assert(SudekiMpLanStorySceneCharacterAreaMatches(&decoded,2u,shops[i].world,""));
            assert(SudekiMpLanStorySceneCharacterAreaMatches(&decoded,3u,shops[i].world,
                j==1u?shops[i].temporary:""));
            assert(!!SudekiMpLanStorySceneSameArea(&decoded,2u,3u)==(j!=1u));
        }
        assert(SudekiMpLanStorySceneAdvances(&before,&inside));
        assert(SudekiMpLanStorySceneAdvances(&inside,&after));
        printf("PASS named KamoSplitScene destination=%s epoch_retained=1 Tal_outside=1\n",shops[i].temporary);
    }
}
int main(void) {
    validity(); codec(); advances(); areas(); kamo_split_scenes();
    puts("StorySceneAreaTest: PASS (scene v2 per-character area policy/codec; no native, network or gameplay)");
    return 0;
}
