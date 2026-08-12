/*
 * wubu_nfsclient.h -- kernel-owned storage NFS client routing.
 */
#ifndef WUBU_NFSCLIENT_H
#define WUBU_NFSCLIENT_H

#include <stddef.h>

void wubu_nfsclient_probe(void);
int  wubu_nfsclient_present(void);
int  wubu_nfsclient_idmapd(void);
int  wubu_nfsclient_statd(void);
int  wubu_nfsclient_mount(void);
int  wubu_nfsclient_sec(void);
const char *wubu_nfsclient_driver(void);
const char *wubu_nfsclient_vers_for(const char *v);
const char *wubu_nfsclient_proto_for(const char *p);
int wubu_nfsclient_summary(char *out, size_t cap);

#endif
