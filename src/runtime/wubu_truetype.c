#include "wubu_truetype.h"

#include <string.h>

/* Every multi-byte field in SFNT is big-endian; every read below is bounds-checked first,
 * because a font is untrusted input and a table offset is an attacker-controlled number. */
static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

struct Table {
    uint32_t off;
    uint32_t len;
};

struct WubuTtf {
    const uint8_t *d;
    size_t n;

    uint16_t upem;
    uint16_t num_glyphs;
    uint16_t num_hmetrics;
    uint16_t loca_long;

    struct Table head, maxp, hhea, hmtx, loca, glyf, cmap;

    uint32_t cmap_off;   /* chosen subtable, absolute */
    uint16_t cmap_fmt;   /* 4 or 12 */
    uint8_t has_glyf;
};

static int in_range(const WubuTtf *t, uint32_t off, uint32_t len)
{
    return (uint64_t)off + len <= (uint64_t)t->n;
}

/* Find a table record by 4-byte tag. Returns 0 when absent. */
static int find_table(const WubuTtf *t, const char *tag, struct Table *out)
{
    uint16_t num = be16(t->d + 4);
    uint32_t rec = 12;
    for (uint16_t i = 0; i < num; i++, rec += 16) {
        if (!in_range(t, rec, 16)) return 0;
        const uint8_t *r = t->d + rec;
        if (memcmp(r, tag, 4) == 0) {
            out->off = be32(r + 8);
            out->len = be32(r + 12);
            return in_range(t, out->off, out->len);
        }
    }
    return 0;
}

/* Pick the cmap subtable: 32-bit Unicode (format 12) first, then BMP (format 4).
 * Per the OpenType cmap rules the 32-bit subtable must be a superset, so preferring the
 * 16-bit one would silently drop characters that the font does contain. */
static int choose_cmap(WubuTtf *t)
{
    const uint8_t *c = t->d + t->cmap.off;
    if (t->cmap.len < 4) return 0;
    uint16_t nsub = be16(c + 2);
    if (t->cmap.len < 4u + (uint32_t)nsub * 8u) return 0;

    uint32_t best12 = 0, best4 = 0;
    for (uint16_t i = 0; i < nsub; i++) {
        const uint8_t *rec = c + 4 + i * 8;
        uint16_t plat = be16(rec), enc = be16(rec + 2);
        uint32_t sub = be32(rec + 4);
        if (sub >= t->cmap.len || !in_range(t, t->cmap.off + sub, 2)) continue;
        uint16_t fmt = be16(c + sub);
        int unicode = (plat == 0) || (plat == 3 && (enc == 1 || enc == 10));
        if (!unicode) continue;
        if (fmt == 12 && !best12) best12 = sub;
        if (fmt == 4 && !best4) best4 = sub;
    }
    if (best12) {
        t->cmap_off = t->cmap.off + best12;
        t->cmap_fmt = 12;
        return 1;
    }
    if (best4) {
        t->cmap_off = t->cmap.off + best4;
        t->cmap_fmt = 4;
        return 1;
    }
    return 0;
}

WubuTtf *wubu_ttf_open(const uint8_t *data, size_t len, const char **why)
{
    static WubuTtf t;  /* one handle: fonts are loaded for the life of the shell, not per glyph */
    if (why) *why = NULL;

    if (!data || len < 12) { if (why) *why = "data shorter than an sfnt header"; return NULL; }

    uint32_t ver = be32(data);
    if (ver == 0x4F54544F) {  /* 'OTTO' */
        if (why) *why = "CFF outlines (OTTO) are not supported by this reader";
        return NULL;
    }
    if (ver != 0x00010000u && ver != 0x74727565u) {  /* 1.0 or 'true' */
        if (why) *why = "not an sfnt: unknown version tag";
        return NULL;
    }

    memset(&t, 0, sizeof t);
    t.d = data;
    t.n = len;

    if (!find_table(&t, "head", &t.head) || t.head.len < 54) {
        if (why) *why = "missing or short head table";
        return NULL;
    }
    if (!find_table(&t, "maxp", &t.maxp) || t.maxp.len < 6) {
        if (why) *why = "missing or short maxp table";
        return NULL;
    }
    if (!find_table(&t, "hhea", &t.hhea) || t.hhea.len < 36) {
        if (why) *why = "missing or short hhea table";
        return NULL;
    }
    if (!find_table(&t, "hmtx", &t.hmtx) || t.hmtx.len < 4) {
        if (why) *why = "missing or short hmtx table";
        return NULL;
    }
    if (!find_table(&t, "cmap", &t.cmap) || t.cmap.len < 4) {
        if (why) *why = "missing or short cmap table";
        return NULL;
    }

    t.upem = be16(t.d + t.head.off + 18);
    t.loca_long = be16(t.d + t.head.off + 50);
    t.num_glyphs = be16(t.d + t.maxp.off + 4);
    t.num_hmetrics = be16(t.d + t.hhea.off + 34);

    if (t.upem == 0) { if (why) *why = "unitsPerEm is zero"; return NULL; }
    if (t.num_glyphs == 0) { if (why) *why = "no glyphs"; return NULL; }
    if (t.num_hmetrics == 0) { if (why) *why = "numberOfHMetrics is zero"; return NULL; }
    if (1u + (uint32_t)t.num_hmetrics > t.hmtx.len / 4) {
        if (why) *why = "hmtx shorter than numberOfHMetrics";
        return NULL;
    }

    t.has_glyf = (uint8_t)(find_table(&t, "glyf", &t.glyf) && find_table(&t, "loca", &t.loca));

    if (!choose_cmap(&t)) { if (why) *why = "no usable Unicode cmap subtable"; return NULL; }

    return &t;
}

void wubu_ttf_close(WubuTtf *ttf) { (void)ttf; }

uint16_t wubu_ttf_units_per_em(const WubuTtf *ttf) { return ttf ? ttf->upem : 0; }
uint16_t wubu_ttf_num_glyphs(const WubuTtf *ttf) { return ttf ? ttf->num_glyphs : 0; }
int wubu_ttf_has_glyf(const WubuTtf *ttf) { return ttf ? ttf->has_glyf : 0; }

uint32_t wubu_ttf_glyph_index(const WubuTtf *ttf, uint32_t cp)
{
    if (!ttf) return 0;
    const uint8_t *c = ttf->d + ttf->cmap_off;

    if (ttf->cmap_fmt == 12) {
        if (!in_range(ttf, ttf->cmap_off, 16)) return 0;
        uint32_t ngroups = be32(c + 12);
        if (!in_range(ttf, ttf->cmap_off + 16, ngroups * 12u)) return 0;
        /* Groups are sorted, so binary search rather than a walk of every range. */
        uint32_t lo = 0, hi = ngroups;
        while (lo < hi) {
            uint32_t mid = lo + (hi - lo) / 2;
            const uint8_t *g = c + 16 + mid * 12;
            uint32_t start = be32(g), end = be32(g + 4), gid = be32(g + 8);
            if (cp < start) hi = mid;
            else if (cp > end) lo = mid + 1;
            else return gid + (cp - start);
        }
        return 0;
    }

    /* format 4: 16-bit only, so anything above the BMP is simply not mapped here. */
    if (cp > 0xFFFFu) return 0;
    if (!in_range(ttf, ttf->cmap_off, 14)) return 0;
    uint16_t segx2 = be16(c + 6);
    uint16_t segs = (uint16_t)(segx2 / 2);
    if (segs == 0) return 0;
    /* endCode[], reservedPad, startCode[], idDelta[], idRangeOffset[] */
    uint32_t need = 16u + (uint32_t)segx2 * 4u;
    if (!in_range(ttf, ttf->cmap_off, need)) return 0;

    const uint8_t *endCode = c + 14;
    const uint8_t *startCode = endCode + segx2 + 2;
    const uint8_t *idDelta = startCode + segx2;
    const uint8_t *idRange = idDelta + segx2;

    for (uint16_t i = 0; i < segs; i++) {
        uint16_t end = be16(endCode + i * 2);
        if (cp > end) continue;
        uint16_t start = be16(startCode + i * 2);
        if (cp < start) return 0;
        uint16_t delta = be16(idDelta + i * 2);
        uint16_t ro = be16(idRange + i * 2);
        if (ro == 0) return (uint16_t)(cp + delta);
        /* idRangeOffset is in bytes, from its own slot; the glyph id lives at that address. */
        uint32_t slot = (uint32_t)(idRange + i * 2 - c) + ro + (cp - start) * 2u;
        if (!in_range(ttf, ttf->cmap_off + slot, 2)) return 0;
        uint16_t gid = be16(c + slot);
        return gid ? (uint16_t)(gid + delta) : 0;
    }
    return 0;
}

uint32_t wubu_ttf_advance_units(const WubuTtf *ttf, uint32_t gid)
{
    if (!ttf || gid >= ttf->num_glyphs) return 0;
    uint32_t idx = gid < ttf->num_hmetrics ? gid : (uint32_t)ttf->num_hmetrics - 1u;
    const uint8_t *h = ttf->d + ttf->hmtx.off + idx * 4u;
    return be16(h);
}

int wubu_ttf_advance_px(const WubuTtf *ttf, uint32_t gid, int ppem)
{
    if (!ttf || ppem <= 0) return 0;
    uint32_t u = wubu_ttf_advance_units(ttf, gid);
    if (u == 0) return 0;
    return (int)(((uint64_t)u * (uint32_t)ppem + ttf->upem / 2) / ttf->upem);
}
