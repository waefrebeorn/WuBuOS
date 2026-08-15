#include <stdio.h>
#include <stdlib.h>
#include "wubu_mir.h"
#include "wubu_isa_driver.h"

int main(void) {
    wubu_mir_prog_t prog;
    wubu_mir_init(&prog);
    wubu_vr_t a = wubu_mir_const(&prog, 7);
    wubu_vr_t b = wubu_mir_const(&prog, 6);
    wubu_vr_t r = wubu_mir_binop(&prog, MIR_ADD, a, b);
    wubu_mir_ret(&prog, r);

    const wubu_isa_driver_t *d = wubu_isa_find("avr");
    if (!d) { printf("AVR: NOT FOUND\n"); return 1; }

    uint8_t *code = NULL; size_t csize = 0;
    if (d->compile(&prog, &code, &csize) != 0 || !code) {
        printf("AVR: COMPILE FAIL\n"); return 1;
    }

    printf("AVR code for 7+6 (%zu bytes): ", csize);
    for (size_t i = 0; i < csize; i++) printf("%02x ", code[i]);
    printf("\n");

    int64_t result = d->run(code, csize, 0);
    printf("AVR: 7+6 = %lld (expected 13) %s\n", (long long)result,
           result == 13 ? "OK" : "WRONG");

    free(code);
    wubu_mir_free(&prog);
    return result == 13 ? 0 : 1;
}
