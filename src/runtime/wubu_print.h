/*
 * wubu_print.h -- printer discovery and job submission (CUPS), nothing more.
 */
#ifndef WUBU_PRINT_H
#define WUBU_PRINT_H

#include <stddef.h>

#define WUBU_PRINTER_NAME_MAX 96
#define WUBU_PRINTER_URI_MAX  256

typedef struct {
    char name[WUBU_PRINTER_NAME_MAX];
    char uri[WUBU_PRINTER_URI_MAX];
    char transport[32];                 /* usb, ipp, socket, file ... */
    char state[32];                     /* as CUPS prints it: Idle, Disabled, Processing */
    int  accepting;
    int  is_default;
} WubuPrinter;

typedef enum {
    WUBU_PRINT_SUBMITTED = 0,
    WUBU_PRINT_NO_PRINTER,
    WUBU_PRINT_DENIED,
    WUBU_PRINT_REFUSED,
    WUBU_PRINT_ABSENT
} WubuPrintResult;

typedef struct {
    char job_id[96];
    char message[256];                  /* the spooler's own words when it says no */
    int  copies;
} WubuPrintOut;

const char *wubu_print_conf_path(void);
int  wubu_print_enum(WubuPrinter *out, int cap);   /* count, or -1 when there is no CUPS */
WubuPrintResult wubu_print_submit(const char *printer, const char *path,
                                  int copies, WubuPrintOut *out);
const char *wubu_print_result_name(WubuPrintResult r);

#endif /* WUBU_PRINT_H */
