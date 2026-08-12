/*
 * wubu_gamepadbm.h -- kernel-owned gamepad button map routing.
 */
#ifndef WUBU_GAMEPADBM_H
#define WUBU_GAMEPADBM_H

#include <stddef.h>

void wubu_gamepadbm_probe(void);
int  wubu_gamepadbm_present(void);
int  wubu_gamepadbm_map(int button);
int  wubu_gamepadbm_is_pressed(int value);
void wubu_gamepadbm_summary(char *out, size_t cap);

#endif
