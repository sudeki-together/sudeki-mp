#!/usr/bin/env bash
set -euo pipefail

# A small desktop front end for the guarded research launcher.  It deliberately
# delegates every game-facing action to continue-research.sh so the exact-build
# checks and per-mode cleanup policy remain in one place.

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
research_launcher="${script_dir}/continue-research.sh"
settings_dir="${XDG_CONFIG_HOME:-${HOME}/.config}/sudekimp-beta-launcher"
settings_file="${settings_dir}/settings"
launch_log="${project_dir}/build/linux/beta-launcher.log"
project_icon="${project_dir}/src/launcher/assets/SudekiMP.png"
music_manifest_url='https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/raw/branch/main/public/music/manifest.txt'
music_track_url='https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/raw/branch/main/public/music/Map%20Inversion.mp3'
music_pid=''

game_path="${SUDEKIMP_GAME:-${HOME}/Games/SudekiMP/working/SUDEKI.exe}"
wine_prefix="${SUDEKIMP_WINEPREFIX:-${HOME}/Games/sudeki-research-prefix}"
controller_path="${SUDEKIMP_INPUT_DEVICE:-/dev/input/js0}"

readonly app_title="SudekiMP Local Co-op"

zenity_app() {
    zenity --window-icon="${project_icon}" "$@"
}

usage() {
    cat <<'EOF'
Usage: tools/sudekimp-beta-launcher.sh [--help|--terminal]

Open the Linux graphical launcher for the supported local co-op profiles.
--terminal forces the accessible terminal fallback.
EOF
}

load_settings() {
    local -a values=()

    if [[ -f "${settings_file}" ]]; then
        mapfile -t values < "${settings_file}"
        if (( ${#values[@]} == 3 )); then
            [[ -n "${values[0]}" ]] && game_path="${values[0]}"
            [[ -n "${values[1]}" ]] && wine_prefix="${values[1]}"
            [[ -n "${values[2]}" ]] && controller_path="${values[2]}"
        fi
    fi
}

save_settings() {
    local temporary_file

    mkdir -p "${settings_dir}"
    temporary_file="$(mktemp "${settings_dir}/.settings.XXXXXX")"
    printf '%s\n%s\n%s\n' \
        "${game_path}" "${wine_prefix}" "${controller_path}" > "${temporary_file}"
    chmod 600 "${temporary_file}"
    mv -f "${temporary_file}" "${settings_file}"
}

mode_argument() {
    case "$1" in
        "Play local co-op beta") printf '%s\n' '--party-lifecycle-trace' ;;
        "Safe launch") printf '%s\n' '--safe' ;;
        "Talos party encounter") printf '%s\n' '--talos-party-test' ;;
        "Verify installation") printf '%s\n' '--check' ;;
        *) return 1 ;;
    esac
}

mode_needs_controller() {
    [[ "$1" == '--party-lifecycle-trace' || "$1" == '--talos-party-test' ]]
}

mode_summary() {
    case "$1" in
        --party-lifecycle-trace)
            printf '%s\n' \
                'The supported two-player local co-op profile. Player 1 uses keyboard/mouse; Player 2 uses the selected Linux controller. Campaign transitions remain host-led, and the Talos party restoration is enabled.'
            ;;
        --safe)
            printf '%s\n' \
                'Launch SudekiMP with optional co-op prototypes disabled. Use this to verify that the game and loader start normally.'
            ;;
        --talos-party-test)
            printf '%s\n' \
                "Restore Tal's retail companions after the native Talos encounter rebuild. This is a focused encounter profile, not a general campaign setting."
            ;;
        --check)
            printf '%s\n' \
                'Build and validate the exact supported SUDEKI.exe/DLL pair without starting the game.'
            ;;
    esac
}

environment_summary() {
    printf 'Game: %s\nWine prefix: %s\nP2 controller: %s' \
        "${game_path}" "${wine_prefix}" "${controller_path}"
}

show_about() {
    zenity_app --info --title="${app_title}" --width=620 \
        --text="<b>Linux local co-op beta launcher</b>\n\nThis app starts the existing guarded SudekiMP profiles; it does not patch SUDEKI.exe or copy game files.\n\nThe co-op beta is one local game process: Player 1 is keyboard/mouse and Player 2 is a Linux controller. Menus, save books, inventory, and merchant checkout are still shared native systems.\n\nDeveloper: wander — git.unfilteredrealm.com/wander\n\nUse Settings to choose the game executable, Wine prefix, and controller device."
}

open_developer_page() {
    if command -v xdg-open >/dev/null 2>&1; then
        xdg-open 'https://git.unfilteredrealm.com/wander' >/dev/null 2>&1 &
        return
    fi
    zenity_app --error --title="${app_title}" \
        --text='xdg-open is unavailable, so the developer page could not be opened.'
}

play_project_music() {
    local catalog

    if [[ -n "${music_pid}" ]] && kill -0 "${music_pid}" 2>/dev/null; then
        zenity_app --info --title="${app_title}" \
            --text='Map Inversion is already playing in the launcher session.'
        return
    fi
    if ! command -v curl >/dev/null 2>&1 || ! command -v ffplay >/dev/null 2>&1; then
        zenity_app --error --title="${app_title}" \
            --text='Project music needs curl and ffplay (from FFmpeg). The game launcher still works without them.'
        return
    fi
    if ! catalog="$(curl --fail --silent --show-error --location "${music_manifest_url}")" || \
        ! grep -Fqx 'track=Map Inversion.mp3' <<< "${catalog}"; then
        zenity_app --error --title="${app_title}" \
            --text='The public SudekiMP music catalog could not be read. No game files were changed.'
        return
    fi
    ffplay -nodisp -autoexit -loglevel error "${music_track_url}" \
        >> "${launch_log}" 2>&1 &
    music_pid=$!
    zenity_app --info --title="${app_title}" \
        --text='Playing Map Inversion inside the launcher session. Choose Stop music to end it.'
}

stop_project_music() {
    if [[ -n "${music_pid}" ]] && kill -0 "${music_pid}" 2>/dev/null; then
        kill "${music_pid}" 2>/dev/null || true
    fi
    music_pid=''
}

paste_paths_gui() {
    local selected

    selected="$(zenity_app --entry --title="${app_title} — Game executable" \
        --text='Paste the full path to SUDEKI.exe.' --entry-text="${game_path}")" || return
    [[ -n "${selected}" ]] && game_path="${selected}"
    selected="$(zenity_app --entry --title="${app_title} — Wine prefix" \
        --text='Paste the Wine prefix directory.' --entry-text="${wine_prefix}")" || return
    [[ -n "${selected}" ]] && wine_prefix="${selected}"
    selected="$(zenity_app --entry --title="${app_title} — Player 2 controller" \
        --text='Paste the controller device path (usually /dev/input/js0).' --entry-text="${controller_path}")" || return
    [[ -n "${selected}" ]] && controller_path="${selected}"
    save_settings
}

configure_gui() {
    local choice selected

    while true; do
        choice="$(zenity_app --list --title="${app_title} — Settings" --width=820 --height=380 \
            --text="Choose what to change.\n\n$(environment_summary)" \
            --column="Setting" --column="Current value" \
            "Game executable" "${game_path}" \
            "Wine prefix" "${wine_prefix}" \
            "Player 2 controller" "${controller_path}" \
            "Paste paths…" "Enter all three paths directly from the keyboard or clipboard" \
            "Reset to defaults" "Use the project defaults" \
            "Back" "Return to play options")" || return

        case "${choice}" in
            "Game executable")
                selected="$(zenity_app --file-selection --title="Choose SUDEKI.exe" \
                    --filename="${game_path}")" || continue
                [[ -n "${selected}" ]] && game_path="${selected}"
                ;;
            "Wine prefix")
                selected="$(zenity_app --file-selection --directory --title="Choose Wine prefix" \
                    --filename="${wine_prefix}")" || continue
                [[ -n "${selected}" ]] && wine_prefix="${selected}"
                ;;
            "Player 2 controller")
                selected="$(zenity_app --file-selection --title="Choose controller device (usually /dev/input/js0)" \
                    --filename="${controller_path}")" || continue
                [[ -n "${selected}" ]] && controller_path="${selected}"
                ;;
            "Paste paths…")
                paste_paths_gui
                ;;
            "Reset to defaults")
                game_path="${SUDEKIMP_GAME:-${HOME}/Games/SudekiMP/working/SUDEKI.exe}"
                wine_prefix="${SUDEKIMP_WINEPREFIX:-${HOME}/Games/sudeki-research-prefix}"
                controller_path="${SUDEKIMP_INPUT_DEVICE:-/dev/input/js0}"
                ;;
            "Back")
                save_settings
                return
                ;;
        esac
        save_settings
    done
}

validate_selection() {
    local mode="$1"

    if [[ ! -f "${game_path}" ]]; then
        zenity_app --error --title="${app_title}" \
            --text="SUDEKI.exe was not found:\n${game_path}\n\nUse Settings to choose your owned GOG game executable."
        return 1
    fi
    if [[ ! -d "${wine_prefix}" ]]; then
        zenity_app --error --title="${app_title}" \
            --text="Wine prefix directory was not found:\n${wine_prefix}\n\nUse Settings to choose the prefix that contains Sudeki."
        return 1
    fi
    if mode_needs_controller "${mode}" && [[ ! -r "${controller_path}" ]]; then
        zenity_app --error --title="${app_title}" \
            --text="The Player 2 controller is not readable:\n${controller_path}\n\nConnect it, grant your user input-device access, then choose it in Settings."
        return 1
    fi
    return 0
}

run_check_gui() {
    local result_file status

    result_file="$(mktemp)"
    if env \
        "SUDEKIMP_GAME=${game_path}" \
        "SUDEKIMP_WINEPREFIX=${wine_prefix}" \
        "SUDEKIMP_INPUT_DEVICE=${controller_path}" \
        "${research_launcher}" --check > "${result_file}" 2>&1; then
        status="Validation passed"
    else
        status="Validation failed"
    fi
    zenity_app --text-info --title="${app_title} — ${status}" --width=850 --height=560 \
        --filename="${result_file}"
    rm -f "${result_file}"
}

launch_mode_gui() {
    local mode="$1" process_id

    if [[ "${mode}" == '--check' ]]; then
        run_check_gui
        return
    fi
    validate_selection "${mode}" || return

    if ! zenity_app --question --title="${app_title}" --width=650 \
        --ok-label="Launch" --cancel-label="Back" \
        --text="<b>$(mode_summary "${mode}")</b>\n\n$(environment_summary)\n\nThe game will open in a separate Wine window. The launcher log is written to:\n${launch_log}"; then
        return
    fi

    mkdir -p "$(dirname -- "${launch_log}")"
    {
        printf '%s\n' "=== SudekiMP launcher started $(date --iso-8601=seconds) ==="
        printf 'Mode: %s\n' "${mode}"
        env \
            "SUDEKIMP_GAME=${game_path}" \
            "SUDEKIMP_WINEPREFIX=${wine_prefix}" \
            "SUDEKIMP_INPUT_DEVICE=${controller_path}" \
            "${research_launcher}" "${mode}"
        printf '=== SudekiMP launcher exited with status %s ===\n' "$?"
    } >> "${launch_log}" 2>&1 &
    process_id=$!
    disown "${process_id}" 2>/dev/null || true

    zenity_app --info --title="${app_title}" --width=620 \
        --text="Launch started. Sudeki should open shortly.\n\nPlayer 1: keyboard and mouse\nPlayer 2: ${controller_path}\n\nIf the game does not open, see:\n${launch_log}"
}

run_terminal_menu() {
    local choice mode

    printf '\n%s\n' "${app_title}"
    printf '%s\n' '1) Play local co-op beta'
    printf '%s\n' '2) Safe launch'
    printf '%s\n' '3) Talos party encounter'
    printf '%s\n' '4) Verify installation'
    printf '%s\n' '5) Quit'
    printf 'Choose an option [1-5]: '
    read -r choice
    case "${choice}" in
        1) mode='--party-lifecycle-trace' ;;
        2) mode='--safe' ;;
        3) mode='--talos-party-test' ;;
        4) mode='--check' ;;
        5) return 0 ;;
        *) printf '%s\n' 'Invalid choice.' >&2; return 2 ;;
    esac
    printf '%s\n\n' "$(mode_summary "${mode}")"
    env \
        "SUDEKIMP_GAME=${game_path}" \
        "SUDEKIMP_WINEPREFIX=${wine_prefix}" \
        "SUDEKIMP_INPUT_DEVICE=${controller_path}" \
        "${research_launcher}" "${mode}"
}

run_gui() {
    local choice mode dialog_status

    while true; do
        choice="$(zenity_app --list --title="${app_title}" --width=900 --height=470 \
            --text="<b>Choose how to start SudekiMP</b>\n\nTwo-player local co-op is the supported beta path. Select Settings if your game, Wine prefix, or controller differs from the defaults." \
            --column="Option" --column="What it does" \
            "Play local co-op beta" "Two local players: keyboard/mouse host plus Linux controller Player 2." \
            "Safe launch" "Start with optional co-op prototypes disabled." \
            "Talos party encounter" "Restore Tal's companions for the focused Talos encounter." \
            "Verify installation" "Build and check the exact supported game/DLL pair without launching." \
            "Settings" "Choose or paste the game, Wine-prefix, and controller paths." \
            "Play music" "Stream Map Inversion inside this launcher session." \
            "Stop music" "Stop the current project-music stream." \
            "Developer: wander" "Open the Sudeki Together developer page." \
            "About" "Read the local co-op beta scope and safety notes." \
            --ok-label="Continue" --cancel-label="Quit")"
        dialog_status=$?
        if (( dialog_status != 0 )) && [[ -z "${choice}" ]]; then
            return
        fi

        case "${choice}" in
            "Settings") configure_gui ;;
            "Play music") play_project_music ;;
            "Stop music") stop_project_music ;;
            "Developer: wander") open_developer_page ;;
            "About") show_about ;;
            *)
                mode="$(mode_argument "${choice}")" || continue
                launch_mode_gui "${mode}"
                ;;
        esac
    done
}

main() {
    local force_terminal=false

    case "${1:-}" in
        --help|-h) usage; return 0 ;;
        --terminal) force_terminal=true ;;
        '') ;;
        *) usage >&2; return 2 ;;
    esac

    load_settings
    if [[ "${force_terminal}" == true ]] || ! command -v zenity >/dev/null 2>&1 || \
        { [[ -z "${DISPLAY:-}" ]] && [[ -z "${WAYLAND_DISPLAY:-}" ]]; }; then
        run_terminal_menu
    else
        run_gui
    fi
}

trap stop_project_music EXIT
main "$@"
