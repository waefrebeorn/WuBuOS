/*
 * solitaire_test.c  --  game-logic tests for Solitaire (no GUI).
 */
#include "solitaire_internal.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } } while (0)

/* Access helpers */
static int total_cards(SolState *s) {
    int n = s->stock_count + s->waste_count;
    for (int p = 0; p < SOL_NPILES; p++) n += s->tab_count[p];
    for (int f = 0; f < SOL_NFOUND; f++) if (s->found[f] >= 0) n++;
    return n;
}

int main(void) {
    SolState *s = solitaire_create();
    CHECK(s != NULL, "create");
    CHECK(total_cards(s) == 52, "52 cards dealt");

    /* Tableau pile sizes: 1,2,3,4,5,6,7 */
    CHECK(s->tab_count[0] == 1 && s->tab_count[6] == 7, "tableau sizes 1..7");
    /* only top card of each pile face-up */
    for (int p = 0; p < SOL_NPILES; p++) {
        for (int k = 0; k < s->tab_count[p] - 1; k++)
            CHECK(s->tab_up[p][k] == 0, "lower tableau cards face-down");
        CHECK(s->tab_up[p][s->tab_count[p]-1] == 1, "top tableau card face-up");
    }
    /* stock has 52 - 28 = 24 cards */
    CHECK(s->stock_count == 24, "stock has 24 cards");
    CHECK(s->waste_count == 0, "waste empty");

    /* Draw stock -> waste */
    int sc = s->stock_count;
    sol_draw_stock(s);
    CHECK(s->stock_count == sc - 1, "stock decremented");
    CHECK(s->waste_count == 1, "waste incremented");

    /* Empty the stock, then draw recycles waste */
    while (s->stock_count > 0) sol_draw_stock(s);
    CHECK(s->stock_count == 0, "stock empty");
    int wc = s->waste_count;
    sol_draw_stock(s);  /* recycle */
    CHECK(s->waste_count == 0 && s->stock_count == wc, "waste recycled to stock");

    /* New game resets */
    sol_new_game(s);
    CHECK(total_cards(s) == 52, "new game 52 cards");
    CHECK(s->won == 0, "new game not won");
    CHECK(s->moves == 0, "new game 0 moves");

    /* Move-rule unit checks on a controlled layout.
     * Empty pile 0, put a red 10 then a black 9 on it via direct state.
     * Encoding: suit*13 + (rank-1). hearts 10 = 1*13+9, spades 9 = 0*13+8. */
    s->tab_count[0] = 0;
    s->tab[0][0] = 1*13 + 9;   /* hearts 10 (red) */ s->tab_up[0][0] = 1; s->tab_count[0] = 1;
    s->tab[1][0] = 0*13 + 8;   /* spades 9 (black) */ s->tab_up[1][0] = 1; s->tab_count[1] = 1;
    s->sel_pile = 1; s->sel_index = 0;
    CHECK(sol_move_to_tableau(s, 0) == 1, "black 9 on red 10 allowed");
    CHECK(s->tab_count[0] == 2 && s->tab_count[1] == 0, "run moved to pile 0");

    /* Wrong-color / wrong-rank rejected */
    s->tab[2][0] = 1*13 + 7;   /* hearts 8 (red) */ s->tab_up[2][0]=1; s->tab_count[2]=1;
    s->sel_pile = 2; s->sel_index = 0;
    /* top of pile 0 is spades 9 (black); red 8 is wrong color -> reject */
    CHECK(sol_move_to_tableau(s, 0) == 0, "red 8 on black 9 rejected");

    /* Foundation: Ace starts, ascending same suit */
    s->found[0] = -1;
    s->tab[3][0] = 2*13 + 0; s->tab_up[3][0]=1; s->tab_count[3]=1;  /* clubs Ace (rank1->0) */
    s->sel_pile = 3; s->sel_index = 0;
    CHECK(sol_move_to_foundation(s, 0) == 0, "clubs Ace rejected on spades found");
    CHECK(sol_move_to_foundation(s, 2) == 1, "clubs Ace to clubs foundation");
    CHECK(s->found[2] == 2*13 + 0, "foundation has clubs Ace");

    /* Win detection: fill all foundations to King (rank 13 -> index 12) */
    for (int f = 0; f < SOL_NFOUND; f++) s->found[f] = f*13 + 12;  /* each King */
    s->won = 0;
    sol_check_win(s);
    CHECK(s->won == 1, "win detected when all foundations at King");

    solitaire_destroy(s);
    if (failures == 0) { printf("ALL SOLITAIRE TESTS PASSED\n"); return 0; }
    printf("%d FAILURES\n", failures);
    return 1;
}
