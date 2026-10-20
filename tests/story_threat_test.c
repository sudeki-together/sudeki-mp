#include "network/story_threat.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static void seats(void) {
    SudekiMpStoryThreatSeat s={.connected=1,.ready=1,.receipt=1000,.max_age=250};
    assert(SudekiMpStoryThreatSeatKind(&s,1250)==STORY_SEAT_REMOTE);
    assert(SudekiMpStoryThreatSeatKind(&s,1251)==STORY_SEAT_AI);
    assert(SudekiMpStoryThreatSeatKind(&s,999)==STORY_SEAT_REMOTE);
    s.receipt=0;assert(SudekiMpStoryThreatSeatKind(&s,1000)==STORY_SEAT_AI);s.receipt=1000;
    s.held=1;assert(SudekiMpStoryThreatSeatKind(&s,1000)==STORY_SEAT_AI);s.held=0;
    s.draining=1;assert(SudekiMpStoryThreatSeatKind(&s,1000)==STORY_SEAT_AI);s.draining=0;
    s.owned_ai=1;assert(SudekiMpStoryThreatSeatKind(&s,1000)==STORY_SEAT_AI);s.owned_ai=0;
    s.native_ai=1;assert(SudekiMpStoryThreatSeatKind(&s,1000)==STORY_SEAT_AI);
    s.away=1;assert(SudekiMpStoryThreatSeatKind(&s,1000)==STORY_SEAT_AWAY);
    s.absent=1;assert(SudekiMpStoryThreatSeatKind(&s,1000)==STORY_SEAT_ABSENT);
    s.down=1;assert(SudekiMpStoryThreatSeatKind(&s,1000)==STORY_SEAT_DOWN);
    s=(SudekiMpStoryThreatSeat){.host=1};assert(SudekiMpStoryThreatSeatKind(&s,1000)==STORY_SEAT_HOST);
    s.host=0;assert(SudekiMpStoryThreatSeatKind(&s,1000)==STORY_SEAT_AI);
}
static void config_tests(void) {
    SudekiMpStoryThreatConfig c;SudekiMpStoryThreatDefaults(&c);
    assert(c.level==STORY_TARGET_WATCH && c.cap_parity && c.lead_always);
    assert(c.value[STORY_MELEE_REACH]==3 && c.value[STORY_THREAT_HALF_LIFE]==4);
    assert(SudekiMpStoryThreatExcluded(&c,"MON_GuardSparPartner_Melee"));
    assert(!SudekiMpStoryThreatExcluded(&c,"MON_Grotesque"));
    for(unsigned i=0;i<STORY_THRESHOLD_COUNT;++i) {
        assert(!strncmp(SudekiMpStoryThreatKeys[i],i<STORY_DEFEND_MARGIN_FRACTION?"EnemyTargeting":"CompanionDefend",14));
        assert(SudekiMpStoryThreatParse(&c,SudekiMpStoryThreatKeys[i],"2"));
        assert(c.value[i]==2);
        assert(!SudekiMpStoryThreatParse(&c,SudekiMpStoryThreatKeys[i],"nan"));
        assert(!SudekiMpStoryThreatParse(&c,SudekiMpStoryThreatKeys[i],"-1"));
        assert(!SudekiMpStoryThreatParse(&c,SudekiMpStoryThreatKeys[i],"2junk"));
        assert(c.value[i]==2);
    }
    assert(!SudekiMpStoryThreatParse(&c,"EnemyTargetingD0","0"));
    assert(!SudekiMpStoryThreatParse(&c,"EnemyTargetingAlertMelee","1.5"));
    assert(!SudekiMpStoryThreatParse(&c,"unknown","1"));
    assert(SudekiMpStoryThreatParse(&c,"EnemyTargeting","tier2"));
    assert(SudekiMpStoryThreatParse(&c,"EnemyTargetingCapParity","off"));
    assert(SudekiMpStoryThreatEffectiveLevel(&c)==STORY_TARGET_TIER0);
    assert(SudekiMpStoryThreatParse(&c,"EnemyTargetingCapParity","on"));
    assert(SudekiMpStoryThreatEffectiveLevel(&c)==STORY_TARGET_TIER2);
    assert(SudekiMpStoryThreatParse(&c,"EnemyTargetingLeadRule","always"));assert(c.lead_always);
    assert(!SudekiMpStoryThreatParse(&c,"EnemyTargetingLeadRule","nearest"));
    assert(SudekiMpStoryThreatParse(&c,"EnemyTargetingExcludeTypes"," One, two ; THREE "));
    assert(SudekiMpStoryThreatExcluded(&c,"two") && SudekiMpStoryThreatExcluded(&c,"three"));
    assert(!SudekiMpStoryThreatExcluded(&c,"tw"));
}
static void caps(void) {
    SudekiMpStoryThreatConfig c;SudekiMpStoryThreatDefaults(&c);
    for(unsigned level=0;level<=STORY_TARGET_TIER2;++level)
        for(unsigned kind=0;kind<=STORY_SEAT_HOST;++kind)
            for(unsigned type=0;type<=6;++type) {
                c.level=level;
                int want=level>=STORY_TARGET_TIER0 &&
                    (kind==STORY_SEAT_REMOTE || kind==STORY_SEAT_AWAY) && type>=1 && type<=3;
                assert(SudekiMpStoryThreatCapParity(&c,1,kind,type,0)==want);
                assert(!SudekiMpStoryThreatCapParity(&c,0,kind,type,0));
                assert(!SudekiMpStoryThreatCapParity(&c,1,kind,type,1));
                c.cap_parity=0;assert(!SudekiMpStoryThreatCapParity(&c,1,kind,type,0));c.cap_parity=1;
            }
    /* Independent fixed checksums from tools/sudekimod.py. */
    assert(SudekiMpStoryThreatParse(&c,"EnemyTargetingExcludeTypes","ab, cD ; xyz"));
    assert(SudekiMpStoryThreatResourceExcluded(&c,4290)); /* 65 * 66 */
    assert(SudekiMpStoryThreatResourceExcluded(&c,4556)); /* 67 * 68 */
    assert(SudekiMpStoryThreatResourceExcluded(&c,7922)); /* 88 * 89 + 90 */
    assert(!SudekiMpStoryThreatResourceExcluded(&c,4291));
    assert(!SudekiMpStoryThreatResourceExcluded(&c,0));
    assert(SudekiMpStoryThreatCapOnly(3,3,1,0,0,0));
    assert(!SudekiMpStoryThreatCapOnly(3,3,0,0,0,0));
    assert(!SudekiMpStoryThreatCapOnly(3,3,3,0,0,0));
    assert(!SudekiMpStoryThreatCapOnly(1,3,1,0,0,0));
    assert(SudekiMpStoryThreatCapOnly(-1,-1,15,15,0,0));
    assert(!SudekiMpStoryThreatCapOnly(3,3,1,0,1,0));
    assert(SudekiMpStoryThreatCapOnly(3,3,2,0,1,0));
    assert(!SudekiMpStoryThreatCapOnly(3,3,0,1,1,1));
    assert(SudekiMpStoryThreatCapOnly(3,3,0,2,1,1));
    assert(!SudekiMpStoryThreatCapOnly(3,0,1,0,0,0));
}
static void lead_tests(void) {
    SudekiMpStoryThreatConfig c;SudekiMpStoryThreatDefaults(&c);c.level=STORY_TARGET_TIER0;
    unsigned k[4]={STORY_SEAT_HOST,STORY_SEAT_REMOTE,STORY_SEAT_AI,STORY_SEAT_AWAY};
    float d[4]={10,3,1,2};
#define PICK(mask,current) SudekiMpStoryThreatLeadPick(&c,1,1,0,11,1,current,mask,k,d)
    assert(PICK(15,0)==1); /* nearby client, rather than lead or closer AI */
    d[1]=9;assert(PICK(15,0)==0); /* authored 20% / 2-unit stickiness */
    d[0]=50;d[1]=41;assert(PICK(15,0)==0);
    d[1]=39;assert(PICK(15,0)==1);
    d[0]=10;d[1]=3;k[0]=STORY_SEAT_DOWN;assert(PICK(15,0)==1);
    k[1]=STORY_SEAT_ABSENT;assert(PICK(15,0)==2); /* living AI fallback */
    k[2]=STORY_SEAT_DOWN;assert(PICK(15,0)==3); /* living away fallback */
    k[3]=STORY_SEAT_DOWN;assert(PICK(15,0)==4);
    k[0]=STORY_SEAT_HOST;k[1]=STORY_SEAT_REMOTE;k[2]=STORY_SEAT_AI;k[3]=STORY_SEAT_AWAY;
    assert(PICK(12,4)==2); /* human candidates filtered out by native range/caps */
    assert(PICK(8,4)==3);assert(PICK(0,4)==4);
    assert(PICK(15,2)==1); /* human priority supersedes an AI fallback */
    d[0]=d[1]=3;assert(PICK(3,4)==0);assert(PICK(3,1)==1);
    d[0]=INFINITY;d[1]=NAN;assert(PICK(15,4)==2);
    d[0]=-1;d[1]=3;assert(PICK(15,4)==1);d[0]=10;
    for(unsigned mode=0;mode<16;++mode)
        assert(SudekiMpStoryThreatLeadPick(&c,1,1,0,mode,1,4,15,k,d)==(mode==11?1u:4u));
    for(unsigned type=0;type<=6;++type)
        assert(SudekiMpStoryThreatLeadPick(&c,1,type,0,11,1,4,15,k,d)==(type>=1 && type<=3?1u:4u));
    assert(SudekiMpStoryThreatLeadPick(&c,0,1,0,11,1,4,15,k,d)==4);
    assert(SudekiMpStoryThreatLeadPick(&c,1,1,1,11,1,4,15,k,d)==4);
    c.lead_always=0;assert(PICK(15,4)==4);
    assert(SudekiMpStoryThreatLeadPick(&c,1,1,0,11,0,4,15,k,d)==1);
    c.lead_always=1;c.level=STORY_TARGET_WATCH;assert(PICK(15,4)==4);
    c.level=STORY_TARGET_OFF;assert(PICK(15,4)==4);
#undef PICK
}
static void defend_tests(void) {
    SudekiMpStoryThreatConfig c;SudekiMpStoryThreatDefaults(&c);c.level=STORY_TARGET_TIER0;
    unsigned k[4]={STORY_SEAT_HOST,STORY_SEAT_REMOTE,STORY_SEAT_AI,STORY_SEAT_AWAY};
    float d[4]={10,3,0,1};
#define ANCHOR(mask,current) SudekiMpStoryThreatDefendAnchor(&c,1,2,current,mask,k,d)
    assert(ANCHOR(15,4)==1); /* human before the closer AI companion or self */
    d[1]=9;assert(ANCHOR(15,0)==0);
    d[0]=50;d[1]=41;assert(ANCHOR(15,0)==0);d[1]=39;assert(ANCHOR(15,0)==1);
    k[0]=STORY_SEAT_DOWN;k[1]=STORY_SEAT_ABSENT;assert(ANCHOR(15,0)==3);
    k[3]=STORY_SEAT_DOWN;assert(ANCHOR(15,3)==2); /* only survivor */
    k[1]=STORY_SEAT_REMOTE;assert(ANCHOR(15,2)==1); /* human returns */
    assert(ANCHOR(4,4)==2);assert(ANCHOR(0,4)==4);
    k[2]=STORY_SEAT_AWAY;assert(ANCHOR(15,4)==1);
    k[2]=STORY_SEAT_DOWN;assert(ANCHOR(15,4)==4);k[2]=STORY_SEAT_AI;
    d[1]=NAN;assert(ANCHOR(15,1)==2);d[1]=3;
    assert(SudekiMpStoryThreatDefendAnchor(&c,0,2,4,15,k,d)==4);
    c.level=STORY_TARGET_WATCH;assert(ANCHOR(15,4)==4);
    c.level=STORY_TARGET_OFF;assert(ANCHOR(15,4)==4);
    c.level=STORY_TARGET_TIER0;c.cap_parity=0;assert(ANCHOR(15,4)==1);
#undef ANCHOR
}
static void memory_tests(void) {
    SudekiMpStoryThreatConfig c;SudekiMpStoryThreatDefaults(&c);
    SudekiMpStoryThreatRecord r={.serial=1,.updated_at=1000,.threat={.8f},.sniper={.4f}};
    SudekiMpStoryThreatDecay(&c,&r,5000);assert(fabsf(r.threat[0]-.4f)<.00001f && fabsf(r.sniper[0]-.2f)<.00001f);
    c.value[STORY_THREAT_HALF_LIFE]=2;SudekiMpStoryThreatDecay(&c,&r,7000);assert(fabsf(r.threat[0]-.2f)<.00001f);
    SudekiMpStoryThreatDecay(&c,&r,6990);assert(fabsf(r.threat[0]-.2f)<.00001f);
    SudekiMpStoryThreatDefaults(&c);
    int coop=1,excluded=0,ranged=1,known=1,all=0,firing=1,unengaged=1,far=1;
    unsigned type=1,seat=0,kind=STORY_SEAT_HOST;float amount=15,hp=100;
#define HIT() SudekiMpStoryThreatAddHit(&c,coop,type,excluded,&r,seat,kind,amount,hp,ranged,known,all,firing,unengaged,far,1000)
#define HIT_REFUSES(change,restore) do {r=(SudekiMpStoryThreatRecord){.serial=1,.updated_at=2,.threat={.7f}}; \
    SudekiMpStoryThreatRecord before=r;change;SudekiMpStoryThreatHitAdded result=HIT(); \
    assert(!result.added_threat && !result.added_sniper && !memcmp(&r,&before,sizeof(r)));restore;} while(0)
    HIT_REFUSES(coop=0,coop=1);HIT_REFUSES(type=0,type=1);HIT_REFUSES(type=4,type=1);HIT_REFUSES(type=6,type=1);
    HIT_REFUSES(excluded=1,excluded=0);HIT_REFUSES(seat=4,seat=0);HIT_REFUSES(hp=0,hp=100);
    HIT_REFUSES(hp=-1,hp=100);HIT_REFUSES(hp=NAN,hp=100);HIT_REFUSES(amount=-1,amount=15);
    HIT_REFUSES(amount=NAN,amount=15);HIT_REFUSES(amount=INFINITY,amount=15);
#define NO_SNIPER(change,restore) do {r=(SudekiMpStoryThreatRecord){.serial=1};change; \
    SudekiMpStoryThreatHitAdded result=HIT();assert(result.added_threat && !result.added_sniper && fabsf(r.threat[0]-.15f)<.000001f && !r.sniper[0]);restore;} while(0)
    NO_SNIPER(ranged=0,ranged=1);NO_SNIPER(known=0,known=1);NO_SNIPER(all=1,all=0);
    NO_SNIPER(firing=0,firing=1);NO_SNIPER(unengaged=0,unengaged=1);NO_SNIPER(far=0,far=1);
    for(kind=0;kind<=STORY_SEAT_HOST;++kind) {
        r=(SudekiMpStoryThreatRecord){.serial=1};SudekiMpStoryThreatHitAdded added=HIT();
        assert(added.added_threat && fabsf(r.threat[0]-.15f)<.000001f);
        assert(added.added_sniper==(kind==STORY_SEAT_HOST || kind==STORY_SEAT_REMOTE));
    }
    kind=STORY_SEAT_HOST;r=(SudekiMpStoryThreatRecord){.serial=1};assert(HIT().added_sniper);
    unsigned kinds[4]={STORY_SEAT_HOST,STORY_SEAT_REMOTE,STORY_SEAT_AI,STORY_SEAT_AWAY};float d[4]={0};
    assert(SudekiMpStoryThreatPick(&c,STORY_TARGET_TIER1,1,1,0,0,&r,9000,4,15,kinds,d)==4);
    assert(SudekiMpStoryThreatDecide(&c,STORY_TARGET_TIER1,1,1,0,0,0,&r,NULL,1,9000,4,15,kinds,d,4).seat==4);
#undef NO_SNIPER
#undef HIT_REFUSES
#undef HIT
}
static void predicates_and_pick_tests(void) {
    SudekiMpStoryThreatConfig c;SudekiMpStoryThreatDefaults(&c);
    SudekiMpStoryThreatRecord r={.serial=7};SudekiMpStoryThreatPushState pushes[4][2]={0};
    assert(SudekiMpStoryThreatSubstitutable(&r));r.mode_seen=1;
    for(unsigned mode=0;mode<16;++mode) {r.last_mode=mode;assert(SudekiMpStoryThreatSubstitutable(&r)==(mode==0 || mode==11));}
    assert(SudekiMpStoryThreatShooterListed(&c,&r,0,1000)==STORY_LIST_UNKNOWN);
    r.list_seen=1;r.last_list_at=1000;r.last_list_mask=1;
    assert(SudekiMpStoryThreatShooterListed(&c,&r,0,2000)==STORY_LIST_TRUE);
    assert(SudekiMpStoryThreatShooterListed(&c,&r,1,2000)==STORY_LIST_FALSE);
    assert(SudekiMpStoryThreatShooterListed(&c,&r,0,2001)==STORY_LIST_UNKNOWN);
    r.responding[0]=(SudekiMpStoryThreatResponding){.active=1,.shooter=0,.raise_seq=4};
    pushes[0][0]=(SudekiMpStoryThreatPushState){.active=1,.serial=7,.start_seq=4};
    for(unsigned seq=3;seq<=5;++seq) {
        r.last_list_seq=seq;
        assert(SudekiMpStoryThreatResponderListed(&r,0)==(seq<=4?STORY_LIST_UNKNOWN:STORY_LIST_TRUE));
        assert(SudekiMpStoryThreatPushListed(&r,&pushes[0][0],0)==(seq<=4?STORY_LIST_UNKNOWN:STORY_LIST_TRUE));
    }
    r.last_list_mask=0;assert(SudekiMpStoryThreatResponderListed(&r,0)==STORY_LIST_FALSE);
    assert(SudekiMpStoryThreatPushListed(&r,&pushes[0][0],0)==STORY_LIST_FALSE);
    r.list_seen=0;assert(SudekiMpStoryThreatResponderListed(&r,0)==STORY_LIST_UNKNOWN);
    assert(SudekiMpStoryThreatPushListed(&r,&pushes[0][0],0)==STORY_LIST_UNKNOWN);
    assert(SudekiMpStoryThreatDesignated(&r,0,pushes));assert(!SudekiMpStoryThreatDesignated(&r,1,pushes));
    r.responding[0].active=0;assert(SudekiMpStoryThreatDesignated(&r,0,pushes));
    pushes[0][0].serial=8;assert(!SudekiMpStoryThreatDesignated(&r,0,pushes));
    unsigned k[4]={STORY_SEAT_HOST,STORY_SEAT_REMOTE,STORY_SEAT_AI,STORY_SEAT_AWAY};float d[4]={0};
    r=(SudekiMpStoryThreatRecord){.serial=1,.updated_at=1000,.threat={.01f,.2f,10,20}};
    unsigned level=STORY_TARGET_TIER1,type=1,mode=0,eligible=15,current=0;int coop=1,excluded=0;
#define PICK() SudekiMpStoryThreatPick(&c,level,coop,type,excluded,mode,&r,1000,current,eligible,k,d)
    assert(PICK()==1);r.threat[0]=.17f;assert(PICK()==0);r.threat[1]=.3f;assert(PICK()==1);
    r.threat[0]=r.threat[1]=.05f;assert(PICK()==4);r.threat[0]=.2f;r.threat[1]=0;
#define PICK_REFUSES(change,restore) do {change;assert(PICK()==4);restore;assert(PICK()==0);} while(0)
    PICK_REFUSES(coop=0,coop=1);PICK_REFUSES(type=0,type=1);PICK_REFUSES(type=4,type=1);PICK_REFUSES(type=6,type=1);
    PICK_REFUSES(excluded=1,excluded=0);PICK_REFUSES(eligible=14,eligible=15);
    PICK_REFUSES(d[0]=NAN,d[0]=0);PICK_REFUSES(d[0]=-1,d[0]=0);
    for(unsigned bad=STORY_SEAT_DOWN;bad<=STORY_SEAT_AI;++bad) {PICK_REFUSES(k[0]=bad,k[0]=STORY_SEAT_HOST);}
    for(unsigned bad=0;bad<16;++bad) if(bad!=0 && bad!=11) {PICK_REFUSES(mode=bad,mode=0);}
    for(unsigned bad=0;bad<=STORY_TARGET_TIER0;++bad) {PICK_REFUSES(level=bad,level=STORY_TARGET_TIER1);}
    d[0]=c.value[STORY_D0];r.threat[0]=.1f;assert(PICK()==4);r.threat[0]=.101f;assert(PICK()==0);
#undef PICK_REFUSES
#undef PICK
}
static void alert_selection_tests(void) {
    SudekiMpStoryThreatConfig c;SudekiMpStoryThreatDefaults(&c);
    SudekiMpStoryThreatRecord r={.serial=10,.updated_at=1000,.sniper={.2f}};
    unsigned level=STORY_TARGET_TIER15,type=1,seat=0,kind=STORY_SEAT_HOST;int coop=1,excluded=0,added=1,active=0;
#define RAISE() SudekiMpStoryThreatAlertRaise(&c,level,coop,type,excluded,0,&r,seat,kind,added,active,1000)
#define RAISE_REFUSES(change,restore) do {change;SudekiMpStoryThreatRecord before=r;assert(!RAISE());assert(!memcmp(&before,&r,sizeof(r)));restore;assert(RAISE());} while(0)
    assert(RAISE());RAISE_REFUSES(r.sniper[0]=.1f,r.sniper[0]=.2f);
    RAISE_REFUSES(added=0,added=1);RAISE_REFUSES(active=1,active=0);
    RAISE_REFUSES(r.alerting[0].active=1,r.alerting[0].active=0);
    r.alerting[0].cooldown_until=2000;RAISE_REFUSES(r.alerting[0].cooldown_active=1,r.alerting[0].cooldown_active=0);
    RAISE_REFUSES(coop=0,coop=1);RAISE_REFUSES(type=0,type=1);RAISE_REFUSES(type=4,type=1);RAISE_REFUSES(type=6,type=1);
    RAISE_REFUSES(excluded=1,excluded=0);RAISE_REFUSES(level=STORY_TARGET_TIER1,level=STORY_TARGET_TIER15);
    RAISE_REFUSES(kind=STORY_SEAT_AI,kind=STORY_SEAT_HOST);RAISE_REFUSES(kind=STORY_SEAT_AWAY,kind=STORY_SEAT_HOST);
    RAISE_REFUSES(seat=4,seat=0);
    r.alerting[0].cooldown_active=1;r.alerting[0].cooldown_until=999;active=1;
    assert(!RAISE() && r.alerting[0].cooldown_active);active=0;assert(RAISE() && !r.alerting[0].cooldown_active);
    SudekiMpStoryThreatCandidate v[7]={0};
    for(unsigned i=0;i<7;++i) v[i]=(SudekiMpStoryThreatCandidate){.serial=20+i,.distance=1+i,.type=1,.substitutable=1,
        .shooter_listed=STORY_LIST_UNKNOWN,.ranged_capable=i<4,.current_target_seat=4};
    v[2].same_group=v[3].same_group=1;v[3].distance=0; /* alerting enemy: own group, first */
    uint32_t out[4]={0,0,0,0x12345678};
    assert(SudekiMpStoryThreatAlertResponders(&c,0,v,7,out,3)==3);
    assert(out[0]==23 && out[1]==22 && out[2]==24 && out[3]==0x12345678);
    v[0].distance=c.value[STORY_ALERT_RADIUS];assert(SudekiMpStoryThreatAlertResponders(&c,0,v,1,out,1)==1);
#define RESP_REFUSES(change,restore) do {change;assert(!SudekiMpStoryThreatAlertResponders(&c,0,v,1,out,1));restore;assert(SudekiMpStoryThreatAlertResponders(&c,0,v,1,out,1)==1);} while(0)
    RESP_REFUSES(v[0].distance=25.01f,v[0].distance=25);RESP_REFUSES(v[0].distance=NAN,v[0].distance=25);
    RESP_REFUSES(v[0].excluded=1,v[0].excluded=0);RESP_REFUSES(v[0].type=4,v[0].type=1);
    RESP_REFUSES(v[0].substitutable=0,v[0].substitutable=1);RESP_REFUSES(v[0].designated=1,v[0].designated=0);
    RESP_REFUSES(v[0].shooter_listed=STORY_LIST_FALSE,v[0].shooter_listed=STORY_LIST_UNKNOWN);
    v[0].current_target_seat=1;RESP_REFUSES(v[0].current_target_threat=.001f,v[0].current_target_threat=0);
    v[0].current_target_threat=.2f;assert(SudekiMpStoryThreatAlertResponders(&c,.2f,v,1,out,1)==1);
    c.value[STORY_ALERT_RANGED]=c.value[STORY_ALERT_MELEE]=32;
    assert(SudekiMpStoryThreatAlertResponders(&c,.2f,v,7,out,4)==3 && out[3]==0x12345678);
    assert(SudekiMpStoryThreatAlertResponders(&c,.2f,v,7,out,1)==1);
    assert(!SudekiMpStoryThreatAlertResponders(&c,.2f,v,7,out,0));
#undef RESP_REFUSES
#undef RAISE_REFUSES
#undef RAISE
}
static void alert_fixture(SudekiMpStoryThreatConfig *c,SudekiMpStoryThreatRecord *a,
    SudekiMpStoryThreatRecord r[3],SudekiMpStoryThreatResponderView views[3],uint32_t now) {
    *a=(SudekiMpStoryThreatRecord){.serial=10};uint32_t serials[3]={20,21,22};SudekiMpStoryThreatRecord *records[3];
    for(unsigned i=0;i<3;++i) {r[i]=(SudekiMpStoryThreatRecord){.serial=serials[i]};records[i]=r+i;
        views[i]=(SudekiMpStoryThreatResponderView){r+i,0,STORY_LIST_UNKNOWN};}
    assert(SudekiMpStoryThreatAlertCommit(c,0,a,0,serials,3,records,now,50));
}
static void alert_lifetime_tests(void) {
    SudekiMpStoryThreatConfig c;SudekiMpStoryThreatDefaults(&c);
    SudekiMpStoryThreatRecord a,r[3];SudekiMpStoryThreatResponderView v[3];uint32_t released[3];unsigned n;
    unsigned level=STORY_TARGET_TIER15,kind=STORY_SEAT_HOST;int coop=1,firing=0;uint32_t now=1000;
#define RESET_ALERT() do {alert_fixture(&c,&a,r,v,1000);now=1000;level=STORY_TARGET_TIER15;kind=STORY_SEAT_HOST;coop=1;firing=0;} while(0)
#define FADE() SudekiMpStoryThreatAlertFade(&c,level,coop,0,&a,v,3,kind,firing,now,released,&n)
    RESET_ALERT();assert(a.alerting[0].raised_at==1000 && a.alerting[0].raise_seq==50 && a.alerting[0].count==3);
    assert(r[0].responding[0].alerting_serial==10 && r[0].responding[0].raise_seq==50 && r[0].responding[0].last_fire==1000);
    SudekiMpStoryThreatRecord before=a;assert(!SudekiMpStoryThreatAlertCommit(&c,0,&a,0,NULL,0,NULL,2000,60));assert(!memcmp(&before,&a,sizeof(a)));
    assert(FADE()==STORY_END_NONE);
#define ENDS(change,reason) do {RESET_ALERT();change;assert(FADE()==STORY_END_##reason && !a.alerting[0].active); \
    assert(a.alerting[0].cooldown_active && a.alerting[0].cooldown_until==now+10000);} while(0)
    ENDS(level=STORY_TARGET_TIER1,LEVEL);ENDS(coop=0,LEVEL);ENDS(kind=STORY_SEAT_DOWN,DOWN);ENDS(kind=STORY_SEAT_ABSENT,DOWN);
    ENDS(now=4001,STOP);ENDS(v[0].close=1,CLOSE);
    ENDS(v[0].listed=v[1].listed=v[2].listed=STORY_LIST_FALSE,LEFT);
    assert(n==3 && released[0]==20 && released[1]==21 && released[2]==22);
    ENDS(now=7000;firing=1,EXPIRED);
    RESET_ALERT();v[0].listed=STORY_LIST_FALSE;assert(!FADE() && n==1 && released[0]==20 && !r[0].responding[0].active);
    r[1].responding[0].recheck_failed=1;assert(!FADE() && n==1 && released[0]==21);
    assert(r[2].responding[0].active); /* UNKNOWN remains a belonging responder */
    /* Released responder joins B. A must never clear B's copy, even when its
     * old serial array still names that responder. */
    SudekiMpStoryThreatRecord b={.serial=11};uint32_t serial=20;SudekiMpStoryThreatRecord *record=&r[0];
    assert(SudekiMpStoryThreatAlertCommit(&c,0,&b,1,&serial,1,&record,1001,51));
    v[2].listed=STORY_LIST_FALSE;assert(FADE()==STORY_END_LEFT && n==1 && released[0]==22);
    assert(r[0].responding[0].active && r[0].responding[0].alerting_serial==11);
    SudekiMpStoryThreatAlertClear(0,&r[0],10,50);assert(r[0].responding[0].active);
    SudekiMpStoryThreatAlertClear(0,&r[0],11,50);assert(r[0].responding[0].active);
    SudekiMpStoryThreatAlertClear(0,&r[0],11,51);assert(!r[0].responding[0].active);
    /* Isolate both ownership clauses: neither a different origin nor a
     * different raise of the same origin may be released or counted. */
    for(unsigned mismatch=0;mismatch<2;++mismatch) {
        RESET_ALERT();
        if(mismatch) ++r[0].responding[0].raise_seq;
        else ++r[0].responding[0].alerting_serial;
        r[1].responding[0].active=r[2].responding[0].active=0;
        v[0].listed=STORY_LIST_FALSE;v[0].close=1;
        assert(FADE()==STORY_END_LEFT && n==0 && r[0].responding[0].active);
    }
    RESET_ALERT();SudekiMpStoryThreatTouch(&a,0,1,4000);SudekiMpStoryThreatTouch(&r[0],0,1,4000);
    assert(a.alerting[0].last_fire==1000 && r[0].responding[0].last_fire==1000);now=4001;assert(FADE()==STORY_END_STOP);
    RESET_ALERT();for(unsigned i=0;i<3;++i) SudekiMpStoryThreatTouch(r+i,0,0,3500);
    SudekiMpStoryThreatTouch(&a,0,0,3500);now=6000;assert(!FADE());now=6501;assert(FADE()==STORY_END_STOP);
    RESET_ALERT();a.sniper[0]=.2f;a.updated_at=1000;
    /* O2 source is either an alerting state or an orphan responding state;
     * refusal must not set or clear the second enemy's cooldown. */
    b=(SudekiMpStoryThreatRecord){.serial=11,.updated_at=1000,.sniper={.2f,.2f}};
    for(unsigned orphan=0;orphan<2;++orphan) {
        int active=orphan?r[0].responding[0].active:a.alerting[0].active;
        before=b;assert(!SudekiMpStoryThreatAlertRaise(&c,level,1,1,0,0,&b,0,kind,1,active,1000));assert(!memcmp(&before,&b,sizeof(b)));
        assert(SudekiMpStoryThreatAlertRaise(&c,level,1,1,0,0,&b,1,kind,1,0,1000));
    }
    record=&r[0];serial=20;assert(!SudekiMpStoryThreatAlertCommit(&c,0,&b,1,&serial,1,&record,1000,51));
    /* Orphan reason ordering and its explicit clear contract. */
    for(unsigned case_id=0;case_id<8;++case_id) {
        RESET_ALERT();int close=0,listed=STORY_LIST_UNKNOWN;unsigned expected;
        switch(case_id) {
        case 0:coop=0;expected=STORY_END_LEVEL;break;
        case 1:level=STORY_TARGET_TIER1;expected=STORY_END_LEVEL;break;
        case 2:kind=STORY_SEAT_DOWN;expected=STORY_END_DOWN;break;
        case 3:now=4001;expected=STORY_END_STOP;break;
        case 4:close=1;expected=STORY_END_CLOSE;break;
        case 5:listed=STORY_LIST_FALSE;expected=STORY_END_LEFT;break;
        case 6:r[0].responding[0].recheck_failed=1;expected=STORY_END_LEFT;break;
        default:now=7000;firing=1;expected=STORY_END_EXPIRED;break;
        }
        assert(SudekiMpStoryThreatResponderFade(&c,level,coop,0,&r[0],close,listed,kind,firing,now)==expected);
        SudekiMpStoryThreatAlertClear(0,&r[0],10,50);assert(!r[0].responding[0].active);
    }
    RESET_ALERT();kind=STORY_SEAT_AWAY;assert(!FADE());kind=STORY_SEAT_AI;assert(!FADE());
    a=(SudekiMpStoryThreatRecord){.serial=10};record=&a;serial=10;
    assert(SudekiMpStoryThreatAlertCommit(&c,0,&a,0,&serial,1,&record,0,0));
    v[0]=(SudekiMpStoryThreatResponderView){&a,0,STORY_LIST_FALSE};
    assert(SudekiMpStoryThreatAlertFade(&c,level,1,0,&a,v,1,STORY_SEAT_HOST,0,0,released,&n)==STORY_END_LEFT);
    assert(!a.alerting[0].active && !a.responding[0].active);
#undef ENDS
#undef FADE
#undef RESET_ALERT
}
static void push_and_decide_tests(void) {
    SudekiMpStoryThreatConfig c;SudekiMpStoryThreatDefaults(&c);SudekiMpStoryThreatPushState p={0};
    unsigned level=STORY_TARGET_TIER2,kind=STORY_SEAT_HOST;int coop=1,firing=1;uint32_t now=5000;
#define EVAL() SudekiMpStoryThreatPushEvaluate(&c,level,coop,0,&p,kind,firing,now)
    SudekiMpStoryThreatPushTrack(0,&p,1,0,0);assert(p.idle_active && p.idle_since==0);
    assert(EVAL());now=4999;assert(!EVAL());now=5000;
#define PUSH_REFUSES(change,restore) do {change;assert(!EVAL());restore;assert(EVAL());} while(0)
    PUSH_REFUSES(level=STORY_TARGET_TIER15,level=STORY_TARGET_TIER2);PUSH_REFUSES(coop=0,coop=1);
    for(unsigned bad=0;bad<STORY_SEAT_REMOTE;++bad) {PUSH_REFUSES(kind=bad,kind=STORY_SEAT_HOST);}
    PUSH_REFUSES(firing=0,firing=1);PUSH_REFUSES(p.idle_active=0,p.idle_active=1);PUSH_REFUSES(p.active=1,p.active=0);
    p.cooldown_until=6000;PUSH_REFUSES(p.cooldown_active=1,p.cooldown_active=0);
    p.cooldown_active=1;p.cooldown_until=4999;assert(EVAL() && !p.cooldown_active);
    SudekiMpStoryThreatPushStart(&c,0,&p,7,5000,20);
    assert(p.active && p.serial==7 && p.started_at==5000 && p.start_seq==20 && p.until==13000 && p.cooldown_until==20000);
    assert(!SudekiMpStoryThreatPushEnd(&c,level,1,0,&p,kind,1,0,1,STORY_LIST_UNKNOWN,6000));
    assert(!SudekiMpStoryThreatPushEnd(&c,level,1,0,&p,kind,0,1,1,STORY_LIST_UNKNOWN,6000));
    assert(SudekiMpStoryThreatPushEnd(&c,level,1,0,&p,kind,1,0,1,STORY_LIST_TRUE,13000)==STORY_END_EXPIRED && !p.active && p.cooldown_active);
    now=19999;assert(!EVAL());now=20000;assert(EVAL());
    for(unsigned test=0;test<3;++test) {
        SudekiMpStoryThreatPushStart(&c,0,&p,7,5000,20);
        if(test==1) p.active=0; /* engagement during cooldown alone */
        SudekiMpStoryThreatPushTrack(0,&p,test!=2,test!=2,6000);assert(!p.idle_active);
        p.active=0;SudekiMpStoryThreatPushTrack(0,&p,1,0,20000);now=24999;assert(!EVAL());now=25000;assert(EVAL());
    }
    p=(SudekiMpStoryThreatPushState){0};SudekiMpStoryThreatPushTrack(0,&p,1,0,0);firing=0;now=4000;assert(!EVAL());
    SudekiMpStoryThreatPushTrack(0,&p,1,0,4000);firing=1;now=5000;assert(EVAL()); /* not-firing does not reset idle */
    for(unsigned test=0;test<8;++test) {
        SudekiMpStoryThreatPushStart(&c,0,&p,7,5000,20);unsigned l=level,k=kind,others=0;int co=1,exists=1,listed=STORY_LIST_UNKNOWN;now=6000;unsigned expected;
        switch(test) {
        case 0:l=STORY_TARGET_TIER15;expected=STORY_END_LEVEL;break;
        case 1:co=0;expected=STORY_END_LEVEL;break;
        case 2:k=STORY_SEAT_DOWN;expected=STORY_END_DOWN;break;
        case 3:exists=0;expected=STORY_END_INVALID;break;
        case 4:p.recheck_failed=1;expected=STORY_END_INVALID;break;
        case 5:listed=STORY_LIST_FALSE;expected=STORY_END_INVALID;break;
        case 6:others=1;expected=STORY_END_ENGAGED;break;
        default:now=13000;expected=STORY_END_EXPIRED;break;
        }
        assert(SudekiMpStoryThreatPushEnd(&c,l,co,0,&p,k,1,others,exists,listed,now)==expected && !p.active && p.cooldown_active);
    }
    SudekiMpStoryThreatRecord r={.serial=7,.updated_at=1000,.threat={.2f,.1f}};
    SudekiMpStoryThreatPushState pushes[4][2]={0};unsigned k[4]={STORY_SEAT_HOST,STORY_SEAT_REMOTE,STORY_SEAT_AI,STORY_SEAT_AWAY};float d[4]={0};
    r.responding[0]=(SudekiMpStoryThreatResponding){.active=1,.shooter=1};pushes[0][0]=(SudekiMpStoryThreatPushState){.active=1,.serial=7};
    int complete=1,excluded=0;unsigned type=1,mode=11,eligible=15,lead=0;const SudekiMpStoryThreatRecord *record=&r;now=1000;
#define DECIDE() SudekiMpStoryThreatDecide(&c,level,coop,type,excluded,mode,0,record,pushes,complete,now,4,eligible,k,d,lead)
    assert(DECIDE().rule==STORY_RULE_ALERT && DECIDE().seat==1);
    r.responding[0].active=0;assert(DECIDE().rule==STORY_RULE_PUSH);
    pushes[0][0].serial=8;assert(DECIDE().rule==STORY_RULE_THREAT);pushes[0][0].serial=7;
    complete=0;assert(DECIDE().rule==STORY_RULE_THREAT);
    r.responding[0].active=1;assert(DECIDE().rule==STORY_RULE_THREAT);complete=1;
    r.responding[0].active=1;level=STORY_TARGET_TIER1;assert(DECIDE().rule==STORY_RULE_THREAT);
    level=STORY_TARGET_TIER0;assert(DECIDE().rule==STORY_RULE_LEAD);
    level=STORY_TARGET_TIER2;record=NULL;assert(DECIDE().rule==STORY_RULE_LEAD);record=&r;
    /* Every naming branch must independently preserve admission and mode gates. */
    for(unsigned branch=0;branch<4;++branch) {
        r.responding[0].active=branch==0;pushes[0][0].active=branch<=1;
        r.threat[0]=branch<=2?.2f:0;r.threat[1]=0;mode=branch==3?11:0;
        unsigned expected=branch==0?STORY_RULE_ALERT:branch==1?STORY_RULE_PUSH:branch==2?STORY_RULE_THREAT:STORY_RULE_LEAD;
        assert(DECIDE().rule==expected);
#define NO_DECISION(change,restore) do {change;assert(DECIDE().seat==4 && DECIDE().rule==STORY_RULE_NONE);restore;assert(DECIDE().rule==expected);} while(0)
        NO_DECISION(coop=0,coop=1);NO_DECISION(type=0,type=1);NO_DECISION(type=4,type=1);NO_DECISION(type=6,type=1);
        NO_DECISION(excluded=1,excluded=0);unsigned original_mode=mode;
        NO_DECISION(mode=1,mode=original_mode);NO_DECISION(mode=14,mode=original_mode);
        NO_DECISION(eligible=0,eligible=15);
        NO_DECISION(k[0]=k[1]=STORY_SEAT_DOWN,k[0]=STORY_SEAT_HOST;k[1]=STORY_SEAT_REMOTE);
        NO_DECISION(k[0]=k[1]=STORY_SEAT_ABSENT,k[0]=STORY_SEAT_HOST;k[1]=STORY_SEAT_REMOTE);
        if(branch<3) {
            NO_DECISION(k[0]=k[1]=STORY_SEAT_AI,k[0]=STORY_SEAT_HOST;k[1]=STORY_SEAT_REMOTE);
            NO_DECISION(k[0]=k[1]=STORY_SEAT_AWAY,k[0]=STORY_SEAT_HOST;k[1]=STORY_SEAT_REMOTE);
        }
#undef NO_DECISION
    }
#undef DECIDE
#undef PUSH_REFUSES
#undef EVAL
}
static void clock_wrap_tests(void) {
    SudekiMpStoryThreatConfig c;SudekiMpStoryThreatDefaults(&c);
    const uint32_t bases[]={0,0x7ffffff0u,0xfffffff0u};
    for(unsigned b=0;b<3;++b) {
        uint32_t now=bases[b];SudekiMpStoryThreatRecord a,r[3];SudekiMpStoryThreatResponderView v[3];
        alert_fixture(&c,&a,r,v,now);a.sniper[0]=.5f;a.updated_at=now;
        uint32_t released[3];unsigned n;
        assert(!SudekiMpStoryThreatAlertFade(&c,STORY_TARGET_TIER15,1,0,&a,v,3,STORY_SEAT_HOST,1,now+5999,released,&n));
        assert(SudekiMpStoryThreatAlertFade(&c,STORY_TARGET_TIER15,1,0,&a,v,3,STORY_SEAT_HOST,1,now+6000,released,&n)==STORY_END_EXPIRED);
        a.sniper[0]=1;a.updated_at=now+16000;
        assert(!SudekiMpStoryThreatAlertRaise(&c,STORY_TARGET_TIER15,1,1,0,0,&a,0,STORY_SEAT_HOST,1,0,now+15999));
        assert(SudekiMpStoryThreatAlertRaise(&c,STORY_TARGET_TIER15,1,1,0,0,&a,0,STORY_SEAT_HOST,1,0,now+16000));
        assert(!a.alerting[0].cooldown_active);
        SudekiMpStoryThreatPushState p={0};SudekiMpStoryThreatPushTrack(0,&p,1,0,now);
        assert(SudekiMpStoryThreatPushEvaluate(&c,STORY_TARGET_TIER2,1,0,&p,STORY_SEAT_HOST,1,now+5000));
        SudekiMpStoryThreatPushStart(&c,0,&p,7,now+5000,0);
        assert(SudekiMpStoryThreatPushEnd(&c,STORY_TARGET_TIER2,1,0,&p,STORY_SEAT_HOST,1,0,1,STORY_LIST_UNKNOWN,now+13000)==STORY_END_EXPIRED);
        assert(!SudekiMpStoryThreatPushEvaluate(&c,STORY_TARGET_TIER2,1,0,&p,STORY_SEAT_HOST,1,now+19999));
        assert(SudekiMpStoryThreatPushEvaluate(&c,STORY_TARGET_TIER2,1,0,&p,STORY_SEAT_HOST,1,now+20000));
    }
}

int main(void) {
    seats();config_tests();caps();lead_tests();defend_tests();memory_tests();predicates_and_pick_tests();
    alert_selection_tests();alert_lifetime_tests();push_and_decide_tests();clock_wrap_tests();
    puts("story tier0/threat/alert/push/Defend policy tests passed");return 0;
}
