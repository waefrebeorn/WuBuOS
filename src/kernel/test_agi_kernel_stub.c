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

/* ---- wubu_console hosted stub (the run loop spawns it) -------------- */
void wubu_console_task(void *arg) { (void)arg; }

/* ---- wubu_theme hosted stubs (agi_theme_step writes /theme nodes) --- */
int wubu_theme_node_set(const char *p, uint32_t v) { (void)p; (void)v; return 0; }
int wubu_theme_node_get(const char *p, uint32_t *v) { (void)p; if (v) *v = 0; return 0; }
void wubu_theme_apply(void) {}
const void *wubu_theme_get(void) { return NULL; }

/* ---- wubu_vmm hosted stub (agi_theme_step reads memory pressure) --- */
uint64_t wubu_vmm_free_count(void) { return 0xFFFFFFFFull; }

/* ---- wubu_sync hosted shims (the ring's cli-based lock is ring-0) --- */
#include "wubu_sync.h"
void wubu_spin_init(wubu_spinlock_t *l) { l->locked = 0; l->irq_state = 0; }
void wubu_spin_lock(wubu_spinlock_t *l)
{ while (__atomic_test_and_set(&l->locked, __ATOMIC_ACQUIRE)) {} }
void wubu_spin_unlock(wubu_spinlock_t *l)
{ __atomic_clear(&l->locked, __ATOMIC_RELEASE); }

/* ---- tasking hosted shims (the main loop reaps DYING tasks) --------- */
void task_reap(void) { }

