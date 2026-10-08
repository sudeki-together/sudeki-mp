/* Policy/association fixture only. No native hooks, AI calls, actor construction
 * or gameplay runs. Spawn observations below are synthetic identity records. */
#include "../src/hooks/control_separation.c"
#include <assert.h>
#include <stdio.h>

static SudekiMpLanStoryAvatarSpawnObservation observed[4];
static BOOL observable=TRUE;
static unsigned calls;
BOOL SudekiMpLanStoryAvatarPartyObserve(SudekiMpLanStoryAvatarPartyObservation *out) {
    (void)out; return FALSE;
}
BOOL SudekiMpLanStoryInputHostFenceExact(void *controller,void *actor) {
    (void)controller; (void)actor; return FALSE;
}
BOOL SudekiMpLanStoryAvatarSpawnObserve(unsigned player,uint32_t epoch,uint32_t generation,
    SudekiMpLanStoryAvatarSpawnObservation *out) {
    ++calls;
    if(!observable || player>=4u || observed[player].epoch!=epoch ||
        observed[player].generation!=generation) return FALSE;
    *out=observed[player]; return TRUE;
}

int main(void) {
    /* Naked wrappers name these static slots in assembly. Keep their symbols
     * visible to the compiler's whole-program fixture elimination. */
    __asm__ volatile("" : : "g"(&position_set_forward),"g"(&lan_arena_first_person_held_fire));
    int world,actors[4],replacement;
    SudekiMpLanPartyLease connections[4],keys[4],out;
    story_native_control.bound=TRUE;
    story_native_control.thread=GetCurrentThreadId();
    story_native_control.world=&world; story_native_control.epoch=9;
    for(unsigned p=0;p<4u;++p) {
        connections[p]=(SudekiMpLanPartyLease){.seat=(uint8_t)p,.token=100u+p,.generation=40u+p};
        observed[p]=(SudekiMpLanStoryAvatarSpawnObservation){.seat=p,.epoch=9,.generation=1000u+p,
            .actor=&actors[p],.world=&world,.ready=TRUE};
    }
    /* Regular heroes and the legacy singleton never admit connection zero. */
    for(unsigned c=0;c<8u;++c) assert(!story_next_lease(&connections[0],c,&out));
    assert(story_next_lease(&connections[1],2,&out) && out.seat==2 && out.generation==1);
    SudekiMpLanPartyControlStoryAllyEntity(&replacement,"ALLY_TALOS");
    assert(story_next_lease(&connections[1],4,&out));
    assert(!story_next_lease(&connections[1],5,&out));
    assert(!story_bind_avatar(0,&actors[0],1000));
    assert(SudekiMpLanPartyControlStoryAllyActor(&out)==&replacement);
    SudekiMpLanPartyControlStoryAllyEntity(NULL,NULL);
    story_native_control.last_generation[4]=1;
    assert(!story_bind_avatar(0,&actors[0],1000)); /* no key4 mode reinterpretation */
    story_native_control.last_generation[4]=0;
    for(unsigned p=0;p<4u;++p) {
        assert(story_bind_avatar(p,&actors[p],1000u+p));
        assert(story_next_lease(&connections[p],4u+p,&keys[p]));
        assert(keys[p].seat==4u+p && keys[p].generation==1 && keys[p].token==connections[p].token);
        for(unsigned other=0;other<4u;++other)
            if(other!=p) assert(!story_next_lease(&connections[other],4u+p,&out));
    }
    assert(story_native_control.avatar_route);
    /* A candidate key cannot migrate to a replacement spawn before Acquire.
     * A repeated request for the unchanged connection remains idempotent. */
    assert(story_next_lease(&connections[3],7,&out) && party_key_equal(&out,&keys[3]));
    SudekiMpLanPartyLease stale=keys[3];
    assert(story_bind_avatar(3,NULL,0));
    assert(!story_avatar_key_exact(&stale));
    observed[3].generation=2003;
    assert(story_bind_avatar(3,&actors[3],2003));
    assert(!story_avatar_key_exact(&stale));
    assert(story_next_lease(&connections[3],7,&keys[3]) && keys[3].generation==2);
    assert(!story_avatar_key_exact(&stale) && story_avatar_key_exact(&keys[3]));
    SudekiMpLanPartyControlStoryAllyEntity(&replacement,"ALLY_TALOS");
    assert(!story_native_control.ally_entity); /* legacy setter cannot replace host avatar */
    for(unsigned c=0;c<4u;++c) assert(!story_next_lease(&connections[0],c,&out));
    observed[1].actor=&actors[0];
    assert(!story_bind_avatar(1,&actors[0],1001)); /* no shared actor across players */
    observed[1].actor=&actors[1];
    assert(!story_bind_avatar(4,&replacement,1004));
    observed[2].unknown=TRUE; assert(!story_next_lease(&connections[2],6,&out));
    observed[2].unknown=FALSE; observed[2].ready=FALSE;
    assert(!story_next_lease(&connections[2],6,&out)); observed[2].ready=TRUE;
    observed[2].world=&replacement; assert(!story_next_lease(&connections[2],6,&out)); observed[2].world=&world;
    observed[2].seat=1; assert(!story_next_lease(&connections[2],6,&out)); observed[2].seat=2;
    observed[2].epoch=10; assert(!story_next_lease(&connections[2],6,&out)); observed[2].epoch=9;
    observed[2].generation++; assert(!story_next_lease(&connections[2],6,&out)); observed[2].generation--;
    for(unsigned p=0;p<4u;++p) {
        PartyNativeLease *owned=&story_native_control.actor[4u+p];
        *owned=(PartyNativeLease){.key=keys[p],.phase=PARTY_NATIVE_HELD,.actor=&actors[p],
            .avatar_generation=observed[p].generation};
        story_native_control.last_generation[4u+p]=keys[p].generation;
        InterlockedOr(&story_native_control.retained,(LONG)(1u<<(4u+p)));
        assert(SudekiMpLanPartyControlStoryAllyActor(&keys[p])==&actors[p]);
        assert(!story_next_lease(&connections[p],4u+p,&out));
        assert(!story_bind_avatar(p,NULL,0));
        assert(!story_bind_avatar(p,&replacement,1000u+p));
        assert(story_bind_avatar(p,&actors[p],observed[p].generation));
        SudekiMpLanPartyLease wrong=keys[p]; ++wrong.generation;
        assert(!SudekiMpLanPartyControlStoryAllyActor(&wrong));
        assert(SudekiMpLanPartyControlStoryRetainsKey(&keys[p]));
    }
    observable=FALSE;
    assert(!SudekiMpLanPartyControlStoryAllyActor(&keys[0]));
    assert(SudekiMpLanPartyControlStoryRetainsKey(&keys[0]));
    assert(!SudekiMpLanPartyControlStoryEnd()); observable=TRUE;
    ++observed[0].generation;
    assert(!SudekiMpLanPartyControlStoryAllyActor(&keys[0]));
    assert(!story_bind_avatar(0,&actors[0],observed[0].generation));
    --observed[0].generation;
    /* Even address reuse and a fresh observation cannot substitute an owned
     * actor generation. The AI lease retains its original spawn identity. */
    ++story_native_control.avatar[0].spawn_generation; ++observed[0].generation;
    assert(!SudekiMpLanPartyControlStoryAllyActor(&keys[0]));
    --story_native_control.avatar[0].spawn_generation; --observed[0].generation;
    for(unsigned p=0;p<4u;++p) {
        story_control_forget(4u+p); /* simulate positively completed native release */
        assert(story_next_lease(&connections[p],4u+p,&out) && out.generation==keys[p].generation+1u);
        assert(!SudekiMpLanPartyControlStoryAllyActor(&keys[p]));
        assert(story_bind_avatar(p,NULL,0));
        assert(!story_next_lease(&connections[p],4u+p,&out));
    }
    assert(calls && SudekiMpLanPartyControlStoryEnd());
    assert(!story_native_control.avatar_route && !SudekiMpLanPartyControlStoryRetains());
    puts("story avatar control mapping, generation, legacy isolation and retained association tests passed");
    return 0;
}
