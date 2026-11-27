/* wubu_x25519.c -- see wubu_x25519.h. RFC 7748 field arithmetic in five 51-bit limbs.
 *
 * The Montgomery ladder runs 255 iterations and swaps with a mask, so no branch
 * depends on the secret scalar. Field elements carry in __int128 and reduce with the
 * identity 2^255 = 19 (mod 2^255-19).
 */
#include "wubu_x25519.h"
#include <stddef.h>
#include <string.h>

typedef struct { uint64_t v[5]; } Fe;
typedef __int128 i128;
#define M51 0x7FFFFFFFFFFFFULL
static void fe_0(Fe *r){ memset(r,0,sizeof *r);} 
static void fe_1(Fe *r){ fe_0(r); r->v[0]=1; }
static void fe_copy(Fe *r,const Fe *a){ memcpy(r,a,sizeof *r); }
static void fe_add(Fe *r,const Fe *a,const Fe *b){ for(int i=0;i<5;i++) r->v[i]=a->v[i]+b->v[i]; }
static void fe_sub(Fe *r,const Fe *a,const Fe *b){
    static const uint64_t two_p0 = 0xFFFFFFFFFFFDAULL, two_pn = 0xFFFFFFFFFFFFEULL;
    r->v[0]=a->v[0]+two_p0-b->v[0];
    for(int i=1;i<5;i++) r->v[i]=a->v[i]+two_pn-b->v[i];
}
static void fe_mul(Fe *r,const Fe *a,const Fe *b){
    const uint64_t *x=a->v,*y=b->v;
    i128 t0=(i128)x[0]*y[0]+(i128)19*((i128)x[1]*y[4]+(i128)x[2]*y[3]+(i128)x[3]*y[2]+(i128)x[4]*y[1]);
    i128 t1=(i128)x[0]*y[1]+(i128)x[1]*y[0]+(i128)19*((i128)x[2]*y[4]+(i128)x[3]*y[3]+(i128)x[4]*y[2]);
    i128 t2=(i128)x[0]*y[2]+(i128)x[1]*y[1]+(i128)x[2]*y[0]+(i128)19*((i128)x[3]*y[4]+(i128)x[4]*y[3]);
    i128 t3=(i128)x[0]*y[3]+(i128)x[1]*y[2]+(i128)x[2]*y[1]+(i128)x[3]*y[0]+(i128)19*((i128)x[4]*y[4]);
    i128 t4=(i128)x[0]*y[4]+(i128)x[1]*y[3]+(i128)x[2]*y[2]+(i128)x[3]*y[1]+(i128)x[4]*y[0];
    uint64_t c;
    /* Shift the 128-bit product BEFORE narrowing: (uint64_t)t >> 51 throws away every
     * carry above bit 63, which silently zeroed any product past 2^63 (2^200 * 2^50
     * became 0 and the ladder produced a public key that matched nothing). */
    uint64_t r0=(uint64_t)(t0 & M51); t1 += (i128)(t0 >> 51);
    uint64_t r1=(uint64_t)(t1 & M51); t2 += (i128)(t1 >> 51);
    uint64_t r2=(uint64_t)(t2 & M51); t3 += (i128)(t2 >> 51);
    uint64_t r3=(uint64_t)(t3 & M51); t4 += (i128)(t3 >> 51);
    uint64_t r4=(uint64_t)(t4 & M51); c=(uint64_t)(t4 >> 51);
    r0 += c*19; c = r0 >> 51; r0 &= M51; r1 += c;
    r->v[0]=r0;r->v[1]=r1;r->v[2]=r2;r->v[3]=r3;r->v[4]=r4;
}
static void fe_sq(Fe *r,const Fe *a){ fe_mul(r,a,a); }
static void fe_pow(Fe *r,const Fe *a,const uint8_t *e,int nbits){
    Fe base, acc; fe_copy(&base,a); fe_1(&acc);
    for(int i=nbits-1;i>=0;i--){ fe_sq(&acc,&acc); if((e[i/8]>>(i%8))&1) fe_mul(&acc,&acc,&base); }
    fe_copy(r,&acc);
}
static void fe_invert(Fe *r,const Fe *a){
    /* p-2 = 2^255 - 21 */
    uint8_t e[32]; memset(e,0,sizeof e);
    for(int i=0;i<32;i++) e[i]=0xFF;
    e[0]=0xEB; e[31]=0x7F;
    fe_pow(r,a,e,255);
}
static void fe_frombytes(Fe *r,const uint8_t s[32]){
    r->v[0]=((uint64_t)s[0]|((uint64_t)s[1]<<8)|((uint64_t)s[2]<<16)|((uint64_t)s[3]<<24)|((uint64_t)s[4]<<32)|((uint64_t)s[5]<<40)|((uint64_t)s[6]<<48)) & M51;
    r->v[1]=(((uint64_t)s[6]>>3)|((uint64_t)s[7]<<5)|((uint64_t)s[8]<<13)|((uint64_t)s[9]<<21)|((uint64_t)s[10]<<29)|((uint64_t)s[11]<<37)|((uint64_t)s[12]<<45)) & M51;
    r->v[2]=(((uint64_t)s[12]>>6)|((uint64_t)s[13]<<2)|((uint64_t)s[14]<<10)|((uint64_t)s[15]<<18)|((uint64_t)s[16]<<26)|((uint64_t)s[17]<<34)|((uint64_t)s[18]<<42)|((uint64_t)s[19]<<50)) & M51;
    r->v[3]=(((uint64_t)s[19]>>1)|((uint64_t)s[20]<<7)|((uint64_t)s[21]<<15)|((uint64_t)s[22]<<23)|((uint64_t)s[23]<<31)|((uint64_t)s[24]<<39)|((uint64_t)s[25]<<47)) & M51;
    r->v[4]=(((uint64_t)s[25]>>4)|((uint64_t)s[26]<<4)|((uint64_t)s[27]<<12)|((uint64_t)s[28]<<20)|((uint64_t)s[29]<<28)|((uint64_t)s[30]<<36)|((uint64_t)s[31]<<44)) & M51;
}
static void fe_tobytes(uint8_t s[32],const Fe *h){
    uint64_t t0=h->v[0],t1=h->v[1],t2=h->v[2],t3=h->v[3],t4=h->v[4],q,c;
    q=(t0+19)>>51; q=(t1+q)>>51; q=(t2+q)>>51; q=(t3+q)>>51; q=(t4+q)>>51;
    t0+=19*q; c=t0>>51; t0&=M51; t1+=c; c=t1>>51; t1&=M51; t2+=c; c=t2>>51; t2&=M51; t3+=c; c=t3>>51; t3&=M51; t4+=c; t4&=M51;
    uint64_t w0=t0|(t1<<51), w1=(t1>>13)|(t2<<38), w2=(t2>>26)|(t3<<25), w3=(t3>>39)|(t4<<12);
    for(int i=0;i<8;i++) s[i]=(uint8_t)(w0>>(8*i));
    for(int i=0;i<8;i++) s[8+i]=(uint8_t)(w1>>(8*i));
    for(int i=0;i<8;i++) s[16+i]=(uint8_t)(w2>>(8*i));
    for(int i=0;i<8;i++) s[24+i]=(uint8_t)(w3>>(8*i));
}
static void cswap(uint64_t swap,Fe *a,Fe *b){
    uint64_t mask = 0 - swap;
    for(int i=0;i<5;i++){ uint64_t t=mask&(a->v[i]^b->v[i]); a->v[i]^=t; b->v[i]^=t; }
}
static int is_zero(const uint8_t *p, size_t n) {
    uint8_t d = 0;
    for (size_t i = 0; i < n; i++) d |= p[i];
    return d == 0;
}

int wubu_x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32]) {
    if (!out || !scalar || !point) return WUBU_X25519_ARGS;
    uint8_t k[32]; memcpy(k,scalar,32); k[0]&=248; k[31]&=127; k[31]|=64;
    Fe x1,x2,z2,x3,z3; fe_frombytes(&x1,point);
    fe_1(&x2); fe_0(&z2); fe_copy(&x3,&x1); fe_1(&z3);
    uint64_t swap=0;
    for(int t=254;t>=0;t--){
        uint64_t kt=(k[t>>3]>>(t&7))&1;
        swap^=kt; cswap(swap,&x2,&x3); cswap(swap,&z2,&z3); swap=kt;
        Fe A,AA,B,BB,E,C,D,DA,CB,t; 
        fe_add(&A,&x2,&z2); fe_sq(&AA,&A);
        fe_sub(&B,&x2,&z2); fe_sq(&BB,&B);
        fe_sub(&E,&AA,&BB);
        fe_add(&C,&x3,&z3); fe_sub(&D,&x3,&z3);
        fe_mul(&DA,&D,&A); fe_mul(&CB,&C,&B);
        fe_add(&t,&DA,&CB); fe_sq(&x3,&t);
        fe_sub(&t,&DA,&CB); fe_sq(&t,&t); fe_mul(&z3,&x1,&t);
        fe_mul(&x2,&AA,&BB);
        /* z2 = E * (AA + a24*E), with a24 = 121665: the scalar multiplies E, it does
         * not multiply the product E*AA -- that mistake produced a plausible-looking
         * public key that matched nothing. */
        Fe a24; fe_0(&a24); a24.v[0]=121665;
        Fe a24e; fe_mul(&a24e,&a24,&E);
        fe_add(&t,&AA,&a24e);
        fe_mul(&z2,&E,&t);
    }
    cswap(swap,&x2,&x3); cswap(swap,&z2,&z3);
    Fe inv; fe_invert(&inv,&z2);
    Fe res; fe_mul(&res,&x2,&inv);
    fe_tobytes(out,&res);
    memset(&k, 0, sizeof k);
    memset(&res, 0, sizeof res);
    memset(&inv, 0, sizeof inv);
    if (is_zero(out, 32)) {          /* low-order peer point: no usable secret exists */
        memset(out, 0, 32);
        return WUBU_X25519_LOW_ORDER;
    }
    return WUBU_X25519_OK;
}

const char *wubu_x25519_reason(int err) {
    switch (err) {
        case WUBU_X25519_OK:        return "ok";
        case WUBU_X25519_ARGS:      return "bad arguments";
        case WUBU_X25519_LOW_ORDER: return "peer sent a low-order point";
        default:                    return "invalid";
    }
}
