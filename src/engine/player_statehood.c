#include "engine/player_statehood.h"

#include <string.h>

static SudekiMpPlayerStatehood runtime_statehood;

static uint32_t next_serial(SudekiMpPlayerStatehood *statehood) {
    ++statehood->next_serial;
    if (statehood->next_serial == 0u) {
        ++statehood->next_serial;
    }
    return statehood->next_serial;
}

static void clear_session(SudekiMpPlayerStatehood *statehood) {
    statehood->session_state = SUDEKIMP_INTERACTION_SESSION_IDLE;
    memset(&statehood->session, 0, sizeof(statehood->session));
    statehood->request_started_ms = 0u;
    statehood->request_deadline_ms = 0u;
}

void SudekiMpPlayerStatehoodInitialize(SudekiMpPlayerStatehood *statehood) {
    if (statehood == NULL) {
        return;
    }
    memset(statehood, 0, sizeof(*statehood));
}

SudekiMpInteractionAuthority SudekiMpInteractionAuthorityForKind(
    SudekiMpInteractionKind kind
) {
    switch (kind) {
    case SUDEKIMP_INTERACTION_NONE:
        return SUDEKIMP_INTERACTION_AUTHORITY_LOCAL_ACTOR;
    case SUDEKIMP_INTERACTION_GENERIC_REQUEST:
        return SUDEKIMP_INTERACTION_AUTHORITY_REQUEST_ONLY;
    case SUDEKIMP_INTERACTION_SHOP:
    case SUDEKIMP_INTERACTION_BLACKSMITH:
    case SUDEKIMP_INTERACTION_PICKUP:
    case SUDEKIMP_INTERACTION_CHEST:
    case SUDEKIMP_INTERACTION_NONPROGRESSION_SWITCH:
        return SUDEKIMP_INTERACTION_AUTHORITY_SERIALIZED_SHARED;
    case SUDEKIMP_INTERACTION_DIALOGUE:
    case SUDEKIMP_INTERACTION_TRAVEL:
    case SUDEKIMP_INTERACTION_QUEST:
    case SUDEKIMP_INTERACTION_SAVE:
    case SUDEKIMP_INTERACTION_CUTSCENE:
    case SUDEKIMP_INTERACTION_UNKNOWN:
    default:
        return SUDEKIMP_INTERACTION_AUTHORITY_HOST_ONLY;
    }
}

SudekiMpInteractionPresentation SudekiMpInteractionPresentationForKind(
    SudekiMpInteractionKind kind,
    int independently_virtualized
) {
    if (kind == SUDEKIMP_INTERACTION_GENERIC_REQUEST) {
        return SUDEKIMP_INTERACTION_PRESENTATION_REQUEST_OVERLAY;
    }
    if (kind == SUDEKIMP_INTERACTION_SHOP ||
        kind == SUDEKIMP_INTERACTION_BLACKSMITH) {
        return independently_virtualized ?
            SUDEKIMP_INTERACTION_PRESENTATION_OWNER_VIEWPORT :
            SUDEKIMP_INTERACTION_PRESENTATION_SHARED_FULL_WIDTH;
    }
    return SUDEKIMP_INTERACTION_PRESENTATION_NONE;
}

int SudekiMpPlayerStatehoodPublishPlayer(
    SudekiMpPlayerStatehood *statehood,
    uint32_t player_index,
    uintptr_t actor,
    uint32_t actor_generation,
    int human_present
) {
    SudekiMpPlayerLease *player;

    if (statehood == NULL ||
        player_index >= SUDEKIMP_PLAYER_STATEHOOD_MAX_PLAYERS) {
        return 0;
    }
    player = &statehood->players[player_index];
    if (statehood->session_state != SUDEKIMP_INTERACTION_SESSION_IDLE &&
        statehood->session.player_index == player_index &&
        (actor != statehood->session.actor ||
         actor_generation != statehood->session.actor_generation ||
         !human_present)) {
        if (statehood->session_state ==
                SUDEKIMP_INTERACTION_SESSION_REQUESTED) {
            clear_session(statehood);
        } else {
            statehood->session_state =
                SUDEKIMP_INTERACTION_SESSION_QUARANTINED;
        }
    }
    player->actor = actor;
    player->actor_generation = actor_generation;
    player->human_present = human_present != 0;
    return 1;
}

int SudekiMpPlayerStatehoodRequest(
    SudekiMpPlayerStatehood *statehood,
    uint32_t player_index,
    uintptr_t actor,
    uint32_t actor_generation,
    SudekiMpInteractionKind kind,
    uintptr_t target,
    int target_known,
    uint32_t source_generation,
    uint32_t now_ms,
    uint32_t *serial
) {
    SudekiMpInteractionAuthority authority;
    const SudekiMpPlayerLease *player;

    if (statehood == NULL ||
        player_index >= SUDEKIMP_PLAYER_STATEHOOD_MAX_PLAYERS ||
        kind == SUDEKIMP_INTERACTION_NONE ||
        statehood->session_state != SUDEKIMP_INTERACTION_SESSION_IDLE) {
        return 0;
    }
    player = &statehood->players[player_index];
    if (!player->human_present || player->actor == 0u ||
        player->actor != actor ||
        player->actor_generation != actor_generation) {
        return 0;
    }
    authority = SudekiMpInteractionAuthorityForKind(kind);
    if (authority == SUDEKIMP_INTERACTION_AUTHORITY_LOCAL_ACTOR ||
        (authority == SUDEKIMP_INTERACTION_AUTHORITY_HOST_ONLY &&
         player_index != 0u) ||
        (target_known && target == 0u) ||
        (!target_known && kind != SUDEKIMP_INTERACTION_GENERIC_REQUEST)) {
        return 0;
    }
    statehood->session_state = SUDEKIMP_INTERACTION_SESSION_REQUESTED;
    memset(&statehood->session, 0, sizeof(statehood->session));
    statehood->session.serial = next_serial(statehood);
    statehood->session.player_index = player_index;
    statehood->session.actor = actor;
    statehood->session.actor_generation = actor_generation;
    statehood->session.target = target;
    statehood->session.source_generation = source_generation;
    statehood->session.kind = kind;
    statehood->session.target_known = target_known != 0;
    statehood->request_started_ms = now_ms;
    statehood->request_deadline_ms = now_ms +
        SUDEKIMP_INTERACTION_REQUEST_LIFETIME_MS;
    if (serial != NULL) {
        *serial = statehood->session.serial;
    }
    return 1;
}

int SudekiMpPlayerStatehoodCancelRequest(
    SudekiMpPlayerStatehood *statehood,
    uint32_t player_index
) {
    if (statehood == NULL ||
        statehood->session_state != SUDEKIMP_INTERACTION_SESSION_REQUESTED ||
        statehood->session.player_index != player_index) {
        return 0;
    }
    clear_session(statehood);
    return 1;
}

int SudekiMpPlayerStatehoodCommitKnownRequest(
    SudekiMpPlayerStatehood *statehood,
    uint32_t serial,
    uintptr_t actor,
    uint32_t actor_generation,
    uintptr_t target,
    uint32_t source_generation
) {
    const SudekiMpInteractionProvenance *request;

    if (statehood == NULL ||
        statehood->session_state != SUDEKIMP_INTERACTION_SESSION_REQUESTED) {
        return 0;
    }
    request = &statehood->session;
    if (!request->target_known || serial == 0u ||
        request->serial != serial || request->actor != actor ||
        request->actor_generation != actor_generation ||
        request->target != target ||
        request->source_generation != source_generation) {
        return 0;
    }
    statehood->session_state = SUDEKIMP_INTERACTION_SESSION_ACTIVE;
    statehood->request_started_ms = 0u;
    statehood->request_deadline_ms = 0u;
    return 1;
}

void SudekiMpPlayerStatehoodObserveNativeModal(
    SudekiMpPlayerStatehood *statehood,
    SudekiMpInteractionKind kind,
    uint32_t owner_index,
    uint32_t now_ms
) {
    const SudekiMpPlayerLease *owner;

    if (statehood == NULL) {
        return;
    }
    if (kind == SUDEKIMP_INTERACTION_NONE) {
        if (statehood->session_state == SUDEKIMP_INTERACTION_SESSION_ACTIVE ||
            statehood->session_state ==
                SUDEKIMP_INTERACTION_SESSION_QUARANTINED) {
            clear_session(statehood);
        }
        return;
    }
    if (owner_index >= SUDEKIMP_PLAYER_STATEHOOD_MAX_PLAYERS) {
        return;
    }
    if (statehood->session_state == SUDEKIMP_INTERACTION_SESSION_ACTIVE &&
        statehood->session.kind == kind &&
        statehood->session.player_index == owner_index) {
        return;
    }
    owner = &statehood->players[owner_index];
    clear_session(statehood);
    statehood->session_state = SUDEKIMP_INTERACTION_SESSION_ACTIVE;
    statehood->session.serial = next_serial(statehood);
    statehood->session.player_index = owner_index;
    statehood->session.actor = owner->actor;
    statehood->session.actor_generation = owner->actor_generation;
    statehood->session.kind = kind;
    statehood->session.target_known = 0;
    statehood->request_started_ms = now_ms;
}

void SudekiMpPlayerStatehoodService(
    SudekiMpPlayerStatehood *statehood,
    uint32_t now_ms
) {
    if (statehood == NULL ||
        statehood->session_state != SUDEKIMP_INTERACTION_SESSION_REQUESTED) {
        return;
    }
    if ((uint32_t)(now_ms - statehood->request_started_ms) >=
        SUDEKIMP_INTERACTION_REQUEST_LIFETIME_MS) {
        clear_session(statehood);
    }
}

int SudekiMpPlayerStatehoodGetSnapshot(
    const SudekiMpPlayerStatehood *statehood,
    uint32_t now_ms,
    SudekiMpPlayerStatehoodSnapshot *snapshot
) {
    if (statehood == NULL || snapshot == NULL) {
        return 0;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->state = statehood->session_state;
    snapshot->provenance = statehood->session;
    if (statehood->session_state == SUDEKIMP_INTERACTION_SESSION_REQUESTED) {
        uint32_t elapsed = now_ms - statehood->request_started_ms;
        snapshot->remaining_ms = elapsed >=
                SUDEKIMP_INTERACTION_REQUEST_LIFETIME_MS ?
            0u : SUDEKIMP_INTERACTION_REQUEST_LIFETIME_MS - elapsed;
    }
    return statehood->session_state != SUDEKIMP_INTERACTION_SESSION_IDLE;
}

SudekiMpPlayerStatehood *SudekiMpPlayerStatehoodRuntime(void) {
    return &runtime_statehood;
}
