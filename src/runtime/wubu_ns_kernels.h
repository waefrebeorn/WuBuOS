/*
 * wubu_ns_kernels.h -- the /n/kernels control subtree.
 *
 *   /n/kernels/list     the registered kernels + which one is active
 *   /n/kernels/active   just the active kernel's name ("(none)" before any selection)
 *   /n/kernels/history  the swap log, newest first
 *
 * The AGI, the shell and the operator all read the same files: the network is
 * observable from the namespace, not only from inside the process.
 */
#ifndef WUBU_NS_KERNELS_H
#define WUBU_NS_KERNELS_H

/* Create the subtree if needed and write every file from the live state. */
int wubu_ns_publish_kernels(void);
/* Re-render list/active/history from the live state (call after a swap). */
int wubu_ns_kernels_refresh(void);

/* The operator's control path: make a kernel active.  This is a swap, so it
 * is fail-closed on the schema contract exactly like wubu_kernel_select(). */
int wubu_ns_kernels_select(const char *name);

#endif /* WUBU_NS_KERNELS_H */
