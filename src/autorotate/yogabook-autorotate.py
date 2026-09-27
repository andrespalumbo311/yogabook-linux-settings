#!/usr/bin/env python3
"""
Yoga Book Smart Auto-Rotation Daemon (Production Calibrated & Singularity-Guarded)

Hardware Specifics (Lenovo Yoga Book YB1-X91F):
- Screen Accelerometer: iio:device2 (ACCEL_LOCATION=base in default udev, physically in display)
- Base Accelerometer:   iio:device4 (unlabelled in udev, physically in keyboard base)
- Hinge Axis:           Along screen Y axis (base -X axis)
- Display Output:       DSI-1 on Wayland (default transform 270)

Key Architectural Pillars:
1. True 2D Hinge Cross-Section Projection:
   - Projects gravity vectors onto the plane perpendicular to the hinge (X-Z plane).
   - Immune to roll/tilt angles up to 70°+.
   - Continuously computes the real physical hinge opening from 0° to 360°.

2. Hinge Singularity Guard:
   - When the device is rolled sideways (gravity parallel to hinge axis Y),
     gravity cannot resolve hinge opening. The daemon freezes the current mode (laptop/tablet)
     so lifting or tilting the laptop never triggers unwanted mode switches.

3. Solid Hysteresis & Mode Debounce:
   - Laptop mode: opening <= 135.0° with keyboard base facing up.
   - Tablet / Flat mode: opening >= 142.0° (calibrated to user active posture).
   - Halo Keyboard: automatic deactivation & LED shutdown when opening >= 190.0° (extended beyond flat).
   - Debounce filter: requires 2 consecutive consistent cycles (~0.8s) to switch modes.

4. Table Flat-Lock & Decisive Orientation Engine:
   - Flat-Lock: when screen normal is tilted < 53° from horizontal (|z| > 0.60),
     orientation freezes completely so placing the device down on a table never flips orientation.
   - Decisive Switching: requires 1.35x dominance on the target axis and min 0.50g force.
   - Debounce: 2 consecutive cycles (~0.8s) before applying rotation change.
"""

import sys
import os
import glob
import time
import math
import signal
import subprocess
import logging

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s"
)

DISPLAY = "DSI-1"
DEFAULT_TRANSFORM = "270"
STATE_FILE = f"/run/user/{os.getuid()}/yogabook-rotation.state"
KBD_BACKLIGHT_SYSFS = "/sys/class/leds/ybwmi::kbd_backlight/brightness"

# Hinge angle thresholds (in degrees)
# Auto-rotation activates beyond active laptop opening angle (~142.5°)
LAPTOP_MAX_OPENING = 142.0    # Opening >= 142.0° triggers tablet mode (auto-rotation)
LAPTOP_ENTER_OPENING = 135.0  # Opening <= 135.0° returns to laptop mode (locked landscape)

# Halo keyboard deactivation thresholds (extended beyond flat)
KEYBOARD_DISABLE_ANGLE = 190.0  # Opening >= 190.0° disables keyboard & turns off backlight
KEYBOARD_ENABLE_ANGLE = 180.0   # Opening <= 180.0° re-enables keyboard & restores backlight

screen_dev = None
base_dev = None
current_mode = "laptop"

class HaloKeyboardManager:
    """
    Manages the Halo Keyboard and touchpad states on the Yoga Book.
    Suppresses input events and backlight when the device is extended beyond 190°.
    Resilient to kernel/udev re-enumeration when reopening from 360° tablet mode.
    """
    def __init__(self):
        self.is_disabled = False
        self.grabbed_devices = []
        self.saved_brightness = 255

    def get_backlight(self):
        try:
            if os.path.exists(KBD_BACKLIGHT_SYSFS):
                with open(KBD_BACKLIGHT_SYSFS, "r") as f:
                    val = int(f.read().strip())
                    return val if val > 0 else 255
        except Exception:
            pass
        return 255

    def set_backlight(self, val):
        try:
            if os.path.exists(KBD_BACKLIGHT_SYSFS):
                with open(KBD_BACKLIGHT_SYSFS, "w") as f:
                    f.write(str(val))
        except Exception as e:
            logging.debug(f"Failed to set kbd backlight: {e}")

    def find_keyboard_devices(self):
        kbd_nodes = []
        for dev_path in sorted(glob.glob("/sys/class/input/event*")):
            name_file = os.path.join(dev_path, "device", "name")
            if os.path.exists(name_file):
                try:
                    with open(name_file, "r") as f:
                        name = f.read().strip()
                    if name in ("Goodix Capacitive TouchScreen", "virtual-keyboard", "virtual-touchpad"):
                        ev_node = os.path.join("/dev", "input", os.path.basename(dev_path))
                        kbd_nodes.append((ev_node, name))
                except Exception:
                    pass
        return kbd_nodes

    def maintain_disabled(self):
        """
        Continuously ensures the keyboard remains suppressed even if the kernel
        or udev re-enumerates the Goodix digitizer and restarts touch-keyboard-handler
        upon reopening from 360° tablet mode.
        """
        current_devs = dict(self.find_keyboard_devices())  # {path: name}

        # 1. Clean up dead/stale device file descriptors
        active_grabbed = []
        for dev in self.grabbed_devices:
            try:
                if dev.path in current_devs:
                    active_grabbed.append(dev)
                else:
                    try:
                        dev.close()
                    except Exception:
                        pass
            except Exception:
                pass
        self.grabbed_devices = active_grabbed
        grabbed_paths = {d.path for d in self.grabbed_devices}

        # 2. Check if touch-keyboard-handler service is running
        handler_running = False
        try:
            res = subprocess.run(
                ["systemctl", "is-active", "--quiet", "touch-keyboard-handler.service"],
                check=False
            )
            handler_running = (res.returncode == 0)
        except Exception:
            pass

        # 3. Check for any ungrabbed keyboard nodes
        ungrabbed = [p for p in current_devs if p not in grabbed_paths]

        if handler_running or ungrabbed:
            if handler_running:
                logging.info("Stopping touch-keyboard-handler in disabled posture...")
                try:
                    subprocess.run(
                        ["systemctl", "stop", "touch-keyboard-handler.service", "--no-ask-password"],
                        check=False,
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL
                    )
                except Exception:
                    pass

            # Always ensure backlight remains off in disabled posture
            self.set_backlight(0)

            # Grab all ungrabbed nodes
            try:
                import evdev
                for ev_node, name in self.find_keyboard_devices():
                    if ev_node not in grabbed_paths:
                        try:
                            dev = evdev.InputDevice(ev_node)
                            dev.grab()
                            self.grabbed_devices.append(dev)
                            grabbed_paths.add(ev_node)
                            logging.info(f"Grabbed {name} on {ev_node} (suppressing input)")
                        except Exception as e:
                            logging.debug(f"Could not grab {ev_node} ({name}): {e}")
            except ImportError:
                logging.warning("evdev module not available for device grabbing")

    def disable(self):
        if not self.is_disabled:
            logging.info("Disabling Halo Keyboard (opening >= 190.0°)...")
            # 1. Save and turn off backlight
            cur_bl = self.get_backlight()
            if cur_bl > 0:
                self.saved_brightness = cur_bl
            self.set_backlight(0)
            self.is_disabled = True

        # Always actively maintain disabled state (reconcile against udev re-plugs)
        self.maintain_disabled()

    def enable(self):
        if not self.is_disabled:
            return
        logging.info("Re-enabling Halo Keyboard (opening <= 180.0°)...")
        # 1. Release all grabbed devices
        for dev in self.grabbed_devices:
            try:
                dev.ungrab()
                dev.close()
            except Exception as e:
                logging.debug(f"Error ungrabbing device: {e}")
        self.grabbed_devices = []

        # 2. Restart system service if it was stopped
        try:
            subprocess.run(
                ["systemctl", "start", "touch-keyboard-handler.service", "--no-ask-password"],
                check=False,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )
        except Exception:
            pass

        # 3. Restore backlight
        restore_val = self.saved_brightness if self.saved_brightness > 0 else 255
        self.set_backlight(restore_val)
        self.is_disabled = False

    def cleanup(self):
        self.enable()

kbd_mgr = HaloKeyboardManager()

def write_state(mode, transform="270"):
    try:
        with open(STATE_FILE, "w") as f:
            f.write(f"mode={mode}\ntransform={transform}\n")
    except Exception:
        pass

def cleanup(*args):
    logging.info("Shutting down yogabook-autorotate...")
    try:
        kbd_mgr.cleanup()
    except Exception:
        pass
    try:
        if os.path.exists(STATE_FILE):
            os.remove(STATE_FILE)
    except Exception:
        pass
    sys.exit(0)

signal.signal(signal.SIGTERM, cleanup)
signal.signal(signal.SIGINT, cleanup)

def find_iio_devices():
    s_dev = None
    b_dev = None
    for dev in sorted(glob.glob("/sys/bus/iio/devices/iio:device*")):
        name_file = os.path.join(dev, "name")
        if not os.path.exists(name_file):
            continue
        try:
            with open(name_file, "r") as f:
                name = f.read().strip()
        except Exception:
            continue
        if name != "accel_3d":
            continue
        try:
            res = subprocess.run(
                ["udevadm", "info", "-q", "property", "-p", os.path.realpath(dev)],
                capture_output=True,
                text=True,
                check=False
            )
            props = dict(line.split("=", 1) for line in res.stdout.splitlines() if "=" in line)
            loc = props.get("ACCEL_LOCATION", "display")
            # Empirical verification: ACCEL_LOCATION=base is physically DISPLAY,
            # and unlabelled is KEYBOARD.
            if loc == "base" and not s_dev:
                s_dev = dev
            elif loc != "base" and not b_dev:
                b_dev = dev
        except Exception as e:
            logging.error(f"Error querying udev for {dev}: {e}")

    return s_dev, b_dev

def apply_transform(transform):
    try:
        env = os.environ.copy()
        if "WAYLAND_DISPLAY" not in env:
            env["WAYLAND_DISPLAY"] = "wayland-0"
        subprocess.run(
            ["wlr-randr", "--output", DISPLAY, "--transform", transform],
            env=env,
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        logging.info(f"Screen transform set to: {transform}")
    except Exception as e:
        logging.error(f"Failed to set transform to {transform}: {e}")

def read_accel_raw(dev):
    try:
        with open(f"{dev}/in_accel_x_raw", "r") as fx, \
             open(f"{dev}/in_accel_y_raw", "r") as fy, \
             open(f"{dev}/in_accel_z_raw", "r") as fz:
            return int(fx.read()), int(fy.read()), int(fz.read())
    except Exception:
        return None

def get_posture(s, b, cur_mode):
    """
    Determines whether device is in LAPTOP mode (locked landscape) or TABLET/FLAT mode (auto-rotate).
    Uses 2D cross-section projection perpendicular to the hinge axis.
    """
    # Align base coordinate system to screen frame:
    # Screen: (s_x, s_y, s_z)
    # Base raw: (b_x, b_y, b_z) -> aligned: (b_y, -b_x, b_z)
    b_al = (b[1], -b[0], b[2])

    ns = math.hypot(*s)
    nb = math.hypot(*b)
    if ns < 100000 or nb < 100000:
        return cur_mode or "laptop", 0.0

    # Cross-section vectors perpendicular to the hinge (X-Z plane):
    vs = (s[0], s[2])
    vb = (b_al[0], b_al[2])

    perp_s = math.hypot(*vs) / ns
    perp_b = math.hypot(*vb) / nb

    # Singularity Guard: when gravity is almost purely along the hinge axis (Y),
    # perpendicular components vanish. Accelerometers cannot measure hinge opening.
    # Preserve current mode to prevent accidental flips when tilting/lifting the laptop!
    if perp_s < 0.35 and perp_b < 0.35:
        return (cur_mode if cur_mode is not None else "tablet"), -1.0

    # True 2D opening angle in cross-section plane
    cross_2d = vs[0] * vb[1] - vs[1] * vb[0]
    dot_2d = vs[0] * vb[0] + vs[1] * vb[1]
    angle_2d = math.degrees(math.atan2(cross_2d, dot_2d))

    # Base is resting horizontal/flat (keyboard facing up towards user/ceiling)
    base_is_flat = b[2] < -0.35 * nb

    opening = 180.0 - angle_2d
    if opening < 0.0:
        opening += 360.0
    elif opening > 360.0:
        opening -= 360.0

    # Branch cut guard at 360° (folded back-to-back):
    # When folded around 360°, sensor noise near angle_2d = ±180° can cause
    # opening to fluctuate around 0° / 360°. If the base is inverted (keyboard facing down)
    # or the device is already in tablet mode, an opening near 0° is physically 360° (tablet).
    if opening < 45.0 and (not base_is_flat or cur_mode == "tablet"):
        opening = 360.0 - opening

    if cur_mode == "laptop":
        # Exit laptop mode if opened wide (>= LAPTOP_MAX_OPENING) or base clearly flipped
        if opening >= LAPTOP_MAX_OPENING or not base_is_flat:
            return "tablet", opening
        return "laptop", opening
    elif cur_mode == "tablet":
        # Enter laptop mode if base is resting flat and opening is in laptop range (<= LAPTOP_ENTER_OPENING)
        if base_is_flat and opening <= LAPTOP_ENTER_OPENING:
            return "laptop", opening
        return "tablet", opening
    else:
        # Initial posture check at startup
        if base_is_flat and opening <= ((LAPTOP_MAX_OPENING + LAPTOP_ENTER_OPENING) / 2.0):
            return "laptop", opening
        return "tablet", opening

class OrientationTracker:
    def __init__(self, initial_tr=DEFAULT_TRANSFORM):
        self.current_transform = initial_tr
        self.pending_transform = None
        self.pending_count = 0
        self.DEBOUNCE_THRESHOLD = 2  # 2 consecutive cycles (~0.8s)

    def set_forced(self, tr):
        self.current_transform = tr
        self.pending_transform = None
        self.pending_count = 0

    def update(self, s):
        ns = math.sqrt(sum(x**2 for x in s))
        if ns < 100000:
            return self.current_transform

        x = s[0] / ns
        y = s[1] / ns
        z = abs(s[2]) / ns

        # 1. Flat Lock: if tilted < 53° from horizontal (|z| > 0.60), freeze orientation!
        # This prevents accidental rotation when setting the device down on a table or resting on a lap.
        if z > 0.60:
            self.pending_transform = None
            self.pending_count = 0
            return self.current_transform

        cur = self.current_transform
        is_portrait = cur in ("180", "normal")
        is_landscape = cur in ("270", "90")

        RATIO = 1.35
        MIN_FORCE = 0.50

        desired = cur

        if is_portrait:
            # To switch to landscape, X must decisively dominate Y
            if abs(x) > RATIO * abs(y) and abs(x) > MIN_FORCE:
                desired = "270" if x < 0 else "90"
            elif y < -MIN_FORCE and cur == "180":
                desired = "normal"
            elif y > MIN_FORCE and cur == "normal":
                desired = "180"
        elif is_landscape:
            # To switch to portrait, Y must decisively dominate X
            if abs(y) > RATIO * abs(x) and abs(y) > MIN_FORCE:
                desired = "180" if y > 0 else "normal"
            elif x > MIN_FORCE and cur == "270":
                desired = "90"
            elif x < -MIN_FORCE and cur == "90":
                desired = "270"
        else:
            if abs(y) > abs(x):
                desired = "180" if y > 0 else "normal"
            else:
                desired = "270" if x < 0 else "90"

        if desired == self.current_transform:
            self.pending_transform = None
            self.pending_count = 0
            return self.current_transform

        # Debounce filter
        if desired == self.pending_transform:
            self.pending_count += 1
            if self.pending_count >= self.DEBOUNCE_THRESHOLD:
                self.current_transform = desired
                self.pending_transform = None
                self.pending_count = 0
                return desired
        else:
            self.pending_transform = desired
            self.pending_count = 1

        return self.current_transform

def main():
    global screen_dev, base_dev, current_mode

    logging.info("Starting Yoga Book smart auto-rotation daemon (calibrated with 2D cross-section & singularity guard)...")

    # Discover devices
    for _ in range(10):
        screen_dev, base_dev = find_iio_devices()
        if screen_dev and base_dev:
            break
        time.sleep(0.5)

    if not screen_dev or not base_dev:
        logging.error("Failed to find screen and base accelerometers!")
        sys.exit(1)

    logging.info(f"Discovered sensors -> Screen: {screen_dev}, Base: {base_dev}")

    tracker = OrientationTracker(DEFAULT_TRANSFORM)

    # Initial posture check
    initial_mode, initial_opening = "laptop", 0.0
    for _ in range(5):
        s = read_accel_raw(screen_dev)
        b = read_accel_raw(base_dev)
        if s and b:
            initial_mode, initial_opening = get_posture(s, b, None)
            break
        time.sleep(0.1)

    current_mode = initial_mode
    if current_mode == "tablet":
        logging.info(f"Initial posture: TABLET / FLAT (~{initial_opening:.1f}°). Enabling auto-rotation.")
        if s:
            target_tr = tracker.update(s)
            apply_transform(target_tr)
            write_state("tablet", target_tr)
        else:
            write_state("tablet", DEFAULT_TRANSFORM)
    else:
        logging.info(f"Initial posture: LAPTOP (~{initial_opening:.1f}°). Locking orientation to {DEFAULT_TRANSFORM}.")
        tracker.set_forced(DEFAULT_TRANSFORM)
        apply_transform(DEFAULT_TRANSFORM)
        write_state("laptop", DEFAULT_TRANSFORM)

    if initial_opening >= KEYBOARD_DISABLE_ANGLE:
        kbd_mgr.disable()

    mode_debounce_target = None
    mode_debounce_count = 0
    MODE_DEBOUNCE_THRESHOLD = 2  # 2 cycles (0.8s) to confirm mode transitions
    last_hdmi_status = None

    while True:
        try:
            time.sleep(0.4)

            # HDMI Hotplug monitor
            try:
                if os.path.exists("/sys/class/drm/card0-HDMI-A-1/status"):
                    with open("/sys/class/drm/card0-HDMI-A-1/status", "r") as f_hdmi:
                        current_hdmi = f_hdmi.read().strip()
                else:
                    current_hdmi = "disconnected"

                if last_hdmi_status is not None and current_hdmi != last_hdmi_status:
                    logging.info(f"HDMI hotplug transition detected: {last_hdmi_status} -> {current_hdmi}")
                    subprocess.Popen(["yogabook-display-mgr", "apply"])
                last_hdmi_status = current_hdmi
            except Exception:
                pass

            s = read_accel_raw(screen_dev)
            b = read_accel_raw(base_dev)
            if not s or not b:
                screen_dev, base_dev = find_iio_devices()
                continue

            detected_mode, opening = get_posture(s, b, current_mode)

            # Manage Halo Keyboard state based on opening angle (when not in singularity)
            if opening >= 0:
                if opening >= KEYBOARD_DISABLE_ANGLE:
                    kbd_mgr.disable()
                elif opening <= KEYBOARD_ENABLE_ANGLE:
                    kbd_mgr.enable()

            # Debounce mode transitions
            if detected_mode != current_mode:
                if detected_mode == mode_debounce_target:
                    mode_debounce_count += 1
                    if mode_debounce_count >= MODE_DEBOUNCE_THRESHOLD:
                        # Transition confirmed!
                        if detected_mode == "tablet":
                            logging.info(f"Transition: LAPTOP -> TABLET/FLAT (~{opening:.1f}°). Activating auto-rotation.")
                            current_mode = "tablet"
                            target_tr = tracker.update(s)
                            apply_transform(target_tr)
                            write_state("tablet", target_tr)
                        else:
                            logging.info(f"Transition: TABLET/FLAT -> LAPTOP (~{opening:.1f}°). Locking orientation to {DEFAULT_TRANSFORM}.")
                            current_mode = "laptop"
                            tracker.set_forced(DEFAULT_TRANSFORM)
                            apply_transform(DEFAULT_TRANSFORM)
                            write_state("laptop", DEFAULT_TRANSFORM)
                            subprocess.run(["pkill", "-SIGUSR1", "-x", "wvkbd"], check=False)

                        mode_debounce_target = None
                        mode_debounce_count = 0
                else:
                    mode_debounce_target = detected_mode
                    mode_debounce_count = 1
            else:
                mode_debounce_target = None
                mode_debounce_count = 0

            # If in tablet mode, perform continuous auto-rotation with hysteresis & flat-lock
            if current_mode == "tablet":
                prev_tr = tracker.current_transform
                target_tr = tracker.update(s)
                if target_tr != prev_tr:
                    apply_transform(target_tr)
                    write_state("tablet", target_tr)

        except Exception as e:
            logging.error(f"Error in main loop: {e}")
            time.sleep(1.0)

if __name__ == "__main__":
    main()
