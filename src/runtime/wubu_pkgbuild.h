/*
 * wubu_pkgbuild.h -- the PKGBUILD parser.
 *
 * A PKGBUILD is a bash script, but a package manager must PARSE it, never run
 * it: executing it to learn pkgver would run arbitrary code at build time on
 * the packager's machine.  So the parser understands the SUBSET that actually
 * carries metadata and nothing else:
 *
 *   pkgname='foo'
 *   pkgver=1.2.3
 *   depends=('a>=1.0' 'b' "c=${pkgver}")
 *   source+=(https://example.com/$pkgver.tar.gz)
 *
 * The rules, all of them security-relevant:
 *
 *   * NOTHING IS EXECUTED.  There is no eval, no shell, no command
 *     substitution.  A '$(...)' is a LITERAL string, not a command.
 *   * An UNTERMINATED quote or bracket is an ERROR, not a best-effort parse --
 *     a half-parsed PKGBUILD silently installs the wrong package.
 *   * Single quotes preserve everything literally; double quotes allow
 *     expansion; an unquoted word allows expansion and word splitting.
 *   * A COMMENT runs to the end of the line, but only OUTSIDE quotes.
 *   * A LINE CONTINUATION (a trailing backslash) joins lines, because a
 *     depends array is routinely wrapped.
 *   * A variable that is not defined expands to EMPTY, which is the shell
 *     behaviour and also what makes a missing optional field harmless.
 *   * Control characters are rejected: they are never legitimate metadata.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_PKGBUILD_H
#define WUBU_PKGBUILD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WB_PB_NAME_MAX   64
#define WB_PB_VER_MAX    32
#define WB_PB_STR_MAX    256
#define WB_PB_ARRAY_MAX  64
#define WB_PB_ARCH_MAX   16

typedef struct {
    char pkgname[WB_PB_NAME_MAX];
    char pkgver[WB_PB_VER_MAX];
    char pkgrel[WB_PB_VER_MAX];
    char pkgdesc[WB_PB_STR_MAX];
    char url[WB_PB_STR_MAX];

    char arch[WB_PB_ARCH_MAX][16];
    int  n_arch;

    char depends[WB_PB_ARRAY_MAX][WB_PB_STR_MAX];
    int  n_depends;
    char makedepends[WB_PB_ARRAY_MAX][WB_PB_STR_MAX];
    int  n_makedepends;
    char optdepends[WB_PB_ARRAY_MAX][WB_PB_STR_MAX];
    int  n_optdepends;
    char source[WB_PB_ARRAY_MAX][WB_PB_STR_MAX];
    int  n_source;
    char sha256sums[WB_PB_ARRAY_MAX][96];
    int  n_sha256sums;

    bool valid;
} wubu_pkgbuild_t;

/* Parse PKGBUILD text.  Returns NULL on success, else a static reason. */
const char *wubu_pkgbuild_parse(const char *text, wubu_pkgbuild_t *out);

/* Expand ${var}, $var and ${var//pat/rep} using the parsed scalars.
 * Returns NULL on success, else a static reason. */
const char *wubu_pkgbuild_expand(const char *s, const wubu_pkgbuild_t *pb,
                                 char *out, size_t n);

/* Is this a plain (non-computed) field?  A pkgver containing '$' was not
 * resolved and must not be treated as a version. */
bool wubu_pkgbuild_is_resolved(const wubu_pkgbuild_t *pb);

#endif /* WUBU_PKGBUILD_H */
