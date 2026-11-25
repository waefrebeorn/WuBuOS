/*
 * wubu_console_cmds.c -- the ring-0 console BUILT-IN COMMANDS.
 *
 * Extracted from wubu_console.c (the command collection). The dispatcher
 * (wubu_console_exec) + serial task + prompt stay in wubu_console.c.
 * Each cmd_* handler is invoked by the dispatcher's argv table.
 *
 * C11, freestanding.
 */
#include "wubu_console_cmds.h"
#include "wubu_console.h"
#include "wubu_serial.h"
#include "wubu_pci.h"
#include "wubu_agi_kernel.h"
#include "wubu_rtc.h"
#include "wubu_theme.h"
#include "wubu_vmm.h"
#include "vbe.h"        /* deskshot/fbmap: the surface and the scanout seam */
#include <stdlib.h>     /* fbmap's client buffer */
#include "wubu_9pc.h"      /* kv9p: drive the 9P server through the client */
#include "wubu_9psrv.h"
#include "wubu_attest.h"
#include "wubu_hid.h"
#include "interrupt.h"
#include "tasking.h"
#include "memory.h"
#include "klog.h"
#include "libc.h"
#include "fat32.h"
#include "wubu_kvfs.h"
#include "wubu_metal_model.h"
#include "ahci.h"
#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------ */

int cmd_help(void)
{
    klog_printf("WuBuOS live console (ring 0)\n"
                "  help                 this list\n"
                "  uptime               kernel ticks + AGI uptime\n"
                "  mem                  heap used/available\n"
                "  tasks                task table (per-task CPU share)\n"
                "  pci                  PCI scan with device roles\n"
                "  drv                  the one driver registry (bound + MMIO)\n"
                "  theme [set|cycle]    /theme graphic-set namespace\n"
                "  hid                  unified input ring stats\n"
                "  vmm [touch|alloc|free] demand pages + allocator\n"
                "  stats                live exception counters\n"
                "  dump <addr> [n]      in-OS hexdump (mapping-gated)\n"
                "  attest               measured-boot chain + runtime PCR\n"
                "  date                 RTC wall clock\n"
                "  agi status|freeze|unfreeze|promote|trace\n"
                "  holyc <src>          compile+run HolyC (metal port)\n"
                "  live <expr>          Live Colonel: eval + - * / % r0..r7\n"
                "  recovery             checkpoint|rollback <0..4>|jesus|status\n"
                "  cls                  scroll the serial\n"
                "  fs ls|cat|write|rm   user files on the boot disk\n"
                "  run <file>           execute a FAT32 script\n"
                "  reboot               VM reboot (isa-debug-exit)\n");
    return 0;
}

static bool fs_list_cb(const fat32_file_info *info, void *ctx)
{
    (void)ctx;
    if (!info || !info->name[0] || (info->attr & FAT32_ATTR_VOLUME_ID)) return true;
    klog_printf("  %-16s %u bytes\n", info->name,
                (unsigned)info->file_size);
    return true;
}

/* Wave 0: real user-facing file commands on the mounted boot volume.
 * Existing theme save/load proves the FAT32 API works in-kernel; this makes
 * it reachable from the metal console so a human can create/read files. */
int cmd_fs(int argc, char **argv)
{
    int i = 1;
    const char *sub = "";
    if (strcmp(argv[0], "ls") == 0 || strcmp(argv[0], "cat") == 0 ||
        strcmp(argv[0], "write") == 0 || strcmp(argv[0], "rm") == 0) {
        sub = argv[0];
        i = 1;
    } else if (argc >= 2) {
        if (strcmp(argv[1], "ls") == 0 || strcmp(argv[1], "cat") == 0 ||
            strcmp(argv[1], "write") == 0 || strcmp(argv[1], "rm") == 0) {
            sub = argv[1];
            i = 2;
        }
    }
    if (!*sub || i >= argc) {
        klog_printf("fs: usage: fs ls [dir] | cat <file> | write <file> <text> | rm <file>\n");
        return -1;
    }
    fat32_volume *vol = fat32_boot_volume();
    if (!vol) { klog_printf("fs: no boot volume\n"); return -1; }
    if (strcmp(sub, "ls") == 0) {
        int n = fat32_dir_read(vol, 0, fs_list_cb, NULL);
        klog_printf("fs: %d root entries\n", n < 0 ? 0 : n);
        return n < 0 ? -1 : 0;
    }
    const char *name = argv[i];
    if (strcmp(sub, "cat") == 0) {
        fat32_file f;
        if (fat32_open(vol, 0, name, "r", &f) != 0) {
            klog_printf("fs: cannot open %s\n", name); return -1;
        }
        char buf[256];
        size_t total = 0;
        for (;;) {
            size_t got = fat32_read(&f, buf, sizeof(buf) - 1);
            if (!got) break;
            buf[got] = 0;
            klog_printf("%s", buf);
            total += got;
        }
        fat32_close(&f);
        klog_printf("\nfs: %s: %u bytes\n", name, (unsigned)total);
        return 0;
    }
    if (strcmp(sub, "write") == 0) {
        char text[240];
        size_t n = 0;
        for (int a = i + 1; a < argc && n + 1 < sizeof(text); a++) {
            if (n) text[n++] = ' ';
            for (const char *p = argv[a]; *p && n + 1 < sizeof(text); p++)
                text[n++] = *p;
        }
        text[n] = 0;
        fat32_file_info fi;
        (void)fat32_create(vol, 0, name, 0, &fi); /* already-exists is fine */
        fat32_file f;
        if (fat32_open(vol, 0, name, "w", &f) != 0 ||
            fat32_write(&f, text, n) != n) {
            klog_printf("fs: write %s failed\n", name); return -1;
        }
        fat32_close(&f);
        fat32_flush(vol);
        klog_printf("fs: wrote %s (%u bytes)\n", name, (unsigned)n);
        return 0;
    }
    if (strcmp(sub, "rm") == 0) {
        if (fat32_delete(vol, 0, name) != 0) {
            klog_printf("fs: cannot delete %s\n", name); return -1;
        }
        fat32_flush(vol);
        klog_printf("fs: removed %s\n", name);
        return 0;
    }
    return -1;
}

static int kv_parse_int(const char *s)
{
    int v = 0, neg = 0;
    if (!s) return 0;
    if (*s == '-') { neg = 1; s++; }
    for (; *s >= '0' && *s <= '9'; s++) v = v * 10 + (*s - '0');
    return neg ? -v : v;
}

/* The kernel console formatter must not rely on a float->int conversion
 * in a varargs expression (it printed 0 under this build). Convert via
 * IEEE-754 bits so both the file and the tensor stay float-backed. */
static int kv_float_to_int(float f)
{
    uint32_t bits = 0;
    memcpy(&bits, &f, sizeof(bits));
    int neg = (int)(bits >> 31);
    int exp = (int)((bits >> 23) & 0xffu);
    uint32_t mant = bits & 0x7fffffu;
    if (!exp) return 0;
    int shift = exp - 127;
    mant |= 0x800000u;
    if (shift < 0) return 0;
    uint32_t val = shift <= 23 ? (mant >> (23 - shift))
                               : (mant << (shift - 23));
    return neg ? -(int)val : (int)val;
}

/* Wave 3: a tiny ring-0 KV state surface backed by the boot volume.
 * The namespace is real (wubu_kvfs path routing over a float tensor);
 * the compact file makes the state survive a reboot. */
/* proof-of-loop: read a KV path back through the kernel 9P server */
static int kv9p_read(const char *path);

int cmd_kv(int argc, char **argv)
{
    extern int fat32_create(fat32_volume *, uint32_t, const char *,
                            uint8_t, fat32_file_info *);
    extern int fat32_open(fat32_volume *, uint32_t, const char *,
                          const char *, fat32_file *);
    extern size_t fat32_read(fat32_file *, void *, size_t);
    extern size_t fat32_write(fat32_file *, const void *, size_t);
    extern void fat32_close(fat32_file *);
    extern int fat32_flush(fat32_volume *);
    const char *sub = argc > 1 ? argv[1] : "";
    if (!g_wubu_kvfs || !g_wubu_kv_base) {
        klog_printf("kv: namespace not initialized\n"); return -1;
    }
    fat32_volume *vol = fat32_boot_volume();
    if (!vol) { klog_printf("kv: no boot volume\n"); return -1; }
    float v[4];
    if (strcmp(sub, "write") == 0) {
        if (argc < 6) { klog_printf("kv: write n1 n2 n3 n4\n"); return -1; }
        int iv[4];
        for (int i = 0; i < 4; i++) {
            iv[i] = kv_parse_int(argv[i + 2]);
            v[i] = (float)iv[i];
        }
        if (wubu_kvfs_write(g_wubu_kvfs, "/kv/world/tick_0",
                            g_wubu_kv_base, v, 4) != 0) {
            klog_printf("kv: namespace write failed\n"); return -1;
        }
        fat32_file_info fi;
        (void)fat32_create(vol, 0, "KVSTATE.BIN", 0, &fi);
        fat32_file f;
        if (fat32_open(vol, 0, "KVSTATE.BIN", "w", &f) != 0 ||
            fat32_write(&f, v, sizeof(v)) != sizeof(v)) {
            klog_printf("kv: state file write failed\n"); return -1;
        }
        fat32_close(&f);
        fat32_flush(vol);
        klog_printf("kv: wrote %d %d %d %d\n", iv[0], iv[1], iv[2], iv[3]);
        /* and prove the 9P server serves those same bytes back */
        (void)kv9p_read("/kv/world/tick_0");
        return 0;
    }
    if (strcmp(sub, "read") == 0) {
        fat32_file f;
        if (fat32_open(vol, 0, "KVSTATE.BIN", "r", &f) != 0 ||
            fat32_read(&f, v, sizeof(v)) != sizeof(v)) {
            klog_printf("kv: no state file\n"); return -1;
        }
        fat32_close(&f);
        if (wubu_kvfs_write(g_wubu_kvfs, "/kv/world/tick_0",
                            g_wubu_kv_base, v, 4) != 0 ||
            wubu_kvfs_read(g_wubu_kvfs, "/kv/world/tick_0",
                           g_wubu_kv_base, v, 4) != 0) {
            klog_printf("kv: namespace route failed\n"); return -1;
        }
        klog_printf("kv state %d %d %d %d\n", kv_float_to_int(v[0]),
                    kv_float_to_int(v[1]), kv_float_to_int(v[2]),
                    kv_float_to_int(v[3]));
        return 0;
    }
    klog_printf("kv: usage: kv write n1 n2 n3 n4 | kv read\n");
    return -1;
}

/* kv9p -- drive the kernel 9P SERVER through the kernel 9P CLIENT, in-kernel.
 * One codec pair, one namespace: if either side mis-encodes a field the read
 * comes back short or wrong. The bytes are served from the LIVE tensor. */
static uint8_t g_kv9p_in[512];
static uint8_t g_kv9p_out[WUBU_9PSRV_MSIZE];

static int kv9p_tx(const uint8_t *req, uint32_t reqlen, uint8_t *rsp,
                   uint32_t *rsplen, void *ud)
{
    (void)ud;
    return wubu_9psrv_serve(req, reqlen, rsp, WUBU_9PSRV_MSIZE, rsplen) == 0
           ? 0 : -1;
}

static int kv9p_read(const char *path)
{
    if (!g_wubu_kvfs || !g_wubu_kv_base) {
        klog_printf("kv9p: namespace not initialized\n");
        return -1;
    }
    /* split the path into walk components */
    const char *name[8];
    int nname = 0;
    for (const char *p = path; *p && nname < 8; ) {
        while (*p == '/') p++;
        if (!*p) break;
        name[nname++] = p;
        while (*p && *p != '/') p++;
    }
    if (nname == 0) { klog_printf("kv9p: usage: kv9p </kv/path>\n"); return -1; }

    wubu_9psrv_reset();
    w9pc_t c;
    memset(&c, 0, sizeof c);
    c.in = g_kv9p_in;   c.in_cap = (uint32_t)sizeof g_kv9p_in;
    c.out = g_kv9p_out; c.out_cap = (uint32_t)sizeof g_kv9p_out;
    c.tx = kv9p_tx;     c.ud = NULL;

    if (w9pc_version(&c, WUBU_9PSRV_MSIZE, "9P2000") != 0 ||
        w9pc_attach(&c, 1, 1, (uint32_t)~0u, "") != 0) {
        klog_printf("kv9p: version/attach failed\n");
        return -1;
    }
    /* the client takes NUL-terminated names: copy each component out */
    static char names[8][64];
    const char *wn[8];
    for (int i = 0; i < nname; i++) {
        uint32_t k = 0;
        while (name[i][k] && name[i][k] != '/' && k < 63) {
            names[i][k] = name[i][k];
            k++;
        }
        names[i][k] = '\0';
        wn[i] = names[i];
    }
    if (w9pc_walk(&c, 2, 1, 2, wn, nname) != 0) {
        klog_printf("kv9p: walk failed (no such path)\n");
        return -1;
    }
    if (w9pc_open(&c, 3, 2, 0) != 0) {
        klog_printf("kv9p: open failed\n");
        return -1;
    }
    float v[4] = { 0, 0, 0, 0 };
    uint32_t got = 0;
    if (w9pc_read(&c, 4, 2, 0, (uint8_t *)v, sizeof v, &got) != 0) {
        klog_printf("kv9p: read failed\n");
        return -1;
    }
    (void)w9pc_clunk(&c, 5, 2);
    klog_printf("kv9p: %s -> %u bytes, [%d %d %d %d]\n", path, (unsigned)got,
                kv_float_to_int(v[0]), kv_float_to_int(v[1]),
                kv_float_to_int(v[2]), kv_float_to_int(v[3]));
    return 0;
}

int cmd_kv9p(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "/kv/world/tick_0";
    return kv9p_read(path);
}

int cmd_model(int argc, char **argv)
{
    (void)argc; (void)argv;
    extern wubu_metal_model_t *wubu_metal_model_global(void);
    wubu_metal_model_t *m = wubu_metal_model_global();
    if (!m) { klog_printf("model: no ring-0 arena\n"); return -1; }
    float x[16], y[8];
    for (uint32_t i = 0; i < wubu_metal_model_k(m); i++)
        x[i] = 1.0f / (float)(i + 1u);
    if (wubu_metal_model_forward(m, x, y) != 0) {
        klog_printf("model: forward failed\n"); return -1;
    }
    klog_printf("model: f16 gemv %ux%u y0m=%d y1m=%d\n",
                wubu_metal_model_m(m), wubu_metal_model_k(m),
                kv_float_to_int(y[0] * 1000.0f),
                kv_float_to_int(y[1] * 1000.0f));
    return 0;
}

int cmd_uptime(void)
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

int cmd_mem(void)
{
    klog_printf("heap used=%u avail=%u\n",
                (unsigned)mem_used(), (unsigned)mem_available());
    return 0;
}

int cmd_tasks(void)
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

int cmd_pci(void)
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

/* The command body lives in wubu_console_drv.c so it can be gated
 * independently; the declaration stays in wubu_console_cmds.h. */

/* /theme graphic-set namespace: `theme`, `theme set <path> <hex>`,
 * `theme cycle`.  Writing a node re-skins the next Bonzi frame live. */
int cmd_theme(int argc, char **argv)
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
int cmd_input(int argc, char **argv)
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
int cmd_vmm(int argc, char **argv)
{
    extern uint64_t wubu_vmm_free_count(void);
    extern uint32_t wubu_vmm_demand_count(void);
    extern uint32_t wubu_vmm_demand_faults(void);
    extern uint64_t wubu_vmm_alloc_pages(uint32_t);
    extern void     wubu_vmm_free_pages(uint64_t, uint32_t);
    (void)argc; (void)argv;
    uint64_t *demo = (uint64_t *)0xffffffff90000000ull;
    if (argc >= 2 && strcmp(argv[1], "huge") == 0) {
        extern uint64_t wubu_vmm_huge_arena_alloc(uint32_t);
        extern void     wubu_vmm_huge_arena_free(uint64_t, uint32_t);
        extern int      wubu_vmm_huge_arena_owned(uint64_t);
        extern uint32_t wubu_vmm_huge_arena_free_slots(void);
        extern uint64_t wubu_vmm_huge_pde(uint64_t);
        /* `vmm huge [slabs]` -- the same arena the KV tensor uses. */
        uint32_t slabs = 1u;
        if (argc >= 3) {
            uint32_t n = 0; int digits = 0;
            for (const char *s = argv[2]; *s; s++) {
                if (*s < '0' || *s > '9') { digits = 0; break; }
                n = n * 10u + (uint32_t)(*s - '0');
                digits = 1;
            }
            if (digits && n >= 1u && n <= 16u) slabs = n;
        }
        uint32_t before = wubu_vmm_huge_arena_free_slots();
        uint64_t va = wubu_vmm_huge_arena_alloc(slabs);
        if (!va) {
            klog_printf("vmm huge: arena alloc FAILED (slabs=%u free=%u)\n",
                        (unsigned)slabs, (unsigned)before);
            return 0;
        }
        uint64_t pde0 = wubu_vmm_huge_pde(va);
        uint64_t pde1 = slabs > 1 ? wubu_vmm_huge_pde(va + 0x200000ull) : 0;
        uint64_t phys0 = pde0 & 0x000ffffffffff000ull;
        uint64_t phys1 = pde1 & 0x000ffffffffff000ull;
        /* Write through the mapping at every slab boundary + the last float.
         * Plain pointer stores: no absolute high-half `mov` for the guest
         * decoder to trip over. */
        volatile uint64_t *p = (volatile uint64_t *)(uintptr_t)va;
        const uint64_t qwords = (uint64_t)slabs * (0x200000ull / 8ull);
        for (uint32_t s = 0; s < slabs; s++)
            p[(uint64_t)s * (0x200000ull / 8ull)] = 0xa5a50000ull + s;
        p[qwords - 1] = 0x5a5affffull;
        int rb_ok = 1;
        for (uint32_t s = 0; s < slabs; s++)
            if (p[(uint64_t)s * (0x200000ull / 8ull)] != 0xa5a50000ull + s)
                rb_ok = 0;
        if (p[qwords - 1] != 0x5a5affffull) rb_ok = 0;
        klog_printf("vmm huge: slabs=%u va=%x owned=%d ps0=%d phys0=%x "
                    "phys1=%x contig=%d rb=%d free=%u/%u\n",
                    (unsigned)slabs, (unsigned long)va,
                    wubu_vmm_huge_arena_owned(va),
                    (int)((pde0 >> 7) & 1u),
                    (unsigned long)phys0, (unsigned long)phys1,
                    slabs > 1 ? (phys1 == phys0 + 0x200000ull) : 1,
                    rb_ok, (unsigned)wubu_vmm_huge_arena_free_slots(),
                    (unsigned)before);
        wubu_vmm_huge_arena_free(va, slabs);
        klog_printf("vmm huge: released, free slots=%u\n",
                    (unsigned)wubu_vmm_huge_arena_free_slots());
        return 0;
    }
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
int cmd_stats(int argc, char **argv)
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
int cmd_dump(int argc, char **argv)
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
int cmd_attest(int argc, char **argv)
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
int cmd_date(int argc, char **argv)
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

int cmd_agi(int argc, char **argv)
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
        int cr = fat32_create(vol, 0, "AGI.CKP", 0, &fi);
        int op = fat32_open(vol, 0, "AGI.CKP", "w", &f);
        size_t wr = (op == 0) ? fat32_write(&f, &ck, sizeof(ck)) : 0;
        if (cr != 0 || op != 0 || wr != sizeof(ck)) {
            klog_printf("agi: checkpoint save failed (no volume?) cr=%d op=%d wr=%u\n",
                        cr, op, (unsigned)wr);
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

int cmd_holyc(int argc, char **argv)
{
    (void)argc; (void)argv;
    klog_printf("holyc: the hosted compiler port runs in the GUI HolyC term -- "
                "metal-side live coding is the 'live' command (Live Colonel)\n");
    return 0;
}

/* ---- deskshot: pixel truth about the desktop the kernel really drew ----
 * Every render change until now was verified by reading arithmetic, because
 * the QEMU gates boot with -display none and the guest draws into its own
 * Bochs VBE buffer (LFB phys=0xC1000000) rather than the emulated VGA
 * surface -- a screendump there is a black 640x480 (measured). This scans the
 * live back buffer on demand: non-black pixel count, a coarse colour-class
 * count (256 buckets, so it stays O(1) per pixel) and an FNV hash for
 * regression detection. On demand only: a full 1024x768 scan is ~2.4M guest
 * steps, which the boot must not pay. */
int cmd_deskshot(int argc, char **argv)
{
    (void)argc; (void)argv;
    int w = vbe_surface_width();
    int h = vbe_height();
    klog_printf("deskshot: scanning %ux%u\n", (unsigned)w, (unsigned)h);
    if (w <= 0 || h <= 0) {
        klog_printf("deskshot: no surface\n");
        return 1;
    }
    uint32_t nonblack = 0, hash = 2166136261u;
    uint8_t bucket[256];
    for (int i = 0; i < 256; i++) bucket[i] = 0;
    int rows = 0;
    for (int y = 0; y < h; y++) {
        const uint32_t *row = vbe_scanline(y);
        if (!row) break;
        rows++;
        for (int x = 0; x < w; x++) {
            uint32_t c = row[x];
            hash = (hash ^ c) * 16777619u;
            if (c) nonblack++;
            bucket[(uint8_t)((c * 2654435761u) >> 24)] = 1;
        }
    }
    uint32_t colours = 0;
    for (int i = 0; i < 256; i++) colours += bucket[i] ? 1u : 0u;
    klog_printf("deskshot: %ux%u nonblack=%x colours=%x hash=%x\n",
                (unsigned)w, (unsigned)rows, (unsigned)nonblack,
                (unsigned)colours, (unsigned)hash);
    return 0;
}

/* ---- fbmap: the display seam a ring-3 compositor needs ----------------
 * Two things a compositor cannot do today, and the kernel reports exactly what
 * it WROTE (the gate verifies it independently by dumping the physical frame):
 *   1. present a damage rect from a CLIENT buffer (packed stride) into the
 *      padded scanout;
 *   2. reach that scanout through a ring-3 mapping (USER at every table level).
 * Readback is deliberately NOT the proof: an MMIO window discards writes (that
 * is how the NIC-BAR bug hid), so the gate reads the physical bytes instead. */
int cmd_fbmap(int argc, char **argv)
{
    (void)argc; (void)argv;
    VBEState *v = vbe_state();
    if (!v || !v->fb) { klog_printf("fbmap: FAIL no framebuffer\n"); return 1; }
    uint64_t phys = vbe_scanout_phys();
    if (!phys) { klog_printf("fbmap: FAIL no scanout (RAM framebuffer)\n"); return 1; }

    const int dx = 8, dy = 8;          /* damage probe */
    const int ux = 16, uy = 16;        /* ring-3 mapping probe */
    const uint32_t DCOLOUR = 0x00FF00FFu;
    const uint32_t UCOLOUR = 0x00CAFEB0u;

    uint32_t *client = (uint32_t *)malloc((size_t)v->width * (size_t)v->height * 4u);
    if (!client) { klog_printf("fbmap: FAIL no client buffer\n"); return 1; }

    uint32_t *scanout = v->fb;
    vbe_set_render_target(client, v->width, v->height);
    vbe_set_pixel(dx, dy, DCOLOUR);
    vbe_set_render_target(scanout, v->width, v->height);
    vbe_present_from(client, (size_t)v->width * 4u, dx, dy, 1, 1);
    free(client);

    const uint64_t va = 0xfffffffd40000000ull;
    uint64_t bytes = 0;
    int mapped = vbe_map_user(va, &bytes);
    if (mapped == 0) {
        volatile uint32_t *u = (volatile uint32_t *)(uintptr_t)va;
        u[(size_t)uy * (size_t)(v->pitch / 4) + (size_t)ux] = UCOLOUR;
    }
    __asm__ __volatile__("mfence" ::: "memory");

    /* map= is a NUMBER, not a string. A gate run under suite load once reported
     * "map=(null)" while every %x on the same line was correct -- the ternary cannot produce
     * NULL, so either the string pointer or the kernel's vararg handling lost it. A boolean
     * has no business travelling through %s: with 1 = mapped, the report is unambiguous and
     * cannot be corrupted into an unparseable token. The gate accepts both spellings. */
    klog_printf("fbmap: WROTE phys=%x pitch=%u dmg=%u:%u:%x usr=%u:%u:%x map=%u bytes=%x\n",
                (unsigned)phys, (unsigned)v->pitch, (unsigned)dx, (unsigned)dy,
                (unsigned)DCOLOUR, (unsigned)ux, (unsigned)uy, (unsigned)UCOLOUR,
                (unsigned)(mapped == 0 ? 1u : 0u), (unsigned)bytes);
    return mapped == 0 ? 0 : 1;
}
