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

