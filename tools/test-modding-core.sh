#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
mod_test_dir="${project_dir}/build/modding-host"
mkdir -p "${mod_test_dir}"
mod_core_sources=(
    "${project_dir}/src/modding/mod_archive.c"
    "${project_dir}/src/modding/mod_image.c"
    "${project_dir}/src/modding/mod_manifest.c"
    "${project_dir}/src/modding/mod_groups.c"
    "${project_dir}/src/engine/texture_mod_index.c"
)
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -UNDEBUG \
    -I"${project_dir}/src" \
    "${project_dir}/tests/modding_core_test.c" \
    "${project_dir}/tests/modding_archive_image_test.c" \
    "${project_dir}/tests/modding_manifest_test.c" \
    "${project_dir}/tests/modding_groups_test.c" \
    "${mod_core_sources[@]}" -o "${mod_test_dir}/SudekiMP.ModdingCoreTest"
"${mod_test_dir}/SudekiMP.ModdingCoreTest"
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -UNDEBUG \
    -I"${project_dir}/src" "${project_dir}/tests/modding_roundtrip.c" \
    "${mod_core_sources[@]}" -o "${mod_test_dir}/ModdingRoundtrip"
python3 "${project_dir}/tests/modding_roundtrip_test.py" "${mod_test_dir}/ModdingRoundtrip"
python3 "${project_dir}/tests/sudekimod_test.py"
