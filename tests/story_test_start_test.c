/* Synthetic test-start state/ABI fixture. Native observation, zone loading,
 * time, resource access, and image identity are controlled seams. Only the exact
 * PositionSet comparison prologue executes; both continuations enter a fastcall spy.
 * No native TEMP, game process, save, or live actor is touched. */
#include <windows.h>
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static DWORD test_now;
static DWORD test_tick_count(void) {return test_now;}
#define GetTickCount test_tick_count
#include "../src/hooks/lan_story_test_start.c"
#undef GetTickCount

static uint8_t *test_image;
static uint8_t test_actors[4][0x48],test_positions[4][0x24],test_world[4],test_group[4];
static SudekiMpLanStoryNativeRoster test_roster;
static BOOL test_exact,test_settled,test_identity,test_split_ready;
static BOOL test_health_enabled,test_resource_known[4],test_resource_set_success[4];
static int test_zone_state,test_enter_result;
static unsigned test_enter_calls,test_entered,test_go_to_calls,test_placed[4];
static unsigned test_zone_calls,test_settled_calls,test_split_calls;
static unsigned test_health_reads[4],test_health_writes[4];
static float test_hp[4],test_sp[4];
static void *test_native_actors[4];
static int test_replace_actor_on_read;
static unsigned test_timeout_logs,test_refused_logs;
static const float exterior_leader[3]={-131.0f,7.12f,-58.10f};
static const float exterior_follower[3]={-133.27f,7.12f,-58.10f};
static const float interior_leader[3]={-14.39f,-0.72f,-0.49f};

BOOL SudekiMpCheckLoadedExecutable(HMODULE image) {return test_identity && image==(HMODULE)test_image;}
void SudekiMpLogWrite(const char *text) {(void)text;}
void SudekiMpLogFormat(const char *format,...) {
    if(strstr(format,"event=temporary_timeout")) ++test_timeout_logs;
    if(strstr(format,"event=temporary_refused")) ++test_refused_logs;
}
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *roster) {
    assert(roster==&test_roster);
    return test_exact;
}
int SudekiMpLanStoryAreaZoneState(const char *zone) {
    assert(!strcmp(zone,"Illumina_Countryside_NW"));
    ++test_zone_calls;
    return test_zone_state;
}
int SudekiMpLanStoryAreaGoTo(const char *zone,uint32_t now) {
    assert(!strcmp(zone,"Illumina_Countryside_NW") && now==test_now);
    ++test_go_to_calls;
    return 1;
}
int SudekiMpLanStoryObserverTestEnterTemporary(const char *zone,const char *marker) {
    assert(!strcmp(zone,"Test_Farmhouse") && !strcmp(marker,"Test_Arrival"));
    ++test_enter_calls;
    if(test_enter_result>0) ++test_entered;
    return test_enter_result;
}
BOOL SudekiMpLanStoryObserverTestTemporarySettled(const char *zone) {
    assert(!strcmp(zone,"Test_Farmhouse"));
    ++test_settled_calls;
    return test_settled;
}
BOOL SudekiMpLanStorySplitTestCanEnterTemporary(const char *zone,unsigned leader) {
    assert(!strcmp(zone,"Test_Farmhouse") && leader==test_roster.leader_character);
    ++test_split_calls;
    return test_split_ready;
}
static unsigned actor_character(SudekiMpCleanroomActor actor) {
    static const SudekiMpCleanroomActor types[4]={SUDEKIMP_CLEANROOM_BUKI,SUDEKIMP_CLEANROOM_ELCO,
        SUDEKIMP_CLEANROOM_TAL,SUDEKIMP_CLEANROOM_AILISH};
    unsigned c;
    for(c=0;c<4 && types[c]!=actor;++c) {}
    assert(c<4);
    return c;
}
static unsigned resource_character(SudekiMpCleanroomActor actor) {
    unsigned c=actor_character(actor);
    assert(test_health_enabled && test_exact && (test_roster.available_mask&(1u<<c)));
    return c;
}
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor actor) {
    return test_native_actors[actor_character(actor)];
}
BOOL SudekiMpCleanroomEngineActorResources(SudekiMpCleanroomActor actor,float *hp,float *sp) {
    unsigned c=resource_character(actor);
    assert(hp && sp);++test_health_reads[c];
    if(!test_resource_known[c]) return FALSE;
    *hp=test_hp[c];*sp=test_sp[c];
    if(test_replace_actor_on_read==(int)c) test_native_actors[c]=test_world;
    return TRUE;
}
BOOL SudekiMpCleanroomEngineSetActorResources(SudekiMpCleanroomActor actor,float hp,float sp) {
    unsigned c=resource_character(actor);
    assert(test_health_reads[c] && isfinite(hp) && hp>=1.0f && sp==test_sp[c]);
    ++test_health_writes[c];test_hp[c]=hp;test_sp[c]=sp;
    return test_resource_set_success[c];
}
static void __attribute__((fastcall,noinline,force_align_arg_pointer)) position_spy(void *position,const float *xyz) {
    unsigned character;
    for(character=0;character<4;++character) if(position==test_positions[character]) break;
    assert(character<4 && xyz && isfinite(xyz[0]) && isfinite(xyz[1]) && isfinite(xyz[2]));
    ++test_placed[character];
    memcpy(test_positions[character]+0x18,xyz,3*sizeof(float));
    /* An interior placement of any follower fails at the mutation seam. */
    if(character!=test_roster.leader_character)
        assert(!memcmp(xyz,exterior_follower,sizeof(exterior_follower)));
}
static void branch_to_spy(unsigned offset) {
    uint8_t *site=test_image+RVA_POSITION_SET+offset;
    int32_t relative=(int32_t)((uintptr_t)position_spy-(uintptr_t)(site+5));
    site[0]=0xe9;memcpy(site+1,&relative,4);
}
static void reset_fixture(void) {
    memset(test_actors,0,sizeof(test_actors));memset(test_positions,0,sizeof(test_positions));
    memset(test_placed,0,sizeof(test_placed));memset(&test_roster,0,sizeof(test_roster));
    memset(test_health_reads,0,sizeof(test_health_reads));memset(test_health_writes,0,sizeof(test_health_writes));
    test_roster.dispatch_serial=1;test_roster.epoch=7;test_roster.revision=4;
    test_roster.available_mask=0x0c;test_roster.leader_character=3;
    test_roster.world=test_world;test_roster.group=test_group;
    for(unsigned c=0;c<4;++c) {
        test_hp[c]=101.0f+(float)c;test_sp[c]=21.0f+(float)c;test_resource_known[c]=TRUE;
        test_resource_set_success[c]=TRUE;
        test_native_actors[c]=test_actors[c];
        *(void **)(test_actors[c]+0x44)=test_positions[c];
        *(void **)test_positions[c]=test_image+RVA_POSITION_VTABLE;
        if(test_roster.available_mask&(1u<<c)) test_roster.actors[c]=test_actors[c];
    }
    test_now=100;test_exact=test_identity=test_split_ready=TRUE;test_settled=FALSE;
    test_zone_state=3;test_enter_result=0;
    test_health_enabled=FALSE;test_replace_actor_on_read=-1;
    test_zone_calls=test_settled_calls=test_split_calls=0;
    test_enter_calls=test_entered=test_go_to_calls=test_timeout_logs=test_refused_logs=0;
    SudekiMpLanStoryTestStartSetZone("Illumina_Countryside_NW");
    SudekiMpLanStoryTestStartSetLeaderHealth(0);
    SudekiMpLanStoryTestStartSetFollowerHealth(0);
    assert(SudekiMpLanStoryTestStartSetTemporary("Test_Farmhouse","Test_Arrival",interior_leader));
    assert(SudekiMpLanStoryTestStartConfigure((HMODULE)test_image,exterior_leader,exterior_follower));
}
static void service_after(DWORD delay) {
    test_now+=delay;
    SudekiMpLanStoryTestStartService(&test_roster);
}
static void exterior_once(void) {
    SudekiMpLanStoryTestStartService(&test_roster);
    assert(!test_placed[2] && !test_placed[3]);
    service_after(1600);
    assert(test_placed[2]==1 && test_placed[3]==1 && !test_enter_calls);
    assert(!memcmp(test_positions[2]+0x18,exterior_follower,sizeof(exterior_follower)));
    assert(!memcmp(test_positions[3]+0x18,exterior_leader,sizeof(exterior_leader)));
    assert(pending && temporary_phase==1);
}
static void enter_once(void) {
    test_enter_result=1;service_after(1600);
    assert(test_entered==1 && temporary_phase==2 && pending);
    assert(test_placed[2]==1 && test_placed[3]==1);
}
static void settled_once(void) {
    test_settled=TRUE;service_after(1);
    assert(!pending && test_placed[2]==1 && test_placed[3]==2);
    assert(!memcmp(test_positions[2]+0x18,exterior_follower,sizeof(exterior_follower)));
    assert(!memcmp(test_positions[3]+0x18,interior_leader,sizeof(interior_leader)));
    service_after(100000);service_after(1);
    assert(test_entered==1 && test_placed[2]==1 && test_placed[3]==2);
}
static void readiness(void) {
    reset_fixture();test_split_ready=FALSE;
    SudekiMpLanStoryTestStartService(&test_roster);service_after(1600);
    assert(!test_placed[2] && !test_placed[3] && !test_enter_calls);
    test_split_ready=TRUE;test_exact=FALSE;service_after(1600);
    assert(!test_placed[2] && !test_placed[3] && !test_enter_calls);
    test_exact=TRUE;service_after(1);
    assert(test_placed[2]==1 && test_placed[3]==1 && !test_enter_calls);
    puts("PASS initial placement waits for remote follower admission and exact roster");

    reset_fixture();exterior_once();
    for(int state=-1;state<3;++state) {
        test_zone_state=state;service_after(1600);
        assert(!test_enter_calls && test_placed[2]==1 && test_placed[3]==1);
    }
    test_zone_state=3;test_exact=FALSE;service_after(1600);
    assert(!test_enter_calls && test_placed[2]==1 && test_placed[3]==1);
    test_exact=TRUE;service_after(1);service_after(1);
    assert(test_enter_calls==2 && !test_entered && temporary_phase==1);
    enter_once();service_after(5000);service_after(5000);
    assert(test_entered==1 && test_placed[2]==1 && test_placed[3]==1 && pending);
    test_exact=FALSE;test_settled=TRUE;service_after(1);
    assert(test_placed[2]==1 && test_placed[3]==1);
    test_exact=TRUE;settled_once();
    puts("PASS unknown/unready exterior, split admission wait, and unsettled TEMP never place inside");
    puts("PASS native entry once; only leader placed inside after exact settlement; follower unchanged");
}
static void roster_changes(void) {
    reset_fixture();exterior_once();
    SudekiMpLanStoryNativeRoster original=test_roster;
    for(unsigned mutation=0;mutation<5;++mutation) {
        test_roster=original;
        switch(mutation) {
        case 0:test_roster.world=test_group;break;
        case 1:test_roster.group=test_world;break;
        case 2:test_roster.leader_character=2;break;
        case 3:test_roster.available_mask=8;break;
        default:test_roster.actors[2]=test_actors[0];break;
        }
        service_after(1600);
        assert(!test_enter_calls && test_placed[2]==1 && test_placed[3]==1);
    }
    /* The initial exterior position change may advance its ordinary epoch;
     * the TEMP must preserve the epoch actually admitted at dispatch. */
    test_roster=original;++test_roster.epoch;original=test_roster;enter_once();
    ++test_roster.epoch;test_settled=TRUE;service_after(1);
    assert(pending && test_placed[2]==1 && test_placed[3]==1);
    test_roster=original;
    test_roster.actors[3]=test_actors[0];test_settled=TRUE;service_after(1);
    assert(pending && test_placed[2]==1 && test_placed[3]==1);
    test_roster=original;settled_once();
    puts("PASS changed world/group/leader/mask/actor/epoch cannot reuse old placement admission");
}
static void refusal_and_timeout(void) {
    reset_fixture();exterior_once();test_enter_result=-1;service_after(1600);
    assert(!pending && test_refused_logs==1 && !test_entered);
    test_enter_result=1;test_settled=TRUE;service_after(100000);
    assert(test_enter_calls==1 && test_placed[2]==1 && test_placed[3]==1);

    reset_fixture();exterior_once();enter_once();test_exact=FALSE;service_after(60001);
    assert(!pending && test_timeout_logs==1 && test_entered==1);
    test_exact=TRUE;test_settled=TRUE;service_after(1);
    assert(test_entered==1 && test_placed[2]==1 && test_placed[3]==1);

    reset_fixture();exterior_once();test_roster.world=test_group;service_after(60001);
    assert(!pending && test_timeout_logs==1 && !test_entered);
    puts("PASS refusal and deadline stop only test aid, including stale roster, without late placement");
}
static void configuration_guards(void) {
    reset_fixture();
    for(unsigned i=0;i<sizeof(setter_entry);++i) {
        test_image[RVA_POSITION_SET+i]^=1;
        assert(!SudekiMpLanStoryTestStartConfigure((HMODULE)test_image,exterior_leader,exterior_follower));
        test_image[RVA_POSITION_SET+i]^=1;
    }
    test_identity=FALSE;
    assert(!SudekiMpLanStoryTestStartConfigure((HMODULE)test_image,exterior_leader,exterior_follower));
    test_identity=TRUE;
    float invalid[3]={NAN,0,0};
    assert(!SudekiMpLanStoryTestStartSetTemporary("Test_Farmhouse","Test_Arrival",invalid));
    assert(!temporary_zone[0]);
    reset_fixture();assert(!SudekiMpLanStoryTestStartSetTemporary(NULL,NULL,NULL));
    SudekiMpLanStoryTestStartService(&test_roster);service_after(1600);
    assert(!pending && !temporary_phase && test_placed[2]==1 && test_placed[3]==1);
    service_after(100000);
    assert(!test_enter_calls && test_placed[2]==1 && test_placed[3]==1);
    puts("PASS setter byte/image guards, invalid TEMP config, and unchanged exterior-only start");
}
static void no_movement_calls(void) {
    for(unsigned c=0;c<4;++c) assert(!test_placed[c]);
    assert(!test_zone_calls && !test_go_to_calls && !test_split_calls &&
        !test_enter_calls && !test_settled_calls);
}
static void no_health_calls(void) {
    for(unsigned c=0;c<4;++c) assert(!test_health_reads[c] && !test_health_writes[c]);
}
static void configure_health_only(float leader,float follower) {
    test_health_enabled=TRUE;
    SudekiMpLanStoryTestStartSetLeaderHealth(leader);
    SudekiMpLanStoryTestStartSetFollowerHealth(follower);
    assert(SudekiMpLanStoryTestStartConfigureHealthOnly((HMODULE)test_image));
    /* reset_fixture deliberately left both old placement stages configured. */
    assert(!start_zone[0] && !temporary_zone[0] && !temporary_marker[0] && !temporary_phase);
}
static void health_only_once(void) {
    reset_fixture();configure_health_only(.5f,.25f);
    test_split_ready=FALSE;test_zone_state=-1;
    /* Health-only needs neither valid position components nor movement admission. */
    for(unsigned c=0;c<4;++c) *(void **)(test_actors[c]+0x44)=NULL;
    SudekiMpLanStoryTestStartService(&test_roster);
    service_after(1499);no_health_calls();no_movement_calls();
    service_after(1);
    assert(!pending && test_health_writes[2]==1 && test_health_writes[3]==1);
    assert(test_health_reads[2]>=2 && test_health_reads[3]>=2);
    assert(test_hp[2]==25.0f && test_hp[3]==52.0f && test_sp[2]==23.0f && test_sp[3]==24.0f);
    assert(!test_health_reads[0] && !test_health_reads[1] &&
        !test_health_writes[0] && !test_health_writes[1]);
    unsigned reads[4];memcpy(reads,test_health_reads,sizeof(reads));
    service_after(100000);service_after(1);
    assert(!memcmp(reads,test_health_reads,sizeof(reads)) &&
        test_health_writes[2]==1 && test_health_writes[3]==1);
    no_movement_calls();
    puts("PASS health-only lowers sparse party HP once, preserves SP, and never calls position/zone/TEMP seams");
}
static void health_only_readiness(void) {
    reset_fixture();configure_health_only(.5f,0.0f);
    SudekiMpLanStoryTestStartService(NULL);
    test_roster.leader_character=4;service_after(1600);
    test_roster.leader_character=3;test_roster.available_mask=4;service_after(1600);
    no_health_calls();no_movement_calls();
    test_roster.available_mask=12;
    SudekiMpLanStoryTestStartService(&test_roster);
    test_exact=FALSE;service_after(1600);
    assert(pending);no_health_calls();no_movement_calls();
    test_exact=TRUE;service_after(1);
    assert(!pending && test_health_writes[3]==1 && !test_health_reads[2]);
    no_movement_calls();

    reset_fixture();configure_health_only(.5f,0.0f);
    test_native_actors[3]=test_actors[2];
    SudekiMpLanStoryTestStartService(&test_roster);service_after(1600);
    assert(!pending);no_health_calls();no_movement_calls();
    test_native_actors[3]=test_actors[3];service_after(1600);
    no_health_calls();

    reset_fixture();configure_health_only(.5f,0.0f);test_replace_actor_on_read=3;
    SudekiMpLanStoryTestStartService(&test_roster);service_after(1600);
    assert(!pending && test_health_reads[3]==1 && !test_health_writes[3] && test_hp[3]==104.0f);
    no_movement_calls();
    test_replace_actor_on_read=-1;test_native_actors[3]=test_actors[3];service_after(1600);
    assert(test_health_reads[3]==1 && !test_health_writes[3]);

    reset_fixture();configure_health_only(.5f,0.0f);test_resource_set_success[3]=FALSE;
    SudekiMpLanStoryTestStartService(&test_roster);service_after(1600);
    assert(!pending && test_hp[3]==52.0f && test_health_writes[3]==1 && test_health_reads[3]==1);
    test_resource_set_success[3]=TRUE;service_after(1600);
    assert(test_hp[3]==52.0f && test_health_writes[3]==1 && test_health_reads[3]==1);
    no_movement_calls();
    puts("PASS health-only waits for ready exact roster and refuses mismatched/replaced actor identity without retry");
    puts("PASS uncertain native resource setter result never reapplies the health fraction");
}
static void health_only_configuration_guards(void) {
    static const float invalid[]={0.0f,-.5f,1.0f,2.0f,NAN,INFINITY};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        reset_fixture();test_health_enabled=TRUE;
        SudekiMpLanStoryTestStartSetLeaderHealth(invalid[i]);
        SudekiMpLanStoryTestStartSetFollowerHealth(invalid[i]);
        assert(!SudekiMpLanStoryTestStartConfigureHealthOnly((HMODULE)test_image));
        SudekiMpLanStoryTestStartService(&test_roster);service_after(1600);
        assert(!pending);no_health_calls();no_movement_calls();
    }
    reset_fixture();test_health_enabled=TRUE;
    SudekiMpLanStoryTestStartSetLeaderHealth(.5f);test_identity=FALSE;
    assert(!SudekiMpLanStoryTestStartConfigureHealthOnly((HMODULE)test_image));
    test_identity=TRUE;SudekiMpLanStoryTestStartService(&test_roster);service_after(1600);
    assert(!pending);no_health_calls();no_movement_calls();

    reset_fixture();
    /* An unused position setter is not a dependency of health-only configuration. */
    test_image[RVA_POSITION_SET]^=1;
    configure_health_only(0.0f,.1f);
    test_image[RVA_POSITION_SET]^=1;
    test_hp[2]=2.0f;
    SudekiMpLanStoryTestStartService(&test_roster);service_after(1600);
    assert(!pending && test_health_writes[2]==1 && test_hp[2]==1.0f && !test_health_reads[3]);
    no_movement_calls();

    reset_fixture();configure_health_only(.5f,.5f);
    test_resource_known[3]=FALSE;test_hp[2]=1.0f;
    SudekiMpLanStoryTestStartService(&test_roster);service_after(1600);
    assert(!pending && test_health_reads[2]==1 && test_health_reads[3]==1 &&
        !test_health_writes[2] && !test_health_writes[3]);
    service_after(1600);
    assert(test_health_reads[2]==1 && test_health_reads[3]==1);
    no_movement_calls();
    puts("PASS invalid health/image configuration disarms prior placement; unknown resources and minimum HP are not mutated");
}
int main(void) {
    test_image=VirtualAlloc(NULL,0x300000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(test_image);
    static const uint8_t expected[14]={0xd9,0x41,0x18,0xd9,0x02,0xda,0xe9,0xdf,0xe0,0xf6,0xc4,0x44,0x7a,0x1e};
    memcpy(test_image+RVA_POSITION_SET,expected,sizeof(expected));
    branch_to_spy(14);branch_to_spy(44);
    assert(FlushInstructionCache(GetCurrentProcess(),test_image,0x300000));
    readiness();roster_changes();refusal_and_timeout();configuration_guards();
    health_only_once();health_only_readiness();health_only_configuration_guards();
    assert(VirtualFree(test_image,0,MEM_RELEASE));
    puts("PASS synthetic test-start state and PositionSet fastcall seam");
    return 0;
}
