# Lenovo Yoga Book (YB1-X91F) – Linux Configurations & Dotfiles

This repository contains the official configuration files, custom scripts, system services, and hardware/software optimizations developed for the **Lenovo Yoga Book 1st Gen (YB1-X91F / YB1-X90F series)** running **Arch Linux** with **MangoWC** (lightweight wlroots Wayland compositor) and **DMS (Dank Material Shell)**.

All active user configurations on the system (`~/.config/`, `~/.local/bin/`, DMS plugins) are **symbolic links** pointing to this repository. Any changes made inside this repository reflect instantly on the running system.

---

## 📁 Repository Structure

```text
yogabook-config/
├── README.md                      # Usage guide and project overview
├── AGENTS.md                      # Architecture guide and instructions for AI agents
├── YOGABOOK_SETUP_REPORT.md       # Full technical report detailing all hardware interventions
├── install.sh                     # Idempotent linker script to set up/restore all symlinks
│
├── bin/                           # User executables (symlinked to ~/.local/bin/)
│   ├── yogabook-autorotate        # Smart posture & auto-rotation daemon (2D projection, singularity guard, table flat-lock)
│   ├── toggle-keyboard            # Toggle script for wvkbd on-screen virtual keyboard
│   └── wvkbd                      # wvkbd-mobintl binary (compiled for minimal footprint)
│
├── config/                        # User dotfiles (symlinked to ~/.config/)
│   ├── mango/
│   │   └── config.conf            # Ultra-low-power MangoWC compositor config (Wacom mapping, rounded corners)
│   ├── systemd/
│   │   └── user/
│   │       ├── rot8.service       # User systemd service for yogabook-autorotate
│   │       └── wvkbd.service      # User systemd service for wvkbd (--hidden background daemon)
│   └── DankMaterialShell/         # DMS configurations and custom QML plugins
│       ├── plugin_settings.json   # Active DMS plugin settings
│       ├── plugins.lock.json      # DMS plugin lock state
│       └── plugins/
│           ├── VirtualKeyboard/   # Top-bar virtual keyboard trigger widget
│           └── CloseWindow/       # Top-bar active window close widget (✕ red button)
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

To link all user configurations to their active system paths:

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

### 2. Wacom Create Pad Digitizer (`config/mango/config.conf`)
- The Create Pad / Halo Keyboard digitizer (`Wacom HID 169 Pen` `056A:0169`) is physically oriented in portrait (1200×1920) matching the display panel.
- Fixed coordinate misalignment (90°/270° offset) by binding `tablet_map_to_mon=DSI-1` in MangoWC, correctly syncing the input coordinates 1:1 with the landscape screen transform.

### 3. Standby & Backlight Recovery (`system/etc/mkinitcpio.conf` & `logind.conf.d`)
- Fixed black screen on resume from suspend (`s2idle`) by including `pwm_lpss`, `pwm_lpss_platform`, and `i915` in `/etc/mkinitcpio.conf`. This guarantees the Cherry Trail SoC PWM hardware controller is available before display initialization, enabling proper backlight restoration.
- Configured `/etc/systemd/logind.conf.d/yogabook.conf` so closing the lid suspends the system, and a short press of the power button suspends/resumes instead of triggering an accidental power off.

### 4. Low-Power Virtual On-Screen Keyboard (`bin/wvkbd`, `bin/toggle-keyboard`)
- Uses `wvkbd-mobintl` with an ultra-low memory footprint (~1.4 MB of RAM) and 0 ms appearance latency (runs hidden in the background via `wvkbd.service`).
- Styled with rounded corners (`-R 10`) and Catppuccin color scheme matching DMS.

### 5. Pacman 7 / Pamac Landlock Bypass (`system/pamac_fix/`)
- Intercepts `alpm_sandbox_setup_child` via `/etc/ld.so.preload` returning `0`, allowing `libpamac` and GUI app stores to synchronize repositories seamlessly on kernels without Landlock support.
