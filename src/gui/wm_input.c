/*
 * wm_input.c -- WuBuOS DosGui WM: input dispatch (key + mouse)
 *
 * Self-contained concern split out of wm.c (the WM facade):
 *   - wm_handle_key(): Alt+Tab cycling, Win-key hotkeys, theme
 *     cycling, maximize, virtual-desktop switching, window dispatch.
 *   - wm_handle_mouse(): taskbar / systray / notification-center hit
 *     testing, window chrome (close/max/min), title-bar drag, client-area
 *     routing, drag + icon drag with GAAD/icon-grid snapping.
 *
 * Depends only on the shared WM state (wm_internal.h) and the public
 * APIs of the sub-systems it routes to (start menu, notif center, icons,
 * taskbar, holyc term). No window lifecycle, no rendering.
 */

#include "wm_internal.h"
#include "wubu_tokens.h"
#include "../kernel/input.h"   /* MOD_SHIFT/CTRL/ALT/WIN */
#include "shell/wubu_osd.h"
#include "shell/wubu_screensaver.h"
#include "shell/wubu_lock_screen.h"

/* Windows double-click interval (SPI_GETDOUBLE CLICKTIME default, 500 ms). Windows
 * SELECTS on the first click and OPENS on the second; this shell used to launch on the
 * first, so a single stray click started a program and there was no way to select an icon
 * for rename/delete at all. */
#define ICON_DBLCLICK_MS 500

/* The double-click pair: which icon, and when it was last pressed. */
static int      s_last_icon    = -1;
static unsigned s_last_icon_ms = 0;

#if defined(__linux__) || defined(_POSIX_C_SOURCE) || defined(WUBU_HOSTED)
#include <time.h>
static unsigned wm_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (unsigned)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000);
}
#else
/* Bare metal: the WM tick is the only clock the input path owns. */
static unsigned wm_now_ms(void) { return (unsigned)g_dwm.ticks * 16u; }
#endif
#include "shell/win11_shell.h"
#include "shell/window_chrome.h"
#include "shell/shell_session.h"
#include "shell/run_dialog.h"
#include "shell/power_menu.h"
#include "../kernel/wubu_gesture.h"
#include "shell/startmenu.h"

/* The shell's launchers are optional cross-layer calls: unit tests link the WM
 * without apps.c, and a shell without them must not fail to link. A test can
 * provide a strong definition to observe the dispatch (wm_test does). */
extern DosGuiWindow *launch_file_manager(void) __attribute__((weak));
extern DosGuiWindow *control_launch(void) __attribute__((weak));
#include "shell/taskbar_tasks.h"

/* The app registry lives in src/apps; a test or minimal build may not link it.
 * Weak, so a pinned button is inert there rather than a link error. */
__attribute__((weak)) void launch_app(const char *name);
#include "wm.h"
#include "buddy_sprite.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

/* Chicago/Mac edge + corner resize: classify which border(s) the point is
 * over for the given window. Returns a bitmask: 1=left 2=right 4=top 8=bottom
 * (combinations give the 8 resize handles). Returns 0 if not on a border. */
static int hit_test_edge(DosGuiWindow *w, int x, int y) {
    int bw = border_width();
    int corner = bw * 2;            /* diagonal grab zones */
    /* The main edge grab zone must be at least 2px so a cursor positioned
     * 2px inside the edge (a common grab in synthetic + real dragging)
     * still classifies as a resize, not a title-bar / client drag. */
    int grab = corner > 2 ? corner : 2;
    int edge = 0;
    bool on_top = (y >= w->y && y < w->y + grab) || (y >= w->y && y < w->y + corner && x >= w->x && x < w->x + corner);
    bool on_bot = (y >= w->y + w->h - grab && y < w->y + w->h) ||
                  (y >= w->y + w->h - corner && y < w->y + w->h && x >= w->x + w->w - corner && x < w->x + w->w);
    bool on_left  = (x >= w->x && x < w->x + grab) ||
                    (x >= w->x && x < w->x + corner && y >= w->y && y < w->y + corner);
    bool on_right = (x >= w->x + w->w - grab && x < w->x + w->w) ||
                    (x >= w->x + w->w - corner && x < w->x + w->w && y >= w->y + w->h - corner && y < w->y + w->h);
    if (on_left)  edge |= 1;
    if (on_right) edge |= 2;
    if (on_top)   edge |= 4;
    if (on_bot)   edge |= 8;
    return edge;
}


/* -- Touchpad gestures --------------------------------------------- */
/* Multi-touch gestures map onto the same actions as the keyboard shortcuts,
 * so a laptop user gets the desktop without memorising hotkeys.  The gesture
 * engine is reached weakly: headless WM targets link no input driver raft. */
void wm_gesture_dispatch(void) {
    WubuGesture g;
    while (wubu_gesture_poll(&g)) {
        switch (g) {
            case WUBU_GESTURE_SWIPE_UP:
                wm_overview_toggle();
                break;
            case WUBU_GESTURE_SWIPE_LEFT:
                if (wm_get_current_desktop() > 0)
                    wm_set_current_desktop(
                        wm_get_current_desktop() - 1);
                break;
            case WUBU_GESTURE_SWIPE_RIGHT:
                if (wm_get_current_desktop() + 1 < DOSGUI_MAX_DESKTOPS)
                    wm_add_desktop();
                wm_set_current_desktop(
                    wm_get_current_desktop() + 1);
                break;
            case WUBU_GESTURE_PINCH_IN:
                wm_overview_toggle();
                break;
            default:
                break;
        }
    }
}

void wm_gesture_poll(void) {
    extern __attribute__((weak)) int wubu_gesture_tick(WubuGesture *out);
    if (wubu_gesture_tick) {
        WubuGesture tmp;
        (void)wubu_gesture_tick(&tmp);   /* feed the device, if one is open */
    }
    wm_gesture_dispatch();
}

/* Provided by src/gui/shell/win11_snap.c in the full shell build. */
extern void win11_snap_toggle(void) __attribute__((weak));
extern bool win11_snap_is_open(void) __attribute__((weak));
extern bool win11_snap_key(uint32_t key) __attribute__((weak));

/* The pointer path carries no modifiers: taskbar_modern_handle(x,y,btn,kind) has no mods
 * parameter and WubuUiEvent has no mods field. Windows' shift-click-a-taskbar-button needs
 * them, so the last state the KEYBOARD delivered is kept here. A modifier press is a key
 * event, so this is current whenever a click follows one. */
static uint32_t g_last_mods;

uint32_t wm_last_mods(void) { return g_last_mods; }

/* Key RELEASES go here and nowhere else. The WM is press-driven (a key-up would fire every
 * toggle twice), but the Alt+Tab switcher must know when Alt is let go -- that release is the
 * commit. Routing ups to one surface keeps the press-driven model intact everywhere else. */
void wm_handle_key_up(uint32_t key, uint32_t mods) {
    extern __attribute__((weak)) bool alt_tab_key_up(uint32_t, uint32_t);
    if (alt_tab_key_up) (void)alt_tab_key_up(key, mods);
}

void wm_handle_key(uint32_t key, uint32_t mods) {
    /* The Open-With resolver owns the keyboard while it is up: arrows move, Enter opens, Space ticks
     * "always use this app", Esc cancels, and every other key is swallowed so a shortcut cannot fire
     * behind a modal card. Weak + guarded -- this file is in every WM test graph. */
    {
        extern __attribute__((weak)) bool add_to_library_is_open(void);
        extern __attribute__((weak)) bool add_to_library_key(uint32_t, int, int);
        extern __attribute__((weak)) void wubu_notify_simple(const char *, const char *, const char *, const char *, int, int);
        extern __attribute__((weak)) const char *add_to_library_status(void);
        {
            extern __attribute__((weak)) bool pin_confirm_is_open(void);
            extern __attribute__((weak)) bool pin_confirm_key(uint32_t, int, int);
            if (pin_confirm_is_open && pin_confirm_key && pin_confirm_is_open()) {
                (void)pin_confirm_key(key, vbe_width(), vbe_height());
                return;
            }
        }
        if (add_to_library_is_open && add_to_library_is_open()) {
            bool was_open = true;
            (void)was_open;
            if (add_to_library_key) (void)add_to_library_key(key, vbe_width(), vbe_height());
            if (add_to_library_is_open && !add_to_library_is_open() && wubu_notify_simple &&
                add_to_library_status)
                wubu_notify_simple("Libraries", "Updated", add_to_library_status(), NULL, 1, 2400);
            return;
        }
    }
    {
        extern __attribute__((weak)) bool open_with_is_open(void);
        extern __attribute__((weak)) const char *open_with_move(int);
        extern __attribute__((weak)) void open_with_toggle_remember(void);
        extern __attribute__((weak)) const char *open_with_accept(void);
        extern __attribute__((weak)) void open_with_close(void);
        extern __attribute__((weak)) int open_with_launch_last(void);
        if (open_with_is_open && open_with_is_open()) {
            if (key == 0xE048 || key == 0xE050) {          /* Up / Down */
                if (open_with_move) open_with_move(key == 0xE048 ? -1 : 1);
            } else if (key == ' ') {
                if (open_with_toggle_remember) open_with_toggle_remember();
            } else if (key == 0x1C || key == 0xE01C || key == '\r' || key == '\n') {
                if (open_with_accept && open_with_accept() && open_with_launch_last)
                    open_with_launch_last();
            } else if (key == 27) {
                if (open_with_close) open_with_close();
            }
            return;
        }
    }
    {
        extern __attribute__((weak)) bool taskbar_overflow_key(uint32_t);
        if (taskbar_overflow_key && taskbar_overflow_key(key)) return;
    }

    g_last_mods = mods;
    wubu_screensaver_activity();

    /* Win+L: lock / unlock. */
    if ((mods & MOD_WIN) && (key == 0x26 || key == 'l' || key == 'L')) {
        if (wubu_lock_screen_is_locked()) wubu_lock_screen_unlock();
        else                                wubu_lock_screen_lock();
        return;
    }

    /* While locked the lock screen consumes EVERY key -- no app sees input. */
    if (wubu_lock_screen_handle_key(key, mods)) return;

    /* The Start menu is modal: while it is open it owns the keyboard (typing
     * searches, Escape closes, Enter launches).  The Win key still toggles
     * it, so it is handled before the modal block. */
    if (key == 0xE05B || key == 0xE05C) {
        startmenu_toggle();
        return;
    }
    /* Ctrl+Esc: open Start. Microsoft's published shortcut list has it, and it is the way IN when the HOST
     * window manager owns the Super key -- measured: Cinnamon grabs Super globally, so the OS's own menu was
     * unreachable in a hosted session unless the user first toggled keyboard capture. Handled before the
     * modal block so it also CLOSES an open menu, like Windows. */
    if ((mods & MOD_CTRL) && key == 0x1B) {
        startmenu_toggle();
        return;
    }
    if (startmenu_is_open()) {
        startmenu_handle_key(key, mods);
        startmenu_handle_search_input(key, mods);
        return;
    }

    /* Win+R: run dialog (Windows 11 parity). */
    if ((mods & MOD_WIN) && (key == 0x13 || key == 'r' || key == 'R')) {
        run_dialog_open();
        return;
    }
    /* An open Print dialog takes keys first: it is modal. */
    {
        extern __attribute__((weak)) int print_dialog_is_open(void);
        extern __attribute__((weak)) void print_dialog_handle_key(uint32_t, uint32_t);
        if (print_dialog_is_open && print_dialog_is_open() && print_dialog_handle_key) {
            print_dialog_handle_key(key, mods);
            return;
        }
    }
    if (run_dialog_is_open()) {
        run_dialog_handle_key(key, mods);
        return;
    }

    /* Win+X: power-user menu (Windows 11 parity). */
    if ((mods & MOD_WIN) && (key == 0x2D || key == 'x' || key == 'X')) {
        power_menu_open();
        return;
    }
    if (power_menu_is_open()) {
        power_menu_handle_key(key, mods);
        return;
    }

    /* Alt+Space: the window system menu every Windows has. There was none. */
    if ((mods & MOD_ALT) && key == 0x39 && g_dwm.focused_id >= 0) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
        if (w->alive) window_show_system_menu(g_dwm.focused_id, w->x + 8, w->y + 4);
        return;
    }

    /* Desktop keyboard, the way Windows has it: with the desktop focused and an icon
     * selected, Enter opens it, F2 renames, Delete deletes. Before this, a selected icon
     * could only be acted on with the mouse. */
    if (g_dwm.focused_id < 0) {
        int sel = -1;
        for (int i = 0; i < g_dwm.icon_count; i++)
            if (g_dwm.icons[i].alive && g_dwm.icons[i].selected) { sel = i; break; }
        if (sel >= 0) {
            if (key == 0x1C) { wm_open_icon(sel); return; }          /* Enter */
            if (key == 0x3C) { wm_rename_selected_icon(); return; }  /* F2 */
            if (key == 0x53) { wm_delete_selected_icon(); return; }  /* Delete */
        }
    }

    /* Ctrl+Shift+O: the Open dialog. WuBuOS convenience (Win11 has no global binding); the
     * point is that the OS finally HAS a file picker for callers that need one. */
    if ((mods & MOD_CTRL) && (mods & MOD_SHIFT) && (key == 'o' || key == 'O')) {
        extern __attribute__((weak)) int file_picker_open(const char *, int,
            int (*)(const char *, void *), void *);
        extern __attribute__((weak)) void wubu_notify_simple(const char *, const char *,
            const char *, const char *, int, int);
        if (file_picker_open) {
            /* Also weak: the WM tests link this TU without the picker module. */
            extern __attribute__((weak)) int wubu_fp_report(const char *path, void *user);
            file_picker_open(getenv("HOME"), 0,
                             wubu_fp_report ? wubu_fp_report : NULL, NULL);
        }
        return;
    }

    if ((mods & MOD_WIN) && getenv("WUBU_KEY_PROBE"))
        fprintf(stderr, "[kp] key=0x%X '%c' mods=0x%X\n", key,
                (key >= 32 && key < 127) ? (char)key : '?', mods);
    /* Win+Shift+S: the CapturePicker (Win11's screenshot region tool). The selector, its capture
     * and its save path all existed; NOTHING bound or drew them, and every capture path began with
     * "if the Wayland shm buffer is absent, give up" -- so on this X11 desktop the feature did
     * nothing at all. The Win+Shift+S binding is the parity one; it only became receiveable once
     * the hosted presenter grabs the keyboard while it is the focused window, because the host WM
     * otherwise resolves Super combos before the OS is handed the event (measured: none arrived). */
    if (((mods & MOD_WIN) && (mods & MOD_SHIFT) && (key == 's' || key == 'S')) ||
        /* Ctrl+Shift+S stays as the shell's own convenience, the way Ctrl+Shift+O is. */
        ((mods & MOD_CTRL) && (mods & MOD_SHIFT) && (key == 's' || key == 'S'))) {
        extern __attribute__((weak)) void wubu_screenshot_handle_shift_printscr(void);
        if (wubu_screenshot_handle_shift_printscr) wubu_screenshot_handle_shift_printscr();
        return;
    }

    /* Ctrl+P: Print. Win11 binds it inside the app that owns a document; here the shell offers it
     * globally so a file selected in Explorer can be printed. The dialog asks the real spooler and
     * reports what it says. Weak: the WM tests link this TU without the dialog or the runtime. */
    if ((mods & MOD_CTRL) && (key == 'p' || key == 'P')) {
        extern __attribute__((weak)) void print_dialog_open(const char *path);
        extern __attribute__((weak)) const char *explorer_selected_path(void);
        if (print_dialog_open) {
            const char *sel = NULL;
            if (explorer_selected_path) sel = explorer_selected_path();
            print_dialog_open(sel);
        }
        return;
    }

    /* Ctrl+Shift+Esc: Task Manager -- a Windows norm that was simply missing. Weak so the
     * headless WM tests (no applet stack) still link. */
    if ((mods & MOD_CTRL) && (mods & MOD_SHIFT) && (key == 0x01 || key == 0x1B)) {
        extern __attribute__((weak)) DosGuiWindow *taskmgr_launch(void);
        if (taskmgr_launch) taskmgr_launch();
        return;
    }

    /* Ctrl+Alt+Up/Down: system volume.  Ctrl+Alt+Left/Right: brightness.
     * Both drive the OSD, so the change is visible. */
    if ((mods & MOD_CTRL) && (mods & MOD_ALT)) {
        if (key == 0xE048 || key == 0xE050 || key == 0xE04B || key == 0xE04D) {
            float dv = (key == 0xE048) ?  0.05f : (key == 0xE050) ? -0.05f : 0.0f;
            float db = (key == 0xE04D) ?  0.05f : (key == 0xE04B) ? -0.05f : 0.0f;
            if (dv != 0.0f) {
                wubu_osd_set_volume(wubu_osd_volume() + dv);
                /* music.c is not linked into every WM target (the WM tests are
                 * headless), so reach it weakly -- the same pattern the theme
                 * engine uses for its kernel node getter. */
                extern __attribute__((weak)) void music_apply_system_volume(float v);
                if (music_apply_system_volume)
                    music_apply_system_volume(wubu_osd_volume());
            }
            if (db != 0.0f) wubu_osd_set_brightness(wubu_osd_brightness() + db);
            wubu_osd_notify();
            return;
        }
    }

    /* Win+Ctrl+D: create a virtual desktop (Windows 11 parity). */
    if ((mods & MOD_WIN) && (mods & MOD_CTRL) && (key == 0x20 || key == 'd' || key == 'D')) {
        wm_add_desktop();
        return;
    }

    /* Win+E: File Explorer, Win+I: Control Panel, Win+Q: Start. These are the
     * three most-used Windows shell shortcuts and none of them existed: a user
     * coming from Windows presses them first and nothing happens. Each requires
     * the extra modifiers to be ABSENT so it cannot swallow a combo. */
    if ((mods & MOD_WIN) && !(mods & (MOD_CTRL | MOD_SHIFT)) &&
        (key == 'e' || key == 'E')) {
        if (launch_file_manager) launch_file_manager();
        return;
    }
    if ((mods & MOD_WIN) && !(mods & (MOD_CTRL | MOD_SHIFT)) &&
        (key == 'i' || key == 'I')) {
        if (control_launch) control_launch();
        return;
    }
    if ((mods & MOD_WIN) && !(mods & (MOD_CTRL | MOD_SHIFT)) &&
        (key == 'q' || key == 'Q' || key == 's' || key == 'S')) {
        startmenu_open();
        return;
    }

    /* While the snap-layout flyout is open it owns the keyboard: arrows move the
     * selection, 1-6 pick the layout, Enter applies. Windows 11 requires a mouse for
     * the REGION; answering it from the keyboard costs one pure function
     * (wubu_snap_neighbor) and makes the whole shortcut reachable without pointing. */
    if (win11_snap_is_open && win11_snap_key && win11_snap_is_open()) {
        if (win11_snap_key(key)) return;
    }

    /* Win+Left / Right / Up / Down: snap, maximize, restore -- the four window
     * shortcuts a Windows user reaches for before anything else. Arrows carry no
     * text, so they arrive as Set-1 scancodes (see input_scancode_to_key): Left
     * 0x4B, Right 0x4D, Up 0x48, Down 0x50.
     *
     * Win+Down on a maximized window restores it; on a floating one it minimizes,
     * which is exactly what Windows 11 does. Extra modifiers are required to be
     * ABSENT so these cannot swallow a combo. */
    if ((mods & MOD_WIN) && !(mods & (MOD_CTRL | MOD_SHIFT | MOD_ALT))) {
        DosGuiWindow *fw = wm_get_focused();
        if (fw) {
            if (key == 0x4B) { wm_snap_half(fw, -1); return; }
            if (key == 0x4D) { wm_snap_half(fw, +1); return; }
            if (key == 0x48) { wm_maximize(fw); return; }
            if (key == 0x50) {
                if (wm_is_maximized(fw)) wm_restore(fw);
                else                      wm_minimize(fw);
                return;
            }
            /* Win+Z: the snap LAYOUTS flyout, which until now was reachable only
             * from the maximize button's hover affordance. */
            if (key == 'z' || key == 'Z' || key == 0x2C) {
                /* Weak + NULL check: the snap flyout lives in the shell, and a
                 * GUI-only link (the applet and window tests) does not carry it.
                 * Same convention as startmenu_db's applet accessors -- strong
                 * in the shell, absent in a narrow link, never a link error. */
                if (win11_snap_toggle) win11_snap_toggle();
                return;
            }
        }
    }

    /* Win+1..9: act on that taskbar button (Windows). Windows users press these dozens of
     * times a day; nothing happened here. Keys arrive as ASCII digits. */
    if ((mods & MOD_WIN) && !(mods & (MOD_CTRL | MOD_ALT)) &&
        key >= '1' && key <= '9') {
        if (taskbar_slot_launch((int)(key - '1'))) return;
    }

    /* Win+P: the projection selector (Windows). Weak: a WM-only link has no shell. */
    {
        extern __attribute__((weak)) void projection_flyout_toggle(void);
        extern __attribute__((weak)) bool projection_flyout_is_open(void);
        extern __attribute__((weak)) bool projection_flyout_key(uint32_t, uint32_t);
        if (projection_flyout_is_open && projection_flyout_is_open()) {
            if (projection_flyout_key && projection_flyout_key(key, mods)) return;
        }
        if ((mods & MOD_WIN) && !(mods & (MOD_CTRL | MOD_SHIFT | MOD_ALT)) &&
            (key == 'p' || key == 'P' || key == 0x19)) {
            if (projection_flyout_toggle) projection_flyout_toggle();
            return;
        }
    }

    /* Win+A: quick settings panel (Windows 11 parity). */
    if ((mods & MOD_WIN) && (key == 0x1E || key == 'a' || key == 'A')) {
        quick_settings_toggle();
        return;
    }

    /* Win+N: notification center (Windows 11 parity). Before this, the
     * center was reachable ONLY by the classic bar's toggle region —
     * unreachable in the modern theme. */
    if ((mods & MOD_WIN) && (key == 0x31 || key == 'n' || key == 'N')) {
        notif_center_toggle();
        return;
    }

    /* Win+Tab: desktop overview grid. */
    if ((mods & MOD_WIN) && key == 0x09) {
        wm_overview_toggle();
        return;
    }

    /* Win11 shell hotkeys (Win+Z snap flyout, Win+W widgets).  Weak: a build
     * without the shell simply has no hotkeys. */
    if (win11_shell_key && win11_shell_key(key, mods)) return;

    /* Alt+Tab: the switcher (previews while Alt is held, commit on release) when the shell
     * provides it; otherwise the old immediate cycle. Weak, so a WM-only link keeps cycling. */
    {
        extern __attribute__((weak)) bool alt_tab_key_down(uint32_t, uint32_t);
        if (alt_tab_key_down && alt_tab_key_down(key, mods)) return;
    }

    /* Alt+Tab: cycle through windows */
    bool alt_held = (mods & MOD_ALT) != 0;
    if (alt_held && key == 0x09 && g_dwm.nz > 1) {
        /* Find current focused index in zorder */
        int cur_idx = 0;
        for (int j = 0; j < g_dwm.nz; j++) {
            if (g_dwm.zorder[j] == g_dwm.focused_id) { cur_idx = j; break; }
        }
        /* Focus next window (wrap around) */
        int next_idx = (cur_idx + 1) % g_dwm.nz;
        int next_id = g_dwm.zorder[next_idx];
        if (next_id >= 0 && next_id < DOSGUI_MAX_WINDOWS && g_dwm.windows[next_id].alive) {
            raise_win(next_id);
            g_dwm.focused_id = next_id;
        }
        return;
    }

    /* Win+H: spawn HolyC terminal */
    if ((mods & MOD_ALT) && (key == 0x48 || key == 'h' || key == 'H')) {
        wm_spawn_holyc_term(100, 100, 700, 500);
        return;
    }

    /* Alt+F4: close the focused window (Windows parity).  The old bare
     * `key == 111` / `0x57` / `0x3F` hotkeys had to go: the input layer now
     * delivers ASCII, so 'o' closed the window, 'W' maximized it and '?'
     * cycled the theme in the middle of typing. */
    if ((mods & MOD_ALT) && key == 0x3E && g_dwm.focused_id >= 0) {
        wm_request_close(g_dwm.focused_id);   /* Alt+F4 animates */
        return;
    }
    if ((mods & MOD_CTRL) && key == 0x14) {
        wubu_theme_cycle();
        fprintf(stderr, "Theme cycled to: %s\n", wubu_theme_name(wubu_theme_current()));
        return;
    }
    if ((mods & MOD_CTRL) && (mods & MOD_SHIFT)) {
        if (key == 0xE04B) {
            g_dwm.current_desktop = (g_dwm.current_desktop - 1 + g_dwm.desktop_count) % g_dwm.desktop_count;
        } else if (key == 0xE04D) {
            g_dwm.current_desktop = (g_dwm.current_desktop + 1) % g_dwm.desktop_count;
        }
        return;
    }

    /* Win+D: show the desktop (minimize all, or restore when all are down).
     * Ctrl must be absent, or Win+Ctrl+D (create desktop) would be swallowed. */
    if ((mods & MOD_WIN) && !(mods & MOD_CTRL) &&
        (key == 0x20 || key == 'd' || key == 'D')) {
        wm_show_desktop();
        return;
    }

    /* Win+Shift+Left/Right: Move focused window to adjacent desktop.
     * This must be decided BEFORE the plain Win+Arrow snap below: that block
     * matches any Win+Arrow, so it used to swallow Win+Shift+Arrow and the
     * "move window to the other desktop" shortcut could never fire. */
    if ((mods & (MOD_WIN | MOD_SHIFT)) == (MOD_WIN | MOD_SHIFT)) {
        if (key == 0xE04B) {  /* Left arrow */
            wm_move_focused_window(-1);
            return;
        } else if (key == 0xE04D) {  /* Right arrow */
            wm_move_focused_window(1);
            return;
        }
    }

    /* Win+Arrow: Snap Layouts (keyboard).  Left/Right = half screen,
     * Up = maximize, Down = right half. SHIFT must be absent (it means
     * "move to desktop", handled above). */
    if ((mods & MOD_WIN) && !(mods & MOD_SHIFT) && g_dwm.focused_id >= 0) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
        if (w->alive) {
            if (key == 0xE04B) { snap_window_to_zone(w, 0); return; }
            if (key == 0xE04D) { snap_window_to_zone(w, 1); return; }
            if (key == 0xE048) { wm_maximize(w); return; }
            if (key == 0xE050) { snap_window_to_zone(w, 1); return; }
        }
    }

    /* LAST: dispatch ordinary keys to the focused window.
     *
     * This used to sit ABOVE Win+D, Win+Shift+arrows and the 0xE0xx arrow path, and it
     * returns unconditionally when a window has focus -- so those three shell combos could
     * never fire in the one situation they exist for: an app window on screen. Windows
     * gives the shell first refusal on its own reserved combos and only then lets the app
     * see the key; this is that order.
     *
     * Anything added below this line is dead code for a focused window. New shell
     * shortcuts belong ABOVE it. */
    if (g_dwm.focused_id >= 0) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
        if (w->alive && w->on_key) {
            w->on_key(w, key, mods);
        }
    }
}

void wm_handle_mouse(int x, int y, int btn, int kind) {
    wubu_screensaver_activity();
    g_dwm.mouse_x = x;
    g_dwm.mouse_y = y;

    /* The CapturePicker owns the pointer while it is up: press starts the selection, motion extends
     * it, release completes it. This must be the FIRST consumer -- placed further down, a press
     * landed on whichever window was under the pointer and started dragging THAT, so the selection
     * never began and no capture was ever saved. Press, motion and release are three different
     * calls: passing down=true for the release is what made the rectangle unfinishable. */
    {
        extern __attribute__((weak)) int wubu_screenshot_region_active(void);
        extern __attribute__((weak)) void wubu_screenshot_update_region_selector(int, int, bool);
        extern __attribute__((weak)) void wubu_screenshot_region_move(int, int);
        if (wubu_screenshot_region_active && wubu_screenshot_region_active()) {
            if (kind == 1) wubu_screenshot_update_region_selector(x, y, true);
            else if (kind == 2) wubu_screenshot_update_region_selector(x, y, false);
            else if (wubu_screenshot_region_move) wubu_screenshot_region_move(x, y);
            return;
        }
    }

    /* Snap assist claims the whole pointer while it is open: a card click picks a window,
     * any other click dismisses the offer (Windows behaviour). Weak: a WM-only link has no
     * shell and therefore no assist. */
    if (btn && kind == 1) {
        extern __attribute__((weak)) bool win11_snap_assist_click(int, int);
        extern __attribute__((weak)) bool win11_snap_assist_active(void);
        if (win11_snap_assist_active && win11_snap_assist_active() &&
            win11_snap_assist_click && win11_snap_assist_click(x, y))
            return;
    }

    if (btn && kind == 1) {
        extern __attribute__((weak)) bool taskbar_overflow_is_open(void);
        extern __attribute__((weak)) bool taskbar_overflow_click(int, int);
        if (taskbar_overflow_is_open && taskbar_overflow_is_open() &&
            taskbar_overflow_click && taskbar_overflow_click(x, y)) return;
    }

    if (btn && kind == 1) {
        extern __attribute__((weak)) bool alt_tab_is_open(void);
        extern __attribute__((weak)) bool alt_tab_click(int, int);
        if (alt_tab_is_open && alt_tab_is_open() && alt_tab_click && alt_tab_click(x, y))
            return;
    }

    /* The projection flyout owns the pointer while it is open (click a row or dismiss). */
    if (btn && kind == 1) {
        extern __attribute__((weak)) bool projection_flyout_is_open(void);
        extern __attribute__((weak)) bool projection_flyout_click(int, int);
        if (projection_flyout_is_open && projection_flyout_is_open() &&
            projection_flyout_click && projection_flyout_click(x, y))
            return;
    }

    /* While locked the pointer is swallowed too. */
    if (wubu_lock_screen_is_locked()) return;

    /* The Open-With resolver is modal: it owns every press while it is up, and the press either picks,
     * cancels or is swallowed -- a click that leaks through would open the file behind the card.
     * Weak + guarded, like every shell surface called from this file. */
    {
        extern __attribute__((weak)) bool open_with_is_open(void);
        extern __attribute__((weak)) int open_with_click(int, int, int, int);
        extern __attribute__((weak)) int open_with_launch_last(void);
        {
            extern __attribute__((weak)) bool pin_confirm_is_open(void);
            extern __attribute__((weak)) int pin_confirm_click(int, int, int, int);
            if (pin_confirm_is_open && pin_confirm_click && pin_confirm_is_open()) {
                (void)pin_confirm_click(x, y, vbe_width(), vbe_height());
                return;
            }
        }
        /* The library card is modal too. Checked before Open-With because a library card is opened
         * from Explorer, which can never have an Open-With card up at the same time. */
        {
            extern __attribute__((weak)) bool add_to_library_is_open(void);
            extern __attribute__((weak)) int add_to_library_click(int, int, int, int);
            extern __attribute__((weak)) void wubu_notify_simple(const char *, const char *, const char *, const char *, int, int);
            extern __attribute__((weak)) const char *add_to_library_status(void);
            if (add_to_library_is_open && add_to_library_click && add_to_library_is_open()) {
                int rr = add_to_library_click(x, y, vbe_width(), vbe_height());
                if (rr != 0 && wubu_notify_simple && add_to_library_status)
                    wubu_notify_simple("Libraries", rr == 1 ? "Updated" : "Cancelled",
                                       add_to_library_status(), NULL, 1, 2400);
                return;
            }
        }
        if (open_with_is_open && open_with_click && open_with_is_open()) {
            int r = open_with_click(x, y, vbe_width(), vbe_height());
            if (r == 1 && open_with_launch_last) open_with_launch_last();
            return;
        }
    }

    /* A toast card is a control, not decoration: clicking one opens the centre, the way the reference
     * desktop takes you to the notification. Checked BEFORE the shell layers and window dispatch --
     * the cards float above everything, and win11_shell_click() owns presses in that corner. */
    /* The same dimensions the RENDERER used: wm_render passes the live framebuffer, so a hit-test
     * against g_dwm's cached screen size can be a whole resize out of date. */
    {
        /* Weak like the toast renderer: this input file is compiled into test graphs that do not
         * link the toast module, and a missing card must not be a link error. */
        extern __attribute__((weak)) int wm_toast_at(int, int, int, int);
        if (kind == 1 && wm_toast_at && wm_toast_at(x, y, vbe_width(), vbe_height()) >= 0) {
            notif_center_toggle();
            return;
        }
    }

    /* Win11 shell layers own the press while the snap flyout or widgets
     * board is open (weak: absent in builds without the shell). */
    if (kind == 1 && win11_shell_click && win11_shell_click(x, y)) return;

    /* The run dialog is modal: a press outside it dismisses it, inside is
     * swallowed (there is no text cursor to place yet). */
    if (kind == 1 && run_dialog_is_open()) {
        run_dialog_handle_click(x, y);
        return;
    }

    /* The Print dialog is modal: a press inside is its own, outside dismisses it. */
    if (kind == 1) {
        extern __attribute__((weak)) int print_dialog_is_open(void);
        extern __attribute__((weak)) int print_dialog_handle_click(int, int, int, int);
        if (print_dialog_is_open && print_dialog_is_open() && print_dialog_handle_click) {
            print_dialog_handle_click(x, y, g_dwm.screen_w, g_dwm.screen_h);
            return;
        }
    }

    /* The power-user menu is modal too. */
    if (kind == 1 && power_menu_is_open()) {
        power_menu_handle_click(x, y);
        return;
    }

    /* While the quick settings panel is open it owns the pointer. */
    if (quick_settings_is_open() && kind == 1) {
        quick_settings_handle_click(x, y, g_dwm.screen_w, g_dwm.screen_h);
        return;
    }

    /* While the overview is open it owns the pointer: a press either picks a
     * desktop tile or dismisses the grid. */
    if (wm_overview_is_open() && kind == 1) {
        wm_overview_handle_click(x, y, g_dwm.screen_w, g_dwm.screen_h);
        return;
    }

    int task_h = taskbar_height_dynamic();
    int tbh = title_bar_height();
    border_width();  // ensure theme is loaded

    /* Start menu: when open, own the click — route PRESS events to the
     * menu's hit-test before anything else grabs it.  A press (kind==1)
     * on a menu item fires its action; a press outside the menu closes it.
     * Releases (kind==2) are consumed here to prevent a second toggle
     * from the Start-button release landing on a now-closed menu. */
    if (startmenu_is_open() && kind == 1) {
        startmenu_handle_click(x, y);
        return;
    }
    if (startmenu_is_open() && kind == 2) {
        /* Swallow release while menu is open so the Start button's own
         * release handler in the fall-through below can't re-toggle. */
        return;
    }

    /* Clock popup (Win98 parity): a press OUTSIDE the open popup closes it
     * (and the press still processes normally). */
    if (g_dwm.clock_menu_open && kind == 1) {
        int px, py, pw, ph;
        clock_menu_rect(g_dwm.screen_w, g_dwm.screen_h,
                               &px, &py, &pw, &ph);
        if (x < px || x >= px + pw || y < py || y >= py + ph)
            clock_menu_close();
    }

    /* Screen-edge slime seam: a press on a blob grabs it before anything
     * else, so it can be dragged along the edge or clicked to store/release. */
    if (wm_slime_handle_pointer(x, y, btn, kind)) return;

    /* Window-rim islands: a press on an island grabs it before any window or
     * taskbar handling, so it can be dragged around the focused window's rim.
     * Inert unless an island is actually under the pointer. */
    if (wm_rim_handle_pointer(x, y, btn, kind)) return;

    /* Taskbar / Start button: must be checked BEFORE the Bonzi Buddy hit-test,
     * because the buddy sits in the lower-left corner and its bounding box
     * overlaps the taskbar region.  A click on the Start button or taskbar
     * buttons must not be swallowed by the mascot. */
    /* Modern (Win11-class) floating bar has its own geometry and hit-test;
     * the classic bar is skipped entirely for that theme. */
    bool modern_bar = (wubu_theme_current() == THEME_DARK);
    if (modern_bar && taskbar_modern_handle(x, y, btn, kind)) return;

    if (!modern_bar && y >= taskbar_top()) {
        int by = taskbar_top() + (task_h - 24) / 2;
        int start_w = theme()->Luna_start_button ? 54 : 60;
        /* Start button hit region must match the DRAWN width
         * (wm_taskbar.c: Win98 draws 60px, Luna draws start_w+20).
         * The old +20 made the hit zone extend to x<84 while the Win98
         * button is only 60px wide — the phantom 20px swallowed the first
         * taskbar button's left 12px (task buttons start at x=72), so
         * clicking a minimized window's restore button's left edge opened
         * the Start menu instead of restoring the window. */
        int start_hit_w = theme()->Luna_start_button ? start_w + 20 : start_w;

        if (x >= 4 && x < 4 + start_hit_w && y >= by && y < by + 24) {
            /* Act on PRESS only — release at the same spot would toggle the
             * menu closed again (same double-fire as the taskbar buttons). */
            if (kind == 1) startmenu_toggle();
            return;
        }

        int bx = theme()->Luna_start_button ? 82 : 72;
        for (int j = 0; j < g_dwm.nz; j++) {
            DosGuiWindow *w = &g_dwm.windows[g_dwm.zorder[j]];
            if (!w->alive) continue;   /* MINIMIZED windows must be hit-testable:
                                          their taskbar button restores them */
            /* Button width MUST match wm_taskbar.c render exactly
             * (tw + 24, min 120, max 200) or the right part of a drawn
             * button is unclickable. */
            int tw = vbe_text_width(w->title, 1);
            int bw = tw + 24;
            if (bw < 120) bw = 120;
            if (bw > 200) bw = 200;
            if (x >= bx && x < bx + bw && y >= by && y < by + 22) {
                /* Act on PRESS only (kind==1). The same (x,y) arrives again
                 * as kind==2 (release); acting there too makes one click
                 * restore-then-immediately-reminimize (double-fire toggle). */
                if (kind != 1) return;
                if (w->flags & DOSGUI_WIN_MINIMIZED) {
                    w->flags &= ~DOSGUI_WIN_MINIMIZED;
                    wm_set_focus(w);   /* restore also refocuses */
                } else if (g_dwm.focused_id == g_dwm.zorder[j]) {
                    w->flags |= DOSGUI_WIN_MINIMIZED;
                } else {
                    wm_set_focus(w);
                }
                return;
            }
            bx += bw + 3;   /* MUST match render gap (taskbar.c) */
            if (bx > g_dwm.screen_w - 160) break;
        }

        /* Pinned tasks with no running window: clicking launches the app.
         * Geometry MUST match wm_taskbar.c exactly. */
        for (int i = 0; i < wubu_taskbar_count(); i++) {
            const WubuTask *t = wubu_taskbar_get(i);
            if (!t || !t->pinned || t->win_id >= 0) continue;
            if (bx + 120 > g_dwm.screen_w - 160) break;
            if (x >= bx && x < bx + 120 && y >= by && y < by + 22) {
                if (kind != 1) return;
                if (launch_app)
                    launch_app(t->exec[0] ? t->exec : t->title);
                return;
            }
            bx += 123;
        }

        /* Desktop pager: derive from the PUBLISHED tray well, exactly as
         * the renderer does (desk_x = tray_x - PAGER_RESERVE). The old
         * screen_w-150 guess was offset by the whole clock+tray reserve,
         * so pager clicks landed on nothing. */
        int desk_x = g_dwm.tray_well_x - WUBU_TASKBAR_CLASSIC_PAGER_RESERVE;
        int desk_y = taskbar_top() + (task_h - WUBU_TASKBAR_CLASSIC_PAGER_H) / 2;
        for (int d = 0; d < g_dwm.desktop_count; d++) {
            int dx = desk_x + d * WUBU_TASKBAR_CLASSIC_PAGER_PITCH;
            if (x >= dx && x < dx + WUBU_TASKBAR_CLASSIC_PAGER_W &&
                y >= desk_y && y < desk_y + WUBU_TASKBAR_CLASSIC_PAGER_H) {
                g_dwm.current_desktop = d;
                return;
            }
        }

        /* System tray: hit-test the well the renderer publishes — one
         * layout owner (systray_icon_at). The old right-aligned walk
         * re-derived positions from the clock and missed the modern
         * centred bar entirely. */
        {
            int ti = systray_icon_at(x, y);
            if (ti >= 0) {
                if (kind == 1 && btn == 2 && g_dwm.systray_icons[ti].on_right_click)
                    g_dwm.systray_icons[ti].on_right_click();
                else if (kind == 1 && g_dwm.systray_icons[ti].on_click)
                    g_dwm.systray_icons[ti].on_click();
                return;
            }
        }

        /* Notification center toggle: immediately left of the tray well. */
        if (g_dwm.tray_well_w > 0 &&
            x >= g_dwm.tray_well_x - WUBU_NOTIF_TOGGLE_W && x < g_dwm.tray_well_x &&
            y >= by && y < by + WUBU_NOTIF_TOGGLE_H) {
            notif_center_toggle();
            return;
        }

        /* Taskbar clock well (Win98 parity): clicking it opens/closes the
         * clock + calendar popup. */
        if (x >= g_dwm.clock_well_x && x < g_dwm.clock_well_x + g_dwm.clock_well_w &&
            y >= g_dwm.clock_well_y && y < g_dwm.clock_well_y + g_dwm.clock_well_h) {
            if (kind == 1) clock_menu_toggle();
            return;
        }

        return;
    }

    /* Accessibility cluster: give it first crack at the event when enabled
     * and a window is focused.  Checked AFTER the taskbar block above: a
     * maximized window's orbs sit at the bottom corners and would otherwise
     * swallow taskbar clicks (the rotate orb overlapped the clock well at
     * 999,754 and the clock popup could never open).  The taskbar is always
     * the topmost strip, so it keeps priority. */
    /* Desktop mascot (WuBu Buddy): chrome-less element above windows.
     * Checked AFTER the taskbar region so Start button clicks aren't
     * swallowed by the mascot's lower-left bounding box. */
    if (wubu_bonzi_is_enabled() && wubu_bonzi_mouse(x, y, btn, kind))
        return;

    if (kind == 1) {
        if (y >= taskbar_top()) {
            return;
        }

        for (int j = g_dwm.nz - 1; j >= 0; j--) {
            int idx = g_dwm.zorder[j];
            DosGuiWindow *w = &g_dwm.windows[idx];
            if (!w->alive || (w->flags & DOSGUI_WIN_MINIMIZED)) continue;
            {
                ChromeButtonRect bmin, bmax, bclose;
                if (chrome_button_rects(w, &bmin, &bmax, &bclose)) {
                    if (x >= bclose.x && x < bclose.x + bclose.w &&
                        y >= bclose.y && y < bclose.y + bclose.h) {
                        wm_request_close(idx);  /* X button animates */
                        return;
                    }
                    if (x >= bmax.x && x < bmax.x + bmax.w &&
                        y >= bmax.y && y < bmax.y + bmax.h) {
                        if (w->flags & DOSGUI_WIN_MAXIMIZED) {
                            w->x = w->min_x; w->y = w->min_y;
                            w->w = w->min_w; w->h = w->min_h;
                            w->flags &= ~DOSGUI_WIN_MAXIMIZED;
                        } else {
                            w->min_x = w->x; w->min_y = w->y;
                            w->min_w = w->w; w->min_h = w->h;
                            w->x = 0; w->y = 0;
                            w->w = g_dwm.screen_w; w->h = taskbar_top();
                            w->flags |= DOSGUI_WIN_MAXIMIZED;
                        }
                        return;
                    }
                    if (x >= bmin.x && x < bmin.x + bmin.w &&
                        y >= bmin.y && y < bmin.y + bmin.h) {
                        w->flags |= DOSGUI_WIN_MINIMIZED;
                        return;
                    }
                }
            }
        }

        int i = hit_test(x, y);
        if (i < 0) {
            int icon_idx = icon_hit_test(x, y);
            if (icon_idx >= 0) {
                if (btn == 2) { /* Right click */
                    icon_show_context_menu(icon_idx, x, y);
                    return;
                }
                /* Windows semantics: the FIRST click selects, the SECOND opens.
                 * Selection is what rename/delete/properties act on, so without it the
                 * only way to touch an icon was to run it. */
                unsigned now = wm_now_ms();
                if (getenv("WUBU_INPUT_DEBUG"))
                    fprintf(stderr, "[icon] hit=%d at %d,%d icon=(%d,%d)\n",
                            icon_idx, x, y, g_dwm.icons[icon_idx].x, g_dwm.icons[icon_idx].y);
                bool dbl = (icon_idx == s_last_icon &&
                            now - s_last_icon_ms <= ICON_DBLCLICK_MS);
                s_last_icon = icon_idx;
                s_last_icon_ms = now;

                for (int k = 0; k < g_dwm.icon_count; k++)
                    g_dwm.icons[k].selected = (k == icon_idx);

                if (!dbl) {
                    /* First click: select and arm the drag, do NOT launch. */
                    g_dwm.drag_icon_id = icon_idx;
                    g_dwm.drag_icon_ox = x - g_dwm.icons[icon_idx].x;
                    g_dwm.drag_icon_oy = y - g_dwm.icons[icon_idx].y;
                    return;
                }

                /* Unfocus first: the launch below focuses the new window, so
                 * clearing it afterwards would leave nothing focused and the
                 * new window would never receive the keyboard. */
                if (getenv("WUBU_INPUT_DEBUG"))
                    fprintf(stderr, "[icon] OPEN '%s' type=%d click=%p exec=%p\n",
                            g_dwm.icons[icon_idx].name, (int)g_dwm.icons[icon_idx].type,
                            (void *)g_dwm.icons[icon_idx].on_click,
                            (void *)g_dwm.icons[icon_idx].on_execute);
                g_dwm.focused_id = -1;
                if (g_dwm.icons[icon_idx].on_click) {
                    g_dwm.icons[icon_idx].on_click();
                } else if (g_dwm.icons[icon_idx].on_execute) {
                    g_dwm.icons[icon_idx].on_execute();
                } else {
                    /* No per-icon closure: dispatch on what the icon IS.
                     * A shortcut must run its .desktop Exec line -- before
                     * this branch existed, DESK_ICON_SHORTCUT was created but
                     * never consulted, so clicking a shortcut silently did
                     * nothing (or launched an app named after the file). */
                    desktop_launch_icon(icon_idx);
                }
                s_last_icon = -1;      /* a third click starts a new pair */
                g_dwm.drag_icon_id = icon_idx;
                g_dwm.drag_icon_ox = x - g_dwm.icons[icon_idx].x;
                g_dwm.drag_icon_oy = y - g_dwm.icons[icon_idx].y;
                return;
            }
            g_dwm.focused_id = -1;
            /* Windows clears the selection when you click the desktop. */
            for (int k = 0; k < g_dwm.icon_count; k++) g_dwm.icons[k].selected = false;
            s_last_icon = -1;
            if (btn == 2) { /* Right click on empty desktop */
                desktop_show_context_menu(x, y);
                return;
            }
            /* Left press on empty desktop: start the rubber-band
             * drag-select lasso (Classic Mac / Win98 lesson). */
            g_dwm.lasso_active = true;
            g_dwm.lasso_x0 = x; g_dwm.lasso_y0 = y;
            g_dwm.lasso_x1 = x; g_dwm.lasso_y1 = y;
            return;
        }

        raise_win(i);
        g_dwm.focused_id = i;
        DosGuiWindow *w = &g_dwm.windows[i];

        ChromeButtonRect bmin, bmax, bclose;
        if (chrome_button_rects(w, &bmin, &bmax, &bclose)) {
            if (x >= bclose.x && x < bclose.x + bclose.w &&
                y >= bclose.y && y < bclose.y + bclose.h) {
                close_win(i);
                return;
            }
            if (x >= bmax.x && x < bmax.x + bmax.w &&
                y >= bmax.y && y < bmax.y + bmax.h) {
                if (w->flags & DOSGUI_WIN_MAXIMIZED) {
                    w->x = w->min_x; w->y = w->min_y;
                    w->w = w->min_w; w->h = w->min_h;
                    w->flags &= ~DOSGUI_WIN_MAXIMIZED;
                } else {
                    w->min_x = w->x; w->min_y = w->y;
                    w->min_w = w->w; w->min_h = w->h;
                    w->x = 0; w->y = 0;
                    w->w = g_dwm.screen_w; w->h = taskbar_top();
                    w->flags |= DOSGUI_WIN_MAXIMIZED;
                }
                return;
            }
            if (x >= bmin.x && x < bmin.x + bmin.w &&
                y >= bmin.y && y < bmin.y + bmin.h) {
                w->flags |= DOSGUI_WIN_MINIMIZED;
                return;
            }
        }

        /* Edge/corner resize (Chicago/Mac baseline) -- takes priority over
         * title-bar drag when the grab is on a window border. */
        if (!(w->flags & DOSGUI_WIN_MAXIMIZED)) {
            int e = hit_test_edge(w, x, y);
            if (e) {
                g_dwm.resize_id = i;
                g_dwm.resize_edge = e;
                g_dwm.resize_ox = x;
                g_dwm.resize_oy = y;
                g_dwm.resize_ow = w->w;
                g_dwm.resize_oh = w->h;
                return;
            }
        }

        if (y < w->y + tbh) {
            g_dwm.drag_id = i;
            g_dwm.drag_start_x = g_dwm.windows[i].x;
            g_dwm.drag_start_y = g_dwm.windows[i].y;
            g_dwm.drag_ox = x - w->x;
            g_dwm.drag_oy = y - w->y;
        } else {
            /* Client area click - dispatch to window */
            if (w->on_mouse) {
                w->on_mouse(w, x - w->x, y - w->y, btn, kind);
            }
        }
    } else if (kind == 2) {
        if (g_dwm.resize_id >= 0) g_dwm.resize_id = -1;
        if (g_dwm.drag_id >= 0 && g_dwm.windows[g_dwm.drag_id].alive) {
            DosGuiWindow *w = &g_dwm.windows[g_dwm.drag_id];
            /* Aero edge snap FIRST (Windows): drag to a screen edge and release there --
             * left/right = half, top = maximize. It requires movement from where the drag
             * started, or clicking the title of a window that already touches an edge would
             * snap it. If no zone matched, the GAAD grid snap applies as before. */
            bool moved = (w->x != g_dwm.drag_start_x) || (w->y != g_dwm.drag_start_y);
            if (!moved || !wm_snap_on_drag_release(w, x, y))
                snap_window_to_gaad(w);
        }
        g_dwm.drag_id = -1;
        if (g_dwm.drag_icon_id >= 0) {
            snap_icon_to_grid(&g_dwm.icons[g_dwm.drag_icon_id]);
            wm_save_icon_layout();   /* persist position (ReactOS-style) */
            g_dwm.drag_icon_id = -1;
        }
        /* Release the lasso: select every icon the rubber band covers. */
        if (g_dwm.lasso_active) {
            icon_select_in_rect(g_dwm.lasso_x0, g_dwm.lasso_y0,
                                       g_dwm.lasso_x1, g_dwm.lasso_y1);
            g_dwm.lasso_active = false;
        }
    } else if (kind == 0) {
        /* Live lasso update: stretch the rubber band with the cursor. */
        if (g_dwm.lasso_active) {
            g_dwm.lasso_x1 = x;
            g_dwm.lasso_y1 = y;
        }
        if (g_dwm.resize_id >= 0 && g_dwm.windows[g_dwm.resize_id].alive) {
            DosGuiWindow *w = &g_dwm.windows[g_dwm.resize_id];
            int dx = x - g_dwm.resize_ox;
            int dy = y - g_dwm.resize_oy;
            int e = g_dwm.resize_edge;
            int min_w = 80, min_h = 40;
            if (e & 1) {  /* left */
                int nw = g_dwm.resize_ow - dx;
                if (nw >= min_w) { w->x = g_dwm.resize_ox - (g_dwm.resize_ow - nw); w->w = nw; }
            }
            if (e & 2) {  /* right */
                int nw = g_dwm.resize_ow + dx;
                if (nw >= min_w) w->w = nw;
            }
            if (e & 4) {  /* top */
                int nh = g_dwm.resize_oh - dy;
                if (nh >= min_h) { w->y = g_dwm.resize_oy - (g_dwm.resize_oh - nh); w->h = nh; }
            }
            if (e & 8) {  /* bottom */
                int nh = g_dwm.resize_oh + dy;
                if (nh >= min_h) w->h = nh;
            }
        } else if (g_dwm.drag_id >= 0 && g_dwm.windows[g_dwm.drag_id].alive) {
            DosGuiWindow *w = &g_dwm.windows[g_dwm.drag_id];
            if (!(w->flags & DOSGUI_WIN_MAXIMIZED)) {
                w->x = x - g_dwm.drag_ox;
                w->y = y - g_dwm.drag_oy;
                if (w->x < -w->w + 60) w->x = -w->w + 60;
                if (w->x > g_dwm.screen_w - 60) w->x = g_dwm.screen_w - 60;
                if (w->y < 0) w->y = 0;
                if (w->y > taskbar_top() - tbh)
                    w->y = taskbar_top() - tbh;
            }
        } else {
            /* Mouse move over client area - dispatch to focused window */
            if (g_dwm.focused_id >= 0) {
                DosGuiWindow *w = &g_dwm.windows[g_dwm.focused_id];
                if (w->alive && w->on_mouse) {
                    w->on_mouse(w, x - w->x, y - w->y, btn, kind);
                }
            }
        }
        if (g_dwm.drag_icon_id >= 0) {
            DosGuiIcon *icon = &g_dwm.icons[g_dwm.drag_icon_id];
            icon->x = x - g_dwm.drag_icon_ox;
            icon->y = y - g_dwm.drag_icon_oy;
            if (icon->x < 0) icon->x = 0;
            if (icon->x > g_dwm.screen_w - DOSGUI_ICON_SIZE) icon->x = g_dwm.screen_w - DOSGUI_ICON_SIZE;
            if (icon->y < 0) icon->y = 0;
            if (icon->y > taskbar_top() - DOSGUI_ICON_SIZE) icon->y = taskbar_top() - DOSGUI_ICON_SIZE;
        }
    }
}
