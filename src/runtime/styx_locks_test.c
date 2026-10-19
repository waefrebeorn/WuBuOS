#include "styx_locks.h"
#include <stdio.h>

static int pass, fail;
#define T(n) do { printf("  TEST %-52s", n); } while (0)
#define OK() do { printf("✅\n"); pass++; } while (0)
#define NO(m) do { printf("❌ %s\n", m); fail++; } while (0)

int main(void)
{
    styx_locktab_t t;
    styx_locks_init(&t);

    T("first writer lock succeeds");
    if (styx_lock(&t, 7, 100, 0, 100, STYX_LOCK_WRLCK) == 0) OK(); else NO("denied");

    T("overlapping writer lock from another owner conflicts");
    if (styx_lock(&t, 7, 200, 50, 100, STYX_LOCK_WRLCK) == -1) OK(); else NO("conflict missed");

    T("non-overlapping writer lock succeeds");
    if (styx_lock(&t, 7, 200, 200, 100, STYX_LOCK_WRLCK) == 0) OK(); else NO("denied");

    T("reader vs writer conflicts");
    if (styx_lock(&t, 7, 300, 10, 10, STYX_LOCK_RDLCK) == -1) OK(); else NO("read over write allowed");

    T("two readers on the same range coexist");
    if (styx_lock(&t, 9, 100, 0, 50, STYX_LOCK_RDLCK) == 0 &&
        styx_lock(&t, 9, 200, 0, 50, STYX_LOCK_RDLCK) == 0) OK(); else NO("readers blocked each other");

    T("same owner may re-lock its own range");
    if (styx_lock(&t, 7, 100, 0, 100, STYX_LOCK_WRLCK) == 0) OK(); else NO("self conflicted");

    T("unlock frees the lock");
    int before = styx_lock_count(&t);
    if (styx_unlock(&t, 7, 100, 0, 100) == 0 && styx_lock_count(&t) == before - 1) OK();
    else NO("unlock failed");

    T("lock ranges reaching end-of-file overlap");
    styx_locks_init(&t);
    if (styx_lock(&t, 1, 1, 100, 0, STYX_LOCK_WRLCK) == 0 &&
        styx_lock(&t, 1, 2, 500, 10, STYX_LOCK_RDLCK) == -1) OK();
    else NO("length-0 range did not reach EOF");

    printf("\n=== styx_locks: %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
