# 04 — Free tiers, sponsorships, and agent-native rentals

Checked 2026-11-19 (pricing pages + a 2026-10 cross-comparison,
macswarm.com/compare/macos-ci-runners-pricing). All prices are list;
the free tiers are the point.

## Free quotas (spares, not the primary)

- Codemagic: 500 free M2 minutes/month, pay-as-you-go after ($0.095/min
  M2, $0.114 M4). API/CLI drivable. Good spare when GitHub queues.
- Blacksmith: ~150 free macOS minutes/month (M4, 6 vCPU/24 GB), GitHub
  App integration. Avrea similar (~150 free min/mo, per-second billing).
- GetMac: free plan 100 compute minutes/month, 1 VM, NO CARD. M4 base
  VM. Partial automation; smallest quota.
- Macswarm: 3 Pod-hours once, no card; then $99/mo. One-shot, not a loop.

These exist so a burst of verification never waits on GitHub's 5-job
concurrency. None replaces the unlimited public-repo tier.

## MacStadium FOSS program (the sponsorship)

macstadium.com/company/opensource: free bare-metal Mac mini (M1 or
Intel) for qualifying FOSS projects; they sponsor 100+ projects
(Homebrew, Swift Package Index, DDEV, Aurora Editor). Criteria:
- active unpaid lead/contributor;
- project not majority-commercially-funded;
- software free to users (paid support/consulting allowed);
- 3+ months of development with regular releases.

WuBuOS fits the criteria on paper (Umbrella v3, active, unfunded).
BUT: application + waitlist = a human gate, and the waitlist means
weeks. Verdict: apply ONCE, asynchronously, as a backup; never block
the loop on it. If approved, the mini becomes a self-hosted runner
(fully automatable from then on).

## Agent-native rentals (paid, but no telephone tag)

- NoMac (nomac.app): $0.80/hr, from $10 prepaid, 7-day money-back.
  "A root session for any agent over MCP, CLI, API or SSH" — M4 to M6,
  Xcode 27.2. This is the RunPod model done right for agents: no
  portal, no nerd. Still PAID; last resort when free paths are down.
- Namespace/WarpBuild/Depot/Bitrise: $0.03-0.08/min macOS, trials or
  small credits. Same class; none beats free.

## Dead and refused

- Cirrus CI: STOPPED RUNNING JOBS 2026-06-01; Cirrus Runners closed to
  new customers 2026-04-07. Old tutorials still recommend it. Dead.
- AWS EC2 Mac instances: dedicated hosts, 24-hour minimum allocation,
  ~$1+/hr — exactly the debt-adjacent rental the owner refuses.
- MacStadium paid / MacRun ($139+/mo): unnecessary given the free tier.
- Xcode Cloud: needs the $99/yr Apple Developer Program. Refused:
  subscription debt for something CI gives free.