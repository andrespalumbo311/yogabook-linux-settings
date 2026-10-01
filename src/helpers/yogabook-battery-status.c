#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

int main(void) {
    int capacity = -1;
    long voltage_uV = 0;
    long current_uA = 0;
    long charge_full = 0;
    long charge_full_design = 0;
    char status[32] = "Discharging";
    char charge_type[32] = "";
    int charger_online = 0;

    FILE *f;

    // 1. Direct hardware SOC from fuel gauge
    f = fopen("/sys/class/power_supply/bq27542-0/capacity", "r");
    if (f) {
        if (fscanf(f, "%d", &capacity) != 1) capacity = -1;
        fclose(f);
    }

    // 2. Voltage (uV)
    f = fopen("/sys/class/power_supply/bq27542-0/voltage_now", "r");
    if (f) {
        if (fscanf(f, "%ld", &voltage_uV) != 1) voltage_uV = 0;
        fclose(f);
    }

    // 3. Current (uA)
    f = fopen("/sys/class/power_supply/bq27542-0/current_now", "r");
    if (f) {
        if (fscanf(f, "%ld", &current_uA) != 1) current_uA = 0;
        fclose(f);
    }

    // 4. Status
    f = fopen("/sys/class/power_supply/bq27542-0/status", "r");
    if (f) {
        if (fscanf(f, "%31s", status) != 1) strcpy(status, "Discharging");
        fclose(f);
    }

    // 5. Charger online & charge type
    f = fopen("/sys/class/power_supply/bq25890-charger-0/online", "r");
    if (f) {
        if (fscanf(f, "%d", &charger_online) != 1) charger_online = 0;
        fclose(f);
    }

    f = fopen("/sys/class/power_supply/bq25890-charger-0/charge_type", "r");
    if (f) {
        if (fscanf(f, "%31s", charge_type) != 1) charge_type[0] = '\0';
        fclose(f);
    }

    // 6. Health calculation (Full / Design)
    f = fopen("/sys/class/power_supply/bq27542-0/charge_full", "r");
    if (f) {
        if (fscanf(f, "%ld", &charge_full) != 1) charge_full = 0;
        fclose(f);
    }

    f = fopen("/sys/class/power_supply/bq27542-0/charge_full_design", "r");
    if (f) {
        if (fscanf(f, "%ld", &charge_full_design) != 1) charge_full_design = 0;
        fclose(f);
    }

    // Fallback if capacity read failed
    if (capacity < 0) {
        long charge_now = 0;
        f = fopen("/sys/class/power_supply/bq27542-0/charge_now", "r");
        if (f) {
            if (fscanf(f, "%ld", &charge_now) != 1) charge_now = 0;
            fclose(f);
        }
        if (charge_full > 0) {
            capacity = (int)round((double)charge_now * 100.0 / (double)charge_full);
        } else {
            capacity = 0;
        }
    }

    if (capacity < 0) capacity = 0;
    if (capacity > 100) capacity = 100;

    int health = 100;
    if (charge_full_design > 0 && charge_full > 0) {
        health = (int)round((double)charge_full * 100.0 / (double)charge_full_design);
        if (health > 100) health = 100;
    }

    bool is_charging = (strcmp(status, "Charging") == 0);
    bool is_full = (strcmp(status, "Full") == 0 || (capacity == 100 && charger_online));

    // Choose class
    const char *css_class = "normal";
    if (is_charging) {
        css_class = "charging";
    } else if (charger_online && is_full) {
        css_class = "plugged";
    } else if (capacity <= 15) {
        css_class = "critical";
    } else if (capacity <= 30) {
        css_class = "warning";
    }

    // Choose icon
    const char *icon = "󰁹";
    if (is_charging) {
        icon = "󰂄";
    } else if (charger_online && is_full) {
        icon = "󰚥";
    } else {
        const char *icons[] = {
            "󰂎", // 0
            "󰁺", // 1
            "󰁻", // 2
            "󰁼", // 3
            "󰁽", // 4
            "󰁾", // 5
            "󰁿", // 6
            "󰂀", // 7
            "󰂁", // 8
            "󰂂", // 9
            "󰁹"  // 10
        };
        int idx = capacity / 10;
        if (idx < 0) idx = 0;
        if (idx > 10) idx = 10;
        icon = icons[idx];
    }

    // Power calculation
    double voltage_v = (double)voltage_uV / 1000000.0;
    double current_a = (double)current_uA / 1000000.0;
    double power_w = fabs(voltage_v * current_a);

    char state_desc[64];
    if (is_charging) {
        if (strcmp(charge_type, "Fast") == 0) {
            snprintf(state_desc, sizeof(state_desc), "In carica rapida (PE+ 12V)");
        } else {
            snprintf(state_desc, sizeof(state_desc), "In carica (Standard)");
        }
    } else if (charger_online) {
        snprintf(state_desc, sizeof(state_desc), "Alimentatore collegato");
    } else {
        snprintf(state_desc, sizeof(state_desc), "In scarica");
    }

    // Emit JSON for Waybar
    printf("{\"text\":\"%s %d%%\",\"tooltip\":\"%s\\nCapacità reale: %d%%\\nTensione: %.2f V\\nPotenza: %.2f W\\nSalute: %d%%\\n• Click: Centro di Controllo\",\"class\":\"%s\",\"percentage\":%d}\n",
           icon, capacity,
           state_desc, capacity, voltage_v, power_w, health,
           css_class, capacity);

    return 0;
}
