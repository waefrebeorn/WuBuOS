/*
 * wubu_crc32.h -- the one owner of CRC-32 (IEEE 802.3, reflected poly
 * 0xEDB88320, init/xorout 0xFFFFFFFF) in WuBuOS.
 *
 * Before this existed the same algorithm was reimplemented independently in
 * the image codec, the PNG writer, the package manager, txfs and the recovery
 * substrate -- five copies that had to agree by luck. Any new use calls these.
 */
#ifndef WUBU_CRC32_H
#define WUBU_CRC32_H

#include <stddef.h>
#include <stdint.h>

/* Raw running state of a CRC-32 stream (init value 0xFFFFFFFF, never inverted). */
#define WUBU_CRC32_INIT_STATE 0xFFFFFFFFu

/*
 * Advance a raw running state over data. `state` must be a raw (not inverted)
 * state -- use WUBU_CRC32_INIT_STATE to start. Returns the next raw state.
 */
uint32_t wubu_crc32_run(uint32_t state, const void *data, size_t len);

/* One-shot CRC-32 of a buffer: the standard value (init + xorout applied). */
uint32_t wubu_crc32(const void *data, size_t len);

/*
 * Chain onto a previously RETURNED value (the convention several chunked
 * writers use, where each call passes back the last result). Returns the next
 * finished value, ready to pass in again.
 */
uint32_t wubu_crc32_chain(uint32_t prev_result, const void *data, size_t len);

#endif /* WUBU_CRC32_H */
