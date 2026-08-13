/* test_vdso.c -- host tests for the vDSO page (gap H6).
 * The VA mapping is metal-only; the header + the counter refresh are
 * verified here. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "wubu_vdso.h"
#include "wubu_vdso.c"

/* the VA mapping is metal-only: the host test stubs it */
int wubu_vmm_map_page(uint64_t virt, uint64_t phys, uint32_t flags)
{
    (void)virt; (void)phys; (void)flags;
    return 0;
}

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

int main(void)
{
    printf("wubu_vdso tests (gap H6)\n");

    /* init stamps the header */
    CHECK(wubu_vdso_init() == 0);
    const wubu_vdso_t *v = wubu_vdso_get();
    CHECK(v->magic == WUBU_VDSO_MAGIC);
    CHECK(v->version == 1);
    CHECK(v->uptime_ms == 0);

    /* the tick refresh propagates */
    wubu_vdso_update(1234, 99, 42);
    CHECK(v->uptime_ms == 1234);
    CHECK(v->tick == 99);
    CHECK(v->promoted_total == 42);
    wubu_vdso_update(0, 0, 0);
    CHECK(v->uptime_ms == 0);

    /* the layout is a stable ABI: fixed offsets */
    CHECK((char *)&v->magic - (char *)v == 0);
    CHECK((char *)&v->uptime_ms - (char *)v == 8);
    CHECK((char *)&v->tick - (char *)v == 16);
    CHECK((char *)&v->promoted_total - (char *)v == 24);

    if (failures == 0) printf("test_vdso: ALL PASS\n");
    else printf("test_vdso: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}
