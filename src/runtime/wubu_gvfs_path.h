/*
 * wubu_gvfs_path.h -- GVfs: path normalization and resolution.
 *
 * GVfs resolves paths against MOUNTS, and a mount is a sandbox: a path that
 * escapes its root is a traversal, not a filename.  The rules:
 *
 *   * "." components are dropped; "a/./b" is "a/b".
 *   * empty components collapse; "a//b" is "a/b".
 *   * ".." pops the previous component; "a/b/../c" is "a/c".
 *   * ".." at the ROOT is a TRAVERSAL and is REJECTED -- not clamped.  A
 *     clamped ".." silently hands the caller a different file than the one they
 *     named, which is worse than an error.
 *   * a trailing slash is dropped, except on the root itself.
 *   * an ABSOLUTE reference cannot be resolved against a base directory: the
 *     two disagree about what the root is, so the caller must pick one.
 *   * is_safe() answers "may this be handed to a mount?" -- and it is checked
 *     against the NORMALIZED form, so "a/../b" (which stays inside) is safe
 *     while "a/../../b" (which does not) is not.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_GVFS_PATH_H
#define WUBU_GVFS_PATH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Normalize a path.  Returns NULL on success (out is filled), else a static
 * reason.  The path is treated as RELATIVE TO THE ROOT, so a leading '/' is
 * accepted and a ".." that would rise above it is a traversal. */
const char *wubu_gvfs_path_normalize(const char *in, char *out, size_t n);

/* Resolve `ref` against the base directory `base`.  Returns NULL on success,
 * else a static reason. */
const char *wubu_gvfs_path_resolve(const char *base, const char *ref,
                                   char *out, size_t n);

/* May this path be handed to a mount?  Rejects anything whose NORMALIZED form
 * escapes the root. */
bool wubu_gvfs_path_is_safe(const char *path);

/* Is the path absolute (rooted at the mount root)? */
bool wubu_gvfs_path_is_absolute(const char *path);

/* Split into directory and final component.  Both may be written.  A path with
 * no separator yields an empty directory and the whole path as the name. */
const char *wubu_gvfs_path_split(const char *path,
                                 char *dir, size_t dn,
                                 char *name, size_t nn);

#endif /* WUBU_GVFS_PATH_H */
