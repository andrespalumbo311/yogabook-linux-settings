#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

int main(void) {
    bool is_dnd = false;
    FILE *f = popen("makoctl mode 2>/dev/null", "r");
    if (f) {
        char buf[128];
        while (fgets(buf, sizeof(buf), f)) {
            if (strstr(buf, "dnd")) {
                is_dnd = true;
                break;
            }
        }
        pclose(f);
    }

    if (is_dnd) {
        printf("{\"alt\":\"dnd\",\"tooltip\":\"Modalità Non Disturbare Attiva (Mako)\\n• Click sinistro: Centro di Controllo\\n• Click destro: Disattiva DND\\n• Click centrale: Ripristina notifiche\",\"class\":\"dnd\"}\n");
    } else {
        printf("{\"alt\":\"default\",\"tooltip\":\"Notifiche Attive (Mako)\\n• Click sinistro: Centro di Controllo\\n• Click destro: Attiva DND\\n• Click centrale: Ripristina notifiche\",\"class\":\"normal\"}\n");
    }
    return 0;
}
