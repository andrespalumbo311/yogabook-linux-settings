#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>

#define PIDFILE "/tmp/yogabook-launcher.pid"

static const char *CSS_DATA =
"* {\n"
"    font-family: \"Adwaita Sans\", \"Liberation Sans\", Roboto, sans-serif;\n"
"}\n"
"window#launcher-overlay {\n"
"    background-color: rgba(0, 0, 0, 0.40);\n"
"}\n"
".drawer-card {\n"
"    background-color: #ffffff;\n"
"    border: 1px solid #dcdfe4;\n"
"    border-radius: 28px;\n"
"    padding: 24px;\n"
"    box-shadow: 0 16px 48px rgba(0, 0, 0, 0.22);\n"
"    margin: 40px 80px;\n"
"}\n"
".search-box {\n"
"    background-color: #f0f3f8;\n"
"    border: 1px solid #dcdfe4;\n"
"    border-radius: 20px;\n"
"    padding: 10px 16px;\n"
"    font-size: 15px;\n"
"    color: #1f1f1f;\n"
"    margin-bottom: 18px;\n"
"}\n"
".search-box:focus {\n"
"    background-color: #ffffff;\n"
"    border-color: #0b57d0;\n"
"}\n"
".apps-scroll {\n"
"    background-color: transparent;\n"
"}\n"
"flowboxchild {\n"
"    background-color: transparent;\n"
"    border-radius: 16px;\n"
"    padding: 10px 6px;\n"
"    margin: 6px;\n"
"    transition: background-color 150ms ease;\n"
"    outline: none;\n"
"}\n"
"flowboxchild:hover,\n"
"flowboxchild:focus,\n"
"flowboxchild:selected {\n"
"    background-color: #e8eef8;\n"
"}\n"
".app-tile {\n"
"    min-width: 96px;\n"
"}\n"
".app-label {\n"
"    font-size: 12px;\n"
"    font-weight: 600;\n"
"    margin-top: 6px;\n"
"    color: #1f1f1f;\n"
"}\n";

typedef struct {
    GtkWidget *window;
    GtkWidget *card;
    GtkWidget *search_entry;
    GtkWidget *flowbox;
    GtkWidget *scrolled_window;
} LauncherApp;

static LauncherApp g_launcher;

static void cleanup_pidfile(void) {
    unlink(PIDFILE);
}

static void on_sigterm(int sig) {
    (void)sig;
    cleanup_pidfile();
    gtk_main_quit();
}

static char* str_tolower(const char *src) {
    if (!src) return strdup("");
    char *res = strdup(src);
    for (char *p = res; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
    }
    return res;
}

static char* build_search_text(GAppInfo *app_info, const char *display_name) {
    GString *s = g_string_new(display_name);
    g_string_append_c(s, ' ');

    const char *desc = g_app_info_get_description(app_info);
    if (desc) {
        g_string_append(s, desc);
        g_string_append_c(s, ' ');
    }

    const char *exe = g_app_info_get_executable(app_info);
    if (exe) {
        g_string_append(s, exe);
        g_string_append_c(s, ' ');
    }

    const char *id = g_app_info_get_id(app_info);
    if (id) {
        g_string_append(s, id);
        g_string_append_c(s, ' ');
    }

    if (G_IS_DESKTOP_APP_INFO(app_info)) {
        const char * const *kw = g_desktop_app_info_get_keywords(G_DESKTOP_APP_INFO(app_info));
        if (kw) {
            for (int i = 0; kw[i]; i++) {
                g_string_append(s, kw[i]);
                g_string_append_c(s, ' ');
            }
        }
    }

    char *lower = str_tolower(s->str);
    g_string_free(s, TRUE);
    return lower;
}

static void launch_app(GAppInfo *app_info) {
    if (!app_info) return;

    GdkDisplay *disp = gdk_display_get_default();
    GdkAppLaunchContext *ctx = gdk_display_get_app_launch_context(disp);
    gdk_app_launch_context_set_timestamp(ctx, GDK_CURRENT_TIME);

    GError *err = NULL;
    if (!g_app_info_launch(app_info, NULL, G_APP_LAUNCH_CONTEXT(ctx), &err)) {
        // Fallback to command line
        const char *cmd = g_app_info_get_commandline(app_info);
        if (cmd) {
            g_spawn_command_line_async(cmd, NULL);
        }
        if (err) g_error_free(err);
    }

    // Flush pending D-Bus activation messages (crucial for DBusActivatable applications like Pamac/Nautilus)
    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    if (conn) {
        g_dbus_connection_flush_sync(conn, NULL, NULL);
        g_object_unref(conn);
    }

    g_object_unref(ctx);

    cleanup_pidfile();
    gtk_main_quit();
}

static void on_child_activated(GtkFlowBox *box, GtkFlowBoxChild *child, gpointer user_data) {
    (void)box;
    (void)user_data;
    GAppInfo *app_info = (GAppInfo *)g_object_get_data(G_OBJECT(child), "app_info");
    if (app_info) {
        launch_app(app_info);
    }
}

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    (void)user_data;
    const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
    char *query = str_tolower(text);
    g_strstrip(query);

    GList *children = gtk_container_get_children(GTK_CONTAINER(g_launcher.flowbox));
    for (GList *l = children; l != NULL; l = l->next) {
        GtkWidget *child = GTK_WIDGET(l->data);
        const char *search_text = (const char *)g_object_get_data(G_OBJECT(child), "search_text");
        if (search_text) {
            if (strlen(query) == 0 || strstr(search_text, query) != NULL) {
                gtk_widget_show(child);
            } else {
                gtk_widget_hide(child);
            }
        }
    }
    g_list_free(children);
    free(query);
}

static void on_search_activate(GtkEntry *entry, gpointer user_data) {
    (void)entry;
    (void)user_data;
    GList *children = gtk_container_get_children(GTK_CONTAINER(g_launcher.flowbox));
    for (GList *l = children; l != NULL; l = l->next) {
        GtkWidget *child = GTK_WIDGET(l->data);
        if (gtk_widget_get_visible(child)) {
            GAppInfo *app_info = (GAppInfo *)g_object_get_data(G_OBJECT(child), "app_info");
            if (app_info) {
                g_list_free(children);
                launch_app(app_info);
                return;
            }
        }
    }
    g_list_free(children);
}

static gboolean on_window_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    (void)widget;
    LauncherApp *app = (LauncherApp *)user_data;
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

static gint compare_apps(gconstpointer a, gconstpointer b) {
    GAppInfo *app_a = (GAppInfo *)a;
    GAppInfo *app_b = (GAppInfo *)b;
    const char *name_a = g_app_info_get_display_name(app_a);
    if (!name_a) name_a = g_app_info_get_name(app_a);
    const char *name_b = g_app_info_get_display_name(app_b);
    if (!name_b) name_b = g_app_info_get_name(app_b);
    return g_utf8_collate(name_a ? name_a : "", name_b ? name_b : "");
}

static void load_applications(LauncherApp *app) {
    GList *apps = g_app_info_get_all();
    apps = g_list_sort(apps, compare_apps);

    GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    for (GList *l = apps; l != NULL; l = l->next) {
        GAppInfo *app_info = (GAppInfo *)l->data;
        if (!g_app_info_should_show(app_info)) {
            g_object_unref(app_info);
            continue;
        }

        const char *name = g_app_info_get_display_name(app_info);
        if (!name) name = g_app_info_get_name(app_info);
        if (!name || strlen(name) == 0 || g_hash_table_contains(seen, name)) {
            g_object_unref(app_info);
            continue;
        }
        g_hash_table_add(seen, strdup(name));

        GtkWidget *child = gtk_flow_box_child_new();
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_style_context_add_class(gtk_widget_get_style_context(box), "app-tile");
        gtk_widget_set_halign(box, GTK_ALIGN_CENTER);

        // Icon
        GIcon *icon = g_app_info_get_icon(app_info);
        GtkWidget *img;
        if (icon) {
            img = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_DIALOG);
        } else {
            img = gtk_image_new_from_icon_name("application-x-executable", GTK_ICON_SIZE_DIALOG);
        }
        gtk_image_set_pixel_size(GTK_IMAGE(img), 48);
        gtk_box_pack_start(GTK_BOX(box), img, FALSE, FALSE, 0);

        // Label
        GtkWidget *lbl = gtk_label_new(name);
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "app-label");
        gtk_label_set_max_width_chars(GTK_LABEL(lbl), 11);
        gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
        gtk_box_pack_start(GTK_BOX(box), lbl, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(child), box);

        g_object_set_data_full(G_OBJECT(child), "app_info", app_info, g_object_unref);
        g_object_set_data_full(G_OBJECT(child), "app_name", strdup(name), free);
        char *search_text = build_search_text(app_info, name);
        g_object_set_data_full(G_OBJECT(child), "search_text", search_text, free);

        gtk_container_add(GTK_CONTAINER(app->flowbox), child);
    }

    g_hash_table_destroy(seen);
    g_list_free(apps);
}

int main(int argc, char *argv[]) {
    // Single instance toggle check
    FILE *pf = fopen(PIDFILE, "r");
    if (pf) {
        int old_pid = 0;
        if (fscanf(pf, "%d", &old_pid) == 1 && old_pid > 0) {
            if (kill(old_pid, 0) == 0) {
                // Already open -> toggle close!
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

    // Ensure display variables
    setenv("WAYLAND_DISPLAY", "wayland-0", 0);
    setenv("DISPLAY", ":0", 0);

    gtk_init(&argc, &argv);

    // CSS
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, CSS_DATA, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);

    // Window
    g_launcher.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(g_launcher.window, "launcher-overlay");

    // LayerShell
    gtk_layer_init_for_window(GTK_WINDOW(g_launcher.window));
    gtk_layer_set_layer(GTK_WINDOW(g_launcher.window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(g_launcher.window), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(g_launcher.window), -1);

    gtk_layer_set_anchor(GTK_WINDOW(g_launcher.window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(g_launcher.window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(g_launcher.window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(g_launcher.window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

    gtk_widget_add_events(g_launcher.window, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
    g_signal_connect(g_launcher.window, "button-press-event", G_CALLBACK(on_window_button_press), &g_launcher);
    g_signal_connect(g_launcher.window, "key-press-event", G_CALLBACK(on_window_key_press), &g_launcher);

    // Main Box
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(main_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(main_box, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(g_launcher.window), main_box);

    // Drawer Card
    g_launcher.card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(g_launcher.card), "drawer-card");
    gtk_widget_set_size_request(g_launcher.card, 680, 520);
    gtk_box_pack_start(GTK_BOX(main_box), g_launcher.card, FALSE, FALSE, 0);

    // Search entry
    g_launcher.search_entry = gtk_search_entry_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(g_launcher.search_entry), "search-box");
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_launcher.search_entry), "Cerca applicazioni...");
    g_signal_connect(g_launcher.search_entry, "search-changed", G_CALLBACK(on_search_changed), &g_launcher);
    g_signal_connect(g_launcher.search_entry, "activate", G_CALLBACK(on_search_activate), &g_launcher);
    gtk_box_pack_start(GTK_BOX(g_launcher.card), g_launcher.search_entry, FALSE, FALSE, 0);

    // Scrolled Window
    g_launcher.scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_style_context_add_class(gtk_widget_get_style_context(g_launcher.scrolled_window), "apps-scroll");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(g_launcher.scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_kinetic_scrolling(GTK_SCROLLED_WINDOW(g_launcher.scrolled_window), TRUE);
    gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(g_launcher.scrolled_window), TRUE);
    gtk_widget_set_vexpand(g_launcher.scrolled_window, TRUE);
    gtk_widget_set_hexpand(g_launcher.scrolled_window, TRUE);
    gtk_box_pack_start(GTK_BOX(g_launcher.card), g_launcher.scrolled_window, TRUE, TRUE, 0);

    // FlowBox
    g_launcher.flowbox = gtk_flow_box_new();
    gtk_widget_set_valign(g_launcher.flowbox, GTK_ALIGN_START);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(g_launcher.flowbox), 4);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(g_launcher.flowbox), 6);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(g_launcher.flowbox), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(g_launcher.flowbox), TRUE);
    gtk_flow_box_set_activate_on_single_click(GTK_FLOW_BOX(g_launcher.flowbox), TRUE);
    g_signal_connect(g_launcher.flowbox, "child-activated", G_CALLBACK(on_child_activated), &g_launcher);
    gtk_container_add(GTK_CONTAINER(g_launcher.scrolled_window), g_launcher.flowbox);

    load_applications(&g_launcher);

    gtk_widget_show_all(g_launcher.window);
    gtk_widget_grab_focus(g_launcher.search_entry);

    gtk_main();

    cleanup_pidfile();
    return 0;
}
