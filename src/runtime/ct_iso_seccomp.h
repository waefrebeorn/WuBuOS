#ifndef WUBU_CT_ISO_SECCOMP_H
#define WUBU_CT_ISO_SECCOMP_H

#include <stdbool.h>
#include "wubu_ct_isolate.h"   /* SeccompProfile */
#include "wubu_host_exec.h"    /* CtRuntime */

int wubu_ct_apply_seccomp(void *ct_ptr);
int wubu_seccomp_install(SeccompProfile profile);
SeccompProfile runtime_to_seccomp(CtRuntime runtime);
int wubu_ct_child_isolation(void);

/* -- Seccomp Profile Registry (the Revolver Doctrine) --
 * The built-in profiles (BASIC/GPU/WINE) seed the registry; callers can
 * register NEW named profiles at runtime (a container runtime ships a
 * custom syscall set -> the system learns it without a recompile).
 * The allowlist array must be NULL-terminated (last entry = -1). */
int wubu_seccomp_profile_register(const char *name, const int *allowlist);

/* Look up a registered profile's allowlist by name. Returns NULL if
 * not registered. Used by the env-var profile path (probe, don't
 * assume — any registered name is loadable). */
const int *wubu_seccomp_profile_lookup(const char *name);

#endif /* WUBU_CT_ISO_SECCOMP_H */
