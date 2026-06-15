/*
 * bridge.c  --  My Seed Bridge Implementation
 */
#include "bridge.h"
#include <string.h>

static BridgeMode g_mode = MODE_GUI;
static char g_clipboard[CLIPBOARD_MAX];
static int  g_clipboard_len = 0;

#define MSG_QUEUE_SIZE 64
static BridgeMessage g_msg_queue[MSG_QUEUE_SIZE];
static int g_msg_head = 0, g_msg_tail = 0;

int bridge_init(void) {
    g_mode = MODE_GUI;
    g_clipboard_len = 0;
    g_msg_head = g_msg_tail = 0;
    return 0;
}

void bridge_shutdown(void) { }

BridgeMode bridge_get_mode(void) { return g_mode; }

void bridge_toggle_mode(void) {
    if (g_mode == MODE_GUI) bridge_enter_temple();
    else                    bridge_exit_temple();
}

void bridge_enter_temple(void)  { g_mode = MODE_TEMPLE; }
void bridge_exit_temple(void)   { g_mode = MODE_GUI; }

void bridge_clipboard_set(const char *data, int len) {
    if (len > CLIPBOARD_MAX - 1) len = CLIPBOARD_MAX - 1;
    memcpy(g_clipboard, data, len);
    g_clipboard[len] = '\0';
    g_clipboard_len = len;
}

int bridge_clipboard_get(char *buf, int max_len) {
    int len = g_clipboard_len < max_len ? g_clipboard_len : max_len;
    memcpy(buf, g_clipboard, len);
    return len;
}

void bridge_send_msg(const BridgeMessage *msg) {
    g_msg_queue[g_msg_tail] = *msg;
    g_msg_tail = (g_msg_tail + 1) % MSG_QUEUE_SIZE;
}

int bridge_poll_msg(int pid, BridgeMessage *out) {
    if (g_msg_head == g_msg_tail) return 0;
    BridgeMessage *m = &g_msg_queue[g_msg_head];
    if (m->to_pid == 0 || m->to_pid == pid) {
        *out = *m;
        g_msg_head = (g_msg_head + 1) % MSG_QUEUE_SIZE;
        return 1;
    }
    return 0;
}
