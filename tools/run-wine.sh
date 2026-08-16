#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
launcher="${project_dir}/build/mingw32/bin/SudekiMP.Launcher.exe"
dll="${project_dir}/build/mingw32/bin/SudekiMP.dll"
check_flag=""
display_mode=""
obs_game_capture="false"
game=""
game_args=()
while (( $# > 0 )); do
    case "$1" in
        --check)
            check_flag="--check"
            ;;
        --windowed)
            display_mode="--windowed"
            ;;
        --fullscreen)
            display_mode="--fullscreen"
            ;;
        --obs-gamecapture)
            obs_game_capture="true"
            ;;
        --game-arg)
            shift
            if (( $# == 0 )); then
                printf '%s\n' '--game-arg requires a value.' >&2
                exit 2
            fi
            game_args+=("$1")
            ;;
        --game-arg=*)
            game_args+=("${1#*=}")
            ;;
        --help|-h)
            printf '%s\n' \
                'usage: tools/run-wine.sh [--check] [--windowed|--fullscreen] [--obs-gamecapture] [--game-arg value] [SUDEKI.exe]'
            exit 0
            ;;
        --*)
            printf 'Unknown option: %s\n' "$1" >&2
            exit 2
            ;;
        *)
            if [[ -n "${game}" ]]; then
                printf 'Unexpected extra game path: %s\n' "$1" >&2
                exit 2
            fi
            game="$1"
            ;;
    esac
    shift
done
game="${game:-${SUDEKIMP_GAME:-${HOME}/Games/SudekiMP/working/SUDEKI.exe}}"

if [[ ! -f "${launcher}" || ! -f "${dll}" ]]; then
    printf '%s\n' 'Build artifacts are missing; run tools/build-linux.sh first.' >&2
    exit 1
fi
if [[ ! -f "${game}" ]]; then
    printf 'Game executable does not exist: %s\n' "${game}" >&2
    exit 1
fi

to_wine_path() {
    local absolute_path
    absolute_path="$(realpath -- "$1")"
    printf 'Z:%s' "${absolute_path//\//\\}"
}

export WINEPREFIX="${SUDEKIMP_WINEPREFIX:-${HOME}/Games/sudeki-offline-prefix}"
if [[ -n "${display_mode}" ]]; then
    "${project_dir}/tools/configure-windowed.sh" "${display_mode}"
fi
if [[ -n "${check_flag}" ]]; then
    exec wine "${launcher}" "${check_flag}" "$(to_wine_path "${game}")" "$(to_wine_path "${dll}")"
fi
if [[ "${obs_game_capture}" == "true" ]]; then
    if ! command -v obs-gamecapture >/dev/null 2>&1; then
        printf '%s\n' \
            'obs-gamecapture is unavailable; install the 32-bit and 64-bit obs-vkcapture hook libraries.' >&2
        exit 1
    fi
    exec obs-gamecapture wine "${launcher}" "$(to_wine_path "${game}")" "$(to_wine_path "${dll}")" "${game_args[@]}"
fi
exec wine "${launcher}" "$(to_wine_path "${game}")" "$(to_wine_path "${dll}")" "${game_args[@]}"
