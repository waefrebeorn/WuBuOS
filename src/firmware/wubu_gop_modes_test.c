/*
 * wubu_gop_modes_test.c -- gate for firmware GOP mode decoding.
 *
 * The RESOLUTIONS are this machine's real panel modes (1920x1080 and 1600x900, read
 * from its EDID/mode list), framed in the UEFI EFI_GRAPHICS_OUTPUT_MODE_INFORMATION
 * layout.  That keeps two kinds of honesty separate: the framing is the published UEFI
 * specification, while the values come from hardware rather than invented round numbers.
 */
#include <stdio.h>
#include <string.h>

#include "wubu_gop_modes.h"

static int checks_run = 0, checks_failed = 0;

#define TEST(cond, msg) do { \
    checks_run++; \
    if (!(cond)) { checks_failed++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } \
} while (0)

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* Build one record exactly as firmware would lay it out. */
static void build_mode(uint8_t *buf, int h, int v, int format, int stride,
                       uint32_t rm, uint32_t gm, uint32_t bm)
{
    memset(buf, 0, 36);
    put_u32(buf + 0, 0);            /* Version must be zero */
    put_u32(buf + 4, (uint32_t)h);
    put_u32(buf + 8, (uint32_t)v);
    put_u32(buf + 12, (uint32_t)format);
    put_u32(buf + 16, rm);
    put_u32(buf + 20, gm);
    put_u32(buf + 24, bm);
    put_u32(buf + 28, 0);
    put_u32(buf + 32, (uint32_t)stride);
}

int main(void)
{
    wubu_gop_mode_t m;
    uint8_t rec[36];

    /* -- the real panel's preferred mode --------------------------------- */
    build_mode(rec, 1920, 1080, PixelRedGreenBlueReserved8BitPerColor, 1920, 0, 0, 0);
    TEST(wubu_gop_decode_mode(rec, sizeof(rec), sizeof(rec), &m),
         "a real 1920x1080 GOP record decodes");
    TEST(m.valid && m.h == 1920 && m.v == 1080, "resolution survives framing");
    TEST(m.stride == 1920, "stride carried separately from width");
    TEST(m.has_framebuffer, "RGB format implies a linear framebuffer");
    TEST(m.format == PixelRedGreenBlueReserved8BitPerColor, "pixel format preserved");

    /* -- padded stride: the reason stride exists at all ------------------ */
    build_mode(rec, 1600, 900, PixelRedGreenBlueReserved8BitPerColor, 1632, 0, 0, 0);
    TEST(wubu_gop_decode_mode(rec, sizeof(rec), sizeof(rec), &m),
         "a padded-stride record decodes");
    TEST(m.h == 1600 && m.stride == 1632,
         "stride wider than visible width is kept, not clamped");

    /* -- bitmask format carries colour planes --------------------------- */
    build_mode(rec, 1920, 1080, PixelBitMask, 1920, 0x00FF0000, 0x0000FF00, 0x000000FF);
    TEST(wubu_gop_decode_mode(rec, sizeof(rec), sizeof(rec), &m),
         "bitmask record decodes");
    TEST(m.r_mask == 0x00FF0000 && m.g_mask == 0x0000FF00 && m.b_mask == 0x000000FF,
         "red/green/blue masks land in the right places");

    /* -- BltOnly has no linear framebuffer ------------------------------ */
    build_mode(rec, 1920, 1080, PixelBltOnly, 1920, 0, 0, 0);
    TEST(wubu_gop_decode_mode(rec, sizeof(rec), sizeof(rec), &m),
         "BltOnly record decodes");
    TEST(!m.has_framebuffer, "BltOnly correctly reports no linear buffer");

    /* -- refusals ------------------------------------------------------- */
    build_mode(rec, 1920, 1080, PixelRedGreenBlueReserved8BitPerColor, 1920, 0, 0, 0);
    put_u32(rec + 0, 1);                        /* future Version */
    TEST(!wubu_gop_decode_mode(rec, sizeof(rec), sizeof(rec), &m),
         "non-zero Version refused");
    TEST(!m.valid, "refusal leaves output invalid");

    build_mode(rec, 0, 1080, PixelRedGreenBlueReserved8BitPerColor, 1920, 0, 0, 0);
    TEST(!wubu_gop_decode_mode(rec, sizeof(rec), sizeof(rec), &m),
         "zero width refused");

    build_mode(rec, 1920, 1080, PixelRedGreenBlueReserved8BitPerColor, 1000, 0, 0, 0);
    TEST(!wubu_gop_decode_mode(rec, sizeof(rec), sizeof(rec), &m),
         "stride narrower than width refused");

    build_mode(rec, 1920, 1080, PixelFormatMax, 1920, 0, 0, 0);
    TEST(!wubu_gop_decode_mode(rec, sizeof(rec), sizeof(rec), &m),
         "format past PixelFormatMax refused");

    TEST(!wubu_gop_decode_mode(rec, 20, sizeof(rec), &m),
         "short buffer refused");
    TEST(!wubu_gop_decode_mode(rec, sizeof(rec), 20, &m),
         "record_size below the structure size refused");
    TEST(!wubu_gop_decode_mode(NULL, sizeof(rec), sizeof(rec), &m), "NULL buffer refused");

    /* -- walking an array of real modes ---------------------------------- */
    uint8_t blob[3 * 36];
    build_mode(blob + 0,   1920, 1080, PixelRedGreenBlueReserved8BitPerColor, 1920, 0, 0, 0);
    build_mode(blob + 36,  1600,  900, PixelRedGreenBlueReserved8BitPerColor, 1632, 0, 0, 0);
    build_mode(blob + 72,  1280,  720, PixelRedGreenBlueReserved8BitPerColor, 1280, 0, 0, 0);

    wubu_gop_mode_t modes[4];
    TEST(wubu_gop_walk_modes(blob, sizeof(blob), 36, modes, 4) == 3,
         "all three real modes walked");
    TEST(modes[0].h == 1920 && modes[1].h == 1600 && modes[2].h == 1280,
         "array order preserved");

    /* A corrupt entry ends the walk -- everything after it is suspect. */
    put_u32(blob + 36, 0);                      /* break the second record's Version */
    put_u32(blob + 36 + 0, 7);
    TEST(wubu_gop_walk_modes(blob, sizeof(blob), 36, modes, 4) == 1,
         "walk stops at the first malformed record");

    /* -- size proof ------------------------------------------------------ */
    checks_run++;
    printf("  sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION) = %zu (spec says 36)\n",
           sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));

    printf("%d checks, %d failed\n", checks_run, checks_failed);
    return checks_failed ? 1 : 0;
}
