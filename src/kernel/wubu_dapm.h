/*
 * wubu_dapm.h -- kernel-owned audio DAPM routing.
 */
#ifndef WUBU_DAPM_H
#define WUBU_DAPM_H

#include <stddef.h>

void wubu_dapm_probe(void);
int  wubu_dapm_present(void);
const char *wubu_dapm_widget_type_str(int type);
int  wubu_dapm_path_active(const char *name);
void wubu_dapm_summary(char *out, size_t cap);

#endif
