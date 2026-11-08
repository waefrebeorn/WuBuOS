/* mk_win_demo.c — real Win32 era-app: writes WUBU_ERA_WIN.OK in CWD and
 * exits 0, proving wubu_exec_win_pe ran the PE through Wine/NT. */
#include <stdio.h>
int main(void) {
    FILE *f = fopen("WUBU_ERA_WIN.OK", "w");
    if (!f) return 1;
    fputs("win era demo ran via Wine/NT\n", f);
    fclose(f);
    return 0;
}
