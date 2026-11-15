/*
 * wubu_pwcrypt.c -- SHA-512 crypt ($6$), the WuBuOS password-hash primitive.
 *
 * omarchy provisions its user at FIRST BOOT and hashes the password with
 * `openssl passwd -6` (SHA-512 crypt). WuBuOS has no GNU crypt(3) on bare
 * metal and the tree only shipped SHA-256 (wubu_crypto.h), so this module
 * implements the scheme itself so first-boot accounts store the same form a
 * real shadow file accepts.
 *
 * THE ALGORITHM IS A TRANSCRIPTION, not a paraphrase: the reference source is
 * phadej/crypt-sha512's sha512crypt.c (itself the glibc construction), which
 * ships the canonical `$6$saltstring` vectors. The tricky parts that are easy
 * to get wrong from the spec text alone, all checked against those vectors:
 *   - digest A folds a key-length bit-select (B vs key) after B is appended;
 *   - P and S are derived from DIGESTS (DP=SHA512(key x key_len),
 *     DS=SHA512(salt x (16 + A[0]))), repeated+truncated to key_len/salt_len;
 *   - the 5000-round loop hashes P/S, upward 0-based;
 *   - there is NO final digest: the last round's digest is encoded directly
 *     through a fixed 21-group b64 permutation.
 *
 * Reference vector (also cross-checked live against `openssl passwd -6`):
 *   password "correct horse", salt "wubuSALT"
 *   => $6$wubuSALT$jYIDcW7O/Uq465L2Z/qagQSaSpKiV4YqRvktyWFUaZOZ75kNNV/fNTGpGcp0u0oQ4HtXArHztdXrMbxGw1Lla.
 *
 * C11, single translation unit, no heap.
 */
#include "wubu_pwcrypt.h"
#include <string.h>
#include <stdint.h>

#define SHA512_BLOCK 128u
#define SHA512_DIG   64u
#define SALT_MAX     16u
#define ROUNDS_DEF   5000u

/* ------------------------------------------------------------------ */
/* SHA-512 (FIPS 180-4)                                               */
/* ------------------------------------------------------------------ */

static const uint64_t K512[80] = {
UINT64_C(0x428a2f98d728ae22), UINT64_C(0x7137449123ef65cd),
UINT64_C(0xb5c0fbcfec4d3b2f), UINT64_C(0xe9b5dba58189dbbc),
UINT64_C(0x3956c25bf348b538), UINT64_C(0x59f111f1b605d019),
UINT64_C(0x923f82a4af194f9b), UINT64_C(0xab1c5ed5da6d8118),
UINT64_C(0xd807aa98a3030242), UINT64_C(0x12835b0145706fbe),
UINT64_C(0x243185be4ee4b28c), UINT64_C(0x550c7dc3d5ffb4e2),
UINT64_C(0x72be5d74f27b896f), UINT64_C(0x80deb1fe3b1696b1),
UINT64_C(0x9bdc06a725c71235), UINT64_C(0xc19bf174cf692694),
UINT64_C(0xe49b69c19ef14ad2), UINT64_C(0xefbe4786384f25e3),
UINT64_C(0x0fc19dc68b8cd5b5), UINT64_C(0x240ca1cc77ac9c65),
UINT64_C(0x2de92c6f592b0275), UINT64_C(0x4a7484aa6ea6e483),
UINT64_C(0x5cb0a9dcbd41fbd4), UINT64_C(0x76f988da831153b5),
UINT64_C(0x983e5152ee66dfab), UINT64_C(0xa831c66d2db43210),
UINT64_C(0xb00327c898fb213f), UINT64_C(0xbf597fc7beef0ee4),
UINT64_C(0xc6e00bf33da88fc2), UINT64_C(0xd5a79147930aa725),
UINT64_C(0x06ca6351e003826f), UINT64_C(0x142929670a0e6e70),
UINT64_C(0x27b70a8546d22ffc), UINT64_C(0x2e1b21385c26c926),
UINT64_C(0x4d2c6dfc5ac42aed), UINT64_C(0x53380d139d95b3df),
UINT64_C(0x650a73548baf63de), UINT64_C(0x766a0abb3c77b2a8),
UINT64_C(0x81c2c92e47edaee6), UINT64_C(0x92722c851482353b),
UINT64_C(0xa2bfe8a14cf10364), UINT64_C(0xa81a664bbc423001),
UINT64_C(0xc24b8b70d0f89791), UINT64_C(0xc76c51a30654be30),
UINT64_C(0xd192e819d6ef5218), UINT64_C(0xd69906245565a910),
UINT64_C(0xf40e35855771202a), UINT64_C(0x106aa07032bbd1b8),
UINT64_C(0x19a4c116b8d2d0c8), UINT64_C(0x1e376c085141ab53),
UINT64_C(0x2748774cdf8eeb99), UINT64_C(0x34b0bcb5e19b48a8),
UINT64_C(0x391c0cb3c5c95a63), UINT64_C(0x4ed8aa4ae3418acb),
UINT64_C(0x5b9cca4f7763e373), UINT64_C(0x682e6ff3d6b2b8a3),
UINT64_C(0x748f82ee5defb2fc), UINT64_C(0x78a5636f43172f60),
UINT64_C(0x84c87814a1f0ab72), UINT64_C(0x8cc702081a6439ec),
UINT64_C(0x90befffa23631e28), UINT64_C(0xa4506cebde82bde9),
UINT64_C(0xbef9a3f7b2c67915), UINT64_C(0xc67178f2e372532b),
UINT64_C(0xca273eceea26619c), UINT64_C(0xd186b8c721c0c207),
UINT64_C(0xeada7dd6cde0eb1e), UINT64_C(0xf57d4f7fee6ed178),
UINT64_C(0x06f067aa72176fba), UINT64_C(0x0a637dc5a2c898a6),
UINT64_C(0x113f9804bef90dae), UINT64_C(0x1b710b35131c471b),
UINT64_C(0x28db77f523047d84), UINT64_C(0x32caab7b40c72493),
UINT64_C(0x3c9ebe0a15c9bebc), UINT64_C(0x431d67c49c100d4c),
UINT64_C(0x4cc5d4becb3e42b6), UINT64_C(0x597f299cfc657e2a),
UINT64_C(0x5fcb6fab3ad6faec), UINT64_C(0x6c44198c4a475817),
};

typedef struct { uint64_t h[8]; uint64_t blen; uint8_t b[SHA512_BLOCK]; size_t bn; } sha512_t;

#define ROR(x,n) (((x)>>(n))|((x)<<(64-(n))))
#define S0(x) (ROR(x,28)^ROR(x,34)^ROR(x,39))
#define S1(x) (ROR(x,14)^ROR(x,18)^ROR(x,41))
#define s0(x) (ROR(x,1)^ROR(x,8)^((x)>>7))
#define s1(x) (ROR(x,19)^ROR(x,61)^((x)>>6))
#define CH(x,y,z) (((x)&(y))^((~(x))&(z)))
#define MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))

static void sha512_block(sha512_t *c, const uint8_t *p)
{
    uint64_t w[80], a,b,d,e,f,g,h,cc;
    for (int i = 0; i < 16; i++)
        w[i] = ((uint64_t)p[i*8]<<56)|((uint64_t)p[i*8+1]<<48)|
               ((uint64_t)p[i*8+2]<<40)|((uint64_t)p[i*8+3]<<32)|
               ((uint64_t)p[i*8+4]<<24)|((uint64_t)p[i*8+5]<<16)|
               ((uint64_t)p[i*8+6]<<8)|(uint64_t)p[i*8+7];
    for (int i = 16; i < 80; i++)
        w[i] = s1(w[i-2]) + w[i-7] + s0(w[i-15]) + w[i-16];
    a=c->h[0]; b=c->h[1]; cc=c->h[2]; d=c->h[3];
    e=c->h[4]; f=c->h[5]; g=c->h[6]; h=c->h[7];
    for (int i = 0; i < 80; i++) {
        uint64_t t1 = h + S1(e) + CH(e,f,g) + K512[i] + w[i];
        uint64_t t2 = S0(a) + MAJ(a,b,cc);
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
}

static void sha512_reset(sha512_t *c)
{
    c->h[0]=UINT64_C(0x6a09e667f3bcc908); c->h[1]=UINT64_C(0xbb67ae8584caa73b);
    c->h[2]=UINT64_C(0x3c6ef372fe94f82b); c->h[3]=UINT64_C(0xa54ff53a5f1d36f1);
    c->h[4]=UINT64_C(0x510e527fade682d1); c->h[5]=UINT64_C(0x9b05688c2b3e6c1f);
    c->h[6]=UINT64_C(0x1f83d9abfb41bd6b); c->h[7]=UINT64_C(0x5be0cd19137e2179);
    c->blen=0; c->bn=0;
}

static void sha512_update(sha512_t *c, const uint8_t *p, size_t len)
{
    while (len) {
        size_t need = SHA512_BLOCK - c->bn;
        if (len < need) { memcpy(c->b + c->bn, p, len); c->bn += len; c->blen += len; return; }
        memcpy(c->b + c->bn, p, need);
        c->blen += need;
        sha512_block(c, c->b);
        c->bn = 0; p += need; len -= need;
    }
}

/* Append 0x80 pad + 128-bit big-endian bit length; digest into out[64]. */
static void sha512_final(sha512_t *c, uint8_t out[SHA512_DIG])
{
    uint64_t bits = (uint64_t)c->blen * 8u;
    c->b[c->bn++] = 0x80;
    if (c->bn > SHA512_BLOCK - 16) {
        while (c->bn < SHA512_BLOCK) c->b[c->bn++] = 0;
        sha512_block(c, c->b); c->bn = 0;
    }
    while (c->bn < SHA512_BLOCK - 16) c->b[c->bn++] = 0;
    for (int i = 0; i < 8; i++) c->b[c->bn++] = 0;      /* hi 64 bits */
    for (int i = 7; i >= 0; i--) c->b[c->bn++] = (uint8_t)(bits >> (i*8));
    sha512_block(c, c->b);
    for (int i = 0; i < 8; i++)
        for (int j = 7; j >= 0; j--)
            out[i*8 + (7-j)] = (uint8_t)(c->h[i] >> (j*8));   /* big-endian */
}

/* ------------------------------------------------------------------ */
/* $6$ crypt                                                  */
/* ------------------------------------------------------------------ */

static const char b64t[] =
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

/* Encode N chars (4 or 2) of the 24-bit word (B2<<16)|(B1<<8)|B0, LSB first,
 * appending to cp. Mirrors glibc's b64_from_24bit. */
static char *b64w(char *cp, unsigned b2, unsigned b1, unsigned b0, int n)
{
    unsigned w = (b2 << 16) | (b1 << 8) | b0;
    while (n-- > 0) { *cp++ = b64t[w & 0x3f]; w >>= 6; }
    return cp;
}

const char *wubu_pw_crypt_sha512(const char *key, const char *salt,
                                 char out[WUBU_PWCRYPT_OUT])
{
    if (!key || !salt || !out) return NULL;

    if (salt[0]=='$' && salt[1]=='6' && salt[2]=='$') salt += 3;   /* strip prefix */

    /* salt_len = up to 16 chars, stop at `$`; key_len = password length. */
    size_t salt_len = 0;
    while (salt[salt_len] && salt[salt_len] != '$' && salt_len < SALT_MAX) salt_len++;
    if (salt_len == 0) return NULL;
    size_t key_len = strlen(key);

    uint8_t alt_result[SHA512_DIG], temp_result[SHA512_DIG];
    sha512_t ctx, alt_ctx;
    size_t cnt;
    char *cp;

    /* Prepare for the real work: ctx accumulates KEY + SALT (the platform
     * digest for A), and alt_ctx computes B = SHA512(key+salt+key). */
    sha512_reset(&ctx);
    sha512_update(&ctx, (const uint8_t*)key, key_len);
    sha512_update(&ctx, (const uint8_t*)salt, salt_len);

    sha512_reset(&alt_ctx);
    sha512_update(&alt_ctx, (const uint8_t*)key, key_len);
    sha512_update(&alt_ctx, (const uint8_t*)salt, salt_len);
    sha512_update(&alt_ctx, (const uint8_t*)key, key_len);
    sha512_final(&alt_ctx, alt_result);                  /* alt_result = B */

    /* Feed B, spread over the key length. */
    for (cnt = key_len; cnt > 64; cnt -= 64)
        sha512_update(&ctx, alt_result, SHA512_DIG);
    sha512_update(&ctx, alt_result, cnt);
    /* Then select B vs key by the bits of key_len. */
    for (cnt = key_len; cnt > 0; cnt >>= 1) {
        if (cnt & 1) sha512_update(&ctx, alt_result, SHA512_DIG);
        else         sha512_update(&ctx, (const uint8_t*)key, key_len);
    }
    sha512_final(&ctx, alt_result);                       /* alt_result = A */

    /* P: DP = SHA512(key repeated key_len times), then repeat the digest,
     * truncated, to fill key_len bytes. */
    sha512_reset(&alt_ctx);
    for (cnt = 0; cnt < key_len; ++cnt)
        sha512_update(&alt_ctx, (const uint8_t*)key, key_len);
    sha512_final(&alt_ctx, temp_result);                  /* temp_result = DP */
    /* P (len key_len), the digest repeated, from DP's 64-byte digest. */
    static uint8_t p_bytes[64 * 4];                       /* key_len <= 256 */
    {
        size_t p_len = key_len;
        for (cnt = 0; cnt + 64 <= key_len; cnt += 64)
            memcpy(p_bytes + cnt, temp_result, 64);
        memcpy(p_bytes + cnt, temp_result, key_len - cnt);
        (void)p_len;
    }

    /* S: DS = SHA512(salt repeated (16 + A[0]) times), repeated/truncated. */
    sha512_reset(&alt_ctx);
    for (cnt = 0; cnt < 16u + alt_result[0]; ++cnt)
        sha512_update(&alt_ctx, (const uint8_t*)salt, salt_len);
    sha512_final(&alt_ctx, temp_result);                  /* temp_result = DS */
    static uint8_t s_bytes[64];                           /* salt_len <= 16 */
    for (cnt = 0; cnt + 64 <= salt_len; cnt += 64)
        memcpy(s_bytes + cnt, temp_result, 64);
    memcpy(s_bytes + cnt, temp_result, salt_len - cnt);

    /* The 5000-round burn loop, upward 0-based. It hashes the digest-derived
     * P/S byte sequences, NOT the raw key/salt. */
    for (cnt = 0; cnt < ROUNDS_DEF; ++cnt) {
        sha512_reset(&ctx);
        if (cnt & 1) sha512_update(&ctx, p_bytes, key_len);
        else         sha512_update(&ctx, alt_result, SHA512_DIG);
        if (cnt % 3 != 0) sha512_update(&ctx, s_bytes, salt_len);
        if (cnt % 7 != 0) sha512_update(&ctx, p_bytes, key_len);
        if (cnt & 1) sha512_update(&ctx, alt_result, SHA512_DIG);
        else         sha512_update(&ctx, p_bytes, key_len);
        sha512_final(&ctx, alt_result);
    }

    /* Encode alt_result DIRECTLY (no final digest) through the fixed 21-group
     * permutation, LSB-first b64. Transcribed from the reference. */
    cp = out;
    *cp++ = '$'; *cp++ = '6'; *cp++ = '$';
    memcpy(cp, salt, salt_len); cp += salt_len;
    *cp++ = '$';

#define E(b2,b1,b0,n) cp = b64w(cp, (b2),(b1),(b0),(n))
    E(alt_result[0],  alt_result[21], alt_result[42], 4);
    E(alt_result[22], alt_result[43], alt_result[1],  4);
    E(alt_result[44], alt_result[2],  alt_result[23], 4);
    E(alt_result[3],  alt_result[24], alt_result[45], 4);
    E(alt_result[25], alt_result[46], alt_result[4],  4);
    E(alt_result[47], alt_result[5],  alt_result[26], 4);
    E(alt_result[6],  alt_result[27], alt_result[48], 4);
    E(alt_result[28], alt_result[49], alt_result[7],  4);
    E(alt_result[50], alt_result[8],  alt_result[29], 4);
    E(alt_result[9],  alt_result[30], alt_result[51], 4);
    E(alt_result[31], alt_result[52], alt_result[10], 4);
    E(alt_result[53], alt_result[11], alt_result[32], 4);
    E(alt_result[12], alt_result[33], alt_result[54], 4);
    E(alt_result[34], alt_result[55], alt_result[13], 4);
    E(alt_result[56], alt_result[14], alt_result[35], 4);
    E(alt_result[15], alt_result[36], alt_result[57], 4);
    E(alt_result[37], alt_result[58], alt_result[16], 4);
    E(alt_result[59], alt_result[17], alt_result[38], 4);
    E(alt_result[18], alt_result[39], alt_result[60], 4);
    E(alt_result[40], alt_result[61], alt_result[19], 4);
    E(alt_result[62], alt_result[20], alt_result[41], 4);
    E(0, 0, alt_result[63], 2);
#undef E
    *cp = 0;
    return out;
}
