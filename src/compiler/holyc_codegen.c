/*
 * holyc_codegen.c  --  My Seed HolyC Code Generator (Facade)
 * Modular codegen: emit, expr, stmt, api submodules.
 * This file is now a thin facade - real implementation in submodules.
 */

#include "holyc_codegen.h"
#include "holyc_codegen_internal.h"

/* This file intentionally left minimal - all implementation moved to:
 *   holyc_codegen_emit.c    - x86-64 emission helpers, instruction patterns
 *   holyc_codegen_expr.c    - Expression code generation
 *   holyc_codegen_stmt.c    - Statement code generation, hc_gen_init
 *   holyc_codegen_api.c     - Public API: hc_compile, hc_eval, hc_compile_func
 *
 * Internal declarations in holyc_codegen_internal.h
 */

const char *holyc_codegen_version(void) {
    return "HolyC Codegen v1.0 (modular)";
}