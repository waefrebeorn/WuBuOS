/*
 * wubu_isa_ptx.c -- the NVIDIA PTX ISA driver (the GPU leg).
 *
 * The eighth driver in the ISA driver space. Consumes wubu_mir_t,
 * emits PTX assembly, compiles to cubin via ptxas, and executes on
 * the GPU via a pre-compiled CUDA host stub.
 *
 * Strategy:
 *   compile(): walk the MIR program, emit PTX text, write to /tmp,
 *              call ptxas to produce cubin, read cubin into memory.
 *   run():     write cubin to /tmp, invoke the pre-compiled host stub
 *              via system(), read its exit code as the int64 result.
 *   describe(): print driver info.
 *
 * The host stub (gpu_host_stub.cu) is compiled once at startup:
 *   nvcc -arch=sm_89 -o /tmp/gpu_host_stub gpu_host_stub.cu
 * It loads a cubin, launches wubu_kernel, prints the result as a
 * decimal int64 to stdout, and the driver reads it back.
 *
 * PTX kernel ABI:
 *   .visible .entry wubu_kernel(.param .b64 result, .param .b64 arg)
 *   — result is a pointer to one int64, arg is the input value.
 *
 * C11, self-contained.
 */
#include "wubu_isa_ptx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

/* ---- PTX assembly emitter ---- */

typedef struct {
    char *text;
    size_t n, cap;
    uint32_t n_vregs;     /* highest virtual register used + 1 */
} ptx_emitter_t;

static void ptx_emit(ptx_emitter_t *e, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (len <= 0) return;

    if (e->n + (size_t)len + 1 >= e->cap) {
        e->cap = e->cap ? e->cap * 2 : 8192;
        e->text = realloc(e->text, e->cap);
    }
    memcpy(e->text + e->n, buf, (size_t)len);
    e->n += (size_t)len;
    e->text[e->n] = '\0';
}

/* Map a MIR virtual register to a PTX register name like %r42.
 * We pre-declare all registers up front, so here we just track count. */
static uint32_t ptx_vr(ptx_emitter_t *e, wubu_vr_t vr)
{
    if (vr + 1 > e->n_vregs)
        e->n_vregs = vr + 1;
    return vr;
}

/* ---- MIR -> PTX translation ---- */

static void emit_kernel_body(ptx_emitter_t *e, const wubu_mir_prog_t *p)
{
    /* We need to track label positions. PTX labels are just identifiers
     * followed by ':'. Since we emit linearly, labels are emitted at the
     * right place. For forward branches we record the label id and emit
     * it when we hit a MIR_LABEL. */

    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *ins = &p->ins[i];
        uint32_t rd;

        switch (ins->op) {
        case MIR_CONST:
            rd = ptx_vr(e, ins->dst);
            /* mov.b64 %rd, imm — PTX allows signed immediates */
            ptx_emit(e, "    mov.b64 %%r%lld, %lld;\n", (long long)rd, (long long)ins->imm);
            break;

        case MIR_MOV:
            rd = ptx_vr(e, ins->dst);
            ptx_emit(e, "    mov.b64 %%r%d, %%r%d;\n", (int)rd, (int)ptx_vr(e, ins->a));
            break;

        case MIR_ADD:
            rd = ptx_vr(e, ins->dst);
            ptx_emit(e, "    add.s64 %%r%d, %%r%d, %%r%d;\n",
                     (int)rd, (int)ptx_vr(e, ins->a), (int)ptx_vr(e, ins->b));
            break;

        case MIR_SUB:
            rd = ptx_vr(e, ins->dst);
            ptx_emit(e, "    sub.s64 %%r%d, %%r%d, %%r%d;\n",
                     (int)rd, (int)ptx_vr(e, ins->a), (int)ptx_vr(e, ins->b));
            break;

        case MIR_MUL:
            rd = ptx_vr(e, ins->dst);
            /* mul.lo.s64 gives the low 64 bits of the product */
            ptx_emit(e, "    mul.lo.s64 %%r%d, %%r%d, %%r%d;\n",
                     (int)rd, (int)ptx_vr(e, ins->a), (int)ptx_vr(e, ins->b));
            break;

        case MIR_DIV:
            rd = ptx_vr(e, ins->dst);
            ptx_emit(e, "    div.s64 %%r%d, %%r%d, %%r%d;\n",
                     (int)rd, (int)ptx_vr(e, ins->a), (int)ptx_vr(e, ins->b));
            break;

        case MIR_MOD:
            rd = ptx_vr(e, ins->dst);
            ptx_emit(e, "    rem.s64 %%r%d, %%r%d, %%r%d;\n",
                     (int)rd, (int)ptx_vr(e, ins->a), (int)ptx_vr(e, ins->b));
            break;

        case MIR_AND:
            rd = ptx_vr(e, ins->dst);
            ptx_emit(e, "    and.b64 %%r%d, %%r%d, %%r%d;\n",
                     (int)rd, (int)ptx_vr(e, ins->a), (int)ptx_vr(e, ins->b));
            break;

        case MIR_OR:
            rd = ptx_vr(e, ins->dst);
            ptx_emit(e, "    or.b64 %%r%d, %%r%d, %%r%d;\n",
                     (int)rd, (int)ptx_vr(e, ins->a), (int)ptx_vr(e, ins->b));
            break;

        case MIR_XOR:
            rd = ptx_vr(e, ins->dst);
            ptx_emit(e, "    xor.b64 %%r%d, %%r%d, %%r%d;\n",
                     (int)rd, (int)ptx_vr(e, ins->a), (int)ptx_vr(e, ins->b));
            break;

        case MIR_SHL:
            rd = ptx_vr(e, ins->dst);
            ptx_emit(e, "    shl.b64 %%r%d, %%r%d, %%r%d;\n",
                     (int)rd, (int)ptx_vr(e, ins->a), (int)ptx_vr(e, ins->b));
            break;

        case MIR_SHR:
            rd = ptx_vr(e, ins->dst);
            ptx_emit(e, "    shr.u64 %%r%d, %%r%d, %%r%d;\n",
                     (int)rd, (int)ptx_vr(e, ins->a), (int)ptx_vr(e, ins->b));
            break;

        case MIR_NEG:
            rd = ptx_vr(e, ins->dst);
            ptx_emit(e, "    neg.s64 %%r%d, %%r%d;\n",
                     (int)rd, (int)ptx_vr(e, ins->a));
            break;

        case MIR_NOT:
            rd = ptx_vr(e, ins->dst);
            ptx_emit(e, "    not.b64 %%r%d, %%r%d;\n",
                     (int)rd, (int)ptx_vr(e, ins->a));
            break;

        /* Comparisons: setp.eq/ne/lt/le/gt/ge — produces a predicate */
        case MIR_EQ:
        case MIR_NE:
        case MIR_LT:
        case MIR_LE:
        case MIR_GT:
        case MIR_GE: {
            rd = ptx_vr(e, ins->dst);
            const char *cmp;
            switch (ins->op) {
                case MIR_EQ: cmp = "eq"; break;
                case MIR_NE: cmp = "ne"; break;
                case MIR_LT: cmp = "lt"; break;
                case MIR_LE: cmp = "le"; break;
                case MIR_GT: cmp = "gt"; break;
                case MIR_GE: cmp = "ge"; break;
                default: cmp = "eq"; break;
            }
            /* PTX setp produces a predicate; we convert to int via sel */
            ptx_emit(e, "    setp.%s.s64 p%d, %%r%d, %%r%d;\n",
                     cmp, (int)rd, (int)ptx_vr(e, ins->a), (int)ptx_vr(e, ins->b));
            ptx_emit(e, "    selp.b64 %%r%d, 1, 0, p%d;\n",
                     (int)rd, (int)rd);
            break;
        }

        case MIR_LABEL:
            /* Label id becomes a PTX label */
            ptx_emit(e, "L_%u:\n", ins->label);
            break;

        case MIR_JMP:
            ptx_emit(e, "    bra L_%u;\n", ins->label);
            break;

        case MIR_JZ: {
            /* if (vr == 0) jump — use a predicate */
            uint32_t predicate = e->n_vregs++;  /* borrow a reg id for predicate naming */
            ptx_emit(e, "    setp.eq.s64 p%u, %%r%d, 0;\n",
                     predicate, (int)ptx_vr(e, ins->a));
            ptx_emit(e, "    @p%u bra L_%u;\n", predicate, ins->label);
            break;
        }

        case MIR_RET: {
            /* Store vr 0 to the result parameter and return */
            ptx_emit(e, "    ld.param.b64 %%rd_result, [result];\n");
            ptx_emit(e, "    st.global.s64 [%%rd_result], %%r0;\n");
            ptx_emit(e, "    ret;\n");
            break;
        }

        default:
            /* Unknown op — emit a comment so it's visible */
            ptx_emit(e, "    /* MIR op %d — not yet implemented */\n", (int)ins->op);
            break;
        }
    }
}

/* ---- Write the full PTX file from a MIR program ---- */

/* First pass: count how many virtual registers and predicates we need */
static void count_regs(const wubu_mir_prog_t *p, uint32_t *out_vregs, uint32_t *out_preds)
{
    uint32_t max_vr = 0;
    uint32_t max_pred = 0;

    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *ins = &p->ins[i];
        uint32_t local_max = 0;

        switch (ins->op) {
        case MIR_CONST:
        case MIR_MOV:
        case MIR_ADD: case MIR_SUB: case MIR_MUL: case MIR_DIV: case MIR_MOD:
        case MIR_AND: case MIR_OR: case MIR_XOR:
        case MIR_SHL: case MIR_SHR:
        case MIR_NEG: case MIR_NOT:
            local_max = ins->dst + 1;
            break;
        case MIR_EQ: case MIR_NE: case MIR_LT: case MIR_LE: case MIR_GT: case MIR_GE:
            /* dst is reused as predicate number */
            local_max = ins->dst + 1;
            if (ins->dst + 1 > max_pred) max_pred = ins->dst + 1;
            break;
        case MIR_JZ:
            /* borrows n_vregs as predicate id */
            local_max = ins->a + 1;
            if (local_max > max_vr) max_vr = local_max;
            /* the predicate will be e->n_vregs at emit time, so we
             * conservatively add 1 to max_vr for the predicate reg */
            break;
        default:
            break;
        }
        if (local_max > max_vr) max_vr = local_max;
    }

    /* JZ may borrow n_vregs as predicate, so ensure headroom */
    *out_vregs = max_vr + 2;  /* +1 for 0-based, +1 for JZ predicate headroom */
    *out_preds = max_pred + 4; /* generous predicate pool */
}

static char *emit_ptx(const wubu_mir_prog_t *p)
{
    ptx_emitter_t e;
    memset(&e, 0, sizeof(e));

    /* Count registers first so the header declaration is correct */
    uint32_t n_vregs, n_preds;
    count_regs(p, &n_vregs, &n_preds);

    /* PTX header */
    ptx_emit(&e, ".version 8.0\n");
    ptx_emit(&e, ".target sm_89\n");
    ptx_emit(&e, ".address_size 64\n\n");

    /* Kernel signature */
    ptx_emit(&e, ".visible .entry wubu_kernel(\n");
    ptx_emit(&e, "    .param .b64 result,\n");
    ptx_emit(&e, "    .param .b64 arg\n");
    ptx_emit(&e, ") {\n");

    /* Declare registers: %r0..%r{n_vregs-1} plus rd_result */
    ptx_emit(&e, "    .reg .b64 %%r<%u>;\n", n_vregs + 1);
    ptx_emit(&e, "    .reg .b64 %%rd_result;\n");
    ptx_emit(&e, "    .reg .pred p<%u>;\n\n", n_preds);

    /* Load the arg parameter into vr 0 (the "value" register) */
    ptx_emit(&e, "    ld.param.b64 %%r0, [arg];\n\n");

    /* Emit the kernel body */
    e.n_vregs = n_vregs;  /* pre-seed so JZ predicate allocation starts above */
    emit_kernel_body(&e, p);

    ptx_emit(&e, "}\n");
    return e.text;
}

/* ---- Host stub management ---- */

static int stub_compiled = 0;

static int ensure_stub(void)
{
    if (stub_compiled) return 0;

    /* Check if the stub binary already exists */
    if (access("/tmp/gpu_host_stub", X_OK) == 0) {
        stub_compiled = 1;
        return 0;
    }

    /* Compile the stub. The source lives alongside this .cu file. */
    const char *stub_src = "/home/wubu/wubuos/src/compiler/gpu_host_stub.cu";
    int rc = system("nvcc -arch=sm_89 -O2 -o /tmp/gpu_host_stub "
                    " -Xptxas -v "  /* show ptxas info */
                    " /home/wubu/wubuos/src/compiler/gpu_host_stub.cu "
                    " -L/usr/lib/x86_64-linux-gnu -lcuda "
                    " 2>/tmp/gpu_stub_build.log");
    if (rc != 0) {
        fprintf(stderr, "[ptx] nvcc host stub compilation failed (rc=%d)\n", rc);
        fprintf(stderr, "[ptx] build log:\n");
        FILE *f = fopen("/tmp/gpu_stub_build.log", "r");
        if (f) {
            int c;
            while ((c = fgetc(f)) != EOF) fputc(c, stderr);
            fclose(f);
        }
        return -1;
    }
    stub_compiled = 1;
    return 0;
}

/* ---- Driver API: compile ---- */

static int ptx_compile(const wubu_mir_prog_t *p, uint8_t **out_code, size_t *out_size)
{
    if (!p || !out_code || !out_size) return -1;

    /* 1. Emit PTX assembly */
    char *ptx = emit_ptx(p);
    if (!ptx) return -1;

    /* 2. Write PTX to temp file */
    const char *ptx_path = "/tmp/wubu_kernel.ptx";
    FILE *f = fopen(ptx_path, "w");
    if (!f) {
        free(ptx);
        return -1;
    }
    fputs(ptx, f);
    fclose(f);
    free(ptx);

    /* 3. Compile PTX -> cubin with ptxas */
    const char *cubin_path = "/tmp/wubu_kernel.cubin";
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "ptxas -arch=sm_89 -O2 %s -o %s 2>/tmp/ptxas_build.log",
             ptx_path, cubin_path);
    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "[ptx] ptxas compilation failed (rc=%d)\n", rc);
        FILE *log = fopen("/tmp/ptxas_build.log", "r");
        if (log) {
            int c;
            while ((c = fgetc(log)) != EOF) fputc(c, stderr);
            fclose(log);
        }
        return -1;
    }

    /* 4. Read cubin into memory */
    FILE *cf = fopen(cubin_path, "rb");
    if (!cf) return -1;
    fseek(cf, 0, SEEK_END);
    long cubin_size = ftell(cf);
    fseek(cf, 0, SEEK_SET);
    uint8_t *cubin = malloc((size_t)cubin_size);
    if (!cubin) { fclose(cf); return -1; }
    fread(cubin, 1, (size_t)cubin_size, cf);
    fclose(cf);

    *out_code = cubin;
    *out_size = (size_t)cubin_size;
    return 0;
}

/* ---- Driver API: run ---- */

static int64_t ptx_run(const uint8_t *code, size_t size, int64_t arg)
{
    if (!code || size == 0) return 0;

    /* Ensure the host stub is compiled */
    if (ensure_stub() != 0) {
        fprintf(stderr, "[ptx] cannot run — host stub not available\n");
        return 0;
    }

    /* Write cubin to /tmp */
    const char *cubin_path = "/tmp/wubu_kernel.cubin";
    FILE *f = fopen(cubin_path, "wb");
    if (!f) return 0;
    fwrite(code, 1, size, f);
    fclose(f);

    /* Launch the host stub: it prints the result to stdout */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "/tmp/gpu_host_stub %s %lld 2>/tmp/gpu_run.log",
             cubin_path, (long long)arg);

    /* Capture stdout via a temp file — the stub writes to stdout,
     * so we redirect to a file and read it back. */
    char result_path[256];
    snprintf(result_path, sizeof(result_path), "/tmp/gu_result_%d.txt", (int)getpid());
    strncat(cmd, " > ", sizeof(cmd) - strlen(cmd) - 1);
    strncat(cmd, result_path, sizeof(cmd) - strlen(cmd) - 1);

    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "[ptx] host stub execution failed (rc=%d)\n", rc);
        FILE *log = fopen("/tmp/gpu_run.log", "r");
        if (log) {
            int c;
            while ((c = fgetc(log)) != EOF) fputc(c, stderr);
            fclose(log);
        }
        remove(result_path);
        return 0;
    }

    /* Read the result */
    int64_t result = 0;
    FILE *rf = fopen(result_path, "r");
    if (rf) {
        if (fscanf(rf, "%lld", (long long *)&result) != 1)
            result = 0;
        fclose(rf);
    }
    remove(result_path);

    return result;
}

/* ---- Driver API: describe ---- */

static void ptx_describe(void)
{
    printf("PTX (NVIDIA GPU) ISA driver\n");
    printf("  Family:        gpu\n");
    printf("  Target:        sm_89 (Ada Lovelace / RTX 40-series)\n");
    printf("  PTX version:   8.0\n");
    printf("  Exec model:    native (CUDA driver API via host stub)\n");
    printf("  Compile:       MIR -> PTX -> cubin (ptxas)\n");
    printf("  Run:           cubin -> GPU launch -> result\n");
    printf("  MIR ops:       ADD SUB MUL DIV MOD AND OR XOR SHL SHR\n");
    printf("                 NEG NOT EQ NE LT LE GT GE MOV JMP JZ RET\n");
    printf("  Registers:     unlimited virtual -> PTX .reg .b64\n");
}

/* ---- The driver object ---- */

const wubu_isa_driver_t wubu_isa_ptx = {
    .name     = "ptx",
    .family   = "gpu",
    .exec     = WUBU_ISA_NATIVE,  /* GPU runs natively on this machine */
    .compile  = ptx_compile,
    .run      = ptx_run,
    .describe = ptx_describe,
};