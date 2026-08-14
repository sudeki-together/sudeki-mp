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

usage() {
    printf '%s\n' \
        'usage: tools/continue-research.sh [--safe|--trace|--input-trace|--character-switch-trace|--freeroam-camera-test|--control-separation-test|--player-input-trace|--second-player-movement-test|--ranged-skill-test|--spirit-strike-test [key]|--speed-test [multiplier]|--camera-speed-test [multiplier]|--check]' \
        '' \
        '  --safe        Build, verify, and launch with every optional hook disabled.' \
        '  --trace       Enable normal-speed Quick Menu and observation-only Plasmatica tracing.' \
        '  --input-trace Trace native QuickSkill input and either native Plasmatica activation route.' \
        '  --character-switch-trace  Observe vanilla party rotation, controller target, and old/new AI-mode transition.' \
        '  --freeroam-camera-test Require LeftCtrl for mouse-Y distance changes outside combat; retain vanilla combat input.' \
        '  --control-separation-test Toggle non-front Buki AI off/on with J through Sudeki native control APIs.' \
        '  --player-input-trace Sample Player 1 world-direction and speed arguments without changing movement.' \
        '  --second-player-movement-test Use F10 to disable Buki AI, then move Buki with I/J/K/L.' \
        '  --ranged-skill-test  Test guarded native UI-state cycling for Elco/Ailish QuickSkills.' \
        '  --spirit-strike-test Test captured Ailish Spirit Strike ID 2; optionally override the default G key.' \
        '  --speed-test  Reproduce per-model animation control; defaults to 2.0x.' \
        '  --camera-speed-test  Run matched 2.0x caster and camera playback.' \
        '  --check       Build and verify the executable/DLL without launching the game.'
}

case "${mode}" in
    --safe|--trace|--input-trace|--character-switch-trace|--freeroam-camera-test|--control-separation-test|--player-input-trace|--second-player-movement-test|--ranged-skill-test|--spirit-strike-test|--speed-test|--camera-speed-test|--check)
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

restore_config() {
    cp -- "${source_config}" "${generated_config}"
}
trap restore_config EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

case "${mode}" in
    --trace)
        sed -i \
            -e 's/^EnableQuickMenuNormalSpeed=false$/EnableQuickMenuNormalSpeed=true/' \
            -e 's/^EnablePlasmaticaTrace=false$/EnablePlasmaticaTrace=true/' \
            "${generated_config}"
        ;;
    --input-trace)
        sed -i \
            -e 's/^EnableQuickMenuNormalSpeed=false$/EnableQuickMenuNormalSpeed=true/' \
            -e 's/^EnablePlasmaticaTrace=false$/EnablePlasmaticaTrace=true/' \
            -e 's/^EnableQuickSkillInputTrace=false$/EnableQuickSkillInputTrace=true/' \
            "${generated_config}"
        ;;
    --character-switch-trace)
        sed -i \
            -e 's/^EnableCharacterSwitchTrace=false$/EnableCharacterSwitchTrace=true/' \
            "${generated_config}"
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
            -e 's/^ToggleBukiAi=J$/ToggleBukiAi=F10/' \
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
    '  Camera status: free-roam wheel and modifier prototypes were not visibly effective; desired-distance tracing is next.' \
    '  Deferred: full no-menu encounter remains required before Milestone 3 closes.' \
    '  Emergency stop: tools/stop-sudeki.sh' \
    '  Detailed handoff: docs/research-log.md and docs/combat.md.' \
    "  Research saves: ${save_count}" \
    "  Mode: ${mode}"

if [[ "${mode}" == "--spirit-strike-test" ]]; then
    printf '  Spirit Strike test key: %s\n' "${spirit_key}"
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
if [[ "${mode}" == "--freeroam-camera-test" ]]; then
    printf '%s\n' \
        '  Test outside combat: mouse forward/back should do nothing until LeftCtrl is held.' \
        '  Hold LeftCtrl and move the mouse forward/back; native distance control should resume.' \
        '  Mouse left/right rotation and all combat input remain vanilla; limits and pitch are unchanged.'
fi

if [[ "${mode}" == "--check" ]]; then
    exit 0
fi

SUDEKIMP_WINEPREFIX="${research_prefix}" \
    "${project_dir}/tools/run-wine.sh" "${game}"
