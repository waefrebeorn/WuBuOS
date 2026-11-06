/*
 * wubu_modsym.c -- runtime-extensible symbol registry for the PE loader.
 * Kernel-clean: no libc beyond libc_string (memcpy/strcasecmp/strcmp), no
 * malloc, no threads. A fixed array of {dll,name,addr}; dll is uppercased so a
 * later case-insensitive lookup (and the resolver) matches any caller's case.
 */
#include "wubu_modsym.h"
#include <string.h>

/* self-contained uppercase/lowercase helpers (kernel-clean: no strings.h).
 * toupper/isspace exist in the kernel's libc_string too. */
static int wm_toupper(int c) {
    return (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c;
}
static int wm_ci_equal(const char *a, const char *b) {
    unsigned char ca, cb;
    do {
        ca = (unsigned char)wm_toupper((unsigned char)*a++);
        cb = (unsigned char)wm_toupper((unsigned char)*b++);
        if (ca != cb) break;
    } while (ca != 0);
    return ca == cb;
}

static wubu_modsym_entry_t g_modsym[WUBU_MODSYM_MAX];
static int g_modsym_n = 0;

/* uppercases in place and returns len (for boundary check upstream). */
static int upper_inplace(char *s, int n) {
    for (int i = 0; i < n && s[i]; i++) s[i] = (char)wm_toupper((unsigned char)s[i]);
    return (int)strlen(s);
}

int wubu_modsym_register(const char *dll, const char *name, uintptr_t addr) {
    if (!dll || !name) return -1;
    int dn = (int)strlen(dll), nn = (int)strlen(name);
    if (dn <= 0 || dn > 62 || nn <= 0 || nn > 126) return -1;

    char d_up[64];
    memcpy(d_up, dll, (size_t)dn);
    d_up[dn] = '\0';
    upper_inplace(d_up, dn);

    /* overwrite existing {dll,name} slot in place (rebind/hot-swap) */
    for (int i = 0; i < g_modsym_n; i++) {
        if (g_modsym[i].present &&
            strcmp(g_modsym[i].dll, d_up) == 0 &&
            strcmp(g_modsym[i].name, name) == 0) {
            g_modsym[i].addr = addr;
            return 0;
        }
    }
    if (g_modsym_n >= WUBU_MODSYM_MAX) return -1;
    memcpy(g_modsym[g_modsym_n].dll, d_up, (size_t)dn);
    g_modsym[g_modsym_n].dll[dn] = '\0';
    memcpy(g_modsym[g_modsym_n].name, name, (size_t)nn);
    g_modsym[g_modsym_n].name[nn] = '\0';
    g_modsym[g_modsym_n].addr = addr;
    g_modsym[g_modsym_n].present = 1;
    g_modsym_n++;
    return 0;
}

int wubu_modsym_lookup(const char *dll, const char *name, uintptr_t *out) {
    if (!dll || !name || !out) return 1;
    for (int i = 0; i < g_modsym_n; i++) {
        if (g_modsym[i].present &&
            wm_ci_equal(g_modsym[i].dll, dll) &&
            strcmp(g_modsym[i].name, name) == 0) {
            *out = g_modsym[i].addr;
            return 0;
        }
    }
    return 1;
}

int wubu_modsym_count(void) { return g_modsym_n; }

void wubu_modsym_clear(void) {
    memset(g_modsym, 0, sizeof(g_modsym));
    g_modsym_n = 0;
}

int wubu_modsym_resolve(const char *dll, const char *func,
                        uint32_t ordinal, int is_ordinal,
                        uintptr_t *out) {
    (void)ordinal;
    /* ordinal imports not supported (name-keyed registry); PE gens use names */
    if (is_ordinal) return 1;
    return wubu_modsym_lookup(dll, func, out);
}
