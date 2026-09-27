#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>

static pid_t find_pid_by_name(const char *name) {
    DIR *d = opendir("/proc");
    if (!d) return 0;
    struct dirent *ent;
    pid_t res = 0;
    while ((ent = readdir(d)) != NULL) {
        pid_t pid = atoi(ent->d_name);
        if (pid <= 0) continue;

        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/comm", pid);
        FILE *f = fopen(path, "r");
        if (f) {
            char comm[64];
            if (fgets(comm, sizeof(comm), f)) {
                comm[strcspn(comm, "\r\n")] = '\0';
                if (strcmp(comm, name) == 0) {
                    res = pid;
                    fclose(f);
                    break;
                }
            }
            fclose(f);
        }
    }
    closedir(d);
    return res;
}

int main(void) {
    pid_t pid = find_pid_by_name("wvkbd");
    if (pid > 0) {
        kill(pid, SIGRTMIN);
    } else {
        int r = system("systemctl --user start wvkbd.service >/dev/null 2>&1");
        (void)r;
        usleep(200000);
        pid = find_pid_by_name("wvkbd");
        if (pid > 0) {
            kill(pid, SIGUSR2);
        }
    }
    return 0;
}
