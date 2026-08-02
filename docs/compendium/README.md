# WuBuOS Documentation Compendium

The living encyclopedia of the WuBuOS project. **Wikipedia-style, split into
many folders**, with two kinds of content:

- **Programmatically generated** (run `make docs` after code changes) — the
  reference sections stay in sync with the code by construction.
- **Addendum / human-written** — philosophy, decisions, and the
  worked/didn't-work ledger, maintained by hand as we learn.

```
compendium/
├── README.md              ← you are here (the index)
├── 00-philosophy/         ← design philosophy, virtues, what we found (human)
│   ├── ring0-colonel.md      the AGI's DOS: everything at ring 0
│   ├── memory.md             single-level store, the hive, MemGPT hierarchy
│   ├── anticheat.md          attestation-rooted anti-cheat
│   ├── magical-os.md         the Willy Wonka / Genera heritage
│   └── decisions.md          the decision log (why we chose what)
├── 01-reference/          ← GENERATED (make docs) — do not edit
│   ├── modules.md             every module + purpose + dependencies
│   ├── api.md                 public functions per module
│   ├── symbols.md             kernel symbols from nm
│   ├── tests.md               test targets
│   └── build.md               build targets
├── 02-architecture/       ← architecture (human + generated pointers)
│   ├── levels.md              level 0-3 model
│   ├── boot-chain.md          WuBuFW measured boot
│   ├── tasking.md             scheduler + task model
│   └── memory-map.md          address map, heap, stacks
├── 03-learned/            ← the PRESTIGE LEDGER (recursive learning)
│   ├── worked.md              what worked, with evidence (append)
│   ├── didnt-work.md          what didn't, with why + when it may change
│   ├── bugs.md                the war stories (root causes, full detail)
│   └── TEMPLATE.md            the addendum template (copy for new entries)
└── 04-roadmap/            ← plans + audits
    ├── da-audit.md            → ../DA_DESIGN_AUDIT.md
    ├── submodules.md          the P0/P1 module plan
    └── research.md            → ../AGI_OS_DESIGN.md (Kevin-Bacon synthesis)
```

## How to keep it honest

1. **After every code change batch:** run `make docs` — the reference sections
   regenerate from the source tree (no drift by construction).
2. **After every lesson:** append to `03-learned/worked.md` or
   `03-learned/didnt-work.md` using the TEMPLATE — date, context, what, why,
   evidence, would-it-change. This is the prestige system: recursive
   learning with memory, not blind ADHD.
3. **After every design decision:** append to `00-philosophy/decisions.md` —
   the choice, the alternative, the reason.
4. **The DA discipline:** every bug entry states the evidence and whether the
   root cause was VERIFIED real before any patch.
