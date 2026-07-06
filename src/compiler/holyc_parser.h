/*
 * holyc_parser.h  --  HolyC Parser
 * Parses HCToken stream into Abstract Syntax Tree (AST).
 * Self-contained, C11, minimal includes.
 */
#ifndef MYSEED_HOLYC_PARSER_H
#define MYSEED_HOLYC_PARSER_H

#include "holyc_types.h"

void hc_parse_init(HCParser *p, HCLexer *lex);
HCASTNode *hc_parse_expr(HCParser *p);
HCASTNode *hc_parse_stmt(HCParser *p);
HCASTNode *hc_parse_decl(HCParser *p);
HCASTNode *hc_parse_compilation_unit(HCParser *p);
HCASTNode *hc_parse_block(HCParser *p);
HCTokenType hc_parse_peek(HCParser *p);

#endif /* MYSEED_HOLYC_PARSER_H */