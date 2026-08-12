/*
 * wubu_bio.h -- kernel-owned storage bio routing.
 */
#ifndef WUBU_BIO_H
#define WUBU_BIO_H

#include <stddef.h>

void wubu_bio_probe(void);
int  wubu_bio_present(void);
int  wubu_bio_vec(void);
int  wubu_bio_bdi(void);
int  wubu_bio_read(void);
int  wubu_bio_write(void);
const char *wubu_bio_driver(void);
const char *wubu_bio_op_for(const char *o);
const char *wubu_bio_layer_for(const char *l);
int wubu_bio_summary(char *out, size_t cap);

#endif
