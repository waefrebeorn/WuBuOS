#include <stdio.h>
#include <stdlib.h>
#include "wubu_mir.h"
#include "wubu_isa_driver.h"

/* Direct declaration of MIPS interpreter */
extern int64_t wubu_mips_run(const uint8_t *code, size_t size, int64_t arg);

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    wubu_mir_prog_t prog;
    wubu_mir_init(&prog);

    /* Test: 7 + 6 = 13 */
    wubu_vr_t a = wubu_mir_const(&prog, 7);
    wubu_vr_t b = wubu_mir_const(&prog, 6);
    wubu_vr_t r = wubu_mir_binop(&prog, MIR_ADD, a, b);
    wubu_mir_ret(&prog, r);

    uint8_t *code = NULL; size_t csize = 0;
    if (wubu_isa_mips.compile(&prog, &code, &csize) != 0 || !code) {
        printf("MIPS: COMPILE FAIL\n");
        return 1;
    }

    printf("MIPS code for 7+6 (%zu bytes):\n  ", csize);
    for (size_t i = 0; i < csize; i++) printf("%02x ", code[i]);
    printf("\n");

    int64_t result = wubu_mips_run(code, csize, 0);
    printf("MIPS: 7+6 = %lld (expected 13) %s\n", (long long)result,
           result == 13 ? "OK" : "WRONG");

    free(code);
    wubu_mir_free(&prog);
    return result == 13 ? 0 : 1;
}
