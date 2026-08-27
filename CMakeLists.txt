cmake_minimum_required(VERSION 3.20)

project(SudekiMP VERSION 0.1.0 LANGUAGES C)

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
    src/cleanroom/audio.c
    src/cleanroom/engine.c
    src/cleanroom/menu.c
    src/engine/arbiter_combat_input.c
    src/engine/blacksmith_commit_adapter.c
    src/engine/blacksmith_read_model.c
    src/engine/blacksmith_shadow.c
    src/engine/blacksmith_ui_session.c
    src/engine/camera_target_abi.c
    src/engine/render_phase_abi.c
    src/engine/log.c
    src/engine/orbit_camera.c
    src/engine/personal_wallet.c
    src/engine/player_combat_context.c
    src/engine/player_statehood.c
    src/engine/roaming_boundary.c
    src/engine/skill_activation_abi.c
    src/engine/transition_vote.c
    src/input/bridge_protocol.c
    src/input/bridge_receiver.c
    src/input/key_binding.c
    src/hooks/accelerator_cache.c
    src/hooks/blacksmith_read_adapter.c
    src/hooks/blacksmith_ui_adapter.c
    src/hooks/call_hook.c
    src/hooks/character_switch_trace.c
    src/hooks/control_separation.c
    src/hooks/freeroam_camera_input.c
    src/hooks/pattern_scan.c
    src/hooks/player_input_trace.c
    src/hooks/zone_transition_trace.c
    src/hooks/quick_menu.c
    src/hooks/quick_skill_input.c
    src/hooks/spirit_strike_input.c
    src/hooks/split_screen_render.c
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

target_include_directories(SudekiMP.Launcher PRIVATE src)
target_compile_definitions(SudekiMP.Launcher PRIVATE WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_compile_options(SudekiMP.Launcher PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(SudekiMP.Launcher PRIVATE -municode)
target_link_libraries(SudekiMP.Launcher PRIVATE advapi32)
set_target_properties(SudekiMP.Launcher PROPERTIES
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
    src/engine/blacksmith_read_model.c
    src/engine/blacksmith_shadow.c
    src/engine/blacksmith_ui_session.c
    src/engine/camera_target_abi.c
    src/engine/render_phase_abi.c
    src/engine/log.c
    src/engine/orbit_camera.c
    src/engine/player_combat_context.c
    src/engine/player_statehood.c
    src/engine/roaming_boundary.c
    src/engine/skill_activation_abi.c
    src/engine/transition_vote.c
    src/input/bridge_protocol.c
    src/input/bridge_receiver.c
    src/hooks/call_hook.c
    src/hooks/blacksmith_read_adapter.c
    src/hooks/blacksmith_ui_adapter.c
    src/hooks/character_switch_trace.c
    src/hooks/control_separation.c
    src/hooks/freeroam_camera_input.c
    src/hooks/player_input_trace.c
    src/hooks/quick_skill_input.c
    src/hooks/spirit_strike_input.c
    src/hooks/split_screen_render.c
    src/hooks/skill_trace.c
    src/hooks/talos_defense_trace.c
    src/hooks/zone_transition_trace.c
)
target_include_directories(SudekiMP.SkillTraceImageTest PRIVATE src)
target_compile_definitions(SudekiMP.SkillTraceImageTest PRIVATE
    WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    SUDEKIMP_BLACKSMITH_UI_ADAPTER_TESTING=1
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

add_executable(SudekiMP.InputBridgeReceiverTest
    tests/input_bridge_receiver_test.c
    src/engine/log.c
    src/input/bridge_protocol.c
    src/input/bridge_receiver.c
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
