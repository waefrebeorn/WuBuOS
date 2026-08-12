/*
 * wubu_dax.h -- kernel-owned storage DAX routing.
 */
#ifndef WUBU_DAX_H
#define WUBU_DAX_H

#include <stddef.h>

void wubu_dax_probe(void);
int  wubu_dax_present(void);
int  wubu_dax_pmem(void);
int  wubu_dax_fs(void);
int  wubu_dax_inode(void);
int  wubu_dax_dev(void);
const char *wubu_dax_driver(void);
const char *wubu_dax_type_for(const char *t);
const char *wubu_dax_fs_for(const char *f);
int wubu_dax_summary(char *out, size_t cap);

#endif
