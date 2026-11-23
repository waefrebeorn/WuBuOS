#ifndef WUBU_FBTERM_H
#define WUBU_FBTERM_H
/* Echo the live console line into the GOP framebuffer. */
void wubu_fbterm_current(const char *line);
void wubu_fbterm_submit(const char *line);
#endif
