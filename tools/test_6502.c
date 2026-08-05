#include "holyc.h"
#include "wubu_mir.h"
#include "wubu_mir_lower.h"
#include "wubu_isa_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run_one(const char *expr, int64_t expected)
{
    HCLexer lex;
    hc_lex_init(&lex, expr);
    if (lex.has_error) { printf("  FAIL parse: %s\n", expr); return 1; }
    HCParser parse;
    hc_parse_init(&parse, &lex);
    HCASTNode *ast = hc_parse_expr(&parse);
    if (!ast || parse.has_error) { printf("  FAIL parse: %s\n", expr); return 1; }

    wubu_mir_prog_t prog;
    wubu_mir_init(&prog);
    wubu_vr_t result = wubu_mir_lower_expr(&prog, ast);
    wubu_mir_ret(&prog, result);

    /* Just test 6502 */
    const wubu_isa_driver_t *d = wubu_isa_find("6502");
    if (!d) { printf("  6502 driver not found\n"); return 1; }

    printf("  6502 driver found, compiling...\n");
    uint8_t *code = NULL;
    size_t csize = 0;
    if (d->compile(&prog, &code, csize) != 0 || !code) {
        printf("  6502 compile failed\n");
        wubu_mir_free(&prog);
        hc_ast_free(ast);
        return 1;
    }
    printf("  6502 compiled %zu bytes\n", csize);

    printf("  6502 running...\n");
    int64_t r = d->run(code, csize, 0);
    printf("  6502 result: %lld (expected %lld)\n", (long long)r, (long long)expected);

    free(code);
    wubu_mir_free(&prog);
    hc_ast_free(ast);
    return r == expected ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc == 3) {
        return run_one(argv[1], strtoll(argv[2], NULL, 10)) ? 1 : 0;
    }

    printf("Testing 6502 with 1+2=3:\n");
    return run_one("1+2", 3);
}