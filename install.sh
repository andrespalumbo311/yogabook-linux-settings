#!/usr/bin/env bash
# ==============================================================================
# Lenovo Yoga Book (YB1-X91F) Configuration Linker & Installer
# Manages symlinks between ~/yogabook-config and live system paths.
# ==============================================================================

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOME_DIR="${HOME:-/home/andres}"

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
link_file "$REPO_DIR/bin/ws-status"           "$HOME_DIR/.local/bin/ws-status"
link_file "$REPO_DIR/bin/close-window"        "$HOME_DIR/.local/bin/close-window"
link_file "$REPO_DIR/bin/zenity-askpass"      "$HOME_DIR/.local/bin/zenity-askpass"
link_file "$REPO_DIR/bin/xdg-user-dir"        "$HOME_DIR/.local/bin/xdg-user-dir"
if [ -f "$REPO_DIR/bin/wvkbd" ]; then
    link_file "$REPO_DIR/bin/wvkbd"          "$HOME_DIR/.local/bin/wvkbd"
fi

# 2. Window Manager & User Directories
link_file "$REPO_DIR/config/mango/config.conf" "$HOME_DIR/.config/mango/config.conf"
link_file "$REPO_DIR/config/mango/outputs.conf" "$HOME_DIR/.config/mango/outputs.conf"
link_file "$REPO_DIR/config/mango/cursor.conf"  "$HOME_DIR/.config/mango/cursor.conf"
link_file "$REPO_DIR/config/mango/binds.conf"   "$HOME_DIR/.config/mango/binds.conf"
link_file "$REPO_DIR/config/user-dirs.dirs"    "$HOME_DIR/.config/user-dirs.dirs"

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

# 5. Setup Report
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
    sudo sysctl --system >/dev/null || true
    sudo systemctl daemon-reload || true
    echo "[OK] System configuration files updated."
fi
