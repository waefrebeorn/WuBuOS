/*
 * holyc.c -- THE COMPILER CLI (the joke, shipped) + WUBURUNTIME broker.
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
 * WUBURUNTIME (the user's directive, research/063): every OO runtime
 * gets its OWN compilation space:
 *   -space <name> [<file>]       compile INTO a named space; the
 *                                snapshot (compiler_ver + language_ver
 *                                + created) is recorded so nothing is
 *                                left in the dust.
 *   -personality <kind>          attach posix/image/wasi to the space
 *                                (the gap filler: runtime syscalls map
 *                                to the OS-native substrate).
 *   -spaces                      list every compilation space (the
 *                                disorganization, solved).
 *
 * C11, self-contained. Links the HolyC compiler objects + brainfuck.c
 * (with -DHOLYC_BF_EMBEDDED so bf_run is callable, no main conflict)
 * + the wuburuntime registry (wubu_runtime.c, personalities, hive).
 */
#include "holyc.h"
#include "holyc_codegen.h"
#include "wubu_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int bf_run(const char *src);   /* from brainfuck.c */

#define WUBU_COMPILER_VER "holyc-0.1.0"
#define WUBU_SNAPSHOT_DATE "2026-08-04"

static wubu_hive_t *g_hive;
static wubu_runtime_t *g_rt;

/* the persistence file: the snapshot survives process exit (the
 * "nothing left in the dust" guarantee, made real). Override with
 * $WUBURUNTIME_FILE. */
static const char *rt_file(void)
{
    const char *f = getenv("WUBURUNTIME_FILE");
    return f ? f : "/tmp/wuburuntime.spaces";
}

static void rt_save(void)
{
    if (g_rt) wubu_runtime_save(g_rt, rt_file());
}

static void usage(void)
{
    fprintf(stderr,
        "holyc — the WuBuOS compiler + wuburuntime broker.\n"
        "  holyc <file.hc>            C11 (the sacred tongue)\n"
        "  holyc -c_developer <f>     C18/C2* updates (the exception)\n"
        "  holyc -i_make_shit_code <f>  any other language (we ballin)\n"
        "  holyc -brainfuck <src>     the meme (compiled for real)\n"
        "  holyc -space <name> <f>    compile INTO a compilation space\n"
        "  holyc -space <name> -personality <kind> <f>  attach a\n"
        "                             personality (posix/image/wasi)\n"
        "  holyc -i_make_shit_code -space <name> <f>  foreign code into\n"
        "                             its runtime's space\n"
        "  holyc -spaces              list the compilation spaces\n");
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

/* boot the wuburuntime registry (once per CLI invocation), loading any
 * previously-saved spaces so -spaces sees the accumulated state */
static wubu_runtime_t *boot_rt(void)
{
    if (g_rt) return g_rt;
    g_hive = wubu_hive_new(0, malloc, free);
    if (!g_hive) return NULL;
    wubu_rt_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_spaces = 16;
    cfg.default_heap_cap = 1ull << 30;
    g_rt = wubu_runtime_init(g_hive, &cfg);
    if (g_rt) wubu_runtime_load(g_rt, rt_file());  /* best-effort */
    return g_rt;
}

/* find-or-create a named compilation space (the broker entry point) */
static wubu_rt_space_t *space_get(const char *name)
{
    wubu_runtime_t *rt = boot_rt();
    if (!rt) return NULL;
    wubu_rt_space_t *sp = wubu_runtime_find_name(rt, name);
    if (sp) return sp;
    uint64_t id = wubu_runtime_create(rt, name, name,
                                      WUBU_COMPILER_VER,
                                      "holyc (the sacred tongue)",
                                      "wubu-abi-v1", "/n/");
    if (!id) return NULL;
    rt_save();  /* the snapshot persists: nothing left in the dust */
    sp = wubu_runtime_find(rt, id);
    if (sp) {
        printf("  [wuburuntime] space '%s' created\n"
               "  [snapshot]    compiler %s | language %s | %s\n"
               "                (nothing left in the dust)\n",
               name, WUBU_COMPILER_VER, sp->language_ver,
               WUBU_SNAPSHOT_DATE);
    }
    return sp;
}

/* the -spaces view: the disorganization, solved */
static int spaces_cb(const wubu_rt_space_t *sp, void *user)
{
    (void)user;
    const char *state = sp->state == WUBU_RT_COLD ? "cold" :
                        sp->state == WUBU_RT_WARM ? "warm" :
                        sp->state == WUBU_RT_LIVE ? "live" : "frozen";
    printf("  %-24s %-12s id=%-3llu heap=%-6llu/%-6llu %s persona=%s\n",
           sp->name, state,
           (unsigned long long)sp->id,
           (unsigned long long)sp->heap_used,
           (unsigned long long)sp->heap_cap,
           sp->created,
           sp->personality ? sp->personality->name : "-");
    return 0;
}

static int run_spaces(void)
{
    wubu_runtime_t *rt = boot_rt();
    if (!rt) return 1;
    size_t n = wubu_runtime_count(rt);
    printf("  [wuburuntime] %zu compilation space(s)\n", n);
    wubu_runtime_list(rt, spaces_cb, NULL);
    return 0;
}

/* compile a file into a named space (with optional personality) */
static int run_into_space(const char *name, const char *personality,
                          const char *path, int roast)
{
    wubu_rt_space_t *sp = space_get(name);
    if (!sp) { fprintf(stderr, "holyc: cannot create space '%s'\n", name); return 1; }

    if (personality) {
        if (wubu_runtime_set_personality(g_rt, sp->id, personality) != 0) {
            fprintf(stderr, "holyc: unknown personality '%s' "
                    "(posix/image/wasi)\n", personality);
            return 1;
        }
        wubu_runtime_set_state(g_rt, sp->id, WUBU_RT_LIVE);
        rt_save();  /* the personality + state persist */
        printf("  [wuburuntime] space '%s' personality -> %s\n",
               name, personality);
    }

    char *src = read_file(path);
    if (!src) { fprintf(stderr, "holyc: cannot read %s\n", path); return 1; }

    if (roast) {
        printf("  [i_make_shit_code] you submitted %s to a C11 compiler.\n"
               "  we judge no language. we compile all of them. we ballin.\n", path);
    }
    int64_t r = hc_eval(src);
    printf("  [%s] result: %lld\n", roast ? "i_make_shit_code" : "space",
           (long long)r);
    free(src);
    return 0;
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

    /* -spaces: the view that ends the disorganization */
    if (!strcmp(argv[1], "-spaces")) return run_spaces();

    /* -space <name> [-personality <kind>] <file>
     *    or -i_make_shit_code -space <name> <file> (either order) */
    if (!strcmp(argv[1], "-space")) {
        if (argc < 4) { usage(); return 2; }
        const char *name = argv[2];
        const char *personality = NULL;
        int idx = 3;
        if (idx < argc && !strcmp(argv[idx], "-personality") && idx + 1 < argc) {
            personality = argv[idx + 1];
            idx += 2;
        }
        if (idx >= argc) { usage(); return 2; }
        return run_into_space(name, personality, argv[idx], 0);
    }
    if (!strcmp(argv[1], "-i_make_shit_code") && argc >= 4 &&
        !strcmp(argv[2], "-space")) {
        return run_into_space(argv[3], NULL, argv[4], 1);
    }
    if (!strcmp(argv[1], "-space") && argc >= 4 &&
        !strcmp(argv[2], "-i_make_shit_code")) {
        return run_into_space(argv[3], NULL, argv[4], 1);
    }

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
