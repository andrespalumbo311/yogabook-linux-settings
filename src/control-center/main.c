#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <pango/pangocairo.h>
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
"    background-color: rgba(0, 0, 0, 0.40);\n"
"}\n"
".cc-card {\n"
"    background-color: #fbfbfe;\n"
"    border: 1px solid #dce0e9;\n"
"    border-top: none;\n"
"    border-radius: 0 0 28px 28px;\n"
"    padding: 18px 20px 20px 20px;\n"
"    box-shadow: 0 16px 48px rgba(0, 0, 0, 0.22);\n"
"    min-width: 420px;\n"
"    margin: 0 10px 0 0;\n"
"}\n"
".header-box {\n"
"    margin-bottom: 14px;\n"
"    padding: 0 2px;\n"
"}\n"
".status-pill {\n"
"    background-color: #edf0f7;\n"
"    border: none;\n"
"    border-radius: 9999px;\n"
"    padding: 6px 14px;\n"
"}\n"
".status-text {\n"
"    font-size: 13px;\n"
"    font-weight: 700;\n"
"    color: #191c20;\n"
"}\n"
"/* TOTAL RESET OF GTK BUTTON DEFAULTS */\n"
"button,\n"
"button:hover,\n"
"button:active,\n"
"button:checked,\n"
"button:focus,\n"
"button:backdrop {\n"
"    background-image: none;\n"
"    background-color: transparent;\n"
"    border: none;\n"
"    box-shadow: none;\n"
"    text-shadow: none;\n"
"    -gtk-icon-shadow: none;\n"
"    outline: none;\n"
"}\n"
".header-btn {\n"
"    min-width: 38px;\n"
"    min-height: 38px;\n"
"    border-radius: 9999px;\n"
"    background-color: #edf0f7;\n"
"    color: #191c20;\n"
"    font-family: \"JetBrainsMono Nerd Font Propo\", \"Symbols Nerd Font\", sans-serif;\n"
"    font-size: 15px;\n"
"    padding: 0;\n"
"    margin-left: 8px;\n"
"    transition: all 150ms ease;\n"
"}\n"
".header-btn:hover {\n"
"    background-color: #dfe3eb;\n"
"    color: #191c20;\n"
"}\n"
".header-btn.close-btn:hover {\n"
"    background-color: #ffdad6;\n"
"    color: #ba1a1a;\n"
"}\n"
"/* Primary Connectivity Cards (2x1 Rectangles) */\n"
".primary-row {\n"
"    margin-top: 4px;\n"
"    margin-bottom: 10px;\n"
"}\n"
".primary-tile-btn {\n"
"    background-color: #edf0f7;\n"
"    border-radius: 22px;\n"
"    padding: 10px 14px;\n"
"    min-height: 64px;\n"
"    transition: all 180ms ease;\n"
"}\n"
".primary-tile-btn:hover {\n"
"    background-color: #e2e6ef;\n"
"}\n"
".primary-tile-badge {\n"
"    background-color: #dfe3ec;\n"
"    border-radius: 9999px;\n"
"    min-width: 44px;\n"
"    min-height: 44px;\n"
"    transition: all 180ms ease;\n"
"}\n"
".primary-tile-icon {\n"
"    font-family: \"JetBrainsMono Nerd Font Propo\", \"Symbols Nerd Font\", sans-serif;\n"
"    font-size: 21px;\n"
"    color: #191c20;\n"
"}\n"
".primary-tile-title {\n"
"    font-size: 14px;\n"
"    font-weight: 700;\n"
"    color: #191c20;\n"
"}\n"
".primary-tile-subtitle {\n"
"    font-size: 11px;\n"
"    font-weight: 600;\n"
"    color: #5c6068;\n"
"}\n"
".primary-tile-arrow {\n"
"    font-family: \"JetBrainsMono Nerd Font Propo\", \"Symbols Nerd Font\", sans-serif;\n"
"    font-size: 14px;\n"
"    color: #8c9199;\n"
"}\n"
"/* ACTIVE PRIMARY TILE (Material You Primary Container) */\n"
".primary-tile-btn.active {\n"
"    background-color: #d3e3fd;\n"
"}\n"
".primary-tile-btn.active:hover {\n"
"    background-color: #c5d8fb;\n"
"}\n"
".primary-tile-btn.active .primary-tile-badge {\n"
"    background-color: #ffffff;\n"
"}\n"
".primary-tile-btn.active .primary-tile-icon {\n"
"    color: #041e49;\n"
"}\n"
".primary-tile-btn.active .primary-tile-title {\n"
"    color: #041e49;\n"
"}\n"
".primary-tile-btn.active .primary-tile-subtitle {\n"
"    color: #004a77;\n"
"}\n"
".primary-tile-btn.active .primary-tile-arrow {\n"
"    color: #041e49;\n"
"}\n"
"/* Secondary Compact Squircles (1x1) */\n"
".compact-row {\n"
"    margin-bottom: 14px;\n"
"}\n"
".compact-tile-btn {\n"
"    background-color: #edf0f7;\n"
"    border-radius: 20px;\n"
"    padding: 10px 4px;\n"
"    min-height: 68px;\n"
"    transition: all 180ms ease;\n"
"}\n"
".compact-tile-btn:hover {\n"
"    background-color: #e2e6ef;\n"
"}\n"
".compact-tile-icon {\n"
"    font-family: \"JetBrainsMono Nerd Font Propo\", \"Symbols Nerd Font\", sans-serif;\n"
"    font-size: 22px;\n"
"    color: #191c20;\n"
"}\n"
".compact-tile-label {\n"
"    font-size: 11px;\n"
"    font-weight: 700;\n"
"    color: #191c20;\n"
"    margin-top: 4px;\n"
"}\n"
"/* ACTIVE COMPACT TILE (Material You Primary Container) */\n"
".compact-tile-btn.active {\n"
"    background-color: #d3e3fd;\n"
"}\n"
".compact-tile-btn.active:hover {\n"
"    background-color: #c5d8fb;\n"
"}\n"
".compact-tile-btn.active .compact-tile-icon {\n"
"    color: #041e49;\n"
"}\n"
".compact-tile-btn.active .compact-tile-label {\n"
"    color: #041e49;\n"
"}\n"
"/* Footer Buttons */\n"
".footer-box {\n"
"    padding-top: 10px;\n"
"    border-top: 1px solid #e2e6ef;\n"
"}\n"
".footer-pill-btn {\n"
"    background-color: #edf0f7;\n"
"    border-radius: 9999px;\n"
"    padding: 8px 16px;\n"
"    font-size: 12px;\n"
"    font-weight: 700;\n"
"    color: #191c20;\n"
"    margin: 0 4px;\n"
"    min-height: 38px;\n"
"    transition: all 150ms ease;\n"
"}\n"
".footer-pill-btn:hover {\n"
"    background-color: #dfe3eb;\n"
"}\n"
".footer-pill-btn.power-btn {\n"
"    background-color: #ffdad6;\n"
"    color: #ba1a1a;\n"
"}\n"
".footer-pill-btn.power-btn:hover {\n"
"    background-color: #ba1a1a;\n"
"    color: #ffffff;\n"
"}\n";

// Helper: Cairo Rounded Rectangle (Capsule / Pill)
static void cairo_rounded_rectangle(cairo_t *cr, double x, double y, double w, double h, double r) {
    if (w < 2 * r) r = w / 2;
    if (h < 2 * r) r = h / 2;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

// Android 14 / Material 3 Fluid Capsule Slider
typedef struct {
    GtkWidget *drawing_area;
    char icon_name[32];
    int value;               // 0 - 100
    gboolean is_muted;
    gboolean is_auto;
    gboolean is_dragging;
    void (*on_change)(int new_val, gpointer user_data);
    void (*on_icon_click)(gpointer user_data);
    gpointer user_data;
} M3Slider;

// Primary Connectivity 2x1 Rectangle (Wi-Fi & Bluetooth)
typedef struct {
    GtkWidget *button;
    GtkWidget *badge;
    GtkWidget *icon_label;
    GtkWidget *title_label;
    GtkWidget *sub_label;
    GtkWidget *arrow_label;
    const char *icon_active;
    const char *icon_inactive;
    const char *title_text;
    const char *sub_active;
    const char *sub_inactive;
    gboolean is_active;
    void (*toggle_fn)(gpointer user_data);
} PrimaryTile;

// Secondary 1x1 Compact Squircle Toggle
typedef struct {
    GtkWidget *button;
    GtkWidget *icon_label;
    GtkWidget *name_label;
    const char *icon_active;
    const char *icon_inactive;
    const char *name;
    gboolean is_active;
    void (*toggle_fn)(gpointer user_data);
} CompactTile;

typedef struct {
    GtkWidget *window;
    GtkWidget *card;
    GtkWidget *battery_label;

    // Sliders
    M3Slider slider_vol;
    gboolean is_muted;
    gboolean syncing_vol;
    guint vol_debounce_id;
    int pending_vol;

    M3Slider slider_bright;
    gboolean syncing_bright;
    guint bright_debounce_id;
    int pending_bright;
    int max_brightness;

    // Primary 2x1 Tiles
    PrimaryTile tile_wifi;
    PrimaryTile tile_bt;

    // Compact 1x1 Tiles
    CompactTile tile_kbd;
    CompactTile tile_rot;
    CompactTile tile_dnd;
    CompactTile tile_shot;
    CompactTile tile_epb;

    // Live Dynamic Polling
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

// -----------------------------------------------------------------------------
// Cairo Slider Rendering & Input
// -----------------------------------------------------------------------------

static gboolean on_m3_slider_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    M3Slider *s = (M3Slider *)user_data;
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);
    double h = (double)height;
    double w = (double)width;
    double r = h / 2.0;

    int val = s->value;
    if (val < 0) val = 0;
    if (val > 100) val = 100;
    double pct = val / 100.0;
    double fill_w = w * pct;

    // 1. Inactive Track (#edf0f7)
    cairo_rounded_rectangle(cr, 0, 0, w, h, r);
    cairo_set_source_rgb(cr, 237.0 / 255.0, 240.0 / 255.0, 247.0 / 255.0);
    cairo_fill(cr);

    // 2. Active Track (Filled portion clipped inside capsule)
    if (fill_w > 0) {
        cairo_save(cr);
        cairo_rounded_rectangle(cr, 0, 0, w, h, r);
        cairo_clip(cr);

        cairo_rectangle(cr, 0, 0, fill_w, h);
        if (s->is_muted) {
            cairo_set_source_rgb(cr, 186.0 / 255.0, 26.0 / 255.0, 26.0 / 255.0);
        } else {
            // M3 Primary #0b57d0
            cairo_set_source_rgb(cr, 11.0 / 255.0, 87.0 / 255.0, 208.0 / 255.0);
        }
        cairo_fill(cr);

        // White Separator Thumb at the progress boundary
        if (fill_w > 12 && fill_w < w - 6) {
            cairo_rounded_rectangle(cr, fill_w - 5, 8, 4, h - 16, 2.0);
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.95);
            cairo_fill(cr);
        }
        cairo_restore(cr);
    }

    // 3. Render Icon & Percentage via PangoCairo
    PangoContext *pctx = gtk_widget_get_pango_context(widget);
    PangoLayout *layout = pango_layout_new(pctx);

    // A. Icon (at x = 18)
    PangoFontDescription *icon_font = pango_font_description_from_string("JetBrainsMono Nerd Font Propo, Symbols Nerd Font 18");
    pango_layout_set_font_description(layout, icon_font);
    pango_layout_set_text(layout, s->icon_name, -1);

    int icon_w, icon_h;
    pango_layout_get_pixel_size(layout, &icon_w, &icon_h);
    double icon_x = 18.0;
    double icon_y = (h - icon_h) / 2.0;

    cairo_move_to(cr, icon_x, icon_y);
    if (fill_w > icon_x + icon_w / 2.0) {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    } else {
        if (s->is_muted) {
            cairo_set_source_rgb(cr, 186.0 / 255.0, 26.0 / 255.0, 26.0 / 255.0);
        } else {
            cairo_set_source_rgb(cr, 25.0 / 255.0, 28.0 / 255.0, 32.0 / 255.0); // #191c20
        }
    }
    pango_cairo_show_layout(cr, layout);
    pango_font_description_free(icon_font);

    // B. Percentage Label (right-aligned at x = w - 20 - pct_w)
    PangoFontDescription *text_font = pango_font_description_from_string("Adwaita Sans, Liberation Sans, Roboto 13");
    pango_font_description_set_weight(text_font, PANGO_WEIGHT_BOLD);
    pango_layout_set_font_description(layout, text_font);

    char pct_str[16];
    snprintf(pct_str, sizeof(pct_str), "%d%%", val);
    pango_layout_set_text(layout, pct_str, -1);

    int pct_w, pct_h;
    pango_layout_get_pixel_size(layout, &pct_w, &pct_h);
    double pct_x = w - 20.0 - pct_w;
    double pct_y = (h - pct_h) / 2.0;

    cairo_move_to(cr, pct_x, pct_y);
    if (fill_w > pct_x + pct_w / 2.0) {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    } else {
        cairo_set_source_rgb(cr, 25.0 / 255.0, 28.0 / 255.0, 32.0 / 255.0); // #191c20
    }
    pango_cairo_show_layout(cr, layout);

    pango_font_description_free(text_font);
    g_object_unref(layout);

    return FALSE;
}

static gboolean on_m3_slider_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    M3Slider *s = (M3Slider *)user_data;
    if (event->button != 1) return FALSE;

    if (event->x < 46.0 && s->on_icon_click) {
        s->on_icon_click(s->user_data);
        return TRUE;
    }

    s->is_dragging = TRUE;
    int width = gtk_widget_get_allocated_width(widget);
    if (width > 0) {
        double pct = (event->x / (double)width) * 100.0;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        s->value = (int)round(pct);
        gtk_widget_queue_draw(widget);
        if (s->on_change) s->on_change(s->value, s->user_data);
    }
    return TRUE;
}

static gboolean on_m3_slider_button_release(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    (void)widget;
    M3Slider *s = (M3Slider *)user_data;
    if (event->button == 1) {
        s->is_dragging = FALSE;
    }
    return TRUE;
}

static gboolean on_m3_slider_motion(GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
    M3Slider *s = (M3Slider *)user_data;
    if (!s->is_dragging) return FALSE;

    int width = gtk_widget_get_allocated_width(widget);
    if (width > 0) {
        double pct = (event->x / (double)width) * 100.0;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        s->value = (int)round(pct);
        gtk_widget_queue_draw(widget);
        if (s->on_change) s->on_change(s->value, s->user_data);
    }
    return TRUE;
}

static gboolean on_m3_slider_touch(GtkWidget *widget, GdkEventTouch *event, gpointer user_data) {
    M3Slider *s = (M3Slider *)user_data;
    if (event->type == GDK_TOUCH_BEGIN) {
        if (event->x < 46.0 && s->on_icon_click) {
            s->on_icon_click(s->user_data);
            return TRUE;
        }
        s->is_dragging = TRUE;
    } else if (event->type == GDK_TOUCH_END || event->type == GDK_TOUCH_CANCEL) {
        s->is_dragging = FALSE;
        return TRUE;
    }

    if (s->is_dragging) {
        int width = gtk_widget_get_allocated_width(widget);
        if (width > 0) {
            double pct = (event->x / (double)width) * 100.0;
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            s->value = (int)round(pct);
            gtk_widget_queue_draw(widget);
            if (s->on_change) s->on_change(s->value, s->user_data);
        }
    }
    return TRUE;
}

static GtkWidget* create_m3_slider(M3Slider *s, const char *initial_icon,
                                   int initial_val, void (*on_change)(int, gpointer),
                                   void (*on_icon_click)(gpointer), gpointer user_data) {
    strncpy(s->icon_name, initial_icon, sizeof(s->icon_name) - 1);
    s->value = initial_val;
    s->is_muted = FALSE;
    s->is_auto = FALSE;
    s->is_dragging = FALSE;
    s->on_change = on_change;
    s->on_icon_click = on_icon_click;
    s->user_data = user_data;

    s->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(s->drawing_area, -1, 48);
    gtk_widget_set_hexpand(s->drawing_area, TRUE);
    gtk_widget_set_margin_bottom(s->drawing_area, 10);

    gtk_widget_add_events(s->drawing_area, GDK_BUTTON_PRESS_MASK |
                                          GDK_BUTTON_RELEASE_MASK |
                                          GDK_POINTER_MOTION_MASK |
                                          GDK_TOUCH_MASK);

    g_signal_connect(s->drawing_area, "draw", G_CALLBACK(on_m3_slider_draw), s);
    g_signal_connect(s->drawing_area, "button-press-event", G_CALLBACK(on_m3_slider_button_press), s);
    g_signal_connect(s->drawing_area, "button-release-event", G_CALLBACK(on_m3_slider_button_release), s);
    g_signal_connect(s->drawing_area, "motion-notify-event", G_CALLBACK(on_m3_slider_motion), s);
    g_signal_connect(s->drawing_area, "touch-event", G_CALLBACK(on_m3_slider_touch), s);

    return s->drawing_area;
}

// -----------------------------------------------------------------------------
// Battery Logic
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// Brightness Logic
// -----------------------------------------------------------------------------

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
        strncpy(app->slider_bright.icon_name, "󰃡", sizeof(app->slider_bright.icon_name) - 1);
        app->slider_bright.is_auto = TRUE;
    } else {
        app->slider_bright.is_auto = FALSE;
        if (val < 33) strncpy(app->slider_bright.icon_name, "󰃞", sizeof(app->slider_bright.icon_name) - 1);
        else if (val < 66) strncpy(app->slider_bright.icon_name, "󰃟", sizeof(app->slider_bright.icon_name) - 1);
        else strncpy(app->slider_bright.icon_name, "󰃠", sizeof(app->slider_bright.icon_name) - 1);
    }
    if (app->slider_bright.drawing_area) {
        gtk_widget_queue_draw(app->slider_bright.drawing_area);
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

static void on_brightness_slider_change(int val, gpointer user_data) {
    AppState *app = (AppState *)user_data;
    if (app->syncing_bright) return;

    app->last_user_bright_time = g_get_monotonic_time();
    if (val < 1) val = 1;
    update_brightness_icon(app, val);

    app->pending_bright = val;
    if (app->bright_debounce_id == 0) {
        app->bright_debounce_id = g_timeout_add(25, apply_pending_brightness, app);
    }
}

static void on_brightness_icon_click(gpointer user_data) {
    AppState *app = (AppState *)user_data;
    spawn_async("yogabook-autobrightness toggle");
    g_usleep(50000);
    int b = get_sysfs_brightness(app);
    app->slider_bright.value = b;
    update_brightness_icon(app, b);
}

// -----------------------------------------------------------------------------
// Volume Logic
// -----------------------------------------------------------------------------

static void update_volume_icon(AppState *app, int vol, gboolean muted) {
    app->is_muted = muted;
    app->slider_vol.is_muted = muted;
    if (muted || vol == 0) {
        strncpy(app->slider_vol.icon_name, "󰝟", sizeof(app->slider_vol.icon_name) - 1);
    } else if (vol < 33) {
        strncpy(app->slider_vol.icon_name, "󰕿", sizeof(app->slider_vol.icon_name) - 1);
    } else if (vol < 66) {
        strncpy(app->slider_vol.icon_name, "󰖀", sizeof(app->slider_vol.icon_name) - 1);
    } else {
        strncpy(app->slider_vol.icon_name, "󰕾", sizeof(app->slider_vol.icon_name) - 1);
    }
    if (app->slider_vol.drawing_area) {
        gtk_widget_queue_draw(app->slider_vol.drawing_area);
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

static void on_volume_slider_change(int val, gpointer user_data) {
    AppState *app = (AppState *)user_data;
    if (app->syncing_vol) return;

    app->last_user_vol_time = g_get_monotonic_time();
    update_volume_icon(app, val, app->is_muted);

    app->pending_vol = val;
    if (app->vol_debounce_id == 0) {
        app->vol_debounce_id = g_timeout_add(30, apply_pending_volume, app);
    }
}

static void on_volume_icon_click(gpointer user_data) {
    AppState *app = (AppState *)user_data;
    spawn_async("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle");
    g_usleep(40000);
    int vol;
    gboolean muted;
    get_current_volume(&vol, &muted);
    app->is_muted = muted;
    app->slider_vol.value = vol;
    update_volume_icon(app, vol, muted);
}

// -----------------------------------------------------------------------------
// Primary 2x1 Tiles (Wi-Fi & Bluetooth)
// -----------------------------------------------------------------------------

static void update_primary_tile_ui(PrimaryTile *tile, gboolean active, const char *custom_sub) {
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

static void on_primary_tile_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    PrimaryTile *tile = (PrimaryTile *)user_data;
    if (tile->toggle_fn) {
        tile->toggle_fn(tile);
    }
}

static GtkWidget* create_primary_tile(PrimaryTile *tile, const char *icon_act, const char *icon_inact,
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
    gtk_style_context_add_class(gtk_widget_get_style_context(tile->button), "primary-tile-btn");
    gtk_widget_set_hexpand(tile->button, TRUE);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    tile->badge = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(tile->badge), "primary-tile-badge");
    gtk_widget_set_size_request(tile->badge, 44, 44);
    gtk_widget_set_halign(tile->badge, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(tile->badge, GTK_ALIGN_CENTER);

    tile->icon_label = gtk_label_new(icon_inact);
    gtk_style_context_add_class(gtk_widget_get_style_context(tile->icon_label), "primary-tile-icon");
    gtk_box_pack_start(GTK_BOX(tile->badge), tile->icon_label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), tile->badge, FALSE, FALSE, 0);

    GtkWidget *text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_valign(text_box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(text_box, GTK_ALIGN_START);
    gtk_widget_set_hexpand(text_box, TRUE);

    tile->title_label = gtk_label_new(title);
    gtk_style_context_add_class(gtk_widget_get_style_context(tile->title_label), "primary-tile-title");
    gtk_widget_set_halign(tile->title_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(text_box), tile->title_label, FALSE, FALSE, 0);

    tile->sub_label = gtk_label_new(sub_inact);
    gtk_style_context_add_class(gtk_widget_get_style_context(tile->sub_label), "primary-tile-subtitle");
    gtk_widget_set_halign(tile->sub_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(text_box), tile->sub_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), text_box, TRUE, TRUE, 0);

    tile->arrow_label = gtk_label_new("󰅂");
    gtk_style_context_add_class(gtk_widget_get_style_context(tile->arrow_label), "primary-tile-arrow");
    gtk_box_pack_start(GTK_BOX(box), tile->arrow_label, FALSE, FALSE, 2);

    gtk_container_add(GTK_CONTAINER(tile->button), box);
    g_signal_connect(tile->button, "clicked", G_CALLBACK(on_primary_tile_clicked), tile);
    return tile->button;
}

// -----------------------------------------------------------------------------
// Secondary 1x1 Compact Squircles (Keyboard, Rotation, DND, Screenshot, CPU)
// -----------------------------------------------------------------------------

static void update_compact_tile_ui(CompactTile *tile, gboolean active) {
    tile->is_active = active;
    GtkStyleContext *ctx = gtk_widget_get_style_context(tile->button);
    if (active) {
        gtk_style_context_add_class(ctx, "active");
        gtk_label_set_text(GTK_LABEL(tile->icon_label), tile->icon_active);
    } else {
        gtk_style_context_remove_class(ctx, "active");
        gtk_label_set_text(GTK_LABEL(tile->icon_label), tile->icon_inactive);
    }
}

static void on_compact_tile_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    CompactTile *tile = (CompactTile *)user_data;
    if (tile->toggle_fn) {
        tile->toggle_fn(tile);
    }
}

static GtkWidget* create_compact_tile(CompactTile *tile, const char *icon_act, const char *icon_inact,
                                      const char *name, void (*toggle_fn)(gpointer)) {
    tile->icon_active = icon_act;
    tile->icon_inactive = icon_inact;
    tile->name = name;
    tile->toggle_fn = toggle_fn;
    tile->is_active = FALSE;

    tile->button = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(tile->button), "compact-tile-btn");
    gtk_widget_set_hexpand(tile->button, TRUE);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);

    tile->icon_label = gtk_label_new(icon_inact);
    gtk_style_context_add_class(gtk_widget_get_style_context(tile->icon_label), "compact-tile-icon");
    gtk_box_pack_start(GTK_BOX(box), tile->icon_label, FALSE, FALSE, 0);

    tile->name_label = gtk_label_new(name);
    gtk_style_context_add_class(gtk_widget_get_style_context(tile->name_label), "compact-tile-label");
    gtk_box_pack_start(GTK_BOX(box), tile->name_label, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(tile->button), box);
    g_signal_connect(tile->button, "clicked", G_CALLBACK(on_compact_tile_clicked), tile);
    return tile->button;
}

// -----------------------------------------------------------------------------
// Hardware Control Callbacks
// -----------------------------------------------------------------------------

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
    update_primary_tile_ui(&g_app.tile_wifi, check_wifi_active(), NULL);
    return G_SOURCE_REMOVE;
}

static void* thread_toggle_wifi(void *arg) {
    (void)arg;
    system("rfkill toggle wifi >/dev/null 2>&1; pkill -RTMIN+7 waybar 2>/dev/null &");
    g_idle_add(idle_update_wifi, NULL);
    return NULL;
}

static void toggle_wifi(gpointer user_data) {
    (void)user_data;
    pthread_t th;
    pthread_create(&th, NULL, thread_toggle_wifi, NULL);
    pthread_detach(th);
}

// 2. Bluetooth
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
    update_primary_tile_ui(&g_app.tile_bt, check_bt_active(), NULL);
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
        usleep(100000);
        system("bluetoothctl power on >/dev/null 2>&1");
    }
    usleep(100000);
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

// 4. Rotation
static gboolean check_rot_status(void) {
    char lock_path[128];
    snprintf(lock_path, sizeof(lock_path), "/run/user/%d/yogabook-rotation.lock", getuid());
    gboolean is_locked = (access(lock_path, F_OK) == 0);

    char state_path[128];
    snprintf(state_path, sizeof(state_path), "/run/user/%d/yogabook-rotation.state", getuid());
    if (access(state_path, F_OK) != 0) return FALSE;

    int autorotate = is_locked ? 0 : 1;
    FILE *f = fopen(state_path, "r");
    if (f) {
        char line[64];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "autorotate=", 11) == 0) {
                sscanf(line + 11, "%d", &autorotate);
            }
        }
        fclose(f);
    }
    return (!is_locked && autorotate == 1);
}

static gboolean idle_update_rot(gpointer user_data) {
    (void)user_data;
    update_compact_tile_ui(&g_app.tile_rot, check_rot_status());
    return G_SOURCE_REMOVE;
}

static void* thread_toggle_rot(void *arg) {
    (void)arg;
    char lock_path[128];
    snprintf(lock_path, sizeof(lock_path), "/run/user/%d/yogabook-rotation.lock", getuid());

    if (system("systemctl --user is-active --quiet rot8.service") != 0) {
        system("systemctl --user start rot8.service >/dev/null 2>&1");
        usleep(200000);
    }

    if (access(lock_path, F_OK) == 0) {
        unlink(lock_path);
    } else {
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

    usleep(150000);
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
    update_compact_tile_ui(&g_app.tile_dnd, check_dnd_active());
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
static gboolean check_epb_status(void) {
    int val = 6;
    FILE *f = fopen("/sys/devices/system/cpu/cpu0/power/energy_perf_bias", "r");
    if (f) {
        fscanf(f, "%d", &val);
        fclose(f);
    }
    return (val <= 4);
}

static gboolean idle_update_epb(gpointer user_data) {
    (void)user_data;
    update_compact_tile_ui(&g_app.tile_epb, check_epb_status());
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

// -----------------------------------------------------------------------------
// Live Dynamic Polling Callback (250ms)
// -----------------------------------------------------------------------------

static gboolean on_live_poll(gpointer data) {
    AppState *app = (AppState *)data;
    gint64 now = g_get_monotonic_time();

    // 1. Live Brightness Sync
    if (!app->syncing_bright && app->pending_bright < 0 && (now - app->last_user_bright_time > 400000)) {
        int curr_b = get_sysfs_brightness(app);
        if (curr_b != app->slider_bright.value) {
            app->syncing_bright = TRUE;
            app->slider_bright.value = curr_b;
            update_brightness_icon(app, curr_b);
            app->syncing_bright = FALSE;
        }
    }

    // 2. Audio volume & RFKill live sync (every 500ms)
    app->poll_counter++;
    if (app->poll_counter % 2 == 0) {
        if (!app->syncing_vol && app->pending_vol < 0 && (now - app->last_user_vol_time > 400000)) {
            int vol;
            gboolean muted;
            get_current_volume(&vol, &muted);
            if (vol != app->slider_vol.value || muted != app->is_muted) {
                app->is_muted = muted;
                app->syncing_vol = TRUE;
                app->slider_vol.value = vol;
                update_volume_icon(app, vol, muted);
                app->syncing_vol = FALSE;
            }
        }

        gboolean wact = check_wifi_active();
        if (wact != app->tile_wifi.is_active) {
            update_primary_tile_ui(&app->tile_wifi, wact, NULL);
        }
        gboolean bact = check_bt_active();
        if (bact != app->tile_bt.is_active) {
            update_primary_tile_ui(&app->tile_bt, bact, NULL);
        }
        gboolean ract = check_rot_status();
        if (ract != app->tile_rot.is_active) {
            update_compact_tile_ui(&app->tile_rot, ract);
        }
    }

    // 3. Battery info & CPU profile (every 2s)
    if (app->poll_counter % 8 == 0) {
        update_battery_info(app);
        gboolean eact = check_epb_status();
        if (eact != app->tile_epb.is_active) {
            update_compact_tile_ui(&app->tile_epb, eact);
        }
    }

    return G_SOURCE_CONTINUE;
}

// -----------------------------------------------------------------------------
// Window Event Handlers
// -----------------------------------------------------------------------------

static gboolean on_window_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    (void)widget;
    AppState *app = (AppState *)user_data;
    GtkAllocation alloc;
    gtk_widget_get_allocation(app->card, &alloc);

    if (event->x >= alloc.x && event->x <= alloc.x + alloc.width &&
        event->y >= alloc.y && event->y <= alloc.y + alloc.height) {
        return FALSE;
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

// Actions
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

// -----------------------------------------------------------------------------
// Main Application
// -----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
    FILE *pf = fopen(PIDFILE, "r");
    if (pf) {
        int old_pid = 0;
        if (fscanf(pf, "%d", &old_pid) == 1 && old_pid > 0) {
            if (kill(old_pid, 0) == 0) {
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

    g_app.pending_bright = -1;
    g_app.pending_vol = -1;
    g_app.last_user_bright_time = 0;
    g_app.last_user_vol_time = 0;

    g_app.max_brightness = 100;
    FILE *fmb = fopen("/sys/class/backlight/intel_backlight/max_brightness", "r");
    if (fmb) {
        fscanf(fmb, "%d", &g_app.max_brightness);
        fclose(fmb);
    }

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, CSS_DATA, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER
    );
    g_object_unref(provider);

    g_app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(g_app.window, "control-center-overlay");

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

    // Main Box
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(main_box, GTK_ALIGN_FILL);
    gtk_widget_set_valign(main_box, GTK_ALIGN_FILL);
    gtk_container_add(GTK_CONTAINER(g_app.window), main_box);

    // CC Card (Floating Bento Card)
    g_app.card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(g_app.card), "cc-card");
    gtk_widget_set_halign(g_app.card, GTK_ALIGN_END);
    gtk_widget_set_valign(g_app.card, GTK_ALIGN_START);
    gtk_widget_set_margin_end(g_app.card, 0);
    gtk_widget_set_margin_top(g_app.card, 0);
    gtk_box_pack_start(GTK_BOX(main_box), g_app.card, FALSE, FALSE, 0);

    // 1. Header (Battery Pill on Left, Settings & Close on Right)
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(header_box), "header-box");
    gtk_box_pack_start(GTK_BOX(g_app.card), header_box, FALSE, FALSE, 0);

    GtkWidget *status_pill = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_style_context_add_class(gtk_widget_get_style_context(status_pill), "status-pill");
    g_app.battery_label = gtk_label_new("󰁹 100%");
    gtk_style_context_add_class(gtk_widget_get_style_context(g_app.battery_label), "status-text");
    gtk_box_pack_start(GTK_BOX(status_pill), g_app.battery_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_box), status_pill, FALSE, FALSE, 0);

    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(header_box), spacer, TRUE, TRUE, 0);

    GtkWidget *btn_set = gtk_button_new_with_label("󰒓");
    gtk_widget_set_tooltip_text(btn_set, "Impostazioni");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_set), "header-btn");
    g_signal_connect(btn_set, "clicked", G_CALLBACK(on_btn_settings), &g_app);
    gtk_box_pack_start(GTK_BOX(header_box), btn_set, FALSE, FALSE, 0);

    GtkWidget *btn_cls = gtk_button_new_with_label("");
    gtk_widget_set_tooltip_text(btn_cls, "Chiudi");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_cls), "header-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_cls), "close-btn");
    g_signal_connect(btn_cls, "clicked", G_CALLBACK(on_btn_close), &g_app);
    gtk_box_pack_start(GTK_BOX(header_box), btn_cls, FALSE, FALSE, 0);

    // 2. Material 3 Sliders (Volume & Brightness - 48px Fluid Capsules)
    int init_vol = 50;
    gboolean init_muted = FALSE;
    get_current_volume(&init_vol, &init_muted);
    g_app.is_muted = init_muted;

    GtkWidget *vol_widget = create_m3_slider(&g_app.slider_vol, "󰕾", init_vol,
                                            on_volume_slider_change, on_volume_icon_click, &g_app);
    gtk_box_pack_start(GTK_BOX(g_app.card), vol_widget, FALSE, FALSE, 0);
    update_volume_icon(&g_app, init_vol, init_muted);

    int init_b = get_sysfs_brightness(&g_app);
    GtkWidget *bright_widget = create_m3_slider(&g_app.slider_bright, "󰃠", init_b,
                                                on_brightness_slider_change, on_brightness_icon_click, &g_app);
    gtk_box_pack_start(GTK_BOX(g_app.card), bright_widget, FALSE, FALSE, 0);
    update_brightness_icon(&g_app, init_b);

    // 3. Primary Connectivity Row (2x1 Rectangles: Wi-Fi & Bluetooth)
    GtkWidget *primary_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(primary_row), "primary-row");
    gtk_box_pack_start(GTK_BOX(g_app.card), primary_row, FALSE, FALSE, 0);

    GtkWidget *w_wifi = create_primary_tile(&g_app.tile_wifi, "󰤨", "󰤭", "Wi-Fi", "Attivo", "Disattivo", toggle_wifi);
    gtk_box_pack_start(GTK_BOX(primary_row), w_wifi, TRUE, TRUE, 0);

    GtkWidget *w_bt = create_primary_tile(&g_app.tile_bt, "󰂯", "󰂲", "Bluetooth", "Attivo", "Disattivo", toggle_bt);
    gtk_box_pack_start(GTK_BOX(primary_row), w_bt, TRUE, TRUE, 0);

    // 4. Secondary Compact Toggles Row (1x1 Squircles: 5 toggles in a row)
    GtkWidget *compact_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(compact_row), "compact-row");
    gtk_box_pack_start(GTK_BOX(g_app.card), compact_row, FALSE, FALSE, 0);

    GtkWidget *w_kbd = create_compact_tile(&g_app.tile_kbd, "", "", "Tastiera", toggle_kbd);
    gtk_box_pack_start(GTK_BOX(compact_row), w_kbd, TRUE, TRUE, 0);

    GtkWidget *w_rot = create_compact_tile(&g_app.tile_rot, "󰑮", "󰑮", "Rotazione", toggle_rot);
    gtk_box_pack_start(GTK_BOX(compact_row), w_rot, TRUE, TRUE, 0);

    GtkWidget *w_dnd = create_compact_tile(&g_app.tile_dnd, "󰂛", "󰂚", "DND", toggle_dnd);
    gtk_box_pack_start(GTK_BOX(compact_row), w_dnd, TRUE, TRUE, 0);

    GtkWidget *w_shot = create_compact_tile(&g_app.tile_shot, "", "", "Cattura", take_screenshot);
    gtk_box_pack_start(GTK_BOX(compact_row), w_shot, TRUE, TRUE, 0);

    GtkWidget *w_epb = create_compact_tile(&g_app.tile_epb, "󰓅", "󰾆", "Profilo", toggle_epb);
    gtk_box_pack_start(GTK_BOX(compact_row), w_epb, TRUE, TRUE, 0);

    // 5. System Footer (Pill Buttons)
    GtkWidget *footer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(footer_box), "footer-box");
    gtk_box_pack_start(GTK_BOX(g_app.card), footer_box, FALSE, FALSE, 0);

    GtkWidget *btn_susp = gtk_button_new_with_label("󰤄  Sospendi");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_susp), "footer-pill-btn");
    gtk_widget_set_hexpand(btn_susp, TRUE);
    g_signal_connect(btn_susp, "clicked", G_CALLBACK(on_btn_suspend), &g_app);
    gtk_box_pack_start(GTK_BOX(footer_box), btn_susp, TRUE, TRUE, 0);

    GtkWidget *btn_reb = gtk_button_new_with_label("󰜉  Riavvia");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_reb), "footer-pill-btn");
    gtk_widget_set_hexpand(btn_reb, TRUE);
    g_signal_connect(btn_reb, "clicked", G_CALLBACK(on_btn_reboot), &g_app);
    gtk_box_pack_start(GTK_BOX(footer_box), btn_reb, TRUE, TRUE, 0);

    GtkWidget *btn_pwr = gtk_button_new_with_label("󰐥  Spegni");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_pwr), "footer-pill-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_pwr), "power-btn");
    gtk_widget_set_hexpand(btn_pwr, TRUE);
    g_signal_connect(btn_pwr, "clicked", G_CALLBACK(on_btn_power), &g_app);
    gtk_box_pack_start(GTK_BOX(footer_box), btn_pwr, TRUE, TRUE, 0);

    // Initial state populate
    update_battery_info(&g_app);
    update_primary_tile_ui(&g_app.tile_wifi, check_wifi_active(), NULL);
    update_primary_tile_ui(&g_app.tile_bt, check_bt_active(), NULL);
    update_compact_tile_ui(&g_app.tile_kbd, FALSE);
    update_compact_tile_ui(&g_app.tile_rot, check_rot_status());
    update_compact_tile_ui(&g_app.tile_dnd, check_dnd_active());
    update_compact_tile_ui(&g_app.tile_shot, FALSE);
    update_compact_tile_ui(&g_app.tile_epb, check_epb_status());

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
