#ifndef SUDEKIMP_TALOS_COMPANION_MEMBERSHIP_ABI_H
#define SUDEKIMP_TALOS_COMPANION_MEMBERSHIP_ABI_H

#include <stddef.h>
#include <stdint.h>

/* This module describes and validates native seams only. It has no enable
 * switch, performs no native calls, installs no hooks, and grants no actor-
 * lifetime or mutation authority. */

#define SUDEKIMP_TALOS_MEMBERSHIP_SUPPORTED_SHA256 \
    "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94"

enum {
    SUDEKIMP_TALOS_MEMBERSHIP_ABI_VERSION = 3u,
    SUDEKIMP_TALOS_MEMBERSHIP_PREFERRED_BASE = 0x00400000u,
    SUDEKIMP_TALOS_MEMBERSHIP_IMAGE_SIZE = 0x0045f000u,
    SUDEKIMP_TALOS_MEMBERSHIP_NATIVE_CAPACITY = 4u,
    SUDEKIMP_TALOS_MEMBERSHIP_GET_PC_WRAPPER_SIZE = 0x18u,
    SUDEKIMP_TALOS_MEMBERSHIP_EMBEDDED_TPTR_SIZE = 0x0cu
};

/* GetPC returns a 0x18-byte GELPointer/PtrObj value. Its 0x0C-byte embedded
 * intrusive TPtr is only a weak observer and is not actor-lifetime ownership. */

typedef enum SudekiMpTalosMembershipSymbol {
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IMAGE = 0,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_GROUP_PLAYERS,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_PC,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_PLAYER,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IS_PLAYER,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_INDEX,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ALL_PENDING_LOADED,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IN_COMBAT,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ASYNC_ACTIVE,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_RESOLVER,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_CLEANUP,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_AI_LISTENER,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_ADD,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_REMOVE,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_CANONICALIZER,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_EPILOGUE,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_CHILD_DISPATCH,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_PLAYER_SET_ARMED,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_REFRESH,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_BAR_FILL,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_CAMERA_SYNC,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_RESOURCE_SELECTOR,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_STRING_ASSIGN,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_COUNT,
    SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_INVALID = 255
} SudekiMpTalosMembershipSymbol;

typedef enum SudekiMpTalosMembershipCallingConvention {
    SUDEKIMP_TALOS_MEMBERSHIP_CALL_NONE = 0,
    SUDEKIMP_TALOS_MEMBERSHIP_CALL_CDECL,
    SUDEKIMP_TALOS_MEMBERSHIP_CALL_THISCALL,
    SUDEKIMP_TALOS_MEMBERSHIP_CALL_INTERNAL_REGISTER,
    SUDEKIMP_TALOS_MEMBERSHIP_CALL_INTERNAL_STACK
} SudekiMpTalosMembershipCallingConvention;

typedef enum SudekiMpTalosMembershipRegisterCarrier {
    SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_NONE = 0,
    SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ECX,
    SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_EAX,
    SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ESI,
    SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_EBP,
    SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_EDI
} SudekiMpTalosMembershipRegisterCarrier;

typedef enum SudekiMpTalosMembershipReturnKind {
    SUDEKIMP_TALOS_MEMBERSHIP_RETURN_NONE = 0,
    SUDEKIMP_TALOS_MEMBERSHIP_RETURN_VOID,
    SUDEKIMP_TALOS_MEMBERSHIP_RETURN_POINTER32,
    SUDEKIMP_TALOS_MEMBERSHIP_RETURN_BOOL_AL,
    SUDEKIMP_TALOS_MEMBERSHIP_RETURN_BOOL_EAX,
    SUDEKIMP_TALOS_MEMBERSHIP_RETURN_INT32
} SudekiMpTalosMembershipReturnKind;

typedef enum SudekiMpTalosMembershipValidationFailure {
    SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_OK = 0,
    SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_NULL_IMAGE,
    SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_IMAGE_SIZE,
    SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_LOADED_BASE,
    SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_PE_HEADER,
    SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_STABLE_BYTES,
    SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_RELOCATED_OPERAND,
    SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_REL32_TARGET
} SudekiMpTalosMembershipValidationFailure;

typedef struct SudekiMpTalosMembershipSymbolDescriptor {
    uint32_t symbol;
    uint32_t rva;
    uint32_t validated_span;
    uint16_t stack_argument_bytes;
    uint16_t callee_pop_bytes;
    uint8_t calling_convention;
    uint8_t return_kind;
    uint8_t object_register;
    uint8_t validation_only;
} SudekiMpTalosMembershipSymbolDescriptor;

typedef struct SudekiMpTalosMembershipAbiDescriptor {
    uint32_t abi_version;
    uint32_t preferred_image_base;
    uint32_t mapped_image_size;
    uint32_t required_symbol_mask;
    uint32_t symbol_count;
    uint32_t native_capacity;
    uint32_t get_pc_wrapper_size;
    uint32_t get_pc_wrapper_helper_rva;
    uint32_t get_pc_wrapper_factory_rva;
    uint32_t get_pc_wrapper_core_destructor_rva;
    uint32_t get_pc_wrapper_scalar_destructor_rva;
    uint32_t embedded_tptr_size;
    uint32_t embedded_tptr_offset;
    uint32_t ptr_object_registration_constructor_rva;
    uint32_t ptr_registry_find_rva;
    uint32_t ptr_registry_erase_rva;
    uint32_t ptr_registry_delete_all_rva;
    uint32_t ptr_registry_root_rva;
    uint32_t ptr_registry_count_rva;
    uint32_t group_add_core_rva;
    uint32_t group_remove_core_rva;
    uint32_t group_remove_epilogue_rva;
    uint32_t ui_controller_global_rva;
    uint32_t ui_controller_vtable_rva;
    uint32_t ui_controller_dispatch_slot_offset;
    uint32_t ui_controller_dispatch_rva;
    uint32_t ui_controller_hud_child_offset;
    uint32_t hud_child_vtable_rva;
    uint32_t hud_child_dispatch_slot_offset;
    uint32_t hud_child_dispatch_rva;
    uint32_t character_arbiter_offset;
    uint32_t player_set_armed_rva;
    uint32_t character_stat_display_offset;
    uint32_t stat_display_refresh_rva;
    uint32_t stat_display_constructor_rva;
    uint32_t stat_display_primary_vtable_rva;
    uint32_t stat_display_secondary_vtable_rva;
    uint32_t stat_display_health_bar_offset;
    uint32_t stat_display_last_hp_offset;
    uint32_t stat_bar_renderer_offset;
    uint32_t stat_bar_handle_offset;
    uint32_t stat_bar_count_offset;
    uint32_t stat_bar_cache_offset;
    uint32_t stat_bar_fill_rva;
    uint32_t stat_bar_fill_early_return_rva;
    uint32_t stat_bar_fill_return_rva;
    /* A runtime sampler must separately prove every described object link is
     * non-null/readable and that both scene nodes' matrix/dirty fields are
     * writable. If camera-init is zero, the UI-scene link through +0x158 is
     * additionally required. This static ABI validator dereferences none. */
    uint32_t stat_display_camera_sync_rva;
    uint32_t stat_display_camera_init_rva;
    uint32_t stat_display_camera_ui_scene_global_rva;
    uint32_t stat_display_camera_saved_bounds_rva;
    uint32_t stat_display_camera_active_bounds_rva;
    uint32_t stat_display_camera_manager_global_rva;
    uint32_t stat_display_camera_ui_scene_last_float_offset;
    uint32_t stat_display_scene_node_offset;
    uint32_t stat_display_owner_offset;
    uint32_t stat_display_owner_node_offset;
    uint32_t scene_node_dirty_word_offset;
    uint32_t scene_node_matrix_offset;
    uint32_t scene_node_matrix_float_count;
    uint32_t camera_manager_active_offset;
    uint32_t camera_active_payload_offset;
    uint32_t camera_payload_position_offset;
    /* For the narrow no-allocation proof, require a selected resource ID in
     * [1, slot_count], a loaded table entry (or the initialized constant-text
     * branch), bounded source text no longer than proof_max_utf16_units, and
     * the destination control word's inline_mask bit already set. */
    uint32_t hud_resource_selector_rva;
    uint32_t hud_resource_selector_jump_table_rva;
    uint32_t hud_resource_initialized_rva;
    uint32_t hud_resource_table_global_rva;
    uint32_t hud_resource_inline_text_rva;
    uint32_t hud_resource_error_text_rva;
    uint32_t hud_resource_default_text_rva;
    uint32_t hud_resource_fallback_text_rva;
    uint32_t hud_resource_missing_report_rva;
    uint32_t hud_resource_actor_component_offset;
    uint32_t hud_resource_set_offset;
    uint32_t hud_resource_count_offset;
    uint32_t hud_resource_entries_offset;
    uint32_t hud_resource_id_offset;
    uint32_t hud_resource_table_count_offset;
    uint32_t hud_resource_table_data_offset;
    uint32_t hud_resource_table_first_slot_offset;
    uint32_t hud_resource_table_slot_stride;
    uint32_t hud_resource_table_slot_count;
    uint32_t hud_resource_first_table_id;
    uint32_t hud_resource_selected_id_min;
    uint32_t hud_resource_selected_id_max;
    uint32_t hud_string_assign_rva;
    uint32_t hud_string_free_rva;
    uint32_t hud_string_allocate_rva;
    uint32_t hud_string_copy_rva;
    uint32_t hud_portrait_gizmo_label_offset;
    uint32_t hud_string_control_offset;
    uint32_t hud_string_data_offset;
    uint32_t hud_string_inline_mask;
    uint32_t hud_string_inline_capacity_utf16;
    uint32_t hud_string_proof_max_utf16_units;
    uint32_t group_members_offset;
    uint32_t group_count_offset;
    uint32_t formation_members_offset;
    uint32_t formation_count_offset;
    uint32_t ai_manager_group_listener_offset;
    uint32_t group_listener_add_slot_offset;
    uint32_t group_listener_remove_slot_offset;
    uint32_t group_listener_to_formation_offset;
    uint32_t ai_manager_formation_offset;
    uint32_t supported_sha256_words[8];
    uint8_t pure_validation_only;
    uint8_t native_calls_permitted;
    uint8_t hooks_permitted;
    uint8_t enabled_by_default;
    uint8_t external_sha256_required;
    uint8_t reserved[3];
} SudekiMpTalosMembershipAbiDescriptor;

typedef struct SudekiMpTalosMembershipValidationResult {
    uint32_t abi_version;
    uint32_t required_symbol_mask;
    uint32_t validated_symbol_mask;
    uint32_t checks_completed;
    uint32_t failure;
    uint32_t failed_symbol;
    uint32_t failed_rva;
    uint32_t expected_value;
    uint32_t observed_value;
    uint8_t seams_valid;
    uint8_t pure_validation_only;
    uint8_t native_calls_permitted;
    uint8_t external_sha256_required;
} SudekiMpTalosMembershipValidationResult;

SudekiMpTalosMembershipAbiDescriptor
SudekiMpTalosCompanionMembershipAbiDescribe(void);

SudekiMpTalosMembershipSymbolDescriptor
SudekiMpTalosCompanionMembershipAbiDescribeSymbol(uint32_t index);

/* image is a loader-ready section-mapped PE32 image. loaded_image_base is the
 * module base represented by the post-load OptionalHeader.ImageBase and by its
 * relocated absolute operands; it need not equal image's host address in an
 * inert test mapping. A seams_valid result proves only the enumerated ABI
 * seams. The caller must separately verify the complete source file against
 * SUDEKIMP_TALOS_MEMBERSHIP_SUPPORTED_SHA256 before treating it as the
 * supported build. The returned evidence is pointer-free and never authorizes
 * a native call. */
SudekiMpTalosMembershipValidationResult
SudekiMpTalosCompanionMembershipAbiValidateMappedImage(
    const uint8_t *image,
    size_t image_size,
    uint32_t loaded_image_base
);

#if defined(SUDEKIMP_TALOS_MEMBERSHIP_ABI_TESTING)
/* Constructs only the inert, section-mapped bytes consumed by the pure gate.
 * This symbol does not exist in normal builds. */
int SudekiMpTalosCompanionMembershipAbiPopulateFixtureForTesting(
    uint8_t *image,
    size_t image_size,
    uint32_t loaded_image_base
);
#endif

#endif
