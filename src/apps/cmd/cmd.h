#ifndef WUBU_CMD_H
#define WUBU_CMD_H

#include <stdint.h>
#include <stdbool.h>

/* Opaque CMD terminal state */
typedef struct WubuCmd WubuCmd;

/* Initialize CMD terminal */
WubuCmd* wubu_cmd_create(int cols, int rows);

/* Destroy CMD terminal */
void wubu_cmd_destroy(WubuCmd *cmd);

/* Input handling */
void wubu_cmd_key(WubuCmd *cmd, int key);
void wubu_cmd_text(WubuCmd *cmd, const char *text);
void wubu_cmd_resize(WubuCmd *cmd, int cols, int rows);

/* PTY operations */
int wubu_cmd_spawn_shell(WubuCmd *cmd, const char *shell_path);
void wubu_cmd_write_pty(WubuCmd *cmd, const char *data, int len);
int wubu_cmd_read_pty(WubuCmd *cmd, char *buf, int maxlen);

/* History */
void wubu_cmd_history_add(WubuCmd *cmd, const char *line);
const char* wubu_cmd_history_prev(WubuCmd *cmd);
const char* wubu_cmd_history_next(WubuCmd *cmd);

/* Draw CMD into a window */
void wubu_cmd_draw(WubuCmd *cmd, void *win, uint32_t *fb, int fb_w, int fb_h);

#endif /* WUBU_CMD_H */