/*
 * wubu_cab.c -- the IN-KERNEL CAB (MS-CAB) reader.
 *
 * Walks the CFHEADER (36 bytes, flags=0 for Halo's Inno cabinet),
 * the CFFOLDER table, the contiguous CFDATA block list, and the
 * CFFILE table, then extracts a member through the kernel's own
 * decoders:
 *
 *   method 0 (stored)   -> memcpy
 *   method 1 (MSZIP)    -> wubu_inflate_raw (kernel DEFLATE)
 *   method 7 (LZX)      -> lzx_decompress (kernel LZX, ReactOS fdi.c
 *                          calling convention: one call per CFDATA
 *                          block, block state persists across calls,
 *                          bit reader restarts per call)
 *
 * Verified against the real Halo PC CAB1.CAB (embedded in
 * halo_pc_trial_setup.exe at offset 550324): all 5 files of folder 0
 * (instmsia/instmsiw/msxmlenu/Shfolder/GSArcade) decode byte-identical
 * to the host-7z oracle (lzx_selftest.c).
 *
 * C11, freestanding: allocations via mem_alloc/mem_free (memory.h).
 */
#include "wubu_cab.h"
#include "wubu_lzx.h"
#include "wubu_zlib.h"
#include "memory.h"
#include <string.h>

/* ---- little-endian readers ---- */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

#define CAB_SIG        0x4643534du   /* "MSCF" little-endian */

int cab_open(cab_archive *cab, const uint8_t *data, uint32_t size) {
    if (!cab || !data || size < 36) return -1;
    if (rd32(data) != CAB_SIG) return -1;

    cab->base = data;
    cab->size = size;
    cab->csum = 0;
    cab->folders = NULL;
    cab->files = NULL;
    cab->folder_uncomp = NULL;

    uint16_t c_folders = rd16(data + 26);
    uint16_t c_files   = rd16(data + 28);
    uint16_t flags     = rd16(data + 30);
    uint32_t coff_files = rd32(data + 16);
    if (c_folders == 0 || c_files == 0) return -1;
    if (coff_files > size) return -1;

    cab->c_folders = c_folders;
    cab->c_files = c_files;

    /* header may carry a reserved area (flags bit 2) */
    uint32_t off = 36;
    if (flags & 4) off += 4;

    /* folder table: 8 bytes each, immediately after the header */
    uint32_t folder_tbl = off;
    if (folder_tbl + 8u * c_folders > coff_files) return -1;

    cab->folders = (cab_folder *)mem_calloc(c_folders, sizeof(cab_folder));
    cab->folder_uncomp = (uint32_t *)mem_calloc(c_folders, sizeof(uint32_t));
    if (!cab->folders || !cab->folder_uncomp) { cab_close(cab); return -1; }

    for (uint32_t i = 0; i < c_folders; i++) {
        const uint8_t *f = data + folder_tbl + i * 8;
        cab->folders[i].coff_cab_start = rd32(f);
        cab->folders[i].c_cfdata       = rd16(f + 4);
        cab->folders[i].type_compress  = rd16(f + 6);
    }

    /* CFDATA block list: contiguous from each folder's coff_cab_start.
     * Sum the uncompressed sizes per folder for extraction buffers. */
    for (uint32_t i = 0; i < c_folders; i++) {
        uint32_t p = cab->folders[i].coff_cab_start;
        uint32_t total = 0;
        for (uint32_t b = 0; b < cab->folders[i].c_cfdata; b++) {
            if (p + 8 > size) { cab_close(cab); return -1; }
            uint32_t cb_data = rd16(data + p + 4);
            uint32_t cb_unc  = rd16(data + p + 6);
            total += cb_unc;
            p += 8 + cb_data;
        }
        cab->folder_uncomp[i] = total;
    }

    /* CFFILE table at coff_files: 16-byte fixed + NUL-terminated name */
    cab->files = (cab_file *)mem_calloc(c_files, sizeof(cab_file));
    if (!cab->files) { cab_close(cab); return -1; }

    uint32_t p = coff_files;
    for (uint32_t i = 0; i < c_files; i++) {
        if (p + 16 > size) { cab_close(cab); return -1; }
        cab_file *f = &cab->files[i];
        f->uncomp_size  = rd32(data + p);
        f->uoff_folder  = rd32(data + p + 4);
        f->i_folder     = rd16(data + p + 8);
        f->attrib       = rd16(data + p + 14);
        p += 16;
        uint32_t nl = 0;
        while (p + nl < size && data[p + nl] != 0 && nl < sizeof(f->name) - 1) nl++;
        if (p + nl >= size) { cab_close(cab); return -1; }
        memcpy(f->name, data + p, nl);
        f->name[nl] = '\0';
        p += nl + 1;
    }

    cab->csum = 1;
    return 0;
}

void cab_close(cab_archive *cab) {
    if (!cab) return;
    if (cab->folders)       mem_free(cab->folders);
    if (cab->files)         mem_free(cab->files);
    if (cab->folder_uncomp) mem_free(cab->folder_uncomp);
    cab->folders = NULL;
    cab->files = NULL;
    cab->folder_uncomp = NULL;
    cab->csum = 0;
}

static int cab_name_eq(const char *a, const char *b) {
    /* case-insensitive (Windows names); '\\' and '/' equivalent */
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca == '\\') ca = '/';
        if (cb == '\\') cb = '/';
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == *b;
}

static int cab_suffix_eq(const char *name, const char *suffix) {
    size_t nl = strlen(name), sl = strlen(suffix);
    if (sl > nl) return 0;
    return cab_name_eq(name + nl - sl, suffix);
}

int cab_find(const cab_archive *cab, const char *name) {
    if (!cab || !name || !cab->csum) return -1;
    for (uint32_t i = 0; i < cab->c_files; i++) {
        if (cab_name_eq(cab->files[i].name, name) ||
            cab_suffix_eq(cab->files[i].name, name))
            return (int)i;
    }
    return -1;
}

/* Decompress one folder's whole stream into `buf` (folder_uncomp bytes). */
static int cab_decompress_folder(const cab_archive *cab, uint16_t fidx,
                                 uint8_t *buf) {
    const uint8_t *data = cab->base;
    uint32_t size = cab->size;
    const cab_folder *fold = &cab->folders[fidx];
    uint16_t method = fold->type_compress & 0x0F;
    /* CAB typeCompress: 0=none 1=MSZIP 3=LZX (upper byte = window bits) */

    if (method == 3) {
        /* LZX:21 .. LZX:25 by the window bits in the high nibble */
        uint32_t winbits = (fold->type_compress >> 8) & 0x1F;
        if (winbits < 15 || winbits > 21) winbits = 21;
        struct lzx_state *st = lzx_init(1u << winbits);
        if (!st) return -1;
        lzx_reset(st);

        uint32_t p = fold->coff_cab_start;
        uint32_t out_off = 0;
        for (uint32_t b = 0; b < fold->c_cfdata; b++) {
            if (p + 8 > size) { lzx_free(st); return -1; }
            uint32_t cb_data = rd16(data + p + 4);
            uint32_t cb_unc  = rd16(data + p + 6);
            p += 8;
            if (p + cb_data > size || out_off + cb_unc > cab->folder_uncomp[fidx]) {
                lzx_free(st); return -1;
            }
            /* +2 zero padding after each block (ReactOS requirement) */
            uint8_t *in = mem_alloc(cb_data + 4);
            if (!in) { lzx_free(st); return -1; }
            memcpy(in, data + p, cb_data);
            in[cb_data] = in[cb_data+1] = in[cb_data+2] = in[cb_data+3] = 0;
            int rc = lzx_decompress(st, in, cb_data, buf + out_off, cb_unc);
            mem_free(in);
            if (rc != LZX_OK) { lzx_free(st); return -1; }
            out_off += cb_unc;
            p += cb_data;
        }
        lzx_free(st);
        return 0;
    }

    if (method == 1) {
        /* MSZIP: each CFDATA block is an independent raw-DEFLATE stream
         * prefixed by the 2-byte "CK" signature. */
        uint32_t p = fold->coff_cab_start;
        uint32_t out_off = 0;
        for (uint32_t b = 0; b < fold->c_cfdata; b++) {
            if (p + 8 > size) return -1;
            uint32_t cb_data = rd16(data + p + 4);
            uint32_t cb_unc  = rd16(data + p + 6);
            p += 8;
            if (p + cb_data > size || out_off + cb_unc > cab->folder_uncomp[fidx])
                return -1;
            const uint8_t *src = data + p;
            uint32_t slen = cb_data;
            if (slen >= 2 && src[0] == 0x43 && src[1] == 0x4B) { src += 2; slen -= 2; }
            uint32_t got = 0, consumed = 0;
            int rc = wubu_inflate_raw(src, slen, buf + out_off, cb_unc,
                                      &got, &consumed);
            if (rc != 0 || got != cb_unc) return -1;
            out_off += got;
            p += cb_data;
        }
        return 0;
    }

    if (method == 0) {
        /* stored: raw copy */
        uint32_t p = fold->coff_cab_start;
        uint32_t out_off = 0;
        for (uint32_t b = 0; b < fold->c_cfdata; b++) {
            if (p + 8 > size) return -1;
            uint32_t cb_data = rd16(data + p + 4);
            uint32_t cb_unc  = rd16(data + p + 6);
            p += 8;
            if (p + cb_data > size || out_off + cb_unc > cab->folder_uncomp[fidx])
                return -1;
            memcpy(buf + out_off, data + p, cb_unc);
            out_off += cb_unc;
            p += cb_data;
        }
        return 0;
    }

    return -1;   /* unknown method */
}

int32_t cab_extract(const cab_archive *cab, int idx,
                    uint8_t *out, uint32_t cap) {
    if (!cab || !cab->csum || idx < 0 || (uint32_t)idx >= cab->c_files)
        return -1;
    const cab_file *f = &cab->files[idx];
    if (f->uncomp_size > cap) return -1;
    if (f->i_folder >= cab->c_folders) return -1;

    uint8_t *buf = (uint8_t *)mem_alloc(cab->folder_uncomp[f->i_folder]);
    if (!buf) return -1;
    if (cab_decompress_folder(cab, f->i_folder, buf) != 0) {
        mem_free(buf);
        return -1;
    }
    if (f->uoff_folder + f->uncomp_size > cab->folder_uncomp[f->i_folder]) {
        mem_free(buf);
        return -1;
    }
    memcpy(out, buf + f->uoff_folder, f->uncomp_size);
    mem_free(buf);
    return (int32_t)f->uncomp_size;
}
