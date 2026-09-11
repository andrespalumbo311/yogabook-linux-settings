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
  2. *Battery Metric Mismatch & Overflow*: Texas Instruments `bq27542` fuel gauge reports a compensated `capacity` (45%), whereas Waybar's `modules/battery.cpp` calculates `round(charge_now * 100.0 / charge_full)` clamped to 100.f. Without clamping, when `charge_now > charge_full` on a full charge, the ratio exceeds 100% (e.g. 104%).
  3. *Daemon vs Posture Coupling*: The rotation toggle checked `systemctl is-active rot8.service`. Because the daemon runs continuously to monitor accelerometers, it returned active even though the daemon deliberately locks orientation to landscape (`270`) in laptop mode.
  4. *Layer-Shell Margin Scale*: Waybar logical height is 38. With Wayland scale 1.5, `38 * 1.5 = 57px`. The Control Center was configured with `margin-top: 40` (60px), leaving a 3-physical-pixel gap (Y=57..59).
- **Resolution**:
  1. Use `bufsize=1` and an explicit `while self.running: line = proc.stdout.readline()` loop for zero-latency event streaming.
  2. Synchronize battery parsing to compute `min(100, max(0, int(round(charge_now * 100.0 / charge_full))))` matching Waybar's exact algorithm and upper bound clamp.
  3. In `yogabook-autorotate`, export runtime posture (`mode=laptop` vs `mode=tablet`) to `/run/user/$UID/yogabook-rotation.state`. The Control Center tile displays "Bloccata (Laptop)" when locked and highlights "Attiva (Tablet)" only when auto-rotation is physically enabled.
  4. Adjust Layer-Shell `margin-top` to 38, making the panel flush against Waybar.
- **Prevention Pattern**:
  Always use unbuffered line reading (`readline()` with `bufsize=1`) when piping IPC monitoring streams in Python. Always inspect the exact sysfs computation algorithm of parent status bars and clamp all percentage computations (`min(100, max(0, val))`) to prevent values exceeding 100% when instantaneous battery charge exceeds learned full capacity.

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

---

## 10. Libadwaita Pango Markup Escaping & Dynamic Wayland Display Hotplug Handling

- **Date**: 2026-09-08
- **Subsystem**: Settings Application / Multi-Display Topologies / GTK4 & Libadwaita (`wlr-randr`, `AdwActionRow`, `Pango`)
- **Symptoms**:
  - Gtk-WARNING and Adwaita-CRITICAL logs when populating `Adw.ActionRow` titles:
    ```text
    Failed to set text 'Salva & Applica' from markup: Entity did not end with a semicolon; most likely you used an ampersand character without intending to start an entity
    ```
  - Running `wlr-randr --output HDMI-A-1 ...` fails with `unknown output HDMI-A-1` when the external cable is physically unplugged.
  - Python single-window GTK apps hang or fail to emit `"activate"` signal when launched with `sys.argv` containing the full script path without explicit command-line handling.
- **Root Cause**:
  1. *Pango Markup by Default*: Libadwaita `AdwActionRow.set_title()` treats strings as Pango markup. Bare ampersands (`&`) trigger XML/Pango entity resolution and fail if unescaped.
  2. *Wlroots Output Lifetime*: Under MangoWC / wlroots, video connectors (e.g. `HDMI-A-1`) are dynamically instantiated. When disconnected, wlroots removes the output from the compositor graph, causing direct `wlr-randr` configuration commands targeting `HDMI-A-1` to fail.
  3. *GApplication Invocation*: Passing non-empty `sys.argv` to `Adw.Application.run()` invokes option parsing which may consume arguments or defer activation if GApplication flags are unconfigured.
- **Resolution**:
  1. Replace unescaped ampersands (`&`) with plain text ("e") or `&amp;` in all Libadwaita titles and subtitles.
  2. Implement a dedicated display manager (`bin/yogabook-display-mgr`) that inspects `/sys/class/drm/card0-HDMI-A-1/status` prior to invoking `wlr-randr`, and store the user's preferred layout (Mirroring, Extend, Solo external, Solo internal) in `~/.config/yogabook/display.json`.
  3. Hook HDMI connector status changes into the existing low-overhead autorotate monitoring loop (`yogabook-autorotate`) for instant automatic layout application upon cable connection/disconnection.
  4. Use `app.run([])` for standalone desktop tool utilities to guarantee direct `activate` signal emission.
- **Prevention Pattern**:
  Never assume Wayland video outputs exist persistently when cables are unplugged; always query DRM sysfs connector state before dispatching `wlr-randr` output rules. Always sanitize strings passed to GTK4/Libadwaita text setters to prevent Pango markup parser crashes.

---

## 11. Physical US Keyboard Accented Character Composition via XKB Dead Keys

- **Date**: 2026-09-08
- **Subsystem**: Input Subsystem / XKB Keymap / Wayland Compositor (`MangoWC`, `libxkbcommon`, `Halo Keyboard`)
- **Symptoms**:
  - Typing an accent (such as `'` or `` ` ``) followed by a vowel on the physical Halo Keyboard (US layout) outputs raw characters sequentially (e.g. `'e`, `` `a ``) rather than composed accented glyphs (`é`, `à`).
  - Users typing Italian, French, Spanish, or Portuguese text cannot produce accented vowels naturally without switching physical layouts.
- **Root Cause**:
  In minimalist Wayland compositors (such as MangoWC), omitting explicit `xkb_rules_layout` and `xkb_rules_variant` defaults to standard `English (US)` (`us` without variant). In standard US XKB rules, apostrophe (`KEY_APOSTROPHE`) and grave (`KEY_GRAVE`) are ordinary printable characters with no dead-key composition rules.
- **Resolution**:
  1. Set `xkb_rules_layout=us` and `xkb_rules_variant=intl` in `config/mango/config.conf`.
  2. Reload compositor configuration live via `WAYLAND_DISPLAY=wayland-0 mmsg dispatch reload_config`.
  3. With `us(intl)`, the dead key combinations operate across all inputs:
     - `'` + `e` $\rightarrow$ `é`
     - `` ` `` + `e` $\rightarrow$ `è`
     - `~` + `n` $\rightarrow$ `ñ`
     - `'` + `c` $\rightarrow$ `ç`
     - `'` + `Space` $\rightarrow$ `'`
- **Prevention Pattern**:
  When deploying devices with physical US keyboards in multilingual environments, explicitly declare `us` with `intl` variant in compositor configuration rather than relying on unconfigured default US layouts.

---

## 12. Unsupervised Polkit Authentication Agent in Minimal Compositor Sessions

- **Date**: 2026-09-08
- **Subsystem**: Privilege Elevation / Security / D-Bus Authentication (`polkit`, `polkit-gnome`, `pamac`)
- **Symptoms**:
  - Graphical package managers (like Pamac / App Store) or privilege-escalating GUI tools fail to show the root password prompt when performing administrative actions (e.g. installing, removing, or updating packages).
  - CLI check `pkcheck --action-id ... --allow-user-interaction` returns error:
    ```text
    Authorization requires authentication but no agent is available.
    ```
- **Root Cause**:
  1. Desktop shell transitions: Monolithic desktop environments (GNOME, KDE, DMS) often provide integrated Polkit authentication agents directly in their shell processes. When transitioning to lightweight standalone modular components (Waybar, SwayNC), the integrated agent is lost.
  2. Incomplete Compositor Cold Boot Directives: Relying on compositor configuration directives (`exec-once`) to spawn authentication agents fails during live migrations because compositors only execute `exec-once` at cold start and ignore them during config reloads (`reload_config`). Furthermore, `exec-once` provides no supervisor process monitoring or auto-restart upon crash.
- **Resolution**:
  1. Wrap the standalone authentication agent (`/usr/lib/polkit-gnome/polkit-gnome-authentication-agent-1`) in a supervised systemd user unit (`polkit-gnome.service`) bound to `mango-session.target`.
  2. Enable automatic restart on failure (`Restart=on-failure`, `RestartSec=1`).
  3. Symlink and register the unit in `install.sh` and enable it under `mango-session.target.wants/`.
- **Prevention Pattern**:
  Never rely on window manager / compositor `exec-once` directives for foundational system D-Bus agents (Polkit, keyring, notifications). Manage critical background daemons via supervised user systemd units hooked to the graphical session target to ensure persistence, restartability, and clean lifecycle management.

---

## 13. Legacy GPU Pipeline Fallbacks & Portal Latency on Low-Power SoCs

- **Date**: 2026-09-08
- **Subsystem**: Graphics Subsystem / Wayland Portals / App Runtime (`GTK4`, `Chromium`, `Mesa`, `xdg-desktop-portal`)
- **Symptoms**:
  - Launching standard graphical applications (Pamac App Store, Chromium, Nautilus, Zenity) feels noticeably sluggish, taking 5–8 seconds to map their initial window.
  - Console and journal logs exhibit:
    - Mesa HasVK Vulkan driver warnings (`anv_device: GTT size larger than 2 GiB`, `DRM modifiers`).
    - Chromium GPU process conflict: `'--ozone-platform=wayland' is not compatible with Vulkan`.
    - D-Bus Inhibit portal errors: `GDBus.Error.InvalidArgs: No such interface org.freedesktop.portal.Inhibit`.
    - Tracker 3 / LocalSearch indexer background disk crawling and missing `XDG_SESSION_CLASS=user` activation failures.
- **Root Cause**:
  1. *Experimental Vulkan Default*: Modern GUI toolkits (GTK 4.20+, Chromium) now default to Vulkan rendering if any Vulkan driver is present on the system. On Intel Gen8 Cherryview (Cherry Trail Atom x5-Z8550), Mesa provides only the legacy, unmaintained `vulkan_hasvk` driver. This driver lacks complete Wayland surface extensions, triggering JIT shader recompilation overhead on CPU, buffer negotiation errors, or GPU process crash-and-fallback cascades.
  2. *Portal Interface Rejection Delay*: When `portals.conf` maps an interface (like `Inhibit`) to `none`, `xdg-desktop-portal` rejects application queries with `InvalidArgs`, creating synchronous D-Bus roundtrip stalls during window realization.
  3. *Unconstrained Background File Indexing*: Tracker 3 / LocalSearch configured to recursively index `$HOME` on flash eMMC severely saturates random I/O queues and starves Atom in-order CPU cores during application startup.
- **Resolution**:
  1. Force mature OpenGL rendering for all GTK4 applications by setting `GSK_RENDERER=gl` in `~/.config/environment.d/10-performance.conf`.
  2. In `~/.config/chromium-flags.conf`, explicitly set `--ozone-platform=wayland` and disable Vulkan (`--disable-features=Vulkan`) alongside `--enable-gpu-rasterization` and `--enable-zero-copy`.
  3. Correct portal routing in `config/xdg-desktop-portal/mango-portals.conf` to map `Inhibit=gtk` instead of `none`.
  4. Restrict Tracker recursive indexing (`index-recursive-directories "[]"` and `fts-enabled false`).
  5. Provide system udev rule (`60-mmc-readahead.rules`) increasing eMMC block device readahead to 1024KB.
- **Prevention Pattern**:
  On legacy low-power SoCs with partial or experimental Vulkan support (e.g. Intel Gen7/Gen8, early Mali), never allow toolkits to default to Vulkan. Always lock the graphical stack to mature OpenGL drivers with persistent on-disk shader caching, and sanitize portal definitions to eliminate D-Bus roundtrip delays.

---

## 14. Missing X11 Display Environment Variable in Systemd App Launcher Daemons

- **Date**: 2026-09-08
- **Subsystem**: Application Lifecycle / Wayland Compositor / Xwayland / Systemd User Sessions
- **Symptoms**:
  - Certain graphical applications (e.g. `readest`, CEF/Electron apps, legacy X11 utilities) fail to start when launched from custom desktop shell launchers or application drawers, while launching normally when run directly from an interactive terminal emulator.
  - Journal logs show immediate application crash/panic on startup:
    ```text
    thread 'main' panicked at ...:
    error while running tauri application: Runtime(CreateWindow)
    ```
- **Root Cause**:
  1. *Dual Display Server Requirement*: Many modern Linux application frameworks (such as Tauri using Chromium Embedded Framework `tauri_runtime_cef` or older Electron builds) rely on X11/Xwayland (`winit_x11`) on Linux rather than native Wayland interfaces. They require an active `DISPLAY` environment variable (e.g. `DISPLAY=:0`).
  2. *Asynchronous Session Environment Activation*: When an application drawer daemon (e.g. `yogabook-launcher.service`) runs as an early systemd user unit, it may be instantiated before the Wayland compositor completes `dbus-update-activation-environment --systemd --all`. Furthermore, systemd user services do not automatically inherit dynamic environment changes made after their startup unless explicitly restarted.
3. *Unset Child Environment*: If the launcher daemon's unit file only specifies `Environment=WAYLAND_DISPLAY=wayland-0` and the launcher uses `Gio.DesktopAppInfo.launch()` without passing an explicitly populated `GAppLaunchContext`, all child processes inherit an environment devoid of `DISPLAY`. When an Xwayland-dependent app starts, it cannot connect to `:0` and immediately terminates.
  4. *Daemon Preload Pollution*: Custom layer-shell daemons utilizing `LD_PRELOAD=/usr/lib/libgtk4-layer-shell.so` leak that variable into child processes unless explicitly stripped. When child applications using GTK 3 or hybrid frameworks (e.g. CEF in Readest) are launched, the dynamic linker loads both GTK 4 and GTK 3 into the same address space, resulting in immediate symbol collisions (`gdk_display_manager_get`) and `SIGABRT` core dumps.
- **Resolution**:
  1. In systemd service units for UI shells and launchers (`yogabook-launcher.service`, `yogabook-control-center.service`), explicitly define both `Environment=WAYLAND_DISPLAY=wayland-0` and `Environment=DISPLAY=:0`.
  2. Implement defensive defaults within the launcher daemon code (`os.environ.setdefault("DISPLAY", ":0")` and `os.environ.setdefault("WAYLAND_DISPLAY", "wayland-0")`).
  3. Strip `LD_PRELOAD` immediately after importing shell libraries (`os.environ.pop("LD_PRELOAD", None)`).
  4. When invoking `item.app_info.launch(files, launch_context)`, construct and pass a `GdkAppLaunchContext` where `DISPLAY` and `WAYLAND_DISPLAY` are explicitly forwarded and `ctx.unsetenv("LD_PRELOAD")` is enforced.
- **Prevention Pattern**:
  In Wayland desktop environments supporting Xwayland, always ensure that custom launcher daemons, notification runners, and application spawners explicitly hold and propagate both Wayland (`WAYLAND_DISPLAY`) and X11 (`DISPLAY`) environment variables to child processes, while strictly isolating daemon-specific hooks (`LD_PRELOAD`).

---

## 15. Silent Deactivation of Input Emulation in Network KVM Daemons (Lan Mouse)

- **Date**: 2026-09-08
- **Subsystem**: Input Emulation / Wayland Protocols / Network KVM (`lan-mouse`, `zwlr_virtual_pointer_v1`)
- **Symptoms**:
  - `lan-mouse` connects to network clients, but incoming mouse and keyboard events from remote devices fail to move the local cursor or register keystrokes.
  - Lan Mouse daemon remains alive in systemd (`active (running)`), but internally drops its wlroots emulation thread after Wayland socket disconnects or pipe errors (`Broken pipe (os error 32)`).
  - The daemon does not restart automatically because the primary process did not terminate.
- **Root Cause**:
  `lan-mouse daemon` isolates its input emulation subroutines. If the Wayland compositor triggers an unhandled event (or pipe reset during auto-rotate, sleep, or config reload), the emulation thread terminates silently and marks emulation as disabled without exiting the main daemon. Remote devices trying to send input receive `"emulation is disabled on the target device"`.
- **Resolution**:
  1. Provide a recovery utility (`bin/fix-lan-mouse`) that queries the daemon via IPC (`lan-mouse cli enable-emulation`, `lan-mouse cli enable-capture`, `lan-mouse cli activate 0`) or restarts the unit if unresponsive.
  2. Map a global shortcut (`bind=SUPER,m,spawn,.../fix-lan-mouse`) in `config/mango/binds.conf` for instantaneous manual recovery.
- **Prevention Pattern**:
  Long-running input emulation daemons that lack internal auto-reconnect loops on Wayland socket errors should be equipped with low-latency IPC recovery hooks or supervisory watchdog checks.


---

## 16. Compositor-Level Multi-Finger Gesture Incompatibility with Network KVM Input Capture

- **Date**: 2026-09-09
- **Subsystem**: Wayland Input Capture / Touchpad Gestures / Network KVM (`lan-mouse`, `layer-shell`, `libinput`)
- **Symptoms**:
  - Multi-finger touchpad gestures (e.g. 3-finger horizontal or vertical swipe) performed on the primary host touchpad fail to register on the remote client even when the pointer has transitioned onto the remote display via Lan Mouse.
  - Instead, the host compositor consumes the gesture, unexpectedly switching windows or workspaces on the host machine.
- **Root Cause**:
  1. *Compositor Gesture Interception*: Multi-finger touchpad gestures (`libinput` swipe events) are intercepted and processed by Wayland compositors (like Niri or GNOME) directly at the compositor level as global actions. They are not routed to client application surfaces.
  2. *Capture & Emulation Protocol Constraints*: Network KVM capture mechanisms (`layer-shell` or `InputCapture` portal) only expose standard `wl_pointer` events (cursor motion, button clicks, two-finger scroll axis) and `wl_keyboard` keys. Neither `layer-shell` nor the network KVM protocol serializes or forwards multi-touch touchpad swipe gestures (`zwp_pointer_gestures_v1`).
- **Resolution**:
  1. In the client compositor's configuration (`binds.conf`), map window scrolling and workspace switching to modifier-assisted axis events (`axisbind=SUPER,LEFT/RIGHT,focusdir` and `axisbind=SUPER,UP/DOWN,viewto{left,right}`). Two-finger scrolling generates standard `wl_pointer.axis` events that are cleanly captured and forwarded across the network KVM bridge.
  2. Provide standard keyboard shortcuts (`SUPER+H/J/K/L` or `SUPER+Arrows`) for remote navigation.
  3. Reserve native multi-finger swipe gestures strictly for physical interaction on the client machine's local touchpad.
- **Prevention Pattern**:
  In multi-device software KVM setups, never expect raw multi-finger touchpad swipe gestures to traverse Wayland compositor boundaries. Always establish dual-mapping parity using modifier-assisted pointer axis events (`Mod + Axis`) and keyboard bindings to ensure remote navigability.

---

## 17. Magnetic Hall Sensor Blind Zone in 2-in-1 Convertible Tablets & Evdev Input Suppression

- **Date**: 2026-09-09
- **Subsystem**: Input Subsystem / Hinge Angle Detection / Sensor Fusion (`evdev`, `udev`, `lenovo-yogabook`, `Goodix-TS`)
- **Symptoms**:
  - Extending or holding a convertible 2-in-1 device beyond flat tablet orientation (e.g. $> 190^\circ$ up to $350^\circ$, tent mode, or easel mode) leaves the bottom capacitive touch keyboard active.
  - Fingers supporting the device from behind trigger accidental keypresses, cursor jumps, and haptic motor vibrations.
  - The keyboard LED backlight remains illuminated on the rear surface, wasting battery.
- **Root Cause**:
  1. *Hardware Hall Sensor Proximity Limit*: Low-level kernel drivers (e.g. `lenovo_yogabook`) bind tablet mode deactivation strictly to magnetic lid/backside Hall switches (`backside_hall_sw`). Because the magnetic field drops off rapidly ($1/r^3$), Hall sensors only fire at complete $360^\circ$ physical contact. At intermediate angles ($180^\circ - 350^\circ$), the hardware sensor is blind.
  2. *Driver Coupling*: Without an accelerometer-aware daemon actively suppressing input, the touch controller continues feeding touch coordinates to userspace handlers (`touch_keyboard_handler`).
- **Resolution**:
  1. Calibrate real-time hinge angles in userspace using dual-accelerometer 2D cross-section projection (`bin/yogabook-autorotate`).
  2. Define an explicit deactivation threshold (`opening >= 190.0°`) with an 10° hysteresis window (`opening <= 180.0°`).
  3. Deploy udev access rules (`TAG+="uaccess"`, `GROUP="video"`) in `/etc/udev/rules.d/62-yogabook-keyboard.rules` allowing the user session to open and exclusively grab (`EVIOCGRAB` via `python-evdev`) the touch digitizer and virtual input nodes, and install a polkit rule (`49-yogabook-keyboard.rules`) for non-blocking supervisor control.
  4. Acquire exclusive evdev grabs on the physical touch controller (`Goodix Capacitive TouchScreen`) and virtual nodes to block all keystrokes, mouse moves, and haptic vibrations without tearing down compositor devices.
  5. Extinguish LED illumination via sysfs (`/sys/class/leds/ybwmi::kbd_backlight/brightness = 0`) on fold, and restore saved levels on unfolding.
- **Prevention Pattern**:
  Never rely solely on magnetic Hall effect switches for peripheral deactivation across the continuous hinge lifecycle of 2-in-1 convertibles. Combine continuous accelerometer hinge measurement with non-destructive userspace exclusive grabs (`EVIOCGRAB`) and sysfs power/backlight controls.

---

## 18. Unstripped ANSI Terminal Escape Codes in CLI Output Wrappers & Pango Glyph Corruption

- **Date**: 2026-09-10
- **Subsystem**: Settings Application / CLI Interop / Pango Layout / Wireless GUI (`iwctl`, `iwd`, `bluetoothctl`, `Libadwaita`, `GTK4`)
- **Symptoms**:
  - Wi-Fi network lists in desktop settings app display corrupted SSID names, table headers appearing as networks, and strange characters or square tofu boxes (`[001B]`).
  - Connecting to new networks fails silently, drops credentials, or opens external terminal emulators that immediately terminate.
  - Repeatedly scanning networks duplicates rows and accumulates stale UI elements.
- **Root Cause**:
  1. *Unconditional ANSI Formatting*: Low-level network utilities (such as `iwctl`) emit ANSI color and cursor formatting codes (`\x1b[1;90m`, `\x1b[0m`, `\x1b[90m`) unconditionally across subprocess pipes.
  2. *Column Shift via Naive Splitting*: Tokenizing raw outputs via `re.split(r'\s{2,}', line)` breaks when spaces within or adjacent to ANSI sequences fragment table lines. In `iwctl`, connection markers (`>`) followed by ANSI codes shift actual network SSIDs into signal strength columns and turn header rows into fake network entries.
  3. *Tofu Glyph Rendering*: Non-printable control bytes (`0x1B` ESC) have no glyph representations in standard typography (`Adwaita Sans`), causing Pango to render missing glyph tofu boxes with hexadecimal indices (`[001B]`).
  4. *External Terminal Spawning on Touch Devices*: Spawning `foot -e iwctl station wlan0 connect <SSID>` bypasses native authentication flows, fails to trigger on-screen virtual keyboards (`wvkbd`) on touch devices, and closes instantly on handshake failure.
  5. *Unmanaged Row Accumulation*: `Adw.PreferencesGroup` retains previously added children unless explicitly tracked and removed via `group.remove(row)`.
- **Resolution**:
  1. Filter all CLI output streams through a universal ANSI stripping regex (`re.compile(r'\x1b(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')`) before string parsing.
  2. Parse columns dynamically based on header index boundaries (`header.find("Security")`, `header.find("Signal")`) and request numerical signal metrics (`rssi-dbms`).
  3. Map signal strengths to standard desktop symbolic icons (`network-wireless-signal-*-symbolic` and secure variants) and format human-readable percentage and dBm subtitles.
  4. Replace external terminal popups with native Libadwaita dialogs (`Adw.AlertDialog` with `Adw.PasswordEntryRow`) and execute non-blocking connections using `iwctl --dont-ask --passphrase <key> station <wlan> connect <SSID>`.
  5. Maintain dynamic row tracking arrays (`self.network_rows`, `self.bluetooth_rows`) and systematically clear child widgets before re-populating preference groups.
  6. Sanitize dynamic text and eliminate non-native typographical symbols (`®`, `™`, raw Unicode bullet dots) using `GLib.markup_escape_text()`.
- **Prevention Pattern**:
  Never ingest CLI utility outputs into graphical interfaces without explicitly stripping ANSI escape sequences. Always employ header-indexed positional slicing rather than greedy space-splitting on tabular terminal output, and manage authentication state via native modal dialogs rather than spawning interactive terminal wrappers.

---

## 19. Peripheral Re-Enumeration & State Inconsistency Across Hardware Hall Switch Boundaries

- **Date**: 2026-09-10
- **Subsystem**: Input Subsystem / Convertible Hardware States / Daemon Lifecycle (`udev`, `systemd`, `Goodix-TS`, `touch_keyboard_handler`, `yogabook-autorotate`)
- **Symptoms**:
  - Folding the device into full tablet mode ($360^\circ$) and then partially reopening it while remaining in tablet posture ($> 190^\circ$) causes the capacitive touch keyboard to unexpectedly re-enable.
  - The keyboard LED backlight remains extinguished, but touches on the rear capacitive surface register keypresses and pointer movement.
- **Root Cause**:
  1. *Hardware Hall Switch Power Cycling*: At $360^\circ$, physical contact activates the backside Hall effect switch, causing the kernel to unbind/power-down the touch controller (`i2c-GDIX1001:00`), destroying the existing evdev input node.
  2. *Asynchronous Udev Re-Triggering*: When partially reopening past $360^\circ$ (e.g. at $250^\circ$), the magnet disengages and the kernel re-enumerates the controller, generating a new `/dev/input/event*` node. The system udev rule (`60-touch-keyboard.rules`) matches the new device and asynchronously triggers `systemctl start touch-keyboard-handler.service`.
  3. *Static One-Shot State Assumptions*: In the autorotation daemon (`yogabook-autorotate`), `HaloKeyboardManager.disable()` used a one-shot guard (`if self.is_disabled: return`). Because the posture was already marked disabled, the daemon ignored the newly spawned background service, failed to grab the newly created evdev node, and held stale file descriptors from the previous instance.
  4. *Trigonometric Discontinuity at $360^\circ$*: At the $\pm 180^\circ$ branch cut of `atan2`, tiny sensor jitter at $360^\circ$ folded back-to-back caused `opening = (180 - angle_2d) % 360` to oscillate to $0.0^\circ$, falsely triggering `enable()` before flipping back to $> 190^\circ$.
- **Resolution**:
  1. Transform `HaloKeyboardManager.disable()` from a passive one-shot flag into an active reconciliation loop (`maintain_disabled()`): on every cycle while in disabled posture, verify whether `touch-keyboard-handler.service` has been started by udev or if ungrabbed keyboard nodes exist in `/dev/input/`, stopping the service and acquiring `evdev.grab()` on new nodes immediately.
  2. In `get_posture()`, implement a branch cut guard for angles near $360^\circ$: when the base is flipped (`base_is_flat == False`) or the system is in tablet mode, map opening angles near $0^\circ$ to $360^\circ$, preventing false drops below $180^\circ$.
- **Prevention Pattern**:
  In convertible systems where hardware switches dynamically power-cycle or destroy peripheral buses, never rely on one-shot state flags for peripheral suppression. Always implement active state reconciliation that continuously verifies device existence, grabs, and background service states against the active physical posture.

---

## 20. Qt Platform Disconnect & Dual Preset Path Migration Collisions in EasyEffects Headless Daemons

- **Date**: 2026-09-10
- **Subsystem**: Audio Subsystem / PipeWire DSP / User Systemd Daemons (`easyeffects`, `PipeWire`, `Qt6`, `lsp-plugins-lv2`)
- **Symptoms**:
  - Launching `easyeffects --service-mode` via systemd user services or CLI fails immediately with:
    ```text
    qt.qpa.xcb: could not connect to display
    qt.qpa.plugin: Could not load the Qt platform plugin "xcb"
    ```
  - Presets placed concurrently in `~/.config/easyeffects/output/` and `~/.local/share/easyeffects/output/` cause startup errors:
    ```text
    presets_directory_manager.cpp: Old ~/.config/easyeffects/output directory detected. Migrating its files...
    util.cpp: Copy Error: Failed to copy ... Reason: File exists
    ```
  - High CPU usage or audio stuttering if complex convolvers or multi-band dynamics processors are applied on ultra-low-power Intel Atom SoCs.
- **Root Cause**:
  1. *Qt6 Wayland Platform Selection*: In standalone Wayland sessions (MangoWC/wlroots) where X11/Xcb is secondary or unexported, Qt6 binaries default to the XCB platform unless `QT_QPA_PLATFORM=wayland` and `WAYLAND_DISPLAY=wayland-0` are explicitly exported in the systemd user service environment.
  2. *Legacy Path Migration Collision*: Starting in EasyEffects v8 (Qt6/Kirigami rewrite), the canonical user presets path was migrated from `~/.config/easyeffects/output` to `~/.local/share/easyeffects/output` (XDG_DATA_HOME). If both directories exist and point (via symlinks) to the same physical repository folder, the built-in migration handler attempts to copy files over themselves, aborting the migration with an unhandled file collision.
  3. *SoC DSP Saturation*: Low-power Atom x5 cores cannot sustain high-tap FIR filters or multi-stage multiband splitting alongside video decoding without periodic audio buffer underruns.
- **Resolution**:
  1. In `config/systemd/user/easyeffects.service`, declare explicit environment keys:
     ```ini
     Environment=WAYLAND_DISPLAY=wayland-0
     Environment=QT_QPA_PLATFORM=wayland
     ExecStart=/usr/bin/easyeffects --service-mode
     ```
  2. Canonicalize repository dotfiles to link solely to `~/.local/share/easyeffects/output/` and eliminate legacy symlinks in `~/.config/easyeffects/output/`.
  3. Deploy a streamlined 3-stage DSP chain (`YogaBook-Speakers.json`) using lightweight, SIMD-optimized `lsp-plugins-lv2`:
     - High-Pass Filter (HPF 110 Hz, 12 dB/oct) to prevent tiny transducer bottoming-out and chassis rattle.
     - 10-Band Parametric Equalizer targeting resonance dips (2.2 kHz) and vocal presence (4.8 kHz).
     - Upward Compressor and True-Peak Limiter (-0.5 dB ceiling) for consistent volume without clipping.
- **Prevention Pattern**:
  When managing Qt6-based background services in Wayland environments, always declare `QT_QPA_PLATFORM=wayland` and `WAYLAND_DISPLAY` explicitly in systemd service definitions. In software migrations where application vendors transition data paths from `XDG_CONFIG_HOME` to `XDG_DATA_HOME`, avoid multi-linking both paths to avoid internal migration collisions.

---

## 21. RealtimeKit Canary Watchdog SIGKILL Loop on Atom SoCs & ALSA DMA Buffer Tuning

- **Date**: 2026-09-10
- **Subsystem**: Audio Pipeline / PipeWire / Process Scheduling / ALSA Hardware Driver (`rtkit`, `easyeffects`, `wireplumber`, `pipewire`, `cht-yogabook`)
- **Symptoms**:
  - Initial symptom: Sporadic audio micro-stutters accompanied by a mechanical "pop" or "scratch" sound resembling a 3.5mm headphone jack being physically plugged in. `pw-top` showed rapid accumulation of `ERR` (XRUNs) on the ALSA sink and Chromium streams.
  - Secondary regression upon installing `rtkit`: YouTube videos play for exactly 1 second and then stall/pause continuously. Journal logs reveal `easyeffects.service` crashing every 2-3 seconds with `status=9/KILL` (`Main process exited, code=killed, status=9/KILL`).
- **Root Cause**:
  1. *Hardware DMA Jitter & DAC Suspend*: The initial stutters and pop sounds were caused by `api.alsa.headroom = 0` combined with dynamic ALSA sink suspension (`session.suspend-timeout-seconds`) and period phase mismatch against the Intel SST Cherryview DSP hardware.
  2. *RTKit Canary Watchdog Timeout on Low-Power Cores*: When `rtkit` is deployed, EasyEffects requests `SCHED_RR` (real-time priority 1). RealtimeKit runs an active canary watchdog (`canary-watchdog-msec`, default 200ms). On Intel Atom x5-Z8550 (ultra-low-power in-order cores), C++/Qt6 userspace DSP filter processing exceeds the watchdog's strict real-time CPU budget. The kernel/RTKit forcefully terminates EasyEffects with `SIGKILL` (signal 9).
  3. *D-Bus Socket-Activation Trap*: Disabling `rtkit-daemon.service` via systemctl is bypassed by `/usr/share/dbus-1/system-services/org.freedesktop.RealtimeKit1.service`, which automatically respawns RTKit whenever an audio process queries D-Bus, re-initiating the kill loop.
  4. *Browser Audio Sink Loss*: When EasyEffects dies, its virtual sink disappears, causing Chromium's HTML5 video player to freeze/pause after playing the unbuffered 1-second video slice.
- **Resolution**:
  1. Completely purge the `rtkit` package (`sudo pacman -Rns rtkit`) to eliminate the D-Bus activation trigger and killer watchdog entirely.
  2. In `config/systemd/user/easyeffects.service`, set `LimitRTPRIO=0` to permanently restrict userspace DSP to stable CFS scheduling.
  3. Fix the underlying hardware DMA underruns cleanly in `config/wireplumber/wireplumber.conf.d/50-yogabook-alsa.conf`:
     - `api.alsa.period-size = 1024` (matches PipeWire quantum).
     - `api.alsa.headroom = 1024` (provides DMA hardware buffer safety margin).
     - `session.suspend-timeout-seconds = 0` (keeps the DAC awake, eliminating the wake-up pop).
  4. In `config/pipewire/pipewire.conf.d/10-rates-quantum.conf`, fix `default.clock.quantum = 1024` (min 512, max 2048) at 48000 Hz.
- **Prevention Pattern**:
  On low-power mobile SoCs (such as Intel Atom/Cherry Trail), never deploy `rtkit` with complex userspace DSP daemons. The in-order cores will inevitably trigger RT watchdog timeouts and catastrophic `SIGKILL` termination loops. Always isolate userspace DSP to CFS (`LimitRTPRIO=0`).

---

## 22. Permanent ALSA Handle Lock (`suspend-timeout-seconds = 0`) & Broken Pipe Deadlock on SoC Standby

- **Date**: 2026-09-11
- **Subsystem**: Audio Pipeline / WirePlumber / Intel SST DMA Driver / Connected Standby (`pipewire`, `wireplumber`, `intel_sst_acpi`, `s2idle`)
- **Symptoms**:
  - After resuming the device from suspend/sleep (`s2idle`), videos play in Chromium and media players with visible progress, but the physical speakers produce absolute silence.
  - PipeWire journal logs show recurring unrecoverable ALSA errors:
    ```text
    spa.alsa: hw:1,0p: (1 suppressed) snd_pcm_avail after recover: Broken pipe
    ```
  - Userspace monitoring (`pw-record`, EasyEffects level meters) shows active signal processing and high amplitudes, yet no acoustic output reaches the speakers.
- **Root Cause**:
  1. *Hardware Power-State Loss during Connected Standby*: On Intel Cherry Trail platforms (`intel_sst_acpi`), the DSP hardware and audio clocking are powered down during system suspend.
  2. *Permanent File Descriptor Lock*: Forcing `session.suspend-timeout-seconds = 0` in WirePlumber to keep the audio DAC awake permanently prevents PipeWire from ever closing or suspending the ALSA PCM device.
  3. *Unrecoverable Broken Pipe State*: When the kernel wakes from `s2idle`, the active audio stream handle `hw:1,0` is desynchronized because the hardware DMA engine was power-cycled beneath it. PipeWire's internal `snd_pcm_recover()` call fails with `-EPIPE` (Broken pipe), leaving the stream in a wedged zombie state where buffer pointers advance in memory without being transferred across the I2S bus.
  4. *Period Size Mismatch*: Forcing `api.alsa.period-size = 1024` conflicts with the Intel SST platform driver's fixed period size of 1008 frames (`buffer_size: 34272`), inducing a 16-sample periodic phase drift.
- **Resolution**:
  1. In `config/wireplumber/wireplumber.conf.d/50-yogabook-alsa.conf`, set `api.alsa.headroom = 1024` to maintain a 1024-sample DMA safety cushion, completely eliminating playback XRUN pops and jitter on the Atom CPU.
  2. Maintain `session.suspend-timeout-seconds = 5` (dynamic node suspension), enabling WirePlumber to cleanly release and close the ALSA file descriptor when idle and across system power-state transitions (`s2idle`), preventing the post-resume `Broken pipe` deadlock.
  3. Allow `period-size` to negotiate dynamically against the native 1008-frame hardware period rather than forcing 1024.
  4. In `config/pipewire/pipewire.conf.d/10-rates-quantum.conf`, set `default.clock.min-quantum = 64` to provide full dynamic headroom for stream adapters.
- **Prevention Pattern**:
  Do not conflate playback DMA buffer underruns with DAC idle sleep. Always solve playback XRUN clicks by tuning hardware headroom (`api.alsa.headroom >= 1024`), while strictly keeping `session.suspend-timeout-seconds > 0` on SoC platforms that implement Connected Standby (`s2idle`) to prevent unrecoverable kernel/DSP descriptor deadlocks upon system resume.

---

## 23. EasyEffects v8 (Qt6) Runtime KConfig DB Desynchronization Causing Acoustic Distortion

- **Date**: 2026-09-11
- **Subsystem**: DSP Processing / EasyEffects v8 / Audio Tuning (`easyeffects`, `equalizerrc`, `compressorrc`)
- **Symptoms**:
  - Harsh harmonic clipping, diaphragm bottoming-out, and resonant distortion on dialogue/voices (especially male/baritone fundamentals) during video and media playback at moderate to high master volume (>50%).
  - The tuned JSON profile (`YogaBook-Speakers.json`) on disk contained gentle high-pass and negative notch cuts, but the acoustic output behaved as if excessive low-end boost was still active.
- **Root Cause**:
  1. *EasyEffects v8 Architecture Shift*: Unlike older GTK3/GSettings-based versions of EasyEffects, EasyEffects v8 (Qt6/Kirigami) caches and prioritizes its live DSP filter parameters inside an internal KConfig INI database located at `~/.config/easyeffects/db/` (`equalizerrc`, `compressorrc`, `limiterrc`).
  2. *Runtime DB Desynchronization*: Modifying preset JSON files on disk does not automatically trigger an update of the internal KConfig database. Restarting `easyeffects.service` (`--service-mode`) merely reloads the existing stale values stored in `~/.config/easyeffects/db/`.
  3. *Unsynchronized Boost Overlap*: The stale KConfig state held an outdated aggressive profile with `band0Frequency=110`, `band1Gain=+3.5 dB` at 180 Hz, and an Upward compressor (`mode=1`) with `boostAmount=+5.0 dB`. This resulted in a massive +8.5 dB cumulative boost around 150–200 Hz, overdriving the miniature tablet transducers into mechanical bottoming-out.
- **Resolution**:
  1. Force EasyEffects to reload and re-parse the JSON preset from disk by toggling presets via the CLI or updating the KConfig database:
     ```bash
     WAYLAND_DISPLAY=wayland-0 QT_QPA_PLATFORM=wayland easyeffects -l <preset-name>
     ```
  2. Verify that stopping `easyeffects.service` flushes the corrected values into `~/.config/easyeffects/db/equalizerrc` (`band0Frequency=150`, `band1Gain=-1.5`) and `~/.config/easyeffects/db/compressorrc` (`mode=0` Downward, `boostAmount=0`).
  3. Restart the service (`systemctl --user restart easyeffects.service`) to ensure all PipeWire DSP filter nodes reflect the tuned parameters.
- **Prevention Pattern**:
  Never assume that editing an EasyEffects JSON preset file will alter DSP behavior across service restarts. Always verify and synchronize the active runtime KConfig database in `~/.config/easyeffects/db/` using `easyeffects -l <preset>` or direct DB inspection.
