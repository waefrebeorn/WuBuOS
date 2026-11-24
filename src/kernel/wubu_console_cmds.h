/*
 * wubu_console_cmds.h -- declarations for the console's built-in commands
 * (defined in wubu_console_cmds.c, dispatched by wubu_console_exec).
 */
#ifndef WUBU_CONSOLE_CMDS_H
#define WUBU_CONSOLE_CMDS_H

int cmd_help(void);
int cmd_uptime(void);
int cmd_mem(void);
int cmd_tasks(void);
int cmd_pci(void);
int cmd_drv(void);
int cmd_theme(int argc, char **argv);
int cmd_input(int argc, char **argv);
int cmd_vmm(int argc, char **argv);
int cmd_stats(int argc, char **argv);
int cmd_dump(int argc, char **argv);
int cmd_attest(int argc, char **argv);
int cmd_date(int argc, char **argv);
int cmd_agi(int argc, char **argv);
int cmd_holyc(int argc, char **argv);
int cmd_fs(int argc, char **argv);
int cmd_kv(int argc, char **argv);
/* The 9P server's own proof: the kernel client walks /n/kv in-kernel. */
int cmd_kv9p(int argc, char **argv);
/* Report the real back buffer: size, non-black pixels, coarse colour classes
 * and an FNV hash. The render gates assert on this line. */
int cmd_deskshot(int argc, char **argv);
int cmd_model(int argc, char **argv);

#endif /* WUBU_CONSOLE_CMDS_H */
