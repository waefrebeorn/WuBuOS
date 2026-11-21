/* maze_app.c -- WUBU MAZE as a real desktop app.
 * Draws the raycaster state straight into the window framebuffer; no text
 * renderer dependency. C11, one concern. */
#include "../../gui/wm.h"
#include "wubu_maze.h"

#include <string.h>

#define MAZE_VIEW_W 72
#define MAZE_VIEW_H 22
#define MAZE_CELL_W 8
#define MAZE_CELL_H 14

static wubu_maze_state g_maze;
static int g_started;

static uint32_t maze_color(char c)
{
    switch (c) {
    case '#': case '=': return 0x00C8BEA0;   /* lit border wall */
    case 'X': case 'x': return 0x00DC5A46;   /* red block */
    case 'O': case 'o': return 0x0050B4D2;   /* cyan block */
    case '+': case '*': return 0x0078D264;   /* green block / gem */
    case '.': case ',': return 0x00403428;   /* floor (both checker shades) */
    default:            return 0x001C2840;   /* ceiling (incl. '`') */
    }
}

static void maze_fill(uint32_t *fb, int fb_w, int fb_h,
                      int x, int y, int w, int h, uint32_t color)
{
    if (!fb) return;
    for (int yy = y; yy < y + h && yy < fb_h; yy++) {
        if (yy < 0) continue;
        for (int xx = x; xx < x + w && xx < fb_w; xx++) {
            if (xx < 0) continue;
            fb[(size_t)yy * (size_t)fb_w + (size_t)xx] = color;
        }
    }
}

static void maze_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h)
{
    (void)win;
    if (!fb || fb_w <= 0 || fb_h <= 0) return;
    char cells[MAZE_VIEW_W * MAZE_VIEW_H];
    wubu_maze_render_ascii(&g_maze, cells, MAZE_VIEW_W, MAZE_VIEW_H);
    for (int y = 0; y < MAZE_VIEW_H; y++)
        for (int x = 0; x < MAZE_VIEW_W; x++)
            maze_fill(fb, fb_w, fb_h, x * MAZE_CELL_W, y * MAZE_CELL_H,
                      MAZE_CELL_W, MAZE_CELL_H,
                      maze_color(cells[(size_t)y * MAZE_VIEW_W + (size_t)x]));

    /* HUD: three gem indicator boxes; a green bar marks the win. */
    char prog[4];
    wubu_maze_progress(&g_maze, prog);
    for (int i = 0; i < WUBU_MAZE_NGEMS; i++)
        maze_fill(fb, fb_w, fb_h,
                  MAZE_VIEW_W * MAZE_CELL_W - (WUBU_MAZE_NGEMS - i) * 14,
                  2, 10, 10, prog[i] == '*' ? 0x00FFDC3C : 0x00404040);
    if (g_maze.won)
        maze_fill(fb, fb_w, fb_h, 0, 0, fb_w, 6, 0x0078D264);
}

static void maze_key(DosGuiWindow *win, uint32_t key, uint32_t mods)
{
    (void)win; (void)mods;
    if (key == 'w' || key == 'W' || key == 0xE048) (void)wubu_maze_walk(&g_maze,  1, 0);
    else if (key == 's' || key == 'S' || key == 0xE050) (void)wubu_maze_walk(&g_maze, -1, 0);
    else if (key == 'a' || key == 'A' || key == 0xE04B) (void)wubu_maze_walk(&g_maze, 0, -1);
    else if (key == 'd' || key == 'D' || key == 0xE04D) (void)wubu_maze_walk(&g_maze, 0,  1);
    else if (key == 'r' || key == 'R') wubu_maze_init(&g_maze);
    else return;
    (void)wubu_maze_tick(&g_maze);
}

DosGuiWindow *wubu_maze_app_launch(void)
{
    if (!g_started) {
        wubu_maze_init(&g_maze);
        g_started = 1;
    }
    DosGuiWindow *win = wm_create(80, 60,
                                  MAZE_VIEW_W * MAZE_CELL_W,
                                  MAZE_VIEW_H * MAZE_CELL_H + 40,
                                  "WUBU MAZE - collect 3 gems, reach E");
    if (win) {
        win->on_draw = maze_draw;
        win->on_key = maze_key;
    }
    return win;
}
