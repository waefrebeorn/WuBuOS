/*
 * test_album_benchmark.c — Cleveland Browns album data processing benchmark.
 *
 * Simulates processing a music album collection: computing statistics,
 * filtering, sorting, and aggregating track metadata. This exercises
 * the compiler's arithmetic, control flow, and function call capabilities
 * across all three backends (x86-64, ARM64, RISC-V).
 *
 * The "Cleveland Browns album" dataset: 12 tracks with metadata.
 * We compute: total duration, average rating, longest track,
 * tracks above a rating threshold, and a weighted score.
 */
#include "jit_codegen.h"
#include "jit.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int jit_minic_compile_cg(CodeGen *cg, const char *src);
const uint8_t *jit_minic_get_code(CodeGen *cg, size_t *size);
extern CodeGen *cg_create_rv64(void);

/* Track data (simulated album metadata) */
#define NUM_TRACKS 12
static const int track_durations[NUM_TRACKS] = {
    240, 185, 312, 198, 267, 223, 289, 176, 254, 301, 198, 233
};
static const int track_ratings[NUM_TRACKS] = {
    8, 7, 9, 6, 8, 7, 9, 5, 8, 9, 6, 7
};
static const int track_plays[NUM_TRACKS] = {
    1200, 850, 2100, 620, 1500, 980, 1800, 430, 1350, 2400, 560, 1100
};

static int64_t exec_x86(const uint8_t *code, size_t sz, int64_t arg0, int64_t arg1) {
    void *mem = mmap(NULL, sz + 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return -999;
    memcpy(mem, code, sz);
    int64_t (*f)(int64_t, int64_t) = (int64_t(*)(int64_t, int64_t))mem;
    int64_t result = f(arg0, arg1);
    munmap(mem, sz + 4096);
    return result;
}

static int64_t exec_via_qemu(const char *arch, const uint8_t *code, size_t sz,
                              int64_t arg0, int64_t arg1) {
    char bin_path[256], elf_path[256], qemu_path[256];
    snprintf(bin_path, sizeof(bin_path), "/tmp/album_%s.bin", arch);
    snprintf(elf_path, sizeof(elf_path), "/tmp/album_%s.elf", arch);
    snprintf(qemu_path, sizeof(qemu_path), "/tmp/qemu-%s-static", arch);

    FILE *f = fopen(bin_path, "wb");
    if (!f) return -998;
    fwrite(code, 1, sz, f);
    fclose(f);

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "python3 /tmp/build_elf.py %s %s %s %lld %lld && chmod 755 %s 2>&1",
             arch, bin_path, elf_path, (long long)arg0, (long long)arg1, elf_path);
    if (system(cmd) != 0) return -997;

    snprintf(cmd, sizeof(cmd), "%s %s 2>&1", qemu_path, elf_path);
    FILE *p = popen(cmd, "r");
    if (!p) return -996;
    char buf[1024];
    fgets(buf, sizeof(buf), p);
    int rc = pclose(p);
    if (WIFEXITED(rc)) return WEXITSTATUS(rc);
    return -995;
}

/* Reference implementations in C */
static int ref_total_duration(void) {
    int total = 0;
    for (int i = 0; i < NUM_TRACKS; i++) total += track_durations[i];
    return total;
}
static int ref_longest_track(void) {
    int longest = 0;
    for (int i = 0; i < NUM_TRACKS; i++)
        if (track_durations[i] > longest) longest = track_durations[i];
    return longest;
}
static int ref_tracks_above_rating(int threshold) {
    int count = 0;
    for (int i = 0; i < NUM_TRACKS; i++)
        if (track_ratings[i] >= threshold) count++;
    return count;
}
static int ref_weighted_score(void) {
    int64_t score = 0;
    for (int i = 0; i < NUM_TRACKS; i++)
        score += track_ratings[i] * track_plays[i];
    return (int)(score / 100000);
}
static int ref_total_duration_formula(void) {
    /* Arithmetic series: sum = n * (first + last) / 2 if sorted,
     * but we compute it the hard way to exercise the compiler */
    return 240 + 185 + 312 + 198 + 267 + 223 + 289 + 176 + 254 + 301 + 198 + 233;
}

int main(void) {
    int pass = 0, fail = 0, total = 0;

    struct {
        const char *src;
        int64_t arg0, arg1;
        int64_t expect;
        const char *desc;
    } tests[] = {
        /* --- Album processing functions (values mod 256 for exit code) --- */
        {"return 240 + 185 + 312 + 198 + 267 + 223 + 289 + 176 + 254 + 301 + 198 + 233;",
         0, 0, 2876 % 256, "total_duration_constfold"},
        {"long total = 0; total = total + 240; total = total + 185; total = total + 312; total = total + 198; total = total + 267; total = total + 223; total = total + 289; total = total + 176; total = total + 254; total = total + 301; total = total + 198; total = total + 233; return total;",
         0, 0, 2876 % 256, "total_duration_accum"},
        {"long longest = 0; long d = 240; if(d > longest) longest = d; d = 185; if(d > longest) longest = d; d = 312; if(d > longest) longest = d; d = 198; if(d > longest) longest = d; d = 267; if(d > longest) longest = d; d = 223; if(d > longest) longest = d; d = 289; if(d > longest) longest = d; d = 176; if(d > longest) longest = d; d = 254; if(d > longest) longest = d; d = 301; if(d > longest) longest = d; d = 198; if(d > longest) longest = d; d = 233; if(d > longest) longest = d; return longest;",
         0, 0, 312 % 256, "longest_track"},
        {"long count = 0; if(8 >= 7) count = count + 1; if(7 >= 7) count = count + 1; if(9 >= 7) count = count + 1; if(6 >= 7) count = count + 1; if(8 >= 7) count = count + 1; if(7 >= 7) count = count + 1; if(9 >= 7) count = count + 1; if(5 >= 7) count = count + 1; if(8 >= 7) count = count + 1; if(9 >= 7) count = count + 1; if(6 >= 7) count = count + 1; if(7 >= 7) count = count + 1; return count;",
         0, 0, 9, "tracks_above_rating"},
        /* Weighted score: sum(rating * plays) / 100000 */
        {"long score = 0; score = score + 8 * 1200; score = score + 7 * 850; score = score + 9 * 2100; score = score + 6 * 620; score = score + 8 * 1500; score = score + 7 * 980; score = score + 9 * 1800; score = score + 5 * 430; score = score + 8 * 1350; score = score + 9 * 2400; score = score + 6 * 560; score = score + 7 * 1100; return score / 100000;",
         0, 0, 1, "weighted_score"},
        /* --- Arithmetic stress tests --- */
        {"return (100 + 200) * 3 / 4 - 50;", 0, 0, (100+200)*3/4-50, "arithmetic_complex"},
        {"long x = 1; x = x * 2 * 2 * 2 * 2; return x;", 0, 0, 16, "power_of_2_mul"},
        {"long x = 1024; x = x / 2 / 2 / 2 / 2; return x;", 0, 0, 64, "power_of_2_div"},
        {"long x = 255; x = x % 16; return x;", 0, 0, 15, "power_of_2_mod"},
        {"return 0x100 | 0x010 | 0x001;", 0, 0, 0x111, "bitwise_combine"},
        {"long s = 0; long i = 1; while(i <= 10) { s = s + i; i = i + 1; } return s;",
         0, 0, 55, "while_sum_1_to_10"},
        {NULL, 0, 0, 0, NULL}
    };

    const char *arch_names[] = {"x86-64", "ARM64", "RISC-V"};
    int arch_pass[3] = {0}, arch_fail[0] = {0}, arch_total = 0;

    for (int t = 0; tests[t].src; t++) {
        arch_total++;
        CodeGen *cg_x86 = cg_create_x86();
        CodeGen *cg_arm = cg_create_arm64();
        CodeGen *cg_rv  = cg_create_rv64();

        jit_minic_compile_cg(cg_x86, tests[t].src);
        jit_minic_compile_cg(cg_arm, tests[t].src);
        jit_minic_compile_cg(cg_rv,  tests[t].src);

        size_t sz_x86, sz_arm, sz_rv;
        const uint8_t *code_x86 = jit_minic_get_code(cg_x86, &sz_x86);
        const uint8_t *code_arm = jit_minic_get_code(cg_arm, &sz_arm);
        const uint8_t *code_rv  = jit_minic_get_code(cg_rv,  &sz_rv);

        int64_t res_x86 = exec_x86(code_x86, sz_x86, tests[t].arg0, tests[t].arg1);
        int64_t res_arm = exec_via_qemu("aarch64", code_arm, sz_arm, tests[t].arg0, tests[t].arg1);
        int64_t res_rv  = exec_via_qemu("riscv64", code_rv, sz_rv, tests[t].arg0, tests[t].arg1);

        int ok = (res_x86 == tests[t].expect && res_arm == tests[t].expect && res_rv == tests[t].expect);
        if (ok) { pass++; arch_pass[0]++; arch_pass[1]++; arch_pass[2]++; }
        else fail++;

        printf("  %s %-28s x86=%-4ld arm=%-4ld rv=%-4ld expect=%-4ld  [%zu/%zu/%zu B]\n",
               ok ? "PASS" : "FAIL", tests[t].desc,
               (long)res_x86, (long)res_arm, (long)res_rv, (long)tests[t].expect,
               sz_x86, sz_arm, sz_rv);

        cg_destroy(cg_x86);
        cg_destroy(cg_arm);
        cg_destroy(cg_rv);
    }

    printf("\n=== CLEVELAND BROWNS ALBUM BENCHMARK ===\n");
    printf("  x86-64:  %d/%d passed\n", arch_pass[0], arch_total);
    printf("  ARM64:   %d/%d passed\n", arch_pass[1], arch_total);
    printf("  RISC-V:  %d/%d passed\n", arch_pass[2], arch_total);
    printf("  Total:   %d/%d passed, %d failed\n", pass, arch_total, fail);

    /* Print optimization impact */
    printf("\n=== OPTIMIZATION IMPACT ===\n");
    {
        CodeGen *cg = cg_create_x86();
        /* Without constant folding, this would emit 12 add instructions.
         * With folding, it should be a single mov. */
        jit_minic_compile_cg(cg, "return 240 + 185 + 312 + 198 + 267 + 223 + 289 + 176 + 254 + 301 + 198 + 233;");
        size_t sz;
        jit_minic_get_code(cg, &sz);
        printf("  Constant fold (12 adds):     %zu bytes (optimal: 23)\n", sz);
        cg_destroy(cg);
    }
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_cg(cg, "return a * 8;");
        size_t sz;
        jit_minic_get_code(cg, &sz);
        printf("  Strength reduce (a*8→a<<3):  %zu bytes\n", sz);
        cg_destroy(cg);
    }
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_cg(cg, "return a / 4;");
        size_t sz;
        jit_minic_get_code(cg, &sz);
        printf("  Strength reduce (a/4→a>>2):  %zu bytes\n", sz);
        cg_destroy(cg);
    }
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_cg(cg, "return a % 8;");
        size_t sz;
        jit_minic_get_code(cg, &sz);
        printf("  Strength reduce (a%%8→a&7):   %zu bytes\n", sz);
        cg_destroy(cg);
    }
    {
        CodeGen *cg = cg_create_x86();
        jit_minic_compile_cg(cg, "long s = 0; long i = 1; while(i <= 10) { s = s + i; i = i + 1; } return s;");
        size_t sz;
        jit_minic_get_code(cg, &sz);
        printf("  While loop (sum 1..10):      %zu bytes\n", sz);
        cg_destroy(cg);
    }

    return fail ? 1 : 0;
}
