/*
 * wubwin32k_test.c -- WuBuOS win32k-lite self-test (hosted gate).
 *
 * Exercises the REAL engine against the wubu_cap authority core:
 *   1. station/desktop/class/window object model + FIFO message queue
 *   2. audience gate (foreign pid denied)
 *   3. revoke -> post/get/peek all denied while the object RECORD survives
 *      (the NT authority property win32k carries through the cap layer)
 *   4. /n/win32 snapshot text reflects the live registry.
 *
 * Self-contained; links wubu_win32k.c + the wubu_cap objects. Opt-in target
 * (make test_win32k), mirroring test_cap's hosted pattern.
 */
#include "../../egojar.h"
#include "wubu_cap.h"
#include "wubu_win32k.h"

#include <stdio.h>
#include <string.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(c, m) do { if (c) { g_pass++; printf("  [PASS] %s\n", m); } \
                        else { g_fail++; printf("  [FAIL] %s\n", m); } } while (0)

struct win32k_prod_arg { wubu_cap_token_t w; int n; };
static void *win32k_producer(void *arg) {
    struct win32k_prod_arg *a = (struct win32k_prod_arg *)arg;
    for (int i = 0; i < a->n; i++)
        (void)wubu_win32k_post_message(a->w, 100, (uint32_t)(0x2000 + i), (uint64_t)i, 0);
    return NULL;
}

int main(void) {
    wubu_win32k_init();
    printf("=== WuBuOS win32k-lite (P1 backbone) test ===\n");

    /* 1. Station, cap-backed, owner 100. */
    wubu_cap_token_t station;
    int r = wubu_win32k_station_create("winsta0", 100, &station);
    CHECK(r > 0, "station create returns a slot");
    CHECK(!wubu_cap_token_is_null(station), "station token non-null");

    /* Foreign pid 300 must not even open the station (audience gate). */
    wubu_cap_object_t *o = wubu_cap_resolve(300, station, WUBU_RIGHT_READ);
    (void)o;
    CHECK(o == NULL, "foreign pid denied on station (audience)");

    /* 2. Desktop under the station. */
    wubu_cap_token_t desktop;
    r = wubu_win32k_desktop_create("default", station, 100, &desktop);
    CHECK(r > 0, "desktop create returns a slot");

    /* 3. Register a window class. */
    r = wubu_win32k_class_register(desktop, "Button", (uintptr_t)0x1c0de, 100);
    CHECK(r == 0, "class registered");

    /* A second desktop cannot have two classes of the same name visible? no —
       classes are scoped per-desktop; identical name on the SAME desktop is a dup. */
    r = wubu_win32k_class_register(desktop, "Button", (uintptr_t)0x1c0de, 100);
    CHECK(r < 0, "duplicate class on same desktop rejected");

    /* 4. Window (message queue). */
    wubu_cap_token_t win;
    r = wubu_win32k_window_create(desktop, "Button", "w1", 100, &win);
    CHECK(r > 0, "window create returns a slot");

    /* Post 3 messages with the right (write) access; depth grows FIFO. */
    CHECK(wubu_win32k_post_message(win, 100, 0x0111, 1, 0) == 1, "post 1 -> depth 1");
    CHECK(wubu_win32k_post_message(win, 100, 0x0112, 2, 0) == 2, "post 2 -> depth 2");
    CHECK(wubu_win32k_post_message(win, 100, 0x0113, 3, 0) == 3, "post 3 -> depth 3");

    /* Peek is non-destructive; get is FIFO and destructive. */
    wubu_win32k_msg_t m;
    CHECK(wubu_win32k_peek_message(win, 100, &m) == 1 && m.msg == 0x0111,
          "peek returns first, non-destructive");
    CHECK(wubu_win32k_get_message(win, 100, &m) == 1 && m.msg == 0x0111 && m.wparam == 1,
          "get #1 FIFO (0x0111, wparam 1)");
    CHECK(wubu_win32k_get_message(win, 100, &m) == 1 && m.msg == 0x0112,
          "get #2 FIFO (0x0112)");
    CHECK(wubu_win32k_get_message(win, 100, &m) == 1 && m.msg == 0x0113,
          "get #3 FIFO (0x0113)");
    CHECK(wubu_win32k_get_message(win, 100, &m) == 0, "queue empty on 4th get");

    /* /n/win32 snapshot reflects the registry. */
    char snap[2048];
    size_t sn = wubu_win32k_snapshot(snap, sizeof(snap));
    CHECK(sn > 0 && strstr(snap, "station winsta0"), "snapshot has station");
    CHECK(strstr(snap, "class Button"), "snapshot has class");
    CHECK(strstr(snap, "window w1"), "snapshot has window");

    /* 5a. Thread-safe message queue: a producer thread posts 20 to a window,
     *       the owning thread drains them in order (the WM pump shape). */
    {
        wubu_cap_token_t pq;
        CHECK(wubu_win32k_window_create(desktop, "Button", "pq", 100, &pq) > 0,
              "thread-sink window create");
        struct win32k_prod_arg pa = { pq, 20 };
        pthread_t th;
        CHECK(pthread_create(&th, NULL, win32k_producer, &pa) == 0, "producer thread start");
        (void)pthread_join(th, NULL);
        int got = 0, ordered = 1;
        wubu_win32k_msg_t m;
        while (wubu_win32k_get_message(pq, 100, &m) == 1) {
            if (m.wparam != (uint64_t)got) ordered = 0;
            got++;
        }
        CHECK(got == 20, "drained 20 cross-thread posts");
        CHECK(ordered, "cross-thread posts arrived in FIFO order");
    }

    /* Foreign pid cannot post to the window even with a valid token. */
    CHECK(wubu_win32k_post_message(win, 500, 0x7fff, 0, 0) < 0,
          "foreign pid denied on post");

    /* 5. The authority property: revoke the window token. */
    CHECK(wubu_win32k_revoke(win) >= 0, "revoke window token");
    CHECK(wubu_win32k_post_message(win, 100, 0, 0, 0) < 0,
          "post denied after revoke");
    wubu_win32k_msg_t m2;
    CHECK(wubu_win32k_peek_message(win, 100, &m2) < 0, "peek denied after revoke");
    CHECK(wubu_win32k_get_message(win, 100, &m2) < 0, "get denied after revoke");
    /* The record survives (snapshot still lists it) even though authority is gone. */
    size_t sn2 = wubu_win32k_snapshot(snap, sizeof(snap));
    CHECK(sn2 > 0 && strstr(snap, "window w1"), "window record survives revoke");

    /* Cleanup: destroy the rest, owner pid 100. */
    CHECK(wubu_win32k_window_destroy(win, 100) < 0, "destroy of revoked window denied (already dead)");
    CHECK(wubu_win32k_class_unregister(desktop, "Button", 100) == 0, "class unregistered");

    /* 6. GDI surface objects: ordered lock (DC<BRUSH<PEN<BITMAP) + shared
     *    refcount + delayed deletion. */
    {
        wubu_cap_token_t dc, br, pen, bm;
        CHECK(wubu_win32k_gdi_create(WUBU_WIN32K_SUB_DC,     "screenDC", 100, &dc)  > 0, "gdi dc create");
        CHECK(wubu_win32k_gdi_create(WUBU_WIN32K_SUB_BRUSH,  "redBrush", 100, &br)  > 0, "gdi brush create");
        CHECK(wubu_win32k_gdi_create(WUBU_WIN32K_SUB_PEN,    "ink",      200, &pen) > 0, "gdi pen create (owner 200)");
        CHECK(wubu_win32k_gdi_create(WUBU_WIN32K_SUB_BITMAP, "canvas",   100, &bm)  > 0, "gdi bitmap create");

        /* Ordered acquisition pid 100: dc then brush. */
        CHECK(wubu_win32k_gdi_acquire(dc, 100) == 1, "acquire DC (1 lock)");
        CHECK(wubu_win32k_gdi_acquire(br, 100) == 2, "acquire BRUSH (2 locks)");

        /* Order rule: pid 200 owns PEN and can resolve it, but a LOWER-order
         * object (dc/brush) is exclusively locked by pid 100 -> denied. */
        CHECK(wubu_win32k_gdi_acquire(pen, 200) < 0,
              "order-jump denied (lower-order locked by another pid)");

        /* Release lower, then the once-denied acquire now succeeds. */
        CHECK(wubu_win32k_gdi_release(br, 100) == 1, "release BRUSH -> 1 lock");
        CHECK(wubu_win32k_gdi_release(dc, 100) == 0, "release DC -> 0 locks");
        CHECK(wubu_win32k_gdi_acquire(pen, 200) == 1, "PEN acquired after lower cleared");
        CHECK(wubu_win32k_gdi_release(pen, 200) == 0, "release PEN -> 0 locks");

        /* Shared refcount + delayed deletion on the bitmap (owner 100). */
        CHECK(wubu_win32k_gdi_addref(bm, 100) == 2, "bitmap addref -> shared 2");
        CHECK(wubu_win32k_gdi_mark_destroy(bm, 100) == 0, "bitmap mark_destroy (delayed)");
        CHECK(wubu_win32k_gdi_addref(bm, 100) < 0, "addref denied after mark_destroy (dead)");
        char s2[4096];
        wubu_win32k_snapshot(s2, sizeof(s2));
        CHECK(strstr(s2, "gdi bitmap") != NULL, "dead-but-refcounted still listed once");
        CHECK(wubu_win32k_gdi_deref(bm, 100) == 1, "deref -> shared 1 (still delayed)");
        CHECK(wubu_win32k_gdi_deref(bm, 100) == 0, "deref -> shared 0 (delayed free)");
        wubu_win32k_snapshot(s2, sizeof(s2));
        CHECK(strstr(s2, "gdi bitmap") == NULL, "bitmap freed once refs drained");
        CHECK(strstr(s2, "gdi dc") != NULL, "snapshot still lists the DC");
    }

    printf("=== win32k-lite: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
