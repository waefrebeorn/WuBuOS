/*
 * hosted_display_test.c -- gate for the guest's display model.
 *
 * These are the rules every later feature depends on: what counts as a real display,
 * how a point maps to a monitor, how the virtual desktop spans several outputs, and
 * which one is primary.  Get these wrong and tiling, the overview and any resolution
 * menu inherit the mistake -- so they are pinned down here with synthetic outputs,
 * no compositor required.
 */
#include <stdio.h>
#include <string.h>

#include "hosted_display.h"

static int checks_run = 0, checks_failed = 0;

#define TEST(cond, msg) do { \
    checks_run++; \
    if (!(cond)) { checks_failed++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } \
} while (0)

int main(void)
{
    hosted_displays_t ds;

    /* -- empty model -------------------------------------------------- */
    hosted_display_reset(&ds);
    TEST(ds.count == 0, "reset yields an empty model");
    TEST(hosted_display_present(&ds) == 0, "no displays present before done events");
    TEST(hosted_display_at(&ds, 100, 100) == NULL, "no display owns a point when empty");
    TEST(hosted_display_primary(&ds) == -1, "no primary when empty");

    int bx, by, bw, bh;
    TEST(!hosted_display_bounds(&ds, &bx, &by, &bw, &bh),
         "bounds fail when there is nothing present");

    /* -- slots are keyed by registry name, not by creation order ------ */
    hosted_display_t *a = hosted_display_slot(&ds, 3);
    hosted_display_t *b = hosted_display_slot(&ds, 4);
    TEST(a != NULL && b != NULL, "two slots created");
    TEST(ds.count == 2, "count tracks distinct names");
    TEST(hosted_display_slot(&ds, 3) == a, "same name returns the same slot");
    TEST(ds.count == 2, "re-binding does not duplicate");

    /* -- a slot is NOT present until committed ------------------------ */
    TEST(hosted_display_present(&ds) == 0,
         "a bound output is not yet a real display");
    a->x = 0;  a->y = 0;  a->w = 1920; a->h = 1080;
    b->x = 1920; b->y = 0; b->w = 2560; b->h = 1440;
    TEST(hosted_display_present(&ds) == 0,
         "filling fields directly still does not make it present");
    hosted_display_commit(a);
    TEST(hosted_display_present(&ds) == 1, "one present after its done event");
    TEST(hosted_display_at(&ds, 100, 100) == a, "point lands on the primary display");
    TEST(hosted_display_at(&ds, 2000, 200) == NULL,
         "uncommitted display does not own points");

    hosted_display_commit(b);
    TEST(hosted_display_present(&ds) == 2, "both displays present");

    /* -- spanning two monitors --------------------------------------- */
    TEST(hosted_display_bounds(&ds, &bx, &by, &bw, &bh), "bounds succeed");
    TEST(bx == 0 && by == 0, "bounds start at the origin");
    TEST(bw == 1920 + 2560, "bounds span both outputs horizontally");
    TEST(bh == 1440, "bounds take the tallest output");

    TEST(hosted_display_at(&ds, 0, 0) == a, "origin belongs to the first display");
    TEST(hosted_display_at(&ds, 1919, 1079) == a, "last pixel of the first display");
    TEST(hosted_display_at(&ds, 1920, 0) == b, "first pixel of the second display");
    TEST(hosted_display_at(&ds, 4479, 1439) == b, "last pixel of the second display");
    TEST(hosted_display_at(&ds, 4480, 0) == NULL,
         "pixel past the right edge belongs to nothing");

    /* -- primary selection ------------------------------------------- */
    TEST(hosted_display_primary(&ds) == 0,
         "the display holding the origin is primary");

    /* Two displays claiming the origin: the one with the corner nearest (0,0) wins,
     * regardless of the order they were enumerated in. */
    hosted_display_t *c = hosted_display_slot(&ds, 5);
    TEST(c != NULL, "third slot created");
    c->x = -1080; c->y = 0; c->w = 2160; c->h = 1920;
    hosted_display_commit(c);
    TEST(hosted_display_primary(&ds) == 0,
         "the display whose corner is nearest the origin is primary");

    /* Primary does not depend on enumeration order. */
    hosted_displays_t order;
    hosted_display_reset(&order);
    hosted_display_t *second = hosted_display_slot(&order, 21);
    hosted_display_t *first  = hosted_display_slot(&order, 22);
    second->x = 1920; second->y = 0; second->w = 2560; second->h = 1440;
    first->x  = 0;    first->y  = 0; first->w  = 1920; first->h  = 1080;
    hosted_display_commit(second);
    hosted_display_commit(first);
    TEST(hosted_display_primary(&order) == 1,
         "primary is the origin holder even when it is enumerated last");

    /* Nothing containing the origin falls back to leftmost-topmost. */
    hosted_displays_t offset;
    hosted_display_reset(&offset);
    hosted_display_t *right = hosted_display_slot(&offset, 31);
    hosted_display_t *left  = hosted_display_slot(&offset, 32);
    right->x = 500; right->y = 400; right->w = 800; right->h = 600;
    left->x  = 100; left->y  = 300; left->w  = 800; left->h  = 600;
    hosted_display_commit(right);
    hosted_display_commit(left);
    TEST(hosted_display_primary(&offset) == 1,
         "leftmost-topmost is primary when nothing holds the origin");

    /* -- stacked (vertical) arrangement ------------------------------ */
    hosted_displays_t stacked;
    hosted_display_reset(&stacked);
    hosted_display_t *top = hosted_display_slot(&stacked, 10);
    hosted_display_t *bot = hosted_display_slot(&stacked, 11);
    top->x = 0; top->y = 0;   top->w = 1920; top->h = 1080;
    bot->x = 0; bot->y = 1080; bot->w = 1920; bot->h = 1200;
    hosted_display_commit(top);
    hosted_display_commit(bot);
    TEST(hosted_display_bounds(&stacked, &bx, &by, &bw, &bh), "stacked bounds succeed");
    TEST(bw == 1920, "stacked width is one display wide");
    TEST(bh == 2280, "stacked height spans both");
    TEST(hosted_display_at(&stacked, 960, 2000) == bot,
         "point below the first display lands on the second");

    /* -- capacity ---------------------------------------------------- */
    hosted_displays_t full;
    hosted_display_reset(&full);
    for (uint32_t n = 1; n <= HOSTED_MAX_DISPLAYS; n++)
        TEST(hosted_display_slot(&full, n) != NULL, "slot allocated within capacity");
    TEST(hosted_display_slot(&full, 99) == NULL,
         "table refuses a display beyond capacity rather than overwriting");
    TEST(full.count == HOSTED_MAX_DISPLAYS, "count stays at capacity");

    /* -- identity guards --------------------------------------------- */
    TEST(hosted_display_slot(&ds, 0) == NULL, "name zero is refused");

    printf("%d checks, %d failed\n", checks_run, checks_failed);
    return checks_failed ? 1 : 0;
}
