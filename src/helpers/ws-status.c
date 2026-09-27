#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

int main(int argc, char **argv) {
    int target = 1;
    if (argc > 1) {
        target = atoi(argv[1]);
        if (target <= 0) target = 1;
    }

    const char *cache = "/tmp/mango_tags.cache";
    struct stat st;
    if (stat(cache, &st) != 0 || st.st_size == 0) {
        int r = system("mmsg get all-tags 2>/dev/null > /tmp/mango_tags.cache");
        (void)r;
    }

    FILE *f = fopen(cache, "r");
    if (!f) {
        printf("{\"text\": \"\", \"class\": \"hidden\"}\n");
        return 0;
    }

    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    int active = 0;
    int max_used = 1;
    bool occupied[64] = {false};

    // Scan through JSON buffer searching for "index":
    char *p = buf;
    while ((p = strstr(p, "\"index\":")) != NULL) {
        p += 8;
        int idx = atoi(p);

        // Find end of this tag block or next tag
        char *end = strchr(p, '}');
        if (!end) break;

        // Check is_active
        char *act = strstr(p, "\"is_active\":true");
        if (act && act < end) {
            active = idx;
        }

        // Check client_count
        char *cnt_str = strstr(p, "\"client_count\":");
        if (cnt_str && cnt_str < end) {
            int cnt = atoi(cnt_str + 15);
            if (cnt > 0) {
                if (idx < 64) occupied[idx] = true;
                if (idx > max_used) max_used = idx;
            }
        }
        p = end;
    }

    if (active > max_used) max_used = active;
    int next_empty = max_used + 1;

    bool is_active = (target == active);
    bool is_occ = (target < 64 && occupied[target]);
    bool is_empty = (!is_occ && !is_active && target <= next_empty);

    if (is_active) {
        printf("{\"text\": \"%d\", \"class\": \"active\", \"tooltip\": \"Workspace %d (Attivo)\"}\n", target, target);
    } else if (is_occ) {
        printf("{\"text\": \"%d\", \"class\": \"occupied\", \"tooltip\": \"Workspace %d (Con finestre)\"}\n", target, target);
    } else if (is_empty) {
        printf("{\"text\": \"%d\", \"class\": \"empty\", \"tooltip\": \"Workspace %d (Nuovo/Vuoto)\"}\n", target, target);
    } else {
        printf("{\"text\": \"\", \"class\": \"hidden\"}\n");
    }

    return 0;
}
