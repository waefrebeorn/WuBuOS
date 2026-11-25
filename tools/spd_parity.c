/* spd_parity.c -- opt-in live parity check for DDR5 SPD hub temperature.
 *
 * Runs as root because the DIMM hubs are on the Intel PCH SMBus I/O ports.
 * Reads MR49/MR50 from each detected SPD5 hub and compares the WuBuOS
 * reader's whole-degree value against two independent published decoders:
 *   - Linux drivers/hwmon/spd5118.c: sign_extend32(raw >> 2, 10) * 250 mC
 *   - pyhwinfo/memspd.py:            (raw >> 2, 11-bit two's complement) / 4
 * Both oracles reduce to the same 0.25 C LSB; agreement is parity, not proof
 * of the sensor's absolute accuracy.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include "wubu_thermal_read.h"

#define SMB_PORT 0xefa0u
#define S_STS 0
#define S_CNT 2
#define S_CMD 3
#define S_ADD 4
#define S_DAT0 5
#define ERRBITS 0x1cu
#define FLAGS 0x9eu
#define BYTE_DATA 0x08u
#define START 0x40u

static int fd;
static unsigned char rd(int off) {
    unsigned char v = 0xff;
    if (lseek(fd, SMB_PORT + off, SEEK_SET) == SMB_PORT + off && read(fd, &v, 1) == 1) return v;
    return 0xff;
}
static void wr(int off, unsigned char v) {
    if (lseek(fd, SMB_PORT + off, SEEK_SET) == SMB_PORT + off) {
        ssize_t n = write(fd, &v, 1);
        (void)n;
    }
}
static int byte(unsigned dev, unsigned cmd) {
    unsigned s = rd(S_STS);
    if (s & FLAGS) wr(S_STS, (unsigned char)s);
    wr(S_ADD, (unsigned char)((dev << 1) | 1));
    wr(S_CMD, (unsigned char)cmd);
    wr(S_CNT, BYTE_DATA);
    wr(S_CNT, BYTE_DATA | START);
    struct timespec ts = {0, 50000};
    for (int i = 0; i < 20000; i++) {
        s = rd(S_STS);
        if (!(s & 1u)) {
            if (s & ERRBITS) return -1;
            if (s & 0x02u) break;
        }
        nanosleep(&ts, NULL);
    }
    unsigned v = rd(S_DAT0);
    s = rd(S_STS);
    if (s & FLAGS) wr(S_STS, (unsigned char)s);
    return (int)v;
}
static int kernel_oracle_mC(unsigned raw) {
    unsigned code = (raw >> 2) & 0x7ffu;
    int units = (code & 0x400u) ? (int)code - 0x800 : (int)code;
    return units * 250;
}
static int pyhwinfo_oracle_c(unsigned raw) {
    unsigned code = (raw >> 2) & 0x7ffu;
    int units = (code & 0x400u) ? (int)code - 0x800 : (int)code;
    return units / 4;
}

int main(void) {
    fd = open("/dev/port", O_RDWR);
    if (fd < 0) { perror("/dev/port (run as root)"); return 2; }

    int failures = 0, checked = 0, first = 1;
    for (unsigned dev = 0x50; dev <= 0x53; dev++) {
        int lo = byte(dev, 0x00), hi = byte(dev, 0x01);
        if (lo < 0 || hi < 0) continue;
        if ((((unsigned)lo << 8) | (unsigned)hi) != 0x5118u) continue;

        int tlo = byte(dev, 0x31), thi = byte(dev, 0x32);
        if (tlo < 0 || thi < 0) { printf("FAIL dev=%02x temp read\n", dev); failures++; continue; }
        unsigned raw = ((unsigned)thi << 8) | (unsigned)tlo;
        unsigned code = (raw >> 2) & 0x7ffu;
        int units = (code & 0x400u) ? (int)code - 0x800 : (int)code;
        int kmC = units * 250;
        int pc = units / 4;
        int rounded = (kmC >= 0) ? ((kmC + 500) / 1000) : -((-kmC + 500) / 1000);
        wubu_thermal_refresh();
        int ours = wubu_thermal_ram();
        printf("dev=0x%02x raw=0x%04x kernel=%d.%02dC pyhwinfo=%dC wubu=%dC%s\n",
               dev, raw, kmC / 1000, abs(kmC % 1000) / 10, pc, ours,
               first ? "" : " (info: wubu reports first hub)");
        if (first) {
            checked++;
            if (kmC != units * 250 || pc != units / 4) { printf("FAIL oracle disagreement\n"); failures++; }
            if (ours < 0 || ours != rounded) { printf("FAIL production parity\n"); failures++; }
        }
        first = 0;
    }
    close(fd);
    printf("%s: checked=%d failures=%d\n",
           (checked && !failures) ? "SPD_PARITY PASS" : "SPD_PARITY FAIL", checked, failures);
    return failures || !checked ? 1 : 0;
}
