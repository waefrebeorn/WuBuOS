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
| Agentic-Corpus (AG) | `docs/compendium/04-roadmap/agentic-corpus-bank.md` | 0 wired | 1000 |
|| DevTools (DT) | `docs/compendium/04-roadmap/devtools-bank.md` | closing (200 wired) | 1000 |
|| Media (MD) | `docs/compendium/04-roadmap/media-bank.md` | open | 1000 |
|| **TOTAL** | | | **~11243** |

Path to 25,000: 14 more thousand-gap avenues (Games,
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
| Kaze Emanuar: FIXING the ENTIRE SM64 Source Code (transcript) | `kaze-fixing-entire-sm64-source.md` | 19308 | AGI-Design |
| Kaze Emanuar: SM64 Audio Optimization (transcript) | `kaze-sm64-audio-optimization.md` | 12953 | AGI-Design |
| Kaze Emanuar: the N64 optimization corpus (catalog) | `kaze-emanuar.md` | 3750 | AGI-Design |
| Wavetable Synthesis (WolfSound) | `wavetable-synthesis.md` | 26421 | Synthesis |
| Kernel Security Review (MDPI) | `kernel-security-review.md` | 119215 | Security |
| Memory Safety Continuum (OpenSSF) | `memory-safety-continuum.md` | 8167 | Security |
| Supply Chain 2026 (Cloudsmith) | `supply-chain-2026.md` | 23981 | Security |
| LLVM CAS build caching (RFC 59864) | `llvm-cas-build-caching.md` | 13799 | DevTools |
| rr record/replay debugging (arXiv 1705.05937) | `rr-record-replay-debugging.md` | 7606 | DevTools |
| Tree-sitter vs LSP (+ Roslyn HN notes) | `tree-sitter-vs-lsp.md` | 6530 | DevTools |
| Comprehensive Fuzzing Guide 2026 (46 sources) | `fuzzing-guide-2026.md` | 117530 | DevTools |
| AV1 State 2026 (Forasoft) | `av1-state-2026.md` | 60855 | Media |
| Neural Audio Codecs RVQ (Forasoft) | `neural-audio-codecs-rvq.md` | 39975 | Media |
| Ultra-Fast Neural Video Compression (CVPR 2026) | `ultra-fast-neural-video-compression.md` | 60642 | Media |

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
- The Media avenue (2026-08-03): AV1/SVT-AV1 encoder optimization (2026
  state: SVT-AV1 v4.0.0, preset 4-6 VOD, preset 12 real-time, 40% faster) ->
  Opus/LC3plus low-latency audio (RFC 6716, 22.5ms delay, Bluetooth LE Audio) ->
  neural audio codecs (SoundStream RVQ, EnCodec, TQCodec arXiv 2603.01592,
  streamable NC arXiv 2504.06561) -> GStreamer zero-copy pipelines (dmabuf,
  V4L2, KMS) -> learned neural video compression (CVPR 2026 Ultra-Fast NVC,
  rate-distortion autoencoders, scene-adaptive NVC) -> the WuBuOS media
  substrate (Bonzi player, Colonel video pipeline, hosted 9P media namespace).

## The closing ledger (the loop's own numbers)

- The recursive loop: closes the wubuwizard INDEX (1243) + the WT/GU wired
  gaps; the FS/NW/KR/AIE/HX banks are queued; the DT bank is closing;
  the MD (Media) bank is the newest avenue (1000 gaps, open).
- The DA-3 meta-plan: `wubuwizard/research/049-triple-da-metaplan.md`.
- The rules: honest-open (never fabricate), DA after every batch, the
  ledger-flip in the same commit, the sibling-collision protocol.
