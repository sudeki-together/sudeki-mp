#ifndef SUDEKIMP_SPIRIT_INSTANCE_ABI_H
#define SUDEKIMP_SPIRIT_INSTANCE_ABI_H
#include <windows.h>
#include <stdint.h>

/* First retained invariant failure in this exact build, or zero. Observation
 * only; never clears the fault or releases a native owner. */
unsigned int SudekiMpSpiritInstanceFaultSite(void);

/* Native objects, NOT a copy of the active singleton and never wire data.
 * Construction/destruction is allowed only at the caller's verified idle
 * game-thread boundary. The witness must exclude casts, native tasks and
 * teardown of the scene containing these objects. */
typedef BOOL (*SudekiMpSpiritInstanceIdleWitness)(void);
typedef struct SudekiMpSpiritInstance {
    void *manager;
    void *camera;
    uint32_t generation;
} SudekiMpSpiritInstance;

BOOL SudekiMpInitializeSpiritInstanceAbi(HMODULE image,
    SudekiMpSpiritInstanceIdleWitness idle_witness);
/* Fresh constructor-owned objects inherit only the original manager's current
 * strike-unlock byte. No new unlocks or active state are granted/copied. Runtime
 * progress changes while these test instances live are not synchronized here. */
BOOL SudekiMpCreateSpiritInstance(SudekiMpSpiritInstance *instance);
BOOL SudekiMpDestroySpiritInstance(SudekiMpSpiritInstance *instance);
/* Optional caster-only native input-lock policy for an owned manager. The
 * caller retains its actor/session lifetime lease through natural cleanup;
 * this witness must still recognize that retained lease after disconnect.
 * actor_type is the confirmed native resource type from that same assignment,
 * not a party index. Startup cross-checks it against the native strike ID.
 * Participant references remain intact for native effects/buffs. Only the
 * caster is locked, awaited at startup, and unlocked at completion. This does
 * not isolate shared menu/light/camera state or enable overlap admission. */
typedef BOOL (*SudekiMpSpiritCasterWitness)(void *actor,uint64_t session);
BOOL SudekiMpBindSpiritInstanceCaster(const SudekiMpSpiritInstance *instance,
    void *actor,uint8_t actor_type,uint64_t session,SudekiMpSpiritCasterWitness witness);
/* Opt-in, idle-boundary isolation of the native shared cast-busy bit. Every
 * instance must already have a distinct retained caster lease. Native writes
 * inside Enter/Leave belong to that caster; nested neutral work has its own
 * bank. Outside scopes the native bit is the union of all live banks. This
 * preserves global busy observers while permitting owner-scoped validators.
 * No caller may write the bit outside a routed scope after enabling this.
 * Does not isolate Q-menu counters/cameras or authorize a second activation. */
BOOL SudekiMpEnableSpiritInstanceCastGates(void);
/* Keep retail's ONE party SSP meter when entering private manager contexts.
 * Native code remains the only cost/reward calculator. At each LIFO boundary
 * publish the outgoing native value to the original manager and load the next
 * context with that latest value; never restore an old balance. No other cast
 * state is copied, and no SSP is granted. Must enable at idle after binding
 * all instances, before admission. Unknown/nonfinite/out-of-scope writes fail
 * closed. This is resource continuity, not concurrency admission. */
BOOL SudekiMpEnableSpiritInstanceSharedSsp(void);
BOOL SudekiMpSpiritInstanceSharedSspAbiReady(void);
/* Idle-only opt-in: constructor-owned CLightManagers per bound instance.
 * Native effect handles/stacks/rates/easing are kept intact and tick once at
 * native cadence. Scope transitions route the singleton; the world manager
 * is restored outside scopes. Private ticks never publish global render RGB.
 * Observer/retirement includes the fade tail and baseline-only native stack.
 * No overlap admission or wire changes are implied. */
BOOL SudekiMpEnableSpiritInstanceLighting(void);
/* Host-only opt-in for a retained REMOTE caster. Omit this for the local
 * caster: its native menu transitions remain unchanged. Suppress this
 * instance's balanced Spirit UI acquisition/release and script-driven caster
 * CState UI locks, not the global counter
 * or unrelated menu locks. Requires an idle bound instance and exact native
 * UI owner. Does not isolate CSkill UI, cameras, or admit overlapping casts. */
BOOL SudekiMpEnableSpiritInstanceRemoteUi(const SudekiMpSpiritInstance *instance);
/* Also contain this remote actor's ordinary CSkill UI acquisition/release.
 * Requires the coordinator's full Use and native cleanup owner routing.
 * Captures the idle actor-owned CSkill; does not isolate camera/light state
 * or authorize another cast. Never applies to an unbound/local actor. */
BOOL SudekiMpEnableSpiritInstanceRemoteSkillUi(const SudekiMpSpiritInstance *instance);
/* Prevent a remote CSkill from disabling/re-enabling the LOCAL singleton
 * controller's action input. Requires full Use/cleanup routing and a retained
 * local actor witness, separate from the caster. Both exact seven-byte native
 * mutations are suppressed; no counter/bit is force-restored. Local casts and
 * unrelated locks keep their native behavior. Does not admit overlapping casts. */
BOOL SudekiMpEnableSpiritInstanceRemoteSkillInput(const SudekiMpSpiritInstance *instance,
    void *local_actor,SudekiMpSpiritCasterWitness local_witness);
/* Optional idle-boundary routing of EnableSkillTargettingMode. A remote
 * ordinary script must not change the local controller's action bit/timer.
 * Local and neutral calls retain the complete native function. */
BOOL SudekiMpEnableSpiritInstanceSkillTargeting(void);
/* Ordinary-skill countdown routing. All calls are on the retained game thread.
 * Host timers advance once per controller update; replicas only consume the
 * admitted host phase. NONE is absent, PENDING precedes the script's setter,
 * AIMING retains its wait, RELEASED permits the script to continue. */
enum { SUDEKIMP_SKILL_TARGET_NONE, SUDEKIMP_SKILL_TARGET_PENDING,
    SUDEKIMP_SKILL_TARGET_AIMING, SUDEKIMP_SKILL_TARGET_RELEASED };
BOOL SudekiMpConfigureSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *,BOOL replica);
BOOL SudekiMpBeginSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *,uint16_t sequence);
BOOL SudekiMpApplySpiritInstanceSkillTiming(const SudekiMpSpiritInstance *,uint16_t sequence,
    uint8_t phase,uint16_t remaining_ms);
BOOL SudekiMpObserveSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *,uint16_t sequence,
    uint8_t *phase,uint16_t *remaining_ms);
BOOL SudekiMpAdvanceSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *,float seconds);
/* Ingress/admission must already be closed. Finish the last authored wait
 * naturally after disconnect, under the retained presentation-only lease. */
BOOL SudekiMpDrainSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *);
/* Rebind an admitted network generation only after this exact caster's task
 * lineage, view, light and native locks are positively idle. Other actors keep
 * their own timers. Never clears or shortens a live authored wait. */
BOOL SudekiMpRearmSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *);
/* Persistent two-player CSkill UI/input isolation, without replacing the
 * native Spirit manager/camera or enabling overlapping admission. Uses the
 * SAME native hook owner as the instance experiment; mutually exclusive with
 * constructor-owned instances. The retained witness must survive disconnect
 * until cleanup. Task witness attributes script-driven CState locks to this
 * actor's CSkill lineage (not a Spirit or unrelated script). Local casts and
 * unrelated UI locks remain native. Bind/unbind only at a drained boundary. */
BOOL SudekiMpBindRemoteCharacterSkillUi(void *local_actor,void *remote_actor,
    uint8_t remote_type,uint64_t session,SudekiMpSpiritCasterWitness retained,
    SudekiMpSpiritCasterWitness skill_task);
BOOL SudekiMpUnbindRemoteCharacterSkillUi(void);
BOOL SudekiMpRemoteCharacterSkillUiHealthy(void);
/* The client already owns the character-input vtable slot. Its existing
 * adapter must prove that exact installed lease; unknown replacements still
 * reject. The callback is retained until successful ABI reset. */
typedef BOOL (*SudekiMpSpiritInputOwnerWitness)(HMODULE image);
BOOL SudekiMpInitializeSpiritInstanceAbiWithInputOwner(HMODULE image,
    SudekiMpSpiritInstanceIdleWitness idle, SudekiMpSpiritInputOwnerWitness input_owner);
/* Optional named-camera namespace, separate from CSpiritCam above. Constructs
 * native InitCam/SkillCam equivalents (two of the ten registry slots). During
 * an owner scope only their names are exposed as InitCam/SkillCam; pointers,
 * native state machines, targets, and render selection are never copied.
 * All inlined native name lookups therefore see the same owner. NULL/empty
 * camera names still mean the native render camera, NOT the scoped caster.
 * Requires retained caster/task lifetime and idle construction/retirement.
 * Failed creation can retain partial allocations until Destroy succeeds.
 * This does NOT isolate render selection, manager targets, lighting or fades,
 * and is not permission to enable concurrent activation. Do not hold an owner
 * scope across unrelated game work; the old whole-frame probe is unsuitable. */
BOOL SudekiMpSpiritInstanceNamedCameraAbiReady(void);
/* Opt-in before creating any named pairs. Retain each constructor-owned pair
 * outside the ten-slot native name registry and publish only the current
 * strict-LIFO owner's pair through two reserved empty slots. Native objects,
 * scheduled callbacks and intrusive references retain their own identities.
 * The ordinary two-instance registered-name path remains the default. */
BOOL SudekiMpEnableSpiritInstanceNamedCameraBanking(void);
BOOL SudekiMpEnableSpiritInstanceNamedCameras(const SudekiMpSpiritInstance *instance);
/* kind: 1=InitCam, 2=SkillCam. Borrowed pointer valid only at the caller's
 * retained game-thread boundary. FALSE is unknown/unavailable, never permission
 * to fall back to a different caster's global camera. */
BOOL SudekiMpObserveSpiritInstanceNamedCamera(const SudekiMpSpiritInstance *instance,
    unsigned int kind,void **camera);
/* Route a remote caster's logical current-camera pointer without selecting
 * its camera for the local renderer or notifying local-view listeners.
 * Requires its private named pair and remote UI lease. Empty-name native
 * lookups then resolve to this private selection while its scope is active.
 * "default" ends remote presentation and selects its private InitCam, not
 * the local player's default camera. Other shared camera names are rejected.
 * Local/neutral scopes retain native selection and listener behavior.
 * Does NOT isolate lighting, TSACam/SpiritCam or enable concurrent admission. */
BOOL SudekiMpSpiritInstanceCameraSelectionAbiReady(void);
/* Route scheduled CCamera updates AND CSpiritCam resource-ready/animation
 * callbacks by their retained native receiver. Those callbacks can start the
 * camera outside a GEL task. Unrelated primary callbacks run neutral; they
 * never borrow whichever caster happened to enclose their invocation. */
BOOL SudekiMpInstallSpiritInstanceNamedCameraUpdates(void);
BOOL SudekiMpEnableSpiritInstanceRemoteCameraSelection(const SudekiMpSpiritInstance *instance);
/* Existing SetRenderCamera hook calls this; no second detour is installed.
 * 0: native passthrough; 1: remote request handled; -1: unknown/rejected.
 * On handled requests kind is 0=default, 1=InitCam, 2=SkillCam. */
int SudekiMpRouteSpiritInstanceRenderCamera(void *manager,const char *name,unsigned int *kind);
/* Observe the current native scope (including manager/camera update callbacks).
 * Success with generation zero means positively neutral, NOT remote merely
 * because another cast is active. Failure leaves the output untouched. */
BOOL SudekiMpObserveSpiritInstanceScope(SudekiMpSpiritInstance *instance);
/* Game-thread, strict-LIFO singleton routing. NULL selects the original game
 * context for unrelated nested work. Returns a nonzero cookie on success.
 * A failed leave retains its scope and blocks destruction/reset for retry.
 * This routes native objects only: it does NOT grant gameplay authority,
 * split participant locks or permit overlapping admission. The named-camera
 * namespace is routed only when explicitly enabled above. */
uint32_t SudekiMpEnterSpiritInstance(const SudekiMpSpiritInstance *instance);
BOOL SudekiMpLeaveSpiritInstance(uint32_t cookie);
/* CSpiritCam and CSoul start with scheduling disabled (period -1). Their native
 * Start/Stop paths own scheduling. Wrap existing ticks; never tick twice.
 * Unrelated instances retain the original callback and neutral context.
 * Manager ticks are routed too; its native f900 ABI consumes Update(float).
 * Installation alone never schedules a new object. */
BOOL SudekiMpInstallSpiritInstanceUpdates(void);
BOOL SudekiMpUninstallSpiritInstanceUpdates(void);
/* Schedule the initially disabled manager at native cadence, matching retail
 * game activation. Requires a fully quiescent instance and idle game thread.
 * Camera/soul Start/Stop routines retain their own scheduling. */
BOOL SudekiMpScheduleSpiritInstanceManager(const SudekiMpSpiritInstance *instance);
BOOL SudekiMpObserveSpiritInstanceManagerTicks(const SudekiMpSpiritInstance *instance,
    uint32_t *ticks);
typedef struct SudekiMpSpiritInstanceState {
    uint32_t state, strike_id;
    BOOL camera_active;
    BOOL idle; /* Includes native references, locks, and UI/busy obligations. */
    /* Same native task/camera/lock/reference proof as idle, excluding the
     * independently retained lighting fade. Never authorizes destruction. */
    BOOL body_idle;
} SudekiMpSpiritInstanceState;
/* Observes the specified constructor-owned instance, not the currently routed
 * global manager. FALSE is unknown; callers must not treat it as completion. */
BOOL SudekiMpObserveSpiritInstance(const SudekiMpSpiritInstance *instance,
    SudekiMpSpiritInstanceState *state);
/* Narrow native manager activity observation with the same thread, retained
 * caster, generation, manager and camera identity checks. Does not calculate
 * idle/body-idle: FALSE activity is never proof of task drain or safe release. */
BOOL SudekiMpObserveSpiritInstanceActivity(const SudekiMpSpiritInstance *instance,
    BOOL *active);
/* Resolve an existing constructor-owned binding using the caller's retained
 * actor/session lease. Never constructs an object or grants new authority. */
BOOL SudekiMpResolveSpiritInstanceCaster(void *actor,uint64_t session,
    SudekiMpSpiritInstance *instance);
BOOL SudekiMpObserveSpiritInstanceUpdates(const SudekiMpSpiritInstance *instance,
    uint32_t *camera_ticks, uint32_t *soul_ticks);
#if defined(SUDEKIMP_CAST_INSTANCE_PROBE)
/* Test-only: use the verified native period setter to schedule idle camera and
 * soul nodes at normal UpdateMgr cadence. No active cast or model is allowed. */
BOOL SudekiMpScheduleIdleSpiritInstanceProbe(const SudekiMpSpiritInstance *instance);
#endif
/* Refuses reset while any native instance remains; does not force cancel. */
BOOL SudekiMpResetSpiritInstanceAbi(void);

#endif
