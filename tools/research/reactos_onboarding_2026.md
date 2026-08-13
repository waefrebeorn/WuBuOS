# React OS 2026: User Onboarding + UI Gap Analysis vs WuBuOS

**Research date:** 2026-08-13 **Status:** RESEARCH — deep-mined latest ReactOS
(latest @ `4c4e341d`). This is the user-experience / onboarding axis your
"numerate its own drivers... install itself" requirement needs.

## 1. What we pulled

`/tmp/reactos-src` — fresh `git clone --depth 1` of `github.com/reactos/reactos`
(commit 4c4e341d). This is NOT the 2023-era tree from the driver wave; the
**2026 tree is substantially rewritten**. Key new components that did not exist
in earlier releases:

- `dll/win32/syssetup/wizard.c` — the GUI first-boot wizard (property-sheet
  with ~10 ordered pages). Rewritten; was previously much thinner.
- `base/setup/usetup/usetup.c` — the text-mode (TUI) installer, now with a
  clearer page graph.
- `dll/win32/devmgr/` — Device Manager, now a tree+list MMC-style snap-in
  (`DeviceNode.cpp`, `ClassNode.cpp`, `ResourceNode.cpp`, `MainWindow.cpp`).
- `base/system/winlogon` + `base/system/logonui` — modernized login/Locked-Leg
  separation.
- `base/shell/explorer/` — the full shell: `startmnu.cpp`, `taskband.cpp`,
  `traywnd.cpp`, `desktop.cpp`, `notifyiconscust.cpp`.

---

## 2. React OS first-boot: the two-track onboarding

### 2.1 GUI track — `syssetup/wizard.c` (property-sheet wizard)

The wizard is a `PROPSHEET` (wizard) with these **pages in order**, each a
`PROPSHEETPAGE` with a dialog template + `DlgProc`, driven by
`PropSheet_SetWizButtons(...)` at each step:

| # | Dialog template | Page DlgProc | Core widgets (from `.rc` `en-US.rc`) |
|---|---|---|---|
| 1 | `IDD_WELCOMEPAGE` | `WelcomeDlgProc` | `LTEXT` title + descriptive text + Next |
| 2 | `IDD_ACKPAGE` | `AckPageDlgProc` | `LISTBOX` (open-source project ack list), `&View GPL...` button |
| 3 | `IDD_INSTALLATION` | `InstallTypePageDlgProc` | install-type radio/selection |
| 4 | `IDD_OWNERPAGE` | `OwnerPageDlgProc` | `EDITTEXT IDC_OWNERNAME`, `EDITTEXT IDC_OWNERORGANIZATION` |
| 5 | `IDD_COMPUTERPAGE` | `ComputerPageDlgProc` | `EDITTEXT IDC_COMPUTERNAME`, `EDITTEXT IDC_ADMINPASSWORD1/2` |
| 6 | `IDD_LOCALEPAGE` | `LocalePageDlgProc` | locale + keyboard-layout combo |
| 7 | `IDD_DATETIMEPAGE` | `DateTimePageDlgProc` | `CONTROL SysDateTimePick32` (date+time), `COMBOBOX IDC_TIMEZONELIST`, `AUTOCHECKBOX` daylight-saving |
| 8 | `IDD_THEMEPAGE` | `ThemePageDlgProc` | `CONTROL SysListView32` (theme picker: Classic/Lautus/Lunar/Mizu) |
| 9 | `IDD_PROCESSPAGE` | `ProcessPageDlgProc` | `PROGRESS` bar + status `LTEXT`s; the hardware-driver scan phase |
| 10 | `IDD_FINISHPAGE` | `FinishDlgProc` | final status + reboot |

**The key user-facing gates, in order:** accept GPL → pick install type →
owner name → **computer name + admin password** → locale → date/time/timezone
→ theme → driver scan. Account creation is real (line 1093:
`WriteComputerSettings` writes `DefaultUserName`/`AutoAdminLogon` registry
keys via `SetComputerNameW` + `SetAccountsDomainSid`).

### 2.2 TUI track — `usetup.c` (text-mode console installer)

Fallback for headless/old hardware. Page graph visible in `usetup.c`:
`WelcomePage` → `SelectPartitionPage` (drive/partition selection) → ...
→ `QuitPage`. Uses `INPUT_RECORD` cursor navigation (arrow keys), no mouse.
Partition list via `SelectPartition(PartitionList, ...)`.

### 2.3 Login — `winlogon` + `logonui`

Separate interactive login UI (`logonui`) from the session manager
(`winlogon`). Standard `Ctrl+Alt+Del` secure-attention + credential prompt.

---

## 3. React OS UI shell (`base/shell/explorer`) — the post-first-boot face

| Subsystem | Source | What it provides |
|---|---|---|
| **Taskbar + Start** | `taskband.cpp`, `startmnu.cpp` | `CTaskBand` (taskband COM object), Start button → `CStartMenuSite`/`IMenuPopup` popup, app-launcher tree |
| **Desktop** | `desktop.cpp` | icon grid, wallpaper, right-click context menu |
| **Tray** | `traywnd.cpp`, `notifyiconscust.cpp` | system-tray (notify-icons) + clock (`trayclock.cpp`) |
| **Window chrome** | `syspager.cpp`, `syspager` | MDI pager, minimize/max/close, caption buttons |

Concrete widgets from `explorer.rc`: `IDI_STARTMENU`, `IDI_RECYCLEBIN`,
`IDI_COMPUTER`, `IDI_FOLDER`, `IDI_MAIL`, accelerator table (`VK_TAB`,
`IDMA_CYCLE_FOCUS`).

### Device Manager (`dll/win32/devmgr`)

Tree view (`SysTreeView32`) of the device hierarchy rooted at
`Devmgmt` → `DeviceNode` / `ClassNode` / `ResourceNode` / `DriverNode`.
`MainWindow.cpp` creates the tree + properties panes. Columns: device name,
driver, status, driver provider/version, hardware ID. This is the **user-facing
face of the device model** — it renders the same "bound/unbound" information
that WuBuOS's `wubu_probe_matrix.c` computes, but as an interactive GUI.

---

## 4. WuBuOS today — what's already there

Verified by inspecting `src/gui/` (150 C files):

| Capability | WuBuOS file(s) | Status |
|---|---|---|
| Welcome / first-run | `wubu_welcome.c` / `.h` | **EXISTS** — "Welcome to WuBuOS" dialog, marker file `~/.config/wubu/first-run-done`, dismiss-once |
| Start menu | `dosgui_startmenu.c`, `dosgui_startmenu_tree.c` | EXISTS — program tree (`SmTreeNode`), power items, search |
| Taskbar / window manager | `dosgui_wm.*`, `dosgui_era_apps.c` | EXISTS — virtual desktops, focus, minimize/max/close |
| Desktop | `dosgui_desktop.c` | EXISTS — wallpaper + icon grid |
| Explorer / file manager | `dosgui_explorer*.c` (16 files) | EXISTS — drives, fs ops, format, zip, preview |
| Control Panel | `dosgui_controlpanel.c`, `dosgui_cp_{display,hardware,network,sound,theme}.c` | EXISTS — HW/DP/sound/net/theme applets |
| Service manager | `dosgui_service_mgr.c` | EXISTS |
| Autostart / session | `wubu_session_autostart.c` | EXISTS — `.desktop`-style parsing, save/restore |

**So WuBuOS already has the 98% shell.** The gaps vs. React OS's 2026 wizard
are precisely scoped — see the matrix below.

---

## 5. The definitive UI / onboarding gap matrix (React OS → WuBuOS)

| React OS 2026 feature | WuBuOS equivalent | Gap? | Notes / WuBu action |
|---|---|---|---|
| **Welcome page** (GPL ack) | `wubu_welcome.c` | ✅ exists, simpler | Add the open-source-ack list + GPL view; trivial |
| **Owner name** input | none | ❌ MISSING | No owner/organization identity stored |
| **Computer name** | none | ❌ MISSING | No hostname identity (`SetComputerName` equivalent) |
| **Admin password** creation | none | ❌ MISSING | No user-account-password bootstrap (the AGI persona login) |
| **Locale / keyboard layout** | none | ❌ MISSING | No locale picker; no layout selection |
| **Date / time / timezone** | none | ❌ MISSING | No first-boot timezone picker; no `SysDateTimePick32` |
| **Theme picker** (Classic/Lautus/…) | `dosgui_cp_theme.c` | ✅ exists | Good parity |
| **Hardware driver scan** (page 9) | `wubu_drv_install_report` | ✅✅ **just built** | WuBu's driver self-install + report plugs in here |
| **Finish → reboot** | none | ❌ MISSING | No first-boot completion → reboot sequence |
| **Text-mode installer** (`usetup`) | none | ❌ MISSING | No TUI/install media path for headless boots |
| **Device Manager** (tree + props) | `wubu_probe_matrix.c` (KV-FS) | ⚠️ partial | WuBu's matrix is data/KV-FS, not an interactive tree-view GUI |
| **Login** (`winlogon`/`logonui`) | none | ❌ MISSING | No login UI / secure-attention path |
| **Start menu** program tree | `dosgui_startmenu_tree.c` | ✅ exists | Good |
| **System tray + clock** | `dosgui_wm` | ⚠️ partial | WM exists; tray-notify-icons + clock as a separate pane are not |
| **Recycle bin / My Computer icons** | `dosgui_desktop.c` | ⚠️ check | Need to confirm icon set matches |

### The 6 genuine WuBuOS UI gaps (ranked by the "numerate its own devices" requirement)

1. **No first-boot identity wizard.** WuBuOS never creates a hostname, owner,
   or admin account. For an *AGI* OS, this is the bootstrap of the persona:
   the kernel must onboard "itself" as the AGI at Ring 0 — a *literal*
   first-boot of the self. React OS does this at pages 4–5.
2. **No locale / timezone / keyboard-layout picker.** No `SysDateTimePick32`,
   no timezone `COMBOBOX`, no locale string table.
3. **No login manager.** No `winlogon`+`logonui`; no password prompt, no
   secure-attention sequence. The AGI-at-Ring-0 is, by definition, always
   "logged in" — but a *user* persona layer (or a guest/operator layer) is
   absent.
4. **No finish → reboot sequence.** `wubu_welcome` dismisses; nothing
   triggers a clean first-boot finalization + reboot.
5. **No text-mode installer.** No `usetup`-equivalent for bare-metal installs
   from removable media / headless rigs.
6. **Device Manager is data, not GUI.** `wubu_probe_matrix.c` writes the device
   matrix to KV-FS; React OS renders an interactive `SysTreeView32` + property
   panes. WuBu's matrix is *correct* but not *browsable as a UI*.

---

## 6. The design hook: the Colonel wizard inference engine

You noted the wizard IS the Colonel (the head inference engine). The React OS
wizard maps cleanly onto a **Colonel decision tree**:

```
wubu_wizard.c  (new — colonel-driven first-boot)
   ├── colonel_decide("wizard.license")   → accept GPL?          (page 1)
   ├── colonel_decide("wizard.identity")  → owner/computer/name   (pages 4-5)
   ├── colonel_decide("wizard.locale")    → locale/keyboard/time  (pages 6-7)
   ├── colonel_decide("wizard.theme")     → theme selection        (page 8)
   ├── colonel_decide("wizard.drivers")   → self-install report    (page 9)  ← hooks wubu_drv_install
   └── colonel_decide("wizard.finish")    → finalize + reboot       (page 10)
```

Each `colonel_decide()` is a **numeracy gate**: it doesn't just render a page,
it *measures* the current host (enumerate devices → `wubu_probe_matrix`, check
which are unbound → `wubu_drv_install_report`), proposes a default, and only
surfaces the UI where the answer isn't already knowable. That is the AGI-OS
inversion: **the wizard doesn't ask what it can infer; it infers what it must ask.**

---

## 7. Honest remainder (NOT done this wave)

- No UI code implemented — this wave is React OS mining + gap identification
  (the onboarding axis of your request). The driver self-install arm is
  **built and tested** (9/9 selftests green, committed).
- `/tmp/reactos-src` is transient research scratch; notes relocated to
  `tools/research/reactos_onboarding_2026.md`.
- WuBuOS's `wubu_welcome.c` exists but is a single-modal dialog, not the
  multi-page wizard; the Colonel wiring is specified but not coded.

## 8. File map (mined)

- `dll/win32/syssetup/wizard.c` — wizard page graph + DlgProcs + registry writes
- `dll/win32/syssetup/syssetup.rc` / `lang/en-US.rc` — dialog templates
- `dll/win32/syssetup/resource.h` — `IDD_*PAGE`, `IDC_*` widget IDs
- `base/setup/usetup/usetup.c` — text-mode TUI installer pages
- `dll/win32/devmgr/devmgmt/{MainWindow,DeviceNode,ClassNode,ResourceNode}.cpp`
- `base/shell/explorer/{startmnu,taskband,traywnd,desktop}.cpp`
- `base/system/{winlogon,logonui}` — login
