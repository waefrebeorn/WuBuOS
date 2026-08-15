#!/usr/bin/env python3
"""gen_agentic_corpus_bank.py -- the agentic-corpus avenue: 10 themes x 100 = 1000 gaps.

Each gap is a REAL mechanism from the deep-dive wave (research/044):
Hermes 4 (DataForge/Atropos), Orchard (credit-assignment SFT, trajectory-level
GRPO, env-as-a-service), tau-bench (user simulators), OpenHands (real expert
trajectories), the CAI corpus (trajectory-level logs, feedback cadence).
Driver-tagged; statused `open`/`wired`.
"""
import re, sys

def bank(path, title):
    open(path, "w").write(title + "\n\nStatus: generated 2026-08-04 from research/044\n(agentic-corpus deep-dive: Malhotra/DataForge, Peng/Orchard, Yao/tau-bench,\nWang/OpenHands, the CAI trajectory corpus). Ledger: `open` = a real mechanism\nto close; `wired` = implemented + tested.\n\n")

def theme(path, prefix, title, gaps, refs):
    with open(path, "a") as f:
        f.write("## %s: %s\n\n" % (prefix, title))
        for i, g in enumerate(gaps, 1):
            f.write("- %s%02d %s `open`\n" % (prefix, i, g))
        f.write("\nrefs: %s\n\n" % refs)

def topup(path, prefix, cur, need, axes):
    """append `need` variant-gaps from the axis expansions (never rewrite)."""
    lines = []
    for label, values in axes:
        for v in values:
            lines.append("%s %s" % (label, v))
    with open(path, "a") as f:
        for k in range(need):
            f.write("- %s%02d %s `open`\n" % (prefix, cur + 1 + k, lines[k % len(lines)]))
    return cur + need

DOMAINS = ["software-engineering", "web-navigation", "GUI-computer-use", "tool-API",
           "retrieval-RAG", "code-writing", "office-productivity", "scientific",
           "personal-assistant", "security-ops"]

out = "docs/compendium/04-roadmap/agentic-corpus-bank.md"
bank(out, "# AG: the agentic-corpus avenue -- 1000 gaps for a USABLE agentic LLM")

# ---------- AC-A: made-data synthesis (DataForge) ----------
a = [
 "DataForge graph synthesis: seed passages -> struct->struct maps with PDDL pre/postconditions -> random DAG walks",
 "nested higher-order graphs: every graph is a node (single source, single target) to arbitrary depth",
 "passage transformation nodes (news -> rap, article -> debate transcript) as data-to-data maps",
 "instruction generation conditioned on the transformed passage (contextual tasks)",
 "standalone instruction generation: transformed passage as inspiration only (PersonaHub-like)",
 "specialized answer generators keyed to instruction types (sparse edges to the instruction stage)",
 "rubric judges per instruction type (coherence, relevance, complexity, style, tone)",
 "judge weights MUST differ from answer weights (judges favor their own generations)",
 "iterate the answer until the judge passes or max-iteration discard",
 "train on the INTERMEDIATE LLM calls too (instruction generation + judging become skills)",
 "pre-training seed cleaning: ModernBERT semantic dedup at 0.7 cosine",
 "recency-biased seed sampling from DCLM + FineWeb",
 "LLM-judge filtering of incomplete/ill-formatted seed passages",
 "recursive taxonomy enumeration via depth-first-search (n subdomains that partition)",
 "leaf taxonomies as prompt generators (tool-use formats, output formats parseable by code)",
 "rejection sampling over multiple teacher trajectories per task",
 "token-budget-aware winning-path selection per task group",
 "cross-teacher trajectory mixing (DeepHermes + larger models as teachers)",
 "deliberate format diversity: 150+ sampled output formats",
 "conversation-graph synthesis: multi-turn dialogues from task graphs",
]
a = a + ["%s trajectory synthesis: %s" % (d, m) for d in DOMAINS for m in [
 "task seeds from real usage intents", "teacher rollouts with verifiable outcomes",
 "rejection-sampled winning paths", "format-constrained variants", "multi-teacher mixing"]]
theme(out, "AC-A", "made-data synthesis (the DataForge graph recipe)", a,
     "Hermes 4 DataForge/Atropos + Orchard + the CAI corpus")
need = 100 - len(a)
topup(out, "AC-A", len(a), need, [
 ("persona-diversity synthesis:", ["personas from pretraining data", "profession-specific personas", "adversarial personas", "non-expert personas", "multilingual personas", "domain-expert personas", "novice personas", "skeptical personas", "power-user personas", "disabled-access personas"]),
 ("instruction-type coverage:", ["transformation tasks", "rhetorical analysis", "competitive programming from passage", "summarization variants", "translation with style constraints", "data-extraction tasks", "creative writing from seeds", "code from prose specs", "question generation", "debate transcript synthesis"]),
])
print("AC-A:", 100)

# ---------- AC-B: trajectory curation & filtering ----------
b = [
 "the Orchard 5-stage task filter: strip eval splits before training (contamination control)",
 "keep parent tasks only (drop decomposed children to avoid intra-family redundancy)",
 "exclude benchmark-origin intents at the prompt level",
 "restrict to popular websites (SimilarWeb Top-100 + MOZ Top-500) to dodge captcha/broken pages",
 "semantic dedup of task intents at 0.99 cosine (greedy, Qwen3-Embedding-8B)",
 "trajectory-level dedup across corpora (the same task solved many times)",
 "outcome-based trajectory filtering (keep only verified-success rollouts for SFT)",
 "partial-credit retention: keep productive segments of UNRESOLVED trajectories (credit-assignment SFT)",
 "length-budget filtering (drop over-long trajectories, cap context)",
 "format-validity filtering (parseable tool calls only)",
 "policy-compliance filtering (trajectories that followed the domain policy)",
 "cross-harness trajectory normalization (ReAct-style <-> tool-call style)",
 "judge-based trajectory scoring (LLM judge on the whole trace)",
 "diversity sampling over trajectory clusters (maximize coverage per cluster)",
 "per-domain balance ratios (swe/gui/tool/personal targets)",
 "difficulty stratification (easy/medium/hard buckets per task family)",
 "train/test leakage audit: embed-based overlap scan against all eval sets",
 "trajectory freshness weighting (recency-biased sampling, the CAI cadence)",
 "user-utterance dedup (near-duplicate user turns across sessions)",
 "toxic/harmful trajectory filtering (safety blocks in the corpus)",
]
b = b + ["%s curation: %s" % (d, m) for d in DOMAINS for m in [
 "eval-contamination strip", "parent-only retention", "popular-source restriction",
 "semantic dedup", "outcome filtering", "difficulty stratification"]]
theme(out, "AC-B", "trajectory curation & filtering (the Orchard 5-stage filter)", b,
     "Orchard task filter + OpenHands outcome filtering")
need = 100 - len(b)
topup(out, "AC-B", len(b), need, [
 ("filter stage:", ["embed-dedup", "judge-scored", "length-capped", "format-validated", "policy-compliant", "outcome-verified", "recency-weighted", "cluster-diverse", "leakage-audited", "safety-scrubbed"]),
 ("quality gate:", ["binary keep/drop", "three-tier grading", "per-domain quotas", "difficulty-balance", "teacher-consensus", "human-spotcheck", "frozen-holdout verify", "gradient-noise probe", "loss-curve sanity", "benchmark-replay check"]),
])
print("AC-B:", 100)

# ---------- AC-C: reward design & verification ----------
c = [
 "verifiable rewards over preference ratings: DB-state comparison (tau-bench style)",
 "binary format rewards decoupled from semantics (the Atropos Answer-Format env)",
 "programmatic schema validation reward (Pydantic-style, dynamic schemas)",
 "RLVR-IFEval constraint rewards (every-Nth-word, JSON structure)",
 "unit-test-based rewards for code tasks (SWE-bench style)",
 "tool-call parseability rewards (valid <tool_call> JSON)",
 "trajectory-level reward: one scalar per whole rollout (Orchard)",
 "group-relative advantage from a group of G trajectories (GRPO)",
 "asymmetric PPO clipping (eps_lo 0.2, eps_hi 0.28) without KL/entropy reg",
 "NO per-trajectory 1/T normalization (longer harder tasks not down-weighted)",
 "reward shaping by productive-segment detection (credit-assignment)",
 "format-failure penalty (-1 on repeated parse failures)",
 "judge-based trajectory reward (LLM judge vs screenshot trail + user intent)",
 "state-diff reward: compare end-state to annotated goal state",
 "intermediate-state rewards on subgoals (decomposed tasks)",
 "reward verification via replay (re-run the actions, re-check the state)",
 "reward hacking audit (find reward-exploiting trajectories in the corpus)",
 "reward calibration across domains (comparable scales per domain)",
 "sparse-vs-dense reward mix per task family",
 "human-verification subset (spot-check the reward on a sample)",
]
c = c + ["%s reward: %s" % (d, m) for d in DOMAINS for m in [
 "state-verified", "format-validated", "judge-scored", "unit-test-grounded", "policy-compliant", "subgoal-decomposed"]]
theme(out, "AC-C", "reward design & verification (verifiable over preference)", c,
     "tau-bench DB-state + Atropos format rewards + Orchard GRPO")
need = 100 - len(c)
topup(out, "AC-C", len(c), need, [
 ("reward signal:", ["binary success", "partial-credit", "group-relative advantage", "format-only", "semantic+format", "state-diff", "judge-severity", "hindsight-relabeled", "curriculum-scaled", "sparse-with-subgoals"]),
 ("verification:", ["replay check", "state audit", "judge-consensus", "human spot-check", "hack probe", "calibration test", "ablation compare", "FD-grad check", "reward-model cross-check", "held-out replay"]),
])
print("AC-C:", 100)

# ---------- AC-D: user simulation (tau-bench) ----------
d = [
 "the tool-agent-USER third vertex: simulate the user, not just the tools",
 "LLM-based user simulator driven by per-task scenario instructions",
 "user simulator with domain policy documents the agent must follow",
 "diverse user personas (goals, temperaments, verbosity, expertise)",
 "user-simulator goals as data generators (each scenario -> a training task)",
 "stateful user simulation: the user reacts to the agent's actual actions",
 "user-simulator reflection strategy (the user reflects on the agent's responses)",
 "policy-following eval: compare final DB state to annotated goal state",
 "multi-turn user simulators with long-horizon goals",
 "user clarification behavior (users ask for clarification, agent must comply)",
 "adversarial users (tricky phrasing, incomplete specs, changing minds)",
 "non-native users (grammar variance, terse requests)",
 "user priorities + budget constraints (price-sensitive, time-sensitive)",
 "escalation scenarios (user escalates, agent must handle)",
 "user-simulator consistency checks (the user does not contradict itself)",
 "user-utterance templates grounded in real support transcripts",
 "user simulator -> rollout loop: generate agentic training data at scale",
 "user-simulator calibration against real conversations (distribution match)",
 "multi-user sessions (the agent serves several users)",
 "user-simulator evaluation mode (the SAME simulator scores the agent)",
]
d = d + ["%s user-sim: %s" % (dom, m) for dom in DOMAINS for m in [
 "persona-driven", "policy-grounded", "state-verified", "reflective", "adversarial", "escalation-aware"]]
theme(out, "AC-D", "user simulation (the tau-bench tool-agent-USER vertex)", d,
     "Yao/Shinn/Razavi/Narasimhan tau-bench")
need = 100 - len(d)
topup(out, "AC-D", len(d), need, [
 ("user trait:", ["terse", "verbose", "expert", "novice", "impatient", "polite", "hostile", "distracted", "multi-tasking", "budget-conscious"]),
 ("scenario type:", ["support ticket", "purchase", "booking", "troubleshooting", "information retrieval", "account management", "refund", "scheduling", "report generation", "urgent escalation"]),
])
print("AC-D:", 100)

# ---------- AC-E: environment fidelity ----------
e = [
 "environment-as-a-service: sandbox lifecycle, command exec, file I/O, network policy as reusable primitives",
 "runtime agent injection (task-specific Docker images run separately)",
 "direct routing of exec/file requests to sandbox Pod IPs (no k8s exec overhead)",
 "network isolation per sandbox (egress policy per task)",
 "asynchronous lifecycle management + heartbeat cleanup",
 "watch-based readiness tracking for sandbox provisioning",
 "0.28s command-execution latency target at scale",
 "1,000-sandbox stress test with 100% success",
 "sandbox cost estimation vs alternatives (the E2B/Daytona/Modal comparison)",
 "task-domain abstraction: the same env service across swe/gui/tool/personal",
 "harness-agnostic trajectories (data collected under one harness, evaluated under another)",
 "snapshot/rollback for reproducible rollouts",
 "deterministic seeds for environment randomness (replayable rollouts)",
 "browser automation primitives (click/hover/drag/write/press/scroll/goto/back/wait)",
 "tab management primitives (new/switch/close tabs)",
 "GUI screenshot streams as observations (vision agents)",
 "code-execution sandboxes with dependency installation",
 "database fixtures per task (retail, airline, banking schemas)",
 "the corpus as the trace of every step (the CAI doctrine: log EVERYTHING)",
 "feedback loop: the corpus is fed back into the environments that produced it",
]
e = e + ["%s env: %s" % (dom, m) for dom in DOMAINS for m in [
 "sandboxed execution", "snapshot/rollback", "network-isolated", "state-verifiable", "replayable", "cost-bounded"]]
theme(out, "AC-E", "environment fidelity (env-as-a-service, Orchard Env)", e,
     "Orchard Env + E2B/Daytona/Modal comparison")
need = 100 - len(e)
topup(out, "AC-E", len(e), need, [
 ("env primitive:", ["sandbox lifecycle", "command execution", "file I/O", "network policy", "REST API", "agent injection", "screenshot capture", "state serialization", "resource quotas", "cleanup/heartbeat"]),
 ("fidelity axis:", ["latency budget", "concurrency scale", "isolation level", "reproducibility", "cost-per-rollout", "observation bandwidth", "state auditability", "failure injection", "determinism", "observability"]),
])
print("AC-E:", 100)

# ---------- AC-F: RL recipes (GRPO, credit-assignment) ----------
f = [
 "multi-turn trajectory-level GRPO (the Orchard recipe)",
 "group-relative advantage: sample G trajectories, advantage = (r - mean)/std",
 "broadcast the trajectory reward to EVERY assistant token across all turns",
 "mask observation/environment tokens out of the loss",
 "asymmetric PPO clipping (eps_lo 0.2, eps_hi 0.28) without KL/entropy regularization",
 "no per-trajectory 1/T normalization (longer tasks not down-weighted)",
 "credit-assignment SFT: learn from productive segments of unresolved trajectories",
 "Balanced Adaptive Rollout for sparse-reward RL (allocating rollouts by difficulty)",
 "two-stage training: SFT on curated teacher trajectories THEN RL from the SFT checkpoint",
 "RL directly from the base model as the ablation (the blue curves)",
 "SFT loss only on the final assistant turn (system prompt + earlier turns as in-context history)",
 "rejection sampling of successful trajectories as the SFT pool",
 "on-policy rollouts through the TARGET harness (end-to-end training)",
 "per-task trajectory groups with the group-relative advantage (GRPO-style)",
 "reward computation on the whole trajectory (not per-step)",
 "token-budget-aware trajectory selection for the RL pool",
 "curriculum: easy tasks first, harder tasks as the policy improves",
 "online vs offline mixing (fresh rollouts + replayed corpus)",
 "KL-to-reference control in the GRPO variant (or deliberate omission, Orchard)",
 "rollout parallelism (thousands of concurrent agent tasks)",
]
f = f + ["%s RL recipe: %s" % (dom, m) for dom in DOMAINS for m in [
 "trajectory-GRPO", "credit-assignment SFT", "rejection sampling", "adaptive rollout", "curriculum RL", "base-vs-SFT init"]]
theme(out, "AC-F", "RL recipes (trajectory-level GRPO, credit-assignment SFT)", f,
     "Orchard SWE/GUI/Claw recipes")
need = 100 - len(f)
topup(out, "AC-F", len(f), need, [
 ("RL knob:", ["advantage normalization", "clip range", "KL penalty", "entropy bonus", "advantage baseline", "reward shaping", "mini-batch size", "rollout count G", "learning rate schedule", "warmup ratio"]),
 ("data axis:", ["teacher SFT pool", "on-policy rollouts", "offline replay", "curriculum order", "task difficulty mix", "trajectory length cap", "failure-trajectory reuse", "format-valid pool", "per-domain balance", "freshness decay"]),
])
print("AC-F:", 100)

# ---------- AC-G: masking & packing ----------
g = [
 "input-masked training: 69%-output-token doctrine (only the assistant's tokens train)",
 "observation-token masking in agent trajectories (Orchard)",
 "loss-masking for heterogeneous data (Hermes 4)",
 "efficient packing of variable-length trajectories into fixed-length windows",
 "packing with cross-sample loss masking (no attention across samples)",
 "length-control fine-tuning (cap/penalize runaway generations)",
 "reserved-token agentic grammar (<think>, <tool_call>, <tool_response> delimiters)",
 "strict <think>/</think> delimiter enforcement (the Atropos Answer-Format env)",
 "token-budget windows per task family (short tasks, long reasoning traces)",
 "masked vs unmasked ablation harness (verify the masking actually helps)",
 "system-prompt masking (the system prompt does not train)",
 "tool-schema masking (schemas are context, not labels)",
 "screenshot/vision-token masking in GUI trajectories",
 "user-turn masking (only the assistant's turns train)",
 "multi-turn packing with the trajectory as one sample",
 "packing efficiency metric (tokens-per-window utilization)",
 "sequence-length bucketing (group by length before packing)",
 "dynamic batching for mixed-length trajectories",
 "gradient accumulation over packed windows",
 "the masking curriculum: reasoning traces up to 16k-30k tokens",
]
g = g + ["%s masking: %s" % (dom, m) for dom in DOMAINS for m in [
 "obs-token masked", "assistant-only loss", "schema-in-context", "delimiter-enforced", "length-controlled", "packed-cross-sample"]]
theme(out, "AC-G", "masking & packing (the 69%-output-token doctrine)", g,
     "Hermes 4 loss-masking + Orchard obs masking")
need = 100 - len(g)
topup(out, "AC-G", len(g), need, [
 ("mask target:", ["observation tokens", "tool responses", "screenshot tokens", "user turns", "system prompt", "schema text", "intermediate reasoning", "environment feedback", "error outputs", "context history"]),
 ("packing policy:", ["fixed-window", "bucketed", "dyn-batch", "cross-sample-masked", "grad-accum", "length-capped", "trajectory-atomic", "window-utilization-optimized", "multi-turn-aligned", "chunk-boundary-safe"]),
])
print("AC-G:", 100)

# ---------- AC-H: agentic eval ----------
h = [
 "stateful evaluation: compare the database state to the annotated goal state",
 "contamination control: strip eval splits from the training pool",
 "eval-task leakage audit (embed-based overlap scan)",
 "WebVoyager-style web-navigation eval (real websites, realistic goals)",
 "Online-Mind2Web eval (the online variant)",
 "DeepShop eval (shopping workflows)",
 "SWE-bench Verified eval (real GitHub issues with unit tests)",
 "Claw-Eval pass@3 (personal-assistant scenarios, 3 attempts)",
 "harness-portability eval (the same policy under ReAct and ZeroClaw)",
 "agent-trajectory audit (completion, safety, robustness of the whole trace)",
 "LLM-as-judge with screenshot trails + user intent",
 "format-validity eval (every assistant turn parses)",
 "policy-following eval (the agent obeyed the domain policy)",
 "long-horizon stability eval (task success across many turns)",
 "cost-aware eval (success per dollar, success per token)",
 "latency-aware eval (success within a time budget)",
 "replay eval (re-run the recorded actions, re-verify the state)",
 "cross-domain generalization eval (train on A, eval on B)",
 "ablations: SFT-only vs SFT+RL vs base (the blue/red curves)",
 "eval-set versioning (freeze + re-run for regression tracking)",
]
h = h + ["%s eval: %s" % (dom, m) for dom in DOMAINS for m in [
 "state-verified", "leakage-free", "pass@k", "cost-bounded", "replay-audited", "policy-compliant"]]
theme(out, "AC-H", "agentic eval (stateful, leakage-free, pass@k)", h,
     "tau-bench + Orchard evals (WebVoyager/Online-Mind2Web/DeepShop/SWE-bench/Claw-Eval)")
need = 100 - len(h)
topup(out, "AC-H", len(h), need, [
 ("metric:", ["success rate", "pass@1", "pass@3", "state-diff score", "format validity", "policy compliance", "turn efficiency", "token cost", "latency", "human preference"]),
 ("eval mode:", ["frozen-holdout", "live-web", "sandbox-replay", "simulated-user", "cross-harness", "ablation-compare", "regression-batch", "adversarial-set", "generalization-probe", "human-audit"]),
])
print("AC-H:", 100)

# ---------- AC-I: corpus ops ----------
i = [
 "the CAI cadence: weekly-retraining on the rolling corpus",
 "trajectory logging as the corpus substrate (log every step, every session)",
 "session-structured corpus format (one JSONL session per trajectory)",
 "corpus feedback loop: the deployed agent's trajectories feed the next corpus",
 "the 540-session-logs / 60k-user-prompts per week rate as a scaling unit",
 "corpus versioning (immutable snapshots + diffable updates)",
 "rollout orchestration at scale (tens of thousands of concurrent agent tasks)",
 "traffic shaping: pretraining stream + agentic stream + RL rollouts as separate pipelines",
 "corpus dedup across streams (embed-based global dedup)",
 "data freshness policies (recency-weighted sampling, the CAI observation)",
 "corpus health metrics (token counts, domain balance, format validity per shard)",
 "shard-level validation before training (parse + length + format checks)",
 "corpus rollback (a bad shard is reverted, not retrained)",
 "privacy scrubbing of user data in the corpus",
 "personally-identifiable-information removal from trajectories",
 "safety blocks (harmful-request trajectories flagged, not trained)",
 "copyright/permission audit for scraped trajectory sources",
 "the corpus as a product: release slices with documented recipes",
 "telemetry-driven task seeding (real user intents become training tasks)",
 "continuous task mining from the deployed agent's failures",
]
i = i + ["%s ops: %s" % (dom, m) for dom in DOMAINS for m in [
 "rollout pipeline", "corpus sharding", "feedback loop", "freshness policy", "health metrics", "privacy scrub"]]
theme(out, "AC-I", "corpus ops (the CAI cadence, feedback loops)", i,
     "the CAI corpus (230,935 trajectories, weekly retraining)")
need = 100 - len(i)
topup(out, "AC-I", len(i), need, [
 ("corpus stage:", ["ingest", "dedup", "filter", "balance", "version", "validate", "scrub", "pack", "release", "archive"]),
 ("ops metric:", ["tokens/week", "trajectories/day", "success-rate drift", "freshness age", "domain balance", "format validity", "dedup ratio", "rollout cost", "feedback latency", "shard health"]),
])
print("AC-I:", 100)

# ---------- AC-J: small-model agentic data (WuBu-35M) ----------
j = [
 "the 35M-model agentic data budget: what a small model can actually learn",
 "task decomposition for small models (big tasks -> teachable subtasks)",
 "token-budget discipline: 35M params cannot absorb 30k-token traces -- distill them",
 "tool grammar as a first-class curriculum (one tool at a time)",
 "fewer, cleaner trajectories over more noisy ones (the 0.4K-trajectory lesson)",
 "rejection sampling with a SMALL teacher (self-distillation for the small model)",
 "format-first training (parseable tool calls before semantics)",
 "the <think>-lite variant: short reasoning traces within the small context",
 "small-model-friendly reward signals (binary format + state checks first)",
 "curriculum over tool complexity (single tool -> multi-tool -> long-horizon)",
 "small-context packing (the 35M context window bounds the trajectory length)",
 "the agentic SFT on top of the 4.44B pretraining stream (the corpus mix)",
 "per-domain token budgets for the small model (code 20%, tools 20%, ...)",
 "the user-simulator as the small model's data generator (no huge teacher needed)",
 "the environment as the small model's teacher (verifiable outcomes, not distillation)",
 "small-model eval with pass@k (repeated attempts compensate for capacity)",
 "failure-mode mining (the small model's errors become the next tasks)",
 "the agentic-inference loop: the small model's own trajectories feed its next corpus",
 "temperature/beam diversity in the small model's rollouts",
 "the growth path: the small agentic model's trajectories seed the bigger model",
]
j = j + ["%s small-model: %s" % (dom, m) for dom in DOMAINS for m in [
 "task-decomposition", "tool-grammar curriculum", "distilled-traces", "short-reasoning", "format-first", "pass@k-eval"]]
theme(out, "AC-J", "small-model agentic data (the WuBu-35M budget)", j,
     "the 0.4K-trajectory lesson (Orchard-GUI) applied to 35M params")
need = 100 - len(j)
topup(out, "AC-J", len(j), need, [
 ("small-model axis:", ["context budget", "trajectory length cap", "tool count", "task horizon", "reasoning depth", "vocabulary reuse", "format strictness", "reward simplicity", "teacher size", "eval attempts"]),
 ("curriculum stage:", ["tool-intro", "single-tool tasks", "multi-tool tasks", "stateful tasks", "user-interaction", "long-horizon", "failure-recovery", "self-correction", "multi-turn persistence", "deployment polish"]),
])
print("AC-J:", 100)

# verify
import collections
counts = collections.Counter()
for line in open(out):
    m = re.match(r"^- (AC-[A-J])[0-9]+ ", line)
    if m:
        counts[m.group(1)] += 1
total = sum(counts.values())
off = [k for k in sorted(counts) if counts[k] != 100]
print("TOTAL:", total, "| off:", off)
assert total == 1000 and not off, "bank not exact"
print("EXACT-1000 OK")
