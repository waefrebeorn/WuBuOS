# Addendum Template (copy me for new entries)

Every new lesson gets an entry in `worked.md` or `didnt-work.md` (and a
deep-dive in `bugs.md` for root causes). Fill ALL fields — a half-filled
ledger is how memory goes blind.

---

## YYYY-MM-DD — Title
- **Context:** what we were doing / the situation.
- **What worked / didn't work:** the outcome, in one line.
- **Why (root cause / reason):** the mechanism. VERIFIED before patching
  (DA discipline): state the evidence that made the cause certain.
- **Evidence:** exact logs, dumps, or test output. Quote numbers.
- **When it may change:** conditions under which this could reverse (new
  hardware, design change, better understanding).
- **Related:** links to bugs.md entries, commits, or modules.

---

## Rules of the ledger

1. **Evidence first.** No entry without a quoted number, log line, or dump.
2. **Distinguish "didn't work" from "wrong".** "Didn't work YET" is a
   hypothesis with a reason; "wrong" is a settled fact. Say which.
3. **Never delete.** The past is data. If a failure later becomes a success
   (or vice versa), APPEND the reversal with its own date.
4. **Programmatic vs addendum:** generated reference docs come from
   `make docs`; this ledger is human and must be maintained by hand — that
   is its point.
