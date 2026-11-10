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
#include "dos_window.h"
#include "../gui/wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include "../runtime/wubu_dos_proc.h"
#include "../runtime/wubu_container.h"
#include "../apps/cmd/cmd.h"
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -- Forward declarations for app callbacks ----------------------- */

/* Terminal (cmd.c) adapter callbacks — forward-declared so
 * launch_terminal can bind them. */
static void terminal_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
static void terminal_key(DosGuiWindow *win, uint32_t key, uint32_t mods);
static void terminal_mouse(DosGuiWindow *win, int x, int y, int btn, int kind);

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
    int x = win->x + 8, y = win->y + 28;
    vbe_fill_rect(x, y, win->w - 16, win->h - 36, 0x00FFFFFF);
    vbe_rect(x, y, win->w - 16, win->h - 36, 0x00808080);
    vbe_draw_text(x + 10, y + 10, win->title, 0x00000000, 1);
    vbe_draw_text(x + 10, y + 28, "(window ready)", 0x00666666, 1);
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

DosGuiWindow* launch_notepad(void) {
    DosGuiWindow *win = wm_create(100, 80, 500, 400, "Untitled - Notepad");
    if (win) {
        win->on_draw  = notepad_draw;
        win->on_key   = notepad_key;
        win->on_mouse = notepad_mouse;
    }
    return win;
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

/* -- Terminal (cmd.c) adapters: bridge DosGuiWindow callbacks to WubuCmd -- */

static void terminal_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    WubuCmd *cmd = (WubuCmd *)win->user_data;
    if (cmd) wubu_cmd_draw(cmd, (void *)win, fb, fb_w, fb_h);
}

static void terminal_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)mods;
    WubuCmd *cmd = (WubuCmd *)win->user_data;
    if (cmd) wubu_cmd_key(cmd, (int)key);
}

static void terminal_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    (void)win; (void)x; (void)y; (void)btn; (void)kind;
    /* Terminal has no mouse-driven UI yet; pty input is keyboard-only. */
}

DosGuiWindow* launch_file_manager(void) {
    app_explorer_init();
    DosGuiWindow *win = wm_create(100, 80, 800, 600, "File Manager");
    if (win) {
        win->on_draw = app_explorer_draw;
        win->on_mouse = app_explorer_mouse;
        win->on_key   = app_explorer_key;
    }
    return win;
}

DosGuiWindow* launch_settings(void) {
    /* Real Control Panel engine (9 tabs: Display, Theme, Desktop, ...). */
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

DosGuiWindow* launch_getting_started(void) {
    /* The first-run path: where to start, in the product rather than in a
     * wiki.  Reachable from Help and from the greeter's offers row. */
    return gs_launch ? gs_launch() : NULL;
}

/* launch_vws_designer is weak (VWS is optional at link time).  Taking its
 * address for the registry is fine, but CALLING it when undefined jumps to 0 --
 * so the registry gets this adapter, exactly like launch_wet() below. */
DosGuiWindow* launch_vws_designer_entry(void) {
    return launch_vws_designer ? launch_vws_designer() : NULL;
}

DosGuiWindow* launch_wet(void) {
    /* A .wet applet: Lua decides, C lays out.  With no path the launcher opens
     * the built-in demo, so the menu item works before any script exists. */
    return launch_wet_applet ? launch_wet_applet(NULL) : NULL;
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
    { "File Manager", "File Manager",  DESK_ICON_EXPLORTER,   0x000080FF, launch_file_manager },
    { "Settings",     "Control Panel", DESK_ICON_SETTINGS,    0x00808080, launch_settings },
    { "Editor",       "Editor",        DESK_ICON_COUNT + 0,   0x008080FF, launch_editor },
    { "WuBu Canvas",  "WuBu Canvas",   DESK_ICON_COUNT + 1,   0x000080FF, launch_canvas },
    { "HolyC Term",   "HolyC Terminal",DESK_ICON_COUNT + 3,   0x00800080, launch_holyc_term },
    { "DOS Box",      "DOS Box",        DESK_ICON_COUNT + 4,   0x0000C000, launch_dos_box_default },
    { "EDR Activity", "EDR Activity",  DESK_ICON_COUNT + 5,   0x00C00000, launch_edr_dashboard },
    { "Bonzi Buddy",  "Bonzi Buddy",   DESK_ICON_COUNT + 6,   0x00A030A0, bonzi_launch },
    { "Comfy",        "Comfy",          DESK_ICON_COUNT + 7,   0x0000A0C0, comfy_launch },
    { "Tandem",       "Tandem",         DESK_ICON_COUNT + 8,   0x00C0A020, tandem_launch },
    { "Task Manager", "Task Manager",   DESK_ICON_COUNT + 9,   0x00C00000, taskmgr_launch },
    { "Notes",        "Notes",          DESK_ICON_COUNT + 10,  0x00A0A000, notes_launch },
    { "Todo",         "Todo",           DESK_ICON_COUNT + 11,  0x0000A0A0, todo_launch },
    { "Music",        "Music",          DESK_ICON_COUNT + 12,  0x00A050C0, music_launch },
    { "VWS Designer", "VWS Designer",   DESK_ICON_COUNT + 13,  0x00A0A0C0, launch_vws_designer_entry },
    { "Getting Started", "Getting Started", DESK_ICON_COUNT + 14, 0x00A0C0A0, launch_getting_started },
    { "Tutorials", "Tutorials", DESK_ICON_COUNT + 15, 0x00C0A0A0, launch_tutorials },
    /* A .wet applet is a first-class program kind: every program made from a
     * VWS template is one of these, so the launcher belongs in the registry. */
    { "WET Applet", "WET Applet", DESK_ICON_COUNT + 16, 0x0080C0A0, launch_wet },
};
const int g_app_def_count = (int)(sizeof(g_app_defs) / sizeof(g_app_defs[0]));

/* -- Discovered .wet applets ---------------------------------------- */

/* The .wet applet directory and its scan now live in apps_wet.c --
 * one owner per concern, and the start menu can ask "what applets exist?"
 * without linking the whole compiled-in registry and launcher set. */

/* -- Lookup helpers ----------------------------------------------- */

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
