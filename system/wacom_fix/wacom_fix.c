/*
 * wacom_fix.c: Hardware coordinate orientation hook for Lenovo Yoga Book Wacom EMR Create Pad.
 *
 * Intercepts wlroots wlr_cursor_warp_absolute() to transform Wacom digitizer
 * hardware coordinates into Wayland screen space matching the active display transform.
 *
 * In standard landscape mode (transform 270):
 *   - Hardware ABS_X (0=spacebar, 1=hinge) maps to Screen Y (0=top/hinge, 1=bottom/spacebar) -> y' = 1.0 - x
 *   - Hardware ABS_Y (0=Esc/left, 1=Enter/right) maps to Screen X (0=left, 1=right)          -> x' = y
 */

#define _GNU_SOURCE
#ifndef WLR_USE_UNSTABLE
#define WLR_USE_UNSTABLE
#endif

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_input_device.h>

typedef void (*wlr_cursor_warp_absolute_fn)(struct wlr_cursor *cur,
                                            struct wlr_input_device *dev,
                                            double x, double y);

static wlr_cursor_warp_absolute_fn real_warp = NULL;

static inline bool is_wacom_device(const struct wlr_input_device *dev) {
    if (!dev || !dev->name) return false;
    return (strcasestr(dev->name, "wacom") != NULL);
}

void wlr_cursor_warp_absolute(struct wlr_cursor *cur,
                              struct wlr_input_device *dev,
                              double x, double y) {
    if (!real_warp) {
        real_warp = (wlr_cursor_warp_absolute_fn)dlsym(RTLD_NEXT, "wlr_cursor_warp_absolute");
        if (!real_warp) return;
    }

    if (!is_wacom_device(dev) || (isnan(x) && isnan(y))) {
        real_warp(cur, dev, x, y);
        return;
    }

    // Invariant physical mapping between Create Pad and display (mechanically fixed by hinge):
    // Hardware long axis ABS_Y (0=Esc/left, 1=Enter/right) -> Display X
    // Hardware short axis ABS_X (0=spacebar/bottom, 1=hinge/top) -> Display Y
    double nx = isnan(y) ? NAN : y;
    double ny = isnan(x) ? NAN : (1.0 - x);

    if (!isnan(nx)) {
        if (nx < 0.0) nx = 0.0;
        else if (nx > 1.0) nx = 1.0;
    }

    if (!isnan(ny)) {
        if (ny < 0.0) ny = 0.0;
        else if (ny > 1.0) ny = 1.0;
    }

    real_warp(cur, dev, nx, ny);
}
