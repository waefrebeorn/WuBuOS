## On this page

[![Language AI Handbook Cover](https://assets.mbrenndoerfer.com/language-ai-handbook/language-ai-handbook-cover.jpg)\\
\\
Part of\\
\\
Language AI Handbook](https://mbrenndoerfer.com/books/language-ai-handbook)

Reading mode

Tooltip detail

BeginnerIntermediateExpertNone

Article linksOn

Back

# Continuous Batching: Optimizing LLM Inference Throughput

Michael Brenndoerfer·Published: January 18, 2026January 18, 2026·59 min read

[Data, Analytics & AI](https://mbrenndoerfer.com/writing/categories/data-analytics-ai) [Machine Learning](https://mbrenndoerfer.com/writing/categories/machine-learning) [Language AI Handbook](https://mbrenndoerfer.com/writing/categories/language-ai-handbook)

Discover how continuous batching achieves 2-3x throughput gains in LLM inference through iteration-level scheduling, eliminating static batch inefficiencies.

Track your reading progress

Sign in to mark chapters as read and track your learning journey

Sign in →

Reading Level

Choose your expertise level to adjust how many terms are explained. Beginners see more tooltips, experts see fewer to maintain reading flow. Hover overunderlined termsfor instant definitions.

Beginner· Maximum helpIntermediate· Medium helpExpert· Minimal helpHide All· No tooltips

Article links

Make inline references clickable

On

## [Link to Continuous Batching](https://mbrenndoerfer.com/writing/continuous-batching\#continuous-batching) Continuous BatchingLink Copied

Large language model inference presents a unique challenge that sets it apart from nearly every other machine learning workload: requests arrive at different times, require different numbers of output tokens, and finish at unpredictable moments. Traditional batch processing, borrowed from training workflows where uniformity is the norm, handles this reality poorly. A batch must wait for its slowest member, leaving GPU resources idle while fast requests sit completed but unable to exit. The result is a system that wastes expensive compute cycles and creates frustrating delays when users expect snappy responses.

Think of static batching like a restaurant where a table of eight cannot leave until every single person has finished their meal. The first three diners who finish quickly just sit there, holding up the table, while the rest of the group takes their time. The restaurant cannot seat the next group until everyone leaves together, even though three slots have been effectively empty for minutes. This is exactly what happens to GPU compute slots under static batching: finished requests occupy space they no longer need, blocking new arrivals from starting.

**Continuous batching** solves this basic mismatch by rethinking how we group work together. Rather than treating a batch as a monolithic unit processed as a single group, continuous batching treats each decoding iteration as an opportunity to reshuffle which requests are actively using compute. Completed requests exit immediately, and waiting requests join without delay. The batch becomes a living, breathing set of concurrent streams rather than a frozen snapshot of work collected at a single moment in time.

The key insight is that [autoregressive generation](https://mbrenndoerfer.com/writing/autoregressive-generation-gpt-text-generation) offers a natural scheduling boundary after every single token. Between any two decoding iterations, the model has completed one forward pass and is about to begin the next. At this boundary, we have complete freedom to change the composition of the batch, adding new requests and removing finished ones. Static batching ignores this flexibility entirely. Continuous batching exploits it fully.

This chapter explores why static batching creates inefficiency, how continuous batching addresses it through **iteration-level scheduling**, and the mechanisms for handling requestcompletion and memory management in a production setting. The throughput gains from this approach are substantial, often doubling or tripling the requests per second a single GPU can handle. Understanding continuous batching is needed for when you deploy language models in production, as it has become the standard technique in modern serving frameworks including [vLLM](https://mbrenndoerfer.com/writing/paged-attention-vllm-kv-cache-memory-management), TensorRT-LLM, and Text Generation Inference.

Historical Context

Static batching was the dominant approach for LLM inference through 2022, borrowed directly from deep learning training where all examples in a batch process identically. The first serious analysis of continuous batching for LLMs appeared in the Orca paper (Yu et al., 2022), which demonstrated that iteration-level scheduling could reach up to 36.9x higher throughput compared to existing systems. This result was surprising because the technique requires no changes to the model itself, only to the serving infrastructure. Orca's insights were rapidly adopted by the open-source community, with vLLM (Kwon et al., 2023) combining continuous batching with paged attention to create the serving paradigm now used across the industry.

Advertisement

## [Link to The Batching Problem in LLM Inference](https://mbrenndoerfer.com/writing/continuous-batching\#the-batching-problem-in-llm-inference) The Batching Problem in LLM InferenceLink Copied

Batching multiple requests together is needed for efficient GPU utilization. Modern GPUs contain thousands of processing cores designed to execute operations in parallel, and feeding them work one request at a time leaves most of this capability unused. As we discussed in the [KV Cache](https://mbrenndoerfer.com/writing/autoregressive-generation-gpt-text-generation) chapter, autoregressive generation produces one token per forward pass. Each forward pass involves substantial overhead: loading model weights from memory, executing attention computations, and applying the feed-forward network. Without batching, each forward pass processes a single request, leaving most of the GPU's parallel processing capability unused. Matrix multiplications that could operate on batch sizes of 32 or 64 instead operate on a single row, materially underutilizing the thousands of available cores.

The arithmetic here is stark. A modern A100 GPU can perform roughly 312 teraFLOPS of bfloat16 computation. A single-requestforward pass through a 7 [B parameter](https://mbrenndoerfer.com/writing/bm25-search-algorithm-elasticsearch-implementation) model performs on the order of 14 billion floating-point operations. Batching 32 requests together multiplies useful work by 32 while the overhead of loading model weights from memory stays roughly constant. The same weight loading that dominates a batch-size-1 forward pass becomes negligible when amortized across 32 concurrent requests. This memory-bandwidth arithmetic is why practical throughput depends so heavily on batching.

Training workflows batch naturally because all samples pass through the same computation graph with identical sequence lengths after padding. Every example in a training batch starts together, processes together, and finishes together. This uniformity makes scheduling trivial. Inference is different in three necessary ways that break this simple model:

- Variable input lengths: requests arrive with prompts of different sizes. You might submit a 10-token question while another user gives a 2000-token document for summarization.
- Variable output lengths: Some responses are 10 tokens, others are 1000. A simple factual answer completes quickly, while a detailed explanation continues for many iterations.
- Asynchronous arrivals: Requests come in continuously, not in neat batches. Users submit queries at unpredictable times, and the system cannot wait indefinitely to form perfect batches.

These differences create a basic tension between batching efficiency and response latency. We want large batches to maximize GPU utilization, but we also want to return responses quickly without waiting for other requests. Static batching resolves this tension badly: it picks one batch and forces everything to conform to it. Continuous batching resolves it well: it adapts the batch to the actual work, iteration by iteration.

Advertisement

## [Link to Static Batching](https://mbrenndoerfer.com/writing/continuous-batching\#static-batching) Static BatchingLink Copied

The simplest approach to batching inference requests is static batching, where we collect a fixed group of requests, process them together until all complete, and then start the next batch. This approach mirrors how training works and requires minimal changes to existing infrastructure.

Before looking at the mechanics, it helps to understand why engineers reached for static batching in the first place. Training code already handles fixed-size batches. Inference engines built on training frameworks naturally inherit this structure. The first LLM serving systems, deployed by research teams on hardware originally intended for training, simply reused the batch abstraction they knew. The problems with this choice only became apparent at scale when high- [variance](https://mbrenndoerfer.com/writing/descriptive-statistics-guide-python-data-analysis)request lengths exposed just how much computation was wasted waiting for laggards.

### [Link to How Static Batching Works](https://mbrenndoerfer.com/writing/continuous-batching\#how-static-batching-works) How Static Batching WorksLink Copied

In static batching, the system follows a straightforward process that prioritizes simplicity over flexibility. The core idea is to treat a batch as an indivisible unit that processes from start to finish without any changes to its composition. This makes implementation easy but creates significant inefficiencies when requests have different characteristics.

The static batching algorithm proceeds through five distinct phases:

1. **Accumulate requests.** Wait until a batch of size BBB requests arrives, or a timeout expires. During this phase, incoming requests queue up while the system decides whether enough work has accumulated to justify starting a new batch.
2. **[Pad](https://mbrenndoerfer.com/writing/special-tokens-transformers-cls-sep-pad-mask) to uniform length.** Pad all input sequences to match the longest input in the batch. Since matrix operations require uniform dimensions, shorter sequences receive padding tokens that consume compute but produce no useful output.
3. **Process together.** Run all requests through every decoding iteration. The entire batch moves in lockstep, with each request generating one token per forward pass.
4. **Wait for completion.** Continue until the longest output sequence finishes. Even if some requests complete after just a few tokens, they remain in the batch, consuming resources while creating only padding.
5. **Return results.** Send all responses back and start the next batch. Only after every request has finished can the system collect a new batch and begin again.

In\[2\]:

Code

```
from dataclasses import dataclass
from typing import List

@dataclass
class Request:
    """Simulated inference request."""

    id: int
    input_length: int
    output_length: int  # How many tokens this request will generate

def simulate_static_batch(requests: List[Request]) -> dict:
    """Simulate static batching behavior."""
    if not requests:
        return {
            "total_iterations": 0,
            "wasted_iterations": 0,
            "max_output": 0,
            "useful_iterations": 0,
            "efficiency": 0,
        }

    # All requests must wait for the longest output
    max_output = max(r.output_length for r in requests)

    # Calculate wasted computation
    total_iterations = len(requests) * max_output
    useful_iterations = sum(r.output_length for r in requests)
    wasted_iterations = total_iterations - useful_iterations

    return {
        "max_output": max_output,
        "total_iterations": total_iterations,
        "useful_iterations": useful_iterations,
        "wasted_iterations": wasted_iterations,
        "efficiency": useful_iterations / total_iterations,
    }
```

Let's see how static batching performs with requests of varying output lengths. This simulation shows the core problem: when requests differ materially in how many tokens they need to generate, the short requests end up waiting for the long ones.

In\[3\]:

Code

```
import numpy as np

# Create a batch with highly variable output lengths
np.random.seed(42)
static_batch = [\
    Request(id=i, input_length=50, output_length=np.random.randint(10, 200))\
    for i in range(8)\
]

# Show the output lengths
output_lengths = [r.output_length for r in static_batch]
```

Out\[4\]:

Console

```
Static Batch Request Output Lengths:
  Request 0: 112 tokens ██████████████████████
  Request 1: 189 tokens █████████████████████████████████████
  Request 2: 102 tokens ████████████████████
  Request 3:  24 tokens ████
  Request 4: 116 tokens ███████████████████████
  Request 5:  81 tokens ████████████████
  Request 6: 198 tokens ███████████████████████████████████████
  Request 7:  30 tokens ██████
```

In\[5\]:

Code

```
static_results = simulate_static_batch(static_batch)
```

Out\[6\]:

Console

```
Static Batching Analysis:
  Maximum output length: 198 tokens
  Total iterations computed: 1584
  Useful iterations: 852
  Wasted iterations: 732
  Compute efficiency: 53.8%
```

### [Link to The Efficiency Problem](https://mbrenndoerfer.com/writing/continuous-batching\#the-efficiency-problem) The Efficiency ProblemLink Copied

The analysis reveals the core problem with static batching: requests that finish early continue consuming compute resources. In our example, some requests only needed around 20 tokens while the batch waited for requests generating nearly 200 tokens. Those short requests sat idle for over 150 iterations, wasting computational resources that could have been serving other users.

To understand why this matters, consider what happens inside the GPU during those wasted iterations. The completed request still occupies a slot in every tensor operation. The [attention mechanism](https://mbrenndoerfer.com/writing/attention-mechanism-intuition-soft-lookup-weights-context-vectors) still computes over its position. The feed-forward network still processes its [hidden state](https://mbrenndoerfer.com/writing/rnn-architecture-recurrent-neural-networks-guide). The output is discarded as padding, but the compute is very real. This is not idle waiting; it is inefficient computation that burns energy and time without creating anything useful.

This inefficiency compounds across several dimensions:

- **Memory waste.** Completed requests retain their [KV cache](https://mbrenndoerfer.com/writing/autoregressive-generation-gpt-text-generation) entries until the batch finishes. As we covered in the [KV Cache Memory](https://mbrenndoerfer.com/writing/multi-query-attention-memory-efficient-inference) chapter, each request's cache can consume gigabytes of GPU memory. A 7 [B parameter](https://mbrenndoerfer.com/writing/bm25-search-algorithm-elasticsearch-implementation) model might need 1-2 GB per request at full context length. Keeping completed requests' caches prevents new requests from starting, creating a bottleneck even when compute capacity exists.
- **Latency inflation.** Short requests experience artificially high latency because they must wait for longer siblings in their batch. A 20-token response that takes 100ms to generate might not be returned for 1000ms while waiting for a 200-token sibling. From the user's perspective, this feels like unexplained slowness, even though the system technically completed the request quickly.
- **GPU underutilization.** As requests complete, the [effective batch size](https://mbrenndoerfer.com/writing/gradient-accumulation-memory-effective-batch-size-training) shrinks. A batch starting with 8 requests might have only 2 active requests for its final iterations, sharply reducing throughput. The GPU cores that could be serving new requests instead sit mostly idle, processing padding tokens for the few remaining active sequences.

Out\[7\]:

Visualization

![Horizontal bar chart showing active computation per request and gray wasted idle time. Short requests have large gray regions.](https://assets.mbrenndoerfer.com/notebooks/13_continuous_batching_files/static-batching-waste.png)

Static batching execution timeline showing active computation (colored) versus wasted waiting time (gray). Each row is one request in the batch, sorted by output length. Short requests finish early but must wait until the longest request completes before the batch ends, leaving large gray idle regions.

The gray regions in this visualization stand for wasted computation: the GPU performs forward passes that produce padding tokens, while the finished requests cannot release their resources. The shorter the original request, the larger the fraction of its total batch time spent waiting. In a workload with high [variance](https://mbrenndoerfer.com/writing/descriptive-statistics-guide-python-data-analysis) in output lengths, which is typical of real deployments, the average efficiency can easily drop to 20-30%.

Advertisement

## [Link to Continuous Batching](https://mbrenndoerfer.com/writing/continuous-batching\#continuous-batching) Continuous BatchingLink Copied

Continuous batching, also called iteration-level scheduling or in-flight batching, takes a fundamentally different approach to managing concurrent requests. Instead of treating a batch as a fixed unit that lives and dies together, it treats each decoding iteration as an independent scheduling decision. This seemingly simple change materially improves both throughput and latency.

Think of continuous batching like a highway toll plaza where each booth independently processes cars as they arrive, rather than waiting for a predetermined convoy to form before opening. Cars enter, pay their toll, and leave the moment they are done, regardless of when the car behind them arrived. The plaza is always running at full capacity because new cars enter as soon as a booth clears. No convoy waits for its slowest member; every car moves at its own natural speed.

The move from static to continuous batching requires rethinking the basic unit of work. In static batching, the unit is a batch of requests that live together. In continuous batching, the unit is a single decoding iteration applied to whatever set of requests currently occupies the available slots. The batch is not a fixed group of requests but a window of concurrent work that shifts continuously as requests enter and exit.

### [Link to The Core Insight](https://mbrenndoerfer.com/writing/continuous-batching\#the-core-insight) The Core InsightLink Copied

The key insight behind continuous batching emerges from a careful examination of how [autoregressive generation](https://mbrenndoerfer.com/writing/autoregressive-generation-gpt-text-generation) works. Each decoding step produces exactly one token per request. Between any two decoding iterations, the model completes one forward pass and begins the next. At this boundary, we have complete freedom to reorganize which requests participate in the next iteration. The model does not care whether the same requests continue; it only needs valid input tokens and their associated KV caches.

This observation reveals that the rigid batch structure of static batching is entirely artificial. There is no technical requirement that forces us to keep completed requests around. We imposed that constraint for implementation simplicity, not because the underlying computation demands it. The batch boundary is just a software construct, and we can move it wherever we want.

With this insight, continuous batching introduces three powerful capabilities:

- **Remove completed requests.** Immediately release slots when requests hit their end token. The moment a request generates its end-of-sequence marker, it exits the batch and its resources become available.
- **Add new requests.** Insert waiting requests into freed slots. New requests join the batch at whatever iteration happens to have space, rather than waiting for a complete batch turnover.
- **Maintain continuous flow.** Keep the batch saturated with active work. The goal is to maintain maximum occupancy at all times, treating the batch like a continuously flowing river rather than a series of discrete buckets.

This turns batch processing from a rigid structure into a dynamic queue where requests flow through the system at their natural pace. Short requests complete quickly and exit. Long requests continue as needed. New requests enter as soon as space permits. The GPU always has real work to do.

### [Link to How Continuous Batching Works](https://mbrenndoerfer.com/writing/continuous-batching\#how-continuous-batching-works) How Continuous Batching WorksLink Copied

The continuous batching algorithm operates in a perpetual loop that never truly stops between batches. Instead of collecting requests, processing them, and collecting again, it maintains a persistent set of active requests that evolves iteration by iteration. Each cycle through the loop is a single forward pass through the model.

The algorithm follows these steps in each iteration:

1. **Check for completions.** After each iteration, identify requests that generated an end-of-sequence token or reached their maximum length. These requests have finished their work and no longer need to participate.
2. **Evict completed requests.** Remove finished requests from the active batch, freeing their slots and [KV cache memory](https://mbrenndoerfer.com/writing/multi-query-attention-memory-efficient-inference). This happens immediately, not at some future batch boundary.
3. **Check waiting queue.** Look for pending requests that arrived while the batch was processing. New requests accumulate in a queue whenever all slots are occupied.
4. **Insert new requests.** Fill empty slots with waiting requests, running their **prefill phase** to build initial KV caches. These new requests join alongside existing **decode phase** requests.
5. **Execute iteration.** Run one decoding step for all active requests. Every active request generates exactly one token.
6. **Return completed results.** Send finished responses back to clients immediately. Users receive their results the moment generation completes, not when some arbitrary batch finishes.

The loop never pauses to collect a new batch. The transition from one set of requests to another occurs within the normal iteration boundary. From the GPU's perspective, every iteration looks nearly identical: a batch of tokens arrives, a batch of logits comes out. Only the composition of the batch changes over time.

In\[8\]:

Code

```
from collections import deque

@dataclass
class ContinuousRequest:
    """Request with tracking for continuous batching."""

    id: int
    input_length: int
    output_length: int  # Total tokens to generate
    tokens_generated: int = 0
    start_iteration: int = 0
    end_iteration: int = 0

    @property
    def is_complete(self) -> bool:
        return self.tokens_generated >= self.output_length

class ContinuousBatchSimulator:
    """Simulates continuous batching behavior."""

    def __init__(self, max_batch_size: int = 8):
        self.max_batch_size = max_batch_size
        self.active_batch: List[ContinuousRequest] = []
        self.waiting_queue: deque = deque()
        self.completed: List[ContinuousRequest] = []
        self.iteration = 0
        self.utilization_history = []

    def submit_request(self, request: ContinuousRequest):
        """Add a request to the waiting queue."""
        self.waiting_queue.append(request)

    def _fill_batch(self):
        """Fill empty slots with waiting requests."""
        while (
            len(self.active_batch) < self.max_batch_size and self.waiting_queue
        ):
            request = self.waiting_queue.popleft()
            request.start_iteration = self.iteration
            self.active_batch.append(request)

    def _process_completions(self):
        """Remove completed requests from the batch."""
        still_active = []
        for request in self.active_batch:
            if request.is_complete:
                request.end_iteration = self.iteration
                self.completed.append(request)
            else:
                still_active.append(request)
        self.active_batch = still_active

    def step(self):
        """Execute one iteration of the continuous batch."""
        # First, fill any empty slots
        self._fill_batch()

        if not self.active_batch:
            return False  # Nothing to process

        # Record utilization
        self.utilization_history.append(
            len(self.active_batch) / self.max_batch_size
        )

        # Generate one token for each active request
        for request in self.active_batch:
            request.tokens_generated += 1

        self.iteration += 1

        # Check for completions
        self._process_completions()

        return True

    def run_until_complete(self):
        """Run until all requests are processed."""
        while self.active_batch or self.waiting_queue:
            self.step()
```

Let's simulate the same requests with continuous batching to see the difference in practice:

In\[9\]:

Code

```
# Convert our static batch to continuous batch format, then add extra requests
# so the batch (size 8) is always full and slot reuse is visible in the timeline
np.random.seed(7)
extra_requests = [\
    Request(id=i + 8, input_length=50, output_length=np.random.randint(10, 200))\
    for i in range(8)\
]
all_requests = static_batch + extra_requests

continuous_requests = [\
    ContinuousRequest(\
        id=r.id, input_length=r.input_length, output_length=r.output_length\
    )\
    for r in all_requests\
]

# Run continuous batching simulation with 16 requests and batch_size=8
# so the second half must wait and slots are visibly reused as the first
# requests finish at different times
simulator = ContinuousBatchSimulator(max_batch_size=8)
for request in continuous_requests:
    simulator.submit_request(request)

simulator.run_until_complete()
```

Out\[10\]:

Console

```
Continuous Batching Results:
  Total iterations: 311
  Average utilization: 75.2%
  Theoretical minimum iterations: 234
```

Out\[11\]:

Visualization

![Gantt-style bar chart showing batch slots reused across iterations. Colored bars fill each slot continuously with no gaps.](https://assets.mbrenndoerfer.com/notebooks/13_continuous_batching_files/continuous-batching-timeline.png)

Continuous batching execution timeline showing how batch slots are reused across iterations. Each colored bar is a single request's active generation window. Slots are immediately reassigned as soon as a request finishes, keeping all slots occupied and eliminating the idle periods seen in static batching.

The high utilization and reduced iteration count show how continuous batching effectively saturates the GPU. By immediately filling slots vacated by completed requests, the system minimizes idle time and approaches the theoretical maximum throughput for the given workload. Notice that the total iterations is much closer to the theoretical minimum than what static batching achieved. The difference becomes even more large with realistic workloads that have higher [variance](https://mbrenndoerfer.com/writing/descriptive-statistics-guide-python-data-analysis) in output lengths.

Advertisement

Join the Community

Enjoying this article?

![Michael Brenndoerfer](https://assets.mbrenndoerfer.com/_optimized/general/resume/michael_brenndoerfer-128w.webp)

Michael Brenndoerfer

I write about AI, data science, machine learning, finance, economics and entrepreneurship. Subscribe to get updates delivered straight to your inbox.

- No popups
- Unobstructed reading
- Commenting

Subscribe

No spam, unsubscribe anytime.

Join Community

[Join Community](https://mbrenndoerfer.com/community)

![Michael Brenndoerfer](https://assets.mbrenndoerfer.com/_optimized/general/resume/michael_brenndoerfer-480w.webp)

Michael Brenndoerfer

Author and community host

## [Link to Iteration-Level Scheduling](https://mbrenndoerfer.com/writing/continuous-batching\#iteration-level-scheduling) Iteration-Level SchedulingLink Copied

The power of continuous batching comes from making scheduling decisions at every iteration. Rather than committing to a fixed plan when a batch starts, the system reevaluates its choices thousands of times per second. This fine-grained control lets advanced policies beyond simple FIFO ordering and allows the system to adapt dynamically to changing conditions.

Iteration-level scheduling is a fundamentally different mental model from batch-level scheduling. In batch-level thinking, the scheduler plans who gets to run for the next several hundred milliseconds. In iteration-level thinking, the scheduler decides who runs for the next 10 to 50 milliseconds, then immediately reconsiders. This rapid reconsideration is what lets the system to react quickly to completions, memory pressure, and priority changes.

Think of iteration-level scheduling like air traffic control at a busy airport. Rather than assigning each plane to a runway at the start of the day and having it sit there regardless of what happens, air traffic controllers continuously decide which plane goes next. They adapt to delays and changing weather, including emergencies. A plane that taxied out for a 30-minute flight does not hold its runway slot while slower cross-country flights catch up. The runway is always occupied with the next available plane.

### [Link to Scheduling Decisions](https://mbrenndoerfer.com/writing/continuous-batching\#scheduling-decisions) Scheduling DecisionsLink Copied

At each iteration boundary, the scheduler faces several interconnected decisions that determine both throughput and fairness. These decisions happen in microseconds and must balance competing objectives.

**Which requests to evict:** Besides naturally completed requests, the scheduler might preempt requests to free memory for higher-priority work. This creates interesting tradeoffs between fairness and throughput. A request that has already consumed significant resources is sunk compute cost, but continuing it also delays new requests. The scheduler must weigh these factors according to the system's priorities. Most production systems avoid preemption except under severe memory pressure, preferring instead to rely on natural completion to free slots.

**Which waiting requests to admit:** When multiple requests are waiting, the scheduler chooses which to start first. This decision materially impacts user-perceived latency and overall system efficiency. Options include:

- **FIFO.** First-come, first-served, which is simple and fair. Every request eventually gets its turn in the order it arrived. This approach gives predictable behavior and prevents starvation.
- **Shortest Job First.** Prioritize requests with shorter expected outputs to minimize average latency. Short requests complete quickly, improving user experience. However, this requires predicting output length, which is not always possible.
- **Priority-based.** Honor explicit priority levels attached to requests. Time-sensitive applications might receive preferential treatment. This approach requires clear policies about what constitutes priority and careful tuning to prevent starvation of low-priority requests.

**How to handle memory pressure:** When the [KV cache](https://mbrenndoerfer.com/writing/autoregressive-generation-gpt-text-generation) approaches capacity, the scheduler must decide whether to wait for completions or preempt active requests. Waiting reduces throughput but preserves work in progress. Preemption frees resources immediately but wastes the computation already invested in the preempted request. Systems like [vLLM](https://mbrenndoerfer.com/writing/paged-attention-vllm-kv-cache-memory-management) use a "swap" mechanism that saves KV caches to CPU memory before preemption, letting resumption without full recomputation, though this adds latency.

The scheduling policy also determines how the system handles surges. When a sudden burst of requests arrives and the queue grows long, the scheduler must decide whether to admit many short requests quickly or maintain fair ordering. Most production systems prioritize fairness to avoid the situation where a user submitting a long job during a burst waits arbitrarily longer than someone who arrived moments later.

### [Link to Prefill and Decode Phases](https://mbrenndoerfer.com/writing/continuous-batching\#prefill-and-decode-phases) Prefill and Decode PhasesLink Copied

Continuous batching must handle requests in different phases of their lifecycle. As we discussed in the KV Cache chapter, generation has two distinct phases with fundamentally different computational characteristics.

**Prefill phase:** Process all input tokens in parallel to build the initial [KV cache](https://mbrenndoerfer.com/writing/autoregressive-generation-gpt-text-generation). This phase is compute-bound and benefits from large batches of tokens. The GPU performs dense matrix multiplications over potentially thousands of input tokens, fully utilizing its computational capacity. Memory bandwidth is not the bottleneck during prefill because the computation is so intensive. A prompt with 2000 tokens generates 2000 key-value pairs simultaneously in a single forward pass.

**Decode phase:** Generate tokens one at a time, reading from the KV cache. This phase is memory-bandwidth-bound and benefits from large batches of requests. Each decode step reads the entire KV cache but performs relatively little computation per byte loaded. The GPU spends most of its time waiting for memory operations to complete, not performing arithmetic. This is why batching many requests together in the decode phase is so important: each request reads a different part of memory in parallel, amortizing the memory latency across many concurrent streams.

These phases have different computational characteristics that create challenges when mixing them in the same batch. Prefill processes many tokens per request; decode processes one token per request. Mixing them creates [load imbalance](https://mbrenndoerfer.com/writing/moe-gating-networks-router-architecture-design): prefill requests demand heavy compute while decode requests sit largely idle waiting for memory operations to complete. The compute and memory bandwidth resources are used differently, which makes it harder to fully utilize either.

In\[12\]:

Code

```
@dataclass
class PhasedRequest:
    """Request tracking both prefill and decode phases."""

    id: int
    input_length: int
    output_length: int
    phase: str = "prefill"  # "prefill" or "decode"
    tokens_generated: int = 0
    prefill_complete: bool = False

class PhasedBatchSimulator:
    """Simulator showing prefill/decode phase handling."""

    def __init__(self, max_batch_size: int = 8):
        self.max_batch_size = max_batch_size
        self.active_batch: List[PhasedRequest] = []
        self.waiting_queue: deque = deque()
        self.completed: List[PhasedRequest] = []
        self.iteration = 0

        # Track phase composition over time
        self.phase_history = []

    def submit_request(self, request: PhasedRequest):
        self.waiting_queue.append(request)

    def _fill_batch(self):
        """Add waiting requests to available slots."""
        while (
            len(self.active_batch) < self.max_batch_size and self.waiting_queue
        ):
            request = self.waiting_queue.popleft()
            request.phase = "prefill"
            self.active_batch.append(request)

    def step(self):
        """Execute one iteration."""
        self._fill_batch()

        if not self.active_batch:
            return False

        # Count phases
        prefill_count = sum(
            1 for r in self.active_batch if r.phase == "prefill"
        )
        decode_count = len(self.active_batch) - prefill_count
        self.phase_history.append(
            {
                "iteration": self.iteration,
                "prefill": prefill_count,
                "decode": decode_count,
            }
        )

        # Process each request based on phase
        still_active = []
        for request in self.active_batch:
            if request.phase == "prefill":
                # Prefill completes in one iteration (simplified)
                request.prefill_complete = True
                request.phase = "decode"
                still_active.append(request)
            else:  # decode phase
                request.tokens_generated += 1
                if request.tokens_generated >= request.output_length:
                    self.completed.append(request)
                else:
                    still_active.append(request)

        self.active_batch = still_active
        self.iteration += 1
        return True

    def run_until_complete(self):
        while self.active_batch or self.waiting_queue:
            self.step()
```

Advertisement

### [Link to Chunked Prefill](https://mbrenndoerfer.com/writing/continuous-batching\#chunked-prefill) Chunked PrefillLink Copied

Modern continuous batching systems often use chunked prefill to better balance the two phases and smooth out the computational load. Instead of processing the entire prompt in one iteration, which can cause latency spikes when long prompts arrive, long prompts are split into manageable chunks.

The problem that chunked prefill solves is sometimes called "prefill monopolization." When a request with a 4000-token prompt arrives, the prefill phase consumes the entire GPU for 10-15 milliseconds while thousands of queued decode steps for existing requests are blocked. From those existing requests' perspectives, the system has frozen. Their time-to-next-token spikes sharply, even though they have been running smoothly for hundreds of iterations.

The chunking strategy divides a long prompt into pieces that can be processed incrementally. This allows decode iterations for other requests to proceed between prefill chunks, maintaining responsiveness. A request with a 3000-token prompt might be split into six chunks of 500 tokens each, with each chunk processed in a separate iteration, interleaved with decode steps for existing requests.

In\[13\]:

Code

```
def chunk_prefill_tokens(input_length: int, chunk_size: int = 512) -> List[int]:
    """Split a prompt into prefill chunks."""
    chunks = []
    remaining = input_length
    while remaining > 0:
        chunk = min(remaining, chunk_size)
        chunks.append(chunk)
        remaining -= chunk
    return chunks
```

In\[14\]:

Code

```
# Example chunking
chunk_size = 512
example_lengths = [128, 512, 1500, 3000]
chunking_results = []

for length in example_lengths:
    chunks = chunk_prefill_tokens(length, chunk_size)
    chunking_results.append((length, chunks))
```

Out\[15\]:

Console

```
Prefill Chunking Examples (chunk_size=512):
  128 tokens -> 1 chunks: [128]
  512 tokens -> 1 chunks: [512]
  1500 tokens -> 3 chunks: [512, 512, 476]
  3000 tokens -> 6 chunks: [512, 512, 512, 512, 512, 440]
```

Out\[16\]:

Visualization

![Horizontal timeline alternating decode steps in blue and prefill chunks in orange across a single row.](https://assets.mbrenndoerfer.com/notebooks/13_continuous_batching_files/chunked-prefill-timeline.png)

Interleaved execution of prefill chunks and decode steps. Request C arrives with a long prompt that requires three prefill chunks (orange). Rather than blocking all existing requests for the duration, each chunk is interleaved with decode steps for Requests A and B (blue). Once all chunks complete, Request C joins the decode phase alongside the others.

Chunked prefill lets interleaving decode tokens between prefill chunks, reducing latency spikes when long prompts arrive. Without chunking, a 3000-token prompt would monopolize the GPU for a significant duration, blocking all decode operations for other requests. With chunking, decode operations proceed smoothly while the long prompt is processed incrementally. The cost is that the prefilling request takes longer to complete its context ingestion, but this tradeoff is almost always worth it in serving scenarios with multiple concurrent users.

Advertisement

## [Link to Request Completion Handling](https://mbrenndoerfer.com/writing/continuous-batching\#request-completion-handling) Request Completion HandlingLink Copied

Handling completed requests efficiently is important for continuous batching performance. The system must quickly detect completions, release resources, and fill the vacated slots. Any delay in this process reduces the benefits of continuous batching by leaving slots empty when work is available.

The completion handling pipeline runs at the boundary between every pair of iterations. It must execute fast enough to avoid becoming a bottleneck: if the completion check takes 5ms and the decode step takes 10ms, you have added 50% overhead. Production implementations use highly optimized routines, often scanning completion conditions directly on GPU tensors without transferring data to CPU.

### [Link to Completion Detection](https://mbrenndoerfer.com/writing/continuous-batching\#completion-detection) Completion DetectionLink Copied

A request completes when it generates a special end-of-sequence token or reaches a maximum length limit. The completion check happens after each decode step and must be fast enough to avoid becoming a bottleneck. In practice, this means simple integer comparisons that execute in nanoseconds.

The end-of-sequence token is a special marker that the model has learned to generate when it believes the response is complete. Different models use different token IDs for this purpose. [GPT](https://mbrenndoerfer.com/writing/gpt1-gpt2-autoregressive-pretraining-transfer-learning)-family models typically use token ID 50256, while [LLaMA](https://mbrenndoerfer.com/writing/llama-architecture-design-training-efficiency) models use token ID 2. The maximum length limit is a safety valve, preventing runaway generation that would consume resources to long sequences. Setting this limit requires balancing protection against runaway costs with flexibility for legitimately long responses.

In\[17\]:

Code

```
def check_completion(
    generated_token: int,
    tokens_generated: int,
    max_length: int,
    eos_token_id: int = 2,
) -> tuple[bool, str]:
    """
    Check if a request has completed.

    Returns:
        (is_complete, reason)
    """
    if generated_token == eos_token_id:
        return True, "eos_token"
    if tokens_generated >= max_length:
        return True, "max_length"
    return False, "ongoing"
```

### [Link to Resource Release](https://mbrenndoerfer.com/writing/continuous-batching\#resource-release) Resource ReleaseLink Copied

When a request completes, the system must release several resources promptly to make room for new work. Each resource type has its own release mechanism and timing considerations.

- **[KV cache memory](https://mbrenndoerfer.com/writing/multi-query-attention-memory-efficient-inference).** The cached key-value tensors for all layers must be freed. With [paged attention](https://mbrenndoerfer.com/writing/paged-attention-vllm-kv-cache-memory-management), as discussed in the Paged Attention chapter, this means returning memory blocks to the free pool. The blocks become immediately available for allocation to new requests, maximizing memory utilization.
- **Batch slot.** The position in the batch tensor becomes available for a new request. This slot is one concurrent generation stream, and freeing it allows the waiting queue to advance.
- **Request metadata.** Tracking structures for the completed request can be cleaned up. This includes timing information, token histories, and any other per-request state maintained by the scheduler.

The order of these release operations matters. KV cache memory should be freed before the new request attempts to allocate it. The batch slot assignment should complete before the next iteration begins. Careful sequencing ensures that resource counters remain consistent and that we never accidentally double-allocate memory.

In\[18\]:

Code

```
@dataclass
class SlotState:
    """Tracks the state of a batch slot."""

    slot_id: int
    request_id: int | None = None
    is_free: bool = True
    kv_blocks_allocated: int = 0

class SlotManager:
    """Manages batch slots for continuous batching."""

    def __init__(self, num_slots: int, blocks_per_slot: int = 16):
        self.slots = [SlotState(slot_id=i) for i in range(num_slots)]
        self.blocks_per_slot = blocks_per_slot
        self.total_blocks = num_slots * blocks_per_slot
        self.free_blocks = self.total_blocks

    def allocate_slot(self, request_id: int, num_blocks: int) -> int | None:
        """Allocate a slot for a new request."""
        if num_blocks > self.free_blocks:
            return None  # Not enough memory

        for slot in self.slots:
            if slot.is_free:
                slot.is_free = False
                slot.request_id = request_id
                slot.kv_blocks_allocated = num_blocks
                self.free_blocks -= num_blocks
                return slot.slot_id

        return None  # No free slots

    def release_slot(self, slot_id: int) -> int:
        """Release a slot and return freed blocks."""
        slot = self.slots[slot_id]
        freed_blocks = slot.kv_blocks_allocated

        slot.is_free = True
        slot.request_id = None
        slot.kv_blocks_allocated = 0
        self.free_blocks += freed_blocks

        return freed_blocks

    def get_active_count(self) -> int:
        return sum(1 for s in self.slots if not s.is_free)
```

In\[19\]:

Code

```
# Demonstrate slot management
manager = SlotManager(num_slots=4, blocks_per_slot=16)

# Define request requirements
req1_id, req1_blocks = 100, 8
req2_id, req2_blocks = 101, 12
req3_id, req3_blocks = 102, 6

# Allocate some slots
slot1 = manager.allocate_slot(request_id=req1_id, num_blocks=req1_blocks)
slot2 = manager.allocate_slot(request_id=req2_id, num_blocks=req2_blocks)
slot3 = manager.allocate_slot(request_id=req3_id, num_blocks=req3_blocks)
```

Out\[20\]:

Console

```
Slot Management Demo:
  Initial free blocks: 64
  Allocated slot 0 for request 100 (8 blocks)
  Allocated slot 1 for request 101 (12 blocks)
  Allocated slot 2 for request 102 (6 blocks)
  Free blocks remaining: 38
  Active requests: 3
```

The manager successfully allocates blocks for three requests, decrementing the free pool accordingly. This tracking ensures we never overcommit the available [KV cache memory](https://mbrenndoerfer.com/writing/multi-query-attention-memory-efficient-inference), which would lead to out-of-memory errors during generation.

In\[21\]:

Code

```
# Complete request 101 and free its resources
freed = manager.release_slot(slot2)
```

Out\[22\]:

Console

```
Released slot 1 (request 101)
  Freed 12 blocks
  Free blocks now: 50
  Active requests: 2
```

Releasing the slot immediately returns blocks to the free pool, making them available for waiting requests in the very next iteration. This rapid turnover is what lets continuous batching to maintain high utilization. Notice that the freed blocks become available synchronously, so the very next call to `_fill_batch` can allocate them to a new request without any delay.

Advertisement

### [Link to Immediate Response Delivery](https://mbrenndoerfer.com/writing/continuous-batching\#immediate-response-delivery) Immediate Response DeliveryLink Copied

Unlike static batching, continuous batching can return completed responses immediately. Users don't wait for other requests in the batch, receiving results the moment generation finishes. This eliminates the latency inflation common in static batching and gives a more responsive experience.

The immediate delivery mechanism requires careful coordination between the [generation loop](https://mbrenndoerfer.com/writing/autoregressive-generation-gpt-text-generation) and the response handling system. Completed results must be extracted and transmitted without blocking the main processing loop, often using asynchronous I/O or separate response threads. A naive implementation might pause the generation loop to send each response, but this would introduce jitter that accumulates across many requests. Production systems decouple response delivery from generation, queuing completed results for transmission by a separate thread or event loop.

In\[23\]:

Code

```
@dataclass
class TimedRequest:
    """Request with timing information."""

    id: int
    output_length: int
    submit_time: float = 0.0
    complete_time: float = 0.0

    @property
    def latency(self) -> float:
        return self.complete_time - self.submit_time

class LatencyTracker:
    """Tracks per-request latencies."""

    def __init__(self):
        self.requests: dict[int, TimedRequest] = {}

    def submit(self, request: TimedRequest, current_time: float):
        request.submit_time = current_time
        self.requests[request.id] = request

    def complete(self, request_id: int, current_time: float):
        if request_id in self.requests:
            self.requests[request_id].complete_time = current_time

    def get_latencies(self) -> List[float]:
        return [\
            r.latency for r in self.requests.values() if r.complete_time > 0\
        ]
```

Advertisement

## [Link to Throughput Analysis](https://mbrenndoerfer.com/writing/continuous-batching\#throughput-analysis) Throughput AnalysisLink Copied

Let's compare the throughput of static and continuous batching quantitatively. Understanding the mathematical relationship between these approaches helps explain why continuous batching delivers such significant improvements and under what conditions those improvements are largest.

The analysis that follows formalizes the intuition we built earlier: static batching allocates computational capacity based on the worst case in each batch, while continuous batching adapts to the actual workload [token by token](https://mbrenndoerfer.com/writing/autoregressive-generation-gpt-text-generation). The efficiency gap between these strategies grows with the [variance](https://mbrenndoerfer.com/writing/descriptive-statistics-guide-python-data-analysis) of output lengths, which is exactly what characterizes real LLM workloads.

### [Link to Theoretical Analysis](https://mbrenndoerfer.com/writing/continuous-batching\#theoretical-analysis) Theoretical AnalysisLink Copied

To understand the efficiency difference mathematically, we need to formalize what happens in each batching strategy. The key insight is that static batching allocates computational capacity based on the worst case, while continuous batching adapts to the actual workload.

For static batching with a fixed [batch size](https://mbrenndoerfer.com/writing/stochastic-gradient-descent-neural-network-optimization) BBB containing requests with output lengths L1,L2,…,LBL\_1, L\_2, \\ldots, L\_BL1​,L2​,…,LB​, the total computational time is determined by the longest request. Every request in the batch must wait for this slowest member, regardless of whether it has long since completed. The total computational capacity allocated, measured in token-slots (one slot processing one token), is:

Cstatic=B×max⁡(L1,L2,…,LB)C\_{\\text{static}} = B \\times \\max(L\_1, L\_2, \\ldots, L\_B)Cstatic​=B×max(L1​,L2​,…,LB​)

where:

- CstaticC\_{\\text{static}}Cstatic​: the total number of processing slots consumed (iterations ×\\times×batch size)
- BBB: the fixed batch size
- LiL\_iLi​: the output length of the iii-th request
- max⁡(L1,…,LB)\\max(L\_1, \\ldots, L\_B)max(L1​,…,LB​): the length of the longest sequence in the batch

This formula captures the basic problem: we allocate capacity based on the maximum, not the sum. Every slot runs for as many iterations as the longest sequence demands, even if that slot has been creating padding for 90% of those iterations.

We can calculate the compute efficiency ηstatic\\eta\_{\\text{static}}ηstatic​ as the ratio of useful work to total capacity. Useful work is the actual tokens generated, while total capacity is the processing slots allocated:

ηstatic=∑i=1BLiB×max⁡jLj\\eta\_{\\text{static}} = \\frac{\\sum\_{i=1}^{B} L\_i}{B \\times \\max\_{j} L\_j}ηstatic​=B×maxj​Lj​∑i=1B​Li​​

where:

- ηstatic\\eta\_{\\text{static}}ηstatic​: the computational efficiency (between 0 and 1)
- ∑i=1BLi\\sum\_{i=1}^{B} L\_i∑i=1B​Li​: the total number of generated tokens (useful work)
- B×max⁡jLjB \\times \\max\_{j} L\_jB×maxj​Lj​: the total capacity allocated during the batch's lifetime

Notice that efficiency depends heavily on the distribution of output lengths. If all requests have the same length, the numerator equals the denominator and efficiency is 100%. But if one request is much longer than the others, efficiency drops sharply.

To make this concrete, consider a batch of 8 requests where 7 need 20 tokens and 1 needs 200 tokens. We can work through the efficiency calculation step by step:

∑i=18Li=7×20+200=340 tokensB×max⁡jLj=8×200=1,600 token-slotsηstatic=3401,600≈0.21\\begin{aligned}
\\sum\_{i=1}^{8} L\_i &= 7 \\times 20 + 200 = 340 \\text{ tokens} \\\
B \\times \\max\_j L\_j &= 8 \\times 200 = 1{,}600 \\text{ token-slots} \\\
\\eta\_{\\text{static}} &= \\frac{340}{1{,}600} \\approx 0.21
\\end{aligned}i=1∑8​Li​B×jmax​Lj​ηstatic​​=7×20+200=340 tokens=8×200=1,600 token-slots=1,600340​≈0.21​

The system performs 1600 computational operations but only 340 of them produce useful output. That is 79% wasted compute, or a 21% efficiency rating. The single outlier request forces seven other requests to continue burning resources for 180 extra iterations each.

For continuous batching, the situation is fundamentally different. Assuming the request queue remains populated (perfect slot filling), the system avoids waiting for long requests. When a request completes, its slot immediately fills with a new request. The total iterations required approach the theoretical minimum:

Tcontinuous≈∑i=1NLiBT\_{\\text{continuous}} \\approx \\frac{\\sum\_{i=1}^{N} L\_i}{B}Tcontinuous​≈B∑i=1N​Li​​

where:

- TcontinuousT\_{\\text{continuous}}Tcontinuous​: the approximate total iterations needed
- NNN: the total number of requests in the workload
- LiL\_iLi​: the output length of the iii-th request
- BBB: the maximum [batch size](https://mbrenndoerfer.com/writing/stochastic-gradient-descent-neural-network-optimization) (concurrency limit)
- ∑i=1NLi\\sum\_{i=1}^{N} L\_i∑i=1N​Li​: the total number of tokens to be generated across all requests

This formula shows that continuous batching distributes work evenly across the available batch slots. The total iterations is simply the total tokens divided by the parallelism factor. Doubling BBB halves the time, and adding more requests to a non-empty queue barely changes the total time because they fill slots freed by completions.

We can derive the efficiency ηcontinuous\\eta\_{\\text{continuous}}ηcontinuous​ by comparing useful work to allocated capacity:

ηcontinuous=Useful WorkTotal Capacity=∑i=1NLiTcontinuous×B≈∑i=1NLi(∑i=1NLiB)×B=1\\begin{aligned}
\\eta\_{\\text{continuous}} &= \\frac{\\text{Useful Work}}{\\text{Total Capacity}} \\\
&= \\frac{\\sum\_{i=1}^{N} L\_i}{T\_{\\text{continuous}} \\times B} \\\
&\\approx \\frac{\\sum\_{i=1}^{N} L\_i}{\\left( \\frac{\\sum\_{i=1}^{N} L\_i}{B} \\right) \\times B} \\\
&= 1
\\end{aligned}ηcontinuous​​=Total CapacityUseful Work​=Tcontinuous​×B∑i=1N​Li​​≈(B∑i=1N​Li​​)×B∑i=1N​Li​​=1​

This shows that efficiency approaches 100% because the total capacity used matches the useful work. Every slot-iteration produces a real output token rather than padding. The approximation becomes exact when the queue is always full and requests are perfectly divisible across slots.

The practical implication is that continuous batching eliminates the sensitivity to output length distribution. Whether requests generate 10 tokens or 1000, the throughput stays near the theoretical maximum. Static batching is at the mercy of whoever happens to be in the same batch as you, while continuous batching processes everyone at their natural speed.

### [Link to Empirical Comparison](https://mbrenndoerfer.com/writing/continuous-batching\#empirical-comparison) Empirical ComparisonLink Copied

Let's run a more complete simulation comparing the two approaches across a realistic workload. Theory tells us continuous batching should win, but simulation reveals the magnitude of improvement and how it varies with workload characteristics.

In\[24\]:

Code

```
def simulate_workload(
    num_requests: int, batch_size: int, output_lengths: List[int], method: str
) -> dict:
    """Simulate a workload with either static or continuous batching."""

    if method == "static":
        # Process in fixed batches
        total_iterations = 0
        total_latency = 0

        for batch_start in range(0, num_requests, batch_size):
            batch_end = min(batch_start + batch_size, num_requests)
            batch_lengths = output_lengths[batch_start:batch_end]
            max_len = max(batch_lengths)

            total_iterations += max_len
            # All requests in batch have same completion time
            for length in batch_lengths:
                total_latency += max_len  # Wait for slowest

        return {
            "total_iterations": total_iterations,
            "average_latency": total_latency / num_requests,
            "throughput": num_requests / total_iterations,
        }

    else:  # continuous
        # Simulate continuous batching
        remaining = list(output_lengths)
        active = []
        waiting_idx = 0
        iteration = 0
        completion_times = []

        while active or waiting_idx < len(remaining):
            # Fill batch with waiting requests
            while len(active) < batch_size and waiting_idx < len(remaining):
                active.append(
                    {
                        "idx": waiting_idx,
                        "remaining": remaining[waiting_idx],
                        "start": iteration,
                    }
                )
                waiting_idx += 1

            if not active:
                break

            iteration += 1

            # Decrement all active and check completions
            still_active = []
            for req in active:
                req["remaining"] -= 1
                if req["remaining"] <= 0:
                    completion_times.append(iteration - req["start"])
                else:
                    still_active.append(req)
            active = still_active

        return {
            "total_iterations": iteration,
            "average_latency": np.mean(completion_times),
            "throughput": num_requests / iteration,
        }
```

In\[25\]:

Code

```
# Generate a realistic workload with variable output lengths
np.random.seed(123)
num_requests = 100
batch_size = 8

# Output lengths following a log-normal distribution (common in real workloads)
output_lengths = np.random.lognormal(
    mean=4.0, sigma=0.8, size=num_requests
).astype(int)
output_lengths = np.clip(output_lengths, 10, 500).tolist()

static_results = simulate_workload(
    num_requests, batch_size, output_lengths, "static"
)
continuous_results = simulate_workload(
    num_requests, batch_size, output_lengths, "continuous"
)
```

Out\[26\]:

Console

```
Workload: 100 requests, batch size 8
Output length range: 10 - 370 tokens
Mean output length: 82.2 tokens

Static Batching:
  Total iterations: 2722
  Average latency: 214.8 iterations
  Throughput: 0.037 requests/iteration

Continuous Batching:
  Total iterations: 1148
  Average latency: 82.2 iterations
  Throughput: 0.087 requests/iteration

Improvement:
  Throughput gain: 2.37x
  Latency reduction: 61.7%
```

Continuous batching reaches materially higher throughput and lower average latency. While the static batch is held back by the longest sequence in each group, continuous batching processes requests at their natural speed, returning short responses quickly while long ones continue processing.

In\[27\]:

Code

```
# Test with different variance levels
variances = [0.2, 0.5, 0.8, 1.2]
static_throughputs = []
continuous_throughputs = []
static_latencies = []
continuous_latencies = []

for var in variances:
    lengths = np.random.lognormal(mean=4.0, sigma=var, size=100).astype(int)
    lengths = np.clip(lengths, 10, 500).tolist()

    static = simulate_workload(100, 8, lengths, "static")
    continuous = simulate_workload(100, 8, lengths, "continuous")

    static_throughputs.append(static["throughput"])
    continuous_throughputs.append(continuous["throughput"])
    static_latencies.append(static["average_latency"])
    continuous_latencies.append(continuous["average_latency"])
```

Out\[28\]:

Visualization

![Grouped bar chart comparing throughput of static versus continuous batching at four variance levels. Continuous batching bars are taller at all variance levels.](https://assets.mbrenndoerfer.com/notebooks/13_continuous_batching_files/static-vs-continuous-batching-throughput.png)

Throughput comparison across output length variances. Continuous batching maintains consistently high throughput as variance increases, while static batching degrades because higher variance means more extreme outlier lengths that force the entire batch to wait.

![Grouped bar chart comparing latency of static versus continuous batching at four variance levels. Static batching latency grows with variance while continuous remains low.](https://assets.mbrenndoerfer.com/notebooks/13_continuous_batching_files/static-vs-continuous-batching-latency.png)

Average latency comparison across output length variances. Continuous batching latency stays low and stable, while static batching latency grows sharply with variance because short requests must wait progressively longer for their batch's outlier to finish.

The improvement from continuous batching grows with output length [variance](https://mbrenndoerfer.com/writing/descriptive-statistics-guide-python-data-analysis). When all requests generate similar lengths, static batching loses less to waiting because the maximum is close to the average. But real workloads have high variance: some users ask for single-sentence summaries while others request long document analyses. This variance makes continuous batching useful in practical deployments.

Advertisement

## [Link to Worked Example: Step-by-Step Trace](https://mbrenndoerfer.com/writing/continuous-batching\#worked-example-step-by-step-trace) Worked Example: Step-by-Step TraceLink Copied

To cement the intuition, let's trace through a small concrete example comparing both strategies on the same five requests. This walkthrough shows exactly what happens at each iteration and why the results differ.

Suppose we have a maximum [batch size](https://mbrenndoerfer.com/writing/stochastic-gradient-descent-neural-network-optimization) of 3, and five requests arrive all at once with the following output lengths: Request A needs 4 tokens, Request B needs 2 tokens, Request C needs 6 tokens, Request D needs 3 tokens, and Request E needs 5 tokens.

**Static batching:** The system collects all five requests but the batch size is 3, so it forms two batches. Batch 1 contains Requests A, B, C. Batch 2 contains Requests D and E (padded to a batch of 2 since no more requests exist).

For Batch 1, the longest request is C with 6 tokens. All three requests run for 6 iterations:

- Iterations 1-2: A, B, C all active (B finishes at iteration 2 but stays in batch)
- Iterations 3-4: A, B (idle), C all running (A finishes at iteration 4 but stays in batch)
- Iterations 5-6: A (idle), B (idle), C all running

Total slot-iterations for Batch 1: 3×6=183 \\times 6 = 183×6=18. Useful tokens: 4+2+6=124 + 2 + 6 = 124+2+6=12. Efficiency: 12/18≈67%12/18 \\approx 67\\%12/18≈67%.

Batch 2 begins only after Batch 1 finishes. The longest of D (3 tokens) and E (5 tokens) is 5, so both run for 5 iterations.

Total slot-iterations for Batch 2: 2×5=102 \\times 5 = 102×5=10. Useful tokens: 3+5=83 + 5 = 83+5=8. Efficiency: 8/10=80%8/10 = 80\\%8/10=80%.

Grand total: 28 slot-iterations, 20 useful tokens, overall efficiency 20/28≈71%20/28 \\approx 71\\%20/28≈71%. Total wall-clock iterations: 6+5=116 + 5 = 116+5=11.

**Continuous batching:** The system loads Requests A, B, C into the three available slots and begins immediately. Requests D and E wait in the queue.

- Iteration 1: A generates token 1, B generates token 1, C generates token 1. No completions. Queue: D, E.
- Iteration 2: A generates token 2, B generates token 2 (B is now complete), C generates token 2. B exits. D enters. Slot composition: A, C, D.
- Iteration 3: A generates token 3, C generates token 3, D generates token 1. No completions. Queue: E.
- Iteration 4: A generates token 4 (A complete), C generates token 4, D generates token 2. A exits. E enters. Slot composition: C, D, E.
- Iteration 5: C generates token 5, D generates token 3 (D complete), E generates token 1. D exits. Queue empty. Slot composition: C, E.
- Iteration 6: C generates token 6 (C complete), E generates token 2. C exits. Slot composition: E.
- Iteration 7: E generates token 3. No completions. Slot composition: E.
- Iteration 8: E generates token 4. No completions. Slot composition: E.
- Iteration 9: E generates token 5 (E complete). All done.

Total wall-clock iterations: 9. Total slot-iterations used: let's count. Iterations 1-4 had 3 active requests each (12), iteration 5 had 2 (2), iteration 6 had 2 (2), iterations 7-9 had 1 each (3). Total: 12+2+2+3=1912 + 2 + 2 + 3 = 1912+2+2+3=19slot-iterations for 20 useful tokens. Efficiency: 20/1920/1920/19 which rounds to approximately 100% once you account for the single unavoidable idle slot-iteration when only E remained.

Static batching needed 11 wall-clock iterations and 28 slot-iterations. Continuous batching needed 9 wall-clock iterations and used only 19 slot-iterations. This is a modest example; with larger batches and higher [variance](https://mbrenndoerfer.com/writing/descriptive-statistics-guide-python-data-analysis), the gap grows substantially.

Advertisement

## [Link to Implementation Considerations](https://mbrenndoerfer.com/writing/continuous-batching\#implementation-considerations) Implementation ConsiderationsLink Copied

Building a production continuous batching system requires careful attention to several practical concerns that go beyond the basic algorithm. The theory is elegant, but the engineering details determine whether the system reaches its theoretical potential.

The main challenges fall into four areas: memory management, preemption handling, tensor operations, and monitoring. Each requires careful design. A system that gets the scheduling logic right but fails at memory management will hit out-of-memory errors under load. A system that handles memory well but lacks proper monitoring will be opaque when performance degrades.

### [Link to Memory Management Integration](https://mbrenndoerfer.com/writing/continuous-batching\#memory-management-integration) Memory Management IntegrationLink Copied

Continuous batching works best when paired with [paged attention](https://mbrenndoerfer.com/writing/paged-attention-vllm-kv-cache-memory-management), as covered in the Paged Attention chapter. Without it, memory fragmentation limits how many requests can be active simultaneously. Traditional contiguous allocation reserves maximum-length [KV cache](https://mbrenndoerfer.com/writing/autoregressive-generation-gpt-text-generation) space for each request, even if most requests use far less. Paged attention allocates memory in small blocks on demand, letting much higher concurrency.

The interaction between continuous batching and paged memory is symbiotic. Continuous batching needs fine-grained memory release to quickly fill freed slots. Paged attention gives exactly this: when a request completes, its pages return to the free pool immediately, ready for allocation to incoming requests. The combination reaches both high throughput (through continuous batching) and high memory efficiency (through paged attention).

In\[29\]:

Code

```
@dataclass
class MemoryConfig:
    """Configuration for KV cache memory."""

    total_blocks: int  # Total memory blocks available
    block_size: int  # Tokens per block

def estimate_blocks_needed(seq_length: int, block_size: int) -> int:
    """Estimate memory blocks needed for a sequence."""
    return (seq_length + block_size - 1) // block_size  # Ceiling division

class MemoryAwareScheduler:
    """Scheduler that respects memory constraints."""

    def __init__(self, config: MemoryConfig, max_batch_size: int):
        self.config = config
        self.max_batch_size = max_batch_size
        self.free_blocks = config.total_blocks
        self.active_requests: dict[int, int] = {}  # request_id -> blocks_used

    def can_admit(self, request_length: int) -> bool:
        """Check if we have memory for a new request."""
        needed = estimate_blocks_needed(request_length, self.config.block_size)
        return (
            needed <= self.free_blocks
            and len(self.active_requests) < self.max_batch_size
        )

    def admit_request(self, request_id: int, initial_length: int) -> bool:
        """Admit a request if resources are available."""
        if not self.can_admit(initial_length):
            return False

        blocks = estimate_blocks_needed(initial_length, self.config.block_size)
        self.active_requests[request_id] = blocks
        self.free_blocks -= blocks
        return True

    def grow_request(self, request_id: int, new_tokens: int = 1) -> bool:
        """Allocate additional blocks as sequence grows."""
        if request_id not in self.active_requests:
            return False

        # Check if we need a new block
        current_blocks = self.active_requests[request_id]
        current_capacity = current_blocks * self.config.block_size

        # Simplified: assume sequence length = current_capacity + new_tokens
        new_length = current_capacity + new_tokens
        needed_blocks = estimate_blocks_needed(
            new_length, self.config.block_size
        )

        additional = needed_blocks - current_blocks
        if additional > 0:
            if additional > self.free_blocks:
                return False  # Out of memory
            self.free_blocks -= additional
            self.active_requests[request_id] = needed_blocks

        return True

    def release_request(self, request_id: int):
        """Release all blocks for a completed request."""
        if request_id in self.active_requests:
            self.free_blocks += self.active_requests[request_id]
            del self.active_requests[request_id]
```

### [Link to Handling Preemption](https://mbrenndoerfer.com/writing/continuous-batching\#handling-preemption) Handling PreemptionLink Copied

When memory pressure is high, the scheduler might need to preempt active requests to make room for higher-priority work. This involves saving the request's state, including the [KV cache](https://mbrenndoerfer.com/writing/autoregressive-generation-gpt-text-generation) and generated tokens, for later resumption. Preemption is a powerful tool for managing resources but adds significant complexity.

The decision of which request to preempt involves tradeoffs. A policy must balance fairness against throughput while remaining practical to implement. Preempting a request that has generated many tokens wastes the computation invested in it. Preempting a request that has been preempted before may seem unfair. Different policies balance these concerns differently. [vLLM](https://mbrenndoerfer.com/writing/paged-attention-vllm-kv-cache-memory-management)'s original implementation used a " [beam search](https://mbrenndoerfer.com/writing/beam-search-decoding-sequence-generation)" style where preempted requests had their KV caches swapped to CPU DRAM, letting resumption without full recomputation when memory pressure eased. This swap-based approach preserves work in progress at the cost of CPU memory and PCIe bandwidth.

An important nuance is that preemption should be a last resort, not a routine operation. Systems that preempt frequently can enter a thrashing state where they spend more time managing preemption and resumption than doing useful generation. The right response to chronic memory pressure is usually to reduce the maximum [batch size](https://mbrenndoerfer.com/writing/stochastic-gradient-descent-neural-network-optimization) rather than to preempt aggressively.

In\[30\]:

Code

```
from enum import Enum

class RequestStatus(Enum):
    WAITING = "waiting"
    RUNNING = "running"
    PREEMPTED = "preempted"
    COMPLETED = "completed"

@dataclass
class PreemptibleRequest:
    """Request that can be preempted and resumed."""

    id: int
    status: RequestStatus = RequestStatus.WAITING
    tokens_generated: int = 0
    preempt_count: int = 0

class PreemptionPolicy:
    """Policy for choosing which requests to preempt."""

    @staticmethod
    def longest_first(
        requests: List[PreemptibleRequest],
    ) -> PreemptibleRequest | None:
        """Preempt the request that has generated the most tokens."""
        running = [r for r in requests if r.status == RequestStatus.RUNNING]
        if not running:
            return None
        return max(running, key=lambda r: r.tokens_generated)

    @staticmethod
    def most_preempted_last(
        requests: List[PreemptibleRequest],
    ) -> PreemptibleRequest | None:
        """Prefer preempting requests that haven't been preempted before."""
        running = [r for r in requests if r.status == RequestStatus.RUNNING]
        if not running:
            return None
        return min(
            running, key=lambda r: (r.preempt_count, -r.tokens_generated)
        )
```

Advertisement

### [Link to Batched Token Operations](https://mbrenndoerfer.com/writing/continuous-batching\#batched-token-operations) Batched Token OperationsLink Copied

For efficiency, token generation for all active requests should happen in a single batched forward pass. This requires careful tensor management to handle requests at different sequence positions. Unlike training, where all sequences in a batch are padded to the same length, continuous batching must handle sequences of varying lengths efficiently.

The key challenge is that different requests have different position [embeddings](https://mbrenndoerfer.com/writing/long-term-knowledge-storage-and-retrieval) and attention masks. Position embeddings tell the model where each token sits in its sequence. Attention masks prevent tokens from attending to future positions. Both vary by request, but both must be batched together for efficient GPU utilization.

The solution is to process just one token per request per forward pass during the decode phase. Each request contributes a single token to the batch input, and each token carries its own position identifier. The position IDs differ across requests in the batch because each request is at a different point in its generation, but the batch still processes as a single matrix operation.

In\[31\]:

Code

```
import torch

def prepare_batch_tensors(
    active_requests: List[dict], vocab_size: int = 32000
) -> dict:
    """
    Prepare input tensors for a continuous batch.

    Each request has:
        - 'last_token': The most recently generated token
        - 'seq_length': Current sequence length (for position embedding)
    """
    batch_size = len(active_requests)

    # Each decode step processes just the last token
    input_ids = torch.tensor(
        [r["last_token"] for r in active_requests], dtype=torch.long
    ).unsqueeze(1)  # Shape: [batch, 1]

    # Position IDs vary by request
    position_ids = torch.tensor(
        [r["seq_length"] for r in active_requests], dtype=torch.long
    ).unsqueeze(1)  # Shape: [batch, 1]

    return {
        "input_ids": input_ids,
        "position_ids": position_ids,
        "batch_size": batch_size,
    }
```

In\[32\]:

Code

```
# Example batch preparation
example_requests = [\
    {"last_token": 1234, "seq_length": 50},  # Short sequence\
    {"last_token": 5678, "seq_length": 200},  # Medium sequence\
    {"last_token": 9012, "seq_length": 15},  # Very short\
    {"last_token": 3456, "seq_length": 500},  # Long sequence\
]

tensors = prepare_batch_tensors(example_requests)
```

Out\[33\]:

Console

```
Batch Tensor Preparation:
  Input IDs shape: torch.Size([4, 1])
  Input IDs: [1234, 5678, 9012, 3456]
  Position IDs: [50, 200, 15, 500]
```

The key insight is that despite requests being at different points in their generation, they can share a single forward pass because attention masks and position [embeddings](https://mbrenndoerfer.com/writing/long-term-knowledge-storage-and-retrieval) handle the differences. The input is just one token per request, but each token gets its own [position embedding](https://mbrenndoerfer.com/writing/sinusoidal-position-encoding-transformers-word-order) corresponding to its place in that request's sequence. The four requests above are at positions 50, 200, 15, and 500 in their respective sequences, yet they process together in the same matrix operations. This batching is what makes continuous batching efficient rather than just correct.

Advertisement

Join the Community

Enjoying this article?

![Michael Brenndoerfer](https://assets.mbrenndoerfer.com/_optimized/general/resume/michael_brenndoerfer-128w.webp)

Michael Brenndoerfer

I write about AI, data science, machine learning, finance, economics and entrepreneurship. Subscribe to get updates delivered straight to your inbox.

- No popups
- Unobstructed reading
- Commenting

Subscribe

No spam, unsubscribe anytime.

Join Community

[Join Community](https://mbrenndoerfer.com/community)

![Michael Brenndoerfer](https://assets.mbrenndoerfer.com/_optimized/general/resume/michael_brenndoerfer-480w.webp)

Michael Brenndoerfer

Author and community host

## [Link to Key Parameters](https://mbrenndoerfer.com/writing/continuous-batching\#key-parameters) Key ParametersLink Copied

Tuning a continuous batching system requires understanding how several parameters interact. Higher throughput may increase latency or memory use, and reducing memory pressure can affect the other two measures. Each parameter affects multiple aspects of system behavior, so changes often have non-obvious second-order effects.

The key parameters for the continuous batching implementation are:

- max\_batch\_size: Maximum number of concurrent requests. Higher values improve throughput but require more memory for KV caches. The optimal value depends on model size, available GPU memory, and expected request characteristics. Setting this too high leads to out-of-memory errors; too low leaves GPU compute underutilized.
- block\_size: Number of tokens per memory block in [paged attention](https://mbrenndoerfer.com/writing/paged-attention-vllm-kv-cache-memory-management). Typically 16 or 32. Smaller blocks reduce internal fragmentation (wasted space within a block) but increase bookkeeping overhead. Larger blocks are simpler to manage but waste more memory for short sequences.
- chunk\_size: Number of tokens processed per iteration during prefill. Helps balance latency and throughput. Smaller chunks give more responsive decode (less preemption of existing requests) but may reduce prefill efficiency due to increased per-iteration overhead.
- scheduler\_policy: The ordering policy for the waiting queue (FIFO, SJF, priority). This affects fairness and average latency more than raw throughput.
- preemption\_threshold: How full the memory pool must be before considering preemption. Setting this too low causes premature preemption; too high risks out-of-memory errors when a burst of long requests arrives.

Advertisement

## [Link to Real-World Performance](https://mbrenndoerfer.com/writing/continuous-batching\#real-world-performance) Real-World PerformanceLink Copied

Production continuous batching systems like [vLLM](https://mbrenndoerfer.com/writing/paged-attention-vllm-kv-cache-memory-management), TensorRT-LLM, and Text Generation Inference report significant improvements over static batching. These systems combine continuous batching with paged attention, optimized kernels, and advanced scheduling to reach impressive throughput on commodity hardware.

The gains in production are often even larger than the simulations suggest, because real workloads have extreme [variance](https://mbrenndoerfer.com/writing/descriptive-statistics-guide-python-data-analysis). A [production LLM](https://mbrenndoerfer.com/writing/inference-scaling-llm-deployment-optimization) serving endpoint might receive requests ranging from two-token completions to 4000-token document generations, all mixed together in the same serving pool. Static batching degrades catastrophically in this setting; continuous batching handles it naturally.

In\[34\]:

Code

```
# Simulated benchmark data based on published results
benchmark_data = {
    "batch_size": [1, 4, 8, 16, 32],
    "static_throughput": [15, 45, 70, 95, 110],  # tokens/sec
    "continuous_throughput": [15, 58, 120, 210, 350],  # tokens/sec
    "static_latency_p50": [65, 85, 110, 150, 220],  # ms
    "continuous_latency_p50": [65, 70, 75, 85, 100],  # ms
}
```

Out\[35\]:

Visualization

![Line plot showing throughput versus batch size for static and continuous batching. Continuous batching grows steeply while static flattens.](https://assets.mbrenndoerfer.com/notebooks/13_continuous_batching_files/throughput-scaling-batch-size.png)

Throughput scaling with maximum batch size. Continuous batching scales nearly linearly with batch size, reaching 350 tokens per second at batch size 32, while static batching plateaus around 110 because synchronization overhead from waiting for batch-mates dominates at larger batches.

![Line plot showing P50 latency versus batch size for static and continuous batching. Static latency rises steeply while continuous stays nearly flat.](https://assets.mbrenndoerfer.com/notebooks/13_continuous_batching_files/latency-scaling-batch-size.png)

P50 latency scaling with maximum batch size. Continuous batching maintains stable latency (65 to 100 ms) even as batch size grows, while static batching latency more than triples (65 to 220 ms) because larger batches mean longer waits for the slowest concurrent request.

The throughput advantage of continuous batching compounds at larger batch sizes because GPU utilization stays high even as requests complete at different times. Static batching's throughput plateaus as the "waiting for slowest" overhead dominates, while continuous batching continues to scale. The latency curves tell an equally important story: continuous batching's latency grows slowly because each request's wait time is bounded by how long it takes to process its own tokens, not the tokens of batch-mates.

Advertisement

## [Link to Limitations and Impact](https://mbrenndoerfer.com/writing/continuous-batching\#limitations-and-impact) Limitations and ImpactLink Copied

Continuous batching improves LLM serving but comes with challenges and tradeoffs. Its throughput and latency benefits require additional engineering.

The primary complexity lies in implementation. A continuous batching system requires tight integration between the scheduler, memory manager, and model execution engine. The scheduler must make split-second decisions about which requests to run, potentially thousands of times per second. Any inefficiency in this hot path directly reduces throughput. Debugging also becomes harder because requests can be preempted and later resumed while other requests run between them. Recreating issues requires capturing the exact sequence of scheduling decisions, which may be difficult in a highly dynamic system. A request that fails intermittently in production might have been scheduled next to a particularly long request or admitted just as memory pressure spiked; these interactions are difficult to replay in isolation.

Memory management presents another significant challenge. While [paged attention](https://mbrenndoerfer.com/writing/paged-attention-vllm-kv-cache-memory-management) solves fragmentation, the combination of variable-length requests and preemption creates complex allocation patterns. Memory pressure can cascade: preempting one request to admit another might itself require preemption of a third request, leading to difficult-to-predict behavior under load. Production systems need careful tuning of admission policies and preemption thresholds to avoid thrashing, where the system spends more time managing state transitions than generating tokens. The right setup depends heavily on the expected workload distribution, which can shift over time.

There are also fairness considerations that static batching never needed to address. Without careful policy design, long requests might be repeatedly preempted in favor of short ones, leading to starvation where a document-generation task waits indefinitely while the system processes an endless stream of brief queries. Conversely, prioritizing completion of long requests might inflate latency for short queries. Finding the right balance depends on the specific application's requirements and user expectations. Many production systems expose scheduler policies as setup, letting operators to tune the tradeoff for their use case.

A subtler limitation involves the mixed-phase problem. When prefill and decode requests share the same batch, neither phase gets optimal hardware utilization. Prefill benefits from large token batches that saturate compute; decode benefits from large request batches that saturate memory bandwidth. Mixing them creates an in-between state that is not ideal for either. Chunked prefill mitigates this by limiting how much prefill work can occur in any single iteration, but it does not eliminate the basic tension. Some systems address this by maintaining separate prefill and decode "pools" that coordinate to share GPU time, adding further scheduling complexity.

Despite these challenges, continuous batching has fundamentally changed LLM deployment economics. Systems that previously needed multiple GPUs to serve moderate traffic can now handle the same load on a single device. This efficiency improvement makes LLMs practical for a much wider range of applications and organizations. The technique has become standard in production serving frameworks, and understanding its principles is needed for anyone deploying language models at scale. The Orca paper that introduced the idea reported up to 36.9x throughput improvements, and production deployments regularly reach 2-5x improvements over equivalent static batching configurations.

As we'll explore in the next chapter on Inference Serving, continuous batching is just one component of a complete serving system. Real deployments must also handle request routing, [load balancing](https://mbrenndoerfer.com/writing/scaling-ai-agents-performance-cost-optimization), model replication, and failure recovery, all while maintaining the low latency users expect.

Advertisement

## [Link to Summary](https://mbrenndoerfer.com/writing/continuous-batching\#summary) SummaryLink Copied

Continuous batching turns LLM inference from rigid, inefficient batch processing into a dynamic, high-utilization system. The main ideas we covered include:

- **Static batching inefficiency.** Traditional batching forces all requests to wait for the slowest, wasting compute on completed requests and inflating latencies. Efficiency can drop to 20% or below for workloads with high output length [variance](https://mbrenndoerfer.com/writing/descriptive-statistics-guide-python-data-analysis).
- **Iteration-level scheduling.** Treating each decode step as a scheduling opportunity allows immediate eviction of completed requests and admission of waiting ones. The batch composition changes continuously rather than at fixed batch boundaries.
- **Phase handling.** Managing the distinct prefill and decode phases, potentially with chunked prefill, balances compute characteristics across the batch and prevents long prompts from monopolizing the GPU.
- **Resource management.** Tight integration with [KV cache memory](https://mbrenndoerfer.com/writing/multi-query-attention-memory-efficient-inference) management, including [paged attention](https://mbrenndoerfer.com/writing/paged-attention-vllm-kv-cache-memory-management), maximizes the number of concurrent requests by returning memory to the pool immediately on completion.
- **Theoretical efficiency.**Continuous batching approaches 100% compute efficiency because total capacity used matches total useful work, independent of output length variance.
- **Throughput gains.** Real-world improvements of 2-5x throughput are common, with better latency characteristics especially for short requests.

Continuous batching is now standard practice for [production LLM](https://mbrenndoerfer.com/writing/inference-scaling-llm-deployment-optimization) serving. Combined with the memory optimizations from paged attention and the [speculative decoding](https://mbrenndoerfer.com/writing/speculative-decoding-accelerating-llm-inference) techniques we covered earlier, it lets serving LLMs at scale with reasonable hardware costs.

## [Link to Quiz](https://mbrenndoerfer.com/writing/continuous-batching\#quiz) QuizLink Copied

Ready to test your understanding? Take this quick quiz to [reinforce](https://mbrenndoerfer.com/writing/policy-gradient-methods-reinforce-algorithm) what you've learned about continuous batching.

### Continuous Batching Fundamentals

Question 1 of 80 of 8 completed

What is the primary inefficiency of static batching in LLM inference?

It requires too much GPU memory for large models

All requests must wait for the longest request to complete, wasting compute on finished requests

It cannot process more than one request at a time

It requires padding all sequences to maximum model length

Check Answer

Track your reading progress

Sign in to mark chapters as read and track your learning journey

Sign in →

## Comments

## Continue reading

[Back to Language AI Handbook](https://mbrenndoerfer.com/books/language-ai-handbook)

[Previous Chapter\\
\\
Speculative Decoding Math: Algorithms & Speedup Limits](https://mbrenndoerfer.com/writing/speculative-decoding-math-acceptance-criterion) [Next Chapter\\
\\
LLM Inference Serving: Architecture, Routing & Auto-Scaling](https://mbrenndoerfer.com/writing/llm-inference-serving-architecture-scaling-optimization)

## Reference

Citation details

Cite or share this article.

BIBTEXAcademic

Copy

@misc{brenndoerfer2026continuousbatching,
author = {Michael Brenndoerfer},
title = {Continuous Batching: Optimizing LLM Inference Throughput},
year = {2026},
url = {https://mbrenndoerfer.com/writing/continuous-batching},
organization = {mbrenndoerfer.com},
note = {Accessed: 2026-08-02}
}

Show other formats
APA · MLA · Chicago · Harvard · Simple

APAAcademic

Copy

Michael Brenndoerfer (2026). Continuous Batching: Optimizing LLM Inference Throughput. Retrieved from https://mbrenndoerfer.com/writing/continuous-batching

MLAAcademic

Copy

Michael Brenndoerfer. "Continuous Batching: Optimizing LLM Inference Throughput." 2026. Web. August 2, 2026. <https://mbrenndoerfer.com/writing/continuous-batching>.

CHICAGOAcademic

Copy

Michael Brenndoerfer. "Continuous Batching: Optimizing LLM Inference Throughput." Accessed August 2, 2026. https://mbrenndoerfer.com/writing/continuous-batching.

HARVARDAcademic

Copy

Michael Brenndoerfer (2026) 'Continuous Batching: Optimizing LLM Inference Throughput'. Available at: https://mbrenndoerfer.com/writing/continuous-batching (Accessed: August 2, 2026).

SimpleBasic

Copy

Michael Brenndoerfer (2026). Continuous Batching: Optimizing LLM Inference Throughput. https://mbrenndoerfer.com/writing/continuous-batching

DIRECT LINKURL

Copy

[https://mbrenndoerfer.com/writing/continuous-batching](https://mbrenndoerfer.com/writing/continuous-batching)

## About the author

![Michael Brenndoerfer](https://assets.mbrenndoerfer.com/_optimized/resume/michael_brenndoerfer-256w.webp)

Author

### Michael Brenndoerfer

Editorial note

All opinions expressed here are my own and do not reflect the views of my employer.

Michael currently works as an Associate Director at EQT Partners in Singapore, leading AI and data initiatives across private capital investments.

With a background spanning private equity, management consulting, and software engineering, he focuses on building practical analytics solutions and helping teams work more effectively with data. He has contributed research to AI conferences and enjoys exploring applications of machine learning and natural language processing.

Explore

[Resume](https://mbrenndoerfer.com/resume) [Publications](https://mbrenndoerfer.com/publications) [Books](https://mbrenndoerfer.com/books)

Connect

[Contact](https://mbrenndoerfer.com/contact) Newsletter

## Related content

[**LLM Inference Serving: Architecture, Routing & Auto-Scaling** \\
\\
Jan 19, 2026•74 min read\\
\\
Data, Analytics & AISoftware Engineering\\
\\
Master LLM inference serving architecture, token-aware load balancing, and auto-scaling. Optimize time-to-first-token and throughput for production systems.\\
\\
Read more](https://mbrenndoerfer.com/writing/llm-inference-serving-architecture-scaling-optimization) [**Speculative Decoding Math: Algorithms & Speedup Limits** \\
\\
Jan 17, 2026•53 min read\\
\\
Data, Analytics & AISoftware Engineering\\
\\
Learn the mathematical framework for speculative decoding, including the exact acceptance criterion, rejection sampling logic, and deriving optimal draft lengths.\\
\\
Read more](https://mbrenndoerfer.com/writing/speculative-decoding-math-acceptance-criterion) [**Speculative Decoding: Fast LLM Inference Without Quality Loss** \\
\\
Jan 16, 2026•55 min read\\
\\
Data, Analytics & AISoftware Engineering\\
\\
Accelerate LLM inference by 2-3x using speculative decoding. Learn how draft models and parallel verification overcome memory bottlenecks without quality loss.\\
\\
Read more](https://mbrenndoerfer.com/writing/speculative-decoding-accelerating-llm-inference) [**GGUF Format: Efficient Storage & Inference for Quantized LLMs** \\
\\
Jan 15, 2026•55 min read\\
\\
Data, Analytics & AIMachine Learning\\
\\
Discover GGUF format for storing quantized LLMs. Learn file structure, quantization types, llama.cpp integration, and deploying models on consumer hardware.\\
\\
Read more](https://mbrenndoerfer.com/writing/gguf-format-quantized-llm-storage-inference)

[All writing](https://mbrenndoerfer.com/writing)

Advertisement

Newsletter

## Stay up to date.

Get articles, book updates, and news delivered to your inbox.

Subscribe

No spam, unsubscribe anytime.

or

## Join the community.

Sign in to remove popups, track your reading progress, and join the discussion.

Join the community

[Join the community](https://mbrenndoerfer.com/community)

Aa