/* service_mgr_test_stub.c -- link-time stub for service_mgr_test.
 *
 * service_mgr.c calls wubu_notify_send() to toast autostart results. A
 * NOTIFICATION TOAST IS NOT what this test checks -- it asserts the archd
 * wiring (init/register/boot/dedup-shutdown) -- so linking the real toast
 * (wubu_notify.c) would drag in the whole vbe/theme/wm/settings render
 * stack for no reason. Stub it: the test target stays a focused unit gate
 * instead of a mini-desktop link.
 */
#include "../egojar.h"
#include "shell/wubu_notify.h"

uint32_t wubu_notify_send(const Notification *n)
{
    (void)n;
    return 0;   /* accepted, no toast */
}
