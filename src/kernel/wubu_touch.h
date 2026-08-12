/*
 * wubu_touch.h -- kernel-owned touchscreen/trackpad driver routing.
 */
#ifndef WUBU_TOUCH_H
#define WUBU_TOUCH_H

#include <stddef.h>

/* W1: probe the touch topology. */
void wubu_touch_probe(void);

/* W2: accessors */
int  wubu_touch_present(void);
int  wubu_touch_elan(void);
int  wubu_touch_synaptics(void);
int  wubu_touch_multitouch(void);
int  wubu_touch_wacom(void);
const char *wubu_touch_driver(void);

/* W3: touch driver routing. */
const char *wubu_touch_driver_for(const char *dev);

/* W4: summary fragment. */
int wubu_touch_summary(char *out, size_t cap);

#endif /* WUBU_TOUCH_H */
