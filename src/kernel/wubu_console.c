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
#include "wubu_rtc.h"   /* date command (gap A17) */
#include "ahci.h"       /* run command (gap F3) */
#include "fat32.h"      /* run command (gap F3) */
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
/* Returns 0 ALWAYS (the drop is a silent success): the callers must
 * NEVER retry a dropped char -- an unbounded retry under backpressure
 * is a spin (the trace showed `call tx; test eax; js` looping forever). */
static inline int serial_tx(uint8_t c) {
    /* BOUNDED TX wait (the tick-12/33/153 freeze root cause): when a
     * slow/no reader backs up the serial socket, the UART's THR-empty
     * stops and the OLD unbounded wait spun the CPU FOREVER (the kernel
     * appeared frozen with the serial-register state). The serial is a
     * DEBUG channel -- the kernel must never block on it: wait a bounded
     * number of polls, then DROP the character. */
    for (int i = 0; i < 65536; i++) {
        if (inb(COM1_LSR) & 0x20) { outb(COM1_PORT, c); return 0; }
    }
    /* timeout: the char is dropped; the kernel continues */
    return 0;
}

/* ------------------------------------------------------------------ */

static int cmd_help(void)
{
    klog_printf("WuBuOS live console (ring 0)\n"
                "  help                 this list\n"
                "  uptime               kernel ticks + AGI uptime\n"
                "  mem                  heap used/available\n"
                "  tasks                task table (per-task CPU share)\n"
                "  pci                  PCI scan with device roles\n"
                "  theme [set|cycle]    /theme graphic-set namespace\n"
                "  hid                  unified input ring stats\n"
                "  vmm [touch|alloc|free] demand pages + allocator\n"
                "  stats                live exception counters\n"
                "  dump <addr> [n]      in-OS hexdump (mapping-gated)\n"
                "  attest               measured-boot chain + runtime PCR\n"
                "  date                 RTC wall clock\n"
                "  agi status|freeze|unfreeze|promote|trace\n"
                "  holyc <src>          compile+run HolyC (metal port)\n"
                "  cls                  scroll the serial\n"
                "  run <file>           execute a FAT32 script\n"
                "  reboot               VM reboot (isa-debug-exit)\n");
    return 0;
}

static int cmd_uptime(void)
{
    wubu_agi_kernel_t *agi = wubu_agi_kernel_global();
    /* Gap G10: the AGI sees the kernel's fault state (exceptions 0..31,
     * spurious, overruns). */
    extern uint64_t interrupt_exception_count(uint8_t);
    extern uint32_t interrupt_isr_overruns(void);
    uint64_t faults = 0;
    for (int v = 0; v < 32; v++) faults += interrupt_exception_count((uint8_t)v);
    klog_printf("tick=%u agi_uptime_ms=%u regions=%d promoted=%d faults=%u spurious=%u overruns=%u\n",
                (unsigned)task_tick_count(),
                (unsigned)wubu_agi_kernel_uptime_ms(agi),
                wubu_agi_kernel_region_count(agi),
                wubu_agi_kernel_promoted_total(agi),
                (unsigned)faults,
                (unsigned)interrupt_exception_count(0xFF),
                (unsigned)interrupt_isr_overruns());
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
    extern uint64_t task_tick_count(void);
    uint64_t total = task_tick_count();
    for (; t && n < 32; t = t->next, n++) {
        /* Gap D5: per-task CPU accounting -- the share of all ticks
         * this task has consumed. */
        uint32_t pct = 0;
        if (total > 0) pct = (uint32_t)((t->total_ticks * 100) / total);
        klog_printf("  [%d] %s state=%d ticks=%u cpu=%u%%\n",
                    t->task_id, t->name, (int)t->state,
                    (unsigned)t->total_ticks, (unsigned)pct);
    }
    return 0;
}

/* Gap E4: PCI class -> human role label. */
static const char *pci_role(uint8_t cls, uint8_t sub)
{
    switch (cls) {
        case 0x01: return "storage";          /* 01: mass storage */
        case 0x02: return "network";          /* 02: network */
        case 0x03: return "display";          /* 03: display */
        case 0x04: return "multimedia";
        case 0x05: return "memory";
        case 0x06: return "bridge";
        case 0x07: return "comm";
        case 0x08: return "system";           /* 08: generic system */
        case 0x0C: return sub == 0x03 ? "usb" : "serial-bus";
        case 0x0D: return "wireless";
        case 0x11: return "signal";
        case 0x12: return "coprocessor";
        default:   return "other";
    }
}

static int cmd_pci(void)
{
    wubu_pci_dev_t devs[WUBU_PCI_MAX_DEVS];
    int n = wubu_pci_scan(devs, WUBU_PCI_MAX_DEVS);
    if (n < 0) {                     /* Gap A20: report the scan failure */
        klog_printf("pci: scan FAILED (rc=%d)\n", n);
        return 0;
    }
    klog_printf("-- PCI bus 0 (%d devices) --\n", n);
    for (int i = 0; i < n; i++)
        klog_printf("  %02x:%02x.%x %04x:%04x %s (%02x.%02x.%02x) bar0=%x bar1=%x\n",
                    devs[i].bus, devs[i].dev, devs[i].fn,
                    devs[i].vendor, devs[i].device,
                    pci_role(devs[i].class_code, devs[i].subclass),
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
    if (argc >= 2 && strcmp(argv[1], "save") == 0) {
        /* Gap G4: persist the nodes to THEME.FX on the FAT32 volume
         * (the same lazy mount the run command uses). */
        extern fat32_volume *fat32_boot_volume(void);
        fat32_volume *vol = fat32_boot_volume();
        char buf[2048];
        int n = wubu_theme_node_list(buf, (int)sizeof(buf));
        if (n <= 0) { klog_printf("theme: nothing to save\n"); return 0; }
        buf[sizeof(buf) - 1] = '\0';
        extern int fat32_create(fat32_volume *, uint32_t, const char *,
                                uint8_t, fat32_file_info *);
        extern int fat32_open(fat32_volume *, uint32_t, const char *,
                              const char *, fat32_file *);
        extern size_t fat32_write(fat32_file *, const void *, size_t);
        extern void fat32_close(fat32_file *);
        extern int fat32_flush(fat32_volume *);
        fat32_file_info fi;
        fat32_file f;
        size_t len = 0;
        for (const char *p = buf; *p; p++) len++;
        if (fat32_create(vol, 0, "THEME.FX", 0, &fi) != 0 ||
            fat32_open(vol, 0, "THEME.FX", "w", &f) != 0 ||
            fat32_write(&f, buf, len) != 0) {
            klog_printf("theme: save failed (no volume?)\n");
            return 0;
        }
        fat32_close(&f);
        fat32_flush(vol);
        klog_printf("theme: saved %d nodes to THEME.FX\n", n);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "load") == 0) {
        /* Gap G4: reload THEME.FX: each 'name=value' line re-applies. */
        extern fat32_volume *fat32_boot_volume(void);
        fat32_volume *vol = fat32_boot_volume();
        extern int fat32_find(fat32_volume *, uint32_t, const char *,
                              fat32_file_info *);
        extern int fat32_open(fat32_volume *, uint32_t, const char *,
                              const char *, fat32_file *);
        extern size_t fat32_read(fat32_file *, void *, size_t);
        extern void fat32_close(fat32_file *);
        fat32_file_info fi;
        fat32_file f;
        if (fat32_find(vol, 0, "THEME.FX", &fi) != 0 ||
            fat32_open(vol, 0, "THEME.FX", "r", &f) != 0) {
            klog_printf("theme: no THEME.FX\n");
            return 0;
        }
        char buf[2048];
        size_t rd = fat32_read(&f, buf, sizeof(buf) - 1);
        fat32_close(&f);
        if (rd == 0) { klog_printf("theme: THEME.FX empty\n"); return 0; }
        buf[rd] = '\0';
        int loaded = 0;
        char *line = buf;
        while (*line) {
            char *nl = line;
            while (*nl && *nl != '\n' && *nl != '\r') nl++;
            char save = *nl;
            *nl = '\0';
            char *eq = line;
            while (*eq && *eq != '=') eq++;
            if (*eq == '=' && eq != line) {
                *eq = '\0';
                char *end = NULL;
                uint32_t v = (uint32_t)strtoul(eq + 1, &end, 16);
                if (end && *end == '\0' && wubu_theme_node_set(line, v) == 0)
                    loaded++;
            }
            if (!save) break;
            line = nl + 1;
        }
        if (loaded > 0) wubu_theme_apply();
        klog_printf("theme: loaded %d nodes from THEME.FX\n", loaded);
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
    extern uint64_t wubu_vmm_alloc_pages(uint32_t);
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
    if (argc >= 2 && strcmp(argv[1], "alloc") == 0 && argc >= 3) {
        /* Gap A20: report the allocator's failure instead of silently
         * returning 0. (The signature is alloc_pages(n) -- the phys is
         * chosen by the allocator.) */
        uint32_t np = (uint32_t)strtoul(argv[2], NULL, 10);
        uint64_t p = wubu_vmm_alloc_pages(np);
        klog_printf("vmm: alloc %u pages -> %s (%x)\n", np,
                    p ? "ok" : "FAILED", (unsigned)p);
        if (p) wubu_vmm_free_pages(p, np);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "free") == 0 && argc >= 3) {
        /* `vmm free <hex-phys> [n]` -- release a previously allocated
         * physical range (the refcounted path, gap B8). */
        char *end = NULL;
        uint64_t phys = strtoul(argv[2], &end, 16);
        uint32_t np = (argc >= 4) ? (uint32_t)strtoul(argv[3], NULL, 10) : 1;
        if (!end || *end != '\0' || phys < 0x1000000ull) {
            klog_printf("vmm: bad phys (want hex, >= 16MB)\n");
            return 0;
        }
        wubu_vmm_free_pages(phys, np);
        klog_printf("vmm: released %u pages @%x (free=%u)\n", np,
                    (unsigned)phys, (unsigned)wubu_vmm_free_count());
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
    volatile uint8_t *p = (volatile uint8_t *)addr;
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

/* attest: the measured-boot chain + the runtime PCR (gap A10). */
static int cmd_attest(int argc, char **argv)
{
    (void)argc; (void)argv;
    extern int  wubu_attest_runtime_pcr(uint8_t out[32]);
    extern int  wubu_attest_pcr(unsigned, uint8_t[32]);
    extern int  wubu_attest_kernel_digest(uint8_t[32]);
    uint8_t p[32];
    char line[80];
    int o;

    klog_printf("attest: measured-boot chain + runtime PCR\n");
    if (wubu_attest_pcr(0, p) == 0) {
        o = 0;
        for (int i = 0; i < 32; i++, o += 2)
            dump_hex_byte(line + o, p[i]);
        line[o] = '\0';
        klog_printf("  pcr0  = %s\n", line);
    }
    if (wubu_attest_kernel_digest(p) == 0) {
        o = 0;
        for (int i = 0; i < 32; i++, o += 2)
            dump_hex_byte(line + o, p[i]);
        line[o] = '\0';
        klog_printf("  ksha  = %s (kernel measured by WuBuFW)\n", line);
    }
    if (wubu_attest_runtime_pcr(p) == 0) {
        o = 0;
        for (int i = 0; i < 32; i++, o += 2)
            dump_hex_byte(line + o, p[i]);
        line[o] = '\0';
        klog_printf("  rtPCR = %s (runtime, chained per promotion)\n", line);
    }
    return 0;
}

/* date: the CMOS RTC wall clock (gap A17). */
static int cmd_date(int argc, char **argv)
{
    extern int wubu_rtc_read(wubu_rtc_tm *);
    wubu_rtc_tm tm;
    (void)argc; (void)argv;
    if (wubu_rtc_read(&tm) != 0) {
        klog_printf("date: RTC not available\n");
        return 0;
    }
    /* klog has NO width/precision (%04u prints literally): hand-format
     * the date into a buffer + one plain %s. */
    {
        static const char *hz = "0123456789";
        char line[32];
        int o = 0;
        unsigned vals[6] = { tm.year, tm.mon, tm.day,
                             tm.hour, tm.min, tm.sec };
        for (int v = 0; v < 6; v++) {
            if (v == 1 || v == 2) line[o++] = '-';
            if (v == 3) line[o++] = ' ';
            if (v == 4 || v == 5) line[o++] = ':';
            /* zero-pad: year is 4 digits; the rest are 2 */
            int digits = (v == 0) ? 4 : 2;
            unsigned val = vals[v];
            for (int d = digits - 1; d >= 0; d--) {
                unsigned pow10 = 1;
                for (int k = 0; k < d; k++) pow10 *= 10;
                line[o++] = hz[val / pow10 % 10];
            }
        }
        line[o] = '\0';
        klog_printf("date: %s\n", line);
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
    if (strcmp(argv[1], "checkpoint") == 0) {   /* Gap G6 */
        /* persist the continuity state to AGI.CKP on the FAT32 volume */
        extern int wubu_agi_kernel_checkpoint(const wubu_agi_kernel_t *,
                                              wubu_agi_ckp_t *);
        extern fat32_volume *fat32_boot_volume(void);
        extern int fat32_create(fat32_volume *, uint32_t, const char *,
                                uint8_t, fat32_file_info *);
        extern int fat32_open(fat32_volume *, uint32_t, const char *,
                              const char *, fat32_file *);
        extern size_t fat32_write(fat32_file *, const void *, size_t);
        extern void fat32_close(fat32_file *);
        extern int fat32_flush(fat32_volume *);
        wubu_agi_ckp_t ck;
        if (wubu_agi_kernel_checkpoint(agi, &ck) != 0) {
            klog_printf("agi: checkpoint failed\n");
            return 0;
        }
        fat32_volume *vol = fat32_boot_volume();
        fat32_file_info fi;
        fat32_file f;
        if (fat32_create(vol, 0, "AGI.CKP", 0, &fi) != 0 ||
            fat32_open(vol, 0, "AGI.CKP", "w", &f) != 0 ||
            fat32_write(&f, &ck, sizeof(ck)) != sizeof(ck)) {
            klog_printf("agi: checkpoint save failed (no volume?)\n");
            return 0;
        }
        fat32_close(&f);
        fat32_flush(vol);
        klog_printf("agi: checkpoint saved (promoted=%d)\n",
                    wubu_agi_kernel_promoted_total(agi));
        return 0;
    }
    if (strcmp(argv[1], "restore") == 0) {      /* Gap G6 */
        extern int wubu_agi_kernel_restore(wubu_agi_kernel_t *,
                                           const wubu_agi_ckp_t *);
        extern fat32_volume *fat32_boot_volume(void);
        extern int fat32_find(fat32_volume *, uint32_t, const char *,
                              fat32_file_info *);
        extern int fat32_open(fat32_volume *, uint32_t, const char *,
                              const char *, fat32_file *);
        extern size_t fat32_read(fat32_file *, void *, size_t);
        extern void fat32_close(fat32_file *);
        fat32_volume *vol = fat32_boot_volume();
        fat32_file_info fi;
        fat32_file f;
        wubu_agi_ckp_t ck;
        if (fat32_find(vol, 0, "AGI.CKP", &fi) != 0 ||
            fat32_open(vol, 0, "AGI.CKP", "r", &f) != 0 ||
            fat32_read(&f, &ck, sizeof(ck)) != sizeof(ck)) {
            klog_printf("agi: no checkpoint to restore\n");
            return 0;
        }
        fat32_close(&f);
        if (wubu_agi_kernel_restore(agi, &ck) != 0) {
            klog_printf("agi: checkpoint invalid\n");
            return 0;
        }
        klog_printf("agi: checkpoint restored (promoted=%d)\n",
                    wubu_agi_kernel_promoted_total(agi));
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

/* Gap F3: `run <file>` -- execute the lines of a FAT32 file as console
 * commands. The volume is mounted lazily over the AHCI port-0 sim disk
 * (the hosted/metal disk adapter: ahci_read/ahci_write). */
static int ahci_blk_read(void *ctx, uint64_t lba, uint32_t n, void *buf)
{
    extern int ahci_read(ahci_hba_t *, int, uint64_t, uint32_t, void *);
    return (ahci_read((ahci_hba_t *)ctx, 0, lba, n, buf) == (int)n) ? 0 : -1;
}
static int ahci_blk_write(void *ctx, uint64_t lba, uint32_t n, const void *buf)
{
    extern int ahci_write(ahci_hba_t *, int, uint64_t, uint32_t, const void *);
    return (ahci_write((ahci_hba_t *)ctx, 0, lba, n, buf) == (int)n) ? 0 : -1;
}

static int cmd_run(int argc, char **argv)
{
    extern int  wubu_console_exec(const char *);
    extern fat32_volume *fat32_boot_volume(void);
    if (argc < 2) { klog_printf("run: usage 'run <file>'\n"); return 0; }

    fat32_volume *g_vol = fat32_boot_volume();
    static int g_mounted = 0;
    static ahci_hba_t  g_hba;
    if (!g_mounted) {
        extern int  ahci_hba_init(ahci_hba_t *);
        extern int  ahci_enumerate_ports(ahci_hba_t *);
        extern int  ahci_port_init(ahci_hba_t *, int);
        extern int  ahci_sim_disk_create(ahci_hba_t *, int, int);
        if (ahci_hba_init(&g_hba) != 0 || ahci_enumerate_ports(&g_hba) <= 0 ||
            ahci_port_init(&g_hba, 0) != 0 ||
            ahci_sim_disk_create(&g_hba, 0, 8) != 0) {
            klog_printf("run: disk unavailable\n");
            return 0;
        }
        fat32_blk_ops ops = {
            .read = ahci_blk_read, .write = ahci_blk_write,
            .ctx = &g_hba, .n_sectors = 8 * 1024 * 1024 / 512
        };
        extern int fat32_mount(fat32_volume *, const fat32_blk_ops *);
        if (fat32_mount(g_vol, &ops) != 0) {
            klog_printf("run: no FAT32 volume\n");
            return 0;
        }
        g_mounted = 1;
    }

    fat32_file_info fi;
    if (fat32_find(g_vol, 0, argv[1], &fi) != 0) {
        klog_printf("run: '%s' not found\n", argv[1]);
        return 0;
    }
    fat32_file fp;
    if (fat32_open(g_vol, 0, argv[1], "r", &fp) != 0) {
        klog_printf("run: cannot open '%s'\n", argv[1]);
        return 0;
    }
    char buf[1024];
    size_t rd = fat32_read(&fp, buf, sizeof(buf) - 1);
    if (rd == 0) { klog_printf("run: '%s' empty\n", argv[1]); return 0; }
    buf[rd] = '\0';
    /* execute line by line (the exec re-splits) */
    char *line = buf;
    int nrun = 0;
    while (*line) {
        char *nl = line;
        while (*nl && *nl != '\n' && *nl != '\r') nl++;
        char save = *nl;
        *nl = '\0';
        if (*line && *line != '#') {
            char copy[256];
            strncpy(copy, line, sizeof(copy) - 1);
            copy[sizeof(copy) - 1] = '\0';
            wubu_console_exec(copy);
            nrun++;
        }
        if (!save) break;
        line = nl + 1;
    }
    klog_printf("run: %s: %d commands executed\n", argv[1], nrun);
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
    if (strcmp(argv[0], "attest") == 0)          return cmd_attest(argc, argv);
    if (strcmp(argv[0], "date") == 0)            return cmd_date(argc, argv);
    if (strcmp(argv[0], "agi") == 0)             return cmd_agi(argc, argv);
    if (strcmp(argv[0], "holyc") == 0)           return cmd_holyc(argc, argv);
    if (strcmp(argv[0], "cls") == 0)             return cmd_cls();
    if (strcmp(argv[0], "run") == 0)             return cmd_run(argc, argv);
    if (strcmp(argv[0], "syscalls") == 0) {      /* Gap H1/H3 */
        extern const char *syscall_name(uint32_t);
        extern uint64_t syscall_call_count(uint32_t);
        klog_printf("-- syscall table (registered + audited) --\n");
        int shown = 0;
        for (uint32_t n = 0; n < 512; n++) {
            const char *nm = syscall_name(n);
            uint64_t c = syscall_call_count(n);
            if (nm || c > 0) {
                klog_printf("  %u %s calls=%u\n", (unsigned)n,
                            nm ? nm : "(anon)", (unsigned)c);
                shown++;
            }
        }
        if (shown == 0) klog_printf("  (no syscalls registered)\n");
        return 0;
    }
    if (strcmp(argv[0], "reboot") == 0)          return cmd_reboot();
    if (strcmp(argv[0], "crash") == 0) {         /* Gap A8: force a dump */
        extern void interrupt_panic_dump(void);
        klog_printf("crash: forcing the panic dump (A7 ring -> serial + disk)\n");
        interrupt_panic_dump();
        return 0;
    }
    if (strcmp(argv[0], "user") == 0) {          /* Gap H4: ring-3 boundary */
        extern void wubu_user_enter(uint64_t, uint64_t);
        extern void wubu_user_selftest(void);
        klog_printf("user: iretq to ring 3 (syscall roundtrip)...\n");
        wubu_user_enter((uint64_t)(uintptr_t)wubu_user_selftest,
                        0xffffffff9fff0000ull);
        return 0;   /* unreachable -- the user code loops */
    }
    if (strcmp(argv[0], "iommu") == 0) {         /* Gap E5: the VT-d plane */
        extern int wubu_iommu_probe(void *);
        struct {
            int found; uint32_t cap, ecap; uint64_t rtaddr;
            uint16_t segment; uint8_t version, flags;
        } io;
        if (wubu_iommu_probe(&io) == 0 && io.found) {
            klog_printf("iommu: DMAR v%u cap=%x ecap=%x seg=%u\n",
                        (unsigned)io.version, (unsigned)io.cap,
                        (unsigned)io.ecap, (unsigned)io.segment);
        } else {
            klog_printf("iommu: no DMAR table\n");
        }
        return 0;
    }
    if (strcmp(argv[0], "smp") == 0) {           /* Gap I2: AP bring-up */
        extern uint32_t wubu_smp_start_aps(void);
        extern uint32_t wubu_smp_cpu_count(void);
        klog_printf("smp: INIT-SIPI-SIPI...\n");
        uint32_t aps = wubu_smp_start_aps();
        klog_printf("smp: %u APs alive (total %u CPUs)\n",
                    (unsigned)aps, (unsigned)wubu_smp_cpu_count());
        return 0;
    }
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
    /* command history (gap F1): ring of the last 8 lines; Up/Down walk
     * it. The 16550 RX is byte-stream, so arrow keys are ESC [ A / B. */
    static char history[8][256];
    static int  hist_n, hist_pos = -1, hist_fill;
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
        extern void klog_tx_poll(void);
        wubu_serial_drain();
        klog_tx_poll();   /* gap E3: flush the serial TX ring */
        uint8_t c;
        if (wubu_serial_pop(&c) == 0) {
            if (c == '\r' || c == '\n') {
                serial_tx('\r'); serial_tx('\n');
                line[n] = 0;
                if (n > 0) {
                    /* history push (dedupe the immediate repeat) */
                    if (hist_n == 0 ||
                        strcmp(line, history[(hist_n - 1 + 8) % 8]) != 0) {
                        for (int i = 0; i < 256; i++)
                            history[hist_n][i] = line[i];
                        hist_n = (hist_n + 1) % 8;
                        if (hist_fill < 8) hist_fill++;
                    }
                    wubu_console_exec(line);
                }
                n = 0;
                hist_pos = -1;
                wubu_console_prompt();
            } else if (c == 0x08 || c == 0x7F) {
                if (n > 0) { n--; serial_tx('\b'); serial_tx(' '); serial_tx('\b'); }
            } else if (c >= 0x20 && c < 0x7F && n + 1 < sizeof(line)) {
                line[n++] = (char)c;
                serial_tx(c);
            } else if (c == '\t') {
                /* Gap F2: tab completion -- complete the current word
                 * against the known commands (first word only). */
                static const char *cmds[] = {
                    "help", "uptime", "mem", "tasks", "pci", "theme",
                    "hid", "vmm", "stats", "dump", "attest", "date",
                    "agi", "holyc", "cls", "reboot"
                };
                int ncmds = (int)(sizeof(cmds) / sizeof(cmds[0]));
                if (n > 0 && line[0] != ' ') {
                    int word_end = n;
                    if (line[word_end - 1] == ' ') { word_end--; }
                    int ws = 0;
                    while (ws < word_end && line[ws] != ' ') ws++;
                    if (ws == word_end) {        /* single word: a command */
                        const char *match = NULL;
                        for (int i = 0; i < ncmds; i++) {
                            if (strncmp(cmds[i], line, (size_t)n) == 0) {
                                if (match) { match = NULL; break; } /* ambiguous */
                                match = cmds[i];
                            }
                        }
                        if (match) {
                            for (size_t i = (size_t)n; match[i]; i++) {
                                line[n++] = match[i];
                                serial_tx((uint8_t)match[i]);
                            }
                            serial_tx(' ');
                            line[n++] = ' ';
                            line[n] = 0;
                        }
                    }
                }
            } else if (c == 0x1B) {
                /* ESC sequence: Up/Down arrow = ESC [ A / B (gap F1) */
                wubu_serial_pop(&c);          /* '[' */
                if (wubu_serial_pop(&c) == 0) {
                    int back = (c == 'A'), fwd = (c == 'B');
                    if ((back || fwd) && hist_fill > 0) {
                        if (hist_pos < 0) hist_pos = hist_n;
                        hist_pos = (hist_pos + (back ? 7 : 1)) % 8;
                        if (hist_pos == hist_n && hist_fill < 8)
                            hist_pos = (hist_pos + (back ? 1 : 7)) % 8;
                        for (size_t i = 0; i < n; i++)
                            serial_tx('\b');
                        n = 0;
                        if (hist_pos != hist_n) {
                            for (size_t i = 0; history[hist_pos][i] &&
                                           i < sizeof(line) - 1; i++) {
                                line[n++] = history[hist_pos][i];
                                serial_tx((uint8_t)line[n - 1]);
                            }
                        }
                        line[n] = 0;
                    }
                }
            }
        }
        task_yield();
    }
}
