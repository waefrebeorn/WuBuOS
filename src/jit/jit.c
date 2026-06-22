/*
 * jit.c  --  My Seed JIT Runtime Implementation (mmap backend)
 *
 * The always-available, zero-dependency JIT backend.
 * Uses mmap(PROT_READ|PROT_WRITE|PROT_EXEC) for executable memory
 * and hand-encoded x86-64 machine code for primitives.
 *
 * Backends:
 *   0 (MMAP): mmap + x86-64 hand-encoding (always available, simple expressions)
 *   1 (MIR):  Self-hosted C→x86-64 via gcc -shared + dlopen (real C compilation)
 *   2 (ASMJIT): Assembly text → wubu_x86 encoder → executable (x86-64 asm JIT)
 *
 * External deps replaced: capstone → wubu_disasm, asmjit → wubu_x86,
 * MIR/c2m → gcc -shared + dlopen (host compiler, not external lib).
 */

#include "jit.h"
#include "wubu_x86.h"
#include "wubu_disasm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dlfcn.h>
#include <limits.h>

/* -- Internal: Page size ------------------------------------------ */

static long g_page_size = 0;

static void init_page_size(void) {
    if (g_page_size == 0)
        g_page_size = sysconf(_SC_PAGESIZE);
}

static size_t align_to_page(size_t size) {
    init_page_size();
    return (size + g_page_size - 1) & ~(g_page_size - 1);
}

/* -- JIT Context ------------------------------------------------- */

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

/* -- Executable Memory ------------------------------------------- */

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

/* -- x86-64 Encoding Helpers ------------------------------------- */

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

/* -- mmap Backend: Simple Expression Compiler -------------------- */

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

/* -- MIR Backend: Self-Hosted C → GCC → dlopen ------------------ */

/* Forward declaration — asmjit backend defined below */
static JITResult jit_asmjit_compile_impl(JITContext *ctx,
                                          const char *source,
                                          JITLang lang,
                                          const char *fn_name,
                                          JITFunc *out_func);

/*
 * Strategy: use self-hosted minic compiler (jit_minic_compile) for C source.
 * Falls back to `gcc -O2 -shared -fPIC` + dlopen for HolyC or complex C
 * that minic can't handle yet.
 */

static JITResult jit_mir_compile_impl(JITContext *ctx,
                                       const char *source,
                                       JITLang lang,
                                       const char *fn_name,
                                       JITFunc *out_func) {
    
    if (!source || !fn_name) return JIT_ERR_COMPILE;
    
    /* Self-hosted path: use minic for C source (expressions and mini-C functions) */
    if (lang == JIT_LANG_C || lang == JIT_LANG_EXPR) {
        JITResult rc = jit_minic_compile(ctx, source, lang, fn_name, out_func);
        if (rc == JIT_OK) return rc;
        /* minic couldn't handle it — fall through to gcc+dlopen */
    }
    
    /* Assembly text → can't compile as C; route to asmjit backend */
    if (lang == JIT_LANG_ASM) {
        return jit_asmjit_compile_impl(ctx, source, lang, fn_name, out_func);
    }
    
    /* Generate unique temp filenames using PID + counter */
    static int mir_seq = 0;
    char tmp_c[256], tmp_so[256];
    snprintf(tmp_c, sizeof(tmp_c), "/tmp/wubu_jit_%d_%d.c", (int)getpid(), mir_seq);
    snprintf(tmp_so, sizeof(tmp_so), "/tmp/wubu_jit_%d_%d.so", (int)getpid(), mir_seq);
    mir_seq++;
    
    /* Write C source */
    FILE *f = fopen(tmp_c, "w");
    if (!f) return JIT_ERR_ALLOC;
    
    if (lang == JIT_LANG_HOLYC) {
        /* Wrap HolyC as C-compatible source */
        fprintf(f, "/* WuBuOS HolyC wrapper */\n");
        fprintf(f, "typedef long I64;\ntypedef unsigned char U8;\n");
        fprintf(f, "typedef unsigned long U64;\n");
        fprintf(f, "#include <stdint.h>\n");
        fprintf(f, "__attribute__((visibility(\"default\")))\n");
        fprintf(f, "long %s(long a, long b) {\n", fn_name);
        fprintf(f, "  %s\n", source);
        fprintf(f, "}\n");
    } else if (lang == JIT_LANG_ASM) {
        /* Assembly text → can't compile as C; route to asmjit backend */
        fclose(f);
        unlink(tmp_c);
        return jit_asmjit_compile_impl(ctx, source, lang, fn_name, out_func);
    } else {
        /* Standard C: wrap the source in a function if it looks like an expression */
        fprintf(f, "#include <stdint.h>\n");
        fprintf(f, "__attribute__((visibility(\"default\")))\n");
        fprintf(f, "long %s(long a, long b) {\n", fn_name);
        fprintf(f, "  return (long)(%s);\n", source);
        fprintf(f, "}\n");
    }
    fclose(f);
    
    /* Compile via gcc -shared -fPIC -O2 */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "gcc -O2 -shared -fPIC -o %s %s 2>/dev/null",
             tmp_so, tmp_c);
    int gcc_rc = system(cmd);
    unlink(tmp_c);
    
    if (gcc_rc != 0) return JIT_ERR_COMPILE;
    
    /* dlopen the .so — RTLD_LOCAL prevents symbol collisions between compiles */
    void *handle = dlopen(tmp_so, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        unlink(tmp_so);
        return JIT_ERR_LINK;
    }
    
    /* dlsym the function */
    void *sym = dlsym(handle, fn_name);
    if (!sym) {
        dlclose(handle);
        unlink(tmp_so);
        return JIT_ERR_LINK;
    }
    
    /* We can't free the .so while the symbol is in use, so we keep
     * a reference. The code pointer is the function entry itself. */
    out_func->code = sym;
    out_func->code_size = 0;  /* unknown size for dynamically loaded code */
    out_func->backend = JIT_BACKEND_MIR;
    out_func->name = strdup(fn_name);
    out_func->n_args = 2;  /* default: (a, b) */
    
    /* Store the dl handle in a way we can clean up later.
     * For now: the .so stays loaded. jit_func_free will dlclose it. */
    /* We repurpose code_size field: 0 means "externally managed, don't munmap" */
    
    /* Clean up .so file (it's loaded in memory now) */
    unlink(tmp_so);
    
    ctx->stats.total_compiled++;
    return JIT_OK;
}

/* -- ASMJIT Backend: Assembly Text → wubu_x86 Encoder ------------ */

/*
 * Parses simple x86-64 assembly text and emits machine code
 * using our wubu_x86 encoder. Supports:
 *   mov reg, imm64 / mov reg, reg / add, sub, imul, xor, cmp,
 *   neg, ret, push, pop, shl, shr, sar, call reg, cqo, idiv
 *   jmp label / je label / jne label / jl / jg / jle / jge
 *   label:  (definition, with backpatching for forward refs)
 */

/* Simple label table for backpatching */
#define JIT_MAX_LABELS 64
#define JIT_MAX_LABEL_NAME 32

typedef struct {
    char name[JIT_MAX_LABEL_NAME];
    size_t pos;       /* Position in code buffer where label is defined */
    bool defined;
} JITLabel;

typedef struct {
    char name[JIT_MAX_LABEL_NAME];
    size_t patch_pos;  /* Position of rel32 to patch */
    Wx86CC cc;         /* Condition code for Jcc; WCC_O for JMP */
} JITLabelRef;

typedef struct {
    JITLabel     labels[JIT_MAX_LABELS];
    int          n_labels;
    JITLabelRef  refs[JIT_MAX_LABELS * 4];  /* up to 4 refs per label */
    int          n_refs;
} JITLabelTable;

static void ltab_init(JITLabelTable *lt) {
    memset(lt, 0, sizeof(*lt));
}

static void ltab_define(JITLabelTable *lt, const char *name, size_t pos) {
    if (lt->n_labels >= JIT_MAX_LABELS) return;
    JITLabel *l = &lt->labels[lt->n_labels++];
    snprintf(l->name, JIT_MAX_LABEL_NAME, "%s", name);
    l->pos = pos;
    l->defined = true;
}

static void ltab_add_ref(JITLabelTable *lt, const char *name,
                          size_t patch_pos, Wx86CC cc) {
    if (lt->n_refs >= (int)(JIT_MAX_LABELS * 4)) return;
    JITLabelRef *r = &lt->refs[lt->n_refs++];
    snprintf(r->name, JIT_MAX_LABEL_NAME, "%s", name);
    r->patch_pos = patch_pos;
    r->cc = cc;
}

static void ltab_resolve(JITLabelTable *lt, Wx86Enc *e) {
    /* First pass: resolve refs to already-defined labels */
    for (int i = 0; i < lt->n_refs; i++) {
        JITLabelRef *r = &lt->refs[i];
        for (int j = 0; j < lt->n_labels; j++) {
            if (strcmp(r->name, lt->labels[j].name) == 0) {
                wx86_patch_rel32(e, r->patch_pos, lt->labels[j].pos);
                break;
            }
        }
    }
}

/* Parse a register name to Wx86Reg */
static Wx86Reg parse_reg(const char *s) {
    const char *names[] = {
        "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
        "r8","r9","r10","r11","r12","r13","r14","r15", NULL
    };
    for (int i = 0; names[i]; i++) {
        if (strcmp(s, names[i]) == 0) return (Wx86Reg)i;
    }
    return WREG_NONE;
}

/* Parse condition code from jcc mnemonic */
static Wx86CC parse_jcc(const char *mnemonic) {
    if (strcmp(mnemonic, "je")  == 0 || strcmp(mnemonic, "jz")  == 0) return WCC_E;
    if (strcmp(mnemonic, "jne") == 0 || strcmp(mnemonic, "jnz") == 0) return WCC_NE;
    if (strcmp(mnemonic, "jl")  == 0) return WCC_L;
    if (strcmp(mnemonic, "jg")  == 0) return WCC_G;
    if (strcmp(mnemonic, "jle") == 0) return WCC_LE;
    if (strcmp(mnemonic, "jge") == 0) return WCC_GE;
    if (strcmp(mnemonic, "jb")  == 0 || strcmp(mnemonic, "jnae") == 0) return WCC_B;
    if (strcmp(mnemonic, "jae") == 0 || strcmp(mnemonic, "jnb")  == 0) return WCC_AE;
    if (strcmp(mnemonic, "ja")  == 0 || strcmp(mnemonic, "jnbe") == 0) return WCC_A;
    if (strcmp(mnemonic, "jbe") == 0 || strcmp(mnemonic, "jna")  == 0) return WCC_BE;
    if (strcmp(mnemonic, "js")  == 0) return WCC_S;
    if (strcmp(mnemonic, "jns") == 0) return WCC_NS;
    return (Wx86CC)-1;
}

/* Skip whitespace and return pointer to next non-space char */
static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static JITResult jit_asmjit_compile_impl(JITContext *ctx,
                                          const char *source,
                                          JITLang lang,
                                          const char *fn_name,
                                          JITFunc *out_func) {
    (void)lang;
    if (!source) return JIT_ERR_COMPILE;
    
    Wx86Enc enc;
    wx86_enc_init_dynamic(&enc, 4096);
    
    JITLabelTable ltab;
    ltab_init(&ltab);
    
    /* Parse assembly line by line */
    const char *p = source;
    char line[256];
    
    while (*p) {
        /* Read one line */
        int i = 0;
        while (*p && *p != '\n' && i < (int)sizeof(line) - 1)
            line[i++] = *p++;
        line[i] = '\0';
        if (*p == '\n') p++;
        
        /* Trim trailing whitespace/newline */
        char *end = line + strlen(line) - 1;
        while (end > line && (*end == ' ' || *end == '\t' || *end == '\r'))
            *end-- = '\0';
        
        /* Skip empty lines and comments */
        const char *lp = skip_ws(line);
        if (*lp == '\0' || *lp == '#') continue;
        
        /* Check for label definition (ends with ':') */
        size_t llen = strlen(lp);
        if (llen > 1 && lp[llen - 1] == ':') {
            char label_name[JIT_MAX_LABEL_NAME];
            size_t lname_len = llen - 1;
            if (lname_len >= JIT_MAX_LABEL_NAME) lname_len = JIT_MAX_LABEL_NAME - 1;
            memcpy(label_name, lp, lname_len);
            label_name[lname_len] = '\0';
            ltab_define(&ltab, label_name, enc.pos);
            continue;
        }
        
        /* Parse mnemonic */
        char mnemonic[16] = {0};
        char op1[64] = {0};
        char op2[64] = {0};
        int nargs = sscanf(lp, "%15s %63[^,] , %63s", mnemonic, op1, op2);
        if (nargs < 1) continue;
        
        /* Decode and emit */
        if (strcmp(mnemonic, "ret") == 0) {
            wx86_ret(&enc);
        } else if (strcmp(mnemonic, "cqo") == 0) {
            wx86_cqo(&enc);
        } else if (strcmp(mnemonic, "neg") == 0 && nargs >= 2) {
            Wx86Reg r = parse_reg(skip_ws(op1));
            if (r != WREG_NONE) wx86_neg_reg(&enc, r);
        } else if (strcmp(mnemonic, "idiv") == 0 && nargs >= 2) {
            Wx86Reg r = parse_reg(skip_ws(op1));
            if (r != WREG_NONE) wx86_idiv_reg(&enc, r);
        } else if (strcmp(mnemonic, "push") == 0 && nargs >= 2) {
            Wx86Reg r = parse_reg(skip_ws(op1));
            if (r != WREG_NONE) wx86_push_reg(&enc, r);
        } else if (strcmp(mnemonic, "pop") == 0 && nargs >= 2) {
            Wx86Reg r = parse_reg(skip_ws(op1));
            if (r != WREG_NONE) wx86_pop_reg(&enc, r);
        } else if (strcmp(mnemonic, "mov") == 0 && nargs >= 3) {
            Wx86Reg dst = parse_reg(skip_ws(op1));
            Wx86Reg src = parse_reg(skip_ws(op2));
            if (dst != WREG_NONE && src != WREG_NONE) {
                wx86_mov_reg_reg(&enc, dst, src);
            } else if (dst != WREG_NONE && src == WREG_NONE) {
                /* mov reg, imm */
                char *endp = NULL;
                long imm = strtol(skip_ws(op2), &endp, 0);
                if (endp && *endp == '\0') {
                    if (imm >= INT32_MIN && imm <= INT32_MAX) {
                        wx86_mov_reg_imm32(&enc, dst, (int32_t)imm);
                    } else {
                        wx86_mov_reg_imm64(&enc, dst, imm);
                    }
                }
            }
        } else if (strcmp(mnemonic, "add") == 0 && nargs >= 3) {
            Wx86Reg dst = parse_reg(skip_ws(op1));
            Wx86Reg src = parse_reg(skip_ws(op2));
            if (dst != WREG_NONE && src != WREG_NONE) {
                wx86_add_reg_reg(&enc, dst, src);
            } else if (dst != WREG_NONE) {
                long imm = strtol(skip_ws(op2), NULL, 0);
                wx86_add_reg_imm32(&enc, dst, (int32_t)imm);
            }
        } else if (strcmp(mnemonic, "sub") == 0 && nargs >= 3) {
            Wx86Reg dst = parse_reg(skip_ws(op1));
            Wx86Reg src = parse_reg(skip_ws(op2));
            if (dst != WREG_NONE && src != WREG_NONE) {
                wx86_sub_reg_reg(&enc, dst, src);
            } else if (dst != WREG_NONE) {
                long imm = strtol(skip_ws(op2), NULL, 0);
                wx86_sub_reg_imm32(&enc, dst, (int32_t)imm);
            }
        } else if (strcmp(mnemonic, "imul") == 0 && nargs >= 3) {
            Wx86Reg dst = parse_reg(skip_ws(op1));
            Wx86Reg src = parse_reg(skip_ws(op2));
            if (dst != WREG_NONE && src != WREG_NONE)
                wx86_imul_reg_reg(&enc, dst, src);
        } else if (strcmp(mnemonic, "xor") == 0 && nargs >= 3) {
            Wx86Reg dst = parse_reg(skip_ws(op1));
            Wx86Reg src = parse_reg(skip_ws(op2));
            if (dst != WREG_NONE && src != WREG_NONE)
                wx86_xor_reg_reg(&enc, dst, src);
        } else if (strcmp(mnemonic, "cmp") == 0 && nargs >= 3) {
            Wx86Reg a = parse_reg(skip_ws(op1));
            Wx86Reg b = parse_reg(skip_ws(op2));
            if (a != WREG_NONE && b != WREG_NONE)
                wx86_cmp_reg_reg(&enc, a, b);
            else if (a != WREG_NONE) {
                long imm = strtol(skip_ws(op2), NULL, 0);
                wx86_cmp_reg_imm32(&enc, a, (int32_t)imm);
            }
        } else if (strcmp(mnemonic, "test") == 0 && nargs >= 3) {
            Wx86Reg a = parse_reg(skip_ws(op1));
            Wx86Reg b = parse_reg(skip_ws(op2));
            if (a != WREG_NONE && b != WREG_NONE)
                wx86_test_reg_reg(&enc, a, b);
        } else if (strcmp(mnemonic, "shl") == 0 && nargs >= 3) {
            Wx86Reg dst = parse_reg(skip_ws(op1));
            long imm = strtol(skip_ws(op2), NULL, 0);
            if (dst != WREG_NONE) wx86_shl_reg_imm8(&enc, dst, (uint8_t)imm);
        } else if (strcmp(mnemonic, "shr") == 0 && nargs >= 3) {
            Wx86Reg dst = parse_reg(skip_ws(op1));
            long imm = strtol(skip_ws(op2), NULL, 0);
            if (dst != WREG_NONE) wx86_shr_reg_imm8(&enc, dst, (uint8_t)imm);
        } else if (strcmp(mnemonic, "sar") == 0 && nargs >= 3) {
            Wx86Reg dst = parse_reg(skip_ws(op1));
            long imm = strtol(skip_ws(op2), NULL, 0);
            if (dst != WREG_NONE) wx86_sar_reg_imm8(&enc, dst, (uint8_t)imm);
        } else if (strcmp(mnemonic, "call") == 0 && nargs >= 2) {
            Wx86Reg r = parse_reg(skip_ws(op1));
            if (r != WREG_NONE) {
                wx86_call_reg(&enc, r);
            } else {
                /* call label — emit rel32 placeholder, add ref */
                wx86_call_rel32(&enc);
                ltab_add_ref(&ltab, skip_ws(op1), enc.pos - 4, (Wx86CC)-2);
            }
        } else if (strcmp(mnemonic, "jmp") == 0 && nargs >= 2) {
            wx86_jmp_rel32(&enc);
            ltab_add_ref(&ltab, skip_ws(op1), enc.pos - 4, (Wx86CC)-1);
        } else if (strncmp(mnemonic, "j", 1) == 0 && nargs >= 2) {
            /* Conditional jump */
            Wx86CC cc = parse_jcc(mnemonic);
            if ((int)cc >= 0) {
                wx86_jcc_rel32(&enc, cc);
                ltab_add_ref(&ltab, skip_ws(op1), enc.pos - 4, cc);
            }
        }
    }
    
    /* Resolve all label references */
    ltab_resolve(&ltab, &enc);
    
    /* Allocate executable memory and copy code */
    size_t alloc_size = align_to_page(enc.pos);
    void *exec_mem = jit_alloc_exec(alloc_size ? alloc_size : (size_t)g_page_size);
    if (!exec_mem) {
        wx86_enc_free(&enc);
        return JIT_ERR_ALLOC;
    }
    
    memcpy(exec_mem, enc.buf, enc.pos);
    wx86_enc_free(&enc);
    
    out_func->code = exec_mem;
    out_func->code_size = enc.pos;
    out_func->backend = JIT_BACKEND_ASMJIT;
    out_func->name = strdup(fn_name ? fn_name : "asm");
    out_func->n_args = 2;  /* default */
    
    ctx->stats.total_alloc += alloc_size;
    ctx->stats.total_compiled++;
    
    return JIT_OK;
}

/* -- Main Compile Dispatch --------------------------------------- */

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
            return jit_mir_compile_impl(ctx, source, lang, fn_name, out_func);
            
        case JIT_BACKEND_ASMJIT:
            return jit_asmjit_compile_impl(ctx, source, lang, fn_name, out_func);
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
    
    size_t nread = fread(source, 1, size, f);
    (void)nread;
    source[size] = '\0';
    fclose(f);
    
    JITResult r = jit_compile(ctx, source, lang, fn_name, out_func);
    free(source);
    return r;
}

/* -- Execution --------------------------------------------------- */

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
    /* Variadic call  --  the caller must ensure correct ABI */
    if (fn && fn->code) {
        void *code = fn->code;
        /* This is inherently unsafe and platform-specific.
         * For a proper implementation, use libffi. */
        (void)code;
        return NULL;
    }
    return NULL;
}

/* -- Function Management ----------------------------------------- */

void jit_func_free(JITFunc *fn) {
    if (fn) {
        if (fn->code) {
            /* MIR backend (code_size == 0): code is from dlopen, don't munmap.
             * The .so stays loaded in the process (minor leak, but safe).
             * MMAP/ASMJIT backend (code_size > 0): mmap'd, use jit_free_exec. */
            if (fn->code_size > 0)
                jit_free_exec(fn->code, fn->code_size);
            /* For MIR: dlopen'd code stays until process exit.
             * A production version would track the dlopen handle and dlclose it. */
        }
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

/* -- Diagnostics ------------------------------------------------- */

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
    if (!fn || !out) return;
    if (!fn->code || fn->code_size == 0) {
        fprintf(out, "(no code)\n");
        return;
    }
    fprintf(out, "JITFunc '%s' (%zu bytes, backend=%d):\n",
            fn->name ? fn->name : "?", fn->code_size, fn->backend);
    /* Use our self-hosted disassembler (replaces capstone requirement) */
    const uint8_t *code = (const uint8_t *)fn->code;
    wdisasm_dump(code, fn->code_size, out);
}

void jit_stats(const JITContext *ctx, JITStats *out) {
    if (ctx && out) *out = ctx->stats;
}

void jit_stats_add_alloc(JITContext *ctx, size_t bytes) {
    if (ctx) ctx->stats.total_alloc += bytes;
}

void jit_stats_inc_compiled(JITContext *ctx) {
    if (ctx) ctx->stats.total_compiled++;
}
