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
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <systemd/sd-bus.h>

#define BACKLIGHT_SYSFS "/sys/class/backlight/intel_backlight/brightness"
#define MAX_BACKLIGHT_SYSFS "/sys/class/backlight/intel_backlight/max_brightness"

#define MIN_BRIGHTNESS 8
#define MAX_BRIGHTNESS 100
#define RAMP_STEP_MS 30
#define DEADBAND_PCT 3
#define EMA_ALPHA 0.35

struct CurvePoint {
    double lux;
    int brightness;
};

static const struct CurvePoint CALIBRATION_CURVE[] = {
    {0.0, 10},      // Pitch dark
    {5.0, 16},      // Very dim night lamp
    {20.0, 26},     // Dim indoor room
    {60.0, 42},     // Typical soft indoor light
    {180.0, 58},    // Well-lit room / office
    {450.0, 74},    // Bright indoor / diffuse daylight
    {1000.0, 88},   // Very bright room / window light
    {2200.0, 100}   // Direct daylight / outdoors
};
static const int CALIBRATION_CURVE_LEN = sizeof(CALIBRATION_CURVE) / sizeof(CALIBRATION_CURVE[0]);

static char g_state_file[128] = {0};
static volatile sig_atomic_t g_running = 1;

static int g_max_brightness = 100;
static double g_current_lux = -1.0;
static double g_smoothed_lux = -1.0;
static double g_user_bias = 0.0;
static int g_last_daemon_set_val = -1;
static double g_last_daemon_set_time = 0.0;
static int g_ramp_target = -1;
static uint64_t g_last_ramp_time_ms = 0;

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static int read_max_brightness(void) {
    FILE *f = fopen(MAX_BACKLIGHT_SYSFS, "r");
    if (!f) return 100;
    int val = 100;
    if (fscanf(f, "%d", &val) != 1 || val < 1) val = 100;
    fclose(f);
    return val;
}

static int get_actual_brightness_pct(void) {
    FILE *f = fopen(BACKLIGHT_SYSFS, "r");
    if (!f) return 50;
    int val = 0;
    if (fscanf(f, "%d", &val) != 1) val = 0;
    fclose(f);
    int pct = (int)round((double)val * 100.0 / (double)g_max_brightness);
    return pct;
}

static void write_actual_brightness_pct(int pct) {
    if (pct < MIN_BRIGHTNESS) pct = MIN_BRIGHTNESS;
    if (pct > MAX_BRIGHTNESS) pct = MAX_BRIGHTNESS;

    int raw_val = (int)round((double)pct * (double)g_max_brightness / 100.0);
    if (raw_val < 1) raw_val = 1;
    if (raw_val > g_max_brightness) raw_val = g_max_brightness;

    // 1. Direct sysfs write
    FILE *f = fopen(BACKLIGHT_SYSFS, "w");
    if (f) {
        fprintf(f, "%d\n", raw_val);
        fclose(f);
        g_last_daemon_set_val = pct;
        g_last_daemon_set_time = get_time_sec();
        return;
    }

    // 2. Fallback to brightnessctl CLI
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "brightnessctl --device=intel_backlight set %d%% >/dev/null 2>&1", pct);
    int r = system(cmd);
    (void)r;
    g_last_daemon_set_val = pct;
    g_last_daemon_set_time = get_time_sec();
}

static double interpolate_curve(double lux) {
    if (lux <= CALIBRATION_CURVE[0].lux) {
        return (double)CALIBRATION_CURVE[0].brightness;
    }
    if (lux >= CALIBRATION_CURVE[CALIBRATION_CURVE_LEN - 1].lux) {
        return (double)CALIBRATION_CURVE[CALIBRATION_CURVE_LEN - 1].brightness;
    }

    for (int i = 0; i < CALIBRATION_CURVE_LEN - 1; i++) {
        double x0 = CALIBRATION_CURVE[i].lux;
        double y0 = (double)CALIBRATION_CURVE[i].brightness;
        double x1 = CALIBRATION_CURVE[i + 1].lux;
        double y1 = (double)CALIBRATION_CURVE[i + 1].brightness;

        if (lux >= x0 && lux <= x1) {
            double span = (x1 - x0) > 1e-6 ? (x1 - x0) : 1e-6;
            double ratio = (lux - x0) / span;
            return y0 + ratio * (y1 - y0);
        }
    }
    return 50.0;
}

static void save_state(int current, int target) {
    if (!g_state_file[0]) return;
    FILE *f = fopen(g_state_file, "w");
    if (!f) return;
    fprintf(f, "active=1\nlux=%.1f\nbrightness=%d\ntarget=%d\nbias=%d\n",
            g_current_lux >= 0.0 ? g_current_lux : 0.0,
            current,
            target,
            (int)g_user_bias);
    fclose(f);
}

static void check_manual_adjustment(void) {
    if (g_ramp_target != -1) return; // Currently ramping
    int current_pct = get_actual_brightness_pct();
    if (g_last_daemon_set_val < 0) {
        g_last_daemon_set_val = current_pct;
        g_last_daemon_set_time = get_time_sec();
        return;
    }

    double now = get_time_sec();
    if (abs(current_pct - g_last_daemon_set_val) >= 4 && (now - g_last_daemon_set_time) > 1.0) {
        double cur_lux = g_smoothed_lux >= 0.0 ? g_smoothed_lux : (g_current_lux >= 0.0 ? g_current_lux : 200.0);
        double base_curve_val = interpolate_curve(cur_lux);
        g_user_bias = (double)current_pct - base_curve_val;
        if (g_user_bias < -40.0) g_user_bias = -40.0;
        if (g_user_bias > 40.0) g_user_bias = 40.0;
        g_last_daemon_set_val = current_pct;
        g_last_daemon_set_time = now;
        save_state(current_pct, current_pct);
    }
}

static void handle_lux_reading(double lux) {
    g_current_lux = lux;
    if (g_smoothed_lux < 0.0) {
        g_smoothed_lux = lux;
    } else {
        g_smoothed_lux = (EMA_ALPHA * lux) + ((1.0 - EMA_ALPHA) * g_smoothed_lux);
    }

    check_manual_adjustment();
    int current_pct = get_actual_brightness_pct();

    double base_target = interpolate_curve(g_smoothed_lux);
    int target = (int)round(base_target + g_user_bias);
    if (target < MIN_BRIGHTNESS) target = MIN_BRIGHTNESS;
    if (target > MAX_BRIGHTNESS) target = MAX_BRIGHTNESS;

    if (abs(target - current_pct) >= DEADBAND_PCT) {
        g_ramp_target = target;
        g_last_ramp_time_ms = get_time_ms();
    }

    save_state(current_pct, target);
}

static void do_ramp_step(void) {
    if (g_ramp_target < 0) return;

    int current = get_actual_brightness_pct();
    if (current == g_ramp_target) {
        g_ramp_target = -1;
        save_state(current, current);
        return;
    }

    int step = (g_ramp_target > current) ? 1 : -1;
    int next_val = current + step;
    write_actual_brightness_pct(next_val);

    if (next_val == g_ramp_target) {
        g_ramp_target = -1;
        save_state(next_val, next_val);
    }
}

static int on_properties_changed(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    (void)userdata; (void)ret_error;
    const char *interface_name = NULL;
    int r = sd_bus_message_read(m, "s", &interface_name);
    if (r < 0 || !interface_name || strcmp(interface_name, "net.hadess.SensorProxy") != 0) {
        return 0;
    }

    r = sd_bus_message_enter_container(m, 'a', "{sv}");
    if (r < 0) return 0;

    const char *prop_name = NULL;
    while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
        r = sd_bus_message_read(m, "s", &prop_name);
        if (r >= 0 && prop_name && strcmp(prop_name, "LightLevel") == 0) {
            double lux = 0.0;
            r = sd_bus_message_enter_container(m, 'v', "d");
            if (r >= 0) {
                r = sd_bus_message_read(m, "d", &lux);
                if (r >= 0) {
                    handle_lux_reading(lux);
                }
                sd_bus_message_exit_container(m);
            }
        } else {
            sd_bus_message_skip(m, "v");
        }
        sd_bus_message_exit_container(m);
    }
    sd_bus_message_exit_container(m);
    return 0;
}

static void sig_handler(int sig) {
    (void)sig;
    g_running = 0;
}

static void cmd_status(void) {
    FILE *f = fopen(g_state_file, "r");
    if (!f) {
        int r = system("systemctl --user is-active --quiet yogabook-autobrightness.service");
        if (r == 0) {
            printf("Status: Active (Initializing...)\n");
        } else {
            printf("Status: Inactive\n");
        }
        return;
    }

    char line[128];
    char lux[32] = "0", brightness[32] = "--", target[32] = "--", bias[32] = "0";

    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        val[strcspn(val, "\r\n")] = '\0';

        if (strcmp(key, "lux") == 0) strncpy(lux, val, sizeof(lux) - 1);
        else if (strcmp(key, "brightness") == 0) strncpy(brightness, val, sizeof(brightness) - 1);
        else if (strcmp(key, "target") == 0) strncpy(target, val, sizeof(target) - 1);
        else if (strcmp(key, "bias") == 0) strncpy(bias, val, sizeof(bias) - 1);
    }
    fclose(f);

    printf("Status: Active\n");
    printf("Ambient Light: %s lux\n", lux);
    printf("Current Brightness: %s%%\n", brightness);
    printf("Target Brightness: %s%%\n", target);
    printf("User Offset Bias: %s%%\n", bias);
}

static void cmd_toggle(void) {
    int r = system("systemctl --user is-active --quiet yogabook-autobrightness.service");
    if (r == 0) {
        int s = system("systemctl --user stop yogabook-autobrightness.service");
        (void)s;
        printf("Luminosità automatica: Disattivata\n");
    } else {
        int s = system("systemctl --user start yogabook-autobrightness.service");
        (void)s;
        printf("Luminosità automatica: Attivata\n");
    }
}

static void cmd_start(void) {
    int s = system("systemctl --user start yogabook-autobrightness.service");
    (void)s;
    printf("Luminosità automatica: Attivata\n");
}

static void cmd_stop(void) {
    int s = system("systemctl --user stop yogabook-autobrightness.service");
    (void)s;
    printf("Luminosità automatica: Disattivata\n");
}

static int get_lux_property(sd_bus *bus, double *out_lux) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    int r = sd_bus_get_property(bus,
                                "net.hadess.SensorProxy",
                                "/net/hadess/SensorProxy",
                                "net.hadess.SensorProxy",
                                "LightLevel",
                                &error,
                                &reply,
                                "d");
    if (r >= 0 && reply) {
        r = sd_bus_message_read(reply, "d", out_lux);
    }
    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);
    return r;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    snprintf(g_state_file, sizeof(g_state_file), "/run/user/%d/yogabook-autobrightness.state", getuid());

    if (argc > 1) {
        char *arg = argv[1];
        while (*arg == '-') arg++;
        if (strcmp(arg, "status") == 0 || strcmp(arg, "s") == 0) {
            cmd_status();
            return 0;
        } else if (strcmp(arg, "toggle") == 0 || strcmp(arg, "t") == 0) {
            cmd_toggle();
            return 0;
        } else if (strcmp(arg, "start") == 0 || strcmp(arg, "on") == 0 || strcmp(arg, "enable") == 0) {
            cmd_start();
            return 0;
        } else if (strcmp(arg, "stop") == 0 || strcmp(arg, "off") == 0 || strcmp(arg, "disable") == 0) {
            cmd_stop();
            return 0;
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    printf("[INFO] Starting Yoga Book smart auto-brightness daemon (Native C, sd-bus)...\n");

    g_max_brightness = read_max_brightness();

    sd_bus *bus = NULL;
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "[ERROR] Failed to connect to system bus: %s\n", strerror(-r));
        return 1;
    }

    // Subscribe to SensorProxy PropertiesChanged
    r = sd_bus_match_signal(bus, NULL,
                            "net.hadess.SensorProxy",
                            "/net/hadess/SensorProxy",
                            "org.freedesktop.DBus.Properties",
                            "PropertiesChanged",
                            on_properties_changed,
                            NULL);
    if (r < 0) {
        fprintf(stderr, "[WARNING] Failed to match PropertiesChanged signal: %s\n", strerror(-r));
    }

    // Claim light sensor
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    r = sd_bus_call_method(bus,
                           "net.hadess.SensorProxy",
                           "/net/hadess/SensorProxy",
                           "net.hadess.SensorProxy",
                           "ClaimLight",
                           &error,
                           &reply,
                           "");
    if (r < 0) {
        fprintf(stderr, "[WARNING] ClaimLight method call failed: %s\n", error.message ? error.message : strerror(-r));
    }
    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);

    // Initial LightLevel query
    double initial_lux = 0.0;
    if (get_lux_property(bus, &initial_lux) >= 0) {
        handle_lux_reading(initial_lux);
    }

    uint64_t last_poll_ms = get_time_ms();

    while (g_running) {
        // Drain pending D-Bus messages
        while (sd_bus_process(bus, NULL) > 0) {}

        uint64_t now_ms = get_time_ms();

        // 1. Ramping step if active
        if (g_ramp_target >= 0) {
            if (now_ms - g_last_ramp_time_ms >= RAMP_STEP_MS) {
                do_ramp_step();
                g_last_ramp_time_ms = now_ms;
            }
        }

        // 2. Periodic poll every 1500ms (manual adjustment & fallback)
        if (now_ms - last_poll_ms >= 1500) {
            check_manual_adjustment();

            double poll_lux = 0.0;
            if (get_lux_property(bus, &poll_lux) >= 0) {
                if (g_current_lux < 0.0 || fabs(poll_lux - g_current_lux) > 1.0) {
                    handle_lux_reading(poll_lux);
                }
            }
            last_poll_ms = now_ms;
        }

        // 3. Sleep waiting for bus events or ramp timeout
        uint64_t timeout_usec = (g_ramp_target >= 0) ? (RAMP_STEP_MS * 1000) : (1500 * 1000);
        sd_bus_wait(bus, timeout_usec);
    }

    printf("[INFO] Shutting down yogabook-autobrightness...\n");

    // Release light sensor
    sd_bus_call_method(bus,
                       "net.hadess.SensorProxy",
                       "/net/hadess/SensorProxy",
                       "net.hadess.SensorProxy",
                       "ReleaseLight",
                       NULL,
                       NULL,
                       "");

    sd_bus_unref(bus);

    if (g_state_file[0]) unlink(g_state_file);

    return 0;
}
