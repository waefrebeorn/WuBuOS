/*
 * wubu_gvfs_test.c -- the async file-operation state machine.
 *
 * The load-bearing rules: a terminal handle cannot be reused without a reset
 * (that is how an operation starts on a finished handle and corrupts the
 * mount), and cancelling something that already finished is an ERROR (that is
 * how a double-completion callback double-frees).
 */
#include "wubu_gvfs.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_wubu_gvfs ===\n");
    wubu_gvfs_state_t s = WUBU_GVFS_IDLE;

    /* -- the happy path -- */
    CHECK(wubu_gvfs_start(&s, WUBU_GVFS_OPENING) == NULL, "idle -> opening");
    CHECK(wubu_gvfs_start(&s, WUBU_GVFS_READING) == NULL, "opening -> reading");
    CHECK(wubu_gvfs_start(&s, WUBU_GVFS_WRITING) == NULL, "reading -> writing");
    CHECK(wubu_gvfs_start(&s, WUBU_GVFS_SEEKING) == NULL, "writing -> seeking");
    CHECK(wubu_gvfs_start(&s, WUBU_GVFS_CLOSING) == NULL, "seeking -> closing");
    CHECK(wubu_gvfs_finish(&s) == NULL, "closing -> done");
    CHECK(s == WUBU_GVFS_DONE, "the handle is done");

    /* -- a terminal handle cannot be reused without a reset -- */
    CHECK(wubu_gvfs_start(&s, WUBU_GVFS_READING) != NULL,
          "a DONE handle refuses a new operation (that would corrupt the mount)");
    CHECK(wubu_gvfs_start(&s, WUBU_GVFS_OPENING) != NULL,
          "and refuses an open too");
    CHECK(wubu_gvfs_reset(&s) == NULL, "it can be reset");
    CHECK(s == WUBU_GVFS_IDLE, "back to idle");
    CHECK(wubu_gvfs_start(&s, WUBU_GVFS_OPENING) == NULL, "and reused");

    /* -- only MOUNTING/OPENING start from idle -- */
    s = WUBU_GVFS_IDLE;
    CHECK(wubu_gvfs_start(&s, WUBU_GVFS_READING) != NULL,
          "a read cannot start from idle (no file is open)");
    CHECK(wubu_gvfs_start(&s, WUBU_GVFS_WRITING) != NULL, "nor a write");
    CHECK(wubu_gvfs_start(&s, WUBU_GVFS_SEEKING) != NULL, "nor a seek");
    CHECK(wubu_gvfs_start(&s, WUBU_GVFS_CLOSING) != NULL, "nor a close");
    CHECK(wubu_gvfs_start(&s, WUBU_GVFS_MOUNTING) == NULL, "a mount CAN start from idle");

    /* -- only CLOSING finishes -- */
    s = WUBU_GVFS_READING;
    CHECK(wubu_gvfs_finish(&s) != NULL,
          "a read does NOT finish the handle (that would leak the descriptor)");
    CHECK(s == WUBU_GVFS_READING, "the state is unchanged");
    s = WUBU_GVFS_IDLE;
    CHECK(wubu_gvfs_finish(&s) != NULL, "idle cannot finish");

    /* -- cancellation -- */
    s = WUBU_GVFS_READING;
    CHECK(wubu_gvfs_cancel(&s) == NULL, "an in-flight read can be cancelled");
    CHECK(s == WUBU_GVFS_CANCELLED, "it is cancelled");
    CHECK(wubu_gvfs_cancel(&s) != NULL,
          "cancelling again is an ERROR (a double-completion double-frees)");
    CHECK(wubu_gvfs_start(&s, WUBU_GVFS_READING) != NULL,
          "a cancelled handle cannot start an operation");
    CHECK(wubu_gvfs_finish(&s) != NULL, "a cancelled handle cannot finish");

    s = WUBU_GVFS_IDLE;
    CHECK(wubu_gvfs_cancel(&s) != NULL, "cancelling an idle handle is an error");
    s = WUBU_GVFS_DONE;
    CHECK(wubu_gvfs_cancel(&s) != NULL,
          "cancelling a FINISHED handle is an error (double completion)");

    /* -- every in-flight state can be cancelled -- */
    {
        wubu_gvfs_state_t inflight[] = {
            WUBU_GVFS_MOUNTING, WUBU_GVFS_OPENING, WUBU_GVFS_READING,
            WUBU_GVFS_WRITING, WUBU_GVFS_SEEKING, WUBU_GVFS_CLOSING
        };
        for (size_t i = 0; i < sizeof(inflight)/sizeof(inflight[0]); i++) {
            s = inflight[i];
            CHECK(wubu_gvfs_is_in_flight(s), "the state is in flight");
            CHECK(wubu_gvfs_cancel(&s) == NULL, "and cancellable");
            CHECK(s == WUBU_GVFS_CANCELLED, "and lands in cancelled");
        }
    }

    /* -- state classification -- */
    CHECK(wubu_gvfs_is_terminal(WUBU_GVFS_DONE), "done is terminal");
    CHECK(wubu_gvfs_is_terminal(WUBU_GVFS_CANCELLED), "cancelled is terminal");
    CHECK(!wubu_gvfs_is_terminal(WUBU_GVFS_IDLE), "idle is not terminal");
    CHECK(!wubu_gvfs_is_terminal(WUBU_GVFS_READING), "reading is not terminal");
    CHECK(!wubu_gvfs_is_in_flight(WUBU_GVFS_IDLE), "idle is not in flight");
    CHECK(!wubu_gvfs_is_in_flight(WUBU_GVFS_DONE), "done is not in flight");

    /* -- names -- */
    CHECK(strcmp(wubu_gvfs_state_name(WUBU_GVFS_IDLE), "idle") == 0,
          "the states have readable names");
    CHECK(strcmp(wubu_gvfs_state_name((wubu_gvfs_state_t)99), "unknown") == 0,
          "an unknown state has a name");

    /* -- handles -- */
    {
        wubu_gvfs_handle_t h;
        wubu_gvfs_handle_init(&h, 42);
        CHECK(h.state == WUBU_GVFS_IDLE && h.handle_id == 42,
              "a handle starts idle with its id");
        CHECK(wubu_gvfs_handle_open(&h) == NULL, "open");
        CHECK(wubu_gvfs_handle_read(&h) == NULL, "read");
        CHECK(h.start_count == 2, "two operations were started");
        CHECK(wubu_gvfs_handle_close(&h) == NULL, "close");
        CHECK(wubu_gvfs_handle_finish(&h) == NULL, "finish");
        CHECK(h.state == WUBU_GVFS_DONE, "done");
        CHECK(wubu_gvfs_handle_read(&h) != NULL, "a done handle refuses a read");
        CHECK(wubu_gvfs_handle_reset(&h) == NULL, "reset");
        CHECK(h.state == WUBU_GVFS_IDLE, "idle again");

        /* cancel counting */
        wubu_gvfs_handle_init(&h, 7);
        wubu_gvfs_handle_open(&h);
        CHECK(wubu_gvfs_handle_cancel(&h) == NULL, "cancel");
        CHECK(h.cancel_count == 1, "one cancel was issued");
        CHECK(wubu_gvfs_handle_cancel(&h) != NULL, "a second is refused");
        CHECK(h.cancel_count == 1, "and is not counted");

        /* null handling */
        CHECK(wubu_gvfs_handle_open(NULL) != NULL, "a null handle is refused");
        CHECK(wubu_gvfs_handle_cancel(NULL) != NULL, "a null cancel is refused");
        CHECK(wubu_gvfs_handle_finish(NULL) != NULL, "a null finish is refused");
        CHECK(wubu_gvfs_handle_reset(NULL) != NULL, "a null reset is refused");
        CHECK(wubu_gvfs_start(NULL, WUBU_GVFS_OPENING) != NULL,
              "a null state is refused");
        CHECK(wubu_gvfs_cancel(NULL) != NULL, "a null cancel is refused");
        wubu_gvfs_handle_init(NULL, 1);
    }

    if (failures == 0) printf("ALL WUBU_GVFS TESTS PASSED\n");
    else printf("%d WUBU_GVFS FAILURES\n", failures);
    return failures ? 1 : 0;
}
