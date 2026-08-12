/*
 * wubu_iosched.h -- kernel-owned storage I/O scheduler routing.
 */
#ifndef WUBU_IOSCHED_H
#define WUBU_IOSCHED_H

#include <stddef.h>

void wubu_iosched_probe(void);
int  wubu_iosched_present(void);
int  wubu_iosched_mq(void);
int  wubu_iosched_deadline(void);
int  wubu_iosched_kyber(void);
int  wubu_iosched_bfq(void);
int  wubu_iosched_none(void);
const char *wubu_iosched_driver(void);
const char *wubu_iosched_algo_for(const char *a);
const char *wubu_iosched_mode_for(const char *m);
int wubu_iosched_summary(char *out, size_t cap);

#endif
