/*
 * oci_media_types.c  --  OCI Media Type String Helpers
 * 
 * Extracted from wubu_oci.c (lines 495-505).
 */

#include "oci_internal.h"

/* -- Media Type Helpers ---------------------------------------------- */

const char *oci_media_type_image_manifest_v1(void) { return "application/vnd.oci.image.manifest.v1+json"; }
const char *oci_media_type_image_manifest_v2(void) { return "application/vnd.oci.image.manifest.v2+json"; }
const char *oci_media_type_image_index_v1(void) { return "application/vnd.oci.image.index.v1+json"; }
const char *oci_media_type_image_config_v1(void) { return "application/vnd.oci.image.config.v1+json"; }
const char *oci_media_type_layer_v1(void) { return "application/vnd.oci.image.layer.v1.tar"; }
const char *oci_media_type_layer_v1_gzip(void) { return "application/vnd.oci.image.layer.v1.tar+gzip"; }
const char *oci_media_type_layer_v1_zstd(void) { return "application/vnd.oci.image.layer.v1.tar+zstd"; }
const char *oci_media_type_empty_json(void) { return "application/vnd.oci.empty.v1+json"; }