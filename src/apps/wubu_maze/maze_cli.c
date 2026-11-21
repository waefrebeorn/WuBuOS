/* maze_cli.c -- playable terminal front end for WUBU MAZE.
 *
 *   make wubu_maze          -> interactive (W/S move, A/D turn, Q quit)
 *   ./wubu_maze --tour      -> deterministic scripted walk, 4 frames
 *
 * No termios, no curses, no libm: a clean C11 app on the tandem tree. */
#include "wubu_maze.h"

#include <stdio.h>
#include <string.h>

#define VW 72
#define VH 22

static int g_show_map;

static void draw(const wubu_maze_state *s)
{
    char buf[VW * VH];
    wubu_maze_render_ascii(s, buf, VW, VH);
    for (int y = 0; y < VH; y++)
        printf("%.*s\n", VW, buf + (size_t)y * VW);
    if (g_show_map) {
        char mm[WUBU_MAZE_MAP_W * WUBU_MAZE_MAP_H];
        wubu_maze_render_minimap(s, mm);
        puts("--- map ---");
        for (int y = 0; y < WUBU_MAZE_MAP_H; y++)
            printf("%.*s\n", WUBU_MAZE_MAP_W,
                   mm + (size_t)y * WUBU_MAZE_MAP_W);
    }
    printf("[pos %.2f,%.2f  steps=%u blocked=%u  gems=%u/3 score=%u%s]\n"
           "[W/S move  A/D turn  Q quit]\n",
           s->p.x, s->p.y, s->steps, s->blocked, s->gems, s->score,
           s->won ? "  *** MAZE CLEARED ***" : "");
}

static void apply(wubu_maze_state *s, char c)
{
    switch (c) {
    case 'w': case 'W': (void)wubu_maze_walk(s,  1, 0); break;
    case 's': case 'S': (void)wubu_maze_walk(s, -1, 0); break;
    case 'a': case 'A': (void)wubu_maze_walk(s,  0, -1); break;
    case 'd': case 'D': (void)wubu_maze_walk(s,  0,  1); break;
    default: break;
    }
    (void)wubu_maze_tick(s);
}

int main(int argc, char **argv)
{
    wubu_maze_state s;
    wubu_maze_init(&s);
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--map") == 0) g_show_map = 1;

    if (argc > 1 && strcmp(argv[1], "--tour") == 0) {
        /* A fixed script: walk east to the first gem, then explore. */
        static char east[41];
        memset(east, 'w', 40);
        east[40] = '\0';
        static const char *script[] = {
            east, "dwwww", "a", "wwwwww", NULL
        };
        printf("=== WUBU MAZE scripted tour ===\n");
        for (int k = 0; script[k]; k++) {
            for (const char *p = script[k]; *p; p++) apply(&s, *p);
            printf("--- frame %d ---\n", k + 1);
            draw(&s);
        }
        printf("tour complete: %u steps, %u blocked\n", s.steps, s.blocked);
        return 0;
    }

    printf("=== WUBU MAZE ===\n");
    draw(&s);
    char line[128];
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == 'q' || line[0] == 'Q') break;
        for (size_t i = 0; line[i]; i++) apply(&s, line[i]);
        draw(&s);
    }
    printf("maze closed: %u steps, %u blocked\n", s.steps, s.blocked);
    return 0;
}
