/* tools/x86_decode_parity.c -- two x86 decoders, one corpus, compared.
 *
 * WEMU decodes x86-64 to EXECUTE it (src/wemu/src/wemu_decode.c, ~240 lines).
 * WuBuNOS decodes x86-64 to CHECK what it emitted (src/compiler/x86_decode.c,
 * ~130 lines, deliberately not a general disassembler). Two models of the same
 * bytes, in one tree, that had never been compared.
 *
 * This walks the code WuBuNOS actually emits (the MIR comparison battery, dumped
 * by test_isa_driver with WUBU_EXEC_DUMP=x86-64) through BOTH decoders and
 * requires them to agree on every instruction length. Where the small walker
 * says "not handled", that is reported separately -- it is a coverage fact, not
 * a disagreement.
 *
 * Agreement is not proof of correctness for either side; DISAGREEMENT is proof
 * that at least one of them is wrong about a byte sequence the compiler emits.
 */
#define _POSIX_C_SOURCE 200809L
#include "wemu.h"
/* wemu_decode()/WemuInsn live in the emulator's internal header: this tool is
 * deliberately poking at the decoder, which is the concern under test. */
#include "wemu_internal.h"
#include "x86_decode.h"
#include <dirent.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define RAM_SIZE  (1u << 20)
#define CODE_AT   0x1000ull

static int compare_blob(const char *name, const uint8_t *code, size_t n,
                        int *agree, int *differ, int *unsupported)
{
    WemuCpu *c = wemu_cpu_create(RAM_SIZE);
    if (!c) return -1;
    if (wemu_cpu_load(c, CODE_AT, code, n) != 0) { wemu_cpu_destroy(c); return -1; }

    size_t off = 0;
    int local_differ = 0;
    while (off < n) {
        x86_insn_t mine;
        int got = x86_decode_one(code, n, off, &mine);
        if (got <= 0) { (*unsupported)++; break; }   /* walker's documented limit */

        WemuInsn theirs;
        wemu_cpu_set_rip(c, CODE_AT + off);
        if (wemu_decode(c, &theirs) != 0) { (*unsupported)++; break; }

        if ((size_t)theirs.len != mine.len) {
            printf("  DIFFER %-24s off=%zu (%02x %02x %02x %02x %02x %02x): "
                   "wemu=%u wubunos=%zu\n",
                   name, off, code[off], off + 1 < n ? code[off + 1] : 0,
                   off + 2 < n ? code[off + 2] : 0, off + 3 < n ? code[off + 3] : 0,
                   off + 4 < n ? code[off + 4] : 0, off + 5 < n ? code[off + 5] : 0,
                   theirs.len, mine.len);
            local_differ++;
            if (local_differ >= 4) break;   /* enough evidence per blob */
        } else {
            (*agree)++;
        }
        off += mine.len;
        if (mine.len == 0) break;
    }
    wemu_cpu_destroy(c);
    *differ += local_differ;
    return local_differ;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/isaexec/x86-64";
    DIR *d = opendir(dir);
    if (!d) {
        printf("x86_decode_parity: SKIP (no %s -- run the dumper first)\n", dir);
        return 0;
    }
    printf("== x86 decode parity: WEMU vs WuBuNOS (%s) ==\n", dir);

    int blobs = 0, agree = 0, differ = 0, unsupported = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t l = strlen(e->d_name);
        if (l < 4 || strcmp(e->d_name + l - 4, ".bin") != 0) continue;
        char path[512];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        uint8_t code[4096];
        size_t n = fread(code, 1, sizeof code, f);
        fclose(f);
        blobs++;
        compare_blob(e->d_name, code, n, &agree, &differ, &unsupported);
    }
    closedir(d);

    printf("== %d blob(s): %d instruction(s) agreed, %d disagreed, "
           "%d beyond the small walker ==\n", blobs, agree, differ, unsupported);
    if (blobs == 0) {
        printf("x86_decode_parity: SKIP (nothing dumped)\n");
        return 0;
    }
    return differ == 0 ? 0 : 1;
}
