---
title: "Explainer: Tree-sitter vs. LSP"
source_url: https://lambdaland.org/posts/2026-01-21_tree-sitter_vs_lsp/
author: Ashton Wiersdorf (Lambda Land)
ingested: 2026-08-03
avenue: DevTools (DT)
type: article
date: 2026-01-21
also: "HN discussion 46719899 (Roslyn incremental parser, semantic token layering)"
---

# Explainer: Tree-sitter vs. LSP

What is the difference between Tree-sitter and a language server? Explained
from an *observable*, *pragmatic* point of view.

## Tree-sitter

Tree-sitter is a *parser generator*. You hand Tree-sitter a description for a
programming language and it will create a program that will parse that language
for you. What's special about Tree-sitter is that it is a.) fast, and b.) can
tolerate *syntax errors* in the input. These two properties make Tree-sitter
ideal for creating syntax highlighting engines in text editors. When you're
editing a program, *most of the time* the program will be in a syntactically
invalid state. During that time, you don't want your colors changing or just
outright breaking while you're typing. Naive regex-based syntax highlighters
frequently suffer from this issue.

Tree-sitter also provides a query language where you can make queries against
the parse tree. It is safer and more robust than using a regular expression
because it can do similar parsing to the language engine itself.

In short, Tree-sitter provides syntax highlighting that is faithful to how the
language implementation parses the program, instead of relying on regular
expressions that incidentally come close.

## Language server

A *language server* is a program that can analyze a program and report
interesting information about that program to a text editor. A standard, the
Language Server Protocol (LSP), defines the kinds of JSON messages that pass
between a text editor and the server. The protocol is an open standard; any
language and any text editor can take advantage of the protocol. Language
servers can provide information like locating the definition of a symbol,
possible completions at the cursor point, etc.

Language servers solve the "N x M problem" where N programming languages and M
text editors would mean there have to be N x M implementations for language
analyzers. Now, every language just needs a language server, and every editor
needs to be able to speak the LSP protocol.

Language servers are powerful because they can hook into the language's runtime
and compiler toolchain to get *semantically correct* answers to user queries.
For example, suppose you have two versions of a `pop` function, one imported
from a `stack` library, and another from a `heap` library. A purely syntactic
jump-to-definition tool might get confused as to where to go because it's not
sure what module is in scope at that point. A language server has access to
this information and would not get confused.

### Using a language server for highlighting

It *is* possible to use the language server for syntax highlighting. The
language server can be a more complicated program and so could surface
particularly detailed information about the syntax; it might also be slower
than tree-sitter.

Emacs' built-in LSP client, Eglot, recently added `eglot-semantic-tokens-mode`
to support syntax highlighting as provided from the language server.

**Update:** a good reason to use a language server for syntax highlighting: the
Rust language server rust-analyzer can tell your text editor when a variable
reference is mutable or not, which means you could highlight `mut` references
differently than non-`mut` ones.

---

## Companion notes from the HN discussion (item 46719899)

Salient engineering points raised by practitioners, relevant to a WuBuOS
editor/LSP design:

- **Tree-sitter is a heuristic, incremental parser.** Regular parsers start
  eating tokens from the start of the file and build the AST top-down.
  Tree-sitter grabs tokens from an arbitrary point, assembles them into AST
  nodes, then tries to extend the AST until the whole file is parsed. This
  supports incremental edits (throw away the AST for the modified part and
  re-parse), but most languages are designed to be unambiguous when parsed
  left-to-right, so this may involve retries and guesswork. C/C++ needs a
  symbol table; tree-sitter has to guess and can guess wrong.
- **Tree-sitter cannot answer semantic questions.** Given `x = 2;` tree-sitter
  has no idea what `x` is -- float or int, local, class variable, or global.
  That requires a symbol table.
- **Latency layering.** Tree-sitter parses on the main thread (or a close
  worker) typically in sub-ms timeframes, ensuring syntax coloring is
  synchronous with keystrokes. LSP semantic tokens are asynchronous by design.
  The ideal hygiene: tree-sitter provides the high-speed lexical coloring
  (keywords, punctuation, basic structure) instantly, and LSP paints the
  semantic modifiers (interfaces vs classes, mutable vs const) asynchronously.
- **Roslyn's counterpoint (from a Roslyn architect).** Roslyn aims for
  *microsecond* (not millisecond) parsing. Its incremental parser design makes
  99.99+% of edits happen in microseconds, reusing 99.99+% of syntax nodes,
  while producing an independent, immutable tree (no threading concerns when
  sharing trees out to concurrent consumers). Roslyn uses a *cascading set of
  classifying threads*: one that classifies lexically, one for syntax, one for
  semantics, and one for embedded languages (regex/JSON nested in C#) -- and
  the embedded languages cascade too. The same concept is applied to
  diagnostics: compiler-syntax, compiler-semantics, and third-party analyzers
  are computed separately. Benefits: scale up with the machine's free cores;
  display results as computed without waiting for the rest. Compiler syntax
  diagnostics take microseconds while third-party analyzers might take seconds
  -- no point stalling the former for the latter.
- **Error-tolerant + incremental parsers can serve both roles.** TypeScript
  used its own parser for VS syntax highlighting prior to tmLanguage, avoiding
  divergence bugs between two parsers; the cost was JSON round-trip latency and
  a dedicated thread/memory overhead for fast highlighting.
- **Grammar size is a real distribution cost.** A multi-language CLI tool
  bloats its binary with grammars; opt-in language packs distributed as WASM
  are the workaround.
- **CST vs AST.** Tree-sitter produces a concrete syntax tree; an extra
  lowering step is needed to get to an AST for a compiler front end.
