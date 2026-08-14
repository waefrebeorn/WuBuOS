/* jit_branch_profile.h -- Runtime branch-feedback subsystem (Subsystem C). */
#ifndef JIT_BRANCH_PROFILE_H
#define JIT_BRANCH_PROFILE_H

#include <stdint.h>

#define JBP_MAX_BRANCHES 256

void    jbp_init(int n_branches);
int64_t *jbp_counter_taken(int id);
int64_t *jbp_counter_not_taken(int id);
double  jbp_taken_fraction(int id);      /* [0,1] or -1 if never executed */
int     jbp_fallthrough_is_hot(int id);  /* #14 layout decision */
int     jbp_branch_count(void);
int     jbp_emit_taken_inc(unsigned char *buf, int id);

#endif /* JIT_BRANCH_PROFILE_H */
