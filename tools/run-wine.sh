#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
launcher="${project_dir}/build/mingw32/bin/SudekiMP.Launcher.exe"
dll="${project_dir}/build/mingw32/bin/SudekiMP.dll"
check_flag=""
if [[ "${1:-}" == "--check" ]]; then
    check_flag="--check"
    shift
fi
game="${1:-${SUDEKIMP_GAME:-${HOME}/Games/SudekiMP/working/SUDEKI.exe}}"

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
if [[ -n "${check_flag}" ]]; then
    exec wine "${launcher}" "${check_flag}" "$(to_wine_path "${game}")" "$(to_wine_path "${dll}")"
fi
exec wine "${launcher}" "$(to_wine_path "${game}")" "$(to_wine_path "${dll}")"
