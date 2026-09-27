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
    setvbuf(stdout, NULL, _IONBF, 0);
    const char *cache = "/tmp/mango_tags.cache";

    // 1. Initial tag state
    FILE *p_init = popen("mmsg get all-tags 2>/dev/null", "r");
    if (p_init) {
        FILE *fc = fopen(cache, "w");
        if (fc) {
            char line[4096];
            while (fgets(line, sizeof(line), p_init)) {
                fputs(line, fc);
            }
            fclose(fc);
        }
        pclose(p_init);
        pid_t wpid = find_pid_by_name("waybar");
        if (wpid > 0) kill(wpid, SIGRTMIN + 1);
    }

    // 2. Stream tag events continuously
    FILE *pipe = popen("mmsg watch all-tags 2>/dev/null", "r");
    if (!pipe) return 1;

    char line[4096];
    while (fgets(line, sizeof(line), pipe)) {
        if (line[0] != '\0' && line[0] != '\n') {
            FILE *fc = fopen(cache, "w");
            if (fc) {
                fputs(line, fc);
                fclose(fc);
            }
            pid_t wpid = find_pid_by_name("waybar");
            if (wpid > 0) kill(wpid, SIGRTMIN + 1);
        }
    }
    pclose(pipe);
    return 0;
}
