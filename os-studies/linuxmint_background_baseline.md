# Linux Mint 22.3 (Cinnamon) — Background Runtime Baseline

> Companion to `cinnamon_study.md` (which covers the Cinnamon DE chrome, panel, WM). This study covers the **other** axis: the layered runtime that runs *behind* the desktop to make a Mint 22.3 (Cinnamon 6.6.9) session usable — the session manager, the systemd + D-Bus service tiers, the autostarted tray/indicator daemons, and the service contracts that glue them together. Live-hose facts below are labelled **"local, raw command output"** and were read on this host (Linux Mint 22.3 `zena`/noble, `uid 1000`, display `lightdm` → `Xorg :0`). [unverified]

## 0. Architecture overview: the background runtime tiers

A running Linux Mint desktop is not one program; it is a **stack of on-demand and always-on background tiers**. From inner to outer: [unverified]

1. **PID 1 + system bus tier** — `systemd` as first process brings up and supervises userspace services; D-Bus runs the systemwide message bus; a fleet of privileged system daemons (NetworkManager, polkit, udisks2, logind, resolved, timesyncd, ModemManager, avahi, cups, accounts-daemon, bluetoothd …) own hardware and system-wide state and expose themselves over D-Bus. [unverified]
2. **User-manager tier** — a per-user `systemd --user` instance (`user@1000.service`, launched via PAM) manages that user's units and cgroups; a per-session **session D-Bus** bus carries desktop IPC. [unverified]
3. **Session-manager tier** — `cinnamon-session` holds the logind session, runs the XDG-autostart apps, and is the parent/orchestrator of the DE's own daemons and launchers. [unverified]
4. **User autostart / tray tier** — background apps with tray/status icons: network, Bluetooth, sound/volume, power, updates notifiers, plus Mint's own `mintUpdate`, `mintreport-tray`, `warpinator`, `nvidia-prime`, and the `cinnamon-settings-daemon-*` plugins. [unverified]
5. **DE background-daemon framework tier** — Mint's cross-DE *XApp* layer (`xapp-sn-watcher`, `xdg-desktop-portal-xapp`, `XApp.StatusIcon`) and `nemo-desktop`, the process that owns desktop icons/context menu. [unverified]

Each tier is independently expendable, which is exactly why WuBuOS can model the baseline as a set of **surfaces** it must expose (see §5) rather than as one monolithic runtime. [unverified]

## 1. Session background: cinnamon-session (session manager)

`cinnamon-session` is the session manager of the Cinnamon desktop, and starting it (e.g. `exec cinnamon-session` from `~/.xinitrc`) is how the Cinnamon session is launched [2]. It is "an X11R6 session manager … typically executed by your login manager … It will load either your saved session, or … a default session" defined in a `.session`-like file looked up in the XDG config/data dirs under `cinnamon-session/sessions` [1]. It is DBus-activated/session-scoped, and its autostart task is to "start all applications" defined in the session or in `--autostart=DIR` [1].

On this host the live invocations confirm the two roles running together: [unverified]

- `lightdm` (PID 1302) spawned `lightdm --session-child` (1521) which exec'd `/usr/lib/x86_64-linux-gnu/cinnamon-session-binary --session cinnamon` (PID 1526). [local, raw command output] [unverified]
- `cinnamon-session` is the direct parent of the whole DE process tree: the `csd-*` settings-daemon plugins (screensaver-proxy, settings-remap, automount, print-notifications, a11y-settings, background, clipboard, housekeeping, xsettings, media-keys, keyboard, power, wacom, color), `cinnamon-launcher` → `cinnamon --replace` (the compositor/WM from `cinnamon_study.md`), plus `xapp-sn-watcher`, `blueman-applet`, `nvidia-prime`, `mintwelcome-launcher`, `nemo-desktop`, `evolution-alarm-notify`, and `cinnamon-killer-daemon`. [local, raw command output] [unverified]

The Linux Mint Developer Guide confirms these are the expected post-login processes: "cinnamon-session (the session manager which starts all the other processes), cinnamon (…the visual part), nemo-desktop (which handles the desktop icons and desktop context menu)" [30]. The session is a logind **X11 session** of class=user on seat0: `loginctl` reports `session c1 … seat0 tty7 active`, `Type=x11`, `Class=user`, `Service=lightdm-autologin` [local, raw command output].

## 2. The bus / service tier: systemd + D-Bus + core daemons

### 2.1 systemd as PID 1 and the per-user instance

`systemd` "is a system and service manager … When run as first process on boot (as PID 1), it acts as init system that brings up and maintains userspace services," and "Separate instances are started for logged-in users to start their services" [8]. It is installed as the `/sbin/init` symlink and started during early boot [8]. Those per-user instances are the `user@.service` manager: "The systemd(1) system manager (PID 1) starts user manager instances as user@UID.service" [4].

`systemd --user` lets each user start/stop/enable/disable their own **user units**; the instance is launched by `pam_systemd` on first login, survives as long as the user has a session, and brings up `default.target` [3]. Unit files live in `$HOME/.config/systemd/user/` and are controlled with `systemctl --user` [3]. Crucially, `systemd --user` is **per-user, not per-session** — user services "run outside of a session," so session programs should not be user services [3]. On this host both the system manager and the user manager are live: `systemctl list-units --type=service` shows `user@1000.service … User Manager for UID 1000` (running) and `user-runtime-dir@1000.service … User Runtime Directory /run/user/1000` [local, raw command output].

### 2.2 D-Bus: system bus + session bus

D-Bus is the IPC backbone, "designed for two specific use cases: a 'system bus' for notifications from the system to user sessions … a 'session bus' used to implement desktop environments such as GNOME and KDE" [6]. Each login starts a per-login-session **session bus** whose address is exported in `DBUS_SESSION_BUS_ADDRESS`; the **system bus** is the well-known systemwide bus owning service-activation and routing [6][7]. `dbus-daemon` is the message bus daemon, run with `--system` (systemwide, `/usr/share/dbus-1/system.conf`) or `--session` (per-login-session, `/usr/share/dbus-1/session.conf`) [7].

Live: `dbus.service — D-Bus System Message Bus` runs in the system manager, and `dbus.service — D-Bus User Message Bus` runs in the user manager [local, raw command output]. This is the two-bus model the whole runtime leans on. [unverified]

### 2.3 Core system daemons on the system bus — as actually observed

`systemctl list-units --type=service` on this host (66 loaded units) confirms these always-on, engagement services are `active (running)` [unverified][local, raw command output]:

| Unit (as printed) | Role |
|---|---|
| `systemd-logind.service` | **User Login Management** — keeps track of users/sessions/seats, allocates a per-user slice + session scope, and starts the `user@.service` per-user manager [9][4]. |
| `polkit.service` | **Authorization Manager** — the PolicyKit *Authority*, a service on the system message bus; a session app runs an *Authentication Agent* [10]. |
| `accounts-daemon.service` | **Accounts Service** executable; provides D-Bus interfaces (`org.freedesktop.Accounts`, based on `useradd`/`usermod`/`userdel`) [24]. |
| `NetworkManager.service` + `NetworkManager-wait-online.service` + `NetworkManager-dispatcher.service` | automatic network connection management; dispatcher runs scripts on network events [14][15]. |
| `systemd-resolved.service` | Network Name Resolution — caching/validating DNS stub resolver (127.0.0.53 / .54) + LLMNR/MDNS [18]. |
| `systemd-timesyncd.service` | Network Time Synchronization — SNTP client syncing the system clock [19][20]. |
| `ModemManager.service` | DBus-activated daemon controlling mobile-broadband (2G/3G/4G) devices/connections [25]. |
| `avahi-daemon.service` | Avahi mDNS/DNS-SD stack — zero-config name & service discovery on the LAN [21]. |
| `cups.service`, `cups-browsed.service` | CUPS Scheduler (queueing, filtering, spooling, IPP, the `cupsd` at `localhost:631`) [22]; remote-printer browsing. |
| `bluetooth.service` (`bluetoothd`) | manages all Bluetooth devices, D-Bus services [37]; plus `obexd` for OBEX transfer. |
| `udisks2.service` | **Disk Manager** — `org.freedesktop.UDisks2` daemon for unmounted-volume mounting/auth (polkit-restricted) [16][17]. |
| `upower.service` + `power-profiles-daemon.service` | power/battery/energy daemons feeding the power tray applet. |
| `colord.service` | color-profile management (feeds `csd-color`). |
| `thermald.service`, `irqbalance.service`, `rsyslog.service`, `systemd-journald.service`, `systemd-udevd.service`, `cron.service`, `ufw.service`, `rtkit-daemon.service` | platform/logging/device/scheduling substrate. |

Because these daemons are mostly D-Bus-activated or socket-activated, many more appear only when triggered — `systemd list-sockets` shows listening sockets that activate services on demand, e.g. `cups.socket → cups.service`, `avahi-daemon.socket → avahi-daemon.service`, and `dbus.socket → dbus.service` [local, raw command output]. [unverified]

### 2.4 Core user-session daemons (systemd --user) — as observed

`systemctl --user list-units --type=service` on this host shows the per-user service tier: `dbus.service` (user bus), `dconf.service` (User preferences database), `gnome-keyring-daemon.service`, `at-spi-dbus-bus.service` (assistive-tech bus), `gvfs-*` volume monitors + `gvfs-metadata.service` + `gvfs-udisks2-volume-monitor.service`, `pipewire.service`, `pipewire-pulse.service`, `wireplumber.service`, `filter-chain.service`, `obex.service` (Bluetooth OBEX), the `xdg-desktop-portal*` family (`xdg-desktop-portal.service`, `-gtk`, `-xapp`, `xdg-document-portal.service`, `xdg-permission-store.service`), `evolution-addressbook/calendar/source-registry.service`, and the `flatpak-portal`/`flatpak-session-helper.service` [local, raw command output]. [unverified]

These are exactly the "per-user services … with all the benefits of systemd, such as socket activation, timers, dependency system, and strict process control via cgroups" [3]. PipeWire is the audio/video server whose daemon "can be configured to be both an audio server (with PulseAudio and JACK features) and a video capture server"; WirePlumber is its "powerful session and policy manager," both managed as systemd user units [35][36]. xdg-desktop-portal "expos[es] a series of D-Bus interfaces … under … `org.freedesktop.portal.Desktop`" and is provided with a systemd/user service started via D-Bus [33][34].

## 3. User-session autostart background apps and tray/indicator daemons

The XDG Autostart spec is what turns `.desktop` files in autostart directories into running background apps: "By placing an application's `.desktop` file in one of the [XDG Autostart] directories the application will be automatically launched during startup of the user's desktop environment after the user has logged in." Directories are `$XDG_CONFIG_DIRS/autostart` (system) plus `$XDG_CONFIG_HOME/autostart` (~/.config) per the basedir spec [5]. `cinnamon-session` is the consumer of these on Cinnamon [1][30].

### 3.1 System autostart in `/etc/xdg/autostart` (as observed on this host)

`ls /etc/xdg/autostart` (as root, session-running entries) shows [unverified][local, raw command output]:

- **Tray/indicator daemons:** `nm-applet.desktop` (network), `blueman.desktop` (Bluetooth applet — running as `blueman-applet`), `print-applet.desktop`, `sticky.desktop` [unverified]
- **Mint's own background apps:** `mintupdate.desktop` (→ `mintUpdate` update-manager tray), `mintreport.desktop` (→ `mintreport-tray`), `mintwelcome.desktop`, `warpinator-autostart.desktop` (→ Warpinator LAN file-share), `xapp-sn-watcher.desktop` [unverified]
- **DE chrome/settings plugins:** the long list of `cinnamon-settings-daemon-*.desktop` (a11y-settings, automount, background, clipboard, color, housekeeping, keyboard, media-keys, power, print-notifications, screensaver-proxy, settings-remap, smartcard, wacom, xsettings) [unverified]
- **Keyring/agents:** `gnome-keyring-pkcs11/secrets/ssh.desktop`, `polkit-gnome-authentication-agent-1.desktop`, `polkit-mate-authentication-agent-1.desktop` (authentication agents) [unverified]
- **Portal/aux:** `at-spi-dbus-bus.desktop`, `orca-autostart.desktop` (screen reader), `onboard-autostart.desktop`, `org.gnome.Evolution-alarm-notify.desktop`, `org.gnome.SettingsDaemon.DiskUtilityNotify.desktop`, `geoclue-demo-agent.desktop`, `nvidia-prime.desktop`, `nvidia-settings-autostart.desktop`, `user-dirs-update-gtk.desktop`, `xdg-user-dirs.desktop`, `im-launch.desktop`, `touchegg.desktop` [unverified]

`~/.config/autostart` is empty on this host (no user-added autostart), so the desktop is running purely on the system-wide XDG set [local, raw command output]. [unverified]

### 3.2 What is actually running (ps, as observed)

`ps --forest` under `cinnamon-session` shows the background apps concretely running: `mintUpdate`, `mintreport-tray`, `mintwelcome-launcher → mintwelcome.py → mintinstall`, `blueman-applet`, `blueman-tray`, `nvidia-prime` applet, `xapp-sn-watcher`, `nemo-desktop`, `evolution-alarm-notify`, `geoclue-2.0/demos/agent`, `system-config-printer/applet.py`, and all the `csd-*` settings daemons; plus `csd-printer` (spawned detached) [local, raw command output]. [unverified]

The Mint tools are documented: **mintupdate** "is a central component of the Linux Mint ecosystem," providing "a safe, user-friendly interface for managing software updates from multiple sources, including APT …, Flatpaks, and Cinnamon Spices"; it launches via an autostart `.desktop` and a `mintupdate-launcher` wrapper [27][26]. **mintreport** ("System Reports") "provides system information and helps the user collect information about application crashes" [32]; its tray is a Gtk status-icon app using `XApp.StatusIcon` that monitors whether certain background processes (e.g. automatic upgrades) are running [31]. **warpinator** is launched by `warpinator-autostart.desktop` [5][local].

## 4. Background service contracts the tiers imply

These are the cross-cutting contracts a DE depends on; each is a documented protocol/mechanism, and each lives on a specific tier: [unverified]

- **D-Bus activation** — a `.service`/`SystemdService=` file tells `dbus-daemon` how to start a program "to provide a particular well-known bus name"; the bus launches the service on-demand on first call, optionally delegating to systemd so a daemon is "race-free" when triggered by both boot and activation [7][11]. (e.g. polkit and ModemManager are explicitly D-Bus-activated [25][10].)
- **systemd socket activation** — `.socket` units "encode information about an IPC or network socket … controlled and supervised by systemd, for socket-based activation"; incoming traffic starts a matching `.service` on demand [12][13]. Observed on host: `cups.socket`, `avahi-daemon.socket`, `dbus.socket`, `systemd-udevd.socket` [local, raw command output].
- **XDG Autostart / .desktop** — `.desktop` files in `$XDG_CONFIG_DIRS/autostart` + `$XDG_CONFIG_HOME/autostart` are launched at login for the running user [5] (see §3; Mint's own tools ship such files [27][5]).
- **logind sessions** — logind "keeps track of user logins and seats," allocating a per-user slice + session scope, managing session IDs, and exposing machine power/sleep operations [9]; the session-scope hooks into the per-user manager and the seat/permission model [9][4].
- **polkit authorization** — the *Authority* on the system bus answers "is this request authorized?" for privileged *mechanisms* serving unprivileged clients through D-Bus; a per-session *Authentication Agent* prompts the user when interactive auth is required [10]. Device/mount and power operations funnel through it (udisks, logind) [16][9].
- **Per-user cgroup/service management** — each `user@UID.service` is "a hierarchy of units specific to that user," collected under `user-UID.slice`, giving per-user process control and cgroup isolation [4][3].

## 5. What a DE background-daemon framework looks like (Mint XApp + nemo-desktop)

Mint deliberately shipped a **cross-DE background-app framework** in place of GNOME-only daemons. **XApp** ("Cross-desktop libraries and common resources") "gathers the components which are common to multiple GTK desktop environments (Cinnamon, MATE and Xfce) and required to implement cross-DE solutions"; it renders those common components as `libxapp` (C/Python via GObject-Introspection) plus an `xapp-common` set of resources and tools [28]. On this host the XApp surface is visible as background processes: `xapp-sn-watcher` runs under the session, and `xdg-desktop-portal-xapp.service` runs as the DE's portal backend [local, raw command output]. `mintreport-tray` also uses `XApp.StatusIcon` for its status icon [31].

**nemo-desktop** is the process that owns the desktop layer. The Cinnamon Developer Guide: "nemo-desktop … handles the desktop icons and desktop context menu," and "When you log in, nemo-desktop is started automatically by cinnamon-session"; the `nemo` file-manager process itself only starts while you browse [30]. ArchWiki's Nemo confirms: "the desktop is managed by *nemo-desktop*," which may be auto-started via `nemo-autostart.desktop` [29]. Live: PID 1977 `nemo-desktop` is a direct child of `cinnamon-session`, and a separate `nemo` (PID 4281, under `cinnamon --replace`) handles file browsing [local, raw command output].

## 6. Baseline component table (background components and why a DE needs them)

| Component | Tier | What it runs | Why the DE needs it |
|---|---|---|---|
| `systemd` | PID 1 | init/managers bringing up + supervising userspace services [8] | everything below is a supervised unit |
| `dbus-daemon --system` | system bus | systemwide message bus [7] | hardened IPC bus for privileged daemons |
| `dbus-daemon --session` | session bus | per-login message bus [7] | desktop apps IPC (org.freedesktop.* panels) |
| `systemd-logind` | system daemon | sessions/seats/users, power ops [9] | the session itself + autologin seat |
| `polkit` (Authority) | system daemon | authorization decision engine [10] | safe unprivileged access to devices/system |
| `accounts-daemon` | system daemon | user-account D-Bus service [24] | user manager/login UI data |
| `NetworkManager` + dispatcher | system daemon | auto network connect, event scripts [14][15] | Wired/Wi-Fi configuration for the desktop |
| `systemd-resolved` | system daemon | caching DNS + stub [18] | name resolution for the session |
| `systemd-timesyncd` | system daemon | SNTP clock sync [19][20] | correct clock for files/updates |
| `ModemManager` | system daemon | WWAN/mobile-broadband control [25] | cellular modem support |
| `avahi-daemon` | system daemon | mDNS/DNS-SD discovery [21] | `.local` and printer/cast discovery |
| `cupsd` (cups) | system daemon | print scheduler/IPP [22] | printing |
| `bluetoothd` (+obexd) | system daemon | Bluetooth devices, D-Bus svcs [37] | BT pairing/devices |
| `udisksd` | system daemon | disk/volume operations [16][17] | mount/eject drives via file manager |
| `upower` / `power-profiles-daemon` | system daemon | battery/power/brightness [local] | power tray/battery state |
| `systemd --user` (`user@1000.service`) | user manager | per-user units + cgroups [4][3] | per-user service lifecycle |
| `pipewire` + `pipewire-pulse` | user service | audio server [35] | sound |
| `wireplumber` | user service | PipeWire session/policy manager [36] | audio routing |
| `xdg-desktop-portal*` (+gtk/xapp backends) | user service | portal frontend + DE backends [33][34] | sandboxed apps file/URI access |
| `gnome-keyring-daemon`, `dconf`, `at-spi-dbus-bus` | user service | secrets, prefs db, a11y bus [local] | app prefs/secrets/a11y |
| `gvfs-daemon` + volume monitors | user service | virtual filesystem + mounts [local] | file-management integration |
| `cinnamon-session` | session mgr | holds session, runs autostart, parent of DE daemons [1][30] | orchestrates the whole desktop start |
| `csd-*` (settings-daemon plugins) | session autostart | power/xsettings/clipboard/media-keys/a11y/etc [local] | DE-feature plumbing |
| `nm-applet`, `blueman-applet`, `nvidia-prime`, `print-applet` | session autostart tray | tray/indicator apps [5][local] | status + control UI in panel tray |
| `mintUpdate` (+launcher) | session autostart tray | update-manager auto-refresh + tray [27][26] | keep packages+kernel current |
| `mintreport-tray` | session autostart tray | System Reports status icon, crash info [32][31] | surface crash/attention items |
| `warpinator` | session autostart | LAN file-share daemon [5][local] | Mint's local file sharing |
| `xapp-sn-watcher` / `xdg-desktop-portal-xapp` / `XApp.StatusIcon` | XApp framework | cross-DE background services [28][31] | shared tray/portal plumbing across DEs |
| `nemo-desktop` | DE desktop daemon | desktop icons + context menu [30][29] | the Gnome-like desktop layer |

## 7. Mapping to a WuBuOS `/n/background` surface

If WuBuOS is to present a usable desktop background runtime, it should expose one control surface per contract rather than emulating every daemon. A minimal `/n/background` (9P namespace) baseline: [unverified]

- **`/n/background/system/services`** — list of system services and their run-state, mirroring `systemctl list-units --type=service`; a `state/` + `activate/` per entry so a client can start an on-demand daemon (socket/D-Bus activation equivalent). [unverified]
- **`/n/background/user/services`** — per-user unit registry in the same shape, with `cgroup/<uid>/` exposing per-user process tracking (the `user-UID.slice` contract [4][3]).
- **`/n/bus/`** — two message-bus surfaces: `/n/bus/system` and `/n/bus/session`, exposing name ownership + method/signal routing (the D-Bus dual-bus contract [6][7]).
- **`/n/background/activation/`** — both on-demand activation mechanisms as a single table: `dbus/` (well-known-name → service) and `socket/` (listen-addr → service) so a service is "started lazily on-demand" exactly like systemd [11][12][13].
- **`/n/background/autostart/`** — the XDG autostart contract [5]: read-only system entries (`/etc/xdg/autostart` equivalent) plus writable per-user entries, consumed by the session manager.
- **`/n/session/`** — logind-equivalent: one entry per logged-in session with seat/type/state (the single live session on the host is `c1, seat0, x11, active` [local]), plus power/suspend/reboot operations gated by the auth surface [9].
- **`/n/background/auth/`** — polkit-equivalent authority: subject → action → allow/deny, with a per-session agent registration slot so interactive auth can be surfaced [10].
- **`/n/background/de/`** — the DE daemon framework surface: `/n/background/de/tray` (status-icon ownership, the `XApp.StatusIcon` contract [31][28]) and `/n/background/de/desktop` (desktop-icons/context-menu owner, the `nemo-desktop` role [30][29]).

The hand-off to the existing `/n/wm`/`/n/ui` work in `cinnamon_study.md` is natural: the background study supplies *“is there a service and can I talk to it / start it / be authorized to act on it”*; the chrome study supplies the visual surfaces those services feed. [unverified]

## Notes on sourcing

- All service/autostart/process/session claims are **local, raw command output** from this host (`systemctl list-units --type=service`, `systemctl --user list-units --type=service`, `ls /etc/xdg/autostart`, `ps --forest`, `loginctl`, `cat /etc/os-release`). They captured a live Mint 22.3 `zena` + Cinnamon 6.6.9 desktop. [unverified]
- `power-profiles-daemon`/`upower`/`colord`/`thermald` explainer sentences are covered by the live `systemctl` line naming each unit; their "why the DE needs it" rationale is common Linux-DE background knowledge [unverified].
- The `cinnamon --replace`/Muffin line is treated as settled by `cinnamon_study.md` and not re-cited here. [unverified]

## Sources

[1] https://man.archlinux.org/man/cinnamon-session.1.en — cinnamon-session(1) man page
[2] https://wiki.archlinux.org/title/Cinnamon — ArchWiki: Cinnamon
[3] https://wiki.archlinux.org/title/Systemd/User — ArchWiki: systemd/User
[4] https://manpages.debian.org/unstable/systemd/user@.service.5.en.html — user@.service(5) man page
[5] https://specifications.freedesktop.org/autostart/latest — XDG Desktop Application Autostart Specification
[6] https://dbus.freedesktop.org/doc/dbus-specification.html — D-Bus Specification
[7] https://dbus.freedesktop.org/doc/dbus-daemon.1.xml — dbus-daemon(1) man page
[8] https://www.freedesktop.org/software/systemd/man/260/init.html — systemd init man page
[9] https://man.archlinux.org/man/systemd-logind.8.en — systemd-logind(8) man page
[10] https://manpages.ubuntu.com/manpages/bionic/man8/polkit.8.html — polkit(8) authorization framework man page
[11] https://manpages.debian.org/testing/dbus-daemon/dbus-daemon.1.en.html — dbus-daemon(1) Debian man page (service activation)
[12] https://man.archlinux.org/man/systemd.socket.5 — systemd.socket(5) man page
[13] https://www.freedesktop.org/software/systemd/man/250/systemd.socket.html — systemd.socket(5) freedesktop man page
[14] https://wiki.archlinux.org/title/NetworkManager — ArchWiki: NetworkManager
[15] https://www.networkmanager.dev/docs/api/1.46.2/NetworkManager.html — NetworkManager daemon reference
[16] https://manpages.ubuntu.com/manpages/focal/man8/udisks.8.html — udisks(8) - Disk Manager man page
[17] https://www.freedesktop.org/wiki/Software/udisks — udisks freedesktop wiki
[18] https://manpages.opensuse.org/Tumbleweed/systemd-resolved/systemd-resolved.8.en.html — systemd-resolved(8) man page
[19] https://manpages.ubuntu.com/manpages/lunar/man8/systemd-timesyncd.service.8.html — systemd-timesyncd(8) man page
[20] https://manpages.debian.org/testing/systemd-timesyncd/timesyncd.conf.5.en.html — timesyncd.conf(5) man page
[21] https://deepwiki.com/avahi/avahi/1-avahi-overview — DeepWiki: Avahi overview (mDNS/DNS-SD)
[22] https://www.man7.org/linux//man-pages/man8/cupsd.8.html — cupsd(8) - CUPS scheduler man page
[24] https://www.linuxfromscratch.org/blfs/view/12.2/general/accountsservice.html — LFS: AccountsService (accounts-daemon)
[25] https://www.freedesktop.org/wiki/Software/ModemManager — ModemManager freedesktop wiki
[26] https://github.com/linuxmint/mintupdate — linuxmint/mintupdate GitHub repo
[27] https://deepwiki.com/linuxmint/mintupdate — DeepWiki: linuxmint/mintupdate
[28] https://github.com/linuxmint/xapp — linuxmint/xapp GitHub repo
[29] https://wiki.archlinux.org/title/Nemo — ArchWiki: Nemo
[30] https://linuxmint-developer-guide.readthedocs.io/en/latest/cinnamon.html — Linux Mint Dev Guide: Cinnamon processes
[31] https://github.com/linuxmint/mintreport — linuxmint/mintreport GitHub repo
[32] https://linuxmint-developer-guide.readthedocs.io/en/latest/mint-tools.html — Linux Mint Dev Guide: mintreport
[33] https://wiki.archlinux.org/title/XDG_Desktop_Portal — ArchWiki: XDG Desktop Portal
[34] https://flatpak.github.io/xdg-desktop-portal — xdg-desktop-portal docs
[35] https://wiki.archlinux.org/title/PipeWire — ArchWiki: PipeWire
[36] https://wiki.archlinux.org/title/WirePlumber — ArchWiki: WirePlumber
[37] https://manpages.debian.org/unstable/bluez/bluetoothd.8.en.html — bluetoothd(8) - BlueZ man page
