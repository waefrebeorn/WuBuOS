/* wubu_console_recovery.c — Recovery console (5+1 rollback substrate)
 *
 * Extracted from wubu_console.c — checkpoint/rollback/jesus/status
 * commands wired to the Colonel's live registers.
 * C11, freestanding.
 */
#include "wubu_console.h"
#include "wubu_serial.h"
#include "wubu_recovery.h"
#include "klog.h"
#include "ahci.h"
#include "fat32.h"
#include <stdint.h>

/* Live Colonel registers (defined in wubu_console_colonel.c) */
extern int64_t live_regs[8];

/* ------------------------------------------------------------------ */
/* The recovery console: the 5+1 rollback substrate wired to the
 * Colonel's live registers. */

static wubu_recovery_t g_recovery;
static int g_recovery_ready = 0;

static void recovery_ensure(void)
{
    if (g_recovery_ready) return;
    wubu_recovery_principles_t p;
    memset(&p, 0, sizeof(p));
    p.version = 1;
    strncpy(p.identity, "wubuwizard-colonel", sizeof(p.identity) - 1);
    p.max_rollback_attempts = WUBU_RECOVERY_SLOTS;
    p.jesus_armed = 1;
    p.human_gate_required = 1;
    p.growth_loop = 1;
    p.human_centric = 1;
    p.no_third_party = 1;
    p.no_stubs = 1;
    p.license_origin = 3;
    wubu_recovery_init(&g_recovery, &p);
    g_recovery_ready = 1;
}

/* `recovery checkpoint|rollback <n>|jesus|status` */
int cmd_recovery(int argc, char **argv)
{
    recovery_ensure();
    if (argc < 2) {
        klog_printf("recovery: checkpoint | rollback <0..4> | jesus | status\n");
        return 0;
    }
    if (strcmp(argv[1], "checkpoint") == 0) {
        int s = wubu_recovery_checkpoint(&g_recovery, live_regs, sizeof(live_regs));
        if (s >= 0) klog_printf("recovery: checkpoint %d (seq %u, live %u)\n",
                                s, (unsigned)g_recovery.seq,
                                (unsigned)wubu_recovery_live(&g_recovery));
        return 0;
    }
    if (strcmp(argv[1], "rollback") == 0 && argc >= 3) {
        int slot = (int)argv[2][0] - '0';
        int64_t out[8];
        int n = wubu_recovery_rollback(&g_recovery, (uint32_t)slot, out, sizeof(out));
        if (n >= 0) {
            for (int k = 0; k < 8; k++) live_regs[k] = out[k];
            klog_printf("recovery: rolled back to slot %d\n", slot);
        } else {
            klog_printf("recovery: slot %d empty or invalid\n", slot);
        }
        return 0;
    }
    if (strcmp(argv[1], "jesus") == 0) {
        int64_t clean[8];
        wubu_recovery_principles_t divine;
        int rc = wubu_recovery_jesus(&g_recovery, clean, sizeof(clean), &divine);
        if (rc == 0) {
            for (int k = 0; k < 8; k++) live_regs[k] = 0;
            klog_printf("recovery: JESUS state -- clean slate, divine good intact "
                        "(identity=%s human_centric=%u)\n",
                        divine.identity, (unsigned)divine.human_centric);
        } else if (rc == -2) {
            klog_printf("recovery: jesus gated (disarmed -- human must re-arm)\n");
        }
        return 0;
    }
    if (strcmp(argv[1], "status") == 0) {
        klog_printf("recovery: healthy=%d live=%u seq=%u rollbacks=%u jesus_used=%u\n",
                    wubu_recovery_healthy(&g_recovery),
                    (unsigned)wubu_recovery_live(&g_recovery),
                    (unsigned)g_recovery.seq,
                    (unsigned)g_recovery.rollback_count,
                    (unsigned)g_recovery.jesus_used);
        return 0;
    }
    return 0;
}

int cmd_cls(void)
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

int cmd_run(int argc, char **argv)
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

int cmd_reboot(void)
{
    klog_printf("WuBuOS: reboot requested\n");
    __asm__ __volatile__("movb $0, %%al\n movw $0xf4, %%dx\n outb %%al, %%dx" ::: "al", "dx");
    for (;;) { }
}

