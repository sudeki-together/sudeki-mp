cmake_minimum_required(VERSION 3.20)

project(SudekiMP VERSION 0.1.0 LANGUAGES C RC)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "SudekiMP must be cross-compiled as a 32-bit Windows target")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(SUDEKIMP_CAST_INSTANCE_PROBE "Private idle-only native Spirit lifetime probe (no overlapping activation)" OFF)
option(SUDEKIMP_CAST_ACTIVE_PROBE "Private single-cast native instance integration test (no overlap admission)" OFF)
option(SUDEKIMP_CAST_CAMERA_PROBE "Private Buki-host/Elco-client scoped named-camera test (no overlap admission)" OFF)
option(SUDEKIMP_CAST_LIGHT_PROBE "Private caster lighting lifetime test (requires camera probe; no overlap admission)" OFF)
option(SUDEKIMP_CAST_SKILL_OVERLAP_PROBE "Private Buki/Elco ordinary-skill overlap test; Spirit overlap stays guarded" OFF)
if(SUDEKIMP_CAST_SKILL_OVERLAP_PROBE AND NOT (SUDEKIMP_CAST_CAMERA_PROBE AND SUDEKIMP_CAST_LIGHT_PROBE))
    message(FATAL_ERROR "Ordinary skill overlap requires scoped cameras and caster lighting")
endif()
if(SUDEKIMP_CAST_LIGHT_PROBE AND NOT SUDEKIMP_CAST_CAMERA_PROBE)
    message(FATAL_ERROR "Caster lighting test requires the scoped camera test")
endif()
if(SUDEKIMP_CAST_CAMERA_PROBE AND (SUDEKIMP_CAST_INSTANCE_PROBE OR SUDEKIMP_CAST_ACTIVE_PROBE))
    message(FATAL_ERROR "Scoped camera test must use persistent runtime, not a lifetime/whole-frame probe")
endif()
if(SUDEKIMP_CAST_INSTANCE_PROBE AND SUDEKIMP_CAST_ACTIVE_PROBE)
    message(FATAL_ERROR "Select only one native Spirit instance probe")
endif()

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
    src/ui/menu_button.c
    src/ui/title_button.c
    src/ui/title_menu_view.c
    src/ui/title_lobby.c
    src/ui/save_catalog.c
    src/ui/party_menu.c
    src/network/title_lobby.c
    src/loader/lobby_launch.c
    src/hooks/lobby_instance.c
    src/hooks/lobby_gameplay.c
    src/hooks/lan_party_presence.c
    src/hooks/lan_party_menu_native.c
    src/hooks/lan_party_local_control.c
    src/hooks/title_multiplayer.c
    src/engine/arbiter_combat_input.c
    src/engine/buki_replica_native.c
    src/engine/controller_action_router.c
    src/engine/coop_roster_assignment.c
    src/engine/party_ownership.c
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
    src/network/lan_arena_shared_simulation.c
    src/network/lan_arena_tal_combo_graph.c
    src/network/lan_arena_endpoint.c
    src/network/lan_party_session.c
    src/network/lan_party_story.c
    src/network/lan_story_frame.c
    src/network/lan_story_handoff.c
    src/network/lan_story_catchup.c
    src/network/lan_story_presentation.c
    src/network/lan_story_world_frame.c
    src/network/lan_party_replica.c
    src/hooks/lan_party_host_control.c
    src/hooks/lan_party_client_control.c
    src/hooks/lan_party_projectile_lifetime.c
    src/hooks/lan_party_snapshot.c
    src/hooks/lan_party_jetpack.c
    src/hooks/lan_party_runtime.c
    src/hooks/lan_party_follow.c
    src/engine/party_follow.c
    src/hooks/lan_story_runtime.c
    src/hooks/lan_story_load.c
    src/hooks/lan_story_task_trace.c
    src/hooks/lan_story_observer.c
    src/hooks/lan_story_snapshot.c
    src/hooks/lan_story_shots.c
    src/hooks/lan_story_cinematic.c
    src/hooks/lan_story_client.c
    src/hooks/lan_story_recruit.c
    src/hooks/lan_story_local_control.c
    src/hooks/lan_story_activity.c
    src/hooks/lan_story_cinematic_audio.c
    src/hooks/lan_story_recruit_setup.c
    src/ui/story_cinematic_view.c
    src/hooks/lan_story_input.c
    src/hooks/lan_story_replica.c
    src/hooks/lan_story_world.c
    src/hooks/lan_story_curve.c
    src/hooks/lan_story_material.c
    src/hooks/lan_story_residency.c
    src/hooks/lan_story_render.c
    src/hooks/lan_story_resource_file.c
    src/hooks/lan_story_effects.c
    src/hooks/lan_story_realtime.c
    src/hooks/lan_story_quick_menu.c
    src/hooks/lan_story_cast.c
    src/hooks/lan_story_host_control.c
    src/hooks/lan_story_control.c
    src/hooks/lan_story_menu.c
    src/hooks/lan_party_cast.c
    src/engine/cast_motion_blur_abi.c
    src/hooks/lan_party_combo_hud.c
    src/hooks/lan_party_dummy_overlay.c
    src/network/lan_party_motion.c
    src/hooks/lan_party_roster.c
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
    src/hooks/lan_arena_hit_feedback.c
    src/hooks/lan_arena_ranged_aim.c
    src/hooks/lan_arena_cast_context.c
    src/engine/spirit_instance_abi.c
    src/engine/cast_light_abi.c
    src/hooks/lan_arena_spirit_audio.c
    src/hooks/lan_arena_spirit_visual_host.c
    src/hooks/lan_arena_spirit_vfx.c
    src/hooks/lan_arena_startup_movie_skip.c
    src/hooks/lan_arena_client_input.c
    src/hooks/lan_arena_client_replica.c
    src/hooks/lan_arena_client_skill_handoff.c
    src/hooks/lan_arena_owner_view.c
    src/hooks/lan_arena_skill_fade.c
    src/hooks/lan_arena_host_input.c
    src/hooks/lan_arena_collision_debug.c
    src/hooks/lan_arena_pause_panel.c
    src/hooks/merchant_provenance_adapter.c
    src/hooks/noncaster_skill_locomotion.c
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
target_link_libraries(SudekiMP PRIVATE advapi32 ws2_32 winmm bcrypt)
set_target_properties(SudekiMP PROPERTIES
    PREFIX ""
    OUTPUT_NAME "SudekiMP"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.Launcher
    src/loader/launcher.c
    src/loader/lobby_launch.c
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
target_link_libraries(SudekiMP.Launcher PRIVATE advapi32 bcrypt)
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

if(SUDEKIMP_CAST_INSTANCE_PROBE)
    target_compile_definitions(SudekiMP PRIVATE SUDEKIMP_CAST_INSTANCE_PROBE=1)
endif()
if(SUDEKIMP_CAST_ACTIVE_PROBE)
    target_compile_definitions(SudekiMP PRIVATE SUDEKIMP_CAST_ACTIVE_PROBE=1)
endif()
if(SUDEKIMP_CAST_CAMERA_PROBE)
    target_compile_definitions(SudekiMP PRIVATE SUDEKIMP_CAST_CAMERA_PROBE=1)
endif()
if(SUDEKIMP_CAST_LIGHT_PROBE)
    target_compile_definitions(SudekiMP PRIVATE SUDEKIMP_CAST_LIGHT_PROBE=1)
endif()
if(SUDEKIMP_CAST_SKILL_OVERLAP_PROBE)
    target_compile_definitions(SudekiMP PRIVATE SUDEKIMP_CAST_SKILL_OVERLAP_PROBE=1)
endif()

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

add_executable(SudekiMP.LanCastContextTest
    tests/lan_arena_cast_context_test.c
    src/hooks/call_hook.c
)
target_include_directories(SudekiMP.LanCastContextTest PRIVATE src)
target_compile_definitions(SudekiMP.LanCastContextTest PRIVATE WIN32_LEAN_AND_MEAN)
target_compile_options(SudekiMP.LanCastContextTest PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(SudekiMP.LanCastContextTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(SudekiMP.SpiritInstanceAbiTest tests/spirit_instance_abi_test.c src/hooks/call_hook.c src/engine/cast_light_abi.c)
add_executable(SudekiMP.LanArenaWindowPolicyTest tests/lan_arena_window_policy_test.c src/hooks/call_hook.c)
target_include_directories(SudekiMP.LanArenaWindowPolicyTest PRIVATE src)
target_compile_definitions(SudekiMP.LanArenaWindowPolicyTest PRIVATE WIN32_LEAN_AND_MEAN)
target_compile_options(SudekiMP.LanArenaWindowPolicyTest PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(SudekiMP.LanArenaWindowPolicyTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_include_directories(SudekiMP.SpiritInstanceAbiTest PRIVATE src)
target_compile_definitions(SudekiMP.SpiritInstanceAbiTest PRIVATE WIN32_LEAN_AND_MEAN)
target_compile_options(SudekiMP.SpiritInstanceAbiTest PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(SudekiMP.SpiritInstanceAbiTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.CastLightAbiTest tests/cast_light_abi_test.c src/hooks/call_hook.c)
target_include_directories(SudekiMP.CastLightAbiTest PRIVATE src)
target_compile_definitions(SudekiMP.CastLightAbiTest PRIVATE WIN32_LEAN_AND_MEAN)
target_compile_options(SudekiMP.CastLightAbiTest PRIVATE -Wall -Wextra)
set_target_properties(SudekiMP.CastLightAbiTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(SudekiMP.LanArenaRuntimeHookTest
    tests/lan_arena_runtime_hook_test.c
    src/hooks/call_hook.c
    src/hooks/lan_arena_owner_view.c
    src/hooks/noncaster_skill_locomotion.c
    src/network/lan_arena_protocol.c
    src/network/lan_arena_shared_simulation.c
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

add_executable(SudekiMP.LanArenaHitFeedbackTest
    tests/lan_arena_hit_feedback_test.c
    src/hooks/call_hook.c
    src/engine/log.c
    src/network/lan_arena_protocol.c)
target_include_directories(SudekiMP.LanArenaHitFeedbackTest PRIVATE src)
target_compile_definitions(SudekiMP.LanArenaHitFeedbackTest PRIVATE WIN32_LEAN_AND_MEAN)
target_compile_options(SudekiMP.LanArenaHitFeedbackTest PRIVATE -Wall -Wextra)
set_target_properties(SudekiMP.LanArenaHitFeedbackTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(SudekiMP.LanArenaRangedAimTest tests/lan_arena_ranged_aim_test.c
    src/hooks/call_hook.c src/engine/log.c)
target_include_directories(SudekiMP.LanArenaRangedAimTest PRIVATE src)
target_compile_definitions(SudekiMP.LanArenaRangedAimTest PRIVATE WIN32_LEAN_AND_MEAN)
target_compile_options(SudekiMP.LanArenaRangedAimTest PRIVATE -Wall -Wextra)
set_target_properties(SudekiMP.LanArenaRangedAimTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(SudekiMP.LanArenaOwnerViewTest
    tests/lan_arena_owner_view_test.c
    src/hooks/lan_arena_owner_view.c
)
add_executable(SudekiMP.LanArenaSkillFadeTest
    tests/lan_arena_skill_fade_test.c
    src/hooks/call_hook.c
)
target_include_directories(SudekiMP.LanArenaSkillFadeTest PRIVATE src)
target_compile_definitions(SudekiMP.LanArenaSkillFadeTest PRIVATE WIN32_LEAN_AND_MEAN)
target_compile_options(SudekiMP.LanArenaSkillFadeTest PRIVATE -Wall -Wextra)
set_target_properties(SudekiMP.LanArenaSkillFadeTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_include_directories(SudekiMP.LanArenaOwnerViewTest PRIVATE src)
target_compile_definitions(SudekiMP.LanArenaOwnerViewTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.LanArenaOwnerViewTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.LanArenaOwnerViewTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.LanArenaSpiritAudioTest
    tests/lan_arena_spirit_audio_test.c
    src/hooks/lan_arena_spirit_audio.c
    src/hooks/call_hook.c
    src/engine/log.c
)

add_executable(SudekiMP.LanArenaSpiritVfxTest
    tests/lan_arena_spirit_vfx_test.c
    src/hooks/lan_arena_spirit_vfx.c
    src/network/lan_arena_protocol.c
)
add_executable(SudekiMP.LanArenaSpiritVisualHostTest
    tests/lan_arena_spirit_visual_host_test.c
    src/hooks/lan_arena_spirit_visual_host.c
    src/hooks/call_hook.c
    src/network/lan_arena_protocol.c
)
target_include_directories(SudekiMP.LanArenaSpiritVisualHostTest PRIVATE src)
target_compile_definitions(SudekiMP.LanArenaSpiritVisualHostTest PRIVATE
    WIN32_LEAN_AND_MEAN
    SUDEKIMP_SPIRIT_VISUAL_HOST_TESTING=1
)
target_compile_options(SudekiMP.LanArenaSpiritVisualHostTest PRIVATE
    -Wall -Wextra -Werror
)
set_target_properties(SudekiMP.LanArenaSpiritVisualHostTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.LanArenaSpiritVfxTest PRIVATE src)
target_compile_definitions(SudekiMP.LanArenaSpiritVfxTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.LanArenaSpiritVfxTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_source_files_properties(
    tests/lan_arena_spirit_vfx_test.c
    src/hooks/lan_arena_spirit_vfx.c
    PROPERTIES COMPILE_OPTIONS -Wno-pedantic
)
set_target_properties(SudekiMP.LanArenaSpiritVfxTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_include_directories(SudekiMP.LanArenaSpiritAudioTest PRIVATE src)
target_compile_definitions(SudekiMP.LanArenaSpiritAudioTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.LanArenaSpiritAudioTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_source_files_properties(
    tests/lan_arena_spirit_audio_test.c
    src/hooks/lan_arena_spirit_audio.c
    PROPERTIES COMPILE_OPTIONS -Wno-pedantic
)
set_target_properties(SudekiMP.LanArenaSpiritAudioTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.LanArenaStartupMovieSkipTest
    tests/lan_arena_startup_movie_skip_test.c
    src/hooks/lan_arena_startup_movie_skip.c
    src/hooks/call_hook.c
    src/engine/log.c
)
target_include_directories(SudekiMP.LanArenaStartupMovieSkipTest PRIVATE src)
target_compile_definitions(SudekiMP.LanArenaStartupMovieSkipTest PRIVATE
    WIN32_LEAN_AND_MEAN
)
target_compile_options(SudekiMP.LanArenaStartupMovieSkipTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_source_files_properties(
    tests/lan_arena_startup_movie_skip_test.c
    src/hooks/lan_arena_startup_movie_skip.c
    PROPERTIES COMPILE_OPTIONS -Wno-pedantic
)
set_target_properties(SudekiMP.LanArenaStartupMovieSkipTest PROPERTIES
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
    src/engine/build_identity.c
    src/engine/sha256.c
    src/hooks/lan_party_menu_native.c
    src/hooks/lan_party_local_control.c
    src/hooks/lan_party_follow.c
    src/engine/party_follow.c
    src/engine/party_ownership.c
    tests/skill_trace_image_test.c
    tests/lan_party_control_image_fixture.c
    src/network/lan_party_session.c
    src/network/lan_party_story.c
    src/network/lan_story_frame.c
    src/network/lan_story_handoff.c
    src/network/lan_story_catchup.c
    src/network/lan_story_presentation.c
    src/network/lan_story_world_frame.c
    src/hooks/lan_party_host_control.c
    src/hooks/lan_party_client_control.c
    src/hooks/lan_party_jetpack.c
    src/hooks/lan_party_projectile_lifetime.c
    src/hooks/lan_party_snapshot.c
    src/hooks/lan_party_cast.c
    src/engine/cast_motion_blur_abi.c
    src/hooks/lan_party_combo_hud.c
    src/hooks/lan_party_dummy_overlay.c
    src/network/lan_party_motion.c
    src/hooks/lan_party_roster.c
    src/cleanroom/engine.c
    src/engine/arbiter_combat_input.c
    src/engine/buki_replica_native.c
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
    src/network/lan_arena_shared_simulation.c
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
    src/hooks/lan_arena_client_skill_handoff.c
    src/hooks/lan_arena_owner_view.c
    src/hooks/lan_arena_skill_fade.c
    src/hooks/lan_arena_cast_context.c
    src/engine/spirit_instance_abi.c
    src/engine/cast_light_abi.c
    src/hooks/lan_arena_spirit_audio.c
    src/hooks/lan_arena_spirit_visual_host.c
    src/hooks/lan_arena_spirit_vfx.c
    src/hooks/lan_arena_host_input.c
    src/hooks/lan_arena_hit_feedback.c
    src/hooks/lan_arena_ranged_aim.c
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
    SUDEKIMP_LAN_ARENA_CLIENT_REPLICA_TESTING=1
    SUDEKIMP_LAN_ARENA_PAUSE_PANEL_TESTING=1
    SUDEKIMP_SAVE_BOOK_INTERCEPT_TESTING=1
    SUDEKIMP_LAN_PARTY_CONTROL_TESTING=1
    SUDEKIMP_LAN_PARTY_FOLLOW_TESTING=1
)
target_compile_options(SudekiMP.SkillTraceImageTest PRIVATE
    -Wall -Wextra -Wpedantic
)
target_link_options(SudekiMP.SkillTraceImageTest PRIVATE -municode)
target_link_libraries(SudekiMP.SkillTraceImageTest PRIVATE ws2_32 bcrypt)
# This fixture exercises the same GCC x86 thiscall/stdcall ABI as the adapter.
set_source_files_properties(tests/lan_party_control_image_fixture.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
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
add_executable(SudekiMP.PartyOwnershipTest
    tests/party_ownership_test.c src/engine/party_ownership.c)
target_include_directories(SudekiMP.PartyOwnershipTest PRIVATE src)
target_compile_options(SudekiMP.PartyOwnershipTest PRIVATE -Wall -Wextra -Wpedantic -Werror)
set_target_properties(SudekiMP.PartyOwnershipTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
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

add_executable(SudekiMP.LanArenaSharedSimulationTest
    tests/lan_arena_shared_simulation_test.c
    src/network/lan_arena_protocol.c
    src/network/lan_arena_shared_simulation.c
)
target_include_directories(SudekiMP.LanArenaSharedSimulationTest PRIVATE src)
target_compile_options(SudekiMP.LanArenaSharedSimulationTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.LanArenaSharedSimulationTest PROPERTIES
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
add_executable(SudekiMP.LanArenaMultiCastTest
    tests/lan_arena_multicast_test.c
    src/network/lan_arena_protocol.c
    src/network/lan_arena_shared_simulation.c
    src/network/lan_arena_tal_combo_graph.c
    src/network/lan_arena_replica.c
)
target_include_directories(SudekiMP.LanArenaMultiCastTest PRIVATE src)
target_compile_options(SudekiMP.LanArenaMultiCastTest PRIVATE -Wall -Wextra -Wpedantic -Werror)
set_target_properties(SudekiMP.LanArenaMultiCastTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_include_directories(SudekiMP.LanArenaReplicaTest PRIVATE src)
target_compile_options(SudekiMP.LanArenaReplicaTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.LanArenaReplicaTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.LanArenaClientSkillHandoffTest
    tests/lan_arena_client_skill_handoff_test.c
    src/hooks/lan_arena_client_skill_handoff.c
)
target_include_directories(SudekiMP.LanArenaClientSkillHandoffTest PRIVATE src)
target_compile_options(SudekiMP.LanArenaClientSkillHandoffTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
set_target_properties(SudekiMP.LanArenaClientSkillHandoffTest PROPERTIES
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
    src/network/lan_arena_authority.c
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
    src/network/lan_arena_authority.c
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

add_executable(SudekiMP.LanPartySessionTest
    src/hooks/lan_party_presence.c
    src/engine/party_ownership.c
    src/network/lan_party_story.c
    src/network/lan_story_frame.c
    src/network/lan_story_handoff.c
    src/network/lan_story_catchup.c
    src/network/lan_story_presentation.c
    src/network/lan_story_world_frame.c
    tests/lan_party_session_test.c
    src/network/lan_party_motion.c
    src/network/lan_party_replica.c
    src/network/lan_arena_replica.c
    src/network/lan_arena_tal_combo_graph.c
    src/network/lan_arena_protocol.c
    src/network/lan_party_session.c
)
target_include_directories(SudekiMP.LanPartySessionTest PRIVATE src)
target_compile_definitions(SudekiMP.LanPartySessionTest PRIVATE WIN32_LEAN_AND_MEAN)
target_compile_options(SudekiMP.LanPartySessionTest PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
target_link_libraries(SudekiMP.LanPartySessionTest PRIVATE ws2_32 bcrypt)
set_target_properties(SudekiMP.LanPartySessionTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.LanPartyClientControlTest tests/lan_party_client_control_test.c)
target_include_directories(SudekiMP.LanPartyClientControlTest PRIVATE src)
target_compile_options(SudekiMP.LanPartyClientControlTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
set_target_properties(SudekiMP.LanPartyClientControlTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(SudekiMP.TitleLobbyProtocolTest tests/title_lobby_protocol_test.c)
target_include_directories(SudekiMP.TitleLobbyProtocolTest PRIVATE src)
target_compile_options(SudekiMP.TitleLobbyProtocolTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.TitleLobbyProtocolTest PRIVATE ws2_32 bcrypt)
set_target_properties(SudekiMP.TitleLobbyProtocolTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(SudekiMP.LanPartyProjectileLifetimeTest
    tests/lan_party_projectile_lifetime_test.c
    src/hooks/call_hook.c
    src/engine/build_identity.c
    src/engine/sha256.c)
target_include_directories(SudekiMP.LanPartyProjectileLifetimeTest PRIVATE src)
target_compile_options(SudekiMP.LanPartyProjectileLifetimeTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_options(SudekiMP.LanPartyProjectileLifetimeTest PRIVATE -municode)
target_link_libraries(SudekiMP.LanPartyProjectileLifetimeTest PRIVATE advapi32)
set_target_properties(SudekiMP.LanPartyProjectileLifetimeTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(SudekiMP.LobbyLaunchPlanTest
    tests/lobby_launch_plan_test.c
    src/loader/lobby_launch.c
    src/engine/sha256.c)
target_include_directories(SudekiMP.LobbyLaunchPlanTest PRIVATE src)
target_compile_options(SudekiMP.LobbyLaunchPlanTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.LobbyLaunchPlanTest PRIVATE advapi32 bcrypt)
set_target_properties(SudekiMP.LobbyLaunchPlanTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(SudekiMP.PartyFollowTest
    tests/party_follow_test.c
    src/engine/party_follow.c)
target_include_directories(SudekiMP.PartyFollowTest PRIVATE src)
target_compile_options(SudekiMP.PartyFollowTest PRIVATE -Wall -Wextra -Werror)
target_link_libraries(SudekiMP.PartyFollowTest PRIVATE m)
set_target_properties(SudekiMP.PartyFollowTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

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
add_executable(SudekiMP.BukiReplicaNativeTest
    tests/buki_replica_native_test.c
    src/engine/buki_replica_native.c
)
target_include_directories(SudekiMP.BukiReplicaNativeTest PRIVATE src)
target_compile_definitions(SudekiMP.BukiReplicaNativeTest PRIVATE
    WIN32_LEAN_AND_MEAN SUDEKIMP_BUKI_REPLICA_NATIVE_TESTING=1)
target_compile_options(SudekiMP.BukiReplicaNativeTest PRIVATE -Wall -Wextra)
set_target_properties(SudekiMP.BukiReplicaNativeTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
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
    src/engine/weapon_activation_abi.c
    src/engine/log.c
    src/hooks/call_hook.c
)
target_include_directories(SudekiMP.CleanroomEngineTest PRIVATE src)
target_compile_definitions(SudekiMP.CleanroomEngineTest PRIVATE
    WIN32_LEAN_AND_MEAN
    SUDEKIMP_CLEANROOM_ENGINE_TESTING=1
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

add_executable(SudekiMP.NoncasterSkillLocomotionTest
    tests/noncaster_skill_locomotion_test.c
    src/hooks/noncaster_skill_locomotion.c
)
target_include_directories(SudekiMP.NoncasterSkillLocomotionTest PRIVATE src)
target_compile_definitions(SudekiMP.NoncasterSkillLocomotionTest PRIVATE
    WIN32_LEAN_AND_MEAN
    SUDEKIMP_NONCASTER_SKILL_LOCOMOTION_TESTING=1
)
target_compile_options(SudekiMP.NoncasterSkillLocomotionTest PRIVATE
    -Wall -Wextra -Wpedantic
)
set_target_properties(SudekiMP.NoncasterSkillLocomotionTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(SudekiMP.RangedWorldPoseTest tests/ranged_world_pose_test.c)
add_executable(SudekiMP.StoryShotsTest tests/story_shots_test.c
    src/network/lan_story_frame.c src/network/lan_party_story.c
    src/network/lan_party_motion.c src/network/lan_arena_protocol.c
    src/network/lan_arena_tal_combo_graph.c src/network/lan_party_session.c
    src/network/lan_story_world_frame.c src/network/lan_story_presentation.c
    src/network/lan_story_handoff.c src/network/lan_story_catchup.c
    src/engine/party_ownership.c)
target_include_directories(SudekiMP.StoryShotsTest PRIVATE src)
target_compile_definitions(SudekiMP.StoryShotsTest PRIVATE WIN32_LEAN_AND_MEAN)
target_compile_options(SudekiMP.StoryShotsTest PRIVATE -Wall -Wextra -Werror -UNDEBUG -ffunction-sections -fdata-sections)
target_link_options(SudekiMP.StoryShotsTest PRIVATE -Wl,--gc-sections)
target_link_libraries(SudekiMP.StoryShotsTest PRIVATE ws2_32 bcrypt)
set_target_properties(SudekiMP.StoryShotsTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryEmissionImageTest tests/story_emission_image_test.c
    src/hooks/call_hook.c src/engine/build_identity.c src/engine/sha256.c src/engine/log.c)
target_include_directories(SudekiMP.StoryEmissionImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryEmissionImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryEmissionImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryEmissionImageTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_include_directories(SudekiMP.RangedWorldPoseTest PRIVATE src)
target_compile_options(SudekiMP.RangedWorldPoseTest PRIVATE -Wall -Wextra -Wpedantic -UNDEBUG)
set_target_properties(SudekiMP.RangedWorldPoseTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryRangedProjectionTest tests/story_ranged_projection_test.c)
add_executable(SudekiMP.StoryWeaponPoseTest tests/story_weapon_pose_test.c)
target_include_directories(SudekiMP.StoryWeaponPoseTest PRIVATE src)
target_compile_options(SudekiMP.StoryWeaponPoseTest PRIVATE
    -Wall -Wextra -Werror -UNDEBUG -fwhole-program -ffunction-sections -fdata-sections)
target_link_options(SudekiMP.StoryWeaponPoseTest PRIVATE -Wl,--gc-sections)
set_target_properties(SudekiMP.StoryWeaponPoseTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_include_directories(SudekiMP.StoryRangedProjectionTest PRIVATE src)
target_compile_options(SudekiMP.StoryRangedProjectionTest PRIVATE
    -Wall -Wextra -UNDEBUG -fwhole-program -ffunction-sections -fdata-sections)
target_link_options(SudekiMP.StoryRangedProjectionTest PRIVATE -Wl,--gc-sections)
set_target_properties(SudekiMP.StoryRangedProjectionTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryControlStatusTest tests/story_control_status_test.c)
target_include_directories(SudekiMP.StoryControlStatusTest PRIVATE src)
target_compile_options(SudekiMP.StoryControlStatusTest PRIVATE -Wall -Wextra -UNDEBUG -fwhole-program)
set_target_properties(SudekiMP.StoryControlStatusTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryHostFilterTest tests/story_host_filter_test.c)
target_include_directories(SudekiMP.StoryHostFilterTest PRIVATE src)
target_compile_options(SudekiMP.StoryHostFilterTest PRIVATE
    -Wall -Wextra -Werror -UNDEBUG -fwhole-program -ffunction-sections -fdata-sections)
target_link_options(SudekiMP.StoryHostFilterTest PRIVATE -Wl,--gc-sections)
set_target_properties(SudekiMP.StoryHostFilterTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(SUDEKIMP_STORY_LOOT_CORE
    src/engine/story_loot.c src/engine/story_loot_codec.c
    src/network/lan_story_loot.c)
target_sources(SudekiMP PRIVATE ${SUDEKIMP_STORY_LOOT_CORE} src/engine/story_loot_sidecar.c
    src/engine/story_breakable.c src/engine/story_placement.c src/hooks/lan_story_objects.c
    src/hooks/lan_story_loot_trace.c)
foreach(target SudekiMP.SkillTraceImageTest SudekiMP.LanPartySessionTest SudekiMP.StoryShotsTest)
    target_sources(${target} PRIVATE ${SUDEKIMP_STORY_LOOT_CORE} src/engine/personal_wallet.c)
endforeach()
add_executable(SudekiMP.StoryLootTest tests/story_loot_test.c ${SUDEKIMP_STORY_LOOT_CORE}
    src/engine/personal_wallet.c src/network/lan_party_story.c)
add_executable(SudekiMP.StoryLootSidecarTest tests/story_loot_sidecar_test.c
    src/engine/story_loot.c src/engine/story_loot_codec.c src/engine/story_loot_sidecar.c
    src/engine/personal_wallet.c)
add_executable(SudekiMP.StoryBreakableTest tests/story_breakable_test.c
    src/engine/story_breakable.c src/engine/story_loot.c src/engine/personal_wallet.c)
add_executable(SudekiMP.StoryPlacementTest tests/story_placement_test.c src/engine/story_placement.c)
target_sources(SudekiMP PRIVATE src/engine/story_area.c src/hooks/lan_story_area_membership.c
    src/hooks/lan_story_collision_owner.c src/hooks/lan_story_collision_query.c
    src/hooks/lan_story_collision_lifetime.c src/hooks/lan_story_area_task.c
    src/hooks/lan_story_area_eviction.c src/hooks/lan_story_area_finalise.c
    src/hooks/lan_story_area_close.c src/hooks/lan_story_area_intent.c
    src/hooks/lan_story_temp_exterior.c src/hooks/lan_story_pause_trace.c src/hooks/lan_story_split.c
    src/hooks/lan_story_anim_trace.c src/hooks/lan_story_name_tags.c
    src/hooks/lan_story_ambient.c src/hooks/lan_story_area_fade.c)
add_executable(SudekiMP.StoryAreaFinaliseImageTest tests/story_area_finalise_image_test.c
    src/hooks/call_hook.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryAreaFinaliseImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryAreaFinaliseImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryAreaFinaliseImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryAreaFinaliseImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryAreaWorkerImageTest tests/story_area_worker_image_test.c
    src/hooks/call_hook.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryAreaWorkerImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryAreaWorkerImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryAreaWorkerImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryAreaWorkerImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryAreaRetirementImageTest tests/story_area_retirement_image_test.c
    src/hooks/call_hook.c src/hooks/lan_story_area_finalise.c src/engine/story_area.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryAreaRetirementImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryAreaRetirementImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryAreaRetirementImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryAreaRetirementImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryAreaPvsImageTest tests/story_area_pvs_image_test.c
    src/hooks/call_hook.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryAreaPvsImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryAreaPvsImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryAreaPvsImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryAreaPvsImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryAreaCloseImageTest tests/story_area_close_image_test.c
    src/hooks/call_hook.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryAreaCloseImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryAreaCloseImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryAreaCloseImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryAreaCloseImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryAreaForegroundImageTest tests/story_area_foreground_image_test.c
    src/hooks/call_hook.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryAreaForegroundImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryAreaForegroundImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryAreaForegroundImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryAreaForegroundImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(SudekiMP.StoryAreaSchedulerImageTest tests/story_area_scheduler_image_test.c
    src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryAreaSchedulerImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryAreaSchedulerImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryAreaSchedulerImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryAreaSchedulerImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(SudekiMP.StoryAreaEvictionImageTest tests/story_area_eviction_image_test.c
    src/hooks/call_hook.c src/engine/story_area.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryAreaEvictionImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryAreaEvictionImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryAreaEvictionImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryAreaEvictionImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryTempExteriorImageTest tests/story_temp_exterior_image_test.c
    src/hooks/call_hook.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryTempExteriorImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryTempExteriorImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryTempExteriorImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryTempExteriorImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StorySceneAreaTest tests/story_scene_area_test.c src/network/lan_party_story.c)
target_include_directories(SudekiMP.StorySceneAreaTest PRIVATE src)
target_compile_options(SudekiMP.StorySceneAreaTest PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
set_target_properties(SudekiMP.StorySceneAreaTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryAreaTest tests/story_area_test.c src/engine/story_area.c)
target_include_directories(SudekiMP.StoryAreaTest PRIVATE src)
target_compile_options(SudekiMP.StoryAreaTest PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
set_target_properties(SudekiMP.StoryAreaTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryAreaTaskImageTest tests/story_area_task_image_test.c
    src/hooks/lan_story_area_finalise.c src/hooks/call_hook.c src/engine/story_area.c
    src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryAreaTaskImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryAreaTaskImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryAreaTaskImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryAreaTaskImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryTemporaryContextTest tests/story_temporary_context_test.c)
target_include_directories(SudekiMP.StoryTemporaryContextTest PRIVATE src)
target_compile_options(SudekiMP.StoryTemporaryContextTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
set_target_properties(SudekiMP.StoryTemporaryContextTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryAreaMembershipImageTest tests/story_area_membership_image_test.c
    src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryAreaMembershipImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryAreaMembershipImageTest PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryAreaMembershipImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryAreaMembershipImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryCollisionOwnerImageTest tests/story_collision_owner_image_test.c
    src/hooks/lan_story_collision_owner.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryCollisionOwnerImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryCollisionOwnerImageTest PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryCollisionOwnerImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryCollisionOwnerImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryCollisionQueryImageTest tests/story_collision_query_image_test.c
    tests/story_collision_lifetime_fixture.c
    src/hooks/call_hook.c src/engine/story_area.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryCollisionQueryImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryCollisionQueryImageTest PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryCollisionQueryImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryCollisionQueryImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryCollisionTerrainImageTest tests/story_collision_terrain_image_test.c
    tests/story_collision_lifetime_fixture.c src/hooks/lan_story_collision_query.c
    src/hooks/call_hook.c src/engine/story_area.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryCollisionTerrainImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryCollisionTerrainImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryCollisionTerrainImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryCollisionTerrainImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryCollisionLifetimeImageTest tests/story_collision_lifetime_image_test.c
    src/hooks/call_hook.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryCollisionLifetimeImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryCollisionLifetimeImageTest PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryCollisionLifetimeImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryCollisionLifetimeImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryObjectsImageTest tests/story_objects_image_test.c
    src/engine/story_placement.c src/engine/build_identity.c src/engine/sha256.c)
target_link_libraries(SudekiMP.StoryObjectsImageTest PRIVATE bcrypt)
add_executable(SudekiMP.StoryLootTraceImageTest tests/story_loot_trace_image_test.c
    src/hooks/call_hook.c src/engine/build_identity.c src/engine/sha256.c)
target_link_libraries(SudekiMP.StoryLootTraceImageTest PRIVATE bcrypt)
add_executable(SudekiMP.StoryEffectClockImageTest tests/story_effect_clock_image_test.c
    src/hooks/call_hook.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryEffectClockImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryEffectClockImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryEffectClockImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryEffectClockImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryRealtimeImageTest tests/story_realtime_image_test.c
    src/hooks/call_hook.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryRealtimeImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryRealtimeImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryRealtimeImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryRealtimeImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryQuickMenuImageTest tests/story_quick_menu_image_test.c
    src/hooks/call_hook.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryQuickMenuImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryQuickMenuImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryQuickMenuImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryQuickMenuImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryMaterialImageTest tests/story_material_image_test.c
    src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryMaterialImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryMaterialImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryMaterialImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryMaterialImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryTaskHostImageTest tests/story_task_host_image_test.c
    src/hooks/lan_arena_cast_context.c src/hooks/call_hook.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryTaskHostImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryTaskHostImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryTaskHostImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryTaskHostImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryLoadIdentityTest tests/story_load_identity_test.c)
add_executable(SudekiMP.StoryReloadFilesTest tests/story_reload_files_test.c)
target_include_directories(SudekiMP.StoryReloadFilesTest PRIVATE src)
target_compile_options(SudekiMP.StoryReloadFilesTest PRIVATE -Wall -Wextra -Werror -UNDEBUG
    -fwhole-program -ffunction-sections -fdata-sections)
target_link_options(SudekiMP.StoryReloadFilesTest PRIVATE -Wl,--gc-sections)
target_link_libraries(SudekiMP.StoryReloadFilesTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryReloadFilesTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_library(SudekiMP.StorySaveOwnerFixture OBJECT src/hooks/save_book_intercept.c)
target_include_directories(SudekiMP.StorySaveOwnerFixture PRIVATE src)
# The same owner is compiled without unrelated legacy UI paths in this focused
# fixture. Its three story APIs remain externally visible; SkillTraceImageTest
# separately links the full, ordinary owner and checks existing profiles.
target_compile_options(SudekiMP.StorySaveOwnerFixture PRIVATE -Wall -Wextra -Werror
    -fwhole-program -ffunction-sections -fdata-sections)
add_executable(SudekiMP.StoryAreaIntentImageTest tests/story_area_intent_image_test.c
    src/hooks/lan_story_area_intent.c $<TARGET_OBJECTS:SudekiMP.StorySaveOwnerFixture>
    src/hooks/call_hook.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryAreaIntentImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryAreaIntentImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG
    -ffunction-sections -fdata-sections)
target_link_options(SudekiMP.StoryAreaIntentImageTest PRIVATE
    -Wl,--wrap=SudekiMpInstallInlineHook -Wl,--wrap=SudekiMpRestoreInlineHook -Wl,--gc-sections)
target_link_libraries(SudekiMP.StoryAreaIntentImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryAreaIntentImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryCastAdmissionTest tests/story_cast_admission_test.c)
add_executable(SudekiMP.StoryQuickInputTest tests/story_quick_input_test.c)
add_executable(SudekiMP.StoryMeleeAdmissionTest tests/story_melee_admission_test.c)
add_executable(SudekiMP.StoryInteractionGuardImageTest tests/story_interaction_guard_image_test.c
    src/engine/arbiter_combat_input.c src/engine/build_identity.c src/engine/sha256.c)
target_include_directories(SudekiMP.StoryInteractionGuardImageTest PRIVATE src)
target_compile_options(SudekiMP.StoryInteractionGuardImageTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryInteractionGuardImageTest PRIVATE bcrypt)
set_target_properties(SudekiMP.StoryInteractionGuardImageTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_include_directories(SudekiMP.StoryMeleeAdmissionTest PRIVATE src)
target_compile_options(SudekiMP.StoryMeleeAdmissionTest PRIVATE -Wall -Wextra -Werror -UNDEBUG
    -fwhole-program -ffunction-sections -fdata-sections)
target_link_options(SudekiMP.StoryMeleeAdmissionTest PRIVATE -Wl,--gc-sections)
set_target_properties(SudekiMP.StoryMeleeAdmissionTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_include_directories(SudekiMP.StoryQuickInputTest PRIVATE src)
target_compile_options(SudekiMP.StoryQuickInputTest PRIVATE -Wall -Wextra -Werror -UNDEBUG
    -fwhole-program -ffunction-sections -fdata-sections)
target_link_options(SudekiMP.StoryQuickInputTest PRIVATE -Wl,--gc-sections)
set_target_properties(SudekiMP.StoryQuickInputTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
get_target_property(SUDEKIMP_STORY_ACTION_TEST_SOURCES SudekiMP.StoryShotsTest SOURCES)
list(REMOVE_ITEM SUDEKIMP_STORY_ACTION_TEST_SOURCES tests/story_shots_test.c)
add_executable(SudekiMP.StoryActionTransportTest tests/story_action_transport_test.c ${SUDEKIMP_STORY_ACTION_TEST_SOURCES})
target_include_directories(SudekiMP.StoryActionTransportTest PRIVATE src)
target_compile_definitions(SudekiMP.StoryActionTransportTest PRIVATE WIN32_LEAN_AND_MEAN)
target_compile_options(SudekiMP.StoryActionTransportTest PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(SudekiMP.StoryActionTransportTest PRIVATE ws2_32 bcrypt)
set_target_properties(SudekiMP.StoryActionTransportTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_include_directories(SudekiMP.StoryCastAdmissionTest PRIVATE src)
target_compile_options(SudekiMP.StoryCastAdmissionTest PRIVATE -Wall -Wextra -Werror -UNDEBUG
    -fwhole-program -ffunction-sections -fdata-sections)
target_link_options(SudekiMP.StoryCastAdmissionTest PRIVATE -Wl,--gc-sections)
set_target_properties(SudekiMP.StoryCastAdmissionTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_include_directories(SudekiMP.StoryLoadIdentityTest PRIVATE src)
target_compile_options(SudekiMP.StoryLoadIdentityTest PRIVATE -Wall -Wextra -Werror -UNDEBUG
    -fwhole-program -ffunction-sections -fdata-sections)
target_link_options(SudekiMP.StoryLoadIdentityTest PRIVATE -Wl,--gc-sections)
set_target_properties(SudekiMP.StoryLoadIdentityTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(SudekiMP.StoryLootTransportTest tests/story_loot_transport_test.c ${SUDEKIMP_STORY_LOOT_CORE}
    src/engine/personal_wallet.c src/network/lan_story_frame.c src/network/lan_party_story.c
    src/network/lan_party_motion.c src/network/lan_arena_protocol.c
    src/network/lan_arena_tal_combo_graph.c src/network/lan_party_session.c
    src/network/lan_story_world_frame.c src/network/lan_story_presentation.c
    src/network/lan_story_handoff.c src/network/lan_story_catchup.c src/engine/party_ownership.c)
target_link_libraries(SudekiMP.StoryLootTransportTest PRIVATE ws2_32 bcrypt)
target_compile_definitions(SudekiMP.StoryLootTransportTest PRIVATE WIN32_LEAN_AND_MEAN)
foreach(target SudekiMP.StoryLootTest SudekiMP.StoryLootSidecarTest SudekiMP.StoryLootTransportTest SudekiMP.StoryBreakableTest SudekiMP.StoryPlacementTest SudekiMP.StoryObjectsImageTest SudekiMP.StoryLootTraceImageTest)
    target_include_directories(${target} PRIVATE src)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endforeach()

configure_file(config/SudekiMP.ini "${CMAKE_BINARY_DIR}/bin/SudekiMP.ini" COPYONLY)
