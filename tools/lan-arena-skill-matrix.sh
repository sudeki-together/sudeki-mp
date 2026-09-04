#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
stage_root="${project_dir}/build/mingw32/lan-loopback"
host_log="${stage_root}/host-runtime.log"
client_log="${stage_root}/client-runtime.log"
host_prefix="${SUDEKIMP_LAN_HOST_WINEPREFIX:-${HOME}/Games/sudeki-lan-host-win32-prefix}"
client_prefix="${SUDEKIMP_LAN_CLIENT_WINEPREFIX:-${HOME}/Games/sudeki-lan-client-win32-prefix}"
operator_exe="${project_dir}/build/mingw32/bin/SudekiMP.LanArenaOperator.exe"
timeout_ms="${SUDEKIMP_LAN_SKILL_TIMEOUT_MS:-30000}"
hard_timeout_ms="${SUDEKIMP_LAN_SKILL_HARD_TIMEOUT_MS:-300000}"
poll_ms="${SUDEKIMP_LAN_SKILL_POLL_MS:-100}"
operator_timeout_seconds="${SUDEKIMP_LAN_SKILL_OPERATOR_TIMEOUT_SECONDS:-10}"
dry_run=false
declare -a transactions=()

usage() {
    printf '%s\n' \
        'usage: tools/lan-arena-skill-matrix.sh [OPTIONS]' \
        '' \
        'Run an explicitly selected, serialized LAN skill-presentation matrix.' \
        'This process-test tool uses only the in-process LAN operator events;' \
        'it never focuses a window or synthesizes keyboard/mouse input.' \
        '' \
        'Options:' \
        '  --tal SLOTS          Append Tal slots (0..5 comma list, or all)' \
        '  --ailish SLOTS       Append Ailish slots (0..5 comma list, or all)' \
        '  --timeout-ms MS      Per-stage/progress-stall timeout (default 30000)' \
        '  --hard-timeout-ms MS Completion hard ceiling after client start (default 300000)' \
        '  --poll-ms MS         Log/process polling interval (default 100)' \
        '  --dry-run            Print the ordered matrix and exact log contract' \
        '  --help               Show this help' \
        '' \
        'Examples:' \
        '  tools/lan-arena-skill-matrix.sh --tal 0 --ailish 5' \
        '  tools/lan-arena-skill-matrix.sh --tal all --ailish 0,2,5'
}

die() {
    printf 'LAN skill matrix: %s\n' "$*" >&2
    exit 1
}

append_slots() {
    local actor="$1"
    local specification="$2"
    local slot
    local -a slots=()

    if [[ "${specification}" == all ]]; then
        slots=(0 1 2 3 4 5)
    else
        if [[ ! "${specification}" =~ ^[0-5](,[0-5])*$ ]]; then
            printf 'Invalid %s slot list (expected comma-separated 0..5 or all): %s\n' \
                "${actor}" "${specification}" >&2
            exit 2
        fi
        IFS=',' read -r -a slots <<<"${specification}"
    fi
    if (( ${#slots[@]} == 0 )); then
        printf 'Slot list for %s must not be empty.\n' "${actor}" >&2
        exit 2
    fi
    for slot in "${slots[@]}"; do
        if [[ ! "${slot}" =~ ^[0-5]$ ]]; then
            printf 'Invalid %s skill slot (expected 0..5): %s\n' \
                "${actor}" "${slot}" >&2
            exit 2
        fi
        transactions+=("${actor}:${slot}")
    done
}

while (( $# > 0 )); do
    case "$1" in
        --tal|--ailish)
            if (( $# < 2 )); then
                printf 'Missing slot list after %s.\n' "$1" >&2
                exit 2
            fi
            actor=Tal
            [[ "$1" == --ailish ]] && actor=Ailish
            append_slots "${actor}" "$2"
            shift 2
            ;;
        --timeout-ms)
            if (( $# < 2 )); then
                printf '%s\n' 'Missing value after --timeout-ms.' >&2
                exit 2
            fi
            timeout_ms="$2"
            shift 2
            ;;
        --hard-timeout-ms)
            if (( $# < 2 )); then
                printf '%s\n' 'Missing value after --hard-timeout-ms.' >&2
                exit 2
            fi
            hard_timeout_ms="$2"
            shift 2
            ;;
        --poll-ms)
            if (( $# < 2 )); then
                printf '%s\n' 'Missing value after --poll-ms.' >&2
                exit 2
            fi
            poll_ms="$2"
            shift 2
            ;;
        --dry-run)
            dry_run=true
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown option: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if (( ${#transactions[@]} == 0 )); then
    printf '%s\n' 'At least one --tal or --ailish slot list is required.' >&2
    usage >&2
    exit 2
fi
if [[ ! "${timeout_ms}" =~ ^[0-9]+$ ]] ||
   (( timeout_ms < 1000 || timeout_ms > 120000 )); then
    printf 'Stage/progress timeout must be 1000..120000 ms: %s\n' \
        "${timeout_ms}" >&2
    exit 2
fi
if [[ ! "${hard_timeout_ms}" =~ ^[0-9]+$ ]] ||
   (( hard_timeout_ms < 1000 || hard_timeout_ms > 600000 )); then
    printf 'Completion hard timeout must be 1000..600000 ms: %s\n' \
        "${hard_timeout_ms}" >&2
    exit 2
fi
if (( hard_timeout_ms < timeout_ms )); then
    printf 'Completion hard timeout must be at least the stage/progress timeout (%s < %s).\n' \
        "${hard_timeout_ms}" "${timeout_ms}" >&2
    exit 2
fi
if [[ ! "${poll_ms}" =~ ^[0-9]+$ ]] ||
   (( poll_ms < 25 || poll_ms > 1000 )); then
    printf 'Poll interval must be 25..1000 ms: %s\n' "${poll_ms}" >&2
    exit 2
fi
if [[ ! "${operator_timeout_seconds}" =~ ^[0-9]+$ ]] ||
   (( operator_timeout_seconds < 1 || operator_timeout_seconds > 60 )); then
    printf 'Operator timeout must be 1..60 seconds: %s\n' \
        "${operator_timeout_seconds}" >&2
    exit 2
fi

if [[ "${dry_run}" == true ]]; then
    printf 'LAN skill matrix dry run: transactions=%u timeout_ms=%s hard_timeout_ms=%s poll_ms=%s\n' \
        "${#transactions[@]}" "${timeout_ms}" "${hard_timeout_ms}" "${poll_ms}"
    for index in "${!transactions[@]}"; do
        IFS=: read -r actor slot <<<"${transactions[index]}"
        printf '  %u. actor=%s slot=%s operator_prefix=%s\n' \
            "$((index + 1))" "${actor}" "${slot}" \
            "$([[ "${actor}" == Tal ]] && printf host || printf client)"
        printf '%s\n' \
            '     await host_skill/started -> client_skill/started ->' \
            '     client_skill/completed (matching-presentation progress watchdog) ->' \
            '     client_skill_handoff/retired; every stage receives a fresh deadline'
    done
    exit 0
fi

for command in awk flock pgrep rg tail timeout tr wc wine; do
    command -v "${command}" >/dev/null 2>&1 ||
        die "required command is missing: ${command}"
done
[[ -f "${operator_exe}" ]] || die "operator executable is missing: ${operator_exe}"
[[ -f "${host_log}" ]] || die "host runtime log is missing: ${host_log}"
[[ -f "${client_log}" ]] || die "client runtime log is missing: ${client_log}"

processes_for_prefix() {
    local expected_prefix="$1"
    local candidate
    for candidate in $(pgrep -x SUDEKI.exe 2>/dev/null || true); do
        if tr '\0' '\n' <"/proc/${candidate}/environ" 2>/dev/null |
           awk -v expected="WINEPREFIX=${expected_prefix}" '$0 == expected { found=1 } END { exit !found }'; then
            printf '%s\n' "${candidate}"
        fi
    done
}

mapfile -t host_pids < <(processes_for_prefix "${host_prefix}")
mapfile -t client_pids < <(processes_for_prefix "${client_prefix}")
(( ${#host_pids[@]} == 1 )) ||
    die "expected exactly one host SUDEKI.exe for ${host_prefix}; found ${#host_pids[@]}"
(( ${#client_pids[@]} == 1 )) ||
    die "expected exactly one client SUDEKI.exe for ${client_prefix}; found ${#client_pids[@]}"
host_pid="${host_pids[0]}"
client_pid="${client_pids[0]}"

latest_line_number() {
    local log="$1"
    local needle="$2"
    awk -v needle="${needle}" 'index($0, needle) { latest=NR } END { print latest + 0 }' \
        "${log}"
}

latest_connection_token() {
    local log="$1"
    local event="$2"
    awk -v event="event=${event} token=" '
        index($0, event) {
            for (field=1; field<=NF; ++field) {
                if ($field ~ /^token=0x[0-9a-fA-F]+$/) {
                    token=substr($field, 7)
                }
            }
        }
        END { print token }
    ' "${log}"
}

host_connected_line="$(latest_line_number "${host_log}" 'event=host_connected ')"
client_connected_line="$(latest_line_number "${client_log}" 'event=client_connected ')"
(( host_connected_line > 0 )) || die 'host has no authenticated connection record'
(( client_connected_line > 0 )) || die 'client has no authenticated connection record'
host_token="$(latest_connection_token "${host_log}" host_connected)"
client_token="$(latest_connection_token "${client_log}" client_connected)"
[[ -n "${host_token}" && "${host_token}" == "${client_token}" ]] ||
    die "host/client session tokens do not match (${host_token:-missing}/${client_token:-missing})"

host_stream_line="$(latest_line_number "${host_log}" 'event=host_snapshot_stream phase=active ')"
client_stream_line="$(latest_line_number "${client_log}" 'event=client_snapshot_replica phase=active ')"
(( host_stream_line > host_connected_line )) ||
    die 'host snapshot stream has not become active for the current session'
(( client_stream_line > client_connected_line )) ||
    die 'client snapshot replica has not become active for the current session'

latest_host_failure_line="$(latest_line_number "${host_log}" 'policy=fail_closed_no_reconnect')"
latest_client_failure_line="$(latest_line_number "${client_log}" 'policy=fail_closed_no_reconnect')"
latest_client_discard_line="$(latest_line_number "${client_log}" 'event=client_snapshot_replica phase=discarded reason=transport_authority_inactive ')"
(( latest_host_failure_line < host_connected_line )) ||
    die 'host transport failed after its latest authenticated connection'
(( latest_client_failure_line < client_connected_line )) ||
    die 'client transport failed after its latest authenticated connection'
(( latest_client_discard_line < client_connected_line )) ||
    die 'client discarded replica state after its latest authenticated connection'

run_host_offset="$(wc -c <"${host_log}")"
run_client_offset="$(wc -c <"${client_log}")"
poll_seconds="$(awk -v value="${poll_ms}" 'BEGIN { printf "%.3f", value / 1000.0 }')"

new_log_text() {
    local log="$1"
    local offset="$2"
    local current_size
    current_size="$(wc -c <"${log}")"
    if (( current_size < offset )); then
        return 2
    fi
    tail -c "+$((offset + 1))" "${log}" | tr -d '\r'
}

print_failure_context() {
    printf '%s\n' '--- new host lifecycle log ---' >&2
    new_log_text "${host_log}" "${run_host_offset}" 2>/dev/null |
        rg 'lan_arena_(runtime|session).*host_|fail_closed_no_reconnect|network_pump' |
        tail -n 40 >&2 || true
    printf '%s\n' '--- new client lifecycle log ---' >&2
    new_log_text "${client_log}" "${run_client_offset}" 2>/dev/null |
        rg 'client_skill|client_snapshot_replica|fail_closed_no_reconnect|network_pump' |
        tail -n 60 >&2 || true
}

fail_with_context() {
    print_failure_context
    die "$*"
}

check_health() {
    local host_size
    local client_size
    local failures

    kill -0 "${host_pid}" 2>/dev/null || fail_with_context 'host SUDEKI.exe exited'
    kill -0 "${client_pid}" 2>/dev/null || fail_with_context 'client SUDEKI.exe exited'
    host_size="$(wc -c <"${host_log}")"
    client_size="$(wc -c <"${client_log}")"
    (( host_size >= run_host_offset )) || fail_with_context 'host runtime log was truncated or replaced'
    (( client_size >= run_client_offset )) || fail_with_context 'client runtime log was truncated or replaced'

    failures="$({
        new_log_text "${host_log}" "${run_host_offset}"
        new_log_text "${client_log}" "${run_client_offset}"
    } | rg -m 1 \
        'reason=activation_retry_exhausted|reason=native_retirement_timeout|policy=fail_closed_no_reconnect|event=network_pump phase=stopped|event=client_snapshot_replica phase=discarded reason=transport_authority_inactive|event=host_auto_rehost state=(hosting|rejected)' || true)"
    [[ -z "${failures}" ]] ||
        fail_with_context "fatal runtime lifecycle record: ${failures}"
}

find_host_start_sequence() {
    local offset="$1"
    local actor="$2"
    local slot="$3"
    local line
    local pattern="event=host_skill phase=started actor=${actor} sequence=([0-9]+) slot=${slot} "

    while IFS= read -r line; do
        if [[ "${line}" =~ ${pattern} ]]; then
            printf '%s\n' "${BASH_REMATCH[1]}"
            return 0
        fi
    done < <(new_log_text "${host_log}" "${offset}" 2>/dev/null || true)
    return 1
}

client_lifecycle_present() {
    local offset="$1"
    local event="$2"
    local state_key="$3"
    local state="$4"
    local actor="$5"
    local sequence="$6"
    local slot="$7"
    local needle="event=${event} ${state_key}=${state} actor=${actor} sequence=${sequence} slot=${slot} "

    new_log_text "${client_log}" "${offset}" 2>/dev/null |
        rg -F -- "${needle}" >/dev/null
}

latest_client_presentation_progress() {
    local offset="$1"
    local actor="$2"
    local sequence="$3"
    local slot="$4"
    local started_needle="event=client_skill phase=started actor=${actor} sequence=${sequence} slot=${slot} "
    local completed_needle="event=client_skill phase=completed actor=${actor} sequence=${sequence} slot=${slot} "
    local presentation_needle="event=client_native_presentation actor=${actor} "

    # The matrix admits only one transaction at a time. Anchor presentation
    # changes after this exact actor/sequence/slot start so another actor's
    # animation, or stale lines before admission, cannot refresh the watchdog.
    new_log_text "${client_log}" "${offset}" 2>/dev/null |
        awk -v started="${started_needle}" \
            -v completed="${completed_needle}" \
            -v presentation="${presentation_needle}" '
            index($0, started) {
                transaction_started=1
                next
            }
            transaction_started && index($0, completed) {
                exit
            }
            transaction_started && index($0, presentation) {
                selectors=""
                states=""
                times=""
                for (field=1; field<=NF; ++field) {
                    if ($field ~ /^selectors=/) selectors=$field
                    else if ($field ~ /^states=/) states=$field
                    else if ($field ~ /^times=/) times=$field
                }
                if (selectors != "" && states != "" && times != "") {
                    latest=selectors " " states " " times
                }
            }
            END {
                if (latest != "") print latest
            }
        '
}

milliseconds_now() {
    date +%s%3N
}

wait_for_host_start() {
    local offset="$1"
    local actor="$2"
    local slot="$3"
    local deadline="$4"
    local sequence

    while (( $(milliseconds_now) < deadline )); do
        check_health
        sequence="$(find_host_start_sequence "${offset}" "${actor}" "${slot}" || true)"
        if [[ -n "${sequence}" ]]; then
            printf '%s\n' "${sequence}"
            return 0
        fi
        sleep "${poll_seconds}"
    done
    return 1
}

wait_for_client_lifecycle() {
    local offset="$1"
    local event="$2"
    local state_key="$3"
    local state="$4"
    local actor="$5"
    local sequence="$6"
    local slot="$7"
    local deadline="$8"

    while (( $(milliseconds_now) < deadline )); do
        check_health
        if client_lifecycle_present \
                "${offset}" "${event}" "${state_key}" "${state}" \
                "${actor}" "${sequence}" "${slot}"; then
            return 0
        fi
        sleep "${poll_seconds}"
    done
    return 1
}

client_completion_wait_failure=""
wait_for_client_completion_with_progress() {
    local offset="$1"
    local actor="$2"
    local sequence="$3"
    local slot="$4"
    local stall_timeout="$5"
    local hard_timeout="$6"
    local now
    local stall_deadline
    local hard_deadline
    local progress=""
    local last_progress=""

    now="$(milliseconds_now)"
    stall_deadline="$((now + stall_timeout))"
    hard_deadline="$((now + hard_timeout))"
    client_completion_wait_failure=""

    while true; do
        check_health
        if client_lifecycle_present \
                "${offset}" client_skill phase completed \
                "${actor}" "${sequence}" "${slot}"; then
            return 0
        fi

        progress="$(latest_client_presentation_progress \
            "${offset}" "${actor}" "${sequence}" "${slot}" || true)"
        now="$(milliseconds_now)"
        if [[ -n "${progress}" && "${progress}" != "${last_progress}" ]]; then
            last_progress="${progress}"
            stall_deadline="$((now + stall_timeout))"
        fi
        if (( now >= hard_deadline )); then
            client_completion_wait_failure="hard ceiling ${hard_timeout} ms elapsed"
            return 1
        fi
        if (( now >= stall_deadline )); then
            if [[ -n "${last_progress}" ]]; then
                client_completion_wait_failure="no matching selector/state/time progress for ${stall_timeout} ms (last: ${last_progress})"
            else
                client_completion_wait_failure="no matching presentation progress for ${stall_timeout} ms"
            fi
            return 1
        fi
        sleep "${poll_seconds}"
    done
}

mkdir -p -- "${stage_root}"
exec 9>"${stage_root}/skill-matrix.lock"
flock -n 9 || die 'another LAN skill matrix is already active'

printf 'LAN skill matrix: session=%s host_pid=%s client_pid=%s transactions=%u\n' \
    "${host_token}" "${host_pid}" "${client_pid}" "${#transactions[@]}"

for index in "${!transactions[@]}"; do
    IFS=: read -r actor slot <<<"${transactions[index]}"
    host_offset="$(wc -c <"${host_log}")"
    client_offset="$(wc -c <"${client_log}")"
    prefix="${host_prefix}"
    [[ "${actor}" == Ailish ]] && prefix="${client_prefix}"

    printf '[%u/%u] actor=%s slot=%s queueing operator event\n' \
        "$((index + 1))" "${#transactions[@]}" "${actor}" "${slot}"
    if ! timeout --signal=TERM "${operator_timeout_seconds}s" \
            env WINEPREFIX="${prefix}" wine "${operator_exe}" skill "${slot}"; then
        fail_with_context "operator failed for ${actor} slot ${slot}"
    fi

    deadline="$(( $(milliseconds_now) + timeout_ms ))"
    sequence="$(wait_for_host_start \
        "${host_offset}" "${actor}" "${slot}" "${deadline}" || true)"
    [[ -n "${sequence}" ]] ||
        fail_with_context "timed out waiting for host start: ${actor} slot ${slot}"
    printf '[%u/%u] actor=%s slot=%s sequence=%s host=started\n' \
        "$((index + 1))" "${#transactions[@]}" "${actor}" "${slot}" "${sequence}"

    # Character skills have one canonical host admission record. The exact
    # client completion record is emitted only after the matching host
    # snapshot reports inactive; handoff/retired then proves the local native
    # presentation lease was positively released. Keep all three replica
    # witnesses on the host-allocated sequence before admitting another case.
    deadline="$(( $(milliseconds_now) + timeout_ms ))"
    wait_for_client_lifecycle \
        "${client_offset}" client_skill phase started \
        "${actor}" "${sequence}" "${slot}" "${deadline}" ||
        fail_with_context "timed out waiting for client start: ${actor} sequence ${sequence} slot ${slot}"

    wait_for_client_completion_with_progress \
        "${client_offset}" "${actor}" "${sequence}" "${slot}" \
        "${timeout_ms}" "${hard_timeout_ms}" ||
        fail_with_context "timed out waiting for client completion: ${actor} sequence ${sequence} slot ${slot}: ${client_completion_wait_failure}"

    deadline="$(( $(milliseconds_now) + timeout_ms ))"
    wait_for_client_lifecycle \
        "${client_offset}" client_skill_handoff state retired \
        "${actor}" "${sequence}" "${slot}" "${deadline}" ||
        fail_with_context "timed out waiting for client retirement: ${actor} sequence ${sequence} slot ${slot}"

    printf '[%u/%u] actor=%s slot=%s sequence=%s client=started,completed,retired PASS\n' \
        "$((index + 1))" "${#transactions[@]}" "${actor}" "${slot}" "${sequence}"
done

check_health
printf 'LAN skill matrix passed: transactions=%u session=%s\n' \
    "${#transactions[@]}" "${host_token}"
