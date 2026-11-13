/*
 * run_dialog.c -- the Win+R run dialog (Windows 11 parity).
 *
 * Self-contained: state, key handling and the render.  Two ways to act on what
 * you typed, tried in this order:
 *
 *   1. A registered application name ("notepad") launches that app.
 *   2. Anything else is treated as a COMMAND LINE and executed, with its output
 *      written to a guest file and opened in Notepad.
 *
 * The second path used to be missing entirely: the dialog called launch_app(),
 * which only resolves names against the app database, so typing "ls -la" looked
 * up an app called "ls -la" and did nothing.  That made troubleshooting,
 * automation and any kind of power use impossible from inside the OS.
 *
 * WHAT THIS RUNS, AND WHAT IT DOES NOT ISOLATE
 *   Commands are interpreted by /bin/sh and run on the host under the same uid
 *   as the desktop.  This is a shell, not a sandbox: anything the user could do
 *   in a terminal they can do here.  There is no privilege boundary being
 *   crossed, which is exactly why no attempt is made to restrict syntax --
 *   pipes, redirection and quoting all work.
 *
 *   The call BLOCKS the UI until the command finishes.  A long-running command
 *   therefore freezes the desktop; making this asynchronous is future work and
 *   is the honest limitation of this implementation.
 *
 * launch_app stays weak so headless WM tests still link.
 */

#include "run_dialog.h"
#include "../wubu_theme.h"
#include "../../kernel/vbe.h"
#include "../../runtime/wubu_spawn.h"
#include "../../apps/apps.h"
#include "../../apps/notepad.h"
#include "../../apps/guest_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RUN_MAX 128
#define RUN_OUT_FILE "run-output.txt"

__attribute__((weak)) void launch_app(const char *name);

/* Defined below; declared here because the Enter key handler calls it. */
static void run_execute(const char *query);

static bool g_open;
static char g_query[RUN_MAX];
static int  g_len;

void run_dialog_open(void)
{
    g_open = true;
    g_query[0] = '\0';
    g_len = 0;
}

void run_dialog_close(void) { g_open = false; }
bool run_dialog_is_open(void) { return g_open; }

void run_dialog_rect(int fb_w, int fb_h, int *x, int *y, int *w, int *h)
{
    int bw = 420, bh = 96;
    if (bw > fb_w - 24) bw = fb_w - 24;
    if (x) *x = (fb_w - bw) / 2;
    if (y) *y = fb_h / 3;
    if (w) *w = bw;
    if (h) *h = bh;
}

void run_dialog_handle_key(uint32_t key, uint32_t mods)
{
    (void)mods;
    if (!g_open) return;

    if (key == 0x1B) { run_dialog_close(); return; }

    if (key == 8 || key == 127) {
        if (g_len > 0) g_query[--g_len] = '\0';
        return;
    }

    if (key == 13 || key == 10) {
        if (g_len > 0) {
            g_query[g_len] = '\0';
            run_execute(g_query);
        }
        run_dialog_close();
        return;
    }

    if (key >= 32 && key < 127 && g_len < RUN_MAX - 1) {
        g_query[g_len++] = (char)key;
        g_query[g_len] = '\0';
    }
}

/* Execute whatever was typed.  A registered app name wins; everything else is
 * a command line whose output is captured into the guest filesystem and shown
 * in Notepad, so the result survives like any other document. */
static void run_execute(const char *query)
{
    if (!query || !*query) return;

    /* Case 1: the name of something the desktop knows how to start. */
    if (launch_app && app_find_by_name(query)) { launch_app(query); return; }

    /* Case 2: a command line.  Merge stderr into stdout so error messages are
     * not lost -- a command that fails silently is how you waste an hour. */
    char cmd[RUN_MAX + 8];
    snprintf(cmd, sizeof(cmd), "%s 2>&1", query);

    char *argv[4];
    argv[0] = "/bin/sh";
    argv[1] = "-c";
    argv[2] = cmd;
    argv[3] = NULL;

    char *out = wubu_popen_read(argv[0], argv);
    size_t out_len = out ? strlen(out) : 0;

    /* Build the report.  The guest file has a hard ceiling, so overflow is
     * marked explicitly rather than chopped off where nobody would notice. */
    char body[GUEST_FILE_MAX];
    int n = snprintf(body, sizeof(body),
                     "$ %s\n"
                     "----------------------------------------\n",
                     query);
    if (n < 0) n = 0;

    if (!out) {
        n += snprintf(body + n, sizeof(body) - (size_t)n,
                      "(could not start the command)\n");
    } else if (out_len == 0) {
        n += snprintf(body + n, sizeof(body) - (size_t)n, "(no output)\n");
    } else if ((size_t)n + out_len + 64 <= sizeof(body)) {
        memcpy(body + n, out, out_len);
        n += (int)out_len;
        if (body[n - 1] != '\n') body[n++] = '\n';
    } else {
        size_t room = sizeof(body) - (size_t)n - 64;
        memcpy(body + n, out, room);
        n += (int)room;
        n += snprintf(body + n, sizeof(body) - (size_t)n,
                      "\n... truncated: %zu bytes of output, %zu bytes shown\n",
                      out_len, room);
    }
    free(out);

    if (guest_file_write(RUN_OUT_FILE, (const uint8_t *)body, (uint32_t)n) != 0) {
        fprintf(stderr, "WuBuOS: run dialog could not save output to %s\n",
                RUN_OUT_FILE);
        return;
    }
    guest_file_flush();         /* the result should outlive the session */
    notepad_open(RUN_OUT_FILE);
}

bool run_dialog_handle_click(int x, int y)
{
    if (!g_open) return false;
    int bx, by, bw, bh;
    run_dialog_rect(1920, 1080, &bx, &by, &bw, &bh);
    /* The rect depends on the real output size; the WM passes it in via
     * run_dialog_rect at render time, so recompute against the WM's size. */
    extern int wm_screen_w(void);
    extern int wm_screen_h(void);
    run_dialog_rect(wm_screen_w(), wm_screen_h(), &bx, &by, &bw, &bh);
    if (x < bx || x >= bx + bw || y < by || y >= by + bh) {
        run_dialog_close();
        return true;    /* consumed: the press dismissed it */
    }
    return true;        /* inside the box: swallow */
}

void run_dialog_render(uint32_t *fb, int fb_w, int fb_h)
{
    (void)fb;
    if (!g_open) return;

    const WubuThemeColors *c = wubu_theme_colors();
    int bx, by, bw, bh;
    run_dialog_rect(fb_w, fb_h, &bx, &by, &bw, &bh);

    vbe_fill_rect_rounded(bx, by, bw, bh, 8, c->startmenu_bg);
    vbe_rect_rounded(bx, by, bw, bh, 8, c->border_light);

    vbe_draw_text(bx + 16, by + 10,
                  "Type a program, folder or document name, and WuBuOS will",
                  c->startmenu_text, 1);
    vbe_draw_text(bx + 16, by + 22, "open it.", c->startmenu_text, 1);

    int ix = bx + 16, iy = by + 48, iw = bw - 32, ih = 28;
    vbe_fill_rect_rounded(ix, iy, iw, ih, 6, c->btn_face);
    vbe_rect_rounded(ix, iy, iw, ih, 6, c->border_light);

    char disp[RUN_MAX + 2];
    snprintf(disp, sizeof(disp), "%s|", g_query);
    vbe_draw_text(ix + 10, iy + 10, disp, c->startmenu_text, 1);
}
