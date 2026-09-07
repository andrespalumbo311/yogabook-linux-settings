/*
 * pamac_fix.c: Bypass Landlock sandbox restriction in libalpm / pacman 7 on kernels without Landlock.
 *
 * Compile:
 *   gcc -O2 -shared -fPIC -o pamac_fix.so pamac_fix.c
 *
 * Install:
 *   sudo cp pamac_fix.so /usr/lib/pamac_fix.so
 *   echo "/usr/lib/pamac_fix.so" | sudo tee /etc/ld.so.preload
 */

int alpm_sandbox_setup_child(void) {
    return 0;
}
