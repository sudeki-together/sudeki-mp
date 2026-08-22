#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
research_prefix="${SUDEKIMP_WINEPREFIX:-${HOME}/Games/sudeki-research-prefix}"
game="${SUDEKIMP_GAME:-${HOME}/Games/SudekiMP/working/SUDEKI.exe}"
source_config="${project_dir}/config/SudekiMP.ini"
generated_config="${project_dir}/build/mingw32/bin/SudekiMP.ini"
save_root="${research_prefix}/drive_c/users/steamuser/AppData/Roaming/Sudeki/Save"
mode="${1:---safe}"
speed="${2:-2.0}"
spirit_key="${2:-G}"
input_device="${SUDEKIMP_INPUT_DEVICE:-/dev/input/js0}"
input_bridge_port="${SUDEKIMP_INPUT_BRIDGE_PORT:-26760}"
input_bridge_helper="${project_dir}/build/linux/bin/sudekimp-input-bridge"
input_bridge_log="${project_dir}/build/linux/input-bridge.log"

usage() {
    printf '%s\n' \
        'usage: tools/continue-research.sh [--safe|--cleanroom|--test-arena|--cafu-testroom|--trace|--input-trace|--character-switch-trace|--party-lifecycle-trace|--talos-party-test|--zone-transition-trace|--zone-traversal-test|--freeroam-camera-test|--control-separation-test|--player-input-trace|--second-player-movement-test|--second-player-camera-movement-test|--second-player-separation-test|--shared-group-camera-test|--split-screen-render-test|--second-player-render-camera-test|--dual-camera-frame-cache-test|--shared-quit-menu-test|--viewport-hud-test|--dual-camera-local-coop-test|--controller-bridge-test|--realtime-skill-coop-test|--second-player-target-trace|--second-player-attack-test|--ranged-skill-test|--spirit-strike-test [key]|--speed-test [multiplier]|--camera-speed-test [multiplier]|--check]' \
        '' \
        '  --safe        Build, verify, and launch with every optional hook disabled.' \
        '  --cleanroom   Start Ailish in the shipped testroom with the F8 spawn/despawn menu.' \
        '  --test-arena  Alias for --cleanroom retained for the research checkpoint.' \
        '  --cafu-testroom  Bootstrap Ailish in testroom and ask Sudeki native developer code to spawn Cafu.' \
        '  --trace       Enable normal-speed Quick Menu and observation-only Plasmatica tracing.' \
        '  --input-trace Trace native QuickSkill input and either native Plasmatica activation route.' \
        '  --character-switch-trace  Observe vanilla party rotation, controller target, and old/new AI-mode transition.' \
        '  --party-lifecycle-trace Observe native PC spawn/removal plus resulting party/controller/AI state during a normal save load.' \
        '  --talos-party-test Load a normal save; after the exact Void four-to-one Tal rebuild, restore the other retail party members natively.' \
        '  --zone-transition-trace  Observe door/zone entry, zone loading, and main-world transitions without changing them.' \
        '  --zone-traversal-test  Open the F7 world/interior traversal menu on a normal save.' \
        '  --freeroam-camera-test Require LeftCtrl for mouse-Y distance changes outside combat; retain vanilla combat input.' \
        '  --control-separation-test Toggle Player 2 AI off/on with J through Sudeki native control APIs.' \
        '  --player-input-trace Sample Player 1 world-direction and speed arguments without changing movement.' \
        '  --second-player-movement-test Use F10 to disable Player 2 AI, then move Player 2 with I/J/K/L.' \
        '  --second-player-camera-movement-test Add the native shared-camera basis to Buki movement.' \
        '  --second-player-separation-test Add a 10-unit outward-only separation guard.' \
        '  --shared-group-camera-test Focus Sudeki camera on the P1/Buki midpoint; zoom remains native.' \
        '  --split-screen-render-test Duplicate the finished native gameplay frame into two halves; menus remain full-screen.' \
        '  --second-player-render-camera-test Toggle a render-only party-slot-1 view with F9 while gameplay ownership stays on Player 1.' \
        '  --dual-camera-frame-cache-test Present cached alternating Ailish/Buki frames side by side without render replay.' \
        '  --shared-quit-menu-test Verify the Quit menu replaces both camera halves with one full-width native interface.' \
        '  --viewport-hud-test Verify the right viewport reads Buki HUD data while the left viewport remains Ailish-owned.' \
        '  --dual-camera-local-coop-test Combine dual cameras with F10 AI override and I/J/K/L Buki movement.' \
        '  --controller-bridge-test Drive the first non-front party member, weak attack, and independent camera from a Linux controller.' \
        '  --realtime-skill-coop-test Add guarded P1/P2 native skills and caster-only Plasmatica camera routing.' \
        '  --second-player-target-trace Passively log Buki native target changes while AI is off.' \
        '  --second-player-attack-test Use F10 for Buki AI, I/J/K/L movement, and U for Buki weak attack.' \
        '  --ranged-skill-test  Test guarded native UI-state cycling for Elco/Ailish QuickSkills.' \
        '  --spirit-strike-test Test captured Ailish Spirit Strike ID 2; optionally override the default G key.' \
        '  --speed-test  Reproduce per-model animation control; defaults to 2.0x.' \
        '  --camera-speed-test  Run matched 2.0x caster and camera playback.' \
        '  --check       Build and verify the executable/DLL without launching the game.'
}

case "${mode}" in
    --safe|--cleanroom|--test-arena|--cafu-testroom|--trace|--input-trace|--character-switch-trace|--party-lifecycle-trace|--talos-party-test|--zone-transition-trace|--zone-traversal-test|--freeroam-camera-test|--control-separation-test|--player-input-trace|--second-player-movement-test|--second-player-camera-movement-test|--second-player-separation-test|--shared-group-camera-test|--split-screen-render-test|--second-player-render-camera-test|--dual-camera-frame-cache-test|--shared-quit-menu-test|--viewport-hud-test|--dual-camera-local-coop-test|--controller-bridge-test|--realtime-skill-coop-test|--second-player-target-trace|--second-player-attack-test|--ranged-skill-test|--spirit-strike-test|--speed-test|--camera-speed-test|--check)
        ;;
    --help|-h)
        usage
        exit 0
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

if [[ "${mode}" == "--zone-transition-trace" ||
      "${mode}" == "--zone-traversal-test" ]]; then
    export SUDEKIMP_ZONE_TRACE=1
fi

if [[ "${mode}" == "--speed-test" || "${mode}" == "--camera-speed-test" ]]; then
    if ! awk -v value="${speed}" 'BEGIN {
        exit !(value ~ /^[0-9]+([.][0-9]+)?$/ && value >= 0.25 && value <= 4.0)
    }'; then
        printf 'Invalid speed multiplier: %s (expected 0.25 through 4.0)\n' "${speed}" >&2
        exit 2
    fi
fi
if [[ "${mode}" == "--spirit-strike-test" ]]; then
    if [[ ! "${spirit_key}" =~ ^[A-Za-z0-9]+$ ]] ||
        (( ${#spirit_key} > 31 )); then
        printf 'Invalid test key: %s (use a configured alphanumeric key name)\n' \
            "${spirit_key}" >&2
        exit 2
    fi
fi
if [[ "${mode}" == "--controller-bridge-test" ||
      "${mode}" == "--cleanroom" || "${mode}" == "--test-arena" ||
      "${mode}" == "--cafu-testroom" ]]; then
    if [[ ! "${input_bridge_port}" =~ ^[0-9]+$ ]] ||
        (( input_bridge_port < 1024 || input_bridge_port > 65535 )); then
        printf 'Invalid bridge port: %s (expected 1024 through 65535)\n' \
            "${input_bridge_port}" >&2
        exit 2
    fi
fi

game_launch_args=()
if [[ "${mode}" == "--cleanroom" || "${mode}" == "--test-arena" ||
      "${mode}" == "--cafu-testroom" ]]; then
    game_launch_args+=(
        --game-arg=-Level
        --game-arg=testroom
        --game-arg=-DT
        --game-arg=1
        --game-arg=-Ailish
        --game-arg=1
    )
    if [[ "${mode}" == "--cafu-testroom" ]]; then
        # Sudeki's shipped -Cafu path dereferences a null model payload because
        # hidden item 48 names a hash absent from SOLData's resource index.
        # This private token waits for the item database, preloads the actual
        # indexed W033_CAFUSPISTOL resource, applies the reversible hash
        # correction, switches to Cafu through Sudeki's native party action,
        # and only uses the visual fallback if that load fails.
        game_launch_args+=(--game-arg=-SudekiMPCafuProbe --game-arg=1)
    fi
fi

if pgrep -x SUDEKI.exe >/dev/null; then
    printf '%s\n' 'Sudeki is already running; close it normally before resuming research.' >&2
    exit 1
fi
if [[ ! -f "${game}" ]]; then
    printf 'Working executable is missing: %s\n' "${game}" >&2
    exit 1
fi
if [[ ! -d "${save_root}" ]]; then
    printf 'Research save directory is missing: %s\n' "${save_root}" >&2
    exit 1
fi

save_count="$(find "${save_root}" -mindepth 1 -maxdepth 1 -type d -name 'SAVESLOT*' | wc -l)"
if (( save_count < 11 )); then
    printf 'Expected at least 11 research saves, found %s in %s\n' \
        "${save_count}" "${save_root}" >&2
    exit 1
fi

"${project_dir}/tools/build-linux.sh"

antialiasing_original=""
input_bridge_pid=""
restore_config() {
    if [[ -n "${input_bridge_pid}" ]]; then
        kill "${input_bridge_pid}" 2>/dev/null || true
        wait "${input_bridge_pid}" 2>/dev/null || true
        input_bridge_pid=""
        printf '%s\n' 'Stopped the Linux Player 2 input bridge.'
    fi
    cp -- "${source_config}" "${generated_config}"
    if [[ -n "${antialiasing_original}" ]]; then
        SUDEKIMP_WINEPREFIX="${research_prefix}" \
            "${project_dir}/tools/configure-antialiasing.sh" \
            --set "${antialiasing_original}" || true
        printf 'Restored Sudeki AntiAliasing=%s.\n' \
            "${antialiasing_original}"
    fi
}
trap restore_config EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

case "${mode}" in
    --cleanroom|--test-arena|--cafu-testroom)
        sed -i \
            -e 's/^EnableQuickMenuNormalSpeed=false$/EnableQuickMenuNormalSpeed=true/' \
            -e 's/^EnableCleanroomMenu=false$/EnableCleanroomMenu=true/' \
            -e 's/^EnableCoopRosterMenu=true$/EnableCoopRosterMenu=false/' \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            -e 's/^EnableSecondPlayerMovementPrototype=false$/EnableSecondPlayerMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraRelativeMovementPrototype=false$/EnableSecondPlayerCameraRelativeMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerSeparationGuardPrototype=false$/EnableSecondPlayerSeparationGuardPrototype=true/' \
            -e 's/^EnableSecondPlayerWeakAttackPrototype=false$/EnableSecondPlayerWeakAttackPrototype=true/' \
            -e 's/^EnableExternalInputBridgePrototype=false$/EnableExternalInputBridgePrototype=true/' \
            -e "s/^InputBridgePort=.*$/InputBridgePort=${input_bridge_port}/" \
            -e 's/^EnableSplitScreenRenderPrototype=false$/EnableSplitScreenRenderPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraPrototype=false$/EnableSecondPlayerCameraPrototype=true/' \
            -e 's/^EnableDualCameraFrameCachePrototype=false$/EnableDualCameraFrameCachePrototype=true/' \
            -e 's/^EnableSecondPlayerControllerCameraPrototype=false$/EnableSecondPlayerControllerCameraPrototype=true/' \
            -e 's/^EnableSplitScreenRangedModelIsolationPrototype=false$/EnableSplitScreenRangedModelIsolationPrototype=true/' \
            -e 's/^EnableSpiritStrikeViewportEffectIsolationPrototype=false$/EnableSpiritStrikeViewportEffectIsolationPrototype=true/' \
            -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
            "${generated_config}"
        ;;
    --trace)
        sed -i \
            -e 's/^EnableQuickMenuNormalSpeed=false$/EnableQuickMenuNormalSpeed=true/' \
            -e 's/^EnablePlasmaticaTrace=false$/EnablePlasmaticaTrace=true/' \
            "${generated_config}"
        ;;
    --zone-traversal-test)
        sed -i \
            -e 's/^EnableCoopRosterMenu=true$/EnableCoopRosterMenu=false/' \
            -e 's/^EnableZoneTraversalMenu=false$/EnableZoneTraversalMenu=true/' \
            -e 's/^ToggleZoneTraversalMenu=F7$/ToggleZoneTraversalMenu=F7/' \
            "${generated_config}"
        ;;
    --input-trace)
        sed -i \
            -e 's/^EnableQuickMenuNormalSpeed=false$/EnableQuickMenuNormalSpeed=true/' \
            -e 's/^EnablePlasmaticaTrace=false$/EnablePlasmaticaTrace=true/' \
            -e 's/^EnableQuickSkillInputTrace=false$/EnableQuickSkillInputTrace=true/' \
            "${generated_config}"
        ;;
    --character-switch-trace|--party-lifecycle-trace|--talos-party-test)
        sed -i \
            -e 's/^EnableCharacterSwitchTrace=false$/EnableCharacterSwitchTrace=true/' \
            "${generated_config}"
        if [[ "${mode}" == "--talos-party-test" ]]; then
            sed -i \
                -e 's/^EnableTalosPartyPrototype=false$/EnableTalosPartyPrototype=true/' \
                "${generated_config}"
        fi
        ;;
    --freeroam-camera-test)
        sed -i \
            -e 's/^EnableFreeRoamCameraModifierPrototype=false$/EnableFreeRoamCameraModifierPrototype=true/' \
            "${generated_config}"
        ;;
    --control-separation-test)
        sed -i \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            "${generated_config}"
        ;;
    --player-input-trace)
        sed -i \
            -e 's/^EnablePlayerMovementTrace=false$/EnablePlayerMovementTrace=true/' \
            "${generated_config}"
        ;;
    --second-player-movement-test)
        sed -i \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            -e 's/^EnableSecondPlayerMovementPrototype=false$/EnableSecondPlayerMovementPrototype=true/' \
            -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
            "${generated_config}"
        ;;
    --second-player-camera-movement-test)
        sed -i \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            -e 's/^EnableSecondPlayerMovementPrototype=false$/EnableSecondPlayerMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraRelativeMovementPrototype=false$/EnableSecondPlayerCameraRelativeMovementPrototype=true/' \
            -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
            "${generated_config}"
        ;;
    --second-player-separation-test)
        sed -i \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            -e 's/^EnableSecondPlayerMovementPrototype=false$/EnableSecondPlayerMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraRelativeMovementPrototype=false$/EnableSecondPlayerCameraRelativeMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerSeparationGuardPrototype=false$/EnableSecondPlayerSeparationGuardPrototype=true/' \
            -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
            "${generated_config}"
        ;;
    --shared-group-camera-test)
        sed -i \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            -e 's/^EnableSecondPlayerMovementPrototype=false$/EnableSecondPlayerMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraRelativeMovementPrototype=false$/EnableSecondPlayerCameraRelativeMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerSeparationGuardPrototype=false$/EnableSecondPlayerSeparationGuardPrototype=true/' \
            -e 's/^EnableSharedGroupCameraPrototype=false$/EnableSharedGroupCameraPrototype=true/' \
            -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
            "${generated_config}"
        ;;
    --split-screen-render-test)
        sed -i \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            -e 's/^EnableSecondPlayerMovementPrototype=false$/EnableSecondPlayerMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraRelativeMovementPrototype=false$/EnableSecondPlayerCameraRelativeMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerSeparationGuardPrototype=false$/EnableSecondPlayerSeparationGuardPrototype=true/' \
            -e 's/^EnableSplitScreenRenderPrototype=false$/EnableSplitScreenRenderPrototype=true/' \
            -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
            "${generated_config}"
        ;;
    --second-player-render-camera-test)
        sed -i \
            -e 's/^EnableSplitScreenRenderPrototype=false$/EnableSplitScreenRenderPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraPrototype=false$/EnableSecondPlayerCameraPrototype=true/' \
            "${generated_config}"
        ;;
    --dual-camera-frame-cache-test)
        sed -i \
            -e 's/^EnableSplitScreenRenderPrototype=false$/EnableSplitScreenRenderPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraPrototype=false$/EnableSecondPlayerCameraPrototype=true/' \
            -e 's/^EnableDualCameraFrameCachePrototype=false$/EnableDualCameraFrameCachePrototype=true/' \
            "${generated_config}"
        ;;
    --shared-quit-menu-test|--viewport-hud-test)
        sed -i \
            -e 's/^EnableSplitScreenRenderPrototype=false$/EnableSplitScreenRenderPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraPrototype=false$/EnableSecondPlayerCameraPrototype=true/' \
            -e 's/^EnableDualCameraFrameCachePrototype=false$/EnableDualCameraFrameCachePrototype=true/' \
            "${generated_config}"
        ;;
    --dual-camera-local-coop-test)
        sed -i \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            -e 's/^EnableSecondPlayerMovementPrototype=false$/EnableSecondPlayerMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraRelativeMovementPrototype=false$/EnableSecondPlayerCameraRelativeMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerSeparationGuardPrototype=false$/EnableSecondPlayerSeparationGuardPrototype=true/' \
            -e 's/^EnableSplitScreenRenderPrototype=false$/EnableSplitScreenRenderPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraPrototype=false$/EnableSecondPlayerCameraPrototype=true/' \
            -e 's/^EnableDualCameraFrameCachePrototype=false$/EnableDualCameraFrameCachePrototype=true/' \
            -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
            "${generated_config}"
        ;;
    --controller-bridge-test)
        sed -i \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            -e 's/^EnableSecondPlayerMovementPrototype=false$/EnableSecondPlayerMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraRelativeMovementPrototype=false$/EnableSecondPlayerCameraRelativeMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerSeparationGuardPrototype=false$/EnableSecondPlayerSeparationGuardPrototype=true/' \
            -e 's/^EnableSecondPlayerWeakAttackPrototype=false$/EnableSecondPlayerWeakAttackPrototype=true/' \
            -e 's/^EnableExternalInputBridgePrototype=false$/EnableExternalInputBridgePrototype=true/' \
            -e "s/^InputBridgePort=.*$/InputBridgePort=${input_bridge_port}/" \
            -e 's/^EnableSplitScreenRenderPrototype=false$/EnableSplitScreenRenderPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraPrototype=false$/EnableSecondPlayerCameraPrototype=true/' \
            -e 's/^EnableDualCameraFrameCachePrototype=false$/EnableDualCameraFrameCachePrototype=true/' \
            -e 's/^EnableSecondPlayerControllerCameraPrototype=false$/EnableSecondPlayerControllerCameraPrototype=true/' \
            -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
            "${generated_config}"
        ;;
    --realtime-skill-coop-test)
        sed -i \
            -e 's/^EnableQuickMenuNormalSpeed=false$/EnableQuickMenuNormalSpeed=true/' \
            -e 's/^EnablePlasmaticaTrace=false$/EnablePlasmaticaTrace=true/' \
            -e 's/^EnableQuickSkillInputTrace=false$/EnableQuickSkillInputTrace=true/' \
            -e 's/^EnableRangedQuickSkillPrototype=false$/EnableRangedQuickSkillPrototype=true/' \
            -e 's/^EnableRealtimeMultiplayerSkillCombatPrototype=false$/EnableRealtimeMultiplayerSkillCombatPrototype=true/' \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            -e 's/^EnableSecondPlayerMovementPrototype=false$/EnableSecondPlayerMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraRelativeMovementPrototype=false$/EnableSecondPlayerCameraRelativeMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerSeparationGuardPrototype=false$/EnableSecondPlayerSeparationGuardPrototype=true/' \
            -e 's/^EnableSecondPlayerWeakAttackPrototype=false$/EnableSecondPlayerWeakAttackPrototype=true/' \
            -e 's/^EnableSplitScreenRenderPrototype=false$/EnableSplitScreenRenderPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraPrototype=false$/EnableSecondPlayerCameraPrototype=true/' \
            -e 's/^EnableDualCameraFrameCachePrototype=false$/EnableDualCameraFrameCachePrototype=true/' \
            -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
            "${generated_config}"
        ;;
    --second-player-target-trace)
        sed -i \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            -e 's/^EnableSecondPlayerTargetTrace=false$/EnableSecondPlayerTargetTrace=true/' \
            -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
            "${generated_config}"
        ;;
    --second-player-attack-test)
        sed -i \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            -e 's/^EnableSecondPlayerMovementPrototype=false$/EnableSecondPlayerMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerWeakAttackPrototype=false$/EnableSecondPlayerWeakAttackPrototype=true/' \
            -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
            "${generated_config}"
        ;;
    --ranged-skill-test)
        sed -i \
            -e 's/^EnableQuickMenuNormalSpeed=false$/EnableQuickMenuNormalSpeed=true/' \
            -e 's/^EnablePlasmaticaTrace=false$/EnablePlasmaticaTrace=true/' \
            -e 's/^EnableQuickSkillInputTrace=false$/EnableQuickSkillInputTrace=true/' \
            -e 's/^EnableRangedQuickSkillPrototype=false$/EnableRangedQuickSkillPrototype=true/' \
            "${generated_config}"
        ;;
    --spirit-strike-test)
        sed -i \
            -e 's/^EnableQuickMenuNormalSpeed=false$/EnableQuickMenuNormalSpeed=true/' \
            -e 's/^EnableQuickSkillInputTrace=false$/EnableQuickSkillInputTrace=true/' \
            -e 's/^EnableRangedQuickSkillPrototype=false$/EnableRangedQuickSkillPrototype=true/' \
            -e 's/^EnableDirectSpiritStrikePrototype=false$/EnableDirectSpiritStrikePrototype=true/' \
            -e "s/^SpiritStrike=.*$/SpiritStrike=${spirit_key}/" \
            "${generated_config}"
        ;;
    --speed-test)
        sed -i \
            -e 's/^EnableQuickMenuNormalSpeed=false$/EnableQuickMenuNormalSpeed=true/' \
            -e 's/^EnablePlasmaticaTrace=false$/EnablePlasmaticaTrace=true/' \
            -e 's/^EnablePlasmaticaAnimationSpeed=false$/EnablePlasmaticaAnimationSpeed=true/' \
            -e "s/^PlasmaticaAnimationSpeed=.*$/PlasmaticaAnimationSpeed=${speed}/" \
            "${generated_config}"
        ;;
    --camera-speed-test)
        sed -i \
            -e 's/^EnableQuickMenuNormalSpeed=false$/EnableQuickMenuNormalSpeed=true/' \
            -e 's/^EnablePlasmaticaTrace=false$/EnablePlasmaticaTrace=true/' \
            -e 's/^EnablePlasmaticaAnimationSpeed=false$/EnablePlasmaticaAnimationSpeed=true/' \
            -e "s/^PlasmaticaAnimationSpeed=.*$/PlasmaticaAnimationSpeed=${speed}/" \
            -e 's/^EnablePlasmaticaCameraSpeed=false$/EnablePlasmaticaCameraSpeed=true/' \
            -e "s/^PlasmaticaCameraSpeed=.*$/PlasmaticaCameraSpeed=${speed}/" \
            "${generated_config}"
        ;;
esac

SUDEKIMP_WINEPREFIX="${research_prefix}" \
    "${project_dir}/tools/run-wine.sh" --check "${game}"

if [[ "${mode}" == "--split-screen-render-test" ||
      "${mode}" == "--cleanroom" ||
      "${mode}" == "--test-arena" ||
      "${mode}" == "--cafu-testroom" ||
      "${mode}" == "--second-player-render-camera-test" ||
      "${mode}" == "--dual-camera-frame-cache-test" ||
      "${mode}" == "--shared-quit-menu-test" ||
      "${mode}" == "--viewport-hud-test" ||
      "${mode}" == "--dual-camera-local-coop-test" ||
      "${mode}" == "--controller-bridge-test" ||
      "${mode}" == "--realtime-skill-coop-test" ]]; then
    antialiasing_original="$(SUDEKIMP_WINEPREFIX="${research_prefix}" \
        "${project_dir}/tools/configure-antialiasing.sh" --get)"
    SUDEKIMP_WINEPREFIX="${research_prefix}" \
        "${project_dir}/tools/configure-antialiasing.sh" --set 0
    printf 'Temporarily changed Sudeki AntiAliasing: %s -> 0 for compositor compatibility.\n' \
        "${antialiasing_original}"
    printf '%s\n' \
        'The original value will be restored automatically when this run exits.'
fi

printf '%s\n' \
    '' \
    'SudekiMP research checkpoint:' \
    '  Milestone 1 complete: Quick Menu can remain open at normal world speed.' \
    '  Milestone 2 complete: Plasmatica caster and cinematic camera have independent rates.' \
    '  Confirmed presentation: matched 2.0x rates preserve all four camera angles.' \
    '  Native damage resolved: collision bridge -> RVA 0x00138870 -> RVA 0x000DAB50 -> RVA 0x000D21D0 HP write.' \
    '  Corrected false lead: RVA 0x000DCD00 -> RVA 0x00018B90 is positional impact SFX, not damage.' \
    '  Caster protection resolved: generic CSkill lifetime increments the arbiter invulnerability refcount.' \
    '  Target retention resolved: both skill and ordinary target pointers are cleared before missile launch.' \
    '  Aim handoff resolved: standard Plasmatica launches along Elco committed actor-forward vector.' \
    '  Recovery resolved: camera, auto targeting, animation, rumble, control filter, and native skill state restore in order.' \
    '  Phase 5 complete: one Skill Strike is traced end to end with independent caster/camera timing.' \
    '  Phase 6: Elco top-row 5-9 direct skills are user-confirmed; 0 and Ailish remain explicit checks.' \
    '  Spirit Strike: direct G -> captured Ailish ID 2 completed normally through native validation and activation.' \
    '  Re-entry guard: repeated G presses during the move were rejected by the native validator.' \
    '  Consumables: native top-row 1-4 remain functional and require no bypass.' \
    '  Configurable input: live H override confirmed for Spirit Strike.' \
    '  Phase 8 mechanism confirmed: vanilla transfers nested AI mode 0/1 with the controller target.' \
    '  Milestone 4 prototype: J uses native AiOverrideControl/AiDefaultControl on non-front Buki.' \
    '  Milestone 5 proof: Player 1 and AI-overridden Buki accepted separate simultaneous movement input.' \
    '  Phase 9 attack proof: Buki accepted independent U weak attacks and entered native IsAttacking state.' \
    '  Shared-camera proof: Buki movement now follows Player 1 camera orientation.' \
    '  Retained targeting proof: Buki target node and auto-target state survive the AI override.' \
    '  Separation proof: the 10-unit guard blocks only outward Buki movement and releases inward movement.' \
    '  Camera proof: the disabled native MatrixTarget prototype follows the P1/Buki midpoint and restores native P1 focus cleanly.' \
    '  Split-screen proof: dual viewports work; current test composites the untouched finished native frame before independent cameras.' \
    '  Deferred: full no-menu encounter remains required before Milestone 3 closes.' \
    '  Emergency stop: tools/stop-sudeki.sh' \
    '  Detailed handoff: docs/research-log.md and docs/combat.md.' \
    "  Research saves: ${save_count}" \
    "  Mode: ${mode}"

if [[ "${mode}" == "--spirit-strike-test" ]]; then
    printf '  Spirit Strike test key: %s\n' "${spirit_key}"
fi
if [[ "${mode}" == "--cleanroom" || "${mode}" == "--test-arena" ||
      "${mode}" == "--cafu-testroom" ]]; then
    printf '%s\n' \
        '  Native test arena: Sudeki received -Level testroom through its shipped startup-test path.' \
        '  Native -DT/-Ailish flags spawn Ailish as the cleanroom lead.' \
        '  Press F8 for party/dummy, combat/camera, and infinite-resource controls.' \
        '  Spawn one additional party member, then toggle SPLIT SCREEN P2 in F8.' \
        '  The right-side badge reads P2 READY only while the Razer bridge is supplying live input.' \
        '  Pure Land test: cast once as Ailish while moving Tal with the controller.' \
        '  Tal must remain live; the Spirit Strike must finish instead of stalling.' \
        '  Use Up/Down and Enter; Escape or F8 closes the overlay.' \
        '  Ailish is lead-locked and cannot be despawned by this menu.'
    if [[ "${mode}" == "--cafu-testroom" ]]; then
        printf '%s\n' \
            '  Cafu probe: hidden item 48 is preserved; its stale W033 hash is corrected to the verified archive entry before spawn.' \
            '  Cafu control: after spawn, the native Next-character action makes Cafu Player 1 for direct movement/fire testing.' \
            '  Cafu fallback: the reversible Elco-pistol visual alias is used only if the actual W033 resource cannot load.'
    fi
fi
if [[ "${mode}" == "--control-separation-test" ]]; then
    printf '%s\n' \
        '  Test: control anyone except Buki, press J once, and check whether Buki stands still.' \
        '  Press J a second time to restore Buki AI before ending the run.' \
        '  Do not switch characters while the override is active.'
fi
if [[ "${mode}" == "--player-input-trace" ]]; then
    printf '%s\n' \
        '  Test: in normal third-person gameplay, move with W/A/S/D for a few seconds.' \
        '  The tracer samples unchanged arbiter movement arguments at 10 Hz.'
fi
if [[ "${mode}" == "--second-player-movement-test" ]]; then
    printf '%s\n' \
        '  Test: control anyone except Buki, press F10 once to disable Buki AI,' \
        '  then move Buki with I/J/K/L while Player 1 retains W/A/S/D.' \
        '  I/J/K/L directions use fixed world axes for this first proof.' \
        '  Do not switch characters; press F10 again to restore Buki AI before exit.'
fi
if [[ "${mode}" == "--second-player-camera-movement-test" ]]; then
    printf '%s\n' \
        '  Test: control anyone except Buki, press F10 once to disable Buki AI,' \
        '  then rotate the shared camera and move Buki with I/J/K/L.' \
        '  Buki should follow the same camera-relative basis as Player 1.' \
        '  Do not switch characters; press F10 again to restore Buki AI before exit.'
fi
if [[ "${mode}" == "--second-player-separation-test" ]]; then
    printf '%s\n' \
        '  Test: control anyone except Buki, press F10 once to disable Buki AI,' \
        '  then move Buki with I/J/K/L relative to the shared camera.' \
        '  At 10 horizontal units, outward movement should stop while inward movement remains available.' \
        '  No teleport or forced catch-up is used; press F10 again to restore Buki AI before exit.'
fi
if [[ "${mode}" == "--shared-group-camera-test" ]]; then
    printf '%s\n' \
        '  Test: control anyone except Buki, press F10 once to disable Buki AI,' \
        '  then move both characters and rotate the camera.' \
        '  Camera focus should track the midpoint; distance/zoom remains native in this first proof.' \
        '  The 10-unit outward guard remains active; press F10 again to restore native AI and focus.'
fi
if [[ "${mode}" == "--split-screen-render-test" ]]; then
    printf '%s\n' \
        '  Test 1: confirm the title/main menu stays one normal full-screen view.' \
        '  Load gameplay, then confirm the same live 3D view appears in both left and right halves.' \
        '  Check that black shadow figures/regions are gone and doors appear in both halves.' \
        '  Player/NPC movement must remain normal; shadows and doors should match exactly in both halves.' \
        '  HUD ownership remains intentionally unchanged; this proof isolates safe frame composition.' \
        '  Test 2: control anyone except Buki, press F10, and move Buki with I/J/K/L.' \
        '  Both halves should still show the same Player 1 camera; independent cameras are the next step.' \
        '  Press F10 again to restore Buki AI before exit.'
fi
if [[ "${mode}" == "--second-player-render-camera-test" ]]; then
    printf '%s\n' \
        '  Test: title/menu should remain full-width; load the two-character Ailish/Buki save.' \
        '  Both compositor halves initially show the original Player 1/Ailish camera.' \
        '  Tap F9 once: both diagnostic halves should shift to a Buki-centered view while retaining Player 1 orientation/distance.' \
        '  Player 1/Ailish must remain the gameplay-owned camera. Move Ailish and enter the castle doorway while F9 is active.' \
        '  The doorway transition and ordinary simulation must complete; a skybox view or held transition is a failure.' \
        '  Tap F9 again: both halves must return cleanly to the original Player 1/Ailish framing.' \
        '  This tests render-only camera-state isolation—not simultaneous views or independent Player 2 rotation/zoom yet.'
fi
if [[ "${mode}" == "--dual-camera-frame-cache-test" ]]; then
    printf '%s\n' \
        '  Test: title/main menu should remain one full-width view; load the Ailish/Buki save.' \
        '  After both clean caches initialize, the left half should follow Ailish and the right half should center on Buki.' \
        '  No key is required. Move Ailish, let Buki move, and rotate the native camera to check that both halves update.' \
        '  Each camera updates every other engine frame in this diagnostic; report visible judder, latency, or stale frames.' \
        '  Check shadows, doors, NPC/player motion, and the castle doorway transition for regressions.' \
        '  The duplicated Ailish-owned HUD is expected. Do not assess pause/exit-menu takeover in this pass.'
fi
if [[ "${mode}" == "--shared-quit-menu-test" ]]; then
    printf '%s\n' \
        '  Focused UI test: load the Ailish/Buki save and wait for the distinct left/right views.' \
        '  Open the Sudeki Quit menu. The split must disappear and one normal full-width native menu must cover the screen.' \
        '  Select Back. The native full-width frame may appear briefly while fresh camera caches initialize.' \
        '  Ailish-left/Buki-right must then resume automatically with no stale menu, swapped view, or frozen control.' \
        '  Repeat once. Do not choose Exit to Windows or Quit to Title Screen during this focused test.'
fi
if [[ "${mode}" == "--viewport-hud-test" ]]; then
    printf '%s\n' \
        '  Load the Ailish/Buki save and wait for the two camera halves.' \
        '  Left: Ailish must remain the large portrait/name/HP/SP, with Buki as the small companion.' \
        '  Right: Buki must become the large portrait/name/HP/SP, with Ailish as the small companion.' \
        '  Watch for portrait flicker, wrong art, any brief unsplit frame, or the minimap pulsing to full-screen size.' \
        '  Open Quit once to ensure the full-width frozen dual-camera backdrop still works, then select Back.'
fi
if [[ "${mode}" == "--dual-camera-local-coop-test" ]]; then
    printf '%s\n' \
        '  Integrated test: load the Ailish/Buki save and confirm Ailish-left/Buki-right views initialize.' \
        '  Press F10 once to disable Buki AI. Player 1 keeps W/A/S/D; move Buki independently with I/J/K/L.' \
        '  Each half should remain centered on its assigned character while both characters move at the same time.' \
        '  Mouse camera rotation/zoom remains shared in this pass; Player 2 does not yet own separate camera input.' \
        '  The 10-unit outward-only separation guard is active. Move inward to release it immediately.' \
        '  Check for view swapping, stale frames, geometry/shadow defects, or control loss.' \
        '  Press F10 again to restore Buki AI before exiting.'
fi
if [[ "${mode}" == "--controller-bridge-test" ]]; then
    printf '%s\n' \
        '  Controller bridge test: load any two-character save (including Tal/Ailish) and wait for both cameras.' \
        '  Player 1 is the front/controller-owned character; Player 2 is the first non-front party member.' \
        '  Press F10 once to disable Player 2 AI. Player 1 remains keyboard/mouse.' \
        '  Move Player 2 with the controller left stick and tap A for the selected character weak attack.' \
        '  Rotate only Player 2/right viewport with the controller right stick; Player 1 mouse should affect only the left viewport.' \
        '  After rotating Camera 2, left-stick forward must follow its forward direction. The 10-unit guard remains active.' \
        '  Unplugging the pad or stopping the helper neutralizes Player 2 input within 250 ms.' \
        '  Press F10 again to restore AI on the exact overridden character before exiting.' \
        "  Linux input device: ${input_device}"
fi
if [[ "${mode}" == "--realtime-skill-coop-test" ]]; then
    printf '%s\n' \
        '  Integrated skill test: load a two-character save, put Elco in the Player 1/front slot, and press F10 to disable Buki AI.' \
        '  Player 1: native 5-8 Skill Strikes. Player 2: F1-F4 Skill Strikes, U weak attack, I/J/K/L movement.' \
        '  Hold Plasmatica targeting while Player 2 moves/attacks; enemies, AI, and projectiles must remain at normal speed.' \
        '  Confirm Plasmatica: only Elco viewport takes the authored camera; Buki viewport remains live and shows Elco casting in-world.' \
        '  Ranged models remain native for their own viewport; the other viewport borrows the saved world body plus copied native animation selection/state/time/rate/blend only during rendering.' \
        '  Try a Player 2 skill during P1 targeting: it must be rejected cleanly. Try again after confirmation: native executions may overlap.' \
        '  Animation and camera multipliers remain native 1.0x. Press F10 again before exit to restore Buki AI.'
fi
if [[ "${mode}" == "--second-player-target-trace" ]]; then
    printf '%s\n' \
        '  Test: control anyone except Buki, enter combat, and press F10 once to disable Buki AI.' \
        '  Move around enemies without attacking first, then use normal Player 1 combat.' \
        '  The trace logs only Buki target-node/auto-target changes at up to 10 Hz.' \
        '  It never assigns a target; press F10 again to restore Buki AI before exit.'
fi
if [[ "${mode}" == "--second-player-attack-test" ]]; then
    printf '%s\n' \
        '  Test: control anyone except Buki, press F10 once to disable Buki AI,' \
        '  then use I/J/K/L for Buki movement and tap U for Buki weak attack.' \
        '  Player 1 retains normal controls; Buki targeting remains native.' \
        '  Do not switch characters; press F10 again to restore Buki AI before exit.'
fi
if [[ "${mode}" == "--freeroam-camera-test" ]]; then
    printf '%s\n' \
        '  Test outside combat: mouse forward/back should do nothing until LeftCtrl is held.' \
        '  Hold LeftCtrl and move the mouse forward/back; native distance control should resume.' \
        '  Mouse left/right rotation and all combat input remain vanilla; limits and pitch are unchanged.'
fi
if [[ "${mode}" == "--zone-transition-trace" ]]; then
    printf '%s\n' \
        '  Observation-only trace: load the Tal save and approach a named door.' \
        '  Press Enter once at the door. The log records EnterZone/SwitchZoneNOW/LoadZone and CWorld::SwitchMainZone before/after.' \
        '  No teleport, combat, door, or level state is changed by this pass.'
fi
if [[ "${mode}" == "--zone-traversal-test" ]]; then
    printf '%s\n' \
        '  F7 opens the world-aware traversal menu.' \
        '  Worlds use native default-entry transitions.' \
        '  Right opens temporary interiors for the active world only.' \
        '  Enter performs the selected authored transition; Left returns to worlds.' \
        '  Cross-world interior jumps are rejected.'
fi

if [[ "${mode}" == "--check" ]]; then
    exit 0
fi

if [[ "${mode}" == "--controller-bridge-test" ||
      "${mode}" == "--cleanroom" || "${mode}" == "--test-arena" ||
      "${mode}" == "--cafu-testroom" ]]; then
    if [[ ! -r "${input_device}" ]]; then
        printf 'Controller device is not readable: %s\n' "${input_device}" >&2
        printf '%s\n' \
            'The cleanroom can still launch; P2 will remain WAITING until the Razer bridge is restarted.' >&2
        if [[ "${mode}" == "--controller-bridge-test" ]]; then
            exit 1
        fi
    else
        : >"${input_bridge_log}"
        "${input_bridge_helper}" \
            --device "${input_device}" \
            --port "${input_bridge_port}" \
            >"${input_bridge_log}" 2>&1 &
        input_bridge_pid=$!
        sleep 0.2
        if ! kill -0 "${input_bridge_pid}" 2>/dev/null; then
            wait "${input_bridge_pid}" || true
            input_bridge_pid=""
            printf '%s\n' 'The Linux input bridge failed to start:' >&2
            sed -n '1,80p' "${input_bridge_log}" >&2
            if [[ "${mode}" == "--controller-bridge-test" ]]; then
                exit 1
            fi
        else
            printf 'Linux Player 2 input bridge started (PID %s, log %s).\n' \
                "${input_bridge_pid}" "${input_bridge_log}"
        fi
    fi
fi

run_wine_args=(--windowed)
if [[ "${SUDEKIMP_DISABLE_OBS_GAMECAPTURE:-false}" != "true" ]]; then
    run_wine_args+=(--obs-gamecapture)
fi
SUDEKIMP_WINEPREFIX="${research_prefix}" \
    "${project_dir}/tools/run-wine.sh" "${run_wine_args[@]}" \
    "${game_launch_args[@]}" "${game}"
