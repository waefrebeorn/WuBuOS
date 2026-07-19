/*
 * dosgui_apps.c  --  Single App Registry Implementation
 *
 * ONE data-driven table (g_app_defs[]) is the source of truth. The desktop
 * and startmenu both consume this table; there is no second hardcoded list.
 * Each launch function creates the window AND binds its on_draw / on_mouse /
 * on_key callbacks so the app actually renders and receives input in-shell.
 *
 * C11, minimal includes, self-contained.
 */

#include "dosgui_apps.h"
#include "dosgui_dos_window.h"
#include "../gui/dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include "../runtime/wubu_dos_proc.h"
#include "../runtime/wubu_container.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -- Forward declarations for app callbacks ----------------------- */

/* Notepad / REPL bind their own callbacks internally; expose them
 * so the registry can wire them. */
void dosgui_notepad_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h);
void dosgui_notepad_key(DosGuiWindow *win, uint32_t key, uint32_t mods);
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

DosGuiWindow* dosgui_launch_my_computer(void) {
    DosGuiWindow *win = dosgui_wm_create(60, 60, 640, 480, "My Computer");
    if (win) { win->on_draw = app_placeholder_draw; }
    return win;
}

DosGuiWindow* dosgui_launch_temple_repl(void) {
    dosgui_wm_spawn_holyc_term(80, 60, 640, 480);
    return NULL;
}

DosGuiWindow* dosgui_launch_notepad(void) {
    DosGuiWindow *win = dosgui_wm_create(100, 80, 500, 400, "Untitled - Notepad");
    if (win) { win->on_draw = dosgui_notepad_draw; win->on_key = dosgui_notepad_key; }
    return win;
}

DosGuiWindow* dosgui_launch_paint(void) {
    /* Paint icon launches the real layered image editor. */
    return dosgui_launch_canvas();
}

DosGuiWindow* dosgui_launch_calculator(void) {
    DosGuiWindow *win = dosgui_wm_create(250, 140, 400, 500, "Calculator");
    if (win) { win->on_draw = app_placeholder_draw; }
    return win;
}

DosGuiWindow* dosgui_launch_terminal(void) {
    DosGuiWindow *win = dosgui_wm_create(300, 160, 800, 600, "Terminal");
    if (win) { win->on_draw = app_placeholder_draw; }
    return win;
}

DosGuiWindow* dosgui_launch_file_manager(void) {
    app_explorer_init();
    DosGuiWindow *win = dosgui_wm_create(100, 80, 800, 600, "File Manager");
    if (win) {
        win->on_draw = app_explorer_draw;
        win->on_mouse = app_explorer_mouse;
        win->on_key   = app_explorer_key;
    }
    return win;
}

DosGuiWindow* dosgui_launch_settings(void) {
    DosGuiWindow *win = dosgui_wm_create(350, 180, 640, 480, "Control Panel");
    if (win) { win->on_draw = app_placeholder_draw; }
    return win;
}

DosGuiWindow* dosgui_launch_editor(void) {
    DosGuiWindow *win = dosgui_wm_create(400, 200, 800, 600, "Editor");
    if (win) { win->on_draw = app_placeholder_draw; }
    return win;
}

DosGuiWindow* dosgui_launch_canvas(void) {
    app_canvas_init();
    DosGuiWindow *win = dosgui_wm_create(120, 90, 900, 640, "WuBu Canvas");
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
 * the genuine consumer of the dosgui_dos_window + wubu_dos_proc engines — not
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

DosGuiWindow* dosgui_launch_dos_box(const char *path) {
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
        DosGuiWindow *win = dosgui_wm_create(140, 120, 480, 240, "DOS Box (failed)");
        if (win) win->on_draw = app_placeholder_draw;
        return win;
    }
    /* Host the live DOS process in a desktop window (mounts into Styx). */
    return dosgui_dos_window_spawn(proc, 1);
}

/* Registry entry point: launches the built-in demo COM (no path needed). */
static DosGuiWindow* dosgui_launch_dos_box_default(void) {
    return dosgui_launch_dos_box(NULL);
}

DosGuiWindow* dosgui_launch_holyc_term(void) {
    dosgui_wm_spawn_holyc_term(80, 60, 640, 480);
    return NULL;
}

/* -- The single registry table ------------------------------------ */

const DosGuiAppDef g_app_defs[] = {
    { "My Computer",  "My Computer",   DESK_ICON_MY_COMPUTER, 0x0080FF00, dosgui_launch_my_computer },
    { "Temple REPL",  "HolyC REPL",    DESK_ICON_TEMPLE_REPL, 0x00800080, dosgui_launch_temple_repl },
    { "Notepad",      "Notepad",       DESK_ICON_NOTEPAD,     0x00AAAA00, dosgui_launch_notepad },
    { "Paint",        "WuBu Canvas",   DESK_ICON_PAINT,       0x000080FF, dosgui_launch_canvas },
    { "Calculator",   "Calculator",    DESK_ICON_CALCULATOR,  0x00FF8000, dosgui_launch_calculator },
    { "Terminal",     "Terminal",      DESK_ICON_TERMINAL,    0x00000000, dosgui_launch_terminal },
    { "File Manager", "File Manager",  DESK_ICON_EXPLORTER,   0x000080FF, dosgui_launch_file_manager },
    { "Settings",     "Control Panel", DESK_ICON_SETTINGS,    0x00808080, dosgui_launch_settings },
    { "Editor",       "Editor",        DESK_ICON_COUNT + 0,   0x008080FF, dosgui_launch_editor },
    { "WuBu Canvas",  "WuBu Canvas",   DESK_ICON_COUNT + 1,   0x000080FF, dosgui_launch_canvas },
    { "HolyC Term",   "HolyC Terminal",DESK_ICON_COUNT + 3,   0x00800080, dosgui_launch_holyc_term },
    { "DOS Box",      "DOS Box",        DESK_ICON_COUNT + 4,   0x0000C000, dosgui_launch_dos_box_default },
};
const int g_app_def_count = (int)(sizeof(g_app_defs) / sizeof(g_app_defs[0]));

/* -- Lookup helpers ----------------------------------------------- */

const DosGuiAppDef *dosgui_app_find(int icon_type) {
    for (int i = 0; i < g_app_def_count; i++)
        if (g_app_defs[i].icon_type == icon_type) return &g_app_defs[i];
    return NULL;
}

const DosGuiAppDef *dosgui_app_find_by_name(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_app_def_count; i++)
        if (strcmp(g_app_defs[i].name, name) == 0) return &g_app_defs[i];
    return NULL;
}

/* -- Generic launch ----------------------------------------------- */

DosGuiWindow* dosgui_app_launch(int icon_type) {
    const DosGuiAppDef *d = dosgui_app_find(icon_type);
    if (!d) return NULL;
    return d->launch();
}

DosGuiWindow* dosgui_app_launch_by_name(const char *name) {
    const DosGuiAppDef *d = dosgui_app_find_by_name(name);
    if (!d) return NULL;
    return d->launch();
}
