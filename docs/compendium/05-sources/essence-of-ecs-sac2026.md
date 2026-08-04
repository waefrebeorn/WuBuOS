---
source_url: https://boyang.cs.uwm.edu/publication/sac2026_ECS.pdf
title: The Essence of Entity Component System
ingested: 2026-08-03
avenue: Games
---

# The Essence of Entity Component System

The Essence of Entity Component System

Anisha Tasnim
University of Wisconsin-Milwaukee
Milwaukee, Wisconsin, USA
tasnim@uwm.edu

Abstract

Abstract
Modern game engines increasingly adopt the Entity Component
System (ECS) paradigm as a data-oriented alternative to traditional
object-oriented architecture. While ECS promotes modularity and
performance through the separation of data and behavior, its practical efficiency depends heavily on the underlying data layout. Despite widespread adoption in frameworks, such as Unity DOTS,
Bevy, and Flecs, the semantics of the archetype ECS remain informal and implementation-dependent, limiting rigorous reasoning
about determinism, system scheduling, and structural mutations.
This work formalizes and experimentally evaluates the archetype

about determinism, system scheduling, and structural mutations.
This work formalizes and experimentally evaluates the archetype
ECS. The formal model captures entity creation, component composition, system execution, and archetype migration as compositional state transitions, establishing the core invariants of archetype
organization. Using aTower Defensesimulation, we compare the
archetype ECS with alternative designs under identical conditions.
Results show that the archetype ECS achieves higher frame rate
and better frame stability than alternative designs, due to improved
cache efficiency and consistent entity access. By uniting formal
semantics with empirical validation, this study shows that the
archetype ECS outperforms traditional architectures and provides
a solid foundation for reasoning about correctness and parallelism.

CCS Concepts

Entity Component System, Semantics, Type System, Simulation,
Computer game

∗
Tian Zhao
University of Wisconsin-Milwaukee
Milwaukee, Wisconsin, USA
tzhao@uwm.edu

Keywords

1 Introduction

Anisha Tasnim and Tian Zhao. 2026. The Essence of Entity Component
System. InThe 41st ACM/SIGAPP Symposium on Applied Computing (SAC
’26), March 23–27, 2026, Thessaloniki, Greece.ACM, New York, NY, USA,
10 pages. https://doi.org/10.1145/3748522.3779910

simultaneously. Each frame must process every active entity’s position, health, and state within milliseconds to maintain 60 frames
per second (FPS) rendering speed. The way these entities are represented and accessed in memory critically affects performance.
Historically, most game engines have used Object-Oriented Pro-

This work is licensed under a Creative Commons Attribution 4.0 International License.
SAC ’26, Thessaloniki, Greece
©2026 Copyright held by the owner/author(s).

Historically, most game engines have used Object-Oriented Programming (OOP), where each object (e.g., Tower, Enemy, Bullet)
encapsulates its data and methods within hierarchical class structures. While intuitive, this design becomes inefficient as the number
of entities increases. Each object instance occupies a scattered region of memory, leading to poor cache utilization when iterating
over thousands of objects during game-play. For example, updating
all enemy positions requires dereferencing each object individually,
a process that repeatedly jumps across memory, resulting in cache
misses and reduced throughput [5, 6]. As game complexity and
hardware parallelism increase, these unpredictable access patterns
become a fundamental bottleneck.
To address these limitations, the Entity Component System (ECS)

To address these limitations, the Entity Component System (ECS)
architecture emerged, representing a shift toward Data-Oriented
Design (DOD). In ECS, entities are simple identifiers, components
are plain data structures (e.g., Position, Velocity, Health), and systems define the logic that operates over sets of components. For
instance, inTower Defense, a movement system updates all entities that have both Position and Velocity components, and
a collision system processes Health components. This approach
decouples data from behavior, allowing systems to operate over
contiguous blocks of homogeneous data and enabling efficient parallelization [7, 13].
However, the performance of ECS frameworks is deeply tied

However, the performance of ECS frameworks is deeply tied
to their internal data layout. Early ECS implementations used an
Array-of-Structs (AoS) layout, where each entity’s components are
grouped together in memory. This still leads to inefficient cache
utilization during system-level iteration, such as systems only need
one or two components from each entity. Struct-of-Arrays (SoA)
layout improves this by storing each component type in its own
contiguous array, enabling vectorized operations across entities
and more predictable access patterns [1, 2].
Modern archetype-based ECS framework uses the SoA principle

Modern archetype-based ECS framework uses the SoA principle
by grouping entities with identical component sets into archetypes,
dense columnar tables where each column represents a component type and each row represents an entity. This design minimizes
pointer chasing, enables constant-time component access, and leverages CPU cache lines efficiently. Frameworks such as Unity DOTS,
Bevy, and Flecs adopt this design to achieve significant performance improvements in large-scale simulations [1, 9]. Moreover,
recent work has extended ECS principles to high-performance domains. Madrona [14] used GPU to accelerate ECS for reinforcement
learning environments, while Vico [8] used ECS for co-simulation
across distributed systems, which demonstrated ECS’s generality
as a concurrent computation model.

©2026 Copyright held by the owner/author(s).
ACM ISBN 979-8-4007-2294-3/2026/03
https://doi.org/10.1145/3748522.3779910

---

Despite these advances, the semantics of archetype ECS frameworks remain informal, often described operationally without formal grounding. This lack of formalism makes it difficult to reason
about determinism, system scheduling, and structural mutations.
As ECS designs increasingly influence simulation engines and parallel runtime systems, establishing a formal semantics for archetype
ECS becomes crucial.
In this research, we make the following contributions:

In this research, we make the following contributions:

(1) An operational semantics is defined to capture the core mechanisms of archetype ECS, such as entity creation, component
association, system execution, and archetype migration in
a compositional and deterministic manner. This semantics
establishes the basic invariants of archetype-based data organization, showing that ECS execution can be described as
a series of stable state transitions.
(2) A type system is defined for archetype ECS to ensure that

(2) A type system is defined for archetype ECS to ensure that
a well-typed ECS program will not have unsafe access to
archetype storage during runtime execution.
(3) An archetype SoA framework is implemented in Scala and

In the rest of the paper, we discuss related works in Section 2 and
present a motivational example in Section 3. In Section 4, we give
an operational semantics to model the evaluation of ECS programs.
Section 5 defines a type system for ECS programs to prevent runtime
errors due to unsafe access to archetype storage. Section 6 describes
an implementation of theTower Defensesimulation in ECS. Section 7
compares the performance ofTower Defenseimplemented in OOP,
AoS, and archetype SoA, which shows that SoA has better gameplay performance than the other two designs.

2 Related Work

Early game engines predominantly relied on object-oriented designs, where data and behavior were encapsulated within rigid
inheritance hierarchies. These architectures encouraged modular
encapsulation but scaled poorly as game logic became increasingly
complex. The deep class hierarchies and runtime polymorphism typical of object-oriented designs produced non-deterministic memory
access and poor cache utilization, leading to inefficiencies in highly
parallel workloads [5, 6]. As hardware concurrency increased, these
limitations prompted researchers to explore data-oriented designs
that treat computation as transformations on structured memory
layouts rather than class abstractions.
ECS emerged as a response to scalability challenges. ECS decom-

While ECS successfully decouples data and logic, the efficiency
of its implementation depends primarily on the internal data layout
used to store components. Early AoS models retained per-entity
memory organization, where all components belonging to a single
entity were stored contiguously. Although this design simplified
access, it resulted in fragmented iteration patterns during system
updates. In contrast, in SoA layouts, each component type is stored
in a contiguous array. This improves spatial and temporal locality
and enables Single Instruction Multiple Data (SIMD) vectorization,
which accelerates iteration across large datasets [2].
Modern archetype ECS architectures build upon the SoA model

Modern archetype ECS architectures build upon the SoA model
by grouping entities with identical component sets into archetypes.
Each archetype is stored as a columnar table, where columns represent component types and rows represent entity instances. This
organization eliminates pointer chasing and supports constanttime component lookup through column indexing [2]. Studies have
demonstrated that this design achieves significant gains in update
throughput and cache coherence compared to sparse-set ECS implementations [4].
Several widely adopted frameworks embody these principles.

plementations [4].
Several widely adopted frameworks embody these principles.
Unity’s DOTS architecture introduced chunk-based archetype storage with Burst-compiled jobs to mitigate cache inefficiency and
main-thread contention [15]. Bevy ECS, implemented in Rust, integrates archetype tables with compile-time type safety through the
Rust ownership model, achieving memory-safe, cache-optimized
traversal [1]. Similarly, Flecs [9, 10], a C-based archetype ECS, organizes entities into dense tables and defers world modifications
through command queues, ensuring thread-safe yet deterministic
execution in parallel environments. These frameworks illustrate
how archetype ECS designs maximize spatial locality and enable
predictable and high-throughput updates suitable for large-scale
simulations such as Tower Defense [11].
The evolution of ECS has also expanded beyond game engines

The evolution of ECS has also expanded beyond game engines
into broader high-performance and concurrent simulation contexts. Vico [8] applied ECS to distributed co-simulation, showing
improved scalability through composition and fine-grained task isolation. Madrona [14] demonstrated a GPU-accelerated ECS runtime
capable of executing large batched reinforcement learning environments as unified “megakernels", achieving orders-of-magnitude
speedups over CPU baselines. Coxet al.[4] benchmarked archetype
and sparse-set ECS designs using John Conway’sGame of Life[3],
showing that archetype implementations nearly doubled iteration
throughput at high entity counts, while sparse sets excelled in
dynamic environments due to lighter update overhead. Fedoseev
et al.[5] compared Unity’s object-oriented and DOD-based ECS
prototypes, observing improved frame stability and reduced CPU
load in the DOD version. These findings suggest that ECS designs
and archetype-based SoA layouts in particular can achieve superior
frame consistency and computational throughput in simulationheavy workloads.
Despite extensive engineering refinement, the formal semantics

heavy workloads.
Despite extensive engineering refinement, the formal semantics
of archetype ECS remain underdeveloped. Most existing frameworks describe their execution behavior operationally, relying on
implementation heuristics rather than rigorous formalism. As ECS
systems increasingly influence simulation engines, AI frameworks,
and parallel schedulers, a formal model is necessary to reason about determinism, component migration, and synchronization invariants. Empirical comparisons reinforce the efficiency of archetypebased ECS systems. The absence of such semantics limits theoretical
analysis and compiler-level optimization, motivating a structured,
language-theoretic definition of ECS execution.
The most closely related work is Core ECS [12], which is a for-

The most closely related work is Core ECS [12], which is a formalism that captures the semantics of ECS system focusing on
concurrency and deterministic scheduling. It showed how safe
schedule can be constructed and that schedule safety implies schedule determinism. In contrast, we model the archetype-based ECS
architecture and provide an operational semantics along with type
system that exposes precise read-write sets for every system. This
enables us to reason statically about conflict freedom and system
compatibility. While Core ECS focuses primarily on scheduling and
deterministic concurrency, our approach provides a type-directed
mechanism for detecting structural conflicts at compile time that
is not expressible in Core ECS. Furthermore, our model directly
reflects the concrete memory layout and archetype transitions used
in real-world ECS engines.

3 Examples

This section introduces an example that illustrates the challenges
addressed by our formalism. In particular, it demonstrates how
ECS systems interact through shared archetypes, how structural
mutations must be deferred to preserve iteration stability, and how
read-write overlaps create the conflicts formalized in Section 4.

Figure 1: Illustration of an ECS game loop, where queued
events are synchronized to archetype table.

InTower Defense, the efficiency of the game loop depends on how
entities and their data are stored in memory. Figure 1 illustrates
a part of the per-frame execution workflow of the ECS, which
runs a BulletSystem and a CollisionSystem. The systems iterate
over stable archetype tables, generate structural events, and defer
these mutations until a subsequent synchronization step. The figure
shows three main properties of archetype ECS: (1) stable iteration
sets, ensuring that systems observe a consistent snapshot of entity
data; (2) event staging, which defers all structural modifications
until after system execution; (3) all deferred mutations synchronized
in an ordered batch to reestablish archetype consistency. These
mechanisms illustrate the interaction between entity iteration and
structural mutation that our semantics captures through conflict
modeling and tracking of archetypes dirtied by deferred mutations.
Each ECS system operates independently over entities with

processed in a later synchronization (world.sync()) phase. This
decoupling between event generation and structural mutation is
essential for maintaining stable SoA memory layout and preventing
mid-iteration inconsistencies.

Listing 1: Bullet system

1
2classBulletSystem(mapWidth=200f, mapHeight=200f)extendsSystems {
3overridevalqueryComponents=Set(classOf[Bullet])
45
overridedefupdate(world:World, dt:Float):Unit={
6valview=world.queryRows(queryComponents)
7
8view.foreach { r=>
9valb=r.table.getAt[Bullet](r.row, classOf[Bullet])
10
11b.x +=b.dx * b.speed * dt
12b.y +=b.dy * b.speed * dt
13b.ttl -=dt
14
15//Bounds&lifetime
16valoutOfBounds=
17b.x < 0f || b.y < 0f || b.x > mapWidth || b.y > mapHeight
18
19if(b.ttl <=0f || outOfBounds)
20world.enqueueEvent(Destroy(r.entity))
21}
22}
23}

23}
BulletSystem and CollisionSystem illustrate how our semantics handles system interactions and detects conflicts. The bullet archetype contains entities with the components Bullet and
Position and the enemy archetype contains entities with the components Position, Health, and Speed. BulletSystem iterates over
the bullet archetype, reading each bullet’s position and time-to-live,
updating its state, and staging Destroy(bullet) events when a
bullet expires or leaves the map bounds. CollisionSystem reads
the enemy archetype to identify alive enemies, and reads the bullet
archetype to test proximity between each bullet and its targeted
enemy. When a hit is detected, it writes to the enemy archetype by
decrementing enemy health and stages a Destroy(bullet) event.

Figure 2: Illustration of conflicts during the execution of
BulletSystemandCollisionSystem.

---

As illustrated in Figure 2, though the two systems do not iterate
the same entities, their access patterns over the bullet archetype
produce conflicts. For example, BulletSystem writes to the bullet
archetype, while CollisionSystem reads from it, creating aconcurrent conflict. The systems also stage Destroy(bullet) events,
which leave the bullet archetype in a temporary dirty state until
a synchronization step commits the structural updates, creating
aresidual conflict. To prevent concurrent conflict, BulletSystem
and CollisionSystem cannot run in parallel. To prevent residual
conflict, Destroy(bullet) events must be synchronized before
running systems that access the bullet archetype.

4 Semantics

In this section, we model the behavior of ECS programs via an
operational semantics, which is a formal model describing how
each “step" in the system modifies stored data or interacts with
entities and components. The semantics model terms, systems, and
game state. By specifying precise semantic rules, we clarify how
systems iterate over entities, change state, and maintain the ECS
data reliably. Runtime errors includeconflictscaused by unsafe
access to archetypes in systems. A type system is defined for terms,
systems, and game state so that a well-typed ECS program will not
have conflicts and other runtime errors.

4.1 Syntax

Our ECS model adopts the syntax in Figure 3 to formally describe
entities, components, and systems. Primitive types % represent the
basic data types available in the ECS, such as Int, Float, and Boolean.
Entities are represented by unique integers. Component types 𝐶

Entities are represented by unique integers. Component types 𝐶
define data objects, each encapsulating multiple fields of primitive
types %8. Components consist of primitive values E8 corresponding
to their declared types. An archetype is a set of component types.
A system (𝐴,_G.C) includes an archetype 𝐴 and a function that

v_{i}

to their declared types. An archetype is a set of component types.
A system (𝐴,_G.C) includes an archetype 𝐴 and a function that
processes all entities of 𝐴. In _G.C, G is the variable for entities and
C is the operation performed on the entities and their components.
Although a system can have multiple archetypes, this syntax only
have one for simplicity. The systems can be sequential, data parallel,
or task parallel. A sequential system will run in a single thread. A
data parallel system can run in multiple threads by splitting the
entities of the system among the threads. The task parallel system
represents two or more systems running in parallel.
ECS libraries like Flecs [9] use query mechanism to retrieve

P_{i}

ECS libraries like Flecs [9] use query mechanism to retrieve
archetypes that satisfy some query conditions. We do not model
query mechanism since the queries of task-parallel systems may
return the same archetypes at runtime and safe treatment of queries
complicates the formalism.
Term C in Figure 4 defines computational expressions within

complicates the formalism.
Term C in Figure 4 defines computational expressions within
our ECS model. The terms include values, variables, conditionals,
sequence, read and write entity components, and events. For simplicity, we omit computations such as function calls, local variables,
and assignments. Event 4EC is the entity’s life-cycle operations. An
entity can be created or destroyed; a component can be added to or
removed from an entity. The events may be staged (;I4EC) or immediate (8<4EC). The immediate events can only be safely evaluated
in a single-threaded system. Thus, most events are staged, which
are placed in the event queue until they can be safely processed.

%∈%A8<8C8E4=𝐼=C|𝐹;>0C|...
𝐶∈𝐶)~?4={% ,...,%1 =}Component type
𝐴∈𝐴)~?4={𝐶1,...,𝐶=}Archetype
4∈𝐸=C8C~==Entity ID
2∈𝐶><?>=4=C={E1,...,E=}Component value
B∈(~BC4<=(𝐴,_G.C)Sequential system
|par(𝐴,_G.C)Data parallel system
| [B1,B2]Task paralllel system

\left\{v\_{1},\ldots,v_{n}\right\}

Figure 3: The syntax of entities, components, and systems.

\ {\mathcal{H}}[e][C]

Figure 4: The syntax for terms, values, and events.

In an archetype ECS model, an entity is grouped with others sharing
the same archetype, allowing for efficient processing. Archetype
stores similar entities in table-like storage, where each row is dedicated to an entity and component values are stored in columns. For
convenience, we use H to represent the heap storage for archetypes,
where H[𝐴] returns the entities of an archetype 𝐴 and H[4][𝐶]
returns the component𝐶of entity4.

4.3 Game State and Frames

{\begin{array}{l c l l l}{{\mathcal{H}}}&{\in}&{H e a p}&{{\stackrel{d e f}{=}}}&{A T y p e\to\{E n t i t y\}}\\ &&}&{\cup}&{E n t i t y\to(C t y p e\to C o m p o n e n t)}\end{array}}

---

The Essence of ECS

\begin{array}{r l r l r l r l r l}{w}&{{}}{{}in}&{{&{}t s a t e}}&{{}}&{={}}&{(\ \ {mathcal H H},\ f r,\ q)}&{{}}&{{\ \ \ {\mathrm{G a m e~s t a t e}}}}\end{array}

F∈(C0C4=(H, 5A, @)Game state
5A∈𝐹A0<4=[B~=2]sync before next frame
|B::5Arun a systemB
|B~=2::5Aexecute staged events
@∈&D4D4=[4EC1,...,4EC=]Event queue

4.4 Conflicts

ECS supports data parallelism (a system runs in parallel threads
where each thread operates on a separate set of entities) and task
parallelism (multiple systems run in parallel). Since a system reads
and/or writes the entities of archetypes or modify the archetypes
(events), ECS must ensure that there is no conflicts between the
threads that can lead to concurrency errors.
The events cannot be executed in parallel since they modify

In practice, not all events can be staged. For example, in the
CollisionSystem, a bullet that collided with an enemy should be
destroyed immediately so that the same bullet cannot hit multiple
enemies. In this case, it is not safe run CollisionSystem in parallel
since race condition will occur due to concurrent modification to
bullet archetype table. Two or more systems can run in parallel if
they do not have read/write access to the same archetype tables.
The staged events must be applied before running a system that

The staged events must be applied before running a system that
depends on the archetypes modified by the events. For example, if
a bullet entity is destroyed in CollisionSystem but the render system runs before the destruction event is applied, then the destroyed
bullet will be rendered.
In summary, there are two types of conflicts.

In summary, there are two types of conflicts.

Concurrent Conflict.This occurs when the systems running in
parallel threads have concurrent access to the same archetype tables (read/write the same entities or add/remove/modify entities).
To avoid this, systems that have conflicting access to the same
archetype tables cannot run in parallel. Also, a system with immediate events cannot run in parallel.
Residual Conflict.This occurs when a system uses an archetype

\mathcal{H},e v t\to\mathcal{H}^{\prime}

\mathcal{H}^{\prime}

\begin{aligned}{}&{{}\underbrace{\mathrm}{{~i~s~a r~s e e t~n n e r e~}~\mathcal{H}_{1}=\mathcal{H}\left[\ \ \ \ C mapstoleftrightarrow\ \ \right]}_{\mathcal{H}_{1}=\mathcal{H}\left[\ epsilon\\ \\]\right\ \\\ \\\ \\\ \\\ \\\ \\\ \\\ \\\ \\\ \\\ \\\ \\\ \\\ \

A new entity is created with a component type and its initial
value. When an entity is destroyed, it is removed from the archetype
table where it was stored. Adding or removing a component from
an entity changes the archetype of the entity. Thus, this entity is
first removed from the old archetype table and then added to the
new table. The component value of the entity is also updated.

4.6 Term Evaluation

\mathcal{H},t,q\rightarrow\mathcal{H}^{\prime},t^{\prime},q^{\prime}

4.6 Term Evaluation
Figure 6 shows the small-step evaluation rules for terms, where
′ ′ ′ ′
H,C,@→H ,C ,@ reduces term C to C with possibly new storage
and queue. Most rules are standard including reading/writing components of an entity. A staged event;I 4EC (;I is short for lazy) is
added to the event queue. An immediate event 8<4EC is evaluated
to a value resulting a new archetype storage.

4.7 System Evaluation

Figure 7 shows the evaluation rules for systems, which can be
sequential, data parallel, or system parallel. A system (𝐴,_G.C) is
evaluated by calling _G.C with each entity of the archetype 𝐴. Rule
(S-Iter) describes how _G.C iterates over entity list 4B, denoted by
∗
(_G.C)@4B. The notation→ means transitive closure of→.

\rightarrow^{*}

Sequential system.A system B=(𝐴,_G.C) runs over the entities
4B of 𝐴. In the sequential mode, it simply iterates over each entity
in4B, applyingC, and possibly staging events.

---

F-Sync

Figure 6: The rules for term reduction.

\begin{array}{r l r}{\underset\}{

$query(A, \lambda x.t) = \{A\} \quad query(\mathrm{par}\ s) = query(s)$

$query([s_1,s_2]) = query(s_1) \cup query(s_2)$

$\forall A. \mathcal{H}'[A] = \mathcal{H}[A] = \mathcal{H}_1[A] = \mathcal{H}_2[A]$

$\forall e, i.\text{if } \mathcal{H}_i[e] \neq \mathcal{H}[e]\text{ then } \mathcal{H}'[e] = \mathcal{H}_i[e]\text{ else } \mathcal{H}'[e] = \mathcal{H}[e]$

$$\frac{\text{merge}(\mathcal{H}, \mathcal{H}_1, \mathcal{H}_2) = \mathcal{H}'}{\frac{A \notin atype(\mathcal{H}, q)}{\text{no\_conflict}(\mathcal{H}, (A, \lambda x.t), q)}}$$

$$\frac{\text{no\_conflict}(\mathcal{H}, s

S-Seq

\frac{A\notin a t y p e(\mathcal{H},q)}{n o\_c o n f l i c t(\mathcal{H},\;(A,\lambda x.t),\,q)}

Figure 8: Auxiliary functions.

that task parallel systems have different archetypes. For simplicity,
Rule (S-Task-P) only shows two systems.

4.8 Frame Reduction
A game state is a triple of archetype heap H, a frame 5A, and an

A game state is a triple of archetype heap H, a frame 5A, and an
event queue @. A frame is a list of systems and sync operations.
The transition of a state is the execution of the system or sync
on top of the frame, which is defined in Figure 7. The predicate
=>_2>=5;82C(H,B,@) ensures that the entities of the archetype accessed byBis not dirtied by events in@.
The sync operation applies all queued events in order and then

The sync operation applies all queued events in order and then
clears the event queue. This design is chosen for simplicity. In practice, an ECS library can automatically execute events if they are in
conflict with the next system in the frame. When a frame completes,
the game loop can restart with the same frame or dynamically reconfigure the frame with new systems.

\begin{array}{r c l l}{\tau}&{=}&{P}&{\mathrm{P r i m i t i v e\:t y p e}}\end{array}

---

{\frac{C\in\Gamma(x)}{\Gamma\vdash;x:C\not\&\ emptyset}}qquad{\frac{C\in\Gamma(x)\quad\Gamma\vdash t:C\not\&W}{\Gamma\vdash x.C\not=&W}{\quad\mathrm{~.~C~C o M P}}}

\frac{\Gamma\vdash e v t:()\And\!W}{\Gamma\vdash l z\;e v t:()\And\!W}\qquad\frac{\Gamma\vdash e v t:()\And W}{\Gamma\vdash i m\,e v t:()\And\emptyset}\qquad\mathrm{T E v E N T}

Figure 9: Term typing rules.

\begin{array}{r l r}&}{{\GammaGamma\vdashc r e a t e(C,c):(\ll\xi\}\ {array l}&{\Gamma{\mathrm{{C C C A E T E}}}}\\ &{\frac{\Gamma\vdash t:\tau\ \ W W}{\Gamma\vdash d e s t r o y:t())\&\ W\cup\{\tau\}}}\\ {{\frac{\Gamma+t:\tau\ \xi W}{\Gamma\vdash d r t t:(\xi)\&\ \tau^{\prime}=\tau\cup\{C\}}}}&{\ \ \ {\mathrm{T A D D}}}\\ {{\frac{\Gamma+t:\tau\ \xi\ W\ \ C\in\tau\ \{\tau\}\\Psi^{\prime}=\tau\cup\{C tau\}}{}}}&{\ \ \ \ \ {\mathrm{T A D D}}}\\ {{\Gamma\vdash t:\tau\not\sim\ W\ \{\tau\}\cap\{\tau\}\in\{\tau\}\cup\{\tau\}\cup\{\tau\}^{\prime}=\ \ \{C\}}}&{\ {\mathrm{T R M M V E}}}\end{array}

\begin{array}{r l r}&{\underbrace{x\ x A:A\:t\:\tau\:Phi W\ t t

of archetypes that may be written by staged events in C. The typing
rules in Figure 9 and 10 collect the archetypes of the staged events
and put them in the write set,. The archetypes of the immediate
events are not tracked since they are evaluated sequentially.
The type judgment for systems has the form ⊢B : (',,), which

produce staged events that can change archetypes in,. Rule (T-
Seq) checks the type of a sequential system, where the parameter
G has the type 𝐴 since the entities passed to G are in the archetype
𝐴. Mixing immediate and staged events in the same system can be
problematic since immediate events may change the heap structure
that the staged events depend on. Thus, for simplicity, we require
sequential system to have no staged events while data/task parallel
systems to have no immediate events.
The type rules for frames in Figure 11 tracks the set of dirtied

The type rules for frames in Figure 11 tracks the set of dirtied
archetypes D. Rule (T-System) ensures that the archetypes in '
are not in D and combines the write set, with D in checking the
rest of the frame. Rule (T-Sync) clearsDfor systems after a sync.

\begin{array}{r l r}{

5.1 Properties

Figure 12: Type rules for runtime values.

In this section, we state the progress and type preservation lemmas to show that a well-typed ECS program will not get stuck. The
proof, omitted here, uses the typing rules of runtime values in Figure 12. Rule (T-Frame) says that a game state H,5A,@ is well-typed
if H is well-typed and the frame 5A is well-typed with respect to
the archetypes dirtied by @. Rules (T-Heap) and (T-Entity) check
that every entity of every archetype is well-typed.
′ ′
Lemma 1 (Progress).If ⊢H,5A,@, then there exists H , 5A ≠5A,

′ ′
Lemma 1 (Progress).If ⊢H,5A,@, then there exists H , 5A ≠5A,
′ ′ ′ ′
and@ such thatH,5A,@→H ,5A ,@ .

′
The interesting part of the proof is that if 5A=B :: 5A, then for
B to reduce, it cannot access the archetypes dirtied in @. This can be
shown from the typing rules for frame and system.
′ ′ ′
Lemma 2 (Preservation).If ⊢H,5A,@ and H,5A,@→H,5A,@,

′

[... middle omitted — see footer ...]

into the same wave, and executed concurrently using the shared
worker pool managed by the Scheduler. Systems that perform structural updates are placed in their own serialized waves to ensure a
consistent view of archetype state.
The scheduler executes waves sequentially. After running all sys-

consistent view of archetype state.
The scheduler executes waves sequentially. After running all systems in a wave in parallel, it immediately invokes sync() to apply
the staged events. Each wave boundary serves as a synchronization
point for both execution and structural consistency, ensuring that
subsequent waves observe a fully updated world state. This design
provides deterministic ordering, eliminates residual conflicts across
waves, and achieves scalable parallelism for system-level tasks.

For instance, the MovementSystem updates entity positions by
applying a uniform computation over all entities with position and
velocity. Using ParFor.parFor, this loop is partitioned into chunks
that execute concurrently on all available processor cores. The
chunk size is dynamically chosen based on the number of hardware
threads and a configurable granularity parameter, balancing load
distribution and scheduling overhead. This model yields near-linear
speedup for data-oriented systems dominated by arithmetic or
memory-bound operations.
In summary, SoA-PAR supports two-level parallelism:

In summary, SoA-PAR supports two-level parallelism:

---

(1) Task-level parallelism between systems whose data dependencies permit concurrent execution.
(2) Data-level parallelism within each system across indepen-

(2) Data-level parallelism within each system across independent entity records.

By nesting these two forms of concurrency, the runtime maximizes
CPU utilization on multi-core architectures while preserving deterministic execution.

7 Performance Comparison

We compared OOP, AoS, archetype SoA, archetype SoA-PAR by
runningTower Defenseimplemented in each design, where only
SoA-PAR uses multiple threads. Although it is well established in
the literature and in industrial practice that ECS outperforms OOP,
our experiments quantify the performance gain obtained specifically from an implementation based on our ECS semantics. We use
OOP as the baseline because OOP remains the common architecture in game development and interactive simulations, especially
in commercial engines and educational materials. By comparing
against OOP, we show how much of the performance improvement
comes directly from our formal model.
We ran each design under the same game-play conditions (max

comes directly from our formal model.
We ran each design under the same game-play conditions (max
entities 20000, max enemies 15000, enemy spawn interval 0.05s,
turret firing interval 0.02s), targeting a fixed frame rate of 60 FPS.
Performance data were collected through an integrated profiler.
Experiments were conducted on a system equipped with Intel Core
i7-12650H CPU, 32 GB DDR5 RAM, and NVIDIA GeForce RTX 4060
GPU. All ECS computation and system scheduling run exclusively
on the CPU and the GPU is used only for rendering. To ensure stability of the performance, each configuration was executed multiple
times. Across these runs we observed minimal variance, and all runs
produced consistent timing behavior. Because the differences were
negligible, we report a representative run for each configuration.
We evaluated two foreground loop policies in LibGDX/LWJGL: an

Table 1: Average FPS across architectures & foreground FPS.

Frame Rate.Table 1 represents the average FPS achieved among
four designs under the two rendering settings. OOP exhibits the
lowest performance in both settings and shows marked frame-time
instability. The AoS design nearly doubles OOP throughput, while
the archetype SoA yields an additional improvement. The parallel
variant (SoA-PAR) performs the best overall, sustaining over 100
FPS with an unconstrained foreground rate and maintaining close
to the 60 FPS target under capped conditions.
The cumulative FPS distributions in Figure 13 illustrates the
stability differences among architectures. Under the uncapped set-

| Architecture | Avg FPS{foregroundFPS=0} | Avg FPS{foregroundFPS=60} |
| --- | --- | --- |
| OOP | 36.13 | 25.49 |
| AOS | 61.51 | 47.71 |
| Archetype SoA | 65.81 | 48.76 |
| Archetype SoA-PAR | 109.54 | 58.54 |

steeply near the 60 FPS mark, indicating minimal variance and stable frame timing close to the target rate. By contrast, OOP exhibits
a much flatter distribution, revealing substantial frame-to-frame
fluctuations and degraded temporal stability.

8 Conclusion

In this paper, we presented a formal semantics and a type system for
an archetype-based ECS framework. Our semantic model captures
the essence of archetype ECS computation in that entities can be
read and updated in parallel by stateless systems while mutations
to archetype tables are delayed until they can be safely processed.
We also evaluated the performance of four architectural designs,
including OOP, AoS, archetype SoA, and its parallel extension (SoA-
PAR) using aTower Defensesimulation. The results align with prior
findings: the SoA architecture consistently outperforms OOP and
AoS due to its contiguous memory layout and improved cache
locality. The parallel SoA design further amplifies these benefits by
exploiting multi-core hardware, achieving both higher throughput
and stable frame pacing under load.
As future work, we will conduct a scalability study with varying

[1] Bevy. 2025. bevy_ecs::archetype - Rust. https://docs.rs/bevy_ecs/latest/bevy_
ecs/archetype/index.html [Online; accessed 2025-10-09].
[2] Bailey V. Compton. 2022.An Investigation of Data Storage in Entity-Component
Systems. Master’s thesis. Air Force Institute of Technology, Wright-Patterson
Air Force Base, Ohio. https://scholar.afit.edu/etd/5353 AFIT-ENG-MS-22-M-018;
DTIC Accession No. AD1166837.
[3] John Conway. 1970. Conway’s Game of Life: Scientific American, October 1970.
https://www.ibiblio.org/lifepatterns/october1970.html [Online; accessed 2025-10-
08].
[4] Louis Cox, Benjamin Williams, James Vickers, Davin Ward, and Christopher
Headleand. 2025. Run-time Performance Comparison of Sparse-set and Archetype

As future work, we will conduct a scalability study with varying
entity counts, game maps, and system complexity. We will also
extend our formalism to model system query to retrieve entities
using filters. While queries are flexible, they introduce challenges
to static checking of task-parallel systems since the sets of queried
archetypes are dynamic. Another interesting feature is entity relationships [9]. For example, Turret entities can relate to Bullet entities via a Fires relationship, which enables more capable queries
than component-based ones. However, cleaning up relationships
after entity destruction is a challenge. Other design choices include
automatic execution of staged events based on the archetypes of
the next system and dynamic scheduling of parallel systems based
on their dependencies and the availability of threads [13].
The source code is available at https://github.com/uwm-se/ecs.

This work is partially supported by the Northwestern Mutual Data
Science Institute (NMDSI) under grant number SS136.

---

(a) Cumulative FPS distribution (foregroundFPS=0).

(b) Cumulative FPS distribution (foregroundFPS=60).

Figure 13: Comparison of cumulative FPS distributions for OOP, AoS, SoA, SoA-PAR with or without capping FPS at 60.

Entity-Component Systems. InComputer Graphics and Visual Computing (CGVC), 2025-10-09].
Yun Sheng and Aidan Slingsby (Eds.). The Eurographics Association. doi:10. [11] Sander Mertens. 2025. SanderMertens/tower_defense: Tower defense game
2312/cgvc.20251224 written in Flecs. https://github.com/SanderMertens/tower_defense [Online;
[5] Kirill Fedoseev, Nursultan Askarbekuly, Ekaterina Uzbekova, and Manuel Maz-accessed 2025-10-09].
zara. 2020. A Case Study on Object-Oriented and Data-Oriented Design [12] Patrick Redmond, Jonathan Castello, José Trilla, and Lindsey Kuper. 2025. Explor-
Paradigms in Game Development. doi:10.13140/RG.2.2.16657.66405 ing the Theory and Practice of Concurrency in the Entity-Component-System
[6] Daniel Masamune Hall. 2014.ECS Game Engine Design. Bachelor’s thesis. Cali-Pattern. doi:10.48550/arXiv.2508.15264
fornia Polytechnic State University - San Luis Obispo. https://digitalcommons. [13] Vittorio Romeo. 2016.Analysis of Entity Encoding Techniques, Design and Imcalpoly.edu/cpesp/135/ plementation of a Multithreaded Compile-time Entity-Component-System C++14
[7] Toni Harkonen. 2019. Advantages and Implementation of Entity-Component-Library. Ph. D. Dissertation. Università degli Studi di Messina. doi:10.13140/RG.2.
Systems - Trepo. https://trepo.tuni.fi/handle/123456789/27593 1.1307.4165
[8] Lars I. Hatledal, Yingguang Chu, Arne Styve, and Houxiang Zhang. 2021. Vico: [14] Brennan Shacklett, Zhiqiang Xie, Bidipta Sarkar, Andrew Szot, Erik Wijmans,
An Entity-Component-System Based Co-simulation Framework.Simulation Vladlen Koltun, Dhruv Batra, and Kayvon Fatahalian. 2023. An Extensible, Data-
Modelling Practice and Theory108 (2021), 102243. doi:10.1016/j.simpat.2020. Oriented Architecture for High-Performance, Many-World Simulation.ACM
102243 Transactions on Graphics42 (07 2023), 1–13. doi:10.1145/3592427
[9] Sander Mertens. 2020. Building an ECS #2: Archetypes and Vectorization | by [15] Šimon Tichý. 2023. The Last Clan - RTS game in Unity. https://dspace.cuni.cz/
Sander Mertens | Medium. https://ajmmertens.medium.com/building-an-ecs-2-handle/20.500.11956/188235
archetypes-and-vectorization-fe21690805f9 [Online; accessed 2025-10-09]. [16] Wikimedia. 2007. Tower defense - Wikipedia. https://en.wikipedia.org/wiki/
[10] Sander Mertens. 2025. SanderMertens/flecs: A Fast Entity Component System Tower_defense [Online; accessed 2025-10-09].
(ECS) for C & C++. https://github.com/SanderMertens/flecs [Online; accessed

──────── [TRUNCATED] ────────
Showing 29,958 chars (head) + 9,984 chars (tail) of 44,746 total clean characters.
Full text saved to: /home/wubu/.hermes/profiles/mind-palace/cache/web/boyang.cs.uwm.edu-47f3d1a46f.md
To read the omitted middle: read_file path="/home/wubu/.hermes/profiles/mind-palace/cache/web/boyang.cs.uwm.edu-47f3d1a46f.md" offset=584 limit=200  (the file is the complete page; raise/lower offset to page through it).
─────────────────────────────