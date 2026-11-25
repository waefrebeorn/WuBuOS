# The JIT development loop

*WuBuOS exists to be developed while it runs — TempleOS's bet, and the reason the shell
is a development surface rather than a desktop that happens to have a terminal. This page
records what the loop actually has today, measured, and what is missing to close it.*

## The loop we are building

```
edit source  ->  compile with OUR compiler  ->  run IN the running OS  ->  see the result
     ^                                                                        |
     +-------------------------- the OS keeps running -------------------------+
```

The agentic workflow rides the same track: the AGI edits source, evaluates it through the
same compiler, and the EDR sees every agent-authored eval. There is one compile path, so a
human eval and an agent eval cannot drift apart.

## What exists (measured 2026-11-25)

| Piece | Evidence |
|---|---|
| In-OS HolyC REPL that compiles and runs | `src/gui/shell/holyc_repl.c` includes `compiler/holyd_codegen.h` and calls `hd_eval` |
| A single eval entry used by the shell | `wubu_holyc_eval()` in `src/runtime/holyc_eval.c`, wired at `hosted.c:204` via `holyc_term_set_eval()` |
| An agent-facing eval with disclosure | `wubu_holyc_agent_eval()`, logs `edr_log_agent_action(..., "holyc: <src>")` |
| Compiler in-tree | `src/comp/` (`holyd_lexer/parse/codegen/runtime`) — the same compiler the kernel's terminal uses |
| Editor window | `src/apps/editor/editor.c` (285 lines) |

## What is missing — and it is the load-bearing piece

**The Editor cannot compile or run anything.** Measured: `grep -rInE "F5|0x3F|compile|hd_eval|holyc"`
over `src/apps/editor/editor.c` returns **zero** hits. So the loop exists only inside the
terminal REPL: you can type one-liners, but you cannot open a file, edit it, and run it
without leaving the OS. That is the difference between a REPL and a development
environment.

**No file-backed eval.** The terminal has no `#include "file.HC"` / `run file` path: the
only `#include` lines in `holyc_repl.c` are C preprocessor includes. A program larger
than a comfortable one-liner cannot be loaded into the eval at all.

**No hot module replacement.** `grep -rIlE "jit_load|self_host" src --include=*.c` returns
zero files. Editing the OS's own source and having the running OS pick it up is not
possible yet; today a rebuild means leaving the OS.

## The order that closes the loop

1. **Editor: F5 = compile & run the buffer.** Take the buffer text, call the SAME eval the
   terminal uses (`wubu_holyc_eval`), and show output in a panel under the buffer. No new
   compiler path — one owner for "compile and run".
2. **Terminal: `#include "<path>"`.** Read a file from the OS filesystem into the eval. This
   is what makes a file — not a line — the unit of development, and it is exactly how
   TempleOS programs are written.
3. **Compile errors as data.** `hd_eval` currently returns an int and text; the editor needs
   line/column to underline the offending line. Without it the loop is "try again and
   guess", which is not a development environment.
4. **Hot module replacement.** The hard one: compile a translation unit, load it, and swap
   the entry the shell calls. Everything else above is achievable with what is in the tree;
   this needs a loader contract (symbol names, ABI, unload safety) before any code.

## Rules the loop must keep

- **One compile path.** The Editor, the Terminal and the AGI call the same eval entry. A
  second compiler invocation added for a UI convenience is how a self-hosted OS ends up
  with two compilers and one of them wrong.
- **Agent-authored evals are disclosed.** `wubu_holyc_agent_eval()` exists precisely so the
  AGI's evals land in the EDR trail. A UI shortcut that calls the plain entry on the agent's
  behalf would erase that distinction.
- **Eval is not a privilege boundary.** Compiled HolyC runs with the shell's authority. Any
  "run this file" path must therefore be an explicit user action, never something a
  document or a .desktop shortcut can trigger by being opened.
- **The shell must survive what it compiles.** A crashing eval must not take the desktop
  with it, or the developer loses the environment used to fix the crash.
