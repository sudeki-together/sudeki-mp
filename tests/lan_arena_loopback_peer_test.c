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
    snapshot->tal.actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    snapshot->tal.native_entity_id = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    snapshot->tal.facing_z = 1.0f;
    snapshot->tal.hp = 6850u;
    snapshot->tal.sp = 440u;
    snapshot->tal.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_IDLE;
    snapshot->ailish.actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    snapshot->ailish.native_entity_id = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    snapshot->ailish.x = 3.0f;
    snapshot->ailish.facing_x = 1.0f;
    snapshot->ailish.hp = 3300u;
    snapshot->ailish.sp = 440u;
    snapshot->ailish.animation_state = SUDEKIMP_LAN_ARENA_ANIMATION_ACTION;
    snapshot->ailish.combat_state = SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK;
    snapshot->ailish.action_variant = SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE;
    snapshot->ailish.action_phase_valid = 1u;
    snapshot->ailish.action_phase_q8 = 18u * 256u;
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
    config.port = port;
    config.timeout_ms = 1500u;
    config.game_hash = game_hash;
    if (!SudekiMpLanArenaSessionStart(&config)) return 10;
    while ((DWORD)(GetTickCount() - started) < 5000u) {
        DWORD now = GetTickCount();
        SudekiMpLanArenaSessionPoll(now);
        if (SudekiMpLanArenaSessionTakeRemoteInput(&input)) {
            if (input.world_direction_x != 16384 ||
                input.world_direction_z != -8192 ||
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
    config.remote_ipv4 = "127.0.0.1";
    config.port = port;
    config.timeout_ms = 1500u;
    config.game_hash = game_hash;
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
                snapshot.tal.hp != 6850u || snapshot.ailish.hp != 3300u ||
                snapshot.ailish.animation_state !=
                    SUDEKIMP_LAN_ARENA_ANIMATION_ACTION ||
                snapshot.ailish.combat_state !=
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

int main(int argc, char **argv) {
    unsigned int index;
    unsigned long parsed_port;
    char *end = NULL;
    if (argc != 3) {
        fputs("usage: SudekiMP.LanArenaLoopbackPeerTest.exe host|client PORT\n",
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
    return 4;
}
