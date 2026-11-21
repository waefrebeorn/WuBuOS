/* test_wubu_maze.c -- behaviour gate for the WUBU MAZE raycaster.
 * Golden frame hash is a COMMITTED constant; no image is stored. */
#include "wubu_maze.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FW 64
#define FH 40
#define GOLDEN UINT64_C(0xd58dd1ee00987cfb)   /* canonical start frame, 64x40 */
#define GOLDEN_ASCII UINT64_C(0x418d801631aed259)   /* canonical ASCII frame */

static int fails;
#define CHECK(c, m) do { if (!(c)) { \
    printf("  FAIL: %s\n", (m)); ++fails; } } while (0)

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("=== WUBU MAZE gate (DDA raycaster, C11) ===\n");

    /* map + start invariants */
    CHECK(wubu_maze_cell(2, 1) == 0, "canonical start cell is open");
    CHECK(wubu_maze_cell(0, 0) == 1, "border is solid");
    CHECK(wubu_maze_cell(4, 4) == 2, "inner block is solid");
    CHECK(wubu_maze_cell(16, 1) == 1, "out of bounds is solid");

    wubu_maze_state s;
    wubu_maze_init(&s);
    CHECK(s.p.x == 2.5 && s.p.y == 1.5, "canonical start position");

    /* DDA: centre ray down the open row 1 hits the east wall at x=15.
     * Perpendicular distance from x=2.5 must be exactly 12.5. */
    int side = -1, cell = -1;
    double d = wubu_maze_cast(&s, 0.0, &side, &cell);
    CHECK(d > 12.4999999 && d < 12.5000001, "centre ray distance = 12.5");
    CHECK(side == 0, "centre ray hits an x-side");
    CHECK(cell == 1, "centre ray hits the border wall");
    printf("  PASS: DDA hits the exact wall (dist=%.6f side=%d cell=%d)\n",
           d, side, cell);

    /* Collision: a long forward walk must stop before the wall and never
     * end up inside one. */
    wubu_maze_init(&s);
    for (int i = 0; i < 100; i++) (void)wubu_maze_walk(&s, 1, 0);
    CHECK(s.blocked > 0, "walking into a wall is blocked");
    CHECK(s.p.x < 15.0, "player never enters the east wall");
    CHECK(wubu_maze_cell((int)s.p.x, (int)s.p.y) == 0,
          "player remains on an open cell");
    printf("  PASS: collision held (%u steps, %u blocked, x=%.3f)\n",
           s.steps, s.blocked, s.p.x);

    /* Quarter turns are exact and inverse. */
    wubu_maze_init(&s);
    (void)wubu_maze_walk(&s, 0, 1);
    CHECK(s.p.dir_x == 0.0 && s.p.dir_y == 1.0, "right turn faces south");
    (void)wubu_maze_walk(&s, 0, -1);
    CHECK(s.p.dir_x == 1.0 && s.p.dir_y == 0.0, "left turn restores east");
    CHECK(s.steps == 0, "turning does not count as a move");

    /* Objective: three gems, a score, and an exit that needs them all. */
    for (int i = 0; i < WUBU_MAZE_NGEMS; i++) {
        double gx, gy;
        CHECK(wubu_maze_gem(i, &gx, &gy) == 0, "gem accessor");
        CHECK(wubu_maze_cell((int)gx, (int)gy) == 0, "gem sits on open cell");
    }
    CHECK(wubu_maze_gem(WUBU_MAZE_NGEMS, NULL, NULL) != 0,
          "bad gem index rejected");

    wubu_maze_init(&s);
    for (int i = 0; i < WUBU_MAZE_NGEMS; i++) {
        double gx, gy;
        (void)wubu_maze_gem(i, &gx, &gy);
        s.p.x = gx; s.p.y = gy;
        CHECK(wubu_maze_tick(&s) == 1, "gem collected");
        CHECK(wubu_maze_tick(&s) == 0, "gem is not double-counted");
    }
    CHECK(s.gems == WUBU_MAZE_NGEMS && s.score == 300,
          "score is 100 per gem");
    double ex, ey;
    wubu_maze_exit(&ex, &ey);
    s.p.x = ex; s.p.y = ey;
    (void)wubu_maze_tick(&s);
    CHECK(s.won == 1, "exit wins once every gem is held");
    printf("  PASS: 3 gems -> 300 points, exit wins (won=%d)\n", s.won);

    /* Win needs ALL gems: a fresh state at the exit must not win. */
    wubu_maze_init(&s);
    s.p.x = ex; s.p.y = ey;
    (void)wubu_maze_tick(&s);
    CHECK(s.won == 0, "exit alone does not win");

    /* Deterministic golden frame: same state -> byte-identical RGB. */
    wubu_maze_init(&s);
    uint8_t f1[FW * FH * 3], f2[FW * FH * 3];
    memset(f1, 0, sizeof(f1));
    memset(f2, 0xAA, sizeof(f2));
    wubu_maze_render(&s, f1, FW, FH);
    wubu_maze_render(&s, f2, FW, FH);
    CHECK(memcmp(f1, f2, sizeof(f1)) == 0, "render is deterministic");
    uint64_t got = wubu_maze_hash(f1, sizeof(f1));
    printf("  frame hash = 0x%016llx (golden 0x%016llx)\n",
           (unsigned long long)got, (unsigned long long)GOLDEN);
    CHECK(GOLDEN != 0, "golden hash has been pinned");
    CHECK(got == GOLDEN, "golden frame hash matches");

    /* The playable terminal renderer gets its own golden frame. */
    char a1[FW * FH], a2[FW * FH];
    wubu_maze_render_ascii(&s, a1, FW, FH);
    wubu_maze_render_ascii(&s, a2, FW, FH);
    CHECK(memcmp(a1, a2, sizeof(a1)) == 0, "ASCII render is deterministic");
    uint64_t ah = wubu_maze_hash((const uint8_t *)a1, sizeof(a1));
    printf("  ascii hash = 0x%016llx (golden 0x%016llx)\n",
           (unsigned long long)ah, (unsigned long long)GOLDEN_ASCII);
    CHECK(GOLDEN_ASCII != 0, "ASCII golden hash has been pinned");
    CHECK(ah == GOLDEN_ASCII, "golden ASCII frame matches");

    /* Sprite differential: a gem is drawn before pickup and gone after. */
    int yellow = 0;
    for (size_t i = 0; i + 2 < sizeof(f1); i += 3)
        if (f1[i] == 255 && f1[i + 1] == 220 && f1[i + 2] == 60) yellow++;
    CHECK(yellow > 0, "gem sprite is rendered");
    wubu_maze_state cleared;
    wubu_maze_init(&cleared);
    for (int i = 0; i < WUBU_MAZE_NGEMS; i++) {
        double gx, gy;
        (void)wubu_maze_gem(i, &gx, &gy);
        cleared.p.x = gx; cleared.p.y = gy;
        (void)wubu_maze_tick(&cleared);
    }
    uint8_t f3[FW * FH * 3];
    wubu_maze_render(&cleared, f3, FW, FH);
    int yellow2 = 0;
    for (size_t i = 0; i + 2 < sizeof(f3); i += 3)
        if (f3[i] == 255 && f3[i + 1] == 220 && f3[i + 2] == 60) yellow2++;
    CHECK(yellow2 == 0, "collected gems no longer render");
    printf("  PASS: billboard sprite %d px -> %d px after pickup\n",
           yellow, yellow2);

    /* Human inspection: --ppm path writes the frame; never committed. */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ppm") == 0 && i + 1 < argc) {
            CHECK(wubu_maze_ppm(argv[i + 1], f1, FW, FH) == 0,
                  "PPM written");
            break;
        }
    }

    if (fails) {
        printf("WUBU MAZE: %d FAILURES\n", fails);
        return 1;
    }
    printf("WUBU MAZE: ALL PASS (raycast, collision, golden frame)\n");
    return 0;
}
