/* holyc_parse_internal.h -- Internal helpers shared by holyc_parse sub-modules.
 * Public API + types in holyc_parse.h. The AST construction helpers live in
 * holyc_parse_ast.c and are declared here so all submodules link the SAME
 * implementation (no double-coding).
 */

#ifndef HOLYC_PARSE_INTERNAL_H
#define HOLYC_PARSE_INTERNAL_H

#include "holyc.h"
#include <stdlib.h>

/* -- AST construction helpers (holyc_parse_ast.c) --------------- */
HCASTNode *hc_ast_new(HCASTKind kind);
void       hc_ast_free(HCASTNode *node);
void       hc_ast_add_stmt(HCASTNode *block, HCASTNode *stmt);
void       hc_ast_add_arg(HCASTNode *call, HCASTNode *arg);

#endif /* HOLYC_PARSE_INTERNAL_H */
