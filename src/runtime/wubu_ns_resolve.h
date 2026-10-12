/*
 * wubu_ns_resolve.h -- resolve a /n/... path to its real on-disk location.
 *
 * The /n namespace is a directory tree under g_ns_root; a shell/tool handed
 * "/n/bonzi/enabled" needs it mapped to <g_ns_root>/bonzi/enabled to read
 * the live tree. Non-/n paths pass through unchanged. Pure + testable.
 */
#ifndef WUBU_NS_RESOLVE_H
#define WUBU_NS_RESOLVE_H

#include <stddef.h>

/* Resolve a /n/... path to its on-disk location under g_ns_root. Returns 0
 * and fills out on success; -1 if /n is given but the root isn't mounted
 * (out=""). A non-/n path is returned unchanged (return 0). */
int wubu_ns_resolve(const char *path, char *out, size_t cap);

/* Whether a path is a /n namespace path. */
int wubu_ns_is_ns_path(const char *path);

#endif /* WUBU_NS_RESOLVE_H */
