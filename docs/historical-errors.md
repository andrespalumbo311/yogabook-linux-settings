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

---

## 24. Serdev Driver Initialization Race with Early RFKill & Kernel Printk Console Bleeding over TUI Display Manager

- **Date**: 2026-09-12
- **Subsystem**: Display Manager / Bluetooth Serdev Driver / Kernel Console Log Level (`greetd`, `tuigreet`, `hci_bcm`, `bluez`, `printk`, `systemd-boot`)
- **Symptoms**:
  - Visual glitch inside the `tuigreet` password field on VT1 at boot: terminal error lines related to Bluetooth timeouts (`command 0xfc45 tx timeout`, `BCM: failed to write clock (-110)`, `BCM: Reset failed (-110)`) are printed directly over the login form.
  - The login prompt functions normally and user authentication succeeds (the text is purely an artifact stamped on the terminal framebuffer, not present in the input buffer).
  - The Bluetooth adapter remains wedged in an uninitialized/failed state until manually power-cycled.
- **Root Cause**:
  1. *Premature RFKill Hardware Disconnection*: An aggressive systemd oneshot unit (`bluetooth-default-off.service`) invoked `rfkill block bluetooth` concurrently while the kernel Broadcom UART driver (`hci_bcm`) was uploading the firmware patch (`BCM4356A2.hcd`) and negotiating baudrate/clocking. Cutting power mid-transaction triggered a hardware UART timeout (`-110 = ETIMEDOUT`).
  2. *Unsilenced Kernel Console Printk*: The default kernel loglevel was unconstrained (`kernel.printk = 7 4 1 7`) and the bootloader options lacked `quiet loglevel=3`, routing all kernel warnings and driver timeouts directly to the active virtual terminal.
  3. *TUI Framebuffer Overwrite*: `greetd` launches `tuigreet` on VT1 (`vt = 1`). Because the driver timeout occurred 2 seconds after `tuigreet` rendered its initial screen, the kernel printed messages across the TTY buffer, corrupting the ncurses display.
- **Resolution**:
  1. Retire the premature `bluetooth-default-off.service` and configure BlueZ's native daemon policy `AutoEnable=false` in `system/etc/bluetooth/main.conf`. This allows `hci_bcm` to complete its ACPI/serdev probe and firmware upload cleanly while keeping the radio powered off at boot.
  2. Update `yogabook-control-center` (`check_bt_active`) and `yogabook-settings` to evaluate both `rfkill` soft-block and BlueZ `Powered` status so the UI accurately displays Bluetooth as inactive when `AutoEnable=false` is in effect.
  3. Add `kernel.printk = 3 4 1 3` to `system/etc/sysctl.d/99-zram-performance.conf` to block kernel warning/info messages from reaching the console TTY.
  4. In `install.sh`, automate appending `quiet loglevel=3` to systemd-boot configuration entries (`/boot/loader/entries/yogabook.conf`) and synchronizing `/etc/bluetooth/main.conf`.
- **Prevention Pattern**:
  Never enforce default-off power management by firing abrupt hardware-level `rfkill` cuts early during boot on UART/serdev SoC platforms. Always rely on Bluetooth daemon policy (`AutoEnable=false`) to ensure firmware patches and clock configuration finish gracefully. Additionally, when using console/TUI greeters (`tuigreet`), always suppress kernel console verbosity (`quiet loglevel=3` and `kernel.printk <= 3`) to prevent asynchronous driver diagnostics from contaminating the login interface.

---

## 25. Broadcom FullMAC (brcmfmac) Firmware Rekey Drops & Uncoordinated Mesh Roaming Handshake Desynchronization

- **Date**: 2026-09-12
- **Subsystem**: Wireless Networking / Kernel Drivers / WPA2 Handshake (`brcmfmac`, `iwd`, `systemd-networkd`, `wireless-regdb`)
- **Symptoms**:
  - Systematic Wi-Fi disconnection every 10 minutes (precisely 600 seconds) on the clock:
    ```text
    iwd[460]: Received Deauthentication event, reason: 2, from_ap: true
    systemd-networkd[334]: wlan0: Lost carrier
    systemd-networkd[334]: wlan0: DHCP lease lost
    ```
  - Spurious connection drops and multi-second freezes during mesh roaming across multiple BSSIDs sharing the same SSID:
    ```text
    iwd[460]: event: state, old: connected, new: fw-roaming
    iwd[460]: 4-Way handshake failed for ifindex: 2, reason: 1
    kernel: ieee80211 phy0: brcmf_run_escan: error (-52)
    iwd[460]: Received error during CMD_TRIGGER_SCAN: Invalid exchange (52)
    ```
  - Kernel boot logs report missing regulatory database:
    ```text
    kernel: faux_driver regulatory: Direct firmware load for regulatory.db failed with error -2
    kernel: cfg80211: failed to load regulatory.db
    ```
- **Root Cause**:
  1. *WPA2 GTK Rekey Dropped over NL80211 Control Port*: Access points on mesh topologies (e.g. AVM Fritz!Box / Fritz!Repeater) enforce a periodic 600-second Group Temporal Key (GTK) renewal. By default, `iwd` listens for EAPoL frames via nl80211 control port (`ControlPortOverNL80211=true`). The Broadcom FullMAC PCIe driver/firmware (`brcmfmac`) does not reliably deliver incoming GTK renewal frames over netlink, exacerbated by 802.11 power saving sleeping through DTIM multicast beacons. Because `iwd` never receives Message 1 to return Message 2, the AP times out and issues a forced IEEE 802.11 Deauthentication with code 2 (`WLAN_REASON_PREV_AUTH_NOT_VALID`).
  2. *Autonomous Firmware Roaming Desynchronization*: In environments with multiple mesh APs, the Broadcom firmware's internal roaming engine (`roamoff=0` default) autonomously attempts BSSID migrations in the background. The on-chip roaming engine fails to coordinate the 4-way WPA handshake with the userland supplicant, triggering handshake timeouts (`reason: 1`) and scan engine stalls (`EBADE -52`).
  3. *Uninstalled Regulatory Database*: The absence of `wireless-regdb` on the minimal rootfs forces `cfg80211` into the global generic "world domain" (`00`), restricting transmission power (Tx power throttled to ~12–14 dBm instead of 20 dBm / 100 mW allowed in Italy/EU) and degrading SNR and throughput.
- **Resolution**:
  1. Disable internal firmware roaming in `system/etc/modprobe.d/brcmfmac.conf`:
     ```ini
     options brcmfmac roamoff=1
     ```
     This delegates roaming decisions cleanly to userland (`iwd`) without firmware-level races.
  2. Enforce raw PAE socket frame processing and disable Wi-Fi power save in `system/etc/iwd/main.conf`:
     ```ini
     [General]
     ControlPortOverNL80211=false
     RoamThreshold=-70
     RoamRetryInterval=60

     [DriverQuirks]
     DefaultInterface=brcmfmac
     ForcePae=brcmfmac
     PowerSaveDisable=brcmfmac
     ```
  3. Install `wireless-regdb` and `iw` (`sudo pacman -S --needed wireless-regdb iw`) and define `WIRELESS_REGDOM="IT"` in `system/etc/conf.d/wireless-regdom`.
  4. Incorporate `modprobe.d/brcmfmac.conf`, `iwd/main.conf`, and `conf.d/wireless-regdom` into `install.sh --system` for deterministic synchronization.
- **Prevention Pattern**:
  On FullMAC wireless chipsets (Broadcom, Realtek) managed by modern userland daemons like `iwd`, never rely on nl80211 control port forwarding for EAPoL frames nor leave on-chip firmware roaming enabled in multi-AP/mesh environments. Always configure raw PAE socket capture (`ForcePae`), disable firmware-level roaming (`roamoff=1`), and ensure `wireless-regdb` is explicitly present in the base package set.

---

## 26. MediaTek Pump Express+ Kernel Workqueue Freezing in Connected Standby Causing Slow Charging (5V Trickle)

- **Date**: 2026-09-15
- **Subsystem**: Power Supply / Battery Charging / Kernel Workqueues / Sleep Inhibitors (`bq25890_charger`, `cht_wcove_pwrsrc`, `systemd-logind`, `systemd-inhibit`)
- **Symptoms**:
  - Connecting the charger while the device is in standby (`s2idle`) or suspended with the lid closed results in extremely slow charging (~2.1W / +6% per hour), requiring over 15 hours for a full charge.
  - No visual or haptic feedback occurs upon connecting the charger during standby (LED driver `cht_wcove_leds` and display are asleep).
  - If the charger is plugged in right as the device is entering suspend, I2C transactions collide with bus suspend, producing kernel warnings (`__i2c_smbus_xfer`, `Error reading/writing extchgrirq reg`, `driver failed to report voltage_now property: -108`).
  - As soon as the device is physically woken up (lid opened), charging speed abruptly jumps to 24W (~11W net into battery, +40% per hour), logging `Hi-voltage charging requested, input voltage is 11300000 mV`.
- **Root Cause**:
  1. *Software-Driven High-Voltage Negotiation*: The Lenovo Yoga Book fast charger utilizes MediaTek Pump Express+ (PE+) high-voltage stepping (5V -> 7V -> 9V -> 12V / 11.3V). The TI BQ25892 charger IC cannot negotiate Pump Express autonomously in hardware; negotiation is driven by a Linux kernel delayed workqueue (`bq25890_pump_express_work`) with an initial 5-second start delay (`PUMP_EXPRESS_START_DELAY`).
  2. *Immediate Re-Suspend & Workqueue Freezing*: While the PMIC generates a wake interrupt on VBUS insertion that briefly resumes the SoC, `systemd-logind` evaluates `HandleLidSwitch=suspend` and immediately forces the system back to `s2idle` before the 5-second timer elapses. Furthermore, with `LidSwitchIgnoreInhibited=yes` by default, standard sleep inhibitors are ignored during lid-closed states.
  3. *Un-negotiated 5V / 500mA Fallback*: Because the workqueue never executes while suspended, the power brick remains at 5V baseline, and the charger IC restricts input current limit to standard SDP 500mA (2.5W).
  4. *Unmasked Driver Sysfs Register Retention*: The TI BQ25892 driver preserves the programmed `input_current_limit` register value (2000000 uA) in sysfs across physical cable disconnections. Naive scripts checking `input_current_limit >= 2000000` immediately exit upon re-plugging, believing fast charge is already established and dropping the inhibitor before Pump Express negotiation even begins.
  5. *Unregistered PMIC IRQ Wake Sources*: Neither `extcon-intel-cht-wc.c` (`cht_wcove_pwrsrc`) nor `bq25890_charger.c` implement `enable_irq_wake()`. When in `s2idle`, the interrupt controller masks IRQ 119 and 134, meaning plugging the cable while already asleep does not wake the CPU autonomously (waking requires tapping the power button or lid opening).
- **Resolution**:
  1. In `system/etc/systemd/logind.conf.d/yogabook.conf`, set `LidSwitchIgnoreInhibited=no` to ensure high-level inhibitor locks on `handle-lid-switch:sleep` are strictly honored by systemd-logind even when the lid is closed.
  2. Implement an automated negotiator helper (`bin/yogabook-charger-negotiate`) and system service (`yogabook-charge-negotiate.service`) triggered by udev on charger connection (`65-yogabook-charging.rules`).
  3. The negotiator unconditionally acquires a block inhibitor on `handle-lid-switch:sleep` for up to 25 seconds upon new AC connection (ignoring cached `input_current_limit`), holding the SoC awake long enough for `bq25890_pump_express_work` to pulse the charger and lock in 11.3V / 12V high-voltage charging (verified via `bq27542-0/current_now >= 1800000`).
  4. Once 12V is negotiated (or after 25s max), the lock is released, allowing the system to cleanly resume `s2idle` while the hardware continues charging at full 24W power throughout standby.
- **Prevention Pattern**:
  When proprietary or software-assisted fast charging protocols (Pump Express, Quick Charge, USB PD policy engines) depend on kernel workqueues or userspace daemons to negotiate voltage/current steps, never allow the OS to immediately re-enter low-power sleep states upon charger connection. Never rely on cached sysfs limit attributes to infer active charger state across disconnects; always hold unconditional wake/sleep inhibitors upon connection to guarantee fresh protocol renegotiation.

---

## 27. Icon Font Usurping Unicode Color Emoji Fallback in Fontconfig

- **Date**: 2026-09-18
- **Subsystem**: Typography / Desktop Rendering / Fontconfig (`noto-fonts-emoji`, `fontconfig`, `Font Awesome`, GTK/Wayland)
- **Symptoms**:
  - Emoji characters render as missing blocks/tofu or black-and-white icon approximations instead of standard full-color emojis.
  - Querying `fc-match emoji` returns non-emoji fallback fonts (e.g. `OpenDyslexic` or `Liberation`).
  - Even after installing `noto-fonts-emoji`, querying generic font fallbacks for emoji codepoints (e.g. `fc-match "sans-serif:charset=1f600"`) resolves to icon fonts such as `Font Awesome 7 Free` instead of `Noto Color Emoji`.
- **Root Cause**:
  1. *Missing Base Emoji Package*: Minimal Arch installations lack a dedicated color emoji font (`noto-fonts-emoji`).
  2. *Language Weighting Distortion in Fontconfig Matching*: Color emoji fonts (such as Google Noto Color Emoji) do not advertise standard natural language tags (like `en` or `it`). Conversely, symbol/icon fonts (such as `Font Awesome`) often declare coverage for standard Latin languages (`lang: en`). During fallback matching for generic families (`sans-serif`, `monospace`), Fontconfig awards higher scoring to fonts matching the user's active system locale, causing monochrome icon fonts to match ahead of color emojis.
  3. *Digit/Punctuation Hijacking from Aggressive Strong Prepending*: Blindly prepending `Noto Color Emoji` with strong binding at the pattern level overrides standard ASCII digits (0–9) and punctuation (`#`, `*`), rendering numbers as giant boxed keycap emojis.
- **Resolution**:
  1. Install `noto-fonts-emoji` via pacman.
  2. Create a declarative `fonts.conf` (`~/.config/fontconfig/fonts.conf` symlinked from `~/yogabook-config/config/fontconfig/fonts.conf`) providing strong `<alias>` bindings for `sans-serif`, `system-ui`, `serif`, and `monospace` that prioritize the primary text/symbol font (`Adwaita Sans`, `JetBrainsMono Nerd Font Propo`) first, followed immediately by `Noto Color Emoji` for missing glyphs.
  3. Rebuild user font cache (`fc-cache -fv`) and reload bar/notification daemons.
- **Prevention Pattern**:
  In desktop environments where icon fonts (Font Awesome, Nerd Fonts) coexist with color emojis, never rely on default Fontconfig fallback heuristics across locale boundaries. Explicitly declare multi-tiered font family fallbacks with primary text fonts listed first (to safeguard ASCII digits and punctuation) and color emoji fonts declared as the immediate secondary fallback before generic symbol fonts.

---

## 28. GTK Touch Kinetic Scrolling & Micro-Jitter Zoom Conflicts in Wayland Canvas Applications

- **Date**: 2026-09-20
- **Subsystem**: Touch Input / Wayland Compositor / Canvas Rendering (`xournalpp`, `GTK3`, `libinput`, `MangoWC`)
- **Symptoms**:
  - Single-finger touchscreen scrolling in GTK3 canvas/note-taking applications (e.g. Xournal++) behaves erratically: scrolling jumps violently across pages, stutters, bounces back and forth, or flickers.
  - Panning with a single finger intermittently triggers unwanted micro-zooms or canvas scale variations.
- **Root Cause**:
  1. *GTK Kinetic/Inertial Velocity Feedback*: In GTK3 under Wayland, enabling built-in kinetic/inertial scrolling (`gtkTouchInertialScrolling=true`) causes GTK to generate synthetic momentum vectors for touch releases. In applications managing custom canvas positioning and redraw viewports (like Xournal++), these kinetic updates collide with the application's internal page layout engine, leading to overshooting and erratic page jumps.
  2. *Zero-Threshold Pinch-to-Zoom Trigger*: Setting `touchZoomStartThreshold=0` causes any microscopic contact area fluctuation or sensor jitter from a single finger to be classified as a pinch gesture rather than a single-finger pan.
  3. *Unmapped Touch Tool Fallback*: Leaving the touch device tool unassigned (`tool="none"`) defers single-finger gestures to generic container scrolling instead of the optimized internal canvas hand/pan tool.
- **Resolution**:
  1. In `~/.config/xournalpp/settings.xml`, disable GTK inertial scrolling: `<property name="gtkTouchInertialScrolling" value="false"/>`.
  2. Increase touch zoom start threshold to filter out finger jitter: `<property name="touchZoomStartThreshold" value="15"/>`.
  3. Explicitly assign the touch tool to pan: `<data name="touch"><attribute name="tool" type="string" value="hand"/></data>`.
- **Prevention Pattern**:
  In specialized GTK-based canvas, drawing, or document-viewing applications on Wayland touchscreens, disable GTK-level kinetic/inertial scrolling in favor of direct 1:1 input tracking, enforce a non-zero threshold for multi-touch pinch gestures, and bind touch input explicitly to hand/pan tools.

---

## 29. Chromium/Brave GPU Hangs, Vulkan Driver Collisions & Video Stream Macroblock Corruption on Intel Cherryview

- **Date**: 2026-09-20
- **Subsystem**: Browser Graphics Pipeline / VA-API Acceleration / GPU Reset (`brave`, `chromium`, `i915`, `vulkan_hasvk`, `VA-API`)
- **Symptoms**:
  - Video playback (e.g. YouTube in Brave or Chromium) exhibits periodic pixelated macroblocks or video corruption across portions of the frame.
  - Kernel logs show:
    ```text
    i915 0000:00:02.0: [drm] GPU HANG: ecode 8:1:8fd8ffff, in brave [...]
    i915 0000:00:02.0: [drm] Resetting rcs0 for stopped heartbeat on rcs0
    i915 0000:00:02.0: [drm] brave[...] context reset due to GPU hang
    ```
  - Browser logs exhibit Mesa warnings:
    `MESA-INTEL: warning: ... anv_device.c: The kernel reported a GTT size larger than 2 GiB but not support for 48-bit addresses`
    along with repeated VSync presentation errors (`GetVSyncParametersIfAvailable() failed`).
- **Root Cause**:
  1. *Browser Wrapper Flags Divergence*: Brave launches via `/usr/bin/brave`, which reads solely `~/.config/brave-flags.conf` rather than `chromium-flags.conf`. If `brave-flags.conf` is missing, Brave defaults to running under X11/Xwayland (`--ozone-platform=x11`) without platform or GPU overrides.
  2. *Legacy Vulkan Driver (`vulkan_hasvk`) Instability*: When Vulkan is present on the system, modern Chromium/Brave defaults to Vulkan compositing. On Intel Cherryview Gen8 graphics (Atom x5-Z8550), Mesa's `vulkan_hasvk` driver has known GTT addressing and memory management flaws that trigger `rcs0` engine hangs and kernel context resets. During reset, active video textures in VRAM are corrupted, producing visible pixelation/macroblocks before recovery.
  3. *Unaccelerated VA-API & Missing LinuxGL Pipeline*: Without explicit flags (`VaapiVideoDecodeLinuxGL`, `--ignore-gpu-blocklist`, `--enable-zero-copy`), the browser does not initialize VA-API properly under Wayland with the OpenGL ES/EGL backend.
- **Resolution**:
  1. Create `config/brave-flags.conf` and `config/chromium-flags.conf` in the repository and symlink them to `~/.config/`.
  2. Declare the required performance and stability flags:
     ```text
     --ozone-platform=wayland
     --disable-features=Vulkan
     --enable-features=VaapiVideoDecodeLinuxGL
     --ignore-gpu-blocklist
     --enable-gpu-rasterization
     --enable-zero-copy
     ```
  3. Update `install.sh` to ensure both browser flag files are idempotently linked.
- **Prevention Pattern**:
  When deploying Chromium derivatives (Brave, Edge, Vivaldi) on minimal Wayland installations with low-power SoCs, never assume flags files are shared across browser flavors. Always maintain browser-specific flag definitions (`brave-flags.conf`, `chromium-flags.conf`) that disable unstable Vulkan drivers and enforce mature OpenGL + VA-API hardware acceleration pipelines.

---

## 30. Periodic Subprocess Fork Storms in Minimal Status Bars & Native PipeWire Filter-Chain Migration

- **Date**: 2026-09-20
- **Subsystem**: Desktop Shell & Audio Engine (`Waybar`, `MangoWC`, `EasyEffects`, `PipeWire`, `Intel Cherryview`)
- **Symptoms**:
  - Waybar consuming ~1% continuous CPU in idle on Intel Atom x5-Z8550.
  - EasyEffects consuming ~204 MB RAM and ~1.8% CPU continuously even with no active audio stream.
  - Periodic CPU wakeups preventing cores from sustaining deep C-states (C6/C7).
- **Root Cause**:
  1. *Subprocess Polling Storm*: In `config/waybar/config.jsonc`, workspace indicators (`custom/ws1`..`custom/ws6`) were polled every second (`"interval": 1`). Each second, Waybar forked 6 bash scripts, which in turn spawned `date`, `stat`, `mmsg`, and `awk` (~25–30 process forks/s). On an in-order Atom CPU, this saturated cache and triggered constant context switches.
  2. *Heavyweight Audio Framework Overhead*: `easyeffects --service-mode` is a full Qt6/C++ application running in the background. While providing essential speaker EQ, it occupied over 200 MB RSS (5% of 4GB RAM) and ran continuous DSP processing.
- **Resolution**:
  1. Converted Waybar workspace indicators to event-driven signals: created [`bin/mango-workspace-watcher`](file:///home/andres/yogabook-config/bin/mango-workspace-watcher) running `mmsg watch all-tags` and signaling Waybar via `SIGRTMIN+1`, changing module intervals in `config/waybar/config.jsonc` to `"interval": "once"`.
  2. Ported the 7-band speaker correction profile directly into native PipeWire [`config/pipewire/pipewire.conf.d/20-yogabook-dsp.conf`](file:///home/andres/yogabook-config/config/pipewire/pipewire.conf.d/20-yogabook-dsp.conf) (`libpipewire-module-filter-chain`), completely disabling `easyeffects.service` and freeing ~204 MB RAM with 0.0% idle CPU.
  3. Integrated `ctypes.CDLL('libc.so.6').malloc_trim(0)` into [`bin/yogabook-launcher`](file:///home/andres/yogabook-config/bin/yogabook-launcher) and [`bin/yogabook-control-center`](file:///home/andres/yogabook-config/bin/yogabook-control-center), dropping launcher RSS from 130 MB to 75 MB.
  4. Added a CPU Energy Performance Bias (EPB) toggle tile to [`bin/yogabook-control-center`](file:///home/andres/yogabook-config/bin/yogabook-control-center) with unprivileged sysfs access via udev rule [`system/etc/udev/rules.d/70-yogabook-cpu-epb.rules`](file:///home/andres/yogabook-config/system/etc/udev/rules.d/70-yogabook-cpu-epb.rules).
- **Prevention Pattern**:
  On low-power, memory-constrained SoCs (Intel Atom, ARM Cortex-A53/A55):
  1. Never poll compositor state with periodic shell forks; always bridge compositor IPC streams (`watch`) directly to status bar signals.
  2. Avoid heavyweight GUI/framework runtime daemons for background audio DSP when native audio server modules (`filter-chain`) can perform the task in-process with 0 idle overhead.
  3. Trim resident Python/GTK4 heap memory via `malloc_trim` to prevent unmapped pages from inflating resident memory.

---

## 31. Wayland Fractional Scaling Buffer/Viewport Desynchronization in Chromium/Brave

- **Date**: 2026-09-20
- **Subsystem**: Browser UI Rendering / Wayland Fractional Scaling / Compositor Layout (`brave`, `chromium`, `MangoWC`, `Niri`, `wp_fractional_scale_v1`)
- **Symptoms**:
  - In tiling or horizontal scroller window layouts (e.g. Niri or MangoWC `scroller`), Brave windows exhibit visual rendering glitches upon resizing or mapping.
  - A vertical black bar appears on the right edge of the window, cutting off UI elements (address bar, tab strip, web content).
  - Alternatively, window content intermittently renders beyond the compositor's window boundaries, clipping into adjacent gaps or windows.
- **Root Cause**:
  1. *Wayland Fractional Scale Protocol Mismatch*: Under native Wayland (`--ozone-platform=wayland`), modern Chromium/Brave enables `WaylandFractionalScaleV1` (`wp_fractional_scale_v1`) by default.
  2. *Buffer vs Viewport Rounding Discrepancy*: When using non-integer display scaling (e.g. `scale: 1.5`), Chromium's internal compositor computes buffer allocations and viewporter destinations with floating-point math rounded to integers. In dynamic scroller/tiling layouts where window widths are continuously calculated by compositor proportions, rounding differences between the client's internal pixel buffer and the compositor's allocated surface viewport cause:
     - Underflow (buffer smaller than viewport): the unpainted surface area displays as an empty black vertical margin on the right edge.
     - Overflow (buffer larger than viewport): the rendered surface overflows the allocated compositor window geometry.
- **Resolution**:
  1. In `config/brave-flags.conf`, explicitly disable `WaylandFractionalScaleV1` alongside Vulkan:
     ```text
     --disable-features=Vulkan,WaylandFractionalScaleV1
     ```
  2. With this feature disabled, Chromium falls back to compositor-driven viewport scaling (`wp_viewport`), eliminating buffer dimension mismatches while maintaining crisp rendering and proper window geometry across scroller transitions.
- **Prevention Pattern**:
  When configuring Chromium-based browsers under Wayland compositors with fractional scaling (especially dynamic tiling or ribbon/scroller layouts), disable `WaylandFractionalScaleV1` if viewport clipping, black margin borders, or geometry overflows occur during window resizing.

---

## 32. Lack of Ambient Light Sensor (ALS) Display Auto-Brightness in Minimalist Wayland Environments

- **Date**: 2026-09-27
- **Subsystem**: Display Subsystem / Sensor Fusion / Ambient Light Regulation (`iio-sensor-proxy`, `intel_backlight`, `SensorProxy`, `MangoWC`)
- **Symptoms**:
  - The physical display does not automatically adapt its backlight to changing ambient lighting conditions (e.g. going from bright daylight into a dark room or vice versa), remaining locked at the last manual percentage.
  - Manual adjustments via sliders or function keys are cumbersome when moving across different environments.
- **Root Cause**:
  1. *Compositor & Shell Scope*: Minimalist Wayland compositors (such as MangoWC, Sway, or dwl) and modular status bars / notification centers (Waybar, SwayNC) do not incorporate integrated power and display automation daemons (unlike monolithic desktop environments such as GNOME's `gnome-settings-daemon` or KDE's `powerdevil`).
  2. *D-Bus Signal Consumer Absence*: While the Linux kernel and `iio-sensor-proxy` successfully expose ambient light sensors over D-Bus (`net.hadess.SensorProxy`), no consumer claims the light sensor (`ClaimLight`) or listens for illuminance property change events. Consequently, the hardware sensor stays unpolled and `/sys/class/backlight/intel_backlight` remains static.
- **Resolution**:
  1. Implement a dedicated, lightweight, event-driven user daemon ([`bin/yogabook-autobrightness`](file:///home/andres/yogabook-config/bin/yogabook-autobrightness)) bound to `mango-session.target`.
  2. Connect to `net.hadess.SensorProxy` via GIO D-Bus, invoke `ClaimLight`, and process live `LightLevel` lux signals.
  3. Deploy a calibrated perceptual human-eye LUT curve (logarithmic response) with:
     - Exponential Moving Average (EMA, $\alpha = 0.35$) for noise filtering.
     - Deadband hysteresis ($3\%$) to eliminate micro-fluctuations and flicker.
     - Smooth stepped ramping ($1\%$ every 30ms) for natural visual adaptation.
     - Adaptive User Bias: continuously monitors sysfs backlight to detect manual user tweaks, preserving user preference offsets across ambient light changes.
  4. Integrate status and toggle controls directly into [`bin/yogabook-control-center`](file:///home/andres/yogabook-config/bin/yogabook-control-center) (slider icon toggle) and [`bin/yogabook-settings`](file:///home/andres/yogabook-config/bin/yogabook-settings) (dedicated ALS preference row).
- **Prevention Pattern**:
  In standalone or minimalist Wayland deployments on sensor-equipped hardware (laptops, 2-in-1 tablets), never expect display backlights to adjust automatically without an active mediator. Bridge standard D-Bus sensor proxies (`net.hadess.SensorProxy`) to hardware sysfs controllers using lightweight, event-driven daemons with perceptual curve mapping, hysteresis deadbands, and adaptive user bias retention.

---

## 33. ALSA Mixer DAPM Desynchronization & Digital Microphone Pin Inactivity in Pro-Audio Profiles

- **Date**: 2026-09-27
- **Subsystem**: Audio Capture / ALSA DAPM / PipeWire Pro-Audio Profile (`cht-yogabook`, `rt5677`, `PipeWire`, `WirePlumber`)
- **Symptoms**:
  - The internal microphone captures only pure digital silence (RMS = 0.00, maximum sample value 0 / 32767) in all applications (browsers, voice recorders, WebRTC).
  - PipeWire reports default audio source volume at 100% and unmuted (`Built-in Audio Pro`), but no audio waveform or ambient noise is registered.
- **Root Cause**:
  1. *Pro-Audio Profile Hardware Isolation*: On the Lenovo Yoga Book Cherryview platform (`cht-yogabook`), the sound card operates under WirePlumber's `pro-audio` profile to maintain deterministic sink naming for the native PipeWire DSP speaker correction chain (`effect_input.yogabook_dsp` -> `alsa_output.platform-cht-yogabook.pro-output-0`).
  2. *Bypassed DAPM Mixer Management*: In `pro-audio` mode, PipeWire opens raw PCM devices (`hw:1,0`) directly without configuring ALSA mixer pins or dynamic audio power management (DAPM) widgets.
  3. *Un-asserted DMIC Pins*: The Realtek RT5677 codec requires explicit DAPM pin and mixer activation to power the MEMS digital microphone (`Int Mic Switch`), multiplex the DMIC input (`Stereo1 DMIC Mux` -> `DMIC1`, `Stereo1 ADC2 Mux` -> `DMIC`), and route it into the Stereo 1 ADC mixer (`Sto1 ADC MIXL ADC2 Switch` and `Sto1 ADC MIXR ADC2 Switch`). Because these were saved as `off`/`false` in `/var/lib/alsa/asound.state`, the codec kept the microphone input circuitry unpowered and streamed null samples.
- **Resolution**:
  1. Activate the internal microphone DAPM power switch and mixer routes via `amixer`:
     - `amixer -c 1 cset name='Int Mic Switch' on`
     - `amixer -c 1 cset name='Sto1 ADC MIXL ADC2 Switch' on`
     - `amixer -c 1 cset name='Sto1 ADC MIXR ADC2 Switch' on`
     - `amixer -c 1 cset name='Stereo1 DMIC Mux' DMIC1`
     - `amixer -c 1 cset name='Stereo1 ADC2 Mux' DMIC`
  2. Calibrate hardware capture gains: set `STO1 ADC Boost Volume` to `2` (+24 dB) and `ADC2 Capture Volume` to `33` (+7.5 dB).
  3. Ensure playback output switches remain intact: verify `Speaker Switch = on` and default sink points to `effect_input.yogabook_dsp` (`wpctl set-default`) so that the native 7-band parametric speaker DSP filter-chain remains in the playback path.
  4. Persist the soundcard hardware state to `/var/lib/alsa/asound.state` via `sudo alsactl store 1` so that `alsa-restore.service` reliably restores both speaker and microphone routes on every system boot.
  5. Ensure `install.sh --system` executes `alsactl store 1` during system synchronization workflows.
- **Prevention Pattern**:
  When deploying low-level or `pro-audio` profiles in PipeWire to bypass high-level abstraction layers, never assume hardware codec DAPM widgets and mixer muxes will automatically be configured. Always explicitly verify that input power switches (`* Mic Switch`), ADC summing bus switches (`Sto* ADC MIX*`), and output switches (`Speaker Switch`) are active, calibrated, and persistently stored in the underlying ALSA hardware state (`asound.state`). Avoid running raw UCM enable sequences (`set _verb`) without device activation, as they reset output switches to `off`.

---

## 34. Interpreted Python/GTK4 Runtime Latency & Memory Overhead in On-Demand Desktop Overlays

- **Date**: 2026-09-27
- **Subsystem**: Desktop Shell / Layer-Shell Overlays / Resource Optimization (`GTK3`, `GTK4`, `PyGObject`, `GtkLayerShell`)
- **Symptoms**:
  - Launching quick-access desktop overlays or control panels (such as `yogabook-control-center`) from scratch takes multiple seconds (2.5 – 3.5s) on low-power SoCs (Intel Atom x5-Z8550).
  - Attempting to mask cold-start latency by keeping the application resident in RAM as a background daemon (`yogabook-control-center.service`) wastes ~100MB to 140MB of memory 24/7 on a RAM-constrained device (4GB LPDDR3).
- **Root Cause**:
  1. *Dynamic Introspection Overhead*: In PyGObject, loading `Gtk 4.0`, `Gdk 4.0`, `Gtk4LayerShell 1.0`, `GLib`, and `Pango` dynamically parses megabytes of binary `.typelib` metadata files on disk on every cold invocation, resolving hundreds of C symbols and instantiating Python wrapper classes. On in-order Atom cores with eMMC storage, this phase takes ~1.2s before a single line of application logic runs.
  2. *Double-Process Trampoline (`os.execve`)*: Python wrapper scripts that dynamically prepend libraries to `LD_PRELOAD` restart the Python interpreter via `os.execve`, doubling initial process creation overhead.
  3. *GSK GPU Scene Graph Initialization*: GTK4 initializes full 3D scene graphs and compiles OpenGL/Vulkan shaders upon window creation, adding ~350ms of GPU context negotiation.
  4. *Subprocess Fork Overhead*: Running multiple synchronous CLI invocations (`wpctl`, `rfkill`, `bluetoothctl`, `swaync-client`) during UI initialization adds ~200ms of cumulative process fork latency.
- **Resolution**:
  1. Replace the interpreted Python/GTK4 control center overlay with a compiled native C binary ([`src/control-center/main.c`](file:///home/andres/yogabook-config/src/control-center/main.c)) linked against **GTK 3** and **`gtk-layer-shell`**.
  2. Use direct Linux sysfs I/O (`/sys/class/backlight/...` and `/sys/class/power_supply/...`) for 0.1ms brightness and battery queries, eliminating external CLI forks.
  3. Provide single-instance toggle semantics directly via PID checks and `SIGTERM`.
  4. Decommission and remove the background systemd service (`yogabook-control-center.service`), saving ~100MB of resident RAM while achieving **~100ms** cold-start response time.
  5. Implement periodic GLib live polling (`on_live_poll` via `g_timeout_add(250, ...)`) to synchronize brightness (ALS daemon), rotation posture, and audio volume dynamically while open. Ensure pending slider values (`pending_bright`, `pending_vol`) are explicitly initialized to `-1` (idle) so static C zero-initialization does not permanently block live update guards (`pending < 0`).
  6. Preserve exact 1:1 RFKill semantics for hardware toggles (Bluetooth, Wi-Fi): query radio states directly via `rfkill list` (`Soft blocked: no`) rather than daemon queries (`bluetoothctl show`), ensuring quick toggles reliably control radio power states and update UI tiles even when underlying SoC controller drivers encounter kernel firmware negotiation delays.
---

## 35. Touch Activation Reliability & Zero-RAM App Drawer Architecture

- **Date**: 2026-09-27
- **Subsystem**: Desktop Shell / Application Drawer / Touch Input (`GTK3`, `GtkFlowBox`, `GtkScrolledWindow`, `gtk-layer-shell`)
- **Symptoms**:
  - Application launchers on touchscreen (e.g. `nwg-drawer` or standard GTK tree views) fail to launch applications on a single finger tap, requiring a clumsy double-tap or prolonged press.
  - Keeping a Python/GTK or background drawer daemon alive to mask multi-second startup latency costs ~85MB of continuous resident RAM.
- **Root Cause**:
  1. *Kinetic Drag Threshold Absorption*: In `GtkScrolledWindow` containers, kinetic scrolling logic intercepts touch events (`GDK_TOUCH_BEGIN`, `GDK_TOUCH_UPDATE`). If finger contact shifts by even a few sub-pixels (natural touchscreen finger contact jitter), the container treats the interaction as a scroll drag gesture rather than a click/activation, dropping the activation event unless a rapid double-tap occurs.
  2. *Daemon Dependency*: Interpreted app drawers (Python) or large C++ desktop suites take 1.5–3 seconds to cold-start due to icon theme parsing and `.desktop` file enumeration, forcing administrators to run background daemons (`yogabook-launcher.service`) that occupy precious RAM.
- **Resolution**:
  1. Implement a compiled native C application launcher ([`src/launcher/main.c`](file:///home/andres/yogabook-config/src/launcher/main.c)) built with GTK 3 and `gtk-layer-shell`.
  2. Use `GtkFlowBox` with `gtk_flow_box_set_activate_on_single_click(flowbox, TRUE)` connected to the `child-activated` signal. `GtkFlowBox` reliably dispatches child activation on single touch release while gracefully tolerating touch contact jitter.
  3. Pre-load `.desktop` entries and Papirus icons with `GAppInfo` during rapid native initialization (<60ms cold start).
  4. Decommission `yogabook-launcher.service`, completely eliminating resident background RAM consumption (0 MB when closed).
- **Prevention Pattern**:
  For touch-first application launchers on Linux Wayland environments, avoid relying on standard button press events inside kinetic scroll windows. Use `GtkFlowBox` with explicit single-click activation (`activate-on-single-click=TRUE`) to guarantee reliable single-touch launching. Keep overlay launchers as native compiled C executables with zero background daemon overhead.

---

## 36. Notification Daemon Memory Bloat & D-Bus Well-Known Name Acquisition Race During Migration

- **Date**: 2026-09-27
- **Subsystem**: Notification Daemon / Desktop Shell / Memory Optimization (`Mako`, `SwayNC`, `D-Bus`, `Waybar`)
- **Symptoms**:
  - Complex notification center daemons (such as SwayNC built on GTK3/Vala with integrated audio volume monitors) consume 65MB to 116MB of continuous resident RAM 24/7 on an Atom-based device with only 4GB total RAM.
  - Attempting to switch or enable an alternative notification daemon (such as `mako`) causes the new service to fail on startup:
    ```text
    mako: Failed to acquire service name: File exists
    mako: Is a notification daemon already running?
    systemd: mako.service: Failed with result 'exit-code'.
    ```
- **Root Cause**:
  1. *Daemon Scope Bloat*: Monolithic notification centers combine pop-up notification rendering with pull-out drawer sidebars, PulseAudio volume listeners, and full GTK widget hierarchies, resulting in an order-of-magnitude larger memory and CPU footprint compared to a focused notification renderer.
  2. *D-Bus Well-Known Name Contention*: Notification daemons claim the well-known session bus name `org.freedesktop.Notifications`. When migrating between daemons in a live session, background subscription scripts (such as `swaync-client -swb` invoked by Waybar) can keep the legacy daemon alive or continuously re-trigger it via D-Bus activation, preventing the new daemon from acquiring the bus name.
- **Resolution**:
  1. Terminate the legacy daemon and all persistent subscriber client processes (`killall -9 swaync swaync-client`), disable `swaync.service`, and reset systemd failure limits (`systemctl --user reset-failed mako.service`).
  2. Deploy `mako` (a lightweight, C-based Wayland notification daemon) running as a supervised systemd user unit (`mako.service`).
  3. Style `~/.config/mako/config` to match the Material Light / Adwaita theme (`background-color=#ffffffee`, `border-radius=16`, `outer-margin=46,12,0,0` to clear Waybar).
  4. Implement Do Not Disturb (DND) mode via `[mode=dnd] invisible=1` while preserving critical battery/system notifications via `[mode=dnd urgency=critical] invisible=0`.
  5. Provide a lightweight Waybar status provider ([`bin/yogabook-mako-status`](file:///home/andres/yogabook-config/bin/yogabook-mako-status)) and update the native C Control Center ([`src/control-center/main.c`](file:///home/andres/yogabook-config/src/control-center/main.c)) to query and toggle `makoctl mode -t dnd` with instant Waybar signal dispatch (`pkill -RTMIN+9 waybar`).
  6. Memory consumption for the notification subsystem dropped from **~115MB to ~1.3MB** (a >98% reduction).
- **Prevention Pattern**:
  On memory-constrained systems (<=4GB RAM), prioritize single-responsibility notification dispatchers (`mako`) over multi-functional notification center suites. When migrating between services implementing identical D-Bus well-known names, systematically terminate client event streams before releasing the bus name to prevent activation deadlocks.

---

## 37. Interpreted Daemon Overhead & Full C-Native Migration for Resource-Constrained Hardware

- **Date**: 2026-09-27
- **Subsystem**: System Services / Hardware Daemons / Desktop Helpers (`yogabook-autorotate`, `yogabook-autobrightness`, `mango-workspace-watcher`, `sd-bus`, `libudev`, `ioctl(EVIOCGRAB)`)
- **Symptoms**:
  - Persistent Python 3 runtimes for hardware background services (`rot8.service`, `yogabook-autobrightness.service`, `mango-workspace-watcher.service`) continuously consume ~70MB–80MB of active resident RAM on an Intel Atom x5-Z8550 with only 4GB LPDDR3.
  - Interactive touch buttons (Control Center, App Launcher, Close Window, Workspace buttons) incur 15–30ms fork/exec latency due to spawning `bash`, `pgrep`, `awk`, and `kill` processes on low-IPC CPU cores.
- **Root Cause**:
  1. *CPython Runtime Overhead*: Each running Python daemon incurs an unavoidable baseline of ~15–20MB RSS plus GC cycles, regardless of how light the actual task is (e.g. reading a sysfs ALS lux value or accelerometer vector).
  2. *Process Fork Costs on Low-IPC Architectures*: Shell scripts executing pipelines (`pgrep`, `kill`, `awk`) require multiple kernel context switches and executable relocations, creating noticeable micro-stutter when triggered repeatedly from interactive Waybar widgets or touch gestures.
- **Resolution**:
  1. Migrate [`bin/yogabook-autorotate`](file:///home/andres/yogabook-config/bin/yogabook-autorotate) to native C ([`src/autorotate/main.c`](file:///home/andres/yogabook-config/src/autorotate/main.c)):
     - Integrates `libudev` for dynamic screen vs keyboard sensor discovery.
     - Direct `ioctl(fd, EVIOCGRAB, 1/0)` for Halo Keyboard suppression.
     - Hardware SSE vector math for 2D hinge projection and singularity protection.
     - Resident memory reduced from **15.4MB to 540KB** (0% CPU).
  2. Migrate [`bin/yogabook-autobrightness`](file:///home/andres/yogabook-config/bin/yogabook-autobrightness) to native C ([`src/autobrightness/main.c`](file:///home/andres/yogabook-config/src/autobrightness/main.c)):
     - Connects directly to `net.hadess.SensorProxy` via `libsystemd` (`sd-bus`) with zero Python/GIO overhead.
     - Fluid 30ms stepped ramping and human perceptual LUT interpolation.
     - Resident memory reduced from **17.8MB to 436KB**.
  3. Migrate interactive helpers and watchers to compiled C binaries ([`src/helpers/`](file:///home/andres/yogabook-config/src/helpers/)):
     - [`toggle-launcher`](file:///home/andres/yogabook-config/bin/toggle-launcher), [`toggle-control-center`](file:///home/andres/yogabook-config/bin/toggle-control-center), [`close-window`](file:///home/andres/yogabook-config/bin/close-window), [`ws-status`](file:///home/andres/yogabook-config/bin/ws-status), [`yogabook-mako-status`](file:///home/andres/yogabook-config/bin/yogabook-mako-status), [`toggle-keyboard`](file:///home/andres/yogabook-config/bin/toggle-keyboard), [`mango-workspace-watcher`](file:///home/andres/yogabook-config/bin/mango-workspace-watcher).
     - Replaces Bash pipelines with instant (<0.5ms) direct system calls (`kill`, `execve`).
  4. Provide a top-level repository [`Makefile`](file:///home/andres/yogabook-config/Makefile) and wire it into [`install.sh`](file:///home/andres/yogabook-config/install.sh) for single-command idempotent builds.
  5. Overall background daemon RAM dropped from **~80MB to ~3.2MB** (>95% reduction).
- **Prevention Pattern**:
  On resource-constrained hardware (Intel Atom, ARM SBCs, <=4GB RAM), never run persistent Python processes for continuous sensor monitoring or background daemons. Implement resident services in single-threaded native C using lightweight platform primitives (`epoll`, `libudev`, `sd-bus`, `ioctl`) and compile transient CLI helpers to eliminate fork/exec shell overhead.

---

## 38. Accelerometer Hinge Singularity Asymmetry & Double-Debounce Race During Posture Transitions

- **Date**: 2026-09-27
- **Subsystem**: Posture Detection / Auto-Rotation Daemon / Accelerometer Sensor Fusion (`yogabook-autorotate`, `iio`, `libudev`, `wlr-randr`)
- **Symptoms**:
  - Spurious, erratic orientation changes immediately upon folding into tablet mode.
  - While holding the device in portrait orientation in tablet mode, the daemon abruptly flips the display back to landscape (`270`), re-enables the Halo Keyboard, and exits tablet mode into laptop mode (~112° / 97° phantom opening angles logged).
- **Root Cause**:
  1. *Logical Asymmetry in 2D Singularity Guard*: When the 2-in-1 device is rotated into portrait mode, gravity aligns with the hinge axis ($Y$). Gravity in the perpendicular cross-section plane ($X-Z$) drops to near zero on both sensors. Evaluating `perp_s < 0.35 && perp_b < 0.35` allowed computation to proceed if a single sensor exceeded 0.35g due to slight tilt or hand jitter (e.g. `perp_s = 0.36, perp_b = 0.25`). Computing `atan2` on noisy near-zero vectors produced arbitrary phantom opening angles (e.g. 112°, 97°), fulfilling `opening <= 135°` and triggering false tablet-to-laptop reverts.
  2. *Intra-Cycle Double Debounce Evaluation*: On the transition cycle confirming `current_mode = MODE_TABLET`, the daemon invoked `tracker_update(s)` during the mode transition block and then immediately evaluated continuous tracking `tracker_update(s)` in the very same loop cycle. This incremented `pending_count` twice in 0ms, completely bypassing the 2-cycle debounce filter and locking in transient folding angles.
  3. *Uncalibrated Adaptive Polling*: Varying sleep interval between 1000ms and 300ms altered the physical debounce duration from 800ms down to 600ms, making orientation switching sensitive to dynamic swing accelerations while folding.
- **Resolution**:
  1. Enforce strict disjunction in the singularity guard: `if (perp_s < 0.35 || perp_b < 0.35)` ensures angle computation is aborted whenever *either* sensor lacks sufficient cross-sectional gravity.
  2. Insert `continue;` upon confirming mode transition to ensure continuous orientation tracking resumes strictly on subsequent cycles, guaranteeing 1 increment per 400ms cycle.
  3. Revert to a constant 400ms (`usleep(400000)`) loop, matching the calibrated Python reference.
- **Prevention Pattern**:
  In sensor-fusion algorithms calculating planar angles from two projected vectors, never compute angles if *either* vector magnitude falls below the noise floor (`mag_a < threshold || mag_b < threshold`). Furthermore, across state-machine mode transitions, never cascade multiple updates to downstream debounce filters within the same loop cycle; always yield the cycle to enforce deterministic time-based debouncing.

---

## 39. Mobile Keymap Ambiguities, Missing Symbols & AltGr Composition Clashes in Minimalist Virtual Keyboards (`wvkbd`)

- **Date**: 2026-09-28
- **Subsystem**: Virtual Touch Keyboard / Input Method / XKB Symbols (`wvkbd`, `zwp_virtual_keyboard_v1`, `MangoWC`, `us(intl)`)
- **Symptoms**:
  - Tapping the symbols/graphic layer (`⌨͕` / `special`) on the on-screen keyboard failed to present essential symbols (such as `@`, `#`, `€`, `?`, `!`, `{`, `}`, `_`, `+`, `~`); instead, it repeated digits `1–0` and required activating Shift inside the symbols layer.
  - Key symbols such as `<` and `>` emitted wrong glyphs (e.g. `ç` instead of `<`).
  - European currency symbols (`€`), ordinal/degree signs (`°`), section signs (`§`), and accented vowels (`à`, `è`, `é`, `ì`, `ò`, `ù`) were either completely missing or locked behind non-standard, tedious compose key sequences (`Cmp + e`).
- **Root Cause**:
  1. *Sub-optimal Layer Architecture*: The upstream mobile layout (`mobintl`) assigned navigation arrow keys across the entire first row and digits across the second row of the special layer. Common symbols (`@#$%^&*()`) were configured strictly as shifted keycodes on digits rather than direct single-tap keys.
  2. *AltGr Mapping Collisions in International Layouts*: Upstream `wvkbd` bound `<` and `>` to `Code, KEY_COMMA, 0, AltGr` and `Code, KEY_DOT, 0, AltGr`. On desktop compositors configured with `us(intl)` (used to enable physical keyboard dead keys), `AltGr + KEY_COMMA` resolves to `ç` in XKB, completely corrupting the `<` comparison symbol.
  3. *Missing European Glyph Definitions*: No keysym or Unicode mapping for `€` (Euro), `°` (Degree), or `§` (Section) existed in upstream `layout.mobintl.h`.
- **Resolution**:
  1. Vendor the `wvkbd` source tree into the repository ([`src/wvkbd/`](file:///home/andres/yogabook-config/src/wvkbd/)) and integrate it into the top-level [`Makefile`](file:///home/andres/yogabook-config/Makefile).
  2. Implement a dedicated, 5-row responsive touch layout ([`layout.yogabook.h`](file:///home/andres/yogabook-config/src/wvkbd/layout.yogabook.h)):
     - **Full (QWERTY)**: Symmetrical 5-row layout with top digits row and direct layer shortcuts (`?123`, `àèì`).
     - **Special (`?123`)**: Direct, single-tap access to all common symbols (`@`, `#`, `€`, `$`, `%`, `&`, `*`, `-`, `+`, `=`, `(`, `)`, `/`, `\`, `"`, `'`, `:`, `;`, `!`, `?`, `[`, `]`, `{`, `}`, `< `, `>`, `_`, `~`, `|`) without requiring Shift.
     - **Special2 (`=\<`)**: Extended typography, currencies, and math (`°`, `§`, `£`, `¥`, `^`, `` ` ``, `±`, `×`, `÷`, `≠`, `«`, `»`, `•`, `…`, `©`, `®`, `™`, arrows, and navigation controls).
     - **Accents (`àèì`)**: Immediate access to Italian and European accented vowels (`à`, `è`, `é`, `ì`, `ò`, `ù`, `ç`, `À`, `È`, `É`, `Ì`, `Ò`, `Ù`, `Ç`).
  3. Decouple non-standard symbols and comparison operators from fragile XKB modifier combinations by emitting direct Unicode codepoints via `Copy` (`0x003C` for `<`, `0x003E` for `>`, `0x20AC` for `€`, `0x00B0` for `°`, etc.).
  4. Update [`wvkbd.service`](file:///home/andres/yogabook-config/config/systemd/user/wvkbd.service) with `-l full,special,special2,accents,emoji` and an optimized landscape height of 240px.
- **Prevention Pattern**:
  In virtual on-screen keyboards for Wayland compositors, never bind graphic symbols to modifier combinations (`AltGr + Key` or `Shift + Key`) that depend on the active system keyboard variant. For graphic symbols, currency markers, and diacritics, always emit unambiguous Unicode codepoints directly via virtual keyboard keymap templates to guarantee layout-invariant character emission.

---

## 40. Asynchronous D-Bus Socket Truncation on Premature Process Termination in App Launchers

- **Date**: 2026-09-28
- **Subsystem**: Application Launchers / D-Bus Activation / GLib GIO (`yogabook-launcher`, `GDesktopAppInfo`, `pamac`, `Nautilus`, `libgio`)
- **Symptoms**:
  - Applications specifying `DBusActivatable=true` in their desktop files (such as Pamac / App Store, Nautilus, or GNOME Text Editor) fail silently to launch when selected in a custom native C launcher.
  - No error dialog is raised, no process is spawned, and fallback launch commands (`g_spawn_command_line_async`) never trigger because `g_app_info_launch()` returns `TRUE`.
  - Non-D-Bus applications (e.g. Foot, Brave, VLC) launched from the exact same launcher start without issue.
  - Searching for common synonyms or terms (e.g. "store", "app", "pamac") in the launcher search bar fails to filter or find the application if its translated desktop name differs (e.g. "Add/Remove Software" or "Aggiungi/Rimuovi software").
- **Root Cause**:
  1. *Asynchronous IPC Truncation*: `g_app_info_launch()` delegates D-Bus activatable applications via `org.freedesktop.Application.Activate` over the session bus asynchronously. Because it queues the message on the GIO connection loop and returns `TRUE` immediately, executing `gtk_main_quit()` and terminating the launcher process immediately closes the UNIX domain socket to `dbus-daemon` before the buffer can be flushed and transmitted to the kernel.
  2. *Single-Field Search Limitation*: The launcher search callback compared the input query only against `app_name` (`g_app_info_get_display_name()`), completely omitting `.desktop` metadata (`Keywords`, `Comment`, `Exec`, and desktop file ID).
- **Resolution**:
  1. In `src/launcher/main.c`, obtain the session bus connection via `g_bus_get_sync()` and invoke `g_dbus_connection_flush_sync()` immediately following `g_app_info_launch()` before invoking `gtk_main_quit()`.
  2. Implement `build_search_text()` to compile a lowercase aggregated search index comprising display name, comment/description, executable name, desktop ID, and all declared desktop keywords (`g_desktop_app_info_get_keywords()`).
  3. Recompile and install the binary via `make -C src/launcher`.
- **Prevention Pattern**:
  In ephemeral or short-lived launcher processes dispatching actions via asynchronous IPC or D-Bus (such as GLib GIO `GAppInfo` or systemd `sd-bus`), never terminate the process or event loop immediately following an IPC invocation without explicitly flushing the connection (`g_dbus_connection_flush_sync` / `sd_bus_flush`). Furthermore, application launchers must index desktop keywords and descriptions rather than relying solely on display names.



