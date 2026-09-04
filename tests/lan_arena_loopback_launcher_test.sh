#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
launcher="${project_dir}/tools/lan-arena-loopback.sh"
common="${project_dir}/tools/lib/lan-arena-loopback-common.sh"
test_root="$(mktemp -d)"
trap 'rm -rf -- "${test_root}"' EXIT

fail() {
    printf 'LAN loopback launcher test failed: %s\n' "$1" >&2
    exit 1
}

assert_file_text() {
    local expected="$1"
    local path="$2"
    local actual
    actual="$(<"${path}")"
    [[ "${actual}" == "${expected}" ]] ||
        fail "${path} contained '${actual}', expected '${expected}'"
}

bash -n "${launcher}" "${common}" "${BASH_SOURCE[0]}"
# shellcheck source=../tools/lib/lan-arena-loopback-common.sh
source "${common}"

help_output="$("${launcher}" --help)"
grep -Fq 'SUDEKIMP_LAN_GRAPHICS_BACKEND=wined3d|software|dxvk' \
    <<<"${help_output}"
grep -Fq 'Graphics backend (default: wined3d)' <<<"${help_output}"

# A locked, active X11/Wayland session warns before hardware-backed launches,
# but missing/unavailable logind state, non-graphical sessions, and the
# software diagnostic backend all remain silent and successful.
(
    loginctl() {
        printf '%s\n' 'Type=wayland' 'Active=yes' 'LockedHint=yes'
    }
    XDG_SESSION_ID=locked-session
    lock_output="$(lan_warn_locked_graphical_session dxvk 2>&1)"
    grep -Fq 'active graphical session locked-session is locked' \
        <<<"${lock_output}"
    grep -Fq \
        'Hardware-backed WineD3D/DXVK performance measurements may be compositor-throttled while locked; launch will continue.' \
        <<<"${lock_output}"
)
(
    loginctl_marker="${test_root}/software-loginctl-called"
    loginctl() {
        : >"${loginctl_marker}"
        printf '%s\n' 'Type=wayland' 'Active=yes' 'LockedHint=yes'
    }
    XDG_SESSION_ID=locked-session
    [[ -z "$(lan_warn_locked_graphical_session software 2>&1)" ]] ||
        fail 'software backend emitted the hardware lock warning'
    [[ ! -e "${loginctl_marker}" ]] ||
        fail 'software backend queried graphical lock state'
)
(
    loginctl() {
        printf '%s\n' 'Type=x11' 'Active=yes' 'LockedHint=no'
    }
    XDG_SESSION_ID=locked-session
    [[ -z "$(lan_warn_locked_graphical_session wined3d 2>&1)" ]] ||
        fail 'unlocked graphical session emitted a graphics warning'

    loginctl() {
        printf '%s\n' 'Type=tty' 'Active=yes' 'LockedHint=yes'
    }
    [[ -z "$(lan_warn_locked_graphical_session wined3d 2>&1)" ]] ||
        fail 'locked non-graphical session emitted a graphics warning'

    loginctl() { return 1; }
    [[ -z "$(lan_warn_locked_graphical_session dxvk 2>&1)" ]] ||
        fail 'unavailable logind state emitted a graphics warning'
)

set +e
invalid_output="$(SUDEKIMP_LAN_GRAPHICS_BACKEND=invalid \
    "${launcher}" --check 2>&1)"
invalid_status="$?"
set -e
[[ "${invalid_status}" == 2 ]] ||
    fail "invalid backend returned ${invalid_status}, expected 2"
grep -Fq 'Invalid SUDEKIMP_LAN_GRAPHICS_BACKEND: invalid' \
    <<<"${invalid_output}"

# DXVK must select one exact GE-Proton11-3 Wine/Wineserver pair, never the
# ambient wrappers used by the other two backend modes.
(
    mock_ge="${test_root}/ge-proton/files"
    mkdir -p -- "${mock_ge}/bin"
    printf '%s\n' '#!/usr/bin/env bash' 'exit 0' >"${mock_ge}/bin/wine"
    printf '%s\n' '#!/usr/bin/env bash' 'exit 0' >"${mock_ge}/bin/wineserver"
    chmod +x "${mock_ge}/bin/wine" "${mock_ge}/bin/wineserver"
    lan_select_wine_runners dxvk "${mock_ge}"
    [[ "${wine_runner}" == "${mock_ge}/bin/wine" ]] ||
        fail 'DXVK wine runner was not pinned'
    [[ "${wineserver_runner}" == "${mock_ge}/bin/wineserver" ]] ||
        fail 'DXVK wineserver runner was not pinned'
    if lan_select_wine_runners dxvk "${test_root}/missing-ge" 2>/dev/null; then
        fail 'DXVK accepted missing pinned GE-Proton runners'
    fi
)

# Plain --stop is backend-independent: it tries a bounded deduplicated set of
# the pinned, selected, installed GE, and ambient wineserver candidates for
# both exact prefixes.
(
    stop_root="${test_root}/stop-runners"
    stop_home="${stop_root}/home"
    exact_root="${stop_root}/GE-Proton11-3/files"
    ambient_root="${stop_root}/ambient"
    other_ge="${stop_home}/.steam/root/compatibilitytools.d/GE-Proton10/files"
    calls="${stop_root}/calls"
    mkdir -p -- "${exact_root}/bin" "${ambient_root}" "${other_ge}/bin"
    for runner in \
        "${exact_root}/bin/wineserver" \
        "${ambient_root}/wineserver" \
        "${other_ge}/bin/wineserver"; do
        printf '%s\n' '#!/usr/bin/env bash' 'exit 0' >"${runner}"
        chmod +x "${runner}"
    done
    timeout() { printf '%s\n' "$*" >>"${calls}"; return 0; }
    HOME="${stop_home}"
    PATH="${ambient_root}:${PATH}"
    lan_stop_prefixes_with_known_runners \
        '/prefix/host' '/prefix/client' "${exact_root}" \
        "${exact_root}/bin/wineserver" >/dev/null
    [[ "$(wc -l <"${calls}")" == 6 ]] ||
        fail 'stop runner list was not deduplicated across two prefixes'
    grep -Fq "WINEPREFIX=/prefix/host ${exact_root}/bin/wineserver -k" \
        "${calls}"
    grep -Fq "WINEPREFIX=/prefix/client ${ambient_root}/wineserver -k" \
        "${calls}"
    grep -Fq "WINEPREFIX=/prefix/host ${other_ge}/bin/wineserver -k" \
        "${calls}"
)

# A stale launch-only environment must not block cleanup. Use a fully mocked
# wineserver path so this main-script control-flow check cannot touch Wine.
(
    stop_root="${test_root}/stop-invalid-launch-env"
    stop_home="${stop_root}/home"
    stop_bin="${stop_root}/bin"
    exact_root="${stop_home}/.steam/root/compatibilitytools.d/GE-Proton11-3/files"
    mkdir -p -- "${stop_bin}" "${exact_root}/bin"
    for utility in dirname env readlink timeout; do
        ln -s -- "$(command -v "${utility}")" "${stop_bin}/${utility}"
    done
    printf '%s\n' '#!/bin/sh' 'exit 0' >"${exact_root}/bin/wineserver"
    chmod +x "${exact_root}/bin/wineserver"
    stop_output="$(
        HOME="${stop_home}" \
        PATH="${stop_bin}" \
        SUDEKIMP_LAN_HOST_WINEPREFIX='/prefix/host' \
        SUDEKIMP_LAN_CLIENT_WINEPREFIX='/prefix/client' \
        SUDEKIMP_LAN_GRAPHICS_BACKEND=invalid \
        SUDEKIMP_LAN_ARENA_PORT=invalid \
        SUDEKIMP_LAN_ARENA_LOOPBACK_TIMEOUT_MS=invalid \
        /usr/bin/bash "${launcher}" --stop
    )"
    grep -Fq 'Stopped the isolated LAN arena Wine servers.' \
        <<<"${stop_output}"
)

# Conversely, locating runners is not enough: if every bounded -k call fails,
# the helper and main stop path must not claim success. A successful -k with no
# active server remains the normal harmless success case covered above.
(
    stop_root="${test_root}/stop-all-fail"
    stop_home="${stop_root}/home"
    exact_root="${stop_root}/GE-Proton11-3/files"
    mkdir -p -- "${exact_root}/bin"
    printf '%s\n' '#!/bin/sh' 'exit 0' >"${exact_root}/bin/wineserver"
    chmod +x "${exact_root}/bin/wineserver"
    timeout() { return 1; }
    HOME="${stop_home}"
    if lan_stop_prefixes_with_known_runners \
        '/prefix/host' '/prefix/client' "${exact_root}" \
        >"${stop_root}/stdout" 2>"${stop_root}/stderr"; then
        fail 'all-failing wineserver stop calls were reported as success'
    fi
    grep -Fq 'Every LAN stop call failed' "${stop_root}/stderr"
)

# Baseline runtime seeding is also same-directory staged, checksum-verified,
# and atomically published. A publish failure leaves the old DLL untouched.
(
    seed_root="${test_root}/atomic-seed"
    mkdir -p -- "${seed_root}"
    printf '%s' 'old-runtime' >"${seed_root}/target.dll"
    printf '%s' 'new-runtime' >"${seed_root}/source.dll"
    lan_atomic_copy_verified \
        "${seed_root}/source.dll" "${seed_root}/target.dll"
    assert_file_text 'new-runtime' "${seed_root}/target.dll"

    printf '%s' 'old-runtime' >"${seed_root}/target.dll"
    mv() {
        local destination_path="${*: -1}"
        if [[ "${destination_path}" == "${seed_root}/target.dll" ]]; then
            return 1
        fi
        command mv "$@"
    }
    if lan_atomic_copy_verified \
        "${seed_root}/source.dll" "${seed_root}/target.dll" 2>/dev/null; then
        fail 'mocked atomic baseline publish unexpectedly succeeded'
    fi
    unset -f mv
    assert_file_text 'old-runtime' "${seed_root}/target.dll"
    lan_cleanup_atomic_seed_temps
    if find "${seed_root}" -maxdepth 1 -name '*.sudekimp-seed.*' |
       grep -q .; then
        fail 'failed baseline seed left a staged temp behind'
    fi

    # Model TERM landing while cp is still populating the same-directory temp:
    # the published DLL remains intact and trap cleanup removes the partial.
    printf '%s' 'old-runtime' >"${seed_root}/target.dll"
    cp() {
        local destination_path="${*: -1}"
        if [[ "${destination_path}" == *'.sudekimp-seed.'* ]]; then
            printf '%s' 'partial-runtime' >"${destination_path}"
            return 1
        fi
        command cp "$@"
    }
    if lan_atomic_copy_verified \
        "${seed_root}/source.dll" "${seed_root}/target.dll" 2>/dev/null; then
        fail 'mocked interrupted baseline copy unexpectedly succeeded'
    fi
    unset -f cp
    assert_file_text 'old-runtime' "${seed_root}/target.dll"
    if ! find "${seed_root}" -maxdepth 1 -name '*.sudekimp-seed.*' |
       grep -q .; then
        fail 'mocked interrupted baseline copy did not leave a partial temp'
    fi
    lan_cleanup_atomic_seed_temps
    if find "${seed_root}" -maxdepth 1 -name '*.sudekimp-seed.*' |
       grep -q .; then
        fail 'trap cleanup model left interrupted baseline temp behind'
    fi
)

# Backend switching clears prior state, software injects only the Mesa pair,
# and host/client DXVK logs and caches never share a directory.
(
    stage="${test_root}/environment"
    lan_configure_graphics_environment software "${stage}"
    [[ "${host_graphics_environment[*]}" == \
       '__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1' ]] ||
        fail 'software host environment mismatch'
    captured="$(lan_run_with_graphics_environment host bash -c \
        'printf "%s|%s" "$__GLX_VENDOR_LIBRARY_NAME" "$LIBGL_ALWAYS_SOFTWARE"')"
    [[ "${captured}" == 'mesa|1' ]] || fail 'software environment was not exported'

    lan_configure_graphics_environment wined3d "${stage}"
    (( ${#host_graphics_environment[@]} == 0 )) ||
        fail 'WineD3D inherited stale graphics variables'
    (( ${#client_graphics_environment[@]} == 0 )) ||
        fail 'WineD3D client inherited stale graphics variables'

    lan_configure_graphics_environment dxvk "${stage}"
    host_capture="$(lan_run_with_graphics_environment host bash -c \
        'printf "%s|%s|%s" "$DXVK_LOG_PATH" "$DXVK_STATE_CACHE_PATH" "$DXVK_SHADER_CACHE_PATH"')"
    client_capture="$(lan_run_with_graphics_environment client bash -c \
        'printf "%s|%s|%s" "$DXVK_LOG_PATH" "$DXVK_STATE_CACHE_PATH" "$DXVK_SHADER_CACHE_PATH"')"
    [[ "${host_capture}" == \
       "${stage}/dxvk/host/logs|${stage}/dxvk/host/state-cache|${stage}/dxvk/host/state-cache" ]] ||
        fail 'host DXVK environment capture mismatch'
    [[ "${client_capture}" == \
       "${stage}/dxvk/client/logs|${stage}/dxvk/client/state-cache|${stage}/dxvk/client/state-cache" ]] ||
        fail 'client DXVK environment capture mismatch'
)

# A completed DXVK install is temporary. Both targets are atomically restored
# from independently verified backups even on a successful launcher exit.
(
    transaction="${test_root}/transaction-success"
    mkdir -p -- "${transaction}/host" "${transaction}/client"
    printf '%s' 'host-wined3d' >"${transaction}/host/d3d9.dll"
    printf '%s' 'client-wined3d' >"${transaction}/client/d3d9.dll"
    printf '%s' 'verified-dxvk' >"${transaction}/dxvk-d3d9.dll"
    dxvk_sha="$(lan_file_sha256 "${transaction}/dxvk-d3d9.dll")"
    lan_begin_dxvk_transaction "${transaction}/dxvk-d3d9.dll" \
        "${dxvk_sha}" \
        "${transaction}/host/d3d9.dll" \
        "${transaction}/client/d3d9.dll"
    assert_file_text 'verified-dxvk' "${transaction}/host/d3d9.dll"
    assert_file_text 'verified-dxvk' "${transaction}/client/d3d9.dll"
    lan_restore_dxvk_transaction
    assert_file_text 'host-wined3d' "${transaction}/host/d3d9.dll"
    assert_file_text 'client-wined3d' "${transaction}/client/d3d9.dll"
    [[ "${lan_dxvk_transaction_active}" == false ]] ||
        fail 'successful restoration left transaction active'
)

# A client-side atomic-install failure occurs after the host replacement. The
# same cleanup path must restore both roles, not merely the failing client.
(
    transaction="${test_root}/transaction-client-failure"
    mkdir -p -- "${transaction}/host" "${transaction}/client"
    printf '%s' 'host-original' >"${transaction}/host/d3d9.dll"
    printf '%s' 'client-original' >"${transaction}/client/d3d9.dll"
    printf '%s' 'dxvk' >"${transaction}/dxvk-d3d9.dll"
    dxvk_sha="$(lan_file_sha256 "${transaction}/dxvk-d3d9.dll")"
    client_target="${transaction}/client/d3d9.dll"
    mv() {
        local source_path="${*: -2:1}"
        local destination_path="${*: -1}"
        if [[ "${source_path}" == *'.sudekimp-dxvk-install.'* &&
              "${destination_path}" == "${client_target}" ]]; then
            return 1
        fi
        command mv "$@"
    }
    if lan_begin_dxvk_transaction "${transaction}/dxvk-d3d9.dll" \
        "${dxvk_sha}" \
        "${transaction}/host/d3d9.dll" "${client_target}" 2>/dev/null; then
        fail 'mocked second-prefix install unexpectedly succeeded'
    fi
    unset -f mv
    assert_file_text 'dxvk' "${transaction}/host/d3d9.dll"
    lan_restore_dxvk_transaction
    assert_file_text 'host-original' "${transaction}/host/d3d9.dll"
    assert_file_text 'client-original' "${transaction}/client/d3d9.dll"
)

# Losing a recovery artifact must never be reported as a successful rollback
# while its target still contains DXVK.
(
    transaction="${test_root}/transaction-missing-backup"
    mkdir -p -- "${transaction}/host"
    printf '%s' 'host-original' >"${transaction}/host/d3d9.dll"
    printf '%s' 'dxvk' >"${transaction}/dxvk-d3d9.dll"
    dxvk_sha="$(lan_file_sha256 "${transaction}/dxvk-d3d9.dll")"
    lan_begin_dxvk_transaction "${transaction}/dxvk-d3d9.dll" \
        "${dxvk_sha}" "${transaction}/host/d3d9.dll"
    rm -f -- "${lan_dxvk_backups[0]}"
    if lan_restore_dxvk_transaction 2>/dev/null; then
        fail 'missing WineD3D backup was silently accepted'
    fi
    [[ "${lan_dxvk_transaction_active}" == true ]] ||
        fail 'failed restoration cleared transaction state'
)

# A damaged recovery copy stays in place for manual retry and is never moved
# over the target merely because it exists.
(
    transaction="${test_root}/transaction-corrupt-backup"
    mkdir -p -- "${transaction}/host"
    printf '%s' 'host-original' >"${transaction}/host/d3d9.dll"
    printf '%s' 'dxvk' >"${transaction}/dxvk-d3d9.dll"
    dxvk_sha="$(lan_file_sha256 "${transaction}/dxvk-d3d9.dll")"
    lan_begin_dxvk_transaction "${transaction}/dxvk-d3d9.dll" \
        "${dxvk_sha}" "${transaction}/host/d3d9.dll"
    printf '%s' 'damaged-backup' >"${lan_dxvk_backups[0]}"
    if lan_restore_dxvk_transaction 2>/dev/null; then
        fail 'damaged WineD3D backup was installed'
    fi
    assert_file_text 'dxvk' "${transaction}/host/d3d9.dll"
    assert_file_text 'damaged-backup' "${lan_dxvk_backups[0]}"
)

# The EXIT trap used by the launcher also restores after TERM. This subprocess
# models interruption without starting Wine or touching the real prefixes.
(
    transaction="${test_root}/transaction-interrupt"
    mkdir -p -- "${transaction}/host" "${transaction}/client"
    printf '%s' 'host-before-term' >"${transaction}/host/d3d9.dll"
    printf '%s' 'client-before-term' >"${transaction}/client/d3d9.dll"
    printf '%s' 'dxvk' >"${transaction}/dxvk-d3d9.dll"
    dxvk_sha="$(lan_file_sha256 "${transaction}/dxvk-d3d9.dll")"
    set +e
    (
        trap 'lan_restore_dxvk_transaction' EXIT
        trap 'exit 143' TERM
        lan_begin_dxvk_transaction "${transaction}/dxvk-d3d9.dll" \
            "${dxvk_sha}" \
            "${transaction}/host/d3d9.dll" \
            "${transaction}/client/d3d9.dll"
        kill -TERM "${BASHPID}"
    )
    interrupt_status="$?"
    set -e
    [[ "${interrupt_status}" == 143 ]] ||
        fail "interrupt transaction returned ${interrupt_status}"
    assert_file_text 'host-before-term' "${transaction}/host/d3d9.dll"
    assert_file_text 'client-before-term' "${transaction}/client/d3d9.dll"
)

# Validate the manifest's parsed library_path and its architecture rather than
# treating an unrelated i686 filename as proof of a usable driver.
(
    vulkan="${test_root}/vulkan"
    mkdir -p -- "${vulkan}"
    : >"${vulkan}/libvulkan.so.1"
    : >"${vulkan}/libGLX_nvidia.so.0"
    printf '%s\n' \
        '{"ICD":{"library_path":"'"${vulkan}"'/libGLX_nvidia.so.0"}}' \
        >"${vulkan}/nvidia_icd.i686.json"
    lan_find_32bit_vulkan_loader() { printf '%s' "${vulkan}/libvulkan.so.1"; }
    lan_find_nvidia_32bit_icd() { printf '%s' "${vulkan}/nvidia_icd.i686.json"; }
    file() { printf '%s\n' 'ELF 32-bit LSB shared object, Intel i386'; }
    lan_validate_nvidia_32bit_vulkan
    [[ "${host_vulkan_icd_library}" == \
       "${vulkan}/libGLX_nvidia.so.0" ]] ||
        fail 'NVIDIA ICD library_path was not parsed'
)
(
    vulkan="${test_root}/vulkan-wrong-arch"
    mkdir -p -- "${vulkan}"
    : >"${vulkan}/libvulkan.so.1"
    : >"${vulkan}/libGLX_nvidia.so.0"
    printf '%s\n' \
        '{"ICD":{"library_path":"'"${vulkan}"'/libGLX_nvidia.so.0"}}' \
        >"${vulkan}/nvidia_icd.i686.json"
    lan_find_32bit_vulkan_loader() { printf '%s' "${vulkan}/libvulkan.so.1"; }
    lan_find_nvidia_32bit_icd() { printf '%s' "${vulkan}/nvidia_icd.i686.json"; }
    file() { printf '%s\n' 'ELF 64-bit LSB shared object, x86-64'; }
    if lan_validate_nvidia_32bit_vulkan 2>/dev/null; then
        fail 'NVIDIA 64-bit ICD library passed the ELF32 gate'
    fi
)

# Exercise ordinary 1x placement, exact 2x compensation, refreshed HWND
# titles, and a failing window manager. All placement failures are warnings.
(
    calls="${test_root}/wmctrl-1x.calls"
    wmctrl() { printf '%s\n' "$*" >>"${calls}"; return 0; }
    sleep() { :; }
    lan_window_geometry() { printf '%s\n' '20 400 1366 739'; }
    place_window_geometry 0xhost 20 400 1366 739
    [[ "$(wc -l <"${calls}")" == 1 ]] ||
        fail '1x placement unexpectedly applied compensation'
    grep -Fq -- '-e 0,20,400,1366,739' "${calls}"
)
(
    calls="${test_root}/wmctrl-2x.calls"
    samples="${test_root}/wmctrl-2x.samples"
    printf '%s' 0 >"${samples}"
    wmctrl() { printf '%s\n' "$*" >>"${calls}"; return 0; }
    sleep() { :; }
    lan_window_geometry() {
        local sample
        sample="$(<"${samples}")"
        if [[ "${sample}" == 0 ]]; then
            printf '%s' 1 >"${samples}"
            printf '%s\n' '40 800 1366 739'
        else
            printf '%s\n' '20 400 1366 739'
        fi
    }
    place_window_geometry 0xhost 20 400 1366 739
    grep -Fq -- '-e 0,20,400,1366,739' "${calls}"
    grep -Fq -- '-e 0,10,200,1366,739' "${calls}"
)
(
    calls="${test_root}/wmctrl-titles.calls"
    wmctrl() { printf '%s\n' "$*" >>"${calls}"; return 0; }
    title_loopback_windows 0xnewhost 0xnewclient post_warmup >/dev/null
    grep -Fq -- '-r 0xnewhost -T Sudeki LAN Host - Tal' "${calls}"
    grep -Fq -- '-r 0xnewclient -T Sudeki LAN Client - Ailish' "${calls}"
)
(
    wmctrl() { return 1; }
    sleep() { :; }
    place_window_geometry 0xfail 20 400 1366 739 2>/dev/null ||
        fail 'wmctrl placement failure propagated'
    title_loopback_windows 0xfail 0xfail post_warmup \
        >/dev/null 2>&1 || fail 'wmctrl title failure propagated'
)

# Current console/runtime/DXVK logs are copied before the next launch can
# truncate or overwrite them.
(
    stage="${test_root}/evidence-stage"
    mkdir -p -- "${stage}/dxvk/host/logs"
    printf '%s' 'client failure' >"${stage}/client-console.log"
    printf '%s' 'NVIDIA GeForce RTX 3050' \
        >"${stage}/dxvk/host/logs/SUDEKI_d3d9.log"
    lan_archive_existing_evidence "${stage}" >/dev/null
    archive="$(find "${stage}/evidence" -mindepth 1 -maxdepth 1 \
        -type d -print -quit)"
    [[ -n "${archive}" ]] || fail 'prior evidence was not archived'
    assert_file_text 'client failure' "${archive}/client-console.log"
    assert_file_text 'NVIDIA GeForce RTX 3050' \
        "${archive}/dxvk/host/SUDEKI_d3d9.log"
)

trap_line="$(rg -n '^trap cleanup_loopback EXIT' "${launcher}" | cut -d: -f1)"
prepare_line="$(rg -n '^if ! lan_prepare_graphics_backend' "${launcher}" | cut -d: -f1)"
lock_warning_line="$(rg -n '^    lan_warn_locked_graphical_session' \
    "${launcher}" | cut -d: -f1)"
prefix_init_line="$(rg -n '^initialize_win32_prefix "\$\{host_prefix\}"' \
    "${launcher}" | cut -d: -f1)"
(( trap_line < prepare_line )) || fail 'DXVK cleanup trap is armed too late'
(( lock_warning_line < prefix_init_line )) ||
    fail 'graphical lock warning runs after prefix mutation'
grep -Fq 'lan_atomic_copy_verified "${source_system32}/${runtime}"' \
    "${launcher}"
if rg -n '(^|[[:space:]])wine "?\$\{launcher\}"?' "${launcher}" >/dev/null; then
    fail 'launcher bypasses the selected Wine runner'
fi

printf '%s\n' 'LAN loopback launcher behavioral checks passed'
