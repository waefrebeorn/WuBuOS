/*
 * wubu_pe.h -- the IN-KERNEL PE loader.
 */
#ifndef WUBU_PE_H
#define WUBU_PE_H

#include <stdint.h>
#include <stddef.h>

/* the parsed PE info */
typedef struct {
    uint16_t machine;
    uint16_t num_sections;
    uint16_t optional_size;
    uint16_t subsystem;      /* 2=GUI 3=CUI */
    uint32_t entry_rva;
    uint32_t image_base;
    uint32_t import_rva;
    uint32_t import_size;
    uint32_t pe_offset;
} wubu_pe_info_t;

/* one mapped section */
typedef struct {
    char     name[9];
    uint32_t virtual_size;
    uint32_t virtual_addr;
    uint32_t raw_size;
    uint32_t raw_ptr;
    uint32_t characteristics;
} wubu_pe_section_t;

/* one import descriptor (a DLL the game needs) */
typedef struct {
    char dll[64];
    int  func_count;
} wubu_pe_import_t;

/* PE1: validate + parse the headers. 0 on success. */
int wubu_pe_parse(const void *data, size_t size, wubu_pe_info_t *out);

/* PE2: walk the section table. Returns the count. */
int wubu_pe_sections(const void *data, size_t size,
                     const wubu_pe_info_t *info,
                     wubu_pe_section_t *out, int max);

/* PE3: resolve an RVA to the raw file offset (the mapping plan). */
int wubu_pe_rva_to_raw(const wubu_pe_section_t *sects, int n,
                       uint32_t rva);

/* PE4: walk the import descriptors. Returns the count. */
int wubu_pe_imports(const void *data, size_t size,
                    const wubu_pe_info_t *info,
                    const wubu_pe_section_t *sects, int nsects,
                    wubu_pe_import_t *out, int max);

#endif
