/*
 * holyc_codegen.h  --  HolyC Code Generator
 * Emits x86-64 machine code from AST. Self-contained, C11.
 */
#ifndef MYSEED_HOLYC_CODEGEN_H
#define MYSEED_HOLYC_CODEGEN_H

#include "holyc_types.h"

/* JIT Memory Management */
void *jit_alloc_exec(size_t size);
void jit_free_exec(void *ptr, size_t size);

/* Code Generator API */
void hc_gen_init(HCGen *gen);
int hc_gen_node(HCGen *gen, const HCASTNode *node);
int hc_gen_function(HCGen *gen, const HCASTNode *func);
int gen_expr(HCGen *gen, const HCASTNode *node);
int gen_stmt(HCGen *gen, const HCASTNode *node);
void emit_prologue(HCGen *gen);
void emit_epilogue(HCGen *gen);

/* Get generated machine code */
const uint8_t *hc_gen_code(const HCGen *gen, size_t *out_size);

/* High-level compile/execute */
void *hc_compile(const char *source, size_t *out_size);
int64_t hc_eval(const char *source);

#endif /* MYSEED_HOLYC_CODEGEN_H */