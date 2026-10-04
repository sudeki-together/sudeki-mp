#ifndef SUDEKIMP_PARTY_OWNERSHIP_H
#define SUDEKIMP_PARTY_OWNERSHIP_H

#include <stdint.h>

/* Canonical character order: Buki, Elco, Tal, Ailish. Player zero is the
 * authority, even when spectating. These are plain game-thread policies, not
 * permission to call native code or authentication of a transport peer. */
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
    SUDEKIMP_PARTY_SWAP_EXHAUSTED,
    SUDEKIMP_PARTY_SWAP_UNAUTHORIZED
} SudekiMpPartySwapResult;
typedef enum SudekiMpPartyControlPhase {
    SUDEKIMP_PARTY_CONTROL_AI,
    SUDEKIMP_PARTY_CONTROL_HUMAN,
    SUDEKIMP_PARTY_CONTROL_DRAINING,
    SUDEKIMP_PARTY_CONTROL_ACQUIRING,
    SUDEKIMP_PARTY_CONTROL_WAIT_ACK
} SudekiMpPartyControlPhase;
typedef enum SudekiMpPartyAbsencePolicy {
    SUDEKIMP_PARTY_AI_COVER,
    SUDEKIMP_PARTY_SHARED_PAUSE
} SudekiMpPartyAbsencePolicy;
typedef struct SudekiMpPartyAssignment {
    uint32_t world, revision, generation[4];
    /* humans is the mask of RESERVED player identities, not connected/input
     * enabled players. Departed claims remain only until native cleanup;
     * connected Menu/Away players keep their claims. Spectators have 4. */
    uint8_t humans, available, character[4];
} SudekiMpPartyAssignment;
typedef struct SudekiMpPartyOwnership {
    SudekiMpPartyAssignment assignment;
    uint32_t last_request[4], request;
    uint8_t player, previous, target, requester, displaced, handoff_control;
    SudekiMpPartySwapPhase phase;
    uint8_t connected, controlling, menu, away, paused;
    SudekiMpPartyControlPhase control[4];
    SudekiMpPartyAbsencePolicy absence_policy;
} SudekiMpPartyOwnership;

int SudekiMpPartyAssignmentValid(const SudekiMpPartyAssignment *assignment);
int SudekiMpPartyOwnershipValid(const SudekiMpPartyOwnership *state);
unsigned SudekiMpPartyCharacterOwner(const SudekiMpPartyAssignment *assignment,
    unsigned character);
/* Legacy initialization assumes every assigned player is connected and owns a
 * proved native human binding. Use Presence initialization for spectators. */
int SudekiMpPartyOwnershipInitialize(SudekiMpPartyOwnership *state,
    const SudekiMpPartyAssignment *assignment);
int SudekiMpPartyOwnershipInitializePresence(SudekiMpPartyOwnership *state,
    const SudekiMpPartyAssignment *assignment, unsigned connected,
    unsigned controlling);
SudekiMpPartySwapResult SudekiMpPartySwapRequest(SudekiMpPartyOwnership *state,
    unsigned player, unsigned character, uint32_t request,
    uint32_t world, uint32_t revision, unsigned busy_characters, int world_busy);
/* Host may take an absent player's reserved AI character for another connected
 * player. The absent actor must already have a positively completed AI drain. */
SudekiMpPartySwapResult SudekiMpPartyHostReassignRequest(SudekiMpPartyOwnership *state,
    unsigned requester, unsigned player, unsigned character, uint32_t request,
    uint32_t world, uint32_t revision, unsigned busy_characters, int world_busy);
/* Native readiness precedes Begin. After Begin only proved native restoration
 * permits AbortRestored. AI/menu swaps keep the new reservation under AI until
 * the separate acquisition below; human swaps wait for exact local binding. */
int SudekiMpPartySwapBegin(SudekiMpPartyOwnership *state, unsigned player,
    uint32_t request);
int SudekiMpPartySwapCancel(SudekiMpPartyOwnership *state, unsigned player,
    uint32_t request);
int SudekiMpPartySwapAbortRestored(SudekiMpPartyOwnership *state, unsigned player,
    uint32_t request);
int SudekiMpPartySwapCommit(SudekiMpPartyOwnership *state, unsigned player,
    uint32_t request);
/* Reservation moves after old native release, while target is still AI.
 * Native acquisition + binding ACK must follow before active human input. */
int SudekiMpPartySwapCommitReleased(SudekiMpPartyOwnership *state, unsigned player,
    uint32_t request);
int SudekiMpPartySwapAcknowledge(SudekiMpPartyOwnership *state, unsigned player,
    uint32_t request, uint32_t world, uint32_t revision);
/* Menu and Away are independent, explicit intents. Closing a menu preserves
 * Away. A new Away edge alone applies the policy; policy changes and repeated
 * Away requests never undo an explicit host resume. Input closes immediately. */
SudekiMpPartySwapResult SudekiMpPartyPresenceRequest(SudekiMpPartyOwnership *state,
    unsigned player, uint32_t request, uint32_t world, uint32_t revision,
    int menu, int away);
/* Native completion methods consume the exact current character generation.
 * Drain requires terminal task/camera proof. Acquire follows AI readiness;
 * Commit follows native ownership success; ACK follows local HUD/camera binding.
 * Unproven restore leaves the phase and dependencies retained. */
int SudekiMpPartyControlDrainComplete(SudekiMpPartyOwnership *state,
    unsigned player, uint32_t world, uint32_t generation);
int SudekiMpPartyControlAcquireBegin(SudekiMpPartyOwnership *state,
    unsigned player, uint32_t world, uint32_t revision, uint32_t generation);
int SudekiMpPartyControlAcquireCommit(SudekiMpPartyOwnership *state,
    unsigned player, uint32_t world, uint32_t generation);
int SudekiMpPartyControlAcquireAbortRestored(SudekiMpPartyOwnership *state,
    unsigned player, uint32_t world, uint32_t generation);
int SudekiMpPartyControlAcknowledge(SudekiMpPartyOwnership *state,
    unsigned player, uint32_t world, uint32_t revision, uint32_t generation);
int SudekiMpPartyPlayerInputReady(const SudekiMpPartyOwnership *state,
    unsigned player, unsigned character, uint32_t world, uint32_t revision,
    uint32_t generation);
SudekiMpPartySwapResult SudekiMpPartySetAbsencePolicy(SudekiMpPartyOwnership *state,
    unsigned requester, uint32_t request, uint32_t world, uint32_t revision,
    SudekiMpPartyAbsencePolicy policy);
SudekiMpPartySwapResult SudekiMpPartySetPaused(SudekiMpPartyOwnership *state,
    unsigned requester, uint32_t request, uint32_t world, uint32_t revision, int paused);
SudekiMpPartySwapResult SudekiMpPartyReleaseReservation(SudekiMpPartyOwnership *state,
    unsigned requester, unsigned player, uint32_t request, uint32_t world,
    uint32_t revision);
/* Transport owns credential validation. Disconnect closes input immediately;
 * its claim is retained until native drain and explicit policy retirement. */
/* Host admission reserves an unclaimed AI actor without granting control.
 * The separately authenticated lobby owns the admission request sequence. */
int SudekiMpPartyOwnershipReserve(SudekiMpPartyOwnership *state, unsigned requester,
    unsigned player, unsigned character, uint32_t world, uint32_t revision);
/* Story lobby may reserve an unavailable hero for a waiting spectator.
 * This grants no native control; Acquire still requires current availability. */
int SudekiMpPartyOwnershipReserveStory(SudekiMpPartyOwnership *state,unsigned requester,
    unsigned player,unsigned character,uint32_t world,uint32_t revision);
int SudekiMpPartyOwnershipConnect(SudekiMpPartyOwnership *state, unsigned player);
int SudekiMpPartyOwnershipDisconnect(SudekiMpPartyOwnership *state, unsigned player);
/* Legacy Join requires native acquire AND local binding already proved. New
 * runtime should use Connect, swap/acquire, and explicit ACK instead. */
int SudekiMpPartyOwnershipJoin(SudekiMpPartyOwnership *state, unsigned player,
    unsigned character);
/* Completes a proved native disconnect drain; retains the reservation. Host zero
 * cannot disconnect: loss of its process ends this authority session. */
int SudekiMpPartyOwnershipLeaveDrained(SudekiMpPartyOwnership *state,
    unsigned player);
/* Host coordinator only, after a real departure and exact native AI proof.
 * Removes the completed claim without consuming any UI request sequence.
 * Initial loading absence and connected Menu/Away are not departures. */
int SudekiMpPartyOwnershipReleaseDeparted(SudekiMpPartyOwnership *state,
    unsigned player);
#endif
