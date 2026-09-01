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
input_device_p3="${SUDEKIMP_INPUT_DEVICE_P3:-/dev/input/js1}"
input_bridge_port="${SUDEKIMP_INPUT_BRIDGE_PORT:-26760}"
input_bridge_helper="${project_dir}/build/linux/bin/sudekimp-input-bridge"
input_bridge_log="${project_dir}/build/linux/input-bridge.log"
input_bridge_p2_log="${project_dir}/build/linux/input-bridge-p2.log"
input_bridge_p3_log="${project_dir}/build/linux/input-bridge-p3.log"
lan_arena_host="${SUDEKIMP_LAN_ARENA_HOST:-127.0.0.1}"
lan_arena_port="${SUDEKIMP_LAN_ARENA_PORT:-26770}"
supported_game_sha256='8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94'
supported_world_sha256='e36a5974f9aedea5b5b428fe2445cf496c52911ff01d4934ea8ab8124abf1ff9'

usage() {
    printf '%s\n' \
        'usage: tools/continue-research.sh [--safe|--cleanroom|--test-arena|--cafu-testroom|--lan-arena-host|--lan-arena-client|--trace|--input-trace|--character-switch-trace|--party-lifecycle-trace|--door-transition-trace|--merchant-checkout-trace|--native-p2-camera-collision-test|--talos-party-test|--talos-lifecycle-observation|--talos-staging-observation|--talos-post-movie-party-test|--talos-post-movie-dual-camera-test|--talos-defense-trace|--zone-transition-trace|--zone-traversal-test|--freeroam-camera-test|--control-separation-test|--player-input-trace|--second-player-movement-test|--second-player-camera-movement-test|--second-player-separation-test|--shared-group-camera-test|--split-screen-render-test|--second-player-render-camera-test|--dual-camera-frame-cache-test|--shared-quit-menu-test|--viewport-hud-test|--dual-camera-local-coop-test|--controller-bridge-test|--three-seat-input-transport-test|--three-player-local-coop-test|--realtime-skill-coop-test|--second-player-target-trace|--second-player-attack-test|--ranged-skill-test|--spirit-strike-test [key]|--speed-test [multiplier]|--camera-speed-test [multiplier]|--check]' \
        '' \
        '  --safe        Build, verify, and launch with every optional hook disabled.' \
        '  --cleanroom   Start Ailish in the shipped testroom with the F8 spawn/despawn menu.' \
        '  --test-arena  Alias for --cleanroom retained for the research checkpoint.' \
        '  --cafu-testroom  Bootstrap Ailish in testroom and ask Sudeki native developer code to spawn Cafu.' \
        '  --lan-arena-host Host the cleanroom LAN arena as Tal (direct IPv4 UDP).' \
        '  --lan-arena-client Join the cleanroom LAN arena as Ailish (set SUDEKIMP_LAN_ARENA_HOST and optional _PORT).' \
        '  --trace       Enable normal-speed Quick Menu and observation-only Plasmatica tracing.' \
        '  --input-trace Trace native QuickSkill input and either native Plasmatica activation route.' \
        '  --character-switch-trace  Observe vanilla party rotation, controller target, and old/new AI-mode transition.' \
        '  --party-lifecycle-trace Exercise roster handoff, party-atomic rooms, the visible roaming boundary, and controller routing. General P2 interaction provenance, custom blacksmith, forge commits, and voting experiments stay disabled.' \
        '  --door-transition-trace Run the co-op profile with passive validated P1 door/OnAction/SOL/temporary-zone tracing. It never opens a vote, delays, or replays a door.' \
        '  --merchant-checkout-trace Run the co-op profile plus passive P1 ShopStart/merchant observation. It never changes shop, inventory, or money.' \
        '  --native-p2-camera-collision-test Add the focused native Exploration camera/obstruction experiment to the complete Tal=P1/Ailish=P2 party-lifecycle profile.' \
        '  --talos-party-test RETIRED: its PC_KAZEL-spawn trigger ran before exact Kazel deletion and TSA settle; use the corrected post-movie mode.' \
        '  --talos-lifecycle-observation Exact-image, one-human, native-passthrough observation of the retail pre-Void lifecycle. It never preserves companions or enables the expanded fight.' \
        '  --talos-staging-observation Exact-image, one-human, read-only capture of a settled ordinary-world four-hero frame. It makes no membership call and never enters the Void.' \
        '  --talos-post-movie-party-test Exact-image closed test: let retail delete the companions and Kazel, then restore Ailish/Buki/Elco after the same-session TSA settle; Ailish is Player 2.' \
        '  --talos-post-movie-dual-camera-test Add split rendering, an independently rotatable Player 2 camera, dual frame caching, and view-relative P2 navigation to the exact post-movie four-hero restore.' \
        '  --talos-defense-trace Trace real-boss damage, invulnerability, reaction IDs, and knockback sessions without restoring companions.' \
        '  --zone-transition-trace  Observe door/zone entry, zone loading, and main-world transitions without changing them.' \
        '  --zone-traversal-test  Open the F7 world/interior traversal menu on a normal save.' \
        '  --freeroam-camera-test Require LeftCtrl for mouse-Y distance changes outside combat; retain vanilla combat input.' \
        '  --control-separation-test Toggle Player 2 AI off/on with J through Sudeki native control APIs.' \
        '  --player-input-trace Sample Player 1 world-direction and speed arguments without changing movement.' \
        '  --second-player-movement-test Use F10 to disable Player 2 AI, then move Player 2 with I/J/K/L.' \
        '  --second-player-camera-movement-test Add the native shared-camera basis to Buki movement.' \
        '  --second-player-separation-test Add the visible symmetric 10-unit roaming boundary to dual cameras.' \
        '  --shared-group-camera-test Focus Sudeki camera on the P1/Buki midpoint; zoom remains native.' \
        '  --split-screen-render-test Duplicate the finished native gameplay frame into two halves; menus remain full-screen.' \
        '  --second-player-render-camera-test Toggle a render-only party-slot-1 view with F9 while gameplay ownership stays on Player 1.' \
        '  --dual-camera-frame-cache-test Present cached alternating Ailish/Buki frames side by side without render replay.' \
        '  --shared-quit-menu-test Verify the Quit menu replaces both camera halves with one full-width native interface.' \
        '  --viewport-hud-test Verify the right viewport reads Buki HUD data while the left viewport remains Ailish-owned.' \
        '  --dual-camera-local-coop-test Combine dual cameras with F10 AI override and I/J/K/L Buki movement.' \
        '  --controller-bridge-test Drive the first non-front party member, weak attack, and independent camera from a Linux controller.' \
        '  --three-seat-input-transport-test Start distinct Linux bridges for P2 and P3 and require the closed LocalInputHub 0x07 transport bank. P3 actor, camera, HUD, and gameplay ownership are not enabled.' \
        '  --three-player-local-coop-test Run the closed three-player roster, control, camera, frame-cache, and fixed js0/js1 UDP profile. One serialized Skills menu stays pinned to its P1/P2/P3 viewport; P4 remains fail-closed.' \
        '  --realtime-skill-coop-test Add guarded P1/P2 native skills and caster-only Plasmatica camera routing.' \
        '  --second-player-target-trace Passively log Buki native target changes while AI is off.' \
        '  --second-player-attack-test Use F10 for Buki AI, I/J/K/L movement, and U for Buki weak attack.' \
        '  --ranged-skill-test  Test guarded native UI-state cycling for Elco/Ailish QuickSkills.' \
        '  --spirit-strike-test Test captured Ailish Spirit Strike ID 2; optionally override the default G key.' \
        '  --speed-test  Reproduce per-model animation control; defaults to 2.0x.' \
        '  --camera-speed-test  Run matched 2.0x caster and camera playback.' \
        '  --check       Build and verify the executable/DLL without launching the game.'
}

if [[ "${mode}" == "--talos-party-test" ]]; then
    printf '%s\n' \
        'The old Talos party test is retired.' \
        'It armed on the authored PC_KAZEL spawn before exact Kazel deletion and TSA settle.' \
        'Use --talos-post-movie-party-test for the corrected exact post-movie boundary.' >&2
    exit 2
fi

case "${mode}" in
    --safe|--cleanroom|--test-arena|--cafu-testroom|--lan-arena-host|--lan-arena-client|--trace|--input-trace|--character-switch-trace|--party-lifecycle-trace|--door-transition-trace|--merchant-checkout-trace|--native-p2-camera-collision-test|--talos-party-test|--talos-lifecycle-observation|--talos-staging-observation|--talos-post-movie-party-test|--talos-post-movie-dual-camera-test|--talos-defense-trace|--zone-transition-trace|--zone-traversal-test|--freeroam-camera-test|--control-separation-test|--player-input-trace|--second-player-movement-test|--second-player-camera-movement-test|--second-player-separation-test|--shared-group-camera-test|--split-screen-render-test|--second-player-render-camera-test|--dual-camera-frame-cache-test|--shared-quit-menu-test|--viewport-hud-test|--dual-camera-local-coop-test|--controller-bridge-test|--three-seat-input-transport-test|--three-player-local-coop-test|--realtime-skill-coop-test|--second-player-target-trace|--second-player-attack-test|--ranged-skill-test|--spirit-strike-test|--speed-test|--camera-speed-test|--check)
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
      "${mode}" == "--talos-lifecycle-observation" ||
      "${mode}" == "--door-transition-trace" ||
      "${mode}" == "--merchant-checkout-trace" ||
      "${mode}" == "--zone-traversal-test" ]]; then
    export SUDEKIMP_ZONE_TRACE=1
fi
if [[ "${mode}" == "--talos-staging-observation" ||
      "${mode}" == "--talos-post-movie-party-test" ||
      "${mode}" == "--talos-post-movie-dual-camera-test" ||
      "${mode}" == "--lan-arena-host" ||
      "${mode}" == "--lan-arena-client" ||
      "${mode}" == "--three-player-local-coop-test" ]]; then
    unset SUDEKIMP_ZONE_TRACE
fi
if [[ "${mode}" == "--lan-arena-host" || "${mode}" == "--lan-arena-client" ]]; then
    if [[ ! "${lan_arena_port}" =~ ^[0-9]+$ ]] ||
       (( lan_arena_port < 1024 || lan_arena_port > 65535 )); then
        printf 'Invalid LAN arena port: %s (expected 1024 through 65535)\n' \
            "${lan_arena_port}" >&2
        exit 2
    fi
    if [[ "${mode}" == "--lan-arena-client" &&
          ! "${lan_arena_host}" =~ ^[0-9]{1,3}(\.[0-9]{1,3}){3}$ ]]; then
        printf 'Invalid LAN arena IPv4 address: %s\n' "${lan_arena_host}" >&2
        exit 2
    fi
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
      "${mode}" == "--three-seat-input-transport-test" ||
      "${mode}" == "--three-player-local-coop-test" ||
      "${mode}" == "--talos-post-movie-party-test" ||
      "${mode}" == "--talos-post-movie-dual-camera-test" ||
      "${mode}" == "--party-lifecycle-trace" ||
      "${mode}" == "--door-transition-trace" ||
      "${mode}" == "--merchant-checkout-trace" ||
      "${mode}" == "--native-p2-camera-collision-test" ||
      "${mode}" == "--cleanroom" || "${mode}" == "--test-arena" ||
      "${mode}" == "--cafu-testroom" ]]; then
    if [[ ! "${input_bridge_port}" =~ ^[0-9]+$ ]] ||
        (( input_bridge_port < 1024 || input_bridge_port > 65535 )); then
        printf 'Invalid bridge port: %s (expected 1024 through 65535)\n' \
            "${input_bridge_port}" >&2
        exit 2
    fi
fi
if [[ "${mode}" == "--three-seat-input-transport-test" ||
      "${mode}" == "--three-player-local-coop-test" ]] &&
   (( input_bridge_port > 65533 )); then
    printf 'Invalid three-seat bridge base port: %s (expected 1024 through 65533)\n' \
        "${input_bridge_port}" >&2
    exit 2
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
if [[ "${mode}" == "--lan-arena-host" || "${mode}" == "--lan-arena-client" ]]; then
    game_launch_args+=(
        --game-arg=-Level --game-arg=testroom
        --game-arg=-DT --game-arg=1
    )
    if [[ "${mode}" == "--lan-arena-host" ]]; then
        game_launch_args+=(--game-arg=-Tal --game-arg=1)
    else
        game_launch_args+=(--game-arg=-Ailish --game-arg=1)
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
if [[ "${mode}" == "--talos-lifecycle-observation" ||
      "${mode}" == "--talos-staging-observation" ||
      "${mode}" == "--talos-post-movie-party-test" ||
      "${mode}" == "--talos-post-movie-dual-camera-test" ]]; then
    world_asset="$(dirname -- "${game}")/Data/SOLWORLDM.gex"
    if ! command -v sha256sum >/dev/null 2>&1; then
        printf '%s\n' 'sha256sum is required for the Talos exact-image gate.' >&2
        exit 1
    fi
    if [[ ! -f "${world_asset}" ]]; then
        printf 'Required Talos script asset is missing: %s\n' "${world_asset}" >&2
        exit 1
    fi
    game_sha256="$(sha256sum -- "${game}")"
    game_sha256="${game_sha256%% *}"
    world_sha256="$(sha256sum -- "${world_asset}")"
    world_sha256="${world_sha256%% *}"
    if [[ "${game_sha256}" != "${supported_game_sha256}" ]]; then
        printf '%s\n' \
            'Talos exact-image gate refused: unsupported SUDEKI.exe.' \
            "  expected: ${supported_game_sha256}" \
            "  actual:   ${game_sha256}" >&2
        exit 1
    fi
    if [[ "${world_sha256}" != "${supported_world_sha256}" ]]; then
        printf '%s\n' \
            'Talos exact-image gate refused: unsupported SOLWORLDM.gex.' \
            "  expected: ${supported_world_sha256}" \
            "  actual:   ${world_sha256}" >&2
        exit 1
    fi
    printf '%s\n' \
        'Talos exact-image gate passed.' \
        "  SUDEKI.exe:    ${game_sha256}" \
        "  SOLWORLDM.gex: ${world_sha256}"
fi
if [[ "${mode}" != "--lan-arena-host" && "${mode}" != "--lan-arena-client" &&
      ! -d "${save_root}" ]]; then
    printf 'Research save directory is missing: %s\n' "${save_root}" >&2
    exit 1
fi

save_count=0
if [[ "${mode}" != "--lan-arena-host" && "${mode}" != "--lan-arena-client" ]]; then
    save_count="$(find "${save_root}" -mindepth 1 -maxdepth 1 -type d -name 'SAVESLOT*' | wc -l)"
fi
if [[ "${mode}" != "--lan-arena-host" && "${mode}" != "--lan-arena-client" &&
      ${save_count} -lt 11 ]]; then
    printf 'Expected at least 11 research saves, found %s in %s\n' \
        "${save_count}" "${save_root}" >&2
    exit 1
fi

"${project_dir}/tools/build-linux.sh"

antialiasing_original=""
input_bridge_pid=""
input_bridge_p3_pid=""
restore_config() {
    if [[ -n "${input_bridge_p3_pid}" ]]; then
        kill "${input_bridge_p3_pid}" 2>/dev/null || true
        wait "${input_bridge_p3_pid}" 2>/dev/null || true
        input_bridge_p3_pid=""
        printf '%s\n' 'Stopped the Linux Player 3 input bridge.'
    fi
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
    --lan-arena-host|--lan-arena-client)
        # Closed direct-IP arena profile. It intentionally does not reuse the
        # local-controller bridge, split renderer, roster, campaign save, or
        # global native QuickMenu experiments.
        sed -i -E \
            -e 's/^(Enable[A-Za-z0-9]+)=.*/\1=false/' \
            -e 's/^SkipStartupMovies=.*/SkipStartupMovies=true/' \
            -e "s/^LanArenaHost=.*$/LanArenaHost=${lan_arena_host}/" \
            -e "s/^LanArenaPort=.*$/LanArenaPort=${lan_arena_port}/" \
            "${generated_config}"
        if [[ "${mode}" == "--lan-arena-host" ]]; then
            sed -i -E \
                -e 's/^EnableLanArenaHostPrototype=false$/EnableLanArenaHostPrototype=true/' \
                -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
                "${generated_config}"
            lan_expected_one='EnableLanArenaHostPrototype'
            lan_expected_two='EnableControlSeparationPrototype'
        else
            sed -i -E \
                -e 's/^EnableLanArenaClientPrototype=false$/EnableLanArenaClientPrototype=true/' \
                -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
                "${generated_config}"
            lan_expected_one='EnableLanArenaClientPrototype'
            lan_expected_two='EnableControlSeparationPrototype'
        fi
        lan_unexpected_enabled="$(awk -F= -v one="${lan_expected_one}" -v two="${lan_expected_two}" '
            $1 ~ /^Enable/ && $2 == "true" && $1 != one && $1 != two { print }
        ' "${generated_config}")"
        if [[ -n "${lan_unexpected_enabled}" ]] ||
           ! grep -Fqx "${lan_expected_one}=true" "${generated_config}" ||
           { [[ -n "${lan_expected_two}" ]] && ! grep -Fqx "${lan_expected_two}=true" "${generated_config}"; } ||
           ! grep -Fqx "LanArenaHost=${lan_arena_host}" "${generated_config}" ||
           ! grep -Fqx "LanArenaPort=${lan_arena_port}" "${generated_config}" ||
           ! grep -Fqx 'EnableExternalInputBridgePrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableThreeSeatUdpTransportPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableSplitScreenRenderPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableCoopRosterMenu=false' "${generated_config}"; then
            printf '%s\n' 'LAN arena refused: generated configuration is not the closed direct-IP profile.' >&2
            if [[ -n "${lan_unexpected_enabled}" ]]; then
                printf '%s\n' "${lan_unexpected_enabled}" >&2
            fi
            exit 1
        fi
        ;;
    --talos-post-movie-party-test)
        # Closed two-human/four-hero profile. Retail owns every companion and
        # Kazel delete plus the entire movie/TSA sequence; the DLL restores only
        # after its exact process-terminal lifecycle ticket is claimable.
        sed -i -E \
            -e 's/^(Enable[A-Za-z0-9]+)=.*/\1=false/' \
            -e 's/^EnableTalosPostMoviePartyRestorePrototype=false$/EnableTalosPostMoviePartyRestorePrototype=true/' \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            -e 's/^EnableSecondPlayerMovementPrototype=false$/EnableSecondPlayerMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerWeakAttackPrototype=false$/EnableSecondPlayerWeakAttackPrototype=true/' \
            -e 's/^EnableExternalInputBridgePrototype=false$/EnableExternalInputBridgePrototype=true/' \
            -e "s/^InputBridgePort=.*$/InputBridgePort=${input_bridge_port}/" \
            -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
            "${generated_config}"
        unexpected_enabled="$(awk -F= '
            $1 ~ /^Enable/ && $2 == "true" &&
                $1 != "EnableTalosPostMoviePartyRestorePrototype" &&
                $1 != "EnableControlSeparationPrototype" &&
                $1 != "EnableSecondPlayerMovementPrototype" &&
                $1 != "EnableSecondPlayerWeakAttackPrototype" &&
                $1 != "EnableExternalInputBridgePrototype" { print }
        ' "${generated_config}")"
        if [[ -n "${unexpected_enabled}" ]] ||
           ! grep -Fqx 'EnableTalosPostMoviePartyRestorePrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableControlSeparationPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableSecondPlayerMovementPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableSecondPlayerWeakAttackPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableExternalInputBridgePrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableNativeXInputPlayerTwoPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableSecondPlayerCameraRelativeMovementPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableCharacterSwitchTrace=false' "${generated_config}" ||
           ! grep -Fqx 'EnableTalosPartyPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableExpandedTalosEncounterPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableExpandedTalosLifecycleTrace=false' "${generated_config}" ||
           ! grep -Fqx 'EnableTalosCompanionStagingObservation=false' "${generated_config}" ||
           ! grep -Fqx 'EnablePartyAtomicTransitionsPrototype=false' "${generated_config}"; then
            printf '%s\n' \
                'Talos post-movie party test refused: generated configuration is not closed.' >&2
            if [[ -n "${unexpected_enabled}" ]]; then
                printf '%s\n' "${unexpected_enabled}" >&2
            fi
            exit 1
        fi
        ;;
    --talos-post-movie-dual-camera-test)
        # Preserve the exact post-movie four-hero/P2 baseline, then add only
        # the already-scoped distinct-angle dual-camera compositor, Camera-2
        # right-stick orbit, and matching movement-basis path.
        # The allowlist below keeps every other experiment forcibly disabled.
        sed -i -E \
            -e 's/^(Enable[A-Za-z0-9]+)=.*/\1=false/' \
            -e 's/^EnableTalosPostMoviePartyRestorePrototype=false$/EnableTalosPostMoviePartyRestorePrototype=true/' \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            -e 's/^EnableSecondPlayerMovementPrototype=false$/EnableSecondPlayerMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraRelativeMovementPrototype=false$/EnableSecondPlayerCameraRelativeMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerWeakAttackPrototype=false$/EnableSecondPlayerWeakAttackPrototype=true/' \
            -e 's/^EnableExternalInputBridgePrototype=false$/EnableExternalInputBridgePrototype=true/' \
            -e 's/^EnableSplitScreenRenderPrototype=false$/EnableSplitScreenRenderPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraPrototype=false$/EnableSecondPlayerCameraPrototype=true/' \
            -e 's/^EnableDualCameraFrameCachePrototype=false$/EnableDualCameraFrameCachePrototype=true/' \
            -e 's/^EnableSecondPlayerControllerCameraPrototype=false$/EnableSecondPlayerControllerCameraPrototype=true/' \
            -e "s/^InputBridgePort=.*$/InputBridgePort=${input_bridge_port}/" \
            -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
            "${generated_config}"
        unexpected_enabled="$(awk -F= '
            $1 ~ /^Enable/ && $2 == "true" &&
                $1 != "EnableTalosPostMoviePartyRestorePrototype" &&
                $1 != "EnableControlSeparationPrototype" &&
                $1 != "EnableSecondPlayerMovementPrototype" &&
                $1 != "EnableSecondPlayerCameraRelativeMovementPrototype" &&
                $1 != "EnableSecondPlayerWeakAttackPrototype" &&
                $1 != "EnableExternalInputBridgePrototype" &&
                $1 != "EnableSplitScreenRenderPrototype" &&
                $1 != "EnableSecondPlayerCameraPrototype" &&
                $1 != "EnableDualCameraFrameCachePrototype" &&
                $1 != "EnableSecondPlayerControllerCameraPrototype" { print }
        ' "${generated_config}")"
        if [[ -n "${unexpected_enabled}" ]] ||
           ! grep -Fqx 'EnableTalosPostMoviePartyRestorePrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableControlSeparationPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableSecondPlayerMovementPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableSecondPlayerCameraRelativeMovementPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableSecondPlayerWeakAttackPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableExternalInputBridgePrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableSplitScreenRenderPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableSecondPlayerCameraPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableDualCameraFrameCachePrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableSecondPlayerControllerCameraPrototype=true' "${generated_config}" ||
           ! grep -Fqx "InputBridgePort=${input_bridge_port}" "${generated_config}" ||
           ! grep -Fqx 'ToggleSecondPlayerAi=F10' "${generated_config}" ||
           ! grep -Fqx 'EnableNativeXInputPlayerTwoPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableSecondPlayerSeparationGuardPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableSharedGroupCameraPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableNativeSecondPlayerCameraCollisionPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableSplitScreenRangedModelIsolationPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableRangedQuickSkillPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableRealtimeMultiplayerSkillCombatPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableCharacterSwitchTrace=false' "${generated_config}" ||
           ! grep -Fqx 'EnableTalosPartyPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableExpandedTalosEncounterPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableExpandedTalosLifecycleTrace=false' "${generated_config}" ||
           ! grep -Fqx 'EnableTalosCompanionStagingObservation=false' "${generated_config}" ||
           ! grep -Fqx 'EnablePartyAtomicTransitionsPrototype=false' "${generated_config}"; then
            printf '%s\n' \
                'Talos post-movie dual-camera test refused: generated configuration is not the exact closed camera profile.' >&2
            if [[ -n "${unexpected_enabled}" ]]; then
                printf '%s\n' "${unexpected_enabled}" >&2
            fi
            exit 1
        fi
        ;;
    --talos-staging-observation)
        # Generate the exact closed ordinary-world profile. The observer is an
        # automatic one-shot immutable capture; no hotkey or native membership
        # operation exists in this build.
        sed -i -E \
            -e 's/^(Enable[A-Za-z0-9]+)=.*/\1=false/' \
            -e 's/^SkipStartupMovies=.*/SkipStartupMovies=false/' \
            -e 's/^EnableTalosCompanionStagingObservation=false$/EnableTalosCompanionStagingObservation=true/' \
            "${generated_config}"
        unexpected_enabled="$(awk -F= '
            $1 ~ /^Enable/ && $2 == "true" &&
                $1 != "EnableTalosCompanionStagingObservation" { print }
        ' "${generated_config}")"
        if [[ -n "${unexpected_enabled}" ]] ||
           ! grep -Fqx 'EnableTalosCompanionStagingObservation=true' "${generated_config}" ||
           ! grep -Fqx 'EnableTalosPostMoviePartyRestorePrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableExpandedTalosLifecycleTrace=false' "${generated_config}" ||
           ! grep -Fqx 'EnableTalosPartyPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableExpandedTalosEncounterPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'SkipStartupMovies=false' "${generated_config}"; then
            printf '%s\n' \
                'Talos staging observation refused: generated configuration is not closed and inert.' >&2
            if [[ -n "${unexpected_enabled}" ]]; then
                printf '%s\n' "${unexpected_enabled}" >&2
            fi
            exit 1
        fi
        ;;
    --talos-lifecycle-observation)
        # Make this a closed research profile even if a developer changed a
        # checked-in default locally. The one trace key is the complete native
        # integration contract; every gameplay/co-op/skill/merchant mutator is
        # forced off, and the retail story movie remains available unchanged.
        sed -i -E \
            -e 's/^(Enable[A-Za-z0-9]+)=.*/\1=false/' \
            -e 's/^SkipStartupMovies=.*/SkipStartupMovies=false/' \
            -e 's/^EnableExpandedTalosLifecycleTrace=false$/EnableExpandedTalosLifecycleTrace=true/' \
            "${generated_config}"
        unexpected_enabled="$(awk -F= '
            $1 ~ /^Enable/ && $2 == "true" &&
                $1 != "EnableExpandedTalosLifecycleTrace" { print }
        ' "${generated_config}")"
        if [[ -n "${unexpected_enabled}" ]] ||
           ! grep -Fqx 'EnableExpandedTalosLifecycleTrace=true' "${generated_config}" ||
           ! grep -Fqx 'EnableTalosPostMoviePartyRestorePrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableTalosPartyPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableExpandedTalosEncounterPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'SkipStartupMovies=false' "${generated_config}"; then
            printf '%s\n' \
                'Talos lifecycle observation refused: generated configuration is not closed and inert.' >&2
            if [[ -n "${unexpected_enabled}" ]]; then
                printf '%s\n' "${unexpected_enabled}" >&2
            fi
            exit 1
        fi
        ;;
    --cleanroom|--test-arena|--cafu-testroom)
        sed -i \
            -e 's/^EnableQuickMenuNormalSpeed=false$/EnableQuickMenuNormalSpeed=true/' \
            -e 's/^EnableCleanroomMenu=false$/EnableCleanroomMenu=true/' \
            -e 's/^EnableCoopRosterMenu=true$/EnableCoopRosterMenu=false/' \
            -e 's/^EnableStoryTestBoost=true$/EnableStoryTestBoost=false/' \
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
            -e 's/^EnableStoryTestBoost=true$/EnableStoryTestBoost=false/' \
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
    --character-switch-trace|--party-lifecycle-trace|--door-transition-trace|--merchant-checkout-trace|--native-p2-camera-collision-test|--talos-party-test|--talos-defense-trace)
        sed -i \
            -e 's/^EnableCharacterSwitchTrace=false$/EnableCharacterSwitchTrace=true/' \
            "${generated_config}"
        if [[ "${mode}" == "--party-lifecycle-trace" ||
              "${mode}" == "--door-transition-trace" ||
              "${mode}" == "--merchant-checkout-trace" ||
              "${mode}" == "--native-p2-camera-collision-test" ]]; then
            sed -i \
                -e 's/^EnableCoopRosterMenu=false$/EnableCoopRosterMenu=true/' \
                -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
                -e 's/^EnableSecondPlayerMovementPrototype=false$/EnableSecondPlayerMovementPrototype=true/' \
                -e 's/^EnableSecondPlayerCameraRelativeMovementPrototype=false$/EnableSecondPlayerCameraRelativeMovementPrototype=true/' \
                -e 's/^EnableSecondPlayerSeparationGuardPrototype=false$/EnableSecondPlayerSeparationGuardPrototype=true/' \
                -e 's/^EnableSecondPlayerWeakAttackPrototype=false$/EnableSecondPlayerWeakAttackPrototype=true/' \
                -e 's/^EnableExternalInputBridgePrototype=false$/EnableExternalInputBridgePrototype=true/' \
                -e 's/^EnablePlayerInteractionRequestsPrototype=true$/EnablePlayerInteractionRequestsPrototype=false/' \
                -e 's/^EnableTransitionVotePrototype=.*/EnableTransitionVotePrototype=false/' \
                -e "s/^InputBridgePort=.*$/InputBridgePort=${input_bridge_port}/" \
                -e 's/^EnableSplitScreenRenderPrototype=false$/EnableSplitScreenRenderPrototype=true/' \
                -e 's/^EnableSecondPlayerCameraPrototype=false$/EnableSecondPlayerCameraPrototype=true/' \
                -e 's/^EnableDualCameraFrameCachePrototype=false$/EnableDualCameraFrameCachePrototype=true/' \
                -e 's/^EnableSecondPlayerControllerCameraPrototype=false$/EnableSecondPlayerControllerCameraPrototype=true/' \
                -e 's/^EnableNativeSecondPlayerCameraCollisionPrototype=.*/EnableNativeSecondPlayerCameraCollisionPrototype=true/' \
                -e 's/^EnableTalosPartyPrototype=.*/EnableTalosPartyPrototype=false/' \
                -e 's/^EnablePartyAtomicTransitionsPrototype=false$/EnablePartyAtomicTransitionsPrototype=true/' \
                -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
                "${generated_config}"
            if [[ "${mode}" != "--party-lifecycle-trace" ]]; then
                sed -i \
                    -e 's/^EnableLoadedSaveCoopAutostartPrototype=false$/EnableLoadedSaveCoopAutostartPrototype=true/' \
                    "${generated_config}"
            fi
            if [[ "${mode}" == "--door-transition-trace" ||
                  "${mode}" == "--merchant-checkout-trace" ]]; then
                sed -i \
                    -e 's/^EnablePlayerInteractionRequestsPrototype=false$/EnablePlayerInteractionRequestsPrototype=true/' \
                    "${generated_config}"
            fi
            if [[ "${mode}" == "--merchant-checkout-trace" ]]; then
                sed -i \
                    -e 's/^EnableMerchantCheckoutTracePrototype=false$/EnableMerchantCheckoutTracePrototype=true/' \
                    "${generated_config}"
            fi
        fi
        if [[ "${mode}" == "--talos-defense-trace" ]]; then
            sed -i \
                -e 's/^EnableTalosDefenseTrace=false$/EnableTalosDefenseTrace=true/' \
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
            -e 's/^EnableSplitScreenRenderPrototype=false$/EnableSplitScreenRenderPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraPrototype=false$/EnableSecondPlayerCameraPrototype=true/' \
            -e 's/^EnableDualCameraFrameCachePrototype=false$/EnableDualCameraFrameCachePrototype=true/' \
            -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
            "${generated_config}"
        ;;
    --shared-group-camera-test)
        sed -i \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            -e 's/^EnableSecondPlayerMovementPrototype=false$/EnableSecondPlayerMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraRelativeMovementPrototype=false$/EnableSecondPlayerCameraRelativeMovementPrototype=true/' \
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
    --three-seat-input-transport-test)
        # This profile proves only transport admission and isolation. P2 keeps
        # the already-live control path; P3 has no actor/render/gameplay caller
        # until the separate fixed-three-seat integration is installed.
        sed -i -E \
            -e 's/^(Enable[A-Za-z0-9]+)=.*/\1=false/' \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            -e 's/^EnableSecondPlayerMovementPrototype=false$/EnableSecondPlayerMovementPrototype=true/' \
            -e 's/^EnableThreeSeatUdpTransportPrototype=false$/EnableThreeSeatUdpTransportPrototype=true/' \
            -e "s/^InputBridgePort=.*$/InputBridgePort=${input_bridge_port}/" \
            -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
            "${generated_config}"
        transport_unexpected_enabled="$(awk -F= '
            $1 ~ /^Enable/ && $2 == "true" &&
                $1 != "EnableControlSeparationPrototype" &&
                $1 != "EnableSecondPlayerMovementPrototype" &&
                $1 != "EnableThreeSeatUdpTransportPrototype" { print }
        ' "${generated_config}")"
        if [[ -n "${transport_unexpected_enabled}" ]] ||
           ! grep -Fqx 'EnableControlSeparationPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableSecondPlayerMovementPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableThreeSeatUdpTransportPrototype=true' "${generated_config}" ||
           ! grep -Fqx "InputBridgePort=${input_bridge_port}" "${generated_config}" ||
           ! grep -Fqx 'ToggleSecondPlayerAi=F10' "${generated_config}"; then
            printf '%s\n' \
                'Three-seat input transport test refused: generated configuration is not the exact closed profile.' >&2
            if [[ -n "${transport_unexpected_enabled}" ]]; then
                printf '%s\n' "${transport_unexpected_enabled}" >&2
            fi
            exit 1
        fi
        ;;
    --three-player-local-coop-test)
        # Closed first three-player gameplay slice. Reset every optional owner
        # before enabling the exact roster/control/render/transport set.
        sed -i -E \
            -e 's/^(Enable[A-Za-z0-9]+)=.*/\1=false/' \
            -e 's/^EnableCoopRosterMenu=false$/EnableCoopRosterMenu=true/' \
            -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
            -e 's/^EnableSecondPlayerMovementPrototype=false$/EnableSecondPlayerMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraRelativeMovementPrototype=false$/EnableSecondPlayerCameraRelativeMovementPrototype=true/' \
            -e 's/^EnableSecondPlayerWeakAttackPrototype=false$/EnableSecondPlayerWeakAttackPrototype=true/' \
            -e 's/^EnableThreeSeatUdpTransportPrototype=false$/EnableThreeSeatUdpTransportPrototype=true/' \
            -e 's/^EnableSplitScreenRenderPrototype=false$/EnableSplitScreenRenderPrototype=true/' \
            -e 's/^EnableSecondPlayerCameraPrototype=false$/EnableSecondPlayerCameraPrototype=true/' \
            -e 's/^EnableDualCameraFrameCachePrototype=false$/EnableDualCameraFrameCachePrototype=true/' \
            -e 's/^EnableFixedThreeSeatRendererPrototype=false$/EnableFixedThreeSeatRendererPrototype=true/' \
            -e 's/^EnableSecondPlayerControllerCameraPrototype=false$/EnableSecondPlayerControllerCameraPrototype=true/' \
            -e 's/^SkipStartupMovies=.*/SkipStartupMovies=true/' \
            -e 's/^InputBridgeDeadzone=.*/InputBridgeDeadzone=0.20/' \
            -e "s/^InputBridgePort=.*$/InputBridgePort=${input_bridge_port}/" \
            -e 's/^ToggleSecondPlayerAi=J$/ToggleSecondPlayerAi=F10/' \
            "${generated_config}"
        three_player_unexpected_enabled="$(awk -F= '
            $1 ~ /^Enable/ && $2 == "true" &&
                $1 != "EnableCoopRosterMenu" &&
                $1 != "EnableControlSeparationPrototype" &&
                $1 != "EnableSecondPlayerMovementPrototype" &&
                $1 != "EnableSecondPlayerCameraRelativeMovementPrototype" &&
                $1 != "EnableSecondPlayerWeakAttackPrototype" &&
                $1 != "EnableThreeSeatUdpTransportPrototype" &&
                $1 != "EnableSplitScreenRenderPrototype" &&
                $1 != "EnableSecondPlayerCameraPrototype" &&
                $1 != "EnableDualCameraFrameCachePrototype" &&
                $1 != "EnableFixedThreeSeatRendererPrototype" &&
                $1 != "EnableSecondPlayerControllerCameraPrototype" { print }
        ' "${generated_config}")"
        if [[ -n "${three_player_unexpected_enabled}" ]] ||
           ! grep -Fqx 'EnableCoopRosterMenu=true' "${generated_config}" ||
           ! grep -Fqx 'EnableControlSeparationPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableSecondPlayerMovementPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableSecondPlayerCameraRelativeMovementPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableSecondPlayerWeakAttackPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableThreeSeatUdpTransportPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableSplitScreenRenderPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableSecondPlayerCameraPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableDualCameraFrameCachePrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableFixedThreeSeatRendererPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableSecondPlayerControllerCameraPrototype=true' "${generated_config}" ||
           ! grep -Fqx 'EnableLoadedSaveCoopAutostartPrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableExternalInputBridgePrototype=false' "${generated_config}" ||
           ! grep -Fqx 'EnableNativeXInputPlayerTwoPrototype=false' "${generated_config}" ||
           [[ -v SUDEKIMP_ZONE_TRACE ]] ||
           ! grep -Fqx 'SkipStartupMovies=true' "${generated_config}" ||
           ! grep -Fqx 'InputBridgeDeadzone=0.20' "${generated_config}" ||
           ! grep -Fqx "InputBridgePort=${input_bridge_port}" "${generated_config}" ||
           ! grep -Fqx 'ToggleSecondPlayerAi=F10' "${generated_config}"; then
            printf '%s\n' \
                'Three-player local co-op test refused: generated configuration is not the exact closed profile.' >&2
            if [[ -n "${three_player_unexpected_enabled}" ]]; then
                printf '%s\n' "${three_player_unexpected_enabled}" >&2
            fi
            exit 1
        fi
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
      "${mode}" == "--party-lifecycle-trace" ||
      "${mode}" == "--door-transition-trace" ||
      "${mode}" == "--native-p2-camera-collision-test" ||
      "${mode}" == "--talos-post-movie-dual-camera-test" ||
      "${mode}" == "--controller-bridge-test" ||
      "${mode}" == "--three-player-local-coop-test" ||
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
    '  Roaming boundary: both players are warned near 10 units; at the hard limit only clearly inward movement is accepted.' \
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
        '  Both viewports warn at 8 units. At 10 units, only clearly inward movement is accepted; outward and lateral requests are blocked for either player.' \
        '  No teleport or forced catch-up is used; press F10 again to restore Buki AI before exit.'
fi
if [[ "${mode}" == "--shared-group-camera-test" ]]; then
    printf '%s\n' \
        '  Test: control anyone except Buki, press F10 once to disable Buki AI,' \
        '  then move both characters and rotate the camera.' \
        '  Camera focus should track the midpoint; distance/zoom remains native in this first proof.' \
        '  No roaming boundary is installed in this single-camera profile; press F10 again to restore native AI and focus.'
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
        '  The visible 10-unit roaming boundary is active. At the limit, move clearly inward to release it; outward and lateral requests are blocked.' \
        '  Check for view swapping, stale frames, geometry/shadow defects, or control loss.' \
        '  Press F10 again to restore Buki AI before exiting.'
fi
if [[ "${mode}" == "--controller-bridge-test" ]]; then
    printf '%s\n' \
        '  Controller bridge test: load any two-character save (including Tal/Ailish) and wait for both cameras.' \
        '  Player 1 is the front/controller-owned character; Player 2 is the first non-front party member.' \
        '  Press F10 once to disable Player 2 AI. Player 1 remains keyboard/mouse.' \
        '  Move Player 2 with the controller left stick and tap A for the selected character weak attack.' \
        '  Tap X: Tal/Buki submit native Strong; Ailish/Elco toggle only Camera 2 between third-person and camera-only first-person.' \
        '  Rotate only Player 2/right viewport with the controller right stick; Player 1 mouse should affect only the left viewport.' \
        '  After rotating Camera 2, left-stick forward must follow its forward direction. The 10-unit guard remains active.' \
        '  Unplugging the pad or stopping the helper neutralizes Player 2 input within 250 ms.' \
        '  Press F10 again to restore AI on the exact overridden character before exiting.' \
        "  Linux input device: ${input_device}"
fi
if [[ "${mode}" == "--three-seat-input-transport-test" ]]; then
    printf '%s\n' \
        '  CLOSED THREE-SEAT INPUT TRANSPORT TEST: Player 1 remains keyboard/mouse.' \
        "  Player 2 bridge: ${input_device} -> UDP ${input_bridge_port}." \
        "  Player 3 bridge: ${input_device_p3} -> UDP $((input_bridge_port + 1))." \
        '  The loader admits only fixed mask 0x07 and requires both controller transports before installing control separation.' \
        '  Player 2 retains the existing gameplay consumer. This profile does not give Player 3 an actor, camera, HUD, menu, combat context, or gameplay authority.' \
        '  Treat distinct P2/P3 packets and identities in SudekiMP.log as the only success criterion; Player 3 input doing nothing in-world is expected in this transport-only checkpoint.'
fi
if [[ "${mode}" == "--three-player-local-coop-test" ]]; then
    printf '%s\n' \
        '  CLOSED THREE-PLAYER LOCAL CO-OP TEST: choose New Game, then Co-op (3 Players).' \
        '  Choose three distinct heroes for P1, P2, and P3, review the confirmation page, and lock the roster. A seat remains inactive until its authored hero has joined the native party.' \
        '  Player 1 uses keyboard/mouse and owns the wide top viewport.' \
        "  Player 2 uses ${input_device} on UDP ${input_bridge_port} and owns the bottom-left viewport." \
        "  Player 3 uses ${input_device_p3} on UDP $((input_bridge_port + 1)) and owns the bottom-right viewport." \
        '  P2 and P3 left sticks move relative to their own camera; each right stick rotates only that seats camera. A submits the guarded native weak attack.' \
        '  Q (P1) and Y (P2/P3) open compact mod-owned Quick Menu panels in their own viewports; the global native Quick Menu remains closed. Each panel has Skills, Weapons, Items, and Spirit tabs and freezes only its owner. Player 4, roaming-boundary policy, party-transition integration, and loaded-save autostart remain disabled.' \
        '  The profile fails closed unless both distinct controller transports, all three actor/input generations, three camera/render/HUD leases, and all three fresh frame caches agree on mask 0x07.' \
        '  Stop the run on a swapped hero/view, cross-driven camera, stale frame, duplicate input device, missing lease, or any fallback that grants P3 control without a ready P3 viewport.'
fi
if [[ "${mode}" == "--party-lifecycle-trace" ]]; then
    printf '%s\n' \
        '  Roster lifecycle test: choose New Game to open the Co-op roster.' \
        '  Use Up/Down to choose P1 or P2, Left/Right to choose a hero, and Enter to lock the roster.' \
        '  The selected roles remain authoritative after WorldReady; the legacy Tal/Ailish loaded-save override is disabled in this profile.' \
        '  A selected hero becomes active only after the native story has added that hero to the party; unavailable seats stay full-screen and fail closed.' \
        '  F10 drops Player 2 out to native AI/full-screen without changing the roster; press it again to rejoin the same character.' \
        '  Controller Start requests drop-in; hold Back+Start for one second to drop out.' \
        '  Authored temporary-room doors are host-led: P1 enters normally, Sudeki moves the active party through the door, and Player 2 is rebuilt only after the destination settles.' \
        '  There is no campaign travel vote. P2 B and P1 Esc are not travel-vote controls; consent remains research-only for future divergent/custom content.' \
        '  Save points remain native and immediate: saving never opens a co-op consent vote.' \
        '  Player 1 remains keyboard/mouse. Player 2 uses the controller left stick. General P2 world-interaction provenance is isolated and disabled in this profile; no GUI Select or SOL action is replayed.' \
        '  X submits Strong Attack for Tal/Buki. For Ailish/Elco it toggles only the Player 2 viewport perspective; this is SudekiMP policy because native ranged Strong is ignored. B submits the native Sweep only in combat and acts as modal Cancel. Y opens the selected Player 2 hero Skills menu; A/B and Up/Down operate that serialized owner-pinned menu.' \
        '  The passive Select/OnAction/SOL provenance hooks remain back-burnered and are not installed. The old orange P2 INTERACT? targetless request has been removed.' \
        '  The rejected custom Blacksmith preview remains OFF. Native Blacksmith behavior is unchanged while actor/merchant provenance and a proven per-player native-window strategy are researched.' \
        '  Shops still use one serialized full-width native menu; Player 2 input is neutralized until it closes and both camera caches refresh.' \
        '  In ordinary exploration Camera 2 uses the native collision-aware Exploration camera, so walls and terrain pull it inward around Ailish. The P2 right stick is unavailable while that native camera is ready; manual orbit resumes only in fallback, combat, or unsupported phases.' \
        '  In settled exploration, both viewports warn at 80% of the 10-unit party range; at the visible hard limit only clearly inward movement is accepted for either player.' \
        '  Combat, loading, cutscenes, travel/votes, Player 2 disconnect/drop-out, or an unavailable overlay disables the hard boundary.' \
        '  A failed ownership step must leave split off rather than exposing a partial co-op state.' \
        "  Linux input device: ${input_device}"
fi
if [[ "${mode}" == "--native-p2-camera-collision-test" ]]; then
    printf '%s\n' \
        '  Focused native Camera 2 collision test: keep the persisted Co-op profile at Tal=P1 and Ailish=P2, then load the same outdoor roaming save.' \
        '  This mode includes the complete party-lifecycle configuration; only EnableNativeSecondPlayerCameraCollisionPrototype is added.' \
        '  Startup log milestone: split_screen_render_prototype_requested must report native_second_player_camera_collision=true.' \
        '  Install log milestone: split_screen_render event=install must report native_player_two_camera_collision=named_exploration_state_targeted_via_one_shot_engine_ptrobj_owned_gel_group_ptr_wrapper_no_mod_destructor_no_reuse_generation_scoped_input_broadcast_suppressed_no_independent_p2_right_stick_native_ready.' \
        '  Live log milestones: player_two_native_camera phase=target_verified must be followed by phase=state_verified and then phase=ready for Ailish in ordinary Exploration.' \
        '  Visually confirm Ailish remains centered as she walks, and that walls/terrain push Camera 2 inward instead of letting it pass through the world.' \
        '  Compare Tal on the left at the same wall or tight corner; both cameras should preserve their own character target and native obstruction behavior.' \
        '  The Player 2 right stick is intentionally unavailable while the native Exploration camera reports ready. Do not use this run to accept or reject independent native orbit.' \
        '  Manual Player 2 right-stick orbit remains available only when the native camera falls back in combat or another unsupported phase.' \
        '  Record any phase=session_disabled and its reason, missing phase=ready, view swap, off-center framing, wall clipping, camera pop, void frame, or loss of either character control.' \
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
if [[ "${mode}" == "--talos-lifecycle-observation" ]]; then
    printf '%s\n' \
        '  RESEARCH OBSERVER ONLY: no prompt, delete suppression, party carry, HP change, camera lease, input lease, or transition replay is permitted.' \
        '  Every observed SOL/native operation must pass through once with its original arguments, result, and ordering unchanged.' \
        '  1. Use one human player and keyboard/mouse only. Do not start the co-op roster or connect a Player 2 input bridge.' \
        '  2. Load a known save immediately before the final Talos handoff. Confirm Tal leads a native four-hero party.' \
        '  3. Move Tal once so the normal gameplay state is settled, then trigger the authored final interaction with Tal.' \
        '  4. Do not skip FMA07. Do not open menus, switch characters, pause, fast-forward, or press controller buttons during the transition.' \
        '  5. Let retail Sudeki remove Buki, Ailish, and Elco and enter the Void normally. Companion preservation or restoration is a failed run.' \
        '  6. After Tal reaches a stable playable Void frame, exit normally and preserve SudekiMP.log before repeating the run.' \
        '  Stop before triggering if the four-hero/Tal-lead baseline is wrong. After the transition starts, never attempt to cancel it; let native Sudeki finish and mark any trace mismatch as a failed observation.' \
        "  Log: $(dirname -- "${game}")/SudekiMP.log"
fi
if [[ "${mode}" == "--talos-staging-observation" ]]; then
    printf '%s\n' \
        '  READ-ONLY ORDINARY-WORLD OBSERVER: no GetPC, RemovePlayer, AddPlayer, wrapper destructor, party carry, or Void hook exists.' \
        '  1. Use one human player and load an ordinary save with Tal leading the native four-hero party.' \
        '  2. Keep the Sudeki window focused and remain in settled noncombat gameplay; do not enter the final Talos transition.' \
        '  3. The observer captures automatically after one exact native controller update and becomes inert after the first valid result.' \
        '  4. Once the log records valid=true, exit normally. A failed observation changes no membership or actor state.' \
        '  This run does not include FMA07 or the Tal/Kazel movie; it is only the ordinary-world four-hero baseline.' \
        "  Log: $(dirname -- "${game}")/SudekiMP.log"
fi
if [[ "${mode}" == "--talos-post-movie-party-test" ]]; then
    printf '%s\n' \
        '  CLOSED LIVE TEST: four heroes maximum. Kazel is Tals shadow half and is never retained as a fifth party member.' \
        '  1. Use Tal as Player 1 and load the known save immediately before the final Talos handoff.' \
        '  2. Trigger the authored transition normally. Retail Sudeki must remove Ailish, Buki, and Elco and execute Kazel unchanged.' \
        '  3. FMA07 is not auto-skipped. Press Escape once during the movie if you want Sudekis native movie skip; do not send synthetic or repeated skip input.' \
        '  4. Wait for the exact Kazel delete and post-movie TSA settle. Only then does one ticket restore Ailish, Buki, and Elco.' \
        '  5. Ailish is claimed automatically as Player 2. Use the controller left stick to move her and A for weak attack; do not press F10.' \
        '  6. Buki and Elco remain native AI in this first two-human/four-hero test. Confirm all four heroes are present and the companions join the fight.' \
        '  If the log reports a quarantined/failed ticket or restore, stop the run; the process deliberately does not retry or guess.' \
        "  Linux input device: ${input_device}" \
        "  Log: $(dirname -- "${game}")/SudekiMP.log"
fi
if [[ "${mode}" == "--talos-post-movie-dual-camera-test" ]]; then
    printf '%s\n' \
        '  CLOSED DUAL-CAMERA LIVE TEST: the exact four-hero restore is unchanged, and Kazel is never retained as a fifth party member.' \
        '  1. Use Tal as Player 1 and load the known save immediately before the final Talos handoff.' \
        '  2. Trigger the authored transition normally. Retail Sudeki must remove Ailish, Buki, and Elco and execute Kazel unchanged.' \
        '  3. FMA07 is not auto-skipped. Press Escape once during the movie only if you want Sudekis native movie skip; never send repeated or synthetic skip input.' \
        '  4. Wait for the exact Kazel delete, post-movie TSA settle, and valid=true four-hero restore before judging the cameras.' \
        '  5. Tal remains Player 1 on the left. Ailish is claimed automatically as Player 2 on the right; do not press F10.' \
        '  6. Rotate Ailishs right-hand camera with the controller right stick. Move with the left stick; movement must follow that independently rotated view basis. Use A for weak attack while verifying Tal-left and Ailish-right remain distinct.' \
        '  7. Buki and Elco remain native AI. Confirm both camera halves keep their assigned hero while all four join the fight.' \
        '  Scope is deliberately narrow: Player 2 right-stick orbit and camera-relative movement through that same Camera-2 basis are enabled; separation guard, native camera collision, shared camera, ranged-model isolation, and skills remain disabled.' \
        '  If the ticket/restore fails, a viewport swaps or freezes, or either control owner changes, stop the run; this process does not retry.' \
        "  Linux input device: ${input_device}" \
        "  Log: $(dirname -- "${game}")/SudekiMP.log"
fi
if [[ "${mode}" == "--door-transition-trace" ]]; then
    printf '%s\n' \
        '  Observation-only door trace: use P1 to activate one temporary-room door normally.' \
        '  The log records the validated P1 candidate, OnAction/SOL handoff, door activation, and native temporary-zone transition.' \
        '  No vote, delay, cancellation, synthetic interaction, or replay is enabled.'
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

if [[ "${mode}" == "--three-seat-input-transport-test" ||
      "${mode}" == "--three-player-local-coop-test" ]]; then
    input_device_canonical=""
    input_device_p3_canonical=""
    if ! input_device_canonical="$(readlink -e -- "${input_device}")" ||
       [[ -z "${input_device_canonical}" ||
          ! -r "${input_device_canonical}" ]]; then
        printf 'Player 2 controller device is not readable: %s\n' \
            "${input_device}" >&2
        exit 1
    fi
    if ! input_device_p3_canonical="$(readlink -e -- "${input_device_p3}")" ||
       [[ -z "${input_device_p3_canonical}" ||
          ! -r "${input_device_p3_canonical}" ]]; then
        printf 'Player 3 controller device is not readable: %s\n' \
            "${input_device_p3}" >&2
        exit 1
    fi
    if [[ "${input_device_canonical}" == "${input_device_p3_canonical}" ]]; then
        printf '%s\n' \
            'Three-seat transport refused: Player 2 and Player 3 resolve to the same controller device.' \
            "  Player 2: ${input_device} -> ${input_device_canonical}" \
            "  Player 3: ${input_device_p3} -> ${input_device_p3_canonical}" >&2
        exit 1
    fi

    : >"${input_bridge_p2_log}"
    : >"${input_bridge_p3_log}"
    "${input_bridge_helper}" \
        --device "${input_device_canonical}" \
        --port "${input_bridge_port}" \
        >"${input_bridge_p2_log}" 2>&1 &
    input_bridge_pid=$!
    sleep 0.2
    if ! kill -0 "${input_bridge_pid}" 2>/dev/null; then
        wait "${input_bridge_pid}" || true
        input_bridge_pid=""
        printf '%s\n' 'The Linux Player 2 input bridge failed to start:' >&2
        sed -n '1,80p' "${input_bridge_p2_log}" >&2
        exit 1
    fi

    "${input_bridge_helper}" \
        --device "${input_device_p3_canonical}" \
        --port "$((input_bridge_port + 1))" \
        >"${input_bridge_p3_log}" 2>&1 &
    input_bridge_p3_pid=$!
    sleep 0.2
    if ! kill -0 "${input_bridge_p3_pid}" 2>/dev/null; then
        wait "${input_bridge_p3_pid}" || true
        input_bridge_p3_pid=""
        printf '%s\n' 'The Linux Player 3 input bridge failed to start:' >&2
        sed -n '1,80p' "${input_bridge_p3_log}" >&2
        exit 1
    fi
    if ! kill -0 "${input_bridge_pid}" 2>/dev/null; then
        wait "${input_bridge_pid}" || true
        input_bridge_pid=""
        printf '%s\n' \
            'The Linux Player 2 input bridge exited while Player 3 was starting:' >&2
        sed -n '1,80p' "${input_bridge_p2_log}" >&2
        exit 1
    fi
    printf 'Linux Player 2 input bridge started (PID %s, device %s, UDP %s, log %s).\n' \
        "${input_bridge_pid}" "${input_device_canonical}" \
        "${input_bridge_port}" "${input_bridge_p2_log}"
    printf 'Linux Player 3 input bridge started (PID %s, device %s, UDP %s, log %s).\n' \
        "${input_bridge_p3_pid}" "${input_device_p3_canonical}" \
        "$((input_bridge_port + 1))" "${input_bridge_p3_log}"
elif [[ "${mode}" == "--controller-bridge-test" ||
      "${mode}" == "--talos-post-movie-party-test" ||
      "${mode}" == "--talos-post-movie-dual-camera-test" ||
      "${mode}" == "--party-lifecycle-trace" ||
      "${mode}" == "--door-transition-trace" ||
      "${mode}" == "--merchant-checkout-trace" ||
      "${mode}" == "--native-p2-camera-collision-test" ||
      "${mode}" == "--cleanroom" || "${mode}" == "--test-arena" ||
      "${mode}" == "--cafu-testroom" ]]; then
    if [[ ! -r "${input_device}" ]]; then
        printf 'Controller device is not readable: %s\n' "${input_device}" >&2
        if [[ "${mode}" == "--cleanroom" ||
              "${mode}" == "--test-arena" ||
              "${mode}" == "--cafu-testroom" ]]; then
            printf '%s\n' \
                'The cleanroom can still launch; P2 will remain WAITING until the Razer bridge is restarted.' >&2
        fi
        if [[ "${mode}" == "--controller-bridge-test" ||
              "${mode}" == "--talos-post-movie-party-test" ||
              "${mode}" == "--talos-post-movie-dual-camera-test" ||
              "${mode}" == "--party-lifecycle-trace" ||
              "${mode}" == "--door-transition-trace" ||
              "${mode}" == "--merchant-checkout-trace" ||
              "${mode}" == "--native-p2-camera-collision-test" ]]; then
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
            if [[ "${mode}" == "--controller-bridge-test" ||
              "${mode}" == "--talos-post-movie-party-test" ||
              "${mode}" == "--talos-post-movie-dual-camera-test" ||
              "${mode}" == "--party-lifecycle-trace" ||
              "${mode}" == "--door-transition-trace" ||
              "${mode}" == "--merchant-checkout-trace" ||
              "${mode}" == "--native-p2-camera-collision-test" ]]; then
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
