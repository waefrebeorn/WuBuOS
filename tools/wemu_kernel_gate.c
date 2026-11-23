/* tools/wemu_kernel_gate.c — boot the real WuBuOS kernel inside WEMU and
 * assert it reaches kernel_main without a CPU fault.
 *
 * This is the permanent form of spike 003. It does NOT claim a full boot:
 * it proves the guest-execution track by loading PT_LOAD at p_paddr,
 * building higher-half page tables outside the kernel heap, redirecting
 * the unmodelled APIC MMIO windows to spare RAM, and entering
 * .long_mode_start directly (WEMU decodes 64-bit only, so the 32-bit crt0
 * is intentionally skipped).
 */
#define _POSIX_C_SOURCE 200809L
#include "wemu.h"
#include <elf.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define RAM_SIZE   (256ull << 20)
#define PT_BASE    0x5000000ull
#define PT_PDPT    (PT_BASE + 0x1000)
#define PT_PD      (PT_BASE + 0x2000)
#define LM_ENTRY   0xffffffff80100365ull
#define MAX_STEPS  1500000ull
#define MARKER     "WuBuOS: kernel_main entered (long mode OK)"

static char serial_buf[8192];
static size_t serial_len;
static int fails;

static void serial_out(uint8_t b, void *ud) {
    (void)ud;
    if (serial_len + 1 < sizeof serial_buf)
        serial_buf[serial_len++] = (char)b;
    serial_buf[serial_len] = 0;
}

static uint8_t *slurp(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz <= 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    uint8_t *b = malloc((size_t)sz);
    if (!b || fread(b, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f); free(b); return NULL;
    }
    fclose(f); *n = (size_t)sz; return b;
}

static uint64_t find_sym(const uint8_t *b, size_t n, const char *name) {
    Elf64_Ehdr eh;
    if (n < sizeof eh) return 0;
    memcpy(&eh, b, sizeof eh);
    for (int i = 0; i < eh.e_shnum; i++) {
        if (eh.e_shoff + (size_t)(i + 1) * eh.e_shentsize > n) break;
        Elf64_Shdr sh;
        memcpy(&sh, b + eh.e_shoff + (size_t)i * eh.e_shentsize, sizeof sh);
        if (sh.sh_type != SHT_SYMTAB || sh.sh_link >= eh.e_shnum) continue;
        Elf64_Shdr str;
        memcpy(&str, b + eh.e_shoff + (size_t)sh.sh_link * eh.e_shentsize,
               sizeof str);
        if (str.sh_offset + str.sh_size > n) continue;
        const char *strings = (const char *)b + str.sh_offset;
        for (size_t off = 0; off + sizeof(Elf64_Sym) <= sh.sh_size;
             off += sh.sh_entsize ? sh.sh_entsize : sizeof(Elf64_Sym)) {
            Elf64_Sym s;
            memcpy(&s, b + sh.sh_offset + off, sizeof s);
            if (s.st_name < str.sh_size && strcmp(strings + s.st_name, name) == 0)
                return s.st_value;
        }
    }
    return 0;
}

static void put64(WemuCpu *c, uint64_t addr, uint64_t v) {
    if (wemu_mem_write(c, addr, &v, 8) != 0) {
        printf("FAIL: page-table write at %llx\n", (unsigned long long)addr);
        fails++;
    }
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "src/kernel/kernel.elf";
    size_t n = 0;
    uint8_t *image = slurp(path, &n);
    if (!image) { printf("FAIL: cannot read %s\n", path); return 2; }
    Elf64_Ehdr eh;
    if (n < sizeof eh) { printf("FAIL: ELF too small\n"); return 2; }
    memcpy(&eh, image, sizeof eh);

    WemuMachine *m = wemu_machine_create(RAM_SIZE, serial_out, NULL);
    if (!m) { printf("FAIL: no WEMU machine\n"); return 2; }
    WemuCpu *c = wemu_machine_cpu(m);

    for (int i = 0; i < eh.e_phnum; i++) {
        Elf64_Phdr ph;
        memcpy(&ph, image + eh.e_phoff + (size_t)i * eh.e_phentsize, sizeof ph);
        if (ph.p_type != PT_LOAD) continue;
        if (ph.p_paddr + ph.p_memsz > RAM_SIZE ||
            wemu_mem_write(c, ph.p_paddr, image + ph.p_offset, ph.p_filesz) != 0) {
            printf("FAIL: PT_LOAD at %llx\n", (unsigned long long)ph.p_paddr);
            fails++;
        }
    }

    /* 2 MiB pages. PML4[0] identity, PML4[511] higher-half mirror. */
    for (uint64_t a = PT_BASE; a < PT_PD + 0x1000; a += 8) put64(c, a, 0);
    put64(c, PT_BASE, PT_PDPT | 3);
    put64(c, PT_BASE + 511 * 8, PT_PDPT | 3);
    for (int i = 0; i < 4; i++) put64(c, PT_PDPT + i * 8, PT_PD | 3);
    put64(c, PT_PDPT + 510 * 8, PT_PD | 3); /* higher-half VA uses PDPT[510] */
    for (int i = 0; i < 34; i++) /* kernel heap: 0..68 MiB */
        put64(c, PT_PD + i * 8, (uint64_t)i * 0x200000ull | 0x83);
    put64(c, PT_PD + 40 * 8, PT_BASE | 0x83);    /* page tables themselves */
    for (int i = 500; i <= 503; i++) /* APIC windows -> spare RAM */
        put64(c, PT_PD + i * 8,
              (0x600000ull + (uint64_t)(i - 500) * 0x200000ull) | 0x83);

    wemu_cpu_set_cr3(c, PT_BASE);
    wemu_cpu_enable_paging(c, 1);
    wemu_cpu_set_rsp(c, 0x70000);
    wemu_cpu_set_reg(c, 0, 0); /* EAX: not multiboot magic */
    wemu_cpu_set_reg(c, 6, 0); /* RSI: no Limine response */
    uint64_t entry = find_sym(image, n, ".long_mode_start");
    if (entry == 0) { printf("FAIL: .long_mode_start symbol missing\n"); fails++; }
    wemu_cpu_set_rip(c, entry);

    uint64_t steps = 0;
    while (steps < MAX_STEPS && !strstr(serial_buf, MARKER)) {
        int64_t s = wemu_cpu_run(c, 50000);
        if (s <= 0) break;
        steps += (uint64_t)s;
        if (wemu_cpu_fault(c) || wemu_cpu_halted(c)) break;
    }
    if (steps >= MAX_STEPS) { printf("FAIL: kernel_main marker timeout\n"); fails++; }
    if (wemu_cpu_fault(c)) {
        printf("FAIL: guest fault %d at %llx cr2=%llx\n", wemu_cpu_fault(c),
               (unsigned long long)wemu_cpu_rip(c),
               (unsigned long long)wemu_cpu_fault_addr(c));
        fails++;
    }
    if (!strstr(serial_buf, MARKER)) {
        printf("FAIL: kernel_main marker absent; serial: %s\n", serial_buf);
        fails++;
    }
    printf("test_wemu_kernel: %s (%llu steps, fault=%d, serial=%zu bytes)\n",
           fails ? "FAIL" : "PASS", (unsigned long long)steps,
           wemu_cpu_fault(c), serial_len);
    wemu_machine_destroy(m);
    free(image);
    return fails ? 1 : 0;
}
