/*
 * jit.h  --  My Seed JIT Runtime Public API
 *
 * Layer 1 core primitive: compile source code to native machine code
 * and execute it at runtime. This recreates the TempleOS "edit source,
 * run instantly" magic in our C-ported kernel.
 *
 * Backend priority:
 *   1. mmap + x86-64 hand-encoding (always available, no deps)
 *   2. MIR (C-to-MIR-to-native, when integrated)
 *   3. AsmJit (x86-64 machine code emitter, fallback)
 *
 * All backends conform to this single API.
 */

#ifndef MYSEED_JIT_H
#define MYSEED_JIT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* -- Types -------------------------------------------------------- */

typedef enum {
    JIT_BACKEND_MMAP   = 0,  /* mmap(PROT_EXEC) + manual encoding */
    JIT_BACKEND_MIR    = 1,  /* MIR: C source → IR → native      */
    JIT_BACKEND_ASMJIT = 2,  /* AsmJit: x86-64 code emitter      */
} JITBackend;

typedef enum {
    JIT_LANG_C      = 0,  /* Standard C source                   */
    JIT_LANG_HOLYC  = 1,  /* HolyC/ZealC source                  */
    JIT_LANG_ASM    = 2,  /* x86-64 assembly text                */
} JITLang;

typedef enum {
    JIT_OK          =  0,
    JIT_ERR_COMPILE = -1,  /* Compilation failed                 */
    JIT_ERR_ALLOC   = -2,  /* Memory allocation failed           */
    JIT_ERR_BACKEND = -3,  /* Backend not available              */
    JIT_ERR_LINK    = -4,  /* Linking/relocation failed          */
    JIT_ERR_RUNTIME = -5,  /* Runtime execution error            */
} JITResult;

/* Opaque JIT context */
typedef struct JITContext JITContext;

/* Compiled function handle */
typedef struct JITFunc {
    void           *code;      /* Executable memory pointer          */
    size_t          code_size; /* Bytes of machine code              */
    JITBackend      backend;   /* Which backend compiled this        */
    char           *name;      /* Function name (for debugging)      */
    int             n_args;    /* Number of arguments                */
} JITFunc;

/* -- Context Lifecycle ------------------------------------------- */

/* Create a new JIT context. Default backend: JIT_BACKEND_MMAP. */
JITContext *jit_init(void);

/* Create with explicit backend selection. */
JITContext *jit_init_backend(JITBackend backend);

/* Free all compiled code and context resources. */
void jit_free(JITContext *ctx);

/* -- Compilation ------------------------------------------------- */

/*
 * Compile source code string to a native function.
 *
 * @param ctx      JIT context
 * @param source   Source code string (C, HolyC, or ASM)
 * @param lang     Source language
 * @param fn_name  Name of the function to extract/compile
 * @param out_func [out] Compiled function handle
 * @return JIT_OK on success, error code on failure
 *
 * The compiled function lives in executable memory and can be called
 * until jit_func_free() is called or the context is freed.
 */
JITResult jit_compile(JITContext *ctx,
                       const char *source,
                       JITLang lang,
                       const char *fn_name,
                       JITFunc *out_func);

/*
 * Compile a file (reads from filesystem, then calls jit_compile).
 */
JITResult jit_compile_file(JITContext *ctx,
                            const char *path,
                            JITLang lang,
                            const char *fn_name,
                            JITFunc *out_func);

/* -- Execution --------------------------------------------------- */

/*
 * Call a compiled function with no arguments.
 * int64_t fn(void)
 */
int64_t jit_call0(JITFunc *fn);

/*
 * Call a compiled function with 1 integer argument.
 * int64_t fn(int64_t)
 */
int64_t jit_call1(JITFunc *fn, int64_t a0);

/*
 * Call a compiled function with 2 integer arguments.
 * int64_t fn(int64_t, int64_t)
 */
int64_t jit_call2(JITFunc *fn, int64_t a0, int64_t a1);

/*
 * Call a compiled function with variable arguments.
 * The caller is responsible for ensuring the calling convention matches.
 */
void *jit_callv(JITFunc *fn, ...);

/* -- RAW mmap Backend (lowest level) ----------------------------- */

/*
 * Allocate RWX memory buffer for writing machine code.
 * This is the primitive all other backends ultimately use.
 *
 * @param size  Minimum bytes to allocate (rounded up to page size)
 * @return Pointer to executable memory, or NULL on failure
 */
void *jit_alloc_exec(size_t size);

/*
 * Free executable memory allocated by jit_alloc_exec.
 */
void jit_free_exec(void *ptr, size_t size);

/*
 * Set memory from writable to executable (drop PROT_WRITE).
 * Optional hardening: call after writing code, before executing.
 */
void jit_lock_exec(void *ptr, size_t size);

/*
 * Set memory from executable to writable (add PROT_WRITE).
 * For re-patching already-compiled code.
 */
void jit_unlock_exec(void *ptr, size_t size);

/* -- Function Management ----------------------------------------- */

/* Free a compiled function's executable memory. */
void jit_func_free(JITFunc *fn);

/* Get human-readable error string for a JITResult code. */
const char *jit_strerror(JITResult result);

/* -- Diagnostics ------------------------------------------------- */

/* Dump compiled machine code to hex (for debugging). */
void jit_func_dump(const JITFunc *fn, FILE *out);

/* Disassemble compiled code (requires libopcodes or capstone). */
void jit_func_disasm(const JITFunc *fn, FILE *out);

/* Get context stats: total allocated, total compiled, etc. */
typedef struct {
    size_t total_alloc;     /* Total executable memory allocated    */
    size_t total_compiled;  /* Total functions compiled             */
    size_t total_freed;     /* Total functions freed                */
    size_t cache_hits;      /* JIT cache hits (if caching enabled)  */
} JITStats;

void jit_stats(const JITContext *ctx, JITStats *out);

#endif /* MYSEED_JIT_H */
