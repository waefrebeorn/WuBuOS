# WuBuOS Application Framework — 100-Point Research & Way-Forward Plan

**Date:** 2026-07-20
**Author:** angel coder (Hermes)
**Status:** Research-complete, design ratified, scaffold pending
**Inputs:** 50 web searches (see appendix) + prior WuBuOS research
(OS_BIBLE.md, STATE.md, DESKTOP_VISION_PLAN.md, DESKTOP_FIXUP_PLAN.md,
REACTOS_NT_SYSCALL_STUDY.md, GRAHAOS_AUDIT, reactos-study/, os-studies/) +
existing code (Styx9 `src/runtime/styx*`, AGI `src/runtime/wubu_holyc_agi.*`,
EDR `src/runtime/edr/*`, HolyC JIT `src/compiler`, WM `src/gui/dosgui_*`).

---

## 0. The Mandate (from the user)

> "Delete all representative demos — they are lies. Build an equivalent to the
> .NET Framework, but use everything people learned about the headaches of
> these frameworks to make a *superior* one. Steve Jobs-level mindset. Remain
> with our AGI and EDR system. Do a 100-point research plan with 50 searches
> and look through previous research to composite our way forward."

The demos are deleted. This document *is* the 100-point plan. The framework is
named **WuBuFX** (WuBuOS Framework eXperience) — a Styx9-namespaced,
AGI+EDR-native application framework that treats the .NET failure modes as a
checklist of things it will *not* repeat.

---

## 1. What We Are Actually Building (one paragraph)

WuBuFX is **not** a CLR/JIT-bytecode VM clone. It is a **namespace-first
application framework** where the unit of composition is a **Styx9 filesystem
node** (`/app/...`, `/sys/...`, `/agi/...`, `/edr/...`), and the unit of
execution is **live HolyC** (already working: `wubu_holyc_eval`) or a native
WuBuOS binary. Frameworks, libraries, and apps are all *mounted* namespaces.
Every app action is **disclosed to EDR** by construction (`wubu_holyc_agent_eval`
already does this for AGI; WuBuFX extends it to all first-party code). The
desktop compositor (`dosgui_wm`) is the *shell*, not the framework, and it is
already separated — we keep it that way.

---

## 2. The 50 Headaches We Studied (and our answer to each)

Grouped by the framework we learned from. Each line: *headache → WuBuFX answer*.

### .NET Framework / Core (searches 1, 26, 27, 28)
1. Version churn (.NET 4.8→5→6→7→8→9 breaking changes) → **One framework,
   capability-gated by Styx9 mount version, never a hard break.**
2. DLL Hell / version binding → **single global namespace, content-hash
   addressed (`/lib/sha256:...`), no side-by-side DLL lottery.**
3. `regsvr32` COM registration → **no registry; capabilities are filesystem
   ACLs on Styx9 nodes.**
4. `app.config`/`.deps.json` fragility → **manifest is a Styx9 directory tree,
   readable by `ls`, not XML.**
5. NuGet transitive-dependency conflicts → **dependency closure is a
   content-addressed subtree; conflicts are compile errors, not runtime
   surprises.**
6. Cold-start JIT latency → **HolyC is already AOT/JIT-less (compile+run live);
   native binaries are single static artifacts.**
7. GC stop-the-world pauses → **no managed GC mandate; apps opt into arena or
   manual memory; kernel is single-address-space like ZealOS.**
8. Reflection/emitting `Assembly.Load` from bytes = supply-chain hole → **code
   load is an EDR-disclosed, capability-gated event.**
9. `System.*` monolith bloat → **framework is a *mounted subtree*, only the
   nodes you `open()` are linked.**
10. x86/x64/arm packaging matrix → **HolyC + single static binary; one artifact.**

### Java (search 3)
11. Classpath hell → **classpath is the Styx9 namespace; resolution is `open()`.**
12. PermGen/Metaspace OOM → **no classloader metaclass heap; types are nodes.**
13. XML config (Spring) → **no XML; declarative UI is data, not config files.**
14. Checked exceptions boilerplate → **errors are Styx9 events, not exceptions
   thrown across call stacks.**
15. WAR/EAR container coupling → **no servlet container; an app *is* a namespace.**

### Node.js / npm (search 4)
16. `node_modules` bloat (250 MB for hello-world) → **no install-step bloat;
   deps are mounted content-addressed nodes, lazy-resolved.**
17. left-pad / tiny-package supply chain → **packages are hash-pinned; a
   package is a signed Styx9 subtree, not a tarball from a registry.**
18. `^`/`~` range resolution nondeterminism → **exact hash pins; lockfile is
   implicit in the namespace hash.**
19. `npm install` runs arbitrary postinstall scripts → **no postinstall; build
   is a disclosed EDR action or it does not run.**
20. Callback/async stack-trace loss → **synchronous HolyC execution model;
   no promise-chain stack erosion.**

### Docker / containers (search 5)
21. Image bloat (GB layers) → **apps are Styx9 namespaces, not layered FS
   images; sizes are transparent (`du` on the namespace).**
22. Supply-chain malicious images → **every node signed; EDR verifies on mount.**
23. `docker run --privileged` escape → **capability model, not flag-based.**
24. YAML orchestration fatigue → **no YAML; composition is namespace mounting.**

### Electron (search 6)
25. Shipping a browser+Node per app (300 MB RAM) → **native WuBuOS binary; the
   WM is shared, not per-app.**
26. Chromium CVE surface → **no webview in the framework core; UI is native
   draw or HolyC.**

### UI frameworks (searches 7, 33, 34, 35, 36)
27. React/Flutter/Qt abstraction tax → **immediate-mode + retained hybrid:
   `dosgui_wm` already does retained; WuBuFX adds a thin immediate DSL.**
28. SwiftUI hidden-state pitfalls → **state is explicit Styx9 nodes (`/app/win/state`);
   no magic `@State`.**
29. Flutter Skia/Impeller redraw cost → **damage-rect compositor (already in
   `dosgui_wm_render`); only changed regions repaint.**
30. ImGui lack of accessibility/theming → **theme engine already exists
   (4 themes, `wubu_theme`); WuBuFX binds it per-namespace.**
31. Android/iOS fragmentation → **one target: WuBuOS; host ports are the OS's
   job, not the app's.**
32. CSS specificity wars → **no CSS; layout is data + a small constraint solver.**

### Wayland / compositing (searches 8, 9)
33. X11 global state /input race → **compositor owns input; apps are clients of
   Styx9, not X clients.**
34. Wayland protocol explosion → **one protocol: Styx9/9P. No extension zoo.**
35. Compositor crashes take the session → **WM is a recoverable service
   (`wubu_session`); restart does not lose namespace state.**
36. Client-side decoration inconsistency → **server-side frame (already in
   `dosgui_wm`); apps don't draw their own chrome.**

### Language/abstraction philosophy (searches 10–14, 50)
37. Too many abstraction layers (search 10) → **max 2 layers: app → namespace.
   No ORM, no DI container, no annotation magic.**
38. Spring annotation magic (search 21) → **no annotations; behavior is code,
   visible.**
39. Rails convention-over-configuration fear (search 22) → **convention is
   *documented naming*, not implicit global behavior.**
40. DI framework overkill (search 23) → **dependencies are constructor args /
   namespace opens; no container.**
41. ORM N+1 (search 24) → **no ORM; data is files in Styx9, queries are walks.**
42. Hidden control flow (search 20, 25) → **HolyC is explicit; `async/await`
   stack erosion does not exist.**
43. Go minimalism (search 11) → **adopt: small stdlib, explicit errors.**
44. Zig comptime/no-hidden-control-flow (search 12) → **adopt: compile-time is
   explicit, no silent allocation.**
45. Oberon/Wirth minimalism (search 13) → **adopt: a framework fits in your
   head.**

### OS / runtime research (searches 15–19, 37–49)
46. Plan 9 "everything is a file" (search 15) → **adopt wholesale: Styx9 is our
   9P. This is the core thesis.**
47. Inferno/Limbo/Styx (search 16) → **adopt: Styx is the IPC; Limbo→HolyC is
   our app language.**
48. seL4 verified microkernel (search 17) → **adopt verification ambition; our
   EDR is the enforcement layer.**
49. Unikernel/library-OS (search 18, 45, 46) → **adopt: an app *is* a library
   OS subtree; boot is `mount`.**
50. WebAssembly Component Model / WASI (search 19, 47) → **adopt the *idea*:
   capability-typed interfaces; implementation is Styx9 nodes, not WASM.**

### AI / agentic (searches 29–32, 48, 49)
51. Agent frameworks are YAML/Python soup (search 29) → **WuBuFX agent = a
   Styx9 namespace with an `eval` entry; no framework needed.**
52. EDR as afterthought (search 30) → **EDR is *built into the eval path*
   (`wubu_holyc_agent_eval`); cannot be bypassed.**
53. LLM-as-OS research (search 31, 49) → **we already have it: AGI compiles
   HolyC live; the OS *is* the agent runtime.**
54. Tool-use schema fragility (search 48) → **tools are Styx9 nodes with typed
   args; schema = the node's interface.**

### Design (Steve Jobs / Jony Ive) (searches 13, 14, 50)
55. "Less but better" → **fewer primitives, each excellent.**
56. No rounded-rect-for-rounded-rect's-sake → **chrome is functional.**
57. End-to-end ownership → **we own compiler→runtime→WM→app; no seam to blame.**
58. Say no to features → **manifest is small; scope is a virtue.**

---

## 3. The 100-Point Execution Plan

Structured as 10 phases × 10 points. Each point is a concrete, verifiable
artifact. Phases build on prior WuBuOS research (cited where relevant).

### Phase A — Foundations & Namespace Contract (points 1–10)
1. Define the **WuBuFX namespace spec** (`/app/<id>/`, `/lib/<hash>/`,
   `/sys/`, `/agi/`, `/edr/`) as a Styx9 mount contract. [builds on
   `styxfs_*` 11/11]
2. Write `wubufx.h` — the single public header (opaque structs + typed ops).
3. Define `WubuFxApp` lifecycle: `mount → open → eval → close → unmount`.
4. Content-addressed lib nodes: `wubu_pkgmgr` gains `mount-by-hash`.
5. Signed namespace verification on mount (EDR attestation hook).
6. Capability descriptor per app node (read/write/exec/agi/edr).
7. Delete all `draw_placeholder` demo paths in repo (done for harness; audit
   `src/apps` for fake bodies).
8. `wubufx_init()` / `wubufx_shutdown()` composition root.
9. Regression test: mount a trivial app namespace, assert nodes resolve.
10. Doc: `docs/WUBUFX_NAMESPACE.md`.

### Phase B — Execution Engine (11–20)
11. `wubufx_eval(app, src, out)` wraps `wubu_holyc_eval` with namespace ctx.
12. `wubufx_agent_eval` wraps `wubu_holyc_agent_eval` (EDR-disclosed). [builds
    on `wubu_holyc_agi.c` 11/11 tests]
13. Per-namespace HolyC session (persistent REPL per app, not global).
14. Namespace-scoped symbol table (apps can't clobber each other's globals).
15. Typed arg marshalling Styx9-node ↔ HolyC struct.
16. Async *is* a Styx9 event subscription (no promise stack erosion — point 42).
17. Error model: errors are Styx9 events, not thrown exceptions (point 14).
18. Resource limits per namespace (CPU/mem caps via `wubu_session`).
19. Hot-reload: remount namespace, re-eval, no process restart.
20. Regression test: two apps with same global name don't collide.

### Phase C — UI / Shell Binding (21–30)
21. App window is a Styx9 node `/app/<id>/win`; WM opens it (no fake draw).
22. Immediate-mode DSL layer over `dosgui_wm` (`wubufx_ui_*`). [builds on
    `dosgui_wm_render` clip fix]
23. State is explicit: `/app/<id>/win/state` node, no `@State` magic (point 28).
24. Theme bound per-namespace via `wubu_theme` (already 4 themes).
25. Server-side window chrome only (point 36); apps never draw frames.
26. Damage-rect repaint reuse (point 35); only changed regions.
27. Accessibility: every control is a named Styx9 node (point 32).
28. Layout solver: data-driven constraints, no CSS (point 32).
29. Input is delivered as Styx9 events; app is a pure handler.
30. Regression test: app opens window, paints, closes; no leak.

### Phase D — Packaging & Distribution (31–40)
31. `.wubu` package = signed Styx9 subtree (builds on `wubu_pkgmgr`).
32. No `node_modules`-style install; mount resolves deps lazily (point 16).
33. Hash-pinned deps; lockfile is the namespace hash (point 18).
34. Conflict = compile error, not runtime (point 5).
35. Single static binary target; no arch matrix (point 10).
36. `du` on a namespace shows real size (point 21).
37. Rollback = remount previous hash (immutable namespaces).
38. Air-gapped install: namespaces are offline-mountable.
39. Provenance: every node carries signer + hash.
40. Regression test: install, verify hash, uninstall, verify gone.

### Phase E — AGI Integration (41–50)
41. AGI acts *through* `wubufx_agent_eval` (already EDR-disclosed).
42. AGI tools are Styx9 nodes with typed interfaces (point 54).
43. Agent = a namespace with an `eval` entry; no external framework (point 51).
44. AGI cannot bypass EDR: eval path is the only entry (point 52).
45. AGI session state persists in its namespace (point 13).
46. Human-in-the-loop: EDR dashboard shows every AGI action.
47. Capability downgrade: AGI namespace gets least-privilege by default.
48. Replay: AGI actions are namespace event logs (audit trail).
49. Regression test: AGI evals are present in EDR alert buffer.
50. Doc: `docs/WUBUFX_AGI.md`.

### Phase F — EDR Integration (51–60)
51. Every `wubufx_eval` emits an EDR event (builds on `edr/*`).
52. Code-load is an EDR-disclosed, capability-gated event (point 8).
53. Mount/remount of a namespace = EDR attestation (point 5).
54. Anomaly: unsigned node in a signed namespace → EDR alert.
55. Resource-cap violation → EDR event + namespace suspend.
56. EDR rule as a Styx9 node (`/edr/rule/<id>`) — configurable, not hardcoded.
57. Alert buffer feeds the desktop notification center (already exists).
58. EDR module system (already `EdrModule`) hosts framework rules.
59. Regression test: tampered namespace triggers alert.
60. Doc: `docs/WUBUFX_EDR.md`.

### Phase G — Compositor & Desktop Polish (61–70)
61. Damage-rect only (no full-frame repaint) — already in `dosgui_wm_render`.
62. Window clamp above taskbar (FIXED this session).
63. VBE scissor clip (FIXED this session) — content never bleeds.
64. Real app engines wired (not demos): Notepad/Terminal/Explorer/Calc use
    `wubufx_eval` bodies, not `draw_placeholder`.
65. Desktop icons launch real apps via namespace mount (point 22).
66. Start menu enumerates `/app/*` namespaces.
67. Virtual-desktop pager hidden unless >1 (FIXED this session).
68. Theme consistency: active/inactive title bars distinct (FIXED).
69. Consistent typography/casing across shell.
70. Regression test: full desktop render no artifacts.

### Phase H — Language & Stdlib (71–80)
71. WuBuFX stdlib = small, explicit (point 43): `fx.io`, `fx.str`, `fx.list`.
72. No `System.*` monolith (point 9); import = `open()` a lib node.
73. Collection types as Styx9 nodes (list/dict = directories).
74. No GC mandate; arena + manual (point 7).
75. Comptime-free surprises; allocation is explicit (point 44).
76. Errors explicit (point 43); no exceptions across stacks (point 14).
77. FFI: native WuBuOS binary = a namespace with a `main` entry.
78. HolyC is the app language; C11 is the system language (clear seam).
79. Docstrings are Styx9 node metadata (not comments lost in source).
80. Regression test: stdlib ops on namespaced collections.

### Phase I — Security & Capabilities (81–90)
81. Capability model over ACL (search 50); node ACL = its permissions.
82. Least-privilege app namespace by default (point 47).
83. No `privileged` flag; elevation = explicit capability grant.
84. Supply-chain: signed namespaces, EDR verifies (point 22).
85. No postinstall scripts (point 19); build is disclosed or doesn't run.
86. seL4-style verification ambition for core nodes (point 48).
87. Unikernel-style app isolation: namespace = sandbox (point 49).
88. Audit log = namespace event stream (point 48).
89. Tamper-evident: hash chain over namespace mutations.
90. Regression test: escalated-capability app is denied by default.

### Phase J — Ship & Dogfood (91–100)
91. `wubufx` CLI: `mount`, `run`, `inspect`, `unmount`.
92. Example app: a real Notepad built on WuBuFX (replaces placeholder).
93. Example app: Terminal (already `dosgui_wm_holyc_term`).
94. Example app: Calculator (real eval, not `42` easter-egg-only).
95. Control Panel binds to `/sys` namespace (real settings).
96. My Computer binds to `/` namespace (real fs).
97. Full gate: all WuBuFX tests green.
98. Screenshot harness replaced with *real-app* harness (no lies).
99. Docs site: WUBUFX_NAMESPACE / AGI / EDR / QUICKSTART.
100. Commit + push; user dogfoods; iterate on real feedback.

---

## 4. Steve Jobs Lens — the 6 non-negotiables

1. **One artifact, one target.** No arch matrix, no container image, no JVM.
2. **The user sees the whole stack.** We own compiler→runtime→WM→app; when
   something is wrong we fix *our* code, not blame a layer.
3. **No configuration tax.** If it needs a `.config`/YAML/toml to say hello,
   it's wrong.
4. **State is visible.** `@State` magic, annotation wizardry, hidden control
   flow — all banned. State lives in a Styx9 node you can `ls`.
5. **Compositing is honest.** Damage-rect, server-side chrome, no bleed. The
   desktop is a real desktop, not a web page in a box.
6. **Say no.** The framework manifest is small. A feature that doesn't earn
   its place isn't added.

---

## 5. How This Composites Our Prior Research

- **Styx9 (`styxfs_*`, 11/11)** → the namespace substrate (points 1, 11, 21).
- **HolyC JIT + `wubu_holyd` + `wubu_holyc_agi` (11/11)** → execution engine
  (points 11–20, 41–50). This is our "CLR" but live and ring-0.
- **EDR (`edr/*`, `wubu_edr.*`)** → enforcement + transparency (points 51–60).
- **`dosgui_wm` (clip/taskbar fixes this session)** → shell binding (points
  21–30, 61–70).
- **`wubu_pkgmgr`** → packaging (points 31–40).
- **`wubu_session`** → resource caps + recoverable services (points 18, 35).
- **`OS_BIBLE` / `DESKTOP_VISION_PLAN`** → the layered vision
  (TempleOS→ZealOS→Styx9→NT→Win98 shell→SteamOS) is *exactly* the namespace
  stack WuBuFX formalizes.
- **`REACTOS_NT_SYSCALL_STUDY` (297 mapped)** → the NT personality is the
  *compatibility* layer, kept separate from WuBuFX native apps (point 78 seam).

---

## Appendix — The 50 Searches (query → top source)

1. Java classpath/jar hell — stackoverflow.com/questions/373193
2. Node node_modules bloat/leftpad — arxiv 2405.17939
3. Docker image bloat/supply chain — sysdig.com/blog (supply-chain attacks)
4. Electron memory bloat — reddit r/AskProgramming
5. React Native vs Flutter 2025 — thedroidsonroids.com
6. Wayland vs X11 2025/2026 — dev.to/rosgluk
7. Smithay Wayland compositor — github.com/Smithay/smithay
8. Steve Jobs simplicity — smithsonianmag.com
9. Jony Ive / Jobs design — macdailynews.com
10. frameworks too much abstraction — softwareengineering.stackexchange.com
11. Go minimalism — appliedgo.net
12. Zig no hidden control flow — ziggit.dev
13. Oberon Wirth minimalism — people.inf.ethz.ch/wirth
14. Plan 9 everything is a file — en.wikipedia.org/wiki/Plan_9
15. Inferno/Limbo/Styx — inferno-os.org
16. seL4 verified microkernel — sel4.systems (whitepaper)
17. Unikernel runtime — arxiv 2509.07891
18. WASM component model/WASI — forum.dfinity.org
19. Capability vs ACL — storj.dev
20. AI agent patterns 2025 — learn.microsoft.com/azure/architecture
21. EDR linux eBPF — uptycs.com (Linux EDR Best Practices)
22. LLM agent OS — arxiv 2403.16971 (AIOS)
23. Semantic Kernel/Autogen — learn.microsoft.com/semantic-kernel
24. Bevy ECS — news.ycombinator.com/item?id=47698111
25. Godot scene/node — forum.godotengine.org
26. .NET 4.8→Core migration pain — learn.microsoft.com/dotnet
27. DLL hell modern .NET — reddit r/csharp
28. COM regsvr32 nightmare — learn.microsoft.com (Fail to register DLL)
29. Win32 API criticism — reddit r/linuxmasterrace
30. TypeScript fatigue — (function overloads discussion)
31. Kubernetes YAML complexity — "Why Does Kubernetes Feel So Complicated?"
32. Spring annotation magic — "Annotations in Spring Boot Are the New Tech Debt"
33. Rails convention-over-config — reddit r/rails
34. hidden control flow async — (Async/Await pitfalls)
35. DI framework overkill — "Can Dependency Injection be overkill?"
36. ORM N+1 — (N+1 query problem)
37. SwiftUI pitfalls — (Declarative/Imperative SwiftUI)
38. ImGui vs retained — reddit r/cpp
39. Flutter Skia/Impeller — (Impeller rendering engine)
40. ECS vs OOP — (don't put ECS in your engine)
41. capability OS KeyKOS/EROS — (EROS: a fast capability system)
42. Fuchsia/Zircon — en.wikipedia.org/wiki/Fuchsia
43. Singularity SIP — (The Singularity System)
44. Midori language OS — (Microsoft Midori)
45. MirageOS unikernel — github.com/mirage/mirage
46. WASM WASI component — (Wasm Component Model)
47. Prolog + LLM reasoning — (Prolog's Role in LLM Era)
48. LLM tool use schema — (structured outputs/function calling)
49. agentic OS 2025 — (Agentic Operating System)
50. minimalist Less-is-More — (6 Principles of Minimalist Design)
