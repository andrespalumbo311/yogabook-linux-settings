#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

int main(void) {
    const char *env_sock = getenv("MANGO_INSTANCE_SIGNATURE");
    struct stat st;
    if (!env_sock || stat(env_sock, &st) != 0 || !S_ISSOCK(st.st_mode)) {
        char dir_path[64];
        snprintf(dir_path, sizeof(dir_path), "/run/user/%d", getuid());
        DIR *d = opendir(dir_path);
        if (d) {
            struct dirent *ent;
            char best_sock[512] = {0};
            time_t best_mtime = 0;
            while ((ent = readdir(d)) != NULL) {
                if (strncmp(ent->d_name, "mango-", 6) == 0 && strstr(ent->d_name, ".sock")) {
                    char full[512];
                    snprintf(full, sizeof(full), "%s/%s", dir_path, ent->d_name);
                    if (stat(full, &st) == 0 && S_ISSOCK(st.st_mode)) {
                        if (st.st_mtime >= best_mtime) {
                            best_mtime = st.st_mtime;
                            snprintf(best_sock, sizeof(best_sock), "%s", full);
                        }
                    }
                }
            }
            closedir(d);
            if (best_sock[0]) {
                setenv("MANGO_INSTANCE_SIGNATURE", best_sock, 1);
            }
        }
    }

    execlp("mmsg", "mmsg", "dispatch", "killclient", NULL);
    return 1;
}
