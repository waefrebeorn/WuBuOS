/*
 * vsl_shared.c  --  VSL Shared Memory Implementation
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "vsl/vsl_internal.h"
#include "vsl/vsl_shared.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Global state */
extern VSL_STATE g_vsl;

/* -- Shared Memory ------------------------------------------------ */

int vsl_send_cmd(uint64_t cmd, uint64_t arg) {
    if (!g_vsl.active) return -1;
    *g_vsl.shared_cmd = cmd;
    *g_vsl.shared_arg = arg;
    *g_vsl.shared_status = 1; /* command pending */
    return 0;
}

uint64_t vsl_read_response(void) {
    return *g_vsl.shared_ret;
}

uint64_t vsl_get_status(void) {
    return *g_vsl.shared_status;
}