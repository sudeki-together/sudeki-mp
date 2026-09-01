#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
game="${SUDEKIMP_GAME:-${HOME}/Games/SudekiMP/working/SUDEKI.exe}"
host_prefix="${SUDEKIMP_LAN_HOST_WINEPREFIX:-${HOME}/Games/sudeki-lan-host-win32-prefix}"
client_prefix="${SUDEKIMP_LAN_CLIENT_WINEPREFIX:-${HOME}/Games/sudeki-lan-client-win32-prefix}"
port="${SUDEKIMP_LAN_ARENA_PORT:-26770}"
loopback_timeout_ms="${SUDEKIMP_LAN_ARENA_LOOPBACK_TIMEOUT_MS:-5000}"
action="${1:---start}"
stage_root="${project_dir}/build/mingw32/lan-loopback"
host_stage="${stage_root}/host"
client_stage="${stage_root}/client"
launcher="${project_dir}/build/mingw32/bin/SudekiMP.Launcher.exe"
source_dll="${project_dir}/build/mingw32/bin/SudekiMP.dll"
source_config="${project_dir}/config/SudekiMP.ini"
supported_game_sha256='8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94'
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

usage() {
    printf '%s\n' \
        'usage: tools/lan-arena-loopback.sh [--start|--check|--network-test|--stop]' \
        '' \
        'Starts host and client Sudeki processes with isolated Wine prefixes,' \
        'DLLs, and closed LAN configs. No campaign saves are read or copied.'
}

case "${action}" in
    --start|--check|--network-test|--stop) ;;
    --help|-h) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
esac

if [[ "${host_prefix}" == "${client_prefix}" ]]; then
    printf '%s\n' 'Host and client Wine prefixes must be different.' >&2
    exit 2
fi
if [[ ! "${port}" =~ ^[0-9]+$ ]] || (( port < 1024 || port > 65535 )); then
    printf 'Invalid LAN arena port: %s\n' "${port}" >&2
    exit 2
fi
if [[ ! "${loopback_timeout_ms}" =~ ^[0-9]+$ ]] ||
   (( loopback_timeout_ms < 250 || loopback_timeout_ms > 10000 )); then
    printf 'Invalid LAN loopback timeout: %s\n' "${loopback_timeout_ms}" >&2
    exit 2
fi

if [[ "${action}" == "--stop" ]]; then
    env WINEPREFIX="${client_prefix}" "${wineserver_runner}" -k 2>/dev/null || true
    env WINEPREFIX="${host_prefix}" "${wineserver_runner}" -k 2>/dev/null || true
    printf '%s\n' 'Stopped the isolated LAN arena Wine servers.'
    exit 0
fi

if [[ "${action}" != "--network-test" && ! -f "${game}" ]]; then
    printf 'Working executable is missing: %s\n' "${game}" >&2
    exit 1
fi

initialize_win32_prefix() {
    local destination="$1"
    local attempt
    if [[ ! -f "${destination}/system.reg" ]]; then
        env WINEARCH=win32 WINEPREFIX="${destination}" wine cmd /c exit
        for attempt in {1..200}; do
            [[ -f "${destination}/system.reg" ]] && break
            sleep 0.1
        done
    fi
    if ! grep -Fqx '#arch=win32' "${destination}/system.reg"; then
        printf 'LAN prefix is not isolated 32-bit Wine: %s\n' \
            "${destination}" >&2
        exit 1
    fi
}

initialize_win32_prefix "${host_prefix}"
initialize_win32_prefix "${client_prefix}"

save_fingerprint() {
    local prefix="$1"
    local save_root="${prefix}/drive_c/users/steamuser/AppData/Roaming/Sudeki/Save"
    if [[ ! -d "${save_root}" ]]; then
        printf '%s' 'ABSENT'
        return
    fi
    find "${save_root}" -type f -print0 | sort -z |
        xargs -0 -r sha256sum | sha256sum | awk '{print $1}'
}

if [[ "${action}" == "--network-test" ]]; then
    peer_test="${project_dir}/build/mingw32/bin/SudekiMP.LanArenaLoopbackPeerTest.exe"
    network_log_root="${project_dir}/build/mingw32/lan-network-loopback"
    host_network_log="${network_log_root}/host.log"
    client_network_log="${network_log_root}/client.log"
    host_network_pid=""
    "${project_dir}/tools/build-linux.sh"
    mkdir -p -- "${network_log_root}"
    host_save_before="$(save_fingerprint "${host_prefix}")"
    client_save_before="$(save_fingerprint "${client_prefix}")"
    env WINEPREFIX="${host_prefix}" wine "${peer_test}" host "${port}" \
        >"${host_network_log}" 2>&1 &
    host_network_pid="$!"
    trap 'kill "${host_network_pid}" 2>/dev/null || true' EXIT INT TERM
    sleep 0.2
    if ! env WINEPREFIX="${client_prefix}" wine "${peer_test}" client "${port}" \
        >"${client_network_log}" 2>&1; then
        cat "${client_network_log}" >&2
        cat "${host_network_log}" >&2
        exit 1
    fi
    if ! wait "${host_network_pid}"; then
        cat "${host_network_log}" >&2
        cat "${client_network_log}" >&2
        exit 1
    fi
    trap - EXIT INT TERM
    host_save_after="$(save_fingerprint "${host_prefix}")"
    client_save_after="$(save_fingerprint "${client_prefix}")"
    if [[ "${host_save_before}" != "${host_save_after}" ||
          "${client_save_before}" != "${client_save_after}" ]]; then
        printf '%s\n' 'LAN network loopback modified a campaign save tree.' >&2
        exit 1
    fi
    cat "${host_network_log}"
    cat "${client_network_log}"
    printf '%s\n' \
        'LAN two-prefix network loopback passed: movement, weak attack, snapshot, and no save mutation.'
    exit 0
fi

actual_game_sha256="$(sha256sum -- "${game}")"
actual_game_sha256="${actual_game_sha256%% *}"
if [[ "${actual_game_sha256}" != "${supported_game_sha256}" ]]; then
    printf '%s\n' \
        'LAN arena exact-image gate refused the executable.' \
        "  expected: ${supported_game_sha256}" \
        "  actual:   ${actual_game_sha256}" >&2
    exit 1
fi

# Seed only the imported DirectX 9 runtime and its WineD3D support DLLs from
# the known-working prefix. Never copy its registry, users tree, roaming
# profile, or Sudeki save directory into either LAN process.
seed_graphics_runtime() {
    local destination="$1"
    local source_system32="${HOME}/Games/sudeki-research-prefix/drive_c/windows/system32"
    local destination_system32="${destination}/drive_c/windows/system32"
    local runtime
    mkdir -p -- "${destination_system32}"
    for runtime in \
        d3d9.dll d3dx9_30.dll d3dcompiler_47.dll wined3d.dll \
        libvkd3d-1.dll libvkd3d-shader-1.dll libvkd3d-utils-1.dll; do
        if [[ ! -f "${source_system32}/${runtime}" ]]; then
            printf 'Working DirectX runtime is missing: %s\n' \
                "${source_system32}/${runtime}" >&2
            exit 1
        fi
        cp --reflink=auto "${source_system32}/${runtime}" \
            "${destination_system32}/${runtime}"
    done
}

seed_graphics_runtime "${host_prefix}"
seed_graphics_runtime "${client_prefix}"

seed_windowed_options() {
    local destination="$1"
    local source_options="${HOME}/Games/sudeki-research-prefix/drive_c/users/steamuser/AppData/Roaming/Sudeki/PlayerOptions.xml"
    local settings_dir="${destination}/drive_c/users/steamuser/AppData/Roaming/Sudeki"
    if [[ ! -f "${source_options}" ]]; then
        printf 'Working Sudeki display options are missing: %s\n' \
            "${source_options}" >&2
        exit 1
    fi
    mkdir -p -- "${settings_dir}"
    if [[ ! -f "${settings_dir}/PlayerOptions.xml" ]]; then
        cp -- "${source_options}" "${settings_dir}/PlayerOptions.xml"
    fi
    SUDEKIMP_WINEPREFIX="${destination}" \
        "${project_dir}/tools/configure-windowed.sh" --windowed
}

seed_windowed_options "${host_prefix}"
seed_windowed_options "${client_prefix}"

"${project_dir}/tools/build-linux.sh"
mkdir -p -- "${host_stage}" "${client_stage}"
cp -- "${source_dll}" "${host_stage}/SudekiMP.dll"
cp -- "${source_dll}" "${client_stage}/SudekiMP.dll"

write_role_config() {
    local role="$1"
    local destination="$2"
    local enabled_key
    cp -- "${source_config}" "${destination}"
    sed -i -E \
        -e 's/^(Enable[A-Za-z0-9]+)=.*/\1=false/' \
        -e 's/^SkipStartupMovies=.*/SkipStartupMovies=true/' \
        -e 's/^LanArenaHost=.*/LanArenaHost=127.0.0.1/' \
        -e "s/^LanArenaPort=.*$/LanArenaPort=${port}/" \
        -e "s/^LanArenaTimeoutMs=.*$/LanArenaTimeoutMs=${loopback_timeout_ms}/" \
        -e 's/^EnableControlSeparationPrototype=false$/EnableControlSeparationPrototype=true/' \
        "${destination}"
    if [[ "${role}" == "host" ]]; then
        enabled_key='EnableLanArenaHostPrototype'
    else
        enabled_key='EnableLanArenaClientPrototype'
    fi
    sed -i -E \
        -e "s/^${enabled_key}=false$/${enabled_key}=true/" \
        "${destination}"
    unexpected="$(awk -F= -v role_key="${enabled_key}" '
        $1 ~ /^Enable/ && $2 == "true" &&
        $1 != role_key && $1 != "EnableControlSeparationPrototype" { print }
    ' "${destination}")"
    if [[ -n "${unexpected}" ]] ||
       ! grep -Fqx "${enabled_key}=true" "${destination}" ||
       ! grep -Fqx 'EnableControlSeparationPrototype=true' "${destination}" ||
       ! grep -Fqx "LanArenaPort=${port}" "${destination}" ||
       ! grep -Fqx "LanArenaTimeoutMs=${loopback_timeout_ms}" "${destination}"; then
        printf 'Failed to generate exact %s LAN profile.\n' "${role}" >&2
        exit 1
    fi
}

write_role_config host "${host_stage}/SudekiMP.ini"
write_role_config client "${client_stage}/SudekiMP.ini"

to_wine_path() {
    local absolute_path
    absolute_path="$(realpath -- "$1")"
    printf 'Z:%s' "${absolute_path//\//\\}"
}

host_dll="$(to_wine_path "${host_stage}/SudekiMP.dll")"
client_dll="$(to_wine_path "${client_stage}/SudekiMP.dll")"
game_windows="$(to_wine_path "${game}")"

if [[ "${action}" == "--check" ]]; then
    env WINEPREFIX="${host_prefix}" wine "${launcher}" --check \
        "${game_windows}" "${host_dll}"
    env WINEPREFIX="${client_prefix}" wine "${launcher}" --check \
        "${game_windows}" "${client_dll}"
    printf '%s\n' 'LAN host/client isolated-profile checks passed.'
    exit 0
fi

host_pid=""
client_pid=""
host_game_pid=""
client_game_pid=""
host_console="${stage_root}/host-console.log"
client_console="${stage_root}/client-console.log"
host_runtime_log="${stage_root}/host-runtime.log"
client_runtime_log="${stage_root}/client-runtime.log"
stop_children() {
    if [[ -n "${client_pid}" ]]; then
        kill "${client_pid}" 2>/dev/null || true
    fi
    if [[ -n "${host_pid}" ]]; then
        kill "${host_pid}" 2>/dev/null || true
    fi
    if [[ -n "${client_game_pid}" ]]; then
        kill "${client_game_pid}" 2>/dev/null || true
    fi
    if [[ -n "${host_game_pid}" ]]; then
        kill "${host_game_pid}" 2>/dev/null || true
    fi
}
trap stop_children EXIT INT TERM

printf '%s\n' \
    "Starting Tal host on UDP ${port} with prefix ${host_prefix}" \
    "Starting Ailish client through 127.0.0.1 with prefix ${client_prefix}" \
    "Host runtime log: ${host_runtime_log}" \
    "Client runtime log: ${client_runtime_log}" \
    'The harness never changes physical focus; switch between the two ordinary windows manually.'
host_save_before="$(save_fingerprint "${host_prefix}")"
client_save_before="$(save_fingerprint "${client_prefix}")"
host_runtime_log_windows="$(to_wine_path "${host_runtime_log}")"
client_runtime_log_windows="$(to_wine_path "${client_runtime_log}")"
truncate -s 0 "${host_runtime_log}" "${client_runtime_log}"
env WINEPREFIX="${host_prefix}" WINEDLLOVERRIDES='d3d9,d3dx9_30=n,b' \
    SUDEKIMP_LOG_PATH="${host_runtime_log_windows}" \
    wine "${launcher}" \
    "${game_windows}" "${host_dll}" \
    -Level testroom -DT 1 -Tal 1 >"${host_console}" 2>&1 &
host_pid="$!"

resolve_game_pid() {
    local expected_prefix="$1"
    local candidate
    for candidate in $(pgrep -x SUDEKI.exe 2>/dev/null || true); do
        if tr '\0' '\n' <"/proc/${candidate}/environ" 2>/dev/null |
           grep -Fqx "WINEPREFIX=${expected_prefix}"; then
            printf '%s' "${candidate}"
            return 0
        fi
    done
    return 1
}

resolve_game_window() {
    local game_pid="$1"
    wmctrl -l -p 2>/dev/null | awk -v pid="${game_pid}" \
        '$3 == pid && $0 ~ /Sudeki/ { print $1; exit }'
}

place_loopback_windows() {
    local host_window_id="$1"
    local client_window_id="$2"
    local line width height x y
    local host_width host_height client_width client_height
    local -a displays=()

    if ! command -v xrandr >/dev/null 2>&1 ||
       ! command -v wmctrl >/dev/null 2>&1; then
        printf '%s\n' \
            'Window placement tools are unavailable; leaving native positions.'
        return
    fi
    while IFS= read -r line; do
        if [[ "${line}" =~ connected[[:space:]]+(primary[[:space:]]+)?([0-9]+)x([0-9]+)\+([0-9]+)\+([0-9]+) ]]; then
            displays+=(
                "${BASH_REMATCH[2]} ${BASH_REMATCH[3]} ${BASH_REMATCH[4]} ${BASH_REMATCH[5]}"
            )
        fi
    done < <(xrandr --query 2>/dev/null)

    wmctrl -i -r "${host_window_id}" -T 'Sudeki LAN Host - Tal' \
        2>/dev/null || true
    wmctrl -i -r "${client_window_id}" -T 'Sudeki LAN Client - Ailish' \
        2>/dev/null || true
    printf '%s\n' \
        'Named windows: Sudeki LAN Host - Tal / Sudeki LAN Client - Ailish.'

    # Clear stale presentation state once. Neither operation activates a
    # window and this is never repeated as a focus pump.
    wmctrl -i -r "${host_window_id}" \
        -b remove,hidden,maximized_vert,maximized_horz,fullscreen 2>/dev/null || true
    wmctrl -i -r "${client_window_id}" \
        -b remove,hidden,maximized_vert,maximized_horz,fullscreen 2>/dev/null || true

    if (( ${#displays[@]} >= 2 )); then
        read -r width height x y <<<"${displays[0]}"
        host_width=$(( width > 1406 ? 1366 : width - 40 ))
        host_height=$(( height > 819 ? 739 : height - 80 ))
        wmctrl -i -r "${host_window_id}" \
            -e "0,$((x + 20)),$((y + 40)),${host_width},${host_height}" \
            2>/dev/null || true

        read -r width height x y <<<"${displays[1]}"
        client_width=$(( width > 1406 ? 1366 : width - 40 ))
        client_height=$(( height > 819 ? 739 : height - 80 ))
        wmctrl -i -r "${client_window_id}" \
            -e "0,$((x + 20)),$((y + 40)),${client_width},${client_height}" \
            2>/dev/null || true
        printf '%s\n' \
            'Placed Host and Client on separate monitors without changing focus.'
        return
    fi

    if (( ${#displays[@]} == 1 )); then
        read -r width height x y <<<"${displays[0]}"
        host_width=$(( width / 2 - 30 ))
        client_width="${host_width}"
        host_height=$(( height - 80 ))
        client_height="${host_height}"
        wmctrl -i -r "${host_window_id}" \
            -e "0,$((x + 20)),$((y + 40)),${host_width},${host_height}" \
            2>/dev/null || true
        wmctrl -i -r "${client_window_id}" \
            -e "0,$((x + width / 2 + 10)),$((y + 40)),${client_width},${client_height}" \
            2>/dev/null || true
        printf '%s\n' \
            'Tiled Host and Client side by side without changing focus.'
    fi
}

host_window=""
client_window=""
for attempt in {1..300}; do
    host_game_pid="$(resolve_game_pid "${host_prefix}" || true)"
    if [[ -n "${host_game_pid}" ]]; then
        host_window="$(resolve_game_window "${host_game_pid}")"
    fi
    [[ -n "${host_window}" ]] && break
    sleep 0.1
done
if [[ -z "${host_window}" ]]; then
    printf '%s\n' 'The isolated LAN host window did not appear.' >&2
    tail -n 40 "${host_console}" >&2 || true
    exit 1
fi

# WineD3D performs exact GL capability probes after the top-level window is
# created. The NVIDIA GLX driver has also failed when two independent WineD3D
# processes probe it concurrently. Warm the host completely before creating
# the client context, then leave both windows untouched through the client's
# own bounded probe. Network hosting safely waits for the later client.
sleep 12
if ! kill -0 "${host_game_pid}" 2>/dev/null; then
    printf '%s\n' 'The LAN host exited during graphics initialization.' >&2
    tail -n 40 "${host_console}" >&2 || true
    tail -n 40 "${host_runtime_log}" >&2 || true
    exit 1
fi

env WINEPREFIX="${client_prefix}" WINEDLLOVERRIDES='d3d9,d3dx9_30=n,b' \
    SUDEKIMP_LOG_PATH="${client_runtime_log_windows}" \
    wine "${launcher}" \
    "${game_windows}" "${client_dll}" \
    -Level testroom -DT 1 -Ailish 1 >"${client_console}" 2>&1 &
client_pid="$!"
for attempt in {1..300}; do
    client_game_pid="$(resolve_game_pid "${client_prefix}" || true)"
    if [[ -n "${client_game_pid}" ]]; then
        client_window="$(resolve_game_window "${client_game_pid}")"
    fi
    [[ -n "${client_window}" ]] && break
    sleep 0.1
done
if [[ -z "${client_window}" ]]; then
    printf '%s\n' 'The isolated LAN client window did not appear.' >&2
    tail -n 40 "${client_console}" >&2 || true
    exit 1
fi
place_loopback_windows "${host_window}" "${client_window}"
sleep 12
if ! kill -0 "${host_game_pid}" 2>/dev/null ||
   ! kill -0 "${client_game_pid}" 2>/dev/null; then
    printf '%s\n' 'A LAN arena process exited during staggered graphics initialization.' >&2
    tail -n 40 "${host_console}" >&2 || true
    tail -n 40 "${client_console}" >&2 || true
    tail -n 40 "${host_runtime_log}" >&2 || true
    tail -n 40 "${client_runtime_log}" >&2 || true
    exit 1
fi

printf 'Loopback windows active without automated focus changes: host_pid=%s client_pid=%s\n' \
    "${host_game_pid}" "${client_game_pid}"

while kill -0 "${host_game_pid}" 2>/dev/null &&
      kill -0 "${client_game_pid}" 2>/dev/null; do
    sleep 1
done
host_alive=false
client_alive=false
kill -0 "${host_game_pid}" 2>/dev/null && host_alive=true
kill -0 "${client_game_pid}" 2>/dev/null && client_alive=true
printf 'LAN arena game process ended: host_alive=%s client_alive=%s\n' \
    "${host_alive}" "${client_alive}"
host_save_after="$(save_fingerprint "${host_prefix}")"
client_save_after="$(save_fingerprint "${client_prefix}")"
if [[ "${host_save_before}" != "${host_save_after}" ||
      "${client_save_before}" != "${client_save_after}" ]]; then
    printf '%s\n' 'LAN arena game run modified a campaign save tree.' >&2
    exit 1
fi
if [[ "${host_alive}" != "${client_alive}" ]]; then
    exit 1
fi
