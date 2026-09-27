#include "network/lan_party_session.h"
#include "network/lan_party_replica.h"
#include "network/lan_party_motion.h"
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
        phase.clip[0]=6; CHECK(!SudekiMpLanPartyMotionValid(type,&phase));
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
        CHECK(SudekiMpLanPartyReplicaSample(&replicas[seat-1],&peer.lease,3000,&sample,&tick));
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
static void test_independent_disconnect_rejoin(void) {
    SudekiMpLanPartyPeerStatus previous = status(0, 1);
    SudekiMpLanPartyLease lease = previous.lease;
    CHECK(!SudekiMpLanPartyReleaseDrained(nodes[0], &lease));
    CHECK(SudekiMpLanPartyDisconnect(nodes[1], &lease)); pump();
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
    CHECK(SudekiMpLanPartyApprove(nodes[0], &current.lease)); pump();
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
    raw_send(sock, to, RAW_INPUT, l->seat, l->generation, l->token, sequence,
        0, bytes + 20, size - 20, SUDEKIMP_LAN_PARTY_VERSION);
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

int main(void) {
    test_context_isolation(); test_motion_catalog(); test_replica_clock_and_atomicity();
    test_world_phase_replica();
    test_replica_fourth_actor_action_clock(); setup();
    if (!nodes[0] || !nodes[1] || !nodes[2] || !nodes[3]) return 1;
    test_three_clients(); test_transport_replica_consumption(); test_independent_disconnect_rejoin();
    test_raw_authority_and_retry(); test_fragment_admission(); test_busy_hash_and_timeouts();
    for (unsigned i = 0; i < 4; ++i) SudekiMpLanPartyDestroy(nodes[i], FALSE);
    if (failures) return 1;
    puts("party session: three real UDP clients, routing, four-actor frames and independent rejoin passed");
    return 0;
}
