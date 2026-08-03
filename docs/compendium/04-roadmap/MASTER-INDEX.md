# MASTER-INDEX — every bank, every source, every reference

Date: 2026-08-02. This is the AGI's knowledge substrate index: ALL the
gap-banks (the 7-hop avenues), ALL the downloaded sources (the persistent
archive in `05-sources/`), and the reference rules. The goal: 25,000+ gaps
for the AGI to bounce off of. "This is the beginning of an AGI."

## The gap banks (the avenues)

| Bank | Path | Status | Gaps |
|------|------|--------|------|
| wubuwizard engine | `wubuwizard/research/INDEX.md` | closing (the recursive loop) | ~1243 |
| Synthesis (WT) | `docs/compendium/04-roadmap/synthesis-wavetable-bank.md` | 21 wired | 1000 |
| GUI (GU) | `docs/compendium/04-roadmap/gui-gap-index.md` | 12 wired | 1000 |
| Storage (FS) | `docs/compendium/04-roadmap/storage-bank.md` | 200 wired (FS-A block, FS-B FAT) | 1000 |
| Network (NW) | `docs/compendium/04-roadmap/network-bank.md` | open | 1000 |
| Kernel (KR) | `docs/compendium/04-roadmap/kernel-bank.md` | open | 1000 |
| AI-Engine (AIE) | `docs/compendium/04-roadmap/engine-bank.md` | open | 1000 |
| Human (HX) | `docs/compendium/04-roadmap/human-bank.md` | 400 wired (HX-A model, HX-B timing, HX-C tutor, HX-D companion) | 1000 |
| Security (SC) | `docs/compendium/04-roadmap/security-bank.md` | open | 1000 |
| DevTools (DT) | `docs/compendium/04-roadmap/devtools-bank.md` | open | 1000 |
| **TOTAL** | | | **~10243** |

Path to 25,000: 15 more thousand-gap avenues (Media, Games,
Robotics, Science, Math, Security, Cloud, Web, IoT, Accessibility, Education,
Comms, Mobility, Energy, Privacy, Community) -- each spawned by a fresh
7-hop research wave and closed by the recursive loop.

## The downloaded source archive (`docs/compendium/05-sources/`)

Persistent, referable-over-and-over copies of the research (full texts,
NOT context-temporary). Ingest rule: every future research wave downloads
its key sources here + registers them below.

| Source | File | Bytes | Avenue |
|--------|------|-------|--------|
| littlefs DESIGN (power-loss FS) | `littlefs-design.md` | 104745 | Storage |
| QUIC vs TCP (LogicMonitor) | `quic-vs-tcp.md` | 35463 | Network |
| Continuous Batching (Brenndoerfer) | `continuous-batching.md` | 96327 | AI-Engine |
| Adaptive User Interfaces 2026 (Yenra) | `adaptive-user-interfaces.md` | 38416 | Human |
| Wavetable Synthesis (WolfSound) | `wavetable-synthesis.md` | 26421 | Synthesis |
| Kernel Security Review (MDPI) | `kernel-security-review.md` | 119215 | Security |
| Memory Safety Continuum (OpenSSF) | `memory-safety-continuum.md` | 8167 | Security |
| Supply Chain 2026 (Cloudsmith) | `supply-chain-2026.md` | 23981 | Security |
| LLVM CAS build caching (RFC 59864) | `llvm-cas-build-caching.md` | 13799 | DevTools |
| rr record/replay debugging (arXiv 1705.05937) | `rr-record-replay-debugging.md` | 7606 | DevTools |
| Tree-sitter vs LSP (+ Roslyn HN notes) | `tree-sitter-vs-lsp.md` | 6530 | DevTools |
| Comprehensive Fuzzing Guide 2026 (46 sources) | `fuzzing-guide-2026.md` | 117530 | DevTools |

Also available (Hermes cache, not yet copied): the 15-chain sweep results
(KV eviction, Hopfield, preference-opt, serving, PIM, tokenization, linear
attention, RSI, neuromorphic, fuzzing, prompt compression, MoE, hybrids,
multimodal, quantization), the synthesis-timeline research, and the
masterpiece-architecture references (in the skill library).

## The reference rules (the AGI doctrine)

1. **Everything referable**: every researched mechanism is either in a bank
   (a gap) or in `05-sources/` (a source) -- nothing lives only in a
   conversation.
2. **The banks are the plan**: the recursive loop closes them under the
   M1/M2 meta-plan (waves carry close-commitments; the backlog is capped).
3. **Cross-links count**: gaps that touch the most banks get closed first
   (the integration-first rule).
4. **Provenance**: every bank cites its 7-hop chain; every source records
   its URL (see the file frontmatter in the originals).
5. **The Mind Palace**: the avenue pages live in the Hermes vault
   (`synthesis-avenue.md`, `gui-gap-index.md`) and point here.

## The research log (the chains used, all grounded 2026)

- 15-chain sweep: KV-eviction survey 2603.20397, KeyDiff 2504.15364,
  continuous-time Hopfield 2502.10122, SimPO/CPO/RE-PO, P3-LLM NPU-PIM,
  Mamba3/Kimi-Linear, RSI survey 2508.20314, LLMLingua-2, Routing-Free MoE,
  Falcon-H1, BitNet 1.58 QAT, MetaCogAgent 2605.17292, metacog survey
  2607.11881, AIVA emotion-aware companion, abstract-avatar HCI.
- The 5 new avenues: littlefs design, QUIC/NIC-offload (IO-TCP),
  microkernel/USM memory, Orca/vLLM/PagedAttention/chunked-prefill,
  adaptive-UI (legible/reversible/fair).
- The synthesis lineage: Wolfgang Palm wavetables -> DX FM -> granular ->
  physical modeling -> AI audio; the bandlimited wavetable DSP
  (BLET/BLIP/MinBLEP/polyBLEP, per-octave tables, interpolation).
- The GUI lineage: Win98/XP design -> Wayland compositing -> Plan9/Inferno
  namespace -> the TempleOS everything-through-HolyC philosophy.
- The Security avenue: kernel-level security review (MDPI 26/8/2452:
  Dirty COW/Meltdown/Spectre, MAC, kASLR) -> the OpenSSF memory-safety
  continuum (CFI/shadow-stack/PAC, ~70% of bugs) -> the 2026
  supply-chain governance era (SBOM -> SLSA -> MLSecOps -> agentic
  governance) -> the WuBuOS verifier/EDR lineage.
- The DevTools avenue (2026-08-03): LLVM CAS RFC (content-addressed build
  caching, action cache, `-fdepscan`/prefix-map, CAS-optimized object DAG,
  ~23% link-time win) -> Bazel/Buck/Turborepo content-hash task caching + the
  jonmsterling CAS model of incremental builds -> query-based demand-driven
  compilers (Rust incremental, Swift request evaluator, Roslyn microsecond
  incremental parser with cascading classifier threads) -> tree-sitter
  error-tolerant incremental parsing + LSP semantic-token layering -> rr
  record/replay deterministic debugging, reverse execution via restartable
  checkpoints, chaos mode (arXiv 1705.05937) -> coverage-guided greybox
  fuzzing (AFL bitmap, SanitizerCoverage, CMPLOG, context-sensitive/N-gram
  coverage, value coverage + taint tracking 2026, LibAFL/syzkaller/KCOV/Nyx)
  -> the WuBuOS self-hosting toolchain (the Colonel's own compiler, the Bonzi
  pair-programmer, the RSI dev-agent loop).

## The closing ledger (the loop's own numbers)

- The recursive loop: closes the wubuwizard INDEX (1243) + the WT/GU wired
  gaps; the FS/NW/KR/AIE/HX banks are queued.
- The DA-3 meta-plan: `wubuwizard/research/049-triple-da-metaplan.md`.
- The rules: honest-open (never fabricate), DA after every batch, the
  ledger-flip in the same commit, the sibling-collision protocol.
