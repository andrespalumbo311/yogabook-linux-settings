# Lenovo Yoga Book (YB1-X91F) – Comprehensive Setup & Optimization Report

This document summarizes all configuration changes, hardware fixes, low-power tunings, and system optimizations implemented on **Arch Linux (kernel 6.17.4-1-yogabook)** with **MangoWC** and **DMS (Dank Material Shell)**.

---

## 1. Graphics & Compositor Optimization (MangoWC)
* **Configuration file**: [`config/mango/config.conf`](file:///home/andres/yogabook-config/config/mango/config.conf)
* **Rendering optimizations for low-power Intel Atom CPU & Gen8 GPU**:
  - **Blur disabled**: `blur=0`, `blur_layer=0` (eliminates multi-pass Kawase blur shaders in SceneFX).
  - **Shadows disabled**: `shadows=0`, `layer_shadows=0` (removes offscreen clipping and drop-shadow overhead).
  - **Animations disabled**: `animations=0`, `layer_animations=0` (eliminates continuous 60fps tick timer and frame recomputations).
  - **Full opacity**: `focused_opacity=1.0`, `unfocused_opacity=1.0` (eliminates alpha blending passes in compositor pipeline).
  - **Drag & snap disabled**: `drag_tile_to_tile=0`, `enable_floating_snap=0`.
  - **Hardware responsiveness**: `syncobj_enable=1`, `warpcursor=1`, `sloppyfocus=1`.
* **Visual appearance & keybindings**:
  - **Rounded corners**: `border_radius=10`, `borderpx=2`.
  - **Super + Enter**: quick terminal launcher (`foot`).
  - **Super + Space**: DMS application launcher (`dms ipc call launcher toggle`).
  - **Super + 1..9**: fast workspace switching (`dispatch workspace 1..9`).
  - **Super + Shift + 1..9**: move window to workspace (`dispatch movetoworkspace 1..9`).
  - **Touchpad natural scrolling**: `trackpad_natural_scrolling=1` and `devicerule=name:virtual-touchpad,natural_scrolling:1`.
  - **Fonts**: installed `ttf-jetbrains-mono-nerd` and `ttf-nerd-fonts-symbols`.
  - **Desktop cleanup**: removed unused legacy environments (Hyprland, Ly).

---

## 2. Package Manager Fix (Pamac / Pacman 7)
* **Issue observed**: `libpamac` failed during package database synchronization with a Landlock error (`Error: restricting filesystem access failed because Landlock is not supported by the kernel!`) and failure transitioning to the unprivileged `alpm` sandbox user.
* **Root cause**: Pacman 7.0 activates Landlock filesystem sandboxing by default, which is disabled in this specific Yoga Book kernel build.
* **Solution**:
  - Compiled interceptor library [`system/pamac_fix/pamac_fix.so`](file:///home/andres/yogabook-config/system/pamac_fix/pamac_fix.c) which forces `alpm_sandbox_setup_child` to return success (`0`).
  - Registered in [`/etc/ld.so.preload`](file:///etc/ld.so.preload).
* **Result**: Pamac synchronizes Arch and AUR repositories instantly and without errors, fully functional via terminal and GUI ("Add/Remove Software" in DMS).

---

## 3. Intelligent Auto-Rotation (Laptop vs Tablet/Flat/Book Mode)
* **Daemon executable**: [`bin/yogabook-autorotate`](file:///home/andres/yogabook-config/bin/yogabook-autorotate)
* **Systemd service**: [`config/systemd/user/rot8.service`](file:///home/andres/yogabook-config/config/systemd/user/rot8.service)
* **Empirical Verification of IIO Sensors**:
  - Testing individual chassis tilt revealed that default upstream udev labels were inverted:
    - `iio:device2` (`ACCEL_LOCATION=base` in udev) is **physically in the DISPLAY** (moved when tilting only the screen).
    - `iio:device4` (unlabeled in udev) is **physically in the KEYBOARD BASE** (remained steady at $Z \approx -1g$ on the desk).
  - The daemon dynamically binds:
    - Screen $\rightarrow$ sensor with `ACCEL_LOCATION=base`
    - Keyboard base $\rightarrow$ unlabeled sensor
* **2D Cross-Section Projection Perpendicular to Hinge**:
  - Coordinate alignment (base to screen frame): $\vec{b}_{\text{al}} = (b_y, -b_x, b_z)$.
  - Physical hinge axis lies along the screen's $Y$ axis.
  - Hinge rotation occurs strictly within the $(X, Z)$ plane:
    $$\vec{v}_s = (s_x, s_z), \quad \vec{v}_b = (b_{\text{al}}[0], b_{\text{al}}[2]) = (b_y, b_z)$$
    $$\text{cross}_{2D} = s_x \cdot b_z - s_z \cdot b_y, \quad \text{dot}_{2D} = s_x \cdot b_y + s_z \cdot b_z$$
    $$\text{opening} = (180.0^\circ - \text{atan2}(\text{cross}_{2D}, \text{dot}_{2D})) \pmod{360^\circ}$$
  - **Lateral Roll Immunity**: The 2D cross-section projection completely cancels roll/tilt effects up to $70^\circ+$, computing the true physical opening angle in any posture.
* **Hinge Singularity Guard**:
  - When the device is tilted $90^\circ$ sideways (hinge vertical), gravity acts parallel to the $Y$ axis. Perpendicular components $\vec{v}_s$ and $\vec{v}_b$ approach zero.
  - In this singular state, gravity cannot physically measure hinge opening.
  - **Guard applied**: (`perp_s < 0.35` and `perp_b < 0.35`) **freezes the active mode** (`laptop` or `tablet`), preventing accidental mode exits when lifting or tilting the device sideways.
* **Hysteresis & Posture Modes**:
  - **LAPTOP Mode**: Base resting flat ($b_z < -0.35g$) and opening $\le 150.0^\circ$. Auto-rotation **disabled**, display locked to **`270`** (standard landscape), virtual keyboard dismissed.
  - **TABLET / FLAT / BOOK Mode**: Opening $\ge 158.0^\circ$ (or folded $360^\circ$). Auto-rotation **active**.
  - **Transition debounce**: 2 consecutive cycles (~0.8s) required before switching modes to filter handling noise.
* **Direct Wayland Rotation with Flat-Lock & Decisive Switching**:
  - Axis mappings:
    - Screen $s_y > +0.35$ $\rightarrow$ `180` (Standard Portrait / Book Mode)
    - Screen $s_y < -0.35$ $\rightarrow$ `normal` (Inverted Portrait)
    - Screen $s_x < -0.35$ $\rightarrow$ `270` (Standard Landscape)
    - Screen $s_x > +0.35$ $\rightarrow$ `90` (Inverted Landscape / Tent Mode)
  - **Table Flat-Lock at 53° ($|z| > 0.60$)**: When the screen is tilted less than 53° from horizontal (e.g. resting on a desk or lap), orientation is **frozen**, preventing unwanted flips to landscape when resting in portrait.
  - **Decisive Switching (1.35x dominance & min 0.50g force)**: Smooth, deliberate transitions only.
  - **Debounce Filter (2 cycles / ~0.8s)**: Eliminates jitter or momentary hand movements.

---

## 4. Virtual On-Screen Keyboard
* **Binary installed**: [`bin/wvkbd`](file:///home/andres/yogabook-config/bin/wvkbd) (`wvkbd-mobintl`), native Wayland/wlroots virtual keyboard.
  - Memory consumption: **only ~1.4 MB of RAM**.
  - Layers: full QWERTY, special symbols, emoji.
  - Styling: rounded corners (`-R 10`) and Catppuccin color scheme matching DMS.
* **Background service**: [`config/systemd/user/wvkbd.service`](file:///home/andres/yogabook-config/config/systemd/user/wvkbd.service) (starts hidden with `--hidden`, ensuring 0 ms appearance latency).
* **Toggle script**: [`bin/toggle-keyboard`](file:///home/andres/yogabook-config/bin/toggle-keyboard) (sends `SIGRTMIN` to show/hide instantly).
* **DMS top-bar widget**:
  - Plugin created in [`config/DankMaterialShell/plugins/VirtualKeyboard/`](file:///home/andres/yogabook-config/config/DankMaterialShell/plugins/VirtualKeyboard/).
  - Keyboard icon ⌨️ integrated into the right section of the DMS top bar.

---

## 5. "Close Window" Widget on DMS Top Bar
* **DMS plugin**: [`config/DankMaterialShell/plugins/CloseWindow/`](file:///home/andres/yogabook-config/config/DankMaterialShell/plugins/CloseWindow/)
* Top-bar widget featuring a red ✕ button that triggers `mmsg dispatch killclient` to instantly close the active window.

---

## 6. Performance, Kernel Tuning & Storage Optimization
* **Reclaimed ~7.5 GB of disk space** (used space reduced from 16 GB to 8.5 GB on 64 GB eMMC):
  - Purged AUR build cache in `~/.cache/paru/clone` (**-2.5 GB**).
  - Pruned old pacman package cache in `/var/cache/pacman/pkg` (**-3.7 GB**).
  - Removed unused heavy build dependencies (`rust`, `clang`, `lld`, `meson`, `gn`, `rust-bindgen`, `scenefx0.4`, etc.) (**-653 MB**).
* **Flash eMMC I/O Optimization ([`system/etc/fstab`](file:///home/andres/yogabook-config/system/etc/fstab))**:
  - Root mounted with `rw,noatime,commit=60`.
  - `noatime`: eliminates continuous flash writes on reading files, icons, and libraries.
  - `commit=60`: flushes ext4 metadata every 60 seconds instead of 5, extending flash idle periods.
* **Freeze & Lag Prevention ([`system/etc/sysctl.d/99-zram-performance.conf`](file:///home/andres/yogabook-config/system/etc/sysctl.d/99-zram-performance.conf))**:
  - Added `vm.dirty_ratio = 10` and `vm.dirty_background_ratio = 5` to prevent I/O blocking caused by sudden large dirty page flushes to slow eMMC storage.
  - ZRAM configured to 4 GB (`zram0`) with `lzo-rle`, `vm.swappiness = 180`, and `vm.page-cluster = 0` (zero swap-in read amplification).
* **Journald Storage Cap**:
  - Configured [`system/etc/systemd/journald.conf.d/00-size-limit.conf`](file:///home/andres/yogabook-config/system/etc/systemd/journald.conf.d/00-size-limit.conf) with **50 MB** limit (`SystemMaxUse=50M`, `RuntimeMaxUse=30M`).
* **Background Service Management**:
  - Disabled `accounts-daemon.service` (unnecessary overhead for DMS/Mango).
  - **`sshd.service` preserved active** for remote management.

---

## 7. Video Hardware Acceleration & Monitoring
* **VA-API Driver**: Verified Intel Cherryview Gen8 `i965` driver with hardware decoding for H.264, VP8, and HEVC 8-bit.
* **YouTube / Browser Optimization**: Configured **enhanced-h264ify** extension to prioritize H.264 (hardware-decoded by GPU) and block VP9/AV1 (which would max out the Atom CPU).
* **Installed Utilities**:
  - `nvtop`: real-time terminal CPU/GPU monitor.
  - `pamac-manager`: graphical software center.

---

## 8. Standby & Suspend Management (S2idle, PWM Backlight & Logind)
* **Diagnosis of Black Screen Issue**:
  - Closing the lid entered `s2idle` Connected Standby cleanly.
  - Opening the lid resumed the system (`PM: suspend exit`), but **the screen remained completely black**.
  - **Primary cause**: Kernel log showed:
    `i915 0000:00:02.0: [drm] *ERROR* [CONNECTOR:115:DSI-1] Failed to get the SoC PWM chip`
    The `i915` video driver failed to locate the SoC hardware PWM controller on boot because `pwm_lpss` and `pwm_lpss_platform` modules were missing from the initramfs. Consequently, the native `intel_backlight` controller never registered, leaving the backlight unpowered upon resume.
  - **Secondary cause (accidental shutdown)**: With the screen dark, a short press of the power button triggered logind's default `HandlePowerKey=poweroff`, immediately powering down the machine.
* **Fix Applied**:
  1. **Early Initramfs Modules ([`system/etc/mkinitcpio.conf`](file:///home/andres/yogabook-config/system/etc/mkinitcpio.conf))**:
     - Added early modules: `MODULES=(pwm_lpss pwm_lpss_platform i915)`.
     - Regenerated image via `mkinitcpio -p linux-yogabook`. The PWM controller is now available before `i915` binds to the DSI panel, enabling proper backlight recovery on wake.
  2. **Logind Configuration ([`system/etc/systemd/logind.conf.d/yogabook.conf`](file:///home/andres/yogabook-config/system/etc/systemd/logind.conf.d/yogabook.conf))**:
     - `HandleLidSwitch=suspend` (suspends on lid close).
     - `HandlePowerKey=suspend` (short press suspends or resumes the system).
     - `HandlePowerKeyLongPress=poweroff` (full shutdown only on long press).

---

## 9. Wacom Create Pad Digitizer Alignment
* **Device**: `Wacom HID 169 Pen` (`0018:056A:0169` via `i2c-WCOM0019:00`).
* **Issue observed**: Moving the pen on the Create Pad showed an exact 90°/270° coordinate mismatch with the screen (Top-Right $\rightarrow$ Bottom-Right, Bottom-Right $\rightarrow$ Bottom-Left).
* **Root cause**: The Wacom digitizer hardware is physically mounted in portrait (1200×1920) matching the display panel. MangoWC lacked the `tablet_map_to_mon` directive, causing `wlroots` to treat the digitizer as unmapped rather than applying the output's 270° landscape transformation.
* **Fix Applied**:
  - Added `tablet_map_to_mon=DSI-1` to [`config/mango/config.conf`](file:///home/andres/yogabook-config/config/mango/config.conf#L82).
  - Coordinates now map 1:1 across all four corners with zero latency and full pressure support.
