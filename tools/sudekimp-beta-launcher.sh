#!/usr/bin/env bash
set -euo pipefail

# Public Linux front end. Game-facing configuration remains delegated to the
# guarded research launcher so exact-image checks and rollback stay centralized.

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
research_launcher="${script_dir}/continue-research.sh"
stop_launcher="${script_dir}/stop-sudeki.sh"
settings_dir="${XDG_CONFIG_HOME:-${HOME}/.config}/sudekimp-launcher"
settings_file="${settings_dir}/settings"
launch_log="${project_dir}/build/linux/sudekimp-launcher.log"
project_icon="${project_dir}/src/launcher/assets/SudekiMP.png"
update_manifest_url='https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/raw/branch/main/public/launcher-manifest.txt'
launcher_version='0.4.0'

game_path="${SUDEKIMP_GAME:-${HOME}/Games/SudekiMP/working/SUDEKI.exe}"
wine_prefix="${SUDEKIMP_WINEPREFIX:-${HOME}/Games/sudeki-research-prefix}"
controller_path="${SUDEKIMP_INPUT_DEVICE:-/dev/input/js0}"
controller_p3_path="${SUDEKIMP_INPUT_DEVICE_P3:-/dev/input/js1}"
lan_host="${SUDEKIMP_LAN_ARENA_HOST:-127.0.0.1}"
lan_port="${SUDEKIMP_LAN_ARENA_PORT:-26770}"
auto_update='false'
cleanroom_tools='true'
packaged_launcher='false'
[[ -f "${project_dir}/.sudekimp-package" ]] && packaged_launcher='true'

readonly app_title="SudekiMP Launcher"

zenity_app() { zenity --window-icon="${project_icon}" "$@"; }

usage() {
    cat <<'EOF'
Usage: tools/sudekimp-beta-launcher.sh [--help|--terminal]

Open the SudekiMP launcher for local co-op, LAN arena, and cleanroom modes.
--terminal uses the accessible terminal menu.
EOF
}

load_settings() {
    local -a values=()
    [[ -f "${settings_file}" ]] || return 0
    mapfile -t values < "${settings_file}"
    (( ${#values[@]} >= 1 )) && [[ -n "${values[0]}" ]] && game_path="${values[0]}"
    (( ${#values[@]} >= 2 )) && [[ -n "${values[1]}" ]] && wine_prefix="${values[1]}"
    (( ${#values[@]} >= 3 )) && [[ -n "${values[2]}" ]] && controller_path="${values[2]}"
    (( ${#values[@]} >= 4 )) && [[ -n "${values[3]}" ]] && controller_p3_path="${values[3]}"
    (( ${#values[@]} >= 5 )) && [[ -n "${values[4]}" ]] && lan_host="${values[4]}"
    (( ${#values[@]} >= 6 )) && [[ -n "${values[5]}" ]] && lan_port="${values[5]}"
    (( ${#values[@]} >= 7 )) && [[ "${values[6]}" == 'true' ]] && auto_update='true'
    (( ${#values[@]} >= 8 )) && [[ "${values[7]}" == 'false' ]] && cleanroom_tools='false'
}

save_settings() {
    local temporary_file
    mkdir -p "${settings_dir}"
    temporary_file="$(mktemp "${settings_dir}/.settings.XXXXXX")"
    printf '%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n' \
        "${game_path}" "${wine_prefix}" "${controller_path}" \
        "${controller_p3_path}" "${lan_host}" "${lan_port}" \
        "${auto_update}" "${cleanroom_tools}" > "${temporary_file}"
    chmod 600 "${temporary_file}"
    mv -f "${temporary_file}" "${settings_file}"
}

mode_argument() {
    case "$1" in
        "Local co-op — 2 players") printf '%s\n' '--party-lifecycle-trace' ;;
        "Local co-op — 3 players") printf '%s\n' '--three-player-local-coop-test' ;;
        "LAN arena — Host as Tal") printf '%s\n' '--lan-arena-host' ;;
        "LAN arena — Join as Ailish") printf '%s\n' '--lan-arena-client' ;;
        "Cleanroom") printf '%s\n' '--cleanroom' ;;
        "Safe launch") printf '%s\n' '--safe' ;;
        "Verify installation") printf '%s\n' '--check' ;;
        *) return 1 ;;
    esac
}

mode_summary() {
    case "$1" in
        --party-lifecycle-trace) printf '%s' 'Two local players: keyboard/mouse host plus Player 2 controller.' ;;
        --three-player-local-coop-test) printf '%s' 'Three local players: keyboard/mouse host plus distinct Player 2 and Player 3 controllers.' ;;
        --lan-arena-host) printf '%s' "Host the save-free cleanroom arena as Tal on UDP ${lan_port}." ;;
        --lan-arena-client) printf '%s' "Join ${lan_host}:${lan_port} as Ailish in a separate full-screen process." ;;
        --cleanroom)
            if [[ "${cleanroom_tools}" == true ]]; then
                printf '%s' 'Start the save-free cleanroom with F8 sandbox tools: actors, Training Dummy, combat/camera modes, full inventory, and infinite meters.'
            else
                printf '%s' 'Start the save-free cleanroom with the F8 sandbox tools disabled.'
            fi
            ;;
        --safe) printf '%s' 'Start Sudeki with optional multiplayer hooks disabled.' ;;
        --check) printf '%s' 'Build and validate the exact supported executable and DLL without starting Sudeki.' ;;
    esac
}

environment_summary() {
    printf 'Game: %s\nWine prefix: %s\nP2: %s\nP3: %s\nLAN: %s:%s' \
        "${game_path}" "${wine_prefix}" "${controller_path}" \
        "${controller_p3_path}" "${lan_host}" "${lan_port}"
}

validate_selection() {
    local mode="$1"
    [[ -f "${game_path}" ]] || { zenity_app --error --title="${app_title}" --text="SUDEKI.exe was not found:\n${game_path}"; return 1; }
    [[ -d "${wine_prefix}" ]] || { zenity_app --error --title="${app_title}" --text="Wine prefix was not found:\n${wine_prefix}"; return 1; }
    if [[ "${mode}" == '--party-lifecycle-trace' || "${mode}" == '--three-player-local-coop-test' ]] && [[ ! -r "${controller_path}" ]]; then
        zenity_app --error --title="${app_title}" --text="Player 2 controller is not readable:\n${controller_path}"
        return 1
    fi
    if [[ "${mode}" == '--three-player-local-coop-test' ]] && [[ ! -r "${controller_p3_path}" ]]; then
        zenity_app --error --title="${app_title}" --text="Player 3 controller is not readable:\n${controller_p3_path}"
        return 1
    fi
    if [[ "${mode}" == '--lan-arena-client' && ! "${lan_host}" =~ ^[0-9]{1,3}(\.[0-9]{1,3}){3}$ ]]; then
        zenity_app --error --title="${app_title}" --text="Enter a direct IPv4 address for the LAN host."
        return 1
    fi
    if [[ ! "${lan_port}" =~ ^[0-9]+$ ]] || (( lan_port < 1024 || lan_port > 65535 )); then
        zenity_app --error --title="${app_title}" --text="LAN port must be between 1024 and 65535."
        return 1
    fi
}

configure_gui() {
    local choice selected
    while true; do
        choice="$(zenity_app --list --title="${app_title} — Settings" --width=900 --height=470 \
            --text="$(environment_summary)\n\nUpdate checks never install silently; a newer release always requires confirmation." \
            --column='Setting' --column='Current value' \
            'Game executable' "${game_path}" \
            'Wine prefix' "${wine_prefix}" \
            'Player 2 controller' "${controller_path}" \
            'Player 3 controller' "${controller_p3_path}" \
            'LAN host address' "${lan_host}" \
            'LAN port' "${lan_port}" \
            'Enable cleanroom sandbox tools (F8)' "${cleanroom_tools}" \
            'Check for updates on startup' "${auto_update}" \
            'Back' 'Save and return')" || return
        case "${choice}" in
            'Game executable') selected="$(zenity_app --file-selection --title='Choose SUDEKI.exe' --filename="${game_path}")" || continue; game_path="${selected}" ;;
            'Wine prefix') selected="$(zenity_app --file-selection --directory --title='Choose Wine prefix' --filename="${wine_prefix}")" || continue; wine_prefix="${selected}" ;;
            'Player 2 controller') selected="$(zenity_app --file-selection --title='Choose Player 2 controller' --filename="${controller_path}")" || continue; controller_path="${selected}" ;;
            'Player 3 controller') selected="$(zenity_app --file-selection --title='Choose Player 3 controller' --filename="${controller_p3_path}")" || continue; controller_p3_path="${selected}" ;;
            'LAN host address') selected="$(zenity_app --entry --title="${app_title}" --text='Direct IPv4 address' --entry-text="${lan_host}")" || continue; lan_host="${selected}" ;;
            'LAN port') selected="$(zenity_app --entry --title="${app_title}" --text='UDP port' --entry-text="${lan_port}")" || continue; lan_port="${selected}" ;;
            'Enable cleanroom sandbox tools (F8)') [[ "${cleanroom_tools}" == true ]] && cleanroom_tools=false || cleanroom_tools=true ;;
            'Check for updates on startup') [[ "${auto_update}" == true ]] && auto_update=false || auto_update=true ;;
            'Back') save_settings; return ;;
        esac
        save_settings
    done
}

run_check_gui() {
    local result_file status
    result_file="$(mktemp)"
    if env SUDEKIMP_GAME="${game_path}" SUDEKIMP_WINEPREFIX="${wine_prefix}" "${research_launcher}" --check >"${result_file}" 2>&1; then
        status='Validation passed'
    else
        status='Validation failed'
    fi
    zenity_app --text-info --title="${app_title} — ${status}" --width=900 --height=580 --filename="${result_file}"
    rm -f -- "${result_file}"
}

launch_mode_gui() {
    local mode="$1" process_id
    [[ "${mode}" == '--check' ]] && { run_check_gui; return; }
    validate_selection "${mode}" || return
    zenity_app --question --title="${app_title}" --width=680 --ok-label='Launch' --cancel-label='Back' \
        --text="<b>$(mode_summary "${mode}")</b>\n\n$(environment_summary)\n\nRuntime output: ${launch_log}" || return
    mkdir -p "$(dirname -- "${launch_log}")"
    {
        printf '=== SudekiMP %s %s ===\n' "${mode}" "$(date --iso-8601=seconds)"
        env SUDEKIMP_GAME="${game_path}" \
            SUDEKIMP_WINEPREFIX="${wine_prefix}" \
            SUDEKIMP_SKIP_BUILD="${packaged_launcher}" \
            SUDEKIMP_PUBLIC_LAUNCHER=true \
            SUDEKIMP_DISABLE_OBS_GAMECAPTURE=true \
            SUDEKIMP_INPUT_DEVICE="${controller_path}" \
            SUDEKIMP_INPUT_DEVICE_P3="${controller_p3_path}" \
            SUDEKIMP_LAN_ARENA_HOST="${lan_host}" \
            SUDEKIMP_LAN_ARENA_PORT="${lan_port}" \
            SUDEKIMP_CLEANROOM_TOOLS="${cleanroom_tools}" \
            "${research_launcher}" "${mode}"
        printf '=== launcher exit %s ===\n' "$?"
    } >>"${launch_log}" 2>&1 &
    process_id=$!
    disown "${process_id}" 2>/dev/null || true
    zenity_app --info --title="${app_title}" --text="Launch requested. Sudeki should open shortly.\n\nUse View recent log if it does not."
}

stop_game_gui() {
    zenity_app --question --title="${app_title}" --ok-label='Stop Sudeki' --cancel-label='Cancel' \
        --text='Stop Sudeki and its Wine session now? Unsaved game progress will be lost.' || return
    env SUDEKIMP_WINEPREFIX="${wine_prefix}" "${stop_launcher}" >>"${launch_log}" 2>&1 || true
    zenity_app --info --title="${app_title}" --text='Stop request completed.'
}

view_recent_log_gui() {
    local runtime_log recent
    runtime_log="$(dirname -- "${game_path}")/SudekiMP.log"
    recent="$(mktemp)"
    {
        printf '=== Launcher log ===\n'
        [[ -f "${launch_log}" ]] && tail -n 1200 -- "${launch_log}"
        printf '\n=== SudekiMP runtime log (latest 5000 lines) ===\n'
        [[ -f "${runtime_log}" ]] && tail -n 5000 -- "${runtime_log}"
    } >"${recent}"
    zenity_app --text-info --title="${app_title} — Recent logs" --width=1000 --height=700 --filename="${recent}"
    rm -f -- "${recent}"
}

export_logs_gui() {
    local destination runtime_log bundle_temp bundle_name archive
    destination="$(zenity_app --file-selection --directory --title='Choose where to save the support bundle')" || return
    runtime_log="$(dirname -- "${game_path}")/SudekiMP.log"
    bundle_temp="$(mktemp -d)"
    bundle_name="SudekiMP-support-$(date +%Y%m%d-%H%M%S)"
    mkdir -p "${bundle_temp}/${bundle_name}"
    [[ -f "${runtime_log}" ]] && tail -n 20000 -- "${runtime_log}" >"${bundle_temp}/${bundle_name}/SudekiMP-recent.log"
    [[ -f "${launch_log}" ]] && cp -- "${launch_log}" "${bundle_temp}/${bundle_name}/launcher.log"
    [[ -f "${project_dir}/build/mingw32/bin/SudekiMP.ini" ]] && cp -- "${project_dir}/build/mingw32/bin/SudekiMP.ini" "${bundle_temp}/${bundle_name}/SudekiMP.ini"
    printf 'launcher_version=%s\nmode_user_selected=true\ngame=%s\nwine_prefix=%s\nlan=%s:%s\n' \
        "${launcher_version}" "${game_path}" "${wine_prefix}" "${lan_host}" "${lan_port}" \
        >"${bundle_temp}/${bundle_name}/launcher-summary.txt"
    archive="${destination}/${bundle_name}.tar.gz"
    tar -C "${bundle_temp}" -czf "${archive}" "${bundle_name}"
    rm -rf -- "${bundle_temp:?}"
    zenity_app --info --title="${app_title}" --width=650 \
        --text="Support bundle saved:\n${archive}\n\nAutomatic upload is intentionally not enabled yet. Send this archive manually when requested."
}

check_updates_gui() {
    local quiet="${1:-false}" manifest remote_version release_url
    command -v curl >/dev/null 2>&1 || { [[ "${quiet}" == true ]] || zenity_app --error --title="${app_title}" --text='curl is required to check for updates.'; return; }
    manifest="$(curl --fail --silent --show-error --location "${update_manifest_url}" 2>/dev/null)" || { [[ "${quiet}" == true ]] || zenity_app --error --title="${app_title}" --text='The official release channel could not be reached. Nothing was changed.'; return; }
    remote_version="$(sed -n 's/^version=//p' <<<"${manifest}" | head -1)"
    release_url="$(sed -n 's/^release_url=//p' <<<"${manifest}" | head -1)"
    [[ -n "${remote_version}" && -n "${release_url}" ]] || { [[ "${quiet}" == true ]] || zenity_app --error --title="${app_title}" --text='The update manifest is malformed.'; return; }
    if [[ "$(printf '%s\n%s\n' "${launcher_version}" "${remote_version}" | sort -V | tail -1)" == "${launcher_version}" ]]; then
        [[ "${quiet}" == true ]] || zenity_app --info --title="${app_title}" --text="SudekiMP Launcher ${launcher_version} is current."
        return
    fi
    zenity_app --question --title="${app_title}" --ok-label='Open download page' --cancel-label='Later' \
        --text="SudekiMP ${remote_version} is available.\n\nUpdates are never installed silently. Open the official download page?" || return
    xdg-open "${release_url}" >/dev/null 2>&1 &
}

show_about() {
    zenity_app --info --title="${app_title}" --width=700 \
        --text="<b>SudekiMP Launcher ${launcher_version}</b>\n\nLocal co-op, direct-IP LAN arena, and cleanroom profiles share one guarded launcher. Campaign saves are never used by LAN arena. Talos research profiles are intentionally not exposed.\n\nLog upload is not implemented: Export support logs creates a reviewable archive for manual sharing. Update checks are opt-in and always ask before opening a download."
}

run_terminal_menu() {
    local choice mode
    printf '\n%s\n' "${app_title} ${launcher_version}"
    printf '%s\n' '1) Local co-op (2 players)' '2) Local co-op (3 players)' '3) LAN host' '4) LAN client' '5) Cleanroom' '6) Safe launch' '7) Verify' '8) Stop Sudeki' '9) Quit'
    read -r -p 'Choose [1-9]: ' choice
    case "${choice}" in
        1) mode='--party-lifecycle-trace' ;; 2) mode='--three-player-local-coop-test' ;;
        3) mode='--lan-arena-host' ;; 4) mode='--lan-arena-client' ;;
        5) mode='--cleanroom' ;; 6) mode='--safe' ;; 7) mode='--check' ;;
        8) env SUDEKIMP_WINEPREFIX="${wine_prefix}" "${stop_launcher}"; return ;;
        9) return ;; *) return 2 ;;
    esac
    env SUDEKIMP_GAME="${game_path}" SUDEKIMP_WINEPREFIX="${wine_prefix}" \
        SUDEKIMP_SKIP_BUILD="${packaged_launcher}" \
        SUDEKIMP_PUBLIC_LAUNCHER=true SUDEKIMP_DISABLE_OBS_GAMECAPTURE=true \
        SUDEKIMP_INPUT_DEVICE="${controller_path}" SUDEKIMP_INPUT_DEVICE_P3="${controller_p3_path}" \
        SUDEKIMP_LAN_ARENA_HOST="${lan_host}" SUDEKIMP_LAN_ARENA_PORT="${lan_port}" \
        SUDEKIMP_CLEANROOM_TOOLS="${cleanroom_tools}" \
        "${research_launcher}" "${mode}"
}

run_gui() {
    local choice mode dialog_status
    [[ "${auto_update}" == true ]] && check_updates_gui true
    while true; do
        choice="$(zenity_app --list --title="${app_title}" --width=980 --height=610 \
            --text="<b>Choose a SudekiMP profile or launcher tool</b>\n\nLAN arena uses no campaign saves. Talos research flags are not part of this launcher." \
            --column='Option' --column='What it does' \
            'Local co-op — 2 players' 'Keyboard/mouse host plus one local controller.' \
            'Local co-op — 3 players' 'Keyboard/mouse host plus two distinct local controllers.' \
            'LAN arena — Host as Tal' 'Host-authoritative cleanroom arena on a direct UDP port.' \
            'LAN arena — Join as Ailish' 'Join a host with a separate full-screen camera and HUD.' \
            'Cleanroom' 'Save-free sandbox; optional F8 tools spawn actors/dummy and expose combat, camera, inventory, and infinite-meter controls.' \
            'Safe launch' 'Launch with optional hooks disabled.' \
            'Verify installation' 'Validate the supported build without starting the game.' \
            'Stop Sudeki' 'Stop the configured Wine Sudeki session.' \
            'View recent log' 'Read bounded launcher and runtime log tails.' \
            'Export support logs' 'Create a local archive for manual sharing.' \
            'Check for updates' 'Check the official manifest; never install silently.' \
            'Settings' 'Game, Wine, controllers, LAN, cleanroom tools, and opt-in update checks.' \
            'About' 'Scope and safety information.' \
            --ok-label='Continue' --cancel-label='Quit')"
        dialog_status=$?
        (( dialog_status != 0 )) && [[ -z "${choice}" ]] && return
        case "${choice}" in
            'Settings') configure_gui ;; 'Stop Sudeki') stop_game_gui ;;
            'View recent log') view_recent_log_gui ;; 'Export support logs') export_logs_gui ;;
            'Check for updates') check_updates_gui false ;; 'About') show_about ;;
            *) mode="$(mode_argument "${choice}")" || continue; launch_mode_gui "${mode}" ;;
        esac
    done
}

main() {
    local force_terminal=false
    case "${1:-}" in --help|-h) usage; return ;; --terminal) force_terminal=true ;; '') ;; *) usage >&2; return 2 ;; esac
    load_settings
    if [[ "${force_terminal}" == true ]] || ! command -v zenity >/dev/null 2>&1 || { [[ -z "${DISPLAY:-}" ]] && [[ -z "${WAYLAND_DISPLAY:-}" ]]; }; then
        run_terminal_menu
    else
        run_gui
    fi
}

main "$@"
