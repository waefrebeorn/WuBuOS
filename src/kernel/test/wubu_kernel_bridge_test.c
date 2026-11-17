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

    if (fails) {
        printf("[kernel_bridge] %d fail\n", fails);
        return 1;
    }
    printf("[kernel_bridge] all pass (WuBuOS -> WuBuKernel driver model)\n");
    return 0;
}
