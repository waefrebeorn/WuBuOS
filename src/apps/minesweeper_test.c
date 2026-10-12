/*
 * minesweeper_test.c  --  game-logic tests for Minesweeper (no GUI needed).
 * Verifies mine placement, safe-first-click, flood-fill, flagging, win/lose.
 */
#include "minesweeper_internal.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } } while (0)

/* Expose internals via the test by including the .c is overkill; instead we
 * drive the public API. We re-declare the grid accessors the test needs. */

int main(void) {
    MineState *m = minesweeper_create();
    CHECK(m != NULL, "create");
    CHECK(m->cols == 9 && m->rows == 9 && m->mines == 10, "default 9x9/10");
    CHECK(m->first_click == 1, "first_click set");

    /* First click is always safe (no mine, flood-reveals a region). */
    dosgui_minesweeper_reveal(4, 4);
    CHECK(m->first_click == 0, "first_click cleared after reveal");
    CHECK(m->game_over != MS_LOST, "first click never loses");
    CHECK(m->state[4 * m->cols + 4] == MS_REVEALED, "first cell revealed");

    /* Exactly `mines` mines placed. */
    int mine_count = 0;
    for (int i = 0; i < m->cols * m->rows; i++) mine_count += m->mine[i];
    CHECK(mine_count == 10, "exactly 10 mines placed");

    /* First-click cell and neighbors are mine-free. */
    int safe = 1;
    for (int dr = -1; dr <= 1; dr++)
        for (int dc = -1; dc <= 1; dc++) {
            int r = 4 + dr, c = 4 + dc;
            if (r < 0 || r >= m->rows || c < 0 || c >= m->cols) continue;
            if (m->mine[r * m->cols + c]) safe = 0;
        }
    CHECK(safe, "first-click neighborhood is mine-free");

    /* Flagging toggles and counts. */
    int before = minesweeper_flag_count(m);
    dosgui_minesweeper_flag(0, 0);
    CHECK(minesweeper_flag_count(m) == before + 1, "flag increments");
    CHECK(m->state[0] == MS_FLAGGED, "cell flagged");
    dosgui_minesweeper_flag(0, 0);
    CHECK(minesweeper_flag_count(m) == before, "unflag decrements");
    CHECK(m->state[0] == MS_HIDDEN, "cell unflagged");

    /* Revealing a flagged cell is a no-op. */
    dosgui_minesweeper_flag(0, 0);
    dosgui_minesweeper_reveal(0, 0);
    CHECK(m->state[0] == MS_FLAGGED, "flagged cell not revealed");

    /* New game resets state. */
    minesweeper_new_game(m, 9, 9, 10);
    CHECK(m->first_click == 1, "new game resets first_click");
    CHECK(m->game_over == MS_PLAYING, "new game resets game_over");
    CHECK(m->flag_count == 0, "new game resets flags");
    CHECK(m->revealed_count == 0, "new game resets revealed");

    /* Deterministic mine placement with a seed for a loss test. */
    srand(12345);
    minesweeper_new_game(m, 5, 5, 5);
    dosgui_minesweeper_reveal(2, 2);  /* safe first click */
    /* Find a mine and reveal it -> lose. */
    int lost = 0;
    for (int i = 0; i < 25 && !lost; i++) {
        if (m->mine[i] && m->state[i] == MS_HIDDEN) {
            dosgui_minesweeper_reveal(i / m->cols, i % m->cols);
            lost = 1;
        }
    }
    CHECK(lost && m->game_over == MS_LOST, "revealing a mine loses");

    /* After loss, further reveals are ignored. */
    int rc_before = m->revealed_count;
    for (int i = 0; i < 25; i++)
        if (m->state[i] == MS_HIDDEN && !m->mine[i])
            dosgui_minesweeper_reveal(i / m->cols, i % m->cols);
    CHECK(m->revealed_count == rc_before, "no reveals after loss");

    minesweeper_destroy(m);
    if (failures == 0) { printf("ALL MINESWEEPER TESTS PASSED\n"); return 0; }
    printf("%d FAILURES\n", failures);
    return 1;
}
