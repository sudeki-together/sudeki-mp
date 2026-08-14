#!/usr/bin/env bash
set -euo pipefail

research_prefix="${SUDEKIMP_WINEPREFIX:-${HOME}/Games/sudeki-research-prefix}"
wineserver_runner=""

for candidate in "${HOME}"/.steam/root/compatibilitytools.d/GE-Proton*/files/bin/wineserver; do
    if [[ -x "${candidate}" ]]; then
        wineserver_runner="${candidate}"
    fi
done

if [[ -z "${wineserver_runner}" ]]; then
    wineserver_runner="$(command -v wineserver || true)"
fi
if [[ -z "${wineserver_runner}" ]]; then
    printf '%s\n' 'No GE-Proton or system wineserver executable was found.' >&2
    exit 127
fi
if [[ ! -d "${research_prefix}" ]]; then
    printf 'Sudeki research prefix does not exist: %s\n' "${research_prefix}" >&2
    exit 1
fi

WINEPREFIX="${research_prefix}" "${wineserver_runner}" -k
printf 'Stopped every Wine process in the dedicated Sudeki prefix: %s\n' \
    "${research_prefix}"
