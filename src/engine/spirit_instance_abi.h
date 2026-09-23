#ifndef SUDEKIMP_SPIRIT_INSTANCE_ABI_H
#define SUDEKIMP_SPIRIT_INSTANCE_ABI_H
#include <windows.h>
#include <stdint.h>

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
BOOL SudekiMpEnableSpiritInstanceNamedCameras(const SudekiMpSpiritInstance *instance);
/* kind: 1=InitCam, 2=SkillCam. Borrowed pointer valid only at the caller's
 * retained game-thread boundary. FALSE is unknown/unavailable, never permission
 * to fall back to a different caster's global camera. */
BOOL SudekiMpObserveSpiritInstanceNamedCamera(const SudekiMpSpiritInstance *instance,
    unsigned int kind,void **camera);
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
} SudekiMpSpiritInstanceState;
/* Observes the specified constructor-owned instance, not the currently routed
 * global manager. FALSE is unknown; callers must not treat it as completion. */
BOOL SudekiMpObserveSpiritInstance(const SudekiMpSpiritInstance *instance,
    SudekiMpSpiritInstanceState *state);
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
