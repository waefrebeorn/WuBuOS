# sizeof + switch + goto + multi-dim arrays + nested pointer access

**Status: SHIPPED + GREEN** (2026-08-13). Closes five kernel-critical C11
gaps the compiler needed to compile the kernel + desktop.

## sizeof

**Why kernel needs it**: `sizeof` is everywhere in kernel C — buffer sizing,
struct layout math, memcpy lengths. Before this wave it lexed as garbage
(returned 0), so any kernel source using it mis-compiled silently.

**Implementation** (4 files):
- `holyc_types.h`: `HC_KW_SIZEOF` token (reused the `HC_KW_UNUSED` enum
  slot — no enum-value churn), `HC_AST_SIZEOF` AST node (reuses the `type`
  field that the struct/cast work already populated).
- `holyc_lexer.c`: `"sizeof"` keyword.
- `holyc_parse.c`: `sizeof` in `parse_primary`. Handles three forms:
  `sizeof(type)` (lookahead: type keyword after `(` → parse_type),
  `sizeof(expr)` (paren expression), `sizeof expr` (no parens).
- `holyc_codegen_expr.c`: `HC_AST_SIZEOF` emits the size as an immediate
  (`mov rax, imm64`) from `hc_type_size` of either the parsed type or the
  expression's static type (`expr_static_type`).

**Debugging lesson (paren backtrack)**: my first `sizeof(x)` paren path
backtracked by restoring `lex->pos`, but the pos captured while the token was
`(` already points *past* the `(` (at the inner expr), so the restore landed
on the wrong char and `hc_eval` failed the whole block with `has_error=1`
before codegen ran. Fix: **don't backtrack** — after consuming `(`, just
`parse_expr` then `expect(RPAREN)`. The parser's cast-disambiguation in
`parse_cast` also consumes the closing `)` itself, so routing the paren expr
through `parse_cast` would double-consume `)`; route through the inline
`parse_expr; expect(RPAREN)` instead.

**Verified**: `sizeof(int)`=4, `sizeof(char)`=1, `sizeof(struct S{a;b;})`=8,
`sizeof var`=4, `sizeof array`=12, `sizeof member`=4, `sizeof nested
struct Q{P p;int b;}`=8 (gcc agrees — 4+4, no padding needed for
4-byte-aligned members).

## switch

**Why kernel needs it**: switch is the backbone of every driver's command/
state dispatch (AHCI, FAT32, LZX, every syscall handler). Before this wave it
was unparsed (returned 0).

**Implementation** (4 files):
- `holyc_types.h`: `HC_AST_SWITCH` (cond=switch expr, body=block of CASE
  nodes) + `HC_AST_CASE` (cond=case value expr, NULL=default; body=stmts).
- `holyc_parse.c`: `switch` in `parse_stmt`. `switch(expr){ case V: stmts
  ... default: stmts }` — collects CASE nodes into a BLOCK body; statements
  after a `case`/`default` label attach to the current case's body.
- `holyc_codegen_emit.c`: switch dispatch helpers (`emit_push_rax`,
  `emit_mov_rax_mem_rsp`, `emit_cmp_rax_rdi`, `emit_add_rsp_8`,
  `emit_jcc_placeholder` already existed).
- `holyc_codegen_stmt.c`: `HC_AST_SWITCH` emits:
  1. eval switch expr → rax, `push rax` ([rsp] = switch value)
  2. dispatch chain — per case: `gen case value → rax; mov rdi,rax; mov
     rax,[rsp]; cmp rdi,rax; je <body>` (case value may be any expression,
     so it's moved to rdi before the switch value is reloaded into rax)
  3. no-match `jmp` → default body (or end if none)
  4. case bodies emitted contiguously → **fallthrough is natural**
  5. `end:` after `add rsp,8` (pop switch value); `break` targets AFTER the
     pop so every exit path balances rsp
- `HC_AST_CASE` standalone emits nothing (only valid inside a switch).

**Verified** (battery + standalone): case match, default match, no-match
with/without default, **fallthrough**, **expression cases** (`case 1+1`),
switch inside a function, multi-statement cases. 8/8 standalone, 6 battery
probes.

## Battery additions (selfhost_battery.c)

7 `sizeof` + 6 `switch` + 3 `goto` + 7 `multi-dim array` + 7 `nested
pointer/deref` probes → battery **78 → 108 probes, 108/108 PASS, 0 WRONG,
0 CRASH**. (The nested-struct sizeof probe exposed a wrong *expectation* on
my part — gcc confirms 8, compiler was right — fixed, not a compiler bug.)

## goto

`goto label;` + `label:` statement. HC_AST_GOTO (jmp placeholder + label
patch registry) + HC_AST_LABEL (records current code position, patches any
pending forward gotos). Forward and backward gotos verified (loop via goto,
skip-init). Labels are function-local; `hc_gen_init` memsets the registry
each compile so it's naturally scoped per `hc_eval`.

Debugging lesson (again the pos-vs-token trap): detecting `ident:` needs a
2-token lookahead. A pos-restore lands on the wrong char; instead capture
the IDENT text / whole token BEFORE advancing, and restore by direct token
assignment.

## Multi-dimensional arrays

`int a[2][3]` → ARRAY(ARRAY(int,3),2). The RIGHTMOST dimension is the
INNERMOST: collect all `[N]` sizes then build innermost→outermost (the
first attempt stacked them in source order, so `a[2][3]` had the dims
reversed and sizeof was wrong). Nested INDEX resolves its scale via
`expr_static_type` on the base (so `a[0][0]` outer scale = int size, not
1). `sizeof a`=24, `sizeof a[0]`=12 (gcc agrees).

## Array-to-pointer decay

`int* p = a;` / `int* p = a[0];` must store the ADDRESS, not load the
first element. VAR_DECL init now detects (declared type PTR + init static
type ARRAY) and emits `emit_base_addr` instead of `gen_expr`. Requires
`expr_static_type` + `emit_base_addr` exported across codegen submodules.

## Nested pointer / deref member access

`p->a`, `(*p).a`, `q.p->a` (arrow through a pointer member), and writes to
all of them. Two root causes fixed:
- `emit_base_addr` on a nested composite now dispatches on static type:
  pointer → LOAD the value (that IS the address, e.g. `q.p->a` where q.p is
  a `struct P*`); struct/array → compute the ADDRESS.
- `(*p).a` parse: parse_cast's not-a-cast paren path orphaned trailing
  postfix (`.a` dropped). Fix: rewind to the `(` (saved_pos-1) and delegate
  to `parse_postfix`, which parses `(expr)` as a primary AND applies `.a`;
  the caller's binop loop still applies `*4` to `(2+3)*4`. Paren arithmetic
  (battery `paren`, `prec paren`) re-verified — this fix preserves it.

## Honest remainder

- No `case`/`default` duplicate detection.
- No jump-table optimization — linear compare chain, O(n) dispatch.
- `sizeof` of a VLA unsupported (arrays are compile-time sized).
- **Struct-by-value RETURN is unsupported** (SysV ABI sret: struct returned
  via a hidden pointer param). Passing a struct by value works; returning
  one gives garbage. Requires ABI machinery for a hidden sret arg + the
  caller passing a buffer. Documented, not yet implemented.

## Gates (all fresh-verified)

- battery 108/108 PASS (was 78) — no regression
- test_holyc 84/84, test_holyc_agi 11/11
