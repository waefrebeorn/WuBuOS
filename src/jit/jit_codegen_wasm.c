/*
 * jit_codegen_wasm.c — WebAssembly backend for abstract codegen.
 *
 * WASM is a stack machine — our expression compiler maps naturally.
 * Phase 1: emit instructions into a temp buffer.
 * Phase 2: assemble the complete .wasm module.
 */
#include "jit_codegen.h"
#include "wubu_wasm.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    WasmEnc   body;       /* function body instructions */
    WasmEnc   module;     /* complete module output */
    uint32_t  n_params;
    uint32_t  n_locals;
    int       finalized;
} WasmEncoder;

static WasmEncoder *wasm_enc(CGEncoder *e) { return (WasmEncoder *)e; }

static uint32_t cg_to_wasm_local(CGReg r, uint32_t n_params) {
    /* Minic maps args to CG_REG_1..6, locals to CG_REG_7+, return to CG_REG_0.
     * WASM params are locals 0..n-1.
     * Map: CG_REG_1..6 → WASM local 0..5 (params) */
    if (r >= 1 && r <= 6) return (uint32_t)(r - 1);
    if (r == 0) return 0;  /* return register */
    return n_params + (uint32_t)(r - 7);
}

static void wasm_mov_imm(CGEncoder *e, CGReg rd, int64_t imm) {
    (void)rd;
    wasm_i64_const(&wasm_enc(e)->body, imm);
}

static void wasm_mov_reg(CGEncoder *e, CGReg rd, CGReg rn) {
    (void)rd;
    uint32_t local = cg_to_wasm_local(rn, wasm_enc(e)->n_params);
    wasm_local_get(&wasm_enc(e)->body, local);
}

static void wasm_add_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    (void)rd; (void)rn; (void)rm;
    wasm_i64_add(&wasm_enc(e)->body);
}

static void wasm_sub_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    (void)rd; (void)rn; (void)rm;
    wasm_i64_sub(&wasm_enc(e)->body);
}

static void wasm_mul_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    (void)rd; (void)rn; (void)rm;
    wasm_i64_mul(&wasm_enc(e)->body);
}

static void wasm_div_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    (void)rd; (void)rn; (void)rm;
    wasm_i64_div_s(&wasm_enc(e)->body);
}

static void wasm_mod_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    (void)rd; (void)rn; (void)rm;
    wasm_i64_rem_s(&wasm_enc(e)->body);
}

static void wasm_and_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    (void)rd; (void)rn; (void)rm;
    wasm_i64_and(&wasm_enc(e)->body);
}

static void wasm_orr_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    (void)rd; (void)rn; (void)rm;
    wasm_i64_or(&wasm_enc(e)->body);
}

static void wasm_eor_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    (void)rd; (void)rn; (void)rm;
    wasm_i64_xor(&wasm_enc(e)->body);
}

static void wasm_lsl_imm(CGEncoder *e, CGReg rd, CGReg rn, uint8_t s) {
    (void)rd; (void)rn;
    wasm_i64_const(&wasm_enc(e)->body, s);
    wasm_i64_shl(&wasm_enc(e)->body);
}

static void wasm_lsr_imm(CGEncoder *e, CGReg rd, CGReg rn, uint8_t s) {
    (void)rd; (void)rn;
    wasm_i64_const(&wasm_enc(e)->body, s);
    wasm_i64_shr_s(&wasm_enc(e)->body);
}

static void wasm_asr_imm(CGEncoder *e, CGReg rd, CGReg rn, uint8_t s) {
    (void)rd; (void)rn;
    wasm_i64_const(&wasm_enc(e)->body, s);
    wasm_i64_shr_s(&wasm_enc(e)->body);
}

static void wasm_cmp_imm(CGEncoder *e, CGReg rn, uint32_t imm) {
    uint32_t local = cg_to_wasm_local(rn, wasm_enc(e)->n_params);
    wasm_local_get(&wasm_enc(e)->body, local);
    wasm_i64_const(&wasm_enc(e)->body, (int64_t)imm);
    wasm_i64_eq(&wasm_enc(e)->body);
}

static void wasm_cmp_reg(CGEncoder *e, CGReg rn, CGReg rm) {
    uint32_t local_n = cg_to_wasm_local(rn, wasm_enc(e)->n_params);
    uint32_t local_m = cg_to_wasm_local(rm, wasm_enc(e)->n_params);
    wasm_local_get(&wasm_enc(e)->body, local_n);
    wasm_local_get(&wasm_enc(e)->body, local_m);
    wasm_i64_eq(&wasm_enc(e)->body);
}

static void wasm_cset(CGEncoder *e, CGReg rd, CGCC cc) {
    (void)rd; (void)cc;
    /* Comparison result is i32 on stack — extend to i64 */
    wasm_i64_extend_i32_s(&wasm_enc(e)->body);
}

static void wasm_b_uncond(CGEncoder *e, int32_t off) { (void)e; (void)off; }
static void wasm_b_cond(CGEncoder *e, int32_t offset, CGCC cc) { (void)e; (void)offset; (void)cc; }
static void wasm_b_reg(CGEncoder *e, CGReg rn) { (void)e; (void)rn; }
static void wasm_ret(CGEncoder *e) { wasm_return(&wasm_enc(e)->body); }
static size_t wasm_get_bp(CGEncoder *e) { return wasm_branch_pos(&wasm_enc(e)->body); }
static void wasm_do_patch(CGEncoder *e, size_t pos, size_t target) { wasm_patch_branch(&wasm_enc(e)->body, pos, target); }
static void wasm_do_drop(CGEncoder *e) { wasm_drop(&wasm_enc(e)->body); }
static void wasm_push(CGEncoder *e, CGReg rt) { (void)e; (void)rt; }
static void wasm_pop(CGEncoder *e, CGReg rt) { (void)e; (void)rt; }

static void wasm_add_imm(CGEncoder *e, CGReg rd, CGReg rn, uint32_t imm) {
    (void)rd; (void)rn;
    wasm_i64_const(&wasm_enc(e)->body, (int64_t)imm);
    wasm_i64_add(&wasm_enc(e)->body);
}

static void wasm_sub_imm(CGEncoder *e, CGReg rd, CGReg rn, uint32_t imm) {
    (void)rd; (void)rn;
    wasm_i64_const(&wasm_enc(e)->body, (int64_t)imm);
    wasm_i64_sub(&wasm_enc(e)->body);
}

static void wasm_load(CGEncoder *e, CGReg rt, CGReg base, int32_t off) {
    (void)e; (void)rt; (void)base; (void)off;
}

static void wasm_store(CGEncoder *e, CGReg rt, CGReg base, int32_t off) {
    (void)e; (void)rt; (void)base; (void)off;
}

static void wasm_prologue(CGEncoder *e, int n_args, int stack_slots) {
    (void)e; (void)n_args; (void)stack_slots;
}

static void wasm_epilogue(CGEncoder *e, int stack_slots) {
    (void)stack_slots;
    wasm_end(&wasm_enc(e)->body);
}

static const uint8_t *wasm_buffer(const CGEncoder *e);
static const uint8_t *wasm_build_module(WasmEncoder *enc, size_t *out_size);

static size_t wasm_get_pos(const CGEncoder *e) {
    WasmEncoder *enc = (WasmEncoder *)e;
    if (!enc->finalized) {
        enc->finalized = 1;
        size_t sz;
        wasm_build_module(enc, &sz);
    }
    return enc->module.pos;
}

static void wasm_emit_byte(CGEncoder *e, uint8_t b) { (void)e; (void)b; }
static void wasm_emit_word32(CGEncoder *e, uint32_t w) { (void)e; (void)w; }
static void wasm_emit_word64(CGEncoder *e, uint64_t q) { (void)e; (void)q; }

static const CodeGenVTable wasm_vtable = {
    .name = "wasm",
    .buffer = wasm_buffer,
    .pos = wasm_get_pos,
    .emit_byte = wasm_emit_byte,
    .emit_word32 = wasm_emit_word32,
    .emit_word64 = wasm_emit_word64,
    .add_imm = wasm_add_imm,
    .sub_imm = wasm_sub_imm,
    .add_reg = wasm_add_reg,
    .sub_reg = wasm_sub_reg,
    .mul_reg = wasm_mul_reg,
    .div_reg = wasm_div_reg,
    .mod_reg = wasm_mod_reg,
    .and_reg = wasm_and_reg,
    .orr_reg = wasm_orr_reg,
    .eor_reg = wasm_eor_reg,
    .lsl_imm = wasm_lsl_imm,
    .lsr_imm = wasm_lsr_imm,
    .asr_imm = wasm_asr_imm,
    .mov_imm = wasm_mov_imm,
    .mov_reg = wasm_mov_reg,
    .load = wasm_load,
    .store = wasm_store,
    .cmp_imm = wasm_cmp_imm,
    .cmp_reg = wasm_cmp_reg,
    .cset = wasm_cset,
    .b_uncond = wasm_b_uncond,
    .b_cond = wasm_b_cond,
    .b_reg = wasm_b_reg,
    .ret = wasm_ret,
    .branch_pos = wasm_get_bp,
    .patch_branch = wasm_do_patch,
    .push = wasm_push,
    .pop = wasm_pop,
    .drop = wasm_do_drop,
    .prologue = wasm_prologue,
    .epilogue = wasm_epilogue,
};

/* -- Module assembly ----------------------------------------------- */
static const uint8_t *wasm_build_module(WasmEncoder *enc, size_t *out_size) {
    WasmEnc *m = &enc->module;
    m->pos = 0;

    /* Magic + version */
    wasm_emit(m, 0x00); wasm_emit(m, 0x61); wasm_emit(m, 0x73); wasm_emit(m, 0x6d);
    wasm_emit(m, 0x01); wasm_emit(m, 0x00); wasm_emit(m, 0x00); wasm_emit(m, 0x00);

    /* Type section (0x01): 1 function type (i64,i64,i64,i64,i64,i64) -> i64 */
    wasm_emit(m, 0x01);
    {
        uint8_t body[32];
        WasmEnc be = {body, 0, 32, 0};
        wasm_write_leb_u32(&be, 1);  /* 1 type */
        wasm_emit(&be, 0x60);         /* func */
        wasm_write_leb_u32(&be, enc->n_params);  /* n params */
        for (uint32_t i = 0; i < enc->n_params; i++) wasm_emit(&be, 0x7e);  /* i64 */
        wasm_write_leb_u32(&be, 1);   /* 1 result */
        wasm_emit(&be, 0x7e);         /* i64 */
        wasm_write_leb_u32(m, (uint32_t)be.pos);
        for (size_t i = 0; i < be.pos; i++) wasm_emit(m, body[i]);
    }

    /* Function section (0x03): 1 function with type 0 */
    wasm_emit(m, 0x03);
    {
        uint8_t body[2] = {0x01, 0x00};
        wasm_write_leb_u32(m, 2);
        wasm_emit(m, body[0]); wasm_emit(m, body[1]);
    }

    /* Export section (0x07): export function 0 as "f" */
    wasm_emit(m, 0x07);
    {
        uint8_t body[8];
        size_t len = 0;
        WasmEnc be = {body, 0, 8, 0};
        wasm_write_leb_u32(&be, 1);  /* 1 export */
        wasm_write_leb_u32(&be, 1);  /* name length */
        wasm_emit(&be, 'f');          /* name */
        wasm_emit(&be, 0x00);         /* kind = function */
        wasm_write_leb_u32(&be, 0);   /* index */
        wasm_write_leb_u32(m, (uint32_t)be.pos);
        for (size_t i = 0; i < be.pos; i++) wasm_emit(m, body[i]);
    }

    /* Code section (0x0a): 1 function body */
    wasm_emit(m, 0x0a);
    {
        /* Build body: locals declaration + code */
        uint8_t body[1024];
        WasmEnc be = {body, 0, 1024, 0};
        wasm_write_leb_u32(&be, 1);  /* number of function bodies */
        /* Function body size (will be patched) */
        size_t body_start = be.pos;
        wasm_write_leb_u32(&be, 0);  /* placeholder */
        /* Locals: 1 group of (n_locals) i64 */
        wasm_write_leb_u32(&be, 1);  /* 1 local group */
        wasm_write_leb_u32(&be, enc->n_locals);
        wasm_emit(&be, 0x7e);         /* i64 */
        /* Copy function body */
        for (size_t i = 0; i < enc->body.pos; i++) wasm_emit(&be, enc->body.buf[i]);
        /* Compute and write body size (LEB) */
        uint32_t body_size = (uint32_t)(be.pos - body_start - 1);
        /* Write LEB at body_start, shifting if needed */
        uint8_t leb[2];
        int leb_n = 0;
        {
            uint32_t v = body_size;
            do { uint8_t b = v & 0x7F; v >>= 7; if (v) b |= 0x80; leb[leb_n++] = b; } while (v);
        }
        if (leb_n > 1) {
            /* Shift to make room */
            memmove(body + body_start + leb_n, body + body_start + 1, be.pos - body_start - 1);
        }
        for (int i = 0; i < leb_n; i++) body[body_start + i] = leb[i];
        be.pos = body_start + leb_n + body_size;
        /* Write section */
        wasm_write_leb_u32(m, (uint32_t)be.pos);
        for (size_t i = 0; i < be.pos; i++) wasm_emit(m, body[i]);
    }

    *out_size = m->pos;
    return m->buf;
}

static const uint8_t *wasm_buffer(const CGEncoder *e) {
    WasmEncoder *enc = (WasmEncoder *)e;
    if (!enc->finalized) {
        enc->finalized = 1;
        size_t sz;
        wasm_build_module(enc, &sz);
    }
    return enc->module.buf;
}

CodeGen *cg_create_wasm(void) {
    CodeGen *cg = (CodeGen *)calloc(1, sizeof(CodeGen));
    WasmEncoder *enc = (WasmEncoder *)calloc(1, sizeof(WasmEncoder));
    if (!cg || !enc) { free(cg); free(enc); return NULL; }
    cg->vt = &wasm_vtable;
    cg->enc = (CGEncoder *)enc;
    cg->backend = 3;
    wasm_enc_init_dynamic(&enc->body, 4096);
    wasm_enc_init_dynamic(&enc->module, 4096);
    enc->n_params = 6;
    enc->n_locals = 10;
    return cg;
}
