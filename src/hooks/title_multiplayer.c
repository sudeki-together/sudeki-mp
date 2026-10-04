#include "hooks/title_multiplayer.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "engine/render_phase_abi.h"
#include <math.h>
#include "ui/title_menu_view.h"
#include "ui/title_lobby.h"
#include "hooks/lobby_gameplay.h"
#include "hooks/lan_story_load.h"
#include <stdint.h>
#include <string.h>
#include <wchar.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Title menu requires the supported 32-bit native ABI"
#endif

typedef unsigned (__attribute__((thiscall)) *TitleAction)(void *, unsigned, unsigned, unsigned);
/* Forward the native update stack word bit-for-bit; this adapter does not
 * reinterpret or alter the native clock argument. */
typedef void (__attribute__((thiscall)) *TitleUpdate)(void *, uint32_t);
typedef void (__attribute__((stdcall)) *UiFlush)(void *);
enum {
    TITLE_VTABLE = 0x002cb1fc, TITLE_ACTION = 0x000a0360,
    TITLE_UPDATE = 0x000a0060, UI_FLUSH_CALL = 0x0000a760,
    UI_FLUSH = 0x0000a820, UI_RENDER_CALL = 0x0000a689, UI_RENDER = 0x001d4750,
    ROW_VTABLE = 0x002d1da8, NODE_VTABLE = 0x002d1df0,
    RENDER_OBJECT_VTABLE = 0x002dd700, ANIM_VTABLE = 0x002df8ec,
    OPTIONS_VTABLE = 0x002d1cb4, LOAD_VTABLE = 0x002ca89c,
    LANGUAGE = 0x003c3000,
    UI_SCENE = 0x00408d1c, MODAL_GLOBAL = 0x00408d70,
    D3D_DEVICE = 0x003c31dc, NO_NATIVE_ITEM = 0xff
};
typedef struct TitleSnapshot {
    void *owner;
    void *scene;
    void *native_rows[5];
    unsigned native_count, count, multiplayer_index;
    unsigned native_index[SUDEKIMP_TITLE_MAX_ROWS];
    SudekiMpTitleLabel label_ids[SUDEKIMP_TITLE_MAX_ROWS];
    wchar_t labels[SUDEKIMP_TITLE_MAX_ROWS][28];
} TitleSnapshot;

static uint8_t *game_base;
static TitleAction original_action;
static TitleUpdate original_update;
static UiFlush original_flush;
static void *original_render;
static SudekiMpPointerHook action_hook, update_hook;
static SudekiMpRelativeCallHook flush_hook, render_hook;
static volatile LONG admission, stopping, callbacks, resources_released = 1;
static DWORD native_thread, last_update;
static TitleSnapshot current;
static unsigned selected, root_selection, generation;
static BOOL multiplayer_page, confirm_armed, displayed, draw_failure_logged;
static BOOL panel_leaving, fade_in_active;
static DWORD fade_in_started;
static HWND game_window;
static POINT last_cursor;
static BOOL cursor_known, frame_composed, native_transition, page_confirm, root_owned;
static DWORD focus_started, transition_started, page_started;
static unsigned page_confirm_generation;
static unsigned hide_rejected_generation;
static DWORD draw_failed_at;
static void *preparation_owner;
static DWORD preparation_seen;
static BOOL preparation_logged;
static BOOL saved_load_probe,saved_load_probe_pending;
static SudekiMpSaveFingerprint saved_load_fingerprint;
static SudekiMpTitleButtonState view_state = SUDEKIMP_TITLE_BUTTON_FOCUS;
/* The panel's Windows messages are independent of the native one-column
 * menu's confirm events. The hook and consumer run on the verified UI thread. */
enum { MOUSE_QUEUE_SIZE=16 };
typedef struct PanelMouseEvent {
    POINT point;
    unsigned page_revision, title_generation;
    BOOL down;
} PanelMouseEvent;
static HHOOK mouse_hook;
static HWND mouse_window;
static PanelMouseEvent mouse_queue[MOUSE_QUEUE_SIZE];
static unsigned mouse_count, mouse_pressed_row, mouse_pressed_page;
static BOOL mouse_overflow, mouse_pressed, mouse_claimed;
static BOOL mouse_sampled, mouse_left_down;
static DWORD mouse_claimed_at;
static unsigned activate_selection(void *owner,unsigned phase,unsigned event,unsigned argument);

typedef struct RowVisibility {
    void *row, *node, *render_object, *model, *animation, *scene_renderer;
    uint32_t saved_hidden;
} RowVisibility;
static RowVisibility hidden_rows[5];
static unsigned hidden_count;
static void *hidden_owner, *hidden_scene;

_Static_assert(SUDEKIMP_TITLE_FIRST_ROW +
    (SUDEKIMP_TITLE_MAX_ROWS - 1) * SUDEKIMP_TITLE_ROW_PITCH +
    SUDEKIMP_TITLE_ROW_HEIGHT + 4 < SUDEKIMP_TITLE_CANVAS_HEIGHT, "Title rows must fit the canvas");
_Static_assert(sizeof(wchar_t) == 2, "Native menu strings are UTF-16");

static BOOL readable(const void *p, size_t size) {
    MEMORY_BASIC_INFORMATION info;
    uintptr_t start = (uintptr_t)p;
    if (!p || !size || start > UINTPTR_MAX - size ||
        !VirtualQuery(p, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return FALSE;
    DWORD access = info.Protect & 0xffu;
    if (access != PAGE_READONLY && access != PAGE_READWRITE &&
        access != PAGE_WRITECOPY && access != PAGE_EXECUTE_READ &&
        access != PAGE_EXECUTE_READWRITE && access != PAGE_EXECUTE_WRITECOPY) return FALSE;
    return start + size <= (uintptr_t)info.BaseAddress + info.RegionSize;
}

static BOOL exact_thread(void) {
    return native_thread && native_thread == GetCurrentThreadId();
}

static BOOL modal_clear(void) {
    void *modal = *(void **)(game_base + MODAL_GLOBAL);
    return readable(modal, 0x1b8) && !*((uint8_t *)modal + 0x1b7);
}

static BOOL action_equals(const void *raw, const char *expected) {
    const uint32_t *record = raw;
    unsigned size = record[0] & 0x7fffffffu;
    const char *text = record[0] & 0x80000000u ?
        (const char *)(record + 1) : *(const char *const *)(record + 1);
    size_t length = strlen(expected);
    return size == length && readable(text, length + 1) &&
        !memcmp(text, expected, length + 1);
}

static BOOL copy_label(const void *raw, wchar_t out[28]) {
    const uint32_t *record = raw;
    unsigned length = record[0] & 0x7fffffffu;
    const wchar_t *text = record[0] & 0x80000000u ?
        (const wchar_t *)(record + 1) : *(const wchar_t *const *)(record + 1);
    if (!length || length > 27 || !readable(text, (length + 1) * sizeof(*text)) ||
        text[length] != 0) return FALSE;
    memcpy(out, text, (length + 1) * sizeof(*text));
    return TRUE;
}

static BOOL title_identity(const void *owner) {
    return game_base && readable(owner, 0x1844) &&
        *(void *const *)owner == game_base + TITLE_VTABLE &&
        *(unsigned *)(game_base + LANGUAGE) == 0;
}

static BOOL root_page_closed(const uint8_t *owner) {
    const uint8_t *page = *(uint8_t *const *)(owner + 0xac);
    if (!page) return TRUE;
    /* 004A0060 returns from Options/Load when page+29 becomes zero.
     * 004A0F40 (state 9) and 004A09A0 (9 -> 5) retain owner+AC.
     * It is a cached page, not an open-page flag. Accept only the title's
     * exact known, closed child; never clear or commandeer that pointer. */
    if (!readable(page, 0x4b) || page[0x29]) return FALSE;
    if (page == *(uint8_t *const *)(owner + 0xb0))
        return *(void *const *)page == game_base + OPTIONS_VTABLE;
    if (page == *(uint8_t *const *)(owner + 0xb4))
        return *(void *const *)page == game_base + LOAD_VTABLE && !page[0x4a];
    return FALSE;
}

static BOOL capture_title(void *raw, TitleSnapshot *out, BOOL presentation) {
    static const char *const names[5] = {
        "Continue", "StartNewGame", "Options", "ShowFrontEndCredits", "QuitGame"
    };
    static const SudekiMpTitleLabel ids[5] = {
        SUDEKIMP_TITLE_CONTINUE, SUDEKIMP_TITLE_NEW_GAME, SUDEKIMP_TITLE_OPTIONS,
        SUDEKIMP_TITLE_CREDITS, SUDEKIMP_TITLE_QUIT
    };
    uint8_t *owner = raw;
    void *scene;
    unsigned state;
    unsigned count, first;
    if (!title_identity(owner)) return FALSE;
    /* The new atlas covers English. Other languages retain their stock UI. */
    state = *(unsigned *)(owner + 0x44);
    /* States 4/9 are the native root entrance/return. Observe presentation
     * there, while action admission remains restricted to state 5. */
    if (state != 5 && !(presentation && (state == 4 || state == 9 ||
            (native_transition && current.owner == owner &&
             (state == 6 || state == 7 || state == 10))))) return FALSE;
    if ((state == 4 || state == 5 || state == 9) && !root_page_closed(owner)) return FALSE;
    scene = *(void **)(game_base + UI_SCENE);
    if (!readable(scene, 0x174) ||
        (state == 5 && *(void **)((uint8_t *)scene + 0x170) != owner))
        return FALSE;
    count = *(unsigned *)(owner + 0x17d8);
    if ((count != 4 && count != 5) || *(unsigned *)(owner + 0x17d4) >= count)
        return FALSE;
    memset(out, 0, sizeof(*out));
    out->owner = owner;
    out->scene = scene;
    out->native_count = count;
    memcpy(out->native_rows, owner + 0x70, sizeof(out->native_rows));
    first = count == 4 ? 1 : 0;
    for (unsigned i = 0; i < count; ++i) {
        unsigned row = out->count;
        if (!action_equals(owner + 0xfd0 + i * 0x20, names[first + i]) ||
            !copy_label(owner + 0xd0 + i * 0x3c, out->labels[row])) return FALSE;
        out->native_index[row] = i;
        out->label_ids[row] = ids[first + i];
        ++out->count;
        if (first + i == 1) {
            out->multiplayer_index = out->count;
            out->native_index[out->count] = NO_NATIVE_ITEM;
            out->label_ids[out->count] = SUDEKIMP_TITLE_MULTIPLAYER;
            wcscpy(out->labels[out->count++], L"Multiplayer");
        }
    }
    return out->count <= SUDEKIMP_TITLE_MAX_ROWS;
}

static BOOL current_exact(void *owner) {
    TitleSnapshot observed;
    return exact_thread() && current.owner == owner &&
        capture_title(owner, &observed, FALSE) && !memcmp(&observed, &current, sizeof(current));
}

static BOOL presentation_exact(void) {
    TitleSnapshot observed;
    return exact_thread() && current.owner &&
        capture_title(current.owner, &observed, TRUE) && !memcmp(&observed, &current, sizeof(current));
}

static void select_row(unsigned row) {
    if (selected != row) {
        selected = row;
        focus_started = GetTickCount();
        view_state = SUDEKIMP_TITLE_BUTTON_FOCUS;
    }
}

static void clear_page(void) {
    SudekiMpLobbyUiClose();
    current.owner = NULL;
    native_transition = page_confirm = frame_composed = root_owned = FALSE;
    multiplayer_page = FALSE;
    panel_leaving = fade_in_active = FALSE;
    displayed = FALSE;
    cursor_known = FALSE;
    game_window = NULL;
    mouse_count=0; mouse_pressed=mouse_claimed=mouse_overflow=FALSE;
    mouse_sampled=FALSE;
}

static unsigned row_count(void) { return multiplayer_page ? SudekiMpLobbyUiView()->count : current.count; }
static unsigned enabled_rows(void) {
    if(multiplayer_page) return saved_load_probe?0u:SudekiMpLobbyUiView()->enabled;
    unsigned enabled=(1u<<current.count)-1u;
    if(saved_load_probe) for(unsigned i=0;i<current.count;++i)
        if(current.label_ids[i]==SUDEKIMP_TITLE_MULTIPLAYER) enabled&=~(1u<<i);
    return enabled;
}
static BOOL view_hit(unsigned *row, POINT *point) {
    return multiplayer_page ? SudekiMpTitlePanelHit(game_window,row_count(),
        &SudekiMpLobbyUiView()->text,row,point) :
        SudekiMpTitleViewHit(game_window,row_count(),row,point);
}
static void begin_panel_close(void) {
    panel_leaving=page_confirm=TRUE;
    page_started=GetTickCount(); page_confirm_generation=generation;
}

static LRESULT CALLBACK panel_mouse_messages(int code,WPARAM removed,LPARAM raw) {
    InterlockedIncrement(&callbacks);
    if (code>=0 && removed==PM_REMOVE && exact_thread() &&
        InterlockedCompareExchange(&admission,0,0) && !InterlockedCompareExchange(&stopping,0,0)) {
        const MSG *message=(const MSG *)raw;
        if (message && message->hwnd==mouse_window && (multiplayer_page || root_owned)) {
            if (message->message==WM_KILLFOCUS || message->message==WM_CANCELMODE) {
                mouse_count=0; mouse_pressed=FALSE; mouse_overflow=FALSE;
            } else if (message->message==WM_RBUTTONDOWN || message->message==WM_RBUTTONUP ||
                       message->message==WM_RBUTTONDBLCLK) {
                mouse_claimed=TRUE; mouse_claimed_at=GetTickCount();
                mouse_pressed=FALSE;
            } else if (multiplayer_page && (message->message==WM_LBUTTONDOWN || message->message==WM_LBUTTONUP ||
                       message->message==WM_LBUTTONDBLCLK)) {
                mouse_claimed=TRUE; mouse_claimed_at=GetTickCount();
                if (displayed && !page_confirm && !fade_in_active && modal_clear() &&
                    current_exact(current.owner) && GetForegroundWindow()==mouse_window) {
                    if (mouse_count==MOUSE_QUEUE_SIZE) mouse_overflow=TRUE;
                    else {
                        PanelMouseEvent *event=&mouse_queue[mouse_count++];
                        event->point.x=(short)LOWORD(message->lParam);
                        event->point.y=(short)HIWORD(message->lParam);
                        event->page_revision=SudekiMpLobbyUiPageRevision();
                        event->title_generation=generation;
                        event->down=message->message!=WM_LBUTTONUP;
                    }
                }
            }
        }
    }
    LRESULT result=CallNextHookEx(mouse_hook,code,removed,raw);
    InterlockedDecrement(&callbacks);
    return result;
}
static BOOL remove_mouse_hook(void) {
    if (mouse_hook && !UnhookWindowsHookEx(mouse_hook)) return FALSE;
    mouse_hook=NULL; mouse_window=NULL; mouse_count=0;
    mouse_pressed=mouse_claimed=mouse_overflow=FALSE;
    return TRUE;
}
static BOOL ensure_mouse_hook(void) {
    DWORD pid=0;
    if (!exact_thread() || !game_window ||
        GetWindowThreadProcessId(game_window,&pid)!=native_thread || pid!=GetCurrentProcessId()) return FALSE;
    if (mouse_hook && mouse_window==game_window) return TRUE;
    if (!remove_mouse_hook()) return FALSE;
    HHOOK hook=SetWindowsHookExW(WH_GETMESSAGE,panel_mouse_messages,NULL,native_thread);
    if (!hook) return FALSE;
    mouse_window=game_window; mouse_hook=hook;
    SudekiMpLogWrite("title_multiplayer event=panel_mouse installed=1 scope=owned_window_ui_thread\r\n");
    return TRUE;
}
static void service_panel_mouse(void) {
    BOOL left=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
    BOOL right=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;
    if (left || right) { mouse_claimed=TRUE; mouse_claimed_at=GetTickCount(); }
    if (!ensure_mouse_hook() || !displayed || native_transition || page_confirm ||
        fade_in_active || !modal_clear() || !current_exact(current.owner) ||
        GetForegroundWindow()!=game_window) {
        mouse_count=0; mouse_pressed=FALSE; mouse_sampled=FALSE; return;
    }
    /* Retail/Wine may consume button messages before WH_GETMESSAGE. Sample
     * the physical left button as well; native right-confirm never selects a
     * panel control. Re-arm after release on focus/page transitions. */
    if (!mouse_sampled) { mouse_left_down=left; mouse_sampled=TRUE; }
    else if (!mouse_count && left!=mouse_left_down) {
        POINT point;
        if (GetCursorPos(&point) && ScreenToClient(game_window,&point)) {
            mouse_queue[mouse_count++]=(PanelMouseEvent){point,
                multiplayer_page?SudekiMpLobbyUiPageRevision():0,generation,left};
            mouse_claimed=TRUE; mouse_claimed_at=GetTickCount();
        } else mouse_pressed=FALSE;
    }
    mouse_left_down=left;
    if (mouse_overflow) {
        mouse_count=0; mouse_pressed=FALSE;
        if (!(GetAsyncKeyState(VK_LBUTTON)&0x8000)) mouse_overflow=FALSE;
        return;
    }
    for (unsigned i=0;i<mouse_count;++i) {
        const PanelMouseEvent *event=&mouse_queue[i];
        unsigned hit;
        POINT point=event->point;
        BOOL same_page=event->title_generation==generation &&
            event->page_revision==(multiplayer_page?SudekiMpLobbyUiPageRevision():0);
        BOOL hit_enabled=same_page && !right && (multiplayer_page?
            SudekiMpTitlePanelHitPoint(game_window,row_count(),&SudekiMpLobbyUiView()->text,event->point,&hit):
            SudekiMpTitleViewHit(game_window,row_count(),&hit,&point)) && (enabled_rows()&(1u<<hit));
        if (event->down) {
            mouse_pressed=hit_enabled;
            if (hit_enabled) {
                if (multiplayer_page) SudekiMpLobbyUiEndEdit();
                select_row(hit);
                if (multiplayer_page) SudekiMpLobbyUiArm(hit);
                mouse_pressed_row=hit; mouse_pressed_page=event->page_revision;
            }
        } else {
            BOOL commit=mouse_pressed && hit_enabled && hit==mouse_pressed_row &&
                mouse_pressed_page==event->page_revision;
            mouse_pressed=FALSE;
            if (commit) {
                select_row(hit);
                if (multiplayer_page) {
                    page_confirm=TRUE; page_started=GetTickCount(); page_confirm_generation=generation;
                } else (void)activate_selection(current.owner,5,0,0);
                SudekiMpLogFormat("title_multiplayer event=left_mouse action=click row=%u page=%u\r\n",
                    hit,event->page_revision);
                break;
            }
        }
    }
    mouse_count=0;
}

static BOOL writable(void *p, size_t size) {
    MEMORY_BASIC_INFORMATION info;
    if (!readable(p, size) || !VirtualQuery(p, &info, sizeof(info))) return FALSE;
    DWORD access = info.Protect & 0xffu;
    return access == PAGE_READWRITE || access == PAGE_WRITECOPY ||
        access == PAGE_EXECUTE_READWRITE || access == PAGE_EXECUTE_WRITECOPY;
}

static BOOL row_exact(const RowVisibility *lease, unsigned index, void *owner, void *scene) {
    const uint8_t *row = lease->row, *node = lease->node, *object = lease->render_object;
    return readable(owner, 0x84) && *(void **)owner == game_base + TITLE_VTABLE &&
        *(void **)((uint8_t *)owner + 0x70 + index * 4) == row &&
        readable(scene, 0x74) && *(void **)((uint8_t *)scene + 0x70) == lease->scene_renderer &&
        readable(row, 0xc0) && *(void **)row == game_base + ROW_VTABLE &&
        *(void **)(row + 0xbc) == node && readable(node, 0x1c) &&
        *(void **)node == game_base + NODE_VTABLE &&
        *(void **)(node + 8) == object && *(void **)(node + 0xc) == lease->model &&
        *(void **)(node + 0x10) == lease->animation &&
        *(void **)(node + 0x14) == lease->scene_renderer &&
        writable((void *)object, 0x38) && *(void **)object == game_base + RENDER_OBJECT_VTABLE &&
        *(void **)(object + 0x14) == lease->model &&
        readable(lease->animation, 4) && *(void **)lease->animation == game_base + ANIM_VTABLE;
}

static BOOL restore_rows(void) {
    BOOL okay = TRUE;
    for (unsigned i = 0; i < hidden_count; ++i) {
        RowVisibility *lease = &hidden_rows[i];
        if (!lease->row) continue;
        if (!row_exact(lease, i, hidden_owner, hidden_scene)) { okay = FALSE; continue; }
        uint32_t *flags = (uint32_t *)((uint8_t *)lease->render_object + 0x34);
        /* Own only the hidden bit for this draw. Native rendering can update
         * other flags; restoring the entire old word would discard its work. */
        *flags = (*flags & ~4u) | lease->saved_hidden;
        lease->row = NULL;
    }
    if (okay) { hidden_count = 0; hidden_owner = hidden_scene = NULL; }
    return okay;
}

static BOOL hide_rows(void *renderer, void *scene) {
    RowVisibility rows[5];
    if (hidden_count || !presentation_exact() || scene != current.scene ||
        *(void **)((uint8_t *)scene + 0x70) != renderer) return FALSE;
    for (unsigned i = 0; i < 5; ++i) {
        uint8_t *row = current.native_rows[i], *node;
        if (!readable(row, 0xc0)) return FALSE;
        node = *(uint8_t **)(row + 0xbc);
        if (!readable(node, 0x1c)) return FALSE;
        rows[i] = (RowVisibility){row, node, *(void **)(node + 8),
            *(void **)(node + 0xc), *(void **)(node + 0x10), renderer, 0};
        if (!row_exact(&rows[i], i, current.owner, scene)) return FALSE;
        for (unsigned j = 0; j < i; ++j)
            if (rows[i].render_object == rows[j].render_object) return FALSE;
        rows[i].saved_hidden = *(uint32_t *)((uint8_t *)rows[i].render_object + 0x34) & 4u;
    }
    hidden_owner = current.owner;
    hidden_scene = scene;
    memcpy(hidden_rows, rows, sizeof(rows));
    hidden_count = 5;
    /* 005D4E70 filters render objects with (flags & 0x204)==pass. This
     * transient bit suppresses both passes without touching animation clocks,
     * material colours, scene membership, or native action/row records. */
    for (unsigned i = 0; i < 5; ++i)
        *(uint32_t *)((uint8_t *)rows[i].render_object + 0x34) |= 4u;
    return TRUE;
}

__attribute__((noinline, used))
static void __cdecl title_render(void *renderer, void *scene) {
    BOOL hidden = FALSE;
    BOOL ready = FALSE;
    InterlockedIncrement(&callbacks);
    if (scene == current.scene) frame_composed = FALSE;
    if (exact_thread() && restore_rows() &&
        InterlockedCompareExchange(&admission, 0, 0) && !InterlockedCompareExchange(&stopping, 0, 0)) {
        BOOL root = presentation_exact() && scene == current.scene && modal_clear();
        /* Start on the render thread during the native loading/intro states,
         * before state 4 exposes its rows. This lease is preparation-only:
         * it admits neither row hiding nor menu input. */
        BOOL warm = title_identity(preparation_owner) &&
            GetTickCount() - preparation_seen <= 250u &&
            *(unsigned *)((uint8_t *)preparation_owner + 0x44) >= 1 &&
            *(unsigned *)((uint8_t *)preparation_owner + 0x44) <= 3 &&
            scene == *(void **)(game_base + UI_SCENE) && readable(scene, 0x74) &&
            *(void **)((uint8_t *)scene + 0x70) == renderer;
        if ((root || warm) && (!draw_failure_logged || GetTickCount() - draw_failed_at >= 500u)) {
            DWORD start = GetTickCount();
            InterlockedExchange(&resources_released, 0);
            ready = SudekiMpTitleViewPrepare(*(void **)(game_base + D3D_DEVICE));
            if (ready && !preparation_logged) {
                preparation_logged = TRUE;
                SudekiMpLogFormat("title_multiplayer event=prepared state=%u core_ms=%lu before_root=%u\r\n",
                    *(unsigned *)((uint8_t *)(warm ? preparation_owner : current.owner) + 0x44),
                    (unsigned long)(GetTickCount() - start), warm ? 1u : 0u);
            }
        }
        if (root && ready) {
            hidden = hide_rows(renderer, scene);
            if (!hidden && hide_rejected_generation != generation) {
                hide_rejected_generation = generation;
                SudekiMpLogFormat("title_multiplayer event=row_identity_rejected generation=%u native_visible=1\r\n", generation);
            }
        }
    }
    SudekiMpCallRenderPhase(renderer, scene, original_render);
    if (hidden) {
        if (restore_rows()) frame_composed = TRUE;
        else {
            InterlockedExchange(&admission, 0);
            SudekiMpLogWrite("title_multiplayer event=row_restore_pending admission=closed\r\n");
        }
    }
    InterlockedDecrement(&callbacks);
}

/* Exact 0040A689 call: EAX renderer, EDI scene, no stack words. */
__attribute__((naked, noinline, used))
static void title_render_entry(void) {
    __asm__ volatile(
        "pushl %edi\n\t"
        "pushl %eax\n\t"
        "call _title_render\n\t"
        "addl $8, %esp\n\t"
        "ret\n\t");
}

static void return_to_root(void) {
    SudekiMpLobbyUiClose();
    multiplayer_page = FALSE;
    panel_leaving = FALSE;
    fade_in_active = TRUE; fade_in_started = GetTickCount();
    selected = root_selection;
    focus_started = GetTickCount();
    view_state = SUDEKIMP_TITLE_BUTTON_RETURN;
    cursor_known = FALSE;
    SudekiMpLogFormat("title_multiplayer event=back generation=%u\r\n", generation);
}

static unsigned activate_selection(void *owner,unsigned phase,unsigned event,unsigned argument) {
    if (!(enabled_rows()&(1u<<selected))) return 1;
    if (multiplayer_page || current.native_index[selected]==NO_NATIVE_ITEM) {
        if (multiplayer_page) SudekiMpLobbyUiArm(selected);
        page_confirm=TRUE; page_started=GetTickCount(); page_confirm_generation=generation;
        return 1;
    }
    *(unsigned *)((uint8_t *)owner+0x17d4)=current.native_index[selected];
    SudekiMpLogFormat("title_multiplayer event=native_action index=%u generation=%u\r\n",
        current.native_index[selected],generation);
    native_transition=TRUE; transition_started=GetTickCount(); displayed=FALSE;
    return original_action(owner,phase,event,argument);
}

static unsigned __attribute__((thiscall)) title_action(void *owner,
    unsigned phase, unsigned event, unsigned argument) {
    unsigned result = 1;
    InterlockedIncrement(&callbacks);
    if (!InterlockedCompareExchange(&admission, 0, 0) ||
        InterlockedCompareExchange(&stopping, 0, 0) || !current_exact(owner)) {
        result = original_action(owner, phase, event, argument);
        goto done;
    }
    if (phase == 6 && event < 4) {
        confirm_armed = TRUE;
        /* Preserve the native latch release even for a mod-owned press. */
        result = original_action(owner, phase, event, argument);
        goto done;
    }
    if (phase != 5) {
        if (phase == 2) { confirm_armed = FALSE; cursor_known = FALSE; page_confirm = panel_leaving = FALSE; }
        result = original_action(owner, phase, event, argument);
        goto done;
    }
    if (!displayed && !multiplayer_page && !root_owned) {
        result = original_action(owner, phase, event, argument);
        goto done;
    }
    if (!displayed || page_confirm || fade_in_active || !modal_clear() || GetTickCount() - last_update > 250u)
        goto done;
    *(uint32_t *)((uint8_t *)owner + 0x54) =
        *(uint32_t *)((uint8_t *)owner + 0x50);
    /* UILayerOptionsMenu input at 0051D0A0 maps phase-5 events 1/3
     * to its native exit operation (0051D132 loads state 13). */
    if (multiplayer_page && SudekiMpLobbyUiEditing()) goto done;
    /* The panel consumes pointer activation from its own bounded message
     * queue. Do not let the same physical click activate the old keyboard
     * selection through a delayed native press/release notification. */
    if (event<4 &&
        (((GetAsyncKeyState(VK_LBUTTON)|GetAsyncKeyState(VK_RBUTTON))&0x8000) || mouse_pressed ||
         (mouse_claimed && (DWORD)(GetTickCount()-mouse_claimed_at)<250u))) goto done;
    if (multiplayer_page && (event == 1 || event == 3)) {
        if (SudekiMpLobbyUiBack(&selected)) begin_panel_close();
        else { focus_started=GetTickCount(); view_state=SUDEKIMP_TITLE_BUTTON_RETURN; cursor_known=FALSE; }
        goto done;
    }
    if (event == 6 || event == 7) {
        unsigned count = row_count(), enabled = enabled_rows(), next = selected;
        do {
            next = event == 6 ? (next + count - 1) % count : (next + 1) % count;
        } while (!(enabled & (1u << next)));
        select_row(next);
        goto done;
    }
    if ((event != 0 && event != 2) || !confirm_armed) goto done;
    confirm_armed = FALSE;
    /* Only exact native row indices reach the retail dispatcher. */
    result=activate_selection(owner,phase,event,argument);
done:
    InterlockedDecrement(&callbacks);
    return result;
}

static void __attribute__((thiscall)) title_update(void *owner, uint32_t update_word) {
    TitleSnapshot observed;
    InterlockedIncrement(&callbacks);
    original_update(owner, update_word);
    if (!InterlockedCompareExchange(&admission, 0, 0) ||
        InterlockedCompareExchange(&stopping, 0, 0)) goto done;
    if (!native_thread) native_thread = GetCurrentThreadId();
    if (!exact_thread()) goto done;
    SudekiMpLanStoryLoadServiceTitle(owner);
    preparation_owner = title_identity(owner) ? owner : NULL;
    preparation_seen = GetTickCount();
    if (!capture_title(owner, &observed, TRUE)) {
        if (current.owner && title_identity(owner))
            SudekiMpLogFormat("title_multiplayer event=leave_root generation=%u state=%u\r\n",
                generation, *(unsigned *)((uint8_t *)owner + 0x44));
        clear_page(); goto done;
    }
    if (native_transition && *(unsigned *)((uint8_t *)owner + 0x44) == 5)
        native_transition = FALSE;
    if (!current.owner || memcmp(&observed, &current, sizeof(current))) {
        clear_page();
        current = observed;
        selected = 0;
        for (unsigned row = 0; row < current.count; ++row)
            if (current.native_index[row] == *(unsigned *)((uint8_t *)owner + 0x17d4))
                selected = row;
        focus_started = GetTickCount();
        view_state = SUDEKIMP_TITLE_BUTTON_RETURN;
        confirm_armed = !*((uint8_t *)owner + 0x60);
        ++generation;
        SudekiMpLogFormat("title_multiplayer event=root generation=%u native_count=%u rows=%u state=%u\r\n",
            generation, current.native_count, current.count, *(unsigned *)((uint8_t *)owner + 0x44));
    }
    last_update = GetTickCount();
    if (fade_in_active && (DWORD)(last_update-fade_in_started)>=300u) fade_in_active=FALSE;
    if(saved_load_probe_pending && current_exact(owner) && modal_clear() && !hidden_count) {
        saved_load_probe_pending=FALSE;
        SudekiMpSaveCatalog catalog;
        BOOL prepared=FALSE;
        DWORD error=ERROR_FILE_NOT_FOUND;
        if(SudekiMpSaveCatalogRefresh(&catalog)) {
            for(unsigned i=0;i<catalog.count;++i)
                if(catalog.entries[i].folder_slot==saved_load_fingerprint.folder_slot) {
                    prepared=SudekiMpLanStoryLoadPrepare(&catalog,i,&saved_load_fingerprint);
                    error=prepared?ERROR_SUCCESS:GetLastError();
                    break;
                }
        } else error=GetLastError();
        if(prepared) for(unsigned row=0;row<current.count;++row)
            if(current.label_ids[row]==SUDEKIMP_TITLE_CONTINUE && SudekiMpLanStoryLoadArmTitle(owner)) {
                page_confirm=panel_leaving=fade_in_active=FALSE;
                native_transition=TRUE; transition_started=GetTickCount();
                *(unsigned *)((uint8_t *)owner+0x17d4)=current.native_index[row];
                (void)original_action(owner,5,0,0);
                SudekiMpLogWrite("lan_story_load probe=native_continue window=same gameplay_enabled=0\r\n");
                goto done;
            }
        if(prepared) error=GetLastError()?GetLastError():ERROR_NOT_FOUND;
        (void)SudekiMpLanStoryLoadCancel();
        SudekiMpLogFormat("lan_story_load probe=refused prepared=%u error=%lu\r\n",
            prepared,(unsigned long)error);
    }
    if (page_confirm && page_confirm_generation == generation && current_exact(owner)) {
        if (!modal_clear()) { page_confirm = panel_leaving = FALSE; goto done; }
        DWORD delay=panel_leaving?300u:multiplayer_page?80u:720u;
        if (last_update - page_started >= delay && displayed) {
            page_confirm = FALSE;
            if (panel_leaving) return_to_root();
            else if (multiplayer_page) {
                if (SudekiMpLobbyUiCommit(&selected)) begin_panel_close();
                else { focus_started=last_update; view_state=SUDEKIMP_TITLE_BUTTON_RETURN; cursor_known=FALSE; }
            } else {
                root_selection = selected;
                multiplayer_page = TRUE;
                fade_in_active=TRUE; fade_in_started=last_update;
                SudekiMpLobbyUiOpen();
                selected = 0;
                cursor_known = FALSE;
                focus_started = last_update;
                view_state = SUDEKIMP_TITLE_BUTTON_RETURN;
                SudekiMpLogFormat("title_multiplayer event=open generation=%u host_actor=unassigned lobby_enabled=1 gameplay_enabled=0\r\n", generation);
            }
        }
    }
    service_panel_mouse();
    if (multiplayer_page) {
        SudekiMpLobbyUiPoll(game_window, displayed && !page_confirm && !fade_in_active && current_exact(owner) && modal_clear());
        if (SudekiMpLobbyGameplayNeedsTitle() && current_exact(owner) && modal_clear() && !hidden_count) {
            unsigned action=SudekiMpLobbyGameplaySavedGame()?SUDEKIMP_TITLE_CONTINUE:SUDEKIMP_TITLE_NEW_GAME;
            for (unsigned row=0;row<current.count;++row) if (current.label_ids[row]==action) {
                if (SudekiMpLobbyGameplayArmTitle(owner)) {
                    page_confirm=panel_leaving=fade_in_active=FALSE;
                    native_transition=TRUE; transition_started=GetTickCount();
                    *(unsigned *)((uint8_t *)owner+0x17d4)=current.native_index[row];
                    (void)original_action(owner,5,0,0);
                    SudekiMpLogFormat("title_multiplayer event=gameplay_fade window=same destination=%s\r\n",
                        SudekiMpLobbyGameplaySavedGame()?"saved_game":"testroom");
                    goto done;
                }
            }
        }
        if (selected>=row_count() || !(enabled_rows() & (1u<<selected))) {
            for (unsigned row=0;row<row_count();++row)
                if (enabled_rows() & (1u<<row)) { select_row(row); break; }
        }
    }
    if (!root_owned && !multiplayer_page && !native_transition && !page_confirm) {
        unsigned native_selected = *(unsigned *)((uint8_t *)owner + 0x17d4);
        for (unsigned row = 0; row < current.count; ++row)
            if (current.native_index[row] == native_selected) select_row(row);
    }
    if (displayed && !native_transition && !page_confirm && !fade_in_active && current_exact(owner) && modal_clear() &&
        !(multiplayer_page && SudekiMpLobbyUiEditing())) {
        unsigned hit;
        POINT point = {-1, -1};
        if (view_hit(&hit, &point)) {
            if ((!cursor_known || point.x != last_cursor.x || point.y != last_cursor.y) &&
                (enabled_rows() & (1u << hit))) select_row(hit);
            last_cursor = point;
            cursor_known = TRUE;
        } else cursor_known = FALSE;
    }
done:
    InterlockedDecrement(&callbacks);
}

static void __attribute__((stdcall)) title_flush(void *scene) {
    static const SudekiMpTitleLabel choices[SUDEKIMP_TITLE_MAX_ROWS] = {0};
    BOOL render_state_ready = TRUE;
    InterlockedIncrement(&callbacks);
    if (exact_thread()) render_state_ready = SudekiMpTitleViewRestore();
    original_flush(scene);
    if (exact_thread() && !InterlockedCompareExchange(&stopping,0,0)) SudekiMpLobbyUiBackground();
    if (InterlockedCompareExchange(&stopping, 0, 0)) {
        if ((!native_thread || exact_thread()) && !hidden_count && SudekiMpTitleViewRelease())
            InterlockedExchange(&resources_released, 1);
        goto done;
    }
    if (!frame_composed || !render_state_ready || !InterlockedCompareExchange(&admission, 0, 0) ||
        !presentation_exact() || scene != current.scene || !modal_clear()) {
        displayed = FALSE;
        goto done;
    }
    frame_composed = FALSE;
    DWORD now = GetTickCount();
    SudekiMpTitleButtonState state = view_state;
    double seconds = (DWORD)(now - focus_started) / 1000.0;
    float opacity = 1;
    if (native_transition || page_confirm) {
        state = SUDEKIMP_TITLE_BUTTON_CONFIRM;
        seconds = (DWORD)(now - (native_transition ? transition_started : page_started)) / 1000.0;
        if (native_transition && seconds > .417)
            opacity = fmaxf(0, 1.f - (float)(seconds - .417) / .3f);
    }
    displayed = SudekiMpTitleViewDraw(*(void **)(game_base + D3D_DEVICE), row_count(),
        selected, enabled_rows(), multiplayer_page ? choices : current.label_ids,
        state, seconds, opacity, &game_window, multiplayer_page ? &SudekiMpLobbyUiView()->text : NULL);
    /* Match the title's confirmation beat, then fade the whole view into a
     * separate panel. Panel fields/actions have no title-button bounce. */
    float fade=0;
    if (page_confirm && !multiplayer_page)
        fade=fminf(1,fmaxf(0,((DWORD)(now-page_started)-420.f)/300.f));
    else if (panel_leaving) fade=fminf(1,(DWORD)(now-page_started)/300.f);
    else if (fade_in_active) fade=fmaxf(0,1-(DWORD)(now-fade_in_started)/300.f);
    if (displayed && fade>0)
        displayed=SudekiMpTitleViewFade(*(void **)(game_base+D3D_DEVICE),fade);
    if (!displayed) {
        draw_failed_at = now;
        if (!draw_failure_logged) {
            draw_failure_logged = TRUE;
            SudekiMpLogWrite("title_multiplayer event=draw_unavailable input_admitted=0\r\n");
        }
    } else {
        if (!root_owned)
            SudekiMpLogFormat("title_multiplayer event=composed generation=%u rows=%u renderer=shared native_rows_hidden=5 root_delay_ms=%lu\r\n",
                generation, row_count(), (unsigned long)(now - focus_started));
        draw_failure_logged = FALSE;
        root_owned = TRUE;
    }
done:
    InterlockedDecrement(&callbacks);
}

static BOOL retained(void) {
    HMODULE module;
    (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCSTR)&admission, &module);
    SetLastError(ERROR_BUSY);
    return FALSE;
}

BOOL SudekiMpUninstallTitleMultiplayer(void) {
    BOOL okay = TRUE;
    DWORD error = ERROR_SUCCESS;
    InterlockedExchange(&admission, 0);
    InterlockedExchange(&stopping, 1);
    if (InterlockedCompareExchange(&callbacks, 0, 0)) return retained();
    if (!remove_mouse_hook()) return retained();
    if (hidden_count && (!exact_thread() || !restore_rows())) return retained();
    if (!InterlockedCompareExchange(&resources_released, 0, 0)) {
        if (!exact_thread() || !SudekiMpTitleViewRelease()) return retained();
        InterlockedExchange(&resources_released, 1);
    }
#define RESTORE(call) do { if (!(call)) { if (okay) error = GetLastError(); okay = FALSE; } } while (0)
    RESTORE(SudekiMpRestoreRelativeCallHook(&render_hook));
    RESTORE(SudekiMpRestoreRelativeCallHook(&flush_hook));
    RESTORE(SudekiMpRestorePointerHook(&update_hook));
    RESTORE(SudekiMpRestorePointerHook(&action_hook));
#undef RESTORE
    if (!okay) { (void)retained(); SetLastError(error); return FALSE; }
    if (InterlockedCompareExchange(&callbacks, 0, 0)) return retained();
    if (!SudekiMpLobbyUiDestroy()) return retained();
    /* Native callbacks/base remain immutable for a caller already holding a
     * function pointer. No trampoline or borrowed native resource is freed. */
    clear_page();
    preparation_owner = NULL;
    return TRUE;
}

BOOL SudekiMpTitleMultiplayerQueueSavedLoadProbe(const SudekiMpSaveFingerprint *f) {
    if(!f || f->folder_slot>9999u || saved_load_probe || native_thread ||
        !InterlockedCompareExchange(&admission,0,0) || SudekiMpLobbyGameplayActive()) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    unsigned fish=0,bunny=0;
    for(unsigned i=0;i<32u;++i) { fish|=f->fish_sha256[i]; bunny|=f->bunny_sha256[i]; }
    if(!fish || !bunny) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    saved_load_fingerprint=*f;
    saved_load_probe=saved_load_probe_pending=TRUE;
    return TRUE;
}
BOOL SudekiMpInstallTitleMultiplayer(HMODULE module) {
    static const uint8_t action_entry[] = {0x8b,0x44,0x24,0x04};
    static const uint8_t update_entry[] = {0x56,0x8b,0xf1,0x80,0xbe,0x40,0x18,0x00,0x00,0x00};
    static const uint8_t render_entry[] = {0x53,0x56,0x8b,0xf0,0x80,0xbe,0x88,0x00,0x00,0x00,0x00};
    uint8_t *base = (uint8_t *)module;
    if (!module || !SudekiMpCheckLoadedExecutable(module) ||
        action_hook.installed || update_hook.installed || flush_hook.installed || render_hook.installed || hidden_count || mouse_hook ||
        InterlockedCompareExchange(&callbacks, 0, 0) ||
        !InterlockedCompareExchange(&resources_released, 0, 0)) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if (memcmp(base + TITLE_ACTION, action_entry, sizeof(action_entry)) ||
        memcmp(base + TITLE_UPDATE, update_entry, sizeof(update_entry)) ||
        memcmp(base + UI_RENDER, render_entry, sizeof(render_entry))) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    game_base = base;
    original_action = (TitleAction)(base + TITLE_ACTION);
    original_update = (TitleUpdate)(base + TITLE_UPDATE);
    original_flush = (UiFlush)(base + UI_FLUSH);
    original_render = base + UI_RENDER;
    native_thread = last_update = 0;
    generation = 0;
    preparation_owner = NULL;
    preparation_seen = 0;
    preparation_logged = FALSE;
    clear_page();
    InterlockedExchange(&stopping, 0);
    InterlockedExchange(&admission, 0);
    if (!SudekiMpInstallPointerHook(&action_hook, (void **)(base + TITLE_VTABLE + 0x2c),
            (void *)original_action, (void *)title_action) ||
        !SudekiMpInstallPointerHook(&update_hook, (void **)(base + TITLE_VTABLE + 8),
            (void *)original_update, (void *)title_update) ||
        !SudekiMpInstallRelativeCallHook(&flush_hook, base + UI_FLUSH_CALL,
            (void *)original_flush, (void *)title_flush) ||
        !SudekiMpInstallRelativeCallHook(&render_hook, base + UI_RENDER_CALL,
            original_render, (void *)title_render_entry)) {
        DWORD error = GetLastError();
        if (!SudekiMpUninstallTitleMultiplayer()) return FALSE;
        SetLastError(error); return FALSE;
    }
    InterlockedExchange(&admission, 1);
    SudekiMpLogWrite("title_multiplayer event=installed native_count_unchanged=1 lobby_enabled=1 gameplay_enabled=0\r\n");
    return TRUE;
}
