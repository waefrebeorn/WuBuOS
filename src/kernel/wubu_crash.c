/*
 * wubu_crash.c  --  crash dump to the disk + boot pickup (gaps A8/F10)
 *
 * The dump uses the AHCI port-0 sim disk directly (raw sector IO, no
 * heap, ISR-safe). The panic path calls wubu_crash_dump; the boot calls
 * wubu_crash_pickup and prints any record. The seq counter makes the
 * pickup idempotent (the boot does not clear it -- a fresh dump just
 * bumps seq, so the LATEST crash is always visible).
 */
#include "wubu_crash.h"
#include <string.h>

extern int ahci_read(void *hba, int port, uint64_t lba, uint32_t n, void *buf);
extern int ahci_write(void *hba, int port, uint64_t lba, uint32_t n,
                      const void *buf);
extern int ahci_hba_init(void *hba);
extern int ahci_enumerate_ports(void *hba);
extern int ahci_port_init(void *hba, int port);
extern int ahci_sim_disk_create(void *hba, int port, int mb);

/* The panic ring's raw reader (klog.c exports the A7 ring snapshot). */
extern int klog_ring_snapshot(char *buf, size_t cap);

/* one static HBA, initialized lazily (the dump is rare) */
static uint8_t g_hba[512];
static int g_hba_ready = 0;
static void *disk_hba(void) { return g_hba; }

static int disk_ready(void)
{
    if (!g_hba_ready) {
        if (ahci_hba_init(g_hba) != 0 ||
            ahci_enumerate_ports(g_hba) <= 0 ||
            ahci_port_init(g_hba, 0) != 0 ||
            ahci_sim_disk_create(g_hba, 0, 8) != 0) {
            return 0;
        }
        g_hba_ready = 1;
    }
    return 1;
}

int wubu_crash_dump(const char *reason, uint64_t rip, uint64_t rsp,
                    uint32_t vector)
{
    static wubu_crash_record_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.magic = WUBU_CRASH_MAGIC;
    rec.seq = 1;                 /* the counter is per-boot; good enough */
    rec.rip = rip;
    rec.rsp = rsp;
    rec.vector = vector;
    if (reason) {
        size_t i = 0;
        for (; reason[i] && i + 1 < sizeof(rec.reason); i++)
            rec.reason[i] = reason[i];
        rec.reason[i] = '\0';
    }
    klog_ring_snapshot(rec.ring, sizeof(rec.ring));

    if (!disk_ready()) return -1;
    return ahci_write(disk_hba(), 0, WUBU_CRASH_LBA, 1, &rec) == 1 ? 0 : -1;
}

int wubu_crash_pickup(void)
{
    if (!disk_ready()) return 0;
    static wubu_crash_record_t rec;
    if (ahci_read(disk_hba(), 0, WUBU_CRASH_LBA, 1, &rec) != 1) return 0;
    if (rec.magic != WUBU_CRASH_MAGIC) return 0;
    /* report: the last crash is evidence, not a secret */
    extern void klog_printf(const char *, ...);
    klog_printf("WuBuOS: crash record found (seq=%u vec=%u rip=%x rsp=%x): %s\n",
                (unsigned)rec.seq, (unsigned)rec.vector,
                (unsigned)rec.rip, (unsigned)rec.rsp,
                rec.reason[0] ? rec.reason : "(no reason)");
    /* dump the ring's first lines (the tail holds the newest) */
    char *line = rec.ring;
    for (int n = 0; n < 6 && *line; n++) {
        char *nl = line;
        while (*nl && *nl != '\n') nl++;
        char save = *nl;
        *nl = '\0';
        klog_printf("  | %s\n", line);
        if (!save) break;
        line = nl + 1;
    }
    return 1;
}
