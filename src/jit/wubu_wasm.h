/*
 * wubu_wasm.h — WebAssembly/WASM encoder interface.
 */
#ifndef WUBU_WASM_H
#define WUBU_WASM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *buf;
    size_t   pos;
    size_t   cap;
    int      owns_buf;
} WasmEnc;

typedef struct {
    WasmEnc   body;       /* function body instructions */
    WasmEnc   module;     /* complete module output */
    uint32_t  n_params;
    uint32_t  n_locals;
    int       finalized;
    int       label_depth;
} WasmEncoder;

void wasm_enc_init(WasmEnc *e, uint8_t *buf, size_t cap);
void wasm_enc_init_dynamic(WasmEnc *e, size_t initial_cap);
void wasm_enc_free(WasmEnc *e);
void wasm_emit(WasmEnc *e, uint8_t b);

/* -- LEB128 encoding ----------------------------------------------- */
void wasm_write_leb_u32(WasmEnc *e, uint32_t val);
void wasm_write_leb_i64(WasmEnc *e, int64_t val);

/* -- Instructions -------------------------------------------------- */
void wasm_i64_const(WasmEnc *e, int64_t val);
void wasm_i64_add(WasmEnc *e);
void wasm_i64_sub(WasmEnc *e);
void wasm_i64_mul(WasmEnc *e);
void wasm_i64_div_s(WasmEnc *e);
void wasm_i64_rem_s(WasmEnc *e);
void wasm_i64_and(WasmEnc *e);
void wasm_i64_or(WasmEnc *e);
void wasm_i64_xor(WasmEnc *e);
void wasm_i64_shl(WasmEnc *e);
void wasm_i64_shr_s(WasmEnc *e);
void wasm_i64_eq(WasmEnc *e);
void wasm_i64_ne(WasmEnc *e);
void wasm_i64_lt_s(WasmEnc *e);
void wasm_i64_gt_s(WasmEnc *e);
void wasm_i64_le_s(WasmEnc *e);
void wasm_i64_ge_s(WasmEnc *e);
void wasm_local_get(WasmEnc *e, uint32_t idx);
void wasm_local_set(WasmEnc *e, uint32_t idx);
void wasm_i64_extend_i32_s(WasmEnc *e);
void wasm_drop(WasmEnc *e);
void wasm_block(WasmEnc *e);      /* 0x02 */
void wasm_block_i64(WasmEnc *e);  /* 0x02 with i64 result */
void wasm_loop(WasmEnc *e);       /* 0x03 */
void wasm_if(WasmEnc *e);         /* 0x04 */
void wasm_else(WasmEnc *e);       /* 0x05 */
void wasm_end(WasmEnc *e);        /* 0x0b end */
void wasm_return(WasmEnc *e);     /* 0x0f return */
void wasm_br(WasmEnc *e, uint32_t label);     /* 0x0c */
void wasm_br_if(WasmEnc *e, uint32_t label);  /* 0x0d */

/* -- Branch fixups ------------------------------------------------ */
size_t wasm_branch_pos(const WasmEnc *e);
void wasm_patch_branch(WasmEnc *e, size_t pos, size_t target);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_WASM_H */
