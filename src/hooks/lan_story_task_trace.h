#ifndef SUDEKIMP_LAN_STORY_TASK_TRACE_H
#define SUDEKIMP_LAN_STORY_TASK_TRACE_H

#include <windows.h>
#include <stdint.h>
#include "hooks/lan_arena_cast_context.h"

/* Native callback sharing, never an extra patch at an already-owned seam.
 * Host-side caster only; attach/detach on the verified native thread outside
 * all task callbacks. Uninstall refuses while this consumer is registered. */
BOOL SudekiMpLanStoryTaskHostExact(HMODULE image);
/* Immutable AddPlayer callsite ownership, including pre-load installation.
 * Accepts the pristine native call only while this owner is fully uninstalled;
 * otherwise requires this exact installed observer and its retained original
 * target. No native thread, actor, world or task admission is granted. */
BOOL SudekiMpLanStoryTaskTraceAddCallImageExact(HMODULE image);
BOOL SudekiMpLanStoryTaskHostAttach(HMODULE image,SudekiMpLanCastCreatedObserver created,
    SudekiMpLanCastStepAdapter step);
BOOL SudekiMpLanStoryTaskHostDetach(SudekiMpLanCastCreatedObserver created,SudekiMpLanCastStepAdapter step);

/* Optional area-coordinator admission, NOT registered by runtime yet. This
 * shares the same pre-fetch step owner alongside cast routing. WAIT returns
 * native yield=2 before opcode fetch/argument consumption; it neither skips a
 * binding nor rewinds an instruction. RUN still passes through cast routing.
 * No world/area lifetime lease is supplied here. Explicit read-only instruction
 * inspection is available below; admission alone does not classify bindings. */
typedef enum SudekiMpStoryTaskAdmissionDecision {
    SUDEKIMP_STORY_TASK_UNKNOWN=0, SUDEKIMP_STORY_TASK_RUN=1,
    SUDEKIMP_STORY_TASK_WAIT=2
} SudekiMpStoryTaskAdmissionDecision;
typedef struct SudekiMpStoryTaskAdmissionView {
    uint32_t load_generation,task_id,function_hash,parent_id;
    void *thread; /* Borrowed only during decide; NULL for retirement notice. */
    BOOL tracked,waiting;
} SudekiMpStoryTaskAdmissionView;
/* Callbacks must only inspect/copy state and make an admission decision; they
 * must not enter native code, change the VM, attach/detach, or retain thread.
 * An untracked task is explicitly unknown provenance. RUN for such a task
 * needs independent consumer proof; WAIT/UNKNOWN cannot manufacture an id and
 * therefore quarantine the registration. Unknown also yields without fetch.
 * Native retirement of a waiting task delivers its copied identity AFTER the
 * original retirement returns; ack must discard that pending intent, not replay
 * it. FALSE retains a quarantined record and all callback dependencies. */
typedef SudekiMpStoryTaskAdmissionDecision (*SudekiMpStoryTaskDecide)(
    const void *consumer,const SudekiMpStoryTaskAdmissionView *view);
typedef BOOL (*SudekiMpStoryTaskRetired)(const void *consumer,
    const SudekiMpStoryTaskAdmissionView *view);
BOOL SudekiMpLanStoryTaskAdmissionAttach(HMODULE,const void *consumer,
    SudekiMpStoryTaskDecide,SudekiMpStoryTaskRetired);
/* Native thread outside callbacks, exact shared hooks, and no pending wait or
 * unknown state. A later RUN clears a wait only after step returns 0/1; a cast
 * router's yield=2 keeps it pending. No timeout/disconnect force-release. */
BOOL SudekiMpLanStoryTaskAdmissionDetach(HMODULE,const void *consumer,
    SudekiMpStoryTaskDecide,SudekiMpStoryTaskRetired);

enum { SUDEKIMP_STORY_INSTRUCTION_OTHER=1,SUDEKIMP_STORY_INSTRUCTION_COMPILED,
    SUDEKIMP_STORY_INSTRUCTION_NATIVE,SUDEKIMP_STORY_INSTRUCTION_METHOD,
    SUDEKIMP_STORY_INSTRUCTION_CHILD };
typedef struct SudekiMpStoryTaskInstruction {
    uint32_t offset,opcode,kind,call_hash;
    uint32_t script_offset,native_rva,argument_count;
} SudekiMpStoryTaskInstruction;
/* Optional read-only pre-fetch lookup, ONLY inside this consumer's decide
 * callback using the exact borrowed view. No new patch or native invocation.
 * Checks the current manager/program bounds and bounded native lookup tables;
 * compiled functions take precedence over global native bindings. OTHER copies
 * the opcode only, METHOD/CHILD copy their operand but do NOT resolve effects.
 * NATIVE identifies the current direct target, not its transitive consequences
 * or permission to run. No stack argument is consumed/copied, no area/file/VM
 * lifetime lease is acquired. The coordinator must retain those separately and
 * repeat lookup on resume; never replay a saved native_rva. Failed/unsupported
 * lookup leaves output untouched and must not be treated as harmless work.
 * This API does not suspend the native C caller of an immediate script. */
BOOL SudekiMpLanStoryTaskInspectInstruction(HMODULE,const void *consumer,
    const SudekiMpStoryTaskAdmissionView *,SudekiMpStoryTaskInstruction *);

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

/* Private saved-load observation and saved-story profiles only. These GEL call
 * observers own the seams otherwise used by standalone cast/Talos task hooks.
 * Cast routing shares this owner through TaskHostAttach instead of repatching.
 * Without optional routing/admission consumers, every native call still runs
 * once with its original arguments and return value. Admission can yield ONLY
 * at the verified pre-fetch step boundary described above.
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

/* Shared EntitySetup owner. Observers may only copy/validate state; they must
 * never call native code, detach, or start a spawn from these callbacks.
 * BEGIN/END pairs run on the verified game thread around the original call.
 * END's result is the native return value (only ctor/destructor use it).
 * WORLD_EXIT is emitted only after the existing verified native Quit witness.
 * UNKNOWN means the bounded nesting journal could not retain exact pairing.
 * The consumer must detach before this adapter can uninstall. */
typedef enum SudekiMpLanStoryEntitySetupPhase {
    SUDEKIMP_ENTITY_SETUP_CTOR_BEGIN,
    SUDEKIMP_ENTITY_SETUP_CTOR_END,
    SUDEKIMP_ENTITY_SETUP_COMPLETE_BEGIN,
    SUDEKIMP_ENTITY_SETUP_COMPLETE_END,
    SUDEKIMP_ENTITY_SETUP_DESTROY_BEGIN,
    SUDEKIMP_ENTITY_SETUP_DESTROY_END,
    SUDEKIMP_ENTITY_SETUP_WORLD_EXIT,
    SUDEKIMP_ENTITY_SETUP_UNKNOWN
} SudekiMpLanStoryEntitySetupPhase;
typedef struct SudekiMpLanStoryEntitySetupEvent {
    SudekiMpLanStoryEntitySetupPhase phase;
    uint32_t load_generation, flags;
    void *job;
    const void *subject; /* ctor placement or completed actor; NULL for destroy */
    void *result;
} SudekiMpLanStoryEntitySetupEvent;
typedef void (*SudekiMpLanStoryEntitySetupObserver)(const void *consumer,
    const SudekiMpLanStoryEntitySetupEvent *event);
BOOL SudekiMpLanStoryTaskTraceEntitySetupAttach(HMODULE image,const void *consumer,
    SudekiMpLanStoryEntitySetupObserver observer);
BOOL SudekiMpLanStoryTaskTraceEntitySetupDetach(HMODULE image,const void *consumer,
    SudekiMpLanStoryEntitySetupObserver observer);
/* Outside callbacks; includes shared hook ownership and both fault latches. */
BOOL SudekiMpLanStoryTaskTraceEntitySetupExact(HMODULE image);
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
