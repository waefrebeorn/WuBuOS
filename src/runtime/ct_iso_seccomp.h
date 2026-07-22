#ifndef WUBU_CT_ISO_SECCOMP_H
#define WUBU_CT_ISO_SECCOMP_H

#include <stdbool.h>
#include "wubu_ct_isolate.h"   /* SeccompProfile */
#include "wubu_host_exec.h"    /* CtRuntime */

int wubu_ct_apply_seccomp(void *ct_ptr);
int wubu_seccomp_install(SeccompProfile profile);
SeccompProfile runtime_to_seccomp(CtRuntime runtime);
int wubu_ct_child_isolation(void);

#endif /* WUBU_CT_ISO_SECCOMP_H */
