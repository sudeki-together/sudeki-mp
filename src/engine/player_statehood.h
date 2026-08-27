#ifndef SUDEKIMP_PLAYER_STATEHOOD_H
#define SUDEKIMP_PLAYER_STATEHOOD_H

#include <stdint.h>

enum {
    SUDEKIMP_PLAYER_STATEHOOD_MAX_PLAYERS = 4u,
    SUDEKIMP_INTERACTION_REQUEST_LIFETIME_MS = 5000u
};

typedef enum SudekiMpInteractionKind {
    SUDEKIMP_INTERACTION_NONE = 0,
    SUDEKIMP_INTERACTION_GENERIC_REQUEST,
    SUDEKIMP_INTERACTION_SHOP,
    SUDEKIMP_INTERACTION_BLACKSMITH,
    SUDEKIMP_INTERACTION_PICKUP,
    SUDEKIMP_INTERACTION_CHEST,
    SUDEKIMP_INTERACTION_NONPROGRESSION_SWITCH,
    SUDEKIMP_INTERACTION_DIALOGUE,
    SUDEKIMP_INTERACTION_TRAVEL,
    SUDEKIMP_INTERACTION_QUEST,
    SUDEKIMP_INTERACTION_SAVE,
    SUDEKIMP_INTERACTION_CUTSCENE,
    SUDEKIMP_INTERACTION_UNKNOWN
} SudekiMpInteractionKind;

typedef enum SudekiMpInteractionAuthority {
    SUDEKIMP_INTERACTION_AUTHORITY_LOCAL_ACTOR = 0,
    SUDEKIMP_INTERACTION_AUTHORITY_REQUEST_ONLY,
    SUDEKIMP_INTERACTION_AUTHORITY_SERIALIZED_SHARED,
    SUDEKIMP_INTERACTION_AUTHORITY_HOST_ONLY
} SudekiMpInteractionAuthority;

typedef enum SudekiMpInteractionPresentation {
    SUDEKIMP_INTERACTION_PRESENTATION_NONE = 0,
    SUDEKIMP_INTERACTION_PRESENTATION_REQUEST_OVERLAY,
    SUDEKIMP_INTERACTION_PRESENTATION_OWNER_VIEWPORT,
    SUDEKIMP_INTERACTION_PRESENTATION_SHARED_FULL_WIDTH
} SudekiMpInteractionPresentation;

typedef enum SudekiMpInteractionSessionState {
    SUDEKIMP_INTERACTION_SESSION_IDLE = 0,
    SUDEKIMP_INTERACTION_SESSION_REQUESTED,
    SUDEKIMP_INTERACTION_SESSION_ACTIVE,
    SUDEKIMP_INTERACTION_SESSION_QUARANTINED
} SudekiMpInteractionSessionState;

typedef struct SudekiMpPlayerLease {
    uintptr_t actor;
    uint32_t actor_generation;
    int human_present;
} SudekiMpPlayerLease;

/* Every authoritative world request must eventually carry this whole tuple.
 * A generic attention request deliberately has target_known == 0 and can
 * never be promoted into a native world action by this coordinator. */
typedef struct SudekiMpInteractionProvenance {
    uint32_t serial;
    uint32_t player_index;
    uintptr_t actor;
    uint32_t actor_generation;
    uintptr_t target;
    uint32_t source_generation;
    SudekiMpInteractionKind kind;
    int target_known;
} SudekiMpInteractionProvenance;

typedef struct SudekiMpPlayerStatehood {
    SudekiMpPlayerLease players[SUDEKIMP_PLAYER_STATEHOOD_MAX_PLAYERS];
    SudekiMpInteractionSessionState session_state;
    SudekiMpInteractionProvenance session;
    uint32_t request_started_ms;
    uint32_t request_deadline_ms;
    uint32_t next_serial;
} SudekiMpPlayerStatehood;

typedef struct SudekiMpPlayerStatehoodSnapshot {
    SudekiMpInteractionSessionState state;
    SudekiMpInteractionProvenance provenance;
    uint32_t remaining_ms;
} SudekiMpPlayerStatehoodSnapshot;

void SudekiMpPlayerStatehoodInitialize(SudekiMpPlayerStatehood *statehood);
int SudekiMpPlayerStatehoodPublishPlayer(
    SudekiMpPlayerStatehood *statehood,
    uint32_t player_index,
    uintptr_t actor,
    uint32_t actor_generation,
    int human_present
);
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
);
int SudekiMpPlayerStatehoodCancelRequest(
    SudekiMpPlayerStatehood *statehood,
    uint32_t player_index
);
int SudekiMpPlayerStatehoodCommitKnownRequest(
    SudekiMpPlayerStatehood *statehood,
    uint32_t serial,
    uintptr_t actor,
    uint32_t actor_generation,
    uintptr_t target,
    uint32_t source_generation
);
void SudekiMpPlayerStatehoodObserveNativeModal(
    SudekiMpPlayerStatehood *statehood,
    SudekiMpInteractionKind kind,
    uint32_t owner_index,
    uint32_t now_ms
);
void SudekiMpPlayerStatehoodService(
    SudekiMpPlayerStatehood *statehood,
    uint32_t now_ms
);
int SudekiMpPlayerStatehoodGetSnapshot(
    const SudekiMpPlayerStatehood *statehood,
    uint32_t now_ms,
    SudekiMpPlayerStatehoodSnapshot *snapshot
);
SudekiMpInteractionAuthority SudekiMpInteractionAuthorityForKind(
    SudekiMpInteractionKind kind
);
SudekiMpInteractionPresentation SudekiMpInteractionPresentationForKind(
    SudekiMpInteractionKind kind,
    int independently_virtualized
);

/* One process-global coordinator is shared by input, world-interaction, UI,
 * and render hooks. It stores runtime leases only and is never save data. */
SudekiMpPlayerStatehood *SudekiMpPlayerStatehoodRuntime(void);

#endif
