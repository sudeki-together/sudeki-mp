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

game_path="${SUDEKIMP_GAME:-${HOME}/Games/SudekiMP/working/SUDEKI.exe}"
wine_prefix="${SUDEKIMP_WINEPREFIX:-${HOME}/Games/sudeki-research-prefix}"
controller_path="${SUDEKIMP_INPUT_DEVICE:-/dev/input/js0}"

readonly app_title="SudekiMP Local Co-op"

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
                'The supported two-player local co-op profile. Player 1 uses keyboard/mouse; Player 2 uses the selected Linux controller. Campaign transitions remain host-led.'
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
    zenity --info --title="${app_title}" --width=620 \
        --text="<b>Linux local co-op beta launcher</b>\n\nThis app starts the existing guarded SudekiMP profiles; it does not patch SUDEKI.exe or copy game files.\n\nThe co-op beta is one local game process: Player 1 is keyboard/mouse and Player 2 is a Linux controller. Menus, save books, inventory, and merchant checkout are still shared native systems.\n\nUse Settings to choose the game executable, Wine prefix, and controller device."
}

configure_gui() {
    local choice selected

    while true; do
        choice="$(zenity --list --title="${app_title} — Settings" --width=820 --height=350 \
            --text="Choose what to change.\n\n$(environment_summary)" \
            --column="Setting" --column="Current value" \
            "Game executable" "${game_path}" \
            "Wine prefix" "${wine_prefix}" \
            "Player 2 controller" "${controller_path}" \
            "Reset to defaults" "Use the project defaults" \
            "Back" "Return to play options")" || return

        case "${choice}" in
            "Game executable")
                selected="$(zenity --file-selection --title="Choose SUDEKI.exe" \
                    --filename="${game_path}")" || continue
                [[ -n "${selected}" ]] && game_path="${selected}"
                ;;
            "Wine prefix")
                selected="$(zenity --file-selection --directory --title="Choose Wine prefix" \
                    --filename="${wine_prefix}")" || continue
                [[ -n "${selected}" ]] && wine_prefix="${selected}"
                ;;
            "Player 2 controller")
                selected="$(zenity --file-selection --title="Choose controller device (usually /dev/input/js0)" \
                    --filename="${controller_path}")" || continue
                [[ -n "${selected}" ]] && controller_path="${selected}"
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
        zenity --error --title="${app_title}" \
            --text="SUDEKI.exe was not found:\n${game_path}\n\nUse Settings to choose your owned GOG game executable."
        return 1
    fi
    if [[ ! -d "${wine_prefix}" ]]; then
        zenity --error --title="${app_title}" \
            --text="Wine prefix directory was not found:\n${wine_prefix}\n\nUse Settings to choose the prefix that contains Sudeki."
        return 1
    fi
    if mode_needs_controller "${mode}" && [[ ! -r "${controller_path}" ]]; then
        zenity --error --title="${app_title}" \
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
    zenity --text-info --title="${app_title} — ${status}" --width=850 --height=560 \
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

    if ! zenity --question --title="${app_title}" --width=650 \
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

    zenity --info --title="${app_title}" --width=620 \
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
    local choice mode

    while true; do
        choice="$(zenity --list --title="${app_title}" --width=900 --height=460 \
            --text="<b>Choose how to start SudekiMP</b>\n\nTwo-player local co-op is the supported beta path. Select Settings if your game, Wine prefix, or controller differs from the defaults." \
            --column="Option" --column="What it does" \
            "Play local co-op beta" "Two local players: keyboard/mouse host plus Linux controller Player 2." \
            "Safe launch" "Start with optional co-op prototypes disabled." \
            "Talos party encounter" "Restore Tal's companions for the focused Talos encounter." \
            "Verify installation" "Build and check the exact supported game/DLL pair without launching." \
            --ok-label="Continue" --cancel-label="Quit" \
            --extra-button="Settings" --extra-button="About")" || return

        case "${choice}" in
            "Settings") configure_gui ;;
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

main "$@"
