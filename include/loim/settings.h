#ifndef LOIM_SETTINGS_H
#define LOIM_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

#include "loim/presentation.h"
#include "loim/status.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Persisted desktop preferences: sheet layout (columns), page number mode,
 * page margin ratio, and preview zoom. Encoded as a tiny key=value text file
 * so it stays forward-compatible: unknown keys are ignored and out-of-range
 * values are clamped, never fatal.
 */
#define LOIM_SETTINGS_TEXT_CAPACITY 160U

typedef struct loim_settings {
    size_t columns;
    loim_page_number_mode page_number_mode;
    float margin_ratio;
    float preview_scale;
    float split_ratio;
    int window_width;
    int window_height;
    int window_x;
    int window_y;
    bool has_window_position;
} loim_settings;

void loim_settings_defaults(loim_settings *settings);
loim_status loim_settings_encode(
    const loim_settings *settings,
    char *output,
    size_t output_capacity);
/*
 * Decodes key=value lines into `settings`. Only known keys overwrite the
 * fields; callers should seed `settings` with loim_settings_defaults() or the
 * current in-app values first, so missing keys keep their existing values.
 */
loim_status loim_settings_decode(
    const char *text,
    loim_settings *settings);

#ifdef __cplusplus
}
#endif

#endif
