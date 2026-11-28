/* wubu_truetype.h -- our own SFNT/TrueType reader (SLERM-2027).
 *
 * Stage 1 (this file's implementation): table directory, head/maxp/hhea/hmtx/loca, and cmap
 * formats 4 and 12, exposing units-per-em, glyph count, glyph index for a code point, and the
 * advance width in font units. Stage 2 adds glyf outline decode and rasterising.
 *
 * Opaque by design: callers never see table offsets or tolerances, so a layout change cannot
 * become an ABI change. Every accessor takes a const pointer and returns a value; nothing here
 * allocates except wubu_ttf_open(), which copies nothing and points at the caller's bytes.
 *
 * The bytes must outlive the handle. wubu_ttf_open() never writes to them.
 */
#ifndef WUBU_TRUETYPE_H
#define WUBU_TRUETYPE_H

#include <stddef.h>
#include <stdint.h>

typedef struct WubuTtf WubuTtf;

/* Parse an SFNT containing TrueType outlines. Returns NULL and writes a reason to `why`
 * (may be NULL) when the data is not a usable font: short data, unknown version, missing
 * required table, or a table range outside the buffer. Refuse, never guess. */
WubuTtf *wubu_ttf_open(const uint8_t *data, size_t len, const char **why);

/* No allocation is owned by the handle, so this only forgets it. NULL is a no-op. */
void wubu_ttf_close(WubuTtf *ttf);

uint16_t wubu_ttf_units_per_em(const WubuTtf *ttf);
uint16_t wubu_ttf_num_glyphs(const WubuTtf *ttf);

/* Glyph index for a Unicode code point. Prefers the 32-bit cmap (format 12) when a font has
 * both, per the OpenType cmap rules: the 32-bit subtable must be a superset, so using the
 * 16-bit one would silently lose characters. Returns 0 (.notdef) when unmapped. */
uint32_t wubu_ttf_glyph_index(const WubuTtf *ttf, uint32_t cp);

/* Advance width in font units. Returns 0 when the glyph id is out of range. */
uint32_t wubu_ttf_advance_units(const WubuTtf *ttf, uint32_t gid);

/* Advance in whole pixels at `ppem`, rounded half up -- the same rounding a rasteriser needs.
 * Integer maths only: (units * ppem + upem/2) / upem. */
int wubu_ttf_advance_px(const WubuTtf *ttf, uint32_t gid, int ppem);

/* True when the font carries glyf/loca (TrueType outlines) rather than CFF. */
int wubu_ttf_has_glyf(const WubuTtf *ttf);

/* ---- stage 2: outlines and coverage ---- */

#define WUBU_TTF_MAX_POINTS   1024
#define WUBU_TTF_MAX_CONTOURS 64

/* One outline point in font units; (0,0) is the glyph origin on the baseline, y grows upward. */
typedef struct { int32_t x, y; uint8_t on; } WubuTtfPoint;

/* Decode a glyph's outline. Simple glyphs and composites with xy offsets and 2.14 transforms are
 * handled; composites anchored to another component's POINT NUMBERS are refused (return 0), because
 * a wrong guess produces a corrupt glyph rather than a visible error. */
/* True when the face carries usable outlines (glyf + loca both present and in range). A face can
 * legitimately be opened metrics-only -- measuring text needs advances, not shapes -- but the caller
 * must be able to tell the difference instead of discovering it when a glyph draws nothing. */
int wubu_ttf_has_outlines(const WubuTtf *ttf);

int wubu_ttf_outline(const WubuTtf *ttf, uint32_t gid, WubuTtfPoint *pts, int max_pts, int *out_npts,
                     uint16_t *contour_end, int max_contours, int *out_ncontours);

/* Rasterise an outline into 8-bit coverage for the glyph's bounding box at `ppem`.
 * cov must hold cw*ch bytes and is fully overwritten. Returns 1 on success. */
int wubu_ttf_raster(const WubuTtfPoint *pts, int npts, const uint16_t *contour_end, int ncontours,
                    int upem, int ppem, uint8_t *cov, int cw, int ch, int *out_w, int *out_h);

/* Same, at a fractional pen position (pen_x/pen_y in pixels). out_bx/out_by receive the integer
 * offset of the returned box from the pen origin, which is what a caller needs to place the glyph
 * without re-deriving the bounding box. */
int wubu_ttf_raster_off(const WubuTtfPoint *pts, int npts, const uint16_t *contour_end, int ncontours,
                        int upem, int ppem, float pen_x, float pen_y, uint8_t *cov, int cw, int ch,
                        int *out_w, int *out_h, int *out_bx, int *out_by);

#endif /* WUBU_TRUETYPE_H */
