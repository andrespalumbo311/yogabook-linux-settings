# AGENTS.md – Guide & Architecture Reference for AI Coding Assistants

Welcome, Agent. This repository contains the complete configuration, hardware workarounds, daemon scripts, and dotfiles for running **Arch Linux** on the **Lenovo Yoga Book 1st Gen (YB1-X91F / YB1-X90F series)** under **MangoWC** (lightweight wlroots Wayland compositor) and the **Modular Touch Stack (Waybar, Mako, Native C Control Center & Launcher)**.

This file serves as your primary context, architectural overview, and operational guidelines when maintaining, debugging, or extending this codebase.

---

## 🎯 Repository Purpose

The Lenovo Yoga Book is a unique 2-in-1 device featuring:
- An ultra-low-power **Intel Atom x5-Z8550 (Cherry Trail / Cherryview Gen8 graphics)** SoC.
- A 10.1" native portrait IPS display (1200×1920) operated in landscape (`transform 270`).
- A bottom half comprising an electromagnetic **Wacom EMR Create Pad** and touch-sensitive **Halo Keyboard**.
- Dual 3D accelerometers (`iio:device*`) in the screen and keyboard base.
- Connected Standby (`s2idle`) with SoC hardware PWM backlight control.

This repository provides production-ready, low-overhead solutions to turn this challenging hardware into a responsive, stable, and daily-drivable Linux workstation.

---

## 📂 Architecture & Directory Layout

All active user configurations on the system are **symbolic links** pointing into this repository:

```text
yogabook-config/
├── README.md                      # Human-facing setup and usage guide
├── AGENTS.md                      # This file: Agent instructions and architecture guide
├── YOGABOOK_SETUP_REPORT.md       # Full historical & technical setup report
├── Makefile                       # Top-level build system for all native C components
├── install.sh                     # Idempotent linker script (~/ paths to repo)
│
├── bin/                           # User executables (symlinked to ~/.local/bin/)
│   ├── yogabook-autorotate        # Smart posture & auto-rotation daemon (Native C, 540 KB RAM)
│   ├── yogabook-autobrightness    # Smart ALS display auto-brightness daemon (Native C, 436 KB RAM)
│   ├── yogabook-control-center    # Native C touch Control Center (GTK 3 + Layer-Shell, 0 MB idle)
│   ├── toggle-control-center      # Instant toggle binary for Control Center (Native C)
│   ├── yogabook-launcher          # Native C touch App Launcher (GTK 3 + Layer-Shell, 0 MB idle)
│   ├── toggle-launcher            # Instant toggle binary for App Launcher (Native C)
│   ├── yogabook-mako-status       # Real-time Mako notification & DND status provider (Native C)
│   ├── toggle-keyboard            # Instant toggle binary for wvkbd virtual keyboard (Native C)
│   ├── close-window               # Safe IPC window close binary for MangoWC (Native C)
│   ├── ws-status                  # Dynamic workspace status provider for Waybar (Native C)
│   ├── mango-workspace-watcher    # MangoWC event watcher daemon for Waybar (Native C)
│   └── wvkbd                      # Native C Touch Virtual Keyboard (custom Yoga Book layout, 1.1 MB RAM)
│
├── src/                           # Native C source trees
│   ├── Makefile                   # Recursive build manager for all submodules
│   ├── control-center/            # Native C Control Center source and Makefile
│   ├── launcher/                  # Native C Application Launcher source and Makefile
│   ├── autorotate/                # Native C Smart Auto-Rotation source and Makefile
│   ├── autobrightness/            # Native C Smart Auto-Brightness source and Makefile
│   ├── helpers/                   # Native C micro-helpers and watchers source and Makefile
│   ├── touch-keyboard/            # Native C++ Halo Keyboard driver source & fix (490 KB RAM)
│   └── wvkbd/                     # Native C Virtual Keyboard source, custom layout and Makefile
├── config/                        # User dotfiles (symlinked to ~/.config/)
│   ├── mango/
│   │   ├── config.conf            # MangoWC compositor config (GPU/CPU optimized, Wacom mapping)
│   │   ├── outputs.conf           # Display mode & 270° transform (scale 1.5)
│   │   ├── cursor.conf            # Trackpad natural scrolling & cursor size
│   │   └── binds.conf             # Keyboard, multimedia & touch gesture bindings
│   ├── waybar/
│   │   ├── config.jsonc           # Waybar top-bar (launcher, workspaces, clock, audio, battery, tray)
│   │   └── style.css              # Touch-friendly Modern Light stylesheet
│   ├── mako/
│   │   └── config                 # Mako lightweight notification daemon config (Material Light)
│   ├── easyeffects/
│   │   └── output/                # PipeWire DSP presets (YogaBook-Speakers, LoudnessEqualizer)
│   └── systemd/
│       └── user/
│           ├── rot8.service       # User systemd service for yogabook-autorotate
│           ├── yogabook-autobrightness.service # User systemd service for auto-brightness
│           ├── wvkbd.service      # User systemd service for wvkbd (hidden background process)
│           ├── waybar.service     # User systemd service for Waybar
│           ├── mako.service       # User systemd service for Mako notification daemon
│           └── easyeffects.service# User systemd service for EasyEffects headless DSP daemon
│
└── system/                        # System configurations and low-level fixes (/etc)
    ├── pamac_fix/                 # C source & Makefile for libalpm Landlock sandbox bypass
    │   ├── pamac_fix.c
    │   └── Makefile
    └── etc/                       # Canonical copies of tuned system files
        ├── fstab                  # Flash eMMC wear & latency optimizations (noatime, commit=60)
        ├── mkinitcpio.conf        # Early KMS and PWM modules (pwm_lpss, pwm_lpss_platform, i915)
        ├── ld.so.preload          # Preload entry for pamac_fix.so
        ├── sysctl.d/
        │   └── 99-zram-performance.conf # ZRAM swappiness=180 and dirty memory flush limits
        ├── systemd/
        │   ├── journald.conf.d/
        │   │   └── 00-size-limit.conf   # Caps systemd journal to 50MB to prevent storage exhaustion
        │   └── logind.conf.d/
        │       └── yogabook.conf        # Lid close & power button mapped to suspend
        └── touch_keyboard/
            ├── touch-hw.csv       # Physical dimensions & 270° orientation of Halo Keyboard
            └── layout.csv         # Active Halo Keyboard layout
```

---

## ⚙️ Core Subsystems & Technical Details

### 1. Smart Auto-Rotation (`bin/yogabook-autorotate` – Native C)
- **Source**: `src/autorotate/main.c` (compiled with `-O2 -ludev -lm`, ~540 KB resident RAM).
- **Sensors**: Resolves screen vs base accelerometers via `libudev` property (`ACCEL_LOCATION=base` is physically the DISPLAY due to Lenovo firmware quirks).
- **2D Hinge Projection**: Projects gravity vectors onto the plane perpendicular to the hinge axis (X-Z cross-section). Computes real hinge opening ($0^\circ-360^\circ$) completely immune to roll tilt up to $70^\circ+$.
- **Hinge Singularity Guard**: Freezes the mode (laptop vs tablet) when gravity aligns with the hinge axis (Y), preventing spurious flips when lifting or tilting the device.
- **Hysteresis**:
  - Laptop mode: opening $\le 152.0^\circ$ and base resting horizontal ($b_z < -0.35g$). Locks screen to landscape (`270`), disables auto-rotation.
  - Tablet mode: opening $\ge 160.0^\circ$ (or flipped). Enables auto-rotation.
- **Table Flat-Lock**: If screen is tilted $< 53^\circ$ from horizontal ($|z| > 0.60$), orientation freezes completely so resting the device flat on a desk preserves the active orientation.
- **Halo Keyboard Suppression**: Directly grabs/ungrabs `/dev/input/event*` nodes via `ioctl(EVIOCGRAB)` when opening $\ge 190.0^\circ$.

### 2. Wacom Create Pad Digitizer (`config/mango/config.conf`)
- Hardware ID: `Wacom HID 169 Pen` (`0018:056A:0169` via `i2c-WCOM0019:00`).
- The digitizer is physically installed in portrait (1200×1920) matching the display panel.
- In `config.conf`, `tablet_map_to_mon=DSI-1` maps the digitizer directly to the rotated display, resolving coordinate mismatches.

### 3. Standby & Backlight Recovery (`system/etc/mkinitcpio.conf`)
- The Cherry Trail SoC PWM hardware controller must be initialized before the `i915` driver binds to the `DSI-1` display.
- `MODULES=(pwm_lpss pwm_lpss_platform i915)` in `/etc/mkinitcpio.conf` ensures the `intel_backlight` device is available, preventing the black screen on resume from `s2idle` suspend.

### 4. Pacman 7 / Pamac Landlock Fix (`system/pamac_fix/`)
- Pacman 7 enforces Landlock filesystem sandboxing by default. On kernels lacking Landlock support, `alpm_sandbox_setup_child` fails.
- `pamac_fix.so` intercepts this call and returns `0` (success), enabling `libpamac` and graphical app store updates without error.

### 5. Fast Charging & Sleep-Hold Negotiator (`bin/yogabook-charger-negotiate`)
- The Yoga Book uses a Texas Instruments BQ25892 charger paired with the official Lenovo 24W MediaTek Pump Express+ (PE+) adapter.
- The 12V / 11.3V voltage negotiation is driven in software by the kernel workqueue `bq25890_pump_express_work`.
- In connected standby (`s2idle`) with the lid closed, `systemd-logind` would immediately re-suspend before the 5-second PE+ negotiation timer could elapse, trapping the charger in 5V / 500mA (~2W) trickle mode.
- `bin/yogabook-charger-negotiate` and `yogabook-charge-negotiate.service` (triggered by `65-yogabook-charging.rules`) acquire a `systemd-inhibit` lock on `handle-lid-switch:sleep` for up to 25 seconds upon AC plug. This holds the SoC awake long enough to lock in 12V (24W) high-voltage fast charging before returning cleanly to sleep.

### 6. Smart Auto-Brightness Daemon (`bin/yogabook-autobrightness` – Native C)
- **Source**: `src/autobrightness/main.c` (compiled with `-O2 -lsystemd -lm`, ~436 KB resident RAM).
- Leverages the ambient light sensor (ALS) exposed via `iio-sensor-proxy` (`net.hadess.SensorProxy`) directly via `sd-bus`.
- Event-driven D-Bus subscriber with perceptual human-eye LUT curve, exponential moving average (EMA) noise filtration, and deadband hysteresis ($3\%$).
- Implements fluid stepped ramping ($1\%$ per 30ms) to eliminate eye strain and abrupt lighting jumps.
- Supports adaptive user bias: automatically detects manual slider adjustments in the Control Center / Settings and preserves the offset relative to the curve across lighting transitions.

---

## 🤖 Instructions for AI Agents

When working on this repository, you **MUST** follow these operating rules:

1. **Always Edit Repo Files First**:
   - Because the system configurations are symlinks to this repository, make your modifications directly inside `~/yogabook-config/...`.
   - Never overwrite symlinks in `~/.config/` or `~/.local/bin/` with regular files.

2. **Reloading Services After Edits**:
   - **MangoWC**: `WAYLAND_DISPLAY=wayland-0 mmsg dispatch reload_config`
   - **Waybar**: `killall -SIGUSR2 waybar` or `systemctl --user restart waybar.service`
   - **Mako**: `makoctl reload` or `systemctl --user restart mako.service`
   - **Rotation Daemon**: `systemctl --user restart rot8.service`
   - **Auto-Brightness**: `systemctl --user restart yogabook-autobrightness.service`
   - **Virtual Keyboard**: `systemctl --user restart wvkbd.service`
   - **EasyEffects**: `systemctl --user restart easyeffects.service` or `easyeffects -l <preset>`
   - **Systemd User Units**: `systemctl --user daemon-reload`

3. **System Files (`/etc`) Safety**:
   - Never replace `/etc/fstab` or `/etc/mkinitcpio.conf` with symlinks into `/home/andres/`.
   - Keep canonical copies in `system/etc/` and use `install.sh --system` or `sudo install` to synchronize.

4. **Public Repository & Security Protocol**:
   - **ZERO SENSITIVE DATA**: This repository is published publicly on GitHub.
   - **NEVER** commit passwords, sudo credentials, personal access tokens, SSH private keys, API keys, or private IP networks.
   - Maintain documentation integrity and keep commit messages clear following Conventional Commits (`feat:`, `fix:`, `refactor:`, `docs:`).

5. **Language & Architecture Best Practice: Avoid Python, Favor Native C**:
   - **Hardware Constraints**: The Yoga Book runs on an ultra-low-power Intel Atom x5-Z8550 with 4GB LPDDR3 and eMMC storage.
   - **RAM & CPU Impact**: Every resident Python daemon wastes 20–35 MB of fixed RAM for the CPython runtime and introduces periodic garbage collection wakeups. Native C implementations consume only **~400–600 KB of RAM** and run at **0% idle CPU**.
   - **Interactive Latency**: Interactive touch/gesture helpers and Waybar providers written in Bash suffer 15–30 ms fork/exec overhead on low-IPC Atom cores. Native C binaries execute in **<0.5 ms**.
   - **C-Native First**: For all background services, status watchers, and interactive helpers, **strictly avoid Python and Bash pipelines**. Implement them in clean, single-threaded native C inside [`src/`](file:///home/andres/yogabook-config/src/) integrated into the top-level [`Makefile`](file:///home/andres/yogabook-config/Makefile).
   - **Avoid Rust on Device**: Avoid introducing Rust on-device; `cargo` builds trigger severe thermal throttling on the fanless chassis, consume gigabytes of eMMC storage, and thrash ZRAM swap. In contrast, `gcc -O2` compiles the entire native C stack in under 2 seconds.
