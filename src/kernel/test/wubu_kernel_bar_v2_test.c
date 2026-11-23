/* Bridge gate: WuBuOS consumes WuBuKernel's BAR v2 decoder and asserts
 * base+size+type+flags plus fail-closed rejection. L21 preserved. */
#include "wubu_driver_contract.h"
#include <stdio.h>

static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); ++fails; } } while (0)

int main(void)
{
    wubu_contract_bar_v2_t v;
    CHECK(wubu_contract_decode_bar_v2(0x0F000000u, 0, 0xFFF00000u, 0,
                                      &v) == 0, "bridge v2 decode");
    CHECK(v.phys == 0x0F000000ull, "bridge v2 base");
    CHECK(v.size == 0x100000ull, "bridge v2 size");
    CHECK(v.type == WUBU_CONTRACT_BARTYPE_32BIT, "bridge v2 type");
    CHECK(v.flags == (WUBU_CONTRACT_FLAG_PRESENT |
                      WUBU_CONTRACT_FLAG_WRITABLE |
                      WUBU_CONTRACT_FLAG_CACHE_INHIBIT), "bridge v2 flags");
    CHECK(wubu_contract_decode_bar_v2(0x0F000006u, 0, 0xFFF00006u, 0,
                                      &v) != 0, "bridge rejects reserved BAR type");
    CHECK(wubu_contract_decode_bar_v2(0x1F0u | WUBU_CONTRACT_BAR_IO, 0,
                                      0xFFFFFFF1u, 0, &v) != 0,
          "bridge rejects I/O BAR");
    if (fails == 0) printf("wubu_kernel_bar_v2_bridge: PASS\n");
    else printf("wubu_kernel_bar_v2_bridge: %d FAILURES\n", fails);
    return fails ? 1 : 0;
}
