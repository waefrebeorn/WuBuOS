/*
 * print_dialog.h -- Win11-parity Print dialog over the real spooler.
 */
#ifndef WUBU_PRINT_DIALOG_H
#define WUBU_PRINT_DIALOG_H

#include <stdint.h>

void print_dialog_open(const char *path);
void print_dialog_close(void);
int  print_dialog_is_open(void);
int  print_dialog_printer_count(void);
int  print_dialog_copies(void);
const char *print_dialog_message(void);

void print_dialog_handle_key(uint32_t key, uint32_t mods);
int  print_dialog_handle_click(int mx, int my, int fb_w, int fb_h);
void print_dialog_rect(int fb_w, int fb_h, int *x, int *y, int *w, int *h);
void print_dialog_render(uint32_t *fb, int fb_w, int fb_h);

#endif /* WUBU_PRINT_DIALOG_H */
