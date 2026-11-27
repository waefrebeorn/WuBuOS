/*
 * wubu_timesync.c -- ask a time server what time it is, and be able to prove it.
 *
 * WHY
 *   Benchmarks #69 and #220: the clock came from the host and nothing ever
 *   corrected it, so a machine that drifts (or boots with a dead RTC) shows the
 *   wrong time in the taskbar and in every file timestamp for the rest of the
 *   session. "The clock is wrong" is a real ticket, not a nicety.
 *
 * HOW
 *   SNTP (RFC 4330) over UDP/123: a 48-byte request, a 48-byte reply carrying
 *   four timestamps, and the standard offset formula
 *       offset = ((T2 - T1) + (T3 - T4)) / 2
 *   The packet parse, the timestamp conversion and the offset math are pure
 *   functions, so the whole thing is gate-able with a canned packet. The socket
 *   is one small transport behind a seam, exactly like the Wi-Fi client.
 *
 * WHAT IT REFUSES TO DO
 *   * Stratum 0 is a KISS-OF-DEATH ("denied", "rate exceeded"): it is reported as
 *     a refusal, never as a time.
 *   * A short packet is not "close enough": it is an error.
 *   * Applying the clock can fail without privilege, and that is reported with
 *     the real errno instead of being swallowed -- the caller still gets the
 *     measured offset either way.
 */

#include "wubu_timesync.h"

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define NTP_EPOCH_DELTA 2208988800ULL   /* 1900-01-01 -> 1970-01-01, seconds */

/* RFC 4330 request: LI=0, VN=3, Mode=3 (client) */
#define NTP_MODE_CLIENT 0x1B

static uint64_t be64(const unsigned char *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v = (v << 8) | p[i];
    return v;
}

/* 64-bit NTP fixed point -> microseconds since the UNIX epoch.
 * Bits 32..63 are seconds since 1900; bits 0..31 are the fraction. */
int64_t wubu_ntp_to_unix_us(uint64_t ntp) {
    uint64_t secs = ntp >> 32;
    uint64_t frac = ntp & 0xFFFFFFFFULL;
    if (secs < NTP_EPOCH_DELTA) return -1;          /* before 1970: nonsense for us */
    int64_t us = (int64_t)(secs - NTP_EPOCH_DELTA) * 1000000LL;
    us += (int64_t)((frac * 1000000ULL) >> 32);
    return us;
}

int wubu_ntp_parse(const unsigned char *pkt, size_t len, WubuNtpResult *out) {
    if (!pkt || !out) return -1;
    if (len < 48) return -2;                         /* short: not a reply */
    memset(out, 0, sizeof *out);

    int li   = (pkt[0] >> 6) & 0x3;
    int mode = pkt[0] & 0x7;
    out->leap_warn = li;
    out->stratum   = pkt[1];
    out->version   = (pkt[0] >> 3) & 0x7;
    if (mode != 4) return -3;                        /* not a server reply */

    /* Stratum 0 is a KISS-O'-DEATH packet: its "reference id" is a 4-char reason
     * ("RATE", "DENY"). Capture the reason BEFORE refusing, or the operator is
     * told "refused" with no idea why. */
    if (out->stratum == 0) {
        memcpy(out->refusal, pkt + 12, 4);
        out->refusal[4] = '\0';
        return -4;
    }

    int64_t t2 = wubu_ntp_to_unix_us(be64(pkt + 32));  /* server receive */
    int64_t t3 = wubu_ntp_to_unix_us(be64(pkt + 40));  /* server transmit */
    if (t2 < 0 || t3 < 0) return -5;
    out->server_recv_us = t2;
    out->server_send_us = t3;
    return 0;
}

int64_t wubu_ntp_offset_us(int64_t t1, int64_t t2, int64_t t3, int64_t t4) {
    /* ((T2 - T1) + (T3 - T4)) / 2 -- the round-trip cancels when the path is
     * symmetric; when it is not, this is still the standard estimate. */
    return ((t2 - t1) + (t3 - t4)) / 2;
}

int64_t wubu_ntp_delay_us(int64_t t1, int64_t t2, int64_t t3, int64_t t4) {
    (void)t2; (void)t3;
    return (t4 - t1) - (t3 - t2);
}

/* -- transport ----------------------------------------------------- */

int wubu_time_now_us(int64_t *out) {
    struct timespec ts;
    if (!out || clock_gettime(CLOCK_REALTIME, &ts) != 0) return -1;
    *out = (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
    return 0;
}

int wubu_ntp_query(const char *host, int timeout_ms, WubuNtpResult *out) {
    if (!host || !out) return -1;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(123);
    if (inet_pton(AF_INET, host, &a.sin_addr) != 1) { close(fd); return -2; }

    unsigned char req[48];
    memset(req, 0, sizeof req);
    req[0] = NTP_MODE_CLIENT;

    int64_t t1 = 0, t4 = 0;
    wubu_time_now_us(&t1);
    if (sendto(fd, req, sizeof req, 0, (struct sockaddr *)&a, sizeof a) != (ssize_t)sizeof req) {
        close(fd); return -3;
    }
    unsigned char rep[64];
    ssize_t n = recv(fd, rep, sizeof rep, 0);
    wubu_time_now_us(&t4);
    close(fd);
    if (n <= 0) return -4;

    int rc = wubu_ntp_parse(rep, (size_t)n, out);
    if (rc != 0) return rc;
    out->offset_us = wubu_ntp_offset_us(t1, out->server_recv_us, out->server_send_us, t4);
    out->delay_us  = wubu_ntp_delay_us(t1, out->server_recv_us, out->server_send_us, t4);
    return 0;
}

int wubu_time_apply(int64_t offset_us, char *err, size_t errsz) {
    int64_t now = 0;
    if (err && errsz) err[0] = '\0';
    if (wubu_time_now_us(&now) != 0) return -1;
    struct timespec ts;
    int64_t target = now + offset_us;
    ts.tv_sec  = (time_t)(target / 1000000LL);
    ts.tv_nsec = (long)((target % 1000000LL) * 1000);
    if (clock_settime(CLOCK_REALTIME, &ts) != 0) {
        if (err && errsz) snprintf(err, errsz, "%s", strerror(errno));
        return -2;                       /* no privilege: the offset still stands */
    }
    return 0;
}
