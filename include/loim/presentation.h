#ifndef LOIM_PRESENTATION_H
#define LOIM_PRESENTATION_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOIM_PDF_REFERENCE_DPI 300U
#define LOIM_PDF_ADAPTIVE_MIN_DPI 72U
#define LOIM_PREVIEW_SCALE_DEFAULT 1.0F
#define LOIM_PREVIEW_SCALE_MIN 0.5F
#define LOIM_PREVIEW_SCALE_MAX 1.3F
#define LOIM_PREVIEW_SCALE_STEP 0.1F
#define LOIM_MARGIN_RATIO_MIN 0.02F
#define LOIM_MARGIN_RATIO_MAX 0.22F
#define LOIM_SPLIT_RATIO_MIN 0.18F
#define LOIM_SPLIT_RATIO_MAX 0.82F
#define LOIM_WINDOW_MIN_WIDTH 760
#define LOIM_WINDOW_MIN_HEIGHT 520
#define LOIM_WINDOW_MAX_SIZE 16384
#define LOIM_WINDOW_DEFAULT_WIDTH 1440
#define LOIM_WINDOW_DEFAULT_HEIGHT 900

typedef enum loim_pdf_dpi_policy {
    LOIM_PDF_DPI_ADAPTIVE = 0,
    LOIM_PDF_DPI_FIXED_300
} loim_pdf_dpi_policy;

typedef enum loim_page_number_mode {
    LOIM_PAGE_NUMBER_NONE = 0,
    LOIM_PAGE_NUMBER_BOTTOM_RIGHT,
    LOIM_PAGE_NUMBER_BOTTOM_CENTER
} loim_page_number_mode;

typedef enum loim_workspace_mode {
    LOIM_WORKSPACE_EMPTY = 0,
    LOIM_WORKSPACE_IMPORTING_EMPTY,
    LOIM_WORKSPACE_READY
} loim_workspace_mode;

/*
 * Supported sheet layouts: one, two, or three columns. Multi-column sheets
 * use magazine reading order: top-to-bottom within a column, then left-to-right.
 */
size_t loim_columns_normalize(size_t columns);
size_t loim_columns_next(size_t columns);
size_t loim_sheet_slot_count(size_t columns);
size_t loim_sheet_slot_column(size_t columns, size_t slot);
size_t loim_sheet_slot_row(size_t columns, size_t slot);
size_t loim_sheet_count(size_t slice_count, size_t columns);
size_t loim_sheet_slice_index(size_t sheet_index, size_t columns, size_t slot);
loim_page_number_mode loim_page_number_normalize(loim_page_number_mode mode);
loim_page_number_mode loim_page_number_next(loim_page_number_mode mode);
/*
 * Resolve the desktop content mode. `workspace_ready` is deliberately
 * separate from `image_count`: during an initial batch import, decoded images
 * may already exist while the finished layout is not ready to display yet.
 */
loim_workspace_mode loim_workspace_mode_resolve(
    size_t image_count,
    bool import_active,
    bool workspace_ready);
bool loim_workspace_shows_divider(loim_workspace_mode mode);
bool loim_workspace_clear_enabled(
    size_t image_count,
    bool import_active,
    bool drop_collecting);
uint64_t loim_import_generation_next(uint64_t generation);
bool loim_import_result_should_commit(
    uint64_t active_generation,
    uint64_t result_generation,
    bool cancel_requested);
/* Resolve the top-left origin of a page number in paper/canvas coordinates. */
bool loim_page_number_origin(
    loim_page_number_mode mode,
    float page_width,
    float page_height,
    float margin_x,
    float margin_y,
    float text_width,
    float text_height,
    float *out_x,
    float *out_y);
/* Adjust one preview scale step in the requested direction and clamp it. */
float loim_preview_scale_adjust(float scale, int direction);
/* Resolve an A4 preview width that always fits inside its viewport. */
float loim_preview_paper_width(float viewport_width, float scale);
/* Preserve the visible sheet position when preview paper width changes. */
float loim_preview_scroll_reanchor(
    float scroll_y,
    float viewport_height,
    float old_paper_width,
    float new_paper_width,
    size_t sheet_count);
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
    unsigned maximum_dpi);
/*
 * Resolve a sheet's output DPI. Adaptive export preserves the highest useful
 * source detail on that sheet while staying in the safe 72-300 DPI range.
 * Print output uses the fixed 300 DPI policy.
 */
unsigned loim_pdf_page_dpi(
    loim_pdf_dpi_policy policy,
    const unsigned *slice_dpis,
    size_t slice_count);
/* Convert a positive floating-point span to a bounded, non-empty pixel span. */
bool loim_pixel_span_quantize(
    float start,
    float length,
    uint32_t limit,
    uint32_t *out_start,
    uint32_t *out_length);
unsigned loim_progress_percent(size_t completed, size_t total);

#ifdef __cplusplus
}
#endif

#endif
