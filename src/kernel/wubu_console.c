/*
 * wubu_console.c -- live ring-0 console REPL (TempleOS-style).
 *
 * The metal kernel owns a COM1 interactive shell: poll the UART RX line,
 * echo characters, dispatch whole lines through wubu_console_exec().  The
 * same entry is callable from anywhere in ring 0 -- including the AGI
 * supervisor's agent task -- so the operating system IS a live development
 * environment: commands run here, HolyC will run here, drivers get built
 * here.  Polling-based (no IRQ dependency while the interrupt path is
 * being brought up).
 */
#include "wubu_console.h"
#include "wubu_pci.h"
#include "wubu_agi_kernel.h"
#include "tasking.h"
#include "memory.h"
#include "klog.h"
#include "libc.h"
#include <stdint.h>
#include <string.h>

#define COM1_PORT 0x3F8
#define COM1_LSR  (COM1_PORT + 5)

static inline uint8_t serial_rx_ready(void) {
    return (uint8_t)(inb(COM1_LSR) & 0x01);
}
static inline uint8_t serial_rx(void) {
    return inb(COM1_PORT);
}
static inline void serial_tx(uint8_t c) {
    while ((inb(COM1_LSR) & 0x20) == 0) { /* wait for THR empty */ }
    outb(COM1_PORT, c);
}

/* ------------------------------------------------------------------ */

static int cmd_help(void)
{
    klog_printf("WuBuOS live console (ring 0)\n"
                "  help                 this list\n"
                "  uptime               kernel ticks + AGI uptime\n"
                "  mem                  heap used/available\n"
                "  tasks                task table\n"
                "  pci                  PCI bus 0 device scan\n"
                "  agi status           AGI supervisor state\n"
                "  agi freeze|unfreeze  stop/resume the self-improve loop\n"
                "  agi promote          promote the pending trace (gated)\n"
                "  agi trace            immutable AGI trace tail\n"
                "  holyc <src>          compile+run HolyC (metal port)\n"
                "  cls                  scroll the serial\n"
                "  reboot               VM reboot (isa-debug-exit)\n");
    return 0;
}

static int cmd_uptime(void)
{
    wubu_agi_kernel_t *agi = wubu_agi_kernel_global();
    klog_printf("tick=%u agi_uptime_ms=%u regions=%d promoted=%d\n",
                (unsigned)task_tick_count(),
                (unsigned)wubu_agi_kernel_uptime_ms(agi),
                wubu_agi_kernel_region_count(agi),
                wubu_agi_kernel_promoted_total(agi));
    return 0;
}

static int cmd_mem(void)
{
    klog_printf("heap used=%u avail=%u\n",
                (unsigned)mem_used(), (unsigned)mem_available());
    return 0;
}

static int cmd_tasks(void)
{
    CTask *t = task_list_head();
    int n = 0;
    klog_printf("-- tasks (%d) --\n", task_count());
    for (; t && n < 32; t = t->next, n++)
        klog_printf("  [%d] %s state=%d ticks=%u\n",
                    t->task_id, t->name, (int)t->state,
                    (unsigned)t->total_ticks);
    return 0;
}

static int cmd_pci(void)
{
    wubu_pci_dev_t devs[WUBU_PCI_MAX_DEVS];
    int n = wubu_pci_scan(devs, WUBU_PCI_MAX_DEVS);
    klog_printf("-- PCI bus 0 (%d devices) --\n", n);
    for (int i = 0; i < n; i++)
        klog_printf("  %02x:%02x.%x %04x:%04x class=%02x.%02x.%02x bar0=%x bar1=%x\n",
                    devs[i].bus, devs[i].dev, devs[i].fn,
                    devs[i].vendor, devs[i].device,
                    devs[i].class_code, devs[i].subclass, devs[i].prog_if,
                    (unsigned)devs[i].bar0, (unsigned)devs[i].bar1);
    return 0;
}

static int cmd_agi(int argc, char **argv)
{
    wubu_agi_kernel_t *agi = wubu_agi_kernel_global();
    if (argc < 2) return cmd_help();
    if (strcmp(argv[1], "status") == 0) {
        klog_printf("agi: frozen=%d attest_valid=%d traces=%d promoted=%d uptime_ms=%u\n",
                    wubu_agi_kernel_is_frozen(agi) ? 1 : 0,
                    wubu_agi_kernel_attest_valid(agi) ? 1 : 0,
                    wubu_agi_kernel_trace_count(agi),
                    wubu_agi_kernel_promoted_total(agi),
                    (unsigned)wubu_agi_kernel_uptime_ms(agi));
        return 0;
    }
    if (strcmp(argv[1], "freeze") == 0) {
        wubu_agi_kernel_freeze(agi, true);
        klog_printf("agi: frozen (self-improve loop stopped)\n");
        return 0;
    }
    if (strcmp(argv[1], "unfreeze") == 0) {
        wubu_agi_kernel_freeze(agi, false);
        klog_printf("agi: unfrozen\n");
        return 0;
    }
    if (strcmp(argv[1], "promote") == 0) {
        int r = wubu_agi_kernel_cycle(agi);
        klog_printf("agi: promote cycle rc=%d (requires loop+verifier+attestation)\n", r);
        return 0;
    }
    if (strcmp(argv[1], "trace") == 0) {
        klog_printf("agi: trace has %d spans (see source for span dump)\n",
                    wubu_agi_kernel_trace_count(agi));
        return 0;
    }
    return cmd_help();
}

static int cmd_holyc(int argc, char **argv)
{
    (void)argc; (void)argv;
    klog_printf("holyc: metal compiler port in progress -- see next boot\n");
    return 0;
}

static int cmd_cls(void)
{
    for (int i = 0; i < 8; i++) serial_tx('\n');
    return 0;
}

static int cmd_reboot(void)
{
    klog_printf("WuBuOS: reboot requested\n");
    __asm__ __volatile__("movb $0, %%al\n movw $0xf4, %%dx\n outb %%al, %%dx" ::: "al", "dx");
    for (;;) { }
}

/* Split a line into argv (up to 8 tokens), returns argc. */
static int split_line(char *line, char **argv, int max)
{
    int argc = 0;
    char *p = line;
    while (*p && argc < max) {
        while (*p == ' ' || *p == '\t') *p++ = 0;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
    }
    return argc;
}

int wubu_console_exec(const char *line)
{
    char buf[256];
    char *argv[8];
    if (!line) return -1;
    size_t len = strlen(line);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, line, len);
    buf[len] = 0;
    int argc = split_line(buf, argv, 8);
    if (argc == 0) return 0;
    if (strcmp(argv[0], "help") == 0)            return cmd_help();
    if (strcmp(argv[0], "uptime") == 0)          return cmd_uptime();
    if (strcmp(argv[0], "mem") == 0)             return cmd_mem();
    if (strcmp(argv[0], "tasks") == 0)           return cmd_tasks();
    if (strcmp(argv[0], "pci") == 0)             return cmd_pci();
    if (strcmp(argv[0], "agi") == 0)             return cmd_agi(argc, argv);
    if (strcmp(argv[0], "holyc") == 0)           return cmd_holyc(argc, argv);
    if (strcmp(argv[0], "cls") == 0)             return cmd_cls();
    if (strcmp(argv[0], "reboot") == 0)          return cmd_reboot();
    klog_printf("console: unknown command '%s' (try 'help')\n", argv[0]);
    return -1;
}

void wubu_console_prompt(void)
{
    const char *p = "WuBuOS> ";
    while (*p) serial_tx((uint8_t)*p++);
}

void wubu_console_task(void *arg)
{
    (void)arg;
    static char line[256];
    size_t n = 0;
    klog_printf("WuBuOS: live console up (COM1, ring 0)\n");
    wubu_console_prompt();
    for (;;) {
        if (serial_rx_ready()) {
            uint8_t c = serial_rx();
            if (c == '\r' || c == '\n') {
                serial_tx('\r'); serial_tx('\n');
                line[n] = 0;
                if (n > 0) wubu_console_exec(line);
                n = 0;
                wubu_console_prompt();
            } else if (c == 0x08 || c == 0x7F) {
                if (n > 0) { n--; serial_tx('\b'); serial_tx(' '); serial_tx('\b'); }
            } else if (c >= 0x20 && c < 0x7F && n + 1 < sizeof(line)) {
                line[n++] = (char)c;
                serial_tx(c);
            }
        }
        task_yield();
    }
}
