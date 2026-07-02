/*
 * vsl_elf.c  --  VSL ELF Loading Implementation
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "vsl/vsl_internal.h"
#include "vsl/vsl_elf.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>

/* Global state */
extern VSL_STATE g_vsl;

/* -- ELF Loading -------------------------------------------------- */

int vsl_elf_validate(const void *elf_data, size_t elf_size,
                     uint64_t *out_entry) {
    if (!elf_data || elf_size < 64) return -1;

    const uint8_t *p = (const uint8_t *)elf_data;

    /* ELF magic */
    if (p[0] != 0x7F || p[1] != 'E' || p[2] != 'L' || p[3] != 'F')
        return -1;

    /* 64-bit ELF */
    if (p[4] != 2) return -1;

    /* Little-endian */
    if (p[5] != 1) return -1;

    /* ELF version */
    if (p[6] != 1) return -1;

    /* e_type: ET_EXEC (2) or ET_DYN (3) */
    uint16_t e_type = (uint16_t)p[16] | ((uint16_t)p[17] << 8);
    if (e_type != 2 && e_type != 3) return -1;

    /* e_machine: x86_64 (0x3E) */
    uint16_t e_machine = (uint16_t)p[18] | ((uint16_t)p[19] << 8);
    if (e_machine != 0x3E) return -1;

    /* Entry point */
    if (out_entry) {
        memcpy(out_entry, p + 24, 8);
    }

    return 0;
}

uint64_t vsl_elf_load(const void *elf_data, size_t elf_size) {
    uint64_t entry;
    if (vsl_elf_validate(elf_data, elf_size, &entry) != 0) return 0;

    const uint8_t *p = (const uint8_t *)elf_data;

    /* Parse ELF64 program headers to load PT_LOAD segments */
    uint16_t e_phnum = (uint16_t)p[56] | ((uint16_t)p[57] << 8);
    uint16_t e_phentsize = (uint16_t)p[54] | ((uint16_t)p[55] << 8);
    uint64_t e_phoff;
    memcpy(&e_phoff, p + 32, 8);

    if (e_phoff + (uint64_t)e_phnum * e_phentsize > elf_size) return entry;

    const uint8_t *phdr = p + e_phoff;

    for (int i = 0; i < e_phnum; i++) {
        const uint8_t *cur = phdr + i * e_phentsize;

        uint32_t p_type;
        memcpy(&p_type, cur, 4);

        /* PT_LOAD = 1: Loadable segment */
        if (p_type == 1) {
            uint64_t p_offset, p_vaddr, p_filesz, p_memsz;
            memcpy(&p_offset, cur + 8, 8);
            memcpy(&p_vaddr, cur + 16, 8);
            memcpy(&p_filesz, cur + 32, 8);
            memcpy(&p_memsz, cur + 40, 8);

            if (p_memsz == 0) continue;

            /* Validate segment is within ELF data */
            if (p_offset + p_filesz > elf_size) continue;

            /* Calculate destination address in VSL user space */
            uint64_t dest_addr = VSL_USER_BASE + (p_vaddr & 0x00FFFFFF);

            /* Ensure allocated memory via mmap if needed */
            uint64_t page_start = dest_addr & ~0xFFFULL;
            uint64_t page_end = (dest_addr + p_memsz + 0xFFF) & ~0xFFFULL;
            size_t map_size = (size_t)(page_end - page_start);

            /* Check if this region is already tracked */
            int existing = -1;
            for (int j = 0; j < (int)g_vsl.n_mmaps; j++) {
                if (g_vsl.mmaps[j].addr <= dest_addr &&
                    g_vsl.mmaps[j].addr + g_vsl.mmaps[j].size >= dest_addr + p_memsz) {
                    existing = j;
                    break;
                }
            }

            if (existing < 0 && g_vsl.n_mmaps < VSL_MAX_MMAPS) {
                int idx = g_vsl.n_mmaps++;
                g_vsl.mmaps[idx].addr = page_start;
                g_vsl.mmaps[idx].size = map_size;
                g_vsl.mmaps[idx].prot = 7; /* RWX */
                g_vsl.mmaps[idx].flags = 0x22; /* MAP_PRIVATE | MAP_ANONYMOUS */
                g_vsl.mmaps[idx].fd = -1;
                g_vsl.mmaps[idx].offset = 0;
            }

            /* Copy segment data from ELF into VSL address space */
            if (p_filesz > 0) {
                memcpy((void *)dest_addr, p + p_offset, p_filesz);
            }

            /* Zero-fill remainder (BSS) */
            if (p_memsz > p_filesz) {
                memset((void *)(dest_addr + p_filesz), 0, (size_t)(p_memsz - p_filesz));
            }
        }
        /* PT_INTERP = 3: Dynamic linker path */
        else if (p_type == 3) {
            uint64_t p_offset, p_filesz;
            memcpy(&p_offset, cur + 8, 8);
            memcpy(&p_filesz, cur + 32, 8);
            if (p_offset + p_filesz <= elf_size && p_filesz > 0) {
                fprintf(stderr, "[vsl] ELF interpreter: %.*s\n", (int)p_filesz, (const char *)(p + p_offset));
            }
        }
        /* PT_DYNAMIC = 2: Dynamic linking information */
        else if (p_type == 2) {
            uint64_t p_offset, p_vaddr, p_filesz, p_memsz;
            memcpy(&p_offset, cur + 8, 8);
            memcpy(&p_vaddr, cur + 16, 8);
            memcpy(&p_filesz, cur + 32, 8);
            memcpy(&p_memsz, cur + 40, 8);
            fprintf(stderr, "[vsl] ELF dynamic section at 0x%llx (size=%llu)\n",
                    (unsigned long long)p_vaddr, (unsigned long long)p_memsz);
        }
        /* PT_TLS = 7: Thread-local storage */
        else if (p_type == 7) {
            uint64_t p_offset, p_vaddr, p_filesz, p_memsz, p_align;
            memcpy(&p_offset, cur + 8, 8);
            memcpy(&p_vaddr, cur + 16, 8);
            memcpy(&p_filesz, cur + 32, 8);
            memcpy(&p_memsz, cur + 40, 8);
            memcpy(&p_align, cur + 48, 8);
            fprintf(stderr, "[vsl] ELF TLS segment: vaddr=0x%llx filesz=%llu memsz=%llu align=%llu\n",
                    (unsigned long long)p_vaddr, (unsigned long long)p_filesz,
                    (unsigned long long)p_memsz, (unsigned long long)p_align);
        }
    }

    return entry;
}

int vsl_elf_interpreter(const void *elf_data, size_t elf_size,
                        char *buf, size_t buf_size) {
    if (!elf_data || elf_size < 64) return -1;
    if (buf && buf_size > 0) buf[0] = '\0';

    const uint8_t *p = (const uint8_t *)elf_data;
    uint16_t e_phnum = (uint16_t)p[56] | ((uint16_t)p[57] << 8);
    uint16_t e_phentsize = (uint16_t)p[54] | ((uint16_t)p[55] << 8);
    uint64_t e_phoff;
    memcpy(&e_phoff, p + 32, 8);

    if (e_phoff + (uint64_t)e_phnum * e_phentsize > elf_size) return -1;

    const uint8_t *phdr = p + e_phoff;

    for (int i = 0; i < e_phnum; i++) {
        const uint8_t *cur = phdr + i * e_phentsize;
        uint32_t p_type;
        memcpy(&p_type, cur, 4);
        if (p_type == 3) { /* PT_INTERP */
            uint64_t p_offset, p_filesz;
            memcpy(&p_offset, cur + 8, 8);
            memcpy(&p_filesz, cur + 32, 8);
            if (p_offset + p_filesz <= elf_size && p_filesz > 0) {
                size_t copy_len = p_filesz < buf_size ? p_filesz : buf_size - 1;
                memcpy(buf, p + p_offset, copy_len);
                buf[copy_len] = '\0';
                return 0;
            }
        }
    }
    return -1; /* Statically linked (no interpreter) */
}