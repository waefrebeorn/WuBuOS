/* wubu_console.c — Ring-0 debug console + CLI dispatch
 *
 * This is the core console. The two large sections extracted into:
 *   wubu_console_colonel.c   — Live Colonel expression evaluator
 *   wubu_console_recovery.c  — Recovery console (rollback substrate)
 * C11, freestanding.
 */
#include "wubu_console.h"
#include "wubu_console_cmds.h"  /* built-in command handlers (cmds.c) */
#include "wubu_serial.h"
#include "wubu_pci.h"
#include "wubu_agi_kernel.h"
#include "wubu_rtc.h"   /* date command (gap A17) */
#include "wubu_recovery.h" /* the 5+1 rollback substrate */
#include "ahci.h"       /* run command (gap F3) */
#include "fat32.h"      /* run command (gap F3) */
#include "tasking.h"
#include "input.h"
#include "ps2.h"
#include "wubu_fbterm.h"
#include "memory.h"
#include "klog.h"
#include "libc.h"
#include <stdint.h>
#include <string.h>


/* Built-in command handlers extracted to wubu_console_cmds.c */

/* ------------------------------------------------------------------ */

/* --- Live Colonel + recovery console extracted to separate files ---
 *   See: wubu_console_colonel.c
 *   See: wubu_console_recovery.c
 */

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
    if (strcmp(argv[0], "drv") == 0)             return cmd_drv();
    if (strcmp(argv[0], "theme") == 0)           return cmd_theme(argc, argv);
    if (strcmp(argv[0], "hid") == 0)             return cmd_input(argc, argv);
    if (strcmp(argv[0], "vmm") == 0)             return cmd_vmm(argc, argv);
    if (strcmp(argv[0], "stats") == 0)           return cmd_stats(argc, argv);
    if (strcmp(argv[0], "dump") == 0)            return cmd_dump(argc, argv);
    if (strcmp(argv[0], "attest") == 0)          return cmd_attest(argc, argv);
    if (strcmp(argv[0], "date") == 0)            return cmd_date(argc, argv);
    if (strcmp(argv[0], "agi") == 0)             return cmd_agi(argc, argv);
    if (strcmp(argv[0], "holyc") == 0)           return cmd_holyc(argc, argv);
    if (strcmp(argv[0], "live") == 0)            return cmd_live(argc, argv);
    if (strcmp(argv[0], "recovery") == 0)        return cmd_recovery(argc, argv);
    if (strcmp(argv[0], "cls") == 0)             return cmd_cls();
    if (strcmp(argv[0], "kv") == 0)                return cmd_kv(argc, argv);
    if (strcmp(argv[0], "kv9p") == 0)              return cmd_kv9p(argc, argv);
    /* Pixel truth for the render gates: the framebuffer the kernel actually
     * drew into, not what an emulator's VGA surface happens to show. */
    if (strcmp(argv[0], "deskshot") == 0)          return cmd_deskshot(argc, argv);
    if (strcmp(argv[0], "model") == 0)             return cmd_model(argc, argv);
    if (strcmp(argv[0], "fs") == 0 ||
        strcmp(argv[0], "ls") == 0 ||
        strcmp(argv[0], "cat") == 0 ||
        strcmp(argv[0], "write") == 0 ||
        strcmp(argv[0], "rm") == 0)             return cmd_fs(argc, argv);
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
    if (strcmp(argv[0], "usb") == 0) {           /* Gap E1: the xHCI driver */
        extern int wubu_xhci_probe(void *);
        extern int wubu_xhci_start(void *);
        struct {
            int present; uint64_t mmio_base, op_base;
            uint32_t cap_length, hcs_params, hcc_params, db_off, rt_off;
            uint32_t port_count, slot_count;
        } x;
        if (wubu_xhci_probe(&x) == 0 && x.present) {
            extern int wubu_xhci_slot_alloc(void *);
            klog_printf("usb: xHCI %u slots %u ports; starting...\n",
                        (unsigned)x.slot_count, (unsigned)x.port_count);
            wubu_xhci_start(&x);
            int slot = wubu_xhci_slot_alloc(&x);
            klog_printf("usb: slot %d allocated (HID transfer path pending)\n", slot);
        } else {
            klog_printf("usb: no xHCI controller on the bus\n");
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

typedef struct {
    char line[256];
    size_t n;
    char history[8][256];
    int hist_n, hist_pos, hist_fill;
} WubuConsoleState;

/* One byte from either COM1 or the PS/2 key path runs through the same
 * line editor: history, tab completion, backspace, ESC arrows. */
static void console_feed_char(WubuConsoleState *st, uint8_t c)
{
    if (c == '\r' || c == '\n') {
        serial_tx('\r'); serial_tx('\n');
        st->line[st->n] = 0;
        if (st->n > 0) {
            if (st->hist_n == 0 ||
                strcmp(st->line, st->history[(st->hist_n - 1 + 8) % 8]) != 0) {
                for (size_t i = 0; i < sizeof(st->line); i++)
                    st->history[st->hist_n][i] = st->line[i];
                st->hist_n = (st->hist_n + 1) % 8;
                if (st->hist_fill < 8) st->hist_fill++;
            }
            wubu_fbterm_submit(st->line);
            wubu_console_exec(st->line);
        }
        st->n = 0;
        st->hist_pos = -1;
        wubu_console_prompt();
    } else if (c == 0x08 || c == 0x7F) {
        if (st->n > 0) {
            st->n--;
            st->line[st->n] = 0;
            wubu_fbterm_current(st->line);
            serial_tx('\b'); serial_tx(' '); serial_tx('\b');
        }
    } else if (c >= 0x20 && c < 0x7F && st->n + 1 < sizeof(st->line)) {
        st->line[st->n++] = (char)c;
        st->line[st->n] = 0;
        wubu_fbterm_current(st->line);
        serial_tx(c);
    } else if (c == '\t') {
        static const char *cmds[] = {
            "help", "uptime", "mem", "tasks", "pci", "drv", "fs", "theme",
            "hid", "vmm", "stats", "dump", "attest", "date", "agi",
            "holyc", "live", "recovery", "cls", "run", "reboot"
        };
        int ncmds = (int)(sizeof(cmds) / sizeof(cmds[0]));
        if (st->n > 0 && st->line[0] != ' ') {
            size_t word_end = st->n;
            if (st->line[word_end - 1] == ' ') word_end--;
            size_t ws = 0;
            while (ws < word_end && st->line[ws] != ' ') ws++;
            if (ws == word_end) {
                const char *match = NULL;
                for (int i = 0; i < ncmds; i++) {
                    if (strncmp(cmds[i], st->line, st->n) == 0) {
                        if (match) { match = NULL; break; }
                        match = cmds[i];
                    }
                }
                if (match) {
                    for (size_t i = st->n; match[i]; i++) {
                        st->line[st->n++] = match[i];
                        serial_tx((uint8_t)match[i]);
                    }
                    serial_tx(' ');
                    st->line[st->n++] = ' ';
                    st->line[st->n] = 0;
                }
            }
        }
    } else if (c == 0x1B) {
        extern int wubu_serial_pop(uint8_t *);
        wubu_serial_pop(&c);                 /* '[' */
        if (wubu_serial_pop(&c) == 0) {
            int back = (c == 'A'), fwd = (c == 'B');
            if ((back || fwd) && st->hist_fill > 0) {
                if (st->hist_pos < 0) st->hist_pos = st->hist_n;
                st->hist_pos = (st->hist_pos + (back ? 7 : 1)) % 8;
                if (st->hist_pos == st->hist_n && st->hist_fill < 8)
                    st->hist_pos = (st->hist_pos + (back ? 1 : 7)) % 8;
                for (size_t i = 0; i < st->n; i++) serial_tx('\b');
                st->n = 0;
                if (st->hist_pos != st->hist_n) {
                    for (size_t i = 0; st->history[st->hist_pos][i] &&
                                       i < sizeof(st->line) - 1; i++) {
                        st->line[st->n++] = st->history[st->hist_pos][i];
                        serial_tx((uint8_t)st->line[st->n - 1]);
                    }
                }
                st->line[st->n] = 0;
            }
        }
    }
}

void wubu_console_task(void *arg)
{
    (void)arg;
    static WubuConsoleState st;
    klog_printf("WuBuOS: live console up (COM1 + PS/2, ring 0)\n");
    wubu_console_prompt();
    for (;;) {
        extern int  wubu_serial_pop(uint8_t *);
        extern void wubu_serial_drain(void);
        extern void klog_tx_poll(void);
        wubu_serial_drain();
        klog_tx_poll();
        ps2_poll_keyboard();   /* feeds input_key_poll() from PS/2 OBF */
        uint8_t c;
        if (wubu_serial_pop(&c) == 0) console_feed_char(&st, c);

        /* PS/2 path: consume key-down events and feed their translated
         * keycode through the SAME editor (history/tab/esc included). */
        KeyEvent ev;
        if (input_key_poll(&ev) == 1 && ev.kind == KEY_EVENT_DOWN) {
            uint8_t k = (uint8_t)ev.keycode;
            if (k) console_feed_char(&st, k);
        }
        task_yield();
    }
}
