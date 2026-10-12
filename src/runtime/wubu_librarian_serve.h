/*
 * wubu_librarian_serve.h -- the librarian's LIVE interop entry point
 * (Track D2, wired). The single front door for cross-system calls, with the
 * default WuBu participants self-registered at init.
 */
#ifndef WUBU_LIBRARIAN_SERVE_H
#define WUBU_LIBRARIAN_SERVE_H

#include <stddef.h>
#include "wubu_syscall_shim.h"   /* wubu_foreign_syscall_t, wubu_shu_op_t */
#include "wubu_librarian.h"       /* wubu_librarian_register + resolver */

/* Register the default WuBu participants (shell/kv/models/bonzi/experts/
 * hive). Idempotent. Returns 0. */
int wubu_librarian_serve_init(void);

/* The live interop call: from `from`, do syscall `sc` on `path_arg` against
 * `to`. Routes through the librarian; fills out_path + *op. Returns 0, or
 * -1 if refused. */
int wubu_librarian_serve(const char *from, const char *to,
                         wubu_foreign_syscall_t sc, const char *path_arg,
                         char *out_path, size_t cap, wubu_shu_op_t *op);

/* Resolve a capability to its offering participant's namespace root.
 * Returns 0 + fills root, or -1 if nobody offers it. */
int wubu_librarian_serve_who(const char *cap, char *root, size_t cap_sz);

#endif /* WUBU_LIBRARIAN_SERVE_H */
