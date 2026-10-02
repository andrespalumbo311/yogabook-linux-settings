/*
 * yogabook-powerd: Ultra-low-overhead power & battery protection daemon for Yoga Book
 * (YB1-X91F / Cherryview SoC).
 *
 * Direct sysfs monitoring of TI BQ27542 fuel gauge & BQ25892 charger.
 * - Hardware SOC & voltage monitoring (immune to coulomb counter drift)
 * - Desktop notifications at 15% and 5% via notify-send / Mako
 * - Emergency graceful poweroff at <= 2% or cell voltage <= 3.15V to prevent brownout & eMMC corruption
 * - Instant Waybar updates via RTMIN+8 signal
 *
 * Footprint: < 350 KB RAM, 0% CPU.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <systemd/sd-bus.h>

#define BATT_CAPACITY_PATH "/sys/class/power_supply/bq27542-0/capacity"
#define BATT_VOLTAGE_PATH  "/sys/class/power_supply/bq27542-0/voltage_now"
#define BATT_STATUS_PATH   "/sys/class/power_supply/bq27542-0/status"
#define CHARGER_ONLINE_PATH "/sys/class/power_supply/bq25890-charger-0/online"

// Voltage cutoff: 3.15V (3150000 uV). Nominally Li-ion cutoff is 3.0V.
// Stopping at 3.15V ensures enough headroom under Atom/GPU load spikes before PMIC UVLO trips.
#define EMERGENCY_VOLTAGE_UV 3150000
#define EMERGENCY_CAPACITY_PCT 2

static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_resumed_from_sleep = 0;

static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_running = 0;
    }
}

static int on_prepare_for_sleep(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    (void)userdata;
    (void)ret_error;
    int going_to_sleep = 0;
    int r = sd_bus_message_read(m, "b", &going_to_sleep);
    if (r < 0) return 0;

    if (going_to_sleep) {
        syslog(LOG_INFO, "System entering sleep (s2idle)...");
    } else {
        syslog(LOG_INFO, "System resumed from sleep: recovering PipeWire audio & refreshing battery status");
        // Non-blocking restart of PipeWire audio stack to clear any broken pipe / stale DMA handle
        int ret = system("systemctl --user restart pipewire wireplumber &");
        (void)ret;
        // Immediate Waybar battery update
        ret = system("pkill -RTMIN+8 waybar 2>/dev/null &");
        (void)ret;
        g_resumed_from_sleep = 1;
    }
    return 0;
}

static sd_bus* init_dbus(void) {
    sd_bus *bus = NULL;
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        syslog(LOG_WARNING, "Failed to connect to system bus: %s", strerror(-r));
        return NULL;
    }
    r = sd_bus_match_signal(bus, NULL,
                            "org.freedesktop.login1",
                            "/org/freedesktop/login1",
                            "org.freedesktop.login1.Manager",
                            "PrepareForSleep",
                            on_prepare_for_sleep,
                            NULL);
    if (r < 0) {
        syslog(LOG_WARNING, "Failed to subscribe to PrepareForSleep signal: %s", strerror(-r));
        sd_bus_close(bus);
        sd_bus_unref(bus);
        return NULL;
    }
    return bus;
}

static int read_int_file(const char *path, int default_val) {
    FILE *f = fopen(path, "r");
    if (!f) return default_val;
    int val = default_val;
    if (fscanf(f, "%d", &val) != 1) val = default_val;
    fclose(f);
    return val;
}

static long read_long_file(const char *path, long default_val) {
    FILE *f = fopen(path, "r");
    if (!f) return default_val;
    long val = default_val;
    if (fscanf(f, "%ld", &val) != 1) val = default_val;
    fclose(f);
    return val;
}

static void read_str_file(const char *path, char *buf, size_t max_len, const char *default_str) {
    FILE *f = fopen(path, "r");
    if (!f) {
        strncpy(buf, default_str, max_len - 1);
        buf[max_len - 1] = '\0';
        return;
    }
    if (fscanf(f, "%s", buf) != 1) {
        strncpy(buf, default_str, max_len - 1);
        buf[max_len - 1] = '\0';
    }
    fclose(f);
}

static void notify_user(const char *urgency, const char *icon, const char *title, const char *body) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "notify-send -u %s -a \"Batteria\" %s%s \"%s\" \"%s\" 2>/dev/null",
             urgency,
             icon ? "-i " : "",
             icon ? icon : "",
             title,
             body);
    int ret = system(cmd);
    (void)ret;
}

int main(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    openlog("yogabook-powerd", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Yoga Book power & brownout protection daemon started.");

    sd_bus *bus = init_dbus();

    bool warned_15 = false;
    bool warned_5 = false;
    bool emergency_triggered = false;

    int last_capacity = -1;
    char last_status[32] = "";

    while (g_running) {
        if (g_resumed_from_sleep) {
            g_resumed_from_sleep = 0;
        }

        int capacity = read_int_file(BATT_CAPACITY_PATH, -1);
        long voltage_uV = read_long_file(BATT_VOLTAGE_PATH, 0);
        int charger_online = read_int_file(CHARGER_ONLINE_PATH, 0);
        char status[32] = "Discharging";
        read_str_file(BATT_STATUS_PATH, status, sizeof(status), "Discharging");

        bool is_discharging = (strcmp(status, "Discharging") == 0) && (charger_online == 0);

        // Detect state transitions
        if (strcmp(status, last_status) != 0) {
            syslog(LOG_INFO, "Battery status changed: %s -> %s (Capacity: %d%%, Online: %d)",
                   last_status, status, capacity, charger_online);
            snprintf(last_status, sizeof(last_status), "%s", status);

            if (!is_discharging) {
                // Connected to power: reset discharge warning flags
                warned_15 = false;
                warned_5 = false;
                emergency_triggered = false;
            }

            // Immediately notify Waybar of the status change
            int ret = system("pkill -RTMIN+8 waybar 2>/dev/null");
            (void)ret;
        }

        // Detect capacity change and notify Waybar
        if (capacity != last_capacity) {
            last_capacity = capacity;
            int ret = system("pkill -RTMIN+8 waybar 2>/dev/null");
            (void)ret;
        }

        // Low Battery & Brownout Protection (Discharging only)
        if (is_discharging && capacity >= 0) {
            // Level 3: Emergency Cut-off (Brownout Prevention)
            if (!emergency_triggered && (capacity <= EMERGENCY_CAPACITY_PCT || (voltage_uV > 0 && voltage_uV <= EMERGENCY_VOLTAGE_UV))) {
                emergency_triggered = true;
                syslog(LOG_EMERG, "EMERGENCY: Battery critically exhausted (%d%%, %ld mV). Initiating graceful shutdown to prevent brownout!",
                       capacity, voltage_uV / 1000);
                notify_user("critical", "battery-caution",
                            "Spegnimento di Emergenza",
                            "Batteria quasi esaurita (<2% o <3.15V). Spegnimento in corso per salvaguardare il sistema...");

                sync();
                sleep(2);
                int ret = system("systemctl poweroff");
                (void)ret;
                // Wait for shutdown
                sleep(60);
                continue;
            }

            // Level 2: Critical Alert (5%)
            if (!warned_5 && capacity <= 5) {
                warned_5 = true;
                syslog(LOG_WARNING, "Battery critical threshold reached: %d%%", capacity);
                notify_user("critical", "battery-caution",
                            "Livello Batteria Critico (5%)",
                            "Collega immediatamente l'alimentatore! Il computer si spegnerà a breve.");
            }

            // Level 1: Low Battery Alert (15%)
            if (!warned_15 && capacity <= 15) {
                warned_15 = true;
                syslog(LOG_NOTICE, "Battery low threshold reached: %d%%", capacity);
                notify_user("normal", "battery-low",
                            "Batteria in esaurimento (15%)",
                            "Collega l'alimentatore per continuare a lavorare senza interruzioni.");
            }
        }

        // Sleep interval: 10s if discharging, 30s if charging/plugged
        int sleep_sec = is_discharging ? 10 : 30;
        time_t next_check = time(NULL) + sleep_sec;

        while (g_running && !g_resumed_from_sleep) {
            time_t now = time(NULL);
            if (now >= next_check) {
                break;
            }

            if (bus) {
                while (sd_bus_process(bus, NULL) > 0) {}
                if (g_resumed_from_sleep || !g_running) break;

                uint64_t wait_us = (uint64_t)(next_check - now) * 1000000ULL;
                int r = sd_bus_wait(bus, wait_us);
                if (r < 0 && r != -EINTR) {
                    syslog(LOG_WARNING, "D-Bus wait error: %s, attempting reconnect...", strerror(-r));
                    sd_bus_close(bus);
                    sd_bus_unref(bus);
                    bus = NULL;
                }
            } else {
                sleep(1);
                bus = init_dbus();
            }
        }
    }

    syslog(LOG_INFO, "Yoga Book power daemon terminating cleanly.");
    if (bus) {
        sd_bus_close(bus);
        sd_bus_unref(bus);
    }
    closelog();
    return 0;
}
