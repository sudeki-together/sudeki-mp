#ifndef SUDEKIMP_PARTY_OWNERSHIP_H
#define SUDEKIMP_PARTY_OWNERSHIP_H

#include <stdint.h>

/* Character indices are canonical snapshot order: Buki, Elco, Tal, Ailish.
 * Player indices are connection slots. Neither is a native pointer/lease.
 * This game-thread policy never grants permission to call native code. */
enum { SUDEKIMP_PARTY_CHARACTERS = 4, SUDEKIMP_PARTY_NO_CHARACTER = 4 };
typedef enum SudekiMpPartySwapPhase {
    SUDEKIMP_PARTY_SWAP_IDLE,
    SUDEKIMP_PARTY_SWAP_RESERVED,
    SUDEKIMP_PARTY_SWAP_HANDOFF,
    SUDEKIMP_PARTY_SWAP_WAIT_ACK
} SudekiMpPartySwapPhase;
typedef enum SudekiMpPartySwapResult {
    SUDEKIMP_PARTY_SWAP_OK,
    SUDEKIMP_PARTY_SWAP_INVALID,
    SUDEKIMP_PARTY_SWAP_STALE,
    SUDEKIMP_PARTY_SWAP_BUSY,
    SUDEKIMP_PARTY_SWAP_OCCUPIED,
    SUDEKIMP_PARTY_SWAP_UNAVAILABLE,
    SUDEKIMP_PARTY_SWAP_SAME_CHARACTER,
    SUDEKIMP_PARTY_SWAP_EXHAUSTED
} SudekiMpPartySwapResult;
typedef struct SudekiMpPartyAssignment {
    uint32_t world, revision, generation[4];
    uint8_t humans, available, character[4];
} SudekiMpPartyAssignment;
typedef struct SudekiMpPartyOwnership {
    SudekiMpPartyAssignment assignment;
    uint32_t last_request[4], request;
    uint8_t player, previous, target;
    SudekiMpPartySwapPhase phase;
} SudekiMpPartyOwnership;

int SudekiMpPartyAssignmentValid(const SudekiMpPartyAssignment *assignment);
unsigned SudekiMpPartyCharacterOwner(const SudekiMpPartyAssignment *assignment,
    unsigned character);
int SudekiMpPartyOwnershipInitialize(SudekiMpPartyOwnership *state,
    const SudekiMpPartyAssignment *assignment);
SudekiMpPartySwapResult SudekiMpPartySwapRequest(SudekiMpPartyOwnership *state,
    unsigned player, unsigned character, uint32_t request,
    uint32_t world, uint32_t revision, unsigned busy_characters, int world_busy);
/* The adapter first proves native readiness. Once handoff starts, Cancel is
 * forbidden: only a positively restored old binding may AbortRestored. */
int SudekiMpPartySwapBegin(SudekiMpPartyOwnership *state, unsigned player,
    uint32_t request);
int SudekiMpPartySwapCancel(SudekiMpPartyOwnership *state, unsigned player,
    uint32_t request);
int SudekiMpPartySwapAbortRestored(SudekiMpPartyOwnership *state, unsigned player,
    uint32_t request);
/* Commit follows native handoff success; ACK follows the requester's exact
 * local controller/camera/HUD binding. Input remains closed until ACK. */
int SudekiMpPartySwapCommit(SudekiMpPartyOwnership *state, unsigned player,
    uint32_t request);
int SudekiMpPartySwapAcknowledge(SudekiMpPartyOwnership *state, unsigned player,
    uint32_t request, uint32_t world, uint32_t revision);
int SudekiMpPartyPlayerInputReady(const SudekiMpPartyOwnership *state,
    unsigned player, unsigned character, uint32_t world, uint32_t revision,
    uint32_t generation);
/* Caller must positively drain native ownership before removal. Joining must
 * acquire the chosen character before publishing this policy change. No host
 * migration is implied; player zero cannot be removed from a live session. */
int SudekiMpPartyOwnershipJoin(SudekiMpPartyOwnership *state, unsigned player,
    unsigned character);
int SudekiMpPartyOwnershipLeaveDrained(SudekiMpPartyOwnership *state,
    unsigned player);
#endif
