/* styx_socket.h -- serve 9P messages over a file descriptor. */
#ifndef WUBU_STYX_SOCKET_H
#define WUBU_STYX_SOCKET_H

#include "styx.h"

/* Read one size-prefixed message, dispatch it, write the size-prefixed reply.
 * Returns the reply length, 0 on EOF, -1 on error. */
int styx_serve_one(int fd, styx_server_t *srv);

/* Write an already-framed request and read one framed reply. */
int styx_socket_request(int fd, const uint8_t *req, uint32_t len,
                        uint8_t *reply, uint32_t cap, uint32_t *rlen);

#endif
