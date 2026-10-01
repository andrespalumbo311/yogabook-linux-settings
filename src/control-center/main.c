#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PIDFILE "/tmp/yogabook-control-center.pid"

static const char *CSS_DATA =
"* {\n"
"    font-family: \"Adwaita Sans\", \"Liberation Sans\", Roboto, sans-serif;\n"
"}\n"
"window#control-center-overlay {\n"
"    background-color: rgba(0, 0, 0, 0.25);\n"
"}\n"
".cc-card {\n"
"    background-color: #ffffff;\n"
"    border: 1px solid #dcdfe4;\n"
"    border-top: none;\n"
"    border-radius: 0 0 24px 24px;\n"
"    padding: 16px;\n"
"    box-shadow: 0 12px 32px rgba(0, 0, 0, 0.22);\n"
"    min-width: 350px;\n"
"}\n"
".header-box {\n"
"    margin-bottom: 12px;\n"
"    padding: 0 2px;\n"
"}\n"
".status-pill {\n"
"    background-color: #f1f3f9;\n"
"    border: 1px solid #e0e2ec;\n"
"    border-radius: 12px;\n"
"    padding: 4px 10px;\n"
"}\n"
".status-text {\n"
"    font-size: 13px;\n"
"    font-weight: 700;\n"
"    color: #1f1f1f;\n"
"}\n"
".header-btn {\n"
"    min-width: 32px;\n"
"    min-height: 32px;\n"
"    border-radius: 16px;\n"
"    background-color: #f1f3f9;\n"
"    border: 1px solid #e0e2ec;\n"
"    color: #444746;\n"
"    font-family: \"JetBrainsMono Nerd Font Propo\", \"Symbols Nerd Font\", sans-serif;\n"
"    font-size: 14px;\n"
"    padding: 0;\n"
"    margin-left: 6px;\n"
"    transition: all 150ms ease;\n"
"}\n"
".header-btn:hover {\n"
"    background-color: #e2e5ee;\n"
"    color: #1f1f1f;\n"
"}\n"
".header-btn.shutdown:hover {\n"
"    background-color: #fce8e6;\n"
"    border-color: #f28b82;\n"
"    color: #c5221f;\n"
"}\n"
".slider-pill {\n"
"    background-color: #f1f3f9;\n"
"    border: 1px solid #e0e2ec;\n"
"    border-radius: 24px;\n"
"    padding: 2px 10px;\n"
"    min-height: 48px;\n"
"    margin-bottom: 10px;\n"
"}\n"
".slider-icon-btn {\n"
"    min-width: 34px;\n"
"    min-height: 34px;\n"
"    border-radius: 17px;\n"
"    background: transparent;\n"
"    border: none;\n"
"    padding: 0;\n"
"    color: #0b57d0;\n"
"    font-family: \"JetBrainsMono Nerd Font Propo\", \"Symbols Nerd Font\", sans-serif;\n"
"    font-size: 18px;\n"
"    box-shadow: none;\n"
"}\n"
".slider-icon-btn.muted {\n"
"    color: #c5221f;\n"
"}\n"
".slider-value-label {\n"
"    min-width: 38px;\n"
"    font-size: 13px;\n"
"    font-weight: 700;\n"
"    color: #1f1f1f;\n"
"    margin-left: 6px;\n"
"}\n"
"scale {\n"
"    min-height: 24px;\n"
"    padding: 0;\n"
"    margin: 0 4px;\n"
"}\n"
"scale trough {\n"
"    min-height: 12px;\n"
"    border-radius: 6px;\n"
"    background-color: #dbe0ea;\n"
"}\n"
"scale highlight {\n"
"    min-height: 12px;\n"
"    border-radius: 6px;\n"
"    background-color: #0b57d0;\n"
"}\n"
"scale slider {\n"
"    min-width: 22px;\n"
"    min-height: 22px;\n"
"    border-radius: 11px;\n"
"    background-color: #0b57d0;\n"
"    margin: -5px 0;\n"
"    box-shadow: 0 2px 6px rgba(0, 0, 0, 0.25);\n"
"    border: 2px solid #ffffff;\n"
"}\n"
".tiles-grid {\n"
"    margin-top: 4px;\n"
"    margin-bottom: 6px;\n"
"}\n"
".tile-btn {\n"
"    background-color: #f1f3f9;\n"
"    border: 1px solid #e0e2ec;\n"
"    border-radius: 18px;\n"
"    padding: 8px 12px;\n"
"    min-height: 54px;\n"
"    transition: all 150ms ease;\n"
"    outline: none;\n"
"}\n"
".tile-btn:hover {\n"
"    background-color: #e6eaf2;\n"
"}\n"
".tile-btn.active {\n"
"    background-color: #d3e3fd;\n"
"    border: 1px solid #7cacf8;\n"
"}\n"
".tile-icon {\n"
"    font-family: \"JetBrainsMono Nerd Font Propo\", \"Symbols Nerd Font\", sans-serif;\n"
"    font-size: 20px;\n"
"    margin-right: 10px;\n"
"    color: #444746;\n"
"}\n"
".tile-btn.active .tile-icon {\n"
"    color: #041e49;\n"
"}\n"
".tile-title {\n"
"    font-size: 13px;\n"
"    font-weight: 700;\n"
"    color: #1f1f1f;\n"
"}\n"
".tile-btn.active .tile-title {\n"
"    color: #041e49;\n"
"}\n"
".tile-subtitle {\n"
"    font-size: 11px;\n"
"    font-weight: 500;\n"
"    color: #74777f;\n"
"}\n"
".tile-btn.active .tile-subtitle {\n"
"    color: #0b57d0;\n"
"}\n";

typedef struct {
    GtkWidget *button;
    GtkWidget *icon_label;
    GtkWidget *title_label;
    GtkWidget *sub_label;
    const char *icon_active;
    const char *icon_inactive;
    const char *title_text;
    const char *sub_active;
    const char *sub_inactive;
    gboolean is_active;
    void (*toggle_fn)(gpointer user_data);
} QuickTile;

typedef struct {
    GtkWidget *window;
    GtkWidget *card;
    GtkWidget *battery_label;
    
    // Sliders
    GtkWidget *vol_icon_btn;
    GtkWidget *vol_scale;
    GtkWidget *vol_label;
    gboolean is_muted;
    gboolean syncing_vol;
    guint vol_debounce_id;
    int pending_vol;

    GtkWidget *bright_icon_btn;
    GtkWidget *bright_scale;
    GtkWidget *bright_label;
    gboolean syncing_bright;
    guint bright_debounce_id;
    int pending_bright;
    int max_brightness;

    // Tiles
    QuickTile tile_wifi;
    QuickTile tile_bt;
    QuickTile tile_kbd;
    QuickTile tile_rot;
    QuickTile tile_dnd;
    QuickTile tile_shot;
    QuickTile tile_epb;

    // Live Polling
    gint64 last_user_bright_time;
    gint64 last_user_vol_time;
    guint poll_counter;
    guint poll_timer_id;
} AppState;

static AppState g_app;

static void cleanup_pidfile(void) {
    unlink(PIDFILE);
}

static void on_sigterm(int sig) {
    (void)sig;
    if (g_app.poll_timer_id > 0) {
        g_source_remove(g_app.poll_timer_id);
        g_app.poll_timer_id = 0;
    }
    cleanup_pidfile();
    gtk_main_quit();
}

static void spawn_async(const char *cmd) {
    g_spawn_command_line_async(cmd, NULL);
}

// Battery Info
static void update_battery_info(AppState *app) {
    int cap = -1;
    char status[32] = "Discharging";
    FILE *f;

    f = fopen("/sys/class/power_supply/bq27542-0/capacity", "r");
    if (f) {
        if (fscanf(f, "%d", &cap) != 1) cap = -1;
        fclose(f);
    }

    f = fopen("/sys/class/power_supply/bq27542-0/status", "r");
    if (f) {
        if (fscanf(f, "%31s", status) != 1) strcpy(status, "Discharging");
        fclose(f);
    }

    if (cap < 0) {
        int charge_now = 0, charge_full = 0;
        f = fopen("/sys/class/power_supply/bq27542-0/charge_now", "r");
        if (f) { fscanf(f, "%d", &charge_now); fclose(f); }

        f = fopen("/sys/class/power_supply/bq27542-0/charge_full", "r");
        if (f) { fscanf(f, "%d", &charge_full); fclose(f); }

        if (charge_full > 0) {
            cap = (int)round(charge_now * 100.0 / charge_full);
        } else {
            cap = 50;
        }
    }

    if (cap < 0) cap = 0;
    if (cap > 100) cap = 100;

    gboolean charging = (strcmp(status, "Charging") == 0 || strcmp(status, "Full") == 0);
    const char *icon = charging ? "󰂄" : (cap > 90 ? "󰁹" : (cap > 60 ? "󰂀" : (cap > 30 ? "󰁾" : "󰁻")));

    char buf[64];
    snprintf(buf, sizeof(buf), "%s %d%%", icon, cap);
    gtk_label_set_text(GTK_LABEL(app->battery_label), buf);
}

// Brightness Logic
static int get_sysfs_brightness(AppState *app) {
    int b = 50;
    FILE *f = fopen("/sys/class/backlight/intel_backlight/brightness", "r");
    if (f) {
        fscanf(f, "%d", &b);
        fclose(f);
    }
    int max_b = app->max_brightness > 0 ? app->max_brightness : 100;
    int pct = (int)round(b * 100.0 / max_b);
    if (pct < 1) pct = 1;
    if (pct > 100) pct = 100;
    return pct;
}

static void update_brightness_icon(AppState *app, int val) {
    char state_path[128];
    snprintf(state_path, sizeof(state_path), "/run/user/%d/yogabook-autobrightness.state", getuid());
    if (access(state_path, F_OK) == 0) {
        gtk_button_set_label(GTK_BUTTON(app->bright_icon_btn), "󰃡");
        gtk_widget_set_tooltip_text(app->bright_icon_btn, "Luminosità Automatica: Attiva (Tocca per disattivare)");
    } else {
        if (val < 33) gtk_button_set_label(GTK_BUTTON(app->bright_icon_btn), "󰃞");
        else if (val < 66) gtk_button_set_label(GTK_BUTTON(app->bright_icon_btn), "󰃟");
        else gtk_button_set_label(GTK_BUTTON(app->bright_icon_btn), "󰃠");
        gtk_widget_set_tooltip_text(app->bright_icon_btn, "Luminosità Manuale (Tocca per attivare)");
    }
}

static void set_sysfs_brightness(AppState *app, int pct) {
    int max_b = app->max_brightness > 0 ? app->max_brightness : 100;
    int b = (pct * max_b + 50) / 100;
    FILE *f = fopen("/sys/class/backlight/intel_backlight/brightness", "w");
    if (f) {
        fprintf(f, "%d\n", b);
        fclose(f);
    }
}

static gboolean apply_pending_brightness(gpointer data) {
    AppState *app = (AppState *)data;
    if (app->pending_bright >= 0) {
        set_sysfs_brightness(app, app->pending_bright);
        app->pending_bright = -1;
    }
    app->bright_debounce_id = 0;
    return G_SOURCE_REMOVE;
}

static void on_brightness_value_changed(GtkRange *range, gpointer user_data) {
    AppState *app = (AppState *)user_data;
    if (app->syncing_bright) return;

    app->last_user_bright_time = g_get_monotonic_time();
    int val = (int)gtk_range_get_value(range);
    if (val < 1) val = 1;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", val);
    gtk_label_set_text(GTK_LABEL(app->bright_label), buf);
    update_brightness_icon(app, val);

    app->pending_bright = val;
    if (app->bright_debounce_id == 0) {
        app->bright_debounce_id = g_timeout_add(25, apply_pending_brightness, app);
    }
}

static void on_autobrightness_toggle(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    spawn_async("yogabook-autobrightness toggle");
    g_usleep(50000);
    int b = get_sysfs_brightness(app);
    app->syncing_bright = TRUE;
    gtk_range_set_value(GTK_RANGE(app->bright_scale), b);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", b);
    gtk_label_set_text(GTK_LABEL(app->bright_label), buf);
    update_brightness_icon(app, b);
    app->syncing_bright = FALSE;
}

// Volume Logic
static void update_volume_icon(AppState *app, int vol, gboolean muted) {
    GtkStyleContext *ctx = gtk_widget_get_style_context(app->vol_icon_btn);
    if (muted || vol == 0) {
        gtk_button_set_label(GTK_BUTTON(app->vol_icon_btn), "󰝟");
        gtk_style_context_add_class(ctx, "muted");
    } else {
        gtk_style_context_remove_class(ctx, "muted");
        if (vol < 33) gtk_button_set_label(GTK_BUTTON(app->vol_icon_btn), "󰕿");
        else if (vol < 66) gtk_button_set_label(GTK_BUTTON(app->vol_icon_btn), "󰖀");
        else gtk_button_set_label(GTK_BUTTON(app->vol_icon_btn), "󰕾");
    }
}

static void get_current_volume(int *out_vol, gboolean *out_muted) {
    *out_vol = 50;
    *out_muted = FALSE;
    FILE *pipe = popen("wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null", "r");
    if (!pipe) return;

    char line[128];
    if (fgets(line, sizeof(line), pipe)) {
        float fval = 0.5f;
        if (sscanf(line, "Volume: %f", &fval) == 1) {
            *out_vol = (int)roundf(fval * 100.0f);
            if (*out_vol < 0) *out_vol = 0;
            if (*out_vol > 100) *out_vol = 100;
        }
        if (strstr(line, "[MUTED]")) {
            *out_muted = TRUE;
        }
    }
    pclose(pipe);
}

static gboolean apply_pending_volume(gpointer data) {
    AppState *app = (AppState *)data;
    if (app->pending_vol >= 0) {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "wpctl set-volume @DEFAULT_AUDIO_SINK@ %d%%", app->pending_vol);
        spawn_async(cmd);
        app->pending_vol = -1;
    }
    app->vol_debounce_id = 0;
    return G_SOURCE_REMOVE;
}

static void on_volume_value_changed(GtkRange *range, gpointer user_data) {
    AppState *app = (AppState *)user_data;
    if (app->syncing_vol) return;

    app->last_user_vol_time = g_get_monotonic_time();
    int val = (int)gtk_range_get_value(range);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", val);
    gtk_label_set_text(GTK_LABEL(app->vol_label), buf);
    update_volume_icon(app, val, app->is_muted);

    app->pending_vol = val;
    if (app->vol_debounce_id == 0) {
        app->vol_debounce_id = g_timeout_add(30, apply_pending_volume, app);
    }
}

static void on_volume_mute_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState *)user_data;
    spawn_async("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle");
    g_usleep(40000);
    int vol;
    gboolean muted;
    get_current_volume(&vol, &muted);
    app->is_muted = muted;
    app->syncing_vol = TRUE;
    gtk_range_set_value(GTK_RANGE(app->vol_scale), vol);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", vol);
    gtk_label_set_text(GTK_LABEL(app->vol_label), buf);
    update_volume_icon(app, vol, muted);
    app->syncing_vol = FALSE;
}

// Quick Tiles Logic
static void update_tile_ui(QuickTile *tile, gboolean active, const char *custom_sub) {
    tile->is_active = active;
    GtkStyleContext *ctx = gtk_widget_get_style_context(tile->button);
    if (active) {
        gtk_style_context_add_class(ctx, "active");
        gtk_label_set_text(GTK_LABEL(tile->icon_label), tile->icon_active);
        gtk_label_set_text(GTK_LABEL(tile->sub_label), custom_sub ? custom_sub : tile->sub_active);
    } else {
        gtk_style_context_remove_class(ctx, "active");
        gtk_label_set_text(GTK_LABEL(tile->icon_label), tile->icon_inactive);
        gtk_label_set_text(GTK_LABEL(tile->sub_label), custom_sub ? custom_sub : tile->sub_inactive);
    }
}

// 1. Wi-Fi
static gboolean check_wifi_active(void) {
    FILE *p = popen("rfkill list wifi 2>/dev/null", "r");
    if (!p) return FALSE;
    char buf[512];
    gboolean ok = FALSE;
    while (fgets(buf, sizeof(buf), p)) {
        if (strstr(buf, "Soft blocked: no")) ok = TRUE;
    }
    pclose(p);
    return ok;
}

static gboolean idle_update_wifi(gpointer user_data) {
    (void)user_data;
    update_tile_ui(&g_app.tile_wifi, check_wifi_active(), NULL);
    return G_SOURCE_REMOVE;
}

static void* thread_toggle_wifi(void *arg) {
    (void)arg;
    system("rfkill toggle wifi >/dev/null 2>&1");
    g_idle_add(idle_update_wifi, NULL);
    return NULL;
}

static void toggle_wifi(gpointer user_data) {
    (void)user_data;
    pthread_t th;
    pthread_create(&th, NULL, thread_toggle_wifi, NULL);
    pthread_detach(th);
}

// 2. Bluetooth (1:1 with Python logic)
static gboolean check_bt_active(void) {
    FILE *p = popen("rfkill list bluetooth 2>/dev/null", "r");
    if (!p) return FALSE;
    char buf[512];
    gboolean ok = FALSE;
    while (fgets(buf, sizeof(buf), p)) {
        if (strstr(buf, "Soft blocked: no")) ok = TRUE;
    }
    pclose(p);
    return ok;
}

static gboolean idle_update_bt(gpointer user_data) {
    (void)user_data;
    update_tile_ui(&g_app.tile_bt, check_bt_active(), NULL);
    return G_SOURCE_REMOVE;
}

static void* thread_toggle_bt(void *arg) {
    (void)arg;
    gboolean active = check_bt_active();
    if (active) {
        system("bluetoothctl power off >/dev/null 2>&1");
        system("rfkill block bluetooth >/dev/null 2>&1");
    } else {
        system("rfkill unblock bluetooth >/dev/null 2>&1");
        usleep(100000); // 100ms
        system("bluetoothctl power on >/dev/null 2>&1");
    }
    usleep(100000); // 100ms
    g_idle_add(idle_update_bt, NULL);
    return NULL;
}

static void toggle_bt(gpointer user_data) {
    (void)user_data;
    pthread_t th;
    pthread_create(&th, NULL, thread_toggle_bt, NULL);
    pthread_detach(th);
}

// 3. Virtual Keyboard
static void toggle_kbd(gpointer user_data) {
    (void)user_data;
    cleanup_pidfile();
    spawn_async("/home/andres/.local/bin/toggle-keyboard");
    gtk_main_quit();
}

// 4. Rotation (Decoupled lock mechanism; rot8.service is never killed)
static gboolean check_rot_status(char *out_sub, size_t maxlen) {
    char lock_path[128];
    snprintf(lock_path, sizeof(lock_path), "/run/user/%d/yogabook-rotation.lock", getuid());
    gboolean is_locked = (access(lock_path, F_OK) == 0);

    char state_path[128];
    snprintf(state_path, sizeof(state_path), "/run/user/%d/yogabook-rotation.state", getuid());
    if (access(state_path, F_OK) != 0) {
        snprintf(out_sub, maxlen, "Disattivata");
        return FALSE;
    }

    char mode[32] = "laptop";
    char tr[32] = "270";
    int autorotate = is_locked ? 0 : 1;

    FILE *f = fopen(state_path, "r");
    if (f) {
        char line[64];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "mode=", 5) == 0) {
                sscanf(line + 5, "%31s", mode);
            } else if (strncmp(line, "transform=", 10) == 0) {
                sscanf(line + 10, "%31s", tr);
            } else if (strncmp(line, "autorotate=", 11) == 0) {
                sscanf(line + 11, "%d", &autorotate);
            }
        }
        fclose(f);
    }

    if (is_locked || autorotate == 0) {
        if (strcmp(mode, "tablet") == 0) {
            snprintf(out_sub, maxlen, "Bloccata (%s°)", tr);
        } else {
            snprintf(out_sub, maxlen, "Bloccata (Laptop)");
        }
        return FALSE;
    } else {
        if (strcmp(mode, "tablet") == 0) {
            snprintf(out_sub, maxlen, "Attiva (Tablet)");
        } else {
            snprintf(out_sub, maxlen, "Attiva (Laptop)");
        }
        return TRUE;
    }
}

static gboolean idle_update_rot(gpointer user_data) {
    (void)user_data;
    char sub[64];
    gboolean act = check_rot_status(sub, sizeof(sub));
    update_tile_ui(&g_app.tile_rot, act, sub);
    return G_SOURCE_REMOVE;
}

static void* thread_toggle_rot(void *arg) {
    (void)arg;
    char lock_path[128];
    snprintf(lock_path, sizeof(lock_path), "/run/user/%d/yogabook-rotation.lock", getuid());

    // Ensure rot8.service is active
    if (system("systemctl --user is-active --quiet rot8.service") != 0) {
        system("systemctl --user start rot8.service >/dev/null 2>&1");
        usleep(200000);
    }

    if (access(lock_path, F_OK) == 0) {
        // Currently locked -> Unlock
        unlink(lock_path);
    } else {
        // Currently unlocked -> Lock
        char state_path[128];
        snprintf(state_path, sizeof(state_path), "/run/user/%d/yogabook-rotation.state", getuid());
        char tr[32] = "current";
        FILE *sf = fopen(state_path, "r");
        if (sf) {
            char line[64];
            while (fgets(line, sizeof(line), sf)) {
                if (strncmp(line, "transform=", 10) == 0) {
                    sscanf(line + 10, "%31s", tr);
                }
            }
            fclose(sf);
        }
        FILE *lf = fopen(lock_path, "w");
        if (lf) {
            fprintf(lf, "%s\n", tr);
            fclose(lf);
        }
    }

    usleep(150000); // 150ms
    g_idle_add(idle_update_rot, NULL);
    return NULL;
}

static void toggle_rot(gpointer user_data) {
    (void)user_data;
    pthread_t th;
    pthread_create(&th, NULL, thread_toggle_rot, NULL);
    pthread_detach(th);
}

// 5. Silent / DND
static gboolean check_dnd_active(void) {
    FILE *p = popen("makoctl mode 2>/dev/null", "r");
    if (p) {
        char buf[64];
        gboolean act = FALSE;
        gboolean found_any = FALSE;
        while (fgets(buf, sizeof(buf), p)) {
            found_any = TRUE;
            if (strstr(buf, "dnd")) {
                act = TRUE;
                break;
            }
        }
        int status = pclose(p);
        if (status == 0 || found_any) return act;
    }
    // Fallback to swaync if mako is not active
    p = popen("swaync-client -D 2>/dev/null", "r");
    if (!p) return FALSE;
    char buf[16];
    gboolean act = FALSE;
    if (fgets(buf, sizeof(buf), p)) {
        if (strstr(buf, "true")) act = TRUE;
    }
    pclose(p);
    return act;
}

static gboolean idle_update_dnd(gpointer user_data) {
    (void)user_data;
    update_tile_ui(&g_app.tile_dnd, check_dnd_active(), NULL);
    return G_SOURCE_REMOVE;
}

static void* thread_toggle_dnd(void *arg) {
    (void)arg;
    system("if which makoctl >/dev/null 2>&1; then makoctl mode -t dnd; else swaync-client -d -sw; fi >/dev/null 2>&1; pkill -RTMIN+9 waybar 2>/dev/null");
    usleep(100000);
    g_idle_add(idle_update_dnd, NULL);
    return NULL;
}

static void toggle_dnd(gpointer user_data) {
    (void)user_data;
    pthread_t th;
    pthread_create(&th, NULL, thread_toggle_dnd, NULL);
    pthread_detach(th);
}

// 6. Screenshot
static void take_screenshot(gpointer user_data) {
    (void)user_data;
    cleanup_pidfile();
    spawn_async("sh -c 'sleep 0.3; mkdir -p ~/Pictures/Screenshots; FILE=~/Pictures/Screenshots/screenshot-$(date +%Y%m%d-%H%M%S).png; grim \"$FILE\" && notify-send \"Screenshot Salvato\" \"$FILE\"'");
    gtk_main_quit();
}

// 7. CPU Profile (EPB)
static gboolean check_epb_status(char *out_sub, size_t maxlen) {
    int val = 6;
    FILE *f = fopen("/sys/devices/system/cpu/cpu0/power/energy_perf_bias", "r");
    if (f) {
        fscanf(f, "%d", &val);
        fclose(f);
    }
    if (val == 0) {
        snprintf(out_sub, maxlen, "Prestazioni (EPB 0)");
        return TRUE;
    } else if (val <= 4) {
        snprintf(out_sub, maxlen, "Bilanciato+ (EPB 4)");
        return TRUE;
    } else {
        snprintf(out_sub, maxlen, "Bilanciato (EPB 6)");
        return FALSE;
    }
}

static gboolean idle_update_epb(gpointer user_data) {
    (void)user_data;
    char sub[64];
    gboolean act = check_epb_status(sub, sizeof(sub));
    update_tile_ui(&g_app.tile_epb, act, sub);
    return G_SOURCE_REMOVE;
}

static void* thread_toggle_epb(void *arg) {
    (void)arg;
    int target = g_app.tile_epb.is_active ? 6 : 0;
    for (int i = 0; i < 4; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/power/energy_perf_bias", i);
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "%d\n", target);
            fclose(f);
        }
    }
    g_idle_add(idle_update_epb, NULL);
    return NULL;
}

static void toggle_epb(gpointer user_data) {
    (void)user_data;
    pthread_t th;
    pthread_create(&th, NULL, thread_toggle_epb, NULL);
    pthread_detach(th);
}

static void on_tile_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    QuickTile *tile = (QuickTile *)user_data;
    if (tile->toggle_fn) {
        tile->toggle_fn(tile);
    }
}

static GtkWidget* create_tile(QuickTile *tile, const char *icon_act, const char *icon_inact,
                              const char *title, const char *sub_act, const char *sub_inact,
                              void (*toggle_fn)(gpointer)) {
    tile->icon_active = icon_act;
    tile->icon_inactive = icon_inact;
    tile->title_text = title;
    tile->sub_active = sub_act;
    tile->sub_inactive = sub_inact;
    tile->toggle_fn = toggle_fn;
    tile->is_active = FALSE;

    tile->button = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(tile->button), "tile-btn");

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    tile->icon_label = gtk_label_new(icon_inact);
    gtk_style_context_add_class(gtk_widget_get_style_context(tile->icon_label), "tile-icon");
    gtk_box_pack_start(GTK_BOX(box), tile->icon_label, FALSE, FALSE, 0);

    GtkWidget *text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_widget_set_valign(text_box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(text_box, GTK_ALIGN_START);
    gtk_widget_set_hexpand(text_box, TRUE);

    tile->title_label = gtk_label_new(title);
    gtk_style_context_add_class(gtk_widget_get_style_context(tile->title_label), "tile-title");
    gtk_widget_set_halign(tile->title_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(text_box), tile->title_label, FALSE, FALSE, 0);

    tile->sub_label = gtk_label_new(sub_inact);
    gtk_style_context_add_class(gtk_widget_get_style_context(tile->sub_label), "tile-subtitle");
    gtk_widget_set_halign(tile->sub_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(text_box), tile->sub_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), text_box, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(tile->button), box);

    g_signal_connect(tile->button, "clicked", G_CALLBACK(on_tile_clicked), tile);
    return tile->button;
}

// Live Dynamic Polling Callback (runs every 250ms while Control Center is open)
static gboolean on_live_poll(gpointer data) {
    AppState *app = (AppState *)data;
    gint64 now = g_get_monotonic_time();

    // 1. Live Brightness Sync (ALS daemon or external adjustments)
    if (!app->syncing_bright && app->pending_bright < 0 && (now - app->last_user_bright_time > 400000)) {
        int curr_b = get_sysfs_brightness(app);
        int ui_b = (int)gtk_range_get_value(GTK_RANGE(app->bright_scale));
        if (curr_b != ui_b) {
            app->syncing_bright = TRUE;
            gtk_range_set_value(GTK_RANGE(app->bright_scale), curr_b);
            char bbuf[16];
            snprintf(bbuf, sizeof(bbuf), "%d%%", curr_b);
            gtk_label_set_text(GTK_LABEL(app->bright_label), bbuf);
            update_brightness_icon(app, curr_b);
            app->syncing_bright = FALSE;
        }
    }

    // 2. Live Rotation / Posture Sync (tablet vs laptop mode)
    char rsub[64];
    gboolean ract = check_rot_status(rsub, sizeof(rsub));
    const char *curr_rsub = gtk_label_get_text(GTK_LABEL(app->tile_rot.sub_label));
    if (ract != app->tile_rot.is_active || (curr_rsub && strcmp(curr_rsub, rsub) != 0)) {
        update_tile_ui(&app->tile_rot, ract, rsub);
    }

    // 3. Audio volume & RFKill live sync (every 500ms = 2 ticks)
    app->poll_counter++;
    if (app->poll_counter % 2 == 0) {
        if (!app->syncing_vol && app->pending_vol < 0 && (now - app->last_user_vol_time > 400000)) {
            int vol;
            gboolean muted;
            get_current_volume(&vol, &muted);
            int ui_vol = (int)gtk_range_get_value(GTK_RANGE(app->vol_scale));
            if (vol != ui_vol || muted != app->is_muted) {
                app->is_muted = muted;
                app->syncing_vol = TRUE;
                gtk_range_set_value(GTK_RANGE(app->vol_scale), vol);
                char vbuf[16];
                snprintf(vbuf, sizeof(vbuf), "%d%%", vol);
                gtk_label_set_text(GTK_LABEL(app->vol_label), vbuf);
                update_volume_icon(app, vol, muted);
                app->syncing_vol = FALSE;
            }
        }

        // Live Wi-Fi & Bluetooth state sync
        gboolean wact = check_wifi_active();
        if (wact != app->tile_wifi.is_active) {
            update_tile_ui(&app->tile_wifi, wact, NULL);
        }
        gboolean bact = check_bt_active();
        if (bact != app->tile_bt.is_active) {
            update_tile_ui(&app->tile_bt, bact, NULL);
        }
    }

    // 4. Battery info & CPU profile (every 2s = 8 ticks)
    if (app->poll_counter % 8 == 0) {
        update_battery_info(app);
        char esub[64];
        gboolean eact = check_epb_status(esub, sizeof(esub));
        update_tile_ui(&app->tile_epb, eact, esub);
    }

    return G_SOURCE_CONTINUE;
}

// Window Event handlers (dismiss on backdrop tap or Escape)
static gboolean on_window_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    (void)widget;
    AppState *app = (AppState *)user_data;
    GtkAllocation alloc;
    gtk_widget_get_allocation(app->card, &alloc);

    if (event->x >= alloc.x && event->x <= alloc.x + alloc.width &&
        event->y >= alloc.y && event->y <= alloc.y + alloc.height) {
        return FALSE; // Inside card
    }
    cleanup_pidfile();
    gtk_main_quit();
    return TRUE;
}

static gboolean on_window_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    (void)widget;
    (void)user_data;
    if (event->keyval == GDK_KEY_Escape) {
        cleanup_pidfile();
        gtk_main_quit();
        return TRUE;
    }
    return FALSE;
}

// Header action callbacks
static void on_btn_settings(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    cleanup_pidfile();
    spawn_async("yogabook-settings");
    gtk_main_quit();
}

static void on_btn_suspend(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    cleanup_pidfile();
    spawn_async("systemctl suspend");
    gtk_main_quit();
}

static void on_btn_reboot(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    cleanup_pidfile();
    spawn_async("systemctl reboot");
    gtk_main_quit();
}

static void on_btn_power(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    cleanup_pidfile();
    spawn_async("systemctl poweroff");
    gtk_main_quit();
}

static void on_btn_close(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    cleanup_pidfile();
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    // Single instance toggle check
    FILE *pf = fopen(PIDFILE, "r");
    if (pf) {
        int old_pid = 0;
        if (fscanf(pf, "%d", &old_pid) == 1 && old_pid > 0) {
            if (kill(old_pid, 0) == 0) {
                // Already running -> toggle close!
                fclose(pf);
                kill(old_pid, SIGTERM);
                return 0;
            }
        }
        fclose(pf);
    }

    pf = fopen(PIDFILE, "w");
    if (pf) {
        fprintf(pf, "%d\n", getpid());
        fclose(pf);
    }

    signal(SIGTERM, on_sigterm);
    signal(SIGINT, on_sigterm);

    gtk_init(&argc, &argv);

    // Initialize pending sliders state
    g_app.pending_bright = -1;
    g_app.pending_vol = -1;
    g_app.last_user_bright_time = 0;
    g_app.last_user_vol_time = 0;

    // Read max brightness
    g_app.max_brightness = 100;
    FILE *fmb = fopen("/sys/class/backlight/intel_backlight/max_brightness", "r");
    if (fmb) {
        fscanf(fmb, "%d", &g_app.max_brightness);
        fclose(fmb);
    }

    // CSS styling
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, CSS_DATA, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);

    // Create window
    g_app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(g_app.window, "control-center-overlay");

    // GtkLayerShell setup
    gtk_layer_init_for_window(GTK_WINDOW(g_app.window));
    gtk_layer_set_layer(GTK_WINDOW(g_app.window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(g_app.window), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(g_app.window), -1);

    gtk_layer_set_anchor(GTK_WINDOW(g_app.window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(g_app.window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(g_app.window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(g_app.window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(g_app.window), GTK_LAYER_SHELL_EDGE_TOP, 38);

    gtk_widget_add_events(g_app.window, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
    g_signal_connect(g_app.window, "button-press-event", G_CALLBACK(on_window_button_press), &g_app);
    g_signal_connect(g_app.window, "key-press-event", G_CALLBACK(on_window_key_press), &g_app);

    // Main Box (fills window)
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(main_box, GTK_ALIGN_FILL);
    gtk_widget_set_valign(main_box, GTK_ALIGN_FILL);
    gtk_container_add(GTK_CONTAINER(g_app.window), main_box);

    // CC Card
    g_app.card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(g_app.card), "cc-card");
    gtk_widget_set_halign(g_app.card, GTK_ALIGN_END);
    gtk_widget_set_valign(g_app.card, GTK_ALIGN_START);
    gtk_widget_set_margin_end(g_app.card, 8);
    gtk_box_pack_start(GTK_BOX(main_box), g_app.card, FALSE, FALSE, 0);

    // 1. Header (Battery Status + Action buttons)
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(header_box), "header-box");
    gtk_box_pack_start(GTK_BOX(g_app.card), header_box, FALSE, FALSE, 0);

    // Status pill
    GtkWidget *status_pill = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_style_context_add_class(gtk_widget_get_style_context(status_pill), "status-pill");
    g_app.battery_label = gtk_label_new("󰁹 100%");
    gtk_style_context_add_class(gtk_widget_get_style_context(g_app.battery_label), "status-text");
    gtk_box_pack_start(GTK_BOX(status_pill), g_app.battery_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_box), status_pill, FALSE, FALSE, 0);

    // Spacer
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(header_box), spacer, TRUE, TRUE, 0);

    // Action buttons
    GtkWidget *btn_set = gtk_button_new_with_label("󰒓");
    gtk_widget_set_tooltip_text(btn_set, "Impostazioni");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_set), "header-btn");
    g_signal_connect(btn_set, "clicked", G_CALLBACK(on_btn_settings), &g_app);
    gtk_box_pack_start(GTK_BOX(header_box), btn_set, FALSE, FALSE, 0);

    GtkWidget *btn_susp = gtk_button_new_with_label("󰤄");
    gtk_widget_set_tooltip_text(btn_susp, "Sospendi");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_susp), "header-btn");
    g_signal_connect(btn_susp, "clicked", G_CALLBACK(on_btn_suspend), &g_app);
    gtk_box_pack_start(GTK_BOX(header_box), btn_susp, FALSE, FALSE, 0);

    GtkWidget *btn_reb = gtk_button_new_with_label("󰜉");
    gtk_widget_set_tooltip_text(btn_reb, "Riavvia");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_reb), "header-btn");
    g_signal_connect(btn_reb, "clicked", G_CALLBACK(on_btn_reboot), &g_app);
    gtk_box_pack_start(GTK_BOX(header_box), btn_reb, FALSE, FALSE, 0);

    GtkWidget *btn_pwr = gtk_button_new_with_label("󰐥");
    gtk_widget_set_tooltip_text(btn_pwr, "Spegni");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_pwr), "header-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_pwr), "shutdown");
    g_signal_connect(btn_pwr, "clicked", G_CALLBACK(on_btn_power), &g_app);
    gtk_box_pack_start(GTK_BOX(header_box), btn_pwr, FALSE, FALSE, 0);

    GtkWidget *btn_cls = gtk_button_new_with_label("");
    gtk_widget_set_tooltip_text(btn_cls, "Chiudi");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_cls), "header-btn");
    g_signal_connect(btn_cls, "clicked", G_CALLBACK(on_btn_close), &g_app);
    gtk_box_pack_start(GTK_BOX(header_box), btn_cls, FALSE, FALSE, 0);

    // 2. Sliders
    // A. Volume Pill
    GtkWidget *vol_pill = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(vol_pill), "slider-pill");
    gtk_box_pack_start(GTK_BOX(g_app.card), vol_pill, FALSE, FALSE, 0);

    g_app.vol_icon_btn = gtk_button_new_with_label("󰕾");
    gtk_style_context_add_class(gtk_widget_get_style_context(g_app.vol_icon_btn), "slider-icon-btn");
    gtk_widget_set_tooltip_text(g_app.vol_icon_btn, "Disattiva/Attiva Audio");
    g_signal_connect(g_app.vol_icon_btn, "clicked", G_CALLBACK(on_volume_mute_clicked), &g_app);
    gtk_box_pack_start(GTK_BOX(vol_pill), g_app.vol_icon_btn, FALSE, FALSE, 0);

    g_app.vol_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_widget_set_hexpand(g_app.vol_scale, TRUE);
    gtk_scale_set_draw_value(GTK_SCALE(g_app.vol_scale), FALSE);
    g_signal_connect(g_app.vol_scale, "value-changed", G_CALLBACK(on_volume_value_changed), &g_app);
    gtk_box_pack_start(GTK_BOX(vol_pill), g_app.vol_scale, TRUE, TRUE, 0);

    g_app.vol_label = gtk_label_new("65%");
    gtk_style_context_add_class(gtk_widget_get_style_context(g_app.vol_label), "slider-value-label");
    gtk_box_pack_start(GTK_BOX(vol_pill), g_app.vol_label, FALSE, FALSE, 0);

    // B. Brightness Pill
    GtkWidget *bright_pill = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(bright_pill), "slider-pill");
    gtk_box_pack_start(GTK_BOX(g_app.card), bright_pill, FALSE, FALSE, 0);

    g_app.bright_icon_btn = gtk_button_new_with_label("󰃠");
    gtk_style_context_add_class(gtk_widget_get_style_context(g_app.bright_icon_btn), "slider-icon-btn");
    gtk_widget_set_tooltip_text(g_app.bright_icon_btn, "Luminosità Automatica");
    g_signal_connect(g_app.bright_icon_btn, "clicked", G_CALLBACK(on_autobrightness_toggle), &g_app);
    gtk_box_pack_start(GTK_BOX(bright_pill), g_app.bright_icon_btn, FALSE, FALSE, 0);

    g_app.bright_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1, 100, 1);
    gtk_widget_set_hexpand(g_app.bright_scale, TRUE);
    gtk_scale_set_draw_value(GTK_SCALE(g_app.bright_scale), FALSE);
    g_signal_connect(g_app.bright_scale, "value-changed", G_CALLBACK(on_brightness_value_changed), &g_app);
    gtk_box_pack_start(GTK_BOX(bright_pill), g_app.bright_scale, TRUE, TRUE, 0);

    g_app.bright_label = gtk_label_new("80%");
    gtk_style_context_add_class(gtk_widget_get_style_context(g_app.bright_label), "slider-value-label");
    gtk_box_pack_start(GTK_BOX(bright_pill), g_app.bright_label, FALSE, FALSE, 0);

    // 3. Quick Setting Tiles Grid
    GtkWidget *grid = gtk_grid_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(grid), "tiles-grid");
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_box_pack_start(GTK_BOX(g_app.card), grid, FALSE, FALSE, 0);

    // Wi-Fi
    GtkWidget *w_wifi = create_tile(&g_app.tile_wifi, "󰤨", "󰤭", "Wi-Fi", "Attivo", "Disattivo", toggle_wifi);
    gtk_grid_attach(GTK_GRID(grid), w_wifi, 0, 0, 1, 1);

    // Bluetooth
    GtkWidget *w_bt = create_tile(&g_app.tile_bt, "󰂯", "󰂲", "Bluetooth", "Attivo", "Disattivo", toggle_bt);
    gtk_grid_attach(GTK_GRID(grid), w_bt, 1, 0, 1, 1);

    // Keyboard
    GtkWidget *w_kbd = create_tile(&g_app.tile_kbd, "", "", "Tastiera", "Virtuale", "Mostra", toggle_kbd);
    gtk_grid_attach(GTK_GRID(grid), w_kbd, 0, 1, 1, 1);

    // Rotation
    GtkWidget *w_rot = create_tile(&g_app.tile_rot, "󰑌", "󰑌", "Rotazione", "Automatica", "Bloccata", toggle_rot);
    gtk_grid_attach(GTK_GRID(grid), w_rot, 1, 1, 1, 1);

    // Silent
    GtkWidget *w_dnd = create_tile(&g_app.tile_dnd, "󰂛", "󰂚", "Silenzioso", "Attivo", "Disattivo", toggle_dnd);
    gtk_grid_attach(GTK_GRID(grid), w_dnd, 0, 2, 1, 1);

    // Screenshot
    GtkWidget *w_shot = create_tile(&g_app.tile_shot, "", "", "Screenshot", "Salva", "Cattura", take_screenshot);
    gtk_grid_attach(GTK_GRID(grid), w_shot, 1, 2, 1, 1);

    // CPU Profile
    GtkWidget *w_epb = create_tile(&g_app.tile_epb, "󰓅", "󰾆", "Profilo CPU", "Prestazioni", "Bilanciato", toggle_epb);
    gtk_grid_attach(GTK_GRID(grid), w_epb, 0, 3, 2, 1);

    // Initial state populate
    update_battery_info(&g_app);

    // Volume initial
    int init_vol = 50;
    gboolean init_muted = FALSE;
    get_current_volume(&init_vol, &init_muted);
    g_app.is_muted = init_muted;
    g_app.syncing_vol = TRUE;
    gtk_range_set_value(GTK_RANGE(g_app.vol_scale), init_vol);
    char vbuf[16];
    snprintf(vbuf, sizeof(vbuf), "%d%%", init_vol);
    gtk_label_set_text(GTK_LABEL(g_app.vol_label), vbuf);
    update_volume_icon(&g_app, init_vol, init_muted);
    g_app.syncing_vol = FALSE;

    // Brightness initial
    int init_b = get_sysfs_brightness(&g_app);
    g_app.syncing_bright = TRUE;
    gtk_range_set_value(GTK_RANGE(g_app.bright_scale), init_b);
    char bbuf[16];
    snprintf(bbuf, sizeof(bbuf), "%d%%", init_b);
    gtk_label_set_text(GTK_LABEL(g_app.bright_label), bbuf);
    update_brightness_icon(&g_app, init_b);
    g_app.syncing_bright = FALSE;

    // Tiles initial
    update_tile_ui(&g_app.tile_wifi, check_wifi_active(), NULL);
    update_tile_ui(&g_app.tile_bt, check_bt_active(), NULL);
    update_tile_ui(&g_app.tile_kbd, FALSE, "Mostra");
    char rsub[64];
    gboolean ract = check_rot_status(rsub, sizeof(rsub));
    update_tile_ui(&g_app.tile_rot, ract, rsub);
    update_tile_ui(&g_app.tile_dnd, check_dnd_active(), NULL);
    update_tile_ui(&g_app.tile_shot, FALSE, "Cattura");
    char esub[64];
    gboolean eact = check_epb_status(esub, sizeof(esub));
    update_tile_ui(&g_app.tile_epb, eact, esub);

    gtk_widget_show_all(g_app.window);

    // Start live hardware sync timer (250ms tick)
    g_app.poll_timer_id = g_timeout_add(250, on_live_poll, &g_app);

    gtk_main();

    if (g_app.poll_timer_id > 0) {
        g_source_remove(g_app.poll_timer_id);
        g_app.poll_timer_id = 0;
    }

    cleanup_pidfile();
    return 0;
}
