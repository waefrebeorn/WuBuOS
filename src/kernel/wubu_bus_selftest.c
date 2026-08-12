/*
 * wubu_bus_selftest.c -- verifies kernel-owned I2C/SPI bus routing.
 */
#include "wubu_bus.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("  ok: %s\n", msg); passed++; } \
} while(0)

static int failures = 0;
static int passed = 0;

int main(void)
{
    printf("=== wubu_bus_selftest ===\n\n");

    wubu_hw_detect();
    wubu_bus_probe();

    printf("  i2c=%d(%d) spi=%d(%d)\n",
           wubu_bus_has_i2c(), wubu_bus_i2c_controllers(),
           wubu_bus_has_spi(), wubu_bus_spi_controllers());

    /* I2C controller routing. */
    CHECK(strcmp(wubu_bus_i2c_driver_for("piix4"), "i2c-piix4") == 0,
          "piix4 -> i2c-piix4");
    CHECK(strcmp(wubu_bus_i2c_driver_for("designware"), "i2c-designware") == 0,
          "designware -> i2c-designware");
    CHECK(strcmp(wubu_bus_i2c_driver_for("imx"), "i2c-imx") == 0,
          "imx -> i2c-imx");
    CHECK(strcmp(wubu_bus_i2c_driver_for("bcm2835"), "i2c-bcm2835") == 0,
          "bcm2835 -> i2c-bcm2835");
    CHECK(strcmp(wubu_bus_i2c_driver_for("qcom"), "i2c-qcom-geni") == 0,
          "qcom -> i2c-qcom-geni");
    CHECK(strcmp(wubu_bus_i2c_driver_for("unknown"), "i2c-core") == 0,
          "unknown i2c -> i2c-core");

    /* SPI controller routing. */
    CHECK(strcmp(wubu_bus_spi_driver_for("orion"), "spi-orion") == 0,
          "orion -> spi-orion");
    CHECK(strcmp(wubu_bus_spi_driver_for("imx"), "spi-imx") == 0,
          "imx -> spi-imx");
    CHECK(strcmp(wubu_bus_spi_driver_for("bcm2835"), "spi-bcm2835") == 0,
          "bcm2835 -> spi-bcm2835");
    CHECK(strcmp(wubu_bus_spi_driver_for("tegra"), "spi-tegra") == 0,
          "tegra -> spi-tegra");
    CHECK(strcmp(wubu_bus_spi_driver_for("qcom"), "spi-qcom-qspi") == 0,
          "qcom -> spi-qcom-qspi");
    CHECK(strcmp(wubu_bus_spi_driver_for("unknown"), "spi-core") == 0,
          "unknown spi -> spi-core");

    char s[256];
    wubu_bus_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "bus summary generated");

    printf("\n=== BUS TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
