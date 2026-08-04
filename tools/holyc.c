/*
 * holyc.c -- THE COMPILER CLI (the joke, shipped).
 *
 * The user's doctrine (2026-08-04):
 *   "we are c11 luddites, right? and we abstract away.
 *    we will allow a c18 and c2* updates exception called -c_developer.
 *    but if you want to submit any other language into our compiler
 *    and it work (cause we ballin), you must use flag -i_make_shit_code.
 *    like any and all languages that isnt c11 or assembly or holyc,
 *    and for the meme 'brainfuck' language."
 *
 * THE FLAGS:
 *   (no flag)             - C11 (the sacred tongue). Compile + run HolyC.
 *   -c_developer          - blesses C18 / C2* updates.
 *   -i_make_shit_code     - any other language, because we ballin.
 *   -brainfuck            - the meme, compiled for real (bf_run, the JIT).
 *
 * C11, self-contained. Links the HolyC compiler objects + brainfuck.c
 * (with -DHOLYC_BF_EMBEDDED so bf_run is callable, no main conflict).
 */
#include "holyc.h"
#include "holyc_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int bf_run(const char *src);   /* from brainfuck.c */

static void usage(void)
{
    fprintf(stderr,
        "holyc — the WuBuOS compiler.\n"
        "  holyc <file.hc>            C11 (the sacred tongue)\n"
        "  holyc -c_developer <f>     C18/C2* updates (the exception)\n"
        "  holyc -i_make_shit_code <f>  any other language (we ballin)\n"
        "  holyc -brainfuck <src>     the meme (compiled for real)\n");
}

/* read a whole file into a malloc'd buffer */
static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = 0;
    return buf;
}

/* the c_developer blessing: accept C18/C2* source. We are luddites —
 * the exception is granted, the blessing is spoken, the code compiles. */
static int run_c_developer(const char *path)
{
    printf("  [c_developer] the exception is granted. C18/C2* is C,\n"
           "  just newer. we abstract away. (blessing spoken, 2026-08-04)\n");
    char *src = read_file(path);
    if (!src) { fprintf(stderr, "holyc: cannot read %s\n", path); return 1; }
    int64_t r = hc_eval(src);
    printf("  [c_developer] result: %lld\n", (long long)r);
    free(src);
    return 0;
}

/* the i_make_shit_code path: any language that isn't C11/asm/HolyC.
 * Because we ballin, we still try — the source is compiled the only
 * way a serious compiler can: through the front-end that eats bytes. */
static int run_i_make_shit_code(const char *path)
{
    printf("  [i_make_shit_code] you submitted %s to a C11 compiler.\n"
           "  we judge no language. we compile all of them. we ballin.\n", path);
    char *src = read_file(path);
    if (!src) { fprintf(stderr, "holyc: cannot read %s\n", path); return 1; }
    /* try it as HolyC (the front-end eats bytes — if it parses, it runs) */
    int64_t r = hc_eval(src);
    printf("  [i_make_shit_code] result: %lld (compiled anyway)\n", (long long)r);
    free(src);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) { usage(); return 2; }

    if (!strcmp(argv[1], "-c_developer")) {
        if (argc < 3) { usage(); return 2; }
        return run_c_developer(argv[2]);
    }
    if (!strcmp(argv[1], "-i_make_shit_code")) {
        if (argc < 3) { usage(); return 2; }
        return run_i_make_shit_code(argv[2]);
    }
    if (!strcmp(argv[1], "-brainfuck")) {
        if (argc < 3) { usage(); return 2; }
        printf("  [brainfuck] the meme. compiled for real (x86-64 JIT).\n");
        return bf_run(argv[2]);
    }
    if (argv[1][0] == '-') { usage(); return 2; }

    /* the sacred tongue: C11 / HolyC, no flag, silent dignity */
    char *src = read_file(argv[1]);
    if (!src) { fprintf(stderr, "holyc: cannot read %s\n", argv[1]); return 1; }
    int64_t r = hc_eval(src);
    printf("result: %lld\n", (long long)r);
    free(src);
    return 0;
}
