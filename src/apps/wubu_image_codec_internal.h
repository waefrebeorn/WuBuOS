/*
 * wubu_image_codec_internal.h -- Self-contained image-codec leaf API.
 *
 * Declares the pure codec primitives extracted from wubu_canvas_io.c
 * (CRC32, big-endian writers, PNG chunk emission, PNG unfilter). Minimal
 * includes, no god header. The codec layer has no canvas dependency.
 */
#ifndef WUBU_IMAGE_CODEC_INTERNAL_H
#define WUBU_IMAGE_CODEC_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* CRC32 (PNG polynomial 0xEDB88320). */
void     crc32_init(void);
uint32_t crc32_update(uint32_t crc_in, const void *data, size_t len);
uint32_t crc32_data(const void *data, size_t len);

/* Big-endian writers into a caller-provided buffer. */
void write_be32(uint8_t *buf, uint32_t val);
void write_be16(uint8_t *buf, uint16_t val);

/* PNG chunk emission (length + type + data + CRC). */
void png_write_chunk(FILE *f, const char *type, const void *data, size_t len);

/* PNG adaptive unfilter: reverses filter types 0-4 in-place. Returns 0 on
 * success, -1 on a malformed row. */
int png_unfilter(uint8_t *raw, int w, int h, int channels);

#endif /* WUBU_IMAGE_CODEC_INTERNAL_H */
