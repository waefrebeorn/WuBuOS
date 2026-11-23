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
#include "fat32.h"
#include "klog.h"
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

/* Persistent crash record on the boot volume. The old AHCI sim-disk
 * sector write vanished on reboot; this file survives write -> reboot ->
 * read. Panic paths still keep the resident record when the volume is
 * unavailable, so ISR safety is not traded away. */
#define CRASH_FILE "CRASH.DMP"

int wubu_crash_dump(const char *reason, uint64_t rip, uint64_t rsp,
                    uint32_t vector)
{
    static wubu_crash_record_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.magic = WUBU_CRASH_MAGIC;
    rec.seq = 1;
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

    fat32_volume *vol = fat32_boot_volume();
    if (!vol) return -1;

    wubu_crash_record_t old;
    fat32_file_info fi;
    fat32_file f;
    if (fat32_find(vol, 0, CRASH_FILE, &fi) == 0 &&
        fat32_open(vol, 0, CRASH_FILE, "r", &f) == 0) {
        size_t got = fat32_read(&f, &old, sizeof(old));
        fat32_close(&f);
        if (got >= sizeof(uint32_t) * 2 && old.magic == WUBU_CRASH_MAGIC)
            rec.seq = old.seq + 1;
    }

    fat32_file_info nfi;
    (void)fat32_create(vol, 0, CRASH_FILE, 0, &nfi); /* exists is fine */
    if (fat32_open(vol, 0, CRASH_FILE, "w", &f) != 0) return -1;
    size_t wrote = fat32_write(&f, &rec, sizeof(rec));
    fat32_close(&f);
    if (wrote != sizeof(rec)) return -1;
    fat32_flush(vol);
    klog_printf("crash: wrote %s seq=%u (%u bytes)\n",
                CRASH_FILE, (unsigned)rec.seq, (unsigned)wrote);
    return 0;
}

int wubu_crash_pickup(void)
{
    fat32_volume *vol = fat32_boot_volume();
    if (!vol) return 0;
    static wubu_crash_record_t rec;
    fat32_file_info fi;
    fat32_file f;
    if (fat32_find(vol, 0, CRASH_FILE, &fi) != 0 ||
        fat32_open(vol, 0, CRASH_FILE, "r", &f) != 0) return 0;
    size_t got = fat32_read(&f, &rec, sizeof(rec));
    fat32_close(&f);
    if (got < sizeof(rec) || rec.magic != WUBU_CRASH_MAGIC) return 0;
    klog_printf("WuBuOS: crash record found (seq=%u vec=%u rip=%x rsp=%x): %s\n",
                (unsigned)rec.seq, (unsigned)rec.vector,
                (unsigned)rec.rip, (unsigned)rec.rsp,
                rec.reason[0] ? rec.reason : "(no reason)");
    return 1;
}
