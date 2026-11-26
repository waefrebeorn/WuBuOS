/*
 * wubu_pkgx.h -- the packaged-app runtime (WindowsAppRuntime's job, done small).
 *
 * A packaged app is not a binary on PATH: it has an IDENTITY (a name plus a publisher-derived family name),
 * a VERSION, a payload directory and an entry point inside that directory. This is the part of a packaged
 * runtime that everything else is built on, and it is the part that can be implemented and tested honestly
 * without pretending to do container isolation or MSIX signature verification.
 *
 * On disk, one directory per package under the root:
 *
 *     <root>/<name>/package.conf        key=value: name, version, publisher, exec
 *     <root>/<name>/<payload>
 *
 * The family name follows the MSIX SHAPE -- "<name>_<8 hex>" -- with the hex derived from the publisher by
 * FNV-1a. It is not Microsoft's hash (that is theirs and undocumented); it is a stable, documented one, and
 * the header says so rather than implying compatibility.
 *
 * The exec entry is VALIDATED: relative, no "..", and it must exist and be executable. A packaged app whose
 * entry point escapes its own package directory is how a packaged runtime becomes a privilege problem.
 */
#ifndef WUBU_PKGX_H
#define WUBU_PKGX_H

#include <stddef.h>

#define WUBU_PKGX_MAX        64
#define WUBU_PKGX_NAME_MAX   64
#define WUBU_PKGX_FAMILY_MAX 160
#define WUBU_PKGX_VER_MAX    32
#define WUBU_PKGX_EXEC_MAX   256
#define WUBU_PKGX_ROOT_MAX   256

typedef struct {
    char name[WUBU_PKGX_NAME_MAX];
    char family[WUBU_PKGX_FAMILY_MAX];
    char version[WUBU_PKGX_VER_MAX];
    char exec[WUBU_PKGX_EXEC_MAX];      /* relative to root */
    char root[WUBU_PKGX_ROOT_MAX];      /* <packages root>/<name> */
} WubuPkgApp;

typedef struct {
    WubuPkgApp a[WUBU_PKGX_MAX];
    int         count;
} WubuPkgRegistry;

/* $WUBU_PKGS, else ~/.local/share/wubu/packages. Never cached. */
const char *wubu_pkgx_default_root(void);

/* The stable family-name hash of a publisher string (FNV-1a 32-bit). */
unsigned wubu_pkgx_family_hash(const char *publisher);

/* Build "<name>_<8 hex>" for a publisher. */
int wubu_pkgx_family_name(const char *name, const char *publisher, char *out, size_t cap);

/* Register the package in `dir` (which must contain package.conf). Returns 0 ok, 2 invalid manifest
 * (missing name, absolute or escaping exec, missing payload), -1 I/O. */
int wubu_pkgx_register(const char *dir, WubuPkgApp *out);

/* Every registered package, sorted by name. */
int wubu_pkgx_list(WubuPkgRegistry *out);

/* Resolve by name OR family name. 0 found, 1 not found. */
int wubu_pkgx_resolve(const char *id, WubuPkgApp *out);

/* Remove a package's directory. 0 removed, 1 not found. */
int wubu_pkgx_unregister(const char *name);

/* The absolute entry point of a package ("<root>/<exec>"). 0 ok. */
int wubu_pkgx_exec_path(const WubuPkgApp *p, char *out, size_t cap);

#endif
