/*
 * cp_test_klog.c -- the two kernel symbols vbe.c needs, for panel test graphs.
 *
 * The Control Panel now draws its text through the kernel renderer (one type stack), so a panel test that wants
 * real glyphs must link vbe.c -- and vbe.c logs through klog_printf and allocates through mem_alloc. These are
 * the test-graph stand-ins for exactly those two, and nothing else: they exist so the gate links the REAL
 * renderer instead of a fake one. test_applet_layout does not need this file (it already links a klog stub).
 */
#include <stddef.h>
#include <stdlib.h>

void klog_printf(const char *fmt, ...) { (void)fmt; }

void *mem_alloc(size_t n) { return malloc(n); }
void  mem_free(void *p) { free(p); }
