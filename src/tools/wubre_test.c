/*
 * wubre_test.c - unit tests for the WuBu regex engine (wubre.c).
 * "occupational supremacy of C11 code" - WaefreBeorn Umbrella License v3.0.
 *
 * Build: $(CC) -O2 -I. wubre.c wubre_test.c -o wubre_test
 * The differential vs GNU grep lives in test_wubugrep.py (full binary).
 */
#include "wubre.h"
#include <stdio.h>
#include <string.h>

static int t(const char *pat, int flags, const char *hay, int expect) {
    char err[256];
    WURegex *re = wubre_compile(pat, flags, err, sizeof err);
    if (!re) { printf("  COMPILE FAIL %s: %s\n", pat, err); return 1; }
    int got = wubre_search(re, (const unsigned char *)hay, strlen(hay)) ? 1 : 0;
    wubre_free(re);
    if (got != expect) {
        printf("  FAIL pat=%s flags=%d hay=%s expect=%d got=%d\n", pat, flags, hay, expect, got);
        return 1;
    }
    return 0;
}

int main(void) {
    int f = 0;
    /* literals */
    f |= t("abc", 0, "xxabcyy", 1);
    f |= t("abc", 0, "xxabd", 0);
    f |= t("", 0, "anything", 1);
    f |= t("", 0, "", 1);
    /* anchors */
    f |= t("^abc", 0, "abcdef", 1);
    f |= t("^abc", 0, "xxabcdef", 0);
    f |= t("abc$", 0, "xxabc", 1);
    f |= t("abc$", 0, "xxabcxx", 0);
    f |= t("^$", 0, "", 1);
    f |= t("^$", 0, "x", 0);
    /* dot */
    f |= t("a.c", 0, "axc", 1);
    f |= t("a.c", 0, "a c", 1);
    f |= t("a.c", 0, "a\nc", 0); /* dot does not match newline */
    /* star/plus */
    f |= t("ab*", 0, "a", 1);
    f |= t("ab*", 0, "abbb", 1);
    f |= t("ab+", 0, "ab", 1);
    f |= t("ab+", 0, "a", 0);
    f |= t("a.*c", 0, "axxxc", 1);
    f |= t("(ab)+", 0, "ababab", 1);
    f |= t("(ab)+", 0, "aba", 1); /* contains 'ab' */
    /* alt */
    f |= t("cat|dog", 0, "i have a dog", 1);
    f |= t("cat|dog", 0, "no pets here", 0);
    f |= t("(foo|bar|baz)", 0, "xx baz yy", 1);
    f |= t("(foo|bar|baz)", 0, "xx nothing yy", 0);
    /* classes */
    f |= t("[a-z]+", 0, "123abc456", 1);
    f |= t("[a-z]+", 0, "123456", 0);
    f |= t("[A-Za-z_][A-Za-z0-9_]*", 0, "var1 = 2", 1);
    f |= t("[^0-9]+", 0, "abc", 1);
    f |= t("[^0-9]+", 0, "123", 0);
    /* icase */
    f |= t("wubu", WUBRE_ICASE, "WUBU", 1);
    f |= t("Wubu", WUBRE_ICASE, "wUbU", 1);
    f |= t("[a-z]+", WUBRE_ICASE, "ABCdef", 1);
    /* BRE: metachars are literal unless backslashed; \{n\} is the interval */
    f |= t("\\(ab\\)+", WUBRE_BRE, "abab+", 1); /* bare + is literal in BRE */
    f |= t("\\(ab\\)+", WUBRE_BRE, "abab", 0);  /* so this should NOT match */
    f |= t("a\\|b", WUBRE_BRE, "xby", 1);
    f |= t("colou\\?r", WUBRE_BRE, "color", 1);
    f |= t("colou\\?r", WUBRE_BRE, "colour", 1);
    f |= t("a\\{2\\}", WUBRE_BRE, "aa", 1);
    f |= t("a\\{2\\}", WUBRE_BRE, "a", 0);
    /* BRE + icase: use a backslashed interval so the semantics are real */
    f |= t("a\\{2\\}", WUBRE_BRE | WUBRE_ICASE, "AA", 1);
    /* word/line is applied by wubugrep, not the engine; cover here lightly */
    if (f) { printf("WUBRE UNIT TESTS: FAILED\n"); return 1; }
    printf("WUBRE UNIT TESTS: ALL PASS\n");
    return 0;
}
