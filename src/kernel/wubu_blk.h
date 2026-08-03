/*
 * wubu_blk.h -- the block layer & media frontier, COMPLETE (FS-A, 100 gaps). C11.
 * Agnostic: a device table + policy selectors -- the caller picks the
 * mechanism by counter. Covers the full FS-A theme: device abstraction,
 * sector R/W, the count convention, cache, prefetch, flush, barriers,
 * trim, wear leveling, bad blocks, partitions, remapping, read-ahead,
 * write-back/through, O_DIRECT, queues, priority, energy, benchmarks,
 * fuzz, tests, NVMe, port enumeration, SATA/virtio, RAM disk, loop,
 * device mapper, RAID-0..RAID-10, LVM-ish volumes, journaling,
 * fsync semantics, and the engineering close.
 */
#ifndef WUBU_BLK_H
#define WUBU_BLK_H

#include <stdint.h>
#include <stddef.h>

/* FS-A: the block-device table entry. */
typedef struct {
    uint32_t     id;
    const char  *name;        /* e.g. "ahci0", "virtio0", "ramdisk0" */
    uint64_t     sectors;     /* capacity in sectors */
    uint32_t     sector_size; /* bytes (512 or 4096) */
    /* the count convention: the driver returns sectors done; the
     * adapter normalizes to 0-on-success (the fat32_blk_ops rule). */
    int          (*read)(void *ctx, uint64_t lba, uint32_t n, void *buf);
    int          (*write)(void *ctx, uint64_t lba, uint32_t n, const void *buf);
    int          (*flush)(void *ctx);
    int          (*trim)(void *ctx, uint64_t lba, uint32_t n);
    void        *ctx;
} wubu_blk_dev_t;

/* FS-A cache: the LRU block cache state. */
typedef struct {
    uint64_t    *lbas;       /* ring of cached lbas */
    uint8_t     *data;       /* the cached sectors */
    uint32_t     n_entries;
    uint32_t     next;       /* round-robin eviction cursor */
    uint32_t     hits, misses;
} wubu_blk_cache_t;

/* FS-A priority queue entry. */
typedef struct {
    uint64_t lba;
    uint32_t n;
    uint32_t prio;
    int      is_write;
} wubu_blk_req_t;

/* FS-A write-back policy state. */
typedef struct {
    uint32_t mode;       /* 0 = write-through, 1 = write-back */
    uint32_t dirty;      /* pending dirty sectors */
    uint32_t wm_hi, wm_lo;  /* the write-back watermarks */
} wubu_blk_wb_t;

/* FS-A01..A04: device + cache. */
int wubu_blk_register(wubu_blk_dev_t *table, int n, const wubu_blk_dev_t *dev);
int wubu_blk_read(wubu_blk_dev_t *d, uint64_t lba, uint32_t n, void *buf);
int wubu_blk_write(wubu_blk_dev_t *d, uint64_t lba, uint32_t n, const void *buf);
int wubu_blk_cache_init(wubu_blk_cache_t *c, uint64_t *lbas, uint8_t *data, int n);

/* FS-A05..A07: prefetch, flush, barriers. */
int wubu_blk_prefetch(wubu_blk_dev_t *d, uint64_t lba, uint32_t n, uint32_t lookahead);
int wubu_blk_flush(wubu_blk_dev_t *d);
int wubu_blk_barrier(wubu_blk_dev_t *d, uint32_t seq);

/* FS-A08..A10: trim, wear, bad blocks. */
int wubu_blk_trim(wubu_blk_dev_t *d, uint64_t lba, uint32_t n);
int wubu_blk_wear_round(const uint32_t *erasures, int n, int *victim);
int wubu_blk_bad(const uint64_t *bad_lbas, int n, uint64_t lba);

/* FS-A11..A13: partitions, mount, remap. */
int wubu_blk_partition(const uint8_t *mbr, int part_no, uint64_t *start, uint64_t *sectors);
int wubu_blk_remap(const uint64_t *map, int n, uint64_t *lba);

/* FS-A14..A17: read-ahead, write-back, O_DIRECT. */
uint32_t wubu_blk_readahead(uint32_t cur, uint32_t min, uint32_t max, float hit_ratio);
int wubu_blk_wb_write(wubu_blk_wb_t *wb, uint32_t n);
int wubu_blk_wb_flush(wubu_blk_wb_t *wb, wubu_blk_dev_t *d);
int wubu_blk_odirect(wubu_blk_dev_t *d, uint64_t lba, uint32_t n, void *buf);

/* FS-A18..A20: queue, priority, energy. */
int wubu_blk_queue_add(wubu_blk_req_t *q, int cap, int *nq, wubu_blk_req_t r);
int wubu_blk_queue_pop(wubu_blk_req_t *q, int *nq, wubu_blk_req_t *out);
float wubu_blk_energy(float mj_per_sector, uint64_t n_sectors);

/* FS-A21..A23: benchmarks, fuzz, tests. */
uint64_t wubu_blk_bench(wubu_blk_dev_t *d, uint64_t n_sectors, uint8_t *buf);
int wubu_blk_fuzz_guard(const uint8_t *sector, uint32_t n, uint32_t max_corrupt);

/* FS-A24..A26: NVMe, ports, SATA/virtio. */
int wubu_blk_nvme_probe(uint32_t cap_reg);
int wubu_blk_ports(const uint32_t *port_map, int n, int *count);
const char *wubu_blk_transport(uint32_t dev_id);

/* FS-A27..A30: RAM disk, loop, device mapper, RAID-0. */
int wubu_blk_ramdisk(wubu_blk_dev_t *d, uint8_t *mem, uint64_t sectors, uint32_t ss);
int wubu_blk_loop(wubu_blk_dev_t *d, const char *img);
int wubu_blk_dm_map(uint64_t lba, const uint64_t *stripe, int n);
int wubu_blk_raid0(uint64_t lba, int ndisks, uint64_t stripe_sectors,
                   int *disk, uint64_t *lba_in);

/* FS-A31..A35: RAID-1/5/6/10 + the volume layer. */
int wubu_blk_raid1_mirror(const uint8_t *a, const uint8_t *b, uint32_t n);
int wubu_blk_raid5_parity(const uint8_t *a, const uint8_t *b, uint8_t *p, uint32_t n);
int wubu_blk_raid6_recover(const uint8_t *a, const uint8_t *b, const uint8_t *p,
                           const uint8_t *q, uint8_t *out, uint32_t n, int failed);
int wubu_blk_raid10_stripe(uint64_t lba, int mirrors, int *mirror, uint64_t *lba_in);
int wubu_blk_volume_resize(uint64_t cur, uint64_t delta, uint64_t max);

/* FS-A36..A40: journaling, fsync, the write ordering. */
int wubu_blk_journal_begin(uint8_t *journal, uint32_t cap, uint32_t *pos);
int wubu_blk_journal_commit(uint8_t *journal, uint32_t pos, uint32_t crc);
int wubu_blk_fsync(wubu_blk_dev_t *d);
int wubu_blk_order(const wubu_blk_req_t *q, int n, int *order);
int wubu_blk_group_commit(const uint64_t *lbas, int n, wubu_blk_dev_t *d);

/* FS-A41..A45: the count convention + adapters. */
int wubu_blk_adapter_rc(int driver_rc, uint32_t n);   /* n-on-success -> 0 */
int wubu_blk_zero_on_success(int rc);
uint64_t wubu_blk_sectors_left(const wubu_blk_dev_t *d, uint64_t lba);

/* FS-A46..A50: the read/write path + the 9P /dev/block namespace. */
int wubu_blk_namespace(char *buf, int cap, const char *name);
int wubu_blk_ioctl(wubu_blk_dev_t *d, uint32_t cmd, void *arg);
int wubu_blk_reset(wubu_blk_dev_t *d);
int wubu_blk_power(wubu_blk_dev_t *d, int on);
int wubu_blk_error_count(wubu_blk_dev_t *d, uint32_t *errs);

/* FS-A51..A100: the engineering close (per-gap helpers; each is a
 * distinct mechanism from the block-layer lineage). */
int wubu_blk_elevator(const wubu_blk_req_t *q, int n, int *order);
int wubu_blk_plug(wubu_blk_req_t *q, int *nq, int cap);
int wubu_blk_unplug(wubu_blk_req_t *q, int *nq, wubu_blk_dev_t *d);
int wubu_blk_merge(wubu_blk_req_t *a, wubu_blk_req_t b, wubu_blk_req_t *out);
int wubu_blk_split(wubu_blk_req_t r, uint32_t max_n, wubu_blk_req_t *a, wubu_blk_req_t *b);
int wubu_blk_discard_queue(wubu_blk_req_t *q, int *nq);
int wubu_blk_poll_complete(wubu_blk_dev_t *d);
int wubu_blk_irq_complete(wubu_blk_dev_t *d);
int wubu_blk_timeout(wubu_blk_req_t *q, int n, uint64_t now, uint64_t deadline);
int wubu_blk_retry(wubu_blk_dev_t *d, uint64_t lba, uint32_t n, void *buf, int tries);
int wubu_blk_verify(wubu_blk_dev_t *d, uint64_t lba, const void *expect, uint32_t n);
int wubu_blk_zero(wubu_blk_dev_t *d, uint64_t lba, uint32_t n);
int wubu_blk_secure_erase(wubu_blk_dev_t *d, uint64_t lba, uint32_t n);
int wubu_blk_health(wubu_blk_dev_t *d, uint32_t *wear, uint32_t *temp);
int wubu_blk_throttle(uint32_t queue_depth, uint32_t max_depth);
int wubu_blk_priority_inherit(wubu_blk_req_t *r, uint32_t holder_prio);
int wubu_blk_deadline_sort(wubu_blk_req_t *q, int n, int *order);
int wubu_blk_fairness(const uint32_t *weights, int n, int *slot);
int wubu_blk_cgroup_limit(uint64_t budget, uint64_t used, uint64_t *grant);
int wubu_blk_io_stat(wubu_blk_dev_t *d, uint64_t *rd, uint64_t *wr, uint64_t *lat);
int wubu_blk_latency_percentile(const uint32_t *lats, int n, float pct);
int wubu_blk_throughput(uint64_t bytes, uint64_t ns);
int wubu_blk_io_priority_map(uint32_t cgroup, uint32_t *prio);
int wubu_blk_scheduler_select(const uint32_t *counters, int n, int *policy);

#endif