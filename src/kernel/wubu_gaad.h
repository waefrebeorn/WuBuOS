/*
 * wubu_gaad.h — WuBuOS Golden Aspect Adaptive Decomposition
 *
 * Cell 393: GAAD — the universal resolution translator.
 *
 * From bytropix THEORY/papers/GAAD-WuBu-ST1.md:
 *   "GAAD provides a multi-scale, aspect-ratio agnostic method
 *    for decomposing frames into geometrically significant
 *    regions based on φ."
 *
 * This is NOT just window snapping. This is the math that solves:
 *
 *   1. TempleOS 640×480 → any modern resolution
 *      (map TempleOS pixel regions through GAAD translate)
 *   2. Linux DRM/KMS arbitrary mode switches
 *      (decompose target, map source regions → target regions)
 *   3. WSL2 windowed mode scaling
 *      (GAAD regions scale proportionally by φ)
 *   4. Container resolution passthrough
 *      (container gets GAAD-translated viewport)
 *   5. Window snap in 4 cardinal feng shui mirrors
 *      (golden subdivision creates asymmetric artsy snap targets)
 *   6. Full free grid while dragging, snap on release
 *      (GAAD regions only activate at drag-end)
 *
 * The key insight: Recursive Golden Subdivision turns ANY
 * rectangle into squares + golden rectangles. This gives us
 * a resolution-independent coordinate system. We decompose
 * both source and target resolutions into GAAD regions,
 * then map source→target through the region correspondence.
 *
 * φ = (1 + √5) / 2 ≈ 1.6180339887
 */
#ifndef WUBU_GAAD_H
#define WUBU_GAAD_H

#include <stdint.h>
#include <stdbool.h>

/* ── Golden Ratio Constants ─────────────────────────────────────── */

#define WUBU_PHI       1.6180339887498948482   /* φ = (1+√5)/2 */
#define WUBU_PHI_INV   0.6180339887498948482   /* 1/φ = φ-1 */
#define WUBU_PHI_SQ    2.6180339887498948482   /* φ² = φ+1 */

/* ── GAAD Region ────────────────────────────────────────────────── */

#define WUBU_GAAD_MAX_REGIONS  64
#define WUBU_GAAD_MAX_DEPTH    6

typedef enum {
    WUBU_GAAD_SQUARE    = 0,   /* Square region from golden subdivision */
    WUBU_GAAD_GOLDEN_W  = 1,   /* Golden rect wider than tall (φ:1) */
    WUBU_GAAD_GOLDEN_H  = 2,   /* Golden rect taller than wide (1:φ) */
    WUBU_GAAD_SPIRAL_PT = 3,   /* Φ-spiral sector center point */
} WubuGaadKind;

typedef struct {
    int      x, y, w, h;       /* Region coordinates in parent space */
    int      depth;            /* Subdivision depth (0 = full frame) */
    int      index;            /* Unique region index */
    WubuGaadKind kind;         /* Square, golden rect, spiral point */
    double   phi_scale;        /* φ^n scale factor for this region */
    int      cardinal;         /* 0=N, 1=E, 2=S, 3=W, -1=center */
    bool     is_snap_target;   /* True = window can snap here */
} WubuGaadRegion;

/* ── GAAD Decomposition ─────────────────────────────────────────── */

typedef struct {
    WubuGaadRegion regions[WUBU_GAAD_MAX_REGIONS];
    int            n_regions;
    int            frame_w, frame_h;   /* Source frame dimensions */
    int            max_depth;          /* Subdivision depth */
    bool           with_spirals;       /* Include phi-spiral points */
    bool           with_cardinals;     /* Mark cardinal mirror regions */
} WubuGaadDecomp;

/* ── GAAD Coordinate (resolution-independent) ───────────────────── */

/*
 * A GAAD coordinate is a region index + (u,v) offset within [0,1]².
 * This is resolution-independent — the same (region_idx, u, v) maps
 * to different pixel positions at different resolutions.
 *
 * This is how we translate TempleOS 640×480 → 1920×1080:
 *   1. GAAD-decompose 640×480 → source regions
 *   2. GAAD-decompose 1920×1080 → target regions
 *   3. Map (region_idx, u, v) from source → target
 */
typedef struct {
    int    region_idx;         /* Which GAAD region */
    double u, v;              /* Position within region [0,1]² */
} WubuGaadCoord;

/* ── Feng Shui Cardinal Mirrors ─────────────────────────────────── */

/*
 * The 4 cardinal mirrors create asymmetric snap regions.
 * Each cardinal direction gets φ-weighted regions:
 *
 *   North (top):    φ² : φ : 1 vertical split
 *   East (right):   1 : φ : φ² horizontal split
 *   South (bottom): 1 : φ : φ² vertical split
 *   West (left):    φ² : φ : 1 horizontal split
 *
 * This creates "artsy and quaint" snap positions that
 * feel natural (φ-based) rather than rigid (½-based).
 *
 * The asymmetry between N/S and E/W mirrors is deliberate
 * feng shui — the heavy side faces the commanding position.
 */
typedef struct {
    WubuGaadRegion north[4];   /* Top regions (φ², φ, 1 vertical) */
    WubuGaadRegion east[4];    /* Right regions (1, φ, φ² horizontal) */
    WubuGaadRegion south[4];   /* Bottom regions (1, φ, φ² vertical) */
    WubuGaadRegion west[4];    /* Left regions (φ², φ, 1 horizontal) */
    WubuGaadRegion center;     /* Golden center region */
} WubuFengShui;

/* ── Resolution Translation ─────────────────────────────────────── */

typedef struct {
    int src_w, src_h;          /* Source resolution (e.g., 640×480) */
    int dst_w, dst_h;          /* Target resolution (e.g., 1920×1080) */
    WubuGaadDecomp src_decomp; /* GAAD decomposition of source */
    WubuGaadDecomp dst_decomp; /* GAAD decomposition of target */
} WubuGaadTranslate;

/* ══════════════════════════════════════════════════════════════════
 *  API: Golden Subdivision
 * ══════════════════════════════════════════════════════════════════ */

/*
 * Decompose a rectangle into GAAD regions via Recursive Golden Subdivision.
 *
 * At each depth, the rectangle is split into:
 *   - A square (the dominant region)
 *   - A remaining golden rectangle (recursively subdivided)
 *
 * Works for ANY aspect ratio. No rounding, no distortion.
 * This IS the aspect-ratio agnostic coordinate system.
 */
void wubu_gaad_decompose(int width, int height, int max_depth,
                          WubuGaadDecomp *out);

/*
 * Decompose with feng shui cardinal mirrors.
 * First decomposes normally, then marks regions by cardinal
 * direction and creates the 4-mirror snap layout.
 */
void wubu_gaad_decompose_feng_shui(int width, int height, int max_depth,
                                    WubuGaadDecomp *out,
                                    WubuFengShui *fs);

/*
 * Find the nearest GAAD snap region for a window position.
 * Used by WM when drag ends → snap to nearest GAAD region.
 *
 * Returns: index of nearest snap target, or -1 if none close enough.
 * snap_threshold: max pixel distance to consider snapping.
 */
int wubu_gaad_find_snap(const WubuGaadDecomp *decomp,
                         int win_x, int win_y, int win_w, int win_h,
                         int snap_threshold);

/*
 * Get snap position for a region index.
 * Fills out_x, out_y with the top-left of the region.
 */
void wubu_gaad_snap_pos(const WubuGaadDecomp *decomp, int region_idx,
                          int *out_x, int *out_y, int *out_w, int *out_h);

/* ══════════════════════════════════════════════════════════════════
 *  API: Phi-Spiral Sectoring
 * ══════════════════════════════════════════════════════════════════ */

/*
 * Add phi-spiral sector points to an existing decomposition.
 * Spirals emanate from center, growing at φ-proportions.
 * num_arms: spiral arms (4 = cardinal, 5 = phi, 8 = fine)
 * points_per_arm: samples along each arm
 */
void wubu_gaad_add_spirals(WubuGaadDecomp *decomp,
                            int num_arms, int points_per_arm);

/* ══════════════════════════════════════════════════════════════════
 *  API: Resolution Translation
 * ══════════════════════════════════════════════════════════════════ */

/*
 * Create a resolution translator.
 * Decomposes both source and target into GAAD regions.
 */
void wubu_gaad_translate_init(int src_w, int src_h,
                               int dst_w, int dst_h,
                               int max_depth,
                               WubuGaadTranslate *out);

/*
 * Translate a pixel coordinate from source → target resolution.
 *
 * This is how TempleOS 640×480 content appears at 1920×1080:
 *   1. Find which GAAD region the source pixel falls in
 *   2. Compute (u,v) position within that region [0,1]²
 *   3. Map to the corresponding target GAAD region
 *   4. Convert (u,v) back to target pixel coordinates
 *
 * This preserves the φ-structured geometry of the content.
 * No stretching, no black bars, no integer scaling artifacts.
 */
void wubu_gaad_translate_pixel(const WubuGaadTranslate *t,
                                int src_x, int src_y,
                                int *dst_x, int *dst_y);

/*
 * Translate a pixel coordinate from target → source (inverse).
 * Used for input: mouse at (dst_x, dst_y) → which source pixel?
 */
void wubu_gaad_translate_inverse(const WubuGaadTranslate *t,
                                  int dst_x, int dst_y,
                                  int *src_x, int *src_y);

/*
 * Translate an entire rectangle (for blitting/rendering).
 */
void wubu_gaad_translate_rect(const WubuGaadTranslate *t,
                               int src_x, int src_y, int src_w, int src_h,
                               int *dst_x, int *dst_y,
                               int *dst_w, int *dst_h);

/*
 * Get the scale factor for a given GAAD region.
 * This tells you how much bigger/smaller a region is in the target.
 */
double wubu_gaad_region_scale(const WubuGaadTranslate *t, int region_idx);

/* ══════════════════════════════════════════════════════════════════
 *  API: Feng Shui Snap Layout
 * ══════════════════════════════════════════════════════════════════ */

/*
 * Build the feng shui cardinal mirror snap layout.
 * Creates the 4 cardinal snap targets:
 *   - North: top-center, φ²:φ:1 split vertically
 *   - East:  right-center, 1:φ:φ² split horizontally
 *   - South: bottom-center, 1:φ:φ² split vertically
 *   - West:  left-center, φ²:φ:1 split horizontally
 *   - Center: golden rectangle at center
 */
void wubu_gaad_feng_shui_build(int frame_w, int frame_h,
                                WubuFengShui *fs);

/*
 * Find the nearest feng shui snap position for a window.
 * Checks all 4 cardinal mirrors + center golden rect.
 *
 * Returns: true if a snap was found, fills out position.
 */
bool wubu_gaad_feng_shui_snap(const WubuFengShui *fs,
                               int win_x, int win_y, int win_w, int win_h,
                               int snap_threshold,
                               int *out_x, int *out_y,
                               int *out_w, int *out_h);

/* ══════════════════════════════════════════════════════════════════
 *  API: Pure C Math (no libm)
 * ══════════════════════════════════════════════════════════════════ */

/* Integer square root (Newton's method) */
int wubu_isqrt(int n);

/* Distance between two points (integer) */
int wubu_dist(int x1, int y1, int x2, int y2);

/* φ^n for integer n (computed via recurrence, no fp pow) */
double wubu_phi_pow(int n);

/* Clamp integer to range */
int wubu_clamp(int val, int lo, int hi);

#endif /* WUBU_GAAD_H */
