/*
 * doom.h — Free Doom Engine for WuBuOS
 */
#ifndef MYSEED_DOOM_H
#define MYSEED_DOOM_H

#include <stdint.h>

void doom_init(void);
void doom_update(void);
void doom_render(uint32_t *fb, int w, int h);
void doom_shutdown(void);

#endif
