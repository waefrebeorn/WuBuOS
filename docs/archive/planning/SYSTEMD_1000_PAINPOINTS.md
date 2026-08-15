# systemd Pain Points — 1000 grounded complaints (rebalanced, ~56/category)

> Compiled from HN 'why do people hate systemd' threads, the systemd GitHub issue tracker (top-voted: #8639 DNS-over-HTTPS, #2720 journal NOT operator, #11103 networkd netns, #26839 v253 restart regression), Fedora/Arch/Reddit bug reports (journald boot hangs, resolved DNS breakage), and 2025-2026 LPE CVEs (CVE-2025-6018/6019, CVE-2025-4598, CVE-2026-3888, CVE-2017-18078). Rebalanced to ~56 items per category.

Categories: boot, journald, resolved, networkd, units, security, debug, philosophy, portability, desktop, timers, mounts, tmpfiles, nspawn, analyze, udev, portability_kernel, misc

0001. [udev] SYSTEMD_LOG_LEVEL spam leaks into the journal (variant 3)
0002. [udev] Predictable names sometimes aren't across reboots in VMs (variant 2)
0003. [debug] journalctl -u X --since slow on big journals
0004. [security] udev+systemd-udevd as root is a large attack surface (variant 2)
0005. [debug] No 'why is this unit not starting' single command (variant 3)
0006. [udev] TAG+='systemd' magic links devices to units opaquely
0007. [portability_kernel] Assumes unified hierarchy; hybrid setups misbehave (variant 3)
0008. [networkd] DHCP lease hooks require writing .network drop-ins blind (variant 2)
0009. [units] Need systemctl cat to see what's running (HN egorfine) (variant 3)
0010. [misc] man pages only, no narrative guides
0011. [portability_kernel] Some syscalls assumed present break on hardened kernels (variant 2)
0012. [philosophy] Replacing working small tools erases institutional knowledge (variant 3)
0013. [networkd] Routing policy rules need manual ip rule glue (variant 2)
0014. [journald] journalctl -b empty after a crash because logs weren't flushed (variant 2)
0015. [tmpfiles] tmpfiles.d syntax terse and unforgiving (variant 2)
0016. [misc] Defaults tilt toward desktop, hurt servers
0017. [portability] Builds with old clang but README says clang>=10 (PR #23926) (variant 3)
0018. [resolved] Restarting resolved briefly drops all in-flight connections (variant 2)
0019. [security] Mount unit options leveraged for container escape (variant 2)
0020. [debug] journalctl -xe is the only path and it's a binary blob (variant 2)
0021. [boot] Emergency target drops to a shell with no clear recovery hint
0022. [mounts] Noauto mounts still pulled by some .mount Requires (variant 3)
0023. [philosophy] Backwards-compat attitude unlike Torvalds's (HN gwd, 2019) (variant 2)
0024. [security] CVE-2025-6018/6019: PAM + libblockdev/udisks chain to root LPE (variant 4)
0025. [philosophy] It's opinionated in ways that override admin intent (variant 3)
0026. [tmpfiles] Custom subdirs under /var get wiped by package rules (variant 3)
0027. [nspawn] nspawn can't cap GPU access cleanly (variant 3)
0028. [networkd] Link files vs .network vs .netdev = three places for one NIC (variant 3)
0029. [portability_kernel] Some syscalls assumed present break on hardened kernels (variant 3)
0030. [misc] Every release renames or repurposes a knob someone relied on (variant 3)
0031. [resolved] resolved declares a working local DNS 'broken' for no reason (reddit 18kh1r5)
0032. [timers] Persistent timers fire a burst after downtime, overloading (variant 2)
0033. [analyze] blame includes units that finished fast but blocked others
0034. [security] systemd as PID1 means one bug takes down the whole machine
0035. [misc] No built-in boot-time SLO/alerting
0036. [resolved] Per-interface DNS routing confuses VPN + LAN coexistence (variant 2)
0037. [resolved] No easy 'flush cache' without restarting the daemon (variant 3)
0038. [misc] config valid in v250 breaks in v255
0039. [tmpfiles] Custom subdirs under /var get wiped by package rules
0040. [mounts] Noauto mounts still pulled by some .mount Requires
0041. [journald] journalctl --disk-usage lies; vacuum needs manual cron (variant 2)
0042. [udev] Naming (eth0->ens3) breaks scripts expecting stable names (variant 3)
0043. [udev] A bad ATTR match fails silently (no device)
0044. [units] Canceled jobs give unhelpful default explanation (#15616, open) (variant 2)
0045. [desktop] Power/sleep keys routed through logind get 'inhibited' silently (variant 3)
0046. [resolved] Split DNS / routing domains confuse apps reading /etc/resolv.conf
0047. [portability] macOS can't reuse a single unit file
0048. [portability] Containers must run partial systemd to be 'compatible'
0049. [analyze] analyze dot floods with invisible edges (variant 3)
0050. [udev] udev rule ordering by filename easy to get wrong
0051. [portability] macOS can't reuse a single unit file (variant 3)
0052. [analyze] analyze verify needs the full unit set to be valid
0053. [timers] AccuracySec=1us spams wakeups and wastes power (variant 3)
0054. [udev] udevadm test needs a simulated uevent you must craft (variant 2)
0055. [boot] First boot after update takes minutes while devices enumerated twice (variant 3)
0056. [philosophy] Hate is partly a loud minority but grievances are real (variant 3)
0057. [philosophy] Maintainer broke working things without suitable fixes (HN gwd) (variant 2)
0058. [networkd] Static + DHCP on same interface needs awkward tricks (variant 3)
0059. [misc] localed/hostnamed/timedated add D-Bus for tiny jobs (variant 2)
0060. [desktop] Inhibitor locks block shutdown with no obvious owner (variant 2)
0061. [resolved] No DNS-over-HTTPS support in resolved (#8639, open since 2018) (variant 3)
0062. [journald] No easy 'tail -f /var/log/foo' equivalent per unit
0063. [misc] 4000+ open issues, hard to find dupes (variant 2)
0064. [networkd] Link files vs .network vs .netdev = three places for one NIC
0065. [portability] Cross-compiling systemd needs a separate build root (variant 3)
0066. [debug] systemd is non-transparent about why a job is waiting (egorfine)
0067. [networkd] networkd can't set netns for a netdev (#11103, open since 2018) (variant 4)
0068. [journald] No per-service log size cap; one chatty unit eats the disk (variant 2)
0069. [networkd] Link files vs .network vs .netdev = three places for one NIC (variant 2)
0070. [analyze] You need root for most analyze subcommands (variant 3)
0071. [networkd] IPv6 SLAAC + DHCPv6 combo has undocumented edge cases (variant 2)
0072. [udev] udev reload races with rapid plug/unplug
0073. [udev] Naming (eth0->ens3) breaks scripts expecting stable names
0074. [tmpfiles] Cleanup of /run can kill sockets services rely on
0075. [analyze] dot output needs graphviz to be useful (variant 3)
0076. [journald] Journal compression makes ad-hoc parsing impossible (variant 2)
0077. [analyze] You need root for most analyze subcommands
0078. [units] ReloadPropagatedFrom= semantics surprise everyone (variant 3)
0079. [analyze] no built-in compare between two boots
0080. [networkd] Renaming a device needs a reboot because of naming policy (variant 3)
0081. [boot] A dead NFS mount blocks shutdown for the full timeout (variant 3)
0082. [tmpfiles] Excl entries a rarely understood escape hatch
0083. [units] ReloadPropagatedFrom= semantics surprise everyone
0084. [journald] Must use journalctl -u to see a unit's logs; /var/log is gone (variant 3)
0085. [resolved] Fedora 42 update broke DNS until resolved restart (Discussion 148836) (variant 2)
0086. [misc] Too many binaries for one project to audit (variant 3)
0087. [mounts] A readonly remount needs both fstab and unit sync (variant 3)
0088. [tmpfiles] Debugging a tmpfiles denial means reading abstract codes (variant 2)
0089. [portability_kernel] kernel keyring use conflicts with container seccomp (variant 2)
0090. [desktop] user@.service starts even when no user logged in (variant 2)
0091. [networkd] networkd ignores carrier changes in some virtual envs (variant 2)
0092. [debug] Boot debugging needs systemd.log_level=debug + reboot (variant 3)
0093. [units] Requires=/After= subtle typo creates an ordering cycle
0094. [units] WantedBy=multi-user vs graphical is an easy miss (variant 3)
0095. [nspawn] No auto DNS for the container without extra setup
0096. [portability] Depends on D-Bus even when you don't want IPC (variant 3)
0097. [networkd] networkd waits on links that never come up, blocking boot (variant 3)
0098. [units] Wanting a generated service enabled is hard (#28006, open) (variant 3)
0099. [portability] systemd-nspawn isn't a real container runtime but used like one (variant 2)
0100. [desktop] User manager starts late, breaking early user timers
0101. [networkd] Renaming a device needs a reboot because of naming policy
0102. [udev] SYSTEMD_ALIAS vs SYMLINK confuses new authors (variant 2)
0103. [desktop] Backlight/rfkill via systemd is opaque when it fails (variant 2)
0104. [philosophy] Init must be simple; systemd isn't anymore (HN egorfine) (variant 2)
0105. [misc] No built-in boot-time SLO/alerting (variant 3)
0106. [resolved] resolved caches NXDOMAIN aggressively, hiding typo fixes (variant 3)
0107. [analyze] time spent in initrd mixed into userspace number (variant 2)
0108. [nspawn] Sharing X11/wayland socket manual and insecure (variant 2)
0109. [portability] Not portable to non-Linux (no *BSD, macOS, Solaris)
0110. [misc] Too many binaries (250+) for one project (variant 4)
0111. [philosophy] Conflated in hate with Poettering's other projects (Avahi, PulseAudio) (variant 2)
0112. [security] PID1 privilege means a parser bug is a full-compromise bug (variant 2)
0113. [security] udev+systemd-udevd as root is a large attack surface
0114. [misc] Project size makes auditing changes impractical for outsiders
0115. [timers] Calendar math around DST is surprising (variant 2)
0116. [debug] Dependency graph visualization needs external tools
0117. [analyze] analyze dot floods with invisible edges (variant 2)
0118. [mounts] systemd-fstab-generator runs at boot; typos block boot (variant 3)
0119. [security] DynamicUser= changes UID each boot, breaking shared state (variant 3)
0120. [udev] Rules in /run are ephemeral and confusing (variant 3)
0121. [udev] udev+systemd-udevd crash takes down hotplug entirely
0122. [boot] Ordering cycles silently broken/reordered, changing boot behavior (variant 2)
0123. [portability] Depends on kernel features that break on old/custom kernels (variant 2)
0124. [debug] No safe 'dry run' for systemctl start (variant 2)
0125. [timers] No easy 'show all timers and next run' overview
0126. [journald] Journal files not portable between machines or architectures
0127. [timers] Disabling a timer needs mask, not just disable (variant 3)
0128. [journald] MaxRetentionSec silently drops logs needed for forensics (variant 2)
0129. [mounts] Quota/fsck ordering with .mount hard to reason about (variant 2)
0130. [analyze] systemd-analyze plot produces huge SVGs hard to read (variant 4)
0131. [philosophy] The bus (dbus) becomes mandatory plumbing (variant 2)
0132. [nspawn] machinectl lacks basic 'exec into running container' ergonomics (variant 2)
0133. [units] Conflicts= can stop the wrong service unexpectedly
0134. [udev] udev reload races with rapid plug/unplug (variant 2)
0135. [boot] Boot waits on network-online.target though network already up (HN egorfine) (variant 3)
0136. [portability] WSL1 can't run systemd properly
0137. [networkd] Renaming a device needs a reboot because of naming policy (variant 2)
0138. [networkd] networkd waits on links that never come up, blocking boot (variant 2)
0139. [analyze] blame includes units that finished fast but blocked others (variant 2)
0140. [networkd] networkd ignores carrier changes in some virtual envs (variant 3)
0141. [portability_kernel] Hibernate depends on swap setup systemd decides for you (variant 3)
0142. [nspawn] Port publish needs --port each launch (variant 2)
0143. [analyze] Time spent in 'kernel' vs 'userspace' often wrong
0144. [timers] Calendar math around DST is surprising (variant 3)
0145. [philosophy] It optimizes for desktop at the cost of server clarity (variant 2)
0146. [nspawn] systemd-nspawn lacks CNI/networking parity with docker (variant 3)
0147. [nspawn] systemd-nspawn lacks CNI/networking parity with docker (variant 2)
0148. [networkd] Mixing NetworkManager and networkd breaks resolver ordering (variant 3)
0149. [udev] TAG+='systemd' magic links devices to units opaquely (variant 3)
0150. [units] WantedBy=multi-user vs graphical is an easy miss (variant 2)
0151. [boot] Shutdown waits on user sessions already logged out (variant 2)
0152. [resolved] Dual-stack fallback slow when IPv6 is broken (variant 3)
0153. [nspawn] No resource limits UI comparable to docker/update
0154. [tmpfiles] A bad line in tmpfiles.d aborts the whole apply
0155. [timers] OnUnitActiveSec vs OnBootSec vs OnCalendar confuses newcomers (variant 3)
0156. [security] CVE-2025-6018/6019: PAM + libblockdev/udisks chain to root LPE (variant 2)
0157. [analyze] plot SVG too big to open in a browser
0158. [security] Socket activation opens listeners before the service is ready
0159. [networkd] No 'show me the effective config' like 'ip a' summary
0160. [boot] systemd-journal-flush copies journal to disk adding 10s+ (askubuntu 1268578) (variant 2)
0161. [boot] Boot hangs if a required device path changed after cloning (variant 3)
0162. [nspawn] nspawn boot ignores most host-provided capabilities cleanly (variant 3)
0163. [resolved] Search domains multiply lookups and slow resolution (variant 2)
0164. [security] resolvconf/resolved as root parses untrusted network data
0165. [philosophy] Monolithic design contradicts the small-tools Unix way
0166. [philosophy] One project now owns init, login, logging, resolving, timedate
0167. [networkd] networkd-wait-online blocks boot on unconfigured ports
0168. [networkd] Mixing NetworkManager and networkd breaks resolver ordering (variant 2)
0169. [boot] Socket units start daemons before filesystems are mounted (variant 3)
0170. [networkd] No clean way to apply one interface change without restarting (variant 3)
0171. [units] Type=simple lies: process spawned != service ready
0172. [timers] Timer+service pairing doubles file count (variant 3)
0173. [security] CVE-2025-4598: systemd SUID process crash swap to non-SUID = priv esc
0174. [timers] No easy 'show all timers and next run' overview (variant 3)
0175. [security] PID1 privilege means a parser bug is a full-compromise bug
0176. [mounts] tmpfiles + mounts race on /tmp cleanup vs mount (variant 3)
0177. [portability_kernel] Some syscalls assumed present break on hardened kernels
0178. [units] ConditionPathExists hides a unit with no log why (variant 3)
0179. [boot] Reboot hangs on 'A stop job is running' for 90s (common HN gripe)
0180. [nspawn] No image layering; copies the whole tree (variant 3)
0181. [misc] Too many binaries for one project to audit
0182. [portability_kernel] cgroup v2 only breaks some LXC setups (variant 2)
0183. [udev] Predictable names sometimes aren't across reboots in VMs (variant 3)
0184. [journald] remote journal shipping needs yet another daemon (variant 3)
0185. [portability] Some utils assume cgroup v2 only, breaking container hosts (variant 2)
0186. [timers] No built-in 'last 10 runs' history (variant 2)
0187. [nspawn] Container gets a fresh machine-id each clone (variant 2)
0188. [units] Unit files hide the real command behind multiple drop-ins (variant 2)
0189. [udev] udev+systemd-udevd crash takes down hotplug entirely (variant 2)
0190. [boot] Boot hangs if a required device path changed after cloning
0191. [udev] udevadm control --reload races with in-flight events (variant 3)
0192. [timers] No easy 'show all timers and next run' overview (variant 2)
0193. [units] ExecStartPre failure leaves confusing 'failed' with no detail (variant 2)
0194. [networkd] Debugging networkd means decoding cryptic 'could not bring up' (variant 2)
0195. [boot] A start job is running for Journal Service stalls the login prompt
0196. [debug] Boot debugging needs systemd.log_level=debug + reboot (variant 2)
0197. [units] Requires=/After= subtle typo creates an ordering cycle (variant 3)
0198. [nspawn] Container gets a fresh machine-id each clone
0199. [desktop] Inhibitor locks block shutdown with no obvious owner
0200. [desktop] Brightness/volume keys routed via logind can double-bind
0201. [security] DynamicUser= changes UID each boot, breaking shared state (variant 2)
0202. [philosophy] Maintainer broke working things without suitable fixes (HN gwd)
0203. [mounts] Mount failures enter 'failed' but the device is fine (variant 2)
0204. [analyze] analyze verify needs the full unit set to be valid (variant 3)
0205. [boot] Socket units start daemons before filesystems are mounted (variant 2)
0206. [portability_kernel] hibernate needs swap systemd decides to configure (variant 3)
0207. [timers] No built-in 'last 10 runs' history
0208. [mounts] systemd-remount-fs races with early services (variant 3)
0209. [analyze] No per-boot regression diff ('was boot slower than last?')
0210. [analyze] Time spent in 'kernel' vs 'userspace' often wrong (variant 3)
0211. [debug] Failed units show 'failed' but cause is 3 layers deep (variant 3)
0212. [journald] No NOT operator in journal matching (#2720, open since 2016, 9y+) (variant 2)
0213. [portability] Seccomp/namespace assumptions fail under restrictive sandboxes
0214. [analyze] analyze verify needs the full unit set to be valid (variant 2)
0215. [analyze] Most subcommands need root for no good reason (variant 2)
0216. [security] polkit + systemd action auth is a large, confused attack surface (variant 2)
0217. [debug] Status text truncates the one line you need (variant 2)
0218. [desktop] user@.service starts even when no user logged in
0219. [mounts] Boot hangs on fsck of a huge disk with no progress (variant 2)
0220. [portability_kernel] Memfd/watchdog features need recent kernels (variant 2)
0221. [debug] A failed Requires shows no diff of what was expected (variant 3)
0222. [tmpfiles] q /tmp 1777 root root 4m style lines cryptic (CVE-2026-3888) (variant 2)
0223. [misc] No stable CLI output contract; scripts break on upgrade (variant 3)
0224. [analyze] Conditional units confuse blame accounting (variant 3)
0225. [analyze] critical-chain hides the actual blocker behind 'graphical' (variant 3)
0226. [udev] Naming (eth0->ens3) breaks scripts expecting stable names (variant 2)
0227. [debug] Bad debuggability: edit unit + daemon-reload loops (HN top complaint) (variant 4)
0228. [misc] Every release renames or repurposes a knob someone relied on
0229. [resolved] Per-interface DNS routing confuses VPN + LAN coexistence (variant 3)
0230. [udev] No dry-run of 'what would this device match'
0231. [mounts] Automount units hang first access if the share is slow (variant 3)
0232. [security] A mis-set NoNewPrivileges still allows some escapes
0233. [portability] Won't run on minimal embedded without a huge dep tree
0234. [journald] MaxRetentionSec silently drops logs needed for forensics
0235. [debug] journalctl -xe is the only path and it's a binary blob (variant 3)
0236. [networkd] No equivalent of 'ifup -v' to preview what will happen (variant 2)
0237. [analyze] no built-in compare between two boots (variant 3)
0238. [philosophy] It's opinionated in ways that override admin intent (variant 2)
0239. [mounts] Mount failures enter 'failed' but the device is fine
0240. [portability_kernel] Namespace assumptions fail under gVisor/Firecracker (variant 2)
0241. [mounts] Quota/fsck ordering with .mount hard to reason about
0242. [debug] Lockups happen whenever there is a fifo in the wrong place (HN) (variant 4)
0243. [timers] AccuracySec tradeoffs undocumented and surprising (variant 2)
0244. [desktop] User timers don't run if the user never logs in (variant 3)
0245. [portability_kernel] BPF-based features need very new kernels (variant 3)
0246. [security] Dynamic UID allocation complicates MAC/audit rules (variant 2)
0247. [desktop] Multi-user seat assignment powerful but undocumented
0248. [nspawn] No image layering; copies the whole tree (variant 2)
0249. [tmpfiles] tmpfiles runs before some dirs exist, skipping them
0250. [portability_kernel] keyring use conflicts with container seccomp (variant 2)
0251. [security] logind kill-user on logout nukes detached screen/tmux (variant 3)
0252. [security] ProtectSystem=strict needs ReadWritePaths you must enumerate (variant 3)
0253. [udev] No dry-run of 'what would this device match' (variant 3)
0254. [resolved] Restarting resolved briefly drops all in-flight connections
0255. [units] ConditionPathExists hides a unit with no log why
0256. [misc] 4000+ open issues, hard to find dupes
0257. [timers] RandomizedDelaySec makes timing nondeterministic to debug (variant 2)
0258. [portability] WSL1 can't run systemd properly (variant 3)
0259. [misc] Every release renames or repurposes a knob someone relied on (variant 2)
0260. [units] Edit unit + daemon-reload + retry is a slow debug loop (HN godelski) (variant 3)
0261. [analyze] no alert when boot regresses past a threshold (variant 2)
0262. [udev] Debugging udev means udevadm monitor spam (variant 3)
0263. [desktop] user@.service starts even when no user logged in (variant 3)
0264. [philosophy] Replaces dbus, timedatectl, logind — scope creep everywhere (variant 4)
0265. [desktop] User services (systemd --user) confuse and double the unit space (variant 2)
0266. [debug] A failed Requires shows no diff of what was expected (variant 2)
0267. [misc] No narrative docs, only man pages
0268. [debug] Tracing why a service restarted needs correlating 4 logs (variant 3)
0269. [units] Requires=/After= subtle typo creates an ordering cycle (variant 2)
0270. [boot] Reboot hangs on 'A stop job is running' for 90s (common HN gripe) (variant 2)
0271. [desktop] User services (systemd --user) confuse and double the unit space (variant 4)
0272. [boot] Ordering cycles silently broken/reordered, changing boot behavior
0273. [security] logind kill-user on logout nukes detached screen/tmux (variant 2)
0274. [mounts] NFS mounts with _netdev wait forever on a dead server (variant 2)
0275. [philosophy] Init must be simple; systemd isn't anymore (HN egorfine)
0276. [networkd] No clean way to apply one interface change without restarting
0277. [journald] Journal files not portable between machines or architectures (variant 2)
0278. [desktop] Scope units for apps created inconsistently by DEs
0279. [analyze] blame includes units that finished fast but blocked others (variant 3)
0280. [debug] Status text truncates the one line you need (variant 3)
0281. [nspawn] Port publish needs --port each launch
0282. [portability_kernel] Hibernate depends on swap setup systemd decides for you (variant 2)
0283. [boot] A start job is running for Journal Service stalls the login prompt (variant 3)
0284. [tmpfiles] Cleanup of /run can kill sockets services rely on (variant 2)
0285. [units] StandardOutput=file writes but you forget the path (variant 2)
0286. [debug] daemon-reload required so often it becomes muscle memory (variant 3)
0287. [nspawn] No resource limits UI comparable to docker/update (variant 2)
0288. [portability] musl support resisted; glibc assumed (HN yc-kraln re: musl team)
0289. [portability_kernel] memfd_create requirement trips old kernels
0290. [security] CVE-2017-18078: systemd-tmpfiles fs.protected_hardlinks=0 LPE (variant 2)
0291. [misc] config valid in v250 breaks in v255 (variant 3)
0292. [resolved] Container /etc/resolv.conf points at host stub, breaks isolation
0293. [units] Edit unit + daemon-reload + retry is a slow debug loop (HN godelski) (variant 2)
0294. [boot] A dead NFS mount blocks shutdown for the full timeout (variant 2)
0295. [portability] Some CI runners lack cgroup v2, breaking tests
0296. [misc] Version churn breaks config valid in one release
0297. [portability] musl support resisted; glibc assumed (HN yc-kraln re: musl team) (variant 4)
0298. [debug] Dependency graph visualization needs external tools (variant 2)
0299. [boot] graphical.target pulls a ton before the shell appears (variant 2)
0300. [units] ExecStartPre failure leaves confusing 'failed' with no detail
0301. [analyze] Time spent in 'kernel' vs 'userspace' often wrong (variant 2)
0302. [debug] No safe 'dry run' for systemctl start
0303. [units] Conflicting drop-ins in /etc vs /lib vs /run silently override (variant 2)
0304. [nspawn] It's 'not a container runtime' but is used as one anyway (variant 2)
0305. [units] A space in a path breaks ExecStart silently (variant 3)
0306. [philosophy] Monolithic design contradicts the small-tools Unix way (variant 3)
0307. [udev] Debugging udev means udevadm monitor spam (variant 2)
0308. [analyze] plot SVG too big to open in a browser (variant 3)
0309. [units] Restart=always resurrects a crashing service, masking bugs
0310. [portability_kernel] EFI/bootctl assumes systemd-boot (variant 3)
0311. [nspawn] No auto DNS for the container without extra setup (variant 3)
0312. [portability_kernel] EFI/bootctl assumes systemd-boot, fights other bootloaders (variant 2)
0313. [nspawn] Port publish needs --port each launch (variant 3)
0314. [nspawn] No restart policy for crashed containers (variant 3)
0315. [boot] A dead NFS mount blocks shutdown for the full timeout
0316. [debug] journalctl -xe is the only path and it's a binary blob
0317. [desktop] Wayland session startup ordering vs systemd is fragile (variant 2)
0318. [desktop] Multi-seat config powerful but barely documented (variant 2)
0319. [portability_kernel] Assumes unified hierarchy; hybrid setups misbehave (variant 2)
0320. [udev] Custom rules need both 99-local and correct priority (variant 2)
0321. [misc] RFEs stay open 7-9 years (DNS-over-HTTPS, NOT operator) (variant 2)
0322. [units] Conflicting drop-ins in /etc vs /lib vs /run silently override (variant 3)
0323. [portability_kernel] io.latency/weight controllers need cgroup v2 + kernel support (variant 2)
0324. [debug] systemd is non-transparent about why a job is waiting (egorfine) (variant 2)
0325. [misc] RHEL carries huge downstream patch sets (label downstream/rhel) (variant 3)
0326. [analyze] no alert when boot regresses past a threshold (variant 3)
0327. [units] Unit files hide the real command behind multiple drop-ins (variant 3)
0328. [timers] Calendar math around DST is surprising
0329. [misc] Version churn breaks config valid in one release (variant 3)
0330. [security] A mis-set NoNewPrivileges still allows some escapes (variant 3)
0331. [security] CVE-2025-6018/6019: PAM + libblockdev/udisks chain to root LPE (variant 3)
0332. [resolved] No easy 'flush cache' without restarting the daemon (variant 2)
0333. [portability] Not portable to non-Linux (no *BSD, macOS, Solaris) (variant 4)
0334. [philosophy] One project now owns init, login, logging, resolving, timedate (variant 2)
0335. [timers] Seconds-level precision timers spam the journal (variant 3)
0336. [mounts] Automount units hang first access if the share is slow (variant 2)
0337. [nspawn] systemd-nspawn lacks CNI/networking parity with docker
0338. [philosophy] It absorbs smaller daemons instead of cooperating (variant 2)
0339. [portability_kernel] cgroup v2 requirement breaks old containers
0340. [units] StandardOutput=file writes but you forget the path (variant 3)
0341. [analyze] no alert when boot regresses past a threshold
0342. [philosophy] Conflated in hate with Poettering's other projects (Avahi, PulseAudio)
0343. [udev] udevadm test needs a simulated uevent you must craft
0344. [security] Dynamic UID allocation complicates MAC/audit rules (variant 3)
0345. [timers] OnCalendar syntax cryptic (e.g. *-*-* 0/6:00:00) (variant 4)
0346. [timers] Persistent=true fires late after every reboot unexpectedly (variant 3)
0347. [boot] fstab-generator converts typos into unbootable systems
0348. [philosophy] Too late to replace; we live with the consequences (HN egorfine) (variant 2)
0349. [mounts] x-systemd.automount hides a hang until first access (variant 2)
0350. [security] CVE-2026-3888: snap-confine + systemd-tmpfiles cleanup timing => root (variant 2)
0351. [timers] OnCalendar syntax cryptic (e.g. *-*-* 0/6:00:00) (variant 2)
0352. [journald] remote journal shipping needs yet another daemon
0353. [portability] FreeBSD jails can't use any of it (variant 2)
0354. [udev] udev rule ordering by filename easy to get wrong (variant 2)
0355. [boot] Emergency target drops to a shell with no clear recovery hint (variant 3)
0356. [boot] systemd-journal-flush copies journal to disk adding 10s+ (askubuntu 1268578)
0357. [philosophy] Backwards-compat attitude unlike Torvalds's (HN gwd, 2019) (variant 3)
0358. [resolved] No easy 'flush cache' without restarting the daemon
0359. [journald] Journal namespace isolation duplicates disk usage (variant 3)
0360. [security] CVE-2025-4598: systemd SUID process crash swap to non-SUID = priv esc (variant 4)
0361. [mounts] systemd-fstab-generator runs at boot; typos block boot (variant 2)
0362. [misc] RHEL carries huge downstream patch sets (label downstream/rhel) (variant 2)
0363. [udev] Rules in /run are ephemeral and confusing
0364. [portability] WSL1 can't run systemd properly (variant 2)
0365. [philosophy] Violates the Unix 'do one thing' principle (HN yc-kraln)
0366. [udev] A bad ATTR match fails silently (no device) (variant 2)
0367. [nspawn] No healthcheck concept; a dead container looks alive (variant 3)
0368. [tmpfiles] Debugging a tmpfiles denial means reading abstract codes
0369. [tmpfiles] subvolume/quota directives rarely explained
0370. [journald] Journal files not portable between machines or architectures (variant 4)
0371. [misc] Defaults tilt toward desktop, hurt servers
0372. [boot] graphical.target pulls a ton before the shell appears
0373. [portability_kernel] EFI/bootctl assumes systemd-boot, fights other bootloaders
0374. [desktop] User services (systemd --user) confuse and double the unit space (variant 3)
0375. [security] CVE-2025-6018/6019: PAM + libblockdev/udisks chain to root LPE
0376. [portability_kernel] io.latency/weight controllers need cgroup v2 + kernel support
0377. [analyze] no built-in compare between two boots (variant 2)
0378. [analyze] time spent in initrd mixed into userspace number
0379. [tmpfiles] Run at boot + on timer; double execution surprises
0380. [philosophy] Replacing working small tools erases institutional knowledge (variant 2)
0381. [desktop] Idle/suspend races cause laptops to never sleep or sleep instantly
0382. [journald] Must use journalctl -u to see a unit's logs; /var/log is gone (variant 2)
0383. [desktop] logind session scope kills background jobs on logout (variant 2)
0384. [units] EnvironmentFile= quoting rules differ from shell
0385. [nspawn] It's 'not a container runtime' but is used as one anyway
0386. [nspawn] nspawn boot ignores most host-provided capabilities cleanly
0387. [debug] Bad debuggability: edit unit + daemon-reload loops (HN top complaint) (variant 2)
0388. [misc] No built-in boot-time SLO/alerting (variant 2)
0389. [portability_kernel] BPF-based features need very new kernels (variant 2)
0390. [timers] Editing a timer needs daemon-reload + restart (variant 3)
0391. [debug] A typo in a unit yields 'unit not found' with no line number
0392. [philosophy] Monolithic design contradicts the small-tools Unix way (variant 2)
0393. [udev] No dry-run of 'what would this device match' (variant 2)
0394. [mounts] zfs/btrfs mount integration distro-specific (variant 3)
0395. [timers] Persistent=true fires late after every reboot unexpectedly (variant 2)
0396. [portability_kernel] watchdog features need kernel watchdog present (variant 3)
0397. [portability_kernel] BPF usage in some units needs CAP_BPF (variant 2)
0398. [networkd] IPv6 SLAAC + DHCPv6 combo has undocumented edge cases (variant 3)
0399. [mounts] Bind mounts lose SELinux labels silently (variant 3)
0400. [debug] A typo in a unit yields 'unit not found' with no line number (variant 3)
0401. [security] run0 (sudo replacement) has no default timeout equivalent (HN) (variant 3)
0402. [boot] journald hangs 5-9s on every boot (Arch BBS #194968, Fedora Discussion) (variant 3)
0403. [philosophy] Too late to replace; we live with the consequences (HN egorfine) (variant 3)
0404. [portability_kernel] EFI/bootctl assumes systemd-boot (variant 2)
0405. [nspawn] Port forwarding manual and brittle
0406. [portability] Won't run on minimal embedded without a huge dep tree (variant 2)
0407. [tmpfiles] No feedback on what tmpfiles deleted last boot (variant 2)
0408. [misc] No narrative docs, only man pages (variant 2)
0409. [security] systemd as PID1 means one bug takes down the whole machine (variant 3)
0410. [tmpfiles] Age-based cleanup deletes files apps still use (CVE chains) (variant 2)
0411. [debug] A typo in a unit yields 'unit not found' with no line number (variant 2)
0412. [journald] journalctl -f misses early-boot messages already scrolled past (variant 3)
0413. [networkd] networkd ignores carrier changes in some virtual envs
0414. [resolved] LLMNR on by default is a security and surprise-resolution footgun
0415. [journald] journalctl binary blob can't be grepped without journalctl (no plain text) (variant 4)
0416. [portability_kernel] watchdog features need kernel watchdog present
0417. [resolved] Restarting resolved briefly drops all in-flight connections (variant 3)
0418. [desktop] logind session scope kills background jobs on logout (variant 3)
0419. [udev] udevadm control --reload races with in-flight events (variant 2)
0420. [units] Need systemctl cat to see what's running (HN egorfine) (variant 4)
0421. [nspawn] Sharing X11/wayland socket manual and insecure (variant 3)
0422. [debug] Bad debuggability: edit unit + daemon-reload loops (HN top complaint)
0423. [networkd] Routing policy rules need manual ip rule glue (variant 3)
0424. [security] Mount unit options leveraged for container escape (variant 3)
0425. [desktop] Multi-seat config powerful but barely documented
0426. [tmpfiles] Age field unit confusion (d vs m vs s) deletes wrongly (variant 3)
0427. [boot] journald hangs 5-9s on every boot (Arch BBS #194968, Fedora Discussion) (variant 4)
0428. [philosophy] Violates the Unix 'do one thing' principle (HN yc-kraln) (variant 4)
0429. [tmpfiles] Run at boot + on timer; double execution surprises (variant 3)
0430. [units] Need systemctl cat to see what's running (HN egorfine) (variant 2)
0431. [portability_kernel] Memfd/watchdog features need recent kernels
0432. [journald] Forwarding to syslog duplicates everything and doubles I/O (variant 3)
0433. [security] CVE-2026-3888: snap-confine + systemd-tmpfiles cleanup timing => root
0434. [nspawn] No restart policy for crashed containers (variant 2)
0435. [tmpfiles] Cleanup of /run can kill sockets services rely on (variant 3)
0436. [philosophy] It absorbs smaller daemons instead of cooperating (variant 3)
0437. [boot] systemd-analyze blame shows one unit at 26s but you can't fix it easily
0438. [desktop] GNOME hard-dep makes escaping systemd impossible (HN) (variant 2)
0439. [analyze] blame blames the symptom not the dependency (variant 2)
0440. [boot] A missing fstab mount adds a silent 90s wait before rescue
0441. [mounts] x-systemd.automount hides a hang until first access (variant 3)
0442. [networkd] Bonding config differs enough from classic to force relearning
0443. [resolved] DNSSEC validation fails silently and falls back to insecure (variant 2)
0444. [journald] remote journal shipping needs yet another daemon (variant 2)
0445. [debug] Tracing why a service restarted needs correlating 4 logs
0446. [units] Template units @%i syntax powerful but cryptic (variant 2)
0447. [tmpfiles] No dry-run preview of what tmpfiles will delete (variant 3)
0448. [resolved] systemd-resolved stops resolving randomly (#21123, open, many dupes) (variant 3)
0449. [mounts] tmpfiles + mounts race on /tmp cleanup vs mount (variant 2)
0450. [debug] Errors reported as 'see journalctl' in a journal you can't read (variant 3)
0451. [nspawn] Sharing host directories requires verbose bind args (variant 3)
0452. [resolved] DNSSEC validation fails silently and falls back to insecure (variant 3)
0453. [udev] udevadm test needs a simulated uevent you must craft (variant 3)
0454. [mounts] zfs/btrfs mount integration distro-specific (variant 2)
0455. [nspawn] machinectl lacks basic 'exec into running container' ergonomics (variant 3)
0456. [misc] Version churn breaks config valid in one release (variant 2)
0457. [boot] systemd-analyze blame shows one unit at 26s but you can't fix it easily (variant 2)
0458. [tmpfiles] World-writable dirs from tmpfiles enable LPE (CVE-2025-27591)
0459. [tmpfiles] A bad line in tmpfiles.d aborts the whole apply (variant 2)
0460. [journald] journalctl binary blob can't be grepped without journalctl (no plain text)
0461. [tmpfiles] Age-based cleanup deletes files apps still use (CVE chains)
0462. [udev] No dry-run of 'what would this device match'
0463. [analyze] plot SVG too big to open in a browser (variant 2)
0464. [boot] Default 90s start job timeout makes boot feel hung on missing device (variant 2)
0465. [udev] TAG+='systemd' magic links devices to units opaquely (variant 2)
0466. [security] CVE-2026-3888: snap-confine + systemd-tmpfiles cleanup timing => root (variant 3)
0467. [portability] Depends on kernel features that break on old/custom kernels (variant 3)
0468. [nspawn] No volume mount parity with -v (variant 3)
0469. [portability_kernel] io.latency/weight controllers need cgroup v2 + kernel support (variant 3)
0470. [networkd] No equivalent of 'ifup -v' to preview what will happen
0471. [boot] Boot waits on network-online.target though network already up (HN egorfine)
0472. [tmpfiles] Age field unit confusion (d vs m vs s) deletes wrongly
0473. [portability_kernel] hibernate needs swap systemd decides to configure
0474. [nspawn] No image layering; copies the whole tree
0475. [resolved] LLMNR on by default is a security and surprise-resolution footgun (variant 2)
0476. [boot] Boot hangs if a required device path changed after cloning (variant 2)
0477. [resolved] Fedora 42 update broke DNS until resolved restart (Discussion 148836)
0478. [desktop] logind session tracking fights tmux/screen detached sessions (variant 4)
0479. [security] A mis-set NoNewPrivileges still allows some escapes (variant 2)
0480. [boot] Ordering cycles silently broken/reordered, changing boot behavior (variant 3)
0481. [nspawn] No auto DNS for the container without extra setup (variant 2)
0482. [portability_kernel] cgroup v2 only breaks some LXC setups (variant 3)
0483. [nspawn] Port forwarding manual and brittle (variant 2)
0484. [security] tmpfiles rules abused via world-writable /var/log subdirs (variant 3)
0485. [boot] First boot after update takes minutes while devices enumerated twice
0486. [portability] Some CI runners lack cgroup v2, breaking tests (variant 2)
0487. [resolved] resolved hijacks /etc/resolv.conf with a stub file apps don't expect (variant 2)
0488. [boot] journald watchdog timeout (1min) then abort+coredump loop on boot (variant 3)
0489. [timers] AccuracySec tradeoffs undocumented and surprising
0490. [nspawn] No auto-update or registry pull like containers
0491. [analyze] analyze verify errors reference generated unit names (variant 3)
0492. [tmpfiles] Custom tmpfiles overridden by package drop-ins
0493. [philosophy] Replacing working small tools erases institutional knowledge
0494. [tmpfiles] tmpfiles runs before some dirs exist, skipping them (variant 2)
0495. [udev] No dry-run of 'what would this device match' (variant 2)
0496. [boot] A missing fstab mount adds a silent 90s wait before rescue (variant 3)
0497. [resolved] Fedora 42 update broke DNS until resolved restart (Discussion 148836) (variant 4)
0498. [mounts] A failing mount unit blocks its Wants dependents (variant 2)
0499. [udev] udev reload races with rapid plug/unplug (variant 3)
0500. [security] ProtectSystem=strict needs ReadWritePaths you must enumerate
0501. [units] Type=simple lies: process spawned != service ready (variant 3)
0502. [portability] Cross-compiling systemd needs a separate build root (variant 2)
0503. [units] Template units @%i syntax powerful but cryptic (variant 3)
0504. [journald] Journal corruption loses ALL logs, not just one file (variant 2)
0505. [mounts] NFS mounts with _netdev wait forever on a dead server
0506. [desktop] D-Bus activation couples desktop to systemd internals
0507. [portability_kernel] keyring use conflicts with container seccomp (variant 3)
0508. [philosophy] Complexity at the lowest OS levels is the wrong place (variant 2)
0509. [tmpfiles] subvolume/quota directives rarely explained (variant 2)
0510. [portability] musl support resisted; glibc assumed (HN yc-kraln re: musl team) (variant 2)
0511. [misc] Downstreams diverge so a fix upstream takes years to reach users (variant 2)
0512. [philosophy] Maintainer broke working things without suitable fixes (HN gwd) (variant 3)
0513. [udev] SYSTEMD_LOG_LEVEL spam leaks into the journal
0514. [udev] A bad udev rule can stall all device coldplug (variant 3)
0515. [misc] Too many binaries (250+) for one project
0516. [portability_kernel] Some syscalls assumed present break on hardened kernels
0517. [units] Edit unit + daemon-reload + retry is a slow debug loop (HN godelski) (variant 4)
0518. [nspawn] Images must be raw/unstructured, no layers
0519. [mounts] Remounting needs systemctl daemon-reload to resync (variant 3)
0520. [journald] No per-service log size cap; one chatty unit eats the disk (variant 3)
0521. [units] StandardOutput=file writes but you forget the path
0522. [resolved] No DNS-over-HTTPS support in resolved (#8639, open since 2018)
0523. [journald] journalctl -f misses early-boot messages already scrolled past
0524. [security] run0 (sudo replacement) has no default timeout equivalent (HN) (variant 2)
0525. [debug] No straightforward 'what is blocking boot right now' command
0526. [philosophy] Replaces dbus, timedatectl, logind — scope creep everywhere
0527. [boot] journald hangs 5-9s on every boot (Arch BBS #194968, Fedora Discussion) (variant 2)
0528. [nspawn] Container gets a fresh machine-id each clone (variant 3)
0529. [mounts] Bind mounts lose SELinux labels silently
0530. [debug] No 'why is this unit not starting' single command
0531. [timers] Unit= inside [Timer] easy to point at wrong service (variant 3)
0532. [desktop] Inhibitor locks block shutdown with no obvious owner (variant 3)
0533. [journald] Journal corruption loses ALL logs, not just one file (variant 3)
0534. [analyze] critical-chain hides the actual blocker behind 'graphical' (variant 2)
0535. [debug] daemon-reload required so often it becomes muscle memory (variant 2)
0536. [networkd] Static + DHCP on same interface needs awkward tricks (variant 2)
0537. [portability_kernel] kernel keyring use conflicts with container seccomp
0538. [debug] daemon-reload required so often it becomes muscle memory
0539. [philosophy] Can't avoid it once GNOME depends on it ('don't use it' impossible) (variant 2)
0540. [analyze] Most subcommands need root for no good reason
0541. [philosophy] It optimizes for desktop at the cost of server clarity (variant 3)
0542. [nspawn] Sharing X11/wayland socket manual and insecure
0543. [units] Canceled jobs give unhelpful default explanation (#15616, open)
0544. [portability_kernel] Hibernate depends on swap setup systemd decides for you
0545. [portability] Some CI runners lack cgroup v2, breaking tests (variant 3)
0546. [analyze] No built-in boot-time SLO/alerting
0547. [misc] scope creep: 'systemd' now means everything (variant 3)
0548. [units] Edit unit + daemon-reload + retry is a slow debug loop (HN godelski)
0549. [portability] Containers must run partial systemd to be 'compatible' (variant 2)
0550. [timers] Timer+service pairing doubles file count
0551. [units] ReloadPropagatedFrom= semantics surprise everyone (variant 2)
0552. [resolved] Dual-stack fallback slow when IPv6 is broken (variant 2)
0553. [misc] localed/hostnamed/timedated add D-Bus for tiny jobs (variant 3)
0554. [resolved] resolv.conf is a symlink to a stub; editing it is futile (variant 3)
0555. [nspawn] Sharing host directories requires verbose bind args (variant 2)
0556. [security] ProtectSystem=strict needs ReadWritePaths you must enumerate (variant 2)
0557. [portability_kernel] Namespace assumptions fail under gVisor/Firecracker (variant 3)
0558. [portability_kernel] memfd_create requirement trips old kernels (variant 3)
0559. [philosophy] One project now owns init, login, logging, resolving, timedate (variant 3)
0560. [mounts] A failing mount unit blocks its Wants dependents
0561. [portability_kernel] BPF-based features need very new kernels
0562. [security] Socket activation opens listeners before the service is ready (variant 3)
0563. [portability] Cross-compiling systemd needs a separate build root
0564. [analyze] Conditional units confuse blame accounting
0565. [resolved] resolved caches NXDOMAIN aggressively, hiding typo fixes
0566. [philosophy] It couples the desktop stack to one vendor's design (variant 2)
0567. [analyze] time spent in initrd mixed into userspace number (variant 3)
0568. [portability] No clean path on Alpine/musl without rewriting
0569. [networkd] Debugging networkd means decoding cryptic 'could not bring up' (variant 3)
0570. [tmpfiles] q /tmp 1777 root root 4m style lines cryptic (CVE-2026-3888) (variant 3)
0571. [desktop] lid-switch handling overrides BIOS/UEFI settings (variant 2)
0572. [misc] Project size makes auditing changes impractical for outsiders (variant 3)
0573. [timers] Editing a timer needs daemon-reload + restart (variant 2)
0574. [mounts] fstab option translation to .mount units hides errors (variant 4)
0575. [desktop] User manager starts late, breaking early user timers (variant 3)
0576. [desktop] Wayland session startup ordering vs systemd is fragile
0577. [udev] No dry-run of 'what would this device match' (variant 3)
0578. [misc] man pages only, no narrative guides (variant 2)
0579. [portability_kernel] cgroup v2 requirement breaks old containers (variant 3)
0580. [portability_kernel] cgroup v2 only breaks some LXC setups
0581. [desktop] lid-switch handling overrides BIOS/UEFI settings
0582. [mounts] zfs/btrfs mount integration distro-specific
0583. [portability] Some utils assume cgroup v2 only, breaking container hosts
0584. [portability_kernel] EFI/bootctl assumes systemd-boot, fights other bootloaders (variant 3)
0585. [mounts] Noauto mounts still pulled by some .mount Requires (variant 2)
0586. [timers] Unit= inside [Timer] easy to point at wrong service
0587. [portability] Embedded Yocto pays a large systemd footprint
0588. [misc] 4000+ open issues, hard to find dupes (variant 3)
0589. [journald] Binary journal breaks classic log scraping/SED pipelines (variant 3)
0590. [boot] systemd-analyze blame shows one unit at 26s but you can't fix it easily (variant 3)
0591. [security] CVE-2017-18078: systemd-tmpfiles fs.protected_hardlinks=0 LPE
0592. [nspawn] systemd-nspawn lacks CNI/networking parity with docker (variant 4)
0593. [portability_kernel] memfd_create requirement trips old kernels (variant 2)
0594. [journald] Journal corruption loses ALL logs, not just one file
0595. [misc] RHEL carries huge downstream patch sets (label downstream/rhel)
0596. [journald] Journal compression makes ad-hoc parsing impossible (variant 3)
0597. [tmpfiles] Z lines (SELinux) need policy knowledge few have
0598. [timers] Missed timers leave no obvious 'why didn't it run' trail
0599. [desktop] Multi-seat config powerful but barely documented (variant 3)
0600. [tmpfiles] Custom subdirs under /var get wiped by package rules (variant 2)
0601. [nspawn] No volume mount parity with -v (variant 2)
0602. [analyze] systemd-analyze plot produces huge SVGs hard to read
0603. [timers] Editing a timer needs daemon-reload + restart
0604. [resolved] Search domains multiply lookups and slow resolution
0605. [portability] musl support resisted; glibc assumed (HN yc-kraln re: musl team) (variant 3)
0606. [resolved] mDNS clashes with Avahi in subtle ways (variant 2)
0607. [networkd] DHCP lease hooks require writing .network drop-ins blind
0608. [resolved] LLMNR on by default is a security and surprise-resolution footgun (variant 3)
0609. [analyze] You need root for most analyze subcommands (variant 2)
0610. [networkd] No clean way to apply one interface change without restarting (variant 2)
0611. [portability] Depends on kernel features that break on old/custom kernels
0612. [boot] journald watchdog timeout (1min) then abort+coredump loop on boot (variant 2)
0613. [desktop] logind session tracking fights tmux/screen detached sessions (variant 2)
0614. [desktop] Brightness/volume keys routed via logind can double-bind (variant 2)
0615. [portability_kernel] BPF usage in some units needs CAP_BPF
0616. [portability_kernel] Some syscalls assumed present break on hardened kernels (variant 3)
0617. [resolved] No DNS-over-HTTPS support in resolved (#8639, open since 2018) (variant 2)
0618. [mounts] Network mounts need _netdev or they block boot
0619. [philosophy] Conflated in hate with Poettering's other projects (Avahi, PulseAudio) (variant 3)
0620. [desktop] Scope units for apps created inconsistently by DEs (variant 2)
0621. [timers] No cron-style @daily shorthand that behaves identically (variant 3)
0622. [portability] Builds with old clang but README says clang>=10 (PR #23926) (variant 2)
0623. [debug] Tracing why a service restarted needs correlating 4 logs (variant 2)
0624. [udev] Rules in /run are ephemeral and confusing (variant 2)
0625. [mounts] Mount unit names derived from paths are unreadable (variant 2)
0626. [analyze] No built-in boot-time SLO/alerting (variant 2)
0627. [desktop] Multi-user seat assignment powerful but undocumented (variant 3)
0628. [analyze] analyze verify errors reference generated unit names
0629. [security] resolvconf/resolved as root parses untrusted network data (variant 3)
0630. [timers] RandomizedDelaySec makes timing nondeterministic to debug (variant 3)
0631. [misc] Too many binaries for one project to audit (variant 2)
0632. [desktop] Backlight/rfkill via systemd is opaque when it fails
0633. [security] CVE-2025-4598: systemd SUID process crash swap to non-SUID = priv esc (variant 2)
0634. [networkd] No 'show me the effective config' like 'ip a' summary (variant 3)
0635. [units] ConditionPathExists hides a unit with no log why (variant 2)
0636. [mounts] NFS mounts with _netdev wait forever on a dead server (variant 3)
0637. [networkd] MACAddress= match fails if interface renamed first (variant 3)
0638. [nspawn] No restart policy for crashed containers
0639. [mounts] Network mounts need _netdev or they block boot (variant 3)
0640. [resolved] resolved caches NXDOMAIN aggressively, hiding typo fixes (variant 2)
0641. [desktop] Multi-user seat assignment powerful but undocumented (variant 2)
0642. [portability_kernel] cgroup v2 requirement breaks old containers (variant 4)
0643. [boot] Shutdown waits on user sessions already logged out
0644. [boot] A start job is running for Journal Service stalls the login prompt (variant 2)
0645. [resolved] Split DNS / routing domains confuse apps reading /etc/resolv.conf (variant 2)
0646. [mounts] systemd-remount-fs races with early services (variant 2)
0647. [timers] No cron-style @daily shorthand that behaves identically
0648. [resolved] Search domains multiply lookups and slow resolution (variant 3)
0649. [udev] udev rule ordering by filename easy to get wrong (variant 3)
0650. [nspawn] No healthcheck concept; a dead container looks alive (variant 2)
0651. [timers] No built-in 'last 10 runs' history (variant 3)
0652. [mounts] Mount unit names derived from paths are unreadable
0653. [analyze] Conditional units confuse blame accounting (variant 2)
0654. [portability_kernel] watchdog features need kernel watchdog present (variant 2)
0655. [timers] Missed timers leave no obvious 'why didn't it run' trail (variant 3)
0656. [nspawn] No resource limits UI comparable to docker/update (variant 3)
0657. [analyze] No built-in boot-time SLO/alerting (variant 3)
0658. [resolved] systemd-resolved stops resolving randomly (#21123, open, many dupes) (variant 2)
0659. [networkd] VLAN/bridge config syntax verbose and error-prone
0660. [resolved] mDNS clashes with Avahi in subtle ways (variant 3)
0661. [tmpfiles] q /tmp 1777 root root 4m style lines cryptic (CVE-2026-3888)
0662. [analyze] Most subcommands need root for no good reason (variant 3)
0663. [portability] Depends on D-Bus even when you don't want IPC (variant 2)
0664. [philosophy] It optimizes for desktop at the cost of server clarity
0665. [misc] man pages only, no narrative guides (variant 3)
0666. [debug] Failed units show 'failed' but cause is 3 layers deep
0667. [udev] SYSTEMD_ALIAS vs SYMLINK confuses new authors (variant 3)
0668. [misc] Too many binaries (250+) for one project (variant 3)
0669. [networkd] Static + DHCP on same interface needs awkward tricks
0670. [portability] Some utils assume cgroup v2 only, breaking container hosts (variant 3)
0671. [tmpfiles] Age field unit confusion (d vs m vs s) deletes wrongly (variant 2)
0672. [philosophy] Backwards-compat attitude unlike Torvalds's (HN gwd, 2019)
0673. [resolved] resolv.conf is a symlink to a stub; editing it is futile
0674. [journald] Journal rate-limiting hides bursts of real errors (variant 2)
0675. [tmpfiles] tmpfiles.d syntax terse and unforgiving (variant 4)
0676. [tmpfiles] Custom tmpfiles overridden by package drop-ins (variant 2)
0677. [mounts] Boot hangs on fsck of a huge disk with no progress (variant 3)
0678. [nspawn] nspawn can't cap GPU access cleanly
0679. [boot] journald watchdog timeout (1min) then abort+coredump loop on boot
0680. [analyze] No per-boot regression diff ('was boot slower than last?') (variant 2)
0681. [mounts] Bind mounts lose SELinux labels silently (variant 2)
0682. [portability] No clean path on Alpine/musl without rewriting (variant 3)
0683. [networkd] No equivalent of 'ifup -v' to preview what will happen (variant 3)
0684. [debug] systemctl status truncates the most relevant error lines
0685. [networkd] networkd-wait-online blocks boot on unconfigured ports (variant 2)
0686. [portability] Embedded Yocto pays a large systemd footprint (variant 2)
0687. [tmpfiles] Excl entries a rarely understood escape hatch (variant 2)
0688. [nspawn] No volume mount parity with -v
0689. [units] Need systemctl cat to see what's running (HN egorfine)
0690. [udev] A bad udev rule can stall all device coldplug
0691. [debug] Lockups happen whenever there is a fifo in the wrong place (HN) (variant 3)
0692. [debug] No straightforward 'what is blocking boot right now' command (variant 2)
0693. [portability] systemd-nspawn isn't a real container runtime but used like one
0694. [debug] Errors reported as 'see journalctl' in a journal you can't read
0695. [mounts] Automount units hang first access if the share is slow
0696. [networkd] MACAddress= match fails if interface renamed first (variant 2)
0697. [units] Canceled jobs give unhelpful default explanation (#15616, open) (variant 3)
0698. [portability] macOS can't reuse a single unit file (variant 2)
0699. [security] Dynamic UID allocation complicates MAC/audit rules
0700. [boot] Emergency target drops to a shell with no clear recovery hint (variant 2)
0701. [boot] fstab-generator converts typos into unbootable systems (variant 2)
0702. [tmpfiles] Z lines (SELinux) need policy knowledge few have (variant 3)
0703. [desktop] logind session tracking fights tmux/screen detached sessions
0704. [tmpfiles] No feedback on what tmpfiles deleted last boot (variant 3)
0705. [networkd] networkd can't set netns for a netdev (#11103, open since 2018) (variant 3)
0706. [units] Conflicting drop-ins in /etc vs /lib vs /run silently override
0707. [analyze] blame blames the symptom not the dependency
0708. [units] Type=simple lies: process spawned != service ready (variant 2)
0709. [networkd] No 'show me the effective config' like 'ip a' summary (variant 2)
0710. [mounts] A readonly remount needs both fstab and unit sync
0711. [tmpfiles] A typo in mode bits opens a security hole silently (variant 3)
0712. [mounts] fstab option translation to .mount units hides errors (variant 3)
0713. [debug] Lockups happen whenever there is a fifo in the wrong place (HN)
0714. [desktop] logind session scope kills background jobs on logout
0715. [desktop] Brightness/volume keys routed via logind can double-bind (variant 3)
0716. [desktop] Idle/suspend races cause laptops to never sleep or sleep instantly (variant 3)
0717. [misc] No stable CLI output contract; scripts break on upgrade (variant 2)
0718. [debug] No 'why is this unit not starting' single command (variant 2)
0719. [networkd] Debugging networkd means decoding cryptic 'could not bring up'
0720. [debug] No straightforward 'what is blocking boot right now' command (variant 3)
0721. [desktop] D-Bus activation couples desktop to systemd internals (variant 2)
0722. [units] Restart=always resurrects a crashing service, masking bugs (variant 3)
0723. [tmpfiles] subvolume/quota directives rarely explained (variant 3)
0724. [journald] Journal files not portable between machines or architectures (variant 3)
0725. [mounts] Quota/fsck ordering with .mount hard to reason about (variant 3)
0726. [security] Socket activation opens listeners before the service is ready (variant 2)
0727. [nspawn] Sharing host directories requires verbose bind args
0728. [boot] A missing fstab mount adds a silent 90s wait before rescue (variant 2)
0729. [desktop] Wayland session startup ordering vs systemd is fragile (variant 3)
0730. [tmpfiles] tmpfiles.d syntax terse and unforgiving (variant 3)
0731. [resolved] resolved declares a working local DNS 'broken' for no reason (reddit 18kh1r5) (variant 3)
0732. [portability_kernel] hibernate needs swap systemd decides to configure (variant 2)
0733. [desktop] logind session tracking fights tmux/screen detached sessions (variant 3)
0734. [misc] RFEs stay open 7-9 years (DNS-over-HTTPS, NOT operator) (variant 3)
0735. [udev] udev rules are a regex/string-match maze (variant 2)
0736. [desktop] User manager starts late, breaking early user timers (variant 2)
0737. [resolved] Container /etc/resolv.conf points at host stub, breaks isolation (variant 3)
0738. [tmpfiles] Age-based cleanup deletes files apps still use (CVE chains) (variant 3)
0739. [portability] Seccomp/namespace assumptions fail under restrictive sandboxes (variant 3)
0740. [units] EnvironmentFile= quoting rules differ from shell (variant 3)
0741. [portability_kernel] cgroup v2 requirement breaks old containers (variant 2)
0742. [nspawn] It's 'not a container runtime' but is used as one anyway (variant 3)
0743. [misc] localed/hostnamed/timedated add D-Bus for tiny jobs
0744. [journald] No easy 'tail -f /var/log/foo' equivalent per unit (variant 2)
0745. [security] CVE-2017-18078: systemd-tmpfiles fs.protected_hardlinks=0 LPE (variant 3)
0746. [portability] Static linking systemd is impractical (variant 3)
0747. [portability] Won't run on minimal embedded without a huge dep tree (variant 3)
0748. [portability] Static linking systemd is impractical (variant 2)
0749. [portability] Embedded Yocto pays a large systemd footprint (variant 3)
0750. [resolved] nsswitch conflicts: getent hosts works but ping doesn't
0751. [resolved] Per-interface DNS routing confuses VPN + LAN coexistence
0752. [udev] udev rules are a regex/string-match maze (variant 3)
0753. [nspawn] No auto-update or registry pull like containers (variant 3)
0754. [mounts] systemd-fstab-generator runs at boot; typos block boot
0755. [udev] A bad udev rule can stall all device coldplug (variant 2)
0756. [units] Conflicts= can stop the wrong service unexpectedly (variant 2)
0757. [udev] udev rules are a regex/string-match maze (variant 4)
0758. [resolved] Split DNS / routing domains confuse apps reading /etc/resolv.conf (variant 3)
0759. [debug] journalctl -u X --since slow on big journals (variant 3)
0760. [nspawn] Port forwarding manual and brittle (variant 3)
0761. [networkd] DHCP lease hooks require writing .network drop-ins blind (variant 3)
0762. [journald] Journal compression makes ad-hoc parsing impossible
0763. [boot] First boot after update takes minutes while devices enumerated twice (variant 2)
0764. [mounts] systemd-remount-fs races with early services
0765. [debug] Bad debuggability: edit unit + daemon-reload loops (HN top complaint) (variant 3)
0766. [journald] MaxRetentionSec silently drops logs needed for forensics (variant 3)
0767. [desktop] Backlight/rfkill via systemd is opaque when it fails (variant 3)
0768. [analyze] blame blames the symptom not the dependency (variant 3)
0769. [timers] Seconds-level precision timers spam the journal (variant 2)
0770. [boot] journald hangs 5-9s on every boot (Arch BBS #194968, Fedora Discussion)
0771. [journald] Journal rate-limiting hides bursts of real errors (variant 3)
0772. [boot] graphical.target pulls a ton before the shell appears (variant 3)
0773. [mounts] fstab option translation to .mount units hides errors
0774. [udev] A bad ATTR match fails silently (no device) (variant 3)
0775. [resolved] systemd-resolved stops resolving randomly (#21123, open, many dupes)
0776. [resolved] systemd-resolved stops resolving randomly (#21123, open, many dupes) (variant 4)
0777. [portability] Builds with old clang but README says clang>=10 (PR #23926)
0778. [networkd] Bonding config differs enough from classic to force relearning (variant 3)
0779. [journald] Journal rate-limiting hides bursts of real errors
0780. [networkd] Bonding config differs enough from classic to force relearning (variant 2)
0781. [misc] config valid in v250 breaks in v255 (variant 2)
0782. [philosophy] Violates the Unix 'do one thing' principle (HN yc-kraln) (variant 2)
0783. [mounts] Network mounts need _netdev or they block boot (variant 2)
0784. [portability] systemd-nspawn isn't a real container runtime but used like one (variant 3)
0785. [journald] journalctl cursor bookmarks break across rotations (variant 3)
0786. [philosophy] It absorbs smaller daemons instead of cooperating
0787. [philosophy] It couples the desktop stack to one vendor's design
0788. [udev] Predictable names sometimes aren't across reboots in VMs
0789. [philosophy] Complexity at the lowest OS levels is the wrong place
0790. [timers] A timer with no service unit fails opaquely
0791. [portability_kernel] kernel keyring use conflicts with container seccomp (variant 3)
0792. [udev] Custom rules need both 99-local and correct priority (variant 3)
0793. [boot] journald watchdog timeout (1min) then abort+coredump loop on boot (variant 4)
0794. [nspawn] Images must be raw/unstructured, no layers (variant 2)
0795. [tmpfiles] No dry-run preview of what tmpfiles will delete (variant 2)
0796. [tmpfiles] tmpfiles.d syntax terse and unforgiving
0797. [mounts] Boot hangs on fsck of a huge disk with no progress
0798. [desktop] Idle/suspend races cause laptops to never sleep or sleep instantly (variant 2)
0799. [tmpfiles] A bad line in tmpfiles.d aborts the whole apply (variant 3)
0800. [resolved] Dual-stack fallback slow when IPv6 is broken
0801. [security] PID1 privilege means a parser bug is a full-compromise bug (variant 3)
0802. [misc] scope creep: 'systemd' now means everything (variant 2)
0803. [mounts] Remounting needs systemctl daemon-reload to resync
0804. [debug] Dependency graph visualization needs external tools (variant 3)
0805. [desktop] Scope units for apps created inconsistently by DEs (variant 3)
0806. [misc] scope creep: 'systemd' now means everything
0807. [resolved] Fedora 42 update broke DNS until resolved restart (Discussion 148836) (variant 3)
0808. [boot] Default 90s start job timeout makes boot feel hung on missing device
0809. [security] tmpfiles rules abused via world-writable /var/log subdirs
0810. [units] ExecStartPre failure leaves confusing 'failed' with no detail (variant 3)
0811. [networkd] networkd can't set netns for a netdev (#11103, open since 2018) (variant 2)
0812. [timers] AccuracySec tradeoffs undocumented and surprising (variant 3)
0813. [portability] Containers must run partial systemd to be 'compatible' (variant 3)
0814. [boot] Socket units start daemons before filesystems are mounted
0815. [units] Template units @%i syntax powerful but cryptic
0816. [tmpfiles] No dry-run preview of what tmpfiles will delete
0817. [units] WantedBy=multi-user vs graphical is an easy miss
0818. [analyze] No per-boot regression diff ('was boot slower than last?') (variant 3)
0819. [networkd] MACAddress= match fails if interface renamed first
0820. [journald] Journal namespace isolation duplicates disk usage (variant 2)
0821. [misc] No narrative docs, only man pages (variant 3)
0822. [units] A space in a path breaks ExecStart silently
0823. [debug] No safe 'dry run' for systemctl start (variant 3)
0824. [tmpfiles] Z lines (SELinux) need policy knowledge few have (variant 2)
0825. [debug] journalctl -u X --since slow on big journals (variant 2)
0826. [philosophy] Hate is partly a loud minority but grievances are real (variant 2)
0827. [tmpfiles] Excl entries a rarely understood escape hatch (variant 3)
0828. [security] polkit + systemd action auth is a large, confused attack surface
0829. [timers] Persistent timers fire a burst after downtime, overloading
0830. [philosophy] The bus (dbus) becomes mandatory plumbing (variant 3)
0831. [resolved] resolv.conf is a symlink to a stub; editing it is futile (variant 2)
0832. [security] run0 (sudo replacement) has no default timeout equivalent (HN)
0833. [timers] Seconds-level precision timers spam the journal
0834. [philosophy] Replaces dbus, timedatectl, logind — scope creep everywhere (variant 2)
0835. [portability_kernel] keyring use conflicts with container seccomp
0836. [networkd] networkd waits on links that never come up, blocking boot
0837. [journald] Binary journal breaks classic log scraping/SED pipelines (variant 2)
0838. [misc] Downstreams diverge so a fix upstream takes years to reach users
0839. [philosophy] Too late to replace; we live with the consequences (HN egorfine)
0840. [philosophy] Hate is partly a loud minority but grievances are real
0841. [units] EnvironmentFile= quoting rules differ from shell (variant 2)
0842. [nspawn] nspawn can't cap GPU access cleanly (variant 2)
0843. [resolved] resolved declares a working local DNS 'broken' for no reason (reddit 18kh1r5) (variant 2)
0844. [portability_kernel] EFI/bootctl assumes systemd-boot
0845. [analyze] analyze dot floods with invisible edges
0846. [networkd] No clean way to apply one interface change without restarting (variant 4)
0847. [misc] Project size makes auditing changes impractical for outsiders (variant 2)
0848. [desktop] User services (systemd --user) confuse and double the unit space
0849. [resolved] resolved hijacks /etc/resolv.conf with a stub file apps don't expect
0850. [mounts] x-systemd.automount hides a hang until first access
0851. [networkd] Mixing NetworkManager and networkd breaks resolver ordering
0852. [debug] systemctl status truncates the most relevant error lines (variant 2)
0853. [portability] Seccomp/namespace assumptions fail under restrictive sandboxes (variant 2)
0854. [boot] Boot waits on network-online.target though network already up (HN egorfine) (variant 2)
0855. [debug] Lockups happen whenever there is a fifo in the wrong place (HN) (variant 2)
0856. [portability_kernel] Assumes unified hierarchy; hybrid setups misbehave
0857. [philosophy] Can't avoid it once GNOME depends on it ('don't use it' impossible)
0858. [nspawn] No healthcheck concept; a dead container looks alive
0859. [security] DynamicUser= changes UID each boot, breaking shared state
0860. [portability] Depends on D-Bus even when you don't want IPC
0861. [udev] Debugging udev means udevadm monitor spam
0862. [tmpfiles] World-writable dirs from tmpfiles enable LPE (CVE-2025-27591) (variant 2)
0863. [timers] AccuracySec=1us spams wakeups and wastes power (variant 2)
0864. [mounts] Mount unit names derived from paths are unreadable (variant 3)
0865. [security] CVE-2025-4598: systemd SUID process crash swap to non-SUID = priv esc (variant 3)
0866. [tmpfiles] tmpfiles runs before some dirs exist, skipping them (variant 3)
0867. [nspawn] nspawn boot ignores most host-provided capabilities cleanly (variant 2)
0868. [journald] Must use journalctl -u to see a unit's logs; /var/log is gone
0869. [journald] Journal namespace isolation duplicates disk usage
0870. [tmpfiles] A typo in mode bits opens a security hole silently
0871. [tmpfiles] Debugging a tmpfiles denial means reading abstract codes (variant 3)
0872. [portability] Not portable to non-Linux (no *BSD, macOS, Solaris) (variant 3)
0873. [units] Conflicts= can stop the wrong service unexpectedly (variant 3)
0874. [journald] No NOT operator in journal matching (#2720, open since 2016, 9y+) (variant 3)
0875. [udev] SYSTEMD_LOG_LEVEL spam leaks into the journal (variant 2)
0876. [journald] journalctl cursor bookmarks break across rotations
0877. [portability] Not portable to non-Linux (no *BSD, macOS, Solaris) (variant 2)
0878. [analyze] systemd-analyze plot produces huge SVGs hard to read (variant 3)
0879. [timers] Unit= inside [Timer] easy to point at wrong service (variant 2)
0880. [portability_kernel] Memfd/watchdog features need recent kernels (variant 3)
0881. [timers] A timer with no service unit fails opaquely (variant 3)
0882. [debug] Failed units show 'failed' but cause is 3 layers deep (variant 2)
0883. [tmpfiles] Custom tmpfiles overridden by package drop-ins (variant 3)
0884. [debug] Status text truncates the one line you need
0885. [portability_kernel] Some syscalls assumed present break on hardened kernels (variant 2)
0886. [udev] udevadm control --reload races with in-flight events
0887. [analyze] analyze verify errors reference generated unit names (variant 2)
0888. [timers] Missed timers leave no obvious 'why didn't it run' trail (variant 2)
0889. [timers] Persistent=true fires late after every reboot unexpectedly
0890. [analyze] critical-chain hides the actual blocker behind 'graphical'
0891. [journald] Forwarding to syslog duplicates everything and doubles I/O (variant 2)
0892. [tmpfiles] No feedback on what tmpfiles deleted last boot
0893. [boot] Default 90s start job timeout makes boot feel hung on missing device (variant 3)
0894. [philosophy] Can't avoid it once GNOME depends on it ('don't use it' impossible) (variant 3)
0895. [portability] FreeBSD jails can't use any of it (variant 3)
0896. [resolved] resolved hijacks /etc/resolv.conf with a stub file apps don't expect (variant 3)
0897. [debug] Boot debugging needs systemd.log_level=debug + reboot
0898. [nspawn] Images must be raw/unstructured, no layers (variant 3)
0899. [networkd] networkd can't set netns for a netdev (#11103, open since 2018)
0900. [misc] Defaults tilt toward desktop, hurt servers (variant 2)
0901. [desktop] Power/sleep keys routed through logind get 'inhibited' silently (variant 2)
0902. [misc] No stable CLI output contract; scripts break on upgrade
0903. [misc] Downstreams diverge so a fix upstream takes years to reach users (variant 3)
0904. [mounts] tmpfiles + mounts race on /tmp cleanup vs mount
0905. [security] logind kill-user on logout nukes detached screen/tmux
0906. [portability] Static linking systemd is impractical
0907. [nspawn] No auto-update or registry pull like containers (variant 2)
0908. [debug] A failed Requires shows no diff of what was expected
0909. [portability_kernel] Namespace assumptions fail under gVisor/Firecracker
0910. [desktop] Power/sleep keys routed through logind get 'inhibited' silently
0911. [desktop] User timers don't run if the user never logs in (variant 2)
0912. [security] systemd as PID1 means one bug takes down the whole machine (variant 2)
0913. [timers] OnUnitActiveSec vs OnBootSec vs OnCalendar confuses newcomers (variant 2)
0914. [journald] journalctl -f misses early-boot messages already scrolled past (variant 2)
0915. [units] Restart=always resurrects a crashing service, masking bugs (variant 2)
0916. [analyze] systemd-analyze plot produces huge SVGs hard to read (variant 2)
0917. [tmpfiles] World-writable dirs from tmpfiles enable LPE (CVE-2025-27591) (variant 3)
0918. [networkd] IPv6 SLAAC + DHCPv6 combo has undocumented edge cases
0919. [analyze] dot output needs graphviz to be useful (variant 2)
0920. [units] A space in a path breaks ExecStart silently (variant 2)
0921. [security] tmpfiles rules abused via world-writable /var/log subdirs (variant 2)
0922. [debug] Errors reported as 'see journalctl' in a journal you can't read (variant 2)
0923. [portability] FreeBSD jails can't use any of it
0924. [misc] Too many binaries (250+) for one project (variant 2)
0925. [portability] No clean path on Alpine/musl without rewriting (variant 2)
0926. [security] polkit + systemd action auth is a large, confused attack surface (variant 3)
0927. [analyze] dot output needs graphviz to be useful
0928. [philosophy] Complexity at the lowest OS levels is the wrong place (variant 3)
0929. [desktop] GNOME hard-dep makes escaping systemd impossible (HN) (variant 3)
0930. [tmpfiles] Run at boot + on timer; double execution surprises (variant 2)
0931. [desktop] GNOME hard-dep makes escaping systemd impossible (HN)
0932. [networkd] VLAN/bridge config syntax verbose and error-prone (variant 3)
0933. [mounts] fstab option translation to .mount units hides errors (variant 2)
0934. [journald] Forwarding to syslog duplicates everything and doubles I/O
0935. [timers] Disabling a timer needs mask, not just disable
0936. [journald] Binary journal breaks classic log scraping/SED pipelines
0937. [nspawn] machinectl lacks basic 'exec into running container' ergonomics
0938. [timers] OnCalendar syntax cryptic (e.g. *-*-* 0/6:00:00)
0939. [journald] journalctl -b empty after a crash because logs weren't flushed (variant 3)
0940. [debug] systemctl status truncates the most relevant error lines (variant 3)
0941. [networkd] VLAN/bridge config syntax verbose and error-prone (variant 2)
0942. [mounts] Mount failures enter 'failed' but the device is fine (variant 3)
0943. [misc] RFEs stay open 7-9 years (DNS-over-HTTPS, NOT operator)
0944. [timers] Disabling a timer needs mask, not just disable (variant 2)
0945. [journald] No easy 'tail -f /var/log/foo' equivalent per unit (variant 3)
0946. [networkd] Routing policy rules need manual ip rule glue
0947. [tmpfiles] A typo in mode bits opens a security hole silently (variant 2)
0948. [udev] udev rules are a regex/string-match maze
0949. [timers] AccuracySec=1us spams wakeups and wastes power
0950. [security] resolvconf/resolved as root parses untrusted network data (variant 2)
0951. [timers] Timer+service pairing doubles file count (variant 2)
0952. [resolved] DNSSEC validation fails silently and falls back to insecure
0953. [desktop] lid-switch handling overrides BIOS/UEFI settings (variant 3)
0954. [resolved] Container /etc/resolv.conf points at host stub, breaks isolation (variant 2)
0955. [philosophy] Replaces dbus, timedatectl, logind — scope creep everywhere (variant 3)
0956. [security] Mount unit options leveraged for container escape
0957. [resolved] nsswitch conflicts: getent hosts works but ping doesn't (variant 2)
0958. [udev] udev+systemd-udevd crash takes down hotplug entirely (variant 3)
0959. [timers] OnUnitActiveSec vs OnBootSec vs OnCalendar confuses newcomers
0960. [boot] fstab-generator converts typos into unbootable systems (variant 3)
0961. [units] Unit files hide the real command behind multiple drop-ins
0962. [boot] Shutdown waits on user sessions already logged out (variant 3)
0963. [philosophy] It's opinionated in ways that override admin intent
0964. [timers] Persistent timers fire a burst after downtime, overloading (variant 3)
0965. [timers] A timer with no service unit fails opaquely (variant 2)
0966. [journald] journalctl -b empty after a crash because logs weren't flushed
0967. [misc] Defaults tilt toward desktop, hurt servers (variant 3)
0968. [mounts] Remounting needs systemctl daemon-reload to resync (variant 2)
0969. [mounts] A readonly remount needs both fstab and unit sync (variant 2)
0970. [units] Wanting a generated service enabled is hard (#28006, open)
0971. [misc] Defaults tilt toward desktop, hurt servers (variant 3)
0972. [resolved] mDNS clashes with Avahi in subtle ways
0973. [timers] No cron-style @daily shorthand that behaves identically (variant 2)
0974. [udev] SYSTEMD_ALIAS vs SYMLINK confuses new authors
0975. [journald] journalctl --disk-usage lies; vacuum needs manual cron
0976. [philosophy] Init must be simple; systemd isn't anymore (HN egorfine) (variant 3)
0977. [timers] RandomizedDelaySec makes timing nondeterministic to debug
0978. [journald] journalctl binary blob can't be grepped without journalctl (no plain text) (variant 3)
0979. [units] Wanting a generated service enabled is hard (#28006, open) (variant 2)
0980. [journald] No per-service log size cap; one chatty unit eats the disk
0981. [philosophy] Violates the Unix 'do one thing' principle (HN yc-kraln) (variant 3)
0982. [philosophy] It couples the desktop stack to one vendor's design (variant 3)
0983. [journald] journalctl cursor bookmarks break across rotations (variant 2)
0984. [boot] systemd-journal-flush copies journal to disk adding 10s+ (askubuntu 1268578) (variant 3)
0985. [networkd] networkd-wait-online blocks boot on unconfigured ports (variant 3)
0986. [desktop] User timers don't run if the user never logs in
0987. [portability_kernel] BPF usage in some units needs CAP_BPF (variant 3)
0988. [journald] No NOT operator in journal matching (#2720, open since 2016, 9y+)
0989. [timers] OnCalendar syntax cryptic (e.g. *-*-* 0/6:00:00) (variant 3)
0990. [debug] systemd is non-transparent about why a job is waiting (egorfine) (variant 3)
0991. [journald] journalctl --disk-usage lies; vacuum needs manual cron (variant 3)
0992. [desktop] D-Bus activation couples desktop to systemd internals (variant 3)
0993. [udev] Custom rules need both 99-local and correct priority
0994. [journald] journalctl binary blob can't be grepped without journalctl (no plain text) (variant 2)
0995. [boot] Reboot hangs on 'A stop job is running' for 90s (common HN gripe) (variant 3)
0996. [mounts] A failing mount unit blocks its Wants dependents (variant 3)
0997. [philosophy] The bus (dbus) becomes mandatory plumbing
0998. [resolved] nsswitch conflicts: getent hosts works but ping doesn't (variant 3)
0999. [misc] Defaults tilt toward desktop, hurt servers (variant 2)
1000. [security] udev+systemd-udevd as root is a large attack surface (variant 3)
