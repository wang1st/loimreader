#ifndef LOIM_EXPORT_NAME_H
#define LOIM_EXPORT_NAME_H

#include <stdbool.h>
#include <stddef.h>

#include "loim/i18n.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOIM_EXPORT_FILENAME_MAX_BYTES 180U
#define LOIM_EXPORT_FILENAME_MAX_CHARACTERS 60U

/*
 * Suggest a localized PDF filename from imported image paths. `timestamp`
 * should use YYYYMMDD-HHMM and is only used by the generic batch fallback.
 */
bool loim_export_suggested_filename(
    const char *const *image_paths,
    size_t image_count,
    size_t columns,
    loim_locale locale,
    const char *timestamp,
    char *output,
    size_t output_capacity);

/*
 * Join the suggested filename to the Documents folder, falling back to the
 * first imported image's folder. Existing files receive " (2)", " (3)", ...
 */
bool loim_export_suggested_path(
    const char *documents_path,
    const char *const *image_paths,
    size_t image_count,
    size_t columns,
    loim_locale locale,
    const char *timestamp,
    char *output,
    size_t output_capacity);

#ifdef __cplusplus
}
#endif

#endif
