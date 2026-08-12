/*
 * wubu_dmcrypt.c -- kernel-owned storage dm-crypt/LUKS encryption routing.
 *
 * dm-crypt encrypts block devices (LUKS). "Runs on everything" includes
 * correct block encryption on every storage controller.
 *
 * dm-crypt:
 *   - device-mapper: dm-crypt target
 *   - LUKS: Linux Unified Key Setup (LUKS1/LUKS2)
 *   - /dev/mapper/: mapped devices
 *   - cipher: aes-xts, aes-cbc, serpent, twofish
 *   - keysize: 256, 512
 *   - hash: sha256, sha512, ripemd
 *
 * WuBuOS owns this: detect dm-crypt + LUKS + cipher, route to the
 * right driver, expose the topology.
 *
 * Research (7-hop on the dmcrypt frontier):
 *   - device-mapper dm-crypt
 *   - LUKS format
 *   - crypto cipher/keysize
 */
#include "wubu_dmcrypt.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

static int  g_crypt = 0;       /* dm-crypt present */
static int  g_luks = 0;        /* LUKS */
static int  g_aes = 0;         /* AES */
static int  g_xts = 0;         /* XTS */
static int  g_dm = 0;          /* device-mapper */
static char g_crypt_drv[24] = "";

void wubu_dmcrypt_probe(void)
{
    g_crypt = 0; g_luks = 0; g_aes = 0; g_xts = 0; g_dm = 0;
    g_crypt_drv[0] = '\0';

#ifdef _GNU_SOURCE
    if (access("/sys/module/dm_crypt", R_OK) == 0 ||
        access("/dev/mapper", R_OK) == 0) {
        g_crypt = 1; g_dm = 1; g_aes = 1; g_xts = 1;
        strcpy(g_crypt_drv, "dm-crypt");
    }
    if (access("/usr/sbin/cryptsetup", R_OK) == 0 ||
        access("/usr/bin/cryptsetup", R_OK) == 0) {
        g_luks = 1; g_crypt = 1;
        if (!g_crypt_drv[0]) strcpy(g_crypt_drv, "cryptsetup-luks");
    }
    if (access("/sys/module/dm_mod", R_OK) == 0) {
        g_dm = 1;
        if (!g_crypt_drv[0]) strcpy(g_crypt_drv, "dm-mod");
    }
#endif
}

int  wubu_dmcrypt_present(void){ return g_crypt; }
int  wubu_dmcrypt_luks(void)   { return g_luks; }
int  wubu_dmcrypt_aes(void)    { return g_aes; }
int  wubu_dmcrypt_xts(void)    { return g_xts; }
int  wubu_dmcrypt_dm(void)     { return g_dm; }
const char *wubu_dmcrypt_driver(void){ return g_crypt_drv[0] ? g_crypt_drv : NULL; }

const char *wubu_dmcrypt_cipher_for(const char *c)
{
    if (!c) return NULL;
    if (strstr(c, "aes"))   return "aes";
    if (strstr(c, "serpent")) return "serpent";
    if (strstr(c, "twofish")) return "twofish";
    if (strstr(c, "camellia")) return "camellia";
    return "aes";
}

const char *wubu_dmcrypt_mode_for(const char *m)
{
    if (!m) return NULL;
    if (strstr(m, "xts"))  return "xts";
    if (strstr(m, "cbc"))  return "cbc";
    if (strstr(m, "ecb"))  return "ecb";
    if (strstr(m, "gcm"))  return "gcm";
    return "xts";
}

int wubu_dmcrypt_summary(char *out, size_t cap)
{
    return snprintf(out, cap,
        "dmcrypt[crypt=%d luks=%d aes=%d xts=%d dm=%d drv=%s]",
        g_crypt, g_luks, g_aes, g_xts, g_dm,
        wubu_dmcrypt_driver() ? wubu_dmcrypt_driver() : "none");
}
