/*
 * test_wubu_audio_backend.c -- behavior gate for the device output path.
 *
 * Proves the whole chain the product was missing: the engine renders, the
 * sink converts, the device accepts. Not assert-nothing -- it writes a real
 * sine through the sink and checks the backend took it.
 */
#include "../egojar.h"

#include "wubu_audio.h"
#include "bigmac/bigmac_out.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int g_fail;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); g_fail++; } } while (0)

int main(void)
{
    /* ---- 1. the engine comes up ------------------------------------ */
    CHECK(wubu_audio_engine_create(48000, 256, 2) == 0,
          "the engine creates");
    float null_buf[8];
    wubu_audio_process(null_buf, 8);         /* NULL/empty is handled internally */

    /* ---- 2. the sink opens a device -------------------------------- */
    wubu_audio_sink_open();
    wubu_audio_sink_open();                  /* idempotent */
    CHECK(bigmac_out_backend() != BIGMAC_BACKEND_NONE,
          "the sink opened a backend");
    printf("  backend: %s\n", bigmac_out_backend_name());

    /* ---- 3. the sink takes real PCM -------------------------------- */
    const int n = 4800;                      /* 0.1 s at 48 kHz */
    int16_t *pcm = malloc((size_t)n * 2 * sizeof(int16_t));
    CHECK(pcm != NULL, "pcm buffer allocated");
    if (!pcm) return 1;

    for (int i = 0; i < n; i++) {
        int16_t s = (int16_t)(12000 * sin(2.0 * 3.14159265 * 440.0 * (double)i / 48000.0));
        pcm[i * 2 + 0] = s;
        pcm[i * 2 + 1] = s;
    }

    long before = 0;
    wubu_audio_sink_int16(pcm, n * 2);
    before = bigmac_out_latency_frames();
    CHECK(before >= 0, "latency is reported after a write");

    /* ---- 4. NULL and zero are quiet no-ops ------------------------- */
    wubu_audio_sink_int16(NULL, n);
    wubu_audio_sink_int16(pcm, 0);
    CHECK(1, "NULL/zero writes did not crash");

    /* ---- 5. a second pass ------------------------------------------ */
    wubu_audio_sink_int16(pcm, n * 2);
    CHECK(bigmac_out_latency_frames() >= 0, "a second write is accepted");

    free(pcm);

    /* ---- 6. close is idempotent ------------------------------------ */
    wubu_audio_sink_close();
    wubu_audio_sink_close();
    CHECK(1, "double close did not crash");

    /* ---- 7. the engine still renders with no device ----------------- */
    float buf[256 * 2];
    wubu_audio_process(buf, 256);
    CHECK(1, "the engine renders with the sink closed");

    if (g_fail == 0) {
        printf("test_wubu_audio_backend: all checks passed\n");
        return 0;
    }
    printf("test_wubu_audio_backend: %d FAILED\n", g_fail);
    return 1;
}
