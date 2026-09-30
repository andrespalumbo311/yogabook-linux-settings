#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <linux/input.h>
#include <libudev.h>

#define DISPLAY "DSI-1"
#define DEFAULT_TRANSFORM "270"
#define KBD_BACKLIGHT_SYSFS "/sys/class/leds/ybwmi::kbd_backlight/brightness"
#define HDMI_STATUS_SYSFS "/sys/class/drm/card0-HDMI-A-1/status"

#define LAPTOP_MAX_OPENING   160.0
#define LAPTOP_ENTER_OPENING 152.0
#define KEYBOARD_DISABLE_ANGLE 190.0
#define KEYBOARD_ENABLE_ANGLE  180.0

#define DEBOUNCE_THRESHOLD 2
#define MODE_DEBOUNCE_THRESHOLD 2

typedef enum {
    MODE_LAPTOP = 0,
    MODE_TABLET = 1
} DeviceMode;

typedef enum {
    TR_NORMAL = 0,
    TR_90     = 1,
    TR_180    = 2,
    TR_270    = 3
} TransformId;

static const char *TRANSFORM_NAMES[] = {
    "normal",
    "90",
    "180",
    "270"
};

static const char* transform_to_string(TransformId tr) {
    if (tr >= 0 && tr <= 3) return TRANSFORM_NAMES[tr];
    return "270";
}

// Halo Keyboard Grab State
#define MAX_GRABBED_DEVS 8
struct GrabbedDev {
    int fd;
    char path[64];
};

static struct {
    bool is_disabled;
    struct GrabbedDev grabbed[MAX_GRABBED_DEVS];
    int grabbed_count;
    int saved_brightness;
} g_kbd_mgr = {
    .is_disabled = false,
    .grabbed_count = 0,
    .saved_brightness = 255
};

static char g_state_file[128] = {0};
static volatile sig_atomic_t g_running = 1;

static int get_kbd_backlight(void) {
    FILE *f = fopen(KBD_BACKLIGHT_SYSFS, "r");
    if (!f) return 255;
    int val = 255;
    if (fscanf(f, "%d", &val) != 1 || val <= 0) {
        val = 255;
    }
    fclose(f);
    return val;
}

static void set_kbd_backlight(int val) {
    FILE *f = fopen(KBD_BACKLIGHT_SYSFS, "w");
    if (!f) return;
    fprintf(f, "%d\n", val);
    fclose(f);
}

static void kbd_enable(void) {
    if (!g_kbd_mgr.is_disabled) return;
    printf("[INFO] Re-enabling Halo Keyboard (opening <= 180.0°)...\n");

    for (int i = 0; i < g_kbd_mgr.grabbed_count; i++) {
        if (g_kbd_mgr.grabbed[i].fd >= 0) {
            ioctl(g_kbd_mgr.grabbed[i].fd, EVIOCGRAB, 0);
            close(g_kbd_mgr.grabbed[i].fd);
            g_kbd_mgr.grabbed[i].fd = -1;
        }
    }
    g_kbd_mgr.grabbed_count = 0;

    int r = system("systemctl start touch-keyboard-handler.service --no-ask-password >/dev/null 2>&1");
    (void)r;

    int restore_val = g_kbd_mgr.saved_brightness > 0 ? g_kbd_mgr.saved_brightness : 255;
    set_kbd_backlight(restore_val);
    g_kbd_mgr.is_disabled = false;
}

static void kbd_maintain_disabled(void) {
    char current_nodes[MAX_GRABBED_DEVS][64];
    int current_nodes_count = 0;

    for (int i = 0; i < 32; i++) {
        char name_path[128];
        snprintf(name_path, sizeof(name_path), "/sys/class/input/event%d/device/name", i);
        FILE *fn = fopen(name_path, "r");
        if (!fn) continue;
        char name[128] = {0};
        if (fgets(name, sizeof(name), fn)) {
            name[strcspn(name, "\r\n")] = '\0';
            if (strcmp(name, "Goodix Capacitive TouchScreen") == 0 ||
                strcmp(name, "virtual-keyboard") == 0 ||
                strcmp(name, "virtual-touchpad") == 0) {
                if (current_nodes_count < MAX_GRABBED_DEVS) {
                    snprintf(current_nodes[current_nodes_count], sizeof(current_nodes[0]), "/dev/input/event%d", i);
                    current_nodes_count++;
                }
            }
        }
        fclose(fn);
    }

    // Clean up dead nodes
    for (int i = 0; i < g_kbd_mgr.grabbed_count; i++) {
        bool still_present = false;
        for (int j = 0; j < current_nodes_count; j++) {
            if (strcmp(g_kbd_mgr.grabbed[i].path, current_nodes[j]) == 0) {
                still_present = true;
                break;
            }
        }
        if (!still_present) {
            if (g_kbd_mgr.grabbed[i].fd >= 0) {
                close(g_kbd_mgr.grabbed[i].fd);
            }
            g_kbd_mgr.grabbed[i] = g_kbd_mgr.grabbed[g_kbd_mgr.grabbed_count - 1];
            g_kbd_mgr.grabbed_count--;
            i--;
        }
    }

    // Check if touch-keyboard-handler is running
    bool handler_running = (system("systemctl is-active --quiet touch-keyboard-handler.service") == 0);

    bool has_ungrabbed = false;
    for (int i = 0; i < current_nodes_count; i++) {
        bool is_grabbed = false;
        for (int j = 0; j < g_kbd_mgr.grabbed_count; j++) {
            if (strcmp(current_nodes[i], g_kbd_mgr.grabbed[j].path) == 0) {
                is_grabbed = true;
                break;
            }
        }
        if (!is_grabbed) {
            has_ungrabbed = true;
            break;
        }
    }

    if (handler_running || has_ungrabbed) {
        if (handler_running) {
            printf("[INFO] Stopping touch-keyboard-handler in disabled posture...\n");
            int r = system("systemctl stop touch-keyboard-handler.service --no-ask-password >/dev/null 2>&1");
            (void)r;
        }

        set_kbd_backlight(0);

        for (int i = 0; i < current_nodes_count; i++) {
            bool is_grabbed = false;
            for (int j = 0; j < g_kbd_mgr.grabbed_count; j++) {
                if (strcmp(current_nodes[i], g_kbd_mgr.grabbed[j].path) == 0) {
                    is_grabbed = true;
                    break;
                }
            }
            if (!is_grabbed && g_kbd_mgr.grabbed_count < MAX_GRABBED_DEVS) {
                int fd = open(current_nodes[i], O_RDONLY | O_NONBLOCK);
                if (fd >= 0) {
                    if (ioctl(fd, EVIOCGRAB, 1) == 0) {
                        g_kbd_mgr.grabbed[g_kbd_mgr.grabbed_count].fd = fd;
                        snprintf(g_kbd_mgr.grabbed[g_kbd_mgr.grabbed_count].path, sizeof(g_kbd_mgr.grabbed[0].path), "%.63s", current_nodes[i]);
                        g_kbd_mgr.grabbed_count++;
                        printf("[INFO] Grabbed %s (suppressing input)\n", current_nodes[i]);
                    } else {
                        close(fd);
                    }
                }
            }
        }
    }
}

static void kbd_disable(void) {
    if (!g_kbd_mgr.is_disabled) {
        printf("[INFO] Disabling Halo Keyboard (opening >= 190.0°)...\n");
        int bl = get_kbd_backlight();
        if (bl > 0) g_kbd_mgr.saved_brightness = bl;
        set_kbd_backlight(0);
        g_kbd_mgr.is_disabled = true;
    }
    kbd_maintain_disabled();
}

static void write_state(DeviceMode mode, TransformId tr) {
    if (!g_state_file[0]) return;
    FILE *f = fopen(g_state_file, "w");
    if (!f) return;
    fprintf(f, "mode=%s\ntransform=%s\n",
            mode == MODE_LAPTOP ? "laptop" : "tablet",
            transform_to_string(tr));
    fclose(f);
}

static void apply_transform(TransformId tr) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "wlr-randr --output %s --transform %s >/dev/null 2>&1",
             DISPLAY, transform_to_string(tr));
    int r = system(cmd);
    (void)r;
    printf("[INFO] Screen transform set to: %s\n", transform_to_string(tr));
}

static void sig_handler(int sig) {
    (void)sig;
    g_running = 0;
}

static int find_iio_devices(char *screen_dev, size_t s_sz, char *base_dev, size_t b_sz) {
    struct udev *udev = udev_new();
    if (!udev) return -1;

    screen_dev[0] = '\0';
    base_dev[0] = '\0';

    for (int i = 0; i < 16; i++) {
        char dev_path[128];
        snprintf(dev_path, sizeof(dev_path), "/sys/bus/iio/devices/iio:device%d", i);

        char name_path[160];
        snprintf(name_path, sizeof(name_path), "%s/name", dev_path);
        FILE *f = fopen(name_path, "r");
        if (!f) continue;
        char name[64] = {0};
        if (!fgets(name, sizeof(name), f)) {
            fclose(f);
            continue;
        }
        fclose(f);
        name[strcspn(name, "\r\n")] = '\0';

        if (strcmp(name, "accel_3d") != 0) continue;

        struct udev_device *dev = udev_device_new_from_syspath(udev, dev_path);
        if (!dev) continue;

        const char *loc = udev_device_get_property_value(dev, "ACCEL_LOCATION");
        if (loc && strcmp(loc, "base") == 0) {
            if (screen_dev[0] == '\0') {
                snprintf(screen_dev, s_sz, "%s", dev_path);
            }
        } else {
            if (base_dev[0] == '\0') {
                snprintf(base_dev, b_sz, "%s", dev_path);
            }
        }
        udev_device_unref(dev);
    }
    udev_unref(udev);

    return (screen_dev[0] && base_dev[0]) ? 0 : -1;
}

static bool read_accel_raw(const char *dev, int32_t val[3]) {
    char path[160];
    FILE *fx = NULL, *fy = NULL, *fz = NULL;

    snprintf(path, sizeof(path), "%s/in_accel_x_raw", dev);
    fx = fopen(path, "r");
    snprintf(path, sizeof(path), "%s/in_accel_y_raw", dev);
    fy = fopen(path, "r");
    snprintf(path, sizeof(path), "%s/in_accel_z_raw", dev);
    fz = fopen(path, "r");

    if (!fx || !fy || !fz) {
        if (fx) fclose(fx);
        if (fy) fclose(fy);
        if (fz) fclose(fz);
        return false;
    }

    int rx = fscanf(fx, "%d", &val[0]);
    int ry = fscanf(fy, "%d", &val[1]);
    int rz = fscanf(fz, "%d", &val[2]);

    fclose(fx);
    fclose(fy);
    fclose(fz);

    return (rx == 1 && ry == 1 && rz == 1);
}

static DeviceMode get_posture(const int32_t s[3], const int32_t b[3], DeviceMode cur_mode, bool initial, double *out_opening) {
    double b_al_x = (double)b[1];
    double b_al_y = -(double)b[0];
    double b_al_z = (double)b[2];
    (void)b_al_y;

    double ns = sqrt((double)s[0] * s[0] + (double)s[1] * s[1] + (double)s[2] * s[2]);
    double nb = sqrt((double)b[0] * b[0] + (double)b[1] * b[1] + (double)b[2] * b[2]);

    if (ns < 100000.0 || nb < 100000.0) {
        *out_opening = 0.0;
        return cur_mode;
    }

    double vs_x = (double)s[0];
    double vs_z = (double)s[2];
    double vb_x = b_al_x;
    double vb_z = b_al_z;

    double perp_s = hypot(vs_x, vs_z) / ns;
    double perp_b = hypot(vb_x, vb_z) / nb;

    if (perp_s < 0.20 || perp_b < 0.20) {
        *out_opening = -1.0;
        return cur_mode;
    }

    double cross_2d = vs_x * vb_z - vs_z * vb_x;
    double dot_2d = vs_x * vb_x + vs_z * vb_z;
    double angle_2d = atan2(cross_2d, dot_2d) * (180.0 / M_PI);

    bool base_is_flat = ((double)b[2] < -0.35 * nb) && (fabs((double)b[0]) < 0.35 * nb);

    double opening = 180.0 - angle_2d;
    while (opening < 0.0) opening += 360.0;
    while (opening > 360.0) opening -= 360.0;

    if (opening < 45.0 && (!base_is_flat || cur_mode == MODE_TABLET)) {
        opening = 360.0 - opening;
    }

    *out_opening = opening;

    if (initial) {
        if (base_is_flat && opening <= ((LAPTOP_MAX_OPENING + LAPTOP_ENTER_OPENING) / 2.0)) {
            return MODE_LAPTOP;
        }
        return MODE_TABLET;
    }

    if (cur_mode == MODE_LAPTOP) {
        if (opening >= LAPTOP_MAX_OPENING) {
            return MODE_TABLET;
        }
        return MODE_LAPTOP;
    } else {
        if (base_is_flat && opening <= LAPTOP_ENTER_OPENING) {
            return MODE_LAPTOP;
        }
        return MODE_TABLET;
    }
}

// Orientation tracker state
static struct {
    TransformId current;
    TransformId pending;
    int pending_count;
} g_tracker = {
    .current = TR_270,
    .pending = (TransformId)-1,
    .pending_count = 0
};

static void tracker_force(TransformId tr) {
    g_tracker.current = tr;
    g_tracker.pending = (TransformId)-1;
    g_tracker.pending_count = 0;
}

static TransformId tracker_update(const int32_t s[3]) {
    double ns = sqrt((double)s[0] * s[0] + (double)s[1] * s[1] + (double)s[2] * s[2]);
    if (ns < 100000.0) {
        return g_tracker.current;
    }

    double x = (double)s[0] / ns;
    double y = (double)s[1] / ns;
    double z = fabs((double)s[2]) / ns;

    if (z > 0.60) {
        g_tracker.pending = (TransformId)-1;
        g_tracker.pending_count = 0;
        return g_tracker.current;
    }

    TransformId cur = g_tracker.current;
    bool is_portrait = (cur == TR_180 || cur == TR_NORMAL);
    bool is_landscape = (cur == TR_270 || cur == TR_90);

    const double RATIO = 1.35;
    const double MIN_FORCE = 0.50;

    TransformId desired = cur;

    if (is_portrait) {
        if (fabs(x) > RATIO * fabs(y) && fabs(x) > MIN_FORCE) {
            desired = (x < 0) ? TR_270 : TR_90;
        } else if (y < -MIN_FORCE && cur == TR_180) {
            desired = TR_NORMAL;
        } else if (y > MIN_FORCE && cur == TR_NORMAL) {
            desired = TR_180;
        }
    } else if (is_landscape) {
        if (fabs(y) > RATIO * fabs(x) && fabs(y) > MIN_FORCE) {
            desired = (y > 0) ? TR_180 : TR_NORMAL;
        } else if (x > MIN_FORCE && cur == TR_270) {
            desired = TR_90;
        } else if (x < -MIN_FORCE && cur == TR_90) {
            desired = TR_270;
        }
    } else {
        if (fabs(y) > fabs(x)) {
            desired = (y > 0) ? TR_180 : TR_NORMAL;
        } else {
            desired = (x < 0) ? TR_270 : TR_90;
        }
    }

    if (desired == g_tracker.current) {
        g_tracker.pending = (TransformId)-1;
        g_tracker.pending_count = 0;
        return g_tracker.current;
    }

    if (desired == g_tracker.pending) {
        g_tracker.pending_count++;
        if (g_tracker.pending_count >= DEBOUNCE_THRESHOLD) {
            g_tracker.current = desired;
            g_tracker.pending = (TransformId)-1;
            g_tracker.pending_count = 0;
            return desired;
        }
    } else {
        g_tracker.pending = desired;
        g_tracker.pending_count = 1;
    }

    return g_tracker.current;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    snprintf(g_state_file, sizeof(g_state_file), "/run/user/%d/yogabook-rotation.state", getuid());

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    printf("[INFO] Starting Yoga Book smart auto-rotation daemon (Native C, 2D cross-section & singularity guard)...\n");

    char screen_dev[128] = {0};
    char base_dev[128] = {0};

    for (int i = 0; i < 10; i++) {
        if (find_iio_devices(screen_dev, sizeof(screen_dev), base_dev, sizeof(base_dev)) == 0) {
            break;
        }
        usleep(500000);
    }

    if (!screen_dev[0] || !base_dev[0]) {
        fprintf(stderr, "[ERROR] Failed to find screen and base accelerometers!\n");
        return 1;
    }

    printf("[INFO] Discovered sensors -> Screen: %s, Base: %s\n", screen_dev, base_dev);

    tracker_force(TR_270);

    int32_t s[3] = {0}, b[3] = {0};
    DeviceMode current_mode = MODE_LAPTOP;
    double initial_opening = 0.0;

    for (int i = 0; i < 5; i++) {
        if (read_accel_raw(screen_dev, s) && read_accel_raw(base_dev, b)) {
            current_mode = get_posture(s, b, MODE_LAPTOP, true, &initial_opening);
            break;
        }
        usleep(100000);
    }

    if (current_mode == MODE_TABLET) {
        printf("[INFO] Initial posture: TABLET / FLAT (~%.1f°). Enabling auto-rotation.\n", initial_opening);
        TransformId target_tr = tracker_update(s);
        apply_transform(target_tr);
        write_state(MODE_TABLET, target_tr);
    } else {
        printf("[INFO] Initial posture: LAPTOP (~%.1f°). Locking orientation to %s.\n", initial_opening, DEFAULT_TRANSFORM);
        tracker_force(TR_270);
        apply_transform(TR_270);
        write_state(MODE_LAPTOP, TR_270);
    }

    if (initial_opening >= KEYBOARD_DISABLE_ANGLE) {
        kbd_disable();
    }

    DeviceMode mode_debounce_target = current_mode;
    int mode_debounce_count = 0;
    char last_hdmi_status[32] = {0};

    // Initial HDMI status
    FILE *f_hdmi = fopen(HDMI_STATUS_SYSFS, "r");
    if (f_hdmi) {
        if (fgets(last_hdmi_status, sizeof(last_hdmi_status), f_hdmi)) {
            last_hdmi_status[strcspn(last_hdmi_status, "\r\n")] = '\0';
        }
        fclose(f_hdmi);
    } else {
        strcpy(last_hdmi_status, "disconnected");
    }

    while (g_running) {
        usleep(400000);

        // 1. HDMI Hotplug check
        char cur_hdmi[32] = {0};
        f_hdmi = fopen(HDMI_STATUS_SYSFS, "r");
        if (f_hdmi) {
            if (fgets(cur_hdmi, sizeof(cur_hdmi), f_hdmi)) {
                cur_hdmi[strcspn(cur_hdmi, "\r\n")] = '\0';
            }
            fclose(f_hdmi);
        } else {
            strcpy(cur_hdmi, "disconnected");
        }

        if (strcmp(cur_hdmi, last_hdmi_status) != 0) {
            printf("[INFO] HDMI hotplug transition detected: %s -> %s\n", last_hdmi_status, cur_hdmi);
            strcpy(last_hdmi_status, cur_hdmi);
            int r = system("yogabook-display-mgr apply >/dev/null 2>&1 &");
            (void)r;
        }

        // 2. Read accelerometer values
        if (!read_accel_raw(screen_dev, s) || !read_accel_raw(base_dev, b)) {
            find_iio_devices(screen_dev, sizeof(screen_dev), base_dev, sizeof(base_dev));
            continue;
        }

        double opening = 0.0;
        DeviceMode detected_mode = get_posture(s, b, current_mode, false, &opening);

        // 3. Halo Keyboard state management
        if (opening >= 0.0) {
            if (opening >= KEYBOARD_DISABLE_ANGLE) {
                kbd_disable();
            } else if (opening <= KEYBOARD_ENABLE_ANGLE) {
                kbd_enable();
            }
        }

        // 4. Debounce mode transitions
        if (detected_mode != current_mode) {
            if (detected_mode == mode_debounce_target) {
                mode_debounce_count++;
                if (mode_debounce_count >= MODE_DEBOUNCE_THRESHOLD) {
                    current_mode = detected_mode;
                    if (current_mode == MODE_TABLET) {
                        printf("[INFO] Transition: LAPTOP -> TABLET/FLAT (~%.1f°). Activating auto-rotation.\n", opening);
                        TransformId target_tr = tracker_update(s);
                        apply_transform(target_tr);
                        write_state(MODE_TABLET, target_tr);
                    } else {
                        printf("[INFO] Transition: TABLET/FLAT -> LAPTOP (~%.1f°). Locking orientation to %s.\n", opening, DEFAULT_TRANSFORM);
                        tracker_force(TR_270);
                        apply_transform(TR_270);
                        write_state(MODE_LAPTOP, TR_270);
                        int r = system("pkill -SIGUSR1 -x wvkbd >/dev/null 2>&1");
                        (void)r;
                    }
                    mode_debounce_count = 0;
                    mode_debounce_target = current_mode;
                    continue; // Skip continuous tracking in transition cycle to maintain debounce timing
                }
            } else {
                mode_debounce_target = detected_mode;
                mode_debounce_count = 1;
            }
        } else {
            mode_debounce_count = 0;
        }

        // 5. Tablet continuous orientation tracking
        if (current_mode == MODE_TABLET) {
            TransformId prev_tr = g_tracker.current;
            TransformId target_tr = tracker_update(s);
            if (target_tr != prev_tr) {
                apply_transform(target_tr);
                write_state(MODE_TABLET, target_tr);
            }
        }
    }

    printf("[INFO] Shutting down yogabook-autorotate...\n");
    kbd_enable();
    if (g_state_file[0]) unlink(g_state_file);

    return 0;
}
