Title:

Content selection saved. Describe the issue below:

Description:

![](https://arxiv.org/static/base/1.0.1/images/icons/smileybones-small.svg)arXiv is now an independent nonprofit! [Learn more](https://info.arxiv.org/about) ×

[License: CC BY 4.0](https://info.arxiv.org/help/license/index.html#licenses-available)

arXiv:2605.15040v1 \[cs.AI\] 14 May 2026

# Orchard: An Open-Source Agentic Modeling Framework

Baolin Peng†\\dagger1 🖂,
Wenlin Yao†\\dagger1,
Qianhui Wu†\\dagger1,
Hao Cheng†\\dagger1

Xiao Yu‡\\ddagger2,
Rui Yang‡\\ddagger3,
Tao Ge1,
Alessandro Sordoni1,
Xingdi Yuan1,
Yelong Shen1

Pengcheng He1,
Tong Zhang3,
Zhou Yu2,
Jianfeng Gao1 🖂

1Microsoft Research  2Columbia University  3UIUC

{baolinpeng,jfgao}@microsoft.com

[GitHub](https://github.com/microsoft/Orchard "")![[Uncaptioned image]](https://arxiv.org/html/2605.15040v1/figures/hf-logo.png) [Hugging Face](https://huggingface.co/datasets/microsoft/Orchard "")

###### Abstract

Agentic modeling aims to transform large language models (LLMs) into autonomous agents that can solve complex tasks through planning, reasoning, tool use, and multi-turn interaction with external environments.
Despite substantial investment, open research in this area remains constrained by infrastructure and training gaps. Many high-performing agentic systems rely on proprietary codebases, models, or services, whereas open-source frameworks focus primarily on agent orchestration and harness design rather than improving agentic capabilities of LLMs through scalable model training.
We present Orchard, an open-source framework for scalable agentic modeling.
At its core is Orchard Env, a thin, Kubernetes-native environment service that provides reusable primitives for sandbox lifecycle management.
Orchard Env is designed to operate across task domains, agent harnesses, and pipeline stages – including trajectory distillation, on-policy reinforcement learning (RL) rollouts, and evaluation.
On top of Orchard Env, we build three agentic modeling recipes.
Orchard-SWE targets software-engineering agents: we distill 107K trajectories from MiniMax-M2.5 and Qwen3.5-397B, introduce credit-assignment supervised fine-tuning (SFT) to learn from productive segments of unresolved trajectories, and apply Balanced Adaptive Rollout for sparse-reward RL.
With Qwen3-30B-A3B-Thinking, Orchard-SWE achieves 64.3% on SWE-bench Verified after SFT and 67.5% after SFT+RL, setting a new state of the art among open-source models of comparable size.
Orchard-GUI trains a 4B vision-language computer-use agent with only 0.4K distilled trajectories and 2.2K open-ended training tasks. It achieves success rates of 74.1%, 67.0%, and 64.0% on WebVoyager, Online-Mind2Web, and DeepShop, respectively (68.4% average), making it the strongest open-source model while remaining competitive with proprietary systems from OpenAI and Google Gemini.
Orchard-Claw targets personal assistant agents for productivity workflows such as email, calendar, and daily tool-use tasks. Trained with only 0.2K synthetic tasks, it achieves 59.6% pass@3 on Claw-Eval, and improves to 73.9% pass@3 when paired with a stronger ZeroClaw harness.
Collectively, these results demonstrate that a thin, open, harness-agnostic environment layer enables the reuse of agentic data, training recipes, and evaluation protocols across domains and harnesses.
We release [Orchard](https://github.com/microsoft/Orchard "") to accelerate agentic modeling research and drive innovation in the open-source AI community.

†††\\dagger First authors ‡\\ddagger Second authors

## 1 Introduction

![Refer to caption](https://arxiv.org/html/2605.15040v1/x1.png)

![Refer to caption](https://arxiv.org/html/2605.15040v1/x2.png)

Figure 1: Performance comparison. _Left:_ Orchard-SWE (30B) reaches 67.5% on SWE-bench Verified, approaching frontier MoE systems 1010–30×30\\times larger.
_Right:_ Orchard-GUI (4B) achieves 68.4% average success across WebVoyager, Online-Mind2web, and DeepShop, making it the strongest open-source GUI agent while staying on par with proprietary systems from OpenAI and Google.

Large language model (LLM) agents that interact with external environments over multiple turns have become a central paradigm for tasks ranging from software engineering (Jimenez et al., [2024](https://arxiv.org/html/2605.15040v1#bib.bib23 "SWE-bench: can language models resolve real-world github issues?"); Yang et al., [2024](https://arxiv.org/html/2605.15040v1#bib.bib29 "SWE-agent: agent-computer interfaces enable automated software engineering")) and web navigation (Zhou et al., [2024](https://arxiv.org/html/2605.15040v1#bib.bib11 "WebArena: a realistic web environment for building autonomous agents"); Zhang et al., [2025](https://arxiv.org/html/2605.15040v1#bib.bib12 "Large language model-brained GUI agents: a survey"); Ning et al., [2025](https://arxiv.org/html/2605.15040v1#bib.bib13 "A survey of WebAgents: towards next-generation AI agents for web automation with large foundation models")) to general computer use (Xie et al., [2024](https://arxiv.org/html/2605.15040v1#bib.bib14 "OSWorld: benchmarking multimodal agents for open-ended tasks in real computer environments"); Hu et al., [2025](https://arxiv.org/html/2605.15040v1#bib.bib15 "OS agents: a survey on MLLM-based agents for computer, phone and browser use")).
Training such agents—through supervised fine-tuning on expert trajectories or reinforcement learning from environment rewards—requires generating large numbers of rollout trajectories, each involving dozens of sequential interactions with a sandboxed execution environment.

As agentic training and evaluation scale to new domains and larger datasets, the need for open, scalable, affordable, and research-friendly infrastructure becomes increasingly acute.
For example, generating a single trajectory for a software engineering task may involve cloning a repository, installing dependencies, applying code edits, and running a test suite—all within an isolated container that must be provisioned, managed, and cleaned up.
At scale, thousands of such environments must run concurrently, each with distinct base images, resource requirements, and network isolation constraints.

We identify the environment layer as the foundational bottleneck. When it is closed or rigidly coupled to a particular training stack, every layer above it—training recipes, evaluation pipelines, trajectory collection—inherits those constraints and cannot be independently reproduced or reused.
Existing systems make different choices about where to place environment management, each with trade-offs.
Managed sandbox platforms such as E2B (E2B, [2024](https://arxiv.org/html/2605.15040v1#bib.bib16 "E2B: open-source secure sandboxes for AI code execution")), Daytona (Daytona, [2025](https://arxiv.org/html/2605.15040v1#bib.bib17 "Daytona: secure and elastic infrastructure for running AI-generated code")), and Modal (Modal Labs, [2024](https://arxiv.org/html/2605.15040v1#bib.bib18 "Modal: high-performance AI infrastructure")) provide convenient hosted runtimes, but give researchers limited control over infrastructure configuration, cost, and reproducibility.
Vertically integrated training stacks such as ProRL Agent (Zhang et al., [2026a](https://arxiv.org/html/2605.15040v1#bib.bib19 "ProRL Agent: rollout-as-a-service for RL training of multi-turn LLM agents")) and MegaFlow (Zhang et al., [2026b](https://arxiv.org/html/2605.15040v1#bib.bib20 "MegaFlow: large-scale distributed orchestration system for the agentic era")) include environment management as part of a larger rollout or training system, coupling it with inference scheduling, reward computation, and training-loop orchestration.
Broader environment frameworks such as ROCK (Wang and others, [2026](https://arxiv.org/html/2605.15040v1#bib.bib21 "Let it flow: agentic crafting on rock and roll, building the ROME model within an open agentic learning ecosystem")) provides rich platform functionality, but do not isolate the environment layer as a minimal service boundary.
As a result, trajectory datasets, training recipes, and evaluation pipelines are often tied to a particular harness or infrastructure implementation, making them difficult to reproduce, compare, or reuse.

We argue that the environment layer should instead be a _thin, standalone service_ reusable along three axes: across _(i)_ task domains, _(ii)_ agent harnesses within a domain, and _(iii)_ pipeline stages, including trajectory distillation, on-policy RL rollouts, and evaluation.
When this boundary is clean, the layers above it become reusable as well: data can be collected under one harness and evaluated under another, SFT and RL recipes can share the same execution backend, and new domains can reuse the same infrastructure rather than rebuild it.

![Refer to caption](https://arxiv.org/html/2605.15040v1/x3.png)Figure 2: Overview of the Orchard framework. Orchard Env (center) is a thin, Kubernetes-native environment service that exposes generic primitives—sandbox lifecycle, command execution, file I/O, network policy, a REST API, and lightweight agent injection—and supports heterogeneous task environments (bottom row). Open training recipes (second row) compose with this service without coupling to it, and we instantiate the same stack in three domains (top row): Orchard-SWE (software engineering), Orchard-GUI (browser navigation), and Orchard-Claw (AI personal assistant); per-domain headline numbers are summarized inside each domain box.

Therefore, we present Orchard (Figure [2](https://arxiv.org/html/2605.15040v1#S1.F2 "Figure 2 ‣ 1 Introduction ‣ Orchard: An Open-Source Agentic Modeling Framework")), an open framework for scalable agentic modeling centered on a thin, reusable environment layer. Its core component, Orchard Env, is a Kubernetes-native service that exposes generic primitives—sandbox lifecycle management, command execution, file I/O, network policy, and a REST API—without coupling to any agent harness, trainer, inference backend, or task domain. Orchard Env scales through two key choices: runtime agent injection, which allows arbitrary task-specific Docker images to run separately, and direct routing of execution and file requests to sandbox Pod IPs, avoiding Kubernetes exec/WebSocket overhead. Together with network isolation, asynchronous lifecycle management, heartbeat cleanup, and watch-based readiness tracking, these mechanisms make Orchard Env broadly composable and practical for large-scale environment interaction. Empirically, it achieves 0.28s average command-execution latency, sustains a 1,000-sandbox stress test with 100% success, and substantially lowers estimated sandboxing cost relative to other alternatives.

On top of Orchard Env, we develop three agentic modeling (SFT+RL) recipes that compose with the environment service without tight coupling.
These recipes handle trajectory collection, data curation, reward computation, and policy optimization. We instantiate them with backbones ranging from Qwen3-VL-4B-Thinking for browser agents to Qwen3-30B-A3B-Thinking (∼\\sim3B active parameters) for software engineering and personal assistant agents.
Across three domains, the same environment abstraction supports diverse modalities, tool interfaces, agent harnesses, and reward mechanisms.

##### Orchard-SWE.

For software engineering, Orchard-SWE targets two key bottlenecks of open SWE-agent training: limited supervision and sparse rewards.
We curate 107K trajectories distilled from MiniMax-M2.5 and Qwen3.5-397B across SWE-rebench (Badertdinov et al., [2025](https://arxiv.org/html/2605.15040v1#bib.bib31 "Swe-rebench: an automated pipeline for task collection and decontaminated evaluation of software engineering agents")), SWE-rebench V2 (Badertdinov et al., [2026](https://arxiv.org/html/2605.15040v1#bib.bib32 "SWE-rebench v2: language-agnostic swe task collection at scale")), and Scale-SWE (Zhao et al., [2026](https://arxiv.org/html/2605.15040v1#bib.bib33 "Immersion in the GitHub universe: scaling coding agents to mastery")), using both the OpenHands (Wang et al., [2025b](https://arxiv.org/html/2605.15040v1#bib.bib28 "OpenHands: an open platform for AI software developers as generalist agents")) and mini-swe-agent (Yang et al., [2024](https://arxiv.org/html/2605.15040v1#bib.bib29 "SWE-agent: agent-computer interfaces enable automated software engineering")) harnesses.
Unlike most prior recipes, we retain not only resolved trajectories but also unresolved ones.
We introduce _credit-assignment SFT_, which uses retrospective value estimation to extract productive rise segments from failed trajectories, converting partial progresses into supervised signals.
We further apply _Balanced Adaptive Rollout (BAR)_, an online rollout-allocation method, to adaptively assemble reward-balanced trajectory groups for sparse-reward RL.
With Qwen3-30B-A3B-Thinking, Orchard-SWE achieves 64.3% resolve rate on SWE-bench Verified after SFT and 67.5% after SFT+RL under mini-swe-agent,
setting a new state of the art among open-source models of comparable size while
remaining competitive with substantially larger models.

##### Orchard-GUI.

For browser-based GUI agents, Orchard-GUI shows that the same environment service and recipe transfer beyond text-only computer use tasks.
We train a 4B vision-language backbone with a generic ReAct-style (Yao et al., [2023](https://arxiv.org/html/2605.15040v1#bib.bib5 "ReAct: synergizing reasoning and acting in language models")) browser harness and evaluate on WebVoyager (He et al., [2024](https://arxiv.org/html/2605.15040v1#bib.bib2 "WebVoyager: building an end-to-end web agent with large multimodal models")), Online-Mind2Web (Deng et al., [2023](https://arxiv.org/html/2605.15040v1#bib.bib3 "Mind2Web: towards a generalist agent for the web")), and DeepShop (Lyu et al., [2025](https://arxiv.org/html/2605.15040v1#bib.bib4 "Deepshop: a benchmark for deep research shopping agents")).
After SFT+RL training, Orchard-GUI achieves success rates of 74.1%, 67.0%, and 64.0% on the three benchmarks, averaging 68.4% overall with the largest gains observed on long-horizon benchmarks, i.e., Online-Mind2Web and DeepShop.
This is a new open-source state of the art while remaining competitive with leading proprietary computer-use systems, despite using a 4B backbone model and only 2.6K training tasks.
Remarkably, Orchard-GUI substantially outperforms both prior open-source agents and its 235B teacher model,
suggesting that environment-grounded RL can improve model’s agentic capabilities beyond those of the teacher.

##### Orchard-Claw.

For personal assistant agent, Orchard-Claw studies whether machine learned agent skills can transfer across different harnesses.
We synthesize training tasks from Claw-Eval (Ye et al., [2026](https://arxiv.org/html/2605.15040v1#bib.bib39 "Claw-eval: toward trustworthy evaluation of autonomous agents")) seeds and ClawHub (OpenClaw, [2026](https://arxiv.org/html/2605.15040v1#bib.bib38 "ClawHub: skill directory for openclaw")) workflows, distill successful MiniMax-M2.5 trajectories, perform agentic training (SFT+RL) on Qwen3-30B-A3B-Thinking, and evaluate across harnesses, including a ReAct-style harness and the ZeroClaw (ZeroClaw Labs, [2026](https://arxiv.org/html/2605.15040v1#bib.bib42 "ZeroClaw")) harness.
Orchard-Claw achieves 31.7% p​a​s​s3pass^{3} and 59.6% p​a​s​s​@​3pass@3 on Claw-Eval, significantly outperforming comparable-size open-source baselines despite using only 0.2K synthetic tasks.
When paired with the stronger ZeroClaw harness at inference time, the same model improves further to 41.0% p​a​s​s3pass^{3} and 73.9% p​a​s​s​@​3pass@3.

Collectively, the results from the three agentic modeling recipes support the central claim of this study: the environment layer is not merely an infrastructural component, but the substrate governing the reusability of agentic modeling artifacts.
A thin, open, harness-agnostic environment service enables trajectory data, SFT recipes, RL rollouts, and evaluation protocols to transfer across domains, agent harnesses, and pipeline stages.
Orchard demonstrates that open-source agentic modeling can be scaled in a manner that is both cost-effective and reproducible, without coupling the environment to any single training stack.
We release the full Orchard framework—environment service, training recipes, and trajectory datasets spanning software engineering, GUI navigation, and personal-assistant tool use—to facilitate open research in scalable agentic modeling.

## 2 Orchard Env

Scaling agentic training across domains and tasks places specific demands on the environment layer.
We identify three core requirements for an environment service that can serve as a practical foundation for the research community:

1. 1.


Thin, standalone service boundary. Environment management should be isolated as a narrow service—decoupled from agent harness, model serving, and training orchestration—so that any combination of trainer, agent design, and task domain can compose with the same service.

2. 2.


Low-cost image compatibility. The service should support heterogeneous task environments and arbitrary Docker images at low adaptation cost.

3. 3.


Accessible and cost-practical at scale. The service should be deployable on any standard cloud infrastructure, making large-scale agentic training affordable and easy to adopt.


This section describes how Orchard Env realizes these requirements, presents its architecture and key design choices, and positions it among existing systems. More details can be found in Appendix [A](https://arxiv.org/html/2605.15040v1#A1 "Appendix A Orchard Env Design Details ‣ 7 Conclusion ‣ Generalist long-running autonomous agents (Claw-agent). ‣ 6 Related Work ‣ 5.4 Main Results ‣ Stage 2: Reinforcement learning. ‣ 5.3 Training Recipe ‣ Trajectory generation. ‣ 5.2 Trajectory Collection and Curation ‣ 5 Orchard-Claw ‣ 4.5 Main Results ‣ Stage 2: Reinforcement learning. ‣ 4.4 Training Recipe ‣ Filtering and curation. ‣ 4.3 Trajectory Collection and Curation ‣ 4.2 Generic Tool-Calling Agent Harness ‣ 4 Orchard-GUI ‣ Effect of reinforcement learning. ‣ 3.6 Ablations and Analysis ‣ Discussion. ‣ 3.5 Generalization to Unseen Harnesses and Tasks ‣ Same-size family lift. ‣ 3.4 Main Results ‣ Data Selection. ‣ 3.3.3 Balanced Adaptive Rollout (BAR) ‣ System Design and Component Orchestration. ‣ 3.3.2 Stage 2: Reinforcement Learning with Balanced Adaptive Rollout (BAR) ‣ SFT objective. ‣ Rise-segment extraction. ‣ Retrospective value estimation. ‣ 3.3.1 Stage 1: Supervised Fine-Tuning with credit assignment ‣ 3.3 Training Recipe ‣ 3 Orchard-SWE ‣ Orchard: An Open-Source Agentic Modeling Framework").

### 2.1 Architecture Overview

Orchard Env follows a three-layer architecture, as illustrated in Figure [3](https://arxiv.org/html/2605.15040v1#S2.F3 "Figure 3 ‣ 2.1 Architecture Overview ‣ 2 Orchard Env ‣ Orchard: An Open-Source Agentic Modeling Framework"): a _client SDK_ that provides synchronous and asynchronous Python interfaces, an _orchestrator_ that manages sandbox lifecycle and scheduling, and a lightweight _in-pod agent_ injected into each sandbox container.

![Refer to caption](https://arxiv.org/html/2605.15040v1/figures/orchard-architecture.png)Figure 3: Orchard Env architecture. A Python client SDK (synchronous or asynchronous) issues REST calls to a FastAPI orchestrator, which manages sandbox lifecycle in a Kubernetes cluster. Pod creation and deletion (cold path) go through the Kubernetes API server, while all execution, file, and health requests (hot path) are dispatched directly to each sandbox pod’s in-pod agent via Pod IP, bypassing the API server and avoiding kubectl exec/WebSocket setup overhead.

This three-layer separation reflects three deliberate choices.
First, the orchestrator and the in-pod agents are deployed and scaled independently: lifecycle decisions (creation, deletion, readiness) flow through the central orchestrator, while per-command execution traffic is dispatched directly to each sandbox’s in-pod agent, isolating control-plane operations from the latency-sensitive hot path.
Second, the in-pod agent is _injected_ into user-supplied images at runtime rather than baked in at build time, so that arbitrary task images integrate with no per-image modifications.
Third, the entire stack runs on standard Kubernetes primitives (Pods, NetworkPolicy, Watch), inheriting open ecosystem tooling, multi-cloud portability, and cost optimizations such as cluster autoscaling and spot instances.
We describe each layer in turn.

##### Client SDK.

Orchard Env provides both synchronous (SandboxClient) and asynchronous (AsyncSandboxClient) Python clients.
Sandboxes are created from user-specified Docker images and expose methods for command execution, file upload/download, and patch application.
Context managers provide automatic cleanup, and the SDK exposes heartbeat utilities for keeping long-lived sandboxes alive when desired.
The SDK also includes configurable retry logic with exponential backoff for transient connection errors and service unavailable errors.

##### Orchestrator.

The orchestrator is a FastAPI service deployed as a Kubernetes Deployment with multiple replicas.
It exposes a REST API for sandbox lifecycle management and can delegate sandbox metadata tracking to an optional Redis backend across replicas.
Key responsibilities include:
Sandbox provisioning: Translating POST /sandboxes requests into Kubernetes Pod specifications, including init container configuration, resource limits, network policies, and readiness probes.
Readiness tracking: A PodWatcher component maintains a persistent Kubernetes LIST+WATCH stream, caching pod state transitions and waking blocked clients
when pods become ready.
Execution scheduling: An ExecManager routes execution requests to the target sandbox’s in-pod agent via direct HTTP calls to the Pod IP, serializing concurrent requests to the same sandbox via per-sandbox locks.
Lifecycle management: A background reconciliation loop detects and cleans up orphaned sandboxes (those whose heartbeat has expired or whose backing Pod has been evicted).

##### In-Pod Agent.

The in-pod agent111Here, “agent” refers to the sandbox-side execution service, not the LLM-based agents studied elsewhere in this paper. is a lightweight FastAPI server that runs inside each sandbox container.
It exposes endpoints for command execution (/exec), file upload, download, listing, and health checking.
Commands are executed as subprocesses with configurable timeouts; on timeout, the entire process tree is killed via process group signal.
The agent is reachable only through the sandbox pod’s internal cluster network endpoint, and its health endpoint serves as the Kubernetes readiness probe.

### 2.2 Comparison with Existing Systems

To position Orchard Env relative to existing systems, Table [1](https://arxiv.org/html/2605.15040v1#S2.T1 "Table 1 ‣ 2.2 Comparison with Existing Systems ‣ 2 Orchard Env ‣ Orchard: An Open-Source Agentic Modeling Framework") compares environment and training infrastructure along four dimensions derived from the requirements above: whether an open-source server stack exists that researchers can self-host, whether the system is operated primarily as a managed service, whether it exposes a thin standalone environment service, and its relative cost at research scale.
Concretely, we treat a system as a _thin env service_ when _(i)_ environment management is the system’s primary scope rather than a by-product of agent harness, training orchestration, or LLM serving; _(ii)_ the environment layer presents a stable API—typically a small REST surface for sandbox lifecycle and command execution—that does not require the caller to adopt the system’s trainer, scheduler, or rollout abstractions; and _(iii)_ that API is independent of the choice of agent harness, RL trainer, and inference backend, so the same service can back distillation, RL rollouts, and evaluation interchangeably. We highlight three aspects of Orchard Env’s positioning222The comparison is based on public documentation and repositories as of April 2026.:

Table 1: Environment and training systems for agentic training, based on public documentation as of April 2026.
_Scope_: the system’s primary design scope.
_Self-host._: an open-source server/control-plane stack exists that researchers can deploy on their own infrastructure.
_Mgd. default_: the system’s primary product is offered as a managed/hosted service.
_Thin env service_: environment management is exposed as a narrow, standalone service boundary independent of agent harness, training loop, and inference backend (operational definition above).
_Rel. cost_: normalized to Daytona for a 2-vCPU, 8-GiB sandbox; “—” indicates no publicly comparable pricing.
See Appendix [B](https://arxiv.org/html/2605.15040v1#A2 "Appendix B Cost Analysis Details ‣ 7 Conclusion ‣ Generalist long-running autonomous agents (Claw-agent). ‣ 6 Related Work ‣ 5.4 Main Results ‣ Stage 2: Reinforcement learning. ‣ 5.3 Training Recipe ‣ Trajectory generation. ‣ 5.2 Trajectory Collection and Curation ‣ 5 Orchard-Claw ‣ 4.5 Main Results ‣ Stage 2: Reinforcement learning. ‣ 4.4 Training Recipe ‣ Filtering and curation. ‣ 4.3 Trajectory Collection and Curation ‣ 4.2 Generic Tool-Calling Agent Harness ‣ 4 Orchard-GUI ‣ Effect of reinforcement learning. ‣ 3.6 Ablations and Analysis ‣ Discussion. ‣ 3.5 Generalization to Unseen Harnesses and Tasks ‣ Same-size family lift. ‣ 3.4 Main Results ‣ Data Selection. ‣ 3.3.3 Balanced Adaptive Rollout (BAR) ‣ System Design and Component Orchestration. ‣ 3.3.2 Stage 2: Reinforcement Learning with Balanced Adaptive Rollout (BAR) ‣ SFT objective. ‣ Rise-segment extraction. ‣ Retrospective value estimation. ‣ 3.3.1 Stage 1: Supervised Fine-Tuning with credit assignment ‣ 3.3 Training Recipe ‣ 3 Orchard-SWE ‣ Orchard: An Open-Source Agentic Modeling Framework") for methodology.

| System | Scope | Self-host. | Mgd. default | Thin env service | Rel. cost |
| --- | --- | --- | --- | --- | --- |
| E2B | managed sandbox | ✓† | ✓ | ✓ | 1.0×\\times |
| Daytona | managed sandbox | ✓† | ✓ | ✓ | 1.0×\\times |
| Modal | compute platform | ✗ | ✓ | ✗ | 1.5×\\times |
| SkyPilot | compute orch. | ✓ | ✗ | ✗ | — |
| ROCK | env. framework | ✓ | ✗ | ✗ | — |
| ProRL Agent | rollout infra. | ✓ | ✗ | ✗ | — |
| MegaFlow | training orch. | ✗ | ✗ | ✗ | — |
| Orchard Env | env. service | ✓ | ✗ | ✓ | 0.47×\\times∗ |

∗0.10×\\times with spot instances. †E2B and Daytona ship limited open-source server components, but their primary product is the hosted control plane and the _Rel. cost_ column reflects that managed offering.

##### Thin env service vs. integrated and broad systems.

ProRL Agent (Zhang et al., [2026a](https://arxiv.org/html/2605.15040v1#bib.bib19 "ProRL Agent: rollout-as-a-service for RL training of multi-turn LLM agents")) achieves an important decoupling—separating the rollout lifecycle from the RL trainer via an HTTP service—but its environment layer remains coupled with agent harness (via AgentHandler plugins), LLM inference routing, and evaluation logic within the same rollout server.
MegaFlow (Zhang et al., [2026b](https://arxiv.org/html/2605.15040v1#bib.bib20 "MegaFlow: large-scale distributed orchestration system for the agentic era")) similarly embeds environment management within a larger training orchestration system.
Modal (Modal Labs, [2024](https://arxiv.org/html/2605.15040v1#bib.bib18 "Modal: high-performance AI infrastructure")) is a different category altogether: it is a general serverless compute platform that offers flexible function and container execution, but it is not specialized as a thin environment service for agentic training, and its hosted control plane and per-second pricing are difficult to amortize across long-running RL training campaigns.
ROCK (Wang and others, [2026](https://arxiv.org/html/2605.15040v1#bib.bib21 "Let it flow: agentic crafting on rock and roll, building the ROME model within an open agentic learning ecosystem")) provides a broader environment framework with multiple protocols and richer platform components, targeting a wider scope than a thin service boundary.
SkyPilot (Kim, [2025](https://arxiv.org/html/2605.15040v1#bib.bib22 "Self-host open-source LLM agent sandbox on your own cloud")) provides open-source multi-cloud compute orchestration and can serve as the underlying infrastructure on which Orchard Env is deployed; the two are complementary rather than competing.
E2B (E2B, [2024](https://arxiv.org/html/2605.15040v1#bib.bib16 "E2B: open-source secure sandboxes for AI code execution")) and Daytona (Daytona, [2025](https://arxiv.org/html/2605.15040v1#bib.bib17 "Daytona: secure and elastic infrastructure for running AI-generated code")), like Orchard Env, expose environment management as standalone sandbox services, but as managed products with hosted control planes and vendor-determined pricing.
Orchard Env’s distinguishing technical choice is _agent injection_: a Kubernetes init container copies a self-contained execution agent into any user-provided Docker image at pod startup, avoiding the need to rebuild task images.
This enables Orchard Env to support hundreds of heterogeneous task environments—such as the diverse images required by SWE-bench—without per-image modifications.

##### Deployment portability.

Orchard Env targets researcher-controlled infrastructure: any standard Kubernetes environment—managed (AKS, EKS, GKE) or self-hosted—can run the full stack, with direct control over resource allocation, network policies, and autoscaling.
This contrasts with HPC-oriented systems like ProRL Agent, which require access to institutional Slurm clusters and Singularity runtimes, limiting adoption to researchers at specific institutions.

##### Cost as a consequence of design choices.

Table [2](https://arxiv.org/html/2605.15040v1#S2.T2 "Table 2 ‣ Cost as a consequence of design choices. ‣ 2.2 Comparison with Existing Systems ‣ 2 Orchard Env ‣ Orchard: An Open-Source Agentic Modeling Framework") compares estimated costs for 128 parallel sandboxes (2 vCPU, 8 GiB each) over 240 hours—a representative RL training workload.
Because Orchard Env is self-hosted on standard Kubernetes, it naturally benefits from cloud-native cost optimization: ephemeral sandbox nodes can run on spot instances, and cluster autoscaling adjusts capacity to actual demand.
This reduces cost to $673 with spot instances—10×\\times lower than managed alternatives like Daytona and E2B.
Even at on-demand rates, Orchard Env ($3,362) is less than half the cost of Daytona ($7,078) and E2B ($7,078).
A detailed breakdown is provided in the Appendix [B](https://arxiv.org/html/2605.15040v1#A2 "Appendix B Cost Analysis Details ‣ 7 Conclusion ‣ Generalist long-running autonomous agents (Claw-agent). ‣ 6 Related Work ‣ 5.4 Main Results ‣ Stage 2: Reinforcement learning. ‣ 5.3 Training Recipe ‣ Trajectory generation. ‣ 5.2 Trajectory Collection and Curation ‣ 5 Orchard-Claw ‣ 4.5 Main Results ‣ Stage 2: Reinforcement learning. ‣ 4.4 Training Recipe ‣ Filtering and curation. ‣ 4.3 Trajectory Collection and Curation ‣ 4.2 Generic Tool-Calling Agent Harness ‣ 4 Orchard-GUI ‣ Effect of reinforcement learning. ‣ 3.6 Ablations and Analysis ‣ Discussion. ‣ 3.5 Generalization to Unseen Harnesses and Tasks ‣ Same-size family lift. ‣ 3.4 Main Results ‣ Data Selection. ‣ 3.3.3 Balanced Adaptive Rollout (BAR) ‣ System Design and Component Orchestration. ‣ 3.3.2 Stage 2: Reinforcement Learning with Balanced Adaptive Rollout (BAR) ‣ SFT objective. ‣ Rise-segment extraction. ‣ Retrospective value estimation. ‣ 3.3.1 Stage 1: Supervised Fine-Tuning with credit assignment ‣ 3.3 Training Recipe ‣ 3 Orchard-SWE ‣ Orchard: An Open-Source Agentic Modeling Framework").

Table 2: Estimated cost for 128 parallel sandboxes over 240 hours (30,720 sandbox-hours). Target: 2 vCPU, 8 GiB RAM per sandbox. Costs normalized to Daytona. Prices from official rate cards as of April 2026; see Appendix [B](https://arxiv.org/html/2605.15040v1#A2 "Appendix B Cost Analysis Details ‣ 7 Conclusion ‣ Generalist long-running autonomous agents (Claw-agent). ‣ 6 Related Work ‣ 5.4 Main Results ‣ Stage 2: Reinforcement learning. ‣ 5.3 Training Recipe ‣ Trajectory generation. ‣ 5.2 Trajectory Collection and Curation ‣ 5 Orchard-Claw ‣ 4.5 Main Results ‣ Stage 2: Reinforcement learning. ‣ 4.4 Training Recipe ‣ Filtering and curation. ‣ 4.3 Trajectory Collection and Curation ‣ 4.2 Generic Tool-Calling Agent Harness ‣ 4 Orchard-GUI ‣ Effect of reinforcement learning. ‣ 3.6 Ablations and Analysis ‣ Discussion. ‣ 3.5 Generalization to Unseen Harnesses and Tasks ‣ Same-size family lift. ‣ 3.4 Main Results ‣ Data Selection. ‣ 3.3.3 Balanced Adaptive Rollout (BAR) ‣ System Design and Component Orchestration. ‣ 3.3.2 Stage 2: Reinforcement Learning with Balanced Adaptive Rollout (BAR) ‣ SFT objective. ‣ Rise-segment extraction. ‣ Retrospective value estimation. ‣ 3.3.1 Stage 1: Supervised Fine-Tuning with credit assignment ‣ 3.3 Training Recipe ‣ 3 Orchard-SWE ‣ Orchard: An Open-Source Agentic Modeling Framework").

| System | $/sandbox-hr | Cost ($) | vs. Daytona |
| --- | --- | --- | --- |
| Daytona | 0.230 | 7,078 | — |
| E2B | 0.230 | 7,078 | 0% |
| Modal | 0.335 | 10,305 | +46% |
| MegaFlow† | 0.150 (est.) | 4,608 (est.) | −-35% |
| Orchard Env (on-demand) | 0.109 | 3,362 | −-53% |
| Orchard Env (spot) | 0.022 | 673 | −-90% |

†MegaFlow is not publicly priced; cells marked _(est.)_ are estimated from reported infrastructure usage in Zhang et al. ( [2026b](https://arxiv.org/html/2605.15040v1#bib.bib20 "MegaFlow: large-scale distributed orchestration system for the agentic era")).

### 2.3 System Evaluation

For both agentic data generation and RL training, the most critical systems metric is environment interaction latency—it directly determines rollout throughput and GPU utilization.
We evaluate Orchard Env on three axes: _(i)_ execution latency relative to existing services, _(ii)_ reliability under high concurrency, and _(iii)_ functional equivalence to a direct Docker baseline in downstream agent evaluations.
Unless noted otherwise, all measurements use a Kubernetes cluster of 8 nodes (each 32 vCPU, 128 GiB RAM) on commodity cloud VMs, with sandbox images pre-pulled on every node and each sandbox provisioned with 2 vCPU and 8 GiB RAM.

##### Execution latency.

We compare average command execution latency across four environment services using the benchmark methodology of SkyPilot Code Sandbox (Kim, [2025](https://arxiv.org/html/2605.15040v1#bib.bib22 "Self-host open-source LLM agent sandbox on your own cloud")), with the same benchmark setup across all platforms.

Table 3: System evaluation of Orchard Env. Left: Average command execution latency across environment services (lower is better; benchmark methodology follows Kim ( [2025](https://arxiv.org/html/2605.15040v1#bib.bib22 "Self-host open-source LLM agent sandbox on your own cloud"))). Right: Agent pass rates (%) on Terminal-Bench 2.0 using Orchard Env vs. a direct Docker baseline, averaged over 3 runs per cell, confirming no regression from the environment service layer.

| System | Avg. latency (s) |
| --- | --- |
| Orchard Env | 0.280 |
| SkyPilot Code Sandbox | 0.284 |
| E2B | 0.747 |
| Modal | 2.046 |

| Model | Docker | Orchard Env |
| --- | --- | --- |
| GPT-4.1 | 34.1 | 35.1 |
| MiniMax-M2.5 | 52.6 | 54.4 |
| Qwen3-8B-thinking | 7.0 | 8.8 |

As shown in Table [3](https://arxiv.org/html/2605.15040v1#S2.T3 "Table 3 ‣ Execution latency. ‣ 2.3 System Evaluation ‣ 2 Orchard Env ‣ Orchard: An Open-Source Agentic Modeling Framework"), Orchard Env achieves an average execution latency of 0.28 s, essentially matching SkyPilot Code Sandbox (0.284 s) and significantly outperforming E2B (0.747 s, 2.7×\\times slower) and Modal (2.046 s, 7.3×\\times slower).
This validates Orchard Env’s direct Pod-IP communication design: by routing execution requests directly to the in-pod agent and bypassing the Kubernetes API server on the hot path, Orchard Env achieves latency comparable to optimized native runtimes while retaining the flexibility of a Kubernetes-based deployment.

##### Reliability under concurrency.

To stress-test Orchard Env at scale, we ran 1,000 sandboxes in parallel through the full lifecycle: create →\\rightarrow execute 4 commands →\\rightarrow delete.

Table 4: Stress test results for 1,000 parallel sandboxes through the full lifecycle
(create →\\rightarrow 4×\\times exec →\\rightarrow delete).

| Metric | Value |
| --- | --- |
| Parallel sandboxes | 1,000 |
| --- | --- |
| Commands per sandbox | 4 |
| --- | --- |
| Success rate | 100% |
| End-to-end time | 26 s |
| Avg. create time | 11.75 s |
| Avg. exec latency | 0.28 s |

As shown in Table [4](https://arxiv.org/html/2605.15040v1#S2.T4 "Table 4 ‣ Reliability under concurrency. ‣ 2.3 System Evaluation ‣ 2 Orchard Env ‣ Orchard: An Open-Source Agentic Modeling Framework"), Orchard Env achieved a 100% success rate across all 1,000 sessions—no failures on creation, execution, or cleanup—with the entire test completing in 26 seconds end-to-end.
Translating these end-to-end numbers, Orchard Env sustains roughly 154 commands per second across the full create →\\rightarrow exec →\\rightarrow delete lifecycle (4,000 commands across 1,000 sandboxes in 26 s), well above the throughput required by typical agentic distillation and RL workloads at this concurrency.
These results confirm that Orchard Env’s architecture—watch-based readiness tracking, per-sandbox locking, and heartbeat-based cleanup—remains reliable at the concurrency levels required for large-scale agentic training.

##### Functional equivalence to Docker.

Beyond infrastructure metrics, we verify that Orchard Env introduces no performance regression in downstream agent evaluations.
We compare Orchard Env against a direct Docker baseline on Terminal-Bench 2.0 (Merrill et al., [2026](https://arxiv.org/html/2605.15040v1#bib.bib26 "Terminal-bench: benchmarking agents on hard, realistic tasks in command line interfaces")), using three models of varying capability; reported numbers are averaged over 3 independent runs per (model, backend) pair.
As shown in Table [3](https://arxiv.org/html/2605.15040v1#S2.T3 "Table 3 ‣ Execution latency. ‣ 2.3 System Evaluation ‣ 2 Orchard Env ‣ Orchard: An Open-Source Agentic Modeling Framework") (right), Orchard Env matches Docker within run-to-run variance across all three models, with a marginal edge in every case (1–2 points).
This confirms that the agent-injection mechanism and Orchard Env’s execution path introduce no observable overhead or interference in agent–environment interactions.

## 3 Orchard-SWE

This section presents Orchard-SWE, our instantiation of the Orchard training recipe for software engineering.
We describe the problem setting, trajectory collection pipeline, two-stage training recipe, main results on SWE-bench Verified, and ablations that isolate key design choices.

### 3.1 Problem Setting

##### Task and evaluation.

We target the SWE-bench task formulation (Jimenez et al., [2024](https://arxiv.org/html/2605.15040v1#bib.bib23 "SWE-bench: can language models resolve real-world github issues?")): given a GitHub issue description and a snapshot of the repository at the time the issue was filed, the agent must produce a code patch that resolves the issue.
A solution is scored as correct if and only if it passes the full gold test suite associated with the ground-truth pull request.
We use SWE-bench Verified(OpenAI, [2024](https://arxiv.org/html/2605.15040v1#bib.bib24 "Introducing SWE-bench Verified"))—a human-validated subset of 500 instances—as our primary evaluation benchmark. We also report auxiliary evaluations on SWE-bench Multilingual(Yang et al., [2025a](https://arxiv.org/html/2605.15040v1#bib.bib25 "SWE-smith: scaling data for software engineering agents")) and Terminal-Bench 2.0(Merrill et al., [2026](https://arxiv.org/html/2605.15040v1#bib.bib26 "Terminal-bench: benchmarking agents on hard, realistic tasks in command line interfaces")).

##### Agent harness and tool interface.

The agent operates in a multi-turn ReAct-style loop (Yao et al., [2023](https://arxiv.org/html/2605.15040v1#bib.bib5 "ReAct: synergizing reasoning and acting in language models")): at each step, it produces a reasoning trace (Thought) and a tool invocation (Action), then observes the environment response before proceeding.
The tool interface includes shell command execution, file viewing and editing, and patch submission.
All environment interactions are routed through the Orchard Env service: each task instance runs in an isolated sandbox (2 vCPU, 8 GiB memory) provisioned from the task-specific Docker image, with Orchard Env’s agent-injection mechanism handling image heterogeneity transparently.
A distinctive aspect of Orchard-SWE is that we collect trajectories using two different agent harnesses—the full-featured _OpenHands_(Wang et al., [2025b](https://arxiv.org/html/2605.15040v1#bib.bib28 "OpenHands: an open platform for AI software developers as generalist agents")) framework and a lightweight _mini-swe-agent_(Yang et al., [2024](https://arxiv.org/html/2605.15040v1#bib.bib29 "SWE-agent: agent-computer interfaces enable automated software engineering"))—and study how harness design affects both trajectory characteristics and downstream training outcomes (Section [3.6](https://arxiv.org/html/2605.15040v1#S3.SS6 "3.6 Ablations and Analysis ‣ Discussion. ‣ 3.5 Generalization to Unseen Harnesses and Tasks ‣ Same-size family lift. ‣ 3.4 Main Results ‣ Data Selection. ‣ 3.3.3 Balanced Adaptive Rollout (BAR) ‣ System Design and Component Orchestration. ‣ 3.3.2 Stage 2: Reinforcement Learning with Balanced Adaptive Rollout (BAR) ‣ SFT objective. ‣ Rise-segment extraction. ‣ Retrospective value estimation. ‣ 3.3.1 Stage 1: Supervised Fine-Tuning with credit assignment ‣ 3.3 Training Recipe ‣ 3 Orchard-SWE ‣ Orchard: An Open-Source Agentic Modeling Framework")).

### 3.2 Trajectory Collection and Curation

We construct the Orchard-SWE dataset through large-scale trajectory distillation from strong teacher models, followed by systematic filtering and curation.
Table [5](https://arxiv.org/html/2605.15040v1#S3.T5 "Table 5 ‣ Filtering and curation. ‣ 3.2 Trajectory Collection and Curation ‣ 3 Orchard-SWE ‣ Orchard: An Open-Source Agentic Modeling Framework") summarizes the composition of the final dataset.

##### Task sources.

We draw task instances from three sources:
(1) SWE-rebench(Badertdinov et al., [2025](https://arxiv.org/html/2605.15040v1#bib.bib31 "Swe-rebench: an automated pipeline for task collection and decontaminated evaluation of software engineering agents")), a large-scale collection of real-world GitHub issues with executable Docker-based test environments.
We use its _filtered_ subset, which applies quality and difficulty filters to retain instances that are both solvable and non-trivial, covering over 1,400 Python repositories.
(2) SWE-rebench V2(Badertdinov et al., [2026](https://arxiv.org/html/2605.15040v1#bib.bib32 "SWE-rebench v2: language-agnostic swe task collection at scale")), a language-agnostic extension of SWE-rebench that harvests more software engineering tasks. It provides over 32k containerized executable tasks spanning 20 programming languages333In our experiments, we primarily use its Python tasks for consistency with the rest of our task pool. and more than 3.6k repositories, together with pre-built images. We reserve SWE-rebench V2 entirely for RL training.
(3) Scale-SWE(Zhao et al., [2026](https://arxiv.org/html/2605.15040v1#bib.bib33 "Immersion in the GitHub universe: scaling coding agents to mastery")), a complementary task source that constructs 100k task instances from real GitHub pull requests across 5.2k repositories.
Each instance is packaged with a Docker image, a gold patch, and automatically generated test scripts, significantly expanding the diversity of repositories and issue types available for trajectory collection.

##### Multi-teacher trajectory generation.

We use multiple teacher models to increase the diversity of successful trajectories while keeping the downstream action space fixed.
For each task instance, we sample five rollout trajectories through Orchard Env and retain all trajectories that successfully resolve the task.
Our teacher pool includes Qwen3.5-397B(Qwen Team, [2026](https://arxiv.org/html/2605.15040v1#bib.bib34 "Qwen3.5: towards native multimodal agents")) and MiniMax-M2.5 230B(MiniMax, [2026](https://arxiv.org/html/2605.15040v1#bib.bib35 "MiniMax M2.5: built for real-world productivity")).
On SWE-rebench, we collect trajectories from both teachers under the mini-swe-agent and OpenHands harnesses.
Empirically, MiniMax-M2.5 achieves a higher task pass rate, while Qwen3.5-397B occasionally emits tool calls that are not defined in the OpenHands tool interface.
Based on these observations, we use MiniMax-M2.5 as the sole teacher for Scale-SWE, where rollout efficiency and stability become more important due to the larger number of instances.
In all cases, teachers interact through the same sandboxed tool interface used at evaluation time, ensuring that collected trajectories remain faithful to the downstream action space.

##### Harness selection.

We collect trajectories with two agent harnesses: OpenHands(Wang et al., [2025b](https://arxiv.org/html/2605.15040v1#bib.bib28 "OpenHands: an open platform for AI software developers as generalist agents")) and mini-swe-agent(Yang et al., [2024](https://arxiv.org/html/2605.15040v1#bib.bib29 "SWE-agent: agent-computer interfaces enable automated software engineering")).
On SWE-rebench, we use both harnesses so that trajectory collection covers a broader range of interaction styles and tool-use patterns.
For the OpenHands runs, we follow its standard SWE-bench tool configuration.444 [https://github.com/OpenHands/benchmarks/tree/main/benchmarks/swebench](https://github.com/OpenHands/benchmarks/tree/main/benchmarks/swebench "")
For Scale-SWE, we use only mini-swe-agent, a lightweight harness with a minimal tool set (bash execution, file editing, submission), since we did not observe a meaningful performance gap relative to OpenHands on this source and the lighter harness is more practical for large-scale rollout collection.
This dual-harness setup also lets us analyze how harness choice affects downstream training outcomes (Section [3.6](https://arxiv.org/html/2605.15040v1#S3.SS6 "3.6 Ablations and Analysis ‣ Discussion. ‣ 3.5 Generalization to Unseen Harnesses and Tasks ‣ Same-size family lift. ‣ 3.4 Main Results ‣ Data Selection. ‣ 3.3.3 Balanced Adaptive Rollout (BAR) ‣ System Design and Component Orchestration. ‣ 3.3.2 Stage 2: Reinforcement Learning with Balanced Adaptive Rollout (BAR) ‣ SFT objective. ‣ Rise-segment extraction. ‣ Retrospective value estimation. ‣ 3.3.1 Stage 1: Supervised Fine-Tuning with credit assignment ‣ 3.3 Training Recipe ‣ 3 Orchard-SWE ‣ Orchard: An Open-Source Agentic Modeling Framework")).

##### Filtering and curation.

Unlike most prior work, which retains only successful (resolved) trajectories for SFT, we keep both resolved and unresolved trajectories in the training corpus.
Resolved trajectories provide standard imitation signal; unresolved trajectories are curated via temporal-difference credit assignment to extract continuous _rise segments_—spans where the trajectory is making progress—which contribute partial-progress supervision (formalized in Section [3.3](https://arxiv.org/html/2605.15040v1#S3.SS3 "3.3 Training Recipe ‣ 3 Orchard-SWE ‣ Orchard: An Open-Source Agentic Modeling Framework")).
We additionally apply the following quality filters:
(1) trajectories exceeding 64K tokens are pruned to ensure training stability;
(2) trajectories containing tool calls not defined in the harness’s tool interface (primarily observed with Qwen3.5-397B) are discarded;
(3) trajectories with syntactically invalid or unparsable actions are removed.

Table 5: Composition of the Orchard-SWE training dataset. The corpus retains both resolved and unresolved trajectories: resolved trajectories provide direct imitation signal, while unresolved trajectories contribute partial-progress signal through credit-assignment SFT.

| Source | Teacher | Resolved | Unresolved | Total | Harness |
| --- | --- | --- | --- | --- | --- |
| Scale-SWE | MiniMax-M2.5 | 33,527 | 20,591 | 54,118 | mini-swe-agent |
| SWE-rebench | MiniMax-M2.5 | 13,364 | 10,099 | 23,463 | mini-swe-agent |
| SWE-rebench | Qwen3.5-397B | 15,545 | 1,846 | 17,391 | mini-swe-agent |
| SWE-rebench | MiniMax-M2.5 | 12,213 | 0 | 12,213 | OpenHands |
| Total |  | 74,649 | 32,536 | 107,185 |  |

After filtering, the Orchard-SWE dataset comprises 107K trajectories (74.6K resolved, 32.5K unresolved) spanning 19,287 unique task instances, with an average of 47.5 interaction turns and approximately 21K tokens per trajectory.
We release the full dataset, including both resolved and unresolved trajectories, as an open-source artifact.

### 3.3 Training Recipe

Our training recipe follows a two-stage pipeline: Supervised Fine-Tuning (SFT) on teacher-distilled trajectories, followed by Reinforcement Learning (RL) with environment-grounded rewards.
Both stages use Orchard Env as the execution backend.

#### 3.3.1 Stage 1: Supervised Fine-Tuning with credit assignment

We initialize from the base backbone and fine-tune on the curated teacher trajectories.
Each training example pairs the issue description and repository context with the full multi-turn interaction trace, serialized as a sequence of observations and actions.
Following standard practice for long-horizon agent training, we apply multi-turn masking so that environment observations are excluded from the loss and the model is trained only to predict its reasoning traces and actions.

A distinguishing feature of our SFT stage is the use of credit-assignment SFT, which incorporates not only the 74.6K resolved trajectories but also a curated subset of unresolved trajectories where partial progress is identifiable.
We instantiate credit assignment as a lightweight LLM-based variant of temporal-difference value estimation, formulated as follows.

##### Retrospective value estimation.

For each unresolved trajectory τ=(s0,a0,s1,…,sT)\\tau=(s\_{0},a\_{0},s\_{1},\\ldots,s\_{T}), we use the trajectory’s own teacher model as a zero-shot retrospective value function.
The teacher is shown the full trajectory together with the gold test outcome and is asked to estimate, at each step tt, the probability that the agent will resolve the issue given the history ht=(s0,a0,…,st)h\_{t}=(s\_{0},a\_{0},\\ldots,s\_{t}):

|     |     |     |     |
| --- | --- | --- | --- |
|  | V​(st)=ℙ​(resolve\|ht,outcome).V(s\_{t})\\;=\\;\\mathbb{P}\\bigl(\\text{resolve}\\,\\big\|\\,h\_{t},\\,\\text{outcome}\\bigr). |  | (1) |

The teacher annotates a sparse set of key steps and the remaining values are interpolated, yielding a per-step value curve V​(s0),…,V​(sT)V(s\_{0}),\\ldots,V(s\_{T}).
Because the judgment is retrospective and outcome-conditioned, the value curve calibrates well to actual progress: across our annotated trajectories, the curve is inverted-U in 98.9% of cases, peaking during exploration and decaying near the failed submission.
The exact prompt format is shown below.

`

Value-Estimation Prompt

Rise-segment extraction.

We define the per-step credit as the temporal-difference shift in estimated success probability,

ct=V​(st+1)−V​(st),c_{t}\;=\;V(s_{t+1})-V(s_{t}),

(2)

and extract rise segments: maximal contiguous subsequences [ti,tj][t_{i},t_{j}] over which the agent makes positive progress, i.e. ct≥εc_{t}\geq\varepsilon for all t∈[ti,tj−1]t\in[t_{i},t_{j}{-}1] (with a small threshold ε\varepsilon to filter annotation noise).555We use ε=0.05\varepsilon=0.05; see Appendix for sensitivity analysis.
Rise segments are typically short (median ∼\sim2 steps before merging with surrounding context) but capture the productive parts of an otherwise unsuccessful trajectory—repository navigation, file localization, and partial root-cause analysis.

SFT objective.

We train with a standard next-token cross-entropy loss on action tokens, masking environment observations as well as action tokens that fall outside any rise segment:

ℒSFT=−∑t∈𝒮​(τ)log⁡πθ​(at|ht),\mathcal{L}_{\text{SFT}}\;=\;-\sum_{t\in\mathcal{S}(\tau)}\log\pi_{\theta}\bigl(a_{t}\,\big|\,h_{t}\bigr),

where 𝒮​(τ)\mathcal{S}(\tau) is the set of action tokens contributing to the loss for trajectory τ\tau.
For resolved trajectories, 𝒮​(τ)\mathcal{S}(\tau) contains all action tokens (equivalent to a single segment spanning the entire trace, since the terminal value is 1).
For unresolved trajectories, 𝒮​(τ)\mathcal{S}(\tau) is restricted to action tokens inside the extracted rise segments, with the preceding history retained as context.
After this construction, the 32,536 unresolved trajectories yield exploration-focused supervision that complements the full solve-and-submit traces from the resolved set.

We train Qwen3-30B-A3B-Thinking (Qwen Team, 2025) with slime (Zhu et al., 2025) for five epochs with a global batch size of 128 and a 64K context window, using a cosine-decayed learning rate from 10−510^{-5} to 10−610^{-6}.
Although training uses a maximum sequence length of 64K, we extend the context limit to 128K at inference time to accommodate longer repository contexts and interaction histories.

3.3.2 Stage 2: Reinforcement Learning with Balanced Adaptive Rollout (BAR)

Starting from the SFT checkpoint, we apply RL to improve the model’s ability to recover from errors and explore alternative solution paths.
The reward signal is binary and environment-grounded: a trajectory receives a reward of +1+1 if the final patch passes the gold test suite in the sandbox, and −1-1 otherwise.
Orchard Env’s fast execution latency (0.28 s per command; Section 2.3) is critical at this stage, as each RL rollout requires dozens of environment interactions, and training throughput scales directly with sandbox responsiveness.

System Design and Component Orchestration.

Our RL system builds on slime (Zhu et al., 2025) post-training framework, extending its Ray-based, Megatron-LM–backed training and SGLang-based inference architecture for asynchronous agentic RL. The system is organized as four loosely coupled services that communicate through Ray actor handles and HTTP endpoints, so that each component can be scaled, replaced, or restarted independently:

•

Policy Trainer. A Megatron-LM-based distributed trainer, sharded with tensor, pipeline, expert, and context parallelism, that owns the trainable parameters and performs the optimization step using advantage-weighted policy gradients.

•

Rollout Inference Service. An SGLang-based inference service, fronted by a request router, that serves the latest policy snapshot. It supports KV-cache reuse, deterministic sampling seeds, and per-token log-probability extraction.

•

Sandboxed Execution Service. A sandbox runtime initialized from Orchard Env. Each agent trajectory is bound to an isolated sandbox in which bash commands and unit-test suites can be executed safely through Orchard Env.

•

Agentic Loop Driver. A per-sample asynchronous coroutine that orchestrates the tool-calling interaction between the inference service and the execution sandbox. At each step, it tokenizes the running message history with the chat template and registered bash tool schema, queries the inference service, parses the structured tool call from the assistant message, executes the call inside the sandbox, and appends the resulting observation as a tool message. The loop terminates when the agent submits a patch, exceeds a step, wall-clock, or token budget, or is aborted due to rare sandbox failures.

The orchestration is asynchronous and pipelined. While the trainer is updating weights for rollout kk, the rollout manager has already dispatched generation for rollout k+1k{+}1. A central rollout manager also implements robust handling of partially-failed trajectories so a single sandbox failure cannot crash the optimizer.

Our Agentic Loop Driver is hardened with a multi-layer timeout and retry hierarchy (sandbox creation, LLM inference, observation execution, sandbox shutdown, total reward evaluation) that bounds tail latency without sacrificing end-to-end reliability. When a sandbox crash is attributed to resource exhaustion, CPU and memory allocations are automatically escalated on retry, and small random jitter is injected before sandbox creation to prevent thundering-herd effects when hundreds of concurrent trajectories spin up simultaneously. Loss masking is applied in token space so that gradients flow only through assistant-generated tokens; tool-result tokens are explicitly masked out, which makes multi-turn agentic RL well-defined under a standard LM cross-entropy objective.

3.3.3 Balanced Adaptive Rollout (BAR)

For challenging agentic tasks such as SWE, the standard fixed-NN group rollout used by GRPO (Shao et al., 2024) has two major problems:

•

Wasted compute. When the policy is competent on a prompt, all NN trajectories tend to succeed; when it is not, all NN tend to fail. In both regimes the resulting group has zero reward variance, contributes a zero advantage to every token, and is silently discarded — yet we have already paid for NN long, environment-bound trajectories.

•

Group-imbalance noise. When the success rate of a prompt is far from 0.50.5, even a "non-degenerate" group is dominated by either positives or negatives, and the resulting GRPO advantages are noisy and biased toward whichever class is over-represented.

Algorithm 1  Balanced Adaptive Rollout (BAR) for a single prompt

1:Prompt xx; policy πθ\pi_{\theta}; reward function RR; environment factory ℰ\mathcal{E}

2:Training group size NN; max budget NmaxN_{\max}; stride ss (with s∣Nmaxs\mid N_{\max})

3:Positive-fraction interval [ρmin,ρmax][\rho_{\min},\rho_{\max}]; ideal ratio ρ⋆=(ρmin+ρmax)/2\rho^{\star}=(\rho_{\min}+\rho_{\max})/2

4:A training group 𝒢⊂𝒯\mathcal{G}\subset\mathcal{T} with |𝒢|=N|\mathcal{G}|=N

5:𝒯←∅\mathcal{T}\leftarrow\emptyset ⊳\triangleright pool of completed trajectories

6:for t=0,s,2​s,…,Nmax−st=0,s,2s,\ldots,N_{\max}-s do

7:  ℬt←{τi∼πθ(⋅∣x;ℰ):i=1,…,s}\mathcal{B}_{t}\leftarrow\{\,\tau_{i}\sim\pi_{\theta}(\cdot\mid x;\mathcal{E})\,:\,i=1,\ldots,s\,\} ⊳\triangleright generate stride in parallel

8:  ri←R​(τi)r_{i}\leftarrow R(\tau_{i}) for each τi∈ℬt\tau_{i}\in\mathcal{B}_{t}

9:  𝒯←𝒯∪ℬt\mathcal{T}\leftarrow\mathcal{T}\cup\mathcal{B}_{t}

10:  (𝒢,ok)←TryAssemble​(𝒯,N,ρmin,ρmax,ρ⋆)(\mathcal{G},\texttt{ok})\leftarrow\textsc{TryAssemble}(\mathcal{T},N,\rho_{\min},\rho_{\max},\rho^{\star})

11:  if ok then

12:   return 𝒢\mathcal{G} ⊳\triangleright early-stop: balanced group found

13:  end if

14:end for

15:(𝒢,ok)←TryAssemble​(𝒯,N,0,1,ρ⋆)(\mathcal{G},\texttt{ok})\leftarrow\textsc{TryAssemble}(\mathcal{T},N,0,1,\rho^{\star}) ⊳\triangleright relaxed fallback

16:if ok then return 𝒢\mathcal{G}

17:end if

18:return TopRanked​(𝒯,N)\textsc{TopRanked}(\mathcal{T},N) ⊳\triangleright best-effort fallback

19:

20:procedure TryAssemble(𝒯,N,ρmin,ρmax,ρ⋆\mathcal{T},N,\rho_{\min},\rho_{\max},\rho^{\star})

21:  Partition 𝒯\mathcal{T} into 𝒯+={τ:R​(τ)>0,τ​ usable}\mathcal{T}_{+}=\{\tau:R(\tau)>0,\ \tau\text{ usable}\},
𝒯−={τ:R​(τ)≤0,τ​ usable}\mathcal{T}_{-}=\{\tau:R(\tau)\leq 0,\ \tau\text{ usable}\},
and put the rest (aborted / time-exceeded) into a backfill pile.

22:  Sort 𝒯+\mathcal{T}_{+} and 𝒯−\mathcal{T}_{-} by (status, response length) ascending
⊳\triangleright prefer completed and concise

23:  if |𝒯+|+|𝒯−|<N|\mathcal{T}_{+}|+|\mathcal{T}_{-}|<N then return (⊥,false)(\bot,\ \texttt{false})

24:  end if

25:  n⋆←round​(ρ⋆⋅N)n^{\star}\leftarrow\mathrm{round}(\rho^{\star}\cdot N); nmin←⌈ρmin⋅N⌉n_{\min}\leftarrow\lceil\rho_{\min}\cdot N\rceil; nmax←⌊ρmax⋅N⌋n_{\max}\leftarrow\lfloor\rho_{\max}\cdot N\rfloor

26:  for n+∈{nmin,…,nmax}n_{+}\in\{n_{\min},\ldots,n_{\max}\} sorted by |n+−n⋆||n_{+}-n^{\star}| do

27:   n−←N−n+n_{-}\leftarrow N-n_{+}

28:   if n+≤|𝒯+|n_{+}\leq|\mathcal{T}_{+}| and n−≤|𝒯−|n_{-}\leq|\mathcal{T}_{-}| then

29:     return (𝒯+[1:n+]∪𝒯−[1:n−],true)(\mathcal{T}_{+}[1{:}n_{+}]\cup\mathcal{T}_{-}[1{:}n_{-}],\ \texttt{true})

30:   end if

31:  end for

32:  return (⊥,false)(\bot,\ \texttt{false})

33:end procedure

We address both issues with Balanced Adaptive Rollout (BAR), a progressive, group-aware rollout algorithm.
Unlike prior dynamic sampling and difficulty-filtering methods that discard zero-variance prompts
(Yu et al., 2025; Le et al., 2025), pre-filter prompts using historical success rates
(Bae et al., 2026; Zheng et al., 2025b), or post-hoc down-sample oversized rollout sets
(Xu et al., 2025; Shang et al., 2025; Zhang et al., 2026c), our method performs online,
per-prompt, stride-based group assembly. It adaptively continues generation only until it can
construct a fixed-size training group whose positive-reward fraction lies in a target interval,
while accounting for trajectory status, truncation, sandbox failures, and length. This makes the
rollout scheduler directly compatible with group-relative estimators in long-horizon agentic
environments.

For each prompt we set three quantities: a training group size NN (the number of trajectories the optimiser will actually consume), a maximum budget Nmax>NN_{\max}>N (an upper bound on how many trajectories we are willing to generate), and a stride ss (the size of an incremental generation batch). We additionally specify a target positive-reward fraction interval [ρmin,ρmax][\rho_{\min},\rho_{\max}], with ideal ratio ρ⋆=(ρmin+ρmax)/2\rho^{\star}=(\rho_{\min}+\rho_{\max})/2.
The algorithm proceeds as follows. We generate ss trajectories, score them with the reward model, and partition the pool of completed trajectories into a positive set (trajectories with reward >0>0) and a negative set (otherwise), after first moving aborted or truncated trajectories to a backfill pile. We then attempt to assemble a training group of exactly NN trajectories whose positive fraction lies in [ρmin,ρmax][\rho_{\min},\rho_{\max}] and that is closest to the ideal ratio ρ⋆\rho^{\star}. Within each class, trajectories are ranked by terminal status (completed > truncated > aborted)666Other ranking criteria could also be used here, for example ranking by trajectory length, model likelihood, diversity, or estimated uncertainty, etc.
, so as to prefer succinct, well-terminated trajectories. If a feasible group exists, we early-stop and return it; otherwise we generate another stride and retry. If after NmaxN_{\max} trajectories no balanced group can be built, we fall back to a relaxed selection (any positive fraction in (0,1)(0,1)), padding with the best backfill trajectories as needed.

BAR therefore behaves as an anytime, self-pacing rollout schedule: 1) Easy or already-mastered prompts (where the first stride is overwhelmingly positive) trigger no further generation — the prompt is either filtered out or returned with the minimum positives needed to satisfy the lower bound; 2) Hard prompts (where positives are rare) keep generating until either enough positives are discovered or the budget NmaxN_{\max} is exhausted; 3) Well-balanced prompts finish near the first stride and yield maximally informative gradients.
The result increases the average information density of every gradient batch. Importantly, BAR composes cleanly with GRPO, GSPO (Zheng et al., 2025a), and any other group-relative advantage estimator, because the contract it must satisfy is simply “return a list of NN trajectories per prompt”.

Final Group Filtering.

Because rewards are produced by a noisy, real environment (sandbox creation can time out, containers can be evicted, the LLM can hit its token budget mid-step), some trajectories carry no usable learning signal even within an otherwise valid group. We therefore plug a group-level filter into the rollout loop, evaluated after reward computation and before the group is admitted into the training batch. A dropped group is simply replenished from over-sampled prompts, which decouples the training batch size from the generation batch size and preserves gradient quality.

Filtering and the Balanced Adaptive Rollout (BAR) are designed to work jointly: BAR maximises the probability that a generated group satisfies the filter on the first try, and the filter provides a hard correctness guarantee on whatever BAR returns. Together they implement a form of reward-aware curriculum that is performed online, at every gradient step.
Together these components form a fault-tolerant, throughput-optimised pipeline for end-to-end RL on long-horizon, sandbox-grounded agentic tasks, with Balanced Adaptive Rollout turning a fixed-batch rollout into a self-pacing, information-dense one. The final estimated advantage for rollout trajectory oio_{i} within a group of NN samples is normalized using the group rewards
Ai,t=ri−mean⁡({r1,r2,…,rN})std⁡({r1,r2,…,rN})A_{i,t}=\frac{r_{i}-\operatorname{mean}(\{r_{1},r_{2},\ldots,r_{N}\})}{\operatorname{std}(\{r_{1},r_{2},\ldots,r_{N}\})}
, where rir_{i} denotes the reward assigned to rollout trajectory oio_{i}. RL train for at most 150 steps with a global batch size of 128 and a 64K context window, using a cosine-decayed learning rate from 10−610^{-6}. We apply the rollout batch size 16 with a training group size of N=8N=8, maximum budget Nm​a​x=16N_{max}=16 and stride s=16s=16 to encourage parallel rollout. The target positive-reward fraction interval used is [ρmin,ρmax]=[0.375,0.625][\rho_{\min},\rho_{\max}]=[0.375,0.625].

Data Selection.

Table 6: Performance on SWE-rebench v2. Baseline results with † are from Badertdinov et al. (2026), where models were evaluated on a 60-task Python subset. Orchard-SWE is evaluated on the full Python subset. All models are evaluated using the mini-swe-agent harness.

Model
pass@1
pass@3

Claude Opus-4.5†

36.11%
36.67%

GLM-4.7†

27.22%
31.67%

MiniMax-M2.1†

26.11%
31.67%

Gemini†

25.56%
33.33%

DeepSeek-V3.2†

23.33%
31.67%

GPT-5.2†

20.56%
23.33%

gpt-oss-120b†

8.89%
16.67%

Orchard-SWE
22.36%
27.94%

For RL training, we construct a task pool using all Python subset data of SWE-rebench V2 and Scale-SWE data that were not used during SFT. We first run the initial SFT model on each candidate task with 8 rollouts to get its initial pass rate. Table 6 reports the performance of initial Orchard-SWE SFT checkpoint on SWE-rebench V2 alongside baseline results. Orchard-SWE SFT achieves 22.36% pass@1 and 27.94% pass@3 on the full Python subset. We then retain only tasks with pass rate 0<p^≤0.50<\hat{p}\leq 0.5, filtering out tasks that are either too difficult to provide a reliable learning signal or already too easy for the SFT model. This selection is particularly important for SWE-rebench V2, which is highly challenging: as shown in the table, even state-of-the-art models achieve 20%-40% pass rates on this dataset. After filtering, the final RL training set contains approximately 2k instances. We apply mini-swe-agent as the harness for RL training.

3.4 Main Results

Table 7 compares Orchard-SWE with open-source SWE-agent recipes on SWE-bench Verified, organized by base-model family.
Orchard-SWE achieves 64.3% after SFT and 67.5% with the full SFT+RL recipe, using only ∼\sim3B active parameters: its Qwen3-30B-A3B MoE backbone activates 3B of 30B total parameters at inference.
At this active-parameter budget, Orchard-SWE is competitive with or exceeds dense open baselines that activate an order of magnitude more parameters: it surpasses every Qwen 2.5 32B and Qwen 3 32B open-source recipe in the table—including OpenSWE-32B (62.4% with SWE-Agent), SWE-Master-32B-RL (61.4%), CoderForge-32B (59.4%), and SWE-Mirror-LM (52.2%)—and surpasses the strongest dense 72B systems (Kimi-Dev 60.6%, OpenSWE-72B 65.0–66.0%).

Table 7: Resolve rates (%) on SWE-bench Verified. We compare Orchard-SWE against open-source SWE-agent recipes, organized by base-model family. Within each section, rows are sorted by reported resolve rate. Orchard-SWE rows are bolded. For broader context, frontier proprietary systems (Claude Opus 4.5, GPT-5.2, Gemini 3, etc.) reach 71–77% on this benchmark.

System
Base Model
Harness
Resolved (%)

Open-Source Methods: Qwen 2.5 32B Coder Series

R2EGym-Agent (Jain et al., 2025)

Qwen2.5-32B-Coder-Base
R2E-Gym
34.4

Openhands-LM (Wang et al., 2025b)

Qwen2.5-Coder-32B-Inst.
OpenHands
37.2

Skywork-SWE (Zeng et al., 2025)

Qwen2.5-Coder-32B-Inst.
OpenHands
38.0

SWE-Agent-LM (Yang et al., 2025a)

Qwen2.5-Coder-32B-Inst.
SWE-Agent
40.2

SWE-Mirror-LM (Wang et al., 2025a)

Qwen2.5-Coder-32B-Inst.
MOpenHands
52.2

SWE-Compressor (Liu et al., 2025)

Qwen2.5-32B-Base
OpenHands
57.6

SWE-Master-32B (Song et al., 2026)

Qwen2.5-Coder-32B-Inst.
R2E-Gym
57.8

SWE-Master-32B-RL (Song et al., 2026)

Qwen2.5-Coder-32B-Inst.
R2E-Gym
61.4

Open-Source Methods: Qwen 3 32B Series

FrogBoss (Sonwane et al., 2025)

Qwen3-32B
R2E-Gym
54.6

SWE-Lego-Qwen3-32B (Tao et al., 2026)

Qwen3-32B
OpenHands
52.6

CoderForge-32B (Ariyak et al., 2026)

Qwen3-32B
OpenHands
59.4

Open-Source Methods: Qwen 2.5 32B Series

daVinci-Dev-32B (Zeng et al., 2026)

Qwen2.5-32B-Base
SWE-Agent
56.1

OpenSWE-32B (Fu et al., 2026)

Qwen2.5-32B-Base
OpenHands
59.8

OpenSWE-32B (Fu et al., 2026)

Qwen2.5-32B-Base
SWE-Agent
62.4

Open-Source Methods: Qwen 2.5 72B Series

SWE-Fixer-72B (Xie et al., 2025)

Qwen2.5-72B-Base
Agentless
32.8

daVinci-Dev-72B (Zeng et al., 2026)

Qwen2.5-72B-Base
SWE-Agent
58.5

Kimi-Dev (Yang et al., 2025b)

Qwen2.5-72B-Base
Agentless
60.6

OpenSWE-72B (Fu et al., 2026)

Qwen2.5-72B-Base
OpenHands
65.0

OpenSWE-72B (Fu et al., 2026)

Qwen2.5-72B-Base
SWE-Agent
66.0

Same-size baselines and our model (30B-A3B; ~3B active)

Qwen3-30B-A3B-Instruct
—
OpenHands
22.0

Qwen3-Coder-30B-A3B-Instruct
—
OpenHands
51.6

GLM-4.7-Flash-30A3B (Team et al., 2025)

—
—
59.2

Scale-SWE-Agent (Zhao et al., 2026)

Qwen3-30B-A3B-Instruct
OpenHands
64.0

Orchard-SWE (SFT)
Qwen3-30B-A3B-Thinking
mini-swe-agent
64.3

Orchard-SWE (SFT)
Qwen3-30B-A3B-Thinking
OpenHands
62.1

Orchard-SWE (SFT+RL)
Qwen3-30B-A3B-Thinking
mini-swe-agent
67.5

Same-size family lift.

The cleanest apples-to-apples comparison is within the 30B-A3B family.
Orchard-SWE improves over Qwen3-30B-A3B-Instruct by a 45.5% absolute lift on SWE-bench Verified (22.0%→64.3%22.0\%\to 64.3\% after SFT, →67.5%\to 67.5\% after SFT+RL), and also exceeds the code-specialized Qwen3-Coder-30B-A3B-Instruct (51.6%) and the broader-distillation GLM-4.7-Flash-30A3B (59.2%) by wide margins.
The closest competitor at comparable scale is Scale-SWE-Agent (64.0%), built on the same backbone family. Orchard-SWE matches it under SFT and outperforms it under SFT+RL.
This isolates the effect of the Orchard-SWE recipe itself—multi-teacher distillation, multi-harness collection, credit-assignment SFT, and RL—rather than any advantage from the underlying base model.

The same Orchard-SWE (SFT) checkpoint reaches 64.3% under the mini-swe-agent harness but 62.1% under OpenHands, indicating that single-condition leaderboard numbers are sensitive to harness choice.
This sensitivity becomes the central empirical question of Section 3.5: when we evaluate our Orchard-SWE alongside the closest open recipes (Scale-SWE, OpenSWE-32B) across multiple harnesses and tasks, the differences become much more pronounced—Orchard-SWE retains capability on unseen harnesses and out-of-distribution tasks, while other models collapse.

3.5 Generalization to Unseen Harnesses and Tasks

On SWE-bench Verified, Scale-SWE (64.0%) and OpenSWE-32B (62.4%) report similar resolve rates.
Single-benchmark scores can mask large differences in how well an agent generalizes. We therefore evaluate each model across three harnesses—OpenHands, mini-swe-agent, and Kimi-CLI (Moonshot AI, 2026)—and three different tasks: SWE-bench Verified, SWE-bench Multilingual (Yang et al., 2025a), and Terminal-Bench 2.0 (Merrill et al., 2026).
Kimi-CLI was not used during training data collection by any of the three systems, making it an unseen harness in this study.

Table 8: Generalization across harnesses and task distributions. Resolve rate (%) is reported for each system under matched conditions. SWE-V = SWE-bench Verified; SWE-M = SWE-bench Multilingual; T-Bench 2.0 = Terminal-Bench 2.0. * denotes numbers reported in the original paper; unmarked entries are our own evaluations under matched conditions; ✗ indicates the system produced malformed tool calls under that harness, yielding no valid resolve rate.

OpenHands
mini-swe-agent
Kimi-CLI (unseen)

System
SWE-V
SWE-V
SWE-M
SWE-V
T-Bench 2.0

Scale-SWE
64.0∗

✗
✗
✗
✗

OpenSWE-32B
62.4∗

54.9
28.7
3.6
0.0

Orchard-SWE
62.1
64.3
51.0
45.0
20.1

Harness lock-in is severe in single-harness training.

We find Scale-SWE (Zhao et al., 2026) produces invalid outputs under any harness other than its native one, yielding no measurable resolve rate.
OpenSWE-32B (Fu et al., 2026) remains structurally valid but degrades sharply: from 62.4% on its native OpenHands to 54.9% on mini-swe-agent (−7.5-7.5 pt) and 3.6% on Kimi-CLI (−58.8-58.8 pt).
Orchard-SWE, in contrast, holds within a narrow band of 45.0–64.3% across all three harnesses, with the worst-case drop bounded at 19.3 points relative to its own best.
The two failure modes observed in Scale-SWE and OpenSWE-32B (catastrophic format failure and degraded resolve rate) have the same root cause - a model trained under a single harness has not learned harness-agnostic SWE skills.
This pattern is exactly what our cross-harness ablation predicted in a controlled experiment (Section 3.6, Table 10); here we see the same failure mode play out in two independently developed open recipes.

Cross-distribution generalization.

On SWE-bench Multilingual under the mini-swe-agent harness, Orchard-SWE drops from 64.3% (Verified) to 51.0% (−13.3-13.3 absolute, −20.7%-20.7\% relative).
OpenSWE-32B drops from 54.9% to 28.7% (−26.2-26.2 absolute, −47.7%-47.7\% relative).
Orchard’s relative drop is roughly half, indicating that multi-teacher distillation across SWE-rebench and Scale-SWE provides broader exposure to repositories and issue types than any single source alone.

Cross-domain transfer to Terminal-Bench 2.0.

Terminal-Bench 2.0 evaluates a broader family of terminal interaction tasks beyond GitHub-issue resolution.
Under the Kimi-CLI harness, Orchard-SWE retains a 20.1% resolve rate, while OpenSWE-32B drops to 0.0%.
Both systems degrade substantially relative to their SWE-bench Verified scores, but only Orchard-SWE retains a non-trivial level of capability on this out-of-domain benchmark.
We hypothesize that broader trajectory diversity during training—multiple teachers, multiple harnesses, and multiple task sources—provides indirect exposure to more varied tool-use and terminal-interaction patterns than narrower training corpora.

Discussion.

Two largely independent generalization improvements are visible in Table 8: robustness across harnesses and across tasks. Both trace to diversity choices at three layers of the Orchard-SWE recipe. Data design: trajectories span two harnesses (mini-swe-agent, OpenHands), two teachers (MiniMax-M2.5, Qwen3.5-397B), and three complementary task sources (SWE-rebench, SWE-rebench V2, Scale-SWE), yielding 107K trajectories that vary along harness, repository structure, and issue type. Orchard Env: it makes data-design choices practical at scale and exposes only sandbox lifecycle, command execution, and file I/O—and imposes no assumptions about the harness or tool schema sitting above it—any harness can compose with the same env layer at zero adaptation cost. Learning design: credit-assignment SFT extracts partial-progress supervision from the unresolved trajectories that resolved-only recipes discard, broadening the exploration patterns the student sees, while Balanced Adaptive Rollout (BAR) keeps RL gradients informative by enforcing a balanced positive/negative mix within each prompt group across the full difficulty spectrum.
However, Orchard-SWE improves but does not solve generalization: the drop to Kimi-CLI Verified is substantial, and entirely-unseen harness or domain conditions still remain a meaningful challenge.

3.6 Ablations and Analysis

We ablate key design choices in Orchard-SWE to understand their contribution to the final result. Four questions guide our analysis: (i) how does data scale interact with selection strategy? (ii) how harness-coupled is the trained model? (iii) what does credit-assignment SFT contribute over resolved-only training? (iv) what does reinforcement learning add on top of large-scale SFT?

Data scale vs. selection strategy.

We first study how SFT performance depends on training data scale and selection strategy.
We hold the recipe fixed—same base model, same SFT hyperparameters, mini-swe-agent harness, no RL—and vary only the number of training trajectories (N∈{512,1024,2048}N\in\{512,1024,2048\}) and the strategy used to choose those trajectories from the resolved-trajectory pool.
The selection strategies fall into two families:

1) Heuristic baselines (no use of gold-patch information): Random: uniform random sampling from the resolved pool.
Diverse repo: maximize repository diversity by capping per-repo trajectory count.
Concentrated repo: concentrate samples on the largest-resolved “core” repositories.
2) Property-based selectors (use gold-patch characteristics as a signal of issue complexity):
Multi-file: prefer instances whose gold patch modifies multiple files.
Large diff: prefer instances with larger gold-patch diffs.
Composite: composite scoring over multiple gold-patch properties.

Table 9: Effect of data scale and selection strategy on SFT-only resolve rate (%) on SWE-bench Verified, evaluated under the mini-swe-agent harness. All cells use identical hyperparameters; only NN (trajectories) and selection strategy vary. Per-row maximum is bolded.

Heuristic baselines
Property-based selectors

NN
Random
Diverse repo
Concentrated repo
Multi-file
Large diff
Composite

512
45.8
44.0
47.9
47.5
49.5
48.1

1024
50.3
49.0
51.3
51.6
52.2
52.1

2048
52.8
52.2
53.4
54.2
52.9
53.7

Table 9 reports SFT-only resolve rates on SWE-bench Verified.
Two patterns dominate the table.
First, data scale dominates selection strategy at every regime tested.
Doubling data twice (512 →\to 2048) on the worst-performing method (Diverse repo, 44.0 →\to 52.2) yields a +8.2-point gain, larger than the entire 5.5-point spread across all selection strategies at N=512N=512 and far larger than the 2.0-point spread at N=2048N=2048.
Second, the spread across selection strategies shrinks monotonically with NN: 5.5 pt at N=512N=512, 3.2 pt at N=1024N=1024, 2.0 pt at N=2048N=2048.
At sufficient data scale, the choice of selection strategy matters much less than scale itself.

A few specific behaviors are worth noting.
Large diff attains the strongest small-NN result (49.5 at N=512N=512) but saturates earliest, gaining only +0.7 pt from N=1024N=1024 to N=2048N=2048, plausibly because the pool of large-diff gold patches is finite and additional samples come from a distribution closer to the overall mean.
Counterintuitively, Concentrated repo beats Diverse repo at small NN by 3.9 points, with the gap shrinking to 1.2 points at N=2048N=2048: at small data scales, deeper exposure to a few repositories produces more transferable behaviors than thin coverage of many.
Property-based selectors edge out heuristic baselines at N=512N=512 but converge to the baselines by N=2048N=2048, suggesting that gold-patch heuristics function as a sample-efficient prior that random sampling matches given enough data.

Even the worst (method, scale) cell in Table 9 (44.0% at N=512N=512) lifts the resolve rate by 22 absolute points over the underlying base model (22.0%, Table 7), confirming that even a small dose of high-quality SFT trajectories provides most of the structural lift over the base.
However, the entire ablation grid plateaus around 54% under SFT-only at N=2048N=2048, while the full Orchard-SWE recipe—using the full 107K-trajectory corpus and adding RL—reaches 67.5% on SWE-bench Verified.

What makes this kind of scaling practical is Orchard Env itself.
Its thin, harness-agnostic service boundary lets the same env layer serve any harness, enabling multi-harness data collection and training at no additional infrastructure cost.
Image-agnostic agent injection allows arbitrary task images to be added to the corpus without per-image rebuilds.
Low command-execution latency (0.28 s; Section 2.3) keeps rollout throughput high.
And affordable cost (an order of magnitude cheaper than managed alternatives; Table 2) makes large scale data collection and RL rollout feasible for academic research groups.

Cross-harness generalization.

To assess whether harness choice during training affects the trained model’s ability to generalize at evaluation time, we run a controlled comparison: using 12K resolved trajectories on SWE-rebench distilled from MiniMax-M2.5, we vary only the collection harness and train two SFT models with otherwise identical recipes.
We then evaluate each model under both harnesses on SWE-bench Verified, yielding results in Table 10.

Table 10: Cross-harness generalization on SWE-bench Verified. Rows are the harness used to collect training trajectories, columns are the harness used at evaluation. All four cells use identical training data (12K resolved trajectories on SWE-rebench, MiniMax-M2.5 teacher) and the same SFT recipe; only the harness pairing differs. Diagonal entries (matched train/eval harness) are bolded.

Evaluation harness

Training harness
mini-swe-agent
OpenHands

mini-swe-agent
57.9
19.0

OpenHands
28.0
53.5

The cross-harness matrix reveals a sharp diagonal–off-diagonal gap.
Models evaluated on the same harness used during training reach 53.5–57.9% resolve rate, but performance collapses to 19.0–28.0% under the mismatched harness.
This asymmetry suggests that OpenHands trajectories, which expose richer tool semantics and more structured observations, transfer slightly better to the simpler mini-swe-agent setting than the reverse.
The dominant effect, however, is that the model has not learned harness-agnostic SWE skills: tool-call format, observation structure, and turn-level conventions are tightly coupled to the harness seen during training.
This finding aligns with concurrent observations on harness coupling reported in the Qwen3-Next-Coder report (Cao et al., 2026).
The implication is that no single-harness training corpus can produce an agent that generalizes well across the harness ecosystem; multi-harness training is necessary.

Effect of credit-assignment SFT.

We isolate the contribution of credit-assignment SFT through a controlled, scale-matched comparison.
Starting from the full resolved pool, we sub-sample 32K resolved trajectories so that the resolved baseline matches the 32,536 unresolved-trajectory rise segments in size, and train two SFT models with otherwise identical recipes:
(i) resolved-only (32K trajectories), and
(ii) resolved + unresolved with credit-assignment SFT (32K resolved + 32K rise-segment trajectories).
On SWE-bench Verified, the resolved-only baseline reaches 59.3%, while adding credit-assignment SFT improves the resolve rate to 61.2%—a gain of +1.9+1.9 points.
This gain validates that credit-assignment SFT extracts useful supervision from otherwise-discarded unresolved trajectories rather than fitting noise.
In the full Orchard-SWE recipe, the same signal compounds with the larger 74.6K-resolved corpus, contributing to the headline 64.3% on SWE-bench Verified.

Effect of reinforcement learning.

A natural question is how RL’s benefit depends on the strength of the SFT checkpoint it builds on—particularly for out-of-distribution generalization, where heavy SFT may leave less surface for RL to preserve cross-distribution capability. We compare RL initialized from two SFT checkpoints that differ by roughly two orders of magnitude in supervision: a moderate checkpoint (the Composite/N=512N{=}512 cell of Table 9, 48.1% on SWE-bench Verified) and a heavy checkpoint (the full 107K-trajectory recipe, 64.3%). We evaluate on SWE-bench Verified (in-distribution) and SWE-bench Multilingual (OOD) under mini-swe-agent.
The two initial models respond to RL very differently. From the moderate init, RL improves both axes, with the OOD gain larger than the in-distribution gain: Verified 48.1%→50.1%48.1\%\rightarrow 50.1\% (+2.0+2.0 pt) and Multilingual 22.0%→28.7%22.0\%\rightarrow 28.7\% (+6.7+6.7 pt). From the heavy init, RL still improves Verified (64.3%→67.5%64.3\%\rightarrow 67.5\%, +3.2+3.2 pt) but Multilingual slightly regresses. We read this as a specialization effect: heavy SFT places the policy on a sharper mode of the training distribution, so on-policy refinement sharpens in-distribution behavior at the cost of OOD transfer; a moderate base retains more behavioral diversity, so the same RL signal acts as broad-coverage refinement rather than narrow optimization.

4 Orchard-GUI

This section presents Orchard-GUI, our instantiation of the Orchard training recipe for multi-modal Graphical User Interface (GUI) agents.
We describe the problem setting, trajectory collection pipeline, two-stage training recipe, main results on evaluation benchmarks.

4.1 Problem Setting

We adopt the standard task formulation for browser-use agents: each task is defined by a starting URL and a natural-language user intent (e.g.,  “Find a dog bed on Amazon that is washable and has a length of at least 30 inches”).
The agent must navigate from the provided start_url within a browser interface, interact with live web pages, and complete the task by producing a natural-language final answer (or executing the requested action). Success is evaluated using an LLM-as-a-judge, which scores the trajectory against the user intent based on the final response and the sequence of screenshots.
We evaluate on three benchmarks: i) WebVoyager (He et al., 2024); ii) Online-Mind2Web (Deng et al., 2023); and iii) DeepShop (Lyu et al., 2025). We use the same evaluation protocol as FARA (Awadallah et al., 2025) and Molmo-Web (Gupta et al., 2026) for fair comparison.

4.2 Generic Tool-Calling Agent Harness

Rather than adopting a bespoke browser-agent harness such as Browser-Use777https://github.com/browser-use/browser-use, we intentionally employ a generic multi-turn ReAct-style loop (Yao et al., 2023). This design choice avoids conflating harness-specific effects with differences in data or training recipes, and more importantly, enables a unified paradigm for agentic learning that can generalize across domains and tasks beyond GUI navigation.

Specifically,
each episode begins with a system prompt that specifies the agent’s role, high-level operating guidelines, and an action schema defined in the standard OpenAI tools format.
Following standard GUI-agent practice, we define a fixed action space of 13 atomic tools using the OpenAI tool-calling interface: click, write, press_keys, scroll, wait, drag, hover, goto_url, go_back, new_tab, switch_tab, close_tab, and the terminal done(response). The done(response) action is the only mechanism for terminating an episode and serves as the sole carrier of the final user-facing output. Table 11 provides one-line summaries of each tool, with full argument specifications deferred to Appendix C.

Table 11: Browser action space: 13 atomic tools grouped by family. Full argument signatures are listed in Appendix C.

Category
Tool

Description

Pointer Mgmt.
click

Mouse-click at a screen pixel; supports single/double click and left/right/middle button.

hover

Move the cursor to a pixel to reveal tooltips or open dropdowns.

drag

Drag-and-drop from a start pixel to an end pixel.

Keyboard Mgmt.
write

Clear the focused input and type a string.

press_keys

Press one or more keys, sequentially or as a hotkey combo.

Page Nav.
scroll

Scroll the page or a sub-element by a fraction of the viewport.

goto_url

Navigate the current tab to a given URL.

go_back

Navigate back in the browser history.

wait

Pause for NN seconds to allow the page to settle.

Tab Mgmt.
new_tab

Open a new blank browser tab.

switch_tab

Switch to the tab with the given 0-based index.

close_tab

Close the current tab.

Termination
done

End the episode and emit the final user-facing answer.

The first user turn provides the task intent along with the initial browser observation, comprising the latest screenshot, viewport dimensions, and a tab summary (URL and title for each open tab) obtained from the task’s start_url.
At each subsequent step, the model produces a reasoning trace inside <think>...</think> followed by one or more <tool_call> blocks.
Each call is parsed and executed in the Orchard Env sandbox. The resulting tool response (e.g., Succeed: click on <button> "Continue shopping") is combined with the updated observation and appended to the context as the next user turn, framed as feedback to the preceding action.
This loop repeats until the model emits done or a predefined step budget is exhausted.

As a single screenshot can expand to thousands of vision tokens, naively concatenating the full screenshot history quickly inflates the context window beyond any reasonable training-time length: a 30-step rollout would saturate even a 64k-token context.
Empirically, the actionable information from earlier screenshots is already distilled into the agent’s prior reasoning traces, which themselves remain in context across turns.
We therefore retain only the last kk screenshots verbatim to reducing context length substantially. Here we show one example input to VLM for the last turn with k=1k=1 context image and the corresponding VLM response. See Appendix D for the complete trajectory.

Example Input to VLM

Example Response from VLM

All interactions are executed within the Orchard Env sandbox: each task runs in an isolated Playwright-controlled Chromium instance (2 vCPU, 8 GiB memory) with task-specific configurations. This isolation not only helps reproducibility for scenarios involving authentication, region-locked content, and rate-limited APIs, but also enables scalable parallel execution, significantly improving training and evaluation throughput.

4.3 Trajectory Collection and Curation

We construct the Orchard-GUI dataset through a three-stage pipeline: (i) sourcing and filtering raw task intents into a clean seed pool, (ii) sampling teacher trajectories on those tasks within the Orchard Env, and (iii) judge-based filtering and quality curation to produce the final SFT/RL splits.
Table 12 summarizes the composition of the collected dataset and the subsets used for SFT/RL.

Task sources.

We draw task instances from the task set provided by WebGym (Bai et al., 2026b), which contains 292,092 raw instances in total. To produce a clean, evaluation-safe, and diverse pool of training prompts, we apply a five-step filtering pipeline (Figure 4). The final filtered pool consists of 15,601 unique task intents, which serve as the seed set for sampling teacher trajectories used in SFT and RL. Of these, 2,537 come from PAE-WebVoyager (Zhou et al., 2025), and 13,064 come from InSTA (Trabucco et al., 2025).
These tasks span 13,063 unique hosts across six broad domain categories (Figure 5, left). They cover 425/500 (85.0%) of the MOZ Top-500 websites and 57/100 (57.0%) of the SimilarWeb Top-100 websites. Correspondingly, 48.5% (7,566) of tasks fall on a MOZ Top-500 host, and 13.0% (2,030) on a SimilarWeb Top-100 host (Figure 5, right). More detailed information is provided in Appendix E.
Note that the tasks used in the RL stage are drawn from the same task pool and processed using the same filtering pipeline, but with a more restrictive similarity-based deduplication threshold of 0.95, yielding a task set of 2,198 tasks (Table 12).

Figure 4: Task-filtering pipeline. Starting from 292,092292{,}092 raw tasks, we sequentially remove evaluation-benchmark overlap, child tasks, WebVoyager intents, long-tail sites, and near-duplicate intents (semantic similarity ≥0.99\geq 0.99 under Qwen3-Embedding-8B), yielding a final pool of 15,60115{,}601 deduplicated seed tasks on popular websites.

Figure 5: Composition of the filtered seed task pool.
Left: task share by top-level domain (66 categories spanning 15,60115{,}601 tasks).
Right: the seed task pool covers 57.0%57.0\% of SimilarWeb Top-100100 Most Visited and 85.0%85.0\% of MOZ Top-500500 Most Popular websites, with 13.0%13.0\% and 48.5%48.5\% of tasks landing on those respective lists.

Trajectory generation.

We use Qwen3-VL-235B-A22B-Thinking (Bai et al., 2025) as the sole teacher for trajectory distillation.
For each of the 15,60115{,}601 filtered seed tasks, we sample 44 independent rollouts through the Orchard Env under the same Tool-Calling Agent Harness described in Section 4.1, yielding a raw pool of 62,39562{,}395 teacher rollouts (a small fraction of attempts abort due to environment or rollout engine errors).
GPT-4.1 serves as the judge during data collection: its verdict on the final done(response) and the agent interaction history, together with the screenshot trail is used as a binary reward.
Under this judge, 68.4%68.4\% of tasks have at least one passing rollout, 26.3%26.3\% pass on all four rollouts, and the remaining 31.6%31.6\% fail on every rollout (Figure 6, left).
A non-trivial share of these failures is environmental rather than agentic: of the 4,9344{,}934 tasks that fail on every rollout, 41.1%41.1\% (2,0262{,}026 tasks; 13.0%13.0\% of the full pool) are captcha-blocked on all four attempts, leaving roughly 18.6%18.6\% of the pool that the teacher genuinely cannot solve.
Per-website success rates also vary widely (Figure 6, right), with anti-bot–prone hosts (e.g., dictionary.cambridge.org, bing.com) clustered at the low end.
Sampling four rollouts per task is intentional: the redundancy provides (i) pass-rate–based difficulty estimates and trajectory diversity for downstream curation, and (ii) a much larger candidate pool than we ultimately train on, letting us study data efficiency by training on small, carefully curated subsets rather than the full collection.

Figure 6: Per-website and per-task success outcomes.
Left: share of tasks by outcome across the 44 rollouts – 26.3%26.3\% all-pass, 42.1%42.1\% mixed (11–33 of 44), and 31.6%31.6\% all-fail. Of the all-fail tasks, 41.1%41.1\% (2,0262{,}026 tasks; 13.0%13.0\% of the full 15,60115{,}601-task pool) are captcha-blocked on every one of their four rollouts.
Right: per-website teacher success rate for the top-3030 websites by rollout count, split into two columns; bar length = number of rollouts, color = success rate.

Filtering and curation.

We first retain only rollouts whose final done(response) is judged success by GPT-4.1 (reward =1.0=1.0), and split the survivors by source benchmark, yielding 4,826 successful PAE-WebVoyager rollouts and 26,154 successful InSTA-v3 rollouts.
For SFT we deliberately avoid using the full successful pool: oversaturating the student on imitation data before RL tends to drive it into a narrow imitation regime that on-policy gradients struggle to escape, so we instead select a small, carefully curated subset.
We further restrict the SFT pool to PAE-WebVoyager: although PAE-WebVoyager contributes only 16%16\% of the seed tasks (2,5372{,}537 of 15,60115{,}601), 38.8%38.8\% of its tasks land on a SimilarWeb Top-100 site, versus just 3.6%3.6\% for InSTA-v3 (Table 12)—a ∼10×\sim 10\times density advantage on popular hosts that more closely reflect everyday user browsing habits.

Within the PAE-WebVoyager success pool we then apply two reductions to balance quality and diversity. (i) Within-task quality: for each task we keep a single rollout, namely the shortest successful trajectory (fewest turns, with ties broken by total response length), since shorter teacher trajectories tend to be cleaner and contain less recovery noise. (ii) Across-website diversity: we cap each website at K=20K=20 tasks, preventing high-volume hosts (e.g. amazon.com, coursera.org) from dominating the SFT mix.
The resulting SFT corpus comprises 412 unique tasks spanning 70 websites; per-source breakdowns and outcome statistics for this subset and the RL pool are reported in Table 12.

Table 12: Composition of the Orchard-GUI training dataset. The Full Set is the pool of seed tasks for which we collected rollouts. SFT Trajectories are the judge-passing rollouts used for supervised fine-tuning, and RL Tasks are seed prompts used to bootstrap rollout-based optimization. Tasks are classified by whether all rollouts succeeded (All Succ.), all failed (All Failed), or the outcome was mixed. The last column reports the number of tasks (restricted to those with at least one successful rollout) whose start URL falls on a SimilarWeb Top-100 site.

Subset
Source
# Tasks
All Succ.
All Failed
Mixed
Tasks on Top-100

Full Set
PAE-WebVoyager
2,5372{,}537
587587
800800
1,1501{,}150
985985

InSTA-v3
13,06413{,}064
3,5163{,}516
4,1344{,}134
5,4145{,}414
473473

Total
𝟏𝟓,𝟔𝟎𝟏\mathbf{15{,}601}
𝟒,𝟏𝟎𝟑\mathbf{4{,}103}
𝟒,𝟗𝟑𝟒\mathbf{4{,}934}
𝟔,𝟓𝟔𝟒\mathbf{6{,}564}
𝟏,𝟒𝟓𝟖\mathbf{1{,}458}

SFT Traj.
PAE-WebVoyager
𝟒𝟏𝟐\mathbf{412}
𝟏𝟐𝟖\mathbf{128}
𝟎\mathbf{0}
𝟐𝟖𝟒\mathbf{284}
𝟏𝟗𝟔\mathbf{196}

RL Tasks
PAE-WebVoyager
734734
189189
226226
319319
290290

InSTA-v3
1,4641{,}464
500500
461461
503503
469469

Total
𝟐,𝟏𝟗𝟖\mathbf{2{,}198}
𝟔𝟖𝟗\mathbf{689}
𝟔𝟖𝟕\mathbf{687}
𝟖𝟐𝟐\mathbf{822}
𝟕𝟓𝟗\mathbf{759}

4.4 Training Recipe

Our training recipe follows a two-stage pipeline: supervised fine-tuning (SFT) on teacher-distilled trajectories, followed by reinforcement learning (RL) with judge-based rewards.
Both stages use the Orchard Env as the execution backend.

Stage 1: Supervised fine-tuning.

We initialize from Qwen3-VL-4B-Thinking (Bai et al., 2025) and fine-tune on the curated teacher trajectories.
From each teacher rollout we generate one training example per assistant turn: the tt-th example carries the chat-template-serialized prefix up through turn tt and supervises only that turn’s assistant response.
The serialized prefix follows the Qwen chat template, with a system turn carrying the agent role and the OpenAI-format tool schema; an initial user turn with the task intent and the start_url observation; and the subsequent alternation of assistant turns (a <think>...</think> reasoning trace followed by one or more <tool_call> blocks) and user turns (the tool response wrapping an updated browser observation that includes the latest screenshot).
Following standard practice for long-horizon agent training, the loss is computed only on the final (target) assistant turn; the system prompt, the earlier assistant turns retained as in-context history, and every environment observation are masked out.
The vision encoder and multi-modal projector are kept frozen and only the language-model weights are updated, which preserves the backbone’s screenshot-grounding capability and concentrates SFT capacity on agent-specific reasoning and action prediction.
We train for 33 epochs with peak learning rate 10−510^{-5} under a cosine schedule with a 10%10\% linear warmup. Each optimizer step uses a per-device batch of 22 with 88-step gradient accumulation, giving a per-worker effective batch of 1616 and a global batch of 128 across 8 data-parallel workers.

Stage 2: Reinforcement learning.

Starting from the SFT checkpoint, we apply RL to improve the model’s ability to recover from errors and explore alternative paths under partial observability.
We optimize a multi-turn variant of GRPO (Shao et al., 2024): for each task we sample a group of GG trajectories from parallel browser instances, compute a group-relative advantage from the trajectory-level reward, and broadcast it to every assistant-response token across all turns; observation and environment-feedback tokens are masked out of the loss.
The reward combines a deterministic format check with a binary judge: a trajectory receives +1+1 when every assistant turn parses as a valid <think>+tool-call and the final done(response) is judged success by GPT-4.1 against the screenshot trail and user intent, −1-1 when the rollout terminates from repeated format failures, and 0 otherwise.
We use asymmetric PPO clipping (ϵlow=0.2\epsilon_{\mathrm{low}}{=}0.2, ϵhigh=0.28\epsilon_{\mathrm{high}}{=}0.28) without KL or entropy regularization, and intentionally omit the per-trajectory 1/Ti1/T_{i} loss normalization so that longer, harder tasks are not down-weighted.
To remove uninformative updates we apply DAPO-style trajectory-level dynamic sampling (Yu et al., 2025)—dropping groups whose rewards are all 0 or all +1+1—and additionally zero the loss mask for judge API failures and captcha-aborted runs via the remove_sample mechanism, so that infrastructure noise does not leak into the policy update.
We further adopt a step-budget curriculum within RL: we first run RL with the per-episode step budget capped at 15 until performance saturates, then continue training from that checkpoint with the budget raised to 30. The short-horizon phase produces dense reward signal cheaply on tasks the policy can already solve within 15 steps, while the long-horizon phase extends the policy to harder tasks that genuinely require more interaction.

Figure 7: Orchard-GUI RL Training and Evaluation Curve. The red curves denote RL training starting from our SFT checkpoint, while the blue curves denote RL training initialized from the base model. Compared with the base model initialization, the SFT-initialized model achieves consistently higher evaluation success rates and more stable reward improvements throughout training.

4.5 Main Results

Table 13 compares Orchard-GUI with proprietary VLMs, prior open-source GUI agents, and same-scale baselines on WebVoyager, Online-Mind2Web, and DeepShop.
After two-stage training, Orchard-GUI reaches 74.1% / 67.0% / 64.0% on WebVoyager / Online-Mind2Web / DeepShop, for a 68.4% average—the strongest open-source result by a wide margin and competitive with the best proprietary system (Gemini computer-use-preview, 69.3% avg) despite a 4B backbone and only 2.6k training tasks. RL contributes most of this gain, lifting the SFT checkpoint by +13.9 / +20.0 / +15.3 absolute points across the three benchmarks (52.0% →\rightarrow 68.4% average).

Four findings stand out.
First, on WebVoyager Orchard-GUI is on par with the strongest open-source baselines (74.1% vs. MolmoWeb-4B’s 75.2% and MolmoWeb-8B’s 78.2%) while consuming roughly two orders of magnitude fewer training tasks (2.6k vs. >>278.5k). WebVoyager covers only 15 popular sites with relatively short horizons, leaving little room to separate from baselines that have been heavily distilled on this exact distribution.

Second, on Online-Mind2Web and DeepShop Orchard-GUI substantially outperforms every previous open-source model—by +31.7 and +21.7 absolute points over MolmoWeb-8B, the strongest prior open baseline—and also surpasses its own 235B Qwen3-VL teacher by +3.3 / +7.3, demonstrating that environment-grounded RL extracts capability the teacher itself does not exhibit.

Third, the training dynamics in Figure 7 show that RL initialized from the SFT checkpoint consistently achieves higher evaluation success rates and more stable optimization behavior than RL initialized directly from the base model. While both settings obtain comparable training rewards, the SFT-initialized policy converges to substantially stronger generalization performance, ultimately reaching over 50% success on the evaluation set compared with below 40% for base-model initialization. This gap indicates that supervised initialization provides a crucial behavioral prior that stabilizes exploration and enables RL to more effectively translate reward optimization into downstream task success.

Table 13: GUI-agent success rates (%) across three open-web benchmarks. * marks numbers reported in FARA (Awadallah et al., 2025); † marks numbers reported in MolmoWeb (Gupta et al., 2026).

System
# Steps
# Tasks
WebVoyager
Online-M2W
DeepShop

\cellcoloravgblueAvg

Proprietary Models

GPT-5 (Axtree)†

30
–
70.6
41.9
40.7

\cellcoloravgblue51.1

Gemini-3-flash (Axtree)†

30
–
74.4
34.8
45.1

\cellcoloravgblue51.4

Gemini-3-flash (Axtree)†

100
–
85.6
44.8
55.3

\cellcoloravgblue61.9

GPT-4o (SoM)*
100
–
65.1
34.6
16.0

\cellcoloravgblue38.6

o3 (SoM)*
100
–
79.3
55.4
49.7

\cellcoloravgblue61.5

GPT-5 (SoM)*
100
–
90.6
57.7
49.1

\cellcoloravgblue65.8

OpenAI computer-use-preview*
100
–
70.9
58.3
24.7

\cellcoloravgblue51.3

Gemini computer-use-preview†

100
–
88.6
57.3
62.0

\cellcoloravgblue69.3

Open-Source Models

Holo1-7B†

30
>15.6k
55.4
–
–

\cellcoloravgblue–

UI-TARS-1.5-7B*
100
–
66.4
31.3
11.6

\cellcoloravgblue36.4

GLM-4.1V-9B-Thinking*
100
–
66.8
33.9
32.0

\cellcoloravgblue44.2

Fara-7B*
100
>123.2k
73.5
34.1
26.2

\cellcoloravgblue44.6

MolmoWeb-4B†

100
>278.5k
75.2
31.3
35.6

\cellcoloravgblue47.4

MolmoWeb-8B†

100
>278.5K
78.2
35.3
42.3

\cellcoloravgblue51.9

Qwen3-VL-4B-Thinking
30
–
49.0
32.0
33.3

\cellcoloravgblue38.1

Qwen3-VL-235B-A22B-Thinking
30
–
63.1
63.7
56.7

\cellcoloravgblue61.2

Orchard-GUI-4B-SFT
30
0.4k
60.2
47.0
48.7

\cellcoloravgblue52.0

Orchard-GUI-4B (SFT + RL)
30
2.6k
74.1
67.0
64.0

\cellcoloravgblue68.4

Finally, the largest gains appear on Online-Mind2Web, which spans a substantially broader and more diverse website distribution than either WebVoyager (15 fixed sites) or DeepShop (a single shopping vertical). Success on this benchmark therefore requires generalization to previously unseen interfaces rather than adaptation to a narrow site set. The fact that Orchard-GUI improves most strongly in this regime suggests that judge-grounded RL over a relatively small but diverse task pool can generalize across the open web more effectively than large-scale teacher distillation on narrow distributions, which is ultimately the practically relevant setting for deployable browser agents.

5 Orchard-Claw

This section presents Orchard-Claw, our instantiation of the Orchard training recipe for claw-based agents.
We describe the problem setting, trajectory collection methods, two-stage training recipe, main results on Claw-Eval (Ye et al., 2026), and ablations that isolate key design choices that impacts performance.

5.1 Problem Setting

Task and evaluation

We target multi-step daily workflow tasks formulated by Claw-Eval (Ye et al., 2026). Given a task instruction such as “Sort my inbox — which emails need a reply, which are notifications, and which are spam?”, the agent need to interact with a diverse set of daily tools such as “gmail_list_messages”, “gmail_get_message”, etc., to complete the task while being safe and robust.
Specifically, after the agent completes the task, the evaluation audits the entire agent trajectory to measure the completion, safety, and robustness of the agent using a combination of automated scripts and LLM-as-a-judge (Zheng et al., 2023; Xiong et al., 2026).
These three dimensions are aggregated into a single task score, task_score=safety×(0.8⋅completion+0.2⋅robustness)\text{task\_score}=\text{safety}\times(0.8\cdot\text{completion}+0.2\cdot\text{robustness}), and a task is counted as a pass if task_score≥0.75\text{task\_score}\geq 0.75.
We use Claw-Eval as our primary evaluation benchmark.

Agent harness and tool interface

We collect trajectories using two different agent harnesses: a ReAct-style harness defined by the Claw-Eval benchmark, and the ZeroClaw (ZeroClaw Labs, 2026) harness – a faster, more lightweight Rust version of the popular OpenClaw (OpenClaw Team, 2026).
All environment and harnesses are implemented in a docker runtime routed through the Orchard Env service: each task run (environment and harness) runs in an isolated sandbox (2 vCPU, 2 GiB memory), provisioned from a python-based image with ClawEval and ZeroClaw pre-installed.
During both SFT and RL, we train Orchard-Claw on both harnesses and study whether such end-to-end training helps the model better leverage advanced harnesses such as ZeroClaw and reach higher performance.

5.2 Trajectory Collection and Curation

As claw-based agents are relatively new, we conduct a preliminary study using Claude Opus 4.6 (Anthropic, 2026) to synthesize claw-agent tasks as our training set.

Task sources.

We draw seed tasks from two sources: (1) tasks from Claw-Eval, and (2) workflows from popular skills on ClawHub888We use the official OpenClaw CLI to access skills on https://clawhub.ai/. From these seeds, we prompt Opus 4.6 via claude-agent-sdk to synthesize new tasks in a four-step loop: (1) propose and filter task ideas; (2) generate the environment, files, tool server, and test script; (3) run MiniMax-M2.5 (MiniMax, 2026) as the solver to produce rollouts; (4) refine the task based on the rollouts to ensure feasibility and instruction clarity. Each task costs 4.9 USD to synthesize on average, yielding 192 tasks in total shared across the Claw-Eval and ZeroClaw harnesses.

Trajectory generation.

For simplicity, we distill SFT data from a single teacher model. We choose MiniMax-M2.5 for its strong performance. For each synthesized task, we sample five rollouts from MiniMax-M2.5 through Orchard Env under the corresponding harness (ReAct-style or ZeroClaw), and keep only the trajectories that complete the task.
To record training samples from complex harnesses such as ZeroClaw, we implement a proxy LLM server that records every LLM call (input and output) during the rollout. An example recorded (input and output) pair from the ZeroClaw harness is shown below.
Once a rollout finishes, each recorded (input and output) pair is grouped back as a trajectory for training (and also for reward computation during RL).
This yields 561 trajectories with 4537 training pairs in total for SFT.

Example Model Input in the ZeroClaw Harness

Example Response from LLM

5.3 Training Recipe

Our training recipe follows a two-stage pipeline: supervised fine-tuning (SFT) on teacher-distilled trajectories, followed by reinforcement learning (RL).
Both stages use Orchard Env as the execution backend.
We use Qwen3-30B-A3B-Thinking-2507 (Qwen Team, 2025) as the backbone model for our training.

Stage 1: Supervised fine-tuning.

We initialize from the base backbone and fine-tune on the curated teacher trajectories. Each training sample is an (input prompt, LLM response) pair logged by our proxy LLM server, and following Section 3.3 we mask the input and train only on the response.
We run SFT for 1 epoch with a global batch size of 16 and a 64k context window, using a cosine learning rate decayed from 10−510^{-5} to 10−610^{-6}, and apply left truncation to sequences exceeding the context window.
At inference time, we extend the context to the model’s maximum of 256k.

Stage 2: Reinforcement learning.

Starting from the SFT checkpoint, we apply RL to teach the model to recover from errors and explore alternative paths to task completion.
The reward is binary and environment-grounded: if a rollout passes all test scripts, every (input, output) pair in the rollout receives +1+1; otherwise, every (input, output) pair receives −1-1.
We optimize with standard GRPO (Shao et al., 2024; Guo et al., 2025) using a batch size of 8 and group size of 8 over 150 training steps.
Orchard’s sandbox parallelization is critical at this stage, allowing us to easily run 64 asynchronous rollout sandboxes per step, which substantially improves training throughput.
Additionally, during rollout we do not set a maximum step limit but rather a 10-minute wall-clock budget for each task.
We find this better accommodates the differing per-step latencies of different harnesses and different tool calls, and better matches real-world usage.
Rollouts that exceed the budget are aborted, and all of their turns are excluded from training.
In Figure 8 we plot the training and validation success rate and trajectory length over the course of RL training.

Figure 8: Orchard-Claw RL training curves.
Left: train and validation success rate over RL steps.
Right: train and validation episode length (number of agent turns per rollout).
Validation tasks are sampled from the ClawEval benchmark.
Both metrics rise steadily over the course of training, indicating that the agent learns to solve more tasks while also engaging in longer multi-turn interactions.

Table 14: Claw-agent performance on Claw-Eval. We use the general domain (0408) for evaluation. * marks numbers reported by Ye et al. (2026).

System
#Tasks

ClawEval(p​a​s​s3pass^{3})

ClawEval(p​a​s​s​@​3pass@3)

SOTA Large Language Models

Claude Opus 4.6*
–
70.8
80.8

GPT 5.4*
–
60.2
75.8

Gemini 3.1 Pro*
–
55.9
80.8

Qwen3.5 397A17B*
–
57.8
70.8

GLM 5 Turbo*
–
52.8
73.3

MiniMax M2.7*
–
49.7
72.0

MiniMax M2.5
–
47.2
65.2

Kimi K2.5*
–
36.6
67.1

Similar-size baselines and our model (30B-A3B; ~3B active)

Nemotron-3-nano-30b-a3b
–
26.1
57.8

Qwen3-30B-A3B-Thinking
–
14.3
39.8

Qwen3-Coder-30B-A3B-Instruct
–
30.4
49.7

Orchard-Claw (SFT)
0.2k
22.4
50.3

Orchard-Claw (SFT + RL)
0.2k
31.7
59.6

5.4 Main Results

Table 14 compares Orchard-Claw against large proprietary models and open models of similar size on Claw-Eval (Ye et al., 2026), using its native ReAct-style harness.
After our two-stage training, Orchard-Claw reaches 31.7% p​a​s​s3pass^{3} and 59.6% p​a​s​s​@​3pass@3, substantially outperforming its backbone and also surpassing code- and tool-call specialized models such as Qwen3-Coder-30B-A3B-Instruct (Cao et al., 2026) and Nemotron-3-nano-30b-a3b (NVIDIA, 2025), despite being trained on only 0.2k synthetic tasks.
RL contributes most of this gain, adding 9.3 absolute points on p​a​s​s3pass^{3} and 9.3 absolute points on p​a​s​s​@​3pass@3 over the SFT checkpoint. This suggests that even with limited synthetic data, RL is highly effective at refining agent behavior beyond what teacher distillation alone can offer.

Table 15: Cross-harness evaluation. ReAct* is the ReAct-style loop from the ClawEval benchmark. ZeroClaw is a lightweight, Rust version of the popular OpenClaw Harness.

ClawEval(p​a​s​s3pass^{3})
ClawEval(p​a​s​s​@​3pass@3)

Model
ReAct*
ZeroClaw
ReAct*
ZeroClaw

Qwen3-30B-A3B-Thinking
14.3
20.5 (+6.2)
39.8
44.7 (+4.9)

Qwen3-Coder-30B-A3B-Instruct
30.4
29.8 (-0.6)
49.7
54.7 (+5.0)

Orchard-Claw (SFT)
22.4
25.5 (+3.1)
50.3
62.1 (+11.8)

Orchard-Claw (SFT+RL)
31.7
41.0 (+9.3)
59.6
73.9 (+14.3)

In Table 15 we further evaluate Orchard-Claw under both its native ReAct-style harness and the more advanced ZeroClaw harness. Pairing Orchard-Claw (SFT+RL) with ZeroClaw lifts performance to 41.0% p​a​s​s3pass^{3} and 73.9% p​a​s​s​@​3pass@3, a +9.3 and +14.3 absolute improvement over the same model run under the ReAct-style harness. This gain is also the largest among all models in the comparison, including baselines such as Qwen3-Coder-30B-A3B-Instruct that benefit much less or even regress when switching to ZeroClaw.
We attribute this to our end-to-end training using the target harnesses during rollout, enabled by Orchard Env.
By exposing the agent to the harnesses during training, the agent learns to take advantage of the features — including but not limited to subagents, auto-compact, and more — that stronger harnesses offer at inference time.

6 Related Work

Interactive environment orchestration for agentic training.

Unlike traditional model training, the cornerstone of agentic training is interactive environment orchestration. It requires agents to execute actions, process feedback, and iterate through multi-turn trajectories within isolated sandboxes.
To address the specialized demand on the underlying intrastructure layer, there are two distinct design paradigms have emerged: integrated training stacks and decoupled environment services.

In the integrated paradigm, the execution environment is a sub-component embedded within a larger training or orchestration system.
This allows the co-design of the environment layer with specific training frameworks or agent harnesses, tailoring the infrastructure for a particular task pipeline.
MegaFlow (Zhang et al., 2026b) decomposes agentic training into three co-designed services (Model, Agent, Environment), having coordinated tens of thousands of concurrent agent tasks.
While it recognizes that the environment service should be independently scalable, the three services are co-designed for the Qwen training pipeline and are not designed to be composed with arbitrary external trainers or third-party harnesses.
ProRL Agent (Zhang et al., 2026a) achieves an important partial step toward decoupling: it separates rollout generation from the trainer via an HTTP service. However, its environment layer remains bound to agent scaffolding through AgentHandler plugins, so the harnesses cannot be swapped without modifying the environment configuration.
In contrast, Orchard Env’s REST API is explicitly decoupled from the training loop, the specific task and the agent harness. This modularity makes it uniquely amenable for open-source development and heterogeneous research environments.
By abstracting the environment into a standalone service, our environment orchestration supports the entire agentic development life cycles: trajectory distillation, on-policy rollouts and evaluation.

The second paradigm is decoupled environment services, where the execution environment is exposed as a thin, independent service with a minimal API surface, reusable across different training frameworks, agent scaffolds, and task domains.
Commercial platforms such as E2B (E2B, 2024) Daytona (Daytona, 2025) and Modal (Modal Labs, 2024) exemplify this approach for developer-facing use cases: they expose REST APIs or SDKs for sandbox lifecycle and code execution.
However, these platforms generally lack the fine-grained environment controls required for scalable RL training, e.g., tunable resource limits, heartbeat-based lifecycle management tied to training state, per-sandbox network isolation policies. Furthermore, the operational costs of these proprietary services are typically significantly higher than those of Orchard Env, which utilizes a Kubernetes-native design to maximize resource efficiency and minimize overhead.
Consequently, Orchard Env serves as a high-performance, cost-effective foundation specifically engineered for open-source research and agentic development.

Software engineering agents.

Automated software engineering has converged on a canonical task formulation: given a real GitHub issue and repository snapshot, produce a patch that passes the associated test suite. SWE-bench (Jimenez et al., 2024) and its human-validated SWE-bench Verified subset (OpenAI, 2024) operationalize this formulation and serve as the primary evaluation benchmark for Orchard-SWE.
Within this landscape, work has pursued two complementary directions: designing better agent scaffolds and scaling training data. On the scaffold side, SWE-agent (Yang et al., 2024) introduces a specialized agent-computer interface enabling structured file viewing, editing, and codebase search.
Its lightweight derivative, mini-swe-agent, serves as one of Orchard-SWE’s two training harnesses.
Additionally, OpenHands (Wang et al., 2025b) provides a full-featured multi-agent platform that is used as the second harness for Orchard-SWE.
On the data-scaling side, SWE-smith (Yang et al., 2025a) generates new training instances from arbitrary repositories via automated task synthesis, scaling task diversity, BugPilot (Sonwane et al., 2025) generates “unintentional” bugs by instructing agents to implement new features in a repository.
Orchard-SWE is orthogonal: rather than synthesizing new tasks, we scale trajectory quality through multi-teacher distillation from frontier models and partial-credit supervision on failed traces, drawing from real Github issues (Badertdinov et al., 2025; 2026; Zhao et al., 2026) as task sources.

GUI and browser navigation agents.

GUI agent research is organized around a set of complementary benchmarks that together span the structural diversity of real-world web and desktop tasks.
Mind2Web (Deng et al., 2023) introduces the first large-scale dataset of human-annotated cross-website tasks across 137 sites, establishing the dominant web-navigation evaluation suite. OSWorld (Xie et al., 2024) extends evaluation to full desktop environments with 369 real computer tasks across multi-application workflows, requiring agents to operate over screenshots with no DOM access.
WebVoyager (He et al., 2024) establishes an end-to-end web task benchmark using live websites with GPT-4V, serving as both a benchmark and a prompting-only baseline.
More recently, Online-Mind2Web revisits the Mind2Web task space in a fully live setting, removing the static snapshot shortcut, and DeepShop (Lyu et al., 2025) introduces a transactional e-commerce benchmark requiring multi-step reasoning under shopping constraints — both serving as held-out evaluation targets for Orchard-GUI. We evaluate Orchard-GUI on WebVoyager, Online-Mind2Web, and DeepShop — selected specifically because they represent structurally distinct task types, use live environments rather than static snapshots, and have no overlap in their action spaces or reward signals, making them a demanding testbed for a single unified model without benchmark-specific tuning.

The methodological progression in this field moves from these prompting-only baselines toward sophisticated training-heavy paradigms.
Early systems like WebVoyager (He et al., 2024) establish strong prompting baselines but leave substantial headroom for trained models.
The subsequent dominant paradigm focuses on supervised fine-tuning (SFT) on human or model-generated demonstrations.
Fara (Awadallah et al., 2025) introduces FaraGen, a scalable pipeline that proposes multi-step web tasks and filters successes via automatic verifiers to produce low-cost SFT data, yielding a screenshot-only 7B agent competitive with frontier models.
More recently, MolmoWeb (Gupta et al., 2026) assembles MolmoWebMix—a large curated blend of synthetic trajectories, human demonstrations, and atomic web-skill data—to train a fully open agent that achieves state-of-the-art among open-weight models across WebVoyager, Online-Mind2Web, and DeepShop.
A newer wave of research integrates reinforcement learning (RL) to enhance reasoning and out-of-distribution (OOD) performance. UI-TARS (Qin et al., 2025) pioneers an iterative RL data flywheel for GUI agents.
Recent open-source methods (Luo et al., 2025; Lu et al., 2025) focus more on improving grounding and reasoning quality through RL.
Despite these advances, cross-benchmark generalization across both online and offline environments under a unified training setup remains rare.
Orchard-GUI trained with Orchard Env as a harness-agnostic execution backend demonstrates that a single SFT+RL recipe applied on a 4B model achieves cross-domain generalization. This provides concrete evidence for the scalability and reusability of the proposed open development system.

Generalist long-running autonomous agents (Claw-agent).

Claw-style agents represent a shift from episodic domain tools (SWE/GUI) toward persistent, general-purpose partners. While domain agents optimize for specific environments with resetting memory, Claw-agents maintain persistent state and identity via structured artifacts. They leverage dynamic skill libraries (ClawHub), execute multi-step workflows across heterogeneous APIs, and use proactive heartbeats to sustain an ambient presence. This architectural divergence targets conversational alignment over open-ended horizons, making cross-harness generalization a central challenge as the agent’s tool surface continually expands.

Several recent benchmarks (Ye et al., 2026; Li et al., 2026; Bai et al., 2026a) establish the evaluation standards for Claws-type agents.
Claw-Eval (Ye et al., 2026) provides high-quality, human-curated scenarios that rigorously assess long-term planning and tool-calling stability. In contrast, ClawGym (Bai et al., 2026a) relies on automated data synthesis over mock workspaces to create a scalable data pipeline for training and evaluation.
Recent training innovations focus on efficiency and rapid adaptation.
MetaClaw (Xia et al., 2026) enables continual skill synthesis from failure trajectories and idle-period RL updates, showing gains on Claw benchmarks.
Furthermore, OpenClaw-RL (OpenClaw Team, 2026) treats live deployment signals, such as user feedback, as a continuous training source via Hindsight-Guided On-Policy Distillation.
Using Orchard-Env, we instantiate an end-to-end training pipeline, Orchard-Claw, to achieve continual improvement on Claw-Eval through a unified, harness-agnostic execution backend.

7 Conclusion

This paper presented Orchard, an open-source framework for scalable agentic modeling built around a thin, Kubernetes-native, harness-agnostic environment service. By decoupling sandbox management from agent harnesses, trainers, and task domains, Orchard Env makes trajectory collection, SFT, RL rollouts, and evaluation more reusable, reproducible, and cost-effective.
Across software engineering, GUI navigation, and personal-assistant workflows, Orchard demonstrates that a shared environment layer can support diverse agents and training recipes. Orchard-SWE, Orchard-GUI, and Orchard-Claw achieve strong results while showing improved transfer across harnesses, domains, and pipeline stages.
Overall, Orchard shows that scalable agentic progress depends on both infrastructure and training design. By making the environment service, training recipes, and data collection reusable across domains and harnesses, Orchard lowers the barrier to open, reproducible, and capability-focused research in agentic AI.

References

Anthropic (2026)
Introducing claude opus 4.6.

Note: https://www.anthropic.com/news/claude-opus-4-6Accessed: 2026-05-03

Cited by: §5.2.

A. Ariyak, J. Zhang, J. Wang, S. Zhu, F. Bianchi, S. Srivastava, A. Panda, S. Bharti, C. Xu, J. Heo, X. S. Wu, J. Zou, P. Liang, L. Song, C. Zhang, B. Athiwaratkun, Z. Zhou, and Q. Wu (2026)
CoderForge-Preview: SOTA open dataset for training efficient agents.

Together AI Blog.

Note: Project core leads: Alpay Ariyak; Zhongzhu Zhou; Qingyang Wu

External Links: Link

Cited by: Table 7.

A. Awadallah, Y. Lara, R. Magazine, H. Mozannar, A. Nambi, Y. Pandya, A. Rajeswaran, C. Rosset, A. Taymanov, V. Vineet, S. Whitehead, and A. Zhao (2025)
Fara-7b: an efficient agentic model for computer use.

arXiv:2511.19663.

Cited by: §4.1,
Table 13,
§6.

I. Badertdinov, A. Golubev, M. Nekrashevich, A. Shevtsov, S. Karasik, A. Andriushchenko, M. Trofimova, D. Litvintseva, and B. Yangel (2025)
Swe-rebench: an automated pipeline for task collection and decontaminated evaluation of software engineering agents.

arXiv preprint arXiv:2505.20411.

Cited by: §1,
§3.2,
§6.

I. Badertdinov, M. Nekrashevich, A. Shevtsov, and A. Golubev (2026)
SWE-rebench v2: language-agnostic swe task collection at scale.

arXiv preprint arXiv:2602.23866.

Cited by: §1,
§3.2,
Table 6,
§6.

S. Bae, J. Hong, M. Y. Lee, H. Kim, J. Nam, and D. Kwak (2026)
Online difficulty filtering for reasoning oriented reinforcement learning.

In Proceedings of the 19th Conference of the European Chapter of the Association for Computational Linguistics (Volume 1: Long Papers),

Rabat, Morocco,  pp. 700–719.

External Links: Document,
Link

Cited by: §3.3.3.

F. Bai, H. Song, S. Sun, D. Cheng, Y. Yang, C. Hao, R. Li, F. Chang, Y. Wei, R. Tao, B. Dai, J. Yang, and W. X. Zhao (2026a)
ClawGym: a scalable framework for building effective claw agents.

External Links: 2604.26904,
Link

Cited by: §6.

H. Bai, A. Taymanov, T. Zhang, A. Kumar, and S. Whitehead (2026b)
WebGym: scaling training environments for visual web agents with realistic tasks.

arXiv preprint arXiv:2601.02439.

Cited by: Appendix E,
§4.3.

S. Bai, Y. Cai, R. Chen, K. Chen, X. Chen, Z. Cheng, L. Deng, W. Ding, C. Gao, C. Ge, W. Ge, Z. Guo, Q. Huang, J. Huang, F. Huang, B. Hui, S. Jiang, Z. Li, M. Li, M. Li, K. Li, Z. Lin, J. Lin, X. Liu, J. Liu, C. Liu, Y. Liu, D. Liu, S. Liu, D. Lu, R. Luo, C. Lv, R. Men, L. Meng, X. Ren, X. Ren, S. Song, Y. Sun, J. Tang, J. Tu, J. Wan, P. Wang, P. Wang, Q. Wang, Y. Wang, T. Xie, Y. Xu, H. Xu, J. Xu, Z. Yang, M. Yang, J. Yang, A. Yang, B. Yu, F. Zhang, H. Zhang, X. Zhang, B. Zheng, H. Zhong, J. Zhou, F. Zhou, J. Zhou, Y. Zhu, and K. Zhu (2025)
Qwen3-vl technical report.

arXiv preprint arXiv:2511.21631.

Cited by: §4.3,
§4.4.

R. Cao, M. Chen, J. Chen, Z. Cui, Y. Feng, B. Hui, Y. Jing, K. Li, M. Li, J. Lin, Z. Ma, K. Shum, X. Wang, J. Wei, J. Yang, J. Zhang, L. Zhang, Z. Zhang, W. Zhao, and F. Zhou (2026)
Qwen3-Coder-Next technical report.

arXiv preprint arXiv:2603.00729.

Cited by: §3.6,
§5.4.

Daytona (2025)
Daytona: secure and elastic infrastructure for running AI-generated code.

Note: https://www.daytona.ioGitHub repository: https://github.com/daytonaio/daytona

Cited by: §1,
§2.2,
§6.

X. Deng, Y. Gu, B. Zheng, S. Chen, S. Stevens, B. Wang, H. Sun, and Y. Su (2023)
Mind2Web: towards a generalist agent for the web.

In Advances in Neural Information Processing Systems,  A. Oh, T. Naumann, A. Globerson, K. Saenko, M. Hardt, and S. Levine (Eds.),

Vol. 36,  pp. 28091–28114.

External Links: Link

Cited by: item 1,
§1,
§4.1,
§6.

E2B (2024)
E2B: open-source secure sandboxes for AI code execution.

Note: https://e2b.devGitHub repository: https://github.com/e2b-dev/E2B

Cited by: §1,
§2.2,
§6.

D. Fu, S. Wu, Y. Wu, Z. Peng, Y. Huang, J. Sun, J. Zeng, M. Jiang, L. Zhang, Y. Li, J. Hu, L. Liu, J. Hou, and P. Liu (2026)
DaVinci-env: open swe environment synthesis at scale.

External Links: 2603.13023,
Link

Cited by: §3.5,
Table 7,
Table 7,
Table 7,
Table 7.

D. Guo, D. Yang, H. Zhang, J. Song, P. Wang, Q. Zhu, R. Xu, R. Zhang, S. Ma, X. Bi, X. Zhang, X. Yu, Y. Wu, Z. F. Wu, Z. Gou, Z. Shao, Z. Li, Z. Gao, A. Liu, B. Xue, B. Wang, B. Wu, B. Feng, C. Lu, C. Zhao, C. Deng, C. Ruan, D. Dai, D. Chen, D. Ji, E. Li, F. Lin, F. Dai, F. Luo, G. Hao, G. Chen, G. Li, H. Zhang, H. Xu, H. Ding, H. Gao, H. Qu, H. Li, J. Guo, J. Li, J. Chen, J. Yuan, J. Tu, J. Qiu, J. Li, J. L. Cai, J. Ni, J. Liang, J. Chen, K. Dong, K. Hu, K. You, K. Gao, K. Guan, K. Huang, K. Yu, L. Wang, L. Zhang, L. Zhao, L. Wang, L. Zhang, L. Xu, L. Xia, M. Zhang, M. Zhang, M. Tang, M. Zhou, M. Li, M. Wang, M. Li, N. Tian, P. Huang, P. Zhang, Q. Wang, Q. Chen, Q. Du, and et al. (2025)
DeepSeek-r1 incentivizes reasoning in llms through reinforcement learning.

Nature 645 (8081),  pp. 633–638.

External Links: ISSN 1476-4687,
Link,
Document

Cited by: §5.3.

T. Gupta, P. Wolters, Z. Ma, P. Sushko, R. Y. Pang, D. Llanes, Y. Yang, T. Anderson, B. Zheng, Z. Ren, H. Trivedi, T. Blanton, C. Ouellette, W. Han, A. Farhadi, and R. Krishna (2026)
MolmoWeb: open visual web agent and open data for the open web.

External Links: 2604.08516

Cited by: §4.1,
Table 13,
§6.

H. He, W. Yao, K. Ma, W. Yu, Y. Dai, H. Zhang, Z. Lan, and D. Yu (2024)
WebVoyager: building an end-to-end web agent with large multimodal models.

arXiv preprint arXiv:2401.13919.

Cited by: item 1,
§1,
§4.1,
§6,
§6.

X. Hu, T. Xiong, B. Yi, Z. Wei, R. Xiao, Y. Chen, J. Ye, M. Tao, X. Zhou, Z. Zhao, Y. Li, S. Xu, S. Wang, X. Xu, S. Qiao, Z. Wang, K. Kuang, T. Zeng, L. Wang, J. Li, Y. E. Jiang, W. Zhou, G. Wang, K. Yin, Z. Zhao, H. Yang, F. Wu, S. Zhang, and F. Wu (2025)
OS agents: a survey on MLLM-based agents for computer, phone and browser use.

In Proceedings of the 63rd Annual Meeting of the Association for Computational Linguistics (Volume 1: Long Papers),

Vienna, Austria,  pp. 7436–7465.

External Links: Document,
Link

Cited by: §1.

N. Jain, J. Singh, M. Shetty, L. Zheng, K. Sen, and I. Stoica (2025)
R2e-gym: procedural environments and hybrid verifiers for scaling open-weights swe agents.

arXiv preprint arXiv:2504.07164.

Cited by: Table 7.

C. E. Jimenez, J. Yang, A. Wettig, S. Yao, K. Pei, O. Press, and K. R. Narasimhan (2024)
SWE-bench: can language models resolve real-world github issues?.

In The Twelfth International Conference on Learning Representations (ICLR),

External Links: Link

Cited by: §1,
§3.1,
§6.

A. Kim (2025)
Self-host open-source LLM agent sandbox on your own cloud.

Note: SkyPilot Blog, https://blog.skypilot.co/skypilot-llm-sandbox/SkyPilot Code Sandbox; GitHub: https://github.com/alex000kim/skypilot-code-sandbox

Cited by: §2.2,
§2.3,
Table 3.

T. V. Le, M. Jeon, K. Vu, V. Lai, and E. Yang (2025)
No prompt left behind: exploiting zero-variance prompts in llm reinforcement learning via entropy-guided advantage shaping.

arXiv preprint arXiv:2509.21880.

External Links: Link

Cited by: §3.3.3.

C. Li, Z. Tang, M. Huang, Y. Lin, S. Huang, S. Liu, B. Ye, R. Li, L. Li, B. Wang, and Y. Yuan (2026)
Claw-eval-live: a live agent benchmark for evolving real-world workflows.

External Links: 2604.28139,
Link

Cited by: §6.

S. Liu, J. Yang, B. Jiang, Y. Li, J. Guo, X. Liu, and B. Dai (2025)
Context as a tool: context management for long-horizon swe-agents.

arXiv preprint arXiv:2512.22087.

Cited by: Table 7.

Z. Lu, Y. Chai, Y. Guo, X. Yin, L. Liu, H. Wang, H. Xiao, S. Ren, G. Xiong, and H. Li (2025)
UI-r1: enhancing efficient action prediction of gui agents by reinforcement learning.

External Links: 2503.21620,
Link

Cited by: §6.

R. Luo, L. Wang, W. He, L. Chen, J. Li, and X. Xia (2025)
GUI-r1 : a generalist r1-style vision-language action model for gui agents.

External Links: 2504.10458,
Link

Cited by: §6.

Y. Lyu, X. Zhang, L. Yan, M. de Rijke, Z. Ren, and X. Chen (2025)
Deepshop: a benchmark for deep research shopping agents.

arXiv preprint arXiv:2506.02839.

Cited by: item 1,
§1,
§4.1,
§6.

M. A. Merrill, A. G. Shaw, N. Carlini, B. Li, H. Raj, I. Bercovich, L. Shi, J. Y. Shin, T. Walshe, E. K. Buchanan, J. Shen, G. Ye, H. Lin, J. Poulos, M. Wang, M. Nezhurina, J. Jitsev, D. Lu, O. M. Mastromichalakis, Z. Xu, Z. Chen, Y. Liu, R. Zhang, L. L. Chen, A. Kashyap, J. Uslu, J. Li, J. Wu, M. Yan, S. Bian, V. Sharma, K. Sun, S. Dillmann, A. Anand, A. Lanpouthakoun, B. Koopah, C. Hu, E. Guha, G. H. S. Dreiman, J. Zhu, K. Krauth, L. Zhong, N. Muennighoff, R. Amanfu, S. Tan, S. Pimpalgaonkar, T. Aggarwal, X. Lin, X. Lan, X. Zhao, Y. Liang, Y. Wang, Z. Wang, C. Zhou, D. Heineman, H. Liu, H. Trivedi, J. Yang, J. Lin, M. Shetty, M. Yang, N. Omi, N. Raoof, S. Li, T. Y. Zhuo, W. Lin, Y. Dai, Y. Wang, W. Chai, S. Zhou, D. Wahdany, Z. She, J. Hu, Z. Dong, Y. Zhu, S. Cui, A. Saiyed, A. Kolbeinsson, J. Hu, C. M. Rytting, R. Marten, Y. Wang, A. Dimakis, A. Konwinski, and L. Schmidt (2026)
Terminal-bench: benchmarking agents on hard, realistic tasks in command line interfaces.

External Links: 2601.11868,
Link

Cited by: §2.3,
§3.1,
§3.5.

MiniMax (2026)
MiniMax M2.5: built for real-world productivity.

Note: https://www.minimax.io/news/minimax-m25HuggingFace: https://huggingface.co/MiniMaxAI/MiniMax-M2.5

Cited by: §3.2,
§5.2.

Modal Labs (2024)
Modal: high-performance AI infrastructure.

Note: https://modal.com

Cited by: §1,
§2.2,
§6.

Moonshot AI (2026)
Kimi Code CLI

Note: AI agent command-line tool for software development and terminal operations. Accessed 2026-05-06

External Links: Link

Cited by: §3.5.

L. Ning, Z. Liang, Z. Jiang, H. Qu, Y. Ding, W. Fan, X. Wei, S. Lin, H. Liu, P. S. Yu, and Q. Li (2025)
A survey of WebAgents: towards next-generation AI agents for web automation with large foundation models.

In Proceedings of the 31st ACM SIGKDD Conference on Knowledge Discovery and Data Mining V.2,

pp. 6140–6150.

External Links: Document

Cited by: §1.

NVIDIA (2025)
Nemotron 3 Nano: open, efficient mixture-of-experts hybrid Mamba-Transformer model for Agentic reasoning.

Note: Technical report

External Links: Link

Cited by: §5.4.

OpenAI (2024)
Introducing SWE-bench Verified.

Note: https://openai.com/index/introducing-swe-bench-verified/Human-validated subset of 500 instances from SWE-bench, released August 13, 2024

Cited by: §3.1,
§6.

OpenClaw Team (2026)
OpenClaw

External Links: Link

Cited by: §5.1,
§6.

OpenClaw (2026)
ClawHub: skill directory for openclaw

External Links: Link

Cited by: §1.

Y. Qin, Y. Ye, J. Fang, H. Wang, S. Liang, S. Tian, J. Zhang, J. Li, Y. Li, S. Huang, W. Zhong, K. Li, J. Yang, Y. Miao, W. Lin, L. Liu, X. Jiang, Q. Ma, J. Li, X. Xiao, K. Cai, C. Li, Y. Zheng, C. Jin, C. Li, X. Zhou, M. Wang, H. Chen, Z. Li, H. Yang, H. Liu, F. Lin, T. Peng, X. Liu, and G. Shi (2025)
UI-tars: pioneering automated gui interaction with native agents.

External Links: 2501.12326,
Link

Cited by: §6.

Qwen Team (2025)
Qwen3 technical report.

External Links: 2505.09388,
Link

Cited by: §3.3.1,
§5.3.

Qwen Team (2026)
Qwen3.5: towards native multimodal agents.

Note: https://qwen.ai/blog?id=qwen3.5Open-weights release of Qwen3.5-397B-A17B; HuggingFace: https://huggingface.co/Qwen/Qwen3.5-397B-A17B

Cited by: §3.2.

N. Shang, Y. Liu, Y. Zhu, L. L. Zhang, W. Xu, X. Guan, B. Zhang, B. Dong, X. Zhou, B. Zhang, Y. Xin, Z. Miao, S. Li, F. Yang, and M. Yang (2025)
rStar2-Agent: agentic reasoning technical report.

arXiv preprint arXiv:2508.20722.

External Links: Link

Cited by: §3.3.3.

Z. Shao, P. Wang, Q. Zhu, R. Xu, J. Song, X. Bi, H. Zhang, M. Zhang, Y. K. Li, Y. Wu, and D. Guo (2024)
DeepSeekMath: pushing the limits of mathematical reasoning in open language models.

External Links: 2402.03300,
Link

Cited by: §3.3.3,
§4.4,
§5.3.

H. Song, L. Huang, S. Sun, J. Jiang, R. Le, D. Cheng, G. Chen, Y. Hu, Z. Chen, W. X. Zhao, Y. Song, T. Zhang, and J. Wen (2026)
SWE-Master: unleashing the potential of software engineering agents via post-training.

External Links: 2602.03411,
Document

Cited by: Table 7,
Table 7.

A. Sonwane, I. White, H. Lee, M. Pereira, L. Caccia, M. Kim, Z. Shi, C. Singh, A. Sordoni, M. Côté, and X. Yuan (2025)
Bugpilot: complex bug generation for efficient learning of swe skills.

arXiv preprint arXiv:2510.19898.

Cited by: Table 7,
§6.

C. Tao, J. Chen, Y. Jiang, K. Kou, S. Wang, R. Wang, X. Li, S. Yang, Y. Du, J. Dai, Z. Mao, X. Wang, L. Shang, and H. Bai (2026)
SWE-Lego: pushing the limits of supervised fine-tuning for software issue resolving.

External Links: 2601.01426,
Document

Cited by: Table 7.

G. Team, A. Zeng, X. Lv, Q. Zheng, Z. Hou, B. Chen, C. Xie, C. Wang, D. Yin, H. Zeng, J. Zhang, K. Wang, L. Zhong, M. Liu, R. Lu, S. Cao, X. Zhang, X. Huang, Y. Wei, Y. Cheng, Y. An, Y. Niu, Y. Wen, Y. Bai, Z. Du, Z. Wang, Z. Zhu, B. Zhang, B. Wen, B. Wu, B. Xu, C. Huang, C. Zhao, C. Cai, C. Yu, C. Li, C. Ge, C. Huang, C. Zhang, C. Xu, C. Zhu, C. Li, C. Yin, D. Lin, D. Yang, D. Jiang, D. Ai, E. Zhu, F. Wang, G. Pan, G. Wang, H. Sun, H. Li, H. Li, H. Hu, H. Zhang, H. Peng, H. Tai, H. Zhang, H. Wang, H. Yang, H. Liu, H. Zhao, H. Liu, H. Yan, H. Liu, H. Chen, J. Li, J. Zhao, J. Ren, J. Jiao, J. Zhao, J. Yan, J. Wang, J. Gui, J. Zhao, J. Liu, J. Li, J. Li, J. Lu, J. Wang, J. Yuan, J. Li, J. Du, J. Du, J. Liu, J. Zhi, J. Gao, K. Wang, L. Yang, L. Xu, L. Fan, L. Wu, L. Ding, L. Wang, M. Zhang, M. Li, M. Xu, M. Zhao, M. Zhai, P. Du, Q. Dong, S. Lei, S. Tu, S. Yang, S. Lu, S. Li, S. Li, Shuang-Li, S. Yang, S. Yi, T. Yu, W. Tian, W. Wang, W. Yu, W. L. Tam, W. Liang, W. Liu, X. Wang, X. Jia, X. Gu, X. Ling, X. Wang, X. Fan, X. Pan, X. Zhang, X. Zhang, X. Fu, X. Zhang, Y. Xu, Y. Wu, Y. Lu, Y. Wang, Y. Zhou, Y. Pan, Y. Zhang, Y. Wang, Y. Li, Y. Su, Y. Geng, Y. Zhu, Y. Yang, Y. Li, Y. Wu, Y. Li, Y. Liu, Y. Wang, Y. Li, Y. Zhang, Z. Liu, Z. Yang, Z. Zhou, Z. Qiao, Z. Feng, Z. Liu, Z. Zhang, Z. Wang, Z. Yao, Z. Wang, Z. Liu, Z. Chai, Z. Li, Z. Zhao, W. Chen, J. Zhai, B. Xu, M. Huang, H. Wang, J. Li, Y. Dong, and J. Tang (2025)
GLM-4.5: agentic, reasoning, and coding (arc) foundation models.

External Links: 2508.06471,
Link

Cited by: Table 7.

B. Trabucco, G. Sigurdsson, R. Piramuthu, and R. Salakhutdinov (2025)
InSTA: towards internet-scale training for agents.

External Links: 2502.06776

Cited by: item 1,
§4.3.

J. Wang, D. Zan, S. Xin, S. Liu, Y. Wu, and K. Shen (2025a)
Swe-mirror: scaling issue-resolving datasets by mirroring issues across repositories.

arXiv preprint arXiv:2509.08724.

Cited by: Table 7.

W. Wang et al. (2026)
Let it flow: agentic crafting on rock and roll, building the ROME model within an open agentic learning ecosystem.

Note: Introduces the ROCK (Reinforcement Open Construction Kit) sandbox environment manager as part of the ALE ecosystem; GitHub: https://github.com/alibaba/ROCK

External Links: 2512.24873,
Link

Cited by: §1,
§2.2.

X. Wang, B. Li, Y. Song, F. F. Xu, X. Tang, M. Zhuge, J. Pan, Y. Song, B. Li, J. Singh, H. H. Tran, F. Li, R. Ma, M. Zheng, B. Qian, Y. Shao, N. Muennighoff, Y. Zhang, B. Hui, J. Lin, R. Brennan, H. Peng, H. Ji, and G. Neubig (2025b)
OpenHands: an open platform for AI software developers as generalist agents.

In The Thirteenth International Conference on Learning Representations (ICLR),

External Links: Link,
2407.16741

Cited by: §1,
§3.1,
§3.2,
Table 7,
§6.

P. Xia, J. Chen, X. Yang, H. Tu, J. Liu, K. Xiong, S. Han, S. Qiu, H. Ji, Y. Zhou, Z. Zheng, C. Xie, and H. Yao (2026)
MetaClaw: just talk – an agent that meta-learns and evolves in the wild.

External Links: 2603.17187,
Link

Cited by: §6.

C. Xie, B. Li, C. Gao, H. Du, W. Lam, D. Zou, and K. Chen (2025)
SWE-Fixer: training open-source LLMs for effective and efficient GitHub issue resolution.

In Findings of the Association for Computational Linguistics: ACL 2025,

Vienna, Austria,  pp. 1123–1139.

External Links: Document,
Link

Cited by: Table 7.

T. Xie, D. Zhang, J. Chen, X. Li, S. Zhao, R. Cao, T. J. Hua, Z. Cheng, D. Shin, F. Lei, Y. Liu, Y. Xu, S. Zhou, S. Savarese, C. Xiong, V. Zhong, and T. Yu (2024)
OSWorld: benchmarking multimodal agents for open-ended tasks in real computer environments.

In Advances in Neural Information Processing Systems,

Vol. 37,  pp. 52040–52094.

Cited by: §1,
§6.

T. Xiong, Y. Ge, M. Li, Z. Zhang, P. Kulkarni, K. Wang, Q. He, Z. Zhu, C. Liu, R. Chen, T. Zheng, Y. Chen, X. Wang, R. Zhang, W. Chen, and H. Huang (2026)
Multi-crit: benchmarking multimodal judges on pluralistic criteria-following.

External Links: 2511.21662,
Link

Cited by: §5.1.

Y. E. Xu, Y. Savani, F. Fang, and Z. Kolter (2025)
Not all rollouts are useful: down-sampling rollouts in llm reinforcement learning.

arXiv preprint arXiv:2504.13818.

External Links: Link

Cited by: §3.3.3.

J. Yang, C. E. Jimenez, A. Wettig, K. Lieret, S. Yao, K. R. Narasimhan, and O. Press (2024)
SWE-agent: agent-computer interfaces enable automated software engineering.

In The Thirty-eighth Annual Conference on Neural Information Processing Systems,

External Links: Link

Cited by: §1,
§1,
§3.1,
§3.2,
§6.

J. Yang, K. Lieret, C. E. Jimenez, A. Wettig, K. Khandpur, Y. Zhang, B. Hui, O. Press, L. Schmidt, and D. Yang (2025a)
SWE-smith: scaling data for software engineering agents.

External Links: 2504.21798,
Link

Cited by: §3.1,
§3.5,
Table 7,
§6.

Z. Yang, S. Wang, K. Fu, W. He, W. Xiong, Y. Liu, Y. Miao, B. Gao, Y. Wang, Y. Ma, Y. Li, Y. Liu, Z. Hu, K. Zhang, S. Wang, H. Chen, F. Sung, Y. Liu, Y. Gao, Z. Yang, and T. Liu (2025b)
Kimi-Dev: agentless training as skill prior for SWE-agents.

External Links: 2509.23045,
Document

Cited by: Table 7.

S. Yao, J. Zhao, D. Yu, N. Du, I. Shafran, K. Narasimhan, and Y. Cao (2023)
ReAct: synergizing reasoning and acting in language models.

In International Conference on Learning Representations (ICLR),

Cited by: §1,
§3.1,
§4.2.

B. Ye, R. Li, Q. Yang, Y. Liu, L. Yao, H. Lv, Z. Xie, C. An, L. Li, L. Kong, Q. Liu, Z. Sui, and T. Yang (2026)
Claw-eval: toward trustworthy evaluation of autonomous agents.

External Links: 2604.06132,
Link

Cited by: §1,
§5.1,
§5.4,
Table 14,
§5,
§6.

Q. Yu, Z. Zhang, R. Zhu, Y. Yuan, X. Zuo, Y. Yue, W. Dai, T. Fan, G. Liu, L. Liu, X. Liu, H. Lin, Z. Lin, B. Ma, G. Sheng, Y. Tong, C. Zhang, M. Zhang, W. Zhang, H. Zhu, J. Zhu, J. Chen, J. Chen, C. Wang, H. Yu, Y. Song, X. Wei, H. Zhou, J. Liu, W. Ma, Y. Zhang, L. Yan, M. Qiao, Y. Wu, and M. Wang (2025)
DAPO: an open-source LLM reinforcement learning system at scale.

arXiv preprint arXiv:2503.14476.

Cited by: §3.3.3,
§4.4.

J. Zeng, D. Fu, T. Mi, Y. Zhuang, Y. Huang, X. Li, L. Ye, M. Xie, Q. Hua, Z. Huang, M. Jiang, H. Wang, J. Lin, Y. Xiao, J. Sun, Y. Wu, and P. Liu (2026)
daVinci-Dev: agent-native mid-training for software engineering.

External Links: 2601.18418,
Document

Cited by: Table 7,
Table 7.

L. Zeng, Y. Li, Y. Xiao, C. Li, C. Y. Liu, R. Yan, T. Wei, J. He, X. Song, Y. Liu, and Y. Zhou (2025)
Skywork-SWE: unveiling data scaling laws for software engineering in LLMs.

External Links: 2506.19290,
Document

Cited by: Table 7.

ZeroClaw Labs (2026)
ZeroClaw

External Links: Link

Cited by: §1,
§5.1.

C. Zhang, S. He, J. Qian, B. Li, L. Li, S. Qin, Y. Kang, M. Ma, G. Liu, Q. Lin, S. Rajmohan, D. Zhang, and Q. Zhang (2025)
Large language model-brained GUI agents: a survey.

Transactions on Machine Learning Research.

External Links: ISSN 2835-8856,
Link

Cited by: §1.

H. Zhang, M. Liu, S. Zhang, S. Han, J. Hu, Z. Jin, Y. Zhang, S. Diao, X. Lu, B. Xu, Z. Yu, J. Kautz, and Y. Dong (2026a)
ProRL Agent: rollout-as-a-service for RL training of multi-turn LLM agents.

External Links: 2603.18815,
Link

Cited by: §1,
§2.2,
§6.

L. Zhang, M. Chen, R. Cao, J. Chen, F. Zhou, Y. Xu, J. Yang, L. Chen, C. Luo, K. Zhang, F. Yan, K. Shum, J. Zhang, Z. Cui, H. Feng, J. Lin, B. Hui, and M. Yang (2026b)
MegaFlow: large-scale distributed orchestration system for the agentic era.

External Links: 2601.07526,
Link

Cited by: §1,
§2.2,
Table 2,
§6.

Z. Zhang, Z. Han, C. Mavromatis, Q. Zhu, Y. Zhang, S. Guan, D. Wang, X. Zhou, S. Wang, S. Adeshina, V. Ioannidis, and H. Rangwala (2026c)
Train less, learn more: adaptive efficient rollout optimization for group-based reinforcement learning.

arXiv preprint arXiv:2602.14338.

External Links: Link

Cited by: §3.3.3.

J. Zhao, G. Chen, F. Meng, M. Li, J. Chen, H. Xu, Y. Sun, W. X. Zhao, R. Song, Y. Zhang, P. Wang, C. Chen, J. Wen, and K. Jia (2026)
Immersion in the GitHub universe: scaling coding agents to mastery.

arXiv preprint arXiv:2602.09892.

Cited by: §1,
§3.2,
§3.5,
Table 7,
§6.

C. Zheng, S. Liu, M. Li, X. Chen, B. Yu, C. Gao, K. Dang, Y. Liu, R. Men, A. Yang, J. Zhou, and J. Lin (2025a)
Group sequence policy optimization.

arXiv preprint arXiv:2507.18071.

Cited by: §3.3.3.

H. Zheng, Y. Zhou, B. R. Bartoldson, B. Kailkhura, F. Lai, J. Zhao, and B. Chen (2025b)
Act only when it pays: efficient reinforcement learning for llm reasoning via selective rollouts.

arXiv preprint arXiv:2506.02177.

External Links: Link

Cited by: §3.3.3.

L. Zheng, W. Chiang, Y. Sheng, S. Zhuang, Z. Wu, Y. Zhuang, Z. Lin, Z. Li, D. Li, E. P. Xing, H. Zhang, J. E. Gonzalez, and I. Stoica (2023)
Judging llm-as-a-judge with mt-bench and chatbot arena.

External Links: 2306.05685,
Link

Cited by: §5.1.

S. Zhou, F. F. Xu, H. Zhu, X. Zhou, R. Lo, A. Sridhar, X. Cheng, T. Ou, Y. Bisk, D. Fried, U. Alon, and G. Neubig (2024)
WebArena: a realistic web environment for building autonomous agents.

In The Twelfth International Conference on Learning Representations,

External Links: Link

Cited by: §1.

Y. Zhou, Q. Yang, K. Lin, M. Bai, X. Zhou, Y. Wang, S. Levione, and E. Li (2025)
Proposer-agent-evaluator (PAE): autonomous skill discovery for foundation model internet agents.

In ICML,

External Links: Link

Cited by: item 1,
§4.3.

Z. Zhu, C. Xie, X. Lv, and slime Contributors (2025)
Slime: an llm post-training framework for rl scaling.

Note: https://github.com/THUDM/slimeGitHub repository. Corresponding author: Xin Lv

Cited by: §3.3.1,
§3.3.2.

Appendix A Orchard Env Design Details

The design of Orchard Env is guided by a central principle: the environment layer should be thin enough to be reusable across training recipes, agent harnesses, and model backends, while providing the isolation and lifecycle management needed for large-scale agentic training.
We highlight five design choices that realize this principle and, together, satisfy requirements R1–R3 stated in the section opening.
Two are distinguishing technical choices that set Orchard Env apart from existing environment services: agent injection addresses image heterogeneity at near-zero adaptation cost (R2), and direct Pod-IP communication keeps the service thin and removes the Kubernetes control plane from the hot path (R1).
The remaining three—network isolation, asynchronous lifecycle with heartbeat-based cleanup, and watch-based readiness—are operational properties that make the service production-grade at the concurrency and reliability levels required by large-scale agentic training; together with the Kubernetes-native deployment they support R3 (quantified in §2.2).
The five paragraphs below present the distinguishing choices first, then the operational properties.

Agent Injection via Init Containers.

A central challenge in building environment services for agentic training is image heterogeneity: different tasks require different base images (e.g., specific Python versions, system libraries, or language toolchains), and modifying each image to include an execution agent is impractical at scale.
Orchard Env addresses this through a Kubernetes init container that copies a self-contained Python runtime and agent server into a shared emptyDir volume before the main container starts.
The main container then launches the agent from the shared volume via /opt/sandbox-agent/start.sh.
This design avoids baking Python or the agent into each task image; in practice, Orchard Env targets Linux container images and, by default, launches the injected agent through sh -c.

Direct Pod-IP Communication.

After a sandbox is provisioned, all execution and file operation requests are routed directly to the pod IP, bypassing the Kubernetes API server entirely.
This avoids the control-plane mediation and WebSocket setup overhead of the Kubernetes exec API.
Direct communication reduces per-command round-trip overhead and removes the API server as a throughput bottleneck under high concurrency.

Network Isolation.

Orchard Env enforces network isolation through Kubernetes NetworkPolicy resources.
A namespace-wide default-deny egress policy prevents sandbox containers from initiating outbound connections.
When a sandbox requires network access (e.g., for package installation), the orchestrator creates a per-sandbox NetworkPolicy that selectively allows egress, which is cleaned up with the sandbox.
This provides defense-in-depth: even if a user-supplied command attempts to exfiltrate data, it is blocked at the network layer.

Asynchronous Lifecycle with Heartbeat-Based Cleanup.

Sandbox creation is asynchronous: the API returns immediately after pod creation, and clients poll or block on a /wait endpoint until readiness.
This decouples API responsiveness from Kubernetes scheduling latency.
Long-running sandboxes can be kept alive by periodic heartbeat messages from the client SDK.
A background cleanup loop in the orchestrator detects sandboxes whose heartbeat has expired and deletes them, preventing resource leakage from crashed or abandoned clients.

Watch-Based Readiness.

Rather than polling the Kubernetes API for pod status, the orchestrator maintains a persistent LIST+WATCH stream that tracks all sandbox pod state transitions in real time.
State changes are cached in memory and waiters are notified via asyncio.Event, avoiding repeated polling of the Kubernetes API.

Appendix B Cost Analysis Details

This appendix provides the full methodology and discussion for the cost comparison in Table 2.

Scenario.

We estimate the monthly cost of running 128 parallel sandbox environments for 240 hours, with each sandbox configured at 2 vCPUs and 8 GiB RAM—matching a typical SWE-bench task environment.

Orchard setup.

Orchard is deployed on 17 Azure Standard_D16ads_v5 instances (16 vCPU, 64 GiB RAM each): 16 nodes host 8 sandboxes each (128 total), and 1 node runs the orchestrator.
Sandbox nodes use spot instances (preemptible VMs at ∼{\sim}80% discount, ∼$​0.165{\sim}\mathdollar 0.165/hr vs. $​0.824\mathdollar 0.824/hr on-demand), which are well-suited for ephemeral sandbox workloads that can tolerate occasional preemption.
The orchestrator node uses standard pay-as-you-go pricing for stability.

Managed service pricing.

For managed services, we use their official per-second or per-hour pricing for a 2-vCPU, 8-GiB sandbox, including both compute and memory charges:

•

E2B: vCPU charge of $0.000014/vCPU/s ×\times 2 = $0.000028/s, plus RAM charge of $0.0000045/GiB/s ×\times 8 = $0.000036/s.
Total: $0.000064/s = $0.2304/hr per sandbox.

•

Daytona: vCPU charge of $0.0504/vCPU/hr ×\times 2 = $0.1008/hr, plus RAM charge of $0.0162/GiB/hr ×\times 8 = $0.1296/hr.
Total: $0.2304/hr per sandbox.

•

Modal: CPU charge of $0.00003942/physical-core/s ×\times 1 core (= 2 vCPU) = $0.00003942/s, plus RAM charge of $0.00000672/GiB/s ×\times 8 = $0.00005376/s.
Total: $0.00009318/s = $0.3354/hr per sandbox.
Modal Sandbox pricing is non-preemptible by default.

•

MegaFlow: Estimated based on Alibaba Cloud ecs.c8a.2xlarge instances (8 vCPU, 16 GiB, ∼$​0.15{\sim}\mathdollar 0.15/hr), one task per instance as described in the original paper. The per-sandbox resource allocation exceeds our 2-vCPU target.

Key observations.

•

Spot-instance economics.
Orchard’s self-hosted design enables the use of cloud spot instances—preemptible VMs offered at steep discounts (∼$​0.165{\sim}\mathdollar 0.165/hr vs. $​0.824\mathdollar 0.824/hr on-demand for D16ads_v5) in exchange for the possibility of short-notice eviction.
Because sandbox containers are ephemeral and can be recreated on eviction, spot pricing is a natural fit for the sandbox node pool.
Managed services cannot pass through spot pricing because they control the underlying infrastructure.

•

VM-level multiplexing.
Because Orchard packs multiple sandboxes onto each VM (8 per D16ads_v5 node), the per-sandbox cost benefits from shared overhead.
MegaFlow’s one-task-per-instance model, by contrast, allocates a full VM to each sandbox, leading to higher per-sandbox cost even with comparable cloud pricing.

•

On-demand comparison.
Even without spot instances, Orchard’s on-demand cost ($3,362) is roughly half the cost of E2B and Daytona ($7,078 each) and one-third of Modal ($10,305).
The key advantage is that Orchard gives researchers full control over the cluster—they can tune node pools, autoscaling, network policies, and resource limits without depending on a vendor’s control plane.

These cost differences compound over the course of a research project.
Generating 160K rollout trajectories, running ablation studies, and iterating on training recipes can easily require thousands of hours of environment interaction.
Orchard’s self-hosted, spot-friendly design makes such workloads practical for academic research budgets.

Appendix C Orchard-GUI Tool List

This appendix reproduces the full OpenAI tool-call JSON Schema for all 13 atomic tools used by Orchard-GUI.
At each step the agent emits one or multiples tool calls with each inside a <tool_call>…</tool_call> blockḞor readability, the schemas are grouped by family, one styled box per family.

Pointer Actions: click / hover / drag

Keyboard Actions: write / press_keys

Page-Navigation Actions: scroll / goto_url / go_back / wait

Tab-Management Actions: new_tab / switch_tab / close_tab

Termination Action: done

Appendix D Example GUI Agent Trajectory

This appendix shows a representative multi-turn rollout produced by Orchard-GUI on a WebVoyager-style task.
The first box gives the prompt fed to the model at the final step (system prompt ++ accumulated trajectory through prior steps).
The second box shows the model’s response at that step (its final reasoning followed by the terminating done call).
The full trajectory of seven think →\rightarrow tool_call →\rightarrow tool_response cycles is shown verbatim. Only the JSON tool schema inside the system prompt is abbreviated with […] (the full schema appears in Appendix C).

Inputs to LLM – full trajectory (last turn, 1 context image)

Example response from LLM at the last turn

Appendix E Orchard-GUI Task Filtering Pipeline

We draw task instances from the task set organized by WebGym (Bai et al., 2026b), which consists of 292,092 raw task instances in total. To produce a clean, evaluation-safe, and diverse pool of training prompts, we apply a five-stage filtering pipeline:

1.

Remove common evaluation benchmarks. We strip out splits that overlap with our held-out benchmarks (e.g., Online-Mind2Web (Deng et al., 2023) and DeepShop (Lyu et al., 2025)) to prevent train/test contamination, retaining only the two complementary PAE-WebVoyager (Zhou et al., 2025) and InSTA-v3 (Trabucco et al., 2025) splits (-13,840, 4.7%4.7\% →\rightarrow 278,252). The former consists of automatically proposed web-navigation tasks generated by a context-aware task proposer, grounded in the websites covered by the WebVoyager (He et al., 2024) benchmark, while the later contains tasks automatically synthesized by an LLM over a large and diverse set of websites. Each task is grounded in a specific domain and phrased as a realistic user goal (e.g., finding information, retrieving attributes, or completing simple workflows), with an emphasis on feasibility and safety.

2.

Keep parent tasks only. WebGym additionally provides child tasks decomposed from each parent intent. Since child tasks share substantial structure with their parents, we retain only the parents to avoid intra-family redundancy (-23,437, 8.4%8.4\% →\rightarrow 254,815).

3.

Exclude WebVoyager tasks. We further drop any task whose intent appears in the original WebVoyager benchmark, eliminating residual contamination at the prompt level (-411, 0.2%0.2\% →\rightarrow 254,404).

4.

Restrict to popular websites. Long-tail websites are noisier (more captchas, anti-bot blocks, broken pages) and less representative of realistic browsing. We keep only tasks whose target site falls within the SimilarWeb Top-100 list and the MOZ Top 500 Most Popular Websites, and where the same site has at least two tasks, ensuring sufficient per-site coverage for the downstream agent (-114,349, 44.9%44.9\% →\rightarrow 140,055).

5.

Semantic deduplication. The remaining pool is dominated by near-duplicate intents (e.g., paraphrases of the same shopping or search query across thousands of products). We embed each task intent with Qwen/Qwen3-Embedding-8B and greedily remove tasks whose cosine similarity to a previously kept task exceeds 0.990.99 (-124,454, 88.9%88.9\% →\rightarrow 15,601).

The final filtered pool of 15,601 unique task intents serves as the seed set from which we sample teacher trajectories for SFT and RL prompts.

`