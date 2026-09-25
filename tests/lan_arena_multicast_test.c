#include "network/lan_arena_shared_simulation.h"
#include "network/lan_arena_replica.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static unsigned int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); ++failures; } } while (0)

static SudekiMpLanArenaSnapshot two_casts(unsigned int first_kind,unsigned int second_kind) {
    SudekiMpLanArenaSnapshot s={0};
    s.sequence=1; s.host_tick=100; s.match_state=SUDEKIMP_LAN_ARENA_MATCH_ACTIVE; s.combat_enabled=1;
    for(unsigned int i=0;i<2;++i) {
        SudekiMpLanArenaActorSnapshot *actor=&s.seat[i];
        SudekiMpLanArenaCastPresentation *cast=&s.cast[i];
        actor->actor_type=i ? SUDEKIMP_LAN_ARENA_ELCO_TYPE:SUDEKIMP_LAN_ARENA_BUKI_TYPE;
        actor->native_entity_id=actor->actor_type;
        actor->facing_z=1.f; actor->hp=100; actor->sp=80;
        actor->skill_sequence=7; /* Deliberately equal: not a global cast ID. */
        actor->skill_kind=(uint8_t)(i ? second_kind:first_kind); actor->skill_active=1;
        actor->skill_presentation_valid=1; actor->skill_presentation_channel_count=i ? 5:4;
        actor->skill_presentation_selector[0]=i ? 73:75;
        actor->skill_presentation_state[0]=1; actor->skill_presentation_rate[0]=24.f;
        for(unsigned int c=1;c<actor->skill_presentation_channel_count;++c)
            actor->skill_presentation_state[c]=192;
        cast->skill_fade=(SudekiMpLanArenaSkillFade){7,(uint8_t)i,actor->skill_kind,{.3f,.3f,.4f}};
        if(actor->skill_kind==SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT) {
            SudekiMpLanArenaSpiritView *view=&cast->spirit_view;
            view->kind=1; view->owner_seat=(uint8_t)i; view->skill_sequence=7;
            view->matrix[0]=-1.f; view->matrix[5]=view->matrix[10]=view->matrix[15]=1.f;
            view->matrix[12]=i ? 10.f:-10.f;
            view->projection[0]=1.f; view->projection[1]=.1f; view->projection[2]=1000.f;
            s.spirit_audio_history[s.spirit_audio_history_count++]=
                (SudekiMpLanArenaSpiritAudioSemanticEvent){(uint16_t)(i+1),7,1,(uint8_t)i};
        }
    }
    return s;
}

static int commit(SudekiMpLanArenaSharedSimulation *host,const SudekiMpLanArenaSnapshot *s) {
    SudekiMpLanArenaNativeWorldObservation world={0};
    SudekiMpLanArenaActorObservation actors[2]={{s->seat[0],1},{s->seat[1],1}};
    world.host_tick=s->host_tick; world.match_state=s->match_state; world.combat_enabled=s->combat_enabled;
    world.tal_hp=s->seat[0].hp; world.tal_sp=s->seat[0].sp;
    world.ailish_hp=s->seat[1].hp; world.ailish_sp=s->seat[1].sp;
    world.native_combat_observed=world.native_resources_observed=world.native_enemies_observed=1;
    memcpy(world.cast,s->cast,sizeof(world.cast));
    world.spirit_audio_history_count=s->spirit_audio_history_count;
    memcpy(world.spirit_audio_history,s->spirit_audio_history,sizeof(world.spirit_audio_history));
    world.spirit_vfx_observed=s->spirit_vfx_observed; world.spirit_vfx_count=s->spirit_vfx_count;
    memcpy(world.spirit_vfx,s->spirit_vfx,sizeof(world.spirit_vfx));
    return SudekiMpLanArenaSharedSimulationCommitNativeFrame(host,900,&world,&actors[0],&actors[1]);
}

static void clear_presentation(SudekiMpLanArenaActorSnapshot *actor) {
    actor->skill_active=actor->skill_presentation_valid=actor->skill_presentation_channel_count=0;
    memset(actor->skill_presentation_selector,0,sizeof(actor->skill_presentation_selector));
    memset(actor->skill_presentation_state,0,sizeof(actor->skill_presentation_state));
    memset(actor->skill_presentation_rate,0,sizeof(actor->skill_presentation_rate));
    memset(actor->skill_presentation_time,0,sizeof(actor->skill_presentation_time));
    memset(actor->skill_presentation_blend,0,sizeof(actor->skill_presentation_blend));
}

static void test_network_and_independent_retirement(void) {
    for(unsigned int a=1;a<=2;++a) for(unsigned int b=1;b<=2;++b) for(unsigned int retiring=0;retiring<2;++retiring) {
        SudekiMpLanArenaSharedSimulation host={0},client={0};
        SudekiMpLanArenaReplica timeline={0};
        SudekiMpLanArenaSnapshot s=two_casts(a,b),sample,retired,invalid;
        SudekiMpLanArenaPacket packet={0},decoded;
        uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE]; size_t size=0;
        CHECK(SudekiMpLanArenaSharedSimulationBegin(&host,SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD,900));
        CHECK(SudekiMpLanArenaSharedSimulationBegin(&client,SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA,900));
        CHECK(commit(&host,&s));
        packet.type=SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT; packet.sequence=1; packet.session_token=900;
        packet.body.snapshot=host.frame; packet.body.snapshot.sequence=1;
        CHECK(SudekiMpLanArenaEncodePacket(bytes,&size,&packet));
        CHECK(size==1132u && size<=1472u); /* IPv4/UDP on a 1500-byte path. */
        CHECK(SudekiMpLanArenaDecodePacket(bytes,size,&decoded));
        CHECK(!memcmp(s.cast,decoded.body.snapshot.cast,sizeof(s.cast)));
        CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(&client,901,&decoded.body.snapshot));
        CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(&client,900,&decoded.body.snapshot));
        CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(&client,900,&decoded.body.snapshot));
        CHECK(SudekiMpLanArenaReplicaPush(&timeline,&decoded.body.snapshot));
        bytes[4]=35; CHECK(!SudekiMpLanArenaDecodePacket(bytes,size,&decoded));
        invalid=s; invalid.cast[retiring].skill_fade.owner_seat=(uint8_t)(retiring^1);
        CHECK(!SudekiMpLanArenaSnapshotValid(&invalid));
        invalid=s; ++invalid.cast[retiring].skill_fade.skill_sequence;
        CHECK(!SudekiMpLanArenaSnapshotValid(&invalid));
        invalid=s; invalid.cast[retiring].skill_fade.rgb[0]=NAN;
        CHECK(!SudekiMpLanArenaSnapshotValid(&invalid));

        retired=s; retired.sequence=2; retired.host_tick=150;
        clear_presentation(&retired.seat[retiring]);
        memset(&retired.cast[retiring].spirit_view,0,sizeof(retired.cast[retiring].spirit_view));
        for(unsigned int c=0;c<3;++c) retired.cast[retiring].skill_fade.rgb[c]=1.f;
        /* Completion retains that owner's natural fade tail, not its camera. */
        CHECK(commit(&host,&retired));
        CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(&client,900,&retired));
        CHECK(SudekiMpLanArenaReplicaPush(&timeline,&retired));
        CHECK(SudekiMpLanArenaReplicaSample(&timeline,150,&sample));
        CHECK(!sample.seat[retiring].skill_active && sample.seat[retiring^1].skill_active);
        CHECK(!memcmp(&sample.cast[retiring^1],&s.cast[retiring^1],sizeof(s.cast[0])));
        CHECK(sample.cast[retiring].skill_fade.rgb[0]==1.f);
        CHECK(SudekiMpLanArenaReplicaSample(&timeline,125,&sample));
        CHECK(sample.seat[retiring].skill_active); /* No early cleanup on receipt. */
        CHECK(fabsf(sample.cast[retiring].skill_fade.rgb[0]-.65f)<.001f);
        CHECK(fabsf(sample.cast[retiring^1].skill_fade.rgb[0]-.3f)<.001f);
        invalid=retired; invalid.host_tick=200; invalid.seat[retiring]=s.seat[retiring];
        CHECK(!commit(&host,&invalid)); /* Same sequence cannot resurrect. */
        retired.host_tick=200; retired.sequence=3;
        memset(&retired.cast[retiring],0,sizeof(retired.cast[retiring]));
        CHECK(commit(&host,&retired));
        CHECK(SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(&client,900,&retired));
        CHECK(client.frame.seat[retiring^1].skill_active);
        SudekiMpLanArenaReplicaReset(&timeline);
        CHECK(!SudekiMpLanArenaReplicaSample(&timeline,200,&sample));
        CHECK(SudekiMpLanArenaSharedSimulationBegin(&client,SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA,901));
        CHECK(!SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(&client,900,&retired));
    }
}

typedef struct AudioSink { unsigned int calls, successes, fail_on; } AudioSink;
static int sound(void *context,SudekiMpLanArenaSpiritAudioCue cue) {
    AudioSink *sink=context;
    CHECK(cue==SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START);
    if(++sink->calls==sink->fail_on) return 0;
    ++sink->successes; return 1;
}
static void test_both_audio_events_and_partial_retry(void) {
    SudekiMpLanArenaSnapshot s=two_casts(2,2);
    SudekiMpLanArenaSpiritAudioCursor cursor={0};
    AudioSink sink={0,0,2}; unsigned int replayed=0;
    CHECK(!SudekiMpLanArenaSpiritAudioConsumeSnapshot(&cursor,&s,sound,&sink,&replayed));
    CHECK(replayed==1 && sink.successes==1 && cursor.last_event_sequence==1);
    CHECK(SudekiMpLanArenaSpiritAudioConsumeSnapshot(&cursor,&s,sound,&sink,&replayed));
    CHECK(replayed==1 && sink.successes==2 && cursor.last_event_sequence==2);
    CHECK(SudekiMpLanArenaSpiritAudioConsumeSnapshot(&cursor,&s,sound,&sink,&replayed));
    CHECK(replayed==0 && sink.successes==2);
    SudekiMpLanArenaSpiritAudioCursorReset(&cursor);
    s.seat[0].skill_active=0;
    CHECK(SudekiMpLanArenaSpiritAudioConsumeSnapshot(&cursor,&s,sound,&sink,&replayed));
    CHECK(replayed==1 && sink.successes==3); /* Finished peer is not replayed. */
    SudekiMpLanArenaSharedSimulation host={0};
    CHECK(SudekiMpLanArenaSharedSimulationBegin(&host,SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD,900));
    s=two_casts(2,2); s.spirit_audio_history[0].skill_sequence=6;
    CHECK(SudekiMpLanArenaSnapshotValid(&s));
    CHECK(!commit(&host,&s)); /* A good latest event cannot hide a bad first one. */
}

int main(void) {
    SudekiMpLanArenaSetSeatTypes(SUDEKIMP_LAN_ARENA_BUKI_TYPE,SUDEKIMP_LAN_ARENA_ELCO_TYPE);
    test_network_and_independent_retirement();
    test_both_audio_events_and_partial_retry();
    if(failures) return 1;
    puts("multi-cast packet, authority, interpolation, retirement and audio tests passed");
    return 0;
}
