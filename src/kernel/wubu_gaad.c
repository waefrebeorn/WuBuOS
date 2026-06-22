/*
 * wubu_gaad.c  --  WuBuOS Golden Aspect Adaptive Decomposition
 *
 * Cell 393: GAAD  --  the universal resolution translator.
 *
 * From bytropix: "Recursive Golden Subdivision turns any rectangle
 * into squares + golden rectangles. This gives a resolution-independent
 * coordinate system."
 *
 * All math is pure C. No libm. No floating point required for
 * the subdivision itself (it's just integer subtraction).
 * φ only appears when computing spiral points and scale factors.
 */
#include "wubu_gaad.h"
#include "wubu_math.h"

#include <string.h>
#include <stdlib.h>

/* Use pure C math when WUBU_NO_LIBM is defined; math.h included last */
#ifndef WUBU_NO_LIBM
#include <math.h>
#endif

/* -- Pure C Math Helpers ------------------------------------------ */

int wubu_isqrt(int n) {
    if (n <= 0) return 0;
    if (n < 2) return 1;
    int x = n;
    int y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

int wubu_dist(int x1, int y1, int x2, int y2) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    return wubu_isqrt(dx * dx + dy * dy);
}

double wubu_phi_pow(int n) {
    /* φ^n via recurrence: φ^0=1, φ^1=φ, φ^(n+1)=φ^n+φ^(n-1) */
    if (n == 0) return 1.0;
    if (n == 1) return WUBU_PHI;
    if (n > 0) {
        double a = 1.0, b = WUBU_PHI;
        for (int i = 2; i <= n; i++) {
            double c = a + b;  /* φ^i = φ^(i-1) + φ^(i-2) */
            a = b;
            b = c;
        }
        return b;
    }
    /* Negative: φ^(-n) = (1/φ)^n, also obeys recurrence */
    double a = 1.0, b = WUBU_PHI_INV;
    for (int i = -2; i >= n; i--) {
        double c = b - a;  /* φ^(-i) = φ^(-(i-1)) - φ^(-(i-2)) */
        /* Actually: 1/φ^n = (-1)^n * (F_n * φ - F_{n+1}) is complex.
         * Simpler: just use floating point for negative powers */
        a = b;
        b = a * WUBU_PHI_INV;
        (void)c; /* suppress unused warning */
    }
    return b;
}

int wubu_clamp(int val, int lo, int hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

/* -- Recursive Golden Subdivision --------------------------------- */

static void golden_subdivide(int x, int y, int w, int h,
                              int depth, int max_depth,
                              int cardinal_hint,
                              WubuGaadDecomp *out) {
    if (depth >= max_depth || out->n_regions >= WUBU_GAAD_MAX_REGIONS)
        return;
    if (w < 4 || h < 4)  /* Too small to subdivide further */
        return;

    WubuGaadRegion *r = &out->regions[out->n_regions];
    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;
    r->depth = depth;
    r->index = out->n_regions;
    r->cardinal = cardinal_hint;
    r->is_snap_target = (depth <= 2);  /* Top 3 levels are snap targets */

    if (w == h) {
        r->kind = WUBU_GAAD_SQUARE;
        r->phi_scale = wubu_phi_pow(-depth);
        out->n_regions++;
        return;
    }

    if (w > h) {
        /* Landscape: cut off a square from the left */
        int sq = h;  /* Square dimension = shorter side */
        r->kind = WUBU_GAAD_SQUARE;
        r->w = sq;
        r->phi_scale = wubu_phi_pow(-depth);
        out->n_regions++;

        /* Remaining is golden rectangle on the right */
        int rem_w = w - sq;
        if (rem_w > 0) {
            golden_subdivide(x + sq, y, rem_w, h,
                             depth + 1, max_depth,
                             cardinal_hint >= 0 ? cardinal_hint : (x + sq > (out->frame_w / 2) ? 1 : 3),
                             out);
        }
    } else {
        /* Portrait: cut off a square from the top */
        int sq = w;  /* Square dimension = shorter side */
        r->kind = WUBU_GAAD_SQUARE;
        r->h = sq;
        r->phi_scale = wubu_phi_pow(-depth);
        out->n_regions++;

        /* Remaining is golden rectangle below */
        int rem_h = h - sq;
        if (rem_h > 0) {
            golden_subdivide(x, y + sq, w, rem_h,
                             depth + 1, max_depth,
                             cardinal_hint >= 0 ? cardinal_hint : (y + sq > (out->frame_h / 2) ? 2 : 0),
                             out);
        }
    }
}

void wubu_gaad_decompose(int width, int height, int max_depth,
                          WubuGaadDecomp *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->frame_w = width;
    out->frame_h = height;
    out->max_depth = max_depth > 0 ? max_depth : WUBU_GAAD_MAX_DEPTH;
    out->with_spirals = false;
    out->with_cardinals = false;

    golden_subdivide(0, 0, width, height, 0, out->max_depth, -1, out);
}

/* -- Feng Shui Cardinal Mirrors ----------------------------------- */

void wubu_gaad_feng_shui_build(int frame_w, int frame_h, WubuFengShui *fs) {
    if (!fs) return;
    memset(fs, 0, sizeof(*fs));

    /*
     * The feng shui layout divides the screen into 4 cardinal regions
     * using φ-weighted splits. Each region gets 3 sub-regions.
     *
     * Vertical split: φ² : φ : 1 (total = φ²+φ+1 = 2φ+2 ≈ 5.236)
     * Horizontal split: 1 : φ : φ² (same, mirrored for balance)
     *
     * The asymmetry is deliberate feng shui:
     * - North/West get the heavy (φ²) side = commanding position
     * - South/East get the light (1) side = receptive position
     */

    double total_v = WUBU_PHI_SQ + WUBU_PHI + 1.0;  /* ≈ 5.236 */

    /* Vertical column positions (N/S) */
    int col1_w = (int)(frame_w * WUBU_PHI_SQ / total_v);  /* φ² wide */
    int col2_w = (int)(frame_w * WUBU_PHI / total_v);     /* φ wide */
    int col3_w = frame_w - col1_w - col2_w;                /* 1 wide */

    /* Horizontal row positions (E/W) */
    int row1_h = (int)(frame_h * 1.0 / total_v);          /* 1 tall */
    int row2_h = (int)(frame_h * WUBU_PHI / total_v);     /* φ tall */
    int row3_h = frame_h - row1_h - row2_h;                /* φ² tall */

    /* North: top 3 columns (commanding = left heavy) */
    for (int i = 0; i < 3; i++) {
        int cx = (i == 0) ? 0 : (i == 1) ? col1_w : col1_w + col2_w;
        int cw = (i == 0) ? col1_w : (i == 1) ? col2_w : col3_w;
        fs->north[i] = (WubuGaadRegion){
            .x = cx, .y = 0, .w = cw, .h = row1_h,
            .depth = 1, .index = i, .kind = WUBU_GAAD_GOLDEN_W,
            .phi_scale = wubu_phi_pow(-1), .cardinal = 0,
            .is_snap_target = true
        };
    }

    /* South: bottom 3 columns (receptive = right heavy) */
    int south_y = row1_h + row2_h;
    for (int i = 0; i < 3; i++) {
        int cx = (i == 0) ? 0 : (i == 1) ? col1_w : col1_w + col2_w;
        int cw = (i == 0) ? col3_w : (i == 1) ? col2_w : col1_w;  /* Mirrored */
        fs->south[i] = (WubuGaadRegion){
            .x = cx, .y = south_y, .w = cw, .h = row3_h,
            .depth = 1, .index = 3 + i, .kind = WUBU_GAAD_GOLDEN_W,
            .phi_scale = wubu_phi_pow(-1), .cardinal = 2,
            .is_snap_target = true
        };
    }

    /* West: left 3 rows (commanding = top heavy) */
    for (int i = 0; i < 3; i++) {
        int cy = (i == 0) ? 0 : (i == 1) ? row1_h : row1_h + row2_h;
        int ch = (i == 0) ? row3_h : (i == 1) ? row2_h : row1_h;  /* φ²,φ,1 */
        fs->west[i] = (WubuGaadRegion){
            .x = 0, .y = cy, .w = col1_w, .h = ch,
            .depth = 1, .index = 6 + i, .kind = WUBU_GAAD_GOLDEN_H,
            .phi_scale = wubu_phi_pow(-1), .cardinal = 3,
            .is_snap_target = true
        };
    }

    /* East: right 3 rows (receptive = bottom heavy) */
    int east_x = col1_w + col2_w;
    for (int i = 0; i < 3; i++) {
        int cy = (i == 0) ? 0 : (i == 1) ? row1_h : row1_h + row2_h;
        int ch = (i == 0) ? row1_h : (i == 1) ? row2_h : row3_h;  /* 1,φ,φ² */
        fs->east[i] = (WubuGaadRegion){
            .x = east_x, .y = cy, .w = col3_w, .h = ch,
            .depth = 1, .index = 9 + i, .kind = WUBU_GAAD_GOLDEN_H,
            .phi_scale = wubu_phi_pow(-1), .cardinal = 1,
            .is_snap_target = true
        };
    }

    /* Center: golden rectangle at the intersection */
    fs->center = (WubuGaadRegion){
        .x = col1_w, .y = row1_h,
        .w = col2_w, .h = row2_h,
        .depth = 1, .index = 12, .kind = WUBU_GAAD_SQUARE,
        .phi_scale = wubu_phi_pow(-1), .cardinal = -1,
        .is_snap_target = true
    };
}

/* -- Feng Shui Decompose ------------------------------------------ */

void wubu_gaad_decompose_feng_shui(int width, int height, int max_depth,
                                    WubuGaadDecomp *out,
                                    WubuFengShui *fs) {
    /* First do normal decomposition */
    wubu_gaad_decompose(width, height, max_depth, out);
    out->with_cardinals = true;

    /* Mark regions by cardinal direction */
    for (int i = 0; i < out->n_regions; i++) {
        WubuGaadRegion *r = &out->regions[i];
        int cx = r->x + r->w / 2;
        int cy = r->y + r->h / 2;

        if (cy < height / 3)       r->cardinal = 0;  /* N */
        else if (cy > 2*height/3)  r->cardinal = 2;  /* S */
        else if (cx < width / 3)   r->cardinal = 3;  /* W */
        else if (cx > 2*width/3)   r->cardinal = 1;  /* E */
        else                       r->cardinal = -1;  /* center */
    }

    /* Build feng shui snap layout */
    if (fs) wubu_gaad_feng_shui_build(width, height, fs);
}

/* -- Find Nearest Snap -------------------------------------------- */

int wubu_gaad_find_snap(const WubuGaadDecomp *decomp,
                         int win_x, int win_y, int win_w, int win_h,
                         int snap_threshold) {
    if (!decomp) return -1;

    int best_idx = -1;
    int best_dist = snap_threshold + 1;

    /* Check window center against each snap target region center */
    int wcx = win_x + win_w / 2;
    int wcy = win_y + win_h / 2;

    for (int i = 0; i < decomp->n_regions; i++) {
        const WubuGaadRegion *r = &decomp->regions[i];
        if (!r->is_snap_target) continue;

        int rcx = r->x + r->w / 2;
        int rcy = r->y + r->h / 2;
        int d = wubu_dist(wcx, wcy, rcx, rcy);

        if (d < best_dist) {
            best_dist = d;
            best_idx = i;
        }
    }

    return (best_dist <= snap_threshold) ? best_idx : -1;
}

void wubu_gaad_snap_pos(const WubuGaadDecomp *decomp, int region_idx,
                          int *out_x, int *out_y, int *out_w, int *out_h) {
    if (!decomp || region_idx < 0 || region_idx >= decomp->n_regions) return;
    const WubuGaadRegion *r = &decomp->regions[region_idx];
    if (out_x) *out_x = r->x;
    if (out_y) *out_y = r->y;
    if (out_w) *out_w = r->w;
    if (out_h) *out_h = r->h;
}

/* -- Feng Shui Snap ----------------------------------------------- */

bool wubu_gaad_feng_shui_snap(const WubuFengShui *fs,
                               int win_x, int win_y, int win_w, int win_h,
                               int snap_threshold,
                               int *out_x, int *out_y,
                               int *out_w, int *out_h) {
    if (!fs) return false;

    int wcx = win_x + win_w / 2;
    int wcy = win_y + win_h / 2;
    int best_dist = snap_threshold + 1;
    const WubuGaadRegion *best = NULL;

    /* Check all cardinal regions */
    for (int dir = 0; dir < 4; dir++) {
        const WubuGaadRegion *group = NULL;
        switch (dir) {
            case 0: group = fs->north; break;
            case 1: group = fs->east;  break;
            case 2: group = fs->south; break;
            case 3: group = fs->west;  break;
        }
        for (int i = 0; i < 4; i++) {
            int rcx = group[i].x + group[i].w / 2;
            int rcy = group[i].y + group[i].h / 2;
            int d = wubu_dist(wcx, wcy, rcx, rcy);
            if (d < best_dist) {
                best_dist = d;
                best = &group[i];
            }
        }
    }

    /* Check center */
    {
        int rcx = fs->center.x + fs->center.w / 2;
        int rcy = fs->center.y + fs->center.h / 2;
        int d = wubu_dist(wcx, wcy, rcx, rcy);
        if (d < best_dist) {
            best_dist = d;
            best = &fs->center;
        }
    }

    if (best && best_dist <= snap_threshold) {
        if (out_x) *out_x = best->x;
        if (out_y) *out_y = best->y;
        if (out_w) *out_w = best->w;
        if (out_h) *out_h = best->h;
        return true;
    }

    return false;
}

/* -- Phi-Spiral Sectoring ----------------------------------------- */

void wubu_gaad_add_spirals(WubuGaadDecomp *decomp,
                            int num_arms, int points_per_arm) {
    if (!decomp) return;
    decomp->with_spirals = true;

    double cx = decomp->frame_w / 2.0;
    double cy = decomp->frame_h / 2.0;
    double min_dim = decomp->frame_w < decomp->frame_h
                     ? decomp->frame_w : decomp->frame_h;
    double initial_r = min_dim * 0.05;
    double max_r = min_dim * 0.45;

    /* b = ln(φ) / (π/2)  --  ensures φ growth per 90° */
    double b = log(WUBU_PHI) / (3.14159265358979323846 / 2.0);

    for (int arm = 0; arm < num_arms; arm++) {
        double angle_offset = (2.0 * 3.14159265358979323846 / num_arms) * arm;

        /* Max angle for this arm */
        double theta_max = 4.0 * 3.14159265358979323846;  /* 2 full revolutions */
        if (initial_r > 0 && max_r > initial_r && b > 1e-6) {
            double tmax = log(max_r / initial_r) / b;
            if (tmax < theta_max) theta_max = tmax;
        }

        for (int pt = 0; pt < points_per_arm; pt++) {
            if (decomp->n_regions >= WUBU_GAAD_MAX_REGIONS) return;

            double theta = theta_max * pt / (points_per_arm - 1);
            double r = initial_r * exp(b * theta);
            if (r > max_r) break;

            double angle = angle_offset + theta;
            int px = (int)(cx + r * cos(angle));
            int py = (int)(cy + r * sin(angle));

            if (px < 0 || px >= decomp->frame_w ||
                py < 0 || py >= decomp->frame_h) continue;

            /* Store as a point-sized region */
            WubuGaadRegion *reg = &decomp->regions[decomp->n_regions];
            reg->x = px;
            reg->y = py;
            reg->w = 1;
            reg->h = 1;
            reg->depth = pt;
            reg->index = decomp->n_regions;
            reg->kind = WUBU_GAAD_SPIRAL_PT;
            reg->phi_scale = r / min_dim;
            reg->cardinal = -1;
            reg->is_snap_target = false;
            decomp->n_regions++;
        }
    }
}

/* ==================================================================
 *  Resolution Translation
 * ================================================================== */

void wubu_gaad_translate_init(int src_w, int src_h,
                               int dst_w, int dst_h,
                               int max_depth,
                               WubuGaadTranslate *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->src_w = src_w;
    out->src_h = src_h;
    out->dst_w = dst_w;
    out->dst_h = dst_h;

    wubu_gaad_decompose(src_w, src_h, max_depth, &out->src_decomp);
    wubu_gaad_decompose(dst_w, dst_h, max_depth, &out->dst_decomp);
}

void wubu_gaad_translate_pixel(const WubuGaadTranslate *t,
                                int src_x, int src_y,
                                int *dst_x, int *dst_y) {
    if (!t || !dst_x || !dst_y) return;

    /*
     * Strategy: Find which source GAAD region the pixel falls in,
     * compute (u,v) within that region [0,1]², then map to the
     * corresponding target GAAD region using the same (u,v).
     *
     * For regions at the same subdivision depth, this is a
     * direct correspondence. For mismatched depths, we find
     * the best overlapping target region.
     *
     * Simple version: use the fractional position within the
     * overall frame, weighted by the φ-structured region map.
     * This preserves the golden geometry.
     */

    /* Convert source pixel to normalized [0,1] coordinates */
    double u = (double)src_x / t->src_w;
    double v = (double)src_y / t->src_h;

    /* Clamp to [0, 1) */
    if (u < 0.0) u = 0.0;
    if (u >= 1.0) u = 1.0 - 1e-9;
    if (v < 0.0) v = 0.0;
    if (v >= 1.0) v = 1.0 - 1e-9;

    /* Find the source GAAD region */
    int src_region = -1;
    for (int i = 0; i < t->src_decomp.n_regions; i++) {
        const WubuGaadRegion *r = &t->src_decomp.regions[i];
        if (src_x >= r->x && src_x < r->x + r->w &&
            src_y >= r->y && src_y < r->y + r->h) {
            src_region = i;
            break;
        }
    }

    /* Compute local (u,v) within the source region */
    double local_u = u, local_v = v;
    if (src_region >= 0) {
        const WubuGaadRegion *sr = &t->src_decomp.regions[src_region];
        if (sr->w > 0) local_u = (double)(src_x - sr->x) / sr->w;
        if (sr->h > 0) local_v = (double)(src_y - sr->y) / sr->h;
    }

    /* Find the corresponding target GAAD region */
    int dst_region = -1;
    if (src_region >= 0 && src_region < t->dst_decomp.n_regions) {
        dst_region = src_region;
    } else {
        /* Find by normalized position match */
        for (int i = 0; i < t->dst_decomp.n_regions; i++) {
            const WubuGaadRegion *r = &t->dst_decomp.regions[i];
            double ru = (double)(r->x + r->w/2) / t->dst_w;
            double rv = (double)(r->y + r->h/2) / t->dst_h;
            if (fabs(ru - u) < 0.2 && fabs(rv - v) < 0.2) {
                dst_region = i;
                break;
            }
        }
    }

    /* Map to target pixel */
    if (dst_region >= 0) {
        const WubuGaadRegion *dr = &t->dst_decomp.regions[dst_region];
        *dst_x = dr->x + (int)(local_u * dr->w);
        *dst_y = dr->y + (int)(local_v * dr->h);
    } else {
        /* Fallback: simple linear scaling */
        *dst_x = (int)(u * t->dst_w);
        *dst_y = (int)(v * t->dst_h);
    }
}

void wubu_gaad_translate_inverse(const WubuGaadTranslate *t,
                                  int dst_x, int dst_y,
                                  int *src_x, int *src_y) {
    if (!t || !src_x || !src_y) return;

    /* Inverse: find target GAAD region, compute local (u,v),
     * map to source region at same (u,v) */
    double u = (double)dst_x / t->dst_w;
    double v = (double)dst_y / t->dst_h;

    if (u < 0.0) u = 0.0;
    if (u >= 1.0) u = 1.0 - 1e-9;
    if (v < 0.0) v = 0.0;
    if (v >= 1.0) v = 1.0 - 1e-9;

    int dst_region = -1;
    for (int i = 0; i < t->dst_decomp.n_regions; i++) {
        const WubuGaadRegion *r = &t->dst_decomp.regions[i];
        if (dst_x >= r->x && dst_x < r->x + r->w &&
            dst_y >= r->y && dst_y < r->y + r->h) {
            dst_region = i;
            break;
        }
    }

    double local_u = u, local_v = v;
    if (dst_region >= 0) {
        const WubuGaadRegion *dr = &t->dst_decomp.regions[dst_region];
        if (dr->w > 0) local_u = (double)(dst_x - dr->x) / dr->w;
        if (dr->h > 0) local_v = (double)(dst_y - dr->y) / dr->h;
    }

    int src_region = dst_region;  /* Correspondence */
    if (src_region >= 0 && src_region < t->src_decomp.n_regions) {
        const WubuGaadRegion *sr = &t->src_decomp.regions[src_region];
        *src_x = sr->x + (int)(local_u * sr->w);
        *src_y = sr->y + (int)(local_v * sr->h);
    } else {
        *src_x = (int)(u * t->src_w);
        *src_y = (int)(v * t->src_h);
    }
}

void wubu_gaad_translate_rect(const WubuGaadTranslate *t,
                               int src_x, int src_y, int src_w, int src_h,
                               int *dst_x, int *dst_y,
                               int *dst_w, int *dst_h) {
    if (!t) return;

    int dx1, dy1, dx2, dy2;
    wubu_gaad_translate_pixel(t, src_x, src_y, &dx1, &dy1);
    wubu_gaad_translate_pixel(t, src_x + src_w, src_y + src_h, &dx2, &dy2);

    if (dst_x) *dst_x = dx1;
    if (dst_y) *dst_y = dy1;
    if (dst_w) *dst_w = dx2 - dx1;
    if (dst_h) *dst_h = dy2 - dy1;
}

double wubu_gaad_region_scale(const WubuGaadTranslate *t, int region_idx) {
    if (!t) return 1.0;
    if (region_idx < 0) return 1.0;

    /* Ratio of target region area to source region area */
    if (region_idx < t->src_decomp.n_regions &&
        region_idx < t->dst_decomp.n_regions) {
        const WubuGaadRegion *sr = &t->src_decomp.regions[region_idx];
        const WubuGaadRegion *dr = &t->dst_decomp.regions[region_idx];
        int src_area = sr->w * sr->h;
        int dst_area = dr->w * dr->h;
        if (src_area > 0) return (double)dst_area / src_area;
    }

    /* Fallback: overall scale */
    return (double)(t->dst_w * t->dst_h) / (t->src_w * t->src_h);
}
