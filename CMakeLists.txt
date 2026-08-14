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
    src/engine/arbiter_combat_input.c
    src/engine/camera_target_abi.c
    src/engine/log.c
    src/input/key_binding.c
    src/hooks/call_hook.c
    src/hooks/character_switch_trace.c
    src/hooks/control_separation.c
    src/hooks/freeroam_camera_input.c
    src/hooks/pattern_scan.c
    src/hooks/player_input_trace.c
    src/hooks/quick_menu.c
    src/hooks/quick_skill_input.c
    src/hooks/spirit_strike_input.c
    src/hooks/skill_trace.c
    src/loader/sudekimp.def
    ${SUDEKIMP_COMMON_SOURCES}
)

target_include_directories(SudekiMP PRIVATE src)
target_compile_definitions(SudekiMP PRIVATE WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_compile_options(SudekiMP PRIVATE -Wall -Wextra -Wpedantic)
set_source_files_properties(src/hooks/skill_trace.c PROPERTIES
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
set_source_files_properties(src/hooks/control_separation.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(src/hooks/freeroam_camera_input.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(src/hooks/player_input_trace.c PROPERTIES
    COMPILE_OPTIONS -Wno-pedantic
)
set_source_files_properties(src/engine/arbiter_combat_input.c PROPERTIES
    COMPILE_OPTIONS "-Wno-pedantic;-Wno-unused-parameter"
)
target_link_libraries(SudekiMP PRIVATE advapi32)
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

add_executable(SudekiMP.KeyBindingTest
    tests/key_binding_test.c
    src/input/key_binding.c
)

add_executable(SudekiMP.ArbiterCombatInputTest
    tests/arbiter_combat_input_test.c
    src/engine/arbiter_combat_input.c
)
add_executable(SudekiMP.CameraTargetAbiTest
    tests/camera_target_abi_test.c
    src/engine/camera_target_abi.c
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
    src/engine/arbiter_combat_input.c
    src/engine/camera_target_abi.c
    src/engine/log.c
    src/hooks/call_hook.c
    src/hooks/character_switch_trace.c
    src/hooks/control_separation.c
    src/hooks/freeroam_camera_input.c
    src/hooks/player_input_trace.c
    src/hooks/quick_skill_input.c
    src/hooks/spirit_strike_input.c
    src/hooks/skill_trace.c
)
target_include_directories(SudekiMP.SkillTraceImageTest PRIVATE src)
target_compile_definitions(SudekiMP.SkillTraceImageTest PRIVATE
    WIN32_LEAN_AND_MEAN UNICODE _UNICODE
)
target_compile_options(SudekiMP.SkillTraceImageTest PRIVATE
    -Wall -Wextra -Wpedantic
)
target_link_options(SudekiMP.SkillTraceImageTest PRIVATE -municode)
set_target_properties(SudekiMP.SkillTraceImageTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

configure_file(config/SudekiMP.ini "${CMAKE_BINARY_DIR}/bin/SudekiMP.ini" COPYONLY)
