#include "network/lan_party_session.h"
#include "network/lan_party_replica.h"
#include "network/lan_party_motion.h"
#include "hooks/lan_party_presence.h"
#include <math.h>
#include <winsock2.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static SudekiMpLanPartySession *nodes[4];
static uint32_t now;
static SudekiMpLanPartyConfig config;

static void pump(void) {
    /* Real nonblocking UDP, with every endpoint independently polled. */
    now = GetTickCount();
    for (unsigned pass = 0; pass < 3; ++pass)
        for (unsigned i = 0; i < 4; ++i) SudekiMpLanPartyPoll(nodes[i], now);
}
static SudekiMpLanPartyPeerStatus status(unsigned node, unsigned seat) {
    SudekiMpLanPartyPeerStatus result;
    memset(&result, 0, sizeof(result));
    CHECK(SudekiMpLanPartyPeerStatusGet(nodes[node], seat, &result));
    return result;
}
static void wait_transport(unsigned seat) {
    DWORD started=GetTickCount();
    while(!status(0,seat).transport_confirmed && GetTickCount()-started<1500u) {
        Sleep(5); pump();
    }
    CHECK(status(0,seat).transport_confirmed);
}
static void fill_frame(SudekiMpLanPartyFrame *frame) {
    memset(frame, 0, sizeof(*frame));
    for (unsigned c = 0; c < 2; ++c) {
        frame->chunk[c].host_tick = 1000;
        frame->chunk[c].match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
        for (unsigned a = 0; a < 2; ++a) {
            SudekiMpLanArenaActorSnapshot *actor = &frame->chunk[c].seat[a];
            actor->actor_type = SudekiMpLanPartyActorType(c * 2 + a);
            actor->native_entity_id = actor->actor_type;
            actor->facing_z = 1; actor->hp = 100; actor->sp = 50;
            actor->x = (float)(c * 2 + a);
        }
    }
}
static void setup(void) {
    memset(&config, 0, sizeof(config));
    memset(config.game_hash, 0x13, sizeof(config.game_hash));
    config.timeout_ms = 5000;
    nodes[0] = SudekiMpLanPartyCreate(&config);
    CHECK(nodes[0] != NULL);
    config.port = SudekiMpLanPartyPort(nodes[0]); config.host_ipv4 = "127.0.0.1";
    for (unsigned i = 1; i < 4; ++i) {
        config.local_seat = (uint8_t)i;
        nodes[i] = SudekiMpLanPartyCreate(&config); CHECK(nodes[i] != NULL);
    }
    now = GetTickCount(); pump();
}
static void test_context_isolation(void) {
    SudekiMpLanPartyFrame frame; fill_frame(&frame);
    SudekiMpLanArenaCodecRoster pair = {{SUDEKIMP_LAN_ARENA_BUKI_TYPE, SUDEKIMP_LAN_ARENA_ELCO_TYPE},0};
    SudekiMpLanArenaCodecRoster other = {{SUDEKIMP_LAN_ARENA_TAL_TYPE, SUDEKIMP_LAN_ARENA_AILISH_TYPE},0};
    SudekiMpLanArenaPacket packet, decoded; memset(&packet, 0, sizeof(packet));
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE]; size_t size = 0;
    packet.type = SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT; packet.session_token = 10;
    packet.body.snapshot = frame.chunk[0];
    CHECK(SudekiMpLanArenaEncodeForRoster(bytes, &size, &packet, &pair));
    CHECK(!SudekiMpLanArenaDecodeForRoster(bytes, size, &decoded, &other));
    CHECK(SudekiMpLanArenaDecodeForRoster(bytes, size, &decoded, &pair));
    uint8_t host, client; CHECK(SudekiMpLanArenaSeatActorTypes(&host, &client));
    CHECK(host == SUDEKIMP_LAN_ARENA_TAL_TYPE && client == SUDEKIMP_LAN_ARENA_AILISH_TYPE);
    CHECK(!SudekiMpLanArenaSnapshotValid(&frame.chunk[0]));
    CHECK(SudekiMpLanArenaSnapshotValid(&frame.chunk[1]));
    pair.actor_type[1] = pair.actor_type[0];
    CHECK(!SudekiMpLanArenaEncodeForRoster(bytes, &size, &packet, &pair));
    CHECK(!SudekiMpLanArenaEncodeForRoster(bytes, &size, &packet, NULL));
    CHECK(!SudekiMpLanArenaDecodeForRoster(bytes, size, &decoded, NULL));
    CHECK(SudekiMpLanPartyFrameValid(&frame));
    frame.chunk[1].host_tick++;
    CHECK(!SudekiMpLanPartyFrameValid(&frame));
}
static void push_replica_frame(SudekiMpLanPartyReplica *r,
    const SudekiMpLanPartyLease *lease, SudekiMpLanPartyFrame *frame,
    unsigned sequence, unsigned tick) {
    for (unsigned c=0;c<2;++c) {
        frame->chunk[c].sequence=sequence; frame->chunk[c].host_tick=tick;
        for (unsigned a=0;a<2;++a) frame->chunk[c].seat[a].x=(float)tick+(float)(c*2+a)*1000;
    }
    CHECK(SudekiMpLanPartyReplicaPush(r,lease,frame));
}
static void test_replica_clock_and_atomicity(void) {
    SudekiMpLanPartyReplica r, before;
    SudekiMpLanPartyFrame frame, sample;
    SudekiMpLanPartyLease lease={123,1,3}, stale=lease;
    uint32_t tick=0;
    SudekiMpLanPartyReplicaReset(&r); fill_frame(&frame);
    push_replica_frame(&r,&lease,&frame,1,1000);
    push_replica_frame(&r,&lease,&frame,2,1050);
    CHECK(!SudekiMpLanPartyReplicaSample(&r,&lease,2000,&sample,&tick));
    push_replica_frame(&r,&lease,&frame,3,1100);
    CHECK(SudekiMpLanPartyReplicaSample(&r,&lease,2000,&sample,&tick));
    CHECK(tick==1050);
    CHECK(SudekiMpLanPartyReplicaSample(&r,&lease,2025,&sample,&tick));
    CHECK(tick==1075 && SudekiMpLanPartyFrameValid(&sample));
    for (unsigned c=0;c<2;++c) for (unsigned a=0;a<2;++a)
        CHECK(fabsf(sample.chunk[c].seat[a].x-((float)tick+(float)(c*2+a)*1000))<0.01f);
    before=r;
    CHECK(!SudekiMpLanPartyReplicaPush(&r,&lease,&frame)); /* duplicate */
    CHECK(!memcmp(&r,&before,sizeof(r)));
    frame.chunk[0].sequence=frame.chunk[1].sequence=4;
    frame.chunk[0].host_tick=frame.chunk[1].host_tick=900;
    CHECK(!SudekiMpLanPartyReplicaPush(&r,&lease,&frame));
    CHECK(!memcmp(&r,&before,sizeof(r)));
    frame.chunk[0].sequence=frame.chunk[1].sequence=3;
    frame.chunk[0].host_tick=frame.chunk[1].host_tick=1100;
    frame.chunk[1].seat[1].actor_type=SUDEKIMP_LAN_ARENA_TAL_TYPE;
    CHECK(!SudekiMpLanPartyReplicaPush(&r,&lease,&frame));
    CHECK(!memcmp(&r,&before,sizeof(r)));
    frame.chunk[1].seat[1].actor_type=SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    ++stale.generation;
    CHECK(!SudekiMpLanPartyReplicaSample(&r,&stale,2030,&sample,&tick));
    CHECK(!memcmp(&r,&before,sizeof(r)));
    /* Only the world chunk changes layout: neither actor pair may keep its
     * old clock/history while the other starts a new stream. */
    frame.chunk[0].enemy_count=1;
    frame.chunk[0].enemies[0].native_entity_id=SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID;
    frame.chunk[0].enemies[0].hp=100;
    push_replica_frame(&r,&lease,&frame,4,1150);
    CHECK(!r.clock.initialized && !r.chunks[0].previous_valid && !r.chunks[1].previous_valid);
    CHECK(!SudekiMpLanPartyReplicaSample(&r,&lease,2030,&sample,&tick));
    push_replica_frame(&r,&lease,&frame,5,1200);
    push_replica_frame(&r,&lease,&frame,6,1250);
    CHECK(SudekiMpLanPartyReplicaSample(&r,&lease,2040,&sample,&tick));
    CHECK(tick==1200);
    /* A new transport lease starts fresh even when sequence restarts. */
    lease=stale; ++lease.token;
    push_replica_frame(&r,&lease,&frame,1,10);
    CHECK(!r.clock.initialized && !r.chunks[0].previous_valid && !r.chunks[1].previous_valid);
    uint8_t host,client;
    CHECK(SudekiMpLanArenaSeatActorTypes(&host,&client));
    CHECK(host==SUDEKIMP_LAN_ARENA_TAL_TYPE && client==SUDEKIMP_LAN_ARENA_AILISH_TYPE);
}
static void test_block_dodge_extension(void) {
    const int selectors[]={21,22,30,31,29};
    const uint8_t variants[]={SUDEKIMP_LAN_ARENA_ACTION_BLOCK_HOLD,
        SUDEKIMP_LAN_ARENA_ACTION_BLOCK_RELEASE,SUDEKIMP_LAN_ARENA_ACTION_ROLL_LEFT,
        SUDEKIMP_LAN_ARENA_ACTION_ROLL_RIGHT,SUDEKIMP_LAN_ARENA_ACTION_BACKFLIP};
    SudekiMpLanPartyFrame f; fill_frame(&f);
    for(unsigned i=0; i<5; ++i) {
        int selector=-1,state=-1; uint8_t variant=0;
        CHECK(SudekiMpLanPartyTalActionToPresentation(variants[i],&selector,&state));
        CHECK(selector==selectors[i]);
        CHECK(SudekiMpLanPartyTalActionObserve(selector,(uint8_t)state,&variant));
        CHECK(variant==variants[i]);
        BOOL found=FALSE;
        for(unsigned clip=1;clip<=SUDEKIMP_LAN_ARENA_PARTY_TAL_MOTION_MAX;++clip)
            if(SudekiMpLanPartyCombatMotionSelector(SUDEKIMP_LAN_ARENA_TAL_TYPE,clip)==selector)
                found=TRUE;
        CHECK(found);
    }
    for(unsigned c=0;c<2;++c) {
        SudekiMpLanArenaSpiritVfxSnapshot *v=&f.chunk[c].spirit_vfx[0];
        f.chunk[c].spirit_vfx_observed=1; f.chunk[c].spirit_vfx_count=1;
        v->instance_sequence=c+1; v->owner_actor_type=SudekiMpLanPartyActorType(c*2);
        v->kind=c?SUDEKIMP_LAN_PARTY_TAL_VFX_SHIELD_LOOP:SUDEKIMP_LAN_ARENA_BUKI_VFX_SHIELD_LOOP;
        v->emitted_host_tick=900; v->rotation_xyzw[3]=1;
        v->scale[0]=v->scale[1]=v->scale[2]=1;
    }
    CHECK(SudekiMpLanPartyFrameValid(&f));
    CHECK(!SudekiMpLanArenaSpiritVfxRosterValid(&f.chunk[1]));
    CHECK(!SudekiMpLanArenaSnapshotValid(&f.chunk[1])); /* LA42 stays closed. */
    SudekiMpLanArenaCodecRoster r={{SUDEKIMP_LAN_ARENA_TAL_TYPE,SUDEKIMP_LAN_ARENA_AILISH_TYPE},1};
    SudekiMpLanArenaPacket packet={0},decoded={0};
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE]; size_t size=0;
    packet.type=SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT; packet.session_token=99;
    packet.body.snapshot=f.chunk[1];
    CHECK(SudekiMpLanArenaEncodeForRoster(bytes,&size,&packet,&r));
    CHECK(SudekiMpLanArenaDecodeForRoster(bytes,size,&decoded,&r));
    CHECK(decoded.body.snapshot.spirit_vfx[0].kind==SUDEKIMP_LAN_PARTY_TAL_VFX_SHIELD_LOOP);
    for(unsigned i=0;i<5;++i) {
        SudekiMpLanArenaActorSnapshot *tal=&packet.body.snapshot.seat[0];
        tal->animation_state=SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
        tal->combat_state=SUDEKIMP_LAN_ARENA_COMBAT_BLOCK;
        tal->action_variant=variants[i]; tal->action_sequence=1;
        tal->action_phase_valid=1; tal->action_phase_q8=16;
        tal->action_history_count=1;
        /* Designated assignments do not depend on private struct packing. */
        tal->action_history[0].sequence=1;
        tal->action_history[0].variant=variants[i]; tal->action_history[0].host_tick=900;
        CHECK(SudekiMpLanArenaEncodeForRoster(bytes,&size,&packet,&r));
        CHECK(SudekiMpLanArenaDecodeForRoster(bytes,size,&decoded,&r));
        CHECK(decoded.body.snapshot.seat[0].action_history[0].variant==variants[i]);
        CHECK(!SudekiMpLanArenaSnapshotValid(&packet.body.snapshot));
    }
    r.world_locomotion=0;
    CHECK(!SudekiMpLanArenaDecodeForRoster(bytes,size,&decoded,&r));
    f.chunk[1].spirit_vfx[0].owner_actor_type=SUDEKIMP_LAN_ARENA_BUKI_TYPE;
    CHECK(!SudekiMpLanPartyFrameValid(&f));
    f.chunk[1].spirit_vfx[0].owner_actor_type=SUDEKIMP_LAN_ARENA_TAL_TYPE;
    f.chunk[1].spirit_vfx[0].instance_sequence=1;
    CHECK(!SudekiMpLanPartyFrameValid(&f));
    f.chunk[1].spirit_vfx[0].instance_sequence=2;
    f.chunk[1].spirit_vfx[0].skill_sequence=1;
    CHECK(!SudekiMpLanPartyFrameValid(&f));
}

static void test_running_attack_extension(void) {
    const uint8_t type=SUDEKIMP_LAN_ARENA_TAL_TYPE;
    const uint8_t attack=SUDEKIMP_LAN_ARENA_ACTION_RUNNING_ATTACK;
    int selector=-1,state=-1; uint8_t variant=0;
    CHECK(SudekiMpLanPartyTalActionToPresentation(attack,&selector,&state));
    CHECK(selector==38 && state==1);
    CHECK(SudekiMpLanPartyTalActionObserve(38,1,&variant) && variant==attack);
    CHECK(!SudekiMpLanPartyTalActionObserve(38,192,&variant));
    CHECK(!SudekiMpLanPartyActionTerminalObserved(type,attack,38,1));
    CHECK(SudekiMpLanPartyActionTerminalObserved(type,attack,38,128));
    CHECK(SudekiMpLanPartyActionTerminalObserved(type,attack,17,128));
    CHECK(!SudekiMpLanPartyActionTerminalObserved(type,attack,36,128));
    CHECK(SudekiMpLanPartyCombatMotionSelector(type,25)==38);
    CHECK(SudekiMpLanPartyCombatMotionSelector(type,26)==-1);
    int selectors[4]={38,0,36,0};
    uint8_t states[4]={1,192,0,192};
    float rates[4]={24,0,24,0},times[4]={2,0,9,0},blends[3]={0,0,0.5f};
    SudekiMpLanArenaLocomotion motion,next;
    CHECK(SudekiMpLanPartyCombatMotionCapture(type,selectors,states,rates,times,
        blends,NULL,&motion));
    CHECK(motion.clip[0]==25 && motion.clip[2]==2 && motion.time[0]==2);
    /* Repeated running attacks retain their own interpolation boundary. */
    times[0]=0;
    CHECK(SudekiMpLanPartyCombatMotionCapture(type,selectors,states,rates,times,
        blends,&motion,&next));
    CHECK(next.sequence==motion.sequence+1);
    selectors[0]=17; selectors[2]=38; states[0]=0; states[2]=128;
    times[0]=0; times[2]=19;
    CHECK(SudekiMpLanPartyCombatMotionCapture(type,selectors,states,rates,times,
        blends,&next,&motion));
    CHECK(motion.clip[0]==1 && motion.clip[2]==25 && motion.time[2]==19);

    SudekiMpLanPartyFrame frame; fill_frame(&frame);
    SudekiMpLanArenaCodecRoster roster={{type,SUDEKIMP_LAN_ARENA_AILISH_TYPE},1};
    SudekiMpLanArenaPacket packet={0},decoded={0};
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE]; size_t size=0;
    packet.type=SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT; packet.session_token=99;
    packet.body.snapshot=frame.chunk[1];
    SudekiMpLanArenaActorSnapshot *tal=&packet.body.snapshot.seat[0];
    tal->animation_state=SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    tal->combat_state=SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    tal->action_variant=attack; tal->action_sequence=1;
    tal->action_phase_valid=1; tal->action_phase_q8=16;
    tal->action_history_count=1;
    tal->action_history[0].sequence=1; tal->action_history[0].variant=attack;
    tal->action_history[0].host_tick=900;
    tal->locomotion=motion;
    CHECK(SudekiMpLanArenaEncodeForRoster(bytes,&size,&packet,&roster));
    CHECK(SudekiMpLanArenaDecodeForRoster(bytes,size,&decoded,&roster));
    CHECK(decoded.body.snapshot.seat[0].locomotion.clip[2]==25);
    CHECK(decoded.body.snapshot.seat[0].action_history[0].variant==attack);
    roster.world_locomotion=0;
    CHECK(!SudekiMpLanArenaDecodeForRoster(bytes,size,&decoded,&roster));
    memset(&tal->locomotion,0,sizeof(tal->locomotion));
    CHECK(!SudekiMpLanArenaEncodeForRoster(bytes,&size,&packet,&roster));
    /* Retained history alone must stay legal in SMP4 after the attack ends,
     * and must still be rejected by the unchanged legacy codec. */
    tal->animation_state=SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    tal->combat_state=SUDEKIMP_LAN_ARENA_COMBAT_IDLE;
    tal->action_variant=0; tal->action_phase_valid=0; tal->action_phase_q8=0;
    CHECK(!SudekiMpLanArenaEncodeForRoster(bytes,&size,&packet,&roster));
    roster.world_locomotion=1;
    CHECK(SudekiMpLanArenaEncodeForRoster(bytes,&size,&packet,&roster));
    CHECK(SudekiMpLanArenaDecodeForRoster(bytes,size,&decoded,&roster));
    tal->locomotion=motion; tal->locomotion.clip[2]=26;
    CHECK(!SudekiMpLanArenaEncodeForRoster(bytes,&size,&packet,&roster));
}

static void test_motion_catalog(void) {
    static const int move[]={6,5,8,7};
    static const uint8_t states[]={SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        SUDEKIMP_LAN_ARENA_ANIMATION_MOVING,SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE,
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO};
    SudekiMpLanPartyFrame f; fill_frame(&f);
    CHECK(!SudekiMpLanPartyMovementFrameValid(&f)); /* v2 requires host phases */
    for(unsigned i=0;i<4;++i) {
        SudekiMpLanPartyMotion m; uint8_t state=255,type=SudekiMpLanPartyActorType(i);
        for(unsigned s=0;s<4;++s) {
            CHECK(SudekiMpLanPartyMotionDescribe(type,states[s],&m));
            CHECK(SudekiMpLanPartyMotionObserve(type,m.primary,&state));
            CHECK(state==states[s]);
            CHECK(m.ranged==(i==1 || i==3));
            if(s==1) CHECK(m.moving && m.primary==move[i] && m.secondary==move[i]+1);
        }
        CHECK(!SudekiMpLanPartyMotionObserve(type,4095,&state));
        CHECK(!SudekiMpLanPartyMotionDescribe(type,SUDEKIMP_LAN_ARENA_ANIMATION_ACTION,&m));
        int selectors[4]={SudekiMpLanPartyMotionSelector(type,1),0,0,0};
        uint8_t native_states[4]={0,192,192,192};
        float rates[4]={12,0,0,0},times[4]={30,0,0,0},blends[3]={0};
        SudekiMpLanArenaLocomotion phase,next,saved;
        CHECK(SudekiMpLanPartyMotionCapture(type,selectors,native_states,rates,times,
            blends,NULL,&phase));
        CHECK(phase.sequence==1 && phase.clip[0]==1 && phase.time[0]==30);
        CHECK(!!SudekiMpLanPartyMotionElcoIdleEnded(type,&phase)==(i==1));
        CHECK(!SudekiMpLanPartyMotionElcoIdleEnded(type,NULL));
        CHECK(SudekiMpLanPartyMotionChannels(type)==4);
        selectors[2]=SudekiMpLanPartyMotionSelector(type,4);
        native_states[2]=1; rates[2]=24; times[2]=11; blends[2]=0.5f;
        CHECK(SudekiMpLanPartyMotionCapture(type,selectors,native_states,rates,times,
            blends,&phase,&next));
        CHECK(next.clip[2]==4 && next.time[2]==11 && next.blend[2]==0.5f);
        CHECK(!SudekiMpLanPartyMotionElcoIdleEnded(type,&next));
        if(i==1) {
            SudekiMpLanArenaLocomotion interrupted=next;
            interrupted.clip[0]=2; /* movement cannot cut outgoing gun idle */
            CHECK(!SudekiMpLanPartyMotionElcoIdleEnded(type,&interrupted));
            interrupted.clip[2]=0; interrupted.state[2]=192;
            interrupted.rate[2]=interrupted.time[2]=0;
            CHECK(SudekiMpLanPartyMotionElcoIdleEnded(type,&interrupted));
            interrupted.valid=0;
            CHECK(!SudekiMpLanPartyMotionElcoIdleEnded(type,&interrupted));
        }
        selectors[2]=0; native_states[2]=192; rates[2]=times[2]=blends[2]=0;
        times[0]=31;
        CHECK(SudekiMpLanPartyMotionCapture(type,selectors,native_states,rates,times,
            blends,&phase,&next));
        CHECK(next.sequence==1 && next.time[0]==31);
        times[0]=0;
        CHECK(SudekiMpLanPartyMotionCapture(type,selectors,native_states,rates,times,
            blends,&next,&phase));
        CHECK(phase.sequence==2); /* native loop fences interpolation */
        f.chunk[i/2].seat[i%2].locomotion=phase;
        saved=next; selectors[0]=9999;
        CHECK(!SudekiMpLanPartyMotionCapture(type,selectors,native_states,rates,times,
            blends,&phase,&next));
        CHECK(!memcmp(&next,&saved,sizeof(next))); /* no partial publication */
        phase.clip[0]=255; CHECK(!SudekiMpLanPartyMotionValid(type,&phase));
        phase=f.chunk[i/2].seat[i%2].locomotion;
        phase.state[0]=3; CHECK(!SudekiMpLanPartyMotionValid(type,&phase));
        phase=f.chunk[i/2].seat[i%2].locomotion;
        phase.time[0]=NAN; CHECK(!SudekiMpLanPartyMotionValid(type,&phase));
    }
    CHECK(SudekiMpLanPartyMovementFrameValid(&f));
    f.chunk[0].combat_enabled=f.chunk[1].combat_enabled=1;
    CHECK(!SudekiMpLanPartyMovementFrameValid(&f));
}
static void test_replica_fourth_actor_action_clock(void) {
    SudekiMpLanPartyReplica r;
    SudekiMpLanPartyFrame frame,sample;
    SudekiMpLanPartyLease lease={456,2,2}; uint32_t tick;
    SudekiMpLanPartyReplicaReset(&r); fill_frame(&frame);
    push_replica_frame(&r,&lease,&frame,1,1000);
    push_replica_frame(&r,&lease,&frame,2,1050);
    push_replica_frame(&r,&lease,&frame,3,1100);
    CHECK(SudekiMpLanPartyReplicaSample(&r,&lease,2000,&sample,&tick));
    frame.chunk[0].combat_enabled=frame.chunk[1].combat_enabled=1;
    push_replica_frame(&r,&lease,&frame,4,1150);
    push_replica_frame(&r,&lease,&frame,5,1200);
    push_replica_frame(&r,&lease,&frame,6,1250);
    CHECK(SudekiMpLanPartyReplicaSample(&r,&lease,2100,&sample,&tick));
    /* Ailish is in chunk one: her action must protect BOTH pairs from the
     * old 2x backlog catch-up. Use a valid native skill record. */
    frame.chunk[1].seat[1].skill_active=1;
    frame.chunk[1].seat[1].skill_sequence=1;
    frame.chunk[1].seat[1].skill_kind=SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    frame.chunk[1].seat[1].skill_slot=1;
    push_replica_frame(&r,&lease,&frame,7,1300);
    CHECK(SudekiMpLanPartyReplicaSample(&r,&lease,2110,&sample,&tick));
    CHECK(tick==1210); /* not 1220 */
}
static void test_world_phase_replica(void) {
    SudekiMpLanPartyReplica r; SudekiMpLanPartyFrame f,sample;
    SudekiMpLanPartyLease lease={12345,4,3}; uint32_t tick;
    fill_frame(&f); SudekiMpLanPartyReplicaReset(&r);
    for(unsigned seq=1;seq<=3;++seq) {
        for(unsigned i=0;i<4;++i) {
            SudekiMpLanArenaActorSnapshot *a=&f.chunk[i/2].seat[i%2];
            a->locomotion.valid=1; a->locomotion.sequence=9;
            a->locomotion.clip[0]=1; a->locomotion.rate[0]=12;
            a->locomotion.time[0]=30+(float)seq;
        }
        push_replica_frame(&r,&lease,&f,seq,1000+seq*50);
    }
    CHECK(SudekiMpLanPartyReplicaSample(&r,&lease,2000,&sample,&tick));
    CHECK(SudekiMpLanPartyReplicaSample(&r,&lease,2025,&sample,&tick));
    CHECK(tick==1125 && SudekiMpLanPartyMovementFrameValid(&sample));
    for(unsigned i=0;i<4;++i)
        CHECK(fabsf(sample.chunk[i/2].seat[i%2].locomotion.time[0]-32.5f)<0.001f);
    /* The opt-in sampler must not alter the old noncombat retirement rule. */
    CHECK(SudekiMpLanArenaReplicaSample(&r.chunks[0],tick,&sample.chunk[0]));
    CHECK(!sample.chunk[0].seat[1].locomotion.valid);
    for(unsigned part=0;part<2;++part) {
        SudekiMpLanArenaCodecRoster context={{SudekiMpLanPartyActorType(part*2),
            SudekiMpLanPartyActorType(part*2+1)},1};
        SudekiMpLanArenaPacket packet={0},decoded;
        uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE]; size_t size;
        packet.type=SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT; packet.session_token=12345;
        packet.body.snapshot=f.chunk[part];
        packet.sequence=packet.body.snapshot.sequence;
        CHECK(SudekiMpLanArenaEncodeForRoster(bytes,&size,&packet,&context));
        CHECK(SudekiMpLanArenaDecodeForRoster(bytes,size,&decoded,&context));
        for(unsigned i=0;i<2;++i)
            CHECK(decoded.body.snapshot.seat[i].locomotion.time[0]==33);
        context.world_locomotion=0;
        CHECK(!SudekiMpLanArenaDecodeForRoster(bytes,size,&decoded,&context));
        CHECK(!SudekiMpLanArenaEncodeForRoster(bytes,&size,&packet,&context));
        context.world_locomotion=2;
        CHECK(!SudekiMpLanArenaEncodeForRoster(bytes,&size,&packet,&context));
    }
}
static void test_transport_replica_consumption(void) {
    SudekiMpLanPartyReplica replicas[3]; SudekiMpLanPartyFrame frame,sample;
    uint32_t tick;
    memset(replicas,0,sizeof(replicas)); fill_frame(&frame);
    for (unsigned seq=1;seq<=3;++seq) {
        frame.chunk[0].host_tick=frame.chunk[1].host_tick=2000+seq*50;
        CHECK(SudekiMpLanPartySendFrame(nodes[0],&frame)); pump();
        for (unsigned seat=1;seat<4;++seat)
            CHECK(SudekiMpLanPartyReplicaConsume(&replicas[seat-1],nodes[seat],seat));
    }
    for (unsigned seat=1;seat<4;++seat) {
        SudekiMpLanPartyPeerStatus peer=status(seat,seat);
        CHECK(SudekiMpLanPartyReplicaSample(&replicas[seat-1],&peer.lease,GetTickCount(),&sample,&tick));
        CHECK(tick==2100 && SudekiMpLanPartyFrameValid(&sample));
    }
}
static void test_three_clients(void) {
    SudekiMpLanPartyInput input;
    SudekiMpLanArenaInput request;
    memset(&request, 0, sizeof(request));
    for (unsigned i = 1; i < 4; ++i) {
        SudekiMpLanPartyPeerStatus p = status(0, i), c = status(i, i);
        CHECK(p.phase == SUDEKIMP_LAN_PARTY_PENDING && c.phase == SUDEKIMP_LAN_PARTY_PENDING);
        CHECK(p.lease.token == c.lease.token && p.lease.generation == 1);
        request.actor_type = SudekiMpLanPartyActorType(i);
        CHECK(!SudekiMpLanPartySendInput(nodes[i], &request));
        CHECK(!SudekiMpLanPartyTakeInput(nodes[0], i, &input));
        CHECK(!SudekiMpLanPartyApprove(nodes[i], &c.lease));
        wait_transport(i);
        CHECK(SudekiMpLanPartyApprove(nodes[0], &p.lease));
        CHECK(!SudekiMpLanPartyApprove(nodes[0], &p.lease));
    }
    pump();
    for (unsigned i = 1; i < 4; ++i) {
        CHECK(status(i, i).phase == SUDEKIMP_LAN_PARTY_ACTIVE);
        request.actor_type = SudekiMpLanPartyActorType(i);
        request.world_direction_x = (int16_t)(i * 1000);
        request.weak_attack_pressed = 1; request.weak_attack_held = 1;
        CHECK(SudekiMpLanPartySendInput(nodes[i], &request));
        request.weak_attack_pressed = 0; request.weak_attack_held = 0;
        request.world_direction_x += 100;
        CHECK(SudekiMpLanPartySendInput(nodes[i], &request));
        request.actor_type = SudekiMpLanPartyActorType(0);
        CHECK(!SudekiMpLanPartySendInput(nodes[i], &request));
    }
    pump();
    for (unsigned i = 1; i < 4; ++i) {
        CHECK(SudekiMpLanPartyTakeInput(nodes[0], i, &input));
        CHECK(input.lease.seat == i && input.input.actor_type == SudekiMpLanPartyActorType(i));
        CHECK(input.input.world_direction_x == (int16_t)(i * 1000 + 100));
        CHECK(input.input.weak_attack_pressed == 1 && input.input.weak_attack_held == 0);
        CHECK(status(0, i).admitted_input_sequence == 0); /* Receipt != execution. */
        CHECK(SudekiMpLanPartyAdmitInput(nodes[0], &input));
        CHECK(!SudekiMpLanPartyAdmitInput(nodes[0], &input));
        CHECK(!SudekiMpLanPartyTakeInput(nodes[0], i, &input));
    }
    SudekiMpLanPartyFrame frame, copy; fill_frame(&frame);
    CHECK(!SudekiMpLanPartySendFrame(nodes[1], &frame));
    CHECK(SudekiMpLanPartySendFrame(nodes[0], &frame)); pump();
    for (unsigned i = 1; i < 4; ++i) {
        CHECK(SudekiMpLanPartyTakeFrame(nodes[i], &copy));
        CHECK(SudekiMpLanPartyFrameValid(&copy));
        CHECK(copy.chunk[0].acknowledged_input == status(0, i).admitted_input_sequence);
        for (unsigned c = 0; c < 2; ++c) for (unsigned a = 0; a < 2; ++a)
            CHECK(copy.chunk[c].seat[a].actor_type == SudekiMpLanPartyActorType(c * 2 + a));
        CHECK(!SudekiMpLanPartyTakeFrame(nodes[i], &copy));
    }
}
static void test_combat_mode_publication(void) {
    SudekiMpLanPartyCombatMode mode={0};
    SudekiMpLanPartyPeerStatus p=status(1,1);
    CHECK(!SudekiMpLanPartyGetCombatMode(nodes[1],&p.lease,now,&mode));
    CHECK(!SudekiMpLanPartyPublishCombatMode(nodes[1],1,now));
    CHECK(!SudekiMpLanPartyPublishCombatMode(nodes[0],2,now));
    CHECK(SudekiMpLanPartyPublishCombatMode(nodes[0],0,now)); pump();
    for(unsigned i=1;i<4;++i) {
        p=status(i,i);
        CHECK(SudekiMpLanPartyGetCombatMode(nodes[i],&p.lease,now,&mode));
        CHECK(mode.sequence==1u && !mode.enabled);
    }
    Sleep(2); pump();
    CHECK(SudekiMpLanPartyPublishCombatMode(nodes[0],1,now)); pump();
    uint32_t transition=0;
    for(unsigned i=1;i<4;++i) {
        p=status(i,i);
        CHECK(SudekiMpLanPartyGetCombatMode(nodes[i],&p.lease,now,&mode));
        CHECK(mode.sequence==2u && mode.enabled);
        transition=mode.host_tick;
        CHECK(!SudekiMpLanPartyGetCombatMode(nodes[i],&p.lease,now+501u,&mode));
        SudekiMpLanPartyLease old=p.lease; ++old.generation;
        CHECK(!SudekiMpLanPartyGetCombatMode(nodes[i],&old,now,&mode));
    }
    CHECK(!SudekiMpLanPartyPublishCombatMode(nodes[0],0,transition-1u));
    CHECK(!SudekiMpLanPartyPublishCombatMode(nodes[0],0,transition));
    uint32_t refresh_started=GetTickCount(); BOOL refreshed=FALSE;
    do {
        Sleep(5); pump();
        CHECK(SudekiMpLanPartyPublishCombatMode(nodes[0],1,now)); pump();
        refreshed=TRUE;
        for(unsigned i=1;i<4;++i) {
            p=status(i,i);
            if(!SudekiMpLanPartyGetCombatMode(nodes[i],&p.lease,now,&mode) ||
                mode.observed_tick==transition) refreshed=FALSE;
        }
    } while(!refreshed && GetTickCount()-refresh_started<1500u);
    CHECK(refreshed);
    for(unsigned i=1;i<4;++i) {
        p=status(i,i);
        CHECK(SudekiMpLanPartyGetCombatMode(nodes[i],&p.lease,now,&mode));
        CHECK(mode.sequence==2u && mode.host_tick==transition &&
            mode.observed_tick!=transition);
    }
}
static void test_combat_mode_frame_fence(void) {
    SudekiMpLanPartyFrame frame; fill_frame(&frame);
    SudekiMpLanPartyCombatMode mode={7u,1000u,1002u,0};
    CHECK(SudekiMpLanPartyCombatModeFrameReady(&mode,&frame));
    mode.host_tick=1001u;
    CHECK(!SudekiMpLanPartyCombatModeFrameReady(&mode,&frame));
    mode.host_tick=1000u; mode.enabled=1;
    CHECK(!SudekiMpLanPartyCombatModeFrameReady(&mode,&frame));
    frame.chunk[0].combat_enabled=frame.chunk[1].combat_enabled=1u;
    CHECK(SudekiMpLanPartyCombatModeFrameReady(&mode,&frame));
    frame.chunk[1].host_tick++;
    CHECK(!SudekiMpLanPartyCombatModeFrameReady(&mode,&frame));
    frame.chunk[1].host_tick--;
    mode.sequence=0;
    CHECK(!SudekiMpLanPartyCombatModeFrameReady(&mode,&frame));
    mode.sequence=1; mode.enabled=2;
    CHECK(!SudekiMpLanPartyCombatModeFrameReady(&mode,&frame));
    mode.enabled=1; mode.host_tick=UINT32_MAX-10u; mode.observed_tick=5u;
    frame.chunk[0].host_tick=frame.chunk[1].host_tick=3u;
    CHECK(SudekiMpLanPartyCombatModeFrameReady(&mode,&frame));
    frame.chunk[0].host_tick=frame.chunk[1].host_tick=UINT32_MAX-11u;
    CHECK(!SudekiMpLanPartyCombatModeFrameReady(&mode,&frame));
    CHECK(!SudekiMpLanPartyCombatModeFrameReady(NULL,&frame));
    CHECK(!SudekiMpLanPartyCombatModeFrameReady(&mode,NULL));
}
static void test_independent_disconnect_rejoin(void) {
    SudekiMpLanPartyPeerStatus previous = status(0, 1);
    SudekiMpLanPartyLease lease = previous.lease;
    CHECK(!SudekiMpLanPartyReleaseDrained(nodes[0], &lease));
    CHECK(SudekiMpLanPartyDisconnect(nodes[1], &lease)); pump();
    SudekiMpLanPartyCombatMode mode;
    CHECK(!SudekiMpLanPartyGetCombatMode(nodes[1],&lease,now,&mode));
    CHECK(status(0, 1).phase == SUDEKIMP_LAN_PARTY_DRAINING);
    CHECK(!SudekiMpLanPartyLeaseActive(nodes[0], &lease));
    CHECK(status(0, 2).phase == SUDEKIMP_LAN_PARTY_ACTIVE);
    CHECK(status(0, 3).phase == SUDEKIMP_LAN_PARTY_ACTIVE);
    CHECK(status(0, 0).phase == SUDEKIMP_LAN_PARTY_ACTIVE);
    SudekiMpLanPartyDestroy(nodes[1], FALSE); config.local_seat = 1;
    nodes[1] = SudekiMpLanPartyCreate(&config); pump();
    CHECK(status(1, 1).phase == SUDEKIMP_LAN_PARTY_REJECTED);
    CHECK(status(1, 1).failure == SUDEKIMP_LAN_ARENA_REJECT_BUSY);
    SudekiMpLanPartyLease wrong = lease; ++wrong.generation;
    CHECK(!SudekiMpLanPartyReleaseDrained(nodes[0], &wrong));
    CHECK(SudekiMpLanPartyReleaseDrained(nodes[0], &lease));
    SudekiMpLanPartyDestroy(nodes[1], FALSE);
    nodes[1] = SudekiMpLanPartyCreate(&config); pump();
    SudekiMpLanPartyPeerStatus current = status(0, 1);
    CHECK(current.phase == SUDEKIMP_LAN_PARTY_PENDING);
    CHECK(current.lease.generation == 2 && current.lease.token != lease.token);
    CHECK(!SudekiMpLanPartyApprove(nodes[0], &lease));
    wait_transport(1);
    CHECK(SudekiMpLanPartyApprove(nodes[0], &current.lease)); pump();
    CHECK(!SudekiMpLanPartyGetCombatMode(nodes[1],&current.lease,now,&mode));
    CHECK(SudekiMpLanPartyPublishCombatMode(nodes[0],1,now)); pump();
    CHECK(SudekiMpLanPartyGetCombatMode(nodes[1],&current.lease,now,&mode));
    CHECK(mode.sequence==2u && mode.enabled);
    CHECK(!SudekiMpLanPartyGetCombatMode(nodes[1],&lease,now,&mode));
    SudekiMpLanPartyInput old_input; memset(&old_input, 0, sizeof(old_input));
    old_input.lease = lease; old_input.input.actor_type = SudekiMpLanPartyActorType(1);
    old_input.input.sequence = 1;
    CHECK(!SudekiMpLanPartyAdmitInput(nodes[0], &old_input));
    CHECK(!SudekiMpLanPartyDisconnect(nodes[0], &lease));
    CHECK(status(0, 2).phase == SUDEKIMP_LAN_PARTY_ACTIVE);
    CHECK(status(0, 3).phase == SUDEKIMP_LAN_PARTY_ACTIVE);
}
static void test_busy_hash_and_timeouts(void) {
    config.local_seat = 2;
    SudekiMpLanPartySession *duplicate = SudekiMpLanPartyCreate(&config);
    CHECK(duplicate != NULL);
    now = GetTickCount();
    SudekiMpLanPartyPoll(duplicate, now); pump(); SudekiMpLanPartyPoll(duplicate, now);
    SudekiMpLanPartyPeerStatus p;
    CHECK(SudekiMpLanPartyPeerStatusGet(duplicate, 2, &p));
    CHECK(p.phase == SUDEKIMP_LAN_PARTY_REJECTED && p.failure == SUDEKIMP_LAN_ARENA_REJECT_BUSY);
    SudekiMpLanPartyDestroy(duplicate, FALSE);
    config.game_hash[0] ^= 1;
    duplicate = SudekiMpLanPartyCreate(&config);
    CHECK(duplicate != NULL);
    now = GetTickCount();
    SudekiMpLanPartyPoll(duplicate, now); pump(); SudekiMpLanPartyPoll(duplicate, now);
    CHECK(SudekiMpLanPartyPeerStatusGet(duplicate, 2, &p));
    CHECK(p.phase == SUDEKIMP_LAN_PARTY_REJECTED && p.failure == SUDEKIMP_LAN_ARENA_REJECT_GAME_HASH);
    SudekiMpLanPartyDestroy(duplicate, FALSE); config.game_hash[0] ^= 1;
    /* Only Elco stops pumping; Tal/Ailish keep their independent heartbeats. */
    for (unsigned t = 0; t < 12; ++t) {
        now += 500;
        SudekiMpLanPartyPoll(nodes[2], now); SudekiMpLanPartyPoll(nodes[3], now);
        SudekiMpLanPartyPoll(nodes[0], now);
    }
    CHECK(status(0, 1).phase == SUDEKIMP_LAN_PARTY_DRAINING);
    CHECK(status(0, 1).failure == SUDEKIMP_LAN_ARENA_REJECT_TIMEOUT);
    CHECK(status(0, 2).phase == SUDEKIMP_LAN_PARTY_ACTIVE);
    CHECK(status(0, 3).phase == SUDEKIMP_LAN_PARTY_ACTIVE);
}

/* Raw datagrams deliberately bypass the friendly API's sender checks. These
 * tests prove rejection at the receiving boundary, not just in a UI helper. */
enum { RAW_HEADER = 28, RAW_HELLO = 1, RAW_ACK = 2, RAW_REJECT = 3,
    RAW_INPUT = 4, RAW_FRAME = 5, RAW_END = 6, RAW_KEEPALIVE = 7 };
static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
}
static void put64(uint8_t *p, uint64_t v) { put32(p, (uint32_t)v); put32(p + 4, (uint32_t)(v >> 32)); }
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint64_t get64(const uint8_t *p) { return get32(p) | (uint64_t)get32(p + 4) << 32; }
static SOCKET raw_socket(struct sockaddr_in *address) {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    memset(address, 0, sizeof(*address)); address->sin_family = AF_INET;
    address->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int size = sizeof(*address); DWORD timeout = 500;
    CHECK(sock != INVALID_SOCKET);
    CHECK(bind(sock, (const struct sockaddr *)address, size) == 0);
    CHECK(getsockname(sock, (struct sockaddr *)address, &size) == 0);
    CHECK(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) == 0);
    return sock;
}
static size_t raw_read(SOCKET sock, uint8_t *bytes, struct sockaddr_in *source) {
    int size = sizeof(*source);
    int n = recvfrom(sock, (char *)bytes, 1468, 0, (struct sockaddr *)source, &size);
    CHECK(n >= RAW_HEADER);
    return n > 0 ? (size_t)n : 0;
}
static void raw_send(SOCKET sock, const struct sockaddr_in *to, unsigned kind,
    unsigned seat, uint32_t generation, uint64_t token, uint32_t sequence,
    unsigned part, const uint8_t *body, size_t size, unsigned version) {
    uint8_t bytes[1468]; memset(bytes, 0, RAW_HEADER);
    CHECK(size + RAW_HEADER <= sizeof(bytes));
    memcpy(bytes, "SMP4", 4); bytes[4] = (uint8_t)version;
    bytes[6] = (uint8_t)kind; bytes[7] = (uint8_t)seat;
    put32(bytes + 8, generation); put64(bytes + 12, token); put32(bytes + 20, sequence);
    bytes[24] = (uint8_t)size; bytes[25] = (uint8_t)(size >> 8); bytes[26] = (uint8_t)part;
    if (size) memcpy(bytes + RAW_HEADER, body, size);
    CHECK(sendto(sock, (const char *)bytes, (int)(RAW_HEADER + size), 0,
        (const struct sockaddr *)to, sizeof(*to)) == (int)(RAW_HEADER + size));
}
static void raw_input(SOCKET sock, const struct sockaddr_in *to,
    const SudekiMpLanPartyLease *l, uint32_t sequence, unsigned actor) {
    SudekiMpLanArenaPacket p; uint8_t bytes[1468]; size_t size;
    SudekiMpLanArenaCodecRoster roster = {{5, 14},0};
    memset(&p, 0, sizeof(p)); p.type = SUDEKIMP_LAN_ARENA_PACKET_INPUT;
    p.sequence = sequence; p.session_token = l->token;
    p.body.input.sequence = sequence; p.body.input.actor_type = (uint8_t)actor;
    p.body.input.weak_attack_pressed = 1;
    CHECK(SudekiMpLanArenaEncodeForRoster(bytes, &size, &p, &roster));
    uint8_t body[1468]={0}; /* v13 assignment fence; zero for unmapped profile. */
    memcpy(body+12,bytes+20,size-20);
    raw_send(sock, to, RAW_INPUT, l->seat, l->generation, l->token, sequence,
        0, body, size-20+12, SUDEKIMP_LAN_PARTY_VERSION);
}
static void test_raw_authority_and_retry(void) {
    SudekiMpLanPartyConfig c = config; c.local_seat = 0; c.port = 0;
    SudekiMpLanPartySession *host = SudekiMpLanPartyCreate(&c); CHECK(host != NULL);
    struct sockaddr_in raw, host_address, source; uint8_t bytes[1468];
    SOCKET sock = raw_socket(&raw), impostor = raw_socket(&source);
    memset(&host_address, 0, sizeof(host_address)); host_address.sin_family = AF_INET;
    host_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    host_address.sin_port = htons((u_short)SudekiMpLanPartyPort(host));
    uint8_t hello[47]; memset(hello, 0, sizeof(hello)); put64(hello, 123456);
    put32(hello + 8, SUDEKIMP_LAN_PARTY_BUILD_ID); memcpy(hello + 12, c.game_hash, 32);
    hello[44] = SUDEKIMP_LAN_ARENA_MAP_CLEANROOM; hello[45] = SUDEKIMP_LAN_ARENA_PROTOCOL_VERSION;
    raw_send(sock, &host_address, RAW_HELLO, 2, 0, 0, 0, 0, hello, sizeof(hello), 99);
    now = GetTickCount(); SudekiMpLanPartyPoll(host, now);
    CHECK(raw_read(sock, bytes, &source) == RAW_HEADER + 9);
    CHECK(bytes[6] == RAW_REJECT && bytes[RAW_HEADER + 8] == SUDEKIMP_LAN_ARENA_REJECT_VERSION);
    raw_send(sock, &host_address, RAW_HELLO, 2, 0, 0, 0, 0, hello, sizeof(hello), SUDEKIMP_LAN_PARTY_VERSION);
    SudekiMpLanPartyPoll(host, now); (void)raw_read(sock, bytes, &source);
    CHECK(bytes[6] == RAW_ACK);
    SudekiMpLanPartyPeerStatus p; CHECK(SudekiMpLanPartyPeerStatusGet(host, 2, &p));
    CHECK(!p.transport_confirmed && !SudekiMpLanPartyApprove(host, &p.lease));
    SudekiMpLanPartyLease lease = p.lease;
    /* Retransmitted HELLO is an idempotent reservation, not a fresh actor. */
    raw_send(sock, &host_address, RAW_HELLO, 2, 0, 0, 0, 0, hello, sizeof(hello), SUDEKIMP_LAN_PARTY_VERSION);
    SudekiMpLanPartyPoll(host, now); (void)raw_read(sock, bytes, &source);
    CHECK(get32(bytes + 8) == lease.generation && get64(bytes + 12) == lease.token);
    raw_input(sock, &host_address, &lease, 1, SUDEKIMP_LAN_ARENA_TAL_TYPE);
    SudekiMpLanPartyPoll(host, now);
    SudekiMpLanPartyInput input; CHECK(!SudekiMpLanPartyTakeInput(host, 2, &input));
    raw_send(sock, &host_address, RAW_KEEPALIVE, 2, lease.generation, lease.token,
        2, 0, NULL, 0, SUDEKIMP_LAN_PARTY_VERSION);
    SudekiMpLanPartyPoll(host, now);
    CHECK(SudekiMpLanPartyApprove(host, &lease)); (void)raw_read(sock, bytes, &source);
    raw_input(sock, &host_address, &lease, 3, SUDEKIMP_LAN_ARENA_BUKI_TYPE);
    raw_input(impostor, &host_address, &lease, 4, SUDEKIMP_LAN_ARENA_TAL_TYPE);
    SudekiMpLanPartyLease stale = lease; ++stale.generation;
    raw_input(sock, &host_address, &stale, 5, SUDEKIMP_LAN_ARENA_TAL_TYPE);
    stale = lease; stale.token ^= 1;
    raw_input(sock, &host_address, &stale, 6, SUDEKIMP_LAN_ARENA_TAL_TYPE);
    SudekiMpLanPartyPoll(host, now); CHECK(!SudekiMpLanPartyTakeInput(host, 2, &input));
    CHECK(SudekiMpLanPartyLeaseActive(host, &lease));
    /* A client cannot publish host mode or advance host transport admission
     * with that message, even using its own valid endpoint/token/generation. */
    uint8_t forged_mode[13]={0}; put32(forged_mode,1); put32(forged_mode+4,1000);
    put32(forged_mode+8,1001); forged_mode[12]=1;
    raw_send(sock,&host_address,17,lease.seat,lease.generation,lease.token,
        1000,0,forged_mode,sizeof(forged_mode),SUDEKIMP_LAN_PARTY_VERSION);
    SudekiMpLanPartyPoll(host,now);
    raw_input(sock, &host_address, &lease, 7, SUDEKIMP_LAN_ARENA_TAL_TYPE);
    SudekiMpLanPartyPoll(host, now); CHECK(SudekiMpLanPartyTakeInput(host, 2, &input));
    CHECK(input.input.sequence == 7 && SudekiMpLanPartyAdmitInput(host, &input));
    raw_input(sock, &host_address, &lease, 7, SUDEKIMP_LAN_ARENA_TAL_TYPE);
    raw_input(sock, &host_address, &lease, 6, SUDEKIMP_LAN_ARENA_TAL_TYPE);
    SudekiMpLanPartyPoll(host, now); CHECK(!SudekiMpLanPartyTakeInput(host, 2, &input));
    raw_input(sock, &host_address, &lease, 8, SUDEKIMP_LAN_ARENA_TAL_TYPE);
    SudekiMpLanPartyPoll(host, now); CHECK(SudekiMpLanPartyTakeInput(host, 2, &input));
    SudekiMpLanPartyPoll(host, now + SUDEKIMP_LAN_PARTY_INPUT_MAX_AGE_MS + 1);
    CHECK(!SudekiMpLanPartyAdmitInput(host, &input));
    raw_input(sock, &host_address, &lease, 9, SUDEKIMP_LAN_ARENA_TAL_TYPE);
    SudekiMpLanPartyPoll(host, now + 300);
    SudekiMpLanPartyPoll(host, now + 600);
    CHECK(!SudekiMpLanPartyTakeInput(host, 2, &input));
    CHECK(SudekiMpLanPartyLeaseActive(host, &lease)); /* no session-wide timeout */
    closesocket(sock); closesocket(impostor); SudekiMpLanPartyDestroy(host, FALSE);
}
static void raw_frame(SOCKET sock, const struct sockaddr_in *to,
    const SudekiMpLanPartyLease *lease, const SudekiMpLanPartyFrame *frame,
    uint32_t sequence, unsigned part) {
    SudekiMpLanArenaPacket p; uint8_t bytes[1468]; size_t size;
    SudekiMpLanArenaCodecRoster roster = {{SudekiMpLanPartyActorType(part * 2), SudekiMpLanPartyActorType(part * 2 + 1)},1};
    memset(&p, 0, sizeof(p)); p.type = SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT;
    p.sequence = sequence; p.session_token = lease->token;
    p.body.snapshot = frame->chunk[part]; p.body.snapshot.sequence = sequence;
    CHECK(SudekiMpLanArenaEncodeForRoster(bytes, &size, &p, &roster));
    CHECK(size - 20 + RAW_HEADER <= 1468);
    raw_send(sock, to, RAW_FRAME, lease->seat, lease->generation, lease->token,
        sequence, part, bytes + 20, size - 20, SUDEKIMP_LAN_PARTY_VERSION);
    if(!part) {
        /* Empty fixture still needs v10's mandatory fuel/fixture sidecar. */
        uint8_t jetpack[42]={0};
        raw_send(sock,to,18,lease->seat,lease->generation,lease->token,
            sequence,0,jetpack,sizeof(jetpack),SUDEKIMP_LAN_PARTY_VERSION);
    }
}
static void raw_mode(SOCKET sock,const struct sockaddr_in *to,
    const SudekiMpLanPartyLease *lease,uint32_t packet,uint32_t sequence,
    uint32_t tick,uint32_t observed,uint8_t enabled) {
    uint8_t body[13]; put32(body,sequence); put32(body+4,tick);
    put32(body+8,observed); body[12]=enabled;
    raw_send(sock,to,17,lease->seat,lease->generation,lease->token,packet,
        0,body,sizeof(body),SUDEKIMP_LAN_PARTY_VERSION);
}
static void test_raw_combat_mode(void) {
    struct sockaddr_in address,source,foreign_address; uint8_t bytes[1468];
    SOCKET server=raw_socket(&address),foreign=raw_socket(&foreign_address);
    SudekiMpLanPartyConfig c=config; c.local_seat=1; c.port=ntohs(address.sin_port);
    SudekiMpLanPartySession *client=SudekiMpLanPartyCreate(&c); CHECK(client!=NULL);
    uint32_t local=GetTickCount(); SudekiMpLanPartyPoll(client,local);
    CHECK(raw_read(server,bytes,&source)==RAW_HEADER+47);
    uint64_t nonce=get64(bytes+RAW_HEADER);
    SudekiMpLanPartyLease lease={UINT64_C(0x2030405060708090),23,1};
    SudekiMpLanPartyCombatMode mode={0},saved;
    raw_mode(server,&source,&lease,2,UINT32_MAX-1u,UINT32_MAX-20u,UINT32_MAX-10u,1);
    SudekiMpLanPartyPoll(client,local);
    CHECK(!SudekiMpLanPartyGetCombatMode(client,&lease,local,&mode));
    uint8_t ack[9]; put64(ack,nonce); ack[8]=SUDEKIMP_LAN_PARTY_ACTIVE;
    raw_send(server,&source,RAW_ACK,1,lease.generation,lease.token,1,0,ack,sizeof(ack),SUDEKIMP_LAN_PARTY_VERSION);
    SudekiMpLanPartyPoll(client,local);
    CHECK(SudekiMpLanPartyLeaseActive(client,&lease));
    raw_mode(server,&source,&lease,2,UINT32_MAX-1u,UINT32_MAX-20u,UINT32_MAX-10u,1);
    SudekiMpLanPartyPoll(client,local);
    CHECK(SudekiMpLanPartyGetCombatMode(client,&lease,local,&mode));
    CHECK(mode.enabled && mode.sequence==UINT32_MAX-1u);
    SudekiMpLanPartyFrame frame;
    CHECK(!SudekiMpLanPartyTakeFrame(client,&frame)); /* No animation frame needed. */
    raw_send(server,&source,RAW_KEEPALIVE,1,lease.generation,lease.token,100,0,NULL,0,SUDEKIMP_LAN_PARTY_VERSION);
    raw_mode(server,&source,&lease,3,UINT32_MAX-1u,UINT32_MAX-20u,UINT32_MAX-5u,1);
    SudekiMpLanPartyPoll(client,local); /* Independent mode ordering survives reordering. */
    CHECK(SudekiMpLanPartyGetCombatMode(client,&lease,local,&mode));
    CHECK(mode.observed_tick==UINT32_MAX-5u); saved=mode;
    SudekiMpLanPartyLease wrong=lease; ++wrong.generation;
    raw_mode(server,&source,&wrong,101,UINT32_MAX,UINT32_MAX-2u,UINT32_MAX-1u,0);
    wrong=lease; ++wrong.token;
    raw_mode(server,&source,&wrong,102,UINT32_MAX,UINT32_MAX-2u,UINT32_MAX-1u,0);
    wrong=lease; wrong.seat=2;
    raw_mode(server,&source,&wrong,103,UINT32_MAX,UINT32_MAX-2u,UINT32_MAX-1u,0);
    raw_mode(foreign,&source,&lease,104,UINT32_MAX,UINT32_MAX-2u,UINT32_MAX-1u,0);
    /* Neither altered repeats, older events nor backward event times replace state. */
    raw_mode(server,&source,&lease,105,UINT32_MAX-1u,UINT32_MAX-20u,UINT32_MAX-4u,0);
    raw_mode(server,&source,&lease,106,UINT32_MAX-1u,UINT32_MAX-19u,UINT32_MAX-4u,1);
    raw_mode(server,&source,&lease,107,UINT32_MAX-2u,UINT32_MAX-2u,UINT32_MAX-1u,0);
    raw_mode(server,&source,&lease,108,UINT32_MAX,UINT32_MAX-6u,UINT32_MAX-1u,0);
    raw_mode(server,&source,&lease,109,0,UINT32_MAX-2u,UINT32_MAX-1u,0);
    raw_mode(server,&source,&lease,110,UINT32_MAX,UINT32_MAX-2u,UINT32_MAX-1u,2);
    uint8_t bad[14]={0}; put32(bad,UINT32_MAX); put32(bad+4,UINT32_MAX-2u); put32(bad+8,UINT32_MAX-1u);
    raw_send(server,&source,17,1,lease.generation,lease.token,111,0,bad,12,SUDEKIMP_LAN_PARTY_VERSION);
    raw_send(server,&source,17,1,lease.generation,lease.token,112,0,bad,14,SUDEKIMP_LAN_PARTY_VERSION);
    raw_send(server,&source,17,1,lease.generation,lease.token,113,1,bad,13,SUDEKIMP_LAN_PARTY_VERSION);
    raw_send(server,&source,17,1,lease.generation,lease.token,114,0,bad,13,7);
    SudekiMpLanPartyPoll(client,local);
    CHECK(SudekiMpLanPartyGetCombatMode(client,&lease,local,&mode));
    CHECK(mode.sequence==saved.sequence && mode.observed_tick==saved.observed_tick && mode.enabled==saved.enabled);
    /* Exact repetitions cannot keep a stopped host game clock fresh. */
    raw_mode(server,&source,&lease,115,saved.sequence,saved.host_tick,saved.observed_tick,saved.enabled);
    SudekiMpLanPartyPoll(client,local+400u);
    mode.sequence=42;
    CHECK(!SudekiMpLanPartyGetCombatMode(client,&lease,local+501u,&mode));
    CHECK(mode.sequence==42u); /* Failure does not manufacture a noncombat output. */
    raw_mode(server,&source,&lease,116,UINT32_MAX,UINT32_MAX-2u,UINT32_MAX-1u,0);
    SudekiMpLanPartyPoll(client,local+501u);
    CHECK(SudekiMpLanPartyGetCombatMode(client,&lease,local+501u,&mode));
    CHECK(mode.sequence==UINT32_MAX && !mode.enabled);
    raw_mode(server,&source,&lease,117,1u,0u,0u,1);
    SudekiMpLanPartyPoll(client,local+502u);
    CHECK(SudekiMpLanPartyGetCombatMode(client,&lease,local+502u,&mode));
    CHECK(mode.sequence==1u && mode.host_tick==0u && mode.enabled);
    raw_mode(server,&source,&lease,118,UINT32_MAX,UINT32_MAX-2u,1u,0);
    SudekiMpLanPartyPoll(client,local+503u);
    CHECK(SudekiMpLanPartyGetCombatMode(client,&lease,local+503u,&mode));
    CHECK(mode.sequence==1u && mode.enabled);
    /* Local receive-age expiry also works through GetTickCount wrap. */
    raw_mode(server,&source,&lease,119,1u,0u,1u,1);
    SudekiMpLanPartyPoll(client,UINT32_MAX-100u);
    CHECK(SudekiMpLanPartyGetCombatMode(client,&lease,399u,&mode));
    CHECK(!SudekiMpLanPartyGetCombatMode(client,&lease,400u,&mode));
    raw_send(server,&source,RAW_END,1,lease.generation,lease.token,200,0,NULL,0,SUDEKIMP_LAN_PARTY_VERSION);
    SudekiMpLanPartyPoll(client,local+504u);
    CHECK(!SudekiMpLanPartyGetCombatMode(client,&lease,local+504u,&mode));
    closesocket(foreign); closesocket(server); SudekiMpLanPartyDestroy(client,FALSE);
}
static void raw_encode_presence(uint8_t *b,const SudekiMpLanPartyPresence *p) {
    const SudekiMpPartyOwnership *o=&p->ownership;
    const SudekiMpPartyAssignment *a=&o->assignment;
    put32(b,p->sequence); put32(b+4,p->observed_tick);
    put32(b+8,a->world); put32(b+12,a->revision);
    for(unsigned i=0;i<4;++i) put32(b+16+4*i,a->generation[i]);
    b[32]=a->humans; b[33]=a->available; memcpy(b+34,a->character,4);
    for(unsigned i=0;i<4;++i) put32(b+38+4*i,o->last_request[i]);
    put32(b+54,o->request); b[58]=o->player; b[59]=o->previous;
    b[60]=o->target; b[61]=o->requester; b[62]=o->displaced;
    b[63]=o->handoff_control; b[64]=(uint8_t)o->phase;
    b[65]=o->connected; b[66]=o->controlling; b[67]=o->menu;
    b[68]=o->away; b[69]=o->paused;
    for(unsigned i=0;i<4;++i) b[70+i]=(uint8_t)o->control[i];
    b[74]=(uint8_t)o->absence_policy;
    for(unsigned i=0;i<4;++i) put32(b+75+4*i,p->ack_request[i]);
    memcpy(b+91,p->ack_result,4);
}
static void test_presence_frame_floor(void) {
    for(unsigned wrapped=0;wrapped<2;++wrapped) {
        struct sockaddr_in address,source; uint8_t bytes[1468],body[95];
        SOCKET server=raw_socket(&address);
        SudekiMpLanPartyConfig c=config; c.local_seat=1; c.port=ntohs(address.sin_port);
        SudekiMpLanPartySession *client=SudekiMpLanPartyCreate(&c); CHECK(client!=NULL);
        now=GetTickCount(); SudekiMpLanPartyPoll(client,now);
        CHECK(raw_read(server,bytes,&source)==RAW_HEADER+47);
        uint8_t ack[9]; put64(ack,get64(bytes+RAW_HEADER)); ack[8]=SUDEKIMP_LAN_PARTY_ACTIVE;
        SudekiMpLanPartyLease lease={UINT64_C(0x1020304050607080),21,1};
        raw_send(server,&source,RAW_ACK,1,lease.generation,lease.token,1,0,ack,9,SUDEKIMP_LAN_PARTY_VERSION);
        SudekiMpLanPartyPoll(client,now); CHECK(SudekiMpLanPartyLeaseActive(client,&lease));
        SudekiMpPartyAssignment assignment={0};
        assignment.world=9; assignment.revision=1; assignment.available=assignment.humans=15;
        for(unsigned i=0;i<4;++i) { assignment.character[i]=(uint8_t)i; assignment.generation[i]=1; }
        SudekiMpLanPartyPresence presence={0};
        CHECK(SudekiMpPartyOwnershipInitializePresence(&presence.ownership,&assignment,15,15));
        CHECK(SudekiMpPartySetPaused(&presence.ownership,0,1,9,1,1)==SUDEKIMP_PARTY_SWAP_OK);
        presence.sequence=1; presence.observed_tick=wrapped?UINT32_MAX-4u:100u;
        raw_encode_presence(body,&presence);
        raw_send(server,&source,21,1,lease.generation,lease.token,2,0,body,95,SUDEKIMP_LAN_PARTY_VERSION);
        SudekiMpLanPartyPoll(client,now);
        CHECK(SudekiMpPartySetPaused(&presence.ownership,0,2,9,2,0)==SUDEKIMP_PARTY_SWAP_OK);
        ++presence.sequence; presence.observed_tick+=10;
        raw_encode_presence(body,&presence);
        raw_send(server,&source,21,1,lease.generation,lease.token,3,0,body,95,SUDEKIMP_LAN_PARTY_VERSION);
        SudekiMpLanPartyPoll(client,now);
        SudekiMpLanPartyFrame frame,result; fill_frame(&frame);
        frame.chunk[0].host_tick=frame.chunk[1].host_tick=presence.observed_tick-5;
        raw_frame(server,&source,&lease,&frame,40,0); raw_frame(server,&source,&lease,&frame,40,1);
        SudekiMpLanPartyPoll(client,now); CHECK(!SudekiMpLanPartyTakeFrame(client,&result));
        frame.chunk[0].host_tick=frame.chunk[1].host_tick=presence.observed_tick;
        raw_frame(server,&source,&lease,&frame,41,1); raw_frame(server,&source,&lease,&frame,41,0);
        SudekiMpLanPartyPoll(client,now); CHECK(SudekiMpLanPartyTakeFrame(client,&result));
        CHECK(result.chunk[0].host_tick==presence.observed_tick);
        raw_send(server,&source,RAW_END,1,lease.generation,lease.token,50,0,NULL,0,SUDEKIMP_LAN_PARTY_VERSION);
        SudekiMpLanPartyPoll(client,now); CHECK(SudekiMpLanPartyClientRejoin(client));
        /* Rejoining clears the prior observation floor and requires new lease. */
        SudekiMpLanPartyPoll(client,now+1);
        int length;
        do { length=raw_read(server,bytes,&source); } while(length>0 && bytes[6]!=RAW_HELLO);
        CHECK(length==RAW_HEADER+47); put64(ack,get64(bytes+RAW_HEADER)); ++lease.generation;
        raw_send(server,&source,RAW_ACK,1,lease.generation,lease.token,1,0,ack,9,SUDEKIMP_LAN_PARTY_VERSION);
        SudekiMpLanPartyPoll(client,now+1);
        frame.chunk[0].host_tick=frame.chunk[1].host_tick=1;
        raw_frame(server,&source,&lease,&frame,2,0); raw_frame(server,&source,&lease,&frame,2,1);
        SudekiMpLanPartyPoll(client,now+1); CHECK(SudekiMpLanPartyTakeFrame(client,&result));
        closesocket(server); SudekiMpLanPartyDestroy(client,FALSE);
    }
}
static void test_fragment_admission(void) {
    struct sockaddr_in address, source; uint8_t bytes[1468];
    SOCKET server = raw_socket(&address);
    SudekiMpLanPartyConfig c = config; c.local_seat = 1; c.port = ntohs(address.sin_port);
    SudekiMpLanPartySession *client = SudekiMpLanPartyCreate(&c); CHECK(client != NULL);
    now = GetTickCount(); SudekiMpLanPartyPoll(client, now);
    CHECK(raw_read(server, bytes, &source) == RAW_HEADER + 47);
    uint64_t nonce = get64(bytes + RAW_HEADER);
    SudekiMpLanPartyLease lease = {UINT64_C(0x1020304050607080), 19, 1};
    uint8_t ack[9]; put64(ack, nonce); ack[8] = SUDEKIMP_LAN_PARTY_ACTIVE;
    raw_send(server, &source, RAW_ACK, 1, lease.generation, lease.token, 1,
        0, ack, sizeof(ack), SUDEKIMP_LAN_PARTY_VERSION);
    SudekiMpLanPartyPoll(client, now);
    CHECK(SudekiMpLanPartyLeaseActive(client, &lease));
    SudekiMpLanPartyFrame frame, result; fill_frame(&frame);
    frame.chunk[0].enemy_count = 1;
    frame.chunk[0].enemies[0].native_entity_id = SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID;
    frame.chunk[0].enemies[0].hp = 100;
    raw_frame(server, &source, &lease, &frame, 10, 1);
    SudekiMpLanPartyPoll(client, now); CHECK(!SudekiMpLanPartyTakeFrame(client, &result));
    raw_frame(server, &source, &lease, &frame, 10, 0);
    SudekiMpLanPartyPoll(client, now); CHECK(SudekiMpLanPartyTakeFrame(client, &result));
    CHECK(result.chunk[0].enemy_count == 1 && result.chunk[1].seat[1].actor_type == 1);
    raw_frame(server, &source, &lease, &frame, 10, 0);
    raw_frame(server, &source, &lease, &frame, 10, 1);
    SudekiMpLanPartyPoll(client, now); CHECK(!SudekiMpLanPartyTakeFrame(client, &result));
    /* Mixing the two actors from different ticks must never publish. */
    raw_frame(server, &source, &lease, &frame, 11, 0);
    frame.chunk[1].host_tick++;
    raw_frame(server, &source, &lease, &frame, 11, 1);
    SudekiMpLanPartyPoll(client, now); CHECK(!SudekiMpLanPartyTakeFrame(client, &result));
    frame.chunk[0].host_tick++;
    raw_frame(server, &source, &lease, &frame, 12, 0); /* lost half */
    raw_frame(server, &source, &lease, &frame, 13, 1);
    raw_frame(server, &source, &lease, &frame, 13, 0);
    SudekiMpLanPartyPoll(client, now); CHECK(SudekiMpLanPartyTakeFrame(client, &result));
    CHECK(result.chunk[0].sequence == 13);
    raw_frame(server, &source, &lease, &frame, 12, 1);
    SudekiMpLanPartyPoll(client, now); CHECK(!SudekiMpLanPartyTakeFrame(client, &result));
    /* Unknown acknowledgements are not accepted as simulation proof. */
    frame.chunk[0].acknowledged_input = frame.chunk[1].acknowledged_input = 999;
    raw_frame(server, &source, &lease, &frame, 14, 0); raw_frame(server, &source, &lease, &frame, 14, 1);
    SudekiMpLanPartyPoll(client, now); CHECK(!SudekiMpLanPartyTakeFrame(client, &result));
    frame.chunk[0].acknowledged_input = frame.chunk[1].acknowledged_input = 0;
    SudekiMpLanPartyLease stale = lease; --stale.generation;
    raw_frame(server, &source, &stale, &frame, 15, 0); raw_frame(server, &source, &stale, &frame, 15, 1);
    SudekiMpLanPartyPoll(client, now); CHECK(!SudekiMpLanPartyTakeFrame(client, &result));
    for (unsigned seq = 20; seq < 30; ++seq) {
        frame.chunk[0].host_tick = frame.chunk[1].host_tick = 1000 + seq;
        raw_frame(server, &source, &lease, &frame, seq, 0);
        raw_frame(server, &source, &lease, &frame, seq, 1);
        SudekiMpLanPartyPoll(client, now);
    }
    for (unsigned seq = 22; seq < 30; ++seq) {
        CHECK(SudekiMpLanPartyTakeFrame(client, &result)); CHECK(result.chunk[0].sequence == seq);
    }
    CHECK(!SudekiMpLanPartyTakeFrame(client, &result));
    /* Even a new transport sequence cannot rewind the canonical world. */
    frame.chunk[0].host_tick = frame.chunk[1].host_tick = 1002;
    raw_frame(server, &source, &lease, &frame, 30, 0);
    raw_frame(server, &source, &lease, &frame, 30, 1);
    SudekiMpLanPartyPoll(client, now); CHECK(!SudekiMpLanPartyTakeFrame(client, &result));
    raw_send(server, &source, RAW_END, 1, lease.generation, lease.token, 31, 0, NULL, 0, SUDEKIMP_LAN_PARTY_VERSION);
    SudekiMpLanPartyPoll(client, now);
    CHECK(!SudekiMpLanPartyLeaseActive(client, &lease));
    CHECK(!SudekiMpLanPartyTakeFrame(client, &result));
    closesocket(server); SudekiMpLanPartyDestroy(client, FALSE);
}

static void publish_presence(SudekiMpLanPartyPresence *presence) {
    ++presence->sequence; presence->observed_tick=GetTickCount();
    CHECK(SudekiMpLanPartyPublishPresence(nodes[0],presence)); pump();
    for(unsigned i=1;i<4;++i) {
        SudekiMpLanPartyPresence copy;
        CHECK(SudekiMpLanPartyGetPresence(nodes[i],&copy));
        CHECK(copy.sequence==presence->sequence);
        CHECK(copy.ownership.assignment.revision==presence->ownership.assignment.revision);
        CHECK(copy.ownership.controlling==presence->ownership.controlling);
        CHECK(copy.ownership.paused==presence->ownership.paused);
    }
}
static void test_dynamic_presence(void) {
    static const uint8_t map[4]={1,0,3,2};
    SudekiMpPartyAssignment assignment={0};
    SudekiMpLanPartyPresence presence={0},copy;
    SudekiMpLanPartyCommand command={0},taken;
    SudekiMpLanArenaInput input={0}; SudekiMpLanPartyInput admitted;
    memset(&config,0,sizeof(config)); memset(config.game_hash,0x19,sizeof(config.game_hash));
    config.assignment_enabled=1; memcpy(config.character,map,4);
    nodes[0]=SudekiMpLanPartyCreate(&config); CHECK(nodes[0]!=NULL);
    config.port=SudekiMpLanPartyPort(nodes[0]); config.host_ipv4="127.0.0.1";
    for(unsigned i=1;i<4;++i) {
        config.local_seat=(uint8_t)i; nodes[i]=SudekiMpLanPartyCreate(&config);
        CHECK(nodes[i]!=NULL);
    }
    pump();
    for(unsigned i=1;i<4;++i) {
        wait_transport(i);
        SudekiMpLanPartyPeerStatus peer=status(0,i);
        CHECK(SudekiMpLanPartyApprove(nodes[0],&peer.lease));
    }
    pump();
    for(unsigned i=0;i<4;++i) {
        CHECK(SudekiMpLanPartyLocalSeat(nodes[i])==i);
        CHECK(SudekiMpLanPartyLocalCharacter(nodes[i])==map[i]);
        CHECK(SudekiMpLanPartyCharacterPlayer(nodes[0],map[i])==i);
    }
    input.actor_type=SudekiMpLanPartyActorType(0); input.world_direction_x=1000;
    CHECK(!SudekiMpLanPartySendInput(nodes[1],&input)); /* no confirmed ownership yet */
    assignment.world=7; assignment.revision=1; assignment.available=15; assignment.humans=15;
    memcpy(assignment.character,map,4);
    for(unsigned i=0;i<4;++i) assignment.generation[i]=5;
    CHECK(SudekiMpPartyOwnershipInitializePresence(&presence.ownership,&assignment,15,15));
    publish_presence(&presence);
    CHECK(!SudekiMpLanPartyPublishPresence(nodes[1],&presence));
    CHECK(SudekiMpLanPartySendInput(nodes[1],&input)); pump();
    CHECK(SudekiMpLanPartyTakeInput(nodes[0],1,&admitted));
    CHECK(admitted.lease.seat==1 && admitted.input.actor_type==SudekiMpLanPartyActorType(0));
    CHECK(admitted.world==7 && admitted.revision==1 && admitted.actor_generation==5);
    CHECK(SudekiMpLanPartyAdmitInput(nodes[0],&admitted));
    input.actor_type=SudekiMpLanPartyActorType(1);
    CHECK(!SudekiMpLanPartySendInput(nodes[1],&input)); input.actor_type=SudekiMpLanPartyActorType(0);
    CHECK(SudekiMpLanPartySendInput(nodes[1],&input)); pump();
    CHECK(SudekiMpLanPartyTakeInput(nodes[0],1,&admitted));
    SudekiMpLanPartyLease lease=admitted.lease;
    CHECK(SudekiMpPartySetPaused(&presence.ownership,0,1,7,1,1)==SUDEKIMP_PARTY_SWAP_OK);
    publish_presence(&presence);
    CHECK(!SudekiMpLanPartyAdmitInput(nodes[0],&admitted));
    CHECK(!SudekiMpLanPartySendInput(nodes[1],&input));
    CHECK(SudekiMpLanPartyLeaseActive(nodes[0],&lease)); /* role and token unchanged */
    CHECK(SudekiMpPartySetPaused(&presence.ownership,0,2,7,2,0)==SUDEKIMP_PARTY_SWAP_OK);
    publish_presence(&presence);
    CHECK(SudekiMpLanPartySendInput(nodes[1],&input)); pump();
    CHECK(SudekiMpLanPartyTakeInput(nodes[0],1,&admitted));
    CHECK(admitted.revision==3 && SudekiMpLanPartyAdmitInput(nodes[0],&admitted));
    command.kind=SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE; command.player=1; command.target=4;
    command.value=1; command.request=1; command.world=7; command.revision=3; command.generation=5;
    CHECK(SudekiMpLanPartyQueueCommand(nodes[1],&command));
    CHECK(SudekiMpLanPartyQueueCommand(nodes[1],&command)); /* identical retry */
    pump(); CHECK(SudekiMpLanPartyTakeCommand(nodes[0],&taken));
    CHECK(taken.lease.token==lease.token && taken.lease.generation==lease.generation &&
        taken.lease.seat==1 && taken.player==1 && taken.request==1);
    CHECK(!SudekiMpLanPartyTakeCommand(nodes[0],&taken));
    CHECK(SudekiMpPartyPresenceRequest(&presence.ownership,1,1,7,3,1,0)==SUDEKIMP_PARTY_SWAP_OK);
    presence.ack_request[1]=1; publish_presence(&presence);
    CHECK(!SudekiMpLanPartySendInput(nodes[1],&input));
    CHECK(SudekiMpPartyControlDrainComplete(&presence.ownership,1,7,5));
    publish_presence(&presence);
    command.request=2; command.revision=presence.ownership.assignment.revision;
    command.generation=6; command.value=0;
    CHECK(SudekiMpLanPartyQueueCommand(nodes[1],&command)); pump();
    CHECK(SudekiMpLanPartyTakeCommand(nodes[0],&taken));
    CHECK(SudekiMpPartyPresenceRequest(&presence.ownership,1,2,7,command.revision,0,0)==SUDEKIMP_PARTY_SWAP_OK);
    presence.ack_request[1]=2; publish_presence(&presence);
    CHECK(!SudekiMpLanPartySendInput(nodes[1],&input));
    CHECK(SudekiMpPartyControlAcquireBegin(&presence.ownership,1,7,presence.ownership.assignment.revision,6));
    publish_presence(&presence);
    CHECK(SudekiMpPartyControlAcquireCommit(&presence.ownership,1,7,6)); publish_presence(&presence);
    CHECK(!SudekiMpLanPartySendInput(nodes[1],&input));
    command.kind=SUDEKIMP_LAN_PARTY_COMMAND_CONTROL_ACK; command.request=3;
    command.revision=presence.ownership.assignment.revision; command.generation=7;
    CHECK(SudekiMpLanPartyQueueCommand(nodes[1],&command)); pump();
    CHECK(SudekiMpLanPartyTakeCommand(nodes[0],&taken));
    CHECK(!SudekiMpPartyControlAcknowledge(&presence.ownership,1,7,command.revision,6));
    CHECK(SudekiMpPartyControlAcknowledge(&presence.ownership,1,7,command.revision,7));
    presence.ack_request[1]=3; publish_presence(&presence);
    CHECK(SudekiMpLanPartySendInput(nodes[1],&input)); pump();
    CHECK(SudekiMpLanPartyTakeInput(nodes[0],1,&admitted));
    CHECK(admitted.actor_generation==7 && SudekiMpLanPartyAdmitInput(nodes[0],&admitted));
    command.kind=SUDEKIMP_LAN_PARTY_COMMAND_PAUSE; command.request=4; command.value=1;
    CHECK(!SudekiMpLanPartyQueueCommand(nodes[1],&command)); /* peer cannot manufacture host op */
    command.player=0; CHECK(!SudekiMpLanPartyQueueCommand(nodes[1],&command));
    CHECK(SudekiMpLanPartyQueueCommand(nodes[0],&command));
    CHECK(SudekiMpLanPartyTakeCommand(nodes[0],&taken));
    CHECK(taken.player==0 && !taken.lease.token);
    copy=presence; ++copy.sequence; --copy.ownership.assignment.generation[0];
    CHECK(!SudekiMpLanPartyPublishPresence(nodes[0],&copy));
    CHECK(!SudekiMpLanPartySetAssignment(nodes[1],&assignment));
    assignment=presence.ownership.assignment; ++assignment.revision;
    CHECK(SudekiMpLanPartySetAssignment(nodes[0],&assignment));
    CHECK(!SudekiMpLanPartyAdmitInput(nodes[0],&admitted));
    CHECK(!SudekiMpLanPartyGetPresence(nodes[0],&copy)); /* explicit fence until full publication */
    for(unsigned i=0;i<4;++i) { SudekiMpLanPartyDestroy(nodes[i],FALSE); nodes[i]=NULL; }
}
static void test_runtime_admission(void) {
    SudekiMpLanPartyConfig c={0};
    SudekiMpLanPartyPeerStatus peer;
    c.lobby_members=1; c.lobby_nonce[0]=11; c.assignment_enabled=1;
    c.character[0]=2; c.character[1]=c.character[2]=c.character[3]=4;
    SudekiMpLanPartySession *host=SudekiMpLanPartyCreate(&c); CHECK(host!=NULL);
    CHECK(SudekiMpLanPartyRegisterAdmission(host,1,22));
    CHECK(!SudekiMpLanPartyRegisterAdmission(host,2,22));
    CHECK(!SudekiMpLanPartyRegisterAdmission(host,1,33));
    CHECK(!SudekiMpLanPartyRevokeAdmission(host,1,33));
    CHECK(SudekiMpLanPartyRevokeAdmission(host,1,22));
    CHECK(SudekiMpLanPartyRegisterAdmission(host,1,33));
    c.port=SudekiMpLanPartyPort(host); c.host_ipv4="127.0.0.1"; c.local_seat=1;
    c.lobby_members=3; c.lobby_nonce[0]=0; c.lobby_nonce[1]=33;
    SudekiMpLanPartySession *client=SudekiMpLanPartyCreate(&c); CHECK(client!=NULL);
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(client,GetTickCount()); SudekiMpLanPartyPoll(host,GetTickCount());
    }
    CHECK(SudekiMpLanPartyPeerStatusGet(host,1,&peer));
    CHECK(peer.phase==SUDEKIMP_LAN_PARTY_PENDING);
    CHECK(!SudekiMpLanPartyRegisterAdmission(host,1,44));
    CHECK(!SudekiMpLanPartyRevokeAdmission(host,1,33));
    CHECK(!SudekiMpLanPartyRegisterAdmission(client,2,44));
    CHECK(SudekiMpLanPartyDisconnect(host,&peer.lease));
    CHECK(!SudekiMpLanPartyRevokeAdmission(host,1,33));
    CHECK(SudekiMpLanPartyReleaseDrained(host,&peer.lease));
    CHECK(SudekiMpLanPartyRevokeAdmission(host,1,33));
    CHECK(SudekiMpLanPartyRegisterAdmission(host,1,44));
    SudekiMpLanPartyDestroy(client,FALSE); SudekiMpLanPartyDestroy(host,FALSE);
}

/* Uses exactly the command-construction entrypoint called by the runtime.
 * The raw transport still rejects noncanonical target values. */
static void test_local_command_builder(void) {
    SudekiMpLanPartyConfig config={0};
    SudekiMpLanPartyPresenceCoordinator coordinator={0};
    SudekiMpLanPartyCommand command;
    CHECK(!SudekiMpLanPartyPresenceQueueLocal(&coordinator,
        SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,1,0));
    CHECK(GetLastError()==ERROR_INVALID_STATE);
    SudekiMpLanPartySession *host=SudekiMpLanPartyCreate(&config);
    CHECK(host && SudekiMpLanPartyPresenceInitialize(&coordinator,host,&config));
    static const struct { SudekiMpLanPartyCommandKind kind; unsigned value; uint32_t transaction; } cases[]={
        {SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,1,0},
        {SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,0,0},
        {SUDEKIMP_LAN_PARTY_COMMAND_POLICY,1,0},
        {SUDEKIMP_LAN_PARTY_COMMAND_PAUSE,1,0},
        {SUDEKIMP_LAN_PARTY_COMMAND_PAUSE,0,0},
        {SUDEKIMP_LAN_PARTY_COMMAND_REQUEST_PAUSE,0,0},
        {SUDEKIMP_LAN_PARTY_COMMAND_CONTROL_ACK,0,0},
        {SUDEKIMP_LAN_PARTY_COMMAND_SWAP_ACK,0,91}
    };
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        uint32_t request=coordinator.next_request;
        SudekiMpLanPartyPresence observed=coordinator.current;
        BOOL acknowledgment=cases[i].kind==SUDEKIMP_LAN_PARTY_COMMAND_CONTROL_ACK ||
            cases[i].kind==SUDEKIMP_LAN_PARTY_COMMAND_SWAP_ACK;
        CHECK(acknowledgment ? SudekiMpLanPartyPresenceQueueAcknowledgment(&coordinator,
            &observed,cases[i].kind,cases[i].transaction) :
            SudekiMpLanPartyPresenceQueueLocal(&coordinator,cases[i].kind,
                cases[i].value,cases[i].transaction));
        CHECK(GetLastError()==ERROR_SUCCESS);
        CHECK(!SudekiMpLanPartyPresenceQueueLocal(&coordinator,
            SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,1,0));
        CHECK(GetLastError()==ERROR_BUSY && coordinator.next_request==request+1);
        CHECK(SudekiMpLanPartyTakeCommand(host,&command));
        CHECK(command.kind==cases[i].kind && command.target==SUDEKIMP_PARTY_NO_CHARACTER &&
            command.value==cases[i].value && command.transaction==cases[i].transaction &&
            command.request==request && SudekiMpLanPartyCommandValid(&command));
        CHECK(command.world==observed.ownership.assignment.world &&
            command.revision==observed.ownership.assignment.revision &&
            command.generation==observed.ownership.assignment.generation[0]);
        CHECK(!SudekiMpLanPartyQueueCommand(host,&command));
        CHECK(GetLastError()==ERROR_RETRY);
        command.target=0;
        CHECK(!SudekiMpLanPartyQueueCommand(host,&command));
        CHECK(GetLastError()==ERROR_INVALID_PARAMETER);
    }
    uint32_t request=coordinator.next_request;
    CHECK(!SudekiMpLanPartyPresenceQueueLocal(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_SWAP,0,0));
    CHECK(GetLastError()==ERROR_INVALID_PARAMETER);
    CHECK(!SudekiMpLanPartyPresenceQueueLocal(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_RELEASE,0,0));
    CHECK(GetLastError()==ERROR_INVALID_PARAMETER);
    CHECK(!SudekiMpLanPartyPresenceQueueLocal(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_REASSIGN,0,0));
    CHECK(GetLastError()==ERROR_INVALID_PARAMETER && coordinator.next_request==request);
    CHECK(!SudekiMpLanPartyPresenceQueueLocal(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_CONTROL_ACK,0,0));
    CHECK(GetLastError()==ERROR_INVALID_PARAMETER);
    CHECK(!SudekiMpLanPartyPresenceQueueLocal(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_SWAP_ACK,0,91));
    CHECK(GetLastError()==ERROR_INVALID_PARAMETER);
    CHECK(!SudekiMpLanPartyPresenceQueueAcknowledgment(&coordinator,&coordinator.current,
        SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,0));
    CHECK(GetLastError()==ERROR_INVALID_PARAMETER && coordinator.next_request==request);
    CHECK(!SudekiMpLanPartyPresenceQueueLocal(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,4,0));
    CHECK(GetLastError()==ERROR_INVALID_PARAMETER && coordinator.next_request==request);
    coordinator.next_request=UINT32_MAX;
    CHECK(!SudekiMpLanPartyPresenceQueueLocal(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,0,0));
    CHECK(GetLastError()==ERROR_ARITHMETIC_OVERFLOW);
    SudekiMpLanPartyDestroy(host,FALSE);
}

static void test_presence_coordinator(void) {
    SudekiMpLanPartyConfig c={0}; c.assignment_enabled=1;
    c.character[0]=2; c.character[1]=c.character[2]=c.character[3]=4;
    SudekiMpLanPartySession *host=SudekiMpLanPartyCreate(&c);
    SudekiMpLanPartyPresenceCoordinator coordinator;
    SudekiMpLanPartyPresence state;
    SudekiMpLanPartyPresenceNativeState native={0};
    CHECK(host!=NULL);
    CHECK(SudekiMpLanPartyPresenceInitialize(&coordinator,host,&c));
    CHECK(SudekiMpLanPartyPresenceRead(&coordinator,&state));
    CHECK(state.ownership.connected==1 && state.ownership.controlling==1 &&
        state.ownership.assignment.character[0]==2);
    CHECK(SudekiMpLanPartyPresenceQueueLocal(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,1,0));
    CHECK(SudekiMpLanPartyPresenceUiFrame(&coordinator,GetTickCount()));
    CHECK(SudekiMpLanPartyPresenceRead(&coordinator,&state));
    CHECK(!state.ownership.paused && state.ownership.control[0]==SUDEKIMP_PARTY_CONTROL_DRAINING);
    native.assignment=state.ownership.assignment; native.connected_mask=1;
    native.native_owned_mask=1; native.local_bound_mask=1;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.control[0]==SUDEKIMP_PARTY_CONTROL_DRAINING);
    native.native_owned_mask=0; native.native_ai_mask=1;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.control[0]==SUDEKIMP_PARTY_CONTROL_AI);
    CHECK(SudekiMpLanPartyPresenceQueueLocal(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,0,0));
    CHECK(SudekiMpLanPartyPresenceUiFrame(&coordinator,GetTickCount()));
    /* An observation for the retired native generation cannot prove ready. */
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.control[0]==SUDEKIMP_PARTY_CONTROL_AI);
    native.assignment=coordinator.current.ownership.assignment;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.control[0]==SUDEKIMP_PARTY_CONTROL_ACQUIRING);
    native.native_ai_mask=0; native.native_owned_mask=1;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.control[0]==SUDEKIMP_PARTY_CONTROL_WAIT_ACK);
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.control[0]==SUDEKIMP_PARTY_CONTROL_WAIT_ACK);
    native.assignment=coordinator.current.ownership.assignment;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.control[0]==SUDEKIMP_PARTY_CONTROL_HUMAN);
    CHECK(SudekiMpLanPartyPresenceQueueLocal(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_POLICY,1,0));
    CHECK(SudekiMpLanPartyPresenceUiFrame(&coordinator,GetTickCount()));
    CHECK(SudekiMpLanPartyPresenceQueueLocal(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,2,0));
    CHECK(SudekiMpLanPartyPresenceUiFrame(&coordinator,GetTickCount()));
    CHECK(coordinator.current.ownership.paused);
    CHECK(SudekiMpLanPartyPresenceQueueLocal(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_PAUSE,0,0));
    CHECK(SudekiMpLanPartyPresenceUiFrame(&coordinator,GetTickCount()));
    CHECK(!coordinator.current.ownership.paused && coordinator.current.ownership.away==1);
    CHECK(SudekiMpLanPartyPresenceQueueLocal(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,2,0));
    CHECK(SudekiMpLanPartyPresenceUiFrame(&coordinator,GetTickCount()));
    CHECK(!coordinator.current.ownership.paused);
    CHECK(SudekiMpLanPartyPresenceReserve(&coordinator,1,3,GetTickCount()));
    CHECK(coordinator.current.ownership.assignment.character[1]==3 &&
        coordinator.current.ownership.connected==1 && coordinator.current.ownership.controlling==1);
    CHECK(!SudekiMpLanPartyPresenceReserve(&coordinator,2,3,GetTickCount()));
    CHECK(!SudekiMpLanPartyPresenceReserve(&coordinator,1,0,GetTickCount()));
    native.assignment=coordinator.current.ownership.assignment;
    native.native_owned_mask=0; native.native_ai_mask=1;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.control[0]==SUDEKIMP_PARTY_CONTROL_AI);
    CHECK(SudekiMpLanPartyPresenceQueueLocal(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,1,0));
    CHECK(SudekiMpLanPartyPresenceUiFrame(&coordinator,GetTickCount()));
    CHECK(SudekiMpLanPartyPresenceQueue(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_SWAP,0,0,0));
    CHECK(SudekiMpLanPartyPresenceUiFrame(&coordinator,GetTickCount()));
    CHECK(coordinator.current.ownership.phase==SUDEKIMP_PARTY_SWAP_RESERVED);
    native.assignment=coordinator.current.ownership.assignment;
    native.swap_request=coordinator.current.ownership.request; native.swap_ready=1;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.phase==SUDEKIMP_PARTY_SWAP_HANDOFF);
    native.swap_ready=0; native.swap_result=SUDEKIMP_LAN_PARTY_PRESENCE_SWAP_COMMITTED;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK &&
        coordinator.current.ownership.assignment.character[0]==0 &&
        coordinator.current.ownership.control[0]==SUDEKIMP_PARTY_CONTROL_AI);
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK); /* old identity */
    native.assignment=coordinator.current.ownership.assignment;
    native.swap_result=SUDEKIMP_LAN_PARTY_PRESENCE_SWAP_NONE;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.phase==SUDEKIMP_PARTY_SWAP_IDLE &&
        !coordinator.current.ownership.controlling); /* menu keeps target under AI */
    CHECK(SudekiMpLanPartyPresenceQueueLocal(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,0,0));
    CHECK(SudekiMpLanPartyPresenceUiFrame(&coordinator,GetTickCount()));
    native.assignment=coordinator.current.ownership.assignment;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.control[0]==SUDEKIMP_PARTY_CONTROL_ACQUIRING);
    native.native_ai_mask=0; native.native_owned_mask=1;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    native.assignment=coordinator.current.ownership.assignment;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.control[0]==SUDEKIMP_PARTY_CONTROL_HUMAN);
    CHECK(SudekiMpLanPartyPresenceQueue(&coordinator,SUDEKIMP_LAN_PARTY_COMMAND_SWAP,1,0,0));
    CHECK(SudekiMpLanPartyPresenceUiFrame(&coordinator,GetTickCount()));
    native.assignment=coordinator.current.ownership.assignment;
    native.swap_request=coordinator.current.ownership.request; native.swap_ready=1;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    native.swap_ready=0; native.swap_result=SUDEKIMP_LAN_PARTY_PRESENCE_SWAP_COMMITTED;
    native.native_ai_mask=1; native.native_owned_mask=0;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK &&
        coordinator.current.ownership.control[0]==SUDEKIMP_PARTY_CONTROL_AI &&
        !coordinator.current.ownership.controlling);
    native.assignment=coordinator.current.ownership.assignment;
    native.swap_result=SUDEKIMP_LAN_PARTY_PRESENCE_SWAP_NONE;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.control[0]==SUDEKIMP_PARTY_CONTROL_ACQUIRING);
    native.native_ai_mask=0; native.native_owned_mask=1;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.control[0]==SUDEKIMP_PARTY_CONTROL_WAIT_ACK);
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK);
    native.assignment=coordinator.current.ownership.assignment;
    CHECK(SudekiMpLanPartyPresenceService(&coordinator,&native,GetTickCount()));
    CHECK(coordinator.current.ownership.phase==SUDEKIMP_PARTY_SWAP_IDLE &&
        coordinator.current.ownership.control[0]==SUDEKIMP_PARTY_CONTROL_HUMAN);
    SudekiMpLanPartyPresenceReset(&coordinator);
    CHECK(!SudekiMpLanPartyPresenceRead(&coordinator,&state));
    SudekiMpLanPartyDestroy(host,FALSE);
}

static void test_loading_assignment_gate(void) {
    SudekiMpLanPartyConfig config={0};
    SudekiMpLanPartyPresenceCoordinator host_state, client_state;
    SudekiMpLanPartyPresenceNativeState native={0};
    SudekiMpLanPartyPeerStatus peer;
    config.assignment_enabled=1; config.lobby_members=3;
    config.character[0]=0; config.character[1]=1;
    config.character[2]=config.character[3]=4;
    config.lobby_nonce[0]=11; config.lobby_nonce[1]=22;
    SudekiMpLanPartySession *host=SudekiMpLanPartyCreate(&config);
    CHECK(host && SudekiMpLanPartyPresenceInitialize(&host_state,host,&config));
    config.local_seat=1; config.host_ipv4="127.0.0.1";
    config.lobby_nonce[0]=0;
    config.port=SudekiMpLanPartyPort(host);
    SudekiMpLanPartySession *client=SudekiMpLanPartyCreate(&config);
    CHECK(client && SudekiMpLanPartyPresenceInitialize(&client_state,client,&config));
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(client,GetTickCount()); SudekiMpLanPartyPoll(host,GetTickCount());
    }
    CHECK(SudekiMpLanPartyPeerStatusGet(host,1,&peer));
    CHECK(peer.phase==SUDEKIMP_LAN_PARTY_PENDING); /* still loading its first binding */
    native.assignment=host_state.current.ownership.assignment;
    native.connected_mask=3; native.native_owned_mask=1;
    native.native_ai_mask=2; native.local_bound_mask=1;
    CHECK(SudekiMpLanPartyPresenceService(&host_state,&native,GetTickCount()));
    CHECK(host_state.bound_players==1);
    CHECK(SudekiMpLanPartyPresenceQueue(&host_state,SUDEKIMP_LAN_PARTY_COMMAND_REASSIGN,1,2,0));
    CHECK(SudekiMpLanPartyPresenceUiFrame(&host_state,GetTickCount()));
    CHECK(host_state.current.ack_result[0]==SUDEKIMP_PARTY_SWAP_BUSY &&
        host_state.current.ownership.assignment.character[1]==1);
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(host,GetTickCount()); SudekiMpLanPartyPoll(client,GetTickCount());
    }
    CHECK(SudekiMpLanPartyPresenceQueueLocal(&client_state,SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,2,0));
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(client,GetTickCount()); SudekiMpLanPartyPoll(host,GetTickCount());
    }
    CHECK(SudekiMpLanPartyPresenceUiFrame(&host_state,GetTickCount()));
    native.assignment=host_state.current.ownership.assignment;
    CHECK(SudekiMpLanPartyPresenceService(&host_state,&native,GetTickCount()));
    CHECK(host_state.current.ownership.control[1]==SUDEKIMP_PARTY_CONTROL_AI);
    CHECK(SudekiMpLanPartyPresenceQueue(&host_state,SUDEKIMP_LAN_PARTY_COMMAND_RELEASE,1,0,0));
    CHECK(SudekiMpLanPartyPresenceUiFrame(&host_state,GetTickCount()));
    CHECK(host_state.current.ack_result[0]==SUDEKIMP_PARTY_SWAP_BUSY &&
        host_state.current.ownership.assignment.character[1]==1);
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(host,GetTickCount()); SudekiMpLanPartyPoll(client,GetTickCount());
    }
    CHECK(SudekiMpLanPartyPresenceQueueLocal(&client_state,SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,0,0));
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(client,GetTickCount()); SudekiMpLanPartyPoll(host,GetTickCount());
    }
    CHECK(SudekiMpLanPartyPresenceUiFrame(&host_state,GetTickCount()));
    native.assignment=host_state.current.ownership.assignment;
    CHECK(SudekiMpLanPartyPresenceService(&host_state,&native,GetTickCount()));
    CHECK(host_state.current.ownership.control[1]==SUDEKIMP_PARTY_CONTROL_ACQUIRING);
    native.native_ai_mask=0; native.native_owned_mask=3;
    native.assignment=host_state.current.ownership.assignment;
    CHECK(SudekiMpLanPartyPresenceService(&host_state,&native,GetTickCount()));
    CHECK(host_state.current.ownership.control[1]==SUDEKIMP_PARTY_CONTROL_WAIT_ACK);
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(host,GetTickCount()); SudekiMpLanPartyPoll(client,GetTickCount());
    }
    /* A host policy update can arrive after native binding was proved. The
     * acknowledgment must keep that proof's revision instead of restamping
     * itself with a newer policy that has never been observed natively. */
    SudekiMpLanPartyPresence observed;
    CHECK(SudekiMpLanPartyPresenceRead(&client_state,&observed));
    CHECK(SudekiMpLanPartyPresenceQueueLocal(&host_state,SUDEKIMP_LAN_PARTY_COMMAND_POLICY,0,0));
    CHECK(SudekiMpLanPartyPresenceUiFrame(&host_state,GetTickCount()));
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(host,GetTickCount()); SudekiMpLanPartyPoll(client,GetTickCount());
    }
    CHECK(SudekiMpLanPartyPresenceQueueAcknowledgment(&client_state,&observed,
        SUDEKIMP_LAN_PARTY_COMMAND_CONTROL_ACK,0));
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(client,GetTickCount()); SudekiMpLanPartyPoll(host,GetTickCount());
    }
    CHECK(SudekiMpLanPartyPresenceUiFrame(&host_state,GetTickCount()));
    CHECK(host_state.bound_players==1 &&
        host_state.current.ownership.control[1]==SUDEKIMP_PARTY_CONTROL_WAIT_ACK &&
        host_state.current.ack_result[1]==SUDEKIMP_PARTY_SWAP_STALE);
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(host,GetTickCount()); SudekiMpLanPartyPoll(client,GetTickCount());
    }
    CHECK(SudekiMpLanPartyPresenceRead(&client_state,&observed));
    CHECK(SudekiMpLanPartyPresenceQueueAcknowledgment(&client_state,&observed,
        SUDEKIMP_LAN_PARTY_COMMAND_CONTROL_ACK,0));
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(client,GetTickCount()); SudekiMpLanPartyPoll(host,GetTickCount());
    }
    CHECK(SudekiMpLanPartyPresenceUiFrame(&host_state,GetTickCount()));
    CHECK(host_state.bound_players==3 &&
        host_state.current.ownership.control[1]==SUDEKIMP_PARTY_CONTROL_HUMAN);
    CHECK(SudekiMpPartyPlayerInputReady(&host_state.current.ownership,1,1,
        host_state.current.ownership.assignment.world,
        host_state.current.ownership.assignment.revision,
        host_state.current.ownership.assignment.generation[1]));
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(host,GetTickCount()); SudekiMpLanPartyPoll(client,GetTickCount());
    }
    CHECK(SudekiMpLanPartyPresenceQueueLocal(&client_state,
        SUDEKIMP_LAN_PARTY_COMMAND_REQUEST_PAUSE,0,0));
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(client,GetTickCount()); SudekiMpLanPartyPoll(host,GetTickCount());
    }
    CHECK(SudekiMpLanPartyPresenceUiFrame(&host_state,GetTickCount()));
    CHECK(host_state.pause_requests==2 && !host_state.current.ownership.paused);
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(host,GetTickCount()); SudekiMpLanPartyPoll(client,GetTickCount());
    }
    CHECK(SudekiMpLanPartyPresenceQueueLocal(&client_state,SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,2,0));
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(client,GetTickCount()); SudekiMpLanPartyPoll(host,GetTickCount());
    }
    CHECK(SudekiMpLanPartyPresenceUiFrame(&host_state,GetTickCount()));
    native.assignment=host_state.current.ownership.assignment;
    native.native_owned_mask=1; native.native_ai_mask=2;
    CHECK(SudekiMpLanPartyPresenceService(&host_state,&native,GetTickCount()));
    CHECK(host_state.current.ownership.control[1]==SUDEKIMP_PARTY_CONTROL_AI);
    CHECK(SudekiMpLanPartyPresenceQueue(&host_state,SUDEKIMP_LAN_PARTY_COMMAND_REASSIGN,1,2,0));
    CHECK(SudekiMpLanPartyPresenceUiFrame(&host_state,GetTickCount()));
    CHECK(host_state.current.ack_result[0]==SUDEKIMP_PARTY_SWAP_OK &&
        host_state.current.ownership.phase==SUDEKIMP_PARTY_SWAP_RESERVED);
    native.assignment=host_state.current.ownership.assignment;
    native.swap_request=host_state.current.ownership.request; native.swap_ready=1;
    CHECK(SudekiMpLanPartyPresenceService(&host_state,&native,GetTickCount()));
    CHECK(host_state.current.ownership.phase==SUDEKIMP_PARTY_SWAP_HANDOFF);
    native.swap_ready=0; native.swap_result=SUDEKIMP_LAN_PARTY_PRESENCE_SWAP_COMMITTED;
    CHECK(SudekiMpLanPartyPresenceService(&host_state,&native,GetTickCount()));
    CHECK(host_state.current.ownership.phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK &&
        host_state.current.ownership.assignment.character[1]==2);
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(host,GetTickCount()); SudekiMpLanPartyPoll(client,GetTickCount());
    }
    CHECK(SudekiMpLanPartyPresenceRead(&client_state,&observed));
    CHECK(SudekiMpLanPartyPresenceQueueAcknowledgment(&client_state,&observed,
        SUDEKIMP_LAN_PARTY_COMMAND_SWAP_ACK,observed.ownership.request));
    for(unsigned i=0;i<3;++i) {
        SudekiMpLanPartyPoll(client,GetTickCount()); SudekiMpLanPartyPoll(host,GetTickCount());
    }
    CHECK(SudekiMpLanPartyPresenceUiFrame(&host_state,GetTickCount()));
    CHECK(host_state.current.ownership.phase==SUDEKIMP_PARTY_SWAP_IDLE &&
        host_state.current.ownership.control[1]==SUDEKIMP_PARTY_CONTROL_AI &&
        host_state.current.ack_result[1]==SUDEKIMP_PARTY_SWAP_OK);
    native.swap_result=SUDEKIMP_LAN_PARTY_PRESENCE_SWAP_NONE;
    native.assignment=host_state.current.ownership.assignment; native.connected_mask=1;
    CHECK(SudekiMpLanPartyPresenceService(&host_state,&native,GetTickCount()));
    CHECK(host_state.bound_players==1);
    native.assignment=host_state.current.ownership.assignment; native.connected_mask=3;
    CHECK(SudekiMpLanPartyPresenceService(&host_state,&native,GetTickCount()));
    CHECK(host_state.bound_players==1);
    native.assignment=host_state.current.ownership.assignment; native.connected_mask=1;
    CHECK(SudekiMpLanPartyPresenceService(&host_state,&native,GetTickCount()));
    CHECK(SudekiMpLanPartyPresenceQueue(&host_state,SUDEKIMP_LAN_PARTY_COMMAND_RELEASE,1,0,0));
    CHECK(SudekiMpLanPartyPresenceUiFrame(&host_state,GetTickCount()));
    CHECK(host_state.current.ack_result[0]==SUDEKIMP_PARTY_SWAP_OK &&
        host_state.current.ownership.assignment.character[1]==4);
    SudekiMpLanPartyDestroy(client,FALSE); SudekiMpLanPartyDestroy(host,FALSE);
}

int main(void) {
    SudekiMpLanStoryView missing_host_view={0};
    CHECK(!SudekiMpLanStoryViewUsable(&missing_host_view,FALSE));
    CHECK(SudekiMpLanStoryViewUsable(&missing_host_view,TRUE));
    missing_host_view.matrix[0]=1; /* malformed absent view is not a camera gap */
    CHECK(!SudekiMpLanStoryViewUsable(&missing_host_view,TRUE));
    CHECK(!SudekiMpLanStoryViewUsable(NULL,TRUE));
    test_context_isolation(); test_block_dodge_extension(); test_running_attack_extension();
    test_motion_catalog(); test_combat_mode_frame_fence(); test_replica_clock_and_atomicity();
    test_world_phase_replica();
    test_replica_fourth_actor_action_clock(); setup();
    if (!nodes[0] || !nodes[1] || !nodes[2] || !nodes[3]) return 1;
    test_three_clients(); test_transport_replica_consumption(); test_combat_mode_publication(); test_independent_disconnect_rejoin();
    test_raw_authority_and_retry(); test_raw_combat_mode(); test_fragment_admission(); test_presence_frame_floor(); test_busy_hash_and_timeouts();
    for (unsigned i = 0; i < 4; ++i) SudekiMpLanPartyDestroy(nodes[i], FALSE);
    test_dynamic_presence(); test_runtime_admission(); test_local_command_builder(); test_presence_coordinator();
    test_loading_assignment_gate();
    if (failures) return 1;
    puts("party session: three real UDP clients, routing, four-actor frames and independent rejoin passed");
    return 0;
}
