#include "loim/settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Floats are stored as scaled integers (margin and split in percent, zoom in
 * tenths) so round-trips are exact and parsing never depends on locale.
 */
static int settings_percent(float ratio)
{
    return (int)(ratio * 100.0F + 0.5F);
}

static int settings_tenths(float scale)
{
    return (int)(scale * 10.0F + 0.5F);
}

static int settings_clamp_int(int value, int minimum, int maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static bool settings_parse_int(const char *text, int *out_value)
{
    char *end = NULL;
    long value;

    if (text == NULL || text[0] == '\0') {
        return false;
    }
    value = strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        return false;
    }
    if (value < -1000000L || value > 1000000L) {
        return false;
    }
    *out_value = (int)value;
    return true;
}

void loim_settings_defaults(loim_settings *settings)
{
    if (settings == NULL) {
        return;
    }
    settings->columns = 1U;
    settings->page_number_mode = LOIM_PAGE_NUMBER_NONE;
    settings->margin_ratio = 0.08F;
    settings->preview_scale = LOIM_PREVIEW_SCALE_DEFAULT;
    settings->split_ratio = 0.5F;
    settings->window_width = LOIM_WINDOW_DEFAULT_WIDTH;
    settings->window_height = LOIM_WINDOW_DEFAULT_HEIGHT;
    settings->window_x = 0;
    settings->window_y = 0;
    settings->has_window_position = false;
}

loim_status loim_settings_encode(
    const loim_settings *settings,
    char *output,
    size_t output_capacity)
{
    int written;

    if (settings == NULL || output == NULL || output_capacity == 0U) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    written = snprintf(
        output,
        output_capacity,
        "columns=%zu\n"
        "page_numbers=%d\n"
        "margin=%d\n"
        "zoom=%d\n"
        "split=%d\n"
        "width=%d\n"
        "height=%d\n",
        loim_columns_normalize(settings->columns),
        (int)loim_page_number_normalize(settings->page_number_mode),
        settings_clamp_int(
            settings_percent(settings->margin_ratio),
            (int)(LOIM_MARGIN_RATIO_MIN * 100.0F),
            (int)(LOIM_MARGIN_RATIO_MAX * 100.0F)),
        settings_clamp_int(
            settings_tenths(settings->preview_scale),
            (int)(LOIM_PREVIEW_SCALE_MIN * 10.0F),
            (int)(LOIM_PREVIEW_SCALE_MAX * 10.0F)),
        settings_clamp_int(
            settings_percent(settings->split_ratio),
            (int)(LOIM_SPLIT_RATIO_MIN * 100.0F),
            (int)(LOIM_SPLIT_RATIO_MAX * 100.0F)),
        settings_clamp_int(
            settings->window_width,
            LOIM_WINDOW_MIN_WIDTH,
            LOIM_WINDOW_MAX_SIZE),
        settings_clamp_int(
            settings->window_height,
            LOIM_WINDOW_MIN_HEIGHT,
            LOIM_WINDOW_MAX_SIZE));
    if (written < 0 || (size_t)written >= output_capacity) {
        return LOIM_ERROR_OVERFLOW;
    }
    if (settings->has_window_position) {
        size_t used = (size_t)written;

        written = snprintf(
            output + used,
            output_capacity - used,
            "x=%d\ny=%d\n",
            settings->window_x,
            settings->window_y);
        if (written < 0 || (size_t)written >= output_capacity - used) {
            return LOIM_ERROR_OVERFLOW;
        }
    }
    return LOIM_OK;
}

typedef struct settings_parse_context {
    loim_settings *settings;
    bool saw_x;
    bool saw_y;
} settings_parse_context;

static void loim_settings_apply_pair(
    const char *key,
    const char *value,
    settings_parse_context *context)
{
    loim_settings *settings = context->settings;
    int number;

    if (!settings_parse_int(value, &number)) {
        return;
    }
    if (strcmp(key, "columns") == 0) {
        settings->columns = (size_t)settings_clamp_int(number, 1, 3);
    } else if (strcmp(key, "page_numbers") == 0) {
        settings->page_number_mode = (loim_page_number_mode)settings_clamp_int(
            number,
            (int)LOIM_PAGE_NUMBER_NONE,
            (int)LOIM_PAGE_NUMBER_BOTTOM_CENTER);
    } else if (strcmp(key, "margin") == 0) {
        settings->margin_ratio =
            (float)settings_clamp_int(
                number,
                (int)(LOIM_MARGIN_RATIO_MIN * 100.0F),
                (int)(LOIM_MARGIN_RATIO_MAX * 100.0F)) /
            100.0F;
    } else if (strcmp(key, "zoom") == 0) {
        settings->preview_scale =
            (float)settings_clamp_int(
                number,
                (int)(LOIM_PREVIEW_SCALE_MIN * 10.0F),
                (int)(LOIM_PREVIEW_SCALE_MAX * 10.0F)) /
            10.0F;
    } else if (strcmp(key, "split") == 0) {
        settings->split_ratio =
            (float)settings_clamp_int(
                number,
                (int)(LOIM_SPLIT_RATIO_MIN * 100.0F),
                (int)(LOIM_SPLIT_RATIO_MAX * 100.0F)) /
            100.0F;
    } else if (strcmp(key, "width") == 0) {
        settings->window_width = settings_clamp_int(
            number, LOIM_WINDOW_MIN_WIDTH, LOIM_WINDOW_MAX_SIZE);
    } else if (strcmp(key, "height") == 0) {
        settings->window_height = settings_clamp_int(
            number, LOIM_WINDOW_MIN_HEIGHT, LOIM_WINDOW_MAX_SIZE);
    } else if (strcmp(key, "x") == 0) {
        settings->window_x = number;
        context->saw_x = true;
    } else if (strcmp(key, "y") == 0) {
        settings->window_y = number;
        context->saw_y = true;
    }
    /* Unknown keys are ignored so newer files load on older builds. */
}

loim_status loim_settings_decode(
    const char *text,
    loim_settings *settings)
{
    settings_parse_context context;
    const char *cursor;

    if (text == NULL || settings == NULL) {
        return LOIM_ERROR_INVALID_ARGUMENT;
    }
    context.settings = settings;
    context.saw_x = false;
    context.saw_y = false;
    settings->has_window_position = false;
    cursor = text;
    while (*cursor != '\0') {
        size_t line_length = strcspn(cursor, "\r\n");
        const char *equals = memchr(cursor, '=', line_length);

        if (equals != NULL) {
            size_t key_length = (size_t)(equals - cursor);
            size_t value_length = line_length - key_length - 1U;
            char key[32];
            char value[32];

            if (key_length > 0U && key_length < sizeof(key) &&
                value_length < sizeof(value)) {
                memcpy(key, cursor, key_length);
                key[key_length] = '\0';
                memcpy(value, equals + 1, value_length);
                value[value_length] = '\0';
                loim_settings_apply_pair(key, value, &context);
            }
        }
        cursor += line_length;
        while (*cursor == '\r' || *cursor == '\n') {
            ++cursor;
        }
    }
    settings->has_window_position = context.saw_x && context.saw_y;
    return LOIM_OK;
}
