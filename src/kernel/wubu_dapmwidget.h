/*
 * wubu_dapmwidget.h -- kernel-owned audio DAPM widget routing.
 */
#ifndef WUBU_DAPMWIDGET_H
#define WUBU_DAPMWIDGET_H

#include <stddef.h>

void wubu_dapmwidget_probe(void);
int  wubu_dapmwidget_present(void);
int  wubu_dapmwidget_widget(void);
int  wubu_dapmwidget_power(void);
int  wubu_dapmwidget_path(void);
int  wubu_dapmwidget_stream(void);
const char *wubu_dapmwidget_driver(void);
const char *wubu_dapmwidget_type_for(const char *t);
const char *wubu_dapmwidget_power_for(const char *p);
int wubu_dapmwidget_summary(char *out, size_t cap);

#endif
