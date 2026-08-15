#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
build_dir="${project_dir}/build/mingw32"
native_build_dir="${project_dir}/build/linux/bin"

mkdir -p "${native_build_dir}"
cc -std=c11 -O2 -Wall -Wextra -Wpedantic \
    -I"${project_dir}/src" \
    "${project_dir}/tools/input-bridge/main.c" \
    "${project_dir}/src/input/bridge_protocol.c" \
    -o "${native_build_dir}/sudekimp-input-bridge"

flatpak run --command=sh org.ghidra_sre.Ghidra -c '
    set -eu
    export PATH="/usr/lib/sdk/mingw-w64/bin:${PATH}"
    cmake -S "$1" -B "$2" -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_TOOLCHAIN_FILE="$1/cmake/toolchains/mingw32-flatpak.cmake"
    cmake --build "$2"
' sh "${project_dir}" "${build_dir}"
