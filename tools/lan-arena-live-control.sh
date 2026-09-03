#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
stage_root="${project_dir}/build/mingw32/lan-loopback"
host_log="${stage_root}/host-runtime.log"
client_log="${stage_root}/client-runtime.log"
trace_log="${stage_root}/live-control.log"
host_prefix="${SUDEKIMP_LAN_HOST_WINEPREFIX:-${HOME}/Games/sudeki-lan-host-win32-prefix}"
client_prefix="${SUDEKIMP_LAN_CLIENT_WINEPREFIX:-${HOME}/Games/sudeki-lan-client-win32-prefix}"
operator_exe="${project_dir}/build/mingw32/bin/SudekiMP.LanArenaOperator.exe"
host_title='Sudeki LAN Host - Tal'
client_title='Sudeki LAN Client - Ailish'
action="${1:---status}"
duration_ms="${2:-250}"
held_key=''
held_button=''
operator_weak_held=''

release_synthetic_input() {
    if [[ -n "${held_key}" ]]; then
        xdotool keyup --clearmodifiers "${held_key}" >/dev/null 2>&1 || true
        held_key=''
    fi
    if [[ -n "${held_button}" ]]; then
        xdotool mouseup "${held_button}" >/dev/null 2>&1 || true
        held_button=''
    fi
    if [[ -n "${operator_weak_held}" ]]; then
        run_operator "${client_prefix}" weak-up >/dev/null 2>&1 || true
        operator_weak_held=''
    fi
}

trap release_synthetic_input EXIT INT TERM

usage() {
    printf '%s\n' \
        'usage: tools/lan-arena-live-control.sh ACTION [duration-ms]' \
        '' \
        'Actions:' \
        '  --status                 Show exact LAN windows and recent state' \
        '  --skip-startup           Tap Escape in both exact LAN windows' \
        '  --advance-startup        Give each startup screen 3 focused seconds' \
        '  --advance-host [ms]      Give only the host startup focused time' \
        '  --advance-client [ms]    Give only the client startup focused time' \
        '  --focus-host [ms]        Give host foreground time without input' \
        '  --focus-client [ms]      Give client foreground time without input' \
        '  --host-confirm           Tap Enter in the exact host window' \
        '  --client-confirm         Tap Enter in the exact client window' \
        '  --client-combat          Queue client combat toggle in-process' \
        '  --client-fire [ms]       Queue one client weak-fire pulse' \
        '  --client-fire-hold [ms]  Hold client fire through the local operator API' \
        '  --client-fire-capture [ms]  Queue fire and capture after delay' \
        '  --host-weak              Queue one native Tal weak transition' \
        '  --host-combo [ms]        Queue three timed Tal weak transitions' \
        '  --host-sequence PATTERN [ms]  Queue Tal W/S transitions (for example WWS)' \
        '  --host-combo-capture [ms]  Queue three Tal weak transitions/captures' \
        '  --host-strong            Queue one native Tal strong transition' \
        '  --host-sweep             Queue one native Tal sweep transition' \
        '  --host-block [ms]        Queue block and observe for duration' \
        '  --client-forward [ms]    Hold client W (50..3000 ms)' \
        '  --client-turn-left       Queue native client camera-left input' \
        '  --client-turn-right      Queue native client camera-right input' \
        '  --capture                Capture both exact LAN windows'
}

case "${action}" in
    --status|--skip-startup|--advance-startup|--advance-host|--advance-client|\
    --focus-host|--focus-client|\
    --host-confirm|--client-confirm|\
    --client-combat|--client-fire|--client-fire-capture|--client-fire-hold|--host-weak|\
    --host-combo|--host-combo-capture|--host-sequence|--host-strong|--host-sweep|--host-block|\
    --client-forward|\
    --client-turn-left|--client-turn-right|--capture) ;;
    --help|-h) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
esac

for tool in wmctrl xdotool; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        printf 'Required desktop tool is missing: %s\n' "${tool}" >&2
        exit 127
    fi
done

window_id_for_title() {
    local title="$1"
    local matches
    matches="$(wmctrl -l -p | awk -v exact="${title}" '
        index($0, exact) && substr($0, length($0) - length(exact) + 1) == exact {
            print $1
        }
    ')"
    if [[ "$(printf '%s\n' "${matches}" | sed '/^$/d' | wc -l)" -ne 1 ]]; then
        printf 'Expected exactly one mapped window titled %s.\n' "${title}" >&2
        return 1
    fi
    printf '%s\n' "${matches}"
}

host_window="$(window_id_for_title "${host_title}")"
client_window="$(window_id_for_title "${client_title}")"

trace_action() {
    local detail="$1"
    mkdir -p -- "${stage_root}"
    printf '%s action=%s detail=%s host_window=%s client_window=%s\n' \
        "$(date --iso-8601=seconds)" "${action}" "${detail}" \
        "${host_window}" "${client_window}" >>"${trace_log}"
}

activate_and_tap_key() {
    local window="$1"
    local key="$2"
    xdotool windowactivate --sync "${window}"
    # Address the exact Wine toplevel as well as making it foreground.  A
    # desktop-global synthetic key can be swallowed during the unskippable
    # logo transition even after the WM confirms activation; direct delivery
    # is deterministic for both startup and native menu confirmation.
    xdotool key --window "${window}" --clearmodifiers "${key}"
}

center_pointer_in_window() {
    local window="$1"
    local geometry
    local window_x
    local window_y
    local window_width
    local window_height

    geometry="$(xdotool getwindowgeometry --shell "${window}")"
    window_x="$(printf '%s\n' "${geometry}" | awk -F= '$1 == "X" { print $2 }')"
    window_y="$(printf '%s\n' "${geometry}" | awk -F= '$1 == "Y" { print $2 }')"
    window_width="$(printf '%s\n' "${geometry}" | awk -F= '$1 == "WIDTH" { print $2 }')"
    window_height="$(printf '%s\n' "${geometry}" | awk -F= '$1 == "HEIGHT" { print $2 }')"
    if [[ ! "${window_x}" =~ ^-?[0-9]+$ ]] ||
       [[ ! "${window_y}" =~ ^-?[0-9]+$ ]] ||
       [[ ! "${window_width}" =~ ^[0-9]+$ ]] ||
       [[ ! "${window_height}" =~ ^[0-9]+$ ]]; then
        printf 'Could not resolve window geometry for %s.\n' "${window}" >&2
        return 1
    fi
    # Wine continuously recenters a grabbed mouse.  Waiting for an exact
    # pointer position can therefore block until the grab is released; the
    # foreground activation plus a best-effort center warp is sufficient to
    # route the following synthetic button edge to the intended process.
    xdotool mousemove \
        "$((window_x + window_width / 2))" \
        "$((window_y + window_height / 2))"
}

activate_and_center_pointer() {
    local window="$1"

    xdotool windowactivate --sync "${window}"
    center_pointer_in_window "${window}"
}

run_operator() {
    local prefix="$1"
    shift

    if [[ ! -f "${operator_exe}" ]]; then
        printf 'LAN operator executable is missing: %s\n' \
            "${operator_exe}" >&2
        exit 1
    fi
    env WINEPREFIX="${prefix}" wine "${operator_exe}" "$@"
}

activate_and_hold_key() {
    local window="$1"
    local key="$2"
    local milliseconds="$3"
    xdotool windowactivate --sync "${window}"
    held_key="${key}"
    xdotool keydown --clearmodifiers "${key}"
    sleep "$(awk -v value="${milliseconds}" 'BEGIN { printf "%.3f", value / 1000.0 }')"
    xdotool keyup --clearmodifiers "${key}"
    held_key=''
}

validate_duration() {
    if [[ ! "${duration_ms}" =~ ^[0-9]+$ ]] ||
       (( duration_ms < 50 || duration_ms > 3000 )); then
        printf 'Duration must be 50..3000 ms: %s\n' "${duration_ms}" >&2
        exit 2
    fi
}

capture_window() {
    local window="$1"
    local output="$2"
    if ! command -v import >/dev/null 2>&1; then
        printf '%s\n' 'ImageMagick import is required for capture.' >&2
        exit 127
    fi
    import -window "${window}" "${output}"
}

case "${action}" in
    --status)
        wmctrl -l -p | awk -v host="${host_title}" -v client="${client_title}" \
            'index($0, host) || index($0, client)'
        printf '%s\n' '--- host state ---'
        tail -n 300 "${host_log}" 2>/dev/null | rg \
            'host_connected|combat_toggle|host_remote_weak|lan_arena_missile_cadence|process_detach' | tail -n 12 || true
        printf '%s\n' '--- host action journal ---'
        rg 'host_action_sequence' "${host_log}" 2>/dev/null | tail -n 12 || true
        printf '%s\n' '--- client state ---'
        tail -n 400 "${client_log}" 2>/dev/null | rg \
            'client_connected|client_combat_mode|client_combat_presentation|first_person_aim_bridge|client_camera_route|client_combat_graph|process_detach' | tail -n 16 || true
        printf '%s\n' '--- client action presentation ---'
        rg 'client_tal_action_presentation|client_first_person_fire_presentation' \
            "${client_log}" 2>/dev/null | tail -n 12 || true
        ;;
    --skip-startup)
        trace_action 'key=Escape windows=host,client'
        activate_and_tap_key "${host_window}" Escape
        activate_and_tap_key "${client_window}" Escape
        ;;
    --advance-startup)
        trace_action 'key=Escape focused_seconds=3 windows=host,client'
        activate_and_tap_key "${host_window}" Escape
        sleep 3
        activate_and_tap_key "${client_window}" Escape
        sleep 3
        ;;
    --advance-host|--advance-client)
        startup_ms="${2:-6000}"
        if [[ ! "${startup_ms}" =~ ^[0-9]+$ ]] ||
           (( startup_ms < 250 || startup_ms > 30000 )); then
            printf 'Startup focus duration must be 250..30000 ms: %s\n' \
                "${startup_ms}" >&2
            exit 2
        fi
        startup_window="${host_window}"
        [[ "${action}" == '--advance-client' ]] && \
            startup_window="${client_window}"
        trace_action "key=Escape focused_ms=${startup_ms}"
        activate_and_tap_key "${startup_window}" Escape
        sleep "$(awk -v value="${startup_ms}" \
            'BEGIN { printf "%.3f", value / 1000.0 }')"
        ;;
    --focus-host|--focus-client)
        focus_ms="${2:-6000}"
        if [[ ! "${focus_ms}" =~ ^[0-9]+$ ]] ||
           (( focus_ms < 250 || focus_ms > 30000 )); then
            printf 'Focus duration must be 250..30000 ms: %s\n' \
                "${focus_ms}" >&2
            exit 2
        fi
        focus_window="${host_window}"
        [[ "${action}" == '--focus-client' ]] && focus_window="${client_window}"
        trace_action "foreground_only_ms=${focus_ms}"
        xdotool windowactivate --sync "${focus_window}"
        sleep "$(awk -v value="${focus_ms}" \
            'BEGIN { printf "%.3f", value / 1000.0 }')"
        ;;
    --host-confirm|--client-confirm)
        confirm_window="${host_window}"
        [[ "${action}" == '--client-confirm' ]] && \
            confirm_window="${client_window}"
        trace_action 'key=Return'
        activate_and_tap_key "${confirm_window}" Return
        ;;
    --client-combat)
        trace_action 'command=combat-toggle source=local_operator_api'
        run_operator "${client_prefix}" combat-toggle
        ;;
    --client-fire|--client-fire-capture)
        validate_duration
        trace_action "command=weak source=local_operator_api capture_delay_ms=${duration_ms}"
        run_operator "${client_prefix}" weak
        if [[ "${action}" == '--client-fire-capture' ]]; then
            mkdir -p -- "${stage_root}"
            stamp="$(date +%Y%m%d-%H%M%S)"
            sleep "$(awk -v value="${duration_ms}" 'BEGIN { printf "%.3f", value / 1000.0 }')"
            capture_window "${client_window}" \
                "${stage_root}/${stamp}-client-fire.png"
            printf '%s\n' "${stage_root}/${stamp}-client-fire.png"
        fi
        ;;
    --client-fire-hold)
        validate_duration
        trace_action "command=weak-down duration_ms=${duration_ms} source=local_operator_api"
        run_operator "${client_prefix}" weak-hold "${duration_ms}"
        ;;
    --host-weak)
        trace_action 'command=weak source=local_operator_api foreground=host'
        xdotool windowactivate --sync "${host_window}"
        run_operator "${host_prefix}" weak
        ;;
    --host-strong)
        trace_action 'command=strong source=local_operator_api foreground=host'
        xdotool windowactivate --sync "${host_window}"
        run_operator "${host_prefix}" strong
        ;;
    --host-sweep)
        trace_action 'command=sweep source=local_operator_api foreground=host'
        xdotool windowactivate --sync "${host_window}"
        run_operator "${host_prefix}" sweep
        ;;
    --host-block)
        validate_duration
        trace_action "command=block source=local_operator_api foreground=host observation_ms=${duration_ms}"
        xdotool windowactivate --sync "${host_window}"
        run_operator "${host_prefix}" block
        sleep "$(awk -v value="${duration_ms}" 'BEGIN { printf "%.3f", value / 1000.0 }')"
        ;;
    --host-combo)
        combo_interval_ms="${2:-450}"
        if [[ ! "${combo_interval_ms}" =~ ^[0-9]+$ ]] ||
           (( combo_interval_ms < 80 || combo_interval_ms > 750 )); then
            printf 'Combo interval must be 80..750 ms: %s\n' \
                "${combo_interval_ms}" >&2
            exit 2
        fi
        trace_action "command=combo clicks=3 interval_ms=${combo_interval_ms} foreground=host capture=none"
        xdotool windowactivate --sync "${host_window}"
        run_operator "${host_prefix}" combo "${combo_interval_ms}"
        ;;
    --host-sequence)
        combo_pattern="${2:-}"
        combo_interval_ms="${3:-450}"
        if [[ ! "${combo_pattern}" =~ ^[WwSs]{2,6}$ ]]; then
            printf 'Combo pattern must contain 2..6 W/S inputs: %s\n' \
                "${combo_pattern}" >&2
            exit 2
        fi
        if [[ ! "${combo_interval_ms}" =~ ^[0-9]+$ ]] ||
           (( combo_interval_ms < 80 || combo_interval_ms > 750 )); then
            printf 'Combo interval must be 80..750 ms: %s\n' \
                "${combo_interval_ms}" >&2
            exit 2
        fi
        combo_pattern="${combo_pattern^^}"
        trace_action "command=sequence pattern=${combo_pattern} interval_ms=${combo_interval_ms} foreground=host ack=native_selector capture=none"
        xdotool windowactivate --sync "${host_window}"
        run_operator "${host_prefix}" sequence \
            "${combo_pattern}" "${combo_interval_ms}"
        ;;
    --host-combo-capture)
        if [[ ! "${duration_ms}" =~ ^[0-9]+$ ]] ||
           (( duration_ms < 100 || duration_ms > 750 )); then
            printf 'Combo interval must be 100..750 ms: %s\n' "${duration_ms}" >&2
            exit 2
        fi
        mkdir -p -- "${stage_root}"
        stamp="$(date +%Y%m%d-%H%M%S)"
        trace_action "button=1 clicks=3 interval_ms=${duration_ms} capture=${stamp}"
        for step in 1 2 3; do
            run_operator "${host_prefix}" weak
            sleep "$(awk -v value="${duration_ms}" 'BEGIN { printf "%.3f", value / 2000.0 }')"
            capture_window "${client_window}" \
                "${stage_root}/${stamp}-tal-combo-${step}.png"
            sleep "$(awk -v value="${duration_ms}" 'BEGIN { printf "%.3f", value / 2000.0 }')"
        done
        printf '%s\n' "${stage_root}/${stamp}"-tal-combo-{1,2,3}.png
        ;;
    --client-forward)
        validate_duration
        trace_action "key=W duration_ms=${duration_ms}"
        activate_and_hold_key "${client_window}" w "${duration_ms}"
        ;;
    --client-turn-left|--client-turn-right)
        camera_command=camera-right
        [[ "${action}" == '--client-turn-left' ]] && \
            camera_command=camera-left
        trace_action "command=${camera_command} source=local_operator_api"
        run_operator "${client_prefix}" "${camera_command}"
        ;;
    --capture)
        mkdir -p -- "${stage_root}"
        stamp="$(date +%Y%m%d-%H%M%S)"
        capture_window "${host_window}" "${stage_root}/${stamp}-host.png"
        capture_window "${client_window}" "${stage_root}/${stamp}-client.png"
        trace_action "capture=${stamp}"
        printf '%s\n' \
            "${stage_root}/${stamp}-host.png" \
            "${stage_root}/${stamp}-client.png"
        ;;
esac
