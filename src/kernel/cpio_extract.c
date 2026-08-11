/*
 * cpio_extract -- decompress a gzipped cpio archive and extract one file,
 *                using ONLY the kernel's own wubu_gunzip (gzip/DEFLATE)
 *                decoder (wubu_inflate.c). The cpio framing is parsed in C11
 *                in this file; the gzip decompression is kernel-owned.
 *
 * Usage:
 *   cpio_extract <gz_path> <member_path> <out_path>
 *   cpio_extract list <gz_path>
 *
 * Example:
 *   cpio_extract Archive.pax.gz "Halo.app/Contents/MacOS/Halo" /tmp/halo
 *
 * C11, freestanding-friendly. Links wubu_inflate.c + libc_string.c + memory.c.
 */
#include "wubu_zlib.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint8_t *load_file(const char *path, uint32_t *sz) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *sz = (uint32_t)n;
    return b;
}

/* Parse one cpio newc entry. Returns 0 on success, 1 if this is the
 * TRAILER!!! entry (end of archive), -1 on error. Sets *name and *data
 * and *data_len, *entry_size (including header+name+padding). */
static int cpio_next(const uint8_t *p, uint32_t n,
                     const char **name, const uint8_t **data,
                     uint32_t *data_len, uint32_t *entry_size) {
    if (n < 6) return -1;
    if (memcmp(p, "070701", 6) != 0 && memcmp(p, "070702", 6) != 0)
        return -1;  /* not a cpio newc entry */

    /* newc header: magic(6) ino(8) mode(8) uid(8) gid(8) nlink(8)
     * mtime(8) filesize(8) dev(8) devmajor(8) devminor(8) rdev(8)
     * enamelen(8) check(16)  = 11 octal fields * 8 bytes after magic */
    /* parse 11 8-char-octal fields at p+6 */
    #define CPL(field) do { \
        char tmp[9]; memcpy(tmp, (char*)p + 6 + (field)*8, 8); tmp[8]=0; \
        c_##field = (uint32_t)strtoul(tmp, NULL, 8); } while(0)
    uint32_t c_namesize, c_filesize;
    CPL(10);  /* enamelen -> namesize */
    CPL(6);   /* filesize */
    /* Actually: fields are 0=ino,1=mode,2=uid,3=gid,4=nlink,5=mtime,
     * 6=filesize,7=dev,8=devmajor,9=devminor,10=rdev,11=enamelen */
    /* recompute with correct indices */
    char tmp[9];
    memcpy(tmp, p+6+6*8, 8); tmp[8]=0; c_filesize = (uint32_t)strtoul(tmp, NULL, 8);
    memcpy(tmp, p+6+11*8, 8); tmp[8]=0; c_namesize = (uint32_t)strtoul(tmp, NULL, 8);

    uint32_t hdr_size = 6 + 11*8 + c_namesize;  /* magic + 11 fields + name */
    if (hdr_size > n) return -1;
    const char *nm = (const char *)p + 6 + 11*8;
    /* name is NUL-padded to even */
    hdr_size = (hdr_size + 1) & ~1u;

    const uint8_t *datap = (const uint8_t *)p + hdr_size;
    uint32_t datasz = c_filesize;
    uint32_t entry_total = hdr_size + datasz;
    entry_total = (entry_total + 1) & ~1u;  /* pad to even */

    *name = nm;
    *data = datap;
    *data_len = datasz;
    *entry_size = entry_total;

    /* check for TRAILER!!! */
    if (strcmp(nm, "TRAILER!!!") == 0) return 1;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s list <gz_path>\n"
                        "       %s get  <gz_path> <member> <out_path>\n",
                argv[0], argv[0]);
        return 1;
    }

    int listing = (strcmp(argv[1], "list") == 0);
    const char *gz_path = listing ? argv[2] : argv[2];
    const char *member = listing ? NULL : argv[3];
    const char *out_path = listing ? NULL : argv[4];

    uint32_t gz_sz;
    uint8_t *gz = load_file(gz_path, &gz_sz);
    if (!gz) {
        fprintf(stderr, "cannot load %s\n", gz_path);
        return 1;
    }

    if (mem_init(1024 * 1024 * 1024) != 0) {  /* 1GB pool for large PKG payloads */
        fprintf(stderr, "mem_init failed\n");
        return 1;
    }

    /* Decompress the gzip stream with the kernel DEFLATE decoder.
     * The PKG payload can be ~1.5GB uncompressed, so we use the kernel kmalloc. */
    uint8_t *decomp = mem_calloc(1, 1024 * 1024 * 1024);  /* 1GB cap */
    if (!decomp) {
        fprintf(stderr, "kmalloc 1GB failed\n");
        return 1;
    }
    uint32_t dec_len = 0;
    uint32_t consumed = 0;
    int rc = wubu_gunzip(gz, gz_sz, decomp, 1024 * 1024 * 1024, &dec_len, &consumed);
    if (rc != 0) {
        fprintf(stderr, "gunzip failed: rc=%d\n", rc);
        mem_free(decomp);
        mem_shutdown();
        free(gz);
        return 1;
    }
    fprintf(stderr, "decompressed %u bytes (consumed %u/%u of gzip)\n", dec_len, consumed, gz_sz);
    free(gz);  /* no longer need the compressed data */

    /* Walk cpio entries */
    uint32_t pos = 0;
    int found = 0;
    while (pos + 6 <= dec_len) {
        const char *name;
        const uint8_t *data;
        uint32_t data_len, entry_size;
        int r = cpio_next(decomp + pos, dec_len - pos, &name, &data, &data_len, &entry_size);
        if (r == 1) break;  /* TRAILER!!! */
        if (r < 0) {
            fprintf(stderr, "cpio parse error at pos %u\n", pos);
            break;
        }
        if (listing) {
            fprintf(stderr, "  [%u] %s (%u bytes)\n", found, name, data_len);
        }
        if (!found && member && strcmp(name, member) == 0) {
            if (out_path) {
                FILE *of = fopen(out_path, "wb");
                if (!of) {
                    fprintf(stderr, "cannot write %s\n", out_path);
                    mem_free(decomp);
                    mem_shutdown();
                    return 1;
                }
                fwrite(data, 1, data_len, of);
                fclose(of);
                fprintf(stderr, "extracted %s: %u bytes -> %s (magic: %02x %02x %02x %02x)\n",
                        name, data_len, out_path, data[0], data[1], data[2], data[3]);
            }
            found = 1;
        }
        pos += entry_size;
    }

    if (!listing && !found) {
        fprintf(stderr, "member '%s' not found in cpio archive\n", member);
        mem_free(decomp);
        mem_shutdown();
        return 1;
    }

    mem_free(decomp);
    mem_shutdown();
    return 0;
}
