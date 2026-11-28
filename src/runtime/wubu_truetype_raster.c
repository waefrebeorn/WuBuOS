/* wubu_truetype_raster.c -- quadratic flattening and scanline coverage (SLERM-2027 stage 2).
 *
 * Input is outline data only (points + contour ends in font units), so this file knows nothing
 * about SFNT: rasterising is a separate concern from parsing, and a caller can rasterise an
 * outline that came from anywhere.
 *
 * TrueType contour rules (OpenType 'glyf' / TrueType fundamentals), all three of which a naive
 * walk gets wrong:
 *   1. consecutive OFF-curve points imply an ON-curve point midway between them;
 *   2. a contour whose first point is OFF-curve starts at the midpoint of (last, first);
 *   3. consecutive ON-curve points are a straight LINE, not a degenerate curve.
 * Rule 3 is the one that cost a debugging round here: 'A', 'M' and 'W' rendered zero ink because
 * only control-point sequences were emitted as segments.
 *
 * Coverage: 4 vertical sub-samples per pixel row with analytic horizontal coverage per span. That
 * is an approximation of exact area coverage, so the test compares against FreeType and pins the
 * tolerance it measures -- the number lives in the gate, not in this comment.
 */
#include "wubu_truetype.h"

#define TT_SUB 8          /* vertical sub-samples per pixel row */
#define TT_SEGS 16        /* line segments per quadratic */
#define TT_MAX_SEG 8192
#define TT_MAX_PTS 1024

typedef struct { float x0, y0, x1, y1; int dir; } Seg;

static int ifloor(float v) { int i = (int)v; return (v < 0.0f && (float)i != v) ? i - 1 : i; }
static int iceil(float v) { int i = (int)v; return (v > 0.0f && (float)i != v) ? i + 1 : i; }

static float px_x(int32_t v, int upem, int ppem) { return (float)v * (float)ppem / (float)upem; }
static float px_y(int32_t v, int upem, int ppem) { return -(float)v * (float)ppem / (float)upem; }

static int emit_line(Seg *s, int n, float x0, float y0, float x1, float y1)
{
    if (y0 == y1) return n;          /* horizontal edges never cross a scanline */
    if (n >= TT_MAX_SEG) return n;
    s[n].x0 = x0; s[n].y0 = y0; s[n].x1 = x1; s[n].y1 = y1;
    s[n].dir = (y1 > y0) ? 1 : -1;
    return n + 1;
}

static int emit_quad(Seg *s, int n, float x0, float y0, float cx, float cy, float x1, float y1)
{
    float px = x0, py = y0;
    for (int i = 1; i <= TT_SEGS; i++) {
        float t = (float)i / (float)TT_SEGS, mt = 1.0f - t;
        float x = mt * mt * x0 + 2.0f * mt * t * cx + t * t * x1;
        float y = mt * mt * y0 + 2.0f * mt * t * cy + t * t * y1;
        n = emit_line(s, n, px, py, x, y);
        px = x; py = y;
    }
    return n;
}

static void add_span(uint8_t *cov, int cw, int row, float x0, float x1, float weight)
{
    if (x1 <= x0) return;
    if (x0 < 0.0f) x0 = 0.0f;
    if (x1 > (float)cw) x1 = (float)cw;
    if (x1 <= x0) return;

    int ix0 = (int)x0, ix1 = (int)x1;
    if (ix1 >= cw) ix1 = cw - 1;
    if (ix1 < ix0) return;

    if (ix0 == ix1) {
        float c = cov[row * cw + ix0] + (x1 - x0) * weight * 255.0f;
        cov[row * cw + ix0] = (uint8_t)(c > 255.0f ? 255.0f : c);
        return;
    }
    float c = cov[row * cw + ix0] + ((float)(ix0 + 1) - x0) * weight * 255.0f;
    cov[row * cw + ix0] = (uint8_t)(c > 255.0f ? 255.0f : c);
    for (int x = ix0 + 1; x < ix1; x++) {
        float cc = cov[row * cw + x] + weight * 255.0f;
        cov[row * cw + x] = (uint8_t)(cc > 255.0f ? 255.0f : cc);
    }
    float cl = cov[row * cw + ix1] + (x1 - (float)ix1) * weight * 255.0f;
    cov[row * cw + ix1] = (uint8_t)(cl > 255.0f ? 255.0f : cl);
}

int wubu_ttf_raster_off(const WubuTtfPoint *pts, int npts, const uint16_t *contour_end, int ncontours,
                        int upem, int ppem, float pen_x, float pen_y, uint8_t *cov, int cw, int ch,
                        int *out_w, int *out_h, int *out_bx, int *out_by)
{
    static Seg seg[TT_MAX_SEG];
    static float gx[TT_MAX_PTS], gy[TT_MAX_PTS];
    static uint8_t gon[TT_MAX_PTS];

    if (!pts || !contour_end || !cov || !out_w || !out_h) return 0;
    if (npts <= 0 || ncontours <= 0 || upem <= 0 || ppem <= 0 || npts > TT_MAX_PTS) return 0;

    /* Bounding box from the outline's own points; the ink box can be smaller, and the caller
     * gets the conservative one so nothing is clipped. */
    int32_t xmin = pts[0].x, xmax = pts[0].x, ymin = pts[0].y, ymax = pts[0].y;
    for (int i = 1; i < npts; i++) {
        if (pts[i].x < xmin) xmin = pts[i].x;
        if (pts[i].x > xmax) xmax = pts[i].x;
        if (pts[i].y < ymin) ymin = pts[i].y;
        if (pts[i].y > ymax) ymax = pts[i].y;
    }
    /* Integer-pixel box that contains the subpixel-shifted outline. The shift is why the box is
     * computed from CEIL/floor rather than by rounding one width: at pen_x = .25 the glyph covers
     * parts of both neighbouring pixels. */
    float fx0 = px_x(xmin, upem, ppem) + pen_x, fx1 = px_x(xmax, upem, ppem) + pen_x;
    /* px_y flips the axis, so ymax maps to the TOP of the box. Getting this backwards collapses
     * every glyph to a one-pixel-tall empty bitmap. */
    float fy_top = px_y(ymax, upem, ppem) + pen_y, fy_bot = px_y(ymin, upem, ppem) + pen_y;
    int bx = ifloor(fx0), by = ifloor(fy_top);
    int w = iceil(fx1) - bx, h = iceil(fy_bot) - by;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    float ox = (float)bx, oy = (float)by;
    if (w > cw || h > ch) return 0;

    int nseg = 0, start = 0;
    for (int c = 0; c < ncontours; c++) {
        int end = contour_end[c];
        if (end < start || end >= npts) return 0;
        int np = end - start + 1;

        for (int k = 0; k < np; k++) {
            gx[k] = px_x(pts[start + k].x, upem, ppem) + pen_x - ox;
            gy[k] = px_y(pts[start + k].y, upem, ppem) + pen_y - oy;
            gon[k] = pts[start + k].on;
        }

        /* Rule 2: choose the contour's start point. */
        int idx0 = 1;
        float sx, sy;
        if (gon[0]) { sx = gx[0]; sy = gy[0]; }
        else if (gon[np - 1]) { sx = gx[np - 1]; sy = gy[np - 1]; idx0 = 0; }
        else { sx = (gx[np - 1] + gx[0]) * 0.5f; sy = (gy[np - 1] + gy[0]) * 0.5f; idx0 = 0; }

        float curx = sx, cury = sy, cx = 0, cy = 0;
        int have_ctrl = 0;

        for (int i = 0; i < np; i++) {
            int k = (idx0 + i) % np;
            float x = gx[k], y = gy[k];
            if (gon[k]) {
                if (have_ctrl) { nseg = emit_quad(seg, nseg, curx, cury, cx, cy, x, y); have_ctrl = 0; }
                else nseg = emit_line(seg, nseg, curx, cury, x, y);      /* rule 3 */
                curx = x; cury = y;
            } else {
                if (have_ctrl) {                                          /* rule 1 */
                    float mx = (cx + x) * 0.5f, my = (cy + y) * 0.5f;
                    nseg = emit_quad(seg, nseg, curx, cury, cx, cy, mx, my);
                    curx = mx; cury = my;
                }
                cx = x; cy = y; have_ctrl = 1;
            }
        }
        if (have_ctrl) nseg = emit_quad(seg, nseg, curx, cury, cx, cy, sx, sy);
        else nseg = emit_line(seg, nseg, curx, cury, sx, sy);

        start = end + 1;
        if (nseg >= TT_MAX_SEG - TT_SEGS) break;
    }
    if (nseg == 0) return 0;

    for (int i = 0; i < w * h; i++) cov[i] = 0;

    float xs[TT_MAX_SEG];
    int dirs[TT_MAX_SEG];
    for (int row = 0; row < h; row++) {
        for (int sub = 0; sub < TT_SUB; sub++) {
            float sy2 = (float)row + ((float)sub + 0.5f) / (float)TT_SUB;
            int nx = 0;
            for (int i = 0; i < nseg && nx < TT_MAX_SEG; i++) {
                float y0 = seg[i].y0, y1 = seg[i].y1;
                if ((sy2 >= y0 && sy2 < y1) || (sy2 >= y1 && sy2 < y0)) {
                    float t = (sy2 - y0) / (y1 - y0);
                    xs[nx] = seg[i].x0 + t * (seg[i].x1 - seg[i].x0);
                    dirs[nx] = seg[i].dir;
                    nx++;
                }
            }
            if (nx < 2) continue;
            for (int i = 1; i < nx; i++) {      /* few crossings per row: insertion sort */
                float x = xs[i]; int d = dirs[i]; int j = i - 1;
                while (j >= 0 && xs[j] > x) { xs[j + 1] = xs[j]; dirs[j + 1] = dirs[j]; j--; }
                xs[j + 1] = x; dirs[j + 1] = d;
            }
            int wind = 0;
            for (int i = 0; i < nx - 1; i++) {
                wind += dirs[i];
                if (wind != 0) add_span(cov, w, row, xs[i], xs[i + 1], 1.0f / (float)TT_SUB);
            }
        }
    }

    *out_w = w;
    *out_h = h;
    if (out_bx) *out_bx = bx;
    if (out_by) *out_by = by;
    return 1;
}

/* Convenience: integer pen position, bearings discarded. */
int wubu_ttf_raster(const WubuTtfPoint *pts, int npts, const uint16_t *contour_end, int ncontours,
                    int upem, int ppem, uint8_t *cov, int cw, int ch, int *out_w, int *out_h)
{
    return wubu_ttf_raster_off(pts, npts, contour_end, ncontours, upem, ppem, 0.0f, 0.0f,
                               cov, cw, ch, out_w, out_h, NULL, NULL);
}
