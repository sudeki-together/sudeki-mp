#ifndef SUDEKIMP_LAN_STORY_TASK_TRACE_H
#define SUDEKIMP_LAN_STORY_TASK_TRACE_H

#include <windows.h>
#include <stdint.h>

typedef struct SudekiMpLanStoryTaskTraceStatus {
    uint32_t load_generation;
    uint32_t start_task, on_load_task;
    BOOL start_terminal, start_retired;
    BOOL on_load_terminal, on_load_retired;
    BOOL unknown, log_truncated;
} SudekiMpLanStoryTaskTraceStatus;
typedef struct SudekiMpLanStoryRecruitmentStatus {
    uint32_t load_generation,task,hash,party_add_count;
    uint32_t party_add_before,party_add_after;
    void *added_actor; /* Borrowed identity only; revalidate against live roster. */
    BOOL terminal,retired,party_add_exact,unknown;
} SudekiMpLanStoryRecruitmentStatus;
typedef struct SudekiMpLanStoryResourceObservation {
    uint32_t encoded_kind,identifier;
    char text[96];
    BOOL exact,text_known;
} SudekiMpLanStoryResourceObservation;
#define SUDEKIMP_STORY_SPAWN_TASKS 16u
typedef struct SudekiMpLanStorySpawnTask {
    uint32_t id,hash,parent;
    BOOL setup_scope,terminal,retired;
} SudekiMpLanStorySpawnTask;
typedef struct SudekiMpLanStorySpawnObservation {
    uint32_t load_generation,task,request,construction,replay_transaction;
    uint32_t replay_created_before;
    SudekiMpLanStoryResourceObservation actor_resource,placement_resource;
    uint32_t group_before,group_after,created_before,created_after;
    /* Only tasks actually born inside the witnessed completion stack, plus
     * children of those tasks. These are not all PC initialization work. */
    uint32_t completion_tasks_created,completion_tasks_retired;
    uint32_t setup_created_before,setup_created_after,setup_tasks_created,setup_tasks_retired;
    uint32_t observed_task_count;
    SudekiMpLanStorySpawnTask observed_tasks[SUDEKIMP_STORY_SPAWN_TASKS];
    uint16_t world_pause;
    uint8_t actor_pause_before,actor_pause_after;
    void *actor; /* Borrowed diagnostic identity, never an input/spawn lease. */
    void *pending_actor; /* Exact retained EntitySetup resource, not ready control. */
    uint8_t pending_pause,pending_stage;
    BOOL pending_actor_exact;
    BOOL construction_exact,completion_returned,group_member_exact;
    BOOL pause_inherited,party_add_matches,unknown;
    BOOL job_destructor_returned,job_storage_released;
} SudekiMpLanStorySpawnObservation;

/* Private saved-load observation and saved-story profiles only. These passive GEL call observers
 * are mutually exclusive with lan_arena_cast_context/Talos task hooks. Every
 * native call still runs once with its original arguments and return value.
 * Recruitment diagnostics report raw copied VM arguments and the next VM PC;
 * neither grants replay/control authority or proves argument object identity.
 * The authored SpawnPC journal copies bounded resource identities and observes
 * native EntitySetup construction/completion/destruction and completion-scoped
 * task lineage. It never starts a spawn itself.
 * No task refcount, scheduling, input, actor, or authority state is changed.
 * Install only during suspended native process startup; hook/trampoline
 * publication is not a concurrent hot-install transaction. */
BOOL SudekiMpLanStoryTaskTraceInstall(HMODULE game);
BOOL SudekiMpLanStoryTaskTraceUninstall(void);
/* After the lobby bridge positively verifies native world destruction and
 * frontend entry, forget the destroyed world's borrowed diagnostics. This
 * neither cancels native work nor marks tasks terminal/retired. Hooks remain
 * installed but task creation is unobserved until the next saved-load root.
 * Requires the owner thread outside every observer scope; unknown trace
 * faults remain latched. Failed/unknown native exit cannot clear anything. */
BOOL SudekiMpLanStoryTaskTraceForgetExitedWorld(void);
/* Read on the observed native thread, outside one of the traced callbacks.
 * Terminal means native STEP returned 1; retired means the corresponding
 * native retirement returned. These two ROOT tasks finishing is not proof
 * that zone loading, descendants, cameras or recruitment are ready. */
BOOL SudekiMpLanStoryTaskTraceGetStatus(SudekiMpLanStoryTaskTraceStatus *out);
/* Only the witnessed Add-bearing New Brightwater lighthouse root4fb91e97.
 * Later overlapping/no-op roots do not replace it. Terminal and retired refer
 * to this task, never all descendants or complete story work. */
BOOL SudekiMpLanStoryTaskTraceGetRecruitmentStatus(SudekiMpLanStoryRecruitmentStatus *out);
/* Passive authored RR request/EntitySetup lifecycle evidence. Completion,
 * destructor return and the observed task lineage are separate facts. None
 * proves that all initialization work, NPC removal or recruitment is ready.
 * Resource observations are bounded copies, never retained native references. */
BOOL SudekiMpLanStoryTaskTraceGetSpawnObservation(SudekiMpLanStorySpawnObservation *out);
/* Diagnostic scope for the separate, positively fenced native recruitment
 * adapter. Does not call native spawning or grant authority. The following
 * exact RR call must receive these same local ResourceName addresses; it is
 * observed without fabricating a GEL task or instruction pointer. End must
 * run immediately after that native RR call returns. */
BOOL SudekiMpLanStoryTaskTraceBeginSpawnReplay(uint32_t transaction,
    const void *actor_resource,const void *placement_resource);
BOOL SudekiMpLanStoryTaskTraceEndSpawnReplay(uint32_t transaction);
/* Same diagnostic scope for the verified InitialSetup adapter. Every birth
 * is recorded by actual native task creation; unknown descendants or missing
 * counter coverage remain unknown, never admitted merely by this scope. */
BOOL SudekiMpLanStoryTaskTraceBeginSpawnSetup(uint32_t transaction);
BOOL SudekiMpLanStoryTaskTraceEndSpawnSetup(uint32_t transaction);
BOOL SudekiMpLanStoryTaskTraceSpawnCounter(uint32_t transaction,
    uint32_t *accounted_created_tasks,BOOL *exact);
/* Positive ownership of the already-installed exact native RR observer. */
BOOL SudekiMpLanStoryTaskTraceSpawnEntryExact(HMODULE image);

#endif
