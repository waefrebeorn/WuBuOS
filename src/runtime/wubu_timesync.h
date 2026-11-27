/*
 * wubu_timesync.h -- SNTP time correction (benchmarks #69, #220).
 *
 * "The clock is wrong" is answered in three parts: measure the offset from a
 * server (pure parse + standard formula), apply it (may need privilege, reported
 * honestly), and be able to show what was measured.
 */

#ifndef WUBU_TIMESYNC_H
#define WUBU_TIMESYNC_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int      version;        /* NTP version from the reply */
    int      stratum;        /* 1..15 sync distance; 0 is a refusal */
    int      leap_warn;      /* LI bits */
    int64_t  server_recv_us; /* T2, unix microseconds */
    int64_t  server_send_us; /* T3 */
    int64_t  offset_us;      /* ((T2-T1)+(T3-T4))/2 */
    int64_t  delay_us;       /* round trip minus server processing */
    char     refusal[8];     /* kiss-o'-death code when stratum==0 */
} WubuNtpResult;

/* Parse a 48-byte SNTP reply. 0 = ok; -1 invalid args, -2 short packet,
 * -3 not a server reply (mode != 4), -4 stratum 0 (kiss-o'-death),
 * -5 timestamp before the unix epoch. */
int wubu_ntp_parse(const unsigned char *pkt, size_t len, WubuNtpResult *out);

/* 64-bit NTP fixed point -> unix microseconds; -1 when before 1970. */
int64_t wubu_ntp_to_unix_us(uint64_t ntp);

/* The standard formulas, exposed so the arithmetic is tested directly. */
int64_t wubu_ntp_offset_us(int64_t t1, int64_t t2, int64_t t3, int64_t t4);
int64_t wubu_ntp_delay_us(int64_t t1, int64_t t2, int64_t t3, int64_t t4);

/* Query a server (dotted IPv4). Fills out->offset_us/delay_us on success. */
int wubu_ntp_query(const char *host, int timeout_ms, WubuNtpResult *out);

/* Apply an offset to CLOCK_REALTIME. -2 = no privilege (errno text in err). */
int wubu_time_apply(int64_t offset_us, char *err, size_t errsz);

int wubu_time_now_us(int64_t *out);

#endif /* WUBU_TIMESYNC_H */
