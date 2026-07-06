/*
 * oci_hooks.c  --  OCI Runtime Hook Operations
 * 
 * Extracted from wubu_oci.c (lines 1614-1632).
 */

#include "oci_internal.h"

/* -- Hooks ----------------------------------------------------------- */

int oci_hook_create(OciHook *hook, const char *path, const char *args[], int argc,
                    const char *env[], int envc, int timeout) {
    if (!hook || !path) return -1;
    memset(hook, 0, sizeof(*hook));
    strncpy(hook->path, path, sizeof(hook->path) - 1);
    hook->argc = argc > 32 ? 32 : argc;
    for (int i = 0; i < hook->argc && args && args[i]; i++)
        strncpy(hook->args[i], args[i], sizeof(hook->args[i]) - 1);
    hook->envc = envc > 32 ? 32 : envc;
    for (int i = 0; i < hook->envc && env && env[i]; i++)
        strncpy(hook->env[i], env[i], sizeof(hook->env[i]) - 1);
    hook->timeout = timeout;
    return 0;
}

void oci_hook_free(OciHook *hook) {
    (void)hook;
}