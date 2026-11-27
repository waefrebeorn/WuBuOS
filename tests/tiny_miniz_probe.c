/* Probe: the vendored miniz actually compresses and inflates.
 * This is the acceptance gate the upstreams table promised for the `archive`
 * gap: a ZIP container and a PNG both begin with a raw deflate stream, so a
 * round-trip through miniz is the mechanism behind archive browsing and
 * DOCX/ODT reading -- not a compile check. */
#include "../third_party/tiny/miniz/miniz.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *msg = "WuBuOS: deflate round-trip proves the archive leg. ";
    char src[1024], comp[2048], back[1024];
    size_t n = 0;
    while (msg[n] && n < sizeof src - 1) { memcpy(src + n, msg, strlen(msg)); n += strlen(msg); }
    src[n] = '\0';

    mz_ulong clen = sizeof comp;
    int ok = mz_compress((unsigned char *)comp, &clen, (const unsigned char *)src, (mz_ulong)n);
    if (ok != MZ_OK) { printf("[FAIL] compress rc=%d\n", ok); return 1; }
    mz_ulong blen = sizeof back;
    ok = mz_uncompress((unsigned char *)back, &blen, (const unsigned char *)comp, clen);
    if (ok != MZ_OK) { printf("[FAIL] uncompress rc=%d\n", ok); return 1; }
    back[blen] = '\0';
    if (blen != n || memcmp(src, back, n) != 0) { printf("[FAIL] round-trip mismatch\n"); return 1; }
    printf("[PASS] miniz: %zu bytes -> %lu compressed -> %lu back, identical\n", n, (unsigned long)clen, (unsigned long)blen);

    /* negative control: a corrupted stream must be REFUSED, not silently accepted */
    comp[clen / 2] ^= 0xFF;
    mz_ulong blen2 = sizeof back;
    ok = mz_uncompress((unsigned char *)back, &blen2, (const unsigned char *)comp, clen);
    if (ok == MZ_OK) { printf("[FAIL] corrupted deflate stream decompressed (fail-open)\n"); return 1; }
    printf("[PASS] miniz: corrupted stream refused rc=%d\n", ok);
    return 0;
}
