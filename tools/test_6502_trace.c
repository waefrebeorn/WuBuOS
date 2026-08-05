#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "wubu_mir.h"
#include "wubu_mir_lower.h"
#include "wubu_isa_driver.h"

int main() {
    const char *expr = "1+2";
    HCLexer lex;
    hc_lex_init(&lex, expr);
    HCParser parse;
    hc_parse_init(&parse, &lex);
    HCASTNode *ast = hc_parse_expr(&parse);
    
    wubu_mir_prog_t prog;
    wubu_mir_init(&prog);
    wubu_vr_t vr_result = wubu_mir_lower_expr(&prog, ast);
    wubu_mir_ret(&prog, vr_result);
    
    printf("MIR program: %zu instructions\n", prog.n);
    for (size_t i = 0; i < prog.n; i++) {
        printf("  [%zu] op=%d dst=%u a=%u b=%u imm=%ld label=%u\n",
               i, prog.ins[i].op, prog.ins[i].dst,
               prog.ins[i].a, prog.ins[i].b, (long)prog.ins[i].imm, prog.ins[i].label);
    }
    
    const wubu_isa_driver_t *d = wubu_isa_find("6502");
    if (!d) { printf("6502 driver not found!\n"); return 1; }
    
    uint8_t *code = NULL;
    size_t csize = 0;
    if (d->compile(&prog, &code, &csize) != 0) {
        printf("Compile failed!\n");
        return 1;
    }
    
    printf("\n6502 code (%zu bytes):\n", csize);
    for (size_t i = 0; i < csize; i++) {
        printf("  [%zu] 0x%02X", i, code[i]);
        uint8_t op = code[i];
        if (op == 0xA9) printf(" LDA #imm");
        else if (op == 0xA5) printf(" LDA zp");
        else if (op == 0x85) printf(" STA zp");
        else if (op == 0x69) printf(" ADC #imm");
        else if (op == 0x65) printf(" ADC zp");
        else if (op == 0x18) printf(" CLC");
        else if (op == 0x38) printf(" SEC");
        else if (op == 0x00) printf(" BRK");
        else if (op == 0x4C) printf(" JMP abs");
        else if (op == 0xF0) printf(" BEQ rel8");
        else if (op == 0xEA) printf(" NOP");
        else if (op == 0x78) printf(" SEI");
        else if (op == 0xD8) printf(" CLD");
        else if (op == 0x06) printf(" ASL zp");
        else if (op == 0x46) printf(" LSR zp");
        else if (op == 0xC6) printf(" DEC zp");
        else if (op == 0xE6) printf(" INC zp");
        else if (op == 0xAD) printf(" LDA abs");
        else if (op == 0x8D) printf(" STA abs");
        else if (op == 0x6D) printf(" ADC abs");
        else if (op == 0x0A) printf(" ASL A");
        else if (op == 0x4A) printf(" LSR A");
        else printf(" (unknown)");
        printf("\n");
    }
    
    int64_t run_result = d->run(code, csize, 0);
    printf("\nInterpreter result: %ld\n", (long)run_result);
    
    free(code);
    wubu_mir_free(&prog);
    hc_ast_free(ast);
    return 0;
}