/*
 * yogabook-bt-audio-watcher.c
 * Native C PipeWire Bluetooth Audio Auto-Switch Daemon for Yoga Book
 * Automatically routes all system audio to ANY Bluetooth speaker,
 * headphones, soundbar, or car receiver upon connection, and cleanly
 * restores internal Yoga Book DSP speakers upon disconnection.
 *
 * Resident RAM: ~300 KB, CPU: 0.0%
 */

#include <pipewire/pipewire.h>
#include <spa/utils/result.h>
#include <spa/utils/dict.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

struct node_info {
    uint32_t id;
    int is_bluez_sink;
    int is_dsp_sink;
    struct node_info *next;
};

struct app_data {
    struct pw_main_loop *loop;
    struct pw_context *context;
    struct pw_core *core;
    struct pw_registry *registry;
    struct spa_hook registry_listener;
    struct node_info *nodes;
    uint32_t active_bt_sink_id;
    uint32_t dsp_sink_id;
    uint32_t alsa_sink_id;
};

static void add_node(struct app_data *data, uint32_t id, int is_bluez, int is_dsp) {
    struct node_info *n = calloc(1, sizeof(struct node_info));
    if (!n) return;
    n->id = id;
    n->is_bluez_sink = is_bluez;
    n->is_dsp_sink = is_dsp;
    n->next = data->nodes;
    data->nodes = n;
}

static void remove_node(struct app_data *data, uint32_t id, int *was_bluez, int *was_dsp) {
    struct node_info **curr = &data->nodes;
    *was_bluez = 0;
    *was_dsp = 0;
    while (*curr) {
        if ((*curr)->id == id) {
            struct node_info *to_free = *curr;
            *was_bluez = to_free->is_bluez_sink;
            *was_dsp = to_free->is_dsp_sink;
            *curr = to_free->next;
            free(to_free);
            return;
        }
        curr = &(*curr)->next;
    }
}

static void set_default_sink(uint32_t id) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "wpctl set-default %u >/dev/null 2>&1", id);
    if (system(cmd) == 0) {
        system("pkill -RTMIN+10 waybar >/dev/null 2>&1 || true");
    }
}

static void registry_event_global(void *object, uint32_t id,
                                  uint32_t permissions, const char *type, uint32_t version,
                                  const struct spa_dict *props)
{
    (void)permissions; (void)version;
    struct app_data *data = object;

    if (strcmp(type, PW_TYPE_INTERFACE_Node) != 0 || !props)
        return;

    const char *media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if (!media_class || strcmp(media_class, "Audio/Sink") != 0)
        return;

    const char *node_name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    const char *dev_api = spa_dict_lookup(props, PW_KEY_DEVICE_API);
    const char *bt_addr = spa_dict_lookup(props, "api.bluez5.address");

    int is_bluez = 0;
    int is_dsp = 0;

    if (node_name && strcmp(node_name, "effect_input.yogabook_dsp") == 0) {
        is_dsp = 1;
        data->dsp_sink_id = id;
    } else if ((dev_api && strcmp(dev_api, "bluez5") == 0) ||
               bt_addr != NULL ||
               (node_name && strncmp(node_name, "bluez_output", 12) == 0)) {
        is_bluez = 1;
    } else if (dev_api && strcmp(dev_api, "alsa") == 0) {
        data->alsa_sink_id = id;
    }

    add_node(data, id, is_bluez, is_dsp);

    if (is_bluez) {
        data->active_bt_sink_id = id;
        set_default_sink(id);
    }
}

static void registry_event_global_remove(void *object, uint32_t id)
{
    struct app_data *data = object;
    int was_bluez = 0;
    int was_dsp = 0;

    remove_node(data, id, &was_bluez, &was_dsp);

    if (was_dsp && data->dsp_sink_id == id) {
        data->dsp_sink_id = 0;
    }
    if (data->alsa_sink_id == id) {
        data->alsa_sink_id = 0;
    }

    if (was_bluez && data->active_bt_sink_id == id) {
        data->active_bt_sink_id = 0;
        uint32_t other_bt_id = 0;
        for (struct node_info *n = data->nodes; n; n = n->next) {
            if (n->is_bluez_sink) {
                other_bt_id = n->id;
                break;
            }
        }

        if (other_bt_id > 0) {
            data->active_bt_sink_id = other_bt_id;
            set_default_sink(other_bt_id);
        } else if (data->dsp_sink_id > 0) {
            set_default_sink(data->dsp_sink_id);
        } else if (data->alsa_sink_id > 0) {
            set_default_sink(data->alsa_sink_id);
        }
    }
}

static const struct pw_registry_events registry_events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = registry_event_global,
    .global_remove = registry_event_global_remove,
};

static struct app_data g_app_data;

static void handle_sigterm(int sig) {
    (void)sig;
    if (g_app_data.loop)
        pw_main_loop_quit(g_app_data.loop);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, handle_sigterm);
    signal(SIGTERM, handle_sigterm);

    pw_init(&argc, &argv);

    g_app_data.loop = pw_main_loop_new(NULL);
    if (!g_app_data.loop) {
        return 1;
    }

    g_app_data.context = pw_context_new(pw_main_loop_get_loop(g_app_data.loop), NULL, 0);
    if (!g_app_data.context) {
        pw_main_loop_destroy(g_app_data.loop);
        return 1;
    }

    g_app_data.core = pw_context_connect(g_app_data.context, NULL, 0);
    if (!g_app_data.core) {
        pw_context_destroy(g_app_data.context);
        pw_main_loop_destroy(g_app_data.loop);
        return 1;
    }

    g_app_data.registry = pw_core_get_registry(g_app_data.core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(g_app_data.registry, &g_app_data.registry_listener,
                            &registry_events, &g_app_data);

    pw_main_loop_run(g_app_data.loop);

    spa_hook_remove(&g_app_data.registry_listener);
    pw_proxy_destroy((struct pw_proxy *)g_app_data.registry);
    pw_core_disconnect(g_app_data.core);
    pw_context_destroy(g_app_data.context);
    pw_main_loop_destroy(g_app_data.loop);
    pw_deinit();

    return 0;
}
