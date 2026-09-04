#ifndef SUDEKIMP_LAN_ARENA_CLIENT_SKILL_HANDOFF_H
#define SUDEKIMP_LAN_ARENA_CLIENT_SKILL_HANDOFF_H

#include <stdint.h>

typedef enum SudekiMpLanArenaClientSkillHandoffDecision {
    SUDEKIMP_LAN_ARENA_CLIENT_SKILL_ACCEPT = 0,
    SUDEKIMP_LAN_ARENA_CLIENT_SKILL_CONTINUE = 1,
    SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETIRE_THEN_ACCEPT = 2,
    SUDEKIMP_LAN_ARENA_CLIENT_SKILL_DRAIN = 3
} SudekiMpLanArenaClientSkillHandoffDecision;

typedef enum SudekiMpLanArenaClientOtherSkillDecision {
    SUDEKIMP_LAN_ARENA_CLIENT_OTHER_SKILL_READY = 0,
    SUDEKIMP_LAN_ARENA_CLIENT_OTHER_SKILL_RETIRE = 1,
    SUDEKIMP_LAN_ARENA_CLIENT_OTHER_SKILL_DRAIN = 2
} SudekiMpLanArenaClientOtherSkillDecision;

enum {
    SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_BACKOFF_MS = 100u,
    SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_MAX_ATTEMPTS = 20u
};

typedef enum SudekiMpLanArenaClientSkillRetryDecision {
    SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_ATTEMPT = 0,
    SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_WAIT = 1,
    SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_EXHAUSTED = 2,
    SUDEKIMP_LAN_ARENA_CLIENT_SKILL_RETRY_ALREADY_STARTED = 3
} SudekiMpLanArenaClientSkillRetryDecision;

typedef struct SudekiMpLanArenaClientSkillRetryGate {
    uint16_t sequence;
    uint8_t slot;
    uint8_t attempt_count;
    uint32_t last_attempt_at_ms;
    uint8_t bound;
} SudekiMpLanArenaClientSkillRetryGate;

/* A host-approved active transaction may reach this service at ApplyLatest
 * and several render boundaries in the same frame. Allocate at most one
 * native activation attempt per conservative backoff interval, and stop
 * permanently for that sequence/slot after a finite number of attempts.
 * The caller deliberately leaves its committed skill sequence unchanged on
 * WAIT or EXHAUSTED. A new sequence or slot starts a fresh bounded gate. */
void SudekiMpLanArenaClientSkillRetryReset(
    SudekiMpLanArenaClientSkillRetryGate *gate
);
SudekiMpLanArenaClientSkillRetryDecision
SudekiMpLanArenaClientSkillRetryDecide(
    SudekiMpLanArenaClientSkillRetryGate *gate,
    uint16_t incoming_sequence,
    uint8_t incoming_slot,
    uint32_t now_ms,
    int native_use_started
);

/* A locally-started asynchronous CSkill may retire only after the exact task
 * has first been observed active and is now positively observed inactive. */
int SudekiMpLanArenaClientSkillNativeLeaseMayRetire(
    int native_started,
    int native_active_seen,
    int local_state_observed,
    int local_native_active
);

/* Decides whether an incoming authenticated skill transaction may replace a
 * locally-running presentation CSkill. A missing host retirement is safe only
 * after the local CSkill has been positively observed active and then
 * inactive. An unknown observation or startup-gap inactive read is
 * deliberately equivalent to an active task. */
SudekiMpLanArenaClientSkillHandoffDecision
SudekiMpLanArenaClientSkillHandoffDecide(
    int native_started,
    int native_active_seen,
    uint16_t current_sequence,
    uint16_t incoming_sequence,
    uint8_t incoming_kind,
    int local_state_observed,
    int local_native_active
);

/* Sudeki's CSkill task/camera/speed transaction is process-global even though
 * the LAN protocol tracks Tal and Ailish independently. A new native task may
 * start only when the other actor has no retained task, or after that exact
 * task has been positively observed active and then inactive. */
SudekiMpLanArenaClientOtherSkillDecision
SudekiMpLanArenaClientOtherSkillDecide(
    int other_native_started,
    int other_native_active_seen,
    int other_state_observed,
    int other_native_active
);

/* Advances the committed host sequence only for an inactive baseline or after
 * CSkill::Use has positively returned STARTED. A failed view/resource/
 * validation/Use attempt deliberately leaves the older committed sequence in
 * place so the same still-active authenticated host transaction is retried. */
uint16_t SudekiMpLanArenaClientSkillAdvanceCommittedSequence(
    uint16_t committed_sequence,
    uint16_t incoming_sequence,
    int incoming_active,
    int native_use_started
);

/* Damage remains contained across authority loss while an activation entry,
 * retained native task, or deferred reset can still execute client gameplay. */
int SudekiMpLanArenaClientSkillDamageContainmentRequired(
    int session_authenticated,
    int activation_entry_in_flight,
    int native_task_retained,
    int reset_pending
);

/* The fixed alternate-speed value is the other half of the same native
 * transaction containment envelope. In particular, losing transport
 * authentication is not permission to restore slow time while a CSkill,
 * combat action, ranged-prime callback, or deferred reset is retained. */
int SudekiMpLanArenaClientSkillRealtimeContainmentRequired(
    int session_authenticated,
    int activation_entry_in_flight,
    int native_transaction_retained,
    int reset_pending
);

#endif
