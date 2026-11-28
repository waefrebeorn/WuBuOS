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

#endif /* WUBU_TRUETYPE_H */
