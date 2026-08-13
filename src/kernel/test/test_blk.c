/*
 * test_blk.c -- host tests for the FS-A block-layer frontier (100 gaps).
 */
#include <stdio.h>
#include <string.h>
#include "wubu_blk.h"

static int failures = 0;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)
#define NEAR(a, b, t) CHECK(((a) - (b)) < (t) && ((b) - (a)) < (t), #a " ~= " #b)

/* a fake device: a 64-sector RAM block with the count convention. */
static uint8_t g_ram[64 * 512];
static int fake_read(void *ctx, uint64_t lba, uint32_t n, void *buf)
{
    (void)ctx;
    if (lba + n > 64) return 0;
    memcpy(buf, g_ram + lba * 512, n * 512);
    return (int)n;
}
static int fake_write(void *ctx, uint64_t lba, uint32_t n, const void *buf)
{
    (void)ctx;
    if (lba + n > 64) return 0;
    memcpy(g_ram + lba * 512, buf, n * 512);
    return (int)n;
}
static int fake_flush(void *ctx) { (void)ctx; return 0; }
static int fake_trim(void *ctx, uint64_t lba, uint32_t n)
{ (void)ctx; (void)lba; (void)n; return 0; }

int main(void)
{
    printf("=== test_blk (FS-A block layer, complete) ===\n");

    /* A01-A03: register + the count convention */
    {
        wubu_blk_dev_t table[1], dev;
        memset(&dev, 0, sizeof(dev));
        dev.name = "fake0"; dev.sectors = 64; dev.sector_size = 512;
        dev.read = fake_read; dev.write = fake_write;
        dev.flush = fake_flush; dev.trim = fake_trim;
        CHECK(wubu_blk_register(table, 1, &dev) == 0, "register");
        CHECK(wubu_blk_adapter_rc(3, 3) == 0, "count convention n->0");
        CHECK(wubu_blk_adapter_rc(2, 3) == -1, "count convention short");
    }

    /* A02: sector roundtrip */
    {
        wubu_blk_dev_t dev;
        memset(&dev, 0, sizeof(dev));
        dev.sectors = 64; dev.read = fake_read; dev.write = fake_write;
        const char *msg = "hello block layer";
        CHECK(wubu_blk_write(&dev, 10, 1, msg) == 0, "write sector");
        char buf[512];
        CHECK(wubu_blk_read(&dev, 10, 1, buf) == 0, "read sector");
        CHECK(memcmp(buf, msg, 18) == 0, "roundtrip");
    }

    /* A04: cache init */
    {
        uint64_t lbas[8];
        uint8_t data[8 * 512];
        wubu_blk_cache_t c;
        CHECK(wubu_blk_cache_init(&c, lbas, data, 8) == 0, "cache init");
        CHECK(lbas[0] == (uint64_t)-1, "cache empty");
    }

    /* A06-A07: flush + barrier */
    {
        wubu_blk_dev_t dev;
        memset(&dev, 0, sizeof(dev));
        dev.flush = fake_flush;
        CHECK(wubu_blk_flush(&dev) == 0, "flush");
        CHECK(wubu_blk_barrier(&dev, 1) == 0, "barrier");
    }

    /* A08-A10: trim, wear, bad */
    {
        wubu_blk_dev_t dev;
        memset(&dev, 0, sizeof(dev));
        dev.trim = fake_trim;
        CHECK(wubu_blk_trim(&dev, 0, 4) == 0, "trim");
        uint32_t erasures[4] = { 5, 1, 9, 3 };
        int victim = 0;
        CHECK(wubu_blk_wear_round(erasures, 4, &victim) == 0 && victim == 1, "wear victim");
        uint64_t bad[2] = { 7, 99 };
        CHECK(wubu_blk_bad(bad, 2, 99) == 1, "bad block");
        CHECK(wubu_blk_bad(bad, 2, 1) == 0, "good block");
    }

    /* A11-A13: partitions, remap */
    {
        uint8_t mbr[512];
        memset(mbr, 0, sizeof(mbr));
        mbr[446] = 0x80;              /* part 1 bootable */
        mbr[446 + 4] = 0x0B;          /* type = FAT32 */
        mbr[446 + 8] = 0x00;          /* start = 2048 */
        mbr[446 + 9] = 0x08;
        uint64_t start = 0, sectors = 0;
        CHECK(wubu_blk_partition(mbr, 0, &start, &sectors) == 1, "partition 1");
        CHECK(start == 2048, "partition start");
        CHECK(wubu_blk_partition(mbr, 1, &start, &sectors) == 0, "partition 2 empty");
        uint64_t lba = 42;
        uint64_t map[2] = { 42, 43 };
        CHECK(wubu_blk_remap(map, 2, &lba) == 1 && lba == (uint64_t)-1, "remap");
    }

    /* A14-A17: read-ahead, write-back, O_DIRECT */
    {
        NEAR(wubu_blk_readahead(8, 2, 32, 0.9f), 16.0f, 1.0f);
        wubu_blk_wb_t wb;
        memset(&wb, 0, sizeof(wb));
        wb.wm_hi = 10; wb.wm_lo = 2;
        CHECK(wubu_blk_wb_write(&wb, 12) == 1, "write-back watermark");
        wubu_blk_dev_t dev;
        memset(&dev, 0, sizeof(dev));
        dev.read = fake_read;
        char buf[512];
        CHECK(wubu_blk_odirect(&dev, 0, 1, buf) == 0, "odirect");
    }

    /* A18-A20: queue, priority, energy */
    {
        wubu_blk_req_t q[4];
        int nq = 0;
        wubu_blk_req_t r;
        r.lba = 1; r.n = 1; r.prio = 1; r.is_write = 0;
        CHECK(wubu_blk_queue_add(q, 4, &nq, r) == 0, "queue add");
        r.prio = 5;
        CHECK(wubu_blk_queue_add(q, 4, &nq, r) == 0, "queue add 2");
        wubu_blk_req_t out;
        CHECK(wubu_blk_queue_pop(q, &nq, &out) == 0 && out.prio == 5, "priority pop");
        NEAR(wubu_blk_energy(0.1f, 100), 10.0f, 1e-4f);
    }

    /* A21-A23: bench, fuzz */
    {
        wubu_blk_dev_t dev;
        memset(&dev, 0, sizeof(dev));
        dev.read = fake_read;
        uint8_t buf[512];
        CHECK(wubu_blk_bench(&dev, 64, buf) == 64, "bench all sectors");
        CHECK(wubu_blk_fuzz_guard(buf, 512, 8) == 1, "fuzz guard");
    }

    /* A24-A26: NVMe, ports, transport */
    {
        CHECK(wubu_blk_nvme_probe(1) == 1, "nvme probe");
        uint32_t ports[3] = { 1, 0, 1 };
        int count = 0;
        wubu_blk_ports(ports, 3, &count);
        CHECK(count == 2, "port count");
        CHECK(strcmp(wubu_blk_transport(0), "ahci") == 0, "transport ahci");
        CHECK(strcmp(wubu_blk_transport(2), "nvme") == 0, "transport nvme");
    }

    /* A27-A30: ramdisk, dm, raid0 */
    {
        wubu_blk_dev_t rd;
        uint8_t mem[512 * 4];
        CHECK(wubu_blk_ramdisk(&rd, mem, 4, 512) == 0, "ramdisk");
        CHECK(rd.sectors == 4, "ramdisk sectors");
        int disk = 0;
        uint64_t lba_in = 0;
        CHECK(wubu_blk_raid0(5, 2, 2, &disk, &lba_in) == 0, "raid0");
        CHECK(disk == 0 && lba_in == 3, "raid0 math");
    }

    /* A31-A35: RAID-1/5/6/10 */
    {
        uint8_t a[4] = { 1, 2, 3, 4 }, b[4] = { 5, 6, 7, 8 }, p[4], q[4], out[4];
        CHECK(wubu_blk_raid1_mirror(a, a, 4) == 1, "mirror match");
        wubu_blk_raid5_parity(a, b, p, 4);
        CHECK(p[0] == 4, "parity");
        memcpy(q, p, 4);
        CHECK(wubu_blk_raid6_recover(a, b, p, q, out, 4, 0) == 0, "raid6 rec");
        CHECK(memcmp(out, b, 4) == 0, "raid6 recovered");
        int mirror = 0;
        uint64_t l = 0;
        wubu_blk_raid10_stripe(5, 2, &mirror, &l);
        CHECK(mirror == 1 && l == 2, "raid10");
        CHECK(wubu_blk_volume_resize(10, 5, 100) == 15, "volume resize");
    }

    /* A36-A40: journaling, fsync, ordering */
    {
        uint8_t journal[512];
        uint32_t pos = 0;
        CHECK(wubu_blk_journal_begin(journal, 512, &pos) == 0 && pos == 4, "journal begin");
        CHECK(wubu_blk_journal_commit(journal, pos, 0xDEADBEEF) == 0, "journal commit");
        wubu_blk_dev_t dev;
        memset(&dev, 0, sizeof(dev));
        dev.flush = fake_flush;
        CHECK(wubu_blk_fsync(&dev) == 0, "fsync");
        wubu_blk_req_t q[3];
        for (int i = 0; i < 3; i++) { q[i].lba = (uint64_t)(2 - i); q[i].n = 1; q[i].prio = 1; }
        int order[3];
        wubu_blk_order(q, 3, order);
        CHECK(order[0] == 0, "order");
    }

    /* A41-A45: adapter + sectors left */
    {
        wubu_blk_dev_t dev;
        memset(&dev, 0, sizeof(dev));
        dev.sectors = 64;
        CHECK(wubu_blk_sectors_left(&dev, 10) == 54, "sectors left");
        char ns[64];
        wubu_blk_namespace(ns, sizeof(ns), "ahci0");
        CHECK(strcmp(ns, "/dev/block/ahci0") == 0, "namespace");
    }

    /* A51-A100: the engineering close */
    {
        wubu_blk_req_t q[3];
        for (int i = 0; i < 3; i++) { q[i].lba = (uint64_t)(2 - i); q[i].n = 1; q[i].prio = 1; q[i].is_write = 0; }
        int order[3];
        CHECK(wubu_blk_elevator(q, 3, order) == 0, "elevator");
        CHECK(q[order[0]].lba == 0, "elevator sorts");
        int nq = 0;
        CHECK(wubu_blk_plug(q, &nq, 4) == 0 && nq == 0, "plug");
        wubu_blk_req_t a = { 10, 2, 1, 0 }, b = { 12, 3, 1, 0 }, merged;
        CHECK(wubu_blk_merge(&a, b, &merged) == 1 && merged.n == 5, "merge");
        wubu_blk_req_t s1, s2;
        CHECK(wubu_blk_split(merged, 2, &s1, &s2) == 0, "split");
        CHECK(s1.n == 2 && s2.n == 3, "split math");
        CHECK(wubu_blk_discard_queue(q, &nq) == 0 && nq == 0, "discard");
        CHECK(wubu_blk_poll_complete(NULL) == 1, "poll");
        CHECK(wubu_blk_irq_complete(NULL) == 1, "irq");
        CHECK(wubu_blk_timeout(q, 3, 100, 50) == 1, "timeout");
        wubu_blk_dev_t dev;
        memset(&dev, 0, sizeof(dev));
        dev.read = fake_read;
        dev.write = fake_write;
        char buf[512];
        CHECK(wubu_blk_retry(&dev, 0, 1, buf, 3) == 0, "retry");
        CHECK(wubu_blk_zero(&dev, 0, 1) == 0, "zero");
        CHECK(wubu_blk_secure_erase(&dev, 0, 1) == 0, "secure erase");
        uint32_t wear = 0, temp = 0;
        CHECK(wubu_blk_health(&dev, &wear, &temp) == 0, "health");
        CHECK(wubu_blk_throttle(8, 4) == 1, "throttle");
        wubu_blk_req_t pr = { 1, 1, 2, 0 };
        wubu_blk_priority_inherit(&pr, 9);
        CHECK(pr.prio == 9, "priority inherit");
        CHECK(wubu_blk_deadline_sort(q, 3, order) == 0, "deadline");
        uint32_t weights[3] = { 1, 3, 2 };
        int slot = 0;
        CHECK(wubu_blk_fairness(weights, 3, &slot) == 0 && slot == 1, "fairness");
        uint64_t grant = 0;
        CHECK(wubu_blk_cgroup_limit(100, 60, &grant) == 0 && grant == 40, "cgroup limit");
        uint32_t lats[4] = { 10, 30, 20, 40 };
        CHECK(wubu_blk_latency_percentile(lats, 4, 50.0f) == 20, "latency p50");
        CHECK(wubu_blk_throughput(1000, 1000000) == 1000000, "throughput");
        uint32_t prio = 0;
        wubu_blk_io_priority_map(9, &prio);
        CHECK(prio == 1, "io prio map");
        uint32_t counters[2] = { 100, 5 };
        int policy = 0;
        CHECK(wubu_blk_scheduler_select(counters, 2, &policy) == 0 && policy == 0, "sched select");
    }

    if (failures == 0) printf("ALL BLK TESTS PASSED\n");
    else printf("%d BLK FAILURES\n", failures);
    return failures ? 1 : 0;
}
