/*
 * wubu_uas.h -- kernel-owned storage UAS routing.
 */
#ifndef WUBU_UAS_H
#define WUBU_UAS_H

#include <stddef.h>

void wubu_uas_probe(void);
int  wubu_uas_present(void);
int  wubu_uas_bot(void);
int  wubu_uas_uasp(void);
int  wubu_uas_queue(void);
int  wubu_uas_part(void);
const char *wubu_uas_driver(void);
const char *wubu_uas_proto_for(const char *p);
const char *wubu_uas_dir_for(const char *d);
int wubu_uas_summary(char *out, size_t cap);

#endif
