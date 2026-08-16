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
#include "holyc_lexer.h"
#include "holyc_parser.h"
#include "wubu_mir.h"
#include "wubu_mir_lower.h"
#include "wubu_isa_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int bf_run(const char *src);   /* from brainfuck.c */

/* Forward declarations for emit functions */
static int compile_main(const char *src_file, HCGen *out_gen, HCFunction *out_main);
static int run_emit_elf(const char *src_file, const char *out_file);
static int run_emit_pe(const char *src_file, const char *out_file);
static int run_emit_bin(const char *src_file, const char *out_file);

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
        "  holyc -spaces              list the compilation spaces\n"
        "  holyc -targets             list the ISA driver targets\n"
        "  holyc -target <isa> <f>    compile <f> for the ISA driver\n"
        "                             (one MIR -> x86-64/8086/m68k/6502/\n"
        "                             riscv/z80) and run it\n");
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

/* -targets: list every ISA driver in the driver space (the compiler
 * reaching ALL machine code — one MIR, N backends). */
static int run_targets(void)
{
    const char *names[] = { "x86-64", "8086", "m68k", "6502", "riscv", "z80" };
    int n = (int)(sizeof(names) / sizeof(names[0]));
    int found = 0;
    printf("  [targets] the ISA driver space (one MIR -> N backends):\n");
    for (int i = 0; i < n; i++) {
        const wubu_isa_driver_t *d = wubu_isa_find(names[i]);
        if (!d) continue;
        printf("    %-8s  %-12s  %s\n", d->name, d->family,
               d->exec == WUBU_ISA_NATIVE ? "native JIT" : "interpreter");
        found++;
    }
    printf("  [targets] %d driver(s) reachable\n", found);
    return found ? 0 : 1;
}

/* -target <isa> <file>: compile a source FILE for the chosen ISA via
 * AST -> MIR (the hourglass neck) -> the driver's compile, then run it
 * on the driver's interpreter/native JIT. Every driver that is
 * reachable produces a machine-code program that RUNS — this is the
 * "compile to ALL machine code" doctrine, made a real CLI flag. */
static int run_target(const char *isa, const char *path)
{
    const wubu_isa_driver_t *d = wubu_isa_find(isa);
    if (!d) {
        fprintf(stderr, "holyc: unknown target '%s' (x86-64/8086/m68k/6502/"
                "riscv/z80)\n", isa);
        return 2;
    }

    char *src = read_file(path);
    if (!src) { fprintf(stderr, "holyc: cannot read %s\n", path); return 1; }

    /* strip a trailing newline/semicolon so the expr-parse is clean */
    size_t len = strlen(src);
    while (len > 0 && (src[len-1] == '\n' || src[len-1] == ';' ||
                       src[len-1] == ' ' || src[len-1] == '\r'))
        src[--len] = 0;

    HCLexer lex;
    hc_lex_init(&lex, src);
    if (lex.has_error) {
        fprintf(stderr, "holyc: lex error in %s\n", path);
        free(src); return 1;
    }
    HCParser parse;
    hc_parse_init(&parse, &lex);
    HCASTNode *ast = hc_parse_expr(&parse);
    if (!ast || parse.has_error) {
        fprintf(stderr, "holyc: parse error in %s\n", path);
        free(src); return 1;
    }

    /* AST -> MIR (the hourglass neck — ISA-neutral) */
    wubu_mir_prog_t prog;
    wubu_mir_init(&prog);
    wubu_vr_t result = wubu_mir_lower_expr(&prog, ast);
    wubu_mir_ret(&prog, result);
    wubu_mir_dump(&prog);

    /* driver.compile(MIR) -> machine code */
    uint8_t *code = NULL;
    size_t csize = 0;
    if (d->compile(&prog, &code, &csize) != 0 || !code) {
        fprintf(stderr, "holyc: target '%s' compile failed\n", d->name);
        wubu_mir_free(&prog); hc_ast_free(ast); free(src);
        return 1;
    }

    printf("  [target] %s: %zu bytes of %s machine code\n",
           d->name, csize,
           d->exec == WUBU_ISA_NATIVE ? "native" : d->family);

    /* driver.run(code) -> the program's result */
    int64_t r = d->run(code, csize, 0);
    free(code);
    printf("  [target] %s ran -> %lld\n", d->name, (long long)r);

    wubu_mir_free(&prog);
    hc_ast_free(ast);
    free(src);
    return 0;
}

/* -emit-elf <source> <output>: compile HolyC source to an ELF64 executable. */
static int run_emit_elf(const char *src_file, const char *out_file) {
    HCGen gen;
    HCFunction mainfunc;
    if (compile_main(src_file, &gen, &mainfunc) != 0) return 1;

    if (!mainfunc.func_ptr || mainfunc.code_size == 0) {
        fprintf(stderr, "holyc: main() has no code\n");
        free(gen.code); free(gen.data);
        return 1;
    }

    /* Copy function body and patch globals */
    size_t func_size = mainfunc.code_size;
    uint8_t *func_code = (uint8_t *)malloc(func_size);
    if (!func_code) {
        fprintf(stderr, "holyc: malloc failed\n");
        free(gen.code); free(gen.data);
        return 1;
    }
    memcpy(func_code, mainfunc.func_ptr, func_size);

    /* Patch RIP-relative globals */
    for (int i = 0; i < mainfunc.n_global_patches; i++) {
        size_t pp = mainfunc.global_patches[i].code_patch_pos;
        size_t go = mainfunc.global_patches[i].global_offset;
        if (pp + 4 <= func_size) {
            int32_t disp32 = (int32_t)(func_size + go - pp - 4);
            memcpy(func_code + pp, &disp32, 4);
        }
    }

    size_t patch_offsets[1] = {0}, patch_globals[1] = {0};
    if (hc_write_elf(out_file, func_code, func_size,
                    gen.data, gen.data_size,
                    patch_offsets, patch_globals, 0) != 0) {
        fprintf(stderr, "holyc: failed to write %s\n", out_file);
        free(func_code); free(gen.code); free(gen.data);
        return 1;
    }

    printf("holyc: %s (%zu bytes code, %zu bytes data)\n",
           out_file, func_size, gen.data_size);
    free(func_code);
    free(gen.code);
    free(gen.data);
    return 0;
}

/* Shared: compile HolyC source and extract main() function body */
static int compile_main(const char *src_file, HCGen *out_gen, HCFunction *out_main) {
    char *src = read_file(src_file);
    if (!src) { fprintf(stderr, "holyc: cannot read %s\n", src_file); return 1; }

    HCLexer lex;
    hc_lex_init(&lex, src);
    if (lex.has_error) { fprintf(stderr, "holyc: %s\n", lex.error); free(src); return 1; }

    HCParser parse;
    hc_parse_init(&parse, &lex);
    HCASTNode *ast = hc_parse_compilation_unit(&parse);
    if (parse.has_error || !ast) {
        fprintf(stderr, "holyc: parse error\n");
        hc_ast_free(ast); free(src); return 1;
    }

    HCGen gen;
    hc_gen_init(&gen);
    emit_prologue(&gen);

    if (ast->kind == HC_AST_BLOCK) {
        for (int i = 0; i < ast->n_stmts; i++) {
            HCASTNode *stmt = ast->stmts[i];
            if (stmt->kind == HC_AST_IF || stmt->kind == HC_AST_WHILE ||
                stmt->kind == HC_AST_FOR || stmt->kind == HC_AST_DO_WHILE ||
                stmt->kind == HC_AST_VAR_DECL || stmt->kind == HC_AST_FUNC_DECL) {
                gen_stmt(&gen, stmt);
            } else {
                gen_expr(&gen, stmt);
            }
        }
    }
    emit_epilogue(&gen);
    hc_ast_free(ast);
    free(src);

    if (gen.has_error) {
        fprintf(stderr, "holyc: codegen failed: %s\n", gen.error);
        free(gen.code); free(gen.data);
        return 1;
    }

    /* Find main() */
    int main_idx = -1;
    for (int f = 0; f < gen.n_functions; f++) {
        if (strcmp(gen.functions[f].name, "main") == 0) {
            main_idx = f;
            break;
        }
    }
    if (main_idx < 0) {
        fprintf(stderr, "holyc: no main() function found\n");
        free(gen.code); free(gen.data);
        return 1;
    }

    *out_gen = gen;
    *out_main = gen.functions[main_idx];
    return 0;
}

/* -emit-pe: compile to PE32+ executable */
static int run_emit_pe(const char *src_file, const char *out_file) {
    HCGen gen;
    HCFunction mainfunc;
    if (compile_main(src_file, &gen, &mainfunc) != 0) return 1;

    if (!mainfunc.func_ptr || mainfunc.code_size == 0) {
        fprintf(stderr, "holyc: main() has no code\n");
        free(gen.code); free(gen.data);
        return 1;
    }

    /* Copy function body and patch globals */
    size_t func_size = mainfunc.code_size;
    uint8_t *func_code = (uint8_t *)malloc(func_size);
    if (!func_code) {
        fprintf(stderr, "holyc: malloc failed\n");
        free(gen.code); free(gen.data);
        return 1;
    }
    memcpy(func_code, mainfunc.func_ptr, func_size);

    /* Patch RIP-relative globals */
    for (int i = 0; i < mainfunc.n_global_patches; i++) {
        size_t pp = mainfunc.global_patches[i].code_patch_pos;
        size_t go = mainfunc.global_patches[i].global_offset;
        if (pp + 4 <= func_size) {
            int32_t disp32 = (int32_t)(func_size + go - pp - 4);
            memcpy(func_code + pp, &disp32, 4);
        }
    }

    size_t patch_offsets[1] = {0}, patch_globals[1] = {0};
    if (hc_write_pe(out_file, func_code, func_size,
                    gen.data, gen.data_size,
                    patch_offsets, patch_globals, 0) != 0) {
        fprintf(stderr, "holyc: failed to write %s\n", out_file);
        free(func_code); free(gen.code); free(gen.data);
        return 1;
    }

    printf("holyc: %s (%zu bytes code)\n", out_file, func_size);
    free(func_code);
    free(gen.code);
    free(gen.data);
    return 0;
}

/* -emit-bin: compile to raw binary */
static int run_emit_bin(const char *src_file, const char *out_file) {
    HCGen gen;
    HCFunction mainfunc;
    if (compile_main(src_file, &gen, &mainfunc) != 0) return 1;

    if (!mainfunc.func_ptr || mainfunc.code_size == 0) {
        fprintf(stderr, "holyc: main() has no code\n");
        free(gen.code); free(gen.data);
        return 1;
    }

    /* Copy function body and patch globals */
    size_t func_size = mainfunc.code_size;
    uint8_t *func_code = (uint8_t *)malloc(func_size);
    if (!func_code) {
        fprintf(stderr, "holyc: malloc failed\n");
        free(gen.code); free(gen.data);
        return 1;
    }
    memcpy(func_code, mainfunc.func_ptr, func_size);

    /* Patch globals */
    for (int i = 0; i < mainfunc.n_global_patches; i++) {
        size_t pp = mainfunc.global_patches[i].code_patch_pos;
        size_t go = mainfunc.global_patches[i].global_offset;
        if (pp + 4 <= func_size) {
            int32_t disp32 = (int32_t)(func_size + go - pp - 4);
            memcpy(func_code + pp, &disp32, 4);
        }
    }

    if (hc_write_bin(out_file, func_code, func_size, 0) != 0) {
        fprintf(stderr, "holyc: failed to write %s\n", out_file);
        free(func_code); free(gen.code); free(gen.data);
        return 1;
    }

    printf("holyc: %s (%zu bytes raw)\n", out_file, func_size);
    free(func_code);
    free(gen.code);
    free(gen.data);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) { usage(); return 2; }

    /* -spaces: the view that ends the disorganization */
    if (!strcmp(argv[1], "-spaces")) return run_spaces();

    /* -targets: list every ISA driver. -target <isa> <file>: compile
     * for that ISA (one MIR -> N backends, the hourglass neck). */
    if (!strcmp(argv[1], "-targets")) return run_targets();
    if (!strcmp(argv[1], "-target")) {
        if (argc < 4) { usage(); return 2; }
        return run_target(argv[2], argv[3]);
    }

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
    if (!strcmp(argv[1], "-emit-elf")) {
        if (argc < 4) { usage(); return 2; }
        return run_emit_elf(argv[2], argv[3]);
    }
    if (!strcmp(argv[1], "-emit-pe")) {
        if (argc < 4) { usage(); return 2; }
        return run_emit_pe(argv[2], argv[3]);
    }
    if (!strcmp(argv[1], "-emit-bin")) {
        if (argc < 4) { usage(); return 2; }
        return run_emit_bin(argv[2], argv[3]);
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
