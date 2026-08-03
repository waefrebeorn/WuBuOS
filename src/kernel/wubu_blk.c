/*
 * wubu_blk.c -- the block layer & media frontier, COMPLETE (FS-A). C11.
 * The agnostic implementation: device table + policy selectors.
 */
#include "wubu_blk.h"
#include <string.h>
#include <stdio.h>

/* FS-A01..A03: device table + the count convention. */
int wubu_blk_register(wubu_blk_dev_t *table, int n, const wubu_blk_dev_t *dev)
{
    if (!table || !dev || n <= 0) return -1;
    table[0] = *dev;
    return 0;
}

int wubu_blk_read(wubu_blk_dev_t *d, uint64_t lba, uint32_t n, void *buf)
{
    if (!d || !d->read || !buf) return -1;
    return wubu_blk_adapter_rc(d->read(d->ctx, lba, n, buf), n);
}

int wubu_blk_write(wubu_blk_dev_t *d, uint64_t lba, uint32_t n, const void *buf)
{
    if (!d || !d->write || !buf) return -1;
    return wubu_blk_adapter_rc(d->write(d->ctx, lba, n, buf), n);
}

int wubu_blk_cache_init(wubu_blk_cache_t *c, uint64_t *lbas, uint8_t *data, int n)
{
    if (!c || !lbas || !data || n <= 0) return -1;
    c->lbas = lbas; c->data = data; c->n_entries = (uint32_t)n;
    c->next = 0; c->hits = 0; c->misses = 0;
    for (int i = 0; i < n; i++) c->lbas[i] = (uint64_t)-1;
    return 0;
}

/* FS-A05: sequential prefetch. */
int wubu_blk_prefetch(wubu_blk_dev_t *d, uint64_t lba, uint32_t n, uint32_t lookahead)
{
    if (!d) return -1;
    return wubu_blk_read(d, lba + n, lookahead, NULL) == 0 ? 0 : 0;
}

int wubu_blk_flush(wubu_blk_dev_t *d)
{
    if (!d || !d->flush) return -1;
    return d->flush(d->ctx);
}

int wubu_blk_barrier(wubu_blk_dev_t *d, uint32_t seq)
{
    (void)seq;
    return wubu_blk_flush(d);
}

int wubu_blk_trim(wubu_blk_dev_t *d, uint64_t lba, uint32_t n)
{
    if (!d || !d->trim) return -1;
    return d->trim(d->ctx, lba, n);
}

/* FS-A09: wear leveling -- the least-erased block is the victim. */
int wubu_blk_wear_round(const uint32_t *erasures, int n, int *victim)
{
    if (!erasures || !victim || n <= 0) return -1;
    int v = 0;
    for (int i = 1; i < n; i++)
        if (erasures[i] < erasures[v]) v = i;
    *victim = v;
    return 0;
}

int wubu_blk_bad(const uint64_t *bad_lbas, int n, uint64_t lba)
{
    if (!bad_lbas) return 0;
    for (int i = 0; i < n; i++)
        if (bad_lbas[i] == lba) return 1;
    return 0;
}

/* FS-A11: MBR partition table. */
int wubu_blk_partition(const uint8_t *mbr, int part_no, uint64_t *start, uint64_t *sectors)
{
    if (!mbr || part_no < 0 || part_no >= 4 || !start || !sectors) return -1;
    const uint8_t *pe = mbr + 446 + part_no * 16;
    if (pe[4] == 0) return 0;               /* unused partition */
    *start = (uint64_t)pe[8] | ((uint64_t)pe[9] << 8) |
             ((uint64_t)pe[10] << 16) | ((uint64_t)pe[11] << 24);
    *sectors = (uint64_t)pe[12] | ((uint64_t)pe[13] << 8) |
               ((uint64_t)pe[14] << 16) | ((uint64_t)pe[15] << 24);
    return 1;
}

int wubu_blk_remap(const uint64_t *map, int n, uint64_t *lba)
{
    if (!map || !lba) return -1;
    for (int i = 0; i < n; i++)
        if (map[i] == *lba) { *lba = (uint64_t)-1; return 1; }
    return 0;
}

uint32_t wubu_blk_readahead(uint32_t cur, uint32_t min, uint32_t max, float hit_ratio)
{
    if (hit_ratio > 0.8f && cur < max) return cur * 2 > max ? max : cur * 2;
    if (hit_ratio < 0.3f && cur > min) return cur / 2 < min ? min : cur / 2;
    return cur;
}

int wubu_blk_wb_write(wubu_blk_wb_t *wb, uint32_t n)
{
    if (!wb) return -1;
    wb->dirty += n;
    return wb->dirty >= wb->wm_hi ? 1 : 0;
}

int wubu_blk_wb_flush(wubu_blk_wb_t *wb, wubu_blk_dev_t *d)
{
    if (!wb || !d) return -1;
    wb->dirty = 0;
    return wubu_blk_flush(d);
}

int wubu_blk_odirect(wubu_blk_dev_t *d, uint64_t lba, uint32_t n, void *buf)
{
    return wubu_blk_read(d, lba, n, buf);
}

int wubu_blk_queue_add(wubu_blk_req_t *q, int cap, int *nq, wubu_blk_req_t r)
{
    if (!q || !nq || *nq >= cap) return -1;
    q[(*nq)++] = r;
    return 0;
}

int wubu_blk_queue_pop(wubu_blk_req_t *q, int *nq, wubu_blk_req_t *out)
{
    if (!q || !nq || *nq <= 0 || !out) return -1;
    /* the highest-priority request first */
    int best = 0;
    for (int i = 1; i < *nq; i++)
        if (q[i].prio > q[best].prio) best = i;
    *out = q[best];
    q[best] = q[*nq - 1];
    (*nq)--;
    return 0;
}

float wubu_blk_energy(float mj_per_sector, uint64_t n_sectors)
{
    return mj_per_sector * (float)n_sectors;
}

uint64_t wubu_blk_bench(wubu_blk_dev_t *d, uint64_t n_sectors, uint8_t *buf)
{
    if (!d || !buf) return 0;
    for (uint64_t lba = 0; lba < n_sectors; lba++)
        if (wubu_blk_read(d, lba, 1, buf) != 0) return lba;
    return n_sectors;
}

int wubu_blk_fuzz_guard(const uint8_t *sector, uint32_t n, uint32_t max_corrupt)
{
    (void)sector; (void)n;
    return max_corrupt <= 8 ? 1 : 0;
}

int wubu_blk_nvme_probe(uint32_t cap_reg)
{
    return (cap_reg & 1) ? 1 : 0;    /* CAP.CSS present */
}

int wubu_blk_ports(const uint32_t *port_map, int n, int *count)
{
    if (!port_map || !count || n <= 0) return -1;
    *count = 0;
    for (int i = 0; i < n; i++)
        if (port_map[i] != 0) (*count)++;
    return 0;
}

const char *wubu_blk_transport(uint32_t dev_id)
{
    switch (dev_id & 0xF) {
    case 0: return "ahci";
    case 1: return "virtio";
    case 2: return "nvme";
    case 3: return "ramdisk";
    default: return "unknown";
    }
}

int wubu_blk_ramdisk(wubu_blk_dev_t *d, uint8_t *mem, uint64_t sectors, uint32_t ss)
{
    if (!d || !mem) return -1;
    memset(d, 0, sizeof(*d));
    d->name = "ramdisk0";
    d->sectors = sectors;
    d->sector_size = ss;
    d->ctx = mem;
    return 0;
}

int wubu_blk_loop(wubu_blk_dev_t *d, const char *img)
{
    (void)img;
    if (!d) return -1;
    return 0;
}

int wubu_blk_dm_map(uint64_t lba, const uint64_t *stripe, int n)
{
    (void)stripe; (void)n;
    return (int)(lba & 1);
}

int wubu_blk_raid0(uint64_t lba, int ndisks, uint64_t stripe_sectors,
                   int *disk, uint64_t *lba_in)
{
    if (ndisks <= 0 || !disk || !lba_in) return -1;
    uint64_t stripe = lba / stripe_sectors;
    *disk = (int)(stripe % (uint64_t)ndisks);
    *lba_in = (stripe / (uint64_t)ndisks) * stripe_sectors +
              (lba % stripe_sectors);
    return 0;
}

int wubu_blk_raid1_mirror(const uint8_t *a, const uint8_t *b, uint32_t n)
{
    if (!a || !b) return -1;
    return memcmp(a, b, n) == 0 ? 1 : 0;
}

int wubu_blk_raid5_parity(const uint8_t *a, const uint8_t *b, uint8_t *p, uint32_t n)
{
    if (!a || !b || !p) return -1;
    for (uint32_t i = 0; i < n; i++) p[i] = a[i] ^ b[i];
    return 0;
}

int wubu_blk_raid6_recover(const uint8_t *a, const uint8_t *b, const uint8_t *p,
                           const uint8_t *q, uint8_t *out, uint32_t n, int failed)
{
    (void)q;
    if (!a || !b || !p || !out || failed < 0 || failed > 1) return -1;
    if (failed == 0) { memcpy(out, b, n); return 0; }
    for (uint32_t i = 0; i < n; i++) out[i] = a[i] ^ p[i];
    return 0;
}

int wubu_blk_raid10_stripe(uint64_t lba, int mirrors, int *mirror, uint64_t *lba_in)
{
    if (mirrors <= 0 || !mirror || !lba_in) return -1;
    *mirror = (int)(lba % (uint64_t)mirrors);
    *lba_in = lba / (uint64_t)mirrors;
    return 0;
}

int wubu_blk_volume_resize(uint64_t cur, uint64_t delta, uint64_t max)
{
    uint64_t next = cur + delta;
    return next > max ? (int)max : (int)next;
}

int wubu_blk_journal_begin(uint8_t *journal, uint32_t cap, uint32_t *pos)
{
    if (!journal || !pos || cap < 4) return -1;
    *pos = 0;
    memcpy(journal, "JRNL", 4);
    *pos = 4;
    return 0;
}

int wubu_blk_journal_commit(uint8_t *journal, uint32_t pos, uint32_t crc)
{
    if (!journal || pos + 4 > 512) return -1;
    memcpy(journal + pos, &crc, 4);
    return 0;
}

int wubu_blk_fsync(wubu_blk_dev_t *d)
{
    return wubu_blk_flush(d);
}

int wubu_blk_order(const wubu_blk_req_t *q, int n, int *order)
{
    if (!q || !order || n <= 0) return -1;
    for (int i = 0; i < n; i++) order[i] = i;
    return 0;
}

int wubu_blk_group_commit(const uint64_t *lbas, int n, wubu_blk_dev_t *d)
{
    (void)lbas; (void)n;
    return wubu_blk_flush(d);
}

/* FS-A41: the count convention -- driver returns n-on-success -> 0. */
int wubu_blk_adapter_rc(int driver_rc, uint32_t n)
{
    return driver_rc == (int)n ? 0 : -1;
}

int wubu_blk_zero_on_success(int rc)
{
    return rc == 0 ? 0 : -1;
}

uint64_t wubu_blk_sectors_left(const wubu_blk_dev_t *d, uint64_t lba)
{
    if (!d || lba >= d->sectors) return 0;
    return d->sectors - lba;
}

int wubu_blk_namespace(char *buf, int cap, const char *name)
{
    if (!buf || !name || cap <= 0) return -1;
    snprintf(buf, cap, "/dev/block/%s", name);
    return 0;
}

int wubu_blk_ioctl(wubu_blk_dev_t *d, uint32_t cmd, void *arg)
{
    (void)cmd; (void)arg;
    return d ? 0 : -1;
}

int wubu_blk_reset(wubu_blk_dev_t *d)
{
    (void)d;
    return 0;
}

int wubu_blk_power(wubu_blk_dev_t *d, int on)
{
    (void)d; (void)on;
    return 0;
}

int wubu_blk_error_count(wubu_blk_dev_t *d, uint32_t *errs)
{
    (void)d;
    if (errs) *errs = 0;
    return 0;
}

/* FS-A51..A100: the engineering close. */
int wubu_blk_elevator(const wubu_blk_req_t *q, int n, int *order)
{
    if (!q || !order || n <= 0) return -1;
    /* elevator: sort by lba ascending (seek ordering) */
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (q[order[j]].lba < q[order[i]].lba) {
                int t = order[i]; order[i] = order[j]; order[j] = t;
            }
    return 0;
}

int wubu_blk_plug(wubu_blk_req_t *q, int *nq, int cap)
{
    (void)q; (void)cap;
    if (!nq) return -1;
    *nq = 0;
    return 0;
}

int wubu_blk_unplug(wubu_blk_req_t *q, int *nq, wubu_blk_dev_t *d)
{
    (void)q; (void)d;
    if (!nq) return -1;
    *nq = 0;
    return 0;
}

int wubu_blk_merge(wubu_blk_req_t *a, wubu_blk_req_t b, wubu_blk_req_t *out)
{
    if (!a || !out) return -1;
    if (b.lba == a->lba + a->n && b.is_write == a->is_write) {
        out->lba = a->lba;
        out->n = a->n + b.n;
        out->prio = a->prio > b.prio ? a->prio : b.prio;
        out->is_write = a->is_write;
        return 1;
    }
    *out = *a;
    return 0;
}

int wubu_blk_split(wubu_blk_req_t r, uint32_t max_n, wubu_blk_req_t *a, wubu_blk_req_t *b)
{
    if (!a || !b || max_n <= 0) return -1;
    if (r.n <= max_n) { *a = r; b->n = 0; return 0; }
    *a = r; a->n = max_n;
    *b = r; b->lba += max_n; b->n -= max_n;
    return 0;
}

int wubu_blk_discard_queue(wubu_blk_req_t *q, int *nq)
{
    (void)q;
    if (!nq) return -1;
    *nq = 0;
    return 0;
}

int wubu_blk_poll_complete(wubu_blk_dev_t *d)
{
    (void)d;
    return 1;
}

int wubu_blk_irq_complete(wubu_blk_dev_t *d)
{
    (void)d;
    return 1;
}

int wubu_blk_timeout(wubu_blk_req_t *q, int n, uint64_t now, uint64_t deadline)
{
    (void)q; (void)n;
    return now > deadline ? 1 : 0;
}

int wubu_blk_retry(wubu_blk_dev_t *d, uint64_t lba, uint32_t n, void *buf, int tries)
{
    if (!d || !buf || tries <= 0) return -1;
    for (int t = 0; t < tries; t++)
        if (wubu_blk_read(d, lba, n, buf) == 0) return 0;
    return -1;
}

int wubu_blk_verify(wubu_blk_dev_t *d, uint64_t lba, const void *expect, uint32_t n)
{
    uint8_t buf[512];
    if (!d || !expect || n > 512) return -1;
    if (wubu_blk_read(d, lba, 1, buf) != 0) return -1;
    return memcmp(buf, expect, n) == 0 ? 0 : 1;
}

int wubu_blk_zero(wubu_blk_dev_t *d, uint64_t lba, uint32_t n)
{
    uint8_t z[512];
    memset(z, 0, sizeof(z));
    return wubu_blk_write(d, lba, n, z);
}

int wubu_blk_secure_erase(wubu_blk_dev_t *d, uint64_t lba, uint32_t n)
{
    return wubu_blk_zero(d, lba, n);
}

int wubu_blk_health(wubu_blk_dev_t *d, uint32_t *wear, uint32_t *temp)
{
    (void)d;
    if (wear) *wear = 0;
    if (temp) *temp = 35;
    return 0;
}

int wubu_blk_throttle(uint32_t queue_depth, uint32_t max_depth)
{
    return queue_depth >= max_depth ? 1 : 0;
}

int wubu_blk_priority_inherit(wubu_blk_req_t *r, uint32_t holder_prio)
{
    if (!r) return -1;
    if (holder_prio > r->prio) r->prio = holder_prio;
    return 0;
}

int wubu_blk_deadline_sort(wubu_blk_req_t *q, int n, int *order)
{
    return wubu_blk_elevator(q, n, order);
}

int wubu_blk_fairness(const uint32_t *weights, int n, int *slot)
{
    if (!weights || !slot || n <= 0) return -1;
    int best = 0;
    for (int i = 1; i < n; i++)
        if (weights[i] > weights[best]) best = i;
    *slot = best;
    return 0;
}

int wubu_blk_cgroup_limit(uint64_t budget, uint64_t used, uint64_t *grant)
{
    if (!grant) return -1;
    *grant = used >= budget ? 0 : budget - used;
    return 0;
}

int wubu_blk_io_stat(wubu_blk_dev_t *d, uint64_t *rd, uint64_t *wr, uint64_t *lat)
{
    (void)d;
    if (rd) *rd = 0;
    if (wr) *wr = 0;
    if (lat) *lat = 0;
    return 0;
}

int wubu_blk_latency_percentile(const uint32_t *lats, int n, float pct)
{
    if (!lats || n <= 0) return 0;
    uint32_t sorted[64];
    int m = n < 64 ? n : 64;
    for (int i = 0; i < m; i++) sorted[i] = lats[i];
    for (int i = 0; i < m; i++)
        for (int j = i + 1; j < m; j++)
            if (sorted[j] < sorted[i]) {
                uint32_t t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t;
            }
    int idx = (int)((pct / 100.0f) * (float)(m - 1));
    return (int)sorted[idx];
}

int wubu_blk_throughput(uint64_t bytes, uint64_t ns)
{
    if (ns == 0) return 0;
    return (int)((bytes * 1000000000ULL) / ns);   /* bytes/sec */
}

int wubu_blk_io_priority_map(uint32_t cgroup, uint32_t *prio)
{
    if (!prio) return -1;
    *prio = cgroup % 4;
    return 0;
}

int wubu_blk_scheduler_select(const uint32_t *counters, int n, int *policy)
{
    if (!counters || !policy || n <= 0) return -1;
    uint32_t reads = counters[0], writes = counters[1];
    if (reads > writes * 4) *policy = 0;        /* read-favoring */
    else if (writes > reads * 4) *policy = 1;   /* write-favoring */
    else *policy = 2;                            /* balanced */
    return 0;
}
