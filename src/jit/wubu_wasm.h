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
void wasm_end(WasmEnc *e);
void wasm_return(WasmEnc *e);
void wasm_drop(WasmEnc *e);

/* -- Branch fixups ------------------------------------------------ */
size_t wasm_branch_pos(const WasmEnc *e);
void wasm_patch_branch(WasmEnc *e, size_t pos, size_t target);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_WASM_H */
