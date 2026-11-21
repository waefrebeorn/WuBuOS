/*
 * wubu_maze.h -- WUBU MAZE: a tiny, dependency-free C11 raycaster.
 *
 * The core is freestanding-safe: no libm, no libc beyond string.h-style
 * local loops, deterministic for a fixed input script. DDA follows
 * Lode Vandevenne's canonical grid step (one cell boundary per loop),
 * so no wall can be skipped between samples.
 */
#ifndef WUBU_MAZE_H
#define WUBU_MAZE_H

#include <stddef.h>
#include <stdint.h>

#define WUBU_MAZE_MAP_W 16
#define WUBU_MAZE_MAP_H 16

#define WUBU_MAZE_NGEMS 3

typedef struct {
    double x, y;             /* player position */
    double dir_x, dir_y;     /* view direction (length irrelevant) */
    double plane_x, plane_y; /* camera plane, perpendicular to dir */
} wubu_maze_player;

typedef struct {
    wubu_maze_player p;
    uint32_t steps;          /* successful moves */
    uint32_t blocked;        /* attempted wall moves */
    uint32_t score;          /* 100 per gem */
    uint32_t gems;           /* collected count */
    uint8_t  gem_taken[WUBU_MAZE_NGEMS];
    int won;                 /* all gems + exit tile reached */
} wubu_maze_state;

/* Fixed gem position. Returns -1 for a bad index.
 * Per-run collected flags live in wubu_maze_state.gem_taken. */
int wubu_maze_gem(int i, double *x, double *y);

/* Exit tile position (only wins once every gem is collected). */
void wubu_maze_exit(double *x, double *y);

/* 16x16 overhead minimap: '#' walls, '*' untaken gems, 'E' exit, '@'
 * player. out must hold WUBU_MAZE_MAP_W * WUBU_MAZE_MAP_H chars. */
void wubu_maze_render_minimap(const wubu_maze_state *s, char *out);

/* BFS cell path from start to target; out holds (x,y) pairs for cells AFTER
 * the start, up to cap pairs. Returns the number of pairs, or -1. */
int wubu_maze_path(double sx, double sy, double tx, double ty,
                   int *out, int cap);

/* Breadth-first reachability over open cells (orthogonal steps).
 * Used by the gate to PROVE start -> every gem -> exit is solvable. */
int wubu_maze_reachable(double sx, double sy, double tx, double ty);

/* Call after movement: collects gems in reach and detects the win.
 * Returns the number of gems collected by this call. */
int wubu_maze_tick(wubu_maze_state *s);

/* Cell value: 0 = open, >0 = wall type (also colours the stripe). */
int wubu_maze_cell(int x, int y);

/* Reset to the canonical start: (2.5,1.5), facing east. */
void wubu_maze_init(wubu_maze_state *s);

/* Move forward/back (forward = +1/-1) and turn by a quarter turn
 * (turn = +1 right, -1 left, 0 none). Returns 0 on a successful move,
 * 1 when the wall blocked it. Turning alone always returns 0. */
int wubu_maze_walk(wubu_maze_state *s, int forward, int turn);

/* Cast one ray for camera_x in [-1,1]. Returns the perpendicular wall
 * distance (never <= 0), and writes the hit side (0 = x, 1 = y) and
 * the wall cell type through the optional out pointers. */
double wubu_maze_cast(const wubu_maze_state *s, double camera_x,
                      int *side_out, int *cell_out);

/* Render a packed RGB24 buffer of w*h pixels. Pure function of state +
 * size: same inputs give byte-identical output. */
void wubu_maze_render(const wubu_maze_state *s, uint8_t *rgb, int w, int h);

/* Terminal renderer: one char per cell, no framebuffer needed. Used by
 * the interactive CLI and by the ASCII golden-frame gate. */
void wubu_maze_render_ascii(const wubu_maze_state *s, char *out, int w, int h);

/* FNV-1a 64-bit over a frame, for deterministic golden-hash gates. */
uint64_t wubu_maze_hash(const uint8_t *data, size_t n);

/* Write a binary P6 PPM (for human inspection, never committed). */
int wubu_maze_ppm(const char *path, const uint8_t *rgb, int w, int h);

#endif /* WUBU_MAZE_H */
