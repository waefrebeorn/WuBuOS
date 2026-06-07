/*
 * jit.c — My Seed JIT Runtime Implementation (mmap backend)
 *
 * The always-available, zero-dependency JIT backend.
 * Uses mmap(PROT_READ|PROT_WRITE|PROT_EXEC) for executable memory
 * and hand-encoded x86-64 machine code for primitives.
 *
 * MIR and AsmJit backends will be added as separate files
 * (jit_mir.c, jit_asmjit.c) and selected at context init time.
 */

#include "jit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <unistd.h>

/* ── Internal: Page size ────────────────────────────────────────── */

static long g_page_size = 0;

static void init_page_size(void) {
    if (g_page_size == 0)
        g_page_size = sysconf(_SC_PAGESIZE);
}

static size_t align_to_page(size_t size) {
    init_page_size();
    return (size + g_page_size - 1) & ~(g_page_size - 1);
}

/* ── JIT Context ───────────────────────────────────────────────── */

struct JITContext {
    JITBackend  backend;
    JITStats    stats;
    /* Future: JITFunc cache (hashmap of source hash → compiled fn) */
};

JITContext *jit_init(void) {
    return jit_init_backend(JIT_BACKEND_MMAP);
}

JITContext *jit_init_backend(JITBackend backend) {
    JITContext *ctx = calloc(1, sizeof(JITContext));
    if (!ctx) return NULL;
    ctx->backend = backend;
    return ctx;
}

void jit_free(JITContext *ctx) {
    if (ctx) free(ctx);
}

/* ── Executable Memory ─────────────────────────────────────────── */

void *jit_alloc_exec(size_t size) {
    init_page_size();
    size_t alloc_size = align_to_page(size);
    void *mem = mmap(NULL, alloc_size,
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return NULL;
    return mem;
}

void jit_free_exec(void *ptr, size_t size) {
    if (ptr) {
        init_page_size();
        munmap(ptr, align_to_page(size));
    }
}

void jit_lock_exec(void *ptr, size_t size) {
    init_page_size();
    mprotect(ptr, align_to_page(size), PROT_READ | PROT_EXEC);
}

void jit_unlock_exec(void *ptr, size_t size) {
    init_page_size();
    mprotect(ptr, align_to_page(size), PROT_READ | PROT_WRITE | PROT_EXEC);
}

/* ── x86-64 Encoding Helpers ───────────────────────────────────── */

/* Encode: mov eax, imm32 */
static int enc_mov_eax_imm32(unsigned char *buf, int32_t imm) {
    buf[0] = 0xB8;
    memcpy(buf + 1, &imm, 4);
    return 5;
}

/* Encode: mov rdi, imm64 (using movabs) */
static int enc_mov_rdi_imm64(unsigned char *buf, int64_t imm) {
    buf[0] = 0x48; buf[1] = 0xBF;
    memcpy(buf + 2, &imm, 8);
    return 10;
}

/* Encode: add eax, edi → eax = eax + edi */
static int enc_add_eax_edi(unsigned char *buf) {
    buf[0] = 0x01; buf[1] = 0xF8;
    return 2;
}

/* Encode: imul eax, edi → eax = eax * edi */
static int enc_imul_eax_edi(unsigned char *buf) {
    buf[0] = 0x0F; buf[1] = 0xAF; buf[2] = 0xC7;
    return 3;
}

/* Encode: sub eax, esi → eax = eax - esi (for a-b) */
static int enc_sub_eax_esi(unsigned char *buf) {
    buf[0] = 0x29; buf[1] = 0xF0;
    return 2;
}

/* Encode: xor eax, eax → eax = 0 */
static int enc_xor_eax_eax(unsigned char *buf) {
    buf[0] = 0x31; buf[1] = 0xC0;
    return 2;
}

/* Encode: ret */
static int enc_ret(unsigned char *buf) {
    buf[0] = 0xC3;
    return 1;
}

/* Encode: mov eax, edi (return first arg) */
static int enc_mov_eax_edi(unsigned char *buf) {
    buf[0] = 0x89; buf[1] = 0xF8;
    return 2;
}

/* Encode: add eax, esi → eax += esi (for 2-arg funcs) */
static int enc_add_eax_esi(unsigned char *buf) {
    buf[0] = 0x01; buf[1] = 0xF0;
    return 2;
}

/* Encode: mov eax, esi (return second arg) */
static int enc_mov_eax_esi(unsigned char *buf) {
    buf[0] = 0x89; buf[1] = 0xF0;
    return 2;
}

/* Encode: neg rax → rax = -rax (64-bit) */
static int enc_neg_eax(unsigned char *buf) {
    buf[0] = 0x48; buf[1] = 0xF7; buf[2] = 0xD8;
    return 3;
}

/* ── mmap Backend: Simple Expression Compiler ──────────────────── */

/*
 * Very simple single-expression compiler for the mmap backend.
 * Supports: a+b, a*b, a-b, -a, a, const
 * 
 * For real compilation, use the MIR backend.
 * This exists as the zero-dependency fallback.
 */
static JITResult mmap_compile_simple(JITContext *ctx,
                                      const char *source,
                                      const char *fn_name,
                                      JITFunc *out_func) {
    (void)fn_name;
    
    unsigned char code[256];
    int pos = 0;
    
    /* Trim whitespace */
    while (*source == ' ' || *source == '\t') source++;
    
    /* Pattern: "a+b" */
    if (strcmp(source, "a+b") == 0) {
        pos += enc_mov_eax_edi(code + pos);   /* eax = a */
        pos += enc_add_eax_esi(code + pos);   /* eax += b */
        pos += enc_ret(code + pos);
    }
    /* Pattern: "a*b" */
    else if (strcmp(source, "a*b") == 0) {
        pos += enc_mov_eax_edi(code + pos);   /* eax = a */
        pos += enc_imul_eax_edi(code + pos);  /* eax *= a (should be esi, fix) */
        /* Fix: imul eax, esi */
        pos -= 3; /* undo the imul */
        code[pos] = 0x0F; code[pos+1] = 0xAF; code[pos+2] = 0xC6; /* imul eax, esi */
        pos += 3;
        pos += enc_ret(code + pos);
    }
    /* Pattern: "a-b" */
    else if (strcmp(source, "a-b") == 0) {
        pos += enc_mov_eax_edi(code + pos);   /* eax = a */
        pos += enc_sub_eax_esi(code + pos);   /* eax -= b */
        pos += enc_ret(code + pos);
    }
    /* Pattern: "-a" */
    else if (strcmp(source, "-a") == 0) {
        pos += enc_mov_eax_edi(code + pos);   /* eax = a */
        pos += enc_neg_eax(code + pos);        /* eax = -eax */
        pos += enc_ret(code + pos);
    }
    /* Pattern: "a" (identity) */
    else if (strcmp(source, "a") == 0) {
        pos += enc_mov_eax_edi(code + pos);   /* eax = a */
        pos += enc_ret(code + pos);
    }
    /* Pattern: integer constant */
    else {
        char *end;
        long val = strtol(source, &end, 10);
        if (*end != '\0' && end != source) {
            return JIT_ERR_COMPILE;
        }
        pos += enc_mov_eax_imm32(code + pos, (int32_t)val);
        pos += enc_ret(code + pos);
    }
    
    /* Allocate executable memory and copy code */
    size_t alloc_size = align_to_page(pos);
    void *exec_mem = jit_alloc_exec(alloc_size);
    if (!exec_mem) return JIT_ERR_ALLOC;
    
    memcpy(exec_mem, code, pos);
    
    out_func->code = exec_mem;
    out_func->code_size = pos;
    out_func->backend = JIT_BACKEND_MMAP;
    out_func->name = strdup(source);
    out_func->n_args = (strchr(source, 'b') != NULL) ? 2 : 
                       (strchr(source, 'a') != NULL) ? 1 : 0;
    
    ctx->stats.total_alloc += alloc_size;
    ctx->stats.total_compiled++;
    
    return JIT_OK;
}

/* ── Main Compile Dispatch ─────────────────────────────────────── */

JITResult jit_compile(JITContext *ctx,
                       const char *source,
                       JITLang lang,
                       const char *fn_name,
                       JITFunc *out_func) {
    if (!ctx || !source || !out_func)
        return JIT_ERR_COMPILE;
    
    memset(out_func, 0, sizeof(JITFunc));
    
    switch (ctx->backend) {
        case JIT_BACKEND_MMAP:
            if (lang != JIT_LANG_ASM && lang != JIT_LANG_C)
                return JIT_ERR_BACKEND;  /* mmap only handles simple expressions/ASM */
            return mmap_compile_simple(ctx, source, fn_name, out_func);
            
        case JIT_BACKEND_MIR:
            /* TODO: jit_mir_compile() */
            return JIT_ERR_BACKEND;
            
        case JIT_BACKEND_ASMJIT:
            /* TODO: jit_asmjit_compile() */
            return JIT_ERR_BACKEND;
    }
    
    return JIT_ERR_BACKEND;
}

JITResult jit_compile_file(JITContext *ctx,
                            const char *path,
                            JITLang lang,
                            const char *fn_name,
                            JITFunc *out_func) {
    FILE *f = fopen(path, "r");
    if (!f) return JIT_ERR_COMPILE;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *source = malloc(size + 1);
    if (!source) { fclose(f); return JIT_ERR_ALLOC; }
    
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    JITResult r = jit_compile(ctx, source, lang, fn_name, out_func);
    free(source);
    return r;
}

/* ── Execution ─────────────────────────────────────────────────── */

int64_t jit_call0(JITFunc *fn) {
    if (fn && fn->code) {
        return ((int64_t (*)(void))fn->code)();
    }
    return 0;
}

int64_t jit_call1(JITFunc *fn, int64_t a0) {
    if (fn && fn->code) {
        return ((int64_t (*)(int64_t))fn->code)(a0);
    }
    return 0;
}

int64_t jit_call2(JITFunc *fn, int64_t a0, int64_t a1) {
    if (fn && fn->code) {
        return ((int64_t (*)(int64_t, int64_t))fn->code)(a0, a1);
    }
    return 0;
}

void *jit_callv(JITFunc *fn, ...) {
    /* Variadic call — the caller must ensure correct ABI */
    if (fn && fn->code) {
        void *code = fn->code;
        /* This is inherently unsafe and platform-specific.
         * For a proper implementation, use libffi. */
        (void)code;
        return NULL;
    }
    return NULL;
}

/* ── Function Management ───────────────────────────────────────── */

void jit_func_free(JITFunc *fn) {
    if (fn) {
        if (fn->code) jit_free_exec(fn->code, fn->code_size);
        if (fn->name) free(fn->name);
        memset(fn, 0, sizeof(JITFunc));
    }
}

const char *jit_strerror(JITResult result) {
    switch (result) {
        case JIT_OK:          return "Success";
        case JIT_ERR_COMPILE: return "Compilation failed";
        case JIT_ERR_ALLOC:   return "Memory allocation failed";
        case JIT_ERR_BACKEND: return "Backend not available";
        case JIT_ERR_LINK:    return "Linking/relocation failed";
        case JIT_ERR_RUNTIME: return "Runtime execution error";
    }
    return "Unknown error";
}

/* ── Diagnostics ───────────────────────────────────────────────── */

void jit_func_dump(const JITFunc *fn, FILE *out) {
    if (!fn || !fn->code || !out) return;
    fprintf(out, "JITFunc '%s' (%zu bytes, backend=%d):\n",
            fn->name ? fn->name : "?", fn->code_size, fn->backend);
    for (size_t i = 0; i < fn->code_size; i++) {
        fprintf(out, "%02X ", ((unsigned char *)fn->code)[i]);
        if ((i + 1) % 16 == 0) fprintf(out, "\n");
    }
    if (fn->code_size % 16 != 0) fprintf(out, "\n");
}

void jit_func_disasm(const JITFunc *fn, FILE *out) {
    /* Requires capstone or libopcodes — TODO */
    if (!fn || !out) return;
    fprintf(out, "Disassembly not yet implemented (need capstone)\n");
    jit_func_dump(fn, out);
}

void jit_stats(const JITContext *ctx, JITStats *out) {
    if (ctx && out) *out = ctx->stats;
}
