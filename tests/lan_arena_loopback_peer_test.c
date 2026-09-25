#include "network/lan_arena_session.h"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t game_hash[SUDEKIMP_LAN_ARENA_GAME_HASH_SIZE];

static void fill_snapshot(SudekiMpLanArenaSnapshot *snapshot, DWORD now) {
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->host_tick = now;
    snapshot->match_state = SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    snapshot->seat[0].actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    snapshot->seat[0].native_entity_id = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    snapshot->seat[0].facing_z = 1.0f;
    snapshot->seat[0].hp = 6850u;
    snapshot->seat[0].sp = 440u;
    snapshot->seat[0].animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    snapshot->seat[1].actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    snapshot->seat[1].native_entity_id = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    snapshot->seat[1].x = 3.0f;
    snapshot->seat[1].facing_x = 1.0f;
    snapshot->seat[1].hp = 3300u;
    snapshot->seat[1].sp = 440u;
    snapshot->seat[1].animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    snapshot->seat[1].combat_state = SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    snapshot->seat[1].action_variant = SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE;
    snapshot->seat[1].action_phase_valid = 1u;
    snapshot->seat[1].action_phase_q8 = 18u * 256u;
    snapshot->enemy_count = 1u;
    snapshot->enemies[0].native_entity_id = 1u;
    snapshot->enemies[0].z = 6.0f;
    snapshot->enemies[0].hp = 950u;
}

static int run_host(unsigned int port) {
    SudekiMpLanArenaSessionConfig config;
    SudekiMpLanArenaSessionStatus status;
    SudekiMpLanArenaInput input;
    SudekiMpLanArenaSnapshot snapshot;
    DWORD started = GetTickCount();
    BOOL input_received = FALSE;
    BOOL snapshot_sent = FALSE;
    memset(&config, 0, sizeof(config));
    config.local_role = SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL;
    config.local_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
    config.port = port;
    config.timeout_ms = 1500u;
    config.game_hash = game_hash;
    config.host_actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    config.client_actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    if (!SudekiMpLanArenaSessionStart(&config)) return 10;
    while ((DWORD)(GetTickCount() - started) < 5000u) {
        DWORD now = GetTickCount();
        SudekiMpLanArenaSessionPoll(now);
        if (SudekiMpLanArenaSessionTakeRemoteInput(&input)) {
            if (input.world_direction_x != 16384 ||
                input.world_direction_z != -8192 ||
                input.actor_type != SUDEKIMP_LAN_ARENA_AILISH_TYPE ||
                input.aim_direction_x != 32767 ||
                input.weak_attack_pressed != 1u ||
                input.weak_attack_held != 1u ||
                input.ranged_first_person_active != 1u ||
                input.cleanroom_combat_test_pressed != 1u) {
                SudekiMpLanArenaSessionStop(FALSE);
                return 11;
            }
            input_received = TRUE;
        }
        if (input_received && !snapshot_sent) {
            fill_snapshot(&snapshot, now);
            if (!SudekiMpLanArenaSessionSendSnapshot(&snapshot)) {
                SudekiMpLanArenaSessionStop(FALSE);
                return 12;
            }
            snapshot_sent = TRUE;
        }
        if (!SudekiMpLanArenaSessionGetStatus(&status) ||
            (status.failure != SUDEKIMP_LAN_ARENA_REJECT_NONE &&
             status.phase != SUDEKIMP_LAN_ARENA_CONNECTION_ENDED)) {
            SudekiMpLanArenaSessionStop(FALSE);
            return 13;
        }
        if (snapshot_sent) {
            Sleep(150u);
            SudekiMpLanArenaSessionStop(TRUE);
            puts("lan arena loopback host passed");
            return 0;
        }
        Sleep(5u);
    }
    SudekiMpLanArenaSessionStop(FALSE);
    return 14;
}

static int run_client(unsigned int port) {
    SudekiMpLanArenaSessionConfig config;
    SudekiMpLanArenaSessionStatus status;
    SudekiMpLanArenaInput input;
    SudekiMpLanArenaSnapshot snapshot;
    DWORD started = GetTickCount();
    BOOL input_sent = FALSE;
    memset(&config, 0, sizeof(config));
    config.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    config.local_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    config.remote_ipv4 = "127.0.0.1";
    config.port = port;
    config.timeout_ms = 1500u;
    config.game_hash = game_hash;
    config.host_actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    config.client_actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    if (!SudekiMpLanArenaSessionStart(&config)) return 20;
    while ((DWORD)(GetTickCount() - started) < 5000u) {
        SudekiMpLanArenaSessionPoll(GetTickCount());
        if (!SudekiMpLanArenaSessionGetStatus(&status)) {
            SudekiMpLanArenaSessionStop(FALSE);
            return 21;
        }
        if (status.peer_connected && !input_sent) {
            memset(&input, 0, sizeof(input));
            input.client_tick = GetTickCount();
            input.actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
            input.world_direction_x = 16384;
            input.world_direction_z = -8192;
            input.aim_direction_x = 32767;
            input.weak_attack_pressed = 1u;
            input.weak_attack_held = 1u;
            input.ranged_first_person_active = 1u;
            input.cleanroom_combat_test_pressed = 1u;
            if (!SudekiMpLanArenaSessionSendInput(&input)) {
                SudekiMpLanArenaSessionStop(FALSE);
                return 22;
            }
            input_sent = TRUE;
        }
        if (SudekiMpLanArenaSessionTakeRemoteSnapshot(&snapshot)) {
            if (!input_sent || snapshot.match_state != SUDEKIMP_LAN_ARENA_MATCH_ACTIVE ||
                snapshot.seat[0].hp != 6850u || snapshot.seat[1].hp != 3300u ||
                snapshot.seat[1].animation_state !=
                    SUDEKIMP_LAN_ARENA_ANIMATION_ACTION ||
                snapshot.seat[1].combat_state !=
                    SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK ||
                snapshot.enemy_count != 1u || snapshot.enemies[0].hp != 950u) {
                SudekiMpLanArenaSessionStop(FALSE);
                return 23;
            }
            SudekiMpLanArenaSessionStop(FALSE);
            puts("lan arena loopback client passed");
            return 0;
        }
        if (status.phase == SUDEKIMP_LAN_ARENA_CONNECTION_REJECTED ||
            status.phase == SUDEKIMP_LAN_ARENA_CONNECTION_TIMED_OUT) {
            SudekiMpLanArenaSessionStop(FALSE);
            return 24;
        }
        Sleep(5u);
    }
    SudekiMpLanArenaSessionStop(FALSE);
    return 25;
}

/* A transport-only matrix: no game process, native object, or input injection.
 * Each input.client_tick acknowledges a test frame; gameplay does not use this
 * test convention. Check all four cast-kind combinations and both cleanup
 * orders through the actual session sockets, not just encode/decode helpers. */
enum { MULTICAST_FRAME_COUNT = 24u };

static void fill_multicast_snapshot(SudekiMpLanArenaSnapshot *snapshot,
    unsigned int phase) {
    unsigned int cycle=phase/3u, retiring=(cycle/4u), step=phase%3u;
    unsigned int kinds[2]={1u+(cycle%4u)/2u,1u+(cycle%2u)};
    memset(snapshot,0,sizeof(*snapshot));
    snapshot->host_tick=1000u+phase*50u;
    snapshot->match_state=SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    snapshot->combat_enabled=1u;
    snapshot->spirit_vfx_observed=1u;
    for(unsigned int i=0;i<2u;++i) {
        SudekiMpLanArenaActorSnapshot *actor=&snapshot->seat[i];
        SudekiMpLanArenaSkillFade *fade=&snapshot->cast[i].skill_fade;
        actor->actor_type=i ? SUDEKIMP_LAN_ARENA_ELCO_TYPE:SUDEKIMP_LAN_ARENA_BUKI_TYPE;
        actor->native_entity_id=actor->actor_type;
        actor->facing_z=1.f; actor->hp=100u; actor->sp=80u;
        actor->skill_sequence=(uint16_t)(7u+cycle);
        actor->skill_kind=(uint8_t)kinds[i];
        actor->skill_active=(uint8_t)(step==0u || (step==1u && i!=retiring));
        if(actor->skill_active) {
            actor->skill_presentation_valid=1u;
            actor->skill_presentation_channel_count=(uint8_t)(i ? 5u:4u);
            actor->skill_presentation_selector[0]=i ? 73:75;
            actor->skill_presentation_state[0]=1u; actor->skill_presentation_rate[0]=24.f;
            for(unsigned int c=1u;c<actor->skill_presentation_channel_count;++c)
                actor->skill_presentation_state[c]=192u;
        }
        fade->owner_seat=(uint8_t)i; fade->kind=actor->skill_kind;
        fade->skill_sequence=actor->skill_sequence;
        fade->rgb[0]=fade->rgb[1]=fade->rgb[2]=actor->skill_active ? .3f:1.f;
        if(actor->skill_active && kinds[i]==SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT) {
            SudekiMpLanArenaSpiritView *view=&snapshot->cast[i].spirit_view;
            SudekiMpLanArenaSpiritVfxSnapshot *visual=&snapshot->spirit_vfx[snapshot->spirit_vfx_count++];
            view->kind=1u; view->owner_seat=(uint8_t)i;
            view->skill_sequence=actor->skill_sequence;
            view->matrix[0]=-1.f; view->matrix[5]=view->matrix[10]=view->matrix[15]=1.f;
            view->matrix[12]=i ? 10.f:-10.f;
            view->projection[0]=1.f; view->projection[1]=.1f; view->projection[2]=1000.f;
            visual->instance_sequence=1u+cycle*2u+i;
            visual->skill_sequence=actor->skill_sequence;
            visual->owner_actor_type=i ? SUDEKIMP_LAN_ARENA_ELCO_TYPE:0u;
            visual->kind=SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE;
            visual->emitted_host_tick=1000u+cycle*150u;
            visual->rotation_xyzw[3]=1.f;
            visual->scale[0]=visual->scale[1]=visual->scale[2]=1.f;
        }
    }
    /* Retain the immutable start journal through either completion order. */
    for(unsigned int c=0;c<=cycle;++c) for(unsigned int i=0;i<2u;++i) {
        unsigned int kind=i ? 1u+c%2u:1u+(c%4u)/2u;
        if(kind==SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT) {
            unsigned int n=snapshot->spirit_audio_history_count++;
            snapshot->spirit_audio_history[n]=(SudekiMpLanArenaSpiritAudioSemanticEvent){
                (uint16_t)(n+1u),(uint16_t)(7u+c),SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START,(uint8_t)i};
        }
    }
}

static int run_multicast_peer(BOOL host,unsigned int port) {
    SudekiMpLanArenaSessionConfig config={0};
    SudekiMpLanArenaSessionStatus status;
    SudekiMpLanArenaInput input={0};
    SudekiMpLanArenaSnapshot snapshot,expected;
    DWORD started=GetTickCount();
    unsigned int phase=0u;
    BOOL requested=FALSE;
    int result=30;
    config.local_role=host ? SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL:SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    config.local_simulation_node_role=host ? SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD:
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    config.remote_ipv4=host ? NULL:"127.0.0.1";
    config.port=port; config.timeout_ms=1500u; config.game_hash=game_hash;
    config.host_actor_type=SUDEKIMP_LAN_ARENA_BUKI_TYPE;
    config.client_actor_type=SUDEKIMP_LAN_ARENA_ELCO_TYPE;
    if(!SudekiMpLanArenaSessionStart(&config)) return result;
    while((DWORD)(GetTickCount()-started)<8000u) {
        SudekiMpLanArenaSessionPoll(GetTickCount());
        result=31;
        if(!SudekiMpLanArenaSessionGetStatus(&status) || status.failure) break;
        if(host && SudekiMpLanArenaSessionTakeRemoteInput(&input)) {
            result=32;
            if(input.actor_type!=SUDEKIMP_LAN_ARENA_ELCO_TYPE || input.client_tick!=phase+1u) break;
            if(phase==MULTICAST_FRAME_COUNT) { result=0; break; }
            fill_multicast_snapshot(&snapshot,phase++);
            result=33;
            if(!SudekiMpLanArenaSessionSendSnapshot(&snapshot)) break;
        }
        if(!host && status.peer_connected && !requested) {
            input.actor_type=SUDEKIMP_LAN_ARENA_ELCO_TYPE; input.client_tick=phase+1u;
            result=34;
            if(!SudekiMpLanArenaSessionSendInput(&input)) break;
            requested=TRUE;
            if(phase==MULTICAST_FRAME_COUNT) {
                Sleep(150u); result=0; break; /* Let the host consume the final ACK. */
            }
        }
        if(!host && SudekiMpLanArenaSessionTakeRemoteSnapshot(&snapshot)) {
            fill_multicast_snapshot(&expected,phase);
            result=35;
            if(!requested || snapshot.host_tick!=expected.host_tick ||
                memcmp(snapshot.seat,expected.seat,sizeof(expected.seat)) ||
                memcmp(snapshot.cast,expected.cast,sizeof(expected.cast)) ||
                snapshot.spirit_audio_history_count!=expected.spirit_audio_history_count ||
                memcmp(snapshot.spirit_audio_history,expected.spirit_audio_history,sizeof(expected.spirit_audio_history)) ||
                snapshot.spirit_vfx_observed!=1u || snapshot.spirit_vfx_count!=expected.spirit_vfx_count ||
                memcmp(snapshot.spirit_vfx,expected.spirit_vfx,sizeof(expected.spirit_vfx))) break;
            ++phase; requested=FALSE;
        }
        Sleep(5u);
    }
    SudekiMpLanArenaSessionStop(FALSE);
    printf("multi-cast UDP %s: %s, %u/%u frames (error %d)\n",
        host ? "host":"client",result ? "FAIL":"PASS",phase,MULTICAST_FRAME_COUNT,result);
    return result;
}

int main(int argc, char **argv) {
    unsigned int index;
    unsigned long parsed_port;
    char *end = NULL;
    if (argc != 3) {
        fputs("usage: SudekiMP.LanArenaLoopbackPeerTest.exe host|client|multicast-host|multicast-client PORT\n",
            stderr);
        return 2;
    }
    parsed_port = strtoul(argv[2], &end, 10);
    if (end == argv[2] || *end != '\0' || parsed_port < 1024u ||
        parsed_port > 65535u) return 3;
    for (index = 0u; index < sizeof(game_hash); ++index) {
        game_hash[index] = (uint8_t)(0xa0u + index);
    }
    if (strcmp(argv[1], "host") == 0) return run_host((unsigned int)parsed_port);
    if (strcmp(argv[1], "client") == 0) return run_client((unsigned int)parsed_port);
    if (strcmp(argv[1], "multicast-host") == 0) return run_multicast_peer(TRUE,(unsigned int)parsed_port);
    if (strcmp(argv[1], "multicast-client") == 0) return run_multicast_peer(FALSE,(unsigned int)parsed_port);
    return 4;
}
