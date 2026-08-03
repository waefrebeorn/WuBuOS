# The Human-Centric Loop: Tandem Desktop + the BarunLM Seed (2026-08-02)

> The AGI is not an app that sits on the desktop — it IS the desktop's
> partner. The Tandem window renders the shared space; the psychology
> engine times the AGI's actions to the human's patience; the recovery
> substrate makes every AGI mistake reversible; and the BarunLM seed is
> the second brain that grows in the training loop.

## The Tandem desktop (user + AGI together)

`src/apps/tandem/tandem.c` — a real WuBuFX/DosGui window registered in
`g_app_defs` (icon +8) and `g_wubufx_apps` (cap AGI). It renders:

1. **USER MODEL gauge** — skill / fatigue / mood / attention
2. **TIMING LOOP** — who drives right now (human drives / AGI waits /
   AGI proposes), the patience window, the control arbitration
3. **COMPANION line** — Bonzi's psychology-driven reaction
4. **PROPOSAL bar** — when the human idles past patience, the AGI
   proposes (e.g. "compact the KV cache?") and waits for acceptance

The doctrine: **when the human takes the mouse/keyboard, the AGI
yields.** `wubu_psych_input_seen` marks the human as driving; `may_act`
blocks the AGI during active input; `patience_left` counts down.

## The psychology engine (`wubu_psych`, HX-A + HX-B, 200 gaps)

- the user model: profile, prefs, habits, skill, pace, fatigue, mood,
  attention, session, continuity, forgetting
- the human timing windows: perception 100 ms, gesture 300 ms, thought
  1500 ms, patience 5 s, boredom 15 s
- the control arbitration: input_seen / ai_act / ai_yield / may_act /
  patience_left — the human is priority, always
- adaptive UI: density, complexity, novice/expert mode, progressive
  disclosure, contextual help, workload adaptation
- the psychology: reaction time, Fitts's law, Hick's law, short-term
  memory (7±2), cognitive load, priming, feedback tones
- preference learning (explicit + implicit), drift detection, privacy
  export/wipe/audit, and the tandem propose/accept/decline loop

## The companion study (`wubu_bonzi_study`, HX-D, 100 gaps)

- empathy / warmth / humor / playfulness / personality
- consistency / honesty / calibration / boundaries / consent
- loneliness support / encouragement / celebration / commiseration /
  presence
- idle chat / small talk / stories / jokes / riddles
- avatar / expression / animation / micro-expressions / eye gaze
- speech / voice / pacing / echo / confirmation
- interruptions / availability / backoff / do-not-disturb
- the STUDY METRICS: success rate, best timing, annotated prestige
  ledger — the GUI persona is measured, not assumed

## The tutor (`wubu_tutor`, HX-C, 100 gaps)

The recursive learning loop made human: lessons, exercises, quizzes,
spaced repetition, Socratic questions, worked examples, interleaving,
mastery/forgetting/transfer models, goals/streaks/badges, focus/notes/
summaries/mind-maps, teach-back, and the engineering close.

## The 5+1 recovery substrate (the AGI may make mistakes)

`wubu_recovery` — five rotating rollback slots + the JESUS emergency
clean slate with the divine-good principles intact (identity,
human-centric, no-third-party, no-stubs, growth-loop). The Live
Colonel console commands: `live <expr>` (ring-0 expression eval with
persistent r0..r7), `recovery checkpoint|rollback|jesus|status`.

## The seed: BarunLM-35M (the second brain)

The wizard is now a TRAINING engine. BarunLM-35M (35,072,768 params,
Apache-2.0 upstream) is ported to C11 (`wubu_barun`), the released
checkpoint loads (SHA-256 verified), and the training core
(`wubu_barun_train`: Muon + AdamW, mean-reduced CE, residual-path
gradient) learns — loss 9.53 → 3.81 in the test. The seed grows via
the AGI brain-cluster loop: research repos → tokens → parameters →
evaluation. See wubuwizard `docs/barunlm-seed.md`.

## Files (2026-08-02)

- `src/kernel/wubu_psych.{h,c}` `wubu_bonzi_study.{h,c}` `wubu_tutor.{h,c}`
- `src/kernel/wubu_recovery.{h,c}` `src/kernel/test_*.c`
- `src/apps/tandem/{tandem.c,tandem.h,test_tandem.c}`
- `src/kernel/crt0.S` — PD_high[1]→PT_high2 (the >2MB higher-half fix)
- `docs/compendium/04-roadmap/human-bank.md` — HX-A/B/C/D wired (400)
- `docs/compendium/04-roadmap/storage-bank.md` — FS-A/B wired (200)
