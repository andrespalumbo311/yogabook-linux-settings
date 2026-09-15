#!/usr/bin/env bash
# ==============================================================================
# Lenovo Yoga Book (YB1-X91F) Configuration Linker & Installer
# Manages symlinks between ~/yogabook-config and live system paths.
# ==============================================================================

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOME_DIR="${HOME:-/home/andres}"

# If running without an interactive terminal or with USE_ASKPASS, use graphical askpass if available
if [ -n "${SUDO_ASKPASS:-}" ] || [ "${USE_ASKPASS:-0}" = "1" ] || [ ! -t 0 ]; then
    if [ -x "$HOME_DIR/.local/bin/zenity-askpass" ]; then
        export SUDO_ASKPASS="$HOME_DIR/.local/bin/zenity-askpass"
        sudo() { command sudo -A "$@"; }
    fi
fi

link_file() {
    local src="$1"
    local dest="$2"
    local dest_dir
    dest_dir="$(dirname "$dest")"

    mkdir -p "$dest_dir"

    if [ -L "$dest" ]; then
        current_target="$(readlink "$dest")"
        if [ "$current_target" = "$src" ]; then
            echo "[OK] Symlink already points to source: $dest"
            return 0
        fi
        rm -f "$dest"
    elif [ -e "$dest" ]; then
        echo "[BACKUP] Moving existing $dest -> ${dest}.bak"
        mv "$dest" "${dest}.bak"
    fi

    echo "[LINK] $dest -> $src"
    ln -s "$src" "$dest"
}

echo "=== Linking Yoga Book User Configurations ==="

# 1. Executables (~/.local/bin)
link_file "$REPO_DIR/bin/yogabook-autorotate" "$HOME_DIR/.local/bin/yogabook-autorotate"
link_file "$REPO_DIR/bin/toggle-keyboard"     "$HOME_DIR/.local/bin/toggle-keyboard"
link_file "$REPO_DIR/bin/toggle-launcher"     "$HOME_DIR/.local/bin/toggle-launcher"
link_file "$REPO_DIR/bin/yogabook-launcher"   "$HOME_DIR/.local/bin/yogabook-launcher"
link_file "$REPO_DIR/bin/toggle-control-center" "$HOME_DIR/.local/bin/toggle-control-center"
link_file "$REPO_DIR/bin/yogabook-control-center" "$HOME_DIR/.local/bin/yogabook-control-center"
link_file "$REPO_DIR/bin/ws-status"           "$HOME_DIR/.local/bin/ws-status"
link_file "$REPO_DIR/bin/yogabook-settings"    "$HOME_DIR/.local/bin/yogabook-settings"
link_file "$REPO_DIR/bin/yogabook-display-mgr" "$HOME_DIR/.local/bin/yogabook-display-mgr"
link_file "$REPO_DIR/bin/close-window"        "$HOME_DIR/.local/bin/close-window"
link_file "$REPO_DIR/bin/zenity-askpass"      "$HOME_DIR/.local/bin/zenity-askpass"
link_file "$REPO_DIR/bin/fix-lan-mouse"        "$HOME_DIR/.local/bin/fix-lan-mouse"
link_file "$REPO_DIR/bin/xdg-user-dir"        "$HOME_DIR/.local/bin/xdg-user-dir"
link_file "$REPO_DIR/bin/yogabook-charger-negotiate" "$HOME_DIR/.local/bin/yogabook-charger-negotiate"
if [ -f "$REPO_DIR/bin/wvkbd" ]; then
    link_file "$REPO_DIR/bin/wvkbd"          "$HOME_DIR/.local/bin/wvkbd"
fi

# 2. Window Manager & User Directories
link_file "$REPO_DIR/config/mango/config.conf" "$HOME_DIR/.config/mango/config.conf"
link_file "$REPO_DIR/config/mango/outputs.conf" "$HOME_DIR/.config/mango/outputs.conf"
link_file "$REPO_DIR/config/mango/cursor.conf"  "$HOME_DIR/.config/mango/cursor.conf"
link_file "$REPO_DIR/config/mango/binds.conf"   "$HOME_DIR/.config/mango/binds.conf"
link_file "$REPO_DIR/config/user-dirs.dirs"    "$HOME_DIR/.config/user-dirs.dirs"
link_file "$REPO_DIR/config/gtk-3.0/settings.ini" "$HOME_DIR/.config/gtk-3.0/settings.ini"
link_file "$REPO_DIR/config/gtk-4.0/settings.ini" "$HOME_DIR/.config/gtk-4.0/settings.ini"
link_file "$REPO_DIR/config/yogabook/display.json" "$HOME_DIR/.config/yogabook/display.json"
link_file "$REPO_DIR/config/applications/yogabook-settings.desktop" "$HOME_DIR/.local/share/applications/yogabook-settings.desktop"
link_file "$REPO_DIR/config/environment.d/10-performance.conf" "$HOME_DIR/.config/environment.d/10-performance.conf"
link_file "$REPO_DIR/config/xdg-desktop-portal/mango-portals.conf" "$HOME_DIR/.config/xdg-desktop-portal/mango-portals.conf"
link_file "$REPO_DIR/config/chromium-flags.conf" "$HOME_DIR/.config/chromium-flags.conf"

# 3. GTK Touch Stack (Waybar, SwayNC, Wofi)
link_file "$REPO_DIR/config/waybar/config.jsonc" "$HOME_DIR/.config/waybar/config.jsonc"
link_file "$REPO_DIR/config/waybar/config.jsonc" "$HOME_DIR/.config/waybar/config"
link_file "$REPO_DIR/config/waybar/style.css"    "$HOME_DIR/.config/waybar/style.css"
link_file "$REPO_DIR/config/swaync/config.json"  "$HOME_DIR/.config/swaync/config.json"
link_file "$REPO_DIR/config/swaync/style.css"    "$HOME_DIR/.config/swaync/style.css"
link_file "$REPO_DIR/config/wofi/config"         "$HOME_DIR/.config/wofi/config"
link_file "$REPO_DIR/config/wofi/style.css"      "$HOME_DIR/.config/wofi/style.css"
link_file "$REPO_DIR/config/nwg-drawer/drawer.css" "$HOME_DIR/.config/nwg-drawer/drawer.css"

# 4. Systemd User Services
link_file "$REPO_DIR/config/systemd/user/rot8.service"   "$HOME_DIR/.config/systemd/user/rot8.service"
link_file "$REPO_DIR/config/systemd/user/wvkbd.service"  "$HOME_DIR/.config/systemd/user/wvkbd.service"
link_file "$REPO_DIR/config/systemd/user/waybar.service" "$HOME_DIR/.config/systemd/user/waybar.service"
link_file "$REPO_DIR/config/systemd/user/swaync.service" "$HOME_DIR/.config/systemd/user/swaync.service"
link_file "$REPO_DIR/config/systemd/user/yogabook-launcher.service" "$HOME_DIR/.config/systemd/user/yogabook-launcher.service"
link_file "$REPO_DIR/config/systemd/user/yogabook-control-center.service" "$HOME_DIR/.config/systemd/user/yogabook-control-center.service"
link_file "$REPO_DIR/config/systemd/user/polkit-gnome.service" "$HOME_DIR/.config/systemd/user/polkit-gnome.service"
link_file "$REPO_DIR/config/systemd/user/easyeffects.service"  "$HOME_DIR/.config/systemd/user/easyeffects.service"

# 5. EasyEffects & PipeWire Audio Tuning
link_file "$REPO_DIR/config/easyeffects/output" "$HOME_DIR/.local/share/easyeffects/output"
link_file "$REPO_DIR/config/pipewire/pipewire.conf.d/10-rates-quantum.conf" "$HOME_DIR/.config/pipewire/pipewire.conf.d/10-rates-quantum.conf"
link_file "$REPO_DIR/config/wireplumber/wireplumber.conf.d/50-yogabook-alsa.conf" "$HOME_DIR/.config/wireplumber/wireplumber.conf.d/50-yogabook-alsa.conf"

# 6. Setup Report
link_file "$REPO_DIR/YOGABOOK_SETUP_REPORT.md" "$HOME_DIR/YOGABOOK_SETUP_REPORT.md"

echo "Reloading systemd user daemon..."
systemctl --user daemon-reload || true

echo ""
echo "=== User configuration symlinks established successfully! ==="

if [[ "${1:-}" == "--system" ]]; then
    echo ""
    echo "=== Synchronizing System Configurations (/etc) ==="
    sudo install -Dm644 "$REPO_DIR/system/etc/sysctl.d/99-zram-performance.conf" /etc/sysctl.d/99-zram-performance.conf
    sudo install -Dm644 "$REPO_DIR/system/etc/systemd/logind.conf.d/yogabook.conf" /etc/systemd/logind.conf.d/yogabook.conf
    sudo install -Dm644 "$REPO_DIR/system/etc/systemd/journald.conf.d/00-size-limit.conf" /etc/systemd/journald.conf.d/00-size-limit.conf
    if [ -f "$REPO_DIR/system/etc/greetd/config.toml" ]; then
        sudo install -Dm644 "$REPO_DIR/system/etc/greetd/config.toml" /etc/greetd/config.toml
    fi
    if [ -f "$REPO_DIR/system/etc/bluetooth/main.conf" ]; then
        sudo install -Dm644 "$REPO_DIR/system/etc/bluetooth/main.conf" /etc/bluetooth/main.conf
        sudo systemctl restart bluetooth.service || true
    fi
    if [ -f /etc/systemd/system/bluetooth-default-off.service ]; then
        echo "Disabling legacy bluetooth-default-off.service in favor of BlueZ AutoEnable=false..."
        sudo systemctl disable --now bluetooth-default-off.service || true
        sudo rm -f /etc/systemd/system/bluetooth-default-off.service
    fi
    if [ -f "$REPO_DIR/system/etc/udev/rules.d/60-mmc-readahead.rules" ]; then
        sudo install -Dm644 "$REPO_DIR/system/etc/udev/rules.d/60-mmc-readahead.rules" /etc/udev/rules.d/60-mmc-readahead.rules
    fi
    if [ -f "$REPO_DIR/system/etc/udev/rules.d/62-yogabook-keyboard.rules" ]; then
        sudo install -Dm644 "$REPO_DIR/system/etc/udev/rules.d/62-yogabook-keyboard.rules" /etc/udev/rules.d/62-yogabook-keyboard.rules
    fi
    if [ -f "$REPO_DIR/system/etc/udev/rules.d/65-yogabook-charging.rules" ]; then
        sudo install -Dm644 "$REPO_DIR/system/etc/udev/rules.d/65-yogabook-charging.rules" /etc/udev/rules.d/65-yogabook-charging.rules
    fi
    if [ -f "$REPO_DIR/bin/yogabook-charger-negotiate" ]; then
        sudo install -Dm755 "$REPO_DIR/bin/yogabook-charger-negotiate" /usr/local/bin/yogabook-charger-negotiate
    fi
    if [ -f "$REPO_DIR/system/etc/systemd/system/yogabook-charge-negotiate.service" ]; then
        sudo install -Dm644 "$REPO_DIR/system/etc/systemd/system/yogabook-charge-negotiate.service" /etc/systemd/system/yogabook-charge-negotiate.service
    fi
    if [ -f "$REPO_DIR/system/etc/modprobe.d/brcmfmac.conf" ]; then
        sudo install -Dm644 "$REPO_DIR/system/etc/modprobe.d/brcmfmac.conf" /etc/modprobe.d/brcmfmac.conf
    fi
    if [ -f "$REPO_DIR/system/etc/iwd/main.conf" ]; then
        sudo install -Dm644 "$REPO_DIR/system/etc/iwd/main.conf" /etc/iwd/main.conf
    fi
    if [ -f "$REPO_DIR/system/etc/conf.d/wireless-regdom" ]; then
        sudo install -Dm644 "$REPO_DIR/system/etc/conf.d/wireless-regdom" /etc/conf.d/wireless-regdom
    fi
    sudo udevadm control --reload-rules && sudo udevadm trigger /dev/input/event* || true
    if [ -f "$REPO_DIR/system/etc/polkit-1/rules.d/49-yogabook-keyboard.rules" ]; then
        sudo install -Dm644 "$REPO_DIR/system/etc/polkit-1/rules.d/49-yogabook-keyboard.rules" /etc/polkit-1/rules.d/49-yogabook-keyboard.rules
    fi

    # Suppress kernel console spam over tuigreet in systemd-boot entries
    for boot_entry in /boot/loader/entries/yogabook.conf /boot/loader/entries/*linux.conf; do
        if [ -f "$boot_entry" ]; then
            if ! grep -q "quiet" "$boot_entry"; then
                echo "Adding quiet loglevel=3 to $boot_entry..."
                sudo sed -i 's/\(^options .*\)/\1 quiet loglevel=3/' "$boot_entry"
            fi
        fi
    done

    sudo sysctl --system >/dev/null || true
    sudo systemctl daemon-reload || true
    echo "[OK] System configuration files updated."
fi
