#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

int main(void) {
    const char *pidfile = "/tmp/yogabook-launcher.pid";
    FILE *f = fopen(pidfile, "r");
    if (f) {
        pid_t pid = 0;
        if (fscanf(f, "%d", &pid) == 1 && pid > 0) {
            if (kill(pid, 0) == 0) {
                kill(pid, SIGTERM);
                fclose(f);
                unlink(pidfile);
                return 0;
            }
        }
        fclose(f);
    }

    execlp("/home/andres/.local/bin/yogabook-launcher", "yogabook-launcher", NULL);
    execlp("yogabook-launcher", "yogabook-launcher", NULL);
    return 1;
}
