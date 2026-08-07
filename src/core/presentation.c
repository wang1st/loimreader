#include "loim/presentation.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

size_t loim_columns_normalize(size_t columns)
{
    return columns < 1U ? 1U : columns > 3U ? 3U : columns;
}

size_t loim_columns_next(size_t columns)
{
    columns = loim_columns_normalize(columns);
    return columns == 3U ? 1U : columns + 1U;
}

size_t loim_sheet_slot_count(size_t columns)
{
    columns = loim_columns_normalize(columns);
    return columns * columns;
}

size_t loim_sheet_slot_column(size_t columns, size_t slot)
{
    columns = loim_columns_normalize(columns);
    return slot % columns;
}

size_t loim_sheet_slot_row(size_t columns, size_t slot)
{
    columns = loim_columns_normalize(columns);
    return slot / columns;
}

size_t loim_sheet_count(size_t slice_count, size_t columns)
{
    size_t slots = loim_sheet_slot_count(columns);

    return slice_count == 0U ? 0U : 1U + (slice_count - 1U) / slots;
}

size_t loim_sheet_slice_index(size_t sheet_index, size_t columns, size_t slot)
{
    size_t slots = loim_sheet_slot_count(columns);

    if (sheet_index > (SIZE_MAX - slot) / slots) {
        return SIZE_MAX;
    }
    return sheet_index * slots + slot;
}

bool loim_export_page_requires_watermark(
    bool licensed,
    size_t columns,
    size_t page_index)
{
    columns = loim_columns_normalize(columns);
    if (licensed) {
        return false;
    }
    if (columns == 1U) {
        return page_index > 5U;
    }
    if (columns == 2U) {
        return page_index > 1U;
    }
    return page_index > 0U;
}

bool loim_export_default_path(
    const char *documents_path,
    const char *image_path,
    char *output,
    size_t output_capacity)
{
    static const char filename[] = "LoimReader.pdf";
    const char *base = documents_path;
    size_t base_length;
    char separator = '/';
    int written;

    if (output == NULL || output_capacity == 0U) {
        return false;
    }
    output[0] = '\0';
    if (base == NULL || base[0] == '\0') {
        const char *slash;
        const char *backslash;
        const char *end;

        if (image_path == NULL || image_path[0] == '\0') {
            base = ".";
        } else {
            slash = strrchr(image_path, '/');
            backslash = strrchr(image_path, '\\');
            end = slash == NULL ? backslash
                : backslash == NULL || slash > backslash ? slash : backslash;
            if (end == NULL) {
                base = ".";
            } else {
                base_length = (size_t)(end - image_path);
                if (base_length + 1U > output_capacity) {
                    return false;
                }
                memcpy(output, image_path, base_length);
                output[base_length] = '\0';
                base = output;
            }
        }
    }
    base_length = strlen(base);
    if (strchr(base, '\\') != NULL && strchr(base, '/') == NULL) {
        separator = '\\';
    }
    if (base == output) {
        bool has_separator = base_length > 0U &&
            (base[base_length - 1U] == '/' || base[base_length - 1U] == '\\');
        size_t required = base_length + (has_separator ? 0U : 1U) + sizeof(filename);

        if (required > output_capacity) {
            return false;
        }
        if (!has_separator) {
            output[base_length++] = separator;
        }
        memcpy(output + base_length, filename, sizeof(filename));
        return true;
    }
    written = snprintf(
        output,
        output_capacity,
        "%s%s%s",
        base,
        base_length > 0U && (base[base_length - 1U] == '/' ||
            base[base_length - 1U] == '\\') ? "" : separator == '/' ? "/" : "\\",
        filename);
    return written >= 0 && (size_t)written < output_capacity;
}

uint32_t loim_a4_width_px(unsigned dpi)
{
    return dpi == 300U ? 2480U : (uint32_t)(((uint64_t)dpi * 210U + 127U) / 254U);
}

uint32_t loim_a4_height_px(unsigned dpi)
{
    return dpi == 300U ? 3508U : (uint32_t)(((uint64_t)dpi * 297U + 127U) / 254U);
}

unsigned loim_progress_percent(size_t completed, size_t total)
{
    uintmax_t numerator;

    if (total == 0U) {
        return 0U;
    }
    if (completed >= total) {
        return 100U;
    }
    numerator = (uintmax_t)completed * 100U;
    return (unsigned)(numerator / (uintmax_t)total);
}
