#include "hooks/lan_arena_pause_panel.h"

#include "engine/log.h"
#include "hooks/call_hook.h"
#include "hooks/lan_arena_runtime.h"
#include "network/lan_arena_session.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef void (*QuitScreenRenderFunction)(void);

enum {
    RVA_PC_QUIT_SCREEN_GLOBAL = 0x00408d68u,
    RVA_PC_QUIT_SCREEN_RENDER = 0x0001d690u,
    RVA_PC_QUIT_SCREEN_RENDER_CALL = 0x0028d572u,
    RVA_UI_SCENE_GLOBAL = 0x00408d1cu,
    RVA_UI_TEXT_SUBMIT = 0x00009930u,
    PC_QUIT_SCREEN_VISIBLE_OFFSET = 0x1c2u,
    NATIVE_UI_TEXT_FONT_TITLE = 1u,
    NATIVE_UI_TEXT_ALIGNMENT_TITLE_LEFT = 1u
};

static SudekiMpRelativeCallHook quit_render_hook;
static QuitScreenRenderFunction original_quit_render;
static uint8_t *game_base;
static BOOL end_key_was_down;
static BOOL join_key_was_down;
static BOOL backspace_was_down;
static BOOL edit_key_was_down[12];
static char client_address_edit[32];
static char last_endpoint_address[16];
static unsigned int last_endpoint_port;
static DWORD last_action_error;

static const uint8_t expected_quit_render_entry[] = {
    0x57u, 0x8bu, 0xf8u, 0x80u, 0xbfu,
    0xc2u, 0x01u, 0x00u, 0x00u, 0x00u
};
static const uint8_t expected_ui_text_submit_entry[] = {
    0x56u, 0x57u, 0x8bu, 0xf1u, 0x8bu,
    0xf8u, 0xe8u, 0xd5u, 0xfeu, 0xffu, 0xffu
};

static BOOL readable_memory(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION region;
    uintptr_t start = (uintptr_t)pointer;
    uintptr_t finish = start + size;
    if (pointer == NULL || size == 0u || finish < start ||
        VirtualQuery(pointer, &region, sizeof(region)) != sizeof(region) ||
        region.State != MEM_COMMIT || (region.Protect & PAGE_GUARD) != 0u ||
        region.Protect == PAGE_NOACCESS) return FALSE;
    return finish <= (uintptr_t)region.BaseAddress + region.RegionSize;
}

static BOOL quit_screen_visible(void) {
    uint8_t *screen;
    if (game_base == NULL || !readable_memory(
            game_base + RVA_PC_QUIT_SCREEN_GLOBAL, sizeof(screen))) return FALSE;
    screen = *(uint8_t **)(game_base + RVA_PC_QUIT_SCREEN_GLOBAL);
    return readable_memory(screen, PC_QUIT_SCREEN_VISIBLE_OFFSET + 1u) &&
        screen[PC_QUIT_SCREEN_VISIBLE_OFFSET] != 0u;
}

static void native_text_set(uint8_t record[0x3c], const char *text) {
    size_t length = text == NULL ? 0u : strlen(text);
    size_t index;
    uint16_t *wide = (uint16_t *)(record + 4u);
    memset(record, 0, 0x3cu);
    if (length > 27u) length = 27u;
    *(uint32_t *)record = UINT32_C(0x80000000) | (uint32_t)length;
    for (index = 0u; index < length; ++index) wide[index] = (uint8_t)text[index];
    wide[length] = 0u;
}

__attribute__((naked, noinline, used))
static void native_text_submit_bridge(
    void *scene,
    void *text,
    unsigned int font,
    unsigned int alignment,
    unsigned int x,
    unsigned int y,
    unsigned int color,
    void *submit
) {
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl 32(%ebp)\n\t"
        "pushl 28(%ebp)\n\t"
        "pushl 24(%ebp)\n\t"
        "pushl 20(%ebp)\n\t"
        "pushl 16(%ebp)\n\t"
        "movl 12(%ebp), %eax\n\t"
        "movl 8(%ebp), %ecx\n\t"
        "call *36(%ebp)\n\t"
        "popl %ebp\n\t"
        "ret\n\t"
    );
}

static BOOL submit_text(const char *text, unsigned int y, unsigned int color) {
    uint8_t record[0x3c];
    void *scene;
    if (game_base == NULL || text == NULL || strlen(text) > 27u ||
        !readable_memory(game_base + RVA_UI_SCENE_GLOBAL, sizeof(scene))) {
        return FALSE;
    }
    scene = *(void **)(game_base + RVA_UI_SCENE_GLOBAL);
    if (scene == NULL) return FALSE;
    native_text_set(record, text);
    native_text_submit_bridge(
        scene, record, NATIVE_UI_TEXT_FONT_TITLE,
        NATIVE_UI_TEXT_ALIGNMENT_TITLE_LEFT, 196u, y, color,
        game_base + RVA_UI_TEXT_SUBMIT);
    return TRUE;
}

static const char *phase_label(SudekiMpLanArenaConnectionPhase phase) {
    switch (phase) {
        case SUDEKIMP_LAN_ARENA_CONNECTION_HOSTING: return "WAITING FOR CLIENT";
        case SUDEKIMP_LAN_ARENA_CONNECTION_JOINING: return "CONNECTING";
        case SUDEKIMP_LAN_ARENA_CONNECTION_CONNECTED: return "CONNECTED";
        case SUDEKIMP_LAN_ARENA_CONNECTION_REJECTED: return "CONNECTION REJECTED";
        case SUDEKIMP_LAN_ARENA_CONNECTION_TIMED_OUT: return "CONNECTION TIMED OUT";
        case SUDEKIMP_LAN_ARENA_CONNECTION_ENDED: return "SESSION ENDED";
        default: return "OFFLINE";
    }
}

static const char *failure_label(SudekiMpLanArenaRejectReason failure) {
    switch (failure) {
        case SUDEKIMP_LAN_ARENA_REJECT_VERSION: return "ERROR PROTOCOL VERSION";
        case SUDEKIMP_LAN_ARENA_REJECT_GAME_HASH: return "ERROR GAME BUILD HASH";
        case SUDEKIMP_LAN_ARENA_REJECT_BUILD: return "ERROR MOD BUILD";
        case SUDEKIMP_LAN_ARENA_REJECT_MAP: return "ERROR ARENA MAP";
        case SUDEKIMP_LAN_ARENA_REJECT_ROLE: return "ERROR PLAYER ROLES";
        case SUDEKIMP_LAN_ARENA_REJECT_TOKEN: return "ERROR SESSION TOKEN";
        case SUDEKIMP_LAN_ARENA_REJECT_SEQUENCE: return "ERROR PACKET SEQUENCE";
        case SUDEKIMP_LAN_ARENA_REJECT_MALFORMED: return "ERROR MALFORMED PACKET";
        case SUDEKIMP_LAN_ARENA_REJECT_BUSY: return "ERROR HOST BUSY";
        case SUDEKIMP_LAN_ARENA_REJECT_TIMEOUT: return "ERROR PEER TIMEOUT";
        case SUDEKIMP_LAN_ARENA_REJECT_AUTHORITY: return "ERROR PACKET AUTHORITY";
        default: return NULL;
    }
}

static void reset_edit_edges(void) {
    end_key_was_down = FALSE;
    join_key_was_down = FALSE;
    backspace_was_down = FALSE;
    memset(edit_key_was_down, 0, sizeof(edit_key_was_down));
}

static void poll_client_address_edit(void) {
    static const int virtual_keys[12] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        VK_OEM_PERIOD, VK_OEM_1
    };
    static const char characters[12] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', ':'
    };
    size_t length = strlen(client_address_edit);
    unsigned int index;
    BOOL down;
    for (index = 0u; index < 12u; ++index) {
        down = (GetAsyncKeyState(virtual_keys[index]) & 0x8000) != 0;
        if (down && !edit_key_was_down[index] &&
            (index != 11u ||
             (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) &&
            length + 1u < sizeof(client_address_edit)) {
            client_address_edit[length++] = characters[index];
            client_address_edit[length] = '\0';
        }
        edit_key_was_down[index] = down;
    }
    down = (GetAsyncKeyState(VK_BACK) & 0x8000) != 0;
    if (down && !backspace_was_down && length > 0u) {
        client_address_edit[length - 1u] = '\0';
    }
    backspace_was_down = down;
}

__attribute__((noinline, used))
static void render_panel(void) {
    SudekiMpLanArenaSessionStatus status;
    char address[32];
    char endpoint[28];
    unsigned int port = 0u;
    BOOL end_down;
    BOOL join_down;
    char edit_line[28];
    char local_error_line[28];
    const char *network_error;
    BOOL error_visible;
    unsigned int action_y;
    if (!quit_screen_visible() ||
        !SudekiMpLanArenaRuntimeGetStatus(&status)) {
        reset_edit_edges();
        return;
    }
    address[0] = '\0';
    if (SudekiMpLanArenaSessionGetDisplayEndpoint(
            address, sizeof(address), &port)) {
        if (strlen(address) < sizeof(last_endpoint_address)) {
            strcpy(last_endpoint_address, address);
            last_endpoint_port = port;
        }
    } else if (last_endpoint_address[0] != '\0') {
        strcpy(address, last_endpoint_address);
        port = last_endpoint_port;
    }
    snprintf(endpoint, sizeof(endpoint), "%s:%u",
        address[0] == '\0' ? "0.0.0.0" : address, port);
    (void)submit_text("MULTIPLAYER", 292u, 0xffffffffu);
    (void)submit_text(
        status.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ?
            "HOST ARENA - TAL" : "JOIN ARENA - AILISH",
        316u, 0xffcfe8ffu);
    (void)submit_text(endpoint, 340u, 0xffffffffu);
    (void)submit_text(phase_label(status.phase), 364u,
        status.peer_connected ? 0xff80ff80u : 0xffffff80u);
    network_error = failure_label(status.failure);
    error_visible = network_error != NULL ||
        last_action_error != ERROR_SUCCESS;
    if (network_error != NULL) {
        (void)submit_text(network_error, 388u, 0xffff8080u);
    } else if (last_action_error != ERROR_SUCCESS) {
        snprintf(local_error_line, sizeof(local_error_line),
            "LOCAL ERROR %lu", (unsigned long)last_action_error);
        (void)submit_text(local_error_line, 388u, 0xffff8080u);
    }
    if (status.local_role == SUDEKIMP_LAN_ARENA_ROLE_CLIENT_AILISH &&
        !status.peer_connected) {
        if (client_address_edit[0] == '\0' && address[0] != '\0') {
            strncpy(client_address_edit, address,
                sizeof(client_address_edit) - 1u);
            client_address_edit[sizeof(client_address_edit) - 1u] = '\0';
        }
        poll_client_address_edit();
        snprintf(edit_line, sizeof(edit_line), "EDIT %s", client_address_edit);
        action_y = error_visible ? 412u : 388u;
        (void)submit_text(edit_line, action_y, 0xffcfe8ffu);
        (void)submit_text("F9 JOIN  TYPE IP[:PORT]", action_y + 24u,
            0xffffd080u);
        join_down = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
        if (join_down && !join_key_was_down &&
            client_address_edit[0] != '\0') {
            if (SudekiMpLanArenaRuntimeJoinEndpoint(client_address_edit)) {
                last_action_error = ERROR_SUCCESS;
            } else {
                last_action_error = GetLastError();
            }
        }
        join_key_was_down = join_down;
        end_key_was_down = FALSE;
        return;
    }
    if (status.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL &&
        !status.peer_connected &&
        status.phase != SUDEKIMP_LAN_ARENA_CONNECTION_HOSTING) {
        (void)submit_text("F9  HOST ARENA", error_visible ? 412u : 400u,
            0xffffd080u);
        join_down = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
        if (join_down && !join_key_was_down) {
            if (SudekiMpLanArenaRuntimeHostArena()) {
                last_action_error = ERROR_SUCCESS;
            } else {
                last_action_error = GetLastError();
            }
        }
        join_key_was_down = join_down;
        end_key_was_down = FALSE;
        return;
    }
    (void)submit_text(
        status.local_role == SUDEKIMP_LAN_ARENA_ROLE_HOST_TAL ?
            "F10  END SESSION" : "F10  LEAVE SESSION",
        error_visible ? 412u : 400u, 0xffffd080u);
    end_down = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
    if (end_down && !end_key_was_down) {
        if (SudekiMpLanArenaRuntimeEndSession()) {
            last_action_error = ERROR_SUCCESS;
        } else {
            last_action_error = GetLastError();
        }
    }
    end_key_was_down = end_down;
    join_key_was_down = FALSE;
}

__attribute__((naked, noinline, used))
static void quit_render_entry(void) {
    __asm__ volatile(
        "call *_original_quit_render\n\t"
        "pushfl\n\t"
        "pushal\n\t"
        "call _render_panel\n\t"
        "popal\n\t"
        "popfl\n\t"
        "ret\n\t"
    );
}

BOOL SudekiMpInstallLanArenaPausePanel(HMODULE game_module) {
    if (game_module == NULL || game_base != NULL || original_quit_render != NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    game_base = (uint8_t *)game_module;
    if (memcmp(game_base + RVA_PC_QUIT_SCREEN_RENDER,
            expected_quit_render_entry,
            sizeof(expected_quit_render_entry)) != 0 ||
        memcmp(game_base + RVA_UI_TEXT_SUBMIT,
            expected_ui_text_submit_entry,
            sizeof(expected_ui_text_submit_entry)) != 0) {
        game_base = NULL;
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    original_quit_render = (QuitScreenRenderFunction)(
        game_base + RVA_PC_QUIT_SCREEN_RENDER);
    if (!SudekiMpInstallRelativeCallHook(
            &quit_render_hook,
            game_base + RVA_PC_QUIT_SCREEN_RENDER_CALL,
            original_quit_render,
            quit_render_entry)) {
        DWORD error = GetLastError();
        original_quit_render = NULL;
        game_base = NULL;
        SetLastError(error);
        return FALSE;
    }
    client_address_edit[0] = '\0';
    last_endpoint_address[0] = '\0';
    last_endpoint_port = 0u;
    last_action_error = ERROR_SUCCESS;
    reset_edit_edges();
    SudekiMpLogWrite(
        "lan_arena_pause_panel event=install layer=native_pc_quit_screen "
        "action=F10_end_or_leave native_pause_ui=unchanged\r\n");
    return TRUE;
}

void SudekiMpUninstallLanArenaPausePanel(void) {
    SudekiMpRestoreRelativeCallHook(&quit_render_hook);
    original_quit_render = NULL;
    game_base = NULL;
    client_address_edit[0] = '\0';
    last_endpoint_address[0] = '\0';
    last_endpoint_port = 0u;
    last_action_error = ERROR_SUCCESS;
    reset_edit_edges();
}
