#ifndef LOIM_PRESENTATION_H
#define LOIM_PRESENTATION_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Supported sheet layouts: one, two, or three horizontal columns. */
size_t loim_columns_normalize(size_t columns);
size_t loim_columns_next(size_t columns);
size_t loim_sheet_slot_count(size_t columns);
size_t loim_sheet_slot_column(size_t columns, size_t slot);
size_t loim_sheet_slot_row(size_t columns, size_t slot);
size_t loim_sheet_count(size_t slice_count, size_t columns);
size_t loim_sheet_slice_index(size_t sheet_index, size_t columns, size_t slot);
bool loim_export_page_requires_watermark(
    bool licensed,
    size_t columns,
    size_t page_index);
bool loim_export_default_path(
    const char *documents_path,
    const char *image_path,
    char *output,
    size_t output_capacity);
uint32_t loim_a4_width_px(unsigned dpi);
uint32_t loim_a4_height_px(unsigned dpi);
/*
 * Compute the DPI at which a slice would be printed if its source pixels were
 * mapped 1:1 onto the layout rectangle (measured on the 300 DPI reference
 * canvas). Takes the minimum of the current document DPI and this slice's
 * width/height-constrained DPI, clamped to a floor of 1. A zero-size layout
 * rectangle leaves `current_dpi` unchanged.
 */
unsigned loim_slice_effective_dpi(
    uint32_t source_width_px,
    uint32_t source_height_px,
    float layout_width_px,
    float layout_height_px,
    unsigned current_dpi);
unsigned loim_progress_percent(size_t completed, size_t total);

#ifdef __cplusplus
}
#endif

#endif
