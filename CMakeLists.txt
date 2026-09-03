cmake_minimum_required(VERSION 3.20)

project(SudekiMP VERSION 0.1.0 LANGUAGES C RC)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "SudekiMP must be cross-compiled as a 32-bit Windows target")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(SUDEKIMP_COMMON_SOURCES
    src/engine/build_identity.c
    src/engine/sha256.c
)

add_library(SudekiMP SHARED
    src/loader/dllmain.c
    src/loader/fixed_three_profile.c
    src/loader/lan_arena_profile.c
    src/cleanroom/audio.c
    src/cleanroom/engine.c
    src/cleanroom/menu.c
    src/engine/arbiter_combat_input.c
    src/engine/controller_action_router.c
    src/engine/coop_roster_assignment.c
    src/engine/economy_coordinator.c
    src/engine/economy_sidecar.c
    src/engine/blacksmith_commit_adapter.c
    src/engine/blacksmith_read_model.c
    src/engine/blacksmith_shadow.c
    src/engine/blacksmith_ui_presenter.c
    src/engine/blacksmith_ui_session.c
    src/engine/camera_target_abi.c
    src/engine/render_phase_abi.c
    src/engine/log.c
    src/engine/local_viewport_layout.c
    src/engine/local_quick_menu.c
    src/engine/item_activation_abi.c
    src/engine/merchant_provenance.c
    src/engine/merchant_checkout.c
    src/engine/orbit_camera.c
    src/engine/personal_wallet.c
    src/engine/shared_inventory_entitlement_ledger.c
    src/engine/player_combat_context.c
    src/engine/player_statehood.c
    src/engine/roaming_boundary.c
    src/engine/save_book_vote.c
    src/engine/save_book_vote_input.c
    src/engine/skill_activation_abi.c
    src/engine/spirit_activation_abi.c
    src/engine/weapon_activation_abi.c
    src/engine/talos_companion_carry.c
    src/engine/talos_companion_staging.c
    src/engine/talos_companion_staging_research.c
    src/engine/talos_encounter_admission.c
    src/engine/talos_encounter_session.c
    src/engine/transition_vote.c
    src/input/bridge_protocol.c
    src/input/bridge_receiver.c
    src/input/local_input_hub.c
    src/input/key_binding.c
    src/network/lan_arena_protocol.c
    src/network/lan_arena_tal_combo_graph.c
    src/network/lan_arena_endpoint.c
    src/network/lan_arena_authority.c
    src/network/lan_arena_replica.c
    src/network/lan_arena_session.c
    src/hooks/accelerator_cache.c
    src/hooks/blacksmith_read_adapter.c
    src/hooks/blacksmith_ui_adapter.c
    src/hooks/call_hook.c
    src/hooks/interaction_provenance.c
    src/hooks/lan_arena_campaign_guard.c
    src/hooks/lan_arena_window_policy.c
    src/hooks/lan_arena_runtime.c
    src/hooks/lan_arena_client_input.c
    src/hooks/lan_arena_client_replica.c
    src/hooks/lan_arena_host_input.c
    src/hooks/lan_arena_collision_debug.c
    src/hooks/lan_arena_pause_panel.c
    src/hooks/merchant_provenance_adapter.c
    src/hooks/character_switch_trace.c
    src/hooks/control_separation.c
    src/hooks/freeroam_camera_input.c
    src/hooks/pattern_scan.c
    src/hooks/player_input_trace.c
    src/hooks/zone_transition_trace.c
    src/hooks/quick_menu.c
    src/hooks/quick_skill_input.c
    src/hooks/save_book_intercept.c
    src/hooks/shop_catalog_adapter.c
    src/hooks/spirit_strike_input.c
    src/hooks/split_screen_render.c
    src/hooks/talos_companion_membership_abi.c
    src/hooks/talos_companion_staging_native_sampler.c
    src/hooks/talos_companion_staging_native_capture.c
    src/hooks/talos_companion_staging_research_adapter.c
    src/hooks/talos_companion_carry_adapter.c
    src/hooks/talos_encounter_intercept.c
    src/hooks/talos_native_lifecycle_trace.c
    src/hooks/talos_post_movie_party_restore.c
    src/hooks/xinput_player_two.c
    src/hooks/skill_trace.c
    src/hooks/talos_coop_balance.c
    src/hooks/talos_defense_trace.c
    src/loader/sudekimp.def
    ${SUDEKIMP_COMMON_SOURCES}
)

target_include_directories(SudekiMP PRIVATE src)
target_compile_definitions(SudekiMP PRIVATE WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_compile_options(SudekiMP PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(SudekiMP PRIVATE -static-libgcc)
set_source_files_properties(src/hooks/skill_trace.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(src/hooks/blacksmith_read_adapter.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(src/hooks/blacksmith_ui_adapter.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(src/hooks/quick_skill_input.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(src/hooks/spirit_strike_input.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(src/hooks/character_switch_trace.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(src/hooks/talos_defense_trace.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(src/hooks/control_separation.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(src/hooks/freeroam_camera_input.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(src/hooks/player_input_trace.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(src/hooks/split_screen_render.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(
    src/cleanroom/engine.c
    src/cleanroom/menu.c
    PROPERTIES COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(src/engine/arbiter_combat_input.c PROPERTIES
    COMPILE_OPTIONS "-Wno-pedantic;-Wno-unused-parameter"
)
set_source_files_properties(src/engine/skill_activation_abi.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
target_link_libraries(SudekiMP PRIVATE advapi32 ws2_32 winmm)
set_target_properties(SudekiMP PROPERTIES
    PREFIX ""
    OUTPUT_NAME "SudekiMP"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.Launcher
    src/loader/launcher.c
    ${SUDEKIMP_COMMON_SOURCES}
)

add_executable(SudekiMP.LanArenaOperator
    src/tools/lan_arena_operator.c
)
target_include_directories(SudekiMP.LanArenaOperator PRIVATE src)
target_compile_definitions(SudekiMP.LanArenaOperator PRIVATE
    WIN32_LEAN_AND_MEAN UNICODE _UNICODE
)
target_compile_options(SudekiMP.LanArenaOperator PRIVATE
    -Wall -Wextra -Wpedantic
)
target_link_options(SudekiMP.LanArenaOperator PRIVATE -municode)
set_target_properties(SudekiMP.LanArenaOperator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(SudekiMP.Launcher PRIVATE src)
target_compile_definitions(SudekiMP.Launcher PRIVATE WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_compile_options(SudekiMP.Launcher PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(SudekiMP.Launcher PRIVATE -municode)
target_link_libraries(SudekiMP.Launcher PRIVATE advapi32)
set_target_properties(SudekiMP.Launcher PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.BetaLauncher WIN32
    src/launcher/beta_launcher.c
    src/launcher/beta_launcher.rc
)
target_include_directories(SudekiMP.BetaLauncher PRIVATE src/launcher)
target_compile_definitions(SudekiMP.BetaLauncher PRIVATE
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)
target_compile_options(SudekiMP.BetaLauncher PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(SudekiMP.BetaLauncher PRIVATE -municode -static-libgcc)
target_link_libraries(SudekiMP.BetaLauncher PRIVATE ole32 shell32 urlmon winmm)
set_target_properties(SudekiMP.BetaLauncher PROPERTIES
    OUTPUT_NAME "SudekiMP.LauncherGUI"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.CallHookTest
    tests/call_hook_test.c
    src/hooks/call_hook.c
)
target_include_directories(SudekiMP.CallHookTest PRIVATE src)
target_compile_definitions(SudekiMP.CallHookTest PRIVATE WIN32_LEAN_AND_MEAN)
target_compile_options(SudekiMP.CallHookTest PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(SudekiMP.CallHookTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.LanArenaRuntimeHookTest
    tests/lan_arena_runtime_hook_test.c
    src/hooks/call_hook.c
    src/network/lan_arena_tal_combo_graph.c
)
target_include_directories(SudekiMP.LanArenaRuntimeHookTest PRIVATE src)
target_compile_definitions(SudekiMP.LanArenaRuntimeHookTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.LanArenaRuntimeHookTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_source_files_properties(
    tests/lan_arena_runtime_hook_test.c
    PROPERTIES COMPILE_OPTIONS -Wno-pedantic
)
set_target_properties(SudekiMP.LanArenaRuntimeHookTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.AcceleratorCacheTest
    tests/accelerator_cache_test.c
    src/hooks/accelerator_cache.c
    src/hooks/call_hook.c
    src/engine/log.c
)
target_include_directories(SudekiMP.AcceleratorCacheTest PRIVATE src)
target_compile_definitions(SudekiMP.AcceleratorCacheTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.AcceleratorCacheTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_target_properties(SudekiMP.AcceleratorCacheTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.KeyBindingTest
    tests/key_binding_test.c
    src/input/key_binding.c
)

add_executable(SudekiMP.TransitionVoteTest
    tests/transition_vote_test.c
    src/engine/transition_vote.c
)

add_executable(SudekiMP.SaveBookVoteTest
    tests/save_book_vote_test.c
    src/engine/save_book_vote.c
)
target_include_directories(SudekiMP.SaveBookVoteTest PRIVATE src)
target_compile_options(SudekiMP.SaveBookVoteTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.SaveBookVoteTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.SaveBookVoteInputTest
    tests/save_book_vote_input_test.c
    src/engine/save_book_vote_input.c
)

add_executable(SudekiMP.LocalQuickMenuTest
    tests/local_quick_menu_test.c
    src/engine/local_quick_menu.c
)
target_include_directories(SudekiMP.LocalQuickMenuTest PRIVATE src)
target_compile_definitions(SudekiMP.LocalQuickMenuTest PRIVATE WIN32_LEAN_AND_MEAN)
target_compile_options(SudekiMP.LocalQuickMenuTest PRIVATE -Wall -Wextra -Wpedantic -Werror)
set_target_properties(SudekiMP.LocalQuickMenuTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.SaveBookVoteInputTest PRIVATE src)
target_compile_options(SudekiMP.SaveBookVoteInputTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.SaveBookVoteInputTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.RoamingBoundaryTest
    tests/roaming_boundary_test.c
    src/engine/roaming_boundary.c
)
target_include_directories(SudekiMP.RoamingBoundaryTest PRIVATE src)
target_compile_options(SudekiMP.RoamingBoundaryTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_target_properties(SudekiMP.RoamingBoundaryTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.TransitionVoteTest PRIVATE src)
target_compile_options(SudekiMP.TransitionVoteTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_target_properties(SudekiMP.TransitionVoteTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.ArbiterCombatInputTest
    tests/arbiter_combat_input_test.c
    src/engine/arbiter_combat_input.c
)
add_executable(SudekiMP.CameraTargetAbiTest
    tests/camera_target_abi_test.c
    src/engine/camera_target_abi.c
)
add_executable(SudekiMP.RenderPhaseAbiTest
    tests/render_phase_abi_test.c
    src/engine/render_phase_abi.c
)
target_include_directories(SudekiMP.RenderPhaseAbiTest PRIVATE src)
target_compile_definitions(SudekiMP.RenderPhaseAbiTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.RenderPhaseAbiTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_source_files_properties(
    tests/render_phase_abi_test.c
    src/engine/render_phase_abi.c
    PROPERTIES COMPILE_OPTIONS "-Wno-pedantic;-Wno-unused-function"
)
set_target_properties(SudekiMP.RenderPhaseAbiTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.CameraTargetAbiTest PRIVATE src)
target_compile_definitions(SudekiMP.CameraTargetAbiTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.CameraTargetAbiTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_source_files_properties(
    tests/camera_target_abi_test.c
    src/engine/camera_target_abi.c
    PROPERTIES COMPILE_OPTIONS "-Wno-pedantic;-Wno-unused-function"
)
set_target_properties(SudekiMP.CameraTargetAbiTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.ArbiterCombatInputTest PRIVATE src)
target_compile_definitions(SudekiMP.ArbiterCombatInputTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.ArbiterCombatInputTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_source_files_properties(
    tests/arbiter_combat_input_test.c
    PROPERTIES COMPILE_OPTIONS -Wno-pedantic
)
set_target_properties(SudekiMP.ArbiterCombatInputTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.KeyBindingTest PRIVATE src)
target_compile_definitions(SudekiMP.KeyBindingTest PRIVATE
    WIN32_LEAN_AND_MEAN UNICODE _UNICODE
)
target_compile_options(SudekiMP.KeyBindingTest PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(SudekiMP.KeyBindingTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.SkillTraceImageTest
    tests/skill_trace_image_test.c
    src/cleanroom/engine.c
    src/engine/arbiter_combat_input.c
    src/engine/controller_action_router.c
    src/engine/coop_roster_assignment.c
    src/engine/local_quick_menu.c
    src/engine/blacksmith_read_model.c
    src/engine/blacksmith_shadow.c
    src/engine/blacksmith_ui_session.c
    src/engine/camera_target_abi.c
    src/engine/render_phase_abi.c
    src/engine/log.c
    src/engine/local_viewport_layout.c
    src/engine/orbit_camera.c
    src/engine/player_combat_context.c
    src/engine/player_statehood.c
    src/engine/roaming_boundary.c
    src/engine/save_book_vote.c
    src/engine/skill_activation_abi.c
    src/engine/spirit_activation_abi.c
    src/engine/weapon_activation_abi.c
    src/engine/item_activation_abi.c
    src/engine/transition_vote.c
    src/input/bridge_protocol.c
    src/input/bridge_receiver.c
    src/input/local_input_hub.c
    src/network/lan_arena_protocol.c
    src/network/lan_arena_tal_combo_graph.c
    src/network/lan_arena_replica.c
    src/network/lan_arena_session.c
    src/network/lan_arena_authority.c
    src/hooks/call_hook.c
    src/hooks/blacksmith_read_adapter.c
    src/hooks/blacksmith_ui_adapter.c
    src/hooks/character_switch_trace.c
    src/hooks/control_separation.c
    src/hooks/freeroam_camera_input.c
    src/hooks/interaction_provenance.c
    src/hooks/lan_arena_campaign_guard.c
    src/hooks/lan_arena_window_policy.c
    src/hooks/lan_arena_client_input.c
    src/hooks/lan_arena_client_replica.c
    src/hooks/lan_arena_host_input.c
    src/hooks/lan_arena_pause_panel.c
    src/hooks/player_input_trace.c
    src/hooks/quick_skill_input.c
    src/hooks/save_book_intercept.c
    src/hooks/spirit_strike_input.c
    src/hooks/split_screen_render.c
    src/hooks/skill_trace.c
    src/hooks/talos_defense_trace.c
    src/hooks/talos_native_lifecycle_trace.c
    src/hooks/zone_transition_trace.c
)
target_include_directories(SudekiMP.SkillTraceImageTest PRIVATE src)
target_compile_definitions(SudekiMP.SkillTraceImageTest PRIVATE
    WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    SUDEKIMP_BLACKSMITH_UI_ADAPTER_TESTING=1
    SUDEKIMP_SAVE_BOOK_INTERCEPT_TESTING=1
)
target_compile_options(SudekiMP.SkillTraceImageTest PRIVATE
    -Wall -Wextra -Wpedantic
)
target_link_options(SudekiMP.SkillTraceImageTest PRIVATE -municode)
target_link_libraries(SudekiMP.SkillTraceImageTest PRIVATE ws2_32)
set_target_properties(SudekiMP.SkillTraceImageTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_source_files_properties(
    src/hooks/zone_transition_trace.c
    PROPERTIES COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(
    src/hooks/talos_native_lifecycle_trace.c
    PROPERTIES COMPILE_OPTIONS -Wno-pedantic
)

add_executable(SudekiMP.PlayerCombatContextTest
    tests/player_combat_context_test.c
    src/engine/player_combat_context.c
    src/engine/log.c
)
target_include_directories(SudekiMP.PlayerCombatContextTest PRIVATE src)
target_compile_definitions(SudekiMP.PlayerCombatContextTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.PlayerCombatContextTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_target_properties(SudekiMP.PlayerCombatContextTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.SkillActivationAbiTest
    tests/skill_activation_abi_test.c
    src/engine/skill_activation_abi.c
    src/engine/player_combat_context.c
    src/engine/log.c
)

add_executable(SudekiMP.SpiritActivationAbiTest
    tests/spirit_activation_abi_test.c
    src/engine/spirit_activation_abi.c
)
target_include_directories(SudekiMP.SpiritActivationAbiTest PRIVATE src)
target_compile_definitions(SudekiMP.SpiritActivationAbiTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.SpiritActivationAbiTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_source_files_properties(
    tests/spirit_activation_abi_test.c
    src/engine/spirit_activation_abi.c
    PROPERTIES COMPILE_OPTIONS -Wno-pedantic
)
set_target_properties(SudekiMP.SpiritActivationAbiTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.SkillActivationAbiTest PRIVATE src)
target_compile_definitions(SudekiMP.SkillActivationAbiTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.SkillActivationAbiTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_source_files_properties(
    tests/skill_activation_abi_test.c
    src/engine/skill_activation_abi.c
    PROPERTIES COMPILE_OPTIONS -Wno-pedantic
)
set_target_properties(SudekiMP.SkillActivationAbiTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.XInputProbe
    tests/xinput_probe.c
)
target_compile_definitions(SudekiMP.XInputProbe PRIVATE WIN32_LEAN_AND_MEAN)
target_compile_options(SudekiMP.XInputProbe PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(SudekiMP.XInputProbe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.DirectInputProbe
    tests/direct_input_probe.c
)
target_compile_definitions(SudekiMP.DirectInputProbe PRIVATE WIN32_LEAN_AND_MEAN)
target_compile_options(SudekiMP.DirectInputProbe PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(SudekiMP.DirectInputProbe PRIVATE dinput8 dxguid)
set_target_properties(SudekiMP.DirectInputProbe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.InputBridgeProtocolTest
    tests/input_bridge_protocol_test.c
    src/input/bridge_protocol.c
)
target_include_directories(SudekiMP.InputBridgeProtocolTest PRIVATE src)
target_compile_options(SudekiMP.InputBridgeProtocolTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_target_properties(SudekiMP.InputBridgeProtocolTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.LanArenaProtocolTest
    tests/lan_arena_protocol_test.c
    src/network/lan_arena_endpoint.c
    src/network/lan_arena_protocol.c
)
target_include_directories(SudekiMP.LanArenaProtocolTest PRIVATE src)
target_compile_options(SudekiMP.LanArenaProtocolTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.LanArenaProtocolTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.LanArenaCollisionDebugTest
    tests/lan_arena_collision_debug_test.c
    src/engine/log.c
    src/hooks/lan_arena_collision_debug.c
)
target_include_directories(SudekiMP.LanArenaCollisionDebugTest PRIVATE src)
target_compile_definitions(SudekiMP.LanArenaCollisionDebugTest PRIVATE
    WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_TESTING=1
)
target_compile_options(SudekiMP.LanArenaCollisionDebugTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
target_link_options(SudekiMP.LanArenaCollisionDebugTest PRIVATE -municode)
set_target_properties(SudekiMP.LanArenaCollisionDebugTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.LanArenaReplicaTest
    tests/lan_arena_replica_test.c
    src/network/lan_arena_protocol.c
    src/network/lan_arena_tal_combo_graph.c
    src/network/lan_arena_replica.c
)
target_include_directories(SudekiMP.LanArenaReplicaTest PRIVATE src)
target_compile_options(SudekiMP.LanArenaReplicaTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.LanArenaReplicaTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.LanArenaTalComboGraphTest
    tests/lan_arena_tal_combo_graph_test.c
    src/network/lan_arena_tal_combo_graph.c
)
target_include_directories(SudekiMP.LanArenaTalComboGraphTest PRIVATE src)
target_compile_definitions(SudekiMP.LanArenaTalComboGraphTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.LanArenaTalComboGraphTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.LanArenaTalComboGraphTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.LanArenaAuthorityTest
    tests/lan_arena_authority_test.c
    src/network/lan_arena_protocol.c
    src/network/lan_arena_authority.c
)
target_include_directories(SudekiMP.LanArenaAuthorityTest PRIVATE src)
target_compile_options(SudekiMP.LanArenaAuthorityTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.LanArenaAuthorityTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.LanArenaSessionTest
    tests/lan_arena_session_test.c
    src/engine/log.c
    src/network/lan_arena_protocol.c
    src/network/lan_arena_session.c
)
target_include_directories(SudekiMP.LanArenaSessionTest PRIVATE src)
target_compile_definitions(SudekiMP.LanArenaSessionTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.LanArenaSessionTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
target_link_libraries(SudekiMP.LanArenaSessionTest PRIVATE ws2_32)
set_target_properties(SudekiMP.LanArenaSessionTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.LanArenaLoopbackPeerTest
    tests/lan_arena_loopback_peer_test.c
    src/engine/log.c
    src/network/lan_arena_protocol.c
    src/network/lan_arena_session.c
)
target_include_directories(SudekiMP.LanArenaLoopbackPeerTest PRIVATE src)
target_compile_definitions(SudekiMP.LanArenaLoopbackPeerTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.LanArenaLoopbackPeerTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
target_link_libraries(SudekiMP.LanArenaLoopbackPeerTest PRIVATE ws2_32)
set_target_properties(SudekiMP.LanArenaLoopbackPeerTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.InputBridgeReceiverTest
    tests/input_bridge_receiver_test.c
    src/engine/log.c
    src/input/bridge_protocol.c
    src/input/bridge_receiver.c
    src/input/local_input_hub.c
)
target_include_directories(SudekiMP.InputBridgeReceiverTest PRIVATE src)
target_compile_definitions(SudekiMP.InputBridgeReceiverTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.InputBridgeReceiverTest PRIVATE
    -Wall -Wextra -Wpedantic
)
target_link_libraries(SudekiMP.InputBridgeReceiverTest PRIVATE ws2_32)
set_target_properties(SudekiMP.InputBridgeReceiverTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.LocalInputHubTest
    tests/local_input_hub_test.c
    src/engine/log.c
    src/input/bridge_protocol.c
    src/input/local_input_hub.c
)
target_include_directories(SudekiMP.LocalInputHubTest PRIVATE src)
target_compile_definitions(SudekiMP.LocalInputHubTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.LocalInputHubTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
target_link_libraries(SudekiMP.LocalInputHubTest PRIVATE ws2_32)
set_target_properties(SudekiMP.LocalInputHubTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.ControllerActionRouterTest
    tests/controller_action_router_test.c
    src/engine/controller_action_router.c
)

add_executable(SudekiMP.LocalViewportLayoutTest
    tests/local_viewport_layout_test.c
    src/engine/local_viewport_layout.c
)

add_executable(SudekiMP.CoopRosterAssignmentTest
    tests/coop_roster_assignment_test.c
    src/engine/coop_roster_assignment.c
)

add_executable(SudekiMP.FixedThreeProfileTest
    tests/fixed_three_profile_test.c
    src/loader/fixed_three_profile.c
)

add_executable(SudekiMP.LanArenaProfileTest
    tests/lan_arena_profile_test.c
    src/loader/lan_arena_profile.c
)
target_include_directories(SudekiMP.LanArenaProfileTest PRIVATE src)
target_compile_options(SudekiMP.LanArenaProfileTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.LanArenaProfileTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.TalosEncounterSessionTest
    tests/talos_encounter_session_test.c
    src/engine/talos_encounter_session.c
)

add_executable(SudekiMP.TalosEncounterAdmissionTest
    tests/talos_encounter_admission_test.c
    src/engine/talos_encounter_admission.c
    src/engine/talos_encounter_session.c
)

add_executable(SudekiMP.TalosCompanionCarryTest
    tests/talos_companion_carry_test.c
    src/engine/talos_companion_carry.c
)

add_executable(SudekiMP.TalosCompanionStagingTest
    tests/talos_companion_staging_test.c
    src/engine/talos_companion_staging.c
)

add_executable(SudekiMP.TalosCompanionStagingResearchTest
    tests/talos_companion_staging_research_test.c
    src/engine/talos_companion_staging_research.c
)

add_executable(SudekiMP.TalosCompanionMembershipAbiTest
    tests/talos_companion_membership_abi_test.c
    src/hooks/talos_companion_membership_abi.c
)

add_executable(SudekiMP.TalosCompanionStagingResearchAdapterTest
    tests/talos_companion_staging_research_adapter_test.c
    src/engine/log.c
    src/engine/talos_companion_staging_research.c
    src/hooks/talos_companion_membership_abi.c
    src/hooks/talos_companion_staging_research_adapter.c
)

add_executable(SudekiMP.TalosCompanionStagingNativeSamplerTest
    tests/talos_companion_staging_native_sampler_test.c
    src/hooks/talos_companion_membership_abi.c
    src/hooks/talos_companion_staging_native_sampler.c
)

add_executable(SudekiMP.TalosCompanionStagingNativeCaptureTest
    tests/talos_companion_staging_native_capture_test.c
    src/hooks/talos_companion_staging_native_capture.c
)

add_executable(SudekiMP.TalosCompanionCarryAdapterTest
    tests/talos_companion_carry_adapter_test.c
    src/hooks/talos_companion_carry_adapter.c
)

add_executable(SudekiMP.TalosEncounterInterceptTest
    tests/talos_encounter_intercept_test.c
    src/hooks/talos_encounter_intercept.c
)

add_executable(SudekiMP.TalosNativeLifecycleTraceTest
    tests/talos_native_lifecycle_trace_test.c
    src/engine/log.c
    src/hooks/call_hook.c
    src/hooks/talos_native_lifecycle_trace.c
)

add_executable(SudekiMP.TalosPostMoviePartyRestoreTest
    tests/talos_post_movie_party_restore_test.c
    src/hooks/talos_post_movie_party_restore.c
)
target_include_directories(SudekiMP.TalosPostMoviePartyRestoreTest PRIVATE src)
target_compile_definitions(SudekiMP.TalosPostMoviePartyRestoreTest PRIVATE
    WIN32_LEAN_AND_MEAN
    SUDEKIMP_TALOS_POST_MOVIE_PARTY_RESTORE_POLICY_ONLY=1
)
target_compile_options(SudekiMP.TalosPostMoviePartyRestoreTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.TalosPostMoviePartyRestoreTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.TalosNativeLifecycleTraceTest PRIVATE src)
target_compile_definitions(SudekiMP.TalosNativeLifecycleTraceTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.TalosNativeLifecycleTraceTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.TalosNativeLifecycleTraceTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.TalosEncounterInterceptTest PRIVATE src)
target_compile_definitions(SudekiMP.TalosEncounterInterceptTest PRIVATE
    SUDEKIMP_TALOS_ENCOUNTER_INTERCEPT_TESTING=1
)
target_compile_options(SudekiMP.TalosEncounterInterceptTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.TalosEncounterInterceptTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.TalosEncounterSessionTest PRIVATE src)
target_compile_options(SudekiMP.TalosEncounterSessionTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.TalosEncounterSessionTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.TalosEncounterAdmissionTest PRIVATE src)
target_compile_options(SudekiMP.TalosEncounterAdmissionTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.TalosEncounterAdmissionTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.TalosCompanionCarryTest PRIVATE src)
target_compile_options(SudekiMP.TalosCompanionCarryTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.TalosCompanionCarryTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.TalosCompanionStagingTest PRIVATE src)
target_compile_options(SudekiMP.TalosCompanionStagingTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.TalosCompanionStagingTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.TalosCompanionStagingResearchTest PRIVATE src)
target_compile_options(SudekiMP.TalosCompanionStagingResearchTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.TalosCompanionStagingResearchTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.TalosCompanionMembershipAbiTest PRIVATE src)
target_compile_definitions(SudekiMP.TalosCompanionMembershipAbiTest PRIVATE
    SUDEKIMP_TALOS_MEMBERSHIP_ABI_TESTING=1
)
target_compile_options(SudekiMP.TalosCompanionMembershipAbiTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.TalosCompanionMembershipAbiTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.TalosCompanionStagingResearchAdapterTest
    PRIVATE src
)
target_compile_definitions(SudekiMP.TalosCompanionStagingResearchAdapterTest
    PRIVATE
        SUDEKIMP_TALOS_MEMBERSHIP_ABI_TESTING=1
        SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING=1
)
target_compile_options(SudekiMP.TalosCompanionStagingResearchAdapterTest
    PRIVATE -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.TalosCompanionStagingResearchAdapterTest
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.TalosCompanionStagingNativeSamplerTest
    PRIVATE src
)
target_compile_definitions(SudekiMP.TalosCompanionStagingNativeSamplerTest
    PRIVATE WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.TalosCompanionStagingNativeSamplerTest
    PRIVATE -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.TalosCompanionStagingNativeSamplerTest
    PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.TalosCompanionStagingNativeCaptureTest
    PRIVATE src
)
target_compile_definitions(SudekiMP.TalosCompanionStagingNativeCaptureTest
    PRIVATE
        WIN32_LEAN_AND_MEAN
        SUDEKIMP_TALOS_STAGING_NATIVE_CAPTURE_TESTING=1
)
target_compile_options(SudekiMP.TalosCompanionStagingNativeCaptureTest
    PRIVATE -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.TalosCompanionStagingNativeCaptureTest
    PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.TalosCompanionCarryAdapterTest PRIVATE src)
target_compile_options(SudekiMP.TalosCompanionCarryAdapterTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.TalosCompanionCarryAdapterTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.LocalViewportLayoutTest PRIVATE src)
target_compile_options(SudekiMP.LocalViewportLayoutTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.LocalViewportLayoutTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.CoopRosterAssignmentTest PRIVATE src)
target_compile_options(SudekiMP.CoopRosterAssignmentTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.CoopRosterAssignmentTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.FixedThreeProfileTest PRIVATE src)
target_compile_definitions(SudekiMP.FixedThreeProfileTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.FixedThreeProfileTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.FixedThreeProfileTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.ControllerActionRouterTest PRIVATE src)
target_compile_options(SudekiMP.ControllerActionRouterTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.ControllerActionRouterTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.PlayerStatehoodTest
    tests/player_statehood_test.c
    src/engine/player_statehood.c
)
target_include_directories(SudekiMP.PlayerStatehoodTest PRIVATE src)
target_compile_options(SudekiMP.PlayerStatehoodTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_target_properties(SudekiMP.PlayerStatehoodTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.PersonalWalletTest
    tests/personal_wallet_test.c
    src/engine/personal_wallet.c
)
target_include_directories(SudekiMP.PersonalWalletTest PRIVATE src)
target_compile_options(SudekiMP.PersonalWalletTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.PersonalWalletTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.SharedInventoryEntitlementLedgerTest
    tests/shared_inventory_entitlement_ledger_test.c
    src/engine/personal_wallet.c
    src/engine/shared_inventory_entitlement_ledger.c
)
target_include_directories(SudekiMP.SharedInventoryEntitlementLedgerTest PRIVATE src)
target_compile_options(SudekiMP.SharedInventoryEntitlementLedgerTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.SharedInventoryEntitlementLedgerTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.EconomyCoordinatorTest
    tests/economy_coordinator_test.c
    src/engine/economy_coordinator.c
    src/engine/personal_wallet.c
    src/engine/shared_inventory_entitlement_ledger.c
)
target_include_directories(SudekiMP.EconomyCoordinatorTest PRIVATE src)
target_compile_options(SudekiMP.EconomyCoordinatorTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.EconomyCoordinatorTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.EconomySidecarTest
    tests/economy_sidecar_test.c
    src/engine/economy_coordinator.c
    src/engine/economy_sidecar.c
    src/engine/personal_wallet.c
    src/engine/shared_inventory_entitlement_ledger.c
)
target_include_directories(SudekiMP.EconomySidecarTest PRIVATE src)
target_compile_options(SudekiMP.EconomySidecarTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.EconomySidecarTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.MerchantCheckoutTest
    tests/merchant_checkout_test.c
    src/engine/economy_coordinator.c
    src/engine/merchant_checkout.c
    src/engine/personal_wallet.c
    src/engine/shared_inventory_entitlement_ledger.c
)

add_executable(SudekiMP.MerchantProvenanceTest
    tests/merchant_provenance_test.c
    src/engine/merchant_provenance.c
)
target_include_directories(SudekiMP.MerchantProvenanceTest PRIVATE src)
target_compile_options(SudekiMP.MerchantProvenanceTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.MerchantProvenanceTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.MerchantCheckoutTest PRIVATE src)
target_compile_options(SudekiMP.MerchantCheckoutTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.MerchantCheckoutTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.ShopCatalogAdapterTest
    tests/shop_catalog_adapter_test.c
    src/hooks/shop_catalog_adapter.c
)
target_include_directories(SudekiMP.ShopCatalogAdapterTest PRIVATE src)
target_compile_options(SudekiMP.ShopCatalogAdapterTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.ShopCatalogAdapterTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.BlacksmithShadowTest
    tests/blacksmith_shadow_test.c
    src/engine/blacksmith_shadow.c
)
target_include_directories(SudekiMP.BlacksmithShadowTest PRIVATE src)
target_compile_options(SudekiMP.BlacksmithShadowTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_target_properties(SudekiMP.BlacksmithShadowTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.BlacksmithUiSessionTest
    tests/blacksmith_ui_session_test.c
    src/engine/blacksmith_read_model.c
    src/engine/blacksmith_ui_session.c
    src/engine/blacksmith_shadow.c
)
target_include_directories(SudekiMP.BlacksmithUiSessionTest PRIVATE src)
target_compile_options(SudekiMP.BlacksmithUiSessionTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_target_properties(SudekiMP.BlacksmithUiSessionTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.BlacksmithUiPresenterTest
    tests/blacksmith_ui_presenter_test.c
    src/engine/blacksmith_read_model.c
    src/engine/blacksmith_ui_presenter.c
)
target_include_directories(SudekiMP.BlacksmithUiPresenterTest PRIVATE src)
target_compile_options(SudekiMP.BlacksmithUiPresenterTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.BlacksmithUiPresenterTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.BlacksmithReadModelTest
    tests/blacksmith_read_model_test.c
    src/engine/blacksmith_read_model.c
)
target_include_directories(SudekiMP.BlacksmithReadModelTest PRIVATE src)
target_compile_options(SudekiMP.BlacksmithReadModelTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_target_properties(SudekiMP.BlacksmithReadModelTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.BlacksmithReadAdapterTest
    tests/blacksmith_read_adapter_test.c
    src/engine/blacksmith_read_model.c
    src/hooks/blacksmith_read_adapter.c
)
target_include_directories(SudekiMP.BlacksmithReadAdapterTest PRIVATE src)
target_compile_definitions(SudekiMP.BlacksmithReadAdapterTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.BlacksmithReadAdapterTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_target_properties(SudekiMP.BlacksmithReadAdapterTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.BlacksmithCommitAdapterTest
    tests/blacksmith_commit_adapter_test.c
    src/engine/blacksmith_commit_adapter.c
    src/engine/blacksmith_shadow.c
)
target_include_directories(SudekiMP.BlacksmithCommitAdapterTest PRIVATE src)
target_compile_options(SudekiMP.BlacksmithCommitAdapterTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_target_properties(SudekiMP.BlacksmithCommitAdapterTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.InteractionProvenanceTest
    tests/interaction_provenance_test.c
    src/engine/player_statehood.c
    src/hooks/call_hook.c
    src/hooks/interaction_provenance.c
)
target_include_directories(SudekiMP.InteractionProvenanceTest PRIVATE src)
target_compile_definitions(SudekiMP.InteractionProvenanceTest PRIVATE
    WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    SUDEKIMP_INTERACTION_PROVENANCE_TESTING=1
)
target_compile_options(SudekiMP.InteractionProvenanceTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_source_files_properties(src/hooks/interaction_provenance.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(src/hooks/merchant_provenance_adapter.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
set_target_properties(SudekiMP.InteractionProvenanceTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.OrbitCameraTest
    tests/orbit_camera_test.c
    src/engine/orbit_camera.c
)
target_include_directories(SudekiMP.OrbitCameraTest PRIVATE src)
target_compile_options(SudekiMP.OrbitCameraTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_target_properties(SudekiMP.OrbitCameraTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.CleanroomEngineTest
    tests/cleanroom_engine_test.c
    src/cleanroom/engine.c
    src/engine/log.c
    src/hooks/call_hook.c
)
target_include_directories(SudekiMP.CleanroomEngineTest PRIVATE src)
target_compile_definitions(SudekiMP.CleanroomEngineTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.CleanroomEngineTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_source_files_properties(
    tests/cleanroom_engine_test.c
    src/cleanroom/engine.c
    PROPERTIES COMPILE_OPTIONS -Wno-pedantic
)
set_target_properties(SudekiMP.CleanroomEngineTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

configure_file(config/SudekiMP.ini "${CMAKE_BINARY_DIR}/bin/SudekiMP.ini" COPYONLY)
