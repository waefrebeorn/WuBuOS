/*
 * wubu_cab.h -- the IN-KERNEL CAB (MS-CAB / "LZX cabinet") reader.
 *
 * Locates and walks Microsoft cabinet files: the 36-byte CFHEADER
 * + CFFOLDER table + CFDATA block list + CFFILE entries. Supports
 * stored (0), MSZIP (1) and LZX (7) compression.
 *
 * Halo PC's Inno installer embeds CAB1.CAB in its .rsrc section;
 * the crashed session farmed this to host 7z. This reader is the
 * kernel's own. No host tools, no libz -- only the kernel heap.
 */
#ifndef WUBU_CAB_H
#define WUBU_CAB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One entry from the CFFILE table. */
typedef struct {
    char     name[128];       /* filename (NUL-terminated, '\\' separators) */
    uint32_t uncomp_size;     /* cbFile: uncompressed size */
    uint32_t uoff_folder;     /* uoffFolderStart: offset in folder stream */
    uint16_t i_folder;        /* which folder holds this file */
    uint16_t attrib;
} cab_file;

/* One CFFOLDER entry. */
typedef struct {
    uint32_t coff_cab_start;  /* file offset of this folder's first CFDATA */
    uint16_t c_cfdata;        /* number of CFDATA blocks in the folder */
    uint16_t type_compress;   /* 0=stored 1=MSZIP 7=LZX; hi bits: window */
} cab_folder;

/* Parsed archive. `base` points at the caller's CAB bytes (read-only). */
typedef struct {
    const uint8_t *base;
    uint32_t       size;
    uint16_t       c_folders;
    uint16_t       c_files;
    cab_folder    *folders;   /* mem_alloc'd, freed by cab_close */
    cab_file      *files;     /* mem_alloc'd, freed by cab_close */
    uint32_t      *folder_uncomp; /* per-folder total uncompressed bytes */
    uint32_t       csum;      /* set if cab_open found a valid cabinet */
} cab_archive;

/* Parse a CAB whose bytes are at `data[0..size)`. Returns 0 on ok. */
int cab_open(cab_archive *cab, const uint8_t *data, uint32_t size);
void cab_close(cab_archive *cab);

/* Find a file by exact name match ("redist\\instmsiw.exe") or by case-
 * insensitive suffix match ("instmsiw.exe"). -1 if absent. */
int cab_find(const cab_archive *cab, const char *name);

/* Extract file `idx` into `out` (capacity cap). On success returns the
 * number of bytes written (== files[idx].uncomp_size) and sets
 * *out_len; returns -1 on any decode/format error. */
int32_t cab_extract(const cab_archive *cab, int idx,
                    uint8_t *out, uint32_t cap);

#ifdef __cplusplus
}
#endif
#endif /* WUBU_CAB_H */
