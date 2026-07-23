/*
 * linux_era_demo.c -- Linux native era demo (2007)
 *
 * A real Linux ELF. It writes a marker file (proving the VSL Linux
 * fileio personality is reachable) and prints a banner.
 *
 * Runs INSIDE WuBuOS through the Linux personality:
 *   wubu_exec_linux_elf() -> native (bwrap) container -> exec.
 *
 * Build:
 *   gcc -O2 -static linux_era_demo.c -o linux_era_demo.elf
 *   (static so it runs inside the minimal bwrap root without libc shims)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int main(void) {
    printf("WuBuOS Linux era: hello from a native ELF through VSL\n");

    int fd = open("WUBU_ERA_LINUX.OK", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const char *msg = "Linux (VSL) personality exercised by WuBuOS\n";
        (void)write(fd, msg, (int)strlen(msg));
        close(fd);
    } else {
        perror("open");
    }
    return 0;
}
