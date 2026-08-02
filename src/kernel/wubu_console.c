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
    /* heap stats + a FULL canary sweep (gap A6: the red-zone check
     * machinery existed but nothing ran it on metal -- now 'mem'
     * validates every allocation's front/back canaries) */
    extern int mem_validate_all(void);
    int corrupt = mem_validate_all();
    klog_printf("heap used=%u avail=%u canaries=%s\n",
                (unsigned)mem_used(), (unsigned)mem_available(),
                corrupt == 0 ? "OK" : "CORRUPT");
    return 0;
}

static int cmd_tasks(void)
{
    CTask *t = task_list_head();
    int n = 0;
    klog_printf("-- tasks (%d) --\n", task_count());
    while (t) {
        /* stack usage = top - low-water, clamped to the stack range
         * (gap B5/B6). The idle masquerades as the boot main-path, so
         * its saved rsp is the higher-half stack -- show n/a. */
        uint64_t base = (uint64_t)(uintptr_t)t->stack_base;
        uint64_t top  = base + t->stack_size;
        int sane = t->stack_size > 0 &&
                   t->stack_min >= base && t->stack_min <= top;
        uint64_t used = sane ? top - t->stack_min : 0;
        const char *tag = "n/a";
        if (!sane && t->stack_size > 0 && t->stack_min < base)
            tag = "OVER";          /* low-water below the base: the stack
                                    * actually exceeded its allocation */
        klog_printf("  %s id=%d state=%d stk=%s\n",
                    t->name, t->task_id, (int)t->state, tag);
        if (sane) {
            klog_printf("     stack %u%% (%u of %u bytes)\n",
                        (unsigned)(used * 100 / t->stack_size),
                        (unsigned)used, (unsigned)t->stack_size);
        }
        t = t->next;
        if (++n >= 32) break;
    }
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

/* /theme graphic-set namespace: `theme`, `theme set <path> <hex>`,
 * `theme cycle`.  Writing a node re-skins the next Bonzi frame live. */
static int cmd_theme(int argc, char **argv)
{
    extern int  wubu_theme_node_set(const char *, uint32_t);
    extern int  wubu_theme_node_list(char *, int);
    extern uint32_t wubu_theme_write_count(void);
    extern void wubu_theme_apply(void);
    extern void wubu_theme_cycle(void);
    if (argc >= 2 && strcmp(argv[1], "set") == 0 && argc >= 4) {
        char *end = NULL;
        uint32_t v = (uint32_t)strtoul(argv[3], &end, 16);
        if (!end || *end != '\0' || wubu_theme_node_set(argv[2], v) != 0) {
            klog_printf("theme: unknown path or bad value\n");
            return 0;
        }
        wubu_theme_apply();
        klog_printf("theme: %s = %x (writes=%u)\n", argv[2],
                    (unsigned)v, (unsigned)wubu_theme_write_count());
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "cycle") == 0) {
        wubu_theme_cycle();
        klog_printf("theme: cycled\n");
        return 0;
    }
    char buf[2048];
    int n = wubu_theme_node_list(buf, (int)sizeof(buf));
    if (n > 0 && (int)sizeof(buf) > 0) buf[sizeof(buf) - 1] = '\0';
    klog_printf("-- /theme (%d nodes, %u writes) --\n%s", n,
                (unsigned)wubu_theme_write_count(), n > 0 ? buf : "");
    return 0;
}

/* Unified input: `hid` — per-device stats + drain the ring. */
static int cmd_input(int argc, char **argv)
{
    extern uint32_t wubu_hid_stats(uint8_t);
    extern int      wubu_hid_queued(void);
    extern uint32_t wubu_hid_overflow(void);
    (void)argc; (void)argv;
    klog_printf("hid: queued=%d overflow=%u keys=%u mouse=%u gamepad=%u\n",
                wubu_hid_queued(), (unsigned)wubu_hid_overflow(),
                (unsigned)wubu_hid_stats(0),
                (unsigned)wubu_hid_stats(1),
                (unsigned)wubu_hid_stats(2));
    return 0;
}

/* Virtual memory: `vmm` — report + a live demand-fault demo. */
static int cmd_vmm(int argc, char **argv)
{
    extern uint64_t wubu_vmm_free_count(void);
    extern uint32_t wubu_vmm_demand_count(void);
    extern uint32_t wubu_vmm_demand_faults(void);
    extern int      wubu_vmm_alloc_pages(uint64_t, uint32_t);
    extern void     wubu_vmm_free_pages(uint64_t, uint32_t);
    (void)argc; (void)argv;
    uint64_t *demo = (uint64_t *)0xffffffff90000000ull;
    if (argc >= 2 && strcmp(argv[1], "touch") == 0) {
        /* the FIRST touch faults: the #PF handler allocates + maps +
         * retries (demand paging on metal) */
        demo[0] = 0x12345678;
        klog_printf("vmm: demand page touched, readback=%x (faults=%u)\n",
                    (unsigned)demo[0],
                    (unsigned)wubu_vmm_demand_faults());
        return 0;
    }
    klog_printf("vmm: free_pages=%u demand_regions=%u faults=%u\n",
                (unsigned)wubu_vmm_free_count(),
                (unsigned)wubu_vmm_demand_count(),
                (unsigned)wubu_vmm_demand_faults());
    return 0;
}

/* Fault/interrupt statistics: `stats` — the exception counters exposed
 * (gap C11/B12: the soak proved "zero faults" only via external probes). */
static int cmd_stats(int argc, char **argv)
{
    extern uint64_t interrupt_get_count(uint8_t);
    extern uint64_t task_tick_count(void);
    (void)argc; (void)argv;
    /* exceptions (0-19 relevant) + the timer vector */
    klog_printf("stats: tick=%u ex=0:%u 6:%u 8:%u 13:%u 14:%u 18:%u irq32=%u\n",
                (unsigned)task_tick_count(),
                (unsigned)interrupt_get_count(0),
                (unsigned)interrupt_get_count(6),
                (unsigned)interrupt_get_count(8),
                (unsigned)interrupt_get_count(13),
                (unsigned)interrupt_get_count(14),
                (unsigned)interrupt_get_count(18),
                (unsigned)interrupt_get_count(32));
    return 0;
}

/* In-OS hexdump: `dump <addr> [bytes]` — the live debugger (gap F5).
 * addr is a raw 32-bit address; bytes defaults to 64, max 256.
 * klog has NO width/precision support (%02x prints literally), so the
 * line is hand-formatted into a buffer + one plain %s. */
static void dump_hex_byte(char *o, uint8_t v)
{
    static const char *hx = "0123456789abcdef";
    o[0] = hx[v >> 4];
    o[1] = hx[v & 0xF];
}
static void dump_hex_qword(char *o, uint32_t v)
{
    for (int i = 7; i >= 0; i--) {
        static const char *hx = "0123456789abcdef";
        o[i] = hx[v & 0xF];
        v >>= 4;
    }
}
static int cmd_dump(int argc, char **argv)
{
    (void)argc;
    if (argc < 2) {
        klog_printf("dump: usage 'dump <addr> [bytes]'\n");
        return 0;
    }
    uint64_t addr = strtoul(argv[1], NULL, 16);  /* addresses are hex */
    uint32_t n = 64;
    if (argc >= 3) n = (uint32_t)strtoul(argv[2], NULL, 0);
    if (n == 0 || n > 256) n = 64;
    /* fault hardening: a typo'd/unmapped address must NOT #PF-halt the
     * OS -- validate every page against the page tables first */
    extern int wubu_vmm_is_mapped(uint64_t);
    volatile uint8_t *p = (volatile uint8_t *)addr;
    for (uint32_t i = 0; i < n; i += 4096) {
        if (!wubu_vmm_is_mapped((uint64_t)(uintptr_t)(p + i))) {
            klog_printf("dump: %x UNMAPPED page at +%x\n",
                        (unsigned)addr, (unsigned)i);
            return 0;
        }
    }
    char line[80];
    klog_printf("dump: %x (%u bytes)\n", (unsigned)addr, (unsigned)n);
    for (uint32_t i = 0; i < n; i += 16) {
        int o = 0;
        dump_hex_qword(line, (uint32_t)(addr + i));
        line[8] = ' ';
        line[9] = ' ';
        o = 10;
        for (uint32_t j = 0; j < 16 && i + j < n; j++, o += 3) {
            dump_hex_byte(line + o, p[i + j]);
            line[o + 2] = ' ';
        }
        line[o] = '\0';
        klog_printf("%s\n", line);
    }
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
    if (strcmp(argv[0], "theme") == 0)           return cmd_theme(argc, argv);
    if (strcmp(argv[0], "hid") == 0)             return cmd_input(argc, argv);
    if (strcmp(argv[0], "vmm") == 0)             return cmd_vmm(argc, argv);
    if (strcmp(argv[0], "stats") == 0)           return cmd_stats(argc, argv);
    if (strcmp(argv[0], "dump") == 0)            return cmd_dump(argc, argv);
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
        /* RX: interrupt-driven (gap E2) -- the UART IRQ pushes the
         * wubu_sync FIFO; the poll backup drains the UART into the FIFO
         * when the IRQ path is quiet. Each byte is consumed exactly once
         * (the data register is a destructive read, whichever side wins). */
        extern int  wubu_serial_pop(uint8_t *);
        extern void wubu_serial_drain(void);
        wubu_serial_drain();
        uint8_t c;
        if (wubu_serial_pop(&c) == 0) {
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
