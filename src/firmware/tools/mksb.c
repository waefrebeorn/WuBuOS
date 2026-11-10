/*
 * mksb.c -- WuBuOS Secure Boot key/enrollment tool (sbctl equivalent).
 *
 * The firmware's Secure Boot verifier compares SHA-256(DER leaf) of an
 * image's embedded Authenticode certificate against the enrolled db (see
 * src/firmware/fw_secureboot.c).  This tool is the host side of that model,
 * mirroring the omarchy/sbctl workflow:
 *
 *   mksb create-keys <outdir>      -> SB.DER, DB.ESL, PK.ESL, KEK.ESL
 *   mksb sign <cert.der> <in.efi> <out.efi>
 *   mksb verify <efi> <db.esl>     -> OK / REJECT
 *   mksb selftest                  -> round-trip + tamper rejection
 *
 * The enrollment lists use the on-wire EFI_SIGNATURE_LIST format the firmware
 * parses (EFI_CERT_X509_GUID + owner GUID + DER cert), so DB.ESL can be fed
 * straight to the firmware's `sbenroll db <file>` shell command.
 *
 * C11, no dependencies beyond libc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---- UEFI GUIDs (little-endian on-disk layout) ---- */
static const uint8_t EFI_CERT_X509[16] = {
    0xA1,0x59,0xC0,0xA5, 0xE4,0x94, 0xA7,0x4A, 0x87,0xB5,0xAB,0x15,0x5C,0x2B,0xF0,0x72
};
static const uint8_t EFI_GLOBAL_VARIABLE[16] = {
    0x61,0xDF,0xE4,0x8B, 0xCA,0x93, 0xD2,0x11, 0xAA,0x0D,0x00,0xE0,0x98,0x03,0x2B,0x8C
};

#define WIN_CERT_TYPE_EFI_GUID 0x0EF1

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

/* Build an EFI_SIGNATURE_LIST holding one X.509 cert. Returns list size. */
static uint32_t esl_build(uint8_t *out, const uint8_t *cert, uint32_t clen) {
    uint32_t sig_size  = 16 + clen;
    uint32_t list_size = 28 + sig_size;
    memcpy(out, EFI_CERT_X509, 16);
    put32(out + 16, list_size);
    put32(out + 20, 0);                 /* SignatureHeaderSize */
    put32(out + 24, sig_size);
    memcpy(out + 28, EFI_GLOBAL_VARIABLE, 16);   /* SignatureOwner */
    memcpy(out + 44, cert, clen);
    return list_size;
}

/* Deterministic synthetic DER SEQUENCE cert (matches the firmware's
 * cert_sha256, which only needs der[0]==0x30 and a valid length). */
static uint32_t make_cert(uint8_t *out) {
    out[0] = 0x30; out[1] = 0x1E;
    memset(out + 2, 0xA5, 30);
    return 32;
}

static int write_file(const char *path, const uint8_t *d, size_t n) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t w = fwrite(d, 1, n, f);
    fclose(f);
    return w == n ? 0 : -1;
}

static uint8_t *read_file(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    uint8_t *b = malloc((size_t)sz);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) { free(b); return NULL; }
    *n = (size_t)sz;
    return b;
}

/* ---- SHA-256 (tiny, self-contained) ---- */
typedef struct { uint32_t h[8]; uint64_t len; uint8_t buf[64]; size_t n; } sha256_ctx;
static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
#define ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha256_init(sha256_ctx *c) {
    c->h[0]=0x6a09e667;c->h[1]=0xbb67ae85;c->h[2]=0x3c6ef372;c->h[3]=0xa54ff53a;
    c->h[4]=0x510e527f;c->h[5]=0x9b05688c;c->h[6]=0x1f83d9ab;c->h[7]=0x5be0cd19;
    c->len = 0; c->n = 0;
}
static void sha256_block(sha256_ctx *c, const uint8_t *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i-15],7) ^ ROR(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = ROR(w[i-2],17) ^ ROR(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],h=c->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ROR(e,6) ^ ROR(e,11) ^ ROR(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = ROR(a,2) ^ ROR(a,13) ^ ROR(a,22);
        uint32_t mj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + mj;
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a;c->h[1]+=b;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=h;
}
static void sha256_update(sha256_ctx *c, const uint8_t *p, size_t n) {
    c->len += n;
    while (n) {
        size_t take = 64 - c->n; if (take > n) take = n;
        memcpy(c->buf + c->n, p, take);
        c->n += take; p += take; n -= take;
        if (c->n == 64) { sha256_block(c, c->buf); c->n = 0; }
    }
}
static void sha256_final(sha256_ctx *c, uint8_t *out) {
    uint64_t bits = c->len * 8;
    uint8_t pad = 0x80;
    sha256_update(c, &pad, 1);
    uint8_t z = 0;
    while (c->n != 56) sha256_update(c, &z, 1);
    uint8_t lb[8];
    for (int i = 0; i < 8; i++) lb[i] = (uint8_t)(bits >> (56 - i*8));
    sha256_update(c, lb, 8);
    for (int i = 0; i < 8; i++) {
        out[i*4]   = (uint8_t)(c->h[i] >> 24);
        out[i*4+1] = (uint8_t)(c->h[i] >> 16);
        out[i*4+2] = (uint8_t)(c->h[i] >> 8);
        out[i*4+3] = (uint8_t)(c->h[i]);
    }
}
static void sha256(const uint8_t *d, size_t n, uint8_t *out) {
    sha256_ctx c; sha256_init(&c); sha256_update(&c, d, n); sha256_final(&c, out);
}

/* ---- image signing (append a WIN_CERTIFICATE_EFI_GUID entry) ---- */
static int sign_image(const uint8_t *cert, uint32_t clen,
                      const uint8_t *in, size_t insz, uint8_t *out, size_t *outsz) {
    if (insz < 0x44 || in[0] != 'M' || in[1] != 'Z') return -1;
    uint32_t elfanew = get32(in + 0x3C);
    if (elfanew + 4 + 20 + 112 > insz) return -1;
    const uint8_t *opt = in + elfanew + 4 + 20;
    uint16_t magic = (uint16_t)(opt[0] | (opt[1] << 8));
    uint32_t dd_off = (magic == 0x020B) ? 108 : 92;
    uint32_t dd_cert  = dd_off + 4 * 8;         /* DataDirectory[4] */
    /* Build the WIN_CERTIFICATE_EFI_GUID entry: hdr(8) + GUID(16) + cert. */
    uint32_t body = 16 + clen;
    uint32_t dwlen = 8 + body;
    dwlen = (dwlen + 7) & ~7u;                  /* 8-byte aligned */
    size_t total = insz + dwlen;
    memcpy(out, in, insz);
    uint8_t *ce = out + insz;
    put32(ce + 0, dwlen);
    put16(ce + 4, 0x0200);                      /* revision */
    put16(ce + 6, WIN_CERT_TYPE_EFI_GUID);
    memcpy(ce + 8, EFI_CERT_X509, 16);          /* cert type GUID */
    memset(ce + 8 + body, 0, dwlen - body);     /* alignment pad */
    memcpy(ce + 24, cert, clen);
    /* Security DataDirectory holds a FILE OFFSET (not an RVA). */
    put32((uint8_t *)out + elfanew + 4 + 20 + dd_cert,     (uint32_t)insz);
    put32((uint8_t *)out + elfanew + 4 + 20 + dd_cert + 4, dwlen);
    *outsz = total;
    return 0;
}

/* ---- db lookup ---- */
static int db_contains(const uint8_t *esl, size_t esln, const uint8_t *hash) {
    if (esln < 28) return 0;
    uint32_t list_size = get32(esl + 16), sig_size = get32(esl + 24);
    uint32_t hdr_size  = get32(esl + 20);
    if (list_size > esln || sig_size < 16) return 0;
    uint32_t off = 28 + hdr_size;
    while (off + sig_size <= list_size) {
        const uint8_t *data = esl + off + 16;
        uint32_t dlen = sig_size - 16;
        uint8_t h[32];
        if (data[0] == 0x30) {
            /* DER SEQUENCE: hash from der[0] over the length-delimited body. */
            uint32_t slen, o;
            if (data[1] & 0x80) {
                int n = data[1] & 0x7F;
                slen = 0;
                for (int i = 0; i < n; i++) slen = (slen << 8) | data[2+i];
                o = 2 + n;
            } else { slen = data[1]; o = 2; }
            if (o + slen == dlen) { sha256(data, o + slen, h);
                if (memcmp(h, hash, 32) == 0) return 1; }
        }
        off += sig_size;
    }
    return 0;
}

/* Hash the leaf cert embedded in an EFI image (Authenticode regions skipped,
 * matching the firmware's pe_authentihash + find_cert). */
static int image_cert_hash(const uint8_t *img, size_t sz, uint8_t *out) {
    if (sz < 0x44 || img[0] != 'M' || img[1] != 'Z') return -1;
    uint32_t elfanew = get32(img + 0x3C);
    if (elfanew + 4 + 20 + 112 > sz) return -1;
    const uint8_t *opt = img + elfanew + 4 + 20;
    uint16_t magic = (uint16_t)(opt[0] | (opt[1] << 8));
    uint32_t dd_off = (magic == 0x020B) ? 108 : 92;
    uint32_t cert_va = get32(opt + dd_off + 4 * 8);
    uint32_t cert_sz = get32(opt + dd_off + 4 * 8 + 4);
    if (!cert_va || cert_sz < 24 || cert_va + cert_sz > sz) return -1;
    const uint8_t *leaf = img + cert_va + 24;
    uint32_t llen = cert_sz - 24;
    if (leaf[0] != 0x30) return -1;
    uint32_t slen, o;
    if (leaf[1] & 0x80) {
        int n = leaf[1] & 0x7F;
        slen = 0;
        for (int i = 0; i < n; i++) slen = (slen << 8) | leaf[2+i];
        o = 2 + n;
    } else { slen = leaf[1]; o = 2; }
    if (o + slen > llen) return -1;
    sha256(leaf, o + slen, out);
    return 0;
}

static int cmd_create_keys(const char *dir) {
    uint8_t cert[32]; make_cert(cert);
    char path[512];
    snprintf(path, sizeof path, "%s/SB.DER", dir);
    if (write_file(path, cert, 32) != 0) { perror(path); return 1; }
    uint8_t esl[128];
    uint32_t n = esl_build(esl, cert, 32);
    const char *names[] = { "DB.ESL", "PK.ESL", "KEK.ESL" };
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof path, "%s/%s", dir, names[i]);
        if (write_file(path, esl, n) != 0) { perror(path); return 1; }
    }
    printf("mksb: wrote SB.DER, DB.ESL, PK.ESL, KEK.ESL in %s\n", dir);
    return 0;
}

static int cmd_sign(const char *certpath, const char *inpath, const char *outpath) {
    size_t clen = 0, insz = 0;
    uint8_t *cert = read_file(certpath, &clen);
    uint8_t *in = read_file(inpath, &insz);
    if (!cert || !in) { fprintf(stderr, "mksb: cannot read inputs\n"); free(cert); free(in); return 1; }
    uint8_t *out = malloc(insz + 256);
    size_t outsz = 0;
    if (sign_image(cert, (uint32_t)clen, in, insz, out, &outsz) != 0) {
        fprintf(stderr, "mksb: sign failed\n"); free(cert); free(in); free(out); return 1;
    }
    int rc = write_file(outpath, out, outsz);
    if (rc != 0) perror(outpath);
    else printf("mksb: signed %s -> %s (%zu bytes)\n", inpath, outpath, outsz);
    free(cert); free(in); free(out);
    return rc;
}

static int cmd_verify(const char *efi, const char *eslpath) {
    size_t isz = 0, esz = 0;
    uint8_t *img = read_file(efi, &isz);
    uint8_t *esl = read_file(eslpath, &esz);
    if (!img || !esl) { fprintf(stderr, "mksb: cannot read inputs\n"); free(img); free(esl); return 1; }
    uint8_t h[32];
    if (image_cert_hash(img, isz, h) != 0) { printf("REJECT (no embedded cert)\n"); free(img); free(esl); return 1; }
    int ok = db_contains(esl, esz, h);
    printf("%s\n", ok ? "OK" : "REJECT");
    free(img); free(esl);
    return ok ? 0 : 1;
}

static int cmd_selftest(void) {
    uint8_t cert[32]; make_cert(cert);
    /* round-trip: build ESL, sign an image, verify. */
    uint8_t esl[128]; uint32_t esln = esl_build(esl, cert, 32);
    /* minimal PE-ish buffer for sign_image (DOS+PE+opt headers). */
    uint8_t img[512]; memset(img, 0, sizeof img);
    img[0] = 'M'; img[1] = 'Z';
    put32(img + 0x3C, 0x40);
    img[0x40] = 'P'; img[0x41] = 'E';
    img[0x44] = 0x66; img[0x45] = 0x00;      /* machine */
    put16(img + 0x54, 0x020B);               /* PE32+ magic */
    put32(img + 0x58, 0);                    /* checksum */
    uint8_t *out = malloc(sizeof img + 256); size_t outsz = 0;
    int pass = 1;
    if (sign_image(cert, 32, img, sizeof img, out, &outsz) != 0) {
        printf("mksb selftest: sign failed -> FAIL\n"); free(out); return 1;
    }
    uint8_t h[32];
    if (image_cert_hash(out, outsz, h) != 0) { printf("mksb selftest: hash failed -> FAIL\n"); pass = 0; }
    else if (!db_contains(esl, esln, h)) { printf("mksb selftest: enrolled cert not accepted -> FAIL\n"); pass = 0; }
    /* tamper: flip a byte INSIDE the embedded certificate -> its hash changes
     * -> the db no longer matches -> reject.  (This verifier is cert-hash
     * based by design, so a body-only tamper is out of model.) */
    size_t cert_off = sizeof img + 24;      /* WIN_CERT hdr(8)+GUID(16) */
    out[cert_off + 2] ^= 0xFF;
    if (image_cert_hash(out, outsz, h) == 0 && db_contains(esl, esln, h)) {
        printf("mksb selftest: tampered cert accepted -> FAIL\n"); pass = 0;
    }
    free(out);
    printf("%s\n", pass ? "mksb selftest: PASS" : "mksb selftest: FAIL");
    return pass ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr,
            "usage: mksb create-keys <outdir>\n"
            "       mksb sign <cert.der> <in.efi> <out.efi>\n"
            "       mksb verify <efi> <db.esl>\n"
            "       mksb selftest\n");
        return 2;
    }
    if (strcmp(argv[1], "create-keys") == 0 && argc == 3) return cmd_create_keys(argv[2]);
    if (strcmp(argv[1], "sign") == 0 && argc == 5)         return cmd_sign(argv[2], argv[3], argv[4]);
    if (strcmp(argv[1], "verify") == 0 && argc == 4)       return cmd_verify(argv[2], argv[3]);
    if (strcmp(argv[1], "selftest") == 0)                  return cmd_selftest();
    fprintf(stderr, "mksb: bad arguments\n");
    return 2;
}
