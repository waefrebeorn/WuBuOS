/*
 * wubu_runline.c -- see header.
 */

#include "wubu_runline.h"
#include "../apps/notepad.h"
#include "../apps/guest_file.h"
#include "../runtime/wubu_spawn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RUNLINE_MAX_CMD 512
#define RUNLINE_MAX_OUT GUEST_FILE_MAX

int wubu_runline_execute(const char *command_line, const char *result_file)
{
    if (!command_line || !*command_line) return -1;
    if (!result_file || !*result_file) return -1;

    char cmd[RUNLINE_MAX_CMD];
    snprintf(cmd, sizeof(cmd), "%s 2>&1", command_line);

    char *argv[4];
    argv[0] = "/bin/sh";
    argv[1] = "-c";
    argv[2] = cmd;
    argv[3] = NULL;

    char *out = wubu_popen_read(argv[0], argv);
    size_t out_len = out ? strlen(out) : 0;

    /* Build the report.  The guest file has a hard ceiling, so overflow is
     * marked explicitly instead of being cut where nobody would notice. */
    char body[RUNLINE_MAX_OUT];
    int n = snprintf(body, sizeof(body),
                     "$ %s\n"
                     "----------------------------------------\n",
                     command_line);
    if (n < 0) n = 0;

    if (!out) {
        n += snprintf(body + n, sizeof(body) - (size_t)n,
                      "(could not start the command)\n");
    } else if (out_len == 0) {
        n += snprintf(body + n, sizeof(body) - (size_t)n, "(no output)\n");
    } else if ((size_t)n + out_len + 64 <= sizeof(body)) {
        memcpy(body + n, out, out_len);
        n += (int)out_len;
        if (body[n - 1] != '\n') body[n++] = '\n';
    } else {
        size_t room = sizeof(body) - (size_t)n - 64;
        memcpy(body + n, out, room);
        n += (int)room;
        n += snprintf(body + n, sizeof(body) - (size_t)n,
                      "\n... truncated: %zu bytes of output, %zu bytes shown\n",
                      out_len, room);
    }
    free(out);

    if (guest_file_write(result_file, (const uint8_t *)body, (uint32_t)n) != 0) {
        fprintf(stderr, "WuBuOS: could not save command output to %s\n",
                result_file);
        return -1;
    }
    guest_file_flush();         /* the result should outlive the session */
    notepad_open(result_file);
    return 0;
}
