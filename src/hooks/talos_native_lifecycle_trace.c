#include "hooks/talos_native_lifecycle_trace.h"

#include "engine/log.h"
#include "hooks/call_hook.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Talos native lifecycle tracing requires the 32-bit Windows target"
#endif

#define SUDEKIMP_FASTCALL __attribute__((fastcall))

#define RESOURCE_IDENTIFIER_PC_BUKI UINT32_C(0x019c1eba)
#define RESOURCE_IDENTIFIER_PC_AILISH UINT32_C(0x8557d453)
#define RESOURCE_IDENTIFIER_PC_ELCO UINT32_C(0x0180e1d4)
#define RESOURCE_IDENTIFIER_PC_KAZEL UINT32_C(0xa6d349cc)

#define HERO_MASK_TAL UINT8_C(0x01)
#define HERO_MASK_AILISH UINT8_C(0x02)
#define HERO_MASK_BUKI UINT8_C(0x04)
#define HERO_MASK_ELCO UINT8_C(0x08)
#define HERO_MASK_ALL UINT8_C(0x0f)

enum {
    RVA_SCRIPT_CALL_OPCODE = 0x001c4970u,
    RVA_SCRIPT_SCENE_OPCODE = 0x001c4d30u,
    RVA_SCRIPT_CALL_OPCODE_SLOT = 0x00323fa0u,
    RVA_SCRIPT_SCENE_OPCODE_SLOT = 0x00323fa8u,
    RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL = 0x001c4db8u,
    RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR = 0x001c3170u,
    RVA_KAZEL_GROUP_ADD_CALL = 0x000b15dbu,
    RVA_RAW_GROUP_ADD = 0x00023280u,
    RVA_AI_LISTENER_VTABLE = 0x002ca244u,
    RVA_AI_LISTENER_ADD = 0x000f2b00u,
    RVA_AI_LISTENER_FORMATION_ADD_CALL = 0x000f2b14u,
    RVA_RAW_FORMATION_ADD = 0x000b2cb0u,
    RVA_DELETE_PC = 0x000b2520u,
    RVA_REMOVE_ALL_PLAYERS = 0x000252d0u,
    RVA_FORMATION_POP_MEMBERS = 0x000f6260u,
    RVA_TSA_IS_PLAYING = 0x0001a230u,
    RVA_TSA_SET_PLAYING = 0x0001a240u,
    RVA_TSA_PLAYING_GLOBAL = 0x00408d4cu,
    RVA_SCENE_MANAGER_GLOBAL = 0x00408d58u,
    RVA_ACTIVE_GROUP_GLOBAL = 0x00408d94u,
    RVA_CHARACTER_CONTROLLER_GLOBAL = 0x00408da4u,
    RVA_CAMERA_MANAGER_GLOBAL = 0x00409d7cu,
    RVA_AI_MANAGER_GLOBAL = 0x00409de4u,
    RVA_SCRIPT_RUNTIME_GLOBAL = 0x003c310cu,
    SCRIPT_THREAD_INSTRUCTION_OFFSET = 0x0cu,
    SCRIPT_RUNTIME_BYTECODE_OFFSET = 0x14u,
    LOAD_VOID_OPERAND = 0x00021c0du,
    DELETE_BUKI_OPERAND = 0x0002194eu,
    DELETE_AILISH_OPERAND = 0x00021959u,
    DELETE_ELCO_OPERAND = 0x00021964u,
    SET_ZONE_CARRIER_OPERAND = 0x0002196fu,
    TAL_KAZEL_MERGE_OPERAND = 0x000219f8u,
    SPAWN_KAZEL_WRAPPER_OPERAND = 0x000bc1b3u,
    DELETE_KAZEL_OPERAND = 0x000bc433u,
    INTERNAL_SPAWN_PC_OPERAND = 0x00003099u,
    SET_ZONE_NOW_OPERAND = 0x0000317du,
    END_TSA_OPERAND = 0x00021addu,
    TSA_SET_PLAYING_FALSE_OPERAND = 0x00039bcbu,
    REMOVE_ALL_PLAYERS_OPERAND = 0x0000284bu,
    HASH_LOAD_VOID = 0x70f470c2u,
    HASH_DELETE_PC = 0xfa7ec379u,
    HASH_SET_ZONE = 0x76fc7114u,
    HASH_SET_ZONE_NOW = 0xbc8fdc32u,
    HASH_END_TSA = 0x343b1e0cu,
    HASH_TAL_KAZEL_MERGE = 0x882300d3u,
    HASH_SPAWN_PC_WRAPPER = 0xb3d3544bu,
    HASH_INTERNAL_SPAWN_PC = 0xe9b77316u,
    HASH_TSA_SET_PLAYING = 0x0f3b3bffu,
    HASH_REMOVE_ALL_PLAYERS = 0xc1366076u,
    GROUP_MEMBERS_OFFSET = 0x90u,
    GROUP_MEMBER_STRIDE = 0x0cu,
    GROUP_COUNT_OFFSET = 0xccu,
    FORMATION_MEMBERS_OFFSET = 0xf4u,
    FORMATION_MEMBER_STRIDE = 0x0cu,
    FORMATION_COUNT_OFFSET = 0x124u,
    SUPPORTED_IMAGE_SIZE = 0x0040b000u,
    ZONE_NAME_CAPACITY = 16u,
    RESOURCE_NAME_TEXT_CAPACITY = 16u,
    NATIVE_MEMBER_LIMIT = 4u,
    ACTOR_SECONDARY_VTABLE_OFFSET = 0x08u,
    ACTOR_RESOURCE_VTABLE_OFFSET = 0x2cu,
    CAMERA_MANAGER_CURRENT_OFFSET = 0x20u,
    CAMERA_MANAGER_TABLE_OFFSET = 0x24u,
    CAMERA_MANAGER_TABLE_COUNT = 10u,
    CAMERA_RENDER_STATE_OFFSET = 0x34u,
    CAMERA_NAME_OFFSET = 0x4cu,
    SCENE_MANAGER_RENDERER_OFFSET = 0x40u,
    SCENE_RENDERER_CAMERA_STATE_OFFSET = 0x7cu,
    CONTROLLER_CURRENT_MODE_OFFSET = 0x80u,
    CONTROLLER_REQUESTED_MODE_OFFSET = 0x84u,
    CONTROLLER_TARGET_OFFSET = 0x248u,
    ACTOR_AI_COMPONENT_OFFSET = 0x94u,
    AI_COMPONENT_STATE_OFFSET = 0x3cu,
    AI_COMPONENT_OVERRIDE_COUNT_OFFSET = 0x16au,
    AI_STATE_MODE_OFFSET = 0x0bu
};

enum {
    KAZEL_SERIAL_MERGE_BEFORE = 0x01u,
    KAZEL_SERIAL_MERGE_AFTER = 0x02u,
    KAZEL_SERIAL_SPAWN_WRAPPER_BEFORE = 0x04u,
    KAZEL_SERIAL_SPAWN_WRAPPER_AFTER = 0x08u,
    KAZEL_SERIAL_INTERNAL_SPAWN_BEFORE = 0x10u,
    KAZEL_SERIAL_INTERNAL_SPAWN_AFTER = 0x20u,
    KAZEL_SERIAL_READY_FOR_INTERNAL_SPAWN = 0x0fu,
    KAZEL_SERIAL_COMPLETE = 0x3fu
};

typedef int (SUDEKIMP_FASTCALL *ScriptOpcodeFunction)(void *, void *);
typedef void (*ScriptTaskConstructorFunction)(void);
typedef void (*RawGroupAddFunction)(void);
typedef void (__cdecl *DeletePcFunction)(const void *);
typedef void (__cdecl *RemoveAllPlayersFunction)(void);
typedef void (__cdecl *FormationPopMembersFunction)(void);
typedef unsigned char (__cdecl *TsaIsPlayingFunction)(void);
typedef void (__cdecl *TsaSetPlayingFunction)(unsigned char);

typedef struct NativeRosterSample {
    uint32_t count;
    uint32_t occupied_mask;
    void *members[NATIVE_MEMBER_LIMIT];
    uint64_t member_tokens[NATIVE_MEMBER_LIMIT];
    uint64_t hero_tokens[NATIVE_MEMBER_LIMIT];
    uint8_t hero_by_slot[NATIVE_MEMBER_LIMIT];
    uint8_t hero_mask;
    uint8_t readable;
    uint8_t owner_present;
    uint8_t structurally_valid;
    uint8_t identity_complete;
} NativeRosterSample;

typedef struct HeroVtableEvidence {
    uint32_t main_vtable_rva;
    uint32_t secondary_vtable_rva;
    uint32_t resource_vtable_rva;
    uint32_t main_col_rva;
    uint32_t secondary_col_rva;
    uint32_t resource_col_rva;
    uint32_t type_descriptor_rva;
    uint32_t type_method_rva;
    uint32_t type_value;
    const char *type_descriptor_name;
} HeroVtableEvidence;

typedef struct RosterIdentityTracker {
    uint64_t last_token[NATIVE_MEMBER_LIMIT];
    uint64_t sequence_token[NATIVE_MEMBER_LIMIT];
    uint32_t lease_generation[NATIVE_MEMBER_LIMIT];
    uint32_t roster_revision;
    uint32_t observation_serial;
    uint32_t writer_native_thread_id;
    uint8_t last_present_mask;
    uint8_t has_prior_valid_sample;
    uint8_t sequence_state;
    uint8_t quarantine_reason;
    uint8_t delete_delta_corroborated_mask;
    uint8_t reserved[3];
} RosterIdentityTracker;

typedef struct SettleEvidenceTracker {
    uint32_t session_generation;
    uint32_t script_runtime_generation;
    uint32_t load_void_task_generation;
    uint32_t native_thread_id;
    uint32_t camera_observation_generation;
    uint32_t default_camera_generation;
    uint32_t settle_validation_generation;
    uint8_t void_set_zone_completed;
    uint8_t default_camera_committed;
    uint8_t default_camera_revalidated;
    uint8_t tal_control_revalidated;
    uint8_t settle_evidence_complete;
    uint8_t reserved[3];
} SettleEvidenceTracker;

typedef struct KazelLifecycleTracker {
    uint32_t session_generation;
    uint32_t observation_serial;
    uint32_t request_generation;
    uint32_t script_runtime_generation;
    uint32_t load_void_task_generation;
    uint32_t source_native_thread_id;
    uint32_t completion_native_thread_id;
    uint64_t source_script_thread_token;
    uint64_t original_tal_token;
    uint64_t kazel_token;
    uint64_t group_before_tokens[NATIVE_MEMBER_LIMIT];
    uint64_t formation_before_tokens[NATIVE_MEMBER_LIMIT];
    uint8_t state;
    uint8_t spawn_binding_before_seen;
    uint8_t spawn_binding_after_seen;
    uint8_t group_add_before_seen;
    uint8_t group_add_after_seen;
    uint8_t exact_dark_tal_identity;
    uint8_t group_add_corroborated;
    uint8_t delete_corroborated;
    uint8_t group_add_in_progress;
    uint8_t completion_during_spawn_binding;
    uint8_t serialized_opcode_mask;
    uint8_t ambiguity_reason;
    uint8_t reserved[2];
} KazelLifecycleTracker;

typedef struct PostMovieRestoreTicketTracker {
    SudekiMpTalosNativePostMovieRestoreTicket ready_ticket;
    uint32_t authorization_generation;
    uint32_t load_void_session_count;
    uint8_t state;
    uint8_t allow_ticket;
    uint8_t exact_asset_authenticated;
    uint8_t reserved;
} PostMovieRestoreTicketTracker;

typedef struct NativeResourceNameEvidence {
    uint32_t encoded_kind;
    uint32_t identifier;
    uint32_t recomputed_identifier;
    uint8_t readable;
    uint8_t backing_valid;
    uint8_t name_readable;
    uint8_t reserved;
    char text[RESOURCE_NAME_TEXT_CAPACITY];
} NativeResourceNameEvidence;

typedef struct TalosOpcodeContext {
    struct TalosOpcodeContext *previous;
    uint64_t thread_token;
    uint32_t operand_offset;
    uint32_t binding_hash;
    uint32_t native_thread_id;
    uint32_t runtime_generation;
    uint32_t source_thread_generation;
    uint32_t matched_load_void_task_generation;
    uint32_t set_zone_task_generation;
    uint32_t set_zone_settle_session_generation;
    SudekiMpTalosNativeLifecycleEvent event;
    uint8_t opcode;
    uint8_t exact;
    uint8_t set_zone_before_seen;
    uint8_t lineage_matched;
    uint8_t reserved;
    char zone_name[ZONE_NAME_CAPACITY];
} TalosOpcodeContext;

typedef struct TalosTaskLineage {
    uint64_t source_thread_token;
    uint64_t load_void_task_token;
    uint64_t load_void_thread_token;
    uint64_t runtime_token;
    uint64_t bytecode_token;
    uint32_t source_thread_generation;
    uint32_t load_void_task_generation;
    uint32_t load_void_thread_generation;
    uint32_t runtime_generation;
    uint32_t native_thread_id;
    uint8_t valid;
    uint8_t reserved[3];
} TalosTaskLineage;

static const uint8_t script_call_opcode_signature[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x81u, 0xecu,
    0x90u, 0x00u, 0x00u, 0x00u, 0x53u, 0x55u, 0x56u, 0x57u,
    0x8bu, 0xe9u
};
static const uint8_t script_scene_opcode_signature[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x83u, 0xecu,
    0x18u, 0x53u, 0x55u, 0x56u, 0x57u, 0x8bu, 0xf9u,
    0x8bu, 0x47u, 0x0cu
};
static const uint8_t script_scene_task_constructor_window[] = {
    0x8bu, 0xc3u, 0xe8u, 0xb3u, 0xe3u, 0xffu, 0xffu,
    0x8bu, 0x74u, 0x24u, 0x10u, 0x85u, 0xf6u, 0x74u, 0x3cu
};
static const uint8_t kazel_group_add_call_prefix[] = {
    0x8bu, 0x4cu, 0x24u, 0x18u, 0xa1u
};
static const uint8_t kazel_group_add_call_suffix[] = {
    0x51u, 0xe8u, 0xa0u, 0x1cu, 0xf7u, 0xffu, 0xebu, 0x1bu
};
static const uint8_t raw_group_add_entry[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x8bu, 0x55u,
    0x08u, 0x83u, 0xecu, 0x14u, 0x53u, 0x56u, 0x57u, 0x8bu,
    0xf0u
};
static const uint8_t ai_listener_add_prefix[] = {
    0x8bu, 0x54u, 0x24u, 0x04u, 0x56u, 0x85u, 0xd2u, 0x74u,
    0x10u, 0x51u, 0x8bu, 0xc4u, 0x8du, 0xb1u, 0xb0u, 0x00u,
    0x00u, 0x00u, 0x89u, 0x10u
};
static const uint8_t raw_formation_add_entry[] = {
    0x51u, 0x8bu, 0x4eu, 0x30u, 0x8bu, 0x54u, 0x24u, 0x08u,
    0x33u, 0xc0u, 0x57u, 0x85u, 0xc9u, 0x7eu, 0x0eu
};
static const uint8_t delete_pc_signature[] = {
    0x83u, 0xecu, 0x0cu, 0x56u, 0x8bu, 0x74u, 0x24u, 0x14u
};
static const uint8_t remove_all_players_tail[] = {
    0x85u, 0xc0u, 0x74u, 0x06u, 0x50u, 0xe8u,
    0x71u, 0xffu, 0xffu, 0xffu, 0xc3u
};
static const uint8_t formation_pop_members_tail[] = {
    0x85u, 0xc0u, 0x74u, 0x10u, 0x05u, 0xf4u, 0x00u, 0x00u,
    0x00u, 0x74u, 0x09u, 0x6au, 0x00u, 0x6au, 0x00u
};
static const uint8_t tsa_is_playing_tail[] = {
    0x85u, 0xc0u, 0x74u, 0x04u, 0x8au, 0x40u, 0x74u, 0xc3u,
    0x32u, 0xc0u, 0xc3u
};
static const uint8_t tsa_set_playing_tail[] = {
    0x8au, 0x4cu, 0x24u, 0x04u, 0x83u, 0xecu, 0x0cu, 0x85u,
    0xc0u, 0x74u, 0x03u, 0x88u, 0x48u, 0x74u, 0x33u, 0xc0u,
    0x84u, 0xc9u, 0x0fu, 0x95u, 0xc0u, 0x56u, 0x8bu, 0xf0u
};
static const uint8_t tsa_set_playing_after_shadow[] = {
    0x3bu, 0xf0u, 0x74u, 0x34u, 0x83u, 0xf8u, 0x01u,
    0x75u, 0x29u, 0x85u, 0xf6u, 0x75u, 0x25u
};
static const uint8_t tsa_set_playing_dispatch_body[] = {
    0x89u, 0x44u, 0x24u, 0x0cu, 0x51u, 0x8du, 0x44u,
    0x24u, 0x0cu, 0xc7u, 0x44u, 0x24u, 0x0cu, 0x09u,
    0x00u, 0x00u, 0x00u, 0x89u, 0x74u, 0x24u, 0x14u
};
static const uint8_t tsa_set_playing_suffix[] = {
    0x5eu, 0x83u, 0xc4u, 0x0cu, 0xc3u, 0xccu
};
static const HeroVtableEvidence hero_vtable_evidence[NATIVE_MEMBER_LIMIT] = {
    {
        0x002d5010u, 0x002d5034u, 0x002d5054u,
        0x002fd4e4u, 0x002fd4d0u, 0x002fd4bcu,
        0x0035a8fcu, 0x00139ad0u, 0x23u, ".?AVTalEntity@@"
    },
    {
        0x002d555cu, 0x002d5580u, 0x002d55a0u,
        0x002fe500u, 0x002fe4ecu, 0x002fe4d8u,
        0x0035ad34u, 0x001e8240u, 0x01u, ".?AVAilishEntity@@"
    },
    {
        0x002d5a88u, 0x002d5aacu, 0x002d5accu,
        0x002fec4cu, 0x002fec38u, 0x002fec24u,
        0x0035af80u, 0x0022c0e0u, 0x05u, ".?AVBukiEntity@@"
    },
    {
        0x002d66fcu, 0x002d6720u, 0x002d6740u,
        0x002ff718u, 0x002ff704u, 0x002ff6f0u,
        0x0035b1d4u, 0x0014d730u, 0x0eu, ".?AVElcoEntity@@"
    }
};
static const HeroVtableEvidence dark_tal_vtable_evidence = {
    0x002d6884u, 0x002d68a8u, 0x002d68c8u,
    0x002ff864u, 0x002ff850u, 0x002ff83cu,
    0x0035b238u, 0x00151230u, 0x0bu, ".?AVDarkTalEntity@@"
};

static uint8_t *game_base;
static HMODULE pinned_module;
static SudekiMpPointerHook script_call_opcode_hook;
static SudekiMpPointerHook script_scene_opcode_hook;
static SudekiMpRelativeCallHook script_scene_task_constructor_hook;
static SudekiMpRelativeCallHook kazel_group_add_call_hook;
static SudekiMpInlineHook delete_pc_hook;
static SudekiMpInlineHook remove_all_players_hook;
static SudekiMpInlineHook formation_pop_members_hook;
static SudekiMpInlineHook tsa_set_playing_hook;
static ScriptOpcodeFunction original_script_call_opcode;
static ScriptOpcodeFunction original_script_scene_opcode;
static ScriptTaskConstructorFunction original_script_task_constructor
    __attribute__((used));
static RawGroupAddFunction original_raw_group_add __attribute__((used));
static DeletePcFunction original_delete_pc;
static RemoveAllPlayersFunction original_remove_all_players;
static FormationPopMembersFunction original_formation_pop_members;
static TsaIsPlayingFunction tsa_is_playing;
static TsaSetPlayingFunction original_tsa_set_playing;
static DWORD opcode_27_tls = TLS_OUT_OF_INDEXES;
static DWORD opcode_29_tls = TLS_OUT_OF_INDEXES;
static SudekiMpTalosNativeLifecycleSnapshot lifecycle_snapshot;
static TalosTaskLineage task_lineage;
static volatile LONG task_lineage_writer_lock;
static volatile LONG task_lineage_sequence;
static uint64_t identity_token_key;
static uint64_t observed_runtime_token;
static uint64_t observed_bytecode_token;
static RosterIdentityTracker roster_identity_tracker;
static SudekiMpTalosNativeRosterIdentitySnapshot roster_identity_snapshot;
static volatile LONG roster_identity_snapshot_sequence;
static volatile LONG roster_identity_writer_lock;
static SettleEvidenceTracker settle_evidence_tracker;
static SudekiMpTalosNativeSettleEvidenceSnapshot settle_evidence_snapshot;
static volatile LONG settle_evidence_snapshot_sequence;
static volatile LONG settle_evidence_writer_lock;
static KazelLifecycleTracker kazel_lifecycle_tracker;
static SudekiMpTalosNativeKazelSnapshot kazel_lifecycle_snapshot;
static volatile LONG kazel_lifecycle_snapshot_sequence;
static volatile LONG kazel_lifecycle_writer_lock;
/* Deliberately process-lifetime state. Uninstall clears copied lifecycle
 * observations, but never resets a claimed/quarantined restoration attempt. */
static PostMovieRestoreTicketTracker post_movie_restore_ticket_tracker;
static volatile LONG post_movie_restore_ticket_writer_lock;
/* Set on the first authenticated LoadVoid session and never cleared. This is
 * the process-lifetime fence behind the public one-shot contract; hook
 * teardown/reinstall cannot create a second restoration attempt. */
static volatile LONG post_movie_restore_process_session_started;

static void publish_roster_identity(
    const NativeRosterSample *group,
    const NativeRosterSample *formation,
    BOOL mapping_valid
);
static void quarantine_roster_sequence(
    uint8_t reason,
    const NativeRosterSample *group,
    const NativeRosterSample *formation
);
static void invalidate_settle_evidence(BOOL clear_void_completion);
static void begin_kazel_lifecycle_session(void);
static void observe_kazel_opcode_edge(
    const TalosOpcodeContext *context,
    BOOL before
);
static BOOL roster_identity_sets_equal(
    const NativeRosterSample *left,
    const NativeRosterSample *right
);
static uint64_t roster_single_added_token(
    const NativeRosterSample *before,
    const NativeRosterSample *after
);
static BOOL classify_native_kazel_actor(const void *actor);
static NativeRosterSample sample_native_roster(
    uint32_t owner_global_rva,
    uint32_t members_offset,
    uint32_t member_stride,
    uint32_t count_offset
);
static uint32_t next_nonzero(uint32_t value);
static void try_arm_post_movie_restore_ticket(void);

static void acquire_post_movie_restore_ticket_writer(void) {
    while (InterlockedCompareExchange(
            &post_movie_restore_ticket_writer_lock, 1, 0) != 0) {
        Sleep(0u);
    }
}

static void release_post_movie_restore_ticket_writer(void) {
    MemoryBarrier();
    (void)InterlockedExchange(&post_movie_restore_ticket_writer_lock, 0);
}

static void reset_post_movie_restore_ticket_for_fresh_install(void) {
    BOOL process_session_started = InterlockedCompareExchange(
        &post_movie_restore_process_session_started, 0, 0) != 0;

    /* game_base==NULL and unowned opcode slots are checked by the caller, so
     * no observer can be inside this lock at a fresh startup boundary. */
    (void)InterlockedExchange(&post_movie_restore_ticket_writer_lock, 0);
    memset(&post_movie_restore_ticket_tracker, 0,
        sizeof(post_movie_restore_ticket_tracker));
    if (process_session_started) {
        post_movie_restore_ticket_tracker.state =
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_QUARANTINED;
        post_movie_restore_ticket_tracker.load_void_session_count = 1u;
    }
}

static void clear_post_movie_restore_ticket_after_teardown(void) {
    BOOL process_session_started = InterlockedCompareExchange(
        &post_movie_restore_process_session_started, 0, 0) != 0;

    acquire_post_movie_restore_ticket_writer();
    memset(&post_movie_restore_ticket_tracker, 0,
        sizeof(post_movie_restore_ticket_tracker));
    if (process_session_started) {
        post_movie_restore_ticket_tracker.state =
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_QUARANTINED;
        post_movie_restore_ticket_tracker.load_void_session_count = 1u;
    }
    release_post_movie_restore_ticket_writer();
}

static void quarantine_post_movie_restore_ticket_for_teardown(void) {
    BOOL quarantined = FALSE;

    acquire_post_movie_restore_ticket_writer();
    if (post_movie_restore_ticket_tracker.state ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_ACTIVE ||
        post_movie_restore_ticket_tracker.state ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_READY) {
        post_movie_restore_ticket_tracker.state =
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_QUARANTINED;
        (void)InterlockedExchange(
            &post_movie_restore_process_session_started, 1);
        quarantined = TRUE;
    }
    release_post_movie_restore_ticket_writer();
    if (quarantined) {
        SudekiMpLogWrite(
            "talos_post_movie_restore_ticket state=quarantined "
            "reason=lifecycle_teardown_started\r\n");
    }
}

static void configure_post_movie_restore_ticket(
    BOOL allow_post_movie_restore_ticket,
    BOOL exact_asset_authenticated
) {
    BOOL effective = allow_post_movie_restore_ticket &&
        exact_asset_authenticated;

    acquire_post_movie_restore_ticket_writer();
    if (effective && InterlockedCompareExchange(
            &post_movie_restore_process_session_started, 0, 0) == 0 &&
        post_movie_restore_ticket_tracker.state ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_DISABLED &&
        post_movie_restore_ticket_tracker.load_void_session_count == 0u) {
        post_movie_restore_ticket_tracker.allow_ticket = 1u;
        post_movie_restore_ticket_tracker.exact_asset_authenticated = 1u;
        post_movie_restore_ticket_tracker.state =
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_WAITING;
    }
    release_post_movie_restore_ticket_writer();
}

static void observe_post_movie_restore_load_void(void) {
    uint8_t prior_state;
    uint8_t next_state;
    uint32_t session_count;

    acquire_post_movie_restore_ticket_writer();
    prior_state = post_movie_restore_ticket_tracker.state;
    next_state = prior_state;
    if (post_movie_restore_ticket_tracker.allow_ticket != 0u &&
        post_movie_restore_ticket_tracker.exact_asset_authenticated != 0u) {
        if (prior_state ==
                SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_WAITING &&
            post_movie_restore_ticket_tracker.load_void_session_count == 0u) {
            post_movie_restore_ticket_tracker.load_void_session_count = 1u;
            (void)InterlockedExchange(
                &post_movie_restore_process_session_started, 1);
            next_state = SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_ACTIVE;
        } else if (prior_state ==
                SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_ACTIVE ||
            prior_state ==
                SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_READY) {
            post_movie_restore_ticket_tracker.load_void_session_count =
                next_nonzero(
                    post_movie_restore_ticket_tracker.
                        load_void_session_count);
            next_state =
                SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_QUARANTINED;
        }
        post_movie_restore_ticket_tracker.state = next_state;
    }
    session_count =
        post_movie_restore_ticket_tracker.load_void_session_count;
    release_post_movie_restore_ticket_writer();
    if (prior_state != next_state) {
        SudekiMpLogFormat(
            "talos_post_movie_restore_ticket event=load_void "
            "prior_state=%lu state=%lu session_count=%lu "
            "policy=fresh_process_one_shot\r\n",
            (unsigned long)prior_state,
            (unsigned long)next_state,
            (unsigned long)session_count);
    }
}

static void observe_post_movie_restore_runtime_change(void) {
    BOOL quarantined = FALSE;

    acquire_post_movie_restore_ticket_writer();
    if (post_movie_restore_ticket_tracker.state ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_ACTIVE ||
        post_movie_restore_ticket_tracker.state ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_READY) {
        post_movie_restore_ticket_tracker.state =
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_QUARANTINED;
        quarantined = TRUE;
    }
    release_post_movie_restore_ticket_writer();
    if (quarantined) {
        SudekiMpLogWrite(
            "talos_post_movie_restore_ticket state=quarantined "
            "reason=runtime_changed_after_session_started\r\n");
    }
}

static void acquire_roster_identity_writer(void) {
    while (InterlockedCompareExchange(
            &roster_identity_writer_lock, 1, 0) != 0) {
        Sleep(0u);
    }
}

static void release_roster_identity_writer(void) {
    MemoryBarrier();
    (void)InterlockedExchange(&roster_identity_writer_lock, 0);
}

static BOOL readable_memory(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    return end >= start && end <= region_end;
}

static uint32_t next_nonzero(uint32_t value) {
    ++value;
    if (value == 0u) ++value;
    return value;
}

static void acquire_settle_evidence_writer(void) {
    while (InterlockedCompareExchange(
            &settle_evidence_writer_lock, 1, 0) != 0) {
        Sleep(0u);
    }
}

static void release_settle_evidence_writer(void) {
    MemoryBarrier();
    (void)InterlockedExchange(&settle_evidence_writer_lock, 0);
}

static void begin_task_lineage_write(void) {
    while (InterlockedCompareExchange(
            &task_lineage_writer_lock, 1, 0) != 0) {
        Sleep(0u);
    }
    (void)InterlockedIncrement(&task_lineage_sequence);
    MemoryBarrier();
}

static void end_task_lineage_write(void) {
    MemoryBarrier();
    (void)InterlockedIncrement(&task_lineage_sequence);
    (void)InterlockedExchange(&task_lineage_writer_lock, 0);
}

static BOOL copy_task_lineage(TalosTaskLineage *copy) {
    unsigned int attempt;

    if (copy == NULL) return FALSE;
    for (attempt = 0u; attempt < 32u; ++attempt) {
        LONG before = InterlockedCompareExchange(
            &task_lineage_sequence, 0, 0);
        LONG after;

        if ((before & 1) != 0) {
            Sleep(0u);
            continue;
        }
        MemoryBarrier();
        *copy = task_lineage;
        MemoryBarrier();
        after = InterlockedCompareExchange(&task_lineage_sequence, 0, 0);
        if (before == after && (after & 1) == 0) return TRUE;
    }
    memset(copy, 0, sizeof(*copy));
    return FALSE;
}

static void publish_settle_evidence_locked(void) {
    SudekiMpTalosNativeSettleEvidenceSnapshot snapshot;

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.session_generation = settle_evidence_tracker.session_generation;
    snapshot.script_runtime_generation =
        settle_evidence_tracker.script_runtime_generation;
    snapshot.load_void_task_generation =
        settle_evidence_tracker.load_void_task_generation;
    snapshot.camera_observation_generation =
        settle_evidence_tracker.camera_observation_generation;
    snapshot.default_camera_generation =
        settle_evidence_tracker.default_camera_generation;
    snapshot.settle_validation_generation =
        settle_evidence_tracker.settle_validation_generation;
    snapshot.void_set_zone_completed =
        settle_evidence_tracker.void_set_zone_completed;
    snapshot.default_camera_committed =
        settle_evidence_tracker.default_camera_committed;
    snapshot.default_camera_revalidated =
        settle_evidence_tracker.default_camera_revalidated;
    snapshot.tal_control_revalidated =
        settle_evidence_tracker.tal_control_revalidated;
    snapshot.settle_evidence_complete =
        settle_evidence_tracker.settle_evidence_complete;

    (void)InterlockedIncrement(&settle_evidence_snapshot_sequence);
    MemoryBarrier();
    settle_evidence_snapshot = snapshot;
    MemoryBarrier();
    (void)InterlockedIncrement(&settle_evidence_snapshot_sequence);
}

static void begin_settle_evidence_session(uint32_t native_thread_id) {
    uint32_t next_session;

    acquire_settle_evidence_writer();
    next_session = next_nonzero(settle_evidence_tracker.session_generation);
    memset(&settle_evidence_tracker, 0, sizeof(settle_evidence_tracker));
    settle_evidence_tracker.session_generation = next_session;
    settle_evidence_tracker.script_runtime_generation =
        lifecycle_snapshot.script_runtime_generation;
    settle_evidence_tracker.native_thread_id = native_thread_id;
    publish_settle_evidence_locked();
    release_settle_evidence_writer();
}

static void invalidate_settle_evidence(BOOL clear_void_completion) {
    acquire_settle_evidence_writer();
    if (settle_evidence_tracker.session_generation == 0u) {
        release_settle_evidence_writer();
        return;
    }
    settle_evidence_tracker.camera_observation_generation = next_nonzero(
        settle_evidence_tracker.camera_observation_generation);
    settle_evidence_tracker.default_camera_committed = 0u;
    settle_evidence_tracker.default_camera_revalidated = 0u;
    settle_evidence_tracker.tal_control_revalidated = 0u;
    settle_evidence_tracker.settle_evidence_complete = 0u;
    if (clear_void_completion) {
        settle_evidence_tracker.void_set_zone_completed = 0u;
        settle_evidence_tracker.script_runtime_generation = 0u;
        settle_evidence_tracker.load_void_task_generation = 0u;
    }
    publish_settle_evidence_locked();
    release_settle_evidence_writer();
}

static void clear_settle_evidence_state(void) {
    acquire_settle_evidence_writer();
    (void)InterlockedIncrement(&settle_evidence_snapshot_sequence);
    MemoryBarrier();
    memset(&settle_evidence_tracker, 0, sizeof(settle_evidence_tracker));
    memset(&settle_evidence_snapshot, 0, sizeof(settle_evidence_snapshot));
    MemoryBarrier();
    (void)InterlockedIncrement(&settle_evidence_snapshot_sequence);
    release_settle_evidence_writer();
}

static void acquire_kazel_lifecycle_writer(void) {
    while (InterlockedCompareExchange(
            &kazel_lifecycle_writer_lock, 1, 0) != 0) {
        Sleep(0u);
    }
}

static void release_kazel_lifecycle_writer(void) {
    MemoryBarrier();
    (void)InterlockedExchange(&kazel_lifecycle_writer_lock, 0);
}

static void publish_kazel_lifecycle_locked(void) {
    SudekiMpTalosNativeKazelSnapshot snapshot;

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.session_generation = kazel_lifecycle_tracker.session_generation;
    snapshot.observation_serial = kazel_lifecycle_tracker.observation_serial;
    snapshot.request_generation = kazel_lifecycle_tracker.request_generation;
    snapshot.script_runtime_generation =
        kazel_lifecycle_tracker.script_runtime_generation;
    snapshot.load_void_task_generation =
        kazel_lifecycle_tracker.load_void_task_generation;
    snapshot.source_native_thread_id =
        kazel_lifecycle_tracker.source_native_thread_id;
    snapshot.completion_native_thread_id =
        kazel_lifecycle_tracker.completion_native_thread_id;
    snapshot.original_tal_token = kazel_lifecycle_tracker.original_tal_token;
    snapshot.kazel_token = kazel_lifecycle_tracker.kazel_token;
    snapshot.state = kazel_lifecycle_tracker.state;
    snapshot.spawn_binding_before_seen =
        kazel_lifecycle_tracker.spawn_binding_before_seen;
    snapshot.spawn_binding_after_seen =
        kazel_lifecycle_tracker.spawn_binding_after_seen;
    snapshot.group_add_before_seen =
        kazel_lifecycle_tracker.group_add_before_seen;
    snapshot.group_add_after_seen =
        kazel_lifecycle_tracker.group_add_after_seen;
    snapshot.exact_dark_tal_identity =
        kazel_lifecycle_tracker.exact_dark_tal_identity;
    snapshot.group_add_corroborated =
        kazel_lifecycle_tracker.group_add_corroborated;
    snapshot.delete_corroborated =
        kazel_lifecycle_tracker.delete_corroborated;
    snapshot.completion_was_synchronous =
        kazel_lifecycle_tracker.completion_during_spawn_binding;
    snapshot.serialized_opcode_mask =
        kazel_lifecycle_tracker.serialized_opcode_mask;
    snapshot.ambiguity_reason = kazel_lifecycle_tracker.ambiguity_reason;
    snapshot.actor_lifetime_authority_proven = 0u;

    (void)InterlockedIncrement(&kazel_lifecycle_snapshot_sequence);
    MemoryBarrier();
    kazel_lifecycle_snapshot = snapshot;
    MemoryBarrier();
    (void)InterlockedIncrement(&kazel_lifecycle_snapshot_sequence);
}

static void begin_kazel_lifecycle_session(void) {
    uint32_t session_generation;
    uint32_t request_generation;
    BOOL first_process_session;

    acquire_kazel_lifecycle_writer();
    first_process_session =
        SudekiMpTalosNativeLifecycleKazelSessionStartPolicy(
            kazel_lifecycle_tracker.session_generation);
    session_generation = next_nonzero(
        kazel_lifecycle_tracker.session_generation);
    request_generation = kazel_lifecycle_tracker.request_generation;
    memset(&kazel_lifecycle_tracker, 0, sizeof(kazel_lifecycle_tracker));
    kazel_lifecycle_tracker.session_generation = session_generation;
    kazel_lifecycle_tracker.request_generation = request_generation;
    if (!first_process_session) {
        kazel_lifecycle_tracker.state =
            SUDEKIMP_TALOS_NATIVE_KAZEL_QUARANTINED;
        kazel_lifecycle_tracker.ambiguity_reason = 8u;
        kazel_lifecycle_tracker.observation_serial = 1u;
    }
    publish_kazel_lifecycle_locked();
    release_kazel_lifecycle_writer();
}

static void clear_kazel_lifecycle_state(void) {
    acquire_kazel_lifecycle_writer();
    (void)InterlockedIncrement(&kazel_lifecycle_snapshot_sequence);
    MemoryBarrier();
    memset(&kazel_lifecycle_tracker, 0, sizeof(kazel_lifecycle_tracker));
    memset(&kazel_lifecycle_snapshot, 0, sizeof(kazel_lifecycle_snapshot));
    MemoryBarrier();
    (void)InterlockedIncrement(&kazel_lifecycle_snapshot_sequence);
    release_kazel_lifecycle_writer();
}

static uint64_t mix_identity_value(uint64_t value) {
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30u)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27u)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31u);
}

/* Raw engine addresses never leave a stack frame or enter a public snapshot.
 * Tokens are salted with a private per-process value that is never logged or
 * serialized. They exist only for equality correlation and are not an address
 * encoding. */
static uint64_t opaque_identity_token(const void *pointer) {
    uint64_t value;

    if (pointer == NULL || identity_token_key == 0u) return 0u;
    value = mix_identity_value(
        (uint64_t)(uintptr_t)pointer ^ identity_token_key);
    return value == 0u ? UINT64_C(1) : value;
}

static uint32_t observe_runtime_generation(
    const void *runtime,
    const void *bytecode
) {
    uint64_t runtime_token = opaque_identity_token(runtime);
    uint64_t bytecode_token = opaque_identity_token(bytecode);

    if (runtime_token == 0u || bytecode_token == 0u) return 0u;
    if (runtime_token != observed_runtime_token ||
        bytecode_token != observed_bytecode_token) {
        observe_post_movie_restore_runtime_change();
        observed_runtime_token = runtime_token;
        observed_bytecode_token = bytecode_token;
        lifecycle_snapshot.script_runtime_generation = next_nonzero(
            lifecycle_snapshot.script_runtime_generation);
        lifecycle_snapshot.tsa_completion_armed = 0u;
        lifecycle_snapshot.tsa_inactive_observed = 0u;
        lifecycle_snapshot.tsa_inactive_caller_rva = 0u;
        invalidate_settle_evidence(TRUE);
        if (roster_identity_tracker.writer_native_thread_id != 0u &&
            roster_identity_tracker.writer_native_thread_id ==
                GetCurrentThreadId() &&
            roster_identity_tracker.sequence_state !=
                SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_RELEASED &&
            roster_identity_tracker.sequence_state !=
                SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_QUARANTINED) {
            quarantine_roster_sequence(5u, NULL, NULL);
        }
        begin_task_lineage_write();
        memset(&task_lineage, 0, sizeof(task_lineage));
        end_task_lineage_write();
        lifecycle_snapshot.load_void_task_bound = 0u;
        lifecycle_snapshot.load_void_descendant_observed = 0u;
    }
    return lifecycle_snapshot.script_runtime_generation;
}

static const char *event_name(SudekiMpTalosNativeLifecycleEvent event) {
    switch (event) {
    case SUDEKIMP_TALOS_NATIVE_EVENT_LOAD_VOID: return "load_void";
    case SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_BUKI: return "delete_buki";
    case SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_AILISH: return "delete_ailish";
    case SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_ELCO: return "delete_elco";
    case SUDEKIMP_TALOS_NATIVE_EVENT_SET_ZONE_CARRIER:
        return "set_zone_carrier";
    case SUDEKIMP_TALOS_NATIVE_EVENT_SET_ZONE_NOW: return "set_zone_now";
    case SUDEKIMP_TALOS_NATIVE_EVENT_END_TSA: return "end_tsa";
    case SUDEKIMP_TALOS_NATIVE_EVENT_REMOVE_ALL_PLAYERS:
        return "remove_all_players";
    case SUDEKIMP_TALOS_NATIVE_EVENT_LOAD_VOID_TASK_CREATED:
        return "load_void_task_created";
    case SUDEKIMP_TALOS_NATIVE_EVENT_FORMATION_POP_MEMBERS:
        return "formation_pop_members";
    case SUDEKIMP_TALOS_NATIVE_EVENT_TSA_BECAME_INACTIVE:
        return "tsa_became_inactive";
    case SUDEKIMP_TALOS_NATIVE_EVENT_TAL_KAZEL_MERGE:
        return "tal_kazel_merge";
    case SUDEKIMP_TALOS_NATIVE_EVENT_SPAWN_KAZEL_WRAPPER:
        return "spawn_kazel_wrapper";
    case SUDEKIMP_TALOS_NATIVE_EVENT_INTERNAL_SPAWN_PC:
        return "internal_spawn_pc";
    case SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_KAZEL:
        return "delete_kazel";
    case SUDEKIMP_TALOS_NATIVE_EVENT_KAZEL_GROUP_ADD:
        return "kazel_group_add";
    default: return "none";
    }
}

SudekiMpTalosNativeLifecycleEvent
SudekiMpTalosNativeLifecycleClassify(
    uint8_t opcode,
    uint32_t operand_offset,
    uint32_t binding_hash
) {
    if (opcode == 0x29u && operand_offset == LOAD_VOID_OPERAND &&
        binding_hash == HASH_LOAD_VOID) {
        return SUDEKIMP_TALOS_NATIVE_EVENT_LOAD_VOID;
    }
    if (opcode != 0x27u) return SUDEKIMP_TALOS_NATIVE_EVENT_NONE;
    if (binding_hash == HASH_DELETE_PC) {
        if (operand_offset == DELETE_BUKI_OPERAND)
            return SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_BUKI;
        if (operand_offset == DELETE_AILISH_OPERAND)
            return SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_AILISH;
        if (operand_offset == DELETE_ELCO_OPERAND)
            return SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_ELCO;
        if (operand_offset == DELETE_KAZEL_OPERAND)
            return SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_KAZEL;
    }
    if (operand_offset == SET_ZONE_CARRIER_OPERAND &&
        binding_hash == HASH_SET_ZONE)
        return SUDEKIMP_TALOS_NATIVE_EVENT_SET_ZONE_CARRIER;
    if (operand_offset == SET_ZONE_NOW_OPERAND &&
        binding_hash == HASH_SET_ZONE_NOW)
        return SUDEKIMP_TALOS_NATIVE_EVENT_SET_ZONE_NOW;
    if (operand_offset == END_TSA_OPERAND && binding_hash == HASH_END_TSA)
        return SUDEKIMP_TALOS_NATIVE_EVENT_END_TSA;
    if (operand_offset == TAL_KAZEL_MERGE_OPERAND &&
        binding_hash == HASH_TAL_KAZEL_MERGE)
        return SUDEKIMP_TALOS_NATIVE_EVENT_TAL_KAZEL_MERGE;
    if (operand_offset == SPAWN_KAZEL_WRAPPER_OPERAND &&
        binding_hash == HASH_SPAWN_PC_WRAPPER)
        return SUDEKIMP_TALOS_NATIVE_EVENT_SPAWN_KAZEL_WRAPPER;
    if (operand_offset == INTERNAL_SPAWN_PC_OPERAND &&
        binding_hash == HASH_INTERNAL_SPAWN_PC)
        return SUDEKIMP_TALOS_NATIVE_EVENT_INTERNAL_SPAWN_PC;
    if (operand_offset == REMOVE_ALL_PLAYERS_OPERAND &&
        binding_hash == HASH_REMOVE_ALL_PLAYERS)
        return SUDEKIMP_TALOS_NATIVE_EVENT_REMOVE_ALL_PLAYERS;
    return SUDEKIMP_TALOS_NATIVE_EVENT_NONE;
}

SudekiMpTalosNativeHeroIdentity
SudekiMpTalosNativeLifecycleHeroFromVtableRvas(
    uint32_t main_vtable_rva,
    uint32_t secondary_vtable_rva,
    uint32_t resource_vtable_rva
) {
    unsigned int hero;

    for (hero = 0u; hero < NATIVE_MEMBER_LIMIT; ++hero) {
        const HeroVtableEvidence *evidence = &hero_vtable_evidence[hero];

        if (main_vtable_rva == evidence->main_vtable_rva &&
            secondary_vtable_rva == evidence->secondary_vtable_rva &&
            resource_vtable_rva == evidence->resource_vtable_rva) {
            return (SudekiMpTalosNativeHeroIdentity)hero;
        }
    }
    return SUDEKIMP_TALOS_NATIVE_HERO_UNKNOWN;
}

BOOL SudekiMpTalosNativeLifecycleKazelFromVtableRvas(
    uint32_t main_vtable_rva,
    uint32_t secondary_vtable_rva,
    uint32_t resource_vtable_rva
) {
    return main_vtable_rva == dark_tal_vtable_evidence.main_vtable_rva &&
        secondary_vtable_rva ==
            dark_tal_vtable_evidence.secondary_vtable_rva &&
        resource_vtable_rva ==
            dark_tal_vtable_evidence.resource_vtable_rva;
}

static BOOL read_opcode_evidence(
    void *thread,
    uint8_t expected_opcode,
    TalosOpcodeContext *context
) {
    TalosTaskLineage lineage;
    uint8_t *runtime;
    uint8_t *bytecode;
    uint32_t operand_offset;
    uint32_t binding_hash;
    uint8_t opcode;

    if (game_base == NULL || thread == NULL || context == NULL ||
        !readable_memory((uint8_t *)thread + SCRIPT_THREAD_INSTRUCTION_OFFSET,
            sizeof(operand_offset)) ||
        !readable_memory(game_base + RVA_SCRIPT_RUNTIME_GLOBAL,
            sizeof(runtime))) return FALSE;
    runtime = *(uint8_t **)(game_base + RVA_SCRIPT_RUNTIME_GLOBAL);
    if (!readable_memory(runtime,
            SCRIPT_RUNTIME_BYTECODE_OFFSET + sizeof(bytecode))) return FALSE;
    bytecode = *(uint8_t **)(runtime + SCRIPT_RUNTIME_BYTECODE_OFFSET);
    memcpy(&operand_offset,
        (uint8_t *)thread + SCRIPT_THREAD_INSTRUCTION_OFFSET,
        sizeof(operand_offset));
    if (operand_offset == 0u ||
        !readable_memory(bytecode + operand_offset - 1u, 5u)) return FALSE;
    opcode = bytecode[operand_offset - 1u];
    memcpy(&binding_hash, bytecode + operand_offset, sizeof(binding_hash));
    memset(context, 0, sizeof(*context));
    context->thread_token = opaque_identity_token(thread);
    context->operand_offset = operand_offset;
    context->binding_hash = binding_hash;
    context->native_thread_id = GetCurrentThreadId();
    context->opcode = opcode;
    context->runtime_generation = observe_runtime_generation(
        runtime, bytecode);
    context->exact = opcode == expected_opcode;
    context->event = context->exact ?
        SudekiMpTalosNativeLifecycleClassify(
            opcode, operand_offset, binding_hash) :
        SUDEKIMP_TALOS_NATIVE_EVENT_NONE;
    memset(&lineage, 0, sizeof(lineage));
    context->lineage_matched = copy_task_lineage(&lineage) &&
        lineage.valid &&
        context->runtime_generation == lineage.runtime_generation &&
        context->thread_token == lineage.load_void_thread_token;
    if (context->lineage_matched) {
        context->matched_load_void_task_generation =
            lineage.load_void_task_generation;
    }
    if (context->event != SUDEKIMP_TALOS_NATIVE_EVENT_NONE &&
        context->lineage_matched) {
        lifecycle_snapshot.load_void_descendant_observed = 1u;
    }
    return TRUE;
}

static void record_opcode_edge(
    const TalosOpcodeContext *context,
    const char *phase,
    int handler_result
) {
    BOOL tsa_playing = FALSE;

    if (context == NULL || context->event == SUDEKIMP_TALOS_NATIVE_EVENT_NONE)
        return;
    if (tsa_is_playing != NULL) tsa_playing = tsa_is_playing() != 0u;
    if (context->event == SUDEKIMP_TALOS_NATIVE_EVENT_END_TSA &&
        context->lineage_matched && phase[0] == 'b') {
        lifecycle_snapshot.tsa_completion_armed = 1u;
        lifecycle_snapshot.tsa_inactive_observed = 0u;
        lifecycle_snapshot.tsa_inactive_caller_rva = 0u;
    }
    lifecycle_snapshot.event_serial =
        next_nonzero(lifecycle_snapshot.event_serial);
    lifecycle_snapshot.last_native_thread_id = context->native_thread_id;
    lifecycle_snapshot.last_operand_offset = context->operand_offset;
    lifecycle_snapshot.last_binding_hash = context->binding_hash;
    lifecycle_snapshot.last_handler_result = (uint32_t)handler_result;
    lifecycle_snapshot.last_tsa_playing = tsa_playing ? 1u : 0u;
    lifecycle_snapshot.observed_event_mask |=
        (uint32_t)1u << (unsigned int)context->event;
    SudekiMpLogFormat(
        "talos_lifecycle event=sol_opcode phase=%s run=%08lx%08lx "
        "serial=%lu kind=%s opcode=0x%02lx operand=0x%08lx "
        "logical_opcode=0x%08lx hash=0x%08lx "
        "native_thread=%lu runtime_generation=%lu lineage_matched=%s "
        "tsa_playing=%s "
        "handler_result=%ld authority=unproven "
        "policy=observation_only_original_called_once_no_mutation\r\n",
        phase,
        (unsigned long)lifecycle_snapshot.run_id_high,
        (unsigned long)lifecycle_snapshot.run_id_low,
        (unsigned long)lifecycle_snapshot.event_serial,
        event_name(context->event),
        (unsigned long)context->opcode,
        (unsigned long)context->operand_offset,
        (unsigned long)(context->operand_offset - 1u),
        (unsigned long)context->binding_hash,
        (unsigned long)context->native_thread_id,
        (unsigned long)context->runtime_generation,
        context->lineage_matched ? "true" : "false",
        tsa_playing ? "true" : "false",
        (long)handler_result);
}

static int observe_opcode(
    void *thread,
    void *ignored_edx,
    uint8_t opcode,
    DWORD tls_index,
    ScriptOpcodeFunction original
) {
    TalosOpcodeContext context;
    TalosOpcodeContext *previous = NULL;
    DWORD incoming_error = GetLastError();
    DWORD result_error;
    int result;

    if (tls_index != TLS_OUT_OF_INDEXES)
        previous = (TalosOpcodeContext *)TlsGetValue(tls_index);
    if (!read_opcode_evidence(thread, opcode, &context)) {
        memset(&context, 0, sizeof(context));
        ++lifecycle_snapshot.rejected_observation_count;
    }
    if (context.event == SUDEKIMP_TALOS_NATIVE_EVENT_LOAD_VOID) {
        observe_post_movie_restore_load_void();
        begin_task_lineage_write();
        memset(&task_lineage, 0, sizeof(task_lineage));
        lifecycle_snapshot.source_thread_generation = next_nonzero(
            lifecycle_snapshot.source_thread_generation);
        lifecycle_snapshot.load_void_task_bound = 0u;
        lifecycle_snapshot.load_void_descendant_observed = 0u;
        lifecycle_snapshot.tsa_completion_armed = 0u;
        lifecycle_snapshot.tsa_inactive_observed = 0u;
        lifecycle_snapshot.tsa_inactive_caller_rva = 0u;
        roster_identity_tracker.sequence_state =
            SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_IDLE;
        roster_identity_tracker.writer_native_thread_id =
            context.native_thread_id;
        roster_identity_tracker.quarantine_reason = 0u;
        roster_identity_tracker.delete_delta_corroborated_mask = 0u;
        memset(roster_identity_tracker.sequence_token, 0,
            sizeof(roster_identity_tracker.sequence_token));
        roster_identity_tracker.observation_serial = next_nonzero(
            roster_identity_tracker.observation_serial);
        publish_roster_identity(NULL, NULL, FALSE);
        begin_settle_evidence_session(context.native_thread_id);
        begin_kazel_lifecycle_session();
        context.source_thread_generation =
            lifecycle_snapshot.source_thread_generation;
        task_lineage.source_thread_token = context.thread_token;
        task_lineage.source_thread_generation =
            context.source_thread_generation;
        task_lineage.runtime_generation = context.runtime_generation;
        task_lineage.native_thread_id = context.native_thread_id;
        end_task_lineage_write();
    }
    context.previous = previous;
    if (tls_index != TLS_OUT_OF_INDEXES) (void)TlsSetValue(tls_index, &context);
    if (opcode == 0x27u) ++lifecycle_snapshot.opcode_27_depth;
    else ++lifecycle_snapshot.opcode_29_depth;
    observe_kazel_opcode_edge(&context, TRUE);
    record_opcode_edge(&context, "before", 0);
    SetLastError(incoming_error);
    result = original(thread, ignored_edx);
    result_error = GetLastError();
    observe_kazel_opcode_edge(&context, FALSE);
    record_opcode_edge(&context, "after", result);
    if (opcode == 0x27u) --lifecycle_snapshot.opcode_27_depth;
    else --lifecycle_snapshot.opcode_29_depth;
    if (tls_index != TLS_OUT_OF_INDEXES) (void)TlsSetValue(tls_index, previous);
    try_arm_post_movie_restore_ticket();
    SetLastError(result_error);
    return result;
}

static int SUDEKIMP_FASTCALL observe_script_call_opcode(
    void *thread,
    void *ignored_edx
) {
    return observe_opcode(thread, ignored_edx, 0x27u, opcode_27_tls,
        original_script_call_opcode);
}

static int SUDEKIMP_FASTCALL observe_script_scene_opcode(
    void *thread,
    void *ignored_edx
) {
    return observe_opcode(thread, ignored_edx, 0x29u, opcode_29_tls,
        original_script_scene_opcode);
}

static void __attribute__((noinline, used))
observe_script_task_constructor_return(
    uint32_t function_hash,
    void **out_task_cell
) {
    TalosOpcodeContext *source_context = NULL;
    void *task_handle = NULL;
    void *task_thread = NULL;
    uint32_t task_generation;
    uint32_t thread_generation;
    BOOL lineage_bound = FALSE;
    DWORD incoming_error = GetLastError();

    ++lifecycle_snapshot.task_constructor_return_count;
    if (opcode_29_tls != TLS_OUT_OF_INDEXES) {
        source_context = (TalosOpcodeContext *)TlsGetValue(opcode_29_tls);
    }
    if (source_context == NULL || !source_context->exact ||
        source_context->event != SUDEKIMP_TALOS_NATIVE_EVENT_LOAD_VOID ||
        source_context->binding_hash != function_hash ||
        source_context->runtime_generation == 0u ||
        source_context->native_thread_id != GetCurrentThreadId() ||
        !readable_memory(out_task_cell, sizeof(*out_task_cell))) {
        SetLastError(incoming_error);
        return;
    }
    task_handle = *out_task_cell;
    if (!readable_memory(task_handle, sizeof(task_thread))) {
        SetLastError(incoming_error);
        return;
    }
    task_thread = *(void **)task_handle;
    if (task_thread == NULL) {
        SetLastError(incoming_error);
        return;
    }

    begin_task_lineage_write();
    if (!task_lineage.valid &&
        source_context->source_thread_generation != 0u &&
        task_lineage.source_thread_token == source_context->thread_token &&
        task_lineage.source_thread_generation ==
            source_context->source_thread_generation &&
        task_lineage.runtime_generation ==
            source_context->runtime_generation &&
        task_lineage.native_thread_id == source_context->native_thread_id) {
        task_lineage.load_void_task_token = opaque_identity_token(task_handle);
        task_lineage.load_void_thread_token = opaque_identity_token(task_thread);
        task_lineage.runtime_token = observed_runtime_token;
        task_lineage.bytecode_token = observed_bytecode_token;
        task_lineage.load_void_task_generation = next_nonzero(
            lifecycle_snapshot.load_void_task_generation);
        task_lineage.load_void_thread_generation = next_nonzero(
            lifecycle_snapshot.load_void_thread_generation);
        task_lineage.valid = 1u;
        task_generation = task_lineage.load_void_task_generation;
        thread_generation = task_lineage.load_void_thread_generation;
        lineage_bound = TRUE;
    } else {
        task_generation = task_lineage.load_void_task_generation;
        thread_generation = task_lineage.load_void_thread_generation;
    }
    end_task_lineage_write();
    if (!lineage_bound) {
        ++lifecycle_snapshot.rejected_observation_count;
        SetLastError(incoming_error);
        return;
    }
    lifecycle_snapshot.load_void_task_generation = task_generation;
    lifecycle_snapshot.load_void_thread_generation = thread_generation;
    lifecycle_snapshot.load_void_task_bound = 1u;
    lifecycle_snapshot.event_serial = next_nonzero(
        lifecycle_snapshot.event_serial);
    lifecycle_snapshot.observed_event_mask |=
        (uint32_t)1u <<
        (unsigned int)SUDEKIMP_TALOS_NATIVE_EVENT_LOAD_VOID_TASK_CREATED;
    SudekiMpLogFormat(
        "talos_lifecycle event=task_constructor phase=after "
        "run=%08lx%08lx serial=%lu kind=load_void_task_created "
        "hash=0x%08lx native_thread=%lu runtime_generation=%lu "
        "source_generation=%lu task_generation=%lu "
        "thread_generation=%lu task_present=true thread_present=true "
        "authority=unproven policy=observation_only_engine_owned_no_retain\r\n",
        (unsigned long)lifecycle_snapshot.run_id_high,
        (unsigned long)lifecycle_snapshot.run_id_low,
        (unsigned long)lifecycle_snapshot.event_serial,
        (unsigned long)function_hash,
        (unsigned long)source_context->native_thread_id,
        (unsigned long)source_context->runtime_generation,
        (unsigned long)source_context->source_thread_generation,
        (unsigned long)task_generation,
        (unsigned long)thread_generation);
    SetLastError(incoming_error);
}

static BOOL active_group_argument_matches(const void *group) {
    void *active_group = NULL;

    if (game_base == NULL || group == NULL ||
        !readable_memory(game_base + RVA_ACTIVE_GROUP_GLOBAL,
            sizeof(active_group))) return FALSE;
    memcpy(&active_group, game_base + RVA_ACTIVE_GROUP_GLOBAL,
        sizeof(active_group));
    return active_group == group;
}

static BOOL exact_tal_only_member_sets(
    const NativeRosterSample *group,
    const NativeRosterSample *formation,
    uint64_t tal_token
) {
    return group != NULL && formation != NULL && tal_token != 0u &&
        group->structurally_valid && formation->structurally_valid &&
        group->count == 1u && formation->count == 1u &&
        group->occupied_mask == HERO_MASK_TAL &&
        formation->occupied_mask == HERO_MASK_TAL &&
        roster_identity_sets_equal(group, formation) &&
        group->member_tokens[0] == tal_token &&
        formation->member_tokens[0] == tal_token;
}

static void log_kazel_group_add_edge(
    const char *phase,
    BOOL tracked,
    BOOL exact_dark_tal,
    BOOL corroborated,
    uint64_t actor_token,
    const NativeRosterSample *group,
    const NativeRosterSample *formation,
    uint32_t observation_serial,
    uint32_t request_generation,
    uint32_t state
) {
    SudekiMpLogFormat(
        "talos_lifecycle event=KazelGroupAdd_native phase=%s "
        "run=%08lx%08lx serial=%lu request_generation=%lu "
        "tracked=%s exact_dark_tal=%s corroborated=%s state=%lu "
        "actor_token=%08lx%08lx group_count=%lu group_mask=0x%02lx "
        "formation_count=%lu formation_mask=0x%02lx sets_equal=%s "
        "native_thread=%lu actor_lifetime_authority=false "
        "policy=observation_only_original_called_once_no_mutation\r\n",
        phase,
        (unsigned long)lifecycle_snapshot.run_id_high,
        (unsigned long)lifecycle_snapshot.run_id_low,
        (unsigned long)observation_serial,
        (unsigned long)request_generation,
        tracked ? "true" : "false",
        exact_dark_tal ? "true" : "false",
        corroborated ? "true" : "false",
        (unsigned long)state,
        (unsigned long)(actor_token >> 32u),
        (unsigned long)actor_token,
        (unsigned long)group->count,
        (unsigned long)group->occupied_mask,
        (unsigned long)formation->count,
        (unsigned long)formation->occupied_mask,
        roster_identity_sets_equal(group, formation) ? "true" : "false",
        (unsigned long)GetCurrentThreadId());
}

static void __attribute__((noinline, used))
observe_kazel_group_add_before(void *group_argument, void *actor) {
    DWORD incoming_error = GetLastError();
    TalosTaskLineage lineage;
    NativeRosterSample group = sample_native_roster(
        RVA_ACTIVE_GROUP_GLOBAL, GROUP_MEMBERS_OFFSET,
        GROUP_MEMBER_STRIDE, GROUP_COUNT_OFFSET);
    NativeRosterSample formation = sample_native_roster(
        RVA_AI_MANAGER_GLOBAL, FORMATION_MEMBERS_OFFSET,
        FORMATION_MEMBER_STRIDE, FORMATION_COUNT_OFFSET);
    uint64_t actor_token = opaque_identity_token(actor);
    BOOL exact_dark_tal = classify_native_kazel_actor(actor);
    BOOL group_matches = active_group_argument_matches(group_argument);
    BOOL tracked = FALSE;
    uint32_t observation_serial;
    uint32_t request_generation;
    uint32_t state;

    memset(&lineage, 0, sizeof(lineage));
    (void)copy_task_lineage(&lineage);
    acquire_kazel_lifecycle_writer();
    if (kazel_lifecycle_tracker.state ==
            SUDEKIMP_TALOS_NATIVE_KAZEL_SPAWN_ARMED &&
        kazel_lifecycle_tracker.serialized_opcode_mask ==
            KAZEL_SERIAL_COMPLETE &&
        !kazel_lifecycle_tracker.group_add_in_progress &&
        kazel_lifecycle_tracker.source_native_thread_id ==
            GetCurrentThreadId() &&
        lineage.valid &&
        kazel_lifecycle_tracker.script_runtime_generation ==
            lineage.runtime_generation &&
        kazel_lifecycle_tracker.load_void_task_generation ==
            lineage.load_void_task_generation &&
        group_matches && exact_dark_tal &&
        exact_tal_only_member_sets(
            &group, &formation,
            kazel_lifecycle_tracker.original_tal_token)) {
        memcpy(kazel_lifecycle_tracker.group_before_tokens,
            group.member_tokens,
            sizeof(kazel_lifecycle_tracker.group_before_tokens));
        memcpy(kazel_lifecycle_tracker.formation_before_tokens,
            formation.member_tokens,
            sizeof(kazel_lifecycle_tracker.formation_before_tokens));
        kazel_lifecycle_tracker.kazel_token = actor_token;
        kazel_lifecycle_tracker.completion_native_thread_id =
            GetCurrentThreadId();
        kazel_lifecycle_tracker.group_add_before_seen = 1u;
        kazel_lifecycle_tracker.exact_dark_tal_identity = 1u;
        kazel_lifecycle_tracker.group_add_in_progress = 1u;
        kazel_lifecycle_tracker.state =
            SUDEKIMP_TALOS_NATIVE_KAZEL_GROUP_ADD_ACTIVE;
        kazel_lifecycle_tracker.observation_serial = next_nonzero(
            kazel_lifecycle_tracker.observation_serial);
        publish_kazel_lifecycle_locked();
        tracked = TRUE;
    } else if (kazel_lifecycle_tracker.serialized_opcode_mask != 0u ||
            kazel_lifecycle_tracker.state !=
                SUDEKIMP_TALOS_NATIVE_KAZEL_IDLE) {
        if (kazel_lifecycle_tracker.state !=
                SUDEKIMP_TALOS_NATIVE_KAZEL_QUARANTINED) {
            kazel_lifecycle_tracker.state =
                SUDEKIMP_TALOS_NATIVE_KAZEL_QUARANTINED;
            kazel_lifecycle_tracker.ambiguity_reason = 4u;
        }
        kazel_lifecycle_tracker.group_add_in_progress = 0u;
        kazel_lifecycle_tracker.observation_serial = next_nonzero(
            kazel_lifecycle_tracker.observation_serial);
        publish_kazel_lifecycle_locked();
    }
    request_generation = kazel_lifecycle_tracker.request_generation;
    observation_serial = kazel_lifecycle_tracker.observation_serial;
    state = kazel_lifecycle_tracker.state;
    release_kazel_lifecycle_writer();
    if (tracked || state == SUDEKIMP_TALOS_NATIVE_KAZEL_QUARANTINED)
        log_kazel_group_add_edge(
            "before", tracked, exact_dark_tal, FALSE, actor_token,
            &group, &formation, observation_serial, request_generation,
            state);
    SetLastError(incoming_error);
}

static void __attribute__((noinline, used))
observe_kazel_group_add_after(void *group_argument, void *actor) {
    DWORD result_error = GetLastError();
    TalosTaskLineage lineage;
    NativeRosterSample group = sample_native_roster(
        RVA_ACTIVE_GROUP_GLOBAL, GROUP_MEMBERS_OFFSET,
        GROUP_MEMBER_STRIDE, GROUP_COUNT_OFFSET);
    NativeRosterSample formation = sample_native_roster(
        RVA_AI_MANAGER_GLOBAL, FORMATION_MEMBERS_OFFSET,
        FORMATION_MEMBER_STRIDE, FORMATION_COUNT_OFFSET);
    NativeRosterSample group_before;
    NativeRosterSample formation_before;
    uint64_t actor_token = opaque_identity_token(actor);
    uint64_t group_added_token;
    uint64_t formation_added_token;
    BOOL exact_dark_tal = classify_native_kazel_actor(actor);
    BOOL group_matches = active_group_argument_matches(group_argument);
    BOOL tracked = FALSE;
    BOOL corroborated = FALSE;
    uint32_t observation_serial;
    uint32_t request_generation;
    uint32_t state;

    memset(&group_before, 0, sizeof(group_before));
    memset(&formation_before, 0, sizeof(formation_before));
    memset(&lineage, 0, sizeof(lineage));
    (void)copy_task_lineage(&lineage);
    acquire_kazel_lifecycle_writer();
    memcpy(group_before.member_tokens,
        kazel_lifecycle_tracker.group_before_tokens,
        sizeof(group_before.member_tokens));
    memcpy(formation_before.member_tokens,
        kazel_lifecycle_tracker.formation_before_tokens,
        sizeof(formation_before.member_tokens));
    group_before.count = 1u;
    formation_before.count = 1u;
    group_before.occupied_mask = HERO_MASK_TAL;
    formation_before.occupied_mask = HERO_MASK_TAL;
    group_before.readable = 1u;
    formation_before.readable = 1u;
    group_before.owner_present = 1u;
    formation_before.owner_present = 1u;
    group_before.structurally_valid = 1u;
    formation_before.structurally_valid = 1u;
    group_added_token = roster_single_added_token(&group_before, &group);
    formation_added_token = roster_single_added_token(
        &formation_before, &formation);
    if (kazel_lifecycle_tracker.state ==
            SUDEKIMP_TALOS_NATIVE_KAZEL_GROUP_ADD_ACTIVE &&
        kazel_lifecycle_tracker.group_add_in_progress &&
        kazel_lifecycle_tracker.kazel_token == actor_token) {
        tracked = TRUE;
        corroborated =
            SudekiMpTalosNativeLifecycleKazelGroupAddEvidencePolicy(
                TRUE,
                kazel_lifecycle_tracker.source_native_thread_id ==
                    GetCurrentThreadId() && lineage.valid &&
                    kazel_lifecycle_tracker.script_runtime_generation ==
                        lineage.runtime_generation,
                kazel_lifecycle_tracker.source_native_thread_id ==
                    GetCurrentThreadId() && lineage.valid &&
                    kazel_lifecycle_tracker.load_void_task_generation ==
                        lineage.load_void_task_generation,
                kazel_lifecycle_tracker.source_native_thread_id ==
                    GetCurrentThreadId(),
                group_matches,
                exact_dark_tal,
                group_before.count, group.count,
                formation_before.count, formation.count,
                kazel_lifecycle_tracker.original_tal_token,
                group_added_token, formation_added_token, actor_token) &&
            roster_identity_sets_equal(&group, &formation) &&
            group.occupied_mask == (HERO_MASK_TAL | HERO_MASK_AILISH) &&
            formation.occupied_mask ==
                (HERO_MASK_TAL | HERO_MASK_AILISH);
        kazel_lifecycle_tracker.group_add_in_progress = 0u;
        kazel_lifecycle_tracker.group_add_after_seen = 1u;
        kazel_lifecycle_tracker.group_add_corroborated =
            corroborated ? 1u : 0u;
        kazel_lifecycle_tracker.observation_serial = next_nonzero(
            kazel_lifecycle_tracker.observation_serial);
        if (corroborated) {
            kazel_lifecycle_tracker.kazel_token = actor_token;
            kazel_lifecycle_tracker.state =
                SUDEKIMP_TALOS_NATIVE_KAZEL_GROUP_ADD_CORROBORATED;
        } else {
            kazel_lifecycle_tracker.state =
                SUDEKIMP_TALOS_NATIVE_KAZEL_QUARANTINED;
            kazel_lifecycle_tracker.ambiguity_reason = 5u;
        }
        publish_kazel_lifecycle_locked();
    } else if (kazel_lifecycle_tracker.serialized_opcode_mask != 0u ||
            kazel_lifecycle_tracker.state !=
                SUDEKIMP_TALOS_NATIVE_KAZEL_IDLE) {
        if (kazel_lifecycle_tracker.state !=
                SUDEKIMP_TALOS_NATIVE_KAZEL_QUARANTINED) {
            kazel_lifecycle_tracker.state =
                SUDEKIMP_TALOS_NATIVE_KAZEL_QUARANTINED;
            kazel_lifecycle_tracker.ambiguity_reason = 4u;
        }
        kazel_lifecycle_tracker.group_add_in_progress = 0u;
        kazel_lifecycle_tracker.observation_serial = next_nonzero(
            kazel_lifecycle_tracker.observation_serial);
        publish_kazel_lifecycle_locked();
    }
    request_generation = kazel_lifecycle_tracker.request_generation;
    observation_serial = kazel_lifecycle_tracker.observation_serial;
    state = kazel_lifecycle_tracker.state;
    release_kazel_lifecycle_writer();
    if (tracked || state == SUDEKIMP_TALOS_NATIVE_KAZEL_QUARANTINED)
        log_kazel_group_add_edge(
            "after", tracked, exact_dark_tal, corroborated, actor_token,
            &group, &formation, observation_serial, request_generation,
            state);
    SetLastError(result_error);
}

/* The raw group-add core receives its group in EAX and a scalar actor on the
 * stack, then returns with ret 4. This bridge observes around the exact
 * relative call while reproducing native volatile registers, flags, and stack
 * cleanup. It never invokes a group or formation mutation of its own. */
__attribute__((naked, noinline, used))
static void observe_kazel_raw_group_add(void) {
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %ebx\n\t"
        "pushl %esi\n\t"
        "pushl %edi\n\t"
        "subl $20, %esp\n\t"
        "movl %eax, -16(%ebp)\n\t"
        "pushl 8(%ebp)\n\t"
        "pushl -16(%ebp)\n\t"
        "call _observe_kazel_group_add_before\n\t"
        "addl $8, %esp\n\t"
        "pushl 8(%ebp)\n\t"
        "movl -16(%ebp), %eax\n\t"
        "call *_original_raw_group_add\n\t"
        "movl %eax, -20(%ebp)\n\t"
        "movl %ecx, -24(%ebp)\n\t"
        "movl %edx, -28(%ebp)\n\t"
        "pushfl\n\t"
        "popl -32(%ebp)\n\t"
        "pushl 8(%ebp)\n\t"
        "pushl -16(%ebp)\n\t"
        "call _observe_kazel_group_add_after\n\t"
        "addl $8, %esp\n\t"
        "pushl -32(%ebp)\n\t"
        "popfl\n\t"
        "movl -28(%ebp), %edx\n\t"
        "movl -24(%ebp), %ecx\n\t"
        "movl -20(%ebp), %eax\n\t"
        "addl $20, %esp\n\t"
        "popl %edi\n\t"
        "popl %esi\n\t"
        "popl %ebx\n\t"
        "popl %ebp\n\t"
        "ret $4\n\t"
    );
}

/* Opcode 0x29 passes the function hash in EAX and five callee-cleaned stack
 * arguments to the task constructor.  This bridge clones those arguments,
 * lets the exact native constructor run once, copies only opaque lineage
 * evidence from its out-cell, then reproduces every caller-visible register
 * and the original ret 0x14 convention. */
__attribute__((naked, noinline, used))
static void observe_script_task_constructor(void) {
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %ebx\n\t"
        "pushl %esi\n\t"
        "pushl %edi\n\t"
        "subl $16, %esp\n\t"
        "movl %eax, -16(%ebp)\n\t"
        "pushl 24(%ebp)\n\t"
        "pushl 20(%ebp)\n\t"
        "pushl 16(%ebp)\n\t"
        "pushl 12(%ebp)\n\t"
        "pushl 8(%ebp)\n\t"
        "movl -16(%ebp), %eax\n\t"
        "call *_original_script_task_constructor\n\t"
        "movl %eax, -20(%ebp)\n\t"
        "movl %ecx, -24(%ebp)\n\t"
        "movl %edx, -28(%ebp)\n\t"
        "pushl 12(%ebp)\n\t"
        "pushl -16(%ebp)\n\t"
        "call _observe_script_task_constructor_return\n\t"
        "addl $8, %esp\n\t"
        "movl -28(%ebp), %edx\n\t"
        "movl -24(%ebp), %ecx\n\t"
        "movl -20(%ebp), %eax\n\t"
        "addl $16, %esp\n\t"
        "popl %edi\n\t"
        "popl %esi\n\t"
        "popl %ebx\n\t"
        "popl %ebp\n\t"
        "ret $20\n\t"
    );
}

static TalosOpcodeContext *current_opcode_27_context(void) {
    TalosOpcodeContext *context;

    if (opcode_27_tls == TLS_OUT_OF_INDEXES) return NULL;
    context = (TalosOpcodeContext *)TlsGetValue(opcode_27_tls);
    if (context == NULL || !context->exact ||
        context->native_thread_id != GetCurrentThreadId()) return NULL;
    return context;
}

static BOOL is_delete_event(SudekiMpTalosNativeLifecycleEvent event) {
    return event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_BUKI ||
        event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_AILISH ||
        event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_ELCO ||
        event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_KAZEL;
}

static BOOL is_companion_delete_event(
    SudekiMpTalosNativeLifecycleEvent event
) {
    return event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_BUKI ||
        event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_AILISH ||
        event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_ELCO;
}

static uint32_t expected_delete_resource_identifier(
    SudekiMpTalosNativeLifecycleEvent event
) {
    if (event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_BUKI)
        return RESOURCE_IDENTIFIER_PC_BUKI;
    if (event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_AILISH)
        return RESOURCE_IDENTIFIER_PC_AILISH;
    if (event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_ELCO)
        return RESOURCE_IDENTIFIER_PC_ELCO;
    if (event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_KAZEL)
        return RESOURCE_IDENTIFIER_PC_KAZEL;
    return 0u;
}

static const char *expected_delete_resource_text(
    SudekiMpTalosNativeLifecycleEvent event
) {
    if (event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_BUKI) return "PC_Buki";
    if (event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_AILISH)
        return "PC_Ailish";
    if (event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_ELCO) return "PC_Elco";
    if (event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_KAZEL)
        return "PC_KAZEL";
    return NULL;
}

static unsigned char resource_ascii_upper(unsigned char value) {
    if (value >= (unsigned char)'a' && value <= (unsigned char)'z')
        return (unsigned char)(value - ((unsigned char)'a' -
            (unsigned char)'A'));
    return value;
}

uint32_t SudekiMpTalosNativeLifecycleResourceIdentifier(
    const char *resource_text
) {
    uint32_t identifier = 0u;
    size_t index;

    if (resource_text == NULL || resource_text[0] == '\0') return 0u;
    for (index = 0u; index < RESOURCE_NAME_TEXT_CAPACITY; ++index) {
        unsigned char value = resource_ascii_upper(
            (unsigned char)resource_text[index]);

        if (value == 0u) return identifier;
        if ((index & 1u) == 0u) identifier += (uint32_t)value;
        else identifier *= (uint32_t)value;
    }
    return 0u;
}

static BOOL resource_text_equal(const char *left, const char *right) {
    size_t index;

    if (left == NULL || right == NULL) return FALSE;
    for (index = 0u; index < RESOURCE_NAME_TEXT_CAPACITY; ++index) {
        unsigned char left_value = resource_ascii_upper(
            (unsigned char)left[index]);
        unsigned char right_value = resource_ascii_upper(
            (unsigned char)right[index]);

        if (left_value != right_value) return FALSE;
        if (left_value == 0u) return TRUE;
    }
    return FALSE;
}

BOOL SudekiMpTalosNativeLifecycleDeleteResourceEvidencePolicy(
    SudekiMpTalosNativeLifecycleEvent event,
    uint32_t identifier,
    const char *resource_text,
    BOOL backing_valid,
    BOOL name_readable
) {
    uint32_t expected_identifier =
        expected_delete_resource_identifier(event);
    const char *expected_text = expected_delete_resource_text(event);

    return expected_identifier != 0u &&
        expected_text != NULL && backing_valid && name_readable &&
        identifier == expected_identifier &&
        resource_text_equal(resource_text, expected_text) &&
        SudekiMpTalosNativeLifecycleResourceIdentifier(resource_text) ==
            expected_identifier;
}

static NativeResourceNameEvidence copy_resource_name_evidence(
    const void *resource
) {
    NativeResourceNameEvidence evidence;
    uint32_t *backing = NULL;
    uint32_t reference_count = 0u;
    const char *text = NULL;
    size_t index;

    memset(&evidence, 0, sizeof(evidence));
    if (!readable_memory(resource, 3u * sizeof(uint32_t))) return evidence;
    memcpy(&evidence.encoded_kind, resource, sizeof(evidence.encoded_kind));
    memcpy(&evidence.identifier,
        (const uint8_t *)resource + sizeof(uint32_t),
        sizeof(evidence.identifier));
    memcpy(&backing, (const uint8_t *)resource + 2u * sizeof(uint32_t),
        sizeof(backing));
    evidence.readable = 1u;
    if (backing != NULL && readable_memory(backing,
            2u * sizeof(uint32_t))) {
        memcpy(&reference_count, backing, sizeof(reference_count));
        memcpy(&text, backing + 1u, sizeof(text));
        evidence.backing_valid = reference_count != 0u && text != NULL;
    }
    if (!evidence.backing_valid) return evidence;
    for (index = 0u; index < RESOURCE_NAME_TEXT_CAPACITY; ++index) {
        if (!readable_memory(text + index, 1u)) return evidence;
        evidence.text[index] = text[index];
        if (text[index] == '\0') {
            evidence.name_readable = 1u;
            evidence.recomputed_identifier =
                SudekiMpTalosNativeLifecycleResourceIdentifier(evidence.text);
            return evidence;
        }
    }
    return evidence;
}

static BOOL roster_identity_sets_equal(
    const NativeRosterSample *left,
    const NativeRosterSample *right
) {
    unsigned int left_index;
    unsigned int right_index;

    if (left == NULL || right == NULL || !left->readable ||
        !right->readable || left->count != right->count) return FALSE;
    for (left_index = 0u; left_index < NATIVE_MEMBER_LIMIT; ++left_index) {
        BOOL found = FALSE;

        if (left->member_tokens[left_index] == 0u) continue;
        for (right_index = 0u; right_index < NATIVE_MEMBER_LIMIT;
             ++right_index) {
            if (left->member_tokens[left_index] ==
                    right->member_tokens[right_index]) {
                found = TRUE;
                break;
            }
        }
        if (!found) return FALSE;
    }
    for (right_index = 0u; right_index < NATIVE_MEMBER_LIMIT; ++right_index) {
        if (right->member_tokens[right_index] == 0u) continue;
        for (left_index = 0u; left_index < NATIVE_MEMBER_LIMIT; ++left_index) {
            if (right->member_tokens[right_index] ==
                    left->member_tokens[left_index]) break;
        }
        if (left_index == NATIVE_MEMBER_LIMIT) return FALSE;
    }
    return TRUE;
}

static uint64_t roster_single_added_token(
    const NativeRosterSample *before,
    const NativeRosterSample *after
) {
    uint64_t added = 0u;
    unsigned int before_index;
    unsigned int after_index;

    if (before == NULL || after == NULL || !before->structurally_valid ||
        !after->structurally_valid || after->count != before->count + 1u)
        return 0u;
    for (before_index = 0u; before_index < NATIVE_MEMBER_LIMIT;
         ++before_index) {
        uint64_t token = before->member_tokens[before_index];
        BOOL found = token == 0u;

        for (after_index = 0u; !found && after_index < NATIVE_MEMBER_LIMIT;
             ++after_index) {
            if (after->member_tokens[after_index] == token) found = TRUE;
        }
        if (!found) return 0u;
    }
    for (after_index = 0u; after_index < NATIVE_MEMBER_LIMIT;
         ++after_index) {
        uint64_t token = after->member_tokens[after_index];
        BOOL found = token == 0u;

        for (before_index = 0u;
             !found && before_index < NATIVE_MEMBER_LIMIT;
             ++before_index) {
            if (before->member_tokens[before_index] == token) found = TRUE;
        }
        if (!found) {
            if (added != 0u) return 0u;
            added = token;
        }
    }
    return added;
}

static uint64_t roster_single_removed_token(
    const NativeRosterSample *before,
    const NativeRosterSample *after
) {
    return roster_single_added_token(after, before);
}

static uint32_t game_address_rva(const void *address) {
    uintptr_t base = (uintptr_t)game_base;
    uintptr_t value = (uintptr_t)address;

    if (base == 0u || value < base || value >= base + SUPPORTED_IMAGE_SIZE)
        return 0u;
    return (uint32_t)(value - base);
}

static BOOL roster_identity_maps_equal(
    const NativeRosterSample *group,
    const NativeRosterSample *formation
);

static BOOL exact_delete_resource_matches(
    const TalosOpcodeContext *context,
    const NativeResourceNameEvidence *resource
) {
    TalosTaskLineage lineage;

    memset(&lineage, 0, sizeof(lineage));
    return context != NULL && is_delete_event(context->event) &&
        context->lineage_matched && copy_task_lineage(&lineage) &&
        lineage.valid &&
        context->runtime_generation == lineage.runtime_generation &&
        context->native_thread_id == lineage.native_thread_id &&
        context->matched_load_void_task_generation ==
            lineage.load_void_task_generation &&
        resource != NULL && resource->readable &&
        SudekiMpTalosNativeLifecycleDeleteResourceEvidencePolicy(
            context->event, resource->identifier, resource->text,
            resource->backing_valid != 0u,
            resource->name_readable != 0u);
}

static void record_delete_pc_native_edge(
    const TalosOpcodeContext *context,
    const char *phase,
    const NativeResourceNameEvidence *resource,
    uint32_t caller_rva,
    const NativeRosterSample *group,
    const NativeRosterSample *formation
) {
    BOOL exact_context = context != NULL && is_delete_event(context->event);
    const char *expected_text = exact_context ?
        expected_delete_resource_text(context->event) : NULL;
    uint32_t expected_identifier = exact_context ?
        expected_delete_resource_identifier(context->event) : 0u;
    BOOL name_matches = expected_text != NULL && resource != NULL &&
        resource->name_readable &&
        resource_text_equal(resource->text, expected_text);
    BOOL resource_matches = exact_delete_resource_matches(context, resource);

    lifecycle_snapshot.event_serial = next_nonzero(
        lifecycle_snapshot.event_serial);
    lifecycle_snapshot.last_delete_resource_kind =
        resource != NULL ? resource->encoded_kind : 0u;
    lifecycle_snapshot.last_delete_resource_identifier =
        resource != NULL ? resource->identifier : 0u;
    lifecycle_snapshot.last_delete_recomputed_identifier =
        resource != NULL ? resource->recomputed_identifier : 0u;
    lifecycle_snapshot.last_delete_caller_rva = caller_rva;
    lifecycle_snapshot.last_delete_resource_backing_valid =
        resource != NULL ? resource->backing_valid : 0u;
    lifecycle_snapshot.last_delete_resource_name_matches =
        name_matches ? 1u : 0u;
    if (phase[0] == 'b') {
        ++lifecycle_snapshot.delete_pc_native_before_count;
    } else {
        ++lifecycle_snapshot.delete_pc_native_after_count;
    }
    SudekiMpLogFormat(
        "talos_lifecycle event=DeletePC_native phase=%s run=%08lx%08lx "
        "serial=%lu kind=%s resource_readable=%s backing_valid=%s "
        "name_readable=%s name_matches=%s "
        "resource_kind=0x%08lx resource_identifier=0x%08lx "
        "recomputed_identifier=0x%08lx expected_identifier=0x%08lx "
        "resource_matches=%s resource_snapshot=precall_copy "
        "native_caller_rva=0x%08lx "
        "enclosing_operand=0x%08lx "
        "enclosing_hash=0x%08lx lineage_matched=%s "
        "group_count=%lu group_mask=0x%02lx "
        "group_tokens=%08lx%08lx,%08lx%08lx,%08lx%08lx,%08lx%08lx "
        "formation_count=%lu formation_mask=0x%02lx "
        "formation_tokens=%08lx%08lx,%08lx%08lx,%08lx%08lx,%08lx%08lx "
        "identity_sets_equal=%s group_hero_mask=0x%02lx "
        "formation_hero_mask=0x%02lx hero_maps_equal=%s "
        "hero_tokens=%08lx%08lx,%08lx%08lx,%08lx%08lx,%08lx%08lx "
        "hero_lease_generations=%lu,%lu,%lu,%lu sequence_state=%lu "
        "corroborated_mask=0x%02lx actor_lifetime_authority=false "
        "native_thread=%lu authority=unproven "
        "policy=observation_only_original_called_once_no_mutation\r\n",
        phase,
        (unsigned long)lifecycle_snapshot.run_id_high,
        (unsigned long)lifecycle_snapshot.run_id_low,
        (unsigned long)lifecycle_snapshot.event_serial,
        exact_context ? event_name(context->event) : "out_of_scope",
        resource != NULL && resource->readable ? "true" : "false",
        resource != NULL && resource->backing_valid ? "true" : "false",
        resource != NULL && resource->name_readable ? "true" : "false",
        name_matches ? "true" : "false",
        (unsigned long)(resource != NULL ? resource->encoded_kind : 0u),
        (unsigned long)(resource != NULL ? resource->identifier : 0u),
        (unsigned long)(resource != NULL ?
            resource->recomputed_identifier : 0u),
        (unsigned long)expected_identifier,
        resource_matches ? "true" : "false",
        (unsigned long)caller_rva,
        (unsigned long)(context != NULL ? context->operand_offset : 0u),
        (unsigned long)(context != NULL ? context->binding_hash : 0u),
        context != NULL && context->lineage_matched ? "true" : "false",
        (unsigned long)group->count,
        (unsigned long)group->occupied_mask,
        (unsigned long)(group->member_tokens[0] >> 32u),
        (unsigned long)group->member_tokens[0],
        (unsigned long)(group->member_tokens[1] >> 32u),
        (unsigned long)group->member_tokens[1],
        (unsigned long)(group->member_tokens[2] >> 32u),
        (unsigned long)group->member_tokens[2],
        (unsigned long)(group->member_tokens[3] >> 32u),
        (unsigned long)group->member_tokens[3],
        (unsigned long)formation->count,
        (unsigned long)formation->occupied_mask,
        (unsigned long)(formation->member_tokens[0] >> 32u),
        (unsigned long)formation->member_tokens[0],
        (unsigned long)(formation->member_tokens[1] >> 32u),
        (unsigned long)formation->member_tokens[1],
        (unsigned long)(formation->member_tokens[2] >> 32u),
        (unsigned long)formation->member_tokens[2],
        (unsigned long)(formation->member_tokens[3] >> 32u),
        (unsigned long)formation->member_tokens[3],
        roster_identity_sets_equal(group, formation) ? "true" : "false",
        (unsigned long)group->hero_mask,
        (unsigned long)formation->hero_mask,
        roster_identity_maps_equal(group, formation) ? "true" : "false",
        (unsigned long)(group->hero_tokens[0] >> 32u),
        (unsigned long)group->hero_tokens[0],
        (unsigned long)(group->hero_tokens[1] >> 32u),
        (unsigned long)group->hero_tokens[1],
        (unsigned long)(group->hero_tokens[2] >> 32u),
        (unsigned long)group->hero_tokens[2],
        (unsigned long)(group->hero_tokens[3] >> 32u),
        (unsigned long)group->hero_tokens[3],
        (unsigned long)roster_identity_tracker.lease_generation[0],
        (unsigned long)roster_identity_tracker.lease_generation[1],
        (unsigned long)roster_identity_tracker.lease_generation[2],
        (unsigned long)roster_identity_tracker.lease_generation[3],
        (unsigned long)roster_identity_tracker.sequence_state,
        (unsigned long)
            roster_identity_tracker.delete_delta_corroborated_mask,
        (unsigned long)GetCurrentThreadId());
}

static unsigned int count_mask_bits(uint32_t mask) {
    unsigned int count = 0u;

    while (mask != 0u) {
        count += mask & 1u;
        mask >>= 1u;
    }
    return count;
}

static SudekiMpTalosNativeHeroIdentity classify_native_hero_actor(
    const void *actor
) {
    void *main_vtable;
    void *secondary_vtable;
    void *resource_vtable;

    if (actor == NULL || !readable_memory(actor,
            ACTOR_RESOURCE_VTABLE_OFFSET + sizeof(resource_vtable))) {
        return SUDEKIMP_TALOS_NATIVE_HERO_UNKNOWN;
    }
    memcpy(&main_vtable, actor, sizeof(main_vtable));
    memcpy(&secondary_vtable,
        (const uint8_t *)actor + ACTOR_SECONDARY_VTABLE_OFFSET,
        sizeof(secondary_vtable));
    memcpy(&resource_vtable,
        (const uint8_t *)actor + ACTOR_RESOURCE_VTABLE_OFFSET,
        sizeof(resource_vtable));
    return SudekiMpTalosNativeLifecycleHeroFromVtableRvas(
        game_address_rva(main_vtable),
        game_address_rva(secondary_vtable),
        game_address_rva(resource_vtable));
}

static BOOL classify_native_kazel_actor(const void *actor) {
    void *main_vtable;
    void *secondary_vtable;
    void *resource_vtable;

    if (actor == NULL || !readable_memory(actor,
            ACTOR_RESOURCE_VTABLE_OFFSET + sizeof(resource_vtable))) {
        return FALSE;
    }
    memcpy(&main_vtable, actor, sizeof(main_vtable));
    memcpy(&secondary_vtable,
        (const uint8_t *)actor + ACTOR_SECONDARY_VTABLE_OFFSET,
        sizeof(secondary_vtable));
    memcpy(&resource_vtable,
        (const uint8_t *)actor + ACTOR_RESOURCE_VTABLE_OFFSET,
        sizeof(resource_vtable));
    return SudekiMpTalosNativeLifecycleKazelFromVtableRvas(
        game_address_rva(main_vtable),
        game_address_rva(secondary_vtable),
        game_address_rva(resource_vtable));
}

static NativeRosterSample sample_native_roster(
    uint32_t owner_global_rva,
    uint32_t members_offset,
    uint32_t member_stride,
    uint32_t count_offset
) {
    NativeRosterSample sample;
    uint8_t *owner;
    uint8_t *owner_after;
    uint32_t count_after;
    void *members_after[NATIVE_MEMBER_LIMIT];
    unsigned int index;

    memset(&sample, 0, sizeof(sample));
    if (game_base == NULL ||
        !readable_memory(game_base + owner_global_rva, sizeof(owner))) {
        return sample;
    }
    owner = *(uint8_t **)(game_base + owner_global_rva);
    if (owner == NULL) {
        sample.readable = 1u;
        return sample;
    }
    sample.owner_present = 1u;
    if (!readable_memory(owner + count_offset, sizeof(sample.count)) ||
        !readable_memory(owner + members_offset,
            (NATIVE_MEMBER_LIMIT - 1u) * member_stride + sizeof(void *))) {
        return sample;
    }
    memcpy(&sample.count, owner + count_offset, sizeof(sample.count));
    for (index = 0u; index < NATIVE_MEMBER_LIMIT; ++index) {
        SudekiMpTalosNativeHeroIdentity hero;
        void *member;

        memcpy(&member, owner + members_offset + index * member_stride,
            sizeof(member));
        sample.members[index] = member;
        if (member != NULL) {
            sample.occupied_mask |= (uint32_t)1u << index;
            sample.member_tokens[index] = opaque_identity_token(member);
            hero = classify_native_hero_actor(member);
            sample.hero_by_slot[index] = (uint8_t)hero;
            if (hero == SUDEKIMP_TALOS_NATIVE_HERO_UNKNOWN ||
                (sample.hero_mask & ((uint8_t)1u << (unsigned int)hero)) !=
                    0u) {
                continue;
            }
            sample.hero_mask |= (uint8_t)1u << (unsigned int)hero;
            sample.hero_tokens[(unsigned int)hero] =
                sample.member_tokens[index];
        } else {
            sample.hero_by_slot[index] =
                (uint8_t)SUDEKIMP_TALOS_NATIVE_HERO_UNKNOWN;
        }
    }
    memcpy(&owner_after, game_base + owner_global_rva, sizeof(owner_after));
    memcpy(&count_after, owner + count_offset, sizeof(count_after));
    for (index = 0u; index < NATIVE_MEMBER_LIMIT; ++index) {
        memcpy(&members_after[index],
            owner + members_offset + index * member_stride,
            sizeof(members_after[index]));
    }
    if (owner_after != owner || count_after != sample.count ||
        memcmp(members_after, sample.members, sizeof(members_after)) != 0) {
        return sample;
    }
    sample.readable = 1u;
    sample.structurally_valid = sample.count <= NATIVE_MEMBER_LIMIT &&
        count_mask_bits(sample.occupied_mask) == sample.count;
    sample.identity_complete = sample.structurally_valid &&
        count_mask_bits(sample.hero_mask) == sample.count;
    return sample;
}

static BOOL roster_identity_maps_equal(
    const NativeRosterSample *group,
    const NativeRosterSample *formation
) {
    unsigned int hero;

    if (group == NULL || formation == NULL ||
        !group->identity_complete || !formation->identity_complete ||
        group->hero_mask != formation->hero_mask ||
        group->count != formation->count) return FALSE;
    for (hero = 0u; hero < NATIVE_MEMBER_LIMIT; ++hero) {
        if (group->hero_tokens[hero] != formation->hero_tokens[hero])
            return FALSE;
    }
    return TRUE;
}

static BOOL roster_matches_original_tal_survivor(
    const NativeRosterSample *group,
    const NativeRosterSample *formation
) {
    unsigned int hero;

    if (!roster_identity_maps_equal(group, formation) ||
        group->count != 1u || formation->count != 1u ||
        group->occupied_mask != HERO_MASK_TAL ||
        formation->occupied_mask != HERO_MASK_TAL ||
        group->hero_mask != HERO_MASK_TAL ||
        formation->hero_mask != HERO_MASK_TAL ||
        group->hero_by_slot[0] != SUDEKIMP_TALOS_NATIVE_HERO_TAL ||
        formation->hero_by_slot[0] != SUDEKIMP_TALOS_NATIVE_HERO_TAL ||
        roster_identity_tracker.sequence_token[
            SUDEKIMP_TALOS_NATIVE_HERO_TAL] == 0u ||
        group->hero_tokens[SUDEKIMP_TALOS_NATIVE_HERO_TAL] !=
            roster_identity_tracker.sequence_token[
                SUDEKIMP_TALOS_NATIVE_HERO_TAL]) return FALSE;
    for (hero = 1u; hero < NATIVE_MEMBER_LIMIT; ++hero) {
        if (group->hero_tokens[hero] != 0u ||
            formation->hero_tokens[hero] != 0u) return FALSE;
    }
    return TRUE;
}

static BOOL is_kazel_serial_event(
    SudekiMpTalosNativeLifecycleEvent event
) {
    return event == SUDEKIMP_TALOS_NATIVE_EVENT_TAL_KAZEL_MERGE ||
        event == SUDEKIMP_TALOS_NATIVE_EVENT_SPAWN_KAZEL_WRAPPER ||
        event == SUDEKIMP_TALOS_NATIVE_EVENT_INTERNAL_SPAWN_PC;
}

static BOOL exact_kazel_serial_context(
    const TalosOpcodeContext *context,
    const TalosTaskLineage *lineage
) {
    return context != NULL && lineage != NULL &&
        is_kazel_serial_event(context->event) && context->exact &&
        context->lineage_matched && lineage->valid &&
        context->matched_load_void_task_generation != 0u &&
        context->matched_load_void_task_generation ==
            lineage->load_void_task_generation &&
        context->runtime_generation == lineage->runtime_generation &&
        context->native_thread_id == lineage->native_thread_id &&
        context->native_thread_id == GetCurrentThreadId() &&
        context->thread_token == lineage->load_void_thread_token;
}

static BOOL kazel_tracker_matches_context(
    const TalosOpcodeContext *context,
    const TalosTaskLineage *lineage
) {
    return exact_kazel_serial_context(context, lineage) &&
        kazel_lifecycle_tracker.script_runtime_generation ==
            lineage->runtime_generation &&
        kazel_lifecycle_tracker.load_void_task_generation ==
            lineage->load_void_task_generation &&
        kazel_lifecycle_tracker.source_native_thread_id ==
            lineage->native_thread_id &&
        kazel_lifecycle_tracker.source_script_thread_token ==
            lineage->load_void_thread_token;
}

static void observe_kazel_opcode_edge(
    const TalosOpcodeContext *context,
    BOOL before
) {
    TalosTaskLineage lineage;
    NativeRosterSample group;
    NativeRosterSample formation;
    BOOL exact_context;
    BOOL exact_chain = FALSE;
    BOOL settle_ready = FALSE;
    BOOL sequence_step;
    BOOL accepted = FALSE;
    uint8_t next_serial_mask = 0u;
    uint32_t request_generation = 0u;
    uint32_t state = 0u;
    DWORD incoming_error = GetLastError();

    if (context == NULL || !is_kazel_serial_event(context->event)) return;
    memset(&group, 0, sizeof(group));
    memset(&formation, 0, sizeof(formation));
    memset(&lineage, 0, sizeof(lineage));
    (void)copy_task_lineage(&lineage);
    exact_context = exact_kazel_serial_context(context, &lineage);
    if (exact_context &&
        ((before && context->event ==
            SUDEKIMP_TALOS_NATIVE_EVENT_TAL_KAZEL_MERGE) ||
         context->event ==
            SUDEKIMP_TALOS_NATIVE_EVENT_INTERNAL_SPAWN_PC)) {
        group = sample_native_roster(
            RVA_ACTIVE_GROUP_GLOBAL, GROUP_MEMBERS_OFFSET,
            GROUP_MEMBER_STRIDE, GROUP_COUNT_OFFSET);
        formation = sample_native_roster(
            RVA_AI_MANAGER_GLOBAL, FORMATION_MEMBERS_OFFSET,
            FORMATION_MEMBER_STRIDE, FORMATION_COUNT_OFFSET);
    }
    if (before && context->event ==
            SUDEKIMP_TALOS_NATIVE_EVENT_TAL_KAZEL_MERGE) {
        acquire_settle_evidence_writer();
        settle_ready = settle_evidence_tracker.session_generation != 0u &&
            settle_evidence_tracker.void_set_zone_completed &&
            settle_evidence_tracker.script_runtime_generation ==
                context->runtime_generation &&
            settle_evidence_tracker.load_void_task_generation ==
                context->matched_load_void_task_generation &&
            settle_evidence_tracker.native_thread_id ==
                context->native_thread_id;
        release_settle_evidence_writer();
    }
    acquire_kazel_lifecycle_writer();
    sequence_step =
        SudekiMpTalosNativeLifecycleKazelSerializedSequencePolicy(
            kazel_lifecycle_tracker.serialized_opcode_mask,
            context->event, before, exact_context, &next_serial_mask);
    if (context->event ==
            SUDEKIMP_TALOS_NATIVE_EVENT_TAL_KAZEL_MERGE && before) {
        if (sequence_step && settle_ready &&
            kazel_lifecycle_tracker.session_generation != 0u &&
            kazel_lifecycle_tracker.state ==
                SUDEKIMP_TALOS_NATIVE_KAZEL_IDLE &&
            lifecycle_snapshot.load_void_task_generation ==
                lineage.load_void_task_generation &&
            roster_identity_tracker.sequence_state ==
                SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_COMPANIONS_REMOVED &&
            roster_matches_original_tal_survivor(&group, &formation)) {
            kazel_lifecycle_tracker.script_runtime_generation =
                context->runtime_generation;
            kazel_lifecycle_tracker.load_void_task_generation =
                lineage.load_void_task_generation;
            kazel_lifecycle_tracker.source_native_thread_id =
                context->native_thread_id;
            kazel_lifecycle_tracker.source_script_thread_token =
                context->thread_token;
            kazel_lifecycle_tracker.original_tal_token =
                group.hero_tokens[SUDEKIMP_TALOS_NATIVE_HERO_TAL];
            kazel_lifecycle_tracker.serialized_opcode_mask = next_serial_mask;
            accepted = TRUE;
        }
    } else if (context->event ==
            SUDEKIMP_TALOS_NATIVE_EVENT_TAL_KAZEL_MERGE && !before) {
        if (kazel_tracker_matches_context(context, &lineage) &&
            sequence_step) {
            kazel_lifecycle_tracker.serialized_opcode_mask = next_serial_mask;
            accepted = TRUE;
        }
    } else if (context->event ==
            SUDEKIMP_TALOS_NATIVE_EVENT_SPAWN_KAZEL_WRAPPER && before) {
        if (kazel_tracker_matches_context(context, &lineage) &&
            sequence_step) {
            kazel_lifecycle_tracker.serialized_opcode_mask = next_serial_mask;
            accepted = TRUE;
        }
    } else if (context->event ==
            SUDEKIMP_TALOS_NATIVE_EVENT_SPAWN_KAZEL_WRAPPER && !before) {
        if (kazel_tracker_matches_context(context, &lineage) &&
            sequence_step) {
            kazel_lifecycle_tracker.serialized_opcode_mask = next_serial_mask;
            accepted = TRUE;
        }
    } else if (context->event ==
            SUDEKIMP_TALOS_NATIVE_EVENT_INTERNAL_SPAWN_PC && before) {
        exact_chain = kazel_tracker_matches_context(context, &lineage) &&
            sequence_step;
        if (exact_chain && kazel_lifecycle_tracker.state ==
                SUDEKIMP_TALOS_NATIVE_KAZEL_IDLE &&
            roster_identity_tracker.sequence_state ==
                SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_COMPANIONS_REMOVED &&
            roster_matches_original_tal_survivor(&group, &formation) &&
            group.hero_tokens[SUDEKIMP_TALOS_NATIVE_HERO_TAL] ==
                kazel_lifecycle_tracker.original_tal_token) {
            kazel_lifecycle_tracker.spawn_binding_before_seen = 1u;
            kazel_lifecycle_tracker.serialized_opcode_mask = next_serial_mask;
            accepted = TRUE;
        }
    } else if (context->event ==
            SUDEKIMP_TALOS_NATIVE_EVENT_INTERNAL_SPAWN_PC && !before) {
        exact_chain = kazel_tracker_matches_context(context, &lineage) &&
            sequence_step;
        if (exact_chain && kazel_lifecycle_tracker.state ==
                SUDEKIMP_TALOS_NATIVE_KAZEL_IDLE &&
            kazel_lifecycle_tracker.spawn_binding_before_seen &&
            kazel_lifecycle_tracker.script_runtime_generation ==
                context->runtime_generation &&
            kazel_lifecycle_tracker.source_native_thread_id ==
                context->native_thread_id &&
            roster_identity_tracker.sequence_state ==
                SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_COMPANIONS_REMOVED &&
            roster_matches_original_tal_survivor(&group, &formation) &&
            group.hero_tokens[SUDEKIMP_TALOS_NATIVE_HERO_TAL] ==
                kazel_lifecycle_tracker.original_tal_token) {
            kazel_lifecycle_tracker.request_generation = next_nonzero(
                kazel_lifecycle_tracker.request_generation);
            kazel_lifecycle_tracker.spawn_binding_after_seen = 1u;
            kazel_lifecycle_tracker.serialized_opcode_mask = next_serial_mask;
            kazel_lifecycle_tracker.state =
                SUDEKIMP_TALOS_NATIVE_KAZEL_SPAWN_ARMED;
            accepted = TRUE;
        }
    }
    if (!accepted) {
        kazel_lifecycle_tracker.state =
            SUDEKIMP_TALOS_NATIVE_KAZEL_QUARANTINED;
        kazel_lifecycle_tracker.ambiguity_reason = 6u;
    }
    kazel_lifecycle_tracker.observation_serial = next_nonzero(
        kazel_lifecycle_tracker.observation_serial);
    publish_kazel_lifecycle_locked();
    request_generation = kazel_lifecycle_tracker.request_generation;
    state = kazel_lifecycle_tracker.state;
    release_kazel_lifecycle_writer();

    if (context->event ==
            SUDEKIMP_TALOS_NATIVE_EVENT_INTERNAL_SPAWN_PC) {
        lifecycle_snapshot.event_serial = next_nonzero(
            lifecycle_snapshot.event_serial);
        lifecycle_snapshot.observed_event_mask |=
            (uint32_t)1u << (unsigned int)
                SUDEKIMP_TALOS_NATIVE_EVENT_INTERNAL_SPAWN_PC;
        SudekiMpLogFormat(
            "talos_lifecycle event=KazelSpawn_binding phase=%s "
            "run=%08lx%08lx serial=%lu request_generation=%lu "
            "exact_serialized_chain=%s accepted=%s state=%lu "
            "serialized_opcode_mask=0x%02lx "
            "group_count=%lu formation_count=%lu authored_resource=PC_KAZEL "
            "authored_coordinates_zero=true native_thread=%lu "
            "runtime_generation=%lu task_generation=%lu "
            "actor_lifetime_authority=false "
            "policy=observation_only_original_called_once_no_mutation\r\n",
            before ? "before" : "after",
            (unsigned long)lifecycle_snapshot.run_id_high,
            (unsigned long)lifecycle_snapshot.run_id_low,
            (unsigned long)lifecycle_snapshot.event_serial,
            (unsigned long)request_generation,
            exact_chain ? "true" : "false",
            accepted ? "true" : "false",
            (unsigned long)state,
            (unsigned long)kazel_lifecycle_snapshot.serialized_opcode_mask,
            (unsigned long)group.count,
            (unsigned long)formation.count,
            (unsigned long)context->native_thread_id,
            (unsigned long)context->runtime_generation,
            (unsigned long)lineage.load_void_task_generation);
    }
    SetLastError(incoming_error);
}

static void publish_roster_identity(
    const NativeRosterSample *group,
    const NativeRosterSample *formation,
    BOOL mapping_valid
) {
    SudekiMpTalosNativeRosterIdentitySnapshot snapshot;
    unsigned int hero;

    acquire_roster_identity_writer();
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.observation_serial = roster_identity_tracker.observation_serial;
    snapshot.script_runtime_generation =
        lifecycle_snapshot.script_runtime_generation;
    snapshot.load_void_task_generation =
        lifecycle_snapshot.load_void_task_generation;
    snapshot.roster_revision = roster_identity_tracker.roster_revision;
    snapshot.hero_present_mask = roster_identity_tracker.last_present_mask;
    snapshot.group_hero_mask = group != NULL ? group->hero_mask : 0u;
    snapshot.formation_hero_mask =
        formation != NULL ? formation->hero_mask : 0u;
    snapshot.sequence_state = roster_identity_tracker.sequence_state;
    snapshot.quarantine_reason = roster_identity_tracker.quarantine_reason;
    snapshot.immediate_identity_complete = mapping_valid && group != NULL &&
        formation != NULL && group->hero_mask == HERO_MASK_ALL &&
        formation->hero_mask == HERO_MASK_ALL;
    snapshot.delete_delta_corroborated_mask =
        roster_identity_tracker.delete_delta_corroborated_mask;
    snapshot.actor_lifetime_authority_proven = 0u;
    for (hero = 0u; hero < NATIVE_MEMBER_LIMIT; ++hero) {
        snapshot.hero_roster_lease_generation[hero] =
            roster_identity_tracker.lease_generation[hero];
        if ((roster_identity_tracker.last_present_mask &
                ((uint8_t)1u << hero)) != 0u) {
            snapshot.hero_token[hero] =
                roster_identity_tracker.last_token[hero];
        }
    }
    (void)InterlockedIncrement(&roster_identity_snapshot_sequence);
    MemoryBarrier();
    roster_identity_snapshot = snapshot;
    MemoryBarrier();
    (void)InterlockedIncrement(&roster_identity_snapshot_sequence);
    release_roster_identity_writer();
}

static void clear_roster_identity_state(void) {
    acquire_roster_identity_writer();
    (void)InterlockedIncrement(&roster_identity_snapshot_sequence);
    MemoryBarrier();
    memset(&roster_identity_tracker, 0, sizeof(roster_identity_tracker));
    memset(&roster_identity_snapshot, 0, sizeof(roster_identity_snapshot));
    MemoryBarrier();
    (void)InterlockedIncrement(&roster_identity_snapshot_sequence);
    release_roster_identity_writer();
}

static BOOL observe_composite_roster_identity(
    const NativeRosterSample *group,
    const NativeRosterSample *formation
) {
    BOOL valid = roster_identity_maps_equal(group, formation);
    BOOL tuple_changed = !roster_identity_tracker.has_prior_valid_sample;
    unsigned int hero;

    if (roster_identity_tracker.writer_native_thread_id == 0u ||
        roster_identity_tracker.writer_native_thread_id !=
            GetCurrentThreadId()) return FALSE;
    roster_identity_tracker.observation_serial = next_nonzero(
        roster_identity_tracker.observation_serial);
    if (!valid) {
        publish_roster_identity(group, formation, FALSE);
        return FALSE;
    }
    for (hero = 0u; hero < NATIVE_MEMBER_LIMIT; ++hero) {
        uint8_t bit = (uint8_t)1u << hero;
        BOOL was_present = roster_identity_tracker.has_prior_valid_sample &&
            (roster_identity_tracker.last_present_mask & bit) != 0u;
        BOOL is_present = (group->hero_mask & bit) != 0u;
        uint64_t token = group->hero_tokens[hero];

        if (!roster_identity_tracker.has_prior_valid_sample) {
            if (is_present) {
                roster_identity_tracker.lease_generation[hero] = next_nonzero(
                    roster_identity_tracker.lease_generation[hero]);
            }
        } else if (was_present != is_present ||
            (is_present &&
             roster_identity_tracker.last_token[hero] != token)) {
            roster_identity_tracker.lease_generation[hero] = next_nonzero(
                roster_identity_tracker.lease_generation[hero]);
            tuple_changed = TRUE;
        }
        if (is_present) roster_identity_tracker.last_token[hero] = token;
    }
    if (roster_identity_tracker.has_prior_valid_sample &&
        roster_identity_tracker.last_present_mask != group->hero_mask) {
        tuple_changed = TRUE;
    }
    if (tuple_changed) roster_identity_tracker.roster_revision = next_nonzero(
        roster_identity_tracker.roster_revision);
    roster_identity_tracker.last_present_mask = group->hero_mask;
    roster_identity_tracker.has_prior_valid_sample = 1u;
    publish_roster_identity(group, formation, TRUE);
    return TRUE;
}

static void quarantine_roster_sequence(
    uint8_t reason,
    const NativeRosterSample *group,
    const NativeRosterSample *formation
) {
    if (roster_identity_tracker.writer_native_thread_id == 0u ||
        roster_identity_tracker.writer_native_thread_id !=
            GetCurrentThreadId()) return;
    roster_identity_tracker.sequence_state =
        SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_QUARANTINED;
    roster_identity_tracker.quarantine_reason = reason == 0u ? 1u : reason;
    roster_identity_tracker.observation_serial = next_nonzero(
        roster_identity_tracker.observation_serial);
    publish_roster_identity(group, formation,
        roster_identity_maps_equal(group, formation));
    invalidate_settle_evidence(TRUE);
}

static void observe_delete_identity_delta(
    const TalosOpcodeContext *context,
    const NativeResourceNameEvidence *resource,
    const NativeRosterSample *group_before,
    const NativeRosterSample *formation_before,
    const NativeRosterSample *group_after,
    const NativeRosterSample *formation_after
) {
    SudekiMpTalosNativeHeroIdentity removed_hero;
    uint8_t expected_before_mask;
    uint8_t expected_after_mask;
    uint8_t expected_state;
    uint8_t next_state;
    unsigned int hero;
    BOOL valid;

    if (!exact_delete_resource_matches(context, resource)) {
        quarantine_roster_sequence(1u, group_after, formation_after);
        return;
    }
    if (context->event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_BUKI) {
        removed_hero = SUDEKIMP_TALOS_NATIVE_HERO_BUKI;
        expected_before_mask = HERO_MASK_ALL;
        expected_after_mask = HERO_MASK_TAL | HERO_MASK_AILISH |
            HERO_MASK_ELCO;
        expected_state = SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_IDLE;
        next_state = SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_BUKI_REMOVED;
    } else if (context->event ==
            SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_AILISH) {
        removed_hero = SUDEKIMP_TALOS_NATIVE_HERO_AILISH;
        expected_before_mask = HERO_MASK_TAL | HERO_MASK_AILISH |
            HERO_MASK_ELCO;
        expected_after_mask = HERO_MASK_TAL | HERO_MASK_ELCO;
        expected_state =
            SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_BUKI_REMOVED;
        next_state =
            SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_AILISH_REMOVED;
    } else {
        removed_hero = SUDEKIMP_TALOS_NATIVE_HERO_ELCO;
        expected_before_mask = HERO_MASK_TAL | HERO_MASK_ELCO;
        expected_after_mask = HERO_MASK_TAL;
        expected_state =
            SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_AILISH_REMOVED;
        next_state =
            SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_COMPANIONS_REMOVED;
    }
    valid = roster_identity_maps_equal(group_before, formation_before) &&
        roster_identity_maps_equal(group_after, formation_after) &&
        group_before->hero_mask == expected_before_mask &&
        group_after->hero_mask == expected_after_mask &&
        group_before->count == group_after->count + 1u &&
        roster_identity_tracker.sequence_state == expected_state &&
        group_before->hero_tokens[(unsigned int)removed_hero] != 0u &&
        group_after->hero_tokens[(unsigned int)removed_hero] == 0u;
    if (valid && expected_state ==
            SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_IDLE) {
        for (hero = 0u; hero < NATIVE_MEMBER_LIMIT; ++hero) {
            roster_identity_tracker.sequence_token[hero] =
                group_before->hero_tokens[hero];
            if (roster_identity_tracker.sequence_token[hero] == 0u)
                valid = FALSE;
        }
    }
    for (hero = 0u; valid && hero < NATIVE_MEMBER_LIMIT; ++hero) {
        uint8_t bit = (uint8_t)1u << hero;
        uint64_t expected_token =
            roster_identity_tracker.sequence_token[hero];

        if ((expected_before_mask & bit) != 0u &&
            group_before->hero_tokens[hero] != expected_token) valid = FALSE;
        if ((expected_after_mask & bit) != 0u) {
            if (group_after->hero_tokens[hero] != expected_token) valid = FALSE;
        } else if (group_after->hero_tokens[hero] != 0u) {
            valid = FALSE;
        }
    }
    if (!valid) {
        quarantine_roster_sequence(3u, group_after, formation_after);
        return;
    }
    roster_identity_tracker.sequence_state = next_state;
    roster_identity_tracker.quarantine_reason = 0u;
    roster_identity_tracker.delete_delta_corroborated_mask |=
        (uint8_t)1u << (unsigned int)removed_hero;
    roster_identity_tracker.observation_serial = next_nonzero(
        roster_identity_tracker.observation_serial);
    publish_roster_identity(group_after, formation_after, TRUE);
    SudekiMpLogFormat(
        "talos_lifecycle event=DeletePC_identity_delta phase=verified "
        "run=%08lx%08lx observation_serial=%lu hero=%lu "
        "before_mask=0x%02lx after_mask=0x%02lx sequence_state=%lu "
        "corroborated_mask=0x%02lx identity_authority="
        "vtable_triplet_plus_resource_delta "
        "actor_lifetime_authority=false policy=observation_only\r\n",
        (unsigned long)lifecycle_snapshot.run_id_high,
        (unsigned long)lifecycle_snapshot.run_id_low,
        (unsigned long)roster_identity_tracker.observation_serial,
        (unsigned long)removed_hero,
        (unsigned long)expected_before_mask,
        (unsigned long)expected_after_mask,
        (unsigned long)next_state,
        (unsigned long)
            roster_identity_tracker.delete_delta_corroborated_mask);
}

static void observe_kazel_delete_delta(
    const TalosOpcodeContext *context,
    const NativeResourceNameEvidence *resource,
    const NativeRosterSample *group_before,
    const NativeRosterSample *formation_before,
    const NativeRosterSample *group_after,
    const NativeRosterSample *formation_after
) {
    TalosTaskLineage lineage;
    uint64_t group_removed = roster_single_removed_token(
        group_before, group_after);
    uint64_t formation_removed = roster_single_removed_token(
        formation_before, formation_after);
    BOOL valid;
    uint32_t request_generation;
    uint32_t state;

    memset(&lineage, 0, sizeof(lineage));
    (void)copy_task_lineage(&lineage);
    acquire_kazel_lifecycle_writer();
    valid = context != NULL &&
        context->event == SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_KAZEL &&
        exact_delete_resource_matches(context, resource) &&
        kazel_lifecycle_tracker.source_native_thread_id ==
            context->native_thread_id &&
        lineage.valid &&
        kazel_lifecycle_tracker.state ==
            SUDEKIMP_TALOS_NATIVE_KAZEL_GROUP_ADD_CORROBORATED &&
        kazel_lifecycle_tracker.group_add_corroborated &&
        kazel_lifecycle_tracker.script_runtime_generation ==
            lineage.runtime_generation &&
        kazel_lifecycle_tracker.load_void_task_generation ==
            lineage.load_void_task_generation &&
        context->matched_load_void_task_generation ==
            lineage.load_void_task_generation &&
        group_before->structurally_valid &&
        formation_before->structurally_valid &&
        group_before->count == 2u && formation_before->count == 2u &&
        group_before->occupied_mask == UINT32_C(0x03) &&
        formation_before->occupied_mask == UINT32_C(0x03) &&
        roster_identity_sets_equal(group_before, formation_before) &&
        exact_tal_only_member_sets(
            group_after, formation_after,
            kazel_lifecycle_tracker.original_tal_token) &&
        group_removed == kazel_lifecycle_tracker.kazel_token &&
        formation_removed == kazel_lifecycle_tracker.kazel_token;
    kazel_lifecycle_tracker.observation_serial = next_nonzero(
        kazel_lifecycle_tracker.observation_serial);
    if (valid) {
        kazel_lifecycle_tracker.delete_corroborated = 1u;
        kazel_lifecycle_tracker.state =
            SUDEKIMP_TALOS_NATIVE_KAZEL_DELETE_CORROBORATED;
    } else {
        kazel_lifecycle_tracker.state =
            SUDEKIMP_TALOS_NATIVE_KAZEL_QUARANTINED;
        kazel_lifecycle_tracker.ambiguity_reason = 6u;
    }
    publish_kazel_lifecycle_locked();
    request_generation = kazel_lifecycle_tracker.request_generation;
    state = kazel_lifecycle_tracker.state;
    release_kazel_lifecycle_writer();
    SudekiMpLogFormat(
        "talos_lifecycle event=KazelDelete_identity_delta phase=verified "
        "run=%08lx%08lx request_generation=%lu valid=%s state=%lu "
        "group_removed_token=%08lx%08lx "
        "formation_removed_token=%08lx%08lx "
        "actor_lifetime_authority=false policy=observation_only\r\n",
        (unsigned long)lifecycle_snapshot.run_id_high,
        (unsigned long)lifecycle_snapshot.run_id_low,
        (unsigned long)request_generation,
        valid ? "true" : "false",
        (unsigned long)state,
        (unsigned long)(group_removed >> 32u),
        (unsigned long)group_removed,
        (unsigned long)(formation_removed >> 32u),
        (unsigned long)formation_removed);
}

static void __cdecl observe_delete_pc(const void *resource) {
    DWORD incoming_error = GetLastError();
    uint32_t caller_rva = game_address_rva(__builtin_return_address(0));
    TalosOpcodeContext *context = current_opcode_27_context();
    DWORD result_error;
    NativeResourceNameEvidence resource_evidence =
        copy_resource_name_evidence(resource);
    NativeRosterSample group_before = sample_native_roster(
        RVA_ACTIVE_GROUP_GLOBAL, GROUP_MEMBERS_OFFSET,
        GROUP_MEMBER_STRIDE, GROUP_COUNT_OFFSET);
    NativeRosterSample formation_before = sample_native_roster(
        RVA_AI_MANAGER_GLOBAL, FORMATION_MEMBERS_OFFSET,
        FORMATION_MEMBER_STRIDE, FORMATION_COUNT_OFFSET);
    NativeRosterSample group_after;
    NativeRosterSample formation_after;
    BOOL exact_identity_edge = context != NULL &&
        is_companion_delete_event(context->event) &&
        exact_delete_resource_matches(context, &resource_evidence);

    if (exact_identity_edge)
        (void)observe_composite_roster_identity(
            &group_before, &formation_before);
    record_delete_pc_native_edge(context, "before", &resource_evidence,
        caller_rva, &group_before, &formation_before);
    SetLastError(incoming_error);
    original_delete_pc(resource);
    result_error = GetLastError();
    group_after = sample_native_roster(
        RVA_ACTIVE_GROUP_GLOBAL, GROUP_MEMBERS_OFFSET,
        GROUP_MEMBER_STRIDE, GROUP_COUNT_OFFSET);
    formation_after = sample_native_roster(
        RVA_AI_MANAGER_GLOBAL, FORMATION_MEMBERS_OFFSET,
        FORMATION_MEMBER_STRIDE, FORMATION_COUNT_OFFSET);
    if (exact_identity_edge)
        (void)observe_composite_roster_identity(
            &group_after, &formation_after);
    if (context != NULL && is_companion_delete_event(context->event)) {
        observe_delete_identity_delta(context, &resource_evidence,
            &group_before, &formation_before, &group_after, &formation_after);
    } else if (context != NULL && context->event ==
            SUDEKIMP_TALOS_NATIVE_EVENT_DELETE_KAZEL) {
        observe_kazel_delete_delta(context, &resource_evidence,
            &group_before, &formation_before, &group_after, &formation_after);
    }
    record_delete_pc_native_edge(context, "after", &resource_evidence,
        caller_rva, &group_after, &formation_after);
    SetLastError(result_error);
}

static BOOL roster_sample_empty(const NativeRosterSample *sample) {
    return sample != NULL && sample->readable && sample->owner_present &&
        sample->structurally_valid && sample->identity_complete &&
        sample->count == 0u && sample->occupied_mask == 0u &&
        sample->hero_mask == 0u;
}

static void record_remove_all_players_edge(
    const TalosOpcodeContext *context,
    const char *phase,
    const NativeRosterSample *group,
    const NativeRosterSample *formation
) {
    BOOL exact_context = context != NULL &&
        context->event == SUDEKIMP_TALOS_NATIVE_EVENT_REMOVE_ALL_PLAYERS;
    BOOL after = phase[0] == 'a';
    BOOL verified_empty = after && roster_sample_empty(group) &&
        roster_sample_empty(formation);

    lifecycle_snapshot.event_serial = next_nonzero(
        lifecycle_snapshot.event_serial);
    if (after) {
        ++lifecycle_snapshot.remove_all_players_after_count;
        lifecycle_snapshot.group_count_after_remove_all = group->count;
        lifecycle_snapshot.formation_count_after_remove_all = formation->count;
        lifecycle_snapshot.remove_all_verified_empty =
            verified_empty ? 1u : 0u;
    } else {
        ++lifecycle_snapshot.remove_all_players_before_count;
        lifecycle_snapshot.group_count_before_remove_all = group->count;
        lifecycle_snapshot.formation_count_before_remove_all = formation->count;
    }
    SudekiMpLogFormat(
        "talos_lifecycle event=RemoveAllPlayers_native phase=%s "
        "run=%08lx%08lx serial=%lu exact_opcode_context=%s "
        "group_readable=%s group_owner=%s group_count=%lu "
        "group_mask=0x%02lx formation_readable=%s formation_owner=%s "
        "formation_count=%lu formation_mask=0x%02lx verified_empty=%s "
        "group_hero_mask=0x%02lx formation_hero_mask=0x%02lx "
        "hero_maps_equal=%s hero_present_mask=0x%02lx "
        "hero_lease_generations=%lu,%lu,%lu,%lu sequence_state=%lu "
        "actor_lifetime_authority=false "
        "native_thread=%lu authority=unproven "
        "policy=observation_only_original_called_once_no_mutation\r\n",
        phase,
        (unsigned long)lifecycle_snapshot.run_id_high,
        (unsigned long)lifecycle_snapshot.run_id_low,
        (unsigned long)lifecycle_snapshot.event_serial,
        exact_context ? "true" : "false",
        group->readable ? "true" : "false",
        group->owner_present ? "true" : "false",
        (unsigned long)group->count,
        (unsigned long)group->occupied_mask,
        formation->readable ? "true" : "false",
        formation->owner_present ? "true" : "false",
        (unsigned long)formation->count,
        (unsigned long)formation->occupied_mask,
        verified_empty ? "true" : "false",
        (unsigned long)group->hero_mask,
        (unsigned long)formation->hero_mask,
        roster_identity_maps_equal(group, formation) ? "true" : "false",
        (unsigned long)roster_identity_tracker.last_present_mask,
        (unsigned long)roster_identity_tracker.lease_generation[0],
        (unsigned long)roster_identity_tracker.lease_generation[1],
        (unsigned long)roster_identity_tracker.lease_generation[2],
        (unsigned long)roster_identity_tracker.lease_generation[3],
        (unsigned long)roster_identity_tracker.sequence_state,
        (unsigned long)GetCurrentThreadId());
}

static void __cdecl observe_remove_all_players(void) {
    DWORD incoming_error = GetLastError();
    TalosOpcodeContext *context = current_opcode_27_context();
    TalosTaskLineage lineage;
    DWORD result_error;
    NativeRosterSample group_before = sample_native_roster(
        RVA_ACTIVE_GROUP_GLOBAL, GROUP_MEMBERS_OFFSET,
        GROUP_MEMBER_STRIDE, GROUP_COUNT_OFFSET);
    NativeRosterSample formation_before = sample_native_roster(
        RVA_AI_MANAGER_GLOBAL, FORMATION_MEMBERS_OFFSET,
        FORMATION_MEMBER_STRIDE, FORMATION_COUNT_OFFSET);
    NativeRosterSample group_after;
    NativeRosterSample formation_after;
    BOOL pre_cleanup_valid = FALSE;

    memset(&lineage, 0, sizeof(lineage));
    (void)copy_task_lineage(&lineage);
    BOOL tracked_cleanup = context != NULL && context->exact &&
        context->event == SUDEKIMP_TALOS_NATIVE_EVENT_REMOVE_ALL_PLAYERS &&
        lineage.valid &&
        context->runtime_generation == lineage.runtime_generation &&
        context->native_thread_id == lineage.native_thread_id &&
        context->matched_load_void_task_generation ==
            lineage.load_void_task_generation &&
        roster_identity_tracker.sequence_state ==
            SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_COMPANIONS_REMOVED;

    if (tracked_cleanup) {
        pre_cleanup_valid = roster_matches_original_tal_survivor(
                &group_before, &formation_before) &&
            observe_composite_roster_identity(
                &group_before, &formation_before);
        if (!pre_cleanup_valid)
            quarantine_roster_sequence(
                7u, &group_before, &formation_before);
    }
    record_remove_all_players_edge(
        context, "before", &group_before, &formation_before);
    SetLastError(incoming_error);
    original_remove_all_players();
    result_error = GetLastError();
    group_after = sample_native_roster(
        RVA_ACTIVE_GROUP_GLOBAL, GROUP_MEMBERS_OFFSET,
        GROUP_MEMBER_STRIDE, GROUP_COUNT_OFFSET);
    formation_after = sample_native_roster(
        RVA_AI_MANAGER_GLOBAL, FORMATION_MEMBERS_OFFSET,
        FORMATION_MEMBER_STRIDE, FORMATION_COUNT_OFFSET);
    if (tracked_cleanup && pre_cleanup_valid) {
        if (observe_composite_roster_identity(
                &group_after, &formation_after) &&
            roster_sample_empty(&group_after) &&
            roster_sample_empty(&formation_after)) {
            roster_identity_tracker.sequence_state =
                SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_RELEASED;
            roster_identity_tracker.observation_serial = next_nonzero(
                roster_identity_tracker.observation_serial);
            publish_roster_identity(&group_after, &formation_after, TRUE);
        } else {
            quarantine_roster_sequence(4u, &group_after, &formation_after);
        }
    }
    record_remove_all_players_edge(
        context, "after", &group_after, &formation_after);
    SetLastError(result_error);
}

static void record_formation_pop_edge(
    const char *phase,
    const NativeRosterSample *group,
    const NativeRosterSample *formation
) {
    BOOL after = phase[0] == 'a';

    lifecycle_snapshot.event_serial = next_nonzero(
        lifecycle_snapshot.event_serial);
    lifecycle_snapshot.observed_event_mask |=
        (uint32_t)1u <<
        (unsigned int)SUDEKIMP_TALOS_NATIVE_EVENT_FORMATION_POP_MEMBERS;
    if (after) ++lifecycle_snapshot.formation_pop_after_count;
    else ++lifecycle_snapshot.formation_pop_before_count;
    SudekiMpLogFormat(
        "talos_lifecycle event=AiPCFormationPopMembers_native phase=%s "
        "run=%08lx%08lx serial=%lu group_readable=%s group_count=%lu "
        "group_mask=0x%02lx formation_readable=%s formation_count=%lu "
        "formation_mask=0x%02lx "
        "group_tokens=%08lx%08lx,%08lx%08lx,%08lx%08lx,%08lx%08lx "
        "formation_tokens=%08lx%08lx,%08lx%08lx,%08lx%08lx,%08lx%08lx "
        "identity_sets_equal=%s group_hero_mask=0x%02lx "
        "formation_hero_mask=0x%02lx hero_maps_equal=%s "
        "native_thread=%lu authority=unproven "
        "policy=observation_only_original_called_once_no_mutation\r\n",
        phase,
        (unsigned long)lifecycle_snapshot.run_id_high,
        (unsigned long)lifecycle_snapshot.run_id_low,
        (unsigned long)lifecycle_snapshot.event_serial,
        group->readable ? "true" : "false",
        (unsigned long)group->count,
        (unsigned long)group->occupied_mask,
        formation->readable ? "true" : "false",
        (unsigned long)formation->count,
        (unsigned long)formation->occupied_mask,
        (unsigned long)(group->member_tokens[0] >> 32u),
        (unsigned long)group->member_tokens[0],
        (unsigned long)(group->member_tokens[1] >> 32u),
        (unsigned long)group->member_tokens[1],
        (unsigned long)(group->member_tokens[2] >> 32u),
        (unsigned long)group->member_tokens[2],
        (unsigned long)(group->member_tokens[3] >> 32u),
        (unsigned long)group->member_tokens[3],
        (unsigned long)(formation->member_tokens[0] >> 32u),
        (unsigned long)formation->member_tokens[0],
        (unsigned long)(formation->member_tokens[1] >> 32u),
        (unsigned long)formation->member_tokens[1],
        (unsigned long)(formation->member_tokens[2] >> 32u),
        (unsigned long)formation->member_tokens[2],
        (unsigned long)(formation->member_tokens[3] >> 32u),
        (unsigned long)formation->member_tokens[3],
        roster_identity_sets_equal(group, formation) ? "true" : "false",
        (unsigned long)group->hero_mask,
        (unsigned long)formation->hero_mask,
        roster_identity_maps_equal(group, formation) ? "true" : "false",
        (unsigned long)GetCurrentThreadId());
}

static void __cdecl observe_formation_pop_members(void) {
    DWORD incoming_error = GetLastError();
    DWORD result_error;
    NativeRosterSample group_before = sample_native_roster(
        RVA_ACTIVE_GROUP_GLOBAL, GROUP_MEMBERS_OFFSET,
        GROUP_MEMBER_STRIDE, GROUP_COUNT_OFFSET);
    NativeRosterSample formation_before = sample_native_roster(
        RVA_AI_MANAGER_GLOBAL, FORMATION_MEMBERS_OFFSET,
        FORMATION_MEMBER_STRIDE, FORMATION_COUNT_OFFSET);
    NativeRosterSample group_after;
    NativeRosterSample formation_after;

    record_formation_pop_edge("before", &group_before, &formation_before);
    SetLastError(incoming_error);
    original_formation_pop_members();
    result_error = GetLastError();
    group_after = sample_native_roster(
        RVA_ACTIVE_GROUP_GLOBAL, GROUP_MEMBERS_OFFSET,
        GROUP_MEMBER_STRIDE, GROUP_COUNT_OFFSET);
    formation_after = sample_native_roster(
        RVA_AI_MANAGER_GLOBAL, FORMATION_MEMBERS_OFFSET,
        FORMATION_MEMBER_STRIDE, FORMATION_COUNT_OFFSET);
    record_formation_pop_edge("after", &group_after, &formation_after);
    SetLastError(result_error);
}

static BOOL exact_tsa_set_playing_false_context(
    const TalosOpcodeContext *context
) {
    TalosTaskLineage lineage;

    memset(&lineage, 0, sizeof(lineage));
    return context != NULL && context->exact && context->opcode == 0x27u &&
        context->operand_offset == TSA_SET_PLAYING_FALSE_OPERAND &&
        context->binding_hash == HASH_TSA_SET_PLAYING &&
        copy_task_lineage(&lineage) && lineage.valid &&
        context->runtime_generation == lineage.runtime_generation &&
        context->native_thread_id == lineage.native_thread_id &&
        context->lineage_matched &&
        context->thread_token == lineage.load_void_thread_token &&
        context->matched_load_void_task_generation ==
            lineage.load_void_task_generation &&
        context->native_thread_id == GetCurrentThreadId();
}

BOOL SudekiMpTalosNativeLifecycleTsaInactiveEvidencePolicy(
    BOOL armed,
    uint8_t opcode,
    uint32_t operand_offset,
    uint32_t binding_hash,
    BOOL runtime_generation_matched,
    BOOL native_thread_matched,
    BOOL script_task_lineage_matched,
    uint8_t requested,
    BOOL before_playing,
    BOOL after_playing
) {
    return armed && opcode == 0x27u &&
        operand_offset == TSA_SET_PLAYING_FALSE_OPERAND &&
        binding_hash == HASH_TSA_SET_PLAYING &&
        runtime_generation_matched && native_thread_matched &&
        script_task_lineage_matched &&
        requested == 0u && before_playing && !after_playing;
}

BOOL SudekiMpTalosNativeLifecycleDefaultCameraEvidencePolicy(
    BOOL void_set_zone_completed,
    BOOL runtime_generation_matched,
    BOOL load_void_task_generation_matched,
    BOOL native_thread_matched,
    BOOL native_call_succeeded,
    BOOL exact_default_name,
    BOOL committed_camera_state_valid
) {
    return void_set_zone_completed && runtime_generation_matched &&
        load_void_task_generation_matched && native_thread_matched &&
        native_call_succeeded && exact_default_name &&
        committed_camera_state_valid;
}

BOOL SudekiMpTalosNativeLifecycleSettleEvidencePolicy(
    BOOL exact_original_tal_survivor,
    BOOL default_camera_committed,
    BOOL default_camera_revalidated,
    BOOL tal_control_revalidated
) {
    return exact_original_tal_survivor && default_camera_committed &&
        default_camera_revalidated && tal_control_revalidated;
}

BOOL SudekiMpTalosNativeLifecyclePostMovieRestoreTicketPolicy(
    BOOL allow_post_movie_restore_ticket,
    BOOL exact_asset_authenticated,
    const SudekiMpTalosNativeLifecycleSnapshot *lifecycle,
    const SudekiMpTalosNativeRosterIdentitySnapshot *roster,
    const SudekiMpTalosNativeKazelSnapshot *kazel,
    const SudekiMpTalosNativeSettleEvidenceSnapshot *settle,
    uint32_t current_native_thread_id,
    uint32_t settle_native_thread_id
) {
    uint32_t runtime_generation;
    uint32_t task_generation;
    uint64_t tal_token;

    if (!allow_post_movie_restore_ticket || !exact_asset_authenticated ||
        lifecycle == NULL || roster == NULL || kazel == NULL ||
        settle == NULL || lifecycle->installed == 0u ||
        (lifecycle->run_id_high | lifecycle->run_id_low) == 0u ||
        lifecycle->event_serial == 0u ||
        lifecycle->load_void_task_bound == 0u ||
        lifecycle->load_void_descendant_observed == 0u ||
        lifecycle->tsa_inactive_observed == 0u ||
        lifecycle->opcode_27_depth != 0u ||
        lifecycle->opcode_29_depth != 0u ||
        current_native_thread_id == 0u || settle_native_thread_id == 0u)
        return FALSE;

    runtime_generation = lifecycle->script_runtime_generation;
    task_generation = lifecycle->load_void_task_generation;
    tal_token = roster->hero_token[SUDEKIMP_TALOS_NATIVE_HERO_TAL];
    if (runtime_generation == 0u || task_generation == 0u ||
        roster->observation_serial == 0u || roster->roster_revision == 0u ||
        roster->script_runtime_generation != runtime_generation ||
        roster->load_void_task_generation != task_generation ||
        roster->sequence_state !=
            SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_COMPANIONS_REMOVED ||
        roster->quarantine_reason != 0u ||
        roster->hero_present_mask != HERO_MASK_TAL ||
        roster->group_hero_mask != HERO_MASK_TAL ||
        roster->formation_hero_mask != HERO_MASK_TAL ||
        roster->delete_delta_corroborated_mask !=
            (HERO_MASK_AILISH | HERO_MASK_BUKI | HERO_MASK_ELCO) ||
        tal_token == 0u) return FALSE;

    if (kazel->session_generation == 0u ||
        kazel->observation_serial == 0u ||
        kazel->request_generation == 0u ||
        kazel->script_runtime_generation != runtime_generation ||
        kazel->load_void_task_generation != task_generation ||
        kazel->source_native_thread_id == 0u ||
        kazel->completion_native_thread_id == 0u ||
        kazel->source_native_thread_id !=
            kazel->completion_native_thread_id ||
        kazel->source_native_thread_id != current_native_thread_id ||
        settle_native_thread_id != current_native_thread_id ||
        kazel->state != SUDEKIMP_TALOS_NATIVE_KAZEL_DELETE_CORROBORATED ||
        kazel->spawn_binding_before_seen == 0u ||
        kazel->spawn_binding_after_seen == 0u ||
        kazel->group_add_before_seen == 0u ||
        kazel->group_add_after_seen == 0u ||
        kazel->exact_dark_tal_identity == 0u ||
        kazel->group_add_corroborated == 0u ||
        kazel->delete_corroborated == 0u ||
        kazel->serialized_opcode_mask != KAZEL_SERIAL_COMPLETE ||
        kazel->ambiguity_reason != 0u ||
        kazel->original_tal_token != tal_token ||
        kazel->kazel_token == 0u || kazel->kazel_token == tal_token)
        return FALSE;

    return settle->session_generation != 0u &&
        settle->session_generation == kazel->session_generation &&
        settle->script_runtime_generation == runtime_generation &&
        settle->load_void_task_generation == task_generation &&
        settle->default_camera_generation != 0u &&
        settle->settle_validation_generation != 0u &&
        settle->void_set_zone_completed != 0u &&
        settle->default_camera_committed != 0u &&
        settle->default_camera_revalidated != 0u &&
        settle->tal_control_revalidated != 0u &&
        settle->settle_evidence_complete != 0u;
}

BOOL SudekiMpTalosNativeLifecyclePostMovieRestoreReadyClaimPolicy(
    const SudekiMpTalosNativePostMovieRestoreTicket *ticket,
    const SudekiMpTalosNativeLifecycleSnapshot *lifecycle,
    const SudekiMpTalosNativeRosterIdentitySnapshot *roster,
    const SudekiMpTalosNativeKazelSnapshot *kazel,
    const SudekiMpTalosNativeSettleEvidenceSnapshot *settle,
    uint32_t current_native_thread_id,
    uint32_t settle_native_thread_id
) {
    uint64_t tal_token;

    if (ticket == NULL || lifecycle == NULL || roster == NULL ||
        kazel == NULL || settle == NULL || lifecycle->installed == 0u ||
        (ticket->run_id_high | ticket->run_id_low) == 0u ||
        ticket->authorization_generation == 0u ||
        ticket->lifecycle_event_serial == 0u ||
        ticket->roster_observation_serial == 0u ||
        ticket->roster_revision == 0u ||
        ticket->kazel_observation_serial == 0u ||
        ticket->kazel_request_generation == 0u ||
        ticket->session_generation == 0u ||
        ticket->script_runtime_generation == 0u ||
        ticket->load_void_task_generation == 0u ||
        ticket->settle_validation_generation == 0u ||
        ticket->default_camera_generation == 0u ||
        ticket->native_thread_id == 0u || ticket->tal_token == 0u ||
        ticket->kazel_token == 0u ||
        ticket->tal_token == ticket->kazel_token ||
        current_native_thread_id == 0u || settle_native_thread_id == 0u)
        return FALSE;

    if (lifecycle->run_id_high != ticket->run_id_high ||
        lifecycle->run_id_low != ticket->run_id_low ||
        lifecycle->event_serial != ticket->lifecycle_event_serial ||
        lifecycle->script_runtime_generation !=
            ticket->script_runtime_generation ||
        lifecycle->load_void_task_generation !=
            ticket->load_void_task_generation ||
        lifecycle->load_void_task_bound == 0u ||
        lifecycle->load_void_descendant_observed == 0u ||
        lifecycle->tsa_inactive_observed == 0u ||
        lifecycle->opcode_27_depth != 0u ||
        lifecycle->opcode_29_depth != 0u ||
        current_native_thread_id != ticket->native_thread_id ||
        settle_native_thread_id != ticket->native_thread_id) return FALSE;

    tal_token = roster->hero_token[SUDEKIMP_TALOS_NATIVE_HERO_TAL];
    if (roster->observation_serial != ticket->roster_observation_serial ||
        roster->roster_revision != ticket->roster_revision ||
        roster->script_runtime_generation !=
            ticket->script_runtime_generation ||
        roster->load_void_task_generation !=
            ticket->load_void_task_generation ||
        roster->sequence_state !=
            SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_COMPANIONS_REMOVED ||
        roster->quarantine_reason != 0u ||
        roster->hero_present_mask != HERO_MASK_TAL ||
        roster->group_hero_mask != HERO_MASK_TAL ||
        roster->formation_hero_mask != HERO_MASK_TAL ||
        roster->delete_delta_corroborated_mask !=
            (HERO_MASK_AILISH | HERO_MASK_BUKI | HERO_MASK_ELCO) ||
        tal_token != ticket->tal_token) return FALSE;

    if (kazel->session_generation != ticket->session_generation ||
        kazel->observation_serial != ticket->kazel_observation_serial ||
        kazel->request_generation != ticket->kazel_request_generation ||
        kazel->script_runtime_generation !=
            ticket->script_runtime_generation ||
        kazel->load_void_task_generation !=
            ticket->load_void_task_generation ||
        kazel->source_native_thread_id != ticket->native_thread_id ||
        kazel->completion_native_thread_id != ticket->native_thread_id ||
        kazel->state != SUDEKIMP_TALOS_NATIVE_KAZEL_DELETE_CORROBORATED ||
        kazel->spawn_binding_before_seen == 0u ||
        kazel->spawn_binding_after_seen == 0u ||
        kazel->group_add_before_seen == 0u ||
        kazel->group_add_after_seen == 0u ||
        kazel->exact_dark_tal_identity == 0u ||
        kazel->group_add_corroborated == 0u ||
        kazel->delete_corroborated == 0u ||
        kazel->serialized_opcode_mask != KAZEL_SERIAL_COMPLETE ||
        kazel->ambiguity_reason != 0u ||
        kazel->original_tal_token != ticket->tal_token ||
        kazel->kazel_token != ticket->kazel_token) return FALSE;

    /* The ticket's nonzero default-camera generation proves the historical
     * boundary. Current default-camera generation and all five volatile flags
     * are intentionally ignored: retail may reacquire default repeatedly
     * before the next controller update. */
    return settle->session_generation == ticket->session_generation &&
        settle->script_runtime_generation ==
            ticket->script_runtime_generation &&
        settle->load_void_task_generation ==
            ticket->load_void_task_generation &&
        settle->settle_validation_generation ==
            ticket->settle_validation_generation;
}

SudekiMpTalosNativePostMovieTicketClaimResult
SudekiMpTalosNativeLifecyclePostMovieTicketClaimTransitionPolicy(
    uint8_t current_state,
    BOOL exact_evidence_ready,
    BOOL irreversible_mismatch,
    uint8_t *next_state
) {
    if (next_state == NULL)
        return SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_INVALID;
    *next_state = current_state;
    if (current_state ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_DISABLED)
        return SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_DISABLED;
    if (current_state ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_CLAIMED)
        return SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_REPLAY;
    if (current_state ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_QUARANTINED) {
        return
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_QUARANTINED;
    }
    if (current_state !=
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_WAITING &&
        current_state !=
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_ACTIVE &&
        current_state !=
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_READY) {
        *next_state =
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_QUARANTINED;
        return
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_QUARANTINED;
    }
    if (irreversible_mismatch ||
        (current_state ==
             SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_READY &&
         !exact_evidence_ready) ||
        (current_state ==
             SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_WAITING &&
         exact_evidence_ready)) {
        *next_state =
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_QUARANTINED;
        return
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_REJECTED_QUARANTINED;
    }
    if (!exact_evidence_ready || current_state !=
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_READY)
        return SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_NOT_READY;
    *next_state = SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_CLAIMED;
    return SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_AUTHORIZED;
}

BOOL SudekiMpTalosNativeLifecycleKazelGroupAddEvidencePolicy(
    BOOL pending_exact_request,
    BOOL runtime_generation_matched,
    BOOL load_void_task_generation_matched,
    BOOL native_thread_matched,
    BOOL group_argument_matches_active,
    BOOL exact_dark_tal_identity,
    uint32_t group_count_before,
    uint32_t group_count_after,
    uint32_t formation_count_before,
    uint32_t formation_count_after,
    uint64_t original_tal_token,
    uint64_t group_added_token,
    uint64_t formation_added_token,
    uint64_t raw_actor_token
) {
    return pending_exact_request && runtime_generation_matched &&
        load_void_task_generation_matched && native_thread_matched &&
        group_argument_matches_active && exact_dark_tal_identity &&
        group_count_before == 1u && group_count_after == 2u &&
        formation_count_before == 1u && formation_count_after == 2u &&
        original_tal_token != 0u && group_added_token != 0u &&
        group_added_token != original_tal_token &&
        group_added_token == formation_added_token &&
        group_added_token == raw_actor_token;
}

BOOL SudekiMpTalosNativeLifecycleKazelSessionStartPolicy(
    uint32_t prior_session_generation
) {
    return prior_session_generation == 0u;
}

BOOL SudekiMpTalosNativeLifecycleKazelSerializedSequencePolicy(
    uint8_t current_mask,
    SudekiMpTalosNativeLifecycleEvent event,
    BOOL before,
    BOOL exact_context,
    uint8_t *next_mask
) {
    uint8_t expected;
    uint8_t bit;

    if (next_mask == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    *next_mask = current_mask;
    if (!exact_context) return FALSE;
    if (event == SUDEKIMP_TALOS_NATIVE_EVENT_TAL_KAZEL_MERGE) {
        expected = before ? 0u : KAZEL_SERIAL_MERGE_BEFORE;
        bit = before ? KAZEL_SERIAL_MERGE_BEFORE : KAZEL_SERIAL_MERGE_AFTER;
    } else if (event ==
            SUDEKIMP_TALOS_NATIVE_EVENT_SPAWN_KAZEL_WRAPPER) {
        expected = before ?
            (KAZEL_SERIAL_MERGE_BEFORE | KAZEL_SERIAL_MERGE_AFTER) :
            (KAZEL_SERIAL_MERGE_BEFORE | KAZEL_SERIAL_MERGE_AFTER |
             KAZEL_SERIAL_SPAWN_WRAPPER_BEFORE);
        bit = before ? KAZEL_SERIAL_SPAWN_WRAPPER_BEFORE :
            KAZEL_SERIAL_SPAWN_WRAPPER_AFTER;
    } else if (event ==
            SUDEKIMP_TALOS_NATIVE_EVENT_INTERNAL_SPAWN_PC) {
        expected = before ? KAZEL_SERIAL_READY_FOR_INTERNAL_SPAWN :
            (KAZEL_SERIAL_READY_FOR_INTERNAL_SPAWN |
             KAZEL_SERIAL_INTERNAL_SPAWN_BEFORE);
        bit = before ? KAZEL_SERIAL_INTERNAL_SPAWN_BEFORE :
            KAZEL_SERIAL_INTERNAL_SPAWN_AFTER;
    } else {
        return FALSE;
    }
    if (current_mask != expected) return FALSE;
    *next_mask = (uint8_t)(current_mask | bit);
    return TRUE;
}

static BOOL exact_readable_text(const char *text, const char *expected) {
    size_t expected_size;

    if (text == NULL || expected == NULL) return FALSE;
    expected_size = strlen(expected) + 1u;
    return readable_memory(text, expected_size) &&
        memcmp(text, expected, expected_size) == 0;
}

static BOOL validate_default_camera_state(const void *expected_manager) {
    void *manager;
    void *current_camera;
    void *table_camera;
    void *camera_render_state;
    void *scene_manager;
    void *scene_renderer;
    void *scene_camera_state;
    unsigned int index;
    BOOL table_contains_current = FALSE;

    if (game_base == NULL ||
        !readable_memory(game_base + RVA_CAMERA_MANAGER_GLOBAL,
            sizeof(manager)) ||
        !readable_memory(game_base + RVA_SCENE_MANAGER_GLOBAL,
            sizeof(scene_manager))) return FALSE;
    memcpy(&manager, game_base + RVA_CAMERA_MANAGER_GLOBAL,
        sizeof(manager));
    memcpy(&scene_manager, game_base + RVA_SCENE_MANAGER_GLOBAL,
        sizeof(scene_manager));
    if (manager == NULL || scene_manager == NULL ||
        (expected_manager != NULL && manager != expected_manager) ||
        !readable_memory((uint8_t *)manager + CAMERA_MANAGER_CURRENT_OFFSET,
            sizeof(current_camera)) ||
        !readable_memory((uint8_t *)manager + CAMERA_MANAGER_TABLE_OFFSET,
            CAMERA_MANAGER_TABLE_COUNT * sizeof(table_camera)) ||
        !readable_memory((uint8_t *)scene_manager +
            SCENE_MANAGER_RENDERER_OFFSET, sizeof(scene_renderer))) {
        return FALSE;
    }
    memcpy(&current_camera,
        (uint8_t *)manager + CAMERA_MANAGER_CURRENT_OFFSET,
        sizeof(current_camera));
    if (current_camera == NULL) return FALSE;
    for (index = 0u; index < CAMERA_MANAGER_TABLE_COUNT; ++index) {
        memcpy(&table_camera,
            (uint8_t *)manager + CAMERA_MANAGER_TABLE_OFFSET +
                index * sizeof(table_camera),
            sizeof(table_camera));
        if (table_camera == current_camera) table_contains_current = TRUE;
    }
    if (!table_contains_current ||
        !exact_readable_text((const char *)current_camera +
            CAMERA_NAME_OFFSET, "default") ||
        !readable_memory((uint8_t *)current_camera +
            CAMERA_RENDER_STATE_OFFSET, sizeof(camera_render_state))) {
        return FALSE;
    }
    memcpy(&camera_render_state,
        (uint8_t *)current_camera + CAMERA_RENDER_STATE_OFFSET,
        sizeof(camera_render_state));
    memcpy(&scene_renderer,
        (uint8_t *)scene_manager + SCENE_MANAGER_RENDERER_OFFSET,
        sizeof(scene_renderer));
    if (camera_render_state == NULL || scene_renderer == NULL ||
        !readable_memory((uint8_t *)scene_renderer +
            SCENE_RENDERER_CAMERA_STATE_OFFSET,
            sizeof(scene_camera_state))) return FALSE;
    memcpy(&scene_camera_state,
        (uint8_t *)scene_renderer + SCENE_RENDERER_CAMERA_STATE_OFFSET,
        sizeof(scene_camera_state));
    return scene_camera_state == camera_render_state;
}

static void *roster_hero_actor(
    const NativeRosterSample *sample,
    SudekiMpTalosNativeHeroIdentity hero
) {
    unsigned int index;

    if (sample == NULL || !sample->identity_complete) return NULL;
    for (index = 0u; index < NATIVE_MEMBER_LIMIT; ++index) {
        if (sample->hero_by_slot[index] == (uint8_t)hero)
            return sample->members[index];
    }
    return NULL;
}

static BOOL validate_tal_control_state(const NativeRosterSample *group) {
    void *tal_actor = roster_hero_actor(
        group, SUDEKIMP_TALOS_NATIVE_HERO_TAL);
    void *controller;
    void *controller_target;
    void *ai_component;
    void *ai_state;
    uint32_t current_mode;
    uint32_t requested_mode;
    uint8_t ai_mode;
    int16_t override_count;

    if (game_base == NULL || tal_actor == NULL ||
        !readable_memory(game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller))) return FALSE;
    memcpy(&controller, game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
        sizeof(controller));
    if (controller == NULL ||
        !readable_memory((uint8_t *)controller +
            CONTROLLER_CURRENT_MODE_OFFSET, sizeof(current_mode)) ||
        !readable_memory((uint8_t *)controller +
            CONTROLLER_REQUESTED_MODE_OFFSET, sizeof(requested_mode)) ||
        !readable_memory((uint8_t *)controller + CONTROLLER_TARGET_OFFSET,
            sizeof(controller_target)) ||
        !readable_memory((uint8_t *)tal_actor + ACTOR_AI_COMPONENT_OFFSET,
            sizeof(ai_component))) return FALSE;
    memcpy(&current_mode,
        (uint8_t *)controller + CONTROLLER_CURRENT_MODE_OFFSET,
        sizeof(current_mode));
    memcpy(&requested_mode,
        (uint8_t *)controller + CONTROLLER_REQUESTED_MODE_OFFSET,
        sizeof(requested_mode));
    memcpy(&controller_target,
        (uint8_t *)controller + CONTROLLER_TARGET_OFFSET,
        sizeof(controller_target));
    memcpy(&ai_component,
        (uint8_t *)tal_actor + ACTOR_AI_COMPONENT_OFFSET,
        sizeof(ai_component));
    if (controller_target != tal_actor || current_mode != 1u ||
        requested_mode != 1u || ai_component == NULL ||
        !readable_memory((uint8_t *)ai_component +
            AI_COMPONENT_STATE_OFFSET, sizeof(ai_state)) ||
        !readable_memory((uint8_t *)ai_component +
            AI_COMPONENT_OVERRIDE_COUNT_OFFSET, sizeof(override_count))) {
        return FALSE;
    }
    memcpy(&ai_state,
        (uint8_t *)ai_component + AI_COMPONENT_STATE_OFFSET,
        sizeof(ai_state));
    memcpy(&override_count,
        (uint8_t *)ai_component + AI_COMPONENT_OVERRIDE_COUNT_OFFSET,
        sizeof(override_count));
    if (ai_state == NULL ||
        !readable_memory((uint8_t *)ai_state + AI_STATE_MODE_OFFSET,
            sizeof(ai_mode))) return FALSE;
    memcpy(&ai_mode, (uint8_t *)ai_state + AI_STATE_MODE_OFFSET,
        sizeof(ai_mode));
    return ai_mode == 0u && override_count == 0;
}

static void __cdecl observe_tsa_set_playing(unsigned char requested) {
    DWORD incoming_error = GetLastError();
    TalosOpcodeContext *context = current_opcode_27_context();
    TalosTaskLineage lineage_after;
    uint32_t caller_rva = game_address_rva(__builtin_return_address(0));
    BOOL exact_context = exact_tsa_set_playing_false_context(context);
    BOOL armed = lifecycle_snapshot.tsa_completion_armed != 0u;
    BOOL before = tsa_is_playing != NULL && tsa_is_playing() != 0u;
    BOOL after;
    BOOL completed;
    BOOL roster_settle_valid = FALSE;
    BOOL default_camera_revalidated = FALSE;
    BOOL tal_control_revalidated = FALSE;
    BOOL settle_evidence_complete = FALSE;
    BOOL default_camera_committed = FALSE;
    uint32_t settle_validation_generation = 0u;
    NativeRosterSample group_settle;
    NativeRosterSample formation_settle;
    DWORD result_error;

    memset(&group_settle, 0, sizeof(group_settle));
    memset(&formation_settle, 0, sizeof(formation_settle));
    memset(&lineage_after, 0, sizeof(lineage_after));
    SetLastError(incoming_error);
    original_tsa_set_playing(requested);
    result_error = GetLastError();
    after = tsa_is_playing != NULL && tsa_is_playing() != 0u;
    (void)copy_task_lineage(&lineage_after);
    completed = exact_context &&
        SudekiMpTalosNativeLifecycleTsaInactiveEvidencePolicy(
            armed, context->opcode, context->operand_offset,
            context->binding_hash,
            lineage_after.valid && context->runtime_generation ==
                lineage_after.runtime_generation,
            lineage_after.valid && context->native_thread_id ==
                lineage_after.native_thread_id,
            context->lineage_matched &&
                lineage_after.valid && context->thread_token ==
                    lineage_after.load_void_thread_token &&
                context->matched_load_void_task_generation ==
                    lineage_after.load_void_task_generation,
            requested, before, after);
    if (exact_context && armed) {
        lifecycle_snapshot.event_serial = next_nonzero(
            lifecycle_snapshot.event_serial);
        lifecycle_snapshot.tsa_set_playing_observation_count = next_nonzero(
            lifecycle_snapshot.tsa_set_playing_observation_count);
        lifecycle_snapshot.last_tsa_requested = requested != 0u ? 1u : 0u;
        lifecycle_snapshot.last_tsa_playing = after ? 1u : 0u;
        lifecycle_snapshot.tsa_inactive_caller_rva = caller_rva;
        if (completed) {
            group_settle = sample_native_roster(
                RVA_ACTIVE_GROUP_GLOBAL, GROUP_MEMBERS_OFFSET,
                GROUP_MEMBER_STRIDE, GROUP_COUNT_OFFSET);
            formation_settle = sample_native_roster(
                RVA_AI_MANAGER_GLOBAL, FORMATION_MEMBERS_OFFSET,
                FORMATION_MEMBER_STRIDE, FORMATION_COUNT_OFFSET);
            if (roster_identity_tracker.sequence_state ==
                    SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_COMPANIONS_REMOVED) {
                roster_settle_valid =
                    roster_matches_original_tal_survivor(
                        &group_settle, &formation_settle) &&
                    observe_composite_roster_identity(
                        &group_settle, &formation_settle);
                if (!roster_settle_valid) {
                    quarantine_roster_sequence(
                        6u, &group_settle, &formation_settle);
                }
            } else {
                invalidate_settle_evidence(TRUE);
            }
            if (roster_settle_valid) {
                acquire_settle_evidence_writer();
                if (lifecycle_snapshot.installed != 0u &&
                    settle_evidence_tracker.session_generation != 0u) {
                    settle_evidence_tracker.settle_validation_generation =
                        next_nonzero(settle_evidence_tracker.
                            settle_validation_generation);
                    if (settle_evidence_tracker.void_set_zone_completed &&
                        settle_evidence_tracker.script_runtime_generation ==
                            context->runtime_generation &&
                        settle_evidence_tracker.load_void_task_generation ==
                            lifecycle_snapshot.load_void_task_generation &&
                        settle_evidence_tracker.native_thread_id ==
                            context->native_thread_id) {
                        default_camera_revalidated =
                            settle_evidence_tracker.
                                default_camera_committed &&
                            validate_default_camera_state(NULL);
                        tal_control_revalidated =
                            validate_tal_control_state(&group_settle);
                    }
                    settle_evidence_complete =
                        SudekiMpTalosNativeLifecycleSettleEvidencePolicy(
                            TRUE,
                            settle_evidence_tracker.
                                default_camera_committed,
                            default_camera_revalidated,
                            tal_control_revalidated);
                    settle_evidence_tracker.default_camera_revalidated =
                        default_camera_revalidated ? 1u : 0u;
                    settle_evidence_tracker.tal_control_revalidated =
                        tal_control_revalidated ? 1u : 0u;
                    settle_evidence_tracker.settle_evidence_complete =
                        settle_evidence_complete ? 1u : 0u;
                    publish_settle_evidence_locked();
                    default_camera_committed =
                        settle_evidence_tracker.default_camera_committed != 0u;
                    settle_validation_generation =
                        settle_evidence_tracker.
                            settle_validation_generation;
                }
                release_settle_evidence_writer();
            }
            lifecycle_snapshot.tsa_completion_armed = 0u;
            lifecycle_snapshot.tsa_inactive_observed = 1u;
            lifecycle_snapshot.observed_event_mask |=
                (uint32_t)1u << (unsigned int)
                    SUDEKIMP_TALOS_NATIVE_EVENT_TSA_BECAME_INACTIVE;
        }
        SudekiMpLogFormat(
            "talos_lifecycle event=TSASetPlaying_native phase=after "
            "run=%08lx%08lx serial=%lu requested=%s before=%s after=%s "
            "completed=%s operand=0x%08lx hash=0x%08lx caller_rva=0x%08lx "
            "group_hero_mask=0x%02lx formation_hero_mask=0x%02lx "
            "roster_settle_valid=%s default_camera_committed=%s "
            "default_camera_revalidated=%s tal_control_revalidated=%s "
            "settle_evidence_complete=%s settle_validation_generation=%lu "
            "sequence_state=%lu "
            "native_thread=%lu runtime_generation=%lu "
            "source_end_tsa_armed=true setter_task_lineage_matched=%s "
            "authority=unproven "
            "policy=observation_only_original_called_once_no_mutation\r\n",
            (unsigned long)lifecycle_snapshot.run_id_high,
            (unsigned long)lifecycle_snapshot.run_id_low,
            (unsigned long)lifecycle_snapshot.event_serial,
            requested != 0u ? "true" : "false",
            before ? "true" : "false",
            after ? "true" : "false",
            completed ? "true" : "false",
            (unsigned long)context->operand_offset,
            (unsigned long)context->binding_hash,
            (unsigned long)caller_rva,
            (unsigned long)group_settle.hero_mask,
            (unsigned long)formation_settle.hero_mask,
            roster_settle_valid ? "true" : "false",
            default_camera_committed ? "true" : "false",
            default_camera_revalidated ? "true" : "false",
            tal_control_revalidated ? "true" : "false",
            settle_evidence_complete ? "true" : "false",
            (unsigned long)settle_validation_generation,
            (unsigned long)roster_identity_tracker.sequence_state,
            (unsigned long)context->native_thread_id,
            (unsigned long)context->runtime_generation,
            context->lineage_matched ? "true" : "false");
    }
    SetLastError(result_error);
}

static void copy_zone_name(const char *source, char destination[16]) {
    unsigned int index;

    memset(destination, 0, 16u);
    if (source == NULL) return;
    for (index = 0u; index + 1u < 16u; ++index) {
        if (!readable_memory(source + index, 1u)) break;
        destination[index] = source[index];
        if (source[index] == '\0') break;
    }
    destination[15] = '\0';
}

static TalosOpcodeContext *current_set_zone_context(void) {
    TalosOpcodeContext *context;

    if (opcode_27_tls == TLS_OUT_OF_INDEXES) return NULL;
    context = (TalosOpcodeContext *)TlsGetValue(opcode_27_tls);
    if (context == NULL || !context->exact ||
        context->event != SUDEKIMP_TALOS_NATIVE_EVENT_SET_ZONE_NOW ||
        context->native_thread_id != GetCurrentThreadId()) return NULL;
    return context;
}

static void log_set_zone_edge(
    TalosOpcodeContext *context,
    const char *phase,
    const char *zone_name
) {
    char copied[ZONE_NAME_CAPACITY];

    copy_zone_name(zone_name, copied);
    lifecycle_snapshot.event_serial =
        next_nonzero(lifecycle_snapshot.event_serial);
    lifecycle_snapshot.set_zone_before_seen =
        context->set_zone_before_seen;
    SudekiMpLogFormat(
        "talos_lifecycle event=set_zone_now phase=%s run=%08lx%08lx "
        "serial=%lu zone=%s operand=0x%08lx hash=0x%08lx "
        "native_thread=%lu before_seen=%s "
        "policy=observation_only_original_called_once_no_mutation\r\n",
        phase,
        (unsigned long)lifecycle_snapshot.run_id_high,
        (unsigned long)lifecycle_snapshot.run_id_low,
        (unsigned long)lifecycle_snapshot.event_serial,
        copied[0] == '\0' ? "<empty-or-unreadable>" : copied,
        (unsigned long)context->operand_offset,
        (unsigned long)context->binding_hash,
        (unsigned long)context->native_thread_id,
        context->set_zone_before_seen ? "true" : "false");
}

void SudekiMpTalosNativeLifecycleObserveSetZoneNowBefore(
    const char *zone_name
) {
    TalosOpcodeContext *context;
    TalosTaskLineage lineage;
    DWORD error = GetLastError();

    memset(&lineage, 0, sizeof(lineage));
    (void)copy_task_lineage(&lineage);
    context = current_set_zone_context();
    if (context != NULL) {
        context->set_zone_before_seen = 1u;
        copy_zone_name(zone_name, context->zone_name);
        acquire_settle_evidence_writer();
        if (lifecycle_snapshot.installed != 0u && lineage.valid &&
            context->lineage_matched &&
            context->thread_token == lineage.load_void_thread_token &&
            context->runtime_generation == lineage.runtime_generation &&
            context->native_thread_id == lineage.native_thread_id &&
            context->matched_load_void_task_generation ==
                lineage.load_void_task_generation &&
            lifecycle_snapshot.load_void_task_generation != 0u &&
            lifecycle_snapshot.load_void_task_generation ==
                lineage.load_void_task_generation &&
            settle_evidence_tracker.session_generation != 0u &&
            settle_evidence_tracker.native_thread_id ==
                context->native_thread_id &&
            settle_evidence_tracker.script_runtime_generation ==
                context->runtime_generation) {
            context->set_zone_task_generation =
                lifecycle_snapshot.load_void_task_generation;
            context->set_zone_settle_session_generation =
                settle_evidence_tracker.session_generation;
        }
        release_settle_evidence_writer();
        log_set_zone_edge(context, "before", zone_name);
    }
    SetLastError(error);
}

void SudekiMpTalosNativeLifecycleObserveSetZoneNowAfter(
    const char *zone_name
) {
    TalosOpcodeContext *context;
    TalosTaskLineage lineage;
    DWORD error = GetLastError();
    BOOL committed = FALSE;
    uint32_t session_generation = 0u;
    uint32_t runtime_generation = 0u;
    uint32_t task_generation = 0u;

    memset(&lineage, 0, sizeof(lineage));
    (void)copy_task_lineage(&lineage);
    context = current_set_zone_context();
    if (context != NULL && context->set_zone_before_seen) {
        log_set_zone_edge(context, "after", zone_name);
        if (context->set_zone_task_generation != 0u &&
            context->set_zone_settle_session_generation != 0u &&
            exact_readable_text(context->zone_name, "Void") &&
            exact_readable_text(zone_name, "Void")) {
            acquire_settle_evidence_writer();
            if (lifecycle_snapshot.installed != 0u && lineage.valid &&
                context->lineage_matched &&
                context->thread_token ==
                    lineage.load_void_thread_token &&
                context->runtime_generation ==
                    lineage.runtime_generation &&
                context->native_thread_id ==
                    lineage.native_thread_id &&
                context->matched_load_void_task_generation ==
                    lineage.load_void_task_generation &&
                lifecycle_snapshot.load_void_task_generation ==
                    context->set_zone_task_generation &&
                lineage.load_void_task_generation ==
                    context->set_zone_task_generation &&
                settle_evidence_tracker.session_generation ==
                    context->set_zone_settle_session_generation &&
                settle_evidence_tracker.native_thread_id ==
                    context->native_thread_id &&
                settle_evidence_tracker.script_runtime_generation ==
                    context->runtime_generation) {
                settle_evidence_tracker.load_void_task_generation =
                    context->set_zone_task_generation;
                settle_evidence_tracker.void_set_zone_completed = 1u;
                settle_evidence_tracker.default_camera_committed = 0u;
                settle_evidence_tracker.default_camera_revalidated = 0u;
                settle_evidence_tracker.tal_control_revalidated = 0u;
                settle_evidence_tracker.settle_evidence_complete = 0u;
                publish_settle_evidence_locked();
                committed = TRUE;
                session_generation =
                    settle_evidence_tracker.session_generation;
                runtime_generation =
                    settle_evidence_tracker.script_runtime_generation;
                task_generation =
                    settle_evidence_tracker.load_void_task_generation;
            }
            release_settle_evidence_writer();
        }
        if (committed) {
            SudekiMpLogFormat(
                "talos_lifecycle event=VoidSetZone_settle phase=after "
                "run=%08lx%08lx session_generation=%lu "
                "runtime_generation=%lu task_generation=%lu "
                "native_thread=%lu exact_void=true "
                "policy=observation_only_no_pointer_retention_no_mutation\r\n",
                (unsigned long)lifecycle_snapshot.run_id_high,
                (unsigned long)lifecycle_snapshot.run_id_low,
                (unsigned long)session_generation,
                (unsigned long)runtime_generation,
                (unsigned long)task_generation,
                (unsigned long)context->native_thread_id);
        }
    }
    SetLastError(error);
}

void SudekiMpTalosNativeLifecycleObserveRenderCameraAfter(
    const void *manager,
    const char *camera_name,
    BOOL native_result
) {
    DWORD error = GetLastError();
    BOOL exact_default = exact_readable_text(camera_name, "default");
    BOOL runtime_matched;
    BOOL task_matched;
    BOOL thread_matched;
    BOOL camera_state_valid = FALSE;
    BOOL accepted = FALSE;
    uint32_t session_generation;
    uint32_t camera_observation_generation;
    uint32_t default_camera_generation;
    uint32_t runtime_generation;
    uint32_t task_generation;

    if (lifecycle_snapshot.installed == 0u || !native_result) {
        SetLastError(error);
        return;
    }
    acquire_settle_evidence_writer();
    if (lifecycle_snapshot.installed == 0u ||
        settle_evidence_tracker.session_generation == 0u ||
        !settle_evidence_tracker.void_set_zone_completed) {
        release_settle_evidence_writer();
        SetLastError(error);
        return;
    }
    runtime_matched = settle_evidence_tracker.
        script_runtime_generation != 0u &&
        settle_evidence_tracker.script_runtime_generation ==
            lifecycle_snapshot.script_runtime_generation;
    task_matched = settle_evidence_tracker.
        load_void_task_generation != 0u &&
        settle_evidence_tracker.load_void_task_generation ==
            lifecycle_snapshot.load_void_task_generation;
    thread_matched = settle_evidence_tracker.native_thread_id != 0u &&
        settle_evidence_tracker.native_thread_id == GetCurrentThreadId();
    settle_evidence_tracker.camera_observation_generation = next_nonzero(
        settle_evidence_tracker.camera_observation_generation);
    if (exact_default) {
        camera_state_valid = validate_default_camera_state(manager);
        accepted =
            SudekiMpTalosNativeLifecycleDefaultCameraEvidencePolicy(
                TRUE, runtime_matched, task_matched, thread_matched,
                native_result, exact_default, camera_state_valid);
        if (accepted) {
            settle_evidence_tracker.default_camera_generation = next_nonzero(
                settle_evidence_tracker.default_camera_generation);
            settle_evidence_tracker.default_camera_committed = 1u;
        } else {
            settle_evidence_tracker.default_camera_committed = 0u;
        }
    } else {
        /* A successful later authored/cinematic camera becomes the current
         * global owner, so prior default-camera evidence is no longer live. */
        settle_evidence_tracker.default_camera_committed = 0u;
    }
    settle_evidence_tracker.default_camera_revalidated = 0u;
    settle_evidence_tracker.tal_control_revalidated = 0u;
    settle_evidence_tracker.settle_evidence_complete = 0u;
    publish_settle_evidence_locked();
    session_generation = settle_evidence_tracker.session_generation;
    camera_observation_generation =
        settle_evidence_tracker.camera_observation_generation;
    default_camera_generation =
        settle_evidence_tracker.default_camera_generation;
    runtime_generation =
        settle_evidence_tracker.script_runtime_generation;
    task_generation = settle_evidence_tracker.load_void_task_generation;
    release_settle_evidence_writer();
    SudekiMpLogFormat(
        "talos_lifecycle event=SetRenderCamera_settle phase=after "
        "run=%08lx%08lx session_generation=%lu "
        "camera_observation_generation=%lu default_camera_generation=%lu "
        "native_result=true exact_default=%s camera_state_valid=%s "
        "accepted=%s invalidated_by_nondefault=%s "
        "runtime_generation=%lu task_generation=%lu native_thread=%lu "
        "policy=borrowed_pointer_copy_only_observation_no_mutation\r\n",
        (unsigned long)lifecycle_snapshot.run_id_high,
        (unsigned long)lifecycle_snapshot.run_id_low,
        (unsigned long)session_generation,
        (unsigned long)camera_observation_generation,
        (unsigned long)default_camera_generation,
        exact_default ? "true" : "false",
        camera_state_valid ? "true" : "false",
        accepted ? "true" : "false",
        !exact_default ? "true" : "false",
        (unsigned long)runtime_generation,
        (unsigned long)task_generation,
        (unsigned long)GetCurrentThreadId());
    SetLastError(error);
}

static BOOL absolute_global_tail_matches(
    uint8_t *base,
    uint32_t function_rva,
    uint32_t global_rva,
    const uint8_t *tail,
    size_t tail_size
) {
    uint32_t operand;

    if (base == NULL || tail == NULL || tail_size == 0u ||
        !readable_memory(base + function_rva, 5u + tail_size) ||
        base[function_rva] != 0xa1u ||
        memcmp(base + function_rva + 5u, tail, tail_size) != 0) return FALSE;
    memcpy(&operand, base + function_rva + 1u, sizeof(operand));
    return operand == (uint32_t)(uintptr_t)(base + global_rva);
}

static BOOL complete_object_locator_matches(
    uint8_t *base,
    uint32_t vtable_rva,
    uint32_t locator_rva,
    uint32_t expected_subobject_offset,
    uint32_t type_descriptor_rva
) {
    uint32_t locator_pointer;
    uint32_t signature;
    uint32_t subobject_offset;
    uint32_t type_descriptor_pointer;

    if (base == NULL || vtable_rva < sizeof(uint32_t) ||
        !readable_memory(base + vtable_rva - sizeof(uint32_t),
            sizeof(uint32_t)) ||
        !readable_memory(base + locator_rva, 5u * sizeof(uint32_t))) {
        return FALSE;
    }
    memcpy(&locator_pointer, base + vtable_rva - sizeof(uint32_t),
        sizeof(locator_pointer));
    memcpy(&signature, base + locator_rva, sizeof(signature));
    memcpy(&subobject_offset, base + locator_rva + sizeof(uint32_t),
        sizeof(subobject_offset));
    memcpy(&type_descriptor_pointer,
        base + locator_rva + 3u * sizeof(uint32_t),
        sizeof(type_descriptor_pointer));
    return locator_pointer == (uint32_t)(uintptr_t)(base + locator_rva) &&
        signature == 0u && subobject_offset == expected_subobject_offset &&
        type_descriptor_pointer ==
            (uint32_t)(uintptr_t)(base + type_descriptor_rva);
}

static BOOL actor_vtable_signature_matches(
    uint8_t *base,
    const HeroVtableEvidence *evidence
) {
    uint32_t method_pointer;
    uint32_t immediate_value;
    const char *type_name;
    size_t type_name_size;

    if (base == NULL || evidence == NULL) return FALSE;
    type_name = (const char *)(base + evidence->type_descriptor_rva +
        2u * sizeof(uint32_t));
    type_name_size = strlen(evidence->type_descriptor_name) + 1u;
    if (!complete_object_locator_matches(
            base, evidence->main_vtable_rva, evidence->main_col_rva,
            0u, evidence->type_descriptor_rva) ||
        !complete_object_locator_matches(
            base, evidence->secondary_vtable_rva,
            evidence->secondary_col_rva, 8u,
            evidence->type_descriptor_rva) ||
        !complete_object_locator_matches(
            base, evidence->resource_vtable_rva,
            evidence->resource_col_rva, ACTOR_RESOURCE_VTABLE_OFFSET,
            evidence->type_descriptor_rva) ||
        !readable_memory(type_name, type_name_size) ||
        memcmp(type_name, evidence->type_descriptor_name,
            type_name_size) != 0 ||
        !readable_memory(base + evidence->resource_vtable_rva + 0x10u,
            sizeof(method_pointer)) ||
        !readable_memory(base + evidence->type_method_rva, 6u)) {
        return FALSE;
    }
    memcpy(&method_pointer,
        base + evidence->resource_vtable_rva + 0x10u,
        sizeof(method_pointer));
    memcpy(&immediate_value, base + evidence->type_method_rva + 1u,
        sizeof(immediate_value));
    return method_pointer ==
            (uint32_t)(uintptr_t)(base + evidence->type_method_rva) &&
        base[evidence->type_method_rva] == 0xb8u &&
        immediate_value == evidence->type_value &&
        base[evidence->type_method_rva + 5u] == 0xc3u;
}

static BOOL hero_vtable_signatures_match(uint8_t *base) {
    unsigned int hero;

    for (hero = 0u; hero < NATIVE_MEMBER_LIMIT; ++hero) {
        if (!actor_vtable_signature_matches(
                base, &hero_vtable_evidence[hero])) return FALSE;
    }
    return actor_vtable_signature_matches(base, &dark_tal_vtable_evidence);
}

static BOOL tsa_set_playing_signature_matches(uint8_t *base) {
    uint8_t *function;
    uint32_t operand;
    int32_t displacement;

    if (base == NULL ||
        !readable_memory(base + RVA_TSA_SET_PLAYING, 0x60u)) return FALSE;
    function = base + RVA_TSA_SET_PLAYING;
    if (function[0x00u] != 0xa1u ||
        memcmp(function + 0x05u, tsa_set_playing_tail,
            sizeof(tsa_set_playing_tail)) != 0 ||
        function[0x1du] != 0xa1u ||
        memcmp(function + 0x22u, tsa_set_playing_after_shadow,
            sizeof(tsa_set_playing_after_shadow)) != 0 ||
        function[0x2fu] != 0x8bu || function[0x30u] != 0x0du ||
        function[0x35u] != 0x68u ||
        memcmp(function + 0x3au, tsa_set_playing_dispatch_body,
            sizeof(tsa_set_playing_dispatch_body)) != 0 ||
        function[0x4fu] != 0xe8u ||
        function[0x54u] != 0x89u || function[0x55u] != 0x35u ||
        memcmp(function + 0x5au, tsa_set_playing_suffix,
            sizeof(tsa_set_playing_suffix)) != 0) return FALSE;
    memcpy(&operand, function + 0x01u, sizeof(operand));
    if (operand != (uint32_t)(uintptr_t)(base + RVA_TSA_PLAYING_GLOBAL))
        return FALSE;
    memcpy(&operand, function + 0x1eu, sizeof(operand));
    if (operand != (uint32_t)(uintptr_t)(base + 0x003c2f3cu)) return FALSE;
    memcpy(&operand, function + 0x31u, sizeof(operand));
    if (operand != (uint32_t)(uintptr_t)(base + 0x00409d8cu)) return FALSE;
    memcpy(&operand, function + 0x36u, sizeof(operand));
    if (operand != (uint32_t)(uintptr_t)(base + 0x003c3a64u)) return FALSE;
    memcpy(&displacement, function + 0x50u, sizeof(displacement));
    if (function + 0x54u + displacement != base + 0x0003f3b0u)
        return FALSE;
    memcpy(&operand, function + 0x56u, sizeof(operand));
    return operand == (uint32_t)(uintptr_t)(base + 0x003c2f3cu);
}

static BOOL kazel_group_add_signatures_match(uint8_t *base) {
    uint8_t *window;
    uint32_t operand;
    uint32_t listener_add;
    int32_t displacement;

    if (base == NULL || RVA_KAZEL_GROUP_ADD_CALL < 10u ||
        !readable_memory(base + RVA_KAZEL_GROUP_ADD_CALL - 10u, 17u) ||
        !readable_memory(base + RVA_RAW_GROUP_ADD,
            sizeof(raw_group_add_entry)) ||
        !readable_memory(base + RVA_RAW_GROUP_ADD + 0x108u, 3u) ||
        !readable_memory(base + RVA_AI_LISTENER_VTABLE + 0x18u,
            sizeof(listener_add)) ||
        !readable_memory(base + RVA_AI_LISTENER_ADD,
            sizeof(ai_listener_add_prefix) + 5u) ||
        !readable_memory(base + RVA_AI_LISTENER_ADD + 0x23u, 3u) ||
        !readable_memory(base + RVA_RAW_FORMATION_ADD,
            sizeof(raw_formation_add_entry)) ||
        !readable_memory(base + RVA_RAW_FORMATION_ADD + 0x87u, 3u) ||
        !readable_memory(base + RVA_RAW_FORMATION_ADD + 0x8eu, 3u)) {
        return FALSE;
    }
    window = base + RVA_KAZEL_GROUP_ADD_CALL - 10u;
    if (memcmp(window, kazel_group_add_call_prefix,
            sizeof(kazel_group_add_call_prefix)) != 0 ||
        memcmp(window + 9u, kazel_group_add_call_suffix,
            sizeof(kazel_group_add_call_suffix)) != 0 ||
        memcmp(base + RVA_RAW_GROUP_ADD, raw_group_add_entry,
            sizeof(raw_group_add_entry)) != 0 ||
        memcmp(base + RVA_RAW_GROUP_ADD + 0x108u,
            "\xc2\x04\x00", 3u) != 0 ||
        memcmp(base + RVA_AI_LISTENER_ADD, ai_listener_add_prefix,
            sizeof(ai_listener_add_prefix)) != 0 ||
        base[RVA_AI_LISTENER_ADD + 0x14u] != 0xe8u ||
        memcmp(base + RVA_AI_LISTENER_ADD + 0x23u,
            "\xc2\x0c\x00", 3u) != 0 ||
        memcmp(base + RVA_RAW_FORMATION_ADD, raw_formation_add_entry,
            sizeof(raw_formation_add_entry)) != 0 ||
        memcmp(base + RVA_RAW_FORMATION_ADD + 0x87u,
            "\xc2\x04\x00", 3u) != 0 ||
        memcmp(base + RVA_RAW_FORMATION_ADD + 0x8eu,
            "\xc2\x04\x00", 3u) != 0) return FALSE;
    memcpy(&operand, window + 5u, sizeof(operand));
    if (operand != (uint32_t)(uintptr_t)(base + RVA_ACTIVE_GROUP_GLOBAL))
        return FALSE;
    memcpy(&displacement, window + 11u, sizeof(displacement));
    if (window + 15u + displacement != base + RVA_RAW_GROUP_ADD)
        return FALSE;
    memcpy(&listener_add, base + RVA_AI_LISTENER_VTABLE + 0x18u,
        sizeof(listener_add));
    if (listener_add != (uint32_t)(uintptr_t)(base + RVA_AI_LISTENER_ADD))
        return FALSE;
    memcpy(&displacement,
        base + RVA_AI_LISTENER_FORMATION_ADD_CALL + 1u,
        sizeof(displacement));
    return base + RVA_AI_LISTENER_FORMATION_ADD_CALL + 5u + displacement ==
        base + RVA_RAW_FORMATION_ADD;
}

static BOOL signatures_match(uint8_t *base) {
    void **call_slot;
    void **scene_slot;

    if (base == NULL ||
        !readable_memory(base + RVA_SCRIPT_CALL_OPCODE,
            sizeof(script_call_opcode_signature)) ||
        !readable_memory(base + RVA_SCRIPT_SCENE_OPCODE,
            sizeof(script_scene_opcode_signature)) ||
        !readable_memory(base + RVA_SCRIPT_CALL_OPCODE_SLOT,
            sizeof(*call_slot)) ||
        !readable_memory(base + RVA_SCRIPT_SCENE_OPCODE_SLOT,
            sizeof(*scene_slot)) ||
        !readable_memory(base + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL - 2u,
            sizeof(script_scene_task_constructor_window)) ||
        !readable_memory(base + RVA_DELETE_PC,
            sizeof(delete_pc_signature)) ||
        memcmp(base + RVA_SCRIPT_CALL_OPCODE, script_call_opcode_signature,
            sizeof(script_call_opcode_signature)) != 0 ||
        memcmp(base + RVA_SCRIPT_SCENE_OPCODE, script_scene_opcode_signature,
            sizeof(script_scene_opcode_signature)) != 0 ||
        memcmp(base + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL - 2u,
            script_scene_task_constructor_window,
            sizeof(script_scene_task_constructor_window)) != 0 ||
        memcmp(base + RVA_DELETE_PC, delete_pc_signature,
            sizeof(delete_pc_signature)) != 0 ||
        !absolute_global_tail_matches(
            base, RVA_REMOVE_ALL_PLAYERS, RVA_ACTIVE_GROUP_GLOBAL,
            remove_all_players_tail,
            sizeof(remove_all_players_tail)) ||
        !absolute_global_tail_matches(
            base, RVA_FORMATION_POP_MEMBERS, RVA_AI_MANAGER_GLOBAL,
            formation_pop_members_tail,
            sizeof(formation_pop_members_tail)) ||
        !absolute_global_tail_matches(
            base, RVA_TSA_IS_PLAYING, RVA_TSA_PLAYING_GLOBAL,
            tsa_is_playing_tail, sizeof(tsa_is_playing_tail)) ||
        !tsa_set_playing_signature_matches(base) ||
        !hero_vtable_signatures_match(base) ||
        !kazel_group_add_signatures_match(base)) return FALSE;
    return TRUE;
}

static BOOL opcode_slots_are_unowned(uint8_t *base) {
    void **call_slot = (void **)(base + RVA_SCRIPT_CALL_OPCODE_SLOT);
    void **scene_slot = (void **)(base + RVA_SCRIPT_SCENE_OPCODE_SLOT);

    return *call_slot == base + RVA_SCRIPT_CALL_OPCODE &&
        *scene_slot == base + RVA_SCRIPT_SCENE_OPCODE;
}

typedef struct PostMovieRestoreEvidenceBundle {
    SudekiMpTalosNativeLifecycleSnapshot lifecycle;
    SudekiMpTalosNativeRosterIdentitySnapshot roster;
    SudekiMpTalosNativeKazelSnapshot kazel;
    SudekiMpTalosNativeSettleEvidenceSnapshot settle;
    uint32_t settle_native_thread_id;
} PostMovieRestoreEvidenceBundle;

static BOOL copy_post_movie_restore_evidence(
    PostMovieRestoreEvidenceBundle *bundle
) {
    if (bundle == NULL || lifecycle_snapshot.installed == 0u) return FALSE;
    memset(bundle, 0, sizeof(*bundle));
    bundle->lifecycle = lifecycle_snapshot;
    if (!SudekiMpTalosNativeLifecycleGetRosterIdentitySnapshot(
            &bundle->roster) ||
        !SudekiMpTalosNativeLifecycleGetKazelSnapshot(&bundle->kazel)) {
        memset(bundle, 0, sizeof(*bundle));
        return FALSE;
    }
    acquire_settle_evidence_writer();
    bundle->settle = settle_evidence_snapshot;
    bundle->settle_native_thread_id =
        settle_evidence_tracker.native_thread_id;
    release_settle_evidence_writer();
    bundle->lifecycle = lifecycle_snapshot;
    return bundle->lifecycle.installed != 0u;
}

static void populate_post_movie_restore_ticket(
    const PostMovieRestoreEvidenceBundle *bundle,
    uint32_t authorization_generation,
    SudekiMpTalosNativePostMovieRestoreTicket *ticket
) {
    memset(ticket, 0, sizeof(*ticket));
    ticket->run_id_high = bundle->lifecycle.run_id_high;
    ticket->run_id_low = bundle->lifecycle.run_id_low;
    ticket->authorization_generation = authorization_generation;
    ticket->lifecycle_event_serial = bundle->lifecycle.event_serial;
    ticket->roster_observation_serial = bundle->roster.observation_serial;
    ticket->roster_revision = bundle->roster.roster_revision;
    ticket->kazel_observation_serial = bundle->kazel.observation_serial;
    ticket->kazel_request_generation = bundle->kazel.request_generation;
    ticket->session_generation = bundle->kazel.session_generation;
    ticket->script_runtime_generation =
        bundle->lifecycle.script_runtime_generation;
    ticket->load_void_task_generation =
        bundle->lifecycle.load_void_task_generation;
    ticket->settle_validation_generation =
        bundle->settle.settle_validation_generation;
    ticket->default_camera_generation =
        bundle->settle.default_camera_generation;
    ticket->native_thread_id = bundle->kazel.source_native_thread_id;
    ticket->tal_token = bundle->roster.hero_token[
        SUDEKIMP_TALOS_NATIVE_HERO_TAL];
    ticket->kazel_token = bundle->kazel.kazel_token;
}

static BOOL post_movie_restore_explicitly_quarantined(
    const PostMovieRestoreEvidenceBundle *bundle
) {
    return bundle == NULL ||
        bundle->roster.sequence_state ==
            SUDEKIMP_TALOS_NATIVE_ROSTER_SEQUENCE_QUARANTINED ||
        bundle->roster.quarantine_reason != 0u ||
        bundle->kazel.state == SUDEKIMP_TALOS_NATIVE_KAZEL_QUARANTINED ||
        bundle->kazel.ambiguity_reason != 0u;
}

static BOOL post_movie_restore_bundle_is_exact(
    const PostMovieRestoreEvidenceBundle *bundle,
    uint32_t current_native_thread_id
) {
    BOOL allow_ticket;
    BOOL exact_asset_authenticated;

    if (bundle == NULL) return FALSE;
    acquire_post_movie_restore_ticket_writer();
    allow_ticket = post_movie_restore_ticket_tracker.allow_ticket != 0u;
    exact_asset_authenticated = post_movie_restore_ticket_tracker.
        exact_asset_authenticated != 0u;
    release_post_movie_restore_ticket_writer();
    return SudekiMpTalosNativeLifecyclePostMovieRestoreTicketPolicy(
        allow_ticket, exact_asset_authenticated,
        &bundle->lifecycle, &bundle->roster, &bundle->kazel,
        &bundle->settle, current_native_thread_id,
        bundle->settle_native_thread_id);
}

static void try_arm_post_movie_restore_ticket(void) {
    DWORD incoming_error = GetLastError();
    PostMovieRestoreEvidenceBundle bundle;
    uint32_t authorization_generation = 0u;
    uint32_t current_native_thread_id = GetCurrentThreadId();
    BOOL armed = FALSE;
    BOOL quarantined = FALSE;
    BOOL copied;
    BOOL exact;
    BOOL explicit_quarantine;

    acquire_post_movie_restore_ticket_writer();
    if (post_movie_restore_ticket_tracker.state !=
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_ACTIVE) {
        release_post_movie_restore_ticket_writer();
        SetLastError(incoming_error);
        return;
    }
    release_post_movie_restore_ticket_writer();
    copied = copy_post_movie_restore_evidence(&bundle);
    exact = copied && post_movie_restore_bundle_is_exact(
        &bundle, current_native_thread_id);
    explicit_quarantine = copied &&
        post_movie_restore_explicitly_quarantined(&bundle);

    acquire_post_movie_restore_ticket_writer();
    if (post_movie_restore_ticket_tracker.state ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_ACTIVE &&
        explicit_quarantine) {
        post_movie_restore_ticket_tracker.state =
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_QUARANTINED;
        quarantined = TRUE;
    } else if (post_movie_restore_ticket_tracker.state ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_ACTIVE && exact) {
        authorization_generation = next_nonzero(
            post_movie_restore_ticket_tracker.authorization_generation);
        post_movie_restore_ticket_tracker.authorization_generation =
            authorization_generation;
        populate_post_movie_restore_ticket(
            &bundle, authorization_generation,
            &post_movie_restore_ticket_tracker.ready_ticket);
        post_movie_restore_ticket_tracker.state =
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_READY;
        armed = TRUE;
    }
    release_post_movie_restore_ticket_writer();
    if (armed) {
        SudekiMpLogFormat(
            "talos_post_movie_restore_ticket state=ready "
            "authorization_generation=%lu session_generation=%lu "
            "runtime_generation=%lu task_generation=%lu "
            "native_thread=%lu policy=pointer_free_one_shot\r\n",
            (unsigned long)authorization_generation,
            (unsigned long)bundle.kazel.session_generation,
            (unsigned long)bundle.lifecycle.script_runtime_generation,
            (unsigned long)bundle.lifecycle.load_void_task_generation,
            (unsigned long)current_native_thread_id);
    } else if (quarantined) {
        SudekiMpLogWrite(
            "talos_post_movie_restore_ticket state=quarantined "
            "reason=lifecycle_tracker_quarantined\r\n");
    }
    SetLastError(incoming_error);
}

static void clear_pointer_hook_record(SudekiMpPointerHook *hook) {
    if (hook != NULL) memset(hook, 0, sizeof(*hook));
}

static BOOL restore_pointer_hook_if_owned(
    SudekiMpPointerHook *hook,
    const void *replacement
) {
    void *current;

    if (hook == NULL || !hook->installed || hook->slot == NULL) return TRUE;
    if (!readable_memory(hook->slot, sizeof(current))) return FALSE;
    current = *hook->slot;
    if (current == hook->original_value) {
        clear_pointer_hook_record(hook);
        return TRUE;
    }
    if (current != replacement) {
        SudekiMpLogWrite(
            "talos_lifecycle_trace_uninstall=held reason=opcode_slot_ownership_changed\r\n");
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    return SudekiMpRestorePointerHook(hook);
}

static uint8_t *relative_call_target(uint8_t *instruction) {
    int32_t displacement;

    if (!readable_memory(instruction, 5u) || instruction[0] != 0xe8u)
        return NULL;
    memcpy(&displacement, instruction + 1u, sizeof(displacement));
    return instruction + 5u + displacement;
}

static BOOL restore_relative_hook_if_owned(
    SudekiMpRelativeCallHook *hook,
    const void *replacement
) {
    uint8_t *native_target;
    uint8_t *current_target;

    if (hook == NULL || !hook->installed || hook->instruction == NULL)
        return TRUE;
    native_target = hook->instruction + 5u + hook->original_displacement;
    current_target = relative_call_target(hook->instruction);
    if (current_target == native_target) {
        memset(hook, 0, sizeof(*hook));
        return TRUE;
    }
    if (current_target != replacement) {
        SudekiMpLogWrite(
            "talos_lifecycle_trace_uninstall=held reason=task_call_ownership_changed\r\n");
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    return SudekiMpRestoreRelativeCallHook(hook);
}

static BOOL inline_hook_targets(
    const SudekiMpInlineHook *hook,
    const void *replacement
) {
    int32_t displacement;
    size_t index;

    if (hook == NULL || hook->target == NULL ||
        !readable_memory(hook->target, 5u) || hook->target[0] != 0xe9u)
        return FALSE;
    memcpy(&displacement, hook->target + 1u, sizeof(displacement));
    if (hook->target + 5u + displacement != (const uint8_t *)replacement)
        return FALSE;
    for (index = 5u; index < hook->length; ++index) {
        if (!readable_memory(hook->target + index, 1u) ||
            hook->target[index] != 0x90u) return FALSE;
    }
    return TRUE;
}

static BOOL restore_inline_hook_if_owned(
    SudekiMpInlineHook *hook,
    const void *replacement
) {
    if (hook == NULL || !hook->installed || hook->target == NULL) return TRUE;
    if (readable_memory(hook->target, hook->length) &&
        memcmp(hook->target, hook->original, hook->length) == 0) {
        if (hook->trampoline != NULL)
            VirtualFree(hook->trampoline, 0u, MEM_RELEASE);
        memset(hook, 0, sizeof(*hook));
        return TRUE;
    }
    if (!inline_hook_targets(hook, replacement)) {
        SudekiMpLogWrite(
            "talos_lifecycle_trace_uninstall=held reason=native_inline_ownership_changed\r\n");
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    return SudekiMpRestoreInlineHook(hook);
}

BOOL SudekiMpInstallTalosNativeLifecycleTrace(
    HMODULE game_module,
    BOOL enabled
) {
    return SudekiMpInstallTalosNativeLifecycleTraceForPostMovieRestore(
        game_module, enabled, FALSE, FALSE);
}

BOOL SudekiMpInstallTalosNativeLifecycleTraceForPostMovieRestore(
    HMODULE game_module,
    BOOL enabled,
    BOOL allow_post_movie_restore_ticket,
    BOOL exact_asset_authenticated
) {
    LARGE_INTEGER counter;
    FILETIME system_time;
    ULARGE_INTEGER system_time_value;
    uint64_t private_entropy;
    uint8_t *base = (uint8_t *)game_module;
    void **call_slot;
    void **scene_slot;
    uint8_t formation_entry[5];
    uint8_t remove_all_players_entry[5];
    uint8_t tsa_set_playing_entry[5];

    if (!enabled) {
        SudekiMpUninstallTalosNativeLifecycleTrace();
        return TRUE;
    }
    if (base == NULL || !signatures_match(base)) {
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return FALSE;
    }
    if (game_base != NULL || !opcode_slots_are_unowned(base)) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    reset_post_movie_restore_ticket_for_fresh_install();
    memcpy(formation_entry, base + RVA_FORMATION_POP_MEMBERS,
        sizeof(formation_entry));
    memcpy(remove_all_players_entry, base + RVA_REMOVE_ALL_PLAYERS,
        sizeof(remove_all_players_entry));
    memcpy(tsa_set_playing_entry, base + RVA_TSA_SET_PLAYING,
        sizeof(tsa_set_playing_entry));
    if (pinned_module == NULL && !GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCSTR)(uintptr_t)&SudekiMpInstallTalosNativeLifecycleTrace,
            &pinned_module)) {
        return FALSE;
    }
    opcode_27_tls = TlsAlloc();
    opcode_29_tls = TlsAlloc();
    if (opcode_27_tls == TLS_OUT_OF_INDEXES ||
        opcode_29_tls == TLS_OUT_OF_INDEXES) {
        SudekiMpUninstallTalosNativeLifecycleTrace();
        return FALSE;
    }
    memset(&lifecycle_snapshot, 0, sizeof(lifecycle_snapshot));
    memset(&roster_identity_tracker, 0, sizeof(roster_identity_tracker));
    memset(&roster_identity_snapshot, 0, sizeof(roster_identity_snapshot));
    memset(&settle_evidence_tracker, 0, sizeof(settle_evidence_tracker));
    memset(&settle_evidence_snapshot, 0, sizeof(settle_evidence_snapshot));
    (void)InterlockedExchange(&roster_identity_writer_lock, 0);
    (void)InterlockedExchange(&roster_identity_snapshot_sequence, 0);
    (void)InterlockedExchange(&settle_evidence_writer_lock, 0);
    (void)InterlockedExchange(&settle_evidence_snapshot_sequence, 0);
    (void)InterlockedExchange(&kazel_lifecycle_writer_lock, 0);
    (void)InterlockedExchange(&kazel_lifecycle_snapshot_sequence, 0);
    (void)InterlockedExchange(&task_lineage_writer_lock, 0);
    (void)InterlockedExchange(&task_lineage_sequence, 0);
    QueryPerformanceCounter(&counter);
    GetSystemTimeAsFileTime(&system_time);
    system_time_value.LowPart = system_time.dwLowDateTime;
    system_time_value.HighPart = system_time.dwHighDateTime;
    private_entropy = (uint64_t)counter.QuadPart ^
        system_time_value.QuadPart ^
        ((uint64_t)(uintptr_t)&counter << 17u) ^
        ((uint64_t)(uintptr_t)base << 29u) ^
        ((uint64_t)GetCurrentProcessId() << 32u) ^
        (uint64_t)GetCurrentThreadId();
    identity_token_key = mix_identity_value(private_entropy);
    if (identity_token_key == 0u)
        identity_token_key = UINT64_C(0x6a09e667f3bcc909);
    configure_post_movie_restore_ticket(
        allow_post_movie_restore_ticket, exact_asset_authenticated);
    lifecycle_snapshot.run_id_high =
        (uint32_t)counter.HighPart ^ GetCurrentProcessId();
    lifecycle_snapshot.run_id_low =
        (uint32_t)counter.LowPart ^ GetTickCount();
    if ((lifecycle_snapshot.run_id_high | lifecycle_snapshot.run_id_low) == 0u)
        lifecycle_snapshot.run_id_low = 1u;
    lifecycle_snapshot.native_passthrough_required = 1u;
    lifecycle_snapshot.mutation_supported = 0u;
    game_base = base;
    call_slot = (void **)(base + RVA_SCRIPT_CALL_OPCODE_SLOT);
    scene_slot = (void **)(base + RVA_SCRIPT_SCENE_OPCODE_SLOT);
    original_script_call_opcode = (ScriptOpcodeFunction)*call_slot;
    original_script_scene_opcode = (ScriptOpcodeFunction)*scene_slot;
    original_script_task_constructor = (ScriptTaskConstructorFunction)(
        base + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR);
    original_raw_group_add = (RawGroupAddFunction)(
        base + RVA_RAW_GROUP_ADD);
    tsa_is_playing = (TsaIsPlayingFunction)(base + RVA_TSA_IS_PLAYING);
    if (!SudekiMpInstallRelativeCallHook(
            &kazel_group_add_call_hook,
            base + RVA_KAZEL_GROUP_ADD_CALL,
            base + RVA_RAW_GROUP_ADD,
            observe_kazel_raw_group_add)) {
        DWORD install_error = GetLastError();

        SudekiMpUninstallTalosNativeLifecycleTrace();
        SetLastError(install_error);
        return FALSE;
    }
    if (!SudekiMpInstallPointerHook(&script_scene_opcode_hook, scene_slot,
            original_script_scene_opcode, observe_script_scene_opcode) ||
        !SudekiMpInstallPointerHook(&script_call_opcode_hook, call_slot,
            original_script_call_opcode, observe_script_call_opcode) ||
        !SudekiMpInstallRelativeCallHook(
            &script_scene_task_constructor_hook,
            base + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR_CALL,
            base + RVA_SCRIPT_SCENE_TASK_CONSTRUCTOR,
            observe_script_task_constructor)) {
        DWORD install_error = GetLastError();

        SudekiMpUninstallTalosNativeLifecycleTrace();
        SetLastError(install_error);
        return FALSE;
    }
    if (!SudekiMpInstallInlineHook(
            &delete_pc_hook,
            base + RVA_DELETE_PC,
            delete_pc_signature,
            sizeof(delete_pc_signature),
            observe_delete_pc)) {
        DWORD install_error = GetLastError();

        SudekiMpUninstallTalosNativeLifecycleTrace();
        SetLastError(install_error);
        return FALSE;
    }
    original_delete_pc = (DeletePcFunction)delete_pc_hook.trampoline;
    if (!SudekiMpInstallInlineHook(
            &remove_all_players_hook,
            base + RVA_REMOVE_ALL_PLAYERS,
            remove_all_players_entry,
            sizeof(remove_all_players_entry),
            observe_remove_all_players)) {
        DWORD install_error = GetLastError();

        SudekiMpUninstallTalosNativeLifecycleTrace();
        SetLastError(install_error);
        return FALSE;
    }
    original_remove_all_players =
        (RemoveAllPlayersFunction)remove_all_players_hook.trampoline;
    if (!SudekiMpInstallInlineHook(
            &formation_pop_members_hook,
            base + RVA_FORMATION_POP_MEMBERS,
            formation_entry,
            sizeof(formation_entry),
            observe_formation_pop_members)) {
        DWORD install_error = GetLastError();

        SudekiMpUninstallTalosNativeLifecycleTrace();
        SetLastError(install_error);
        return FALSE;
    }
    original_formation_pop_members =
        (FormationPopMembersFunction)formation_pop_members_hook.trampoline;
    if (!SudekiMpInstallInlineHook(
            &tsa_set_playing_hook,
            base + RVA_TSA_SET_PLAYING,
            tsa_set_playing_entry,
            sizeof(tsa_set_playing_entry),
            observe_tsa_set_playing)) {
        DWORD install_error = GetLastError();

        SudekiMpUninstallTalosNativeLifecycleTrace();
        SetLastError(install_error);
        return FALSE;
    }
    original_tsa_set_playing =
        (TsaSetPlayingFunction)tsa_set_playing_hook.trampoline;
    lifecycle_snapshot.installed = 1u;
    SudekiMpLogFormat(
        "talos_lifecycle_trace_install=success run=%08lx%08lx "
        "opcode29_slot_rva=0x00323fa8 opcode27_slot_rva=0x00323fa0 "
        "coverage=opcode_edges_task_constructor_nested_SetZoneNOW_"
        "DeletePC_native_RemoveAllPlayers_native_FormationPopMembers_"
        "TSA_query_TSASetPlaying_false_edge_group_formation_counts_"
        "hero_vtable_identity_resource_membership_delta_"
        "Void_default_camera_Tal_control_settle_"
        "Kazel_SOL_chain_DarkTal_raw_group_add_group_formation_delta "
        "missing=actor_lifetime_authority "
        "post_movie_restore_ticket=%s exact_asset_authenticated=%s "
        "policy=original_called_once_lifecycle_observation_"
        "pointer_free_ticket_only_no_native_mutation\r\n",
        (unsigned long)lifecycle_snapshot.run_id_high,
        (unsigned long)lifecycle_snapshot.run_id_low,
        allow_post_movie_restore_ticket && exact_asset_authenticated ?
            "enabled" : "disabled",
        exact_asset_authenticated ? "true" : "false");
    return TRUE;
}

void SudekiMpUninstallTalosNativeLifecycleTrace(void) {
    DWORD error = GetLastError();
    BOOL restored = TRUE;

    quarantine_post_movie_restore_ticket_for_teardown();
    if (restore_inline_hook_if_owned(
            &tsa_set_playing_hook, observe_tsa_set_playing)) {
        original_tsa_set_playing = NULL;
    } else {
        restored = FALSE;
    }
    if (restore_inline_hook_if_owned(
            &formation_pop_members_hook, observe_formation_pop_members)) {
        original_formation_pop_members = NULL;
    } else {
        restored = FALSE;
    }
    if (restore_inline_hook_if_owned(
            &remove_all_players_hook, observe_remove_all_players)) {
        original_remove_all_players = NULL;
    } else {
        restored = FALSE;
    }
    if (restore_inline_hook_if_owned(&delete_pc_hook, observe_delete_pc)) {
        original_delete_pc = NULL;
    } else {
        restored = FALSE;
    }
    if (!restore_relative_hook_if_owned(
            &script_scene_task_constructor_hook,
            observe_script_task_constructor)) restored = FALSE;
    if (!restore_pointer_hook_if_owned(
            &script_call_opcode_hook,
            observe_script_call_opcode)) restored = FALSE;
    if (!restore_pointer_hook_if_owned(
            &script_scene_opcode_hook,
            observe_script_scene_opcode)) restored = FALSE;
    if (!restore_relative_hook_if_owned(
            &kazel_group_add_call_hook,
            observe_kazel_raw_group_add)) restored = FALSE;
    if (!restored) {
        lifecycle_snapshot.installed = 1u;
        SudekiMpLogWrite(
            "talos_lifecycle_trace_uninstall=quarantined reason=hook_ownership_not_exclusive\r\n");
        SetLastError(error);
        return;
    }
    original_script_call_opcode = NULL;
    original_script_scene_opcode = NULL;
    original_script_task_constructor = NULL;
    original_raw_group_add = NULL;
    tsa_is_playing = NULL;
    game_base = NULL;
    begin_task_lineage_write();
    memset(&task_lineage, 0, sizeof(task_lineage));
    end_task_lineage_write();
    clear_roster_identity_state();
    clear_settle_evidence_state();
    clear_kazel_lifecycle_state();
    clear_post_movie_restore_ticket_after_teardown();
    identity_token_key = 0u;
    observed_runtime_token = 0u;
    observed_bytecode_token = 0u;
    if (opcode_27_tls != TLS_OUT_OF_INDEXES) {
        TlsFree(opcode_27_tls);
        opcode_27_tls = TLS_OUT_OF_INDEXES;
    }
    if (opcode_29_tls != TLS_OUT_OF_INDEXES) {
        TlsFree(opcode_29_tls);
        opcode_29_tls = TLS_OUT_OF_INDEXES;
    }
    lifecycle_snapshot.installed = 0u;
    lifecycle_snapshot.opcode_27_depth = 0u;
    lifecycle_snapshot.opcode_29_depth = 0u;
    lifecycle_snapshot.set_zone_before_seen = 0u;
    SetLastError(error);
}

BOOL SudekiMpTalosNativeLifecycleGetSnapshot(
    SudekiMpTalosNativeLifecycleSnapshot *snapshot
) {
    if (snapshot == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    *snapshot = lifecycle_snapshot;
    return lifecycle_snapshot.installed != 0u;
}

BOOL SudekiMpTalosNativeLifecycleGetRosterIdentitySnapshot(
    SudekiMpTalosNativeRosterIdentitySnapshot *snapshot
) {
    unsigned int attempt;

    if (snapshot == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (lifecycle_snapshot.installed == 0u) {
        memset(snapshot, 0, sizeof(*snapshot));
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }
    for (attempt = 0u; attempt < 64u; ++attempt) {
        LONG before = InterlockedCompareExchange(
            &roster_identity_snapshot_sequence, 0, 0);
        LONG after;

        if ((before & 1) != 0) continue;
        MemoryBarrier();
        *snapshot = roster_identity_snapshot;
        MemoryBarrier();
        after = InterlockedCompareExchange(
            &roster_identity_snapshot_sequence, 0, 0);
        if (before == after && (after & 1) == 0) return TRUE;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    SetLastError(ERROR_BUSY);
    return FALSE;
}

BOOL SudekiMpTalosNativeLifecycleGetSettleEvidenceSnapshot(
    SudekiMpTalosNativeSettleEvidenceSnapshot *snapshot
) {
    unsigned int attempt;

    if (snapshot == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (lifecycle_snapshot.installed == 0u) {
        memset(snapshot, 0, sizeof(*snapshot));
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }
    for (attempt = 0u; attempt < 64u; ++attempt) {
        LONG before = InterlockedCompareExchange(
            &settle_evidence_snapshot_sequence, 0, 0);
        LONG after;

        if ((before & 1) != 0) continue;
        MemoryBarrier();
        *snapshot = settle_evidence_snapshot;
        MemoryBarrier();
        after = InterlockedCompareExchange(
            &settle_evidence_snapshot_sequence, 0, 0);
        if (before == after && (after & 1) == 0) return TRUE;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    SetLastError(ERROR_BUSY);
    return FALSE;
}

BOOL SudekiMpTalosNativeLifecycleGetKazelSnapshot(
    SudekiMpTalosNativeKazelSnapshot *snapshot
) {
    unsigned int attempt;

    if (snapshot == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (lifecycle_snapshot.installed == 0u) {
        memset(snapshot, 0, sizeof(*snapshot));
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }
    for (attempt = 0u; attempt < 64u; ++attempt) {
        LONG before = InterlockedCompareExchange(
            &kazel_lifecycle_snapshot_sequence, 0, 0);
        LONG after;

        if ((before & 1) != 0) continue;
        MemoryBarrier();
        *snapshot = kazel_lifecycle_snapshot;
        MemoryBarrier();
        after = InterlockedCompareExchange(
            &kazel_lifecycle_snapshot_sequence, 0, 0);
        if (before == after && (after & 1) == 0) return TRUE;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    SetLastError(ERROR_BUSY);
    return FALSE;
}

BOOL SudekiMpTalosNativeLifecycleClaimPostMovieRestoreTicket(
    SudekiMpTalosNativePostMovieRestoreTicket *ticket
) {
    DWORD incoming_error;
    PostMovieRestoreEvidenceBundle bundle;
    SudekiMpTalosNativePostMovieTicketClaimResult result;
    uint32_t current_native_thread_id;
    uint8_t prior_state;
    uint8_t next_state;
    BOOL copied;
    BOOL exact;
    BOOL irreversible_mismatch;

    if (ticket == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    incoming_error = GetLastError();
    memset(ticket, 0, sizeof(*ticket));
    try_arm_post_movie_restore_ticket();
    current_native_thread_id = GetCurrentThreadId();
    copied = copy_post_movie_restore_evidence(&bundle);
    exact = copied && post_movie_restore_bundle_is_exact(
        &bundle, current_native_thread_id);
    irreversible_mismatch = copied &&
        post_movie_restore_explicitly_quarantined(&bundle);

    acquire_post_movie_restore_ticket_writer();
    prior_state = post_movie_restore_ticket_tracker.state;
    if (post_movie_restore_ticket_tracker.load_void_session_count > 1u)
        irreversible_mismatch = TRUE;
    if (prior_state == SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_READY) {
        exact = copied &&
            SudekiMpTalosNativeLifecyclePostMovieRestoreReadyClaimPolicy(
                &post_movie_restore_ticket_tracker.ready_ticket,
                &bundle.lifecycle, &bundle.roster, &bundle.kazel,
                &bundle.settle, current_native_thread_id,
                bundle.settle_native_thread_id);
    }
    result =
        SudekiMpTalosNativeLifecyclePostMovieTicketClaimTransitionPolicy(
            prior_state, exact, irreversible_mismatch, &next_state);
    post_movie_restore_ticket_tracker.state = next_state;
    if (result == SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_AUTHORIZED)
        *ticket = post_movie_restore_ticket_tracker.ready_ticket;
    release_post_movie_restore_ticket_writer();

    if (result == SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_AUTHORIZED) {
        SudekiMpLogFormat(
            "talos_post_movie_restore_ticket state=claimed "
            "authorization_generation=%lu session_generation=%lu "
            "runtime_generation=%lu task_generation=%lu "
            "native_thread=%lu policy=consumed_once_no_replay\r\n",
            (unsigned long)ticket->authorization_generation,
            (unsigned long)ticket->session_generation,
            (unsigned long)ticket->script_runtime_generation,
            (unsigned long)ticket->load_void_task_generation,
            (unsigned long)ticket->native_thread_id);
    } else if (prior_state != next_state && next_state ==
            SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_QUARANTINED) {
        SudekiMpLogWrite(
            "talos_post_movie_restore_ticket state=quarantined "
            "reason=ready_evidence_changed_or_irreversible_mismatch\r\n");
    }
    SetLastError(incoming_error);
    return result == SUDEKIMP_TALOS_NATIVE_POST_MOVIE_TICKET_AUTHORIZED;
}
