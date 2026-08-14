/*
 * wubu_wasm.c — WebAssembly binary encoder.
 *
 * WASM is a stack machine — our expression compiler maps naturally.
 * We buffer the function body, then assemble the complete module.
 */
#include "wubu_wasm.h"
#include <stdlib.h>
#include <string.h>

static void wasm_grow(WasmEnc *e, size_t need) {
    if (e->pos + need <= e->cap) return;
    size_t new_cap = e->cap ? e->cap * 2 : 256;
    while (new_cap < e->pos + need) new_cap *= 2;
    e->buf = (uint8_t *)realloc(e->buf, new_cap);
    e->cap = new_cap;
}

void wasm_emit(WasmEnc *e, uint8_t b) {
    wasm_grow(e, 1);
    e->buf[e->pos++] = b;
}

void wasm_enc_init(WasmEnc *e, uint8_t *buf, size_t cap) {
    e->buf = buf; e->pos = 0; e->cap = cap; e->owns_buf = 0;
}

void wasm_enc_init_dynamic(WasmEnc *e, size_t initial_cap) {
    if (initial_cap == 0) initial_cap = 256;
    e->buf = (uint8_t *)malloc(initial_cap);
    e->cap = e->buf ? initial_cap : 0;
    e->pos = 0;
    e->owns_buf = 1;
}

void wasm_enc_free(WasmEnc *e) {
    if (e && e->owns_buf && e->buf) { free(e->buf); e->buf = NULL; }
}

/* -- LEB128 encoding ----------------------------------------------- */
void wasm_write_leb_u32(WasmEnc *e, uint32_t val) {
    do {
        uint8_t b = val & 0x7F;
        val >>= 7;
        if (val) b |= 0x80;
        wasm_emit(e, b);
    } while (val);
}

void wasm_write_leb_i64(WasmEnc *e, int64_t val) {
    int more = 1;
    while (more) {
        uint8_t b = val & 0x7F;
        val >>= 7;
        if ((val == 0 && (b & 0x40) == 0) || (val == -1 && (b & 0x40)))
            more = 0;
        else
            b |= 0x80;
        wasm_emit(e, b);
    }
}

/* -- Instructions -------------------------------------------------- */
void wasm_i64_const(WasmEnc *e, int64_t val) { wasm_emit(e, 0x42); wasm_write_leb_i64(e, val); }
void wasm_i64_add(WasmEnc *e) { wasm_emit(e, 0x7c); }
void wasm_i64_sub(WasmEnc *e) { wasm_emit(e, 0x7d); }
void wasm_i64_mul(WasmEnc *e) { wasm_emit(e, 0x7e); }
void wasm_i64_div_s(WasmEnc *e) { wasm_emit(e, 0x7f); }
void wasm_i64_rem_s(WasmEnc *e) { wasm_emit(e, 0x81); }
void wasm_i64_and(WasmEnc *e) { wasm_emit(e, 0x83); }
void wasm_i64_or(WasmEnc *e) { wasm_emit(e, 0x84); }
void wasm_i64_xor(WasmEnc *e) { wasm_emit(e, 0x85); }
void wasm_i64_shl(WasmEnc *e) { wasm_emit(e, 0x86); }
void wasm_i64_shr_s(WasmEnc *e) { wasm_emit(e, 0x87); }
void wasm_i64_eq(WasmEnc *e) { wasm_emit(e, 0x50); }
void wasm_i64_ne(WasmEnc *e) { wasm_emit(e, 0x51); }
void wasm_i64_lt_s(WasmEnc *e) { wasm_emit(e, 0x52); }
void wasm_i64_gt_s(WasmEnc *e) { wasm_emit(e, 0x53); }
void wasm_i64_le_s(WasmEnc *e) { wasm_emit(e, 0x54); }
void wasm_i64_ge_s(WasmEnc *e) { wasm_emit(e, 0x55); }
void wasm_local_get(WasmEnc *e, uint32_t idx) { wasm_emit(e, 0x20); wasm_write_leb_u32(e, idx); }
void wasm_local_set(WasmEnc *e, uint32_t idx) { wasm_emit(e, 0x21); wasm_write_leb_u32(e, idx); }
void wasm_i64_extend_i32_s(WasmEnc *e) { wasm_emit(e, 0xac); }
void wasm_drop(WasmEnc *e) { wasm_emit(e, 0x1a); }
void wasm_block(WasmEnc *e) { wasm_emit(e, 0x02); wasm_emit(e, 0x40); }  /* block void */
void wasm_block_i64(WasmEnc *e) { wasm_emit(e, 0x02); wasm_emit(e, 0x7e); }  /* block (result i64) */
void wasm_loop(WasmEnc *e) { wasm_emit(e, 0x03); wasm_emit(e, 0x40); }   /* loop void */
void wasm_if(WasmEnc *e) { wasm_emit(e, 0x04); wasm_emit(e, 0x40); }     /* if void */
void wasm_else(WasmEnc *e) { wasm_emit(e, 0x05); }
void wasm_end(WasmEnc *e) { wasm_emit(e, 0x0b); }
void wasm_return(WasmEnc *e) { wasm_emit(e, 0x0f); }
void wasm_br(WasmEnc *e, uint32_t label) { wasm_emit(e, 0x0c); wasm_write_leb_u32(e, label); }
void wasm_br_if(WasmEnc *e, uint32_t label) { wasm_emit(e, 0x0d); wasm_write_leb_u32(e, label); }

size_t wasm_branch_pos(const WasmEnc *e) { return e->pos; }
void wasm_patch_branch(WasmEnc *e, size_t pos, size_t target) {
    (void)e; (void)pos; (void)target;
    /* WASM uses structured control flow — not yet needed for our use case */
}
