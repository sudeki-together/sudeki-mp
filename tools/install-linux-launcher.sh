#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
launcher="${script_dir}/sudekimp-launcher.sh"
icon="${project_dir}/src/launcher/assets/SudekiMP.png"
desktop_dir="${XDG_DATA_HOME:-${HOME}/.local/share}/applications"
desktop_file="${desktop_dir}/sudekimp-beta-launcher.desktop"

if [[ ! -x "${launcher}" || ! -f "${icon}" ]]; then
    printf '%s\n' 'SudekiMP launcher or project icon is missing.' >&2
    exit 1
fi

mkdir -p "${desktop_dir}"
cat > "${desktop_file}" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=SudekiMP Launcher
Comment=Launch SudekiMP local co-op, LAN arena, or cleanroom
Exec=/bin/bash "${launcher}"
Icon=${icon}
Terminal=false
Categories=Game;
StartupNotify=true
EOF
chmod 644 "${desktop_file}"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${desktop_dir}" >/dev/null 2>&1 || true
fi

printf 'Installed Linux desktop launcher: %s\n' "${desktop_file}"
