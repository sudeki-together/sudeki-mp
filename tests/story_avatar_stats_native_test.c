/* Synthetic spawn observations and memory pages. This proves read-only stats
 * admission and output fencing, not native actor construction or gameplay. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "../src/hooks/lan_story_ally_seat.c"
#pragma GCC diagnostic pop
#include <assert.h>
#include <stdio.h>

enum Mutation { STABLE, GENERATION_CHANGED, ACTOR_CHANGED, VALUES_CHANGED,
    OWNER_CHANGED, CLASS_CHANGED, COMPONENT_CHANGED, STATS_REVOKED, UNKNOWN_AFTER_COPY };
static SudekiMpLanStoryAvatarSpawnObservation observations[4];
static uint8_t *fixture_image,*fixture_actors[4],*fixture_stats[4];
static SudekiMpLanStoryNativeRoster fixture_roster;
static int fixture_world;
static unsigned observe_calls;
static enum Mutation mutation;
static BOOL observable=TRUE;
static void put(void *p,unsigned offset,void *value) { *(void **)((uint8_t *)p+offset)=value; }
static void protect(void *p,DWORD access) { DWORD old; assert(VirtualProtect(p,4096,access,&old)); }
static void mutate(unsigned p) {
    switch(mutation) {
    case GENERATION_CHANGED: ++observations[p].generation; break;
    case ACTOR_CHANGED: observations[p].actor=fixture_actors[(p+1)%4]; break;
    case VALUES_CHANGED: *(float *)(fixture_stats[p]+0x2c)+=1; break;
    case OWNER_CHANGED: put(fixture_stats[p],0x10,fixture_actors[(p+1)%4]); break;
    case CLASS_CHANGED: put(fixture_stats[p],0,fixture_image+0x2cc068); break;
    case COMPONENT_CHANGED: put(fixture_actors[p],0x4c,fixture_stats[(p+1)%4]); break;
    case STATS_REVOKED: protect(fixture_stats[p],PAGE_NOACCESS); break;
    case UNKNOWN_AFTER_COPY: observations[p].unknown=TRUE; break;
    default: break;
    }
}
BOOL SudekiMpLanStoryAvatarSpawnObserve(unsigned p,uint32_t epoch,uint32_t gen,
    SudekiMpLanStoryAvatarSpawnObservation *out) {
    ++observe_calls;
    if(!observable || p>=4) return FALSE;
    if(observe_calls==2) mutate(p);
    if(observations[p].epoch!=epoch || observations[p].generation!=gen) return FALSE;
    *out=observations[p]; return TRUE;
}
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *r) {
    ++observe_calls;
    if(!observable || r!=&fixture_roster) return FALSE;
    if(observe_calls==2) {
        mutate(0);
        if(mutation==GENERATION_CHANGED || mutation==ACTOR_CHANGED || mutation==UNKNOWN_AFTER_COPY)
            return FALSE;
    }
    return TRUE;
}
static void reset(void) {
    observe_calls=0; observable=TRUE; mutation=STABLE;
    fixture_roster=(SudekiMpLanStoryNativeRoster){.world=&fixture_world,.epoch=7,.available_mask=15};
    for(unsigned p=0;p<4;++p) {
        protect(fixture_actors[p],PAGE_READWRITE); protect(fixture_stats[p],PAGE_READWRITE);
        memset(fixture_actors[p],0,4096); memset(fixture_stats[p],0,4096);
        put(fixture_actors[p],0x4c,fixture_stats[p]);
        fixture_roster.actors[p]=fixture_actors[p];
        put(fixture_stats[p],0,fixture_image+0x2cc064); put(fixture_stats[p],0x10,fixture_actors[p]);
        float v[4]={25.f+p,100.f+p,5.5f+p,20.f+p}; memcpy(fixture_stats[p]+0x2c,v,sizeof(v));
        avatar_seats.seat[p].world=&fixture_world; avatar_seats.seat[p].epoch=7;
        avatar_seats.seat[p].generation=10+p;
        observations[p]=(SudekiMpLanStoryAvatarSpawnObservation){.epoch=7,.generation=10+p,
            .seat=p,.actor=fixture_actors[p],.world=&fixture_world,.ready=TRUE};
    }
}
static void rejected(unsigned p) {
    uint32_t gen=0xabcdef01u; float v[4]={-1,-2,-3,-4},before[4]; memcpy(before,v,sizeof(v));
    assert(!SudekiMpLanStoryAvatarSeatStats(p,&fixture_roster,&gen,v));
    assert(gen==0xabcdef01u && !memcmp(v,before,sizeof(v)));
}
static void valid_read_only(void) {
    reset();
    for(unsigned p=0;p<4;++p) {
        uint8_t actor_before[0x50],stats_before[0x3c];
        memcpy(actor_before,fixture_actors[p],sizeof(actor_before));
        memcpy(stats_before,fixture_stats[p],sizeof(stats_before));
        protect(fixture_actors[p],PAGE_READONLY); protect(fixture_stats[p],PAGE_READONLY);
        uint32_t gen=0; float v[4]={0}; observe_calls=0;
        assert(SudekiMpLanStoryAvatarSeatStats(p,&fixture_roster,&gen,v));
        assert(gen==10+p && observe_calls==2 && v[0]==25.f+p && v[1]==100.f+p &&
            v[2]==5.5f+p && v[3]==20.f+p);
        assert(!memcmp(actor_before,fixture_actors[p],sizeof(actor_before)));
        assert(!memcmp(stats_before,fixture_stats[p],sizeof(stats_before)));
    }
    reset(); *(float *)(fixture_stats[0]+0x2c)=0;
    *(float *)(fixture_stats[0]+0x34)=*(float *)(fixture_stats[0]+0x38)=0;
    uint32_t gen; float v[4];
    assert(SudekiMpLanStoryAvatarSeatStats(0,&fixture_roster,&gen,v) && !v[0] && !v[2] && !v[3]);
}
static void identity_and_inaccessible_memory(void) {
    reset(); put(fixture_stats[0],0,fixture_image+0x2cc068); rejected(0);
    reset(); put(fixture_stats[0],0x10,fixture_actors[1]); rejected(0);
    reset(); protect(fixture_stats[0],PAGE_NOACCESS); rejected(0);
    reset(); observations[0].unknown=TRUE;
    protect(fixture_actors[0],PAGE_NOACCESS); protect(fixture_stats[0],PAGE_NOACCESS);
    rejected(0); assert(observe_calls==1); /* Unknown identity is rejected before any actor/stat read. */
    reset(); protect(fixture_actors[0],PAGE_NOACCESS); rejected(0);
    reset(); observations[0].ready=FALSE; rejected(0);
    reset(); observable=FALSE; rejected(0);
    reset(); fixture_roster.epoch=8; rejected(0); assert(!observe_calls);
    reset(); fixture_roster.world=fixture_actors[0]; rejected(0); assert(!observe_calls);
    reset(); rejected(4); assert(!observe_calls);
    uint32_t gen=0; float v[4];
    assert(!SudekiMpLanStoryAvatarSeatStats(0,NULL,&gen,v));
    assert(!SudekiMpLanStoryAvatarSeatStats(0,&fixture_roster,NULL,v));
    assert(!SudekiMpLanStoryAvatarSeatStats(0,&fixture_roster,&gen,NULL));
}
static void copied_snapshot_cannot_outlive_identity(void) {
    for(enum Mutation m=GENERATION_CHANGED;m<=UNKNOWN_AFTER_COPY;++m) {
        reset(); mutation=m; rejected(0); assert(observe_calls==2);
    }
}
static void invalid_resources(void) {
    static const float invalid[]={NAN,INFINITY,-INFINITY,-1.0f};
    for(unsigned field=0;field<4;++field) for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);++i) {
        reset(); *(float *)(fixture_stats[0]+0x2c+4*field)=invalid[i]; rejected(0);
        assert(observe_calls==1);
    }
    reset(); *(float *)(fixture_stats[0]+0x30)=0; rejected(0);
    reset(); *(float *)(fixture_stats[0]+0x2c)=101; rejected(0);
    reset(); *(float *)(fixture_stats[0]+0x34)=21; rejected(0);
    reset(); *(float *)(fixture_stats[0]+0x38)=0; rejected(0);
}
static void native_hero_values(void) {
    for(unsigned c=0;c<4;++c) {
        reset(); float values[4]={0};
        protect(fixture_actors[c],PAGE_READONLY); protect(fixture_stats[c],PAGE_READONLY);
        assert(SudekiMpLanStoryPartySeatStats(c,&fixture_roster,values));
        assert(observe_calls==2 && values[0]==25.f+c && values[3]==20.f+c);
    }
    for(enum Mutation m=GENERATION_CHANGED;m<=UNKNOWN_AFTER_COPY;++m) {
        reset(); mutation=m;
        float values[4]={-1,-2,-3,-4},before[4]; memcpy(before,values,sizeof(before));
        assert(!SudekiMpLanStoryPartySeatStats(0,&fixture_roster,values));
        assert(observe_calls==2 && !memcmp(values,before,sizeof(values)));
    }
    reset(); observable=FALSE; protect(fixture_actors[0],PAGE_NOACCESS);
    float values[4]={-1,-2,-3,-4};
    assert(!SudekiMpLanStoryPartySeatStats(0,&fixture_roster,values) && observe_calls==1);
    reset(); fixture_roster.available_mask=14;
    assert(!SudekiMpLanStoryPartySeatStats(0,&fixture_roster,values) && !observe_calls);
    reset(); assert(!SudekiMpLanStoryPartySeatStats(4,&fixture_roster,values));
    assert(!SudekiMpLanStoryPartySeatStats(0,NULL,values));
    assert(!SudekiMpLanStoryPartySeatStats(0,&fixture_roster,NULL));
    assert(!observe_calls);
    reset(); *(float *)(fixture_stats[0]+0x34)=*(float *)(fixture_stats[0]+0x38)=0;
    assert(SudekiMpLanStoryPartySeatStats(0,&fixture_roster,values) && values[2]==0 && values[3]==0);
}
int main(void) {
    fixture_image=VirtualAlloc(NULL,0x500000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE); assert(fixture_image);
    for(unsigned p=0;p<4;++p) {
        fixture_actors[p]=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        fixture_stats[p]=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        assert(fixture_actors[p] && fixture_stats[p]);
    }
    const uint8_t choices[4]={5,5,5,5};
    assert(SudekiMpLanStoryAvatarSeatsConfigure((HMODULE)fixture_image,choices));
    valid_read_only(); identity_and_inaccessible_memory(); invalid_resources();
    copied_snapshot_cannot_outlive_identity();
    native_hero_values();
    for(unsigned p=0;p<4;++p) {
        VirtualFree(fixture_actors[p],0,MEM_RELEASE); VirtualFree(fixture_stats[p],0,MEM_RELEASE);
    }
    VirtualFree(fixture_image,0,MEM_RELEASE);
    puts("StoryAvatarStatsNativeTest PASS: read-only pages, native owner/class, stale generation, inaccessible memory, changing component/value, finite/max admission");
    return 0;
}
