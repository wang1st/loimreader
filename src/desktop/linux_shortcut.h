#ifndef LOIM_LINUX_SHORTCUT_H
#define LOIM_LINUX_SHORTCUT_H

#include <stdbool.h>
#include <stddef.h>

typedef enum loim_shortcut_result {
    LOIM_SHORTCUT_CREATED = 0,
    LOIM_SHORTCUT_ALREADY_PRESENT,
    LOIM_SHORTCUT_CONFLICT,
    LOIM_SHORTCUT_UNAVAILABLE,
    LOIM_SHORTCUT_ERROR
} loim_shortcut_result;

bool loim_linux_resolve_desktop_directory(
    const char *home_directory,
    const char *config_directory,
    char *output,
    size_t output_capacity);

loim_shortcut_result loim_linux_desktop_shortcut_ensure(
    const char *desktop_directory,
    const char *installed_entry,
    const char *shortcut_name);

#endif
