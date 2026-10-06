#ifndef SUDEKIMP_LAN_STORY_AREA_TASK_H
#define SUDEKIMP_LAN_STORY_AREA_TASK_H
#include "engine/story_area.h"
#include "hooks/control_separation.h"

enum { SUDEKIMP_AREA_JOB_ARMED=1, SUDEKIMP_AREA_JOB_SUBMITTED,
    SUDEKIMP_AREA_JOB_RUNNING, SUDEKIMP_AREA_JOB_RETURNED,
    SUDEKIMP_AREA_JOB_DESTROYING, SUDEKIMP_AREA_JOB_DESTROYED };
typedef struct SudekiMpLanStoryAreaTaskReceipt {
    uint64_t ticket;
    SudekiMpStoryAreaRef area;
    unsigned phase;
    BOOL executed, submitting;
    /* Copied at the native publication stores, never dereferenceable leases.
     * A NULL resource distinguishes the native non-zone interface result.
     * Generation is copied synchronously from continuous resource history;
     * it is zero for a non-zone result. Publication is NOT readiness, native
     * keepalive or child drain. Never resolve the generation later by address. */
    BOOL published;
    uintptr_t published_descriptor,published_interface,published_resource;
    uint64_t published_resource_generation;
} SudekiMpLanStoryAreaTaskReceipt;
/* Experimental passive observer; not installed by story runtime yet. Install
 * only during suspended startup, before native world/worker creation. It owns
 * the ZoneRequest enqueue call, resource-publication stores and that class's execute/destructor slots, not
 * the shared native destructor or StoryObserver's transition hooks.
 * Every original still runs once. No load/cancel/unpause/placement is issued. */
BOOL SudekiMpLanStoryAreaTaskInstall(HMODULE);
BOOL SudekiMpLanStoryAreaTaskUninstall(void);
/* Arm on verified game dispatch BEFORE submitting a native request. The caller
 * separately retains the native world/descriptor; the policy pin below is NOT
 * a native reference. Only an exact unloaded descriptor matching this LOADING
 * area's name is accepted. Arm neither submits work nor authorizes travel.
 * Requires the resource journal and its coordinator to be attached already.
 * Keep policy storage alive until Disarm/Release succeeds. Output unchanged on
 * refusal. Tickets never repeat, including across successful reinstall. */
BOOL SudekiMpLanStoryAreaTaskArm(HMODULE,const SudekiMpControlUpdateDispatchWitness *,
    SudekiMpStoryAreas *,SudekiMpStoryAreaRef,const void *descriptor,uint64_t *ticket);
BOOL SudekiMpLanStoryAreaTaskRead(HMODULE,uint64_t,SudekiMpLanStoryAreaTaskReceipt *);
/* Disarm only an intent whose native enqueue was never observed. Release only
 * after exact destructor RETURN and enqueue RETURN. Neither is native cancel.
 * DESTROYED is this job's lifetime only, NOT successful loading, child-task
 * drain, PVS readiness, collision readiness or playable-area proof. */
BOOL SudekiMpLanStoryAreaTaskDisarm(HMODULE,uint64_t);
BOOL SudekiMpLanStoryAreaTaskRelease(HMODULE,uint64_t);
#endif
