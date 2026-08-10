/*
 * wubu_elf.h -- the IN-KERNEL ELF loader interface.
 */
#ifndef WUBU_ELF_H
#define WUBU_ELF_H

#include <stdint.h>
#include <stddef.h>

#define WUBU_ELF_MAX_SEG 8
#define WUBU_ELF_MAX_NEED 16

typedef struct {
    uint64_t vaddr;
    uint64_t memsz;
    uint32_t flags;
    int present;
} wubu_elf_seg_t;

typedef struct {
    uint64_t entry;
    uint64_t phoff;
    uint16_t phnum;
    uint16_t phentsize;
    uint64_t dyn_vaddr;      /* the .dynamic section (the NEEDED walk) */
    wubu_elf_seg_t ph_load[WUBU_ELF_MAX_SEG];
    int n_load;
} wubu_elf_info_t;

/* EL1: validate + parse the 64-bit ELF header. 0 on success. */
int wubu_elf_parse(const void *data, size_t size, wubu_elf_info_t *out);

/* EL2: walk the .dynamic section -> the NEEDED libs (the import
 * contract the kernel answers, not the host ld.so). */
int wubu_elf_needed(const void *data, size_t size,
                    const wubu_elf_info_t *info,
                    char out[][64], int max);

/* EL3: name the ELF machine. */
const char *wubu_elf_machine_name(uint16_t machine);

#endif
