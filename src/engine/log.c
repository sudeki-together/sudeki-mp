#include "engine/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static HANDLE log_file = INVALID_HANDLE_VALUE;

BOOL SudekiMpLogOpenBesideGame(const wchar_t *game_path) {
    wchar_t path[MAX_PATH];
    wchar_t *slash;
    static const wchar_t filename[] = L"SudekiMP.log";
    size_t prefix_length;

    DWORD override_length = GetEnvironmentVariableW(
        L"SUDEKIMP_LOG_PATH", path, MAX_PATH);

    if (override_length > 0u) {
        if (override_length >= MAX_PATH) return FALSE;
    } else if (game_path == NULL || lstrlenW(game_path) >= MAX_PATH) {
        return FALSE;
    } else {
        lstrcpyW(path, game_path);
        slash = wcsrchr(path, L'\\');
        if (slash == NULL) {
            slash = wcsrchr(path, L'/');
        }
        if (slash != NULL) {
            slash[1] = L'\0';
        } else {
            path[0] = L'\0';
        }

        prefix_length = (size_t)lstrlenW(path);
        if (prefix_length + (sizeof(filename) / sizeof(filename[0])) > MAX_PATH) {
            return FALSE;
        }
        lstrcatW(path, filename);
    }

    log_file = CreateFileW(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    return log_file != INVALID_HANDLE_VALUE;
}

void SudekiMpLogWrite(const char *message) {
    DWORD written;
    if (log_file == INVALID_HANDLE_VALUE || message == NULL) {
        return;
    }
    WriteFile(log_file, message, (DWORD)strlen(message), &written, NULL);
    /* WriteFile makes appended records immediately visible to live readers.
     * Do not force a physical filesystem flush for every diagnostic line:
     * LAN presentation tracing emits several records per second and Wine's
     * FlushFileBuffers maps to a synchronous fsync, which can stall the game
     * thread long enough to starve snapshot consumption.  CloseHandle still
     * closes the stream normally, while a process crash retains completed
     * writes in the host kernel cache. */
}

void SudekiMpLogFormat(const char *format, ...) {
    char buffer[1024];
    va_list arguments;
    int length;

    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);

    if (length < 0) {
        return;
    }
    buffer[sizeof(buffer) - 1] = '\0';
    SudekiMpLogWrite(buffer);
}

void SudekiMpLogClose(void) {
    if (log_file != INVALID_HANDLE_VALUE) {
        CloseHandle(log_file);
        log_file = INVALID_HANDLE_VALUE;
    }
}
