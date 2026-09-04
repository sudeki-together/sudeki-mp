#include "network/lan_arena_shared_simulation.h"

#include <limits.h>
#include <string.h>

static int valid_node_role(SudekiMpLanArenaSimulationNodeRole node_role) {
    return node_role ==
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD ||
        node_role == SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
}

static int actor_index(uint8_t actor_type) {
    if (actor_type == SUDEKIMP_LAN_ARENA_TAL_TYPE) return 0;
    if (actor_type == SUDEKIMP_LAN_ARENA_AILISH_TYPE) return 1;
    return -1;
}

static int valid_observation(
    const SudekiMpLanArenaNativeWorldObservation *observation
) {
    SudekiMpLanArenaSnapshot audio;
    if (observation == NULL) return 0;
    memset(&audio, 0, sizeof(audio));
    audio.spirit_audio_history_count =
        observation->spirit_audio_history_count;
    memcpy(audio.spirit_audio_history,
        observation->spirit_audio_history,
        sizeof(audio.spirit_audio_history));
    return
        observation->match_state <= SUDEKIMP_LAN_ARENA_MATCH_ENDED &&
        observation->combat_enabled <= 1u &&
        observation->enemy_count <= SUDEKIMP_LAN_ARENA_MAX_ENEMIES &&
        observation->native_combat_observed == 1u &&
        observation->native_resources_observed == 1u &&
        observation->native_enemies_observed == 1u &&
        SudekiMpLanArenaSpiritAudioJournalValid(&audio);
}

static int valid_actor_observation(
    const SudekiMpLanArenaActorObservation *observation,
    uint8_t expected_actor_type
) {
    return observation != NULL &&
        observation->native_actor_observed == 1u &&
        observation->actor.actor_type == expected_actor_type;
}

static int next_tick_allowed(
    const SudekiMpLanArenaSharedSimulation *simulation,
    uint32_t host_tick
) {
    return !simulation->tick_initialized ||
        SudekiMpLanArenaSequenceNewer(host_tick, simulation->last_host_tick);
}

static int next_match_state_allowed(
    const SudekiMpLanArenaSharedSimulation *simulation,
    uint8_t match_state
) {
    uint8_t previous;
    if (!simulation->frame_valid) {
        return match_state == SUDEKIMP_LAN_ARENA_MATCH_WAITING ||
            match_state == SUDEKIMP_LAN_ARENA_MATCH_ACTIVE;
    }
    previous = simulation->frame.match_state;
    if (previous == SUDEKIMP_LAN_ARENA_MATCH_WAITING) {
        return match_state == SUDEKIMP_LAN_ARENA_MATCH_WAITING ||
            match_state == SUDEKIMP_LAN_ARENA_MATCH_ACTIVE ||
            match_state == SUDEKIMP_LAN_ARENA_MATCH_ENDED;
    }
    if (previous == SUDEKIMP_LAN_ARENA_MATCH_ACTIVE) {
        return match_state == SUDEKIMP_LAN_ARENA_MATCH_ACTIVE ||
            match_state == SUDEKIMP_LAN_ARENA_MATCH_ENDED;
    }
    return previous == SUDEKIMP_LAN_ARENA_MATCH_ENDED &&
        match_state == SUDEKIMP_LAN_ARENA_MATCH_ENDED;
}

static int skill_sequence_newer(uint16_t candidate, uint16_t previous) {
    uint16_t distance = (uint16_t)(candidate - previous);
    return distance != 0u && distance < UINT16_C(0x8000);
}

static int next_actor_skill_allowed(
    const SudekiMpLanArenaActorSnapshot *previous,
    const SudekiMpLanArenaActorSnapshot *candidate
) {
    if (previous == NULL || candidate == NULL) return 0;
    if (previous->skill_sequence == 0u) {
        /* Zero is the session baseline. Any structurally valid first
         * transaction may follow it, including an inactive endpoint when
         * packet coalescing hid the complete active interval. */
        return 1;
    }
    if (candidate->skill_sequence == 0u) return 0;
    if (candidate->skill_sequence != previous->skill_sequence) {
        return skill_sequence_newer(
            candidate->skill_sequence, previous->skill_sequence);
    }
    /* A transaction's actor-local capability tuple is immutable. Renderer
     * selectors and phases may advance within it, but kind/slot/cost may not
     * be rewritten by a later datagram. Once retired, the same sequence can
     * never become active again. */
    return candidate->skill_kind == previous->skill_kind &&
        candidate->skill_slot == previous->skill_slot &&
        candidate->skill_cost == previous->skill_cost &&
        !(previous->skill_active == 0u && candidate->skill_active != 0u);
}

static int next_skill_lifecycle_allowed(
    const SudekiMpLanArenaSharedSimulation *simulation,
    const SudekiMpLanArenaSnapshot *candidate
) {
    return simulation != NULL && candidate != NULL &&
        (!simulation->frame_valid ||
         (next_actor_skill_allowed(
              &simulation->frame.tal, &candidate->tal) &&
          next_actor_skill_allowed(
              &simulation->frame.ailish, &candidate->ailish)));
}

static int spirit_audio_event_equal(
    const SudekiMpLanArenaSpiritAudioSemanticEvent *left,
    const SudekiMpLanArenaSpiritAudioSemanticEvent *right
) {
    return left != NULL && right != NULL &&
        left->event_sequence == right->event_sequence &&
        left->skill_sequence == right->skill_sequence &&
        left->cue == right->cue;
}

static int spirit_audio_event_matches_current_spirit(
    const SudekiMpLanArenaSnapshot *frame,
    const SudekiMpLanArenaSpiritAudioSemanticEvent *event
) {
    return frame != NULL && event != NULL &&
        frame->match_state == SUDEKIMP_LAN_ARENA_MATCH_ACTIVE &&
        frame->combat_enabled == 1u && frame->tal.skill_active == 1u &&
        frame->tal.skill_kind ==
            SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT &&
        frame->tal.skill_sequence == event->skill_sequence;
}

static int next_spirit_audio_journal_allowed(
    const SudekiMpLanArenaSharedSimulation *simulation,
    const SudekiMpLanArenaSnapshot *candidate
) {
    const SudekiMpLanArenaSnapshot *previous;
    const SudekiMpLanArenaSpiritAudioSemanticEvent *previous_latest;
    const SudekiMpLanArenaSpiritAudioSemanticEvent *candidate_latest;
    unsigned int previous_count;
    unsigned int candidate_count;
    unsigned int candidate_previous_latest = UINT_MAX;
    unsigned int index;
    int require_current_match;

    if (simulation == NULL || candidate == NULL) return 0;
    require_current_match = simulation->node_role ==
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
    candidate_count = candidate->spirit_audio_history_count;
    if (!simulation->frame_valid) {
        return candidate_count == 0u || !require_current_match ||
            spirit_audio_event_matches_current_spirit(
                candidate,
                &candidate->spirit_audio_history[candidate_count - 1u]);
    }
    previous = &simulation->frame;
    previous_count = previous->spirit_audio_history_count;
    if (previous_count == 0u) {
        return candidate_count == 0u || !require_current_match ||
            spirit_audio_event_matches_current_spirit(
                candidate,
                &candidate->spirit_audio_history[candidate_count - 1u]);
    }
    if (candidate_count == 0u) return 0;
    previous_latest =
        &previous->spirit_audio_history[previous_count - 1u];
    candidate_latest =
        &candidate->spirit_audio_history[candidate_count - 1u];
    if (candidate_latest->event_sequence ==
            previous_latest->event_sequence) {
        if (candidate_count != previous_count) return 0;
        for (index = 0u; index < candidate_count; ++index) {
            if (!spirit_audio_event_equal(
                    &previous->spirit_audio_history[index],
                    &candidate->spirit_audio_history[index])) return 0;
        }
        return 1;
    }
    if (!skill_sequence_newer(
            candidate_latest->event_sequence,
            previous_latest->event_sequence) ||
        (require_current_match &&
         !spirit_audio_event_matches_current_spirit(
             candidate, candidate_latest))) return 0;

    for (index = 0u; index < candidate_count; ++index) {
        if (candidate->spirit_audio_history[index].event_sequence ==
                previous_latest->event_sequence) {
            candidate_previous_latest = index;
            break;
        }
    }
    if (candidate_previous_latest != UINT_MAX) {
        unsigned int overlap = candidate_previous_latest + 1u;
        unsigned int previous_start;
        if (overlap > previous_count) return 0;
        previous_start = previous_count - overlap;
        for (index = 0u; index < overlap; ++index) {
            if (!spirit_audio_event_equal(
                    &previous->spirit_audio_history[previous_start + index],
                    &candidate->spirit_audio_history[index])) return 0;
        }
        return 1;
    }
    /* A complete bounded-window replacement is admissible only when its
     * oldest event is already newer than the last accepted event. This is a
     * recovery path for a long packet gap, never permission to regress or
     * reinterpret an existing journal entry. */
    return skill_sequence_newer(
            candidate->spirit_audio_history[0].event_sequence,
            previous_latest->event_sequence) &&
        skill_sequence_newer(
            candidate->spirit_audio_history[0].skill_sequence,
            previous_latest->skill_sequence);
}

static void commit_frame(
    SudekiMpLanArenaSharedSimulation *simulation,
    const SudekiMpLanArenaSnapshot *frame
) {
    simulation->frame = *frame;
    simulation->last_host_tick = frame->host_tick;
    simulation->tick_initialized = 1u;
    simulation->frame_valid = 1u;
    ++simulation->revision;
    if (simulation->revision == 0u) simulation->revision = 1u;
}

void SudekiMpLanArenaSharedSimulationReset(
    SudekiMpLanArenaSharedSimulation *simulation
) {
    if (simulation != NULL) memset(simulation, 0, sizeof(*simulation));
}

int SudekiMpLanArenaSharedSimulationBegin(
    SudekiMpLanArenaSharedSimulation *simulation,
    SudekiMpLanArenaSimulationNodeRole node_role,
    uint64_t session_token
) {
    if (simulation == NULL || !valid_node_role(node_role) ||
        session_token == 0u) return 0;
    memset(simulation, 0, sizeof(*simulation));
    simulation->node_role = (uint8_t)node_role;
    simulation->session_token = session_token;
    return 1;
}

int SudekiMpLanArenaSharedSimulationSessionExact(
    const SudekiMpLanArenaSharedSimulation *simulation,
    SudekiMpLanArenaSimulationNodeRole node_role,
    uint64_t session_token
) {
    return simulation != NULL && valid_node_role(node_role) &&
        session_token != 0u && simulation->node_role == (uint8_t)node_role &&
        simulation->session_token == session_token;
}

int SudekiMpLanArenaSharedSimulationAdmitPlayerInput(
    SudekiMpLanArenaSharedSimulation *simulation,
    uint64_t session_token,
    uint8_t actor_type,
    const SudekiMpLanArenaInput *input
) {
    int index = actor_index(actor_type);
    uint8_t mask;
    if (!SudekiMpLanArenaSharedSimulationSessionExact(
            simulation,
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD,
            session_token) || index < 0 || input == NULL ||
        input->actor_type != actor_type ||
        !SudekiMpLanArenaInputValid(input) ||
        (simulation->frame_valid &&
         simulation->frame.match_state ==
             SUDEKIMP_LAN_ARENA_MATCH_ENDED)) return 0;
    mask = (uint8_t)(1u << (unsigned int)index);
    if ((simulation->player_input_valid_mask & mask) != 0u &&
        !SudekiMpLanArenaSequenceNewer(
            input->sequence,
            simulation->player_input[index].sequence)) return 0;
    simulation->player_input[index] = *input;
    simulation->player_input_valid_mask |= mask;
    ++simulation->player_input_revision[index];
    if (simulation->player_input_revision[index] == 0u) {
        simulation->player_input_revision[index] = 1u;
    }
    return 1;
}

int SudekiMpLanArenaSharedSimulationReadPlayerInput(
    const SudekiMpLanArenaSharedSimulation *simulation,
    uint8_t actor_type,
    SudekiMpLanArenaInput *input,
    uint32_t *revision
) {
    int index = actor_index(actor_type);
    uint8_t mask;
    if (simulation == NULL || input == NULL || index < 0 ||
        simulation->session_token == 0u ||
        simulation->node_role !=
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD) {
        return 0;
    }
    mask = (uint8_t)(1u << (unsigned int)index);
    if ((simulation->player_input_valid_mask & mask) == 0u) return 0;
    *input = simulation->player_input[index];
    if (revision != NULL) {
        *revision = simulation->player_input_revision[index];
    }
    return 1;
}

int SudekiMpLanArenaSharedSimulationCommitNativeFrame(
    SudekiMpLanArenaSharedSimulation *simulation,
    uint64_t session_token,
    const SudekiMpLanArenaNativeWorldObservation *observation,
    const SudekiMpLanArenaActorObservation *tal,
    const SudekiMpLanArenaActorObservation *ailish
) {
    SudekiMpLanArenaSnapshot committed;
    if (!SudekiMpLanArenaSharedSimulationSessionExact(
            simulation,
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD,
            session_token) ||
        !valid_observation(observation) ||
        !valid_actor_observation(tal, SUDEKIMP_LAN_ARENA_TAL_TYPE) ||
        !valid_actor_observation(ailish, SUDEKIMP_LAN_ARENA_AILISH_TYPE) ||
        !next_match_state_allowed(simulation, observation->match_state) ||
        !next_tick_allowed(simulation, observation->host_tick)) return 0;
    memset(&committed, 0, sizeof(committed));
    committed.host_tick = observation->host_tick;
    committed.match_state = observation->match_state;
    committed.combat_enabled = observation->combat_enabled;
    committed.tal = tal->actor;
    committed.ailish = ailish->actor;
    committed.tal.hp = observation->tal_hp;
    committed.tal.sp = observation->tal_sp;
    committed.ailish.hp = observation->ailish_hp;
    committed.ailish.sp = observation->ailish_sp;
    committed.enemy_count = observation->enemy_count;
    memcpy(committed.enemies, observation->enemies,
        sizeof(committed.enemies));
    committed.spirit_audio_history_count =
        observation->spirit_audio_history_count;
    memcpy(committed.spirit_audio_history,
        observation->spirit_audio_history,
        sizeof(committed.spirit_audio_history));
    if ((simulation->player_input_valid_mask & 0x02u) != 0u) {
        committed.acknowledged_input =
            simulation->player_input[1].sequence;
    } else {
        committed.acknowledged_input = 0u;
    }
    if (!SudekiMpLanArenaSnapshotValid(&committed) ||
        !next_skill_lifecycle_allowed(simulation, &committed) ||
        !next_spirit_audio_journal_allowed(simulation, &committed)) return 0;
    commit_frame(simulation, &committed);
    return 1;
}

int SudekiMpLanArenaSharedSimulationAcceptReplicaFrame(
    SudekiMpLanArenaSharedSimulation *simulation,
    uint64_t session_token,
    const SudekiMpLanArenaSnapshot *frame
) {
    if (!SudekiMpLanArenaSharedSimulationSessionExact(
            simulation, SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA,
            session_token) || frame == NULL ||
        !SudekiMpLanArenaSnapshotValid(frame) ||
        !next_match_state_allowed(simulation, frame->match_state) ||
        !next_skill_lifecycle_allowed(simulation, frame) ||
        !next_spirit_audio_journal_allowed(simulation, frame) ||
        (simulation->frame_valid &&
         simulation->frame.acknowledged_input != 0u &&
         frame->acknowledged_input !=
             simulation->frame.acknowledged_input &&
         !SudekiMpLanArenaSequenceNewer(
             frame->acknowledged_input,
             simulation->frame.acknowledged_input)) ||
        !next_tick_allowed(simulation, frame->host_tick)) return 0;
    commit_frame(simulation, frame);
    return 1;
}

int SudekiMpLanArenaSharedSimulationReadFrame(
    const SudekiMpLanArenaSharedSimulation *simulation,
    SudekiMpLanArenaSnapshot *frame,
    uint32_t *revision
) {
    if (simulation == NULL || frame == NULL || !simulation->frame_valid ||
        !valid_node_role((SudekiMpLanArenaSimulationNodeRole)
            simulation->node_role) || simulation->session_token == 0u) {
        return 0;
    }
    *frame = simulation->frame;
    if (revision != NULL) *revision = simulation->revision;
    return 1;
}
