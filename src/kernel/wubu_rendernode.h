/*
 * wubu_rendernode.h -- kernel-owned GPU render node routing.
 */
#ifndef WUBU_RENDERNODE_H
#define WUBU_RENDERNODE_H

#include <stddef.h>

void wubu_rendernode_probe(void);
int  wubu_rendernode_present(void);
int  wubu_rendernode_valid_fd(int fd);
int  wubu_rendernode_priority(int ctx_priority);
void wubu_rendernode_summary(char *out, size_t cap);

#endif
