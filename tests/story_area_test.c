#include "engine/story_area.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static SudekiMpStoryAreaOwner owner(unsigned player,unsigned character) {
    SudekiMpStoryAreaOwner o={.session=101,.actor_generation=1000+character,
        .connection_generation=10+player,.player=player,.character=character};
    return o;
}
static SudekiMpStoryAreas initial(SudekiMpStoryAreaRef *exterior) {
    SudekiMpStoryAreas s;
    memset(&s,0,sizeof(s));
    assert(SudekiMpStoryAreasInitialize(&s,101));
    assert(SudekiMpStoryAreaLoad(&s,"newbrightwater","",exterior));
    assert(!SudekiMpStoryAreaBind(&s,owner(0,3),*exterior));
    assert(SudekiMpStoryAreaReady(&s,*exterior));
    return s;
}
static SudekiMpStoryAreaRef interior(SudekiMpStoryAreas *s) {
    SudekiMpStoryAreaRef r;
    /* A policy fixture name, not a discovered/authored native doorway. */
    assert(SudekiMpStoryAreaLoad(s,"newbrightwater","fixture_interior",&r));
    return r;
}
static void unchanged(const SudekiMpStoryAreas *s,const SudekiMpStoryAreas *before) {
    assert(!memcmp(s,before,sizeof(*s)));
}
static void finish(SudekiMpStoryAreas *s,SudekiMpStoryAreaOwner o,
    uint64_t ticket,uint64_t task,SudekiMpStoryAreaRef destination) {
    assert(SudekiMpStoryAreaNativeReturned(s,o,ticket,task,destination));
    assert(!SudekiMpStoryAreaInputAllowed(s,o)); /* old presentation not released */
    assert(SudekiMpStoryAreaTravelReleased(s,o,ticket));
    assert(SudekiMpStoryAreaInputAllowed(s,o));
}
static void independent_entry_join_and_exit(void) {
    SudekiMpStoryAreaRef outside;
    SudekiMpStoryAreas s=initial(&outside);
    SudekiMpStoryAreaOwner ailish=owner(0,3),tal=owner(1,2);
    assert(SudekiMpStoryAreaBind(&s,ailish,outside));
    assert(SudekiMpStoryAreaBind(&s,tal,outside));
    SudekiMpStoryAreaPlayer tal_before=s.players[tal.player];
    SudekiMpStoryAreaRef inside=interior(&s),same;
    uint64_t t,repeat,loader;
    assert(SudekiMpStoryAreaRetain(&s,inside,&loader));
    assert(SudekiMpStoryAreaRequest(&s,ailish,inside,&t));
    assert(SudekiMpStoryAreaRequest(&s,ailish,inside,&repeat) && t==repeat);
    assert(SudekiMpStoryAreaInputAllowed(&s,ailish)); /* request is not native start */
    assert(SudekiMpStoryAreaStarted(&s,ailish,t,700));
    assert(!SudekiMpStoryAreaInputAllowed(&s,ailish));
    assert(SudekiMpStoryAreaInputAllowed(&s,tal));
    assert(!SudekiMpStoryAreaNativeReturned(&s,ailish,t,700,inside)); /* not ready */
    assert(!SudekiMpStoryAreaTravelReleased(&s,ailish,t));
    assert(!SudekiMpStoryAreaCancelRequest(&s,ailish,t)); /* cannot cancel live work */
    assert(!SudekiMpStoryAreaRetireBegin(&s,outside));
    assert(!SudekiMpStoryAreaRetireBegin(&s,inside));
    assert(SudekiMpStoryAreaReady(&s,inside));
    assert(SudekiMpStoryAreaRelease(&s,inside,loader));
    assert(!SudekiMpStoryAreaNativeReturned(&s,ailish,t,701,inside));
    finish(&s,ailish,t,700,inside);
    assert(!memcmp(&tal_before,&s.players[tal.player],sizeof(tal_before)));
    assert(SudekiMpStoryAreaRefSame(s.players[tal.player].area,outside));
    assert(SudekiMpStoryAreaLoad(&s,"newbrightwater","fixture_interior",&same));
    assert(SudekiMpStoryAreaRefSame(inside,same)); /* one shared interior lifetime */
    assert(SudekiMpStoryAreaRequest(&s,tal,inside,&t));
    assert(SudekiMpStoryAreaStarted(&s,tal,t,701));
    assert(SudekiMpStoryAreaInputAllowed(&s,ailish));
    finish(&s,tal,t,701,inside);
    assert(SudekiMpStoryAreaRequest(&s,ailish,outside,&t));
    assert(SudekiMpStoryAreaStarted(&s,ailish,t,702));
    finish(&s,ailish,t,702,outside);
    assert(SudekiMpStoryAreaRefSame(s.players[tal.player].area,inside));
    assert(SudekiMpStoryAreaInputAllowed(&s,tal));
    assert(SudekiMpStoryAreaRefSame(s.areas[0].ref,outside)); /* no global epoch bump */
    assert(!SudekiMpStoryAreaRetireBegin(&s,inside)); /* Tal is still there */
}
static void concurrent_requests_and_opposite_travel(void) {
    SudekiMpStoryAreaRef outside;
    SudekiMpStoryAreas s=initial(&outside);
    SudekiMpStoryAreaRef inside=interior(&s);
    assert(SudekiMpStoryAreaReady(&s,inside));
    const unsigned characters[4]={3,2,0,1};
    uint64_t tickets[4];
    for(unsigned p=0;p<4;++p) assert(SudekiMpStoryAreaBind(&s,owner(p,characters[p]),outside));
    for(unsigned pass=0;pass<100;++pass) {
        for(unsigned p=0;p<4;++p) {
            SudekiMpStoryAreaOwner o=owner(p,characters[p]);
            SudekiMpStoryAreaRef target=s.players[p].area.slot?outside:inside;
            assert(SudekiMpStoryAreaRequest(&s,o,target,&tickets[p]));
            assert(SudekiMpStoryAreaStarted(&s,o,tickets[p],500+p));
        }
        /* Opposite completion order cannot impose a global party barrier. */
        for(unsigned i=4;i>0;--i) {
            unsigned p=i-1;
            SudekiMpStoryAreaOwner o=owner(p,characters[p]);
            SudekiMpStoryAreaRef target=s.players[p].travel.destination;
            finish(&s,o,tickets[p],500+p,target);
            assert(!SudekiMpStoryAreaNativeReturned(&s,o,tickets[p],500+p,target));
        }
        assert(SudekiMpStoryAreaRefSame(s.areas[0].ref,outside));
        assert(SudekiMpStoryAreaRefSame(s.areas[1].ref,inside));
    }
    /* Have one player leave while another enters the already occupied room. */
    assert(SudekiMpStoryAreaRequest(&s,owner(0,3),inside,&tickets[0]));
    assert(SudekiMpStoryAreaStarted(&s,owner(0,3),tickets[0],800));
    finish(&s,owner(0,3),tickets[0],800,inside);
    assert(SudekiMpStoryAreaRequest(&s,owner(0,3),outside,&tickets[0]));
    assert(SudekiMpStoryAreaRequest(&s,owner(1,2),inside,&tickets[1]));
    assert(SudekiMpStoryAreaStarted(&s,owner(0,3),tickets[0],801));
    assert(SudekiMpStoryAreaStarted(&s,owner(1,2),tickets[1],802));
    finish(&s,owner(1,2),tickets[1],802,inside);
    finish(&s,owner(0,3),tickets[0],801,outside);
}
static void disconnect_retains_native_and_view_lifetimes(void) {
    SudekiMpStoryAreaRef outside;
    SudekiMpStoryAreas s=initial(&outside);
    SudekiMpStoryAreaRef inside=interior(&s);
    SudekiMpStoryAreaOwner o=owner(0,3),stale=o;
    uint64_t ticket;
    assert(SudekiMpStoryAreaReady(&s,inside));
    assert(SudekiMpStoryAreaBind(&s,o,outside));
    assert(SudekiMpStoryAreaRequest(&s,o,inside,&ticket));
    assert(SudekiMpStoryAreaStarted(&s,o,ticket,100));
    assert(SudekiMpStoryAreaRevoke(&s,o));
    assert(!SudekiMpStoryAreaInputAllowed(&s,o));
    assert(!SudekiMpStoryAreaUnbind(&s,o));
    assert(!SudekiMpStoryAreaRetireBegin(&s,inside));
    assert(!SudekiMpStoryAreaTravelReleased(&s,o,ticket));
    /* Disconnect is not native cancellation; exact work may finish inside. */
    assert(SudekiMpStoryAreaNativeReturned(&s,o,ticket,100,inside));
    assert(!SudekiMpStoryAreaUnbind(&s,o));
    assert(SudekiMpStoryAreaTravelReleased(&s,o,ticket));
    assert(!SudekiMpStoryAreaInputAllowed(&s,o));
    assert(!SudekiMpStoryAreaRetireBegin(&s,inside)); /* actor cleanup still owed */
    assert(SudekiMpStoryAreaUnbind(&s,o));
    assert(!SudekiMpStoryAreaBind(&s,stale,outside));
    ++o.connection_generation; ++o.actor_generation;
    assert(SudekiMpStoryAreaBind(&s,o,outside));
    SudekiMpStoryAreas before=s;
    assert(!SudekiMpStoryAreaRevoke(&s,stale));
    assert(!SudekiMpStoryAreaNativeReturned(&s,stale,ticket,100,inside));
    unchanged(&s,&before);
    assert(SudekiMpStoryAreaInputAllowed(&s,o));
    /* An unstarted request can be dropped without inventing cancellation. */
    assert(SudekiMpStoryAreaRequest(&s,o,inside,&ticket));
    assert(SudekiMpStoryAreaRevoke(&s,o));
    assert(SudekiMpStoryAreaUnbind(&s,o));
    assert(SudekiMpStoryAreaRetireBegin(&s,inside));
    assert(SudekiMpStoryAreaRetireReturned(&s,inside));
}
static void retirement_pins_parent_and_aba(void) {
    SudekiMpStoryAreaRef outside;
    SudekiMpStoryAreas s=initial(&outside);
    SudekiMpStoryAreaRef inside=interior(&s),fresh;
    uint64_t pins[SUDEKIMP_STORY_AREA_PINS],extra=999;
    assert(!SudekiMpStoryAreaRetireBegin(&s,outside)); /* interior return context */
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_PINS;++i)
        assert(SudekiMpStoryAreaRetain(&s,inside,&pins[i]));
    SudekiMpStoryAreas before=s;
    assert(!SudekiMpStoryAreaRetain(&s,inside,&extra) && extra==999);
    unchanged(&s,&before);
    assert(!SudekiMpStoryAreaRelease(&s,outside,pins[0]));
    assert(!SudekiMpStoryAreaRetireBegin(&s,inside));
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_PINS;++i) {
        assert(SudekiMpStoryAreaRelease(&s,inside,pins[i]));
        assert(!SudekiMpStoryAreaRelease(&s,inside,pins[i]));
    }
    assert(SudekiMpStoryAreaRetireBegin(&s,inside));
    assert(!SudekiMpStoryAreaReady(&s,inside));
    assert(!SudekiMpStoryAreaRetain(&s,inside,&extra));
    assert(!SudekiMpStoryAreaLoad(&s,"newbrightwater","fixture_interior",&fresh));
    assert(!SudekiMpStoryAreaRetireBegin(&s,outside)); /* even while child retires */
    assert(SudekiMpStoryAreaRetireReturned(&s,inside));
    fresh=interior(&s);
    assert(fresh.lifetime>inside.lifetime && fresh.slot==inside.slot);
    before=s;
    assert(!SudekiMpStoryAreaReady(&s,inside));
    assert(!SudekiMpStoryAreaRelease(&s,inside,pins[0]));
    assert(!SudekiMpStoryAreaRetireReturned(&s,inside));
    unchanged(&s,&before);
    assert(SudekiMpStoryAreaRetireBegin(&s,fresh));
    assert(SudekiMpStoryAreaRetireReturned(&s,fresh));
    assert(SudekiMpStoryAreaRetireBegin(&s,outside));
    assert(SudekiMpStoryAreaRetireReturned(&s,outside));
    assert(!SudekiMpStoryAreasInitialize(&s,102)); /* session reset is not teardown */
}
static void malformed_stale_and_exhausted(void) {
    SudekiMpStoryAreaRef outside;
    SudekiMpStoryAreas s=initial(&outside),before=s;
    SudekiMpStoryAreaRef out={0},wrong=outside;
    uint64_t ticket=123;
    assert(!SudekiMpStoryAreasInitialize(NULL,1));
    assert(!SudekiMpStoryAreaLoad(NULL,"newbrightwater","",&out));
    assert(!SudekiMpStoryAreaLoad(&s,"NewBrightwater","fixture_interior",&out));
    assert(!SudekiMpStoryAreaLoad(&s,"","fixture_interior",&out));
    assert(!SudekiMpStoryAreaLoad(&s,"newbrightwater","../church",&out));
    assert(!SudekiMpStoryAreaLoad(&s,"otherworld","fixture_interior",&out));
    char long_name[65]; memset(long_name,'a',64); long_name[64]=0;
    assert(!SudekiMpStoryAreaLoad(&s,"newbrightwater",long_name,&out));
    unchanged(&s,&before);
    SudekiMpStoryAreaOwner o=owner(0,3),forged=o;
    ++forged.session;
    assert(!SudekiMpStoryAreaBind(&s,forged,outside));
    assert(SudekiMpStoryAreaBind(&s,o,outside));
    assert(SudekiMpStoryAreaBind(&s,o,outside));
    assert(!SudekiMpStoryAreaBind(&s,owner(1,3),outside)); /* one actor per owner */
    SudekiMpStoryAreaRef inside=interior(&s);
    assert(!SudekiMpStoryAreaLoad(&s,"newbrightwater","other_interior",&out));
    assert(!SudekiMpStoryAreaRequest(&s,o,outside,&ticket) && ticket==123);
    assert(SudekiMpStoryAreaRequest(&s,o,inside,&ticket));
    assert(!SudekiMpStoryAreaStarted(&s,forged,ticket,1));
    assert(!SudekiMpStoryAreaStarted(&s,o,ticket,0));
    assert(SudekiMpStoryAreaCancelRequest(&s,o,ticket));
    assert(!SudekiMpStoryAreaStarted(&s,o,ticket,1));
    assert(!SudekiMpStoryAreaCancelRequest(&s,o,ticket));
    assert(SudekiMpStoryAreaRequest(&s,o,inside,&ticket));
    assert(SudekiMpStoryAreaStarted(&s,o,ticket,1));
    ++wrong.session;
    before=s;
    assert(!SudekiMpStoryAreaNativeReturned(&s,o,ticket,1,wrong));
    assert(!SudekiMpStoryAreaRevoke(&s,forged));
    unchanged(&s,&before);
    /* Exact source placement proves rollback, even if destination load failed. */
    finish(&s,o,ticket,1,outside);
    assert(SudekiMpStoryAreaRefSame(s.players[0].area,outside));
    s.next_ticket=UINT64_MAX; before=s;
    assert(!SudekiMpStoryAreaRequest(&s,o,inside,&ticket)); unchanged(&s,&before);
    s.next_pin=UINT64_MAX; before=s;
    assert(!SudekiMpStoryAreaRetain(&s,inside,&ticket)); unchanged(&s,&before);
    assert(SudekiMpStoryAreaRetireBegin(&s,inside));
    assert(SudekiMpStoryAreaRetireReturned(&s,inside));
    s.next_lifetime=UINT64_MAX; before=s;
    assert(!SudekiMpStoryAreaLoad(&s,"newbrightwater","fixture_interior",&out));
    unchanged(&s,&before);
}
int main(void) {
    independent_entry_join_and_exit();
    concurrent_requests_and_opposite_travel();
    disconnect_retains_native_and_view_lifetimes();
    retirement_pins_parent_and_aba();
    malformed_stale_and_exhausted();
    puts("story_area_test: PASS (host policy only; native multi-area gameplay unproven)");
    return 0;
}
