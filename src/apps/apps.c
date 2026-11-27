/*
 * apps.c  --  Single App Registry Implementation
 *
 * ONE data-driven table (g_app_defs[]) is the source of truth. The desktop
 * and startmenu both consume this table; there is no second hardcoded list.
 * Each launch function creates the window AND binds its on_draw / on_mouse /
 * on_key callbacks so the app actually renders and receives input in-shell.
 *
 * C11, minimal includes, self-contained.
 */

/* strdup/opendir/readdir need POSIX; without this a link that does not
 * define _POSIX_C_SOURCE (e.g. the start menu test) gets an implicit
 * declaration, and the returned pointer is truncated to 32 bits. */
#define _POSIX_C_SOURCE 200809L

#include "apps.h"
#include "dos_guest_view.h"
#include "../gui/wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_tokens.h"
#include "../gui/wubu_theme.h"
#include "../runtime/wubu_dos_proc.h"
#include "../runtime/wubu_container.h"
#include "../apps/cmd/cmd.h"
#include <unistd.h>   /* access, X_OK */
#include "../apps/calc/calc.h"
#include "../apps/bonzi/bonzi.h"
#include "../apps/comfy/comfy.h"
#include "../apps/tandem/tandem.h"
#include "../apps/taskmgr/taskmgr.h"
#include "../apps/notes.h"
#include "../apps/todo.h"
#include "../apps/music.h"
#include "../apps/control/control.h"
#include "editor/editor.h"
#include "wet_applet.h"
#include "vws_designer.h"
#include "wubu_tutorial.h"

#include <dirent.h>   /* app_scan_wet_dir: a program is a file */

/* Cross-module launchers.  Weak, so a test or minimal build that does not link
 * the applet host / designer / tutorial host still links; the menu entry is
 * simply inert there.  The real kernel build links all of them. */
__attribute__((weak)) DosGuiWindow *launch_wet_applet(const char *path);
__attribute__((weak)) DosGuiWindow *launch_vws_designer(void);
__attribute__((weak)) DosGuiWindow *gs_launch(void);
__attribute__((weak)) DosGuiWindow *wubu_tutorial_browser(void);
/* L8 Guests window: lives in src/gui/guests_panel.c (GUI_OBJS). Weak so
 * lean app-registry links without the GUI panel still link. */
__attribute__((weak)) DosGuiWindow *launch_guests(void);
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include "shell/wubu_notify.h"   /* Notification + wubu_notify_send: a dead launch must speak */

/* -- Forward declarations for app callbacks ----------------------- */

/* Terminal (cmd.c) adapter callbacks — forward-declared so
 * launch_terminal can bind them. */
void terminal_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void terminal_key(DosGuiWindow *win, uint32_t key, uint32_t mods);
void terminal_mouse(DosGuiWindow *win, int x, int y, int btn, int kind);

/* Notepad / REPL bind their own callbacks internally; expose them
 * so the registry can wire them. */
void notepad_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void notepad_key(DosGuiWindow *win, uint32_t key, uint32_t mods);
void notepad_mouse(DosGuiWindow *win, int x, int y, int btn, int kind);
void repl_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void repl_handle_key(DosGuiWindow *win, uint32_t key, uint32_t mods);

/* WuBu Canvas: real layered image editor (see app_canvas.c). */
void app_canvas_init(void);
void app_canvas_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void app_canvas_mouse(DosGuiWindow *win, int x, int y, int btn, int kind);
void app_canvas_key(DosGuiWindow *win, uint32_t key, uint32_t mods);

/* WuBu File Manager: real Explorer engine (see app_explorer.c). */
void app_explorer_init(void);
void app_explorer_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void app_explorer_mouse(DosGuiWindow *win, int x, int y, int btn, int kind);
void app_explorer_key(DosGuiWindow *win, uint32_t key, uint32_t mods);

/* -- Generic placeholder draw for apps without dedicated UI yet ------- */

static void app_placeholder_draw(DosGuiWindow *win, uint32_t *fb,
                                  int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    int cx, cy, cw, ch;
    wm_content_rect(win, &cx, &cy, &cw, &ch);
    int x = cx + 8, y = cy + 8;
    vbe_fill_rect(x, y, win->w - 16, win->h - 36, wubu_theme_colors()->win_face);
    vbe_rect(x, y, win->w - 16, win->h - 36, wubu_theme_colors()->border_dark);
    vbe_draw_text(x + 10, y + 10, win->title, wubu_theme_colors()->btn_text, 1);
    vbe_draw_text(x + 10, y + 28, "(window ready)", wubu_theme_colors()->win_title_text_ina, 1);
}

/* -- Launch functions (create window + bind callbacks) ------------ */

DosGuiWindow* launch_my_computer(void) {
    /* My Computer is the filesystem root -- bound to the real Explorer
     * engine (not a placeholder). The explorer shows the Styx9 root. */
    return launch_file_manager();
}

DosGuiWindow* launch_temple_repl(void) {
    wm_spawn_holyc_term(80, 60, 640, 480);
    return NULL;
}

/* ONE Notepad. This built its own window straight onto notepad_draw/notepad_key and never called
 * notepad_open(), so np_reset() never ran: the buffer was uninitialised (measured line_count=0,
 * cursor_line=-1), and the first keystroke ran memmove over lines[-1] -- "*** buffer overflow detected ***",
 * SIGABRT, the whole session gone. The app owns its own window; the registry asks it. */
DosGuiWindow* launch_notepad(void) {
    /* Weak: the weak-linkage test graph purposely omits Notepad entirely, and a stub that satisfies the linker
     * must never disagree with the real implementation -- here there is simply no app to launch. */
    extern DosGuiWindow* notepad_open(const char *filename) __attribute__((weak));
    if (!notepad_open) return NULL;
    return notepad_open(NULL);
}

DosGuiWindow* launch_paint(void) {
    /* Paint icon launches the real layered image editor. */
    return launch_canvas();
}

DosGuiWindow* launch_calculator(void) {
    /* Real calculator engine (Standard/Scientific/Programmer/Graphing). */
    return calc_launch();
}

DosGuiWindow* launch_terminal(void) {
    DosGuiWindow *win = wm_create(300, 160, 800, 600, "Terminal");
    if (!win) return NULL;

    /* Real terminal engine: a pty-backed shell rendered in-window. */
    WubuCmd *cmd = wubu_cmd_create(80, 24);
    if (cmd) wubu_cmd_spawn_shell(cmd, NULL);   /* NULL -> $SHELL / /bin/sh */
    win->user_data = cmd;
    win->on_draw  = terminal_draw;
    win->on_key   = terminal_key;
    win->on_mouse = terminal_mouse;
    return win;
}

/* -- W-CODE: the agentic IDE -------------------------------------------------
 *
 * W-CODE is a raw-termios TUI, so it cannot draw into a DosGuiWindow itself; it needs
 * a pty. The hosting lives in the terminal module (wcode_terminal_launch, term.c),
 * because that is where the tabbed PTY state and the spawn machinery are.
 *
 * It is declared WEAK and defined there, so a target that links apps.c WITHOUT the
 * terminal (lean app tests) simply sees NULL and reports "cannot launch" instead of
 * dragging term.o and its whole dependency tree in. Same convention as the petdex hook.
 */
__attribute__((weak)) DosGuiWindow *wcode_terminal_launch(char *const *argv);

/* Resolve the W-CODE binary: $WUBU_WCODE_BIN, then the standard prefixes, then the
 * development checkout, then PATH. A missing binary surfaces in the terminal rather
 * than being silently replaced with something else. */
static const char *wcode_binary(void)
{
    const char *env = getenv("WUBU_WCODE_BIN");
    if (env && *env) return env;

    static char path[512];

    if (access("/usr/local/bin/wcode", X_OK) == 0) return "/usr/local/bin/wcode";
    if (access("/usr/bin/wcode", X_OK) == 0)     return "/usr/bin/wcode";

    const char *home = getenv("HOME");
    if (home && *home) {
        snprintf(path, sizeof(path), "%s/W-Code/wcode", home);
        if (access(path, X_OK) == 0) return path;
    }
    return "wcode";                       /* last resort: let PATH decide */
}

/* Shared body: build argv (binary + optional file to open) and hand off to the
 * terminal module. Lives here so every caller resolves the binary identically. */
static DosGuiWindow *wcode_open(const char *path)
{
    if (!wcode_terminal_launch) return NULL;      /* terminal not linked */

    const char *bin = wcode_binary();
    char *argv[3];
    argv[0] = (char *)bin;
    argv[1] = (char *)path;
    argv[2] = NULL;
    return wcode_terminal_launch(path ? argv : (char *[]){ (char *)bin, NULL });
}

DosGuiWindow *launch_wcode(void) {
    return wcode_open(NULL);
}

/* Called by the Explorer's "Open With..." menu item. Declared WEAK in the explorer so
 * lean targets that link explorer.c without apps.o simply see NULL -- the same
 * convention as desktop_stubs.c and the petdex hook. */
void ex_launch_with_wcode(const char *path) {
    if (path && *path) wcode_open(path);
}

/* -- Terminal (cmd.c) adapters: bridge DosGuiWindow callbacks to WubuCmd -- */

void terminal_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    WubuCmd *cmd = (WubuCmd *)win->user_data;
    if (cmd) wubu_cmd_draw(cmd, (void *)win, fb, fb_w, fb_h);
}

void terminal_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)mods;
    WubuCmd *cmd = (WubuCmd *)win->user_data;
    if (cmd) wubu_cmd_key(cmd, (int)key);
}

void terminal_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    (void)win; (void)x; (void)y; (void)btn; (void)kind;
    /* Terminal has no mouse-driven UI yet; pty input is keyboard-only. */
}

DosGuiWindow* launch_file_manager(void) {
    /* ONE File Manager: the explorer ENGINE owns the window id, the path, the
     * tree, the tabs and the input handlers. This used to build its own window
     * with app_* callbacks, which left g_explorer.win_id at 0 -- so
     * explorer_is_open() was false for the window the user was looking at, and
     * every engine guard (navigate, back, and the rescan once the Styx namespace
     * is built) silently did nothing. The pane therefore stayed empty. */
    extern void explorer_show(void) __attribute__((weak));
    extern DosGuiWindow *explorer_window(void) __attribute__((weak));
    if (explorer_show) {
        explorer_show();
        return explorer_window ? explorer_window() : NULL;
    }
    app_explorer_init();                 /* narrow link with no engine */
    return wm_create(100, 80, 800, 600, "File Manager");
}

DosGuiWindow* launch_settings(void) {
    /* The Control Panel the desktop opens is the APPLET ENGINE (src/gui/control_panel.c): a labelled
     * sidebar and the ten machine pages. It used to open a second, older panel (control_launch), which
     * is why every applet page was unreachable in a real session. The old panel stays only as a fallback
     * for a link that has no WM. */
    extern DosGuiWindow *control_panel_launch(void) __attribute__((weak));
    if (control_panel_launch) {
        DosGuiWindow *w = control_panel_launch();
        if (w) return w;
    }
    return control_launch();
}

DosGuiWindow* launch_editor(void) {
    /* The real WubuEditor engine (apps/editor/editor.c -> wubu_editor.h).
     * This used to call launch_notepad(), so the 468-LOC editor plus
     * its find/selection/undo/macro/bookmark modules were unreachable from
     * the desktop. */
    return editor_launch();
}

DosGuiWindow* launch_getting_started(void);

DosGuiWindow* launch_tutorials(void) {
    /* Learn by doing: every tutorial can open the real tool it teaches. */
    return wubu_tutorial_browser ? wubu_tutorial_browser() : NULL;
}

/* Weak: the petdex pulls in the window manager, and the lean test targets
 * that link apps.c do not carry it. A missing petdex must not break them. */
__attribute__((weak)) DosGuiWindow* wubu_petdex_open(void);

DosGuiWindow* launch_petdex(void) {
    /* The petdex: the pixel pets, animated, pick one. */
    if (!wubu_petdex_open) return NULL;
    return wubu_petdex_open();
}

DosGuiWindow* launch_getting_started(void) {
    /* The first-run path: where to start, in the product rather than in a
     * wiki.  Reachable from Help and from the greeter's offers row. */
    return gs_launch ? gs_launch() : NULL;
}

/* launch_vws_designer is weak (VWS is optional at link time).  Taking its
 * address for the registry is fine, but CALLING it when undefined jumps to 0 --
 * so the registry gets this adapter, exactly like launch_wet() below. */
/* A capability we do not have must SAY SO. Returning NULL from an icon's launch path makes
 * the click vanish with no feedback -- which is how "VWS Designer" and "WET Applet" sat on the
 * desktop doing nothing. docs/dead-paths.md records the fix (link the vws + wb_lv stack after
 * the duplicated wb_lv is unified); until then the user gets a notification, not silence. */
/* WEAK, not linked-in: the product has the strong unit (wubu_notify.c), the registry tests do
 * not, and a test that supplies its own stub must not collide with a second definition. When
 * the symbol is absent the branch below falls through to stderr and says what is missing. */
extern uint32_t wubu_notify_send(const Notification *n) __attribute__((weak));

static void launch_unavailable(const char *what) {
    if (wubu_notify_send) {
        Notification n;
        memset(&n, 0, sizeof n);
        snprintf(n.app_name, sizeof n.app_name, "WuBuOS");
        snprintf(n.summary, sizeof n.summary, "%s is not in this build", what);
        snprintf(n.body, sizeof n.body, "The applet exists but its runtime is not linked yet.");
        (void)wubu_notify_send(&n);
        return;
    }
    fprintf(stderr, "wubu: %s is not in this build\n", what);
}

DosGuiWindow* launch_vws_designer_entry(void) {
    if (launch_vws_designer) return launch_vws_designer();
    launch_unavailable("VWS Designer");
    return NULL;
}

DosGuiWindow* launch_wet(void) {
    /* A .wet applet: Lua decides, C lays out.  With no path the launcher opens the
     * built-in demo, so the menu item works before any script exists. */
    if (launch_wet_applet) return launch_wet_applet(NULL);
    launch_unavailable("WET Applet");
    return NULL;
}

/* L8: the Guests window (imported rootfs list + Launch). Adapter guards
 * the weak symbol -- calling an undefined weak function jumps to 0. */
DosGuiWindow* launch_guests_entry(void) {
    return launch_guests ? launch_guests() : NULL;
}

DosGuiWindow* launch_canvas(void) {
    app_canvas_init();
    DosGuiWindow *win = wm_create(120, 90, 900, 640, "WuBu Canvas");
    if (win) {
        win->on_draw = app_canvas_draw;
        win->on_mouse = app_canvas_mouse;
        win->on_key   = app_canvas_key;
    }
    return win;
}

/*
 * DOS Box: launch a real 16-bit .COM/.EXE INSIDE WuBuOS (in-process 8086
 * interpreter) and host its captured framebuffer in a desktop window. This is
 * the genuine consumer of the dos_window + wubu_dos_proc engines — not
 * a placeholder. `path` may be NULL, in which case a tiny built-in demo .COM
 * is assembled and run so the box always has something real to show.
 */
static const uint8_t g_demo_com[] = {
    0xB4,0x09,             /* MOV AH,09h */
    0xBA,0x10,0x01,        /* MOV DX,msg  (offset 0x0110) */
    0xCD,0x21,             /* INT 21h     (DOS print $-string) */
    0xB4,0x4C,0x00,0x00,   /* MOV AX,4C00h ; INT 21h/4Ch (exit) */
    /* msg at 0x0110 */    'W','u','B','u','O','S',' ','D','O','S',' ','B','o','x','!',0x0D,0x0A,'$'
};

DosGuiWindow* launch_dos_box(const char *path) {
    WubuDosProc *proc = NULL;
    if (path) {
        /* Extension-based format detection. */
        int fmt = WUBU_PAYLOAD_DOS_COM;
        const char *ext = strrchr(path, '.');
        if (ext && (strcmp(ext, ".exe") == 0 || strcmp(ext, ".EXE") == 0))
            fmt = WUBU_PAYLOAD_DOS_EXE;
        proc = wubu_dos_proc_launch(path, fmt);
    } else {
        /* No binary supplied: run the built-in demo COM from memory. */
        char tmp[256];
        snprintf(tmp, sizeof(tmp), "/tmp/wubu_dosbox_demo_%d.com", (int)getpid());
        FILE *f = fopen(tmp, "wb");
        if (f) {
            fwrite(g_demo_com, 1, sizeof(g_demo_com), f);
            fclose(f);
            proc = wubu_dos_proc_launch(tmp, WUBU_PAYLOAD_DOS_COM);
            remove(tmp);
        }
    }
    if (!proc) {
        /* Engine failed to start; surface a real window reporting the fault
         * rather than silently returning NULL. */
        DosGuiWindow *win = wm_create(140, 120, 480, 240, "DOS Box (failed)");
        if (win) win->on_draw = app_placeholder_draw;
        return win;
    }
    /* Host the live DOS process in a desktop window (mounts into Styx). */
    return dos_window_spawn(proc, 1);
}

/* Registry entry point: launches the built-in demo COM (no path needed). */
DosGuiWindow* launch_dos_box_default(void) {
    return launch_dos_box(NULL);
}

DosGuiWindow* launch_holyc_term(void) {
    wm_spawn_holyc_term(80, 60, 640, 480);
    return NULL;
}

/* WubuPad / WubuOffice: our own applications, built from their own repositories
 * (mk/vendored.mk) and started through the desktop's single spawn path. They are not
 * .WET applets yet, so there is no in-process window to return; the launch still has to
 * be REAL, which is why it execs the built binary rather than opening an empty window
 * and calling it wired. WUBUPAD_BIN / WUBUOFFICE_BIN override the paths. */
/* The vendored apps build with their own CMake into their own trees, so the binary
 * path is not a constant we can guess: try the candidates in order and start the first
 * one that exists. A hardcoded path that is wrong is indistinguishable from a broken
 * shortcut -- the click does nothing and nobody knows why. */
/* WEAK, because apps.c is linked into test graphs that have no shell: a registry test
 * must be able to assert "this app is registered" without dragging in the compositor.
 * This is the same pattern wm_input.c already uses for launch_app(). */
extern void desktop_launch_program(const char *cmd) __attribute__((weak));

static int bin_exists(const char *p) { return p && *p && access(p, X_OK) == 0; }

static void launch_vendored(const char *env_var, const char *const *candidates, size_t n) {
    if (!desktop_launch_program) {
        fprintf(stderr, "wubu: no desktop launch path in this build (%s)\n", env_var);
        return;
    }
    const char *env = getenv(env_var);
    if (bin_exists(env)) { desktop_launch_program(env); return; }
    for (size_t i = 0; i < n; i++)
        if (bin_exists(candidates[i])) { desktop_launch_program(candidates[i]); return; }
    fprintf(stderr, "wubu: %s is not built -- run `make vendored`\n", env_var);
}

static DosGuiWindow* launch_wubupad(void) {
    static const char *const paths[] = {
        "src/apps/wubupad/build/wubupad", "build/wubupad", "../src/apps/wubupad/build/wubupad"
    };
    launch_vendored("WUBUPAD_BIN", paths, sizeof paths / sizeof paths[0]);
    return NULL;
}

static DosGuiWindow* launch_wubuoffice(void) {
    static const char *const paths[] = {
        "src/apps/wubuoffice/build/wubushow", "build/wubushow",
        "../src/apps/wubuoffice/build/wubushow"
    };
    launch_vendored("WUBUOFFICE_BIN", paths, sizeof paths / sizeof paths[0]);
    return NULL;
}

DosGuiWindow* launch_edr_dashboard(void) {
    return edr_dash_launch();
}

/* -- The single registry table ------------------------------------ */

const DosGuiAppDef g_app_defs[] = {
    { "My Computer",  "My Computer",   DESK_ICON_MY_COMPUTER, 0x0080FF00, launch_my_computer },
    { "Temple REPL",  "HolyC REPL",    DESK_ICON_TEMPLE_REPL, 0x00800080, launch_temple_repl },
    { "Notepad",      "Notepad",       DESK_ICON_NOTEPAD,     0x00AAAA00, launch_notepad },
    { "Paint",        "WuBu Canvas",   DESK_ICON_PAINT,       0x000080FF, launch_canvas },
    { "Calculator",   "Calculator",    DESK_ICON_CALCULATOR,  0x00FF8000, launch_calculator },
    { "Terminal",     "Terminal",      DESK_ICON_TERMINAL,    0x00000000, launch_terminal },
    { "File Manager", "File Manager",  DESK_ICON_EXPLORER,   0x000080FF, launch_file_manager },
    { "Settings",     "Control Panel", DESK_ICON_SETTINGS,    0x00808080, launch_settings },
    { "Editor",       "Editor",        DESK_ICON_COUNT + 0,   0x008080FF, launch_editor },
    { "WuBu Canvas",  "WuBu Canvas",   DESK_ICON_COUNT + 1,   0x000080FF, launch_canvas },
    { "HolyC Term",   "HolyC Terminal",DESK_ICON_COUNT + 3,   0x00800080, launch_holyc_term },
    { "DOS Box",      "DOS Box",        DESK_ICON_COUNT + 4,   0x0000C000, launch_dos_box_default },
    { "EDR Activity", "EDR Activity",  DESK_ICON_COUNT + 5,   0x00C00000, launch_edr_dashboard },
    { "Bonzi Buddy",  "Bonzi Buddy",   DESK_ICON_COUNT + 6,   0x00A030A0, bonzi_launch },
    { "WubuPad",      "WubuPad",       DESK_ICON_COUNT + 7,   0x000078D4, launch_wubupad },
    { "WubuOffice",   "WubuOffice",    DESK_ICON_COUNT + 8,   0x00C42B1C, launch_wubuoffice },
    { "Comfy",        "Comfy",          DESK_ICON_COUNT + 7,   0x0000A0C0, comfy_launch },
    { "Tandem",       "Tandem",         DESK_ICON_COUNT + 8,   0x00C0A020, tandem_launch },
    { "Task Manager", "Task Manager",   DESK_ICON_COUNT + 9,   0x00C00000, taskmgr_launch },
    { "Notes",        "Notes",          DESK_ICON_COUNT + 10,  0x00A0A000, notes_launch },
    { "Todo",         "Todo",           DESK_ICON_COUNT + 11,  0x0000A0A0, todo_launch },
    { "Music",        "Music",          DESK_ICON_COUNT + 12,  0x00A050C0, music_launch },
    { "VWS Designer", "VWS Designer",   DESK_ICON_COUNT + 13,  0x00A0A0C0, launch_vws_designer_entry },
    { "Getting Started", "Getting Started", DESK_ICON_COUNT + 14, 0x00A0C0A0, launch_getting_started },
    { "Petdex", "Petdex", DESK_ICON_COUNT + 17, 0x00A0C0A0, launch_petdex },
    { "Tutorials", "Tutorials", DESK_ICON_COUNT + 15, 0x00C0A0A0, launch_tutorials },
    { "W-CODE",       "W-CODE",         DESK_ICON_COUNT + 16, 0x00A02020, launch_wcode },
    /* A .wet applet is a first-class program kind: every program made from a
     * VWS template is one of these, so the launcher belongs in the registry. */
    { "WET Applet", "WET Applet", DESK_ICON_COUNT + 16, 0x0080C0A0, launch_wet },
    /* L8: load their system -- imported distro roots, launchable guests. */
    { "Guests", "Guests", DESK_ICON_COUNT + 18, 0x0020A060, launch_guests_entry },
};
const int g_app_def_count = (int)(sizeof(g_app_defs) / sizeof(g_app_defs[0]));

/* -- Discovered .wet applets ---------------------------------------- */

/* The .wet applet directory and its scan now live in apps_wet.c --
 * one owner per concern, and the start menu can ask "what applets exist?"
 * without linking the whole compiled-in registry and launcher set. */

/* -- Lookup helpers ----------------------------------------------- */

/* Test seams: let a gate type at a registry-launched window without exposing the window struct to every
 * test TU (apps_test.c keeps its own stubs for the chrome types, so it cannot include wm.h). */
void app_test_window_key(DosGuiWindow *w, uint32_t key) { if (w && w->on_key) w->on_key(w, key, 0); }
int  app_test_window_alive(DosGuiWindow *w) { return w && w->alive; }
void app_test_window_resize(DosGuiWindow *w, int nw, int nh) {
    /* Weak, same reason as the paint probe: the weak-linkage graph has no WM to resize anything in. */
    extern void wm_resize(DosGuiWindow *win, int width, int height) __attribute__((weak));
    if (w && wm_resize) wm_resize(w, nw, nh);
}

/* DID THE APP PAINT?
 *
 * Sampling the rendered frame cannot answer this: the WM's own chrome and fill supply the second colour, so a
 * mutant whose on_draw() returns immediately still looked painted. This seam removes the WM from the
 * measurement entirely -- clear the window's content rect to a sentinel, call the app's on_draw, and count the
 * pixels that changed. Both drawing styles are covered: apps that paint through vbe_* write to the surface we
 * cleared, and apps that paint through the fb argument write to the buffer we hand them, which is also
 * cleared. Returns the number of changed pixels (0 = the app painted nothing). */
int app_test_paint_probe(DosGuiWindow *w, int *out_cw, int *out_ch) {
    /* Weak: the weak-linkage graph links apps.c with no renderer at all, and a stub that satisfies the linker
     * must never disagree with the real implementation -- here there is simply no surface to measure. */
    extern VBEState *vbe_state(void) __attribute__((weak));
    extern uint32_t vbe_get_pixel(int x, int y) __attribute__((weak));
    extern void vbe_fill_rect(int x, int y, int w, int h, uint32_t color) __attribute__((weak));
    if (!vbe_state || !vbe_get_pixel || !vbe_fill_rect) return -1;   /* -1: cannot measure in this graph */

    if (out_cw) *out_cw = 0;
    if (out_ch) *out_ch = 0;
    if (!w || !w->on_draw) return 0;

    int cx, cy, cw, ch;
    wm_content_rect(w, &cx, &cy, &cw, &ch);
    VBEState *st = vbe_state();
    if (!st || !st->fb || cw <= 0 || ch <= 0) return 0;
    int sw = st->width, sh = st->height;
    if (out_cw) *out_cw = cw;
    if (out_ch) *out_ch = ch;

    /* A colour no theme uses, so a change cannot be mistaken for the sentinel. */
    const uint32_t SENTINEL = 0xFF00FF01u;
    static uint32_t *buf;
    static int buf_cap;
    size_t need = (size_t)sw * (size_t)sh;
    if (need > (size_t)buf_cap) {
        uint32_t *nb = (uint32_t *)realloc(buf, need * sizeof(uint32_t));
        if (!nb) return -1;
        buf = nb; buf_cap = (int)need;
    }
    for (size_t i = 0; i < need; i++) buf[i] = SENTINEL;

    /* Clear through the SAME API the apps paint with. Writing st->fb directly cleared the wrong buffer:
     * vbe_* painters draw into the render target (st->back), so every app then measured as "painted 0 px". */
    vbe_fill_rect(cx, cy, cw, ch, SENTINEL);

    w->on_draw(w, buf, sw, sh);

    int changed = 0;
    for (int y = cy; y < cy + ch; y++) {
        if (y < 0 || y >= sh) continue;
        for (int x = cx; x < cx + cw; x++) {
            if (x < 0 || x >= sw) continue;
            uint32_t a = vbe_get_pixel(x, y);
            uint32_t b = buf[(size_t)y * (size_t)sw + (size_t)x];
            if (a != SENTINEL || b != SENTINEL) changed++;
        }
    }
    return changed;
}

const DosGuiAppDef *app_find(int icon_type) {
    for (int i = 0; i < g_app_def_count; i++)
        if (g_app_defs[i].icon_type == icon_type) return &g_app_defs[i];
    return app_find_wet(icon_type);   /* owned by apps_wet.c */
}

const DosGuiAppDef *app_find_by_name(const char *name) {
    if (!name) return NULL;
    /* Match by registry/launch key (name) OR by display title, so a caller
     * can resolve an app by the label a user clicks ("Control Panel") as well
     * as the canonical key ("Settings"). */
    for (int i = 0; i < g_app_def_count; i++)
        if (strcmp(g_app_defs[i].name, name) == 0 ||
            strcmp(g_app_defs[i].title, name) == 0)
            return &g_app_defs[i];
    /* Case-insensitive pass: the run dialog and the Start-menu search accept
     * what the user actually typed, and "notepad" should find "Notepad". */
    for (int i = 0; i < g_app_def_count; i++)
        if (strcasecmp(g_app_defs[i].name, name) == 0 ||
            strcasecmp(g_app_defs[i].title, name) == 0)
            return &g_app_defs[i];
    return app_find_wet_by_name(name);   /* owned by apps_wet.c */
}

/* -- Generic launch ----------------------------------------------- */

DosGuiWindow* app_launch(int icon_type) {
    const DosGuiAppDef *d = app_find(icon_type);
    if (!d) return NULL;
    if (d->path) return app_launch_wet_path(d->path);
    return d->launch();
}

DosGuiWindow* app_launch_by_name(const char *name) {
    const DosGuiAppDef *d = app_find_by_name(name);
    if (!d) return NULL;
    if (d->path) return app_launch_wet_path(d->path);
    return d->launch();
}
