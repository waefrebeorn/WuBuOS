/* Bridge gate: WuBuOS consumes the WuBuKernel capability contract and
 * proves fail-closed behavior in its own hosted gate. L21 preserved. */
#include "wubu_cap.h"
#include <stdio.h>

static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); ++fails; } } while (0)

int main(void)
{
    uint64_t cap = WUBU_CAP_NONE;
    wubu_cap_reset();
    CHECK(wubu_cap_check(cap, WUBU_CAP_DEV_OPEN, 0x1af4u) == 0,
          "uncapped access denied");
    CHECK(wubu_cap_grant(WUBU_CAP_ROOT, WUBU_CAP_DEV_OPEN, 0x1af4u,
                         &cap) == 0, "root grants device cap");
    CHECK(wubu_cap_check(cap, WUBU_CAP_DEV_OPEN, 0x1af4u) == 1,
          "device cap grants exact access");
    CHECK(wubu_cap_check(cap, WUBU_CAP_MMIO_MAP, 0x1af4u) == 0,
          "device cap does not grant MMIO MAP");
    CHECK(wubu_cap_revoke(cap) == 0, "revoke cap");
    CHECK(wubu_cap_check(cap, WUBU_CAP_DEV_OPEN, 0x1af4u) == 0,
          "revoked cap fails closed");
    if (fails == 0) printf("wubu_kernel_cap_bridge: PASS\n");
    else printf("wubu_kernel_cap_bridge: %d FAILURES\n", fails);
    return fails ? 1 : 0;
}
