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

usage() {
    printf '%s\n' \
        'usage: tools/continue-research.sh [--safe|--trace|--speed-test [multiplier]|--camera-speed-test [multiplier]|--check]' \
        '' \
        '  --safe        Build, verify, and launch with every optional hook disabled.' \
        '  --trace       Enable normal-speed Quick Menu and observation-only Plasmatica tracing.' \
        '  --speed-test  Reproduce per-model animation control; defaults to 2.0x.' \
        '  --camera-speed-test  Run matched 2.0x caster and camera playback.' \
        '  --check       Build and verify the executable/DLL without launching the game.'
}

case "${mode}" in
    --safe|--trace|--speed-test|--camera-speed-test|--check)
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
    '  Next: Phase 6 observation-first direct activation and one disabled-by-default hotkey.' \
    '  Detailed handoff: docs/research-log.md and docs/combat.md.' \
    "  Research saves: ${save_count}" \
    "  Mode: ${mode}"

if [[ "${mode}" == "--check" ]]; then
    exit 0
fi

SUDEKIMP_WINEPREFIX="${research_prefix}" \
    "${project_dir}/tools/run-wine.sh" "${game}"
