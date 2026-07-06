/*
 * oci_descriptor.c  --  OCI Descriptor Operations
 * 
 * Extracted from wubu_oci.c (lines 506-516).
 */

#include "oci_internal.h"

/* -- Descriptor Operations -------------------------------------------- */

int oci_create_descriptor(OciDescriptor *desc, const char *media_type, uint64_t size, const char *sha256_digest) {
    if (!desc || !media_type || !sha256_digest) return -1;
    memset(desc, 0, sizeof(OciDescriptor));
    desc->schema_version = 2;
    strncpy(desc->media_type, media_type, sizeof(desc->media_type) - 1);
    desc->size = size;
    snprintf(desc->digest, sizeof(desc->digest), "sha256:%s", sha256_digest);
    return 0;
}