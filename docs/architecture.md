# Architecture & System Design

This document details the high-level architecture, module boundaries, directory tree, and data flow of the **Lenovo Yoga Book 1st Gen (YB1-X91F / YB1-X90F)** configuration repository.

---

## 1. High-Level Architecture Overview

The system runs **Arch Linux** under the **MangoWC** Wayland compositor (wlroots-based) with a modular, touch-optimized user space designed specifically for the resource-constrained Intel Atom x5-Z8550 SoC (4 cores, 4GB RAM, eMMC).

The architecture prioritizes:
- **Zero Idle Resource Consumption**: Daemons are event-driven or socket/trigger-based, avoiding polling loops.
- **Native C Implementation**: Critical daemons, launchers, status providers, and control centers are written in pure C (using GTK 3, libudev, sd-bus, and libinput) rather than Python or Web/Electron frameworks, keeping RAM usage to <1 MB per daemon and cold launch times <10 ms.
- **Hardware Integration**: Custom low-level hooks handle unique hardware quirks (Wacom digitizer rotation, dual accelerometer hinge angle calculation, PE+ 24W charging negotiation, Landlock sandboxing).

```
+-----------------------------------------------------------------------+
|                              MangoWC (wlroots)                         |
|                    (DSI-1 Landscape 270°, scale 1.5)                  |
+-----------------------------------+-----------------------------------+
                                    |
          +-------------------------+-------------------------+
          |                         |                         |
          v                         v                         v
   Waybar (Top Bar)        Control Center (Native C)    App Launcher (Native C)
(Modules: Audio, Bat,      (GTK 3 + Layer Shell,        (GTK 3 + Layer Shell,
 Wi-Fi, Workspaces)         Volume, Brightness, Toggles) Search & Grid)
          ^                         ^
          |                         |
+---------+-------------------------+-----------------------------------+
|               Event Daemons & Hardware Providers (Native C)            |
| - yogabook-autorotate       : Dual IIO accelerometer hinge math & rot  |
| - yogabook-autobrightness   : ALS perceptual curve & user bias         |
| - yogabook-powerd           : SOC monitoring & UVLO emergency shutdown |
| - yogabook-battery-status   : Direct BQ27542 fuel gauge JSON provider   |
| - yogabook-wifi-status      : Direct iwd/netlink status provider       |
| - yogabook-bt-audio-watcher : Automatic Bluetooth audio routing        |
| - wvkbd                     : Virtual on-screen touch keyboard         |
+-----------------------------------------------------------------------+
```

---

## 2. Directory Structure

```text
yogabook-config/
├── README.md                      # Setup and usage guide
├── AGENTS.md                      # Agent rules and operational guidelines
├── YOGABOOK_SETUP_REPORT.md       # Technical setup history & hardware quirks
├── Makefile                       # Top-level recursive build manager
├── install.sh                     # Idempotent symlinker for user dotfiles
│
├── bin/                           # User executables (~/.local/bin/ symlinks)
│   ├── yogabook-autorotate        # Smart posture & auto-rotation daemon (C)
│   ├── yogabook-autobrightness    # Smart ALS display auto-brightness daemon (C)
│   ├── yogabook-control-center    # Touch Control Center overlay (GTK 3 C)
│   ├── toggle-control-center      # Instant toggle binary for Control Center (C)
│   ├── yogabook-launcher          # Touch App Launcher overlay (GTK 3 C)
│   ├── toggle-launcher            # Instant toggle binary for App Launcher (C)
│   ├── yogabook-mako-status       # Notification & DND status provider (C)
│   ├── yogabook-battery-status    # BQ27542 SOC battery status provider (C)
│   ├── yogabook-wifi-status       # Direct hardware/iwd Wi-Fi status provider (C)
│   ├── yogabook-powerd            # Low-battery alert & anti-brownout daemon (C)
│   ├── yogabook-charger-negotiate # PE+ 24W charging & sleep-hold negotiator
│   ├── toggle-keyboard            # Instant toggle binary for wvkbd (C)
│   ├── close-window               # Safe IPC window close binary for MangoWC (C)
│   ├── ws-status                  # Dynamic workspace status provider (C)
│   ├── mango-workspace-watcher    # MangoWC workspace event watcher (C)
│   ├── yogabook-bt-audio-watcher  # Bluetooth audio auto-route daemon (C)
│   └── wvkbd                      # Virtual on-screen keyboard (C)
│
├── src/                           # Native C source trees
│   ├── Makefile                   # Master build file for all C submodules
│   ├── control-center/            # Native C Control Center (GTK 3 + Layer-Shell)
│   ├── launcher/                  # Native C App Launcher (GTK 3 + Layer-Shell)
│   ├── autorotate/                # Accelerometer hinge angle & rotation math
│   ├── autobrightness/            # ALS perceptual auto-brightness daemon
│   ├── powerd/                    # Power & anti-brownout protection daemon
│   ├── helpers/                   # Micro-helpers and watchers
│   ├── touch-keyboard/            # Halo Keyboard driver source & fix
│   └── wvkbd/                     # Custom touch layout virtual keyboard
│
├── config/                        # User dotfiles (~/.config/ symlinks)
│   ├── mango/                     # MangoWC compositor configurations
│   ├── waybar/                    # Waybar status bar configuration & GTK CSS
│   ├── mako/                      # Mako notification daemon configuration
│   ├── easyeffects/               # PipeWire DSP audio presets
│   └── systemd/user/              # User-level systemd service definitions
│
├── system/                        # System-level fixes and configurations (/etc)
│   ├── pamac_fix/                 # Landlock sandbox bypass for Pacman/Pamac
│   ├── wacom_fix/                 # LD_PRELOAD coordinate rotation hook for Wacom
│   └── etc/                       # Canonical system configurations (fstab, mkinitcpio)
│
└── docs/                          # Project documentation
    ├── architecture.md            # This file: Architecture & module layout
    ├── historical-errors.md       # Knowledge base of technical challenges & patterns
    └── TODO-KERNEL-PMIC-WAKEUP.md # PMIC / kernel standby notes
```

---

## 3. Module Boundaries & Data Flow

### 3.1 Graphical & UI Stack
- **Styling Architecture**: UI widgets are rendered natively via **GTK 3** and styled using **GTK CSS** (`GtkCssProvider`).
  - Waybar: Styled via `config/waybar/style.css`.
  - Control Center: Native C application injecting `CSS_DATA` via `GtkCssProvider`.
  - Launcher: Native C application injecting `CSS_DATA` via `GtkCssProvider`.
  - Notification Daemon: Mako styled via `config/mako/config`.
- **Inter-Process Communication**:
  - Waybar communicates with native C status helpers via stdout JSON streams and real-time UNIX signals (`SIGRTMIN+x`).
  - The Control Center and Launcher are triggered instantaneously via lightweight C CLI binaries (`toggle-control-center`, `toggle-launcher`) that send UNIX signals or launch single instances via layer-shell.

### 3.2 Sensor & Hardware Data Flow
- **Auto-Rotation**:
  - `yogabook-autorotate` reads dual accelerometers via `libudev` and `sysfs` IIO triggers.
  - Computes 2D projection angles to determine hinge position ($0^\circ-360^\circ$).
  - Invokes `mmsg` IPC to update MangoWC transform, sets `/run/user/<uid>/yogabook-rotation.state`, and controls touch keyboard state via `ioctl(EVIOCGRAB)`.
- **Wacom Digitizer Correction**:
  - `wacom_fix.so` is preloaded into the compositor process via `/etc/ld.so.preload`.
  - Intercepts `wlr_cursor_warp_absolute`, reads rotation state cache, and applies coordinate transforms in real time.
- **Power & Charging**:
  - `yogabook-powerd` monitors fuel gauge capacity directly from sysfs.
  - Signals desktop notifications and executes emergency shutdown if cell voltage drops below 3.15V.
  - `yogabook-charger-negotiate` inhibits suspend to allow the hardware PE+ 12V workqueue to complete negotiation.
