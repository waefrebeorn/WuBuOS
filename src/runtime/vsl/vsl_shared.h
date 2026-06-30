/*
 * vsl_shared.h  --  VSL Shared Memory API
 * Opaque struct pattern - only public API exposed
 */

#ifndef WUBUOS_VSL_SHARED_H
#define WUBUOS_VSL_SHARED_H

#include <stdint.h>

/* Shared memory commands */
#define VSL_CMD_NONE        0
#define VSL_CMD_EXEC        1
#define VSL_CMD_MMAP        2
#define VSL_CMD_MUNMAP      3
#define VSL_CMD_EXIT        4
#define VSL_CMD_DRIVER      5

/* Send command to VSL via shared memory.
 * Returns 0 on success, -1 on error. */
int vsl_send_cmd(uint64_t cmd, uint64_t arg);

/* Read VSL response from shared memory. */
uint64_t vsl_read_response(void);

/* Check VSL status flags. */
uint64_t vsl_get_status(void);

#endif /* WUBUOS_VSL_SHARED_H */