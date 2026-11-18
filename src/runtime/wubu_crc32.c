/*
 * wubu_crc32.c -- single implementation of CRC-32 (see wubu_crc32.h).
 *
 * Self-contained C11: no table, no libc. Bitwise reflected form is what the
 * previous five copies agreed on; one owner now guarantees they keep agreeing.
 */
#include "wubu_crc32.h"

uint32_t wubu_crc32_run(uint32_t state, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = state;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc;
}

uint32_t wubu_crc32(const void *data, size_t len)
{
    return ~wubu_crc32_run(WUBU_CRC32_INIT_STATE, data, len);
}

uint32_t wubu_crc32_chain(uint32_t prev_result, const void *data, size_t len)
{
    /* prev_result is already inverted; recover the raw state and invert again. */
    return ~wubu_crc32_run(~prev_result, data, len);
}
