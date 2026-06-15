/*
 * bridge.h  --  My Seed GUI↔Temple Mode Bridge
 */
#ifndef MYSEED_BRIDGE_H
#define MYSEED_BRIDGE_H
#include <stdint.h>

typedef enum { MODE_GUI = 0, MODE_TEMPLE = 1 } BridgeMode;

/* Clipboard (shared between modes) */
#define CLIPBOARD_MAX 4096

/* IPC Message */
typedef struct {
    int    from_pid;
    int    to_pid;       /* 0 = broadcast */
    int    type;
    char   payload[256];
} BridgeMessage;

/* Init bridge subsystem */
int  bridge_init(void);
void bridge_shutdown(void);

/* Mode switching */
BridgeMode bridge_get_mode(void);
void       bridge_toggle_mode(void);  /* Ctrl+Alt+T handler */
void       bridge_enter_temple(void);
void       bridge_exit_temple(void);

/* Clipboard */
void       bridge_clipboard_set(const char *data, int len);
int        bridge_clipboard_get(char *buf, int max_len);

/* IPC */
void       bridge_send_msg(const BridgeMessage *msg);
int        bridge_poll_msg(int pid, BridgeMessage *out);

#endif
