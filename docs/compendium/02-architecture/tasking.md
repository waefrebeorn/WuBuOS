# 02-architecture — Tasking & Memory Map

## Task model (cooperative, stable base)

4 tasks round-robin via `task_yield`; the 100 Hz LAPIC tick advances
`g_tick`, wakes sleepers, and ticks the AGI supervisor:

| Task | Entry | Stack | Role |
|------|-------|-------|------|
| idle | `idle_task` | small | yields forever |
| agent | `agi_agent_task` | 256 KB | cog loop, emits AGENT trace spans |
| bonzi | `wubu_bonzi_task` | 128 KB | framebuffer gorilla + input + heartbeat |
| console | `wubu_console_task` | 64 KB | COM1 REPL (help/uptime/mem/tasks/…) |

`TaskContext` (tasking_switch.S): 18 qwords — r15..rsi callee-saved (RSI
restored LAST), rax@112, rip@120, rsp@128, rflags@136.

Timer PREEMPTION is DISABLED (tracked #GP — see 03-learned/didnt-work.md).

## Interrupt frame (isr_stubs.S ↔ interrupt.h, MUST stay in lockstep)

15 registers (push order rdi,rsi,rdx,rcx,r8,r9,r10,r11,rbx,rbp,r12,r13,
r14,r15,rax) → frame: rax@0, r15@8, …, rdi@112, vector@120, error@128,
rip@136, cs@144, rflags@152, rsp@160, ss@168.

## Memory map (metal)

- kernel.elf: single PT_LOAD at phys 0x100000, linked 0xffffffff80100000.
- early stack 0x70000; page tables at base+0x200000 (PT_high @ +0x5000).
- heap (mem_alloc): 64 MB from ~0x400000 (vbe fb @ 0x402018, back @ 0xbec018,
  task stacks ~0x13E-0x146 MB region).
- identity map 0-1 GB; higher-half for the kernel; APIC/IOAPIC MMIO via
  the 0xC0000000 window; PCI BARs via `wubu_map_phys_range`.
- BSS hazard note: `g_current` @ 0xffffffff80117c58 sits 8 bytes before
  `g_vbe.fb` @ 0xffffffff80117c60 — overlong writes silently corrupt the
  framebuffer pointer (see 03-learned/bugs.md).
