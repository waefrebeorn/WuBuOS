/*
 * wubu_nfsmount.h -- kernel-owned storage NFS mount routing.
 */
#ifndef WUBU_NFSMOUNT_H
#define WUBU_NFSMOUNT_H

#include <stddef.h>

void wubu_nfsmount_probe(void);
int  wubu_nfsmount_present(void);
int  wubu_nfsmount_mount(void);
int  wubu_nfsmount_vers(void);
int  wubu_nfsmount_rsize(void);
int  wubu_nfsmount_wsize(void);
const char *wubu_nfsmount_driver(void);
const char *wubu_nfsmount_vers_for(const char *v);
const char *wubu_nfsmount_opt_for(const char *o);
int wubu_nfsmount_summary(char *out, size_t cap);

#endif
