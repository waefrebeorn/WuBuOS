/*
 * compiler_diff.c -- THE DIFFERENTIAL TESTING HARNESS (compiler-doctrine).
 *
 * The bug-bank doctrine (the user's directive): "we can fix all of the
 * bugs by finding all of the bug bugs on the Internet that allows us
 * to know where we are by knowing where we aren't."
 *
 * This harness compiles the SAME program with OUR compiler (hc_eval)
 * AND with gcc (via a generated C file + subprocess), runs both, and
 * compares. Every divergence is a FINDING: either our compiler is
 * wrong (fix ours) or gcc is wrong (we learn their bug, avoid it).
 * The Csmith doctrine applied to ourselves.
 *
 * Usage:
 *   compiler_diff <holyc-expr> [expected]
 *   compiler_diff --file <src.hc>
 *
 * C11, self-contained. Links the HolyC compiler objects.
 */
#include "holyc.h"
#include "holyc_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int run_gcc(const char *c_src, const char *prog, int64_t *out_rc)
{
    /* write the C file */
    FILE *f = fopen("/tmp/wubu_diff.c", "w");
    if (!f) return -1;
    fprintf(f, "#include <stdio.h>\n#include <stdint.h>\nint main(void){\n"
               "int64_t r = (%s);\nprintf(\"%%lld\\n\", (long long)r);\n"
               "return 0;\n}\n", c_src);
    fclose(f);
    /* compile with gcc */
    if (system("gcc -O0 -w /tmp/wubu_diff.c -o /tmp/wubu_diff_bin 2>/dev/null") != 0)
        return -1;
    /* run it, capture stdout */
    FILE *p = popen("/tmp/wubu_diff_bin", "r");
    if (!p) return -1;
    char buf[64];
    if (!fgets(buf, sizeof(buf), p)) { pclose(p); return -1; }
    pclose(p);
    *out_rc = atoll(buf);
    return 0;
}

/* translate a HolyC expression to a C expression (the subset that
 * differs: U0/U8/I64 etc. -> the C equivalents; HolyC ";" statement
 * separators in expressions are not C — the harness uses pure
 * expressions for the differential). */
static void translate(const char *hc, char *out, size_t cap)
{
    size_t o = 0;
    for (size_t i = 0; hc[i] && o + 4 < cap; i++) {
        /* HolyC integer suffixes: U8/U16/U32/U64/I64 are TYPES, but in
         * an expression context they don't appear — keep the rest */
        if (!strncmp(hc + i, "True", 4) || !strncmp(hc + i, "TRUE", 4)) {
            memcpy(out + o, "1", 1); o += 1; i += 3; continue;
        }
        if (!strncmp(hc + i, "False", 5) || !strncmp(hc + i, "FALSE", 5)) {
            memcpy(out + o, "0", 1); o += 1; i += 4; continue;
        }
        out[o++] = hc[i];
    }
    out[o] = 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <holyc-expr> [expected]\n", argv[0]);
        return 2;
    }
    const char *expr = argv[1];
    long long expected = 0;
    int have_expected = 0;
    if (argc >= 3) { expected = atoll(argv[2]); have_expected = 1; }

    printf("=== differential: %s ===\n", expr);

    /* 1. OUR compiler */
    int64_t ours = hc_eval(expr);
    printf("  ours : %lld\n", (long long)ours);

    /* 2. gcc */
    char c_src[512];
    translate(expr, c_src, sizeof(c_src));
    int64_t gcc_rc = 0;
    int gcc_ok = run_gcc(c_src, "/tmp/wubu_diff_bin", &gcc_rc);
    if (gcc_ok == 0)
        printf("  gcc  : %lld\n", (long long)gcc_rc);
    else
        printf("  gcc  : (not expressible as C — skipped)\n");

    /* 3. the comparison — the FINDING */
    int finding = 0;
    if (gcc_ok == 0 && ours != gcc_rc) {
        printf("  ⚠ FINDING: ours=%lld gcc=%lld — a divergence!\n",
               (long long)ours, (long long)gcc_rc);
        finding = 1;
    } else if (have_expected && ours != expected) {
        printf("  ⚠ FINDING: ours=%lld expected=%lld — we are wrong!\n",
               (long long)ours, (long long)expected);
        finding = 1;
    } else if (gcc_ok == 0) {
        printf("  ✓ agree: ours == gcc == %lld\n", (long long)ours);
    }

    if (have_expected && !finding && gcc_ok == 0)
        printf("  ✓ all three agree\n");
    return finding ? 1 : 0;
}
