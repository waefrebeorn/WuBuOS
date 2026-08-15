/*
 * mir_driver_test.c -- the DRIVER SPACE battery.
 *
 * The same MIR program is compiled by EVERY ISA driver and executed:
 *   - x86-64: mmap+JIT (native)
 *   - 8086:   16-bit real-mode, runs via the repo's in-process DOS emulator
 *   - m68k:   Motorola 68000 encoder + interpreter
 *   - 6502:   MOS 6502 encoder + interpreter (the 8-bit proof)
 *   - riscv:  RV64I encoder + interpreter
 * The results must ALL agree with the gcc-verified expected value.
 * Any divergence is a FINDING (the bug-bank doctrine applied to the
 * whole driver space).
 *
 * Usage:  mir_driver_test   (runs the full battery on all drivers)
 *         mir_driver_test <expr> <expected>  (one expression)
 *
 * C11, self-contained. Links holyc (for the AST), wubu_mir, and all
 * drivers + interpreters.
 */
#include "holyc.h"
#include "wubu_mir.h"
#include "wubu_mir_lower.h"
#include "wubu_isa_driver.h"
#include "holyc_lexer.h"
#include "holyc_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static int total = 0;
static int driver_count = 0;

static int run_one(const char *expr, int64_t expected)
{
    total++;

    HCLexer lex;
    hc_lex_init(&lex, expr);
    if (lex.has_error) { printf("  FAIL parse: %s\n", expr); failures++; return 1; }
    HCParser parse;
    hc_parse_init(&parse, &lex);
    HCASTNode *ast = hc_parse_expr(&parse);
    if (!ast || parse.has_error) {
        printf("  FAIL parse: %s\n", expr);
        failures++;
        return 1;
    }

    /* AST -> MIR (the hourglass neck) */
    wubu_mir_prog_t prog;
    wubu_mir_init(&prog);
    wubu_vr_t result = wubu_mir_lower_expr(&prog, ast);
    wubu_mir_ret(&prog, result);

    /* every driver: compile + run */
    const char *names[] = { "x86-64", "8086", "m68k", "6502", "riscv", "z80" };
    int nd = 0;
    const wubu_isa_driver_t *drv[6] = {0};
    int64_t results[6] = {0};
    int ok = 1;
    char detail[512] = {0};

    for (int i = 0; i < 6; i++) {
        const wubu_isa_driver_t *d = wubu_isa_find(names[i]);
        if (!d) continue;
        uint8_t *code = NULL;
        size_t csize = 0;
        if (d->compile(&prog, &code, &csize) != 0 || !code) {
            snprintf(detail + strlen(detail), sizeof(detail) - strlen(detail),
                     "%s:COMPILE-FAIL ", names[i]);
            ok = 0;
            continue;
        }
        drv[nd] = d;
        results[nd] = d->run(code, csize, 0);
        free(code);
        if (results[nd] != expected) {
            snprintf(detail + strlen(detail), sizeof(detail) - strlen(detail),
                     "%s:%lld ", names[i], (long long)results[nd]);
            ok = 0;
        }
        nd++;
    }

    if (driver_count < nd) driver_count = nd;

    if (!ok) {
        printf("  \u26a0 FINDING: %s (expected %lld)  [%s]\n",
               expr, (long long)expected, detail);
        failures++;
    } else {
        printf("  ok: %s = %lld (", expr, (long long)expected);
        for (int i = 0; i < nd; i++) {
            printf("%s=%lld%s", drv[i]->name, (long long)results[i],
                   i + 1 < nd ? " " : "");
        }
        printf(")\n");
    }

    wubu_mir_free(&prog);
    hc_ast_free(ast);
    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *names[] = { "x86-64", "8086", "m68k", "6502", "riscv", "z80" };
    int ndrv = 0;
    for (int i = 0; i < 6; i++) {
        if (wubu_isa_find(names[i])) ndrv++;
    }

    printf("=== mir_driver_test: the driver space (%d drivers) ===\n", ndrv);

    if (argc == 3) {
        return run_one(argv[1], strtoll(argv[2], NULL, 10)) ? 1 : 0;
    }

    struct { const char *e; int64_t v; } B[] = {
        { "1+2", 3 }, { "7*6", 42 }, { "(1<<4)|3", 19 }, { "100/7", 14 },
        { "-5+10", 5 }, { "3>2 ? 1 : 0", 1 }, { "1<<2+1", 8 },
        { "-2*-3", 6 }, { "5^3", 6 }, { "-7%3", -1 }, { "1|2&4", 1 },
        { "((2+3)*4-6)/2", 7 }, { "1<2==1", 1 }, { "(1+2)*3", 9 },
        { "(1|2)", 3 }, { "(5&3)", 1 }, { "1+(2*3)", 7 }, { "10-(3*2)", 4 },
        { "1+(2+3)", 6 }, { "(1+2)+(3+4)", 10 }, { "5-(2-1)", 4 },
        { "1<<(1+2)", 8 }, { "16>>(1+1)", 4 }, { "1<(2-1)", 0 },
        { "(1+2)==(2+1)", 1 }, { "3>(1+1)", 1 }, { "2+3*4", 14 },
        { "(2+3)*4", 20 }, { "1|2^3", 1 }, { "8>>1+1", 2 }, { "~0", -1 },
        { "1&&0", 0 }, { "1||0", 1 },
    };
    int n = (int)(sizeof(B) / sizeof(B[0]));
    for (int i = 0; i < n; i++) run_one(B[i].e, B[i].v);

    printf("\n=== %s (%d drivers, %d/%d expressions) ===\n",
           failures == 0 ? "DRIVER SPACE PASSED" : "DRIVER SPACE FAILED",
           driver_count, total - failures, total);
    return failures == 0 ? 0 : 1;
}
