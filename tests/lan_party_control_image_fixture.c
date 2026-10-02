#include "hooks/lan_party_control.h"
#include "hooks/lan_party_host_control.h"
#include "hooks/lan_party_client_control.h"
#include "network/lan_party_motion.h"
#include "network/lan_arena_tal_combo_graph.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Real image signatures and real borrowed controller-dispatch witnesses;
 * inert actor fixtures and recording AI/movement calls. Not gameplay proof. */
static uint8_t actors[4][0xc8], ai[4][0x180], modes[4][0x10];
static uint8_t movements[4][0xc0], arbiters[4][0x64], turns[4][0x20];
static uint8_t accepted[4][0x60], group[0xe0], controller[0x260];
static uint8_t *image_base;
static unsigned int acquire_calls[4], release_calls[4], move_calls[4];
static unsigned int combat_calls[4];
static SudekiMpLanPartyLease leases[4];
static SudekiMpControlUpdateDispatchWitness escaped;
static unsigned int phase, errors;
static BOOL refuse_acquire, wrong_release_mode, no_release, ready_now;
static SudekiMpLanPartySession *nodes[4];
static SudekiMpLanPartyHostControl *network_host;
static SudekiMpLanPartyHostControlReport network_report;
static uint32_t network_now;
static uint8_t present_mask = 15;
static BOOL world_ready = TRUE, in_combat, init_ready = TRUE, spawn_ready = TRUE;
static unsigned int spawn_calls[4], init_calls[4];
static float spawn_positions[4][3], world_anchor[3];
static SudekiMpLanPartyRoster roster;
static SudekiMpLanPartyRosterStatus roster_status;
static SudekiMpLanPartyClientControl *network_client;
static SudekiMpLanPartyClientControlReport client_report;
#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL: party control image fixture line %u: %s\n", \
        (unsigned int)__LINE__, #x); ++errors; } } while (0)
static unsigned int actor_index(void *actor) {
    unsigned int i;
    for (i=0; i<4; ++i) if (actor == actors[i]) return i;
    CHECK(FALSE); return 0;
}
static void *lookup(unsigned int seat) {
    return seat < 4 && (present_mask & (1u << seat)) ? actors[seat] : NULL;
}
static BOOL world_stub(float anchor[3], BOOL *combat) {
    memcpy(anchor,world_anchor,sizeof(world_anchor)); *combat=in_combat;
    return world_ready;
}
static BOOL combat_mode_stub(BOOL *combat) {
    if(!combat) return FALSE;
    *combat=in_combat; return world_ready;
}
static BOOL spawn_stub(unsigned int seat, const float position[3]) {
    if (!spawn_ready) return FALSE;
    ++spawn_calls[seat]; memcpy(spawn_positions[seat],position,sizeof(float)*3);
    return TRUE; /* Async: fixture explicitly completes it later. */
}
static BOOL initialize_stub(unsigned int seat) {
    ++init_calls[seat]; return init_ready;
}
static void set_present(uint8_t mask) {
    unsigned int i, count=0;
    present_mask=mask;
    for (i=0;i<4;++i)
        if (mask & (1u << i)) *(void **)(group+0x90+(count++)*0xc)=actors[i];
    for (i=count;i<4;++i) *(void **)(group+0x90+i*0xc)=NULL;
    *(unsigned int *)(group+0xcc)=count;
}
static BOOL ready(const SudekiMpLanPartyLease *key, void *actor,
    const SudekiMpControlUpdateDispatchWitness *w) {
    return ready_now && key->seat == actor_index(actor) &&
        SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w);
}
static void acquire_stub(void *slot) {
    unsigned int i = actor_index(*(void **)slot);
    ++acquire_calls[i];
    if (refuse_acquire) return;
    ++*(int16_t *)(ai[i]+0x16a); modes[i][0xb] = 0;
}
static void release_stub(void *slot) {
    unsigned int i = actor_index(*(void **)slot);
    ++release_calls[i];
    if (no_release) return;
    --*(int16_t *)(ai[i]+0x16a);
    modes[i][0xb] = wrong_release_mode ? 0 : 1;
}
static void __attribute__((stdcall)) movement_stub(
    void *arbiter, const float *direction, float speed, float turn, uint32_t mode) {
    unsigned int i = actor_index(*(void **)((uint8_t *)arbiter+0x10));
    ++move_calls[i];
    CHECK(isfinite(direction[0]) && isfinite(direction[2]));
    CHECK(speed >= 0 && speed <= 1 && turn == 1 && mode <= 3);
    *(float *)(movements[i]+0x24) = speed;
    *(float *)(movements[i]+0x28) = speed;
}
static void combat_stub(void *actor,void *arbiter) {
    unsigned i=actor_index(actor);
    CHECK(*(void **)((uint8_t *)arbiter+0x10)==actor);
    ++combat_calls[i];
}
static void __attribute__((thiscall)) original_stub(void *c, void *d) {
    (void)c; (void)d;
}
static BOOL move(const SudekiMpControlUpdateDispatchWitness *w, unsigned int seat) {
    return SudekiMpLanPartyControlMove(w,&leases[seat],actors[seat],0,1,0,1,TRUE);
}
static void callback(void *c, void *d,
    const SudekiMpControlUpdateDispatchWitness *w) {
    unsigned int i, before;
    SudekiMpLanPartyLease wrong;
    (void)c; (void)d;
    CHECK(SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w));
    escaped = *w;
    if (phase == 5) {
        CHECK(SudekiMpLanPartyClientControlService(network_client,w,&client_report));
        return;
    }
    if (phase == 3) {
        roster_status=SudekiMpLanPartyRosterService(&roster,w);
        return;
    }
    if (phase == 4) {
        SudekiMpLanPartyRosterObservation observation;
        CHECK(SudekiMpLanPartyControlObserveRoster(w,&observation));
        CHECK(!SudekiMpLanPartyControlInitializeActor(&escaped,&observation,0));
        CHECK(!SudekiMpLanPartyControlSpawnActor(&escaped,&observation,1,world_anchor));
        CHECK(!SudekiMpLanPartyControlInitializeActor(w,&observation,4));
        CHECK(!SudekiMpLanPartyControlSpawnActor(w,&observation,0,world_anchor));
        CHECK(!SudekiMpLanPartyControlSpawnActor(w,&observation,1,world_anchor));
        observation.actors[1]=actors[2];
        CHECK(!SudekiMpLanPartyControlInitializeActor(w,&observation,1));
        return;
    }
    if (phase == 2) {
        CHECK(SudekiMpLanPartyHostControlService(network_host,w,network_now,&network_report));
        return;
    }
    if (phase == 0) {
        CHECK(SudekiMpLanPartyControlBeginSession(w));
        /* Service-only installs return before legacy movement binding. The
         * noncaster path must bind its native stop helper as well as delta. */
        CHECK(!SudekiMpLanPartyControlEnableCastInputIsolation(&escaped));
        CHECK(!SudekiMpControlSeparationForceStopCharacter(actors[0]));
        {
            uint8_t saved=image_base[0xc3870u];
            image_base[0xc3870u]^=1u;
            CHECK(!SudekiMpLanPartyControlEnableCastInputIsolation(w));
            image_base[0xc3870u]=saved;
        }
        CHECK(SudekiMpLanPartyControlEnableCastInputIsolation(w));
        *(float *)(movements[0]+0x24u)=1.f;
        *(float *)(movements[0]+0x28u)=1.f;
        CHECK(SudekiMpControlSeparationForceStopCharacter(actors[0]));
        CHECK(*(float *)(movements[0]+0x24u)==0.f &&
              *(float *)(movements[0]+0x28u)==0.f);
        CHECK(!SudekiMpLanPartyControlSkillFacing(w,&leases[1],actors[1],0,1));
        /* Empty network seats are acceptable only with positively observed
         * default AI. A copied witness, wrong actor or inactive AI is not
         * equivalent to an unoccupied companion. */
        for (i=1; i<4; ++i) {
            CHECK(SudekiMpLanPartyControlHostAiExact(w,i,actors[i]));
            CHECK(!SudekiMpLanPartyControlHostAiExact(&escaped,i,actors[i]));
            CHECK(!SudekiMpLanPartyControlHostAiExact(w,i,actors[0]));
            modes[i][0xb]=0;
            CHECK(!SudekiMpLanPartyControlHostAiExact(w,i,actors[i]));
            modes[i][0xb]=1;
            *(int16_t *)(ai[i]+0x16a)=1;
            CHECK(!SudekiMpLanPartyControlHostAiExact(w,i,actors[i]));
            *(int16_t *)(ai[i]+0x16a)=0;
        }
        CHECK(!SudekiMpLanPartyControlHostAiExact(w,0,actors[0]));
        CHECK(!SudekiMpLanPartyControlHostAiExact(w,4,actors[0]));
        CHECK(!SudekiMpLanPartyControlAcquire(w,&leases[1],actors[1],NULL));
        CHECK(!SudekiMpLanPartyControlAcquire(w,&leases[1],actors[2],ready));
        ready_now = FALSE;
        CHECK(!SudekiMpLanPartyControlAcquire(w,&leases[1],actors[1],ready));
        ready_now = TRUE;
        wrong = leases[1]; wrong.seat = 0;
        CHECK(!SudekiMpLanPartyControlAcquire(w,&wrong,actors[0],ready));
        image_base[0xf60d0] ^= 1;
        CHECK(!SudekiMpLanPartyControlAcquire(w,&leases[1],actors[1],ready));
        image_base[0xf60d0] ^= 1;
        image_base[0xf611c] ^= 1; /* relative Default call target */
        CHECK(!SudekiMpLanPartyControlAcquire(w,&leases[1],actors[1],ready));
        image_base[0xf611c] ^= 1;
        CHECK(acquire_calls[1] == 0);
        /* No P2 prerequisite: independent Tal and Ailish before Elco. */
        for (i=2; i<4; ++i) {
            CHECK(SudekiMpLanPartyControlAcquire(w,&leases[i],actors[i],ready));
            CHECK(!SudekiMpLanPartyControlHostAiExact(w,i,actors[i]));
            CHECK(SudekiMpLanPartyControlHostAiExact(w,1,actors[1]));
        }
        CHECK(SudekiMpLanPartyControlAcquire(w,&leases[1],actors[1],ready));
        for (i=1; i<4; ++i) {
            CHECK(SudekiMpLanPartyControlAcquire(w,&leases[i],actors[i],ready));
            CHECK(acquire_calls[i] == 1);
            CHECK(SudekiMpLanPartyControlExact(w,&leases[i],actors[i]));
            CHECK(move(w,i));
            CHECK(move_calls[i] == 1);
        }
        CHECK(acquire_calls[0] == 0 && move_calls[0] == 0);
        wrong = leases[2]; ++wrong.generation;
        CHECK(!SudekiMpLanPartyControlMove(w,&wrong,actors[2],0,1,0,1,TRUE));
        wrong = leases[2]; ++wrong.token;
        CHECK(!SudekiMpLanPartyControlMove(w,&wrong,actors[2],0,1,0,1,TRUE));
        CHECK(!SudekiMpLanPartyControlMove(w,&leases[2],actors[1],0,1,0,1,TRUE));
        CHECK(!SudekiMpLanPartyControlMove(w,&leases[2],actors[2],NAN,0,0,1,TRUE));
        CHECK(!SudekiMpLanPartyControlMove(&escaped,&leases[2],actors[2],0,1,0,1,TRUE));
        CHECK(!SudekiMpControlSeparationRequestSeatCharacter(1, actors[1]));
        CHECK(!SudekiMpControlSeparationSetLanArenaRemoteInputEnabled(TRUE));
        *(void **)(controller+0x248) = actors[1];
        CHECK(!move(w,1)); CHECK(!move(w,2)); CHECK(!move(w,3));
        *(void **)(controller+0x248) = actors[0];
        *(void **)(ai[2]+0x10) = actors[3]; CHECK(!move(w,2));
        *(void **)(ai[2]+0x10) = actors[2];
        *(void **)(ai[2]+0x3c) = modes[3]; CHECK(!move(w,2));
        *(void **)(ai[2]+0x3c) = modes[2];
        *(void **)movements[2] = image_base; CHECK(!move(w,2));
        *(void **)movements[2] = image_base+0x2c8644;
        *(int16_t *)(ai[2]+0x16a) = 2; CHECK(!move(w,2));
        *(int16_t *)(ai[2]+0x16a) = 1;
        CHECK(SudekiMpLanPartyControlHasLeases());
        CHECK(!SudekiMpLanPartyControlBeginSession(w));
        return;
    }
    *(void **)(ai[1]+0x10) = actors[2];
    CHECK(!SudekiMpLanPartyControlQuiesce(w,&leases[1],actors[1]));
    *(void **)(ai[1]+0x10) = actors[1];
    CHECK(!SudekiMpLanPartyControlExact(w,&leases[1],actors[1]));
    CHECK(SudekiMpLanPartyControlQuiesce(w,&leases[1],actors[1]));
    CHECK(SudekiMpLanPartyControlRetains(w,&leases[1],actors[1]));
    CHECK(!SudekiMpLanPartyControlHostAiExact(w,1,actors[1]));
    CHECK(!move(w,1));
    CHECK(*(float *)(movements[1]+0x24) == 0);
    CHECK(*(float *)(movements[1]+0x28) == 0);
    CHECK(move(w,2) && move(w,3));
    ready_now = FALSE;
    CHECK(!SudekiMpLanPartyControlRelease(w,&leases[1],actors[1],ready));
    ready_now = TRUE;
    CHECK(release_calls[1] == 0);
    /* A Default which didn't decrement is retriable, not completion. */
    no_release = TRUE;
    CHECK(!SudekiMpLanPartyControlRelease(w,&leases[1],actors[1],ready));
    CHECK(SudekiMpLanPartyControlHasLeases());
    no_release = FALSE;
    wrong_release_mode = TRUE;
    CHECK(!SudekiMpLanPartyControlRelease(w,&leases[1],actors[1],ready));
    before = release_calls[1];
    CHECK(!SudekiMpLanPartyControlRelease(w,&leases[1],actors[1],ready));
    CHECK(release_calls[1] == before); /* consumed ref not decremented twice */
    wrong_release_mode = FALSE; modes[1][0xb] = 1;
    CHECK(SudekiMpLanPartyControlRelease(w,&leases[1],actors[1],ready));
    CHECK(!SudekiMpLanPartyControlAcquire(w,&leases[1],actors[1],ready));
    wrong = leases[1]; ++leases[1].generation; ++leases[1].token;
    CHECK(SudekiMpLanPartyControlAcquire(w,&leases[1],actors[1],ready));
    CHECK(!SudekiMpLanPartyControlQuiesce(w,&wrong,actors[1]));
    CHECK(move(w,1));
    for (i=1; i<4; ++i) {
        CHECK(SudekiMpLanPartyControlQuiesce(w,&leases[i],actors[i]));
        CHECK(SudekiMpLanPartyControlRelease(w,&leases[i],actors[i],ready));
        CHECK(*(int16_t *)(ai[i]+0x16a) == 0 && modes[i][0xb] == 1);
        CHECK(SudekiMpLanPartyControlHostAiExact(w,i,actors[i]));
    }
    CHECK(!SudekiMpLanPartyControlHasLeases());
    /* Positively observed no-op refusal does not invent ownership; retry the
     * same pending handshake only once native readiness actually returns. */
    ++leases[1].generation; ++leases[1].token; refuse_acquire = TRUE;
    CHECK(!SudekiMpLanPartyControlAcquire(w,&leases[1],actors[1],ready));
    CHECK(!SudekiMpLanPartyControlHasLeases());
    refuse_acquire = FALSE;
    CHECK(SudekiMpLanPartyControlAcquire(w,&leases[1],actors[1],ready));
    CHECK(SudekiMpLanPartyControlQuiesce(w,&leases[1],actors[1]));
    CHECK(SudekiMpLanPartyControlRelease(w,&leases[1],actors[1],ready));
}
static void network_pump(void) {
    unsigned int pass,i;
    ++network_now;
    for (pass=0; pass<12; ++pass)
        for (i=0; i<4; ++i) SudekiMpLanPartyPoll(nodes[i],network_now);
}
/* HELLO/extension acknowledgements do not prove the client's keepalive
 * return path. Advance the synthetic clock through its 250ms interval. */
static void network_join_pump(void) {
    network_pump(); network_now+=251; network_pump();
}
static SudekiMpLanPartyPeerStatus peer(unsigned int node, unsigned int seat) {
    SudekiMpLanPartyPeerStatus p;
    memset(&p,0,sizeof(p));
    CHECK(SudekiMpLanPartyPeerStatusGet(nodes[node],seat,&p)); return p;
}
static void roster_fixture(void (__attribute__((thiscall)) *update)(void *,void *)) {
    unsigned int i;
    phase=3; memset(&roster,0,sizeof(roster));
    memset(spawn_calls,0,sizeof(spawn_calls)); memset(init_calls,0,sizeof(init_calls));
    world_anchor[0]=10; world_anchor[1]=2; world_anchor[2]=20;
    set_present(1);
    world_ready=FALSE; update(controller,NULL);
    CHECK(!roster.bound_valid && init_calls[0] == 0);
    world_ready=TRUE;
    *(void **)(controller+0x248)=actors[1]; update(controller,NULL);
    CHECK(!roster.bound_valid && spawn_calls[1] == 0);
    *(void **)(controller+0x248)=actors[0];
    in_combat=TRUE; update(controller,NULL);
    CHECK(roster.bound_valid && init_calls[0] == 0);
    in_combat=FALSE; init_ready=FALSE; update(controller,NULL);
    CHECK(roster.initialized_mask == 0 && init_calls[0] == 1);
    init_ready=TRUE; update(controller,NULL);
    CHECK(roster.initialized_mask == 1);
    spawn_ready=FALSE; update(controller,NULL);
    CHECK(roster.spawn_pending_mask == 0 && spawn_calls[1] == 0);
    spawn_ready=TRUE; update(controller,NULL);
    CHECK(roster.spawn_pending_mask == 2 && spawn_calls[1] == 1);
    world_anchor[0]=100; /* Spawns still use the captured initial anchor. */
    for (i=0;i<100;++i) update(controller,NULL);
    CHECK(spawn_calls[1] == 1 && spawn_calls[2] == 0 && init_calls[1] == 0);
    CHECK(spawn_positions[1][0] == 11.5f && spawn_positions[1][2] == 18.5f);
    set_present(3); update(controller,NULL);
    CHECK(roster.spawn_pending_mask == 0 && roster.initialized_mask == 3);
    update(controller,NULL); CHECK(spawn_calls[2] == 1 && roster.spawn_pending_mask == 4);
    set_present(7); update(controller,NULL); update(controller,NULL);
    CHECK(spawn_calls[3] == 1 && roster.spawn_pending_mask == 8);
    CHECK(spawn_positions[2][0] == 8.5f && spawn_positions[3][0] == 11.5f);
    set_present(15); update(controller,NULL); update(controller,NULL);
    CHECK(roster_status == SUDEKIMP_LAN_PARTY_ROSTER_READY && roster.initialized_mask == 15);
    in_combat=TRUE;
    for (i=0;i<20;++i) update(controller,NULL);
    CHECK(roster_status == SUDEKIMP_LAN_PARTY_ROSTER_READY);
    CHECK(init_calls[0] == 2 && init_calls[1] == 1 && init_calls[2] == 1 && init_calls[3] == 1);
    in_combat=FALSE;
    *(void **)(group+0x90+3*0xc)=actors[2]; update(controller,NULL);
    CHECK(roster_status == SUDEKIMP_LAN_PARTY_ROSTER_WAITING); /* Duplicate != absent. */
    CHECK(spawn_calls[3] == 1);
    set_present(15); phase=4; update(controller,NULL); phase=3;
    set_present(7); update(controller,NULL);
    CHECK(roster_status == SUDEKIMP_LAN_PARTY_ROSTER_REPLACED);
    set_present(15); update(controller,NULL);
    CHECK(roster_status == SUDEKIMP_LAN_PARTY_ROSTER_REPLACED);
    CHECK(init_calls[3] == 1 && spawn_calls[3] == 1);
    memset(world_anchor,0,sizeof(world_anchor));
}
static void network_fixture(void (__attribute__((thiscall)) *update)(void *,void *)) {
    SudekiMpLanPartyConfig config;
    SudekiMpLanArenaInput input;
    SudekiMpLanPartyLease previous;
    uint32_t acknowledged;
    unsigned int i, moves, combat_before;
    memset(&config,0,sizeof(config)); memset(&input,0,sizeof(input));
    memset(config.game_hash,0x15,sizeof(config.game_hash));
    nodes[0]=SudekiMpLanPartyCreate(&config); CHECK(nodes[0] != NULL);
    config.port=SudekiMpLanPartyPort(nodes[0]); config.host_ipv4="127.0.0.1";
    for (i=1;i<4;++i) {
        config.local_seat=(uint8_t)i;
        nodes[i]=SudekiMpLanPartyCreate(&config); CHECK(nodes[i] != NULL);
    }
    network_now=GetTickCount(); network_join_pump();
    CHECK(SudekiMpLanPartyHostControlCreate(nodes[1],ready) == NULL);
    network_host=SudekiMpLanPartyHostControlCreate(nodes[0],ready);
    CHECK(network_host != NULL);
    CHECK(SudekiMpLanPartyHostControlCreate(nodes[0],ready) == NULL);
    phase=2; ready_now=FALSE;
    CHECK(!SudekiMpLanPartyHostControlService(network_host,&escaped,network_now,&network_report));
    set_present(1);
    update(controller,NULL);
    CHECK(network_report.waiting_mask == 14 && network_report.owned_mask == 0);
    for (i=1;i<4;++i) CHECK(peer(0,i).phase == SUDEKIMP_LAN_PARTY_PENDING);
    /* Approval waits for all four native actors' equipment initialization,
     * independently of the transport-confirmed pending peers. */
    for (i=1;i<4;++i) {
        unsigned int calls=spawn_calls[i];
        update(controller,NULL);
        CHECK(network_report.roster_pending_mask == (1u << i));
        CHECK(spawn_calls[i] == calls+1);
        update(controller,NULL); update(controller,NULL);
        CHECK(spawn_calls[i] == calls+1 && network_report.owned_mask == 0);
        CHECK(peer(0,i).phase == SUDEKIMP_LAN_PARTY_PENDING);
        set_present((uint8_t)((1u << (i+1))-1));
        update(controller,NULL);
        CHECK(network_report.roster_pending_mask == 0);
    }
    update(controller,NULL);
    CHECK(network_report.roster_status == SUDEKIMP_LAN_PARTY_ROSTER_READY);
    CHECK(network_report.roster_initialized_mask == 15);
    CHECK(network_report.owned_mask == 0); /* Native ready probe still false. */
    ready_now=TRUE; update(controller,NULL); network_pump();
    CHECK(network_report.owned_mask == 14 && network_report.failed_mask == 0);
    for (i=1;i<4;++i) {
        CHECK(peer(i,i).phase == SUDEKIMP_LAN_PARTY_ACTIVE);
        input.actor_type=SudekiMpLanPartyActorType(i);
        input.world_direction_x=(int16_t)(4000*i);
        CHECK(SudekiMpLanPartySendInput(nodes[i],&input));
    }
    network_pump(); --network_now; update(controller,NULL); ++network_now;
    CHECK(network_report.failed_mask == 0 && network_report.owned_mask == 14);
    for (i=1;i<4;++i) {
        CHECK(peer(0,i).admitted_input_sequence != 0);
        CHECK(fabsf(*(float *)(movements[i]+0x24) - (4000*i)/32767.0f) < 0.000001f);
    }
    CHECK(*(float *)(movements[0]+0x24) == 0 && move_calls[0] == 0);
    moves=move_calls[2]; update(controller,NULL);
    CHECK(move_calls[2] == moves+1); /* held input serviced, not replayed edges */
    acknowledged=peer(0,2).admitted_input_sequence;
    input.actor_type=SudekiMpLanPartyActorType(2);
    CHECK(SudekiMpLanPartySendInput(nodes[2],&input)); network_pump();
    world_ready=FALSE; update(controller,NULL);
    CHECK(network_report.waiting_mask == 14 && network_report.owned_mask == 14);
    CHECK(peer(0,2).admitted_input_sequence == acknowledged);
    for (i=1;i<4;++i) CHECK(*(float *)(movements[i]+0x24) == 0);
    world_ready=TRUE; update(controller,NULL);
    CHECK(peer(0,2).admitted_input_sequence > acknowledged);
    network_now+=251; network_pump(); update(controller,NULL);
    for (i=1;i<4;++i) {
        CHECK(*(float *)(movements[i]+0x24) == 0);
        CHECK(peer(0,i).phase == SUDEKIMP_LAN_PARTY_ACTIVE);
    }
    /* A fresh weak edge is submitted only through that actor's validated
     * native arbiter and ACKed after submission, for all three client seats. */
    in_combat=TRUE;
    for(i=1;i<4;++i) {
        input.actor_type=SudekiMpLanPartyActorType(i); input.weak_attack_pressed=1;
        combat_before=combat_calls[i];
        acknowledged=peer(0,i).admitted_input_sequence;
        CHECK(SudekiMpLanPartySendInput(nodes[i],&input));
        network_pump(); update(controller,NULL);
        CHECK(!(network_report.unsupported_input_mask&(1u<<i)));
        CHECK(peer(0,i).admitted_input_sequence>acknowledged);
        CHECK(combat_calls[i]==combat_before+1);
    }
    in_combat=FALSE; input.weak_attack_pressed=0; input.actor_type=SudekiMpLanPartyActorType(2);
    input.world_direction_x=INT16_MIN;
    CHECK(SudekiMpLanPartySendInput(nodes[2],&input));
    network_pump(); update(controller,NULL);
    CHECK(network_report.failed_mask == 0 && *(float *)(movements[2]+0x24) == 1);
    previous=peer(0,1).lease;
    CHECK(SudekiMpLanPartyDisconnect(nodes[1],&previous)); network_pump();
    ready_now=FALSE; update(controller,NULL);
    CHECK(peer(0,1).phase == SUDEKIMP_LAN_PARTY_DRAINING);
    CHECK(*(int16_t *)(ai[1]+0x16a) == 1 && *(float *)(movements[1]+0x24) == 0);
    CHECK(peer(0,2).phase == SUDEKIMP_LAN_PARTY_ACTIVE);
    CHECK(peer(0,3).phase == SUDEKIMP_LAN_PARTY_ACTIVE);
    ready_now=TRUE; update(controller,NULL);
    CHECK(peer(0,1).phase == SUDEKIMP_LAN_PARTY_FREE);
    CHECK(*(int16_t *)(ai[1]+0x16a) == 0 && modes[1][0xb] == 1);
    moves=init_calls[1];
    SudekiMpLanPartyDestroy(nodes[1],FALSE); config.local_seat=1;
    nodes[1]=SudekiMpLanPartyCreate(&config); CHECK(nodes[1] != NULL);
    network_join_pump(); update(controller,NULL); network_pump();
    CHECK(peer(0,1).phase == SUDEKIMP_LAN_PARTY_ACTIVE);
    CHECK(peer(0,1).lease.generation > previous.generation);
    CHECK(peer(0,1).lease.token != previous.token);
    CHECK(*(int16_t *)(ai[1]+0x16a) == 1 && modes[1][0xb] == 0);
    CHECK(init_calls[1] == moves); /* Rejoin never reapplies the startup kit. */
    CHECK(!SudekiMpUninstallControlSeparation());
    moves=spawn_calls[3];
    set_present(7); update(controller,NULL);
    CHECK(network_report.roster_status == SUDEKIMP_LAN_PARTY_ROSTER_REPLACED);
    CHECK(network_report.draining_mask == 14 && SudekiMpLanPartyControlHasLeases());
    for (i=1;i<4;++i) CHECK(peer(0,i).phase == SUDEKIMP_LAN_PARTY_DRAINING);
    CHECK(spawn_calls[3] == moves); /* Replacement cannot fabricate a new actor. */
    set_present(15); /* Original identities return: retained release can finish. */
    SudekiMpLanPartyHostControlRequestStop(network_host);
    CHECK(!SudekiMpLanPartyHostControlDestroy(network_host));
    for (i=1;i<4;++i) CHECK(peer(0,i).phase == SUDEKIMP_LAN_PARTY_DRAINING);
    update(controller,NULL);
    CHECK(!SudekiMpLanPartyControlHasLeases());
    CHECK(SudekiMpLanPartyHostControlDestroy(network_host)); network_host=NULL;
    for (i=1;i<4;++i) CHECK(*(int16_t *)(ai[i]+0x16a) == 0 && modes[i][0xb] == 1);
    for (i=0;i<4;++i) { SudekiMpLanPartyDestroy(nodes[i],FALSE); nodes[i]=NULL; }
}
static void client_fixture(void (__attribute__((thiscall)) *update)(void *,void *),unsigned local) {
    SudekiMpLanPartyConfig config;
    SudekiMpLanPartyPeerStatus p;
    unsigned order=0, before[4];
    memset(&config,0,sizeof(config)); memset(config.game_hash,0x13,32);
    config.timeout_ms=5000;
    nodes[0]=SudekiMpLanPartyCreate(&config); CHECK(nodes[0]!=NULL);
    config.port=SudekiMpLanPartyPort(nodes[0]); config.host_ipv4="127.0.0.1";
    config.local_seat=(uint8_t)local;
    nodes[local]=SudekiMpLanPartyCreate(&config); CHECK(nodes[local]!=NULL);
    CHECK(SudekiMpLanPartyClientControlCreate(nodes[0],local,ready)==NULL);
    CHECK(SudekiMpLanPartyClientControlCreate(nodes[local],local%3+1,ready)==NULL);
    network_client=SudekiMpLanPartyClientControlCreate(nodes[local],local,ready);
    CHECK(network_client!=NULL);
    CHECK(SudekiMpLanPartyClientControlCreate(nodes[local],local,ready)==NULL);
    phase=5; set_present(15); world_ready=init_ready=ready_now=TRUE; in_combat=FALSE;
    *(void **)(group+0x90)=actors[local];
    for(unsigned i=0;i<4;++i) if(i!=local)
        *(void **)(group+0x90+(++order)*0xc)=actors[i];
    *(void **)(controller+0x248)=actors[local];
    memcpy(before,acquire_calls,sizeof(before));
    network_now=GetTickCount(); network_join_pump();
    /* Native local controller is never overridden. The other three are
     * presentation leases, even Buki (network host, locally a replica). */
    for(unsigned i=0;i<8;++i) update(controller,NULL);
    CHECK(!client_report.ready && client_report.owned_mask==(15u&~(1u<<local)));
    for(unsigned i=0;i<4;++i) CHECK(acquire_calls[i]==before[i]+(i!=local));
    p=peer(0,local); CHECK(SudekiMpLanPartyApprove(nodes[0],&p.lease));
    network_pump(); update(controller,NULL);
    CHECK(client_report.ready && client_report.failed_mask==0);
    CHECK(!SudekiMpLanPartyClientControlService(network_client,&escaped,&client_report));
    update(controller,NULL); CHECK(client_report.ready);
    /* A transport END closes presentation immediately, but cannot release
     * a native actor whose positively observed drain proof is still false. */
    ready_now=FALSE; CHECK(SudekiMpLanPartyDisconnect(nodes[0],&p.lease));
    network_pump(); update(controller,NULL);
    CHECK(!client_report.ready && client_report.draining_mask==(15u&~(1u<<local)));
    CHECK(SudekiMpLanPartyControlHasLeases());
    SudekiMpLanPartyClientControlRequestStop(network_client);
    CHECK(!SudekiMpLanPartyClientControlDestroy(network_client));
    CHECK(!SudekiMpUninstallControlSeparation());
    ready_now=TRUE; update(controller,NULL);
    CHECK(!SudekiMpLanPartyControlHasLeases());
    CHECK(SudekiMpLanPartyClientControlDestroy(network_client)); network_client=NULL;
    for(unsigned i=0;i<4;++i) { CHECK(*(int16_t *)(ai[i]+0x16a)==0);
        if(nodes[i]) { SudekiMpLanPartyDestroy(nodes[i],FALSE); nodes[i]=NULL; } }
    set_present(15); *(void **)(controller+0x248)=actors[0];
}
static unsigned presentation_visits;
static void (*presentation_thread_call)(void);
static DWORD WINAPI presentation_wrong_thread(void *unused) {
    (void)unused;
    CHECK(!SudekiMpLanPartyPresentationBoundary());
    presentation_thread_call();
    return 0;
}
static void presentation_callback(unsigned phase) {
    SudekiMpLanPartyRosterObservation o;
    CHECK(phase<=2 && SudekiMpLanPartyPresentationBoundary());
    CHECK(SudekiMpLanPartyControlObserveRoster(NULL,&o));
    CHECK(!SudekiMpLanPartyControlMove(NULL,&leases[1],actors[1],1,0,0,0,FALSE));
    CHECK(!SudekiMpLanPartyControlRelease(NULL,&leases[1],actors[1],ready));
    CHECK(!SudekiMpLanPartyRemovePresentationObserver()); /* in-flight hook */
    CHECK(!SudekiMpLanPartyControlInitializeActor(NULL,&o,1));
    { float p[3]={0}; CHECK(!SudekiMpLanPartyControlSpawnActor(NULL,&o,1,p)); }
    ++presentation_visits;
}
static void presentation_fixture(uint8_t *base) {
    uint8_t saved=base[0x1dce30], patched[5]; int32_t d;
    HANDLE thread;
    CHECK(!SudekiMpLanPartyPresentationBoundary());
    /* Failed second install must roll back the first and allow a clean retry. */
    base[0x28d539]=0x90;
    CHECK(!SudekiMpLanPartyInstallPresentationObserver(presentation_callback));
    memcpy(&d,base+0x28d444,4); CHECK(base+0x28d448+d==base+0x1dce30);
    base[0x28d539]=0xe8;
    CHECK(SudekiMpLanPartyInstallPresentationObserver(presentation_callback));
    CHECK(!SudekiMpLanPartyInstallPresentationObserver(presentation_callback));
    base[0x1dce30]=0xc3; /* inert original, not live rendering */
    presentation_visits=0;
    memcpy(&d,base+0x28d444,4); ((void (*)(void))(base+0x28d448+d))();
    memcpy(&d,base+0x28d53a,4); ((void (*)(void))(base+0x28d53e + d))();
    CHECK(presentation_visits==3 && !SudekiMpLanPartyPresentationBoundary());
    memcpy(&d,base+0x28d444,4); presentation_thread_call=(void (*)(void))(base+0x28d448+d);
    thread=CreateThread(NULL,0,presentation_wrong_thread,NULL,0,NULL);
    CHECK(thread!=NULL);
    if(thread) { CHECK(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0); CloseHandle(thread); }
    CHECK(presentation_visits==3);
    /* A foreign first seam does not prevent restoring the second; it must
     * retain callable dependencies and refuse reinstall until ownership returns. */
    memcpy(patched,base+0x28d443,5); base[0x28d443]=0x90;
    CHECK(!SudekiMpLanPartyRemovePresentationObserver());
    CHECK(base[0x28d443]==0x90);
    memcpy(&d,base+0x28d53a,4); CHECK(base+0x28d53e + d==base+0x1dce30);
    CHECK(!SudekiMpLanPartyInstallPresentationObserver(presentation_callback));
    memcpy(base+0x28d443,patched,5);
    CHECK(SudekiMpLanPartyRemovePresentationObserver());
    memcpy(&d,base+0x28d444,4); CHECK(base+0x28d448+d==base+0x1dce30);
    memcpy(&d,base+0x28d53a,4); CHECK(base+0x28d53e + d==base+0x1dce30);
    base[0x1dce30]=saved;
}
static void ranged_action_fixture(void) {
    uint8_t action=0xffu;
    int tal_weak, tal_state;
    int elco=SudekiMpLanArenaRangedCombatSelector(
        SUDEKIMP_LAN_ARENA_ELCO_TYPE,0x85u);
    int ailish=SudekiMpLanArenaRangedCombatSelector(
        SUDEKIMP_LAN_ARENA_AILISH_TYPE,0x85u);
    CHECK(elco>=0 && ailish>=0);
    CHECK(SudekiMpLanPartyRangedActionObserve(
        SUDEKIMP_LAN_ARENA_ELCO_TYPE,elco,1u,&action));
    CHECK(action==SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE);
    CHECK(SudekiMpLanPartyRangedActionObserve(
        SUDEKIMP_LAN_ARENA_AILISH_TYPE,ailish,65u,&action));
    CHECK(action==SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE);
    CHECK(SudekiMpLanPartyRangedActionObserve(
        SUDEKIMP_LAN_ARENA_AILISH_TYPE,ailish,192u,&action));
    CHECK(action==SUDEKIMP_LAN_ARENA_ACTION_NONE);
    /* Straight aim selector54 is not a supported firing channel. */
    CHECK(!SudekiMpLanPartyRangedActionObserve(
        SUDEKIMP_LAN_ARENA_ELCO_TYPE,elco+1,1u,&action));
    CHECK(action==SUDEKIMP_LAN_ARENA_ACTION_NONE);
    CHECK(!SudekiMpLanPartyRangedActionObserve(
        SUDEKIMP_LAN_ARENA_TAL_TYPE,elco,1u,&action));
    CHECK(SudekiMpLanArenaTalActionToNativePresentation(
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE,&tal_weak,&tal_state));
    CHECK(!SudekiMpLanPartyActionTerminalObserved(
        SUDEKIMP_LAN_ARENA_TAL_TYPE,SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE,
        tal_weak,1u));
    CHECK(SudekiMpLanPartyActionTerminalObserved(
        SUDEKIMP_LAN_ARENA_TAL_TYPE,SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE,
        tal_weak,128u));
    CHECK(SudekiMpLanPartyActionTerminalObserved(
        SUDEKIMP_LAN_ARENA_TAL_TYPE,SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE,
        SudekiMpLanPartyCombatMotionSelector(SUDEKIMP_LAN_ARENA_TAL_TYPE,1u),128u));
    CHECK(!SudekiMpLanPartyActionTerminalObserved(
        SUDEKIMP_LAN_ARENA_TAL_TYPE,SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE,
        tal_weak+1,128u));
    CHECK(SudekiMpLanPartyActionTerminalObserved(
        SUDEKIMP_LAN_ARENA_ELCO_TYPE,SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE,
        elco,192u));
    CHECK(SudekiMpLanPartyActionTerminalObserved(
        SUDEKIMP_LAN_ARENA_AILISH_TYPE,SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE,
        0,192u));
    CHECK(!SudekiMpLanPartyActionTerminalObserved(
        SUDEKIMP_LAN_ARENA_AILISH_TYPE,SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE,
        ailish,65u));
    CHECK(SudekiMpLanPartyRangedActionChannelDrained(
        SUDEKIMP_LAN_ARENA_ELCO_TYPE,0,192u));
    CHECK(SudekiMpLanPartyRangedActionChannelDrained(
        SUDEKIMP_LAN_ARENA_AILISH_TYPE,ailish,192u));
    CHECK(!SudekiMpLanPartyRangedActionChannelDrained(
        SUDEKIMP_LAN_ARENA_AILISH_TYPE,ailish,65u));
    CHECK(!SudekiMpLanPartyRangedActionChannelDrained(
        SUDEKIMP_LAN_ARENA_ELCO_TYPE,elco+1,192u));
}
int SudekiMpLanPartyControlImageFixture(uint8_t *base) {
    unsigned int i;
    uint8_t original[16];
    void *saved_group = *(void **)(base+0x408d94);
    void *saved_controller = *(void **)(base+0x408da4);
    uint32_t saved_input_enable_operand;
    void (__attribute__((thiscall)) *update)(void *,void *);
    int32_t displacement;
    static char observer_owner;
    image_base = base; errors = phase = 0; ready_now = TRUE;
    /* The mapped fixture relocates exercised operands explicitly. Retail's
     * loader relocates this absolute group load before cast binding. */
    memcpy(&saved_input_enable_operand,base+0x27121u,4);
    CHECK(base[0x27120u]==0xa1u && saved_input_enable_operand==0x00808d94u);
    *(void **)(base+0x27121u)=base+0x408d94u;
    ranged_action_fixture();
    memset(actors,0,sizeof(actors)); memset(ai,0,sizeof(ai));
    memset(modes,0,sizeof(modes)); memset(group,0,sizeof(group));
    memset(controller,0,sizeof(controller));
    memset(movements,0,sizeof(movements)); memset(arbiters,0,sizeof(arbiters));
    memset(acquire_calls,0,sizeof(acquire_calls));
    memset(release_calls,0,sizeof(release_calls)); memset(move_calls,0,sizeof(move_calls));
    memset(combat_calls,0,sizeof(combat_calls));
    for (i=0; i<4; ++i) {
        leases[i].seat=(uint8_t)i; leases[i].token=1234u+i; leases[i].generation=1;
        *(void **)(actors[i]+0x94)=ai[i]; *(void **)(ai[i]+0x10)=actors[i];
        *(void **)ai[i]=base+0x2d4924; /* CCharacterEvent, also owns AI state. */
        *(void **)(ai[i]+0x3c)=modes[i]; modes[i][0xb]=1;
        *(void **)(actors[i]+0x80)=movements[i];
        *(void **)movements[i]=base+0x2c8644;
        *(void **)(movements[i]+0x10)=actors[i]; movements[i][0xbe]=8;
        *(void **)(actors[i]+0x90)=arbiters[i]; *(void **)arbiters[i]=base+0x2cc9ac;
        *(void **)(arbiters[i]+0x10)=actors[i];
        *(void **)(actors[i]+0x8c)=turns[i]; *(void **)turns[i]=base+0x2d48d4;
        *(void **)(turns[i]+0x10)=actors[i];
        *(void **)(actors[i]+0xac)=accepted[i]; *(void **)accepted[i]=base+0x2d4b24;
        *(void **)(accepted[i]+0x10)=actors[i];
        *(void **)(group+0x90+i*0xc)=actors[i];
    }
    *(unsigned int *)(group+0xcc)=4;
    *(void **)(controller+0x248)=actors[0];
    *(void **)(base+0x408d94)=group; *(void **)(base+0x408da4)=controller;
    memcpy(original,base+0x27cf0,sizeof(original));
    CHECK(SudekiMpControlSeparationRegisterUpdateObserver(&observer_owner,callback));
    CHECK(SudekiMpInstallControlSeparation((HMODULE)base,0,FALSE,FALSE,FALSE,0,
        FALSE,0,FALSE,NULL,FALSE,FALSE,FALSE,0));
    SudekiMpLanPartyControlTestCalls(acquire_stub,release_stub,movement_stub,lookup);
    SudekiMpLanPartyControlTestRosterCalls(world_stub,spawn_stub,initialize_stub);
    SudekiMpLanPartyControlTestCombat(combat_stub);
    SudekiMpLanPartyControlTestCombatMode(combat_mode_stub);
    SudekiMpLanPartyHostControlTestCombatMode(combat_mode_stub);
    /* Only the mapped controller original is stubbed; the real wrapper issues
     * the unforgeable witness, and real native SetSpeedImmediate stops motion. */
    base[0x27cf0]=0xe9;
    displacement=(int32_t)((uintptr_t)original_stub-(uintptr_t)(base+0x27cf5));
    memcpy(base+0x27cf1,&displacement,4);
    update=(void (__attribute__((thiscall)) *)(void *,void *))*(void **)(base+0x2c9f60);
    update(controller,NULL);
    presentation_fixture(base);
    CHECK(!SudekiMpLanPartyControlExact(&escaped,&leases[1],actors[1]));
    CHECK(!SudekiMpLanPartyControlQuiesce(&escaped,&leases[1],actors[1]));
    CHECK(!SudekiMpUninstallControlSeparation());
    CHECK(GetLastError() == ERROR_BUSY && SudekiMpLanPartyControlHasLeases());
    phase=1; update(controller,NULL);
    CHECK(!SudekiMpLanPartyControlHasLeases());
    CHECK(SudekiMpControlSeparationUnregisterUpdateObserver(&observer_owner));
    CHECK(SudekiMpUninstallControlSeparation());
    memcpy(base+0x27cf0,original,sizeof(original));
    /* Fresh adapter lifetime for a real three-client UDP-to-native route. */
    CHECK(SudekiMpControlSeparationRegisterUpdateObserver(&observer_owner,callback));
    CHECK(SudekiMpInstallControlSeparation((HMODULE)base,0,FALSE,FALSE,FALSE,0,
        FALSE,0,FALSE,NULL,FALSE,FALSE,FALSE,0));
    base[0x27cf0]=0xe9; memcpy(base+0x27cf1,&displacement,4);
    update=(void (__attribute__((thiscall)) *)(void *,void *))*(void **)(base+0x2c9f60);
    roster_fixture(update);
    network_fixture(update);
    for(i=1;i<4;++i) client_fixture(update,i);
    /* A new host session uses new tokens and starts generations at one; it
     * must not inherit the retired session's generation high-water marks. */
    network_fixture(update);
    CHECK(SudekiMpControlSeparationUnregisterUpdateObserver(&observer_owner));
    CHECK(SudekiMpUninstallControlSeparation());
    memcpy(base+0x27cf0,original,sizeof(original));
    *(void **)(base+0x408d94)=saved_group; *(void **)(base+0x408da4)=saved_controller;
    memcpy(base+0x27121u,&saved_input_enable_operand,4);
    SudekiMpLanPartyControlTestCalls(NULL,NULL,NULL,NULL);
    SudekiMpLanPartyControlTestRosterCalls(NULL,NULL,NULL);
    SudekiMpLanPartyControlTestCombat(NULL);
    SudekiMpLanPartyControlTestCombatMode(NULL);
    SudekiMpLanPartyHostControlTestCombatMode(NULL);
    if (!errors) puts("lan_party_control_image_fixture: PASS (inert native calls)");
    return (int)errors;
}
