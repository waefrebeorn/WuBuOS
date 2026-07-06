/*
 * wubu_image_internal.h  --  WuBuOS Image Builder Internal API
 *
 * Shared helpers and forward declarations for the wubu_image subsystem.
 * Extracted from wubu_image.c (2026-07-06): parser module split.
 *
 * C11 only. No god headers. Include from wubu_image_parse.c and
 * wubu_image.c for shared utility functions.
 */
#ifndef WUBU_IMAGE_INTERNAL_H
#define WUBU_IMAGE_INTERNAL_H

#include "wubu_image.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

/* -- Safe String Helpers (shared across modules) ------------------- */

static inline void str_trim(char *s) {
    char *end;
    while (isspace(*s)) s++;
    if (!*s) return;
    end = s + strlen(s) - 1;
    while (end > s && isspace(*end)) end--;
    *(end + 1) = '\0';
}

static inline bool str_startswith(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static inline char *str_dup(const char *s) {
    if (!s) return NULL;
    char *d = malloc(strlen(s) + 1);
    if (d) strcpy(d, s);
    return d;
}

/* -- Shared Crypto Wrapper ----------------------------------------- */
/* wubu_crypto.h provides wubu_sha256_digest / wubu_SHA256_CTX.
 * These aliases keep existing code unchanged. */
#include "wubu_crypto.h"
#define SHA256_CTX    wubu_SHA256_CTX
#define sha256_digest wubu_sha256_digest
#define sha256_file   wubu_sha256_file

/* -- Parser API (implemented in wubu_image_parse.c) ---------------- */

int  parse_instruction(const char *line, WubuInstruction *inst, int line_num);
int  wubu_parse_wubufile(const char *path, WubuBuildContext *ctx);
int  wubu_parse_wubufile_str(const char *content, WubuBuildContext *ctx);

#endif /* WUBU_IMAGE_INTERNAL_H */