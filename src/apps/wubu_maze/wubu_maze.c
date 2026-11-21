/* wubu_maze.c -- WUBU MAZE core: map, movement, DDA raycast, renderer.
 * C11. No libm; quarter-turn movement keeps the core freestanding-safe. */
#include "wubu_maze.h"

#include <stdio.h>
#include <string.h>

/* Procedural map: a bordered 16x16 arena with four solid blocks and a
 * two-cell pillar. Procedural generation avoids hand-counted string rows
 * and guarantees every cell is in range. */
int wubu_maze_cell(int x, int y)
{
    if (x < 0 || x >= WUBU_MAZE_MAP_W || y < 0 || y >= WUBU_MAZE_MAP_H) return 1;
    if (x == 0 || x == WUBU_MAZE_MAP_W - 1 || y == 0 || y == WUBU_MAZE_MAP_H - 1)
        return 1;
    if (x >= 3 && x <= 6 && y >= 3 && y <= 6) return 2;
    if (x >= 9 && x <= 12 && y >= 2 && y <= 4) return 3;
    if (x >= 2 && x <= 6 && y >= 9 && y <= 11) return 4;
    if (x >= 10 && x <= 13 && y >= 8 && y <= 12) return 2;
    if (y == 7 && (x == 8 || x == 9)) return 3;
    return 0;
}

/* Objective: three fixed gems and one exit tile. All positions are on
 * open cells; the exit only wins after all gems are collected. */
static const double g_gem_x[WUBU_MAZE_NGEMS] = { 12.5, 1.5, 14.5 };
static const double g_gem_y[WUBU_MAZE_NGEMS] = {  1.5, 13.5, 6.5 };
static const double g_exit_x = 7.5, g_exit_y = 13.5;

int wubu_maze_gem(int i, double *x, double *y)
{
    if (i < 0 || i >= WUBU_MAZE_NGEMS) return -1;
    if (x) *x = g_gem_x[i];
    if (y) *y = g_gem_y[i];
    return 0;
}

void wubu_maze_exit(double *x, double *y)
{
    if (x) *x = g_exit_x;
    if (y) *y = g_exit_y;
}

void wubu_maze_render_minimap(const wubu_maze_state *s, char *out)
{
    if (!s || !out) return;
    for (int y = 0; y < WUBU_MAZE_MAP_H; y++)
        for (int x = 0; x < WUBU_MAZE_MAP_W; x++)
            out[(size_t)y * WUBU_MAZE_MAP_W + (size_t)x] =
                wubu_maze_cell(x, y) ? '#' : ' ';
    out[(size_t)(int)g_exit_y * WUBU_MAZE_MAP_W + (size_t)(int)g_exit_x] = 'E';
    for (int i = 0; i < WUBU_MAZE_NGEMS; i++)
        if (!s->gem_taken[i])
            out[(size_t)(int)g_gem_y[i] * WUBU_MAZE_MAP_W +
                (size_t)(int)g_gem_x[i]] = '*';
    int px = (int)s->p.x, py = (int)s->p.y;
    if (px >= 0 && px < WUBU_MAZE_MAP_W && py >= 0 && py < WUBU_MAZE_MAP_H)
        out[(size_t)py * WUBU_MAZE_MAP_W + (size_t)px] = '@';
}

void wubu_maze_progress(const wubu_maze_state *s, char out[4])
{
    if (!out) return;
    for (int i = 0; i < WUBU_MAZE_NGEMS; i++)
        out[i] = (s && s->gem_taken[i]) ? '*' : '-';
    out[WUBU_MAZE_NGEMS] = '\0';
}

int wubu_maze_path(double sx, double sy, double tx, double ty,
                   int *out, int cap)
{
    int sxi = (int)sx, syi = (int)sy, txi = (int)tx, tyi = (int)ty;
    if (!out || cap <= 0 || wubu_maze_cell(sxi, syi) != 0 ||
        wubu_maze_cell(txi, tyi) != 0) return -1;
    const int ncells = WUBU_MAZE_MAP_W * WUBU_MAZE_MAP_H;
    int prev[WUBU_MAZE_MAP_H][WUBU_MAZE_MAP_W];
    memset(prev, 0xFF, sizeof(prev));
    int qx[256], qy[256], head = 0, tail = 0;
    qx[tail] = sxi; qy[tail] = syi; tail++;
    prev[syi][sxi] = -2;                 /* start marker */
    static const int dx[4] = { 1, -1, 0, 0 };
    static const int dy[4] = { 0, 0, 1, -1 };
    while (head < tail) {
        int x = qx[head], y = qy[head];
        head++;
        if (x == txi && y == tyi) break;
        for (int k = 0; k < 4; k++) {
            int nx = x + dx[k], ny = y + dy[k];
            if (nx < 0 || nx >= WUBU_MAZE_MAP_W ||
                ny < 0 || ny >= WUBU_MAZE_MAP_H) continue;
            if (prev[ny][nx] != -1 || wubu_maze_cell(nx, ny) != 0) continue;
            prev[ny][nx] = y * WUBU_MAZE_MAP_W + x;
            qx[tail] = nx; qy[tail] = ny; tail++;
        }
    }
    if (prev[tyi][txi] == -1) return -1; /* unreachable */

    /* Reconstruct backwards into a private buffer, then reverse. */
    int bx[256], by[256], n = 0;
    int cx = txi, cy = tyi;
    while (!(cx == sxi && cy == syi) && n < 256) {
        bx[n] = cx; by[n] = cy; n++;
        int p = prev[cy][cx];
        if (p < 0 || p >= ncells) return -1;
        cx = p % WUBU_MAZE_MAP_W;
        cy = p / WUBU_MAZE_MAP_W;
    }
    if (n > cap) return -1;
    for (int i = 0; i < n; i++) {
        out[2 * i]     = bx[n - 1 - i];
        out[2 * i + 1] = by[n - 1 - i];
    }
    return n;
}

int wubu_maze_reachable(double sx, double sy, double tx, double ty)
{
    int sxi = (int)sx, syi = (int)sy, txi = (int)tx, tyi = (int)ty;
    if (wubu_maze_cell(sxi, syi) != 0 || wubu_maze_cell(txi, tyi) != 0)
        return 0;
    unsigned char seen[WUBU_MAZE_MAP_H][WUBU_MAZE_MAP_W];
    memset(seen, 0, sizeof(seen));
    int qx[WUBU_MAZE_MAP_W * WUBU_MAZE_MAP_H];
    int qy[WUBU_MAZE_MAP_W * WUBU_MAZE_MAP_H];
    int head = 0, tail = 0;
    qx[tail] = sxi; qy[tail] = syi; tail++;
    seen[syi][sxi] = 1;
    static const int dx[4] = { 1, -1, 0, 0 };
    static const int dy[4] = { 0, 0, 1, -1 };
    while (head < tail) {
        int x = qx[head], y = qy[head];
        head++;
        if (x == txi && y == tyi) return 1;
        for (int k = 0; k < 4; k++) {
            int nx = x + dx[k], ny = y + dy[k];
            if (nx < 0 || nx >= WUBU_MAZE_MAP_W ||
                ny < 0 || ny >= WUBU_MAZE_MAP_H) continue;
            if (seen[ny][nx] || wubu_maze_cell(nx, ny) != 0) continue;
            seen[ny][nx] = 1;
            qx[tail] = nx; qy[tail] = ny; tail++;
        }
    }
    return 0;
}

int wubu_maze_tick(wubu_maze_state *s)
{
    if (!s) return 0;
    int got = 0;
    for (int i = 0; i < WUBU_MAZE_NGEMS; i++) {
        if (s->gem_taken[i]) continue;
        double dx = s->p.x - g_gem_x[i], dy = s->p.y - g_gem_y[i];
        if (dx * dx + dy * dy < 0.36) {       /* pickup radius 0.6 */
            s->gem_taken[i] = 1;
            s->gems++;
            s->score += 100;
            got++;
        }
    }
    double ex = s->p.x - g_exit_x, ey = s->p.y - g_exit_y;
    if (s->gems == WUBU_MAZE_NGEMS && ex * ex + ey * ey < 0.36)
        s->won = 1;
    return got;
}

void wubu_maze_init(wubu_maze_state *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->p.x = 2.5;
    s->p.y = 1.5;
    s->p.dir_x = 1.0;
    s->p.dir_y = 0.0;
    s->p.plane_x = 0.0;
    s->p.plane_y = 0.66;
}

int wubu_maze_walk(wubu_maze_state *s, int forward, int turn)
{
    if (!s) return 1;
    if (turn == 1) {                     /* quarter turn right */
        double dx = s->p.dir_x, px = s->p.plane_x;
        s->p.dir_x = -s->p.dir_y;  s->p.dir_y = dx;
        s->p.plane_x = -s->p.plane_y; s->p.plane_y = px;
    } else if (turn == -1) {             /* quarter turn left */
        double dx = s->p.dir_x, px = s->p.plane_x;
        s->p.dir_x = s->p.dir_y;   s->p.dir_y = -dx;
        s->p.plane_x = s->p.plane_y; s->p.plane_y = -px;
    }
    if (!forward) return 0;

    double nx = s->p.x + s->p.dir_x * 0.25 * (double)forward;
    double ny = s->p.y + s->p.dir_y * 0.25 * (double)forward;
    if (wubu_maze_cell((int)nx, (int)ny) != 0) {
        s->blocked++;
        return 1;
    }
    s->p.x = nx;
    s->p.y = ny;
    s->steps++;
    return 0;
}

double wubu_maze_cast(const wubu_maze_state *s, double camera_x,
                      int *side_out, int *cell_out)
{
    if (!s) {
        if (side_out) *side_out = 0;
        if (cell_out) *cell_out = 1;
        return 1e30;
    }
    double ray_x = s->p.dir_x + s->p.plane_x * camera_x;
    double ray_y = s->p.dir_y + s->p.plane_y * camera_x;
    int map_x = (int)s->p.x, map_y = (int)s->p.y;

    /* |1/ray| with no libm and the same 1e30 guard Lode uses for zero. */
    double delta_x = (ray_x == 0.0) ? 1e30 : 1.0 / ray_x;
    double delta_y = (ray_y == 0.0) ? 1e30 : 1.0 / ray_y;
    if (delta_x < 0) delta_x = -delta_x;
    if (delta_y < 0) delta_y = -delta_y;

    int step_x, step_y;
    double side_x, side_y;
    if (ray_x < 0) { step_x = -1; side_x = (s->p.x - map_x) * delta_x; }
    else           { step_x =  1; side_x = (map_x + 1.0 - s->p.x) * delta_x; }
    if (ray_y < 0) { step_y = -1; side_y = (s->p.y - map_y) * delta_y; }
    else           { step_y =  1; side_y = (map_y + 1.0 - s->p.y) * delta_y; }

    int side = 0, cell = 0;
    for (int i = 0; i < 64; i++) {       /* DDA: exactly one boundary/loop */
        if (side_x < side_y) { side_x += delta_x; map_x += step_x; side = 0; }
        else                 { side_y += delta_y; map_y += step_y; side = 1; }
        cell = wubu_maze_cell(map_x, map_y);
        if (cell > 0) break;
    }
    double dist = (side == 0) ? (side_x - delta_x) : (side_y - delta_y);
    if (dist <= 0.0 || dist > 1e29) dist = 1e29;
    if (side_out) *side_out = side;
    if (cell_out) *cell_out = cell;
    return dist;
}

/* Sprite pass: project uncollected gems as billboards using the inverse
 * camera matrix, clipped per column against the wall z-buffer. Follows the
 * standard transform (Lode's raycasting III): transformY is depth, so it
 * is compared directly with the perpendicular wall distance. */
#define MAZE_MAX_SPRITE_W 512

static void maze_sprites(const wubu_maze_state *s, double *zbuf,
                         int w, int h, uint8_t *rgb, char *ascii)
{
    if (w <= 0 || w > MAZE_MAX_SPRITE_W) return;
    double det = s->p.plane_x * s->p.dir_y - s->p.dir_x * s->p.plane_y;
    if (det == 0.0) return;
    double inv = 1.0 / det;
    for (int i = 0; i < WUBU_MAZE_NGEMS; i++) {
        if (s->gem_taken[i]) continue;
        double sx = g_gem_x[i] - s->p.x, sy = g_gem_y[i] - s->p.y;
        double tx = inv * (s->p.dir_y * sx - s->p.dir_x * sy);
        double ty = inv * (-s->p.plane_y * sx + s->p.plane_x * sy);
        if (ty <= 0.05) continue;                    /* behind the camera */
        int screen_x = (int)((w / 2.0) * (1.0 + tx / ty));
        int size = (int)((double)h / ty);
        if (size < 1) size = 1;
        if (size > h * 3) size = h * 3;
        int x0 = -size / 2 + screen_x, x1 = size / 2 + screen_x;
        int y0 = -size / 2 + h / 2, y1 = size / 2 + h / 2;
        if (x0 < 0) x0 = 0;
        if (x1 >= w) x1 = w - 1;
        if (y0 < 0) y0 = 0;
        if (y1 >= h) y1 = h - 1;
        for (int x = x0; x <= x1; x++) {
            if (ty >= zbuf[x]) continue;             /* wall is closer */
            for (int y = y0; y <= y1; y++) {
                if (rgb) {
                    size_t off = ((size_t)y * (size_t)w + (size_t)x) * 3u;
                    rgb[off] = 255; rgb[off + 1] = 220; rgb[off + 2] = 60;
                }
                if (ascii) ascii[(size_t)y * (size_t)w + (size_t)x] = '*';
            }
        }
    }
}

void wubu_maze_render(const wubu_maze_state *s, uint8_t *rgb, int w, int h)
{
    if (!s || !rgb || w <= 0 || h <= 0 || w > MAZE_MAX_SPRITE_W) return;
    double zbuf[MAZE_MAX_SPRITE_W];
    for (int x = 0; x < w; x++) {
        double cam = 2.0 * (double)x / (double)w - 1.0;
        int side = 0, cell = 0;
        double dist = wubu_maze_cast(s, cam, &side, &cell);
        zbuf[x] = dist;
        int line = (int)((double)h / dist);
        int top = -line / 2 + h / 2;
        int bot =  line / 2 + h / 2;
        if (top < 0) top = 0;
        if (bot >= h) bot = h - 1;

        uint8_t wr = 200, wg = 190, wb = 160;
        switch (cell) {
        case 2: wr = 220; wg = 90;  wb = 70;  break;
        case 3: wr = 80;  wg = 180; wb = 210; break;
        case 4: wr = 120; wg = 210; wb = 100; break;
        default: break;
        }
        if (side == 1) { wr /= 2; wg /= 2; wb /= 2; }

        double rdx = s->p.dir_x + s->p.plane_x * cam;
        double rdy = s->p.dir_y + s->p.plane_y * cam;
        for (int y = 0; y < h; y++) {
            uint8_t r, g, b;
            if (y < top || y > bot) {
                /* Floor casting (Lode's scanline formula): rowDistance =
                 * posZ / p with posZ = h/2 and p the row's offset from the
                 * horizon. Ceiling mirrors the floor symmetrically. */
                int p = (y > h / 2) ? (y - h / 2) : (h / 2 - y - 1);
                double row = (p > 0) ? (0.5 * (double)h) / (double)p : 1e6;
                int fx = (int)(s->p.x + row * rdx);
                int fy = (int)(s->p.y + row * rdy);
                int chk = (fx + fy) & 1;
                if (y > bot) { r = chk ? 72 : 56; g = chk ? 60 : 46; b = chk ? 46 : 36; }
                else         { r = chk ? 46 : 20; g = chk ? 64 : 28; b = chk ? 100 : 46; }
            } else { r = wr; g = wg; b = wb; }
            size_t off = ((size_t)y * (size_t)w + (size_t)x) * 3u;
            rgb[off] = r; rgb[off + 1] = g; rgb[off + 2] = b;
        }
    }
    maze_sprites(s, zbuf, w, h, rgb, NULL);
}

void wubu_maze_render_ascii(const wubu_maze_state *s, char *out, int w, int h)
{
    if (!s || !out || w <= 0 || h <= 0 || w > MAZE_MAX_SPRITE_W) return;
    double zbuf[MAZE_MAX_SPRITE_W];
    for (int x = 0; x < w; x++) {
        double cam = 2.0 * (double)x / (double)w - 1.0;
        int side = 0, cell = 0;
        double dist = wubu_maze_cast(s, cam, &side, &cell);
        zbuf[x] = dist;
        int line = (int)((double)h / dist);
        int top = -line / 2 + h / 2;
        int bot =  line / 2 + h / 2;
        if (top < 0) top = 0;
        if (bot >= h) bot = h - 1;

        char wall = '#';                    /* border wall */
        if (cell == 2) wall = 'X';
        else if (cell == 3) wall = 'O';
        else if (cell == 4) wall = '+';
        if (side == 1) {                    /* the y-side is in shadow */
            if (wall == '#') wall = '=';
            else if (wall == 'X') wall = 'x';
            else if (wall == 'O') wall = 'o';
            else if (wall == '+') wall = '*';
        }
        double rdx = s->p.dir_x + s->p.plane_x * cam;
        double rdy = s->p.dir_y + s->p.plane_y * cam;
        for (int y = 0; y < h; y++) {
            char c;
            if (y < top || y > bot) {
                int p = (y > h / 2) ? (y - h / 2) : (h / 2 - y - 1);
                double row = (p > 0) ? (0.5 * (double)h) / (double)p : 1e6;
                int fx = (int)(s->p.x + row * rdx);
                int fy = (int)(s->p.y + row * rdy);
                int chk = (fx + fy) & 1;
                c = (y > bot) ? (chk ? ',' : '.') : (chk ? '`' : ' ');
            } else c = wall;
            out[(size_t)y * (size_t)w + (size_t)x] = c;
        }
    }
    maze_sprites(s, zbuf, w, h, NULL, out);
}

uint64_t wubu_maze_hash(const uint8_t *data, size_t n)
{
    uint64_t h = 1469598103934665603ull;   /* FNV-1a 64 */
    for (size_t i = 0; i < n; i++) {
        h ^= (uint64_t)data[i];
        h *= 1099511628211ull;
    }
    return h;
}

int wubu_maze_ppm(const char *path, const uint8_t *rgb, int w, int h)
{
    if (!path || !rgb || w <= 0 || h <= 0) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    int rc = fprintf(f, "P6\n%d %d\n255\n", w, h);
    if (rc > 0 && fwrite(rgb, 1, (size_t)w * (size_t)h * 3u, f) ==
                  (size_t)w * (size_t)h * 3u)
        rc = 0;
    else
        rc = -1;
    if (fclose(f) != 0) rc = -1;
    return rc;
}
