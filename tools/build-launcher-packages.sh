#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
release_dir="${project_dir}/build/releases"
stage_root="$(mktemp -d)"
linux_archive="${release_dir}/sudekimp-linux-launcher-0.4.0.tar.gz"
windows_archive="${release_dir}/sudekimp-windows-launcher-0.4.0.zip"

cleanup() { rm -rf -- "${stage_root:?}"; }
trap cleanup EXIT

"${project_dir}/tools/build-linux.sh"
mkdir -p "${release_dir}"
rm -f -- "${linux_archive}" "${windows_archive}"

linux_root="${stage_root}/SudekiMP-Linux"
mkdir -p "${linux_root}/tools" "${linux_root}/config" \
    "${linux_root}/build/mingw32/bin" "${linux_root}/build/linux/bin" \
    "${linux_root}/src/launcher/assets" "${linux_root}/public"
touch "${linux_root}/.sudekimp-package"
cp -- "${project_dir}/tools/sudekimp-beta-launcher.sh" \
    "${project_dir}/tools/sudekimp-launcher.sh" \
    "${project_dir}/tools/install-linux-launcher.sh" \
    "${project_dir}/tools/continue-research.sh" \
    "${project_dir}/tools/run-wine.sh" \
    "${project_dir}/tools/stop-sudeki.sh" \
    "${project_dir}/tools/configure-windowed.sh" \
    "${project_dir}/tools/configure-antialiasing.sh" \
    "${linux_root}/tools/"
cp -- "${project_dir}/config/SudekiMP.ini" "${linux_root}/config/"
cp -- "${project_dir}/build/mingw32/bin/SudekiMP.Launcher.exe" \
    "${project_dir}/build/mingw32/bin/SudekiMP.dll" \
    "${project_dir}/build/mingw32/bin/SudekiMP.ini" \
    "${linux_root}/build/mingw32/bin/"
cp -- "${project_dir}/build/linux/bin/sudekimp-input-bridge" \
    "${linux_root}/build/linux/bin/"
cp -- "${project_dir}/src/launcher/assets/SudekiMP.png" \
    "${linux_root}/src/launcher/assets/"
cp -- "${project_dir}/public/launcher-manifest.txt" "${linux_root}/public/"
cp -- "${project_dir}/docs/linux-coop-beta.md" "${linux_root}/README-Linux.md"
cp -- "${project_dir}/LICENSE" "${project_dir}/NOTICE" "${linux_root}/"
tar -C "${stage_root}" -czf \
    "${linux_archive}" SudekiMP-Linux

windows_root="${stage_root}/SudekiMP-Windows"
mkdir -p "${windows_root}"
cp -- "${project_dir}/build/mingw32/bin/SudekiMP.Launcher.exe" \
    "${project_dir}/build/mingw32/bin/SudekiMP.LauncherGUI.exe" \
    "${project_dir}/build/mingw32/bin/SudekiMP.XInputProbe.exe" \
    "${project_dir}/build/mingw32/bin/SudekiMP.dll" \
    "${project_dir}/build/mingw32/bin/SudekiMP.ini" \
    "${windows_root}/"
cp -- "${project_dir}/packaging/windows-beta/README-Windows.txt" \
    "${windows_root}/README-Windows.txt"
cp -- "${project_dir}/LICENSE" "${project_dir}/NOTICE" "${windows_root}/"
printf '%s\r\n%s\r\n' '@echo off' '"%~dp0SudekiMP.LauncherGUI.exe"' \
    >"${windows_root}/Launch SudekiMP.cmd"
if command -v zip >/dev/null 2>&1; then
    (cd "${stage_root}" && zip -qr \
        "${windows_archive}" SudekiMP-Windows)
else
    cmake -E tar cf "${windows_archive}" \
        --format=zip -- "${windows_root}"
fi

(cd "${release_dir}" && sha256sum \
    sudekimp-linux-launcher-0.4.0.tar.gz \
    sudekimp-windows-launcher-0.4.0.zip >SHA256SUMS)
printf 'Launcher packages written to %s\n' "${release_dir}"
