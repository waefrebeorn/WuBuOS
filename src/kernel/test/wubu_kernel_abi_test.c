/* Bridge gate: WuBuOS consumes WuBuKernel's v2 handle-only ABI exactly as
 * the owning repo defines it. L21 is preserved: this is a HOSTED test,
 * the ABI module is NOT linked into kernel.elf. */
#include "wubu_abi.h"
#include <stdio.h>

static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); ++fails; } } while (0)

int main(void)
{
    uint8_t wire[sizeof(wubu_abi_desc_t)];
    wubu_abi_desc_t in = {
        WUBU_ABI_MAGIC, sizeof(wubu_abi_desc_t), WUBU_ABI_VERSION,
        WUBU_ABI_FLAG_PRESENT, 0xaabbccddeeff0011ull,
        0x1100ffeeddccbbaaull, 0x0F000400ull
    };
    wubu_abi_desc_t out;
    CHECK(wubu_abi_encode(&in, wire, sizeof wire) == 0, "bridge encode");
    CHECK(wubu_abi_decode(wire, sizeof wire, &out) == 0, "bridge decode");
    CHECK(out.device_handle == in.device_handle, "bridge device handle");
    CHECK(out.driver_handle == in.driver_handle, "bridge driver handle");
    CHECK(out.mmio_token == in.mmio_token, "bridge mmio token");
    wire[8] = 3;
    CHECK(wubu_abi_decode(wire, sizeof wire, &out) != 0,
          "bridge rejects unknown version");
    CHECK(wubu_abi_decode(wire, sizeof wire - 1, &out) != 0,
          "bridge rejects truncation");
    if (fails == 0) printf("wubu_kernel_abi_bridge: PASS\n");
    else printf("wubu_kernel_abi_bridge: %d FAILURES\n", fails);
    return fails ? 1 : 0;
}
