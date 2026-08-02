/*
 * test_agi_kernel_stub.c -- Minimal kernel-API shims so wubu_agi_kernel.c
 * links + runs in the HOSTED unit test without the real ring-0 scheduler.
 *
 * The AGI kernel's run loop (task_create/task_yield) is NOT exercised here --
 * the test drives init/cycle/tick directly. These stubs just satisfy the
 * linker and the (unused-in-test) run path. On bare metal, real tasking/klog
 * are linked instead.
 */
#include "tasking.h"
#include "klog.h"
#include "input.h"
#include "vbe.h"
#include <stdio.h>

CTask *task_create(const char *name, void (*entry)(void *arg), void *arg,
                   size_t stack_sz, TaskPriority prio) {
    (void)name; (void)entry; (void)arg; (void)stack_sz; (void)prio;
    return NULL;   /* test never calls wubu_agi_kernel_run() */
}
void task_yield(void) {}
void task_sleep(uint64_t ticks) { (void)ticks; }
void task_preempt_enable(void) {}
void task_timer_tick(void) {}

int klog_printf(const char *fmt, ...) {
    (void)fmt;
    return 0;   /* silence kernel logging in the hosted test */
}

/* ---- wubu_bonzi hosted stubs (input + vbe) ------------------------- */
int input_key_poll(KeyEvent *out) { (void)out; return 0; }
int input_key_wait(KeyEvent *out) { (void)out; return 0; }
void input_key_push(KeyEvent ev) { (void)ev; }
int input_key_pressed(uint32_t sc) { (void)sc; return 0; }

uint32_t *vbe_framebuffer(void) { return NULL; }
int vbe_width(void) { return 640; }
int vbe_height(void) { return 480; }
void vbe_clear(uint32_t c) { (void)c; }
void vbe_swap(void) {}
void vbe_set_pixel(int x, int y, uint32_t c) { (void)x; (void)y; (void)c; }
void vbe_fill_rect(int x, int y, int w, int h, uint32_t c) { (void)x;(void)y;(void)w;(void)h;(void)c; }
void vbe_rect(int x, int y, int w, int h, uint32_t c) { (void)x;(void)y;(void)w;(void)h;(void)c; }
void vbe_hline(int x1, int x2, int y, uint32_t c) { (void)x1;(void)x2;(void)y;(void)c; }
void vbe_fill_circle(int cx, int cy, int r, uint32_t c) { (void)cx;(void)cy;(void)r;(void)c; }
void vbe_fill_rect_rounded(int x, int y, int w, int h, int r, uint32_t c) { (void)x;(void)y;(void)w;(void)h;(void)r;(void)c; }
void vbe_rect_rounded(int x, int y, int w, int h, int r, uint32_t c) { (void)x;(void)y;(void)w;(void)h;(void)r;(void)c; }
void vbe_draw_text(int x, int y, const char *s, uint32_t c, int scale) { (void)x;(void)y;(void)s;(void)c;(void)scale; }
