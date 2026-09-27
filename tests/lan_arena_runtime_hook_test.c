#include <windows.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Keep this lifecycle test deterministic.  The runtime's worker owns only
 * transport polling, which is outside the callsite contract exercised here.
 * Fake kernel handles let installation and teardown run without a live
 * thread while the production runtime implementation remains unchanged. */
static HANDLE WINAPI test_create_event(
    LPSECURITY_ATTRIBUTES attributes,
    BOOL manual_reset,
    BOOL initial_state,
    LPCWSTR name
);
static HANDLE WINAPI test_create_thread(
    LPSECURITY_ATTRIBUTES attributes,
    SIZE_T stack_size,
    LPTHREAD_START_ROUTINE start,
    LPVOID parameter,
    DWORD flags,
    LPDWORD thread_id
);
static BOOL WINAPI test_set_event(HANDLE handle);
static DWORD WINAPI test_wait_for_single_object(HANDLE handle, DWORD timeout);
static BOOL WINAPI test_close_handle(HANDLE handle);

#define CreateEventW test_create_event
#define CreateThread test_create_thread
#define SetEvent test_set_event
#define WaitForSingleObject test_wait_for_single_object
#define CloseHandle test_close_handle
#define SUDEKIMP_CAST_CAMERA_PROBE 1
#define SUDEKIMP_CAST_LIGHT_PROBE 1
#define SUDEKIMP_CAST_SKILL_OVERLAP_PROBE 1
#include "../src/hooks/lan_arena_runtime.c"

static BOOL describe_equipped_weapon;
static void *elco_weapon_fixture_owner;
static SudekiMpElcoWeaponObservation elco_weapon_fixture;
void SudekiMpLanAimSetShotObserver(SudekiMpLanWeaponShotObserver observer) { (void)observer; }
void SudekiMpLanAimSetIdleWitness(SudekiMpLanIdleWitness witness) { (void)witness; }
void SudekiMpLanArenaClientReplicaNativeWeaponFired(void *actor) { (void)actor; }
BOOL SudekiMpObserveElcoWeapon(void *actor, SudekiMpElcoWeaponObservation *out) {
    if (!actor || actor!=elco_weapon_fixture_owner || !out) return FALSE;
    *out=elco_weapon_fixture; return TRUE;
}
BOOL SudekiMpElcoWeaponReady(const SudekiMpElcoWeaponObservation *state) {
    (void)state; return FALSE;
}
BOOL SudekiMpLanAimInstall(HMODULE image,SudekiMpLanAimWitness witness,
    SudekiMpLanAimTargetWitness target,SudekiMpLanAimFireWitness fire) {
    return image && witness && target && fire;
}
BOOL SudekiMpLanAimCameraTarget(float d[3],float t[3]) { (void)d;(void)t;return FALSE; }
BOOL SudekiMpLanAimTargetNearActor(const float p[3],const float d[3],const float t[3]) {
    float n=sqrtf(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]),distance=0;
    if(!isfinite(n) || n<.5f || n>1.5f) return FALSE;
    for(unsigned i=0;i<3;++i) { float x=t[i]-100*d[i]/n-p[i]; distance+=x*x; }
    return isfinite(distance) && distance<=25;
}
BOOL SudekiMpLanArenaClientReplicaRangedTarget(void *a,float t[3]) { (void)a;(void)t;return FALSE; }
BOOL SudekiMpLanAimUninstall(void) { return TRUE; }
void SudekiMpLanAimActors(void *a,void *b) { (void)a; (void)b; }
BOOL SudekiMpLanAimNormalize(const float in[3],float out[3]) {
    float n=sqrtf(in[0]*in[0]+in[1]*in[1]+in[2]*in[2]);
    if(!isfinite(n) || n<0.5f || n>1.5f) return FALSE;
    for(unsigned i=0;i<3;++i) out[i]=in[i]/n;
    return TRUE;
}
BOOL SudekiMpLanArenaClientReplicaRangedAim(void *a,float d[3]) { (void)a;(void)d;return FALSE; }
void *SudekiMpControlSeparationSeatCharacter(unsigned seat) { (void)seat;return NULL; }
BOOL SudekiMpControlSeparationSeatInputLeaseActive(unsigned seat) { (void)seat;return FALSE; }
static void *ranged_remote_lease;
static BOOL ranged_aim_fixture;
BOOL SudekiMpControlSeparationLanArenaRemoteActorExact(void *actor) {
    return actor && actor == ranged_remote_lease;
}
static BOOL cast_context_drained=TRUE;
static void *cast_context_actor_busy;
static BOOL cast_context_actor_known=TRUE;
BOOL SudekiMpLanCastContextActorDrained(void *actor,uint64_t session) {
    return actor && session && cast_context_actor_known && actor!=cast_context_actor_busy;
}
static BOOL cast_context_current;
static SudekiMpLanCastOwner cast_context_owner;
BOOL SudekiMpInstallLanCastContext(HMODULE image, SudekiMpLanCastOwnerWitness witness) {
    return image!=NULL && witness!=NULL;
}
BOOL SudekiMpUninstallLanCastContext(void) { return TRUE; }
BOOL SudekiMpLanCastContextPoll(void) { return TRUE; }
BOOL SudekiMpLanCastContextDrained(void) {
    if (!cast_context_drained) SetLastError(ERROR_BUSY);
    return cast_context_drained;
}
BOOL SudekiMpLanCastContextCurrent(SudekiMpLanCastOwner *owner) {
    if (!cast_context_current || !owner) return FALSE;
    *owner=cast_context_owner;
    return TRUE;
}
BOOL SudekiMpLanCastContextCurrentRetained(SudekiMpLanCastOwner *owner) {
    return SudekiMpLanCastContextCurrent(owner);
}
static BOOL ui_abi_reset_result=TRUE,ui_abi_bind_result=TRUE,ui_abi_healthy=TRUE;
static unsigned int ui_abi_init_calls,ui_abi_bind_calls,ui_abi_reset_calls;
static void *ui_abi_local,*ui_abi_remote;
static uint64_t ui_abi_session;
static uint8_t ui_abi_type;
BOOL SudekiMpInitializeSpiritInstanceAbi(HMODULE image,SudekiMpSpiritInstanceIdleWitness idle) {
    if(!image || !idle || !idle()) return FALSE;
    ++ui_abi_init_calls; return TRUE;
}
BOOL SudekiMpInstallSpiritInstanceUpdates(void) { return TRUE; }
BOOL SudekiMpLanArenaClientCharacterInputOwnerExact(HMODULE image) { return image!=NULL; }
BOOL SudekiMpInitializeSpiritInstanceAbiWithInputOwner(HMODULE image,
    SudekiMpSpiritInstanceIdleWitness idle,SudekiMpSpiritInputOwnerWitness input_owner) {
    return input_owner && input_owner(image) && SudekiMpInitializeSpiritInstanceAbi(image,idle);
}
BOOL SudekiMpBindRemoteCharacterSkillUi(void *local,void *remote,uint8_t type,uint64_t session,
    SudekiMpSpiritCasterWitness retained,SudekiMpSpiritCasterWitness task) {
    if(!retained || !task || !retained(local,session) || !retained(remote,session)) return FALSE;
    ++ui_abi_bind_calls; ui_abi_local=local; ui_abi_remote=remote;
    ui_abi_session=session; ui_abi_type=type;
    return ui_abi_bind_result;
}
BOOL SudekiMpRemoteCharacterSkillUiHealthy(void) { return ui_abi_healthy; }
BOOL SudekiMpResetSpiritInstanceAbi(void) { ++ui_abi_reset_calls; return ui_abi_reset_result; }
static SudekiMpSpiritInstance fake_spirit_instances[2];
static SudekiMpSpiritInstanceState fake_spirit_states[2];
static unsigned int fake_spirit_next_generation;
static SudekiMpSpiritRoutingEnter fake_spirit_enter;
static SudekiMpSkillRoutingEnter fake_owned_skill_enter;
static SudekiMpLanCastTaskEnter fake_owned_task_enter;
static SudekiMpSpiritPresentationObserver fake_spirit_observer;
static BOOL fake_owned_menu;
static uint32_t fake_scope_cookie;
static unsigned int fake_destroy_calls;
static BOOL fake_client_replay_active;
static BOOL fake_client_replay_complete;
static unsigned int fake_client_drain_calls;
static void *fake_replay_actor;
static int fake_replay_slot;
BOOL SudekiMpLanArenaClientReplicaAnySkillReplayActive(void) { return fake_client_replay_active; }
BOOL SudekiMpLanArenaClientReplicaDrainSkillReplay(void) {
    ++fake_client_drain_calls;
    if(fake_client_replay_complete) fake_client_replay_active=FALSE;
    return !fake_client_replay_active;
}
BOOL SudekiMpLanArenaClientSkillReplayAdmission(void *actor,int slot,uint64_t session) {
    return actor && actor==fake_replay_actor && slot==fake_replay_slot && session==ui_abi_session;
}
static SudekiMpSpiritInstance fake_camera_scope;
static BOOL fake_camera_scope_known=TRUE;
static int fake_camera_route_result=1;
static unsigned int fake_camera_route_kind=2;
BOOL SudekiMpEnableSpiritInstanceNamedCameras(const SudekiMpSpiritInstance *instance) { return instance!=NULL; }
BOOL SudekiMpInstallSpiritInstanceNamedCameraUpdates(void) { return TRUE; }
BOOL SudekiMpEnableSpiritInstanceRemoteCameraSelection(const SudekiMpSpiritInstance *instance) {
    return instance && instance->generation==fake_spirit_instances[1].generation;
}
BOOL SudekiMpObserveSpiritInstanceNamedCamera(const SudekiMpSpiritInstance *instance,
    unsigned int kind,void **camera) {
    (void)instance; (void)kind; (void)camera; return FALSE;
}
BOOL SudekiMpObserveSpiritInstanceScope(SudekiMpSpiritInstance *scope) {
    if(!scope || !fake_camera_scope_known) return FALSE;
    *scope=fake_camera_scope; return TRUE;
}
int SudekiMpRouteSpiritInstanceRenderCamera(void *manager,const char *name,unsigned int *kind) {
    (void)manager; (void)name;
    if(fake_camera_route_result==1) *kind=fake_camera_route_kind;
    return fake_camera_route_result;
}
BOOL SudekiMpCreateSpiritInstance(SudekiMpSpiritInstance *instance) {
    if(!ui_abi_bind_result) return FALSE;
    for(unsigned int i=0;i<2;++i) if(!fake_spirit_instances[i].generation) {
        *instance=(SudekiMpSpiritInstance){&fake_spirit_instances[i],&fake_spirit_states[i],++fake_spirit_next_generation};
        fake_spirit_instances[i]=*instance;
        fake_spirit_states[i]=(SudekiMpSpiritInstanceState){0,0,FALSE,TRUE};
        return TRUE;
    }
    return FALSE;
}
BOOL SudekiMpBindSpiritInstanceCaster(const SudekiMpSpiritInstance *instance,void *actor,
    uint8_t type,uint64_t session,SudekiMpSpiritCasterWitness witness) {
    if(!instance || !witness || !witness(actor,session)) return FALSE;
    if(instance->generation==fake_spirit_instances[0].generation) ui_abi_local=actor;
    else { ui_abi_remote=actor; ui_abi_type=type; }
    ui_abi_session=session; return TRUE;
}
BOOL SudekiMpEnableSpiritInstanceRemoteUi(const SudekiMpSpiritInstance *instance) {
    ++ui_abi_bind_calls; return instance!=NULL;
}
BOOL SudekiMpEnableSpiritInstanceRemoteSkillUi(const SudekiMpSpiritInstance *instance) { return instance!=NULL; }
BOOL SudekiMpEnableSpiritInstanceRemoteSkillInput(const SudekiMpSpiritInstance *instance,
    void *actor,SudekiMpSpiritCasterWitness witness) { return instance && witness(actor,ui_abi_session); }
BOOL SudekiMpEnableSpiritInstanceCastGates(void) { return TRUE; }
BOOL SudekiMpEnableSpiritInstanceSharedSsp(void) { return TRUE; }
BOOL SudekiMpEnableSpiritInstanceLighting(void) { return TRUE; }
BOOL SudekiMpEnableSpiritInstanceSkillTargeting(void) { return TRUE; }
BOOL SudekiMpConfigureSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *i,BOOL r) { (void)r; return i!=NULL; }
static BOOL fake_timing_begin_result=TRUE;
static unsigned int fake_timing_begin_calls;
static SudekiMpSpiritInstance fake_timing_instance;
static uint16_t fake_timing_sequence;
BOOL SudekiMpBeginSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *i,uint16_t s) {
    ++fake_timing_begin_calls;
    if(i) fake_timing_instance=*i;
    fake_timing_sequence=s;
    return i && s && fake_timing_begin_result;
}
BOOL SudekiMpApplySpiritInstanceSkillTiming(const SudekiMpSpiritInstance *i,uint16_t s,uint8_t p,uint16_t ms) {
    (void)ms; return i && s && p; }
BOOL SudekiMpObserveSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *i,uint16_t s,uint8_t *p,uint16_t *ms) {
    if(!i || !s) return FALSE; *p=3; *ms=0; return TRUE; }
BOOL SudekiMpAdvanceSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *i,float dt) { (void)dt; return i!=NULL; }
BOOL SudekiMpDrainSpiritInstanceSkillTiming(const SudekiMpSpiritInstance *i) { return i!=NULL; }
BOOL SudekiMpReadCastLight(uint32_t key,float current[3],float baseline[3]) {
    if(!key) return FALSE;
    for(unsigned int i=0;i<3;++i) { current[i]=.6f; baseline[i]=1.f; }
    return TRUE;
}
BOOL SudekiMpScheduleSpiritInstanceManager(const SudekiMpSpiritInstance *instance) { return instance!=NULL; }
BOOL SudekiMpObserveSpiritInstance(const SudekiMpSpiritInstance *instance,SudekiMpSpiritInstanceState *state) {
    if(!ui_abi_healthy || !instance || !state) return FALSE;
    for(unsigned int i=0;i<2;++i) if(instance->generation &&
        instance->generation==fake_spirit_instances[i].generation) { *state=fake_spirit_states[i]; return TRUE; }
    return FALSE;
}
BOOL SudekiMpResolveSpiritInstanceCaster(void *actor,uint64_t session,SudekiMpSpiritInstance *instance) {
    if(session!=ui_abi_session || !instance) return FALSE;
    if(actor==ui_abi_local) *instance=fake_spirit_instances[0];
    else if(actor==ui_abi_remote) *instance=fake_spirit_instances[1];
    else return FALSE;
    return instance->generation!=0;
}
BOOL SudekiMpDestroySpiritInstance(SudekiMpSpiritInstance *instance) {
    for(unsigned int i=0;i<2;++i) if(instance->generation &&
        instance->generation==fake_spirit_instances[i].generation) {
        if(!fake_spirit_states[i].idle) return FALSE;
        memset(&fake_spirit_instances[i],0,sizeof(*instance));
        memset(instance,0,sizeof(*instance)); ++fake_destroy_calls; return TRUE;
    }
    return FALSE;
}
uint32_t SudekiMpEnterSpiritInstance(const SudekiMpSpiritInstance *instance) {
    (void)instance; return ++fake_scope_cookie;
}
BOOL SudekiMpLeaveSpiritInstance(uint32_t cookie) { return cookie!=0; }
BOOL SudekiMpQuickSkillSpiritRoutingReady(void) { return fake_owned_menu; }
BOOL SudekiMpInstallQuickSkillInputTrace(HMODULE image,BOOL a,BOOL b) {
    if(!image || a || b || fake_owned_menu) return FALSE;
    fake_owned_menu=TRUE; return TRUE;
}
BOOL SudekiMpUninstallQuickSkillInputTrace(void) { fake_owned_menu=FALSE; return TRUE; }
BOOL SudekiMpSetSpiritActivationRouting(SudekiMpSpiritRoutingIdleWitness idle,
    SudekiMpSpiritRoutingEnter enter,SudekiMpSpiritRoutingLeave leave) {
    if(enter && (!idle || !leave || !idle() || fake_spirit_enter)) return FALSE;
    fake_spirit_enter=enter; return TRUE;
}
BOOL SudekiMpSetSkillActivationRouting(SudekiMpSkillRoutingIdleWitness idle,
    SudekiMpSkillRoutingEnter enter,SudekiMpSkillRoutingLeave leave) {
    if(enter && (!idle || !leave || !idle() || fake_owned_skill_enter)) return FALSE;
    fake_owned_skill_enter=enter; return TRUE;
}
BOOL SudekiMpSpiritActivationRoutingHealthy(void) { return TRUE; }
BOOL SudekiMpSkillActivationRoutingHealthy(void) { return TRUE; }
BOOL SudekiMpLanCastContextSetTaskRouting(SudekiMpLanCastTaskEnter enter,SudekiMpLanCastTaskLeave leave) {
    if(!cast_context_drained || (enter && (!leave || fake_owned_task_enter))) return FALSE;
    fake_owned_task_enter=enter; return TRUE;
}
BOOL SudekiMpCleanroomEngineSetSpiritPresentationObserver(SudekiMpSpiritPresentationObserver observer,
    SudekiMpSpiritPresentationIdle idle) {
    if(observer && (!idle || !idle() || fake_spirit_observer)) return FALSE;
    fake_spirit_observer=observer; return TRUE;
}
BOOL SudekiMpInstallLanArenaSkillFade(HMODULE image, SudekiMpLanArenaSkillFadeWitness witness) {
    return image!=NULL && witness!=NULL;
}
BOOL SudekiMpInstallLanArenaSkillFadeWithDrawView(HMODULE image,
    SudekiMpLanArenaSkillFadeWitness witness,
    SudekiMpLanArenaDrawViewBoundary begin, SudekiMpLanArenaDrawViewBoundary end) {
    BOOL client=runtime_config.local_role==SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    return image && witness &&
        begin==(client ? begin_client_private_spirit_draw:NULL) &&
        end==(client ? SudekiMpLanArenaClientSpiritViewEndFrame:NULL);
}
BOOL SudekiMpUninstallLanArenaSkillFade(void) { return TRUE; }
BOOL SudekiMpLanArenaReadSkillLight(float current[3],float baseline[3]) {
    (void)current; (void)baseline; return FALSE;
}
BOOL SudekiMpLanArenaClientReplicaGetSkillFade(SudekiMpLanArenaSkillFade *fade) {
    (void)fade; return FALSE;
}
BOOL SudekiMpDescribeCharacterWeapons(void *character,
    SudekiMpWeaponQuickList *weapons) {
    (void)character;
    if (!describe_equipped_weapon || weapons == NULL) return FALSE;
    memset(weapons, 0, sizeof(*weapons));
    weapons->row_count = 1u;
    weapons->rows[0].equipped = TRUE;
    return TRUE;
}
BOOL SudekiMpEnsureCharacterStarterWeapon(void *character) {
    return character != NULL;
}
BOOL SudekiMpGrantTestroomCharacterWeapons(void *character, const char *command) {
    return character != NULL && command != NULL;
}
BOOL SudekiMpServiceRemoteRapidWeapon(void *character, void *local_character,
    float delta, uint32_t *repeat_ms) {
    (void)character; (void)local_character; (void)delta;
    if (repeat_ms != NULL) *repeat_ms = 0u;
    return FALSE;
}
#undef CloseHandle
#undef WaitForSingleObject
#undef SetEvent
#undef CreateThread
#undef CreateEventW

enum {
    TEST_IMAGE_SIZE = 0x002d0000u,
    TEST_RVA_RENDER_START = 0x001dce30u,
    TEST_RVA_RENDER_START_CALL = 0x0028d443u,
    TEST_RVA_RENDER_PRE_WORLD_CALL = 0x0028d539u,
    TEST_RVA_FRAME_END = 0x001dd540u,
    TEST_RVA_FRAME_END_CALL = 0x0028d58cu,
    TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA = 0x00036fb0u,
    TEST_RVA_GAME_SPEED_SET_MODE = 0x00207560u,
    TEST_RVA_FIXED_ALTERNATE_SPEED = 0x002c4018u
};

static int failures;
static BOOL session_start_result = TRUE;
static BOOL campaign_guard_install_result = TRUE;
static BOOL campaign_guard_uninstall_result = TRUE;
static BOOL collision_debug_install_result = TRUE;
static BOOL spirit_audio_install_result = TRUE;
static BOOL spirit_audio_uninstall_result = TRUE;
static BOOL spirit_audio_installed;
static BOOL spirit_audio_physically_installed;
static BOOL spirit_visual_install_result = TRUE;
static BOOL spirit_visual_reset_result = TRUE;
static BOOL spirit_visual_capture_result = TRUE;
static BOOL spirit_visual_capture_observed;
static BOOL spirit_visual_install_leaves_lease;
static BOOL spirit_visual_installed;
static BOOL host_input_install_result = TRUE;
static BOOL client_input_install_result = TRUE;
static BOOL client_replica_initialize_result = TRUE;
static BOOL client_replica_reset_result = TRUE;
static BOOL replica_stub_initialized;
static BOOL initialize_party_actor_result;
static BOOL update_witness_exact = TRUE;
static BOOL client_tal_release_ready_result = TRUE;
static BOOL observer_gate_enable_result = TRUE;
static BOOL observer_register_result = TRUE;
static BOOL replica_apply_result = TRUE;
static BOOL visible_publish_result = TRUE;
static BOOL spirit_presentation_state_result = TRUE;
static BOOL character_skill_observe_result = TRUE;
static BOOL ranged_combat_prime_pending;
static BOOL ranged_combat_prime_result = TRUE;
static BOOL cleanroom_combat_enabled;
static BOOL cleanroom_pause_active;
static BOOL cleanroom_menu_active;
static BOOL host_spirit_request_available;
static unsigned int host_spirit_request_variant;
static BOOL host_spirit_options_result = TRUE;
static BOOL host_spirit_option_available = TRUE;
static BOOL host_spirit_activation_started = TRUE;
static BOOL host_tal_controller_lease_exact = TRUE;
static BOOL session_status_result;
static SudekiMpLanArenaSessionStatus session_status;
static BOOL player_two_active;
static void *player_two_character;
static BOOL player_two_request_result = TRUE;
static BOOL player_two_release_result = TRUE;
static BOOL actor_position_result;
static BOOL actor_facing_result;
static BOOL actor_resources_result;
static BOOL snapshot_send_result;
static BOOL actor_present_results[SUDEKIMP_CLEANROOM_ACTOR_COUNT];
static BOOL spawn_actor_result;
static BOOL remove_actor_result = TRUE;
static BOOL remove_actor_clears_presence = TRUE;
static BOOL host_spirit_activation_probe_teardown;
static BOOL host_spirit_activation_probe_saw_busy;
static LONG host_spirit_activation_probe_depth;
static BOOL host_spirit_reproof_probe_teardown;
static BOOL host_spirit_reproof_probe_saw_busy;
static LONG host_spirit_reproof_probe_depth;
static BOOL host_spirit_describe_probe_teardown;
static BOOL host_spirit_describe_probe_saw_busy;
static LONG host_spirit_describe_probe_depth;
static int spirit_presentation_state;
static int fixture_spirit_id;
static SudekiMpCharacterSkillState character_skill_observation;
static void *paired_skill_actors[2];
static SudekiMpCharacterSkillState paired_skill_observations[2];
static void *cleanroom_actor_entities[SUDEKIMP_CLEANROOM_ACTOR_COUNT];
static BOOL player_two_skill_isolation_enabled;
static unsigned int player_two_skill_isolation_call_count;
static unsigned int ranged_combat_prime_call_count;
static unsigned int maintain_resources_call_count;
static unsigned int host_spirit_request_take_count;
static unsigned int host_spirit_request_discard_count;
static unsigned int host_spirit_activation_call_count;
static unsigned int host_spirit_last_activated_variant;
static SudekiMpLanArenaHostNativeSkillStartObserver
    host_native_skill_start_observer;
static unsigned int session_start_count;
static unsigned int session_stop_count;
static unsigned int replica_initialize_count;
static unsigned int client_input_uninstall_count;
static unsigned int host_input_uninstall_count;
static unsigned int replica_reset_count;
static unsigned int replica_apply_count;
static unsigned int client_tal_lease_publish_count;
static unsigned int client_tal_release_ready_count;
static void *client_tal_published_actor;
static uint32_t client_tal_published_generation;
static unsigned int request_player_two_count;
static unsigned int release_player_two_count;
static unsigned int spawn_actor_count;
static unsigned int remove_actor_count;
static unsigned int initialize_party_actor_count;
static char client_tal_teardown_events[8];
static size_t client_tal_teardown_event_count;
static unsigned int campaign_guard_uninstall_count;
static unsigned int collision_debug_uninstall_count;
static unsigned int spirit_audio_install_count;
static unsigned int spirit_audio_uninstall_count;
static unsigned int spirit_audio_physical_patch_count;
static unsigned int spirit_visual_install_count;
static unsigned int spirit_visual_reset_count;
static unsigned int spirit_visual_capture_count;
static unsigned int snapshot_send_count;
static SudekiMpLanArenaSnapshot last_sent_snapshot;
static SudekiMpLanArenaSpiritVisualHostWitness spirit_visual_witness;
static void *spirit_visual_witness_context;
static char spirit_observer_teardown_events[16];
static size_t spirit_observer_teardown_event_count;
static SudekiMpLanArenaSpiritActiveWitness spirit_audio_witness;
static void *spirit_audio_witness_context;
static SudekiMpLanArenaSpiritAudioEvent spirit_audio_events[
    SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY];
static size_t spirit_audio_event_count;
static char callback_events[32];
static size_t callback_event_count;
static unsigned int log_format_call_count;

static void check(BOOL condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s (error=%lu)\n", message,
            (unsigned long)GetLastError());
        ++failures;
    }
}

static void record_callback_event(char event) {
    if (callback_event_count + 1u < sizeof(callback_events)) {
        callback_events[callback_event_count++] = event;
        callback_events[callback_event_count] = '\0';
    }
}

static void record_client_tal_teardown_event(char event) {
    if (client_tal_teardown_event_count + 1u <
            sizeof(client_tal_teardown_events)) {
        client_tal_teardown_events[client_tal_teardown_event_count++] = event;
        client_tal_teardown_events[client_tal_teardown_event_count] = '\0';
    }
}

static void record_spirit_observer_teardown(char event) {
    if (spirit_observer_teardown_event_count + 1u <
            sizeof(spirit_observer_teardown_events)) {
        spirit_observer_teardown_events[spirit_observer_teardown_event_count++] = event;
        spirit_observer_teardown_events[spirit_observer_teardown_event_count] = '\0';
    }
}

static void reset_stub_policy(void) {
    ui_abi_reset_result = ui_abi_bind_result = ui_abi_healthy = TRUE;
    session_start_result = TRUE;
    campaign_guard_install_result = TRUE;
    campaign_guard_uninstall_result = TRUE;
    collision_debug_install_result = TRUE;
    spirit_audio_install_result = TRUE;
    spirit_audio_uninstall_result = TRUE;
    spirit_visual_install_result = TRUE;
    spirit_visual_reset_result = TRUE;
    spirit_visual_capture_result = TRUE;
    spirit_visual_capture_observed = FALSE;
    spirit_visual_install_leaves_lease = FALSE;
    host_input_install_result = TRUE;
    client_input_install_result = TRUE;
    client_replica_initialize_result = TRUE;
    client_replica_reset_result = TRUE;
    replica_stub_initialized = FALSE;
    initialize_party_actor_result = FALSE;
    update_witness_exact = TRUE;
    client_tal_release_ready_result = TRUE;
    observer_gate_enable_result = TRUE;
    observer_register_result = TRUE;
    replica_apply_result = TRUE;
    visible_publish_result = TRUE;
    spirit_presentation_state_result = TRUE;
    character_skill_observe_result = TRUE;
    ranged_combat_prime_pending = FALSE;
    ranged_combat_prime_result = TRUE;
    cleanroom_combat_enabled = FALSE;
    cleanroom_pause_active = FALSE;
    cleanroom_menu_active = FALSE;
    host_spirit_request_available = FALSE;
    host_spirit_request_variant = 0u;
    host_spirit_options_result = TRUE;
    host_spirit_option_available = TRUE;
    host_spirit_activation_started = TRUE;
    host_tal_controller_lease_exact = TRUE;
    session_status_result = FALSE;
    memset(&session_status, 0, sizeof(session_status));
    player_two_active = FALSE;
    player_two_character = NULL;
    player_two_request_result = TRUE;
    player_two_release_result = TRUE;
    actor_position_result = FALSE;
    actor_facing_result = FALSE;
    actor_resources_result = FALSE;
    snapshot_send_result = FALSE;
    memset(actor_present_results, 0, sizeof(actor_present_results));
    spawn_actor_result = FALSE;
    remove_actor_result = TRUE;
    remove_actor_clears_presence = TRUE;
    host_spirit_activation_probe_teardown = FALSE;
    host_spirit_activation_probe_saw_busy = FALSE;
    host_spirit_activation_probe_depth = 0;
    host_spirit_reproof_probe_teardown = FALSE;
    host_spirit_reproof_probe_saw_busy = FALSE;
    host_spirit_reproof_probe_depth = 0;
    host_spirit_describe_probe_teardown = FALSE;
    host_spirit_describe_probe_saw_busy = FALSE;
    host_spirit_describe_probe_depth = 0;
    spirit_presentation_state = 0;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    memset(cleanroom_actor_entities, 0, sizeof(cleanroom_actor_entities));
    player_two_skill_isolation_enabled = FALSE;
    host_native_skill_start_observer = NULL;
}

static void reset_stub_counts(void) {
    ui_abi_init_calls = ui_abi_bind_calls = ui_abi_reset_calls = 0u;
    ui_abi_local = ui_abi_remote = NULL;
    ui_abi_session = 0u;
    ui_abi_type = 0u;
    session_start_count = 0u;
    session_stop_count = 0u;
    replica_initialize_count = 0u;
    client_input_uninstall_count = 0u;
    host_input_uninstall_count = 0u;
    replica_reset_count = 0u;
    replica_apply_count = 0u;
    client_tal_lease_publish_count = 0u;
    client_tal_release_ready_count = 0u;
    client_tal_published_actor = NULL;
    client_tal_published_generation = 0u;
    request_player_two_count = 0u;
    release_player_two_count = 0u;
    spawn_actor_count = 0u;
    remove_actor_count = 0u;
    initialize_party_actor_count = 0u;
    client_tal_teardown_event_count = 0u;
    client_tal_teardown_events[0] = '\0';
    campaign_guard_uninstall_count = 0u;
    collision_debug_uninstall_count = 0u;
    spirit_audio_install_count = 0u;
    spirit_audio_uninstall_count = 0u;
    spirit_visual_install_count = 0u;
    spirit_visual_reset_count = 0u;
    spirit_visual_capture_count = 0u;
    snapshot_send_count = 0u;
    memset(&last_sent_snapshot, 0, sizeof(last_sent_snapshot));
    spirit_observer_teardown_event_count = 0u;
    spirit_observer_teardown_events[0] = '\0';
    player_two_skill_isolation_call_count = 0u;
    ranged_combat_prime_call_count = 0u;
    maintain_resources_call_count = 0u;
    host_spirit_request_take_count = 0u;
    host_spirit_request_discard_count = 0u;
    host_spirit_activation_call_count = 0u;
    host_spirit_last_activated_variant = 0u;
    spirit_audio_event_count = 0u;
    memset(spirit_audio_events, 0, sizeof(spirit_audio_events));
    log_format_call_count = 0u;
}

static void write_call(
    uint8_t *image,
    uint32_t call_rva,
    uint32_t target_rva
) {
    int32_t displacement = (int32_t)(
        (image + target_rva) - (image + call_rva + 5u));
    image[call_rva] = 0xe8u;
    memcpy(image + call_rva + 1u, &displacement, sizeof(displacement));
}

static uint8_t *call_target(uint8_t *image, uint32_t call_rva) {
    int32_t displacement;
    memcpy(&displacement, image + call_rva + 1u, sizeof(displacement));
    return image + call_rva + 5u + displacement;
}

static void prepare_native_calls(uint8_t *image) {
    static const uint8_t camera_entry[] = {
        0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u
    };
    static const uint8_t speed_entry[] = {
        0x8bu, 0x44u, 0x24u, 0x04u, 0x89u, 0x41u, 0x24u
    };
    static const uint8_t native_scale[] = {
        0x29u, 0x5cu, 0x8fu, 0x3du
    };
    write_call(
        image, TEST_RVA_RENDER_START_CALL, TEST_RVA_RENDER_START);
    write_call(
        image, TEST_RVA_RENDER_PRE_WORLD_CALL, TEST_RVA_RENDER_START);
    write_call(image, TEST_RVA_FRAME_END_CALL, TEST_RVA_FRAME_END);
    memcpy(image + TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
        camera_entry, sizeof(camera_entry));
    memcpy(image + TEST_RVA_GAME_SPEED_SET_MODE,
        speed_entry, sizeof(speed_entry));
    memcpy(image + TEST_RVA_FIXED_ALTERNATE_SPEED,
        native_scale, sizeof(native_scale));
}

static SudekiMpLanArenaSessionConfig make_config(
    SudekiMpLanArenaRole role
) {
    SudekiMpLanArenaSessionConfig config;
    memset(&config, 0, sizeof(config));
    config.local_role = role;
    config.local_simulation_node_role =
        role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ?
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD :
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    config.port = SUDEKIMP_LAN_ARENA_DEFAULT_PORT;
    config.timeout_ms = 1000u;
    config.host_actor_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    config.client_actor_type = SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    return config;
}

static void fixture_render_start(void) {
    record_callback_event('R');
}

static void fixture_frame_end(void) {
    record_callback_event('F');
}

static void verify_client_install_and_uninstall(uint8_t *image) {
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH);
    prepare_native_calls(image);
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "client runtime installs over exact native calls");
    check(SudekiMpLanArenaRuntimeInstalled(),
        "client runtime reports installed");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_render_start_entry,
        "client redirects first RenderStart call");
    check(call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_render_pre_world_entry,
        "client redirects pre-world RenderStart call");
    check(call_target(image, TEST_RVA_FRAME_END_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_frame_end_entry,
        "client redirects frame-end call");
    check(spirit_audio_install_count == 0u && !spirit_audio_installed,
        "client profile never installs the host-only Spirit audio trace");
    check(spirit_visual_install_count == 0u && !spirit_visual_installed,
        "client profile never installs the host-only Spirit visual observer");

    SudekiMpUninstallLanArenaRuntime();
    check(!SudekiMpLanArenaRuntimeInstalled(),
        "client runtime reports uninstalled");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            image + TEST_RVA_RENDER_START,
        "client uninstall restores first RenderStart call");
    check(call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            image + TEST_RVA_RENDER_START,
        "client uninstall restores pre-world RenderStart call");
    check(call_target(image, TEST_RVA_FRAME_END_CALL) ==
            image + TEST_RVA_FRAME_END,
        "client uninstall restores frame-end call");
}

static void verify_host_hook_scope(uint8_t *image) {
    uint8_t original_camera[sizeof(expected_camera_manager_set_render_camera_entry)];
    uint8_t original_speed[sizeof(expected_game_speed_set_mode_entry)];
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL);
    prepare_native_calls(image);
    memcpy(original_camera,
        image + TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
        sizeof(original_camera));
    memcpy(original_speed,
        image + TEST_RVA_GAME_SPEED_SET_MODE,
        sizeof(original_speed));
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "host runtime installs over exact native calls");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_render_start_entry,
        "host redirects first RenderStart for safe owner-basis capture");
    check(call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_render_pre_world_entry,
        "host redirects pre-world RenderStart call");
    check(call_target(image, TEST_RVA_FRAME_END_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_frame_end_entry,
        "host redirects frame-end call");
    check(memcmp(image + TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
            original_camera, sizeof(original_camera)) != 0 &&
          memcmp(image + TEST_RVA_GAME_SPEED_SET_MODE,
            original_speed, sizeof(original_speed)) != 0,
        "host redirects remote-skill camera and realtime speed seams");
    check(host_native_skill_start_observer ==
            host_native_tal_skill_started,
        "host runtime registers the exact native Tal skill-start observer");
    check(spirit_audio_install_count == 1u && spirit_audio_installed &&
          spirit_audio_physical_patch_count == 1u &&
          spirit_audio_witness == host_spirit_audio_active_witness &&
          spirit_audio_witness_context == &runtime_config,
        "host runtime installs the exact Spirit-active PlayCue witness");
    check(spirit_visual_install_count == 1u && spirit_visual_installed &&
          spirit_visual_witness == host_spirit_visual_active_witness &&
          spirit_visual_witness_context == &runtime_config,
        "host runtime installs the exact Spirit visual sequence witness");
    tal_initialized = TRUE;
    spirit_presentation_state = 4;
    {
        int native_state = 0;
        uint64_t session=0;
        uint16_t sequence=0;
        uint8_t owner=0;
        check(spirit_audio_witness != NULL &&
              !spirit_audio_witness(spirit_audio_witness_context,
                  &native_state,&session,&sequence,&owner),
            "active native state alone cannot authorize audio without a connected caster session");
    }
    spirit_presentation_state = 0;
    tal_initialized = FALSE;
    SudekiMpUninstallLanArenaRuntime();
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            image + TEST_RVA_RENDER_START,
        "host uninstall restores first RenderStart call");
    check(call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            image + TEST_RVA_RENDER_START,
        "host uninstall restores pre-world RenderStart call");
    check(call_target(image, TEST_RVA_FRAME_END_CALL) ==
            image + TEST_RVA_FRAME_END,
        "host uninstall restores frame-end call");
    check(memcmp(image + TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
            original_camera, sizeof(original_camera)) == 0 &&
          memcmp(image + TEST_RVA_GAME_SPEED_SET_MODE,
            original_speed, sizeof(original_speed)) == 0,
        "host uninstall restores remote-skill camera and speed seams");
    check(host_native_skill_start_observer == NULL,
        "host input teardown releases the native skill-start observer");
    check(!spirit_audio_installed && spirit_audio_uninstall_count == 1u,
        "host uninstall restores the Spirit PlayCue trace exactly once");
    check(!spirit_visual_installed,
        "host uninstall releases the Spirit visual observer");
}

static void verify_host_spirit_audio_rollback(uint8_t *image) {
    uint8_t original_camera[
        sizeof(expected_camera_manager_set_render_camera_entry)];
    uint8_t original_speed[sizeof(expected_game_speed_set_mode_entry)];
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL);

    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    memcpy(original_camera,
        image + TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
        sizeof(original_camera));
    memcpy(original_speed,
        image + TEST_RVA_GAME_SPEED_SET_MODE,
        sizeof(original_speed));
    spirit_audio_install_result = FALSE;
    SetLastError(ERROR_SUCCESS);
    check(!SudekiMpInstallLanArenaRuntime((HMODULE)image, &config) &&
          GetLastError() == ERROR_INVALID_DATA,
        "Spirit audio preflight failure rejects the host runtime");
    check(!SudekiMpLanArenaRuntimeInstalled() &&
          spirit_audio_install_count == 1u &&
          spirit_audio_uninstall_count == 0u &&
          !spirit_audio_installed,
        "failed Spirit audio install leaves no false hook ownership");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            image + TEST_RVA_RENDER_START &&
          call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            image + TEST_RVA_RENDER_START &&
          call_target(image, TEST_RVA_FRAME_END_CALL) ==
            image + TEST_RVA_FRAME_END &&
          memcmp(image + TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
            original_camera, sizeof(original_camera)) == 0 &&
          memcmp(image + TEST_RVA_GAME_SPEED_SET_MODE,
            original_speed, sizeof(original_speed)) == 0,
        "Spirit audio preflight failure restores earlier host hooks");

    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    host_input_install_result = FALSE;
    check(!SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "host input failure rolls the earlier Spirit trace back");
    check(spirit_audio_install_count == 1u &&
          spirit_audio_uninstall_count == 1u &&
          !spirit_audio_installed &&
          host_input_uninstall_count == 1u,
        "downstream host failure releases input then Spirit trace");
    check(memcmp(image + TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
            original_camera, sizeof(original_camera)) == 0 &&
          memcmp(image + TEST_RVA_GAME_SPEED_SET_MODE,
            original_speed, sizeof(original_speed)) == 0,
        "downstream host failure restores skill isolation after Spirit trace");

    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "host runtime reinstalls before Spirit restore retry test");
    check(SudekiMpUninstallLanArenaRuntime(),
        "host runtime logically unbinds the pinned Spirit trace");
    check(!SudekiMpLanArenaRuntimeInstalled() && !spirit_audio_installed &&
          spirit_audio_uninstall_count == 1u &&
          spirit_audio_physical_patch_count == 1u &&
          memcmp(image + TEST_RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
            original_camera, sizeof(original_camera)) == 0 &&
          memcmp(image + TEST_RVA_GAME_SPEED_SET_MODE,
            original_speed, sizeof(original_speed)) == 0,
        "logical Spirit unbind releases every ordinary host hook");

    prepare_native_calls(image);
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "later host runtime rebinds the process-lifetime Spirit trace");
    check(spirit_audio_installed &&
          spirit_audio_physical_patch_count == 1u,
        "host rebind does not install a second physical PlayCue patch");
    check(SudekiMpUninstallLanArenaRuntime(),
        "rebound host runtime unbinds cleanly");
    reset_stub_policy();
}

static void verify_host_spirit_visual_rollback(uint8_t *image) {
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL);
    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    spirit_visual_install_result = FALSE;
    check(!SudekiMpInstallLanArenaRuntime((HMODULE)image, &config) &&
          GetLastError() == ERROR_BAD_FORMAT,
        "visual observer preflight failure preserves its install error");
    check(spirit_audio_install_count == 1u &&
          spirit_audio_uninstall_count == 1u &&
          spirit_visual_install_count == 1u && spirit_visual_reset_count == 1u &&
          !spirit_audio_installed && !spirit_visual_installed,
        "failed visual install resets partial visual state and unbinds prior audio");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
              image + TEST_RVA_RENDER_START &&
          call_target(image, TEST_RVA_FRAME_END_CALL) ==
              image + TEST_RVA_FRAME_END,
        "visual preflight rollback restores earlier runtime callsites");

    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    spirit_visual_install_result = FALSE;
    spirit_visual_install_leaves_lease = TRUE;
    spirit_visual_reset_result = FALSE;
    check(!SudekiMpInstallLanArenaRuntime((HMODULE)image, &config) &&
          GetLastError() == ERROR_BUSY,
        "visual partial-install reset failure supersedes the install error");
    check(spirit_visual_installed && !spirit_audio_installed &&
          spirit_audio_uninstall_count == 1u &&
          strcmp(spirit_observer_teardown_events, "VA") == 0 &&
          original_render_start != NULL && original_frame_end != NULL &&
          call_target(image, TEST_RVA_RENDER_START_CALL) !=
              image + TEST_RVA_RENDER_START,
        "visual rollback failure still attempts audio and retains live dependencies");
    spirit_visual_reset_result = TRUE;
    spirit_visual_install_result = TRUE;
    check(SudekiMpUninstallLanArenaRuntime() && !spirit_visual_installed,
        "public uninstall retries retained partial visual installation");

    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "host installs before independent observer restoration failures");
    spirit_visual_reset_result = FALSE;
    spirit_audio_uninstall_result = FALSE;
    check(!SudekiMpUninstallLanArenaRuntime() && GetLastError() == ERROR_BUSY,
        "combined observer teardown preserves the first visual reset failure");
    check(spirit_visual_reset_count == 1u && spirit_audio_uninstall_count == 1u &&
          spirit_visual_installed && spirit_audio_installed &&
          strcmp(spirit_observer_teardown_events, "VA") == 0 &&
          SudekiMpLanArenaRuntimeInstalled() && original_render_start != NULL,
        "both independent observer restorations run before runtime quarantine");
    spirit_visual_reset_result = TRUE;
    check(!SudekiMpUninstallLanArenaRuntime() &&
          GetLastError() == ERROR_WRITE_FAULT &&
          !spirit_visual_installed && spirit_audio_installed,
        "retry releases visual ownership but preserves failed audio ownership");
    spirit_audio_uninstall_result = TRUE;
    check(SudekiMpUninstallLanArenaRuntime() &&
          !spirit_visual_installed && !spirit_audio_installed &&
          !SudekiMpLanArenaRuntimeInstalled(),
        "final observer retry completes runtime teardown");
    reset_stub_policy();
}

static void verify_host_visual_witness_and_capture(uint8_t *image) {
    static uint8_t tal_character;
    static uint8_t ailish_character;
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL);
    uint64_t token = 0u;
    uint16_t skill = 0u;
    uint32_t tick = 0u;
    uint8_t owner_type = 0u;
    SudekiMpLanArenaSpiritVfxSnapshot empty[SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY];
    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "host installs before visual witness and optional capture checks");
    session_status_result = TRUE;
    session_status.peer_connected = TRUE;
    session_status.local_role = SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL;
    session_status.local_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
    session_status.peer_simulation_node_role = SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    session_status.session_token = 456u;
    tal_initialized = TRUE;
    ailish_initialized = TRUE;
    spirit_presentation_state = 4;
    host_actor_skill_sequence[0] = UINT16_MAX;
    host_spirit_previous_active = FALSE;
    check(spirit_visual_witness(spirit_visual_witness_context, NULL, &token, &skill, &tick, &owner_type) &&
          token == 456u && skill == 1u && host_actor_skill_sequence[0] == UINT16_MAX,
        "visual witness predicts the next nonzero Spirit sequence without mutation");
    host_actor_skill_sequence[0] = 7u;
    host_spirit_previous_active = TRUE;
    check(spirit_visual_witness(spirit_visual_witness_context, NULL, &token, &skill, &tick, &owner_type) &&
          skill == 7u,
        "visual witness preserves the current observed Spirit sequence");
    session_status.peer_connected = FALSE;
    check(!spirit_visual_witness(spirit_visual_witness_context, NULL, &token, &skill, &tick, &owner_type),
        "visual witness rejects disconnected authority");
    session_status.peer_connected = TRUE;
    spirit_presentation_state = 0;
    check(!spirit_visual_witness(spirit_visual_witness_context, NULL, &token, &skill, &tick, &owner_type),
        "visual witness rejects an inactive native Spirit");
    tal_initialized = FALSE;
    ailish_initialized = FALSE;
    reset_host_skill_tracking();
    actor_position_result = actor_facing_result = actor_resources_result = TRUE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = &ailish_character;
    snapshot_send_result = TRUE;
    spirit_visual_capture_result = FALSE;
    host_last_snapshot_at_ms = 0u;
    host_publish_snapshot(1000u);
    memset(empty, 0, sizeof(empty));
    check(spirit_visual_capture_count == 1u && snapshot_send_count == 1u &&
          last_sent_snapshot.spirit_vfx_observed == 0u &&
          last_sent_snapshot.spirit_vfx_count == 0u &&
          memcmp(last_sent_snapshot.spirit_vfx, empty, sizeof(empty)) == 0,
        "failed partially written visual sample becomes UNKNOWN without blocking gameplay");
    spirit_visual_capture_result = TRUE;
    spirit_visual_capture_observed = TRUE;
    host_publish_snapshot(1050u);
    check(spirit_visual_capture_count == 2u && snapshot_send_count == 2u &&
          last_sent_snapshot.spirit_vfx_observed == 1u &&
          last_sent_snapshot.spirit_vfx_count == 0u,
        "complete empty visual roster reaches the canonical snapshot as positive removal");
    check(SudekiMpUninstallLanArenaRuntime(),
        "visual witness/capture fixture tears down cleanly");
    reset_stub_policy();
}

static void verify_second_render_mismatch_rollback(uint8_t *image) {
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH);
    prepare_native_calls(image);
    write_call(
        image,
        TEST_RVA_RENDER_PRE_WORLD_CALL,
        TEST_RVA_RENDER_START + 1u);
    reset_stub_counts();
    SetLastError(ERROR_SUCCESS);
    check(!SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "client rejects mismatched second RenderStart call");
    check(!SudekiMpLanArenaRuntimeInstalled(),
        "mismatched second RenderStart leaves runtime uninstalled");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            image + TEST_RVA_RENDER_START,
        "second RenderStart mismatch rolls first render hook back");
    check(call_target(image, TEST_RVA_FRAME_END_CALL) ==
            image + TEST_RVA_FRAME_END,
        "second RenderStart mismatch rolls frame-end hook back");
    check(call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            image + TEST_RVA_RENDER_START + 1u,
        "mismatch rollback preserves foreign second-call bytes");
    check(session_stop_count == 1u &&
            campaign_guard_uninstall_count == 1u &&
            collision_debug_uninstall_count == 1u,
        "second RenderStart mismatch rolls downstream ownership back");

    prepare_native_calls(image);
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "client reinstall succeeds after second-call mismatch rollback");
    SudekiMpUninstallLanArenaRuntime();
}

static void verify_downstream_failure_rollback(uint8_t *image) {
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH);
    prepare_native_calls(image);
    reset_stub_counts();
    client_replica_initialize_result = FALSE;
    SetLastError(ERROR_NOT_READY);
    check(!SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "client replica init failure rejects runtime install");
    check(!SudekiMpLanArenaRuntimeInstalled(),
        "downstream failure leaves runtime uninstalled");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            image + TEST_RVA_RENDER_START &&
          call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            image + TEST_RVA_RENDER_START &&
          call_target(image, TEST_RVA_FRAME_END_CALL) ==
            image + TEST_RVA_FRAME_END,
        "downstream failure restores all three native calls");
    check(client_input_uninstall_count == 1u &&
            host_input_uninstall_count == 1u &&
            replica_reset_count == 1u &&
            session_stop_count == 1u,
        "downstream failure releases input replica and session state");

    client_replica_initialize_result = TRUE;
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "client reinstall succeeds after downstream rollback");
    SudekiMpUninstallLanArenaRuntime();
}

static void verify_campaign_guard_teardown_containment(uint8_t *image) {
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH);

    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "client runtime installs before campaign teardown containment");
    campaign_guard_uninstall_result = FALSE;
    SetLastError(ERROR_SUCCESS);
    check(!SudekiMpUninstallLanArenaRuntime() &&
          GetLastError() == ERROR_BUSY,
        "campaign restore failure rejects runtime uninstall");
    check(SudekiMpLanArenaRuntimeInstalled() &&
          runtime_game_module == (HMODULE)image &&
          runtime_config.local_role ==
              SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
          campaign_guard_uninstall_count == 1u,
        "campaign restore failure retains runtime base config and ownership");

    campaign_guard_uninstall_result = TRUE;
    check(SudekiMpUninstallLanArenaRuntime(),
        "campaign restore retry completes runtime uninstall");
    check(!SudekiMpLanArenaRuntimeInstalled() &&
          runtime_game_module == NULL &&
          runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_INVALID &&
          campaign_guard_uninstall_count == 2u,
        "successful campaign retry clears retained runtime state once safe");

    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    collision_debug_install_result = FALSE;
    campaign_guard_uninstall_result = FALSE;
    SetLastError(ERROR_SUCCESS);
    check(!SudekiMpInstallLanArenaRuntime((HMODULE)image, &config) &&
          GetLastError() == ERROR_BUSY,
        "campaign rollback failure supersedes downstream install error");
    check(!SudekiMpLanArenaRuntimeInstalled() &&
          runtime_game_module == (HMODULE)image &&
          runtime_config.local_role ==
              SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
          campaign_guard_uninstall_count == 1u &&
          session_stop_count == 0u,
        "failed install rollback retains runtime state and live session lease");

    campaign_guard_uninstall_result = TRUE;
    collision_debug_install_result = TRUE;
    check(SudekiMpUninstallLanArenaRuntime(),
        "public runtime uninstall retries partial-install campaign rollback");
    check(runtime_game_module == NULL &&
          runtime_config.local_role == SUDEKIMP_LAN_ARENA_ROLE_INVALID &&
          campaign_guard_uninstall_count == 2u,
        "partial-install rollback retry clears retained runtime state");
    reset_stub_policy();
}

static void verify_client_busy_reset_containment(uint8_t *image) {
    SudekiMpLanArenaSessionConfig config = make_config(
        SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH);
    unsigned int start_count;
    unsigned int initialize_count;

    prepare_native_calls(image);
    reset_stub_policy();
    reset_stub_counts();
    check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
        "client runtime installs before BUSY reset containment checks");
    start_count = session_start_count;
    initialize_count = replica_initialize_count;
    client_remote_tal_owned = TRUE;
    client_tal_spawn_attempted = TRUE;
    client_replica_reset_result = FALSE;

    SetLastError(ERROR_SUCCESS);
    check(!SudekiMpLanArenaRuntimeEndSession() &&
          GetLastError() == ERROR_BUSY,
        "End Session reports BUSY while the client CSkill drain is pending");
    check(SudekiMpLanArenaRuntimeInstalled() && client_remote_tal_owned &&
          release_player_two_count == 0u &&
          replica_initialize_count == initialize_count &&
          session_start_count == start_count,
        "BUSY End Session retains Tal and cannot reinitialize the session");

    SetLastError(ERROR_SUCCESS);
    check(!SudekiMpLanArenaRuntimeJoinEndpoint("127.0.0.1:26770") &&
          GetLastError() == ERROR_BUSY,
        "Join reports BUSY while the prior client CSkill drain is pending");
    check(SudekiMpLanArenaRuntimeInstalled() && client_remote_tal_owned &&
          release_player_two_count == 0u &&
          replica_initialize_count == initialize_count &&
          session_start_count == start_count,
        "BUSY Join retains Tal and admits no replica or session reinit");

    SetLastError(ERROR_SUCCESS);
    SudekiMpUninstallLanArenaRuntime();
    check(GetLastError() == ERROR_BUSY &&
          SudekiMpLanArenaRuntimeInstalled() && client_remote_tal_owned &&
          release_player_two_count == 0u &&
          client_input_uninstall_count == 0u &&
          host_input_uninstall_count == 0u,
        "BUSY uninstall retains Tal, input adapters, and runtime ownership");
    check(call_target(image, TEST_RVA_RENDER_START_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_render_start_entry &&
          call_target(image, TEST_RVA_RENDER_PRE_WORLD_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_render_pre_world_entry &&
          call_target(image, TEST_RVA_FRAME_END_CALL) ==
            (uint8_t *)(uintptr_t)&lan_arena_frame_end_entry,
        "BUSY uninstall leaves all client frame callbacks installed");

    client_replica_reset_result = TRUE;
    SudekiMpUninstallLanArenaRuntime();
    check(!SudekiMpLanArenaRuntimeInstalled() &&
          !client_remote_tal_owned && release_player_two_count == 1u,
        "confirmed drain permits exactly one Tal release and final uninstall");
    check(replica_initialize_count == initialize_count &&
          session_start_count == start_count,
        "final uninstall still performs no client session reinitialization");
}

static void verify_client_delayed_connection_recovery(uint8_t *image) {
    static uint8_t host_actor, local_actor;
    SudekiMpControlUpdateDispatchWitness witness = {0};
    for (unsigned int pairing = 0; pairing < 2; ++pairing) {
        SudekiMpLanArenaSessionConfig config = make_config(
            SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH);
        unsigned int initialized, applied;
        SudekiMpCleanroomActor host_kind, client_kind;
        if (pairing) {
            config.host_actor_type = SUDEKIMP_LAN_ARENA_BUKI_TYPE;
            config.client_actor_type = SUDEKIMP_LAN_ARENA_ELCO_TYPE;
        }
        SudekiMpLanArenaSetSeatTypes(config.host_actor_type, config.client_actor_type);
        prepare_native_calls(image);
        reset_stub_policy(); reset_stub_counts();
        check(SudekiMpInstallLanArenaRuntime((HMODULE)image, &config),
            "delayed client installs replica before a host exists");
        host_kind = seat_host_actor(); client_kind = seat_client_actor();
        initialized = replica_initialize_count;
        lan_arena_control_update_observer(NULL, NULL, &witness);
        lan_arena_control_update_observer(NULL, NULL, &witness);
        check(!replica_stub_initialized && client_replica_recovery_pending &&
              replica_initialize_count == initialized && replica_apply_count == 0u,
            "waiting teardown is remembered without applying or repeatedly initializing");

        session_status_result = TRUE;
        session_status.peer_connected = TRUE;
        session_status.local_role = config.local_role;
        session_status.local_simulation_node_role = config.local_simulation_node_role;
        session_status.peer_simulation_node_role =
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
        session_status.session_token = 123u;
        update_witness_exact = FALSE;
        lan_arena_control_update_observer(NULL, NULL, &witness);
        check(replica_initialize_count == initialized && !replica_stub_initialized,
            "transport connection cannot reinitialize on an inexact native witness");
        update_witness_exact = TRUE;
        client_replica_initialize_result = FALSE;
        lan_arena_control_update_observer(NULL, NULL, &witness);
        check(client_replica_recovery_pending && !replica_stub_initialized &&
              request_player_two_count == 0u && replica_apply_count == 0u,
            "failed exact reinitialization admits neither actor takeover nor playback");
        cleanroom_actor_entities[client_kind] = &local_actor;
        actor_present_results[client_kind] = TRUE;
        actor_position_result = TRUE;
        spawn_actor_result = TRUE;
        client_replica_initialize_result = TRUE;
        lan_arena_control_update_observer(NULL, NULL, &witness);
        check(!client_replica_recovery_pending && replica_stub_initialized &&
              replica_initialize_count == initialized + 2u,
            "late host connection retries and restores the torn-down replica");
        check(client_tal_spawn_attempted && spawn_actor_count == 1u,
            "recovery requests the remote actor before its asynchronous publication");

        cleanroom_actor_entities[host_kind] = &host_actor;
        cleanroom_actor_entities[client_kind] = &local_actor;
        actor_present_results[host_kind] = TRUE;
        actor_present_results[client_kind] = TRUE;
        player_two_active = TRUE; player_two_character = &host_actor;
        initialize_party_actor_result = TRUE;
        lan_arena_control_update_observer(NULL, NULL, &witness);
        check(replica_apply_count == 1u && client_replica_frame_ready &&
              client_remote_tal_owned && client_tal_published_generation != 0u,
            "recovered replica claims fresh actor generation then applies snapshots");
        initialized = replica_initialize_count;
        client_replica_frame_attempted = FALSE;
        lan_arena_control_update_observer(NULL, NULL, &witness);
        check(replica_initialize_count == initialized && replica_apply_count == 2u,
            "healthy frames never reinstall an already-live replica");

        applied = replica_apply_count;
        session_status.peer_connected = FALSE;
        client_replica_reset_result = FALSE;
        lan_arena_control_update_observer(NULL, NULL, &witness);
        check(client_replica_recovery_pending && client_remote_tal_owned &&
              remove_actor_count == 0u && replica_apply_count == applied,
            "disconnect with BUSY drain retains actor and stops playback");
        session_status.peer_connected = TRUE;
        session_status.session_token = 124u;
        lan_arena_control_update_observer(NULL, NULL, &witness);
        check(replica_initialize_count == initialized && client_remote_tal_owned &&
              replica_apply_count == applied && remove_actor_count == 0u,
            "reconnected transport cannot bypass a BUSY native drain");

        client_replica_reset_result = TRUE;
        remove_actor_clears_presence = FALSE;
        lan_arena_control_update_observer(NULL, NULL, &witness);
        check(client_replica_recovery_pending && client_remote_tal_remove_pending &&
              replica_initialize_count == initialized && remove_actor_count == 1u,
            "recovery waits for asynchronous native actor removal before reinstall");
        lan_arena_control_update_observer(NULL, NULL, &witness);
        check(replica_initialize_count == initialized && remove_actor_count == 1u,
            "pending actor removal neither reinstalls nor enqueues duplicate removals");
        actor_present_results[host_kind] = FALSE;
        cleanroom_actor_entities[host_kind] = NULL;
        lan_arena_control_update_observer(NULL, NULL, &witness);
        check(!client_replica_recovery_pending && replica_stub_initialized &&
              replica_initialize_count == initialized + 1u,
            "positive actor disappearance permits one clean replica reinstall");
        cleanroom_actor_entities[host_kind] = &host_actor;
        actor_present_results[host_kind] = TRUE;
        remove_actor_clears_presence = TRUE;
        client_replica_frame_attempted = FALSE;
        lan_arena_control_update_observer(NULL, NULL, &witness);
        check(replica_apply_count == applied + 1u && client_replica_frame_ready,
            "reconnect resumes playback with rebuilt actor ownership");
        check(SudekiMpUninstallLanArenaRuntime() && !replica_stub_initialized,
            "recovered client still tears down cleanly");
    }
    SudekiMpLanArenaSetSeatTypes(SUDEKIMP_LAN_ARENA_TAL_TYPE,
        SUDEKIMP_LAN_ARENA_AILISH_TYPE);
    reset_stub_policy(); reset_stub_counts();
}

static void verify_callback_order(void) {
    callback_event_count = 0u;
    callback_events[0] = '\0';
    runtime_config.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    tal_initialized = TRUE;
    ailish_initialized = TRUE;
    original_render_start = fixture_render_start;
    replica_apply_result = TRUE;
    visible_publish_result = TRUE;
    client_replica_frame_attempted = FALSE;
    client_replica_frame_ready = FALSE;
    prepare_client_replica_frame();
    prepare_client_replica_frame();
    check(strcmp(callback_events, "A") == 0,
        "post-controller prepares exactly one replica sample per render frame");
    lan_arena_render_start_entry();
    check(strcmp(callback_events, "APXRVSCP") == 0,
        "first render orders visible Spirit VFX before native render and remote reassert");

    callback_event_count = 0u; callback_events[0] = '\0';
    runtime_skill_ui_initialized=runtime_spirit_named_cameras=TRUE;
    runtime_spirit_skill_routed=runtime_spirit_tasks_routed=TRUE;
    prepare_client_replica_frame();
    lan_arena_render_start_entry();
    check(strcmp(callback_events,"APXBRVSCP")==0,
        "private Spirit body preparation precedes native animation preparation without moving the camera draw boundary");
    callback_event_count = 0u; callback_events[0] = '\0';
    visible_publish_result=FALSE;
    prepare_client_replica_frame(); lan_arena_render_start_entry();
    check(strcmp(callback_events,"APRVSCP")==0,
        "failed private publication cannot acquire a body visibility lease");
    visible_publish_result=TRUE;
    runtime_skill_ui_initialized=runtime_spirit_named_cameras=FALSE;
    runtime_spirit_skill_routed=runtime_spirit_tasks_routed=FALSE;

    callback_event_count = 0u;
    callback_events[0] = '\0';
    lan_arena_render_pre_world_entry();
    check(strcmp(callback_events, "RSCP") == 0,
        "pre-world reasserts first-render basis without second refresh");

    callback_event_count = 0u;
    callback_events[0] = '\0';
    visible_publish_result = FALSE;
    prepare_client_replica_frame();
    lan_arena_render_start_entry();
    check(strcmp(callback_events, "APRVSCP") == 0,
        "failed visible publication skips Spirit VFX admission");
    visible_publish_result = TRUE;

    callback_event_count = 0u;
    callback_events[0] = '\0';
    replica_apply_result = FALSE;
    prepare_client_replica_frame();
    lan_arena_render_start_entry();
    check(strcmp(callback_events, "ARVC") == 0,
        "unapplied replica frame skips publication and Spirit VFX admission");
    replica_apply_result = TRUE;

    callback_event_count = 0u;
    callback_events[0] = '\0';
    lan_arena_render_start_entry();
    check(strcmp(callback_events, "RVC") == 0,
        "render without a fresh controller sample never resamples or reuses a consumed frame");

    callback_event_count = 0u;
    callback_events[0] = '\0';
    runtime_config.local_role = SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL;
    original_frame_end = fixture_frame_end;
    lan_arena_frame_end_entry();
    check(strcmp(callback_events, "FH") == 0,
        "frame-end callback preserves native-before-service ordering");

    original_render_start = NULL;
    original_frame_end = NULL;
    tal_initialized = FALSE;
    ailish_initialized = FALSE;
}

static void verify_client_tal_lifecycle_generation(void) {
    static uint8_t tal_character;
    uint32_t first_generation;
    uint32_t rearmed_generation;
    unsigned int publish_count;

    client_remote_tal_owned = FALSE;
    client_tal_spawn_attempted = FALSE;
    client_remote_tal_request_owned = FALSE;
    client_remote_tal_remove_pending = FALSE;
    client_remote_tal_generation_counter = 0u;
    client_remote_tal_active_generation = 0u;
    client_remote_tal_generation_actor = NULL;
    client_tal_release_ready_result = TRUE;
    client_tal_lease_publish_count = 0u;
    client_tal_published_actor = NULL;
    client_tal_published_generation = 0u;
    tal_initialized = TRUE;

    client_tal_spawn_attempted = TRUE;
    check(!client_tal_missing_actor_requires_release(),
        "pending asynchronous Tal spawn is not misclassified as actor loss");
    client_remote_tal_owned = TRUE;
    check(client_tal_missing_actor_requires_release(),
        "owned missing Tal requires lifecycle release");
    client_remote_tal_owned = FALSE;
    client_tal_spawn_attempted = FALSE;

    check(claim_client_remote_tal_generation(&tal_character) &&
          client_remote_tal_active_generation != 0u &&
          client_tal_published_actor == &tal_character &&
          client_tal_published_generation ==
              client_remote_tal_active_generation &&
          !tal_initialized,
        "Tal claim publishes a nonzero runtime lifecycle generation");
    first_generation = client_remote_tal_active_generation;
    publish_count = client_tal_lease_publish_count;
    tal_initialized = TRUE;
    check(claim_client_remote_tal_generation(&tal_character) &&
          client_remote_tal_active_generation == first_generation &&
          client_tal_lease_publish_count == publish_count &&
          tal_initialized,
        "repeated exact Tal claim preserves one lifecycle generation");

    invalidate_client_remote_tal_generation("test_loss");
    check(client_remote_tal_active_generation == 0u &&
          client_remote_tal_generation_actor == NULL &&
          client_tal_published_actor == NULL &&
          client_tal_published_generation == 0u &&
          !tal_initialized,
        "Tal loss invalidates presentation before address reuse");
    check(claim_client_remote_tal_generation(&tal_character) &&
          client_remote_tal_active_generation != first_generation,
        "same-address Tal replacement receives a fresh generation");

    invalidate_client_remote_tal_generation("test_wrap");
    client_remote_tal_generation_counter = UINT32_MAX;
    check(claim_client_remote_tal_generation(&tal_character) &&
          client_remote_tal_active_generation == 1u,
        "Tal lifecycle generation skips zero on wrap");

    first_generation = client_remote_tal_active_generation;
    client_remote_tal_owned = TRUE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    player_two_release_result = FALSE;
    check(!release_client_remote_tal("test_native_release_retry") &&
          client_remote_tal_owned &&
          client_remote_tal_active_generation == 0u &&
          client_tal_published_actor == NULL,
        "failed native Tal release keeps ownership but revokes presentation");
    check(claim_client_remote_tal_generation(&tal_character) &&
          client_remote_tal_active_generation != first_generation &&
          client_tal_published_actor == &tal_character,
        "an exact retained PlayerTwo claim rearms a revoked Tal generation");
    rearmed_generation = client_remote_tal_active_generation;

    tal_initialized = TRUE;
    client_tal_release_ready_result = FALSE;
    SetLastError(ERROR_SUCCESS);
    check(!release_client_remote_tal("test_busy") &&
          GetLastError() == ERROR_BUSY &&
          client_remote_tal_active_generation == rearmed_generation &&
          tal_initialized,
        "busy native presentation defers Tal lifecycle invalidation");
    client_tal_release_ready_result = TRUE;
    player_two_release_result = TRUE;
    check(release_client_remote_tal("test_release") &&
          client_remote_tal_active_generation == 0u &&
          !tal_initialized,
        "confirmed Tal release invalidates generation and initialization");

    tal_initialized = TRUE;
    check(release_client_remote_tal("host_role") && tal_initialized,
        "no-client Tal release leaves host initialization unchanged");
}

static void verify_client_tal_pending_request_teardown(void) {
    static uint8_t tal_character;
    static uint8_t ailish_character;
    SudekiMpControlUpdateDispatchWitness witness;

    reset_stub_policy();
    reset_stub_counts();
    memset(&witness, 0, sizeof(witness));
    runtime_config.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    session_status_result = TRUE;
    session_status.peer_connected = TRUE;
    session_status.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    session_status.local_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    session_status.peer_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] =
        &ailish_character;
    actor_position_result = TRUE;
    spawn_actor_result = TRUE;
    client_remote_tal_owned = FALSE;
    client_remote_tal_request_owned = FALSE;
    client_remote_tal_remove_pending = FALSE;
    client_tal_spawn_attempted = FALSE;
    client_remote_tal_generation_actor = NULL;
    client_remote_tal_active_generation = 0u;
    client_release_pending_logged = FALSE;

    lan_arena_control_update_observer(NULL, NULL, &witness);
    check(client_tal_spawn_attempted && spawn_actor_count == 1u &&
          cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] == NULL &&
          !client_remote_tal_request_owned,
        "accepted asynchronous Tal spawn remains pending before publication");

    SetLastError(ERROR_SUCCESS);
    check(!release_client_remote_tal("test_spawn_publication_pending") &&
          GetLastError() == ERROR_BUSY && client_tal_spawn_attempted &&
          spawn_actor_count == 1u && release_player_two_count == 0u &&
          remove_actor_count == 0u,
        "unpublished Tal spawn blocks release without clearing its request");

    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    actor_present_results[SUDEKIMP_CLEANROOM_TAL] = TRUE;
    check(release_client_remote_tal("test_spawn_published") &&
          !client_tal_spawn_attempted &&
          !client_remote_tal_remove_pending && remove_actor_count == 1u &&
          release_player_two_count == 0u &&
          cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] == NULL &&
          strcmp(client_tal_teardown_events, "R") == 0,
        "published pending Tal spawn permits immediate confirmed removal");

    reset_stub_policy();
    reset_stub_counts();
    runtime_config.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    session_status_result = TRUE;
    session_status.peer_connected = TRUE;
    session_status.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    session_status.local_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    session_status.peer_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
    client_tal_spawn_attempted = TRUE;
    client_remote_tal_owned = FALSE;
    client_remote_tal_request_owned = FALSE;
    client_remote_tal_remove_pending = FALSE;
    client_remote_tal_generation_actor = NULL;
    client_remote_tal_active_generation = 0u;
    client_release_pending_logged = FALSE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    actor_present_results[SUDEKIMP_CLEANROOM_TAL] = TRUE;
    remove_actor_clears_presence = FALSE;

    SetLastError(ERROR_SUCCESS);
    check(!release_client_remote_tal("test_remove_disappearance_pending") &&
          GetLastError() == ERROR_BUSY && client_tal_spawn_attempted &&
          client_remote_tal_remove_pending && remove_actor_count == 1u &&
          strcmp(client_tal_teardown_events, "R") == 0,
        "successful Tal remove request waits for positive native disappearance");

    SetLastError(ERROR_SUCCESS);
    lan_arena_control_update_observer(NULL, NULL, &witness);
    check(GetLastError() == ERROR_BUSY && client_tal_spawn_attempted &&
          client_remote_tal_remove_pending && remove_actor_count == 1u &&
          request_player_two_count == 0u &&
          client_tal_lease_publish_count == 0u &&
          initialize_party_actor_count == 0u && spawn_actor_count == 0u &&
          strcmp(client_tal_teardown_events, "R") == 0,
        "authenticated observer services pending removal without reclaiming Tal");

    actor_present_results[SUDEKIMP_CLEANROOM_TAL] = FALSE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = NULL;
    check(release_client_remote_tal("test_remove_disappeared") &&
          !client_tal_spawn_attempted &&
          !client_remote_tal_remove_pending && remove_actor_count == 1u &&
          strcmp(client_tal_teardown_events, "R") == 0,
        "positive Tal disappearance retires pending spawn and remove state");

    reset_stub_policy();
    reset_stub_counts();
    runtime_config.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    session_status_result = TRUE;
    session_status.peer_connected = TRUE;
    session_status.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    session_status.local_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    session_status.peer_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    actor_present_results[SUDEKIMP_CLEANROOM_TAL] = TRUE;
    client_tal_spawn_attempted = TRUE;
    client_remote_tal_owned = FALSE;
    client_remote_tal_request_owned = FALSE;
    client_remote_tal_remove_pending = FALSE;
    client_remote_tal_generation_actor = NULL;
    client_remote_tal_active_generation = 0u;
    client_release_pending_logged = FALSE;
    player_two_active = FALSE;
    player_two_character = NULL;
    player_two_request_result = TRUE;

    lan_arena_control_update_observer(NULL, NULL, &witness);
    check(request_player_two_count == 1u &&
          client_remote_tal_request_owned &&
          !client_remote_tal_owned &&
          client_remote_tal_active_generation == 0u,
        "accepted pre-publication PlayerTwo request acquires teardown ownership");

    player_two_release_result = FALSE;
    check(!release_client_remote_tal("test_player_two_request_pending") &&
          client_remote_tal_request_owned && client_tal_spawn_attempted &&
          release_player_two_count == 1u && remove_actor_count == 0u &&
          strcmp(client_tal_teardown_events, "L") == 0,
        "failed pre-publication PlayerTwo release retains request before actor removal");

    player_two_release_result = TRUE;
    check(release_client_remote_tal("test_player_two_request_released") &&
          !client_remote_tal_request_owned &&
          !client_remote_tal_owned && !client_tal_spawn_attempted &&
          !client_remote_tal_remove_pending &&
          release_player_two_count == 2u && remove_actor_count == 1u &&
          strcmp(client_tal_teardown_events, "LLR") == 0,
        "confirmed pre-publication PlayerTwo release precedes Tal removal");
    reset_stub_policy();
    reset_stub_counts();
}

static BOOL apply_host_spirit_fixture(SudekiMpLanArenaActorSnapshot *actor) {
    SudekiMpLanArenaSnapshot snapshot = {0};
    BOOL result;
    snapshot.seat[0] = *actor;
    result = host_apply_spirit_state(&snapshot);
    *actor = snapshot.seat[0];
    return result;
}

static void verify_host_spirit_lifecycle(void) {
    static uint8_t tal_character;
    SudekiMpLanArenaActorSnapshot tal;

    memset(&tal, 0, sizeof(tal));
    memset(host_actor_skill_sequence, 0, sizeof(host_actor_skill_sequence));
    memset(host_actor_skill_kind, 0, sizeof(host_actor_skill_kind));
    memset(host_actor_skill_slot, 0, sizeof(host_actor_skill_slot));
    memset(host_actor_skill_cost, 0, sizeof(host_actor_skill_cost));
    memset(host_actor_previous_skill_active, 0,
        sizeof(host_actor_previous_skill_active));
    host_spirit_previous_active = FALSE;
    host_spirit_previous_state = 0;
    spirit_presentation_state_result = TRUE;
    spirit_presentation_state = 0;
    character_skill_observe_result = TRUE;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    player_two_skill_isolation_enabled = FALSE;
    player_two_skill_isolation_call_count = 0u;

    check(apply_host_spirit_fixture(&tal) &&
          host_actor_skill_sequence[0] == 0u &&
          tal.skill_sequence == 0u &&
          player_two_skill_isolation_call_count == 0u,
        "inactive Spirit observation does not fabricate a transaction");

    spirit_presentation_state = 1;
    check(apply_host_spirit_fixture(&tal) &&
          host_actor_skill_sequence[0] == 1u &&
          tal.skill_sequence == 1u &&
          tal.skill_kind == SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT &&
          tal.skill_slot == 0u && tal.skill_cost == 0u &&
          tal.skill_active == 1u &&
          player_two_skill_isolation_enabled,
        "Spirit start publishes one host-authoritative Tal transaction");

    memset(&tal, 0, sizeof(tal));
    spirit_presentation_state = 2;
    check(apply_host_spirit_fixture(&tal) &&
          host_actor_skill_sequence[0] == 1u &&
          tal.skill_sequence == 1u && tal.skill_active == 1u,
        "Spirit native phase changes preserve one transaction sequence");

    memset(&tal, 0, sizeof(tal));
    spirit_presentation_state = 0;
    check(apply_host_spirit_fixture(&tal) &&
          tal.skill_sequence == 1u &&
          tal.skill_kind == SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT &&
          tal.skill_active == 0u,
        "Spirit completion publishes retirement for the same sequence");
    refresh_host_player_two_skill_isolation(&tal_character);
    check(!player_two_skill_isolation_enabled,
        "Spirit completion releases Ailish input isolation on refresh");

    memset(&tal, 0, sizeof(tal));
    spirit_presentation_state = 3;
    check(apply_host_spirit_fixture(&tal) &&
          tal.skill_sequence == 2u && tal.skill_active == 1u &&
          player_two_skill_isolation_enabled,
        "a later Spirit activation advances exactly one sequence");

    memset(&tal, 0, sizeof(tal));
    spirit_presentation_state_result = FALSE;
    check(!apply_host_spirit_fixture(&tal) &&
          tal.skill_sequence == 0u &&
          host_actor_skill_sequence[0] == 2u,
        "failed Spirit observation neither publishes nor advances state");

    spirit_presentation_state_result = TRUE;
    spirit_presentation_state = 0;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = NULL;
    reset_host_skill_tracking();
}

static void verify_remote_spirit_lifecycle(void) {
    SudekiMpLanArenaSnapshot snapshot = {0};
    SudekiMpLanArenaSessionConfig previous_config = runtime_config;
    unsigned int variant;
    reset_host_skill_tracking();
    SudekiMpLanArenaSetSeatTypes(SUDEKIMP_LAN_ARENA_BUKI_TYPE, SUDEKIMP_LAN_ARENA_ELCO_TYPE);
    runtime_config.host_actor_type = SUDEKIMP_LAN_ARENA_BUKI_TYPE;
    runtime_config.client_actor_type = SUDEKIMP_LAN_ARENA_ELCO_TYPE;
    snapshot.seat[0].actor_type = SUDEKIMP_LAN_ARENA_BUKI_TYPE;
    snapshot.seat[1].actor_type = SUDEKIMP_LAN_ARENA_ELCO_TYPE;
    spirit_presentation_state_result = TRUE;
    for (variant = 0u; variant < 2u; ++variant) {
        fixture_spirit_id = 6 + (int)variant;
        spirit_presentation_state = 1;
        check(host_apply_spirit_state(&snapshot) &&
              snapshot.seat[0].skill_sequence == 0u && snapshot.seat[0].skill_active == 0u &&
              snapshot.seat[1].skill_sequence == variant + 1u && snapshot.seat[1].skill_active == 1u &&
              host_spirit_actor_index == 1u,
            "both Elco Spirit variants belong to client actor, not Buki host");
        fixture_spirit_id = 4;
        check(!host_apply_spirit_state(&snapshot) && host_spirit_actor_index == 1u,
            "native owner replacement cannot silently migrate an active Spirit");
        fixture_spirit_id = 6 + (int)variant;
        spirit_presentation_state = 0;
        check(host_apply_spirit_state(&snapshot) && snapshot.seat[1].skill_active == 0u &&
              snapshot.seat[1].skill_sequence == variant + 1u,
            "Elco Spirit cleanup retires same actor and permits subsequent cast");
    }
    fixture_spirit_id = 0;
    runtime_config = previous_config;
    SudekiMpLanArenaSetSeatTypes(SUDEKIMP_LAN_ARENA_TAL_TYPE, SUDEKIMP_LAN_ARENA_AILISH_TYPE);
    reset_host_skill_tracking();
}

static void append_spirit_audio_event(
    uint32_t sequence,
    int native_state,
    const char *cue, uint16_t cast_sequence
) {
    SudekiMpLanArenaSpiritAudioEvent *event;
    size_t length = strlen(cue);
    check(spirit_audio_event_count <
            SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY &&
          length < SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_CUE_CAPACITY,
        "Spirit audio fixture remains bounded");
    if (spirit_audio_event_count >=
            SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY ||
        length >= SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_CUE_CAPACITY) return;
    event = &spirit_audio_events[spirit_audio_event_count++];
    memset(event, 0, sizeof(*event));
    event->sequence = sequence;
    event->native_state = native_state;
    event->session_token=session_status.session_token;
    event->skill_sequence=cast_sequence;
    event->cue_length = (uint8_t)length;
    memcpy(event->cue, cue, length + 1u);
}

static void verify_host_spirit_audio_semantic_journal(void) {
    BOOL previous_installed = spirit_audio_installed;
    SudekiMpLanArenaActorSnapshot tal;
    SudekiMpLanArenaSnapshot snapshot;
    HostSpiritAudioStage stage;
    SudekiMpLanArenaSessionStatus previous_status=session_status;
    BOOL previous_status_result=session_status_result;

    session_status_result=TRUE;
    session_status.peer_connected=TRUE;
    session_status.session_token=901u;

    spirit_audio_installed = TRUE;
    spirit_audio_event_count = 0u;
    memset(spirit_audio_events, 0, sizeof(spirit_audio_events));
    reset_host_spirit_audio_tracking();
    memset(&tal, 0, sizeof(tal));
    tal.skill_sequence = 7u;
    tal.skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    tal.skill_active = 1u;
    append_spirit_audio_event(1u, 2, "stop_tal",7u);
    append_spirit_audio_event(2u, 2, "spiritstrike_start",7u);
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.seat[0]=tal;
    host_capture_spirit_audio(&snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 1u &&
          snapshot.spirit_audio_history[0].event_sequence == 1u &&
          snapshot.spirit_audio_history[0].skill_sequence == 7u &&
          snapshot.spirit_audio_history[0].cue ==
              SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START,
        "host maps only the allowlisted raw start cue to exact Spirit sequence 7");
    check(host_spirit_audio_history_count == 0u,
        "host audio capture remains staged before canonical commit");
    commit_host_spirit_audio_stage(&stage);

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.seat[0]=tal;
    host_capture_spirit_audio(&snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 1u,
        "re-reading the trace cannot duplicate a semantic start event");
    commit_host_spirit_audio_stage(&stage);
    append_spirit_audio_event(3u, 2, "spiritstrike_start",7u);
    snapshot.seat[0]=tal;
    host_capture_spirit_audio(&snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 1u,
        "a repeated raw start cannot duplicate one Spirit transaction");
    commit_host_spirit_audio_stage(&stage);

    tal.skill_sequence = 8u;
    tal.skill_active = 0u;
    append_spirit_audio_event(4u, 2, "spiritstrike_start",8u);
    snapshot.seat[0]=tal;
    host_capture_spirit_audio(&snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 1u,
        "raw start without the matching active Spirit transaction is discarded");
    commit_host_spirit_audio_stage(&stage);
    tal.skill_active = 1u;
    snapshot.seat[0]=tal;
    host_capture_spirit_audio(&snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 1u,
        "discarded raw start cannot be rebound to a later active state");
    commit_host_spirit_audio_stage(&stage);
    append_spirit_audio_event(5u, 2, "spiritstrike_start",8u);
    snapshot.seat[0]=tal;
    host_capture_spirit_audio(&snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 2u &&
          snapshot.spirit_audio_history[1].event_sequence == 2u &&
          snapshot.spirit_audio_history[1].skill_sequence == 8u,
        "a later Spirit transaction receives one newer semantic start event");
    commit_host_spirit_audio_stage(&stage);

    reset_host_spirit_audio_tracking();
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.seat[0]=tal;
    host_capture_spirit_audio(&snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 0u,
        "session reset drains old raw cues and clears the wire journal");
    commit_host_spirit_audio_stage(&stage);
    append_spirit_audio_event(6u, 2, "spiritstrike_start",8u);
    snapshot.seat[0]=tal;
    host_capture_spirit_audio(&snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 1u &&
          snapshot.spirit_audio_history[0].event_sequence == 1u &&
          snapshot.spirit_audio_history[0].skill_sequence == 8u,
        "fresh post-reset raw cue starts a fresh semantic sequence");
    commit_host_spirit_audio_stage(&stage);

    /* If every active snapshot fails, its staged start must not poison the
     * persistent journal. The first retired frame consumes the raw trace edge
     * without audio, commits an empty journal, and later frames continue. */
    reset_host_spirit_audio_tracking();
    tal.skill_sequence = 9u;
    tal.skill_active = 1u;
    append_spirit_audio_event(7u, 2, "spiritstrike_start",9u);
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.seat[0]=tal;
    host_capture_spirit_audio(&snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 1u &&
          host_spirit_audio_history_count == 0u,
        "failed active snapshot leaves persistent audio journal unchanged");
    tal.skill_active = 0u;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.seat[0]=tal;
    host_capture_spirit_audio(&snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 0u,
        "retired snapshot drops a start never admitted while active");
    commit_host_spirit_audio_stage(&stage);
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.seat[0]=tal;
    host_capture_spirit_audio(&snapshot, &stage);
    check(snapshot.spirit_audio_history_count == 0u,
        "post-retirement snapshots continue after failed active publication");
    commit_host_spirit_audio_stage(&stage);
    reset_host_spirit_audio_tracking();
    memset(&snapshot,0,sizeof(snapshot));
    tal.skill_sequence=10u; tal.skill_active=1u;
    snapshot.seat[0]=snapshot.seat[1]=tal;
    append_spirit_audio_event(8u,2,"spiritstrike_start",10u);
    append_spirit_audio_event(9u,2,"spiritstrike_start",10u);
    spirit_audio_events[spirit_audio_event_count-1u].owner_seat=1u;
    host_capture_spirit_audio(&snapshot,&stage);
    check(snapshot.spirit_audio_history_count==2 &&
        snapshot.spirit_audio_history[0].owner_seat==0 &&
        snapshot.spirit_audio_history[1].owner_seat==1 &&
        snapshot.spirit_audio_history[0].skill_sequence==10 &&
        snapshot.spirit_audio_history[1].skill_sequence==10,
        "two native starts between frames survive as separate owner-exact audio events");
    check(host_spirit_audio_history_count==0,"both audio events remain staged until canonical commit");
    commit_host_spirit_audio_stage(&stage);
    append_spirit_audio_event(10u,2,"spiritstrike_start",11u);
    spirit_audio_events[spirit_audio_event_count-1u].session_token++;
    snapshot.seat[0].skill_sequence=11u;
    host_capture_spirit_audio(&snapshot,&stage);
    check(snapshot.spirit_audio_history_count==2,"reconnected session cannot inherit another session's native audio emission");
    commit_host_spirit_audio_stage(&stage);
    spirit_audio_installed = previous_installed;
    session_status=previous_status;
    session_status_result=previous_status_result;
    reset_host_spirit_audio_tracking();
}

static SudekiMpLanArenaSessionStatus prepare_host_spirit_operator_fixture(
    void *tal,
    void *ailish,
    uint64_t session_token
) {
    SudekiMpLanArenaSessionStatus status;
    reset_stub_policy();
    reset_stub_counts();
    runtime_installed = TRUE;
    runtime_game_module = (HMODULE)(uintptr_t)1u;
    runtime_config = make_config(SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL);
    tal_initialized = TRUE;
    ailish_initialized = TRUE;
    host_remote_ailish_owned = TRUE;
    player_two_active = TRUE;
    player_two_character = ailish;
    cleanroom_combat_enabled = TRUE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = tal;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = ailish;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    reset_host_skill_tracking();
    host_operator_spirit_session_token = session_token;
    memset(&status, 0, sizeof(status));
    status.phase = SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED;
    status.peer_connected = 1u;
    status.local_role = SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL;
    status.local_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
    status.peer_simulation_node_role =
        SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
    status.session_token = session_token;
    return status;
}

static void queue_host_spirit_request(unsigned int variant) {
    host_spirit_request_available = TRUE;
    host_spirit_request_variant = variant;
}

static void verify_host_spirit_operator_two_phase(void) {
    char tal_one;
    char tal_two;
    char ailish;
    char ailish_two;
    SudekiMpLanArenaSessionStatus status;
    SudekiMpLanArenaActorSnapshot snapshot;
    const uint64_t token = UINT64_C(0x1122334455667788);

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    host_operator_spirit_session_token = 0u;
    queue_host_spirit_request(2u);
    check(!host_operator_spirit_session_ready(&status) &&
          host_operator_spirit_session_token == token &&
          !host_spirit_request_available &&
          host_spirit_request_discard_count == 1u,
        "first authenticated Spirit generation discards a pre-session request");
    queue_host_spirit_request(1u);
    check(host_operator_spirit_session_ready(&status) &&
          host_spirit_request_available,
        "armed Spirit generation preserves a later same-session request");
    discard_host_operator_spirit_requests("test_generation_cleanup");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    host_spirit_describe_probe_teardown = TRUE;
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    check(host_operator_spirit_intent.pending &&
          host_operator_spirit_intent.seat[0] == &tal_one &&
          host_operator_spirit_intent.seat[1] == &ailish &&
          host_operator_spirit_intent.session_token == token &&
          host_operator_spirit_intent.variant == 1u &&
          ranged_combat_prime_pending &&
          ranged_combat_prime_call_count == 1u &&
          host_spirit_activation_call_count == 0u &&
          host_spirit_describe_probe_saw_busy &&
          host_spirit_describe_probe_depth == 1,
        "host Spirit operator admits one exact intent and starts only the UI prime");
    service_host_operator_spirit(&status, 0u);
    check(host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "host Spirit operator cannot activate while native UI prime is pending");
    ranged_combat_prime_pending = FALSE;
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 1u &&
          host_spirit_last_activated_variant == 1u &&
          host_actor_skill_sequence[0] == 0u,
        "positive UI retirement activates exactly once without fabricating a wire edge");
    service_host_operator_spirit(&status, 0u);
    check(host_spirit_activation_call_count == 1u,
        "retired host Spirit intent cannot execute twice");
    memset(&snapshot, 0, sizeof(snapshot));
    spirit_presentation_state = 3;
    check(apply_host_spirit_fixture(&snapshot) &&
          host_actor_skill_sequence[0] == 1u &&
          snapshot.skill_sequence == 1u &&
          snapshot.skill_active == 1u,
        "only the later positive native manager edge starts the Spirit wire transaction");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    ranged_combat_prime_pending = FALSE;
    host_spirit_activation_probe_teardown = TRUE;
    host_spirit_reproof_probe_teardown = TRUE;
    service_host_operator_spirit(&status, 0u);
    check(host_spirit_activation_call_count == 1u &&
          host_spirit_reproof_probe_saw_busy &&
          host_spirit_reproof_probe_depth == 1 &&
          host_spirit_activation_probe_saw_busy &&
          host_spirit_activation_probe_depth == 1 &&
          InterlockedCompareExchange(
              &host_operator_spirit_activation_depth, 0, 0) == 0,
        "native Spirit activation publishes an in-flight teardown barrier until return");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    host_spirit_option_available = FALSE;
    queue_host_spirit_request(2u);
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending &&
          ranged_combat_prime_call_count == 0u &&
          host_spirit_activation_call_count == 0u,
        "unavailable Tal Spirit variant is rejected before UI prime");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    spirit_presentation_state = 1;
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "already-active native Spirit manager rejects operator admission");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    status.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "wrong LAN role rejects host Spirit operator admission");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    cleanroom_menu_active = TRUE;
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending,
        "active cleanroom menu rejects host Spirit operator admission");
    cleanroom_menu_active = FALSE;
    cleanroom_pause_active = TRUE;
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending,
        "active LAN pause panel rejects host Spirit operator admission");
    cleanroom_pause_active = FALSE;
    ranged_combat_prime_pending = TRUE;
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending &&
          ranged_combat_prime_call_count == 0u,
        "an existing native UI transition rejects a new Spirit intent");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    queue_host_spirit_request(2u);
    service_host_operator_spirit(&status, 0u);
    check(host_operator_spirit_intent.pending &&
          host_spirit_request_take_count == 2u &&
          host_spirit_activation_call_count == 0u,
        "concurrent Spirit request is consumed without replacing pending intent");
    ranged_combat_prime_pending = FALSE;
    service_host_operator_spirit(&status, 0u);
    check(host_spirit_activation_call_count == 1u &&
          host_spirit_last_activated_variant == 1u,
        "concurrent request cannot change the admitted Spirit variant");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    ranged_combat_prime_pending = FALSE;
    status.session_token = token + 1u;
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "changed session token cancels primed Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    ranged_combat_prime_pending = FALSE;
    status.local_role = SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH;
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "changed authority role cancels primed Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    ranged_combat_prime_pending = FALSE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_two;
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "changed Tal identity cancels primed Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    ranged_combat_prime_pending = FALSE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = &ailish_two;
    player_two_character = &ailish_two;
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "changed Ailish identity cancels primed Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    ranged_combat_prime_pending = FALSE;
    player_two_active = FALSE;
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "lost Ailish Player-2 lease cancels primed Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    ranged_combat_prime_pending = FALSE;
    host_tal_controller_lease_exact = FALSE;
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "changed Tal controller lease cancels primed Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    ranged_combat_prime_pending = FALSE;
    cleanroom_menu_active = TRUE;
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "menu entry after prime cancels Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    ranged_combat_prime_pending = FALSE;
    cleanroom_pause_active = TRUE;
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "pause entry after prime cancels Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    ranged_combat_prime_pending = FALSE;
    cleanroom_combat_enabled = FALSE;
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "combat retirement cancels primed Spirit intent before activation");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    host_native_skill_leases[0].pending = TRUE;
    queue_host_spirit_request(1u);
    service_host_operator_spirit(&status, 0u);
    check(!host_operator_spirit_intent.pending &&
          host_spirit_activation_call_count == 0u,
        "concurrent native CSkill lease rejects Spirit operator admission");

    status = prepare_host_spirit_operator_fixture(
        &tal_one, &ailish, token);
    host_operator_spirit_intent.pending = TRUE;
    host_operator_spirit_intent.seat[0] = &tal_one;
    host_operator_spirit_intent.seat[1] = &ailish;
    host_operator_spirit_intent.session_token = token;
    host_operator_spirit_intent.variant = 1u;
    queue_host_spirit_request(2u);
    discard_host_operator_spirit_requests("test_session_loss");
    check(!host_operator_spirit_intent.pending &&
          !host_spirit_request_available &&
          host_spirit_request_discard_count == 1u,
        "authority reset clears retained and raw Spirit operator requests");

    host_operator_spirit_intent.pending = TRUE;
    SetLastError(ERROR_SUCCESS);
    check(!host_native_tasks_drained() && GetLastError() == ERROR_BUSY,
        "retained Spirit operator intent is a host teardown barrier");
    reset_host_operator_spirit_intent();
    ranged_combat_prime_pending = TRUE;
    SetLastError(ERROR_SUCCESS);
    check(!host_native_tasks_drained() && GetLastError() == ERROR_BUSY,
        "engine-owned Spirit UI prime remains a teardown barrier after intent clear");

    reset_host_skill_tracking();
    runtime_installed = FALSE;
    runtime_game_module = NULL;
    host_remote_ailish_owned = FALSE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = NULL;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = NULL;
    ranged_combat_prime_pending = FALSE;
    spirit_presentation_state = 0;
}

static void verify_host_character_skill_observation_gap(void) {
    static uint8_t tal_character;
    SudekiMpLanArenaActorSnapshot tal;

    memset(&tal, 0, sizeof(tal));
    memset(host_actor_skill_sequence, 0, sizeof(host_actor_skill_sequence));
    memset(host_actor_skill_kind, 0, sizeof(host_actor_skill_kind));
    memset(host_actor_skill_slot, 0, sizeof(host_actor_skill_slot));
    memset(host_actor_skill_cost, 0, sizeof(host_actor_skill_cost));
    memset(host_actor_previous_skill_active, 0,
        sizeof(host_actor_previous_skill_active));
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    character_skill_observe_result = TRUE;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    character_skill_observation.skill = &tal_character;
    character_skill_observation.slot = 2;
    character_skill_observation.cost = 45u;
    character_skill_observation.active = 1u;
    player_two_skill_isolation_enabled = FALSE;
    player_two_skill_isolation_call_count = 0u;

    check(host_apply_skill_state(
              0u, SUDEKIMP_CLEANROOM_TAL, &tal) &&
          tal.skill_sequence == 1u && tal.skill_active == 1u &&
          host_actor_skill_sequence[0] == 1u &&
          host_actor_previous_skill_active[0] &&
          player_two_skill_isolation_enabled,
        "host starts one Tal CSkill sequence from an exact active observation");

    memset(&tal, 0, sizeof(tal));
    character_skill_observe_result = FALSE;
    SetLastError(ERROR_SUCCESS);
    check(!host_apply_skill_state(
              0u, SUDEKIMP_CLEANROOM_TAL, &tal) &&
          host_actor_skill_sequence[0] == 1u &&
          host_actor_previous_skill_active[0] &&
          host_actor_skill_slot[0] == 2u &&
          player_two_skill_isolation_enabled,
        "failed host CSkill observation preserves sequence and active ownership");

    memset(&tal, 0, sizeof(tal));
    character_skill_observe_result = TRUE;
    check(host_apply_skill_state(
              0u, SUDEKIMP_CLEANROOM_TAL, &tal) &&
          tal.skill_sequence == 1u && tal.skill_active == 1u &&
          host_actor_skill_sequence[0] == 1u &&
          player_two_skill_isolation_call_count == 1u,
        "active observation after a gap resumes the original CSkill sequence");

    memset(&tal, 0, sizeof(tal));
    character_skill_observation.active = 0u;
    check(host_apply_skill_state(
              0u, SUDEKIMP_CLEANROOM_TAL, &tal) &&
          tal.skill_sequence == 1u && tal.skill_active == 0u &&
          !host_actor_previous_skill_active[0] &&
          !player_two_skill_isolation_enabled,
        "positive inactive observation retires the retained Tal sequence");

    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = NULL;
    character_skill_observe_result = TRUE;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    reset_host_skill_tracking();
}

static void verify_host_exact_character_skill_sequences(void) {
    static uint8_t tal_character;
    static uint8_t tal_skill;
    static uint8_t ailish_character;
    static uint8_t ailish_skill;
    static uint8_t foreign_character;
    SudekiMpSkillActivationResult started;
    SudekiMpLanArenaActorSnapshot tal;

    reset_host_skill_tracking();
    memset(&tal, 0, sizeof(tal));
    memset(&started, 0, sizeof(started));
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    character_skill_observe_result = TRUE;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    character_skill_observation.skill = &tal_skill;
    character_skill_observation.slot = 2;
    character_skill_observation.cost = 45u;
    character_skill_observation.active = 1u;
    started.status = SUDEKIMP_SKILL_ACTIVATION_STARTED;
    started.skill = &tal_skill;
    started.slot = 2;

    host_track_started_native_skill(0u, &tal_character, &started);
    check(host_actor_skill_sequence[0] == 1u &&
          host_native_skill_leases[0].sequence_allocated &&
          host_native_skill_leases[0].wire_sequence == 1u &&
          !host_native_skill_leases[0].active_seen,
        "exact STARTED return allocates the first Tal wire sequence before observation");
    check(host_apply_skill_state(
              0u, SUDEKIMP_CLEANROOM_TAL, &tal) &&
          tal.skill_sequence == 1u && tal.skill_active == 1u &&
          tal.skill_cost == 45u &&
          host_actor_skill_sequence[0] == 1u,
        "active snapshot adopts an exact STARTED sequence without double increment");

    /* This is the problematic 1 -> 0 -> 1 same-slot cycle with both the
     * inactive edge and the second active edge hidden between snapshots.
     * The second exact admission must still create a distinct transaction. */
    host_track_started_native_skill(0u, &tal_character, &started);
    memset(&tal, 0, sizeof(tal));
    check(host_actor_skill_sequence[0] == 2u &&
          host_native_skill_leases[0].wire_sequence == 2u &&
          host_native_skill_leases[0].sequence_allocated &&
          host_apply_skill_state(
              0u, SUDEKIMP_CLEANROOM_TAL, &tal) &&
          tal.skill_sequence == 2u && tal.skill_active == 1u &&
          host_actor_skill_sequence[0] == 2u,
        "same-slot activation hidden between 20 Hz samples keeps its exact second sequence");

    character_skill_observation.active = 0u;
    host_native_tal_skill_started(
        &tal_character, &tal_skill, 2, 45u, FALSE);
    check(host_actor_skill_sequence[0] == 3u &&
          host_native_skill_leases[0].pending &&
          !host_native_skill_leases[0].active_seen &&
          host_native_skill_startup_pending(
              0u, &tal_character, &character_skill_observation),
        "native Tal Use STARTED allocates during the inactive-byte startup gap");
    character_skill_observation.active = 1u;
    memset(&tal, 0, sizeof(tal));
    check(host_actor_skill_sequence[0] == 3u &&
          host_apply_skill_state(
              0u, SUDEKIMP_CLEANROOM_TAL, &tal) &&
          host_native_skill_leases[0].active_seen &&
          tal.skill_sequence == 3u &&
          host_actor_skill_sequence[0] == 3u,
        "host native UI startup-gap sequence is adopted once active appears");
    host_native_tal_skill_started(
        &foreign_character, &tal_skill, 2, 45u, TRUE);
    check(host_actor_skill_sequence[0] == 3u,
        "host native UI admission rejects a foreign actor identity");

    character_skill_observation.active = 0u;
    memset(&tal, 0, sizeof(tal));
    check(host_apply_skill_state(
              0u, SUDEKIMP_CLEANROOM_TAL, &tal) &&
          tal.skill_sequence == 3u && tal.skill_active == 0u &&
          !host_native_skill_leases[0].pending,
        "exact inactive observation retires the latest admitted sequence");

    reset_host_skill_tracking();
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = &ailish_character;
    character_skill_observation.skill = &ailish_skill;
    character_skill_observation.slot = 4;
    character_skill_observation.cost = 30u;
    character_skill_observation.active = 1u;
    started.skill = &ailish_skill;
    started.slot = 4;
    host_track_started_native_skill(1u, &ailish_character, &started);
    memset(&tal, 0, sizeof(tal));
    check(host_actor_skill_sequence[1] == 1u &&
          host_native_skill_leases[1].wire_sequence == 1u &&
          host_apply_skill_state(
              1u, SUDEKIMP_CLEANROOM_AILISH, &tal) &&
          tal.skill_sequence == 1u && tal.skill_cost == 30u &&
          host_actor_skill_sequence[1] == 1u,
        "remote Ailish exact STARTED sequence is adopted without snapshot double increment");
    host_track_started_native_skill(1u, &ailish_character, &started);
    memset(&tal, 0, sizeof(tal));
    check(host_actor_skill_sequence[1] == 2u &&
          host_apply_skill_state(
              1u, SUDEKIMP_CLEANROOM_AILISH, &tal) &&
          tal.skill_sequence == 2u &&
          host_actor_skill_sequence[1] == 2u,
        "remote Ailish same-slot replay hidden between snapshots gets a distinct sequence");

    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = NULL;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = NULL;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    reset_host_skill_tracking();
}

static void verify_host_snapshot_failure_telemetry_policy(void) {
    SudekiMpLanArenaSnapshot snapshot;
    unsigned int baseline;

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.seat[0].skill_sequence = 7u;
    snapshot.seat[0].skill_kind =
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    snapshot.seat[0].skill_active = 1u;
    snapshot.seat[0].skill_presentation_selector[0] = 112;
    ZeroMemory(&host_snapshot_failure_telemetry,
        sizeof(host_snapshot_failure_telemetry));
    baseline = log_format_call_count;

    host_snapshot_publish_failed(
        100u, 33u, HOST_SNAPSHOT_FAILURE_SNAPSHOT_VALIDATION, &snapshot);
    check(log_format_call_count == baseline + 1u &&
          host_snapshot_failure_telemetry.stage ==
              HOST_SNAPSHOT_FAILURE_SNAPSHOT_VALIDATION &&
          host_snapshot_failure_telemetry.consecutive_failures == 1u,
        "first unsupported Spirit selector snapshot logs its validation stage");
    host_snapshot_publish_failed(
        200u, 33u, HOST_SNAPSHOT_FAILURE_SNAPSHOT_VALIDATION, &snapshot);
    host_snapshot_publish_failed(
        1099u, 33u, HOST_SNAPSHOT_FAILURE_SNAPSHOT_VALIDATION, &snapshot);
    check(log_format_call_count == baseline + 1u &&
          host_snapshot_failure_telemetry.consecutive_failures == 3u,
        "repeated snapshot validation failures are quiet inside one second");
    host_snapshot_publish_failed(
        1100u, 33u, HOST_SNAPSHOT_FAILURE_SNAPSHOT_VALIDATION, &snapshot);
    check(log_format_call_count == baseline + 2u &&
          host_snapshot_failure_telemetry.consecutive_failures == 4u,
        "sustained snapshot validation failures aggregate once per second");

    host_snapshot_publish_failed(
        1101u, 33u, HOST_SNAPSHOT_FAILURE_SEND, &snapshot);
    check(log_format_call_count == baseline + 3u &&
          host_snapshot_failure_telemetry.stage ==
              HOST_SNAPSHOT_FAILURE_SEND &&
          host_snapshot_failure_telemetry.consecutive_failures == 1u,
        "a changed first-failure stage logs its transition immediately");
    host_snapshot_publish_succeeded(1102u, 33u);
    check(log_format_call_count == baseline + 4u &&
          host_snapshot_failure_telemetry.stage ==
              HOST_SNAPSHOT_FAILURE_NONE,
        "the first successful send closes and clears failure telemetry");
    host_snapshot_publish_succeeded(1103u, 33u);
    check(log_format_call_count == baseline + 4u,
        "an already-healthy snapshot stream emits no recovery spam");
}

static void verify_host_character_skill_sidecar_wire_fallback(void) {
    SudekiMpLanArenaActorSnapshot snapshot;
    SudekiMpLanArenaActorSnapshot zero;
    unsigned int channel;

    memset(&snapshot, 0, sizeof(snapshot));
    memset(&zero, 0, sizeof(zero));
    memset(host_actor_presentation, 0, sizeof(host_actor_presentation));
    memset(host_actor_presentation_valid, 0,
        sizeof(host_actor_presentation_valid));
    host_actor_presentation_valid[1] = TRUE;
    for (channel = 0u;
         channel < SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHANNELS;
         ++channel) {
        host_actor_presentation[1].selector[channel] =
            channel == 0u ? AILISH_COMBAT_IDLE_SELECTOR : 0;
        host_actor_presentation[1].state[channel] =
            channel == 0u ? 0u : 192u;
        host_actor_presentation[1].rate[channel] = 24.0f;
        host_actor_presentation[1].time[channel] = 40.8f;
    }
    snapshot.skill_sequence = 6u;
    snapshot.skill_kind =
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    snapshot.skill_slot = 5u;
    snapshot.skill_active = 1u;
    snapshot.skill_cost = 40u;
    host_actor_presentation[1].time[4] = 4096.0f;
    host_apply_skill_presentation(1u, &snapshot);
    check(snapshot.skill_presentation_valid == 1u &&
          snapshot.skill_presentation_channel_count == 5u &&
          snapshot.skill_presentation_time[4] == 4096.0f &&
          SudekiMpLanArenaSkillPresentationValid(
              &snapshot, SUDEKIMP_LAN_ARENA_AILISH_TYPE),
        "bounded Ailish character-skill renderer sidecar survives at the exact wire clock limit");

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.skill_sequence = 6u;
    snapshot.skill_kind =
        SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER;
    snapshot.skill_slot = 5u;
    snapshot.skill_active = 1u;
    snapshot.skill_cost = 40u;
    host_actor_presentation[1].time[4] = 4161.93896f;
    host_apply_skill_presentation(1u, &snapshot);
    check(snapshot.skill_sequence == 6u &&
          snapshot.skill_kind ==
              SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER &&
          snapshot.skill_slot == 5u && snapshot.skill_active == 1u &&
          snapshot.skill_cost == 40u &&
          snapshot.skill_presentation_valid == 0u &&
          snapshot.skill_presentation_channel_count == 0u &&
          memcmp(snapshot.skill_presentation_selector,
              zero.skill_presentation_selector,
              sizeof(snapshot.skill_presentation_selector)) == 0 &&
          memcmp(snapshot.skill_presentation_state,
              zero.skill_presentation_state,
              sizeof(snapshot.skill_presentation_state)) == 0 &&
          memcmp(snapshot.skill_presentation_rate,
              zero.skill_presentation_rate,
              sizeof(snapshot.skill_presentation_rate)) == 0 &&
          memcmp(snapshot.skill_presentation_time,
              zero.skill_presentation_time,
              sizeof(snapshot.skill_presentation_time)) == 0 &&
          memcmp(snapshot.skill_presentation_blend,
              zero.skill_presentation_blend,
              sizeof(snapshot.skill_presentation_blend)) == 0 &&
          SudekiMpLanArenaSkillPresentationValid(
              &snapshot, SUDEKIMP_LAN_ARENA_AILISH_TYPE),
        "out-of-range dormant Ailish channel omits only the optional sidecar and preserves sequence 6 slot 5");

    memset(&snapshot, 0, sizeof(snapshot));
    memset(&host_actor_presentation[0], 0,
        sizeof(host_actor_presentation[0]));
    host_actor_presentation_valid[0] = TRUE;
    host_actor_presentation[0].selector[0] = 113;
    host_actor_presentation[0].state[0] = 1u;
    host_actor_presentation[0].state[1] = 192u;
    host_actor_presentation[0].rate[0] = 24.0f;
    host_actor_presentation[0].time[0] = 64.0f;
    snapshot.skill_sequence = 7u;
    snapshot.skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    snapshot.skill_active = 1u;
    host_apply_skill_presentation(0u, &snapshot);
    check(snapshot.skill_presentation_valid == 1u &&
          snapshot.skill_presentation_selector[0] == 113 &&
          SudekiMpLanArenaSkillPresentationValid(
              &snapshot, SUDEKIMP_LAN_ARENA_TAL_TYPE),
        "host keeps authored Spirit selector 113 publishable during the active middle stage");

    memset(&snapshot, 0, sizeof(snapshot));
    host_actor_presentation[0].selector[0] = 112;
    snapshot.skill_sequence = 7u;
    snapshot.skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    snapshot.skill_active = 1u;
    host_apply_skill_presentation(0u, &snapshot);
    check(snapshot.skill_presentation_valid == 1u &&
          !SudekiMpLanArenaSkillPresentationValid(
              &snapshot, SUDEKIMP_LAN_ARENA_TAL_TYPE),
        "host retains a future unsupported Spirit selector for fail-closed snapshot diagnostics");

    memset(&snapshot, 0, sizeof(snapshot));
    host_actor_presentation[0].selector[0] = 75;
    host_actor_presentation[0].time[0] = 4161.93896f;
    snapshot.skill_sequence = 7u;
    snapshot.skill_kind = SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    snapshot.skill_active = 1u;
    host_apply_skill_presentation(0u, &snapshot);
    check(snapshot.skill_presentation_valid == 1u,
        "active Spirit retains its required host renderer sidecar");
    check(snapshot.skill_presentation_time[0] > 4096.0f,
        "active Spirit keeps the observed out-of-range renderer clock");
    check(!SudekiMpLanArenaSkillPresentationValid(
              &snapshot, SUDEKIMP_LAN_ARENA_TAL_TYPE),
        "active Spirit keeps its required sidecar fail-closed instead of silently degrading");

    memset(host_actor_presentation, 0, sizeof(host_actor_presentation));
    memset(host_actor_presentation_valid, 0,
        sizeof(host_actor_presentation_valid));
}

static void verify_host_noncaster_startup_gap_ownership(void) {
    static uint8_t tal_character;
    static uint8_t ailish_character;
    static uint8_t ailish_skill;
    static uint8_t foreign_skill;
    SudekiMpSkillActivationResult started;
    SudekiMpCharacterSkillState state;

    reset_host_skill_tracking();
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = &ailish_character;
    memset(&started, 0, sizeof(started));
    started.status = SUDEKIMP_SKILL_ACTIVATION_STARTED;
    started.skill = &ailish_skill;
    started.slot = 4;
    host_track_started_native_skill(1u, &ailish_character, &started);
    memset(&state, 0, sizeof(state));
    state.skill = &ailish_skill;
    state.slot = -1;
    state.active = 0u;

    check(host_native_skill_startup_pending(
              1u, &ailish_character, &state),
        "pre-world ownership retains Tal locomotion during Ailish STARTED active-byte gap");
    state.skill = &foreign_skill;
    check(!host_native_skill_startup_pending(
              1u, &ailish_character, &state),
        "pre-world startup ownership rejects a mismatched native skill lease");
    state.skill = &ailish_skill;
    check(!host_native_skill_startup_pending(
              1u, &tal_character, &state),
        "pre-world startup ownership rejects a mismatched caster actor");
    host_native_skill_leases[1].active_seen = TRUE;
    check(!host_native_skill_startup_pending(
              1u, &ailish_character, &state),
        "pre-world startup-only ownership ends after exact active observation");

    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = NULL;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = NULL;
    reset_host_skill_tracking();
}

static void verify_host_native_task_drain_without_peer(void) {
    static uint8_t tal_character;
    static uint8_t tal_skill;
    SudekiMpSkillActivationResult started;

    runtime_config.local_role = SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL;
    tal_initialized = FALSE;
    ailish_initialized = FALSE;
    host_remote_ailish_owned = FALSE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = &tal_character;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH] = NULL;
    character_skill_observe_result = TRUE;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    character_skill_observation.skill = &tal_skill;
    character_skill_observation.slot = 2;
    character_skill_observation.active = 1u;
    spirit_presentation_state_result = TRUE;
    spirit_presentation_state = 0;
    reset_host_skill_tracking();

    SetLastError(ERROR_SUCCESS);
    check(!host_native_tasks_drained() && GetLastError() == ERROR_BUSY,
        "present no-peer Tal CSkill blocks host teardown");

    memset(&started, 0, sizeof(started));
    started.status = SUDEKIMP_SKILL_ACTIVATION_STARTED;
    started.skill = &tal_skill;
    started.slot = 2u;
    host_track_started_native_skill(0u, &tal_character, &started);
    character_skill_observation.active = 0u;
    player_two_skill_isolation_enabled = FALSE;
    refresh_host_player_two_skill_isolation(&tal_character);
    check(player_two_skill_isolation_enabled,
        "Tal STARTED lease retains Ailish input through the active-byte startup gap");
    SetLastError(ERROR_SUCCESS);
    check(!host_native_tasks_drained() && GetLastError() == ERROR_BUSY &&
          host_native_skill_leases[0].pending &&
          !host_native_skill_leases[0].active_seen,
        "STARTED followed by an early inactive read retains the host task lease");

    character_skill_observation.active = 1u;
    check(!host_native_tasks_drained() &&
          host_native_skill_leases[0].pending &&
          host_native_skill_leases[0].active_seen,
        "exact active observation arms host task retirement");
    character_skill_observation.active = 0u;
    check(host_native_tasks_drained() &&
          !host_native_skill_leases[0].pending,
        "exact inactive observation after active retires host task lease");
    refresh_host_player_two_skill_isolation(&tal_character);
    check(!player_two_skill_isolation_enabled,
        "retired Tal task releases Ailish input isolation");

    spirit_presentation_state = 1;
    SetLastError(ERROR_SUCCESS);
    check(!host_native_tasks_drained() && GetLastError() == ERROR_BUSY,
        "present no-peer Tal Spirit transaction blocks host teardown");
    spirit_presentation_state = 0;
    check(host_native_tasks_drained(),
        "positive inactive no-peer Tal and Spirit observations permit teardown");

    cast_context_drained=FALSE;
    check(!host_native_tasks_drained() && GetLastError()==ERROR_BUSY,
        "completed actor and Spirit do not release a surviving child script");
    cast_context_drained=TRUE;
    check(host_native_tasks_drained(),
        "positive child task drain permits native actor teardown");

    host_remote_skill_camera_active=TRUE;
    cast_context_current=TRUE;
    cast_context_owner.actor_type=(uint8_t)seat_host_type();
    check(!host_remote_skill_camera_owned(),
        "host script camera is not suppressed by another remote cast");
    cast_context_owner.actor_type=(uint8_t)seat_client_type();
    host_remote_skill_camera_active=FALSE;
    check(host_remote_skill_camera_owned(),
        "remote child script cannot claim the host camera after root cleanup");
    cast_context_current=FALSE;
    check(!host_remote_skill_camera_owned(),
        "ordinary unowned camera retains existing single-cast lease policy");

    ranged_combat_prime_pending = TRUE;
    SetLastError(ERROR_SUCCESS);
    check(!host_native_tasks_drained() && GetLastError() == ERROR_BUSY,
        "pending native ranged-prime timer blocks host teardown");
    ranged_combat_prime_pending = FALSE;

    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = NULL;
    memset(&character_skill_observation, 0,
        sizeof(character_skill_observation));
    reset_host_skill_tracking();
}

static void verify_authoritative_locomotion_stop_policy(void) {
    SudekiMpLanArenaActorSnapshot tal;
    SudekiMpLanArenaActorSnapshot ailish;
    memset(&tal, 0, sizeof(tal));
    memset(&ailish, 0, sizeof(ailish));
    tal.hp = 100u;
    ailish.hp = 100u;
    memset(host_previous_actor_position, 0,
        sizeof(host_previous_actor_position));
    memset(host_previous_actor_position_valid, 0,
        sizeof(host_previous_actor_position_valid));
    memset(host_actor_last_translation_at_ms, 0,
        sizeof(host_actor_last_translation_at_ms));
    memset(host_replica_idle_position, 0,
        sizeof(host_replica_idle_position));
    memset(host_replica_idle_position_valid, 0,
        sizeof(host_replica_idle_position_valid));
    memset(host_actor_was_moving, 0,
        sizeof(host_actor_was_moving));
    host_ailish_idle_variant_state = 0u;
    host_ailish_idle_variant_seen_at_ms = 0u;
    host_ailish_idle_variant_armed = TRUE;
    memset(host_actor_presentation_valid, 0,
        sizeof(host_actor_presentation_valid));
    reset_host_action_tracking();

    host_apply_presentation_state(0u, 100u, FALSE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        "first Tal sample begins idle without fabricated translation");
    tal.x = 0.01f;
    host_apply_presentation_state(0u, 150u, FALSE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING,
        "authoritative Tal horizontal translation starts locomotion");
    host_apply_presentation_state(0u, 300u, FALSE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING,
        "Tal stop grace includes its exact bounded endpoint");
    host_apply_presentation_state(0u, 301u, FALSE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        "Tal becomes idle immediately after bounded stop grace");
    tal.x = 0.012f;
    host_apply_presentation_state(0u, 351u, FALSE, &tal);
    check(fabsf(tal.x - 0.01f) < 0.00001f,
        "stationary Tal replica suppresses sub-threshold native settling");
    tal.x = 0.014f;
    host_apply_presentation_state(0u, 376u, FALSE, &tal);
    check(fabsf(tal.x - 0.01f) < 0.00001f,
        "successive sub-threshold Tal steps remain bounded by idle latch");
    tal.x = 0.016f;
    host_apply_presentation_state(0u, 401u, FALSE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING &&
          fabsf(tal.x - 0.016f) < 0.00001f,
        "cumulative Tal translation releases latch without hidden backlog");

    host_actor_presentation_valid[0] = TRUE;
    host_actor_presentation[0].selector[0] = TAL_WORLD_IDLE_SELECTOR;
    tal.x = 0.03f;
    host_apply_presentation_state(0u, 451u, FALSE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        "native Tal idle selector ends replica locomotion despite root motion");
    host_actor_presentation[0].selector[0] = TAL_WORLD_MOVE_PRIMARY_SELECTOR;
    host_apply_presentation_state(0u, 1000u, FALSE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING,
        "native Tal movement selector preserves visible host locomotion");
    {
        uint8_t saved_host_type = runtime_config.host_actor_type;
        const uint8_t types[] = {SUDEKIMP_LAN_ARENA_TAL_TYPE,
            SUDEKIMP_LAN_ARENA_BUKI_TYPE};
        unsigned int hero;
        host_actor_previous_skill_active[1] = FALSE;
        for (hero = 0u; hero < 2u; ++hero) {
            DWORD tick = 1500u + hero * 1000u;
            runtime_config.host_actor_type = types[hero];
            host_actor_presentation[0].selector[0] = hero == 0u ?
                TAL_COMBAT_IDLE_SELECTOR : BUKI_COMBAT_IDLE_SELECTOR;
            host_spirit_previous_active = TRUE;
            host_spirit_actor_index = 1u;
            tal.x += 1.0f;
            {
                float translated_x = tal.x;
                host_apply_presentation_state(0u, tick, TRUE, &tal);
                check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING &&
                    host_actor_locomotion_moving[0] && tal.x == translated_x,
                    "remote Spirit with inactive CSkill releases noncaster idle position and drives run compositor");
            }
            host_apply_presentation_state(0u, tick + 151u, TRUE, &tal);
            check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE &&
                !host_actor_locomotion_moving[0],
                "noncaster still stops after bounded grace during remote Spirit");
            host_spirit_actor_index = 0u;
            tal.x += 0.1f;
            host_apply_presentation_state(0u, tick + 200u, TRUE, &tal);
            check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
                "own Spirit does not acquire remote noncaster translation exception");
            host_spirit_previous_active = FALSE;
            tal.x += 0.1f;
            host_apply_presentation_state(0u, tick + 250u, TRUE, &tal);
            check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
                "Spirit cleanup restores native idle/root-motion suppression");
        }
        runtime_config.host_actor_type = saved_host_type;
    }
    host_actor_presentation_valid[0] = FALSE;

    host_remote_ailish_owned = TRUE;
    host_remote_ailish_moving = TRUE;
    host_apply_presentation_state(1u, 1000u, FALSE, &ailish);
    ailish.y = 1.0f;
    host_apply_presentation_state(1u, 1050u, FALSE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        "held Ailish input and vertical floor motion cannot fabricate running");
    ailish.z = 0.01f;
    host_apply_presentation_state(1u, 1100u, FALSE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING,
        "authoritative Ailish horizontal translation starts locomotion");
    host_apply_presentation_state(1u, 1251u, FALSE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        "Ailish stops against a wall despite continuously held input");
    ailish.z = 0.012f;
    host_apply_presentation_state(1u, 1301u, FALSE, &ailish);
    check(fabsf(ailish.z - 0.01f) < 0.00001f,
        "stationary Ailish replica suppresses native root-motion settling");
    host_remote_ailish_owned = FALSE;
    host_remote_ailish_moving = FALSE;
    ailish.z = 0.25f;
    host_apply_presentation_state(1u, 1351u, FALSE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE &&
          fabsf(ailish.z - 0.01f) < 0.00001f,
        "Ailish idle root-motion lunge cannot release latch without input");

    memset(&host_actor_presentation[1], 0,
        sizeof(host_actor_presentation[1]));
    host_actor_presentation_valid[1] = TRUE;
    host_ailish_idle_variant_state = 0u;
    host_ailish_idle_variant_seen_at_ms = 0u;
    host_ailish_idle_variant_armed = TRUE;
    host_actor_presentation[1].selector[0] = AILISH_WORLD_IDLE_SELECTOR;
    host_actor_presentation[1].selector[2] =
        AILISH_WORLD_IDLE_VARIANT_ONE_SELECTOR;
    host_actor_presentation[1].state[2] = 1u;
    host_apply_presentation_state(1u, 2000u, FALSE, &ailish);
    check(ailish.animation_state ==
            SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE,
        "Ailish idle variant is recognized while cross-fading on channel two");
    host_actor_presentation[1].selector[2] = 0;
    host_actor_presentation[1].state[2] = 192u;
    host_apply_presentation_state(1u, 2250u, FALSE, &ailish);
    check(ailish.animation_state ==
            SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE,
        "Ailish idle variant survives exact channel-transition grace endpoint");
    host_apply_presentation_state(1u, 2251u, FALSE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        "Ailish idle variant retires after stable base-idle evidence");
    host_actor_presentation[1].selector[0] =
        AILISH_WORLD_IDLE_VARIANT_TWO_SELECTOR;
    host_actor_presentation[1].state[0] = 1u;
    host_apply_presentation_state(1u, 2300u, FALSE, &ailish);
    check(ailish.animation_state ==
            SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO,
        "Ailish second idle variant remains recognized on channel zero");

    host_remote_ailish_moving = TRUE;
    ailish.z += 0.02f;
    host_apply_presentation_state(1u, 2350u, FALSE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING &&
          !host_ailish_idle_variant_armed,
        "Ailish movement cancels and disarms the active idle variant");
    host_remote_ailish_moving = FALSE;
    host_actor_presentation[1].selector[0] = AILISH_WORLD_IDLE_SELECTOR;
    host_actor_presentation[1].state[0] = 128u;
    host_actor_presentation[1].selector[2] =
        AILISH_WORLD_IDLE_VARIANT_TWO_SELECTOR;
    host_actor_presentation[1].state[2] = 65u;
    host_apply_presentation_state(1u, 2400u, FALSE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE &&
          !host_ailish_idle_variant_armed,
        "hidden state-65 Ailish variant cannot resume after movement");
    host_actor_presentation[1].selector[2] = 0;
    host_actor_presentation[1].state[2] = 192u;
    host_apply_presentation_state(1u, 2450u, FALSE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE &&
          host_ailish_idle_variant_armed,
        "clean Ailish base idle rearms future native variants");
    host_actor_presentation[1].selector[2] =
        AILISH_WORLD_IDLE_VARIANT_ONE_SELECTOR;
    host_actor_presentation[1].state[2] = 1u;
    host_apply_presentation_state(1u, 2500u, FALSE, &ailish);
    check(ailish.animation_state ==
            SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE,
        "new native state-1 Ailish variant starts after clean rearm");

    memset(host_actor_presentation, 0, sizeof(host_actor_presentation));
    memset(host_actor_presentation_valid, 0,
        sizeof(host_actor_presentation_valid));
    host_actor_presentation_valid[0] = TRUE;
    host_actor_presentation[0].selector[0] = TAL_COMBAT_IDLE_SELECTOR;
    host_actor_presentation[0].state[0] = 128u;
    tal.x += 0.03f;
    host_apply_presentation_state(0u, 3000u, TRUE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        "Tal combat idle overrides residual authoritative root translation");
    host_actor_presentation[0].selector[0] =
        TAL_COMBAT_MOVE_PRIMARY_SELECTOR;
    host_actor_presentation[0].state[0] = 65u;
    host_apply_presentation_state(0u, 3050u, TRUE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_MOVING,
        "Tal combat locomotion maps to semantic movement");
    host_actor_presentation[0].selector[0] = TAL_COMBAT_ENTRY_SELECTOR;
    host_actor_presentation[0].state[0] = 1u;
    host_apply_presentation_state(0u, 3100u, TRUE, &tal);
    check(tal.animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
          tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_NONE,
        "Tal draw-weapon transition is not mislabeled as a weak attack");
    host_actor_presentation[0].selector[0] = 50;
    host_actor_presentation[0].state[0] = 65u;
    host_apply_presentation_state(0u, 3350u, TRUE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
          tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE &&
          tal.action_sequence == 1u && tal.action_phase_valid == 1u &&
          tal.action_phase_q8 == 0u,
        "Tal first native weak variant is transmitted semantically");
    host_actor_presentation[0].state[0] = 1u;
    host_actor_presentation[0].time[0] = 17.5f;
    host_apply_presentation_state(0u, 3360u, TRUE, &tal);
    check(tal.action_sequence == 1u && tal.action_phase_valid == 1u &&
          tal.action_phase_q8 == 17u * 256u + 128u,
        "Tal internal clip state cycling preserves one action and host phase");
    host_actor_presentation[0].selector[0] = 51;
    host_actor_presentation[0].state[0] = 1u;
    host_apply_presentation_state(0u, 3375u, TRUE, &tal);
    check(tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO &&
          tal.action_sequence == 2u,
        "Tal second native weak variant remains distinct");
    host_actor_presentation[0].time[0] = 49.5f;
    host_apply_presentation_state(0u, 3390u, TRUE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
          tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO &&
          tal.action_sequence == 2u,
        "Tal terminal action remains sequenced before retirement");
    {
        SudekiMpCleanroomActorPresentation terminal =
            host_actor_presentation[0];
        SudekiMpCleanroomActorPresentation idle;
        terminal.time[0] = 49.5f;
        memset(&idle, 0, sizeof(idle));
        idle.selector[0] = TAL_COMBAT_IDLE_SELECTOR;
        idle.state[0] = 0u;
        idle.time[0] = 2.25f;
        host_capture_actor_action_retirement(0u, &terminal, &idle);
        host_actor_presentation[0] = idle;
    }
    host_apply_presentation_state(0u, 3400u, TRUE, &tal);
    check(tal.animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
          tal.action_sequence == 2u && tal.action_phase_valid == 0u &&
          tal.action_phase_q8 == 0u &&
          tal.action_retirement_valid == 1u &&
          tal.action_terminal_phase_q8 == 49u * 256u + 128u &&
          tal.idle_entry_phase_q8 == 2u * 256u + 64u,
        "Tal combat action retirement carries terminal and idle clocks");
    host_apply_presentation_state(0u, 3410u, TRUE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_IDLE &&
          tal.action_sequence == 2u &&
          tal.action_retirement_valid == 1u &&
          tal.action_terminal_phase_q8 == 49u * 256u + 128u &&
          tal.idle_entry_phase_q8 == 2u * 256u + 64u,
        "Tal retirement handoff remains latched throughout idle");
    host_actor_presentation[0].selector[0] = 52;
    host_actor_presentation[0].state[0] = 1u;
    host_apply_presentation_state(0u, 3425u, TRUE, &tal);
    check(tal.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
          tal.combat_state == SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK &&
          tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_STRONG &&
          tal.action_sequence == 3u,
        "Tal native strong attack is transmitted semantically");
    host_actor_presentation[0].selector[0] = 53;
    host_apply_presentation_state(0u, 3435u, TRUE, &tal);
    check(tal.combat_state == SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK &&
          tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_STRONG_TWO &&
          tal.action_sequence == 4u,
        "Tal native second-stage strong attack remains distinct");
    host_actor_presentation[0].selector[0] = 54;
    host_actor_presentation[0].state[0] = 65u;
    host_apply_presentation_state(0u, 3440u, TRUE, &tal);
    check(tal.combat_state == SUDEKIMP_LAN_ARENA_COMBAT_STRONG_ATTACK &&
          tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS &&
          tal.action_sequence == 5u,
        "Tal WWS heavy finisher is transmitted by exact combo identity");
    host_actor_presentation[0].selector[0] = 71;
    host_actor_presentation[0].state[0] = 1u;
    host_actor_presentation[0].state[0] = 1u;
    host_apply_presentation_state(0u, 3450u, TRUE, &tal);
    check(tal.combat_state == SUDEKIMP_LAN_ARENA_COMBAT_SWEEP_ATTACK &&
          tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_SWEEP &&
          tal.action_sequence == 6u,
        "Tal native sweep attack is transmitted semantically");
    host_actor_presentation[0].selector[0] = 20;
    host_actor_presentation[0].state[0] = 65u;
    host_apply_presentation_state(0u, 3475u, TRUE, &tal);
    check(tal.combat_state == SUDEKIMP_LAN_ARENA_COMBAT_BLOCK &&
          tal.action_variant == SUDEKIMP_LAN_ARENA_ACTION_BLOCK &&
          tal.action_sequence == 7u,
        "Tal native block entry is transmitted semantically");
    host_actor_presentation[0].selector[0] = 21;
    host_actor_presentation[0].state[0] = 128u;
    host_apply_presentation_state(0u, 3490u, TRUE, &tal);
    check(tal.combat_state == SUDEKIMP_LAN_ARENA_COMBAT_BLOCK &&
          tal.action_sequence == 7u,
        "Tal native block hold preserves the same semantic action edge");

    host_actor_presentation_valid[1] = TRUE;
    host_actor_presentation[1].selector[4] = AILISH_COMBAT_WEAK_SELECTOR;
    host_actor_presentation[1].state[4] = 1u;
    host_actor_presentation[1].time[4] = 9.25f;
    host_apply_presentation_state(1u, 3500u, TRUE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
          ailish.combat_state == SUDEKIMP_LAN_ARENA_COMBAT_WEAK_ATTACK &&
          ailish.action_variant == SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE &&
          ailish.action_sequence == 1u && ailish.action_phase_valid == 1u &&
          ailish.action_phase_q8 == 9u * 256u + 64u,
        "Ailish combat shot remains active for its native clip");
    host_actor_presentation[1].state[4] = 65u;
    host_apply_presentation_state(1u, 3800u, TRUE, &ailish);
    check(ailish.animation_state == SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
          ailish.action_sequence == 1u,
        "Ailish combat shot survives beyond the old 250ms pulse");
    host_actor_presentation[1].selector[4] = 0;
    host_actor_presentation[1].state[4] = 192u;
    host_apply_presentation_state(1u, 3850u, TRUE, &ailish);
    check(ailish.animation_state != SUDEKIMP_LAN_ARENA_ANIMATION_ACTION &&
          ailish.action_sequence == 1u,
        "Ailish combat shot retires with the native action layer");

    {
        uint8_t saved_type = runtime_config.client_actor_type;
        runtime_config.client_actor_type = SUDEKIMP_LAN_ARENA_ELCO_TYPE;
        host_actor_presentation[1].selector[4] = 53;
        host_actor_presentation[1].state[4] = 0u;
        check(host_actor_native_action_variant(1u, TRUE) ==
                SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE,
            "Elco firing blend entry uses Elco world selector, not Ailish");
        host_actor_presentation[1].state[4] = 192u;
        check(host_actor_native_action_variant(1u, TRUE) ==
                SUDEKIMP_LAN_ARENA_ACTION_NONE,
            "Elco inactive firing layer retires");
        host_actor_presentation[1].selector[4] = 59;
        host_actor_presentation[1].state[4] = 1u;
        check(host_actor_native_action_variant(1u, TRUE) ==
                SUDEKIMP_LAN_ARENA_ACTION_NONE,
            "Elco never classifies Ailish's selector as a shot");
        runtime_config.client_actor_type = saved_type;
        host_actor_presentation[1].selector[4] = 0;
        host_actor_presentation[1].state[4] = 192u;
    }

    reset_host_action_tracking();
    memset(&ailish, 0, sizeof(ailish));
    host_track_actor_action_sequence(
        1u, 4000u, TRUE, FALSE,
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE, 0u, &ailish);
    host_track_actor_action_sequence(
        1u, 4050u, TRUE, FALSE,
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE, 0u, &ailish);
    host_track_actor_action_sequence(
        1u, 4100u, TRUE, FALSE,
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE, 0u, &ailish);
    check(host_actor_action_sequence[1] == 1u &&
          host_actor_action_history_count[1] == 1u,
        "one fallback ranged pulse journals one Ailish action edge");
    host_track_actor_action_sequence(
        1u, 4300u, FALSE, FALSE,
        SUDEKIMP_LAN_ARENA_ACTION_NONE, 0u, &ailish);
    host_track_actor_action_sequence(
        1u, 4350u, TRUE, FALSE,
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE, 0u, &ailish);
    check(host_actor_action_sequence[1] == 2u &&
          host_actor_action_history_count[1] == 2u,
        "a later fallback ranged pulse journals exactly one new edge");

    check(SudekiMpLanArenaRangedRepeatReady(5000u, 0u) &&
          !SudekiMpLanArenaRangedRepeatReady(5499u, 5500u) &&
          SudekiMpLanArenaRangedRepeatReady(5500u, 5500u) &&
          !SudekiMpLanArenaRangedRepeatReady(0xfffffff0u, 0x00000010u) &&
          SudekiMpLanArenaRangedRepeatReady(0x00000010u, 0x00000010u),
        "Ailish ranged repeat cadence is wrap-safe and inclusive");
    check(SudekiMpLanArenaRangedRepeatIntervalMs(0x4000u) == 2000u &&
          SudekiMpLanArenaRangedRepeatIntervalMs(0x3c00u) == 1000u &&
          SudekiMpLanArenaRangedRepeatIntervalMs(0x0000u) == 2000u &&
          SudekiMpLanArenaRangedRepeatIntervalMs(0x7c00u) == 2000u &&
          SudekiMpLanArenaRangedRepeatIntervalMs(0xbc00u) == 2000u,
        "Ailish ranged cadence decodes safe authored half-float seconds");
}

static void verify_host_anim_id_read(void) {
    /* Synthetic CNewGameModelAnimation chain laid out as the game does:
     * character+0x130 -> model (the uniform offset), model+0x10 -> character
     * backpointer, model+0x131 -> mode flag, model+0xF8 -> per-channel state
     * array (4-byte records, byte +2 = current ANIMID). */
    static uint8_t character_bytes[0x200];
    static uint8_t model_bytes[0x200];
    static uint8_t state_bytes[32u];
    uint8_t *character = character_bytes;
    uint8_t *model = model_bytes;
    uint8_t *state = state_bytes;
    SudekiMpLanArenaActorSnapshot tal;

    memset(character, 0, sizeof(character_bytes));
    memset(model, 0, sizeof(model_bytes));
    memset(state, 0, sizeof(state_bytes));
    memset(&tal, 0, sizeof(tal));
    tal.hp = 100u;
    runtime_config.host_actor_type = 0u; /* default -> Tal host seat */

    *(uint8_t **)(character + 0x130u) = model;
    *(uint8_t **)(model + 0x10u) = character;
    model[0x131u] = 3u;
    *(uint8_t **)(model + 0xF8u) = state;
    state[2u] = 0x72u; /* ANIMID_ATTACK_WEAK */

    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = character;
    host_apply_presentation_state(0u, 100u, FALSE, &tal);
    check(tal.anim_id == 0x72u,
        "host reads the model's current ANIMID (0x72 ATTACK_WEAK)");

    model[0x131u] = 0u; /* mode flag is NOT a gate: channel 0 is still read */
    host_apply_presentation_state(0u, 100u, FALSE, &tal);
    check(tal.anim_id == 0x72u,
        "0x131 mode flag is ignored; channel 0 ANIMID still reported");
    model[0x131u] = 3u;

    state[2u] = 0xC4u; /* first out-of-range id */
    host_apply_presentation_state(0u, 100u, FALSE, &tal);
    check(tal.anim_id == 0u, "out-of-range ANIMID yields UNKNOWN");
    state[2u] = 0x72u;

    *(uint8_t **)(model + 0x10u) = NULL; /* backpointer mismatch */
    host_apply_presentation_state(0u, 100u, FALSE, &tal);
    check(tal.anim_id == 0u,
        "model/character backpointer mismatch yields UNKNOWN");
    *(uint8_t **)(model + 0x10u) = character;

    *(uint8_t **)(model + 0xF8u) = NULL; /* unreadable state array */
    host_apply_presentation_state(0u, 100u, FALSE, &tal);
    check(tal.anim_id == 0u, "unreadable state array yields UNKNOWN");

    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL] = NULL;
}

static void verify_weapon_snapshot_family(void) {
    static const SudekiMpCleanroomActor actors[] = {
        SUDEKIMP_CLEANROOM_TAL, SUDEKIMP_CLEANROOM_AILISH,
        SUDEKIMP_CLEANROOM_BUKI, SUDEKIMP_CLEANROOM_ELCO
    };
    static const uint8_t types[] = {0x23u, 0x01u, 0x05u, 0x0eu};
    unsigned int i;
    SudekiMpLanArenaActorSnapshot snapshot;
    actor_position_result = actor_facing_result = actor_resources_result = TRUE;
    describe_equipped_weapon = TRUE;
    for (i = 0u; i < 4u; ++i) {
        check(fill_actor_snapshot(actors[i], types[i], &snapshot),
            "equipped actor snapshot is captured for every hero");
        check(snapshot.weapon_slot_plus_one == 1u,
            "LA27 publishes an equipped own-family slot for every hero");
    }
    describe_equipped_weapon = FALSE;
}

static void verify_persistent_skill_ui_lifecycle(uint8_t *image) {
    static unsigned char actors[2][0x138],skills[2][0x78];
    unsigned int direction;
    for(direction=0;direction<2;++direction) {
        reset_stub_policy();
        reset_stub_counts();
        runtime_config=make_config(direction ? SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH :
            SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL);
        runtime_config.host_actor_type=SUDEKIMP_LAN_ARENA_BUKI_TYPE;
        runtime_config.client_actor_type=SUDEKIMP_LAN_ARENA_ELCO_TYPE;
        runtime_game_module=(HMODULE)image;
        runtime_installed=tal_initialized=ailish_initialized=TRUE;
        host_remote_ailish_owned=!direction;
        client_remote_tal_owned=direction;
        cleanroom_actor_entities[SUDEKIMP_CLEANROOM_BUKI]=&actors[0];
        cleanroom_actor_entities[SUDEKIMP_CLEANROOM_ELCO]=&actors[1];
        session_status_result=TRUE;
        session_status.session_token=701u;
        session_status.peer_connected=TRUE;
        session_status.local_simulation_node_role=runtime_config.local_simulation_node_role;
        session_status.peer_simulation_node_role=direction ?
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD :
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA;
        cast_context_drained=TRUE;
        character_skill_observe_result=FALSE;
        check(service_runtime_skill_ui() && !runtime_skill_ui_initialized,
            "unknown skill observation cannot install persistent UI isolation");
        character_skill_observe_result=TRUE;
        character_skill_observation.active=TRUE;
        check(service_runtime_skill_ui() && !runtime_skill_ui_initialized,
            "active cast cannot bind persistent UI isolation mid-acquisition");
        character_skill_observation.active=FALSE;
        check(service_runtime_skill_ui() && runtime_skill_ui_bound &&
              ui_abi_init_calls==1u && ui_abi_bind_calls==1u &&
              ui_abi_local==&actors[direction] && ui_abi_remote==&actors[1u-direction] &&
              ui_abi_type==(direction ? SUDEKIMP_LAN_ARENA_BUKI_TYPE : SUDEKIMP_LAN_ARENA_ELCO_TYPE) &&
              ui_abi_session==701u,
            "host and client bind only their assigned remote skill UI receiver");
        check(service_runtime_skill_ui() && ui_abi_bind_calls==1u,
            "ordinary frames do not rebind native UI ownership");
        if(!direction) {
            void *manager=NULL;
            int state=-1,id=-1;
            uint32_t cookie;
            check(fake_spirit_enter && fake_owned_skill_enter && fake_owned_task_enter &&
                fake_spirit_observer && runtime_spirit_instances[0].generation &&
                runtime_spirit_instances[1].generation &&
                runtime_spirit_instances[0].manager!=runtime_spirit_instances[1].manager,
                "host installs both persistent native lifetimes and every entry/task/observation route");
            check(runtime_spirit_named_cameras,"camera experiment binds after both actor namespaces exist");
            check(runtime_spirit_lighting,"lighting experiment binds after private cameras and actor leases");
            host_remote_skill_camera_active=TRUE;
            fake_camera_scope=(SudekiMpSpiritInstance){0};
            check(SudekiMpLanArenaRouteCastCamera(NULL,"default")==2,
                "positively neutral camera call is not suppressed by a live remote cast");
            fake_camera_scope=runtime_spirit_instances[0];
            check(SudekiMpLanArenaRouteCastCamera(NULL,"SkillCam")==2,
                "local cast keeps native camera selection while peer cast is alive");
            fake_camera_scope=runtime_spirit_instances[1];
            check(SudekiMpLanArenaRouteCastCamera(NULL,"SkillCam")==1 && host_spirit_camera_kind==2,
                "remote scheduled/VM camera request uses its owned selection and publication kind");
            fake_camera_route_result=-1;
            check(SudekiMpLanArenaRouteCastCamera(NULL,"SpiritCam")==-1,
                "unknown remote cinematic camera does not fall back to local rendering");
            fake_camera_route_result=1; fake_camera_scope_known=FALSE;
            check(SudekiMpLanArenaRouteCastCamera(NULL,"default")==-1,"unknown scope cannot steal a camera");
            fake_camera_scope_known=TRUE;
            ++fake_camera_scope.generation;
            check(SudekiMpLanArenaRouteCastCamera(NULL,"default")==-1,"replaced camera generation rejected");
            fake_camera_scope=(SudekiMpSpiritInstance){0}; host_remote_skill_camera_active=FALSE;
            cookie=fake_spirit_enter(NULL,4,TRUE,&manager);
            check(cookie && manager==runtime_spirit_instances[0].manager,
                "native Q selects the assigned local caster manager");
            cookie=fake_spirit_enter(ui_abi_remote,6,TRUE,&manager);
            check(cookie && manager==runtime_spirit_instances[1].manager,
                "explicit client request selects the retained remote caster manager");
            check(!fake_spirit_enter(NULL,6,TRUE,&manager) &&
                !fake_spirit_enter((void *)1,4,TRUE,&manager),
                "strike ID cannot grant another actor or foreign pointer authority");
            *(void **)(skills[1]+0x10)=ui_abi_remote;
            character_skill_observation.skill=skills[1];
            reset_host_skill_tracking();
            fake_timing_begin_calls=0;
            check(fake_owned_skill_enter(skills[1],0,FALSE)!=0 && !fake_timing_begin_calls,
                "menu validation cannot prepare or publish a new timing transaction");
            check(fake_owned_skill_enter(skills[1],0,TRUE)!=0,
                "ordinary skill uses the same retained caster namespace");
            check(fake_timing_begin_calls==1 && fake_timing_sequence==1 &&
                fake_timing_instance.generation==runtime_spirit_instances[1].generation &&
                !host_actor_skill_sequence[1] && !host_native_skill_leases[1].pending,
                "before native Use, prepare remote timing without claiming success or publishing a cast");
            check(fake_owned_skill_enter(skills[1],0,TRUE)!=0 && fake_timing_sequence==1 &&
                !host_actor_skill_sequence[1],"rejected Use can retry without consuming the wire sequence");
            check(host_begin_character_skill_sequence(1,ui_abi_remote,skills[1],0,0,FALSE,"test") &&
                fake_timing_sequence==1 && host_actor_skill_sequence[1]==1 &&
                host_native_skill_leases[1].pending,
                "successful Use adopts the prepared timing ID exactly once");
            fake_timing_begin_result=FALSE;
            check(!fake_owned_skill_enter(skills[1],0,TRUE) && host_actor_skill_sequence[1]==1,
                "failed timing admission rejects Use without advancing sequence");
            check(!host_begin_character_skill_sequence(1,ui_abi_remote,skills[1],0,0,FALSE,"test") &&
                host_actor_skill_sequence[1]==1,
                "failed timing adoption cannot publish a replacement sequence");
            fake_timing_begin_result=TRUE;
            host_actor_skill_sequence[1]=UINT16_MAX;
            check(fake_owned_skill_enter(skills[1],0,TRUE)!=0 && fake_timing_sequence==1 &&
                host_actor_skill_sequence[1]==UINT16_MAX,"preparation handles wrap without consuming it");
            reset_host_skill_tracking();
            fake_spirit_states[1]=(SudekiMpSpiritInstanceState){10,6,TRUE,FALSE};
            check(SudekiMpCleanroomEngineSpiritPresentationState(&state) && state==10 &&
                SudekiMpCleanroomEngineSpiritStrikeId(&id) && id==6,
                "snapshot observers see active owned Elco manager, not inactive retail singleton");
            check(!fake_spirit_enter(NULL,4,TRUE,&manager) &&
                fake_spirit_enter(NULL,4,FALSE,&manager)!=0 &&
                !fake_owned_skill_enter(skills[1],0,TRUE),
                "menu eligibility can inspect local namespace but native overlap admission remains guarded");
            fake_spirit_states[0]=(SudekiMpSpiritInstanceState){10,4,TRUE,FALSE};
            state=77;
            check(!SudekiMpCleanroomEngineSpiritPresentationState(&state) && state==77,
                "two live managers cannot silently collapse into one serialized snapshot");
            {
                SudekiMpLanArenaSnapshot paired={0};
                uint64_t token=0; uint16_t sequence=0; uint8_t owner=9;
                paired.seat[0].actor_type=SUDEKIMP_LAN_ARENA_BUKI_TYPE;
                paired.seat[1].actor_type=SUDEKIMP_LAN_ARENA_ELCO_TYPE;
                reset_host_skill_tracking();
                check(host_apply_spirit_state(&paired) && paired.seat[0].skill_active &&
                    paired.seat[1].skill_active && paired.seat[0].skill_sequence==1 &&
                    paired.seat[1].skill_sequence==1,
                    "multi-cast publisher independently observes both native managers with equal actor-local sequences");
                fake_camera_scope=runtime_spirit_instances[0];
                check(host_spirit_audio_active_witness(&runtime_config,&state,&token,&sequence,&owner) &&
                    token==701 && sequence==1 && owner==0,
                    "audio emission binds the scoped host caster rather than latest global owner");
                fake_camera_scope=runtime_spirit_instances[1];
                check(host_spirit_audio_active_witness(&runtime_config,&state,&token,&sequence,&owner) &&
                    sequence==1 && owner==1,"audio emission distinguishes equal client cast sequence");
                ++fake_camera_scope.generation;
                check(!host_spirit_audio_active_witness(&runtime_config,&state,&token,&sequence,&owner),
                    "replaced native generation cannot emit into a live caster's journal");
                fake_camera_scope=runtime_spirit_instances[1];
                ++session_status.session_token;
                check(!host_spirit_audio_active_witness(&runtime_config,&state,&token,&sequence,&owner),
                    "replaced session cannot reuse the retained native event owner");
                --session_status.session_token;
                fake_camera_scope=(SudekiMpSpiritInstance){0};
                check(!host_spirit_audio_active_witness(&runtime_config,&state,&token,&sequence,&owner),
                    "neutral native emissions never borrow an active caster");
                {
                    uint8_t components[2][0x14]={{0}};
                    void *actors[2]={ui_abi_local,ui_abi_remote};
                    void *saved_components[2];
                    uint32_t tick=0;
                    for(unsigned int i=0;i<2;++i) {
                        saved_components[i]=*(void **)((uint8_t *)actors[i]+0x58u);
                        *(void **)((uint8_t *)actors[i]+0x58u)=components[i];
                        *(void **)(components[i]+0x10u)=actors[i];
                    }
                    check(host_spirit_visual_active_witness(&runtime_config,components[0],
                        &token,&sequence,&tick,&owner) && token==701 && sequence==1 && owner==0,
                        "unscoped animation emission uses exact host component ownership");
                    check(host_spirit_visual_active_witness(&runtime_config,components[1],
                        &token,&sequence,&tick,&owner) && sequence==1 && owner==SUDEKIMP_LAN_ARENA_ELCO_TYPE,
                        "equal-sequence Elco animation emission cannot be attributed to Buki");
                    *(void **)(components[1]+0x10u)=actors[0];
                    fake_camera_scope=runtime_spirit_instances[0];
                    check(!host_spirit_visual_active_witness(&runtime_config,components[1],
                        &token,&sequence,&tick,&owner),"foreign animation component cannot borrow current caster scope");
                    *(void **)(components[1]+0x10u)=actors[1];
                    ++session_status.session_token;
                    check(!host_spirit_visual_active_witness(&runtime_config,components[1],
                        &token,&sequence,&tick,&owner),"source attribution rejects a replaced session");
                    --session_status.session_token;
                    ++runtime_spirit_instances[1].generation;
                    check(!host_spirit_visual_active_witness(&runtime_config,components[1],
                        &token,&sequence,&tick,&owner),"source attribution rejects a replaced native manager generation");
                    --runtime_spirit_instances[1].generation;
                    for(unsigned int i=0;i<2;++i)
                        *(void **)((uint8_t *)actors[i]+0x58u)=saved_components[i];
                    fake_camera_scope=(SudekiMpSpiritInstance){0};
                }
                host_capture_skill_fade(&paired);
                check(paired.cast[0].skill_fade.kind==2 && paired.cast[0].skill_fade.owner_seat==0 &&
                    paired.cast[1].skill_fade.kind==2 && paired.cast[1].skill_fade.owner_seat==1,
                    "both private fade banks publish independently during overlapping lifetimes");
                fake_spirit_states[0]=(SudekiMpSpiritInstanceState){0,4,FALSE,TRUE};
                fake_spirit_states[1].strike_id=4u;
                check(!host_apply_spirit_state(&paired) && host_private_spirit_active[0] &&
                    host_private_spirit_active[1] && host_actor_skill_sequence[0]==1 &&
                    host_actor_skill_sequence[1]==1,
                    "wrong second manager identity cannot partially retire the first caster");
                fake_spirit_states[1].strike_id=6u;
                check(host_apply_spirit_state(&paired) && !paired.seat[0].skill_active && paired.seat[1].skill_active,
                    "one Spirit completion cannot retire the other caster");
                fake_spirit_states[1]=(SudekiMpSpiritInstanceState){0,6,FALSE,TRUE};
                check(host_apply_spirit_state(&paired) && !paired.seat[0].skill_active && !paired.seat[1].skill_active,
                    "both independent Spirit transactions finish without allocating extra sequences");
                reset_host_skill_tracking();
            }
            fake_spirit_states[0]=(SudekiMpSpiritInstanceState){0,4,FALSE,TRUE};
            fake_spirit_states[1]=(SudekiMpSpiritInstanceState){0,6,FALSE,TRUE};
            character_skill_observation.skill=NULL;
        } else {
            uint64_t session=0; uint8_t type=0;
            check(SudekiMpLanArenaClientPrivateCastCamerasOwned() && fake_owned_skill_enter &&
                fake_owned_task_enter && !fake_spirit_enter && !fake_spirit_observer &&
                !fake_owned_menu && runtime_spirit_instances[0].generation &&
                runtime_spirit_instances[1].generation,
                "client owns two presentation namespaces but no Spirit activation or observer");
            check(!runtime_cast_owner_witness(ui_abi_local,2,&session,&type),
                "replica cannot acquire a native Spirit gameplay lineage");
            fake_camera_scope=runtime_spirit_instances[0];
            check(SudekiMpLanArenaRouteCastCamera(NULL,"SkillCam")==2,
                "client's local namespace keeps Elco's native camera selection");
            fake_camera_scope=runtime_spirit_instances[1];
            check(SudekiMpLanArenaRouteCastCamera(NULL,"SkillCam")==1,
                "client remote Buki namespace does not change Elco's selected camera");
            fake_camera_scope=(SudekiMpSpiritInstance){0};
            for(unsigned int i=0;i<2u;++i) {
                void *actor=i ? ui_abi_remote:ui_abi_local;
                *(void **)(skills[i]+0x10)=actor;
                character_skill_observation.skill=skills[i];
                check(fake_owned_skill_enter(skills[i],2,FALSE)!=0 &&
                    !fake_owned_skill_enter(skills[i],2,TRUE),
                    "client can query a skill but cannot use it without host-approved replay");
                fake_replay_actor=actor; fake_replay_slot=2;
                check(fake_owned_skill_enter(skills[i],2,TRUE)!=0 &&
                    !fake_owned_skill_enter(skills[i],1,TRUE),
                    "client replay routes only the exact admitted actor and slot");
                fake_replay_actor=NULL;
            }
            character_skill_observation.skill=NULL;
            fake_client_replay_active=TRUE;
            fake_client_drain_calls=0;
            check(!release_runtime_skill_ui() && runtime_skill_ui_initialized &&
                runtime_spirit_instances[0].generation && runtime_spirit_instances[1].generation &&
                fake_client_drain_calls==1 && fake_client_replay_active,
                "native client replay lease retains both namespaces even before active observation");
            fake_client_replay_complete=TRUE;
            cast_context_drained=FALSE;
            check(!release_runtime_skill_ui() && !fake_client_replay_active &&
                fake_client_drain_calls==2 && runtime_skill_ui_initialized,
                "early teardown services terminal replay before snapshot reset, but retains pending native lineage");
            fake_client_replay_complete=FALSE;
            cast_context_drained=TRUE;
            /* Re-open only this fixture's retirement flag to continue its
             * disconnect tests; production teardown never revokes retirement. */
            runtime_skill_ui_retiring=FALSE;
        }
        /* Actor-specific ordinary admission must not turn the same native
         * namespaces into Spirit overlap or weaken all-idle teardown. */
        check(SudekiMpLanArenaOrdinarySkillOverlapOwned(),"closed ordinary overlap plumbing is owned");
        for(unsigned int i=0;i<2u;++i) {
            paired_skill_actors[i]=i ? ui_abi_remote:ui_abi_local;
            *(void **)(skills[i]+0x10)=paired_skill_actors[i];
            paired_skill_observations[i]=(SudekiMpCharacterSkillState){0};
            paired_skill_observations[i].skill=skills[i];
        }
        for(unsigned int incoming=0;incoming<2u;++incoming) {
            unsigned int peer=incoming^1u;
            paired_skill_observations[peer].active=TRUE;
            fake_spirit_states[peer].idle=FALSE; /* Ordinary UI/task obligation. */
            cast_context_drained=FALSE; cast_context_actor_busy=paired_skill_actors[peer];
            fake_replay_actor=paired_skill_actors[incoming]; fake_replay_slot=2;
            check(!runtime_skill_ui_idle(),"live peer still blocks global teardown");
            check(fake_owned_skill_enter(skills[incoming],2,TRUE)!=0,
                "ready caster starts ordinary skill while the peer has an ordinary task, either process/order");
            runtime_spirit_lighting=FALSE;
            check(!fake_owned_skill_enter(skills[incoming],2,TRUE),
                "incomplete isolation preserves serialized ordinary admission");
            runtime_spirit_lighting=TRUE;
            runtime_skill_targeting=FALSE;
            check(!fake_owned_skill_enter(skills[incoming],2,TRUE),
                "missing targeting isolation preserves serialized admission");
            runtime_skill_targeting=TRUE;
            cast_context_actor_busy=paired_skill_actors[incoming];
            check(!fake_owned_skill_enter(skills[incoming],2,TRUE),"same actor's retained child blocks repeat cast");
            cast_context_actor_busy=paired_skill_actors[peer]; cast_context_actor_known=FALSE;
            check(!fake_owned_skill_enter(skills[incoming],2,TRUE),"unknown lineage does not grant overlap");
            cast_context_actor_known=TRUE;
            paired_skill_observations[incoming].active=TRUE;
            check(!fake_owned_skill_enter(skills[incoming],2,TRUE),"active caster cannot start a second skill");
            paired_skill_observations[incoming].active=FALSE;
            fake_spirit_states[peer].state=10;
            check(!fake_owned_skill_enter(skills[incoming],2,TRUE),"ordinary skill plus Spirit remains guarded");
            fake_spirit_states[peer].state=0;
            if(!direction) {
                void *manager=NULL;
                check(!fake_spirit_enter(NULL,4,TRUE,&manager),"Spirit plus ordinary skill remains guarded");
                check(capture_host_tal_skill_view(2) && !host_tal_skill_view_lease.valid,
                    "overlap profile cannot capture a cast-long peer view");
            } else {
                fake_replay_actor=NULL;
                check(!fake_owned_skill_enter(skills[incoming],2,TRUE),"overlap never grants client authority");
            }
            ++session_status.session_token;
            check(!fake_owned_skill_enter(skills[incoming],2,TRUE),"stale session rejects overlapping start");
            --session_status.session_token;
            paired_skill_observations[peer].active=FALSE; fake_spirit_states[peer].idle=TRUE;
        }
        memset(paired_skill_actors,0,sizeof(paired_skill_actors));
        cast_context_drained=TRUE; cast_context_actor_busy=NULL; fake_replay_actor=NULL;
        check((direction ? release_host_remote_ailish("client_role"):
                release_client_remote_tal("host_role")) && runtime_skill_ui_bound &&
                ui_abi_reset_calls==0u,
            "opposite-role no-op cleanup cannot unbind the live UI owner");
        cast_context_current=TRUE;
        cast_context_owner.actor=ui_abi_remote;
        cast_context_owner.session=701u;
        cast_context_owner.kind=1u;
        check(runtime_skill_ui_task(ui_abi_remote,701u),
            "retained remote CSkill lineage admits its scripted UI cleanup");
        cast_context_owner.kind=2u;
        check(!runtime_skill_ui_task(ui_abi_remote,701u),
            "Spirit lineage cannot borrow the CSkill UI namespace");
        cast_context_current=FALSE;
        session_status.peer_connected=FALSE;
        cast_context_drained=FALSE;
        if(!direction) {
            SudekiMpLanCastOwner retained={0};
            void *manager=NULL;
            retained.actor=ui_abi_remote; retained.session=701; retained.kind=2; retained.cast_id=3;
            check(!fake_spirit_enter(ui_abi_remote,6,TRUE,&manager) && fake_owned_task_enter(&retained),
                "disconnect closes activation but preserves the old Spirit task cleanup owner");
            retained.session=702;
            check(!fake_owned_task_enter(&retained),"new session cannot borrow retained task namespace");
            check(fake_owned_task_enter(NULL)!=0,"unrelated task routes to neutral native context");
        } else {
            SudekiMpLanCastOwner retained={0};
            retained.actor=ui_abi_remote; retained.session=701; retained.kind=1; retained.cast_id=3;
            check(fake_owned_task_enter(&retained)!=0,
                "disconnected client retains ordinary skill cleanup namespace");
            retained.kind=2;
            check(!fake_owned_task_enter(&retained),"client cleanup cannot borrow native Spirit authority");
        }
        check(runtime_skill_ui_retained(ui_abi_remote,701u) &&
              !runtime_skill_ui_retained(ui_abi_remote,702u) &&
              !service_runtime_skill_ui() && runtime_skill_ui_bound && ui_abi_reset_calls==0u,
            "disconnect retains old-session cleanup without admitting a replacement session");
        cast_context_drained=TRUE;
        ui_abi_reset_result=FALSE;
        check(!service_runtime_skill_ui() && runtime_skill_ui_bound && runtime_skill_ui_initialized,
            "failed native restoration retains UI callbacks and actor witnesses");
        ui_abi_reset_result=TRUE;
        check(service_runtime_skill_ui() && !runtime_skill_ui_bound && !runtime_skill_ui_initialized &&
              !runtime_skill_ui_remote && !runtime_skill_ui_session,
            "disconnect releases persistent UI only after native drain and restoration");
        if(!direction) check(!fake_spirit_enter && !fake_owned_skill_enter && !fake_owned_task_enter &&
            !fake_spirit_observer && !fake_owned_menu && fake_destroy_calls>=2 && !runtime_spirit_named_cameras,
            "successful host drain restores routes/menu and destroys both native lifetimes");
        else check(!fake_owned_skill_enter && !fake_owned_task_enter &&
            !runtime_spirit_named_cameras && !SudekiMpLanArenaClientPrivateCastCamerasOwned() &&
            !runtime_spirit_instances[0].generation && !runtime_spirit_instances[1].generation,
            "successful client drain restores routes before releasing retained actor witnesses");
        session_status.peer_connected=TRUE;
        session_status.session_token=702u;
        ui_abi_bind_result=FALSE;
        check(!service_runtime_skill_ui() && runtime_skill_ui_initialized && !runtime_skill_ui_bound,
            "failed binding retains partial installed hooks for safe cleanup");
        ui_abi_bind_result=TRUE;
        check(service_runtime_skill_ui() && runtime_skill_ui_bound && ui_abi_session==702u,
            "retry restores partial hook owner before fresh-session binding");
        cleanroom_actor_entities[SUDEKIMP_CLEANROOM_ELCO]=NULL;
        check(!runtime_skill_ui_retained(ui_abi_remote,702u),
            "actor replacement invalidates retained UI witness");
        cleanroom_actor_entities[SUDEKIMP_CLEANROOM_ELCO]=&actors[1];
        ui_abi_healthy=FALSE;
        check(!service_runtime_skill_ui() && runtime_skill_ui_bound,
            "faulted native UI owner prevents continued session servicing");
        ui_abi_healthy=TRUE;
        check(release_runtime_skill_ui(),"drained UI owner can release before actor removal");
        runtime_installed=tal_initialized=ailish_initialized=FALSE;
        host_remote_ailish_owned=client_remote_tal_owned=FALSE;
        runtime_game_module=NULL;
    }
    reset_stub_policy();
    reset_stub_counts();
}

static void verify_legacy_client_skill_ui(uint8_t *image) {
    int actors[2]={0};
    reset_stub_policy();
    reset_stub_counts();
    runtime_config=make_config(SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH);
    runtime_config.host_actor_type=SUDEKIMP_LAN_ARENA_TAL_TYPE;
    runtime_config.client_actor_type=SUDEKIMP_LAN_ARENA_AILISH_TYPE;
    runtime_game_module=(HMODULE)image;
    runtime_installed=tal_initialized=ailish_initialized=TRUE;
    client_remote_tal_owned=TRUE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL]=&actors[0];
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH]=&actors[1];
    session_status_result=TRUE;
    session_status.session_token=703u;
    session_status.peer_connected=TRUE;
    session_status.local_simulation_node_role=runtime_config.local_simulation_node_role;
    session_status.peer_simulation_node_role=SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
    cast_context_drained=TRUE;
    character_skill_observe_result=TRUE;
    character_skill_observation.active=FALSE;
    character_skill_observation.skill=NULL;
    check(service_runtime_skill_ui() && runtime_skill_ui_bound &&
        ui_abi_local==&actors[1] && ui_abi_remote==&actors[0] &&
        ui_abi_type==SUDEKIMP_LAN_ARENA_TAL_TYPE && ui_abi_session==703u &&
        !runtime_spirit_instances[0].generation && !runtime_spirit_instances[1].generation &&
        !fake_owned_skill_enter && !fake_owned_task_enter &&
        !SudekiMpLanArenaClientPrivateCastCamerasOwned() &&
        SudekiMpLanArenaRouteCastCamera(NULL,"SkillCam")==0,
        "original Tal/Ailish client retains its legacy binding without private probe cameras");
    session_status.peer_connected=FALSE;
    check(service_runtime_skill_ui() && !runtime_skill_ui_initialized,
        "original client binding still drains and restores on disconnect");
    client_remote_tal_owned=FALSE;
    runtime_installed=tal_initialized=ailish_initialized=FALSE;
    runtime_game_module=NULL;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL]=NULL;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_AILISH]=NULL;
    reset_stub_policy();
    reset_stub_counts();
}

static void verify_remote_ranged_aim(void) {
    int actors[2]={0}; float direction[3];
    reset_stub_policy(); reset_stub_counts();
    runtime_config=make_config(SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL);
    runtime_config.host_actor_type=SUDEKIMP_LAN_ARENA_BUKI_TYPE;
    runtime_config.client_actor_type=SUDEKIMP_LAN_ARENA_ELCO_TYPE;
    runtime_installed=tal_initialized=ailish_initialized=TRUE;
    cleanroom_combat_enabled=TRUE;
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_BUKI]=&actors[0];
    cleanroom_actor_entities[SUDEKIMP_CLEANROOM_ELCO]=&actors[1];
    session_status_result=TRUE;
    session_status.local_role=runtime_config.local_role;
    session_status.local_simulation_node_role=runtime_config.local_simulation_node_role;
    session_status.peer_connected=TRUE; session_status.session_token=888;
    character_skill_observation.active=FALSE;
    memset(runtime_spirit_instances,0,sizeof(runtime_spirit_instances));
    host_remote_ailish_owned=host_remote_first_person_active=TRUE;
    host_remote_aim_x=0; host_remote_aim_y=19660; host_remote_aim_z=26214;
    host_last_remote_input_at_ms=GetTickCount();
    ranged_remote_lease=&actors[1];
    ranged_aim_fixture=TRUE;
    reset_host_action_tracking();
    elco_weapon_fixture_owner=&actors[1];
    elco_weapon_fixture=(SudekiMpElcoWeaponObservation){34,0,100,50,0};
    for (unsigned i=0;i<4;++i) runtime_weapon_shot(&actors[1]);
    check(host_weapon_journal[1].shot_count==3 &&
        host_weapon_journal[1].shots[0].sequence==2 &&
        host_weapon_journal[1].shots[2].sequence==4 &&
        host_weapon_journal[1].shots[2].pre_charge_q8==25600,
        "actual native projectile observer journals bounded host-confirmed pre-shot resources");
    elco_weapon_fixture.charge=0;
    runtime_weapon_shot(&actors[1]);
    check(host_weapon_sequences[1]==4,"empty-charge callback is not an executed gun shot");
    elco_weapon_fixture.charge=100; character_skill_observation.active=TRUE;
    runtime_weapon_shot(&actors[1]);
    check(host_weapon_sequences[1]==4,"skill projectile cannot become gun feedback");
    character_skill_observation.active=FALSE; ranged_remote_lease=NULL;
    runtime_weapon_shot(&actors[1]);
    check(host_weapon_sequences[1]==4,"lost remote actor lease rejects shot journal entry");
    ranged_remote_lease=&actors[1]; session_status.peer_connected=FALSE;
    runtime_weapon_shot(&actors[1]);
    check(host_weapon_sequences[1]==4,"disconnect cannot publish weapon shots");
    session_status.peer_connected=TRUE;
    reset_host_action_tracking(); elco_weapon_fixture_owner=NULL;
    check(!SudekiMpControlSeparationSeatInputLeaseActive(1),
        "fixture has no local controller bridge for LAN actor");
    check(runtime_ranged_aim(&actors[1],TRUE,direction) && direction[1]>.59f && direction[2]>.79f,
        "authenticated remote projectile receives vertical aim without local-controller bridge");
    check(runtime_ranged_aim(&actors[1],FALSE,direction),
        "same LAN aim reaches observer-pose publication");
    BOOL held=FALSE;host_remote_weak_held=TRUE;
    check(runtime_ranged_held_fire(&actors[1],&held) && held,
        "fresh authenticated trigger admits observer held stance");
    host_remote_weak_held=FALSE;
    check(runtime_ranged_held_fire(&actors[1],&held) && !held,
        "release is observable independently of per-shot animation edges");
    float target[3]; actor_position_result=TRUE;
    host_remote_target_valid=TRUE;
    memcpy(host_remote_target,(float[]){1,62,83},12);
    check(runtime_ranged_target(&actors[1],target) && target[1]==62,
        "host validates and preserves client's native convergence point");
    host_remote_target[0]=50;
    check(!runtime_ranged_target(&actors[1],target),"remote camera cannot be relocated away from actor");
    host_remote_target[0]=1; host_remote_target_valid=FALSE;
    check(!runtime_ranged_target(&actors[1],target),"missing target cannot admit host gun firing");
    host_remote_target_valid=TRUE;
    ranged_remote_lease=NULL;
    check(!runtime_ranged_aim(&actors[1],TRUE,direction),"lost native takeover rejects aim");
    ranged_remote_lease=&actors[1];
    host_last_remote_input_at_ms=GetTickCount()-300u;
    check(!runtime_ranged_aim(&actors[1],TRUE,direction),"stale LAN aim rejects");
    check(!runtime_ranged_held_fire(&actors[1],&held),"stale trigger cannot maintain held pose");
    check(!runtime_ranged_target(&actors[1],target),"stale convergence point rejects too");
    host_last_remote_input_at_ms=GetTickCount();
    session_status.peer_connected=FALSE;
    check(!runtime_ranged_aim(&actors[1],TRUE,direction),"disconnected LAN aim rejects");
    check(!runtime_ranged_held_fire(&actors[1],&held),"disconnect cannot retain held stance");
    check(!runtime_ranged_target(&actors[1],target),"disconnected point cannot be reused");
    session_status.peer_connected=TRUE;
    character_skill_observation.active=TRUE;
    check(!runtime_ranged_aim(&actors[1],FALSE,direction),"own cast rejects gun pose");
    check(!runtime_ranged_held_fire(&actors[1],&held),"own cast rejects held stance");
    character_skill_observation.active=FALSE;
    check(!runtime_ranged_aim(&actors[0],TRUE,direction),"non-ranged actor cannot borrow aim");
    runtime_installed=tal_initialized=ailish_initialized=FALSE;
    host_remote_ailish_owned=host_remote_first_person_active=FALSE;
    host_remote_target_valid=FALSE; memset(host_remote_target,0,12);
    ranged_remote_lease=NULL;
    reset_stub_policy(); reset_stub_counts();
    ranged_aim_fixture=FALSE;
}

static void verify_local_gun_idle(void) {
    int elco=0,buki=0,replacement=0;
    for(unsigned host=0;host<2;++host) {
        reset_stub_policy();reset_stub_counts();
        runtime_config=make_config(host ? SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL:
            SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH);
        runtime_config.host_actor_type=host ? SUDEKIMP_LAN_ARENA_ELCO_TYPE:SUDEKIMP_LAN_ARENA_BUKI_TYPE;
        runtime_config.client_actor_type=host ? SUDEKIMP_LAN_ARENA_BUKI_TYPE:SUDEKIMP_LAN_ARENA_ELCO_TYPE;
        runtime_installed=tal_initialized=ailish_initialized=TRUE;
        cleanroom_combat_enabled=TRUE;
        cleanroom_actor_entities[SUDEKIMP_CLEANROOM_ELCO]=&elco;
        cleanroom_actor_entities[SUDEKIMP_CLEANROOM_BUKI]=&buki;
        session_status_result=TRUE;session_status.peer_connected=TRUE;
        session_status.local_role=runtime_config.local_role;
        session_status.local_simulation_node_role=runtime_config.local_simulation_node_role;
        session_status.peer_simulation_node_role=host ? SUDEKIMP_LAN_ARENA_SIMULATION_NODE_REPLICA:
            SUDEKIMP_LAN_ARENA_SIMULATION_NODE_CANONICAL_NATIVE_WORLD;
        session_status.session_token=777;
        runtime_skill_ui_bound=TRUE;runtime_skill_ui_retiring=FALSE;
        runtime_skill_ui_local=&elco;runtime_skill_ui_remote=&buki;runtime_skill_ui_session=777;
        fake_spirit_instances[0]=(SudekiMpSpiritInstance){&elco,&elco,1};
        fake_spirit_instances[1]=(SudekiMpSpiritInstance){&buki,&buki,2};
        memcpy(runtime_spirit_instances,fake_spirit_instances,sizeof(runtime_spirit_instances));
        fake_spirit_states[0]=(SudekiMpSpiritInstanceState){0,0,FALSE,TRUE};
        fake_spirit_states[1]=(SudekiMpSpiritInstanceState){10,4,TRUE,FALSE};
        character_skill_observation.active=FALSE;cast_context_actor_busy=NULL;cast_context_actor_known=TRUE;
        check(runtime_local_gun_idle(&elco),"local idle uses its own manager; no observer aim snapshot needed");
        check(!runtime_local_gun_idle(&buki),"remote/non-Elco actor cannot borrow local idle witness");
        fake_spirit_states[0].idle=FALSE;
        check(!runtime_local_gun_idle(&elco),"local strike tail blocks idle correction");
        fake_spirit_states[0].idle=TRUE;ui_abi_healthy=FALSE;
        check(!runtime_local_gun_idle(&elco),"unknown private manager is not idle");ui_abi_healthy=TRUE;
        character_skill_observation.active=TRUE;
        check(!runtime_local_gun_idle(&elco),"active ordinary cast blocks idle correction");
        character_skill_observation.active=FALSE;cast_context_actor_busy=&elco;
        check(!runtime_local_gun_idle(&elco),"undrained local cast blocks idle correction");cast_context_actor_busy=NULL;
        cleanroom_actor_entities[SUDEKIMP_CLEANROOM_ELCO]=&replacement;
        check(!runtime_local_gun_idle(&elco),"replaced local actor invalidates idle lease");
        cleanroom_actor_entities[SUDEKIMP_CLEANROOM_ELCO]=&elco;
        session_status.session_token=778;
        check(!runtime_local_gun_idle(&elco),"new session cannot reuse old idle lease");session_status.session_token=777;
        session_status.peer_connected=FALSE;
        check(!runtime_local_gun_idle(&elco),"disconnect rejects local correction");session_status.peer_connected=TRUE;
        runtime_skill_ui_retiring=TRUE;
        check(!runtime_local_gun_idle(&elco),"retiring ownership rejects local correction");runtime_skill_ui_retiring=FALSE;
        cleanroom_combat_enabled=FALSE;
        check(!runtime_local_gun_idle(&elco),"noncombat does not borrow FP idle correction");
        runtime_installed=tal_initialized=ailish_initialized=runtime_skill_ui_bound=FALSE;
        runtime_skill_ui_local=runtime_skill_ui_remote=NULL;runtime_skill_ui_session=0;
        memset(runtime_spirit_instances,0,sizeof(runtime_spirit_instances));
        memset(fake_spirit_instances,0,sizeof(fake_spirit_instances));
        cleanroom_actor_entities[SUDEKIMP_CLEANROOM_ELCO]=NULL;
        cleanroom_actor_entities[SUDEKIMP_CLEANROOM_BUKI]=NULL;
    }
    reset_stub_policy();reset_stub_counts();
}

int main(void) {
    uint8_t *image = (uint8_t *)VirtualAlloc(
        NULL,
        TEST_IMAGE_SIZE,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE);
    check(image != NULL, "allocate synthetic supported image");
    if (image == NULL) return 1;

    memset(image, 0xcc, TEST_IMAGE_SIZE);
    reset_stub_policy();
    reset_stub_counts();

    verify_remote_ranged_aim();
    verify_local_gun_idle();
    verify_weapon_snapshot_family();
    verify_client_install_and_uninstall(image);
    verify_client_install_and_uninstall(image);
    verify_host_hook_scope(image);
    verify_host_spirit_audio_rollback(image);
    verify_host_spirit_visual_rollback(image);
    verify_host_visual_witness_and_capture(image);
    verify_second_render_mismatch_rollback(image);
    verify_downstream_failure_rollback(image);
    verify_campaign_guard_teardown_containment(image);
    verify_client_busy_reset_containment(image);
    verify_client_delayed_connection_recovery(image);
    verify_callback_order();
    verify_client_tal_lifecycle_generation();
    verify_client_tal_pending_request_teardown();
    verify_host_spirit_lifecycle();
    verify_remote_spirit_lifecycle();
    verify_host_spirit_audio_semantic_journal();
    verify_host_spirit_operator_two_phase();
    verify_host_character_skill_observation_gap();
    verify_host_exact_character_skill_sequences();
    verify_host_snapshot_failure_telemetry_policy();
    verify_host_character_skill_sidecar_wire_fallback();
    verify_host_noncaster_startup_gap_ownership();
    verify_host_native_task_drain_without_peer();
    verify_authoritative_locomotion_stop_policy();
    verify_host_anim_id_read();
    verify_persistent_skill_ui_lifecycle(image);
    verify_legacy_client_skill_ui(image);

    VirtualFree(image, 0u, MEM_RELEASE);
    if (failures != 0) {
        fprintf(stderr, "%d LAN runtime hook test(s) failed\n", failures);
        return 1;
    }
    puts("LAN arena runtime hook tests passed");
    return 0;
}

static HANDLE WINAPI test_create_event(
    LPSECURITY_ATTRIBUTES attributes,
    BOOL manual_reset,
    BOOL initial_state,
    LPCWSTR name
) {
    (void)attributes;
    (void)manual_reset;
    (void)initial_state;
    (void)name;
    return (HANDLE)(uintptr_t)1u;
}

static HANDLE WINAPI test_create_thread(
    LPSECURITY_ATTRIBUTES attributes,
    SIZE_T stack_size,
    LPTHREAD_START_ROUTINE start,
    LPVOID parameter,
    DWORD flags,
    LPDWORD thread_id
) {
    (void)attributes;
    (void)stack_size;
    (void)start;
    (void)parameter;
    (void)flags;
    if (thread_id != NULL) *thread_id = 1u;
    return (HANDLE)(uintptr_t)2u;
}

static BOOL WINAPI test_set_event(HANDLE handle) {
    (void)handle;
    return TRUE;
}

static DWORD WINAPI test_wait_for_single_object(HANDLE handle, DWORD timeout) {
    (void)handle;
    (void)timeout;
    return WAIT_OBJECT_0;
}

static BOOL WINAPI test_close_handle(HANDLE handle) {
    (void)handle;
    return TRUE;
}

/* Runtime dependency fakes.  They deliberately expose only lifecycle state;
 * native actor/network behavior belongs to the focused adapter tests. */
BOOL SudekiMpLanArenaSessionStart(
    const SudekiMpLanArenaSessionConfig *config
) {
    (void)config;
    ++session_start_count;
    if (!session_start_result) SetLastError(ERROR_NOT_READY);
    return session_start_result;
}

void SudekiMpLanArenaSessionStop(BOOL notify_peer) {
    (void)notify_peer;
    ++session_stop_count;
}

void SudekiMpLanArenaSessionPoll(uint32_t now_ms) {
    (void)now_ms;
}

BOOL SudekiMpLanArenaSessionGetStatus(SudekiMpLanArenaSessionStatus *status) {
    if (status != NULL) {
        if (session_status_result) {
            *status = session_status;
        } else {
            memset(status, 0, sizeof(*status));
        }
    }
    return session_status_result;
}

BOOL SudekiMpLanArenaSessionTakeRemoteInput(SudekiMpLanArenaInput *input) {
    (void)input;
    return FALSE;
}

BOOL SudekiMpLanArenaSessionSendSnapshot(
    const SudekiMpLanArenaSnapshot *snapshot
) {
    ++snapshot_send_count;
    if (snapshot != NULL) last_sent_snapshot = *snapshot;
    return snapshot_send_result;
}

BOOL SudekiMpInstallLanArenaCampaignGuard(HMODULE game_module) {
    (void)game_module;
    if (!campaign_guard_install_result) SetLastError(ERROR_INVALID_DATA);
    return campaign_guard_install_result;
}

BOOL SudekiMpUninstallLanArenaCampaignGuard(void) {
    ++campaign_guard_uninstall_count;
    if (!campaign_guard_uninstall_result) SetLastError(ERROR_BUSY);
    return campaign_guard_uninstall_result;
}

BOOL SudekiMpInstallLanArenaCollisionDebug(HMODULE game_module) {
    (void)game_module;
    if (!collision_debug_install_result) SetLastError(ERROR_INVALID_DATA);
    return collision_debug_install_result;
}

void SudekiMpLanArenaCollisionDebugServiceHotkey(void) {
    record_callback_event('H');
}

unsigned int SudekiMpLanArenaCollisionDebugMode(void) {
    return SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF;
}

void SudekiMpUninstallLanArenaCollisionDebug(void) {
    ++collision_debug_uninstall_count;
}

/* New hit observer is independently covered by HitFeedbackTest and the exact
 * image test. These stubs keep this coordinator fixture native-call-free. */
BOOL SudekiMpLanHitHostInstall(HMODULE image, SudekiMpLanHitWitness witness) {
    return image != NULL && witness != NULL;
}
BOOL SudekiMpLanHitHostUninstall(void) { return TRUE; }
void SudekiMpLanHitHostSnapshot(SudekiMpLanArenaEnemySnapshot *enemy) { (void)enemy; }
BOOL SudekiMpLanHitResolveTarget(void *entity, uint64_t session, SudekiMpLanHitTarget *target) {
    (void)entity; (void)session; (void)target; return FALSE;
}

BOOL SudekiMpInstallLanArenaSpiritAudioTrace(
    HMODULE game_module,
    SudekiMpLanArenaSpiritActiveWitness active_witness,
    void *witness_context
) {
    (void)game_module;
    ++spirit_audio_install_count;
    if (!spirit_audio_install_result) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (!spirit_audio_physically_installed) {
        spirit_audio_physically_installed = TRUE;
        ++spirit_audio_physical_patch_count;
    }
    spirit_audio_installed = TRUE;
    spirit_audio_witness = active_witness;
    spirit_audio_witness_context = witness_context;
    return TRUE;
}

BOOL SudekiMpUninstallLanArenaSpiritAudioTrace(void) {
    if (!spirit_audio_installed) return TRUE;
    ++spirit_audio_uninstall_count;
    record_spirit_observer_teardown('A');
    if (!spirit_audio_uninstall_result) {
        SetLastError(ERROR_WRITE_FAULT);
        return FALSE;
    }
    spirit_audio_installed = FALSE;
    return TRUE;
}

BOOL SudekiMpLanArenaSpiritAudioTraceInstalled(void) {
    return spirit_audio_installed;
}

BOOL SudekiMpLanArenaSpiritVisualHostImageMatches(HMODULE game_module) {
    return game_module != NULL && spirit_visual_install_result;
}

BOOL SudekiMpLanArenaSpiritVisualHostInitialize(
    HMODULE game_module,
    SudekiMpLanArenaSpiritVisualHostWitness witness,
    void *context
) {
    ++spirit_visual_install_count;
    if (!SudekiMpLanArenaSpiritVisualHostImageMatches(game_module)) {
        spirit_visual_installed = spirit_visual_install_leaves_lease;
        SetLastError(ERROR_BAD_FORMAT);
        return FALSE;
    }
    spirit_visual_installed = TRUE;
    spirit_visual_witness = witness;
    spirit_visual_witness_context = context;
    return TRUE;
}

BOOL SudekiMpLanArenaSpiritVisualHostReset(void) {
    ++spirit_visual_reset_count;
    if (!spirit_visual_installed) return TRUE;
    record_spirit_observer_teardown('V');
    if (!spirit_visual_reset_result) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    spirit_visual_installed = FALSE;
    spirit_visual_witness = NULL;
    spirit_visual_witness_context = NULL;
    return TRUE;
}

BOOL SudekiMpLanArenaSpiritVisualHostCapture(
    uint64_t session, uint16_t skill, uint32_t tick,
    void *tal, void *ailish,
    SudekiMpLanArenaSnapshot *snapshot
) {
    (void)session;
    (void)skill;
    (void)tick;
    (void)tal;
    (void)ailish;
    ++spirit_visual_capture_count;
    if (snapshot == NULL) return FALSE;
    snapshot->spirit_vfx_observed = spirit_visual_capture_observed ? 1u : 0u;
    snapshot->spirit_vfx_count = 0u;
    memset(snapshot->spirit_vfx, 0, sizeof(snapshot->spirit_vfx));
    if (!spirit_visual_capture_result) {
        /* Inject a partially written failed sample: runtime must sanitize it
         * before the optional domain reaches canonical snapshot validation. */
        snapshot->spirit_vfx_observed = 1u;
        snapshot->spirit_vfx_count = 9u;
        snapshot->spirit_vfx[0].instance_sequence = 99u;
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    return TRUE;
}

size_t SudekiMpLanArenaSpiritAudioTraceSnapshot(
    SudekiMpLanArenaSpiritAudioEvent *events,
    size_t capacity,
    uint32_t *dropped_count
) {
    size_t copied = spirit_audio_event_count < capacity ?
        spirit_audio_event_count : capacity;
    if (events != NULL && copied != 0u) {
        memcpy(events,
            spirit_audio_events + spirit_audio_event_count - copied,
            copied * sizeof(*events));
    }
    if (dropped_count != NULL) *dropped_count = 0u;
    return copied;
}

BOOL SudekiMpInstallLanArenaHostInput(HMODULE game_module) {
    (void)game_module;
    if (!host_input_install_result) SetLastError(ERROR_INVALID_DATA);
    return host_input_install_result;
}

BOOL SudekiMpUninstallLanArenaHostInput(void) {
    ++host_input_uninstall_count;
    host_native_skill_start_observer = NULL;
    return TRUE;
}

void SudekiMpLanArenaHostInputSetNativeSkillStartObserver(
    SudekiMpLanArenaHostNativeSkillStartObserver observer
) {
    host_native_skill_start_observer = observer;
}

BOOL SudekiMpLanArenaHostInputTakeSkillSlot(unsigned int *slot) {
    (void)slot;
    return FALSE;
}

SudekiMpSkillActivationResult SudekiMpReplayHostApprovedCharacterSkillSlot(
    void *character,
    int slot
) {
    SudekiMpSkillActivationResult result;
    (void)character;
    (void)slot;
    memset(&result, 0, sizeof(result));
    return result;
}

const char *SudekiMpSkillActivationStatusName(
    SudekiMpSkillActivationStatus status
) {
    (void)status;
    return "inactive";
}

BOOL SudekiMpLanArenaHostInputTakeSpiritVariant(unsigned int *variant) {
    if (!host_spirit_request_available || variant == NULL) return FALSE;
    ++host_spirit_request_take_count;
    host_spirit_request_available = FALSE;
    *variant = host_spirit_request_variant;
    return TRUE;
}

BOOL SudekiMpLanArenaHostInputDiscardSpiritRequests(void) {
    BOOL discarded = host_spirit_request_available;
    if (discarded) ++host_spirit_request_discard_count;
    host_spirit_request_available = FALSE;
    return discarded;
}

BOOL SudekiMpLanArenaHostInputTalControllerLeaseExact(void *tal) {
    return host_tal_controller_lease_exact &&
        tal == cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL];
}

void SudekiMpLanArenaHostInputServiceCombatToggle(void) {}

void SudekiMpLanArenaHostInputNotifyNativeActionObserved(void) {}

BOOL SudekiMpLanArenaHostInputDiagnosticTraceActive(void) {
    return FALSE;
}

BOOL SudekiMpLanArenaPausePanelActive(void) {
    return cleanroom_pause_active;
}

BOOL SudekiMpCleanroomMenuActive(void) {
    return cleanroom_menu_active;
}

BOOL SudekiMpInstallLanArenaClientInput(HMODULE game_module) {
    (void)game_module;
    if (!client_input_install_result) SetLastError(ERROR_INVALID_DATA);
    return client_input_install_result;
}

BOOL SudekiMpUninstallLanArenaClientInput(void) {
    ++client_input_uninstall_count;
    return TRUE;
}

void SudekiMpLanArenaClientInputService(void) {}

BOOL SudekiMpInitializeLanArenaClientReplica(HMODULE game_module) {
    (void)game_module;
    ++replica_initialize_count;
    if (!client_replica_initialize_result) SetLastError(ERROR_NOT_READY);
    if (client_replica_initialize_result) replica_stub_initialized = TRUE;
    return client_replica_initialize_result;
}

void SudekiMpLanArenaClientReplicaDiscardSnapshots(void) {}

BOOL SudekiMpResetLanArenaClientReplica(void) {
    ++replica_reset_count;
    if (!client_replica_reset_result) SetLastError(ERROR_BUSY);
    if (client_replica_reset_result) replica_stub_initialized = FALSE;
    return client_replica_reset_result;
}

void SudekiMpLanArenaClientReplicaSetRemoteTalLease(
    void *actor,
    uint32_t actor_generation
) {
    ++client_tal_lease_publish_count;
    client_tal_published_actor = actor;
    client_tal_published_generation = actor_generation;
}

BOOL SudekiMpLanArenaClientReplicaRemoteTalReleaseReady(void) {
    ++client_tal_release_ready_count;
    if (!client_tal_release_ready_result) SetLastError(ERROR_BUSY);
    return client_tal_release_ready_result;
}

BOOL SudekiMpLanArenaClientReplicaApplyLatest(void) {
    ++replica_apply_count;
    record_callback_event('A');
    return replica_apply_result;
}

BOOL SudekiMpLanArenaClientReplicaRefreshOwnerViewAfterRender(void) {
    record_callback_event('V');
    return TRUE;
}

BOOL SudekiMpLanArenaClientReplicaReassertOwnerViewAfterRemoteMutation(void) {
    record_callback_event('C');
    return TRUE;
}
BOOL SudekiMpLanArenaClientSpiritViewBeginFrame(void) { return TRUE; }
BOOL SudekiMpLanArenaClientSpiritViewPrepareBody(void) { record_callback_event('B'); return TRUE; }
BOOL SudekiMpLanArenaClientSpiritViewEndFrame(void) { return TRUE; }

BOOL SudekiMpLanArenaClientReplicaReassertPresentation(void) {
    record_callback_event('S');
    return TRUE;
}

void SudekiMpLanArenaClientObserveFirstPersonFrame(unsigned int phase) {
    (void)phase;
}

BOOL SudekiMpLanArenaClientReplicaPublishVisibleTransforms(void) {
    record_callback_event('P');
    if (!visible_publish_result) SetLastError(ERROR_INVALID_STATE);
    return visible_publish_result;
}

BOOL SudekiMpLanArenaClientReplicaServiceSpiritVfx(void) {
    record_callback_event('X');
    return TRUE;
}

void SudekiMpLanArenaClientReplicaRefreshDiagnostics(void) {}

BOOL SudekiMpLanArenaClientReplicaGetDiagnostics(
    SudekiMpLanArenaReplicaDiagnostics *diagnostics
) {
    (void)diagnostics;
    return FALSE;
}

BOOL SudekiMpControlUpdateObserverGateEnable(
    SudekiMpControlUpdateObserverGate *gate
) {
    if (gate != NULL) gate->enabled = observer_gate_enable_result ? 1 : 0;
    if (!observer_gate_enable_result) SetLastError(ERROR_INVALID_STATE);
    return observer_gate_enable_result;
}

BOOL SudekiMpControlUpdateObserverGateTryEnter(
    SudekiMpControlUpdateObserverGate *gate
) {
    (void)gate;
    return TRUE;
}

void SudekiMpControlUpdateObserverGateLeave(
    SudekiMpControlUpdateObserverGate *gate
) {
    (void)gate;
}

void SudekiMpControlUpdateObserverGateDisable(
    SudekiMpControlUpdateObserverGate *gate
) {
    if (gate != NULL) gate->enabled = 0;
}

void SudekiMpControlUpdateObserverGateDrain(
    SudekiMpControlUpdateObserverGate *gate
) {
    (void)gate;
}

BOOL SudekiMpControlSeparationRegisterUpdateObserver(
    const void *owner,
    SudekiMpControlUpdateObserver observer
) {
    (void)owner;
    (void)observer;
    if (!observer_register_result) SetLastError(ERROR_INVALID_STATE);
    return observer_register_result;
}

BOOL SudekiMpControlSeparationUnregisterUpdateObserver(const void *owner) {
    (void)owner;
    return TRUE;
}

BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(
    const SudekiMpControlUpdateDispatchWitness *witness
) {
    (void)witness;
    return update_witness_exact;
}

BOOL SudekiMpControlSeparationSetManualToggleEnabled(BOOL enabled) {
    (void)enabled;
    return TRUE;
}

BOOL SudekiMpControlSeparationSetLanArenaRemoteInputEnabled(BOOL enabled) {
    (void)enabled;
    return TRUE;
}

BOOL SudekiMpControlSeparationBindLanArenaMovementActors(
    unsigned int local_type, unsigned int remote_type
) {
    check(local_type == seat_host_type(), "movement binding follows host assignment");
    check(remote_type == seat_client_type(), "movement binding follows client assignment");
    return TRUE;
}

BOOL SudekiMpControlSeparationSetPlayerOneSkillInputIsolation(BOOL enabled) {
    (void)enabled;
    return TRUE;
}

BOOL SudekiMpControlSeparationSetLanArenaPlayerTwoSkillInputIsolation(
    BOOL enabled
) {
    player_two_skill_isolation_enabled = enabled != FALSE;
    ++player_two_skill_isolation_call_count;
    return TRUE;
}

BOOL SudekiMpControlSeparationReleasePlayerTwoNow(void) {
    ++release_player_two_count;
    record_client_tal_teardown_event('L');
    return player_two_release_result;
}

BOOL SudekiMpControlSeparationForceStopCharacter(void *character) {
    (void)character;
    return TRUE;
}

BOOL SudekiMpControlSeparationPlayerTwoActive(void) {
    return player_two_active;
}

void *SudekiMpControlSeparationPlayerTwoCharacter(void) {
    return player_two_character;
}

BOOL SudekiMpControlSeparationRequestPlayerTwoCharacter(void *character) {
    (void)character;
    ++request_player_two_count;
    return player_two_request_result;
}

BOOL SudekiMpControlSeparationSubmitLanArenaPlayerTwoInput(
    float world_direction_x,
    float world_direction_z,
    float aim_direction_x,
    float aim_direction_z,
    BOOL aim_direction_valid,
    BOOL weak_attack_active,
    float frame_delta_seconds
) {
    (void)world_direction_x;
    (void)world_direction_z;
    (void)aim_direction_x;
    (void)aim_direction_z;
    (void)aim_direction_valid;
    (void)weak_attack_active;
    (void)frame_delta_seconds;
    return TRUE;
}

BOOL SudekiMpControlSeparationSubmitLanArenaPlayerTwoRangedFire(void) {
    return TRUE;
}

BOOL SudekiMpControlSeparationLanArenaPlayerTwoRangedReady(
    BOOL *ready,
    uint16_t *authored_delay_half
) {
    if (ready == NULL || authored_delay_half == NULL) return FALSE;
    *ready = TRUE;
    *authored_delay_half = 0x4000u;
    return TRUE;
}

BOOL SudekiMpLanArenaRemoteInputFresh(
    uint32_t last_input_at_ms,
    uint32_t now_ms,
    uint32_t maximum_age_ms
) {
    return ranged_aim_fixture && last_input_at_ms != 0u &&
        (uint32_t)(now_ms-last_input_at_ms) <= maximum_age_ms;
}

int SudekiMpLanArenaParseEndpoint(
    const char *text,
    uint16_t default_port,
    char *ipv4,
    size_t ipv4_capacity,
    uint16_t *port
) {
    static const char loopback[] = "127.0.0.1";
    if (text == NULL || ipv4 == NULL || port == NULL ||
        ipv4_capacity < sizeof(loopback)) return 0;
    memcpy(ipv4, loopback, sizeof(loopback));
    *port = default_port;
    return 1;
}

const char *SudekiMpCleanroomActorLabel(SudekiMpCleanroomActor actor) {
    (void)actor;
    return "actor";
}

void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor actor) {
    if ((unsigned int)actor >= SUDEKIMP_CLEANROOM_ACTOR_COUNT) return NULL;
    return cleanroom_actor_entities[actor];
}

BOOL SudekiMpCleanroomActorFromType(unsigned int type, SudekiMpCleanroomActor *actor) {
    if (actor == NULL) {
        return FALSE;
    }
    switch (type) {
    case 0x23u: *actor = SUDEKIMP_CLEANROOM_TAL; return TRUE;
    case 0x05u: *actor = SUDEKIMP_CLEANROOM_BUKI; return TRUE;
    case 0x0eu: *actor = SUDEKIMP_CLEANROOM_ELCO; return TRUE;
    case 0x01u: *actor = SUDEKIMP_CLEANROOM_AILISH; return TRUE;
    default: return FALSE;
    }
}

BOOL SudekiMpCleanroomActorIsRanged(SudekiMpCleanroomActor actor) {
    return actor == SUDEKIMP_CLEANROOM_AILISH ||
        actor == SUDEKIMP_CLEANROOM_ELCO;
}

BOOL SudekiMpObserveCharacterSkill(
    void *character,
    SudekiMpCharacterSkillState *state
) {
    if (!character_skill_observe_result || character == NULL || state == NULL) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    *state = character_skill_observation;
    for(unsigned int i=0;i<2u;++i)
        if(character==paired_skill_actors[i]) *state=paired_skill_observations[i];
    return TRUE;
}

void SudekiMpCleanroomMenuRender(void) {
}

BOOL SudekiMpCleanroomEngineActorPresent(SudekiMpCleanroomActor actor) {
    if ((unsigned int)actor >= SUDEKIMP_CLEANROOM_ACTOR_COUNT) return FALSE;
    return actor_present_results[actor];
}

BOOL SudekiMpCleanroomEngineCombatMode(BOOL *enabled) {
    if (enabled == NULL) return FALSE;
    if (host_spirit_reproof_probe_teardown) {
        host_spirit_reproof_probe_depth = InterlockedCompareExchange(
            &host_operator_spirit_activation_depth, 0, 0);
        SetLastError(ERROR_SUCCESS);
        host_spirit_reproof_probe_saw_busy =
            !host_native_tasks_drained() && GetLastError() == ERROR_BUSY;
    }
    *enabled = cleanroom_combat_enabled;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineSpiritPresentationState(int *state) {
    if(fake_spirit_observer) { int id; return fake_spirit_observer(state,&id); }
    if (!spirit_presentation_state_result || state == NULL) return FALSE;
    *state = spirit_presentation_state;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineSpiritStrikeId(int *id) {
    if(fake_spirit_observer) { int state; return fake_spirit_observer(&state,id); }
    if (!spirit_presentation_state_result || id == NULL) return FALSE;
    *id = fixture_spirit_id;
    return TRUE;
}

BOOL SudekiMpResolveSpiritStrikeId(unsigned int type, unsigned int variant, int *id) {
    int first = type == 0x23u ? 0 : type == 1u ? 2 : type == 5u ? 4 : type == 14u ? 6 : -1;
    if (first < 0 || variant < 1u || variant > 2u || id == NULL) return FALSE;
    *id = first + (int)variant - 1;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineRangedCombatPrimePending(void) {
    return ranged_combat_prime_pending;
}

BOOL SudekiMpCleanroomEnginePrimeRangedCombat(void) {
    ++ranged_combat_prime_call_count;
    if (!ranged_combat_prime_result) return FALSE;
    ranged_combat_prime_pending = TRUE;
    return TRUE;
}

void SudekiMpCleanroomEngineMaintainResources(void) {
    ++maintain_resources_call_count;
}

BOOL SudekiMpDescribeCharacterSpiritOptions(
    void *character,
    SudekiMpSpiritQuickOptionList *options
) {
    unsigned int variant;
    if (!host_spirit_options_result || character == NULL || options == NULL) {
        return FALSE;
    }
    if (host_spirit_describe_probe_teardown) {
        host_spirit_describe_probe_depth = InterlockedCompareExchange(
            &host_operator_spirit_activation_depth, 0, 0);
        SetLastError(ERROR_SUCCESS);
        host_spirit_describe_probe_saw_busy =
            !host_native_tasks_drained() && GetLastError() == ERROR_BUSY;
    }
    memset(options, 0, sizeof(*options));
    options->resource_type = SUDEKIMP_LAN_ARENA_TAL_TYPE;
    options->option_count = 2u;
    for (variant = 1u; variant <= 2u; ++variant) {
        options->options[variant - 1u].variant = variant;
        options->options[variant - 1u].strike_id = (int)variant - 1;
        options->options[variant - 1u].validation_result =
            host_spirit_option_available ? 0 : 7;
        options->options[variant - 1u].available =
            host_spirit_option_available ? 1u : 0u;
    }
    return TRUE;
}

SudekiMpSpiritActivationResult SudekiMpActivateCharacterSpirit(
    void *character,
    unsigned int variant
) {
    SudekiMpSpiritActivationResult result;
    memset(&result, 0, sizeof(result));
    ++host_spirit_activation_call_count;
    host_spirit_last_activated_variant = variant;
    if (host_spirit_activation_probe_teardown) {
        host_spirit_activation_probe_depth = InterlockedCompareExchange(
            &host_operator_spirit_activation_depth, 0, 0);
        SetLastError(ERROR_SUCCESS);
        host_spirit_activation_probe_saw_busy =
            !host_native_tasks_drained() && GetLastError() == ERROR_BUSY;
    }
    result.strike_id = (int)variant - 1;
    if (character != cleanroom_actor_entities[SUDEKIMP_CLEANROOM_TAL]) {
        result.status = SUDEKIMP_SPIRIT_ACTIVATION_INVALID_CONTEXT;
    } else if (host_spirit_activation_started) {
        result.status = SUDEKIMP_SPIRIT_ACTIVATION_STARTED;
        result.activation_result = 1;
    } else {
        result.status = SUDEKIMP_SPIRIT_ACTIVATION_ACTIVATION_REJECTED;
    }
    return result;
}

const char *SudekiMpSpiritActivationStatusName(
    SudekiMpSpiritActivationStatus status
) {
    return status == SUDEKIMP_SPIRIT_ACTIVATION_STARTED ?
        "started" : "rejected";
}

BOOL SudekiMpCleanroomEngineActorPosition(
    SudekiMpCleanroomActor actor,
    float position[3]
) {
    (void)actor;
    if (!actor_position_result || position == NULL) return FALSE;
    position[0] = 1.0f;
    position[1] = 2.0f;
    position[2] = 3.0f;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineActorFacing(
    SudekiMpCleanroomActor actor,
    float facing[2]
) {
    (void)actor;
    if (!actor_facing_result || facing == NULL) return FALSE;
    facing[0] = 0.0f;
    facing[1] = 1.0f;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineActorResources(
    SudekiMpCleanroomActor actor,
    float *hit_points,
    float *skill_points
) {
    (void)actor;
    if (!actor_resources_result || hit_points == NULL || skill_points == NULL) {
        return FALSE;
    }
    *hit_points = 100.0f;
    *skill_points = 50.0f;
    return TRUE;
}

BOOL SudekiMpCleanroomEngineActorPresentation(
    SudekiMpCleanroomActor actor,
    SudekiMpCleanroomActorPresentation *presentation
) {
    (void)actor;
    (void)presentation;
    return FALSE;
}

BOOL SudekiMpCleanroomEngineSpawnActor(
    SudekiMpCleanroomActor actor,
    const float position[3]
) {
    (void)actor;
    (void)position;
    ++spawn_actor_count;
    return spawn_actor_result;
}

BOOL SudekiMpCleanroomEngineInitializePartyActor(
    SudekiMpCleanroomActor actor
) {
    (void)actor;
    ++initialize_party_actor_count;
    return initialize_party_actor_result;
}

BOOL SudekiMpCleanroomEngineRemoveActor(SudekiMpCleanroomActor actor) {
    ++remove_actor_count;
    record_client_tal_teardown_event('R');
    if (remove_actor_result && remove_actor_clears_presence &&
        (unsigned int)actor < SUDEKIMP_CLEANROOM_ACTOR_COUNT) {
        actor_present_results[actor] = FALSE;
        cleanroom_actor_entities[actor] = NULL;
    }
    return remove_actor_result;
}

BOOL SudekiMpCleanroomEngineDummyPresent(void) {
    return FALSE;
}

BOOL SudekiMpCleanroomEngineDummySnapshot(
    float position[3],
    float *hit_points
) {
    (void)position;
    (void)hit_points;
    return FALSE;
}

BOOL SudekiMpCleanroomEngineSpawnDummy(const float position[3]) {
    (void)position;
    return FALSE;
}

BOOL SudekiMpCleanroomEngineRemoveDummy(void) {
    return TRUE;
}

void SudekiMpLogWrite(const char *message) {
    (void)message;
}

void SudekiMpLogFormat(const char *format, ...) {
    (void)format;
    ++log_format_call_count;
}
