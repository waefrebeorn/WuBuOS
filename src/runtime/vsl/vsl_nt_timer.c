/*
 * vsl_nt_timer.c -- WuBuOS NT transliteration: Timer objects.
 *
 * Split from vsl_nt_proc.c (which mixed 5 NT subsystems in one TU).
 * Self-contained: real VSL/Linux work; dispatched via vsl_nt_timer_register().
 * C11, opaque structs, minimal includes -- shares the vsl_nt_internal.h surface.
 */

#include "vsl_nt_internal.h"

int64_t vsl_nt_create_timer(uint64_t a_out, uint64_t b_unused,
                            uint64_t c_timer_type, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_unused; (void)c_timer_type; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (tfd < 0) return vsl_errno_to_nt_status(errno);
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, tfd, 0, NT_OBJECT_TYPE_TIMER);
    if (h == 0) { close(tfd); return NT_STATUS_UNSUCCESSFUL; }
    for (int i = 0; i < 4096; i++)
        if (g_nt_ctx->handle_table[i].valid &&
            g_nt_ctx->handle_table[i].nt_handle == h) {
            g_nt_ctx->handle_table[i].data = (uint64_t)tfd; break;
        }
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_set_timer(uint64_t a_timer, uint64_t b_due, uint64_t c_period,
                         uint64_t d_apc, uint64_t e_ctx, uint64_t f) {
    (void)d_apc; (void)e_ctx; (void)f;
    uint64_t data = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_timer, &data) != 0)
        return NT_STATUS_INVALID_HANDLE;
    int tfd = (int)data;
    int64_t due_ns = (int64_t)b_due;          /* 100ns units, negative = relative */
    int64_t period_ms = (int64_t)c_period / 10000;
    struct itimerspec its;
    if (due_ns < 0) due_ns = -due_ns;          /* relative: absolute value */
    else due_ns = due_ns - (int64_t)116444736000000000LL; /* absolute -> since epoch */
    its.it_value.tv_sec = (time_t)(due_ns / 1000000000LL);
    its.it_value.tv_nsec = (long)(due_ns % 1000000000LL);
    its.it_interval.tv_sec = (time_t)(period_ms / 1000);
    its.it_interval.tv_nsec = (long)((period_ms % 1000) * 1000000LL);
    if (timerfd_settime(tfd, 0, &its, NULL) != 0)
        return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_cancel_timer(uint64_t a_timer, uint64_t b_prev, uint64_t c,
                            uint64_t d, uint64_t e, uint64_t f) {
    (void)b_prev; (void)c; (void)d; (void)e; (void)f;
    uint64_t data = 0;
    if (vsl_nt_handle_to_data(g_nt_ctx, (uint32_t)a_timer, &data) != 0)
        return NT_STATUS_INVALID_HANDLE;
    struct itimerspec its; memset(&its, 0, sizeof(its));
    if (timerfd_settime((int)data, 0, &its, NULL) != 0)
        return vsl_errno_to_nt_status(errno);
    return NT_STATUS_SUCCESS;
}

int64_t vsl_nt_open_timer(uint64_t a_out, uint64_t b_access,
                          uint64_t c_obj_attr, uint64_t d, uint64_t e, uint64_t f) {
    (void)b_access; (void)c_obj_attr; (void)d; (void)e; (void)f;
    if (!a_out) return NT_STATUS_INVALID_PARAMETER;
    uint32_t h = vsl_nt_allocate_handle(g_nt_ctx, -1, 0, NT_OBJECT_TYPE_TIMER);
    if (h == 0) return NT_STATUS_UNSUCCESSFUL;
    *(uint32_t *)a_out = h;
    return NT_STATUS_SUCCESS;
}

/* NtExtendSection (79): grow the section's mapped view (mremap best-effort). */

/* Register this module's NT handlers into the global dispatch table. */
void vsl_nt_timer_register(vsl_syscall_fn_t *tbl, int size) {
    (void)size;
    tbl[97-1] = vsl_nt_cancel_timer;
    tbl[203-1] = vsl_nt_create_timer;
    tbl[314-1] = vsl_nt_open_timer;
    tbl[98-1] = vsl_nt_set_timer;
}
