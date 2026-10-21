/* Actual H1b new-entry gate; no native entities or zone requests. */
#include "../src/hooks/lan_story_remote_interior.c"
#include <assert.h>
#include <stdio.h>
static unsigned away_mask;
static BOOL away_known=TRUE;
int SudekiMpLanStoryRuntimeCharacterAwayState(unsigned c) { return away_known?!!(away_mask&(1u<<c)):-1; }

static BOOL split_on=TRUE,parked_body=TRUE,unpark_ok=TRUE,roster_exact=TRUE,holding;
static unsigned moves,parks,unparks,shows;static int zone_state=3;
static uint8_t actor[0x140],ai[0x170],arb[0x100],position[0x30];
static SudekiMpLanStoryNativeRoster test_roster;
static SudekiMpLanStoryScene test_scene;
void SudekiMpLogFormat(const char *f,...) {(void)f;}
void SudekiMpLogWrite(const char *s) {(void)s;}
BOOL SudekiMpLanStorySplitActive(void) {return split_on;}
BOOL SudekiMpLanStorySplitMemberParked(unsigned c) {assert(c==2);return parked_body;}
BOOL SudekiMpLanStorySplitUnparkMember(unsigned c) {assert(c==2);if(members[c].away_return)assert(snap_changed && !base[RVA_SNAP_FLAG]);++unparks;if(unpark_ok)parked_body=FALSE;return unpark_ok;}
BOOL SudekiMpLanStorySplitParkMember(unsigned c) {assert(c==2);++parks;parked_body=TRUE;return TRUE;}
int SudekiMpLanStorySplitShowMember(const SudekiMpLanStoryNativeRoster *r,unsigned c) {(void)r;assert(c==2);++shows;return 1;}
void SudekiMpLanStorySplitReleaseMember(unsigned c,const char *why) {(void)c;(void)why;}
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *r) {return roster_exact && r==&test_roster;}
BOOL SudekiMpLanStoryObserverHolding(void) {return holding;}
void SudekiMpLanStoryObserverSetRemoteVisit(const char *s) {(void)s;}
void SudekiMpLanStoryObserverNoteResidentHouse(const char *s) {(void)s;}
BOOL SudekiMpLanStoryObserverSetRemoteInterior(const char *zone,uint8_t mask) {assert(!strcmp(zone,doors[0].zone));test_scene.inside_mask|=mask;return TRUE;}
void SudekiMpLanStoryObserverClearRemoteMembers(uint8_t mask) {test_scene.inside_mask&=~mask;}
int SudekiMpLanStoryAreaZoneState(const char *s) {(void)s;return zone_state;}
int SudekiMpLanStoryAreaUnloadResident(const char *s) {(void)s;return 1;}
BOOL SudekiMpLanStoryAreaLoadResident(const char *s) {(void)s;return TRUE;}
BOOL SudekiMpLanStoryAreaResidentUnloadable(const char *s) {(void)s;return TRUE;}
BOOL SudekiMpLanStoryAreaResidentActivate(const char *s) {(void)s;return TRUE;}
void SudekiMpLanStorySkyHoldLighting(BOOL hold) {(void)hold;}
void SudekiMpLanStorySkyReleaseHeld(BOOL apply) {(void)apply;}
static void __attribute__((fastcall)) placement(void *p,const float *xyz) {
    assert(p==position && snap_changed && !base[RVA_SNAP_FLAG]);++moves;memcpy(position+0x18,xyz,12);
}
static void setup(void) {
    memset(members,0,sizeof(members));memset(actor,0,sizeof(actor));memset(ai,0,sizeof(ai));memset(arb,0,sizeof(arb));
    ready=TRUE;house=NULL;house_draining=house_loaded_by_us=FALSE;split_on=parked_body=unpark_ok=roster_exact=TRUE;holding=FALSE;
    moves=parks=unparks=shows=0;zone_state=3;snap_changed=FALSE;base[RVA_SNAP_FLAG]=1;
    test_roster=(SudekiMpLanStoryNativeRoster){.epoch=7,.available_mask=12,.leader_character=3};
    test_roster.actors[2]=actor;test_roster.ai[2]=ai;
    *(void **)(actor+0x94)=ai;*(void **)(ai+0x10)=actor;*(void **)(actor+0x90)=arb;*(void **)(arb+0x10)=actor;
    *(void **)(actor+0x44)=position;*(void **)position=base+RVA_POSITION_VTABLE;*(void **)(position+0x10)=actor;
    test_scene=(SudekiMpLanStoryScene){.epoch=7,.revision=1,.phase=SUDEKIMP_LAN_STORY_READY,.available_mask=12,.leader_seat=3,.inside_mask=8};
    strcpy(test_scene.temporary,doors[0].zone);away_mask=4;away_known=TRUE;
}
static void return_cases(void) {
    base=VirtualAlloc(NULL,0x410000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(base);
    memcpy(base+RVA_SNAP_EXPORT,snap_entry,5);*(void **)(base+RVA_SNAP_EXPORT+5)=base+RVA_SNAP_FLAG;base[RVA_SNAP_EXPORT+9]=0xc3;
    base[RVA_POSITION_SET]=0xe9;*(int32_t *)(base+RVA_POSITION_SET+1)=(uint8_t *)placement-(base+RVA_POSITION_SET+5);
    setup();assert(!SudekiMpLanStoryRemoteInteriorAwayReturn(&test_roster,&test_scene,2,100));
    assert(members[2].phase==LOADING && parked_body && !moves);
    member_service(2,&test_roster,&test_scene,4,0,TRUE,1699);assert(!moves);
    zone_state=1;member_service(2,&test_roster,&test_scene,4,0,TRUE,1700);assert(!moves);
    zone_state=3;roster_exact=FALSE;member_service(2,&test_roster,&test_scene,4,0,TRUE,1700);assert(!moves);
    roster_exact=TRUE;
    SudekiMpLanStoryScene frame=test_scene; /* service's snapshot predates member_enter */
    member_service(2,&test_roster,&frame,4,0,TRUE,1700);
    SudekiMpLanStoryRemoteInteriorAwayDrained(&test_roster,&frame,2);
    assert(!parked_body && parks==0);
    assert(moves==1 && unparks==1 && !parked_body && snap_changed && members[2].phase==INSIDE && members[2].away_inside);
    members[2].exit_at=1;member_service(2,&test_roster,&test_scene,4,0,TRUE,50000);
    assert(members[2].phase==INSIDE && !members[2].exit_at && moves==1);
    unsigned inside_shows=shows;
    assert(SudekiMpLanStoryRemoteInteriorAwayReturn(&test_roster,&test_scene,2,50000)==1 && shows==inside_shows+1 && !members[2].away_inside);
    SudekiMpLanStoryRemoteInteriorAwayDrained(&test_roster,&test_scene,2);assert(members[2].away_inside);
    evict();assert(members[2].phase==IDLE && moves==2);
    setup();unpark_ok=FALSE;assert(!SudekiMpLanStoryRemoteInteriorAwayReturn(&test_roster,&test_scene,2,100));
    member_service(2,&test_roster,&test_scene,4,0,TRUE,1700);assert(parked_body && !moves && members[2].phase==IDLE);
    setup();strcpy(test_scene.temporary,"LICo_Frappe_Farm");assert(SudekiMpLanStoryRemoteInteriorAwayReturn(&test_roster,&test_scene,2,100)==1 && !parked_body && !moves);
    for(unsigned guard=0;guard<6;++guard) {
        setup();Door other=doors[0];
        if(guard==0)split_on=FALSE;
        if(guard==1)house_draining=TRUE;
        if(guard==2)house=&other;
        if(guard==3)*(uint32_t *)(ai+0x44)=8;
        if(guard==4)*(uint32_t *)(arb+0x50)=0x10000;
        if(guard==5)holding=TRUE;
        int result=SudekiMpLanStoryRemoteInteriorAwayReturn(&test_roster,&test_scene,2,100);
        assert((guard==5?result==0:result<0) && parked_body && !moves && !unparks);
    }
    setup();assert(!SudekiMpLanStoryRemoteInteriorAwayReturn(&test_roster,&test_scene,2,100));
    SudekiMpLanStoryRemoteInteriorCancelAwayReturn(2);assert(members[2].phase==IDLE && parked_body && !moves);
    setup();test_scene.inside_mask=8;SudekiMpLanStoryRemoteInteriorAwayDrained(&test_roster,&test_scene,2);assert(parks==1);
    setup();test_scene.phase=SUDEKIMP_LAN_STORY_LOADING;holding=TRUE;
    assert(SudekiMpLanStoryRemoteInteriorAwayReturn(&test_roster,&test_scene,2,100)<0 && parked_body && !moves && !unparks);
    setup();roster_exact=FALSE;
    assert(SudekiMpLanStoryRemoteInteriorAwayReturn(&test_roster,&test_scene,2,100)<0 && parked_body && !moves && !unparks);
    setup();parked_body=FALSE;holding=TRUE;
    assert(SudekiMpLanStoryRemoteInteriorAwayReturn(&test_roster,&test_scene,2,100)==1 && !moves && !unparks);
    setup();assert(!SudekiMpLanStoryRemoteInteriorAwayReturn(&test_roster,&test_scene,2,100));
    member_service(2,&test_roster,&test_scene,4,0,TRUE,1700);
    parked_body=TRUE;unpark_ok=FALSE;unsigned before_shows=shows;
    assert(SudekiMpLanStoryRemoteInteriorAwayReturn(&test_roster,&test_scene,2,1800)==0 && parked_body && shows==before_shows);
    unpark_ok=TRUE;
    assert(SudekiMpLanStoryRemoteInteriorAwayReturn(&test_roster,&test_scene,2,1801)==1 && !parked_body && shows==before_shows+1 && !members[2].away_inside);
    VirtualFree(base,0,MEM_RELEASE);base=NULL;
}
int main(void) {
    return_cases();memset(members,0,sizeof(members));
    SudekiMpLanStoryNativeRoster r={.leader_character=0,.available_mask=15};
    SudekiMpLanStoryScene s={0};
    for(unsigned c=0;c<4;++c) {
        away_mask=0; assert(!!entry_member_admitted(c,&r,&s,15)==(c!=0));
        away_mask=1u<<c; assert(!entry_member_admitted(c,&r,&s,15));
        away_mask=0;away_known=FALSE;assert(!entry_member_admitted(c,&r,&s,15));away_known=TRUE;
        assert(!entry_member_admitted(c,&r,&s,0));
        members[c].phase=LOADING; assert(!entry_member_admitted(c,&r,&s,15));
        members[c].phase=IDLE; s.inside_mask=1u<<c; assert(!entry_member_admitted(c,&r,&s,15)); s.inside_mask=0;
        r.available_mask=0; assert(!entry_member_admitted(c,&r,&s,15)); r.available_mask=15;
    }
    puts("H1b refuses new AI-away entries; returning IDLE member can enter"); return 0;
}

int SudekiMpLanStoryAreaBackgroundLoad(const char *s) {(void)s;assert(!"unexpected background load");return -1;}
