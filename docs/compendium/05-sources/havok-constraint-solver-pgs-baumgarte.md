---
source_url: https://www.havok.com/blog/how-havoks-constraint-solver-works-pgs-baumgarte/
title: Physics Constraint Solver Deep Dive: PGS & Baumgarte in Havok
ingested: 2026-08-03
avenue: Games
---

# Physics Constraint Solver Deep Dive: PGS & Baumgarte in Havok

03Apr 2026

April 3, 2026hkadmin

# How Havok’s Constraint Solver Works:   PGS, Baumgarte Stabilization, and Production-Scale Solving

## How a Robust Constraint Solver Impacts Your Game.

Action-focused games push constraint solving harder than almost any other simulation workload. Common scenarios that define these titles are the same ones that expose solver limitations:

- **High-impact ragdolls.** A character takes a shotgun blast at close range, gets launched by an explosion, or ragdolls off a rooftop onto a moving vehicle. Joints need to hold together through extreme forces without visible separation at shoulders, hips, or elbows – and recover cleanly within a frame or two. When the solver falls short, players see limbs stretching apart or snapping back unnaturally. In solver terms, this is the **joint separation under load** problem – the solver must correct constraint violations quickly without introducing instability.

- **Heavy objects on chains and constraint chains with wide mass ranges.** A wrecking ball swinging through a destructible building. A massive boss creature whose heavy torso drives a chain of lightweight tail segments. A grappling hook pulling a character toward a heavy anchor point. In each case, the solver must propagate impulses through a constraint chain where bodies differ dramatically in mass. When it cannot, the chain stretches visibly, the tail lags behind the body, or the grapple feels elastic instead of taut. This is the **mass ratio convergence** problem – one of the hardest challenges for iterative solvers, where more iterations alone cannot cost-effectively close the gap.

- **Mixed constraint stiffness in the same scene.** A horror game where a creaking door swings loosely on a worn hinge while, in the next room, a mechanical press slams down with rigid precision. A single global stiffness setting cannot serve both. The solver needs to support per-constraint tuning so that soft interactive objects and rigid mechanical systems coexist without compromise. This is a **constraint stiffness control** problem – the solver must expose fine-grained tuning surfaces without requiring global parameter changes that affect everything else.

- **Large-scale physics events.** A building collapses and spawns hundreds of constrained fragments. A battle scene has dozens of active ragdolls and vehicles simultaneously. The solver must scale across available cores and absorb sudden spikes in constraint count without requiring manual thread management or per-scene partitioning from the engineering team. This is a **CPU scaling** problem – parallelizing constraint solving without introducing race conditions or excessive synchronization overhead.

### Mixed constraint stiffness in the same scene.

These challenges are architectural – they depend on how the solver formulates constraints, how it corrects errors, and what tools it provides when iterative convergence alone is not enough. The rest of this post covers how Havok’s solver is built to handle each of them. To see how these solutions work within your own game, [reach out to evaluate Havok Physics](https://www.havok.com/evaluate/).

Havok Physics uses a **velocity-based iterative solver** built on sequential constraint processing, Jacobian-based formulation, and projected impulse clamping – an approach consistent with the **[Projected Gauss-Seidel (PGS)](https://www.scitepress.org/papers/2010/28307/28307.pdf)** family of methods well-known in simulation literature. Its positional error correction uses tau and damping parameters proportional to dt2 and dt respectively, an approach consistent with **[Baumgarte stabilization](https://www.sciencedirect.com/science/article/abs/pii/0045782572900187)**. These established techniques form the mathematical foundation of how Havok’s solver operates. This solver architecture is the foundation of constraint solving in both Havok SDK and Havok Physics for Unreal.

![Diagram showing the two main phases of a Havok Physics simulation step: collision detection generates contact and constraint Jacobians, which the solver then processes to produce corrective impulses.](https://www.havok.com/wp-content/uploads/2026/04/two-phases-physics-simulation-step.jpg)

The two main phases of a Havok Physics simulation step: collision detection generates contact and constraint Jacobians, which the solver then processes to produce corrective impulses.

## How Constraints are Represented in Havok Physics.

Every constraint in Havok Physics – whether it is a ragdoll shoulder, a door hinge, or a contact between a crate and the floor – must be converted into a form the solver can process. That form is a set of Jacobian rows, and understanding what they contain explains why the solver behaves the way it does under different conditions.

Two unconstrained rigid bodies have six degrees of relative freedom: three linear (they can move apart along X, Y, and Z) and three rotational (they can twist relative to each other around each axis). A constraint removes some of those freedoms. Each removed degree of freedom becomes one Jacobian row – a compact, linearized equation that tells the solver exactly how to enforce that restriction.

A Jacobian row encodes four things:

- **The direction of restriction.** Linear and angular components for each body that define which relative motion this row prevents. For a ball-and-socket joint, one row might encode “these two pivot points must not drift apart along the X axis.”
- **Effective mass.** A scalar derived from both bodies’ masses and inertia tensors that tells the solver how much impulse is needed to produce a unit velocity change along this row’s direction. This is where mass ratios directly affect solver behavior – when one body is dramatically heavier than the other, the effective mass for the row is dominated by the lighter body, which limits how much correction the solver can apply per iteration.
- **Error correction term**. The current positional violation along this direction, scaled by tau and damping (covered in the next section). This is the Baumgarte stabilization term that drives the solver to close position-level gaps, not just prevent new ones.
- **Impulse bounds.** The minimum and maximum impulse the solver is allowed to apply along this row. These bounds are what make the “projected” step in Projected Gauss-Seidel work – they enforce physical constraints on the solution. Contact rows have a lower bound of zero (they push bodies apart but never pull them together). Friction rows are bounded by the [Coulomb friction cone](https://pdf.sciencedirectassets.com/271977/1-s2.0-S0997753820X00052/1-s2.0-S0997753820304344/am.pdf?X-Amz-Security-Token=IQoJb3JpZ2luX2VjEGEaCXVzLWVhc3QtMSJHMEUCIQCQDwXQEQVagZ26%2FOtwalzjmJYLD2qrSC9ThGzDexsIbAIgARxUjpzm%2Bz8mPqWlrcNSN4Xh3Br795ih9ea17G3%2BGosqsgUIKhAFGgwwNTkwMDM1NDY4NjUiDOArMXpWdCedUgT%2B4SqPBTgKo9VBNZbZz6SW6zKtgPeLA8OQzFIm59U7VSnl3%2BLLyPaOy6rd9NAYWYD%2FNxeF1cl68A7MNJKDujuK3jv1uynKjB4C6IYlkyNLaEftr4UtH4ZeAmMZq3x%2BaQNYQEP3yJbl%2F1PC18ugWT9MBi8NXWrh9f22PIE9aXQUUlU9lzZ4o5GsVAKigafG%2FKJfLpkit0U1Xu0Ep5LtrcOfDB33PUHYGwp024stdIT8WeSWRqAtR9AyFwCCu2hendWzALq9T48TWjBsUAFZHyKcDyc1JAw2%2Fw%2BYRbO49WCkOiIIjDLqCFOL4yWoTs3JxPNzm1LGEU6UUfLiFsCD1qqtOw2qyZA8j%2BzrXqefhWLP38Jh5tYwFJoilmfQtHZ50%2FvL0wH5p1wFITw3LA7%2BMUmsgY2t%2BS8fHl0HzfM%2BcCMRe5TFShWRkdK%2F2TlGLiKb7my4nep594hdJo2GNtfoE3SoTM7RfWgJzYKWmQjXJ5O%2FGG%2Bwrf%2F7CXRGGGJE%2Bd5aAN%2FqW8EWXdFVCB5JMNRamFuOvoCf%2BUWPTlWzcHr735VJHR2uDs%2B77C1bbP3UYrRKBSIVjU285Z5PnsAsZsnqG%2BlEBU1ksmpDIL9tGXV1FIXkHAOE10PMVJMEXM4vTQdmFu2Vk9dRCyBDg2m4twooOt4PV31x8YRw%2BQWgOQEZiIlVffVTY7G10tTDgtgnNLXK53BvfvzRCo4EAiAj5PqnspNSPgK7a6OQkh9GtKn6IJkUQ10W1AUdGH15CROemvm9TQrZE3BHTPeytBfm2v%2BdSyoYzFI9c40svCUDf4TTTebWO%2FSICX%2BAij10dBcY3SmUQvqdiRfWxy3VxhXTqN4iHJzQW68ceQg3ltTLS%2FEtt5R53iKlJGUw8e7izgY6sQEA%2BrFHyFKz40A7It%2FSOhnVNwlgSFFf%2FZP7lGFH%2F2VDsEu3Iqzsoyk1kc3OM7MLznvABSFxjP3IE37L%2BGl3v0qO29%2FDFBBA%2Fz8HFx3YuY373HRMVJzp2gl5crrwIQesgRFCRc1id%2Bxtc1ttfEIHieaaOvEZYNx9%2BHYbTD3Af8ijS9bqdaMV4zfEDc5daybOBnnenaret6bYuFo6uPZ6gnWpagZVxLxr6pMeJm4VdOqr%2B1w%3D&X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Date=20260410T090104Z&X-Amz-SignedHeaders=host&X-Amz-Expires=300&X-Amz-Credential=ASIAQ3PHCVTYXFKHXB4C%2F20260410%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Signature=f5478647db55b71880f806b6491690c8f38e6807e2675d7d2123b1eb3a10047d&hash=6d10533949cd668545a562796e526283d534c4948d8b5f1d08065fd32c68ebbd&host=68042c943591013ac2b2430a89b270f6af2c76d8dfd086a07176afe7c76c2c61&pii=S0997753820304344&tid=pdf-6539c179-2352-4e79-8bc1-09acc79eed96&sid=68fdec9b184d5047c22b42383a1334d41372gxrqb&type=client) (maximum friction force proportional to the normal contact force). Motor rows have configurable force limits.

### From constraint types to Jacobian rows.

Different constraint types produce different numbers of Jacobian rows, corresponding to how many degrees of freedom they restrict:

|     |     |     |
| --- | --- | --- |
| **Constraint type** | **Degrees of freedom restricted** | **Jacobian rows** |
| Ball-and-socket | 3 linear (pivot points must coincide) | 3 |
| Hinge | 3 linear + 2 angular (rotation allowed on one axis only) | 5 |
| Limited hinge | 3 linear + 2 angular + angular limit rows | 5+ |
| Ragdoll | 3 linear + angular cone and twist limits | 5+ |
| Fixed | 3 linear + 3 angular (no relative movement) | 6 |
| Contact point | 1 normal + friction rows | 1-3 |

Internally, Havok builds these rows from **constraint atoms** – modular building blocks that each handle a specific type of restriction. A hinge constraint, for example, is composed of three atoms: one defines the constraint space and pivot points, a second restricts the linear position of the pivots (3 rows), and a third restricts angular movement to the hinge axis (2 rows). Adding angular limits or motors adds further atoms and further rows. This modular atom structure is how Havok achieves per-constraint tuning – tau and damping factors, stabilization methods, and solving modes are configured at the atom level, which means they apply to specific Jacobian rows rather than to the constraint as a whole.

### Why this representation matters for production

This Jacobian representation is computed automatically during collision detection for contacts and from constraint definitions for joints, then stored in solver data. The solver operates entirely in this linearized constraint space during iteration – it never needs to re-evaluate geometry or constraint definitions during the solve loop. Every constraint type, regardless of complexity, is reduced to the same uniform row format before solving begins.

This uniformity is what makes the solver’s iteration loop efficient and what enables the parallelization strategy described later in this post. It also means that the solver’s convergence characteristics – how iteration count, mass ratios, and constraint ordering affect stiffness – apply consistently across all constraint types. A ragdoll shoulder and a ground contact are processed by the same solving logic, differing only in their Jacobian row count, impulse bounds, and error correction terms.

![Diagram showing how a constraint restricts the relative movement between two bodies by defining pivot positions in each body's local space. Each restricted degree of freedom becomes a Jacobian row that the solver processes during iteration.](https://www.havok.com/wp-content/uploads/2026/04/constraint-restrictions-by-pivot-point.jpg)

A constraint restricts the relative movement between two bodies by defining pivot positions in each body's local space. Each restricted degree of freedom becomes a Jacobian row that the solver processes during iteration.

## Why iteration count is a tunable quality lever, not a fixed cost.

The PGS method solves constraint equations by processing them one at a time, in sequence. For each constraint row, the solver computes the impulse needed to satisfy that constraint, clamps it to the feasible range (the “projected” step that correctly handles contacts and limits), and applies the impulse change to both bodies’ velocities.

Because each constraint uses the most recent velocity state updated by all previous constraints, PGS converges faster than parallel Jacobi approaches for the same iteration count – a meaningful advantage when iteration budgets are tight.

One complete pass through all constraints constitutes a single solver iteration. More iterations improve convergence. Havok exposes the iteration count as a world-level parameter and supports dynamic adjustment based on timestep.

### Convergence depends on the constraint system.

- **Well-conditioned systems**(similar mass objects, few constraint chains) converge quickly.
- **Ill-conditioned systems**(large mass ratios, long chains like ragdolls or suspensions) converge slowly. The impulse a heavy body needs to transfer through lighter bodies is limited at each step by the lightest body’s effective mass.
- **Constraint ordering** Havok’s solver scheduling uses a priority system to ensure that high-priority constraints like ground contacts are solved last and “win out” over lower-priority internal constraints.

The practical implication: increasing global solver iterations improves stiffness but costs proportional CPU time across the entire scene. Havok’s constraint group system (covered below) provides a more targeted approach – additional iterations exactly where they matter, without increasing the global budget.

## How Havok Exposes Constraint Stiffness as a Tunable Parameter.

An iterative velocity solver does not directly correct positional errors. If two bodies are already penetrating, or a joint has separated slightly, the velocity solver alone prevents further violation but does not push bodies back into compliance. Over time, small errors accumulate.

Havok addresses this through positional error correction injected into the velocity constraint equations – an approach consistent with **Baumgarte stabilization** in simulation literature. The solver targets a corrective velocity proportional to the current positional error, gradually closing any position-level constraint violation.

Two parameters control this behavior:

**Tau** controls the fraction of positional error to correct per solver sub-step. Higher values correct faster; lower values correct more gradually. Tau is proportional to dt2, reflecting the relationship between position correction and acceleration.

**Damping** controls the fraction of velocity error to correct per solver sub-step. Lower values allow bodies to sink into each other more before corrective impulses fully engage. Damping is proportional to dt, reflecting the linear relationship between velocity correction and impulse.

Together, these parameters define the constraint feel:

| **Tau** | **Damping** | **Behavior** |
| --- | --- | --- |
| High (near 1.0) | High (near 1.0) | Stiff, aggressive error correction. Can cause jitter if iterations are insufficient. |
| Low | High | Slow positional recovery but strong velocity-level response. Bodies hold relative velocity well but drift slightly. |
| Low | Low | Soft, springy feel. Constraints allow significant play before corrective forces engage. |
| High | Low | Not recommended – fast positional correction with weak velocity damping causes oscillation and jitter. |

These values are configured globally via _hknpWorldCinfo_ and can be overridden per-constraint through tau and damping factor fields on individual constraint atoms. The per-atom factors scale the global values, so a soft door hinge and a stiff vehicle axle can coexist in the same scene at the same solver budget.

For studios evaluating solver flexibility, this means constraint feel can be tuned per-joint without rebuilding assets or changing global settings.

### Why Softening Improves Stability

An iterative solver with a fixed iteration budget cannot perfectly satisfy all constraints simultaneously. When tau and damping are high, each constraint aggressively corrects its own violation, but the correction for one constraint creates violations in neighbors. With insufficient iterations, this mutual interference causes visible jitter.

Softening via lower tau and damping spreads the correction across more steps. Each step applies a smaller impulse, so mutual interference is reduced. The result is smoother behavior at lower iteration counts, at the cost of some residual constraint error that is typically not visible.

When per-constraint atom scaling factors for tau and damping are both reduced significantly below 1, damping factors should remain higher than tau factors to avoid jittery behavior. This guidance applies specifically to the per-atom override multipliers rather than to the global solver values.

## Consistent feel across variable timesteps.

Because tau is proportional to dt2 and damping is proportional to dt, changing the simulation timestep changes the effective stiffness of every constraint. A solver tuned to feel tight at 60 Hz will feel loose at 30 Hz and overly stiff at 120 Hz.

Havok provides an adaptive solver settings option ( _m\_adjustSolverSettingsBasedOnTimestep_) that automatically adjusts tau, damping, and solver iterations on the fly to maintain consistent constraint stiffness across variable timesteps. The reference point is defined by _m\_expectedDeltaTime_, and the allowed iteration range can be bounded to keep CPU cost predictable.

For studios using fixed timesteps (recommended for determinism), this feature is not needed. For titles with variable frame rates or substepping, it maintains consistent simulation feel without manual parameter adjustment per frame.

### Stabilization Atoms: Eliminating Pivot Drift in Joint Constraints

Standard tau/damping error correction works well for contacts and simple constraints but can be insufficient for joint constraints under high stress. Ball-and-socket joints, hinges, limited hinges, and ragdoll constraints can exhibit visible pivot-point separation when bodies are subject to large forces or high angular velocities – the “rubber banding” effect visible at shoulders, hips, or elbows.

Havok provides **stabilization atoms** as an enhanced solving method for these cases. The stabilization atom ( _hknpSetupStabilizationAtom_) pre-computes parameters that improve error recovery for the ball-and-socket constraint atom when its solving method is set to stabilized mode. This provides better results for configurations where standard positional error correction under-corrects.

The stabilized solving method is available on all constraints that use the ball-and-socket atom – covering ball-and-socket, hinge, limited hinge, and ragdoll constraints. It is enabled per-constraint, so it can be applied selectively to the joints where pivot drift is a visual quality concern.

## Solving mass-ratio problems at author time, not runtime

Solver convergence is fundamentally limited by the condition number of the constraint system. The condition number worsens when constrained bodies have very different masses or inertia tensors – a thin, light forearm bone constrained to a massive torso body is a poorly conditioned system that the iterative solver struggles to resolve.

Havok provides utility functions that address this at the asset level, with no per-frame runtime cost:

- **Inertia optimization** adjusts body inertia tensors across a ragdoll skeleton so that each body’s inertia is within some factor of the summed inertias of its children. This makes the effective mass ratios across the constraint chain more uniform, directly improving solver convergence. The optimization makes inertias more “box-like” and helps balance the inertia hierarchy. Note that the optimization does not fully converge in a single call – each subsequent call will continue to refine the system inertia, so multiple passes can be applied at asset setup time if additional refinement is needed.
- **Mass optimization** redistributes mass between bodies in a ragdoll without changing the total system mass, ensuring stable mass ratios between constrained bodies.

The result: a constraint system that converges faster with fewer iterations, producing stiffer joints and less visible separation at the same solver budget. These are authoring-time fixes that pay off every frame without consuming any runtime CPU.

![Screenshot of the interface for the Stablize Inertias tool in the Ureal Engine through Havok Physics for Unreal.](https://www.havok.com/wp-content/uploads/2026/04/stabilize-inertia-tool.jpg)

The Stabilize Inertias tool in the Physics Asset Editor (Havok Physics for Unreal). This authoring-time tool produces inertia scaling factors across the full skeleton, improving solver convergence without additional runtime cost.

## Targeted iteration budgets with constraint groups

Not all constraints in a scene need the same solving fidelity. Ground contacts on a static floor converge easily. A digger with a heavy load in the bucket requires significantly more iterations.

Havok’s **constraint group** system allows studios to assign constraints to groups, each with an independent **micro-step multiplier**. Groups with a multiplier greater than 1 receive additional solver iterations each solver sub-step. The cost is proportional to the multiplier and isolated to the constraints in that group – the rest of the scene continues at the base iteration count.

This multiplier can be adjusted at runtime without penalty, enabling dynamic quality management. A ragdoll in the player’s immediate view could receive a higher multiplier than one at the edge of the screen. A vehicle being actively driven could receive more iterations than one parked in the background.

For studios evaluating solver cost management, this means the global iteration count can be set conservatively for the common case, with targeted overrides applied precisely where constraint quality demands it.

![Three cranes with wrecking balls showing the results of different solvers on the rigid body behavior. The first shows the results from the default solver, the second (16 micro steps) shows the iterative solver (explained below), the third shows results with the direct solver (explained below).](https://www.havok.com/wp-content/uploads/2026/04/wrecking-ball-different-solvers.jpg)

Three cranes with wrecking balls showing the results of different solvers on the rigid body behavior. The first shows the results from the default solver, the second (16 micro steps) shows the iterative solver (explained below), the third shows results with the direct solver (explained below).

### When iterative solving is not enough – and what to do about it

For constraint systems where iterative convergence is fundamentally too slow – particularly systems with large mass ratios or where exact constraint satisfaction is required – Havok provides a **direct solver** option.

The direct solver operates on a constraint group and solves all constraints in the group simultaneously rather than one at a time, eliminating the slow convergence problem that iterative solvers experience with large mass ratios. Where the iterative solver’s convergence is limited by the lightest body in a chain, the direct solver considers all constraints together and reduces errors at all joints simultaneously.

The direct solver is particularly effective for:

- **Long ragdoll chains** where impulse propagation through many lightweight bodies is slow under iterative solving.
- **Mechanical assemblies** (cranes, chains, articulated machinery) where visible joint separation is unacceptable.

An **accurate single constraint solver** is also available for individual constraints (doors, windows, individual hinges) that need exact solutions, at a modest additional cost over the iterative solver. This solver is designed for isolated constraints – for multi-constraint systems such as chains, ragdolls, or vehicles, the constraint group direct solver is the appropriate tool.

Important limitations: the direct solver operates on linearized constraint equations, so large rotations within a single timestep can still introduce errors – smaller timesteps are needed for high-rotation scenarios. Motor atoms are optionally supported but can cause jitter when motors frequently switch between limited and unlimited modes.

For certain use cases constraints can be spread across both the direct solver and iterative solver to achieve the desired results.

![Diagram comparison of iterative solving vs direct solving.](https://www.havok.com/wp-content/uploads/2026/04/iterative-solving-vs-direct-solving.jpg)

Iterative solving (left) vs direct solving (right) under large mass ratios. The iterative solver's convergence is limited by the lightest body in the chain, leaving persistent joint errors. The direct solver considers all constraints simultaneously, reducing errors at every joint in a single pass. The direct solver is enabled per constraint group at runtime, allowing studios to target it precisely where mass ratios demand it.

### Eliminating visible joint separation with post-projection (beta)

As a physics extension, Havok provides optional **positional constraint iterations** (post-projection) that directly move body transforms to reduce joint separation after the velocity solve. Unlike velocity-based solving, post-projection changes positions without introducing velocities, which can eliminate visible joint gaps without causing the instabilities that large corrective impulses would produce.

Post-projection is generally called after the solve phase so that joint errors are reduced before bodies are rendered, though it can also be invoked at other stages of the simulation.

A notable variant is **shock propagation**, which propagates positional corrections hierarchically through the constraint chain. Shock propagation can eliminate joint separations more effectively and in fewer iterations than standard post-projection.

The trade-off is that post-projection operates independently of the contact system. Because positional correction moves body transforms directly without re-evaluating contacts, it can place bodies into penetrating states. This is a fundamental design trade-off: joint accuracy is prioritized over contact response. Studios should apply post-projection selectively – typically on ragdolls or constraint chains where visible joint separation is the primary quality concern – and validate that the corrected configurations do not introduce unacceptable penetration

![Three ragdolls are shown with different implementations of shock propagation](https://www.havok.com/wp-content/uploads/2026/04/shock-propagation.jpg)

A high-velocity heavy box impacts a ragdoll's arm. Left: the default solver shows large joint separation. Center: shock propagation eliminates joint gaps in approximately 10 iterations. Right: standard post-projection significantly reduces separation with up to 20 iterations. These results are achievable through configuration alone - no custom solver code required

### How the solver scales across cores without manual thread management

Constraint solving has inherently sequential dependencies – solving one constraint updates body velocities that affect neighboring constraints. Naive parallelization causes race conditions or requires excessive synchronization.

Havok addresses this by partitioning bodies into spatial groups (cells), identifying constraint dependencies between those groups (links), and ordering constraints by priority (grids). Contact constraints are prioritized above joint constraints, and static-to-dynamic contacts receive the highest priority. Higher-priority groups are solved last, ensuring they “win out” over lower-priority constraints.

Each combination of link and priority level produces a solve task. Tasks that affect different groups of bodies can execute in parallel without data races, allowing the solver to scale automatically across available cores.

![A Colosseum constructed from separate rigid bodies is separated into different groups of cells for solving.](https://www.havok.com/wp-content/uploads/2026/04/spatial-cells.jpg)

Havok's Visual Debugger Cell Viewer: bodies are color-coded by spatial cell. Each cell's constraints can be solved independently of other cells, enabling parallel execution across threads. This visualization is available through Havok's Visual Debugger, providing direct insight into solver parallelization during profiling.

![A closer look at how the cells on the model are linked.](https://www.havok.com/wp-content/uploads/2026/04/spatial-cell-to-constraint-dependency.jpg)

Links between spatial cells represent constraint dependencies. Each link produces solve tasks that must be scheduled to avoid concurrent modification of the same bodies, while independent links execute in parallel.

The spatial partitioning system exploits temporal coherence to maintain balanced workloads – newly added bodies are assigned to the nearest existing group. To avoid sudden imbalances, bodies should be added incrementally across multiple simulation steps where possible.

Studios do not need to manually partition work or manage solver threading. The system scales automatically to available cores, which simplifies integration and reduces the engineering surface area during optimization passes.

### The solver configuration hierarchy

Havok’s solver settings form a layered system that allows studios to set sensible defaults and override them precisely where needed:

| **Level** | **Controls** | **When to use** |
| --- | --- | --- |
| **World** | Solver type preset, base solver iterations, tau, damping, timestep adjustment | Default behavior for the entire scene |
| **Constraint group** | Micro-step multiplier, direct solver enable | Subsystems needing higher fidelity (ragdolls, vehicles) |
| **Per-constraint** | Tau/damping factor overrides, stabilization method, accurate single solver | Individual joints needing specific softness or precision |

At the highest level, solver type presets ( _hknpWorldCinfo::setSolverType()_) configure substeps, tau, and damping to tested defaults. Studios can also set lower-level parameters directly for finer control.

This hierarchy means physics teams do not need to over-budget the global iteration count to satisfy the most demanding constraints. The base settings handle the common case efficiently, while targeted group and per-constraint overrides address specific quality requirements at proportional cost.

### Evaluating solver quality: a practical test matrix

The following scenarios form a practical test matrix for assessing solver quality during a Havok integration assessment. Each reveals specific solver characteristics and maps to concrete configuration levers.

| **Scenario** | **What it tests** | **Key configuration levers** | **Expected outcome** |
| --- | --- | --- | --- |
| Ragdoll pile (10+ ragdolls) | Solver convergence under high constraint density | Inertia optimization, constraint group multiplier, direct solver | Stable pile with minimal joint separation; ragdolls settle without jitter |
| Vehicle on terrain | Mass ratio handling, suspension chain convergence | Constraint group direct solver, micro-step multiplier | Tight suspension response; no visible separation between chassis and wheels |
| Destruction event (100+ fragments) | Solver scaling under sudden body count spike | Space splitter rebalancing, deactivation thresholds | Fragments resolve contacts within 1-2 frames; no visible CPU spike beyond one step |
| Chain/rope constraint (20+ links) | Impulse propagation through long chains | Direct solver, iteration count, tau/damping tuning | Chain hangs taut under gravity; no visible stretching or oscillation |
| Ragdoll shot with high-velocity impact | Constraint separation recovery, stability under extreme forces | Stabilization atoms, inertia optimization, angular damping | Joint gaps eliminated within 1-2 frames; no residual oscillation |

For each scenario, the tuning process follows a consistent pattern: start with default settings, identify the specific constraint quality issue (separation, jitter, softness), then apply the most targeted fix available – per-constraint stabilization before group multipliers, group multipliers before global iteration increases.

## Assessing Havok’s solver for your production

The solver architecture described in this post – PGS iteration, Baumgarte error correction, the direct solver, stabilization atoms, inertia optimization, and automatic multithreaded scheduling – represents a coherent system designed for production-scale constraint solving. Each mechanism addresses a specific class of problem, and the layered configuration hierarchy provides precise control without requiring global compromises.

The scenarios outlined in this document can be used as reference points for how to construct your games’ use case with Havok Physics. A comprehensive implementation strategy is fundamental to getting the best possible performance and behavior in your game. The Havok team can support this process with an evaluation license and direct engineering guidance. [Contact us to start an evaluation](https://www.havok.com/evaluate/)

# Frequently Asked Questions

What constraint solver does Havok Physics use?

Havok Physics uses a velocity-based iterative solver whose behavior is consistent with the Projected Gauss-Seidel (PGS) family of methods. Constraints are formulated as Jacobian rows during collision detection, and the solver iterates through them sequentially to find impulses that satisfy all constraints within a fixed iteration budget. Havok also offers an alternative direct constraint solver that can be used independently or in combination with the iterative solver.

What is Baumgarte stabilization in physics simulation?

Baumgarte stabilization is an established method for correcting positional constraint errors within a velocity-based solver. Instead of solving for zero constraint velocity, the solver targets a corrective velocity proportional to the current positional error. Havok’s solver uses an approach consistent with this technique, implemented through tau (position error correction rate) and damping (velocity error correction rate) parameters.

What do tau and damping control in Havok's solver?

Tau controls how aggressively positional constraint errors are corrected each solver step – higher values correct faster but may cause jitter. Damping controls how aggressively velocity-level constraint violations are corrected. Together, they determine constraint stiffness and softness. Both can be configured globally and overridden per-constraint, enabling different constraint feels in the same scene without global parameter changes.

How does Havok handle large mass ratio constraints?

Iterative solvers converge slowly when constrained bodies have very different masses. Havok provides multiple tools: inertia optimization to balance mass ratios across ragdoll skeletons at asset time with no runtime cost, constraint group micro-step multipliers for additional iterations on critical constraints, and a direct solver that solves all constraints in a group simultaneously for exact results.

What is the direct solver in Havok Physics?

The direct solver is an optional solving mode for constraint groups that solves all constraints simultaneously rather than iteratively, eliminating the slow convergence problem of iterative solving under large mass ratios. It is particularly effective for ragdoll chains, vehicle suspensions, and mechanical assemblies where constraint accuracy is critical. It is enabled per constraint group at runtime.

How does Havok multithread the constraint solver?

Havok partitions bodies into spatial groups, identifies constraint dependencies between groups, and orders constraints by priority. Tasks that affect independent groups run in parallel safely. The partitioning system exploits temporal coherence to maintain balanced workloads across available threads. Studios do not need to manage solver threading manually.

How do solver iterations affect performance and quality?

Each solver iteration processes all active constraints once. More iterations improve constraint stiffness and reduce visible errors (joint separation, penetration) but cost proportional CPU time. Havok allows targeted iteration increases through constraint groups so that critical subsystems get more iterations without increasing the global budget.

What are stabilization atoms in Havok constraints?

Stabilization atoms are an enhanced solving method for joint constraints (ball-and-socket, hinge, ragdoll) that provides better pivot-point error recovery than standard positional error correction. They pre-compute parameters used by the stabilized constraint atoms to improve error recovery for configurations where standard correction under-corrects. They are enabled per-constraint through configuration.

## Recommended Posts

### ![Havok Navigation Showcase demo with many AI agents navigating in a dynamically changing scene](https://www.havok.com/wp-content/uploads/2026/03/havok-navigation-showcase1.jpg)[Asynchronous Navigation Processing: Stable AI Pathfinding in Games](https://www.havok.com/blog/asynchronous-navigation-processing/ "Asynchronous Navigation Processing: Stable AI Pathfinding in Games")

### ![A large level filled with simulating particles](https://www.havok.com/wp-content/uploads/2025/12/havok-physics-for-unreal.jpg)[Havok Physics Particles Technical Overview (SDK)](https://www.havok.com/blog/asynchronous-navigation-processing/ "Asynchronous Navigation Processing: Stable AI Pathfinding in Games")

### ![](https://www.havok.com/wp-content/uploads/2026/01/havok-cloth-preview-tool-mouse-picking.jpg)[Introduction to Havok Cloth Preview Tool](https://www.havok.com/blog/asynchronous-navigation-processing/ "Asynchronous Navigation Processing: Stable AI Pathfinding in Games")

[See All Posts](https://www.havok.com/blog/ "Blog")

havok products

## Explore Havok’s Physics Offerings

### Havok Physics

Award-winning, battle-tested, and scalable, Havok Physics is the gold-standard for game developers when it comes real-time collision detection and dynamic simulation.

[Explore Havok Physics](https://www.havok.com/havok-physics/ "Havok Physics")

### Havok Physics for Unreal

Fully supported, battle-tested, and flexible to any game, Havok Physics for Unreal is the gold-standard for video game developers when it comes real-time collision detection and dynamic simulation.

[Discover Havok Physics for Unreal](https://www.havok.com/havok-physics-for-unreal/ "Discover Havok Physics for Unreal")

havok products

[![Havok logo](https://www.havok.com/wp-content/uploads/2024/07/havok-logo-footer-162x54-w-bg-2.png)](https://www.havok.com/)

- [Official Havok logos](https://www.havok.com/havok-logos/ "Havok logos")
- [Legal notices](https://www.havok.com/legal-notices/ "Legal notices")
- [Basics Privacy Principles](https://www.havok.com/basics-privacy-principles/ "Basics Privacy Principles")
- [Havok® Group Terms of Use](https://www.havok.com/terms-of-use/ "Terms of use")
- [Consumer Health Privacy](https://go.microsoft.com/fwlink/?linkid=2259814 "Consumer Health Privacy")
- [Microsoft Privacy Statement](https://www.microsoft.com/en-gb/privacy/privacystatement "Microsoft Privacy Statement")

###### Dublin

- One Microsoft Court

Leopardstown, Dublin, D18 P521

- +353.1.472.4300


###### San Francisco

- 1355 Market Street 3rd Floor

San Francisco, CA, 94103 USA

- +1.415.796.7401


###### Tokyo

- Shinagawa Grand Central

Tower 2-16-3, Konan

- Minato-ku, Tokyo, 108-0075, Japan

- +81.3.4535.8023


section-contact-footer