#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
build_dir="${project_dir}/build/windows-mingw32"

if [[ "${MSYSTEM:-}" != "MINGW32" ]]; then
    printf '%s\n' \
        'SudekiMP must be built from the MSYS2 MINGW32 terminal.' \
        'Open "MSYS2 MINGW32" and run this script again.' >&2
    exit 1
fi

for tool in gcc cmake ninja; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        printf 'Missing required MINGW32 tool: %s\n' "${tool}" >&2
        printf '%s\n' \
            'Install the documented MSYS2 packages, then run this script again.' >&2
        exit 1
    fi
done

compiler_target="$(gcc -dumpmachine)"
if [[ "${compiler_target}" != i686-w64-mingw32* ]]; then
    printf 'Wrong compiler target: %s (expected i686-w64-mingw32)\n' \
        "${compiler_target}" >&2
    exit 1
fi

cmake -S "${project_dir}" -B "${build_dir}" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "${build_dir}"

printf '\nSudekiMP Windows build complete:\n'
printf '  %s\n' "${build_dir}/bin/SudekiMP.Launcher.exe"
printf '  %s\n' "${build_dir}/bin/SudekiMP.LauncherGUI.exe"
printf '  %s\n' "${build_dir}/bin/SudekiMP.dll"
printf '  %s\n' "${build_dir}/bin/SudekiMP.ini"
printf '\nSee docs/windows-build.md for validation and installation.\n'
