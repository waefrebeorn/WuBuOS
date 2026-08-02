# WuBuOS AGI Operating System — Architecture (firmware-mediated)

## The platform (3 rings)
1. **Root of trust** (WuBuFW, already proved): PCR0–7 + Secure Boot + TPM measurements. This is the *verifiable* anchor — every self-modification the AGI makes is checked against a measurement that survives boot.
2. **Recursive learning substrate** (wubuwizard C11 engine ↔ recursive_optimize): the optimizer proposes self-modifications, the firmware's attestation chain verifies them, and the delta is committed only if the TCG vectors + AuthentiCode proofs still pass. Closed = open gaps in research/INDEX.md, specifically:
   - AV01–AV08: vector memory substrate (HNSW, PQ quant, episodic ANN index)
   - AW01–AW10: causal/neuro-symbolic (SCM, do-intervention, counterfactual, PDDL/STRIPS, ASP logic, abductive diagnosis)
   - AX02/AX03: seccomp sandbox + formal verification of generated C11
3. **Human surface** (WuBuFX GUI): Bonzi Buddy agent persona + Comfy node-graph editor. The human *bonzi* (queries the AGI, gets causal explanations) and *comfy* (edits the node graph of optimizer↔attestation↔memory). `wubufx_app_launch` routes real actions.

## Boot sequence (`make test_agi`)
```
WuBuFW firmware → firmware self-tests (SB+TPM green)
  → attest: PCR0..7 + SB state pinned
  → wubuwizard kernel shim loads the optimizer
  → recursive_optimize closes gaps (Kevin-Bacon 7-hop + Triple-DA)
  → Bonzi reports a converged fact, Comfy graph updates live
  → 0 open gaps → emit WUBU_AGI_SELFTEST_OK
```

## Gap closure priority (firmware-anchored)
The firmware gives us a *trusted measurement channel*. The gaps that most directly need that channel:
- AW01–AW10: every causal model mutation must be measured into PCR4 (code-as-data), so a hallucinated intervention shows up as a PCR drift and is rejected. Close as C11 modules, each with a TCG-vector-style self-test.
- AV01: HNSW on the KV cache — close first (single file, ~300 LOC, oracle-matched). Then AV02 (PQ quant), AV03 (session KV reuse), AV04 (similarity eviction).
- AX02: seccomp sandbox for generated code — research but scoped as C11 + a WuBuFW seccomp shim driver.
