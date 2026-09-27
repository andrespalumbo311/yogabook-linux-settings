# Lenovo Yoga Book (YB1-X91F) – Linux Configurations & Dotfiles

This repository contains the official configuration files, custom scripts, system services, and hardware/software optimizations developed for the **Lenovo Yoga Book 1st Gen (YB1-X91F / YB1-X90F series)** running **Arch Linux** under **MangoWC** (lightweight wlroots Wayland compositor) and a high-efficiency **Modular Touch Stack (Waybar, Mako, Native C Control Center & App Launcher)**.

All active user configurations on the system (`~/.config/`, `~/.local/bin/`) are **symbolic links** pointing to this repository. Any changes made inside this repository reflect instantly on the running system.

---

## 📁 Repository Structure

```text
yogabook-config/
├── README.md                      # Usage guide and project overview
├── AGENTS.md                      # Architecture guide and instructions for AI agents
├── YOGABOOK_SETUP_REPORT.md       # Full technical report detailing all hardware interventions
├── Makefile                       # Root Makefile to build all C submodules
├── install.sh                     # Idempotent linker script to set up/restore all symlinks
│
├── bin/                           # User executables (symlinked to ~/.local/bin/)
│   ├── yogabook-autorotate        # Smart posture & auto-rotation daemon (Native C, 540 KB RAM)
│   ├── yogabook-autobrightness    # Smart ALS display auto-brightness daemon (Native C, 436 KB RAM)
│   ├── yogabook-control-center    # Native C touch Control Center (GTK 3 + Layer-Shell, 0 MB idle)
│   ├── toggle-control-center      # Instant toggle binary for Control Center (Native C)
│   ├── yogabook-launcher          # Native C touch App Launcher (GTK 3 + Layer-Shell, 0 MB idle)
│   ├── toggle-launcher            # Instant toggle binary for App Launcher (Native C)
│   ├── yogabook-mako-status       # Real-time Mako notification & DND status provider (Native C)
│   ├── yogabook-settings          # Libadwaita native settings application
│   ├── yogabook-display-mgr       # Micro-HDMI hotplug & display topology manager
│   ├── yogabook-charger-negotiate # Pump Express 12V fast charging sleep-hold negotiator
│   ├── toggle-keyboard            # Instant toggle binary for wvkbd virtual keyboard (Native C)
│   ├── close-window               # Safe IPC window close binary for MangoWC (Native C)
│   ├── ws-status                  # Dynamic workspace status provider for Waybar (Native C)
│   ├── mango-workspace-watcher    # MangoWC event watcher daemon for Waybar (Native C)
│   └── wvkbd                      # wvkbd-mobintl binary (compiled for minimal footprint)
│
├── src/                           # Native C source trees
│   ├── Makefile                   # Recursive build manager for all submodules
│   ├── control-center/            # Native C Control Center source and Makefile
│   ├── launcher/                  # Native C Application Launcher source and Makefile
│   ├── autorotate/                # Native C Smart Auto-Rotation source and Makefile
│   ├── autobrightness/            # Native C Smart Auto-Brightness source and Makefile
│   └── helpers/                   # Native C micro-helpers and watchers source and Makefile
│
├── config/                        # User dotfiles (symlinked to ~/.config/)
│   ├── mango/
│   │   ├── config.conf            # Ultra-low-power MangoWC compositor config (Wacom mapping, rounded corners)
│   │   ├── outputs.conf           # Display mode & 270° transform (scale 1.5)
│   │   ├── cursor.conf            # Trackpad natural scrolling & cursor size
│   │   └── binds.conf             # Keyboard, multimedia & touch gesture bindings
│   ├── waybar/
│   │   ├── config.jsonc           # Waybar top-bar (launcher, workspaces, clock, audio, battery, tray)
│   │   └── style.css              # Touch-friendly Material Light stylesheet
│   ├── mako/
│   │   └── config                 # Mako lightweight notification daemon config (Material Light)
│   ├── easyeffects/
│   │   └── output/                # PipeWire DSP presets (YogaBook-Speakers, LoudnessEqualizer)
│   └── systemd/
│       └── user/
│           ├── rot8.service       # User systemd service for yogabook-autorotate
│           ├── yogabook-autobrightness.service # User systemd service for auto-brightness
│           ├── wvkbd.service      # User systemd service for wvkbd (--hidden background daemon)
│           ├── waybar.service     # User systemd service for Waybar
│           ├── mako.service       # User systemd service for Mako notification daemon
│           └── easyeffects.service# User systemd service for EasyEffects headless DSP daemon
│
└── system/                        # System configuration files and low-level patches (/etc)
    ├── pamac_fix/                 # C source & Makefile to bypass Landlock sandbox limit on Pacman 7 / libpamac
    │   ├── pamac_fix.c
    │   └── Makefile
    └── etc/                       # Canonical copies of tuned system files
        ├── fstab                  # Flash eMMC wear & latency optimizations (noatime, commit=60)
        ├── mkinitcpio.conf        # Early KMS & PWM modules (pwm_lpss, pwm_lpss_platform, i915) for backlight recovery
        ├── ld.so.preload          # Preload entry for pamac_fix.so
        ├── sysctl.d/
        │   └── 99-zram-performance.conf # ZRAM swappiness=180 and dirty memory flush limits
        ├── systemd/
        │   ├── journald.conf.d/
        │   │   └── 00-size-limit.conf   # Caps systemd journal to 50MB to prevent storage exhaustion
        │   └── logind.conf.d/
        │       └── yogabook.conf        # Lid switch & power key mapped to s2idle suspend
        └── touch_keyboard/
            ├── touch-hw.csv       # Physical dimensions & 270° orientation of Halo Keyboard
            └── layout.csv         # Active Halo Keyboard layout
```

---

## 🚀 Installation & Symlink Management

To link all user configurations to their active system paths (and compile native C binaries if needed):

```bash
cd ~/yogabook-config
./install.sh
```

To also synchronize system configuration files in `/etc` (requires `sudo` privileges):

```bash
./install.sh --system
```

---

## 🛠️ Summary of Key Components

### 1. Smart Hinge & Screen Auto-Rotation (`bin/yogabook-autorotate`)
- **Real Hinge Angle Measurement ($0^\circ-360^\circ$)**: Projects gravity vectors onto the plane perpendicular to the physical hinge (X-Z cross-section), making angle calculation 100% immune to lateral roll/tilt up to $70^\circ+$.
- **Hinge Singularity Guard**: When the device is placed sideways (gravity parallel to the hinge axis), the daemon freezes the current mode (`laptop` vs `tablet`), preventing accidental flips when lifting or tilting the device.
- **Laptop Mode ($\le 150^\circ$)**: Screen locked to standard landscape (`270`), auto-rotation disabled, on-screen keyboard dismissed.
- **Tablet / Flat / Book Mode ($\ge 158^\circ$)**: Dynamic auto-rotation enabled.
- **Table Flat-Lock ($< 53^\circ$ tilt, $|z| > 0.60$)**: Freezes orientation when the device is placed flat on a table or lap, preventing unwanted flips to landscape when resting in portrait.

### 2. Native C Touch Control Center & Launcher (`src/control-center`, `src/launcher`)
- **Zero Idle Overhead**: Replaced heavy resident Python GTK4 daemons with native compiled C binaries (`gtk+-3.0`, `gtk-layer-shell-0`).
- **Instant Launch**: Starts and renders in ~50ms upon invocation via `toggle-control-center` / `toggle-launcher` or Waybar icons, dismissing on outside touch or Escape.
- **Direct Hardware Control**: Direct PipeWire volume and backlight adjustments without spawning subshells.

### 3. Smart ALS Auto-Brightness Daemon (`bin/yogabook-autobrightness`)
- Leverages ambient light sensor (ALS) data via `net.hadess.SensorProxy` (D-Bus) and direct sysfs (`iio:device2`).
- Uses a perceptual human-eye curve, exponential moving average (EMA) noise filtering, deadband hysteresis (3%), and smooth stepped ramping (1% per 30ms).
- Preserves manual brightness offsets adjusted via Control Center or Settings across ambient lighting changes.

### 4. Fast Charging & Sleep-Hold Negotiator (`bin/yogabook-charger-negotiate`)
- Resolves the Texas Instruments BQ25892 charger negotiation failure with Lenovo 24W Pump Express+ (PE+) adapters.
- Acquires a temporary `systemd-inhibit` lock on AC connect to hold the SoC awake long enough (~25s) to negotiate 12V high-voltage fast charging before returning to Connected Standby (`s2idle`).

### 5. Wacom Create Pad Digitizer (`config/mango/config.conf`)
- The Create Pad / Halo Keyboard digitizer (`Wacom HID 169 Pen` `056A:0169`) is physically oriented in portrait (1200×1920) matching the display panel.
- Fixed coordinate misalignment (90°/270° offset) by binding `tablet_map_to_mon=DSI-1` in MangoWC, correctly syncing input coordinates 1:1 with the landscape screen transform.

### 6. Standby & Backlight Recovery (`system/etc/mkinitcpio.conf` & `logind.conf.d`)
- Fixed black screen on resume from suspend (`s2idle`) by including `pwm_lpss`, `pwm_lpss_platform`, and `i915` in `/etc/mkinitcpio.conf`. This guarantees the Cherry Trail SoC PWM hardware controller is available before display initialization, enabling proper backlight restoration.
- Configured `/etc/systemd/logind.conf.d/yogabook.conf` so closing the lid suspends the system, and a short press of the power button suspends/resumes instead of triggering an accidental power off.

### 7. Low-Power Virtual On-Screen Keyboard (`bin/wvkbd`, `bin/toggle-keyboard`)
- Uses `wvkbd-mobintl` with an ultra-low memory footprint (~1.4 MB of RAM) and 0 ms appearance latency (runs hidden in the background via `wvkbd.service`).
- Configured with Material Light styling matching the system theme.

### 8. Pacman 7 / Pamac Landlock Bypass (`system/pamac_fix/`)
- Intercepts `alpm_sandbox_setup_child` via `/etc/ld.so.preload` returning `0`, allowing `libpamac` and GUI app stores to synchronize repositories seamlessly on kernels without Landlock support.

### 9. C-Native First Architecture & Best Practices
- **Strictly Avoid Python for Background Daemons**: The Intel Atom x5-Z8550 SoC has 4 low-IPC cores and 4GB LPDDR3 RAM. Running persistent Python interpreters wastes ~20–35 MB of RAM per process and induces periodic CPU wakeups due to Garbage Collection.
- **Micro-Footprint Native C**: All core daemons (`yogabook-autorotate`, `yogabook-autobrightness`, `mango-workspace-watcher`) and interactive triggers (`toggle-*`, `close-window`, `ws-status`) are compiled in native C. They run with **0% idle CPU** and occupy only **~400–600 KB RAM** each (>95% total memory savings across the system).
- **Fast Build Times Without Thermal Throttling**: Avoid heavy Rust or complex C++ toolchains on-device. The entire native C codebase is built recursively in under 2 seconds via `make` from repository root without stressing the fanless magnesium chassis or wearing out the eMMC storage.

