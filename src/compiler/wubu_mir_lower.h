/*
 * wubu_mir_lower.h -- AST -> MIR lowering (the hourglass neck).
 * See wubu_mir_lower.c.
 */
#ifndef WUBU_MIR_LOWER_H
#define WUBU_MIR_LOWER_H

#include "holyc_ast.h"
#include "wubu_mir.h"

/* lower an AST expression into `p`; returns the vr holding its value */
wubu_vr_t wubu_mir_lower_expr(wubu_mir_prog_t *p, const HCASTNode *n);

#endif /* WUBU_MIR_LOWER_H */
