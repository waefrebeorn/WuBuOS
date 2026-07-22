#ifndef WUBU_CT_ISO_NS_H
#define WUBU_CT_ISO_NS_H

#include <sched.h>

/* Full container namespace isolation flags (passed to wubu_ns_unshare). */
#define WUBU_NS_FLAGS (CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWNET | \
                       CLONE_NEWUSER | CLONE_NEWUTS | CLONE_NEWIPC)

int wubu_ns_unshare(int flags);

#endif /* WUBU_CT_ISO_NS_H */
