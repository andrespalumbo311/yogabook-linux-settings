# Historical Errors & Prevention

This document records architectural, packaging, and runtime issues encountered on the system, their underlying technical causes, and generalized patterns to prevent regressions.

---

## 1. Missing XDG User Directory Runtime in Minimal Compositor Environments

- **Date**: 2026-09-07
- **Subsystem**: User Environment / Wayland Session / Flutter (`path_provider_linux`)
- **Symptoms**:
  - GUI applications (e.g. Flutter-based apps like `saber`) crash immediately upon launch without displaying a window.
  - Error logs reveal:
    ```text
    SEVERE: ErrorLogger: MissingPlatformDirectoryException(Unable to get application documents directory)
    ```
- **Root Cause**:
  Cross-platform UI runtimes (such as Flutter's `path_provider_linux` / `xdg_directories`) do not rely solely on environment variables; they explicitly spawn the `xdg-user-dir` CLI utility (`xdg-user-dir DOCUMENTS`) to resolve paths. In minimal or standalone window manager setups (e.g. MangoWC, Sway, dwl) where standard desktop environments (GNOME, KDE) are not installed, the `xdg-user-dirs` package may not be present in `/usr/bin` nor is `~/.config/user-dirs.dirs` populated by default. An unhandled exception during platform directory lookup causes the app to terminate during initialization.
- **Resolution**:
  1. Provide a canonical, lightweight fallback implementation in `~/yogabook-config/bin/xdg-user-dir` that reads `~/.config/user-dirs.dirs` (or falls back to `$HOME/Documents`, etc.) and defers to `/usr/bin/xdg-user-dir` when present.
  2. Maintain `config/user-dirs.dirs` in the repository and symlink it via `install.sh`.
  3. Ensure standard directories like `~/Documents` are created.
  4. (Optional system package): For full localization and system hooks, install `xdg-user-dirs` via the system package manager (`sudo pacman -S xdg-user-dirs`).
- **Prevention Pattern**:
  When configuring minimal Wayland/X11 compositors from scratch, always ensure baseline freedesktop standards (XDG user directories, D-Bus environment activation, and credential storage) are explicitly satisfied in user space or installed at system level.

---

## 2. Unregistered D-Bus Secret Service Provider

- **Date**: 2026-09-07
- **Subsystem**: Security / Credentials Storage (`libsecret`, `flutter_secure_storage`)
- **Symptoms**:
  - Repeated warnings in application stderr:
    ```text
    WARNING **: libsecret_error: secret_service_get_sync: The name is not activatable
    SEVERE: ErrorLogger: PlatformException(Libsecret error, secret_service_get_sync: ...)
    ```
  - Failure to persist cloud accounts, OAuth tokens, or encrypted local vaults across restarts.
- **Root Cause**:
  `libsecret` is a client library that requires a running D-Bus provider implementing `org.freedesktop.secrets`. In lightweight standalone setups, no secret service daemon (such as `gnome-keyring` or `keepassxc`) is started or socket-activated.
- **Resolution**:
  Install `gnome-keyring` (`sudo pacman -S gnome-keyring`) and ensure D-Bus user session activation is enabled if credential persistence or cloud synchronization is required.

---

## 3. Stale Compositor IPC Sockets in Persistent Desktop Shell Daemons

- **Date**: 2026-09-08
- **Subsystem**: Desktop Shell / IPC / Compositor Communication (`mmsg`, `Quickshell`, DMS)
- **Symptoms**:
  - Custom UI widgets in the desktop shell bar (e.g. DMS close window button) stop working after compositor reload/restart.
  - Shell logs or spawned processes report `connect: No such file or directory` or socket server not found.
- **Root Cause**:
  When a desktop shell daemon (such as DMS running under systemd `dms.service`) starts, it inherits the compositor's IPC environment variable (e.g. `MANGO_INSTANCE_SIGNATURE` pointing to `/run/user/$UID/mango-<pid>.sock`). If the compositor restarts or changes socket path without terminating the shell daemon, the shell daemon continues to retain the old socket path in its environment. Inline shell fallbacks using parameter expansion `${VAR:-fallback}` fail to recover because `$VAR` is set and non-empty even though the socket file no longer exists on disk.
- **Resolution**:
  1. Delegate IPC actions to dedicated wrapper scripts (e.g. `bin/close-window`) rather than complex inline shell strings in QML widgets.
  2. In wrapper scripts, explicitly validate the socket's existence on the filesystem using `[ -S "$MANGO_INSTANCE_SIGNATURE" ]`.
  3. If the socket does not exist, query the systemd user environment (`systemctl --user show-environment`) or dynamically locate the most recent valid socket in `/run/user/$UID/`.
- **Prevention Pattern**:
  Never assume inherited environment variables pointing to IPC socket paths remain valid across the lifecycle of long-running daemon processes. Always verify file socket existence (`test -S`) before dispatching commands and provide resilient runtime discovery.

---

## 4. Display Manager Greeter Lockout & Implicit Compositor Coupling During Desktop Shell Migration

- **Date**: 2026-09-08
- **Subsystem**: Display Manager / Compositor Configuration / Session Lifecycle (`greetd`, `dms-greeter`, `MangoWC`)
- **Symptoms**:
  - Removing a heavy desktop shell package (such as DMS) leaves the system unbootable into a graphical session or greeting prompt if `/etc/greetd/config.toml` continues to invoke the removed greeter binary (e.g. `dms-greeter`).
  - MangoWC fails to apply the native 270° display transform or custom keybindings if `config.conf` sources shell-generated files (`./dms/outputs.conf`, `./dms/binds.conf`) that are deleted alongside the uninstalled shell.
- **Root Cause**:
  Implicit dependencies between system-level services (`greetd`) and user-level desktop suites, combined with external tool generation of compositor core parameters (display rotation, DPI scale, hotkeys).
- **Resolution**:
  1. Decouple display configuration (`outputs.conf`) and keybindings (`binds.conf`) into standalone, canonical repository files sourced directly by the compositor.
  2. Pre-configure and install an alternative greeter (e.g. `greetd-tuigreet`) in `/etc/greetd/config.toml` *prior* to decommissioning the legacy desktop shell packages.
- **Prevention Pattern**:
  Always audit system display manager definitions (`/etc/greetd/`, systemd display-manager services) and compositor inclusion trees before replacing or uninstalling an active desktop shell environment.

---

## 5. Monospace Font Logical Advance Mismatch in Bar Icon Centering

- **Date**: 2026-09-08
- **Subsystem**: Status Bar / Typography / Pango Layout (`Waybar`, `JetBrainsMono Nerd Font`, GTK3)
- **Symptoms**:
  - Top bar icons (e.g. keyboard, Wi-Fi) appear heavily shifted to the right inside symmetric pill buttons, even when CSS horizontal padding and margins are balanced.
  - Pixel measurement shows asymmetrical margins (e.g. left: 19px, right: 9px; 10px discrepancy).
- **Root Cause**:
  When a monospace font family (like `JetBrainsMono Nerd Font`) is defined as the primary font, Pango calculates the logical advance (`Log width`) based on a single monospace character cell (e.g. 9px at 15px size). Multicharacter or wide icon glyphs (e.g. `` width 16px, `󰤨` width 15px) have an *ink width* much larger than 9px. While GTK centers the 9px logical cell, the ink extends 6–7px out to the right beyond the cell boundary, creating a prominent visual asymmetry.
- **Resolution**:
  1. Use proportional font variants (`JetBrainsMono Nerd Font Propo` or `Symbols Nerd Font`) where Pango sets the logical advance equal to the glyph's ink bounding box.
  2. In GTK CSS, avoid invalid properties like `text-align: center;` or `margin: 0 auto;` (which cause Waybar parse crashes); rely on explicit `min-width` and symmetric `padding` on the parent button and `margin: 0; padding: 0;` on the child label.
- **Prevention Pattern**:
  Never use monospace font families for single-character icon labels in status bars or buttons. Always prioritize proportional icon font definitions (`Nerd Font Propo` or `Symbols Nerd Font`).

---

## 6. Kinetic Touch Micro-Jitter & ScrolledWindow Event Cancellation in Touch App Drawers

- **Date**: 2026-09-08
- **Subsystem**: Touch Input / Application Launchers (`nwg-drawer`, GTK3, GtkLayerShell)
- **Symptoms**:
  - Tapping an application icon on a physical touchscreen fails to launch on the first tap, requiring an aggressive double-tap.
  - Re-clicking the top bar launcher icon fails to toggle or dismiss the drawer.
  - Tapping outside the launcher card fails to close the drawer.
- **Root Cause**:
  1. Desktop-oriented drawers (like `nwg-drawer`) place icon grids inside a kinetic `GtkScrolledWindow` and attach an adjustment listener that flags `beenScrolled = true` upon any viewport displacement. Physical finger touch contains natural micro-jitter that triggers micro-scrolling on touch down, causing the subsequent `button-release-event` to discard the launch action.
  2. Outside dismissal in `nwg-drawer` is tied to mouse Button 3 (right click), which is never generated by a standard touchscreen tap.
  3. Toggle scripts using `nwg-drawer -close` hang indefinitely when no background resident daemon is actively listening.
- **Resolution**:
  1. Implement a purpose-built touch launcher (`bin/yogabook-launcher`) using `GtkLayerShell`.
  2. Connect app activation directly to `GtkButton`'s `"clicked"` signal, which GTK's touch gesture engine recognizes reliably on touch release regardless of micro-jitter.
  3. Wrap the launcher card in a fullscreen overlay where any tap on the outer backdrop triggers `Gtk.main_quit()`.
  4. Use a deterministic PID file (`/tmp/yogabook-launcher.pid`) in `bin/toggle-launcher` for instantaneous (1ms) toggle without socket hangs.
- **Prevention Pattern**:
  For touchscreens, avoid mouse-centric launcher binaries that rely on `button-release-event` inside kinetic scrolled windows. Use standard GTK `"clicked"` gesture bindings and fullscreen overlay dismissal.

---

## 7. Upstream PulseAudio Port Lookup Fallback Typo & Native GTK4 Control Center Migration

- **Date**: 2026-09-08
- **Subsystem**: Audio Control / Desktop Shell / Wayland Widgets (`SwayNC`, `WirePlumber`, `PipeWire`, GTK4 Layer-Shell)
- **Symptoms**:
  - The volume slider inside SwayNC slides visually upon touch or mouse interaction, but produces zero change in actual audio output volume.
  - In contrast, physical volume keys and keyboard hotkeys adjust audio volume properly.
- **Root Cause**:
  1. The Lenovo Yoga Book Cherryview sound card (`cht-yogabook`) operates under the `pro-audio` WirePlumber profile (`alsa_output.platform-cht-yogabook.pro-output-0`). In this profile, PipeWire does not expose standard ALSA output ports (`info.active_port` is NULL).
  2. In SwayNC's upstream Vala source (`pulseDaemon.vala`), when a sink has no active ports, the fallback path contains a programming error:
     `bool is_default = device.device_name == this.default_source_name;`
     It compares the output sink's device name against the default *microphone input source* name. As a result, `default_sink` is always resolved as NULL, causing `slider.value_changed` to silently drop all volume adjustment requests.
  3. Physical volume buttons bypass PulseAudio daemon abstractions and invoke `wpctl set-volume @DEFAULT_AUDIO_SINK@ ...` directly.
- **Resolution**:
  1. Implement a purpose-built, resident touch Control Center in GTK 4 (`bin/yogabook-control-center`) using `Gtk4LayerShell`.
  2. Connect slider adjustments directly to `wpctl set-volume` and `brightnessctl set` using a non-blocking debounced worker (30ms rate limit) to ensure 60fps gesture fluidness without blocking the UI thread.
  3. Anchor the panel overlay at `Gtk4LayerShell.Edge.TOP` with `margin-top: 40px` (Waybar total height), ensuring the card sits directly flush against the bottom border of Waybar with zero gap.
  4. Provide instant daemon toggling (<15ms) via `SIGUSR1` and an explicit boolean state flag (`self.is_open`).
- **Prevention Pattern**:
  When managing audio in specialized ALSA/PipeWire setups lacking standard output port trees, avoid monolithic notification daemons with fragile PulseAudio port assumptions. Direct IPC or CLI control via `wpctl` guarantees profile-agnostic audio management.

---

## 8. Python Subprocess Stream Block Buffering & Hardware Battery Metric Divergence

- **Date**: 2026-09-08
- **Subsystem**: Control Center / PipeWire Audio / Sysfs Battery / Posture State (`pactl`, `bq27542-0`, `yogabook-autorotate`)
- **Symptoms**:
  - Live volume slider in the Control Center failed to move when volume was changed externally via physical volume keys, even though a background thread monitored `pactl subscribe`.
  - Battery percentage displayed in Waybar differed from the Control Center card (e.g. 51% vs 45%).
  - Auto-rotation quick tile displayed "Attiva" (Active blue) while device orientation was physically locked in laptop mode.
  - A 3-pixel gap existed between the Waybar bottom border and the Control Center card.
- **Root Cause**:
  1. *Subprocess Block Buffering*: In Python, iterating over `for line in proc.stdout:` uses internal 4KB block buffering on file iterators. For sporadic real-time event streams like `pactl subscribe`, events are buffered in memory and never yielded until 4096 bytes accumulate.
  2. *Battery Metric Mismatch*: Texas Instruments `bq27542` fuel gauge reports a compensated `capacity` (45%), whereas Waybar's `modules/battery.cpp` calculates `round(charge_now * 100.0 / charge_full)` (51%).
  3. *Daemon vs Posture Coupling*: The rotation toggle checked `systemctl is-active rot8.service`. Because the daemon runs continuously to monitor accelerometers, it returned active even though the daemon deliberately locks orientation to landscape (`270`) in laptop mode.
  4. *Layer-Shell Margin Scale*: Waybar logical height is 38. With Wayland scale 1.5, `38 * 1.5 = 57px`. The Control Center was configured with `margin-top: 40` (60px), leaving a 3-physical-pixel gap (Y=57..59).
- **Resolution**:
  1. Use `bufsize=1` and an explicit `while self.running: line = proc.stdout.readline()` loop for zero-latency event streaming.
  2. Synchronize battery parsing to compute `int(round(charge_now * 100.0 / charge_full))` matching Waybar's exact algorithm.
  3. In `yogabook-autorotate`, export runtime posture (`mode=laptop` vs `mode=tablet`) to `/run/user/$UID/yogabook-rotation.state`. The Control Center tile displays "Bloccata (Laptop)" when locked and highlights "Attiva (Tablet)" only when auto-rotation is physically enabled.
  4. Adjust Layer-Shell `margin-top` to 38, making the panel flush against Waybar.
- **Prevention Pattern**:
  Always use unbuffered line reading (`readline()` with `bufsize=1`) when piping IPC monitoring streams in Python. Always inspect the exact sysfs computation algorithm of parent status bars to prevent widget metric discrepancies.

---

## 9. Broadcom UART Bluetooth ACPI Firmware Resolution & Default-Off Lifecycle

- **Date**: 2026-09-08
- **Subsystem**: Bluetooth Subsystem / Kernel Drivers / Power Management (`hci_bcm`, `bluez`, `broadcom-bt-firmware`)
- **Symptoms**:
  - Bluetooth inoperable; `bluetooth.service` unit missing.
  - Kernel logs report: `Bluetooth: hci0: BCM: firmware Patch file not found, tried: 'brcm/BCM4356A2.hcd'`.
  - BlueZ daemon logs report: `no bluetooth adapter found: The name is not activatable`.
- **Root Cause**:
  1. *ACPI UART Firmware Naming*: Broadcom combo chips (BCM4356A2 / ACPI ID `BCM2E8A:00`) attached via serdev/UART lack USB vendor/product IDs. The kernel `hci_bcm` driver requests a generic `/lib/firmware/brcm/BCM4356A2.hcd`. The AUR package `broadcom-bt-firmware-git` distributes these files named by vendor-product sub-ID (e.g. `BCM4356A2-0a5c-640e.hcd` for Lenovo 4356 NGFF combo).
  2. *Uninstalled Protocol Stack*: `bluez` and `bluez-utils` were not installed on the minimal installation.
- **Resolution**:
  1. Install `broadcom-bt-firmware-git`, `bluez`, and `bluez-utils`.
  2. Create a canonical symlink `/usr/lib/firmware/brcm/BCM4356A2.hcd -> BCM4356A2-0a5c-640e.hcd`.
  3. Enable `bluetooth.service` for system D-Bus activation.
  4. Create and enable `bluetooth-default-off.service` (`rfkill block bluetooth` at multi-user boot) to enforce zero standby battery drain on boot.
  5. In `yogabook-control-center`, connect the Bluetooth quick toggle to both `rfkill` unblock/block and `bluetoothctl power on/off`.
- **Prevention Pattern**:
  On SoC platforms with UART/serdev Broadcom Bluetooth, always verify whether the kernel driver expects a generic `.hcd` alias rather than vendor-tagged firmware filenames. Ensure default-off power management services do not cut radio power concurrently during active driver baudrate negotiation.




