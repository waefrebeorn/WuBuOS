/* wubu_x11_request.h -- the request side of the bridge.
 *
 * A connected client sends a stream of requests: a 4-byte header (opcode, data,
 * length-in-words) followed by body. This module owns the header parse and the
 * replies for the handful of requests a client needs before it can do anything
 * visible. Everything is derived from the reference server's bytes, not from the
 * prose spec (see workflow-priorities.md: 43 self-checks passed while a real
 * client refused the connection).
 */
#ifndef WUBU_X11_REQUEST_H
#define WUBU_X11_REQUEST_H

#include <stddef.h>
#include <stdint.h>

typedef struct WubuX11Request {
    uint8_t  opcode;
    uint8_t  data;
    uint16_t length;      /* in 4-byte words, INCLUDING the header */
    const uint8_t *body;  /* body bytes alive in the caller's buffer */
} WubuX11Request;

/* Core opcodes the bridge answers today. */
enum {
    WUBU_X11_OP_CREATE_WINDOW = 1,
    WUBU_X11_OP_MAP_WINDOW = 8,
    WUBU_X11_OP_GET_GEOMETRY = 14,
    WUBU_X11_OP_INTERN_ATOM = 16,
    WUBU_X11_OP_GET_ATOM_NAME = 17,
    WUBU_X11_OP_GET_PROPERTY = 20,       /* 23 is GetSelectionOwner, not this */
    WUBU_X11_OP_GET_SELECTION_OWNER = 23,
    WUBU_X11_OP_GET_INPUT_FOCUS = 43,
    WUBU_X11_OP_QUERY_EXTENSION = 98,
    WUBU_X11_OP_LIST_EXTENSIONS = 99
};

/* Parse one request header. Returns 0 and fills `out` when the 4-byte header is
 * present; `need` receives the total byte count of the request. */
int wubu_x11_parse_request(const uint8_t *buf, size_t n, WubuX11Request *out,
                           size_t *need, int lsb_first);

/* Replies. Each returns the byte count written, or a negative value. `seq` is the
 * client's sequence number echoed in the reply -- clients match replies by it,
 * so a wrong value stalls them without any error message. */
int wubu_x11_reply_query_extension(uint8_t *out, size_t cap, uint16_t seq,
                                   int present, int lsb);
int wubu_x11_reply_list_extensions(uint8_t *out, size_t cap, uint16_t seq,
                                   const char *const *names, int count, int lsb);
int wubu_x11_reply_atom(uint8_t *out, size_t cap, uint16_t seq, uint32_t atom,
                        int lsb);
int wubu_x11_reply_atom_name(uint8_t *out, size_t cap, uint16_t seq,
                             const char *name, int lsb);
int wubu_x11_reply_geometry(uint8_t *out, size_t cap, uint16_t seq,
                            uint32_t root, int16_t x, int16_t y,
                            uint16_t w, uint16_t h, uint16_t bw, int lsb);
int wubu_x11_reply_input_focus(uint8_t *out, size_t cap, uint16_t seq,
                               uint32_t focus, int lsb);

/* Does this core opcode expect a reply?
 *
 * This matters more than it looks: a request that expects NO reply must be
 * consumed silently. Answering it with an Unimplemented error aborts a client
 * that was doing nothing wrong -- xdpyinfo died at X_CreateGC for exactly that
 * reason while the server was working fine. */
int wubu_x11_op_expects_reply(uint8_t opcode);

/* GetProperty for a property that does not exist: format 0, empty value. This is
 * how a client learns "no such property" -- refusing the request with an error
 * makes it abort instead, which is what happened to xdpyinfo at X_GetProperty. */
int wubu_x11_reply_property_missing(uint8_t *out, size_t cap, uint16_t seq, int lsb);

/* GetWindowAttributes for the root window, in the InputOutput map state. */
int wubu_x11_reply_window_attrs(uint8_t *out, size_t cap, uint16_t seq,
                                uint32_t visual, uint32_t colormap, int lsb);

/* GetKeyboardMapping: `keysyms_per` keysyms for `count` keycodes, all zero. */
int wubu_x11_reply_keymap(uint8_t *out, size_t cap, uint16_t seq,
                          uint8_t keysyms_per, int count, int lsb);
int wubu_x11_reply_best_size(uint8_t *out, size_t cap, uint16_t seq,
                             uint16_t w, uint16_t h, int lsb);
int wubu_x11_reply_screen_saver(uint8_t *out, size_t cap, uint16_t seq, int lsb);
int wubu_x11_reply_list_hosts(uint8_t *out, size_t cap, uint16_t seq, int lsb);
int wubu_x11_reply_pointer_control(uint8_t *out, size_t cap, uint16_t seq, int lsb);

#endif
