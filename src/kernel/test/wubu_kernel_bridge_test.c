/*
 * Bridge gate: WuBuOS consumes WuBuKernel's driver model and core services.
 *
 * This is the proof that WuBuOS *uses* the kernel repository rather than merely
 * carrying it as a submodule: a real OS-side device ("com0", the COM1 UART this
 * box boots on) is bound to a driver, driven through suspend/resume, detached,
 * and the kernel heap/console are exercised through their public headers.
 */
#include "wubudriver.h"
#include "wubukcore.h"
#include "wubukernel_pmm.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL %s\n", msg); ++fails; } \
} while (0)

static char captured[2048];
static void capture(const char *line, void *arg)
{
    (void)arg;
    size_t cur = strlen(captured);
    size_t n = strlen(line);
    if (cur + n + 1 < sizeof captured) memcpy(captured + cur, line, n + 1);
}

/* The OS-side UART device and its driver. */
static unsigned char com1_regs[64] __attribute__((aligned(16)));
static wubu_device com0 = {
    .name = "com0",
    .class_id = WUBU_CLASS_UART,
    .hw = com1_regs,
    .irq = 4
};
static wubu_driver uart_drv = {
    .name = "wubu-com1",
    .class_id = WUBU_CLASS_UART,
};

static int com1_probe(wubu_device *dev)
{
    /* A real probe would read the UART's line-status register; here we require
     * the MMIO window we were handed, which is exactly what hardware gives us. */
    return dev->hw ? 0 : -1;
}
static int com1_suspend(wubu_device *dev) { (void)dev; return 0; }
static int com1_resume(wubu_device *dev)  { (void)dev; return 0; }

int main(void)
{
    static unsigned char arena[4096] __attribute__((aligned(16)));
    CHECK(wubu_heap_init(arena, sizeof arena) == 0, "kernel heap initialises");

    wubu_console_set_sink(capture, NULL);
    wubu_klog("bridge online\n");
    CHECK(strstr(captured, "bridge online") != NULL,
          "console reaches the injected sink");

    uart_drv.probe = com1_probe;
    uart_drv.suspend = com1_suspend;
    uart_drv.resume = com1_resume;

    CHECK(wubu_driver_register(&uart_drv) == 0, "OS driver registered");
    CHECK(wubu_device_register(&com0) == 0, "OS com0 device registered");

    CHECK(wubu_bus_bind() == 1, "bus binds com0 to wubu-com1");
    CHECK(com0.state == WUBU_DEV_BOUND && com0.driver == &uart_drv,
          "device reports its bound driver");

    CHECK(wubu_bus_suspend() == 0 && wubu_bus_resume() == 0,
          "suspend/resume fan-out over the bound device");
    CHECK(wubu_device_open(&com0) == 0 && com0.refcount == 1,
          "refcounted open through the kernel API");

    wubu_bus_detach_all();
    CHECK(com0.state == WUBU_DEV_IDLE, "detach returns the device to idle");

    wubu_bus_shutdown();

    /* The third WuBuKernel concern: PMM. The old bridge claim that this
     * was proven here was false (the recipe compiled only heap+driver).
     * Assert exact frames from an independent region, not just "it ran". */
    static uint8_t pmm_bitmap[8];
    static const wubu_pmm_region pmm_region = { .base = 0x100000, .pages = 4 };
    static wubu_pmm pmm;
    CHECK(wubu_pmm_init(&pmm, &pmm_region, 1, pmm_bitmap, sizeof pmm_bitmap) == 0,
          "PMM initialises from a boot region");
    CHECK(wubu_pmm_total(&pmm) == 4 && wubu_pmm_free_count(&pmm) == 4,
          "PMM reports all four frames free");
    uint64_t f0 = wubu_pmm_alloc(&pmm);
    uint64_t f1 = wubu_pmm_alloc(&pmm);
    CHECK(f0 == 0x100000 && f1 == 0x101000,
          "PMM hands out distinct, exact page addresses");
    CHECK(wubu_pmm_is_allocated(&pmm, f0) == 1 &&
          wubu_pmm_is_allocated(&pmm, f1) == 1 &&
          wubu_pmm_is_allocated(&pmm, 0x102000) == 0,
          "PMM allocation bitmap matches reality");
    CHECK(wubu_pmm_alloc(&pmm) == 0x102000 &&
          wubu_pmm_alloc(&pmm) == 0x103000 &&
          wubu_pmm_alloc(&pmm) == 0,
          "PMM exhausts the pool instead of reusing frames");
    wubu_pmm_free(&pmm, f0);
    CHECK(wubu_pmm_is_allocated(&pmm, f0) == 0 && wubu_pmm_free_count(&pmm) == 1,
          "PMM free returns exactly one frame");
    wubu_pmm_free(&pmm, 0x999000); /* foreign frame: must be ignored */
    CHECK(wubu_pmm_free_count(&pmm) == 1,
          "PMM ignores a frame outside its regions");

    if (fails) {
        printf("[kernel_bridge] %d fail\n", fails);
        return 1;
    }
    printf("[kernel_bridge] all pass (WuBuKernel driver model, heap, PMM)\n");
    return 0;
}
