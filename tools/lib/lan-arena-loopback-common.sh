#!/usr/bin/env bash
# Shared, side-effect-free function definitions for lan-arena-loopback.sh.
# The launcher and its mocked shell tests both source this file.

declare -ag host_graphics_environment=()
declare -ag client_graphics_environment=()
declare -ag lan_dxvk_targets=()
declare -ag lan_dxvk_backups=()
declare -ag lan_dxvk_backup_temps=()
declare -ag lan_dxvk_install_temps=()
declare -ag lan_dxvk_original_hashes=()
declare -ag lan_atomic_seed_temps=()
lan_dxvk_transaction_active=false
wine_runner=""
wineserver_runner=""
host_vulkan_loader=""
host_vulkan_icd=""
host_vulkan_icd_library=""

lan_warn_locked_graphical_session() {
    local backend="$1"
    local session_id="${XDG_SESSION_ID:-}"
    local properties key value
    local session_type="" active="" locked=""

    case "${backend}" in
        wined3d|dxvk) ;;
        *) return 0 ;;
    esac
    [[ "${session_id}" =~ ^[A-Za-z0-9][A-Za-z0-9_.:-]*$ ]] || return 0
    command -v loginctl >/dev/null 2>&1 || return 0
    if ! properties="$(loginctl show-session "${session_id}" --no-pager \
        --property=Type --property=Active --property=LockedHint \
        2>/dev/null)"; then
        return 0
    fi
    while IFS='=' read -r key value; do
        case "${key}" in
            Type) session_type="${value}" ;;
            Active) active="${value}" ;;
            LockedHint) locked="${value}" ;;
        esac
    done <<<"${properties}"
    if [[ ( "${session_type}" == "x11" ||
            "${session_type}" == "wayland" ) &&
          "${active}" == "yes" && "${locked}" == "yes" ]]; then
        printf '%s\n' \
            "WARNING: active graphical session ${session_id} is locked." \
            'Hardware-backed WineD3D/DXVK performance measurements may be compositor-throttled while locked; launch will continue.' >&2
    fi
    return 0
}

lan_validate_graphics_backend() {
    local backend="$1"
    case "${backend}" in
        wined3d|software|dxvk) return 0 ;;
        *)
            printf 'Invalid SUDEKIMP_LAN_GRAPHICS_BACKEND: %s\n' \
                "${backend}" >&2
            printf '%s\n' 'Expected one of: wined3d, software, dxvk.' >&2
            return 2
            ;;
    esac
}

lan_select_wine_runners() {
    local backend="$1"
    local dxvk_root="$2"
    local candidate
    wine_runner=""
    wineserver_runner=""
    if [[ "${backend}" == "dxvk" ]]; then
        wine_runner="${dxvk_root}/bin/wine"
        wineserver_runner="${dxvk_root}/bin/wineserver"
        if [[ ! -x "${wine_runner}" || ! -x "${wineserver_runner}" ]]; then
            printf '%s\n' \
                'DXVK runs are pinned to GE-Proton11-3, but its Wine runners are unavailable.' \
                "  wine:       ${wine_runner}" \
                "  wineserver: ${wineserver_runner}" >&2
            return 127
        fi
        return 0
    fi

    wine_runner="$(command -v wine || true)"
    for candidate in \
        "${HOME}"/.steam/root/compatibilitytools.d/GE-Proton*/files/bin/wineserver; do
        if [[ -x "${candidate}" ]]; then
            wineserver_runner="${candidate}"
        fi
    done
    if [[ -z "${wineserver_runner}" ]]; then
        wineserver_runner="$(command -v wineserver || true)"
    fi
    if [[ -z "${wine_runner}" || -z "${wineserver_runner}" ]]; then
        printf '%s\n' 'No usable Wine and wineserver executables were found.' >&2
        return 127
    fi
}

lan_stop_prefixes_with_known_runners() {
    local host_prefix="$1"
    local client_prefix="$2"
    local dxvk_root="$3"
    local selected_runner="${4:-}"
    local ambient_runner candidate resolved prefix
    local attempted=0 succeeded=0
    local -a candidates=("${dxvk_root}/bin/wineserver")
    local -A seen=()
    if [[ -n "${selected_runner}" ]]; then
        candidates+=("${selected_runner}")
    fi
    ambient_runner="$(command -v wineserver || true)"
    if [[ -n "${ambient_runner}" ]]; then
        candidates+=("${ambient_runner}")
    fi
    for candidate in \
        "${HOME}"/.steam/root/compatibilitytools.d/GE-Proton*/files/bin/wineserver; do
        [[ -x "${candidate}" ]] || continue
        candidates+=("${candidate}")
    done

    for candidate in "${candidates[@]}"; do
        [[ -x "${candidate}" ]] || continue
        resolved="$(readlink -f -- "${candidate}" 2>/dev/null || true)"
        [[ -n "${resolved}" ]] || resolved="${candidate}"
        [[ -z "${seen[${resolved}]:-}" ]] || continue
        seen["${resolved}"]=1
        if (( attempted >= 8 )); then
            break
        fi
        ((attempted += 1))
        for prefix in "${client_prefix}" "${host_prefix}"; do
            if command -v timeout >/dev/null 2>&1; then
                if timeout 2s env WINEPREFIX="${prefix}" \
                    "${candidate}" -k 2>/dev/null; then
                    ((succeeded += 1))
                fi
            elif env WINEPREFIX="${prefix}" \
                "${candidate}" -k 2>/dev/null; then
                ((succeeded += 1))
            fi
        done
    done
    if (( attempted == 0 )); then
        printf '%s\n' 'No wineserver executable was available for LAN stop.' >&2
        return 127
    fi
    if (( succeeded == 0 )); then
        printf 'Every LAN stop call failed across %s deduplicated wineserver runner(s).\n' \
            "${attempted}" >&2
        return 1
    fi
    printf 'LAN stop tried %s deduplicated wineserver runner(s) across both prefixes; successful calls=%s.\n' \
        "${attempted}" "${succeeded}"
    return 0
}

lan_configure_graphics_environment() {
    local backend="$1"
    local stage_root="$2"
    local host_dxvk_root="${stage_root}/dxvk/host"
    local client_dxvk_root="${stage_root}/dxvk/client"
    host_graphics_environment=()
    client_graphics_environment=()
    case "${backend}" in
        wined3d)
            return 0
            ;;
        software)
            host_graphics_environment+=(
                '__GLX_VENDOR_LIBRARY_NAME=mesa'
                'LIBGL_ALWAYS_SOFTWARE=1'
            )
            client_graphics_environment+=(
                '__GLX_VENDOR_LIBRARY_NAME=mesa'
                'LIBGL_ALWAYS_SOFTWARE=1'
            )
            return 0
            ;;
        dxvk)
            mkdir -p -- \
                "${host_dxvk_root}/logs" "${host_dxvk_root}/state-cache" \
                "${client_dxvk_root}/logs" "${client_dxvk_root}/state-cache" ||
                return 1
            host_graphics_environment+=(
                "DXVK_LOG_PATH=${host_dxvk_root}/logs"
                "DXVK_STATE_CACHE_PATH=${host_dxvk_root}/state-cache"
                "DXVK_SHADER_CACHE_PATH=${host_dxvk_root}/state-cache"
            )
            client_graphics_environment+=(
                "DXVK_LOG_PATH=${client_dxvk_root}/logs"
                "DXVK_STATE_CACHE_PATH=${client_dxvk_root}/state-cache"
                "DXVK_SHADER_CACHE_PATH=${client_dxvk_root}/state-cache"
            )
            return 0
            ;;
    esac
    return 2
}

lan_run_with_graphics_environment() {
    local role="$1"
    shift
    case "${role}" in
        host) env "${host_graphics_environment[@]}" "$@" ;;
        client) env "${client_graphics_environment[@]}" "$@" ;;
        *)
            printf 'Unknown LAN graphics role: %s\n' "${role}" >&2
            return 2
            ;;
    esac
}

lan_file_sha256() {
    local result
    result="$(sha256sum -- "$1")" || return 1
    printf '%s' "${result%% *}"
}

lan_atomic_copy_verified() {
    local source_path="$1"
    local target_path="$2"
    local source_sha256 staged_sha256 target_sha256
    local marker staged_path
    source_sha256="$(lan_file_sha256 "${source_path}")" || return 1
    marker="${BASHPID:-$$}.${RANDOM}"
    staged_path="${target_path}.sudekimp-seed.${marker}"
    lan_atomic_seed_temps+=("${staged_path}")
    if ! cp --reflink=auto -- "${source_path}" "${staged_path}"; then
        printf 'Failed to stage runtime DLL: %s -> %s\n' \
            "${source_path}" "${target_path}" >&2
        return 1
    fi
    staged_sha256="$(lan_file_sha256 "${staged_path}" || true)"
    if [[ "${staged_sha256}" != "${source_sha256}" ]]; then
        printf 'Staged runtime DLL checksum mismatch: %s\n' \
            "${staged_path}" >&2
        return 1
    fi
    if ! mv -f -- "${staged_path}" "${target_path}"; then
        printf 'Atomic runtime DLL publish failed: %s\n' "${target_path}" >&2
        return 1
    fi
    target_sha256="$(lan_file_sha256 "${target_path}" || true)"
    if [[ "${target_sha256}" != "${source_sha256}" ]]; then
        printf 'Published runtime DLL checksum mismatch: %s\n' \
            "${target_path}" >&2
        return 1
    fi
}

lan_cleanup_atomic_seed_temps() {
    local staged_path status=0
    for staged_path in "${lan_atomic_seed_temps[@]}"; do
        rm -f -- "${staged_path}" || status=1
    done
    lan_atomic_seed_temps=()
    return "${status}"
}

lan_find_32bit_vulkan_loader() {
    local candidate description
    for candidate in \
        /usr/lib/libvulkan.so.1 \
        /usr/lib32/libvulkan.so.1 \
        /lib/libvulkan.so.1 \
        /lib32/libvulkan.so.1; do
        [[ -r "${candidate}" ]] || continue
        description="$(file -L -b -- "${candidate}" 2>/dev/null || true)"
        if [[ "${description}" == *'ELF 32-bit'* ]]; then
            printf '%s' "${candidate}"
            return 0
        fi
    done
    return 1
}

lan_find_nvidia_32bit_icd() {
    local candidate
    for candidate in \
        /usr/share/vulkan/icd.d/nvidia_icd.i686.json \
        /etc/vulkan/icd.d/nvidia_icd.i686.json \
        /usr/share/vulkan/icd.d/nvidia_icd.i386.json \
        /etc/vulkan/icd.d/nvidia_icd.i386.json; do
        if [[ -r "${candidate}" ]]; then
            printf '%s' "${candidate}"
            return 0
        fi
    done
    return 1
}

lan_resolve_icd_library() {
    local manifest="$1"
    local library_path candidate
    library_path="$(sed -nE \
        's/.*"library_path"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/p' \
        "${manifest}" | head -n 1)"
    [[ -n "${library_path}" ]] || return 1
    if [[ "${library_path}" == /* ]]; then
        [[ -r "${library_path}" ]] || return 1
        printf '%s' "${library_path}"
        return 0
    fi
    for candidate in \
        "/usr/lib/${library_path}" \
        "/usr/lib32/${library_path}" \
        "/lib/${library_path}" \
        "/lib32/${library_path}"; do
        if [[ -r "${candidate}" ]]; then
            printf '%s' "${candidate}"
            return 0
        fi
    done
    return 1
}

lan_validate_nvidia_32bit_vulkan() {
    local description
    host_vulkan_loader="$(lan_find_32bit_vulkan_loader || true)"
    if [[ -z "${host_vulkan_loader}" ]]; then
        printf '%s\n' 'DXVK requires a readable ELF32 host libvulkan.so.1.' >&2
        return 1
    fi
    host_vulkan_icd="$(lan_find_nvidia_32bit_icd || true)"
    if [[ -z "${host_vulkan_icd}" ]]; then
        printf '%s\n' \
            'DXVK requires the NVIDIA 32-bit Vulkan ICD manifest (nvidia_icd.i686.json).' >&2
        return 1
    fi
    host_vulkan_icd_library="$(lan_resolve_icd_library \
        "${host_vulkan_icd}" || true)"
    if [[ -z "${host_vulkan_icd_library}" ]]; then
        printf 'DXVK could not resolve the library_path in %s.\n' \
            "${host_vulkan_icd}" >&2
        return 1
    fi
    description="$(file -L -b -- "${host_vulkan_icd_library}" \
        2>/dev/null || true)"
    if [[ "${description}" != *'ELF 32-bit'* ]]; then
        printf '%s\n' \
            'DXVK refused a non-ELF32 NVIDIA Vulkan ICD library.' \
            "  manifest: ${host_vulkan_icd}" \
            "  library:  ${host_vulkan_icd_library}" \
            "  file:     ${description:-unavailable}" >&2
        return 1
    fi
}

lan_validate_dxvk_prefix_bridge() {
    local role="$1"
    local prefix="$2"
    local dxvk_vulkan_loader="$3"
    local dxvk_winevulkan="$4"
    local destination_system32="${prefix}/drive_c/windows/system32"
    local bridge
    for bridge in vulkan-1.dll winevulkan.dll; do
        if [[ ! -r "${destination_system32}/${bridge}" ]]; then
            printf 'DXVK requires a 32-bit Wine Vulkan bridge for %s: %s\n' \
                "${role}" "${destination_system32}/${bridge}" >&2
            return 1
        fi
    done
    if ! cmp -s -- "${dxvk_vulkan_loader}" \
        "${destination_system32}/vulkan-1.dll" ||
       ! cmp -s -- "${dxvk_winevulkan}" \
        "${destination_system32}/winevulkan.dll"; then
        printf '%s\n' \
            "DXVK requires GE-Proton11-3's matching 32-bit Wine Vulkan bridge for ${role}." \
            "Recreate the isolated prefix with GE-Proton11-3: ${prefix}" >&2
        return 1
    fi
}

lan_reset_dxvk_transaction() {
    lan_dxvk_targets=()
    lan_dxvk_backups=()
    lan_dxvk_backup_temps=()
    lan_dxvk_install_temps=()
    lan_dxvk_original_hashes=()
    lan_dxvk_transaction_active=false
}

lan_begin_dxvk_transaction() {
    local source_d3d9="$1"
    local expected_dxvk_sha256="$2"
    shift 2
    local target marker backup backup_temp install_temp
    local original_sha256 staged_sha256
    lan_reset_dxvk_transaction
    marker="${BASHPID:-$$}.${RANDOM}"
    lan_dxvk_transaction_active=true
    for target in "$@"; do
        if [[ ! -r "${target}" ]]; then
            printf 'Cannot snapshot WineD3D d3d9 before DXVK install: %s\n' \
                "${target}" >&2
            return 1
        fi
        backup="${target}.sudekimp-wined3d-backup.${marker}"
        backup_temp="${backup}.partial"
        install_temp="${target}.sudekimp-dxvk-install.${marker}"
        original_sha256="$(lan_file_sha256 "${target}")" || return 1
        lan_dxvk_targets+=("${target}")
        lan_dxvk_backups+=("${backup}")
        lan_dxvk_backup_temps+=("${backup_temp}")
        lan_dxvk_install_temps+=("${install_temp}")
        lan_dxvk_original_hashes+=("${original_sha256}")

        if ! cp --reflink=auto -- "${target}" "${backup_temp}"; then
            printf 'Failed to snapshot WineD3D d3d9: %s\n' "${target}" >&2
            return 1
        fi
        if [[ "$(lan_file_sha256 "${backup_temp}" || true)" != \
              "${original_sha256}" ]]; then
            printf 'WineD3D d3d9 snapshot verification failed: %s\n' \
                "${target}" >&2
            return 1
        fi
        if ! mv -f -- "${backup_temp}" "${backup}"; then
            printf 'Failed to publish WineD3D d3d9 snapshot: %s\n' \
                "${backup}" >&2
            return 1
        fi
        if ! cp --reflink=auto -- "${source_d3d9}" "${install_temp}"; then
            printf 'Failed to stage DXVK d3d9 for: %s\n' "${target}" >&2
            return 1
        fi
        staged_sha256="$(lan_file_sha256 "${install_temp}" || true)"
        if [[ "${staged_sha256}" != "${expected_dxvk_sha256}" ]]; then
            printf 'Staged DXVK d3d9 verification failed: %s\n' \
                "${install_temp}" >&2
            return 1
        fi
        if ! mv -f -- "${install_temp}" "${target}"; then
            printf 'Atomic DXVK d3d9 install failed: %s\n' "${target}" >&2
            return 1
        fi
    done
}

lan_restore_dxvk_transaction() {
    local status=0 index target backup backup_temp install_temp restore_temp
    local expected actual
    [[ "${lan_dxvk_transaction_active}" == true ]] || return 0
    for ((index=${#lan_dxvk_targets[@]} - 1; index >= 0; index--)); do
        target="${lan_dxvk_targets[index]}"
        backup="${lan_dxvk_backups[index]}"
        backup_temp="${lan_dxvk_backup_temps[index]}"
        install_temp="${lan_dxvk_install_temps[index]}"
        expected="${lan_dxvk_original_hashes[index]}"
        rm -f -- "${backup_temp}" "${install_temp}" || status=1
        if [[ -f "${backup}" ]]; then
            restore_temp="${target}.sudekimp-wined3d-restore.${BASHPID:-$$}.${RANDOM}"
            if ! cp --reflink=auto -- "${backup}" "${restore_temp}"; then
                printf '%s\n' \
                    "ERROR: could not stage WineD3D restoration; recovery copy remains at ${backup}" >&2
                status=1
                continue
            fi
            actual="$(lan_file_sha256 "${restore_temp}" || true)"
            if [[ "${actual}" != "${expected}" ]]; then
                printf '%s\n' \
                    "ERROR: staged WineD3D restoration checksum mismatch; recovery copy remains at ${backup}" >&2
                rm -f -- "${restore_temp}" || true
                status=1
                continue
            fi
            if ! mv -f -- "${restore_temp}" "${target}"; then
                printf '%s\n' \
                    "ERROR: atomic WineD3D restoration failed; recovery copy remains at ${backup}" >&2
                status=1
                continue
            fi
            actual="$(lan_file_sha256 "${target}" || true)"
            if [[ "${actual}" != "${expected}" ]]; then
                printf '%s\n' \
                    "ERROR: restored WineD3D checksum mismatch; recovery copy remains at ${backup}" >&2
                status=1
                continue
            fi
            if ! rm -f -- "${backup}"; then
                printf 'ERROR: restored WineD3D but could not remove backup: %s\n' \
                    "${backup}" >&2
                status=1
            fi
        else
            actual="$(lan_file_sha256 "${target}" || true)"
            if [[ "${actual}" != "${expected}" ]]; then
                printf '%s\n' \
                    "ERROR: WineD3D backup is missing and target is not original: ${target}" >&2
                status=1
            fi
        fi
    done
    if (( status == 0 )); then
        lan_reset_dxvk_transaction
    fi
    return "${status}"
}

lan_prepare_graphics_backend() {
    local backend="$1"
    local stage_root="$2"
    local dxvk_root="$3"
    local expected_dxvk_sha256="$4"
    local host_prefix="$5"
    local client_prefix="$6"
    local dxvk_d3d9="${dxvk_root}/lib/wine/dxvk/i386-windows/d3d9.dll"
    local dxvk_vulkan_loader="${dxvk_root}/lib/wine/i386-windows/vulkan-1.dll"
    local dxvk_winevulkan="${dxvk_root}/lib/wine/i386-windows/winevulkan.dll"
    local actual_dxvk_sha256
    if [[ "${backend}" != "dxvk" ]]; then
        lan_configure_graphics_environment "${backend}" "${stage_root}" ||
            return 1
        if [[ "${backend}" == "software" ]]; then
            printf '%s\n' \
                'Software backend enabled: both game processes use Mesa llvmpipe.'
        fi
        return 0
    fi

    if [[ ! -r "${dxvk_d3d9}" ]]; then
        printf 'DXVK backend file is missing: %s\n' "${dxvk_d3d9}" >&2
        return 1
    fi
    actual_dxvk_sha256="$(lan_file_sha256 "${dxvk_d3d9}")" || return 1
    if [[ "${actual_dxvk_sha256}" != "${expected_dxvk_sha256}" ]]; then
        printf '%s\n' \
            'DXVK refused an unverified GE-Proton11-3 d3d9.dll.' \
            "  expected: ${expected_dxvk_sha256}" \
            "  actual:   ${actual_dxvk_sha256}" >&2
        return 1
    fi
    if [[ ! -r "${dxvk_vulkan_loader}" || ! -r "${dxvk_winevulkan}" ]]; then
        printf '%s\n' \
            "GE-Proton11-3's 32-bit Wine Vulkan bridge is incomplete: ${dxvk_root}" >&2
        return 1
    fi
    lan_validate_nvidia_32bit_vulkan || return 1
    lan_validate_dxvk_prefix_bridge host "${host_prefix}" \
        "${dxvk_vulkan_loader}" "${dxvk_winevulkan}" || return 1
    lan_validate_dxvk_prefix_bridge client "${client_prefix}" \
        "${dxvk_vulkan_loader}" "${dxvk_winevulkan}" || return 1
    lan_configure_graphics_environment dxvk "${stage_root}" || return 1
    lan_begin_dxvk_transaction "${dxvk_d3d9}" \
        "${expected_dxvk_sha256}" \
        "${host_prefix}/drive_c/windows/system32/d3d9.dll" \
        "${client_prefix}/drive_c/windows/system32/d3d9.dll" || return 1
    printf '%s\n' \
        "DXVK d3d9 source: ${dxvk_d3d9}" \
        "ELF32 Vulkan loader: ${host_vulkan_loader}" \
        "NVIDIA 32-bit Vulkan ICD manifest: ${host_vulkan_icd}" \
        "NVIDIA ELF32 Vulkan ICD library: ${host_vulkan_icd_library}" \
        "Host DXVK logs/cache: ${stage_root}/dxvk/host" \
        "Client DXVK logs/cache: ${stage_root}/dxvk/client" \
        'DXVK d3d9 is transactional and will restore to WineD3D on launcher exit.'
}

lan_archive_existing_evidence() {
    local stage_root="$1"
    local timestamp archive_root role source_file destination_dir
    local found=false
    local -a ordinary_logs=(
        "${stage_root}/host-console.log"
        "${stage_root}/client-console.log"
        "${stage_root}/host-runtime.log"
        "${stage_root}/client-runtime.log"
    )
    for source_file in "${ordinary_logs[@]}"; do
        if [[ -s "${source_file}" ]]; then
            found=true
            break
        fi
    done
    if [[ "${found}" == false ]]; then
        for role in host client; do
            if compgen -G "${stage_root}/dxvk/${role}/logs/*.log" >/dev/null; then
                found=true
                break
            fi
        done
    fi
    [[ "${found}" == true ]] || return 0

    timestamp="$(date -u +%Y%m%dT%H%M%SZ)"
    archive_root="${stage_root}/evidence/${timestamp}-${BASHPID:-$$}-${RANDOM}"
    mkdir -p -- "${archive_root}" || return 1
    for source_file in "${ordinary_logs[@]}"; do
        [[ -s "${source_file}" ]] || continue
        cp -- "${source_file}" "${archive_root}/" || return 1
    done
    for role in host client; do
        destination_dir="${archive_root}/dxvk/${role}"
        for source_file in "${stage_root}/dxvk/${role}/logs/"*.log; do
            [[ -s "${source_file}" ]] || continue
            mkdir -p -- "${destination_dir}" || return 1
            cp -- "${source_file}" "${destination_dir}/" || return 1
        done
    done
    printf 'Archived prior LAN run evidence before overwrite: %s\n' \
        "${archive_root}"
}

monitor_geometry_by_product() {
    local product_needle="$1"
    local status_file connector_dir connector product line
    if [[ -z "${product_needle}" ]] ||
       ! command -v edid-decode >/dev/null 2>&1; then
        return 1
    fi
    for status_file in /sys/class/drm/card*-*/status; do
        [[ -r "${status_file}" ]] || continue
        grep -Fqx connected "${status_file}" || continue
        connector_dir="${status_file%/status}"
        product="$(edid-decode "${connector_dir}/edid" 2>/dev/null |
            sed -n "s/.*Display Product Name: '\([^']*\)'.*/\1/p" |
            head -n 1)"
        [[ "${product}" == *"${product_needle}"* ]] || continue
        connector="${connector_dir##*/}"
        connector="${connector#card*-}"
        line="$(xrandr --query 2>/dev/null |
            awk -v connector="${connector}" \
                '$1 == connector && $2 == "connected" { print; exit }')"
        if [[ "${line}" =~ ([0-9]+)x([0-9]+)\+([0-9]+)\+([0-9]+) ]]; then
            printf '%s %s %s %s' \
                "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" \
                "${BASH_REMATCH[3]}" "${BASH_REMATCH[4]}"
            return 0
        fi
    done
    return 1
}

lan_window_geometry() {
    local window_id="$1"
    wmctrl -l -G 2>/dev/null |
        awk -v id="${window_id}" '$1 == id { print $4, $5, $6, $7; exit }'
}

place_window_geometry() {
    local window_id="$1"
    local requested_x="$2"
    local requested_y="$3"
    local requested_width="$4"
    local requested_height="$5"
    local actual actual_x actual_y corrected_x corrected_y attempt
    if ! wmctrl -i -r "${window_id}" \
        -e "0,${requested_x},${requested_y},${requested_width},${requested_height}" \
        2>/dev/null; then
        printf 'WARNING: could not place window %s; leaving its native position.\n' \
            "${window_id}" >&2
        return 0
    fi
    # Wine/KWin can report exactly doubled top-level coordinates even when
    # XRandR exposes physical output coordinates. Detect only that observed
    # transform and leave ordinary 1x placement untouched.
    sleep 1
    actual="$(lan_window_geometry "${window_id}" || true)"
    read -r actual_x actual_y _ <<<"${actual}"
    corrected_x="${requested_x}"
    corrected_y="${requested_y}"
    if [[ "${actual_x:-}" == "$((requested_x * 2))" ]]; then
        corrected_x=$((requested_x / 2))
    fi
    if [[ "${actual_y:-}" == "$((requested_y * 2))" ]]; then
        corrected_y=$((requested_y / 2))
    fi
    if (( corrected_x != requested_x || corrected_y != requested_y )); then
        for attempt in {1..4}; do
            if ! wmctrl -i -r "${window_id}" \
                -e "0,${corrected_x},${corrected_y},${requested_width},${requested_height}" \
                2>/dev/null; then
                printf 'WARNING: compensated placement failed for window %s.\n' \
                    "${window_id}" >&2
                return 0
            fi
            sleep 1
            actual="$(lan_window_geometry "${window_id}" || true)"
            read -r actual_x actual_y _ <<<"${actual}"
            if [[ "${actual_x:-}" == "${requested_x}" &&
                  "${actual_y:-}" == "${requested_y}" ]]; then
                break
            fi
        done
    fi
    return 0
}

log_window_geometry() {
    local window_id="$1"
    local role="$2"
    local phase="$3"
    local display_label="$4"
    local display_geometry="$5"
    local requested_x="$6"
    local requested_y="$7"
    local display_width display_height display_x display_y
    local actual actual_x actual_y actual_width actual_height value
    read -r display_width display_height display_x display_y \
        <<<"${display_geometry}"
    actual="$(lan_window_geometry "${window_id}" || true)"
    read -r actual_x actual_y actual_width actual_height <<<"${actual}"
    for value in \
        "${display_width:-}" "${display_height:-}" \
        "${display_x:-}" "${display_y:-}" \
        "${actual_x:-}" "${actual_y:-}" \
        "${actual_width:-}" "${actual_height:-}"; do
        if [[ ! "${value}" =~ ^-?[0-9]+$ ]]; then
            printf 'WARNING: %s window geometry is unavailable after %s placement.\n' \
                "${role}" "${phase}" >&2
            return 0
        fi
    done
    printf '%s window geometry [%s]: actual=%s,%s %sx%s target=%s,%s output="%s" %sx%s+%s+%s\n' \
        "${role}" "${phase}" \
        "${actual_x}" "${actual_y}" "${actual_width}" "${actual_height}" \
        "${requested_x}" "${requested_y}" "${display_label}" \
        "${display_width}" "${display_height}" "${display_x}" "${display_y}"
    if (( actual_x < display_x || actual_y < display_y ||
          actual_x + actual_width > display_x + display_width ||
          actual_y + actual_height > display_y + display_height )); then
        printf 'WARNING: %s window is outside its intended output "%s" after %s placement.\n' \
            "${role}" "${display_label}" "${phase}" >&2
    fi
    return 0
}

title_loopback_windows() {
    local host_window_id="$1"
    local client_window_id="$2"
    local phase="$3"
    if ! wmctrl -i -r "${host_window_id}" -T 'Sudeki LAN Host - Tal' \
        2>/dev/null; then
        printf 'WARNING: could not title refreshed host window %s.\n' \
            "${host_window_id}" >&2
    fi
    if ! wmctrl -i -r "${client_window_id}" -T 'Sudeki LAN Client - Ailish' \
        2>/dev/null; then
        printf 'WARNING: could not title refreshed client window %s.\n' \
            "${client_window_id}" >&2
    fi
    printf 'Named LAN windows during %s placement.\n' "${phase}"
    return 0
}

place_loopback_windows() {
    local host_window_id="$1"
    local client_window_id="$2"
    local phase="${3:-initial}"
    local line width height x y
    local host_width host_height client_width client_height
    local host_display client_display
    local host_requested_x host_requested_y client_requested_x client_requested_y
    local -a displays=()

    if ! command -v wmctrl >/dev/null 2>&1; then
        printf '%s\n' \
            'wmctrl is unavailable; leaving native window titles and positions.'
        return 0
    fi
    title_loopback_windows "${host_window_id}" "${client_window_id}" \
        "${phase}"
    if [[ "${phase}" == "initial" ]]; then
        wmctrl -i -r "${host_window_id}" \
            -b remove,hidden,maximized_vert,maximized_horz,fullscreen \
            2>/dev/null || true
        wmctrl -i -r "${client_window_id}" \
            -b remove,hidden,maximized_vert,maximized_horz,fullscreen \
            2>/dev/null || true
    fi
    if ! command -v xrandr >/dev/null 2>&1; then
        printf '%s\n' \
            'xrandr is unavailable; titles applied but positions left native.'
        return 0
    fi
    while IFS= read -r line; do
        if [[ "${line}" =~ connected[[:space:]]+(primary[[:space:]]+)?([0-9]+)x([0-9]+)\+([0-9]+)\+([0-9]+) ]]; then
            displays+=(
                "${BASH_REMATCH[2]} ${BASH_REMATCH[3]} ${BASH_REMATCH[4]} ${BASH_REMATCH[5]}"
            )
        fi
    done < <(xrandr --query 2>/dev/null || true)

    host_display="$(monitor_geometry_by_product \
        "${host_monitor_product}" || true)"
    client_display="$(monitor_geometry_by_product \
        "${client_monitor_product}" || true)"
    if [[ -n "${host_display}" && -n "${client_display}" &&
          "${host_display}" != "${client_display}" ]]; then
        read -r width height x y <<<"${host_display}"
        host_width=$(( width > 1406 ? 1366 : width - 40 ))
        host_height=$(( height > 819 ? 739 : height - 80 ))
        host_requested_x=$((x + 20))
        host_requested_y=$((y + 40))
        place_window_geometry "${host_window_id}" \
            "${host_requested_x}" "${host_requested_y}" \
            "${host_width}" "${host_height}"

        read -r width height x y <<<"${client_display}"
        client_width=$(( width > 1406 ? 1366 : width - 40 ))
        client_height=$(( height > 819 ? 739 : height - 80 ))
        client_requested_x=$((x + 20))
        client_requested_y=$((y + 40))
        place_window_geometry "${client_window_id}" \
            "${client_requested_x}" "${client_requested_y}" \
            "${client_width}" "${client_height}"
        log_window_geometry "${host_window_id}" Host "${phase}" \
            "${host_monitor_product}" "${host_display}" \
            "${host_requested_x}" "${host_requested_y}"
        log_window_geometry "${client_window_id}" Client "${phase}" \
            "${client_monitor_product}" "${client_display}" \
            "${client_requested_x}" "${client_requested_y}"
        printf '%s\n' \
            "Placed Host on ${host_monitor_product} and Client on ${client_monitor_product} during ${phase} without changing focus."
        return 0
    fi

    if (( ${#displays[@]} >= 2 )); then
        read -r width height x y <<<"${displays[0]}"
        host_width=$(( width > 1406 ? 1366 : width - 40 ))
        host_height=$(( height > 819 ? 739 : height - 80 ))
        host_requested_x=$((x + 20))
        host_requested_y=$((y + 40))
        place_window_geometry "${host_window_id}" \
            "${host_requested_x}" "${host_requested_y}" \
            "${host_width}" "${host_height}"
        host_display="${displays[0]}"

        read -r width height x y <<<"${displays[1]}"
        client_width=$(( width > 1406 ? 1366 : width - 40 ))
        client_height=$(( height > 819 ? 739 : height - 80 ))
        client_requested_x=$((x + 20))
        client_requested_y=$((y + 40))
        place_window_geometry "${client_window_id}" \
            "${client_requested_x}" "${client_requested_y}" \
            "${client_width}" "${client_height}"
        client_display="${displays[1]}"
        log_window_geometry "${host_window_id}" Host "${phase}" \
            'display 1' "${host_display}" \
            "${host_requested_x}" "${host_requested_y}"
        log_window_geometry "${client_window_id}" Client "${phase}" \
            'display 2' "${client_display}" \
            "${client_requested_x}" "${client_requested_y}"
        printf '%s\n' \
            "Placed Host and Client on separate monitors during ${phase} without changing focus."
        return 0
    fi

    if (( ${#displays[@]} == 1 )); then
        read -r width height x y <<<"${displays[0]}"
        host_width=$(( width / 2 - 30 ))
        client_width="${host_width}"
        host_height=$(( height - 80 ))
        client_height="${host_height}"
        host_requested_x=$((x + 20))
        host_requested_y=$((y + 40))
        client_requested_x=$((x + width / 2 + 10))
        client_requested_y=$((y + 40))
        place_window_geometry "${host_window_id}" \
            "${host_requested_x}" "${host_requested_y}" \
            "${host_width}" "${host_height}"
        place_window_geometry "${client_window_id}" \
            "${client_requested_x}" "${client_requested_y}" \
            "${client_width}" "${client_height}"
        log_window_geometry "${host_window_id}" Host "${phase}" \
            'single display' "${displays[0]}" \
            "${host_requested_x}" "${host_requested_y}"
        log_window_geometry "${client_window_id}" Client "${phase}" \
            'single display' "${displays[0]}" \
            "${client_requested_x}" "${client_requested_y}"
        printf '%s\n' \
            "Tiled Host and Client side by side during ${phase} without changing focus."
    fi
    return 0
}
