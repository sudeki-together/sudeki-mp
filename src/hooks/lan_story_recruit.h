#ifndef SUDEKIMP_LAN_STORY_RECRUIT_H
#define SUDEKIMP_LAN_STORY_RECRUIT_H

#include "hooks/lan_story_observer.h"
#include "hooks/lan_story_task_trace.h"

/* One authored, exact-build route. Network data never chooses a native
 * resource name, function, task hash, instruction pointer or entity pointer. */
#define SUDEKIMP_STORY_RECRUIT_LIGHTHOUSE_AILISH 1u
typedef struct SudekiMpLanStoryRecruitRequest {
    /* Generation and before identity are LOCAL retained observations. Host
     * after identity is an opaque transaction tag, never a local epoch. */
    uint32_t route,transaction,load_generation;
    uint32_t before_epoch,before_revision,after_epoch,after_revision;
    uint32_t actor_generation,observed_tick;
} SudekiMpLanStoryRecruitRequest;
typedef enum SudekiMpLanStoryRecruitPhase {
    SUDEKIMP_STORY_RECRUIT_IDLE=0,
    SUDEKIMP_STORY_RECRUIT_REMOVING,
    SUDEKIMP_STORY_RECRUIT_SPAWNING,
    SUDEKIMP_STORY_RECRUIT_SETUP,
    SUDEKIMP_STORY_RECRUIT_READY,
    SUDEKIMP_STORY_RECRUIT_UNKNOWN
} SudekiMpLanStoryRecruitPhase;
typedef struct SudekiMpLanStoryRecruitReport {
    SudekiMpLanStoryRecruitRequest request;
    unsigned phase;
    /* Borrowed local identities; never dereference removed_npc after its
     * destructor observation. Native retirement and registry removal differ. */
    void *world,*group,*spawn_group,*removed_npc,*added_actor;
    void *pending_actor;
    uint8_t pending_actor_pause,pending_setup_stage;
    BOOL pending_actor_exact;
    BOOL removal_entered,npc_registry_removed,npc_destructor_returned;
    BOOL spawn_entered,setup_exact;
    uint32_t accounted_created_tasks;
    BOOL task_delta_exact;
    SudekiMpLanStorySpawnObservation spawn;
    const char *reason;
} SudekiMpLanStoryRecruitReport;

/* The containment owner must recheck the unchanged paused registry/roster,
 * admitting ONLY the positively observed delta in this report. It retains
 * full world/GEL pause plus input/trigger fencing throughout. It must not
 * reset its baseline to an arbitrary current registry or release a pause.
 * Called on the native thread before/after each mutation and each Service. */
typedef BOOL (*SudekiMpLanStoryRecruitExact)(
    const SudekiMpLanStoryRecruitReport *report,void *context);
BOOL SudekiMpLanStoryRecruitInstall(HMODULE image);
BOOL SudekiMpLanStoryRecruitBegin(const SudekiMpLanStoryRecruitRequest *request,
    const SudekiMpLanStoryNativeRoster *roster,
    SudekiMpLanStoryRecruitExact exact,void *context);
BOOL SudekiMpLanStoryRecruitService(SudekiMpLanStoryRecruitReport *report);
BOOL SudekiMpLanStoryRecruitGetReport(SudekiMpLanStoryRecruitReport *report);
/* Only after the containment owner has adopted this exact accounted delta.
 * No input or camera authority is granted by this native transaction. */
BOOL SudekiMpLanStoryRecruitCommit(void);
BOOL SudekiMpLanStoryRecruitRetains(void);
/* Terminal cleanup requires the existing positive native Quit return. It
 * forgets destroyed borrowed owners without touching the old world again. */
BOOL SudekiMpLanStoryRecruitNativeExitReturned(void);
BOOL SudekiMpLanStoryRecruitUninstall(void);

#endif
