/* Pure identity mapping with a fixture resolver; no native memory or callback
 * executes. The spawn exact-image test covers the resolver's required lease. */
#include "../src/hooks/lan_story_world.c"
#include <assert.h>
#include <stdio.h>

static unsigned resolver_calls,seat_for_actor[4]={3,0,2,1};
static uint32_t spawn_generations[4]={11,12,13,14};
static int fixture_actor[4],fixture_world;
static BOOL resolver_ready=TRUE;
static BOOL resolve(void *context,const SudekiMpLanStoryNativeRoster *roster,
    void *actor,unsigned *player,uint32_t *generation) {
    ++resolver_calls;
    assert(context==fixture_actor);
    if(!resolver_ready || !roster || roster->world!=&fixture_world || roster->epoch!=19) return FALSE;
    for(unsigned i=0;i<4;++i) if(actor==&fixture_actor[i]) {
        *player=seat_for_actor[i]; *generation=spawn_generations[i]; return TRUE;
    }
    return FALSE;
}
int main(void) {
    SudekiMpLanStoryNativeRoster roster={.world=&fixture_world,.epoch=19};
    Target targets[4];
    memset(targets,0,sizeof(targets));
    for(unsigned i=0;i<4;++i) {
        assert(SudekiMpLanStoryWorldAvatarPlayer(SudekiMpLanStoryWorldAvatarIdentifier(i))==i);
        targets[i].entity=(void *)&fixture_actor[i]; targets[i].kind=SUDEKIMP_LAN_STORY_WORLD_ALLY_KIND;
        targets[i].identifier=targets[i].native_identifier=0x123456;
        assert(avatar_identity(&targets[i],&roster,TRUE));
        assert(targets[i].identifier==0x123456 && !targets[i].dev_avatar);
    }
    assert(!SudekiMpLanStoryWorldAvatarIdentifier(4));
    assert(!SudekiMpLanStoryWorldAvatarIdentifier(UINT32_MAX));
    assert(SudekiMpLanStoryWorldAvatarPlayer(0xfffffeff)==4);
    assert(SudekiMpLanStoryWorldAvatarPlayer(0xffffff04)==4);
    assert(SudekiMpLanStoryWorldSetAvatarResolver(resolve,fixture_actor));
    for(unsigned i=0;i<4;++i) {
        assert(avatar_identity(&targets[i],&roster,TRUE));
        assert(targets[i].native_identifier==0x123456 && targets[i].dev_avatar);
        assert(targets[i].identifier==SudekiMpLanStoryWorldAvatarIdentifier(seat_for_actor[i]));
        assert(avatar_identity(&targets[i],&roster,FALSE));
        Target saved=targets[i]; ++spawn_generations[i];
        assert(!avatar_identity(&targets[i],&roster,FALSE)); --spawn_generations[i];
        assert(same_target(&targets[i],&saved));
    }
    qsort(targets,4,sizeof(*targets),target_order);
    for(unsigned i=0;i<4;++i) assert(targets[i].identifier==SudekiMpLanStoryWorldAvatarIdentifier(i));
    Target alias=targets[3]; seat_for_actor[0]=0;
    assert(avatar_identity(&alias,&roster,TRUE));
    assert(alias.entity!=targets[0].entity && target_order(&alias,&targets[0])==0);
    seat_for_actor[0]=3; /* Catalog duplicate test rejects this equal wire key. */
    resolver_ready=FALSE; assert(!avatar_identity(&targets[0],&roster,FALSE)); resolver_ready=TRUE;
    ++roster.epoch; assert(!avatar_identity(&targets[0],&roster,FALSE)); --roster.epoch;
    roster.world=NULL; assert(!avatar_identity(&targets[0],&roster,FALSE)); roster.world=&fixture_world;
    seat_for_actor[0]=4; assert(!avatar_identity(&targets[3],&roster,FALSE)); seat_for_actor[0]=3;
    spawn_generations[0]=0; assert(!avatar_identity(&targets[3],&roster,FALSE)); spawn_generations[0]=11;
    Target different=targets[0]; ++different.native_identifier; assert(!same_target(&different,&targets[0]));
    different=targets[0]; ++different.avatar_generation; assert(!same_target(&different,&targets[0]));
    active=TRUE; assert(!SudekiMpLanStoryWorldSetAvatarResolver(NULL,NULL)); active=FALSE;
    host_seen=TRUE; assert(!SudekiMpLanStoryWorldSetAvatarResolver(NULL,NULL)); host_seen=FALSE;
    client_seen=TRUE; assert(!SudekiMpLanStoryWorldSetAvatarResolver(NULL,NULL)); client_seen=FALSE;
    assert(SudekiMpLanStoryWorldSetAvatarResolver(NULL,NULL));
    assert(!avatar_identity(&targets[0],&roster,FALSE));
    assert(resolver_calls>=15);

    SudekiMpLanStoryWorldFrame frame={.epoch=19,.revision=1,.sequence=7,.host_tick=100,.count=4};
    for(unsigned i=0;i<4;++i) {
        SudekiMpLanStoryWorldActor *a=&frame.actors[i];
        a->kind=SUDEKIMP_LAN_STORY_WORLD_ALLY_KIND; a->identifier=SudekiMpLanStoryWorldAvatarIdentifier(i);
        a->generation=i+1; a->animation_sequence=1; a->bank_fingerprint=55; a->submodels=1;
        a->forward[2]=1; a->position[0]=(float)i;
    }
    assert(SudekiMpLanStoryWorldFrameValid(&frame));
    uint8_t bytes[SUDEKIMP_LAN_STORY_WORLD_CHUNK_MAX_SIZE]; size_t written=0;
    assert(SudekiMpLanStoryWorldChunkEncode(&frame,0,bytes,sizeof(bytes),&written));
    SudekiMpLanStoryWorldChunk chunk;
    assert(SudekiMpLanStoryWorldChunkDecode(bytes,written,&chunk) && chunk.count==4);
    for(unsigned i=0;i<4;++i) assert(chunk.actors[i].identifier==frame.actors[i].identifier &&
        chunk.actors[i].position[0]==frame.actors[i].position[0]);
    frame.actors[1].identifier=frame.actors[0].identifier;
    assert(!SudekiMpLanStoryWorldFrameValid(&frame)); /* Alias is not registry-order deduplication. */
    frame.actors[1].identifier=SudekiMpLanStoryWorldAvatarIdentifier(1);
    frame.actors[0].kind=SUDEKIMP_LAN_STORY_WORLD_PC_KIND;
    assert(!SudekiMpLanStoryWorldFrameValid(&frame)); /* Never a canonical hero ID. */
    frame.actors[0].kind=SUDEKIMP_LAN_STORY_WORLD_ALLY_KIND;
    frame.count=1; frame.actors[0].identifier=0x123456;
    assert(SudekiMpLanStoryWorldFrameValid(&frame)); /* Existing ally path unchanged. */
    puts("story avatar world: PASS (fixture resolver, distinct portable IDs, stale leases, v7 roundtrip; no native gameplay)");
    return 0;
}
