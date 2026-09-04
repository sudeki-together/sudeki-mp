#include "network/lan_arena_operator.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static BOOL parse_duration(
    const wchar_t *text,
    DWORD minimum,
    DWORD maximum,
    DWORD *duration
) {
    wchar_t *end = NULL;
    unsigned long value;
    if (text == NULL || duration == NULL || *text == L'\0') return FALSE;
    value = wcstoul(text, &end, 10);
    if (end == text || *end != L'\0' || value < minimum || value > maximum) {
        return FALSE;
    }
    *duration = (DWORD)value;
    return TRUE;
}

static BOOL valid_melee_sequence(const wchar_t *pattern) {
    size_t index;
    size_t length;
    if (pattern == NULL) return FALSE;
    length = wcslen(pattern);
    if (length < 2u || length > 6u) return FALSE;
    for (index = 0u; index < length; ++index) {
        if (pattern[index] != L'W' && pattern[index] != L'S' &&
            pattern[index] != L'w' && pattern[index] != L's') return FALSE;
    }
    return TRUE;
}

static const wchar_t *skill_event_name(unsigned int slot) {
    static const wchar_t *const names[6] = {
        SUDEKIMP_LAN_ARENA_SKILL_ZERO_EVENT,
        SUDEKIMP_LAN_ARENA_SKILL_ONE_EVENT,
        SUDEKIMP_LAN_ARENA_SKILL_TWO_EVENT,
        SUDEKIMP_LAN_ARENA_SKILL_THREE_EVENT,
        SUDEKIMP_LAN_ARENA_SKILL_FOUR_EVENT,
        SUDEKIMP_LAN_ARENA_SKILL_FIVE_EVENT
    };
    return slot < 6u ? names[slot] : NULL;
}

static const wchar_t *spirit_event_name(unsigned int variant) {
    static const wchar_t *const names[2] = {
        SUDEKIMP_LAN_ARENA_HOST_SPIRIT_VARIANT_ONE_EVENT,
        SUDEKIMP_LAN_ARENA_HOST_SPIRIT_VARIANT_TWO_EVENT
    };
    return variant >= 1u && variant <= 2u ? names[variant - 1u] : NULL;
}

int wmain(int argc, wchar_t **argv) {
    HANDLE event;
    DWORD error;
    const wchar_t *event_name = NULL;
    const wchar_t *command = NULL;
    BOOL reset_event = FALSE;
    BOOL timed_hold = FALSE;
    const wchar_t *sequence_pattern = NULL;
    unsigned int pulse_count = 1u;
    DWORD duration_ms = 0u;
    DWORD skill_slot = 0u;
    DWORD spirit_variant = 0u;
    if (argc == 2 && wcscmp(argv[1], L"combat-toggle") == 0) {
        command = L"combat-toggle";
        event_name = SUDEKIMP_LAN_ARENA_HOST_COMBAT_TOGGLE_EVENT;
    } else if (argc == 2 && wcscmp(argv[1], L"combat-on") == 0) {
        command = L"combat-on";
        event_name = SUDEKIMP_LAN_ARENA_HOST_COMBAT_ON_EVENT;
    } else if (argc == 2 && wcscmp(argv[1], L"combat-off") == 0) {
        command = L"combat-off";
        event_name = SUDEKIMP_LAN_ARENA_HOST_COMBAT_OFF_EVENT;
    } else if (argc == 2 && wcscmp(argv[1], L"weak") == 0) {
        command = L"weak";
        event_name = SUDEKIMP_LAN_ARENA_WEAK_ATTACK_EVENT;
    } else if (argc == 2 && wcscmp(argv[1], L"weak-down") == 0) {
        command = L"weak-down";
        event_name = SUDEKIMP_LAN_ARENA_CLIENT_WEAK_HOLD_EVENT;
    } else if (argc == 2 && wcscmp(argv[1], L"weak-up") == 0) {
        command = L"weak-up";
        event_name = SUDEKIMP_LAN_ARENA_CLIENT_WEAK_HOLD_EVENT;
        reset_event = TRUE;
    } else if (argc == 3 && wcscmp(argv[1], L"weak-hold") == 0 &&
               parse_duration(argv[2], 50u, 30000u, &duration_ms)) {
        command = L"weak-hold";
        event_name = SUDEKIMP_LAN_ARENA_CLIENT_WEAK_HOLD_EVENT;
        timed_hold = TRUE;
    } else if (argc == 2 && wcscmp(argv[1], L"host-forward-down") == 0) {
        command = L"host-forward-down";
        event_name = SUDEKIMP_LAN_ARENA_HOST_FORWARD_HOLD_EVENT;
    } else if (argc == 2 && wcscmp(argv[1], L"host-forward-up") == 0) {
        command = L"host-forward-up";
        event_name = SUDEKIMP_LAN_ARENA_HOST_FORWARD_HOLD_EVENT;
        reset_event = TRUE;
    } else if (argc == 3 &&
               wcscmp(argv[1], L"host-forward-hold") == 0 &&
               parse_duration(argv[2], 50u, 30000u, &duration_ms)) {
        command = L"host-forward-hold";
        event_name = SUDEKIMP_LAN_ARENA_HOST_FORWARD_HOLD_EVENT;
        timed_hold = TRUE;
    } else if (argc == 2 && wcscmp(argv[1], L"client-forward-down") == 0) {
        command = L"client-forward-down";
        event_name = SUDEKIMP_LAN_ARENA_CLIENT_FORWARD_HOLD_EVENT;
    } else if (argc == 2 && wcscmp(argv[1], L"client-forward-up") == 0) {
        command = L"client-forward-up";
        event_name = SUDEKIMP_LAN_ARENA_CLIENT_FORWARD_HOLD_EVENT;
        reset_event = TRUE;
    } else if (argc == 3 &&
               wcscmp(argv[1], L"client-forward-hold") == 0 &&
               parse_duration(argv[2], 50u, 30000u, &duration_ms)) {
        command = L"client-forward-hold";
        event_name = SUDEKIMP_LAN_ARENA_CLIENT_FORWARD_HOLD_EVENT;
        timed_hold = TRUE;
    } else if (argc == 3 && wcscmp(argv[1], L"combo") == 0 &&
               parse_duration(argv[2], 80u, 750u, &duration_ms)) {
        command = L"combo";
        event_name = SUDEKIMP_LAN_ARENA_WEAK_ATTACK_EVENT;
        pulse_count = 3u;
    } else if (argc == 4 && wcscmp(argv[1], L"sequence") == 0 &&
               valid_melee_sequence(argv[2]) &&
               parse_duration(argv[3], 80u, 750u, &duration_ms)) {
        command = L"sequence";
        sequence_pattern = argv[2];
    } else if (argc == 2 && wcscmp(argv[1], L"strong") == 0) {
        command = L"strong";
        event_name = SUDEKIMP_LAN_ARENA_HOST_STRONG_ATTACK_EVENT;
    } else if (argc == 2 && wcscmp(argv[1], L"sweep") == 0) {
        command = L"sweep";
        event_name = SUDEKIMP_LAN_ARENA_HOST_SWEEP_ATTACK_EVENT;
    } else if (argc == 2 && wcscmp(argv[1], L"block") == 0) {
        command = L"block";
        event_name = SUDEKIMP_LAN_ARENA_HOST_BLOCK_EVENT;
    } else if (argc == 2 && wcscmp(argv[1], L"camera-left") == 0) {
        command = L"camera-left";
        event_name = SUDEKIMP_LAN_ARENA_CLIENT_CAMERA_LEFT_EVENT;
    } else if (argc == 2 && wcscmp(argv[1], L"camera-right") == 0) {
        command = L"camera-right";
        event_name = SUDEKIMP_LAN_ARENA_CLIENT_CAMERA_RIGHT_EVENT;
    } else if (argc == 3 && wcscmp(argv[1], L"skill") == 0 &&
               parse_duration(argv[2], 0u, 5u, &skill_slot)) {
        command = L"skill";
        event_name = skill_event_name((unsigned int)skill_slot);
    } else if (argc == 3 && wcscmp(argv[1], L"spirit") == 0 &&
               parse_duration(argv[2], 1u, 2u, &spirit_variant)) {
        command = L"spirit";
        event_name = spirit_event_name((unsigned int)spirit_variant);
    } else {
        fwprintf(stderr,
            L"usage: SudekiMP.LanArenaOperator.exe "
            L"combat-toggle|combat-on|combat-off|"
            L"weak|weak-down|weak-up|weak-hold MS|combo MS|"
            L"host-forward-down|host-forward-up|host-forward-hold MS|"
            L"client-forward-down|client-forward-up|client-forward-hold MS|"
            L"sequence WWS MS|"
            L"strong|sweep|block|"
            L"camera-left|camera-right|skill SLOT(0-5)|"
            L"spirit VARIANT(1-2)\n");
        return 2;
    }
    if (sequence_pattern != NULL) {
        HANDLE weak_event = OpenEventW(
            EVENT_MODIFY_STATE, FALSE,
            SUDEKIMP_LAN_ARENA_WEAK_ATTACK_EVENT);
        HANDLE strong_event = OpenEventW(
            EVENT_MODIFY_STATE, FALSE,
            SUDEKIMP_LAN_ARENA_HOST_STRONG_ATTACK_EVENT);
        HANDLE action_ack_event = OpenEventW(
            SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE,
            SUDEKIMP_LAN_ARENA_HOST_ACTION_ACK_EVENT);
        size_t index;
        size_t length = wcslen(sequence_pattern);
        DWORD wait_result;
        if (weak_event == NULL || strong_event == NULL ||
            action_ack_event == NULL) {
            error = GetLastError();
            if (action_ack_event != NULL) CloseHandle(action_ack_event);
            if (strong_event != NULL) CloseHandle(strong_event);
            if (weak_event != NULL) CloseHandle(weak_event);
            fwprintf(stderr,
                L"LAN operator endpoints for %ls are unavailable (error %lu).\n",
                command, (unsigned long)error);
            return 1;
        }
        for (index = 0u; index < length; ++index) {
            HANDLE selected = sequence_pattern[index] == L'W' ||
                    sequence_pattern[index] == L'w' ?
                weak_event : strong_event;
            if (!ResetEvent(action_ack_event) || !SetEvent(selected)) {
                error = GetLastError();
                CloseHandle(action_ack_event);
                CloseHandle(strong_event);
                CloseHandle(weak_event);
                fwprintf(stderr,
                    L"LAN operator command %ls step %u failed (error %lu).\n",
                    command, (unsigned int)index + 1u, (unsigned long)error);
                return 1;
            }
            /* The acknowledgement is the observed native selector edge, not
             * merely the controller submission.  Background Wine frames can
             * legitimately take longer than an input service pass. */
            wait_result = WaitForSingleObject(action_ack_event, 5000u);
            if (wait_result != WAIT_OBJECT_0) {
                error = wait_result == WAIT_FAILED ? GetLastError() :
                    ERROR_TIMEOUT;
                CloseHandle(action_ack_event);
                CloseHandle(strong_event);
                CloseHandle(weak_event);
                fwprintf(stderr,
                    L"LAN operator command %ls step %u was not acknowledged "
                    L"(error %lu).\n",
                    command, (unsigned int)index + 1u,
                    (unsigned long)error);
                return 1;
            }
            if (index + 1u < length) Sleep(duration_ms);
        }
        CloseHandle(action_ack_event);
        CloseHandle(strong_event);
        CloseHandle(weak_event);
        wprintf(L"LAN operator command %ls completed "
            L"(pattern %ls, %lu ms).\n",
            command, sequence_pattern, (unsigned long)duration_ms);
        return 0;
    }
    event = OpenEventW(
        EVENT_MODIFY_STATE, FALSE, event_name);
    if (event == NULL) {
        error = GetLastError();
        fwprintf(stderr,
            L"LAN operator endpoint for %ls is unavailable (error %lu).\n",
            command, (unsigned long)error);
        return 1;
    }
    if (timed_hold) {
        if (!SetEvent(event)) {
            error = GetLastError();
            CloseHandle(event);
            fwprintf(stderr,
                L"LAN operator command %ls failed (error %lu).\n",
                command, (unsigned long)error);
            return 1;
        }
        Sleep(duration_ms);
        if (!ResetEvent(event)) {
            error = GetLastError();
            CloseHandle(event);
            fwprintf(stderr,
                L"LAN operator command %ls release failed (error %lu).\n",
                command, (unsigned long)error);
            return 1;
        }
    } else if (pulse_count > 1u) {
        unsigned int pulse;
        for (pulse = 0u; pulse < pulse_count; ++pulse) {
            if (!SetEvent(event)) {
                error = GetLastError();
                CloseHandle(event);
                fwprintf(stderr,
                    L"LAN operator command %ls pulse %u failed (error %lu).\n",
                    command, pulse + 1u, (unsigned long)error);
                return 1;
            }
            if (pulse + 1u < pulse_count) Sleep(duration_ms);
        }
    } else if (!(reset_event ? ResetEvent(event) : SetEvent(event))) {
        error = GetLastError();
        CloseHandle(event);
        fwprintf(stderr,
            L"LAN operator command %ls failed (error %lu).\n",
            command, (unsigned long)error);
        return 1;
    }
    CloseHandle(event);
    if (timed_hold) {
        wprintf(L"LAN operator command %ls completed (%lu ms).\n",
            command, (unsigned long)duration_ms);
    } else if (pulse_count > 1u) {
        wprintf(L"LAN operator command %ls completed (%u pulses, %lu ms).\n",
            command, pulse_count, (unsigned long)duration_ms);
    } else {
        if (wcscmp(command, L"skill") == 0) {
            wprintf(L"LAN operator command %ls queued (slot %lu).\n",
                command, (unsigned long)skill_slot);
        } else if (wcscmp(command, L"spirit") == 0) {
            wprintf(L"LAN operator command %ls queued (variant %lu).\n",
                command, (unsigned long)spirit_variant);
        } else {
            wprintf(L"LAN operator command %ls queued.\n", command);
        }
    }
    return 0;
}
