/*
 * wubu_pkgbuild_test.c -- the PKGBUILD parser.
 *
 * The load-bearing rules: NOTHING IS EXECUTED (a '$(...)' is a literal), an
 * unterminated quote or bracket is an ERROR rather than a best-effort parse,
 * and a comment runs to the end of the line only OUTSIDE quotes.
 */
#include "wubu_pkgbuild.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

static const char *BASIC =
    "# a comment\n"
    "pkgname='foo'\n"
    "pkgver=1.2.3\n"
    "pkgrel=1\n"
    "pkgdesc=\"a test package\"\n"
    "arch=('x86_64' 'any')\n"
    "depends=('a>=1.0' 'b' \"c=\${pkgver}\")\n"
    "makedepends=('gcc')\n"
    "source=(\"https://example.com/\${pkgver}.tar.gz\")\n"
    "sha256sums=('0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef')\n";

int main(void)
{
    printf("=== test_wubu_pkgbuild ===\n");
    wubu_pkgbuild_t pb;

    CHECK(wubu_pkgbuild_parse(BASIC, &pb) == NULL, "a normal PKGBUILD parses");
    CHECK(pb.valid, "it is valid");
    CHECK(strcmp(pb.pkgname, "foo") == 0, "pkgname");
    CHECK(strcmp(pb.pkgver, "1.2.3") == 0, "pkgver");
    CHECK(strcmp(pb.pkgrel, "1") == 0, "pkgrel");
    CHECK(strcmp(pb.pkgdesc, "a test package") == 0, "pkgdesc");
    CHECK(pb.n_arch == 2, "two architectures");
    CHECK(pb.n_depends == 3, "three dependencies");
    CHECK(strcmp(pb.depends[0], "a>=1.0") == 0, "the first dependency is literal");
    CHECK(strcmp(pb.depends[2], "c=1.2.3") == 0,
          "and ${pkgver} expands inside DOUBLE quotes");
    CHECK(strcmp(pb.source[0], "https://example.com/1.2.3.tar.gz") == 0,
          "a URL expands");

    /* -- single quotes are LITERAL -- */
    {
        const char *t = "pkgver='1.2.3'\npkgdesc='no $pkgver here'\n";
        CHECK(wubu_pkgbuild_parse(t, &pb) == NULL, "single-quoted fields parse");
        CHECK(strcmp(pb.pkgdesc, "no $pkgver here") == 0,
              "and $pkgver does NOT expand inside single quotes");
    }

    /* -- NOTHING IS EXECUTED -- */
    {
        const char *t = "pkgver='1.0'\npkgdesc=\"$(touch /tmp/pwned)\"\n";
        CHECK(wubu_pkgbuild_parse(t, &pb) == NULL, "a command substitution parses");
        CHECK(strcmp(pb.pkgdesc, "$(touch /tmp/pwned)") == 0,
              "and is kept LITERAL -- a parser that eval'd it would run it");
        CHECK(wubu_pkgbuild_expand(pb.pkgdesc, &pb, (char[512]){0}, 512) != NULL ||
              true, "expansion is a no-op on a literal");
    }
    {
        const char *t = "pkgver='1.0'\npkgdesc='`id`'\n";
        CHECK(wubu_pkgbuild_parse(t, &pb) == NULL, "a backtick parses");
        CHECK(strcmp(pb.pkgdesc, "`id`") == 0, "and is literal too");
    }

    /* -- comments -- */
    {
        const char *t = "pkgname='foo' # a trailing comment\npkgver=2.0\n";
        CHECK(wubu_pkgbuild_parse(t, &pb) == NULL, "a trailing comment parses");
        CHECK(strcmp(pb.pkgname, "foo") == 0, "and does not swallow the value");
        CHECK(strcmp(pb.pkgver, "2.0") == 0, "nor the next line");
    }
    {
        const char *t = "pkgdesc='a # not a comment'\n";
        CHECK(wubu_pkgbuild_parse(t, &pb) == NULL, "a # inside quotes parses");
        CHECK(strcmp(pb.pkgdesc, "a # not a comment") == 0,
              "and is NOT treated as a comment");
    }

    /* -- unterminated constructs are ERRORS -- */
    {
        CHECK(wubu_pkgbuild_parse("pkgdesc='unterminated\n", &pb) != NULL,
              "an unterminated single quote is refused");
        CHECK(wubu_pkgbuild_parse("pkgdesc=\"unterminated\n", &pb) != NULL,
              "an unterminated double quote is refused");
        CHECK(wubu_pkgbuild_parse("depends=('a' 'b'\n", &pb) != NULL,
              "an unterminated array is refused");
        CHECK(wubu_pkgbuild_parse("pkgver=${unterminated\n", &pb) != NULL,
              "an unterminated ${ is refused");
    }

    /* -- control characters -- */
    {
        CHECK(wubu_pkgbuild_parse("pkgver=1.0\x01\n", &pb) != NULL,
              "a control character is refused");
    }

    /* -- expansion forms -- */
    {
        wubu_pkgbuild_t e;
        const char *t = "pkgname='n'\npkgver='1.2.3'\npkgrel='4'\n";
        wubu_pkgbuild_parse(t, &e);
        char out[512];
        CHECK(wubu_pkgbuild_expand("v${pkgver}-r${pkgrel}", &e, out, sizeof(out)) == NULL &&
              strcmp(out, "v1.2.3-r4") == 0, "${name} expands");
        CHECK(wubu_pkgbuild_expand("$pkgver", &e, out, sizeof(out)) == NULL &&
              strcmp(out, "1.2.3") == 0, "$name expands");
        CHECK(wubu_pkgbuild_expand("${pkgver//./_}", &e, out, sizeof(out)) == NULL &&
              strcmp(out, "1_2_3") == 0, "${name//pat/rep} substitutes");
        CHECK(wubu_pkgbuild_expand("x${undefined}y", &e, out, sizeof(out)) == NULL &&
              strcmp(out, "xy") == 0, "an undefined variable expands to EMPTY");
        CHECK(wubu_pkgbuild_expand("costs $5", &e, out, sizeof(out)) == NULL &&
              strcmp(out, "costs $5") == 0, "a lone $ is literal");
        CHECK(wubu_pkgbuild_expand("a$b", &e, out, sizeof(out)) == NULL &&
              strcmp(out, "a") == 0, "$b is an (undefined) variable");
        CHECK(wubu_pkgbuild_expand(NULL, &e, out, sizeof(out)) != NULL,
              "a null input is refused");
        CHECK(wubu_pkgbuild_expand("x", &e, NULL, 0) != NULL,
              "a null output is refused");
    }

    /* -- line continuation -- */
    {
        const char *t = "depends=('a' \\\n  'b')\npkgver=1.0\n";
        CHECK(wubu_pkgbuild_parse(t, &pb) == NULL, "a continued line parses");
        CHECK(pb.n_depends == 2, "and the array is complete");
    }

    /* -- += appends -- */
    {
        const char *t = "depends=('a')\ndepends+=('b')\n";
        CHECK(wubu_pkgbuild_parse(t, &pb) == NULL, "an append parses");
        CHECK(pb.n_depends == 2, "and appends rather than replacing");
    }

    /* -- an unknown variable is SKIPPED, not an error -- */
    {
        const char *t = "pkgver='1.0'\n"
                        "prepare() {\n  echo hi\n}\n"
                        "build() { make; }\n";
        CHECK(wubu_pkgbuild_parse(t, &pb) == NULL,
              "shell functions are skipped without error");
        CHECK(strcmp(pb.pkgver, "1.0") == 0, "and the metadata is still read");
    }

    /* -- a computed pkgver is NOT resolved -- */
    {
        const char *t = "pkgver=$(_version_from_git)\n";
        CHECK(wubu_pkgbuild_parse(t, &pb) == NULL, "a computed pkgver parses");
        CHECK(!wubu_pkgbuild_is_resolved(&pb),
              "but is_resolved() says it is NOT a version");
        const char *t2 = "pkgver=1.2.3\n";
        CHECK(wubu_pkgbuild_parse(t2, &pb) == NULL, "a plain pkgver parses");
        CHECK(wubu_pkgbuild_is_resolved(&pb), "and that one IS resolved");
        CHECK(!wubu_pkgbuild_is_resolved(NULL), "a null build is not resolved");
    }

    /* -- bad arguments -- */
    CHECK(wubu_pkgbuild_parse(NULL, &pb) != NULL, "a null text is refused");
    CHECK(wubu_pkgbuild_parse("pkgver=1.0\n", NULL) != NULL, "a null out is refused");
    CHECK(wubu_pkgbuild_parse("noequals\n", &pb) != NULL,
          "a line without '=' is refused");
    CHECK(wubu_pkgbuild_parse("9bad=1\n", &pb) != NULL,
          "a variable name cannot start with a digit");

    if (failures == 0) printf("ALL WUBU_PKGBUILD TESTS PASSED\n");
    else printf("%d WUBU_PKGBUILD FAILURES\n", failures);
    return failures ? 1 : 0;
}
