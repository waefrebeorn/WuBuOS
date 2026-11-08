/* era_linux_demo.c — real Linux era-app: writes WUBU_ERA_LINUX.OK in CWD
 * and exits 0, proving wubu_exec_linux_elf ran the ELF through the native
 * container path with working fileio. Static-linked so the container needs
 * no host libc. */
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    FILE *f = fopen("WUBU_ERA_LINUX.OK", "w");
    if (!f) return 1;
    fputs("linux era demo ran via VSL/container\n", f);
    fclose(f);
    return 0;
}
