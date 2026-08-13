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
int cmd_theme(int argc, char **argv);
int cmd_input(int argc, char **argv);
int cmd_vmm(int argc, char **argv);
int cmd_stats(int argc, char **argv);
int cmd_dump(int argc, char **argv);
int cmd_attest(int argc, char **argv);
int cmd_date(int argc, char **argv);
int cmd_agi(int argc, char **argv);
int cmd_holyc(int argc, char **argv);

#endif /* WUBU_CONSOLE_CMDS_H */
