/*
 * paint.h — WuBuOS Paint (Photoshop-style image editor)
 */
#ifndef MYSEED_PAINT_H
#define MYSEED_PAINT_H

void paint_init(void);
void paint_open(void);
void paint_update(void);
void paint_render(void *win, void *fb, int w, int h);
void paint_shutdown(void);

#endif
