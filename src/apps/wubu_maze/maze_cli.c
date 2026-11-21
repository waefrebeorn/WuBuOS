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

static void draw(const wubu_maze_state *s)
{
    char buf[VW * VH];
    wubu_maze_render_ascii(s, buf, VW, VH);
    for (int y = 0; y < VH; y++)
        printf("%.*s\n", VW, buf + (size_t)y * VW);
    printf("[pos %.2f,%.2f  steps=%u blocked=%u  W/S move  A/D turn  Q quit]\n",
           s->p.x, s->p.y, s->steps, s->blocked);
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
}

int main(int argc, char **argv)
{
    wubu_maze_state s;
    wubu_maze_init(&s);

    if (argc > 1 && strcmp(argv[1], "--tour") == 0) {
        /* A fixed script: forward, right, forward, left, forward. */
        static const char *script[] = {
            "wwwwwwww", "dwwww", "a", "wwwwww", NULL
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
