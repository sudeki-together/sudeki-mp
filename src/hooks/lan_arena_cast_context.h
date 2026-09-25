#ifndef SUDEKIMP_LAN_ARENA_CAST_CONTEXT_H
#define SUDEKIMP_LAN_ARENA_CAST_CONTEXT_H

#include <windows.h>
#include <stdint.h>

/* Native-only lineage, never serialized. A cast ID belongs to one session,
 * actor and root invocation, not to the process's most recently active skill. */
typedef struct SudekiMpLanCastOwner {
    uint64_t session;
    void *actor;
    void *script_component;
    uint32_t cast_id;
    uint8_t actor_type;
    uint8_t kind; /* 1: CSkill, 2: Spirit */
} SudekiMpLanCastOwner;

typedef BOOL (*SudekiMpLanCastOwnerWitness)(void *actor, uint8_t kind,
    uint64_t *session, uint8_t *actor_type);

/* These seams supply attribution only. They do not bypass native admission,
 * cancel scripts, change speed, or grant client gameplay authority. */
BOOL SudekiMpInstallLanCastContext(HMODULE image,
    SudekiMpLanCastOwnerWitness witness);
BOOL SudekiMpUninstallLanCastContext(void);
/* Called at the verified native controller boundary. Retained task references
 * are released only after the native scheduler has cleared their thread. */
BOOL SudekiMpLanCastContextPoll(void);
/* Actor/session teardown must wait for the whole task lineage, including
 * children that outlive the actor's CSkill/root task. Never cancels a task. */
BOOL SudekiMpLanCastContextDrained(void);
/* Read-only per-actor retirement check after Poll at the game-thread boundary.
 * Other actors' live roots/children do not block this actor. Any retained cast
 * for this actor (even from an older session) or an unknown registry blocks
 * retirement. The caller must still hold its exact actor/session lease.
 * This is a lifetime observation, never authority to start a new action. */
BOOL SudekiMpLanCastContextActorDrained(void *actor,uint64_t session);
/* Valid only inside the original native script binding call on that thread;
 * a non-cast/replaced/unowned nested call never inherits another cast. */
BOOL SudekiMpLanCastContextCurrent(SudekiMpLanCastOwner *owner);
/* Retained task lineage during native execution, including disconnect drain.
 * Unlike Current this is NOT a current network-authority witness. Callers
 * must separately validate their retained native actor/session lease. */
BOOL SudekiMpLanCastContextCurrentRetained(SudekiMpLanCastOwner *owner);
/* CSkill::Use may execute the freshly constructed task before assigning its
 * handle to CSkill+74. Only during that exact, owned root submission, return
 * the currently executing constructor-pinned task. Never infer a handle from
 * the last cast or grant authority to submit another task. */
BOOL SudekiMpLanCastContextStartingSkillTask(void *actor,uint64_t session,
    void *skill,void **handle,void **thread);
/* Optional bounded diagnostic: observes native UI recomputation and its
 * caller/retained cast, without changing locks, menu admission or native flow.
 * Enable only at the drained game-thread boundary; removed with this adapter. */
BOOL SudekiMpLanCastContextEnableUiTrace(void);

/* Optional game-thread task routing, before the VM fetches an opcode. NULL
 * owner means an unrelated task and must select neutral context, not inherit
 * the enclosing caster. Enter returns a nonzero LIFO cookie; failure yields
 * without advancing the native instruction pointer. A failed leave retains
 * the scope/callbacks and blocks teardown instead of releasing live state.
 * Install/remove only while the native lineage is positively drained. */
typedef uint32_t (*SudekiMpLanCastTaskEnter)(const SudekiMpLanCastOwner *owner);
typedef BOOL (*SudekiMpLanCastTaskLeave)(uint32_t cookie);
BOOL SudekiMpLanCastContextSetTaskRouting(SudekiMpLanCastTaskEnter enter,
    SudekiMpLanCastTaskLeave leave);

#endif
