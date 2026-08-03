---
title: "rr: lightweight recording & deterministic debugging"
source_url: https://rr-project.org/
author: rr-debugger project (Robert O'Callahan et al., sponsored by Pernosco)
ingested: 2026-08-03
avenue: DevTools (DT)
type: project-page
refs: "Extended Technical Report arXiv:1705.05937"
---

# rr

- github: https://github.com/rr-debugger/rr
- mailing list: https://groups.google.com/g/rr-devel
- Development sponsored by Pernosco

## what rr does

rr aspires to be your primary C/C++ debugging tool for Linux, replacing --
well, enhancing -- gdb. You record a failure once, then debug the recording,
deterministically, as many times as you want. The same execution is replayed
every time.

rr also provides efficient reverse execution under gdb. Set breakpoints and
data watchpoints and quickly reverse-execute to where they were hit.

rr works on real applications and is used by many developers to fix real bugs.
It makes debugging hard bugs much easier, but also speeds up debugging of easy
bugs.

rr features:

- Low overhead compared to other similar tools, especially on
  mostly-single-threaded workloads
- Supports recording and replay of all kinds of applications: Firefox, Chrome,
  QEMU, LibreOffice, Go programs, ...
- Record, replay and debug multiple-process workloads, including entire
  containers
- Works with gdb scripting and IDE integration
- Durable, compact traces that can be ported between machines
- *Chaos mode* to make intermittent bugs more reproducible

## the rr debugging experience

Start by using rr to record your application:

```
$ rr record /your/application --args
...
FAIL: oh no!
```

The entire execution, including the failure, was saved to disk. That recording
can now be debugged.

```
$ rr replay
GNU gdb (GDB) ...
0x4cee2050 in _start () from /lib/ld-linux.so.2
(gdb)
```

You're debugging the *recorded trace* deterministically; *not* a live,
nondeterministic execution. The replayed execution's address spaces, register
contents, syscall data etc are exactly the same in every run.

Most of the common gdb commands can be used.

```
(gdb) break mozilla::dom::HTMLMediaElement::HTMLMediaElement
(gdb) continue
Breakpoint 1, mozilla::dom::HTMLMediaElement::HTMLMediaElement (this=0x61362f70, ...)
```

If you need to restart the debugging session, use gdb's `run` command to
restart replay. The `run` command started another replay run of your recording
from the beginning -- but the *same execution* was replayed again, and all your
debugging state was preserved across the restart. The `this` pointer of the
dynamically-allocated object was the same in both replay sessions. Memory
allocations are exactly the same in each replay, meaning you can hard-code
addresses you want to watch.

Even more powerful is reverse execution. Suppose we're debugging Firefox
layout:

```
Breakpoint 1, nsCanvasFrame::BuildDisplayList (this=0x2aaadd7dbeb0, ...)
    at layout/generic/nsCanvasFrame.cpp:460
(gdb) p mRect.width
12000
```

We happen to know that value is wrong. We want to find out where it was set.

```
(gdb) watch -l mRect.width
(gdb) reverse-cont
Continuing.
Hardware watchpoint 2: -location mRect.width
Old value = 12000
New value = 11220
0x00002aaab100c0fd in nsIFrame::SetRect (this=0x2aaadd7dbeb0, aRect=...)
    at layout/base/../generic/nsIFrame.h:718
718       mRect = aRect;
```

This combination of hardware data watchpoints with reverse execution is
extremely powerful.

## background and motivation

rr's original motivation was to make debugging of intermittent failures easier.
These failures are hard to debug because any given program run may not show the
failure. We wanted to create a tool that would record program executions with
low overhead, so you can record test executions until you see a failure, and
then replay the failing execution repeatedly under a debugger until it has been
completely understood.

We also hoped deterministic replay would make debugging of any kind of bug
easier. With normal debuggers, information you learn during the debugging
session (e.g. the addresses of objects of interest, and the ordering of
important events) often becomes obsolete when you have to rerun the testcase.
With deterministic replay, that never needs to happen: your knowledge of what
happens during the failing run increases monotonically.

Furthermore, since debugging is the process of tracing effects to their causes,
it's much easier if your debugger can execute backwards in time. Given a
record/replay system which provides restartable checkpoints during replay, you
can simulate reverse execution to a particular point in time by restoring the
previous checkpoint and executing forwards to the desired point.

rr records a group of Linux user-space processes and captures all inputs to
those processes from the kernel, plus any nondeterministic CPU effects
performed by those processes (of which there are very few). rr replay
guarantees that execution preserves instruction-level control flow and memory
and register contents. The memory layout is always the same, the addresses of
objects don't change, register values are identical, syscalls return the same
data, etc.

Tools like fuzzers and randomized fault injectors become even more powerful
when used with rr. Those tools are very good at triggering *some* intermittent
failure, but it's often hard to reproduce *that same* failure again to debug
it. With rr, the randomized execution can simply be recorded.

## rr in context

Record-and-replay debugging is an old idea; many systems preceded rr. What
makes rr different are the design goals:

- *Initial focus on Firefox*. Many record and replay techniques require
  specific programming languages or don't scale well and thus can't handle
  Firefox -- or were just experimental and were never fleshed out.
- *Deployability*. rr runs on stock Linux kernels, on commodity hardware, and
  requires no system configuration changes. Many record and replay techniques
  require kernel changes or rely on running the OS in a virtual machine.
- *Low run-time overhead*. We want rr to replace gdb in your workflow. That
  means you need to start getting results with rr about as quickly as you would
  if you were using gdb. Low overhead also means less perturbation of tests.
- *Simplicity of design*. We avoided approaches that rely on complex techniques
  such as dynamic binary instrumentation. This simplicity has also made rr more
  robust and lower overhead.

Overhead depends on your workload. On Firefox test suites, slowdowns are as low
as <= 1.2x. For mostly-single-threaded programs, rr has much lower overhead
than any competing record-and-replay system we know of.

## limitations

rr ...

- emulates a single-core machine. Parallel programs incur the slowdown of
  running on a single core. This is an inherent feature of the design.
- cannot record processes that share memory with processes outside the
  recording tree. rr automatically disables features such as X shared memory
  for recorded processes to avoid this problem.
- requires a reasonably modern x86 CPU or certain ARM CPUs (Apple M1+).
- requires knowledge of every system call executed by the recorded processes.
  Support isn't complete, so running rr on your application may uncover a
  syscall that needs to be implemented.
- sometimes needs to be updated in response to kernel changes, updates to
  system libraries, or new CPU families.

## further reference

The Extended Technical Report (https://arxiv.org/pdf/1705.05937.pdf) is the
best overview of how rr works and performs. The rr wiki covers technical
topics related to rr.
