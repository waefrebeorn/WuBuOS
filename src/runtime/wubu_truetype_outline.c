/* wubu_truetype_outline -- glyf decode: simple glyphs and composites (SLERM-2027 stage 2).
 *
 * Included by wubu_truetype.c so the SFNT handle stays private: a caller gets points and
 * contour ends, never table offsets.
 *
 * Simple glyph layout (OpenType glyf, Apple TrueType Ref. Tables 15/16):
 *   int16 numberOfContours, xMin/yMin/xMax/yMax,
 *   uint16 endPtsOfContours[n], uint16 instructionLength, uint8 instructions[],
 *   uint8 flags[] (bit3 = repeat, next byte is the count of extra repeats),
 *   x deltas then y deltas, 1 byte when the short flag is set else 2 bytes,
 *   with bit4/bit5 meaning "sign" when short and "same as previous" when not.
 *
 * Composites: a loop of component records while MORE_COMPONENTS is set, each with an optional
 * 2.14 transform. Point-number arguments (ARGS_ARE_XY_VALUES clear) are refused by name rather
 * than guessed: they anchor to another component's points and a wrong guess is a corrupt glyph.
 */

#define TT_MAX_POINTS   1024
#define TT_MAX_CONTOURS 64
#define TT_MAX_DEPTH    4

#define COMP_ARG_WORDS   0x0001
#define COMP_ARGS_XY     0x0002
#define COMP_HAVE_SCALE  0x0008
#define COMP_MORE        0x0020
#define COMP_XY_SCALE    0x0040
#define COMP_TWO_BY_TWO  0x0080

static int32_t f2dot14(uint16_t v) { return (int32_t)(int16_t)v; }  /* raw, scaled later */

static int glyph_range(const WubuTtf *t, uint32_t gid, const uint8_t **g, uint32_t *len)
{
    if (!t->has_glyf || gid >= t->num_glyphs) return 0;
    const uint8_t *l = t->d + t->loca.off;
    uint32_t o1, o2;
    if (t->loca_long) {
        if (!in_range(t, t->loca.off + gid * 4u, 8)) return 0;
        o1 = be32(l + gid * 4u);
        o2 = be32(l + (gid + 1) * 4u);
    } else {
        if (!in_range(t, t->loca.off + gid * 2u, 4)) return 0;
        o1 = (uint32_t)be16(l + gid * 2u) * 2u;
        o2 = (uint32_t)be16(l + (gid + 1) * 2u) * 2u;
    }
    if (o2 < o1) return 0;
    *len = o2 - o1;
    if (!in_range(t, t->glyf.off + o1, *len)) return 0;
    *g = t->d + t->glyf.off + o1;
    return 1;
}

static int load_glyph(const WubuTtf *t, uint32_t gid, WubuTtfPoint *pts, int max_pts, int *npts,
                      uint16_t *ends, int max_ends, int *nends, int dx, int dy, int depth);

static int load_simple(const WubuTtf *t, const uint8_t *g, uint32_t len, int16_t ncont,
                       WubuTtfPoint *pts, int max_pts, int *npts, uint16_t *ends, int max_ends, int *nends)
{
    (void)t;
    if (ncont <= 0) return 0;
    uint32_t need = 10u + (uint32_t)ncont * 2u;
    if (len < need) return 0;
    if (*nends + ncont > max_ends) return 0;

    uint32_t pos = 10;
    int total = 0;
    for (int i = 0; i < ncont; i++) {
        uint16_t e = be16(g + pos + (uint32_t)i * 2u);
        if (e < (uint16_t)(total ? total - 1 : 0)) return 0;
        ends[*nends + i] = e;
        total = e + 1;
    }
    pos += (uint32_t)ncont * 2u;
    if (total <= 0 || *npts + total > max_pts) return 0;

    uint16_t ilen = be16(g + pos);
    pos += 2 + ilen;
    if (pos > len) return 0;

    /* flags, with the repeat bit expanding runs */
    uint8_t fl[1024];
    if (total > (int)sizeof fl) return 0;
    int f = 0;
    while (f < total) {
        if (pos >= len) return 0;
        uint8_t b = g[pos++];
        fl[f++] = b;
        if (b & 0x08) {
            if (pos >= len) return 0;
            uint8_t rep = g[pos++];
            while (rep-- && f < total) fl[f++] = b;
        }
    }
    if (f != total) return 0;

    /* x deltas */
    int32_t acc = 0;
    for (int i = 0; i < total; i++) {
        uint8_t b = fl[i];
        if (b & 0x02) {
            if (pos >= len) return 0;
            int32_t v = g[pos++];
            acc += (b & 0x10) ? v : -v;
        } else if (!(b & 0x10)) {
            if (pos + 2 > len) return 0;
            acc += (int16_t)be16(g + pos);
            pos += 2;
        }
        pts[*npts + i].x = acc;
    }
    acc = 0;
    for (int i = 0; i < total; i++) {
        uint8_t b = fl[i];
        if (b & 0x04) {
            if (pos >= len) return 0;
            int32_t v = g[pos++];
            acc += (b & 0x20) ? v : -v;
        } else if (!(b & 0x20)) {
            if (pos + 2 > len) return 0;
            acc += (int16_t)be16(g + pos);
            pos += 2;
        }
        pts[*npts + i].y = acc;
        pts[*npts + i].on = (uint8_t)(fl[i] & 0x01);
    }

    *npts += total;
    *nends += ncont;
    return 1;
}

static int load_composite(const WubuTtf *t, const uint8_t *g, uint32_t len, uint32_t gid,
                          WubuTtfPoint *pts, int max_pts, int *npts,
                          uint16_t *ends, int max_ends, int *nends, int dx, int dy, int depth)
{
    if (depth >= TT_MAX_DEPTH) return 0;
    uint32_t pos = 10;
    uint16_t flags = 0;
    do {
        if (pos + 4 > len) return 0;
        flags = be16(g + pos);
        uint16_t comp = be16(g + pos + 2);
        pos += 4;

        int a1 = 0, a2 = 0;
        if (flags & COMP_ARG_WORDS) {
            if (pos + 4 > len) return 0;
            a1 = (int16_t)be16(g + pos);
            a2 = (int16_t)be16(g + pos + 2);
            pos += 4;
        } else {
            if (pos + 2 > len) return 0;
            a1 = (int8_t)g[pos];
            a2 = (int8_t)g[pos + 1];
            pos += 2;
        }
        if (!(flags & COMP_ARGS_XY)) return 0;  /* point-anchored: refuse, never guess */

        int32_t sx = 0x4000, sy = 0x4000, s01 = 0, s10 = 0;  /* 2.14: 0x4000 == 1.0 */
        if (flags & COMP_HAVE_SCALE) {
            if (pos + 2 > len) return 0;
            sx = sy = f2dot14(be16(g + pos));
            pos += 2;
        } else if (flags & COMP_XY_SCALE) {
            if (pos + 4 > len) return 0;
            sx = f2dot14(be16(g + pos));
            sy = f2dot14(be16(g + pos + 2));
            pos += 4;
        } else if (flags & COMP_TWO_BY_TWO) {
            if (pos + 8 > len) return 0;
            sx = f2dot14(be16(g + pos));
            s01 = f2dot14(be16(g + pos + 2));
            s10 = f2dot14(be16(g + pos + 4));
            sy = f2dot14(be16(g + pos + 6));
            pos += 8;
        }
        /* SCALED_COMPONENT_OFFSET (0x0800) is the spec default when both offset bits are clear,
         * which is what the reference implementation does; we apply the same. */
        int scale_offsets = (flags & 0x0800) || !(flags & 0x1000);

        int32_t offx = (int32_t)a1, offy = (int32_t)a2;
        if (scale_offsets) {
            offx = (int32_t)(((int64_t)a1 * sx + (int64_t)a2 * s10) / 0x4000);
            offy = (int32_t)(((int64_t)a1 * s01 + (int64_t)a2 * sy) / 0x4000);
        }

        WubuTtfPoint tmp[TT_MAX_POINTS];
        uint16_t tmpe[TT_MAX_CONTOURS];
        int tn = 0, te = 0;
        if (!load_glyph(t, comp, tmp, TT_MAX_POINTS, &tn, tmpe, TT_MAX_CONTOURS, &te,
                        dx + offx, dy + offy, depth + 1))
            return 0;
        if (*npts + tn > max_pts || *nends + te > max_ends) return 0;

        for (int i = 0; i < tn; i++) {
            int32_t x = tmp[i].x, y = tmp[i].y;
            pts[*npts + i].x = (int32_t)(((int64_t)x * sx + (int64_t)y * s10) / 0x4000);
            pts[*npts + i].y = (int32_t)(((int64_t)x * s01 + (int64_t)y * sy) / 0x4000);
            pts[*npts + i].on = tmp[i].on;
        }
        int base = *npts;
        for (int i = 0; i < te; i++) ends[*nends + i] = (uint16_t)(base + tmpe[i]);
        *npts += tn;
        *nends += te;
    } while (flags & COMP_MORE);
    (void)gid;
    return 1;
}

static int load_glyph(const WubuTtf *t, uint32_t gid, WubuTtfPoint *pts, int max_pts, int *npts,
                      uint16_t *ends, int max_ends, int *nends, int dx, int dy, int depth)
{
    const uint8_t *g = NULL;
    uint32_t len = 0;
    if (!glyph_range(t, gid, &g, &len)) return 0;
    if (len == 0) return 0;  /* empty glyph (space) */
    if (len < 10) return 0;

    int16_t ncont = (int16_t)be16(g);
    int before = *npts;
    int ok;
    if (ncont >= 0) ok = load_simple(t, g, len, ncont, pts, max_pts, npts, ends, max_ends, nends);
    else ok = load_composite(t, g, len, gid, pts, max_pts, npts, ends, max_ends, nends, dx, dy, depth);
    if (!ok) return 0;

    if (dx || dy) {
        for (int i = before; i < *npts; i++) { pts[i].x += dx; pts[i].y += dy; }
    }
    return 1;
}
